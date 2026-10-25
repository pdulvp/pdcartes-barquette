#include "Palette.h"

#include <QPainter>
#include <QPixmap>

#include <algorithm>

namespace {

// Une couleur obtenue à partir de la teinte du thème, pour une clarté et une
// saturation données. HSL plutôt que HSV : une couleur pastel est une couleur
// rapprochée du blanc à teinte constante, et c'est la clarté qui fait cela.
QColor tint(int hue, qreal lightness, qreal saturation) {
    return QColor::fromHslF(qBound(0, hue, 359) / 360.0, qBound(0.0, saturation, 1.0),
                            qBound(0.0, lightness, 1.0));
}

// La palette en cours. Une teinte par défaut est posée ici, de sorte qu'une
// palette demandée avant tout choix soit une vraie palette, et non du noir sur
// du noir.
int g_hue = 150;
bool g_dark = false;
Palette g_worn;
bool g_wornBuilt = false;

void rebuildWorn() {
    g_worn = paletteForHue(g_hue, g_dark);
    g_wornBuilt = true;
}

} // namespace

QColor readableTextOn(const QColor &background) {
    return (background.isValid() && background.lightnessF() > 0.58) ? QColor(0x1a, 0x1a, 0x1f)
                                                                    : QColor(Qt::white);
}

QColor blendColours(const QColor &under, const QColor &over, qreal weight) {
    const qreal keep = 1.0 - qBound(0.0, weight, 1.0);
    const qreal mix = qBound(0.0, weight, 1.0);
    return QColor(qRound(under.red() * keep + over.red() * mix),
                  qRound(under.green() * keep + over.green() * mix),
                  qRound(under.blue() * keep + over.blue() * mix));
}

QIcon tintedIcon(const QStringList &names, const QColor &ink) {
    QIcon source;
    for (const QString &name : names) {
        source = QIcon::fromTheme(name);
        if (!source.isNull()) break;
    }
    if (source.isNull()) return {};

    // Chaque taille proposée par l'icône est repeinte : la forme est
    // conservée et la couleur remplacée, ce qui est précisément ce que
    // signifie recolorer une icône monochrome.
    QIcon tinted;
    for (int size : {16, 22, 24, 32, 48}) {
        QPixmap artwork = source.pixmap(size, size);
        if (artwork.isNull()) continue;

        QPixmap coloured(artwork.size());
        coloured.setDevicePixelRatio(artwork.devicePixelRatio());
        coloured.fill(Qt::transparent);

        QPainter painter(&coloured);
        painter.drawPixmap(0, 0, artwork);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(coloured.rect(), ink);
        painter.end();

        tinted.addPixmap(coloured);
    }
    return tinted;
}

Palette paletteForHue(int hue, bool dark) {
    Palette p;
    p.hue = qBound(0, hue, 359);

    // Le tapis est la seule chose qui reste sombre dans les deux modes. Une
    // table de jeu est un tapis foncé avec des cartes claires dessus ; un
    // tapis clair sous des cartes claires ne se lit plus comme une table de
    // jeu. Le tapis reste donc foncé et saturé quel que soit le mode, et ce
    // qui change autour de lui, c'est le fond, les panneaux et le texte.
    p.felt = tint(hue, dark ? 0.165 : 0.248, dark ? 0.40 : 0.44);
    p.feltEdge = tint(hue, dark ? 0.085 : 0.145, dark ? 0.44 : 0.46);
    // Le fil du tissage, à peine plus clair que le tapis : vu de loin, un
    // tissage n'est pas un trait, mais un tapis un peu plus clair.
    p.feltWeave = tint(hue, dark ? 0.205 : 0.295, dark ? 0.36 : 0.40);
    p.feltText = tint(hue, dark ? 0.70 : 0.84, dark ? 0.26 : 0.34);
    // Le galon : la ligne chaude autour du tapis. Rapproché de l'or plutôt
    // que dérivé de la teinte, car c'est ainsi qu'un galon se voit.
    p.trim = blendColours(tint(hue, 0.52, 0.30), QColor(0xd9, 0xb3, 0x64), 0.62);
    p.gold = QColor(0xe2, 0xbc, 0x6b);

    if (!dark) {
        p.desk = tint(hue, 0.905, 0.42);
        p.surface = tint(hue, 0.972, 0.52);
        p.surfaceText = tint(hue, 0.19, 0.40);
        p.face = tint(hue, 0.888, 0.36);
        p.faceText = tint(hue, 0.19, 0.40);
        p.accent = tint(hue, 0.38, 0.52);
        p.highlight = tint(hue, 0.70, 0.62);
        p.edge = tint(hue, 0.775, 0.32);
        p.hint = tint(hue, 0.48, 0.22);
        p.danger = QColor::fromHslF(0.015, 0.68, 0.48);
        p.field = tint(hue, 0.965, 0.44);
        p.cardFace = QColor(0xfb, 0xfa, 0xf6);
        p.cardInk = QColor(0x24, 0x26, 0x2c);
        p.cardRed = QColor(0xc3, 0x29, 0x3c);
    } else {
        p.desk = tint(hue, 0.085, 0.30);
        p.surface = tint(hue, 0.145, 0.26);
        p.surfaceText = tint(hue, 0.90, 0.20);
        p.face = tint(hue, 0.175, 0.24);
        p.faceText = tint(hue, 0.90, 0.18);
        p.accent = tint(hue, 0.62, 0.52);
        p.highlight = tint(hue, 0.48, 0.48);
        p.edge = tint(hue, 0.29, 0.22);
        p.hint = tint(hue, 0.58, 0.17);
        p.danger = QColor::fromHslF(0.015, 0.70, 0.58);
        p.field = tint(hue, 0.165, 0.22);
        // Un blanc légèrement cassé en mode sombre : un papier blanc pur sur
        // un fond foncé éblouit, et quinze cartes alignées en bas de la
        // fenêtre feraient autant de petites lampes.
        p.cardFace = QColor(0xe9, 0xe6, 0xdd);
        p.cardInk = QColor(0x22, 0x24, 0x29);
        p.cardRed = QColor(0xb8, 0x2b, 0x3c);
    }

    // Le dos d'une carte reprend la teinte du thème, plus foncée que le
    // tapis, afin qu'une main face cachée se lise comme des cartes posées sur
    // le tapis et non comme un trou dedans.
    p.cardBack = tint(hue, dark ? 0.34 : 0.40, 0.42);
    p.cardBackInk = tint(hue, dark ? 0.52 : 0.58, 0.44);

    p.accentText = readableTextOn(p.accent);
    p.highlightText = readableTextOn(p.highlight);
    return p;
}

const Palette &currentPalette() {
    if (!g_wornBuilt) rebuildWorn();
    return g_worn;
}

void setCurrentPalette(int hue) {
    g_hue = qBound(0, hue, 359);
    rebuildWorn();
}

bool darkMode() { return g_dark; }

void setDarkMode(bool dark) {
    if (dark == g_dark && g_wornBuilt) return;
    g_dark = dark;
    rebuildWorn();
}

QColor festiveColour(int index) {
    // L'écart de chaque nuance par rapport à la teinte du thème sur le cercle
    // des couleurs, et son écart de clarté. Deux suites de longueurs
    // différentes, de sorte que deux étincelles voisines diffèrent sur les
    // deux axes et qu'une série de trente ne se répète jamais.
    static const int kAside[] = {0, 17, -15, 29, -26, 8, -8, 34, -31, 23, -20, 5, 12, -12};
    static const qreal kStep[] = {0.0, -0.09, 0.08, -0.05, 0.12, 0.04, -0.13, 0.15};

    const Palette &worn = currentPalette();
    const int at = index < 0 ? 0 : index;
    const int aside = kAside[at % int(std::size(kAside))];
    const qreal step = kStep[at % int(std::size(kStep))];

    const int hue = ((worn.hue + aside) % 360 + 360) % 360;
    return tint(hue, 0.60 + step, 0.62);
}

QColor rivalColour(int hue) {
    // Vive et un peu claire : ces couleurs se lisent sur le tapis, foncé dans
    // les deux modes. La couleur d'un adversaire est donc nettement détachée
    // de lui, au lieu d'être prise dans la même plage de clarté.
    return tint(hue, 0.66, 0.58);
}
