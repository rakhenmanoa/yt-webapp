// yt-webapp — YouTube en appli dédiée, sur QtWebEngine, pensé pour Plasma Mobile.

#include "backend.h"
#include "interceptor.h"
#include "mpris.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWebEngineProfile>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QSocketNotifier>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

namespace {

// SIGTERM/SIGINT/SIGHUP (pkill, fin de session) → sortie propre : profil
// enregistré, et écran remis comme avant si le paysage auto l'a tourné.
int g_sigFd[2] = {-1, -1};

void onSignal(int)
{
    const char c = 1;
    [[maybe_unused]] auto n = ::write(g_sigFd[0], &c, 1);
}

// Réglages Chromium par défaut, remplaçables via QTWEBENGINE_CHROMIUM_FLAGS.
//  --num-raster-threads=4             : plus de cœurs pour peindre la page (défilement)
//  --js-flags=--max-semi-space-size=32 : moins de pauses du ramasse-miettes V8
//  --disable-features=...              : services inutiles pour une appli YouTube,
//                                        et le MPRIS générique de Chromium (on a le nôtre)
const char kDefaultFlags[] =
    "--num-raster-threads=4 "
    "--js-flags=--max-semi-space-size=32 "
    "--disable-features=Translate,MediaRouter,OptimizationHints,HardwareMediaKeyHandling";

QString socketName()
{
    return QStringLiteral("yt-webapp-") + qEnvironmentVariable("USER", QStringLiteral("user"));
}

// Si une instance tourne déjà, on lui demande de s'afficher et on s'arrête.
bool forwardToRunningInstance(const QUrl &url)
{
    QLocalSocket sock;
    sock.connectToServer(socketName());
    if (!sock.waitForConnected(300))
        return false;
    sock.write((QStringLiteral("show ") + url.toString() + QLatin1Char('\n')).toUtf8());
    sock.waitForBytesWritten(300);
    sock.disconnectFromServer();
    return true;
}

QString mobileUserAgent(const QString &defaultUa)
{
    // Réutilise la vraie version de Chromium embarquée, avec un UA Android
    // pour que YouTube serve m.youtube.com (interface tactile, plus légère).
    QString major = QStringLiteral("140");
    const auto m = QRegularExpression(QStringLiteral("Chrome/(\\d+)")).match(defaultUa);
    if (m.hasMatch())
        major = m.captured(1);
    return QStringLiteral("Mozilla/5.0 (Linux; Android 14; K) AppleWebKit/537.36 "
                          "(KHTML, like Gecko) Chrome/%1.0.0.0 Mobile Safari/537.36").arg(major);
}

} // namespace

int main(int argc, char *argv[])
{
    const bool ownFlags = qEnvironmentVariableIsEmpty("QTWEBENGINE_CHROMIUM_FLAGS");
    if (ownFlags) {
        QByteArray flags(kDefaultFlags);
        // Composition logicielle (option du menu) : sur le téléphone, la
        // composition GPU de la vidéo par Chromium affiche une image verte
        // au démarrage de chaque vidéo. Lu avant le démarrage de Chromium.
        if (QSettings(QStringLiteral("yt-webapp"), QStringLiteral("yt-webapp"))
                .value(QStringLiteral("softwareCompositing"), false).toBool())
            flags += " --disable-gpu-compositing";
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
    }

    QtWebEngineQuick::initialize();

    // Le script injecté parle à l'appli via console.debug : inutile de
    // l'afficher dans le terminal.
    QLoggingCategory::setFilterRules(QStringLiteral("js.debug=false"));

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("yt-webapp"));
    app.setApplicationDisplayName(QStringLiteral("YouTube"));
    app.setDesktopFileName(QStringLiteral("yt-webapp"));
    app.setApplicationVersion(QStringLiteral(YTW_VERSION));
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("YouTube en appli dédiée (QtWebEngine)"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption bgOpt(QStringLiteral("background"),
                             QStringLiteral("Démarre caché et précharge YouTube."));
    parser.addOption(bgOpt);
    parser.addPositionalArgument(QStringLiteral("url"), QStringLiteral("Lien YouTube à ouvrir."));
    parser.process(app);

    const QStringList pos = parser.positionalArguments();
    const QUrl startUrl = pos.isEmpty() ? QUrl() : QUrl::fromUserInput(pos.first());

    if (forwardToRunningInstance(startUrl))
        return 0;

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_sigFd) == 0) {
        auto *sn = new QSocketNotifier(g_sigFd[1], QSocketNotifier::Read, &app);
        QObject::connect(sn, &QSocketNotifier::activated, &app, [] {
            char c;
            [[maybe_unused]] auto n = ::read(g_sigFd[1], &c, 1);
            QCoreApplication::exit(0);
        });
        for (int sig : {SIGTERM, SIGINT, SIGHUP})
            std::signal(sig, onSignal);
    }

    QLocalServer::removeServer(socketName());
    QLocalServer server;
    server.listen(socketName());

    // Profil persistant : cookies (connexion au compte), cache HTTP et cache de
    // code V8 sur disque. Le profil par défaut de Qt 6 est privé et perd tout
    // à la fermeture.
    auto *profile = new QQuickWebEngineProfile(&app);
    profile->setStorageName(QStringLiteral("youtube"));
    profile->setOffTheRecord(false);
    profile->setHttpCacheType(QQuickWebEngineProfile::DiskHttpCache);
    profile->setHttpCacheMaximumSize(512 * 1024 * 1024);
    profile->setPersistentCookiesPolicy(QQuickWebEngineProfile::ForcePersistentCookies);
    profile->setHttpUserAgent(mobileUserAgent(profile->httpUserAgent()));

    auto *interceptor = new Interceptor(&app);
    profile->setUrlRequestInterceptor(interceptor);

    Backend backend(interceptor, profile, parser.isSet(bgOpt), startUrl);
    {
        const QByteArray f = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
        backend.setSoftwareCompositingActive(f.contains("--disable-gpu-compositing")
                                             || f.split(' ').contains("--disable-gpu"));
    }
    Mpris mpris;
    QObject::connect(&mpris, &Mpris::raiseRequested, &backend, [&backend] { backend.activate(QUrl()); });
    QObject::connect(&mpris, &Mpris::openRequested, &backend, &Backend::activate);
    QObject::connect(&mpris, &Mpris::quitRequested, &backend, &Backend::quit);

    QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
        while (QLocalSocket *c = server.nextPendingConnection()) {
            QObject::connect(c, &QLocalSocket::readyRead, c, [c, &backend] {
                const QString line = QString::fromUtf8(c->readLine()).trimmed();
                if (line.startsWith(QLatin1String("show")))
                    backend.activate(QUrl(line.mid(4).trimmed()));
                c->deleteLater();
            });
            QObject::connect(c, &QLocalSocket::disconnected, c, &QObject::deleteLater);
        }
    });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.rootContext()->setContextProperty(QStringLiteral("ytProfile"), profile);
    engine.rootContext()->setContextProperty(QStringLiteral("mpris"), &mpris);
    engine.rootContext()->setContextProperty(QStringLiteral("appVersion"), QStringLiteral(YTW_VERSION));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    const int rc = app.exec();

    // Redémarrage demandé (réglage qui ne s'applique qu'au lancement).
    if (backend.restartRequested()) {
        server.close(); // sinon la nouvelle instance nous redonnerait la main
        QProcess p;
        p.setProgram(QCoreApplication::applicationFilePath());
        // Nos réglages Chromium seront recalculés par la nouvelle instance.
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (ownFlags)
            env.remove(QStringLiteral("QTWEBENGINE_CHROMIUM_FLAGS"));
        p.setProcessEnvironment(env);
        p.startDetached();
    }
    return rc;
}
