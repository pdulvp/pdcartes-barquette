#pragma once

#include "CardArt.h"

#include <QRectF>
#include <QWidget>

#include <functional>

// L'aperçu : les cartes telles que la recette les dessine.
//
// En version compacte, il montre la carte choisie, et sert alors à placer
// l'image de son panneau : glisser la déplace, la molette la redimensionne,
// un double-clic la recadre. Le panneau est le cadre du geste — l'image y est
// rognée, et un mouvement de souris se convertit en part de sa taille, de
// sorte que le cadrage vaille quelle que soit la taille du rendu.
//
// Il n'a ni signal ni slot, et n'est donc pas un `QObject` déclaré comme tel :
// c'est un widget qui se peint et qui rappelle celui qui l'a posé.
class CardStylePreview : public QWidget {
public:
    // `compact` montre la carte choisie, et reçoit le geste qui place
    // l'image de son panneau ; sinon, un échantillon des quatre sortes de
    // dessin, qui ne le reçoit pas.
    explicit CardStylePreview(bool compact, QWidget *parent = nullptr);

    void setRecipe(const DrawnRecipe &how);
    void showCard(int key);
    DrawnRecipe recipe() const { return m_recipe; }

    // Appelé lorsque l'image du panneau a bougé. `settled` est faux tant que
    // le bouton est enfoncé : la carte du jeu n'est refaite qu'une fois, au
    // relâchement, le tracé puis l'encodage d'une image par mouvement de
    // souris rendant le glissement poussif.
    void onPlaced(std::function<void(const PanelArt &, bool settled)> what);

    // Porter l'image à une échelle, et la recadrer : ce que font la molette
    // et le double-clic, pour qui les commande d'ailleurs. Les deux
    // annoncent le placement comme le ferait le geste.
    void setScale(qreal factor);
    void refit();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    // Le panneau de la carte montrée, dans les coordonnées du widget, ou un
    // rectangle nul : carte sans panneau, ou aperçu d'exemple.
    QRectF panelHere() const;
    // L'image du panneau peut-elle être placée ici ?
    bool placeable() const;

    DrawnRecipe m_recipe;
    bool m_compact = false;
    int m_key = 0;

    // Le placement de l'image du panneau.
    mutable QRectF m_cardBox;
    std::function<void(const PanelArt &, bool settled)> m_placed;
    QPointF m_dragFrom;
    QPointF m_dragOffset;
    bool m_dragging = false;
};
