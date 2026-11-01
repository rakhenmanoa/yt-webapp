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

    // --- 3. Lecture en arrière-plan -----------------------------------------
    // m.youtube.com met la vidéo en pause quand la page devient invisible.
    // On lui fait croire qu'elle reste toujours visible.
    if (cfg.bg) {
        const def = (name, value) => {
            try {
                Object.defineProperty(Document.prototype, name, {
                    configurable: true, get: () => value,
                });
            } catch (e) { /* ignoré */ }
        };
        def('hidden', false);
        def('webkitHidden', false);
        def('visibilityState', 'visible');
        def('webkitVisibilityState', 'visible');
        for (const t of ['visibilitychange', 'webkitvisibilitychange']) {
            window.addEventListener(t, (e) => e.stopImmediatePropagation(), true);
            document.addEventListener(t, (e) => e.stopImmediatePropagation(), true);
        }
    }

    // --- 4. Orientation cohérente pendant le paysage auto -------------------
    // Quand l'appli fait pivoter sa vue, la page voit des dimensions paysage
    // mais QtWebEngine lui annonce toujours une orientation portrait
    // (screen.orientation = portrait-primary). Le lecteur YouTube, qui se fie
    // à l'orientation, calcule alors une vidéo petite et décalée. L'appli
    // appelle __ytwSetLandscape(true/false) pour forcer une orientation
    // paysage le temps de la rotation.
    if (window.top === window) {
        let forced = null;
        const so = screen.orientation;
        if (so) {
            const proto = Object.getPrototypeOf(so);
            const dType = Object.getOwnPropertyDescriptor(proto, 'type');
            const dAngle = Object.getOwnPropertyDescriptor(proto, 'angle');
            if (dType && dAngle) {
                Object.defineProperty(proto, 'type', {
                    configurable: true,
                    get() { return forced ? forced.type : dType.get.call(this); },
                });
                Object.defineProperty(proto, 'angle', {
                    configurable: true,
                    get() { return forced ? forced.angle : dAngle.get.call(this); },
                });
            }
        }
        const hadOrientation = 'orientation' in window;
        const origOrientation = hadOrientation ? window.orientation : undefined;
        try {
            Object.defineProperty(window, 'orientation', {
                configurable: true,
                get() { return forced ? forced.legacy : origOrientation; },
            });
        } catch (e) { /* ignoré */ }

        window.__ytwSetLandscape = (on, flip) => {
            forced = on
                ? (flip ? { type: 'landscape-secondary', angle: 270, legacy: -90 }
                        : { type: 'landscape-primary', angle: 90, legacy: 90 })
                : null;
            try { if (so) so.dispatchEvent(new Event('change')); } catch (e) { /* ignoré */ }
            window.dispatchEvent(new Event('orientationchange'));
            window.dispatchEvent(new Event('resize'));
            // Certains lecteurs recalculent au prochain rafraîchissement.
            setTimeout(() => window.dispatchEvent(new Event('resize')), 300);
        };
    }

    // --- 5. Intégration Plasma (MPRIS) ---------------------------------------
    // Même principe que Plasma Browser Integration : on écoute la Media Session
    // de YouTube et l'élément <video> actif, et on transmet l'état à l'appli.
    if (cfg.mpris && window.top === window) {
        const PFX = '__ytmpris__';
        const handlers = {};
        let meta = null;
        let active = null;
        let pendingSeeked = false;
        let timer = 0;
        let lastSent = '';

        const ms = navigator.mediaSession;
        if (ms) {
            const origSet = ms.setActionHandler.bind(ms);
            ms.setActionHandler = (name, cb) => {
                handlers[name] = cb;
                schedule();
                return origSet(name, cb);
            };
            const proto = Object.getPrototypeOf(ms);
            const md = Object.getOwnPropertyDescriptor(proto, 'metadata');
            if (md && md.get && md.set) {
                Object.defineProperty(ms, 'metadata', {
                    configurable: true,
                    get: () => md.get.call(ms),
                    set: (v) => { md.set.call(ms, v); meta = v; schedule(); },
                });
            }
        }

        // Ignore les petits sons (< 8 s), comme Plasma Browser Integration.
        const isShort = (v) => isFinite(v.duration) && v.duration > 0 && v.duration < 8;

        const videoId = () => {
            const p = new URLSearchParams(location.search).get('v');
            if (p) return p;
            const m = location.pathname.match(/^\/(?:shorts|live)\/([\w-]{11})/);
            return m ? m[1] : '';
        };

        const bestArt = (art) => {
            let best = '', size = -1;
            for (const a of art || []) {
                const s = parseInt(String(a.sizes || '0').split(/[x ]/)[0], 10) || 0;
                if (a.src && s >= size) { best = a.src; size = s; }
            }
            return best;
        };

        function send() {
            timer = 0;
            const v = active;
            if (!v || !v.isConnected) {
                if (lastSent !== 'null') { console.debug(PFX + 'null'); lastSent = 'null'; }
                return;
            }
            const m = meta || {};
            const id = videoId();
            const state = {
                st: v.ended ? 'Stopped' : v.paused ? 'Paused' : 'Playing',
                title: m.title || document.title.replace(/ - YouTube$/, ''),
                artist: m.artist || '',
                album: m.album || '',
                art: bestArt(m.artwork) || (id ? 'https://i.ytimg.com/vi/' + id + '/hqdefault.jpg' : ''),
                url: location.href,
                dur: isFinite(v.duration) ? v.duration : -1,
                pos: v.currentTime || 0,
                rate: v.playbackRate || 1,
                vol: v.muted ? 0 : v.volume,
                next: typeof handlers.nexttrack === 'function',
                prev: true,
                seek: isFinite(v.duration),
                seeked: pendingSeeked,
            };
            pendingSeeked = false;
            const msg = JSON.stringify(state);
            if (msg !== lastSent || state.seeked) {
                lastSent = msg;
                console.debug(PFX + msg);
            }
        }

        function schedule(seeked) {
            if (seeked) pendingSeeked = true;
            if (!timer) timer = setTimeout(send, 150);
        }

        const onMedia = (e) => {
            const v = e.target;
            if (!(v instanceof HTMLMediaElement) || isShort(v)) return;
            if (v !== active && (e.type === 'play' || e.type === 'playing' || !active)) active = v;
            if (v === active) schedule(e.type === 'seeked');
        };
        for (const t of ['play', 'playing', 'pause', 'ended', 'durationchange', 'seeked',
                         'ratechange', 'volumechange', 'loadedmetadata', 'emptied']) {
            document.addEventListener(t, onMedia, true);
        }

        // Commandes venant de Plasma (via l'appli).
        window.__ytmprisCmd = (action, x) => {
            const v = active;
            const h = (n) => {
                if (typeof handlers[n] !== 'function') return false;
                try { handlers[n]({ action: n, seekTime: x, seekOffset: Math.abs(x) }); return true; }
                catch (e) { return false; }
            };
            if (!v && action !== 'next') return;
            switch (action) {
            case 'play': if (!h('play')) v.play(); break;
            case 'pause': if (!h('pause')) v.pause(); break;
            case 'playpause':
                if (v.paused) { if (!h('play')) v.play(); } else if (!h('pause')) v.pause();
                break;
            case 'stop': if (!h('stop')) v.pause(); break;
            case 'next': h('nexttrack'); break;
            case 'previous': if (!h('previoustrack')) v.currentTime = 0; break;
            case 'seek': v.currentTime = Math.max(0, v.currentTime + x); break;
            case 'setpos': v.currentTime = Math.max(0, x); break;
            case 'volume': v.muted = false; v.volume = Math.min(1, Math.max(0, x)); break;
            case 'rate': v.playbackRate = x; break;
            }
        };
    }
})();
