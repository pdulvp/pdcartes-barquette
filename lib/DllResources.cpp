#include "DllResources.h"
#include "CardSet.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>

namespace {

// Lecture bornée : toute lecture hors du tampon met `ok()` à faux au lieu de
// déborder. Un fichier abîmé fait échouer la lecture, pas le programme.
class Peek {
public:
    explicit Peek(const QByteArray &bytes) : m_bytes(bytes) {}

    bool ok() const { return m_ok; }
    qint64 size() const { return m_bytes.size(); }

    quint8 u8(qint64 at) { return quint8(byteAt(at)); }

    quint16 u16(qint64 at) {
        return quint16(quint8(byteAt(at))) | (quint16(quint8(byteAt(at + 1))) << 8);
    }

    quint32 u32(qint64 at) {
        return quint32(quint8(byteAt(at))) | (quint32(quint8(byteAt(at + 1))) << 8) |
               (quint32(quint8(byteAt(at + 2))) << 16) | (quint32(quint8(byteAt(at + 3))) << 24);
    }

    QByteArray slice(qint64 at, qint64 length) {
        if (at < 0 || length < 0 || at + length > m_bytes.size()) {
            m_ok = false;
            return {};
        }
        return m_bytes.mid(int(at), int(length));
    }

private:
    char byteAt(qint64 at) {
        if (at < 0 || at >= m_bytes.size()) {
            m_ok = false;
            return 0;
        }
        return m_bytes.at(int(at));
    }

    QByteArray m_bytes;
    bool m_ok = true;
};

// Les types de ressources de Windows dont on veut quelque chose.
constexpr quint16 kRtBitmap = 2;

QString guessKind(const QByteArray &bytes) {
    const QByteArray kind = Artwork::sniff(bytes);
    return QString::fromLatin1(kind);
}

// Un bitmap de ressource n'a pas les 14 octets d'en-tête de fichier : on les
// remet pour obtenir un .bmp lisible.
QByteArray wrapDib(const QByteArray &dib) {
    if (dib.size() < 16) return {};
    Peek peek(dib);
    const quint32 headerSize = peek.u32(0);
    if (!peek.ok()) return {};

    quint32 bitCount = 0;
    quint32 usedColours = 0;
    quint32 paletteEntry = 0;

    if (headerSize == 12) {
        // BITMAPCOREHEADER : dimensions sur 2 octets, palette en triplets.
        bitCount = peek.u16(10);
        paletteEntry = 3;
    } else if (headerSize >= 40) {
        bitCount = peek.u16(14);
        usedColours = peek.u32(32);
        paletteEntry = 4;
    } else {
        return {};
    }
    if (!peek.ok()) return {};

    quint32 colours = usedColours;
    if (colours == 0 && bitCount <= 8) colours = 1u << bitCount;
    if (bitCount > 8) colours = usedColours;

    const quint32 offBits = 14 + headerSize + colours * paletteEntry;
    if (offBits > quint32(dib.size()) + 14) return {};

    QByteArray file;
    file.reserve(14 + dib.size());
    file.append("BM", 2);
    const quint32 total = 14 + quint32(dib.size());
    for (int i = 0; i < 4; ++i) file.append(char((total >> (8 * i)) & 0xff));
    file.append(4, '\0');   // les deux champs réservés
    for (int i = 0; i < 4; ++i) file.append(char((offBits >> (8 * i)) & 0xff));
    file.append(dib);
    return file;
}

void measure(DllImage *image) {
    QBuffer buffer;
    buffer.setData(image->bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, image->kind);
    const QSize found = reader.size();
    if (!found.isValid()) return;
    image->width = found.width();
    image->height = found.height();
}

void keep(DllReading *reading, const QString &label, int id, const QByteArray &raw, bool isDib) {
    DllImage image;
    image.label = label;
    image.id = id;

    if (isDib) {
        const QByteArray file = wrapDib(raw);
        if (file.isEmpty()) {
            ++reading->skipped;
            return;
        }
        image.bytes = file;
        image.kind = "bmp";
    } else {
        const QString kind = guessKind(raw);
        if (kind.isEmpty()) {
            ++reading->skipped;
            return;
        }
        image.bytes = raw;
        image.kind = kind.toLatin1();
    }

    measure(&image);
    if (image.width <= 0 || image.height <= 0) {
        ++reading->skipped;
        return;
    }
    reading->images.append(image);
}

// --- NE : les bibliothèques 16 bits ---------------------------------------

bool readNe(Peek &peek, qint64 neAt, DllReading *reading, QString *why) {
    reading->container = QStringLiteral("NE");

    const quint16 tableOffset = peek.u16(neAt + 0x24);
    if (!peek.ok() || tableOffset == 0) {
        if (why) {
            *why = QCoreApplication::translate("dll", "Cette bibliothèque n'a pas de "
                                                      "ressources.");
        }
        return false;
    }
    const qint64 table = neAt + tableOffset;
    const quint16 shift = peek.u16(table);
    if (!peek.ok() || shift > 16) {
        if (why) *why = QCoreApplication::translate("dll", "La table des ressources est abîmée.");
        return false;
    }

    qint64 at = table + 2;
    for (int guard = 0; guard < 256; ++guard) {
        const quint16 typeId = peek.u16(at);
        if (!peek.ok()) break;
        if (typeId == 0) break;
        const quint16 count = peek.u16(at + 2);
        at += 8;
        if (count > 4096) break;

        const bool numbered = (typeId & 0x8000) != 0;
        const quint16 type = typeId & 0x7fff;
        for (quint16 i = 0; i < count; ++i) {
            const qint64 offset = qint64(peek.u16(at)) << shift;
            const qint64 length = qint64(peek.u16(at + 2)) << shift;
            const quint16 nameId = peek.u16(at + 6);
            at += 12;
            if (!peek.ok()) break;
            if (!numbered || type != kRtBitmap) continue;

            const int id = (nameId & 0x8000) ? int(nameId & 0x7fff) : -1;
            const QByteArray raw = peek.slice(offset, length);
            if (raw.isEmpty()) continue;
            keep(reading, QCoreApplication::translate("dll", "bitmap %1").arg(id), id, raw, true);
        }
    }

    if (reading->images.isEmpty() && why) {
        *why = QCoreApplication::translate("dll", "Aucune image dans cette bibliothèque.");
    }
    return !reading->images.isEmpty();
}

// --- PE : les bibliothèques 32 et 64 bits ----------------------------------

struct Section {
    quint32 virtualAddress = 0;
    quint32 rawSize = 0;
    quint32 rawPointer = 0;
};

qint64 fileOffsetFor(const QVector<Section> &sections, quint32 rva) {
    for (const Section &section : sections) {
        if (rva >= section.virtualAddress && rva < section.virtualAddress + section.rawSize) {
            return qint64(section.rawPointer) + (rva - section.virtualAddress);
        }
    }
    return -1;
}

QString resourceName(Peek &peek, qint64 base, quint32 nameField) {
    if (!(nameField & 0x80000000u)) return QString();
    const qint64 at = base + (nameField & 0x7fffffffu);
    const quint16 length = peek.u16(at);
    if (!peek.ok() || length > 1024) return QString();
    QString name;
    name.reserve(length);
    for (quint16 i = 0; i < length; ++i) name.append(QChar(peek.u16(at + 2 + i * 2)));
    return name;
}

void walkResources(Peek &peek, qint64 base, qint64 at, const QVector<Section> &sections,
                   int depth, quint16 type, const QString &trail, DllReading *reading) {
    if (depth > 3) return;
    const quint16 named = peek.u16(at + 12);
    const quint16 numbered = peek.u16(at + 14);
    if (!peek.ok()) return;
    const int total = int(named) + int(numbered);
    if (total > 8192) return;

    for (int i = 0; i < total; ++i) {
        const qint64 entry = at + 16 + qint64(i) * 8;
        const quint32 nameField = peek.u32(entry);
        const quint32 dataField = peek.u32(entry + 4);
        if (!peek.ok()) return;

        const bool hasName = (nameField & 0x80000000u) != 0;
        const QString name = hasName ? resourceName(peek, base, nameField) : QString();
        const int id = hasName ? -1 : int(nameField & 0x7fffffffu);

        if (dataField & 0x80000000u) {
            const quint16 nextType = depth == 0 ? quint16(hasName ? 0xffff : id) : type;
            const QString nextTrail = depth == 1 ? (hasName ? name : QString::number(id)) : trail;
            walkResources(peek, base, base + (dataField & 0x7fffffffu), sections, depth + 1,
                          nextType, nextTrail, reading);
            continue;
        }

        // Une feuille : adresse et taille des octets.
        const qint64 leaf = base + dataField;
        const quint32 dataRva = peek.u32(leaf);
        const quint32 dataSize = peek.u32(leaf + 4);
        if (!peek.ok() || dataSize == 0 || dataSize > 64u * 1024 * 1024) continue;
        const qint64 where = fileOffsetFor(sections, dataRva);
        if (where < 0) continue;
        const QByteArray raw = peek.slice(where, dataSize);
        if (raw.isEmpty()) continue;

        const bool isDib = type == kRtBitmap;
        QString label = trail;
        if (label.isEmpty()) label = QString::number(id);
        if (isDib) {
            label = QCoreApplication::translate("dll", "bitmap %1").arg(label);
        }
        bool numericTrail = false;
        const int trailId = trail.toInt(&numericTrail);
        keep(reading, label, numericTrail ? trailId : -1, raw, isDib);
    }
}

bool readPe(Peek &peek, qint64 peAt, DllReading *reading, QString *why) {
    const qint64 coff = peAt + 4;
    const quint16 sectionCount = peek.u16(coff + 2);
    const quint16 optionalSize = peek.u16(coff + 16);
    const qint64 optional = coff + 20;
    const quint16 magic = peek.u16(optional);
    if (!peek.ok() || sectionCount > 96) {
        if (why) *why = QCoreApplication::translate("dll", "L'en-tête est abîmé.");
        return false;
    }

    qint64 directories = 0;
    if (magic == 0x10b) {
        reading->container = QStringLiteral("PE32");
        directories = optional + 96;
    } else if (magic == 0x20b) {
        reading->container = QStringLiteral("PE32+");
        directories = optional + 112;
    } else {
        if (why) *why = QCoreApplication::translate("dll", "Format d'en-tête inconnu.");
        return false;
    }

    // La deuxième entrée de la table des répertoires : les ressources.
    const quint32 resourceRva = peek.u32(directories + 2 * 8);
    if (!peek.ok() || resourceRva == 0) {
        if (why) {
            *why = QCoreApplication::translate("dll", "Cette bibliothèque n'a pas de "
                                                      "ressources.");
        }
        return false;
    }

    QVector<Section> sections;
    const qint64 sectionTable = optional + optionalSize;
    for (quint16 i = 0; i < sectionCount; ++i) {
        const qint64 at = sectionTable + qint64(i) * 40;
        Section section;
        section.virtualAddress = peek.u32(at + 12);
        section.rawSize = peek.u32(at + 16);
        section.rawPointer = peek.u32(at + 20);
        if (!peek.ok()) break;
        sections.append(section);
    }

    const qint64 base = fileOffsetFor(sections, resourceRva);
    if (base < 0) {
        if (why) *why = QCoreApplication::translate("dll", "La table des ressources est hors du "
                                                           "fichier.");
        return false;
    }

    walkResources(peek, base, base, sections, 0, 0, QString(), reading);
    if (reading->images.isEmpty() && why) {
        *why = QCoreApplication::translate("dll", "Aucune image dans cette bibliothèque.");
    }
    return !reading->images.isEmpty();
}

} // namespace

DllReading readDllImages(const QString &path, QString *why) {
    DllReading reading;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (why) {
            *why = QCoreApplication::translate("dll", "%1 ne s'ouvre pas.")
                       .arg(QFileInfo(path).fileName());
        }
        return reading;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    Peek peek(bytes);
    if (peek.size() < 0x40 || peek.u16(0) != 0x5a4d) {
        if (why) {
            *why = QCoreApplication::translate("dll", "%1 n'est pas une bibliothèque Windows.")
                       .arg(QFileInfo(path).fileName());
        }
        return reading;
    }

    const quint32 headerAt = peek.u32(0x3c);
    if (!peek.ok() || headerAt >= quint32(peek.size())) {
        if (why) *why = QCoreApplication::translate("dll", "L'en-tête est abîmé.");
        return reading;
    }

    const quint16 signature = peek.u16(headerAt);
    if (signature == 0x4550) {             // « PE »
        readPe(peek, headerAt, &reading, why);
    } else if (signature == 0x454e) {      // « NE »
        readNe(peek, headerAt, &reading, why);
    } else if (why) {
        *why = QCoreApplication::translate("dll", "%1 n'est ni une bibliothèque 16 bits ni une "
                                                  "bibliothèque 32 bits.")
                   .arg(QFileInfo(path).fileName());
    }

    if (reading.skipped > 0) {
        reading.note = QCoreApplication::translate(
                           "dll", "%1 ressources laissées de côté : ce ne sont pas des images, "
                                  "ou bien elles sont enfermées dans une archive.")
                           .arg(reading.skipped);
    }
    return reading;
}

int classicCardSlotForResource(int resourceId) {
    if (resourceId < 1 || resourceId > 52) return -1;
    // Ordre des DLL : trèfle, carreau, cœur, pique. Ordre du format :
    // pique, cœur, carreau, trèfle.
    static const int kSuitOrder[4] = {3, 2, 1, 0};
    const int block = (resourceId - 1) / 13;
    const int rank = (resourceId - 1) % 13 + 1;
    return CardSet::slotFor(rank, kSuitOrder[block]);
}

bool classicResourceIsBack(int resourceId) {
    // Les douze dos seulement : au-delà viennent les images de l'ancienne
    // boîte de dialogue (croix, rond, rectangle vide).
    return resourceId >= 53 && resourceId <= 64;
}

// « SHEETS\HIGHLIGHTSHEET1X22.JPG » devient « HIGHLIGHTSHEET1X22 ». Les
// antislash sont retournés d'abord : un nom de ressource vient de Windows, et
// QFileInfo ne les tient pas pour des séparateurs ici.
QString imageStem(const QString &label) {
    QString name = label;
    name.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QFileInfo(name).completeBaseName();
}
