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
constexpr quint16 kFormatVersion = 1;

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
    default: break;
    }
    return QString::number(rank);
}

QString rankMark(int rank) {
    switch (rank) {
    case 0: return QStringLiteral("★");
    case 1: return QCoreApplication::translate("cardset", "A");
    case 11: return QCoreApplication::translate("cardset", "V");
    case 12: return QCoreApplication::translate("cardset", "D");
    case 13: return QCoreApplication::translate("cardset", "R");
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
    if (rank <= 0) return kJokerSlot;
    return 1 + qBound(0, suit, 3) * 13 + (qBound(1, rank, 13) - 1);
}

int CardSet::rankOfSlot(int slot) {
    if (slot <= kJokerSlot || slot >= kSlots) return 0;
    return (slot - 1) % 13 + 1;
}

int CardSet::suitOfSlot(int slot) {
    if (slot <= kJokerSlot || slot >= kSlots) return 0;
    return (slot - 1) / 13;
}

QString CardSet::slotName(int slot) {
    if (slot == kJokerSlot) return QCoreApplication::translate("cardset", "le joker");
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
    return rankMark(rankOfSlot(slot)) + suitMark(suitOfSlot(slot));
}

bool CardSet::isEmpty() const { return m_faces.isEmpty() && m_backs.isEmpty(); }

int CardSet::filledFaces() const {
    int count = 0;
    for (int slot = 0; slot < kSlots; ++slot) {
        if (hasFace(slot)) ++count;
    }
    return count;
}

bool CardSet::complete() const { return filledFaces() == kSlots; }

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
    forgetDrawings();
    refreshArtSize();
}

void CardSet::clearFace(int slot) {
    m_faces.remove(slot);
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
    forgetDrawings();
    refreshArtSize();
}

void CardSet::removeBack(int at) {
    if (at < 0 || at >= m_backs.size()) return;
    m_backs.remove(at);
    forgetDrawings();
    refreshArtSize();
}

void CardSet::renameBack(int at, const QString &name) {
    if (at < 0 || at >= m_backs.size() || name.trimmed().isEmpty()) return;
    m_backs[at].first = name.trimmed();
}

// ---------------------------------------------------------------------------

bool CardSet::save(const QString &path, QString *why) const {
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
        out << qint32(filled.size());
        for (int slot : filled) {
            const Artwork art = face(slot);
            out << qint8(rankOfSlot(slot)) << qint8(suitOfSlot(slot)) << art.kind << art.bytes;
        }

        out << qint32(m_backs.size());
        for (const auto &back : m_backs) out << back.first << back.second.kind << back.second.bytes;
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
    head << kFormatVersion;
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
    // Un dossier unique, partagé par tous les programmes qui lisent ce
    // format ; chacun garde de son côté le jeu qu'il a choisi.
    const QString root =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(root).filePath(QStringLiteral("cartes"));
}
