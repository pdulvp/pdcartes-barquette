#pragma once

#include "DllResources.h"

#include <QDialog>
#include <QImage>
#include <QMargins>
#include <QVector>

class QLabel;
class QSpinBox;
class SheetPreview;

// Découper une planche en morceaux. Les jeux de cartes récents de Windows ne
// rangent plus une image par carte : ils en mettent cinquante-deux sur une
// seule planche, en rangées et en colonnes. La boîte demande la taille d'un
// morceau, montre le découpage sur l'image, et rend les morceaux.
//
// Une taille plutôt qu'un compte de colonnes : un compte partage la planche en
// parts égales, et des parts égales tombent entre deux pixels dès que la
// planche ne se divise pas juste — les morceaux sortent alors à un pixel près
// les uns des autres, décalés d'autant. Une taille donne des morceaux qui font
// tous exactement cela, et le reste du bord se voit au lieu d'être réparti.
class SplitDialog : public QDialog {
    Q_OBJECT

public:
    // `sheet` est l'image à découper, telle qu'elle est dans la corbeille.
    SplitDialog(const DllImage &sheet, QWidget *parent = nullptr);

    // Vrai si l'image a pu être lue : une planche illisible n'a rien à
    // découper, et la boîte n'a pas lieu d'être ouverte.
    bool readable() const { return !m_sheet.isNull(); }

    // Les morceaux, dans l'ordre de lecture. Vides tant que la boîte n'a pas
    // été acceptée.
    const QVector<DllImage> &pieces() const { return m_pieces; }

private:
    // Ce que les quatre champs de marge retirent de la planche avant qu'elle
    // soit quadrillée.
    QMargins margins() const;
    void retell();
    void cut();

    QImage m_sheet;
    QString m_stem;        // le nom de la planche, sans dossier ni extension
    QVector<DllImage> m_pieces;

    SheetPreview *m_preview = nullptr;
    QSpinBox *m_left = nullptr;
    QSpinBox *m_top = nullptr;
    QSpinBox *m_right = nullptr;
    QSpinBox *m_bottom = nullptr;
    QSpinBox *m_cellWidth = nullptr;
    QSpinBox *m_cellHeight = nullptr;
    QSpinBox *m_trim = nullptr;
    QLabel *m_tally = nullptr;
};
