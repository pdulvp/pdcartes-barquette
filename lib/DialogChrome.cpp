#include "DialogChrome.h"

#include "Palette.h"

#include <QAbstractButton>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace {

// Le bandeau coloré le long du bord gauche d'une boîte de dialogue : une
// bande de la couleur du thème portant le titre de la boîte, assez large pour
// que le texte reste lisible à la taille employée dans le reste de la
// boîte.
constexpr int kBannerWidth = 36;

// Le bandeau lui-même, dont le texte est écrit verticalement et se lit de bas
// en haut, comme le dos d'un livre.
class EdgeBanner : public QWidget {
public:
    EdgeBanner(const QString &text, QWidget *parent) : QWidget(parent), m_text(text) {
        setFixedWidth(kBannerWidth);
        // Rien à cliquer : le bandeau est une étiquette, et un glissement
        // dessus doit atteindre ce qui se trouve en dessous.
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        // Les couleurs sont prises dans la palette du widget plutôt que
        // conservées ici : recolorer l'habillage revient alors à donner une
        // nouvelle palette à chacune de ses parties.
        painter.fillRect(rect(), palette().color(QPalette::Window));

        QFont heading = font();
        if (heading.pointSizeF() > 0) {
            heading.setPointSizeF(std::max(7.0, heading.pointSizeF() * 0.92));
        } else {
            heading.setPixelSize(std::max(9, int(heading.pixelSize() * 0.92)));
        }
        heading.setBold(true);
        heading.setCapitalization(QFont::AllUppercase);
        heading.setLetterSpacing(QFont::PercentageSpacing, 140);

        painter.setFont(heading);
        painter.setPen(palette().color(QPalette::WindowText));
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.translate(width() / 2.0, height() / 2.0);
        painter.rotate(-90);
        painter.drawText(QRectF(-height() / 2.0, -width() / 2.0, height(), width()),
                         Qt::AlignCenter, m_text);
    }

private:
    QString m_text;
};

// Les champs reçoivent un filet et un peu de marge intérieure, et la couleur
// du thème indique celui qui a le focus clavier.
QString styleSheetFor(const Palette &scheme) {
    return QStringLiteral(R"(
        QLineEdit, QComboBox, QListWidget, QAbstractScrollArea {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 5px;
            selection-background-color: %4;
            selection-color: %5;
        }
        QLineEdit, QComboBox { padding: 5px 7px; }
        QLineEdit:focus, QComboBox:focus { border: 1px solid %4; }
        QPushButton {
            background: %6;
            color: %7;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 6px 16px;
        }
        QPushButton:hover { border: 1px solid %4; }
        QPushButton:pressed { background: %8; }
        QPushButton:disabled { color: %3; }
        QPushButton[dialogDefault="true"] {
            background: %4;
            color: %5;
            border: 1px solid %4;
            font-weight: bold;
        }
        QLabel[dialogHint="true"] { color: %9; }
        /* Les cases à cocher sont dessinées ici plutôt que laissées au style
           du bureau : celui-ci les dessine dans ses propres couleurs, ce qui
           donne, sur une surface claire teintée, un résultat allant de
           l'invisible au trou noir. Une case cochée est remplie de la couleur
           du thème, ce qui est la façon la plus simple de le signaler. */
        QCheckBox { spacing: 9px; }
        QCheckBox::indicator {
            width: 15px;
            height: 15px;
            border: 1px solid %3;
            border-radius: 4px;
            background: %1;
        }
        QCheckBox::indicator:hover { border: 1px solid %4; }
        QCheckBox::indicator:checked {
            background: %4;
            border: 1px solid %4;
        }
        /* Certaines listes défilent, et une zone de défilement laissée au
           style du bureau apparaît comme un cadre gris enfoncé avec une barre
           grise sur le côté, c'est-à-dire comme la fenêtre d'un autre
           programme posée au milieu du formulaire. Son cadre et le fond de sa
           zone visible sont donc supprimés, afin que ce soit le formulaire
           lui-même qui défile. */
        QScrollArea { border: none; background: transparent; }
        QScrollArea > QWidget > QWidget { background: transparent; }
        QScrollBar:vertical {
            background: transparent;
            width: 11px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %3;
            border-radius: 5px;
            min-height: 28px;
        }
        QScrollBar::handle:vertical:hover { background: %4; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        /* Les textes longs sont affichés dans un QTextBrowser : sur la
           surface du formulaire, et sans cadre propre. */
        QTextBrowser {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 8px;
            padding: 6px 10px;
        }
        /* Les boutons radio sont dessinés ici pour la même raison que les
           cases à cocher. Celui qui est choisi est rempli plutôt que cerclé :
           un filet épais modifie la boîte sur laquelle le rayon est calculé, et
           le cercle devient un carré arrondi — c'est-à-dire la forme de la case
           à cocher voisine, qui signifie autre chose. */
        QRadioButton { spacing: 9px; }
        QRadioButton::indicator {
            width: 15px;
            height: 15px;
            border: 1px solid %3;
            /* La moitié de quinze, plus les deux pixels de filet, et pas un
               pixel de plus : Qt ignore un rayon supérieur à la moitié de la
               boîte au lieu de le ramener à cette valeur, et le cercle devient
               alors un carré. */
            border-radius: 8px;
            background: %1;
        }
        QRadioButton::indicator:hover { border: 1px solid %4; }
        QRadioButton::indicator:checked {
            background: %4;
            border: 1px solid %4;
        }
    )")
        .arg(scheme.field.name(), scheme.surfaceText.name(), scheme.edge.name(),
             scheme.accent.name(), scheme.accentText.name(), scheme.face.name(),
             scheme.faceText.name(),
             blendColours(scheme.face, scheme.surfaceText, 0.12).name(), scheme.hint.name());
}

// Le nom de chaque partie de l'habillage, afin qu'une recoloration ultérieure
// puisse les retrouver sans que la boîte ait à en conserver les pointeurs.
const char *kBannerName = "dialogBanner";
const char *kBodyName = "dialogBody";
const char *kFooterName = "dialogFooter";
const char *kRuleName = "dialogRule";

void colourChrome(QDialog *dialog, const Palette &scheme) {
    // La feuille de style est posée en premier. En reposer une fait que le
    // style « dépolit » toute la boîte et redonne à chaque widget la palette
    // qu'il avait au premier polissage : les palettes posées avant cet appel
    // seraient donc effacées.
    dialog->setStyleSheet(styleSheetFor(scheme));

    if (QWidget *banner = dialog->findChild<QWidget *>(QLatin1String(kBannerName))) {
        QPalette pal = banner->palette();
        pal.setColor(QPalette::Window, scheme.accent);
        pal.setColor(QPalette::WindowText, scheme.accentText);
        banner->setPalette(pal);
        banner->update();
    }
    if (QWidget *body = dialog->findChild<QWidget *>(QLatin1String(kBodyName))) {
        QPalette pal = body->palette();
        pal.setColor(QPalette::Window, scheme.surface);
        pal.setColor(QPalette::WindowText, scheme.surfaceText);
        body->setPalette(pal);
    }
    if (QWidget *footer = dialog->findChild<QWidget *>(QLatin1String(kFooterName))) {
        QPalette pal = footer->palette();
        pal.setColor(QPalette::Window, scheme.face);
        pal.setColor(QPalette::WindowText, scheme.faceText);
        footer->setPalette(pal);
    }
    if (QWidget *rule = dialog->findChild<QWidget *>(QLatin1String(kRuleName))) {
        rule->setStyleSheet(
            QStringLiteral("background: %1; border: none;").arg(scheme.edge.name()));
    }
}

} // namespace

DialogChrome applyDialogChrome(QDialog *dialog, const QString &bannerText) {
    auto *fields = new QWidget(dialog);
    auto *fieldsLayout = new QVBoxLayout(fields);
    fieldsLayout->setContentsMargins(kDialogMargin, kDialogMargin, kDialogMargin, kDialogMargin);
    fieldsLayout->setSpacing(kDialogRowSpacing);

    // Un filet, puis les boutons dans une bande de la couleur des boutons :
    // d'abord ce qui est à régler, ensuite ce qu'on en fait.
    auto *rule = new QFrame(dialog);
    rule->setObjectName(QLatin1String(kRuleName));
    rule->setFrameShape(QFrame::HLine);
    rule->setFixedHeight(1);

    auto *footer = new QWidget(dialog);
    footer->setObjectName(QLatin1String(kFooterName));
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(kDialogMargin, 12, kDialogMargin, 12);
    footer->setAutoFillBackground(true);

    auto *body = new QWidget(dialog);
    body->setObjectName(QLatin1String(kBodyName));
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(fields);
    bodyLayout->addWidget(rule);
    bodyLayout->addWidget(footer);
    body->setAutoFillBackground(true);

    auto *banner = new EdgeBanner(bannerText, dialog);
    banner->setObjectName(QLatin1String(kBannerName));

    auto *outer = new QHBoxLayout(dialog);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(banner);
    outer->addWidget(body);

    colourChrome(dialog, currentPalette());

    return DialogChrome{fieldsLayout, footerLayout};
}

void refreshDialogChrome(QDialog *dialog) {
    if (dialog) colourChrome(dialog, currentPalette());
}

void markDefaultButton(QAbstractButton *button) {
    if (!button) return;
    button->setProperty("dialogDefault", true);
}

void markHintLabel(QWidget *label) {
    if (!label) return;
    label->setProperty("dialogHint", true);
}

namespace {
// Le nom des deux parties d'un titre, afin qu'une recoloration ultérieure
// puisse les retrouver.
const char *kHeadingLabel = "dialogHeadingLabel";
const char *kHeadingRule = "dialogHeadingRule";
} // namespace

void recolourSectionHeadings(QWidget *root) {
    if (!root) return;
    const Palette &scheme = currentPalette();
    for (QLabel *label : root->findChildren<QLabel *>(QLatin1String(kHeadingLabel))) {
        QPalette ink = label->palette();
        ink.setColor(QPalette::WindowText, scheme.accent);
        label->setPalette(ink);
    }
    for (QFrame *rule : root->findChildren<QFrame *>(QLatin1String(kHeadingRule))) {
        QPalette pal = rule->palette();
        pal.setColor(QPalette::WindowText, scheme.edge);
        rule->setPalette(pal);
    }
}

QWidget *dialogSectionHeading(QWidget *parent, const QString &text) {
    auto *heading = new QWidget(parent);
    auto *rows = new QVBoxLayout(heading);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(4);

    auto *label = new QLabel(text, heading);
    label->setObjectName(QLatin1String(kHeadingLabel));
    QFont bold = label->font();
    bold.setBold(true);
    label->setFont(bold);
    QPalette ink = label->palette();
    // La couleur du thème, celle-là même que porte le bandeau latéral : c'est
    // ce qui rattache la boîte au programme.
    ink.setColor(QPalette::WindowText, currentPalette().accent);
    label->setPalette(ink);
    rows->addWidget(label);

    auto *rule = new QFrame(heading);
    rule->setObjectName(QLatin1String(kHeadingRule));
    rule->setFrameShape(QFrame::HLine);
    rule->setFixedHeight(1);
    QPalette pal = rule->palette();
    pal.setColor(QPalette::WindowText, currentPalette().edge);
    rule->setPalette(pal);
    rows->addWidget(rule);
    return heading;
}
