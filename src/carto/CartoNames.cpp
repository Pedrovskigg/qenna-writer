#include "CartoNames.h"
#include "CartoProf.h"

#include "CartoNameLang.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace carto {

namespace {
const char* const kKinds[] = { "ocean", "sea", "range", "isles", "island", "forest", "desert", "ice", "marsh", "bay", "lake", "plains", "river", "peak" };
const int kPrio[] = { 100, 90, 80, 70, 62, 56, 56, 55, 50, 46, 44, 40, 38, 34 };
int kindIndex(const QString& k) { for (int i = 0; i < 14; i++) if (k == QLatin1String(kKinds[i])) return i; return -1; }

QString makeName(const QString& tplKey, const std::function<double(int)>& r)
{
    const NameLang& lang = nameLang();
    const QStringList T = lang.templates.value(tplKey);
    if (T.isEmpty()) return QString();
    QString t = T[static_cast<int>(std::floor(r(5) * T.size()))];
    const int at = t.indexOf(QLatin1String("{R}"));
    if (at >= 0) t.replace(at, 3, nameRoot(r));
    // adjetivos e nomes concordam em gênero e número (o mesmo índice em todas as listas)
    static const QRegularExpression ph(QStringLiteral("\\{(m|f|mp|fp|Nm|Nf|Nmp|Nfp)\\}"));
    QRegularExpressionMatch m;
    while ((m = ph.match(t)).hasMatch()) {
        const QStringList W = lang.words.value(m.captured(1));
        const QString word = W.isEmpty() ? QString() : W[static_cast<int>(std::floor(r(6) * W.size()))];
        t.replace(m.capturedStart(), m.capturedLength(), word);
    }
    return polishName(t);
}

struct Comp { int id; std::vector<int> cells; bool border; };
struct Comps { std::vector<int> lab; std::vector<Comp> list; };
template <typename Mask>
Comps comps(int gw, int gh, Mask mask)
{
    const int N = gw * gh;
    Comps c;
    c.lab.assign(N, -1);
    std::vector<int> stack(N);
    for (int s0 = 0; s0 < N; s0++) {
        if (c.lab[s0] >= 0 || !mask(s0)) continue;
        Comp cp; cp.id = static_cast<int>(c.list.size()); cp.border = false;
        int sp = 0;
        stack[sp++] = s0; c.lab[s0] = cp.id;
        while (sp) {
            const int i = stack[--sp];
            cp.cells.push_back(i);
            const int x = i % gw, y = i / gw;
            if (x == 0 || y == 0 || x == gw - 1 || y == gh - 1) cp.border = true;
            if (x > 0 && c.lab[i - 1] < 0 && mask(i - 1)) { c.lab[i - 1] = cp.id; stack[sp++] = i - 1; }
            if (x < gw - 1 && c.lab[i + 1] < 0 && mask(i + 1)) { c.lab[i + 1] = cp.id; stack[sp++] = i + 1; }
            if (y > 0 && c.lab[i - gw] < 0 && mask(i - gw)) { c.lab[i - gw] = cp.id; stack[sp++] = i - gw; }
            if (y < gh - 1 && c.lab[i + gw] < 0 && mask(i + gw)) { c.lab[i + gw] = cp.id; stack[sp++] = i + gw; }
        }
        c.list.push_back(std::move(cp));
    }
    return c;
}
template <typename Arr>
int argmax(const std::vector<int>& cells, const Arr& arr) { int b = cells[0]; for (int i : cells) if (arr[i] > arr[b]) b = i; return b; }
}

std::vector<NameFeature> buildNames(World& w, const std::vector<River>& rivers, const QString& density)
{
    CARTO_PROF("buildNames");
    const int S = infoStep(w), W = w.W, H = w.H;
    const double ref = w.ref > 0 ? w.ref : 1;
    const int gw = (W + S - 1) / S, gh = (H + S - 1) / S, N = gw * gh;
    const double M = std::min(W, H);
    const int dens = density == QLatin1String("few") ? 0 : density == QLatin1String("many") ? 2 : 1;
    const double K = dens == 2 ? 0.65 : dens == 0 ? 1.5 : 1;   // régua de tamanho mínimo
    std::vector<float> hg(N);
    for (int gy = 0; gy < gh; gy++) for (int gx = 0; gx < gw; gx++)
        hg[gy * gw + gx] = w.h[static_cast<size_t>(std::min(H - 1, gy * S + (S >> 1))) * W + std::min(W - 1, gx * S + (S >> 1))];
    auto wx = [&](int i) { return (i % gw) * double(S) + S / 2.0; };
    auto wy = [&](int i) { return (i / gw) * double(S) + S / 2.0; };
    std::vector<NameFeature> F;
    auto add = [&](const char* kind, double x, double y, double r) -> NameFeature& {
        NameFeature f; f.kind = QString::fromLatin1(kind); f.x = x; f.y = y; f.r = r; f.prio = kPrio[kindIndex(f.kind)];
        F.push_back(f);
        return F.back();
    };

    // ── água: oceano, mares, baías e lagos ──
    const std::vector<float> dW = distanceToLand(hg, gw, gh);
    std::vector<float> inv(N);
    for (int i = 0; i < N; i++) inv[i] = hg[i] > 0 ? -1.f : 1.f;
    const std::vector<float> dL = distanceToLand(inv, gw, gh);   // em terra: distância até a água
    const Comps water = comps(gw, gh, [&](int i) { return hg[i] <= 0; });
    // o meio da água aberta conta a borda do mapa como costa (senão o nome do oceano nasce cortado na beirada)
    std::vector<double> dE(N);
    for (int gy = 0; gy < gh; gy++) for (int gx = 0; gx < gw; gx++)
        dE[gy * gw + gx] = std::min<double>(dW[gy * gw + gx], 1 + std::min(std::min(gx, gy), std::min(gw - 1 - gx, gh - 1 - gy)));
    std::vector<int> cand;
    for (int gy = 1; gy < gh - 1; gy++) for (int gx = 1; gx < gw - 1; gx++) {
        const int i = gy * gw + gx;
        const double d = dE[i];
        if (d < 2 || !water.list[water.lab[i]].border) continue;
        bool peak = true;
        for (int dy = -1; dy <= 1 && peak; dy++) for (int dx = -1; dx <= 1; dx++) if (dE[i + dy * gw + dx] > d) { peak = false; break; }
        if (peak) cand.push_back(i);
    }
    std::stable_sort(cand.begin(), cand.end(), [&](int a, int b) { return dE[a] > dE[b]; });
    // fração das direções em que a terra aparece em até L células (16 raios; sair do mapa = aberto)
    auto enclosure = [&](int i, int L) {
        const int x0 = i % gw, y0 = i / gw;
        int hit = 0;
        for (int k = 0; k < 16; k++) {
            const double a = k / 16.0 * M_PI * 2, ca = std::cos(a), sa = std::sin(a);
            for (int t = 1; t <= L; t++) {
                const int x = static_cast<int>(jsRound(x0 + ca * t)), y = static_cast<int>(jsRound(y0 + sa * t));
                if (x < 0 || y < 0 || x >= gw || y >= gh) break;
                if (hg[y * gw + x] > 0) { hit++; break; }
            }
        }
        return hit / 16.0;
    };
    struct WL { double x, y, r; };
    std::vector<WL> wl;
    auto farFrom = [&](double x, double y, double r, double gap) {
        for (const WL& o : wl) if (!(std::hypot(o.x - x, o.y - y) > std::max(o.r, r) * 2.2 + gap)) return false;
        return true;
    };
    int nOcean = 0, nSea = 0, nBay = 0;
    const int extra = static_cast<int>(std::floor(double(W) * H / 3e6));
    const int maxSea = (dens == 0 ? 1 : dens == 1 ? 3 : 5) + extra, maxBay = (dens == 0 ? 1 : dens == 1 ? 3 : 6) + extra;
    for (int i : cand) {
        const double r = dE[i] * S;
        if (r < M * 0.045 * K) break;
        if (!farFrom(wx(i), wy(i), r, M * 0.08)) continue;
        const double enc = enclosure(i, static_cast<int>(std::ceil(dE[i] * 3)));
        if (nOcean < (double(W) * H > 4e6 ? 2 : 1) && r > M * 0.1 && enc < 0.55) { add("ocean", wx(i), wy(i), r); wl.push_back({ wx(i), wy(i), r }); nOcean++; continue; }
        if (nSea >= maxSea) continue;
        add("sea", wx(i), wy(i), r); wl.push_back({ wx(i), wy(i), r }); nSea++;
    }
    for (int i : cand) {
        const double r = dE[i] * S;
        if (nBay >= maxBay) break;
        if (r > M * 0.07 || dE[i] < 2 || dE[i] < dW[i]) continue;      // baía de verdade: cercada de terra, não da beirada
        if (!farFrom(wx(i), wy(i), r, M * 0.05)) continue;
        if (enclosure(i, static_cast<int>(std::ceil(dE[i] * 5)) + 2) < 0.62) continue;
        const double rr = std::max(r, S * 3.0);
        add("bay", wx(i), wy(i), rr); wl.push_back({ wx(i), wy(i), rr }); nBay++;
    }
    struct Lake { int p; double area; };
    std::vector<Lake> lakes;
    for (const Comp& c : water.list) {
        if (c.border) continue;
        const double area = double(c.cells.size()) * S * S;
        const int p = argmax(c.cells, dW);
        if (area >= std::pow(M * 0.16, 2)) { const double r = dW[p] * S; add("sea", wx(p), wy(p), r); wl.push_back({ wx(p), wy(p), r }); continue; }
        if (c.cells.size() < 6 || area < std::pow(M * 0.03 * K, 2)) continue;
        lakes.push_back({ p, area });
    }
    std::stable_sort(lakes.begin(), lakes.end(), [](const Lake& a, const Lake& b) { return a.area > b.area; });
    const size_t nLakes = std::min<size_t>(lakes.size(), dens == 0 ? 1 : dens == 1 ? 4 : 10);
    for (size_t k = 0; k < nLakes; k++) {
        const int p = lakes[k].p;
        const double r = std::max<double>(dW[p] * S, S * 2.0);
        add("lake", wx(p), wy(p), r); wl.push_back({ wx(p), wy(p), r });
    }

    // ── ilhas e arquipélagos (a massa de terra principal fica com os reinos) ──
    const Comps lands = comps(gw, gh, [&](int i) { return hg[i] > 0; });
    size_t totalLand = 0;
    for (const Comp& c : lands.list) totalLand += c.cells.size();
    if (!totalLand) totalLand = 1;
    const double big = std::pow(M * 0.075 * K, 2);
    struct Isl { double area, cx, cy; int pole; };
    std::vector<Isl> isl, small;
    for (const Comp& c : lands.list) {
        if (c.cells.size() > totalLand * 0.3) continue;
        const double area = double(c.cells.size()) * S * S;
        double sx = 0, sy = 0;
        for (int i : c.cells) { sx += wx(i); sy += wy(i); }
        const Isl o{ area, sx / c.cells.size(), sy / c.cells.size(), argmax(c.cells, dL) };
        if (area >= big) isl.push_back(o); else if (c.cells.size() >= 2) small.push_back(o);
    }
    std::stable_sort(isl.begin(), isl.end(), [](const Isl& a, const Isl& b) { return a.area > b.area; });
    for (size_t k = 0; k < std::min<size_t>(isl.size(), dens == 0 ? 2 : dens == 1 ? 6 : 12); k++)
        add("island", wx(isl[k].pole), wy(isl[k].pole), std::sqrt(isl[k].area / M_PI));
    // ilhotas perto umas das outras viram um arquipélago só
    const double link = M * 0.075;
    std::vector<uint8_t> seen(small.size(), 0);
    std::vector<std::vector<int>> groups;
    for (size_t a = 0; a < small.size(); a++) {
        if (seen[a]) continue;
        std::vector<int> g{ static_cast<int>(a) };
        seen[a] = 1;
        for (size_t q = 0; q < g.size(); q++) for (size_t b = 0; b < small.size(); b++)
            if (!seen[b] && std::hypot(small[g[q]].cx - small[b].cx, small[g[q]].cy - small[b].cy) < link) { seen[b] = 1; g.push_back(static_cast<int>(b)); }
        if (g.size() >= 3) groups.push_back(g);
    }
    std::stable_sort(groups.begin(), groups.end(), [](const std::vector<int>& a, const std::vector<int>& b) { return a.size() > b.size(); });
    for (size_t k = 0; k < std::min<size_t>(groups.size(), dens == 0 ? 1 : dens == 1 ? 3 : 6); k++) {
        const auto& g = groups[k];
        double cx = 0, cy = 0;
        for (int o : g) { cx += small[o].cx; cy += small[o].cy; }
        cx /= g.size(); cy /= g.size();
        double r = -INFINITY;
        for (int o : g) r = std::max(r, std::hypot(small[o].cx - cx, small[o].cy - cy));
        add("isles", cx, cy, r + S * 2);
    }

    // ── serras: o nome corre pela linha da crista ──
    const double TH = 0.40;
    std::vector<uint8_t> hi(N, 0), hi2(N, 0);
    for (int i = 0; i < N; i++) if (hg[i] / ref >= TH) hi[i] = 1;
    for (int gy = 0; gy < gh; gy++) for (int gx = 0; gx < gw; gx++) {      // dilata 1 célula: picos vizinhos viram uma serra
        uint8_t v = 0;
        for (int dy = -1; dy <= 1 && !v; dy++) for (int dx = -1; dx <= 1; dx++) {
            const int x = gx + dx, y = gy + dy;
            if (x >= 0 && y >= 0 && x < gw && y < gh && hi[y * gw + x]) { v = 1; break; }
        }
        hi2[gy * gw + gx] = v;
    }
    struct Range { double x, y, r; size_t area; Path pts; QString tpl; double px, py, ph; };
    std::vector<Range> ranges;
    for (const Comp& c : comps(gw, gh, [&](int i) { return hi2[i] == 1; }).list) {
        std::vector<int> cells;
        for (int i : c.cells) if (hi[i]) cells.push_back(i);
        if (double(cells.size()) * S * S < std::pow(M * 0.06 * K, 2)) continue;
        double sw = 0, mx = 0, my = 0;
        for (int i : cells) { const double wt = hg[i] / ref - TH + 0.04; sw += wt; mx += wx(i) * wt; my += wy(i) * wt; }
        mx /= sw; my /= sw;
        double xx = 0, xy = 0, yy = 0;
        for (int i : cells) { const double wt = hg[i] / ref - TH + 0.04, dx = wx(i) - mx, dy = wy(i) - my; xx += wt * dx * dx; xy += wt * dx * dy; yy += wt * dy * dy; }
        xx /= sw; xy /= sw; yy /= sw;
        const double tr = xx + yy, det = xx * yy - xy * xy, l1 = tr / 2 + std::sqrt(std::max(0.0, tr * tr / 4 - det)), l2 = std::max(1e-6, tr - l1);
        double ux = xy, uy = l1 - xx;
        if (std::hypot(ux, uy) < 1e-6) { ux = 1; uy = 0; }
        const double un = std::hypot(ux, uy);
        ux /= un; uy /= un;
        const double len = 4 * std::sqrt(l1), wid = 4 * std::sqrt(l2);
        // espinha: média da posição de lado a lado em faixas ao longo do eixo
        const int nb = std::max(3, std::min(24, static_cast<int>(jsRound(len / (S * 5.0)))));
        double u0 = INFINITY, u1 = -INFINITY;
        for (int i : cells) { const double u = (wx(i) - mx) * ux + (wy(i) - my) * uy; u0 = std::min(u0, u); u1 = std::max(u1, u); }
        std::vector<double> bs(nb, 0), bw2(nb, 0), bu(nb, 0);
        for (int i : cells) {
            const double dx = wx(i) - mx, dy = wy(i) - my, u = dx * ux + dy * uy, v = -dx * uy + dy * ux;
            const int b = std::min(nb - 1, static_cast<int>(std::floor((u - u0) / (u1 - u0 + 1e-6) * nb)));
            const double wt = hg[i] / ref - TH + 0.04;
            bs[b] += v * wt; bw2[b] += wt; bu[b] += u * wt;
        }
        Path pts;
        for (int b = 0; b < nb; b++) if (bw2[b] > 0) { const double u = bu[b] / bw2[b], v = bs[b] / bw2[b]; pts.push_back({ mx + u * ux - v * uy, my + u * uy + v * ux }); }
        const int top = argmax(cells, hg);
        Range R{ mx, my, len / 2, cells.size(), pts.size() >= 2 ? smoothPath(pts) : Path(),
                 len > M * 0.38 ? QStringLiteral("rangeL") : len / std::max(wid, 1.0) < 1.7 ? QStringLiteral("rangeS") : QStringLiteral("range"),
                 wx(top), wy(top), hg[top] };
        ranges.push_back(std::move(R));
    }
    std::stable_sort(ranges.begin(), ranges.end(), [](const Range& a, const Range& b) { return a.area > b.area; });
    ranges.resize(std::min<size_t>(ranges.size(), dens == 0 ? 3 : dens == 1 ? 6 : 10));
    for (const Range& g : ranges) { NameFeature& f = add("range", g.x, g.y, g.r); f.pts = g.pts; f.tpl = g.tpl; }
    {
        std::vector<const Range*> pk;
        for (const Range& g : ranges) pk.push_back(&g);
        std::stable_sort(pk.begin(), pk.end(), [](const Range* a, const Range* b) { return a->ph > b->ph; });
        for (size_t k = 0; k < std::min<size_t>(pk.size(), dens == 0 ? 0 : dens == 1 ? 2 : 5); k++) add("peak", pk[k]->px, pk[k]->py, S * 2.0);
    }

    // ── regiões de bioma (só no chão abaixo da serra) ──
    std::vector<int8_t> idg(N, -1);
    for (int i = 0; i < N; i++) {
        if (hg[i] <= 0 || hg[i] / ref >= TH) continue;
        const ClimateSample c = climateAt(w, wx(i), wy(i));
        if (c.valid) idg[i] = static_cast<int8_t>(c.id);
    }
    struct Bio { int id; const char* kind; double minF; int maxN[3]; };
    static const Bio BIOME[] = { { 1, "forest", 0.12, { 1, 3, 6 } }, { 3, "desert", 0.12, { 1, 3, 5 } }, { 0, "ice", 0.12, { 1, 2, 4 } },
                                 { 4, "marsh", 0.07, { 0, 2, 5 } }, { 2, "plains", 0.2, { 0, 1, 3 } } };
    for (const Bio& bio : BIOME) {
        auto mask = [&](int i) { return idg[i] == bio.id; };
        std::vector<float> out(N);
        for (int i = 0; i < N; i++) out[i] = mask(i) ? -1.f : 1.f;
        const std::vector<float> inner = distanceToLand(out, gw, gh);
        struct Reg { int p; double area; };
        std::vector<Reg> regs;
        for (const Comp& c : comps(gw, gh, mask).list) {
            const double area = double(c.cells.size()) * S * S;
            if (area < std::pow(M * bio.minF * K, 2)) continue;
            const int p = argmax(c.cells, inner);
            if (inner[p] * S < 10) continue;
            regs.push_back({ p, area });
        }
        std::stable_sort(regs.begin(), regs.end(), [](const Reg& a, const Reg& b) { return a.area > b.area; });
        for (size_t k = 0; k < std::min<size_t>(regs.size(), bio.maxN[dens]); k++)
            add(bio.kind, wx(regs[k].p), wy(regs[k].p), std::sqrt(regs[k].area / M_PI));
    }

    // ── rios pintados (o pincel de Rio guarda o traço) ──
    for (const River& rv : rivers) {
        Path wet;
        for (const Pt& p : rv.pts) if (heightAt(w, p[0], p[1]) <= 0) wet.push_back(p);
        if (wet.size() < 4 || wet.size() < rv.pts.size() * 0.5) continue;
        const Pt mid = wet[wet.size() >> 1];
        NameFeature& f = add("river", mid[0], mid[1], wet.size() * 3.0);
        f.pts = wet; f.rid = rv.id;
    }

    // nomes: em ordem de importância, cada um preso à sua posição (ou ao rio)
    std::stable_sort(F.begin(), F.end(), [](const NameFeature& a, const NameFeature& b) { return a.prio != b.prio ? a.prio > b.prio : a.r > b.r; });
    QSet<QString> used;
    const double seed = static_cast<double>(w.seed) * 31;
    for (NameFeature& f : F) {
        const int ki = kindIndex(f.kind) + 1;
        const double gx = f.rid >= 0 ? f.rid * 7919.0 : jsRound(f.x / 40), gy = f.rid >= 0 ? 17.0 : jsRound(f.y / 40);
        QString name;
        for (int salt = 0; salt < 8; salt++) {
            auto r = [&](int k) { return hash2(gx + salt * 101, gy, seed + ki * 977 + k * 13); };
            name = makeName(f.tpl.isEmpty() ? f.kind : f.tpl, r);
            if (!used.contains(name)) break;
        }
        used.insert(name);
        f.name = name;
        f.id = f.rid >= 0 ? QStringLiteral("river:%1").arg(f.rid)
                          : QStringLiteral("%1:%2:%3").arg(f.kind).arg(jsRound(f.x)).arg(jsRound(f.y));
    }
    return F;
}

// ── edições do usuário ──
namespace {
int matchEdit(const LabelLayer& L, const NameFeature& f)
{
    int best = -1;
    double bd = INFINITY;
    for (size_t k = 0; k < L.edits.size(); k++) {
        const NameEdit& e = L.edits[k];
        if (e.kind != f.kind) continue;
        if (e.id == f.id) return static_cast<int>(k);
        const double d = std::hypot(e.x - f.x, e.y - f.y);
        if (d < std::max(40.0, f.r * 0.6) && d < bd) { bd = d; best = static_cast<int>(k); }
    }
    return best;
}
}

std::vector<Label> resolveNames(const std::vector<NameFeature>& F, const LabelLayer& L, bool autoOn)
{
    std::vector<Label> out;
    if (autoOn) for (size_t i = 0; i < F.size(); i++) {
        const NameFeature& f = F[i];
        const int ei = matchEdit(L, f);
        const NameEdit* e = ei >= 0 ? &L.edits[ei] : nullptr;
        const double dx = e ? e->dx : 0, dy = e ? e->dy : 0;
        Label lab;
        lab.ref = f.id; lab.autoName = true; lab.feature = static_cast<int>(i); lab.edit = ei;
        lab.kind = f.kind; lab.text = e && !e->name.isEmpty() ? e->name : f.name;
        lab.x = f.x + dx; lab.y = f.y + dy; lab.r = f.r;
        lab.pts = f.pts;
        if (dx || dy) for (Pt& p : lab.pts) { p[0] += dx; p[1] += dy; }
        lab.size = e && e->size > 0 ? e->size : 1;
        lab.hidden = e && e->hidden;
        lab.touched = e && (!e->name.isEmpty() || e->dx || e->dy || e->size > 0);
        lab.prio = f.prio;
        out.push_back(lab);
    }
    for (size_t i = 0; i < L.custom.size(); i++) {
        const NameCustom& c = L.custom[i];
        Label lab;
        lab.ref = c.id; lab.autoName = false; lab.custom = static_cast<int>(i);
        lab.kind = c.style == QLatin1String("water") ? QStringLiteral("cwater") : QStringLiteral("cland");
        lab.text = c.text; lab.x = c.x; lab.y = c.y; lab.pts = c.pts; lab.r = c.r > 0 ? c.r : 90; lab.size = c.size > 0 ? c.size : 1;
        lab.prio = 1000;
        out.push_back(lab);
    }
    return out;
}

int editFor(LabelLayer& L, const std::vector<NameFeature>& F, Label& lab)
{
    if (lab.edit >= 0 && lab.edit < static_cast<int>(L.edits.size())) return lab.edit;
    if (lab.feature < 0 || lab.feature >= static_cast<int>(F.size())) return -1;
    const NameFeature& f = F[lab.feature];
    NameEdit e; e.id = f.id; e.kind = f.kind; e.x = f.x; e.y = f.y;
    L.edits.push_back(e);
    lab.edit = static_cast<int>(L.edits.size()) - 1;
    return lab.edit;
}

QString nameKindLabel(const QString& kind)
{
    static const QHash<QString, const char*> K = {
        { "ocean", QT_TRANSLATE_NOOP("carto", "Oceano") }, { "sea", QT_TRANSLATE_NOOP("carto", "Mar") }, { "bay", QT_TRANSLATE_NOOP("carto", "Baía") },
        { "lake", QT_TRANSLATE_NOOP("carto", "Lago") }, { "river", QT_TRANSLATE_NOOP("carto", "Rio") }, { "range", QT_TRANSLATE_NOOP("carto", "Serra") },
        { "peak", QT_TRANSLATE_NOOP("carto", "Pico") }, { "forest", QT_TRANSLATE_NOOP("carto", "Floresta") }, { "desert", QT_TRANSLATE_NOOP("carto", "Deserto") },
        { "marsh", QT_TRANSLATE_NOOP("carto", "Pântano") }, { "ice", QT_TRANSLATE_NOOP("carto", "Gelo") }, { "plains", QT_TRANSLATE_NOOP("carto", "Campos") },
        { "island", QT_TRANSLATE_NOOP("carto", "Ilha") }, { "isles", QT_TRANSLATE_NOOP("carto", "Arquipélago") },
        { "cland", QT_TRANSLATE_NOOP("carto", "Nome livre · terra") }, { "cwater", QT_TRANSLATE_NOOP("carto", "Nome livre · água") },
    };
    auto it = K.find(kind);
    return it == K.end() ? QCoreApplication::translate("carto", "Nome") : QCoreApplication::translate("carto", it.value());
}

// ── gravar e ler ──
namespace {
QJsonArray pathJ(const Path& p) { QJsonArray a; for (const Pt& q : p) a.append(QJsonArray{ std::round(q[0] * 100) / 100, std::round(q[1] * 100) / 100 }); return a; }
Path jPath(const QJsonValue& v) { Path p; for (const QJsonValue& q : v.toArray()) { const QJsonArray a = q.toArray(); p.push_back({ a.at(0).toDouble(), a.at(1).toDouble() }); } return p; }
}

QJsonObject LabelLayer::toJson() const
{
    QJsonArray E, C;
    for (const NameEdit& e : edits) {
        QJsonObject o{ { "id", e.id }, { "kind", e.kind }, { "x", e.x }, { "y", e.y } };
        if (!e.name.isEmpty()) o.insert("name", e.name);
        if (e.dx) o.insert("dx", e.dx);
        if (e.dy) o.insert("dy", e.dy);
        if (e.size > 0) o.insert("size", e.size);
        if (e.hidden) o.insert("hidden", true);
        E.append(o);
    }
    for (const NameCustom& c : custom) {
        QJsonObject o{ { "id", c.id }, { "text", c.text }, { "style", c.style }, { "size", c.size }, { "x", c.x }, { "y", c.y }, { "r", c.r } };
        if (!c.pts.empty()) o.insert("pts", pathJ(c.pts));
        C.append(o);
    }
    return QJsonObject{ { "edits", E }, { "custom", C }, { "seq", seq } };
}

LabelLayer LabelLayer::fromJson(const QJsonObject& o)
{
    LabelLayer L;
    for (const QJsonValue& v : o.value("edits").toArray()) {
        const QJsonObject a = v.toObject();
        NameEdit e;
        e.id = a.value("id").toString(); e.kind = a.value("kind").toString(); e.x = a.value("x").toDouble(); e.y = a.value("y").toDouble();
        e.name = a.value("name").toString(); e.dx = a.value("dx").toDouble(); e.dy = a.value("dy").toDouble(); e.size = a.value("size").toDouble();
        e.hidden = a.value("hidden").toBool();
        L.edits.push_back(e);
    }
    for (const QJsonValue& v : o.value("custom").toArray()) {
        const QJsonObject a = v.toObject();
        NameCustom c;
        c.id = a.value("id").toString(); c.text = a.value("text").toString(); c.style = a.value("style").toString(QStringLiteral("land"));
        c.size = a.value("size").toDouble(1); c.x = a.value("x").toDouble(); c.y = a.value("y").toDouble(); c.r = a.value("r").toDouble(90);
        c.pts = jPath(a.value("pts"));
        L.custom.push_back(c);
    }
    L.seq = o.value("seq").toInt();
    return L;
}

bool LabelLayer::operator==(const LabelLayer& o) const { return toJson() == o.toJson(); }

} // namespace carto
