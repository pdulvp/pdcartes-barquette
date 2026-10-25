#include "SourceTray.h"

#include "Palette.h"

#include <QBuffer>
#include <QFontMetricsF>
#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace {

constexpr qreal kThumbHeight = 76.0;
constexpr qreal kCaption = 13.0;
constexpr qreal kGap = 7.0;
constexpr qreal kPad = 4.0;

} // namespace

SourceTray::SourceTray(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setMinimumHeight(120);
}

void SourceTray::setItems(const QVector<DllImage> &images) {
    m_items.clear();
    m_chosen = -1;
    addItems(images);
}

void SourceTray::addItems(const QVector<DllImage> &images) {
    for (const DllImage &image : images) {
        Item item;
        item.image = image;
        makeThumb(&item);
        if (item.thumb.isNull()) continue;
        m_items.append(item);
    }
    relayout();
    update();
}

bool SourceTray::replaceItem(int index, const QVector<DllImage> &images) {
    if (index < 0 || index >= m_items.size()) return false;

    QVector<Item> made;
    for (const DllImage &image : images) {
        Item item;
        item.image = image;
        makeThumb(&item);
        if (item.thumb.isNull()) continue;
        made.append(item);
    }
    if (made.isEmpty()) return false;

    m_items.remove(index);
    for (int i = 0; i < made.size(); ++i) m_items.insert(index + i, made.at(i));
    m_chosen = index;
    m_hovered = -1;
    relayout();
    update();
    Q_EMIT picked(m_chosen);
    return true;
}

void SourceTray::removeItem(int index) {
    if (index < 0 || index >= m_items.size()) return;
    m_items.remove(index);
    // La sélection suit l'image qu'elle désignait : le retrait d'une image
    // placée avant elle la décale d'un rang.
    if (m_chosen == index) m_chosen = -1;
    else if (m_chosen > index) --m_chosen;
    m_hovered = -1;
    relayout();
    update();
}

void SourceTray::clearItems() {
    m_items.clear();
    m_chosen = -1;
    relayout();
    update();
}

const SourceTray::Item *SourceTray::at(int index) const {
    if (index < 0 || index >= m_items.size()) return nullptr;
    return &m_items.at(index);
}

void SourceTray::markUsed(int index, bool used) {
    if (index < 0 || index >= m_items.size()) return;
    m_items[index].used = used;
    update();
}

void SourceTray::forgetUsage() {
    for (Item &item : m_items) item.used = false;
    update();
}

void SourceTray::makeThumb(Item *item) {
    QBuffer buffer;
    buffer.setData(item->image.bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, item->image.kind);

    const bool vector = item->image.kind == "svg" || item->image.kind == "svgz";
    QImage image;
    if (vector) {
        // Rendu directement à la taille de la vignette.
        const QSize natural = reader.size();
        const qreal aspect = natural.isValid() && natural.height() > 0
                                 ? qreal(natural.width()) / natural.height()
                                 : 0.7;
        reader.setScaledSize(QSize(std::max(8, int(kThumbHeight * aspect)), int(kThumbHeight)));
        image = reader.read();
    } else {
        image = reader.read();
        if (!image.isNull()) {
            image = image.scaled(QSize(int(kThumbHeight * 3), int(kThumbHeight)),
                                 Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    if (image.isNull()) return;
    item->thumb = QPixmap::fromImage(image);
}

void SourceTray::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    relayout();
}

void SourceTray::relayout() {
    m_boxes.clear();
    if (width() < 40) return;

    const qreal rowHeight = kThumbHeight + kCaption + kPad * 2;
    qreal x = kGap;
    qreal y = kGap;
    qreal tallest = rowHeight;

    for (const Item &item : m_items) {
        const qreal cellWidth = item.thumb.width() + kPad * 2;
        if (x > kGap && x + cellWidth > width() - kGap) {
            x = kGap;
            y += tallest + kGap;
        }
        m_boxes.append(QRectF(x, y, cellWidth, rowHeight));
        x += cellWidth + kGap;
    }

    // C'est la hauteur minimale demandée ici qui fait défiler la zone
    // contenant ce widget.
    setMinimumHeight(int(y + tallest + kGap));
}

int SourceTray::cellAt(const QPointF &where) const {
    for (int i = 0; i < m_boxes.size(); ++i) {
        if (m_boxes.at(i).contains(where)) return i;
    }
    return -1;
}

void SourceTray::mousePressEvent(QMouseEvent *event) {
    const int at = cellAt(event->position());
    if (at < 0) return;

    // Le clic droit ne change pas la sélection : le menu proposé dépend à la
    // fois de l'image sélectionnée et de celle qui est sous le pointeur, et les
    // confondre ne laisserait rien à recoller.
    if (event->button() == Qt::RightButton) {
        Q_EMIT menuAsked(at, event->globalPosition().toPoint());
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    m_chosen = at;
    update();
    Q_EMIT picked(at);
}

void SourceTray::mouseDoubleClickEvent(QMouseEvent *event) {
    const int at = cellAt(event->position());
    if (at < 0) return;
    m_chosen = at;
    update();
    Q_EMIT pickedTwice(at);
}

void SourceTray::mouseMoveEvent(QMouseEvent *event) {
    const int at = cellAt(event->position());
    if (at == m_hovered) return;
    m_hovered = at;
    setCursor(at < 0 ? Qt::ArrowCursor : Qt::PointingHandCursor);
    update();
}

void SourceTray::leaveEvent(QEvent *) {
    m_hovered = -1;
    update();
}

void SourceTray::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(rect(), scheme.desk);

    if (m_items.isEmpty()) {
        QFont empty = font();
        empty.setPixelSize(12);
        painter.setFont(empty);
        painter.setPen(scheme.hint);
        painter.drawText(rect().adjusted(18, 0, -18, 0), Qt::AlignCenter | Qt::TextWordWrap,
                         tr("Aucune image ouverte.\nOuvrez des images, un dossier, une "
                            "DLL, ou les cartes d'un Autre set."));
        return;
    }

    QFont caption = font();
    caption.setPixelSize(9);
    const QFontMetricsF metrics(caption);

    for (int i = 0; i < m_items.size() && i < m_boxes.size(); ++i) {
        const Item &item = m_items.at(i);
        const QRectF box = m_boxes.at(i);
        const bool chosen = i == m_chosen;
        const bool lit = i == m_hovered;

        QPainterPath shape;
        shape.addRoundedRect(box, 5, 5);
        painter.setPen(Qt::NoPen);
        painter.fillPath(shape, chosen ? blendColours(scheme.surface, scheme.accent, 0.18)
                                       : (lit ? scheme.surface
                                              : blendColours(scheme.surface, scheme.desk, 0.4)));

        // Un damier sous la vignette, pour rendre la transparence
        // visible.
        QRectF where(0, 0, item.thumb.width(), item.thumb.height());
        where.moveCenter(QPointF(box.center().x(), box.top() + kPad + kThumbHeight / 2.0));
        painter.save();
        painter.setClipRect(where);
        const int square = 7;
        for (int cy = 0; cy * square < where.height() + square; ++cy) {
            for (int cx = 0; cx * square < where.width() + square; ++cx) {
                const bool dark = (cx + cy) % 2 == 0;
                painter.fillRect(QRectF(where.left() + cx * square, where.top() + cy * square,
                                        square, square),
                                 dark ? blendColours(scheme.surface, scheme.desk, 0.25)
                                      : scheme.surface);
            }
        }
        painter.restore();
        painter.drawPixmap(where.topLeft(), item.thumb);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(chosen ? scheme.accent : (lit ? scheme.hint : scheme.edge),
                            chosen ? 2.2 : 1.0));
        painter.drawPath(shape);

        // Les images déjà placées portent une pastille.
        if (item.used) {
            QRectF tick(0, 0, 15, 15);
            tick.moveTopRight(QPointF(box.right() - 3, box.top() + 3));
            painter.setPen(Qt::NoPen);
            painter.setBrush(scheme.accent);
            painter.drawEllipse(tick);
            painter.setPen(QPen(scheme.accentText, 1.8, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(tick.center() + QPointF(-3.4, 0.2), tick.center() + QPointF(-0.9, 2.8));
            painter.drawLine(tick.center() + QPointF(-0.9, 2.8), tick.center() + QPointF(3.6, -2.8));
        }

        painter.setFont(caption);
        painter.setPen(chosen ? scheme.accent : scheme.hint);
        const QString label =
            metrics.elidedText(item.image.label, Qt::ElideMiddle, box.width() - 6);
        painter.drawText(QRectF(box.left() + 3, box.bottom() - kCaption - 2, box.width() - 6,
                                kCaption),
                         Qt::AlignCenter, label);
    }
}
