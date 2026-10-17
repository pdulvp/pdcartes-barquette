#pragma once

#include "DllResources.h"

#include <QPixmap>
#include <QPoint>
#include <QRectF>
#include <QVector>
#include <QWidget>

// Les images ouvertes, en attente d'être rangées dans une place du jeu.
class SourceTray : public QWidget {
    Q_OBJECT

public:
    explicit SourceTray(QWidget *parent = nullptr);

    struct Item {
        DllImage image;
        QPixmap thumb;
        bool used = false;     // déjà rangée dans une place
    };

    void setItems(const QVector<DllImage> &images);
    void addItems(const QVector<DllImage> &images);
    // Une planche s'en va, ses morceaux prennent sa place : l'image découpée
    // n'est pas une carte et n'a plus rien à faire sous la main. Faux si
    // aucune des images données ne se lit, l'ancienne restant alors en place.
    bool replaceItem(int index, const QVector<DllImage> &images);
    // Une image s'en va seule : la seconde moitié d'un collage, dont l'image
    // recollée a pris la place de la première.
    void removeItem(int index);
    void clearItems();
    int count() const { return m_items.size(); }
    const Item *at(int index) const;
    const QVector<Item> &items() const { return m_items; }

    void markUsed(int index, bool used);
    void forgetUsage();

    int chosen() const { return m_chosen; }

Q_SIGNALS:
    void picked(int index);
    void pickedTwice(int index);
    // Clic droit sur une image : ce qu'on peut en faire est décidé par la
    // fenêtre, qui est seule à savoir ce qui est choisi ailleurs.
    void menuAsked(int index, const QPoint &where);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void relayout();
    void makeThumb(Item *item);
    int cellAt(const QPointF &where) const;

    QVector<Item> m_items;
    QVector<QRectF> m_boxes;
    int m_chosen = -1;
    int m_hovered = -1;
};
