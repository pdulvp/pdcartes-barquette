#pragma once

#include "DllResources.h"

class QString;

// Recoller deux images en une.
//
// Les bibliothèques récentes stockent leurs planches en double résolution
// découpées en quatre quarts, et un quart se découpe mal : ses rangées ne
// tombent pas juste, et ses bords traversent des cartes. Recollés, les quatre
// quarts reconstituent la planche entière, qui se découpe comme n'importe
// quelle autre.
enum class JoinSide { Left, Right, Above, Below };

// `other` placée du côté indiqué de `anchor`.
//
// Les deux images sont alignées sur le bord qu'elles partagent — le haut pour
// un collage latéral, la gauche pour un collage vertical — et ce qui manque à
// la plus courte reste transparent : deux quarts d'une même planche ont la
// même hauteur, et deux images sans rapport ne sont pas étirées pour le
// paraître.
//
// Renvoie une image vide, et renseigne `why`, si l'une des deux n'est pas
// lisible ou si le collage produirait une image démesurée.
DllImage joinImages(const DllImage &anchor, const DllImage &other, JoinSide side, QString *why);
