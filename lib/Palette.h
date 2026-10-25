#pragma once

#include <QColor>
#include <QIcon>
#include <QStringList>

// Les couleurs du programme, dérivées d'une seule teinte.
//
// Un jeu de cartes est surtout une grande surface de tapis, avec quelques
// cartes claires dessus : tout l'aspect tient donc à une teinte unique, par
// exemple un vert jade ou un rouge bordeaux. Toutes les couleurs ci-dessous
// sont calculées à partir de ce seul nombre, plutôt qu'écrites une à une, ce
// qui permet d'ajouter un thème en ne donnant qu'une teinte.
struct Palette {
    int hue;              // la teinte, 0-359, dont tout le reste est dérivé
    QColor desk;          // le fond, derrière tout le reste
    QColor felt;          // le tapis, au centre
    QColor feltEdge;      // le tapis sur son pourtour, plus sombre
    QColor feltWeave;     // le fil du tissage, à peine plus clair que le tapis
    QColor feltText;      // le texte écrit à même le tapis
    QColor trim;          // le galon autour du tapis, et le bord de la table
    QColor accent;        // la couleur du thème : bandeaux, titres, tour de jeu
    QColor accentText;
    QColor surface;       // les panneaux, le corps des boîtes, les étiquettes
    QColor surfaceText;
    QColor field;         // les champs de saisie et le fond des listes
    QColor face;          // la bande dans laquelle sont posés les boutons
    QColor faceText;
    QColor cardFace;      // le papier d'une carte
    QColor cardInk;       // les couleurs noires, et le rang dans le coin
    QColor cardRed;       // les couleurs rouges
    QColor cardBack;      // le motif du dos d'une carte
    QColor cardBackInk;
    QColor highlight;     // la carte sous le pointeur
    QColor highlightText;
    QColor edge;          // le filet entre un champ et son fond
    QColor hint;          // une explication, par opposition à une étiquette
    QColor danger;        // un coup interdit, ou un score qui se dégrade
    QColor gold;          // ce qui est gagné : un record, une partie terminée
};

// Noir ou blanc, selon celui qui reste lisible sur cette couleur.
QColor readableTextOn(const QColor &background);

// `weight` de `over`, le reste de `under`.
QColor blendColours(const QColor &under, const QColor &over, qreal weight);

// Une icône du thème du bureau, désignée par son nom et recolorée en `ink`.
// Les noms sont essayés dans l'ordre, de sorte qu'un bureau qui ne connaît que
// l'ancien nom obtienne tout de même une icône. Nulle si aucun n'est installé.
QIcon tintedIcon(const QStringList &names, const QColor &ink);

// La palette que produit une teinte, en clair ou en sombre. Elle est renvoyée
// au lieu d'être seulement appliquée, car un menu peut avoir à présenter
// plusieurs thèmes, chacun dans ses couleurs, pendant que le programme en
// porte un.
Palette paletteForHue(int hue, bool dark);

// La palette en cours. Elle est fixée au démarrage, et à chaque changement de
// mode clair ou sombre.
const Palette &currentPalette();
void setCurrentPalette(int hue);

bool darkMode();
void setDarkMode(bool dark);

// La couleur de la n-ième étincelle d'un feu d'artifice : la teinte du thème
// décalée un peu sur le cercle des couleurs et un peu en clarté, de sorte
// qu'un grand nombre d'étincelles reste dans la même famille de couleurs tout
// en restant distinguables.
QColor festiveColour(int index);

// La couleur propre à un adversaire, lisible sur le tapis du thème en cours.
// Chaque adversaire a sa teinte, qui figure sur son étiquette, sur son emblème
// et sur le bord gauche de chacune de ses combinaisons : c'est ainsi qu'un
// tapis couvert de combinaisons indique à qui appartient chacune.
QColor rivalColour(int hue);
