#include "GalaxyCaderno.h"
#include "GalaxyHab.h"
#include "GalaxyPanel.h"
#include "GalaxyWidgets.h"

#include "../Theme.h"
#include "../carto/CartoIcons.h"

#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRadialGradient>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace galaxy {

namespace {
inline QString coreT(const char* s) { return QCoreApplication::translate("galaxy::Core", s); }
inline QString catT(const char* s) { return QCoreApplication::translate("galaxy::Catalog", s); }
inline int cdPx(const Body& p, double k, double lo, double hi) { return int(std::floor(std::min(hi, std::max(lo, lo + k * std::sqrt(p.radius != 0 ? p.radius : 1))) + 0.5)); }
QColor typeColorOf(Type t)
{
    switch (t) {
    case Type::Temperado: case Type::Temperada: return QColor(70, 150, 110); case Type::Oceano: return QColor(50, 110, 180); case Type::Deserto: return QColor(200, 150, 90); case Type::Arido: return QColor(140, 132, 124);
    case Type::Lava: return QColor(210, 90, 50); case Type::Gelado: return QColor(205, 222, 234); case Type::Toxico: return QColor(200, 178, 76); case Type::Anao: return QColor(166, 156, 146);
    case Type::Gigante: return QColor(202, 165, 122); case Type::Quente: return QColor(196, 106, 64); case Type::GeloG: return QColor(111, 179, 204); case Type::Subnetuno: return QColor(147, 166, 204);
    case Type::Rochosa: return QColor(154, 148, 142); case Type::Gelada: return QColor(214, 228, 236); case Type::Vulcanica: return QColor(220, 190, 90);
    default: return QColor(150, 150, 150);
    }
}
class Dot : public QWidget {
public:
    Dot(const QColor& c, int px, bool living, QWidget* parent) : QWidget(parent), m_c(c), m_living(living) { setFixedSize(px + 4, px + 4); setAttribute(Qt::WA_TransparentForMouseEvents); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r(2, 2, width() - 4, height() - 4);
        p.setPen(Qt::NoPen); p.setBrush(m_c);
        p.drawEllipse(r);
        if (m_living) { p.setPen(QPen(QColor(0x6f, 0xd6, 0x9a), 1)); p.setBrush(Qt::NoBrush); p.drawEllipse(r.adjusted(-1.5, -1.5, 1.5, 1.5)); }
    }
private:
    QColor m_c;
    bool m_living;
};
class Sun : public QWidget {
public:
    Sun(const std::array<int, 3>& c, int px, QWidget* parent, bool half = false) : QWidget(parent), m_c(c), m_half(half) { setFixedSize(half ? px / 2 : px, px); setAttribute(Qt::WA_TransparentForMouseEvents); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double R = height() / 2.0, cx = m_half ? 0 : R;
        QRadialGradient g(cx, R, R);
        g.setColorAt(0, Qt::white); g.setColorAt(0.16, Qt::white); g.setColorAt(0.42, QColor(m_c[0], m_c[1], m_c[2])); g.setColorAt(0.72, QColor(m_c[0], m_c[1], m_c[2], 0));
        p.setPen(Qt::NoPen); p.setBrush(g);
        p.drawEllipse(QPointF(cx, R), R, R);
    }
private:
    std::array<int, 3> m_c;
    bool m_half;
};
QLabel* lab(const QString& text, const QString& obj, QWidget* parent, bool wrap = false)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(obj);
    l->setWordWrap(wrap);
    return l;
}
}

Caderno::Caderno(Panel* panel, Canvas* canvas, Doc* doc) : QWidget(panel), m_panel(panel), m_canvas(canvas), m_doc(doc)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("cdRoot"));
    m_qTimer.setSingleShot(true);
    m_qTimer.setInterval(140);
    connect(&m_qTimer, &QTimer::timeout, this, &Caderno::render);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 16);
    root->setSpacing(10);
    applyTheme();
}
void Caderno::applyTheme()
{
    setStyleSheet(Theme::qss(QStringLiteral(
        "QWidget#cdRoot { background: %1; }"
        "QLabel#cdBrand { color: %5; font-family: 'IM FELL English SC', Georgia, serif; font-size: 17px; letter-spacing: 2px; background: transparent; }"
        "QFrame#cdPane { background: %2; border: 1px solid %3; border-radius: 12px; }"
        "QScrollArea#cdScroll, QWidget#cdScrollBody { background: transparent; border: none; }"
        "QLineEdit#cdSearch { background: %6; border: 1px solid %3; border-radius: 8px; color: %4; font-size: 13px; padding: 6px 10px; }"
        "QPushButton#cdClose, QPushButton#cdPick, QPushButton#cdOpt { background: %6; border: 1px solid %3; border-radius: 8px; color: %4; font-size: 12px; padding: 6px 11px; text-align: left; }"
        "QPushButton#cdClose:hover, QPushButton#cdPick:hover, QPushButton#cdOpt:hover { border-color: %5; }"
        "QPushButton#cdOpt[on=\"true\"] { background: %7; border-color: %5; }"
        "QPushButton#cdRow { background: transparent; border: 1px solid transparent; border-radius: 9px; text-align: left; }"
        "QPushButton#cdRow:hover { background: %6; border-color: %3; }"
        "QPushButton#cdRow[on=\"true\"] { background: %7; border-color: %5; }"
        "QToolButton#cdTw { background: transparent; border: none; color: %8; font-size: 11px; padding: 0; }"
        "QLabel#cdName { color: %4; font-size: 13px; font-weight: 600; background: transparent; }"
        "QLabel#cdUnnamed { color: %8; font-size: 13px; font-weight: 500; background: transparent; }"
        "QLabel#cdSmall { color: %8; font-size: 11px; background: transparent; }"
        "QLabel#cdBadge { color: %5; font-size: 11px; font-weight: 500; background: transparent; }"
        "QLabel#cdH1 { color: %9; font-size: 32px; font-weight: 600; background: transparent; }"
        "QLabel#cdH1Un { color: %8; font-size: 32px; font-weight: 500; background: transparent; }"
        "QLabel#cdH2 { color: %9; font-size: 22px; font-weight: 600; background: transparent; }"
        "QLabel#cdSub { color: %8; font-size: 13px; background: transparent; }"
        "QLabel#cdEye { color: %8; font-size: 11px; font-weight: 700; letter-spacing: 1px; background: transparent; }"
        "QPushButton#cdLink { background: transparent; border: none; color: %5; font-size: 12px; font-weight: 600; padding: 0; text-align: left; }"
        "QPushButton#cdLink:hover { text-decoration: underline; }"
        "QPushButton#cdCrumb { background: transparent; border: none; color: %8; font-size: 12px; padding: 0; text-align: left; }"
        "QPushButton#cdCrumb:hover { color: %4; }"
        "QFrame#cdHero { background: %6; border: 1px solid %3; border-radius: 14px; }"
        "QFrame#cdMini { background: transparent; border: 1px solid %3; border-radius: 12px; }"
        "QPushButton#cdCard { background: %6; border: 1px solid %3; border-radius: 10px; text-align: left; }"
        "QPushButton#cdCard:hover { border-color: %5; }"
        "QPushButton#cdDoc { background: %6; border: 1px solid %3; border-radius: 10px; text-align: left; }"
        "QPushButton#cdDoc:hover { border-color: %5; }"
        "QPushButton#cdDocNew { background: transparent; border: 1px dashed %3; border-radius: 10px; color: %5; font-size: 12px; font-weight: 600; min-height: 80px; }"
        "QPushButton#cdDocNew:hover { border-color: %5; }"
        "QPushButton#cdKid { background: %6; border: 1px solid %3; border-radius: 15px; color: %4; font-size: 12px; padding: 4px 12px; text-align: left; }"
        "QPushButton#cdKid:hover { border-color: %5; }"
        "QPushButton#cdP { background: transparent; border: 1px solid transparent; border-radius: 12px; }"
        "QPushButton#cdP:hover { background: %6; border-color: %3; }"
        "QPushButton#cdMm { background: transparent; border: 1.5px solid transparent; border-radius: 22px; }"
        "QPushButton#cdMm[on=\"true\"] { border-color: %5; }"
        "QPushButton#cdMm:hover { border-color: %3; }"
    ).arg(Theme::chromeBackground(), Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(), Theme::accentDefault(), Theme::hoverOverlay(),
          QStringLiteral("rgba(%1,%2,%3,0.18)").arg(Theme::toColor(Theme::accentDefault()).red()).arg(Theme::toColor(Theme::accentDefault()).green()).arg(Theme::toColor(Theme::accentDefault()).blue()), Theme::textMuted(), Theme::textBright())));
}
void Caderno::paintEvent(QPaintEvent* e) { QWidget::paintEvent(e); }
void Caderno::showEvent(QShowEvent*) {}

QString Caderno::label(const QString& k) const
{
    if (k == QLatin1String("OUT")) return tr("Fora das regiões");
    return m_doc->labelOf(k);
}
bool Caderno::valid(const QString& k) const { return k == QLatin1String("OUT") || bool(m_doc->resolve(k)); }
QStringList Caderno::path(const QString& k) const
{
    if (k == QLatin1String("OUT")) return { QStringLiteral("GAL"), QStringLiteral("OUT") };
    const QStringList p = m_doc->pathOf(k);
    if (p.size() > 1) {
        const Ref o = m_doc->resolve(p[1]);
        if (o.kind == RefKind::System && !o.sys->isCore) { QStringList out { QStringLiteral("GAL"), QStringLiteral("OUT") }; out << p.mid(1); return out; }
    }
    return p;
}
QString Caderno::parentKey(const QString& k) const { const QStringList p = path(k); return p.size() > 1 ? p[p.size() - 2] : QString(); }
QString Caderno::catOf(const Body& p) const
{
    if (isLiving(p.type)) return QStringLiteral("vida");
    if (p.type == Type::Gigante || p.type == Type::Quente || p.type == Type::Subnetuno) return QStringLiteral("gas");
    if (p.type == Type::Gelado || p.type == Type::GeloG || p.type == Type::Gelada) return QStringLiteral("gelo");
    return QStringLiteral("rocha");
}
bool Caderno::pMatch(const System& s, const Body& p) const
{
    if (m_filter == QLatin1String("tudo")) return true;
    if (m_filter == QLatin1String("meus")) {
        if (m_doc->touched(keyP(s, p))) return true;
        for (const auto& m : p.moons) if (m_doc->touched(keyM(s, p, *m))) return true;
        return false;
    }
    const auto cl = [&](const Body& b, const Body* h) { return habInfo(b, &s, h).cls; };
    if (m_filter == QLatin1String("vida")) {
        const int c0 = cl(p, nullptr);
        if (c0 && c0 <= 2) return true;
        for (const auto& m : p.moons) { const int c1 = cl(*m, &p); if (c1 && c1 <= 2) return true; }
        return false;
    }
    if (m_filter.size() == 2 && m_filter[0] == QLatin1Char('c')) {
        const int n = m_filter.mid(1).toInt();
        if (cl(p, nullptr) == n) return true;
        for (const auto& m : p.moons) if (cl(*m, &p) == n) return true;
        return false;
    }
    return catOf(p) == m_filter;
}
bool Caderno::sysVisible(const System& s, const QString& q) const
{
    if (!q.isEmpty()) {
        const auto hit = [&](const QString& k) { return m_doc->labelOf(k).toLower().contains(q); };
        bool any = hit(keyS(s));
        if (!any) for (Body* p : planetsOf(s)) { if (hit(keyP(s, *p))) any = true; for (const auto& m : p->moons) if (hit(keyM(s, *p, *m))) any = true; }
        if (!any) return false;
    }
    if (m_filter == QLatin1String("tudo")) return true;
    if (m_filter == QLatin1String("meus") && m_doc->touched(keyS(s))) return true;
    for (Body* p : planetsOf(s)) if (pMatch(s, *p)) return true;
    return false;
}
std::vector<System*> Caderno::systemsOf(const QString& k) const
{
    std::vector<System*> list;
    if (k == QLatin1String("OUT")) { for (const auto& s : m_doc->gal->systems) if (!s->gone && !s->isCore && !m_doc->regionOfSys(*s)) list.push_back(s.get()); }
    else if (const Ref o = m_doc->resolve(k); o.kind == RefKind::Region) for (int id : o.region->systems) list.push_back(m_doc->gal->systems[size_t(id)].get());
    const auto rank = [this](const System* s) { return m_doc->userNames.contains(keyS(*s)) ? 0 : livingCount(*s) ? 1 : 2; };
    std::stable_sort(list.begin(), list.end(), [&](const System* a, const System* b) {
        const int ra = rank(a), rb = rank(b);
        if (ra != rb) return ra < rb;
        return std::hypot(a->x, a->y) < std::hypot(b->x, b->y);
    });
    return list;
}

void Caderno::open(const QString& key, const QString& fromTool)
{
    if (!isVisible()) m_from = fromTool == QLatin1String("explore") || fromTool == QLatin1String("edit") ? fromTool : QStringLiteral("pan");
    m_key = valid(key) ? key : QStringLiteral("GAL");
    m_cdDoc = nullptr;
    for (const QString& a : path(m_key)) m_open.insert(a);
    render();
}
void Caderno::closeIt() { m_cdDoc = nullptr; }
void Caderno::go(const QString& k)
{
    QTimer::singleShot(0, this, [this, k]() {
        m_key = k;
        m_cdDoc = nullptr;
        for (const QString& a : path(k)) m_open.insert(a);
        if (k != QLatin1String("OUT")) m_canvas->sel = k;
        render();
    });
}
void Caderno::openDoc(DocLink* d)
{
    QTimer::singleShot(0, this, [this, d]() {
        m_cdDoc = d;
        m_key = d->owner;
        for (const QString& a : path(d->owner)) m_open.insert(a);
        render();
    });
}
void Caderno::leaveDoc()
{
    DocLink* d = m_cdDoc;
    m_cdDoc = nullptr;
    if (d) m_panel->docDropIfEmpty(d);
    QTimer::singleShot(0, this, &Caderno::render);
}
bool Caderno::escape()
{
    if (m_cdDoc) { leaveDoc(); return true; }
    const QString p = parentKey(m_key);
    if (p.isEmpty()) return false;
    go(p);
    return true;
}

// ── a tela ──
void Caderno::render()
{
    if (!valid(m_key)) m_key = QStringLiteral("GAL");
    const int keepTree = m_treeScroll ? m_treeScroll->verticalScrollBar()->value() : 0;
    const bool hadQ = m_qEdit && m_qEdit->hasFocus();
    const int pos = hadQ ? m_qEdit->cursorPosition() : 0;
    if (m_content) { m_content->hide(); m_content->deleteLater(); }
    m_content = new QWidget(this);
    static_cast<QVBoxLayout*>(layout())->addWidget(m_content);
    auto* root = new QVBoxLayout(m_content);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);
    const bool side = m_cdDoc && m_ficha;
    auto* top = new QHBoxLayout();
    top->addWidget(lab(tr("CADERNO"), QStringLiteral("cdBrand"), m_content));
    top->addWidget(lab(m_doc->labelOf(QStringLiteral("GAL")), QStringLiteral("cdSub"), m_content));
    top->addStretch();
    auto* cl = new QPushButton(tr("Voltar pro mapa"), m_content);
    cl->setObjectName(QStringLiteral("cdClose"));
    cl->setCursor(Qt::PointingHandCursor);
    cl->setFocusPolicy(Qt::NoFocus);
    connect(cl, &QPushButton::clicked, this, [this]() { QTimer::singleShot(0, m_panel, [this]() { m_panel->closeCaderno(); }); });
    top->addWidget(cl);
    root->addLayout(top);
    auto* grid = new QHBoxLayout();
    grid->setSpacing(12);

    // a árvore
    auto* treePane = new QFrame(m_content);
    treePane->setObjectName(QStringLiteral("cdPane"));
    treePane->setFixedWidth(side ? 300 : 340);
    auto* tp = new QVBoxLayout(treePane);
    tp->setContentsMargins(0, 0, 0, 0);
    tp->setSpacing(0);
    auto* headW = new QWidget(treePane);
    auto* head = new QVBoxLayout(headW);
    head->setContentsMargins(14, 12, 14, 10);
    head->setSpacing(8);
    m_qEdit = new QLineEdit(m_q, headW);
    m_qEdit->setObjectName(QStringLiteral("cdSearch"));
    m_qEdit->setPlaceholderText(tr("Buscar sistema, planeta ou lua"));
    connect(m_qEdit, &QLineEdit::textEdited, this, [this](const QString& t) { m_q = t; m_qTimer.start(); });
    head->addWidget(m_qEdit);
    // as contagens do filtro
    QHash<QString, int> c;
    for (const auto& s : m_doc->gal->systems) {
        if (s->gone) continue;
        if (m_doc->touched(keyS(*s))) c[QStringLiteral("meus")]++;
        for (Body* p : planetsOf(*s)) {
            const auto see = [&](int n) { if (n) c[QStringLiteral("c%1").arg(n)]++; if (n && n <= 2) c[QStringLiteral("vida")]++; };
            c[QStringLiteral("tudo")]++;
            { const QString cat = catOf(*p); if (cat != QLatin1String("vida")) c[cat]++; }
            see(habInfo(*p, s.get(), nullptr).cls);
            if (m_doc->touched(keyP(*s, *p))) c[QStringLiteral("meus")]++;
            for (const auto& m : p->moons) { see(habInfo(*m, s.get(), p).cls); if (m_doc->touched(keyM(*s, *p, *m))) c[QStringLiteral("meus")]++; }
        }
    }
    const QList<QPair<QString, QString>> cats = { { "tudo", tr("Tudo") }, { "vida", tr("Habitáveis (classe I ou II)") }, { "c1", tr("Classe %1").arg(habLabel(1)) }, { "c2", tr("Classe %1").arg(habLabel(2)) }, { "c3", tr("Classe %1").arg(habLabel(3)) }, { "c4", tr("Classe %1").arg(habLabel(4)) }, { "c5", tr("Classe %1").arg(habLabel(5)) }, { "c6", tr("Classe %1").arg(habLabel(6)) }, { "rocha", tr("Rochosos") }, { "gas", tr("Gasosos") }, { "gelo", tr("Gelados") }, { "meus", tr("Com nome ou docs") } };
    QString curLabel;
    for (const auto& q : cats) if (q.first == m_filter) curLabel = q.second;
    auto* pick = new QPushButton(QStringLiteral("%1 %2  %3  %4").arg(tr("Mostrar"), curLabel, nf(c.value(m_filter)), m_pick ? QStringLiteral("▴") : QStringLiteral("▾")), headW);
    pick->setObjectName(QStringLiteral("cdPick"));
    pick->setCursor(Qt::PointingHandCursor);
    pick->setFocusPolicy(Qt::NoFocus);
    connect(pick, &QPushButton::clicked, this, [this]() { m_pick = !m_pick; QTimer::singleShot(0, this, &Caderno::render); });
    head->addWidget(pick, 0, Qt::AlignLeft);
    if (m_pick) {
        auto* g = new QGridLayout();
        g->setSpacing(3);
        int i = 0;
        for (const auto& q : cats) {
            auto* b = new QPushButton(QStringLiteral("%1  %2").arg(q.second, nf(c.value(q.first))), headW);
            b->setObjectName(QStringLiteral("cdOpt"));
            b->setProperty("on", m_filter == q.first);
            b->setEnabled(q.first == QLatin1String("tudo") || c.value(q.first) > 0);
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            const QString f = q.first;
            connect(b, &QPushButton::clicked, this, [this, f]() { m_filter = f; m_pick = false; QTimer::singleShot(0, this, &Caderno::render); });
            g->addWidget(b, i / 2, i % 2);
            i++;
        }
        head->addLayout(g);
    }
    tp->addWidget(headW);
    m_treeScroll = new QScrollArea(treePane);
    m_treeScroll->setObjectName(QStringLiteral("cdScroll"));
    m_treeScroll->setFrameShape(QFrame::NoFrame);
    m_treeScroll->setWidgetResizable(true);
    m_treeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_treeScroll->viewport()->setAutoFillBackground(false);
    auto* treeBody = new QWidget(m_treeScroll);
    treeBody->setObjectName(QStringLiteral("cdScrollBody"));
    auto* treeLay = new QVBoxLayout(treeBody);
    treeLay->setContentsMargins(6, 6, 6, 14);
    treeLay->setSpacing(1);
    renderTree(treeLay);
    treeLay->addStretch();
    m_treeScroll->setWidget(treeBody);
    tp->addWidget(m_treeScroll, 1);
    grid->addWidget(treePane);

    // a página
    auto* pagePane = new QFrame(m_content);
    pagePane->setObjectName(QStringLiteral("cdPane"));
    auto* pp = new QVBoxLayout(pagePane);
    pp->setContentsMargins(0, 0, 0, 0);
    auto* pageScroll = new QScrollArea(pagePane);
    pageScroll->setObjectName(QStringLiteral("cdScroll"));
    pageScroll->setFrameShape(QFrame::NoFrame);
    pageScroll->setWidgetResizable(true);
    pageScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pageScroll->viewport()->setAutoFillBackground(false);
    auto* pageBody = new QWidget(pageScroll);
    pageBody->setObjectName(QStringLiteral("cdScrollBody"));
    auto* pg = new QVBoxLayout(pageBody);
    pg->setContentsMargins(20, 16, 20, 28);
    pg->setSpacing(18);
    if (m_cdDoc) {
        DocLink* d = m_cdDoc;
        QWidget* cr = crumbs(d->owner, true);
        cr->setParent(pageBody);
        pg->addWidget(cr);
        auto* tog = new QPushButton(tr("Ficha técnica"));
        tog->setObjectName(QStringLiteral("cdOpt"));
        tog->setProperty("on", m_ficha);
        tog->setToolTip(tr("Mostrar ou esconder a ficha técnica ao lado"));
        tog->setCursor(Qt::PointingHandCursor);
        tog->setFocusPolicy(Qt::NoFocus);
        connect(tog, &QPushButton::clicked, this, [this]() { m_ficha = !m_ficha; QTimer::singleShot(0, this, &Caderno::render); });
        QWidget* ed = m_panel->editorFor(d, [this]() { leaveDoc(); }, tog);
        ed->setParent(pageBody);
        pg->addWidget(ed, 1);
    } else renderPage(pg);
    pg->addStretch();
    pageScroll->setWidget(pageBody);
    pp->addWidget(pageScroll);
    grid->addWidget(pagePane, 1);

    if (side) {
        auto* fx = new QFrame(m_content);
        fx->setObjectName(QStringLiteral("cdPane"));
        fx->setFixedWidth(430);
        auto* fl = new QVBoxLayout(fx);
        fl->setContentsMargins(0, 0, 0, 0);
        auto* fs = new QScrollArea(fx);
        fs->setObjectName(QStringLiteral("cdScroll"));
        fs->setFrameShape(QFrame::NoFrame);
        fs->setWidgetResizable(true);
        fs->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        fs->viewport()->setAutoFillBackground(false);
        auto* fb = new QWidget(fs);
        fb->setObjectName(QStringLiteral("cdScrollBody"));
        auto* fbl = new QVBoxLayout(fb);
        fbl->setContentsMargins(16, 14, 16, 16);
        fbl->setSpacing(10);
        renderFicha(fbl, m_cdDoc->owner);
        fbl->addStretch();
        fs->setWidget(fb);
        fl->addWidget(fs);
        grid->addWidget(fx);
    }
    root->addLayout(grid, 1);
    QTimer::singleShot(0, this, [this, keepTree]() { if (m_treeScroll) m_treeScroll->verticalScrollBar()->setValue(keepTree); });
    if (hadQ) { m_qEdit->setFocus(); m_qEdit->setCursorPosition(pos); }
}
QWidget* Caderno::crumbs(const QString& k, bool withLast)
{
    auto* w = new QWidget();
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    QStringList p = path(k);
    if (!withLast && !p.isEmpty()) p.removeLast();
    for (const QString& x : p) {
        auto* b = new QPushButton(label(x), w);
        b->setObjectName(QStringLiteral("cdCrumb"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        connect(b, &QPushButton::clicked, this, [this, x]() { go(x); });
        h->addWidget(b);
        h->addWidget(lab(QStringLiteral("›"), QStringLiteral("cdSmall"), w));
    }
    h->addStretch();
    return w;
}
void Caderno::treeRow(QVBoxLayout* box, const QString& k, int depth, bool hasKids, QWidget* icon, const QString& sub, bool region, const std::function<void()>& kids, bool forceOpen)
{
    const bool open = m_open.contains(k) || forceOpen;
    auto* b = new QPushButton(box->parentWidget());
    b->setObjectName(QStringLiteral("cdRow"));
    b->setProperty("on", m_key == k);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setMinimumHeight(region ? 34 : 40);
    auto* h = new QHBoxLayout(b);
    h->setContentsMargins(6 + depth * 16, 3, 8, 3);
    h->setSpacing(7);
    auto* tw = new QToolButton(b);
    tw->setObjectName(QStringLiteral("cdTw"));
    tw->setFixedSize(14, 18);
    tw->setFocusPolicy(Qt::NoFocus);
    tw->setCursor(Qt::PointingHandCursor);
    tw->setText(hasKids ? (open ? QStringLiteral("▾") : QStringLiteral("▸")) : QString());
    if (hasKids) connect(tw, &QToolButton::clicked, this, [this, k]() { if (m_open.contains(k)) m_open.remove(k); else m_open.insert(k); QTimer::singleShot(0, this, &Caderno::render); });
    h->addWidget(tw);
    auto* ib = new QWidget(b);
    ib->setFixedSize(28, 28);
    ib->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* il = new QHBoxLayout(ib);
    il->setContentsMargins(0, 0, 0, 0);
    icon->setParent(ib);
    il->addWidget(icon, 0, Qt::AlignCenter);
    h->addWidget(ib);
    auto* col = new QVBoxLayout();
    col->setSpacing(0);
    const Ref o = k == QLatin1String("OUT") ? Ref() : m_doc->resolve(k);
    const bool named = k == QLatin1String("OUT") || k == QLatin1String("GAL") || k.startsWith(QLatin1Char('R')) || k.startsWith(QLatin1Char('G')) || m_doc->userNames.contains(k) || (o.kind == RefKind::System && o.sys->isCore);
    auto* name = lab(label(k), named ? QStringLiteral("cdName") : QStringLiteral("cdUnnamed"), b);
    name->setAttribute(Qt::WA_TransparentForMouseEvents);
    col->addWidget(name);
    if (!sub.isEmpty()) { auto* s = lab(sub, QStringLiteral("cdSmall"), b); s->setAttribute(Qt::WA_TransparentForMouseEvents); col->addWidget(s); }
    h->addLayout(col, 1);
    const int nd = k == QLatin1String("OUT") ? 0 : int(m_doc->docsOf(k).size());
    if (nd) { auto* bd = lab(QString::number(nd), QStringLiteral("cdBadge"), b); bd->setAttribute(Qt::WA_TransparentForMouseEvents); h->addWidget(bd); }
    connect(b, &QPushButton::clicked, this, [this, k]() { go(k); });
    box->addWidget(b);
    if (hasKids && open && kids) kids();
}
// A árvore: só desenha o que está aberto, então a galáxia inteira não pesa.
void Caderno::renderTree(QVBoxLayout* box)
{
    const QString q = m_q.trimmed().toLower();
    const double dpr = devicePixelRatioF();
    const auto ball = [&](const Body& b, int px) -> QWidget* { auto* l = new QLabel(); l->setAttribute(Qt::WA_TransparentForMouseEvents); l->setPixmap(QPixmap::fromImage(m_canvas->smallBall(b, px, V3 { -0.62, 0.3, 0.72 }, dpr))); return l; };
    const auto swatch = [](const QString& color) -> QWidget* { auto* l = new QLabel(); l->setFixedSize(10, 10); l->setStyleSheet(QStringLiteral("background: %1; border-radius: 3px;").arg(color)); return l; };
    std::function<void(const QString&, int)> groups;
    groups = [&](const QString& k, int d) {
        for (Group* g : m_doc->groupsOf(k)) {
            const QString gk = g->key;
            treeRow(box, gk, d, !m_doc->groupsOf(gk).empty(), m_panel->iconEl(gk, 24), g->kind == QLatin1String("Social") ? tr("Grupo social") : tr("Lugar"), false, [&, gk, d]() { groups(gk, d + 1); }, false);
        }
    };
    const auto builds = [&](System& s, int host, int d) {
        for (Build* b : m_doc->buildsIn(s)) if (b->host == host) {
            const QString bk = b->key;
            treeRow(box, bk, d, !m_doc->groupsOf(bk).empty(), m_panel->iconEl(bk, 24), catT(structOf(b->type)->name), false, [&, bk, d]() { groups(bk, d + 1); }, false);
        }
    };
    const auto sysRow = [&](System& s, int d) {
        const QString k = keyS(s);
        std::vector<Body*> ps;
        for (Body* p : planetsOf(s)) if (m_filter == QLatin1String("tudo") || pMatch(s, *p)) ps.push_back(p);
        bool hasBuilds = !m_doc->buildsIn(s).empty();
        treeRow(box, k, d, !ps.empty() || hasBuilds || !m_doc->groupsOf(k).empty(), new Sun(sysColor(s), 20, nullptr), coreT(s.stars[0].label.c_str()), false, [&, k, d, ps]() {
            for (Body* p : ps) {
                const QString pk = keyP(s, *p);
                const int uid = p->uid ? p->uid : p->idx;
                bool pb = false;
                for (Build* b : m_doc->buildsIn(s)) if (b->host == uid) pb = true;
                treeRow(box, pk, d + 1, !p->moons.empty() || pb || !m_doc->groupsOf(pk).empty(), ball(*p, cdPx(*p, 2.2, 18, 26)), coreT(typeLabel(p->type)), false, [&, p, pk, uid, d]() {
                    for (const auto& m : p->moons) {
                        const QString mk = keyM(s, *p, *m);
                        treeRow(box, mk, d + 2, !m_doc->groupsOf(mk).empty(), ball(*m, 16), coreT(typeLabel(m->type)), false, [&, mk, d]() { groups(mk, d + 3); }, false);
                    }
                    builds(s, uid, d + 2);
                    groups(pk, d + 2);
                }, false);
            }
            builds(s, -1, d + 1);
            groups(k, d + 1);
        }, false);
    };
    const auto regKids = [&](const QString& rk) {
        std::vector<System*> list;
        for (System* s : systemsOf(rk)) if (sysVisible(*s, q)) list.push_back(s);
        int more = 0;
        if (!q.isEmpty() && list.size() > 40) { more = int(list.size()) - 40; list.resize(40); }
        for (System* s : list) sysRow(*s, 2);
        if (more) box->addWidget(lab(tr("E mais %1. Continue digitando.").arg(more), QStringLiteral("cdSmall"), box->parentWidget()));
    };
    const auto regCount = [&](const QString& rk) { int n = 0; for (System* s : systemsOf(rk)) if (sysVisible(*s, q)) n++; return n; };
    auto* star = new QLabel(QStringLiteral("✦"));
    star->setObjectName(QStringLiteral("cdBadge"));
    treeRow(box, QStringLiteral("GAL"), 0, true, star, tr("%1 anos-luz").arg(nf(m_doc->gal->ly)), false, [&]() {
        if (q.isEmpty() && !m_doc->gal->systems.empty() && m_doc->gal->systems[0]->isCore) sysRow(*m_doc->gal->systems[0], 1);
        for (const auto& r : m_doc->regions) {
            const int n = regCount(r->key);
            if (!q.isEmpty() && !n) continue;
            const QString rk = r->key;
            treeRow(box, rk, 1, n > 0, swatch(r->color), QString(), true, [&, rk]() { regKids(rk); }, !q.isEmpty());
        }
        if (regCount(QStringLiteral("OUT")) > 0) treeRow(box, QStringLiteral("OUT"), 1, true, swatch(QStringLiteral("#56637a")), QString(), true, [&]() { regKids(QStringLiteral("OUT")); }, !q.isEmpty());
        groups(QStringLiteral("GAL"), 1);
    }, !q.isEmpty());
}
QWidget* Caderno::ringedBall(const Body& p, int px)
{
    auto* l = new QLabel();
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
    const double dpr = devicePixelRatioF();
    const QImage ball = m_canvas->smallBall(p, px, V3 { -0.62, 0.3, 0.72 }, dpr);
    if (!p.rings) { l->setPixmap(QPixmap::fromImage(ball)); return l; }
    const int S = int(std::ceil(px * p.rings->outer * 1.1));
    QImage out(int(S * dpr), int(S * dpr), QImage::Format_ARGB32_Premultiplied);
    out.setDevicePixelRatio(dpr);
    out.fill(Qt::transparent);
    QPainter g(&out);
    g.setRenderHint(QPainter::SmoothPixmapTransform);
    const double mid = S / 2.0, r = px / 2.0;
    m_canvas->ringOn(g, p, mid, mid, r, false, 0.34);
    g.drawImage(QRectF(mid - r, mid - r, r * 2, r * 2), ball);
    m_canvas->ringOn(g, p, mid, mid, r, true, 0.34);
    g.end();
    l->setPixmap(QPixmap::fromImage(out));
    return l;
}
// O palco no topo da página.
QWidget* Caderno::hero(const QString& k, const Ref& o)
{
    auto* sec = new QFrame();
    sec->setObjectName(QStringLiteral("cdHero"));
    const bool out = k == QLatin1String("OUT");
    auto* textW = new QWidget(sec);
    auto* text = new QVBoxLayout(textW);
    text->setContentsMargins(26, 22, 26, 22);
    text->setSpacing(10);
    if (k != QLatin1String("GAL")) { QWidget* c = crumbs(k, false); c->setParent(textW); text->addWidget(c); }
    const bool generated = !out && (o.kind == RefKind::System || o.kind == RefKind::Planet || o.kind == RefKind::Moon || o.kind == RefKind::Build || o.kind == RefKind::Comet || o.kind == RefKind::Belt) && !m_doc->userNames.contains(k) && !(o.kind == RefKind::System && o.sys->isCore);
    auto* nameBox = new QWidget(textW);
    auto* nb = new QVBoxLayout(nameBox);
    nb->setContentsMargins(0, 0, 0, 0);
    nb->addWidget(lab(label(k), generated ? QStringLiteral("cdH1Un") : QStringLiteral("cdH1"), nameBox, true));
    text->addWidget(nameBox);
    if (!out) {
        auto* sr = new QHBoxLayout();
        sr->setSpacing(10);
        QString subT = m_panel->subShort(k, o).toHtmlEscaped();
        if (o.kind == RefKind::Planet && o.body->hz == Hz::Dentro) subT += QStringLiteral(" · <span style=\"color:#6fd69a\">%1</span>").arg(tr("zona habitável"));
        auto* sub = lab(subT, QStringLiteral("cdSub"), textW, true);
        sub->setTextFormat(Qt::RichText);
        sr->addWidget(sub, 1);
        text->addLayout(sr);
        auto* lnk = new QHBoxLayout();
        lnk->setSpacing(12);
        const auto link = [&](const QString& t, const std::function<void()>& fn) {
            auto* b = new QPushButton(t, textW);
            b->setObjectName(QStringLiteral("cdLink"));
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            connect(b, &QPushButton::clicked, this, fn);
            lnk->addWidget(b);
        };
        link(o.kind == RefKind::Region || o.kind == RefKind::Group || m_doc->userNames.contains(k) ? tr("Renomear") : tr("Dar nome"), [this, nameBox, k]() { m_panel->startRename(nameBox, k); });
        link(tr("Ver no mapa"), [this, k]() { QTimer::singleShot(0, m_panel, [this, k]() { m_from = QStringLiteral("pan"); m_panel->closeCaderno(); m_canvas->select(k, true); }); });
        lnk->addStretch();
        text->addLayout(lnk);
        const auto facts = m_panel->quickFacts(k, o);
        if (!facts.empty()) { QWidget* fg = m_panel->factsGrid(facts); fg->setParent(textW); text->addWidget(fg); }
    }
    const double dpr = devicePixelRatioF();
    if (o.kind == RefKind::Planet || o.kind == RefKind::Moon || o.kind == RefKind::Build) {
        auto* h = new QHBoxLayout(sec);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(textW, 1);
        QWidget* art = o.kind == RefKind::Build ? static_cast<QWidget*>(new ModelView(m_canvas, o.build->type, o.build->seed, sec)) : new PlanetView(m_canvas, o.kind == RefKind::Moon ? o.moon : o.body, true, sec);
        art->setMinimumSize(300, 340);
        h->addWidget(art, 1);
    } else if (o.kind == RefKind::System && !o.sys->isCore && !planetsOf(*o.sys).empty()) {
        auto* v = new QVBoxLayout(sec);
        v->setContentsMargins(0, 0, 0, 0);
        v->addWidget(textW);
        auto* scroll = new QScrollArea(sec);
        scroll->setObjectName(QStringLiteral("cdScroll"));
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->viewport()->setAutoFillBackground(false);
        auto* row = new QWidget(scroll);
        row->setObjectName(QStringLiteral("cdScrollBody"));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 8, 18, 14);
        rl->setSpacing(6);
        rl->addWidget(new Sun(sysColor(*o.sys), 170, row, true));
        for (Body* p : planetsOf(*o.sys)) {
            const QString pk = keyP(*o.sys, *p);
            auto* b = new QPushButton(row);
            b->setObjectName(QStringLiteral("cdP"));
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            b->setFixedWidth(132);
            b->setMinimumHeight(170);
            auto* pv = new QVBoxLayout(b);
            pv->setContentsMargins(4, 6, 4, 6);
            pv->setSpacing(4);
            QWidget* bw = ringedBall(*p, cdPx(*p, p->rings ? 12 : 17, 36, p->rings ? 76 : 116));
            bw->setParent(b);
            pv->addWidget(bw, 1, Qt::AlignCenter);
            auto* nm = lab(m_doc->labelOf(pk), m_doc->userNames.contains(pk) ? QStringLiteral("cdName") : QStringLiteral("cdUnnamed"), b);
            nm->setAlignment(Qt::AlignCenter);
            nm->setAttribute(Qt::WA_TransparentForMouseEvents);
            pv->addWidget(nm);
            auto* tp = lab(coreT(typeLabel(p->type)), QStringLiteral("cdSmall"), b);
            tp->setAlignment(Qt::AlignCenter);
            tp->setAttribute(Qt::WA_TransparentForMouseEvents);
            pv->addWidget(tp);
            connect(b, &QPushButton::clicked, this, [this, pk]() { go(pk); });
            rl->addWidget(b);
        }
        rl->addStretch();
        scroll->setWidget(row);
        scroll->setWidgetResizable(true);
        scroll->setFixedHeight(200);
        v->addWidget(scroll);
    } else if (o.kind == RefKind::Galaxy || o.kind == RefKind::Region || out) {
        auto* v = new QVBoxLayout(sec);
        v->setContentsMargins(0, 0, 0, 0);
        v->addWidget(textW);
        auto* cardsW = new QWidget(sec);
        auto* cards = new QGridLayout(cardsW);
        cards->setContentsMargins(22, 0, 22, 22);
        cards->setSpacing(8);
        int i = 0;
        const int cols = 3;
        const auto card = [&](const QString& ck, QWidget* ic, const QString& title, bool named, QWidget* rowW) {
            auto* b = new QPushButton(cardsW);
            b->setObjectName(QStringLiteral("cdCard"));
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            b->setMinimumHeight(58);
            auto* h = new QHBoxLayout(b);
            h->setContentsMargins(10, 8, 10, 8);
            h->setSpacing(10);
            ic->setParent(b);
            h->addWidget(ic);
            auto* col = new QVBoxLayout();
            col->setSpacing(3);
            auto* nm = lab(title, named ? QStringLiteral("cdName") : QStringLiteral("cdUnnamed"), b);
            nm->setAttribute(Qt::WA_TransparentForMouseEvents);
            col->addWidget(nm);
            rowW->setParent(b);
            rowW->setAttribute(Qt::WA_TransparentForMouseEvents);
            col->addWidget(rowW);
            h->addLayout(col, 1);
            connect(b, &QPushButton::clicked, this, [this, ck]() { go(ck); });
            cards->addWidget(b, i / cols, i % cols);
            i++;
        };
        if (o.kind == RefKind::Galaxy) {
            const auto sw = [](const QString& c) -> QWidget* { auto* l = new QLabel(); l->setFixedSize(18, 18); l->setStyleSheet(QStringLiteral("background: %1; border-radius: 6px;").arg(c)); return l; };
            for (const auto& r : m_doc->regions) card(r->key, sw(r->color), r->name, true, lab(tr("%1 sistemas").arg(r->systems.size()), QStringLiteral("cdSmall"), nullptr));
            const int n = int(systemsOf(QStringLiteral("OUT")).size());
            if (n) card(QStringLiteral("OUT"), sw(QStringLiteral("#56637a")), tr("Fora das regiões"), true, lab(tr("%1 sistemas").arg(nf(n)), QStringLiteral("cdSmall"), nullptr));
        } else {
            const auto all = systemsOf(k);
            for (size_t j = 0; j < std::min<size_t>(60, all.size()); j++) {
                System* s = all[j];
                const QString sk = keyS(*s);
                auto* dots = new QWidget();
                auto* dl = new QHBoxLayout(dots);
                dl->setContentsMargins(0, 0, 0, 0);
                dl->setSpacing(2);
                for (Body* p : planetsOf(*s)) dl->addWidget(new Dot(typeColorOf(p->type), cdPx(*p, 1.8, 7, 14), isLiving(p->type), dots));
                dl->addStretch();
                card(sk, new Sun(sysColor(*s), 34, nullptr), m_doc->labelOf(sk), m_doc->userNames.contains(sk), dots);
            }
            if (all.size() > 60) { cards->addWidget(lab(tr("E mais %1 na árvore à esquerda.").arg(nf(double(all.size() - 60))), QStringLiteral("cdSmall"), cardsW), i / cols, i % cols); }
        }
        for (int c = 0; c < cols; c++) cards->setColumnStretch(c, 1);
        v->addWidget(cardsW);
    } else {
        auto* v = new QVBoxLayout(sec);
        v->setContentsMargins(0, 0, 0, 0);
        v->addWidget(textW);
    }
    Q_UNUSED(dpr);
    return sec;
}
QPushButton* Caderno::docCard(DocLink* d, const QString& from)
{
    auto* b = new QPushButton();
    b->setObjectName(QStringLiteral("cdDoc"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setMinimumHeight(92);
    auto* v = new QVBoxLayout(b);
    v->setContentsMargins(11, 9, 11, 9);
    v->setSpacing(4);
    auto* t = lab(d->title, QStringLiteral("cdName"), b);
    t->setAttribute(Qt::WA_TransparentForMouseEvents);
    v->addWidget(t);
    auto* s = lab(m_panel->docPlain(d).left(120), QStringLiteral("cdSmall"), b, true);
    s->setAttribute(Qt::WA_TransparentForMouseEvents);
    s->setMaximumHeight(48);
    v->addWidget(s);
    v->addStretch();
    const QString where = (from.isEmpty() ? QString() : tr("de %1 · ").arg(m_doc->labelOf(from))) + (d->gaveta ? tr("gaveta") : tr("Criador de Mundos"));
    auto* w = lab(where, QStringLiteral("cdSmall"), b);
    w->setAttribute(Qt::WA_TransparentForMouseEvents);
    v->addWidget(w);
    connect(b, &QPushButton::clicked, this, [this, d]() { openDoc(d); });
    return b;
}
void Caderno::renderPage(QVBoxLayout* pg)
{
    QWidget* parent = pg->parentWidget();
    const QString k = m_key;
    const bool out = k == QLatin1String("OUT");
    const Ref o = out ? Ref() : m_doc->resolve(k);
    QWidget* h = hero(k, o);
    h->setParent(parent);
    pg->addWidget(h);
    if (o) if (QWidget* hs = m_panel->habSection(k, o)) { hs->setParent(parent); pg->addWidget(hs); }
    const double dpr = devicePixelRatioF();
    // Minimapa do sistema.
    System* sys = nullptr;
    if (o.kind == RefKind::Planet || o.kind == RefKind::Moon || o.kind == RefKind::Group || o.kind == RefKind::Build) for (const QString& x : m_doc->pathOf(k)) { const Ref q = m_doc->resolve(x); if (q.kind == RefKind::System) { sys = q.sys; break; } }
    if (sys) {
        const QStringList p = m_doc->pathOf(k);
        auto* m = new QFrame(parent);
        m->setObjectName(QStringLiteral("cdMini"));
        auto* ml = new QHBoxLayout(m);
        ml->setContentsMargins(12, 8, 12, 8);
        ml->setSpacing(6);
        auto* sb = new QPushButton(m);
        sb->setObjectName(QStringLiteral("cdCrumb"));
        sb->setCursor(Qt::PointingHandCursor);
        sb->setFocusPolicy(Qt::NoFocus);
        sb->setText(m_doc->labelOf(keyS(*sys)));
        const QString sk = keyS(*sys);
        connect(sb, &QPushButton::clicked, this, [this, sk]() { go(sk); });
        ml->addWidget(new Sun(sysColor(*sys), 24, m));
        ml->addWidget(sb);
        for (Body* q : planetsOf(*sys)) {
            const QString pk = keyP(*sys, *q);
            const int px = cdPx(*q, 3.4, 14, 32);
            auto* b = new QPushButton(m);
            b->setObjectName(QStringLiteral("cdMm"));
            b->setProperty("on", p.contains(pk));
            b->setToolTip(m_doc->labelOf(pk));
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            b->setFixedSize(px + 12, px + 12);
            b->setStyleSheet(QStringLiteral("border-radius: %1px;").arg((px + 12) / 2));
            b->setIconSize(QSize(px, px));
            b->setIcon(QIcon(QPixmap::fromImage(m_canvas->smallBall(*q, px, V3 { -0.62, 0.3, 0.72 }, dpr))));
            connect(b, &QPushButton::clicked, this, [this, pk]() { go(pk); });
            ml->addWidget(b);
        }
        ml->addStretch();
        pg->addWidget(m);
    }
    // Aqui dentro.
    if (o && o.kind != RefKind::Galaxy && o.kind != RefKind::Region) {
        QStringList keys;
        if (o.kind == RefKind::System) {
            for (Build* b : m_doc->buildsIn(*o.sys)) if (b->host < 0) keys << b->key;
            for (Group* g : m_doc->groupsOf(k)) keys << g->key;
        } else keys = m_panel->insideOf(k, o).keys;
        auto* sec = new QWidget(parent);
        auto* sv = new QVBoxLayout(sec);
        sv->setContentsMargins(0, 0, 0, 0);
        sv->setSpacing(8);
        sv->addWidget(lab(tr("AQUI DENTRO"), QStringLiteral("cdEye"), sec));
        auto* g = new QGridLayout();
        g->setSpacing(6);
        int i = 0;
        const int cols = 3;
        for (const QString& x : keys) {
            auto* b = new QPushButton(sec);
            b->setObjectName(QStringLiteral("cdKid"));
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            b->setFixedHeight(34);
            auto* hl = new QHBoxLayout(b);
            hl->setContentsMargins(6, 2, 10, 2);
            hl->setSpacing(6);
            QWidget* ic = m_panel->iconEl(x, 22);
            ic->setParent(b);
            hl->addWidget(ic);
            auto* nm = lab(m_doc->labelOf(x), QStringLiteral("cdName"), b);
            nm->setAttribute(Qt::WA_TransparentForMouseEvents);
            hl->addWidget(nm, 1);
            connect(b, &QPushButton::clicked, this, [this, x]() { go(x); });
            g->addWidget(b, i / cols, i % cols);
            i++;
        }
        const auto add = [&](const QString& text, const QString& kind) {
            auto* b = new QPushButton(text, sec);
            b->setObjectName(QStringLiteral("cdKid"));
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            b->setFixedHeight(34);
            connect(b, &QPushButton::clicked, this, [this, b, sv, kind, k]() {
                auto* inp = new QLineEdit(b->parentWidget());
                inp->setObjectName(QStringLiteral("cdSearch"));
                inp->setPlaceholderText(kind == QLatin1String("Lugar") ? tr("Nome do lugar. Enter cria") : tr("Nome do grupo. Enter cria"));
                sv->addWidget(inp);
                inp->setFocus();
                connect(inp, &QLineEdit::editingFinished, this, [this, inp, kind, k]() {
                    if (!inp->isVisible()) return;
                    inp->hide();
                    if (!inp->text().trimmed().isEmpty()) { m_doc->newGroup(k, inp->text().trimmed(), kind); m_open.insert(k); m_panel->scheduleSave(); }
                    QTimer::singleShot(0, this, &Caderno::render);
                });
            });
            g->addWidget(b, i / cols, i % cols);
            i++;
        };
        add(tr("+ Lugar"), QStringLiteral("Lugar"));
        add(tr("+ Grupo social"), QStringLiteral("Social"));
        for (int c = 0; c < cols; c++) g->setColumnStretch(c, 1);
        sv->addLayout(g);
        pg->addWidget(sec);
    }
    if (!o) return;
    // Documentos.
    const auto own = m_doc->docsOf(k);
    auto* sec = new QWidget(parent);
    auto* sv = new QVBoxLayout(sec);
    sv->setContentsMargins(0, 0, 0, 0);
    sv->setSpacing(8);
    sv->addWidget(lab(QStringLiteral("%1  %2").arg(tr("DOCUMENTOS")).arg(own.size()), QStringLiteral("cdEye"), sec));
    auto* g = new QGridLayout();
    g->setSpacing(8);
    int i = 0;
    const int cols = 3;
    for (DocLink* d : own) { QPushButton* c = docCard(d, QString()); c->setParent(sec); g->addWidget(c, i / cols, i % cols); i++; }
    auto* nb = new QPushButton(tr("+ Novo documento"), sec);
    nb->setObjectName(QStringLiteral("cdDocNew"));
    nb->setCursor(Qt::PointingHandCursor);
    nb->setFocusPolicy(Qt::NoFocus);
    connect(nb, &QPushButton::clicked, this, [this, k]() { openDoc(m_panel->newDocFor(k)); });
    g->addWidget(nb, i / cols, i % cols);
    for (int c = 0; c < cols; c++) g->setColumnStretch(c, 1);
    sv->addLayout(g);
    pg->addWidget(sec);
    const auto foldSec = [&](const QString& title, const std::vector<std::pair<QString, std::vector<DocLink*>>>& list, bool open) {
        int n = 0;
        for (const auto& x : list) n += int(x.second.size());
        if (!n) return;
        auto* w = new QWidget(parent);
        auto* wv = new QVBoxLayout(w);
        wv->setContentsMargins(0, 0, 0, 0);
        wv->setSpacing(8);
        auto* head = new QToolButton(w);
        head->setObjectName(QStringLiteral("cartoLink"));
        head->setText(QStringLiteral("%1  %2").arg(title).arg(n));
        head->setCheckable(true);
        head->setChecked(open);
        head->setCursor(Qt::PointingHandCursor);
        head->setFocusPolicy(Qt::NoFocus);
        wv->addWidget(head, 0, Qt::AlignLeft);
        auto* body = new QWidget(w);
        auto* gg = new QGridLayout(body);
        gg->setContentsMargins(0, 0, 0, 0);
        gg->setSpacing(8);
        int j = 0;
        for (const auto& [from, ds] : list) for (DocLink* d : ds) { QPushButton* c = docCard(d, from); c->setParent(body); gg->addWidget(c, j / cols, j % cols); j++; }
        for (int c = 0; c < cols; c++) gg->setColumnStretch(c, 1);
        wv->addWidget(body);
        body->setVisible(open);
        connect(head, &QToolButton::toggled, body, &QWidget::setVisible);
        pg->addWidget(w);
    };
    if (!out) {
        if (o.kind == RefKind::Galaxy || o.kind == RefKind::Region || o.kind == RefKind::System) m_panel->renderLinksSection(pg, k);
        if (o.kind != RefKind::Group || o.group->kind != QLatin1String("Social")) m_panel->renderPeopleSection(pg, k);
    }
    foldSec(tr("Dos lugares daqui"), m_panel->docsBelow(k), true);
    foldSec(tr("Vindo de cima"), m_panel->docsAbove(k), false);
}
// A ficha técnica ao lado do editor: o lugar de que o documento fala.
void Caderno::renderFicha(QVBoxLayout* box, const QString& k)
{
    QWidget* parent = box->parentWidget();
    QString hk = QStringLiteral("GAL");
    QStringList p = path(k);
    std::reverse(p.begin(), p.end());
    for (const QString& x : p) {
        if (x == QLatin1String("OUT")) continue;
        const Ref q = m_doc->resolve(x);
        if (q.kind == RefKind::Planet || q.kind == RefKind::Moon || q.kind == RefKind::System || q.kind == RefKind::Region || q.kind == RefKind::Galaxy) { hk = x; break; }
    }
    const Ref o = m_doc->resolve(hk);
    if (hk != k) box->addWidget(lab(tr("%1 fica em %2").arg(m_doc->labelOf(k), m_doc->labelOf(hk)), QStringLiteral("cdSmall"), parent, true));
    if (hk != QLatin1String("GAL")) { QWidget* c = crumbs(hk, false); c->setParent(parent); box->addWidget(c); }
    box->addWidget(lab(m_doc->labelOf(hk), m_doc->userNames.contains(hk) || !(o.kind == RefKind::System || o.kind == RefKind::Planet || o.kind == RefKind::Moon) ? QStringLiteral("cdH2") : QStringLiteral("cdUnnamed"), parent, true));
    QString sub = m_panel->subShort(hk, o).toHtmlEscaped();
    if (o.kind == RefKind::Planet) { sub = coreT(typeLabel(o.body->type)).toHtmlEscaped(); if (o.body->hz == Hz::Dentro) sub += QStringLiteral(" · <span style=\"color:#6fd69a\">%1</span>").arg(tr("zona habitável")); }
    auto* subL = lab(sub, QStringLiteral("cdSub"), parent, true);
    subL->setTextFormat(Qt::RichText);
    box->addWidget(subL);
    auto facts = m_panel->quickFacts(hk, o);
    if (facts.size() > 4) facts.resize(4);
    QWidget* fg = m_panel->factsGrid(facts);
    fg->setParent(parent);
    box->addWidget(fg);
    const auto sec = [&](const QString& t, const std::vector<std::pair<QString, QString>>& rows) {
        box->addWidget(lab(t.toUpper(), QStringLiteral("cdEye"), parent));
        QWidget* s = m_panel->statsGrid(rows);
        s->setParent(parent);
        box->addWidget(s);
    };
    if (o.kind == RefKind::Planet || o.kind == RefKind::Moon) {
        const bool moon = o.kind == RefKind::Moon;
        const Body& b = moon ? *o.moon : *o.body;
        auto* art = new PlanetView(m_canvas, &b, true, parent);
        art->setFixedHeight(260);
        box->addWidget(art);
        std::vector<std::pair<QString, QString>> r;
        const auto add = [&](const QString& a, const QString& c) { r.push_back({ a, c }); };
        if (moon) {
            add(tr("Raio"), QStringLiteral("%1 km").arg(nf(b.radiusKm, 0)));
            add(tr("Gravidade"), QStringLiteral("%1 g").arg(nf(b.gravity, b.gravity < 0.1 ? 3 : 2)));
            if (b.temp != 0) add(tr("Temperatura média"), celsius(b.temp));
            if (!b.atm.empty() || b.air > 0) add(tr("Atmosfera"), atmText(b));
            sec(tr("Corpo"), r);
            r.clear();
            add(tr("Distância do planeta"), QStringLiteral("%1 km").arg(nf(std::round(b.dist / 100) * 100)));
            add(tr("Uma volta"), periodText(b.P));
            add(tr("Rotação"), tr("Travada: mostra sempre a mesma face"));
            sec(tr("Órbita"), r);
        } else {
            add(tr("Massa"), QStringLiteral("%1 M⊕").arg(sig(b.mass)));
            add(tr("Raio"), QStringLiteral("%1 km (%2 R⊕)").arg(nf(std::round(b.radius * 6371 / 10) * 10), sig(b.radius)));
            if (b.people != 0) add(tr("População"), fmtCount(b.people));
            add(tr("Gravidade"), QStringLiteral("%1 g").arg(nf(b.gravity, 2)));
            add(tr("Densidade"), QStringLiteral("%1 g/cm³").arg(nf(b.density, 1)));
            add(tr("Temperatura média"), celsius(b.temp));
            add(tr("Atmosfera"), atmText(b));
            if (b.look.vegKeyV != Veg::None) add(tr("Vegetação"), coreT(vegLabel(b.look.vegKeyV)));
            sec(tr("Corpo"), r);
            r.clear();
            add(tr("Distância da estrela"), QStringLiteral("%1 UA").arg(sig(b.a)));
            add(tr("Excentricidade"), nf(b.e, 3));
            add(tr("Ano"), periodText(b.P));
            add(tr("Dia"), b.locked ? tr("Igual ao ano") : periodText(b.dayH / 24));
            add(tr("Inclinação do eixo"), QStringLiteral("%1°").arg(nf(b.tilt, 0)));
            sec(tr("Órbita e rotação"), r);
        }
        box->addWidget(lab(tr("Os números saem da física da galáxia. Pra mudar à mão, use a ferramenta Editar."), QStringLiteral("cdSmall"), parent, true));
    } else if (o.kind == RefKind::System) {
        const System& s = *o.sys;
        const Star& st = s.stars[0];
        auto* art = new QHBoxLayout();
        art->setSpacing(4);
        art->addWidget(new Sun(sysColor(s), 90, parent));
        for (Body* q : planetsOf(s)) art->addWidget(new Dot(typeColorOf(q->type), cdPx(*q, 3, 10, 28), isLiving(q->type), parent));
        art->addStretch();
        box->addLayout(art);
        std::vector<std::pair<QString, QString>> r;
        r.push_back({ tr("Espectro"), !st.spec.empty() ? QString::fromStdString(st.spec) : QStringLiteral("—") });
        if (st.temp != 0) r.push_back({ tr("Temperatura"), QStringLiteral("%1 K").arg(nf(std::round(st.temp))) });
        r.push_back({ tr("Massa"), QStringLiteral("%1 M☉").arg(sig(st.mass)) });
        sec(tr("Estrela"), r);
        r.clear();
        if (s.hasHz) r.push_back({ tr("Zona habitável"), tr("%1 a %2 UA").arg(sig(s.hz[0]), sig(s.hz[1])) });
        r.push_back({ tr("Planetas"), QString::number(planetsOf(s).size()) });
        r.push_back({ tr("Mundos vivos"), livingCount(s) ? QString::number(livingCount(s)) : tr("Nenhum") });
        sec(tr("Zonas"), r);
    } else if (o.kind == RefKind::Region) {
        auto* art = new QHBoxLayout();
        art->setSpacing(2);
        int n = 0;
        for (int id : o.region->systems) { if (n++ >= 12) break; art->addWidget(new Sun(sysColor(*m_doc->gal->systems[size_t(id)]), 26, parent)); }
        art->addStretch();
        box->addLayout(art);
    }
}

} // namespace galaxy
