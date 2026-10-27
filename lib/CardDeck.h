#pragma once

#include "CardSet.h"

#include <QString>
#include <QVector>

// Les jeux de cartes disponibles, et celui qui est choisi.
//
// La liste contient toujours le jeu que le programme dessine lui-même, puis
// les fichiers trouvés dans le dossier commun. Le dossier est partagé entre
// les programmes ; le choix, lui, est enregistré dans les réglages de
// chacun.

struct DeckEntry {
    QString id;          // « dessine », ou le nom du fichier sans extension
    QString name;
    QString author;
    QString note;
    QString licence;     // facultative, telle que le jeu la porte
    QString path;        // vide pour le jeu dessiné
    bool builtIn = false;
    DeckKind kind = DeckKind::Full;
    int faces = 0;
    int slotCount = 0;   // les cases que son genre emploie
    int backs = 0;
    bool complete = false;
};

class CardDecks {
public:
    static CardDecks &instance();

    // Relit le dossier : au démarrage, et après l'installation d'un jeu.
    void refresh();

    // Les jeux d'un genre donné. Un programme de rami n'offre que les jeux de
    // 52 : un tarot n'a pas les bonnes cartes, un jeu de 32 n'en a pas assez.
    QVector<DeckEntry> entriesOfKind(DeckKind kind) const;

    const QVector<DeckEntry> &entries() const { return m_entries; }
    const DeckEntry *entry(const QString &id) const;

    // `builtInId()` identifie le jeu dessiné par le programme, toujours
    // présent dans la liste.
    static QString builtInId();
    QString chosenId() const;
    void choose(const QString &id);
    bool usingBuiltIn() const;

    // Le jeu choisi, chargé à la demande. Renvoie nullptr quand le jeu
    // choisi est celui que le programme dessine lui-même ; c'est à cela que
    // le code de dessin reconnaît qu'il doit dessiner les cartes.
    const CardSet *current() const;

    int chosenBack() const;
    void chooseBack(int at);

    static QString folder();
    static bool ensureFolder();

private:
    CardDecks() = default;
    void loadChosen() const;

    QVector<DeckEntry> m_entries;
    mutable CardSet m_current;
    mutable QString m_loaded;      // l'identifiant du jeu chargé
    mutable bool m_tried = false;
};
