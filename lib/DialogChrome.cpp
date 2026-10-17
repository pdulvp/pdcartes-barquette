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

// The coloured strip down the left of a dialog: a band of the table's colour
// with the dialog's business written up it, wide enough for the words to be
// read at the size the rest of the dialog is set in.
constexpr int kBannerWidth = 36;

// The strip itself, with the word it carries set sideways, reading upwards
// the way a book's spine does.
class EdgeBanner : public QWidget {
public:
    EdgeBanner(const QString &text, QWidget *parent) : QWidget(parent), m_text(text) {
        setFixedWidth(kBannerWidth);
        // Nothing to click: the strip is a label, and a drag on it should
        // reach whatever is underneath.
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        // Its colours come from its palette rather than being kept here, so
        // that recolouring the chrome is a matter of handing its parts new
        // palettes.
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

// Fields with an edge and a little padding, and the table's colour to say
// which one the keyboard is in.
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
        /* The switches, drawn here rather than left to the platform style: a
           desktop style hands a checkbox an indicator in its own colours, and
           on a pale tinted form that lands anywhere between invisible and a
           black hole. Filled with the table's own colour when it is on, which
           is the plainest way there is of saying so. */
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
        /* The rivals' list in the settings scrolls, and a scroll area left to
           the platform style arrives as a sunken grey box with a grey bar down
           the side -- a window from another application dropped into the
           middle of the form. Its frame and its viewport are taken off so that
           what scrolls is the form itself. */
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
        /* The rules are a long document, so the dialog that holds them is a
           text browser: on the form's own surface, with no frame of its own. */
        QTextBrowser {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 8px;
            padding: 6px 10px;
        }
        /* Radio buttons, drawn here for the same reason the switches are.
           Filled round when chosen rather than ringed: a thick border changes
           the box the radius is measured against, and the circle comes out a
           rounded square -- which is the shape next to it and means something
           else. */
        QRadioButton { spacing: 9px; }
        QRadioButton::indicator {
            width: 15px;
            height: 15px;
            border: 1px solid %3;
            /* Half of fifteen plus the two border pixels, and not a pixel
               more: Qt drops a radius larger than half the box rather than
               clamping it, and the circle comes out a square. */
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

// What each part of the chrome is called, so that recolouring it later can
// find the parts again without the dialog having to keep hold of them.
const char *kBannerName = "dialogBanner";
const char *kBodyName = "dialogBody";
const char *kFooterName = "dialogFooter";
const char *kRuleName = "dialogRule";

void colourChrome(QDialog *dialog, const Palette &scheme) {
    // The style sheet goes on first. Re-setting one makes the style unpolish
    // the whole dialog and hand every widget back the palette it had saved
    // at the first polish, so palettes set before this call would be wiped.
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

    // A rule, then the buttons in a band of the button colour: what is being
    // chosen, then what is done about it.
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
// What the two parts of a heading are called, so that recolouring one later
// can find them again.
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
    // The table's colour, which is what marks the dialog as this game's: the
    // banner down the side is the same colour.
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
