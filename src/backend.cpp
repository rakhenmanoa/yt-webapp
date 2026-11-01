#include "backend.h"
#include "interceptor.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QQuickWebEngineProfile>
#include <QScreen>
#include <QStandardPaths>
#include <QUrlQuery>

#include <signal.h>

namespace {

// Incrustation : mpv dans une petite fenêtre flottante, gardée au-dessus des
// autres applis par une règle KWin (Wayland ne laisse pas une appli le faire
// elle-même). La règle reconnaît la fenêtre au préfixe de son titre.
const QLatin1String kPipRuleGroup("yt-webapp-pip");
const QLatin1String kPipTitleTag("[PiP]");
const int kPipRuleVersion = 1;

bool hostIs(const QString &host, QLatin1String domain)
{
    return host == domain
        || (host.endsWith(domain) && host.size() > domain.size()
            && host.at(host.size() - domain.size() - 1) == QLatin1Char('.'));
}

bool isYouTubeHost(const QString &host)
{
    return hostIs(host, QLatin1String("youtube.com"))
        || hostIs(host, QLatin1String("youtu.be"))
        || hostIs(host, QLatin1String("youtube-nocookie.com"));
}

} // namespace

Backend::Backend(Interceptor *interceptor, QQuickWebEngineProfile *profile,
                 bool startHidden, const QUrl &startUrl, QObject *parent)
    : QObject(parent)
    , m_interceptor(interceptor)
    , m_profile(profile)
    , m_settings(QStringLiteral("yt-webapp"), QStringLiteral("yt-webapp"))
    , m_startHidden(startHidden)
    , m_startUrl(startUrl)
{
    QFile f(QStringLiteral(":/data/inject.js"));
    if (f.open(QIODevice::ReadOnly))
        m_scriptTemplate = QString::fromUtf8(f.readAll());

    m_mpvAvailable = !QStandardPaths::findExecutable(QStringLiteral("mpv")).isEmpty();
    applyToInterceptor();
}

bool Backend::forceH264() const { return m_settings.value(QStringLiteral("forceH264"), true).toBool(); }
bool Backend::blockAds() const { return m_settings.value(QStringLiteral("blockAds"), true).toBool(); }
bool Backend::autoMpv() const { return m_settings.value(QStringLiteral("autoMpv"), false).toBool(); }
bool Backend::keepInMemory() const { return m_settings.value(QStringLiteral("keepInMemory"), true).toBool(); }
int Backend::mpvHeight() const { return m_settings.value(QStringLiteral("mpvHeight"), 720).toInt(); }
bool Backend::pipMode() const { return m_settings.value(QStringLiteral("pipMode"), false).toBool(); }

void Backend::setPipMode(bool v)
{
    if (v == pipMode()) return;
    m_settings.setValue(QStringLiteral("pipMode"), v);
    if (v && !ensurePipRule())
        emit message(tr("KWin introuvable : la fenêtre ne restera pas au-dessus."));
    emit settingsChanged();
}

void Backend::setForceH264(bool v)
{
    if (v == forceH264()) return;
    m_settings.setValue(QStringLiteral("forceH264"), v);
    emit settingsChanged();
}

void Backend::setBlockAds(bool v)
{
    if (v == blockAds()) return;
    m_settings.setValue(QStringLiteral("blockAds"), v);
    applyToInterceptor();
    emit settingsChanged();
}

void Backend::setAutoMpv(bool v)
{
    if (v == autoMpv()) return;
    m_settings.setValue(QStringLiteral("autoMpv"), v);
    applyToInterceptor();
    emit settingsChanged();
}

void Backend::setKeepInMemory(bool v)
{
    if (v == keepInMemory()) return;
    m_settings.setValue(QStringLiteral("keepInMemory"), v);
    emit settingsChanged();
}

void Backend::setMpvHeight(int v)
{
    if (v == mpvHeight()) return;
    m_settings.setValue(QStringLiteral("mpvHeight"), v);
    emit settingsChanged();
}

void Backend::applyToInterceptor()
{
    m_interceptor->blockAds = blockAds();
    // En mode « toujours mpv », inutile de télécharger la vidéo dans la page.
    m_interceptor->blockWebVideo = autoMpv() && m_mpvAvailable;
}

QString Backend::injectScript() const
{
    return QStringLiteral("window.__ytwebapp = {h264: %1, ads: %2};\n")
               .arg(forceH264() ? QStringLiteral("true") : QStringLiteral("false"),
                    blockAds() ? QStringLiteral("true") : QStringLiteral("false"))
        + m_scriptTemplate;
}

bool Backend::isAllowedUrl(const QUrl &url) const
{
    const QString scheme = url.scheme();
    if (scheme == QLatin1String("about") || scheme == QLatin1String("data")
        || scheme == QLatin1String("blob"))
        return true;
    if (scheme != QLatin1String("https") && scheme != QLatin1String("http"))
        return false;

    const QString host = url.host();
    if (isYouTubeHost(host))
        return true;

    // Connexion au compte Google et pages de consentement.
    if (hostIs(host, QLatin1String("google.com"))) {
        if (host.startsWith(QLatin1String("accounts.")) || host.startsWith(QLatin1String("consent."))
            || host.startsWith(QLatin1String("myaccount.")) || host.startsWith(QLatin1String("gds.")))
            return true;
        const QString path = url.path();
        for (const char *p : {"/accounts", "/signin", "/ServiceLogin", "/sorry", "/recaptcha"}) {
            if (path.startsWith(QLatin1String(p)))
                return true;
        }
    }
    return false;
}

bool Backend::isVideoUrl(const QUrl &url) const
{
    const QString host = url.host();
    if (hostIs(host, QLatin1String("youtu.be")))
        return url.path().size() > 1;
    if (!isYouTubeHost(host))
        return false;
    const QString path = url.path();
    if (path == QLatin1String("/watch"))
        return QUrlQuery(url).hasQueryItem(QStringLiteral("v"));
    return path.startsWith(QLatin1String("/shorts/")) || path.startsWith(QLatin1String("/live/"));
}

bool Backend::openInMpv(const QUrl &url)
{
    if (!m_mpvAvailable) {
        emit message(tr("mpv n'est pas installé."));
        return false;
    }
    const int h = mpvHeight();
    // H.264 en priorité : le moins coûteux à décoder sur le téléphone.
    const QString fmt = QStringLiteral(
        "bestvideo[height<=?%1][vcodec^=avc1]+bestaudio[ext=m4a]/"
        "best[height<=?%1][vcodec^=avc1]/bestvideo[height<=?%1]+bestaudio/best").arg(h);

    QStringList args = {
        QStringLiteral("--force-window=immediate"),
        QStringLiteral("--hwdec=auto-safe"),
        QStringLiteral("--ytdl-format=") + fmt,
    };

    const bool pip = pipMode();
    if (pip) {
        ensurePipRule();
        // Une seule incrustation à la fois.
        if (m_pipPid > 0)
            ::kill(static_cast<pid_t>(m_pipPid), SIGTERM);
        args << QStringLiteral("--title=") + kPipTitleTag + QStringLiteral(" ${media-title}")
             << QStringLiteral("--ontop")          // X11 ; sous Wayland c'est la règle KWin
             << QStringLiteral("--no-border")
             << QStringLiteral("--autofit=55%")
             << QStringLiteral("--keep-open=no");
    } else {
        args << QStringLiteral("--fs");
    }
    args << url.toString();

    qint64 pid = 0;
    const bool ok = QProcess::startDetached(QStringLiteral("mpv"), args, QString(), &pid);
    if (ok && pip)
        m_pipPid = pid;
    emit message(!ok ? tr("Impossible de lancer mpv.")
                     : pip ? tr("Incrustation…") : tr("Ouverture dans mpv…"));
    return ok;
}

bool Backend::ensurePipRule(bool force)
{
    if (!force && m_settings.value(QStringLiteral("pipRuleVersion")).toInt() >= kPipRuleVersion)
        return true;

    const QString kw = QStandardPaths::findExecutable(QStringLiteral("kwriteconfig6"));
    const QString kr = QStandardPaths::findExecutable(QStringLiteral("kreadconfig6"));
    if (kw.isEmpty() || kr.isEmpty())
        return false;

    // Petite fenêtre 16:9 en bas à droite, au-dessus de la barre de navigation.
    QRect g(0, 0, 400, 860);
    if (QScreen *scr = QGuiApplication::primaryScreen())
        g = scr->availableGeometry();
    const int w = qRound(g.width() * 0.55);
    const int h = qRound(w * 9.0 / 16.0);
    const int x = g.x() + g.width() - w - 12;
    const int y = g.y() + g.height() - h - 96;

    auto set = [&](const QString &key, const QString &value, bool isBool = false) {
        QStringList a = {QStringLiteral("--file"), QStringLiteral("kwinrulesrc"),
                         QStringLiteral("--group"), kPipRuleGroup,
                         QStringLiteral("--key"), key};
        if (isBool)
            a << QStringLiteral("--type") << QStringLiteral("bool");
        a << value;
        QProcess::execute(kw, a);
    };
    const QString T = QStringLiteral("true"), F = QStringLiteral("false");
    // Valeurs des règles KWin : 1 = exact, 2 = sous-chaîne (correspondance) ;
    // 2 = Forcer, 3 = Appliquer au début (actions).
    set(QStringLiteral("Description"), QStringLiteral("yt-webapp : incrustation mpv"));
    set(QStringLiteral("wmclass"), QStringLiteral("mpv"));
    set(QStringLiteral("wmclassmatch"), QStringLiteral("1"));
    set(QStringLiteral("wmclasscomplete"), F, true);
    set(QStringLiteral("title"), kPipTitleTag);
    set(QStringLiteral("titlematch"), QStringLiteral("2"));
    set(QStringLiteral("above"), T, true);
    set(QStringLiteral("aboverule"), QStringLiteral("2"));
    set(QStringLiteral("noborder"), T, true);
    set(QStringLiteral("noborderrule"), QStringLiteral("2"));
    set(QStringLiteral("fullscreen"), F, true);
    set(QStringLiteral("fullscreenrule"), QStringLiteral("2"));
    set(QStringLiteral("maximizehoriz"), F, true);
    set(QStringLiteral("maximizehorizrule"), QStringLiteral("2"));
    set(QStringLiteral("maximizevert"), F, true);
    set(QStringLiteral("maximizevertrule"), QStringLiteral("2"));
    set(QStringLiteral("position"), QStringLiteral("%1,%2").arg(x).arg(y));
    set(QStringLiteral("positionrule"), QStringLiteral("3"));
    set(QStringLiteral("size"), QStringLiteral("%1,%2").arg(w).arg(h));
    set(QStringLiteral("sizerule"), QStringLiteral("3"));

    // Ajoute notre règle à la liste générale sans toucher aux autres.
    QProcess rd;
    rd.start(kr, {QStringLiteral("--file"), QStringLiteral("kwinrulesrc"),
                  QStringLiteral("--group"), QStringLiteral("General"),
                  QStringLiteral("--key"), QStringLiteral("rules")});
    rd.waitForFinished(3000);
    QStringList rules = QString::fromUtf8(rd.readAllStandardOutput()).trimmed()
                            .split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (!rules.contains(kPipRuleGroup)) {
        rules << kPipRuleGroup;
        QProcess::execute(kw, {QStringLiteral("--file"), QStringLiteral("kwinrulesrc"),
                               QStringLiteral("--group"), QStringLiteral("General"),
                               QStringLiteral("--key"), QStringLiteral("rules"),
                               rules.join(QLatin1Char(','))});
    }
    QProcess::execute(kw, {QStringLiteral("--file"), QStringLiteral("kwinrulesrc"),
                           QStringLiteral("--group"), QStringLiteral("General"),
                           QStringLiteral("--key"), QStringLiteral("count"),
                           QString::number(rules.size())});

    QDBusConnection::sessionBus().send(QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin"), QStringLiteral("reconfigure")));

    m_settings.setValue(QStringLiteral("pipRuleVersion"), kPipRuleVersion);
    return true;
}

void Backend::resetPipPosition()
{
    if (ensurePipRule(true))
        emit message(tr("Position de l'incrustation réinitialisée."));
    else
        emit message(tr("KWin introuvable."));
}

void Backend::openExternally(const QUrl &url)
{
    QDesktopServices::openUrl(url);
}

void Backend::clearCache()
{
    m_profile->clearHttpCache();
    emit message(tr("Cache vidé."));
}

void Backend::quit()
{
    QCoreApplication::quit();
}

void Backend::activate(const QUrl &url)
{
    emit showRequested(url);
}
