#pragma once

#include <QWebEngineUrlRequestInterceptor>
#include <atomic>

// Bloque pubs / télémétrie avant même que la requête parte,
// et (en mode « toujours mpv ») les flux vidéo du lecteur web.
class Interceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

    std::atomic<bool> blockAds{true};
    std::atomic<bool> blockWebVideo{false};
};
