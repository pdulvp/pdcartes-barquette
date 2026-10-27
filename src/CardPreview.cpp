#include "CardPreview.h"

#include "CardSet.h"
#include "Palette.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

CardStylePreview::CardStylePreview(bool compact, QWidget *parent)
    : QWidget(parent), m_compact(compact), m_key(CardSet::slotFor(13, 1)) {
    setMinimumHeight(compact ? 70 : 136);
}

void CardStylePreview::setRecipe(const DrawnRecipe &how) {
    m_recipe = how;
    update();
}

void CardStylePreview::showCard(int key) {
    m_key = key;
    update();
}

void CardStylePreview::onPlaced(std::function<void(const PanelArt &, bool settled)> what) {
    m_placed = std::move(what);
}

QRectF CardStylePreview::panelHere() const {
    if (!m_compact || m_cardBox.isEmpty()) return {};
    // Un dos est une carte comme les autres : son panneau est le rectangle
    // qu'occupe son treillis, et le geste s'y fait de la même façon.
    if (m_key < 0) return cardBackPanelBox(m_cardBox, m_recipe.style);
    return cardPanelBox(m_cardBox, CardSet::rankOfSlot(m_key), CardSet::suitOfSlot(m_key),
                        m_recipe.style);
}

bool CardStylePreview::placeable() const {
    return !m_recipe.panelArt.isEmpty() && !panelHere().isEmpty();
}

void CardStylePreview::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !placeable()) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_dragFrom = event->position();
    m_dragOffset = m_recipe.panelArt.offset;
    m_dragging = true;
    setCursor(Qt::ClosedHandCursor);
}

void CardStylePreview::mouseMoveEvent(QMouseEvent *event) {
    if (!m_dragging) {
        setCursor(placeable() ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }
    const QRectF panel = panelHere();
    const QPointF moved = event->position() - m_dragFrom;
    // Le décalage est une part du panneau : le même geste donne le même
    // cadrage, que l'aperçu soit grand ou petit. Il va jusqu'au bord de
    // l'image, et pas plus loin : c'est la cartothèque qui dit où, le dessin
    // bornant de la même façon.
    const QPointF reach = panelArtReach(panel, m_recipe.panelArt);
    m_recipe.panelArt.offset =
        QPointF(std::clamp(m_dragOffset.x() + moved.x() / panel.width(), -reach.x(), reach.x()),
                std::clamp(m_dragOffset.y() + moved.y() / panel.height(), -reach.y(), reach.y()));
    update();
    // Celui qui a posé l'aperçu suit le geste sans le classer : sa copie de la
    // recette doit rester celle qu'on voit, faute de quoi le réglage suivant
    // remettrait l'image où elle était.
    if (m_placed) m_placed(m_recipe.panelArt, false);
}

void CardStylePreview::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    if (!m_dragging) return;
    m_dragging = false;
    setCursor(placeable() ? Qt::OpenHandCursor : Qt::ArrowCursor);
    if (m_placed) m_placed(m_recipe.panelArt, true);
}

void CardStylePreview::setScale(qreal factor) {
    m_recipe.panelArt.scale = std::clamp(factor, kPanelArtSmallest, kPanelArtLargest);
    // Réduire l'image réduit ce qui dépasse : le décalage enregistré doit
    // rester celui qu'on voit.
    const QPointF reach = panelArtReach(panelHere(), m_recipe.panelArt);
    m_recipe.panelArt.offset =
        QPointF(std::clamp(m_recipe.panelArt.offset.x(), -reach.x(), reach.x()),
                std::clamp(m_recipe.panelArt.offset.y(), -reach.y(), reach.y()));
    update();
    if (m_placed) m_placed(m_recipe.panelArt, true);
}

void CardStylePreview::refit() {
    // L'image recouvre de nouveau le panneau au plus juste, centrée.
    m_recipe.panelArt.offset = QPointF();
    m_recipe.panelArt.scale = 1.0;
    update();
    if (m_placed) m_placed(m_recipe.panelArt, true);
}

void CardStylePreview::mouseDoubleClickEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    if (!placeable()) return;
    refit();
}

void CardStylePreview::wheelEvent(QWheelEvent *event) {
    if (!placeable()) {
        QWidget::wheelEvent(event);
        return;
    }
    const int steps = event->angleDelta().y();
    if (steps == 0) return;
    setScale(m_recipe.panelArt.scale * std::pow(1.0012, steps));
    event->accept();
}

void CardStylePreview::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    const Palette &scheme = currentPalette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(rect(), blendColours(scheme.surface, scheme.desk, 0.45));

    // En version compacte, la carte choisie ; sinon un échantillon des quatre
    // sortes de dessin : une figure, une carte à symboles, le joker et le dos.
    QVector<int> shown;
    if (m_compact) {
        shown.append(m_key);
    } else {
        shown = {CardSet::slotFor(13, 1), CardSet::slotFor(7, 0), CardSet::kJokerSlot, -1};
    }

    constexpr qreal kGap = 7.0;
    const qreal room = (width() - kGap * (shown.size() + 1)) / shown.size();
    qreal tall = height() - kGap * 2.0;
    qreal wide = tall * kCardRatio;
    if (wide > room) {
        wide = room;
        tall = wide / kCardRatio;
    }
    if (wide < 8.0 || tall < 12.0) return;

    qreal x = (width() - (wide * shown.size() + kGap * (shown.size() - 1))) / 2.0;
    const qreal y = (height() - tall) / 2.0;
    for (const int key : shown) {
        const QRectF box(x + 0.5, y + 0.5, wide - 1.0, tall - 1.0);
        // La boîte de la carte est retenue pour le placement : c'est elle qui
        // donne le panneau, et donc le cadre du geste.
        if (m_compact) m_cardBox = box;
        if (key < 0) {
            drawDefaultBack(&painter, box, m_recipe.style, m_recipe.panelArt);
        } else {
            drawDefaultFace(&painter, box, CardSet::rankOfSlot(key), CardSet::suitOfSlot(key),
                            m_recipe.style, m_recipe.panelArt);
        }
        x += wide + kGap;
    }

    // Le cadre du placement, pendant le geste : un filet pointillé sur le
    // panneau, qui dit où l'image est rognée.
    if (m_dragging) {
        const QRectF panel = panelHere();
        if (!panel.isEmpty()) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(scheme.accent, 1.0, Qt::DashLine));
            painter.drawRect(panel);
        }
    }
}
