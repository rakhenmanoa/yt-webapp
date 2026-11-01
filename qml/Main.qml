import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine

ApplicationWindow {
    id: win
    width: 420
    height: 860
    title: web.title !== "" ? web.title : "YouTube"

    readonly property bool onVideo: backend.isVideoUrl(web.url)
    property string lastAutoMpvUrl: ""

    // Mode immersif : barre de l'appli masquée, affichée temporairement
    // par la poignée.
    property bool barPeek: false
    readonly property bool barShown: !web.isFullScreen && (!backend.immersive || barPeek)

    // Paysage automatique : en plein écran, une vidéo paysage regardée avec le
    // téléphone en portrait fait pivoter le contenu de l'appli de 90°.
    // (Sous Wayland, une appli ne peut pas tourner l'écran du système.)
    property bool wantLandscape: false
    readonly property bool rotated: wantLandscape && web.isFullScreen && win.height > win.width

    readonly property string videoSizeJs:
        "(function () {" +
        "  var f = document.fullscreenElement || document.webkitFullscreenElement;" +
        "  var v = f && (f.tagName === 'VIDEO' ? f : f.querySelector('video'));" +
        "  v = v || document.querySelector('video');" +
        "  return v ? [v.videoWidth, v.videoHeight] : null;" +
        "})()"

    readonly property string pauseJs:
        "document.querySelectorAll('video').forEach(function (v) { v.pause(); });"

    function applyScripts() {
        web.userScripts.clear();
        var s = WebEngine.script();
        s.name = "yt-webapp";
        s.sourceCode = backend.injectScript;
        s.injectionPoint = WebEngineScript.DocumentCreation;
        s.worldId = WebEngineScript.MainWorld;
        s.runsOnSubFrames = true;
        web.userScripts.insert(s);
    }

    // Plein écran (masque les barres de Plasma Mobile) en mode immersif ou
    // pendant une vidéo plein écran.
    function applyWindowMode() {
        if (web.isFullScreen || backend.immersive)
            win.showFullScreen();
        else
            win.showNormal();
    }

    function showWindow() {
        applyWindowMode();
        win.raise();
        win.requestActivate();
    }

    function peekBar() {
        barPeek = true;
        peekTimer.restart();
    }

    function playInMpv(url) {
        web.runJavaScript(pauseJs);
        backend.openInMpv(url);
    }

    function toast(text) {
        toastLabel.text = text;
        toastPopup.open();
        toastTimer.restart();
    }

    Component.onCompleted: {
        if (!backend.startHidden)
            applyWindowMode();
    }

    // Fermeture : on garde l'appli en mémoire (réouverture instantanée).
    // Avec la lecture en arrière-plan, le son continue et se pilote depuis
    // Plasma (écran de verrouillage, widget multimédia).
    onClosing: function (close) {
        if (!backend.keepInMemory || !backend.backgroundPlay)
            web.runJavaScript(pauseJs);
        if (backend.keepInMemory) {
            close.accepted = false;
            win.hide();
        } else {
            backend.quit();
        }
    }

    Connections {
        target: backend
        function onShowRequested(url) {
            if (url.toString() !== "")
                web.url = url;
            win.showWindow();
        }
        function onMessage(text) { win.toast(text); }
    }

    // Commandes MPRIS (Plasma) → page YouTube.
    Connections {
        target: mpris
        function onCommand(action, arg) {
            web.runJavaScript("window.__ytmprisCmd && window.__ytmprisCmd("
                              + JSON.stringify(action) + "," + Number(arg) + ");");
        }
    }

    Item {
        id: stage
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        // En mode immersif, la barre passe par-dessus la page sans la
        // redimensionner.
        anchors.bottom: (backend.immersive || !win.barShown) ? parent.bottom : bar.top

    WebEngineView {
        id: web
        anchors.centerIn: parent
        width: win.rotated ? parent.height : parent.width
        height: win.rotated ? parent.width : parent.height
        rotation: win.rotated ? (backend.landscapeFlip ? -90 : 90) : 0
        // En rendu matériel, la vidéo peut être une couche à part qui ne suit
        // pas la rotation (vidéo décalée, commandes bien placées). On compose
        // donc toute la page dans une texture, puis on fait pivoter celle-ci
        // d'un bloc. Seulement pendant la rotation (coût : une copie GPU).
        layer.enabled: win.rotated
        profile: ytProfile
        settings.fullScreenSupportEnabled: true
        settings.playbackRequiresUserGesture: false
        settings.webGLEnabled: false
        settings.pluginsEnabled: false
        settings.showScrollBars: false

        Component.onCompleted: {
            win.applyScripts();
            web.url = backend.startUrl.toString() !== "" ? backend.startUrl : backend.homeUrl;
        }

        // Pont page → appli : le script injecté décrit l'état du lecteur.
        onJavaScriptConsoleMessage: function (level, message, lineNumber, sourceID) {
            if (message.startsWith("__ytmpris__"))
                mpris.update(message.substring(11));
        }

        // Tout ce qui n'est pas YouTube (ou la connexion Google) part dans le
        // navigateur par défaut.
        onNavigationRequested: function (request) {
            if (!request.isMainFrame)
                return;
            if (!backend.isAllowedUrl(request.url)) {
                request.reject();
                backend.openExternally(request.url);
            }
        }

        onNewWindowRequested: function (request) {
            if (backend.isAllowedUrl(request.requestedUrl))
                web.url = request.requestedUrl;
            else
                backend.openExternally(request.requestedUrl);
        }

        onFullScreenRequested: function (request) {
            request.accept();
            if (request.toggleOn) {
                win.showFullScreen();
                if (backend.autoLandscape) {
                    web.runJavaScript(win.videoSizeJs, function (size) {
                        win.wantLandscape = !!size && size[0] > size[1];
                    });
                }
            } else {
                win.wantLandscape = false;
                if (!backend.immersive)
                    win.showNormal();
            }
        }

        // YouTube navigue sans recharger la page : on suit l'URL.
        onUrlChanged: {
            if (backend.autoMpv && backend.mpvAvailable && backend.isVideoUrl(web.url)) {
                var u = web.url.toString();
                if (u !== win.lastAutoMpvUrl) {
                    win.lastAutoMpvUrl = u;
                    win.playInMpv(web.url);
                }
            }
        }

        onRenderProcessTerminated: function (status, code) {
            mpris.update("null");
            reloadTimer.start();
        }

        Keys.onEscapePressed: {
            if (web.isFullScreen)
                web.triggerWebAction(WebEngineView.ExitFullScreen);
        }
    }
    }

    // Barre de chargement fine en haut.
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        height: 3
        width: parent.width * web.loadProgress / 100
        color: palette.highlight
        visible: web.loading
    }

    Timer {
        id: reloadTimer
        interval: 300
        onTriggered: web.reload()
    }

    Timer {
        id: peekTimer
        interval: 5000
        onTriggered: {
            if (!menu.visible)
                win.barPeek = false;
        }
    }

    // Poignée du mode immersif : un tap affiche la barre quelques secondes.
    RoundButton {
        id: handle
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 14
        width: 44
        height: 44
        opacity: 0.45
        visible: backend.immersive && !win.barShown && !web.isFullScreen
        icon.name: "go-up"
        text: "⋯"
        display: AbstractButton.IconOnly
        onClicked: win.peekBar()
    }

    ToolBar {
        id: bar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: win.barShown

        RowLayout {
            anchors.fill: parent
            spacing: 0

            ToolButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                icon.name: "go-previous"
                text: "Retour"
                display: AbstractButton.TextUnderIcon
                enabled: web.canGoBack
                onClicked: { web.goBack(); if (win.barPeek) peekTimer.restart(); }
            }
            ToolButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                icon.name: "go-home"
                text: "Accueil"
                display: AbstractButton.TextUnderIcon
                onClicked: { web.url = backend.homeUrl; if (win.barPeek) peekTimer.restart(); }
            }
            ToolButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                icon.name: web.loading ? "process-stop" : "view-refresh"
                text: web.loading ? "Arrêter" : "Recharger"
                display: AbstractButton.TextUnderIcon
                onClicked: { web.loading ? web.stop() : web.reload(); if (win.barPeek) peekTimer.restart(); }
            }
            ToolButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                icon.name: backend.pipMode ? "view-restore" : "media-playback-start"
                text: backend.pipMode ? "Incruster" : "mpv"
                display: AbstractButton.TextUnderIcon
                visible: win.onVideo && backend.mpvAvailable
                onClicked: win.playInMpv(web.url)
            }
            ToolButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                icon.name: "application-menu"
                text: "Menu"
                display: AbstractButton.TextUnderIcon
                onClicked: menu.open()

                Menu {
                    id: menu
                    y: -height
                    onClosed: if (win.barPeek) peekTimer.restart()

                    MenuItem {
                        text: "Mode immersif (masquer les barres)"
                        checkable: true
                        checked: backend.immersive
                        onToggled: {
                            backend.immersive = checked;
                            win.barPeek = false;
                            win.applyWindowMode();
                        }
                    }
                    MenuItem {
                        text: "Paysage auto en plein écran"
                        checkable: true
                        checked: backend.autoLandscape
                        onToggled: backend.autoLandscape = checked
                    }
                    MenuItem {
                        text: "Inverser le sens du paysage"
                        visible: backend.autoLandscape
                        height: visible ? implicitHeight : 0
                        checkable: true
                        checked: backend.landscapeFlip
                        onToggled: backend.landscapeFlip = checked
                    }
                    MenuItem {
                        text: "Lecture en arrière-plan"
                        checkable: true
                        checked: backend.backgroundPlay
                        onToggled: {
                            backend.backgroundPlay = checked;
                            win.applyScripts();
                            web.reload();
                        }
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: "Forcer le H.264"
                        checkable: true
                        checked: backend.forceH264
                        onToggled: {
                            backend.forceH264 = checked;
                            win.applyScripts();
                            web.reload();
                        }
                    }
                    MenuItem {
                        text: "Bloquer les pubs"
                        checkable: true
                        checked: backend.blockAds
                        onToggled: {
                            backend.blockAds = checked;
                            win.applyScripts();
                            web.reload();
                        }
                    }
                    MenuItem {
                        text: "Toujours lire dans mpv"
                        checkable: true
                        enabled: backend.mpvAvailable
                        checked: backend.autoMpv
                        onToggled: backend.autoMpv = checked
                    }
                    MenuItem {
                        text: "Incrustation (mpv flottant)"
                        checkable: true
                        enabled: backend.mpvAvailable
                        checked: backend.pipMode
                        onToggled: backend.pipMode = checked
                    }
                    MenuItem {
                        text: "Replacer l'incrustation"
                        visible: backend.pipMode
                        height: visible ? implicitHeight : 0
                        onTriggered: backend.resetPipPosition()
                    }
                    MenuItem {
                        text: "Rester en mémoire"
                        checkable: true
                        checked: backend.keepInMemory
                        onToggled: backend.keepInMemory = checked
                    }
                    Menu {
                        title: "Qualité mpv"
                        enabled: backend.mpvAvailable
                        Repeater {
                            model: [360, 480, 720, 1080]
                            MenuItem {
                                text: modelData + "p"
                                checkable: true
                                checked: backend.mpvHeight === modelData
                                onTriggered: backend.mpvHeight = modelData
                            }
                        }
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: "Vider le cache"
                        onTriggered: backend.clearCache()
                    }
                    MenuItem {
                        text: "Quitter"
                        onTriggered: backend.quit()
                    }
                    MenuItem {
                        text: "Version " + appVersion
                        enabled: false
                    }
                }
            }
        }
    }

    Popup {
        id: toastPopup
        x: (win.width - width) / 2
        y: win.height - height - 80
        padding: 12
        closePolicy: Popup.NoAutoClose
        Label { id: toastLabel }
    }
    Timer {
        id: toastTimer
        interval: 2200
        onTriggered: toastPopup.close()
    }
}
