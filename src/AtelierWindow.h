#pragma once

#include "CardSet.h"
#include "ImageJoin.h"

#include <QMainWindow>
#include <QPoint>
#include <QString>

class FaceGrid;
class QCheckBox;
class QLabel;
class QLineEdit;
class QScrollArea;
class QToolButton;
class SourceTray;

// La fenêtre de l'atelier : les places du jeu à gauche, les images ouvertes à
// droite. On désigne une place, on désigne une image, on range.
class AtelierWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit AtelierWindow(QWidget *parent = nullptr);

    // Ouvrir un jeu au démarrage, depuis la ligne de commande.
    bool openPath(const QString &path);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *buildTopBar();
    QWidget *buildLeft();
    QWidget *buildRight();

    void startFresh();
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

    QWidget *m_topBar = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_tally = nullptr;
    QLabel *m_hint = nullptr;
    QLineEdit *m_name = nullptr;
    QLineEdit *m_author = nullptr;
    QLineEdit *m_note = nullptr;
    QCheckBox *m_clearCorners = nullptr;

    QToolButton *m_newButton = nullptr;
    QToolButton *m_openButton = nullptr;
    QToolButton *m_saveButton = nullptr;
    QToolButton *m_installButton = nullptr;
    QToolButton *m_helpButton = nullptr;
    QToolButton *m_darkButton = nullptr;
};
