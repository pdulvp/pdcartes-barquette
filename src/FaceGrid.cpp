#include "FaceGrid.h"

#include "CardSet.h"
#include "Palette.h"

#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace {

constexpr int kNothing = -10000;
constexpr int kAddBack = -9000;

constexpr qreal kGap = 5.0;
constexpr qreal kCaption = 13.0;

// Proportion d'une carte ordinaire, employée tant qu'aucun jeu n'est
// chargé.
constexpr qreal kDefaultAspect = 0.642;

QString suitMark(int suit) {
    switch (suit) {
    case 0: return QStringLiteral("♠");
    case 1: return QStringLiteral("♥");
    case 2: return QStringLiteral("♦");
    case 3: return QStringLiteral("♣");
    default: break;
    }
    return QString();
}

bool suitIsRed(int suit) { return suit == 1 || suit == 2; }

} // namespace

FaceGrid::FaceGrid(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(520, 300);
}

void FaceGrid::watch(CardSet *set) {
    m_set = set;
    relayout();
    update();
}

void FaceGrid::reread() {
    relayout();
    update();
}

void FaceGrid::setTarget(int target) {
    if (m_target == target) return;
    m_target = target;
    update();
}

int FaceGrid::nextEmptyFace(int from) const {
    if (!m_set) return from;
    for (int step = 1; step <= CardSet::kSlots; ++step) {
        const int slot = (qMax(0, from) + step) % CardSet::kSlots;
        if (!m_set->hasFace(slot)) return slot;
    }
    return from;
}

void FaceGrid::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    relayout();
}

void FaceGrid::relayout() {
    m_cells.clear();
    if (width() < 60 || height() < 60) return;

    const int backs = m_set ? m_set->backCount() : 0;
    // Une rangée par couleur, puis une dernière : le joker, les dos, et la
    // case « + ».
    const int lastRow = 1 + backs + 1;
    const int columns = std::max(13, lastRow);

    qreal aspect = m_set ? m_set->aspect() : 0.0;
    if (aspect <= 0.01) aspect = kDefaultAspect;

    const qreal room = width() - m_labelWidth - kGap * (columns + 1);
    qreal cellWidth = room / columns;
    qreal cellHeight = cellWidth / aspect;

    // Cinq rangées, chacune avec sa légende en dessous.
    const qreal rowHeight = cellHeight + kCaption + kGap;
    const qreal wanted = rowHeight * 5 + kGap;
    if (wanted > height()) {
        const qreal shrink = (height() - kGap - 5 * (kCaption + kGap)) / 5.0;
        cellHeight = std::max(24.0, shrink);
        cellWidth = cellHeight * aspect;
    }

    const qreal startX = m_labelWidth + kGap;
    qreal y = kGap;
    for (int suit = 0; suit < 4; ++suit) {
        for (int rank = 1; rank <= 13; ++rank) {
            Cell cell;
            cell.target = CardSet::slotFor(rank, suit);
            cell.box = QRectF(startX + (rank - 1) * (cellWidth + kGap), y, cellWidth, cellHeight);
            cell.caption = CardSet::slotShort(cell.target).left(
                CardSet::slotShort(cell.target).size() - 1);
            m_cells.append(cell);
        }
        y += cellHeight + kCaption + kGap;
    }

    qreal x = startX;
    Cell joker;
    joker.target = CardSet::kJokerSlot;
    joker.box = QRectF(x, y, cellWidth, cellHeight);
    joker.caption = tr("joker");
    m_cells.append(joker);
    x += cellWidth + kGap * 3;

    for (int at = 0; at < backs; ++at) {
        Cell back;
        back.target = backTarget(at);
        back.box = QRectF(x, y, cellWidth, cellHeight);
        back.caption = tr("dos %1").arg(at + 1);
        m_cells.append(back);
        x += cellWidth + kGap;
    }

    Cell add;
    add.target = kAddBack;
    add.box = QRectF(x, y, cellWidth, cellHeight);
    add.caption = tr("ajouter");
    m_cells.append(add);

    setMinimumHeight(int(y + cellHeight + kCaption + kGap * 2));
}

int FaceGrid::cellAt(const QPointF &where) const {
    for (const Cell &cell : m_cells) {
        if (cell.box.adjusted(-1, -1, 1, 1).contains(where)) return cell.target;
    }
    return kNothing;
}

void FaceGrid::mousePressEvent(QMouseEvent *event) {
    const int at = cellAt(event->position());
    if (at == kNothing) return;

    if (event->button() == Qt::RightButton) {
        if (at != kAddBack) Q_EMIT clearRequested(at);
        return;
    }
    if (at == kAddBack) {
        Q_EMIT addBackRequested();
        return;
    }
    setTarget(at);
    Q_EMIT targetChosen(at);
}

void FaceGrid::mouseMoveEvent(QMouseEvent *event) {
    const int at = cellAt(event->position());
    if (at == m_hovered) return;
    m_hovered = at;
    setCursor(at == kNothing ? Qt::ArrowCursor : Qt::PointingHandCursor);
    update();
}

void FaceGrid::leaveEvent(QEvent *) {
    m_hovered = kNothing;
    update();
}

void FaceGrid::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        Q_EMIT clearRequested(m_target);
        return;
    case Qt::Key_Right:
        if (m_target >= 0 && m_target < CardSet::kSlots - 1) {
            setTarget(m_target + 1);
            Q_EMIT targetChosen(m_target);
        }
        return;
    case Qt::Key_Left:
        if (m_target > 0) {
            setTarget(m_target - 1);
            Q_EMIT targetChosen(m_target);
        }
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void FaceGrid::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    painter.fillRect(rect(), scheme.desk);

    QFont caption = font();
    caption.setPixelSize(10);
    QFont mark = font();
    mark.setBold(true);

    for (const Cell &cell : m_cells) {
        const bool chosen = cell.target == m_target;
        const bool lit = cell.target == m_hovered;
        const bool adder = cell.target == kAddBack;

        QPainterPath shape;
        shape.addRoundedRect(cell.box, 5, 5);

        // La case elle-même : un fond uni quand elle est vide, l'image
        // lorsqu'elle en contient une.
        QPixmap art;
        if (m_set && !adder) {
            const QSize want = cell.box.size().toSize();
            art = isBack(cell.target) ? m_set->backImage(backOfTarget(cell.target), want)
                                      : m_set->faceImage(cell.target, want);
        }

        if (!art.isNull()) {
            painter.setPen(Qt::NoPen);
            painter.fillPath(shape, scheme.cardFace);
            painter.save();
            painter.setClipPath(shape);
            QRectF where(0, 0, art.width(), art.height());
            where.moveCenter(cell.box.center());
            painter.drawPixmap(where.topLeft(), art);
            painter.restore();
        } else {
            painter.setPen(Qt::NoPen);
            painter.fillPath(shape, adder ? scheme.desk
                                          : blendColours(scheme.surface, scheme.desk, 0.45));
            if (adder) {
                // La case qui ajoute un dos.
                painter.setPen(QPen(scheme.hint, 2.0, Qt::DashLine));
                painter.setBrush(Qt::NoBrush);
                painter.drawPath(shape);
                painter.setPen(QPen(scheme.accent, 2.2, Qt::SolidLine, Qt::RoundCap));
                const QPointF middle = cell.box.center();
                const qreal arm = std::min(cell.box.width(), cell.box.height()) * 0.20;
                painter.drawLine(middle - QPointF(arm, 0), middle + QPointF(arm, 0));
                painter.drawLine(middle - QPointF(0, arm), middle + QPointF(0, arm));
            } else {
                // Une case vide affiche le rang et la couleur qu'elle
                // attend.
                const int slot = isBack(cell.target) ? -1 : cell.target;
                mark.setPixelSize(std::max(9, int(cell.box.height() * 0.30)));
                painter.setFont(mark);
                painter.setPen(blendColours(scheme.surface, scheme.surfaceText, 0.35));
                QString inside = tr("dos");
                if (slot == CardSet::kJokerSlot) {
                    inside = QStringLiteral("★");
                } else if (slot > 0) {
                    inside = CardSet::slotShort(slot);
                }
                painter.drawText(cell.box, Qt::AlignCenter, inside);
            }
        }

        painter.setBrush(Qt::NoBrush);
        if (chosen) {
            painter.setPen(QPen(scheme.accent, 2.4));
        } else if (lit) {
            painter.setPen(QPen(blendColours(scheme.accent, scheme.edge, 0.5), 1.8));
        } else {
            painter.setPen(QPen(scheme.edge, 1.0));
        }
        painter.drawPath(shape);

        painter.setFont(caption);
        painter.setPen(chosen ? scheme.accent : scheme.hint);
        painter.drawText(QRectF(cell.box.left(), cell.box.bottom() + 1.0, cell.box.width(),
                                kCaption),
                         Qt::AlignCenter, cell.caption);
    }

    // Les couleurs, dans la marge de gauche.
    for (int suit = 0; suit < 4; ++suit) {
        const int slot = CardSet::slotFor(1, suit);
        const Cell *first = nullptr;
        for (const Cell &cell : m_cells) {
            if (cell.target == slot) first = &cell;
        }
        if (!first) continue;
        QFont pip = font();
        pip.setBold(true);
        pip.setPixelSize(std::max(11, int(first->box.height() * 0.34)));
        painter.setFont(pip);
        painter.setPen(suitIsRed(suit) ? scheme.cardRed : scheme.surfaceText);
        painter.drawText(QRectF(0, first->box.top(), m_labelWidth, first->box.height()),
                         Qt::AlignCenter, suitMark(suit));
    }
}
