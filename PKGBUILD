# PKGBUILD local : à lancer avec « makepkg -si » dans ce dossier.
pkgname=yt-webapp
pkgver=0.1.0
pkgrel=1
pkgdesc="YouTube en appli dédiée (QtWebEngine), pensée pour Plasma Mobile"
arch=('aarch64' 'x86_64')
license=('MIT')
depends=('qt6-base' 'qt6-declarative' 'qt6-webengine')
makedepends=('cmake')
optdepends=('mpv: bouton « mpv » et lecture externe'
            'yt-dlp: utilisé par mpv pour lire YouTube')

build() {
  cmake -B build -S "$startdir" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release
  cmake --build build
}

package() {
  DESTDIR="$pkgdir" cmake --install build
}
