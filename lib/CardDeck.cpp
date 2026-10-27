#include "CardDeck.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace {

// Le choix est enregistré dans les réglages du programme, et non dans le
// dossier commun : chaque programme peut employer un jeu différent.
const char *kChosenKey = "cartes/jeu";
const char *kBackKey = "cartes/dos";

} // namespace

CardDecks &CardDecks::instance() {
    static CardDecks only;
    return only;
}

QString CardDecks::builtInId() { return QStringLiteral("dessine"); }

QString CardDecks::folder() { return CardSet::installFolder(); }

bool CardDecks::ensureFolder() {
    const QString where = folder();
    if (QFileInfo::exists(where)) return true;
    return QDir().mkpath(where);
}

void CardDecks::refresh() {
    m_entries.clear();

    DeckEntry drawn;
    drawn.id = builtInId();
    drawn.name = QCoreApplication::translate("decks", "Le jeu dessiné");
    drawn.note = QCoreApplication::translate(
        "decks", "Celui que le programme dessine lui-même.");
    drawn.builtIn = true;
    drawn.complete = true;
    drawn.slotCount = CardSet::slotCountOf(DeckKind::Full);
    drawn.faces = drawn.slotCount;
    m_entries.append(drawn);

    QDir dir(folder());
    if (dir.exists()) {
        const QStringList pattern = {QStringLiteral("*.") + CardSet::extension()};
        const QFileInfoList files =
            dir.entryInfoList(pattern, QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo &file : files) {
            // Lu une fois pour connaître son contenu ; le jeu choisi est
            // rechargé plus tard, à la demande.
            CardSet set;
            QString why;
            if (!set.load(file.absoluteFilePath(), &why)) continue;

            DeckEntry entry;
            entry.id = file.completeBaseName();
            entry.name = set.name.trimmed().isEmpty() ? entry.id : set.name.trimmed();
            entry.author = set.author;
            entry.note = set.note;
            entry.path = file.absoluteFilePath();
            entry.kind = set.kind;
            entry.faces = set.filledFaces();
            entry.slotCount = set.slotCount();
            entry.backs = set.backCount();
            entry.complete = set.complete();
            m_entries.append(entry);
        }
    }

    // Le fichier chargé a pu changer : le cache est vidé.
    m_loaded.clear();
    m_tried = false;
    m_current = CardSet();
}

QVector<DeckEntry> CardDecks::entriesOfKind(DeckKind kind) const {
    QVector<DeckEntry> wanted;
    for (const DeckEntry &entry : m_entries) {
        if (entry.kind == kind) wanted.append(entry);
    }
    return wanted;
}

const DeckEntry *CardDecks::entry(const QString &id) const {
    for (const DeckEntry &entry : m_entries) {
        if (entry.id == id) return &entry;
    }
    return nullptr;
}

QString CardDecks::chosenId() const {
    QSettings settings;
    const QString kept = settings.value(QLatin1String(kChosenKey), builtInId()).toString();
    // Si le fichier du jeu choisi a disparu, on revient au jeu dessiné.
    if (kept != builtInId() && !entry(kept)) return builtInId();
    return kept;
}

void CardDecks::choose(const QString &id) {
    QSettings settings;
    settings.setValue(QLatin1String(kChosenKey), id);
    m_loaded.clear();
    m_tried = false;
    m_current = CardSet();
}

bool CardDecks::usingBuiltIn() const { return chosenId() == builtInId(); }

void CardDecks::loadChosen() const {
    const QString want = chosenId();
    if (m_tried && m_loaded == want) return;
    m_tried = true;
    m_loaded = want;
    m_current = CardSet();
    if (want == builtInId()) return;

    const DeckEntry *found = entry(want);
    if (!found || found->path.isEmpty()) return;
    QString why;
    if (!m_current.load(found->path, &why)) m_current = CardSet();
}

const CardSet *CardDecks::current() const {
    loadChosen();
    if (m_current.filledFaces() == 0) return nullptr;
    return &m_current;
}

int CardDecks::chosenBack() const {
    QSettings settings;
    const int at = settings.value(QLatin1String(kBackKey), 0).toInt();
    const CardSet *set = current();
    if (!set || set->backCount() == 0) return -1;
    return qBound(0, at, set->backCount() - 1);
}

void CardDecks::chooseBack(int at) {
    QSettings settings;
    settings.setValue(QLatin1String(kBackKey), qMax(0, at));
}
