// As texturas equiretangulares dos corpos e a esfera sombreada (porte de core.js).

#include "GalaxyCore.h"

#include <algorithm>

namespace galaxy {

namespace {
inline uint8_t u8c(double v)
{
    if (!(v > 0)) return 0;
    if (v >= 255) return 255;
    const double f = std::floor(v), d = v - f;
    if (d < 0.5) return uint8_t(f);
    if (d > 0.5) return uint8_t(f + 1);
    return int(f) % 2 == 0 ? uint8_t(f) : uint8_t(f + 1);
}
inline double hypot3(double x, double y, double z)
{
    x = std::fabs(x); y = std::fabs(y); z = std::fabs(z);
    const double mx = std::max(x, std::max(y, z));
    if (mx == 0) return 0;
    double sum = 0, comp = 0;
    for (double v : { x, y, z }) { const double n = v / mx, summand = n * n - comp, pre = sum + summand; comp = (pre - sum) - summand; sum = pre; }
    return std::sqrt(sum) * mx;
}
void ramp(const std::vector<PalStop>& stops, double t, double* out)
{
    t = clamp(t, 0, 1);
    size_t i = 1;
    while (i < stops.size() - 1 && stops[i].t < t) i++;
    const PalStop& a = stops[i - 1];
    const PalStop& b = stops[i];
    const double den = b.t - a.t;
    const double k = clamp((t - a.t) / (den != 0 ? den : 1), 0, 1);
    out[0] = mix(a.c[0], b.c[0], k); out[1] = mix(a.c[1], b.c[1], k); out[2] = mix(a.c[2], b.c[2], k);
}

struct OldCrater { double x, y, z, rad, lim; };
struct Crater { double x, y, z, r, lim, s, depth, flat, peak; bool fresh, spot; V3 e1, e2; };
struct Ex {
    double cmask = 0, spec = 0;
    std::vector<Crater> rockCr;
    bool hasRockCr = false;
    std::vector<OldCrater> craters;
    bool hasCraters = false;
};
struct RockOut { double br = 0, floor = 0, spot = 0, mare = 0, pat = 0, gro = 0, plain = 0, lines = 0, crack = 0, cellsP = 0; int band = 0; };

std::vector<OldCrater> craterList(uint32_t seed, int count)
{
    Rng r(seed ^ 0x51edu);
    std::vector<OldCrater> out;
    for (int i = 0; i < count; i++) {
        const double z = uni(r, -1, 1), t = r() * TAU, s = std::sqrt(1 - z * z);
        const double rad = std::pow(r(), 2.2) * 0.32 + 0.02;
        out.push_back({ s * std::cos(t), s * std::sin(t), z, rad, 1 - std::pow(rad * 1.3, 2) / 2 });
    }
    return out;
}
void applyCraters(double* col, double x, double y, double z, const std::vector<OldCrater>& list)
{
    double f = 1;
    for (const OldCrater& c : list) {
        const double d = x * c.x + y * c.y + z * c.z;
        if (d < c.lim) continue;
        const double t = std::sqrt(std::max(0.0, 2 * (1 - d))) / c.rad;
        if (t < 0.85) f *= 0.8 + 0.14 * t;
        else if (t < 1.25) f *= 1 + 0.22 * std::max(0.0, 1 - std::fabs(t - 1.05) / 0.2);
    }
    col[0] *= f; col[1] *= f; col[2] *= f;
}
// Crateras grandes: bacia, borda alta, pico central nas maiores; as novas têm chão claro e raios.
std::vector<Crater> rockCraters(const Look& L)
{
    Rng r(L.seed ^ 0x51edu);
    std::vector<Crater> out;
    for (int i = 0; i < L.nCr; i++) {
        const double z = uni(r, -1, 1), t = r() * TAU, s = std::sqrt(1 - z * z);
        const double x = s * std::cos(t), y = s * std::sin(t);
        const double rad = 0.014 + 0.25 * std::pow(r(), 3.2);
        const bool fresh = r() < L.fresh;
        const double reach = std::min(PI_D, rad * (fresh ? 5.5 : 1.8));
        // Base tangente pra medir o ângulo dos raios.
        V3 e1 = std::fabs(z) < 0.9 ? V3 { -y, x, 0 } : V3 { 0, -z, y };
        const double n1 = hypot3(e1[0], e1[1], e1[2]);
        e1 = { e1[0] / n1, e1[1] / n1, e1[2] / n1 };
        const V3 e2 { y * e1[2] - z * e1[1], z * e1[0] - x * e1[2], x * e1[1] - y * e1[0] };
        Crater c;
        c.x = x; c.y = y; c.z = z; c.r = rad; c.lim = std::cos(reach); c.fresh = fresh; c.e1 = e1; c.e2 = e2; c.s = r() * 50;
        c.depth = rad * 0.32 * (rad < 0.05 ? 1 : std::sqrt(0.05 / rad)); c.flat = rad > 0.08 ? 0.62 : 1; c.peak = rad > 0.07 ? 0.4 : 0;
        c.spot = fresh && r() < 0.35;
        out.push_back(c);
    }
    return out;
}
inline uint32_t hash3(uint32_t i, uint32_t j, uint32_t k, uint32_t s)
{
    uint32_t h = (i * 0x27d4eb2du) ^ (j * 0x165667b1u) ^ (k * 0x9e3779b1u) ^ s;
    h = (h ^ (h >> 15)) * 0x85ebca6bu; h = (h ^ (h >> 13)) * 0xc2b2ae35u;
    return h ^ (h >> 16);
}
// Crateras pequenas sem lista: uma por célula de uma grade 3D, cada uma com seu tamanho.
double cellCraters(double x, double y, double z, double sc, double prob, int seed)
{
    const double px = x * sc, py = y * sc, pz = z * sc;
    const int ix = int(std::floor(px)), iy = int(std::floor(py)), iz = int(std::floor(pz));
    double h = 0;
    const uint32_t us = uint32_t(seed);
    for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) for (int dz = -1; dz <= 1; dz++) {
        uint32_t hs = hash3(uint32_t(ix + dx), uint32_t(iy + dy), uint32_t(iz + dz), us);
        if (double(hs & 1023) > prob * 1024) continue;
        hs = hash3(hs, 7, 3, us);
        const double fx = (hs & 1023) / 1024.0, fy = ((hs >> 10) & 1023) / 1024.0, fz = ((hs >> 20) & 1023) / 1024.0;
        const double fr = (hash3(hs, 1, 9, us) & 1023) / 1024.0, rad = 0.16 + 0.3 * fr * fr;
        const double rx = ix + dx + fx - px, ry = iy + dy + fy - py, rz = iz + dz + fz - pz;
        const double t2 = (rx * rx + ry * ry + rz * rz) / (rad * rad);
        if (t2 > 2.4) continue;
        const double t = std::sqrt(t2);
        const double p = (t < 1 ? t2 - 1 : 0) + 0.22 * std::exp(-((t - 1) * (t - 1)) / 0.05);
        h += (p * rad * 0.3) / sc;
    }
    return h;
}
// Rachaduras em polígonos: distância até a fronteira entre as duas células mais perto.
double cellEdge(double x, double y, double z, double sc, int seed)
{
    const double px = x * sc, py = y * sc, pz = z * sc;
    const int ix = int(std::floor(px)), iy = int(std::floor(py)), iz = int(std::floor(pz));
    double f1 = 9, f2 = 9;
    for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) for (int dz = -1; dz <= 1; dz++) {
        const uint32_t hs = hash3(uint32_t(ix + dx), uint32_t(iy + dy), uint32_t(iz + dz), uint32_t(seed));
        const double rx = ix + dx + (hs & 1023) / 1024.0 - px, ry = iy + dy + ((hs >> 10) & 1023) / 1024.0 - py, rz = iz + dz + ((hs >> 20) & 1023) / 1024.0 - pz;
        const double d = std::sqrt(rx * rx + ry * ry + rz * rz);
        if (d < f1) { f2 = f1; f1 = d; } else if (d < f2) f2 = d;
    }
    return f2 - f1;
}
double craterField(const Noise& nz, const std::vector<Crater>& list, double x, double y, double z, double w, RockOut* o)
{
    double h = 0, br = 0, fl = 0, spot = 0;
    for (const Crater& c : list) {
        const double d = x * c.x + y * c.y + z * c.z;
        if (d < c.lim) continue;
        const double t = std::sqrt(std::max(0.0, 2 * (1 - d))) / c.r;
        double p = 0;
        if (t < 1) { const double bowl = t * t - 1; p = bowl > -c.flat ? bowl : -c.flat + (bowl + c.flat) * 0.15; if (c.peak != 0) p += c.peak * std::exp(-(t * t) / 0.025); }
        p += 0.24 * std::exp(-((t - 1) * (t - 1)) / 0.045);
        if (t > 1) p += 0.07 * std::exp(-(t - 1) * 1.6);
        h += p * c.depth * (c.fresh ? 1 : w);
        if (o) {
            if (t < 1) fl = std::max(fl, (1 - t) * (c.fresh ? 1 : w));
            if (c.fresh) {
                br += 0.3 * std::exp(-std::max(0.0, t - 0.6) * 1.9);
                if (t > 1 && t < 5.5) {
                    const double ca = x * c.e1[0] + y * c.e1[1] + z * c.e1[2], sa = x * c.e2[0] + y * c.e2[1] + z * c.e2[2];
                    const double an = std::atan2(sa, ca);
                    // Raios finos e irregulares: muitos, de comprimentos diferentes, que se quebram no caminho.
                    const double ca2 = std::cos(an), sa2 = std::sin(an);
                    const double fine = nz.n3(ca2 * 26 + c.s, sa2 * 26, t * 0.9) * 0.6 + nz.n3(ca2 * 9, sa2 * 9 + c.s, t * 0.35) * 0.4;
                    const double len = 2 + 3.5 * (0.5 + 0.5 * nz.n3(ca2 * 6 + c.s, sa2 * 6, 7));
                    const double ray = smooth(0.18, 0.5, fine) * (1 - smooth(len * 0.6, len, t));
                    br += ray * 0.42 * std::exp(-(t - 1.1) / 1.8) * smooth(1, 1.4, t);
                }
                if (c.spot && t < 0.35) spot = std::max(spot, 1 - t / 0.35);
            }
        }
    }
    if (o) { o->br = br; o->floor = fl; o->spot = spot; }
    return h;
}
// Altura do terreno rochoso (o relevo sai da diferença entre dois pontos vizinhos).
double rockHeight(const Noise& nz, const Look& L, const std::vector<Crater>* cr, double x, double y, double z, RockOut* o)
{
    const LookStyle st = L.style;
    const double k = L.k1;
    double h = nz.fbm3(x * k * 2.2, y * k * 2.2, z * k * 2.2, 5) * 0.03 + nz.fbm3(x * 9, y * 9, z * 9, 3) * 0.006;
    double cw = 1, mare = 0, pat = 0, gro = 0, plain = 0;
    if (st == LookStyle::Mares) {
        mare = smooth(L.thr * 0.3 - 0.08, L.thr * 0.3 - 0.15, nz.fbm3(x * k + 5, y * k, z * k, 4) + nz.fbm3(x * 5, y * 5, z * 5, 2) * 0.12);
        h = mix(h, -0.02 + h * 0.15, mare); cw = 1 - 0.85 * mare;
    } else if (st == LookStyle::Ferrugem) {
        pat = smooth(L.thr, L.thr + 0.18, nz.fbm3(x * 1.3 + 9, y * 1.3, z * 1.3, 5));
        if (L.canyon) h -= smooth(0.88, 0.975, nz.ridge3(x * 1.4 + 4, y * 1.4, z * 1.4 + 2, 4)) * 0.035;
        for (const auto& v : L.volc) {
            const double d = x * v[0] + y * v[1] + z * v[2];
            if (d < 0.9) continue;
            const double t = std::sqrt(std::max(0.0, 2 * (1 - d))) / v[3];
            if (t < 1.3) h += v[3] * 0.3 * std::pow(std::max(0.0, 1 - t / 1.3), 1.6) - (t < 0.14 ? v[3] * 0.08 * (1 - t / 0.14) : 0);
        }
    } else if (st == LookStyle::Fraturado) {
        gro = smooth(-0.04, 0.08, nz.fbm3(x * 1.2 + 2, y * 1.2, z * 1.2, 4));
        const double wv = nz.fbm3(x * 2, y * 2, z * 2, 3);
        const double lines = nz.ridge3(x * 3 + wv * 2, y * 3 - wv, z * 22 + wv * 6, 2);
        h += gro * (lines * lines * (3 - 2 * lines) * 0.008 + 0.01);
        h += smooth(0, 0.015, nz.fbm3(x * 2.6 + 7, y * 2.6, z * 2.6, 4) - L.thr) * 0.01;
        cw = 1 - 0.65 * gro;
        if (o) o->lines = lines;
    } else if (st == LookStyle::Sal) {
        const double crack = (1 - smooth(0, 0.07, cellEdge(x, y, z, L.cellF, L.cseed))) * smooth(-0.08, 0.12, nz.fbm3(x * 1.7 + 13, y * 1.7, z * 1.7, 4));
        if (o) o->crack = crack;
        h = h * 0.5 - crack * 0.004;
    } else if (st == LookStyle::Estratos) {
        const double bnd = (nz.fbm3(x * 1.4, y * 1.4, z * 1.4, 5) + 0.5) * L.bandsI, i = std::floor(bnd), f = bnd - i;
        h = ((i + smooth(0.75, 1, f)) / L.bandsI) * 0.07 + nz.fbm3(x * 12, y * 12, z * 12, 2) * 0.003;
        h -= smooth(0.9, 0.98, nz.ridge3(x * 4 + 1, y * 4, z * 4, 3)) * 0.012;
        if (o) o->band = int(i);
    } else if (st == LookStyle::Tolinas) {
        const double d = x * L.plain[0] + y * L.plain[1] + z * L.plain[2];
        const double edge = std::cos(L.plainR) + nz.fbm3(x * 3, y * 3, z * 3, 3) * 0.12;
        plain = smooth(edge - 0.02, edge + 0.04, d);
        h = mix(h, -0.012, plain);
        cw = 1 - plain;
        // Montanhas de blocos de gelo na borda da planície.
        const double rim = smooth(edge - 0.12, edge - 0.01, d) * (1 - plain);
        h += rim * std::max(0.0, nz.n3(x * 18, y * 18, z * 18)) * 0.02;
        if (o) o->cellsP = plain > 0.5 ? 1 - smooth(0, 0.08, cellEdge(x, y, z, 14, L.cseed)) : 0;
    }
    if (cr) h += craterField(nz, *cr, x, y, z, cw, o);
    if (L.cells > 0) {
        h += cellCraters(x, y, z, 11, L.cells * cw, L.cseed) * 1.2;
        h += cellCraters(x, y, z, 27, L.cells * cw, L.cseed + 1);
    }
    if (o) { o->mare = mare; o->pat = pat; o->gro = gro; o->plain = plain; }
    return h;
}
inline void tint3(double* col, const V3& c, double k) { col[0] = mix(col[0], c[0], k); col[1] = mix(col[1], c[1], k); col[2] = mix(col[2], c[2], k); }
inline V3 sc3(const V3& c, double k) { return { c[0] * k, c[1] * k, c[2] * k }; }
double shadeRock(const Body& b, const Noise& nz, double x, double y, double z, double lat, double* col, const Ex& ex, RockOut& o)
{
    const Look& L = b.look;
    const LookStyle st = L.style;
    const std::vector<Crater>* cr = ex.hasRockCr ? &ex.rockCr : nullptr;
    const double H = rockHeight(nz, L, cr, x, y, z, &o);
    // O vizinho tem que ficar na superfície: fora dela, as crateras grandes escurecem por igual.
    const double e = 0.0035, qx = x - y * e, qy = y + x * e, qz = z + e * 0.7, qn = hypot3(qx, qy, qz);
    const double H2 = rockHeight(nz, L, cr, qx / qn, qy / qn, qz / qn, nullptr);
    const double relief = clamp(1 + (H2 - H) * 120, 0.62, 1.4);
    const double f = nz.fbm3(x * 2.4 * L.k1 + 11, y * 2.4 * L.k1, z * 2.4 * L.k1, 5), g = nz.fbm3(x * 8, y * 8, z * 8, 3);
    const double v = clamp(0.5 + f * 1.15 + g * 0.35, 0, 1);
    const V3& lo = L.lo; const V3& hi = L.hi; const V3& ac = L.ac;
    col[0] = mix(lo[0], hi[0], v); col[1] = mix(lo[1], hi[1], v); col[2] = mix(lo[2], hi[2], v);
    if (st == LookStyle::Mares) tint3(col, sc3(ac, 0.85 + g * 0.5), o.mare * 0.92);
    else if (st == LookStyle::Ferrugem) tint3(col, ac, o.pat * 0.75);
    else if (st == LookStyle::Fraturado) tint3(col, sc3(ac, 0.82 + 0.25 * o.lines), o.gro * 0.85);
    else if (st == LookStyle::Carbono) { const double d = (0.75 + 0.5 * v) * 1.75; col[0] = lo[0] * d; col[1] = lo[1] * d; col[2] = lo[2] * d; }
    else if (st == LookStyle::Sal) { tint3(col, ac, smooth(0.05, 0.3, nz.fbm3(x * 2 + 3, y * 2, z * 2, 4)) * 0.6); tint3(col, sc3(lo, 0.8), o.crack * 0.7); }
    else if (st == LookStyle::Estratos) tint3(col, sc3(L.bandCols[size_t(((o.band % 5) + 5) % 5)], 0.9 + g * 0.3), 0.55);
    else if (st == LookStyle::Tolinas) {
        // Faixa escura perto do equador e a planície de gelo dividida em células, como em Plutão.
        tint3(col, V3 { lo[0] * 0.7, lo[1] * 0.6, lo[2] * 0.6 }, smooth(0.05, 0.25, nz.fbm3(x * 1.6 + 4, y * 1.6, z * 1.6, 4)) * (1 - smooth(0.25, 0.55, std::fabs(lat))) * 0.85);
        tint3(col, ac, o.plain); tint3(col, V3 { ac[0] * 0.78, ac[1] * 0.78, ac[2] * 0.8 }, o.cellsP * 0.6);
        tint3(col, hi, smooth(0.9, 1.2, std::fabs(lat)) * 0.5);
    } else if (st == LookStyle::Geada) tint3(col, ac, smooth(0.15, 0.35, nz.fbm3(x * 1.5 + 6, y * 1.5, z * 1.5, 4)) * 0.6);
    // Material novo das crateras recentes: claro, em raios.
    const V3 fresh = st == LookStyle::Carbono ? V3 { 0.62, 0.6, 0.57 } : V3 { std::min(1.0, hi[0] * 1.2), std::min(1.0, hi[1] * 1.2), std::min(1.0, hi[2] * 1.2) };
    tint3(col, fresh, std::min(0.85, o.br * (st == LookStyle::Carbono ? 1.3 : 0.9)));
    if (o.spot != 0) tint3(col, V3 { 0.97, 0.97, 0.95 }, o.spot * 0.95);
    if (L.frost != 0) tint3(col, V3 { 0.88, 0.9, 0.94 }, L.frost * std::max(smooth(1.0, 1.3, std::fabs(lat) + nz.fbm3(x * 5 + 2, y * 5, z * 5, 4) * 0.5) * (0.75 + 0.25 * v), o.floor * smooth(0.6, 1.2, std::fabs(lat) + 0.4)));
    col[0] *= relief; col[1] *= relief; col[2] *= relief;
    return 0;
}

// ── mundos artificiais: placas de casco na textura, pra girar e receber luz como qualquer planeta ──
struct HC { int row, col, cols; double fu, fv; };
inline HC hullCell(double lat, double lon, int rows)
{
    const double v = (lat / PI_D + 0.5) * rows;
    const int row = std::min(rows - 1, int(std::floor(v)));
    const double latC = ((row + 0.5) / rows - 0.5) * PI_D;
    const int cols = std::max(4, int(std::floor(rows * 2 * std::cos(latC) + 0.5)));
    const double u = ((lon + PI_D) / TAU) * cols;
    const int c = int(std::floor(u));
    return { row, (c + cols) % cols, cols, u - c, v - row };
}
inline double seam(double f, double w) { const double d = std::min(f, 1 - f); return d < w ? 1 - d / w : 0; }
inline double unitHash(int i, int j, int k, uint32_t s) { return (hash3(uint32_t(i), uint32_t(j), uint32_t(k), s) & 1023) / 1023.0; }
inline void mix3(double* col, double r, double g, double b, double k) { col[0] = mix(col[0], r, k); col[1] = mix(col[1], g, k); col[2] = mix(col[2], b, k); }

double shadeArtificial(const Body& b, const Noise& nz, double x, double y, double z, double lat, double lon, double* col, Ex& ex)
{
    const Look& L = b.look;
    const Type t = b.type;
    const uint32_t s = L.seed & 4095u;
    if (t == Type::Fortaleza) {
        static const V3 HULL_TONE[] = { { 0.56, 0.58, 0.61 }, { 0.6, 0.56, 0.49 }, { 0.3, 0.32, 0.36 } };
        const V3& tone = HULL_TONE[L.tone];
        const HC big = hullCell(lat, lon, 14);
        const double hb = unitHash(big.row, big.col, 1, s), sb = std::max(seam(big.fv, 0.035), seam(big.fu, 0.035));
        const HC mid = hullCell(lat, lon, 46);
        const double hm = unitHash(mid.row, mid.col, 2, s), sm = std::max(seam(mid.fv, 0.06), seam(mid.fu, 0.06));
        const HC fine = hullCell(lat, lon, 210);
        const double hf = unitHash(fine.row, fine.col, 3, s);
        const bool lit = hf > 0.982;
        double v = 0.72 + 0.26 * hb + 0.16 * (hm - 0.5) + 0.08 * (hf - 0.5);
        v *= 1 - sb * 0.55; v *= 1 - sm * 0.18;
        double e = 0, wk = lit ? 1 : 0;
        const double wr = 0.98, wg = 0.84, wb = 0.56;
        // Faixas de latitude e a trincheira do equador.
        for (double q : { 0.52, -0.52, 1.02, -1.02 }) { const double d = std::fabs(lat - q); if (d < 0.012) v *= 0.45; }
        const double tr = std::fabs(lat);
        if (tr < 0.034) { v *= 0.14 + 0.2 * (tr / 0.034); wk = hf > 0.9 ? 1 : 0; }
        else if (tr < 0.042) v *= 1.3;
        col[0] = tone[0] * v; col[1] = tone[1] * v; col[2] = tone[2] * v;
        // O prato: uma concha funda com raios e o emissor no centro.
        const V3& D = L.dish;
        const double dc = clamp(x * D[0] + y * D[1] + z * D[2], -1, 1), ang = std::acos(dc), R = 0.4;
        if (ang < R * 1.05) {
            const double k = ang / R;
            if (k < 1) {
                const V3 ex0 = std::fabs(D[2]) > 0.9 ? V3 { 1, 0, 0 } : V3 { 0, 0, 1 };
                const double ux = D[1] * ex0[2] - D[2] * ex0[1], uy = D[2] * ex0[0] - D[0] * ex0[2], uz = D[0] * ex0[1] - D[1] * ex0[0], un = hypot3(ux, uy, uz);
                const double vx = D[1] * uz - D[2] * uy, vy = D[2] * ux - D[0] * uz, vz = D[0] * uy - D[1] * ux;
                const double a = std::atan2((x * vx + y * vy + z * vz) / un, (x * ux + y * uy + z * uz) / un);
                const double spoke = std::fabs(std::sin(a * 4)) < 0.035 ? 1 : 0, rings = std::fabs(std::sin(k * 21)) < 0.09 ? 1 : 0;
                const double bowl = (0.16 + 0.62 * k * k) * (1 + spoke * 0.55 + rings * 0.22);
                col[0] = tone[0] * bowl; col[1] = tone[1] * bowl; col[2] = tone[2] * bowl;
                wk = rings != 0 && hf > 0.6 ? 1 : 0;
                if (k < 0.13) { const double g = 1 - k / 0.13; mix3(col, 0.55, 1, 0.72, g); e = g; wk = 0; }
            } else { col[0] *= 1.35; col[1] *= 1.35; col[2] *= 1.35; wk = 0; }
        }
        if (wk != 0) { mix3(col, wr, wg, wb, 0.6); e = std::max(e, 0.85); }
        return e;
    }
    if (t == Type::Ecumeno) {
        // A cidade cobre o planeta: quadras, avenidas e sobra pouco mar. As luzes vêm do mapa de cidades.
        const bool sea = nz.fbm3(x * 1.8 + 7, y * 1.8, z * 1.8, 4) < -0.24;
        ex.cmask = sea ? 0 : 1;
        if (sea) { col[0] = 0.04; col[1] = 0.1; col[2] = 0.15; ex.spec = 1; return 0; }
        const double zone = nz.fbm3(x * 2.4, y * 2.4, z * 2.4, 4);
        const HC blk = hullCell(lat, lon, 190);
        const double hb = unitHash(blk.row, blk.col, 4, s), sb = std::max(seam(blk.fv, 0.1), seam(blk.fu, 0.1));
        const HC dist = hullCell(lat, lon, 23);
        const double sd = std::max(seam(dist.fv, 0.016), seam(dist.fu, 0.016)) * smooth(-0.1, 0.1, nz.fbm3(x * 5 + 3, y * 5, z * 5, 2));
        const double base = 0.44 + 0.14 * hb + zone * 0.42;
        static const V3 WARM[] = { { 0.62, 0.56, 0.5 }, { 0.5, 0.52, 0.56 }, { 0.58, 0.5, 0.46 } };
        const V3& warm = WARM[L.tone];
        const double v = base * (1 - sb * 0.16) * (1 - sd * 0.5);
        col[0] = warm[0] * v; col[1] = warm[1] * v; col[2] = warm[2] * v;
        const double cloud = smooth(0.2, 0.42, nz.fbm3(x * 3 + 21, y * 3, z * 3, 4));
        if (cloud > 0) { const double c = cloud * 0.55; mix3(col, 0.8, 0.8, 0.82, c); }
        return 0;
    }
    if (t == Type::Casco) {
        // Um planeta inteiro construído: placas escuras e costuras de energia.
        static const V3 GLOW[] = { { 0.36, 0.86, 1 }, { 0.62, 1, 0.74 }, { 1, 0.62, 0.4 } };
        const V3& glowC = GLOW[L.tone];
        const HC big = hullCell(lat, lon, 11);
        const double hb = unitHash(big.row, big.col, 5, s), sb = std::max(seam(big.fv, 0.022), seam(big.fu, 0.022));
        const HC mid = hullCell(lat, lon, 64);
        const double hm = unitHash(mid.row, mid.col, 6, s), sm = std::max(seam(mid.fv, 0.05), seam(mid.fu, 0.05));
        const double v = 0.14 + 0.13 * hb + 0.07 * (hm - 0.3);
        col[0] = v * 0.86 * (1 - sm * 0.35); col[1] = v * 0.95 * (1 - sm * 0.35); col[2] = v * 1.1 * (1 - sm * 0.35);
        double e = 0;
        const bool on = sb > 0 && unitHash(big.row, big.fv < 0.5 ? big.col : big.col + 99, 7, s) > 0.3;
        if (on) { const double g = sb * sb; mix3(col, glowC[0], glowC[1], glowC[2], g); e = g * 1.2; }
        if (hm > 0.988) { const double g = 1 - sm; mix3(col, glowC[0], glowC[1], glowC[2], g * 0.8); e = std::max(e, g * 0.8); }
        const double pole = std::fabs(lat);
        if (pole > 1.22 && pole < 1.27) { col[0] = glowC[0]; col[1] = glowC[1]; col[2] = glowC[2]; e = 1; }
        return e;
    }
    // Mundo-forja: basalto escuro, rachaduras de metal fundido e fornalhas em fileira.
    const double f = nz.fbm3(x * 3, y * 3, z * 3, 5), rock = 0.09 + f * 0.1;
    col[0] = rock * 1.05; col[1] = rock * 0.95; col[2] = rock * 0.9;
    const HC plate = hullCell(lat, lon, 72);
    const double hp = unitHash(plate.row, plate.col, 8, s), sp = std::max(seam(plate.fv, 0.06), seam(plate.fu, 0.06));
    const double works = smooth(-0.05, 0.14, nz.fbm3(x * 2 + 11, y * 2, z * 2, 3));
    if (works > 0) { const double v = mix(rock, 0.13 + 0.07 * hp, works) * (1 - sp * 0.35 * works); col[0] = v; col[1] = v * 0.95; col[2] = v * 0.92; }
    const double crack = smooth(0.72, 0.88, nz.ridge3(x * 2.1 + 3, y * 2.1, z * 2.1, 5));
    double e = 0;
    if (crack > 0) { mix3(col, 1, 0.42, 0.1, crack); e = crack; }
    const HC furnace = hullCell(lat, lon, 170);
    const double hc = unitHash(furnace.row, furnace.col, 9, s);
    if (hc > 0.93 && works > 0.4) { const double g = 0.75 * works; mix3(col, 1, 0.58, 0.2, g); e = std::max(e, g); }
    return e;
}

// Altura do terreno dos mundos vivos (o mar fica abaixo de L.sea).
inline double livingHeight(const Noise& nz, const Look& L, double x, double y, double z)
{
    const double F = L.freq;
    return nz.fbm3(x * F + 3.1, y * F, z * F, 6) + 0.16 * nz.fbm3(x * 4, y * 4, z * 4, 3);
}
// Regiões povoadas: crescem com a população.
inline double cityRegion(const Noise& nz, double x, double y, double z, double v)
{
    const double c = nz.fbm3(x * 6 + 40, y * 6 - 7, z * 6, 4);
    const double thr = mix(0.3, -0.16, std::pow(v, 0.8));
    return smooth(thr, thr + 0.16, c);
}
struct CityC { double x, y, z, r, k; };
// Metrópoles: poucos centros em terra firme, uns muito maiores que os outros (a maioria é pequena).
std::vector<CityC> cityCenters(const Body& b, const Noise& nz, double v)
{
    Rng r(b.look.seed ^ 0xc17e5u);
    const Look& L = b.look;
    const bool living = L.hasFreq;
    const int want = int(std::floor(3 + 34 * std::pow(v, 1.4) + 0.5));
    std::vector<CityC> out;
    for (int t = 0; t < want * 30 && int(out.size()) < want; t++) {
        const double z = uni(r, -0.92, 0.92), a = r() * TAU, rr = std::sqrt(1 - z * z);
        const double x = rr * std::cos(a), y = rr * std::sin(a);
        if (living) {
            if (livingHeight(nz, L, x, y, z) < L.sea + 0.005) continue;
            if (std::fabs(std::asin(z)) > (L.cap != 0 ? L.cap : 9) - 0.12) continue;
        }
        if (cityRegion(nz, x, y, z, std::min(1.0, v + 0.15)) < 0.2) continue;
        const double big = std::pow(r(), 2.6);
        out.push_back({ x, y, z, (0.012 + 0.05 * big) * (0.7 + 0.6 * v), 0.5 + 0.6 * big });
    }
    return out;
}
// Luz de um ponto: metrópoles (núcleo forte, subúrbio em volta), cidades médias, vilas e o pontilhado rural.
double cityLight(const Noise& nz, double x, double y, double z, double v, const std::vector<CityC>& centers)
{
    const double region = cityRegion(nz, x, y, z, v);
    double metro = 0;
    for (const CityC& c : centers) {
        const double d = x * c.x + y * c.y + z * c.z;
        if (d < 0.9) continue;
        const double ang = std::sqrt(std::max(0.0, 2 * (1 - d))) / c.r;
        if (ang > 3) continue;
        metro += c.k * std::exp(-std::pow(ang, 1.2) * 2.2);
    }
    const double f1 = nz.fbm3(x * 40 + 5, y * 40, z * 40, 3) * 0.5 + 0.5;
    const double f2 = nz.n3(x * 230, y * 230 + 3, z * 230) * 0.5 + 0.5;
    const double f3 = nz.n3(x * 420 + 9, y * 420, z * 420) * 0.5 + 0.5;
    // Grão: no miolo da metrópole a luz é contínua; pra fora, vira bairros e pontos.
    const double grain = smooth(0.42, 0.72, f2 * 0.6 + f3 * 0.4);
    const double metroL = metro > 0.02 ? std::min(1.3, metro) * mix(0.2 + 0.8 * grain, 1, smooth(0.6, 1.3, metro)) : 0;
    const double towns = region * smooth(mix(0.7, 0.5, v), mix(0.84, 0.68, v), f1 * 0.6 + f2 * 0.4) * (0.45 + 0.25 * grain);
    const double rural = smooth(-0.15, 0.2, nz.fbm3(x * 6 + 40, y * 6 - 7, z * 6, 4) + v * 0.3) * smooth(0.8 - v * 0.08, 0.93, f3) * (0.16 + 0.14 * v);
    const double roads = region * smooth(0.92, 0.985, nz.ridge3(x * 14 + 2, y * 14, z * 14, 2)) * 0.16;
    return clamp(metroL + towns + rural + roads, 0, 1.4);
}
// Borrão separável (raio r) pra dar o brilho em volta das luzes.
std::vector<float> blurMap(const std::vector<float>& src, int W, int H, int r)
{
    std::vector<float> tmp(size_t(W) * size_t(H)), out(size_t(W) * size_t(H));
    const int n = 2 * r + 1;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        double s2 = 0;
        for (int k = -r; k <= r; k++) s2 += src[size_t(y * W + ((x + k + W) % W))];
        tmp[size_t(y * W + x)] = float(s2 / n);
    }
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        double s2 = 0;
        for (int k = -r; k <= r; k++) s2 += tmp[size_t(std::min(H - 1, std::max(0, y + k)) * W + x)];
        out[size_t(y * W + x)] = float(s2 / n);
    }
    return out;
}

double shade(const Body& b, const Noise& nz, double x, double y, double z, double lat, double lon, double* col, Ex& ex, RockOut& ro)
{
    const Look& L = b.look;
    const Type t = b.type;
    if (isArtificial(t)) return shadeArtificial(b, nz, x, y, z, lat, lon, col, ex);
    if (t == Type::Gigante || t == Type::Quente || t == Type::GeloG || t == Type::Subnetuno) {
        const double turb = nz.fbm3(x * 2.2, y * 2.2, z * 5, 4);
        const double bb = z * L.bandsF + turb * L.turb + nz.fbm3(x * 0.8 + 5, y * 0.8, z * 3, 2) * 0.6;
        ramp(L.pal, 0.5 + 0.5 * std::sin(bb * PI_D), col);
        const double fine = 1 + 0.06 * nz.n3(x * 1.5, y * 1.5, z * 40);
        col[0] *= fine; col[1] *= fine; col[2] *= fine;
        for (const Storm& st : L.storms) {
            double dl = lon - st.lon;
            dl -= TAU * std::floor(dl / TAU + 0.5);
            const double e = std::pow((dl * std::cos(lat)) / st.w, 2) + std::pow((lat - st.lat) / st.h, 2);
            if (e > 1.6) continue;
            const double m = 1 - smooth(0.55, 1.0, e + nz.n3(x * 8, y * 8, z * 8) * 0.25);
            if (m > 0) { const double sw = 0.85 + 0.15 * std::sin(e * 12); mix3(col, st.c[0] * sw, st.c[1] * sw, st.c[2] * sw, m * 0.85); }
        }
        return t == Type::Quente ? 0.35 * smooth(0.3, 0.7, 0.5 + turb) : 0;
    }
    if (isLiving(t)) {
        const double h = livingHeight(nz, L, x, y, z);
        const double al = std::fabs(lat);
        double cold = 0, hot = 0, cloudK = 1;
        if (L.hasSub) {
            // Planeta-olho: o lado da estrela torra, o lado de trás congela, a vida mora no anel do meio.
            const double cd = std::cos(lat) * std::cos(lon - L.sub) + nz.fbm3(x * 3, y * 3, z * 3, 3) * 0.15;
            cold = 1 - smooth(-0.3, 0.02, cd);
            hot = smooth(0.55, 0.92, cd);
            cloudK = 0.25 + 1.3 * smooth(0.15, 0.85, cd);
        }
        if (h < L.sea) {
            // Raso só bem junto da costa; no fundo, variação lenta de profundidade.
            const double dpt = std::pow(smooth(0, 0.1, L.sea - h + nz.fbm3(x * 9 + 4, y * 9, z * 9, 2) * 0.025), 0.55);
            const double deepV = 0.85 + 0.3 * (nz.fbm3(x * 3 + 8, y * 3, z * 3, 3) + 0.5);
            col[0] = mix(L.shallow[0] * 0.7, L.deep[0] * deepV * 0.62, dpt); col[1] = mix(L.shallow[1] * 0.75, L.deep[1] * deepV * 0.62, dpt); col[2] = mix(L.shallow[2] * 0.8, L.deep[2] * deepV * 0.66, dpt);
            ex.spec = 1;
        } else {
            // Umidade em regiões grandes (biomas), com a borda levemente irregular; faixa seca nas latitudes médias.
            const double e = h - L.sea, moist = nz.fbm3(x * 1.1 + 9, y * 1.1, z * 1.1, 3) + nz.fbm3(x * 5 + 2, y * 5, z * 5, 2) * 0.12;
            const double beltK = L.belt * smooth(0.18, 0.32, al) * (1 - smooth(0.52, 0.66, al));
            const double dry = smooth(0.25, 0.75, 0.5 - moist * 1.8 + L.dryBias + beltK * 1.6 + hot * 0.7 - e * 0.4);
            col[0] = mix(L.veg[0], L.dry[0], dry); col[1] = mix(L.veg[1], L.dry[1], dry); col[2] = mix(L.veg[2], L.dry[2], dry);
            if (L.style == LookStyle::Pantano && e < 0.07) {
                const double m = smooth(0.0, 0.12, nz.fbm3(x * 9, y * 9, z * 9, 3)) * (1 - smooth(0.03, 0.07, e));
                tint3(col, L.murk, m);
            }
            if (L.style == LookStyle::Tundra) { const double m = smooth(0.35, 1.0, al) * 0.45; mix3(col, 0.42, 0.4, 0.33, m); }
            // Textura fina do chão (campos, matas, manchas de solo).
            const double dt = nz.fbm3(x * 22 + 1, y * 22, z * 22, 3);
            const double dk = 1 + dt * 0.07;
            col[0] *= dk; col[1] *= dk * (1 + dt * 0.05); col[2] *= dk;
            // Serras: cristas que sobem com o terreno.
            const double ridge = nz.ridge3(x * 5 + 11, y * 5, z * 5, 4);
            const double range = smooth(0.8, 0.96, ridge) * smooth(0.1, 0.3, e);
            if (range > 0) tint3(col, L.rock, range * 0.55);
            if (e > 0.42) { const double m = smooth(0.42, 0.58, e) * 0.8; tint3(col, L.rock, m); }
            const double peak = e + range * 0.05;
            if (peak > L.snowAt) { const double m = smooth(L.snowAt, L.snowAt + 0.08, peak); mix3(col, 0.93, 0.94, 0.96, m); }
            // Relevo: a encosta virada pra luz clareia, a de trás escurece (luz fixa, como num mapa em relevo).
            const double h2 = livingHeight(nz, L, x - y * 0.006, y + x * 0.006, z + 0.004) + range * 0.02;
            const double relief = clamp(1 + (h2 - h - range * 0.004) * 26, 0.86, 1.12);
            col[0] *= relief; col[1] *= relief; col[2] *= relief;
        }
        const double ice = std::max(cold, smooth(L.cap - 0.06, L.cap + 0.04, al + nz.fbm3(x * 3, y * 3, z * 3, 3) * 0.25));
        ex.spec = ex.spec * (1 - ice) + ice * 0.25;
        tint3(col, L.ice, ice);
        double cx = x, cy = y, cz = z;
        if (L.swirl) { const double q = nz.fbm3(x * 1.4 + 7, y * 1.4, z * 1.4, 3) * 1.4; cx += q; cy -= q * 0.6; cz += q * 0.4; }
        const double c = nz.fbm3(cx * 2.4 + 20, cy * 2.4, cz * 4.5, 6) + nz.fbm3(x * 7, y * 7, z * 9, 3) * 0.18;
        const double wisp = 0.72 + 0.28 * smooth(-0.22, 0.22, nz.fbm3(cx * 16, cy * 16, cz * 24, 3));
        const double ca = clamp(smooth(L.cloud - 0.03, L.cloud + 0.24, c) * 1.08 * cloudK * wisp, 0, 0.96);
        const double cv = 0.93 + 0.07 * smooth(0, 0.2, c - L.cloud);
        // Cidades: só em terra firme e fora do gelo; de dia, a mancha cinza das metrópoles.
        if (L.pop > 0 && h >= L.sea && ice < 0.6) {
            ex.cmask = (1 - ice) * (1 - ca * 0.5) * (h - L.sea < 0.04 ? 1.3 : 1);
            const double u = cityRegion(nz, x, y, z, L.pop) * 0.3 * L.pop * (1 - ca);
            mix3(col, 0.46, 0.45, 0.47, u);
        }
        mix3(col, L.cloudCol[0] * cv, L.cloudCol[1] * cv, L.cloudCol[2] * cv, ca);
        ex.spec *= 1 - ca;
        return 0;
    }
    if (t == Type::Deserto) {
        const double f = nz.fbm3(x * 1.8, y * 1.8, z * 1.8, 5), d = nz.ridge3(x * 7, y * 7, z * 7, 3);
        ramp(L.pal, 0.5 + f * 1.1 + (d - 0.5) * 0.25, col);
        if (L.canyon) { const double cn = smooth(0.82, 0.95, nz.ridge3(x * 2.5 + 4, y * 2.5, z * 2.5, 3)); col[0] *= 1 - cn * 0.45; col[1] *= 1 - cn * 0.5; col[2] *= 1 - cn * 0.5; }
        const double ice = smooth(L.cap - 0.05, L.cap + 0.04, std::fabs(lat) + f * 0.2);
        mix3(col, 0.92, 0.9, 0.9, ice);
        return 0;
    }
    if (L.hasRock && (t == Type::Arido || t == Type::Rochosa || t == Type::Anao)) return shadeRock(b, nz, x, y, z, lat, col, ex, ro);
    if (t == Type::Arido || t == Type::Rochosa || t == Type::Anao || t == Type::Cometa) {
        const double f = nz.fbm3(x * 2, y * 2, z * 2, 5), g = nz.fbm3(x * 7, y * 7, z * 7, 3);
        const double v = clamp(0.5 + f * 1.1 + g * 0.3, 0, 1);
        const V3 tint = t == Type::Cometa && L.tint[0] == 0 ? V3 { 0.8, 0.78, 0.75 } : L.tint;
        const double base = t == Type::Cometa ? 0.32 : 0.62;
        col[0] = tint[0] * mix(0.28, base + 0.12, v); col[1] = tint[1] * mix(0.27, base + 0.1, v); col[2] = tint[2] * mix(0.26, base + 0.08, v);
        if (t == Type::Anao) { const double m = smooth(0.1, 0.3, nz.fbm3(x * 1.2 + 4, y * 1.2, z * 1.2, 3)); mix3(col, 0.92, 0.88, 0.82, m * 0.6); }
        if (ex.hasCraters) applyCraters(col, x, y, z, ex.craters);
        return 0;
    }
    if (t == Type::Lava) {
        const double f = nz.fbm3(x * 2.5, y * 2.5, z * 2.5, 5);
        const double cr = nz.ridge3(x * 3, y * 3, z * 3, 4);
        const double pool = smooth(L.pool, L.pool + 0.18, nz.fbm3(x * 1.4 + 7, y * 1.4, z * 1.4, 4));
        const double e = std::max(smooth(L.crack, L.crack + 0.17, cr), pool);
        const double v = 0.12 + (f + 0.5) * 0.12;
        col[0] = mix(v, L.tint[0], e); col[1] = mix(v * 0.9, L.tint[1], e); col[2] = mix(v * 0.85, L.tint[2], e);
        return e;
    }
    if (t == Type::Gelado || t == Type::Gelada) {
        const double f = nz.fbm3(x * 2, y * 2, z * 2, 5);
        const double ln = nz.ridge3(x * 3.5, y * 3.5, z * 3.5, 3);
        const double v = 0.85 + f * 0.25;
        col[0] = L.tint[0] * v; col[1] = L.tint[1] * v; col[2] = L.tint[2] * v;
        const double m = smooth(0.8, 0.95, ln) * 0.7 + smooth(0.15, 0.4, nz.fbm3(x + 3, y, z, 3)) * 0.3;
        tint3(col, L.line, m);
        return 0;
    }
    if (t == Type::Toxico) {
        const double wv = nz.fbm3(x * 1.5, y * 1.5, z * 1.5, 4);
        ramp(L.pal, 0.5 + 0.5 * std::sin(z * 5 + wv * 5 + lon * 0.5), col);
        return 0;
    }
    if (t == Type::Vulcanica) {
        const double f = nz.fbm3(x * 2.2, y * 2.2, z * 2.2, 5);
        col[0] = L.tint[0] * (0.85 + f * 0.4); col[1] = L.tint[1] * (0.85 + f * 0.4); col[2] = L.tint[2] * (0.8 + f * 0.3);
        const double sp = smooth(0.28, 0.4, nz.fbm3(x * 4 + 2, y * 4, z * 4, 3));
        const double ring = smooth(0.18, 0.28, nz.fbm3(x * 4 + 2, y * 4, z * 4, 3)) - sp;
        mix3(col, 0.75, 0.35, 0.12, ring * 0.6);
        mix3(col, 0.12, 0.08, 0.06, sp);
        return sp * 0.8;
    }
    if (t == Type::Estrela) {
        const double g = nz.fbm3(x * 22, y * 22, z * 22, 2), big = nz.fbm3(x * 3, y * 3, z * 3, 3);
        const auto& c = b.star->color;
        double v = 0.94 + g * 0.16 + big * 0.06;
        if (b.star->kind == StarKind::Gigante) v = 0.82 + nz.fbm3(x * 4, y * 4, z * 4, 4) * 0.45;
        // Manchas pequenas, em duas faixas perto do equador, como no Sol.
        const double band = smooth(0.1, 0.2, std::fabs(lat)) * (1 - smooth(0.45, 0.6, std::fabs(lat)));
        const double spot = (b.star->kind == StarKind::G || b.star->kind == StarKind::K || b.star->kind == StarKind::M) ? smooth(0.4, 0.47, nz.fbm3(x * 10 + 9, y * 10, z * 10, 3)) * band : 0;
        v *= 1 - spot * 0.6;
        col[0] = (c[0] / 255.0) * v; col[1] = (c[1] / 255.0) * v; col[2] = (c[2] / 255.0) * v;
        return 1;
    }
    col[0] = col[1] = col[2] = 0.5;
    return 0;
}
inline bool isSolid(Type t)
{
    switch (t) { case Type::Lava: case Type::Arido: case Type::Deserto: case Type::Gelado: case Type::Toxico: case Type::Anao: case Type::Rochosa: case Type::Gelada: case Type::Vulcanica: return true; default: return false; }
}
const Atm ATM_TEMPERADO { { 0.45, 0.65, 1.0 }, 0.95 }, ATM_OCEANO { { 0.45, 0.68, 1.0 }, 1.0 }, ATM_TOXICO { { 1.0, 0.85, 0.55 }, 0.8 }, ATM_DESERTO { { 0.95, 0.65, 0.45 }, 0.3 },
    ATM_GIGANTE { { 0.95, 0.86, 0.72 }, 0.4 }, ATM_QUENTE { { 1.0, 0.5, 0.3 }, 0.5 }, ATM_GELO { { 0.55, 0.8, 1.0 }, 0.6 }, ATM_SUB { { 0.7, 0.8, 1.0 }, 0.65 }, ATM_GELADO { { 0.7, 0.85, 1.0 }, 0.2 }, ATM_LAVA { { 1.0, 0.45, 0.2 }, 0.25 };
} // namespace

const Atm* atmOf(const Body& b)
{
    if (b.body == BodyKind::Lua) return b.type == Type::Temperada ? &ATM_TEMPERADO : nullptr;
    if (b.atm.empty() || b.atm == "Nenhuma") return nullptr;
    switch (b.type) {
    case Type::Temperado: case Type::Ecumeno: return &ATM_TEMPERADO; case Type::Oceano: return &ATM_OCEANO; case Type::Toxico: return &ATM_TOXICO; case Type::Deserto: return &ATM_DESERTO;
    case Type::Gigante: return &ATM_GIGANTE; case Type::Quente: return &ATM_QUENTE; case Type::GeloG: return &ATM_GELO; case Type::Subnetuno: return &ATM_SUB; case Type::Gelado: return &ATM_GELADO; case Type::Lava: return &ATM_LAVA;
    default: return nullptr;
    }
}

Texture bodyTexture(const Body& b, int w, int h)
{
    const Noise nz(b.look.seed);
    Texture tx;
    tx.w = w; tx.h = h;
    const size_t n = size_t(w) * size_t(h);
    tx.rgb.resize(n * 3); tx.emi.resize(n);
    const double pop = b.look.pop;
    const bool solidColony = pop > 0 && isSolid(b.type) && b.type != Type::Toxico;
    std::vector<float> mask;
    if (pop > 0) mask.assign(n, 0.f);
    const bool living = b.look.hasFreq;
    if (living) tx.spec.resize(n);
    Ex ex;
    RockOut ro;
    if (b.look.hasRock) { ex.rockCr = rockCraters(b.look); ex.hasRockCr = true; }
    else if (b.look.craters != 0 || b.type == Type::Rochosa || b.type == Type::Cometa) { ex.craters = craterList(b.look.seed, b.look.craters != 0 ? b.look.craters : 30); ex.hasCraters = true; }
    double col[3] = { 0, 0, 0 };
    for (int j = 0; j < h; j++) {
        const double lat = (0.5 - (j + 0.5) / h) * PI_D, cl = std::cos(lat), sl = std::sin(lat);
        for (int i = 0; i < w; i++) {
            const double lon = ((i + 0.5) / w) * TAU - PI_D;
            ex.cmask = 0; ex.spec = 0;
            const double x = cl * std::cos(lon), y = cl * std::sin(lon);
            const double e = shade(b, nz, x, y, sl, lat, lon, col, ex, ro);
            if (solidColony) {
                // Colônias em mundos sem vida: cúpulas e bases; de dia, manchas cinza.
                ex.cmask = 0.85;
                const double u = cityRegion(nz, x, y, sl, pop * 0.85) * 0.25 * pop;
                mix3(col, 0.5, 0.5, 0.52, u);
            }
            const size_t q = size_t(j * w + i);
            if (!mask.empty()) mask[q] = float(ex.cmask);
            if (living) tx.spec[q] = u8c(clamp(ex.spec, 0, 1) * 255);
            tx.rgb[q * 3] = u8c(col[0] * 255); tx.rgb[q * 3 + 1] = u8c(col[1] * 255); tx.rgb[q * 3 + 2] = u8c(col[2] * 255);
            tx.emi[q] = u8c(clamp(e, 0, 1) * 255);
        }
    }
    // Luzes das cidades no dobro da resolução, com brilho em volta (lidas suavizadas na esfera).
    if (!mask.empty()) {
        const int q = w >= 400 && w < 900 ? 2 : 1;
        const double vEff = solidColony ? pop * 0.85 : pop;
        const auto centers = cityCenters(b, nz, vEff);
        const int cw = w * q, ch = h * q;
        tx.cw = cw; tx.ch = ch;
        std::vector<float> raw(size_t(cw) * size_t(ch), 0.f);
        for (int j = 0; j < ch; j++) {
            const double lat = (0.5 - (j + 0.5) / ch) * PI_D, cl = std::cos(lat), sl = std::sin(lat);
            const double gj = clamp((j + 0.5) / q - 0.5, 0, h - 1.001);
            const int j0 = int(std::floor(gj));
            const double fj = gj - j0;
            for (int i = 0; i < cw; i++) {
                const double gi = (i + 0.5) / q - 0.5, gf = std::floor(gi);
                const int i0 = (int(gf) + w) % w, i1 = (i0 + 1) % w;
                const double fi = gi - gf;
                const double m = (double(mask[size_t(j0 * w + i0)]) * (1 - fi) + double(mask[size_t(j0 * w + i1)]) * fi) * (1 - fj) + (double(mask[size_t((j0 + 1) * w + i0)]) * (1 - fi) + double(mask[size_t((j0 + 1) * w + i1)]) * fi) * fj;
                if (m <= 0.01) continue;
                const double lon = ((i + 0.5) / cw) * TAU - PI_D;
                raw[size_t(j * cw + i)] = float(cityLight(nz, cl * std::cos(lon), cl * std::sin(lon), sl, vEff, centers) * std::min(1.0, m));
            }
        }
        const auto soft = blurMap(raw, cw, ch, 2 * q);
        const auto halo = blurMap(raw, cw, ch, 4 * q);
        tx.city.resize(raw.size());
        for (size_t k = 0; k < raw.size(); k++) tx.city[k] = u8c(clamp(double(raw[k]) * 0.85 + double(soft[k]) * 0.4 + double(halo[k]) * 0.18, 0, 1) * 255);
    }
    return tx;
}
Texture starTexture(const Star& star, uint32_t seed, int w, int h)
{
    Body b;
    b.type = Type::Estrela; b.body = BodyKind::Estrela; b.star = std::make_unique<Star>(star); b.look.seed = seed;
    return bodyTexture(b, w, h);
}

// Tabela por pixel do disco: normal na câmera e (lat, lon) no corpo, sem o giro do dia.
SphereTable sphereTable(int D, double yaw, double pitch, const Frame& fr)
{
    SphereTable T;
    T.D = D;
    const size_t n = size_t(D) * size_t(D);
    T.lat.assign(n, 0.f); T.lon.assign(n, 0.f); T.alpha.assign(n, 0.f); T.nx.assign(n, 0.f); T.ny.assign(n, 0.f); T.nz.assign(n, 0.f);
    const double R = D / 2.0;
    const double cy = std::cos(yaw), sy = std::sin(yaw), sp = std::sin(pitch), cp = std::cos(pitch);
    for (int j = 0; j < D; j++) {
        for (int i = 0; i < D; i++) {
            const double x = (i + 0.5 - R) / R, yu = (R - (j + 0.5)) / R, d2 = x * x + yu * yu;
            const size_t k = size_t(j * D + i);
            const double a = clamp((1 - std::sqrt(d2)) * R + 0.5, 0, 1);
            if (a <= 0) continue;
            const double z = std::sqrt(std::max(0.0, 1 - d2));
            T.alpha[k] = float(a); T.nx[k] = float(x); T.ny[k] = float(yu); T.nz[k] = float(z);
            const double yr = yu * sp - z * cp, wz = yu * cp + z * sp;
            const double wx = x * cy + yr * sy, wy = -x * sy + yr * cy;
            const double bx = wx * fr.e1[0] + wy * fr.e1[1] + wz * fr.e1[2];
            const double by = wx * fr.e2[0] + wy * fr.e2[1] + wz * fr.e2[2];
            const double bz = wx * fr.ax[0] + wy * fr.ax[1] + wz * fr.ax[2];
            T.lat[k] = float(std::asin(clamp(bz, -1, 1))); T.lon[k] = float(std::atan2(by, bx));
        }
    }
    return T;
}

// Pinta a esfera em `out` (RGBA D×D). Lc = direção da luz na câmera; nullptr = o corpo brilha sozinho.
void shadeSphere(uint8_t* out, const SphereTable& T, const Texture& tex, double spin, const V3* Lc, const Atm* atm)
{
    const size_t n = size_t(T.D) * size_t(T.D);
    const int tw = tex.w, th = tex.h;
    const uint8_t* rgb = tex.rgb.data();
    for (size_t k = 0; k < n; k++) {
        const double a = T.alpha[k];
        const size_t o = k * 4;
        if (a <= 0) { out[o + 3] = 0; continue; }
        // Leitura bilinear: a textura dá a volta em longitude e para nos polos.
        double u = (double(T.lon[k]) + spin) / TAU + 0.5;
        u -= std::floor(u);
        const double fu = u * tw - 0.5, fv = clamp((0.5 - double(T.lat[k]) / PI_D) * th - 0.5, 0, th - 1.001);
        const int i0 = int(std::floor(fu)), j0 = int(std::floor(fv));
        const double du = fu - i0, dv = fv - j0;
        const int ia = (i0 + tw) % tw, ib = (i0 + 1) % tw;
        const size_t q00 = size_t(j0 * tw + ia) * 3, q10 = size_t(j0 * tw + ib) * 3, q01 = size_t((j0 + 1) * tw + ia) * 3, q11 = size_t((j0 + 1) * tw + ib) * 3;
        const double w00 = (1 - du) * (1 - dv), w10 = du * (1 - dv), w01 = (1 - du) * dv, w11 = du * dv;
        double r = rgb[q00] * w00 + rgb[q10] * w10 + rgb[q01] * w01 + rgb[q11] * w11;
        double g = rgb[q00 + 1] * w00 + rgb[q10 + 1] * w10 + rgb[q01 + 1] * w01 + rgb[q11 + 1] * w11;
        double b = rgb[q00 + 2] * w00 + rgb[q10 + 2] * w10 + rgb[q01 + 2] * w01 + rgb[q11 + 2] * w11;
        const size_t idx = (du < 0.5 ? (dv < 0.5 ? q00 : q01) : (dv < 0.5 ? q10 : q11)) / 3;
        const double nx = T.nx[k], ny = T.ny[k], nz = T.nz[k];
        if (Lc) {
            const V3& L = *Lc;
            const double dl = nx * L[0] + ny * L[1] + nz * L[2];
            // Crepúsculo largo: a luz cai devagar até a noite, sem degrau no terminador.
            const double day = smooth(-0.3, 0.35, dl);
            const double lit = 0.025 + 0.975 * std::pow(smooth(-0.3, 1.0, dl), 1.15);
            const double em = (tex.emi[idx] / 255.0) * (1 - day * 0.65) * 1.3;
            r = r * (lit + em); g = g * (lit + em * 0.8); b = b * (lit + em * 0.6);
            if (!tex.spec.empty()) {
                // Reflexo do sol na água: só um clarão fraco e espalhado.
                const double sm = tex.spec[idx] / 255.0;
                if (sm > 0 && dl > 0) {
                    double hx = L[0], hy = L[1], hz = L[2] + 1;
                    double hn = hypot3(hx, hy, hz);
                    if (hn == 0) hn = 1;
                    hx /= hn; hy /= hn; hz /= hn;
                    const double nh = std::max(0.0, nx * hx + ny * hy + nz * hz);
                    const double sp = std::pow(nh, 18) * 0.07 * sm * smooth(0, 0.25, dl) * 255;
                    r += sp; g += sp * 0.97; b += sp * 0.9;
                }
            }
            if (atm) {
                // Pôr do sol: uma faixa alaranjada fina onde o dia vira noite.
                const double dusk = smooth(-0.16, -0.02, dl) * (1 - smooth(-0.02, 0.12, dl)) * atm->k * 13;
                r += dusk; g += dusk * 0.45; b += dusk * 0.18;
            }
            if (!tex.city.empty()) {
                // Luz das cidades: só no lado da noite, amarelada, lida com interpolação.
                const int cw2 = tex.cw, ch2 = tex.ch;
                const double fu2 = u * cw2 - 0.5, fv2 = clamp((0.5 - double(T.lat[k]) / PI_D) * ch2 - 0.5, 0, ch2 - 1.001);
                const int a0 = int(std::floor(fu2)), b0 = int(std::floor(fv2));
                const double du2 = fu2 - a0, dv2 = fv2 - b0;
                const int ia2 = (a0 + cw2) % cw2, ib2 = (a0 + 1) % cw2;
                const uint8_t* cm = tex.city.data();
                const double c = ((cm[b0 * cw2 + ia2] * (1 - du2) + cm[b0 * cw2 + ib2] * du2) * (1 - dv2) + (cm[(b0 + 1) * cw2 + ia2] * (1 - du2) + cm[(b0 + 1) * cw2 + ib2] * du2) * dv2) / 255;
                if (c > 0) { const double k2 = std::min(1.0, c * 1.45) * (1 - smooth(-0.3, 0.15, dl)) * 255 * 1.15; r += k2; g += k2 * (0.64 + 0.28 * c); b += k2 * (0.28 + 0.36 * c); }
            }
            if (atm) {
                // Atmosfera: perto da borda o chão some numa névoa azul (mistura, não reflexo), só no lado do dia.
                const double lim = std::pow(1 - nz, 2.2), dayK = smooth(-0.2, 0.4, dl);
                const double haze = lim * atm->k * 0.55 * dayK;
                const double hk = 0.45 + 0.55 * std::max(0.0, dl);
                r = mix(r, atm->c[0] * 255 * hk, haze);
                g = mix(g, atm->c[1] * 255 * hk, haze);
                b = mix(b, atm->c[2] * 255 * hk, haze);
                const double edge = std::pow(1 - nz, 6) * atm->k * dayK * 70;
                r += atm->c[0] * edge; g += atm->c[1] * edge; b += atm->c[2] * edge;
            }
        } else {
            const double ld = 0.42 + 0.58 * std::pow(nz, 0.55);
            r *= ld * 1.1; g *= ld * 1.1; b *= ld * 1.1;
        }
        out[o] = u8c(r); out[o + 1] = u8c(g); out[o + 2] = u8c(b); out[o + 3] = u8c(a * 255);
    }
}

} // namespace galaxy
