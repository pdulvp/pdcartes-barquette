#pragma once

#include "CardArt.h"

#include <QDialog>

class CardStylePreview;
class QSpinBox;

// Placer l'image d'un panneau, la carte montrée en grand.
//
// L'aperçu de la section « La carte dessinée » reçoit la hauteur de cette
// section, laquelle est celle de la colonne des champs : la carte y tient en
// cent soixante-cinq pixels, et son panneau en soixante-cinq. C'est assez
// pour cadrer grossièrement, non pour ajuster. Cette fenêtre montre la même
// carte à quatre cents pixels, et c'est là qu'on place.
//
// Le placement s'applique à mesure : la fenêtre n'a pas de « Valider », la
// carte se refaisant derrière elle comme pour tout autre réglage. C'est
// `placed()` qui le dit, à chaque fois que l'image a bougé.
class PlaceArtDialog : public QDialog {
    Q_OBJECT

public:
    // `how` est la recette de la carte, image comprise ; `key` la case de
    // `CardSet` qu'elle occupe, de quoi savoir quelle carte dessiner.
    PlaceArtDialog(const DrawnRecipe &how, int key, QWidget *parent = nullptr);

Q_SIGNALS:
    // L'image a bougé. La carte est à refaire avec cette place.
    void placed(const PanelArt &art);

private:
    // Reporte dans le champ l'échelle que le geste vient de donner, et
    // l'inverse, sans que l'un rappelle l'autre indéfiniment.
    void showScale();

    DrawnRecipe m_recipe;
    CardStylePreview *m_preview = nullptr;
    QSpinBox *m_scale = nullptr;
    bool m_quiet = false;
};
