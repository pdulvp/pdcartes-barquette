#pragma once

#include "Palette.h"

#include <QColor>
#include <QRectF>
#include <QString>

class QFont;
class QPainter;
class QPointF;

// Le dessin d'une carte à jouer.
//
// Les cartes sont tracées par le programme plutôt que chargées depuis des
// images. Un jeu en demanderait cinquante-quatre, ce qui supposerait soit un
// travail d'illustration, soit un dossier de fichiers à localiser à
// l'exécution et susceptible d'être absent. Dessinées, les cartes restent
// nettes à toute taille, prennent les couleurs qu'on leur donne, et le
// programme demeure un fichier unique.
//
// Ce module ne connaît qu'une carte seule, posée dans un rectangle. Le tapis
// sur lequel elle est posée, l'ombre qu'elle y porte et la mise en évidence
// du coup qui vient d'être joué sont l'affaire du programme de jeu.

// Proportion conservée par une carte dessinée, quelle que soit sa taille de
// rendu.
//
// C'est celle des cartes de `CARDS.DLL`, qui mesurent 71 sur 96 pixels. Les
// cartes réelles sont plus étroites — 2,25 sur 3,5 pouces, soit 0,642 — mais
// à l'écran cette proportion donne une carte haute, dont le milieu reste
// vide.
//
// Un jeu de cartes lu dans un fichier fournit la sienne : voir
// `CardSet::aspect()`.
inline constexpr qreal kCardRatio = 71.0 / 96.0;

// Rayon d'arrondi des coins d'une carte de cette taille. La fonction est
// partagée afin qu'un aperçu de carte affiché ailleurs adopte exactement le
// même arrondi.
qreal cardCornerFor(const QRectF &box);

// Les couleurs d'une carte dessinée.
//
// Elles sont rassemblées ici plutôt que tirées d'une `Palette` à l'intérieur
// du dessin, et ce pour deux raisons. Une palette décrit un programme entier
// et ne propose pas la couleur du panneau des figures, qui n'existe que sur
// une carte. Et l'atelier fabrique des jeux dont les couleurs n'ont aucune
// raison de suivre les siennes : elles sont choisies carte par carte.
// Ce que « les figures » recouvre ici : le valet, la dame, le roi, le joker et
// l'as. Ce sont les cinq cartes qui ne montrent pas de grille de symboles mais
// un motif unique, assez grand pour porter un panneau, un dégradé et une
// taille à soi.
struct CardStyle {
    QColor paper;    // le papier de la carte
    QColor ink;      // le pique et le trèfle, et le rang dans le coin
    QColor red;      // le cœur et le carreau
    QColor panel;    // le panneau des figures, beige
    QColor cap;      // le bonnet et l'étoile du joker, et le mot « Joker »
    QColor bell;     // les grelots du bonnet
    QColor back;     // le fond du dos
    QColor backInk;  // le treillis du dos

    // Le panneau des figures est-il tracé ? Sans lui, l'emblème est posé à
    // même le papier, et les traits évidés qui s'y découpaient -- le bandeau
    // de la couronne, celui du diadème, la ceinture de la fleur de lis -- se
    // découpent sur le papier.
    //
    // Seule la présence du rectangle est en jeu : l'emblème garde sa taille
    // et sa place, l'as compris. C'est `emblemSize` qui règle la taille.
    //
    // C'est un booléen plutôt qu'un panneau transparent : un panneau
    // transparent laisserait son filet, et le filet est ce qui se voit en
    // premier.
    bool panelled = true;

    // La taille de l'emblème d'une figure -- couronne, diadème, fleur de lis,
    // bonnet du joker, symbole de l'as -- en facteur de celle qu'il a par
    // défaut.
    //
    // Un facteur et non une mesure : l'emblème est inscrit dans un carré
    // déduit de la carte, et les cartes sont rendues à la hauteur qu'on
    // demande. La valeur est bornée au dessin, entre `kEmblemSmallest` et
    // `kEmblemLargest`.
    qreal emblemSize = 1.0;

    // Les grands motifs ont-ils l'aspect du verre ?
    //
    // Il s'agit de l'emblème des figures, seuls motifs d'une carte assez
    // grands pour qu'un dégradé s'y lise. Ils sont alors remplis d'un dégradé
    // au lieu d'un aplat — clair en haut, sombre juste sous le milieu, et une lumière
    // renvoyée par le bas.
    //
    // Les symboles des cartes de deux à dix restent plats, et ceux des coins
    // avec eux : à leur taille, la rampe ne produirait qu'une salissure. Les
    // traits évidés d'un emblème restent plats également : ils représentent
    // un creux, et un creux ne renvoie pas la lumière de la surface qui
    // l'entoure.
    bool glassy = false;

    // Les couleurs qu'une palette donne à une carte. C'est le style des
    // cartes que `pdcartes-rami` dessine sur son tapis.
    static CardStyle fromPalette(const Palette &scheme);

    bool operator==(const CardStyle &other) const;
    bool operator!=(const CardStyle &other) const { return !(*this == other); }
};

// De quoi refaire une carte dessinée : ses couleurs, et la hauteur en pixels
// à laquelle elle est rendue.
//
// Une recette est conservée dans le fichier à côté de l'image qu'elle a
// produite, de sorte qu'un jeu rouvert dans l'atelier reste modifiable au
// lieu de n'être qu'une collection d'images figées.
struct DrawnRecipe {
    CardStyle style;
    int height = 420;  // hauteur de rendu, en pixels

    bool operator==(const DrawnRecipe &other) const {
        return height == other.height && style == other.style;
    }
    bool operator!=(const DrawnRecipe &other) const { return !(*this == other); }
};

// Résolution par défaut d'une carte rendue en image : suffisante pour un
// affichage agrandi, sans donner un fichier démesuré.
inline constexpr int kDrawnHeight = 420;

// Bornes de `CardStyle::emblemSize`. En dessous, l'emblème n'est plus
// qu'une tache ; au-dessus, il déborde du papier.
inline constexpr qreal kEmblemSmallest = 0.30;
inline constexpr qreal kEmblemLargest = 1.80;

// Retourne la police du peintre réinitialisée.
//
// `QPainter` conserve la dernière police qui lui a été donnée, et les
// fonctions de dessin s'enchaînent sur le même peintre : un titre composé en
// capitales espacées laisserait le dessin suivant dans la même composition.
// Toutes les polices employées par le dessin dérivent de cette fonction
// plutôt que de `painter->font()`.
QFont plainFont(const QPainter *painter);

// Le rang écrit dans le coin d'une carte : « A », « 2 » à « 10 », « V »,
// « D », « R », et l'étoile du joker.
QString cardRankLabel(int rank);

// Vrai pour le cœur et le carreau. La numérotation des couleurs est celle de
// `CardSet` : pique, cœur, carreau, trèfle.
bool cardSuitIsRed(int suit);

// Le symbole d'une couleur, centré sur un point et inscrit dans un carré de
// `span` de côté.
void drawPip(QPainter *painter, const QPointF &middle, qreal span, int suit, const QColor &ink);
// Le même, inscrit dans un rectangle : pour une légende ou le coin d'une
// plaque.
void drawSuitPip(QPainter *painter, const QRectF &box, int suit, const QColor &ink);

// Dessine une carte : le papier, son filet, et ce qu'elle porte.
//
// `rank` va de 1 (l'as) à 13 (le roi), zéro désignant le joker ; `suit` de 0
// à 3 dans l'ordre pique, cœur, carreau, trèfle. C'est la numérotation de
// `CardSet::slotFor()`.
void drawDefaultFace(QPainter *painter, const QRectF &box, int rank, int suit,
                     const CardStyle &style);

// Dessine le dos d'une carte : une marge encadrant un treillis, disposition
// traditionnelle.
void drawDefaultBack(QPainter *painter, const QRectF &box, const CardStyle &style);
