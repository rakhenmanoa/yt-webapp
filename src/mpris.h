#pragma once

// Lecteur MPRIS2 sur D-Bus : comme Plasma Browser Integration, mais intégré.
// Le script injecté décrit l'état du lecteur YouTube (via la console JS) ;
// Plasma (widget multimédia, écran de verrouillage, KDE Connect, touches
// multimédia) pilote la lecture en retour via le signal command().

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

class MprisPlayerAdaptor;

class Mpris : public QObject
{
    Q_OBJECT
public:
    explicit Mpris(QObject *parent = nullptr);

    // État envoyé par la page (JSON), ou "null" s'il n'y a plus de lecteur.
    Q_INVOKABLE void update(const QString &json);

    // --- Accès pour les adaptateurs D-Bus ---
    QString playbackStatus() const { return m_status; }
    QVariantMap metadata() const { return m_metadata; }
    qlonglong position() const;
    double volume() const { return m_volume; }
    double rate() const { return m_rate; }
    bool canGoNext() const { return m_canNext; }
    bool canGoPrevious() const { return m_canPrev; }
    bool canSeek() const { return m_canSeek; }
    QDBusObjectPath trackId() const { return m_trackId; }

    void send(const QString &action, double arg = 0) { emit command(action, arg); }
    void raise() { emit raiseRequested(); }
    void quit() { emit quitRequested(); }
    void openUri(const QString &uri) { emit openRequested(QUrl(uri)); }

signals:
    void command(const QString &action, double arg);
    void raiseRequested();
    void quitRequested();
    void openRequested(const QUrl &url);

private:
    void setRegistered(bool on);
    void propertiesChanged(const QString &iface, const QVariantMap &props);

    MprisPlayerAdaptor *m_player = nullptr;
    bool m_registered = false;
    int m_trackCounter = 0;
    QString m_lastUrl;

    QString m_status = QStringLiteral("Stopped");
    QVariantMap m_metadata;
    QDBusObjectPath m_trackId{QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack")};
    double m_posSec = 0;
    QElapsedTimer m_posClock;
    double m_volume = 1;
    double m_rate = 1;
    bool m_canNext = false;
    bool m_canPrev = false;
    bool m_canSeek = false;
};

class MprisRootAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)
public:
    explicit MprisRootAdaptor(Mpris *parent) : QDBusAbstractAdaptor(parent), m(parent) {}
    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("YouTube"); }
    QString desktopEntry() const { return QStringLiteral("yt-webapp"); }
    QStringList supportedUriSchemes() const { return {QStringLiteral("https")}; }
    QStringList supportedMimeTypes() const { return {}; }
public slots:
    void Raise() { m->raise(); }
    void Quit() { m->quit(); }
private:
    Mpris *m;
};

class MprisPlayerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(double MinimumRate READ minimumRate)
    Q_PROPERTY(double MaximumRate READ maximumRate)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl)
public:
    explicit MprisPlayerAdaptor(Mpris *parent) : QDBusAbstractAdaptor(parent), m(parent) {}
    QString playbackStatus() const { return m->playbackStatus(); }
    double rate() const { return m->rate(); }
    void setRate(double r) { m->send(QStringLiteral("rate"), r); }
    double minimumRate() const { return 0.25; }
    double maximumRate() const { return 2.0; }
    QVariantMap metadata() const { return m->metadata(); }
    double volume() const { return m->volume(); }
    void setVolume(double v) { m->send(QStringLiteral("volume"), v); }
    qlonglong position() const { return m->position(); }
    bool canGoNext() const { return m->canGoNext(); }
    bool canGoPrevious() const { return m->canGoPrevious(); }
    bool canPlay() const { return true; }
    bool canPause() const { return true; }
    bool canSeek() const { return m->canSeek(); }
    bool canControl() const { return true; }

    void emitSeeked(qlonglong pos) { emit Seeked(pos); }

public slots:
    void Next() { m->send(QStringLiteral("next")); }
    void Previous() { m->send(QStringLiteral("previous")); }
    void Pause() { m->send(QStringLiteral("pause")); }
    void PlayPause() { m->send(QStringLiteral("playpause")); }
    void Stop() { m->send(QStringLiteral("stop")); }
    void Play() { m->send(QStringLiteral("play")); }
    void Seek(qlonglong offsetUs) { m->send(QStringLiteral("seek"), offsetUs / 1e6); }
    void SetPosition(const QDBusObjectPath &track, qlonglong posUs)
    {
        if (track == m->trackId())
            m->send(QStringLiteral("setpos"), posUs / 1e6);
    }
    void OpenUri(const QString &uri) { m->openUri(uri); }

signals:
    void Seeked(qlonglong Position);

private:
    Mpris *m;
};
