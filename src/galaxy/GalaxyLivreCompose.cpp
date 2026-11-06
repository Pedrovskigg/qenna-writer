// Criação Livre: o montador do sistema e a prévia do sistema na prateleira.
// O montador abre na direita quando se pede "Um sistema para X", "Um sistema aqui dentro" ou "Um sistema":
// nome, estrela, quantos planetas e o tipo de cada um, em ordem de órbita. O mundo que já existe fica fixo.
// Sem mexer, é só apertar Gerar; o que é montado nasce de verdade, porque montar já é o sim.

#include "GalaxyLivre.h"
#include "GalaxyDoc.h"
#include "GalaxyCanvas.h"
#include "GalaxyHab.h"
#include "GalaxyPanel.h"

#include "../Theme.h"
#include "../carto/CartoIcons.h"

#include <QCoreApplication>
#include <QGridLayout>
#include <QPainter>
#include <QRadialGradient>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <numeric>

namespace galaxy {

namespace {
inline QString coreT(const char* s) { return QCoreApplication::translate("galaxy::Core", s); }
inline QColor col(const QString& c) { return Theme::toColor(c); }
inline double rnd() { return QRandomGenerator::global()->generateDouble(); }
template <typename T> T pickOne(std::initializer_list<T> l) { const std::vector<T> v(l); return v[size_t(rnd() * double(v.size())) % v.size()]; }
constexpr int COMPOSE_MAX = 12;
const std::vector<Type> PLANET_TYPES = { Type::Temperado, Type::Oceano, Type::Deserto, Type::Arido, Type::Gelado, Type::Lava, Type::Toxico, Type::Anao, Type::Gigante, Type::Quente, Type::GeloG, Type::Subnetuno, Type::Fortaleza, Type::Ecumeno, Type::Casco, Type::Forja };
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
// n planetas, de dentro pra fora: quentes perto da estrela, gigantes no meio do caminho, gelados no fim
Type slotTypeAt(double f)
{
    if (f < 0.3) return pickOne({ Type::Lava, Type::Quente, Type::Arido, Type::Deserto, Type::Toxico });
    if (f < 0.48) return pickOne({ Type::Deserto, Type::Arido, Type::Oceano, Type::Temperado });
    if (f < 0.85) return pickOne({ Type::Gigante, Type::GeloG, Type::Subnetuno, Type::Gigante });
    return pickOne({ Type::Gelado, Type::Anao });
}
QPixmap sunPix(const QColor& c, bool hole, int px)
{
    QPixmap pm(px * 2, px * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF m(px / 2.0, px / 2.0);
    QRadialGradient g(m, px / 2.0);
    if (hole) { g.setColorAt(0, Qt::black); g.setColorAt(0.5, Qt::black); g.setColorAt(0.58, QColor(255, 176, 112)); g.setColorAt(1, QColor(255, 120, 50, 0)); }
    else { QColor h = c; h.setAlpha(110); QColor z = c; z.setAlpha(0); g.setColorAt(0, c); g.setColorAt(0.45, c); g.setColorAt(0.5, h); g.setColorAt(1, z); }
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(m, px / 2.0, px / 2.0);
    return pm;
}
}

std::vector<Livre::Slot> Livre::suggestSlots(int n)
{
    std::vector<Slot> out;
    for (int i = 0; i < n; i++) out.push_back({ slotTypeAt((i + 0.5) / std::max(1, n)), ghostName(QStringLiteral("planet")), false });
    return out;
}
void Livre::openComposer(const QString& k)
{
    m_cmp = Composer();
    m_cmp.on = true;
    m_cmp.forKey = k.isEmpty() ? QStringLiteral("root") : k;
    static const StarKind pick[] = { StarKind::G, StarKind::K, StarKind::M, StarKind::F, StarKind::A };
    m_cmp.star = pick[int(rnd() * 5) % 5];
    m_cmp.name = ghostName(QStringLiteral("system"));
    const int n = 4 + int(rnd() * 3);
    if (isWorld(k)) {
        m_cmp.planet = k;
        m_cmp.terr = ancestor(k, LK::Terr);
        m_cmp.list = suggestSlots(n - 1);
        const Body* b = bodyOf(k);
        m_cmp.list.push_back({ b ? b->type : Type::Temperado, label(k), true });
    } else {
        m_cmp.terr = kindOf(k) == LK::Terr ? k : QString();
        m_cmp.list = suggestSlots(n);
    }
}
void Livre::setComposeCount(int n)
{
    n = std::clamp(n, 1, COMPOSE_MAX);
    std::vector<Slot> fixed, free;
    for (const Slot& s : m_cmp.list) (s.fixed ? fixed : free).push_back(s);
    const int want = std::max(0, n - int(fixed.size()));
    if (want < int(free.size())) {
        // tira do meio da fila: o mais quente e o mais gelado ficam
        std::stable_sort(free.begin(), free.end(), [](const Slot& a, const Slot& b) { return orbitRank(a.type) < orbitRank(b.type); });
        while (int(free.size()) > want) free.erase(free.begin() + long(free.size() / 2));
    } else while (int(free.size()) < want) free.push_back({ slotTypeAt((free.size() + 0.5) / std::max(1, want)), ghostName(QStringLiteral("planet")), false });
    m_cmp.list = fixed;
    for (const Slot& s : free) m_cmp.list.push_back(s);
    renderRight();
}
void Livre::pickSlotType(QWidget* anchor, int idx)
{
    if (idx < 0 || idx >= int(m_cmp.list.size())) return;
    const Type cur = m_cmp.list[size_t(idx)].type;
    openPop(anchor, [&](QVBoxLayout* l, QFrame* p) {
        popTitle(l, p, tr("Tipo"));
        auto* grid = new QGridLayout();
        grid->setSpacing(2);
        int n = 0;
        for (Type t : PLANET_TYPES) {
            auto* o = btn(QString(), QStringLiteral("lvTy"), p);
            o->setProperty("on", t == cur);
            o->setFixedSize(76, 76);
            auto* v = new QVBoxLayout(o);
            v->setContentsMargins(2, 4, 2, 2);
            v->setSpacing(2);
            auto* ic = new QLabel(o);
            ic->setPixmap(typeThumb(t, 40));
            ic->setAlignment(Qt::AlignHCenter);
            ic->setAttribute(Qt::WA_TransparentForMouseEvents);
            auto* tl = lab(coreT(typeLabel(t)), QStringLiteral("lvSmall"), o, true);
            tl->setAlignment(Qt::AlignHCenter);
            tl->setAttribute(Qt::WA_TransparentForMouseEvents);
            v->addWidget(ic);
            v->addWidget(tl);
            connect(o, &QPushButton::clicked, this, [this, idx, t]() { closePop(); if (idx < int(m_cmp.list.size())) m_cmp.list[size_t(idx)].type = t; QTimer::singleShot(0, this, &Livre::renderRight); });
            grid->addWidget(o, n / 4, n % 4);
            n++;
        }
        l->addLayout(grid);
    }, 336);
}
QWidget* Livre::composerEl(QWidget* parent)
{
    auto* c = new QWidget(parent);
    auto* l = new QVBoxLayout(c);
    l->setContentsMargins(0, 4, 0, 0);
    l->setSpacing(8);
    l->addWidget(lab(tr("GERAR SISTEMA"), QStringLiteral("lvSideTitle"), c));
    l->addWidget(lab(!m_cmp.planet.isEmpty() ? tr("Para %1, que já existe").arg(label(m_cmp.planet)) : !m_cmp.terr.isEmpty() ? tr("Dentro de %1").arg(label(m_cmp.terr)) : tr("Solto na galáxia"), QStringLiteral("lvFxNote"), c, true));
    const auto row = [&](const QString& key, QWidget* el) {
        auto* h = new QHBoxLayout();
        h->setSpacing(10);
        auto* k = lab(key, QStringLiteral("lvFxKey"), c);
        k->setFixedWidth(64);
        h->addWidget(k);
        h->addWidget(el, 1);
        l->addLayout(h);
    };
    // nome
    auto* nm = new QLineEdit(m_cmp.name, c);
    nm->setObjectName(QStringLiteral("lvCmpName"));
    connect(nm, &QLineEdit::textEdited, this, [this](const QString& v) { m_cmp.name = v; });
    row(tr("Nome"), nm);
    // estrela
    auto* stars = new QWidget(c);
    auto* sg = new QGridLayout(stars);
    sg->setContentsMargins(0, 0, 0, 0);
    sg->setSpacing(2);
    int si = 0;
    for (StarKind k : STAR_KINDS) {
        auto* b = new QToolButton(stars);
        b->setObjectName(QStringLiteral("lvCmpStar"));
        b->setCheckable(true);
        b->setChecked(k == m_cmp.star);
        b->setIcon(QIcon(sunPix(starShown(k), k == StarKind::BuracoNegro, 20)));
        b->setIconSize(QSize(20, 20));
        b->setFixedSize(26, 26);
        b->setToolTip(coreT(starKindLabel(k)));
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        connect(b, &QToolButton::clicked, this, [this, k]() { m_cmp.star = k; QTimer::singleShot(0, this, &Livre::renderRight); });
        sg->addWidget(b, si / 8, si % 8);
        si++;
    }
    row(tr("Estrela"), stars);
    auto* sn = lab(coreT(starKindLabel(m_cmp.star)), QStringLiteral("lvSmall"), c);
    sn->setContentsMargins(74, 0, 0, 0);
    l->addWidget(sn);
    // quantos planetas
    auto* cnt = new QWidget(c);
    auto* ch = new QHBoxLayout(cnt);
    ch->setContentsMargins(0, 0, 0, 0);
    ch->setSpacing(6);
    const int total = int(m_cmp.list.size());
    auto* minus = btn(QStringLiteral("−"), QStringLiteral("lvCmpStep"), cnt);
    minus->setFixedSize(26, 26);
    minus->setToolTip(tr("Menos um planeta"));
    minus->setEnabled(total > 1);
    connect(minus, &QPushButton::clicked, this, [this, total]() { QTimer::singleShot(0, this, [this, total]() { setComposeCount(total - 1); }); });
    auto* val = lab(QString::number(total), QStringLiteral("lvCmpN"), cnt);
    val->setAlignment(Qt::AlignCenter);
    val->setFixedWidth(22);
    auto* plus = btn(QStringLiteral("+"), QStringLiteral("lvCmpStep"), cnt);
    plus->setFixedSize(26, 26);
    plus->setToolTip(tr("Mais um planeta"));
    plus->setEnabled(total < COMPOSE_MAX);
    connect(plus, &QPushButton::clicked, this, [this, total]() { QTimer::singleShot(0, this, [this, total]() { setComposeCount(total + 1); }); });
    ch->addWidget(minus); ch->addWidget(val); ch->addWidget(plus);
    if (!m_cmp.planet.isEmpty()) ch->addWidget(lab(tr("contando %1").arg(label(m_cmp.planet)), QStringLiteral("lvSmall"), cnt));
    ch->addStretch();
    row(tr("Planetas"), cnt);
    // os planetas, de dentro pra fora
    l->addWidget(lab(tr("PLANETAS, DE DENTRO PRA FORA"), QStringLiteral("lvSideTitle"), c));
    std::vector<int> order(m_cmp.list.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [this](int a, int b) { return orbitRank(m_cmp.list[size_t(a)].type) < orbitRank(m_cmp.list[size_t(b)].type); });
    for (int idx : order) {
        const Slot& s = m_cmp.list[size_t(idx)];
        auto* r = new QFrame(c);
        r->setObjectName(s.fixed ? QStringLiteral("lvCmpFixed") : QStringLiteral("lvCmpRow"));
        auto* h = new QHBoxLayout(r);
        h->setContentsMargins(4, 3, 4, 3);
        h->setSpacing(8);
        if (s.fixed) {
            auto* b = new QLabel(r);
            b->setPixmap(ball(m_cmp.planet, 34));
            b->setFixedSize(38, 38);
            b->setAlignment(Qt::AlignCenter);
            h->addWidget(b);
            auto* v = new QVBoxLayout();
            v->setSpacing(0);
            v->addWidget(lab(s.name, QStringLiteral("lvOrbitName"), r));
            v->addWidget(lab(tr("%1 · já existe").arg(coreT(typeLabel(s.type))), QStringLiteral("lvSmall"), r));
            h->addLayout(v, 1);
        } else {
            auto* th = btn(QString(), QStringLiteral("lvCmpThumb"), r);
            th->setFixedSize(38, 38);
            th->setIcon(QIcon(typeThumb(s.type, 34)));
            th->setIconSize(QSize(34, 34));
            th->setToolTip(tr("Trocar o tipo"));
            connect(th, &QPushButton::clicked, this, [this, th, idx]() { pickSlotType(th, idx); });
            h->addWidget(th);
            auto* v = new QVBoxLayout();
            v->setSpacing(0);
            auto* in = new QLineEdit(s.name, r);
            in->setObjectName(QStringLiteral("lvCmpPlanet"));
            connect(in, &QLineEdit::textEdited, this, [this, idx](const QString& t) { if (idx < int(m_cmp.list.size())) m_cmp.list[size_t(idx)].name = t; });
            v->addWidget(in);
            auto* ty = btn(coreT(typeLabel(s.type)) + QStringLiteral(" ▾"), QStringLiteral("lvCmpType"), r);
            connect(ty, &QPushButton::clicked, this, [this, ty, idx]() { pickSlotType(ty, idx); });
            v->addWidget(ty, 0, Qt::AlignLeft);
            h->addLayout(v, 1);
            auto* x = new QToolButton(r);
            x->setObjectName(QStringLiteral("lvRowAdd"));
            x->setIcon(carto::icon(QStringLiteral("close"), col(Theme::textMuted()), col(Theme::accentWarning()), 12));
            x->setIconSize(QSize(12, 12));
            x->setToolTip(tr("Tirar %1").arg(s.name));
            x->setCursor(Qt::PointingHandCursor);
            x->setFocusPolicy(Qt::NoFocus);
            connect(x, &QToolButton::clicked, this, [this, idx]() { if (idx < int(m_cmp.list.size())) m_cmp.list.erase(m_cmp.list.begin() + idx); QTimer::singleShot(0, this, &Livre::renderRight); });
            h->addWidget(x);
        }
        l->addWidget(r);
    }
    auto* acts = new QHBoxLayout();
    acts->setSpacing(6);
    auto* add = btn(tr("+ Planeta"), QStringLiteral("lvPill"), c);
    add->setEnabled(total < COMPOSE_MAX);
    connect(add, &QPushButton::clicked, this, [this]() { m_cmp.list.push_back({ slotTypeAt(rnd()), ghostName(QStringLiteral("planet")), false }); QTimer::singleShot(0, this, &Livre::renderRight); });
    auto* roll = btn(tr("Sortear de novo"), QStringLiteral("lvPill"), c);
    connect(roll, &QPushButton::clicked, this, [this]() {
        std::vector<Slot> fixed;
        for (const Slot& s : m_cmp.list) if (s.fixed) fixed.push_back(s);
        std::vector<Slot> fresh = suggestSlots(int(m_cmp.list.size() - fixed.size()));
        for (const Slot& s : fixed) fresh.push_back(s);
        m_cmp.list = fresh;
        static const StarKind pick[] = { StarKind::G, StarKind::K, StarKind::M, StarKind::F, StarKind::A };
        m_cmp.star = pick[int(rnd() * 5) % 5];
        QTimer::singleShot(0, this, &Livre::renderRight);
    });
    acts->addWidget(add);
    acts->addWidget(roll);
    acts->addStretch();
    l->addLayout(acts);
    auto* foot = new QHBoxLayout();
    foot->addStretch();
    auto* no = btn(tr("Cancelar"), QStringLiteral("lvQuiet"), c);
    connect(no, &QPushButton::clicked, this, [this]() { m_cmp = Composer(); QTimer::singleShot(0, this, &Livre::renderRight); });
    auto* go = btn(tr("Gerar sistema"), QStringLiteral("lvGo"), c);
    connect(go, &QPushButton::clicked, this, [this]() { QTimer::singleShot(0, this, &Livre::buildComposed); });
    foot->addWidget(no);
    foot->addWidget(go);
    l->addLayout(foot);
    // na primeira vez, a direita rola até o montador
    if (!m_cmp.shown) {
        m_cmp.shown = true;
        QPointer<QWidget> self(c);
        QTimer::singleShot(0, this, [this, self]() {
            if (!self || !m_right->widget()) return;
            m_right->verticalScrollBar()->setValue(self->mapTo(m_right->widget(), QPoint(0, 0)).y() - 8);
        });
    }
    return c;
}
// O que foi montado nasce de verdade. Pro mundo que já existia, o abrigo dele vira o sistema: a chave não muda.
void Livre::buildComposed()
{
    if (!m_cmp.on) return;
    const Composer cp = m_cmp;
    m_cmp = Composer();
    Doc* d = m_doc;
    const uint32_t seed = QRandomGenerator::global()->generate();
    System* s = nullptr;
    const bool shelter = !cp.planet.isEmpty() && sysOf(cp.planet) && m_doc->isSolo(*sysOf(cp.planet));
    std::vector<Star> wasStars;
    StarKind wasKind = StarKind::G;
    if (shelter) {
        s = sysOf(cp.planet);
        wasStars = s->stars; wasKind = s->kind;
        s->stars = { starOf(cp.star, 0, s->seed) };
        s->kind = cp.star;
        refreshSystem(*s);
        m_doc->solo.remove(s->id);
        s->gone = false;
    } else {
        Region* r = cp.terr.isEmpty() ? nullptr : m_doc->resolve(cp.terr).region;
        double x = 0, y = 0;
        m_doc->freeSpot(r, x, y, seed);
        s = m_doc->addHandSystem(x, y, cp.star, seed);
        if (r) r->systems.insert(s->id);
    }
    s->edited = true;
    const QString sk = keyS(*s);
    const QString wasName = m_doc->userNames.value(sk);
    m_doc->userNames.insert(sk, cp.name.trimmed().isEmpty() ? ghostName(QStringLiteral("system")) : cp.name.trimmed());
    // os planetas, de dentro pra fora
    std::vector<Slot> order = cp.list;
    std::stable_sort(order.begin(), order.end(), [](const Slot& a, const Slot& b) { return orbitRank(a.type) < orbitRank(b.type); });
    std::vector<Body*> born;
    for (const Slot& slot : order) {
        if (slot.fixed) continue;
        Body* p = addPlanet(*s, slot.type, orbitFor(*s, slot.type, int(planetsOf(*s).size())), QRandomGenerator::global()->generate());
        p->moons.clear();
        m_doc->userNames.insert(keyP(*s, *p), slot.name.trimmed().isEmpty() ? ghostName(QStringLiteral("planet")) : slot.name.trimmed());
        born.push_back(p);
    }
    m_doc->layoutSystems();
    m_doc->place(*s);
    auto held = std::make_shared<std::vector<std::unique_ptr<Body>>>();
    auto alive = std::make_shared<std::vector<Body*>>(born);
    const QString name = label(sk);
    const int np = int(cp.list.size());
    undoable(np == 1 ? tr("%1 nasceu com 1 planeta.").arg(name) : tr("%1 nasceu com %2 planetas.").arg(name).arg(np),
        [d, s, held, alive, shelter, wasStars, wasKind, sk, wasName] {
            held->clear();
            for (Body* p : *alive) held->push_back(removeBody(*s, p));
            if (shelter) { s->stars = wasStars; s->kind = wasKind; refreshSystem(*s); d->solo.insert(s->id); }
            s->gone = true;
            if (wasName.isEmpty()) d->userNames.remove(sk); else d->userNames.insert(sk, wasName);
            d->layoutSystems();
        },
        [d, s, held, alive, shelter, cp, sk, name] {
            alive->clear();
            for (auto& b : *held) { alive->push_back(b.get()); restoreBody(*s, std::move(b), nullptr); }
            held->clear();
            if (shelter) { s->stars = { starOf(cp.star, 0, s->seed) }; s->kind = cp.star; refreshSystem(*s); d->solo.remove(s->id); }
            s->gone = false;
            d->userNames.insert(sk, name);
            d->layoutSystems();
        });
    if (cp.planet.isEmpty()) m_sel = sk;
    m_justMade = sk;
    changed();
    render();
}

// ── a prévia do sistema, no jeito da do mapa ──
void Livre::showSysPeek(QWidget* at, const QString& k)
{
    const System* sy = sysOf(k);
    if (!sy) return;
    m_peek = new QFrame(this);
    m_peek->setObjectName(QStringLiteral("lvPeek"));
    m_peek->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_peek->setFixedWidth(340);
    auto* v = new QVBoxLayout(m_peek);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(9);
    QStringList ws;
    int pending = 0, moons = 0;
    for (const QString& w : children(k)) {
        if (kindOf(w) != LK::Planet) continue;
        if (isGhost(w)) { pending++; continue; }
        ws << w;
        for (const QString& m : children(w)) if (kindOf(m) == LK::Moon && !isGhost(m)) moons++;
    }
    const Star& st = sy->stars[0];
    auto* head = new QHBoxLayout();
    head->setSpacing(10);
    auto* pic = new QLabel(m_peek);
    pic->setFixedSize(44, 44);
    pic->setPixmap(sunPix(starShown(st.kind), st.kind == StarKind::BuracoNegro, 44));
    head->addWidget(pic);
    auto* names = new QVBoxLayout();
    names->setSpacing(1);
    names->addWidget(lab(label(k), QStringLiteral("lvPeekName"), m_peek));
    const QString terr = ancestor(k, LK::Terr);
    names->addWidget(lab(coreT(starKindLabel(st.kind, st.pulsar)) + (terr.isEmpty() ? QString() : QStringLiteral(" · ") + label(terr)), QStringLiteral("lvSmall"), m_peek));
    head->addLayout(names, 1);
    v->addLayout(head);
    // dados rápidos, do motor
    auto* facts = new QGridLayout();
    facts->setHorizontalSpacing(14);
    facts->setVerticalSpacing(6);
    int fi = 0;
    const auto fact = [&](const QString& a, const QString& b) {
        auto* w = new QWidget(m_peek);
        auto* fv = new QVBoxLayout(w);
        fv->setContentsMargins(0, 0, 0, 0);
        fv->setSpacing(0);
        fv->addWidget(lab(a, QStringLiteral("lvSmall"), w));
        fv->addWidget(lab(b, QStringLiteral("lvPeekFact"), w));
        facts->addWidget(w, fi / 2, fi % 2);
        fi++;
    };
    if (st.temp > 0) fact(tr("Temperatura"), QStringLiteral("%1 K").arg(nf(std::round(st.temp / 10) * 10)));
    if (st.lum > 0) fact(tr("Luminosidade"), QStringLiteral("%1 L☉").arg(sig(st.lum)));
    if (sy->hasHz) fact(tr("Zona habitável"), QStringLiteral("%1–%2 UA").arg(sig(sy->hz[0]), sig(sy->hz[1])));
    fact(tr("Mundos"), QString::number(ws.size()) + (moons ? QStringLiteral(" · ") + (moons == 1 ? tr("1 lua") : tr("%1 luas").arg(moons)) : QString()));
    v->addLayout(facts);
    // a mistura das classes numa barra só
    std::array<int, 7> cnt {};
    int any = 0;
    for (const QString& w : ws) {
        const int c = habOf(w); if (c) { cnt[size_t(c)]++; any++; }
        for (const QString& m : children(w)) if (kindOf(m) == LK::Moon && !isGhost(m)) { const int cm = habOf(m); if (cm) { cnt[size_t(cm)]++; any++; } }
    }
    if (any) { auto* bar = new HabBar(m_peek); bar->setCounts(cnt); v->addWidget(bar); }
    // os planetas
    for (int i = 0; i < std::min<int>(8, ws.size()); i++) {
        const QString w = ws[i];
        const Body* b = bodyOf(w);
        auto* r = new QWidget(m_peek);
        r->setFixedHeight(26);
        auto* h = new QHBoxLayout(r);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(8);
        auto* bl = new QLabel(r);
        bl->setFixedSize(24, 24);
        bl->setPixmap(ball(w, 24));
        h->addWidget(bl);
        auto* nm = lab(QString(), QStringLiteral("lvPeekRow"), r);
        nm->setText(nm->fontMetrics().elidedText(label(w), Qt::ElideRight, 108));
        h->addWidget(nm);
        if (isCapital(w)) h->addWidget(starMark(r));
        auto* ty = lab(QString(), QStringLiteral("lvSmall"), r);
        ty->setText(ty->fontMetrics().elidedText(typeLabelOf(w), Qt::ElideRight, 86));
        h->addWidget(ty, 1, Qt::AlignRight);
        if (b && b->temp > 0) {
            auto* tv = lab(celsius(b->temp), QStringLiteral("lvSmall"), r);
            if (b->hz == Hz::Dentro) tv->setStyleSheet(QStringLiteral("color: #3fae75;"));
            h->addWidget(tv);
        }
        const int c = habOf(w);
        if (c) h->addWidget(habPill(c, r));
        v->addWidget(r);
    }
    if (ws.isEmpty()) v->addWidget(lab(tr("Nenhum mundo ainda."), QStringLiteral("lvPeekText"), m_peek));
    if (ws.size() > 8) v->addWidget(lab(tr("E mais %1").arg(ws.size() - 8), QStringLiteral("lvSmall"), m_peek));
    // o que o autor já pôs aqui
    int docs = 0;
    const std::function<void(const QString&)> walk = [&](const QString& x) { for (const QString& c2 : children(x)) { if (kindOf(c2) == LK::Doc && !isGhost(c2)) docs++; walk(c2); } };
    walk(k);
    int links = 0;
    for (const Panel::GLink& x : m_panel->linksOf(k)) if (x.a == k) links++;
    QStringList bits;
    if (docs) bits << (docs == 1 ? tr("1 documento") : tr("%1 documentos").arg(docs));
    if (links) bits << (links == 1 ? tr("1 conexão") : tr("%1 conexões").arg(links));
    if (!bits.isEmpty()) v->addWidget(lab(bits.join(QStringLiteral(" · ")), QStringLiteral("lvPeekFoot"), m_peek, true));
    if (pending) {
        auto* pd = lab(pending == 1 ? tr("1 vizinho sugerido esperando o sim") : tr("%1 vizinhos sugeridos esperando o sim").arg(pending), QStringLiteral("lvPeekPend"), m_peek, true);
        v->addWidget(pd);
    }
    m_peek->adjustSize();
    const QRect rc(at->mapTo(this, QPoint(0, 0)), at->size());
    const int x = std::clamp(rc.right() + 10, 8, std::max(8, width() - m_peek->width() - 8));
    const int y = std::clamp(rc.top() - 6, 8, std::max(8, height() - m_peek->height() - 8));
    m_peek->move(x, y);
    m_peek->show();
    m_peek->raise();
}

} // namespace galaxy
