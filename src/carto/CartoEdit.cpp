#include "CartoEdit.h"
#include "CartoProf.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <thread>

namespace carto {

std::array<double, 2> planeToWorld(const World& w, double ang, double px, double py)
{
    const double ca = std::cos(ang), sa = std::sin(ang);
    return { w.W / 2.0 + ca * px + sa * py, w.H / 2.0 - sa * px + ca * py };
}
std::array<double, 2> worldToPlane(const World& w, double ang, double qx, double qy)
{
    const double ca = std::cos(ang), sa = std::sin(ang);
    const double dx = qx - w.W / 2.0, dy = qy - w.H / 2.0;
    return { ca * dx - sa * dy, sa * dx + ca * dy };
}

double brushNoise(const World& w, double x, double y, double salt)
{
    const double s = static_cast<double>(w.seed) + salt;
    return 0.55 * fbm(x * 0.015, y * 0.015, 3, s) + 0.45 * fbm(x * 0.07, y * 0.07, 3, s + 57);
}
double riverOffset(const World& w, double len, double r)
{
    return (fbm(len * 0.011, 7.3, 3, static_cast<double>(w.seed) + 99) - 0.5) * 2 * r * 3.2;
}

// ── ruído das bordas, guardado ──
// brushNoise custa 6 ruídos por pixel e o pincel pedia 2 (mais 4 da Montanha) a cada
// carimbada: num pincel de raio 140 eram 185 ms por carimbo. Ele só depende da semente
// e da posição, então cada bloco de 64×64 é calculado uma vez e reaproveitado.
struct NoiseCache {
    static constexpr int B = 64;
    int W = 0, H = 0, bw = 0, bh = 0, seed = 0;
    std::vector<std::unique_ptr<float[]>> f[3];   // 0: borda (901) · 1: alvo (501) · 2: cristas da Montanha
};

namespace {
template <typename Fn>
void parallelRows(int y0, int y1, size_t work, Fn fn)
{
    const int rows = y1 - y0 + 1;
    int nt = work > 30000 ? static_cast<int>(std::min<unsigned>(std::max(1u, std::thread::hardware_concurrency()), 16u)) : 1;
    nt = std::max(1, std::min(nt, rows / 6));
    if (nt <= 1) { fn(y0, y1); return; }
    std::vector<std::thread> th;
    for (int t = 0; t < nt; t++) {
        const int a = y0 + rows * t / nt, b = y0 + rows * (t + 1) / nt - 1;
        if (a <= b) th.emplace_back(fn, a, b);
    }
    for (std::thread& t : th) t.join();
}

NoiseCache& noiseCache(World& w)
{
    if (!w.brushNoise || w.brushNoise->W != w.W || w.brushNoise->H != w.H || w.brushNoise->seed != w.seed) {
        auto c = std::make_shared<NoiseCache>();
        c->W = w.W; c->H = w.H; c->seed = w.seed;
        c->bw = (w.W + NoiseCache::B - 1) / NoiseCache::B; c->bh = (w.H + NoiseCache::B - 1) / NoiseCache::B;
        for (auto& f : c->f) f.resize(static_cast<size_t>(c->bw) * c->bh);
        w.brushNoise = c;
    }
    return *w.brushNoise;
}

// garante os blocos do campo `field` que cobrem a caixa (os que faltam, calculados em paralelo)
void ensureNoise(World& w, int field, int x0, int y0, int x1, int y1)
{
    NoiseCache& c = noiseCache(w);
    std::vector<int> todo;
    for (int by = y0 / NoiseCache::B; by <= y1 / NoiseCache::B; by++)
        for (int bx = x0 / NoiseCache::B; bx <= x1 / NoiseCache::B; bx++) {
            const int i = by * c.bw + bx;
            if (!c.f[field][i]) todo.push_back(i);
        }
    if (todo.empty()) return;
    const double seed = static_cast<double>(w.seed);
    // poucos blocos novos por carimbada: reparte por linha (bloco × 64 linhas), senão sobra núcleo parado
    std::vector<float*> dst(todo.size());
    for (size_t t = 0; t < todo.size(); t++) { c.f[field][todo[t]].reset(new float[NoiseCache::B * NoiseCache::B]); dst[t] = c.f[field][todo[t]].get(); }
    auto fill = [&](int a, int b) {
        for (int r = a; r <= b; r++) {
            const int t = r / NoiseCache::B, yy = r % NoiseCache::B;
            const int i = todo[t], bx = i % c.bw, by = i / c.bw;
            float* row = dst[t] + yy * NoiseCache::B;
            const int Y = by * NoiseCache::B + yy;
            for (int xx = 0; xx < NoiseCache::B; xx++) {
                const int X = bx * NoiseCache::B + xx;
                double v = 0;
                if (field == 0) v = brushNoise(w, X, Y, 901);
                else if (field == 1) v = brushNoise(w, X, Y, 501);
                else v = 1 - std::fabs(2 * fbm(X * 0.022, Y * 0.022, 4, seed + 733) - 1);
                row[xx] = static_cast<float>(v);
            }
        }
    };
    parallelRows(0, static_cast<int>(todo.size()) * NoiseCache::B - 1, todo.size() * 40000, fill);
}
inline float noiseAt(const NoiseCache& c, int field, int x, int y)
{
    return c.f[field][(y / NoiseCache::B) * c.bw + x / NoiseCache::B][(y % NoiseCache::B) * NoiseCache::B + x % NoiseCache::B];
}
}

std::optional<std::array<int, 4>> applyDab(World& world, Brush tool, double x, double y, const DabState& st)
{
    CARTO_PROF("applyDab");
    const int W = world.W, H = world.H;
    std::vector<float>& h = world.h; std::vector<float>& m = world.m; std::vector<float>& k = world.k;
    const double ref = world.ref != 0 ? world.ref : 1;
    const std::vector<float>& F = ensureDetail(world);
    const double r = st.r;
    double rr = r;
    if (tool == Brush::River) rr = r * (0.35 + 0.65 * std::min(1.0, st.riverLen / (r * 30)));
    const int x0 = std::max(0, static_cast<int>(std::floor(x - rr * 1.3))), x1 = std::min(W - 1, static_cast<int>(std::ceil(x + rr * 1.3)));
    const int y0 = std::max(0, static_cast<int>(std::floor(y - rr * 1.3))), y1 = std::min(H - 1, static_cast<int>(std::ceil(y + rr * 1.3)));
    if (x0 > x1 || y0 > y1) return std::nullopt;
    const double rate = 0.05 + 0.25 * st.strength;
    // o inverso de cada pincel (Shift)
    enum class T { Land, Sea, Mountain, Erode, Smooth, River, Wet, Dry, Cold, Warm } t;
    switch (tool) {
    case Brush::Land:     t = st.inverse ? T::Sea : T::Land; break;
    case Brush::Sea:      t = st.inverse ? T::Land : T::Sea; break;
    case Brush::Mountain: t = st.inverse ? T::Erode : T::Mountain; break;
    case Brush::Smooth:   t = T::Smooth; break;
    case Brush::River:    t = T::River; break;
    case Brush::Wet:      t = st.inverse ? T::Dry : T::Wet; break;
    case Brush::Dry:      t = st.inverse ? T::Wet : T::Dry; break;
    case Brush::Cold:     t = st.inverse ? T::Warm : T::Cold; break;
    default:              t = T::Land; break;
    }

    std::vector<float> avg;
    const int aw = x1 - x0 + 1, ah = y1 - y0 + 1;
    { CARTO_PROF("dab: ruído");
    ensureNoise(world, 0, x0, y0, x1, y1);
    ensureNoise(world, 1, x0, y0, x1, y1);
    if (t == T::Mountain) ensureNoise(world, 2, x0, y0, x1, y1); }
    const NoiseCache& NC = *world.brushNoise;
    if (t == T::Smooth) {
        avg.resize(static_cast<size_t>(aw) * ah);
        parallelRows(y0, y1, static_cast<size_t>(aw) * ah, [&](int ya, int yb) { for (int yy = ya; yy <= yb; yy++) for (int xx = x0; xx <= x1; xx++) {
            double sum = 0; int n = 0;
            for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) {
                const int X = static_cast<int>(clampd(xx + dx, 0, W - 1)), Y = static_cast<int>(clampd(yy + dy, 0, H - 1));
                sum += h[static_cast<size_t>(Y) * W + X]; n++;
            }
            avg[static_cast<size_t>(yy - y0) * aw + (xx - x0)] = static_cast<float>(sum / n);
        } });
    }

    parallelRows(y0, y1, static_cast<size_t>(aw) * ah, [&](int ya, int yb) {
    for (int yy = ya; yy <= yb; yy++) {
        for (int xx = x0; xx <= x1; xx++) {
            const size_t i = static_cast<size_t>(yy) * W + xx;
            const double nE = noiseAt(NC, 0, xx, yy);
            const double Fi = F[i];
            const double warp = t == T::River ? 0.85 + 0.3 * nE : (0.74 + 0.5 * nE) * (1 - 0.28 * clampd(Fi, -1, 1));
            const double ddx = xx - x, ddy = yy - y;
            const double d = std::sqrt(ddx * ddx + ddy * ddy) / rr * warp;
            if (d >= 1) continue;
            const double f = (1 - d * d) * (1 - d * d);
            const double D = noiseAt(NC, 1, xx, yy);
            const double a = f * rate;
            switch (t) {
            case T::Land: {
                const double target = ref * (0.03 + 0.30 * smoothstep(-0.9, 1.0, Fi) + 0.04 * D);
                if (h[i] < target) h[i] = static_cast<float>(h[i] + (target - h[i]) * a);
                break;
            }
            case T::Sea: {
                const double target = -ref * (0.05 + 0.15 * smoothstep(-1, 1, -Fi) + 0.03 * D);
                if (h[i] > target) h[i] = static_cast<float>(h[i] + (target - h[i]) * a);
                break;
            }
            case T::Mountain: {
                const double rn = noiseAt(NC, 2, xx, yy);
                const double peak = 0.55 * smoothstep(-0.6, 1.0, Fi) + 0.45 * rn * rn;
                const double target = ref * (0.30 + 1.0 * peak) * (0.55 + 0.45 * (1 - d * d));
                if (h[i] < target) h[i] = static_cast<float>(h[i] + (target - h[i]) * a * 0.9);
                break;
            }
            case T::Erode: {
                const double target = ref * (0.10 + 0.12 * D);
                if (h[i] > target) h[i] = static_cast<float>(h[i] + (target - h[i]) * a * 0.6);
                break;
            }
            case T::Smooth:
                h[i] = static_cast<float>(h[i] + (static_cast<double>(avg[static_cast<size_t>(yy - y0) * aw + (xx - x0)]) - h[i]) * a * 1.5);
                break;
            case T::River:
                if (d < 0.55) h[i] = static_cast<float>(std::min(static_cast<double>(h[i]), -ref * 0.02));
                else if (h[i] > 0) {
                    const double bank = ref * 0.015 + (h[i] - ref * 0.015) * smoothstep(0.55, 1, d);
                    if (bank < h[i]) h[i] = static_cast<float>(bank);
                }
                break;
            case T::Wet:  m[i] = static_cast<float>(m[i] + (0.82 + 0.18 * D - m[i]) * a); break;
            case T::Dry:  m[i] = static_cast<float>(m[i] + (0.04 + 0.12 * D - m[i]) * a); k[i] = static_cast<float>(k[i] + (-0.12 - k[i]) * a * 0.5); break;
            case T::Cold: k[i] = static_cast<float>(k[i] + (0.42 + 0.25 * D - k[i]) * a); break;
            case T::Warm: k[i] = static_cast<float>(k[i] + (-0.18 - k[i]) * a); break;
            }
        }
    }
    });
    return std::array<int, 4>{ x0, y0, x1 + 1, y1 + 1 };
}

bool addArea(World& world, const PlaneRect& rect, double ang, int seed)
{
    CARTO_PROF("addArea");
    const int W = world.W, H = world.H;
    std::vector<float>& h = world.h;
    const double rx = rect.x, ry = rect.y, rw = rect.w, rh = rect.h;
    if (rw < 8 || rh < 8) return false;
    double x0 = std::numeric_limits<double>::infinity(), y0 = x0, x1 = -x0, y1 = -x0;
    const double cs[4][2] = { { rx, ry }, { rx + rw, ry }, { rx, ry + rh }, { rx + rw, ry + rh } };
    for (const auto& c : cs) {
        const auto q = planeToWorld(world, ang, c[0], c[1]);
        x0 = std::min(x0, q[0]); x1 = std::max(x1, q[0]); y0 = std::min(y0, q[1]); y1 = std::max(y1, q[1]);
    }
    x0 = std::max(0.0, std::floor(x0)); y0 = std::max(0.0, std::floor(y0));
    x1 = std::min(static_cast<double>(W), std::ceil(x1)); y1 = std::min(static_cast<double>(H), std::ceil(y1));
    if (x1 - x0 < 4 || y1 - y0 < 4) return false;
    const int gw = static_cast<int>(clampd(std::ceil(rw / 5) + 1, 24, 220));
    const int gh = static_cast<int>(clampd(std::ceil(rh / 5) + 1, 24, 220));
    const Upsampled up = islandRelief(gw, gh, 3000, seed, 0.45);
    const std::vector<float>& P = up.map;
    const int PW = up.W, PH = up.H;
    for (int y = static_cast<int>(y0); y < static_cast<int>(y1); y++) {
        for (int x = static_cast<int>(x0); x < static_cast<int>(x1); x++) {
            const auto p = worldToPlane(world, ang, x, y);
            const double lu = (p[0] - rx) / rw, lv = (p[1] - ry) / rh;
            if (lu < 0 || lu > 1 || lv < 0 || lv > 1) continue;
            const double u = lu * (PW - 1), v = lv * (PH - 1);
            const int ix = std::min(PW - 2, static_cast<int>(std::floor(u))); const double fx = u - ix;
            const int iy = std::min(PH - 2, static_cast<int>(std::floor(v))); const double fy = v - iy;
            const double pv = P[static_cast<size_t>(iy) * PW + ix] * (1 - fx) * (1 - fy) + P[static_cast<size_t>(iy) * PW + ix + 1] * fx * (1 - fy)
                            + P[static_cast<size_t>(iy + 1) * PW + ix] * (1 - fx) * fy + P[static_cast<size_t>(iy + 1) * PW + ix + 1] * fx * fy;
            const size_t i = static_cast<size_t>(y) * W + x;
            if (pv > h[i]) h[i] = static_cast<float>(pv);
        }
    }
    cleanup(world);
    updateRef(world);
    return true;
}

Snapshot snapshotRect(const World& w, const std::vector<float>& h, const std::vector<float>& m, const std::vector<float>& k,
                      int x0, int y0, int x1, int y1)
{
    CARTO_PROF("snapshotRect");
    Snapshot s; s.x0 = x0; s.y0 = y0; s.x1 = x1; s.y1 = y1;
    const int ww = x1 - x0, hh = y1 - y0, W = w.W;
    auto take = [&](const std::vector<float>& a) {
        std::vector<float> o(static_cast<size_t>(ww) * hh);
        for (int y = 0; y < hh; y++) std::copy_n(a.begin() + (static_cast<size_t>(y + y0) * W + x0), ww, o.begin() + static_cast<size_t>(y) * ww);
        return o;
    };
    s.h = take(h); s.m = take(m); s.k = take(k);
    return s;
}
void applySnapshot(World& w, const Snapshot& s)
{
    const int ww = s.x1 - s.x0, W = w.W;
    for (int y = 0; y < s.y1 - s.y0; y++) {
        const size_t o = static_cast<size_t>(y + s.y0) * W + s.x0;
        std::copy_n(s.h.begin() + static_cast<size_t>(y) * ww, ww, w.h.begin() + o);
        std::copy_n(s.m.begin() + static_cast<size_t>(y) * ww, ww, w.m.begin() + o);
        std::copy_n(s.k.begin() + static_cast<size_t>(y) * ww, ww, w.k.begin() + o);
    }
}

} // namespace carto
