#include "CardItem.h"

#include <algorithm>
#include <cmath>
#include <QBuffer>
#include <QFontComboBox>
#include <QLineEdit>
#include <QColorDialog>
#include <QDialog>
#include <QFileDialog>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGraphicsProxyWidget>
#include <QGraphicsRectItem>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QImage>
#include <QKeyEvent>
#include <QRadialGradient>
#include <QRegularExpression>
#include <QStyleOption>
#include <QTextBrowser>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextOption>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

const QColor kNotePalette[] = {
    QColor(QStringLiteral("#ffd060")), QColor(QStringLiteral("#ffb347")),
    QColor(QStringLiteral("#ff8fab")), QColor(QStringLiteral("#ff6b6b")),
    QColor(QStringLiteral("#a8e6cf")), QColor(QStringLiteral("#87ceeb")),
    QColor(QStringLiteral("#c9b1ff")), QColor(QStringLiteral("#f8f8f8")),
};
const QColor kCommentPalette[] = {
    QColor(QStringLiteral("#a78bfa")), QColor(QStringLiteral("#60a5fa")),
    QColor(QStringLiteral("#f472b6")), QColor(QStringLiteral("#34d399")),
    QColor(QStringLiteral("#fb923c")), QColor(QStringLiteral("#94a3b8")),
    QColor(QStringLiteral("#fbbf24")), QColor(QStringLiteral("#f87171")),
};

// Subclasse com boundingRect fixo — garante que toda a área do card captura
// cliques para edição, independente do tamanho real do texto.
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

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) override {
        // Card de imagem: double-click no texto → fecha a descrição e volta à imagem.
        // (Igual ao Mira 1: <textarea onDoubleClick={() => setShowDesc(false)} />)
        if (auto* card = dynamic_cast<CardItem*>(parentItem())) {
            const QString t = card->cardData().type;
        if (t == QStringLiteral("image") || t == QStringLiteral("character")) {
                card->toggleImageDesc(false);
                e->accept();
                return;
            }
        }
        QGraphicsTextItem::mouseDoubleClickEvent(e);
    }

    void focusOutEvent(QFocusEvent* e) override {
        QGraphicsTextItem::focusOutEvent(e);
        if (auto* card = dynamic_cast<CardItem*>(parentItem()))
            card->onTextEditFinished();
    }
};

// Editor inline do título do card (note/comment). Uma linha: Enter/Esc encerram.
class TitleEditItem : public QGraphicsTextItem {
public:
    explicit TitleEditItem(QGraphicsItem* p) : QGraphicsTextItem(p) {}
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

// Injeta width="maxPx" em todas as <img> para que QTextDocument respeite
// o limite da coluna e não renderize imagens no tamanho original.
QString htmlWithScaledImages(const QString& html, int maxPx)
{
    if (html.isEmpty() || maxPx <= 0) return html;
    static const QRegularExpression kImgRe(
        QStringLiteral("<img\\b"), QRegularExpression::CaseInsensitiveOption);
    QString out = html;
    out.replace(kImgRe, QStringLiteral("<img width=\"%1\"").arg(maxPx));
    return out;
}

bool calcIsDark(const QColor& c)
{
    return (c.red() * 299 + c.green() * 587 + c.blue() * 114) / 1000 < 128;
}

// Catálogo de símbolos (igual ao LOUSA_SYMBOLS do Mira 1)
const QStringList kLousaSymbols = {
    "★","☆","✦","✧","✩","✪","✰","⭐","💫","✨",
    "●","○","◉","◎","◆","◇","♦","▲","△","▼","▽","◀","▶","⬡","⬢",
    "×","✕","✖","✗","✘","✚","✛","✝","✞","+","−","÷","=","≠","≈","∞","±","√","∑",
    "→","←","↑","↓","↗","↘","↙","↖","↔","↕","⇒","⇐","⇑","⇓","⇔","➔","➤",
    "✿","❀","❁","✽","✾","❋","❆","❅","⚜","🌸","🌺","🍃",
    "☽","☾","☀","☁","⚡","❄","🌙","☄","⊕","⊗","⊙",
    "⚔","🗡","🛡","👁","🌹","🕊","🐉","🦋","🌿","🍂","⚙","⚛","🔮",
    "Ω","Δ","Σ","Φ","Ψ","Λ","Θ","α","β","γ","π",
    "†","‡","§","¶","©","®","™","⁂","※","⌘","⌛",
    "«","»","—","–","…","⸻",
};

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

    const bool isImg   = (m_data.type == QStringLiteral("image"));
    const bool isChar  = (m_data.type == QStringLiteral("character"));
    const bool isDoc   = (m_data.type == QStringLiteral("doc"));

    const bool isText  = (m_data.type == QStringLiteral("text"));
    const bool isSym   = (m_data.type == QStringLiteral("symbol"));

    // Pixmap: image usa content (base64 puro), character usa photoDataUrl (data URL)
    if (isImg)  loadPixmapFromContent();
    if (isChar) loadCharacterPhoto();

    // ── text / symbol: elementos flutuantes (sem fundo). Texto editável via         ──
    //    QGraphicsTextItem; símbolo pintado. Rotação por Shift+drag, resize=fonte.   ──
    if (isText || isSym) {
        if (isText) {
            auto* bti  = new BodyTextItem(this);
            bti->bodyW = 0; bti->bodyH = 0;      // tamanho natural, sem mínimo
            m_textItem = bti;
            m_textItem->setTextInteractionFlags(Qt::NoTextInteraction); // edita só no double-click
            m_textItem->setTextWidth(-1);        // sem quebra de linha (whiteSpace:pre)
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
        }
        refreshContentMetrics();
    }
    // ── doc / character: conteúdo rico pintado via QTextDocument (read-only,        ──
    //    scroll por translate). Mesma técnica confiável do doc card.                 ──
    else if (isDoc || isChar) {
        rebuildRichDoc();
    } else {
        m_bodyClip = new QGraphicsRectItem(this);
        m_bodyClip->setPen(Qt::NoPen);
        m_bodyClip->setBrush(Qt::NoBrush);
        m_bodyClip->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);

        auto* bti  = new BodyTextItem(m_bodyClip);
        m_textItem = bti;

        if (isImg) {
            // Overlay de descrição: começa escondido, double-click mostra
            m_textItem->setVisible(false);
            m_textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
            m_textItem->document()->setPlainText(m_data.description);
            connect(m_textItem->document(), &QTextDocument::contentsChanged, this, [this]() {
                m_data.description = m_textItem->document()->toPlainText();
                emit dataChanged(m_data);
            });
        } else {
            // note/comment: texto editável
            m_textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
            m_textItem->document()->setPlainText(m_data.content);
            connect(m_textItem->document(), &QTextDocument::contentsChanged, this, [this]() {
                m_data.content = m_textItem->document()->toPlainText();
                emit dataChanged(m_data);
            });
        }

        m_textItem->setAcceptHoverEvents(false);
        QTextOption opt;
        opt.setAlignment(Qt::AlignLeft);
        opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        m_textItem->document()->setDefaultTextOption(opt);
        m_textItem->document()->setDocumentMargin(0);

        updateTextItem();
        applyTextColor();
    }
}

// ── Conteúdo rico (doc / character) ──────────────────────────────────────────

void CardItem::rebuildRichDoc()
{
    const bool isChar = (m_data.type == QStringLiteral("character"));
    const bool isDoc  = (m_data.type == QStringLiteral("doc"));
    if (!isChar && !isDoc) return;
    if (!m_richDoc) m_richDoc = new QTextDocument(this);

    static const QRegularExpression kImg(
        QStringLiteral("<img[^>]*>"), QRegularExpression::CaseInsensitiveOption);

    QFont f(QStringLiteral("Segoe UI"));
    if (isChar) {
        // Mesma aparência do doc card. O HTML do editor vem com estilos inline
        // (body font-family:Alegreya 16pt, <p align="center"> text-indent) que o
        // setDefaultStyleSheet não vence — então normalizamos o documento à força.
        f.setPointSize(9);
        m_richDoc->setDefaultFont(f);
        m_richDoc->setDefaultStyleSheet(QStringLiteral(
            "body,p,div,span,li{color:rgba(255,255,255,0.92); text-align:left;}"));
        const QString html = m_data.content.trimmed().isEmpty()
            ? QStringLiteral("<p><em>Doc vazio</em></p>")
            : QString(m_data.content).remove(kImg);
        m_richDoc->setHtml(html);

        QTextCursor cur(m_richDoc);
        cur.select(QTextCursor::Document);
        QTextCharFormat cf;
        cf.setFontFamilies({QStringLiteral("Segoe UI")});
        cf.setFontPointSize(9);
        cf.setForeground(QColor(255, 255, 255, 235));
        cur.mergeCharFormat(cf);   // preserva negrito/itálico, troca família/tamanho/cor
        for (QTextBlock b = m_richDoc->begin(); b.isValid(); b = b.next()) {
            QTextCursor bc(b);
            QTextBlockFormat bf = b.blockFormat();
            bf.setAlignment(Qt::AlignLeft);
            bf.setTextIndent(0);
            bf.setLeftMargin(0);
            bf.setRightMargin(0);
            bf.setTopMargin(0);
            bf.setBottomMargin(4);
            bc.setBlockFormat(bf);
        }
    } else {
        // doc card: branco translúcido, fonte um pouco menor, imagens escaladas à coluna
        f.setPointSize(9);
        m_richDoc->setDefaultFont(f);
        m_richDoc->setDefaultStyleSheet(QStringLiteral(
            "body,p,div,span,li{color:rgba(255,255,255,0.85); text-align:left; margin:0 0 4px 0;}"));
        m_richDoc->setHtml(m_data.content.isEmpty()
            ? QString()
            : htmlWithScaledImages(m_data.content, qRound(m_data.width - 20)));
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
        x = 10.0; y = 34.0;
        w = qMax(10.0, W - 20.0);
        h = qMax(10.0, H - 34.0 - 10.0);
    } else { // doc
        x = 9.0;  y = kDocHeaderH + 20.0;
        w = qMax(10.0, W - 18.0);
        h = qMax(10.0, H - y - 6.0);
    }
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
    // text
    if (m_textItem) {
        const QRectF r = m_textItem->boundingRect();
        return QRectF(0, 0, qMax(24.0, r.width()), qMax(24.0, r.height()));
    }
    return QRectF(0, 0, 40, 28);
}

void CardItem::controlRects(QRectF& del, QRectF& color, QRectF& sym) const
{
    const QRectF cb = contentBounds();
    const qreal y = cb.top() - 24.0;
    const qreal r = cb.right();
    del   = QRectF(r - 18, y, 18, 18);
    color = QRectF(r - 40, y, 18, 18);
    sym   = QRectF(r - 62, y, 18, 18); // usado só em symbol
}

QRectF CardItem::tsResizeRect() const
{
    const QRectF cb = contentBounds();
    return QRectF(cb.right() - 11, cb.bottom() - 11, 18, 18);
}

QRectF CardItem::tsRotateRect() const
{
    const QRectF cb = contentBounds();
    const QPointF c(cb.center().x(), cb.top() - 18.0);
    return QRectF(c.x() - 9, c.y() - 9, 18, 18);
}

void CardItem::applyContentFont()
{
    if (m_data.type != QStringLiteral("text") || !m_textItem) return;
    m_textItem->setFont(contentFont());
    m_textItem->setDefaultTextColor(m_data.color.isValid() ? m_data.color : QColor(Qt::white));
}

void CardItem::refreshContentMetrics()
{
    if (!isTextSymbol()) return;
    prepareGeometryChange();
    setTransformOriginPoint(contentBounds().center());
    setRotation(m_data.rotation);
    update();
}

void CardItem::onTextEditFinished()
{
    if (m_data.type == QStringLiteral("text") && m_textItem)
        m_textItem->setTextInteractionFlags(Qt::NoTextInteraction);
}

void CardItem::beginTitleEdit()
{
    if (m_data.type != QStringLiteral("note") && m_data.type != QStringLiteral("comment"))
        return;
    if (!m_titleEditor) {
        m_titleEditor = new TitleEditItem(this);
        m_titleEditor->setAcceptHoverEvents(false);
        m_titleEditor->document()->setDocumentMargin(0);
        m_titleEditor->setTextWidth(-1);  // uma linha (sem quebra)
    }
    QFont f(QStringLiteral("Segoe UI"), 9, QFont::Bold);
    m_titleEditor->setFont(f);
    m_titleEditor->setDefaultTextColor(contrastColor());
    {
        QSignalBlocker bl(m_titleEditor->document());
        m_titleEditor->document()->setPlainText(m_data.title);
    }
    const qreal th = m_titleEditor->boundingRect().height();
    m_titleEditor->setPos(20.0, (kHeaderH - th) / 2.0);
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
        emit dataChanged(m_data);
    }
    update();
}

void CardItem::setCardSelected(bool on)
{
    if (m_selected == on) return;
    m_selected = on;
    if (!on && m_data.type == QStringLiteral("text"))
        onTextEditFinished(); // ao desselecionar, sai do modo edição
    update();
}

bool CardItem::pickSymbol(QWidget* parent, QString& symbol, QColor& color)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(tr("Escolher símbolo"));
    dlg.resize(380, 360);
    auto* outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(10);

    // ── Linha da cor ──
    QColor chosen = color.isValid() ? color : QColor(Qt::white);
    auto* colorRow = new QHBoxLayout;
    colorRow->setSpacing(8);
    auto* colorLbl = new QLabel(tr("Cor:"), &dlg);
    auto* swatch = new QToolButton(&dlg);
    swatch->setFixedSize(28, 28);
    swatch->setCursor(Qt::PointingHandCursor);
    auto paintSwatch = [&]() {
        swatch->setStyleSheet(QStringLiteral(
            "QToolButton{background:%1;border:1px solid rgba(255,255,255,0.3);border-radius:6px;}")
            .arg(chosen.name()));
    };
    paintSwatch();
    QObject::connect(swatch, &QToolButton::clicked, &dlg, [&]() {
        const QColor nc = QColorDialog::getColor(chosen, &dlg, tr("Cor do símbolo"));
        if (nc.isValid()) { chosen = nc; paintSwatch(); }
    });
    auto* hint = new QLabel(tr("escolha a cor e clique num símbolo"), &dlg);
    hint->setStyleSheet(QStringLiteral("color:rgba(255,255,255,0.45);font-size:11px;"));
    colorRow->addWidget(colorLbl);
    colorRow->addWidget(swatch);
    colorRow->addWidget(hint, 1);
    outer->addLayout(colorRow);

    // ── Grade de símbolos ──
    auto* scroll = new QScrollArea(&dlg);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* grid_w = new QWidget;
    auto* grid = new QGridLayout(grid_w);
    grid->setSpacing(2);

    bool accepted = false;
    QString picked = symbol;
    int n = 0;
    for (const QString& s : kLousaSymbols) {
        auto* b = new QToolButton(grid_w);
        b->setText(s);
        b->setFixedSize(38, 38);
        QFont bf(QStringLiteral("Segoe UI Symbol"));
        bf.setPixelSize(20);
        b->setFont(bf);
        b->setCheckable(true);
        b->setChecked(s == symbol);
        b->setCursor(Qt::PointingHandCursor);
        QObject::connect(b, &QToolButton::clicked, &dlg, [&, s]() {
            picked   = s;
            accepted = true;
            dlg.accept();
        });
        grid->addWidget(b, n / 9, n % 9);
        ++n;
    }
    scroll->setWidget(grid_w);
    outer->addWidget(scroll, 1);

    dlg.exec();
    if (accepted) { symbol = picked; color = chosen; }
    return accepted;
}

bool CardItem::pickText(QWidget* parent, QString& text, QColor& color, QString& fontFamily)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(tr("Texto livre"));
    dlg.setMinimumWidth(360);
    auto* outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(9);

    // ── Campo de texto (uma linha) ──
    auto* edit = new QLineEdit(text, &dlg);
    edit->setPlaceholderText(tr("Escreva aqui..."));
    outer->addWidget(edit);

    // ── Fonte ──
    auto* fontRow = new QHBoxLayout;
    fontRow->setSpacing(8);
    fontRow->addWidget(new QLabel(tr("Fonte:"), &dlg));
    auto* fontCombo = new QFontComboBox(&dlg);
    if (!fontFamily.isEmpty()) fontCombo->setCurrentFont(QFont(fontFamily));
    else                       fontCombo->setCurrentFont(QFont(QStringLiteral("Segoe UI")));
    fontRow->addWidget(fontCombo, 1);
    outer->addLayout(fontRow);

    // ── Cores predefinidas + picker custom ──
    QColor chosen = color.isValid() ? color : QColor(Qt::white);
    static const char* kPresets[] = {
        "#ffffff", "#111111", "#ff6b6b", "#ffb347", "#ffd060",
        "#34d399", "#60a5fa", "#a78bfa", "#f472b6",
    };
    auto* colorRow = new QHBoxLayout;
    colorRow->setSpacing(4);
    colorRow->addWidget(new QLabel(tr("Cor:"), &dlg));

    QList<QToolButton*> swatches;
    QToolButton* customBtn = nullptr;
    auto refresh = [&]() {
        for (QToolButton* b : swatches) {
            const QColor c(b->property("col").toString());
            const bool sel = (c.rgb() == chosen.rgb());
            b->setStyleSheet(QStringLiteral(
                "QToolButton{background:%1;border:%2;border-radius:5px;}")
                .arg(c.name(),
                     sel ? QStringLiteral("2px solid #6ea8fe")
                         : QStringLiteral("1px solid rgba(255,255,255,0.25)")));
        }
        if (customBtn) {
            const bool isPreset = std::any_of(swatches.begin(), swatches.end(),
                [&](QToolButton* b){ return QColor(b->property("col").toString()).rgb() == chosen.rgb(); });
            customBtn->setStyleSheet(QStringLiteral(
                "QToolButton{background:%1;border:%2;border-radius:5px;color:#fff;}")
                .arg(isPreset ? QStringLiteral("rgba(255,255,255,0.08)") : chosen.name(),
                     (!isPreset) ? QStringLiteral("2px solid #6ea8fe")
                                 : QStringLiteral("1px solid rgba(255,255,255,0.25)")));
        }
    };
    for (const char* hex : kPresets) {
        auto* b = new QToolButton(&dlg);
        b->setProperty("col", QString::fromLatin1(hex));
        b->setFixedSize(22, 22);
        b->setCursor(Qt::PointingHandCursor);
        swatches.append(b);
        QObject::connect(b, &QToolButton::clicked, &dlg, [&, b]() {
            chosen = QColor(b->property("col").toString());
            refresh();
        });
        colorRow->addWidget(b);
    }
    customBtn = new QToolButton(&dlg);
    customBtn->setFixedSize(22, 22);
    customBtn->setText(QStringLiteral("…"));
    customBtn->setToolTip(tr("Cor personalizada"));
    customBtn->setCursor(Qt::PointingHandCursor);
    QObject::connect(customBtn, &QToolButton::clicked, &dlg, [&]() {
        const QColor nc = QColorDialog::getColor(chosen, &dlg, tr("Cor do texto"));
        if (nc.isValid()) { chosen = nc; refresh(); }
    });
    colorRow->addWidget(customBtn);
    colorRow->addStretch(1);
    outer->addLayout(colorRow);
    refresh();

    // ── Botões ──
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    outer->addWidget(bb);

    edit->setFocus();
    if (dlg.exec() != QDialog::Accepted) return false;
    text       = edit->text();
    color      = chosen;
    fontFamily = fontCombo->currentFont().family();
    return true;
}

void CardItem::openSymbolPicker()
{
    QString sym = m_data.content.isEmpty() ? QStringLiteral("★") : m_data.content;
    QColor  col = m_data.color;
    if (!pickSymbol(nullptr, sym, col)) return;
    m_data.content = sym;
    if (col.isValid()) m_data.color = col;
    applyContentFont();
    refreshContentMetrics();
    update();
    emit dataChanged(m_data);
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
    if (m_data.type == QStringLiteral("doc") ||
        m_data.type == QStringLiteral("character")) rebuildRichDoc();
    if (m_data.type == QStringLiteral("text") && m_textItem) {
        QSignalBlocker bl(m_textItem->document());
        m_textItem->document()->setPlainText(m_data.content);
    }
    if (isTextSymbol()) { applyContentFont(); refreshContentMetrics(); }
    else                updateTextItem();
    applyTextColor();
    update();
}

// ── Geometria ──────────────────────────────────────────────────────────────

QRectF CardItem::boundingRect() const
{
    if (isTextSymbol()) {
        // conteúdo + faixa de controles/alça de rotação no topo + alça de resize
        return contentBounds().adjusted(-6, -34, 14, 14);
    }
    // kMargin cobre: sombra (6) + glow máximo do snap (12px pen width / 2 + margem)
    // Sem isso o glow pintaria fora do rect e deixaria rastro ao mover o card.
    constexpr qreal kMargin = 16.0;
    const qreal extraH = (m_data.type == QStringLiteral("comment")) ? kTailH : 0.0;
    return QRectF(-kMargin, -kMargin,
                  m_data.width  + kMargin * 2,
                  m_data.height + extraH + kMargin * 2);
}

QPainterPath CardItem::shape() const
{
    QPainterPath p;
    if (isTextSymbol()) {
        p.addRect(boundingRect()); // toda a área (conteúdo + controles + alça) é interativa
        return p;
    }
    p.addRoundedRect(QRectF(0, 0, m_data.width, m_data.height), kRadius, kRadius);
    return p;
}

void CardItem::updateTextItem()
{
    if (isTextSymbol()) { refreshContentMetrics(); return; }
    // Relayout do conteúdo rico (doc / character) à largura atual
    if (m_richDoc) {
        qreal x, y, wv, hv;
        richContentRect(x, y, wv, hv);
        m_richDoc->setTextWidth(wv);
    }

    // Reposiciona o BodyTextItem (note / comment / image)
    if (m_textItem) {
        const bool isImg = (m_data.type == QStringLiteral("image"));
        constexpr qreal padL = 10.0, padBot = 17.0;
        const qreal padTop = isImg ? 34.0 : (kHeaderH + 4.0);
        const qreal tw = qMax(10.0, m_data.width  - 2.0 * padL);
        const qreal th = qMax(10.0, m_data.height - padTop - padBot);
        if (auto* bti = static_cast<BodyTextItem*>(m_textItem)) {
            bti->bodyW = tw; bti->bodyH = th;
        }
        m_textItem->setTextWidth(tw);
        if (m_bodyClip)
            m_bodyClip->setRect(QRectF(0, padTop, m_data.width, th));
        m_textItem->setPos(padL, padTop - m_scrollOffset);
    }

    // Mantém o scroll dentro dos limites após resize
    qreal visH = 0.0, contentH = 0.0;
    if (scrollRegion(visH, contentH)) {
        const qreal maxS = qMax(0.0, contentH - visH);
        if (m_scrollOffset > maxS) {
            m_scrollOffset = maxS;
            if (m_textItem) {
                const bool isImg = (m_data.type == QStringLiteral("image"));
                const qreal padTop = isImg ? 34.0 : (kHeaderH + 4.0);
                m_textItem->setPos(10.0, padTop - m_scrollOffset);
            }
        }
    }
}

// ── Cores ──────────────────────────────────────────────────────────────────

bool CardItem::isDark() const { return calcIsDark(m_data.color); }

QColor CardItem::bodyColor() const
{
    // comment: corpo tem cor levemente escurecida (igual Mira 1: darkenHex 0.28)
    return (m_data.type == QStringLiteral("comment"))
        ? m_data.color.darker(140)
        : m_data.color;
}

QColor CardItem::contrastColor() const
{
    // Contraste calculado sobre bodyColor (onde o texto está)
    const QColor bg = bodyColor();
    return calcIsDark(bg) ? QColor(255, 255, 255, 220) : QColor(0, 0, 0, 180);
}

void CardItem::applyTextColor()
{
    if (isTextSymbol()) { applyContentFont(); return; }
    if (!m_textItem) return;
    const QColor tc = (m_data.type == QStringLiteral("image"))
        ? QColor(255, 255, 255, 220)
        : contrastColor();
    m_textItem->setDefaultTextColor(tc);
}

void CardItem::setLinkedHtml(const QString& html)
{
    m_data.content = html;
    if (m_data.type == QStringLiteral("character") ||
        m_data.type == QStringLiteral("doc")) {
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
    // photoDataUrl pode ser data URL completo ("data:image/...;base64,<b64>")
    // ou base64 puro. Ambos suportados.
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

void CardItem::openImagePicker()
{
    const QString path = QFileDialog::getOpenFileName(
        nullptr, tr("Escolher imagem"), QString(),
        tr("Imagens (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
    if (path.isEmpty()) return;

    QImage img(path);
    if (img.isNull()) return;
    // Comprime: máximo 900px no lado maior, JPEG 82
    if (img.width() > 900 || img.height() > 900)
        img = img.scaled(900, 900, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", 82);

    m_data.content = QString::fromLatin1(ba.toBase64());
    loadPixmapFromContent();
    update();
    emit dataChanged(m_data);
}

void CardItem::toggleImageDesc(bool show)
{
    m_showDesc = show;
    if (m_textItem) {
        m_textItem->setVisible(show);
        if (show) m_textItem->setFocus();
    }
    if (!show) m_scrollOffset = 0.0; // reset scroll ao fechar
    update();
}

bool CardItem::scrollRegion(qreal& visH, qreal& contentH) const
{
    const QString t = m_data.type;
    if (t == QStringLiteral("doc") || t == QStringLiteral("character")) {
        if (t == QStringLiteral("character") && !m_showDesc) return false;
        qreal x, y, wv, hv;
        richContentRect(x, y, wv, hv);
        visH     = hv;
        contentH = m_richDoc ? m_richDoc->size().height() : 0.0;
        return true;
    }
    if (!m_textItem) return false;
    const bool isImg = (t == QStringLiteral("image"));
    if (isImg && !m_showDesc) return false; // descrição fechada → nada a rolar
    const qreal padTop = isImg ? 34.0 : (kHeaderH + 4.0);
    constexpr qreal padBot = 17.0;
    visH     = qMax(0.0, m_data.height - padTop - padBot);
    contentH = m_textItem->boundingRect().height();
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
    if (m_textItem) updateTextItem(); // reposiciona note/comment/image
    update();
    return true; // consome o wheel: não dá zoom enquanto o cursor está sobre o card
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

void CardItem::paintSelectionRing(QPainter* p) const
{
    // Contorno azul + glow para cards selecionados (igual Mira 1: 2px #6ea8fe + glow).
    const qreal w = m_data.width, h = m_data.height;
    p->save();
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(QColor(110, 168, 254, 60), 6));
    p->drawRoundedRect(QRectF(-1, -1, w + 2, h + 2), kRadius + 1, kRadius + 1);
    p->setPen(QPen(QColor(QStringLiteral("#6ea8fe")), 2));
    p->drawRoundedRect(QRectF(-1, -1, w + 2, h + 2), kRadius + 1, kRadius + 1);
    p->restore();
}

// ── Pintura ────────────────────────────────────────────────────────────────

void CardItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    const qreal w = m_data.width;
    const qreal h = m_data.height;
    const QColor& bg = m_data.color;

    p->setRenderHint(QPainter::Antialiasing);

    // ── text / symbol: elementos flutuantes ────────────────────────────────
    if (isTextSymbol()) {
        const bool isSym = (m_data.type == QStringLiteral("symbol"));
        const QRectF cb = contentBounds();
        const QColor col = m_data.color.isValid() ? m_data.color : QColor(Qt::white);

        if (isSym) {
            p->setRenderHint(QPainter::TextAntialiasing);
            p->setFont(contentFont());
            p->setPen(col);
            p->drawText(cb, Qt::AlignCenter,
                        m_data.content.isEmpty() ? QStringLiteral("★") : m_data.content);
        } else if (m_textItem && m_textItem->document()->toPlainText().isEmpty()) {
            // placeholder "Aa" (texto real é desenhado pelo m_textItem filho)
            p->setFont(contentFont());
            p->setPen(QColor(255, 255, 255, 70));
            p->drawText(cb, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Aa"));
        }

        // Controles flutuantes (só quando selecionado)
        if (m_selected) {
            // Contorno azul ao redor do conteúdo (igual Mira 1: outline 2px offset 4)
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(QColor(QStringLiteral("#6ea8fe")), 2));
            p->drawRoundedRect(cb.adjusted(-4, -4, 4, 4), 4, 4);

            QRectF del, color, sym;
            controlRects(del, color, sym);
            auto pill = [&](const QRectF& r) {
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(0, 0, 0, 140));
                p->drawRoundedRect(r, 4, 4);
            };
            // cor
            pill(color);
            p->setBrush(col);
            p->setPen(QPen(QColor(255, 255, 255, 90), 1));
            p->drawEllipse(color.center(), 6, 6);
            // re-escolher símbolo
            if (isSym) {
                pill(sym);
                p->setPen(QColor(255, 255, 255, 210));
                QFont sf(QStringLiteral("Segoe UI Symbol")); sf.setPixelSize(12);
                p->setFont(sf);
                p->drawText(sym, Qt::AlignCenter, QStringLiteral("✦"));
            }
            // remover
            pill(del);
            p->setPen(QPen(QColor(255, 255, 255, 210), 1.3, Qt::SolidLine, Qt::RoundCap));
            const QPointF c = del.center();
            p->drawLine(c + QPointF(-3, -3), c + QPointF(3, 3));
            p->drawLine(c + QPointF(3, -3), c + QPointF(-3, 3));

            // Alça de rotação (topo-centro): haste + círculo com seta circular
            const QPointF rc = tsRotateRect().center();
            p->setPen(QPen(QColor(255, 255, 255, 90), 1.2));
            p->drawLine(QPointF(cb.center().x(), cb.top()), QPointF(rc.x(), rc.y() + 7));
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(0, 0, 0, 150));
            p->drawEllipse(rc, 7.5, 7.5);
            p->setPen(QPen(QColor(255, 255, 255, 220), 1.4));
            p->setBrush(Qt::NoBrush);
            p->drawArc(QRectF(rc.x() - 4, rc.y() - 4, 8, 8), 40 * 16, 270 * 16);
        }

        // Alça de resize de fonte (só quando selecionado)
        if (m_selected) {
            const QColor rh(255, 255, 255, 180);
            p->setPen(QPen(rh, 1.5, Qt::SolidLine, Qt::RoundCap));
            const qreal ox = cb.right(), oy = cb.bottom();
            p->drawLine(QPointF(ox - 8, oy), QPointF(ox, oy - 8));
            p->drawLine(QPointF(ox - 5, oy), QPointF(ox, oy - 5));
            p->drawLine(QPointF(ox - 2, oy), QPointF(ox, oy - 2));
        }
        return;
    }

    // ── Card de imagem: layout completamente diferente ──────────────────────
    if (m_data.type == QStringLiteral("image")) {
        // Sombra
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 40));
        p->drawRoundedRect(QRectF(3, 5, w, h), 10, 10);

        // Fundo escuro
        p->setPen(QPen(QColor(255, 255, 255, 25), 1));
        p->setBrush(QColor(QStringLiteral("#111111")));
        p->drawRoundedRect(QRectF(0, 0, w, h), 10, 10);

        // Imagem (objectFit: cover)
        if (!m_pixmap.isNull()) {
            p->save();
            QPainterPath clip;
            clip.addRoundedRect(QRectF(0, 0, w, h), 10, 10);
            p->setClipPath(clip);
            p->setOpacity(m_showDesc ? 0.41 : 1.0);
            // Calcula rect de crop centralizado (cover)
            const qreal iw = m_pixmap.width(), ih = m_pixmap.height();
            const qreal scale = qMax(w / iw, h / ih);
            const qreal sw = iw * scale, sh = ih * scale;
            p->drawPixmap(QRectF((w-sw)/2, (h-sh)/2, sw, sh), m_pixmap,
                          QRectF(0, 0, iw, ih));
            p->setOpacity(1.0);
            p->restore();
        } else {
            // Placeholder
            p->setPen(QColor(255, 255, 255, 60));
            p->setFont(QFont(QStringLiteral("Segoe UI"), 10));
            p->drawText(QRectF(0, 0, w, h), Qt::AlignCenter,
                        tr("Clique direito → Escolher imagem"));
        }

        // Header overlay (gradiente escuro no topo, até right-28 para o ×)
        QLinearGradient hg(0, 0, 0, 32);
        hg.setColorAt(0, QColor(0, 0, 0, 107));
        hg.setColorAt(1, QColor(0, 0, 0, 0));
        p->setPen(Qt::NoPen);
        p->setBrush(hg);
        p->drawRect(QRectF(0, 0, w, 32));

        // × no canto superior direito (badge semi-transparente)
        const QColor xBadge = m_hoverDelete ? QColor(0,0,0,180) : QColor(0,0,0,115);
        p->setBrush(xBadge);
        p->setPen(Qt::NoPen);
        p->drawRoundedRect(QRectF(w-24, 4, 20, 20), 4, 4);
        const QColor xc(255, 255, 255, m_hoverDelete ? 220 : 127);
        p->setPen(QPen(xc, 1.2, Qt::SolidLine, Qt::RoundCap));
        const qreal xxc = w-14, xyc = 14;
        p->drawLine(QPointF(xxc-4, xyc-4), QPointF(xxc+4, xyc+4));
        p->drawLine(QPointF(xxc+4, xyc-4), QPointF(xxc-4, xyc+4));

        // Resize handle
        const QColor rhCol(255, 255, 255, int((m_hoverResize ? 0.7 : 0.35) * 255));
        p->setPen(QPen(rhCol, 1.5, Qt::SolidLine, Qt::RoundCap));
        const qreal ox = w-3-10, oy = h-3-10;
        p->drawLine(QPointF(ox+2,oy+9), QPointF(ox+9,oy+2));
        p->drawLine(QPointF(ox+5,oy+9), QPointF(ox+9,oy+5));
        p->drawLine(QPointF(ox+8,oy+9), QPointF(ox+9,oy+8));

        // Pin de conexão
        const qreal px = w/2.0, py = 0.0, pr = 5.0;
        QRadialGradient pinGrad(px - pr*0.3, py - pr*0.3, pr*2.2);
        pinGrad.setColorAt(0.0, QColor(200, 200, 200, 180));
        pinGrad.setColorAt(1.0, QColor(80, 80, 80, 180));
        p->setPen(QPen(QColor(40, 40, 40, 140), 2));
        p->setBrush(pinGrad);
        p->setOpacity(0.55);
        p->drawEllipse(QPointF(px, py), pr, pr);
        p->setOpacity(1.0);

        // Scrollbar da descrição (quando aberta e com overflow)
        if (m_showDesc) {
            qreal visH = 0.0, contentH = 0.0;
            if (scrollRegion(visH, contentH))
                paintScrollbar(p, 34.0, visH, contentH, QColor(255, 255, 255, 150));
        }
        if (m_selected) paintSelectionRing(p);
        return;
    }

    // ── Doc card ─────────────────────────────────────────────────────────────
    if (m_data.type == QStringLiteral("doc")) {
        const QColor accentColor = m_data.color; // #60a5fa
        // Sombra
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 40));
        p->drawRoundedRect(QRectF(3, 5, w, h), 10, 10);
        // Fundo
        p->setBrush(QColor(QStringLiteral("#1c1c24")));
        p->setPen(QPen(QColor(255,255,255,25), 1));
        p->drawRoundedRect(QRectF(0, 0, w, h), 10, 10);
        // Borda top colorida (3px)
        p->setPen(QPen(accentColor, 3));
        p->setBrush(Qt::NoBrush);
        p->drawLine(QPointF(kRadius, 1.5), QPointF(w - kRadius, 1.5));

        // Header bar
        constexpr qreal kDocH = 26.0;
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0,0,0,38));
        p->drawRect(QRectF(0, 0, w, kDocH));

        // Título em azul
        p->setPen(accentColor);
        p->setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
        p->drawText(QRectF(8, 0, w - 28, kDocH), Qt::AlignVCenter | Qt::AlignLeft,
                    m_data.title.isEmpty() ? tr("Documento") : m_data.title);

        // × button
        const QColor xc(255,255,255, m_hoverDelete ? 220 : 76);
        p->setPen(QPen(xc, 1.2, Qt::SolidLine, Qt::RoundCap));
        const qreal xx = w-13, xy = kDocH/2;
        p->drawLine(QPointF(xx-4,xy-4), QPointF(xx+4,xy+4));
        p->drawLine(QPointF(xx+4,xy-4), QPointF(xx-4,xy+4));

        // Drawer name badge
        p->setPen(QColor(255,255,255,64));
        p->setFont(QFont(QStringLiteral("Segoe UI"), 8, -1, true));
        p->drawText(QRectF(8, kDocH, w-16, 18), Qt::AlignVCenter | Qt::AlignLeft,
                    m_data.linkedDrawerKey.isEmpty() ? QString() : m_data.linkedDrawerKey);

        // HTML content via m_richDoc (scroll por translate)
        if (m_richDoc && !m_data.content.isEmpty()) {
            qreal cx, cy, cw, ch;
            richContentRect(cx, cy, cw, ch);
            p->save();
            p->setClipRect(QRectF(0, cy, w, ch));
            p->translate(cx, cy - m_scrollOffset);
            m_richDoc->drawContents(p);
            p->restore();
            paintScrollbar(p, cy, ch, m_richDoc->size().height(), QColor(255,255,255,120));
        } else if (m_data.content.isEmpty()) {
            p->setPen(QColor(255,255,255,64));
            p->setFont(QFont(QStringLiteral("Segoe UI"), 10));
            p->drawText(QRectF(0, kDocH, w, h-kDocH), Qt::AlignCenter, tr("documento vazio"));
        }
        // Resize handle
        const QColor rhD(255,255,255, int((m_hoverResize ? 0.7 : 0.25)*255));
        p->setPen(QPen(rhD, 1.5, Qt::SolidLine, Qt::RoundCap));
        const qreal ox = w-13, oy = h-13;
        p->drawLine(QPointF(ox-7,oy), QPointF(ox,oy-7));
        p->drawLine(QPointF(ox-4,oy), QPointF(ox,oy-4));
        p->drawLine(QPointF(ox-1,oy), QPointF(ox,oy-1));
        // Pin
        const qreal px = w/2.0, py = 0.0, pr = 5.0;
        QRadialGradient pg(px-pr*0.3, py-pr*0.3, pr*2.2);
        pg.setColorAt(0, accentColor.lighter(130));
        pg.setColorAt(1, accentColor.darker(160));
        p->setPen(QPen(accentColor.darker(140), 2));
        p->setBrush(pg); p->setOpacity(0.55);
        p->drawEllipse(QPointF(px,py), pr, pr); p->setOpacity(1.0);
        if (m_selected) paintSelectionRing(p);
        return;
    }

    // ── Character card ───────────────────────────────────────────────────────
    if (m_data.type == QStringLiteral("character")) {
        const QColor accent = m_data.color; // #f97316 orange
        // Sombra
        p->setPen(Qt::NoPen); p->setBrush(QColor(0,0,0,40));
        p->drawRoundedRect(QRectF(3,5,w,h), 10, 10);
        // Fundo escuro
        p->setPen(QPen(QColor(255,255,255,25), 1));
        p->setBrush(QColor(QStringLiteral("#111111")));
        p->drawRoundedRect(QRectF(0,0,w,h), 10, 10);
        // Borda top
        p->setPen(QPen(accent, 3)); p->setBrush(Qt::NoBrush);
        p->drawLine(QPointF(kRadius,1.5), QPointF(w-kRadius,1.5));

        // Foto ou iniciais (dimmed quando overlay de doc aberto)
        p->save();
        QPainterPath clip; clip.addRoundedRect(QRectF(0,0,w,h), 10, 10);
        p->setClipPath(clip);
        p->setOpacity(m_showDesc ? 0.41 : 1.0);
        if (!m_pixmap.isNull()) {
            const qreal iw=m_pixmap.width(), ih=m_pixmap.height();
            const qreal scale=qMax(w/iw, h/ih);
            const qreal sw=iw*scale, sh=ih*scale;
            p->drawPixmap(QRectF((w-sw)/2,(h-sh)/2,sw,sh), m_pixmap, QRectF(0,0,iw,ih));
        } else {
            // Iniciais com fundo da cor accent
            p->setBrush(accent); p->setPen(Qt::NoPen);
            p->drawRect(QRectF(0,0,w,h));
            const QString name = m_data.title;
            const QStringList parts = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            QString initials;
            for (int i = 0; i < qMin(int(parts.size()), 2); ++i)
                if (!parts[i].isEmpty()) initials += parts[i][0].toUpper();
            if (initials.isEmpty()) initials = QStringLiteral("?");
            p->setPen(calcIsDark(accent) ? QColor(255,255,255,220) : QColor(0,0,0,180));
            p->setFont(QFont(QStringLiteral("Segoe UI"), 28, QFont::Bold));
            p->drawText(QRectF(0,0,w,h), Qt::AlignCenter, initials);
        }
        p->restore();

        // Mira 1: background rgba(0,0,0,0.10) sobre a foto + texto do doc quando aberto
        if (m_showDesc) {
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(0, 0, 0, 25)); // ~0.10 opacity sobre todo o card
            p->drawRect(QRectF(0, 0, w, h));

            if (m_richDoc) {
                qreal cx, cy, cw, ch;
                richContentRect(cx, cy, cw, ch);
                p->save();
                p->setClipRect(QRectF(0, cy, w, ch));
                p->translate(cx, cy - m_scrollOffset);
                m_richDoc->drawContents(p);
                p->restore();
                paintScrollbar(p, cy, ch, m_richDoc->size().height(), QColor(255,255,255,150));
            }
        }

        // Overlay de nome no topo (se tem título)
        if (!m_data.title.isEmpty()) {
            QLinearGradient hg(0,0,0,32);
            hg.setColorAt(0, QColor(0,0,0,115)); hg.setColorAt(1, QColor(0,0,0,0));
            p->setPen(Qt::NoPen); p->setBrush(hg);
            p->drawRect(QRectF(0,0,w,32));
            p->setPen(QColor(255,255,255,220));
            p->setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
            p->drawText(QRectF(8,0,w-28,28), Qt::AlignVCenter|Qt::AlignLeft, m_data.title);
        }

        // × no canto
        const QColor xB = m_hoverDelete ? QColor(0,0,0,180) : QColor(0,0,0,115);
        p->setPen(Qt::NoPen); p->setBrush(xB);
        p->drawRoundedRect(QRectF(w-24,4,20,20),4,4);
        const QColor xc(255,255,255, m_hoverDelete ? 220 : 127);
        p->setPen(QPen(xc,1.2,Qt::SolidLine,Qt::RoundCap));
        p->drawLine(QPointF(w-14-4,10),QPointF(w-14+4,18));
        p->drawLine(QPointF(w-14+4,10),QPointF(w-14-4,18));
        // Resize + pin
        const QColor rhC(255,255,255, int((m_hoverResize?0.7:0.35)*255));
        p->setPen(QPen(rhC,1.5,Qt::SolidLine,Qt::RoundCap));
        const qreal ox=w-13, oy=h-13;
        p->drawLine(QPointF(ox-7,oy),QPointF(ox,oy-7));
        p->drawLine(QPointF(ox-4,oy),QPointF(ox,oy-4));
        p->drawLine(QPointF(ox-1,oy),QPointF(ox,oy-1));
        const qreal px=w/2.0, py=0.0, pr=5.0;
        QRadialGradient pg(px-pr*0.3,py-pr*0.3,pr*2.2);
        pg.setColorAt(0, accent.lighter(130)); pg.setColorAt(1, accent.darker(160));
        p->setPen(QPen(accent.darker(140),2)); p->setBrush(pg);
        p->setOpacity(0.55); p->drawEllipse(QPointF(px,py),pr,pr); p->setOpacity(1.0);
        if (m_selected) paintSelectionRing(p);
        return;
    }

    // Glow de snap (waypoint snapping) — anel colorido ao redor do card
    if (m_snapping && m_snapColor.isValid()) {
        p->setPen(Qt::NoPen);
        p->setBrush(Qt::NoBrush);
        // Anel externo brilhante
        QPen snapPen(m_snapColor, 3.0);
        p->setPen(snapPen);
        p->setOpacity(0.85);
        p->drawRoundedRect(QRectF(-3, -3, w+6, h+6), kRadius+2, kRadius+2);
        // Glow difuso
        QPen glowPen(m_snapColor, 12.0);
        p->setPen(glowPen);
        p->setOpacity(0.22);
        p->drawRoundedRect(QRectF(-6, -6, w+12, h+12), kRadius+4, kRadius+4);
        p->setOpacity(1.0);
    }

    // Sombra (note/comment)
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 40));
    p->drawRoundedRect(QRectF(3, 5, w, h), kRadius, kRadius);

    // Fundo
    p->setBrush(bg);
    p->drawRoundedRect(QRectF(0, 0, w, h), kRadius, kRadius);

    // Comment: área de corpo com cor escurecida + rabinho abaixo
    if (m_data.type == QStringLiteral("comment")) {
        p->save();
        p->setClipPath(shape()); // clips ao contorno arredondado do card
        p->fillRect(QRectF(0, kHeaderH, w, h - kHeaderH), m_data.color.darker(140));
        p->restore();

        // Rabinho (CSS border trick → triângulo apontando para baixo)
        const qreal tx = 22.0, tw = kTailH * 1.2, th = kTailH;
        QPolygonF tail;
        tail << QPointF(tx, h) << QPointF(tx + tw, h) << QPointF(tx + tw / 2.0, h + th);
        p->setPen(Qt::NoPen);
        p->setBrush(m_data.color);
        p->drawPolygon(tail);
    }

    // Separador do header
    const QColor sep = isDark() ? QColor(255,255,255,30) : QColor(0,0,0,18);
    p->setPen(QPen(sep, 1));
    p->drawLine(QPointF(1, kHeaderH), QPointF(w - 1, kHeaderH));

    // Dobra de canto
    const qreal f = kFoldSize;
    p->setPen(Qt::NoPen);
    p->setBrush(isDark() ? QColor(0,0,0,70) : QColor(0,0,0,36));
    p->drawPolygon(QPolygonF() << QPointF(w-f,h) << QPointF(w,h) << QPointF(w,h-f));
    p->setBrush(isDark() ? QColor(255,255,255,18) : QColor(255,255,255,60));
    p->drawPolygon(QPolygonF() << QPointF(w-f,h) << QPointF(w,h-f) << QPointF(w-f,h-f));
    p->setPen(QPen(isDark() ? QColor(255,255,255,30) : QColor(0,0,0,30), 0.8));
    p->drawLine(QPointF(w-f, h), QPointF(w, h-f));

    // ── Header ──
    const QColor tc    = contrastColor();
    const QColor muted = QColor(tc.red(), tc.green(), tc.blue(), 90);

    // Grip (6 pontos)
    p->setPen(Qt::NoPen);
    p->setBrush(muted);
    const qreal gx = 8.0, gy = (kHeaderH - 9.0) / 2.0;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
            p->drawEllipse(QPointF(gx + col*4.5, gy + row*4.5), 1.2, 1.2);

    // Título do card no header (entre o grip e os botões da direita).
    // Duplo-clique no header abre o editor inline; enquanto edita, não pinta o texto.
    if (!m_editingTitle) {
        const qreal tx = 20.0;
        const qreal tw = qMax(10.0, w - 73.0 - tx);
        const QRectF titleRect(tx, 0, tw, kHeaderH);
        QFontMetricsF fm(QFont(QStringLiteral("Segoe UI"), 9, QFont::Bold));
        if (m_data.title.trimmed().isEmpty()) {
            QFont itf(QStringLiteral("Segoe UI"), 9);
            itf.setItalic(true);
            p->setFont(itf);
            p->setPen(QColor(tc.red(), tc.green(), tc.blue(), 90));
            p->drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft, tr("Sem título"));
        } else {
            p->setFont(QFont(QStringLiteral("Segoe UI"), 9, QFont::Bold));
            p->setPen(tc);
            p->drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft,
                        fm.elidedText(m_data.title, Qt::ElideRight, tw));
        }
    }

    // Timeline-event button (ponto sobre uma linha do tempo) — só note/comment
    {
        const qreal ebx = w - 60, eby = kHeaderH / 2.0;
        const QColor ecol = m_hoverEvent ? tc : muted;
        p->setPen(QPen(ecol, 1.2, Qt::SolidLine, Qt::RoundCap));
        p->setBrush(Qt::NoBrush);
        p->drawLine(QPointF(ebx - 5, eby + 2.5), QPointF(ebx + 5, eby + 2.5));
        p->setPen(Qt::NoPen);
        p->setBrush(ecol);
        p->drawEllipse(QPointF(ebx, eby - 1.5), 2.6, 2.6);
    }

    // Doc+ button (SVG: rect + cruz, igual Mira 1)
    {
        const qreal dbx = w - 43, dby = kHeaderH / 2.0;
        const QColor dcol = m_hoverDoc ? tc : muted;
        p->setPen(QPen(dcol, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(QRectF(dbx - 4.5, dby - 5.5, 9, 11), 1, 1);
        p->drawLine(QPointF(dbx - 1.5, dby - 1), QPointF(dbx - 1.5, dby + 3));
        p->drawLine(QPointF(dbx - 3.5, dby + 1), QPointF(dbx + 0.5, dby + 1));
    }

    // Color dot (círculo com a cor do card, igual Mira 1)
    {
        const qreal cdx = w - 28, cdy = kHeaderH / 2.0;
        p->setPen(QPen(QColor(0, 0, 0, 76), 1.5));
        p->setBrush(m_data.color);
        p->drawEllipse(QPointF(cdx, cdy), 5.5, 5.5);
        if (m_hoverColor) {  // anel de hover
            p->setPen(QPen(tc, 1.5));
            p->setBrush(Qt::NoBrush);
            p->drawEllipse(QPointF(cdx, cdy), 7, 7);
        }
    }

    // Botão ×
    const QColor xCol = m_hoverDelete ? QColor(220, 60, 60) : muted;
    p->setPen(QPen(xCol, 1.4, Qt::SolidLine, Qt::RoundCap));
    const qreal xx = w - 12.0, xy = kHeaderH / 2.0, xr = 4.0;
    p->drawLine(QPointF(xx-xr, xy-xr), QPointF(xx+xr, xy+xr));
    p->drawLine(QPointF(xx+xr, xy-xr), QPointF(xx-xr, xy+xr));

    // ── Pin de conexão no topo central (igual Mira 1 getCardCenter) ──
    {
        const qreal px = w / 2.0, py = 0.0, pr = 5.0;
        QRadialGradient pinGrad(px - pr * 0.3, py - pr * 0.3, pr * 2.2);
        pinGrad.setColorAt(0.0, m_data.color.lighter(130));
        pinGrad.setColorAt(1.0, m_data.color.darker(150));
        p->setPen(QPen(m_data.color.darker(145), 2));
        p->setBrush(pinGrad);
        p->setOpacity(0.55);
        p->drawEllipse(QPointF(px, py), pr, pr);
        // Anel externo de luz (boxShadow simulado)
        p->setPen(QPen(QColor(255, 255, 255, 20), 1.5));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(QPointF(px, py), pr + 1.5, pr + 1.5);
        p->setOpacity(1.0);
    }

    // Placeholder quando vazio
    if (m_textItem && m_textItem->document()->toPlainText().isEmpty()) {
        constexpr qreal padL = 10.0, padTop = 4.0;
        const QColor ph(tc.red(), tc.green(), tc.blue(), 80);
        p->setPen(ph);
        p->setFont(QFont(QStringLiteral("Segoe UI"), 12));
        const QRectF phRect(padL, kHeaderH + padTop,
                            w - 2*padL, h - kHeaderH - padTop - kFoldSize);
        p->drawText(phRect, Qt::AlignLeft | Qt::AlignTop, tr("Escreva aqui..."));
    }

    // Scrollbar fina quando o texto excede a área visível (note/comment)
    {
        qreal visH = 0.0, contentH = 0.0;
        if (scrollRegion(visH, contentH))
            paintScrollbar(p, kHeaderH + 4.0, visH, contentH,
                           QColor(tc.red(), tc.green(), tc.blue(), 150));
    }

    // Resize handle (3 linhas diagonais — igual Mira 1)
    const qreal opacity = isDark() ? 0.35 : 0.25;
    const QColor rhCol(tc.red(), tc.green(), tc.blue(),
                       int((m_hoverResize ? 0.7 : opacity) * 255));
    p->setPen(QPen(rhCol, 1.5, Qt::SolidLine, Qt::RoundCap));
    const qreal ox = w-3-10, oy = h-3-10;
    p->drawLine(QPointF(ox+2,oy+9), QPointF(ox+9,oy+2));
    p->drawLine(QPointF(ox+5,oy+9), QPointF(ox+9,oy+5));
    p->drawLine(QPointF(ox+8,oy+9), QPointF(ox+9,oy+8));

    if (m_selected) paintSelectionRing(p);
}

// ── Hit-test ────────────────────────────────────────────────────────────────

void CardItem::setSnapConnected(const QColor& color, const QString& connId)
{
    m_data.color        = color;
    m_data.linkedToConn = connId;
    applyTextColor();  // recomputa contraste do texto com a nova cor
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
    return QGraphicsObject::itemChange(change, value);
}

bool CardItem::isOnPin(const QPointF& p) const
{
    // Pin está em (w/2, 0) — hit zone raio 10
    return QLineF(p, QPointF(m_data.width / 2.0, 0.0)).length() <= 10.0;
}

// Layout do header (da direita): [×@w-12] [colorDot@w-27] [doc+@w-42] [...] [grip]
bool CardItem::isOnDeleteBtn(const QPointF& p) const
{
    if (m_data.type == QStringLiteral("image"))
        return QRectF(m_data.width - 24, 4, 20, 20).contains(p);
    return QRectF(m_data.width - 20, 0, 20, kHeaderH).contains(p);
}
bool CardItem::isOnColorDot(const QPointF& p) const
{
    return QRectF(m_data.width - 36, 0, 16, kHeaderH).contains(p);
}
bool CardItem::isOnDocBtn(const QPointF& p) const
{
    return QRectF(m_data.width - 52, 0, 16, kHeaderH).contains(p);
}
bool CardItem::isOnEventBtn(const QPointF& p) const
{
    // Botão "criar evento na Timeline": só nos cards de texto (note/comment),
    // à esquerda do botão de doc.
    if (m_data.type != QStringLiteral("note") && m_data.type != QStringLiteral("comment"))
        return false;
    return QRectF(m_data.width - 68, 0, 16, kHeaderH).contains(p);
}
bool CardItem::isOnResizeZone(const QPointF& p) const
{
    return QRectF(m_data.width - 17, m_data.height - 17, 17, 17).contains(p);
}

void CardItem::showColorMenu(const QPoint& screenPos)
{
    const bool isComment = (m_data.type == QStringLiteral("comment"));
    const QColor* palette = isComment ? kCommentPalette : kNotePalette;
    QMenu menu;
    for (int i = 0; i < 8; ++i) {
        const QColor& c = palette[i];
        QPixmap px(16, 16); px.fill(c);
        auto* act = menu.addAction(QIcon(px), QString());
        act->setData(c.name());
    }
    menu.addSeparator();
    auto* custom = menu.addAction(tr("Personalizada..."));

    QAction* chosen = menu.exec(screenPos);
    if (!chosen) return;
    QColor nc = (chosen == custom)
        ? QColorDialog::getColor(m_data.color, nullptr, tr("Cor do card"))
        : QColor(chosen->data().toString());
    if (!nc.isValid()) return;
    emit gestureStarted();
    prepareGeometryChange();
    m_data.color = nc;
    applyTextColor();
    update();
    emit dataChanged(m_data);
}

// ── Mouse events ────────────────────────────────────────────────────────────

void CardItem::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { e->ignore(); return; }

    // Seleciona este card (e desseleciona os outros via LousaScene).
    const bool wasSelected = m_selected;
    emit cardPressed();

    // Shift+click em cards normais = só alterna a seleção (sem arrastar/agir nos botões).
    // Exceção: Shift+× precisa chegar ao handler de delete (remoção permanente).
    // text/symbol mantêm o Shift+drag para rotação.
    if ((e->modifiers() & Qt::ShiftModifier) && !isTextSymbol()
        && !isOnDeleteBtn(e->pos())) {
        e->accept();
        return;
    }

    // ── text / symbol ──
    if (isTextSymbol()) {
        // Controles só respondem se o card já estava selecionado (visíveis).
        // Alça de rotação (topo-centro) → rotação por ângulo absoluto
        if (wasSelected && tsRotateRect().contains(e->pos())) {
            emit gestureStarted();
            m_rotating    = true;
            m_rotByHandle = true;
            e->accept(); return;
        }
        // Shift+drag → rotacionar (atalho do Mira 1)
        if (e->modifiers() & Qt::ShiftModifier) {
            emit gestureStarted();
            m_rotating      = true;
            m_rotByHandle   = false;
            m_rotStartDeg   = m_data.rotation;
            m_rotStartScene = e->scenePos();
            e->accept(); return;
        }
        if (wasSelected) {
            QRectF del, color, sym;
            controlRects(del, color, sym);
            if (del.contains(e->pos())) {
                if (e->modifiers() & Qt::ShiftModifier) emit deleteRequested(m_data.id);
                else                                    emit stashRequested(m_data.id);
                e->accept(); return;
            }
            if (color.contains(e->pos())) {
                const QColor nc = QColorDialog::getColor(
                    m_data.color, nullptr, tr("Cor"));
                if (nc.isValid()) {
                    m_data.color = nc;
                    applyContentFont();
                    update();
                    emit dataChanged(m_data);
                }
                e->accept(); return;
            }
            if (m_data.type == QStringLiteral("symbol") && sym.contains(e->pos())) {
                openSymbolPicker(); e->accept(); return;
            }
        }
        if (wasSelected && tsResizeRect().contains(e->pos())) {
            emit gestureStarted();
            m_fontResizing  = true;
            m_pressScene    = e->scenePos();
            m_pressFontSize = effFontSize();
            e->accept(); return;
        }
        // drag
        emit gestureStarted();
        emit dragStarted(m_data.id);
        m_dragging        = true;
        m_pressScene      = e->scenePos();
        m_pressItemOrigin = pos();
        setCursor(Qt::ClosedHandCursor);
        e->accept(); return;
    }

    // Pin drag — tem prioridade sobre tudo (funciona em qualquer tipo de card)
    if (isOnPin(e->pos())) {
        m_draggingPin = true;
        const QPointF pinScene = mapToScene(QPointF(m_data.width / 2.0, 0.0));
        emit pinDragStarted(m_data.id, pinScene);
        e->accept();
        return;
    }

    // Doc / character: drag de qualquer ponto, sem área de texto editável
    const bool isDarkCard = (m_data.type == QStringLiteral("doc") ||
                             m_data.type == QStringLiteral("character"));
    if (isDarkCard) {
        if (isOnDeleteBtn(e->pos())) {
            if (e->modifiers() & Qt::ShiftModifier) emit deleteRequested(m_data.id);
            else                                    emit stashRequested(m_data.id);
            e->accept(); return;
        }
        if (isOnResizeZone(e->pos())) {
            emit gestureStarted();
            m_resizing = true; m_pressScene = e->scenePos();
            m_pressSize = QSizeF(m_data.width, m_data.height);
            e->accept(); return;
        }
        emit gestureStarted();
        emit dragStarted(m_data.id);
        m_dragging = true; m_pressScene = e->scenePos(); m_pressItemOrigin = pos();
        setCursor(Qt::ClosedHandCursor); e->accept(); return;
    }

    // Imagem: interação própria
    if (m_data.type == QStringLiteral("image")) {
        if (isOnDeleteBtn(e->pos())) {
            if (e->modifiers() & Qt::ShiftModifier) emit deleteRequested(m_data.id);
            else                                    emit stashRequested(m_data.id);
            e->accept(); return;
        }
        if (isOnResizeZone(e->pos())) {
            emit gestureStarted();
            m_resizing = true; m_pressScene = e->scenePos();
            m_pressSize = QSizeF(m_data.width, m_data.height);
            e->accept(); return;
        }
        if (!m_showDesc) { // drag de qualquer ponto quando descrição não está aberta
            emit gestureStarted();
            emit dragStarted(m_data.id);
            m_dragging = true; m_pressScene = e->scenePos(); m_pressItemOrigin = pos();
            setCursor(Qt::ClosedHandCursor); e->accept(); return;
        }
        e->ignore(); return;
    }

    if (isOnDeleteBtn(e->pos())) {
        if (e->modifiers() & Qt::ShiftModifier) emit deleteRequested(m_data.id);
        else                                    emit stashRequested(m_data.id);
        e->accept();
        return;
    }
    if (isOnColorDot(e->pos())) {
        // Converte posição local → tela para posicionar o menu
        QPoint screenPos = e->screenPos();
        showColorMenu(screenPos);
        e->accept();
        return;
    }
    if (isOnDocBtn(e->pos())) {
        emit createDocRequested(m_data.id);
        e->accept();
        return;
    }
    if (isOnEventBtn(e->pos())) {
        emit createTimelineEventRequested(m_data.id);
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
    // Drag pelo header
    if (e->pos().y() < kHeaderH) {
        emit gestureStarted();
        emit dragStarted(m_data.id);
        m_dragging        = true;
        m_pressScene      = e->scenePos();
        m_pressItemOrigin = pos();
        setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    e->ignore();
}

void CardItem::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
    if (m_rotating) {
        if (m_rotByHandle) {
            // ângulo do pivô (centro) até o cursor; a alça aponta pra cima a 0°
            const QPointF pivot = mapToScene(transformOriginPoint());
            const QPointF v = e->scenePos() - pivot;
            const qreal deg = std::atan2(v.y(), v.x()) * 180.0 / 3.14159265358979 + 90.0;
            m_data.rotation = std::fmod(deg + 360.0, 360.0);
        } else {
            const qreal dx = e->scenePos().x() - m_rotStartScene.x();
            m_data.rotation = std::fmod(m_rotStartDeg + dx * 0.5, 360.0);
        }
        setRotation(m_data.rotation);
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
        }
        e->accept();
        return;
    }
    if (m_draggingPin) {
        // O LousaScene recebe o evento via pinDragStarted e controla a ghost line.
        // O CardItem apenas propaga a posição atual do cursor em cena.
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
        m_data.width  = qMax(kMinW, m_pressSize.width()  + d.x());
        m_data.height = qMax(kMinH, m_pressSize.height() + d.y());
        updateTextItem();
        update();
        // O centro/pino do card mudou de lugar embora pos() não tenha mudado —
        // avisa a cena para as conexões acompanharem em tempo real.
        emit positionChanged(m_data.id);
        e->accept();
        return;
    }
    e->ignore();
}

void CardItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
    if (m_rotating || m_fontResizing) {
        m_rotating = m_fontResizing = m_rotByHandle = false;
        setCursor(Qt::ArrowCursor);
        emit dataChanged(m_data);
        e->accept();
        return;
    }
    if (m_draggingPin) {
        m_draggingPin = false;
        // LousaScene finalizará a conexão via o evento da view — não fazemos nada aqui.
        e->accept();
        return;
    }
    if (m_dragging || m_resizing) {
        m_dragging = m_resizing = false;
        setCursor(Qt::ArrowCursor);
        const QPointF p = pos();
        m_data.x = p.x(); m_data.y = p.y();
        emit dataChanged(m_data);
        e->accept();
        return;
    }
    e->ignore();
}

// ── Hover ────────────────────────────────────────────────────────────────────

static QString stripHtml(const QString& html)
{
    if (!html.contains(QLatin1Char('<'))) return html;
    QTextDocument doc;
    doc.setHtml(html);
    return doc.toPlainText();
}

QString CardItem::cardTooltipText() const
{
    // Símbolos são auto-explicativos visualmente.
    if (m_data.type == QStringLiteral("symbol")) return {};

    QStringList parts;
    const QString title = m_data.title.trimmed();
    if (!title.isEmpty()) parts << title;

    if (m_data.type == QStringLiteral("note")    ||
        m_data.type == QStringLiteral("comment") ||
        m_data.type == QStringLiteral("text"))
    {
        QString body = stripHtml(m_data.content).trimmed();
        // Remove linhas extras e trunca pra caber bem no tooltip.
        body.replace(QStringLiteral(" "), QStringLiteral("\n")); // separador de parágrafo Qt
        body = body.simplified();   // condensa espaços/newlines
        if (body.length() > 220)
            body = body.left(217) + QStringLiteral("…");
        if (!body.isEmpty() && body != title) parts << body;
    } else if (m_data.type == QStringLiteral("image") && !m_data.description.isEmpty()) {
        const QString desc = m_data.description.trimmed();
        if (!desc.isEmpty() && desc != title) parts << desc;
    }

    return parts.join(QStringLiteral("\n"));
}

void CardItem::hoverEnterEvent(QGraphicsSceneHoverEvent* e)
{
    if (!cardTooltipText().isEmpty())
        emit hoverPreviewRequested(m_data, e->screenPos());
    QGraphicsObject::hoverEnterEvent(e);
}

void CardItem::hoverMoveEvent(QGraphicsSceneHoverEvent* e)
{
    if (isTextSymbol()) {
        const QPointF lp = e->pos();
        if (m_selected) {
            QRectF del, color, sym;
            controlRects(del, color, sym);
            if (tsRotateRect().contains(lp))                       setCursor(Qt::CrossCursor);
            else if (tsResizeRect().contains(lp))                  setCursor(Qt::SizeFDiagCursor);
            else if (del.contains(lp) || color.contains(lp) ||
                     (m_data.type == QStringLiteral("symbol") && sym.contains(lp)))
                                                                   setCursor(Qt::PointingHandCursor);
            else                                                   setCursor(Qt::OpenHandCursor);
        } else {
            setCursor(Qt::OpenHandCursor);
        }
        return;
    }
    const QPointF lp = e->pos();
    const bool onDel    = isOnDeleteBtn(lp);
    const bool onColor  = isOnColorDot(lp);
    const bool onDoc    = isOnDocBtn(lp);
    const bool onEvent  = isOnEventBtn(lp);
    const bool onResize = isOnResizeZone(lp);
    const bool onHeader = lp.y() < kHeaderH;

    if (onDel != m_hoverDelete || onColor != m_hoverColor ||
        onDoc != m_hoverDoc || onEvent != m_hoverEvent || onResize != m_hoverResize) {
        m_hoverDelete = onDel;
        m_hoverColor  = onColor;
        m_hoverDoc    = onDoc;
        m_hoverEvent  = onEvent;
        m_hoverResize = onResize;
        update();
    }
    if (isOnPin(lp))               setCursor(Qt::CrossCursor);
    else if (onDel || onColor || onDoc || onEvent) setCursor(Qt::PointingHandCursor);
    else if (onResize)             setCursor(Qt::SizeFDiagCursor);
    else if (onHeader)             setCursor(Qt::OpenHandCursor);
    else                           setCursor(Qt::ArrowCursor);
}

void CardItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* e)
{
    emit hoverPreviewDismissed();
    const bool needUpdate = m_hoverDelete || m_hoverColor || m_hoverDoc || m_hoverEvent || m_hoverResize || m_hovered;
    m_hoverDelete = m_hoverColor = m_hoverDoc = m_hoverEvent = m_hoverResize = false;
    m_hovered = false;
    if (needUpdate) update();
    setCursor(Qt::ArrowCursor);
    QGraphicsObject::hoverLeaveEvent(e);
}

// ── Context menu ─────────────────────────────────────────────────────────────

void CardItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)
{
    if (m_data.type == QStringLiteral("image") || m_data.type == QStringLiteral("character")) {
        toggleImageDesc(!m_showDesc);
        e->accept();
        return;
    }
    if (m_data.type == QStringLiteral("symbol")) {
        openSymbolPicker();
        e->accept();
        return;
    }
    if (m_data.type == QStringLiteral("text") && m_textItem) {
        QString txt = m_textItem->document()->toPlainText();
        QColor  col = m_data.color;
        QString fam = m_data.fontFamily;
        if (pickText(nullptr, txt, col, fam)) {
            if (col.isValid()) m_data.color = col;
            m_data.fontFamily = fam;
            QSignalBlocker bl(m_textItem->document());
            m_textItem->document()->setPlainText(txt);
            m_data.content = txt;
            applyContentFont();
            refreshContentMetrics();
            update();
            emit dataChanged(m_data);
        }
        e->accept();
        return;
    }
    // note / comment: duplo-clique no header abre o editor inline do título.
    if ((m_data.type == QStringLiteral("note") || m_data.type == QStringLiteral("comment"))
        && e->pos().y() < kHeaderH) {
        beginTitleEdit();
        e->accept();
        return;
    }
    QGraphicsObject::mouseDoubleClickEvent(e);
}

void CardItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* e)
{
    if (m_data.type == QStringLiteral("image")) {
        QMenu menu;
        menu.addAction(tr("Escolher imagem..."), this, &CardItem::openImagePicker);
        menu.addAction(tr("Criar documento"), this, [this]() {
            emit createDocRequested(m_data.id);
        });
        menu.addSeparator();
        menu.addAction(tr("Remover card"), this, [this]() {
            emit deleteRequested(m_data.id);
        });
        menu.exec(e->screenPos());
        return;
    }
    if (isTextSymbol()) {
        QMenu menu;
        if (m_data.type == QStringLiteral("symbol"))
            menu.addAction(tr("Trocar símbolo..."), this, [this]() { openSymbolPicker(); });
        menu.addAction(tr("Cor..."), this, [this]() {
            const QColor nc = QColorDialog::getColor(m_data.color, nullptr, tr("Cor"));
            if (nc.isValid()) { m_data.color = nc; applyContentFont(); update(); emit dataChanged(m_data); }
        });
        menu.addSeparator();
        menu.addAction(tr("Remover"), this, [this]() { emit deleteRequested(m_data.id); });
        menu.exec(e->screenPos());
        return;
    }
    showColorMenu(e->screenPos());
}
