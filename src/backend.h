#pragma once

#include <QObject>
#include <QSettings>
#include <QUrl>

class Interceptor;
class QQuickWebEngineProfile;

// Réglages, lancement de mpv et utilitaires exposés au QML.
class Backend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool forceH264 READ forceH264 WRITE setForceH264 NOTIFY settingsChanged)
    Q_PROPERTY(bool blockAds READ blockAds WRITE setBlockAds NOTIFY settingsChanged)
    Q_PROPERTY(bool autoMpv READ autoMpv WRITE setAutoMpv NOTIFY settingsChanged)
    Q_PROPERTY(bool keepInMemory READ keepInMemory WRITE setKeepInMemory NOTIFY settingsChanged)
    Q_PROPERTY(int mpvHeight READ mpvHeight WRITE setMpvHeight NOTIFY settingsChanged)
    Q_PROPERTY(bool mpvAvailable READ mpvAvailable CONSTANT)
    Q_PROPERTY(bool startHidden READ startHidden CONSTANT)
    Q_PROPERTY(QUrl startUrl READ startUrl CONSTANT)
    Q_PROPERTY(QUrl homeUrl READ homeUrl CONSTANT)
    Q_PROPERTY(QString injectScript READ injectScript NOTIFY settingsChanged)

public:
    Backend(Interceptor *interceptor, QQuickWebEngineProfile *profile,
            bool startHidden, const QUrl &startUrl, QObject *parent = nullptr);

    bool forceH264() const;
    void setForceH264(bool v);
    bool blockAds() const;
    void setBlockAds(bool v);
    bool autoMpv() const;
    void setAutoMpv(bool v);
    bool keepInMemory() const;
    void setKeepInMemory(bool v);
    int mpvHeight() const;
    void setMpvHeight(int v);

    bool mpvAvailable() const { return m_mpvAvailable; }
    bool startHidden() const { return m_startHidden; }
    QUrl startUrl() const { return m_startUrl; }
    QUrl homeUrl() const { return QUrl(QStringLiteral("https://m.youtube.com/")); }
    QString injectScript() const;

    Q_INVOKABLE bool isAllowedUrl(const QUrl &url) const;
    Q_INVOKABLE bool isVideoUrl(const QUrl &url) const;
    Q_INVOKABLE bool openInMpv(const QUrl &url);
    Q_INVOKABLE void openExternally(const QUrl &url);
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE void quit();

    // Appelé par l'instance unique quand l'appli est relancée.
    void activate(const QUrl &url);

signals:
    void settingsChanged();
    void showRequested(const QUrl &url);
    void message(const QString &text);

private:
    void applyToInterceptor();

    Interceptor *m_interceptor;
    QQuickWebEngineProfile *m_profile;
    QSettings m_settings;
    QString m_scriptTemplate;
    bool m_mpvAvailable = false;
    bool m_startHidden = false;
    QUrl m_startUrl;
};
