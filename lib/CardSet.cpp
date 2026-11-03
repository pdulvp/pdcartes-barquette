#include "CardSet.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QStandardPaths>

namespace {

const char kMagic[] = "CARTES";
constexpr int kMagicSize = 6;
// Le format d'un jeu de 52 ou de 32 reste 1, et s'ouvre donc partout. Un
// tarot s'écrit en 2 : ses atouts sont rangés sous la couleur 4, qu'un
// lecteur ancien ramènerait dans les bornes 0-3 et poserait par-dessus les
// trèfles. Il faut qu'il refuse le fichier, et le format 2 le lui fait
// refuser avec un message juste.
constexpr quint16 kFormatVersion = 2;
constexpr quint16 kPlainVersion = 1;

// L'étiquette du bloc des recettes, et sa version.
//
// Le bloc est écrit après les dos, et la version du format reste 1 : un
// lecteur écrit avant lui s'arrête aux dos sans jamais regarder plus loin.
// L'étiquette est là pour le lecteur qui, lui, regarde : elle lui dit qu'il
// s'agit bien de recettes et non d'un bloc d'une autre sorte, ajouté un jour
// à la même place.
// L'étiquette du bloc du genre, et sa version.
const char kKindTag[] = "genre";
constexpr qint32 kKindVersion = 1;

// L'étiquette du bloc de la licence.
//
// Il est écrit en dernier, après les recettes. Un lecteur qui rencontre une
// étiquette qu'il ne connaît pas s'arrête, les blocs ne portant pas leur
// longueur : placée avant les recettes, la licence les ferait perdre à un
// lecteur écrit avant elle.
//
// La licence n'est pas dans l'en-tête parce que celui-ci est une suite de
// champs lue par position : l'y ajouter décalerait tout ce qui suit.
const char kLicenceTag[] = "licence";
constexpr qint32 kLicenceVersion = 1;

const char kRecipeTag[] = "recettes";
// Les versions du bloc se lisent en cascade, chacune n'ajoutant que ses
// champs à la fin de chaque recette :
//
//     1  les couleurs et la hauteur de rendu
//     2  le panneau des figures, qui peut être retiré, et la taille de
//        l'emblème
//     3  l'aspect de verre de l'emblème
//     4  la convention qui donne les lettres du coin
//     5  le panneau sous les symboles des cartes de deux à dix, et sa largeur
//        et sa hauteur
//     6  l'image posée dans le panneau, et sa place dedans
//     7  l'emblème et les symboles du milieu, qui peuvent s'effacer
//     8  le fond des lettres du coin : sa présence, sa couleur, et sa
//        largeur et sa hauteur
//
// Une recette plus ancienne garde donc les valeurs d'origine des champs
// qu'elle n'a pas : son panneau derrière les seules figures, à sa taille et de
// sa couleur, un emblème à sa taille, sans image, un emblème plat, les
// lettres françaises, son motif du milieu, et pas de fond sous les lettres
// du coin.
constexpr qint32 kRecipeVersion = 8;

// La taille d'une image, lue dans son en-tête : il n'est pas nécessaire de
// décoder l'image entière pour connaître ses dimensions. Nulle si le type
// d'image n'est pas lisible, par exemple un SVG en l'absence du greffon.
QSize sizeOfArtwork(const Artwork &art) {
    if (art.isEmpty()) return {};
    QBuffer buffer;
    buffer.setData(art.bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, art.kind);
    const QSize found = reader.size();
    return found.isValid() && !found.isEmpty() ? found : QSize();
}

// Les noms des rangs et des couleurs, redéfinis ici : la bibliothèque ne
// dépend du modèle de cartes d'aucun programme.
QString rankWord(int rank) {
    switch (rank) {
    case 0: return QCoreApplication::translate("cardset", "joker");
    case 1: return QCoreApplication::translate("cardset", "as");
    case 11: return QCoreApplication::translate("cardset", "valet");
    case 12: return QCoreApplication::translate("cardset", "dame");
    case 13: return QCoreApplication::translate("cardset", "roi");
    case CardSet::kKnightRank: return QCoreApplication::translate("cardset", "cavalier");
    default: break;
    }
    return QString::number(rank);
}

// L'ordre dans lequel les rangs d'une couleur se présentent : l'as, le deux
// au dix, le valet, le cavalier, la dame, le roi. Le cavalier porte le rang
// 14 et vient pourtant en douzième position ; c'est tout l'objet de cette
// table.
constexpr int kRankOrder[CardSet::kRanksInSuit] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 14, 12, 13};

int placeOfRank(int rank) {
    for (int at = 0; at < CardSet::kRanksInSuit; ++at) {
        if (kRankOrder[at] == rank) return at;
    }
    return 0;
}

// Les rangs d'un jeu de 32 : du sept à l'as, le dix et les trois figures
// compris.
bool shortDeckRank(int rank) {
    return rank == 1 || (rank >= 7 && rank <= 13);
}

QString rankMark(int rank) {
    switch (rank) {
    case 0: return QStringLiteral("★");
    case 1: return QCoreApplication::translate("cardset", "A");
    case 11: return QCoreApplication::translate("cardset", "V");
    case 12: return QCoreApplication::translate("cardset", "D");
    case 13: return QCoreApplication::translate("cardset", "R");
    case CardSet::kKnightRank: return QCoreApplication::translate("cardset", "C");
    default: break;
    }
    return QString::number(rank);
}

QString suitWord(int suit) {
    switch (suit) {
    case 0: return QCoreApplication::translate("cardset", "pique");
    case 1: return QCoreApplication::translate("cardset", "cœur");
    case 2: return QCoreApplication::translate("cardset", "carreau");
    case 3: return QCoreApplication::translate("cardset", "trèfle");
    default: break;
    }
    return QString();
}

QString suitMark(int suit) {
    switch (suit) {
    case 0: return QStringLiteral("♠");
    case 1: return QStringLiteral("♥");
    case 2: return QStringLiteral("♦");
    case 3: return QStringLiteral("♣");
    default: break;
    }
    return QString();
}

} // namespace

QByteArray Artwork::sniff(const QByteArray &bytes) {
    if (bytes.startsWith(QByteArray("\x89PNG\r\n\x1a\n", 8))) return "png";
    if (bytes.startsWith(QByteArray("\xff\xd8\xff", 3))) return "jpg";
    if (bytes.startsWith("BM")) return "bmp";
    if (bytes.startsWith("GIF8")) return "gif";
    // Un SVG commence par une déclaration XML, un commentaire ou une
    // marque d'ordre des octets : la balise est donc recherchée dans les
    // premiers octets.
    const QByteArray head = bytes.left(512).toLower();
    if (head.contains("<svg")) return "svg";
    if (bytes.startsWith(QByteArray("\x1f\x8b", 2))) return "svgz";
    return QByteArray();
}

Artwork Artwork::fromFile(const QString &path, QString *why) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (why) {
            *why = QCoreApplication::translate("cardset", "%1 ne s'ouvre pas.")
                       .arg(QFileInfo(path).fileName());
        }
        return {};
    }
    Artwork art;
    art.bytes = file.readAll();
    art.kind = sniff(art.bytes);
    if (art.kind.isEmpty()) {
        if (why) {
            *why = QCoreApplication::translate(
                       "cardset", "%1 n'est ni un PNG, ni un SVG, ni une image que l'on sache "
                                  "lire.")
                       .arg(QFileInfo(path).fileName());
        }
        return {};
    }
    return art;
}

// ---------------------------------------------------------------------------

int CardSet::slotFor(int rank, int suit) {
    if (suit == kTrumpSuit) return slotForTrump(rank);
    if (rank <= 0) return kJokerSlot;
    return 1 + qBound(0, suit, 3) * kRanksInSuit + placeOfRank(qBound(1, rank, kKnightRank));
}

int CardSet::rankOfSlot(int slot) {
    if (slot <= kJokerSlot || slot >= kSlots) return 0;
    if (isTrumpSlot(slot)) return trumpOfSlot(slot);
    return kRankOrder[(slot - 1) % kRanksInSuit];
}

int CardSet::suitOfSlot(int slot) {
    if (slot <= kJokerSlot || slot >= kSlots) return 0;
    if (isTrumpSlot(slot)) return kTrumpSuit;
    return (slot - 1) / kRanksInSuit;
}

bool CardSet::isTrumpSlot(int slot) {
    return slot > 4 * kRanksInSuit && slot < kSlots;
}

int CardSet::trumpOfSlot(int slot) {
    if (!isTrumpSlot(slot)) return 0;
    return slot - 4 * kRanksInSuit;
}

int CardSet::slotForTrump(int number) {
    if (number < 1 || number > kTrumpCount) return kJokerSlot;
    return 4 * kRanksInSuit + number;
}

QString CardSet::romanNumber(int number) {
    // Les atouts vont de I à XXI : quatre symboles suffisent, et la table
    // reste plus courte qu'une division répétée.
    static const struct {
        int value;
        const char *sign;
    } kSigns[] = {{10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"}};
    QString written;
    int left = qBound(0, number, 3999);
    for (const auto &sign : kSigns) {
        while (left >= sign.value) {
            written += QLatin1String(sign.sign);
            left -= sign.value;
        }
    }
    return written;
}

QString CardSet::slotName(int slot) {
    if (slot == kJokerSlot) {
        return QCoreApplication::translate("cardset", "le joker ou l'excuse");
    }
    if (isTrumpSlot(slot)) {
        return QCoreApplication::translate("cardset", "l'atout %1").arg(trumpOfSlot(slot));
    }
    const int rank = rankOfSlot(slot);
    QString spelled;
    if (rank == 1) {
        spelled = QCoreApplication::translate("cardset", "l'as");
    } else if (rank == 12) {
        spelled = QCoreApplication::translate("cardset", "la dame");
    } else {
        spelled = QCoreApplication::translate("cardset", "le %1").arg(rankWord(rank));
    }
    return QCoreApplication::translate("cardset", "%1 de %2")
        .arg(spelled, suitWord(suitOfSlot(slot)));
}

QString CardSet::slotShort(int slot) {
    if (slot == kJokerSlot) return QStringLiteral("★");
    if (isTrumpSlot(slot)) return romanNumber(trumpOfSlot(slot));
    return rankMark(rankOfSlot(slot)) + suitMark(suitOfSlot(slot));
}

// ---------------------------------------------------------------------------
// Les genres.

QString CardSet::kindName(DeckKind kind) {
    switch (kind) {
    case DeckKind::Short: return QCoreApplication::translate("cardset", "Jeu de 32");
    case DeckKind::Tarot: return QCoreApplication::translate("cardset", "Tarot");
    case DeckKind::Full: break;
    }
    return QCoreApplication::translate("cardset", "Jeu de 52");
}

QString CardSet::kindNote(DeckKind kind) {
    switch (kind) {
    case DeckKind::Short:
        return QCoreApplication::translate(
            "cardset", "Du sept à l'as dans quatre couleurs, et le joker : la belote, "
                       "la manille, le piquet.");
    case DeckKind::Tarot:
        return QCoreApplication::translate(
            "cardset", "Quatre couleurs de quatorze cartes, cavalier compris, les "
                       "vingt et un atouts et l'excuse.");
    case DeckKind::Full: break;
    }
    return QCoreApplication::translate(
        "cardset", "L'as au roi dans quatre couleurs, et le joker : le rami, la belote "
                   "à 52, la bataille.");
}

bool CardSet::kindUses(DeckKind kind, int slot) {
    if (slot < 0 || slot >= kSlots) return false;
    if (slot == kJokerSlot) return true;  // le joker, ou l'excuse du tarot
    if (isTrumpSlot(slot)) return kind == DeckKind::Tarot;
    const int rank = kRankOrder[(slot - 1) % kRanksInSuit];
    if (rank == kKnightRank) return kind == DeckKind::Tarot;
    if (kind == DeckKind::Short) return shortDeckRank(rank);
    return true;
}

int CardSet::slotCountOf(DeckKind kind) {
    switch (kind) {
    case DeckKind::Short: return 1 + 4 * 8;
    case DeckKind::Tarot: return kSlots;
    case DeckKind::Full: break;
    }
    return 1 + 4 * 13;
}

QVector<int> CardSet::slotsOf(DeckKind kind) {
    QVector<int> wanted;
    wanted.reserve(slotCountOf(kind));
    // Les couleurs d'abord, puis les atouts, et le joker ou l'excuse en
    // dernier : c'est l'ordre dans lequel la grille les présente.
    for (int slot = 1; slot < kSlots; ++slot) {
        if (kindUses(kind, slot)) wanted.append(slot);
    }
    wanted.append(kJokerSlot);
    return wanted;
}

bool CardSet::isEmpty() const { return m_faces.isEmpty() && m_backs.isEmpty(); }

int CardSet::filledFaces() const {
    int count = 0;
    for (int slot = 0; slot < kSlots; ++slot) {
        if (usesSlot(slot) && hasFace(slot)) ++count;
    }
    return count;
}

bool CardSet::complete() const { return filledFaces() == slotCount(); }

qreal CardSet::aspect() const {
    if (artSize.width() > 0 && artSize.height() > 0) {
        return qreal(artSize.width()) / artSize.height();
    }
    return 0.0;
}

// La taille que partagent le plus de faces, et non celle de la première
// reçue : une planche placée dans une case puis retirée ne doit pas continuer
// de déterminer la proportion de toutes les cartes du jeu.
QSize CardSet::measuredArtSize() const {
    // Cinquante-trois tailles au plus : un parcours linéaire les compte
    // aussi vite qu'une table de hachage, et sans avoir à hacher des QSize.
    QVector<QPair<QSize, int>> tally;
    auto note = [&tally](const QSize &size) {
        if (size.isNull()) return;
        for (auto &seen : tally) {
            if (seen.first == size) {
                ++seen.second;
                return;
            }
        }
        tally.append({size, 1});
    };

    for (int slot = 0; slot < kSlots; ++slot) {
        if (hasFace(slot)) note(sizeOfArtwork(face(slot)));
    }
    // Les dos ne sont comptés qu'en l'absence de faces : un dos est souvent
    // une image d'un autre format, et c'est la taille des faces qui
    // intéresse.
    if (tally.isEmpty()) {
        for (const auto &back : m_backs) note(sizeOfArtwork(back.second));
    }

    QSize most;
    int best = 0;
    for (const auto &seen : tally) {
        if (seen.second > best) {
            best = seen.second;
            most = seen.first;
        }
    }
    return most;
}

void CardSet::refreshArtSize() {
    const QSize measured = measuredArtSize();
    // S'il n'y a rien à mesurer — un jeu vide, ou des images qu'aucun
    // greffon installé ne sait lire — le champ est laissé tel quel.
    if (!measured.isNull()) artSize = measured;
}

void CardSet::setFace(int slot, const Artwork &art) {
    if (slot < 0 || slot >= kSlots) return;
    if (art.isEmpty()) {
        clearFace(slot);
        return;
    }
    m_faces.insert(slot, art);
    // L'image reçue n'est plus celle que la recette produisait.
    m_recipes.remove(slot);
    forgetDrawings();
    refreshArtSize();
}

void CardSet::clearFace(int slot) {
    m_faces.remove(slot);
    m_recipes.remove(slot);
    forgetDrawings();
    refreshArtSize();
}

bool CardSet::hasFace(int slot) const {
    const auto it = m_faces.constFind(slot);
    return it != m_faces.constEnd() && !it->isEmpty();
}

Artwork CardSet::face(int slot) const { return m_faces.value(slot); }

QString CardSet::backName(int at) const {
    if (at < 0 || at >= m_backs.size()) return QString();
    return m_backs.at(at).first;
}

Artwork CardSet::back(int at) const {
    if (at < 0 || at >= m_backs.size()) return {};
    return m_backs.at(at).second;
}

void CardSet::addBack(const QString &name, const Artwork &art) {
    if (art.isEmpty()) return;
    m_backs.append({name.trimmed().isEmpty()
                        ? QCoreApplication::translate("cardset", "Dos %1").arg(m_backs.size() + 1)
                        : name.trimmed(),
                    art});
    forgetDrawings();
    refreshArtSize();
}

void CardSet::setBack(int at, const Artwork &art) {
    if (at < 0 || at >= m_backs.size() || art.isEmpty()) return;
    m_backs[at].second = art;
    m_recipes.remove(backKey(at));
    forgetDrawings();
    refreshArtSize();
}

void CardSet::removeBack(int at) {
    if (at < 0 || at >= m_backs.size()) return;
    m_backs.remove(at);
    // Les dos suivants reculent d'un cran, et leurs recettes avec eux : la
    // clé d'une recette de dos est une position dans la liste, non un
    // identifiant.
    QHash<int, DrawnRecipe> moved;
    for (auto it = m_recipes.constBegin(); it != m_recipes.constEnd(); ++it) {
        if (!isBackKey(it.key())) {
            moved.insert(it.key(), it.value());
            continue;
        }
        const int was = backOfKey(it.key());
        if (was == at) continue;
        moved.insert(backKey(was > at ? was - 1 : was), it.value());
    }
    m_recipes = moved;
    forgetDrawings();
    refreshArtSize();
}

void CardSet::renameBack(int at, const QString &name) {
    if (at < 0 || at >= m_backs.size() || name.trimmed().isEmpty()) return;
    m_backs[at].first = name.trimmed();
}

// ---------------------------------------------------------------------------
// Les cases dessinées.

bool CardSet::hasRecipe(int key) const { return m_recipes.contains(key); }

DrawnRecipe CardSet::recipe(int key) const { return m_recipes.value(key); }

QVector<int> CardSet::drawnKeys() const {
    QVector<int> keys;
    keys.reserve(m_recipes.size());
    for (int slot = 0; slot < kSlots; ++slot) {
        if (m_recipes.contains(slot)) keys.append(slot);
    }
    for (int at = 0; at < m_backs.size(); ++at) {
        if (m_recipes.contains(backKey(at))) keys.append(backKey(at));
    }
    return keys;
}

void CardSet::setDrawnFace(int slot, const Artwork &art, const DrawnRecipe &how) {
    setFace(slot, art);
    // `setFace()` a retiré la recette : une image reçue de l'extérieur n'en a
    // pas. Celle-ci est posée après coup, et seulement si l'image a bien été
    // rangée.
    if (hasFace(slot)) m_recipes.insert(slot, how);
}

void CardSet::setDrawnBack(int at, const Artwork &art, const DrawnRecipe &how) {
    setBack(at, art);
    if (at >= 0 && at < m_backs.size() && !m_backs.at(at).second.isEmpty()) {
        m_recipes.insert(backKey(at), how);
    }
}

void CardSet::addDrawnBack(const QString &name, const Artwork &art, const DrawnRecipe &how) {
    const int was = m_backs.size();
    addBack(name, art);
    if (m_backs.size() > was) m_recipes.insert(backKey(was), how);
}

// ---------------------------------------------------------------------------

bool CardSet::save(const QString &path, QString *why) const {
    // Le fichier contient-il une carte que le format 1 ne sait pas désigner ?
    //
    // Un lecteur écrit avant les genres ramène les rangs et les couleurs dans
    // les bornes qu'il connaît : un cavalier, rang 14, y devient le roi de sa
    // couleur, et un atout, couleur 4, y devient un trèfle. Il ouvrirait donc
    // le fichier sans erreur et montrerait de fausses cartes. Le format 2 le
    // lui fait refuser.
    //
    // C'est le contenu qui décide, et non le genre déclaré : celui-ci dit
    // comment présenter le jeu, pas ce qu'il contient — un tarot ramené à un
    // jeu de 52 garde ses cavaliers et ses atouts.
    bool needsNewFormat = false;
    for (int slot = 0; slot < kSlots && !needsNewFormat; ++slot) {
        if (!hasFace(slot)) continue;
        needsNewFormat = isTrumpSlot(slot) || rankOfSlot(slot) == kKnightRank;
    }

    QByteArray body;
    {
        QDataStream out(&body, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        out.setByteOrder(QDataStream::LittleEndian);
        out << name << author << note;
        out << qint32(artSize.width()) << qint32(artSize.height());

        QVector<int> filled;
        for (int slot = 0; slot < kSlots; ++slot) {
            if (hasFace(slot)) filled.append(slot);
        }
        // Toutes les faces sont écrites, y compris celles que le genre
        // déclaré ne montre pas : un tarot ramené à un jeu de 52 garde ses
        // cavaliers et ses atouts, et les retrouve si on lui rend son genre.
        // C'est aussi pourquoi la version du format se déduit d'elles et non
        // du genre, un peu plus bas.
        out << qint32(filled.size());
        for (int slot : filled) {
            const Artwork art = face(slot);
            out << qint8(rankOfSlot(slot)) << qint8(suitOfSlot(slot)) << art.kind << art.bytes;
        }

        out << qint32(m_backs.size());
        for (const auto &back : m_backs) out << back.first << back.second.kind << back.second.bytes;

        // Le genre. Le bloc est omis pour un jeu de 52, qui est ce que vaut
        // un genre absent : un fichier ordinaire reste ainsi octet pour
        // octet celui qu'il était.
        if (kind != DeckKind::Full) {
            out << QByteArray(kKindTag) << kKindVersion << qint32(kind);
        }

        // Les recettes des cases dessinées. Le bloc est omis quand il n'y en
        // a aucune, ce qui laisse le fichier identique à ce qu'il était.
        const QVector<int> drawn = drawnKeys();
        if (!drawn.isEmpty()) {
            out << QByteArray(kRecipeTag) << kRecipeVersion << qint32(drawn.size());
            for (const int key : drawn) {
                const DrawnRecipe how = m_recipes.value(key);
                out << qint32(key) << qint32(how.height);
                out << how.style.paper << how.style.ink << how.style.red << how.style.panel
                    << how.style.cap << how.style.bell << how.style.back << how.style.backInk;
                out << how.style.panelled << qreal(how.style.emblemSize);
                out << how.style.glassy;
                out << qint32(how.style.letters);
                out << how.style.panelOnPips << qreal(how.style.panelWidth)
                    << qreal(how.style.panelHeight);
                // L'image du panneau est écrite telle qu'elle a été fournie,
                // et non le rendu qu'on en fait : un jeu rouvert doit pouvoir
                // la replacer, ce qu'une image déjà rognée ne permettrait
                // plus.
                out << how.panelArt.bytes << how.panelArt.kind << qreal(how.panelArt.offset.x())
                    << qreal(how.panelArt.offset.y()) << qreal(how.panelArt.scale);
                out << how.style.emblemShown << how.style.pipsShown;
                out << how.style.indexBackShown << how.style.indexBack
                    << qreal(how.style.indexWidth) << qreal(how.style.indexHeight);
            }
        }

        // La licence, en dernier. Le bloc est omis quand il n'y en a pas, ce
        // qui laisse le fichier identique à ce qu'il était.
        if (!licence.trimmed().isEmpty()) {
            out << QByteArray(kLicenceTag) << kLicenceVersion << licence;
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (why) {
            *why = QCoreApplication::translate("cardset", "%1 ne s'écrit pas : %2")
                       .arg(QFileInfo(path).fileName(), file.errorString());
        }
        return false;
    }
    file.write(kMagic, kMagicSize);
    QDataStream head(&file);
    head.setVersion(QDataStream::Qt_6_0);
    head.setByteOrder(QDataStream::LittleEndian);
    head << quint16(needsNewFormat ? kFormatVersion : kPlainVersion);
    // Les SVG et l'index ne sont pas compressés individuellement.
    head << qCompress(body, 9);
    file.close();
    return file.error() == QFileDevice::NoError;
}

bool CardSet::load(const QString &path, QString *why) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (why) {
            *why = QCoreApplication::translate("cardset", "%1 ne s'ouvre pas.")
                       .arg(QFileInfo(path).fileName());
        }
        return false;
    }
    if (file.read(kMagicSize) != QByteArray(kMagic, kMagicSize)) {
        if (why) {
            *why = QCoreApplication::translate("cardset", "%1 n'est pas un jeu de cartes.")
                       .arg(QFileInfo(path).fileName());
        }
        return false;
    }

    QDataStream head(&file);
    head.setVersion(QDataStream::Qt_6_0);
    head.setByteOrder(QDataStream::LittleEndian);
    quint16 version = 0;
    QByteArray packed;
    head >> version >> packed;
    if (head.status() != QDataStream::Ok) {
        if (why) {
            *why = QCoreApplication::translate("cardset", "%1 est tronqué.")
                       .arg(QFileInfo(path).fileName());
        }
        return false;
    }
    if (version > kFormatVersion) {
        if (why) {
            *why = QCoreApplication::translate(
                       "cardset", "%1 vient d'une version plus récente de l'atelier (format %2).")
                       .arg(QFileInfo(path).fileName())
                       .arg(version);
        }
        return false;
    }

    const QByteArray body = qUncompress(packed);
    if (body.isEmpty()) {
        if (why) {
            *why = QCoreApplication::translate("cardset", "%1 ne se décompresse pas.")
                       .arg(QFileInfo(path).fileName());
        }
        return false;
    }

    QDataStream in(body);
    in.setVersion(QDataStream::Qt_6_0);
    in.setByteOrder(QDataStream::LittleEndian);

    CardSet read;
    qint32 width = 0;
    qint32 height = 0;
    in >> read.name >> read.author >> read.note >> width >> height;
    read.artSize = QSize(width, height);

    qint32 faces = 0;
    in >> faces;
    if (faces < 0 || faces > kSlots) {
        if (why) *why = QCoreApplication::translate("cardset", "Le fichier est abîmé.");
        return false;
    }
    for (qint32 i = 0; i < faces; ++i) {
        qint8 rank = 0;
        qint8 suit = 0;
        Artwork art;
        in >> rank >> suit >> art.kind >> art.bytes;
        if (in.status() != QDataStream::Ok) break;
        read.m_faces.insert(slotFor(rank, suit), art);
    }

    qint32 backs = 0;
    in >> backs;
    if (backs > 0 && backs < 4096) {
        for (qint32 i = 0; i < backs; ++i) {
            QString backName;
            Artwork art;
            in >> backName >> art.kind >> art.bytes;
            if (in.status() != QDataStream::Ok) break;
            read.m_backs.append({backName, art});
        }
    }

    if (in.status() != QDataStream::Ok) {
        if (why) {
            *why = QCoreApplication::translate("cardset", "%1 est tronqué.")
                       .arg(QFileInfo(path).fileName());
        }
        return false;
    }

    // Les blocs de queue, s'il y en a. Leur absence est le cas ordinaire d'un
    // jeu fabriqué à partir d'images, et celui de tous les jeux écrits avant
    // eux.
    //
    // La boucle lit l'étiquette et la version de chaque bloc, puis traite
    // ceux qu'elle connaît. Un bloc inconnu arrête la lecture : les blocs ne
    // portant pas leur longueur, on ne saurait pas où reprendre.
    while (!in.atEnd()) {
        QByteArray tag;
        qint32 blockVersion = 0;
        in >> tag >> blockVersion;
        if (in.status() != QDataStream::Ok) break;

        if (tag == QByteArray(kKindTag)) {
            qint32 named = 0;
            in >> named;
            if (in.status() != QDataStream::Ok) break;
            if (named >= int(DeckKind::Full) && named <= int(DeckKind::Tarot)) {
                read.kind = DeckKind(named);
            }
            continue;
        }

        if (tag == QByteArray(kLicenceTag)) {
            QString said;
            in >> said;
            if (in.status() != QDataStream::Ok) break;
            read.licence = said;
            continue;
        }

        if (tag != QByteArray(kRecipeTag)) break;

        qint32 count = 0;
        in >> count;
        if (in.status() == QDataStream::Ok &&
            blockVersion <= kRecipeVersion && count > 0 && count <= kSlots + 4096) {
            for (qint32 i = 0; i < count; ++i) {
                qint32 key = 0;
                qint32 height = 0;
                DrawnRecipe how;
                in >> key >> height;
                in >> how.style.paper >> how.style.ink >> how.style.red >> how.style.panel >>
                    how.style.cap >> how.style.bell >> how.style.back >> how.style.backInk;
                if (blockVersion >= 2) {
                    qreal emblem = 1.0;
                    in >> how.style.panelled >> emblem;
                    how.style.emblemSize = emblem > 0.0 ? emblem : 1.0;
                }
                if (blockVersion >= 3) in >> how.style.glassy;
                if (blockVersion >= 4) {
                    qint32 letters = 0;
                    in >> letters;
                    if (letters >= 0 && letters < kRankLettersCount) {
                        how.style.letters = RankLetters(letters);
                    }
                }
                if (blockVersion >= 5) {
                    qreal wide = 1.0;
                    qreal tall = 1.0;
                    in >> how.style.panelOnPips >> wide >> tall;
                    how.style.panelWidth = wide > 0.0 ? wide : 1.0;
                    how.style.panelHeight = tall > 0.0 ? tall : 1.0;
                }
                if (blockVersion >= 6) {
                    qreal across = 0.0;
                    qreal down = 0.0;
                    qreal zoom = 1.0;
                    in >> how.panelArt.bytes >> how.panelArt.kind >> across >> down >> zoom;
                    how.panelArt.offset = QPointF(across, down);
                    how.panelArt.scale = zoom > 0.0 ? zoom : 1.0;
                    // Une image illisible — un SVG sans son greffon — laisse
                    // le panneau à sa couleur plutôt que de perdre la carte.
                    if (!how.panelArt.bytes.isEmpty()) how.panelArt.decode();
                }
                if (blockVersion >= 7) in >> how.style.emblemShown >> how.style.pipsShown;
                if (blockVersion >= 8) {
                    qreal wide = 1.0;
                    qreal tall = 1.0;
                    in >> how.style.indexBackShown >> how.style.indexBack >> wide >> tall;
                    how.style.indexWidth = wide > 0.0 ? wide : 1.0;
                    how.style.indexHeight = tall > 0.0 ? tall : 1.0;
                }
                if (in.status() != QDataStream::Ok) break;
                how.height = height > 0 ? height : kDrawnHeight;
                // Une recette dont la case n'a rien reçu serait sans objet.
                const bool there = isBackKey(key) ? backOfKey(key) < read.m_backs.size()
                                                  : read.m_faces.contains(key);
                if (there) read.m_recipes.insert(key, how);
            }
        }
        // Un bloc illisible n'invalide pas le fichier : les images, qui sont
        // l'essentiel, ont déjà été lues. La lecture s'arrête là, faute de
        // savoir où le bloc suivant commence.
        if (in.status() != QDataStream::Ok) {
            in.resetStatus();
            break;
        }
    }
    in.resetStatus();

    read.m_path = path;
    if (read.name.trimmed().isEmpty()) read.name = QFileInfo(path).completeBaseName();
    // La taille est recalculée sur les faces du fichier plutôt que reprise
    // telle quelle : les jeux écrits par les premières versions du programme
    // annoncent celle de la première image reçue, planche comprise. Un tel
    // fichier se corrige donc à l'ouverture, sans avoir à le refabriquer.
    read.refreshArtSize();
    *this = read;
    return true;
}

// ---------------------------------------------------------------------------

QPixmap CardSet::drawn(const Artwork &art, const QSize &size, quint64 key) const {
    if (art.isEmpty() || size.width() < 1 || size.height() < 1) return {};

    const quint64 full = key ^ (quint64(size.width()) << 20) ^ (quint64(size.height()) << 40);
    const auto kept = m_drawn.constFind(full);
    if (kept != m_drawn.constEnd()) return *kept;

    QBuffer buffer;
    buffer.setData(art.bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, art.kind);

    QImage image;
    if (art.isVector()) {
        // Rendu directement à la taille demandée, donc net.
        reader.setScaledSize(size);
        image = reader.read();
    } else {
        image = reader.read();
        if (!image.isNull()) {
            image = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    if (image.isNull()) return {};

    const QPixmap made = QPixmap::fromImage(image);
    if (m_drawn.size() > 512) m_drawn.clear();
    m_drawn.insert(full, made);
    return made;
}

QPixmap CardSet::faceImage(int slot, const QSize &size) const {
    if (!hasFace(slot)) return {};
    return drawn(face(slot), size, quint64(slot) + 1);
}

QPixmap CardSet::backImage(int at, const QSize &size) const {
    if (at < 0 || at >= m_backs.size()) return {};
    return drawn(m_backs.at(at).second, size, quint64(at) + 1000);
}

void CardSet::forgetDrawings() const { m_drawn.clear(); }

QString CardSet::installFolder() {
    // Le dossier de réglages du programme, et « cartes » dedans : les jeux
    // installés appartiennent au programme qui les lit, chacun ayant les
    // siens. C'est le seul dossier où le programme écrit.
    const QString root =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(root).filePath(QStringLiteral("cartes"));
}

QStringList CardSet::searchFolders() {
    QStringList folders;
    // Un même dossier peut être désigné deux fois -- le dossier voisin de
    // l'exécutable est parfois l'un des dossiers de données -- et un jeu y
    // serait alors lu deux fois.
    const auto keep = [&folders](const QString &path) {
        if (path.isEmpty()) return;
        const QString clean = QDir::cleanPath(path);
        if (!folders.contains(clean)) folders.append(clean);
    };

    keep(installFolder());

    // Les jeux livrés avec le programme, que son installation dépose dans un
    // dossier de données du système : « /usr/share/pdcartes-rami/cartes »
    // pour un paquet installé sous « /usr ». Le programme n'y écrit pas, et
    // un utilisateur ordinaire n'en a pas le droit.
    const QString app = QCoreApplication::applicationName();
    if (!app.isEmpty()) {
        const QStringList roots =
            QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
        for (const QString &root : roots) {
            keep(QDir(root).filePath(app + QStringLiteral("/cartes")));
        }
    }

    // Et le dossier voisin de l'exécutable, dont se sert l'archive Windows :
    // on la décompresse où l'on veut, et les jeux qu'elle emporte voyagent
    // avec le programme plutôt que dans un dossier du système.
    keep(QDir(QCoreApplication::applicationDirPath())
             .filePath(QStringLiteral("cartes")));

    return folders;
}
