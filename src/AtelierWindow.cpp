#include "AtelierWindow.h"

#include "CardStyleEditor.h"
#include "DialogChrome.h"
#include "DllResources.h"
#include "DrawnDeck.h"
#include "FaceGrid.h"
#include "HelpDialog.h"
#include "PlaceArtDialog.h"
#include "ImageJoin.h"
#include "Palette.h"
#include "SourceTray.h"
#include "SplitDialog.h"

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QQueue>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kTopBarHeight = 50;

QToolButton *barButton(QWidget *parent, const QString &text, const QStringList &iconNames,
                       const QString &tip) {
    auto *button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(tip);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRaise(true);
    button->setIconSize(QSize(18, 18));
    button->setProperty("iconNames", iconNames);
    return button;
}

QPushButton *panelButton(QWidget *parent, const QString &text, const QString &tip) {
    auto *button = new QPushButton(text, parent);
    button->setToolTip(tip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

void inkLabel(QLabel *label, const QColor &colour) {
    const QString rule = QStringLiteral("color: %1;").arg(colour.name());
    if (label->styleSheet() == rule) return;
    label->setStyleSheet(rule);
}

// Efface la couleur de fond autour des coins arrondis, par propagation depuis
// les quatre coins. Une propagation, et non « tous les pixels de cette
// couleur » : la couleur du coin se retrouve aussi au milieu de la carte.
QByteArray clearCorners(const QByteArray &bytes, const QByteArray &kind) {
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, kind);
    QImage image = reader.read();
    if (image.isNull()) return bytes;

    image = image.convertToFormat(QImage::Format_ARGB32);
    const int w = image.width();
    const int h = image.height();
    if (w < 3 || h < 3) return bytes;

    QVector<bool> seen(size_t(w) * h, false);
    QQueue<QPoint> todo;
    const QPoint corners[4] = {QPoint(0, 0), QPoint(w - 1, 0), QPoint(0, h - 1),
                               QPoint(w - 1, h - 1)};

    auto near = [](QRgb a, QRgb b) {
        return std::abs(qRed(a) - qRed(b)) <= 12 && std::abs(qGreen(a) - qGreen(b)) <= 12 &&
               std::abs(qBlue(a) - qBlue(b)) <= 12;
    };

    for (const QPoint &corner : corners) {
        const QRgb wanted = image.pixel(corner);
        if (qAlpha(wanted) == 0) continue;
        todo.enqueue(corner);
        while (!todo.isEmpty()) {
            const QPoint at = todo.dequeue();
            if (at.x() < 0 || at.y() < 0 || at.x() >= w || at.y() >= h) continue;
            const size_t index = size_t(at.y()) * w + at.x();
            if (seen[index]) continue;
            if (!near(image.pixel(at), wanted)) continue;
            seen[index] = true;
            image.setPixel(at, qRgba(0, 0, 0, 0));
            todo.enqueue(QPoint(at.x() + 1, at.y()));
            todo.enqueue(QPoint(at.x() - 1, at.y()));
            todo.enqueue(QPoint(at.x(), at.y() + 1));
            todo.enqueue(QPoint(at.x(), at.y() - 1));
        }
    }

    QByteArray made;
    QBuffer out(&made);
    out.open(QIODevice::WriteOnly);
    if (!image.save(&out, "PNG")) return bytes;
    return made;
}

} // namespace

AtelierWindow::AtelierWindow(QWidget *parent) : QMainWindow(parent) {
    QSettings settings;
    setDarkMode(settings.value(QStringLiteral("look/sombre"), false).toBool());
    setCurrentPalette(settings.value(QStringLiteral("look/teinte"), 262).toInt());

    setWindowTitle(tr("Atelier des cartes"));

    auto *centre = new QWidget(this);
    auto *rows = new QVBoxLayout(centre);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(0);
    rows->addWidget(buildTopBar());

    auto *across = new QHBoxLayout;
    across->setContentsMargins(12, 12, 12, 12);
    across->setSpacing(12);
    across->addWidget(buildLeft(), 3);
    across->addWidget(buildRight(), 2);
    rows->addLayout(across, 1);

    // La section des propriétés passe sous les deux colonnes, et non dans
    // celle de gauche.
    //
    // Elle décrit ce qui est choisi ailleurs : sa place est celle d'un
    // bandeau d'inspection, en bordure basse de la fenêtre. Et sa largeur
    // minimale -- un aperçu, trois parties de champs et deux boutons -- se
    // compare ainsi à la fenêtre entière, au lieu d'être multipliée par cinq
    // tiers en traversant la colonne de gauche. Posée dans cette colonne,
    // elle faisait grandir la fenêtre de trois cents pixels au moment où
    // elle apparaissait.
    rows->addWidget(buildCardProperties(centre));
    setCentralWidget(centre);

    startFresh();
    applyColours();
    resize(1300, 880);
}

QWidget *AtelierWindow::buildTopBar() {
    m_topBar = new QWidget(this);
    m_topBar->setObjectName(QStringLiteral("topBar"));
    m_topBar->setFixedHeight(kTopBarHeight);

    m_title = new QLabel(tr("Atelier"), m_topBar);
    QFont name = m_title->font();
    name.setBold(true);
    if (name.pointSizeF() > 0) name.setPointSizeF(name.pointSizeF() * 1.22);
    name.setLetterSpacing(QFont::PercentageSpacing, 106);
    m_title->setFont(name);

    m_subtitle = new QLabel(m_topBar);
    m_subtitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    m_tally = new QLabel(m_topBar);
    QFont counter = m_tally->font();
    counter.setBold(true);
    m_tally->setFont(counter);

    m_newButton = barButton(m_topBar, tr("Nouveau"),
                            {QStringLiteral("document-new-symbolic"),
                             QStringLiteral("document-new")},
                            tr("Repartir d'un jeu vide"));
    m_openButton = barButton(m_topBar, tr("Ouvrir"),
                             {QStringLiteral("document-open-symbolic"),
                              QStringLiteral("document-open")},
                             tr("Ouvrir un jeu de cartes déjà fabriqué"));
    m_saveButton = barButton(m_topBar, tr("Enregistrer"),
                             {QStringLiteral("document-save-symbolic"),
                              QStringLiteral("document-save")},
                             tr("Écrire le jeu dans un fichier (Ctrl+S)"));
    m_installButton = barButton(m_topBar, tr("Installer"),
                                {QStringLiteral("emblem-ok-symbolic"),
                                 QStringLiteral("document-send-symbolic")},
                                tr("Déposer le jeu dans le dossier des cartes"));
    m_helpButton = barButton(m_topBar, tr("Aide"),
                             {QStringLiteral("help-contents-symbolic"),
                              QStringLiteral("help-about-symbolic")},
                             tr("Comment on fabrique un jeu (F1)"));
    m_darkButton = barButton(m_topBar, QString(), {}, QString());

    auto *across = new QHBoxLayout(m_topBar);
    across->setContentsMargins(16, 0, 10, 0);
    across->setSpacing(6);
    across->addWidget(m_title);
    across->addSpacing(12);
    across->addWidget(m_subtitle, 1);
    across->addWidget(m_tally);
    across->addSpacing(16);
    across->addWidget(m_newButton);
    across->addWidget(m_openButton);
    across->addWidget(m_saveButton);
    across->addWidget(m_installButton);
    across->addSpacing(10);
    across->addWidget(m_helpButton);
    across->addWidget(m_darkButton);

    connect(m_newButton, &QToolButton::clicked, this, &AtelierWindow::startFresh);
    connect(m_openButton, &QToolButton::clicked, this, &AtelierWindow::openFile);
    connect(m_saveButton, &QToolButton::clicked, this, [this] { saveFile(); });
    connect(m_installButton, &QToolButton::clicked, this, &AtelierWindow::installFile);
    connect(m_helpButton, &QToolButton::clicked, this, &AtelierWindow::openHelp);
    connect(m_darkButton, &QToolButton::clicked, this, &AtelierWindow::toggleDark);

    return m_topBar;
}

QWidget *AtelierWindow::buildLeft() {
    auto *panel = new QWidget(this);
    auto *rows = new QVBoxLayout(panel);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(8);

    rows->addWidget(dialogSectionHeading(panel, tr("Le jeu")));

    auto *fields = new QHBoxLayout;
    fields->setSpacing(8);
    m_name = new QLineEdit(panel);
    m_name->setPlaceholderText(tr("Nom du jeu"));
    m_author = new QLineEdit(panel);
    m_author->setPlaceholderText(tr("Auteur"));
    m_note = new QLineEdit(panel);
    m_note->setPlaceholderText(tr("Une ligne pour dire ce que c'est"));
    m_kind = new QComboBox(panel);
    for (const DeckKind kind : {DeckKind::Full, DeckKind::Short, DeckKind::Tarot}) {
        m_kind->addItem(CardSet::kindName(kind), int(kind));
    }
    m_kind->setFocusPolicy(Qt::NoFocus);
    m_kind->setToolTip(tr("Ce que le jeu contient, et donc les cases qu'il montre."));

    fields->addWidget(m_name, 2);
    fields->addWidget(m_author, 1);
    fields->addWidget(m_note, 3);
    fields->addWidget(m_kind);
    rows->addLayout(fields);

    m_kindNote = new QLabel(panel);
    m_kindNote->setWordWrap(true);
    markHintLabel(m_kindNote);
    rows->addWidget(m_kindNote);

    connect(m_kind, &QComboBox::currentIndexChanged, this, [this](int at) {
        if (at < 0) return;
        chooseKind(DeckKind(m_kind->itemData(at).toInt()));
    });

    for (QLineEdit *field : {m_name, m_author, m_note}) {
        connect(field, &QLineEdit::textEdited, this, [this] {
            m_set.name = m_name->text();
            m_set.author = m_author->text();
            m_set.note = m_note->text();
            noteChange();
        });
    }

    m_grid = new FaceGrid(panel);
    m_grid->watch(&m_set);
    m_gridRoom = new QScrollArea(panel);
    m_gridRoom->setWidgetResizable(true);
    m_gridRoom->setFrameShape(QFrame::NoFrame);
    m_gridRoom->setWidget(m_grid);
    // Une taille minimale explicite, et modeste.
    //
    // Sans elle, c'est celle de la grille qui vaut minimum pour la zone
    // défilante, et de là pour la fenêtre : une grille de tarot réclamant
    // sept rangées, la fenêtre grandissait jusqu'à les loger. Or une zone
    // défilante est précisément ce qui dispense d'être assez grand ; la
    // grille défile, et la fenêtre reste où l'on l'a mise.
    m_gridRoom->setMinimumSize(360, 220);
    rows->addWidget(m_gridRoom, 1);

    m_hint = new QLabel(panel);
    m_hint->setWordWrap(true);
    markHintLabel(m_hint);
    rows->addWidget(m_hint);

    connect(m_grid, &FaceGrid::targetChosen, this, [this] { updateChrome(); });
    connect(m_grid, &FaceGrid::clearRequested, this, &AtelierWindow::clearTarget);
    connect(m_grid, &FaceGrid::addBackRequested, this, &AtelierWindow::addBackFromTray);

    return panel;
}

// Les propriétés de la carte choisie, sous la grille.
//
// La section n'apparaît que lorsque la case choisie porte une carte dessinée :
// une image venue de l'extérieur n'a pas de couleurs à régler, et une section
// grisée en permanence sous la grille prendrait la place pour rien.
QWidget *AtelierWindow::buildCardProperties(QWidget *parent) {
    m_cardProps = new QWidget(parent);
    auto *rows = new QVBoxLayout(m_cardProps);
    // Les marges de la section sont celles des deux colonnes, qu'elle borde
    // par en dessous : seule celle du haut est reprise par l'espacement.
    rows->setContentsMargins(12, 0, 12, 12);
    rows->setSpacing(6);
    rows->addWidget(dialogSectionHeading(m_cardProps, tr("La carte dessinée")));

    auto *across = new QHBoxLayout;
    across->setSpacing(10);
    m_cardStyle = new CardStyleEditor(true, m_cardProps);
    across->addWidget(m_cardStyle, 1);

    auto *side = new QVBoxLayout;
    side->setSpacing(5);
    auto *spread = panelButton(m_cardProps, tr("Appliquer à tout"),
                               tr("Donner ces couleurs à toutes les cartes dessinées du jeu"));
    auto *adopt = panelButton(m_cardProps, tr("Garder pour la suite"),
                              tr("Reprendre ces couleurs dans l'onglet « Le jeu dessiné », "
                                 "pour les cartes posées ensuite"));
    m_placeArt = panelButton(m_cardProps, tr("Placer l'image…"),
                             tr("Déplacer et redimensionner l'image dans le panneau, la "
                                "carte montrée en grand"));
    m_dropArt = panelButton(m_cardProps, tr("Retirer l'image"),
                            tr("Rendre au panneau de cette carte sa couleur de fond"));
    side->addWidget(spread);
    side->addWidget(adopt);
    side->addWidget(m_placeArt);
    side->addWidget(m_dropArt);
    side->addStretch();
    across->addLayout(side);
    rows->addLayout(across);

    connect(spread, &QPushButton::clicked, this, &AtelierWindow::spreadRecipe);
    connect(adopt, &QPushButton::clicked, this, &AtelierWindow::adoptRecipe);
    connect(m_placeArt, &QPushButton::clicked, this, &AtelierWindow::placePanelArt);
    connect(m_dropArt, &QPushButton::clicked, this, &AtelierWindow::dropPanelArt);
    connect(m_cardStyle, &CardStyleEditor::edited, this, [this](const DrawnRecipe &how) {
        const int target = m_grid->target();
        if (!m_set.hasRecipe(target)) return;
        placeDrawnInto(target, how);
        m_grid->reread();
        noteChange();
    });

    m_cardProps->setVisible(false);
    return m_cardProps;
}

QWidget *AtelierWindow::buildRight() {
    m_sources = new QTabWidget(this);
    m_sources->setDocumentMode(true);
    m_sources->addTab(buildTrayTab(), tr("Les images"));
    m_sources->addTab(buildDrawnTab(), tr("Le jeu dessiné"));
    return m_sources;
}

// L'onglet du jeu dessiné : les cartes que la cartothèque trace elle-même.
//
// C'est la seule source qui ne demande aucun fichier : on choisit des
// couleurs et on remplit. Elle sert à partir d'un jeu complet qu'on retouche,
// plutôt que des cinquante-trois images qu'il faudrait trouver ailleurs.
QWidget *AtelierWindow::buildDrawnTab() {
    auto *panel = new QWidget(this);
    auto *rows = new QVBoxLayout(panel);
    rows->setContentsMargins(0, 10, 0, 0);
    rows->setSpacing(8);

    auto *says = new QLabel(tr("Les cartes que la cartothèque dessine elle-même. On les pose "
                               "dans les cases, puis on retouche leurs couleurs — ici pour "
                               "celles qui vont être posées, sous la grille pour celle qui "
                               "est choisie."),
                            panel);
    says->setWordWrap(true);
    markHintLabel(says);
    rows->addWidget(says);

    m_drawnStyle = new CardStyleEditor(false, panel);
    rows->addWidget(m_drawnStyle);

    auto *fills = new QHBoxLayout;
    fills->setSpacing(6);
    auto *empties = panelButton(panel, tr("Remplir les cases vides"),
                                tr("Poser une carte dessinée dans chaque case encore vide, "
                                   "sans toucher à ce qui est déjà placé"));
    auto *all = panelButton(panel, tr("Tout remplir"),
                            tr("Poser une carte dessinée dans les cinquante-trois cases, en "
                               "remplaçant ce qui s'y trouve"));
    fills->addWidget(empties);
    fills->addWidget(all);
    fills->addStretch();
    rows->addLayout(fills);

    auto *ones = new QHBoxLayout;
    ones->setSpacing(6);
    auto *here = panelButton(panel, tr("Poser dans la case choisie"),
                             tr("Dessiner la carte que la case choisie attend"));
    auto *back = panelButton(panel, tr("Ajouter le dos"),
                             tr("Ajouter le treillis comme dos de plus"));
    ones->addWidget(here);
    ones->addWidget(back);
    ones->addStretch();
    rows->addLayout(ones);
    rows->addStretch();

    connect(empties, &QPushButton::clicked, this, [this] { fillWithDrawn(true); });
    connect(all, &QPushButton::clicked, this, [this] { fillWithDrawn(false); });
    connect(here, &QPushButton::clicked, this, &AtelierWindow::placeDrawnHere);
    connect(back, &QPushButton::clicked, this, &AtelierWindow::addDrawnBack);

    // L'onglet défile, et sa taille minimale est modeste.
    //
    // Il porte un aperçu et une vingtaine de champs : sa hauteur naturelle
    // dépassait celle d'un écran, et c'est elle qui décidait de la hauteur
    // minimale de la fenêtre. Celle-ci grandissait donc à l'ouverture, puis
    // encore quand la section des propriétés apparaissait.
    auto *room = new QScrollArea(this);
    room->setWidgetResizable(true);
    room->setFrameShape(QFrame::NoFrame);
    room->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    room->setWidget(panel);
    room->setMinimumSize(300, 240);
    // Ni la fenêtre d'affichage ni le panneau ne peignent leur fond : c'est
    // celui de la fenêtre principale qui doit se voir. Sans cela, la fenêtre
    // d'affichage peint le sien, dans la palette du bureau et non dans celle
    // que le programme s'est donnée — l'onglet entier passait au noir sur un
    // bureau sombre.
    room->viewport()->setAutoFillBackground(false);
    panel->setAutoFillBackground(false);
    return room;
}

QWidget *AtelierWindow::buildTrayTab() {
    auto *panel = new QWidget(this);
    auto *rows = new QVBoxLayout(panel);
    rows->setContentsMargins(0, 10, 0, 0);
    rows->setSpacing(8);

    auto *opens = new QHBoxLayout;
    opens->setSpacing(6);
    auto *images = panelButton(panel, tr("Images…"), tr("Ouvrir des PNG ou des SVG"));
    auto *folder = panelButton(panel, tr("Dossier…"), tr("Prendre toutes les images d'un "
                                                        "dossier"));
    auto *dll = panelButton(panel, tr("Bibliothèque…"),
                            tr("Extraire les images d'une DLL Windows"));
    auto *other = panelButton(panel, tr("Autre set…"),
                              tr("Ouvrir les cartes d'un jeu existant, sans quitter "
                                 "celui-ci"));
    opens->addWidget(images);
    opens->addWidget(folder);
    opens->addWidget(dll);
    opens->addWidget(other);
    opens->addStretch();
    rows->addLayout(opens);

    m_clearCorners = new QCheckBox(tr("Rendre les coins transparents"), panel);
    m_clearCorners->setChecked(true);
    m_clearCorners->setToolTip(tr("Efface la couleur de fond autour des coins arrondis."));
    QPalette ink = m_clearCorners->palette();
    ink.setColor(QPalette::WindowText, currentPalette().surfaceText);
    m_clearCorners->setPalette(ink);
    rows->addWidget(m_clearCorners);

    auto *guess = panelButton(panel, tr("Deviner la correspondance"),
                              tr("Placer les cartes et les dos d'un coup : dans leur case "
                                 "d'origine pour les images d'un Autre set, par exemple"));
    rows->addWidget(guess);

    m_tray = new SourceTray(panel);
    m_trayRoom = new QScrollArea(panel);
    m_trayRoom->setWidgetResizable(true);
    m_trayRoom->setFrameShape(QFrame::NoFrame);
    m_trayRoom->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_trayRoom->setWidget(m_tray);
    m_trayRoom->setMinimumSize(240, 180);
    rows->addWidget(m_trayRoom, 1);

    auto *places = new QHBoxLayout;
    places->setSpacing(6);
    auto *place = panelButton(panel, tr("Ranger ici"),
                              tr("Placer l'image choisie dans la case choisie (ou "
                                 "double-clic)"));
    auto *asBack = panelButton(panel, tr("Ajouter comme dos"),
                               tr("Faire de l'image choisie un dos de plus"));
    auto *split = panelButton(panel, tr("Découper…"),
                              tr("Découper l'image choisie en autant de morceaux qu'elle "
                                 "contient de cartes"));
    auto *empty = panelButton(panel, tr("Vider"), tr("Oublier les images ouvertes"));
    places->addWidget(place);
    places->addWidget(asBack);
    places->addWidget(split);
    places->addStretch();
    places->addWidget(empty);
    rows->addLayout(places);

    connect(images, &QPushButton::clicked, this, &AtelierWindow::importImages);
    connect(folder, &QPushButton::clicked, this, &AtelierWindow::importFolder);
    connect(dll, &QPushButton::clicked, this, &AtelierWindow::importDll);
    connect(other, &QPushButton::clicked, this, &AtelierWindow::importSet);
    connect(guess, &QPushButton::clicked, this, &AtelierWindow::guessMapping);
    connect(place, &QPushButton::clicked, this, &AtelierWindow::placeChosen);
    connect(asBack, &QPushButton::clicked, this, &AtelierWindow::addBackFromTray);
    connect(split, &QPushButton::clicked, this, &AtelierWindow::splitSheet);
    connect(empty, &QPushButton::clicked, this, [this] {
        m_tray->clearItems();
        updateChrome();
    });
    connect(m_tray, &SourceTray::picked, this, [this] { updateChrome(); });
    connect(m_tray, &SourceTray::pickedTwice, this, &AtelierWindow::placeChosen);
    connect(m_tray, &SourceTray::menuAsked, this, &AtelierWindow::openTrayMenu);

    return panel;
}

// ---------------------------------------------------------------------------
// Le fichier.

void AtelierWindow::startFresh() {
    if (!offerToSave()) return;
    m_set = CardSet();
    m_set.name = tr("Jeu sans nom");
    m_path.clear();
    m_dirty = false;
    m_name->setText(m_set.name);
    m_author->clear();
    m_note->clear();
    m_kind->setCurrentIndex(m_kind->findData(int(m_set.kind)));
    m_grid->setTarget(CardSet::slotsOf(m_set.kind).value(0, CardSet::kJokerSlot));
    m_grid->reread();
    updateChrome();
}

// Le genre du jeu en cours.
//
// Les faces déjà posées ne sont pas effacées : un tarot ramené à un jeu de 52
// garde ses cavaliers et ses atouts dans le fichier, simplement cachés, et
// les retrouve si on lui rend son genre. Seules les cases que le genre
// emploie sont montrées et comptées.
void AtelierWindow::chooseKind(DeckKind kind) {
    if (m_set.kind == kind) return;
    m_set.kind = kind;
    const QVector<int> wanted = CardSet::slotsOf(kind);
    if (!FaceGrid::isBack(m_grid->target()) && !wanted.contains(m_grid->target())) {
        m_grid->setTarget(wanted.value(0, CardSet::kJokerSlot));
    }
    m_grid->reread();
    noteChange();
}

void AtelierWindow::openFile() {
    if (!offerToSave()) return;
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Ouvrir un jeu de cartes"), CardSet::installFolder(),
        tr("Jeux de cartes (*.%1)").arg(CardSet::extension()));
    if (path.isEmpty()) return;
    openPath(path);
}

bool AtelierWindow::openPath(const QString &path) {
    CardSet read;
    QString why;
    if (!read.load(path, &why)) {
        QMessageBox::warning(this, tr("Atelier"), why);
        return false;
    }
    m_set = read;
    m_path = path;
    m_dirty = false;
    m_name->setText(m_set.name);
    m_author->setText(m_set.author);
    m_note->setText(m_set.note);
    m_kind->setCurrentIndex(m_kind->findData(int(m_set.kind)));
    m_grid->reread();
    updateChrome();
    return true;
}

bool AtelierWindow::saveFile() {
    if (m_path.isEmpty()) return saveFileAs();
    QString why;
    if (!m_set.save(m_path, &why)) {
        QMessageBox::warning(this, tr("Atelier"), why);
        return false;
    }
    m_dirty = false;
    updateChrome();
    return true;
}

bool AtelierWindow::saveFileAs() {
    QString suggested = m_set.name.trimmed();
    if (suggested.isEmpty()) suggested = tr("jeu");
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Enregistrer le jeu de cartes"),
        QDir(QDir::homePath()).filePath(suggested + QLatin1Char('.') + CardSet::extension()),
        tr("Jeux de cartes (*.%1)").arg(CardSet::extension()));
    if (path.isEmpty()) return false;
    m_path = path;
    return saveFile();
}

void AtelierWindow::installFile() {
    if (m_set.filledFaces() == 0) {
        QMessageBox::information(this, tr("Atelier"),
                                 tr("Il n'y a encore aucune carte dans ce jeu."));
        return;
    }
    const QString folder = CardSet::installFolder();
    if (!QDir().mkpath(folder)) {
        QMessageBox::warning(this, tr("Atelier"),
                             tr("Le dossier %1 ne se crée pas.").arg(folder));
        return;
    }

    QString stem = m_set.name.trimmed();
    if (stem.isEmpty()) stem = tr("jeu");
    // Le nom du jeu transformé en nom de fichier.
    stem.replace(QRegularExpression(QStringLiteral("[^\\w\\-]+")), QStringLiteral("-"));
    if (stem.isEmpty()) stem = QStringLiteral("jeu");

    const QString path = QDir(folder).filePath(stem + QLatin1Char('.') + CardSet::extension());
    if (QFileInfo::exists(path)) {
        const auto answer = QMessageBox::question(
            this, tr("Atelier"),
            tr("Un jeu nommé « %1 » est déjà installé. Le remplacer ?").arg(stem),
            QMessageBox::Cancel | QMessageBox::Yes);
        if (answer != QMessageBox::Yes) return;
    }

    QString why;
    if (!m_set.save(path, &why)) {
        QMessageBox::warning(this, tr("Atelier"), why);
        return;
    }
    if (m_path.isEmpty()) m_path = path;
    m_dirty = false;
    updateChrome();

    QString told = tr("« %1 » est installé dans :\n%2").arg(m_set.name, folder);
    if (!m_set.complete()) {
        told += tr("\n\n%1 cartes manquantes, qui resteront dessinées par le jeu.")
                    .arg(m_set.slotCount() - m_set.filledFaces());
    }
    QMessageBox::information(this, tr("Atelier"), told);
}

// ---------------------------------------------------------------------------
// L'ouverture des images.

void AtelierWindow::importImages() {
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Ouvrir des images"), QDir::homePath(),
        tr("Images (*.png *.svg *.svgz *.bmp *.jpg *.jpeg *.gif)"));
    takeFiles(paths);
}

void AtelierWindow::importFolder() {
    const QString folder =
        QFileDialog::getExistingDirectory(this, tr("Ouvrir un dossier d'images"), QDir::homePath());
    if (folder.isEmpty()) return;
    const QStringList names = {QStringLiteral("*.png"),  QStringLiteral("*.svg"),
                               QStringLiteral("*.svgz"), QStringLiteral("*.bmp"),
                               QStringLiteral("*.jpg"),  QStringLiteral("*.jpeg"),
                               QStringLiteral("*.gif")};
    QStringList paths;
    for (const QFileInfo &file :
         QDir(folder).entryInfoList(names, QDir::Files | QDir::Readable, QDir::Name)) {
        paths.append(file.absoluteFilePath());
    }
    takeFiles(paths);
}

void AtelierWindow::takeFiles(const QStringList &paths) {
    if (paths.isEmpty()) return;
    QVector<DllImage> found;
    QStringList refused;
    for (const QString &path : paths) {
        QString why;
        const Artwork art = Artwork::fromFile(path, &why);
        if (art.isEmpty()) {
            refused.append(QFileInfo(path).fileName());
            continue;
        }
        DllImage image;
        image.label = QFileInfo(path).fileName();
        image.kind = art.kind;
        image.bytes = art.bytes;
        found.append(image);
    }
    m_tray->addItems(found);
    if (!refused.isEmpty()) {
        QMessageBox::information(
            this, tr("Atelier"),
            tr("%1 fichiers n'ont pas été lus : %2")
                .arg(refused.size())
                .arg(refused.mid(0, 6).join(QStringLiteral(", "))));
    }
    updateChrome();
}

void AtelierWindow::importDll() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Ouvrir une bibliothèque Windows"), QDir::homePath(),
        tr("Bibliothèques (*.dll *.DLL *.exe *.EXE);;Tous les fichiers (*)"));
    if (path.isEmpty()) return;

    QString why;
    const DllReading reading = readDllImages(path, &why);
    if (reading.images.isEmpty()) {
        QMessageBox::warning(this, tr("Atelier"),
                             why.isEmpty() ? tr("Aucune image dans ce fichier.") : why);
        return;
    }

    QVector<DllImage> images = reading.images;
    if (m_clearCorners->isChecked()) {
        for (DllImage &image : images) {
            if (image.kind != "bmp") continue;
            const QByteArray made = clearCorners(image.bytes, image.kind);
            if (made == image.bytes) continue;
            image.bytes = made;
            image.kind = "png";
        }
    }
    m_tray->setItems(images);

    QString told = tr("%1 images sorties de %2 (%3).")
                       .arg(images.size())
                       .arg(QFileInfo(path).fileName(), reading.container);
    if (!reading.note.isEmpty()) told += QStringLiteral("\n") + reading.note;
    QMessageBox::information(this, tr("Atelier"), told);
    updateChrome();
}

// Reprendre les cartes d'un jeu existant. Le jeu en cours n'est pas modifié :
// les faces et les dos de l'autre jeu sont ajoutés à la liste des images, avec
// la case qu'ils occupaient, et on y prend ce qu'on veut — une seule carte, ou
// le jeu entier d'un coup avec « Deviner la correspondance ».
void AtelierWindow::importSet() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Prendre les cartes d'un Autre set"), CardSet::installFolder(),
        tr("Jeux de cartes (*.%1)").arg(CardSet::extension()));
    if (path.isEmpty()) return;

    CardSet read;
    QString why;
    if (!read.load(path, &why)) {
        QMessageBox::warning(this, tr("Atelier"), why);
        return;
    }

    QVector<DllImage> images;
    // Toutes les cases, et non celles du genre en cours : on reprend les
    // images d'un jeu qui peut être d'un autre genre que celui-ci.
    for (int slot = 0; slot < CardSet::kSlots; ++slot) {
        if (!read.hasFace(slot)) continue;
        const Artwork art = read.face(slot);
        DllImage image;
        image.label = CardSet::slotShort(slot);
        image.kind = art.kind;
        image.bytes = art.bytes;
        image.place = slot;
        images.append(image);
    }
    const int faces = images.size();
    for (int at = 0; at < read.backCount(); ++at) {
        const Artwork art = read.back(at);
        if (art.isEmpty()) continue;
        DllImage image;
        image.label = read.backName(at);
        image.kind = art.kind;
        image.bytes = art.bytes;
        image.place = FaceGrid::backTarget(at);
        images.append(image);
    }

    if (images.isEmpty()) {
        QMessageBox::information(this, tr("Atelier"), tr("Ce jeu ne porte aucune image."));
        return;
    }

    // Les images s'ajoutent à celles qui sont déjà ouvertes : on reprend
    // souvent deux figures d'un jeu et le reste d'un autre.
    const int before = m_tray->count();
    m_tray->addItems(images);
    const int landed = m_tray->count() - before;

    const QString from = read.name.trimmed().isEmpty() ? QFileInfo(path).fileName() : read.name;
    QString told = tr("%1 cartes et %2 dos de « %3 » sont ouverts. Ils connaissent la case "
                      "qu'ils occupaient : « Deviner la correspondance » les y replace tous "
                      "d'un coup.")
                       .arg(faces)
                       .arg(images.size() - faces)
                       .arg(from);
    // Un jeu vectoriel ouvert sans le greffon SVG n'affiche rien : il vaut
    // mieux le signaler que de laisser compter des images qui ne seront pas
    // affichées.
    if (landed < images.size()) {
        told += QLatin1Char(' ')
                + tr("%1 images n'ont pas pu être affichées.").arg(images.size() - landed);
    }
    QMessageBox::information(this, tr("Atelier"), told);
    updateChrome();
}

// Les jeux de cartes récents ne contiennent plus une image par carte : ils en
// placent cinquante-deux sur une seule planche. Celle-ci est découpée, et ses
// morceaux prennent sa place dans la liste.
void AtelierWindow::splitSheet() {
    const int chosen = m_tray->chosen();
    const SourceTray::Item *item = m_tray->at(chosen);
    if (!item) {
        QMessageBox::information(this, tr("Atelier"),
                                 tr("Choisissez d'abord l'image à découper."));
        return;
    }

    SplitDialog dialog(item->image, this);
    if (!dialog.readable()) {
        QMessageBox::warning(this, tr("Atelier"), tr("Cette image n'a pas pu être lue."));
        return;
    }
    if (dialog.exec() != QDialog::Accepted || dialog.pieces().isEmpty()) return;

    m_tray->replaceItem(chosen, dialog.pieces());
    updateChrome();
}

// Le menu du clic droit sur une image : ce qu'on peut faire de cette image
// avec celle qui est sélectionnée. Il est construit ici plutôt que dans la
// liste, qui ne connaît ni ce qui est sélectionné ailleurs ni ce que devient
// chacune de ses images.
void AtelierWindow::openTrayMenu(int index, const QPoint &where) {
    const SourceTray::Item *under = m_tray->at(index);
    if (!under) return;
    const int chosen = m_tray->chosen();
    const SourceTray::Item *anchor = chosen == index ? nullptr : m_tray->at(chosen);

    QMenu menu(this);
    // Le nom des deux images, raccourci : « HIGHLIGHTSHEET1X22 » tient dans
    // un menu, un chemin de ressource complet non.
    const QString here = imageStem(under->image.label);
    if (!anchor) {
        QAction *why = menu.addAction(
            chosen == index
                ? tr("Cette image est celle qui est choisie : le collage en demande deux.")
                : tr("Choisissez d'abord l'image à laquelle coller celle-ci."));
        why->setEnabled(false);
    } else {
        const QString there = imageStem(anchor->image.label);
        QAction *title = menu.addAction(tr("Coller « %1 »…").arg(here));
        title->setEnabled(false);
        menu.addSeparator();
        // Le côté est exprimé du point de vue de l'image sélectionnée, qui
        // sert d'ancre : c'est elle qu'on a sous les yeux, bordée de la
        // couleur du thème.
        const struct {
            JoinSide side;
            QString text;
        } ways[] = {
            {JoinSide::Left, tr("à gauche de « %1 »").arg(there)},
            {JoinSide::Right, tr("à droite de « %1 »").arg(there)},
            {JoinSide::Above, tr("au-dessus de « %1 »").arg(there)},
            {JoinSide::Below, tr("au-dessous de « %1 »").arg(there)},
        };
        for (const auto &way : ways) {
            const JoinSide side = way.side;
            connect(menu.addAction(way.text), &QAction::triggered, this,
                    [this, index, side] { joinWith(index, side); });
        }
    }
    menu.exec(where);
}

// Deux images recollées en une. C'est ainsi que se reconstituent les quatre
// quarts d'une planche en double résolution, la planche entière se découpant
// correctement là où aucun de ses quarts ne s'y prêtait.
void AtelierWindow::joinWith(int other, JoinSide side) {
    const int chosen = m_tray->chosen();
    const SourceTray::Item *anchor = m_tray->at(chosen);
    const SourceTray::Item *second = m_tray->at(other);
    if (!anchor || !second || chosen == other) return;

    QString why;
    const DllImage joined = joinImages(anchor->image, second->image, side, &why);
    if (joined.bytes.isEmpty()) {
        QMessageBox::warning(this, tr("Atelier"), why);
        return;
    }

    // L'image recollée prend la place de l'ancre, et la seconde est retirée :
    // les quarts d'une planche reconstituée n'ont plus à figurer dans la
    // liste, et les collages s'enchaînent jusqu'à la planche entière.
    if (!m_tray->replaceItem(chosen, {joined})) {
        QMessageBox::warning(this, tr("Atelier"), tr("Le collage n'a pas pu être relu."));
        return;
    }
    m_tray->removeItem(other);
    updateChrome();
}

void AtelierWindow::guessMapping() {
    if (m_tray->count() == 0) {
        QMessageBox::information(this, tr("Atelier"),
                                 tr("Ouvrez d'abord une bibliothèque DLL, ou prenez les cartes d'un autre set."));
        return;
    }

    int faces = 0;
    int backs = 0;
    // Ce qui a été placé d'après l'ordre supposé d'une DLL doit être
    // vérifié ; une image reprise d'un autre jeu, elle, connaît sa case avec
    // certitude.
    int supposed = 0;
    m_tray->forgetUsage();
    for (int i = 0; i < m_tray->count(); ++i) {
        const SourceTray::Item *item = m_tray->at(i);
        if (!item) continue;

        Artwork art;
        art.kind = item->image.kind;
        art.bytes = item->image.bytes;

        // Une image reprise d'un jeu existant connaît la case qu'elle
        // occupait : il n'y a rien à deviner, elle y retourne.
        const int place = item->image.place;
        if (place != DllImage::kNowhere) {
            if (FaceGrid::isBack(place)) {
                m_set.addBack(item->image.label, art);
                ++backs;
            } else if (place < CardSet::kSlots) {
                m_set.setFace(place, art);
                ++faces;
            } else {
                continue;
            }
            m_tray->markUsed(i, true);
            continue;
        }

        if (item->image.id < 0) continue;

        const int slot = classicCardSlotForResource(item->image.id);
        if (slot >= 0) {
            m_set.setFace(slot, art);
            m_tray->markUsed(i, true);
            ++faces;
            ++supposed;
            continue;
        }
        if (classicResourceIsBack(item->image.id)) {
            m_set.addBack(tr("Dos %1").arg(item->image.id - 52), art);
            m_tray->markUsed(i, true);
            ++backs;
            ++supposed;
        }
    }

    if (faces == 0 && backs == 0) {
        QMessageBox::information(
            this, tr("Atelier"),
            tr("Ces images ne viennent ni d'un Autre set ni d'une bibliothèque : rien à "
               "deviner."));
        return;
    }

    m_grid->reread();
    noteChange();
    QString told = tr("%1 cartes et %2 dos placés.").arg(faces).arg(backs);
    if (supposed > 0) {
        told += QLatin1Char(' ')
                + tr("Vérifiez la grille : l'ordre des couleurs varie selon les versions.");
    }
    QMessageBox::information(this, tr("Atelier"), told);
}

// ---------------------------------------------------------------------------
// Le placement des images.

void AtelierWindow::placeInto(int target, int trayIndex) {
    const SourceTray::Item *item = m_tray->at(trayIndex);
    if (!item) return;

    Artwork art;
    art.kind = item->image.kind;
    art.bytes = item->image.bytes;

    // Une image posée sur une carte dessinée qui porte un panneau vient à la
    // place de la couleur de fond de ce panneau : la carte garde son papier,
    // son filet, son coin et ce qu'elle porte. Une carte sans panneau, et une
    // case qui n'est pas dessinée, reçoivent l'image à la place de la carte
    // entière, comme auparavant.
    if (!FaceGrid::isBack(target) && m_set.hasRecipe(target)) {
        DrawnRecipe how = m_set.recipe(target);
        PanelArt placed;
        placed.bytes = art.bytes;
        placed.kind = art.kind;
        // Une image illisible — un SVG en l'absence du greffon qui le lit —
        // ne saurait être placée : elle remplace alors la carte, où le même
        // greffon manquera, mais où le jeu la gardera telle quelle.
        if (cardCarriesPanel(target, how) && placed.decode()) {
            how.panelArt = placed;
            placeDrawnInto(target, how);
            m_tray->markUsed(trayIndex, true);
            m_grid->reread();
            updateCardProperties();
            noteChange();
            return;
        }
    }

    if (FaceGrid::isBack(target)) {
        m_set.setBack(FaceGrid::backOfTarget(target), art);
    } else {
        m_set.setFace(target, art);
    }

    m_tray->markUsed(trayIndex, true);
    m_grid->reread();

    // On avance à la case vide suivante.
    if (!FaceGrid::isBack(target)) m_grid->setTarget(m_grid->nextEmptyFace(target));
    noteChange();
}

void AtelierWindow::placeChosen() {
    if (m_tray->chosen() < 0) {
        QMessageBox::information(this, tr("Atelier"),
                                 tr("Choisissez d'abord une image à droite."));
        return;
    }
    placeInto(m_grid->target(), m_tray->chosen());
}

void AtelierWindow::clearTarget(int target) {
    if (FaceGrid::isBack(target)) {
        m_set.removeBack(FaceGrid::backOfTarget(target));
    } else {
        m_set.clearFace(target);
    }
    m_grid->reread();
    noteChange();
}

void AtelierWindow::addBackFromTray() {
    if (m_tray->chosen() < 0) {
        QMessageBox::information(this, tr("Atelier"),
                                 tr("Choisissez d'abord l'image qui fera le dos."));
        return;
    }
    const SourceTray::Item *item = m_tray->at(m_tray->chosen());
    if (!item) return;
    Artwork art;
    art.kind = item->image.kind;
    art.bytes = item->image.bytes;
    m_set.addBack(tr("Dos %1").arg(m_set.backCount() + 1), art);
    m_tray->markUsed(m_tray->chosen(), true);
    m_grid->reread();
    noteChange();
}

// ---------------------------------------------------------------------------
// Le jeu dessiné.

DrawnRecipe AtelierWindow::recipeFor(int key, DrawnRecipe how) const {
    how.panelArt = m_set.hasRecipe(key) ? m_set.recipe(key).panelArt : PanelArt();
    return how;
}

bool AtelierWindow::cardCarriesPanel(int slot, const DrawnRecipe &how) const {
    if (FaceGrid::isBack(slot) || slot < 0 || slot >= CardSet::kSlots) return false;
    // La case du joker est l'excuse dans un tarot, laquelle prend la couleur
    // des atouts : c'est ainsi que le dessin la reconnaît.
    const int suit = (slot == CardSet::kJokerSlot && m_set.kind == DeckKind::Tarot)
                         ? CardSet::kTrumpSuit
                         : CardSet::suitOfSlot(slot);
    const QSize size = drawnCardSize(how.height);
    return !cardPanelBox(QRectF(QPointF(), QSizeF(size)), CardSet::rankOfSlot(slot), suit,
                         how.style)
                .isEmpty();
}

void AtelierWindow::placePanelArt() {
    const int target = m_grid->target();
    if (!m_set.hasRecipe(target)) return;
    const DrawnRecipe how = m_set.recipe(target);
    if (how.panelArt.isEmpty()) return;

    // La carte se refait à mesure, derrière la fenêtre : celle-ci place, et
    // n'a donc rien à valider.
    PlaceArtDialog box(how, target, this);
    connect(&box, &PlaceArtDialog::placed, this, [this, target](const PanelArt &art) {
        if (!m_set.hasRecipe(target)) return;
        DrawnRecipe moved = m_set.recipe(target);
        moved.panelArt = art;
        placeDrawnInto(target, moved);
        m_grid->reread();
        m_cardStyle->setRecipe(moved);
        noteChange();
    });
    box.exec();
}

void AtelierWindow::dropPanelArt() {
    const int target = m_grid->target();
    if (!m_set.hasRecipe(target)) return;
    DrawnRecipe how = m_set.recipe(target);
    if (how.panelArt.isEmpty()) return;
    how.panelArt = PanelArt();
    placeDrawnInto(target, how);
    m_grid->reread();
    updateCardProperties();
    noteChange();
}

void AtelierWindow::placeDrawnInto(int target, const DrawnRecipe &how) {
    if (FaceGrid::isBack(target)) {
        m_set.setDrawnBack(FaceGrid::backOfTarget(target), drawnBackArt(how), how);
        return;
    }
    m_set.setDrawnFace(target, drawnFaceArt(target, how, m_set.kind), how);
}

void AtelierWindow::fillWithDrawn(bool emptyOnly) {
    const DrawnRecipe how = m_drawnStyle->recipe();

    // Cinquante-trois cartes à tracer puis à encoder en PNG : c'est l'affaire
    // d'une seconde, mais d'une seconde pendant laquelle la fenêtre ne
    // répond pas.
    QApplication::setOverrideCursor(Qt::WaitCursor);
    for (const int slot : CardSet::slotsOf(m_set.kind)) {
        if (emptyOnly && m_set.hasFace(slot)) continue;
        const DrawnRecipe mine = recipeFor(slot, how);
        m_set.setDrawnFace(slot, drawnFaceArt(slot, mine, m_set.kind), mine);
    }
    // Un jeu a besoin d'un dos. « Remplir les cases vides » n'en ajoute un
    // que s'il n'y en a aucun ; « Tout remplir » refait le premier.
    if (m_set.backCount() == 0) {
        m_set.addDrawnBack(tr("Treillis"), drawnBackArt(how), how);
    } else if (!emptyOnly) {
        m_set.setDrawnBack(0, drawnBackArt(how), how);
    }
    QApplication::restoreOverrideCursor();

    m_grid->reread();
    noteChange();
}

void AtelierWindow::placeDrawnHere() {
    const int target = m_grid->target();
    placeDrawnInto(target, recipeFor(target, m_drawnStyle->recipe()));
    m_grid->reread();
    // On avance à la case vide suivante, comme après une image posée.
    if (!FaceGrid::isBack(target)) m_grid->setTarget(m_grid->nextEmptyFace(target));
    noteChange();
}

void AtelierWindow::addDrawnBack() {
    const DrawnRecipe how = m_drawnStyle->recipe();
    m_set.addDrawnBack(tr("Treillis %1").arg(m_set.backCount() + 1), drawnBackArt(how), how);
    m_grid->reread();
    noteChange();
}

void AtelierWindow::spreadRecipe() {
    const QVector<int> drawn = m_set.drawnKeys();
    if (drawn.isEmpty()) return;
    const DrawnRecipe how = m_cardStyle->recipe();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    for (const int key : drawn) placeDrawnInto(key, recipeFor(key, how));
    QApplication::restoreOverrideCursor();

    m_grid->reread();
    noteChange();
}

void AtelierWindow::adoptRecipe() {
    // Les couleurs de cette carte, et non son image : l'onglet du jeu dessiné
    // pose son style sur cinquante-trois cartes à la fois.
    DrawnRecipe how = m_cardStyle->recipe();
    how.panelArt = PanelArt();
    m_drawnStyle->setRecipe(how);
    m_sources->setCurrentIndex(1);
}

void AtelierWindow::updateCardProperties() {
    const int target = m_grid->target();
    const bool drawn = m_set.hasRecipe(target);
    m_cardProps->setVisible(drawn);
    if (!drawn) return;
    m_cardStyle->showCard(target);
    m_cardStyle->setRecipe(m_set.recipe(target));
    const bool placed = !m_set.recipe(target).panelArt.isEmpty();
    m_placeArt->setEnabled(placed);
    m_dropArt->setEnabled(placed);
}

// ---------------------------------------------------------------------------

void AtelierWindow::noteChange() {
    m_set.name = m_name->text();
    m_set.author = m_author->text();
    m_set.note = m_note->text();
    m_dirty = true;
    updateChrome();
}

void AtelierWindow::updateChrome() {
    const int faces = m_set.filledFaces();
    m_tally->setText(tr("%1 / %2 faces · %3 dos")
                         .arg(faces)
                         .arg(m_set.slotCount())
                         .arg(m_set.backCount()));
    m_kindNote->setText(CardSet::kindNote(m_set.kind));

    QString where = m_path.isEmpty() ? tr("jamais enregistré") : QFileInfo(m_path).fileName();
    if (m_dirty) where += tr(" · modifié");
    m_subtitle->setText(where);

    const int target = m_grid->target();
    QString says;
    if (FaceGrid::isBack(target)) {
        says = tr("Case choisie : le dos %1. ").arg(FaceGrid::backOfTarget(target) + 1);
    } else {
        says = tr("Case choisie : %1. ").arg(CardSet::slotName(target));
    }
    if (m_set.hasRecipe(target)) {
        says += tr("Elle est dessinée : ses couleurs se règlent juste en dessous.");
    } else if (m_tray->count() == 0) {
        says += tr("Ouvrez des images à droite, ou posez-y le jeu dessiné.");
    } else if (m_tray->chosen() < 0) {
        says += tr("Choisissez une image à droite, puis « Ranger ici ».");
    } else if (m_tray->count() > 1) {
        says += tr("Clic droit sur une case pour la vider, sur une autre image pour la "
                   "coller à celle qui est choisie.");
    } else {
        says += tr("Clic droit sur une case pour la vider.");
    }
    m_hint->setText(says);

    updateCardProperties();

    m_saveButton->setEnabled(faces > 0 || m_set.backCount() > 0);
    m_installButton->setEnabled(faces > 0);
}

void AtelierWindow::applyColours() {
    const Palette &scheme = currentPalette();

    m_topBar->setStyleSheet(
        QStringLiteral("QWidget#topBar { background: %5; border-bottom: 1px solid %6; }"
                       "QToolButton { border: none; border-radius: 7px;"
                       " padding: 6px 10px; color: %1; background: transparent; }"
                       "QToolButton:hover:enabled { background: %2; color: %3; }"
                       "QToolButton:pressed:enabled { background: %4; }"
                       "QToolButton:disabled { color: %6; }")
            .arg(scheme.faceText.name(), blendColours(scheme.face, scheme.accent, 0.22).name(),
                 scheme.faceText.name(),
                 blendColours(scheme.face, scheme.accent, 0.36).name(), scheme.face.name(),
                 scheme.edge.name()));

    setStyleSheet(QStringLiteral(
                      "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
                      " border-radius: 5px; padding: 5px 7px; }"
                      "QLineEdit:focus { border: 1px solid %4; }"
                      "QPushButton { background: %5; color: %2; border: 1px solid %3;"
                      " border-radius: 6px; padding: 6px 13px; }"
                      "QPushButton:hover { border: 1px solid %4; }"
                      "QCheckBox { spacing: 9px; color: %2; }"
                      "QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid %3;"
                      " border-radius: 4px; background: %1; }"
                      "QCheckBox::indicator:checked { background: %4; border: 1px solid %4; }"
                      "QScrollArea { border: 1px solid %3; border-radius: 8px;"
                      " background: transparent; }"
                      // Le contenu d'une zone défilante, fenêtre d'affichage
                      // comprise : sans cette règle, celle-ci peint son fond
                      // dans la palette du bureau, et un contenu qui ne peint
                      // pas le sien laisse voir ce fond-là.
                      "QScrollArea > QWidget > QWidget { background: transparent; }"
                      "QScrollBar:vertical { background: transparent; width: 11px; }"
                      "QScrollBar::handle:vertical { background: %3; border-radius: 5px;"
                      " min-height: 28px; }"
                      "QScrollBar::handle:vertical:hover { background: %4; }"
                      "QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }"
                      "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }"
                      // Le menu du clic droit est habillé ici comme le
                      // reste : un menu laissé au style du bureau arrive dans
                      // les couleurs du bureau, au milieu d'une fenêtre qui
                      // porte les siennes.
                      "QMenu { background: %5; color: %2; border: 1px solid %3; padding: 5px; }"
                      "QMenu::item { padding: 6px 16px; border-radius: 5px; }"
                      "QMenu::item:selected { background: %4; color: %6; }"
                      "QMenu::item:disabled { color: %7; }"
                      "QMenu::separator { height: 1px; background: %3; margin: 4px 8px; }"
                      // Les onglets du panneau de droite. Le style du bureau
                      // les dessinerait dans ses propres couleurs, au milieu
                      // d'une fenêtre qui porte les siennes.
                      "QTabWidget::pane { border: none; }"
                      "QTabBar::tab { background: transparent; color: %7;"
                      " border-bottom: 2px solid transparent; padding: 7px 14px;"
                      " margin-right: 2px; }"
                      "QTabBar::tab:hover { color: %2; }"
                      "QTabBar::tab:selected { color: %4; border-bottom: 2px solid %4; }")
                      .arg(scheme.field.name(), scheme.surfaceText.name(), scheme.edge.name(),
                           scheme.accent.name(), scheme.surface.name(), scheme.accentText.name(),
                           scheme.hint.name()));

    QPalette window = palette();
    window.setColor(QPalette::Window, scheme.desk);
    window.setColor(QPalette::WindowText, scheme.surfaceText);
    window.setColor(QPalette::Base, scheme.field);
    window.setColor(QPalette::Text, scheme.surfaceText);
    setPalette(window);

    QPalette bar = m_topBar->palette();
    bar.setColor(QPalette::Window, scheme.face);
    bar.setColor(QPalette::WindowText, scheme.faceText);
    m_topBar->setPalette(bar);
    m_topBar->setAutoFillBackground(true);

    inkLabel(m_title, scheme.accent);
    inkLabel(m_subtitle, scheme.hint);
    inkLabel(m_tally, scheme.accent);
    inkLabel(m_hint, scheme.hint);

    m_darkButton->setToolTip(darkMode() ? tr("Couleurs claires") : tr("Couleurs sombres"));
    m_darkButton->setProperty(
        "iconNames", darkMode() ? QStringList{QStringLiteral("weather-clear-symbolic")}
                                : QStringList{QStringLiteral("weather-clear-night-symbolic")});

    for (QToolButton *button : {m_newButton, m_openButton, m_saveButton, m_installButton,
                                m_helpButton, m_darkButton}) {
        const QStringList names = button->property("iconNames").toStringList();
        const QIcon icon = tintedIcon(names, scheme.faceText);
        button->setIcon(icon);
        if (button == m_darkButton) {
            button->setToolButtonStyle(icon.isNull() ? Qt::ToolButtonTextOnly
                                                     : Qt::ToolButtonIconOnly);
            button->setText(darkMode() ? tr("Clair") : tr("Sombre"));
        }
    }

    recolourSectionHeadings(this);
    m_drawnStyle->applyColours();
    m_cardStyle->applyColours();
    m_grid->update();
    m_tray->update();
    update();
}

void AtelierWindow::toggleDark() {
    setDarkMode(!darkMode());
    QSettings().setValue(QStringLiteral("look/sombre"), darkMode());
    applyColours();
}

void AtelierWindow::openHelp() {
    HelpDialog dialog(this);
    dialog.exec();
}

bool AtelierWindow::offerToSave() {
    if (!m_dirty || m_set.filledFaces() == 0) return true;
    const auto answer = QMessageBox::question(
        this, tr("Atelier"), tr("Le jeu en cours n'est pas enregistré. L'enregistrer ?"),
        QMessageBox::Discard | QMessageBox::Cancel | QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) return saveFile();
    return true;
}

void AtelierWindow::closeEvent(QCloseEvent *event) {
    if (offerToSave()) {
        event->accept();
        return;
    }
    event->ignore();
}

void AtelierWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_F1) {
        openHelp();
        return;
    }
    if (event->matches(QKeySequence::Save)) {
        saveFile();
        return;
    }
    if (event->matches(QKeySequence::Open)) {
        openFile();
        return;
    }
    QMainWindow::keyPressEvent(event);
}
