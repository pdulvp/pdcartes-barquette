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
    // Le panneau : sa présence, son extension aux cartes de deux à dix, sa
    // largeur et sa hauteur, puis la taille de l'emblème qu'il porte.
    QCheckBox *m_panelled = nullptr;
    QCheckBox *m_pipPanel = nullptr;
    QSpinBox *m_panelWide = nullptr;
    QSpinBox *m_panelTall = nullptr;
    QSpinBox *m_emblem = nullptr;
    // Le fond des lettres du coin : sa présence, sa couleur, et sa taille.
    QCheckBox *m_indexBack = nullptr;
    QSpinBox *m_indexWide = nullptr;
    QSpinBox *m_indexTall = nullptr;
    // Le milieu de la carte : l'emblème des cartes à motif unique, et la
    // grille des cartes chiffrées. Deux cases, un style valant pour les
    // cinquante-trois cartes d'un jeu.
    QCheckBox *m_emblemShown = nullptr;
    QCheckBox *m_pipsShown = nullptr;
    QCheckBox *m_glassy = nullptr;
    QComboBox *m_letters = nullptr;
};
