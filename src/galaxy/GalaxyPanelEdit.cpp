// Editar: os dados de cada lugar. Cada mudança refaz o painel sem pular pro topo.

#include "GalaxyPanel.h"

#include "../Theme.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace galaxy {

namespace {
inline QString coreT(const char* s) { return QCoreApplication::translate("galaxy::Core", s); }
inline uint32_t randSeed() { return QRandomGenerator::global()->generate(); }
const std::vector<Type> PLANET_TYPES = { Type::Temperado, Type::Oceano, Type::Deserto, Type::Arido, Type::Gelado, Type::Lava, Type::Toxico, Type::Anao, Type::Gigante, Type::Quente, Type::GeloG, Type::Subnetuno, Type::Fortaleza, Type::Ecumeno, Type::Casco, Type::Forja };
const std::vector<Type> MOON_TYPES = { Type::Rochosa, Type::Gelada, Type::Vulcanica, Type::Temperada };
const QStringList REGION_COLORS = { QStringLiteral("#e0a458"), QStringLiteral("#6fb3e8"), QStringLiteral("#9bc49a"), QStringLiteral("#c98bd9"), QStringLiteral("#e07a6a"), QStringLiteral("#d6c46a"), QStringLiteral("#7fd1c7") };
inline bool isSolid(const Body& b) { return b.body == BodyKind::Lua || !(b.type == Type::Gigante || b.type == Type::Quente || b.type == Type::GeloG || b.type == Type::Subnetuno); }
// População pela densidade: o teto cresce com a área do corpo (um planeta-cidade tem ~200 Terras de gente por área).
inline double radiusE(const Body& b) { return b.body == BodyKind::Lua ? b.radiusKm / 6371 : b.radius; }
inline double popMax(const Body& b) { return 1.6e12 * radiusE(b) * radiusE(b); }
inline double sliderToPeople(const Body& b, double s) { return s <= 0 ? 0 : std::pow(10, mix(3, std::log10(popMax(b)), s / 100)); }
inline double peopleToSlider(const Body& b, double n) { return n > 0 ? clamp(((std::log10(n) - 3) / (std::log10(popMax(b)) - 3)) * 100, 1, 100) : 0; }
inline double tempToS(double k) { return std::sqrt(std::max(0.0, k - 3) / 2000) * 1000; }
inline double sToTemp(double v) { return 3 + 2000 * std::pow(v / 1000, 2); }
QString densityNote(const Body& b, double n)
{
    if (n == 0) return Panel::tr("Ninguém mora aqui.");
    const double d = n / (8e9 * radiusE(b) * radiusE(b));
    const QString t = d < 0.002 ? Panel::tr("Tão pouca gente pra esse tamanho que nada aparece visto do espaço.") : d < 0.05 ? Panel::tr("Cidades esparsas: pontinhos isolados no lado da noite.") : d < 0.4 ? Panel::tr("Menos gente por área que a Terra de hoje.") : d < 2.5 ? Panel::tr("Mais ou menos a densidade da Terra de hoje.") : d < 40 ? Panel::tr("Bem mais povoado que a Terra: as luzes viram redes contínuas.") : Panel::tr("Planeta-cidade: quase toda a terra firme é cidade.");
    return b.look.style == LookStyle::Olho ? t + Panel::tr(" Num planeta-olho, as cidades ficam no anel entre o dia e a noite.") : t;
}
}

bool Panel::editable(const Ref& o) const
{
    switch (o.kind) {
    case RefKind::System: return !o.sys->isCore;
    case RefKind::Galaxy: case RefKind::Region: case RefKind::Planet: case RefKind::Moon: case RefKind::Build: case RefKind::Comet: case RefKind::Belt: return true;
    default: return false;
    }
}
void Panel::edited(Body* b)
{
    const Ref o = m_doc.resolve(m_canvas->sel);
    if (o.sys) o.sys->edited = true;
    if (b) { m_canvas->invalidateBody(*b); b->texRev++; }
    m_canvas->refreshAfterEdit(nullptr);
    scheduleSave();
    QTimer::singleShot(0, this, [this]() { renderPanel(true); });
}
void Panel::renderEdit(QVBoxLayout* body, const QString& k, const Ref& o)
{
    QWidget* parent = body->parentWidget();
    const auto sect = [&](const QString& title) {
        auto* w = new QWidget(parent);
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(0, 4, 0, 4);
        v->setSpacing(6);
        if (!title.isEmpty()) { auto* t = new QLabel(title.toUpper(), w); t->setObjectName(QStringLiteral("gmEyebrow")); v->addWidget(t); }
        body->addWidget(w);
        return v;
    };
    const auto note = [&](QVBoxLayout* into, const QString& text) { auto* l = new QLabel(text, into->parentWidget()); l->setObjectName(QStringLiteral("cartoNote")); l->setWordWrap(true); into->addWidget(l); return l; };
    const auto btn = [&](QVBoxLayout* into, const QString& text, const std::function<void()>& fn, bool danger = false) {
        auto* b = new QPushButton(text, into->parentWidget());
        b->setObjectName(QStringLiteral("gmEditBtn"));
        b->setProperty("danger", danger);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        connect(b, &QPushButton::clicked, this, [this, fn]() { QTimer::singleShot(0, this, fn); });
        into->addWidget(b);
        return b;
    };
    const auto row = [&](QVBoxLayout* into, const QString& label, QWidget* el) {
        auto* w = new QVBoxLayout();
        w->setSpacing(3);
        auto* l = new QLabel(label, into->parentWidget());
        l->setObjectName(QStringLiteral("gmEditLabel"));
        w->addWidget(l);
        el->setParent(into->parentWidget());
        w->addWidget(el);
        into->addLayout(w);
    };
    const auto sel = [&](const std::vector<std::pair<QString, QString>>& pairs, const QString& val, const std::function<void(const QString&)>& fn) {
        auto* c = new QComboBox();
        c->setObjectName(QStringLiteral("cartoCombo"));
        c->setFocusPolicy(Qt::NoFocus);
        for (const auto& [v, l] : pairs) c->addItem(l, v);
        c->setCurrentIndex(std::max(0, c->findData(val)));
        connect(c, &QComboBox::activated, this, [c, fn]() { fn(c->currentData().toString()); });
        return c;
    };
    // Régua com o valor na ponta: mexendo mostra, soltando aplica.
    const auto range = [&](QVBoxLayout* into, const QString& label, int lo, int hi, double val, const std::function<QString(double)>& fmt, const std::function<void(double)>& apply) {
        auto* w = new QVBoxLayout();
        w->setSpacing(3);
        auto* h = new QHBoxLayout();
        auto* l = new QLabel(label, into->parentWidget());
        l->setObjectName(QStringLiteral("gmEditLabel"));
        auto* v = new QLabel(fmt(val), into->parentWidget());
        v->setObjectName(QStringLiteral("cartoValue"));
        h->addWidget(l); h->addStretch(); h->addWidget(v);
        w->addLayout(h);
        auto* s = new QSlider(Qt::Horizontal, into->parentWidget());
        s->setFocusPolicy(Qt::NoFocus);
        s->setRange(lo, hi);
        s->setValue(qRound(val));
        connect(s, &QSlider::valueChanged, this, [v, fmt](int q) { v->setText(fmt(q)); });
        connect(s, &QSlider::sliderReleased, this, [s, apply]() { apply(s->value()); });
        w->addWidget(s);
        into->addLayout(w);
        return s;
    };
    const auto typePairs = [](const std::vector<Type>& list) { std::vector<std::pair<QString, QString>> out; for (Type t : list) out.push_back({ QString::fromLatin1(typeKey(t)), coreT(typeLabel(t)) }); return out; };
    const auto stylePairs = [](const std::vector<LookStyle>& list) { std::vector<std::pair<QString, QString>> out; for (LookStyle t : list) out.push_back({ QString::fromLatin1(lookStyleKey(t)), coreT(lookStyleLabel(t)) }); return out; };
    const auto removeBtn = [&](const QString& label) { auto* v = sect(QString()); btn(v, label, [this, k]() { m_canvas->removeThing(k); }, true); };
    const auto removeRow = [&](QVBoxLayout* into, const QString& thumbKey, const QString& title, const QString& sub) {
        auto* h = new QHBoxLayout();
        h->setSpacing(8);
        QWidget* ic = iconEl(thumbKey, 24);
        ic->setParent(into->parentWidget());
        h->addWidget(ic);
        auto* t = new QLabel(QStringLiteral("%1 <span style=\"color:%2\">· %3</span>").arg(title.toHtmlEscaped(), Theme::textMuted(), sub.toHtmlEscaped()), into->parentWidget());
        t->setObjectName(QStringLiteral("cartoValue"));
        t->setTextFormat(Qt::RichText);
        h->addWidget(t, 1);
        auto* x = new QPushButton(tr("Remover"), into->parentWidget());
        x->setObjectName(QStringLiteral("gmLink"));
        x->setStyleSheet(QStringLiteral("color: #ff9c8c;"));
        x->setCursor(Qt::PointingHandCursor);
        x->setFocusPolicy(Qt::NoFocus);
        connect(x, &QPushButton::clicked, this, [this, thumbKey]() { QTimer::singleShot(0, this, [this, thumbKey]() { m_canvas->removeThing(thumbKey); }); });
        h->addWidget(x);
        into->addLayout(h);
    };
    const auto popSect = [&](Body* b) {
        auto* sec = sect(tr("População"));
        QLabel* n = nullptr;
        auto* holder = new QVBoxLayout();
        sec->addLayout(holder);
        n = note(sec, densityNote(*b, b->people));
        range(holder, tr("Quanta gente"), 0, 200, peopleToSlider(*b, b->people) * 2, [b, n](double v) { const double q = sliderToPeople(*b, v / 2); if (n) n->setText(densityNote(*b, q)); return q != 0 ? fmtCount(q) : Panel::tr("Ninguém"); }, [this, b](double v) { b->people = sliderToPeople(*b, v / 2); b->look.pop = lightLevel(b->people, radiusE(*b)); edited(b); });
        note(sec, tr("Aqui cabem até %1. A luz no lado da noite segue a densidade, não o número solto. Em mundo sem vida, as cidades viram cúpulas e bases.").arg(fmtCount(popMax(*b))));
    };
    const auto livingRows = [&](QVBoxLayout* into, Body* b) {
        range(into, tr("Água"), 0, 100, std::floor(waterOf(*b) * 100 + 0.5), [](double v) { return Panel::tr("%1% da superfície").arg(int(v)); }, [this, b](double v) { setWater(*b, v / 100); edited(b); });
        std::vector<LookStyle> styles;
        for (LookStyle q : LIVING_STYLES) if (q != LookStyle::Olho || b->locked) styles.push_back(q);
        row(into, tr("Paisagem"), sel(stylePairs(styles), QString::fromLatin1(lookStyleKey(b->look.style)), [this, b](const QString& q) { LookStyle st; if (lookStyleFromKey(q.toStdString(), st)) { restyle(*b, st); edited(b); } }));
        std::vector<std::pair<QString, QString>> vegs;
        for (Veg v : { Veg::Verde, Veg::Selva, Veg::Oliva, Veg::Carmim, Veg::Roxa, Veg::Azulada, Veg::Outono, Veg::Escura }) vegs.push_back({ QString::fromLatin1(vegKey(v)), coreT(vegLabel(v)) });
        row(into, tr("Vegetação"), sel(vegs, QString::fromLatin1(vegKey(b->look.vegKeyV)), [this, b](const QString& q) { for (Veg v : { Veg::Verde, Veg::Selva, Veg::Oliva, Veg::Carmim, Veg::Roxa, Veg::Azulada, Veg::Outono, Veg::Escura }) if (q == QLatin1String(vegKey(v))) { b->look.vegKeyV = v; b->look.veg = vegColor(v); } edited(b); }));
        range(into, tr("Nuvens"), 0, 100, std::floor(clamp((0.36 - b->look.cloud) / 0.48, 0, 1) * 100 + 0.5), [](double v) { return v < 15 ? Panel::tr("Céu limpo") : v < 55 ? Panel::tr("Algumas") : v < 85 ? Panel::tr("Muitas") : Panel::tr("Fechado"); }, [this, b](double v) { b->look.cloud = mix(0.36, -0.12, v / 100); edited(b); });
    };

    if (o.kind == RefKind::Galaxy) {
        auto* sec = sect(tr("Mais sistemas"));
        auto* h = new QHBoxLayout();
        auto* n = new QSpinBox(sec->parentWidget());
        n->setObjectName(QStringLiteral("cartoSpin"));
        n->setRange(1, 300);
        n->setValue(10);
        h->addWidget(n, 1);
        auto* msg = new QLabel(tr("Os sistemas que já existem ficam onde estão."), sec->parentWidget());
        msg->setObjectName(QStringLiteral("cartoNote"));
        msg->setWordWrap(true);
        auto* add = new QPushButton(tr("Adicionar"), sec->parentWidget());
        add->setObjectName(QStringLiteral("gmEditBtn"));
        add->setCursor(Qt::PointingHandCursor);
        add->setFocusPolicy(Qt::NoFocus);
        connect(add, &QPushButton::clicked, this, [this, n, msg]() {
            const size_t before = m_doc.gal->systems.size();
            const int want = n->value(), got = addSystems(*m_doc.gal, want);
            for (size_t i = before; i < m_doc.gal->systems.size(); i++) m_doc.gal->systems[i]->edited = true;
            m_doc.layoutSystems();
            m_canvas->docChanged();
            syncTitle();
            scheduleSave();
            msg->setText(got == want ? tr("%1 sistemas novos. Agora são %2.").arg(got).arg(m_doc.gal->systems.size()) : tr("Couberam %1. A galáxia está ficando cheia.").arg(got));
        });
        h->addWidget(add);
        sec->addLayout(h);
        sec->addWidget(msg);
    } else if (o.kind == RefKind::Region) {
        Region* r = o.region;
        auto* sec = sect(tr("Cor"));
        auto* dots = new QHBoxLayout();
        dots->setSpacing(6);
        for (const QString& c : REGION_COLORS) {
            auto* d = new QPushButton(sec->parentWidget());
            d->setFixedSize(22, 22);
            d->setCursor(Qt::PointingHandCursor);
            d->setFocusPolicy(Qt::NoFocus);
            d->setStyleSheet(QStringLiteral("QPushButton { background: %1; border-radius: 11px; border: 2px solid %2; }").arg(c, c == r->color ? Theme::textBright() : QStringLiteral("transparent")));
            connect(d, &QPushButton::clicked, this, [this, r, c]() { r->color = c; m_canvas->docChanged(); edited(); });
            dots->addWidget(d);
        }
        dots->addStretch();
        sec->addLayout(dots);
        auto* acts = sect(QString());
        btn(acts, tr("Pôr e tirar sistemas"), [this, r]() { m_canvas->realmActive = r->key; setTool(QStringLiteral("realm")); });
        const QString key = r->key;
        btn(acts, tr("Desfazer o território"), [this, key]() {
            size_t i = 0;
            for (; i < m_doc.regions.size(); i++) if (m_doc.regions[i]->key == key) break;
            if (i >= m_doc.regions.size()) return;
            auto held = std::make_shared<std::unique_ptr<Region>>(std::move(m_doc.regions[i]));
            m_doc.regions.erase(m_doc.regions.begin() + long(i));
            if (m_canvas->focusKey == key) m_canvas->setFocus2(QString());
            const QString name = (*held)->name;
            Doc* d = &m_doc;
            m_canvas->docChanged();
            m_canvas->pushUndo(tr("%1 desfeito.").arg(name), [d, held, i] { d->regions.insert(d->regions.begin() + long(std::min(i, d->regions.size())), std::move(*held)); });
            m_canvas->select(QStringLiteral("GAL"), false);
        }, true);
    } else if (o.kind == RefKind::System) {
        System* s = o.sys;
        for (size_t i = 0; i < s->stars.size(); i++) {
            const Star st = s->stars[i];
            auto* sec = sect(s->stars.size() > 1 ? tr("Estrela %1").arg(i ? QStringLiteral("B") : QStringLiteral("A")) : tr("Estrela"));
            const auto afterStar = [this, s, i](const Star& was) {
                refreshSystem(*s);
                s->edited = true;
                for (Body* b : planetsOf(*s)) if (isLiving(b->type)) { tuneTemperature(*b, b->temp); m_canvas->invalidateBody(*b); b->texRev++; }
                m_canvas->pushUndo(tr("Estrela trocada."), [s, i, was] { s->stars[i] = was; if (s->stars.size() == 1) s->kind = was.kind; refreshSystem(*s); });
                edited();
            };
            std::vector<std::pair<QString, QString>> kinds;
            for (StarKind q : STAR_KINDS) kinds.push_back({ QString::fromLatin1(starKindKey(q)), coreT(starKindLabel(q)) });
            row(sec, tr("Tipo"), sel(kinds, QString::fromLatin1(starKindKey(st.kind)), [this, s, i, st, afterStar](const QString& v) {
                StarKind nk;
                if (!starKindFromKey(v.toStdString(), nk)) return;
                s->stars[i] = starOf(nk, 0, s->seed + uint32_t(i) * 17 + randSeed() % 9973);
                if (s->stars.size() == 1) s->kind = nk;
                afterStar(st);
            }));
            double m0, m1;
            massRange(st.kind, m0, m1);
            const auto toM = [m0, m1](double v) { return m0 * std::pow(m1 / m0, v / 1000); };
            range(sec, tr("Massa"), 0, 1000, m1 > m0 ? (std::log(st.mass / m0) / std::log(m1 / m0)) * 1000 : 0, [toM](double v) { return QStringLiteral("%1 M☉").arg(sig(toM(v))); }, [this, s, i, st, toM, afterStar](double v) { s->stars[i] = starOf(st.kind, toM(v), s->seed + uint32_t(i) * 17); afterStar(st); });
            note(sec, (st.spec != "—" ? QString::fromStdString(st.spec) + QStringLiteral(" · ") : QString()) + QStringLiteral("%1 K · %2 L☉. ").arg(nf(std::round(st.temp / 10) * 10), sig(st.lum)) + tr("Trocar a estrela recalcula a temperatura, o ano e a zona habitável de todo o sistema."));
        }
        auto* reg = sect(tr("Território"));
        Region* cur = m_doc.regionOfSys(*s);
        std::vector<std::pair<QString, QString>> regs { { QString(), tr("Nenhum") } };
        for (const auto& r : m_doc.regions) regs.push_back({ r->key, r->name });
        regs.push_back({ QStringLiteral("+"), tr("Novo território…") });
        auto* rc = sel(regs, cur ? cur->key : QString(), [this, s](const QString& v) {
            if (v == QLatin1String("+")) { setTool(QStringLiteral("realm")); return; }
            Region* r = nullptr;
            for (const auto& q : m_doc.regions) if (q->key == v) r = q.get();
            Region* old = m_doc.regionOfSys(*s);
            if (old != r) m_canvas->assignSystems({ s }, r, r ? tr("%1 agora é de %2.").arg(m_doc.labelOf(keyS(*s)), r->name) : tr("%1 saiu de %2.").arg(m_doc.labelOf(keyS(*s)), old->name));
            edited();
        });
        rc->setParent(reg->parentWidget());
        reg->addWidget(rc);
        // Planeta novo: tipo e distância (no mapa, é só clicar com o Construir).
        auto* add = sect(tr("Novo planeta"));
        auto type = std::make_shared<Type>(Type::Temperado);
        const double a0 = s->hasHz ? std::sqrt(s->hz[0] * s->hz[1]) : (s->amax != 0 ? s->amax : 1) * 1.3;
        const double lo = std::max(0.015, s->stars[0].radius * 0.00465 * 5), hi = std::max((s->amax != 0 ? s->amax : 1) * 3, lo * 10);
        const auto toA = [lo, hi](double v) { return lo * std::pow(hi / lo, v / 1000); };
        row(add, tr("Tipo"), sel(typePairs(PLANET_TYPES), QString::fromLatin1(typeKey(*type)), [type](const QString& q) { typeFromKey(q.toStdString(), *type); }));
        QSlider* dist = range(add, tr("Distância da estrela"), 0, 1000, (std::log(a0 / lo) / std::log(hi / lo)) * 1000, [s, toA](double v) {
            const double x = toA(v);
            const QString zone = !s->hasHz ? QString() : x < s->hz[0] ? Panel::tr(" · mais quente que a zona habitável") : x > s->hz[1] ? Panel::tr(" · mais fria que a zona habitável") : Panel::tr(" · na zona habitável");
            return QStringLiteral("%1 UA").arg(sig(x)) + zone;
        }, [](double) {});
        btn(add, tr("Adicionar planeta"), [this, s, type, dist, toA]() {
            Body* p = addPlanet(*s, *type, toA(dist->value()), randSeed());
            s->edited = true;
            m_doc.layoutSystem(*s);
            m_doc.place(*s);
            auto held = std::make_shared<std::unique_ptr<Body>>();
            Doc* d = &m_doc;
            m_canvas->pushUndo(tr("%1 entrou em órbita.").arg(m_doc.labelOf(keyP(*s, *p))), [d, s, p, held] { *held = removeBody(*s, p); d->layoutSystem(*s); });
            edited();
        });
        const auto ps = planetsOf(*s);
        if (!ps.empty()) {
            auto* ls = sect(tr("Planetas"));
            for (Body* p : ps) removeRow(ls, keyP(*s, *p), m_doc.labelOf(keyP(*s, *p)), QStringLiteral("%1 · %2 UA").arg(coreT(typeLabel(p->type)), sig(p->a)));
        }
        if (s->hand) removeBtn(tr("Remover este sistema"));
    } else if (o.kind == RefKind::Planet) {
        System* s = o.sys;
        Body* p = o.body;
        const bool art = isArtificial(p->type);
        auto* look = sect(tr("Aparência"));
        btn(look, tr("Gerar outro visual"), [this, s, p]() { m_canvas->invalidateBody(*p); regenLook(*s, *p, randSeed()); edited(p); });
        row(look, tr("Tipo"), sel(typePairs(PLANET_TYPES), QString::fromLatin1(typeKey(p->type)), [this, s, p](const QString& v) {
            Type t;
            if (!typeFromKey(v.toStdString(), t)) return;
            m_canvas->invalidateBody(*p);
            setType(*s, *p, t);
            if (isArtificial(t)) p->rings.reset();
            m_doc.layoutSystem(*s);
            edited(p);
        }));
        if (p->look.hasRock) row(look, tr("Paisagem"), sel(stylePairs(rockStylesOf(p->type)), QString::fromLatin1(lookStyleKey(p->look.style)), [this, s, p](const QString& q) { LookStyle st; if (lookStyleFromKey(q.toStdString(), st)) { rockRestyle(*s, *p, st); edited(p); } }));
        auto* clima = sect(tr("Clima"));
        range(clima, tr("Temperatura média"), 0, 1000, tempToS(p->temp), [](double v) { return celsius(sToTemp(v)); }, [this, p](double v) { tuneTemperature(*p, sToTemp(v)); edited(p); });
        if (isLiving(p->type)) livingRows(clima, p);
        if (!art && isSolid(*p)) popSect(p);
        if (!art) {
            auto* rings = sect(tr("Anéis"));
            auto* ck = new QToolButton(rings->parentWidget());
            ck->setObjectName(QStringLiteral("cartoCheck"));
            ck->setCheckable(true);
            ck->setChecked(p->rings != nullptr);
            ck->setCursor(Qt::PointingHandCursor);
            ck->setFocusPolicy(Qt::NoFocus);
            ck->setText((p->rings ? QStringLiteral("☑  ") : QStringLiteral("☐  ")) + tr("Este planeta tem anéis"));
            connect(ck, &QToolButton::clicked, this, [this, p](bool on) { setRings(*p, on, randSeed()); edited(p); });
            rings->addWidget(ck);
            if (p->rings) btn(rings, tr("Outros anéis"), [this, p]() { setRings(*p, true, randSeed()); edited(p); });
            auto* moons = sect(tr("Luas · %1").arg(p->moons.empty() ? tr("nenhuma") : QString::number(p->moons.size())));
            auto mt = std::make_shared<Type>(Type::Rochosa);
            auto* h = new QHBoxLayout();
            auto* ms = sel(typePairs(MOON_TYPES), QString::fromLatin1(typeKey(*mt)), [mt](const QString& q) { typeFromKey(q.toStdString(), *mt); });
            ms->setParent(moons->parentWidget());
            h->addWidget(ms, 1);
            auto* am = new QPushButton(tr("+ Lua"), moons->parentWidget());
            am->setObjectName(QStringLiteral("gmEditBtn"));
            am->setCursor(Qt::PointingHandCursor);
            am->setFocusPolicy(Qt::NoFocus);
            connect(am, &QPushButton::clicked, this, [this, s, p, mt]() {
                QTimer::singleShot(0, this, [this, s, p, mt]() {
                    Body* m = addMoon(*s, *p, *mt, randSeed());
                    s->edited = true;
                    m_doc.layoutSystem(*s);
                    m_doc.place(*s);
                    auto held = std::make_shared<std::unique_ptr<Body>>();
                    Doc* d = &m_doc;
                    m_canvas->pushUndo(tr("%1 entrou em órbita.").arg(m_doc.labelOf(keyM(*s, *p, *m))), [d, s, m, held] { *held = removeBody(*s, m); d->layoutSystem(*s); });
                    edited();
                });
            });
            h->addWidget(am);
            moons->addLayout(h);
            for (const auto& m : p->moons) removeRow(moons, keyM(*s, *p, *m), m_doc.labelOf(keyM(*s, *p, *m)), coreT(typeLabel(m->type)));
        }
        removeBtn(tr("Remover este planeta"));
    } else if (o.kind == RefKind::Moon) {
        System* s = o.sys;
        Body* m = o.moon;
        auto* look = sect(tr("Aparência"));
        btn(look, tr("Gerar outro visual"), [this, s, m]() { m_canvas->invalidateBody(*m); regenLook(*s, *m, randSeed()); edited(m); });
        row(look, tr("Tipo"), sel(typePairs(MOON_TYPES), QString::fromLatin1(typeKey(m->type)), [this, s, m](const QString& v) { Type t; if (typeFromKey(v.toStdString(), t)) { m_canvas->invalidateBody(*m); setType(*s, *m, t); edited(m); } }));
        if (m->look.hasRock) row(look, tr("Paisagem"), sel(stylePairs(rockStylesOf(m->type)), QString::fromLatin1(lookStyleKey(m->look.style)), [this, s, m](const QString& q) { LookStyle st; if (lookStyleFromKey(q.toStdString(), st)) { rockRestyle(*s, *m, st); edited(m); } }));
        if (m->type == Type::Temperada) { auto* clima = sect(tr("Clima")); livingRows(clima, m); }
        popSect(m);
        removeBtn(tr("Remover esta lua"));
    } else {
        if (o.kind == RefKind::Build) {
            Build* b = o.build;
            auto* sec = sect(QString());
            range(sec, tr("Tamanho"), 40, 400, std::floor(b->scale * 100 + 0.5), [](double v) { return QStringLiteral("%1%").arg(int(v)); }, [this, b](double v) { b->scale = v / 100; edited(); });
        }
        if (m_canvas->removable(k)) removeBtn(tr("Remover do mapa"));
    }
}

} // namespace galaxy
