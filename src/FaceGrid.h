#pragma once

#include <QRectF>
#include <QVector>
#include <QWidget>

class CardSet;

// Les cases d'un jeu : quatre rangées de treize, puis le joker et les dos.
class FaceGrid : public QWidget {
    Q_OBJECT

public:
    explicit FaceGrid(QWidget *parent = nullptr);

    // Le jeu affiché. Il n'est ni possédé ni copié.
    void watch(CardSet *set);
    void reread();

    // La case sélectionnée : positive ou nulle pour une face, négative pour
    // un dos (voir backTarget).
    int target() const { return m_target; }
    void setTarget(int target);

    static int backTarget(int at) { return -1 - at; }
    static int backOfTarget(int target) { return target <= -1 ? -1 - target : -1; }
    static bool isBack(int target) { return target <= -1; }

    // La case vide suivante.
    int nextEmptyFace(int from) const;

Q_SIGNALS:
    void targetChosen(int target);
    void clearRequested(int target);
    void addBackRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct Cell {
        int target = 0;
        QRectF box;
        QString caption;
    };

    void relayout();
    int cellAt(const QPointF &where) const;

    CardSet *m_set = nullptr;
    QVector<Cell> m_cells;
    qreal m_labelWidth = 26.0;
    int m_target = 0;
    int m_hovered = -10000;
};
