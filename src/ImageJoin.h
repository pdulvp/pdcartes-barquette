#pragma once

#include "DllResources.h"

class QString;

// Recoller deux images en une.
//
// Les bibliothèques récentes gardent leurs planches à double résolution
// coupées en quatre quarts, et un quart ne se découpe pas d'aplomb : ses
// rangées ne tombent pas rondes, et ses bords traversent des cartes. Recollés,
// les quatre quarts refont la planche entière, qui se découpe comme n'importe
// quelle autre.
enum class JoinSide { Left, Right, Above, Below };

// `other` posée du côté dit de `anchor`.
//
// Les deux sont alignées sur le bord qu'elles partagent — le haut pour un
// collage de côté, la gauche pour un collage en hauteur — et ce qui manque à
// la plus courte des deux reste transparent : deux quarts d'une même planche
// ont la même hauteur, et deux images qui n'ont rien à voir ne sont pas
// étirées pour faire semblant.
//
// Rend une image vide, et dit pourquoi, si l'une des deux ne se lit pas ou si
// le collage ferait une image démesurée.
DllImage joinImages(const DllImage &anchor, const DllImage &other, JoinSide side, QString *why);
