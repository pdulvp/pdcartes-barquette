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
//         qint8 rang (0 = joker, 1 = as … 13 = roi), qint8 couleur,
//         QByteArray type d'image (« png », « svg », « bmp », « jpg »),
//         QByteArray octets
//     qint32  nombre de dos      puis, pour chacun :
//         QString nom, QByteArray type d'image, QByteArray octets
//
// Les images sont stockées telles quelles : un PNG reste un PNG, un SVG reste
// un SVG.
//
// Après les dos vient un bloc facultatif, les recettes des cases dessinées :
//
//     QByteArray « recettes »    l'étiquette du bloc
//     qint32                     la version du bloc, 1 pour l'instant
//     qint32  nombre de recettes puis, pour chacune :
//         qint32 case, qint32 hauteur de rendu, puis huit QColor
//
// Le bloc est à la fin, et la version du format reste 1 : un programme écrit
// avant lui s'arrête aux dos et ignore ce qui suit. Un jeu fabriqué
// aujourd'hui s'ouvre donc dans une version déjà installée, laquelle y voit
// de simples images -- ce qu'elles sont.

struct Artwork {
    QByteArray kind;    // « png », « svg », « bmp », « jpg »
    QByteArray bytes;

    bool isEmpty() const { return bytes.isEmpty(); }
    bool isVector() const { return kind == "svg" || kind == "svgz"; }

    // Le type d'image, reconnu aux premiers octets plutôt qu'à l'extension.
    static QByteArray sniff(const QByteArray &bytes);
    static Artwork fromFile(const QString &path, QString *why = nullptr);
};

class CardSet {
public:
    // 53 emplacements : le joker, puis les 52 cartes.
    static constexpr int kSlots = 53;
    static constexpr int kJokerSlot = 0;

    // `rank` va de 1 (l'as) à 13 (le roi), `suit` de 0 à 3 dans l'ordre
    // pique, cœur, carreau, trèfle. Chaque programme convertit sa propre
    // numérotation vers celle-ci.
    static int slotFor(int rank, int suit);
    static int rankOfSlot(int slot);
    static int suitOfSlot(int slot);
    static QString slotName(int slot);       // « roi de cœur »
    static QString slotShort(int slot);      // « R♥ »

    QString name;
    QString author;
    QString note;
    QSize artSize = QSize(0, 0);

    bool isEmpty() const;
    int filledFaces() const;
    bool complete() const;                   // les 53 emplacements remplis
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
