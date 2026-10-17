#pragma once

#include <QString>

class QAbstractButton;
class QDialog;
class QHBoxLayout;
class QVBoxLayout;
class QWidget;

// The look the dialogs share: a strip of the table's colour down the
// left-hand edge with the dialog's business written up it, the fields on the
// table's pale surface, and the buttons in a band of their own along the
// bottom.
//
// A dialog is a window of its own, so it inherits none of the palette the
// table is wearing: the colours come from currentPalette() instead, which is
// what keeps a dialog opened over a burgundy table burgundy.

// Room the fields breathe in: a dialog is a form to be read, not a control
// panel to be packed.
inline constexpr int kDialogMargin = 18;
inline constexpr int kDialogRowSpacing = 10;

// Where a dialog puts its own widgets: its fields go in the one, its buttons
// in the other. Both layouts are already laid out and coloured.
struct DialogChrome {
    QVBoxLayout *fields;
    QHBoxLayout *footer;
};

// Dresses `dialog` and returns the two layouts to fill. Takes over the
// dialog's top-level layout, so call it before anything else is added.
DialogChrome applyDialogChrome(QDialog *dialog, const QString &bannerText);

// Puts the scheme that is current now on a dialog already dressed, for a
// dialog still open when the dark button is pressed.
void refreshDialogChrome(QDialog *dialog);

// Draws the eye to the button that finishes the dialog.
void markDefaultButton(QAbstractButton *button);

// A line of explanation rather than a label on something: set in the table's
// ink, lightened, so it reads as an aside.
void markHintLabel(QWidget *label);

// A heading over a group of fields, with a rule under it: the table's own
// colour, which is what marks it as a heading without a larger type size.
QWidget *dialogSectionHeading(QWidget *parent, const QString &text);

// Puts the scheme that is current now on every heading under `root`. A dialog
// is built fresh each time it is opened and does not need this; the opening
// menu is built once and lives as long as the window does, so when the dark
// button is pressed its headings have to be told.
void recolourSectionHeadings(QWidget *root);
