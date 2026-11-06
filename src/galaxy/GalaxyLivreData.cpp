// Criação Livre: o que cada chave é e tudo que muda a galáxia. Cada mudança entra no desfazer do mapa,
// e o aviso com "Desfazer" aparece na própria tela.

#include "GalaxyLivre.h"
#include "GalaxyDoc.h"
#include "GalaxyCanvas.h"
#include "GalaxyHab.h"
#include "GalaxyPanel.h"
#include "GalaxyStructures.h"

#include "../ProjectModel.h"
#include "../TerritorioStore.h"

#include <QCoreApplication>
#include <QRandomGenerator>

#include <algorithm>

namespace galaxy {

namespace {
inline QString coreT(const char* s) { return QCoreApplication::translate("galaxy::Core", s); }
inline QString catT(const char* s) { return QCoreApplication::translate("galaxy::Catalog", s); }
inline uint32_t randSeed() { return QRandomGenerator::global()->generate(); }
const QStringList COLORS = { QStringLiteral("#7fb2e6"), QStringLiteral("#5fc7c0"), QStringLiteral("#79c98a"), QStringLiteral("#c4cf6d"), QStringLiteral("#e0b35e"), QStringLiteral("#e08a5e"), QStringLiteral("#d96f6f"), QStringLiteral("#c77fc4"), QStringLiteral("#9587de"), QStringLiteral("#a8b4c6"), QStringLiteral("#d8cdb8") };
const std::vector<Type> NATURAL = { Type::Temperado, Type::Oceano, Type::Deserto, Type::Arido, Type::Gelado, Type::Lava, Type::Toxico, Type::Anao, Type::Gigante, Type::Quente, Type::GeloG, Type::Subnetuno };
const std::vector<Type> MOONS = { Type::Rochosa, Type::Gelada, Type::Vulcanica, Type::Temperada };
// Cada tipo na distância em que faz sentido, a partir da zona habitável da estrela.
double orbitK(Type t)
{
    switch (t) {
    case Type::Lava: return 0.25; case Type::Quente: return 0.15; case Type::Toxico: return 0.6; case Type::Deserto: return 0.85; case Type::Arido: return 1.3;
    case Type::Gelado: return 2.2; case Type::Anao: return 4; case Type::Gigante: return 5; case Type::GeloG: return 12; case Type::Subnetuno: return 3; case Type::Forja: return 0.7;
    default: return 1;
    }
}
double orbitA(const System& s, Type t, int slot)
{
    const double mid = s.hasHz ? (s.hz[0] + s.hz[1]) / 2 : std::max(0.3, std::sqrt(std::max(s.L, 1e-4)));
    double a = mid * orbitK(t) * (1 + 0.12 * std::max(0, slot));
    // duas órbitas no mesmo lugar não: empurra pra fora até achar vaga
    for (int i = 0; i < 40; i++) {
        bool taken = false;
        for (const auto& b : s.bodies) if (b->body == BodyKind::Planeta && std::fabs(b->a - a) < a * 0.06) taken = true;
        if (!taken) break;
        a *= 1.12;
    }
    return a;
}
int uidOf(const Body& b) { return b.uid ? b.uid : b.idx; }
}

double Livre::orbitFor(const System& s, Type t, int slot) { return orbitA(s, t, slot); }
double Livre::orbitRank(Type t) { return orbitK(t); }

// ── o que é cada chave ──
Livre::LK Livre::kindOf(const QString& k) const
{
    if (k.isEmpty()) return LK::None;
    if (k == QLatin1String("GAL")) return LK::Gal;
    switch (k[0].unicode()) {
    case 'R': return LK::Terr;
    case 'S': return LK::Sys;
    case 'P': return LK::Planet;
    case 'M': return LK::Moon;
    case 'G': return LK::Place;
    case 'B': return LK::Build;
    case 'D': return LK::Doc;
    case 'X': return LK::Link;
    default: return LK::None;
    }
}
DocLink* Livre::docOf(const QString& k) const
{
    if (kindOf(k) != LK::Doc) return nullptr;
    const int id = k.mid(1).toInt();
    for (const auto& d : m_doc->docs) if (d->id == id) return d.get();
    return nullptr;
}
QString Livre::docKey(const DocLink* d) const { return QStringLiteral("D%1").arg(d->id); }
bool Livre::valid(const QString& k) const
{
    switch (kindOf(k)) {
    case LK::Gal: return true;
    case LK::Doc: { const DocLink* d = docOf(k); return d && (d->parent ? valid(QStringLiteral("D%1").arg(d->parent)) : valid(d->owner)); }
    case LK::Link: return m_panel->m_link.places && m_panel->m_link.places->link(k.mid(1));
    case LK::None: return false;
    default: break;
    }
    const Ref o = m_doc->resolve(k);
    if (!o) return false;
    if (o.kind == RefKind::System) return !o.sys->isCore && !m_doc->isSolo(*o.sys);
    return true;
}
bool Livre::isGhost(const QString& k) const { return m_doc->ghosts.contains(k); }
bool Livre::isCapital(const QString& k) const { return m_doc->capitals.contains(k); }
Body* Livre::bodyOf(const QString& k) const
{
    const Ref o = m_doc->resolve(k);
    if (o.kind == RefKind::Moon) return o.moon;
    if (o.kind == RefKind::Planet) return o.body;
    return nullptr;
}
Body* Livre::hostOf(const QString& k) const { const Ref o = m_doc->resolve(k); return o.kind == RefKind::Moon ? o.body : nullptr; }
System* Livre::sysOf(const QString& k) const { return m_doc->resolve(k).sys; }
QString Livre::solarKey(const QString& world) const
{
    const QString w = kindOf(world) == LK::Moon ? parentKey(world) : world;
    System* s = sysOf(w);
    return s && !m_doc->isSolo(*s) ? keyS(*s) : QString();
}
std::string Livre::buildType(const QString& k) const { const Ref o = m_doc->resolve(k); return o.kind == RefKind::Build ? o.build->type : std::string(); }
uint32_t Livre::buildSeed(const QString& k) const { const Ref o = m_doc->resolve(k); return o.kind == RefKind::Build ? o.build->seed : 91; }

QString Livre::kindName(LK t) const
{
    switch (t) {
    case LK::Gal: return tr("Galáxia"); case LK::Terr: return tr("Território"); case LK::Sys: return tr("Sistema"); case LK::Planet: return tr("Planeta");
    case LK::Moon: return tr("Lua"); case LK::Place: return tr("Lugar"); case LK::Build: return tr("Construção"); case LK::Doc: return tr("Documento"); case LK::Link: return tr("Conexão");
    default: return QString();
    }
}
QString Livre::label(const QString& k) const
{
    switch (kindOf(k)) {
    case LK::Doc: { const DocLink* d = docOf(k); return d ? d->title : QString(); }
    case LK::Link: {
        const auto* l = m_panel->m_link.places ? m_panel->m_link.places->link(k.mid(1)) : nullptr;
        if (!l) return QString();
        const QString a = m_panel->keyOfPlace(l->fromTerritorioId), b = m_panel->keyOfPlace(l->toTerritorioId);
        const auto* ta = m_panel->m_link.places->territorio(l->fromTerritorioId);
        const auto* tb = m_panel->m_link.places->territorio(l->toTerritorioId);
        return QStringLiteral("%1 ↔ %2").arg(a.isEmpty() ? (ta ? ta->name : QStringLiteral("?")) : label(a), b.isEmpty() ? (tb ? tb->name : QStringLiteral("?")) : label(b));
    }
    case LK::None: return QString();
    default: {
        const QString n = m_doc->labelOf(k);
        return n.isEmpty() ? tr("%1 sem nome").arg(kindName(kindOf(k))) : n;
    }
    }
}
QString Livre::parentKey(const QString& k) const
{
    switch (kindOf(k)) {
    case LK::Terr: case LK::Link: case LK::Gal: case LK::None: return QString();
    case LK::Doc: { const DocLink* d = docOf(k); if (!d) return QString(); return d->parent ? QStringLiteral("D%1").arg(d->parent) : d->owner; }
    case LK::Sys: { const Ref o = m_doc->resolve(k); const Region* r = o.sys ? m_doc->regionOfSys(*o.sys) : nullptr; return r ? r->key : QString(); }
    case LK::Planet: {
        const Ref o = m_doc->resolve(k);
        if (!o.sys) return QString();
        if (m_doc->isSolo(*o.sys)) { const Region* r = m_doc->regionOfSys(*o.sys); return r ? r->key : QString(); }
        return keyS(*o.sys);
    }
    default: {
        const QString p = m_doc->parentOf(k);
        return p == QLatin1String("GAL") ? QString() : p;
    }
    }
}
QStringList Livre::pathOf(const QString& k) const
{
    QStringList out;
    QString c = parentKey(k);
    for (int guard = 0; !c.isEmpty() && guard < 40; guard++) { out.prepend(c); c = parentKey(c); }
    return out;
}
bool Livre::isInside(const QString& k, const QString& anc) const
{
    if (anc.isEmpty()) return true;
    for (QString c = k; !c.isEmpty(); c = parentKey(c)) if (c == anc) return true;
    return false;
}
QString Livre::ancestor(const QString& k, LK t) const
{
    for (QString c = parentKey(k); !c.isEmpty(); c = parentKey(c)) if (kindOf(c) == t) return c;
    return QString();
}
std::vector<System*> Livre::livreSystems() const
{
    std::vector<System*> out;
    for (const auto& s : m_doc->gal->systems) if (!s->isCore && !s->gone && !m_doc->isSolo(*s)) out.push_back(s.get());
    return out;
}
QStringList Livre::children(const QString& k0) const
{
    const QString k = k0 == QLatin1String("GAL") ? QString() : k0;
    QStringList out;
    const auto docsOf = [&](const QString& owner) { for (const auto& d : m_doc->docs) if (d->owner == owner && !d->parent) out << docKey(d.get()); };
    const auto placesOf = [&](const QString& parent) { for (const auto& g : m_doc->groups) if (g->parent == parent) out << g->key; };
    const auto worldsOfSys = [&](const System& s) { for (Body* p : planetsOf(s)) out << keyP(s, *p); };
    const LK t = kindOf(k);
    if (k.isEmpty()) {
        for (const auto& r : m_doc->regions) out << r->key;
        for (System* s : livreSystems()) if (!m_doc->regionOfSys(*s)) out << keyS(*s);
        for (const auto& s : m_doc->gal->systems) if (m_doc->isSolo(*s) && !m_doc->regionOfSys(*s)) worldsOfSys(*s);
        return out;
    }
    switch (t) {
    case LK::Terr: {
        const Ref o = m_doc->resolve(k);
        if (!o.region) return out;
        for (System* s : livreSystems()) if (o.region->systems.contains(s->id)) out << keyS(*s);
        for (const auto& s : m_doc->gal->systems) if (m_doc->isSolo(*s) && o.region->systems.contains(s->id)) worldsOfSys(*s);
        placesOf(k); docsOf(k);
        break;
    }
    case LK::Sys: {
        const Ref o = m_doc->resolve(k);
        if (!o.sys) return out;
        worldsOfSys(*o.sys);
        for (const auto& b : m_doc->builds) if (b->sys == o.sys->id && b->host < 0) out << b->key;
        placesOf(k); docsOf(k);
        break;
    }
    case LK::Planet: {
        const Ref o = m_doc->resolve(k);
        if (!o.body) return out;
        for (const auto& m : o.body->moons) out << keyM(*o.sys, *o.body, *m);
        placesOf(k);
        for (const auto& b : m_doc->builds) if (b->sys == o.sys->id && b->host == uidOf(*o.body)) out << b->key;
        docsOf(k);
        break;
    }
    case LK::Moon: case LK::Place: case LK::Build: placesOf(k); docsOf(k); break;
    case LK::Doc: { const DocLink* d = docOf(k); if (d) for (const auto& x : m_doc->docs) if (x->parent == d->id) out << docKey(x.get()); break; }
    default: break;
    }
    return out;
}
QStringList Livre::inside(const QString& k) const
{
    QStringList out;
    const bool planet = kindOf(k) == LK::Planet;
    for (const QString& c : children(k)) {
        const LK t = kindOf(c);
        if (t == LK::Sys || t == LK::Planet || (t == LK::Moon && !planet)) continue;
        out << c;
    }
    return out;
}
QStringList Livre::worldsIn(const QString& k) const
{
    QStringList out;
    const std::function<void(const QString&)> walk = [&](const QString& x) {
        for (const QString& c : children(x)) {
            const LK t = kindOf(c);
            if (t == LK::Planet || t == LK::Moon) { if (!isGhost(c)) out << c; walk(c); }
            else if (t == LK::Sys || t == LK::Terr) walk(c);
        }
    };
    walk(k);
    return out;
}
QStringList Livre::allWorlds() const
{
    QStringList out;
    for (const auto& s : m_doc->gal->systems) {
        if (s->isCore || (s->gone && !m_doc->isSolo(*s))) continue;
        for (Body* p : planetsOf(*s)) {
            const QString pk = keyP(*s, *p);
            if (!isGhost(pk)) out << pk;
            for (const auto& m : p->moons) { const QString mk = keyM(*s, *p, *m); if (!isGhost(mk)) out << mk; }
        }
    }
    return out;
}
QString Livre::typeLabelOf(const QString& k) const
{
    if (const Body* b = bodyOf(k)) return coreT(typeLabel(b->type));
    if (kindOf(k) == LK::Build) { const StructDef* d = structOf(buildType(k)); return d ? catT(d->name) : QString(); }
    return QString();
}
int Livre::habOf(const QString& k, bool* isAuto) const
{
    const Body* b = bodyOf(k);
    if (!b) return 0;
    const HabInfo i = habInfo(*b, sysOf(k), hostOf(k));
    if (isAuto) *isAuto = b->habSet == 0;
    return i.cls;
}
// Quem orbita o mundo na janela 3D: construções em órbita baixa, luas mais longe; anel orbital e elevador abraçam o planeta.
Livre::OrbitPlan Livre::orbitPlan(const QString& world) const
{
    OrbitPlan out;
    if (kindOf(world) != LK::Planet) return out;
    const Ref o = m_doc->resolve(world);
    if (!o.body) return out;
    std::vector<QString> builds, moons;
    for (const auto& b : m_doc->builds) {
        if (b->sys != o.sys->id || b->host != uidOf(*o.body)) continue;
        if (b->type == "anelOrbital" || b->type == "elevador") out.wraps.push_back({ b->key, b->type == "anelOrbital", int(std::count_if(out.wraps.begin(), out.wraps.end(), [](const Wrap& w) { return w.ring; })) });
        else builds.push_back(b->key);
    }
    for (const auto& m : o.body->moons) moons.push_back(keyM(*o.sys, *o.body, *m));
    const auto phaseOf = [](const QString& k) { return double(qHash(k) % 628) / 100; };
    for (size_t i = 0; i < builds.size(); i++) out.list.push_back({ builds[i], 1.34 + 0.16 * double(i), 0.42 - 0.04 * double(i), phaseOf(builds[i]), false });
    const double b0 = builds.empty() ? 1.7 : 1.34 + 0.16 * double(builds.size()) + 0.22;
    for (size_t i = 0; i < moons.size(); i++) out.list.push_back({ moons[i], b0 + 0.46 * double(i), 0.2 / (1 + double(i) * 0.6), phaseOf(moons[i]), true });
    int rings = 0;
    bool elev = false;
    for (const Wrap& w : out.wraps) { if (w.ring) rings++; else elev = true; }
    double outer = 0;
    for (const Orbiter& x : out.list) outer = std::max(outer, x.rr + 0.12);
    if (rings) outer = std::max(outer, 1.62 + 0.15 * (rings - 1) + 0.12);
    if (elev) outer = std::max(outer, 1.95);
    out.outer = outer;
    return out;
}

// ── mudar ──
void Livre::changed(bool map)
{
    m_panel->scheduleSave();
    if (map) m_canvas->docChanged();
}
void Livre::undoable(const QString& label, std::function<void()> undo, std::function<void()> redo)
{
    m_canvas->pushUndo(label, std::move(undo), std::move(redo));
}
QString Livre::ghostName(const QString& kind)
{
    const std::string key = QStringLiteral("livre-%1-%2").arg(kind).arg(++m_nameN + uint32_t(m_doc->seq) * 7919u).toStdString();
    if (kind == QLatin1String("system")) return QString::fromStdString(m_doc->names->system(key));
    if (kind == QLatin1String("moon")) return QString::fromStdString(m_doc->names->moon(key));
    return QString::fromStdString(m_doc->names->planet(key));
}
QString Livre::createWorld(const QString& name, const QString& parent, Type t, bool ghost)
{
    System* s = nullptr;
    if (kindOf(parent) == LK::Sys) s = sysOf(parent);
    const uint32_t seed = randSeed();
    Doc* d = m_doc;
    if (!s) {
        Region* r = kindOf(parent) == LK::Terr ? m_doc->resolve(parent).region : nullptr;
        double x = 0, y = 0;
        m_doc->freeSpot(r, x, y, seed);
        s = m_doc->addHandSystem(x, y, StarKind::G, seed ^ 0x5bd1e995u);
        m_doc->solo.insert(s->id);
        s->gone = true;
        if (r) r->systems.insert(s->id);
    }
    Body* p = addPlanet(*s, t, orbitA(*s, t, int(planetsOf(*s).size())), seed);
    p->moons.clear();   // o gerador dá luas de brinde; aqui só existe o que o autor cria
    s->edited = true;
    m_doc->layoutSystem(*s);
    m_doc->place(*s);
    const QString key = keyP(*s, *p);
    if (!name.trimmed().isEmpty()) m_doc->userNames.insert(key, name.trimmed());
    if (ghost) m_doc->ghosts.insert(key);
    auto held = std::make_shared<std::unique_ptr<Body>>();
    undoable(tr("%1 nasceu.").arg(label(key)), [d, s, p, held] { *held = removeBody(*s, p); d->layoutSystem(*s); }, [d, s, held] { restoreBody(*s, std::move(*held), nullptr); d->layoutSystem(*s); });
    m_justMade = key;
    changed(!m_doc->isSolo(*s));
    return key;
}
QString Livre::createSystem(const QString& name, const QString& parent, StarKind kind, bool ghost)
{
    Region* r = kindOf(parent) == LK::Terr ? m_doc->resolve(parent).region : nullptr;
    const uint32_t seed = randSeed();
    double x = 0, y = 0;
    m_doc->freeSpot(r, x, y, seed);
    System* s = m_doc->addHandSystem(x, y, kind, seed);
    if (r) r->systems.insert(s->id);
    const QString key = keyS(*s);
    if (!name.trimmed().isEmpty()) m_doc->userNames.insert(key, name.trimmed());
    if (ghost) m_doc->ghosts.insert(key);
    Doc* d = m_doc;
    undoable(tr("%1 nasceu.").arg(label(key)), [d, s] { s->gone = true; d->layoutSystems(); }, [d, s] { s->gone = false; d->layoutSystems(); });
    m_justMade = key;
    changed();
    return key;
}
QString Livre::createTerritory(const QString& name)
{
    Region* r = m_doc->newRegion(name.trimmed().isEmpty() ? tr("Território novo") : name.trimmed(), COLORS[int(m_doc->regions.size() * 7) % COLORS.size()], {});
    const QString key = r->key;
    Doc* d = m_doc;
    auto held = std::make_shared<std::unique_ptr<Region>>();
    undoable(tr("%1 nasceu.").arg(r->name), [d, key, held] {
        for (size_t i = 0; i < d->regions.size(); i++) if (d->regions[i]->key == key) { *held = std::move(d->regions[i]); d->regions.erase(d->regions.begin() + long(i)); break; }
    }, [d, held] { if (*held) d->regions.push_back(std::move(*held)); });
    m_justMade = key;
    changed();
    return key;
}
QString Livre::createMoon(const QString& name, const QString& planet, Type t, bool ghost)
{
    const Ref o = m_doc->resolve(planet);
    if (o.kind != RefKind::Planet) return QString();
    System* s = o.sys;
    Body* p = o.body;
    Body* m = addMoon(*s, *p, t, randSeed());
    s->edited = true;
    m_doc->layoutSystem(*s);
    m_doc->place(*s);
    const QString key = keyM(*s, *p, *m);
    if (!name.trimmed().isEmpty()) m_doc->userNames.insert(key, name.trimmed());
    if (ghost) m_doc->ghosts.insert(key);
    Doc* d = m_doc;
    auto held = std::make_shared<std::unique_ptr<Body>>();
    undoable(tr("%1 agora orbita %2.").arg(label(key), label(planet)), [d, s, m, held] { *held = removeBody(*s, m); d->layoutSystem(*s); }, [d, s, p, held] { restoreBody(*s, std::move(*held), p); d->layoutSystem(*s); });
    m_justMade = key;
    changed(!m_doc->isSolo(*s));
    return key;
}
QString Livre::createPlace(const QString& name, const QString& parent, const QString& form, bool ghost)
{
    Group* g = m_doc->newGroup(parent, name.trimmed().isEmpty() ? tr("Lugar sem nome") : name.trimmed(), QStringLiteral("Lugar"));
    g->form = form;
    const QString key = g->key;
    if (ghost) m_doc->ghosts.insert(key);
    Doc* d = m_doc;
    auto held = std::make_shared<std::unique_ptr<Group>>();
    undoable(tr("%1 nasceu.").arg(g->name), [d, key, held] {
        for (size_t i = 0; i < d->groups.size(); i++) if (d->groups[i]->key == key) { *held = std::move(d->groups[i]); d->groups.erase(d->groups.begin() + long(i)); break; }
    }, [d, held] { if (*held) d->groups.push_back(std::move(*held)); });
    m_justMade = key;
    changed(false);
    return key;
}
QString Livre::structName(const std::string& type, const QString& parent) const
{
    const StructDef* def = structOf(type);
    const QString base = tr("%1 de %2").arg(def ? catT(def->pre) : tr("Construção"), label(parent));
    QSet<QString> taken;
    for (const QString& c : children(parent)) taken.insert(label(c));
    if (!taken.contains(base)) return base;
    for (int n = 2;; n++) if (!taken.contains(QStringLiteral("%1 %2").arg(base).arg(n))) return QStringLiteral("%1 %2").arg(base).arg(n);
}
QString Livre::createBuild(const std::string& type, const QString& parent, bool ghost)
{
    const Ref o = m_doc->resolve(parent);
    const StructDef* def = structOf(type);
    if (!def || !o.sys) return QString();
    auto b = std::make_unique<Build>();
    b->key = QStringLiteral("B%1").arg(m_doc->seq++);
    b->type = type; b->seed = randSeed(); b->scale = 1; b->size = def->sizes ? QStringLiteral("m") : QString();
    b->rot0 = (randSeed() % 6283) / 1000.0;
    b->sys = o.sys->id;
    const double simT = m_doc->simT;
    if (o.kind == RefKind::Planet) {
        b->host = uidOf(*o.body);
        int n = 0;
        for (const auto& x : m_doc->builds) if (x->sys == b->sys && x->host == b->host) n++;
        b->rp = 1.9 + 0.35 * n;
        b->w = SPEED_MOON / (1 + b->rp * 0.3);
        const double ang = (randSeed() % 6283) / 1000.0;
        b->a0 = type == "elevador" ? ang - simT * 0.02 : ang - b->w * simT;
    } else {
        b->host = -1;
        b->rr = def->mega && type != "mundoAnel" ? 0 : 0.55 + 0.08 * double(m_doc->buildsIn(*o.sys).size() % 5);
        if (!def->mega) { b->w = SPEED_PLANET * 0.6; b->a0 = (randSeed() % 6283) / 1000.0 - b->w * simT; }
    }
    const QString key = b->key, name = structName(type, parent);
    Build* raw = m_doc->addBuild(std::move(b));
    m_doc->userNames.insert(key, name);
    if (ghost) m_doc->ghosts.insert(key);
    Doc* d = m_doc;
    auto held = std::make_shared<std::unique_ptr<Build>>();
    const QString verb = type == "anelOrbital" ? tr("%1 agora envolve %2.") : type == "elevador" ? tr("%1 agora sobe de %2.") : tr("%1 agora orbita %2.");
    undoable(verb.arg(name, label(parent)), [d, raw, held] { *held = d->dropBuild(raw); }, [d, held] { d->addBuild(std::move(*held)); });
    m_justMade = key;
    changed(!m_doc->isSolo(*o.sys));
    return key;
}
QString Livre::createDoc(const QString& name, const QString& parent, bool gaveta, const QString& drawer)
{
    const DocLink* pd = docOf(parent);
    auto d = std::make_unique<DocLink>();
    d->id = m_doc->seq++;
    d->owner = pd ? pd->owner : parent;
    d->parent = pd ? pd->id : 0;
    d->title = name.trimmed().isEmpty() ? Panel::tr("Documento sem título") : name.trimmed();
    d->gaveta = gaveta;
    d->drawer = gaveta ? drawer : QString();
    DocLink* raw = d.get();
    m_doc->docs.push_back(std::move(d));
    m_panel->docMaterialize(raw);
    m_panel->m_lastDest = gaveta ? QStringLiteral("g:") + drawer : QStringLiteral("criador");
    changed(false);
    return docKey(raw);
}
QString Livre::createLink(const QString& a, const QString& b)
{
    auto* places = m_panel->m_link.places;
    if (!places) return QString();
    const QString pa = m_panel->placeOfKey(a, true), pb = m_panel->placeOfKey(b, true);
    if (pa.isEmpty() || pb.isEmpty() || pa == pb) return QString();
    QString id;
    if (const auto* l = places->linkBetween(pa, pb)) id = l->id;
    if (id.isEmpty()) {
        QString color;
        const QString terr = kindOf(a) == LK::Terr ? a : ancestor(a, LK::Terr);
        if (const Region* r = terr.isEmpty() ? nullptr : m_doc->resolve(terr).region) color = r->color;
        id = places->addLink(pa, pb, color.isEmpty() ? QStringLiteral("#7fb2e6") : color);
        places->save();
    }
    changed(false);
    return QStringLiteral("X") + id;
}

// ── mover ──
bool Livre::canDrop(const QString& k, const QString& parent) const
{
    const LK t = kindOf(k), pt = kindOf(parent);
    if (!valid(k) || isGhost(k) || parentKey(k) == parent) return false;
    if (t == LK::Sys) return parent.isEmpty() || pt == LK::Terr;
    if (t == LK::Planet) return parent.isEmpty() || pt == LK::Terr || pt == LK::Sys;
    return false;
}
void Livre::moveTo(const QString& k, const QString& parent)
{
    if (!canDrop(k, parent)) return;
    const QString from = parentKey(k);
    // move de verdade e devolve a chave nova (o mundo que muda de sistema muda de chave)
    const std::function<QString(const QString&, const QString&)> raw = [this](const QString& key, const QString& to) -> QString {
        const LK t = kindOf(key);
        Region* target = kindOf(to) == LK::Terr ? m_doc->resolve(to).region : nullptr;
        if (t == LK::Sys) {
            System* s = sysOf(key);
            for (auto& r : m_doc->regions) r->systems.remove(s->id);
            if (target) target->systems.insert(s->id);
            return key;
        }
        const Ref o = m_doc->resolve(key);
        if (o.kind != RefKind::Planet) return key;
        System* s = o.sys;
        Body* b = o.body;
        const bool solo = m_doc->isSolo(*s);
        if (kindOf(to) != LK::Sys && solo) {
            // continua solto: só troca de território
            for (auto& r : m_doc->regions) r->systems.remove(s->id);
            if (target) target->systems.insert(s->id);
            return key;
        }
        System* dest = nullptr;
        if (kindOf(to) == LK::Sys) dest = sysOf(to);
        else {
            double x = 0, y = 0;
            m_doc->freeSpot(target, x, y, randSeed());
            dest = m_doc->addHandSystem(x, y, StarKind::G, randSeed());
            m_doc->solo.insert(dest->id);
            dest->gone = true;
            if (target) target->systems.insert(dest->id);
        }
        if (!dest || dest == s) return key;
        // as peças em volta vão junto (o rekey acerta sys e host delas)
        std::unique_ptr<Body> body = removeBody(*s, b);
        int mx = 0;
        for (const auto& x : dest->bodies) if (x->body == BodyKind::Planeta) mx = std::max(mx, uidOf(*x));
        body->uid = mx + 1;
        body->a = orbitA(*dest, body->type, int(planetsOf(*dest).size()));
        body->layHasW = false;
        Body* nb = body.get();
        restoreBody(*dest, std::move(body), nullptr);
        s->edited = true; dest->edited = true;
        if (solo) for (auto& r : m_doc->regions) r->systems.remove(s->id);
        m_doc->layoutSystem(*s);
        m_doc->layoutSystem(*dest);
        const QString nk = keyP(*dest, *nb);
        m_doc->rekey(key, nk);
        rewriteMentions(key, nk);
        return nk;
    };
    const QString nk = raw(k, parent);
    // capital de território só vale dentro do território
    if (isCapital(nk) && isWorld(nk)) {
        const QString terr = ancestor(nk, LK::Terr);
        bool clash = terr.isEmpty();
        for (const QString& c : m_doc->capitals) if (c != nk && isWorld(c) && !terr.isEmpty() && ancestor(c, LK::Terr) == terr) clash = true;
        if (clash) m_doc->capitals.remove(nk);
    }
    auto cur = std::make_shared<QString>(nk);
    undoable(tr("%1 agora está em %2.").arg(label(nk), parent.isEmpty() ? tr("Soltos") : label(parent)),
        [this, raw, cur, from] { *cur = raw(*cur, from); },
        [this, raw, cur, parent] { *cur = raw(*cur, parent); });
    if (m_sel == k) m_sel = nk;
    m_justMade = nk;
    changed();
}

// ── tirar ──
void Livre::removeItem(const QString& k)
{
    const LK t = kindOf(k);
    const QString name = label(k), parent = parentKey(k);
    Doc* d = m_doc;
    if (t == LK::Planet || t == LK::Moon || t == LK::Sys || t == LK::Build) {
        m_canvas->removeThing(k);
    } else if (t == LK::Terr) {
        size_t i = 0;
        for (; i < d->regions.size(); i++) if (d->regions[i]->key == k) break;
        if (i >= d->regions.size()) return;
        auto held = std::make_shared<std::unique_ptr<Region>>(std::move(d->regions[i]));
        d->regions.erase(d->regions.begin() + long(i));
        const QString key = k;
        undoable(name, [d, held, i] { d->regions.insert(d->regions.begin() + long(std::min(i, d->regions.size())), std::move(*held)); },
            [d, held, key] { for (size_t j = 0; j < d->regions.size(); j++) if (d->regions[j]->key == key) { *held = std::move(d->regions[j]); d->regions.erase(d->regions.begin() + long(j)); break; } });
    } else if (t == LK::Place) {
        size_t i = 0;
        for (; i < d->groups.size(); i++) if (d->groups[i]->key == k) break;
        if (i >= d->groups.size()) return;
        auto held = std::make_shared<std::unique_ptr<Group>>(std::move(d->groups[i]));
        d->groups.erase(d->groups.begin() + long(i));
        const QString key = k;
        undoable(name, [d, held, i] { d->groups.insert(d->groups.begin() + long(std::min(i, d->groups.size())), std::move(*held)); },
            [d, held, key] { for (size_t j = 0; j < d->groups.size(); j++) if (d->groups[j]->key == key) { *held = std::move(d->groups[j]); d->groups.erase(d->groups.begin() + long(j)); break; } });
    } else if (t == LK::Doc) {
        // O documento continua no projeto (gaveta ou Criador): aqui só se desfaz a ligação com o lugar.
        DocLink* dl = docOf(k);
        if (!dl) return;
        const int id = dl->id, up = dl->parent;
        std::vector<DocLink*> kids;
        for (const auto& x : d->docs) if (x->parent == id) kids.push_back(x.get());
        for (DocLink* x : kids) x->parent = up;
        size_t i = 0;
        for (; i < d->docs.size(); i++) if (d->docs[i].get() == dl) break;
        auto held = std::make_shared<std::unique_ptr<DocLink>>(std::move(d->docs[i]));
        d->docs.erase(d->docs.begin() + long(i));
        undoable(name, [d, held, kids, id] { for (DocLink* x : kids) x->parent = id; d->docs.push_back(std::move(*held)); },
            [d, held, kids, up, id] { for (DocLink* x : kids) x->parent = up; for (size_t j = 0; j < d->docs.size(); j++) if (d->docs[j]->id == id) { *held = std::move(d->docs[j]); d->docs.erase(d->docs.begin() + long(j)); break; } });
    } else if (t == LK::Link) {
        auto* places = m_panel->m_link.places;
        const auto* l = places ? places->link(k.mid(1)) : nullptr;
        if (!l) return;
        const TerritorioStore::TerritorioLink copy = *l;
        const QString type = d->linkTypes.value(copy.id);
        places->removeLink(copy.id);
        places->save();
        auto id = std::make_shared<QString>(copy.id);
        undoable(name, [places, copy, id, d, type] {
            *id = places->addLink(copy.fromTerritorioId, copy.toTerritorioId, copy.color);
            places->updateLinkContent(*id, copy.docContent);
            if (!type.isEmpty()) d->linkTypes.insert(*id, type);
            places->save();
        }, [places, id] { places->removeLink(*id); places->save(); });
    } else return;
    m_doc->ghosts.remove(k);
    if (m_sel == k || isInside(m_sel, k)) m_sel = parent;
    changed();
    const QString msg = t == LK::Doc ? tr("%1 saiu daqui. O documento continua no projeto.").arg(name) : t == LK::Link ? tr("Conexão desfeita.") : tr("%1 saiu da galáxia.").arg(name);
    QTimer::singleShot(0, this, [this, msg]() { toast(msg, true); render(); });
}
void Livre::accept(const QString& k) { m_doc->ghosts.remove(k); changed(); render(); }
void Livre::dismiss(const QString& k)
{
    removeItem(k);
}

// ── capital ──
bool Livre::canBeCapital(const QString& k) const
{
    const LK t = kindOf(k);
    if (t == LK::Planet || t == LK::Moon) return !ancestor(k, LK::Terr).isEmpty();
    return (t == LK::Place || t == LK::Build) && !parentKey(k).isEmpty();
}
QString Livre::capitalOf(const QString& terr) const
{
    for (const QString& c : m_doc->capitals) if (isWorld(c) && valid(c) && ancestor(c, LK::Terr) == terr) return c;
    return QString();
}
void Livre::toggleCapital(const QString& k)
{
    if (m_doc->capitals.remove(k)) { changed(false); render(); return; }
    const bool world = isWorld(k);
    const QString scope = world ? ancestor(k, LK::Terr) : parentKey(k);
    for (const QString& c : QSet<QString>(m_doc->capitals)) {
        if (world ? (isWorld(c) && ancestor(c, LK::Terr) == scope) : (!isWorld(c) && parentKey(c) == scope)) m_doc->capitals.remove(c);
    }
    m_doc->capitals.insert(k);
    changed(false);
    render();
}

// ── mexer no mundo ──
void Livre::setWorldType(const QString& k, Type t)
{
    Body* b = bodyOf(k);
    System* s = sysOf(k);
    if (!b || !s || b->type == t) return;
    const Type was = b->type;
    const auto apply = [this, b, s](Type to) {
        m_canvas->invalidateBody(*b);
        setType(*s, *b, to);
        if (isArtificial(to)) b->rings.reset();
        b->texRev++;
        m_doc->layoutSystem(*s);
    };
    apply(t);
    s->edited = true;
    undoable(tr("Agora é %1.").arg(coreT(typeLabel(t)).toLower()), [apply, was] { apply(was); }, [apply, t] { apply(t); });
    changed();
}
void Livre::setStar(const QString& sysKey, StarKind kind)
{
    System* s = sysOf(sysKey);
    if (!s) return;
    const std::vector<Star> was = s->stars;
    const StarKind wasK = s->kind;
    const auto after = [this, s]() {
        refreshSystem(*s);
        s->edited = true;
        for (Body* b : planetsOf(*s)) if (isLiving(b->type)) { tuneTemperature(*b, b->temp); m_canvas->invalidateBody(*b); b->texRev++; }
    };
    s->stars = { starOf(kind, 0, s->seed + randSeed() % 9973) };
    s->kind = kind;
    after();
    auto now = std::make_shared<std::vector<Star>>(s->stars);
    undoable(tr("Estrela trocada."), [s, was, wasK, after] { s->stars = was; s->kind = wasK; after(); }, [s, now, kind, after] { s->stars = *now; s->kind = kind; after(); });
    changed();
}
void Livre::reseed(const QString& k)
{
    if (Body* b = bodyOf(k)) {
        System* s = sysOf(k);
        const Look was = b->look;
        m_canvas->invalidateBody(*b);
        regenLook(*s, *b, randSeed());
        s->edited = true;
        undoable(tr("Variação nova."), [this, b, was] { m_canvas->invalidateBody(*b); b->look = was; b->texRev++; }, nullptr);
        changed();
        return;
    }
    const Ref o = m_doc->resolve(k);
    if (o.kind == RefKind::Build) {
        const uint32_t was = o.build->seed;
        Build* b = o.build;
        b->seed = randSeed();
        undoable(tr("Variação nova."), [b, was] { b->seed = was; }, nullptr);
        changed();
    }
}
void Livre::setBuildType(const QString& k, const std::string& type)
{
    const Ref o = m_doc->resolve(k);
    if (o.kind != RefKind::Build || o.build->type == type) return;
    Build* b = o.build;
    const std::string was = b->type;
    b->type = type;
    undoable(tr("Modelo trocado."), [b, was] { b->type = was; }, [b, type] { b->type = type; });
    changed();
}
void Livre::rename(const QString& k, const QString& name)
{
    const QString n = name.trimmed();
    switch (kindOf(k)) {
    case LK::Terr: { if (Region* r = m_doc->resolve(k).region) r->name = n; break; }
    case LK::Place: { if (Group* g = m_doc->resolve(k).group) g->name = n; break; }
    case LK::Doc: { if (DocLink* d = docOf(k)) m_panel->docSetTitle(d, n); return; }
    case LK::Link: case LK::None: case LK::Gal: return;
    default: if (n.isEmpty()) m_doc->userNames.remove(k); else m_doc->userNames.insert(k, n); break;
    }
    // o Lugar do Criador de Mundos tem o mesmo nome
    const QString pid = m_doc->placeIds.value(k);
    if (!pid.isEmpty() && !n.isEmpty() && m_panel->m_link.places && m_panel->m_link.places->territorio(pid)) { m_panel->m_link.places->updateTerritorio(pid, n); m_panel->m_docPlacesDirty = true; }
    m_doc->rev++;
    changed();
}

// ── a folha de cada coisa: mora no projeto ──
// Planeta, sistema, território, lugar e construção: o texto é o do Lugar deles no Criador de Mundos.
// Documento: a gaveta ou o Criador, como no Explorar. Conexão: o vínculo do Criador.
QString Livre::sheetHtml(const QString& k) const
{
    auto* places = m_panel->m_link.places;
    switch (kindOf(k)) {
    case LK::Doc: { const DocLink* d = docOf(k); return d ? m_panel->docHtml(d) : QString(); }
    case LK::Link: { const auto* l = places ? places->link(k.mid(1)) : nullptr; return l ? l->docContent : QString(); }
    default: {
        const QString pid = m_doc->placeIds.value(k);
        const auto* t = places && !pid.isEmpty() ? places->territorio(pid) : nullptr;
        return t ? t->content : QString();
    }
    }
}
void Livre::writeSheet(const QString& k, const QString& html)
{
    auto* places = m_panel->m_link.places;
    switch (kindOf(k)) {
    case LK::Doc: if (DocLink* d = docOf(k)) m_panel->docWriteHtml(d, html); return;
    case LK::Link: if (places) { places->updateLinkContent(k.mid(1), html); m_panel->m_docPlacesDirty = true; m_panel->scheduleSave(); } return;
    case LK::None: case LK::Gal: return;
    default: {
        if (!places) return;
        if (html.isEmpty() && m_doc->placeIds.value(k).isEmpty()) return;
        const QString pid = m_panel->placeOfKey(k, true);
        if (pid.isEmpty()) return;
        places->updateTerritorioContent(pid, html);
        m_panel->m_docPlacesDirty = true;
        m_panel->scheduleSave();
    }
    }
}

// ── a ficha técnica ──
// O motor dá os números; o Gerar só preenche o que o autor deixou vazio, e fica tracejado até ele aceitar.
QHash<QString, QString> Livre::motorFicha(const QString& k) const
{
    QHash<QString, QString> out;
    const Body* b = bodyOf(k);
    if (!b) return out;
    if (b->body == BodyKind::Lua) {
        out.insert(QStringLiteral("raio"), QStringLiteral("%1 km").arg(nf(b->radiusKm, 0)));
        out.insert(QStringLiteral("grav"), QStringLiteral("%1 g").arg(nf(b->gravity, b->gravity < 0.1 ? 3 : 2)));
        if (b->temp != 0) out.insert(QStringLiteral("temp"), celsius(b->temp));
        if (!b->atm.empty()) out.insert(QStringLiteral("atm"), atmText(*b));
        out.insert(QStringLiteral("dist"), QStringLiteral("%1 km").arg(nf(std::round(b->dist / 100) * 100)));
        out.insert(QStringLiteral("ano"), periodText(b->P));
        out.insert(QStringLiteral("rot"), tr("Travada: mostra sempre a mesma face"));
        return out;
    }
    out.insert(QStringLiteral("massa"), QStringLiteral("%1 M⊕").arg(sig(b->mass)));
    out.insert(QStringLiteral("raio"), QStringLiteral("%1 km (%2 R⊕)").arg(nf(std::round(b->radius * 6371 / 10) * 10), sig(b->radius)));
    if (b->people > 0) out.insert(QStringLiteral("pop"), fmtCount(b->people));
    out.insert(QStringLiteral("grav"), QStringLiteral("%1 g").arg(nf(b->gravity, 2)));
    out.insert(QStringLiteral("dens"), QStringLiteral("%1 g/cm³").arg(nf(b->density, 1)));
    out.insert(QStringLiteral("temp"), celsius(b->temp));
    if (!b->atm.empty()) out.insert(QStringLiteral("atm"), atmText(*b));
    if (b->look.vegKeyV != Veg::None) out.insert(QStringLiteral("veg"), coreT(vegLabel(b->look.vegKeyV)));
    out.insert(QStringLiteral("dist"), QStringLiteral("%1 UA").arg(sig(b->a)));
    out.insert(QStringLiteral("exc"), nf(b->e, 3));
    out.insert(QStringLiteral("ano"), periodText(b->P));
    out.insert(QStringLiteral("dia"), b->locked ? tr("Igual ao ano") : periodText(b->dayH / 24));
    out.insert(QStringLiteral("eixo"), QStringLiteral("%1°").arg(nf(b->tilt, 0)));
    return out;
}
void Livre::fillFicha(const QString& k)
{
    const QHash<QString, QString> got = motorFicha(k), mine = m_doc->fichas.value(k);
    QHash<QString, QString> gen;
    for (auto it = got.constBegin(); it != got.constEnd(); ++it) if (mine.value(it.key()).trimmed().isEmpty()) gen.insert(it.key(), it.value());
    if (gen.isEmpty()) { m_doc->fichasGen.remove(k); toast(tr("A ficha já está toda preenchida. Nada foi gerado."), false); }
    else m_doc->fichasGen.insert(k, gen);
    m_fichaOpen = true;
    changed(false);
}

// ── o Gerar: só faz o que pedir, e nada entra sem o sim ──
std::vector<std::pair<QString, std::function<void()>>> Livre::orders(const QString& k)
{
    std::vector<std::pair<QString, std::function<void()>>> out;
    const LK t = kindOf(k);
    const auto natural = []() { return NATURAL[randSeed() % NATURAL.size()]; };
    if (t == LK::None || t == LK::Gal) {
        out.push_back({ tr("Um planeta qualquer"), [this, natural]() { createWorld(ghostName(QStringLiteral("planet")), QString(), natural(), true); } });
        out.push_back({ tr("Um sistema"), [this]() { openComposer(QString()); } });
        return out;
    }
    if (t == LK::Planet || t == LK::Moon) {
        const QString sk = solarKey(k);
        if (t == LK::Planet && sk.isEmpty()) out.push_back({ tr("Um sistema para %1").arg(label(k)), [this, k]() { openComposer(k); } });
        if (!sk.isEmpty() && t == LK::Planet) out.push_back({ tr("Mais um planeta neste sistema"), [this, sk, natural]() { createWorld(ghostName(QStringLiteral("planet")), sk, natural(), true); } });
        out.push_back({ tr("Três lugares em %1").arg(label(k)), [this, k]() { for (const QString& f : { tr("Cidade"), tr("Região"), tr("Base") }) createPlace(ghostName(QStringLiteral("planet")), k, f, true); } });
        if (t == LK::Planet) out.push_back({ tr("Uma cidade orbital"), [this, k]() { createBuild("cidade", k, true); } });
        out.push_back({ tr("A ficha técnica"), [this, k]() { fillFicha(k); } });
        if (t == LK::Planet) out.push_back({ tr("Frota e satélites"), [this, k]() { createBuild("frota", k, true); createBuild("sat", k, true); } });
        return out;
    }
    if (t == LK::Sys) {
        out.push_back({ tr("Mais um planeta aqui"), [this, k, natural]() { createWorld(ghostName(QStringLiteral("planet")), k, natural(), true); } });
        out.push_back({ tr("Outra estrela para %1").arg(label(k)), [this, k]() { setStar(k, STAR_KINDS[randSeed() % 7]); } });
        return out;
    }
    if (t == LK::Terr) {
        out.push_back({ tr("Um sistema aqui dentro"), [this, k]() { openComposer(k); } });
        out.push_back({ tr("Um planeta aqui dentro"), [this, k, natural]() { createWorld(ghostName(QStringLiteral("planet")), k, natural(), true); } });
    }
    return out;
}

} // namespace galaxy
