#include "SplitDialog.h"

#include "DialogChrome.h"
#include "Palette.h"

#include <QBuffer>
#include <QDialogButtonBox>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QImageReader>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSpinBox>

#include <cmath>

namespace {

// Les lignes du découpage tombent rarement sur des pixels entiers : une
// planche de 1019 pixels en sept colonnes fait des colonnes de 145,57. Les
// bornes sont donc calculées en proportion de la planche entière et arrondies
// une seule fois, pour que les morceaux se suivent sans trou ni recouvrement.
int boundary(int total, int parts, int index) {
    return qRound(qreal(total) * index / parts);
}

// La planche moins ses marges : la part qui est quadrillée. Les planches ont
// souvent un cadre — une bordure en bas, un filet tout autour — qui n'est pas
// une carte : quadrillée avec le reste, elle met une tranche de cadre dans
// chacun des cent quarante morceaux. Nulle si les marges ne laissent rien.
QRect insideRect(const QSize &sheet, const QMargins &margins) {
    const QRect inside = QRect(QPoint(0, 0), sheet).marginsRemoved(margins);
    if (inside.width() <= 0 || inside.height() <= 0) return QRect();
    return inside;
}

// La place d'un morceau dans la planche, rognage compris. `inside` est la
// planche moins ses marges. Nulle si le rognage ne laisse rien.
QRect pieceRect(const QRect &inside, int columns, int rows, int column, int row, int trim) {
    const int left = inside.left() + boundary(inside.width(), columns, column) + trim;
    const int right = inside.left() + boundary(inside.width(), columns, column + 1) - trim;
    const int top = inside.top() + boundary(inside.height(), rows, row) + trim;
    const int bottom = inside.top() + boundary(inside.height(), rows, row + 1) - trim;
    if (right <= left || bottom <= top) return QRect();
    return QRect(left, top, right - left, bottom - top);
}

// L'image telle qu'elle est dans la corbeille, rendue en pixels. Un SVG est
// dessiné à sa taille naturelle : on ne découpe pas un dessin vectoriel sans
// le rendre d'abord.
QImage readSheet(const DllImage &sheet) {
    QBuffer buffer;
    buffer.setData(sheet.bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, sheet.kind);
    return reader.read();
}

// Un champ où l'on écrit un nombre.
//
// Sans boutons, et habillé ici plutôt que par l'habillage des boîtes : une
// boîte à flèches que le bureau dessine arrive dans les couleurs du bureau,
// et habillée elle perd ses flèches, que Qt ne dessine plus dès qu'on touche
// à ses sous-parties. Un champ où l'on écrit treize est plus simple que deux
// flèches qu'il faudrait cliquer treize fois — la molette et les flèches du
// clavier y marchent toujours.
QSpinBox *countBox(QWidget *parent, int least, int most, int now) {
    const Palette &scheme = currentPalette();
    auto *box = new QSpinBox(parent);
    box->setRange(least, most);
    box->setValue(now);
    box->setButtonSymbols(QAbstractSpinBox::NoButtons);
    box->setAlignment(Qt::AlignCenter);
    // Un nombre à deux chiffres : un champ large comme la boîte dirait qu'on
    // y écrit une phrase.
    box->setFixedWidth(84);
    box->setStyleSheet(QStringLiteral(R"(
        QSpinBox {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 5px;
            padding: 5px 7px;
        }
        QSpinBox:focus { border: 1px solid %4; }
    )")
                           .arg(scheme.field.name(), scheme.surfaceText.name(),
                                scheme.edge.name(), scheme.accent.name()));
    return box;
}

// Le nom d'un réglage. Les étiquettes qu'un QFormLayout fabrique lui-même
// arrivent dans l'encre du bureau, qui n'est pas celle de la table : elles se
// lisent à peine sur la surface pâle d'une boîte.
QLabel *fieldLabel(QWidget *parent, const QString &text) {
    auto *label = new QLabel(text, parent);
    QPalette ink = label->palette();
    ink.setColor(QPalette::WindowText, currentPalette().surfaceText);
    label->setPalette(ink);
    return label;
}

} // namespace

// La planche, et le découpage par-dessus. Le quadrillage est la seule façon de
// savoir avant de découper si le compte de colonnes est le bon : sur une
// planche de cinquante-deux cartes, une colonne de trop se voit tout de suite.
//
// La planche entière tient dans la boîte, donc une carte y est minuscule : la
// molette grossit sous le curseur, et l'on déplace la planche en la tirant.
// C'est de quoi aller voir si un trait de découpe passe bien entre deux
// cartes, ce dont on ne peut pas juger sur une vignette.
class SheetPreview : public QWidget {
public:
    explicit SheetPreview(const QImage &sheet, QWidget *parent)
        : QWidget(parent), m_sheet(sheet) {
        setMinimumHeight(240);
    }

    void setCut(int columns, int rows, int trim, const QMargins &margins) {
        m_columns = columns;
        m_rows = rows;
        m_trim = trim;
        m_margins = margins;
        update();
    }

protected:
    void wheelEvent(QWheelEvent *event) override {
        const QPointF at = event->position();
        const int notches = event->angleDelta().y();
        if (notches == 0) return;
        // Un cran de molette grossit d'un cinquième : assez pour qu'on sente
        // l'effet, assez peu pour viser. Les pavés tactiles, qui envoient le
        // mouvement par petits bouts, grossissent d'autant par pouce parcouru.
        magnify(at, std::pow(1.0015, notches));
        event->accept();
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton || m_zoom <= 1.0) return;
        m_dragging = true;
        m_lastDrag = event->position();
        setCursor(Qt::ClosedHandCursor);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (!m_dragging) return;
        m_pan += event->position() - m_lastDrag;
        m_lastDrag = event->position();
        settle();
        update();
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) return;
        m_dragging = false;
        tellCursor();
    }

    // Revenir à la planche entière, sans avoir à dézoomer cran par cran.
    void mouseDoubleClickEvent(QMouseEvent *) override {
        m_zoom = 1.0;
        m_pan = QPointF();
        tellCursor();
        update();
    }

    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        settle();
    }

    void paintEvent(QPaintEvent *) override {
        const Palette &scheme = currentPalette();
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.fillRect(rect(), scheme.field);
        if (m_sheet.isNull()) return;

        const QRectF where = sheetRect();
        const qreal scale = where.width() / m_sheet.width();
        // Grossie, une planche doit montrer ses pixels : c'est à un pixel près
        // qu'on juge si un trait de découpe tombe entre deux cartes, et un
        // lissage le ferait disparaître.
        painter.setRenderHint(QPainter::SmoothPixmapTransform, scale <= 2.0);
        painter.drawImage(where, m_sheet);
        // De l'image vers l'écran : les marges et le quadrillage sont calculés
        // en pixels de la planche et montrés à l'échelle où elle est dessinée.
        auto onScreen = [&where, scale](const QRect &box) {
            return QRectF(where.left() + box.left() * scale, where.top() + box.top() * scale,
                          box.width() * scale, box.height() * scale);
        };

        const QRect inside = insideRect(m_sheet.size(), m_margins);

        // Ce que les marges mettent dehors, voilé : on voit ce qui part et ce
        // qui reste d'un seul coup d'œil, et une bordure oubliée en bas se
        // reconnaît à ce qu'elle n'est pas voilée.
        if (!m_margins.isNull()) {
            QPainterPath veil;
            veil.addRect(where);
            if (!inside.isNull()) veil.addRect(onScreen(inside));
            painter.setPen(Qt::NoPen);
            QColor wash = scheme.desk;
            wash.setAlphaF(0.66);
            painter.fillPath(veil, wash);
        }
        if (inside.isNull()) return;

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(scheme.accent, 1.0));
        for (int row = 0; row < m_rows; ++row) {
            for (int column = 0; column < m_columns; ++column) {
                const QRect cut = pieceRect(inside, m_columns, m_rows, column, row, m_trim);
                if (cut.isNull()) continue;
                painter.drawRect(onScreen(cut));
            }
        }

        // Le tour de la planche, pour que le bord du dernier morceau se
        // distingue du bord de l'image.
        painter.setPen(QPen(scheme.edge, 1.0));
        painter.drawRect(where.adjusted(-0.5, -0.5, 0.5, 0.5));
    }

private:
    // La planche à l'écran : au plus grand qu'elle tienne, centrée, jamais
    // agrandie — puis grossie et déplacée de ce que la molette et la main ont
    // demandé.
    QRectF sheetRect() const { return sheetRect(m_zoom, m_pan); }

    QRectF sheetRect(qreal zoom, const QPointF &pan) const {
        const QSize room = size() - QSize(2, 2);
        QSize shown = m_sheet.size().scaled(room, Qt::KeepAspectRatio);
        if (shown.width() > m_sheet.width()) shown = m_sheet.size();
        QRectF fitted(QPointF(0, 0), QSizeF(shown));
        fitted.moveCenter(QRectF(rect()).center());

        QRectF where(QPointF(0, 0), fitted.size() * zoom);
        where.moveCenter(fitted.center());
        return where.translated(pan);
    }

    // Grossir d'autant, en gardant sous le curseur le point de la planche qui
    // s'y trouvait : c'est ainsi qu'on va voir un coin sans avoir à le
    // ramener au centre d'abord.
    void magnify(const QPointF &at, qreal by) {
        const qreal was = m_zoom;
        m_zoom = qBound(1.0, m_zoom * by, 20.0);
        if (qFuzzyCompare(m_zoom, was)) return;

        const QRectF before = sheetRect(was, m_pan);
        const qreal acrossX = before.width() > 0 ? (at.x() - before.left()) / before.width() : 0.5;
        const qreal acrossY =
            before.height() > 0 ? (at.y() - before.top()) / before.height() : 0.5;
        const QRectF after = sheetRect();
        m_pan += at - QPointF(after.left() + acrossX * after.width(),
                              after.top() + acrossY * after.height());

        settle();
        tellCursor();
        update();
    }

    // Le déplacement ramené à ce qui a du sens : une planche plus grande que
    // la boîte ne s'en détache pas, une planche plus petite reste centrée. On
    // ne peut donc pas la perdre de vue.
    void settle() {
        const QRectF room(rect());
        const QRectF where = sheetRect();
        QPointF fix;
        if (where.width() <= room.width()) {
            fix.setX(room.center().x() - where.center().x());
        } else if (where.left() > room.left()) {
            fix.setX(room.left() - where.left());
        } else if (where.right() < room.right()) {
            fix.setX(room.right() - where.right());
        }
        if (where.height() <= room.height()) {
            fix.setY(room.center().y() - where.center().y());
        } else if (where.top() > room.top()) {
            fix.setY(room.top() - where.top());
        } else if (where.bottom() < room.bottom()) {
            fix.setY(room.bottom() - where.bottom());
        }
        m_pan += fix;
    }

    void tellCursor() {
        setCursor(m_zoom > 1.0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
    }

    QImage m_sheet;
    int m_columns = 1;
    int m_rows = 1;
    int m_trim = 0;
    QMargins m_margins;

    qreal m_zoom = 1.0;      // 1 : la planche entière, comme elle tient
    QPointF m_pan;           // ce que la main a déplacé, en pixels d'écran
    bool m_dragging = false;
    QPointF m_lastDrag;
};

SplitDialog::SplitDialog(const DllImage &sheet, QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Découper une planche"));

    m_sheet = readSheet(sheet);
    m_stem = imageStem(sheet.label);
    if (m_sheet.isNull()) return;

    const DialogChrome chrome = applyDialogChrome(this, tr("Découper"));

    chrome.fields->addWidget(dialogSectionHeading(this, m_stem));

    m_preview = new SheetPreview(m_sheet, this);
    chrome.fields->addWidget(m_preview, 1);

    auto *handling = new QLabel(tr("Molette : grossir sous le curseur · glisser : déplacer · "
                                   "double-clic : revoir la planche entière"),
                                this);
    handling->setWordWrap(true);
    markHintLabel(handling);
    chrome.fields->addWidget(handling);

    auto *form = new QFormLayout;
    form->setSpacing(kDialogRowSpacing);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    // Les marges se prennent sur la planche avant tout le reste : c'est leur
    // raison d'être. Une bordure laissée dans le quadrillage ne se rogne pas
    // après coup — elle est répartie entre tous les morceaux d'une rangée, et
    // chacun en reçoit sa tranche.
    m_left = countBox(this, 0, m_sheet.width() - 1, 0);
    m_right = countBox(this, 0, m_sheet.width() - 1, 0);
    m_top = countBox(this, 0, m_sheet.height() - 1, 0);
    m_bottom = countBox(this, 0, m_sheet.height() - 1, 0);
    for (QSpinBox *box : {m_left, m_right, m_top, m_bottom}) {
        box->setSuffix(tr(" px"));
        box->setToolTip(tr("Met ce bord de la planche dehors avant le quadrillage : le cadre "
                           "d'une planche n'est pas une carte, et certaines n'en ont que "
                           "d'un côté."));
    }

    // Treize colonnes et quatre rangées : la planche d'un jeu complet, et le
    // découpage qu'on demande neuf fois sur dix.
    m_columns = countBox(this, 1, 64, 13);
    m_rows = countBox(this, 1, 64, 4);
    m_trim = countBox(this, 0, 64, 0);
    m_trim->setSuffix(tr(" px"));
    m_trim->setToolTip(tr("Retire autant de pixels sur chaque bord de chaque morceau : "
                          "de quoi laisser derrière soi les traits qui séparent les cartes."));

    // Les quatre marges en carré plutôt qu'en quatre lignes : la planche est
    // là, au-dessus, et la main va plus vite à « bas » dans un carré que dans
    // une liste.
    auto *edges = new QGridLayout;
    edges->setHorizontalSpacing(8);
    edges->setVerticalSpacing(6);
    edges->addWidget(fieldLabel(this, tr("gauche")), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
    edges->addWidget(m_left, 0, 1);
    edges->addWidget(fieldLabel(this, tr("droite")), 0, 2, Qt::AlignRight | Qt::AlignVCenter);
    edges->addWidget(m_right, 0, 3);
    edges->addWidget(fieldLabel(this, tr("haut")), 1, 0, Qt::AlignRight | Qt::AlignVCenter);
    edges->addWidget(m_top, 1, 1);
    edges->addWidget(fieldLabel(this, tr("bas")), 1, 2, Qt::AlignRight | Qt::AlignVCenter);
    edges->addWidget(m_bottom, 1, 3);
    edges->setColumnStretch(4, 1);

    form->addRow(fieldLabel(this, tr("Marges")), edges);
    form->addRow(fieldLabel(this, tr("Colonnes")), m_columns);
    form->addRow(fieldLabel(this, tr("Rangées")), m_rows);
    form->addRow(fieldLabel(this, tr("Rogner les bords")), m_trim);
    chrome.fields->addLayout(form);

    m_tally = new QLabel(this);
    m_tally->setWordWrap(true);
    markHintLabel(m_tally);
    chrome.fields->addWidget(m_tally);

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *cancel = buttons->addButton(tr("Annuler"), QDialogButtonBox::RejectRole);
    QPushButton *split = buttons->addButton(tr("Découper"), QDialogButtonBox::AcceptRole);
    markDefaultButton(split);
    split->setDefault(true);
    chrome.footer->addStretch();
    chrome.footer->addWidget(buttons);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(split, &QPushButton::clicked, this, [this] {
        cut();
        accept();
    });
    for (QSpinBox *box : {m_left, m_top, m_right, m_bottom, m_columns, m_rows, m_trim}) {
        connect(box, &QSpinBox::valueChanged, this, &SplitDialog::retell);
    }

    retell();
    resize(560, 720);
}

QMargins SplitDialog::margins() const {
    return QMargins(m_left->value(), m_top->value(), m_right->value(), m_bottom->value());
}

// Ce que le découpage courant donnera : le compte des morceaux et leur taille.
void SplitDialog::retell() {
    const int columns = m_columns->value();
    const int rows = m_rows->value();
    const int trim = m_trim->value();
    const QMargins edges = margins();
    m_preview->setCut(columns, rows, trim, edges);

    const QRect inside = insideRect(m_sheet.size(), edges);
    if (inside.isNull()) {
        m_tally->setText(tr("Les marges ne laissent rien de la planche."));
        return;
    }
    const QRect first = pieceRect(inside, columns, rows, 0, 0, trim);
    if (first.isNull()) {
        m_tally->setText(tr("Le rognage ne laisse rien des morceaux."));
        return;
    }

    QString told = tr("%1 × %2 : %3 morceaux d'environ %4 × %5 pixels, pris sur une planche de "
                      "%6 × %7.")
                       .arg(columns)
                       .arg(rows)
                       .arg(columns * rows)
                       .arg(first.width())
                       .arg(first.height())
                       .arg(m_sheet.width())
                       .arg(m_sheet.height());
    if (!edges.isNull()) {
        told += tr(" Les marges en gardent %1 × %2, à partir de %3, %4.")
                    .arg(inside.width())
                    .arg(inside.height())
                    .arg(inside.left())
                    .arg(inside.top());
    }
    m_tally->setText(told);
}

// Les morceaux, en PNG : une planche est un JPEG le plus souvent, et un
// morceau de carte a des coins à rendre transparents plus tard.
void SplitDialog::cut() {
    const int columns = m_columns->value();
    const int rows = m_rows->value();
    const int trim = m_trim->value();
    const QRect inside = insideRect(m_sheet.size(), margins());
    if (inside.isNull()) return;

    m_pieces.clear();
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const QRect cut = pieceRect(inside, columns, rows, column, row, trim);
            if (cut.isNull()) continue;
            const QImage piece = m_sheet.copy(cut);
            if (piece.isNull()) continue;

            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            if (!piece.save(&buffer, "PNG")) continue;

            DllImage made;
            made.label = columns > 1 && rows > 1
                             ? tr("%1 %2-%3").arg(m_stem).arg(row + 1).arg(column + 1)
                             : tr("%1 %2").arg(m_stem).arg(row * columns + column + 1);
            made.kind = "png";
            made.bytes = bytes;
            made.width = piece.width();
            made.height = piece.height();
            m_pieces.append(made);
        }
    }
}
