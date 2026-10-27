#include "HelpDialog.h"

#include "DialogChrome.h"
#include "Palette.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

QString sheetFor(const Palette &scheme) {
    return QStringLiteral(R"CSS(
        h1 { color: %1; font-size: 17px; margin-top: 22px; margin-bottom: 2px; }
        h2 { color: %1; font-size: 13px; margin-top: 16px; margin-bottom: 2px; }
        p { color: %2; margin-top: 6px; margin-bottom: 6px; }
        p.lead { color: %2; font-size: 13px; }
        li { color: %2; margin-bottom: 3px; }
        b { color: %1; }
        th { color: %3; text-align: left; padding-right: 14px;
             padding-top: 3px; padding-bottom: 3px; font-weight: bold; }
        td { color: %2; padding-top: 3px; padding-bottom: 3px; }
        kbd { color: %1; font-weight: bold; }
    )CSS")
        .arg(scheme.accent.name(), scheme.surfaceText.name(), scheme.hint.name());
}

const char *kPages = R"HTML(
<h1>Un jeu de cartes</h1>

<p class="lead">Un jeu de cartes est <b>un seul fichier</b>. Il contient les
cinquante-deux faces, le joker, et autant de dos qu'on veut. On le copie dans
le dossier des cartes, et les programmes de jeu qui lisent ce format le
proposent dans leurs réglages : il n'y a rien à installer, rien à compiler, et
aucune bibliothèque à charger.</p>

<p>Le fichier porte l'extension <b>.cartes</b>. Il s'échange comme n'importe
quel fichier : on le joint à un courriel, on le copie sur une clé, on le
supprime quand on n'en veut plus.</p>

<h2>Trois genres de jeux</h2>

<p>Un jeu annonce ce qu'il contient, et la grille ne montre que les cases de
ce genre-là. Le choix se fait à droite du nom.</p>

<table>
<tr><th>Jeu de 52</th><td>l'as au roi dans quatre couleurs, et le joker :
le rami, la belote à 52, la bataille</td></tr>
<tr><th>Jeu de 32</th><td>du sept à l'as, et le joker : la belote, la
manille, le piquet</td></tr>
<tr><th>Tarot</th><td>quatre couleurs de quatorze cartes — le cavalier
s'ajoute entre le valet et la dame — les vingt et un atouts, et l'excuse à la
place du joker</td></tr>
</table>

<p>Changer de genre <b>n'efface rien</b> : un tarot ramené à un jeu de 52
garde ses cavaliers et ses atouts, simplement cachés, et les retrouve si on
lui rend son genre.</p>

<p>Un jeu de 52 et un jeu de 32 s'ouvrent dans n'importe quel programme, même
écrit avant ces genres : il n'y voit qu'un jeu de 52, complet ou non. Un
<b>tarot</b>, lui, est écrit dans un format que les programmes plus anciens
refusent — ils rangeraient ses atouts par-dessus les trèfles. Les programmes
de jeu ne proposent d'ailleurs que les genres dont ils savent se servir :
<b>pdcartes-rami</b> ne liste que les jeux de 52.</p>

<h2>Le dossier des cartes</h2>

<p>C'est <b>~/.config/cartes</b>, commun à tous les programmes qui lisent ce
format : un jeu de cartes copié là est disponible dans tous, sans avoir à le
copier plusieurs fois. Chaque programme garde de son côté lequel il a choisi,
de sorte qu'on peut employer un jeu de cartes différent dans chacun.</p>

<p>Le bouton <b>Installer</b> fait la copie.</p>

<h1>Fabriquer un jeu</h1>

<p>La fenêtre a deux zones : les cases du jeu à gauche, et à droite les cartes
dont on dispose, réparties en deux onglets — <b>Les images</b>, celles qu'on a
ouvertes, et <b>Le jeu dessiné</b>, que le programme trace lui-même. Le travail
consiste à sélectionner une case, sélectionner une carte, et recommencer.</p>

<table>
<tr><th>Images…</th><td>ouvre des fichiers un par un : PNG, SVG, BMP, JPEG,
GIF</td></tr>
<tr><th>Dossier…</th><td>ouvre toutes les images d'un dossier</td></tr>
<tr><th>Bibliothèque…</th><td>extrait les images d'une DLL Windows ; voir plus
bas</td></tr>
<tr><th>Autre set…</th><td>ouvre les cartes d'un jeu existant, pour en
reprendre ; voir plus bas</td></tr>
<tr><th>clic sur une case</th><td>la sélectionne ; elle se borde de la couleur
du programme</td></tr>
<tr><th>clic sur une image</th><td>la sélectionne</td></tr>
<tr><th><b>Ranger ici</b></th><td>place l'image sélectionnée dans la case
sélectionnée</td></tr>
<tr><th><b>Découper…</b></th><td>découpe l'image sélectionnée en morceaux,
quand elle contient plusieurs cartes ; voir plus bas</td></tr>
<tr><th>double-clic sur une image</th><td>la place directement</td></tr>
<tr><th>clic droit sur une case</th><td>la vide</td></tr>
<tr><th><kbd>←</kbd> <kbd>→</kbd></th><td>passer à la case précédente ou
suivante</td></tr>
</table>

<p>Après chaque placement, la sélection <b>avance automatiquement</b> à la
case vide suivante : placer cinquante-trois images demande donc peu
d'attention. Les images déjà placées portent une pastille, afin de voir d'un
coup d'œil ce qui reste à faire.</p>

<h1>Le jeu dessiné</h1>

<p>L'onglet <b>Le jeu dessiné</b> ne demande aucun fichier : les cartes y sont
tracées par le programme, et il suffit de choisir des couleurs. C'est de quoi
obtenir un jeu complet en un clic, puis le retoucher — plutôt que de réunir
cinquante-trois images.</p>

<table>
<tr><th><b>Remplir les cases vides</b></th><td>dessine ce qui manque, sans
toucher aux images déjà placées</td></tr>
<tr><th><b>Tout remplir</b></th><td>dessine les cinquante-trois cases, en
remplaçant ce qui s'y trouve</td></tr>
<tr><th><b>Poser dans la case choisie</b></th><td>dessine la seule carte que
cette case attend</td></tr>
<tr><th><b>Ajouter le dos</b></th><td>ajoute le treillis comme dos de
plus</td></tr>
</table>

<p>Le <b>thème</b> donne toutes les couleurs d'un coup ; les pastilles en
dessous les retouchent une à une.</p>

<p><b>Panneau</b>, dans la partie <b>Le dessin</b>, est le rectangle beige que
portent les cinq cartes à motif unique : le <b>valet</b>, la <b>dame</b>, le
<b>roi</b>, le <b>joker</b> et l'<b>as</b>. Décochez-le et l'emblème est posé à
même le papier ; seul le rectangle disparaît, l'emblème gardant sa taille et sa
place. <b>Sous les symboles</b> le trace en plus sous les symboles des
cartes de deux à dix, les lettres et les symboles des coins venant par-dessus :
sur une carte imprimée, l'index du coin se lit toujours, quoi qu'il y ait
derrière.</p>

<p><b>Taille du panneau</b> donne sa largeur puis sa hauteur, en proportion de
celles qu'il a par défaut. L'emblème est inscrit dans le panneau et suit donc
ces deux facteurs ; la grille des symboles, elle, garde la disposition des jeux
imprimés et ne bouge pas — un panneau réduit laisse dépasser les symboles, ce
que l'aperçu montre aussitôt. Un panneau ne déborde jamais du papier : au-delà
de la marge de la carte, le facteur ne produit plus rien.</p>

<p><b>Taille de l'emblème</b> grossit ou réduit la couronne, le diadème, la
fleur de lis, le bonnet et le symbole de l'as, et <b>Emblèmes en verre</b> les
remplit d'un dégradé au lieu d'un aplat. Les symboles des cartes de deux à dix
restent plats : à leur taille, un dégradé ne se lirait pas.</p>

<p><b>Lettres</b> choisit celles du coin. Un jeu à enseignes françaises ne se
lit pas de la même façon selon le pays qui l'imprime :</p>

<table>
<tr><th>Français</th><td>A V C D R — as, valet, cavalier, dame, roi</td></tr>
<tr><th>Anglais</th><td>A J Kn Q K — ace, jack, knight, queen, king</td></tr>
<tr><th>Allemand</th><td>A B C D K — Ass, Bube, Cavall, Dame, König</td></tr>
<tr><th>Espagnol</th><td>1 S C D R — l'as y est chiffré, <b>C</b> est le
caballo, c'est-à-dire le cavalier, et la dame emprunte <b>D</b>, le jeu
espagnol n'en ayant pas</td></tr>
</table>

<p>Le <b>cavalier</b> ne paraît que sur un tarot, mais chaque convention a sa
lettre : une carte ne change pas de lettre selon le jeu où elle se
trouve.</p>

<p> La <b>hauteur</b> est celle des images produites :
quelques centaines de pixels suffisent, les cinquante-trois cartes partageant
le même fichier.</p>

<h2>Retoucher une carte déjà posée</h2>

<p>Une case dessinée porte une <b>pastille</b> dans son coin. Lorsqu'elle est
sélectionnée, la section <b>La carte dessinée</b> apparaît sous la grille et
règle ses couleurs à elle : la carte est redessinée à chaque changement.</p>

<p><b>Appliquer à tout</b> donne ces couleurs à toutes les cartes dessinées du
jeu, ce qui est la manière ordinaire de procéder : on règle une carte jusqu'à
ce qu'elle convienne, puis on étend le réglage. <b>Garder pour la suite</b>
reprend ces couleurs dans l'onglet, pour les cartes posées ensuite.</p>

<p>Ces réglages sont <b>conservés dans le fichier</b>, à côté des images : un
jeu rouvert ici reste donc modifiable. Les programmes de jeu, eux, n'y voient
que des images — ce qu'elles sont.</p>

<h2>Reprendre les cartes d'un autre jeu</h2>

<p><b>Autre set…</b> ouvre un fichier <b>.cartes</b> et ajoute ses faces et
ses dos à la liste des images, sans toucher au jeu en cours : de quoi reprendre
deux figures d'un jeu, les dos d'un autre, ou refaire un jeu entier à partir
d'un jeu existant. Les images arrivent sous le nom de la case qu'elles
occupaient — <b>R♥</b>, <b>A♠</b> — et s'ajoutent à celles qui étaient déjà
ouvertes.</p>

<p>Comme chacune connaît son origine, <b>Deviner la correspondance</b> les
replace toutes d'un seul coup, sans rien à vérifier. Pour n'en reprendre
qu'une, c'est la manipulation ordinaire : la case à gauche, l'image à droite,
<b>Ranger ici</b>.</p>

<h2>Les dos</h2>

<p>Les dos sont au bout de la dernière rangée, après le joker — ou après
l'excuse, dans un tarot. La case marquée
d'un <b>+</b> en ajoute un avec l'image sélectionnée ; le clic droit en enlève
un.</p>

<h2>PNG ou SVG</h2>

<p>Les deux conviennent, et les images sont conservées telles quelles : un PNG
reste un PNG, un SVG reste un SVG.</p>

<p>Préférez le <b>SVG</b> quand vous avez le choix : c'est le seul format net
à n'importe quelle taille. Une carte doit être affichée très petite sur le
plateau et beaucoup plus grande dans la main du joueur ; un PNG dimensionné
pour la main est flou sur le plateau, et un PNG dimensionné pour le plateau est
inutilement lourd.</p>

<p>La <b>proportion</b> des cartes est celle que partage la <b>plupart des
faces</b>, et l'affichage s'y conforme : des images carrées donneront un
affichage de cartes carrées. Donnez-leur donc toutes la même proportion.</p>

<p>Elle est mesurée à chaque placement, et non une fois pour toutes : une
planche placée dans une case le temps de voir ce qu'elle contient, puis
retirée, ne laisse pas sa proportion derrière elle. Un jeu fabriqué par une
version plus ancienne du programme se corrige de la même façon : il suffit de
l'ouvrir.</p>

<h1>Les DLL Windows</h1>

<p>Les jeux de cartes de Windows sont livrés depuis trente ans dans une
bibliothèque, et il n'y a aucune raison de s'en priver. Le programme l'ouvre
comme un dossier d'images : <b>rien n'est exécuté</b> — le fichier est lu, sa
table des ressources parcourue, les octets des images recopiés. Lue ainsi, une
DLL est un fichier de données comme un autre.</p>

<table>
<tr><th>NE</th><td>les bibliothèques 16 bits</td></tr>
<tr><th>PE32</th><td>les bibliothèques 32 bits</td></tr>
<tr><th>PE32+</th><td>les bibliothèques 64 bits</td></tr>
</table>

<p>Les bitmaps sont extraits avec leurs couleurs d'origine ; les PNG et les
JPEG des bibliothèques récentes sont extraits tels quels. Les ressources
contenues dans une <b>archive</b> — les bibliothèques de Windows 7 rangent
leurs images dans des fichiers .cab — ne peuvent pas être extraites, et le
programme indique combien de ressources il a ignorées.</p>

<h2>Deviner la correspondance</h2>

<p>Dans une DLL, les ressources <b>1 à 52</b> sont les cartes, rangées couleur
par couleur — treize cartes de l'as au roi, puis les treize suivantes — et les
ressources <b>53 à 64</b> sont les douze dos. Le bouton <b>Deviner la
correspondance</b> applique cet ordre d'un coup.</p>

<p>Le programme demande ensuite de vérifier la grille, et ce n'est pas une
précaution de style : l'ordre des couleurs a changé d'une version de Windows à
l'autre. Si les piques se retrouvent chez les trèfles, le plus simple est de
reprendre les quatre rangées à la main.</p>

<h2>Les planches</h2>

<p>Les jeux de cartes récents ne contiennent plus une image par carte : ils en
placent cinquante-deux, ou cent quarante, sur <b>une seule image</b> en rangées
et en colonnes. Une telle planche arrive en un seul morceau, et n'est pas
utilisable telle quelle.</p>

<p>Le bouton <b>Découper…</b> la découpe. On indique la <b>taille d'un
morceau</b> — largeur et hauteur en pixels — et le découpage se dessine sur
l'image à mesure qu'on saisit les valeurs : si les traits ne tombent pas entre
les cartes, la taille n'est pas la bonne, et cela se voit avant tout
découpage.</p>

<p>Une taille plutôt qu'un nombre de colonnes, afin que les morceaux fassent
<b>tous exactement cette taille</b>. Un nombre de colonnes divise la planche en
parts égales, et des parts égales tombent entre deux pixels dès que la largeur
de la planche n'est pas un multiple du nombre de colonnes : les cartes sortent
alors décalées d'un pixel les unes par rapport aux autres, et ce décalage ne se
corrige plus. La taille d'une carte, elle, se mesure sur la planche : la
molette, décrite plus bas, est là pour cela.</p>

<p>Ce qui reste au bord quand la taille ne tombe pas juste forme un dernier
morceau <b>plus petit</b>, dessiné en pointillé et jamais agrandi. Le compte
des morceaux l'indique également. Il s'agit presque toujours d'une marge qu'on
n'avait pas remarquée : il suffit de la déclarer en marge pour qu'elle
disparaisse.</p>

<p>Une planche entière tient dans la boîte, où une carte est donc minuscule.
La <b>molette</b> agrandit l'aperçu sous le curseur, on <b>déplace</b> la
planche en la tirant, et le <b>double-clic</b> la réaffiche en entier : de quoi
vérifier à un pixel près qu'un trait de découpe passe bien entre deux
cartes.</p>

<p><b>Rogner les bords</b> retire quelques pixels tout autour de chaque
morceau, de quoi laisser sur la planche le trait qui sépare les cartes. Un ou
deux pixels suffisent d'ordinaire.</p>

<h2>Les marges</h2>

<p>Les quatre <b>marges</b> — gauche, droite, haut, bas — sont retirées de la
planche <b>avant</b> le quadrillage, et c'est là tout leur intérêt. Une planche
contient souvent ce qui n'est pas une carte : un cadre, un filet, une bordure en
bas. Laissé dans le quadrillage, ce bord décale toutes les cartes de sa
largeur, et le découpage ne tombe plus juste. Déclaré en marge, il est exclu une
fois pour toutes et le reste s'aligne.</p>

<p>C'est pourquoi le rognage ne suffit pas : il s'applique à chaque morceau
déjà découpé, alors que la bordure a faussé le découpage lui-même. Ce que les
marges excluent est <b>grisé</b> sur l'aperçu : on voit d'un coup d'œil ce qui
sera retiré et ce qui sera conservé.</p>

<p>Une marge peut être <b>négative</b>, et c'est alors l'inverse : le
quadrillage dépasse la planche de ce côté. Les planches ne sont pas toutes bien
alignées — il manque trois pixels à gauche, la première rangée commence avant le
bord — et une marge négative replace le quadrillage où il doit être, au lieu de
décaler toutes les cartes. La partie qui dépasse de la planche n'existe pas : le
morceau du bord garde sa taille et est complété en <b>transparent</b>. La
première ou la dernière rangée est donc incomplète, mais les cartes du milieu
sont correctement alignées.</p>

<p>Une planche qui n'est qu'une <b>partie</b> d'une plus grande — les
bibliothèques découpent parfois en quatre celle qu'elles stockent en double
résolution — se découpe ainsi très bien. Le quart de 1019 × 1024 contient six
colonnes et cinq rangées entières de cartes de 146 × 196 ; le reste, 143 pixels
à droite et 44 en bas, est la moitié des cartes voisines. On peut le déclarer en
marge, ou le laisser produire ses morceaux en pointillé et ne pas les
placer : dans les deux cas, les trente cartes entières sortent correctement
alignées.</p>

<p>La planche est retirée de la liste et ses morceaux prennent sa place, dans
l'ordre de lecture : il n'y a plus qu'à les placer.</p>

<h2>Recoller les quarts</h2>

<p>Mieux que de déclarer un quart de planche en marge : le recoller. Les
bibliothèques stockent leur planche en <b>double résolution</b> découpée en
quatre, parce qu'un bitmap de deux mille pixels de côté n'était pas partout
accepté — et une carte prise sur la grande planche a quatre fois la surface de
la même carte prise sur la petite.</p>

<p>Le <b>clic droit</b> sur une image propose de la coller à l'image
sélectionnée : à gauche, à droite, au-dessus ou au-dessous. Les deux images sont
alignées sur le bord qu'elles partagent, et ce qui manque à la plus courte reste
transparent : rien n'est étiré. L'image obtenue remplace l'image sélectionnée et
<b>reste sélectionnée</b>, de sorte qu'on enchaîne aussitôt avec la
suivante.</p>

<table>
<tr><th>X21 + X22 à droite</th><td>1024 × 1024 et 1019 × 1024 donnent le haut,
2043 × 1024</td></tr>
<tr><th>X23 + X24 à droite</th><td>le bas, 2043 × 935</td></tr>
<tr><th>le bas au-dessous du haut</th><td>la planche entière,
2043 × 1959</td></tr>
</table>

<p>Trois clics droits, et elle se découpe en 14 × 10 comme n'importe quelle
autre : cent quarante cartes de 146 × 196. Les quarts sont retirés de la liste à
mesure qu'ils sont recollés, de sorte qu'il n'y reste que ce qui sert.</p>

<p>Deux mille quarante-trois pixels ne font pas quatorze colonnes entières :
les morceaux sortent à un pixel près les uns des autres. C'est sans conséquence,
puisque la proportion du jeu est celle que partage la plupart des faces et que
les programmes de jeu affichent les cartes à la taille qu'ils veulent.</p>

<h2>Les coins transparents</h2>

<p>Les cartes de Windows ont les coins arrondis, et ce qui entoure l'arrondi
est peint de la couleur de fond de l'époque. Sur un autre fond, cela fait
quatre petits carrés de couleur autour de chaque carte.</p>

<p>La case <b>Rendre les coins transparents</b> les efface. L'effacement part
des quatre coins et se propage de pixel en pixel tant que la couleur ne change
pas. Il ne supprime pas tous les pixels de cette couleur : le blanc du coin est
aussi celui du milieu de la carte, et l'effacer ferait des trous dans les
figures.</p>

<h1>Un jeu incomplet</h1>

<p>Il n'est pas obligatoire de remplir les cinquante-trois cases. Un jeu qui
n'a que ses figures, ou que ses dos, s'enregistre et s'installe comme un
autre : les programmes de jeu <b>dessinent eux-mêmes les cartes manquantes</b>.
C'est ce qui permet de ne refaire que les quatre as, si c'est tout ce qu'on
voulait refaire.</p>
)HTML";

} // namespace

HelpDialog::HelpDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Fabriquer un jeu de cartes"));

    const DialogChrome chrome = applyDialogChrome(this, tr("L'atelier"));
    const Palette &scheme = currentPalette();

    m_pages = new QTextBrowser(this);
    m_pages->setOpenExternalLinks(false);
    m_pages->document()->setDefaultStyleSheet(sheetFor(scheme));
    m_pages->setHtml(QString::fromUtf8(kPages));
    m_pages->moveCursor(QTextCursor::Start);
    chrome.fields->addWidget(m_pages, 1);

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *ok = buttons->addButton(tr("Entendu"), QDialogButtonBox::AcceptRole);
    markDefaultButton(ok);
    ok->setDefault(true);
    chrome.footer->addStretch();
    chrome.footer->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    resize(660, 720);
}
