#include "GalaxyDoc.h"
#include "GalaxyPresets.h"
#include "GalaxyStructures.h"

#include <QCoreApplication>
#include <QJsonArray>

#include <algorithm>

namespace galaxy {

std::vector<Body*> planetsOf(const System& s)
{
    std::vector<Body*> out;
    for (const auto& b : s.bodies) if (b->body == BodyKind::Planeta) out.push_back(b.get());
    return out;
}
int livingCount(const System& s)
{
    int n = 0;
    for (const auto& b : s.bodies) {
        if (b->body != BodyKind::Planeta) continue;
        if (isLiving(b->type)) n++;
        for (const auto& m : b->moons) if (isLiving(m->type)) n++;
    }
    return n;
}
std::array<int, 3> sysColor(const System& s) { return s.isCore ? std::array<int, 3> { 255, 196, 140 } : s.stars[0].color; }

QImage Doc::galaxyImage(const Galaxy& g, int T)
{
    const std::vector<uint8_t> raw = buildGalaxyImage(g, T);
    QImage img(T, T, QImage::Format_RGB32);
    for (int y = 0; y < T; y++) {
        QRgb* d = reinterpret_cast<QRgb*>(img.scanLine(y));
        const uint8_t* s = raw.data() + size_t(y) * size_t(T) * 4;
        for (int x = 0; x < T; x++) d[x] = qRgb(s[x * 4], s[x * 4 + 1], s[x * 4 + 2]);
    }
    return img;
}

void Doc::generate(const GalOpts& o)
{
    opts = o;
    GalaxyOpts g;
    g.seed = o.seed; g.style = o.style; g.size = o.size; g.arms = o.arms; g.life = o.life; g.count = o.count; g.mix = o.mix;
    if (const Preset* p = presetById(o.preset)) g.tune = p->tune;
    gal = makeGalaxy(g);
    galImg = galaxyImage(*gal, 1400);
    names = std::make_unique<NameGen>(gal->seed);
    genNames.clear();
    placeIds.clear(); userNames.clear(); regions.clear(); groups.clear(); docs.clear(); builds.clear(); labels.clear(); hidden.clear();
    addedBatches.clear();
    veiled.clear(); autoSys.clear();
    seq = 1;
    simT = 0;
    if (!o.preset.isEmpty()) applyPreset();
    layoutSystems();
    if (o.organic) setOrganic(true);
    rev++;
}

// ── Semeação Orgânica ──
// O número do sistema dentro de uma chave de lugar (S12, P12.3, M12.3.1, C12.1, K12.1).
static int systemOfKey(const QString& key)
{
    if (key.size() < 2) return -1;
    const QChar t = key[0];
    if (t != QLatin1Char('S') && t != QLatin1Char('P') && t != QLatin1Char('M') && t != QLatin1Char('C') && t != QLatin1Char('K')) return -1;
    bool ok = false;
    const int id = key.mid(1, key.indexOf(QLatin1Char('.')) - 1).toInt(&ok);
    return ok ? id : -1;
}
QSet<int> Doc::organicKeep() const
{
    QSet<int> keep;
    const auto add = [&](const QString& key) { const int id = systemOfKey(key); if (id >= 0) keep.insert(id); };
    for (auto it = userNames.constBegin(); it != userNames.constEnd(); ++it) add(it.key());
    for (auto it = placeIds.constBegin(); it != placeIds.constEnd(); ++it) add(it.key());
    for (const auto& d : docs) add(d->owner);
    for (const auto& g : groups) add(g->parent);
    for (const auto& b : builds) if (b->sys >= 0) keep.insert(b->sys);
    for (const auto& r : regions) keep.unite(r->systems);
    for (const auto& s : gal->systems) if ((s->edited && !autoSys.contains(s->id)) || s->hand) keep.insert(s->id);
    return keep;
}
void Doc::setOrganic(bool on)
{
    opts.organic = on;
    if (on) {
        const QSet<int> keep = organicKeep();
        for (const auto& s : gal->systems) if (!s->gone && !s->isCore && !keep.contains(s->id)) veil(*s);
    } else {
        for (int id : veiled) gal->systems[size_t(id)]->gone = false;
        veiled.clear();
    }
    rev++;
}
void Doc::restoreVeil(bool organic, const QSet<int>& ids)
{
    for (int id : veiled) gal->systems[size_t(id)]->gone = false;
    veiled = ids;
    for (int id : veiled) gal->systems[size_t(id)]->gone = true;
    opts.organic = organic;
    rev++;
}

// O Sistema Solar entra no lugar do sistema mais perto de onde ele fica, a 26 mil anos-luz do centro, entre Sagitário e Perseu.
static void buildSolarSystem(Doc& d, const Preset& P)
{
    Galaxy& g = *d.gal;
    const Tune& t = P.tune;
    const double R0 = 0.24, rs = 26000 / (t.ly / 2);
    const double th = t.angle + t.k * std::log(rs / R0) + PI_D / 4, x = std::cos(th) * rs, y = std::sin(th) * rs;
    System* s = nullptr;
    double best = 1e9;
    for (const auto& q : g.systems) {
        if (q->isCore) continue;
        const double dd = std::hypot(q->x - x, q->y - y);
        if (dd < best) { best = dd; s = q.get(); }
    }
    if (!s) return;
    s->x = x; s->y = y; s->rCore = rs; s->kind = StarKind::G; s->bodies.clear(); s->sep = 0; s->binP = 0; s->age = 4.6;
    s->belts.clear();
    { Belt b; b.a0 = 2.2; b.a1 = 3.3; b.n = 900; s->belts.push_back(b); }
    { Belt b; b.kuiper = true; b.a0 = 40; b.a1 = 55; b.n = 1100; s->belts.push_back(b); }
    Star st = starOf(StarKind::G, 1, 1977);
    st.mass = 1; st.temp = 5772; st.lum = 1; st.radius = 1; st.color = starColor(5772); st.spec = "G2 V"; st.age = 4.6;
    s->stars = { st };
    refreshSystem(*s);
    d.userNames.insert(keyS(*s), QCoreApplication::translate("galaxy::Preset", "Sol"));
    int i = 0;
    for (const SolarPlanet& q : solarPlanets()) {
        Body* p = addPlanet(*s, q.type, q.a, uint32_t(3000 + i * 17));
        p->mass = q.M; p->radius = q.R; p->dayH = q.day; p->temp = q.T; p->tilt = q.tilt; p->atm = q.atm; p->e = q.e; p->retro = q.retro; p->locked = false; p->rings.reset(); p->moons.clear();
        p->hz = q.a < s->hz[0] ? Hz::Quente : q.a > s->hz[1] ? Hz::Fria : Hz::Dentro;
        p->gravity = q.M / (q.R * q.R); p->density = (5.51 * q.M) / (q.R * q.R * q.R);
        p->frame = frameOf(q.tilt, p->az);
        if (q.rings) setRings(*p, true, uint32_t(3000 + i * 19));
        if (q.style != LookStyle::None) restyle(*p, q.style);
        applySolarLook(q.name, p->look);
        if (q.water != 0) setWater(*p, q.water);
        if (q.people != 0) { p->people = q.people; p->look.pop = lightLevel(q.people, q.R); }
        d.userNames.insert(keyP(*s, *p), QCoreApplication::translate("galaxy::Preset", q.name));
        int j = 0;
        for (const SolarMoon& mq : q.moons) {
            Body* m = addMoon(*s, *p, mq.type, uint32_t(4000 + i * 31 + j));
            const double km = mq.r * 1000;
            m->dist = mq.d; m->radiusKm = mq.r; m->P = mq.P; m->gravity = mq.g; m->massKg = (mq.g * 9.81 * km * km) / 6.674e-11;
            d.userNames.insert(keyM(*s, *p, *m), QCoreApplication::translate("galaxy::Preset", mq.name));
            j++;
        }
        i++;
    }
    // O Halley: 76 anos, indo de dentro da órbita de Vênus até depois de Netuno.
    auto c = std::make_unique<Body>();
    c->body = BodyKind::Cometa; c->type = Type::Cometa; c->a = 17.8; c->e = 0.967; c->w = 1.95; c->M0 = 0.4; c->P = 27510; c->radius = 0.0009; c->idx = 1; c->look.seed = 1986;
    s->bodies.push_back(std::move(c));
    d.userNames.insert(QStringLiteral("C%1.1").arg(s->id), QCoreApplication::translate("galaxy::Preset", "Cometa Halley"));
    s->planetCount = int(solarPlanets().size());
    s->amax = 55;
    // Os braços, medidos em cima da mesma espiral.
    const auto armAt = [&](int n, double r, double& ax, double& ay) { const double a = t.angle + t.k * std::log(r / R0) + (n * PI_D) / 2; ax = std::cos(a) * r; ay = std::sin(a) * r; };
    const auto arm = [&](const char* text, int n, double r) { double ax, ay; armAt(n, r, ax, ay); d.labels.push_back({ QStringLiteral("L%1").arg(d.seq++), ax, ay, QCoreApplication::translate("galaxy::Preset", text) }); };
    arm(QT_TRANSLATE_NOOP("galaxy::Preset", "Braço de Perseu"), 0, 0.74); arm(QT_TRANSLATE_NOOP("galaxy::Preset", "Braço de Sagitário"), 1, 0.5);
    arm(QT_TRANSLATE_NOOP("galaxy::Preset", "Braço de Escudo-Centauro"), 2, 0.62); arm(QT_TRANSLATE_NOOP("galaxy::Preset", "Braço de Norma"), 3, 0.56);
    d.labels.push_back({ QStringLiteral("L%1").arg(d.seq++), x + 0.03, y + 0.085, QCoreApplication::translate("galaxy::Preset", "Braço de Órion") });
}

void Doc::applyPreset()
{
    const Preset* P = presetById(opts.preset);
    if (!P) return;
    if (P->core && !gal->systems.empty() && gal->systems[0]->isCore) userNames.insert(keyS(*gal->systems[0]), QString::fromUtf8(P->core));
    for (const PresetLabel& l : P->labels) labels.push_back({ QStringLiteral("L%1").arg(seq++), l.x, l.y, QCoreApplication::translate("galaxy::Preset", l.text) });
    if (P->solar) buildSolarSystem(*this, *P);
}

Ref Doc::resolve(const QString& key) const
{
    Ref o;
    if (key.isEmpty() || !gal) return o;
    if (key == QLatin1String("GAL")) { o.kind = RefKind::Galaxy; return o; }
    const QChar t = key[0];
    if (t == QLatin1Char('R')) { for (const auto& r : regions) if (r->key == key) { o.kind = RefKind::Region; o.region = r.get(); return o; } return o; }
    if (t == QLatin1Char('G')) { for (const auto& g : groups) if (g->key == key) { o.kind = RefKind::Group; o.group = g.get(); return o; } return o; }
    if (t == QLatin1Char('L')) return o;
    if (t == QLatin1Char('B')) {
        for (const auto& b : builds) if (b->key == key) {
            o.kind = RefKind::Build; o.build = b.get();
            if (b->sys >= 0) {
                o.sys = gal->systems[size_t(b->sys)].get();
                if (b->host >= 0) for (const auto& p : o.sys->bodies) if (p->body == BodyKind::Planeta && (p->uid ? p->uid : p->idx) == b->host) { o.host = p.get(); break; }
            }
            return o;
        }
        return o;
    }
    const QStringList parts = key.mid(1).split(QLatin1Char('.'));
    bool ok = false;
    const int n0 = parts[0].toInt(&ok);
    if (!ok || n0 < 0 || n0 >= int(gal->systems.size())) return o;
    System* sys = gal->systems[size_t(n0)].get();
    if (sys->gone) return o;
    o.sys = sys;
    if (t == QLatin1Char('S')) { o.kind = RefKind::System; return o; }
    const int n1 = parts.size() > 1 ? parts[1].toInt() : 0;
    if (t == QLatin1Char('C')) { for (const auto& b : sys->bodies) if (b->body == BodyKind::Cometa && b->idx == n1) { o.kind = RefKind::Comet; o.comet = b.get(); return o; } o.sys = nullptr; return o; }
    if (t == QLatin1Char('K')) { for (size_t i = 0; i < sys->belts.size(); i++) if ((sys->belts[i].uid ? sys->belts[i].uid : int(i) + 1) == n1) { o.kind = RefKind::Belt; o.belt = &sys->belts[i]; return o; } o.sys = nullptr; return o; }
    Body* body = nullptr;
    for (const auto& b : sys->bodies) if (b->body == BodyKind::Planeta && (b->uid ? b->uid : b->idx) == n1) { body = b.get(); break; }
    if (!body) { o.sys = nullptr; return o; }
    o.body = body;
    if (t == QLatin1Char('P')) { o.kind = RefKind::Planet; return o; }
    const int n2 = parts.size() > 2 ? parts[2].toInt() : 0;
    for (const auto& m : body->moons) if ((m->uid ? m->uid : m->idx) == n2) { o.kind = RefKind::Moon; o.moon = m.get(); return o; }
    o.sys = nullptr; o.body = nullptr;
    return o;
}

Region* Doc::regionOfSys(const System& s) const
{
    for (const auto& r : regions) if (r->systems.contains(s.id)) return r.get();
    return nullptr;
}

// Nome: o que o autor deu; senão, o que a galáxia gerou.
QString Doc::genName(const QString& key)
{
    auto it = genNames.find(key);
    if (it != genNames.end()) return it.value();
    const Ref o = resolve(key);
    QString n;
    const std::string k = key.toStdString();
    switch (o.kind) {
    case RefKind::System: n = o.sys->isCore ? QCoreApplication::translate("galaxy::Doc", "Núcleo galáctico") : QString::fromStdString(names->system(k)); break;
    case RefKind::Planet: n = QString::fromStdString(names->planet(k)); break;
    case RefKind::Moon: n = QString::fromStdString(names->moon(k)); break;
    case RefKind::Build: {

        n = QCoreApplication::translate("galaxy::Catalog", structOf(o.build->type)->pre) + QLatin1Char(' ') + QString::fromStdString(names->moon(k));
        break;
    }
    case RefKind::Comet: n = QCoreApplication::translate("galaxy::Doc", "Cometa %1").arg(QString::fromStdString(names->moon(k))); break;
    case RefKind::Belt: n = (o.belt->kuiper ? QCoreApplication::translate("galaxy::Doc", "Disco de %1") : QCoreApplication::translate("galaxy::Doc", "Cinturão de %1")).arg(QString::fromStdString(names->moon(k))); break;
    default: break;
    }
    genNames.insert(key, n);
    return n;
}
QString Doc::labelOf(const QString& key)
{
    const Ref o = resolve(key);
    if (!o) return QString();
    if (o.kind == RefKind::Galaxy) {
        const QString u = userNames.value(QStringLiteral("GAL"));
        return !u.isEmpty() ? u : QCoreApplication::translate("galaxy::Doc", "Galáxia %1").arg(QCoreApplication::translate("galaxy::Core", styleLabel(gal->style)).toLower());
    }
    if (o.kind == RefKind::Region) return o.region->name;
    if (o.kind == RefKind::Group) return o.group->name;
    const QString u = userNames.value(key);
    return !u.isEmpty() ? u : genName(key);
}
QString Doc::parentOf(const QString& key) const
{
    const Ref o = resolve(key);
    switch (o.kind) {
    case RefKind::Moon: return keyP(*o.sys, *o.body);
    case RefKind::Planet: return keyS(*o.sys);
    case RefKind::System: { const Region* r = regionOfSys(*o.sys); return r ? r->key : QStringLiteral("GAL"); }
    case RefKind::Region: return QStringLiteral("GAL");
    case RefKind::Group: return o.group->parent;
    case RefKind::Build: return o.host ? keyP(*o.sys, *o.host) : o.sys ? keyS(*o.sys) : QStringLiteral("GAL");
    case RefKind::Comet: case RefKind::Belt: return keyS(*o.sys);
    default: return QString();
    }
}
QStringList Doc::pathOf(const QString& key) const
{
    QStringList out;
    for (QString c = key; !c.isEmpty(); c = parentOf(c)) out.prepend(c);
    return out;
}
std::vector<DocLink*> Doc::docsOf(const QString& k) const
{
    std::vector<DocLink*> out;
    for (const auto& d : docs) if (d->owner == k) out.push_back(d.get());
    return out;
}
std::vector<Group*> Doc::groupsOf(const QString& k) const
{
    std::vector<Group*> out;
    for (const auto& g : groups) if (g->parent == k) out.push_back(g.get());
    return out;
}
// Importância: o que o autor mexeu vem primeiro (nome, documento, lugar).
bool Doc::touched(const QString& k) const
{
    if (userNames.contains(k)) return true;
    for (const auto& d : docs) if (d->owner == k) return true;
    for (const auto& g : groups) if (g->parent == k) return true;
    if (k.startsWith(QLatin1Char('S'))) { const Ref o = resolve(k); if (o.sys) for (const auto& b : builds) if (b->sys == o.sys->id) return true; }
    return false;
}
std::vector<Build*> Doc::buildsIn(const System& s) const
{
    std::vector<Build*> out;
    for (const auto& b : builds) if (b->sys == s.id) out.push_back(b.get());
    return out;
}
Build* Doc::addBuild(std::unique_ptr<Build> b)
{
    Build* raw = b.get();
    builds.push_back(std::move(b));
    rev++;
    return raw;
}
std::unique_ptr<Build> Doc::dropBuild(Build* b)
{
    for (size_t i = 0; i < builds.size(); i++) if (builds[i].get() == b) { auto out = std::move(builds[i]); builds.erase(builds.begin() + long(i)); rev++; return out; }
    return nullptr;
}
Region* Doc::newRegion(const QString& name, const QString& color, const std::vector<System*>& systems)
{
    auto r = std::make_unique<Region>();
    r->key = QStringLiteral("R%1").arg(seq++); r->name = name; r->color = color;
    for (const System* s : systems) r->systems.insert(s->id);
    Region* raw = r.get();
    regions.push_back(std::move(r));
    rev++;
    return raw;
}
Group* Doc::newGroup(const QString& parent, const QString& name, const QString& kind)
{
    auto g = std::make_unique<Group>();
    g->key = QStringLiteral("G%1").arg(seq++); g->name = name; g->kind = kind; g->parent = parent;
    Group* raw = g.get();
    groups.push_back(std::move(g));
    rev++;
    return raw;
}

// ── o esquema de cada sistema ──
// Na escala da galáxia o sistema é um ponto; chegando perto, ele se abre num desenho das órbitas, do tamanho
// que cabe entre os vizinhos. A escala muda com o zoom, como num mapa de ruas.
void Doc::layoutSystems()
{
    auto& S = gal->systems;
    // O sistema escondido conta: o desenho de cada um não pode mudar de tamanho quando o vizinho aparece.
    const auto present = [this](const System& q) { return !q.gone || veiled.contains(q.id); };
    for (const auto& s : S) {
        if (!present(*s)) continue;
        double nn = 1;
        for (const auto& o : S) if (o != s && present(*o)) { const double d = std::hypot(o->x - s->x, o->y - s->y); if (d < nn) nn = d; }
        s->layR = std::min(0.03, std::max(0.005, nn * 0.38));
        s->layH = s->z * 1.6;
        layoutSystem(*s);
    }
}
// Refazer as órbitas de um sistema (planeta novo, por exemplo) mantendo cada corpo onde está na volta dele.
void Doc::layoutSystem(System& s)
{
    const auto ps = planetsOf(s);
    const int n = int(ps.size());
    s.layStar = s.layR * (s.isCore ? 0.16 : s.kind == StarKind::Binaria ? 0.11 : 0.085);
    const double gap = n ? (s.layR * 0.8) / n : 1;
    s.layGap = gap;
    for (int i = 0; i < n; i++) {
        Body& p = *ps[size_t(i)];
        const double orb = s.layR * 0.2 + gap * (i + 0.75);
        const double sizeK = std::min(1.5, 0.55 + 0.26 * std::sqrt(p.radius != 0 ? p.radius : 1));
        // Mundo construído tem o tamanho que o dono quis: não se encolhe pra caber entre as órbitas.
        const double pr = isArtificial(p.type) ? s.layR * 0.062 * sizeK * (p.radius > 3 ? 1.35 : 1) : std::min(s.layR * 0.062 * sizeK, gap * 0.36);
        // As luas ficam em volta do planeta, sem invadir a órbita vizinha.
        const int nm = int(p.moons.size());
        const double outer = nm ? pr * (1.9 + 0.62 * (nm - 1)) : 0;
        const double k = outer > gap * 0.46 ? (gap * 0.46) / outer : 1;
        // Sem espaço, as órbitas se apertam, mas nunca pra dentro do planeta.
        const double lo = pr * 1.4, step = nm > 1 ? (std::max(gap * 0.46, lo + (nm - 1) * pr * 0.09) - lo) / (nm - 1) : 0;
        const double w = SPEED_PLANET * std::pow((s.layR * 0.2 + gap * 0.75) / orb, 1.5) * (p.retro ? -1 : 1);
        p.layA0 = !p.layHasW ? p.M0 + i * 0.7 : p.layA0 + (p.layW - w) * simT;
        p.layOrb = orb; p.layR = pr; p.layW = w; p.layHasW = true;
        for (int j = 0; j < nm; j++) {
            Body& m = *p.moons[size_t(j)];
            m.layOrb = k < 1 ? lo + j * step : pr * (1.9 + 0.62 * j);
            if (!m.layHasA0) { m.layA0 = m.M0 + j * 1.3; m.layHasA0 = true; }
            m.layW = SPEED_MOON / (1 + 0.45 * j);
            m.layR = std::max(pr * 0.12, pr * std::min(0.32, 0.1 + std::sqrt((m.radiusKm != 0 ? m.radiusKm : 500) / 2600) * 0.17)) * std::min(1.0, k * 1.2);
        }
        p.layExt = std::max(pr * 2.2, (nm ? p.moons[size_t(nm - 1)]->layOrb : 0) + pr * 0.4);
    }
    place(s);
}
// Posição de cada corpo no instante atual do relógio do mapa.
void Doc::place(System& s) const
{
    for (const auto& pp : s.bodies) {
        if (pp->body != BodyKind::Planeta) continue;
        Body& p = *pp;
        const double a = p.layA0 + p.layW * simT;
        p.layX = s.x + std::cos(a) * p.layOrb; p.layY = s.y + std::sin(a) * p.layOrb;
        for (const auto& mp : p.moons) { Body& m = *mp; const double ma = m.layA0 + m.layW * simT; m.layX = p.layX + std::cos(ma) * m.layOrb; m.layY = p.layY + std::sin(ma) * m.layOrb; }
    }
}
// Distância real (UA) → raio do esquema, encaixando entre as órbitas dos planetas.
double Doc::toR(const System& s, double au) const
{
    const auto ps = planetsOf(s);
    if (ps.empty()) return s.layR * std::min(0.95, 0.3 + 0.12 * std::log2(1 + au));
    if (au <= ps[0]->a) return ps[0]->layOrb * std::pow(std::max(au, 1e-4) / ps[0]->a, 0.5);
    const Body* last = ps.back();
    if (au >= last->a) return std::min(s.layR * 1.1, last->layOrb * std::pow(au / last->a, 0.3));
    for (size_t i = 1; i < ps.size(); i++) if (au <= ps[i]->a) { const Body* lo = ps[i - 1]; const Body* hi = ps[i]; return lo->layOrb + (hi->layOrb - lo->layOrb) * (std::log(au / lo->a) / std::log(hi->a / lo->a)); }
    return last->layOrb;
}
// O contrário do toR: de um raio no esquema pra distância real.
double Doc::fromR(const System& s, double rr) const
{
    const auto ps = planetsOf(s);
    if (ps.empty()) return std::max(0.02, std::pow(2, (rr / s.layR - 0.3) / 0.12) - 1);
    if (rr <= ps[0]->layOrb) return ps[0]->a * std::pow(rr / ps[0]->layOrb, 2);
    const Body* last = ps.back();
    if (rr >= last->layOrb) return last->a * std::pow(rr / last->layOrb, 1 / 0.3);
    for (size_t i = 1; i < ps.size(); i++) if (rr <= ps[i]->layOrb) { const Body* lo = ps[i - 1]; const Body* hi = ps[i]; return lo->a * std::pow(hi->a / lo->a, (rr - lo->layOrb) / (hi->layOrb - lo->layOrb)); }
    return last->a;
}

} // namespace galaxy
