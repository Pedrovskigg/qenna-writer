#include "CartoCore.h"
#include "CartoProf.h"
#include "CartoNameLang.h"

#include <QCoreApplication>
#include <QRegularExpression>

#include <algorithm>
#include <limits>

namespace carto {

// ── JS ──
int32_t toInt32(double v)
{
    if (!std::isfinite(v)) return 0;
    double t = std::trunc(v);
    double m = std::fmod(t, 4294967296.0);
    if (m < 0) m += 4294967296.0;
    return static_cast<int32_t>(static_cast<uint32_t>(m));
}
uint32_t toUint32(double v) { return static_cast<uint32_t>(toInt32(v)); }

static inline uint32_t imul(uint32_t a, uint32_t b) { return a * b; }

double hash2(double x, double y, double s)
{
    uint32_t n = imul(static_cast<uint32_t>(toInt32(x)), 374761393u) ^ imul(static_cast<uint32_t>(toInt32(y)), 668265263u)
               ^ imul(static_cast<uint32_t>(toInt32(s)), 2147483647u);
    n = imul(n ^ (n >> 13), 1274126177u);
    return (n ^ (n >> 16)) / 4294967296.0;
}
double vnoise(double x, double y, double s)
{
    const double ix = std::floor(x), iy = std::floor(y);
    const double fx = x - ix, fy = y - iy;
    const double u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    const double a = hash2(ix, iy, s), b = hash2(ix + 1, iy, s), c = hash2(ix, iy + 1, s), d = hash2(ix + 1, iy + 1, s);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
double fbm(double x, double y, int oct, double s)
{
    double sum = 0, amp = 0.5, norm = 0, f = 1;
    for (int o = 0; o < oct; o++) { sum += vnoise(x * f, y * f, s + o * 101) * amp; norm += amp; amp *= 0.5; f *= 2; }
    return sum / norm;
}

// ── presets ──
const SizePreset kSizes[6] = {
    { "island", 120, 90 }, { "region", 200, 150 }, { "world", 280, 210 },
    { "epic", 360, 270 }, { "continent", 500, 375 }, { "super", 640, 480 },
};
const SizePreset& sizePreset(int size) { return (size >= 0 && size < 6) ? kSizes[size] : kSizes[1]; }

// ── falhas tectônicas ──
// Cada falha sobe um lado +1 e desce o outro −1; em cada linha acha o x onde
// a falha corta e soma por intervalo (array de diferenças): O(H) por falha.
std::vector<float> faultMap(int W, int H, int numFaults, double seed)
{
    SeededRand rand(seed);
    const int S = W + 1;
    std::vector<float> D(static_cast<size_t>(S) * H, 0.0f);
    double off = 0;
    for (int f = 0; f < numFaults; f++) {
        const double cx = rand.next() * W, cy = rand.next() * H;
        const double ang = rand.next() * M_PI * 2;
        const double nx = std::sin(ang), ny = -std::cos(ang);
        const double sign = rand.next() > 0.5 ? 1 : -1;
        off -= sign;
        const double v = 2 * sign;
        for (int y = 0; y < H; y++) {
            const double c = cx * nx - (y - cy) * ny;
            double a, b;
            if (nx > 1e-9)       { a = std::ceil(c / nx); b = W; }
            else if (nx < -1e-9) { a = 0; b = std::floor(c / nx) + 1; }
            else if ((y - cy) * ny >= 0) { a = 0; b = W; }
            else continue;
            if (a < 0) a = 0;
            if (b > W) b = W;
            if (a >= b) continue;
            D[static_cast<size_t>(y) * S + static_cast<int>(a)] += static_cast<float>(v);
            D[static_cast<size_t>(y) * S + static_cast<int>(b)] -= static_cast<float>(v);
        }
    }
    std::vector<float> out(static_cast<size_t>(W) * H);
    for (int y = 0; y < H; y++) {
        double acc = off;
        for (int x = 0; x < W; x++) { acc += D[static_cast<size_t>(y) * S + x]; out[static_cast<size_t>(y) * W + x] = static_cast<float>(acc); }
    }
    return out;
}

double computeThreshold(const std::vector<float>& map, double pct)
{
    std::vector<float> s(map);
    const size_t idx = std::min(static_cast<size_t>(std::floor(pct * s.size())), s.size() - 1);
    std::nth_element(s.begin(), s.begin() + idx, s.end());
    return s[idx];
}

static std::vector<float> islandFalloff(const std::vector<float>& map, int W, int H)
{
    double maxAbs = 0;
    for (float v : map) maxAbs = std::max(maxAbs, static_cast<double>(std::fabs(v)));
    const double penalty = maxAbs * 3;
    std::vector<float> out(map);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const double nx = (static_cast<double>(x) / (W - 1)) * 2 - 1, ny = (static_cast<double>(y) / (H - 1)) * 2 - 1;
            const double d = std::sqrt(nx * nx + ny * ny) / M_SQRT2;
            const size_t i = static_cast<size_t>(y) * W + x;
            out[i] = static_cast<float>(out[i] - smoothstep(0.55, 0.88, d) * penalty);
        }
    return out;
}

// upsample bicúbico Catmull-Rom (separável)
struct Taps { std::vector<int> idx; std::vector<float> w; };
static Taps crTaps(int n, int s, int len)
{
    Taps t; t.idx.resize(static_cast<size_t>(n) * 4); t.w.resize(static_cast<size_t>(n) * 4);
    for (int x = 0; x < n; x++) {
        const double g = static_cast<double>(x) / s, i = std::floor(g), tt = g - i;
        const double t2 = tt * tt, t3 = t2 * tt;
        t.w[x * 4]     = static_cast<float>(0.5 * (-tt + 2 * t2 - t3));
        t.w[x * 4 + 1] = static_cast<float>(0.5 * (2 - 5 * t2 + 3 * t3));
        t.w[x * 4 + 2] = static_cast<float>(0.5 * (tt + 4 * t2 - 3 * t3));
        t.w[x * 4 + 3] = static_cast<float>(0.5 * (-t2 + t3));
        for (int k = 0; k < 4; k++) t.idx[x * 4 + k] = static_cast<int>(clampd(i - 1 + k, 0, len - 1));
    }
    return t;
}
Upsampled upsample(const std::vector<float>& map, int W, int H, int s)
{
    const int nW = (W - 1) * s + 1, nH = (H - 1) * s + 1;
    const Taps tx = crTaps(nW, s, W), ty = crTaps(nH, s, H);
    std::vector<float> tmp(static_cast<size_t>(nW) * H);
    for (int y = 0; y < H; y++) {
        const size_t row = static_cast<size_t>(y) * W;
        for (int x = 0; x < nW; x++) {
            const int k = x * 4;
            tmp[static_cast<size_t>(y) * nW + x] = static_cast<float>(
                static_cast<double>(tx.w[k]) * map[row + tx.idx[k]] + static_cast<double>(tx.w[k + 1]) * map[row + tx.idx[k + 1]] +
                static_cast<double>(tx.w[k + 2]) * map[row + tx.idx[k + 2]] + static_cast<double>(tx.w[k + 3]) * map[row + tx.idx[k + 3]]);
        }
    }
    Upsampled u; u.W = nW; u.H = nH; u.map.resize(static_cast<size_t>(nW) * nH);
    for (int y = 0; y < nH; y++) {
        const int k = y * 4;
        const size_t r0 = static_cast<size_t>(ty.idx[k]) * nW, r1 = static_cast<size_t>(ty.idx[k + 1]) * nW,
                     r2 = static_cast<size_t>(ty.idx[k + 2]) * nW, r3 = static_cast<size_t>(ty.idx[k + 3]) * nW;
        const double w0 = ty.w[k], w1 = ty.w[k + 1], w2 = ty.w[k + 2], w3 = ty.w[k + 3];
        for (int x = 0; x < nW; x++)
            u.map[static_cast<size_t>(y) * nW + x] = static_cast<float>(w0 * tmp[r0 + x] + w1 * tmp[r1 + x] + w2 * tmp[r2 + x] + w3 * tmp[r3 + x]);
    }
    return u;
}

Upsampled islandRelief(int gw, int gh, int faults, double seed, double pct)
{
    std::vector<float> raw = faultMap(gw, gh, faults, seed);
    const double thr = computeThreshold(raw, pct);
    for (float& v : raw) v = static_cast<float>(v - thr);
    return upsample(islandFalloff(raw, gw, gh), gw, gh, 5);
}

static std::vector<float> moistureField(int gw, int gh, double seed)
{
    std::vector<float> mo = faultMap(gw, gh, 350, toUint32(seed + 77777));
    double lo = std::numeric_limits<double>::infinity(), hi = -lo;
    for (float v : mo) { if (v < lo) lo = v; if (v > hi) hi = v; }
    const double rg = (hi - lo) != 0 ? hi - lo : 1;
    for (float& v : mo) v = static_cast<float>((v - lo) / rg);
    std::vector<float> up = upsample(mo, gw, gh, 5).map;
    for (float& v : up) v = static_cast<float>(clampd(v, 0, 1));
    return up;
}

// ── clima ──
struct Preset { QString preset; Planet planet; double tb, tAmp, mb, mg; };
static Preset climatePreset(int seed, int size, QString preset)
{
    if (preset == QLatin1String("random") || preset == QLatin1String("temperate") || preset.isEmpty()) preset = QStringLiteral("mixed");
    else if (preset == QLatin1String("polar")) preset = QStringLiteral("cold");
    else if (preset == QLatin1String("tropical")) preset = QStringLiteral("green");
    const double r1 = hash2(seed, 999, 17), r2 = hash2(seed, 4242, 23), r3 = hash2(seed, 77, 5), r4 = hash2(seed, 31, 9);
    const bool whole = size >= 2;
    const double hemi = r2 < 0.5 ? -1 : 1;
    double span = whole ? 196 : size == 0 ? 46 : 78;
    double lat0, tb, tAmp, mb, mg;
    if (preset == QLatin1String("cold")) {
        lat0 = whole ? 0 : 58 + r1 * 20; tb = whole ? 0.26 : 0.10 + 0.08 * r3; tAmp = 0.06; mb = -0.02; mg = 1.0;
        if (whole) span = 290;
    } else if (preset == QLatin1String("arid")) {
        lat0 = whole ? 0 : 14 + r1 * 22; tb = -0.06; tAmp = 0.05; mb = -0.30; mg = 1.25;
    } else if (preset == QLatin1String("green")) {
        lat0 = whole ? 0 : 4 + r1 * 44; tb = -0.02; tAmp = 0.05; mb = 0.19; mg = 1.0;
    } else {
        lat0 = whole ? (r1 - 0.5) * 44 : 2 + r1 * 76; tb = -0.06 + 0.24 * r3; tAmp = 0.10 + 0.08 * r4;
        mb = (r4 - 0.5) * 0.16; mg = 1.35; preset = QStringLiteral("mixed");
    }
    Preset p; p.preset = preset; p.planet.lat0 = whole ? lat0 : lat0 * hemi; p.planet.span = span; p.planet.set = true;
    p.tb = tb; p.tAmp = tAmp; p.mb = mb; p.mg = mg;
    return p;
}
static void bakeClimate(World& w, int gw, int gh, const Preset& pre)
{
    for (float& v : w.m) v = static_cast<float>(clampd(0.5 + (v - 0.5) * pre.mg + pre.mb, 0, 1));
    std::vector<float> raw = faultMap(gw, gh, 250, toUint32(static_cast<double>(w.seed) + 8888));
    const double med = computeThreshold(raw, 0.5);
    double mx = 0;
    for (float& v : raw) { v = static_cast<float>(v - med); mx = std::max(mx, static_cast<double>(std::fabs(v))); }
    for (float& v : raw) v = static_cast<float>(v / (mx != 0 ? mx : 1));
    const std::vector<float> up = upsample(raw, gw, gh, 5).map;
    for (size_t i = 0; i < w.k.size(); i++) {
        const double u = (i < up.size() && up[i] == up[i]) ? up[i] : 0.0;
        w.k[i] = static_cast<float>(pre.tb + pre.tAmp * u);
    }
    w.climate = pre.preset;
    w.planet = pre.planet;
}
double latitudeAt(const World& w, double qy)
{
    if (!w.planet.set) return 0;
    return std::max(-90.0, std::min(90.0, w.planet.lat0 - (qy / w.H - 0.5) * w.planet.span));
}
double latCold(const World& w, double qy, double wave)
{
    if (!w.planet.set) return std::fabs(qy / w.H - 0.5) * 2 * 0.52;
    const double phi = std::fabs(latitudeAt(w, qy)) + 14 * wave;
    return 0.25 * clampd(phi / 90, 0, 1) + 0.70 * smoothstep(35, 75, phi);
}
int biomeId(double t, double latC, double mois, double cold)
{
    const double temp = 1.0 - std::min(1.0, t) * 0.72 - latC - cold;
    if (temp < 0.18) return 0;
    if (temp < 0.38) return mois > 0.52 ? 1 : 2;
    if (temp < 0.65) return mois < 0.28 ? 3 : mois < 0.65 ? 2 : 1;
    if (mois < 0.30) return 3;
    if (mois < 0.52) return t < 0.10 ? 4 : 2;
    return 1;
}
ClimateSample climateAt(World& w, double x, double y)
{
    ClimateSample c;
    const int W = w.W, H = w.H;
    if (x < 0 || y < 0 || x > W - 1 || y > H - 1) return c;
    const std::vector<float>& T = ensureTempWave(w);
    const int ix = std::min(W - 2, toInt32(x)), iy = std::min(H - 2, toInt32(y));
    const double fx = x - ix, fy = y - iy;
    const size_t j = static_cast<size_t>(iy) * W + ix;
    auto bl = [&](const std::vector<float>& a) {
        return a[j] * (1 - fx) * (1 - fy) + a[j + 1] * fx * (1 - fy) + a[j + W] * (1 - fx) * fy + a[j + W + 1] * fx * fy;
    };
    c.h = bl(w.h); c.m = bl(w.m);
    const double k = bl(w.k), tv = bl(T);
    c.t = c.h / (w.ref != 0 ? w.ref : 1);
    const double lc = latCold(w, y, tv / 0.09), cold = k + tv * 0.4;
    c.id = biomeId(c.t, lc, c.m, cold);
    c.temp = 1 - std::min(1.0, c.t) * 0.72 - lc - cold;
    c.valid = true;
    return c;
}

// ── mundo ──
static double waterByType(const QString& type)
{
    if (type == QLatin1String("archipelago")) return 0.65;
    if (type == QLatin1String("oceanic")) return 0.78;
    return 0.45;
}
World generate(int seed, int water, const QString& type, int size, const QString& climate)
{
    CARTO_PROF("generate");
    const double pct = (water / 100.0 + waterByType(type)) / 2;
    const int faults = type == QLatin1String("archipelago") ? 3500 : 3000;
    const SizePreset& sp = sizePreset(size);
    Upsampled up = islandRelief(sp.gw, sp.gh, faults, seed, pct);
    World w;
    w.W = up.W; w.H = up.H; w.h = std::move(up.map);
    w.m = moistureField(sp.gw, sp.gh, seed);
    w.k.assign(w.h.size(), 0.0f);
    w.seed = seed; w.water = water; w.type = type; w.size = size;
    bakeClimate(w, sp.gw, sp.gh, climatePreset(seed, size, climate));
    cleanup(w);
    updateRef(w);
    return w;
}
World emptyWorld(int seed, int size, const QString& climate)
{
    CARTO_PROF("emptyWorld");
    const double EMPTY_REF = 80;
    const SizePreset& sp = sizePreset(size);
    World w;
    w.W = (sp.gw - 1) * 5 + 1; w.H = (sp.gh - 1) * 5 + 1;
    w.h.assign(static_cast<size_t>(w.W) * w.H, static_cast<float>(-EMPTY_REF * 0.3));
    w.m = moistureField(sp.gw, sp.gh, seed);
    w.k.assign(w.h.size(), 0.0f);
    w.seed = seed; w.water = 100; w.type = QStringLiteral("empty"); w.size = size; w.ref = EMPTY_REF;
    bakeClimate(w, sp.gw, sp.gh, climatePreset(seed, size, climate));
    return w;
}
void updateRef(World& w)
{
    double mx = 0;
    for (float v : w.h) if (v > mx) mx = v;
    w.ref = mx;
}
// Some com ilhotas e laguinhos minúsculos; laguinho vira terra na altura média da margem.
void cleanup(World& w)
{
    const int W = w.W, H = w.H, N = W * H;
    std::vector<float>& h = w.h;
    const double minA = jsRound(160 * (W / 1000.0) * (W / 1000.0));
    std::vector<uint8_t> lab(N, 0);
    std::vector<int> stack(N);
    std::vector<int> cells;
    auto nbOf = [&](int i, int out[4]) {
        const int x = i % W, y = i / W;
        out[0] = x > 0 ? i - 1 : -1; out[1] = x < W - 1 ? i + 1 : -1; out[2] = y > 0 ? i - W : -1; out[3] = y < H - 1 ? i + W : -1;
    };
    for (int s0 = 0; s0 < N; s0++) {
        if (lab[s0]) continue;
        const bool land = h[s0] > 0;
        int sp = 0; double area = 0; bool border = false; double shoreSum = 0; int shoreN = 0;
        stack[sp++] = s0; lab[s0] = 1;
        cells.clear();
        int nb[4];
        while (sp > 0) {
            const int i = stack[--sp];
            cells.push_back(i); area++;
            const int x = i % W, y = i / W;
            if (x == 0 || y == 0 || x == W - 1 || y == H - 1) border = true;
            nbOf(i, nb);
            for (int q = 0; q < 4; q++) {
                const int j = nb[q];
                if (j < 0) continue;
                if ((h[j] > 0) != land) { if (!land) { shoreSum += h[j]; shoreN++; } continue; }
                if (lab[j]) continue;
                lab[j] = 1; stack[sp++] = j;
            }
            if (area > minA) break;
        }
        if (area > minA) {
            while (sp > 0) {
                const int i = stack[--sp];
                nbOf(i, nb);
                for (int q = 0; q < 4; q++) { const int j = nb[q]; if (j >= 0 && !lab[j] && (h[j] > 0) == land) { lab[j] = 1; stack[sp++] = j; } }
            }
            continue;
        }
        if (land) for (int i : cells) h[i] = -0.5f;
        else if (!border) {
            const double fill = shoreN ? std::max(0.5, shoreSum / shoreN) : 0.5;
            for (int i : cells) h[i] = static_cast<float>(fill);
        }
    }
}
const std::vector<float>& ensureTempWave(World& w)
{
    if (w.T.size() == static_cast<size_t>(w.W) * w.H && !w.T.empty()) return w.T;
    const SizePreset& sp = sizePreset(w.size);
    std::vector<float> raw = faultMap(sp.gw, sp.gh, 300, toUint32(static_cast<double>(w.seed) + 5555));
    double mx = 0;
    const double med = computeThreshold(raw, 0.5);
    for (float& v : raw) { v = static_cast<float>(v - med); mx = std::max(mx, static_cast<double>(std::fabs(v))); }
    for (float& v : raw) v = static_cast<float>(v / (mx != 0 ? mx : 1) * 0.09);
    Upsampled up = upsample(raw, sp.gw, sp.gh, 5);
    if (up.map.size() == static_cast<size_t>(w.W) * w.H) w.T = std::move(up.map);
    else w.T.assign(static_cast<size_t>(w.W) * w.H, 0.0f);
    return w.T;
}
const std::vector<float>& ensureDetail(World& w)
{
    if (w.F.size() == static_cast<size_t>(w.W) * w.H && !w.F.empty()) return w.F;
    const SizePreset& sp = sizePreset(w.size);
    std::vector<float> raw = faultMap(sp.gw, sp.gh, 3000, toUint32(static_cast<double>(w.seed) + 31337));
    const double med = computeThreshold(raw, 0.5);
    double mx = 0;
    for (float& v : raw) { v = static_cast<float>(v - med); mx = std::max(mx, static_cast<double>(std::fabs(v))); }
    for (float& v : raw) v = static_cast<float>(v / (mx * 0.7));
    Upsampled up = upsample(raw, sp.gw, sp.gh, 5);
    if (up.map.size() == static_cast<size_t>(w.W) * w.H) w.F = std::move(up.map);
    else w.F.assign(static_cast<size_t>(w.W) * w.H, 0.0f);
    return w.F;
}

// ── campos auxiliares ──
std::vector<float> distanceToLand(const std::vector<float>& h, int W, int H)
{
    std::vector<float> d(static_cast<size_t>(W) * H);
    for (size_t i = 0; i < d.size(); i++) d[i] = h[i] > 0 ? 0.0f : 1e9f;
    const double D2 = M_SQRT2;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        const size_t i = static_cast<size_t>(y) * W + x; double v = d[i]; if (v == 0) continue;
        if (x > 0) v = std::min(v, d[i - 1] + 1.0);
        if (y > 0) {
            v = std::min(v, d[i - W] + 1.0);
            if (x > 0) v = std::min(v, d[i - W - 1] + D2);
            if (x < W - 1) v = std::min(v, d[i - W + 1] + D2);
        }
        d[i] = static_cast<float>(v);
    }
    for (int y = H - 1; y >= 0; y--) for (int x = W - 1; x >= 0; x--) {
        const size_t i = static_cast<size_t>(y) * W + x; double v = d[i]; if (v == 0) continue;
        if (x < W - 1) v = std::min(v, d[i + 1] + 1.0);
        if (y < H - 1) {
            v = std::min(v, d[i + W] + 1.0);
            if (x < W - 1) v = std::min(v, d[i + W + 1] + D2);
            if (x > 0) v = std::min(v, d[i + W - 1] + D2);
        }
        d[i] = static_cast<float>(v);
    }
    return d;
}

// blur de caixa separável, ponderado (só mistura cor de terra com terra)
void boxBlurWeighted(const std::vector<std::vector<float>*>& ch, std::vector<float>& wt, int W, int H, int r)
{
    auto pass = [&](const std::vector<float>& src, std::vector<float>& dst, bool horiz) {
        const int len = horiz ? W : H, lines = horiz ? H : W;
        const int step = horiz ? 1 : W, lineStep = horiz ? W : 1;
        for (int L = 0; L < lines; L++) {
            const size_t base = static_cast<size_t>(L) * lineStep;
            double acc = 0;
            for (int k = -r; k <= r; k++) acc += src[base + static_cast<size_t>(clampd(k, 0, len - 1)) * step];
            for (int p = 0; p < len; p++) {
                dst[base + static_cast<size_t>(p) * step] = static_cast<float>(acc / (2 * r + 1));
                acc += static_cast<double>(src[base + static_cast<size_t>(clampd(p + r + 1, 0, len - 1)) * step])
                     - src[base + static_cast<size_t>(clampd(p - r, 0, len - 1)) * step];
            }
        }
    };
    std::vector<float> tmp(static_cast<size_t>(W) * H);
    for (std::vector<float>* a : ch) { pass(*a, tmp, true); pass(tmp, *a, false); }
    pass(wt, tmp, true); pass(tmp, wt, false);
    const size_t N = static_cast<size_t>(W) * H;
    for (size_t i = 0; i < N; i++) {
        const double w = wt[i];
        if (w > 1e-6) for (std::vector<float>* c : ch) (*c)[i] = static_cast<float>((*c)[i] / w);
    }
}

// ── azulejos de ruído ──
const NoiseTiles& noiseTiles()
{
    static NoiseTiles NZ = [] {
        NoiseTiles t;
        const int S = 256;
        auto vnw = [](double x, double y, double p, double seed) {
            const double ix = std::floor(x), iy = std::floor(y), fx = x - ix, fy = y - iy;
            const double u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
            auto wr = [p](double a) { return std::fmod(std::fmod(a, p) + p, p); };
            const double a = hash2(wr(ix), wr(iy), seed), b = hash2(wr(ix + 1), wr(iy), seed);
            const double c = hash2(wr(ix), wr(iy + 1), seed), d = hash2(wr(ix + 1), wr(iy + 1), seed);
            return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
        };
        auto build = [&](double p0, int oct, double seed) {
            std::vector<float> T(static_cast<size_t>(S) * S);
            double lo = std::numeric_limits<double>::infinity(), hi = -lo;
            for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) {
                double sum = 0, amp = 0.5, p = p0;
                for (int o = 0; o < oct; o++, p *= 2, amp *= 0.5) sum += vnw(static_cast<double>(x) / S * p, static_cast<double>(y) / S * p, p, seed + o * 31) * amp;
                T[static_cast<size_t>(y) * S + x] = static_cast<float>(sum); lo = std::min(lo, sum); hi = std::max(hi, sum);
            }
            const double rg = (hi - lo) != 0 ? hi - lo : 1;
            for (float& v : T) v = static_cast<float>((v - lo) / rg);
            return T;
        };
        t.S = S; t.A = build(8, 4, 1201); t.B = build(64, 3, 3407);
        return t;
    }();
    return NZ;
}
double tileAt(const std::vector<float>& T, double x, double y)
{
    const double x0 = std::floor(x), y0 = std::floor(y), fx = x - x0, fy = y - y0;
    const int ix = toInt32(x0) & 255, iy = toInt32(y0) & 255, x1 = (ix + 1) & 255;
    const int r0 = iy << 8, r1 = ((iy + 1) & 255) << 8;
    const double a = T[r0 + ix], b = T[r0 + x1], c = T[r1 + ix], d = T[r1 + x1];   // (em double, como no JS)
    return a + (b - a) * fx + (c - a + (d - c - b + a) * fx) * fy;
}

// ── paleta ──
const RGB kBiomeColors[5] = { { 222, 230, 240 }, { 92, 124, 64 }, { 158, 162, 102 }, { 198, 178, 130 }, { 76, 96, 56 } };
const RGB kRock = { 134, 128, 116 }, kRockHi = { 188, 184, 176 }, kSnow = { 232, 236, 244 }, kSand = { 194, 172, 122 };
const RGB kRich[5][3] = {
    { { 240, 243, 248 }, { 216, 226, 240 }, { 196, 204, 214 } },
    { { 62, 102, 54 }, { 88, 122, 60 }, { 128, 138, 72 } },
    { { 118, 150, 74 }, { 164, 166, 94 }, { 198, 180, 114 } },
    { { 220, 192, 138 }, { 200, 156, 102 }, { 228, 210, 166 } },
    { { 92, 116, 74 }, { 112, 128, 80 }, { 84, 112, 92 } },
};
const RGB kAtlas[5] = { { 244, 244, 238 }, { 152, 176, 122 }, { 204, 204, 156 }, { 230, 210, 164 }, { 166, 184, 146 } };
const RGB kAtlasRock = { 184, 172, 154 }, kAtlasRockHi = { 208, 200, 188 }, kAtlasSnow = { 248, 248, 244 };
const RGB kPaper = { 240, 231, 208 }, kInk = { 40, 33, 28 };
RGB richTone(int id, double u)
{
    const RGB* T = kRich[id];
    if (u < 0.5) {
        const double q = u * 2;
        return { T[0][0] + (T[1][0] - T[0][0]) * q, T[0][1] + (T[1][1] - T[0][1]) * q, T[0][2] + (T[1][2] - T[0][2]) * q };
    }
    const double q = (u - 0.5) * 2;
    return { T[1][0] + (T[2][0] - T[1][0]) * q, T[1][1] + (T[2][1] - T[1][1]) * q, T[1][2] + (T[2][2] - T[1][2]) * q };
}

// ── nomes ──
static const char* const kNameA[] = { "Val", "Mor", "Ast", "Cor", "Thal", "Dur", "Ven", "Os", "Tre", "Al", "Nor", "Gar", "Eld", "Um", "Bra", "Fen", "Lys", "Kar", "Ith",
    "Ro", "Vael", "Sar", "Mir", "Tor", "Ka", "El", "Bel", "Dra", "Is", "Quel", "Har", "Zan", "Or", "Cael", "Ser", "Ond", "Var", "Gal" };
static const char* const kNameB[] = { "a", "e", "i", "o", "an", "ar", "or", "en", "el", "u" };
static const char* const kNameC[] = { "dren", "vaun", "ter", "is", "mar", "wyn", "ath", "ul", "esh", "ond", "ra", "ys", "eth", "os", "ia", "ane", "orn", "ak", "iel",
    "um", "en", "oth", "ira", "ess", "al" };
QString nameRoot(const std::function<double(int)>& r)
{
    const int nA = sizeof(kNameA) / sizeof(kNameA[0]), nB = sizeof(kNameB) / sizeof(kNameB[0]), nC = sizeof(kNameC) / sizeof(kNameC[0]);
    const QString a = QString::fromLatin1(kNameA[static_cast<int>(std::floor(r(1) * nA))]);
    const QString c = QString::fromLatin1(kNameC[static_cast<int>(std::floor(r(3) * nC))]);
    QString b = r(2) < 0.3 ? QString::fromLatin1(kNameB[static_cast<int>(std::floor(r(4) * nB))]) : QString();
    static const QRegularExpression dbl(QStringLiteral("([aeiou])\\1"), QRegularExpression::CaseInsensitiveOption);
    if (dbl.match(a + b + c).hasMatch()) b.clear();                 // nada de "Rooal"
    QString s = a + b + c;
    s.replace(dbl, QStringLiteral("\\1"));
    return s.left(1) + s.mid(1).toLower();
}
QString mapTitle(const World& w)
{
    const double seed = static_cast<double>(w.seed) + 13;
    const QString root = nameRoot([seed](int k) { return hash2(seed, 4242, 600 + k); });
    return polishName(QCoreApplication::translate("carto", "Terras de %1").arg(root));
}

} // namespace carto
