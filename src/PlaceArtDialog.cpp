#include "PlaceArtDialog.h"

#include "CardPreview.h"
#include "DialogChrome.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>

namespace {

// La hauteur de la carte dans la fenêtre. Assez grande pour que le panneau
// mesure deux cent cinquante pixels de haut, c'est-à-dire pour qu'un
// mouvement de souris d'un pixel déplace l'image d'un pixel du rendu plutôt
// que de trois.
constexpr int kCardTall = 400;

} // namespace

PlaceArtDialog::PlaceArtDialog(const DrawnRecipe &how, int key, QWidget *parent)
    : QDialog(parent), m_recipe(how) {
    setWindowTitle(tr("Placer l'image"));
    const DialogChrome chrome = applyDialogChrome(this, tr("Placer"));

    m_preview = new CardStylePreview(true, this);
    m_preview->setRecipe(m_recipe);
    m_preview->showCard(key);
    m_preview->setMinimumHeight(kCardTall);
    m_preview->setMinimumWidth(int(kCardTall * kCardRatio) + 40);
    chrome.fields->addWidget(m_preview, 1);

    auto *hint = new QLabel(tr("Glissez l'image pour la déplacer dans le panneau. La molette "
                               "l'agrandit ou la réduit, un double-clic la recadre."),
                            this);
    hint->setWordWrap(true);
    markHintLabel(hint);
    chrome.fields->addWidget(hint);

    // Le champ d'échelle à côté du geste : la molette cadre vite, un nombre
    // cadre juste.
    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    auto *label = new QLabel(tr("Échelle"), this);
    markHintLabel(label);
    m_scale = new QSpinBox(this);
    m_scale->setRange(int(kPanelArtSmallest * 100.0), int(kPanelArtLargest * 100.0));
    m_scale->setSingleStep(5);
    m_scale->setSuffix(tr(" %"));
    m_scale->setToolTip(tr("La taille de l'image, cent pour cent étant la taille qui recouvre "
                           "le panneau au plus juste."));
    row->addWidget(label);
    row->addWidget(m_scale);
    row->addStretch();
    chrome.fields->addLayout(row);

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *again = buttons->addButton(tr("Recadrer"), QDialogButtonBox::ResetRole);
    again->setToolTip(tr("Rendre à l'image son cadrage d'origine : centrée, recouvrant le "
                         "panneau au plus juste."));
    QPushButton *close = buttons->addButton(tr("Fermer"), QDialogButtonBox::AcceptRole);
    markDefaultButton(close);
    chrome.footer->addWidget(buttons);

    showScale();

    // Le geste dit la place, et la carte se refait derrière la fenêtre.
    m_preview->onPlaced([this](const PanelArt &art, bool settled) {
        m_recipe.panelArt = art;
        showScale();
        if (settled) Q_EMIT placed(art);
    });
    // Le champ et « Recadrer » passent par l'aperçu, et non par la recette
    // d'ici : la molette et le double-clic font déjà ces deux choses, bornes
    // comprises, et il n'y a pas deux façons de les faire. C'est le rappel de
    // l'aperçu qui ramène ensuite le placement et met le champ à jour.
    connect(m_scale, &QSpinBox::valueChanged, this, [this](int percent) {
        if (m_quiet) return;
        m_preview->setScale(percent / 100.0);
    });
    connect(again, &QPushButton::clicked, this, [this] { m_preview->refit(); });
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
}

void PlaceArtDialog::showScale() {
    const int percent = int(std::round(m_recipe.panelArt.scale * 100.0));
    if (m_scale->value() == percent) return;
    m_quiet = true;
    m_scale->setValue(percent);
    m_quiet = false;
}
