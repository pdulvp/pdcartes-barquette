#pragma once

#include <QString>

class QAbstractButton;
class QDialog;
class QHBoxLayout;
class QVBoxLayout;
class QWidget;

// L'habillage commun aux boîtes de dialogue : un bandeau de la couleur du
// thème le long du bord gauche, portant le titre écrit verticalement, les
// champs sur la surface claire du thème, et les boutons dans une bande qui
// leur est propre en bas.
//
// Une boîte de dialogue est une fenêtre à part, qui n'hérite donc pas de la
// palette appliquée à la fenêtre principale : les couleurs sont prises dans
// currentPalette(), ce qui garantit qu'une boîte ouverte par-dessus une
// fenêtre d'un thème donné porte ce même thème.

// L'espace laissé autour des champs : une boîte de dialogue est un formulaire
// à lire, et non un panneau de commande à remplir au maximum.
inline constexpr int kDialogMargin = 18;
inline constexpr int kDialogRowSpacing = 10;

// Les deux emplacements où une boîte de dialogue place ses widgets : ses
// champs dans le premier, ses boutons dans le second. Les deux dispositions
// sont déjà positionnées et colorées.
struct DialogChrome {
    QVBoxLayout *fields;
    QHBoxLayout *footer;
};

// Habille `dialog` et renvoie les deux dispositions à remplir. La fonction
// prend en charge la disposition principale de la boîte : il faut donc
// l'appeler avant d'y ajouter quoi que ce soit.
DialogChrome applyDialogChrome(QDialog *dialog, const QString &bannerText);

// Applique la palette courante à une boîte déjà habillée, pour le cas d'une
// boîte encore ouverte au moment d'un changement de mode clair ou sombre.
void refreshDialogChrome(QDialog *dialog);

// Met en évidence le bouton qui valide la boîte.
void markDefaultButton(QAbstractButton *button);

// Une ligne d'explication, par opposition à l'étiquette d'un champ : écrite
// dans la couleur de texte du thème, éclaircie, pour se lire comme une
// remarque.
void markHintLabel(QWidget *label);

// Un titre au-dessus d'un groupe de champs, souligné d'un filet. Il est écrit
// dans la couleur du thème, ce qui le signale comme titre sans avoir à
// l'agrandir.
QWidget *dialogSectionHeading(QWidget *parent, const QString &text);

// Applique la palette courante à tous les titres situés sous `root`. Une
// boîte de dialogue est reconstruite à chaque ouverture et n'en a pas besoin ;
// un menu construit une seule fois et conservé pendant toute la vie de la
// fenêtre, lui, doit voir ses titres recolorés à chaque changement de mode.
void recolourSectionHeadings(QWidget *root);
