// Formas da gaveta que guarda só documentos (planejamento, notas, pesquisa): Árvore, Fichário, Sumário e
// Caderno corrido. Aprovadas pelo Pe em 09/11/2026 no lab das gavetas de documento (o Mural ficou de fora).
// As quatro mostram a gaveta inteira, com as pastas como seções (ou galhos, na Árvore) no lugar dos botões
// de pasta do Clássico, que cortavam o nome e escondiam o que tinha dentro. Clicar abre o documento,
// arrastar leva pra outra pasta (soltando no título dela), o botão direito é o de sempre e a prévia ao
// passar o mouse é a mesma do Clássico.
#include "DrawerListPanel.h"
#include "FlowLayout.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStyle>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

namespace {
QColor tc(const QString& c) { return Theme::toColor(c); }
QColor mixc(const QColor& a, const QColor& b, double t)
{
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t), float(a.greenF() + (b.greenF() - a.greenF()) * t),
                            float(a.blueF() + (b.blueF() - a.blueF()) * t));
}
QString rgba(const QColor& c, double a) { return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a); }
QString cut(const QString& s, int n)
{
    if (s.size() <= n) return s;
    QString t = s.left(n);
    const int sp = int(t.lastIndexOf(QLatin1Char(' ')));
    if (sp > n * 2 / 3) t = t.left(sp);
    return t.trimmed() + QStringLiteral("…");
}
QString romanOf(int n)
{
    static const char* r[] = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV" };
    return n >= 1 && n <= 15 ? QString::fromLatin1(r[n - 1]) : QString::number(n);
}
QFont fontOf(const QString& family, double px, int weight = QFont::Normal, bool italic = false)
{
    QFont f(family);
    f.setPixelSize(int(px + 0.5));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    return f;
}
QLabel* textLabel(const QString& text, const QFont& f, const QColor& c, QWidget* parent, bool wrap = false)
{
    auto* l = new QLabel(text, parent);
    l->setFont(f);
    l->setWordWrap(wrap);
    l->setTextFormat(Qt::PlainText);
    l->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(c.name(QColor::HexArgb)));
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
    return l;
}

// Texto de uma linha que corta com reticências no espaço que tiver.
class ElideText : public QWidget {
public:
    ElideText(const QString& t, const QFont& f, const QColor& c, QWidget* parent) : QWidget(parent), m_t(t), m_c(c)
    {
        setFont(f);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(QFontMetrics(f).height() + 2);
        setToolTip(t);
    }
    QSize sizeHint() const override { return { qMin(220, QFontMetrics(font()).horizontalAdvance(m_t) + 2), height() }; }
    QSize minimumSizeHint() const override { return { 24, height() }; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setPen(m_c);
        p.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(font()).elidedText(m_t, Qt::ElideRight, width()));
    }

private:
    QString m_t;
    QColor m_c;
};

// Linha do Sumário: o nome, a linha pontilhada e as palavras.
class TocLine : public QWidget {
public:
    TocLine(const QString& name, const QString& num, const QColor& ink, const QColor& dim, const QColor& hover, const QColor& on, QWidget* parent)
        : QWidget(parent), m_name(name), m_num(num), m_ink(ink), m_dim(dim), m_hoverC(hover), m_onC(on)
    {
        setFixedHeight(30);
        setAttribute(Qt::WA_Hover);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setToolTip(name);
    }
    void setOn(bool v) { setProperty("on", v); update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(0, 1, 0, -1);
        const bool m_on = property("on").toBool();
        if (m_on || underMouse()) {
            p.setPen(Qt::NoPen);
            p.setBrush(m_on ? m_onC : m_hoverC);
            p.drawRoundedRect(r, 5, 5);
        }
        const QFont fn = fontOf(QStringLiteral("Alegreya"), 15);
        const QFont fw = fontOf(QStringLiteral("Alegreya"), 13);
        const QFontMetrics mn(fn), mw(fw);
        const int pad = 6, numW = mw.horizontalAdvance(m_num);
        const int nameMax = width() - pad * 2 - numW - 22;
        const QString name = mn.elidedText(m_name, Qt::ElideRight, qMax(20, nameMax));
        const int nameW = mn.horizontalAdvance(name);
        const int base = height() / 2 + mn.ascent() / 2 - 1;
        p.setFont(fn);
        p.setPen(m_ink);
        p.drawText(pad, base, name);
        p.setFont(fw);
        p.setPen(m_dim);
        p.drawText(width() - pad - numW, base, m_num);
        // pontilhado entre o nome e o número
        const int x0 = pad + nameW + 6, x1 = width() - pad - numW - 6;
        if (x1 - x0 > 6) {
            QColor d = m_ink;
            d.setAlphaF(0.32);
            p.setPen(Qt::NoPen);
            p.setBrush(d);
            for (int x = x0; x < x1; x += 4) p.drawEllipse(QPointF(x + 0.75, base - 1.0), 0.75, 0.75);
        }
    }

private:
    QString m_name, m_num;
    QColor m_ink, m_dim, m_hoverC, m_onC;
};

// Barrinha do tamanho do documento (ferramenta "Tamanho").
class SizeBar : public QWidget {
public:
    SizeBar(double frac, const QColor& c, int w, QWidget* parent) : QWidget(parent), m_f(frac), m_c(c)
    {
        setFixedSize(w, 5);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor bg = m_c;
        bg.setAlphaF(0.14);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(QRectF(0, 1, width(), 3), 1.5, 1.5);
        QColor fg = m_c;
        fg.setAlphaF(0.7);
        p.setBrush(fg);
        p.drawRoundedRect(QRectF(0, 1, qMax(3.0, width() * m_f), 3), 1.5, 1.5);
    }

private:
    double m_f;
    QColor m_c;
};

// Página com a dobra (documento) e pasta, desenhadas na cor certa.
QPixmap docGlyph(const QColor& c, qreal dpr)
{
    QPixmap pm(QSize(15, 15) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, 1.3);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    QPainterPath path;
    path.moveTo(3.5, 1.5);
    path.lineTo(9.5, 1.5);
    path.lineTo(12.5, 4.5);
    path.lineTo(12.5, 13.5);
    path.lineTo(3.5, 13.5);
    path.closeSubpath();
    p.drawPath(path);
    p.drawLine(QPointF(9.5, 1.5), QPointF(9.5, 4.5));
    p.drawLine(QPointF(9.5, 4.5), QPointF(12.5, 4.5));
    return pm;
}
QPixmap folderGlyph(const QColor& c, qreal dpr)
{
    QPixmap pm(QSize(16, 15) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QColor fill = c;
    fill.setAlphaF(0.18);
    p.setPen(QPen(c, 1.3));
    p.setBrush(fill);
    QPainterPath path;
    path.moveTo(1.5, 3.5);
    path.lineTo(6.0, 3.5);
    path.lineTo(7.5, 5.0);
    path.lineTo(14.5, 5.0);
    path.lineTo(14.5, 12.5);
    path.lineTo(1.5, 12.5);
    path.closeSubpath();
    p.drawPath(path);
    return pm;
}
} // namespace

bool DrawerListPanel::isDocStyle(Style s)
{
    return s == Style::DocTree || s == Style::DocCards || s == Style::DocToc || s == Style::DocNotebook;
}

DrawerListPanel::DocInfo DrawerListPanel::docInfo(const DrawerItem& it) const
{
    auto c = m_docInfo.constFind(it.id);
    if (c != m_docInfo.constEnd()) return c.value();
    DocInfo info;
    const QString html = itemHtml(it);
    if (!html.isEmpty()) {
        QTextDocument d;
        d.setHtml(html);
        const QString text = d.toPlainText();
        static const QRegularExpression ws(QStringLiteral("\\s+"));
        info.words = int(text.split(ws, Qt::SkipEmptyParts).size());
        // o primeiro parágrafo de verdade: pula título curto e linha de uma palavra só
        QString first;
        for (const QString& line : text.split(QLatin1Char('\n'))) {
            const QString t = line.simplified();
            if (t.isEmpty()) continue;
            if (first.isEmpty()) first = t;
            if (t.size() > 40) { first = t; break; }
        }
        info.excerpt = first.left(700);
    }
    m_docInfo.insert(it.id, info);
    return info;
}

QList<DrawerItem> DrawerListPanel::docItems() const
{
    QList<DrawerItem> out;
    const Drawer* d = m_model ? m_model->findDrawer(m_currentKey) : nullptr;
    if (!d) return out;
    const bool filter = m_toolTags && !m_tagFilter.isEmpty();
    for (const DrawerItem& it : d->items)
        if (!filter || it.tags.contains(m_tagFilter, Qt::CaseInsensitive)) out.append(it);
    if (m_sortMode == SortAlpha)
        std::sort(out.begin(), out.end(), [](const DrawerItem& a, const DrawerItem& b) { return QString::localeAwareCompare(a.title, b.title) < 0; });
    else if (!m_sortAscending)
        std::reverse(out.begin(), out.end());
    return out;
}

QStringList DrawerListPanel::docTags() const
{
    QStringList out;
    const Drawer* d = m_model ? m_model->findDrawer(m_currentKey) : nullptr;
    if (!d) return out;
    for (const DrawerItem& it : d->items)
        for (const QString& t : it.tags)
            if (!out.contains(t, Qt::CaseInsensitive)) out << t;
    std::sort(out.begin(), out.end(), [](const QString& a, const QString& b) { return QString::localeAwareCompare(a, b) < 0; });
    return out;
}

QWidget* DrawerListPanel::makeTagBar()
{
    const QStringList tags = docTags();
    if (!m_toolTags || tags.isEmpty()) return nullptr;
    auto* bar = new QWidget(this);
    auto* fl = new FlowLayout(bar, 5);
    fl->setContentsMargins(0, 2, 0, 6);
    const QColor dc(currentDrawerColor());
    const QString qss = Theme::qss(QStringLiteral(
        "QPushButton { color: %1; background: transparent; border: 1px solid %2; border-radius: @radius-control; padding: 2px 9px; font-size: 11.5px; }"
        "QPushButton:hover { color: %3; }"
        "QPushButton[on=\"true\"] { color: %3; border-color: %4; background: %5; }"))
        .arg(Theme::textMuted(), Theme::subtleBorder(), Theme::textBright(), dc.name(), rgba(dc, 0.14));
    const auto add = [&](const QString& label, const QString& tag) {
        auto* b = new QPushButton(label, bar);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setProperty("on", m_tagFilter.compare(tag, Qt::CaseInsensitive) == 0);
        b->setStyleSheet(qss);
        connect(b, &QPushButton::clicked, this, [this, tag]() {
            m_tagFilter = tag;
            QTimer::singleShot(0, this, &DrawerListPanel::rebuildContents);
        });
        fl->addWidget(b);
    };
    add(tr("Todas"), QString());
    for (const QString& t : tags) add(QStringLiteral("#") + t, t);
    return bar;
}

// Clicar abre, arrastar leva pra outra pasta, botão direito e prévia como no Clássico.
void DrawerListPanel::wireDocWidget(QWidget* w, const QString& itemId)
{
    w->setProperty("hoverItemId", itemId);
    w->setProperty("isItemPhoto", true);
    w->setProperty("itemId", itemId);
    w->setProperty("docId", itemId);
    w->setProperty("on", itemId == m_activeDocId);
    w->setAttribute(Qt::WA_Hover);
    w->setCursor(Qt::PointingHandCursor);
    w->installEventFilter(this);
    w->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(w, &QWidget::customContextMenuRequested, this, [this, w, itemId](const QPoint& pos) { showItemContextMenu(itemId, w->mapToGlobal(pos)); });
}

// Título de pasta: recebe o documento arrastado. kind 0 = galho da Árvore, 1 = seção, 2 = parte do Sumário, 3 = aba do Caderno.
QWidget* DrawerListPanel::makeFolderHeader(const QString& folderId, const QString& text, const QString& right, int kind)
{
    auto* b = new QPushButton(this);
    b->setObjectName(QStringLiteral("dfHead%1").arg(kind));
    b->setFocusPolicy(Qt::NoFocus);
    b->setCursor(kind == 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    auto* h = new QHBoxLayout(b);
    const QColor dc(currentDrawerColor()), dim = tc(Theme::textMuted()), ink = tc(Theme::textBright());
    const qreal dpr = devicePixelRatioF();
    if (kind == 0) {
        h->setContentsMargins(6, 4, 8, 4);
        h->setSpacing(7);
        const bool folded = m_foldedFolders.contains(folderId);
        h->addWidget(textLabel(folded ? QStringLiteral("▸") : QStringLiteral("▾"), fontOf(QStringLiteral("Segoe UI"), 10), dim, b));
        auto* ic = new QLabel(b);
        ic->setPixmap(folderGlyph(dc, dpr));
        ic->setAttribute(Qt::WA_TransparentForMouseEvents);
        h->addWidget(ic);
        h->addWidget(new ElideText(text, fontOf(QStringLiteral("Segoe UI"), 13.5, QFont::DemiBold), ink, b), 1);
        h->addWidget(textLabel(right, fontOf(QStringLiteral("Segoe UI"), 11), dim, b));
        b->setMinimumHeight(28);
        connect(b, &QPushButton::clicked, this, [this, folderId]() {
            if (!m_foldedFolders.remove(folderId)) m_foldedFolders.insert(folderId);
            QSettings().setValue(QStringLiteral("ui/drawerFolded/") + m_currentKey, QStringList(m_foldedFolders.begin(), m_foldedFolders.end()));
            const int y = m_scroll->verticalScrollBar()->value();
            QTimer::singleShot(0, this, [this, y]() {
                rebuildContents();
                QTimer::singleShot(0, this, [this, y]() { m_scroll->verticalScrollBar()->setValue(y); });
            });
        });
    } else if (kind == 1) {
        h->setContentsMargins(2, 10, 2, 2);
        h->setSpacing(8);
        QFont f = fontOf(QStringLiteral("Segoe UI"), 10.5, QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
        h->addWidget(textLabel(text.toUpper(), f, dim, b));
        h->addWidget(textLabel(right, fontOf(QStringLiteral("Segoe UI"), 10.5), dim, b));
        auto* line = new QFrame(b);
        line->setFixedHeight(1);
        line->setStyleSheet(QStringLiteral("background: %1;").arg(Theme::subtleBorder()));
        line->setAttribute(Qt::WA_TransparentForMouseEvents);
        h->addWidget(line, 1, Qt::AlignVCenter);
    } else if (kind == 2) {
        h->setContentsMargins(6, 14, 6, 4);
        h->setSpacing(8);
        QFont f = fontOf(QStringLiteral("Segoe UI"), 11, QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.8);
        h->addWidget(new ElideText(text.toUpper(), f, dc, b), 1);
        h->addWidget(textLabel(right, fontOf(QStringLiteral("Segoe UI"), 11), dim, b));
    } else {
        h->setContentsMargins(0, 14, 0, 0);
        h->setSpacing(0);
        QFont f = fontOf(QStringLiteral("Segoe UI"), 10.5, QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
        auto* tab = textLabel(text.toUpper(), f, ink, b);
        tab->setStyleSheet(QStringLiteral("color: %1; background: %2; border: 1px solid %3; border-bottom: none;"
                                          " border-top-left-radius: 4px; border-top-right-radius: 4px; padding: 4px 10px;")
                               .arg(ink.name(), rgba(dc, 0.14), dc.name()));
        h->addWidget(tab);
        h->addStretch(1);
    }
    b->setStyleSheet(Theme::qss(QStringLiteral(
        "QPushButton#dfHead%1 { background: transparent; border: none; border-radius: 6px; text-align: left; padding: 0; %2 }"
        "QPushButton#dfHead%1:hover { background: %3; }"))
        .arg(kind)
        .arg(kind == 3 ? QStringLiteral("border-bottom: 1px solid %1; border-radius: 0;").arg(rgba(dc, 0.6)) : QString())
        .arg(kind == 0 ? Theme::hoverOverlay() : QStringLiteral("transparent")));
    // botão com layout dentro não cresce sozinho: a altura vem do conteúdo
    b->setMinimumHeight(qMax(b->minimumHeight(), h->sizeHint().height()));
    if (!folderId.isEmpty()) {
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, &QPushButton::customContextMenuRequested, this, [this, b, folderId](const QPoint& pos) { showFolderContextMenu(folderId, b->mapToGlobal(pos)); });
    }
    installDropTargetOnFolderChip(b, folderId);
    return b;
}

namespace {
// As pastas em ordem de leitura (pai antes dos filhos), com o caminho "Pai / Filha" e a profundidade.
struct FolderRow { QString id, title, path; int depth = 0; };
QList<FolderRow> folderOrder(const Drawer& d)
{
    QList<FolderRow> out;
    std::function<void(const QString&, const QString&, int)> walk = [&](const QString& parent, const QString& path, int depth) {
        QList<Folder> kids;
        for (const Folder& f : d.folders) if (f.parentId == parent) kids << f;
        std::sort(kids.begin(), kids.end(), [](const Folder& a, const Folder& b) { return QString::localeAwareCompare(a.title, b.title) < 0; });
        for (const Folder& f : kids) {
            const QString p = path.isEmpty() ? f.title : path + QStringLiteral(" / ") + f.title;
            out.append({ f.id, f.title, p, depth });
            walk(f.id, p, depth + 1);
        }
    };
    walk(QString(), QString(), 0);
    return out;
}
} // namespace

// ── Árvore: pastas como galhos que dobram, documentos embaixo; os soltos no topo ──
void DrawerListPanel::buildDocTree()
{
    const Drawer* d = m_model ? m_model->findDrawer(m_currentKey) : nullptr;
    if (!d) return;
    auto* box = new QWidget(this);
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(1);
    if (QWidget* tb = makeTagBar()) v->addWidget(tb);
    const QList<DrawerItem> items = docItems();
    const bool filter = m_toolTags && !m_tagFilter.isEmpty();
    int maxW = 1;
    if (m_toolSize) for (const DrawerItem& it : items) maxW = qMax(maxW, docInfo(it).words);
    const QColor ink = tc(Theme::textPrimary()), dim = tc(Theme::textMuted()), dc(currentDrawerColor());
    const qreal dpr = devicePixelRatioF();
    const QString rowQss = QStringLiteral(
        "QFrame#dtRow { background: transparent; border: 1px solid transparent; border-radius: 6px; }"
        "QFrame#dtRow:hover { background: %1; }"
        "QFrame#dtRow[on=\"true\"] { background: %2; border-color: %3; }")
        .arg(Theme::hoverOverlay(), rgba(tc(Theme::accentDefault()), 0.15), Theme::accentDefault());
    const auto docRow = [&](const DrawerItem& it, int depth) {
        auto* r = new QFrame(box);
        r->setObjectName(QStringLiteral("dtRow"));
        r->setStyleSheet(rowQss);
        auto* h = new QHBoxLayout(r);
        h->setContentsMargins(8 + depth * 20, 4, 8, 4);
        h->setSpacing(8);
        auto* ic = new QLabel(r);
        ic->setPixmap(docGlyph(dim, dpr));
        ic->setAttribute(Qt::WA_TransparentForMouseEvents);
        h->addWidget(ic);
        h->addWidget(new ElideText(it.title.isEmpty() ? tr("(sem nome)") : it.title, fontOf(QStringLiteral("Segoe UI"), 13.5), ink, r), 1);
        if (m_toolSize) {
            const int w = docInfo(it).words;
            h->addWidget(new SizeBar(double(w) / maxW, dc, 40, r), 0, Qt::AlignVCenter);
            h->addWidget(textLabel(QLocale().toString(w), fontOf(QStringLiteral("Segoe UI"), 11), dim, r));
        }
        wireDocWidget(r, it.id);
        v->addWidget(r);
    };
    for (const DrawerItem& it : items) if (it.folderId.isEmpty()) docRow(it, 0);
    // quantos documentos moram em cada pasta, contando as de dentro
    const QList<FolderRow> folders = folderOrder(*d);
    QHash<QString, int> count;
    for (const DrawerItem& it : items) {
        QString f = it.folderId;
        while (!f.isEmpty()) {
            count[f]++;
            QString up;
            for (const Folder& x : d->folders) if (x.id == f) { up = x.parentId; break; }
            f = up;
        }
    }
    QSet<QString> hiddenUnder;   // pastas dentro de uma dobrada
    for (const FolderRow& f : folders) {
        QString parent;
        for (const Folder& x : d->folders) if (x.id == f.id) { parent = x.parentId; break; }
        if (!parent.isEmpty() && (hiddenUnder.contains(parent) || m_foldedFolders.contains(parent))) { hiddenUnder.insert(f.id); continue; }
        if (filter && !count.value(f.id)) continue;
        auto* head = makeFolderHeader(f.id, f.title, QString::number(count.value(f.id)), 0);
        if (auto* lay = head->layout()) { const QMargins m = lay->contentsMargins(); lay->setContentsMargins(m.left() + f.depth * 20, m.top(), m.right(), m.bottom()); }
        v->addWidget(head);
        if (m_foldedFolders.contains(f.id)) continue;
        for (const DrawerItem& it : items) if (it.folderId == f.id) docRow(it, f.depth + 1);
    }
    if (items.isEmpty() && folders.isEmpty()) v->addWidget(makeEmptyState());
    m_listLayout->insertWidget(0, box);
}

// ── Fichário: um cartão por documento, com as primeiras linhas; as pastas viram seções ──
void DrawerListPanel::buildDocCards()
{
    const Drawer* d = m_model ? m_model->findDrawer(m_currentKey) : nullptr;
    if (!d) return;
    auto* box = new QWidget(this);
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);
    if (QWidget* tb = makeTagBar()) v->addWidget(tb);
    const QList<DrawerItem> items = docItems();
    const QColor page = tc(Theme::editorBackground()), pink = tc(Theme::editorTextColor()), dc(currentDrawerColor());
    const QColor ex = mixc(pink, page, 0.24), dim = mixc(pink, page, 0.45);
    int maxW = 1;
    if (m_toolSize) for (const DrawerItem& it : items) maxW = qMax(maxW, docInfo(it).words);
    const QString cardQss = QStringLiteral(
        "QFrame#dcCard { background: %1; border: 1px solid %2; border-left: 3px solid %3; border-radius: 6px; }"
        "QFrame#dcCard:hover { border-color: %4; border-left-color: %3; }"
        "QFrame#dcCard[on=\"true\"] { border-color: %5; border-left-color: %3; }")
        .arg(page.name(), rgba(pink, 0.14), dc.name(), rgba(dc, 0.6), Theme::accentDefault());
    const auto card = [&](const DrawerItem& it) {
        const DocInfo info = docInfo(it);
        auto* c = new QFrame(box);
        c->setObjectName(QStringLiteral("dcCard"));
        c->setStyleSheet(cardQss);
        auto* cv = new QVBoxLayout(c);
        cv->setContentsMargins(13, 10, 13, 10);
        cv->setSpacing(5);
        cv->addWidget(textLabel(it.title.isEmpty() ? tr("(sem nome)") : it.title, fontOf(QStringLiteral("Lora"), 15, QFont::DemiBold), pink, c, true));
        if (!info.excerpt.isEmpty()) cv->addWidget(textLabel(cut(info.excerpt, 170), fontOf(QStringLiteral("Alegreya"), 13.5), ex, c, true));
        auto* ft = new QHBoxLayout();
        ft->setSpacing(10);
        ft->addWidget(textLabel(tr("%1 palavras · %2 min").arg(QLocale().toString(info.words)).arg(qMax(1, int(info.words / 200.0 + 0.5))),
                                fontOf(QStringLiteral("Segoe UI"), 11), dim, c));
        if (!it.tags.isEmpty()) {
            QStringList hs;
            for (const QString& t : it.tags) hs << QStringLiteral("#") + t;
            ft->addWidget(textLabel(hs.join(QLatin1Char(' ')), fontOf(QStringLiteral("Segoe UI"), 11), dc, c));
        }
        if (m_toolSize) ft->addWidget(new SizeBar(double(info.words) / maxW, dc, 56, c), 0, Qt::AlignVCenter);
        ft->addStretch(1);
        cv->addLayout(ft);
        wireDocWidget(c, it.id);
        v->addWidget(c);
    };
    const QList<FolderRow> folders = folderOrder(*d);
    QList<DrawerItem> loose;
    for (const DrawerItem& it : items) if (it.folderId.isEmpty()) loose << it;
    if (!folders.isEmpty() && !loose.isEmpty()) v->addWidget(makeFolderHeader(QString(), tr("Sem pasta"), QString::number(loose.size()), 1));
    for (const DrawerItem& it : loose) card(it);
    for (const FolderRow& f : folders) {
        QList<DrawerItem> in;
        for (const DrawerItem& it : items) if (it.folderId == f.id) in << it;
        if (in.isEmpty() && m_toolTags && !m_tagFilter.isEmpty()) continue;
        v->addWidget(makeFolderHeader(f.id, f.path, QString::number(in.size()), 1));
        for (const DrawerItem& it : in) card(it);
    }
    if (items.isEmpty() && folders.isEmpty()) v->addWidget(makeEmptyState());
    m_listLayout->insertWidget(0, box);
}

// ── Sumário: cada pasta é uma parte, com o total; o pontilhado leva até as palavras de cada um ──
void DrawerListPanel::buildDocToc()
{
    const Drawer* d = m_model ? m_model->findDrawer(m_currentKey) : nullptr;
    if (!d) return;
    auto* box = new QWidget(this);
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    if (QWidget* tb = makeTagBar()) v->addWidget(tb);
    const QList<DrawerItem> items = docItems();
    int total = 0;
    for (const DrawerItem& it : items) total += docInfo(it).words;
    const QColor ink = tc(Theme::textPrimary()), dim = tc(Theme::textMuted()), bright = tc(Theme::textBright());
    const QColor hover = tc(Theme::hoverOverlay()), on = [] { QColor c = tc(Theme::accentDefault()); c.setAlphaF(0.15); return c; }();
    auto* top = new QWidget(box);
    auto* tv = new QVBoxLayout(top);
    tv->setContentsMargins(0, 8, 0, 4);
    tv->setSpacing(2);
    auto* tt = textLabel(d->title, fontOf(QStringLiteral("Alegreya"), 20, QFont::Normal, true), bright, top);
    tt->setAlignment(Qt::AlignHCenter);
    tv->addWidget(tt);
    auto* ts = textLabel(tr("%1 documentos · %2 palavras · %3 min de leitura").arg(items.size()).arg(QLocale().toString(total)).arg(qMax(1, int(total / 200.0 + 0.5))),
                         fontOf(QStringLiteral("Segoe UI"), 11), dim, top, true);
    ts->setAlignment(Qt::AlignHCenter);
    tv->addWidget(ts);
    v->addWidget(top);
    const auto line = [&](const DrawerItem& it) {
        auto* l = new TocLine(it.title.isEmpty() ? tr("(sem nome)") : it.title, QLocale().toString(docInfo(it).words), ink, dim, hover, on, box);
        l->setOn(it.id == m_activeDocId);
        wireDocWidget(l, it.id);
        v->addWidget(l);
    };
    for (const DrawerItem& it : items) if (it.folderId.isEmpty()) line(it);
    int n = 0;
    for (const FolderRow& f : folderOrder(*d)) {
        QList<DrawerItem> in;
        int w = 0;
        for (const DrawerItem& it : items) if (it.folderId == f.id) { in << it; w += docInfo(it).words; }
        if (in.isEmpty() && m_toolTags && !m_tagFilter.isEmpty()) continue;
        v->addWidget(makeFolderHeader(f.id, QStringLiteral("%1 · %2").arg(romanOf(++n), f.path), tr("%1 palavras").arg(QLocale().toString(w)), 2));
        for (const DrawerItem& it : in) line(it);
    }
    if (items.isEmpty() && d->folders.isEmpty()) v->addWidget(makeEmptyState());
    m_listLayout->insertWidget(0, box);
}

// ── Caderno corrido: título, de onde é, palavras e o primeiro parágrafo de cada um, pra reler ──
void DrawerListPanel::buildDocNotebook()
{
    const Drawer* d = m_model ? m_model->findDrawer(m_currentKey) : nullptr;
    if (!d) return;
    auto* box = new QWidget(this);
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    if (QWidget* tb = makeTagBar()) v->addWidget(tb);
    const QList<DrawerItem> items = docItems();
    const QColor ink = tc(Theme::textPrimary()), dim = tc(Theme::textMuted()), bright = tc(Theme::textBright()), dc(currentDrawerColor());
    const QString blockQss = QStringLiteral(
        "QFrame#dnBlock { background: transparent; border: none; border-bottom: 1px solid %1; border-radius: 0; }"
        "QFrame#dnBlock:hover { background: %2; }"
        "QFrame#dnBlock[on=\"true\"] { background: %3; }")
        .arg(Theme::subtleBorder(), Theme::hoverOverlay(), rgba(tc(Theme::accentDefault()), 0.13));
    const auto block = [&](const DrawerItem& it, const QString& where) {
        const DocInfo info = docInfo(it);
        auto* b = new QFrame(box);
        b->setObjectName(QStringLiteral("dnBlock"));
        b->setStyleSheet(blockQss);
        auto* bv = new QVBoxLayout(b);
        bv->setContentsMargins(8, 10, 8, 13);
        bv->setSpacing(4);
        bv->addWidget(textLabel(it.title.isEmpty() ? tr("(sem nome)") : it.title, fontOf(QStringLiteral("Alegreya"), 17, QFont::Bold), bright, b, true));
        QStringList meta;
        if (!where.isEmpty()) meta << where;
        meta << tr("%1 palavras").arg(QLocale().toString(info.words)) << tr("%1 min").arg(qMax(1, int(info.words / 200.0 + 0.5)));
        for (const QString& t : it.tags) meta << QStringLiteral("#") + t;
        bv->addWidget(textLabel(meta.join(QStringLiteral(" · ")), fontOf(QStringLiteral("Segoe UI"), 11), dim, b, true));
        if (!info.excerpt.isEmpty()) bv->addWidget(textLabel(cut(info.excerpt, 460), fontOf(QStringLiteral("Alegreya"), 14), ink, b, true));
        bv->addWidget(textLabel(tr("Abrir ›"), fontOf(QStringLiteral("Segoe UI"), 12, QFont::DemiBold), dc, b));
        wireDocWidget(b, it.id);
        v->addWidget(b);
    };
    for (const DrawerItem& it : items) if (it.folderId.isEmpty()) block(it, QString());
    for (const FolderRow& f : folderOrder(*d)) {
        QList<DrawerItem> in;
        for (const DrawerItem& it : items) if (it.folderId == f.id) in << it;
        if (in.isEmpty() && m_toolTags && !m_tagFilter.isEmpty()) continue;
        v->addWidget(makeFolderHeader(f.id, f.path, QString(), 3));
        for (const DrawerItem& it : in) block(it, f.title);
    }
    if (items.isEmpty() && d->folders.isEmpty()) v->addWidget(makeEmptyState());
    m_listLayout->insertWidget(0, box);
}
