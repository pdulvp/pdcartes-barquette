#pragma once

#include "CardArt.h"
#include "CardSet.h"
#include "ImageJoin.h"

#include <QMainWindow>
#include <QPoint>
#include <QString>

class CardStyleEditor;
class QPushButton;
class FaceGrid;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QScrollArea;
class QTabWidget;
class QToolButton;
class SourceTray;

// La fenêtre principale : les cases du jeu à gauche, et à droite deux onglets
// qui fournissent les cartes — les images ouvertes, ou le jeu que la
// cartothèque dessine elle-même. On sélectionne une case, on sélectionne une
// carte, et on place.
class AtelierWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit AtelierWindow(QWidget *parent = nullptr);

    // Ouvre un jeu au démarrage, depuis la ligne de commande.
    bool openPath(const QString &path);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *buildTopBar();
    QWidget *buildLeft();
    QWidget *buildRight();
    QWidget *buildTrayTab();
    QWidget *buildDrawnTab();
    QWidget *buildCardProperties(QWidget *parent);

    void startFresh();
    void chooseKind(DeckKind kind);
    void openFile();
    bool saveFile();
    bool saveFileAs();
    void installFile();
    void openHelp();
    void toggleDark();

    void importImages();
    void importFolder();
    void importDll();
    void importSet();
    void takeFiles(const QStringList &paths);
    void splitSheet();
    // Le menu du clic droit sur une image, et le collage qu'il propose.
    void openTrayMenu(int index, const QPoint &where);
    void joinWith(int other, JoinSide side);
    void guessMapping();

    void placeChosen();
    void placeInto(int target, int trayIndex);
    void clearTarget(int target);
    void addBackFromTray();

    // Le jeu dessiné : on le pose dans les cases, et on retouche après coup
    // les couleurs de ce qui y est posé.
    void placeDrawnInto(int target, const DrawnRecipe &how);
    // Une recette rendue à une case : l'image du panneau appartient à la
    // carte où elle a été posée, et non au style qu'on répand.
    DrawnRecipe recipeFor(int key, DrawnRecipe how) const;
    // La carte de cette case porte-t-elle un panneau, où une image puisse
    // venir ?
    bool cardCarriesPanel(int slot, const DrawnRecipe &how) const;
    void dropPanelArt();
    void placePanelArt();
    void fillWithDrawn(bool emptyOnly);
    void placeDrawnHere();
    void addDrawnBack();
    void spreadRecipe();
    void adoptRecipe();
    void updateCardProperties();

    void applyColours();
    void updateChrome();
    void noteChange();
    bool offerToSave();

    CardSet m_set;
    QString m_path;
    bool m_dirty = false;

    FaceGrid *m_grid = nullptr;
    SourceTray *m_tray = nullptr;
    QScrollArea *m_gridRoom = nullptr;
    QScrollArea *m_trayRoom = nullptr;

    QTabWidget *m_sources = nullptr;
    // Les réglages des cartes à poser, et ceux de la carte choisie.
    CardStyleEditor *m_drawnStyle = nullptr;
    CardStyleEditor *m_cardStyle = nullptr;
    QWidget *m_cardProps = nullptr;
    QPushButton *m_placeArt = nullptr;
    QPushButton *m_dropArt = nullptr;

    QWidget *m_topBar = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_tally = nullptr;
    QLabel *m_hint = nullptr;
    QLineEdit *m_name = nullptr;
    QLineEdit *m_author = nullptr;
    QLineEdit *m_note = nullptr;
    QComboBox *m_kind = nullptr;
    QLabel *m_kindNote = nullptr;
    QCheckBox *m_clearCorners = nullptr;

    QToolButton *m_newButton = nullptr;
    QToolButton *m_openButton = nullptr;
    QToolButton *m_saveButton = nullptr;
    QToolButton *m_installButton = nullptr;
    QToolButton *m_helpButton = nullptr;
    QToolButton *m_darkButton = nullptr;
};
