// Injecté dans chaque page YouTube avant ses propres scripts.
// window.__ytwebapp = {h264: bool, ads: bool} est préfixé par l'appli.
(function () {
    'use strict';
    const cfg = window.__ytwebapp || {};

    // --- 1. Forcer le H.264 -------------------------------------------------
    // YouTube choisit VP9/AV1 si le navigateur les annonce. On les masque pour
    // qu'il serve du H.264, bien plus léger à décoder (et décodable en matériel).
    if (cfg.h264 && window.MediaSource && MediaSource.isTypeSupported) {
        const blocked = /vp0?9|vp8|av01/i;
        const origIsType = MediaSource.isTypeSupported.bind(MediaSource);

        // Garde-fou : si ce build ne lit pas le H.264, on ne touche à rien,
        // sinon plus aucune vidéo ne marcherait.
        if (origIsType('video/mp4; codecs="avc1.4d401f"')) {
            MediaSource.isTypeSupported = (t) => (blocked.test(t) ? false : origIsType(t));

            const origCanPlay = HTMLMediaElement.prototype.canPlayType;
            HTMLMediaElement.prototype.canPlayType = function (t) {
                return blocked.test(t) ? '' : origCanPlay.call(this, t);
            };

            const mc = navigator.mediaCapabilities;
            if (mc && mc.decodingInfo) {
                const origInfo = mc.decodingInfo.bind(mc);
                mc.decodingInfo = (c) =>
                    c && c.video && blocked.test(c.video.contentType)
                        ? Promise.resolve({ supported: false, smooth: false, powerEfficient: false })
                        : origInfo(c);
            }
        } else {
            console.warn('[yt-webapp] H.264 absent de ce QtWebEngine : forçage désactivé.');
        }
    }

    // --- 2. Pubs restantes ---------------------------------------------------
    // Les requêtes publicitaires sont déjà bloquées côté C++ ; ici on masque les
    // emplacements vides et on saute les pubs vidéo intégrées au flux.
    if (cfg.ads) {
        const css = [
            'ytm-promoted-sparkles-web-renderer',
            'ytm-promoted-video-renderer',
            'ytm-companion-ad-renderer',
            'ytm-ad-slot-renderer',
            'ad-slot-renderer',
            'ytd-ad-slot-renderer',
            '.ytp-ad-overlay-container',
            '#player-ads',
        ].join(',') + '{display:none!important}';

        const addStyle = () => {
            const s = document.createElement('style');
            s.textContent = css;
            (document.head || document.documentElement).appendChild(s);
        };
        if (document.documentElement) addStyle();
        else document.addEventListener('DOMContentLoaded', addStyle, { once: true });

        setInterval(() => {
            const v = document.querySelector('.ad-showing video');
            if (v) {
                v.muted = true;
                if (isFinite(v.duration) && v.duration > 0) v.currentTime = v.duration;
            }
            const skip = document.querySelector(
                '.ytp-ad-skip-button, .ytp-ad-skip-button-modern, .ytp-skip-ad-button');
            if (skip) skip.click();
        }, 500);
    }
})();
