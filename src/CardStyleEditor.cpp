#include "CardStyleEditor.h"

#include "CardSet.h"
#include "DialogChrome.h"
#include "Palette.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

// Les couleurs réglables, et le morceau de carte que chacune colore.
//
// La table est la seule description de ces champs : les pastilles, leurs
// infobulles et la couleur qu'un clic va changer en découlent toutes, et il
// suffit d'une ligne de plus pour exposer une couleur supplémentaire.
struct Field {
    const char *label;
    QColor CardStyle::*member;
    const char *tip;
    // Le champ se retire-t-il ? Un seul le peut, le panneau des figures : son
    // étiquette est alors une case à cocher, posée à côté de la pastille qui
    // en donne la couleur.
    bool togglable;
};

const Field kFields[] = {
    {"Papier", &CardStyle::paper, "Le fond de la carte.", false},
    {"Encre noire", &CardStyle::ink, "Le pique et le trèfle, et le rang dans le coin.", false},
    {"Encre rouge", &CardStyle::red, "Le cœur et le carreau.", false},
    {"Panneau", &CardStyle::panel,
     "Le rectangle beige que portent les figures : le valet, la dame, le roi, le joker et "
     "l'as. Décochez pour poser l'emblème à même le papier : seul le rectangle disparaît, "
     "l'emblème gardant sa taille et sa place.",
     true},
    {"Bonnet du joker", &CardStyle::cap, "Le bonnet, l'étoile, et le mot « Joker ».", false},
    {"Grelots du joker", &CardStyle::bell, "Les trois grelots du bonnet.", false},
    {"Fond du dos", &CardStyle::back, "La couleur pleine du dos.", false},
    {"Treillis du dos", &CardStyle::backInk, "Les diagonales tracées sur le dos.", false},
};
constexpr int kFieldCount = int(sizeof(kFields) / sizeof(kFields[0]));

// Les thèmes proposés : une teinte chacun, dont toutes les couleurs d'une
// carte sont dérivées. C'est un point de départ, que les pastilles retouchent
// ensuite.
struct Theme {
    int hue;
    const char *name;
};

const Theme kThemes[] = {
    {142, "Vert"}, {168, "Jade"},      {196, "Bleu"},
    {268, "Prune"}, {348, "Bordeaux"}, {32, "Tabac"},
};
constexpr int kThemeCount = int(sizeof(kThemes) / sizeof(kThemes[0]));

} // namespace

// L'aperçu : les cartes telles que la recette les dessine.
//
// Il n'a ni signal ni slot, et n'est donc pas un `QObject` déclaré comme tel :
// c'est un widget qui ne fait que se peindre.
class CardStylePreview : public QWidget {
public:
    explicit CardStylePreview(bool compact, QWidget *parent = nullptr)
        : QWidget(parent), m_compact(compact) {
        setMinimumHeight(compact ? 70 : 136);
    }

    void setRecipe(const DrawnRecipe &how) {
        m_recipe = how;
        update();
    }

    void showCard(int key) {
        m_key = key;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        const Palette &scheme = currentPalette();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.fillRect(rect(), blendColours(scheme.surface, scheme.desk, 0.45));

        // En version compacte, la carte choisie ; sinon un échantillon des
        // quatre sortes de dessin : une figure, une carte à symboles, le
        // joker et le dos.
        QVector<int> shown;
        if (m_compact) {
            shown.append(m_key);
        } else {
            shown = {CardSet::slotFor(13, 1), CardSet::slotFor(7, 0), CardSet::kJokerSlot, -1};
        }

        constexpr qreal kGap = 7.0;
        const qreal room = (width() - kGap * (shown.size() + 1)) / shown.size();
        qreal tall = height() - kGap * 2.0;
        qreal wide = tall * kCardRatio;
        if (wide > room) {
            wide = room;
            tall = wide / kCardRatio;
        }
        if (wide < 8.0 || tall < 12.0) return;

        qreal x = (width() - (wide * shown.size() + kGap * (shown.size() - 1))) / 2.0;
        const qreal y = (height() - tall) / 2.0;
        for (const int key : shown) {
            const QRectF box(x + 0.5, y + 0.5, wide - 1.0, tall - 1.0);
            if (key < 0) {
                drawDefaultBack(&painter, box, m_recipe.style);
            } else {
                drawDefaultFace(&painter, box, CardSet::rankOfSlot(key),
                                CardSet::suitOfSlot(key), m_recipe.style);
            }
            x += wide + kGap;
        }
    }

private:
    DrawnRecipe m_recipe;
    bool m_compact = false;
    int m_key = CardSet::slotFor(13, 1);
};

// ---------------------------------------------------------------------------

CardStyleEditor::CardStyleEditor(bool compact, QWidget *parent)
    : QWidget(parent), m_compact(compact) {
    m_recipe.style = CardStyle::fromPalette(paletteForHue(kThemes[0].hue, false));

    m_preview = new CardStylePreview(compact, this);
    m_preview->setRecipe(m_recipe);

    m_theme = new QComboBox(this);
    for (int at = 0; at < kThemeCount; ++at) m_theme->addItem(tr(kThemes[at].name));
    m_theme->setToolTip(tr("Reprendre toutes les couleurs d'un thème, puis les retoucher."));
    m_theme->setFocusPolicy(Qt::NoFocus);

    m_dark = new QCheckBox(tr("Sombre"), this);
    m_dark->setToolTip(tr("Les couleurs du thème en version sombre : un papier foncé et une "
                          "encre claire."));
    m_dark->setFocusPolicy(Qt::NoFocus);

    m_height = new QSpinBox(this);
    m_height->setRange(64, 1200);
    m_height->setSingleStep(20);
    m_height->setValue(m_recipe.height);
    m_height->setSuffix(tr(" px"));
    m_height->setToolTip(tr("Hauteur de l'image produite. Les cinquante-trois cartes la "
                            "partagent : au-delà de quelques centaines de pixels, le fichier "
                            "grossit sans que la carte se lise mieux."));

    m_emblem = new QSpinBox(this);
    m_emblem->setRange(int(kEmblemSmallest * 100.0), int(kEmblemLargest * 100.0));
    m_emblem->setSingleStep(5);
    m_emblem->setValue(int(std::round(m_recipe.style.emblemSize * 100.0)));
    m_emblem->setSuffix(tr(" %"));
    m_emblem->setToolTip(tr("La taille de l'emblème des figures — couronne, diadème, fleur "
                            "de lis, bonnet du joker, symbole de l'as — en proportion de "
                            "celle qu'il a par défaut."));

    // Les deux champs de taille du panneau. Ils sont bornés comme la taille de
    // l'emblème, et le dessin ramène de toute façon le panneau dans la marge
    // du papier.
    auto panelField = [this](const QString &tip) {
        auto *field = new QSpinBox(this);
        field->setRange(int(kPanelSmallest * 100.0), int(kPanelLargest * 100.0));
        field->setSingleStep(5);
        field->setSuffix(tr(" %"));
        field->setToolTip(tip);
        return field;
    };
    m_panelWide = panelField(tr("La largeur du panneau, en proportion de celle qu'il a par "
                                "défaut. L'emblème étant inscrit dedans, il suit cette "
                                "largeur ; la grille des symboles, elle, ne bouge pas."));
    m_panelWide->setValue(int(std::round(m_recipe.style.panelWidth * 100.0)));
    m_panelTall = panelField(tr("La hauteur du panneau, en proportion de celle qu'il a par "
                                "défaut. Un panneau ne déborde pas du papier : au-delà de la "
                                "marge de la carte, le facteur ne produit plus rien."));
    m_panelTall->setValue(int(std::round(m_recipe.style.panelHeight * 100.0)));

    m_pipPanel = new QCheckBox(tr("Sous les symboles"), this);
    m_pipPanel->setToolTip(tr("Trace le panneau sous les symboles des cartes de deux à dix, "
                              "et non derrière les seules figures. Les lettres et les "
                              "symboles des coins se tracent par-dessus : sur une carte "
                              "imprimée, l'index du coin se lit toujours, quoi qu'il y ait "
                              "derrière."));
    m_pipPanel->setChecked(m_recipe.style.panelOnPips);
    m_pipPanel->setCursor(Qt::PointingHandCursor);
    m_pipPanel->setFocusPolicy(Qt::NoFocus);

    m_glassy = new QCheckBox(tr("Emblèmes en verre"), this);
    m_glassy->setToolTip(tr("Remplit l'emblème des figures d'un dégradé au lieu d'un aplat : "
                            "clair en haut, sombre sous le milieu, et une lumière renvoyée "
                            "par le bas. Les symboles des cartes de deux à dix restent "
                            "plats, trop petits pour qu'un dégradé s'y lise, et les traits "
                            "évidés aussi, un creux ne renvoyant pas la lumière."));
    m_glassy->setChecked(m_recipe.style.glassy);
    m_glassy->setCursor(Qt::PointingHandCursor);
    m_glassy->setFocusPolicy(Qt::NoFocus);

    m_letters = new QComboBox(this);
    for (int at = 0; at < kRankLettersCount; ++at) {
        m_letters->addItem(rankLettersName(RankLetters(at)));
    }
    m_letters->setCurrentIndex(int(m_recipe.style.letters));
    m_letters->setToolTip(tr("Les lettres du coin — as, valet, cavalier, dame, roi — selon "
                             "le pays qui imprime la carte : A V C D R en France, "
                             "A J Kn Q K dans le monde anglais, A B C D K en Allemagne, et "
                             "1 S C D R en Espagne, où l'as est chiffré. Le cavalier ne "
                             "paraît que sur un tarot."));
    m_letters->setFocusPolicy(Qt::NoFocus);

    for (int at = 0; at < kFieldCount; ++at) {
        auto *swatch = new QToolButton(this);
        swatch->setToolTip(tr(kFields[at].tip));
        swatch->setCursor(Qt::PointingHandCursor);
        swatch->setFocusPolicy(Qt::NoFocus);
        swatch->setFixedSize(compact ? QSize(28, 18) : QSize(34, 22));
        connect(swatch, &QToolButton::clicked, this, [this, at] { pickColour(at); });
        m_swatches.append(swatch);
    }

    // L'étiquette d'une pastille : un simple mot, sauf pour le panneau des
    // figures, dont l'étiquette commande aussi la présence.
    auto captionFor = [this](int at) -> QWidget * {
        if (!kFields[at].togglable) {
            auto *label = new QLabel(tr(kFields[at].label), this);
            label->setToolTip(tr(kFields[at].tip));
            markHintLabel(label);
            return label;
        }
        m_panelled = new QCheckBox(tr(kFields[at].label), this);
        m_panelled->setToolTip(tr(kFields[at].tip));
        m_panelled->setChecked(m_recipe.style.panelled);
        m_panelled->setCursor(Qt::PointingHandCursor);
        m_panelled->setFocusPolicy(Qt::NoFocus);
        return m_panelled;
    };

    // Les réglages vont par trois : les couleurs, les figures, et l'image
    // produite. Les blocs sont construits ici une fois, et les deux
    // dispositions ci-dessous les assemblent chacune à sa façon — empilés
    // dans l'onglet, côte à côte sous la grille.

    // Les couleurs : le thème qui les donne toutes, puis une pastille par
    // couleur. Celle du panneau n'y figure pas : elle va avec le panneau.
    auto colourBlock = [&] {
        auto *cells = new QGridLayout;
        cells->setHorizontalSpacing(9);
        cells->setVerticalSpacing(compact ? 2 : 5);
        int row = 0;
        for (int at = 0; at < kFieldCount; ++at) {
            if (kFields[at].togglable) continue;
            const int column = (row / 4) * 2;
            cells->addWidget(m_swatches.at(at), row % 4, column);
            cells->addWidget(captionFor(at), row % 4, column + 1);
            ++row;
        }
        cells->setColumnStretch(1, 1);
        cells->setColumnStretch(3, 1);
        return cells;
    };

    auto themeRow = [&] {
        auto *across = new QHBoxLayout;
        across->setSpacing(6);
        if (!compact) {
            auto *label = new QLabel(tr("Thème"), this);
            markHintLabel(label);
            across->addWidget(label);
        }
        across->addWidget(m_theme, compact ? 1 : 0);
        across->addWidget(m_dark);
        if (!compact) across->addStretch();
        return across;
    };

    // Le dessin : le panneau, la couleur qu'il prend, les cartes qu'il garnit,
    // sa taille, et la taille de l'emblème qu'il porte. La case à cocher sert
    // d'étiquette à la pastille, la présence et la couleur du panneau se
    // commandant du même endroit.
    auto shapeBlock = [&] {
        int panelAt = 0;
        for (int at = 0; at < kFieldCount; ++at) {
            if (kFields[at].togglable) panelAt = at;
        }
        auto *cells = new QGridLayout;
        cells->setHorizontalSpacing(9);
        cells->setVerticalSpacing(compact ? 2 : 5);
        // La ligne du panneau porte ses trois commandes : sa présence, sa
        // couleur, et les cartes qu'il garnit. La seconde case se lit dans la
        // suite de la première — « Panneau », « aussi sous les symboles » — et
        // tenir sur une ligne lui évite de pousser la section d'un cran sous
        // la grille des cases.
        cells->addWidget(captionFor(panelAt), 0, 0);
        cells->addWidget(m_swatches.at(panelAt), 0, 1);
        cells->addWidget(m_pipPanel, 0, 2);
        // La taille du panneau : deux champs sur une même ligne, séparés du
        // signe du produit, la largeur puis la hauteur.
        auto *sizeLabel = new QLabel(tr("Taille du panneau"), this);
        sizeLabel->setToolTip(m_panelWide->toolTip());
        markHintLabel(sizeLabel);
        auto *sizeRow = new QHBoxLayout;
        sizeRow->setSpacing(4);
        sizeRow->addWidget(m_panelWide);
        auto *times = new QLabel(QStringLiteral("×"), this);
        markHintLabel(times);
        sizeRow->addWidget(times);
        sizeRow->addWidget(m_panelTall);
        // Les deux champs restent côte à côte : c'est la place qui suit qui
        // s'étire, et non l'espace entre eux.
        sizeRow->addStretch();
        cells->addWidget(sizeLabel, 1, 0);
        cells->addLayout(sizeRow, 1, 1, 1, 2);
        auto *lettersLabel = new QLabel(tr("Lettres"), this);
        lettersLabel->setToolTip(m_letters->toolTip());
        markHintLabel(lettersLabel);
        cells->addWidget(lettersLabel, 2, 0);
        cells->addWidget(m_letters, 2, 1);
        auto *label = new QLabel(tr("Taille de l'emblème"), this);
        label->setToolTip(m_emblem->toolTip());
        markHintLabel(label);
        cells->addWidget(label, 3, 0);
        cells->addWidget(m_emblem, 3, 1);
        cells->addWidget(m_glassy, 4, 0, 1, 3);
        // C'est la place restante qui s'étire, et non l'étiquette : les deux
        // champs restent côte à côte au lieu d'être jetés aux deux bords.
        cells->setColumnStretch(3, 1);
        return cells;
    };

    // L'image : ce qui est produit, et non ce qui est dessiné.
    auto outputBlock = [&] {
        auto *cells = new QGridLayout;
        cells->setHorizontalSpacing(9);
        cells->setVerticalSpacing(compact ? 2 : 5);
        auto *label = new QLabel(tr("Hauteur"), this);
        label->setToolTip(m_height->toolTip());
        markHintLabel(label);
        cells->addWidget(label, 0, 0);
        cells->addWidget(m_height, 0, 1);
        cells->setColumnStretch(2, 1);
        return cells;
    };

    auto *rows = new QVBoxLayout(this);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(compact ? 5 : 8);

    if (compact) {
        // Sous la grille, l'espace est en largeur et non en hauteur :
        // l'aperçu et les trois parties se rangent côte à côte.
        auto *across = new QHBoxLayout;
        across->setSpacing(12);

        // L'aperçu, et sous lui le choix du thème.
        //
        // Le thème vaut pour les couleurs, et sa place serait dans leur
        // partie ; il est ici parce que la hauteur de la section est celle de
        // sa plus haute colonne, et qu'une ligne de moins dans les couleurs
        // est une ligne de moins sous la grille. La colonne de l'aperçu, elle,
        // avait de la place : la carte y est dessinée à la hauteur disponible
        // et laisse de l'air sur les côtés.
        m_preview->setFixedWidth(126);
        auto *first = new QVBoxLayout;
        first->setSpacing(4);
        first->addWidget(m_preview);
        first->addLayout(themeRow());
        first->addStretch();
        across->addLayout(first);

        auto part = [this, &across](const QString &title, QLayout *block, int stretch) {
            auto *column = new QVBoxLayout;
            column->setSpacing(3);
            column->addWidget(dialogSectionHeading(this, title));
            column->addLayout(block);
            column->addStretch();
            across->addLayout(column, stretch);
        };

        part(tr("Les couleurs"), colourBlock(), 1);
        part(tr("Le dessin"), shapeBlock(), 0);
        part(tr("L'image"), outputBlock(), 0);
        rows->addLayout(across);
    } else {
        rows->addWidget(m_preview);
        rows->addWidget(dialogSectionHeading(this, tr("Les couleurs")));
        rows->addLayout(themeRow());
        rows->addLayout(colourBlock());
        rows->addWidget(dialogSectionHeading(this, tr("Le dessin")));
        rows->addLayout(shapeBlock());
        rows->addWidget(dialogSectionHeading(this, tr("L'image")));
        rows->addLayout(outputBlock());
    }

    // Un thème ne donne que des couleurs. Le panneau et la taille de
    // l'emblème sont des choix de forme, que l'on ne perd donc pas en
    // essayant un thème après l'autre.
    auto takeTheme = [this] {
        const int at = std::clamp(m_theme->currentIndex(), 0, kThemeCount - 1);
        const bool panelled = m_recipe.style.panelled;
        const bool onPips = m_recipe.style.panelOnPips;
        const qreal wide = m_recipe.style.panelWidth;
        const qreal tall = m_recipe.style.panelHeight;
        const bool glassy = m_recipe.style.glassy;
        const RankLetters letters = m_recipe.style.letters;
        const qreal emblem = m_recipe.style.emblemSize;
        m_recipe.style = CardStyle::fromPalette(paletteForHue(kThemes[at].hue, m_dark->isChecked()));
        m_recipe.style.panelled = panelled;
        m_recipe.style.panelOnPips = onPips;
        m_recipe.style.panelWidth = wide;
        m_recipe.style.panelHeight = tall;
        m_recipe.style.glassy = glassy;
        m_recipe.style.letters = letters;
        m_recipe.style.emblemSize = emblem;
        noteEdit();
    };
    connect(m_theme, &QComboBox::currentIndexChanged, this, takeTheme);
    connect(m_dark, &QCheckBox::toggled, this, takeTheme);
    connect(m_height, &QSpinBox::valueChanged, this, [this](int tall) {
        m_recipe.height = tall;
        noteEdit();
    });
    connect(m_emblem, &QSpinBox::valueChanged, this, [this](int percent) {
        m_recipe.style.emblemSize = percent / 100.0;
        noteEdit();
    });
    connect(m_panelled, &QCheckBox::toggled, this, [this](bool on) {
        m_recipe.style.panelled = on;
        noteEdit();
    });
    connect(m_pipPanel, &QCheckBox::toggled, this, [this](bool on) {
        m_recipe.style.panelOnPips = on;
        noteEdit();
    });
    connect(m_panelWide, &QSpinBox::valueChanged, this, [this](int percent) {
        m_recipe.style.panelWidth = percent / 100.0;
        noteEdit();
    });
    connect(m_panelTall, &QSpinBox::valueChanged, this, [this](int percent) {
        m_recipe.style.panelHeight = percent / 100.0;
        noteEdit();
    });
    connect(m_glassy, &QCheckBox::toggled, this, [this](bool on) {
        m_recipe.style.glassy = on;
        noteEdit();
    });
    connect(m_letters, &QComboBox::currentIndexChanged, this, [this](int at) {
        if (at < 0 || at >= kRankLettersCount) return;
        m_recipe.style.letters = RankLetters(at);
        noteEdit();
    });

    refreshSwatches();
}

void CardStyleEditor::setRecipe(const DrawnRecipe &how) {
    m_recipe = how;
    // Les champs sont remplis sans que leurs signaux ne reviennent modifier
    // la recette qu'on vient de poser.
    m_quiet = true;
    m_height->setValue(m_recipe.height);
    m_emblem->setValue(int(std::round(m_recipe.style.emblemSize * 100.0)));
    m_panelled->setChecked(m_recipe.style.panelled);
    m_pipPanel->setChecked(m_recipe.style.panelOnPips);
    m_panelWide->setValue(int(std::round(m_recipe.style.panelWidth * 100.0)));
    m_panelTall->setValue(int(std::round(m_recipe.style.panelHeight * 100.0)));
    m_glassy->setChecked(m_recipe.style.glassy);
    m_letters->setCurrentIndex(int(m_recipe.style.letters));
    m_quiet = false;
    m_preview->setRecipe(m_recipe);
    refreshSwatches();
}

void CardStyleEditor::showCard(int key) { m_preview->showCard(key); }

void CardStyleEditor::applyColours() {
    const Palette &scheme = currentPalette();
    const QString rule = QStringLiteral(
                             "QComboBox, QSpinBox { background: %1; color: %2;"
                             " border: 1px solid %3; border-radius: 5px; padding: 3px 6px; }"
                             "QComboBox::drop-down { border: none; width: 16px; }"
                             "QComboBox QAbstractItemView { background: %4; color: %2;"
                             " border: 1px solid %3; selection-background-color: %5; }")
                             .arg(scheme.field.name(), scheme.surfaceText.name(),
                                  scheme.edge.name(), scheme.surface.name(),
                                  scheme.accent.name());
    if (styleSheet() != rule) setStyleSheet(rule);
    refreshSwatches();
    m_preview->update();
}

void CardStyleEditor::pickColour(int field) {
    if (field < 0 || field >= kFieldCount) return;
    QColor &slot = m_recipe.style.*(kFields[field].member);
    const QColor picked = QColorDialog::getColor(slot, this, tr(kFields[field].label));
    if (!picked.isValid() || picked == slot) return;
    slot = picked;
    noteEdit();
}

void CardStyleEditor::refreshSwatches() {
    const Palette &scheme = currentPalette();
    // Les commandes du panneau s'éteignent avec lui : sans rectangle, ni les
    // cartes qu'il garnit ni sa taille ne veulent dire quoi que ce soit.
    const bool panelled = m_recipe.style.panelled;
    if (m_pipPanel) m_pipPanel->setEnabled(panelled);
    if (m_panelWide) m_panelWide->setEnabled(panelled);
    if (m_panelTall) m_panelTall->setEnabled(panelled);
    for (int at = 0; at < m_swatches.size() && at < kFieldCount; ++at) {
        // La pastille d'un élément retiré ne colore plus rien : elle est
        // éteinte, plutôt que de laisser croire qu'elle agit encore.
        const bool live = !kFields[at].togglable || m_recipe.style.panelled;
        m_swatches.at(at)->setEnabled(live);
        const QColor colour = live ? m_recipe.style.*(kFields[at].member)
                                   : blendColours(m_recipe.style.*(kFields[at].member),
                                                  scheme.surface, 0.72);
        m_swatches.at(at)->setStyleSheet(
            QStringLiteral("QToolButton { background: %1; border: 1px solid %2;"
                           " border-radius: 4px; }"
                           "QToolButton:hover { border: 1px solid %3; }")
                .arg(colour.name(), scheme.edge.name(), scheme.accent.name()));
    }
}

void CardStyleEditor::noteEdit() {
    m_preview->setRecipe(m_recipe);
    refreshSwatches();
    if (m_quiet) return;
    Q_EMIT edited(m_recipe);
}
