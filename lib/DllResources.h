#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

// Extraction des images d'une bibliothèque Windows. Rien n'est exécuté : le
// fichier est lu, sa table des ressources est parcourue, et les octets des
// images sont recopiés.
//
// Trois conteneurs :
//
//   NE    16 bits (Windows 3). Table des ressources avec décalage
//         d'alignement, bitmaps à en-tête de 12 octets.
//   PE32  32 bits. Arbre de ressources à trois niveaux.
//   PE32+ 64 bits ; seul l'en-tête optionnel diffère.
//
// Les bitmaps reçoivent les 14 octets d'en-tête de fichier que Windows ne
// stocke pas avec eux ; les PNG et les JPEG sont extraits tels quels.

struct DllImage {
    // La case qu'occupait l'image dans le jeu dont elle provient, au sens de
    // la grille : 0 ou plus pour une face, -1 ou moins pour un dos. Seules
    // les images reprises d'un fichier .cartes connaissent leur case : d'une
    // bibliothèque ou d'un dossier, rien n'indique quelle carte est quelle
    // carte.
    static constexpr int kNowhere = -1000000;

    QString label;        // « bitmap 23 », ou le nom de la ressource
    int id = -1;          // numéro de la ressource, -1 si elle est nommée
    QByteArray kind;      // « bmp », « png », « jpg »
    QByteArray bytes;
    int width = 0;
    int height = 0;
    int place = kNowhere;
};

struct DllReading {
    QVector<DllImage> images;
    QString container;    // « NE », « PE32 », « PE32+ »
    // Ressources rencontrées mais non extraites : archives .cab, données qui
    // ne sont pas des images.
    int skipped = 0;
    QString note;
};

DllReading readDllImages(const QString &path, QString *why);

// Le nom d'une image réduit à sa racine : « SHEETS\HIGHLIGHTSHEET1X22.JPG »
// devient « HIGHLIGHTSHEET1X22 ». C'est le nom donné aux morceaux d'une
// planche et aux images recollées, pour lesquels un chemin complet serait trop
// long.
QString imageStem(const QString &label);

// La correspondance habituelle des DLL : les ressources 1 à 52 pour les
// cartes, couleur par couleur (trèfle, carreau, cœur, pique) de l'as au roi,
// et 53 à 64 pour les douze dos. Au-delà commencent les images de l'ancienne
// boîte de dialogue.
//
// Renvoie la case au sens de CardSet, ou -1 si la ressource n'est pas une
// carte. L'ordre des couleurs a changé d'une version de Windows à l'autre :
// c'est donc une proposition, à vérifier.
int classicCardSlotForResource(int resourceId);
bool classicResourceIsBack(int resourceId);
