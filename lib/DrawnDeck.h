#pragma once

#include "CardArt.h"
#include "CardSet.h"

// Le jeu dessiné, rendu en images.
//
// La cartothèque sait dessiner une carte ; ces fonctions en font des PNG, et
// de ces PNG un jeu complet. C'est ce qui permet à l'atelier de partir du jeu
// dessiné plutôt que d'images venues d'ailleurs, et à un programme de jeu
// d'exporter en fichier les cartes qu'il trace lui-même.
//
// La recette accompagne chaque image dans le jeu produit : un jeu rouvert
// dans l'atelier reste donc modifiable.

// La taille à laquelle une carte est rendue pour cette hauteur, à la
// proportion du jeu dessiné.
QSize drawnCardSize(int height);

// L'image d'une face, et celle d'un dos. `slot` est une case de `CardSet`.
// Vide si le rendu échoue, ce qui ne devrait se produire qu'à défaut de
// mémoire.
Artwork drawnFaceArt(int slot, const DrawnRecipe &how);
Artwork drawnBackArt(const DrawnRecipe &how);

// Le jeu complet : les cinquante-trois faces et un dos, avec leurs recettes.
// Le nom, l'auteur et la note sont laissés à l'appelant, qui sait pour quel
// programme il le fabrique.
CardSet drawnCardSet(const DrawnRecipe &how);
