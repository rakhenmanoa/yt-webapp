#include "mpris.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

namespace {
const QString kService = QStringLiteral("org.mpris.MediaPlayer2.ytwebapp");
const QString kPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString kPlayerIface = QStringLiteral("org.mpris.MediaPlayer2.Player");
} // namespace

Mpris::Mpris(QObject *parent)
    : QObject(parent)
{
    new MprisRootAdaptor(this);
    m_player = new MprisPlayerAdaptor(this);
    m_posClock.start();
}

qlonglong Mpris::position() const
{
    double sec = m_posSec;
    if (m_status == QLatin1String("Playing"))
        sec += m_posClock.elapsed() / 1000.0 * m_rate;
    return static_cast<qlonglong>(sec * 1e6);
}

// On n'apparaît dans Plasma que lorsqu'une vidéo est réellement chargée.
void Mpris::setRegistered(bool on)
{
    if (on == m_registered)
        return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (on) {
        bus.registerObject(kPath, this, QDBusConnection::ExportAdaptors);
        m_registered = bus.registerService(kService);
    } else {
        bus.unregisterService(kService);
        bus.unregisterObject(kPath);
        m_registered = false;
    }
}

void Mpris::propertiesChanged(const QString &iface, const QVariantMap &props)
{
    if (!m_registered || props.isEmpty())
        return;
    QDBusMessage sig = QDBusMessage::createSignal(
        kPath, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    sig << iface << props << QStringList();
    QDBusConnection::sessionBus().send(sig);
}

void Mpris::update(const QString &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    if (o.isEmpty()) {
        m_status = QStringLiteral("Stopped");
        setRegistered(false);
        return;
    }

    QVariantMap changed;

    // Morceau : nouvel identifiant quand l'URL change.
    const QString url = o.value(QStringLiteral("url")).toString();
    if (url != m_lastUrl) {
        m_lastUrl = url;
        m_trackId = QDBusObjectPath(
            QStringLiteral("/org/ytwebapp/track/%1").arg(++m_trackCounter));
    }

    QVariantMap md;
    md.insert(QStringLiteral("mpris:trackid"), QVariant::fromValue(m_trackId));
    const double dur = o.value(QStringLiteral("dur")).toDouble(-1);
    if (dur > 0 && std::isfinite(dur))
        md.insert(QStringLiteral("mpris:length"), static_cast<qlonglong>(dur * 1e6));
    const QString title = o.value(QStringLiteral("title")).toString();
    if (!title.isEmpty())
        md.insert(QStringLiteral("xesam:title"), title);
    const QString artist = o.value(QStringLiteral("artist")).toString();
    if (!artist.isEmpty())
        md.insert(QStringLiteral("xesam:artist"), QStringList{artist});
    const QString album = o.value(QStringLiteral("album")).toString();
    if (!album.isEmpty())
        md.insert(QStringLiteral("xesam:album"), album);
    const QString art = o.value(QStringLiteral("art")).toString();
    if (!art.isEmpty())
        md.insert(QStringLiteral("mpris:artUrl"), art);
    if (!url.isEmpty())
        md.insert(QStringLiteral("xesam:url"), url);
    if (md != m_metadata) {
        m_metadata = md;
        changed.insert(QStringLiteral("Metadata"), md);
    }

    const QString st = o.value(QStringLiteral("st")).toString(QStringLiteral("Paused"));
    if (st != m_status) {
        m_status = st;
        changed.insert(QStringLiteral("PlaybackStatus"), st);
    }

    m_posSec = o.value(QStringLiteral("pos")).toDouble(0);
    m_posClock.restart();

    const double rate = o.value(QStringLiteral("rate")).toDouble(1);
    if (!qFuzzyCompare(rate, m_rate)) {
        m_rate = rate;
        changed.insert(QStringLiteral("Rate"), rate);
    }
    const double vol = o.value(QStringLiteral("vol")).toDouble(1);
    if (!qFuzzyCompare(vol + 1, m_volume + 1)) {
        m_volume = vol;
        changed.insert(QStringLiteral("Volume"), vol);
    }
    auto setBool = [&](bool &field, const char *key, const char *prop) {
        const bool v = o.value(QLatin1String(key)).toBool();
        if (v != field) {
            field = v;
            changed.insert(QLatin1String(prop), v);
        }
    };
    setBool(m_canNext, "next", "CanGoNext");
    setBool(m_canPrev, "prev", "CanGoPrevious");
    setBool(m_canSeek, "seek", "CanSeek");

    const bool wasRegistered = m_registered;
    setRegistered(true);
    if (wasRegistered) {
        propertiesChanged(kPlayerIface, changed);
        if (o.value(QStringLiteral("seeked")).toBool())
            m_player->emitSeeked(position());
    }
}
