#pragma once

#include "CardArt.h"

#include <QWidget>

class CardStylePreview;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QToolButton;

// Les réglages d'une carte dessinée : ses couleurs, sa résolution, et un
// aperçu de ce qu'elles donnent.
//
// Le même widget sert deux fois. À droite, en version complète, il règle les
// cartes qui vont être posées ; à gauche, en version compacte, il règle celle
// qui est choisie dans la grille. Un seul widget pour les deux, car ce sont
// bien les mêmes réglages : deux formulaires finiraient par ne plus proposer
// les mêmes couleurs.
class CardStyleEditor : public QWidget {
    Q_OBJECT

public:
    // `compact` réduit l'aperçu à une carte et resserre les champs : c'est la
    // version qui tient sous la grille des cases.
    explicit CardStyleEditor(bool compact, QWidget *parent = nullptr);

    DrawnRecipe recipe() const { return m_recipe; }
    // Pose une recette sans émettre `edited()` : c'est l'appelant qui la
    // fournit, il n'a pas à être rappelé.
    void setRecipe(const DrawnRecipe &how);

    // La carte montrée par l'aperçu, désignée comme une case de `CardSet` :
    // de 0 à 52 une face, une valeur négative un dos.
    void showCard(int key);

    // Reprend les couleurs du thème de la fenêtre. L'aperçu, lui, garde
    // celles de la recette : ce sont celles du jeu en fabrication.
    void applyColours();

Q_SIGNALS:
    void edited(const DrawnRecipe &how);

private:
    void pickColour(int field);
    void refreshSwatches();
    void noteEdit();

    DrawnRecipe m_recipe;
    bool m_compact = false;
    bool m_quiet = false;  // vrai pendant `setRecipe()`

    CardStylePreview *m_preview = nullptr;
    QVector<QToolButton *> m_swatches;
    QComboBox *m_theme = nullptr;
    QCheckBox *m_dark = nullptr;
    QSpinBox *m_height = nullptr;
    // La présence du panneau des figures, et la taille de l'emblème qu'il
    // porte.
    QCheckBox *m_panelled = nullptr;
    QSpinBox *m_emblem = nullptr;
};
