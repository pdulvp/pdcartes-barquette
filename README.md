# L'atelier des cartes

De quoi fabriquer un jeu de cartes pour les jeux de la maison — le rami
(`card-sol`) et la belote (`belottesol`) — écrit en C++17 avec Qt 6.

On y dépose des images, ou on y ouvre une DLL pour Windows, on range les
faces et les dos dans une grille, et il en sort **un fichier** à déposer dans le
dossier des cartes. Pas d'installation, pas de bibliothèque à charger : les
jeux le proposent dans leurs réglages au lancement suivant.

## Les deux moitiés du projet

| | |
|---|---|
| `lib/` | **la cartothèque** : le format, la liste des jeux installés, et le style que les deux jeux recopiaient chacun de son côté |
| `src/` | **l'atelier** : le programme qui écrit les fichiers que la cartothèque lit |

La cartothèque est une **bibliothèque statique**. Elle entre dans le binaire de
qui l'emploie : `ramisol`, `belotesol` et `cardset-creator` restent chacun un
exécutable et rien d'autre, et rien n'est installé à côté d'eux.

Un programme s'en sert depuis un dossier voisin :

```cmake
add_subdirectory(../cardset-creator/lib cartotheque)
target_link_libraries(monjeu PRIVATE cartotheque)
```

et les en-têtes s'incluent par leur nom — `#include "CardSet.h"`,
`#include "Palette.h"` — comme avant qu'ils soient partagés.

## Le format

Un jeu de cartes est **un fichier**, extension `.cartes`. Il porte les
cinquante-deux faces, le joker, autant de dos qu'on veut, et le nom de qui l'a
fait. Il s'échange comme n'importe quel fichier.

```
« CARTES »       6 octets, pour reconnaître le fichier
quint16          la version du format
QByteArray       tout le reste, compressé
```

et le reste est un `QDataStream` : le nom, l'auteur, la note, la taille
naturelle d'une face, puis les faces (rang, couleur, genre, octets) et les dos
(nom, genre, octets). Les images sont gardées **telles qu'elles arrivent** — un
PNG reste un PNG, un SVG reste un SVG.

La **taille naturelle** est celle que portent la plupart des faces, mesurée sur
elles à chaque rangement et à chaque ouverture. C'est de là que les jeux
tiennent la forme de leurs cartes : un jeu qui annonce une taille presque
carrée fait un tapis de cartes presque carrées, même quand ses images, elles,
sont bien des cartes. Une planche posée un instant dans une place puis reprise
ne laisse donc pas sa forme derrière elle, et un jeu écrit par un atelier plus
ancien se rattrape à la simple ouverture.

Les rangs vont de 1 (l'as) à 13 (le roi), zéro pour le joker ; les couleurs de
0 à 3 dans l'ordre pique, cœur, carreau, trèfle. Chaque programme traduit sa
propre numérotation en arrivant : la belote compte son as à 14 parce que c'est
là qu'il est dans ce jeu-là, et le format n'a pas à connaître les règles de
personne.

## Le dossier des cartes

`~/.config/cartes`, et il est **commun** à tous les jeux de la maison : un jeu
déposé là se retrouve dans le rami et dans la belote sans qu'on ait à le
déposer deux fois. Ce que chaque jeu garde pour lui, c'est lequel il a choisi —
rien n'empêche de jouer au rami avec un jeu et à la belote avec un autre.

Le bouton **Installer** fait le dépôt tout seul.

## Fabriquer un jeu

Les places du jeu à gauche, les images qu'on a sous la main à droite.

| | |
|---|---|
| **Images…** | ouvre des fichiers un par un : PNG, SVG, BMP, JPEG, GIF |
| **Dossier…** | ouvre toutes les images d'un dossier d'un coup |
| **Bibliothèque…** | sort les images d'une DLL pour Windows |
| **Autre set…** | prend les cartes d'un `.cartes` déjà fait, pour en copier ici |
| clic sur une place | la désigne |
| clic sur une image | la désigne |
| clic droit sur une image | la colle à celle qui est désignée, d'un côté ou de l'autre |
| **Ranger ici**, ou double-clic | met l'image désignée dans la place désignée |
| **Découper…** | débite une planche de plusieurs cartes en morceaux d'une taille donnée |
| **Marges** | met le cadre d'une planche dehors avant de la quadriller |
| clic droit sur une place | la vide |
| `←` `→` | passer d'une place à la suivante |
| `Ctrl+S` / `Ctrl+O` | enregistrer / ouvrir |
| `F1` | l'aide, qui dit tout ceci dans la fenêtre |

Après chaque rangement la place désignée **avance toute seule** à la suivante
qui est vide, et les images déjà rangées portent une pastille : ranger
cinquante-trois images est un travail de suite, pas cinquante-trois décisions.

Les dos sont au bout de la dernière rangée, après le joker ; la case **+** en
ajoute un.

**Autre set…** met les faces et les dos d'un jeu déjà fait sous la main sans
toucher à celui qu'on tient : de quoi reprendre deux figures d'un jeu et les
dos d'un autre. Ces images-là savent la place qu'elles tenaient, et **Deviner
la correspondance** les y remet toutes d'un coup.

Rien n'oblige à tout remplir. Un jeu qui n'a que ses figures s'installe comme
un autre : **les jeux dessinent eux-mêmes ce qui manque.**

## Les DLL pour Windows

Certaines DLL peuvent contenir des images de cartes.

| | |
|---|---|
| NE | les bibliothèques 16 bits |
| PE32 | le portage 32 bits |
| PE32+ | le même en 64 bits |

Les bitmaps en sortent avec les quatorze octets d'en-tête que Windows ne range
pas avec eux ; les PNG et les JPEG des bibliothèques récentes sortent tels
quels. Ce qui est enfermé dans une archive — les bibliothèques de Windows 7
gardent leurs jeux dans des cabinets — n'en sort pas, et l'atelier dit combien
de ressources il a laissées de côté.

**Deviner la correspondance** applique l'ordre connu : ressources 1 à 52 pour
les cartes, couleur par couleur de l'as au roi, et 53 à 64 pour les douze dos.
Il dit ensuite de vérifier la grille, et ce n'est pas une formule : l'ordre des
couleurs n'a pas toujours été le même d'une version à l'autre.

**Rendre les coins transparents** efface la couleur du tapis d'époque autour
des coins arrondis. Elle part des quatre coins et efface de proche en proche
tant que la couleur ne change pas — et non partout où cette couleur se trouve :
le blanc du coin est aussi celui du milieu de la carte.

## Les planches

Les jeux de cartes récents ne rangent plus une image par carte : ils en mettent
cinquante-deux, ou cent quarante, sur **une seule image** en rangées et en
colonnes — le `CardGames.dll` de Windows 7 n'a que des planches.

**Découper…** débite l'image désignée. On lui donne la **taille d'un morceau**
— largeur et hauteur en pixels — et le découpage se dessine sur l'image pendant
qu'on écrit : des traits qui ne tombent pas entre les cartes disent que la
taille n'est pas la bonne avant qu'on ait découpé. **Rogner les bords** retire
quelques pixels tout autour de chaque morceau, de quoi laisser derrière soi le
trait qui sépare les cartes.

Une taille plutôt qu'un nombre de colonnes : les morceaux font alors **tous
exactement cette taille-là**. Un nombre de colonnes partage la planche en parts
égales, et des parts égales tombent entre deux pixels dès que la planche ne se
divise pas juste — les cartes sortent décalées d'un pixel les unes des autres,
et le décalage ne se rattrape plus. Ce qui reste au bord fait un dernier
morceau plus petit, dessiné en pointillé, jamais agrandi, et annoncé en toutes
lettres sous les champs : c'est presque toujours une marge qu'on n'avait pas
vue.

L'aperçu se regarde de près : la **molette** grossit sous le curseur, on
**tire** la planche pour aller voir ailleurs, le **double-clic** la rend
entière. Une planche de cinquante-deux cartes tient dans la boîte, et un trait
de découpe qui passe à deux pixels d'une carte ne se voit pas autrement.

Les quatre **marges** se prennent sur la planche *avant* le quadrillage, et
c'est tout l'intérêt : ce qui n'est pas une carte — un cadre, un filet, la
rangée coupée en deux au bas d'un quart de planche — sort du compte au lieu de
décaler toutes les cartes de sa largeur. Une bordure laissée dans le
quadrillage ne se rattrape pas après coup ; le rognage, lui, travaille sur
chaque morceau et n'y peut rien.

Une marge peut être **négative** : le quadrillage déborde alors la planche de
ce côté-là. Les planches ne sont pas toutes d'aplomb — il manque trois pixels à
gauche, la première rangée commence avant le bord — et une marge négative remet
le quadrillage en place au lieu de décaler toutes les cartes. Ce qui dépasse la
planche sort **transparent** : le morceau du bord garde sa taille, et seules la
première rangée ou la dernière en pâtissent. Ce que les marges mettent dehors
est voilé sur l'image, de sorte qu'on voit ce qui part.

Ainsi la planche à double résolution que les bibliothèques coupent en quatre :
le quart de 1019 × 1024 porte six colonnes et cinq rangées entières de cartes
de 146 × 196, et le reste — 143 pixels à droite, 44 en bas — est la moitié des
cartes voisines. En marge, ou laissé à ses morceaux en pointillé qu'on ne
rangera pas : les trente cartes entières sortent d'aplomb.

La planche s'en va, ses morceaux prennent sa place sous la main dans l'ordre de
lecture, et il n'y a plus qu'à les ranger.

## Recoller les quarts d'une planche

Les bibliothèques gardent leurs planches à **double résolution** coupées en
quatre quarts, parce qu'un bitmap de deux mille pixels de côté ne passait pas
partout. Un quart ne se découpe pas, ou mal : ses bords traversent des cartes.

Le **clic droit** sur une image propose de la coller à celle qui est désignée —
à gauche, à droite, au-dessus, au-dessous. Les deux sont alignées sur le bord
qu'elles partagent, et ce qui manque à la plus courte reste transparent : rien
n'est étiré. L'image recollée prend la place de celle qui était désignée et
reste désignée, de sorte que le collage se reprend de proche en proche.

Les quatre quarts du `CardGames.dll` de Windows 7 s'y remontent en trois
gestes :

| | |
|---|---|
| `…X21` + `…X22` **à droite** | 1024 × 1024 et 1019 × 1024 font le haut, 2043 × 1024 |
| `…X23` + `…X24` **à droite** | le bas, 2043 × 935 |
| le bas **au-dessous** du haut | la planche entière, 2043 × 1959 |

et elle se découpe en 14 × 10 comme n'importe quelle autre, en cartes de
146 × 196 — quatre fois la surface des 73 × 98 de la petite planche. Les quarts
s'en vont à mesure qu'ils sont recollés.

Deux mille quarante-trois pixels ne font pas quatorze colonnes rondes : les
morceaux sortent à un pixel près les uns des autres, et c'est sans
conséquence — la taille du jeu est celle que porte la plupart d'entre eux, et
les jeux dessinent les cartes à la taille qu'ils veulent.

## Partir du jeu dessiné

Le plus commode pour commencer est de partir de celui du rami :

```sh
ramisol --exporter-cartes mon-jeu.cartes
cardset-creator mon-jeu.cartes
```

On remplace les quatre as, on enregistre, on installe.

## Construire

```sh
cmake -S . -B build
cmake --build build
./build/cardset-creator
```

Un paquet Debian : `cmake --build build --target deb`. Il porte l'heure du
build derrière un `~` — `cardset-creator_1.0.0~261024225112_amd64.deb` — ce qui
le classe *avant* la version vers laquelle il va, et le laisse donc remplacer
par elle.

Le paquet d'une version, nommé par son numéro seul et sans empreinte dedans :
`cmake --build build --target release`.

Le greffon Qt qui lit les SVG (`qt6-svg-plugins`) n'est pas une dépendance
dure — un jeu en PNG marche sans — mais sans lui un jeu vectoriel est une table
de cartes blanches.

## Le code

| | |
|---|---|
| `lib/CardSet.cpp` | le format : écrire, relire, et rendre une face à la taille voulue |
| `lib/CardDeck.cpp` | les jeux installés, celui qui est choisi, le dos choisi |
| `lib/DllResources.cpp` | la lecture des bibliothèques Windows, NE et PE |
| `lib/Palette.cpp` | toutes les couleurs, dérivées d'une seule teinte |
| `lib/DialogChrome.cpp` | l'habillage que les boîtes de dialogue partagent |
| `src/FaceGrid.cpp` | les places d'un jeu : quatre rangées de treize, le joker, les dos |
| `src/SourceTray.cpp` | les images qu'on a sous la main |
| `src/SplitDialog.cpp` | le découpage d'une planche en morceaux |
| `src/AtelierWindow.cpp` | la fenêtre, et le geste entre les deux surfaces |
| `src/HelpDialog.cpp` | comment on fabrique un jeu, en entier, dans la fenêtre |
