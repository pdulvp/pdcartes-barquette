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
    // Le parcours suit l'ordre d'affichage du genre : une case qu'il
    // n'emploie pas n'est pas une case à remplir.
    const QVector<int> wanted = CardSet::slotsOf(m_set->kind);
    if (wanted.isEmpty()) return from;
    int start = wanted.indexOf(from);
    if (start < 0) start = 0;
    for (int step = 1; step <= wanted.size(); ++step) {
        const int slot = wanted.at((start + step) % wanted.size());
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

    const DeckKind kind = m_set ? m_set->kind : DeckKind::Full;
    const int backs = m_set ? m_set->backCount() : 0;

    // Les rangs que le genre emploie dans une couleur, dans l'ordre
    // d'affichage : c'est la largeur des quatre premières rangées.
    QVector<int> ranks;
    for (int at = 1; at <= CardSet::kRanksInSuit; ++at) {
        const int slot = CardSet::kJokerSlot + at;  // la première couleur
        if (CardSet::kindUses(kind, slot)) ranks.append(CardSet::rankOfSlot(slot));
    }

    // Les atouts, s'il y en a, en rangées de la largeur des couleurs.
    const int trumps = kind == DeckKind::Tarot ? CardSet::kTrumpCount : 0;
    const int perRow = std::max(1, int(ranks.size()));
    const int trumpRows = (trumps + perRow - 1) / perRow;

    // La dernière rangée : le joker ou l'excuse, les dos, et la case « + ».
    const int lastRow = 1 + backs + 1;
    const int columns = std::max(perRow, lastRow);
    const int rows = 4 + trumpRows + 1;

    qreal aspect = m_set ? m_set->aspect() : 0.0;
    if (aspect <= 0.01) aspect = kDefaultAspect;

    const qreal room = width() - m_labelWidth - kGap * (columns + 1);
    qreal cellWidth = room / columns;
    qreal cellHeight = cellWidth / aspect;

    // La hauteur dont on dispose réellement.
    //
    // Ce n'est pas `height()` : la grille vit dans une zone défilante qui la
    // redimensionne, et c'est la hauteur minimale fixée en fin de fonction
    // qui décide alors de la sienne. Comparer la place voulue à sa propre
    // hauteur revenait donc à la comparer à elle-même : la grille ne se
    // resserrait jamais, et dès que la fenêtre manquait d'une vingtaine de
    // pixels, la rangée du joker et des dos passait sous le bord avec un
    // ascenseur pour aller la chercher. C'est la fenêtre d'affichage -- le
    // parent, dans une zone défilante -- qui dit la place disponible.
    const qreal visible = parentWidget() ? qreal(parentWidget()->height()) : qreal(height());

    // Autant de rangées que le genre en demande, chacune avec sa légende en
    // dessous.
    const qreal rowHeight = cellHeight + kCaption + kGap;
    const qreal wanted = rowHeight * rows + kGap;
    if (wanted > visible) {
        // Deux interlignes, et non un : c'est la hauteur minimale fixée en fin
        // de fonction qu'il faut ramener à la place disponible, et elle
        // compte un interligne de plus que `wanted` -- celui du bas. Un seul
        // laissait la grille dépasser de cinq pixels, et l'ascenseur
        // apparaissait pour un contenu qui tenait.
        const qreal shrink = (visible - 2.0 * kGap - 1.0 - rows * (kCaption + kGap)) / rows;
        cellHeight = std::max(24.0, shrink);
        cellWidth = cellHeight * aspect;
    }

    const qreal startX = m_labelWidth + kGap;
    qreal y = kGap;
    for (int suit = 0; suit < 4; ++suit) {
        for (int at = 0; at < ranks.size(); ++at) {
            Cell cell;
            cell.target = CardSet::slotFor(ranks.at(at), suit);
            cell.box = QRectF(startX + at * (cellWidth + kGap), y, cellWidth, cellHeight);
            const QString mark = CardSet::slotShort(cell.target);
            cell.caption = mark.left(mark.size() - 1);
            m_cells.append(cell);
        }
        y += cellHeight + kCaption + kGap;
    }

    // Les atouts, numérotés de un à vingt et un.
    for (int number = 1; number <= trumps; ++number) {
        const int at = (number - 1) % perRow;
        Cell cell;
        cell.target = CardSet::slotForTrump(number);
        cell.box = QRectF(startX + at * (cellWidth + kGap), y, cellWidth, cellHeight);
        cell.caption = CardSet::slotShort(cell.target);
        m_cells.append(cell);
        if (at == perRow - 1 || number == trumps) y += cellHeight + kCaption + kGap;
    }

    qreal x = startX;
    Cell joker;
    joker.target = CardSet::kJokerSlot;
    joker.box = QRectF(x, y, cellWidth, cellHeight);
    joker.caption = kind == DeckKind::Tarot ? tr("excuse") : tr("joker");
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
    case Qt::Key_Left: {
        // Les flèches suivent l'ordre d'affichage du genre, et non les
        // numéros de case : le cavalier vient entre le valet et la dame.
        if (!m_set) return;
        const QVector<int> wanted = CardSet::slotsOf(m_set->kind);
        const int at = wanted.indexOf(m_target);
        if (at < 0) return;
        const int step = event->key() == Qt::Key_Right ? 1 : -1;
        const int next = at + step;
        if (next < 0 || next >= wanted.size()) return;
        setTarget(wanted.at(next));
        Q_EMIT targetChosen(m_target);
        return;
    }
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
                if (slot > 0 && CardSet::isTrumpSlot(slot)) {
                    // Un atout n'a pas d'enseigne : son chiffre romain seul
                    // remplirait mal la case, le numéro l'accompagne.
                    inside = QStringLiteral("%1\n%2")
                                 .arg(CardSet::trumpOfSlot(slot))
                                 .arg(CardSet::slotShort(slot));
                }
                painter.drawText(cell.box, Qt::AlignCenter | Qt::TextWordWrap, inside);
            }
        }

        // Une case dessinée porte une marque : c'est à cela qu'on voit
        // laquelle se retouche, une image venue de l'extérieur n'ayant pas de
        // couleurs à régler.
        if (m_set && !adder && m_set->hasRecipe(cell.target)) {
            // Au coin supérieur droit : le rang et la couleur d'une carte
            // s'écrivent dans le coin opposé, que la marque masquerait.
            const qreal dot = std::max(5.0, cell.box.width() * 0.17);
            painter.setPen(QPen(scheme.desk, 1.0));
            painter.setBrush(scheme.accent);
            painter.drawEllipse(QRectF(cell.box.right() - dot - 2.5, cell.box.top() + 2.5,
                                       dot, dot));
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
    int firstRank = 1;
    for (int at = 1; at <= CardSet::kRanksInSuit; ++at) {
        if (CardSet::kindUses(m_set ? m_set->kind : DeckKind::Full, CardSet::kJokerSlot + at)) {
            firstRank = CardSet::rankOfSlot(CardSet::kJokerSlot + at);
            break;
        }
    }
    for (int suit = 0; suit < 4; ++suit) {
        // La première case de la rangée, quel que soit le rang par lequel le
        // genre la commence : un jeu de 32 commence au sept.
        const int slot = CardSet::slotFor(firstRank, suit);
        const Cell *first = nullptr;
        for (const Cell &cell : m_cells) {
            if (cell.target == slot) first = &cell;
        }
        if (!first) continue;
        QFont pip = font();
        pip.setBold(true);
        pip.setPixelSize(std::max(11, int(first->box.height() * 0.34)));
        painter.setFont(pip);
        painter.setPen(cardSuitIsRed(suit) ? scheme.cardRed : scheme.surfaceText);
        painter.drawText(QRectF(0, first->box.top(), m_labelWidth, first->box.height()),
                         Qt::AlignCenter, suitMark(suit));
    }
}
