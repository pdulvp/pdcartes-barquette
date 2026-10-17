#include "Palette.h"

#include <QPainter>
#include <QPixmap>

#include <algorithm>

namespace {

// A colour from the table's hue at a given lightness and saturation. HSL
// rather than HSV, because what a pastel is is a colour pushed up towards
// white with its hue left alone, and lightness is the axis that does that.
QColor tint(int hue, qreal lightness, qreal saturation) {
    return QColor::fromHslF(qBound(0, hue, 359) / 360.0, qBound(0.0, saturation, 1.0),
                            qBound(0.0, lightness, 1.0));
}

// What is worn at the moment. The first table's hue until a game says
// otherwise, so a palette asked for before anything is chosen is still a
// palette rather than black on black.
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

    // Each size the icon offers, painted through: the shape is kept and the
    // colour thrown away, which is what tinting a one-colour icon means.
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

    // The cloth is the one thing that is dark in both halves of the scheme. A
    // card table is green baize with white cards on it, and a pale table with
    // pale cards on it is not a card table -- it is a spreadsheet. So the felt
    // stays deep and saturated whichever way the dark button is set, and what
    // changes around it is the wood, the plaques and the lettering.
    p.felt = tint(hue, dark ? 0.165 : 0.248, dark ? 0.40 : 0.44);
    p.feltEdge = tint(hue, dark ? 0.085 : 0.145, dark ? 0.44 : 0.46);
    // A thread a shade lighter than the cloth, which is what a weave looks
    // like from across a room: not a line, a slightly brighter cloth.
    p.feltWeave = tint(hue, dark ? 0.205 : 0.295, dark ? 0.36 : 0.40);
    p.feltText = tint(hue, dark ? 0.70 : 0.84, dark ? 0.26 : 0.34);
    // Braid: the warm line around the cloth. Walked towards gold rather than
    // taken from the hue, because that is what braid is.
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
        // A shade off white after dark: pure white paper on a dark table is a
        // row of little lamps, and fifteen of them along the bottom of the
        // window is a glare rather than a hand.
        p.cardFace = QColor(0xe9, 0xe6, 0xdd);
        p.cardInk = QColor(0x22, 0x24, 0x29);
        p.cardRed = QColor(0xb8, 0x2b, 0x3c);
    }

    // The back of a card is the table's colour, deeper than the cloth so that
    // a face-down hand reads as cards on felt rather than as a hole in it.
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
    // How far round the wheel each shade sits from the table's own hue, and how
    // much lighter or darker it is drawn. Two sequences of different length, so
    // that neighbouring sparks differ in both and a run of thirty never repeats
    // itself.
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
    // Bright and a little pale: these are read against the cloth, which is
    // dark in both halves of the scheme, so a rival's colour is lifted well
    // clear of it rather than taken from the same band.
    return tint(hue, 0.66, 0.58);
}
