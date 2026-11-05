// Explorar: a busca, o painel com abas (Visão geral · Documentos · Aqui dentro · Dados) e o cartão minimizado.

#include "GalaxyPanel.h"
#include "GalaxyHab.h"
#include "GalaxyBlackHole.h"
#include "../TerritorioStore.h"
#include "GalaxyCaderno.h"
#include "GalaxyWidgets.h"

#include "../ProjectModel.h"
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
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace galaxy {

namespace {
inline QString catT(const char* s) { return QCoreApplication::translate("galaxy::Catalog", s); }
QString fold(const QString& t)
{
    QString out;
    const QString n = t.normalized(QString::NormalizationForm_D);
    for (QChar c : n) if (c.category() != QChar::Mark_NonSpacing) out += c.toLower();
    return out;
}
// o sol borrado, como nos cartões
class StarDot : public QWidget {
public:
    StarDot(const std::array<int, 3>& c, int px, QWidget* parent, int offsetX = 0) : QWidget(parent), m_c(c), m_off(offsetX) { setFixedSize(px, px); setAttribute(Qt::WA_TransparentForMouseEvents); }
    // Em buraco negro, o retrato é o próprio buraco negro.
    StarDot(const System& s, int px, QWidget* parent, int offsetX = 0) : StarDot(sysColor(s), px, parent, offsetX)
    {
        if (s.isCore || s.stars[0].kind == StarKind::BuracoNegro) { m_hole = true; blackHoleAxis(s, m_a, m_b); }
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double R = height() / 2.0, cx = R + m_off;
        if (m_hole) { paintBlackHole(p, QPointF(cx, R), R / BLACK_HOLE_EXTENT, m_a, m_b, 0, 0, 40); return; }
        QRadialGradient g(cx, R, R);
        g.setColorAt(0, Qt::white); g.setColorAt(0.18, Qt::white);
        g.setColorAt(0.42, QColor(m_c[0], m_c[1], m_c[2]));
        g.setColorAt(0.72, QColor(m_c[0], m_c[1], m_c[2], 0));
        p.setPen(Qt::NoPen); p.setBrush(g);
        p.drawEllipse(QPointF(cx, R), R, R);
    }
private:
    std::array<int, 3> m_c;
    int m_off;
    bool m_hole = false;
    double m_a = 0, m_b = 0;
};
class Swatch : public QWidget {
public:
    Swatch(const QColor& c, QWidget* parent, bool wash = false) : QWidget(parent), m_c(c), m_wash(wash) { setAttribute(Qt::WA_TransparentForMouseEvents); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (m_wash) {
            QRadialGradient g(width() * 0.3, height() * 0.6, width() * 0.7);
            QColor a = m_c; a.setAlphaF(0.35);
            QColor b = m_c; b.setAlpha(0);
            g.setColorAt(0, a); g.setColorAt(1, b);
            p.fillRect(rect(), g);
        } else { p.setPen(Qt::NoPen); p.setBrush(m_c); p.drawEllipse(rect().adjusted(1, 1, -1, -1)); }
    }
private:
    QColor m_c;
    bool m_wash;
};
// a galáxia na foto do painel
class GalaxyPhoto : public QWidget {
public:
    GalaxyPhoto(const QImage& img, QWidget* parent) : QWidget(parent), m_img(img) { setAttribute(Qt::WA_TransparentForMouseEvents); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.fillRect(rect(), QColor(3, 5, 10));
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.drawImage(QRectF(-60, -175, 520, 520), m_img);
    }
private:
    QImage m_img;
};
QLabel* textLabel(const QString& text, const QString& obj, QWidget* parent, bool wrap = false)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(obj);
    l->setWordWrap(wrap);
    return l;
}
// Um rótulo que corta com reticências em vez de empurrar o painel.
class Elide : public QLabel {
public:
    Elide(const QString& text, const QString& obj, QWidget* parent) : QLabel(parent), m_text(text) { setObjectName(obj); setMinimumWidth(0); setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); setToolTip(text); }
protected:
    void resizeEvent(QResizeEvent* e) override { QLabel::resizeEvent(e); setText(fontMetrics().elidedText(m_text, Qt::ElideRight, width())); }
private:
    QString m_text;
};
}

void Panel::buildSide()
{
    m_side = new QWidget(this);
    auto* l = new QVBoxLayout(m_side);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(10);
    m_searchBox = new QFrame(m_side);
    m_searchBox->setObjectName(QStringLiteral("gmSearch"));
    m_searchBox->setFixedHeight(46);
    auto* sl = new QHBoxLayout(m_searchBox);
    sl->setContentsMargins(14, 0, 12, 0);
    sl->setSpacing(10);
    auto* ic = new QLabel(m_searchBox);
    ic->setPixmap(carto::iconPixmap(QStringLiteral("galSearch"), Theme::toColor(Theme::textMuted()), 18));
    sl->addWidget(ic);
    m_query = new QLineEdit(m_searchBox);
    m_query->setObjectName(QStringLiteral("gmQuery"));
    m_query->setPlaceholderText(tr("Buscar sistema, planeta, lua ou lugar"));
    m_query->installEventFilter(this);
    connect(m_query, &QLineEdit::textEdited, this, &Panel::search);
    connect(m_query, &QLineEdit::returnPressed, this, [this]() { if (m_resIdx >= 0 && m_resIdx < m_resList.size()) chooseResult(m_resList[m_resIdx]); });
    sl->addWidget(m_query, 1);
    l->addWidget(m_searchBox);
    m_panelBox = new QFrame(m_side);
    m_panelBox->setObjectName(QStringLiteral("gmPanel"));
    auto* pl = new QVBoxLayout(m_panelBox);
    pl->setContentsMargins(0, 0, 0, 0);
    m_pscroll = new QScrollArea(m_panelBox);
    m_pscroll->setObjectName(QStringLiteral("gmScroll"));
    m_pscroll->setFrameShape(QFrame::NoFrame);
    m_pscroll->setWidgetResizable(true);
    m_pscroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pscroll->viewport()->setAutoFillBackground(false);
    pl->addWidget(m_pscroll);
    l->addWidget(m_panelBox, 1);
    m_side->hide();

    m_results = new QFrame(this);
    m_results->setObjectName(QStringLiteral("gmResults"));
    new QVBoxLayout(m_results);
    m_results->layout()->setContentsMargins(4, 4, 4, 4);
    m_results->layout()->setSpacing(0);
    m_results->hide();

    m_mini = new QFrame(this);
    m_mini->setObjectName(QStringLiteral("gmMini"));
    m_mini->setFixedWidth(400);
    m_mini->hide();
}
void Panel::openPanel() { m_panelOpen = true; renderPanel(); }
void Panel::closePanel() { m_panelOpen = false; if (m_openDoc) docDropIfEmpty(m_openDoc); m_openDoc = nullptr; m_panelMini = false; renderPanel(); }
void Panel::selected(const QString& key)
{
    Q_UNUSED(key);
    m_tip->hide();
    m_peopleFor.clear();
    if (!m_linkFrom.isEmpty()) { m_linkFrom.clear(); m_canvas->pickingLink = false; }
    if (m_openDoc) { docDropIfEmpty(m_openDoc); m_openDoc = nullptr; }
    syncSelPill(false);
    if (m_tool == QLatin1String("realm")) renderRealmDock();
    renderPanel();
    syncTitle();
    if (m_caderno && m_caderno->isVisible()) syncUi();
}

// ── o que cada lugar tem ──
QString Panel::subShort(const QString& k, const Ref& o)
{
    Q_UNUSED(k);
    switch (o.kind) {
    case RefKind::Galaxy: return tr("%1 sistemas").arg(nf(double(m_doc.gal->systems.size())));
    case RefKind::Region: return tr("Região · %1 sistemas").arg(o.region->systems.size());
    case RefKind::System: {
        if (o.sys->isCore) return tr("Buraco negro supermassivo");
        const Region* r = m_doc.regionOfSys(*o.sys);
        return QCoreApplication::translate("galaxy::Core", o.sys->stars[0].label.c_str()) + (r ? QStringLiteral(" · ") + r->name : QString());
    }
    case RefKind::Planet: return QCoreApplication::translate("galaxy::Core", typeLabel(o.body->type)) + (o.body->look.style != LookStyle::None ? QStringLiteral(" · ") + QCoreApplication::translate("galaxy::Core", lookStyleLabel(o.body->look.style)).toLower() : QString()) + QStringLiteral(" · ") + m_doc.labelOf(keyS(*o.sys));
    case RefKind::Moon: return tr("%1 · lua de %2").arg(QCoreApplication::translate("galaxy::Core", typeLabel(o.moon->type)), m_doc.labelOf(keyP(*o.sys, *o.body)));
    case RefKind::Build: return catT(structOf(o.build->type)->name) + (o.host ? tr(" · em volta de %1").arg(m_doc.labelOf(keyP(*o.sys, *o.host))) : o.sys ? QStringLiteral(" · ") + m_doc.labelOf(keyS(*o.sys)) : tr(" · no espaço aberto"));
    case RefKind::Comet: return tr("Cometa · %1").arg(m_doc.labelOf(keyS(*o.sys)));
    case RefKind::Belt: return (o.belt->kuiper ? tr("Disco gelado") : tr("Cinturão de asteroides")) + QStringLiteral(" · ") + m_doc.labelOf(keyS(*o.sys));
    case RefKind::Group: return (o.group->kind == QLatin1String("Social") ? tr("Grupo social") : tr("Lugar")) + tr(" em %1").arg(m_doc.labelOf(o.group->parent));
    default: return QString();
    }
}
Panel::Inside Panel::insideOf(const QString& k, const Ref& o)
{
    Inside in;
    in.label = tr("Aqui dentro");
    const auto groups = [&]() { for (Group* g : m_doc.groupsOf(k)) in.keys << g->key; };
    if (o.kind == RefKind::Galaxy) { in.label = tr("Regiões"); for (const auto& r : m_doc.regions) in.keys << r->key; return in; }
    if (o.kind == RefKind::Region) { in.label = tr("Sistemas"); QList<int> ids = o.region->systems.values(); std::sort(ids.begin(), ids.end()); for (int id : ids) in.keys << keyS(*m_doc.gal->systems[size_t(id)]); return in; }
    if (o.kind == RefKind::System) {
        QStringList built, extra;
        for (Build* b : m_doc.buildsIn(*o.sys)) if (b->host < 0) built << b->key;
        if (m_tool == QLatin1String("edit")) {
            for (const auto& c : o.sys->bodies) if (c->body == BodyKind::Cometa) extra << QStringLiteral("C%1.%2").arg(o.sys->id).arg(c->idx);
            for (size_t i = 0; i < o.sys->belts.size(); i++) extra << QStringLiteral("K%1.%2").arg(o.sys->id).arg(o.sys->belts[i].uid ? o.sys->belts[i].uid : int(i) + 1);
        }
        in.label = built.isEmpty() && extra.isEmpty() ? tr("Planetas") : tr("Aqui dentro");
        for (Body* p : planetsOf(*o.sys)) in.keys << keyP(*o.sys, *p);
        in.keys << built << extra;
        groups();
        return in;
    }
    if (o.kind == RefKind::Planet) {
        for (const auto& m : o.body->moons) in.keys << keyM(*o.sys, *o.body, *m);
        const int uid = o.body->uid ? o.body->uid : o.body->idx;
        for (Build* b : m_doc.buildsIn(*o.sys)) if (b->host == uid) in.keys << b->key;
    }
    groups();
    return in;
}
std::vector<std::pair<QString, QString>> Panel::quickFacts(const QString&, const Ref& o)
{
    std::vector<std::pair<QString, QString>> r;
    const auto add = [&](const QString& a, const QString& b) { r.push_back({ a, b }); };
    if (o.kind == RefKind::Galaxy) {
        int planets = 0, living = 0;
        for (const auto& s : m_doc.gal->systems) { planets += int(planetsOf(*s).size()); living += livingCount(*s); }
        add(tr("Diâmetro"), tr("%1 a.l.").arg(nf(m_doc.gal->ly)));
        add(tr("Sistemas"), nf(double(m_doc.gal->systems.size())));
        add(tr("Planetas"), nf(planets));
        add(tr("Mundos vivos"), nf(living));
    } else if (o.kind == RefKind::Region) {
        int planets = 0, living = 0;
        for (int id : o.region->systems) { const System& s = *m_doc.gal->systems[size_t(id)]; planets += int(planetsOf(s).size()); living += livingCount(s); }
        add(tr("Sistemas"), QString::number(o.region->systems.size()));
        add(tr("Planetas"), QString::number(planets));
        add(tr("Mundos vivos"), QString::number(living));
    } else if (o.kind == RefKind::System) {
        const System& s = *o.sys;
        const int np = int(planetsOf(s).size()), nl = livingCount(s);
        add(tr("Espectro"), !s.stars[0].spec.empty() ? QString::fromStdString(s.stars[0].spec) : QStringLiteral("—"));
        add(tr("Planetas"), np ? QString::number(np) : tr("Nenhum"));
        add(tr("Mundos vivos"), nl ? QString::number(nl) : tr("Nenhum"));
        if (s.hasHz) add(tr("Zona habitável"), QStringLiteral("%1–%2 UA").arg(sig(s.hz[0]), sig(s.hz[1])));
    } else if (o.kind == RefKind::Planet) {
        const Body& p = *o.body;
        if (p.people != 0) add(tr("População"), fmtCount(p.people));
        add(tr("Temperatura"), celsius(p.temp));
        add(tr("Gravidade"), QStringLiteral("%1 g").arg(nf(p.gravity, 2)));
        add(tr("Ano"), periodText(p.P));
        add(tr("Dia"), p.locked ? tr("Igual ao ano") : periodText(p.dayH / 24));
    } else if (o.kind == RefKind::Moon) {
        const Body& m = *o.moon;
        add(tr("Temperatura"), m.temp != 0 ? celsius(m.temp) : QStringLiteral("—"));
        add(tr("Gravidade"), QStringLiteral("%1 g").arg(nf(m.gravity, m.gravity < 0.1 ? 3 : 2)));
        add(tr("Uma volta"), periodText(m.P));
        add(tr("Raio"), QStringLiteral("%1 km").arg(nf(m.radiusKm, 0)));
    } else if (o.kind == RefKind::Build) {
        add(tr("Tipo"), catT(structOf(o.build->type)->name));
        add(tr("Tamanho"), QStringLiteral("%1%").arg(qRound(o.build->scale * 100)));
        add(tr("Onde"), o.host ? m_doc.labelOf(keyP(*o.sys, *o.host)) : o.sys ? m_doc.labelOf(keyS(*o.sys)) : tr("Espaço aberto"));
    } else if (o.kind == RefKind::Comet) {
        add(tr("Órbita"), QStringLiteral("%1 UA").arg(sig(o.comet->a)));
        add(tr("Volta"), periodText(o.comet->P));
    } else if (o.kind == RefKind::Belt) {
        add(tr("De"), QStringLiteral("%1 UA").arg(sig(o.belt->a0)));
        add(tr("Até"), QStringLiteral("%1 UA").arg(sig(o.belt->a1)));
    }
    return r;
}
// Documentos dos lugares de dentro (luas, cidades, grupos...), por lugar.
std::vector<std::pair<QString, std::vector<DocLink*>>> Panel::docsBelow(const QString& k)
{
    std::vector<std::pair<QString, std::vector<DocLink*>>> out;
    QSet<QString> seen;
    const std::function<void(const QString&, int)> walk = [&](const QString& x, int depth) {
        const Ref o = m_doc.resolve(x);
        for (const QString& c : insideOf(x, o).keys) {
            if (seen.contains(c)) continue;
            seen.insert(c);
            const auto ds = m_doc.docsOf(c);
            if (!ds.empty()) out.push_back({ c, ds });
            if (depth < 3) walk(c, depth + 1);
        }
    };
    if (m_doc.resolve(k).kind != RefKind::Galaxy) walk(k, 0);
    return out;
}
std::vector<std::pair<QString, std::vector<DocLink*>>> Panel::docsAbove(const QString& k)
{
    std::vector<std::pair<QString, std::vector<DocLink*>>> out;
    QStringList path = m_doc.pathOf(k);
    if (!path.isEmpty()) path.removeLast();
    std::reverse(path.begin(), path.end());
    for (const QString& a : path) { const auto ds = m_doc.docsOf(a); if (!ds.empty()) out.push_back({ a, ds }); }
    return out;
}
QStringList Panel::siblingsOf(const QString& k)
{
    const Ref o = m_doc.resolve(k);
    QStringList out;
    switch (o.kind) {
    case RefKind::Planet: for (Body* p : planetsOf(*o.sys)) out << keyP(*o.sys, *p); return out;
    case RefKind::Moon: for (const auto& m : o.body->moons) out << keyM(*o.sys, *o.body, *m); return out;
    case RefKind::System: { const Region* r = m_doc.regionOfSys(*o.sys); if (!r) return { k }; QList<int> ids = r->systems.values(); std::sort(ids.begin(), ids.end()); for (int id : ids) out << keyS(*m_doc.gal->systems[size_t(id)]); return out; }
    case RefKind::Region: for (const auto& r : m_doc.regions) out << r->key; return out;
    case RefKind::Build:
        if (o.sys) { for (Build* b : m_doc.buildsIn(*o.sys)) if (b->host == o.build->host) out << b->key; }
        else for (const auto& b : m_doc.builds) if (b->deep) out << b->key;
        return out;
    case RefKind::Comet: for (const auto& c : o.sys->bodies) if (c->body == BodyKind::Cometa) out << QStringLiteral("C%1.%2").arg(o.sys->id).arg(c->idx); return out;
    case RefKind::Belt: for (size_t i = 0; i < o.sys->belts.size(); i++) out << QStringLiteral("K%1.%2").arg(o.sys->id).arg(o.sys->belts[i].uid ? o.sys->belts[i].uid : int(i) + 1); return out;
    case RefKind::Group: for (Group* g : m_doc.groupsOf(o.group->parent)) out << g->key; return out;
    default: return { k };
    }
}

// ── pedaços ──
QWidget* Panel::iconEl(const QString& k, int px)
{
    const Ref o = m_doc.resolve(k);
    auto* l = new QLabel();
    l->setFixedSize(px, px);
    l->setAlignment(Qt::AlignCenter);
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
    const QColor muted = Theme::toColor(Theme::textMuted());
    switch (o.kind) {
    case RefKind::Planet: l->setPixmap(QPixmap::fromImage(m_canvas->smallBall(*o.body, px, V3 { -0.62, 0.3, 0.72 }, devicePixelRatioF()))); return l;
    case RefKind::Moon: l->setPixmap(QPixmap::fromImage(m_canvas->smallBall(*o.moon, std::max(16, px * 3 / 4), V3 { -0.62, 0.3, 0.72 }, devicePixelRatioF()))); return l;
    case RefKind::System: delete l; return new StarDot(*o.sys, px * 4 / 5, nullptr);
    case RefKind::Build: l->setPixmap(modelArt(o.build->type, o.build->seed, px)); return l;
    case RefKind::Comet: { CatalogItem it { "cometa", ItemKind::Comet, "", false, false }; l->setPixmap(itemArt(it, px)); return l; }
    case RefKind::Belt: { CatalogItem it { o.belt->kuiper ? "kuiper" : "asteroides", ItemKind::Belt, "", false, false }; l->setPixmap(itemArt(it, px)); return l; }
    case RefKind::Region: { delete l; auto* s = new Swatch(QColor(o.region->color), nullptr); s->setFixedSize(14, 14); return s; }
    case RefKind::Group: l->setPixmap(carto::iconPixmap(o.group->kind == QLatin1String("Social") ? QStringLiteral("galSocial") : QStringLiteral("galPlace"), muted, 16)); return l;
    default: return l;
    }
}
QWidget* Panel::factsGrid(const std::vector<std::pair<QString, QString>>& rows)
{
    auto* w = new QWidget();
    auto* g = new QGridLayout(w);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(14);
    g->setVerticalSpacing(10);
    int i = 0;
    for (const auto& [a, b] : rows) {
        auto* box = new QVBoxLayout();
        box->setSpacing(2);
        box->addWidget(textLabel(a, QStringLiteral("gmFactK"), w));
        box->addWidget(textLabel(b, QStringLiteral("gmFactV"), w));
        g->addLayout(box, i / 3, i % 3);
        i++;
    }
    for (int c = 0; c < 3; c++) g->setColumnStretch(c, 1);
    return w;
}
QWidget* Panel::statsGrid(const std::vector<std::pair<QString, QString>>& rows)
{
    auto* w = new QWidget();
    auto* g = new QGridLayout(w);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(14);
    g->setVerticalSpacing(5);
    int i = 0;
    for (const auto& [a, b] : rows) {
        g->addWidget(textLabel(a, QStringLiteral("gmStatK"), w), i, 0);
        auto* v = textLabel(b, QStringLiteral("gmStatV"), w);
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        g->addWidget(v, i, 1);
        i++;
    }
    g->setColumnStretch(1, 1);
    return w;
}
QWidget* Panel::physSection(const Ref& o, bool flat)
{
    const bool moon = o.kind == RefKind::Moon;
    const Body& b = moon ? *o.moon : *o.body;
    auto* w = new QWidget();
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);
    auto* body = new QWidget(w);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(8);
    auto* chips = new QHBoxLayout();
    chips->setSpacing(4);
    const auto chip = [&](const QString& t, bool hz) { chips->addWidget(textLabel(t, hz ? QStringLiteral("gmTagHz") : QStringLiteral("gmTag"), body)); };
    if (moon) { if (isLiving(b.type)) chip(tr("Habitável"), true); chip(tr("Travada por maré"), false); }
    else {
        if (b.hz == Hz::Dentro) chip(tr("Zona habitável"), true);
        if (b.look.style != LookStyle::None) chip(QCoreApplication::translate("galaxy::Core", lookStyleLabel(b.look.style)), false);
        if (b.locked) chip(tr("Travado por maré"), false);
        if (b.water) chip(tr("Água líquida possível"), true);
        if (b.rings) chip(tr("Anéis"), false);
    }
    chips->addStretch();
    bl->addLayout(chips);
    std::vector<std::pair<QString, QString>> rows;
    const auto add = [&](const QString& a, const QString& c) { rows.push_back({ a, c }); };
    if (moon) {
        add(tr("Raio"), QStringLiteral("%1 km").arg(nf(b.radiusKm, 0)));
        add(tr("Gravidade"), QStringLiteral("%1 g").arg(nf(b.gravity, b.gravity < 0.1 ? 3 : 2)));
        if (b.temp != 0) add(tr("Temperatura média"), celsius(b.temp));
        if (!b.atm.empty() || b.air > 0) add(tr("Atmosfera"), atmText(b));
        add(tr("Distância do planeta"), QStringLiteral("%1 km").arg(nf(std::round(b.dist / 100) * 100)));
        add(tr("Uma volta"), periodText(b.P));
    } else {
        add(tr("Massa"), QStringLiteral("%1 M⊕").arg(sig(b.mass)));
        add(tr("Raio"), QStringLiteral("%1 km (%2 R⊕)").arg(nf(std::round(b.radius * 6371 / 10) * 10), sig(b.radius)));
        if (b.people != 0) add(tr("População"), fmtCount(b.people));
        add(tr("Gravidade"), QStringLiteral("%1 g").arg(nf(b.gravity, 2)));
        add(tr("Densidade"), QStringLiteral("%1 g/cm³").arg(nf(b.density, 1)));
        add(tr("Temperatura média"), celsius(b.temp));
        add(tr("Atmosfera"), atmText(b));
        if (b.look.vegKeyV != Veg::None) add(tr("Vegetação"), QCoreApplication::translate("galaxy::Core", vegLabel(b.look.vegKeyV)));
        add(tr("Distância da estrela"), QStringLiteral("%1 UA").arg(sig(b.a)));
        add(tr("Ano"), periodText(b.P));
        add(tr("Dia"), b.locked ? tr("Igual ao ano") : periodText(b.dayH / 24));
        add(tr("Inclinação do eixo"), QStringLiteral("%1°").arg(nf(b.tilt, 0)));
    }
    bl->addWidget(statsGrid(rows));
    if (!flat) {
        auto* head = new QToolButton(w);
        head->setObjectName(QStringLiteral("cartoLink"));
        head->setText(tr("Dados físicos"));
        head->setCheckable(true);
        head->setCursor(Qt::PointingHandCursor);
        head->setFocusPolicy(Qt::NoFocus);
        v->addWidget(head, 0, Qt::AlignLeft);
        body->hide();
        connect(head, &QToolButton::toggled, body, &QWidget::setVisible);
    }
    v->addWidget(body);
    return w;
}
QPushButton* Panel::itemRow(const QString& k, const QString& sub, const QString& val)
{
    auto* b = new QPushButton();
    b->setObjectName(QStringLiteral("gmItem"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setFixedHeight(46);
    auto* h = new QHBoxLayout(b);
    h->setContentsMargins(8, 4, 8, 4);
    h->setSpacing(10);
    auto* holder = new QWidget(b);
    holder->setFixedSize(34, 34);
    holder->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* hl = new QHBoxLayout(holder);
    hl->setContentsMargins(0, 0, 0, 0);
    QWidget* ic = iconEl(k, 30);
    ic->setParent(holder);
    hl->addWidget(ic, 0, Qt::AlignCenter);
    h->addWidget(holder);
    auto* col = new QVBoxLayout();
    col->setSpacing(0);
    auto* name = new Elide(m_doc.labelOf(k), QStringLiteral("cartoValue"), b);
    name->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* s = new Elide(sub, QStringLiteral("cartoNote"), b);
    s->setAttribute(Qt::WA_TransparentForMouseEvents);
    col->addWidget(name); col->addWidget(s);
    h->addLayout(col, 1);
    if (!val.isEmpty()) { auto* v = textLabel(val, QStringLiteral("cartoNote"), b); v->setAttribute(Qt::WA_TransparentForMouseEvents); h->addWidget(v); }
    connect(b, &QPushButton::clicked, this, [this, k]() { QTimer::singleShot(0, this, [this, k]() { m_canvas->select(k, true); }); });
    return b;
}
QPushButton* Panel::docCard(DocLink* d)
{
    auto* b = new QPushButton();
    b->setObjectName(QStringLiteral("gmDoc"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setMinimumHeight(80);
    auto* v = new QVBoxLayout(b);
    v->setContentsMargins(10, 8, 10, 8);
    v->setSpacing(3);
    auto* t = new Elide(d->title, QStringLiteral("cartoValue"), b);
    t->setAttribute(Qt::WA_TransparentForMouseEvents);
    v->addWidget(t);
    const QString plain = docPlain(d);
    auto* s = textLabel(plain.isEmpty() ? tr("Vazio") : plain.left(90), QStringLiteral("cartoNote"), b, true);
    s->setAttribute(Qt::WA_TransparentForMouseEvents);
    s->setMaximumHeight(46);
    v->addWidget(s);
    v->addStretch();
    QString where = d->gaveta ? d->drawer : tr("Criador de Mundos");
    if (d->gaveta && m_link.model) for (const Drawer& dr : m_link.model->drawers()) if (dr.key == d->drawer) where = tr("Gaveta %1").arg(dr.title);
    auto* w = new Elide(where, QStringLiteral("cartoNote"), b);
    w->setAttribute(Qt::WA_TransparentForMouseEvents);
    v->addWidget(w);
    connect(b, &QPushButton::clicked, this, [this, d]() { QTimer::singleShot(0, this, [this, d]() { m_openDoc = d; renderPanel(); syncUi(); }); });
    return b;
}
QWidget* Panel::docGrid(const std::vector<DocLink*>& ds, const QString& withNew)
{
    auto* w = new QWidget();
    auto* g = new QGridLayout(w);
    g->setContentsMargins(0, 0, 0, 0);
    g->setSpacing(8);
    int i = 0;
    for (DocLink* d : ds) { g->addWidget(docCard(d), i / 2, i % 2); i++; }
    if (!withNew.isEmpty()) {
        auto* nb = new QPushButton(tr("+ Novo documento"), w);
        nb->setObjectName(QStringLiteral("gmDocNew"));
        nb->setCursor(Qt::PointingHandCursor);
        nb->setFocusPolicy(Qt::NoFocus);
        connect(nb, &QPushButton::clicked, this, [this, withNew]() { QTimer::singleShot(0, this, [this, withNew]() { m_openDoc = newDocFor(withNew); renderPanel(); syncUi(); }); });
        g->addWidget(nb, i / 2, i % 2);
    }
    g->setColumnStretch(0, 1);
    g->setColumnStretch(1, 1);
    return w;
}
QWidget* Panel::crumbsOf(const QString& k)
{
    auto* w = new QWidget();
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    QStringList path = m_doc.pathOf(k);
    if (!path.isEmpty()) path.removeLast();
    for (const QString& x : path) {
        auto* b = new QPushButton(m_doc.labelOf(x), w);
        b->setObjectName(QStringLiteral("gmCrumb"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        connect(b, &QPushButton::clicked, this, [this, x]() { QTimer::singleShot(0, this, [this, x]() { m_canvas->select(x, true); }); });
        h->addWidget(b);
        h->addWidget(textLabel(QStringLiteral("›"), QStringLiteral("cartoNote"), w));
    }
    h->addStretch();
    return w;
}
// A "foto" do lugar: o planeta 3D (arrastar gira), a estrela com os planetas, ou a cor da região.
QWidget* Panel::photoOf(const QString& k, const Ref& o)
{
    auto* ph = new QWidget();
    ph->setFixedHeight(230);
    auto* lay = new QHBoxLayout(ph);
    lay->setContentsMargins(0, 0, 0, 0);
    const auto dragHint = [&]() {
        auto* h = new QLabel(tr("ARRASTE PRA GIRAR"), ph);
        h->setObjectName(QStringLiteral("gmGen"));
        h->setAttribute(Qt::WA_TransparentForMouseEvents);
        h->adjustSize();
        h->move(400 - h->width() - 14, 230 - h->height() - 8);
    };
    if (o.kind == RefKind::Planet || o.kind == RefKind::Moon) {
        lay->addWidget(new PlanetView(m_canvas, o.kind == RefKind::Moon ? o.moon : o.body, false, ph));
        dragHint();
    } else if (o.kind == RefKind::System) {
        if (planetsOf(*o.sys).empty() && (o.sys->isCore || o.sys->stars[0].kind == StarKind::BuracoNegro)) {
            // Buraco negro sem planetas: o retrato é ele inteiro, não a beirada do sol.
            auto* hole = new StarDot(*o.sys, 230, ph);
            hole->move((400 - 230) / 2, 0);
            return ph;
        }
        auto* sun = new StarDot(*o.sys, 200, ph, -100);
        sun->setFixedSize(100, 200);
        sun->move(0, 15);
        int n = 0, cursorX = 110;
        for (Body* p : planetsOf(*o.sys)) {
            if (n++ >= 8) break;
            const int px = int(std::min(56.0, std::floor(18 + 9 * std::sqrt(p->radius != 0 ? p->radius : 1) + 0.5)));
            auto* b = new QToolButton(ph);
            b->setObjectName(QStringLiteral("gmAct"));
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            b->setIconSize(QSize(px, px));
            b->setIcon(QIcon(QPixmap::fromImage(m_canvas->smallBall(*p, px, V3 { -0.95, 0.15, 0.3 }, devicePixelRatioF()))));
            const QString pk = keyP(*o.sys, *p);
            b->setToolTip(m_doc.labelOf(pk));
            connect(b, &QToolButton::clicked, this, [this, pk]() { QTimer::singleShot(0, this, [this, pk]() { m_canvas->select(pk, true); }); });
            b->setGeometry(cursorX, (230 - px - 6) / 2, px + 6, px + 6);
            cursorX += px + 16;
        }
    } else if (o.kind == RefKind::Region) {
        ph->setFixedHeight(120);
        lay->addWidget(new Swatch(QColor(o.region->color), ph, true));
    } else if (o.kind == RefKind::Build) {
        lay->addWidget(new ModelView(m_canvas, o.build->type, o.build->seed, ph));
        dragHint();
    } else if (o.kind == RefKind::Comet || o.kind == RefKind::Belt) {
        auto* l = new QLabel(ph);
        l->setAlignment(Qt::AlignCenter);
        CatalogItem it { o.kind == RefKind::Comet ? "cometa" : o.belt->kuiper ? "kuiper" : "asteroides", o.kind == RefKind::Comet ? ItemKind::Comet : ItemKind::Belt, "", false, false };
        l->setPixmap(itemArt(it, 200));
        lay->addWidget(l);
    } else if (o.kind == RefKind::Group) {
        ph->setFixedHeight(120);
        Body* host = nullptr;
        for (const QString& x : m_doc.pathOf(k)) { const Ref q = m_doc.resolve(x); if (q.kind == RefKind::Planet) host = q.body; if (q.kind == RefKind::Moon) host = q.moon; }
        if (host) {
            auto* l = new QLabel(ph);
            l->setPixmap(QPixmap::fromImage(m_canvas->smallBall(*host, 110, V3 { -0.62, 0.3, 0.72 }, devicePixelRatioF())));
            lay->addStretch();
            lay->addWidget(l);
        }
    } else {
        ph->setFixedHeight(150);
        lay->addWidget(new GalaxyPhoto(m_doc.galImg, ph));
    }
    return ph;
}
void Panel::startRename(QWidget* holder, const QString& k)
{
    auto* lay = holder->layout();
    if (!lay) return;
    auto* inp = new QLineEdit(m_doc.labelOf(k), holder);
    inp->setObjectName(QStringLiteral("gmRename"));
    static_cast<QVBoxLayout*>(lay)->insertWidget(0, inp);
    if (QLayoutItem* it = lay->itemAt(1)) if (it->widget()) it->widget()->hide();
    inp->setFocus();
    inp->selectAll();
    connect(inp, &QLineEdit::editingFinished, this, [this, k, inp]() {
        if (!inp->isVisible()) return;
        inp->hide();
        const QString v = inp->text().trimmed();
        const Ref o = m_doc.resolve(k);
        if (o.kind == RefKind::Region) { if (!v.isEmpty()) o.region->name = v; }
        else if (o.kind == RefKind::Group) { if (!v.isEmpty()) o.group->name = v; }
        else if (!v.isEmpty() && v != m_doc.genName(k)) m_doc.userNames.insert(k, v);
        else m_doc.userNames.remove(k);
        // o Lugar ligado a este pedaço da galáxia acompanha o nome
        if (const QString pid = m_doc.placeIds.value(k); !pid.isEmpty() && m_link.places && m_link.places->territorio(pid)) { m_link.places->updateTerritorio(pid, m_doc.labelOf(k)); m_link.places->save(); }
        scheduleSave();
        m_canvas->update();
        QTimer::singleShot(0, this, [this]() { renderPanel(true); syncTitle(); });
    });
}
void Panel::addGroup(QVBoxLayout* into, const QString& k, const QString& kind)
{
    auto* inp = new QLineEdit(into->parentWidget());
    inp->setObjectName(QStringLiteral("gmField"));
    inp->setPlaceholderText(tr("Nome do lugar (cidade, base, palácio…): Enter cria"));
    into->insertWidget(std::min(4, into->count()), inp);
    inp->setFocus();
    connect(inp, &QLineEdit::editingFinished, this, [this, inp, k, kind]() {
        if (!inp->isVisible()) return;
        inp->hide();
        const QString v = inp->text().trimmed();
        if (!v.isEmpty()) { m_doc.newGroup(k, v, kind); scheduleSave(); }
        QTimer::singleShot(0, this, [this]() { renderPanel(true); });
    });
}

// ── o painel ──
void Panel::renderPanel(bool keepScroll)
{
    syncLinkView();
    if (m_editorFlush) { m_editorFlush(); m_editorFlush = nullptr; }
    if (!isOpen()) { m_side->hide(); m_mini->hide(); return; }
    const bool open = panelShown();
    const bool mini = open && m_panelMini && !m_openDoc;
    m_side->setVisible(open);
    m_panelBox->setVisible(open && !mini);
    m_searchBox->setVisible(open);
    if (mini) m_side->setFixedHeight(46);
    else { m_side->setMinimumHeight(0); m_side->setMaximumHeight(QWIDGETSIZE_MAX); }
    m_mini->setVisible(mini);
    if (!open) { m_results->hide(); positionOverlays(); return; }
    if (mini) { renderMini(); positionOverlays(); return; }
    const int keep = keepScroll ? m_pscroll->verticalScrollBar()->value() : 0;
    auto* box = new QWidget();
    box->setObjectName(QStringLiteral("gmScrollBody"));
    auto* root = new QVBoxLayout(box);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    const QString k = m_canvas->sel;
    if (m_openDoc) {
        DocLink* d = m_openDoc;
        root->addWidget(editorFor(d, [this, d]() { docDropIfEmpty(d); m_openDoc = nullptr; QTimer::singleShot(0, this, [this]() { renderPanel(); syncUi(); }); }));
    } else {
        const Ref o = m_doc.resolve(k);
        // A foto, com minimizar e fechar por cima.
        QWidget* ph = photoOf(k, o);
        ph->setParent(box);
        root->addWidget(ph);
        const QColor ink = Theme::toColor(Theme::textPrimary()), a = Theme::toColor(Theme::accentDefault());
        const auto round = [&](const QString& ic, const QString& tip) {
            auto* b = new QToolButton(ph);
            b->setObjectName(QStringLiteral("gmRound"));
            b->setFixedSize(32, 32);
            b->setIconSize(QSize(16, 16));
            b->setIcon(carto::icon(ic, ink, a, 16));
            b->setToolTip(tip);
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            return b;
        };
        auto* minBtn = round(QStringLiteral("chevDown"), tr("Minimizar (vira um cartão)"));
        minBtn->move(400 - 32 - 48, 10);
        connect(minBtn, &QToolButton::clicked, this, [this]() { QTimer::singleShot(0, this, [this]() { m_panelMini = true; renderPanel(); }); });
        auto* closeBtn = round(QStringLiteral("close"), tr("Fechar"));
        closeBtn->move(400 - 32 - 10, 10);
        connect(closeBtn, &QToolButton::clicked, this, [this]() { QTimer::singleShot(0, this, [this]() { closePanel(); syncUi(); }); });

        auto* headW = new QWidget(box);
        auto* head = new QVBoxLayout(headW);
        head->setContentsMargins(16, 14, 16, 10);
        head->setSpacing(10);
        if (k != QLatin1String("GAL")) head->addWidget(crumbsOf(k));
        auto* t = new QWidget(headW);
        auto* tl = new QVBoxLayout(t);
        tl->setContentsMargins(0, 0, 0, 0);
        tl->setSpacing(3);
        auto* h1 = textLabel(m_doc.labelOf(k), QStringLiteral("gmH1"), t, true);
        tl->addWidget(h1);
        QString subT = subShort(k, o);
        auto* sub = textLabel(subT, QStringLiteral("gmSub"), t, true);
        if (o.kind == RefKind::Planet && o.body->hz == Hz::Dentro) { sub->setTextFormat(Qt::RichText); sub->setText(subT.toHtmlEscaped() + QStringLiteral(" · <span style=\"color:#6fd69a\">%1</span>").arg(tr("zona habitável"))); }
        tl->addWidget(sub);
        const bool generated = (o.kind == RefKind::System || o.kind == RefKind::Planet || o.kind == RefKind::Moon || o.kind == RefKind::Build || o.kind == RefKind::Comet || o.kind == RefKind::Belt) && !m_doc.userNames.contains(k) && !(o.kind == RefKind::System && o.sys->isCore);
        if (generated) tl->addWidget(textLabel(tr("Nome gerado pela galáxia. Renomeie quando quiser."), QStringLiteral("gmGen"), t, true));
        head->addWidget(t);
        auto* acts = new QHBoxLayout();
        acts->setSpacing(4);
        const auto act = [&](const QString& label, const QString& ic, const std::function<void()>& fn) {
            auto* b = new QToolButton(headW);
            b->setObjectName(QStringLiteral("gmAct"));
            b->setText(label);
            b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            b->setIconSize(QSize(20, 20));
            b->setIcon(carto::icon(ic, a, ink, 20));
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            b->setMinimumWidth(62);
            connect(b, &QToolButton::clicked, this, fn);
            acts->addWidget(b);
        };
        act(tr("Centralizar"), QStringLiteral("galCenter"), [this, k]() { m_canvas->select(k, true); });
        act(tr("Caderno"), QStringLiteral("galBook"), [this, k]() { openCaderno(k); });
        if (o.kind == RefKind::Region) act(m_canvas->focusKey == k ? tr("Sair do foco") : tr("Focar área"), QStringLiteral("galFocus"), [this, k]() { m_canvas->setFocus2(m_canvas->focusKey == k ? QString() : k); QTimer::singleShot(0, this, [this]() { renderPanel(true); }); });
        act(o.kind == RefKind::Region || o.kind == RefKind::Group || m_doc.userNames.contains(k) ? tr("Renomear") : tr("Dar nome"), QStringLiteral("pen"), [this, t, k]() { startRename(t, k); });
        if (o.kind != RefKind::Galaxy && o.kind != RefKind::Region) {
            act(tr("Lugar aqui"), QStringLiteral("galPlace"), [this, head, k]() { addGroup(head, k, QStringLiteral("Lugar")); });
            act(tr("Grupo social"), QStringLiteral("galSocial"), [this, head, k]() { addGroup(head, k, QStringLiteral("Social")); });
        }
        acts->addStretch();
        head->addLayout(acts);
        auto* w = new QPushButton(tr("Escrever sobre %1…").arg(m_doc.labelOf(k)), headW);
        w->setObjectName(QStringLiteral("gmWrite"));
        w->setIcon(carto::icon(QStringLiteral("galWrite"), Theme::toColor(Theme::textMuted()), a, 16));
        w->setCursor(Qt::PointingHandCursor);
        w->setFocusPolicy(Qt::NoFocus);
        connect(w, &QPushButton::clicked, this, [this, k]() { QTimer::singleShot(0, this, [this, k]() { m_openDoc = newDocFor(k); renderPanel(); syncUi(); }); });
        head->addWidget(w);
        root->addWidget(headW);

        // Abas.
        const Inside ins = insideOf(k, o);
        const auto below = docsBelow(k);
        int nDocs = int(m_doc.docsOf(k).size());
        for (const auto& x : below) nDocs += int(x.second.size());
        const bool canEdit = m_tool == QLatin1String("edit") && editable(o);
        auto* tabsF = new QFrame(box);
        tabsF->setObjectName(QStringLiteral("gmTabs"));
        auto* tb = new QHBoxLayout(tabsF);
        tb->setContentsMargins(12, 0, 12, 0);
        tb->setSpacing(4);
        const QList<std::array<QString, 2>> tabs = { { "geral", tr("Visão geral") }, { "docs", tr("Documentos %1").arg(nDocs) }, { "dentro", QStringLiteral("%1 %2").arg(ins.label).arg(ins.keys.size()) }, { "dados", canEdit ? tr("Editar") : tr("Dados") } };
        for (const auto& t2 : tabs) {
            auto* b = new QToolButton(tabsF);
            b->setObjectName(QStringLiteral("gmTab"));
            b->setText(t2[1]);
            b->setCheckable(true);
            b->setChecked(m_tab == t2[0]);
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            const QString id = t2[0];
            connect(b, &QToolButton::clicked, this, [this, id]() { QTimer::singleShot(0, this, [this, id]() { m_tab = id; renderPanel(); }); });
            tb->addWidget(b);
        }
        tb->addStretch();
        root->addWidget(tabsF);
        auto* bodyW = new QWidget(box);
        auto* body = new QVBoxLayout(bodyW);
        body->setContentsMargins(16, 12, 16, 16);
        body->setSpacing(10);
        if (m_tab == QLatin1String("geral")) renderTabGeneral(body, k, o);
        else if (m_tab == QLatin1String("docs")) renderTabDocs(body, k);
        else if (m_tab == QLatin1String("dentro")) renderTabInside(body, k, o);
        else renderTabData(body, k, o);
        root->addWidget(bodyW);
    }
    root->addStretch();
    if (QWidget* old = m_pscroll->takeWidget()) { old->hide(); old->deleteLater(); }
    m_pscroll->setWidget(box);
    m_side->setFixedWidth(m_openDoc ? 620 : 400);
    m_panelKey = k;
    positionOverlays();
    if (keep) QTimer::singleShot(0, this, [this, keep]() { m_pscroll->verticalScrollBar()->setValue(keep); });
}
void Panel::renderTabGeneral(QVBoxLayout* body, const QString& k, const Ref& o)
{
    QWidget* parent = body->parentWidget();
    auto* facts = factsGrid(quickFacts(k, o));
    facts->setParent(parent);
    body->addWidget(facts);
    if (QWidget* hs = habSection(k, o)) { hs->setParent(parent); body->addWidget(hs); }
    const auto own = m_doc.docsOf(k);
    const auto eyebrow = [&](const QString& t, int n) {
        auto* h = new QHBoxLayout();
        h->addWidget(textLabel(t.toUpper(), QStringLiteral("gmEyebrow"), parent));
        h->addWidget(textLabel(QString::number(n), QStringLiteral("cartoNote"), parent));
        h->addStretch();
        body->addLayout(h);
    };
    eyebrow(tr("Documentos"), int(own.size()));
    std::vector<DocLink*> first(own.begin(), own.begin() + std::min<long>(3, long(own.size())));
    auto* grid = docGrid(first, k);
    grid->setParent(parent);
    body->addWidget(grid);
    const auto below = docsBelow(k);
    int nDocs = int(own.size());
    for (const auto& x : below) nDocs += int(x.second.size());
    const auto link = [&](const QString& text, const QString& tab) {
        auto* l = new QPushButton(text, parent);
        l->setObjectName(QStringLiteral("gmLink"));
        l->setCursor(Qt::PointingHandCursor);
        l->setFocusPolicy(Qt::NoFocus);
        connect(l, &QPushButton::clicked, this, [this, tab]() { QTimer::singleShot(0, this, [this, tab]() { m_tab = tab; renderPanel(); }); });
        body->addWidget(l, 0, Qt::AlignLeft);
    };
    if (own.size() > 3 || !below.empty()) link(tr("Ver todos os documentos (%1)").arg(nDocs), QStringLiteral("docs"));
    if (o.kind == RefKind::Galaxy || o.kind == RefKind::Region || o.kind == RefKind::System) renderLinksSection(body, k);
    if (o.kind != RefKind::Group || o.group->kind != QLatin1String("Social")) renderPeopleSection(body, k);
    const Inside ins = insideOf(k, o);
    if (!ins.keys.isEmpty()) {
        eyebrow(ins.label, int(ins.keys.size()));
        auto* wrap = new QWidget(parent);
        auto* g = new QGridLayout(wrap);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(4);
        int i = 0;
        for (const QString& c : ins.keys.mid(0, 10)) {
            auto* b = new QPushButton(wrap);
            b->setObjectName(QStringLiteral("gmChip"));
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            b->setFixedHeight(28);
            auto* h = new QHBoxLayout(b);
            h->setContentsMargins(4, 2, 10, 2);
            h->setSpacing(6);
            QWidget* ic = iconEl(c, 20);
            ic->setParent(b);
            h->addWidget(ic);
            auto* nm = new Elide(m_doc.labelOf(c), QStringLiteral("cartoValue"), b);
            nm->setAttribute(Qt::WA_TransparentForMouseEvents);
            h->addWidget(nm, 1);
            connect(b, &QPushButton::clicked, this, [this, c]() { QTimer::singleShot(0, this, [this, c]() { m_canvas->select(c, true); }); });
            g->addWidget(b, i / 2, i % 2);
            i++;
        }
        if (ins.keys.size() > 10) {
            auto* b = new QPushButton(QStringLiteral("+ %1").arg(ins.keys.size() - 10), wrap);
            b->setObjectName(QStringLiteral("gmChip"));
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            connect(b, &QPushButton::clicked, this, [this]() { QTimer::singleShot(0, this, [this]() { m_tab = QStringLiteral("dentro"); renderPanel(); }); });
            g->addWidget(b, i / 2, i % 2);
        }
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        body->addWidget(wrap);
    }
    const auto above = docsAbove(k);
    if (!above.empty()) {
        int n = 0;
        QStringList names;
        for (const auto& x : above) { n += int(x.second.size()); names << m_doc.labelOf(x.first); }
        link(tr("%1 documentos vindo de cima (%2)").arg(n).arg(names.join(QStringLiteral(", "))), QStringLiteral("docs"));
    }
    if (o.kind == RefKind::Galaxy) {
        QStringList mine;
        for (auto it = m_doc.userNames.begin(); it != m_doc.userNames.end(); ++it) mine << it.key();
        for (const auto& d : m_doc.docs) if (!mine.contains(d->owner)) mine << d->owner;
        QStringList show;
        for (const QString& x : mine) if (x != QLatin1String("GAL") && !x.startsWith(QLatin1Char('R')) && m_doc.resolve(x)) show << x;
        if (!show.isEmpty()) {
            std::sort(show.begin(), show.end(), [this](const QString& a, const QString& b) { return m_doc.labelOf(a).localeAwareCompare(m_doc.labelOf(b)) < 0; });
            eyebrow(tr("Lugares com nome ou documento"), int(show.size()));
            for (const QString& x : show.mid(0, 60)) {
                const int n = int(m_doc.docsOf(x).size());
                QPushButton* row = itemRow(x, subShort(x, m_doc.resolve(x)), n ? tr("%1 doc").arg(n) : QString());
                row->setParent(parent);
                body->addWidget(row);
            }
        }
    }
}
void Panel::renderTabDocs(QVBoxLayout* body, const QString& k)
{
    QWidget* parent = body->parentWidget();
    auto* g = docGrid(m_doc.docsOf(k), k);
    g->setParent(parent);
    body->addWidget(g);
    const auto level = [&](const std::vector<std::pair<QString, std::vector<DocLink*>>>& list, const QString& title) {
        if (list.empty()) return;
        body->addWidget(textLabel(title.toUpper(), QStringLiteral("gmEyebrow"), parent));
        for (const auto& [c, ds] : list) {
            auto* h = new QHBoxLayout();
            h->setSpacing(6);
            QWidget* ic = iconEl(c, 20);
            ic->setParent(parent);
            h->addWidget(ic);
            h->addWidget(textLabel(m_doc.labelOf(c), QStringLiteral("cartoValue"), parent));
            h->addStretch();
            body->addLayout(h);
            auto* dg = docGrid(ds, QString());
            dg->setParent(parent);
            body->addWidget(dg);
        }
    };
    level(docsBelow(k), tr("Dos lugares aqui dentro"));
    level(docsAbove(k), tr("Vindo de cima"));
}
void Panel::renderTabInside(QVBoxLayout* body, const QString& k, const Ref& o)
{
    QWidget* parent = body->parentWidget();
    const Inside ins = insideOf(k, o);
    if (!ins.keys.isEmpty()) {
        body->addWidget(textLabel(ins.label.toUpper(), QStringLiteral("gmEyebrow"), parent));
        for (const QString& c : ins.keys.mid(0, 60)) {
            const Ref oc = m_doc.resolve(c);
            const int n = int(m_doc.docsOf(c).size());
            QPushButton* row = itemRow(c, subShort(c, oc), oc.kind == RefKind::Planet ? celsius(oc.body->temp) : n ? tr("%1 doc").arg(n) : QString());
            row->setParent(parent);
            body->addWidget(row);
        }
        if (ins.keys.size() > 60) body->addWidget(textLabel(tr("E mais %1. Use a busca.").arg(ins.keys.size() - 60), QStringLiteral("cartoNote"), parent));
    } else body->addWidget(textLabel(tr("Nada aqui dentro ainda."), QStringLiteral("cartoNote"), parent));
    if (o.kind != RefKind::Galaxy && o.kind != RefKind::Region) {
        const auto add = [&](const QString& kind, const QString& label) {
            auto* b = new QPushButton(label, parent);
            b->setObjectName(QStringLiteral("gmLink"));
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            connect(b, &QPushButton::clicked, this, [this, body, k, kind]() { addGroup(body, k, kind); });
            body->addWidget(b, 0, Qt::AlignLeft);
        };
        add(QStringLiteral("Lugar"), tr("+ Lugar aqui dentro"));
        add(QStringLiteral("Social"), tr("+ Grupo social"));
    }
}
void Panel::renderTabData(QVBoxLayout* body, const QString& k, const Ref& o)
{
    QWidget* parent = body->parentWidget();
    const bool canEdit = m_tool == QLatin1String("edit") && editable(o);
    if (canEdit) renderEdit(body, k, o);
    if (o.kind == RefKind::Planet || o.kind == RefKind::Moon) { QWidget* d = physSection(o, !canEdit); d->setParent(parent); body->addWidget(d); }
    else if (o.kind == RefKind::Build) body->addWidget(textLabel(catT(structOf(o.build->type)->desc), QStringLiteral("cartoNote"), parent, true));
    else { QWidget* s = statsGrid(quickFacts(k, o)); s->setParent(parent); body->addWidget(s); }
}
// O cartão: o painel minimizado. O mapa fica livre; os botões reabrem o painel na aba certa.
void Panel::renderMini()
{
    if (QLayout* old = m_mini->layout()) { QLayoutItem* it; while ((it = old->takeAt(0))) { if (it->widget()) { it->widget()->hide(); it->widget()->deleteLater(); } if (it->layout()) { QLayoutItem* q; while ((q = it->layout()->takeAt(0))) { if (q->widget()) { q->widget()->hide(); q->widget()->deleteLater(); } delete q; } } delete it; } delete old; }
    for (QObject* c : m_mini->children()) if (auto* w = qobject_cast<QWidget*>(c)) { w->hide(); w->deleteLater(); }
    const QString k = m_canvas->sel;
    const Ref o = m_doc.resolve(k);
    auto* v = new QVBoxLayout(m_mini);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(8);
    const QStringList sibs = siblingsOf(k);
    const int si = sibs.indexOf(k);
    auto* top = new QHBoxLayout();
    top->setSpacing(10);
    QWidget* ic;
    if (o.kind == RefKind::Planet || o.kind == RefKind::Moon) { auto* l = new QLabel(m_mini); l->setPixmap(QPixmap::fromImage(m_canvas->smallBall(o.kind == RefKind::Moon ? *o.moon : *o.body, 56, V3 { -0.62, 0.3, 0.72 }, devicePixelRatioF()))); ic = l; }
    else if (o.kind == RefKind::System) ic = new StarDot(*o.sys, 50, m_mini);
    else { ic = iconEl(k, 40); ic->setParent(m_mini); }
    top->addWidget(ic);
    auto* tx = new QVBoxLayout();
    tx->setSpacing(1);
    if (k != QLatin1String("GAL")) { QWidget* cr = crumbsOf(k); cr->setParent(m_mini); tx->addWidget(cr); }
    tx->addWidget(new Elide(m_doc.labelOf(k), QStringLiteral("gmMName"), m_mini));
    tx->addWidget(new Elide(subShort(k, o), QStringLiteral("gmSub"), m_mini));
    top->addLayout(tx, 1);
    const QColor ink = Theme::toColor(Theme::textPrimary()), a = Theme::toColor(Theme::accentDefault());
    const auto nav = [&](const QString& icn, const QString& tip, bool enabled, const std::function<void()>& fn) {
        auto* b = new QToolButton(m_mini);
        b->setObjectName(QStringLiteral("cartoBtn"));
        b->setFixedSize(26, 26);
        b->setIconSize(QSize(15, 15));
        b->setIcon(carto::icon(icn, ink, a, 15));
        b->setToolTip(tip);
        b->setEnabled(enabled);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        connect(b, &QToolButton::clicked, this, [this, fn]() { QTimer::singleShot(0, this, fn); });
        top->addWidget(b, 0, Qt::AlignTop);
    };
    if (sibs.size() > 1) {
        nav(QStringLiteral("back"), tr("Anterior"), si > 0, [this, sibs, si]() { m_canvas->select(sibs[si - 1], true); });
        nav(QStringLiteral("chevRight"), tr("Próximo"), si < sibs.size() - 1, [this, sibs, si]() { m_canvas->select(sibs[si + 1], true); });
    }
    nav(QStringLiteral("up"), tr("Abrir o painel"), true, [this]() { m_panelMini = false; renderPanel(); });
    nav(QStringLiteral("close"), tr("Fechar"), true, [this]() { closePanel(); syncUi(); });
    v->addLayout(top);
    QStringList facts;
    for (const auto& [x, y] : quickFacts(k, o)) facts << QStringLiteral("<span style=\"color:%1\">%2</span> <b>%3</b>").arg(Theme::textMuted(), x.toHtmlEscaped(), y.toHtmlEscaped());
    auto* mf = new QLabel(facts.join(QStringLiteral("&nbsp;&nbsp;&nbsp;")), m_mini);
    mf->setObjectName(QStringLiteral("cartoValue"));
    mf->setTextFormat(Qt::RichText);
    mf->setWordWrap(true);
    v->addWidget(mf);
    const Inside ins = insideOf(k, o);
    int nDocs = int(m_doc.docsOf(k).size());
    for (const auto& x : docsBelow(k)) nDocs += int(x.second.size());
    auto* btns = new QHBoxLayout();
    btns->setSpacing(6);
    const auto btn = [&](const QString& text, bool pri, const std::function<void()>& fn) {
        auto* b = new QPushButton(text, m_mini);
        b->setObjectName(pri ? QStringLiteral("gmMPri") : QStringLiteral("gmMBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        connect(b, &QPushButton::clicked, this, [this, fn]() { QTimer::singleShot(0, this, fn); });
        btns->addWidget(b);
    };
    btn(tr("+ Escrever"), true, [this, k]() { m_panelMini = false; m_openDoc = newDocFor(k); renderPanel(); syncUi(); });
    btn(tr("Documentos %1").arg(nDocs), false, [this]() { m_panelMini = false; m_tab = QStringLiteral("docs"); renderPanel(); });
    btn(QStringLiteral("%1 %2").arg(ins.label).arg(ins.keys.size()), false, [this]() { m_panelMini = false; m_tab = QStringLiteral("dentro"); renderPanel(); });
    btn(tr("Dados"), false, [this]() { m_panelMini = false; m_tab = QStringLiteral("dados"); renderPanel(); });
    v->addLayout(btns);
    m_mini->adjustSize();
}

// ── a busca ──
void Panel::search(const QString& q)
{
    const QString f = fold(q.trimmed());
    auto* lay = static_cast<QVBoxLayout*>(m_results->layout());
    QLayoutItem* it;
    while ((it = lay->takeAt(0))) { if (it->widget()) { it->widget()->hide(); it->widget()->deleteLater(); } delete it; }
    if (f.isEmpty()) { m_results->hide(); return; }
    QStringList keys { QStringLiteral("GAL") };
    for (const auto& r : m_doc.regions) keys << r->key;
    for (const auto& g : m_doc.groups) keys << g->key;
    for (const auto& s : m_doc.gal->systems) {
        if (s->gone) continue;
        keys << keyS(*s);
        for (Body* p : planetsOf(*s)) { keys << keyP(*s, *p); for (const auto& m : p->moons) keys << keyM(*s, *p, *m); }
    }
    for (const auto& b : m_doc.builds) keys << b->key;
    std::vector<std::pair<double, QString>> scored;
    for (const QString& k : keys) {
        const QString n = fold(m_doc.labelOf(k));
        const int i = n.indexOf(f);
        if (i < 0) continue;
        scored.push_back({ (i == 0 ? 0 : 10) + (m_doc.touched(k) ? 0 : 5) + n.size() / 20.0, k });
    }
    std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    m_resList.clear();
    for (size_t i = 0; i < std::min<size_t>(12, scored.size()); i++) m_resList << scored[i].second;
    m_resIdx = 0;
    if (m_resList.isEmpty()) lay->addWidget(textLabel(tr("Nada com esse nome."), QStringLiteral("cartoNote"), m_results));
    for (const QString& k : m_resList) {
        auto* b = new QPushButton(m_results);
        b->setObjectName(QStringLiteral("gmRes"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setFixedHeight(42);
        auto* h = new QHBoxLayout(b);
        h->setContentsMargins(6, 2, 6, 2);
        h->setSpacing(8);
        QWidget* ic = iconEl(k, 24);
        ic->setParent(b);
        h->addWidget(ic);
        auto* col = new QVBoxLayout();
        col->setSpacing(0);
        auto* nm = new Elide(m_doc.labelOf(k), QStringLiteral("cartoValue"), b);
        nm->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* sb = new Elide(subShort(k, m_doc.resolve(k)), QStringLiteral("cartoNote"), b);
        sb->setAttribute(Qt::WA_TransparentForMouseEvents);
        col->addWidget(nm); col->addWidget(sb);
        h->addLayout(col, 1);
        connect(b, &QPushButton::clicked, this, [this, k]() { QTimer::singleShot(0, this, [this, k]() { chooseResult(k); }); });
        lay->addWidget(b);
    }
    m_results->setFixedWidth(m_searchBox->width());
    m_results->adjustSize();
    m_results->move(m_searchBox->mapTo(this, QPoint(0, m_searchBox->height() + 4)));
    m_results->show();
    m_results->raise();
}
void Panel::chooseResult(const QString& k)
{
    m_query->setText(m_doc.labelOf(k));
    m_results->hide();
    m_canvas->select(k, true);
    openPanel();
    m_canvas->setFocus();
}

} // namespace galaxy
