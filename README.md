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
- **Réglages Chromium par défaut** : 4 threads de peinture, moins de pauses du ramasse-miettes V8, services inutiles coupés.

## Installation sur le téléphone (Arch Linux ARM)

Outils de compilation :

    sudo pacman -S --needed base-devel cmake qt6-webengine qt6-declarative

Pour le bouton mpv (facultatif) :

    sudo pacman -S --needed mpv yt-dlp

Dans le dossier du projet, compiler et installer le paquet :

    makepkg -si

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
- `src/backend.cpp` : réglages, mpv, liste des domaines autorisés
- `qml/Main.qml` : interface (vue web, barre du bas, menu)
- `data/inject.js` : script injecté dans YouTube (H.264, pubs)
