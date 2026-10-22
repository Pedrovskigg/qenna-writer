#include "CartoInfo.h"
#include "CartoProf.h"

#include "CartoEdit.h"

#include <QCoreApplication>
#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>

namespace carto {

namespace {
constexpr double INF = std::numeric_limits<double>::infinity();
inline int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

const char* const kTownNames[] = { "Pedravia", "Talhamar", "Ventania", "Corvélia", "Sal do Norte", "Ermo Rubro", "Lumen", "Orvalho",
                                   "Brasa Alta", "Kethis", "Vau Cinzento", "Ameixal", "Torre Funda", "Rio Manso", "Altaneira", "Marisco" };
const char* const kRealmNames[] = { "Valdren", "Morvaun", "Aster", "Isk", "Corvin", "Sael" };
const RGB kRealmColors[6] = { { 196, 72, 60 }, { 58, 104, 168 }, { 212, 160, 52 }, { 116, 74, 150 }, { 62, 140, 96 }, { 204, 106, 46 } };

double hAt(const World& w, double x, double y)
{
    return w.h[static_cast<size_t>(clampi(static_cast<int>(jsRound(y)), 0, w.H - 1)) * w.W + clampi(static_cast<int>(jsRound(x)), 0, w.W - 1)];
}

// custo de caminhar: inclinação e altura encarecem; água custa `waterCost` (ou vira ponte, se estreita)
CostGrid costGrid(const World& w, double waterCost, bool bridge, int S)
{
    CostGrid g;
    g.gw = (w.W + S - 1) / S; g.gh = (w.H + S - 1) / S;
    const double ref = w.ref > 0 ? w.ref : 1;
    g.cost.assign(static_cast<size_t>(g.gw) * g.gh, 0.f);
    g.land.assign(g.cost.size(), 0);
    auto at = [&](int x, int y) -> double { return w.h[static_cast<size_t>(clampi(y, 0, w.H - 1)) * w.W + clampi(x, 0, w.W - 1)]; };
    for (int gy = 0; gy < g.gh; gy++) for (int gx = 0; gx < g.gw; gx++) {
        const int x = gx * S + 2, y = gy * S + 2;
        const double hv = at(x, y);
        const size_t i = static_cast<size_t>(gy) * g.gw + gx;
        if (hv <= 0) { g.cost[i] = bridge ? 0.f : static_cast<float>(waterCost); continue; }
        g.land[i] = 1;
        const double sl = (std::fabs(at(x + S, y) - at(x - S, y)) + std::fabs(at(x, y + S) - at(x, y - S))) / ref;
        g.cost[i] = static_cast<float>(1 + 22 * sl + 5 * std::max(0.0, hv / ref - 0.2));
    }
    if (bridge) {
        // ponte: água a até 3 células da terra é atravessável, cada vez mais cara
        std::vector<float> lm(g.cost.size());
        for (size_t i = 0; i < lm.size(); i++) lm[i] = g.land[i] ? 1.f : -1.f;
        const std::vector<float> d = distanceToLand(lm, g.gw, g.gh);
        for (size_t i = 0; i < lm.size(); i++)
            if (!g.land[i]) g.cost[i] = d[i] <= 3 ? static_cast<float>(14 + 4 * double(d[i]) * d[i]) : std::numeric_limits<float>::infinity();
    }
    return g;
}
}

int infoStep(const World& w) { return static_cast<int>(jsRound(4 * std::max(1.0, std::sqrt(double(w.W) * w.H / 2.4e6)))); }
double heightAt(const World& w, double x, double y) { return hAt(w, x, y); }

Tree dijkstra(int gw, int gh, const std::vector<float>& cost, const std::vector<int>& src)
{
    const int N = gw * gh;
    Tree t;
    t.dist.assign(N, INF);
    t.prev.assign(N, -1);
    // fila de prioridade em vetores (sem objeto por troca)
    std::vector<double> hk; std::vector<int> hv;
    hk.reserve(1 << 16); hv.reserve(1 << 16);
    auto push = [&](double d, int i) {
        hk.push_back(0); hv.push_back(0);
        int c = static_cast<int>(hk.size()) - 1;
        while (c > 0) { const int p = (c - 1) >> 1; if (hk[p] <= d) break; hk[c] = hk[p]; hv[c] = hv[p]; c = p; }
        hk[c] = d; hv[c] = i;
    };
    auto pop = [&](double& outD) {
        const int top = hv[0]; outD = hk[0];
        const double d = hk.back(); const int v = hv.back();
        hk.pop_back(); hv.pop_back();
        const int hn = static_cast<int>(hk.size());
        if (hn == 0) return top;
        int c = 0;
        for (;;) {
            const int l = c * 2 + 1; if (l >= hn) break;
            const int m = l + 1 < hn && hk[l + 1] < hk[l] ? l + 1 : l;
            if (hk[m] >= d) break;
            hk[c] = hk[m]; hv[c] = hv[m]; c = m;
        }
        hk[c] = d; hv[c] = v;
        return top;
    };
    for (int c : src) if (c >= 0 && c < N && t.dist[c] != 0) { t.dist[c] = 0; push(0, c); }
    static const int D[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
    static const double Wt[8] = { 1, 1, 1, 1, 1.414, 1.414, 1.414, 1.414 };
    while (!hk.empty()) {
        double d = 0;
        const int i = pop(d);
        if (d > t.dist[i]) continue;
        const int x = i % gw, y = i / gw;
        for (int k = 0; k < 8; k++) {
            const int nx = x + D[k][0], ny = y + D[k][1];
            if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
            const int j = ny * gw + nx;
            const double c = (double(cost[i]) + cost[j]) * 0.5 * Wt[k];
            if (!std::isfinite(c)) continue;
            const double nd = d + c;
            if (nd < t.dist[j]) { t.dist[j] = nd; t.prev[j] = i; push(nd, j); }
        }
    }
    return t;
}

Path smoothPath(Path p)
{
    for (int it = 0; it < 2; it++) {
        if (p.size() < 3) break;
        Path o;
        o.reserve(p.size() * 2);
        o.push_back(p.front());
        for (size_t i = 0; i + 1 < p.size(); i++) {
            const Pt a = p[i], b = p[i + 1];
            o.push_back({ a[0] * 0.75 + b[0] * 0.25, a[1] * 0.75 + b[1] * 0.25 });
            o.push_back({ a[0] * 0.25 + b[0] * 0.75, a[1] * 0.25 + b[1] * 0.75 });
        }
        o.push_back(p.back());
        p = std::move(o);
    }
    return p;
}

Path resample(const Path& pts, double step)
{
    Path out;
    if (pts.empty()) return out;
    out.push_back(pts[0]);
    size_t lastIdx = 0;
    double acc = 0;
    for (size_t k = 1; k < pts.size(); k++) {
        acc += std::hypot(pts[k][0] - pts[k - 1][0], pts[k][1] - pts[k - 1][1]);
        if (acc >= step) { out.push_back(pts[k]); lastIdx = k; acc = 0; }
    }
    if (lastIdx != pts.size() - 1) out.push_back(pts.back());   // a ponta final sempre entra
    return out;
}

const RGB kRealmPalette[8] = { { 196, 72, 60 }, { 58, 104, 168 }, { 212, 160, 52 }, { 116, 74, 150 },
                               { 62, 140, 96 }, { 204, 106, 46 }, { 164, 70, 120 }, { 70, 150, 160 } };

QString artName(const QString& kind)
{
    if (kind == QLatin1String("castle")) return QCoreApplication::translate("carto", "Castelo");
    if (kind == QLatin1String("tower")) return QCoreApplication::translate("carto", "Torre");
    if (kind == QLatin1String("lighthouse")) return QCoreApplication::translate("carto", "Farol");
    if (kind == QLatin1String("windmill")) return QCoreApplication::translate("carto", "Moinho");
    if (kind == QLatin1String("temple")) return QCoreApplication::translate("carto", "Templo");
    if (kind == QLatin1String("ruins")) return QCoreApplication::translate("carto", "Ruínas");
    return QCoreApplication::translate("carto", "Artefato");
}

// ── reinos ──
int Info::cellOf(double x, double y) const
{
    return clampi(static_cast<int>(std::floor(y / step)), 0, gh - 1) * gw + clampi(static_cast<int>(std::floor(x / step)), 0, gw - 1);
}

RealmSample Info::realmAt(double qx, double qy) const
{
    RealmSample s;
    if (dist.empty() || gw < 2 || gh < 2) return s;
    const double gx = clampd((qx - 2) / step, 0, gw - 1.001), gy = clampd((qy - 2) / step, 0, gh - 1.001);
    const int ix = static_cast<int>(gx), iy = static_cast<int>(gy);
    const double fx = gx - ix, fy = gy - iy;
    const size_t j = static_cast<size_t>(iy) * gw + ix;
    int id = -1, near = -1;
    double b1 = kFar, b2 = kFar, nearGap = kFar;
    for (size_t r = 0; r < dist.size(); r++) {
        const std::vector<double>& D = dist[r];
        const double v = D[j] * (1 - fx) * (1 - fy) + D[j + 1] * fx * (1 - fy) + D[j + gw] * (1 - fx) * fy + D[j + gw + 1] * fx * fy;
        const double rc = reach[r];
        if (v < rc) { if (v < b1) { b2 = b1; b1 = v; id = static_cast<int>(r); } else if (v < b2) b2 = v; }
        const double gap = std::fabs(v - rc);
        if (rc < kReachAll && gap < nearGap) { nearGap = gap; near = static_cast<int>(r); }
    }
    const double between = id >= 0 && b2 < kFar ? 1 - smoothstep(0, 1.6, b2 - b1) : 0;
    const double outer = near >= 0 ? 1 - smoothstep(0, 1.6, nearGap) : 0;
    if (between >= outer) { s.id = id; s.edge = between; s.eid = id; }
    else { s.id = id; s.edge = outer; s.eid = near; }
    return s;
}

RGB Info::realmColor(int id) const
{
    if (id < 0 || id >= static_cast<int>(realms.size())) return { 150, 146, 140 };
    return realms[id].color;
}

void recomputeRealms(const World& w, Info& info)
{
    CARTO_PROF("recomputeRealms");
    const CostGrid& g = info.realmGrid;
    const int gw = g.gw, gh = g.gh, N = gw * gh, S = info.step;
    std::vector<float> cost = g.cost;
    for (int i = 0; i < N; i++) if (info.barrier[i]) cost[i] = std::numeric_limits<float>::infinity();
    // um Dijkstra por território, cada um num núcleo
    info.dist.assign(info.defs.size(), {});
    auto one = [&](size_t r) {
        const RealmDef& d = info.defs[r];
        std::vector<int> src;
        for (const Pt& p : d.seeds) { const int c = info.cellOf(p[0], p[1]); if (!info.barrier[c]) src.push_back(c); }
        for (int c : d.paint) if (c >= 0 && c < N && !info.barrier[c]) src.push_back(c);
        if (src.empty()) { info.dist[r].assign(N, kFar); return; }
        Tree t = dijkstra(gw, gh, cost, src);
        for (double& v : t.dist) if (!std::isfinite(v)) v = kFar;
        info.dist[r] = std::move(t.dist);
    };
    {
        std::vector<std::thread> th;
        for (size_t r = 1; r < info.defs.size(); r++) th.emplace_back(one, r);
        if (!info.defs.empty()) one(0);
        for (std::thread& t : th) t.join();
    }
    info.reach.clear();
    for (const RealmDef& d : info.defs) info.reach.push_back(d.reach);
    info.owner.assign(N, -1);
    for (int i = 0; i < N; i++) {
        int best = -1; double bv = kFar;
        for (size_t r = 0; r < info.defs.size(); r++) { const double v = info.dist[r][i]; if (v < info.reach[r] && v < bv) { bv = v; best = static_cast<int>(r); } }
        info.owner[i] = static_cast<int16_t>(best);
    }
    info.realms.clear();
    for (size_t r = 0; r < info.defs.size(); r++) {
        double sx = 0, sy = 0; int n = 0;
        for (int i = 0; i < N; i++) if (g.land[i] && info.owner[i] == static_cast<int>(r)) { sx += i % gw; sy += i / gw; n++; }
        Realm R; R.name = info.defs[r].name; R.color = info.defs[r].color;
        R.x = n ? (sx / n) * S + 2 : 0; R.y = n ? (sy / n) * S + 2 : 0; R.area = n;
        info.realms.push_back(R);
    }
    refreshOwners(info);
    (void)w;
}

void refreshOwners(Info& info)
{
    if (info.owner.empty()) return;
    for (Town& t : info.towns) t.realm = info.owner[info.cellOf(t.x, t.y)];
    for (Building& b : info.buildings) if (b.town >= 0) b.realm = info.towns[b.town].realm;
    for (Art& a : info.user.arts) a.realm = info.owner[info.cellOf(a.x, a.y)];
}

void setRealmState(const World& w, Info& info, const RealmState& st)
{
    info.defs = st.defs;
    info.barrier = st.barrier;
    if (info.barrier.size() != static_cast<size_t>(info.gw) * info.gh) info.barrier.assign(static_cast<size_t>(info.gw) * info.gh, 0);
    recomputeRealms(w, info);
}

bool paintTerritory(const World& w, Info& info, double x, double y, double r, bool neutral, int realm)
{
    const int S = info.step, gw = info.gw, gh = info.gh;
    const int g0x = static_cast<int>(std::floor((x - r * 1.3) / S)), g1x = static_cast<int>(std::ceil((x + r * 1.3) / S));
    const int g0y = static_cast<int>(std::floor((y - r * 1.3) / S)), g1y = static_cast<int>(std::ceil((y + r * 1.3) / S));
    bool changed = false;
    for (int gy = std::max(0, g0y); gy <= std::min(gh - 1, g1y); gy++) for (int gx = std::max(0, g0x); gx <= std::min(gw - 1, g1x); gx++) {
        const double cx = gx * S + 2, cy = gy * S + 2;
        const double warp = 0.72 + 0.56 * brushNoise(w, cx, cy, 1301);
        if (std::hypot(cx - x, cy - y) / r * warp >= 1) continue;
        const int i = gy * gw + gx;
        if (neutral) {
            if (!info.barrier[i]) { info.barrier[i] = 1; changed = true; }
            for (RealmDef& d : info.defs) if (d.paint.erase(i)) changed = true;
        } else {
            if (realm < 0 || realm >= static_cast<int>(info.defs.size())) return changed;
            if (info.barrier[i]) { info.barrier[i] = 0; changed = true; }
            for (size_t k = 0; k < info.defs.size(); k++) if (static_cast<int>(k) != realm && info.defs[k].paint.erase(i)) changed = true;
            if (info.defs[realm].paint.insert(i).second) changed = true;
        }
    }
    return changed;
}

// ── cidades e estradas sugeridas ──
std::unique_ptr<Info> buildInfo(World& w, int count)
{
    CARTO_PROF("buildInfo");
    auto info = std::make_unique<Info>();
    const int S = infoStep(w);
    info->step = S;
    info->realmGrid = costGrid(w, 28, false, S);   // mar atravessável, mas caro (ilhas perto entram no reino)
    info->roadGrid = costGrid(w, 0, true, S);      // estrada não anda no mar aberto; água estreita vira ponte
    const int gw = info->roadGrid.gw, gh = info->roadGrid.gh;
    info->gw = gw; info->gh = gh;
    info->barrier.assign(static_cast<size_t>(gw) * gh, 0);
    const std::vector<uint8_t>& land = info->roadGrid.land;
    const double ref = w.ref > 0 ? w.ref : 1;
    // candidatos a cidade: chão baixo, perto da costa ganha ponto
    struct Cand { int i, gx, gy; double score; };
    std::vector<Cand> cand;
    const double seedR = static_cast<double>(w.seed) + 404;
    for (int gy = 3; gy < gh - 3; gy++) for (int gx = 3; gx < gw - 3; gx++) {
        const int i = gy * gw + gx;
        if (!land[i]) continue;
        const double t = w.h[static_cast<size_t>(clampi(gy * S + 2, 0, w.H - 1)) * w.W + clampi(gx * S + 2, 0, w.W - 1)] / ref;
        if (t < 0.03 || t > 0.32) continue;
        int coast = 0;
        for (int dy = -3; dy <= 3 && !coast; dy++) for (int dx = -3; dx <= 3; dx++) if (!land[(gy + dy) * gw + gx + dx]) { coast = 1; break; }
        cand.push_back({ i, gx, gy, coast * 0.6 + (0.32 - t) + hash2(gx, gy, seedR) * 0.5 });
    }
    std::stable_sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) { return a.score > b.score; });
    const int minD = std::max(18, static_cast<int>(jsRound(std::sqrt(double(gw) * gh) / 7)));
    const int maxCities = std::min(36, std::max(12, static_cast<int>(jsRound(12.0 * w.W * w.H / 2.4e6))));
    std::vector<Cand> cities;
    for (const Cand& c : cand) {
        if (static_cast<int>(cities.size()) >= maxCities) break;
        bool near = false;
        for (const Cand& o : cities) if (std::hypot(o.gx - c.gx, o.gy - c.gy) < minD) { near = true; break; }
        if (!near) cities.push_back(c);
    }
    count = std::min<int>(count, static_cast<int>(cities.size()));
    // capitais: as mais afastadas entre si
    std::vector<int> caps;
    if (count > 0) caps.push_back(0);
    while (static_cast<int>(caps.size()) < count) {
        int best = -1; double bd = -1;
        for (int c = 0; c < static_cast<int>(cities.size()); c++) {
            if (std::find(caps.begin(), caps.end(), c) != caps.end()) continue;
            double d = INF;
            for (int o : caps) d = std::min(d, std::hypot(cities[o].gx - cities[c].gx, cities[o].gy - cities[c].gy));
            if (d > bd) { bd = d; best = c; }
        }
        caps.push_back(best);
    }
    std::vector<std::vector<double>> realmD;
    std::vector<Tree> roadT;
    for (int c : caps) {
        realmD.push_back(dijkstra(gw, gh, info->realmGrid.cost, { cities[c].i }).dist);
        roadT.push_back(dijkstra(gw, gh, info->roadGrid.cost, { cities[c].i }));
    }
    auto toWorld = [&](int i) -> Pt { return { double((i % gw) * S + 2), double((i / gw) * S + 2) }; };
    auto trace = [&](const Tree& t, int i) { Path p; for (int k = i; k >= 0; k = t.prev[k]) p.push_back(toWorld(k)); return p; };
    for (int n = 0; n < static_cast<int>(cities.size()); n++) {
        const Cand& c = cities[n];
        int r = 0;
        for (int k = 1; k < count; k++) if (realmD[k][c.i] < realmD[r][c.i]) r = k;
        const bool isCap = std::find(caps.begin(), caps.end(), n) != caps.end();
        if (count > 0 && !isCap && std::isfinite(roadT[r].dist[c.i])) info->roads.push_back(smoothPath(trace(roadT[r], c.i)));
        const Pt p = toWorld(c.i);
        Town t; t.x = p[0]; t.y = p[1]; t.realm = count > 0 ? r : -1; t.capital = isCap;
        t.name = QString::fromUtf8(kTownNames[n % 16]);
        info->towns.push_back(t);
    }
    // estradas reais entre as capitais (só se houver caminho por terra)
    for (int a = 0; a < count; a++) for (int b = a + 1; b < count; b++)
        if (std::isfinite(roadT[a].dist[cities[caps[b]].i])) info->roads.push_back(smoothPath(trace(roadT[a], cities[caps[b]].i)));

    // construções: castelo nas capitais, casas em volta de toda cidade
    const double seedB = static_cast<double>(w.seed) + 909, KS = worldScale(w);
    for (int n = 0; n < static_cast<int>(info->towns.size()); n++) {
        const Town& tw = info->towns[n];
        if (tw.capital) {
            Building b; b.castle = true; b.size = 1; b.x = tw.x; b.y = tw.y; b.yaw = hash2(n, 1, seedB) * M_PI / 2;
            b.realm = tw.realm; b.town = n; b.v = hash2(n, 9, seedB);
            info->buildings.push_back(b);
        }
        const int want = tw.capital ? 9 : 4 + static_cast<int>(std::floor(hash2(n, 2, seedB) * 4));
        const double r0 = (tw.capital ? 13 : 2.5) * KS, r1 = (tw.capital ? 23 : 11) * KS;
        int made = 0;
        for (int k = 0; k < 80 && made < want; k++) {
            const double a = hash2(n, 100 + k, seedB) * M_PI * 2, rr = r0 + hash2(n, 300 + k, seedB) * (r1 - r0);
            const double x = tw.x + std::cos(a) * rr, y = tw.y + std::sin(a) * rr, t = hAt(w, x, y) / ref;
            if (t < 0.02 || t > 0.42) continue;
            // a base inteira em terra firme (senão a casa flutua na água)
            bool dry = true;
            for (int q = 0; q < 8 && dry; q++) { const double qa = q / 8.0 * M_PI * 2; if (hAt(w, x + std::cos(qa) * 3 * KS, y + std::sin(qa) * 3 * KS) <= 0) dry = false; }
            if (!dry) continue;
            bool crowd = false;
            for (const Building& b : info->buildings) if (std::hypot(b.x - x, b.y - y) < (b.castle ? 12.5 : 4.6) * KS) { crowd = true; break; }
            if (crowd) continue;
            Building b; b.x = x; b.y = y; b.yaw = hash2(n, 500 + k, seedB) * M_PI; b.realm = tw.realm; b.v = hash2(n, 700 + k, seedB);
            b.floors = hash2(n, 900 + k, seedB) < (tw.capital ? 0.45 : 0.2) ? 2 : 1;
            b.timber = hash2(n, 1100 + k, seedB) < 0.35;
            info->buildings.push_back(b);
            made++;
        }
    }
    // pontes: trechos de estrada sobre a água
    for (const Path& rd : info->roads) {
        Path run; bool on = false;
        for (size_t k = 0; k < rd.size(); k++) {
            const bool wet = hAt(w, rd[k][0], rd[k][1]) <= 0;
            if (wet && !on) { run.clear(); run.push_back(rd[k > 0 ? k - 1 : 0]); on = true; }
            if (on) run.push_back(rd[k]);
            if (!wet && on) { if (run.size() >= 3) info->bridges.push_back(run); on = false; }
        }
    }
    // plantações em retalhos em volta das cidades (chão baixo, firme e pouco inclinado)
    auto slopeAt = [&](double x, double y) { return (std::fabs(hAt(w, x + 3, y) - hAt(w, x - 3, y)) + std::fabs(hAt(w, x, y + 3) - hAt(w, x, y - 3))) / ref; };
    for (int n = 0; n < static_cast<int>(info->towns.size()); n++) {
        const Town& tw = info->towns[n];
        const int want = tw.capital ? 9 : 5;
        const double r0 = (tw.capital ? 20 : 11) * KS, r1 = (tw.capital ? 38 : 26) * KS;
        int made = 0;
        for (int k = 0; k < 160 && made < want; k++) {
            const double a = hash2(n, 1300 + k, seedB) * M_PI * 2, rr = r0 + hash2(n, 1500 + k, seedB) * (r1 - r0);
            const double x = tw.x + std::cos(a) * rr, y = tw.y + std::sin(a) * rr;
            const double t = hAt(w, x, y) / ref;
            if (t < 0.03 || t > 0.32 || slopeAt(x, y) > 0.085) continue;
            const double fw = (5 + hash2(n, 1700 + k, seedB) * 4) * KS, fd = (3.5 + hash2(n, 1900 + k, seedB) * 3) * KS;
            bool ok = true;
            for (int q = 0; q < 8 && ok; q++) { const double qa = q / 8.0 * M_PI * 2; if (hAt(w, x + std::cos(qa) * fw * 0.6, y + std::sin(qa) * fw * 0.6) <= 0) ok = false; }
            if (!ok) continue;
            bool crowd = false;
            for (const Field& f : info->fields) if (std::hypot(f.x - x, f.y - y) < 7.5 * KS) { crowd = true; break; }
            if (!crowd) for (const Building& b : info->buildings) if (std::hypot(b.x - x, b.y - y) < (b.castle ? 12 : 6) * KS) { crowd = true; break; }
            if (crowd) continue;
            Field f; f.x = x; f.y = y; f.w = fw; f.d = fd; f.yaw = a + (hash2(n, 2100 + k, seedB) - 0.5) * 0.5;
            f.crop = static_cast<int>(std::floor(hash2(n, 2300 + k, seedB) * 4));
            info->fields.push_back(f);
            made++;
        }
    }
    // cidades da costa: píer saindo pra água e barquinhos ancorados
    for (int n = 0; n < static_cast<int>(info->towns.size()); n++) {
        const Town& tw = info->towns[n];
        bool found = false; double ba = 0, brr = 0;
        for (int q = 0; q < 32; q++) {
            const double a = q / 32.0 * M_PI * 2;
            for (int rr = tw.capital ? 8 : 3; rr <= (tw.capital ? 20 : 16); rr++) {
                if (hAt(w, tw.x + std::cos(a) * rr, tw.y + std::sin(a) * rr) <= 0) { if (!found || rr < brr) { found = true; ba = a; brr = rr; } break; }
            }
        }
        if (!found) continue;
        const double sx = tw.x + std::cos(ba) * (brr - 1), sy = tw.y + std::sin(ba) * (brr - 1);
        const double ex = sx + std::cos(ba) * 6 * KS, ey = sy + std::sin(ba) * 6 * KS;
        if (hAt(w, ex, ey) > 0) continue;
        info->docks.push_back({ sx, sy, ex, ey });
        const int nb = tw.capital ? 3 : 1 + static_cast<int>(std::floor(hash2(n, 2500, seedB) * 2));
        for (int k = 0; k < nb; k++) {
            const double side = (k % 2 ? 1 : -1) * (2.6 + k * 1.2) * KS, along = (3 + k * 1.6) * KS;
            const double bx = sx + std::cos(ba) * along - std::sin(ba) * side, by = sy + std::sin(ba) * along + std::cos(ba) * side;
            if (hAt(w, bx, by) > 0) continue;
            info->boats.push_back({ bx, by, ba + (hash2(n, 2700 + k, seedB) - 0.5) * 0.8, hash2(n, 2900 + k, seedB) });
        }
    }
    // os reinos sugeridos viram os primeiros territórios editáveis (alcance sem limite)
    for (int r = 0; r < count; r++) {
        RealmDef d;
        d.name = QString::fromUtf8(kRealmNames[r % 6]);
        d.color = kRealmColors[r % 6];
        d.seeds.push_back({ info->towns[caps[r]].x, info->towns[caps[r]].y });
        d.reach = kReachAll;
        info->defs.push_back(d);
    }
    recomputeRealms(w, *info);
    return info;
}

// ── construir ──
namespace {
double uRand(const UserLayer& U, int k) { return hash2(U.seq, k, 7331); }
bool solidGround(const World& w, double x, double y, double r, double tMax, double slMax = 0.4)
{
    const double ref = w.ref > 0 ? w.ref : 1, t = hAt(w, x, y) / ref;
    if (t < 0.02 || t > tMax) return false;
    for (int q = 0; q < 8; q++) { const double a = q / 8.0 * M_PI * 2; if (hAt(w, x + std::cos(a) * r, y + std::sin(a) * r) <= 0) return false; }
    const double sl = (std::fabs(hAt(w, x + 3, y) - hAt(w, x - 3, y)) + std::fabs(hAt(w, x, y + 3) - hAt(w, x, y - 3))) / ref;
    return sl < slMax;   // inclinação típica do terreno ≈ 0,3 nessa medida
}
bool crowded(const UserLayer& U, double x, double y, double dHouse, double dArt, double dField)
{
    const double k = U.scale > 0 ? U.scale : 1;
    for (const House& b : U.houses) if (std::hypot(b.x - x, b.y - y) < dHouse) return true;
    for (const Art& a : U.arts) if (std::hypot(a.x - x, a.y - y) < std::max(dArt, (artRadius(a) + 2) * k)) return true;
    for (const Field& f : U.fields) if (std::hypot(f.x - x, f.y - y) < dField) return true;
    return false;
}
}

double artRadius(const Art& a)
{
    if (a.kind == QLatin1String("castle")) { static const double R[4] = { 7, 11.5, 17.5, 29 }; return R[clampi(a.size, 0, 3)]; }
    if (a.kind == QLatin1String("temple") || a.kind == QLatin1String("ruins")) return 6;
    return 3;
}

Art& addArt(const World& w, Info& info, const QString& kind, double x, double y, int size)
{
    (void)w;
    UserLayer& U = info.user;
    U.seq++;
    Art a;
    a.id = QStringLiteral("a%1").arg(U.seq);
    a.kind = kind; a.x = x; a.y = y; a.yaw = uRand(U, 1) * M_PI / 2; a.v = uRand(U, 2);
    a.size = kind == QLatin1String("castle") ? size : 0;
    if (kind == QLatin1String("castle") || kind == QLatin1String("temple") || kind == QLatin1String("lighthouse")) {
        int n = 0;
        for (const Art& o : U.arts) if (o.kind == kind) n++;
        a.name = QStringLiteral("%1 %2").arg(artName(kind)).arg(n + 1);
    }
    U.arts.push_back(a);
    U.ver++;
    return U.arts.back();
}

bool paintVillage(const World& w, Info& info, double x, double y, double r)
{
    UserLayer& U = info.user;
    int made = 0;
    U.scale = worldScale(w);
    const double k = worldScale(w);
    for (int it = 0; it < 5; it++) {
        U.seq++;
        const double a = uRand(U, 3) * M_PI * 2, rr = std::sqrt(uRand(U, 4)) * r;
        const double px = x + std::cos(a) * rr, py = y + std::sin(a) * rr;
        if (!solidGround(w, px, py, 3 * k, 0.45) || crowded(U, px, py, 4.8 * k, 9 * k, 5.5 * k)) continue;
        House h; h.x = px; h.y = py; h.yaw = uRand(U, 5) * M_PI; h.v = uRand(U, 6); h.floors = uRand(U, 7) < 0.22 ? 2 : 1; h.timber = uRand(U, 8) < 0.35;
        U.houses.push_back(h);
        made++;
    }
    if (made) U.ver++;
    return made > 0;
}

bool paintFieldsUser(const World& w, Info& info, double x, double y, double r)
{
    UserLayer& U = info.user;
    int made = 0;
    U.scale = worldScale(w);
    const double k = worldScale(w);
    for (int it = 0; it < 3; it++) {
        U.seq++;
        const double a = uRand(U, 3) * M_PI * 2, rr = std::sqrt(uRand(U, 4)) * r;
        const double px = x + std::cos(a) * rr, py = y + std::sin(a) * rr;
        const double fw = (5 + uRand(U, 5) * 4) * k, fd = (3.5 + uRand(U, 6) * 3) * k;
        if (!solidGround(w, px, py, fw * 0.6, 0.34, 0.3) || crowded(U, px, py, 5.5 * k, 9 * k, 7.5 * k)) continue;
        Field f; f.x = px; f.y = py; f.w = fw; f.d = fd; f.yaw = uRand(U, 7) * M_PI; f.crop = static_cast<int>(std::floor(uRand(U, 8) * 4));
        U.fields.push_back(f);
        made++;
    }
    if (made) U.ver++;
    return made > 0;
}

bool addWall(Info& info, const Path& pts)
{
    if (pts.size() < 2) return false;
    const Path sm = smoothPath(pts);
    Path out{ sm[0] };
    double acc = 0;
    for (size_t k = 1; k < sm.size(); k++) {
        acc += std::hypot(sm[k][0] - sm[k - 1][0], sm[k][1] - sm[k - 1][1]);
        if (acc >= 2) { out.push_back(sm[k]); acc = 0; }
    }
    if (out.size() < 2) return false;
    info.user.walls.push_back({ out });
    info.user.ver++;
    return true;
}

bool addRoad(Info& info, const Path& pts, const QString& kind, bool smooth)
{
    if (pts.size() < 2) return false;
    const Path out = resample(smooth ? smoothPath(pts) : pts, 1.5);
    if (out.size() < 2) return false;
    Road r; r.pts = out;
    r.kind = (kind == QLatin1String("trail") || kind == QLatin1String("royal")) ? kind : QStringLiteral("road");
    info.user.roads.push_back(r);
    info.user.ver++;
    return true;
}

bool addBridge(Info& info, Pt a, Pt b)
{
    const double L = std::hypot(b[0] - a[0], b[1] - a[1]);
    if (L < 4) return false;
    const int n = static_cast<int>(std::ceil(L / 1.5));
    Path pts;
    for (int k = 0; k <= n; k++) pts.push_back({ a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n });
    info.user.bridges.push_back(pts);
    info.user.ver++;
    return true;
}

bool eraseUser(Info& info, double x, double y, double r)
{
    UserLayer& U = info.user;
    auto near = [&](double px, double py) { return std::hypot(px - x, py - y) < r; };
    auto count = [&]() {
        size_t n = U.houses.size() + U.fields.size() + U.arts.size() + U.bridges.size();
        for (const Wall& wl : U.walls) n += wl.pts.size();
        for (const Road& rd : U.roads) n += rd.pts.size();
        return n;
    };
    const size_t n0 = count();
    U.houses.erase(std::remove_if(U.houses.begin(), U.houses.end(), [&](const House& b) { return near(b.x, b.y); }), U.houses.end());
    U.fields.erase(std::remove_if(U.fields.begin(), U.fields.end(), [&](const Field& f) { return near(f.x, f.y); }), U.fields.end());
    U.arts.erase(std::remove_if(U.arts.begin(), U.arts.end(), [&](const Art& a) { return near(a.x, a.y); }), U.arts.end());
    U.bridges.erase(std::remove_if(U.bridges.begin(), U.bridges.end(), [&](const Path& br) {
        for (const Pt& p : br) if (near(p[0], p[1])) return true;
        return false;
    }), U.bridges.end());
    std::vector<Wall> walls;
    for (const Wall& wl : U.walls) {
        Path cur;
        for (const Pt& p : wl.pts) { if (near(p[0], p[1])) { if (cur.size() > 1) walls.push_back({ cur }); cur.clear(); } else cur.push_back(p); }
        if (cur.size() > 1) walls.push_back({ cur });
    }
    U.walls = std::move(walls);
    std::vector<Road> roads;
    for (const Road& rd : U.roads) {
        Path cur;
        for (const Pt& p : rd.pts) { if (near(p[0], p[1])) { if (cur.size() > 1) roads.push_back({ cur, rd.kind }); cur.clear(); } else cur.push_back(p); }
        if (cur.size() > 1) roads.push_back({ cur, rd.kind });
    }
    U.roads = std::move(roads);
    const size_t n1 = count();
    if (n1 != n0) U.ver++;
    return n1 != n0;
}

std::vector<Path> roadBridges(const World& w, const UserLayer& U)
{
    std::vector<Path> out;
    for (const Road& rd : U.roads) {
        const Path& p = rd.pts;
        Path run; bool on = false;
        for (size_t k = 0; k < p.size(); k++) {
            const bool wet = hAt(w, p[k][0], p[k][1]) <= 0;
            if (wet && !on) { run.clear(); run.push_back(p[k > 0 ? k - 1 : 0]); on = true; }
            if (on) run.push_back(p[k]);
            if (!wet && on) { if (run.size() >= 3) out.push_back(run); on = false; }
        }
    }
    return out;
}

// ── estrada pelo vale ──
Router::Router(const World& w, Info& info, double x, double y) : m_info(&info), m_x(x), m_y(y)
{
    (void)w;
    m_step = info.step;
    const CostGrid& g = info.roadGrid;
    m_gw = g.gw; m_gh = g.gh;
    m_land = g.land;
    std::vector<float> cost = g.cost;
    // andar por estrada que já existe sai barato: a nova se junta à velha
    auto cheap = [&](const Path& pts) { for (const Pt& p : pts) { const int i = cellOf(p[0], p[1]); if (m_land[i]) cost[i] = std::min(cost[i], 0.45f); } };
    for (const Road& r : info.user.roads) cheap(r.pts);
    if (info.user.showGen) for (const Path& r : info.roads) cheap(r);
    m_src = nearLand(x, y);
    if (m_src >= 0) m_tree = dijkstra(m_gw, m_gh, cost, { m_src });
}
int Router::cellOf(double px, double py) const
{
    return clampi(static_cast<int>(std::floor(py / m_step)), 0, m_gh - 1) * m_gw + clampi(static_cast<int>(std::floor(px / m_step)), 0, m_gw - 1);
}
// clicou na água rente à costa (porto, farol): começa na terra mais perto
int Router::nearLand(double px, double py) const
{
    const int i0 = cellOf(px, py);
    if (m_land[i0]) return i0;
    const int cx = i0 % m_gw, cy = i0 / m_gw;
    int best = -1, bd = 1 << 30;
    for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) {
        const int nx = cx + dx, ny = cy + dy;
        if (nx < 0 || ny < 0 || nx >= m_gw || ny >= m_gh || !m_land[ny * m_gw + nx]) continue;
        const int d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = ny * m_gw + nx; }
    }
    return best;
}
bool Router::pathTo(double tx, double ty, Path& out) const
{
    out.clear();
    if (!ok()) return false;
    const int j = nearLand(tx, ty);
    if (j < 0 || !std::isfinite(m_tree.dist[j])) return false;
    Path p;
    for (int k = j; k >= 0; k = m_tree.prev[k]) p.push_back({ (k % m_gw) * double(m_step) + m_step / 2.0, (k / m_gw) * double(m_step) + m_step / 2.0 });
    std::reverse(p.begin(), p.end());
    // as pontas caem exatamente onde o usuário clicou (no portão do castelo, não no meio da célula)
    if (cellOf(m_x, m_y) == m_src) p[0] = { m_x, m_y }; else p.insert(p.begin(), { m_x, m_y });
    if (cellOf(tx, ty) == j && p.size() > 1) p.back() = { tx, ty }; else p.push_back({ tx, ty });
    out = smoothPath(p);
    return true;
}

// ── vegetação: uma tentativa a cada 4 px, sorteio preso à semente ──
namespace {
// a planta da célula (cx, cy) da grade: `inside` diz se o ponto sorteado cai onde interessa
template <typename In>
bool floraCell(World& w, double cx, double cy, double S, double seed, In inside, Flora& out)
{
    {
        const double r1 = hash2(jsRound(cx * 7), jsRound(cy * 7), seed), r2 = hash2(cx, cy, seed + 1), r3 = hash2(cx, cy, seed + 2), r4 = hash2(cx, cy, seed + 3);
        const double x = cx + r1 * S, y = cy + r2 * S;
        if (!inside(x, y)) return false;
        const ClimateSample c = climateAt(w, x, y);
        if (!c.valid || c.t < 0.025) return false;
        const double alt = 1 - smoothstep(0.34, 0.55, c.t);
        double p = 0;
        int type = -1;
        switch (c.id) {
        case 1:   // floresta: pinheiro no frio, mistura no meio, uns pinheiros soltos até no calor
            p = (0.40 + 0.38 * clampd((c.m - 0.45) / 0.3, 0, 1)) * alt;
            type = c.temp < 0.46 ? Pine : c.temp < 0.66 ? (r4 < 0.5 ? Pine : Broad) : (r4 < 0.12 ? Pine : Broad);
            if (c.temp > 0.70 && c.t < 0.09 && r4 < 0.35) type = Palm;
            break;
        case 2:   // planície: árvore solta e arbusto
            p = (0.04 + 0.10 * c.m) * alt;
            type = c.temp < 0.42 ? Pine : r4 < 0.6 ? Broad : Shrub;
            break;
        case 4:   // brejo
            p = 0.28 * alt; type = r4 < 0.55 ? Reed : r4 < 0.75 ? Dead : Broad; break;
        case 3:   // deserto
            p = 0.05; type = r4 < 0.45 ? (c.temp > 0.62 ? Cactus : Shrub) : r4 < 0.8 ? Shrub : Rock;
            if (c.t > 0.3) { type = Rock; p = 0.03; }
            break;
        case 0:   // gelo
            if (c.t < 0.42) { p = 0.12; type = r4 < 0.85 ? SnowPine : Rock; } else { p = 0.02; type = Rock; }
            break;
        }
        if (c.t > 0.55 && c.id != 0) { p = 0.015; type = Rock; }   // pedra solta acima da linha das árvores
        if (type < 0 || r3 >= p) return false;
        double v = std::fmod(r1 * 7.13 + r2 * 3.7, 1.0);
        out = { x, y, type, v, 0.8 + 0.45 * r4, c.id };
        return true;
    }
}
}

std::vector<Flora> buildFlora(World& w)
{
    CARTO_PROF("buildFlora");
    std::vector<Flora> out;
    const double S = 4 * std::pow(worldScale(w), 0.8);
    const double seed = static_cast<double>(w.seed) + 77;
    Flora f;
    auto all = [](double, double) { return true; };
    for (double cy = 0; cy < w.H; cy += S) for (double cx = 0; cx < w.W; cx += S)
        if (floraCell(w, cx, cy, S, seed, all, f)) out.push_back(f);
    return out;
}

void updateFloraBox(World& w, std::vector<Flora>& flora, int x0, int y0, int x1, int y1)
{
    CARTO_PROF("vegetação do pedaço");
    // a planta lê o terreno nos 4 px em volta do ponto: 2 px de folga cobrem a borda
    const double X0 = x0 - 2, Y0 = y0 - 2, X1 = x1 + 2, Y1 = y1 + 2;
    auto inside = [&](double x, double y) { return x >= X0 && x <= X1 && y >= Y0 && y <= Y1; };
    flora.erase(std::remove_if(flora.begin(), flora.end(), [&](const Flora& f) { return inside(f.x, f.y); }), flora.end());
    const double S = 4 * std::pow(worldScale(w), 0.8);
    const double seed = static_cast<double>(w.seed) + 77;
    // as mesmas células da grade inteira (cy = n·S somado do zero, como no laço de buildFlora)
    Flora f;
    double cy = 0;
    for (; cy + S < Y0 - S; cy += S) {}
    for (; cy < w.H && cy <= Y1; cy += S) {
        double cx = 0;
        for (; cx + S < X0 - S; cx += S) {}
        for (; cx < w.W && cx <= X1; cx += S)
            if (floraCell(w, cx, cy, S, seed, inside, f)) flora.push_back(f);
    }
}

// ── gravar e ler ──
namespace {
QJsonArray ptJ(const Pt& p) { return QJsonArray{ std::round(p[0] * 100) / 100, std::round(p[1] * 100) / 100 }; }
Pt jPt(const QJsonValue& v) { const QJsonArray a = v.toArray(); return { a.at(0).toDouble(), a.at(1).toDouble() }; }
QJsonArray pathJ(const Path& p) { QJsonArray a; for (const Pt& q : p) a.append(ptJ(q)); return a; }
Path jPath(const QJsonValue& v) { Path p; for (const QJsonValue& q : v.toArray()) p.push_back(jPt(q)); return p; }
QJsonArray rgbJ(const RGB& c) { return QJsonArray{ c[0], c[1], c[2] }; }
RGB jRgb(const QJsonValue& v) { const QJsonArray a = v.toArray(); return { a.at(0).toDouble(), a.at(1).toDouble(), a.at(2).toDouble() }; }
}

QJsonObject UserLayer::toJson() const
{
    QJsonArray A, Hs, Ws, Fs, Bs, Rs;
    for (const Art& a : arts)
        A.append(QJsonObject{ { "id", a.id }, { "kind", a.kind }, { "x", a.x }, { "y", a.y }, { "yaw", a.yaw }, { "v", a.v }, { "size", a.size }, { "name", a.name } });
    for (const House& h : houses)
        Hs.append(QJsonObject{ { "x", h.x }, { "y", h.y }, { "yaw", h.yaw }, { "v", h.v }, { "floors", h.floors }, { "timber", h.timber } });
    for (const Wall& wl : walls) Ws.append(pathJ(wl.pts));
    for (const Field& f : fields)
        Fs.append(QJsonObject{ { "x", f.x }, { "y", f.y }, { "w", f.w }, { "d", f.d }, { "yaw", f.yaw }, { "crop", f.crop } });
    for (const Path& b : bridges) Bs.append(pathJ(b));
    for (const Road& r : roads) Rs.append(QJsonObject{ { "kind", r.kind }, { "pts", pathJ(r.pts) } });
    return QJsonObject{ { "arts", A }, { "houses", Hs }, { "walls", Ws }, { "fields", Fs }, { "bridges", Bs }, { "roads", Rs },
                        { "showGen", showGen }, { "seq", seq }, { "scale", scale } };
}

UserLayer UserLayer::fromJson(const QJsonObject& o)
{
    UserLayer U;
    for (const QJsonValue& v : o.value("arts").toArray()) {
        const QJsonObject a = v.toObject();
        Art t; t.id = a.value("id").toString(); t.kind = a.value("kind").toString(); t.x = a.value("x").toDouble(); t.y = a.value("y").toDouble();
        t.yaw = a.value("yaw").toDouble(); t.v = a.value("v").toDouble(); t.size = a.value("size").toInt(); t.name = a.value("name").toString();
        U.arts.push_back(t);
    }
    for (const QJsonValue& v : o.value("houses").toArray()) {
        const QJsonObject a = v.toObject();
        House h; h.x = a.value("x").toDouble(); h.y = a.value("y").toDouble(); h.yaw = a.value("yaw").toDouble(); h.v = a.value("v").toDouble();
        h.floors = a.value("floors").toInt(1); h.timber = a.value("timber").toBool();
        U.houses.push_back(h);
    }
    for (const QJsonValue& v : o.value("walls").toArray()) U.walls.push_back({ jPath(v) });
    for (const QJsonValue& v : o.value("fields").toArray()) {
        const QJsonObject a = v.toObject();
        Field f; f.x = a.value("x").toDouble(); f.y = a.value("y").toDouble(); f.w = a.value("w").toDouble(); f.d = a.value("d").toDouble();
        f.yaw = a.value("yaw").toDouble(); f.crop = a.value("crop").toInt();
        U.fields.push_back(f);
    }
    for (const QJsonValue& v : o.value("bridges").toArray()) U.bridges.push_back(jPath(v));
    for (const QJsonValue& v : o.value("roads").toArray()) {
        const QJsonObject a = v.toObject();
        Road r; r.kind = a.value("kind").toString(QStringLiteral("road")); r.pts = jPath(a.value("pts"));
        U.roads.push_back(r);
    }
    U.showGen = o.value("showGen").toBool(true);
    U.seq = o.value("seq").toInt();
    U.scale = o.value("scale").toDouble(1);
    return U;
}

QJsonObject realmsToJson(const Info& info)
{
    QJsonArray defs;
    for (const RealmDef& d : info.defs) {
        QJsonArray seeds, paint;
        for (const Pt& p : d.seeds) seeds.append(ptJ(p));
        for (int c : d.paint) paint.append(c);
        QJsonObject o{ { "name", d.name }, { "color", rgbJ(d.color) }, { "seeds", seeds },
                       { "reach", d.reach >= kReachAll ? -1.0 : d.reach }, { "paint", paint } };
        if (!d.groupId.isEmpty()) o.insert("group", d.groupId);
        if (!d.placeId.isEmpty()) o.insert("place", d.placeId);
        defs.append(o);
    }
    QJsonArray barrier;
    for (size_t i = 0; i < info.barrier.size(); i++) if (info.barrier[i]) barrier.append(static_cast<int>(i));
    return QJsonObject{ { "gw", info.gw }, { "gh", info.gh }, { "defs", defs }, { "barrier", barrier } };
}

void realmsFromJson(Info& info, const QJsonObject& o)
{
    // grade de outro tamanho (não deveria acontecer): as células pintadas não valem, as sementes sim
    const bool sameGrid = o.value("gw").toInt() == info.gw && o.value("gh").toInt() == info.gh;
    const int N = info.gw * info.gh;
    info.defs.clear();
    for (const QJsonValue& v : o.value("defs").toArray()) {
        const QJsonObject a = v.toObject();
        RealmDef d;
        d.name = a.value("name").toString();
        d.color = jRgb(a.value("color"));
        d.groupId = a.value("group").toString();
        d.placeId = a.value("place").toString();
        for (const QJsonValue& s : a.value("seeds").toArray()) d.seeds.push_back(jPt(s));
        const double r = a.value("reach").toDouble(-1);
        d.reach = r < 0 ? kReachAll : r;
        if (sameGrid) for (const QJsonValue& c : a.value("paint").toArray()) { const int i = c.toInt(-1); if (i >= 0 && i < N) d.paint.insert(i); }
        info.defs.push_back(d);
    }
    info.barrier.assign(static_cast<size_t>(N), 0);
    if (sameGrid) for (const QJsonValue& c : o.value("barrier").toArray()) { const int i = c.toInt(-1); if (i >= 0 && i < N) info.barrier[i] = 1; }
}

} // namespace carto
