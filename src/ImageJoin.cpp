#include "ImageJoin.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QString>

namespace {

// Une image de la liste, rendue en pixels. Un SVG est dessiné à sa taille
// naturelle : un dessin vectoriel doit être rendu avant d'être recollé.
QImage readImage(const DllImage &image) {
    QBuffer buffer;
    buffer.setData(image.bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, image.kind);
    return reader.read();
}

// Limite destinée à écarter un collage déraisonnable. La planche entière d'un
// jeu en double résolution fait quatre millions de pixels : la marge est donc
// confortable.
constexpr qreal kMostPixels = 80e6;

QString tell(const char *text) { return QCoreApplication::translate("imagejoin", text); }

} // namespace

DllImage joinImages(const DllImage &anchor, const DllImage &other, JoinSide side, QString *why) {
    const QImage first = readImage(anchor);
    const QImage second = readImage(other);
    if (first.isNull() || second.isNull()) {
        if (why) *why = tell("Une des deux images n'a pas pu être lue.");
        return {};
    }

    const bool sideways = side == JoinSide::Left || side == JoinSide::Right;
    const QSize room =
        sideways ? QSize(first.width() + second.width(), qMax(first.height(), second.height()))
                 : QSize(qMax(first.width(), second.width()), first.height() + second.height());
    if (qreal(room.width()) * room.height() > kMostPixels) {
        if (why) {
            *why = tell("Le collage produirait une image de %1 × %2 pixels, ce qui est "
                        "trop grand.")
                       .arg(room.width())
                       .arg(room.height());
        }
        return {};
    }

    // L'image collée se place avant ou après selon le côté demandé : à
    // gauche et au-dessus, c'est elle qui commence, et c'est l'ancre qui est
    // décalée.
    QPoint anchorAt(0, 0);
    QPoint otherAt(0, 0);
    switch (side) {
    case JoinSide::Right: otherAt = QPoint(first.width(), 0); break;
    case JoinSide::Left: anchorAt = QPoint(second.width(), 0); break;
    case JoinSide::Below: otherAt = QPoint(0, first.height()); break;
    case JoinSide::Above: anchorAt = QPoint(0, second.height()); break;
    }

    QImage made(room, QImage::Format_ARGB32);
    made.fill(Qt::transparent);
    {
        QPainter painter(&made);
        painter.drawImage(anchorAt, first);
        painter.drawImage(otherAt, second);
    }

    QByteArray bytes;
    QBuffer out(&bytes);
    out.open(QIODevice::WriteOnly);
    // En PNG : les quarts d'une planche sont des JPEG, et une planche
    // recollée n'a pas à subir une seconde compression avec perte, qui
    // dégraderait ses traits.
    if (!made.save(&out, "PNG")) {
        if (why) *why = tell("Le collage n'a pas pu être écrit.");
        return {};
    }

    DllImage joined;
    joined.label = QStringLiteral("%1 + %2").arg(imageStem(anchor.label), imageStem(other.label));
    joined.kind = "png";
    joined.bytes = bytes;
    joined.width = made.width();
    joined.height = made.height();
    return joined;
}
