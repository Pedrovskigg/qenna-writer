#include "CardItem.h"
#include "ColorPopover.h"

#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QBuffer>
#include <QCursor>
#include <QDialog>
#include <QFileDialog>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QGridLayout>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QStyleOptionGraphicsItem>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QToolButton>
#include <QVBoxLayout>

bool CardItem::s_tilt = true;
bool CardItem::s_boardLight = false;
QColor CardItem::s_boardColor = QColor(0x1a, 0x1a, 0x19);

namespace {

// Texto editável dentro do card (corpo do post-it, legenda da imagem, texto
// livre). boundingRect mínimo garante que toda a área do corpo pega o clique.
class BodyTextItem : public QGraphicsTextItem {
public:
    qreal bodyW = 180;
    qreal bodyH = 100;

    explicit BodyTextItem(QGraphicsItem* p) : QGraphicsTextItem(p) {}

    QRectF boundingRect() const override {
        QRectF r = QGraphicsTextItem::boundingRect();
        r.setWidth(qMax(r.width(),  bodyW));
        r.setHeight(qMax(r.height(), bodyH));
        return r;
    }
    QPainterPath shape() const override {
        QPainterPath p;
        p.addRect(boundingRect());
        return p;
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override {
        QStyleOptionGraphicsItem opt = *option;
        opt.state &= ~QStyle::State_Selected;
        opt.state &= ~QStyle::State_HasFocus;
        QGraphicsTextItem::paint(painter, &opt, widget);
    }
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* e) override {
        // Clicar no texto também seleciona o card (a barra de ações aparece).
        if (auto* card = dynamic_cast<CardItem*>(topLevelItem()))
            if (e->button() == Qt::LeftButton) emit card->cardPressed();
        QGraphicsTextItem::mousePressEvent(e);
    }
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Escape) { clearFocus(); e->accept(); return; }
        QGraphicsTextItem::keyPressEvent(e);
    }
    void focusOutEvent(QFocusEvent* e) override {
        QGraphicsTextItem::focusOutEvent(e);
        // Menu, troca de janela (Alt+Tab) ou agarrar o próprio card pra mover
        // não encerram a escrita. Quem encerra é desmarcar o card (clicar fora).
        if (e->reason() == Qt::PopupFocusReason || e->reason() == Qt::ActiveWindowFocusReason) return;
        if (auto* card = dynamic_cast<CardItem*>(topLevelItem())) {
            if (card->isCardSelected()) return;
            if (card->cardData().type == QStringLiteral("image")) card->onCaptionEditFinished();
            else                                                    card->onTextEditFinished();
        }
    }
};

// Editor inline do título do card (note/comment). Uma linha: Enter/Esc encerram.
class TitleEditItem : public QGraphicsTextItem {
public:
    qreal fieldW = 0;      // largura do campo (a faixa do título inteira)
    QColor fieldColor;     // fundo do campo: a cor do post, mais clara

    explicit TitleEditItem(QGraphicsItem* p) : QGraphicsTextItem(p) {}
    void setField(qreal w, const QColor& c) {
        prepareGeometryChange();
        fieldW = w;
        fieldColor = c;
        update();
    }
    QRectF boundingRect() const override {
        const QRectF r = QGraphicsTextItem::boundingRect();
        return QRectF(-5, -3, qMax(r.width(), fieldW) + 10, r.height() + 6);
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override {
        // Campo em destaque enquanto escreve o título.
        if (fieldColor.isValid()) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(fieldColor);
            painter->drawRoundedRect(boundingRect(), 4, 4);
            painter->restore();
        }
        // Sem o retângulo pontilhado de foco que o Qt desenha em volta do texto.
        QStyleOptionGraphicsItem opt = *option;
        opt.state &= ~QStyle::State_Selected;
        opt.state &= ~QStyle::State_HasFocus;
        QGraphicsTextItem::paint(painter, &opt, widget);
    }
protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter
            || e->key() == Qt::Key_Escape) {
            clearFocus();
            e->accept();
            return;
        }
        QGraphicsTextItem::keyPressEvent(e);
    }
    void focusOutEvent(QFocusEvent* e) override {
        QGraphicsTextItem::focusOutEvent(e);
        if (auto* card = dynamic_cast<CardItem*>(parentItem()))
            card->onTitleEditFinished();
    }
};

bool calcIsDark(const QColor& c)
{
    return (c.red() * 299 + c.green() * 587 + c.blue() * 114) / 1000 < 128;
}

QFont uiFont(qreal px, int weight = QFont::Normal, bool italic = false)
{
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(qMax(1, qRound(px)));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    // Card inclinado + ClearType = franja colorida nas letras.
    f.setStyleStrategy(QFont::NoSubpixelAntialias);
    return f;
}

// Papel claro e tinta do personagem e do verso
const QColor kPolaroid(0xf3, 0xef, 0xe6);
const QColor kPolaroidInk(0x2a, 0x26, 0x22);
const QColor kPolaroidMuted(0x7a, 0x72, 0x68);
// Folha escura do documento / capítulo
const QColor kSheet(0x2a, 0x2a, 0x28);
const QColor kSheetBorder(0x3a, 0x3a, 0x37);
const QColor kSheetTitle(0xf5, 0xf4, 0xef);
const QColor kSheetText(0xa9, 0xa5, 0x9d);
const QColor kSheetMuted(0x8a, 0x87, 0x7f);

// Post-it: cantos quase retos e o canto de baixo à direita cortado (a dobrinha).
QPainterPath foldedPath(qreal w, qreal h, qreal r, qreal f)
{
    QPainterPath p;
    p.moveTo(r, 0);
    p.lineTo(w - r, 0);
    p.quadTo(w, 0, w, r);
    p.lineTo(w, h - f);
    p.lineTo(w - f, h);
    p.lineTo(r, h);
    p.quadTo(0, h, 0, h - r);
    p.lineTo(0, r);
    p.quadTo(0, 0, r, 0);
    p.closeSubpath();
    return p;
}

QString plainExcerpt(const QString& html, int maxChars)
{
    QString t;
    if (html.contains(QLatin1Char('<'))) {
        QTextDocument d;
        d.setHtml(html);
        t = d.toPlainText();
    } else {
        t = html;
    }
    t.replace(QChar(0x2029), QLatin1Char('\n'));
    t.replace(QRegularExpression(QStringLiteral("\n{3,}")), QStringLiteral("\n\n"));
    t = t.trimmed();
    if (t.size() > maxChars) t = t.left(maxChars).trimmed() + QStringLiteral("…");
    return t;
}

constexpr qreal kPi = 3.14159265358979;

// Sorteio pequeno semeado pelo id: o mesmo card sai sempre igual.
struct Jitter {
    quint32 s;
    explicit Jitter(const QString& id, quint32 salt = 0) : s(qHash(id) ^ (salt * 2654435761u)) { if (!s) s = 7; }
    qreal next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s % 10000u) / 10000.0; }  // 0..1
};

// Picotado: recortado com tesoura de picotar, os quatro lados em zigue-zague.
QPainterPath zigzagPath(qreal w, qreal h, qreal t)
{
    QPolygonF pts;
    const int n = qMax(2, qRound(w / t)), m = qMax(2, qRound(h / t));
    const qreal dx = w / n, dy = h / m, d = t * 0.55;
    for (int i = 0; i < n; ++i) pts << QPointF(i * dx, 0) << QPointF((i + 0.5) * dx, d);
    for (int i = 0; i < m; ++i) pts << QPointF(w, i * dy) << QPointF(w - d, (i + 0.5) * dy);
    for (int i = n; i > 0; --i) pts << QPointF(i * dx, h) << QPointF((i - 0.5) * dx, h - d);
    for (int i = m; i > 0; --i) pts << QPointF(0, i * dy) << QPointF(d, (i - 0.5) * dy);
    QPainterPath p;
    p.addPolygon(pts);
    p.closeSubpath();
    return p;
}

// Folha arrancada do caderno: a borda de cima rasgada e os furos da espiral.
QPainterPath tornPath(qreal w, qreal h, const QString& id)
{
    Jitter j(id, 3);
    QPolygonF pts;
    pts << QPointF(0, h) << QPointF(0, 18);
    qreal x = 0;
    while (x < w) {
        x += 4 + j.next() * 7;
        pts << QPointF(qMin(x, w), 16 + j.next() * 5);
    }
    pts << QPointF(w, h);
    QPainterPath p;
    p.addPolygon(pts);
    p.closeSubpath();
    QPainterPath holes;
    for (qreal hx = 12; hx < w - 6; hx += 17) holes.addEllipse(QPointF(hx, 28), 3.2, 3.2);
    return p.subtracted(holes);
}

// Etiqueta de bagagem: os cantos de cima chanfrados e o furo do barbante.
QPainterPath tagPath(qreal w, qreal h)
{
    const qreal c = qMin(28.0, qMin(w, h) * 0.22);
    QPolygonF pts;
    pts << QPointF(c, 0) << QPointF(w - c, 0) << QPointF(w, c)
        << QPointF(w, h) << QPointF(0, h) << QPointF(0, c);
    QPainterPath p;
    p.addPolygon(pts);
    p.closeSubpath();
    QPainterPath hole;
    hole.addEllipse(QPointF(w / 2.0, 15.0), 5.5, 5.5);
    return p.subtracted(hole);
}

// Contorno de caneta à mão em volta do card.
QPainterPath penLoop(const QRectF& r, bool round, const QString& id)
{
    Jitter j(id, 11);
    auto jt = [&j](qreal amp) { return (j.next() - 0.5) * 2.0 * amp; };
    QPainterPath p;
    if (round) {
        const QPointF c = r.center();
        const qreal rx = r.width() / 2.0, ry = r.height() / 2.0;
        constexpr int n = 10;
        QVector<QPointF> pts;
        for (int i = 0; i <= n + 1; ++i) {
            const qreal a = i * 2.0 * kPi / n - kPi / 2 + 0.2;
            const qreal k = 1.0 + jt(0.03) + (i > n ? 0.06 : 0.0);   // a ponta passa da largada
            pts << QPointF(c.x() + std::cos(a) * rx * k, c.y() + std::sin(a) * ry * k);
        }
        p.moveTo(pts[0]);
        for (int i = 1; i < pts.size(); ++i) p.quadTo(pts[i - 1], (pts[i - 1] + pts[i]) / 2.0);
        return p;
    }
    const qreal L = r.left(), T = r.top(), R = r.right(), B = r.bottom(), W = r.width(), H = r.height();
    p.moveTo(L + 2 + jt(2), T + 4 + jt(2));
    p.cubicTo(L + W * 0.35, T - 2 + jt(2), L + W * 0.7, T + 3 + jt(2), R - 2 + jt(2), T + jt(2));
    p.cubicTo(R + 2 + jt(2), T + H * 0.4, R - 2 + jt(2), T + H * 0.7, R + jt(2), B - 2 + jt(2));
    p.cubicTo(L + W * 0.6, B + 2 + jt(2), L + W * 0.3, B - 3 + jt(2), L + 3 + jt(2), B + jt(2));
    p.cubicTo(L - 2 + jt(2), T + H * 0.6, L + 2 + jt(2), T + H * 0.3, L - 1 + jt(2), T + 12 + jt(2));
    return p;
}

// Mistura duas cores (t = quanto de b).
QColor mixColor(const QColor& a, const QColor& b, qreal t)
{
    return QColor(qRound(a.red()   + (b.red()   - a.red())   * t),
                  qRound(a.green() + (b.green() - a.green()) * t),
                  qRound(a.blue()  + (b.blue()  - a.blue())  * t));
}

// Fita rotuladora: a cor do post-it, funda, como a fita de plástico de verdade.
QColor dymoColor(const QColor& c)
{
    const QColor h = c.toHsl();
    const int hue = h.hslHue() < 0 ? 220 : h.hslHue();
    return QColor::fromHsl(hue, qMin(255, int(h.hslSaturation() * 0.7)), 52);
}

QFont dymoFont()
{
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(14);
    f.setWeight(QFont::Black);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 2.2);
    f.setStyleStrategy(QFont::NoSubpixelAntialias);
    return f;
}

// Tamanho com que cada formato nasce.
QSizeF noteSizeFor(const QString& shape, const QString& type)
{
    if (shape == QStringLiteral("index"))  return { 240, 150 };
    if (shape == QStringLiteral("strip"))  return { 220, 44 };
    if (shape == QStringLiteral("round"))  return { 150, 150 };
    if (shape == QStringLiteral("tag"))    return { 170, 190 };
    if (shape == QStringLiteral("torn"))   return { 200, 210 };
    if (shape == QStringLiteral("pinked")) return { 200, 170 };
    if (shape == QStringLiteral("dymo"))   return { 160, CardItem::kDymoH };
    return type == QStringLiteral("comment") ? QSizeF(220, 130) : QSizeF(200, 160);
}

// Grupos do seletor de símbolos (mesma lista do Mira 1, organizada).
struct SymbolGroup { const char* label; QStringList symbols; };
const QVector<SymbolGroup>& symbolGroups()
{
    static const QVector<SymbolGroup> g = {
        { QT_TRANSLATE_NOOP("CardItem", "Estrelas"),
          { "★","☆","✦","✧","✩","✪","✰","⭐","💫","✨" } },
        { QT_TRANSLATE_NOOP("CardItem", "Formas"),
          { "●","○","◉","◎","◆","◇","♦","▲","△","▼","▽","◀","▶","⬡","⬢" } },
        { QT_TRANSLATE_NOOP("CardItem", "Marcas"),
          { "×","✕","✖","✗","✘","✚","✛","✝","✞","+","−","÷","=","≠","≈","∞","±","√","∑" } },
        { QT_TRANSLATE_NOOP("CardItem", "Setas"),
          { "→","←","↑","↓","↗","↘","↙","↖","↔","↕","⇒","⇐","⇑","⇓","⇔","➔","➤" } },
        { QT_TRANSLATE_NOOP("CardItem", "Natureza e céu"),
          { "✿","❀","❁","✽","✾","❋","❆","❅","⚜","🌸","🌺","🍃",
            "☽","☾","☀","☁","⚡","❄","🌙","☄","⊕","⊗","⊙" } },
        { QT_TRANSLATE_NOOP("CardItem", "Objetos"),
          { "⚔","🗡","🛡","👁","🌹","🕊","🐉","🦋","🌿","🍂","⚙","⚛","🔮" } },
        { QT_TRANSLATE_NOOP("CardItem", "Letras e sinais"),
          { "Ω","Δ","Σ","Φ","Ψ","Λ","Θ","α","β","γ","π",
            "†","‡","§","¶","©","®","™","⁂","※","⌘","⌛","«","»","—","–","…","⸻" } },
    };
    return g;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

CardItem::CardItem(const CanvasCard& data, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_data(data)
{
    setFlag(ItemIsMovable, false);
    setFlag(ItemSendsGeometryChanges);
    setFlag(ItemIsFocusable);
    setAcceptHoverEvents(true);
    setPos(m_data.x, m_data.y);

    const QString t = m_data.type;
    if (t == QStringLiteral("image"))     loadPixmapFromContent();
    if (t == QStringLiteral("character")) loadCharacterPhoto();

    if (t == QStringLiteral("sticker")) {
        loadStickerFromContent();
        setZValue(m_data.z);
    } else if (t == QStringLiteral("text")) {
        auto* bti  = new BodyTextItem(this);
        bti->bodyW = 0; bti->bodyH = 0;      // tamanho natural, sem mínimo
        m_textItem = bti;
        m_textItem->setTextInteractionFlags(Qt::NoTextInteraction); // escreve no duplo clique
        m_textItem->setAcceptHoverEvents(false);
        m_textItem->document()->setDocumentMargin(0);
        m_textItem->document()->setPlainText(m_data.content);
        connect(m_textItem->document(), &QTextDocument::contentsChanged, this, [this]() {
            m_data.content = m_textItem->document()->toPlainText();
            refreshContentMetrics();
            emit dataChanged(m_data);
        });
        applyContentFont();
        m_textItem->setPos(0, 0);
        refreshContentMetrics();
    } else if (t == QStringLiteral("symbol")) {
        refreshContentMetrics();
    } else if (t == QStringLiteral("doc") || t == QStringLiteral("chapter")
               || t == QStringLiteral("character")) {
        rebuildRichDoc();
    } else {
        // note / comment / image: texto de verdade, editável no lugar
        m_bodyClip = new QGraphicsRectItem(this);
        m_bodyClip->setPen(Qt::NoPen);
        m_bodyClip->setBrush(Qt::NoBrush);
        m_bodyClip->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);

        auto* bti  = new BodyTextItem(m_bodyClip);
        m_textItem = bti;
        m_textItem->setAcceptHoverEvents(false);

        if (t == QStringLiteral("image")) {
            m_textItem->setTextInteractionFlags(Qt::NoTextInteraction); // legenda: duplo clique
            m_textItem->document()->setPlainText(m_data.description);
            connect(m_textItem->document(), &QTextDocument::contentsChanged, this, [this]() {
                m_data.description = m_textItem->document()->toPlainText();
                emit dataChanged(m_data);
                update();
            });
        } else {
            m_textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
            m_textItem->document()->setPlainText(m_data.content);
            connect(m_textItem->document(), &QTextDocument::contentsChanged, this, [this]() {
                m_data.content = m_textItem->document()->toPlainText();
                emit dataChanged(m_data);
            });
            // Ficha e folha de caderno: parágrafo novo também cai em cima da pauta.
            connect(m_textItem->document(), &QTextDocument::blockCountChanged, this, [this]() {
                applyLineGrid();
            });
        }
        m_textItem->setFont(uiFont(t == QStringLiteral("image") ? 12.5 : 13));
        QTextOption opt;
        opt.setAlignment(Qt::AlignLeft);
        opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        m_textItem->document()->setDefaultTextOption(opt);
        m_textItem->document()->setDocumentMargin(0);

        updateTextItem();
        applyTextColor();
        applyNoteShape();
    }
    refreshTilt();
}

qreal CardItem::radius()
{
    return static_cast<qreal>(Theme::panelRadius());
}

QColor CardItem::accent()
{
    return QColor(Theme::accentDefault());
}

bool CardItem::isNoteLike() const
{
    return m_data.type == QStringLiteral("note") || m_data.type == QStringLiteral("comment");
}

bool CardItem::isPaperDoc() const
{
    return m_data.type == QStringLiteral("doc") || m_data.type == QStringLiteral("chapter");
}

// ── Inclinação ───────────────────────────────────────────────────────────────

qreal CardItem::tiltDegrees() const
{
    if (!s_tilt || isTextSymbol() || isSticker()) return 0.0;
    // Mesmo id, mesma inclinação: o quadro não "mexe" toda vez que abre.
    const uint hv = qHash(m_data.id);
    const qreal unit = (int(hv % 2001u) - 1000) / 1000.0;   // -1 .. 1
    qreal span = 1.4;
    if (m_data.type == QStringLiteral("character")) span = 2.2;
    else if (m_data.type == QStringLiteral("image")) span = 1.2;
    else if (isPaperDoc()) span = 0.6;
    return unit * span;
}

void CardItem::refreshTilt()
{
    if (isTextSymbol()) { refreshContentMetrics(); return; }
    setTransformOriginPoint(m_data.width / 2.0, m_data.height / 2.0);
    setRotation(isSticker() ? m_data.rotation : tiltDegrees());
}

QPointF CardItem::pinScenePos() const
{
    if (isTextSymbol()) {
        const QRectF cb = contentBounds();
        return mapToScene(QPointF(cb.center().x(), cb.top()));
    }
    return mapToScene(anchorPoint());
}

// ── Formatos do post-it ──────────────────────────────────────────────────────

CardItem::NoteGeo CardItem::noteGeo() const
{
    const qreal w = m_data.width, h = m_data.height;
    const QString s = noteShape();
    NoteGeo g;
    if (s == QStringLiteral("index")) {
        g.title = QRectF(14, 7, w - 28, 20);
        g.body  = QRectF(14, 36, w - 28, h - 44);
    } else if (s == QStringLiteral("strip")) {
        g.title = QRectF(32, 0, w - 44, h);
        g.hasBody = false;
    } else if (s == QStringLiteral("round")) {
        g.title = QRectF(w * 0.14, h * 0.14, w * 0.72, h * 0.72);
        g.hasBody = false;
        g.centered = true;
    } else if (s == QStringLiteral("tag")) {
        g.title = QRectF(14, 28, w - 28, 20);
        g.body  = QRectF(14, 52, w - 28, h - 64);
    } else if (s == QStringLiteral("torn")) {
        g.title = QRectF(30, 36, w - 44, 20);
        g.body  = QRectF(30, 56, w - 44, h - 68);
    } else if (s == QStringLiteral("pinked")) {
        g.title = QRectF(18, 14, w - 36, 20);
        g.body  = QRectF(18, 38, w - 36, h - 54);
    } else if (s == QStringLiteral("dymo")) {
        g.title = QRectF(12, 0, w - 24, h);
        g.hasBody = false;
        g.centered = true;
    } else {
        const qreal padBot = (m_data.type == QStringLiteral("note")) ? kFoldSize : 10.0;
        g.title = QRectF(12, 6, w - 24, 20);
        g.body  = QRectF(12, kHeaderH + 2.0, w - 24, h - kHeaderH - 2.0 - padBot);
    }
    g.body.setWidth(qMax(10.0, g.body.width()));
    g.body.setHeight(qMax(10.0, g.body.height()));
    return g;
}

QPointF CardItem::anchorPoint() const
{
    const qreal cx = m_data.width / 2.0;
    if (!isNoteLike()) return QPointF(cx, 0.0);
    const QString s = noteShape();
    const QString f = m_data.fastener;
    const qreal top = (s == QStringLiteral("torn")) ? 21.0 : 0.0;   // a folha começa abaixo do rasgo
    if (f == QStringLiteral("clip"))   return QPointF(cx, top - 16.0);
    if (f == QStringLiteral("staple")) return QPointF(cx, top + 10.0);
    if (f.isEmpty() && s == QStringLiteral("tag")) return QPointF(cx, -30.0);  // o pin, lá em cima do barbante
    return QPointF(cx, top);
}

QColor CardItem::paperColor() const
{
    const QString s = noteShape();
    if (s == QStringLiteral("index")) return mixColor(QColor(0xfd, 0xfb, 0xf5), m_data.color, 0.28);
    if (s == QStringLiteral("torn"))  return QColor(0xfd, 0xfc, 0xf8);
    if (s == QStringLiteral("dymo"))  return dymoColor(m_data.color);
    return m_data.color;
}

void CardItem::applyLineGrid()
{
    if (!isNoteLike() || !m_textItem) return;
    const bool ruled = noteShape() == QStringLiteral("index") || noteShape() == QStringLiteral("torn");
    QTextDocument* d = m_textItem->document();
    for (QTextBlock b = d->begin(); b.isValid(); b = b.next()) {
        QTextBlockFormat bf = b.blockFormat();
        const bool isRuled = bf.lineHeightType() == QTextBlockFormat::FixedHeight;
        if (isRuled == ruled) continue;
        if (ruled) bf.setLineHeight(20, QTextBlockFormat::FixedHeight);
        else       bf.setLineHeight(100, QTextBlockFormat::SingleHeight);
        QTextCursor(b).setBlockFormat(bf);
    }
}

void CardItem::fitDymo()
{
    if (noteShape() != QStringLiteral("dymo")) return;
    QString t = m_data.title.simplified();
    if (t.isEmpty()) t = plainExcerpt(m_data.content, 40).section(QLatin1Char('\n'), 0, 0).simplified();
    if (t.isEmpty()) t = tr("Sem título");
    const qreal nw = qBound(80.0, QFontMetricsF(dymoFont()).horizontalAdvance(t.toUpper()) + 32.0, 900.0);
    if (qAbs(nw - m_data.width) < 0.5 && qAbs(m_data.height - kDymoH) < 0.5) return;
    prepareGeometryChange();
    m_data.width = nw;
    m_data.height = kDymoH;
    setTransformOriginPoint(m_data.width / 2.0, m_data.height / 2.0);
    update();
}

void CardItem::applyNoteShape()
{
    if (!isNoteLike() || !m_textItem) return;
    const NoteGeo g = noteGeo();
    if (m_bodyClip) m_bodyClip->setVisible(g.hasBody);
    m_textItem->setTextInteractionFlags(g.hasBody ? Qt::TextEditorInteraction : Qt::NoTextInteraction);
    if (!g.hasBody && m_textItem->hasFocus()) m_textItem->clearFocus();
    applyLineGrid();
    fitDymo();
    m_scrollOffset = 0.0;
    updateTextItem();
    applyTextColor();
}

void CardItem::setNoteStyle(const QString& shape, const QString& fastener, const QString& frame)
{
    if (!isNoteLike()) return;
    if (shape == m_data.shape && fastener == m_data.fastener && frame == m_data.frame) return;
    emit gestureStarted();
    prepareGeometryChange();
    if (shape != m_data.shape) {
        // Cada formato nasce no tamanho dele, com o centro no mesmo lugar.
        const QPointF c = pos() + QPointF(m_data.width / 2.0, m_data.height / 2.0);
        const QSizeF sz = noteSizeFor(shape, m_data.type);
        m_data.width  = sz.width();
        m_data.height = sz.height();
        setPos(c - QPointF(sz.width() / 2.0, sz.height() / 2.0));
        m_data.x = pos().x();
        m_data.y = pos().y();
    }
    m_data.shape    = shape;
    m_data.fastener = fastener;
    m_data.frame    = frame;
    applyNoteShape();
    refreshTilt();
    update();
    emit dataChanged(m_data);
    emit positionChanged(m_data.id);
    emit gestureFinished();
}

// ── Conteúdo pintado (doc / chapter / verso do personagem) ──────────────────

void CardItem::rebuildRichDoc()
{
    const bool isChar = (m_data.type == QStringLiteral("character"));
    if (!isChar && !isPaperDoc()) return;
    if (!m_richDoc) m_richDoc = new QTextDocument(this);

    // Texto corrido, sem a formatação do editor: no quadro só interessa ler.
    m_richDoc->setDefaultFont(uiFont(isChar ? 11.5 : 12));
    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_richDoc->setDefaultTextOption(opt);
    const QString body = plainExcerpt(m_data.content, 1600);
    m_richDoc->setPlainText(body);
    QTextCursor cur(m_richDoc);
    cur.select(QTextCursor::Document);
    QTextCharFormat cf;
    cf.setForeground(isChar ? QColor(0x4f, 0x48, 0x40) : kSheetText);
    cur.mergeCharFormat(cf);
    for (QTextBlock b = m_richDoc->begin(); b.isValid(); b = b.next()) {
        QTextCursor bc(b);
        QTextBlockFormat bf = b.blockFormat();
        bf.setBottomMargin(4);
        bf.setLineHeight(118, QTextBlockFormat::ProportionalHeight);
        bc.setBlockFormat(bf);
    }
    m_richDoc->setDocumentMargin(0);
    qreal x, y, wv, hv;
    richContentRect(x, y, wv, hv);
    m_richDoc->setTextWidth(wv);
}

void CardItem::richContentRect(qreal& x, qreal& y, qreal& w, qreal& h) const
{
    const qreal W = m_data.width, H = m_data.height;
    if (m_data.type == QStringLiteral("character")) {
        x = 12.0; y = 48.0;
        w = qMax(10.0, W - 24.0);
        h = qMax(10.0, H - y - 12.0);
    } else { // doc / chapter
        x = 14.0; y = 58.0;
        w = qMax(10.0, W - 28.0);
        h = qMax(10.0, H - y - 34.0);
    }
}

void CardItem::setLinkedMeta(const QString& kicker, const QString& footer)
{
    if (m_kicker == kicker && m_footer == footer) return;
    m_kicker = kicker;
    m_footer = footer;
    update();
}

void CardItem::setLinkedTitle(const QString& title)
{
    if (title.isEmpty() || title == m_data.title) return;
    m_data.title = title;
    update();
}

void CardItem::setRoleLabel(const QString& role)
{
    if (m_role == role) return;
    m_role = role;
    update();
}

// ── text / symbol ────────────────────────────────────────────────────────────

bool CardItem::isTextSymbol() const
{
    return m_data.type == QStringLiteral("text") || m_data.type == QStringLiteral("symbol");
}

int CardItem::effFontSize() const
{
    if (m_data.fontSize > 0) return m_data.fontSize;
    return (m_data.type == QStringLiteral("symbol")) ? 60 : 18;
}

QFont CardItem::contentFont() const
{
    const bool isSym = (m_data.type == QStringLiteral("symbol"));
    QString family = isSym ? QStringLiteral("Segoe UI Symbol")
                           : (m_data.fontFamily.isEmpty() ? QStringLiteral("Segoe UI")
                                                          : m_data.fontFamily);
    QFont f(family);
    f.setPixelSize(effFontSize());
    f.setBold(m_data.bold);
    f.setItalic(m_data.italic);
    f.setStyleStrategy(QFont::NoSubpixelAntialias);
    return f;
}

QRectF CardItem::contentBounds() const
{
    if (m_data.type == QStringLiteral("symbol")) {
        QFontMetricsF fm(contentFont());
        const QString g = m_data.content.isEmpty() ? QStringLiteral("★") : m_data.content;
        const qreal w = qMax(24.0, fm.horizontalAdvance(g));
        const qreal h = qMax(24.0, fm.height());
        return QRectF(0, 0, w, h);
    }
    if (m_textItem) {
        const QRectF r = m_textItem->boundingRect();
        const QFontMetricsF fm(contentFont());
        qreal w = r.width();
        if (m_data.wrapWidth > 0) w = m_data.wrapWidth;
        else if (m_textItem->document()->isEmpty())
            w = fm.horizontalAdvance(QCoreApplication::translate("CardItem", "Escreva aqui…")) + 4;
        return QRectF(0, 0, qMax(24.0, w), qMax(fm.height(), r.height()));
    }
    return QRectF(0, 0, 40, 28);
}

QRectF CardItem::tsResizeRect() const
{
    const QRectF cb = contentBounds();
    return QRectF(cb.right() - 2, cb.bottom() - 2, 12, 12);
}

QRectF CardItem::tsRotateRect() const
{
    const QRectF cb = contentBounds();
    const QPointF c(cb.center().x(), cb.top() - 22.0);
    return QRectF(c.x() - 8, c.y() - 8, 16, 16);
}

QRectF CardItem::tsWidthRect() const
{
    if (m_data.type != QStringLiteral("text")) return {};
    const QRectF cb = contentBounds();
    return QRectF(cb.right() + 2, cb.center().y() - 10, 8, 20);
}

QColor CardItem::readableInk() const
{
    // Texto branco num quadro claro (ou preto num escuro) some. Quando a cor
    // escolhida quase não se distingue do fundo, desenha com a tinta do fundo;
    // a cor escolhida continua gravada e volta quando o fundo combinar.
    const QColor c = m_data.color.isValid() ? m_data.color : QColor(Qt::white);
    auto lum = [](const QColor& x) {
        auto ch = [](qreal v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * ch(x.redF()) + 0.7152 * ch(x.greenF()) + 0.0722 * ch(x.blueF());
    };
    const qreal a = lum(c), b = lum(s_boardColor);
    const qreal ratio = (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
    if (ratio >= 1.25) return c;
    return s_boardLight ? QColor(0x2a, 0x26, 0x22) : QColor(0xf5, 0xf4, 0xef);
}

void CardItem::onBoardChanged()
{
    if (isTextSymbol()) applyContentFont();
    update();
}

void CardItem::applyContentFont()
{
    if (m_data.type != QStringLiteral("text") || !m_textItem) return;
    m_textItem->setFont(contentFont());
    m_textItem->setDefaultTextColor(readableInk());
    m_textItem->setTextWidth(m_data.wrapWidth > 0 ? m_data.wrapWidth : -1);
}

void CardItem::refreshContentMetrics()
{
    if (!isTextSymbol()) return;
    // Quando chega aqui o tamanho já mudou (letra, largura, texto digitado):
    // o lugar onde o card estava antes precisa ser repintado, senão fica
    // rastro das alças e do contorno no quadro.
    if (scene() && !m_paintedSceneRect.isNull()) scene()->update(m_paintedSceneRect);
    prepareGeometryChange();
    setTransformOriginPoint(contentBounds().center());
    setRotation(m_data.rotation);
    m_paintedSceneRect = sceneBoundingRect();
    update();
}

void CardItem::beginTextEdit(bool cursorAtEnd)
{
    if (m_data.type != QStringLiteral("text") || !m_textItem) return;
    if (!m_editingText) emit gestureStarted();   // snapshot p/ desfazer o que for escrito
    m_editingText = true;
    m_textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
    m_textItem->setFocus(Qt::MouseFocusReason);
    if (cursorAtEnd) {
        QTextCursor cur = m_textItem->textCursor();
        cur.movePosition(QTextCursor::End);
        m_textItem->setTextCursor(cur);
    }
    setCursor(Qt::IBeamCursor);
    update();
}

void CardItem::onTextEditFinished()
{
    if (m_data.type != QStringLiteral("text") || !m_textItem) return;
    if (!m_editingText) return;
    m_editingText = false;
    m_textItem->setTextInteractionFlags(Qt::NoTextInteraction);
    QTextCursor cur = m_textItem->textCursor();
    cur.clearSelection();
    m_textItem->setTextCursor(cur);
    setCursor(Qt::OpenHandCursor);
    update();
    if (m_textItem->document()->toPlainText().trimmed().isEmpty())
        emit emptyTextFinished(m_data.id);
    else
        emit gestureFinished();
}

void CardItem::beginTitleEdit()
{
    if (!isNoteLike()) return;
    if (!m_titleEditor) {
        m_titleEditor = new TitleEditItem(this);
        m_titleEditor->setAcceptHoverEvents(false);
        m_titleEditor->document()->setDocumentMargin(0);
        m_titleEditor->setTextWidth(-1);  // uma linha (sem quebra)
    }
    const NoteGeo geo = noteGeo();
    m_titleEditor->setFont(uiFont(13, QFont::Bold));
    m_titleEditor->setDefaultTextColor(inkColor());
    {
        QSignalBlocker bl(m_titleEditor->document());
        m_titleEditor->document()->setPlainText(m_data.title);
    }
    {
        // a cor do papel 40% mais clara (misturada com branco); papel escuro clareia menos
        const QColor c = paperColor();
        static_cast<TitleEditItem*>(m_titleEditor)->setField(
            qMax(10.0, geo.title.width()),
            calcIsDark(c) ? c.lighter(135) : mixColor(c, Qt::white, 0.4));
    }
    m_titleEditor->setPos(geo.title.left(), geo.title.center().y() - 10.0);
    m_titleEditor->setVisible(true);
    m_titleEditor->setTextInteractionFlags(Qt::TextEditorInteraction);

    emit gestureStarted();  // snapshot p/ undo antes de mexer no título
    m_editingTitle = true;
    m_titleEditor->setFocus();
    QTextCursor cur = m_titleEditor->textCursor();
    cur.select(QTextCursor::Document);
    m_titleEditor->setTextCursor(cur);
    update();
}

void CardItem::onTitleEditFinished()
{
    if (!m_editingTitle || !m_titleEditor) return;
    m_editingTitle = false;
    const QString t = m_titleEditor->document()->toPlainText().simplified();
    m_titleEditor->setTextInteractionFlags(Qt::NoTextInteraction);
    m_titleEditor->setVisible(false);  // título passa a ser pintado pelo paint()
    if (t != m_data.title) {
        m_data.title = t;
        if (noteShape() == QStringLiteral("dymo")) {
            fitDymo();
            emit positionChanged(m_data.id);
        }
        emit dataChanged(m_data);
    }
    update();
}

void CardItem::beginCaptionEdit()
{
    if (m_data.type != QStringLiteral("image") || !m_textItem) return;
    if (!m_editingText) emit gestureStarted();
    m_editingText = true;
    m_textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
    m_textItem->setFocus(Qt::MouseFocusReason);
    QTextCursor cur = m_textItem->textCursor();
    cur.movePosition(QTextCursor::End);
    m_textItem->setTextCursor(cur);
    update();
}

void CardItem::onCaptionEditFinished()
{
    if (!m_editingText || !m_textItem) return;
    m_editingText = false;
    m_textItem->setTextInteractionFlags(Qt::NoTextInteraction);
    QTextCursor cur = m_textItem->textCursor();
    cur.clearSelection();
    m_textItem->setTextCursor(cur);
    update();
}

void CardItem::setCardSelected(bool on)
{
    if (m_selected == on) return;
    m_selected = on;
    if (!on && m_editingText) {
        if (m_textItem && m_textItem->hasFocus()) m_textItem->clearFocus();
        // Sem foco (o card tinha sido arrastado), o focusOut não vem: encerra aqui.
        if (m_editingText) {
            if (m_data.type == QStringLiteral("image")) onCaptionEditFinished();
            else                                        onTextEditFinished();
        }
    }
    update();
    emit selectionFlagChanged();
}

// ── Ações da barra ───────────────────────────────────────────────────────────

void CardItem::setCardColor(const QColor& c)
{
    if (!c.isValid() || c == m_data.color) return;
    emit gestureStarted();
    m_data.color = c;
    if (isTextSymbol()) applyContentFont();
    applyTextColor();
    update();
    emit dataChanged(m_data);
}

void CardItem::setTextStyle(const QString& family, bool bold, bool italic)
{
    if (m_data.type != QStringLiteral("text")) return;
    if (family == m_data.fontFamily && bold == m_data.bold && italic == m_data.italic) return;
    emit gestureStarted();
    m_data.fontFamily = family;
    m_data.bold = bold;
    m_data.italic = italic;
    applyContentFont();
    refreshContentMetrics();
    emit dataChanged(m_data);
}

void CardItem::setSymbol(const QString& symbol)
{
    if (m_data.type != QStringLiteral("symbol") || symbol.isEmpty() || symbol == m_data.content) return;
    emit gestureStarted();
    m_data.content = symbol;
    refreshContentMetrics();
    emit dataChanged(m_data);
}

void CardItem::chooseImage()
{
    const QString path = QFileDialog::getOpenFileName(
        nullptr, tr("Escolher imagem"), QString(),
        tr("Imagens (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
    if (path.isEmpty()) return;

    QImage img(path);
    if (img.isNull()) return;
    if (img.width() > 900 || img.height() > 900)
        img = img.scaled(900, 900, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", 82);

    emit gestureStarted();
    m_data.content = QString::fromLatin1(ba.toBase64());
    loadPixmapFromContent();
    update();
    emit dataChanged(m_data);
}

bool CardItem::pickSymbol(QWidget* parent, QString& symbol, const QPoint& globalPos)
{
    QDialog dlg(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground);
    dlg.setObjectName(QStringLiteral("lousaSymbolPopup"));
    auto* outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* panel = new QWidget(&dlg);
    panel->setObjectName(QStringLiteral("lousaSymbolPanel"));
    outer->addWidget(panel);
    auto* pl = new QVBoxLayout(panel);
    pl->setContentsMargins(14, 12, 10, 12);
    pl->setSpacing(8);

    auto* title = new QLabel(tr("SÍMBOLO"), panel);
    title->setObjectName(QStringLiteral("lousaSymbolTitle"));
    pl->addWidget(title);

    auto* scroll = new QScrollArea(panel);
    scroll->setObjectName(QStringLiteral("lousaSymbolScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget;
    content->setObjectName(QStringLiteral("lousaSymbolContent"));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 4, 0);
    cl->setSpacing(6);

    QString picked;
    for (const SymbolGroup& g : symbolGroups()) {
        auto* gl = new QLabel(QCoreApplication::translate("CardItem", g.label), content);
        gl->setObjectName(QStringLiteral("lousaSymbolGroup"));
        cl->addWidget(gl);
        auto* gridW = new QWidget(content);
        auto* grid = new QGridLayout(gridW);
        grid->setContentsMargins(0, 0, 0, 4);
        grid->setSpacing(3);
        int n = 0;
        for (const QString& s : g.symbols) {
            auto* b = new QToolButton(gridW);
            b->setObjectName(QStringLiteral("lousaSymbolBtn"));
            b->setText(s);
            b->setFixedSize(34, 34);
            QFont bf(QStringLiteral("Segoe UI Symbol"));
            bf.setPixelSize(19);
            b->setFont(bf);
            b->setCheckable(true);
            b->setChecked(s == symbol);
            b->setCursor(Qt::PointingHandCursor);
            QObject::connect(b, &QToolButton::clicked, &dlg, [&dlg, &picked, s]() {
                picked = s;
                dlg.accept();
            });
            grid->addWidget(b, n / 9, n % 9);
            ++n;
        }
        cl->addWidget(gridW);
    }
    cl->addStretch(1);
    scroll->setWidget(content);
    pl->addWidget(scroll, 1);

    panel->setStyleSheet(Theme::qss(QStringLiteral(
        "QWidget#lousaSymbolPanel { background: %1; border: 1px solid %2; border-radius: @radius-panel; }"
        "QLabel#lousaSymbolTitle { color: %4; font-size: 11px; font-weight: 700; letter-spacing: 1px; background: transparent; }"
        "QLabel#lousaSymbolGroup { color: %4; font-size: 11px; background: transparent; }"
        "QScrollArea#lousaSymbolScroll, QWidget#lousaSymbolContent { background: transparent; }"
        "QToolButton#lousaSymbolBtn { background: transparent; border: 1px solid transparent;"
        "  border-radius: @radius-control; color: %3; }"
        "QToolButton#lousaSymbolBtn:hover { background: %5; border-color: %2; }"
        "QToolButton#lousaSymbolBtn:checked { background: %5; border-color: %6; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(),
          Theme::textMuted(), Theme::hoverOverlay(), Theme::accentDefault())));

    dlg.resize(352, 420);
    QPoint at = globalPos.isNull() ? QCursor::pos() : globalPos;
    if (QScreen* scr = QGuiApplication::screenAt(at)) {
        const QRect ar = scr->availableGeometry();
        at.setX(qBound(ar.left() + 8, at.x() - 176, ar.right() - 360));
        at.setY(qBound(ar.top() + 8, at.y() + 12, ar.bottom() - 428));
    }
    dlg.move(at);
    if (dlg.exec() != QDialog::Accepted || picked.isEmpty()) return false;
    symbol = picked;
    return true;
}

CanvasCard CardItem::cardData() const
{
    CanvasCard d = m_data;
    const QPointF p = pos();
    d.x = p.x();
    d.y = p.y();
    return d;
}

void CardItem::syncFromData()
{
    setPos(m_data.x, m_data.y);
    prepareGeometryChange();
    if (m_data.type == QStringLiteral("image"))     loadPixmapFromContent();
    if (m_data.type == QStringLiteral("character")) loadCharacterPhoto();
    if (isSticker()) { loadStickerFromContent(); setZValue(m_data.z); }
    if (m_data.type == QStringLiteral("doc") || m_data.type == QStringLiteral("chapter")
        || m_data.type == QStringLiteral("character")) rebuildRichDoc();
    if (m_data.type == QStringLiteral("text") && m_textItem) {
        QSignalBlocker bl(m_textItem->document());
        m_textItem->document()->setPlainText(m_data.content);
    }
    if (isTextSymbol()) { applyContentFont(); refreshContentMetrics(); }
    else                updateTextItem();
    applyTextColor();
    applyNoteShape();
    refreshTilt();
    update();
}

// ── Geometria ──────────────────────────────────────────────────────────────

QRectF CardItem::boundingRect() const
{
    if (isTextSymbol()) {
        // conteúdo + alça de rotação em cima + alças de tamanho/largura
        return contentBounds().adjusted(-10, -36, 18, 16);
    }
    if (isSticker()) {
        // gira em volta do centro: um quadrado que cabe o adesivo em qualquer
        // ângulo, mais as alças e o "120% · 15°" embaixo
        const qreal r = std::hypot(m_data.width, m_data.height) / 2.0 + 48.0;
        return QRectF(m_data.width / 2.0 - r, m_data.height / 2.0 - r, r * 2, r * 2);
    }
    // sombra, pin (sobe 8 px), anel de seleção e brilho do snap
    constexpr qreal kMargin = 18.0;
    const qreal extraH = (m_data.type == QStringLiteral("comment")) ? kTailH : 0.0;
    const qreal top = qMax(kMargin, -anchorPoint().y() + 16.0);   // clipe e barbante sobem mais
    return QRectF(-kMargin, -top,
                  m_data.width  + kMargin * 2,
                  m_data.height + extraH + kMargin + top);
}

QPainterPath CardItem::outlinePath() const
{
    const qreal w = m_data.width, h = m_data.height;
    QPainterPath p;
    if (isNoteLike() && !m_data.shape.isEmpty()) {
        const QString s = m_data.shape;
        if (s == QStringLiteral("round"))  { p.addEllipse(QRectF(0, 0, w, h)); return p; }
        if (s == QStringLiteral("tag"))    return tagPath(w, h);
        if (s == QStringLiteral("torn"))   return tornPath(w, h, m_data.id);
        if (s == QStringLiteral("pinked")) return zigzagPath(w, h, 9.0);
        const qreal r = (s == QStringLiteral("dymo")) ? 3.0 : 2.0;   // ficha, tira, fita
        p.addRoundedRect(QRectF(0, 0, w, h), r, r);
        return p;
    }
    if (m_data.type == QStringLiteral("note")) {
        return foldedPath(w, h, 3.0, kFoldSize);
    }
    const qreal r = (m_data.type == QStringLiteral("comment")) ? 10.0
                  : (m_data.type == QStringLiteral("character")) ? 2.0
                  : 9.0;
    p.addRoundedRect(QRectF(0, 0, w, h), r, r);
    return p;
}

QPainterPath CardItem::shape() const
{
    QPainterPath p;
    if (isTextSymbol()) {
        p.addRect(boundingRect()); // toda a área (conteúdo + alças) é interativa
        return p;
    }
    if (isSticker()) {
        const qreal w = m_data.width, h = m_data.height;
        if (m_selected && !m_data.locked) {
            p.addRect(QRectF(-14, -14, w + 28, h + 28));   // as alças dos cantos
            p.addRect(stkRotateRect());
            return p.simplified();
        }
        p.addRect(QRectF(0, 0, w, h));
        return p;
    }
    p = outlinePath();
    QPainterPath extra;
    extra.addEllipse(anchorPoint(), 10.0, 10.0);   // o pin (ou a presilha) pega o clique
    if (isNoteLike() && m_data.shape == QStringLiteral("round"))
        extra.addRect(QRectF(m_data.width * 0.854 - 10, m_data.height * 0.854 - 10, 20, 20));
    return p.united(extra);
}

void CardItem::updateTextItem()
{
    if (isTextSymbol()) { refreshContentMetrics(); return; }
    if (m_richDoc) {
        qreal x, y, wv, hv;
        richContentRect(x, y, wv, hv);
        m_richDoc->setTextWidth(wv);
    }
    if (!m_textItem) return;

    const qreal w = m_data.width, h = m_data.height;
    if (m_data.type == QStringLiteral("image")) {
        const qreal top = h - kCaptionH + 7.0;
        const qreal tw = qMax(10.0, w - 22.0);
        const qreal th = kCaptionH - 9.0;
        if (auto* bti = static_cast<BodyTextItem*>(m_textItem)) { bti->bodyW = tw; bti->bodyH = th; }
        m_textItem->setTextWidth(tw);
        if (m_bodyClip) m_bodyClip->setRect(QRectF(0, top - 1, w, th + 2));
        m_textItem->setPos(11.0, top);
        return;
    }

    // note / comment
    const NoteGeo g = noteGeo();
    const qreal tw = g.body.width();
    const qreal th = g.body.height();
    if (auto* bti = static_cast<BodyTextItem*>(m_textItem)) { bti->bodyW = tw; bti->bodyH = th; }
    m_textItem->setTextWidth(tw);
    if (m_bodyClip) m_bodyClip->setRect(QRectF(0, g.body.top(), w, th));

    qreal visH = 0.0, contentH = 0.0;
    if (scrollRegion(visH, contentH))
        m_scrollOffset = qBound(0.0, m_scrollOffset, qMax(0.0, contentH - visH));
    m_textItem->setPos(g.body.left(), g.body.top() - m_scrollOffset);
}

// ── Cores ──────────────────────────────────────────────────────────────────

bool CardItem::isDark() const { return calcIsDark(m_data.color); }

QColor CardItem::inkColor() const
{
    // Tinta do papel: a própria cor bem escura (marrom no amarelo, vinho no
    // vermelho), em vez de preto chapado. Em papel escuro, branco suave.
    const QColor bg = paperColor();
    if (calcIsDark(bg)) return QColor(255, 255, 255, 228);
    QColor c = bg.toHsl();
    const int hue = c.hslHue() < 0 ? 40 : c.hslHue();
    return QColor::fromHsl(hue, qMin(255, int(c.hslSaturation() * 0.55)), 38);
}

void CardItem::applyTextColor()
{
    if (isTextSymbol()) { applyContentFont(); return; }
    if (!m_textItem) return;
    const QColor tc = (m_data.type == QStringLiteral("image")) ? QColor(0xc9, 0xc5, 0xbd) : inkColor();
    m_textItem->setDefaultTextColor(tc);
}

void CardItem::setLinkedHtml(const QString& html)
{
    m_data.content = html;
    if (m_data.type == QStringLiteral("character") || isPaperDoc()) {
        rebuildRichDoc();
        updateTextItem();
    }
    update();
}

void CardItem::setCharacterPhoto(const QString& dataUrl)
{
    m_data.photoDataUrl = dataUrl;
    loadCharacterPhoto();
    update();
}

void CardItem::loadCharacterPhoto()
{
    const QString& url = m_data.photoDataUrl;
    if (url.isEmpty()) { m_pixmap = QPixmap(); return; }
    const int comma = url.indexOf(QLatin1Char(','));
    const QByteArray ba = (comma >= 0)
        ? QByteArray::fromBase64(url.mid(comma + 1).toLatin1())
        : QByteArray::fromBase64(url.toLatin1());
    m_pixmap.loadFromData(ba);
}

void CardItem::loadPixmapFromContent()
{
    if (m_data.content.isEmpty()) { m_pixmap = QPixmap(); return; }
    const QByteArray ba = QByteArray::fromBase64(m_data.content.toLatin1());
    m_pixmap.loadFromData(ba);
}

void CardItem::toggleImageDesc(bool show)
{
    if (m_data.type != QStringLiteral("character")) return;
    m_showDesc = show;
    m_scrollOffset = 0.0;
    update();
}

bool CardItem::scrollRegion(qreal& visH, qreal& contentH) const
{
    const QString t = m_data.type;
    if (isPaperDoc() || t == QStringLiteral("character")) {
        if (t == QStringLiteral("character") && !m_showDesc) return false;
        qreal x, y, wv, hv;
        richContentRect(x, y, wv, hv);
        visH     = hv;
        contentH = m_richDoc ? m_richDoc->size().height() : 0.0;
        return true;
    }
    if (!m_textItem || !isNoteLike()) return false;
    const NoteGeo g = noteGeo();
    if (!g.hasBody) return false;
    visH     = g.body.height();
    contentH = m_textItem->document()->size().height();
    return true;
}

bool CardItem::wheelScroll(int angleDeltaY)
{
    qreal visH = 0.0, contentH = 0.0;
    if (!scrollRegion(visH, contentH)) return false;
    const qreal maxScroll = qMax(0.0, contentH - visH);
    if (maxScroll < 1.0) return false; // não há overflow → deixa a view dar zoom

    const qreal delta = (angleDeltaY / 120.0) * 40.0;
    m_scrollOffset = qBound(0.0, m_scrollOffset - delta, maxScroll);
    if (m_textItem) updateTextItem();
    update();
    return true;
}

void CardItem::paintScrollbar(QPainter* p, qreal top, qreal visH,
                              qreal contentH, const QColor& thumb) const
{
    if (contentH <= visH + 1.0 || visH <= 0.0) return;
    const qreal w        = m_data.width;
    const qreal trackX    = w - 6.0, trackW = 3.0;
    const qreal maxScroll = contentH - visH;
    const qreal thumbH    = qMax(20.0, visH * visH / contentH);
    const qreal thumbY    = top + (maxScroll > 0.0 ? (m_scrollOffset / maxScroll) : 0.0)
                                    * (visH - thumbH);
    p->save();
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(thumb.red(), thumb.green(), thumb.blue(), 38));
    p->drawRoundedRect(QRectF(trackX, top, trackW, visH), 1.5, 1.5);
    p->setBrush(thumb);
    p->drawRoundedRect(QRectF(trackX, thumbY, trackW, thumbH), 1.5, 1.5);
    p->restore();
}

// ── Pintura: peças comuns ────────────────────────────────────────────────────

void CardItem::paintShadow(QPainter* p, const QRectF& r, qreal rad, qreal lift) const
{
    // Sombra macia de papel: camadas finas, mais densas perto do card.
    p->save();
    p->setPen(Qt::NoPen);
    for (int i = 6; i >= 1; --i) {
        const qreal grow = i * 1.6;
        p->setBrush(QColor(0, 0, 0, int(9 + (6 - i) * 3)));
        p->drawRoundedRect(r.adjusted(-grow + 1, -grow + lift + 1, grow - 1, grow + lift),
                           rad + grow, rad + grow);
    }
    p->restore();
}

void CardItem::paintPin(QPainter* p, const QColor& c) const
{
    paintPinAt(p, QPointF(m_data.width / 2.0, 0.0), c);
}

void CardItem::paintPinAt(QPainter* p, const QPointF& ctr, const QColor& c) const
{
    // Alfinete de quadro de investigação: é dele que a linha sai.
    const qreal r = 6.0;
    p->save();
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 70));
    p->drawEllipse(ctr + QPointF(1.4, 2.4), r, r);
    QRadialGradient g(ctr + QPointF(-r * 0.35, -r * 0.35), r * 1.6);
    g.setColorAt(0.0, c.lighter(165));
    g.setColorAt(0.55, c);
    g.setColorAt(1.0, c.darker(150));
    p->setBrush(g);
    p->setPen(QPen(c.darker(170), 1.0));
    p->drawEllipse(ctr, r, r);
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(255, 255, 255, 170));
    p->drawEllipse(ctr + QPointF(-2.0, -2.2), 1.7, 1.4);
    p->restore();
}

void CardItem::paintSelectionRing(QPainter* p, const QPainterPath& outline) const
{
    QPainterPathStroker st;
    st.setWidth(9.0);
    st.setJoinStyle(Qt::RoundJoin);
    const QPainterPath ring = st.createStroke(outline).united(outline);
    p->save();
    p->setBrush(Qt::NoBrush);
    QColor a = accent();
    a.setAlpha(230);
    p->setPen(QPen(a, 2.0));
    p->drawPath(ring.simplified());
    p->restore();
}

void CardItem::paintCover(QPainter* p, const QRectF& r, qreal rad) const
{
    p->save();
    QPainterPath clip;
    clip.addRoundedRect(r, rad, rad);
    p->setClipPath(clip);
    if (!m_pixmap.isNull()) {
        const qreal iw = m_pixmap.width(), ih = m_pixmap.height();
        const qreal scale = qMax(r.width() / iw, r.height() / ih);
        const qreal sw = iw * scale, sh = ih * scale;
        p->drawPixmap(QRectF(r.center().x() - sw / 2, r.center().y() - sh / 2, sw, sh),
                      m_pixmap, QRectF(0, 0, iw, ih));
    } else if (m_data.type == QStringLiteral("character")) {
        // Sem foto: a cor do personagem, em degradê, com a silhueta.
        const QColor base = m_data.color.isValid() ? m_data.color : QColor(0xf9, 0x73, 0x16);
        QLinearGradient lg(r.topLeft(), r.bottomRight());
        lg.setColorAt(0, base.lighter(135));
        lg.setColorAt(1, base.darker(125));
        p->fillRect(r, lg);
        const qreal s = qMin(r.width(), r.height());
        const QPointF c(r.center().x(), r.bottom());
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(255, 255, 255, 140));
        p->drawEllipse(QPointF(c.x(), r.bottom() - s * 0.62), s * 0.2, s * 0.2);
        QPainterPath body;
        body.moveTo(c.x() - s * 0.42, r.bottom() + 1);
        body.cubicTo(c.x() - s * 0.38, r.bottom() - s * 0.3, c.x() - s * 0.2, r.bottom() - s * 0.38,
                     c.x(), r.bottom() - s * 0.38);
        body.cubicTo(c.x() + s * 0.2, r.bottom() - s * 0.38, c.x() + s * 0.38, r.bottom() - s * 0.3,
                     c.x() + s * 0.42, r.bottom() + 1);
        body.closeSubpath();
        p->drawPath(body);
    } else {
        p->fillRect(r, QColor(0x1e, 0x1e, 0x1d));
        p->setPen(QColor(255, 255, 255, 90));
        p->setFont(uiFont(12));
        p->drawText(r.adjusted(10, 0, -10, 0), Qt::AlignCenter | Qt::TextWordWrap,
                    tr("Clique com o botão direito para escolher a imagem"));
    }
    p->restore();
}

// ── Pintura ────────────────────────────────────────────────────────────────

void CardItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    const qreal w = m_data.width;
    const qreal h = m_data.height;
    const QString t = m_data.type;

    p->setRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::TextAntialiasing);
    p->setRenderHint(QPainter::SmoothPixmapTransform);

    if (isSticker()) { paintSticker(p); return; }

    // ── text / symbol: soltos no quadro, sem papel ─────────────────────────
    if (isTextSymbol()) {
        const bool isSym = (t == QStringLiteral("symbol"));
        const QRectF cb = contentBounds();
        const QColor col = readableInk();

        if (isSym) {
            p->setFont(contentFont());
            p->setPen(col);
            p->drawText(cb, Qt::AlignCenter,
                        m_data.content.isEmpty() ? QStringLiteral("★") : m_data.content);
        } else if (m_textItem && m_textItem->document()->isEmpty()) {
            // placeholder (o texto real é desenhado pelo m_textItem filho)
            p->setFont(contentFont());
            p->setPen(s_boardLight ? QColor(0, 0, 0, 80) : QColor(255, 255, 255, 80));
            p->drawText(cb, Qt::AlignLeft | Qt::AlignTop, tr("Escreva aqui…"));
        }

        if (m_selected || m_editingText) {
            QColor a = accent();
            p->setBrush(Qt::NoBrush);
            QPen ring(a, 1.6);
            if (m_editingText) ring.setStyle(Qt::DashLine);
            p->setPen(ring);
            p->drawRoundedRect(cb.adjusted(-5, -4, 5, 4), 5, 5);
        }
        if (m_selected && !m_editingText) {
            const QColor handleFill = s_boardLight ? QColor(255, 255, 255) : QColor(0x23, 0x23, 0x22);
            const QColor a = accent();
            // Alça de rotação (topo-centro)
            const QPointF rc = tsRotateRect().center();
            p->setPen(QPen(a, 1.2));
            p->drawLine(QPointF(cb.center().x(), cb.top() - 4), QPointF(rc.x(), rc.y() + 7));
            p->setBrush(handleFill);
            p->drawEllipse(rc, 7, 7);
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(a, 1.4));
            p->drawArc(QRectF(rc.x() - 3.6, rc.y() - 3.6, 7.2, 7.2), 40 * 16, 270 * 16);
            // Alça do tamanho da letra (canto)
            const QRectF rr = tsResizeRect();
            p->setPen(QPen(a, 1.4));
            p->setBrush(handleFill);
            p->drawRoundedRect(rr.adjusted(1, 1, -1, -1), 2, 2);
            // Alça da largura (direita) — só texto
            if (t == QStringLiteral("text")) {
                const QRectF wr = tsWidthRect();
                p->drawRoundedRect(wr, 3, 3);
            }
        }
        return;
    }

    // ── Imagem: moldura escura, foto e legenda embaixo ──────────────────────
    if (t == QStringLiteral("image")) {
        paintShadow(p, QRectF(0, 0, w, h), 9, 3);
        p->setPen(QPen(kSheetBorder, 1));
        p->setBrush(kSheet);
        p->drawRoundedRect(QRectF(0, 0, w, h), 9, 9);
        paintCover(p, QRectF(7, 7, w - 14, h - 7 - kCaptionH), 6);
        if (m_textItem && m_textItem->document()->isEmpty() && !m_editingText
            && (m_hovered || m_selected)) {
            p->setPen(kSheetMuted);
            p->setFont(uiFont(12.5, QFont::Normal, true));
            p->drawText(QRectF(11, h - kCaptionH + 6, w - 22, kCaptionH - 8),
                        Qt::AlignLeft | Qt::AlignTop, tr("Legenda (duplo clique)"));
        }
        paintPin(p, m_data.color.isValid() ? m_data.color : QColor(0x34, 0xd3, 0x99));
        if (m_selected) paintSelectionRing(p, outlinePath());
        return;
    }

    // ── Documento / capítulo: folha com nome da gaveta e começo do texto ──
    if (isPaperDoc()) {
        const bool isCh = (t == QStringLiteral("chapter"));
        paintShadow(p, QRectF(0, 0, w, h), 9, 3);
        p->setPen(QPen(kSheetBorder, 1));
        p->setBrush(kSheet);
        p->drawRoundedRect(QRectF(0, 0, w, h), 9, 9);

        const QColor acc = m_data.color.isValid() ? m_data.color : accent();
        // Linha de cima: ícone + gaveta (ou "Capítulo 3")
        const qreal ky = 13.0;
        p->setPen(QPen(isCh ? acc : kSheetMuted, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        QPolygonF page;
        page << QPointF(14, ky) << QPointF(19.5, ky) << QPointF(22.5, ky + 3)
             << QPointF(22.5, ky + 11) << QPointF(14, ky + 11);
        p->drawPolygon(page);
        QString kicker = m_kicker.isEmpty() ? (isCh ? tr("Capítulo") : tr("Documento")) : m_kicker;
        QFont kf = uiFont(10, QFont::Bold);
        kf.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        p->setFont(kf);
        QFontMetricsF kfm(kf);
        p->setPen(isCh ? acc : kSheetMuted);
        p->drawText(QRectF(28, ky - 2, w - 42, 15), Qt::AlignVCenter | Qt::AlignLeft,
                    kfm.elidedText(kicker.toUpper(), Qt::ElideRight, w - 42));
        // Título
        const QFont tf = uiFont(15, QFont::Bold);
        p->setFont(tf);
        p->setPen(kSheetTitle);
        const QString title = m_data.title.isEmpty()
            ? (isCh ? tr("Capítulo sem título") : tr("Documento sem título")) : m_data.title;
        p->drawText(QRectF(14, 30, w - 28, 22), Qt::AlignVCenter | Qt::AlignLeft,
                    QFontMetricsF(tf).elidedText(title, Qt::ElideRight, w - 28));
        // Começo do texto
        if (m_richDoc && !m_richDoc->isEmpty()) {
            qreal cx, cy, cw, ch;
            richContentRect(cx, cy, cw, ch);
            p->save();
            p->setClipRect(QRectF(0, cy, w, ch));
            p->translate(cx, cy - m_scrollOffset);
            m_richDoc->drawContents(p);
            p->restore();
            paintScrollbar(p, cy, ch, m_richDoc->size().height(), QColor(255, 255, 255, 110));
        } else {
            qreal cx, cy, cw, ch;
            richContentRect(cx, cy, cw, ch);
            p->setPen(kSheetMuted);
            p->setFont(uiFont(12, QFont::Normal, true));
            p->drawText(QRectF(cx, cy, cw, ch), Qt::AlignLeft | Qt::AlignTop,
                        isCh ? tr("Sem resumo ainda.") : tr("Documento vazio."));
        }
        // Rodapé: palavras · abrir
        p->setFont(uiFont(11));
        p->setPen(kSheetMuted);
        p->drawText(QRectF(14, h - 26, w - 90, 16), Qt::AlignVCenter | Qt::AlignLeft, m_footer);
        QFont of = uiFont(11, m_hoverOpen ? QFont::Bold : QFont::DemiBold);
        p->setFont(of);
        p->setPen(acc);
        p->drawText(QRectF(w - 84, h - 26, 70, 16), Qt::AlignVCenter | Qt::AlignRight,
                    tr("abrir →"));
        paintPin(p, acc);
        if (m_selected) paintSelectionRing(p, outlinePath());
        return;
    }

    // ── Personagem: polaroid (frente) ou a ficha (verso) ────────────────────
    if (t == QStringLiteral("character")) {
        paintShadow(p, QRectF(0, 0, w, h), 2, 4);
        p->setPen(Qt::NoPen);
        p->setBrush(kPolaroid);
        p->drawRoundedRect(QRectF(0, 0, w, h), 2, 2);
        if (!m_showDesc) {
            paintCover(p, QRectF(7, 7, w - 14, h - 7 - kPolaroidFoot), 1);
            const qreal fy = h - kPolaroidFoot;
            const QFont nf = uiFont(14, QFont::Bold);
            p->setFont(nf);
            p->setPen(kPolaroidInk);
            p->drawText(QRectF(9, fy + 6, w - 18, 20), Qt::AlignVCenter | Qt::AlignLeft,
                        QFontMetricsF(nf).elidedText(m_data.title, Qt::ElideRight, w - 18));
            if (!m_role.isEmpty()) {
                const QFont rf = uiFont(10.5);
                p->setFont(rf);
                p->setPen(kPolaroidMuted);
                p->drawText(QRectF(9, fy + 25, w - 18, 14), Qt::AlignVCenter | Qt::AlignLeft,
                            QFontMetricsF(rf).elidedText(m_role.toLower(), Qt::ElideRight, w - 18));
            }
        } else {
            QFont kf = uiFont(9.5, QFont::Bold);
            kf.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
            p->setFont(kf);
            p->setPen(QColor(0x9a, 0x7f, 0x64));
            p->drawText(QRectF(12, 9, w - 24, 14), Qt::AlignVCenter | Qt::AlignLeft,
                        tr("FICHA · DUPLO CLIQUE VOLTA"));
            const QFont nf = uiFont(14, QFont::Bold);
            p->setFont(nf);
            p->setPen(kPolaroidInk);
            p->drawText(QRectF(12, 24, w - 24, 20), Qt::AlignVCenter | Qt::AlignLeft,
                        QFontMetricsF(nf).elidedText(m_data.title, Qt::ElideRight, w - 24));
            if (m_richDoc && !m_richDoc->isEmpty()) {
                qreal cx, cy, cw, ch;
                richContentRect(cx, cy, cw, ch);
                p->save();
                p->setClipRect(QRectF(0, cy, w, ch));
                p->translate(cx, cy - m_scrollOffset);
                m_richDoc->drawContents(p);
                p->restore();
                paintScrollbar(p, cy, ch, m_richDoc->size().height(), QColor(0, 0, 0, 90));
            } else {
                p->setPen(kPolaroidMuted);
                p->setFont(uiFont(11.5, QFont::Normal, true));
                p->drawText(QRectF(12, 48, w - 24, h - 60), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                            tr("A ficha deste personagem ainda está vazia."));
            }
        }
        paintPin(p, m_data.color.isValid() ? m_data.color : QColor(0xf9, 0x73, 0x16));
        if (m_selected) paintSelectionRing(p, outlinePath());
        return;
    }

    // ── Post-it e comentário ────────────────────────────────────────────────
    paintNote(p);
}

void CardItem::paintPathShadow(QPainter* p, const QPainterPath& path, qreal lift) const
{
    // A mesma sombra macia de papel, pra formatos que não são retângulo.
    p->save();
    p->setBrush(Qt::NoBrush);
    const QPainterPath sp = path.translated(0, lift);
    for (int i = 6; i >= 1; --i) {
        p->setPen(QPen(QColor(0, 0, 0, int(7 + (6 - i) * 2)), i * 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPath(sp);
    }
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 40));
    p->drawPath(sp);
    p->restore();
}

void CardItem::paintNote(QPainter* p)
{
    const qreal w = m_data.width;
    const qreal h = m_data.height;
    const QString s = noteShape();
    const bool isComment = (m_data.type == QStringLiteral("comment"));
    const QPainterPath outline = outlinePath();
    const NoteGeo g = noteGeo();
    const QColor bg = m_data.color;
    const QColor paper = paperColor();

    if (m_snapping && m_snapColor.isValid()) {
        p->save();
        p->setBrush(Qt::NoBrush);
        QPen snapPen(m_snapColor, 3.0);
        p->setPen(snapPen);
        p->setOpacity(0.85);
        p->drawRoundedRect(QRectF(-4, -4, w + 8, h + 8), 8, 8);
        p->setPen(QPen(m_snapColor, 12.0));
        p->setOpacity(0.2);
        p->drawRoundedRect(QRectF(-7, -7, w + 14, h + 14), 10, 10);
        p->restore();
    }

    if (s.isEmpty()) paintShadow(p, QRectF(0, 0, w, h), isComment ? 10 : 3, 3);
    else             paintPathShadow(p, outline, s == QStringLiteral("dymo") ? 2 : 3);
    p->setPen(Qt::NoPen);
    p->setBrush(paper);
    p->drawPath(outline);

    if (isComment) {
        QPolygonF tail;
        if (s == QStringLiteral("round"))
            tail << QPointF(w * 0.30, h * 0.93) << QPointF(w * 0.46, h * 0.99) << QPointF(w * 0.24, h + kTailH);
        else
            tail << QPointF(22, h - 1) << QPointF(40, h - 1) << QPointF(22, h + kTailH);
        p->drawPolygon(tail);
    } else if (s.isEmpty()) {
        // A dobrinha: o canto cortado vira uma aba dobrada por cima.
        const qreal f = kFoldSize;
        QPolygonF flap;
        flap << QPointF(w - f, h - f) << QPointF(w, h - f) << QPointF(w - f, h);
        p->setBrush(QColor(0, 0, 0, 60));
        p->drawPolygon(QPolygonF() << QPointF(w - f + 1, h - f + 2) << QPointF(w, h - f + 1)
                                   << QPointF(w - f + 1, h + 1));
        p->setBrush(bg.darker(isDark() ? 85 : 118));
        p->drawPolygon(flap);
    }

    // Pauta, margem e faixa: desenhadas dentro do papel.
    if (s == QStringLiteral("index") || s == QStringLiteral("torn") || s == QStringLiteral("strip")) {
        p->save();
        p->setClipPath(outline);
        if (s == QStringLiteral("strip")) {
            p->fillRect(QRectF(0, 0, 22, h), bg.darker(isDark() ? 80 : 122));
        } else {
            const bool torn = (s == QStringLiteral("torn"));
            p->setPen(QPen(torn ? QColor(0x7a, 0xa3, 0xd4, 90) : QColor(0x6b, 0x9b, 0xd1, 90), 1.0));
            if (torn) p->drawLine(QPointF(0, g.title.top() + 17.5), QPointF(w, g.title.top() + 17.5));
            for (qreal y = g.body.top() + 17.5 - m_scrollOffset; y < h - 3; y += 20.0)
                if (y > g.body.top()) p->drawLine(QPointF(0, y), QPointF(w, y));
            if (torn) {
                p->setPen(QPen(QColor(0xe0, 0x70, 0x70, 120), 1.4));
                p->drawLine(QPointF(22.5, 0), QPointF(22.5, h));
                p->fillRect(QRectF(0, 0, 6, h), bg);
            } else {
                p->setPen(QPen(QColor(0xd9, 0x53, 0x4f, 150), 1.4));
                p->drawLine(QPointF(0, 31), QPointF(w, 31));
            }
        }
        p->restore();
    }

    const QColor ink = inkColor();
    const QColor muted(ink.red(), ink.green(), ink.blue(), 110);

    // Título (duplo clique em cima edita). Sem corpo (tira, redondo, fita) e
    // sem título, aparece o começo do texto: nada some ao trocar de formato.
    if (!m_editingTitle) {
        QString title = m_data.title.trimmed();
        bool fromBody = false;
        if (title.isEmpty() && !g.hasBody) {
            title = plainExcerpt(m_data.content, 80).section(QLatin1Char('\n'), 0, 0).simplified();
            fromBody = !title.isEmpty();
        }
        if (s == QStringLiteral("dymo")) {
            const QString t = (title.isEmpty() ? tr("Sem título") : title).toUpper();
            p->setFont(dymoFont());
            p->setPen(QColor(0, 0, 0, 120));
            p->drawText(g.title.translated(0, 1.2), Qt::AlignCenter, t);
            p->setPen(QColor(255, 255, 255, 45));
            p->drawText(g.title.translated(0, -1.0), Qt::AlignCenter, t);
            p->setPen(QColor(255, 255, 255, title.isEmpty() ? 120 : 235));
            p->drawText(g.title, Qt::AlignCenter, t);
        } else if (title.isEmpty()) {
            if (m_hovered || m_selected) {
                p->setFont(uiFont(12.5, QFont::Normal, true));
                p->setPen(muted);
                p->drawText(g.title, (g.centered ? Qt::AlignCenter : Qt::AlignVCenter | Qt::AlignLeft),
                            tr("Sem título"));
            }
        } else if (g.centered) {
            const QFont tf = uiFont(s == QStringLiteral("round") ? 15 : 13, fromBody ? QFont::Normal : QFont::Bold);
            p->setFont(tf);
            p->setPen(ink);
            p->save();
            p->setClipRect(g.title);
            p->drawText(g.title, Qt::AlignCenter | Qt::TextWordWrap, title);
            p->restore();
        } else {
            const QFont tf = uiFont(13, fromBody ? QFont::Normal : QFont::Bold);
            p->setFont(tf);
            p->setPen(ink);
            p->drawText(g.title, Qt::AlignVCenter | Qt::AlignLeft,
                        QFontMetricsF(tf).elidedText(title, Qt::ElideRight, g.title.width()));
        }
    }

    // Placeholder quando vazio
    if (g.hasBody && m_textItem && m_textItem->document()->isEmpty() && (m_hovered || m_selected)) {
        p->setPen(QColor(ink.red(), ink.green(), ink.blue(), 80));
        p->setFont(uiFont(13));
        p->drawText(g.body, Qt::AlignLeft | Qt::AlignTop, tr("Escreva aqui…"));
    }

    {
        qreal visH = 0.0, contentH = 0.0;
        if (scrollRegion(visH, contentH))
            paintScrollbar(p, g.body.top(), visH, contentH,
                           QColor(ink.red(), ink.green(), ink.blue(), 140));
    }

    // Alça de tamanho no canto (no post-it quadrado, a dobrinha já é a alça)
    if (s.isEmpty() && !isComment) {
        if (m_hoverResize) {
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(0, 0, 0, 30));
            p->drawPolygon(QPolygonF() << QPointF(w - kFoldSize, h - kFoldSize)
                                       << QPointF(w, h - kFoldSize) << QPointF(w - kFoldSize, h));
        }
    } else if (s != QStringLiteral("dymo") && (m_hoverResize || m_selected)) {
        p->setPen(QPen(QColor(ink.red(), ink.green(), ink.blue(), m_hoverResize ? 170 : 90),
                       1.5, Qt::SolidLine, Qt::RoundCap));
        const QPointF o = (s == QStringLiteral("round")) ? QPointF(w * 0.854 + 2, h * 0.854 + 2)
                                                         : QPointF(w - 6, h - 6);
        p->drawLine(QPointF(o.x() - 8, o.y()), QPointF(o.x(), o.y() - 8));
        p->drawLine(QPointF(o.x() - 4, o.y()), QPointF(o.x(), o.y() - 4));
    }

    paintFrame(p, outline);
    paintFastener(p, bg.darker(isDark() ? 70 : 125));
    if (m_selected) paintSelectionRing(p, outline);
}

void CardItem::paintFastener(QPainter* p, const QColor& pinColor) const
{
    const QString f = m_data.fastener;
    const QPointF a = anchorPoint();
    const qreal cx = m_data.width / 2.0;
    if (f.isEmpty()) {
        if (noteShape() == QStringLiteral("tag")) {
            // o barbante sai do furo e sobe até o pin
            p->save();
            p->setPen(QPen(QColor(0x9b, 0x7a, 0x55), 1.6, Qt::SolidLine, Qt::RoundCap));
            p->drawLine(QPointF(cx, 15.0), a);
            p->restore();
        }
        paintPinAt(p, a, pinColor);
        return;
    }
    if (f == QStringLiteral("none")) return;
    p->save();
    if (f == QStringLiteral("tape")) {
        // um pedaço de fita, tortinho, com as pontas rasgadas
        Jitter j(m_data.id, 5);
        p->translate(a);
        p->rotate(-5.0 + j.next() * 10.0);
        const qreal tw = qMin(78.0, qMax(40.0, m_data.width * 0.55)), th = 22.0;
        QPolygonF poly;
        poly << QPointF(-tw / 2, -th / 2) << QPointF(tw / 2, -th / 2);
        for (int i = 1; i <= 5; ++i)
            poly << QPointF(tw / 2 + ((i % 2) ? -3.0 : 0.0), -th / 2 + th * i / 5.0);
        poly << QPointF(-tw / 2, th / 2);
        for (int i = 4; i >= 1; --i)
            poly << QPointF(-tw / 2 + ((i % 2) ? 3.0 : 0.0), -th / 2 + th * i / 5.0);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 22));
        p->drawPolygon(poly.translated(0, 1.3));
        p->setBrush(QColor(236, 226, 196, 215));
        p->drawPolygon(poly);
        p->setBrush(QColor(255, 255, 255, 40));
        p->drawRect(QRectF(-tw / 2 + 4, -th / 2 + 3, tw - 8, 4));
    } else if (f == QStringLiteral("clip")) {
        // clipe de arame, metade pra fora da borda de cima
        p->translate(a.x() - 11.0, a.y() - 2.0);
        QPainterPath cp;
        cp.moveTo(15, 16);
        cp.lineTo(15, 44);
        cp.arcTo(QRectF(5, 39, 10, 10), 0, -180);
        cp.lineTo(5, 9);
        cp.arcTo(QRectF(5, 2, 14, 14), 180, -180);
        cp.lineTo(19, 43);
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(QColor(0, 0, 0, 50), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPath(cp.translated(0.8, 1.4));
        p->setPen(QPen(QColor(0x8d, 0x93, 0x99), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPath(cp);
        p->setPen(QPen(QColor(255, 255, 255, 110), 0.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPath(cp.translated(-0.5, -0.5));
    } else if (f == QStringLiteral("staple")) {
        // grampeado no quadro
        p->translate(a);
        p->rotate(-3.0);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 60));
        p->drawRoundedRect(QRectF(-15, -1, 30, 4), 2, 2);
        QLinearGradient lg(0, -2, 0, 2);
        lg.setColorAt(0, QColor(0xe6, 0xe8, 0xea));
        lg.setColorAt(1, QColor(0x8d, 0x93, 0x99));
        p->setBrush(lg);
        p->drawRoundedRect(QRectF(-15, -2, 30, 4), 2, 2);
    }
    p->restore();
}

void CardItem::paintFrame(QPainter* p, const QPainterPath& outline) const
{
    const QString fr = m_data.frame;
    if (fr.isEmpty()) return;
    const qreal w = m_data.width, h = m_data.height;
    const QString s = noteShape();
    p->save();
    if (fr == QStringLiteral("pen")) {
        // contornado com caneta vermelha, à mão
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(QColor(0xc0, 0x39, 0x2b), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPath(penLoop(QRectF(-6, -6, w + 12, h + 12), s == QStringLiteral("round"), m_data.id));
    } else if (fr == QStringLiteral("cut")) {
        // "destaque aqui": tracejado por dentro e a tesourinha
        QTransform t;
        t.translate(w / 2.0, h / 2.0);
        t.scale((w - 8.0) / w, (h - 8.0) / h);
        t.translate(-w / 2.0, -h / 2.0);
        const QColor ink = inkColor();
        QPen pen(QColor(ink.red(), ink.green(), ink.blue(), 115), 1.3);
        pen.setDashPattern({ 4.0, 3.0 });
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawPath(t.map(outline));
        const bool mid = (s == QStringLiteral("round") || s == QStringLiteral("tag"));
        // a tesourinha fica em cima do tracejado, na sobra acima do título
        const QPointF at(mid ? w / 2.0 + 10.0 : w - 30.0,
                         s == QStringLiteral("torn") ? 22.0 : 4.0);
        p->setPen(Qt::NoPen);
        p->setBrush(paperColor());
        p->drawRect(QRectF(at.x() - 1, at.y() - 7, 16, 14));
        QFont sf(QStringLiteral("Segoe UI Symbol"));
        sf.setPixelSize(13);
        p->setFont(sf);
        p->setPen(QColor(ink.red(), ink.green(), ink.blue(), 190));
        p->drawText(QRectF(at.x(), at.y() - 8, 14, 16), Qt::AlignCenter, QString(QChar(0x2702)));
    } else if (fr == QStringLiteral("corners")) {
        // cantoneiras de álbum de foto
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0x2e, 0x2a, 0x24));
        const qreal k = 22.0, o = 4.0;
        p->drawPolygon(QPolygonF() << QPointF(-o, -o) << QPointF(-o + k, -o) << QPointF(-o, -o + k));
        p->drawPolygon(QPolygonF() << QPointF(w + o, -o) << QPointF(w + o - k, -o) << QPointF(w + o, -o + k));
        p->drawPolygon(QPolygonF() << QPointF(-o, h + o) << QPointF(-o + k, h + o) << QPointF(-o, h + o - k));
        p->drawPolygon(QPolygonF() << QPointF(w + o, h + o) << QPointF(w + o - k, h + o) << QPointF(w + o, h + o - k));
    }
    p->restore();
}

// ── Snap ─────────────────────────────────────────────────────────────────────

void CardItem::setSnapConnected(const QColor& color, const QString& connId)
{
    m_data.color        = color;
    m_data.linkedToConn = connId;
    applyTextColor();
    update();
    emit dataChanged(m_data);
}

void CardItem::setSnapping(bool active, const QColor& color)
{
    if (m_snapping == active && m_snapColor == color) return;
    m_snapping  = active;
    m_snapColor = color;
    update();
}

QVariant CardItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionHasChanged)
        emit positionChanged(m_data.id);
    if ((change == ItemPositionHasChanged || change == ItemRotationHasChanged
         || change == ItemTransformOriginPointHasChanged) && isTextSymbol())
        m_paintedSceneRect = sceneBoundingRect();
    return QGraphicsObject::itemChange(change, value);
}

// ── Hit-test ────────────────────────────────────────────────────────────────

bool CardItem::isOnPin(const QPointF& p) const
{
    if (isSticker()) return false;   // adesivo não tem pin: não liga linha
    return QLineF(p, anchorPoint()).length() <= 10.0;
}

bool CardItem::isOnResizeZone(const QPointF& p) const
{
    if (isSticker() || noteShape() == QStringLiteral("dymo")) return false;   // a fita segue o título
    if (noteShape() == QStringLiteral("round"))
        return QRectF(m_data.width * 0.854 - 10, m_data.height * 0.854 - 10, 20, 20).contains(p);
    return QRectF(m_data.width - 18, m_data.height - 18, 20, 20).contains(p);
}

bool CardItem::isOnOpenLink(const QPointF& p) const
{
    if (!isPaperDoc()) return false;
    return QRectF(m_data.width - 84, m_data.height - 30, 76, 24).contains(p);
}

// ── Mouse events ────────────────────────────────────────────────────────────

void CardItem::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { e->ignore(); return; }

    const bool wasSelected = m_selected;
    emit cardPressed();

    // Adesivo: alça de girar e cantos de tamanho (Shift na alça = de 15 em 15°).
    if (isSticker()) {
        const bool onRot = wasSelected && !m_data.locked && stkRotateRect().contains(e->pos());
        const bool onCorner = wasSelected && !m_data.locked && stkCornerAt(e->pos()) >= 0;
        if ((e->modifiers() & Qt::ShiftModifier) && !onRot && !onCorner) { e->accept(); return; }
        if (m_data.locked) { e->accept(); return; }   // travado: só marca
        if (onRot) {
            emit gestureStarted();
            m_stkRotating = true;
            e->accept(); return;
        }
        if (onCorner) {
            emit gestureStarted();
            m_stkScaling     = true;
            m_stkCenterScene = mapToScene(QPointF(m_data.width / 2.0, m_data.height / 2.0));
            m_stkPressDist   = qMax(4.0, QLineF(m_stkCenterScene, e->scenePos()).length());
            m_stkPressSize   = QSizeF(m_data.width, m_data.height);
            e->accept(); return;
        }
        emit gestureStarted();
        emit dragStarted(m_data.id);
        m_dragging        = true;
        m_pressScene      = e->scenePos();
        m_pressItemOrigin = pos();
        setCursor(Qt::ClosedHandCursor);
        e->accept(); return;
    }

    // Shift+click em cards = só alterna a seleção. text/symbol usam Shift+arrastar pra girar.
    if ((e->modifiers() & Qt::ShiftModifier) && !isTextSymbol()) {
        e->accept();
        return;
    }

    // ── text / symbol ──
    if (isTextSymbol()) {
        const bool emptyText = m_textItem && m_textItem->document()->isEmpty();
        if (m_editingText && !emptyText && contentBounds().contains(e->pos())) {
            e->ignore();   // o próprio texto cuida do cursor
            return;
        }
        if (wasSelected && tsRotateRect().contains(e->pos())) {
            emit gestureStarted();
            m_rotating    = true;
            m_rotByHandle = true;
            e->accept(); return;
        }
        if (e->modifiers() & Qt::ShiftModifier) {
            emit gestureStarted();
            m_rotating      = true;
            m_rotByHandle   = false;
            m_rotStartDeg   = m_data.rotation;
            m_rotStartScene = e->scenePos();
            e->accept(); return;
        }
        if (wasSelected && tsResizeRect().adjusted(-4, -4, 4, 4).contains(e->pos())) {
            emit gestureStarted();
            m_fontResizing  = true;
            m_pressScene    = e->scenePos();
            m_pressFontSize = effFontSize();
            e->accept(); return;
        }
        if (wasSelected && tsWidthRect().adjusted(-5, -4, 5, 4).contains(e->pos())) {
            emit gestureStarted();
            m_widthResizing = true;
            m_pressScene    = e->scenePos();
            m_pressWrap     = contentBounds().width();
            e->accept(); return;
        }
        emit gestureStarted();
        emit dragStarted(m_data.id);
        m_dragging        = true;
        m_pressScene      = e->scenePos();
        m_pressItemOrigin = pos();
        setCursor(Qt::ClosedHandCursor);
        e->accept(); return;
    }

    // Pin: puxar uma linha — vale pra qualquer card com papel
    if (isOnPin(e->pos())) {
        m_draggingPin = true;
        emit pinDragStarted(m_data.id, pinScenePos());
        e->accept();
        return;
    }

    if (isOnOpenLink(e->pos())) {
        emit openRequested(m_data.id);
        e->accept();
        return;
    }

    if (isOnResizeZone(e->pos())) {
        emit gestureStarted();
        m_resizing   = true;
        m_pressScene = e->scenePos();
        m_pressSize  = QSizeF(m_data.width, m_data.height);
        e->accept();
        return;
    }

    // Post-it e comentário: o corpo é texto; arrasta pela faixa do título.
    if (isNoteLike()) {
        const NoteGeo g = noteGeo();
        if (g.hasBody && e->pos().y() >= g.body.top()) {
            e->ignore();
            return;
        }
    }
    // Imagem escrevendo a legenda: o clique na legenda é do texto.
    if (m_data.type == QStringLiteral("image") && m_editingText
        && e->pos().y() >= m_data.height - kCaptionH) {
        e->ignore();
        return;
    }

    emit gestureStarted();
    emit dragStarted(m_data.id);
    m_dragging        = true;
    m_pressScene      = e->scenePos();
    m_pressItemOrigin = pos();
    setCursor(Qt::ClosedHandCursor);
    e->accept();
}

void CardItem::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
    if (m_stkRotating) {
        const QPointF c = mapToScene(QPointF(m_data.width / 2.0, m_data.height / 2.0));
        const QPointF v = e->scenePos() - c;
        qreal deg = std::atan2(v.y(), v.x()) * 180.0 / kPi + 90.0;
        if (e->modifiers() & Qt::ShiftModifier) deg = std::round(deg / 15.0) * 15.0;
        m_data.rotation = std::fmod(deg + 720.0, 360.0);
        setRotation(m_data.rotation);
        update();
        emit positionChanged(m_data.id);
        e->accept();
        return;
    }
    if (m_stkScaling) {
        const qreal f = QLineF(m_stkCenterScene, e->scenePos()).length() / m_stkPressDist;
        const qreal base = m_data.baseWidth > 0 ? m_data.baseWidth : m_stkPressSize.width();
        const qreal nw = qBound(base * 0.2, m_stkPressSize.width() * f, base * 4.0);
        const qreal nh = nw * m_stkPressSize.height() / qMax(1.0, m_stkPressSize.width());
        prepareGeometryChange();
        m_data.width  = nw;
        m_data.height = nh;
        setTransformOriginPoint(nw / 2.0, nh / 2.0);
        setPos(m_stkCenterScene - QPointF(nw / 2.0, nh / 2.0));   // o centro fica parado
        update();
        emit positionChanged(m_data.id);
        e->accept();
        return;
    }
    if (m_rotating) {
        if (m_rotByHandle) {
            const QPointF pivot = mapToScene(transformOriginPoint());
            const QPointF v = e->scenePos() - pivot;
            const qreal deg = std::atan2(v.y(), v.x()) * 180.0 / 3.14159265358979 + 90.0;
            m_data.rotation = std::fmod(deg + 360.0, 360.0);
        } else {
            const qreal dx = e->scenePos().x() - m_rotStartScene.x();
            m_data.rotation = std::fmod(m_rotStartDeg + dx * 0.5, 360.0);
        }
        setRotation(m_data.rotation);
        emit positionChanged(m_data.id);
        e->accept();
        return;
    }
    if (m_fontResizing) {
        const QPointF d = e->scenePos() - m_pressScene;
        const qreal delta = (d.x() + d.y()) / 2.0;
        const bool isSym = (m_data.type == QStringLiteral("symbol"));
        const int minS = isSym ? 12 : 8;
        const int maxS = isSym ? 800 : 400;
        const int ns = qBound(minS, int(qRound(m_pressFontSize + delta)), maxS);
        if (ns != effFontSize()) {
            m_data.fontSize = ns;
            applyContentFont();
            refreshContentMetrics();
            emit positionChanged(m_data.id);
        }
        e->accept();
        return;
    }
    if (m_widthResizing) {
        const qreal dx = e->scenePos().x() - m_pressScene.x();
        const qreal nw = qBound(40.0, m_pressWrap + dx, 4000.0);
        if (!qFuzzyCompare(nw, m_data.wrapWidth)) {
            m_data.wrapWidth = nw;
            applyContentFont();
            refreshContentMetrics();
            emit positionChanged(m_data.id);
        }
        e->accept();
        return;
    }
    if (m_draggingPin) {
        if (scene()) scene()->update();
        e->accept();
        return;
    }
    if (m_dragging) {
        setPos(m_pressItemOrigin + (e->scenePos() - m_pressScene));
        emit draggedBy(m_data.id, e->scenePos() - m_pressScene);
        e->accept();
        return;
    }
    if (m_resizing) {
        const QPointF d = e->scenePos() - m_pressScene;
        prepareGeometryChange();
        const qreal minW = (m_data.type == QStringLiteral("character")) ? 100.0 : kMinW;
        const qreal minH = (m_data.type == QStringLiteral("image") || m_data.type == QStringLiteral("character"))
                           ? 110.0 : kMinH;
        m_data.width  = qMax(minW, m_pressSize.width()  + d.x());
        m_data.height = qMax(minH, m_pressSize.height() + d.y());
        if (noteShape() == QStringLiteral("strip")) {
            m_data.height = m_pressSize.height();            // a tira só alarga
        } else if (noteShape() == QStringLiteral("round")) {
            const qreal side = qMax(90.0, qMax(m_pressSize.width() + d.x(), m_pressSize.height() + d.y()));
            m_data.width = m_data.height = side;             // continua redondo
        } else if (noteShape() == QStringLiteral("tag")) {
            m_data.height = qMax(120.0, m_data.height);
        }
        updateTextItem();
        update();
        emit positionChanged(m_data.id);
        e->accept();
        return;
    }
    e->ignore();
}

void CardItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
    if (m_stkRotating || m_stkScaling) {
        const bool scaled = m_stkScaling;
        m_stkRotating = m_stkScaling = false;
        m_data.x = pos().x();
        m_data.y = pos().y();
        if (scaled) m_stkCacheKey.clear();   // contorno refeito no tamanho novo
        update();
        emit dataChanged(m_data);
        emit gestureFinished();
        e->accept();
        return;
    }
    if (m_rotating || m_fontResizing || m_widthResizing) {
        m_rotating = m_fontResizing = m_widthResizing = m_rotByHandle = false;
        setCursor(Qt::OpenHandCursor);
        emit dataChanged(m_data);
        emit gestureFinished();
        e->accept();
        return;
    }
    if (m_draggingPin) {
        m_draggingPin = false;
        e->accept();
        return;
    }
    if (m_dragging || m_resizing) {
        const bool wasResizing = m_resizing;
        m_dragging = m_resizing = false;
        setCursor(Qt::ArrowCursor);
        const QPointF p = pos();
        m_data.x = p.x(); m_data.y = p.y();
        if (wasResizing) refreshTilt();
        // Texto em escrita que foi arrastado: o cursor volta pra ele.
        if (m_editingText && m_textItem && m_data.type == QStringLiteral("text"))
            m_textItem->setFocus(Qt::MouseFocusReason);
        emit dataChanged(m_data);
        emit gestureFinished();
        e->accept();
        return;
    }
    e->ignore();
}

// ── Hover ────────────────────────────────────────────────────────────────────

QString CardItem::cardTooltipText() const
{
    if (m_data.type == QStringLiteral("symbol") || isSticker()) return {};

    QStringList parts;
    const QString title = m_data.title.trimmed();
    if (!title.isEmpty()) parts << title;

    if (isNoteLike() || m_data.type == QStringLiteral("text")) {
        QString body = plainExcerpt(m_data.content, 220).simplified();
        if (!body.isEmpty() && body != title) parts << body;
    } else if (m_data.type == QStringLiteral("image") && !m_data.description.isEmpty()) {
        const QString desc = m_data.description.trimmed();
        if (!desc.isEmpty() && desc != title) parts << desc;
    }
    return parts.join(QStringLiteral("\n"));
}

void CardItem::hoverEnterEvent(QGraphicsSceneHoverEvent* e)
{
    m_hovered = true;
    if (!cardTooltipText().isEmpty())
        emit hoverPreviewRequested(m_data, e->screenPos());
    update();
    QGraphicsObject::hoverEnterEvent(e);
}

void CardItem::hoverMoveEvent(QGraphicsSceneHoverEvent* e)
{
    const QPointF lp = e->pos();
    if (isSticker()) {
        if (m_data.locked)                                     setCursor(Qt::ArrowCursor);
        else if (m_selected && stkRotateRect().contains(lp))   setCursor(Qt::CrossCursor);
        else if (m_selected && stkCornerAt(lp) >= 0)
            setCursor((stkCornerAt(lp) == 0 || stkCornerAt(lp) == 3) ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
        else                                                   setCursor(Qt::OpenHandCursor);
        return;
    }
    if (isTextSymbol()) {
        if (m_editingText && contentBounds().contains(lp)) setCursor(Qt::IBeamCursor);
        else if (m_selected && tsRotateRect().contains(lp)) setCursor(Qt::CrossCursor);
        else if (m_selected && tsResizeRect().adjusted(-4, -4, 4, 4).contains(lp)) setCursor(Qt::SizeFDiagCursor);
        else if (m_selected && tsWidthRect().adjusted(-5, -4, 5, 4).contains(lp)) setCursor(Qt::SizeHorCursor);
        else setCursor(Qt::OpenHandCursor);
        return;
    }
    const bool onResize = isOnResizeZone(lp);
    const bool onOpen   = isOnOpenLink(lp);
    if (onResize != m_hoverResize || onOpen != m_hoverOpen) {
        m_hoverResize = onResize;
        m_hoverOpen   = onOpen;
        update();
    }
    if (isOnPin(lp))                                   setCursor(Qt::CrossCursor);
    else if (onOpen)                                   setCursor(Qt::PointingHandCursor);
    else if (onResize)                                 setCursor(Qt::SizeFDiagCursor);
    else if (isNoteLike() && noteGeo().hasBody && lp.y() >= noteGeo().body.top())
                                                       setCursor(Qt::IBeamCursor);
    else                                               setCursor(Qt::OpenHandCursor);
}

void CardItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* e)
{
    emit hoverPreviewDismissed();
    m_hoverResize = m_hoverOpen = false;
    m_hovered = false;
    update();
    setCursor(Qt::ArrowCursor);
    QGraphicsObject::hoverLeaveEvent(e);
}

// ── Duplo clique e menu ──────────────────────────────────────────────────────

void CardItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)
{
    const QString t = m_data.type;
    if (isSticker()) {
        // dois cliques na alça de girar: volta pra 0°
        if (m_selected && !m_data.locked && stkRotateRect().contains(e->pos()) && !qFuzzyIsNull(m_data.rotation)) {
            emit gestureStarted();
            m_data.rotation = 0.0;
            setRotation(0.0);
            update();
            emit dataChanged(m_data);
            emit gestureFinished();
        }
        e->accept();
        return;
    }
    if (t == QStringLiteral("character")) {
        toggleImageDesc(!m_showDesc);
        e->accept();
        return;
    }
    if (t == QStringLiteral("image")) {
        beginCaptionEdit();
        e->accept();
        return;
    }
    if (isPaperDoc()) {
        emit openRequested(m_data.id);
        e->accept();
        return;
    }
    if (t == QStringLiteral("symbol")) {
        QString sym = m_data.content.isEmpty() ? QStringLiteral("★") : m_data.content;
        if (pickSymbol(nullptr, sym, e->screenPos())) setSymbol(sym);
        e->accept();
        return;
    }
    if (t == QStringLiteral("text") && m_textItem) {
        beginTextEdit(false);
        // Cursor onde foi o clique
        const QPointF lp = m_textItem->mapFromParent(e->pos());
        const int pos = m_textItem->document()->documentLayout()->hitTest(lp, Qt::FuzzyHit);
        if (pos >= 0) {
            QTextCursor cur = m_textItem->textCursor();
            cur.setPosition(pos);
            m_textItem->setTextCursor(cur);
        }
        e->accept();
        return;
    }
    if (isNoteLike() && (!noteGeo().hasBody || e->pos().y() < noteGeo().body.top())) {
        beginTitleEdit();
        e->accept();
        return;
    }
    QGraphicsObject::mouseDoubleClickEvent(e);
}

void CardItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* e)
{
    emit cardPressed();
    QMenu menu;
    const QString t = m_data.type;
    auto addColor = [&]() {
        menu.addAction(tr("Cor…"), this, [this]() {
            const QColor nc = ColorPopover::getColor(m_data.color, nullptr, tr("Cor"));
            if (nc.isValid()) setCardColor(nc);
        });
    };
    if (isSticker()) {
        menu.addAction(tr("Espelhar"), this, [this]() { setStickerFlip(!m_data.flipX, m_data.flipY); });
        menu.addAction(tr("Virar de ponta-cabeça"), this, [this]() { setStickerFlip(m_data.flipX, !m_data.flipY); });
        QMenu* ol = menu.addMenu(tr("Contorno"));
        const QList<QPair<QString, QString>> outlines = {
            { QString(), tr("Recorte branco") }, { QStringLiteral("none"), tr("Nenhum") },
            { QStringLiteral("shadow"), tr("Só sombra") } };
        for (const auto& [key, label] : outlines) {
            QAction* a = ol->addAction(label, this, [this, key]() { setStickerOutline(key); });
            a->setCheckable(true);
            a->setChecked(m_data.outline == key);
        }
        menu.addAction(m_data.locked ? tr("Destravar") : tr("Travar"), this,
                       [this]() { setLocked(!m_data.locked); });
    } else if (t == QStringLiteral("image")) {
        menu.addAction(tr("Escolher imagem…"), this, &CardItem::chooseImage);
        menu.addAction(tr("Escrever a legenda"), this, &CardItem::beginCaptionEdit);
        menu.addAction(tr("Criar documento"), this, [this]() { emit createDocRequested(m_data.id); });
    } else if (t == QStringLiteral("symbol")) {
        menu.addAction(tr("Trocar símbolo…"), this, [this]() {
            QString sym = m_data.content;
            if (pickSymbol(nullptr, sym)) setSymbol(sym);
        });
        addColor();
    } else if (t == QStringLiteral("text")) {
        menu.addAction(tr("Escrever"), this, [this]() { beginTextEdit(true); });
        addColor();
    } else if (t == QStringLiteral("character")) {
        menu.addAction(m_showDesc ? tr("Ver a foto") : tr("Ver a ficha"), this,
                       [this]() { toggleImageDesc(!m_showDesc); });
    } else if (isPaperDoc()) {
        menu.addAction(tr("Abrir"), this, [this]() { emit openRequested(m_data.id); });
    } else {
        menu.addAction(tr("Editar o título"), this, &CardItem::beginTitleEdit);
        addColor();
        menu.addAction(tr("Criar documento"), this, [this]() { emit createDocRequested(m_data.id); });
        menu.addAction(tr("Virar evento na Timeline"), this,
                       [this]() { emit createTimelineEventRequested(m_data.id); });
    }
    menu.addSeparator();
    menu.addAction(tr("Guardar na gaveta da lousa"), this, [this]() { emit stashRequested(m_data.id); });
    menu.addAction(tr("Apagar de vez"), this, [this]() { emit deleteRequested(m_data.id); });
    menu.exec(e->screenPos());
}

// ── Adesivo ──────────────────────────────────────────────────────────────────

void CardItem::loadStickerFromContent()
{
    m_stkSrc = QImage();
    if (!m_data.content.isEmpty())
        m_stkSrc.loadFromData(QByteArray::fromBase64(m_data.content.toLatin1()));
    if (!m_stkSrc.isNull())
        m_stkSrc = m_stkSrc.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    m_stkCache = QImage();
    m_stkCacheKey.clear();
}

void CardItem::ensureStickerCache()
{
    if (m_stkSrc.isNull() || m_data.width < 1 || m_data.height < 1) return;
    // O contorno é feito numa cópia do tamanho que aparece (com folga pro zoom),
    // não no PNG inteiro: fica rápido e a borda branca tem sempre a mesma grossura.
    const qreal longest = qMax(m_data.width, m_data.height);
    const qreal want = qBound(240.0, longest * 2.5, 1600.0);
    const qreal sc = qMin(1.0, want / qMax(m_stkSrc.width(), m_stkSrc.height()));
    const QSize ws(qMax(1, qRound(m_stkSrc.width() * sc)), qMax(1, qRound(m_stkSrc.height() * sc)));
    const QString ol = m_data.outline;
    const QString key = QStringLiteral("%1|%2x%3|%4|%5|%6").arg(ol).arg(ws.width()).arg(ws.height())
        .arg(m_data.flipX).arg(m_data.flipY).arg(qRound(ws.width() / m_data.width * 8));
    if (key == m_stkCacheKey && !m_stkCache.isNull()) return;
    m_stkCacheKey = key;

    QImage work = (ws == m_stkSrc.size()) ? m_stkSrc
                : m_stkSrc.scaled(ws, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (m_data.flipX || m_data.flipY) work = work.mirrored(m_data.flipX, m_data.flipY);
    work = work.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    const qreal k = ws.width() / m_data.width;             // px da cópia por px do quadro
    const bool cut    = ol.isEmpty();
    const bool shadow = (ol != QStringLiteral("none"));
    const qreal border = cut ? 3.2 * k : 0.0;
    const qreal lift   = shadow ? 3.0 * k : 0.0;
    const qreal blur   = shadow ? 3.0 * k : 0.0;
    const int pad = int(std::ceil(border + lift + blur * 2.0 + 2.0));
    m_stkCachePad = pad;
    const QSize full(ws.width() + pad * 2, ws.height() + pad * 2);

    // Silhueta branca do desenho: base do recorte e da sombra.
    QImage sil(ws, QImage::Format_ARGB32_Premultiplied);
    sil.fill(Qt::transparent);
    {
        QPainter sp(&sil);
        sp.drawImage(0, 0, work);
        sp.setCompositionMode(QPainter::CompositionMode_SourceIn);
        sp.fillRect(sil.rect(), Qt::white);
    }
    QImage body(full, QImage::Format_ARGB32_Premultiplied);
    body.fill(Qt::transparent);
    {
        QPainter bp(&body);
        bp.setRenderHint(QPainter::SmoothPixmapTransform);
        if (cut) {
            // a borda branca: a silhueta carimbada em volta, num círculo
            for (int ring = 1; ring <= 2; ++ring) {
                const qreal r = border * ring / 2.0;
                const int steps = ring == 1 ? 12 : 24;
                for (int i = 0; i < steps; ++i) {
                    const qreal a = i * 2.0 * kPi / steps;
                    bp.drawImage(QPointF(pad + std::cos(a) * r, pad + std::sin(a) * r), sil);
                }
            }
        } else {
            bp.drawImage(pad, pad, sil);
        }
    }
    QImage out(full, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter op(&out);
    op.setRenderHint(QPainter::SmoothPixmapTransform);
    if (shadow) {
        // sombra: a mesma forma em preto, borrada (encolhe e estica de volta)
        QImage dark = body;
        {
            QPainter dp(&dark);
            dp.setCompositionMode(QPainter::CompositionMode_SourceIn);
            dp.fillRect(dark.rect(), Qt::black);
        }
        const int fct = qMax(2, qRound(blur));
        QImage small = dark.scaled(qMax(1, full.width() / fct), qMax(1, full.height() / fct),
                                   Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const QImage soft = small.scaled(full, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        op.setOpacity(0.30);
        op.drawImage(QPointF(0, lift), soft);
        op.setOpacity(1.0);
    }
    if (cut) op.drawImage(0, 0, body);
    op.drawImage(pad, pad, work);
    op.end();
    m_stkCache = out;
}

QRectF CardItem::stkRotateRect() const
{
    return QRectF(m_data.width / 2.0 - 10.0, -44.0, 20.0, 20.0);
}

int CardItem::stkCornerAt(const QPointF& p) const
{
    const qreal w = m_data.width, h = m_data.height;
    const QPointF c[4] = { { -8, -8 }, { w + 8, -8 }, { -8, h + 8 }, { w + 8, h + 8 } };
    for (int i = 0; i < 4; ++i)
        if (QLineF(p, c[i]).length() <= 10.0) return i;
    return -1;
}

void CardItem::paintSticker(QPainter* p)
{
    const qreal w = m_data.width, h = m_data.height;
    ensureStickerCache();
    if (!m_stkCache.isNull()) {
        const qreal k = (m_stkCache.width() - 2.0 * m_stkCachePad) / w;
        const qreal padD = m_stkCachePad / k;
        p->drawImage(QRectF(-padD, -padD, w + 2 * padD, h + 2 * padD), m_stkCache);
    } else {
        QPen pen(QColor(0, 0, 0, 90), 1.2, Qt::DashLine);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(QRectF(0, 0, w, h), 6, 6);
    }

    if (m_selected) {
        const QColor a = accent();
        const QColor handleFill = s_boardLight ? QColor(255, 255, 255) : QColor(0x23, 0x23, 0x22);
        QPen dash(a, 1.3);
        dash.setDashPattern({ 4.0, 3.0 });
        p->setPen(dash);
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(QRectF(-8, -8, w + 16, h + 16), 4, 4);
        if (!m_data.locked) {
            p->setPen(QPen(a, 1.4));
            p->setBrush(handleFill);
            for (const QPointF& c : { QPointF(-8, -8), QPointF(w + 8, -8), QPointF(-8, h + 8), QPointF(w + 8, h + 8) })
                p->drawRoundedRect(QRectF(c.x() - 5, c.y() - 5, 10, 10), 2, 2);
            // alça de girar
            const QPointF rc = stkRotateRect().center();
            p->setPen(QPen(a, 1.3));
            p->drawLine(QPointF(w / 2.0, -8), QPointF(rc.x(), rc.y() + 9));
            p->setPen(Qt::NoPen);
            p->setBrush(a);
            p->drawEllipse(rc, 9, 9);
            p->setPen(QPen(Qt::white, 1.5, Qt::SolidLine, Qt::RoundCap));
            p->setBrush(Qt::NoBrush);
            p->drawArc(QRectF(rc.x() - 4, rc.y() - 4, 8, 8), 60 * 16, 280 * 16);
        } else {
            // travado: um cadeado no canto
            const QPointF lc(w + 8, -8);
            p->setPen(Qt::NoPen);
            p->setBrush(a);
            p->drawEllipse(lc, 10, 10);
            p->setPen(QPen(Qt::white, 1.4));
            p->setBrush(Qt::NoBrush);
            p->drawArc(QRectF(lc.x() - 3, lc.y() - 6, 6, 7), 0, 180 * 16);
            p->setPen(Qt::NoPen);
            p->setBrush(Qt::white);
            p->drawRoundedRect(QRectF(lc.x() - 4.5, lc.y() - 2, 9, 7), 1.5, 1.5);
        }
    }

    if (m_stkScaling || m_stkRotating) {
        // "120% · 15°" de pé, embaixo do adesivo, durante o gesto
        const qreal base = m_data.baseWidth > 0 ? m_data.baseWidth : w;
        int deg = qRound(m_data.rotation) % 360;
        if (deg > 180) deg -= 360;
        const QString t = QStringLiteral("%1% · %2°").arg(qRound(w / base * 100.0)).arg(deg);
        p->save();
        p->translate(w / 2.0, h / 2.0);
        p->rotate(-rotation());
        const QFont f = uiFont(12);
        p->setFont(f);
        const qreal tw = QFontMetricsF(f).horizontalAdvance(t) + 20.0;
        const qreal ext = std::hypot(w, h) / 2.0 + 12.0;
        const QRectF r(-tw / 2.0, ext, tw, 22);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0x2e, 0x2a, 0x24, 235));
        p->drawRoundedRect(r, 11, 11);
        p->setPen(Qt::white);
        p->drawText(r, Qt::AlignCenter, t);
        p->restore();
    }
}

void CardItem::setStickerFlip(bool flipX, bool flipY)
{
    if (!isSticker() || (flipX == m_data.flipX && flipY == m_data.flipY)) return;
    emit gestureStarted();
    m_data.flipX = flipX;
    m_data.flipY = flipY;
    update();
    emit dataChanged(m_data);
    emit gestureFinished();
}

void CardItem::setStickerOutline(const QString& outline)
{
    if (!isSticker() || outline == m_data.outline) return;
    emit gestureStarted();
    m_data.outline = outline;
    update();
    emit dataChanged(m_data);
    emit gestureFinished();
}

void CardItem::setStickerZ(qreal z)
{
    if (!isSticker() || qFuzzyCompare(z, m_data.z)) return;
    m_data.z = z;
    setZValue(z);
    emit dataChanged(m_data);
}

void CardItem::setLocked(bool locked)
{
    if (locked == m_data.locked) return;
    emit gestureStarted();
    prepareGeometryChange();
    m_data.locked = locked;
    update();
    emit dataChanged(m_data);
    emit gestureFinished();
}

QSizeF CardItem::defaultNoteSize(const QString& shape, const QString& type)
{
    return noteSizeFor(shape, type);
}

QPixmap CardItem::renderPreview(const CanvasCard& data, const QSize& size, qreal dpr)
{
    // Desenha um card de verdade numa cena à parte: a miniatura nunca mente.
    const bool tilt = s_tilt;
    s_tilt = false;
    QGraphicsScene sc;
    auto* it = new CardItem(data);
    sc.addItem(it);
    s_tilt = tilt;
    QRectF src = it->mapRectToScene(it->shape().boundingRect());
    if (data.type == QStringLiteral("comment")) src.setBottom(src.bottom() + kTailH);
    if (!data.frame.isEmpty()) src.adjust(-8, -8, 8, 8);
    src.adjust(-4, -4, 4, 4);
    QPixmap pm(QSize(qRound(size.width() * dpr), qRound(size.height() * dpr)));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    sc.render(&p, QRectF(QPointF(0, 0), QSizeF(size)), src, Qt::KeepAspectRatio);
    p.end();
    return pm;
}
