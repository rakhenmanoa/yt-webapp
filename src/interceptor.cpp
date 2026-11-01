#include "interceptor.h"

#include <QUrl>

namespace {

bool hostMatches(const QString &host, const char *domain)
{
    const QLatin1String d(domain);
    return host == d || (host.endsWith(d) && host.at(host.size() - d.size() - 1) == QLatin1Char('.'));
}

const char *const kAdHosts[] = {
    "doubleclick.net",
    "googleadservices.com",
    "googlesyndication.com",
    "google-analytics.com",
    "googletagservices.com",
    "googletagmanager.com",
    "adservice.google.com",
    "imasdk.googleapis.com",
};

// Chemins YouTube de pub et de télémétrie (n'affectent pas la lecture).
const char *const kAdPaths[] = {
    "/pagead/",
    "/ptracking",
    "/api/stats/ads",
    "/api/stats/qoe",
    "/youtubei/v1/log_event",
    "/get_midroll_info",
    "/csi_204",
    "/generate_204",
};

} // namespace

void Interceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    const QUrl url = info.requestUrl();
    const QString host = url.host();

    if (blockAds.load(std::memory_order_relaxed)) {
        for (const char *h : kAdHosts) {
            if (hostMatches(host, h)) {
                info.block(true);
                return;
            }
        }
        if (hostMatches(host, "youtube.com") || hostMatches(host, "googlevideo.com")) {
            const QString path = url.path();
            for (const char *p : kAdPaths) {
                if (path.startsWith(QLatin1String(p))) {
                    info.block(true);
                    return;
                }
            }
        }
    }

    if (blockWebVideo.load(std::memory_order_relaxed)
        && hostMatches(host, "googlevideo.com")
        && url.path().startsWith(QLatin1String("/videoplayback"))) {
        info.block(true);
    }
}
