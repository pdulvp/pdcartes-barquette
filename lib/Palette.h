#pragma once

#include <QColor>
#include <QIcon>
#include <QStringList>

// The colours the game wears, which are one table's colours.
//
// A card game is mostly cloth: a wide field of felt with a few white cards on
// it, so the whole look hangs off a single hue -- the table for two is laid in
// jade, the table for six in burgundy. Everything here is derived from that one
// number rather than written out, which is what lets another table arrive with
// nothing but a hue.
struct Palette {
    int hue;              // the one number the rest is built from, 0-359
    QColor desk;          // behind everything: the wood the table stands on
    QColor felt;          // the cloth itself, in the middle of the table
    QColor feltEdge;      // the cloth at the rim, where the light does not reach
    QColor feltWeave;     // the thread of the weave, a shade off the cloth
    QColor feltText;      // what is written straight onto the cloth
    QColor trim;          // the braid around the cloth, and the rail of the table
    QColor accent;        // the table's own colour -- banners, headings, the turn
    QColor accentText;
    QColor surface;       // panels, dialog bodies, a player's plaque
    QColor surfaceText;
    QColor field;         // what you type in, and what a list sits on
    QColor face;          // the band the buttons sit in
    QColor faceText;
    QColor cardFace;      // the paper a card is printed on
    QColor cardInk;       // the black suits, and the rank in the corner
    QColor cardRed;       // the red suits
    QColor cardBack;      // the pattern on the back of a card
    QColor cardBackInk;
    QColor highlight;     // the card under the pointer
    QColor highlightText;
    QColor edge;          // a line between a field and what it sits on
    QColor hint;          // an aside rather than a label on something
    QColor danger;        // a move that is not allowed, and a score going bad
    QColor gold;          // what is won: a record, a finished match
};

// Black or white, whichever stays readable on this colour.
QColor readableTextOn(const QColor &background);

// `weight` of `over`, the rest of `under`.
QColor blendColours(const QColor &under, const QColor &over, qreal weight);

// A desktop's own icon by name, recoloured to `ink`. The names are tried in
// order, so a desktop that has only the older spelling still gets an icon;
// null when none of them is installed.
QIcon tintedIcon(const QStringList &names, const QColor &ink);

// The scheme a hue makes, in the light or in the dark. Handed out rather than
// only applied, because the opening menu lays five tables each in its own
// colours while the application is wearing one of them.
Palette paletteForHue(int hue, bool dark);

// The scheme on show. Set as a game starts, and again whenever the dark
// button is pressed.
const Palette &currentPalette();
void setCurrentPalette(int hue);

bool darkMode();
void setDarkMode(bool dark);

// The colour the n-th spark of the firework is drawn in: the table's hue walked
// a little to one side and a little up or down in brightness, so that a sky
// full of them is a sky of tellable shades of the one family and never a colour
// from outside it.
QColor festiveColour(int index);

// A rival's own colour, readable against the felt the game is wearing. Each of
// the six has a hue of their own -- it is on their plaque, on their emblem and
// on the left edge of every combination they have laid, which is how a tapis
// of twenty combinations says whose is whose without a word.
QColor rivalColour(int hue);
