#include "DrawnDeck.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace {

// Bornes de la hauteur de rendu. En dessous, la carte n'est plus lisible ; au
// delà, le fichier grossit sans profit, cinquante-trois images y figurant.
constexpr int kLowest = 64;
constexpr int kHighest = 1200;

QByteArray renderOne(int rank, int suit, bool back, const DrawnRecipe &how) {
    const QSize size = drawnCardSize(how.height);
    QImage image(size, QImage::Format_ARGB32);
    if (image.isNull()) return {};
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Un demi-pixel de marge : le filet du bord est tracé au centre du trait,
    // et sans cette marge sa moitié extérieure tomberait hors de l'image.
    const QRectF box(0.5, 0.5, image.width() - 1.0, image.height() - 1.0);
    if (back) {
        drawDefaultBack(&painter, box, how.style, how.panelArt);
    } else {
        drawDefaultFace(&painter, box, rank, suit, how.style, how.panelArt);
    }
    painter.end();

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) return {};
    return bytes;
}

} // namespace

QSize drawnCardSize(int height) {
    const int tall = std::clamp(height, kLowest, kHighest);
    return QSize(std::max(1, int(std::round(tall * kCardRatio))), tall);
}

Artwork drawnFaceArt(int slot, const DrawnRecipe &how, DeckKind kind) {
    if (slot < 0 || slot >= CardSet::kSlots) return {};
    // La case du joker est l'excuse dans un tarot. Le dessin le reconnaît à
    // la couleur des atouts posée sur un rang nul.
    const int suit = (slot == CardSet::kJokerSlot && kind == DeckKind::Tarot)
                         ? CardSet::kTrumpSuit
                         : CardSet::suitOfSlot(slot);
    Artwork art;
    art.kind = "png";
    art.bytes = renderOne(CardSet::rankOfSlot(slot), suit, false, how);
    if (art.bytes.isEmpty()) return {};
    return art;
}

Artwork drawnBackArt(const DrawnRecipe &how) {
    Artwork art;
    art.kind = "png";
    art.bytes = renderOne(0, 0, true, how);
    if (art.bytes.isEmpty()) return {};
    return art;
}

CardSet drawnCardSet(const DrawnRecipe &how, DeckKind kind) {
    CardSet set;
    set.kind = kind;
    set.name = QCoreApplication::translate("drawn", "Le jeu dessiné");
    set.note = QCoreApplication::translate(
        "drawn", "Les cartes que la cartothèque dessine elle-même, rendues en images.");

    for (const int slot : CardSet::slotsOf(kind)) {
        set.setDrawnFace(slot, drawnFaceArt(slot, how, kind), how);
    }
    set.addDrawnBack(QCoreApplication::translate("drawn", "Treillis"), drawnBackArt(how), how);
    return set;
}
