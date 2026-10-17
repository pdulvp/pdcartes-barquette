#pragma once

#include "CardSet.h"

#include <QString>
#include <QVector>

// Les jeux de cartes disponibles, et celui qui est choisi.
//
// La liste contient toujours le jeu que le programme dessine lui-même, puis
// les fichiers trouvés dans le dossier commun. Le dossier est partagé ; le
// choix est rangé dans les réglages de chaque programme.

struct DeckEntry {
    QString id;          // « dessine », ou le nom du fichier sans extension
    QString name;
    QString author;
    QString note;
    QString path;        // vide pour le jeu dessiné
    bool builtIn = false;
    int faces = 0;
    int backs = 0;
    bool complete = false;
};

class CardDecks {
public:
    static CardDecks &instance();

    // Relit le dossier : au démarrage, et quand un jeu vient d'être installé.
    void refresh();

    const QVector<DeckEntry> &entries() const { return m_entries; }
    const DeckEntry *entry(const QString &id) const;

    // `builtInId()` est le jeu dessiné par le programme, toujours présent.
    static QString builtInId();
    QString chosenId() const;
    void choose(const QString &id);
    bool usingBuiltIn() const;

    // Le jeu choisi, chargé à la demande. Nul quand c'est le jeu dessiné :
    // c'est ce que le code de dessin teste.
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
