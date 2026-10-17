#include "ImageJoin.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QString>

namespace {

// Une image de la corbeille, rendue en pixels. Un SVG est dessiné à sa taille
// naturelle : on ne recolle pas un dessin vectoriel sans le rendre d'abord.
QImage readImage(const DllImage &image) {
    QBuffer buffer;
    buffer.setData(image.bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, image.kind);
    return reader.read();
}

// De quoi arrêter un collage qui ne tiendrait pas : la planche entière d'un
// jeu à double résolution fait quatre millions de pixels, et il y a de la
// marge jusqu'ici.
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
            *why = tell("Le collage ferait une image de %1 × %2 pixels, qui ne sert à rien.")
                       .arg(room.width())
                       .arg(room.height());
        }
        return {};
    }

    // Celle qu'on colle passe devant ou derrière selon le côté : à gauche et
    // en haut, c'est elle qui commence et l'ancre qui est décalée.
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
    // En PNG : les quarts d'une planche sont des JPEG, et une planche recollée
    // n'a pas à repasser par une compression qui abîme ses traits.
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
