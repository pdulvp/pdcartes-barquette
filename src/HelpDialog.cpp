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

<p class="lead">Un jeu de cartes est <b>un fichier</b>, et un seul. Il porte
les cinquante-deux faces, le joker, et autant de dos qu'on veut. On le dépose
dans le dossier des cartes et les jeux de la maison le proposent dans leurs
réglages — il n'y a rien à installer, rien à compiler, et aucune bibliothèque
à charger.</p>

<p>Le fichier porte l'extension <b>.cartes</b>. Il s'échange comme n'importe
quel fichier : on le met dans un courriel, on le copie sur une clé, on
l'efface quand on en a assez.</p>

<h2>Le dossier des cartes</h2>

<p>C'est <b>~/.config/cartes</b>, et il est commun à tous les jeux de la
maison : un jeu de cartes déposé là se retrouve dans le rami et dans la
belote sans qu'on ait à le déposer deux fois. Ce que chaque jeu garde pour
lui, c'est lequel il a choisi — rien n'empêche de jouer au rami avec un jeu et
à la belote avec un autre.</p>

<p>Le bouton <b>Installer</b> fait le dépôt tout seul.</p>

<h1>Fabriquer un jeu</h1>

<p>Il y a deux surfaces : les places du jeu à gauche, les images qu'on a sous
la main à droite. Tout le travail consiste à désigner une place, désigner une
image, et recommencer.</p>

<table>
<tr><th>Images…</th><td>ouvre des fichiers un par un : PNG, SVG, BMP, JPEG,
GIF</td></tr>
<tr><th>Dossier…</th><td>ouvre toutes les images d'un dossier d'un coup</td></tr>
<tr><th>Bibliothèque…</th><td>sort les images d'une DLL pour Windows ; voir
plus bas</td></tr>
<tr><th>Autre set…</th><td>prend les cartes d'un jeu déjà fait pour en copier
ici ; voir plus bas</td></tr>
<tr><th>clic sur une place</th><td>la désigne ; elle se borde de la couleur de
l'atelier</td></tr>
<tr><th>clic sur une image</th><td>la désigne</td></tr>
<tr><th><b>Ranger ici</b></th><td>met l'image désignée dans la place
désignée</td></tr>
<tr><th><b>Découper…</b></th><td>débite l'image désignée en morceaux, quand
elle porte plusieurs cartes ; voir plus bas</td></tr>
<tr><th>double-clic sur une image</th><td>la même chose, en un geste</td></tr>
<tr><th>clic droit sur une place</th><td>la vide</td></tr>
<tr><th><kbd>←</kbd> <kbd>→</kbd></th><td>passer d'une place à la suivante</td></tr>
</table>

<p>Après chaque rangement, la place désignée <b>avance toute seule</b> à la
suivante qui est encore vide : ranger cinquante-trois images est un travail de
suite, pas cinquante-trois décisions. Les images déjà rangées portent une
pastille, pour qu'on voie d'un coup d'œil ce qui reste.</p>

<h2>Copier les cartes d'un Autre set</h2>

<p><b>Autre set…</b> ouvre un fichier <b>.cartes</b> et met ses faces et ses
dos sous la main, sans toucher au jeu en cours : c'est de quoi reprendre deux
figures d'un jeu, les dos d'un autre, ou refaire un jeu entier à partir d'un
jeu voisin. Les images arrivent sous le nom de la place qu'elles tenaient —
<b>R♥</b>, <b>A♠</b> — et s'ajoutent à ce qui était déjà là.</p>

<p>Comme chacune sait d'où elle vient, <b>Deviner la correspondance</b> les
remet toutes à leur place d'un seul coup, sans rien avoir à vérifier. Pour
n'en copier qu'une, c'est le travail ordinaire : la place à gauche, l'image à
droite, <b>Ranger ici</b>.</p>

<h2>Les dos</h2>

<p>Les dos sont au bout de la dernière rangée, après le joker. La case marquée
d'un <b>+</b> en ajoute un avec l'image désignée ; le clic droit en enlève un.</p>

<h2>PNG ou SVG</h2>

<p>Les deux marchent, et les images sont gardées telles qu'elles arrivent — un
PNG reste un PNG, un SVG reste un SVG.</p>

<p>Préférez le <b>SVG</b> quand vous avez le choix : c'est le seul qui soit net
à n'importe quelle taille. Une carte doit être dessinée toute petite sur le
tapis et beaucoup plus grande dans la main, et un PNG qui fait l'affaire dans
la main est flou sur le tapis, ou lourd pour rien.</p>

<p>La <b>forme</b> des cartes est celle que portent la <b>plupart des
faces</b>, et le tapis s'y range : des cartes carrées feront une table de
cartes carrées. Donnez-leur toutes la même proportion.</p>

<p>Elle se mesure à chaque rangement, et non une fois pour toutes : une
planche posée dans une place le temps de voir ce qu'elle porte, puis reprise,
ne laisse pas sa forme derrière elle. Un jeu fabriqué par un atelier plus
ancien se rattrape de la même façon — il suffit de l'ouvrir.</p>

<h1>Les DLL pour Windows</h1>

<p>Les jeux de cartes de Windows sont livrés depuis trente ans dans une
bibliothèque, et il n'y a aucune raison de s'en priver. L'atelier l'ouvre
comme un dossier d'images : <b>rien n'est exécuté</b> — on ouvre le fichier,
on suit sa table des ressources, on en sort les octets des images. Lue de
cette façon, une DLL est un fichier de données comme un autre.</p>

<table>
<tr><th>NE</th><td>les bibliothèques 16 bits</td></tr>
<tr><th>PE32</th><td>le portage 32 bits</td></tr>
<tr><th>PE32+</th><td>le même en 64 bits</td></tr>
</table>

<p>Les bitmaps en sortent avec leurs couleurs d'époque ; les PNG et les JPEG
des bibliothèques récentes sortent tels quels. Ce qui est enfermé dans une
<b>archive</b> — les bibliothèques de Windows 7 gardent leurs jeux dans des
cabinets — n'en sort pas, et l'atelier dit combien de ressources il a laissées
de côté.</p>

<h2>Deviner la correspondance</h2>

<p>Dans une DLL, les ressources <b>1 à 52</b> sont les cartes, rangées
couleur par couleur — treize cartes de l'as au roi, puis les treize
suivantes — et <b>53 à 64</b> sont les douze dos. Le bouton
<b>Deviner la correspondance</b> applique cet ordre d'un coup.</p>

<p>Il dit ensuite de vérifier la grille, et ce n'est pas une formule : l'ordre
des couleurs n'a pas toujours été le même d'une version à l'autre. Si les
piques se retrouvent chez les trèfles, le plus simple est de reprendre les
quatre rangées à la main — c'est quatre fois treize clics, et c'est juste.</p>

<h2>Les planches</h2>

<p>Les jeux de cartes récents ne rangent plus une image par carte : ils en
mettent cinquante-deux, ou cent quarante, sur <b>une seule image</b> en
rangées et en colonnes. Une planche pareille arrive sous la main en un seul
morceau, et il n'y a rien à en faire telle quelle.</p>

<p>Le bouton <b>Découper…</b> la débite. On lui dit la <b>taille d'un
morceau</b> — sa largeur et sa hauteur en pixels — et le découpage se dessine
sur l'image pendant qu'on écrit : si les traits ne tombent pas entre les
cartes, c'est que la taille n'est pas la bonne, et on le voit avant d'avoir
découpé.</p>

<p>Une taille plutôt qu'un nombre de colonnes : les morceaux font alors
<b>tous exactement cette taille-là</b>. Un nombre de colonnes partage la
planche en parts égales, et des parts égales tombent entre deux pixels dès que
la planche ne se divise pas juste — les cartes sortent décalées d'un pixel les
unes des autres, et ce décalage ne se rattrape plus. La taille d'une carte,
elle, se lit sur la planche : la molette, ci-dessous, est là pour cela.</p>

<p>Ce qui reste au bord quand la taille ne tombe pas juste fait un dernier
morceau <b>plus petit</b>, dessiné en pointillé et jamais agrandi. Le compte
des morceaux le dit aussi, en toutes lettres. Neuf fois sur dix c'est une
marge qu'on n'avait pas vue : on la met en marge, justement, et elle s'en
va.</p>

<p>Une planche entière tient dans la boîte, et une carte y est donc minuscule.
La <b>molette</b> grossit l'aperçu sous le curseur, on <b>tire</b> la planche
pour aller voir ailleurs, et le <b>double-clic</b> la rend entière : c'est de
quoi vérifier à un pixel près qu'un trait de découpe passe bien entre deux
cartes.</p>

<p><b>Rogner les bords</b> retire quelques pixels tout autour de chaque
morceau — de quoi laisser sur la planche le trait qui sépare les cartes. Un ou
deux pixels suffisent d'ordinaire.</p>

<h2>Les marges</h2>

<p>Les quatre <b>marges</b> — gauche, droite, haut, bas — se prennent sur la
planche <b>avant</b> qu'elle soit quadrillée, et c'est là tout leur intérêt.
Une planche porte souvent ce qui n'est pas une carte : un cadre, un filet, une
bordure en bas. Laissé dans le quadrillage, ce bord-là décale toutes les
cartes de sa largeur, et le découpage ne tombe plus nulle part. Mis en marge,
il s'en va une fois pour toutes et le reste tombe d'aplomb.</p>

<p>C'est pourquoi le rognage n'y suffit pas : il travaille sur chaque morceau
déjà découpé, quand la bordure, elle, a faussé le découpage lui-même. Ce que
les marges mettent dehors est <b>voilé</b> sur l'image : on voit d'un coup
d'œil ce qui part et ce qui reste.</p>

<p>Une marge peut être <b>négative</b>, et c'est alors l'inverse : le
quadrillage déborde la planche de ce côté-là. Les planches ne sont pas toutes
d'aplomb — il manque trois pixels à gauche, la première rangée commence avant
le bord — et une marge négative remet le quadrillage où il doit être au lieu
de décaler toutes les cartes. Ce qui dépasse la planche n'existe pas : le
morceau du bord garde sa taille et se complète de <b>transparent</b>. La
première rangée ou la dernière y perd quelque chose, mais les cartes du milieu
sortent d'aplomb.</p>

<p>Une planche qui n'est qu'un <b>bout</b> d'une plus grande — les
bibliothèques coupent parfois en quatre celle qu'elles tiennent à double
résolution — se découpe ainsi très bien. Le quart de 1019 × 1024 porte six
colonnes et cinq rangées entières de cartes de 146 × 196 ; le reste, 143
pixels à droite et 44 en bas, est la moitié des cartes voisines. On peut le
mettre en marge, ou le laisser faire ses morceaux en pointillé et ne pas les
ranger : dans les deux cas les trente cartes entières sortent d'aplomb.</p>

<p>La planche s'en va et ses morceaux prennent sa place sous la main, dans
l'ordre de lecture : il n'y a plus qu'à les ranger.</p>

<h2>Recoller les quarts</h2>

<p>Mieux encore que de mettre un quart de planche en marge : le recoller. Les
bibliothèques gardent leur planche à <b>double résolution</b> coupée en quatre,
parce qu'un bitmap de deux mille pixels de côté ne passait pas partout — et une
carte prise sur la grande planche a quatre fois la surface de la même prise sur
la petite.</p>

<p>Le <b>clic droit</b> sur une image propose de la coller à celle qui est
désignée : à gauche, à droite, au-dessus, au-dessous. Les deux images sont
alignées sur le bord qu'elles partagent et ce qui manque à la plus courte reste
transparent — rien n'est étiré pour faire semblant. L'image recollée prend la
place de celle qui était désignée et <b>reste désignée</b>, de sorte qu'on
reprend aussitôt avec la suivante.</p>

<table>
<tr><th>X21 + X22 à droite</th><td>1024 × 1024 et 1019 × 1024 : le haut,
2043 × 1024</td></tr>
<tr><th>X23 + X24 à droite</th><td>le bas, 2043 × 935</td></tr>
<tr><th>le bas au-dessous du haut</th><td>la planche entière,
2043 × 1959</td></tr>
</table>

<p>Trois clics droits, et elle se découpe en 14 × 10 comme n'importe quelle
autre : cent quarante cartes de 146 × 196. Les quarts s'en vont à mesure qu'ils
sont recollés, et il ne reste sous la main que ce qui sert.</p>

<p>Deux mille quarante-trois pixels ne font pas quatorze colonnes rondes : les
morceaux sortent à un pixel près les uns des autres. C'est sans conséquence —
la forme du jeu est celle que porte la plupart des faces, et les jeux dessinent
les cartes à la taille qu'ils veulent.</p>

<h2>Les coins transparents</h2>

<p>Les cartes de Windows ont les coins arrondis, et ce qu'il y a autour de
l'arrondi est peint de la couleur du tapis de l'époque. Sur un autre tapis,
cela fait quatre petits carrés de couleur autour de chaque carte.</p>

<p>La case <b>Rendre les coins transparents</b> les efface. Elle part des
quatre coins et efface de proche en proche tant que la couleur ne change pas —
de proche en proche, et non partout où cette couleur se trouve : le blanc du
coin est aussi le blanc du milieu de la carte, et effacer celui-là ferait des
trous dans les figures.</p>

<h1>Un jeu incomplet</h1>

<p>Rien n'oblige à remplir les cinquante-trois places. Un jeu qui n'a que ses
figures, ou que ses dos, s'enregistre et s'installe comme un autre : les jeux
de la maison <b>dessinent eux-mêmes ce qui manque</b>. C'est ce qui permet de
ne refaire que les quatre as si c'est tout ce qu'on voulait refaire.</p>
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
