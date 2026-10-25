#pragma once

#include "DllResources.h"

#include <QPixmap>
#include <QPoint>
#include <QRectF>
#include <QVector>
#include <QWidget>

// Les images ouvertes, en attente d'être placées dans une case du jeu.
class SourceTray : public QWidget {
    Q_OBJECT

public:
    explicit SourceTray(QWidget *parent = nullptr);

    struct Item {
        DllImage image;
        QPixmap thumb;
        bool used = false;     // déjà placée dans une case
    };

    void setItems(const QVector<DllImage> &images);
    void addItems(const QVector<DllImage> &images);
    // Remplace une planche par ses morceaux : la planche découpée n'est pas
    // une carte et n'a plus à figurer dans la liste. Renvoie faux si aucune
    // des images fournies n'est lisible, l'ancienne restant alors en place.
    bool replaceItem(int index, const QVector<DllImage> &images);
    // Retire une seule image : la seconde moitié d'un collage, l'image
    // recollée ayant pris la place de la première.
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
    // Clic droit sur une image. C'est la fenêtre qui décide des actions
    // proposées, car elle seule connaît ce qui est sélectionné dans l'autre
    // zone.
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
