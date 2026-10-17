#pragma once

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
//     qint32  largeur, hauteur   la taille naturelle d'une face
//     qint32  nombre de faces    puis, pour chacune :
//         qint8 rang (0 = joker, 1 = as … 13 = roi), qint8 couleur,
//         QByteArray genre (« png », « svg », « bmp », « jpg »),
//         QByteArray octets
//     qint32  nombre de dos      puis, pour chacun :
//         QString nom, QByteArray genre, QByteArray octets
//
// Les images sont stockées telles quelles : un PNG reste un PNG, un SVG reste
// un SVG.

struct Artwork {
    QByteArray kind;    // « png », « svg », « bmp », « jpg »
    QByteArray bytes;

    bool isEmpty() const { return bytes.isEmpty(); }
    bool isVector() const { return kind == "svg" || kind == "svgz"; }

    // Le genre d'image, reconnu aux premiers octets plutôt qu'à l'extension.
    static QByteArray sniff(const QByteArray &bytes);
    static Artwork fromFile(const QString &path, QString *why = nullptr);
};

class CardSet {
public:
    // 53 places : le joker, puis les 52 cartes.
    static constexpr int kSlots = 53;
    static constexpr int kJokerSlot = 0;

    // `rank` va de 1 (l'as) à 13 (le roi), `suit` de 0 à 3 dans l'ordre
    // pique, cœur, carreau, trèfle. Chaque programme traduit sa propre
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
    bool complete() const;                   // les 53 places sont remplies
    // Largeur / hauteur d'une face, pour que la table adopte la forme des
    // cartes qu'on lui donne. Nul quand le jeu est vide.
    qreal aspect() const;

    // La taille que portent la plupart des faces : celle qu'une carte de ce
    // jeu-là mesure vraiment. Nulle quand il n'y a rien à mesurer.
    QSize measuredArtSize() const;

    // Remet la taille mesurée dans `artSize`. Appelée dès que les faces
    // changent, et une fois à l'ouverture : c'est de ce champ que la table
    // tient la forme de ses cartes, et un jeu dont le champ dit 1022 × 980
    // quand ses faces font 71 × 96 fait une table de cartes carrées.
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

    bool save(const QString &path, QString *why) const;
    bool load(const QString &path, QString *why);

    // Une face rendue à la taille voulue. Le résultat est mis en cache : une
    // carte est redemandée à chaque repaint.
    QPixmap faceImage(int slot, const QSize &size) const;
    QPixmap backImage(int at, const QSize &size) const;
    void forgetDrawings() const;

    QString path() const { return m_path; }

    static QString extension() { return QStringLiteral("cartes"); }
    // Le dossier où les jeux installés sont attendus.
    static QString installFolder();

private:
    QPixmap drawn(const Artwork &art, const QSize &size, quint64 key) const;

    QHash<int, Artwork> m_faces;
    QVector<QPair<QString, Artwork>> m_backs;
    QString m_path;
    mutable QHash<quint64, QPixmap> m_drawn;
};
