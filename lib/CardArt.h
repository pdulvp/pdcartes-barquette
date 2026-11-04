#pragma once

#include "Palette.h"

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QPointF>
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

// Les lettres qui désignent les rangs dans le coin d'une carte.
//
// Un jeu à enseignes françaises ne se lit pas de la même façon selon le pays
// qui l'imprime : le valet est `V` en France, `J` dans le monde anglais, `B`
// en Allemagne et `S` en Espagne. Le dessin suit donc une convention, et non
// la seule que la France emploie.
//
// Les cinq lettres sont celles de l'as, du valet, du cavalier, de la dame et
// du roi. Le cavalier n'apparaît que sur un tarot, mais chaque convention a
// la sienne : une carte ne change pas de lettre selon le jeu où elle se
// trouve.
//
// Ces lettres ne sont pas des chaînes à traduire : `A` n'est pas la
// traduction française de `ace`, c'est la lettre qu'imprime une carte
// française, et elle reste `A` quelle que soit la langue du programme.
enum class RankLetters {
    French = 0,  // A V C D R   -- as, valet, cavalier, dame, roi
    English,     // A J Kn Q K  -- ace, jack, knight, queen, king
    German,      // A B C D K   -- Ass, Bube, Cavall, Dame, König
    // L'as espagnol est chiffré, et `C` y est le caballo, c'est-à-dire le
    // cavalier. Le jeu espagnol n'ayant pas de dame, celle-ci emprunte `D` :
    // lui laisser `C`, comme c'était le cas avant le tarot, donnerait deux
    // `C` sur un même jeu.
    Spanish,     // 1 S C D R   -- as, sota, caballo, dame empruntée, rey
};

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
    QColor panel;    // le panneau, beige
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

    // Le panneau se trace-t-il aussi sous les symboles des cartes de deux à
    // dix ?
    //
    // C'est un champ à part, et non la case ci-dessus étendue : celle-ci est
    // écrite dans les fichiers depuis qu'elle existe, où elle ne voulait dire
    // que « panneau derrière les figures ». Lui faire dire aussi « panneau
    // sous les symboles » changerait l'aspect des jeux déjà enregistrés dès
    // qu'on les redessine.
    //
    // La case ci-dessus reste le commutateur général : les cartes de deux à
    // dix reçoivent un panneau lorsque les deux sont vraies.
    //
    // La grille des symboles, elle, ne bouge pas : sa disposition est celle
    // des jeux imprimés, et son bord gauche est posé pour dégager la colonne
    // du coin. Le panneau est une assise tracée derrière elle.
    bool panelOnPips = false;

    // La largeur et la hauteur du panneau, en facteur de celles qu'il a par
    // défaut.
    //
    // Des facteurs et non des mesures, pour la raison qui vaut pour
    // `emblemSize` : le panneau est déduit de la carte, et les cartes sont
    // rendues à la hauteur qu'on demande. Les valeurs sont bornées au dessin,
    // entre `kPanelSmallest` et `kPanelLargest`, et le rectangle obtenu est
    // ramené dans la marge du papier : un panneau ne déborde pas de la carte.
    //
    // L'emblème étant inscrit dans un carré déduit du panneau, il suit ces
    // deux facteurs ; c'est `emblemSize` qui l'en détache.
    qreal panelWidth = 1.0;
    qreal panelHeight = 1.0;

    // La taille de l'emblème d'une figure -- couronne, diadème, fleur de lis,
    // bonnet du joker, symbole de l'as -- en facteur de celle qu'il a par
    // défaut.
    //
    // Un facteur et non une mesure : l'emblème est inscrit dans un carré
    // déduit de la carte, et les cartes sont rendues à la hauteur qu'on
    // demande. La valeur est bornée au dessin, entre `kEmblemSmallest` et
    // `kEmblemLargest`.
    qreal emblemSize = 1.0;

    // La carte porte-t-elle son emblème, et ses symboles ?
    //
    // Ce sont les deux façons qu'a une carte d'occuper son milieu, et aucune
    // carte n'a les deux : les cinq cartes à motif unique portent un emblème,
    // les cartes de deux à dix une grille de symboles. Ce sont néanmoins deux
    // cases et non une, parce qu'un style vaut pour les cinquante-trois
    // cartes d'un jeu : une seule case imposerait aux cartes chiffrées le
    // choix fait pour les figures.
    //
    // `emblemShown` couvre l'emblème d'une figure, le symbole de l'as, le
    // personnage du joker et le numéro romain d'un atout ; `pipsShown`, la
    // grille des symboles des cartes de deux à dix.
    //
    // Décochées, il ne reste que le papier, le panneau avec ce qu'il porte, et
    // les deux index des coins. C'est ce qu'il faut à une carte dont l'image
    // du panneau est le dessin : l'emblème viendrait par-dessus.
    //
    // Les index, eux, restent : ce sont eux qui disent le rang et la couleur.
    // Le joker porte lui aussi un index — l'étoile « ★ » — et son mot écrit
    // en dessous se règle à part, par `jokerNameShown`.
    bool emblemShown = true;
    bool pipsShown = true;

    // Le mot écrit sous le panneau du joker : « Joker », ou « Excuse » dans
    // un tarot — c'est la même case, et c'est la couleur des atouts qui
    // choisit le mot. Décochée, il ne reste que le personnage et l'étoile
    // des coins, qui nomme la carte à sa place.
    bool jokerNameShown = true;

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

    // La convention qui donne les lettres du coin.
    RankLetters letters = RankLetters::French;

    // Les lettres du coin sont-elles tracées ? Décochée, il ne reste que ce
    // que la carte porte au milieu. Le fond des lettres disparaît avec elles :
    // il n'est tracé que là où la lettre l'est, et il n'y a plus de lettre.
    bool indexShown = true;

    // Le fond des lettres du coin : un carré arrondi posé derrière le rang,
    // en haut à gauche et en bas à droite, et tracé par-dessus le panneau
    // comme l'index l'est toujours.
    //
    // La case commande la présence du rectangle, comme `panelled` commande
    // celle du panneau : décochée, il ne reste que la lettre sur le papier
    // ou le panneau. Le filet du carré n'est pas un réglage : il est calculé
    // au dessin, de la même couleur que celui du panneau.
    bool indexBackShown = false;
    QColor indexBack = QColor(Qt::white);  // le fond, blanc par défaut

    // La largeur et la hauteur du fond des lettres, en facteur de celles
    // qu'il a par défaut — le contenu entier du coin : la lettre et le
    // symbole posé sous elle. Des facteurs et non des mesures, pour la
    // raison qui vaut pour `panelWidth` : le coin est déduit de la carte, et
    // les cartes sont rendues à la hauteur qu'on demande. Bornés au dessin
    // entre `kIndexSmallest` et `kIndexLargest`. Le fond est centré sur ce
    // contenu, de sorte que le symbole reste dedans.
    qreal indexWidth = 1.0;
    qreal indexHeight = 1.0;

    // La taille de la police des lettres du coin, en facteur de celle
    // qu'elle a par défaut. La colonne du coin s'allonge avec, le symbole
    // posé sous le rang grossit dans la même proportion, et le fond des
    // lettres recouvre les deux. Bornée au dessin entre `kIndexFontSmallest`
    // et `kIndexFontLargest`.
    qreal indexFontSize = 1.0;

    // Les couleurs qu'une palette donne à une carte. C'est le style des
    // cartes que `pdcartes-rami` dessine sur son tapis.
    static CardStyle fromPalette(const Palette &scheme);

    bool operator==(const CardStyle &other) const;
    bool operator!=(const CardStyle &other) const { return !(*this == other); }
};

// L'image posée dans le panneau d'une carte, et sa place à l'intérieur.
//
// Elle vient à la place de la couleur de fond du panneau, et rien de plus :
// le filet du panneau, l'emblème ou les symboles de la carte, et le coin, se
// tracent par-dessus. Elle est rognée à l'arrondi du panneau.
//
// L'image est portée deux fois : telle qu'on l'a fournie, puisque c'est ainsi
// qu'elle est enregistrée, et décodée, puisque c'est ainsi qu'elle se dessine.
// Les deux sont des objets partagés : copier une recette ne copie pas les
// pixels.
struct PanelArt {
    QByteArray bytes;  // l'image telle qu'elle a été fournie
    QByteArray kind;   // son type : « png », « svg »...
    QImage image;      // la même, décodée

    // Le décalage du centre de l'image par rapport à celui du panneau, en
    // part de la largeur et de la hauteur de ce dernier, et le facteur
    // d'échelle, lequel vaut un lorsque l'image recouvre le panneau au plus
    // juste.
    //
    // Des parts et un facteur, et non des pixels : une carte est rendue à la
    // hauteur qu'on demande, et le cadrage doit valoir d'un rendu à l'autre
    // comme d'une image fournie à l'autre.
    QPointF offset;
    qreal scale = 1.0;

    bool isEmpty() const { return bytes.isEmpty() || image.isNull(); }

    // Remplit `image` à partir de `bytes`. Faux si l'image est illisible —
    // un SVG, par exemple, en l'absence du greffon qui le lit.
    bool decode();

    // L'égalité ne regarde pas les pixels : deux recettes qui portent les
    // mêmes octets portent la même image.
    bool operator==(const PanelArt &other) const {
        return bytes == other.bytes && kind == other.kind && offset == other.offset &&
               qFuzzyCompare(scale, other.scale);
    }
    bool operator!=(const PanelArt &other) const { return !(*this == other); }
};

// Bornes du facteur d'échelle d'une image de panneau. En dessous, l'image
// n'est plus qu'une vignette au milieu du panneau ; au-dessus, on n'en voit
// plus qu'un détail.
inline constexpr qreal kPanelArtSmallest = 0.20;
inline constexpr qreal kPanelArtLargest = 6.00;

// De quoi refaire une carte dessinée : ses couleurs, et la hauteur en pixels
// à laquelle elle est rendue.
//
// Une recette est conservée dans le fichier à côté de l'image qu'elle a
// produite, de sorte qu'un jeu rouvert dans l'atelier reste modifiable au
// lieu de n'être qu'une collection d'images figées.
struct DrawnRecipe {
    CardStyle style;
    int height = 420;  // hauteur de rendu, en pixels

    // L'image du panneau appartient à la carte, et non au style : un style se
    // pose sur cinquante-trois cartes à la fois, une image sur une seule.
    // C'est pourquoi elle est ici et non dans `CardStyle`.
    PanelArt panelArt;

    bool operator==(const DrawnRecipe &other) const {
        return height == other.height && style == other.style && panelArt == other.panelArt;
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

// Bornes de `CardStyle::panelWidth` et `CardStyle::panelHeight`. En dessous,
// le panneau n'est plus qu'une plaque au milieu du dessin qu'il devait
// porter ; au-dessus, il occupe la carte entière et le facteur ne produit
// plus rien.
inline constexpr qreal kPanelSmallest = 0.50;
// Deux cents pour cent : il en faut cent quatre-vingt-dix au plus étroit des
// panneaux, celui des figures, pour couvrir toute la largeur de la carte.
inline constexpr qreal kPanelLargest = 2.00;

// Bornes de `CardStyle::indexWidth` et `CardStyle::indexHeight`. En dessous,
// le fond des lettres n'est plus qu'une tache sous le rang ; au-dessus, il
// déborde de la colonne du coin sur le panneau.
inline constexpr qreal kIndexSmallest = 0.50;
inline constexpr qreal kIndexLargest = 2.00;

// Bornes de `CardStyle::indexFontSize`. En dessous, la lettre n'est plus
// lisible ; au-dessus, la colonne du coin mange la moitié de la carte.
inline constexpr qreal kIndexFontSmallest = 0.50;
inline constexpr qreal kIndexFontLargest = 2.00;

// Retourne la police du peintre réinitialisée.
//
// `QPainter` conserve la dernière police qui lui a été donnée, et les
// fonctions de dessin s'enchaînent sur le même peintre : un titre composé en
// capitales espacées laisserait le dessin suivant dans la même composition.
// Toutes les polices employées par le dessin dérivent de cette fonction
// plutôt que de `painter->font()`.
QFont plainFont(const QPainter *painter);

// Le rang écrit dans le coin d'une carte : la lettre que la convention donne
// aux quatre rangs qui en portent une, le nombre pour les autres, et l'étoile
// du joker.
QString cardRankLabel(int rank, RankLetters letters = RankLetters::French);

// Le nom d'une convention, pour une liste à choix : « Français (V D R) ».
QString rankLettersName(RankLetters letters);
// Les quatre conventions, dans l'ordre de l'énumération.
inline constexpr int kRankLettersCount = 4;

// Vrai pour le cœur et le carreau. La numérotation des couleurs est celle de
// `CardSet` : pique, cœur, carreau, trèfle.
bool cardSuitIsRed(int suit);

// Le symbole d'une couleur, centré sur un point et inscrit dans un carré de
// `span` de côté.
void drawPip(QPainter *painter, const QPointF &middle, qreal span, int suit, const QColor &ink);
// Le même, inscrit dans un rectangle : pour une légende ou le coin d'une
// plaque.
void drawSuitPip(QPainter *painter, const QRectF &box, int suit, const QColor &ink);

// Le rectangle du panneau d'une carte posée dans `box`, ou un rectangle nul
// lorsque la carte n'en porte pas : panneau retiré, carte de deux à dix sans
// panneau sous les symboles, ou carte trop petite pour qu'un panneau s'y
// lise.
//
// La fonction est publiée parce que l'atelier en a besoin pour placer une
// image dans le panneau : il faut savoir où ce dernier se trouve pour y
// rogner l'image et pour convertir un mouvement de souris en décalage. Le
// dessin emploie la même, de sorte qu'il n'y ait qu'une définition du
// panneau.
QRectF cardPanelBox(const QRectF &box, int rank, int suit, const CardStyle &style);

// Le même, pour un dos : le rectangle qu'occupe son treillis, et où son image
// vient se poser. Un dos est une carte comme les autres, à ceci près que son
// panneau ne se retire pas — il n'y aurait plus qu'un rectangle de couleur.
QRectF cardBackPanelBox(const QRectF &box, const CardStyle &style);

// Jusqu'où l'image d'un panneau peut être décalée, par axe et en part du
// panneau : exactement ce qu'il faut pour amener chacun de ses bords sur le
// bord correspondant du panneau, et pas davantage.
//
// La règle vaut dans les deux sens. Plus grande que le panneau, l'image se
// décale jusqu'à son bord sans jamais découvrir le beige qu'elle
// recouvrait ; plus petite, elle se pose où l'on veut dans le panneau sans
// jamais en sortir.
//
// Le dessin applique cette borne de lui-même : une recette dont le décalage
// la dépasse est ramenée, plutôt que de montrer un panneau vide. Elle est
// publiée pour que l'atelier borne le geste de la même façon qu'il sera
// dessiné.
QPointF panelArtReach(const QRectF &panel, const PanelArt &art);

// Dessine une carte : le papier, son filet, et ce qu'elle porte.
//
// `rank` va de 1 (l'as) à 13 (le roi), zéro désignant le joker ; `suit` de 0
// à 3 dans l'ordre pique, cœur, carreau, trèfle. C'est la numérotation de
// `CardSet::slotFor()`.
// `art` est l'image qui remplace la couleur de fond du panneau ; vide, le
// panneau garde sa couleur.
void drawDefaultFace(QPainter *painter, const QRectF &box, int rank, int suit,
                     const CardStyle &style, const PanelArt &art = {});

// Dessine le dos d'une carte : une marge encadrant un treillis, disposition
// traditionnelle.
// `art` est l'image qui remplace le treillis ; vide, le dos porte son
// treillis.
void drawDefaultBack(QPainter *painter, const QRectF &box, const CardStyle &style,
                     const PanelArt &art = {});
