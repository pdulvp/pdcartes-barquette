#include "CardArt.h"

#include <QBuffer>
#include <QImageReader>

// Pour la numérotation des rangs et des couleurs, et les chiffres romains des
// atouts. `CardSet.h` inclut `CardArt.h`, et non l'inverse : l'inclure ici ne
// fait donc pas de cycle, et évite de recopier deux constantes qui
// divergeraient.
#include "CardSet.h"

#include <QBrush>
#include <QCoreApplication>
#include <QFont>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QTransform>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace {

constexpr qreal kPi = 3.14159265358979323846;

// Les rangs, selon la nomenclature du jeu français. La valeur zéro désigne le
// joker, qui n'a pas de couleur.
constexpr int kJoker = 0;
constexpr int kAs = 1;
constexpr int kValet = 11;
constexpr int kDame = 12;
constexpr int kRoi = 13;

// Les seize lettres des quatre conventions, dans l'ordre de `RankLetters` :
// l'as, le valet, la dame et le roi. Une convention de plus tient en une
// ligne de plus.
struct LetterSet {
    const char *ace;
    const char *jack;
    const char *knight;  // le cavalier, que seul un tarot porte
    const char *queen;
    const char *king;
    const char *name;
};

// Les cinq lettres d'une convention : l'as, le valet, le cavalier, la dame et
// le roi. Le nom affiché montre les quatre dernières, celles qui distinguent
// une convention d'une autre.
//
// La dame espagnole porte `D` et non `C` : `C` est le caballo, c'est-à-dire
// le cavalier, et un tarot espagnol montrerait sinon deux `C`. Le jeu
// espagnol n'ayant pas de dame, `D` est un emprunt — mais un emprunt sans
// ambiguïté.
const LetterSet kLetterSets[kRankLettersCount] = {
    {"A", "V", "C", "D", "R", "Français (V C D R)"},
    {"A", "J", "Kn", "Q", "K", "Anglais (J Kn Q K)"},
    {"A", "B", "C", "D", "K", "Allemand (B C D K)"},
    {"1", "S", "C", "D", "R", "Espagnol (S C D R)"},
};

const LetterSet &letterSet(RankLetters letters) {
    const int at = int(letters);
    if (at < 0 || at >= kRankLettersCount) return kLetterSets[0];
    return kLetterSets[at];
}

// Les couleurs, dans l'ordre de `CardSet`.
constexpr int kPique = 0;
constexpr int kCoeur = 1;
constexpr int kCarreau = 2;
constexpr int kTrefle = 3;

// Rayon d'arrondi des coins d'une carte, en proportion de sa largeur.
//
// La valeur est faible, pour une raison qui n'apparaît qu'avec un jeu lu dans
// un fichier : une carte provenant d'une image possède déjà ses propres
// coins. Les fichiers `CARDS.DLL` de Windows comportent ainsi un filet noir
// situé à deux pixels du bord sur soixante et onze, soit moins de trois
// centièmes de la largeur. Appliquer par-dessus un arrondi plus large
// couperait ce filet et laisserait un fragment visible à chaque coin.
//
// Cette valeur convient donc à tous les cas : elle suit le filet des cartes
// de Windows, n'entame pas un jeu dont les coins sont déjà transparents, et
// adoucit un jeu aux coins vifs. Les cartes dessinées l'emploient également,
// afin qu'un jeu lu dans un fichier et le jeu dessiné présentent la même
// découpe.
constexpr qreal kCornerPart = 0.035;

// Les symboles de couleur sont tracés et non composés.
//
// Ces symboles existent dans toutes les polices, mais leur forme et leur
// graisse y varient, et une police dépourvue du caractère affiche un
// rectangle de substitution. Tracés par le programme, ils ont une apparence
// constante.

// Côté de la boîte carrée où les quatre symboles sont inscrits.
//
// C'est l'encombrement qu'avait le trèfle, le plus ample des quatre.
constexpr qreal kPipBox = 1.72;

// L'emprise verticale des deux motifs qui peuvent être en verre, dans leur
// repère local : celle d'un emblème, et celle d'un symbole de couleur, lequel
// est inscrit dans un carré et est donc symétrique.
constexpr qreal kEmblemTop = -0.95;
constexpr qreal kEmblemBottom = 0.85;
constexpr qreal kPipTop = -kPipBox / 2.0;
constexpr qreal kPipBottom = kPipBox / 2.0;

QPainterPath buildPipPath(int suit) {
    QPainterPath path;
    switch (suit) {
    case kCoeur: {
        // Deux lobes et une pointe, formés d'une courbe de chaque côté.
        path.moveTo(0.0, 0.92);
        path.cubicTo(-0.86, 0.22, -0.60, -0.58, -0.26, -0.58);
        path.cubicTo(-0.06, -0.58, 0.0, -0.36, 0.0, -0.22);
        path.cubicTo(0.0, -0.36, 0.06, -0.58, 0.26, -0.58);
        path.cubicTo(0.60, -0.58, 0.86, 0.22, 0.0, 0.92);
        path.closeSubpath();
        break;
    }
    case kCarreau: {
        path.moveTo(0.0, -0.95);
        path.cubicTo(0.26, -0.44, 0.52, -0.16, 0.66, 0.0);
        path.cubicTo(0.52, 0.16, 0.26, 0.44, 0.0, 0.95);
        path.cubicTo(-0.26, 0.44, -0.52, 0.16, -0.66, 0.0);
        path.cubicTo(-0.52, -0.16, -0.26, -0.44, 0.0, -0.95);
        path.closeSubpath();
        break;
    }
    case kPique: {
        // Un cœur renversé, surmontant une tige.
        //
        // Les abscisses sont plus écartées que celles du cœur, pour une
        // hauteur plus grande : le pique s'effile vers sa pointe, et à
        // largeur égale il paraissait le plus étroit des quatre symboles.
        // Seules les abscisses ont été écartées ; les ordonnées sont celles
        // d'avant, le symbole n'ayant pas à monter.
        path.moveTo(0.0, -0.95);
        path.cubicTo(1.10, -0.18, 0.79, 0.52, 0.33, 0.52);
        // La taille, où le corps rejoint la tige. Elle est courte : c'est un
        // rebroussement, qu'un long trajet ferait voir comme un ergot.
        path.cubicTo(0.13, 0.52, 0.07, 0.48, 0.04, 0.40);
        path.lineTo(0.28, 0.90);
        path.lineTo(-0.28, 0.90);
        path.lineTo(-0.04, 0.40);
        path.cubicTo(-0.07, 0.48, -0.13, 0.52, -0.33, 0.52);
        path.cubicTo(-0.79, 0.52, -1.10, -0.18, 0.0, -0.95);
        path.closeSubpath();
        break;
    }
    case kTrefle: {
        // Trois lobes qui se recouvrent, et une tige qui s'évase vers le bas.
        //
        // Les lobes sont réunis au lieu d'être ajoutés les uns aux autres :
        // la règle de remplissage par parité évide l'intersection de deux
        // sous-tracés, et trois disques ajoutés ensemble ne se joignent donc
        // que s'ils ne se touchent pas. C'est cette contrainte qui laissait
        // auparavant un jour entre eux.
        //
        // La réunion est faite à l'échelle cent, puis ramenée à l'unité.
        // `QPainterPath::united()` remplace les courbes par des segments, et
        // la finesse de ce découpage est rapportée aux coordonnées du tracé :
        // réunis dans le carré unité, les trois disques revenaient en
        // polygones aux facettes visibles dès que le symbole dépassait la
        // vingtaine de pixels.
        constexpr qreal k = 100.0;
        path.addEllipse(QPointF(0.0, -0.40 * k), 0.41 * k, 0.41 * k);
        QPainterPath lobe;
        lobe.addEllipse(QPointF(-0.45 * k, 0.19 * k), 0.41 * k, 0.41 * k);
        path = path.united(lobe);
        lobe = QPainterPath();
        lobe.addEllipse(QPointF(0.45 * k, 0.19 * k), 0.41 * k, 0.41 * k);
        path = path.united(lobe);

        // La tige part d'au-dessus du point où les trois lobes se rejoignent.
        // Les trois disques laissent entre eux un creux qu'aucun ne couvre ;
        // une tige partant d'en dessous y laisserait une encoche blanche au
        // milieu du trèfle.
        QPainterPath stem;
        stem.moveTo(0.085 * k, -0.05 * k);
        stem.cubicTo(0.12 * k, 0.42 * k, 0.25 * k, 0.72 * k, 0.36 * k, 0.90 * k);
        stem.lineTo(-0.36 * k, 0.90 * k);
        stem.cubicTo(-0.25 * k, 0.72 * k, -0.12 * k, 0.42 * k, -0.085 * k, -0.05 * k);
        stem.closeSubpath();
        path = path.united(stem);
        path = QTransform::fromScale(1.0 / k, 1.0 / k).map(path);
        break;
    }
    default:
        return path;
    }

    // Le symbole est ramené à la boîte commune.
    //
    // Tracés un à un, les quatre n'avaient ni la même largeur ni la même
    // hauteur : le cœur était de loin le plus étroit, le carreau le plus
    // haut. Or c'est leur encombrement qu'on compare lorsqu'ils sont posés
    // côte à côte sur une carte, et non la boîte dans laquelle chacun a été
    // tracé. Les tracés ci-dessus décrivent donc une forme, et non une
    // taille : celle-ci est donnée ici, la même pour les quatre.
    const QRectF bounds = path.boundingRect();
    if (bounds.width() <= 0.0 || bounds.height() <= 0.0) return path;
    path = QTransform::fromScale(kPipBox / bounds.width(), kPipBox / bounds.height()).map(path);
    const QRectF fitted = path.boundingRect();
    return QTransform::fromTranslate(-fitted.center().x(), -fitted.center().y()).map(path);
}

// Les quatre tracés, construits une fois.
//
// Celui du trèfle réunit quatre formes, calcul qui n'a pas à être refait pour
// chacun des dix symboles d'une carte, à chaque rafraîchissement.
const QPainterPath &pipPath(int suit) {
    static const QVector<QPainterPath> kShapes = [] {
        QVector<QPainterPath> made;
        for (int at = 0; at < 4; ++at) made.append(buildPipPath(at));
        return made;
    }();
    static const QPainterPath none;
    if (suit < 0 || suit >= kShapes.size()) return none;
    return kShapes.at(suit);
}

// Le pinceau d'un motif : un aplat, ou le dégradé qui lui donne l'aspect du
// verre.
//
// Le dégradé est donné dans le repère local du motif, celui où le carré unité
// va de -1 à 1 : il traverse donc tous les morceaux d'un même emblème sans
// raccord, quelle que soit la place de chacun, et il suit la taille du motif
// sans qu'il y ait rien à recalculer.
//
// Il est porté par le pinceau, et non repassé après coup sur l'emblème fini :
// un reflet translucide repassé supposerait de connaître la silhouette de
// l'emblème, dont les morceaux se recouvrent — la croix sur la couronne, la
// perle sur le diadème —, et laisserait une couture sur chaque recouvrement.
// `top` et `bottom` sont la hauteur qu'occupe réellement le motif dans son
// repère local, et non le carré unité : une rampe plus haute que le dessin
// garde ses deux extrémités hors de celui-ci, et la lumière renvoyée par le
// bas ne se voit jamais. Un emblème et un symbole de couleur n'ayant pas la
// même emprise, chacun donne la sienne.
QBrush glassBrush(const QColor &ink, bool glassy, qreal top, qreal bottom) {
    if (!glassy) return QBrush(ink);
    QLinearGradient sheen(QPointF(0.0, top), QPointF(0.0, bottom));
    sheen.setColorAt(0.00, blendColours(ink, QColor(Qt::white), 0.62));
    sheen.setColorAt(0.30, blendColours(ink, QColor(Qt::white), 0.28));
    sheen.setColorAt(0.48, ink);
    // La cassure du milieu, nette : c'est elle qui fait lire le dégradé comme
    // un reflet plutôt que comme une ombre.
    sheen.setColorAt(0.52, blendColours(ink, QColor(Qt::black), 0.24));
    sheen.setColorAt(0.86, blendColours(ink, QColor(Qt::black), 0.06));
    // La lumière renvoyée par en dessous, qui détache le bas de l'emblème.
    sheen.setColorAt(1.00, blendColours(ink, QColor(Qt::white), 0.34));
    return QBrush(sheen);
}

// La colonne du coin : sa largeur, et la hauteur du rang qui s'y écrit, en
// part de la carte.
//
// Elles sont nommées ici parce que deux dessins les emploient : le coin
// lui-même, et la grille des symboles, dont le bord gauche est posé au-delà
// de cette colonne.
constexpr qreal kIndexWidthPart = 0.175;
constexpr qreal kIndexHeightPart = 0.155;

// La marge intérieure de la carte : le coin y est posé, et c'est elle qui
// borne le panneau. Elle est étroite : une marge large laissait la carte vide
// en son milieu.
QRectF innerMarginOf(const QRectF &box) {
    return box.adjusted(box.width() * 0.045, box.height() * 0.040,
                        -box.width() * 0.045, -box.height() * 0.040);
}

// Un rectangle de panneau porté à la taille demandée, autour de son centre,
// puis ramené dans la marge du papier.
//
// Les facteurs sont bornés, et la taille obtenue l'est une seconde fois par
// la marge : un panneau ne déborde pas de la carte, quel que soit le facteur
// qu'on lui donne.
QRectF panelSized(const QRectF &base, const QRectF &box, const CardStyle &style) {
    // La carte entière, et non sa marge : un panneau porté à sa plus grande
    // taille prend la carte d'un bord à l'autre, sans laisser de papier
    // autour.
    const QRectF limit = box;
    const qreal wide = std::clamp(style.panelWidth, kPanelSmallest, kPanelLargest);
    const qreal tall = std::clamp(style.panelHeight, kPanelSmallest, kPanelLargest);
    QRectF out(0.0, 0.0, std::min(base.width() * wide, limit.width()),
               std::min(base.height() * tall, limit.height()));
    out.moveCenter(base.center());
    // Le panneau du joker n'est pas centré en hauteur sur la carte, le mot
    // « Joker » s'écrivant en dessous : porté à sa plus grande taille, il sort
    // par le haut, et il est alors ramené.
    if (out.left() < limit.left()) out.moveLeft(limit.left());
    if (out.right() > limit.right()) out.moveRight(limit.right());
    if (out.top() < limit.top()) out.moveTop(limit.top());
    if (out.bottom() > limit.bottom()) out.moveBottom(limit.bottom());
    return out;
}

// Le rectangle du panneau, le même pour les quatre figures : le valet, la
// dame, le roi et l'as. Le joker a le sien, un peu moins haut, le mot
// « Joker » s'écrivant en dessous, et les cartes de deux à dix le leur, déduit
// de la grille des symboles qu'il porte.
QRectF emblemPanelBox(const QRectF &box, const CardStyle &style) {
    const QRectF base(box.left() + box.width() * 0.235, box.top() + box.height() * 0.155,
                      box.width() * 0.53, box.height() * 0.69);
    return panelSized(base, box, style);
}

// La taille d'un symbole sur une carte de deux à dix, et le rectangle que
// forment les *centres* de ceux des quatre coins.
//
// Le bord gauche de la grille est posé au-delà de la colonne du coin, moitié
// d'un symbole comprise, afin qu'aucun symbole ne vienne sous le rang ni sous
// le symbole du coin.
qreal pipSpanOf(const QRectF &box) {
    return std::min(box.width() * 0.170, box.height() * 0.132);
}

QRectF pipGridOf(const QRectF &box) {
    const qreal column = innerMarginOf(box).left() + box.width() * kIndexWidthPart +
                         box.width() * 0.014 + pipSpanOf(box) / 2.0;
    return QRectF(column, box.top() + box.height() * 0.180,
                  box.width() - 2.0 * (column - box.left()), box.height() * 0.640);
}

// Le panneau des cartes de deux à dix. La grille ne donne que les centres des
// symboles : il faut la rendre à la place qu'ils occupent vraiment — une
// moitié de symbole de chaque côté — puis ajouter le peu de beige qu'il faut
// autour pour que ce soit une assise et non un fond rempli à ras.
QRectF pipPanelBox(const QRectF &box, const CardStyle &style) {
    const qreal bezel = pipSpanOf(box) * 0.70;
    return panelSized(pipGridOf(box).adjusted(-bezel, -bezel, bezel, bezel), box, style);
}

// Le panneau d'un dos : le rectangle qu'occupe son treillis. La marge est
// prise sur la largeur dans les quatre sens, de sorte qu'elle soit la même en
// haut et sur le côté.
QRectF backPanelBox(const QRectF &box, const CardStyle &style) {
    return panelSized(box.adjusted(box.width() * 0.085, box.width() * 0.085,
                                   -box.width() * 0.085, -box.width() * 0.085),
                      box, style);
}

// Le panneau du joker : un peu moins haut que celui des figures, le mot
// « Joker » s'écrivant en dessous.
QRectF jokerPanelBox(const QRectF &box, const CardStyle &style) {
    return panelSized(QRectF(box.left() + box.width() * 0.235, box.top() + box.height() * 0.150,
                             box.width() * 0.53, box.height() * 0.640),
                      box, style);
}

// L'arrondi d'un panneau.
//
// `own` est celui qu'il a en propre — marqué pour le panneau d'une face,
// discret pour celui d'un dos. Il le garde tant qu'il occupe moins de 90 pour
// cent de la carte, puis rejoint celui de la carte à mesure qu'il la remplit.
//
// Sans cela, un panneau porté à la taille de la carte laisserait quatre
// éclats de papier dans les coins s'il est le plus arrondi des deux, ou
// déborderait du bord arrondi de la carte s'il l'est le moins. Aucun panneau
// d'un jeu déjà enregistré n'atteint le seuil : le plus grand, celui du dos,
// occupe 83 pour cent de la carte.
qreal panelRoundOf(const QRectF &panel, const QRectF &card, qreal own) {
    if (card.isEmpty()) return own;
    const qreal fill = std::min(panel.width() / card.width(), panel.height() / card.height());
    const qreal part = std::clamp((fill - 0.90) / 0.10, 0.0, 1.0);
    return own + (cardCornerFor(card) - own) * part;
}

// Un panneau est-il assez grand pour se lire ? En dessous, la carte montre
// son motif à même le papier.
bool panelFits(const QRectF &panel) {
    return panel.width() > 11.0 && panel.height() > 14.0;
}

// Le rectangle où l'image d'un panneau est posée : recouvrant le panneau au
// plus juste lorsque l'échelle vaut un, puis portée au facteur demandé et
// décalée de ce qu'on lui a demandé.
QSizeF panelArtSize(const QRectF &panel, const PanelArt &art) {
    const QSizeF size = art.image.size();
    if (size.isEmpty()) return {};
    const qreal cover = std::max(panel.width() / size.width(), panel.height() / size.height());
    const qreal factor = cover * std::clamp(art.scale, kPanelArtSmallest, kPanelArtLargest);
    return QSizeF(size.width() * factor, size.height() * factor);
}

QRectF panelArtBox(const QRectF &panel, const PanelArt &art) {
    const QSizeF drawn = panelArtSize(panel, art);
    if (drawn.isEmpty()) return panel;
    const QPointF reach = panelArtReach(panel, art);
    QRectF out(QPointF(), drawn);
    out.moveCenter(panel.center() +
                   QPointF(std::clamp(art.offset.x(), -reach.x(), reach.x()) * panel.width(),
                           std::clamp(art.offset.y(), -reach.y(), reach.y()) * panel.height()));
    return out;
}

// Le carré où s'inscrit l'emblème que porte un panneau.
QRectF emblemFieldOf(const QRectF &panel) {
    return panel.adjusted(panel.width() * 0.12, panel.height() * 0.10,
                          -panel.width() * 0.12, -panel.height() * 0.10);
}

// Le symbole d'une couleur, au pinceau demandé. `drawPip()` n'expose que
// l'aplat : seul l'as emploie un dégradé, et c'est ici qu'il est dessiné.
void paintPip(QPainter *painter, const QPointF &middle, qreal span, int suit,
              const QBrush &ink) {
    painter->save();
    painter->translate(middle);
    painter->scale(span / 2.0, span / 2.0);
    painter->setPen(Qt::NoPen);
    painter->setBrush(ink);
    painter->drawPath(pipPath(suit));
    painter->restore();
}

// Le panneau des cartes à emblème : un rectangle beige arrondi, cerné d'un
// filet à la couleur de la carte. Seul au milieu du papier, un emblème
// flottait ; le panneau lui donne une assise, et la marge blanche qui
// l'entoure reste celle de la carte.
//
// Les trois figures et le joker le portent, et le portent identique : ce sont
// les quatre cartes d'un jeu qui ne montrent pas de symboles, et elles se
// reconnaissent d'autant mieux qu'elles se ressemblent.
void paintEmblemPanel(QPainter *painter, const QRectF &box, const QRectF &card,
                      const QColor &ink, const QColor &panel, const PanelArt &art = {}) {
    const qreal round = panelRoundOf(box, card, std::min(box.width(), box.height()) * 0.11);
    painter->save();
    painter->setPen(QPen(blendColours(panel, ink, 0.26), std::max(1.0, box.width() * 0.020)));
    painter->setBrush(panel);
    painter->drawRoundedRect(box, round, round);
    if (!art.isEmpty()) {
        // L'image vient à la place de la couleur de fond, et de rien d'autre :
        // elle est rognée à l'arrondi du panneau, et le filet est retracé
        // par-dessus, la moitié intérieure de son trait venant d'être
        // recouverte.
        QPainterPath clip;
        clip.addRoundedRect(box, round, round);
        painter->save();
        painter->setClipPath(clip);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawImage(panelArtBox(box, art), art.image);
        painter->restore();
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(box, round, round);
    }
    painter->restore();
}

// Les figures. Elles ne sont pas représentées par des portraits, illisibles à
// quarante pixels, mais par un emblème propre à chacune, tracé dans la
// couleur de la carte : une couronne pour le roi, un diadème pour la dame,
// une fleur de lis pour le valet.
//
// Les quelques traits évidés de l'emblème — le bandeau de la couronne, celui
// du diadème, la ceinture de la fleur de lis — sont tracés dans la couleur du
// panneau, et se découpent donc sur le beige et non sur le blanc. Lorsque le
// panneau n'est pas tracé, c'est sur le papier qu'ils se découpent.
void paintCourtMark(QPainter *painter, const QRectF &box, const QRectF &card, int rank,
                    const QColor &ink, const CardStyle &style, const PanelArt &art) {
    if (style.panelled) paintEmblemPanel(painter, box, card, ink, style.panel, art);
    // L'emblème effacé, il ne reste que le panneau et ce qu'il porte.
    if (!style.emblemShown) return;
    const QColor hollow = style.panelled ? style.panel : style.paper;

    // L'emblème, à l'intérieur du panneau. Sa taille suit le facteur demandé ;
    // sa place, elle, ne bouge pas : il reste centré sur le panneau.
    const QRectF field = emblemFieldOf(box);
    const qreal span = std::min(field.width(), field.height()) *
                       std::clamp(style.emblemSize, kEmblemSmallest, kEmblemLargest);
    painter->save();
    painter->translate(field.center());
    painter->scale(span / 2.0, span / 2.0);
    painter->setPen(QPen(ink, 0.08, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QBrush body = glassBrush(ink, style.glassy, kEmblemTop, kEmblemBottom);
    painter->setBrush(body);

    if (rank == kRoi) {
        // Couronne du roi : cinq pointes surmontées d'une croix, qui la
        // distingue immédiatement du diadème de la dame.
        QPainterPath crown;
        crown.moveTo(-0.72, 0.34);
        crown.lineTo(-0.86, -0.46);
        crown.lineTo(-0.34, -0.06);
        crown.lineTo(0.0, -0.56);
        crown.lineTo(0.34, -0.06);
        crown.lineTo(0.86, -0.46);
        crown.lineTo(0.72, 0.34);
        crown.closeSubpath();
        painter->drawPath(crown);
        painter->drawRect(QRectF(-0.09, -0.92, 0.18, 0.34));
        painter->drawRect(QRectF(-0.24, -0.80, 0.48, 0.16));
        painter->setBrush(hollow);
        painter->drawRect(QRectF(-0.72, 0.44, 1.44, 0.18));
        painter->setBrush(body);
        painter->drawRect(QRectF(-0.78, 0.40, 1.56, 0.26));
    } else if (rank == kDame) {
        // Diadème de la dame : trois pointes, plus basses et plus arrondies
        // que celles de la couronne, terminées chacune par une perle.
        QPainterPath tiara;
        tiara.moveTo(-0.66, 0.34);
        tiara.quadTo(-0.62, -0.18, -0.44, -0.30);
        tiara.lineTo(-0.22, 0.04);
        tiara.quadTo(0.0, -0.34, 0.0, -0.44);
        tiara.quadTo(0.0, -0.34, 0.22, 0.04);
        tiara.lineTo(0.44, -0.30);
        tiara.quadTo(0.62, -0.18, 0.66, 0.34);
        tiara.closeSubpath();
        painter->drawPath(tiara);
        for (const qreal at : {-0.44, 0.0, 0.44}) {
            const qreal top = (at == 0.0) ? -0.56 : -0.42;
            painter->drawEllipse(QRectF(at - 0.13, top - 0.13, 0.26, 0.26));
        }
        painter->setBrush(hollow);
        painter->drawRect(QRectF(-0.66, 0.42, 1.32, 0.16));
        painter->setBrush(body);
        painter->drawRect(QRectF(-0.72, 0.38, 1.44, 0.24));
    } else if (rank == CardSet::kKnightRank) {
        // Cavalier : une tête de cheval de profil, tournée vers la gauche.
        //
        // C'est le cavalier qu'on reconnaît le plus vite à quarante pixels :
        // un homme à cheval s'y réduirait à une tache, là où une tête de
        // cheval garde une silhouette que rien d'autre dans le jeu n'évoque.
        QPainterPath horse;
        horse.moveTo(-0.84, 0.18);                                  // le bout du museau
        horse.cubicTo(-0.76, 0.38, -0.60, 0.46, -0.42, 0.42);       // les lèvres, le menton
        horse.cubicTo(-0.18, 0.38, -0.06, 0.52, -0.02, 0.78);       // la gorge
        horse.lineTo(0.54, 0.78);                                   // le bas de l'encolure
        horse.cubicTo(0.52, 0.24, 0.42, -0.16, 0.28, -0.44);        // la nuque
        horse.lineTo(0.36, -0.80);                                  // l'oreille arrière
        horse.lineTo(0.08, -0.54);
        horse.lineTo(0.02, -0.88);                                  // l'oreille avant
        horse.lineTo(-0.18, -0.48);
        horse.cubicTo(-0.44, -0.38, -0.70, -0.14, -0.80, 0.02);     // le chanfrein
        horse.closeSubpath();
        painter->drawPath(horse);

        // L'œil et le naseau, évidés comme le bandeau de la couronne.
        painter->setBrush(hollow);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(QPointF(-0.20, -0.14), 0.085, 0.085);
        painter->drawEllipse(QPointF(-0.66, 0.16), 0.055, 0.055);
    } else {
        // Valet : une fleur de lis.
        QPainterPath lis;
        lis.moveTo(0.0, -0.80);
        lis.cubicTo(0.18, -0.46, 0.22, -0.26, 0.14, -0.10);
        lis.cubicTo(0.46, -0.44, 0.84, -0.20, 0.52, 0.22);
        lis.cubicTo(0.40, 0.00, 0.22, 0.02, 0.14, 0.14);
        lis.lineTo(0.30, 0.74);
        lis.lineTo(-0.30, 0.74);
        lis.lineTo(-0.14, 0.14);
        lis.cubicTo(-0.22, 0.02, -0.40, 0.00, -0.52, 0.22);
        lis.cubicTo(-0.84, -0.20, -0.46, -0.44, -0.14, -0.10);
        lis.cubicTo(-0.22, -0.26, -0.18, -0.46, 0.0, -0.80);
        lis.closeSubpath();
        painter->drawPath(lis);
        painter->setBrush(hollow);
        painter->drawRect(QRectF(-0.34, 0.30, 0.68, 0.10));
    }
    painter->restore();
}

// Joker : un bonnet à grelots surmontant une étoile.
void paintJoker(QPainter *painter, const QRectF &box, const CardStyle &style) {
    const qreal span = std::min(box.width(), box.height()) *
                       std::clamp(style.emblemSize, kEmblemSmallest, kEmblemLargest);
    painter->save();
    painter->translate(box.center());
    painter->scale(span / 2.0, span / 2.0);

    const QBrush cloth = glassBrush(style.cap, style.glassy, kEmblemTop, kEmblemBottom);
    const QBrush bell = glassBrush(style.bell, style.glassy, kEmblemTop, kEmblemBottom);

    QPainterPath hat;
    hat.moveTo(-0.70, 0.18);
    hat.cubicTo(-0.60, -0.48, 0.60, -0.48, 0.70, 0.18);
    hat.closeSubpath();
    painter->setPen(Qt::NoPen);
    painter->setBrush(cloth);
    painter->drawPath(hat);

    // Les trois pointes du bonnet, terminées chacune par un grelot.
    const QPointF tips[3] = {QPointF(-0.92, -0.42), QPointF(0.0, -0.80), QPointF(0.92, -0.42)};
    const QPointF roots[3] = {QPointF(-0.52, -0.14), QPointF(0.0, -0.34), QPointF(0.52, -0.14)};
    for (int i = 0; i < 3; ++i) {
        QPainterPath horn;
        horn.moveTo(roots[i].x() - 0.16, roots[i].y() + 0.10);
        horn.quadTo((roots[i].x() + tips[i].x()) / 2.0 - 0.10, tips[i].y() + 0.10, tips[i].x(),
                    tips[i].y());
        horn.quadTo((roots[i].x() + tips[i].x()) / 2.0 + 0.10, tips[i].y() + 0.26,
                    roots[i].x() + 0.16, roots[i].y() + 0.10);
        horn.closeSubpath();
        painter->setBrush(cloth);
        painter->drawPath(horn);
        painter->setBrush(bell);
        painter->drawEllipse(QRectF(tips[i].x() - 0.15, tips[i].y() - 0.15, 0.30, 0.30));
    }

    // Le bonnet n'a pas de bandeau. Il en portait un, doré, large comme le
    // bonnet : posé en travers du dessin, il se lisait comme un cadre jaune
    // plutôt que comme la coiffe d'un bonnet.

    // L'étoile placée sous le bonnet, symbole du joker.
    QPainterPath star;
    for (int i = 0; i < 10; ++i) {
        const qreal angle = -kPi / 2.0 + i * kPi / 5.0;
        const qreal reach = (i % 2 == 0) ? 0.44 : 0.19;
        const QPointF at(std::cos(angle) * reach, 0.66 + std::sin(angle) * reach);
        if (i == 0) {
            star.moveTo(at);
        } else {
            star.lineTo(at);
        }
    }
    star.closeSubpath();
    painter->setBrush(cloth);
    painter->drawPath(star);
    painter->restore();
}

// Disposition des symboles sur les cartes de deux à dix : celle qu'emploient
// les jeux de cartes depuis deux siècles.
//
// Les positions sont données dans le carré unité du *centre* des symboles :
// `{0, 0}` est le centre du symbole en haut à gauche, `{1, 1}` celui d'en bas
// à droite. Elles ne décrivent donc pas une zone à remplir mais une grille de
// points, ce qui permet d'écarter ou de resserrer la grille sans retoucher
// dix lignes de nombres — et c'est ce qu'il a fallu faire pour dégager les
// deux colonnes de symboles des index du coin.
const QVector<QPointF> &pipLayout(int rank) {
    constexpr qreal kThird = 1.0 / 3.0;
    static const QVector<QVector<QPointF>> kLayouts = {
        {},                                                               // 0
        {{0.50, 0.50}},                                                   // as
        {{0.50, 0.00}, {0.50, 1.00}},                                     // 2
        {{0.50, 0.00}, {0.50, 0.50}, {0.50, 1.00}},                       // 3
        {{0.00, 0.00}, {1.00, 0.00}, {0.00, 1.00}, {1.00, 1.00}},         // 4
        {{0.00, 0.00}, {1.00, 0.00}, {0.50, 0.50}, {0.00, 1.00}, {1.00, 1.00}},
        {{0.00, 0.00}, {1.00, 0.00}, {0.00, 0.50}, {1.00, 0.50}, {0.00, 1.00}, {1.00, 1.00}},
        {{0.00, 0.00}, {1.00, 0.00}, {0.50, 0.25}, {0.00, 0.50}, {1.00, 0.50}, {0.00, 1.00},
         {1.00, 1.00}},
        {{0.00, 0.00}, {1.00, 0.00}, {0.50, 0.25}, {0.00, 0.50}, {1.00, 0.50}, {0.50, 0.75},
         {0.00, 1.00}, {1.00, 1.00}},
        {{0.00, 0.00}, {1.00, 0.00}, {0.00, kThird}, {1.00, kThird}, {0.50, 0.50},
         {0.00, 1.0 - kThird}, {1.00, 1.0 - kThird}, {0.00, 1.00}, {1.00, 1.00}},
        {{0.00, 0.00}, {1.00, 0.00}, {0.50, 0.18}, {0.00, kThird}, {1.00, kThird},
         {0.00, 1.0 - kThird}, {1.00, 1.0 - kThird}, {0.50, 0.82}, {0.00, 1.00}, {1.00, 1.00}},
    };
    static const QVector<QPointF> none;
    if (rank < 1 || rank > 10) return none;
    return kLayouts.at(rank);
}

QFont indexFont(const QFont &base, qreal height, bool bold) {
    QFont font = base;
    font.setBold(bold);
    font.setPixelSize(std::max(7, int(std::round(height))));
    return font;
}

// Le coin : le rang, surmontant le symbole de la couleur, le tout répété une
// seconde fois tête-bêche comme sur une carte imprimée. C'est ce qui permet
// de lire une main en éventail dont seul le bord gauche de chaque carte est
// visible.
//
// Il est tracé en dernier, par-dessus ce que la carte porte : le panneau peut
// venir sous lui, et l'index du coin d'une carte imprimée se lit toujours,
// quoi qu'il y ait derrière.
//
// La colonne a la même largeur sur les treize rangs : c'est elle qui fixe le
// bord des deux colonnes de symboles, et une colonne dont la largeur
// dépendrait du rang placerait les symboles du dix ailleurs que ceux du neuf.
// Le « 10 », seul rang à deux chiffres, est donc composé un peu plus petit
// pour y tenir — comme sur une carte imprimée.
//
// `withPip` est faux pour un atout, qui n'a pas de couleur : son coin ne
// porte que son numéro.
//
// Lorsque le style porte un fond de lettres (`indexBackShown`), un carré
// arrondi est tracé derrière le rang, centré sur la colonne du coin et à la
// taille demandée : ici, en dernier, donc par-dessus le panneau, la lettre
// venant elle-même par-dessus le fond. Son filet est de la même couleur que
// celui du panneau.
void paintCornerIndex(QPainter *painter, const QRectF &box, const QString &label, int suit,
                      const QColor &ink, bool withPip, const CardStyle &style) {
    const QRectF inner = innerMarginOf(box);
    const qreal indexWidth = box.width() * kIndexWidthPart;
    const qreal indexHeight = std::max(8.0, box.height() * kIndexHeightPart);
    QFont rankFont = indexFont(plainFont(painter), indexHeight, true);
    const qreal wanted = QFontMetricsF(rankFont).horizontalAdvance(label);
    if (wanted > indexWidth) {
        rankFont = indexFont(plainFont(painter),
                             indexHeight * std::max(0.66, indexWidth / wanted), true);
    }
    painter->setFont(rankFont);

    for (int turn = 0; turn < 2; ++turn) {
        painter->save();
        if (turn == 1) {
            painter->translate(box.center());
            painter->rotate(180);
            painter->translate(-box.center());
        }
        const QRectF where(inner.left(), inner.top(), indexWidth, indexHeight);
        if (style.indexBackShown) {
            QRectF back(0.0, 0.0,
                        where.width() *
                            std::clamp(style.indexWidth, kIndexSmallest, kIndexLargest),
                        where.height() *
                            std::clamp(style.indexHeight, kIndexSmallest, kIndexLargest));
            back.moveCenter(where.center());
            const qreal round = std::min(back.width(), back.height()) * 0.24;
            // Le même filet que le panneau, à la largeur près : la couleur
            // est calculée comme celle du panneau, du beige et de l'encre.
            painter->setPen(QPen(blendColours(style.panel, ink, 0.26),
                                 std::max(1.0, back.width() * 0.020)));
            painter->setBrush(style.indexBack);
            painter->drawRoundedRect(back, round, round);
        }
        painter->setPen(ink);
        painter->drawText(where, Qt::AlignHCenter | Qt::AlignVCenter, label);
        if (withPip) {
            const qreal pip = std::min(indexWidth * 0.68, indexHeight * 0.74);
            drawPip(painter,
                    QPointF(where.center().x(),
                            where.bottom() + pip * 0.60 + box.height() * 0.012),
                    pip, suit, ink);
        }
        painter->restore();
        // Une carte trop petite pour deux coins n'en reçoit qu'un : un coin
        // lisible vaut mieux que deux illisibles.
        if (box.width() < 40) break;
    }
}

// Ce que porte le papier : le coin, les symboles ou l'emblème. Le papier et
// son filet sont tracés par l'appelant.
void paintFaceContent(QPainter *painter, const QRectF &box, int rank, int suit,
                      const CardStyle &style, const PanelArt &art) {
    // Un atout et le joker n'ont pas de couleur : ils prennent l'encre des
    // enseignes noires.
    const QColor ink = (rank != kJoker && suit != CardSet::kTrumpSuit && cardSuitIsRed(suit))
                           ? style.red
                           : style.ink;

    // La marge intérieure, d'où partent le coin et le joker.
    const QRectF inner = innerMarginOf(box);

    if (rank != kJoker && suit == CardSet::kTrumpSuit) {
        // Un atout : son numéro, en chiffres romains sur le panneau des
        // figures et en chiffres arabes dans les coins. Les vingt et une
        // figures traditionnelles -- le Bateleur, la Papesse -- ne sont pas
        // dessinées : c'est un travail d'illustration. Le numéro suffit à
        // faire un tarot utilisable, dont les atouts se remplacent ensuite
        // par de vraies images.
        const QString roman = CardSet::romanNumber(rank);
        const QRectF panel = emblemPanelBox(box, style);
        if (style.panelled && panelFits(panel)) {
            paintEmblemPanel(painter, panel, box, ink, style.panel, art);
        }
        if (panelFits(panel) && style.emblemShown) {
            const QRectF field = emblemFieldOf(panel);
            QFont number = plainFont(painter);
            number.setBold(true);
            number.setPixelSize(std::max(9, int(field.height() * 0.30 *
                                                std::clamp(style.emblemSize, kEmblemSmallest,
                                                           kEmblemLargest))));
            // Les chiffres romains des atouts vont jusqu'à quatre signes :
            // la police est resserrée jusqu'à ce que « XVIII » tienne.
            while (number.pixelSize() > 9 &&
                   QFontMetricsF(number).horizontalAdvance(roman) > field.width()) {
                number.setPixelSize(number.pixelSize() - 1);
            }
            painter->setFont(number);
            painter->setPen(ink);
            painter->drawText(field, Qt::AlignCenter, roman);
        }

        // Le coin : le numéro en chiffres arabes, tête-bêche comme le rang
        // d'une carte ordinaire. Un atout n'a pas de couleur, et son coin ne
        // porte donc pas de symbole.
        if (style.indexShown) {
            paintCornerIndex(painter, box, QString::number(rank), suit, ink, false, style);
        }
        return;
    }

    if (rank == kJoker) {
        // Le joker, ou l'excuse du tarot : c'est la même carte à la même
        // place, et le même dessin lui va. Seul le mot écrit dessous change,
        // et c'est la couleur des atouts qui le dit ; tous deux portent la
        // même étoile dans les coins.
        //
        // Le joker porte le panneau des figures : lui non plus ne montre pas
        // de symboles, et il remplissait jusqu'au bord une carte où les
        // treize autres gardent une marge. Il est un peu moins haut que celui
        // des figures, le mot « Joker » s'écrivant en dessous.
        const QRectF panel = jokerPanelBox(box, style);
        if (panelFits(panel)) {
            if (style.panelled) paintEmblemPanel(painter, panel, box, ink, style.panel, art);
            if (style.emblemShown) {
                paintJoker(painter, panel.adjusted(panel.width() * 0.10, panel.height() * 0.08,
                                                   -panel.width() * 0.10, -panel.height() * 0.08),
                           style);
            }
        } else if (style.emblemShown) {
            paintJoker(painter, inner, style);
        }
        if (style.jokerNameShown && box.width() >= 42) {
            QFont name = indexFont(plainFont(painter), box.height() * 0.085, true);
            name.setCapitalization(QFont::AllUppercase);
            name.setLetterSpacing(QFont::PercentageSpacing, 130);
            painter->setFont(name);
            painter->setPen(style.cap);
            painter->drawText(QRectF(box.left(), box.top() + box.height() * 0.820,
                                     box.width(), box.height() * 0.110),
                              Qt::AlignCenter,
                              suit == CardSet::kTrumpSuit
                                  ? QCoreApplication::translate("cardart", "Excuse")
                                  : QCoreApplication::translate("cardart", "Joker"));
        }
        // Le coin : l'étoile, qui est le rang du joker (`cardRankLabel(0)`),
        // tête-bêche comme le rang d'une carte ordinaire. C'est elle qui nomme
        // la carte dans les coins lorsque le mot en dessous est retiré. Un
        // joker n'a pas de couleur : son coin ne porte pas de symbole.
        if (style.indexShown) {
            paintCornerIndex(painter, box, cardRankLabel(kJoker, style.letters), suit, ink, false,
                             style);
        }
        return;
    }

    // Le milieu de la carte : son panneau, et ce qu'il porte. Il est tracé
    // avant le coin, lequel vient par-dessus.
    if (rank >= kValet) {
        // Les figures et l'as tiennent le centre sur presque toute la
        // largeur : sur une carte imprimée, le coin n'occupe que le coin. Ils
        // ne sont donc pas placés dans la grille des symboles, qui est
        // étroite, mais dans leur propre rectangle.
        const QRectF figure = emblemPanelBox(box, style);
        if (panelFits(figure)) {
            paintCourtMark(painter, figure, box, rank, ink, style, art);
        } else if (style.emblemShown) {
            drawPip(painter, box.center(), std::min(box.width(), box.height()) * 0.42, suit, ink);
        }
    } else if (rank == kAs) {
        // L'as compte parmi les figures : il porte le même panneau qu'elles,
        // suit la même taille d'emblème, et peut être en verre comme elles.
        //
        // Son symbole est inscrit dans le carré du panneau que ce dernier
        // soit tracé ou non, exactement comme l'emblème des trois autres
        // figures : la case à cocher ne commande que la présence du
        // rectangle. Il a brièvement gardé, sans panneau, la grande taille
        // traditionnelle au centre de la carte ; décocher la case changeait
        // alors deux choses au lieu d'une, ce qui se remarquait aussitôt.
        // C'est « Taille de l'emblème » qui règle la taille, et elle seule.
        const QBrush body = glassBrush(ink, style.glassy, kPipTop, kPipBottom);
        const qreal size = std::clamp(style.emblemSize, kEmblemSmallest, kEmblemLargest);
        const QRectF panel = emblemPanelBox(box, style);
        if (panelFits(panel)) {
            if (style.panelled) paintEmblemPanel(painter, panel, box, ink, style.panel, art);
            if (style.emblemShown) {
                const QRectF field = emblemFieldOf(panel);
                paintPip(painter, field.center(),
                         std::min(field.width(), field.height()) * size, suit, body);
            }
        } else if (style.emblemShown) {
            // Carte trop petite pour un panneau : un symbole centré, comme
            // pour les trois autres figures. À cette taille il ne s'agit plus
            // d'apparence mais de lisibilité.
            paintPip(painter, box.center(),
                     std::min(box.width() * 0.52, box.height() * 0.42) * size, suit, body);
        }
    } else {
        // La grille des symboles : le rectangle que forment les *centres* des
        // symboles des quatre coins.
        //
        // Son bord gauche est posé au-delà de la colonne du coin, moitié d'un
        // symbole comprise, afin qu'aucun symbole ne vienne sous le rang ni
        // sous le symbole du coin.
        const QVector<QPointF> &layout = pipLayout(rank);
        const qreal pipSpan = pipSpanOf(box);
        const QRectF grid = pipGridOf(box);
        if (grid.width() < 12.0 || pipSpan < 4.0 || layout.isEmpty()) {
            // Carte trop petite pour la disposition complète : un symbole
            // central indique la couleur et le coin indique le rang, ce qui
            // suffit pour une carte intégrée à une combinaison posée sur le
            // tapis.
            if (style.pipsShown) {
                drawPip(painter, box.center(), std::min(box.width(), box.height()) * 0.40, suit,
                        ink);
            }
        } else {
            // Le panneau, lorsqu'il est demandé sous les symboles. Son
            // rectangle de départ est déduit de la grille : la grille ne donne
            // que les centres des symboles, et il faut donc la rendre à la
            // place qu'ils occupent vraiment — une moitié de symbole de chaque
            // côté — puis ajouter le peu de beige qu'il faut autour pour que
            // ce soit une assise et non un fond rempli à ras.
            if (style.panelled && style.panelOnPips) {
                const QRectF panel = pipPanelBox(box, style);
                if (panelFits(panel)) paintEmblemPanel(painter, panel, box, ink, style.panel, art);
            }
            if (style.pipsShown) {
                for (const QPointF &at : layout) {
                    drawPip(painter,
                            QPointF(grid.left() + at.x() * grid.width(),
                                    grid.top() + at.y() * grid.height()),
                            pipSpan, suit, ink);
                }
            }
        }
    }

    if (style.indexShown) {
        paintCornerIndex(painter, box, cardRankLabel(rank, style.letters), suit, ink, true, style);
    }
}

} // namespace

// ---------------------------------------------------------------------------

qreal cardCornerFor(const QRectF &box) {
    return std::max(2.0, std::min(box.width(), box.height()) * kCornerPart);
}

CardStyle CardStyle::fromPalette(const Palette &scheme) {
    CardStyle style;
    style.paper = scheme.cardFace;
    style.ink = scheme.cardInk;
    style.red = scheme.cardRed;
    // Le beige du panneau est tiré de l'or de la palette : les cartes suivent
    // la teinte de la table, et un beige fixe jurerait avec elle comme en
    // mode sombre.
    style.panel = blendColours(scheme.cardFace, scheme.gold, 0.30);
    style.cap = scheme.accent;
    style.bell = scheme.gold;
    style.back = scheme.cardBack;
    style.backInk = scheme.cardBackInk;
    return style;
}

bool CardStyle::operator==(const CardStyle &other) const {
    return paper == other.paper && ink == other.ink && red == other.red &&
           panel == other.panel && cap == other.cap && bell == other.bell &&
           back == other.back && backInk == other.backInk &&
           panelled == other.panelled && panelOnPips == other.panelOnPips &&
           panelWidth == other.panelWidth && panelHeight == other.panelHeight &&
           emblemShown == other.emblemShown && pipsShown == other.pipsShown &&
           glassy == other.glassy &&
           letters == other.letters && qFuzzyCompare(emblemSize, other.emblemSize) &&
           indexShown == other.indexShown &&
           indexBackShown == other.indexBackShown && indexBack == other.indexBack &&
           indexWidth == other.indexWidth && indexHeight == other.indexHeight &&
           jokerNameShown == other.jokerNameShown;
}

QFont plainFont(const QPainter *painter) {
    QFont font = painter ? painter->font() : QFont();
    // La famille est réinitialisée au même titre que la graisse et
    // l'interlettrage : un titre composé sur le peintre laissait sinon sa
    // police d'affichage à tout ce qui était dessiné ensuite. Il en résultait
    // des zéros barrés sur les cartes et un accent manquant sur une plaque,
    // ces polices d'affichage ne comportant ni l'un ni l'autre.
    font.setFamily(QGuiApplication::font().family());
    font.setBold(false);
    font.setItalic(false);
    font.setCapitalization(QFont::MixedCase);
    font.setLetterSpacing(QFont::PercentageSpacing, 100);
    return font;
}

QString cardRankLabel(int rank, RankLetters letters) {
    const LetterSet &set = letterSet(letters);
    switch (rank) {
    case kJoker: return QStringLiteral("★");
    case kAs: return QString::fromLatin1(set.ace);
    case kValet: return QString::fromLatin1(set.jack);
    case kDame: return QString::fromLatin1(set.queen);
    case kRoi: return QString::fromLatin1(set.king);
    case CardSet::kKnightRank: return QString::fromLatin1(set.knight);
    default: break;
    }
    return QString::number(rank);
}

QString rankLettersName(RankLetters letters) {
    return QCoreApplication::translate("cardart", letterSet(letters).name);
}

bool cardSuitIsRed(int suit) { return suit == kCoeur || suit == kCarreau; }

void drawPip(QPainter *painter, const QPointF &middle, qreal span, int suit, const QColor &ink) {
    paintPip(painter, middle, span, suit, QBrush(ink));
}

void drawSuitPip(QPainter *painter, const QRectF &box, int suit, const QColor &ink) {
    drawPip(painter, box.center(), std::min(box.width(), box.height()), suit, ink);
}

QPointF panelArtReach(const QRectF &panel, const PanelArt &art) {
    if (panel.isEmpty()) return {};
    const QSizeF drawn = panelArtSize(panel, art);
    if (drawn.isEmpty()) return {};
    // L'écart entre l'image et le panneau, de moitié : c'est le décalage qui
    // amène un bord de l'image sur le bord correspondant du panneau.
    //
    // Sa valeur absolue, car la règle vaut dans les deux sens. Une image plus
    // grande que le panneau se décale jusqu'à ce que son bord arrive sur
    // celui du panneau, et pas au-delà : on ne découvre donc jamais le beige
    // derrière une image qui le recouvrait. Une image plus petite se décale
    // jusqu'à ce que son bord arrive, lui aussi, sur celui du panneau : elle
    // se pose donc où l'on veut dedans, sans jamais en sortir.
    const qreal across = (drawn.width() / panel.width() - 1.0) / 2.0;
    const qreal down = (drawn.height() / panel.height() - 1.0) / 2.0;
    return QPointF(std::abs(across), std::abs(down));
}

bool PanelArt::decode() {
    image = QImage();
    if (bytes.isEmpty()) return false;
    QBuffer source(&bytes);
    source.open(QIODevice::ReadOnly);
    QImageReader reader(&source, kind);
    image = reader.read();
    return !image.isNull();
}

QRectF cardBackPanelBox(const QRectF &box, const CardStyle &style) {
    if (box.width() < 3 || box.height() < 5) return {};
    const QRectF panel = backPanelBox(box, style);
    return panelFits(panel) ? panel : QRectF();
}

QRectF cardPanelBox(const QRectF &box, int rank, int suit, const CardStyle &style) {
    if (box.width() < 3 || box.height() < 5 || !style.panelled) return {};
    QRectF panel;
    if (rank == kJoker) {
        panel = jokerPanelBox(box, style);
    } else if (suit == CardSet::kTrumpSuit || rank >= kValet || rank == kAs) {
        // Un atout, une figure ou l'as : le panneau des figures.
        panel = emblemPanelBox(box, style);
    } else {
        // Une carte de deux à dix : seulement si on l'a demandé, et seulement
        // si la carte est assez grande pour porter la grille complète, faute
        // de quoi elle ne montre qu'un symbole central sans panneau.
        if (!style.panelOnPips) return {};
        const QRectF grid = pipGridOf(box);
        if (grid.width() < 12.0 || pipSpanOf(box) < 4.0 || pipLayout(rank).isEmpty()) return {};
        panel = pipPanelBox(box, style);
    }
    return panelFits(panel) ? panel : QRectF();
}

void drawDefaultFace(QPainter *painter, const QRectF &box, int rank, int suit,
                     const CardStyle &style, const PanelArt &art) {
    if (box.width() < 3 || box.height() < 5) return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal corner = cardCornerFor(box);
    QPainterPath shape;
    shape.addRoundedRect(box, corner, corner);

    painter->setPen(Qt::NoPen);
    painter->fillPath(shape, style.paper);

    // Le filet du bord : sans lui, une carte claire posée sur un fond clair
    // n'a pas de contour.
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(blendColours(style.paper, style.ink, 0.22), 1.0));
    painter->drawPath(shape);

    paintFaceContent(painter, box, rank, suit, style, art);
    painter->restore();
}

void drawDefaultBack(QPainter *painter, const QRectF &box, const CardStyle &style,
                     const PanelArt &art) {
    if (box.width() < 3 || box.height() < 5) return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal corner = cardCornerFor(box);
    QPainterPath shape;
    shape.addRoundedRect(box, corner, corner);

    painter->setPen(Qt::NoPen);
    painter->fillPath(shape, style.back);

    // Le dos est une carte comme les autres : un papier, et un panneau posé
    // dessus. Son panneau est le rectangle qu'occupe le treillis, et les deux
    // facteurs de taille valent pour lui comme pour celui d'une face.
    const QRectF panel = backPanelBox(box, style);
    if (panel.width() > 6 && panel.height() > 6) {
        const qreal round = panelRoundOf(panel, box, corner * 0.55);
        QPainterPath clip;
        clip.addRoundedRect(panel, round, round);
        painter->setBrush(Qt::NoBrush);

        if (art.isEmpty()) {
            // La marge encadrant un treillis, disposition traditionnelle du
            // dos d'une carte à jouer.
            painter->setPen(QPen(blendColours(style.back, QColor(Qt::white), 0.45), 1.0));
            painter->drawRoundedRect(panel, round, round);

            painter->save();
            painter->setClipPath(clip);
            painter->setPen(QPen(style.backInk, 1.0));
            const qreal pitch = std::max(4.0, box.width() * 0.17);
            for (qreal at = -panel.height(); at < panel.width() + panel.height(); at += pitch) {
                painter->drawLine(QPointF(panel.left() + at, panel.top()),
                                  QPointF(panel.left() + at + panel.height(), panel.bottom()));
                painter->drawLine(QPointF(panel.left() + at, panel.bottom()),
                                  QPointF(panel.left() + at + panel.height(), panel.top()));
            }
            painter->restore();
        } else {
            // L'image vient à la place du treillis : le treillis est le fond
            // du panneau d'un dos, comme le beige est celui d'une face, et
            // c'est le fond que l'image remplace. Le tracer par-dessus une
            // photographie la salirait sans rien dire de plus.
            painter->save();
            painter->setClipPath(clip);
            painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter->drawImage(panelArtBox(panel, art), art.image);
            painter->restore();
            painter->setPen(QPen(blendColours(style.back, QColor(Qt::white), 0.45), 1.0));
            painter->drawRoundedRect(panel, round, round);
        }
    }
    painter->restore();
}
