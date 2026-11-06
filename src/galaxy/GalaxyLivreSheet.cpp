// Criação Livre: a folha. O texto usa o editor do Qenna (corretor, sinônimos rápidos, glossário e leitura em voz alta
// emprestados do editor principal), a barra na ordem que o Pe pediu (fonte, tamanho | B I U S | alinhamento, entrelinha
// | imagem | Modo Foco), "o que é em onde" embaixo do nome, palavras no rodapé, Ctrl + − pro travessão, colar limpo e
// menções com @ que viram link pro lugar.

#include "GalaxyLivre.h"
#include "GalaxyDoc.h"
#include "GalaxyCanvas.h"
#include "GalaxyPanel.h"
#include "GalaxyStructures.h"

#include "../FontPickerPopup.h"
#include "../IconUtils.h"
#include "../ProjectModel.h"
#include "../SpellEditor.h"
#include "../SpellHighlighter.h"
#include "../TerritorioStore.h"
#include "../Theme.h"
#include "../carto/CartoIcons.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace galaxy {

namespace {
inline QColor col(const QString& c) { return Theme::toColor(c); }
inline QString catT(const char* s) { return QCoreApplication::translate("galaxy::Catalog", s); }
QLabel* lab(const QString& text, const QString& obj, QWidget* parent, bool wrap = false)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(obj);
    l->setWordWrap(wrap);
    return l;
}
QPushButton* btn(const QString& text, const QString& obj, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(obj);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}
const QString MENTION = QStringLiteral("livre:");

// O editor da folha: o do Qenna, com o que a folha precisa a mais.
class SheetEdit : public SpellEditor {
public:
    explicit SheetEdit(QWidget* parent) : SpellEditor(parent) { viewport()->setMouseTracking(true); }
    std::function<bool(QKeyEvent*)> keyHook;                  // true = já tratei
    std::function<void(const QString&)> mentionClicked;
    std::function<void(const QString&, const QRect&)> mentionHover;   // vazio = saiu de cima
    std::function<void(const QImage&)> imagePasted;
protected:
    void keyPressEvent(QKeyEvent* e) override
    {
        if (keyHook && keyHook(e)) return;
        SpellEditor::keyPressEvent(e);
    }
    // colar limpo: só o texto, sem a fonte e a cor de onde veio; imagem entra como imagem
    void insertFromMimeData(const QMimeData* m) override
    {
        if (m->hasImage() && imagePasted) { const QImage im = qvariant_cast<QImage>(m->imageData()); if (!im.isNull()) { imagePasted(im); return; } }
        if (m->hasText()) { textCursor().insertText(m->text()); return; }
        SpellEditor::insertFromMimeData(m);
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        SpellEditor::mouseMoveEvent(e);
        const QString href = anchorAt(e->position().toPoint());
        const QString key = href.startsWith(MENTION) ? href.mid(MENTION.size()) : QString();
        viewport()->setCursor(key.isEmpty() ? Qt::IBeamCursor : Qt::PointingHandCursor);
        if (key != m_hover) {
            m_hover = key;
            if (mentionHover) mentionHover(key, QRect(e->position().toPoint() - QPoint(6, 12), QSize(12, 24)));
        }
    }
    void mousePressEvent(QMouseEvent* e) override { m_pressAt = e->position().toPoint(); SpellEditor::mousePressEvent(e); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        SpellEditor::mouseReleaseEvent(e);
        if (e->button() != Qt::LeftButton || (e->position().toPoint() - m_pressAt).manhattanLength() > 4) return;
        const QString href = anchorAt(e->position().toPoint());
        if (href.startsWith(MENTION) && mentionClicked) mentionClicked(href.mid(MENTION.size()));
    }
    void leaveEvent(QEvent* e) override { SpellEditor::leaveEvent(e); if (!m_hover.isEmpty()) { m_hover.clear(); if (mentionHover) mentionHover(QString(), QRect()); } }
private:
    QString m_hover;
    QPoint m_pressAt;
};
}

QString Livre::whatIs(const QString& k) const
{
    switch (kindOf(k)) {
    case LK::Place: { const Group* g = m_doc->resolve(k).group; return g && !g->form.isEmpty() ? g->form : kindName(LK::Place); }
    case LK::Build: { const StructDef* d = structOf(buildType(k)); return d ? catT(d->name) : kindName(LK::Build); }
    default: return kindName(kindOf(k));
    }
}
// Onde estou: o que é e onde fica ("Cidade orbital em Zolon"), do mais perto pro mais longe; cada lugar abre.
QWidget* Livre::whereLine(const QString& k, QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(5);
    h->addWidget(lab(whatIs(k), QStringLiteral("lvWlKind"), w));
    QStringList path = pathOf(k);
    std::reverse(path.begin(), path.end());
    const LK t = kindOf(k);
    if (!path.isEmpty()) {
        h->addWidget(lab(tr("em"), QStringLiteral("lvWlSep"), w));
        for (int i = 0; i < path.size(); i++) {
            auto* b = btn(label(path[i]) + (i < path.size() - 1 ? QStringLiteral(",") : QString()), QStringLiteral("lvWlLink"), w);
            const QString p = path[i];
            connect(b, &QPushButton::clicked, this, [this, p]() { select(p); });
            h->addWidget(b);
        }
    } else if (t != LK::Terr && t != LK::Link) h->addWidget(lab(t == LK::Moon || t == LK::Build ? tr("solta") : tr("solto"), QStringLiteral("lvWlSep"), w));
    h->addStretch();
    return w;
}
// Quantas folhas citam este lugar (as do Criador de Mundos, os documentos e as conexões).
int Livre::mentionsOf(const QString& k) const
{
    const QString needle = QStringLiteral("\"") + MENTION + k + QStringLiteral("\"");
    int n = 0;
    auto* places = m_panel->m_link.places;
    if (places) {
        for (auto it = m_doc->placeIds.constBegin(); it != m_doc->placeIds.constEnd(); ++it) {
            if (it.key() == k) continue;
            if (const auto* t = places->territorio(it.value())) if (t->content.contains(needle)) n++;
        }
        for (const auto& l : places->links()) if (l.docContent.contains(needle)) n++;
    }
    for (const auto& d : m_doc->docs) if (m_panel->docHtml(d.get()).contains(needle)) n++;
    return n;
}
// O mundo mudou de chave (mudou de sistema): as menções a ele acompanham, em todo texto que mora no projeto.
void Livre::rewriteMentions(const QString& from, const QString& to)
{
    if (from == to) return;
    const QString a = MENTION + from + QStringLiteral("\""), b = MENTION + to + QStringLiteral("\"");
    // luas: livre:M12.3.x → livre:M40.1.x
    const QString ma = from.startsWith(QLatin1Char('P')) ? MENTION + QStringLiteral("M") + from.mid(1) + QLatin1Char('.') : QString();
    const QString mb = to.startsWith(QLatin1Char('P')) ? MENTION + QStringLiteral("M") + to.mid(1) + QLatin1Char('.') : QString();
    const auto fix = [&](QString s) { s.replace(a, b); if (!ma.isEmpty()) s.replace(ma, mb); return s; };
    auto* places = m_panel->m_link.places;
    if (places) {
        for (auto it = m_doc->placeIds.constBegin(); it != m_doc->placeIds.constEnd(); ++it)
            if (const auto* t = places->territorio(it.value())) { const QString n = fix(t->content); if (n != t->content) { places->updateTerritorioContent(it.value(), n); m_panel->m_docPlacesDirty = true; } }
        for (const auto& l : places->links()) { const QString n = fix(l.docContent); if (n != l.docContent) { places->updateLinkContent(l.id, n); m_panel->m_docPlacesDirty = true; } }
    }
    for (const auto& d : m_doc->docs) { const QString h = m_panel->docHtml(d.get()); const QString n = fix(h); if (n != h) m_panel->docWriteHtml(d.get(), n); }
}

void Livre::syncCount()
{
    if (!m_body || !m_count) return;
    const QString tx = m_body->toPlainText().trimmed();
    const int n = tx.isEmpty() ? 0 : int(tx.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size());
    QString s = n == 1 ? tr("1 palavra") : tr("%1 palavras").arg(nf(n));
    if (n > 220) s += QStringLiteral(" · ") + tr("%1 min de leitura").arg(qRound(n / 220.0));
    m_count->setText(s);
}
// Modo Foco: só o parágrafo do cursor fica aceso
void Livre::applyFocus()
{
    if (!m_body) return;
    QList<QTextEdit::ExtraSelection> sels;
    if (m_focusMode) {
        QColor dim = col(Theme::editorTextColor());
        dim.setAlphaF(0.28);
        const QTextBlock b = m_body->textCursor().block();
        QTextDocument* doc = m_body->document();
        const int s0 = b.position(), s1 = b.position() + b.length() - 1, end = doc->characterCount() - 1;
        const auto add = [&](int from, int to) {
            if (to <= from) return;
            QTextEdit::ExtraSelection x;
            x.cursor = QTextCursor(doc);
            x.cursor.setPosition(from);
            x.cursor.setPosition(to, QTextCursor::KeepAnchor);
            x.format.setForeground(dim);
            sels << x;
        };
        add(0, s0);
        add(s1, end);
    }
    m_body->setExtraSelections(sels);
}
// alinhamento e entrelinha valem pra folha inteira, como a fonte
void Livre::applyAlignAndSpacing()
{
    if (!m_body) return;
    const bool was = m_sheetLoading;
    m_sheetLoading = true;
    QTextCursor c(m_body->document());
    c.beginEditBlock();
    c.select(QTextCursor::Document);
    QTextBlockFormat f;
    f.setAlignment(m_align == QLatin1String("center") ? Qt::AlignHCenter : m_align == QLatin1String("right") ? Qt::AlignRight : m_align == QLatin1String("justify") ? Qt::AlignJustify : Qt::AlignLeft);
    f.setLineHeight(qRound(m_lineH / 1.2), QTextBlockFormat::ProportionalHeight);   // o Qt conta sobre a linha da fonte (~1,2 em)
    c.mergeBlockFormat(f);
    c.endEditBlock();
    m_sheetLoading = was;
}
void Livre::applyEdFont()
{
    if (!m_body) return;
    QFont f(m_fontFamily);
    f.setPixelSize(m_size);
    m_body->document()->setDefaultFont(f);
    m_body->setFont(f);
}

// ── menções com @ ──
void Livre::closeMention()
{
    if (m_mentionPop) { m_mentionPop->hide(); m_mentionPop->deleteLater(); m_mentionPop = nullptr; }
    m_mention = Mention();
}
void Livre::openMention()
{
    if (!m_body) return;
    const QTextCursor c = m_body->textCursor();
    const QString before = c.block().text().left(c.positionInBlock());
    static const QRegularExpression re(QStringLiteral("(^|\\s)@([^\\s@]{0,24})$"));
    const auto m = re.match(before);
    if (!m.hasMatch()) { closeMention(); return; }
    const QString q = m.captured(2);
    // o que dá pra mencionar: tudo que tem nome, menos a própria folha
    QStringList all;
    for (const auto& r : m_doc->regions) all << r->key;
    for (System* s : livreSystems()) all << keyS(*s);
    all << allWorlds();
    for (const auto& g : m_doc->groups) if (valid(g->key)) all << g->key;
    for (const auto& b : m_doc->builds) if (valid(b->key)) all << b->key;
    for (const auto& d : m_doc->docs) if (valid(docKey(d.get()))) all << docKey(d.get());
    QStringList hits, rest;
    for (const QString& k : all) {
        if (k == m_sheetFor || isGhost(k)) continue;
        const QString n = label(k);
        if (q.isEmpty() || n.contains(q, Qt::CaseInsensitive)) (n.startsWith(q, Qt::CaseInsensitive) ? hits : rest) << k;
    }
    hits << rest;
    while (hits.size() > 8) hits.removeLast();
    const int idx = std::clamp(m_mention.idx, 0, std::max(0, int(hits.size()) - 1));
    if (m_mentionPop) { m_mentionPop->hide(); m_mentionPop->deleteLater(); m_mentionPop = nullptr; }
    m_mention.start = c.position() - int(q.size()) - 1;
    m_mention.end = c.position();
    m_mention.list = hits;
    m_mention.idx = idx;
    auto* pop = new QFrame(this);
    pop->setObjectName(QStringLiteral("lvPop"));
    pop->setFixedWidth(270);
    auto* l = new QVBoxLayout(pop);
    l->setContentsMargins(5, 5, 5, 5);
    l->setSpacing(1);
    if (hits.isEmpty()) l->addWidget(lab(tr("Nada com esse nome."), QStringLiteral("lvEmpty"), pop));
    for (int i = 0; i < hits.size(); i++) {
        const QString k = hits[i];
        auto* b = btn(QString(), QStringLiteral("lvOpt"), pop);
        b->setProperty("on", i == idx);
        auto* h = new QHBoxLayout(b);
        h->setContentsMargins(6, 3, 6, 3);
        h->setSpacing(8);
        QWidget* mk = markOf(k, isWorld(k) ? 16 : 13, b);
        h->addWidget(mk);
        auto* t = lab(label(k), QStringLiteral("lvRowName"), b);
        t->setAttribute(Qt::WA_TransparentForMouseEvents);
        h->addWidget(t, 1);
        auto* s = lab(whatIs(k), QStringLiteral("lvSmall"), b);
        s->setAttribute(Qt::WA_TransparentForMouseEvents);
        h->addWidget(s);
        b->setMinimumHeight(28);
        connect(b, &QPushButton::clicked, this, [this, k]() { insertMention(k); });
        l->addWidget(b);
    }
    pop->adjustSize();
    const QRect cr = m_body->cursorRect();
    QPoint at = m_body->viewport()->mapTo(this, cr.bottomLeft() + QPoint(0, 6));
    if (at.y() + pop->height() > height() - 8) at = m_body->viewport()->mapTo(this, cr.topLeft()) - QPoint(0, pop->height() + 6);
    at.setX(std::clamp(at.x(), 8, std::max(8, width() - pop->width() - 8)));
    pop->move(at);
    pop->show();
    pop->raise();
    m_mentionPop = pop;
}
void Livre::insertMention(const QString& key)
{
    if (!m_body || m_mention.start < 0) { closeMention(); return; }
    const int s = m_mention.start, e = m_mention.end;
    closeMention();
    QTextCursor c(m_body->document());
    c.setPosition(s);
    c.setPosition(e, QTextCursor::KeepAnchor);
    const QColor a = col(Theme::accentDefault());
    c.insertHtml(QStringLiteral("<a href=\"%1%2\" style=\"color:%3; text-decoration:none; font-weight:600;\">%4</a>").arg(MENTION, key, a.name(), label(key).toHtmlEscaped()));
    QTextCharFormat plain = c.charFormat();
    plain.setAnchor(false);
    plain.setAnchorHref(QString());
    plain.clearForeground();
    plain.setFontWeight(QFont::Normal);
    c.insertText(QString(QChar(0x00A0)), plain);
    m_body->setTextCursor(c);
    m_body->setCurrentCharFormat(plain);
    m_body->setFocus();
}

// ── a barra ──
QWidget* Livre::edBar(const QString& k, QWidget* parent)
{
    auto* bar = new QWidget(parent);
    bar->setObjectName(QStringLiteral("lvEdBar"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(28, 8, 22, 8);
    h->setSpacing(4);
    const QColor ink = col(Theme::editorTextColor()), a = col(Theme::accentDefault());
    const auto tool = [&](const QString& text, const QString& tip) {
        auto* b = new QToolButton(bar);
        b->setObjectName(QStringLiteral("lvEd"));
        b->setText(text);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        h->addWidget(b);
        return b;
    };
    const auto sep = [&]() { auto* s = new QFrame(bar); s->setFixedSize(1, 18); s->setStyleSheet(QStringLiteral("background: %1;").arg(QStringLiteral("rgba(%1,%2,%3,0.18)").arg(ink.red()).arg(ink.green()).arg(ink.blue()))); h->addWidget(s); };
    // fonte (as do Qenna, no mesmo seletor do editor principal) e tamanho
    auto* fb = new QToolButton(bar);
    fb->setObjectName(QStringLiteral("lvEdFont"));
    fb->setText(m_fontFamily + QStringLiteral("  ▾"));
    fb->setToolTip(tr("Fonte da folha"));
    fb->setCursor(Qt::PointingHandCursor);
    fb->setFocusPolicy(Qt::NoFocus);
    connect(fb, &QToolButton::clicked, this, [this, fb]() {
        QStringList fonts = m_panel->m_link.fonts ? m_panel->m_link.fonts() : QStringList();
        if (fonts.isEmpty()) fonts = { m_fontFamily };
        if (!m_fontPop) {
            m_fontPop = new FontPickerPopup(this);
            connect(m_fontPop, &FontPickerPopup::fontSelected, this, [this](const QString& fam) {
                m_fontFamily = fam;
                QSettings().setValue(QStringLiteral("galaxy/livreFontFamily"), fam);
                applyEdFont();
                m_edFor.clear();
                QTimer::singleShot(0, this, [this]() { flushSheet(); render(); });
            });
        }
        m_fontPop->setFontFamilies(fonts, m_fontFamily);
        m_fontPop->showNear(QRect(fb->mapToGlobal(QPoint(0, 0)), fb->size()), Qt::TopEdge);
    });
    h->addWidget(fb);
    auto* minus = tool(QStringLiteral("−"), tr("Diminuir a letra"));
    auto* sz = lab(QString::number(m_size), QStringLiteral("lvEdSize"), bar);
    sz->setAlignment(Qt::AlignCenter);
    h->addWidget(sz);
    auto* plus = tool(QStringLiteral("+"), tr("Aumentar a letra"));
    const auto step = [this, sz](int d) { m_size = std::clamp(m_size + d, 11, 32); QSettings().setValue(QStringLiteral("galaxy/livreSize"), m_size); sz->setText(QString::number(m_size)); applyEdFont(); };
    connect(minus, &QToolButton::clicked, this, [step]() { step(-1); });
    connect(plus, &QToolButton::clicked, this, [step]() { step(1); });
    sep();
    // negrito, itálico, sublinhado, tachado
    struct Fmt { QString text, tip; bool b, i, u, s; };
    const Fmt fmts[] = { { QStringLiteral("B"), tr("Negrito (Ctrl+B)"), true, false, false, false }, { QStringLiteral("I"), tr("Itálico (Ctrl+I)"), false, true, false, false },
                         { QStringLiteral("U"), tr("Sublinhado (Ctrl+U)"), false, false, true, false }, { QStringLiteral("S"), tr("Tachado (Ctrl+Shift+S)"), false, false, false, true } };
    QList<QPointer<QToolButton>> fbtns;
    for (const Fmt& fm : fmts) {
        auto* b = tool(fm.text, fm.tip);
        b->setCheckable(true);
        QFont f = b->font(); f.setPixelSize(14); f.setBold(fm.b); f.setItalic(fm.i); f.setUnderline(fm.u); f.setStrikeOut(fm.s); b->setFont(f);
        const bool B = fm.b, I = fm.i, U = fm.u, S = fm.s;
        connect(b, &QToolButton::clicked, this, [this, B, I, U, S]() {
            if (!m_body) return;
            QTextCharFormat cf;
            const QTextCharFormat cur = m_body->currentCharFormat();
            if (B) cf.setFontWeight(cur.fontWeight() >= QFont::Bold ? QFont::Normal : QFont::Bold);
            if (I) cf.setFontItalic(!cur.fontItalic());
            if (U) cf.setFontUnderline(!cur.fontUnderline());
            if (S) cf.setFontStrikeOut(!cur.fontStrikeOut());
            m_body->mergeCurrentCharFormat(cf);
            m_body->setFocus();
        });
        fbtns << b;
    }
    sep();
    // alinhamento e entrelinha (da folha inteira)
    struct Al { QString key, name; };
    const QList<Al> aligns = { { QStringLiteral("left"), tr("À esquerda") }, { QStringLiteral("center"), tr("Centralizado") }, { QStringLiteral("right"), tr("À direita") }, { QStringLiteral("justify"), tr("Justificado") } };
    auto* al = tool(QString(), tr("Alinhamento"));
    const auto alignIcon = [](const QString& key) { return key == QLatin1String("center") ? QStringLiteral("align-center.svg") : key == QLatin1String("right") ? QStringLiteral("align-right.svg") : key == QLatin1String("justify") ? QStringLiteral("align-justify.svg") : QStringLiteral("align-left.svg"); };
    const auto iconOf = [ink, a](const QString& file) { return IconUtils::loadToolbarIcon(QStringLiteral(":/icons/%1").arg(file), ink, a, a, QSize(16, 16)); };
    al->setIcon(iconOf(alignIcon(m_align)));
    al->setIconSize(QSize(16, 16));
    connect(al, &QToolButton::clicked, this, [this, al, aligns, alignIcon, iconOf]() {
        openPop(al, [&](QVBoxLayout* l, QFrame* p) {
            popTitle(l, p, tr("Alinhamento"));
            for (const Al& x : aligns) {
                const QString key = x.key;
                popOpt(l, p, x.name, m_align == key, QString(), [this, key]() { m_align = key; QSettings().setValue(QStringLiteral("galaxy/livreAlign"), key); applyAlignAndSpacing(); m_body->setProperty("dirty", true); m_sheetTimer.start(); m_edFor.clear(); render(); }, iconOf(alignIcon(key)).pixmap(16, 16));
            }
        });
    });
    auto* lh = tool(QString(), tr("Entrelinha"));
    lh->setIcon(iconOf(QStringLiteral("text-spacing.svg")));
    lh->setIconSize(QSize(16, 16));
    if (lh->icon().isNull()) lh->setText(QStringLiteral("↕"));
    connect(lh, &QToolButton::clicked, this, [this, lh]() {
        openPop(lh, [&](QVBoxLayout* l, QFrame* p) {
            popTitle(l, p, tr("Entrelinha"));
            const QList<QPair<int, QString>> opts = { { 130, tr("Junto") }, { 150, tr("Normal") }, { 175, tr("Arejado") }, { 200, tr("Duplo") } };
            for (const auto& [v, n] : opts) popOpt(l, p, n, m_lineH == v, nf(v / 100.0, v % 10 ? 2 : 1), [this, v]() { m_lineH = v; QSettings().setValue(QStringLiteral("galaxy/livreLine"), v); applyAlignAndSpacing(); m_body->setProperty("dirty", true); m_sheetTimer.start(); });
        });
    });
    sep();
    // imagem
    auto* img = tool(QString(), tr("Inserir imagem"));
    img->setIcon(carto::icon(QStringLiteral("image"), ink, a, 15));
    connect(img, &QToolButton::clicked, this, [this, k]() {
        if (!m_body) return;
        const QString path = QFileDialog::getOpenFileName(this, tr("Imagem"), QString(), tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (path.isEmpty() || !m_body) return;
        QImage im(path);
        if (im.isNull()) { toast(tr("Não consegui abrir essa imagem."), false); return; }
        insertImage(k, im, QFileInfo(path).completeBaseName());
    });
    sep();
    // Modo Foco, com o ícone do Qenna (o anel; aceso, o anel com o ponto)
    auto* fo = tool(QString(), tr("Modo Foco: só o parágrafo onde você está fica aceso"));
    fo->setCheckable(true);
    fo->setChecked(m_focusMode);
    fo->setIcon(iconOf(m_focusMode ? QStringLiteral("focusmode-on.svg") : QStringLiteral("focusmode-off.svg")));
    fo->setIconSize(QSize(16, 16));
    connect(fo, &QToolButton::clicked, this, [this, fo, iconOf](bool on) {
        m_focusMode = on;
        QSettings().setValue(QStringLiteral("galaxy/livreFocus"), on);
        fo->setIcon(iconOf(on ? QStringLiteral("focusmode-on.svg") : QStringLiteral("focusmode-off.svg")));
        applyFocus();
        if (m_body) m_body->setFocus();
    });
    h->addStretch();
    if (DocLink* d = docOf(k)) {
        auto* wb = btn(QString(), QStringLiteral("lvWhere"), bar);
        auto* wl = new QHBoxLayout(wb);
        wl->setContentsMargins(6, 0, 6, 0);
        QWidget* tag = whereTag(d, wb);
        tag->setAttribute(Qt::WA_TransparentForMouseEvents);
        tag->setStyleSheet(QStringLiteral("border: none;"));
        wl->addWidget(tag);
        wb->setMinimumWidth(tag->sizeHint().width() + 16);
        wb->setFixedHeight(24);
        wb->setToolTip(tr("Salvo em: %1. Clique para mudar.").arg(m_panel->docDestLabel(d)));
        connect(wb, &QPushButton::clicked, this, [this, wb, d]() { moveDocWhere(wb, d); });
        h->addWidget(wb);
    }
    // negrito e companhia acesos onde o cursor está
    QTimer::singleShot(0, this, [this, fbtns]() {
        if (!m_body) return;
        connect(m_body, &QTextEdit::currentCharFormatChanged, this, [fbtns](const QTextCharFormat& f) {
            if (fbtns.size() < 4) return;
            if (fbtns[0]) fbtns[0]->setChecked(f.fontWeight() >= QFont::Bold);
            if (fbtns[1]) fbtns[1]->setChecked(f.fontItalic());
            if (fbtns[2]) fbtns[2]->setChecked(f.fontUnderline());
            if (fbtns[3]) fbtns[3]->setChecked(f.fontStrikeOut());
        });
    });
    return bar;
}
void Livre::insertImage(const QString& k, QImage im, const QString& baseName)
{
    if (!m_body || im.isNull()) return;
    if (im.width() > 1400) im = im.scaledToWidth(1400, Qt::SmoothTransformation);
    QString src;
    const DocLink* d = docOf(k);
    if (d && d->gaveta && !m_panel->m_link.root.isEmpty()) {
        QDir().mkpath(m_panel->m_link.root + QStringLiteral("/imagens"));
        const QString file = m_panel->m_link.root + QStringLiteral("/imagens/") + (baseName.isEmpty() ? QStringLiteral("imagem") : baseName) + QLatin1Char('-') + ProjectModel::uid().left(8) + QStringLiteral(".jpg");
        if (im.save(file, "JPG", 88)) src = QUrl::fromLocalFile(file).toString();
    }
    if (src.isEmpty()) {
        QByteArray bytes; QBuffer buf(&bytes); buf.open(QIODevice::WriteOnly); im.save(&buf, "JPG", 86);
        src = QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(bytes.toBase64());
        m_body->document()->addResource(QTextDocument::ImageResource, QUrl(src), im);
    }
    QTextImageFormat tf;
    tf.setName(src);
    const double w = std::min<double>(im.width(), m_body->viewport()->width() - 40);
    tf.setWidth(w); tf.setHeight(im.height() * w / im.width());
    m_body->textCursor().insertImage(tf);
    m_body->setFocus();
}

// ── a folha ──
void Livre::renderCenter()
{
    auto* cl = static_cast<QVBoxLayout*>(m_center->layout());
    while (QLayoutItem* it = cl->takeAt(0)) {
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        else if (QLayout* lay = it->layout()) { while (QLayoutItem* x = lay->takeAt(0)) { if (QWidget* w = x->widget()) { w->hide(); w->deleteLater(); } delete x; } }
        delete it;
    }
    closeMention();
    m_body = nullptr;
    m_name = nullptr;
    m_count = nullptr;
    m_sheetFor.clear();
    const QString k = m_sel;
    if (k.isEmpty()) {
        auto* e = new QWidget(m_center);
        auto* v = new QVBoxLayout(e);
        v->addStretch();
        const bool any = !allWorlds().isEmpty() || !m_doc->regions.empty() || !livreSystems().empty();
        auto* b = lab(any ? tr("Escolha um mundo na prateleira.") : tr("Aqui não existe nada até você pedir."), QStringLiteral("lvHintBig"), e);
        b->setAlignment(Qt::AlignHCenter);
        auto* s = lab(any ? tr("Ou clique no + para criar outro.") : tr("Clique no + em Soltos, à esquerda, dê um nome e aperte Enter. O cursor já cai na folha dele."), QStringLiteral("lvHint"), e, true);
        s->setAlignment(Qt::AlignHCenter);
        s->setMaximumWidth(380);
        v->addWidget(b, 0, Qt::AlignHCenter);
        v->addWidget(s, 0, Qt::AlignHCenter);
        v->addStretch(2);
        cl->addWidget(e);
        return;
    }
    // a fonte: a escolhida, se o Qenna tem ela; senão a do app
    const QStringList fonts = m_panel->m_link.fonts ? m_panel->m_link.fonts() : QStringList();
    if (!fonts.isEmpty() && !fonts.contains(m_fontFamily, Qt::CaseInsensitive)) {
        for (const QString& c : { QStringLiteral("Source Serif 4"), QStringLiteral("Alegreya"), fonts.first() }) if (fonts.contains(c, Qt::CaseInsensitive)) { m_fontFamily = c; break; }
    }
    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    auto* folha = new QFrame(m_center);
    folha->setObjectName(QStringLiteral("lvFolha"));
    folha->setMaximumWidth(860);
    auto* fv = new QVBoxLayout(folha);
    fv->setContentsMargins(0, 0, 0, 0);
    fv->setSpacing(0);
    fv->addWidget(edBar(k, folha));
    auto* inner = new QWidget(folha);
    auto* iv = new QVBoxLayout(inner);
    iv->setContentsMargins(44, 22, 44, 8);
    iv->setSpacing(8);
    const LK t = kindOf(k);
    auto* name = new QLineEdit(inner);
    name->setObjectName(QStringLiteral("lvNameInput"));
    name->setPlaceholderText(t == LK::Link ? label(k) : tr("%1 sem nome").arg(kindName(t)));
    if (t == LK::Link) { name->setText(label(k)); name->setReadOnly(true); }
    else if (t == LK::Doc) { const DocLink* d = docOf(k); name->setText(d && d->title != Panel::tr("Documento sem título") ? d->title : QString()); }
    else if (t == LK::Terr || t == LK::Place) name->setText(m_doc->labelOf(k));
    else name->setText(m_doc->userNames.value(k));
    m_name = name;
    iv->addWidget(name);
    iv->addWidget(whereLine(k, inner));
    iv->addSpacing(4);
    if (t != LK::Link) {
        auto* nameTimer = new QTimer(name);
        nameTimer->setSingleShot(true);
        nameTimer->setInterval(260);
        connect(nameTimer, &QTimer::timeout, this, [this]() { renderLeft(); renderRight(); renderNav(); });
        connect(name, &QLineEdit::textEdited, this, [this, k, nameTimer](const QString& v) { m_typing.start(); rename(k, v); nameTimer->start(); });
    }
    connect(name, &QLineEdit::returnPressed, this, &Livre::goBody);
    if (t == LK::Link) {
        const auto* l = m_panel->m_link.places ? m_panel->m_link.places->link(k.mid(1)) : nullptr;
        auto* ends = new QHBoxLayout();
        ends->setSpacing(8);
        if (l) for (const QString& pid : { l->fromTerritorioId, l->toTerritorioId }) {
            const QString ek = m_panel->keyOfPlace(pid);
            auto* e = btn(ek.isEmpty() ? QStringLiteral("?") : label(ek), QStringLiteral("lvEnd"), inner);
            if (!ek.isEmpty()) { e->setIcon(QIcon(ball(ek, 22))); e->setIconSize(QSize(22, 22)); connect(e, &QPushButton::clicked, this, [this, ek]() { select(ek); }); }
            ends->addWidget(e);
            if (pid == l->fromTerritorioId) { auto* ic = new QLabel(inner); ic->setPixmap(carto::iconPixmap(QStringLiteral("link"), col(Theme::accentDefault()), 16)); ends->addWidget(ic); }
        }
        const QString type = m_doc->linkTypes.value(k.mid(1));
        auto* tp = btn(type.isEmpty() ? tr("Que tipo de conexão?") : type, QStringLiteral("lvLinkType"), inner);
        tp->setProperty("unset", type.isEmpty());
        connect(tp, &QPushButton::clicked, this, [this, tp, k]() {
            const QStringList types = { tr("Aliança"), tr("Rivalidade"), tr("Guerra"), tr("Comércio"), tr("Rota"), tr("Origem"), tr("Segredo") };
            pickList(tp, tr("TIPO"), types, m_doc->linkTypes.value(k.mid(1)), tr("Sem tipo"), [this, k](const QString& v) {
                if (v.isEmpty()) m_doc->linkTypes.remove(k.mid(1)); else m_doc->linkTypes.insert(k.mid(1), v);
                changed(false);
                m_edFor.clear();
                render();
            });
        });
        ends->addWidget(tp);
        ends->addStretch();
        iv->addLayout(ends);
    }
    // o texto: o editor do Qenna
    auto* body = new SheetEdit(inner);
    body->setObjectName(QStringLiteral("lvBody"));
    body->setFrameShape(QFrame::NoFrame);
    body->setAcceptRichText(true);
    body->setPlaceholderText(tr("Escreva aqui. @ menciona um lugar."));
    body->setFullSynonyms(false);
    body->setScreenplayMode(false);
    body->setPageMargins(0, 0, 0, 0);
    if (SpellChecker* sc = m_panel->m_link.spell) { body->setSpellChecker(sc); new SpellHighlighter(body->document(), sc, body); }
    if (m_panel->m_link.synonyms) body->setSynonymProvider(m_panel->m_link.synonyms);
    connect(body, &SpellEditor::synonymChosen, this, [body](const QString& rep) { QTextCursor c = body->textCursor(); if (c.hasSelection()) c.insertText(rep); });
    connect(body, &SpellEditor::addToGlossaryRequested, this, [this](const QString& w, QPoint gp) { if (m_panel->m_link.addGlossary) m_panel->m_link.addGlossary(w, gp); });
    connect(body, &SpellEditor::readAloudRequested, this, [this, body](int s, int e) {
        if (!m_panel->m_link.readAloud) return;
        QRect a = body->cursorRect();
        a.moveTopLeft(body->viewport()->mapToGlobal(a.topLeft()));
        m_panel->m_link.readAloud(body->document(), s, e, a);
    });
    m_sheetLoading = true;
    m_body = body;
    m_sheetFor = k;
    applyEdFont();
    const QString html = sheetHtml(k);
    if (!html.isEmpty()) body->setHtml(html);
    applyAlignAndSpacing();
    m_sheetLoading = false;
    body->setProperty("dirty", false);
    connect(body, &QTextEdit::textChanged, this, [this, body]() {
        if (m_sheetLoading) return;
        body->setProperty("dirty", true);
        m_typing.start();
        m_sheetTimer.start();
        syncCount();
        openMention();
    });
    connect(body, &QTextEdit::cursorPositionChanged, this, [this]() { applyFocus(); if (m_mentionPop) openMention(); });
    body->keyHook = [this, body](QKeyEvent* e) -> bool {
        const bool ctrl = e->modifiers() & Qt::ControlModifier;
        // Ctrl + − : travessão
        if (ctrl && (e->key() == Qt::Key_Minus)) { body->textCursor().insertText(QStringLiteral("—")); return true; }
        if (ctrl && e->key() == Qt::Key_B) { QTextCharFormat f; f.setFontWeight(body->currentCharFormat().fontWeight() >= QFont::Bold ? QFont::Normal : QFont::Bold); body->mergeCurrentCharFormat(f); return true; }
        if (ctrl && e->key() == Qt::Key_I) { QTextCharFormat f; f.setFontItalic(!body->currentCharFormat().fontItalic()); body->mergeCurrentCharFormat(f); return true; }
        if (ctrl && e->key() == Qt::Key_U) { QTextCharFormat f; f.setFontUnderline(!body->currentCharFormat().fontUnderline()); body->mergeCurrentCharFormat(f); return true; }
        if (ctrl && (e->modifiers() & Qt::ShiftModifier) && e->key() == Qt::Key_S) { QTextCharFormat f; f.setFontStrikeOut(!body->currentCharFormat().fontStrikeOut()); body->mergeCurrentCharFormat(f); return true; }
        if (m_mentionPop && !m_mention.list.isEmpty()) {
            if (e->key() == Qt::Key_Down || e->key() == Qt::Key_Up) { m_mention.idx = (m_mention.idx + (e->key() == Qt::Key_Down ? 1 : -1) + int(m_mention.list.size())) % int(m_mention.list.size()); openMention(); return true; }
            if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Tab) { insertMention(m_mention.list[m_mention.idx]); return true; }
        }
        if (m_mentionPop && e->key() == Qt::Key_Escape) { closeMention(); return true; }
        return false;
    };
    body->mentionClicked = [this](const QString& key) { if (valid(key)) select(key); else toast(tr("Esse lugar não existe mais."), false); };
    body->mentionHover = [this, body](const QString& key, const QRect& r) {
        if (key.isEmpty()) { m_peekTimer.stop(); QTimer::singleShot(120, this, [this]() { hidePeek(); }); return; }
        if (!valid(key)) return;
        if (!m_mentionAnchor) m_mentionAnchor = new QWidget(body->viewport());
        m_mentionAnchor->setGeometry(r);
        QPointer<QWidget> at(m_mentionAnchor);
        QTimer::singleShot(160, this, [this, at, key]() { if (at) showPeek(at, key); });
    };
    body->imagePasted = [this, k](const QImage& im) { insertImage(k, im, QString()); };
    iv->addWidget(body, 1);
    fv->addWidget(inner, 1);
    // o rodapé: quantas palavras
    auto* foot = new QWidget(folha);
    foot->setObjectName(QStringLiteral("lvSheetFoot"));
    foot->setAttribute(Qt::WA_StyledBackground, true);
    auto* fh = new QHBoxLayout(foot);
    fh->setContentsMargins(44, 7, 28, 8);
    fh->addStretch();
    m_count = lab(QString(), QStringLiteral("lvSheetCount"), foot);
    fh->addWidget(m_count);
    fv->addWidget(foot);
    syncCount();
    applyFocus();
    row->addStretch();
    row->addWidget(folha, 100);
    row->addStretch();
    cl->addLayout(row, 1);
    if (m_justMade == k && t == LK::Terr) focusName(k);
}

} // namespace galaxy
