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
// contiennent plus une image par carte : ils en placent cinquante-deux sur une
// seule planche, en rangées et en colonnes. La boîte demande la taille d'un
// morceau, affiche le découpage sur l'image, et renvoie les morceaux.
//
// Une taille plutôt qu'un nombre de colonnes : un nombre de colonnes divise la
// planche en parts égales, et des parts égales tombent entre deux pixels dès
// que la planche ne se divise pas exactement — les morceaux sortent alors
// décalés d'un pixel les uns par rapport aux autres. Une taille donne des
// morceaux qui font tous exactement cette taille, et ce qui reste au bord
// apparaît comme tel au lieu d'être réparti sur tous les morceaux.
class SplitDialog : public QDialog {
    Q_OBJECT

public:
    // `sheet` est l'image à découper, telle qu'elle figure dans la liste des
    // images ouvertes.
    SplitDialog(const DllImage &sheet, QWidget *parent = nullptr);

    // Vrai si l'image a pu être lue. Une planche illisible n'a rien à
    // découper : la boîte n'a alors pas lieu d'être ouverte.
    bool readable() const { return !m_sheet.isNull(); }

    // Les morceaux, dans l'ordre de lecture. Vide tant que la boîte n'a pas
    // été validée.
    const QVector<DllImage> &pieces() const { return m_pieces; }

private:
    // Ce que les quatre champs de marge retirent de la planche avant son
    // quadrillage.
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
