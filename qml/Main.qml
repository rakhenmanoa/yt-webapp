import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine

ApplicationWindow {
    id: win
    width: 420
    height: 860
    visible: !backend.startHidden
    title: web.title !== "" ? web.title : "YouTube"

    readonly property bool onVideo: backend.isVideoUrl(web.url)
    property string lastAutoMpvUrl: ""

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

    function playInMpv(url) {
        web.runJavaScript(pauseJs);
        backend.openInMpv(url);
    }

    function toast(text) {
        toastLabel.text = text;
        toastPopup.open();
        toastTimer.restart();
    }

    // Fermeture : on garde l'appli en mémoire (réouverture instantanée).
    onClosing: function (close) {
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
            win.show();
            win.raise();
            win.requestActivate();
        }
        function onMessage(text) { win.toast(text); }
    }

    WebEngineView {
        id: web
        anchors.fill: parent
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
            if (request.toggleOn)
                win.showFullScreen();
            else
                win.showNormal();
        }

        // YouTube navigue sans recharger la page : on suit l'URL.
        onUrlChanged: {
            if (backend.autoMpv && backend.mpvAvailable && win.onVideo) {
                var u = web.url.toString();
                if (u !== win.lastAutoMpvUrl) {
                    win.lastAutoMpvUrl = u;
                    win.playInMpv(web.url);
                }
            }
        }

        onRenderProcessTerminated: function (status, code) {
            reloadTimer.start();
        }

        Keys.onEscapePressed: {
            if (web.isFullScreen)
                web.triggerWebAction(WebEngineView.ExitFullScreen);
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

    footer: ToolBar {
        visible: !web.isFullScreen
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
                onClicked: web.goBack()
            }
            ToolButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                icon.name: "go-home"
                text: "Accueil"
                display: AbstractButton.TextUnderIcon
                onClicked: web.url = backend.homeUrl
            }
            ToolButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                icon.name: web.loading ? "process-stop" : "view-refresh"
                text: web.loading ? "Arrêter" : "Recharger"
                display: AbstractButton.TextUnderIcon
                onClicked: web.loading ? web.stop() : web.reload()
            }
            ToolButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                icon.name: "media-playback-start"
                text: "mpv"
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
