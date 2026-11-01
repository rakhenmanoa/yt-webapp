# yt-webapp

YouTube en appli dédiée, sur QtWebEngine (Chromium), pensée pour Plasma Mobile.

## Ce qu'elle fait

- **Profil persistant.** Connexion au compte conservée, 512 Mo de cache HTTP et cache de code JavaScript sur disque.
- **Reste en mémoire.** Fermer la fenêtre la cache seulement, donc la réouverture est instantanée. Pour quitter vraiment : menu → Quitter.
- **Instance unique.** Relancer l'appli ou ouvrir un lien YouTube réaffiche la fenêtre existante.
- **Pubs et télémétrie bloquées** avant le chargement. Les pubs vidéo restantes sont coupées et sautées.
- **H.264 forcé.** YouTube sert du H.264 au lieu du VP9 ou de l'AV1. Le forçage se désactive tout seul si ce QtWebEngine ne lit pas le H.264.
- **YouTube seulement.** Les liens externes s'ouvrent dans le navigateur par défaut. La connexion Google reste possible.
- **Bouton « mpv »** sur les vidéos, plus une option « Toujours lire dans mpv » : la page ne télécharge alors plus la vidéo et tout part dans mpv.
- **Incrustation.** Avec l'option du menu « Incrustation (mpv flottant) », le bouton devient « Incruster » : la vidéo part dans une petite fenêtre mpv sans bordure, gardée au-dessus des autres applis, en bas à droite. Tu peux continuer à naviguer pendant ce temps.
- **Intégration Plasma (MPRIS).** La vidéo en cours apparaît, avec titre, chaîne, miniature et position, sur l'écran de verrouillage, dans le widget multimédia et dans KDE Connect. On peut la piloter de là, ainsi qu'avec les boutons du casque. C'est le même principe que Plasma Browser Integration, mais intégré à l'appli : pas besoin d'extension ni de programme séparé.
- **Lecture en arrière-plan.** Fermer la fenêtre ne coupe plus le son : la lecture continue écran éteint et se pilote depuis l'écran de verrouillage. Désactivable dans le menu.
- **Mode immersif.** Menu → « Mode immersif (masquer les barres) » : l'appli passe en plein écran, ce qui masque les barres de Plasma Mobile en haut et en bas, et sa propre barre disparaît. Une petite poignée ronde en bas à droite l'affiche par-dessus la page ; elle se masque d'elle-même après 5 secondes.
- **Paysage auto en plein écran.** Quand une vidéo paysage passe en plein écran alors que le téléphone est en portrait, le contenu de l'appli pivote de 90°. La page reçoit aussi une orientation paysage cohérente, puisque QtWebEngine annonce sinon « portrait ». La vidéo est forcée en CSS à remplir l'écran pivoté : sans cela, le lecteur de m.youtube.com lui garde sa taille et sa position d'avant le plein écran. Une ligne `[yt-webapp diag]` s'affiche dans le terminal à chaque rotation, pour le diagnostic. Les vidéos verticales (Shorts) ne pivotent pas. Si le sens ne te convient pas : menu → « Inverser le sens du paysage ».
  - En secours, l'option « Tourner l'écran du système » fait tourner l'écran réel via `kscreen-doctor` (paquet `libkscreen`) le temps du plein écran. Il est restauré à la sortie, à la fermeture de l'appli, et même si elle est arrêtée avec `pkill`.
- **Rendu logiciel (anti-image verte).** Sur le téléphone, la composition par la carte graphique (GPU) de Chromium affiche une image verte au démarrage de chaque vidéo. Cette option du menu fait composer la page par le processeur, ce qui supprime le vert mais demande plus de calcul. Elle s'applique au redémarrage, que l'appli propose aussitôt.
- **Version ordinateur.** Menu → « Version ordinateur » : l'appli s'annonce comme Chrome pour Linux et charge www.youtube.com au lieu de m.youtube.com, sur la même page. Sur un écran étroit, le zoom passe automatiquement à 67 % ; il reste réglable dans menu → « Zoom ». C'est le mode par défaut sur PC (x86_64), avec une fenêtre de 1200×800.
- **Réglages Chromium par défaut** : 4 threads de peinture, moins de pauses du ramasse-miettes V8, services inutiles coupés.

## Installation sur le téléphone (Arch Linux ARM)

Outils de compilation :

    sudo pacman -S --needed base-devel cmake qt6-webengine qt6-declarative

Pour le bouton mpv (facultatif) :

    sudo pacman -S --needed mpv yt-dlp

Aller dans le dossier du paquet :

    cd packaging/arch

Compiler et installer :

    makepkg -sif

L'option `-f` force la reconstruction : sans elle, makepkg réinstalle l'ancien paquet s'il en trouve un de même version dans le dossier.

## Mettre à jour

Depuis le dossier `packaging/arch` :

    git pull

    makepkg -sif

Puis quitte l'ancienne instance (menu → Quitter), sinon la relance réaffiche celle qui tourne encore en mémoire. La version et le commit compilés s'affichent en bas du menu.

## Installation sur PC (x86_64)

Le code est le même que sur le téléphone. Sur PC, l'appli démarre en version ordinateur, dans une fenêtre de 1200×800. Il faut Qt 6.4 ou plus récent avec QtWebEngine.

**Arch Linux**, avec le même paquet que sur le téléphone :

    sudo pacman -S --needed base-devel cmake git qt6-webengine qt6-declarative mpv yt-dlp

    cd packaging/arch

    makepkg -sif

**Debian / Ubuntu**, dépendances :

    sudo apt install cmake g++ qt6-webengine-dev qt6-declarative-dev qml6-module-qtwebengine qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-window qml6-module-qtqml-workerscript mpv yt-dlp

**Fedora**, dépendances :

    sudo dnf install cmake gcc-c++ qt6-qtwebengine-devel qt6-qtdeclarative-devel mpv yt-dlp

Puis, sur Debian, Ubuntu ou Fedora, depuis le dossier du projet :

    cmake -B build -S . -DCMAKE_INSTALL_PREFIX=/usr

    cmake --build build

    sudo cmake --install build

## Préchargement au démarrage (facultatif)

L'appli se lance cachée à l'ouverture de session et charge YouTube en arrière-plan.

    mkdir -p ~/.config/autostart

    cp data/yt-webapp-autostart.desktop ~/.config/autostart/

## Incrustation : comment ça marche

QtWebEngine désactive l'incrustation intégrée de Chromium (Picture-in-Picture), donc l'appli passe par mpv. Sous Wayland, une fenêtre ne peut pas se placer seule au-dessus des autres. Au premier usage, l'appli ajoute donc une règle KWin nommée `yt-webapp-pip` dans `~/.config/kwinrulesrc`, sans toucher à tes autres règles. Cette règle :

- repère la fenêtre mpv à son titre, qui commence par `[PiP]` ;
- la garde au-dessus des autres fenêtres, sans bordure ni maximisation ;
- lui donne au départ 55 % de la largeur de l'écran, en bas à droite.

Dans mpv, un double tap passe en plein écran. Pour fermer l'incrustation, ferme sa fenêtre depuis le sélecteur de tâches.

Si la position ne convient plus (rotation, changement d'échelle), utilise menu → « Replacer l'incrustation ». Pour ajuster la règle à la main, ou la supprimer : Configuration du système → Gestion des fenêtres → Règles de fenêtres → « yt-webapp : incrustation mpv ».

Plasma Mobile maximise normalement toutes les fenêtres. La règle force la fenêtre à ne pas l'être, mais selon la version de Plasma Mobile, il faudra peut-être activer le mode « convergence » (fenêtres flottantes) pour que l'incrustation reste petite.

## Intégration Plasma : comment ça marche

1. Le script injecté écoute l'élément `<video>` actif (lecture, pause, position…) et intercepte la Media Session de YouTube, par laquelle la page annonce titre, chaîne, miniature et ses boutons précédent/suivant.
2. Il transmet l'état à l'appli via la console JavaScript (messages préfixés `__ytmpris__`, masqués du terminal).
3. L'appli publie un lecteur MPRIS2 sur D-Bus (`org.mpris.MediaPlayer2.ytwebapp`), seulement quand une vidéo est chargée.
4. Les commandes de Plasma (lecture, pause, avance, volume…) redescendent dans la page.

Le MPRIS générique de Chromium est désactivé (`HardwareMediaKeyHandling`) pour éviter un doublon nommé « Chromium ».

Pour vérifier depuis un terminal du téléphone :

    busctl --user introspect org.mpris.MediaPlayer2.ytwebapp /org/mpris/MediaPlayer2

## Réglages de performance

L'appli utilise les réglages Chromium par défaut, sauf si la variable `QTWEBENGINE_CHROMIUM_FLAGS` est définie. Pour tester tes propres réglages :

    QTWEBENGINE_CHROMIUM_FLAGS="--num-raster-threads=4" yt-webapp

Pour la lancer sur les gros cœurs (4 à 7) :

    taskset -c 4-7 yt-webapp

Si le gain se confirme, copie le lanceur dans ton dossier personnel (il prend le pas sur celui du système) :

    cp /usr/share/applications/yt-webapp.desktop ~/.local/share/applications/

Puis modifie sa ligne `Exec=` en `Exec=taskset -c 4-7 yt-webapp %u`.

## Vérifier le H.264 et mesurer

Lance l'appli avec le débogage à distance :

    QTWEBENGINE_CHROMIUM_FLAGS="--remote-debugging-port=9222 --remote-allow-origins=*" yt-webapp

Depuis le PC, crée un tunnel vers le téléphone :

    ssh -L 9222:localhost:9222 utilisateur@telephone

Puis ouvre `chrome://inspect` dans Chromium sur le PC.

- La console affiche `[yt-webapp] H.264 absent…` si ce QtWebEngine n'a pas les codecs propriétaires.
- Pendant une vidéo, la ligne « Stats for nerds » (clic long sur la vidéo) indique le codec utilisé : `avc1` = H.264.
- L'onglet Performance montre si le temps part dans le réseau, les scripts ou le rendu.

## Fichiers

- `src/main.cpp` : démarrage, profil, instance unique, réglages Chromium
- `src/interceptor.cpp` : blocage des requêtes
- `src/backend.cpp` : réglages, mpv, incrustation, liste des domaines autorisés
- `src/mpris.cpp` : lecteur MPRIS2 (intégration Plasma)
- `qml/Main.qml` : interface (vue web, barre du bas, menu)
- `data/inject.js` : script injecté dans YouTube (H.264, pubs, arrière-plan, MPRIS)
