#pragma once

#include "CardArt.h"

#include <QByteArray>
#include <QHash>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QVector>

// Un jeu de cartes : 52 faces, le joker et des dos, dans un fichier unique.
//
// Format :
//
//     « CARTES »          6 octets, pour reconnaître le fichier
//     quint16                 la version du format, 1 pour l'instant
//     QByteArray              tout le reste, compressé
//
// et le reste, une fois décompressé, est un QDataStream en Qt_6_0 :
//
//     QString nom, auteur, note
//     qint32  largeur, hauteur   la taille de référence d'une face
//     qint32  nombre de faces    puis, pour chacune :
//         qint8 rang (0 = joker, 1 = as … 13 = roi, 14 = cavalier),
//         qint8 couleur (0-3 les enseignes, 4 les atouts, dont le rang est
//                        alors le numéro de 1 à 21),
//         QByteArray type d'image (« png », « svg », « bmp », « jpg »),
//         QByteArray octets
//     qint32  nombre de dos      puis, pour chacun :
//         QString nom, QByteArray type d'image, QByteArray octets
//
// Les images sont stockées telles quelles : un PNG reste un PNG, un SVG reste
// un SVG.
//
// Après les dos viennent des blocs facultatifs, chacun précédé de son
// étiquette et de sa version :
//
//     QByteArray « genre »       qint32 version, qint32 genre
//     QByteArray « recettes »    qint32 version, qint32 nombre, puis pour
//                                chacune : qint32 case, qint32 hauteur de
//                                rendu, et les couleurs et réglages du style
//
// Les blocs sont à la fin, et la version du format reste 1 pour les jeux de
// 52 et de 32 : un programme écrit avant eux s'arrête aux dos et ignore ce
// qui suit. Un tel jeu s'ouvre donc dans une version déjà installée, laquelle
// y voit de simples images -- ce qu'elles sont.
//
// Le format 2 est réservé aux fichiers qui contiennent une carte que le
// format 1 ne sait pas désigner : un cavalier, de rang 14, ou un atout, de
// couleur 4. Un lecteur ancien ramène l'un et l'autre dans les bornes qu'il
// connaît -- le cavalier devient le roi de sa couleur, l'atout devient un
// trèfle -- et montrerait donc de fausses cartes sans signaler d'erreur. Le
// format 2 le lui fait refuser avec un message juste.
//
// C'est le contenu qui décide, et non le genre déclaré : celui-ci dit comment
// présenter le jeu, pas ce qu'il contient. Un tarot ramené à un jeu de 52
// garde ses cavaliers et ses atouts, cachés mais écrits, et reste donc du
// format 2 ; un tarot vide, lui, s'écrit en format 1, n'ayant rien que le
// format 1 ne sache dire.

struct Artwork {
    QByteArray kind;    // « png », « svg », « bmp », « jpg »
    QByteArray bytes;

    bool isEmpty() const { return bytes.isEmpty(); }
    bool isVector() const { return kind == "svg" || kind == "svgz"; }

    // Le type d'image, reconnu aux premiers octets plutôt qu'à l'extension.
    static QByteArray sniff(const QByteArray &bytes);
    static Artwork fromFile(const QString &path, QString *why = nullptr);
};

// Le genre d'un jeu : ce qu'il contient, et donc les cases qu'il emploie.
//
// Les trois genres partagent la même numérotation de cases ; le genre ne dit
// que lesquelles sont employées. La dame de cœur est donc la même case dans
// un jeu de 52 et dans un tarot.
enum class DeckKind {
    Full = 0,  // 52 cartes et le joker : l'as au roi, quatre couleurs
    Short,     // 32 cartes et le joker : du sept à l'as
    Tarot,     // 78 cartes : quatre couleurs de 14, 21 atouts, l'excuse
};

class CardSet {
public:
    // L'espace des cases, celui du plus grand genre : l'excuse ou le joker,
    // puis quatre couleurs de quatorze rangs, puis les vingt et un atouts.
    // Les genres plus petits laissent les cases qu'ils n'emploient pas
    // vides, et ne les montrent pas.
    static constexpr int kRanksInSuit = 14;
    static constexpr int kTrumpCount = 21;
    static constexpr int kSlots = 1 + 4 * kRanksInSuit + kTrumpCount;  // 78
    static constexpr int kJokerSlot = 0;
    // La couleur sous laquelle les atouts sont rangés : une cinquième, qui
    // n'est pas une enseigne.
    static constexpr int kTrumpSuit = 4;
    // Le rang du cavalier. Il vient après le roi et non entre le valet et la
    // dame : les rangs sont écrits dans le fichier, et en insérer un au
    // milieu aurait décalé la dame et le roi de tous les jeux déjà
    // fabriqués. Il s'affiche en revanche à sa place, `slotFor()` suivant
    // l'ordre d'affichage et non celui des rangs.
    static constexpr int kKnightRank = 14;

    // `rank` va de 1 (l'as) à 13 (le roi), plus 14 pour le cavalier ; `suit`
    // de 0 à 3 dans l'ordre pique, cœur, carreau, trèfle, plus 4 pour les
    // atouts, dont le rang est le numéro, de 1 à 21. Chaque programme
    // convertit sa propre numérotation vers celle-ci.
    static int slotFor(int rank, int suit);
    static int rankOfSlot(int slot);
    static int suitOfSlot(int slot);
    static QString slotName(int slot);       // « roi de cœur », « atout 7 »
    static QString slotShort(int slot);      // « R♥ », « VII »
    static bool isTrumpSlot(int slot);
    // Le numéro d'un atout, de 1 à 21 ; zéro si la case n'en est pas un.
    static int trumpOfSlot(int slot);
    static int slotForTrump(int number);
    // Le numéro d'un atout en chiffres romains : « XXI ».
    static QString romanNumber(int number);

    // Le genre, et ce qu'il entraîne.
    DeckKind kind = DeckKind::Full;
    static QString kindName(DeckKind kind);       // « Jeu de 52 »
    static QString kindNote(DeckKind kind);       // la phrase qui l'explique
    static int slotCountOf(DeckKind kind);        // 53, 33 ou 78
    // Les cases de ce genre, dans l'ordre d'affichage : le joker ou l'excuse
    // en dernier, après les couleurs et les atouts.
    static QVector<int> slotsOf(DeckKind kind);
    static bool kindUses(DeckKind kind, int slot);

    int slotCount() const { return slotCountOf(kind); }
    bool usesSlot(int slot) const { return kindUses(kind, slot); }

    QString name;
    QString author;
    QString note;
    // La licence du jeu, facultative et en texte libre : « CC BY-SA 4.0 »,
    // « Domaine public », un nom de licence ou une adresse. Une liste à choix
    // imposerait un catalogue à tenir, et n'aurait pas la case qu'il faut le
    // jour où quelqu'un veut autre chose.
    QString licence;
    QSize artSize = QSize(0, 0);

    bool isEmpty() const;
    // Les faces posées dans les cases que le genre emploie. Une face rangée
    // hors du genre -- un cavalier dans un jeu de 52 -- n'est pas comptée.
    int filledFaces() const;
    bool complete() const;                   // toutes les cases du genre remplies
    // Largeur / hauteur d'une face, pour que l'affichage adopte la proportion
    // des cartes qu'on lui donne. Nul quand le jeu est vide.
    qreal aspect() const;

    // La taille que partage la plupart des faces, c'est-à-dire la taille
    // réelle d'une carte de ce jeu. Nulle quand il n'y a rien à mesurer.
    QSize measuredArtSize() const;

    // Recopie la taille mesurée dans `artSize`. Appelée dès que les faces
    // changent, et une fois à l'ouverture : c'est de ce champ que les
    // programmes de jeu tirent la proportion des cartes, et un jeu dont le
    // champ annonce 1022 × 980 alors que ses faces font 71 × 96 produit un
    // affichage de cartes carrées.
    void refreshArtSize();

    void setFace(int slot, const Artwork &art);
    void clearFace(int slot);
    bool hasFace(int slot) const;
    Artwork face(int slot) const;

    int backCount() const { return m_backs.size(); }
    QString backName(int at) const;
    Artwork back(int at) const;
    void addBack(const QString &name, const Artwork &art);
    // Remplace l'image d'un dos sans changer sa place dans la liste.
    void setBack(int at, const Artwork &art);
    void removeBack(int at);
    void renameBack(int at, const QString &name);

    // ----------------------------------------------------------------------
    // Les cases dessinées.
    //
    // Une case peut recevoir une carte que la cartothèque dessine elle-même
    // au lieu d'une image venue d'ailleurs. L'image produite est rangée comme
    // les autres -- c'est un PNG, et un programme de jeu n'a pas à savoir
    // d'où il vient -- mais la recette qui l'a produite est conservée à côté,
    // de sorte que l'atelier puisse en changer les couleurs plus tard.
    //
    // Les cases sont numérotées comme ailleurs dans cette classe : de 0 à 52
    // pour une face, et `backKey(n)` pour le n-ième dos.
    static int backKey(int at) { return -1 - at; }
    static bool isBackKey(int key) { return key < 0; }
    static int backOfKey(int key) { return key < 0 ? -1 - key : -1; }

    bool hasRecipe(int key) const;
    DrawnRecipe recipe(int key) const;
    // Les cases dessinées, dans l'ordre des faces puis des dos.
    QVector<int> drawnKeys() const;
    int drawnCount() const { return m_recipes.size(); }

    void setDrawnFace(int slot, const Artwork &art, const DrawnRecipe &how);
    void setDrawnBack(int at, const Artwork &art, const DrawnRecipe &how);
    void addDrawnBack(const QString &name, const Artwork &art, const DrawnRecipe &how);

    bool save(const QString &path, QString *why) const;
    bool load(const QString &path, QString *why);

    // Une face rendue à la taille demandée. Le résultat est mis en cache, car
    // une carte est redemandée à chaque réaffichage.
    QPixmap faceImage(int slot, const QSize &size) const;
    QPixmap backImage(int at, const QSize &size) const;
    void forgetDrawings() const;

    QString path() const { return m_path; }

    static QString extension() { return QStringLiteral("cartes"); }
    // Le dossier où les jeux installés sont recherchés.
    static QString installFolder();

private:
    QPixmap drawn(const Artwork &art, const QSize &size, quint64 key) const;

    QHash<int, Artwork> m_faces;
    QHash<int, DrawnRecipe> m_recipes;
    QVector<QPair<QString, Artwork>> m_backs;
    QString m_path;
    mutable QHash<quint64, QPixmap> m_drawn;
};
