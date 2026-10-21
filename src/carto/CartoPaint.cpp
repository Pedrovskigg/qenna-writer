#include "CartoPaint.h"
#include "CartoProf.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <thread>

namespace carto {

namespace {
const double kMiraPitch = std::asin(0.65);
const double kInkWater[4] = { 3.2, 7.4, 12.6, 19.5 };
const double kInkWaterA[4] = { 0.78, 0.5, 0.34, 0.2 };
inline double tEffRock(double tc) { return clampd((tc - kBioStart) / (kBioEnd - kBioStart), 0, 1); }
}

Camera cameraOf(const View& v)
{
    const double th = clampd(v.pitch, 20, 90) * M_PI / 180;
    const double sq = std::sin(th);
    const double liftK = (std::cos(th) / sq) / (std::cos(kMiraPitch) / std::sin(kMiraPitch));
    return { v.ang, sq, liftK, std::max(1, v.step) };
}
int padFor(const World& w) { return static_cast<int>(std::ceil(340.0 * w.W / 1500)); }

Geom viewGeom(const World& w, const View& v)
{
    const Camera cam = cameraOf(v);
    const int s = cam.step;
    const double WW = w.W, WH = w.H;
    const double P = padFor(w);
    const double ccx = WW / 2, ccy = WH / 2;
    const double ca = std::cos(cam.ang), sa = std::sin(cam.ang);
    double minx = std::numeric_limits<double>::infinity(), miny = minx, maxx = -minx, maxy = -minx;
    const double cs[4][2] = { { -P, -P }, { WW + P, -P }, { -P, WH + P }, { WW + P, WH + P } };
    for (const auto& c : cs) {
        const double dx = c[0] - ccx, dy = c[1] - ccy;
        const double px = ca * dx - sa * dy, py = sa * dx + ca * dy;
        minx = std::min(minx, px); maxx = std::max(maxx, px); miny = std::min(miny, py); maxy = std::max(maxy, py);
    }
    Geom g;
    const double U = (WW / 1500) / s;
    g.W = static_cast<int>(std::ceil((maxx - minx) / s)) + 1;
    g.H = static_cast<int>(std::ceil((maxy - miny) / s)) + 1;
    g.cu = -minx / s; g.cv = -miny / s; g.step = s; g.sq = cam.sq; g.ang = cam.ang; g.pad = static_cast<int>(P);
    g.ca = ca; g.sa = sa; g.ccx = ccx; g.ccy = ccy; g.U = U; g.style = v.style; g.info = v.info;
    g.extF = WH * 0.062 / 0.65 * cam.liftK / s;
    g.reach = static_cast<int>(std::ceil((210 + 2 * 55) * U)) + 2;
    g.nearReach = static_cast<int>(std::ceil((75 + 2 * 22) * U)) + 2;
    return g;
}

std::array<int, 4> worldRectToView(const Geom& g, double x0, double y0, double x1, double y1)
{
    double u0 = std::numeric_limits<double>::infinity(), v0 = u0, u1 = -u0, v1 = -u0;
    const double cs[4][2] = { { x0, y0 }, { x1, y0 }, { x0, y1 }, { x1, y1 } };
    for (const auto& c : cs) {
        const double dx = c[0] - g.ccx, dy = c[1] - g.ccy;
        const double u = (g.ca * dx - g.sa * dy) / g.step + g.cu, v = (g.sa * dx + g.ca * dy) / g.step + g.cv;
        u0 = std::min(u0, u); u1 = std::max(u1, u); v0 = std::min(v0, v); v1 = std::max(v1, v);
    }
    return { std::max(0, static_cast<int>(std::floor(u0)) - 2), std::max(0, static_cast<int>(std::floor(v0)) - 2),
             std::min(g.W, static_cast<int>(std::ceil(u1)) + 3), std::min(g.H, static_cast<int>(std::ceil(v1)) + 3) };
}
std::array<int, 4> dirtyToView(const Geom& g, const std::array<double, 4>& box, bool live)
{
    const auto r = worldRectToView(g, box[0], box[1], box[2], box[3]);
    const int reach = live ? g.nearReach : g.reach;
    const int e = static_cast<int>(std::ceil(g.extF)) + 2;
    return { std::max(0, r[0] - reach), std::max(0, r[1] - reach - e), std::min(g.W, r[2] + reach), std::min(g.H, r[3] + reach) };
}

double liftAt(const World& w, const View& v, double qx, double qy)
{
    const Camera cam = cameraOf(v);
    const int W = w.W, H = w.H;
    const int x = static_cast<int>(clampd(jsRound(qx), 0, W - 1)), y = static_cast<int>(clampd(jsRound(qy), 0, H - 1));
    const double t = std::max(0.0, static_cast<double>(w.h[static_cast<size_t>(y) * W + x])) / (w.ref != 0 ? w.ref : 1);
    const double extF = H * 0.062 / 0.65 * cam.liftK;
    if (v.style == Style::Mira) {
        const double a = std::max(0.0, (t - kLiftFloor) / (1 - kLiftFloor));
        return a > 0 ? std::pow(a, 0.55) * extF : 0;
    }
    const double a = std::max(0.0, (t - 0.12) / 0.88);
    const double k = v.style == Style::Atlas ? kAtlasLift : v.style == Style::Ink ? kInkLift : 1.0;
    return a > 0 ? std::pow(a, 1.3) * extF * 1.05 * k : 0;
}

// ── o pintor ──
DistField makeDistField(const World& world, const Geom& g, int tx0, int ty0, int tx1, int ty1, int threads)
{
    const int W = g.W, H = g.H, s = g.step, WW = world.W, WH = world.H;
    const int dM = g.style == Style::Ink ? 18 : 8, reach = g.reach, DS = 2;
    const std::vector<float>& wh = world.h;
    const double dux = g.ca * s, duy = -g.sa * s, dvx = g.sa * s, dvy = g.ca * s;
    const double bqx = g.ccx - g.cu * dux - g.cv * dvx, bqy = g.ccy - g.cu * duy - g.cv * dvy;
    DistField f;
    f.sx0 = std::max(0, static_cast<int>(std::floor(static_cast<double>(tx0 - reach - dM) / DS)) * DS);
    f.sy0 = std::max(0, static_cast<int>(std::floor(static_cast<double>(ty0 - reach - dM) / DS)) * DS);
    const int sx1 = std::min(W, tx1 + reach + dM), sy1 = std::min(H, ty1 + reach + dM);
    f.cw = static_cast<int>(std::ceil(static_cast<double>(sx1 - f.sx0) / DS)) + 1;
    f.ch = static_cast<int>(std::ceil(static_cast<double>(sy1 - f.sy0) / DS)) + 1;
    std::vector<float> hc(static_cast<size_t>(f.cw) * f.ch);
    auto rows = [&](int va, int vb) {
        for (int v = va; v < vb; v++) {
            const int V = f.sy0 + v * DS;
            double qx = bqx + f.sx0 * dux + V * dvx, qy = bqy + f.sx0 * duy + V * dvy;
            for (int u = 0; u < f.cw; u++, qx += dux * DS, qy += duy * DS) {
                double val = -1e3;
                if (!(qx < 0 || qy < 0 || qx > WW - 1 || qy > WH - 1)) {
                    const int ix = std::min(WW - 2, toInt32(qx)), iy = std::min(WH - 2, toInt32(qy));
                    const double fx = qx - ix, fy = qy - iy;
                    const size_t j = static_cast<size_t>(iy) * WW + ix;
                    val = wh[j] * (1 - fx) * (1 - fy) + wh[j + 1] * fx * (1 - fy) + wh[j + WW] * (1 - fx) * fy + wh[j + WW + 1] * fx * fy;
                }
                hc[static_cast<size_t>(v) * f.cw + u] = static_cast<float>(val);
            }
        }
    };
    if (threads <= 1 || f.ch < 64) rows(0, f.ch);
    else {
        std::vector<std::thread> th;
        for (int t = 0; t < threads; t++) th.emplace_back(rows, f.ch * t / threads, f.ch * (t + 1) / threads);
        for (std::thread& t : th) t.join();
    }
    f.d = distanceToLand(hc, f.cw, f.ch);
    return f;
}

void paintRegion(World& world, const Geom& g, uint8_t* out, int tx0, int ty0, int tx1, int ty1,
                 Aux* aux, const Dst* dst, const RealmPaint* realms, bool inkEdges, const DistField* dfIn)
{
    const int W = g.W, H = g.H, s = g.step;
    const int OW = dst ? dst->w : W, OX = dst ? dst->x0 : 0, OY = dst ? dst->y0 : 0;
    auto PI = [&](int x, int y) -> long long { return static_cast<long long>(y - OY) * OW + (x - OX); };
    const int WW = world.W, WH = world.H;
    const std::vector<float>& wh = world.h; const std::vector<float>& wm = world.m; const std::vector<float>& wk = world.k;
    const std::vector<float>& wt = ensureTempWave(world);
    const double maxH = world.ref;
    // dM: folga em volta do pedaço (derivada 3 + cavidade 6); a Tinta borra o relevo num raio maior e precisa de 18
    const int eR = static_cast<int>(std::ceil(g.extF)) + 2, dM = g.style == Style::Ink ? 18 : 8, reach = g.reach;

    for (int y = ty0; y < ty1; y++) std::fill(out + PI(tx0, y) * 4, out + PI(tx1, y) * 4, 0);
    int16_t* AD = aux ? aux->depth.data() : nullptr;
    float* AL = aux ? aux->lift.data() : nullptr;
    int8_t* AN = (aux && !aux->nrm.empty()) ? aux->nrm.data() : nullptr;
    if (aux) for (int y = ty0; y < ty1; y++) { std::fill(AD + PI(tx0, y), AD + PI(tx1, y), -1); std::fill(AL + PI(tx0, y), AL + PI(tx1, y), 0.0f); }
    if (AN) for (int y = ty0; y < ty1; y++) std::fill(AN + 2 * PI(tx0, y), AN + 2 * PI(tx1, y), 0);
    if (!(maxH > 0)) return;

    auto sampleW = [&](const std::vector<float>& arr, double qx, double qy, double out0) -> double {
        if (qx < 0 || qy < 0 || qx > WW - 1 || qy > WH - 1) return out0;
        const int ix = std::min(WW - 2, toInt32(qx)), iy = std::min(WH - 2, toInt32(qy));
        const double fx = qx - ix, fy = qy - iy;
        const size_t j = static_cast<size_t>(iy) * WW + ix;
        return arr[j] * (1 - fx) * (1 - fy) + arr[j + 1] * fx * (1 - fy) + arr[j + WW] * (1 - fx) * fy + arr[j + WW + 1] * fx * fy;
    };
    const double dux = g.ca * s, duy = -g.sa * s, dvx = g.sa * s, dvy = g.ca * s;
    const double bqx = g.ccx - g.cu * dux - g.cv * dvx, bqy = g.ccy - g.cu * duy - g.cv * dvy;

    // B: vizinhança justa, resolução cheia
    const int rowEnd = std::min(H, ty1 + eR);
    const int bx0 = std::max(0, tx0 - dM), by0 = std::max(0, ty0 - dM);
    const int bx1 = std::min(W, tx1 + dM), by1 = std::min(H, rowEnd + dM);
    const int bw = bx1 - bx0, bh = by1 - by0;
    const size_t NB = static_cast<size_t>(bw) * bh;
    std::vector<float> h(NB), m(NB), k(NB), tw(NB), qxA(NB), qyA(NB);
    for (int v = 0; v < bh; v++) {
        if (g.cancel && g.cancel->load(std::memory_order_relaxed)) return;
        const int V = v + by0;
        double qx = bqx + bx0 * dux + V * dvx, qy = bqy + bx0 * duy + V * dvy;
        for (int u = 0; u < bw; u++, qx += dux, qy += duy) {
            const size_t i = static_cast<size_t>(v) * bw + u;
            qxA[i] = static_cast<float>(qx); qyA[i] = static_cast<float>(qy);
            h[i] = static_cast<float>(sampleW(wh, qx, qy, -1e3));
            m[i] = static_cast<float>(sampleW(wm, qx, qy, 0.5));
            k[i] = static_cast<float>(sampleW(wk, qx, qy, 0));
            tw[i] = static_cast<float>(sampleW(wt, qx, qy, 0));
        }
    }
    auto LB = [&](int x, int y) -> long long { return static_cast<long long>(y - by0) * bw + (x - bx0); };
    // leitura com a mesma tolerância do JS (fora do buffer = NaN, comparações falsas)
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    auto hh = [&](long long i) -> double { return (i >= 0 && i < static_cast<long long>(NB)) ? static_cast<double>(h[static_cast<size_t>(i)]) : NaN; };
    auto at = [&](int x, int y) -> double { return h[static_cast<size_t>(clampd(y - by0, 0, bh - 1) * bw + clampd(x - bx0, 0, bw - 1))]; };
    auto smp = [&](double x, double y) -> double {
        x = clampd(x - bx0, 0, bw - 1.001); y = clampd(y - by0, 0, bh - 1.001);
        const int ix = toInt32(x), iy = toInt32(y);
        const double fx = x - ix, fy = y - iy;
        const size_t j = static_cast<size_t>(iy) * bw + ix;
        return h[j] * (1 - fx) * (1 - fy) + h[j + 1] * fx * (1 - fy) + h[j + bw] * (1 - fx) * fy + h[j + bw + 1] * fx * fy;
    };

    // S: vizinhança larga em meia resolução, só pra medir a distância até a costa
    // (quem pinta em faixas manda o campo pronto, calculado uma vez pro retângulo inteiro)
    const int DS = 2;
    DistField dfOwn;
    if (!dfIn) dfOwn = makeDistField(world, g, tx0, ty0, tx1, ty1, 1);
    const DistField& DF = dfIn ? *dfIn : dfOwn;
    const int sx0 = DF.sx0, sy0 = DF.sy0, cw = DF.cw, ch = DF.ch;
    const std::vector<float>& distC = DF.d;
    (void)reach;
    auto distAt = [&](double x, double y) -> double {
        const double u = clampd((x - sx0) / DS, 0, cw - 1.001), v = clampd((y - sy0) / DS, 0, ch - 1.001);
        const int iu = toInt32(u), iv = toInt32(v);
        const double fu = u - iu, fv = v - iv;
        const size_t j = static_cast<size_t>(iv) * cw + iu;
        return (distC[j] * (1 - fu) * (1 - fv) + distC[j + 1] * fu * (1 - fv) + distC[j + cw] * (1 - fu) * fv + distC[j + cw + 1] * fu * fv) * DS;
    };

    // 1. cor de bioma por pixel, suavizada
    const bool rich = g.style == Style::Rich, atlas = g.style == Style::Atlas, ink = g.style == Style::Ink, soft = rich || atlas || ink;
    const NoiseTiles* NZ = (rich || ink) ? &noiseTiles() : nullptr;
    const double fw = std::sqrt(std::max(1.0, WW / 1000.0));
    const double HPER = 3.4 * fw / s, SQ = g.sq;
    auto hatchK = [&](double X, double Y, double dk) -> double {
        if (dk < 0.05) return 0;
        const double e1 = X + Y * SQ, d1 = std::fabs(e1 - jsRound(e1 / HPER) * HPER) * 0.7071;
        double c = clampd(dk * HPER * 0.3 - d1 + 0.5, 0, 1) * std::min(1.0, dk * 2.2);
        if (dk > 0.6) {
            const double e2 = X - Y * SQ, d2 = std::fabs(e2 - jsRound(e2 / HPER) * HPER) * 0.7071;
            c = std::max(c, clampd((dk - 0.6) * HPER * 0.5 - d2 + 0.5, 0, 1));
        }
        return c;
    };
    auto inkMix = [&](long long o, double kk, double pn) {
        out[o]     = u8c(kPaper[0] * pn * (1 - kk) + kInk[0] * kk);
        out[o + 1] = u8c(kPaper[1] * pn * (1 - kk) + kInk[1] * kk);
        out[o + 2] = u8c(kPaper[2] * pn * (1 - kk) + kInk[2] * kk);
    };
    const RealmPaint* info = g.info ? realms : nullptr;
    std::vector<float> R(NB, 0.0f), G(NB, 0.0f), B(NB, 0.0f), Wt(NB, 0.0f);
    std::vector<int8_t> idA(NB, 0);
    for (size_t i = 0; i < NB; i++) {
        if (h[i] <= 0) continue;
        const int id = biomeId(h[i] / maxH, latCold(world, qyA[i], tw[i] / 0.09), m[i], k[i] + tw[i] * 0.4);
        idA[i] = static_cast<int8_t>(id);
        RGB c;
        if (rich) {
            const double n1 = tileAt(NZ->A, qxA[i] * 0.5, qyA[i] * 0.5);
            const double n2 = tileAt(NZ->A, qxA[i] * 0.17 + 91, qyA[i] * 0.17 + 37);
            double u = 0.55 * n1 + 0.45 * n2;
            if (id == 1 || id == 2 || id == 4) u += (0.5 - m[i]) * 0.7;
            c = richTone(id, clampd((u - 0.5) * 2.1 + 0.5, 0, 1));
        } else c = atlas ? kAtlas[id] : ink ? kPaper : kBiomeColors[id];
        R[i] = static_cast<float>(c[0]); G[i] = static_cast<float>(c[1]); B[i] = static_cast<float>(c[2]); Wt[i] = 1;
    }
    if (!ink) boxBlurWeighted({ &R, &G, &B }, Wt, bw, bh, std::max(1, static_cast<int>(jsRound((rich ? 2.0 : atlas ? 4.0 : 3.0) / s))));

    // a altura que ERGUE a montanha é lida suavizada; cor e sombra usam a cheia
    std::vector<float> hLiftBuf;
    const std::vector<float>* hLiftP = &h;
    if (soft) {
        hLiftBuf = h;
        std::vector<float> ones(NB, 1.0f);
        boxBlurWeighted({ &hLiftBuf }, ones, bw, bh, std::max(1, static_cast<int>(jsRound(3.0 / s))));
        hLiftP = &hLiftBuf;
    }
    const std::vector<float>& hLift = *hLiftP;
    std::vector<float> hInk;
    if (ink) {
        hInk = hLift;
        std::vector<float> ones(NB, 1.0f);
        boxBlurWeighted({ &hInk }, ones, bw, bh, std::max(1, static_cast<int>(jsRound(7.0 / s))));
    }

    // 2. água rasa (ou linhas d'água na Tinta)
    const double nearR = 75 * g.U, nearS = 22 * g.U, farR = 210 * g.U, farS = 55 * g.U;
    for (int y = ty0; y < ty1; y++) for (int x = tx0; x < tx1; x++) {
        const long long i = LB(x, y);
        if (h[static_cast<size_t>(i)] > 0) continue;
        const double d = distAt(x, y);
        if (ink) {
            const double qx = bqx + x * dux + y * dvx, qy = bqy + x * duy + y * dvy;
            const double dw = d * s + (tileAt(NZ->A, qx * 0.9, qy * 0.9) - 0.5) * 1.6 * fw;
            double a = 0;
            for (int L = 0; L < 4; L++) {
                const double dd = std::fabs(dw - kInkWater[L] * fw) / s;
                if (dd >= 2) continue;
                const double Rr = (kInkWater[L] * fw + 3) / s;
                bool open = false;
                for (int q = 0; q < 8 && !open; q++) {
                    const double an = q * M_PI / 4;
                    if (distAt(x + std::cos(an) * Rr, y + std::sin(an) * Rr) * s > kInkWater[L] * fw * 1.45 + 2) open = true;
                }
                if (!open) continue;
                a = std::max(a, clampd(0.5 + (L ? 0.5 : 0.7) - dd, 0, 1) * kInkWaterA[L]);
            }
            if (a <= 0.002) continue;
            const long long o = PI(x, y) * 4;
            out[o] = u8c(kInk[0]); out[o + 1] = u8c(kInk[1]); out[o + 2] = u8c(kInk[2]); out[o + 3] = u8c(a * 255);
            continue;
        }
        const double af = 0.50 * (1 - smoothstep(0.15 * farR, farR + 2 * farS, d));
        const double an = 0.80 * (1 - smoothstep(nearR - 2 * nearS, nearR + 2 * nearS, d));
        const double a = an + af * (1 - an);
        if (a <= 0.002) continue;
        const long long o = PI(x, y) * 4;
        double vr = 1, vg = 1, vb = 1;
        if (rich) {
            const double qx = bqx + x * dux + y * dvx, qy = bqy + x * duy + y * dvy;
            const double n = tileAt(NZ->A, qx * 0.35 + 17, qy * 0.35 + 53);
            const double near = 1 - smoothstep(0, nearR * 0.6, d);
            vr = 0.90 + 0.14 * n - 0.10 * near; vg = 0.95 + 0.08 * n + 0.02 * near; vb = 0.97 + 0.05 * n - 0.06 * near;
        }
        if (atlas) {
            out[o] = u8c((204 * an + 186 * af * (1 - an)) / a); out[o + 1] = u8c((226 * an + 212 * af * (1 - an)) / a); out[o + 2] = u8c((226 * an + 216 * af * (1 - an)) / a);
            out[o + 3] = u8c(a * 255);
            continue;
        }
        out[o]     = u8c((130 * an + 110 * af * (1 - an)) / a * vr);
        out[o + 1] = u8c((210 * an + 200 * af * (1 - an)) / a * vg);
        out[o + 2] = u8c((240 * an + 235 * af * (1 - an)) / a * vb);
        out[o + 3] = u8c(a * 255);
    }

    auto over = [&](long long o, double r, double gg, double b, double cov) {
        const double aE = out[o + 3] / 255.0;
        const double a = cov + aE * (1 - cov);
        if (a <= 0) return;
        const double o0 = out[o], o1 = out[o + 1], o2 = out[o + 2];
        out[o]     = u8c((r * cov + o0 * aE * (1 - cov)) / a);
        out[o + 1] = u8c((gg * cov + o1 * aE * (1 - cov)) / a);
        out[o + 2] = u8c((b * cov + o2 * aE * (1 - cov)) / a);
        out[o + 3] = u8c(a * 255);
    };
    auto touchesLand = [&](long long i) {
        return hh(i - 1) > 0 || hh(i + 1) > 0 || hh(i - bw) > 0 || hh(i + bw) > 0 ||
               hh(i - bw - 1) > 0 || hh(i - bw + 1) > 0 || hh(i + bw - 1) > 0 || hh(i + bw + 1) > 0;
    };
    // antisserrilhado da costa, lado do mar
    for (int y = std::max(by0 + 1, ty0); y < std::min(by1 - 1, ty1); y++)
        for (int x = std::max(bx0 + 1, tx0); x < std::min(bx1 - 1, tx1); x++) {
            const long long i = LB(x, y);
            if (hh(i) > 0 || !touchesLand(i)) continue;
            const double gr = jsHypot((hh(i + 1) - hh(i - 1)) * 0.5, (hh(i + bw) - hh(i - bw)) * 0.5);
            if (!(gr > 1e-6)) continue;
            const double cov = clampd(0.5 + hh(i) / gr, 0, 1);
            if (cov > 0.01) {
                if (atlas) over(PI(x, y) * 4, 120, 110, 92, cov * 0.8);
                else if (ink) over(PI(x, y) * 4, kInk[0], kInk[1], kInk[2], cov);
                else over(PI(x, y) * 4, kSand[0] * 0.97, kSand[1] * 0.97, kSand[2] * 0.97, cov);
            }
        }

    // 3. terreno isométrico (pintor: de trás pra frente, coluna sólida)
    const double extF = g.extF;
    const double ZF = 3.5, LX = -0.55, LY = -0.75, LZ = 1.8;
    const double LLn = std::sqrt(LX * LX + LY * LY + LZ * LZ);
    const double AMBIENT = 0.76, DIRECT = 0.28;
    const double o1 = 2.0 / s, o2 = 3.0 / s;
    const bool fast = s == 1;
    auto matNoise = [](double x, double y) {
        const double t = std::sin(x * 0.41 + y * 0.83) * 43758.5453;
        return (t - std::floor(t)) * 0.06 - 0.03;
    };

    for (int row = ty0; row < rowEnd; row++) {
        if (g.cancel && g.cancel->load(std::memory_order_relaxed)) return;
        for (int col = tx0; col < tx1; col++) {
            const long long i = LB(col, row);
            const size_t iu = static_cast<size_t>(i);
            const double hv = h[iu];
            if (hv <= 0) continue;
            const double t = hv / maxH;
            double lift;
            if (soft) {
                const double tl = hLift[iu] / maxH;
                const double ta = std::max(0.0, (tl - 0.12) / 0.88);
                lift = ta > 0 ? std::pow(ta, 1.3) * extF * 1.05 * (atlas ? kAtlasLift : 1) : 0;
            } else {
                const double tAdj = std::max(0.0, (t - kLiftFloor) / (1 - kLiftFloor));
                lift = tAdj > 0 ? std::pow(tAdj, 0.55) * extF : 0;
            }
            const bool low = lift < 0.5;
            if (low ? row >= ty1 : (soft ? std::floor(row - lift) : jsRound(row - lift)) >= ty1) continue;

            const double tc = std::min(t, 1.15);
            const double gx = qxA[iu] / 5.0, gy = qyA[iu] / 5.0;
            double cr, cg, cb;
            if (ink) { cr = kPaper[0]; cg = kPaper[1]; cb = kPaper[2]; }
            else if (tc < 0.04) { if (atlas) { cr = 232; cg = 218; cb = 182; } else { cr = kSand[0]; cg = kSand[1]; cb = kSand[2]; } }
            else {
                cr = R[iu]; cg = G[iu]; cb = B[iu];
                const double tEff = tc + matNoise(gx * 1.7, gy * 1.7) * 1.5;
                if (tEff >= kBioStart) {
                    const double bl = std::min(1.0, (tEff - kBioStart) / (kBioEnd - kBioStart));
                    const double sb = bl * bl * (3 - 2 * bl);
                    const RGB& RK = atlas ? kAtlasRock : kRock; const RGB& RH = atlas ? kAtlasRockHi : kRockHi; const RGB& SN = atlas ? kAtlasSnow : kSnow;
                    double rr, rg, rb;
                    if (tc < kRockMid) { rr = RK[0]; rg = RK[1]; rb = RK[2]; }
                    else if (tc < kSnowAt) {
                        double q = (tc - kRockMid) / (kSnowAt - kRockMid); q = q * q * (3 - 2 * q);
                        rr = RK[0] + (RH[0] - RK[0]) * q; rg = RK[1] + (RH[1] - RK[1]) * q; rb = RK[2] + (RH[2] - RK[2]) * q;
                    } else {
                        double q = std::min(1.0, (tc - kSnowAt) / 0.14); q = q * q * (3 - 2 * q);
                        rr = RH[0] + (SN[0] - RH[0]) * q; rg = RH[1] + (SN[1] - RH[1]) * q; rb = RH[2] + (SN[2] - RH[2]) * q;
                    }
                    const double snowy = clampd((cb - 180) / 55, 0, 1);
                    if (snowy > 0) {
                        const double q = snowy * 0.85;
                        rr += (SN[0] - rr) * q; rg += (SN[1] - rg) * q; rb += (SN[2] - rb) * q;
                    }
                    cr += (rr - cr) * sb; cg += (rg - cg) * sb; cb += (rb - cb) * sb;
                }
            }

            double dzdx, dzdy;
            if (fast) {
                dzdx = ((hh(i + 2) + hh(i + 3)) - (hh(i - 2) + hh(i - 3))) * 0.5 * ZF;
                dzdy = ((hh(i + 2 * bw) + hh(i + 3 * bw)) - (hh(i - 2 * bw) + hh(i - 3 * bw))) * 0.5 * ZF;
            } else {
                dzdx = ((smp(col + o1, row) + smp(col + o2, row)) - (smp(col - o1, row) + smp(col - o2, row))) * 0.5 * ZF;
                dzdy = ((smp(col, row + o1) + smp(col, row + o2)) - (smp(col, row - o1) + smp(col, row - o2))) * 0.5 * ZF;
            }
            const double slopeFull = std::sqrt(dzdx * dzdx + dzdy * dzdy);
            if (soft) {
                double sx, sy;
                auto hl = [&](long long j) -> double { return (j >= 0 && j < static_cast<long long>(NB)) ? static_cast<double>(hLift[static_cast<size_t>(j)]) : NaN; };
                if (fast) {
                    sx = ((hl(i + 2) + hl(i + 3)) - (hl(i - 2) + hl(i - 3))) * 0.5 * ZF;
                    sy = ((hl(i + 2 * bw) + hl(i + 3 * bw)) - (hl(i - 2 * bw) + hl(i - 3 * bw))) * 0.5 * ZF;
                } else {
                    auto sl = [&](double x, double y) -> double {
                        return hLift[static_cast<size_t>(clampd(jsRound(y) - by0, 0, bh - 1) * bw + clampd(jsRound(x) - bx0, 0, bw - 1))];
                    };
                    sx = ((sl(col + o1, row) + sl(col + o2, row)) - (sl(col - o1, row) + sl(col - o2, row))) * 0.5 * ZF;
                    sy = ((sl(col, row + o1) + sl(col, row + o2)) - (sl(col, row - o1) + sl(col, row - o2))) * 0.5 * ZF;
                }
                dzdx = 0.72 * sx + 0.28 * dzdx; dzdy = 0.72 * sy + 0.28 * dzdy;
            }
            const double nl = std::sqrt(dzdx * dzdx + dzdy * dzdy + 1);
            const double dotRaw = (-dzdx * LX - dzdy * LY + LZ) / (nl * LLn);
            const double dot = std::isnan(dotRaw) ? dotRaw : std::max(0.0, dotRaw);
            const int8_t nqx = toI8(jsRound(-dzdx / nl * 127)), nqy = toI8(jsRound(-dzdy / nl * 127));
            double shade = AMBIENT + dot * DIRECT;
            double nz = matNoise(gx, gy);
            const double lfRaw = (shade - AMBIENT) / DIRECT;
            const double lf = std::isnan(lfRaw) ? lfRaw : std::max(0.0, lfRaw);
            double tr = 0.95 + 0.05 * lf, tg = 0.96 + 0.04 * lf, tb = 1.02 - 0.02 * lf;
            if (atlas) { shade = 0.95 + dot * 0.09; nz = 0; tr = tg = tb = 1; }

            if (rich) {
                const double qx = qxA[iu], qy = qyA[iu];
                const int id = idA[iu];
                const double d1 = tileAt(NZ->B, qx, qy), d2 = tileAt(NZ->B, qx * 2.3 + 13, qy * 2.3 + 7);
                const int o6 = std::max(1, static_cast<int>(jsRound(6.0 / s)));
                const double cav = fast
                    ? hv - 0.25 * (hh(i - o6) + hh(i + o6) + hh(i - static_cast<long long>(o6) * bw) + hh(i + static_cast<long long>(o6) * bw))
                    : hv - 0.25 * (smp(col - o6, row) + smp(col + o6, row) + smp(col, row - o6) + smp(col, row + o6));
                const double cv = clampd(cav / maxH * 9, -1, 1) * (cav < 0 ? smoothstep(0.06, 0.16, tc) : 1);
                const double rocky = smoothstep(40, 62, slopeFull) * smoothstep(0.10, 0.26, tc) * (id == 0 ? 0.35 : 0.6);
                if (rocky > 0) { cr += (kRock[0] - cr) * rocky; cg += (kRock[1] - cg) * rocky; cb += (kRock[2] - cb) * rocky; }
                double tex = 0;
                if (tc < 0.035) tex = (d2 - 0.5) * 0.06 - (tc < 0.012 ? 0.12 : 0);
                else if (tEffRock(tc) > 0.5) tex = (d2 - 0.5) * 0.14 + (d1 - 0.5) * 0.06;
                else if (id == 1) tex = -0.15 * smoothstep(0.52, 0.74, d1) + (d2 - 0.5) * 0.08;
                else if (id == 4) {
                    const double pool = smoothstep(0.60, 0.74, d1) * 0.55;
                    cr += (104 - cr) * pool; cg += (168 - cg) * pool; cb += (172 - cb) * pool;
                    tex = (d2 - 0.5) * 0.08 + pool * 0.10;
                }
                else if (id == 2) tex = (d2 - 0.5) * 0.10 - 0.08 * smoothstep(0.66, 0.82, d1);
                else if (id == 3) {
                    const double flat = 1 - smoothstep(1, 6, lift);
                    tex = flat * 0.028 * std::sin(qx * 0.11 + qy * 0.06 + tileAt(NZ->A, qx * 0.5, qy * 0.5) * 6) + (d2 - 0.5) * 0.04;
                }
                else tex = (d2 - 0.5) * 0.05;
                nz = tex + cv * 0.06;
                tr = 0.95 + 0.07 * lf; tg = 0.97 + 0.03 * lf; tb = 1.03 - 0.07 * lf;
                shade = AMBIENT + 0.02 + dot * DIRECT;
            }

            // Tinta: tom da luz (vira hachura) e traço fixo do bioma e da fronteira
            double inkD = 0, inkE = 0, paperN = 1;
            if (ink) {
                const int oI = std::max(1, static_cast<int>(jsRound(3.0 / s)));
                const double dI = 1.0 / (2 * oI * s);
                auto hi = [&](long long j) { return static_cast<double>(hInk[static_cast<size_t>(clampd(static_cast<double>(j), 0, static_cast<double>(NB - 1)))]); };
                const double gX = (hi(i + oI) - hi(i - oI)) * dI;
                const double gY = (hi(i + static_cast<long long>(oI) * bw) - hi(i - static_cast<long long>(oI) * bw)) * dI;
                const double qL = (gX * LX + gY * LY) / LLn / maxH * 100;
                inkD = 0.72 * smoothstep(0.22, 1.3, qL) * (0.3 + 0.7 * smoothstep(0.12, 0.45, tc));
                const double qx = qxA[iu], qy = qyA[iu];
                const int id = idA[iu];
                paperN = 0.985 + 0.03 * tileAt(NZ->B, qx * 1.3, qy * 1.3);
                if (id == 3 && tc < 0.32) { if (hash2(std::floor(qx * 1.2), std::floor(qy * 1.2), 71) > 0.955) inkE = 0.55; }
                else if (id == 4 && tc < 0.2) {
                    const double v = qy * 0.42;
                    if (v - std::floor(v) < 0.2 && tileAt(NZ->B, qx * 0.35, qy * 1.4) > 0.62) inkE = 0.6;
                } else if (id == 1 && tc < 0.36) { if (hash2(std::floor(qx * 0.8), std::floor(qy * 0.8), 73) > 0.985) inkE = 0.35; }
            }

            if (info && ink) {
                const RealmSample ra = info->realmAt(qxA[iu], qyA[iu]);
                if (ra.edge > 0.01 && ra.eid >= 0) {
                    const double dash = std::sin((qxA[iu] + qyA[iu] * 0.83) * 0.62) > -0.25 ? 1 : 0.15;
                    inkE = std::max(inkE, smoothstep(0.25, 0.85, ra.edge) * 0.9 * dash);
                }
            } else if (info) {
                const RealmSample ra = info->realmAt(qxA[iu], qyA[iu]);
                if (ra.id >= 0) {
                    const RGB rc = info->realmColor(ra.id);
                    const double ta = atlas ? 0.30 : 0.12;
                    cr += (rc[0] - cr) * ta; cg += (rc[1] - cg) * ta; cb += (rc[2] - cb) * ta;
                }
                if (ra.edge > 0.01 && ra.eid >= 0) {
                    const RGB ec = info->realmColor(ra.eid);
                    const double ea = 0.85 * ra.edge;
                    cr += (ec[0] * 0.45 - cr) * ea; cg += (ec[1] * 0.45 - cg) * ea; cb += (ec[2] * 0.45 - cb) * ea;
                }
            }

            if (low && ink) {
                const bool near1 = hh(i - 1) <= 0 || hh(i + 1) <= 0 || hh(i - bw) <= 0 || hh(i + bw) <= 0;
                double kk = std::max(inkE, hatchK(col, row, inkD));
                if (near1) kk = 1;
                else if (hh(i - 2) <= 0 || hh(i + 2) <= 0 || hh(i - 2 * bw) <= 0 || hh(i + 2 * bw) <= 0) kk = std::max(kk, 0.5);
                double cov = 1;
                if (near1) {
                    const double gr = jsHypot((at(col + 1, row) - at(col - 1, row)) * 0.5, (at(col, row + 1) - at(col, row - 1)) * 0.5);
                    if (gr > 1e-6) cov = clampd(0.5 + hv / gr, 0, 1);
                }
                const long long o = PI(col, row) * 4;
                if (cov >= 0.999) { inkMix(o, kk, paperN); out[o + 3] = 255; }
                else over(o, kInk[0], kInk[1], kInk[2], cov);
                if (AD) { AD[PI(col, row)] = static_cast<int16_t>(row); AL[PI(col, row)] = static_cast<float>(lift); }
                if (AN) { const long long p2 = 2 * PI(col, row); AN[p2] = nqx; AN[p2 + 1] = nqy; }
                continue;
            }
            if (low) {
                const double ls = shade + nz;
                double r = cr * ls * tr, gg = cg * ls * tg, b = cb * ls * tb;
                const bool coast = hh(i - 1) <= 0 || hh(i + 1) <= 0 || hh(i - bw) <= 0 || hh(i + bw) <= 0;
                if (atlas && coast) { r = 120; gg = 110; b = 92; }
                double cov = 1;
                if (coast) {
                    const double gr = jsHypot((at(col + 1, row) - at(col - 1, row)) * 0.5, (at(col, row + 1) - at(col, row - 1)) * 0.5);
                    if (gr > 1e-6) cov = clampd(0.5 + hv / gr, 0, 1);
                }
                const long long o = PI(col, row) * 4;
                if (cov >= 0.999) { out[o] = u8c(r); out[o + 1] = u8c(gg); out[o + 2] = u8c(b); out[o + 3] = 255; }
                else over(o, r, gg, b, cov);
                if (AD) { AD[PI(col, row)] = static_cast<int16_t>(row); AL[PI(col, row)] = static_cast<float>(lift); }
                if (AN) { const long long p2 = 2 * PI(col, row); AN[p2] = nqx; AN[p2 + 1] = nqy; }
                continue;
            }

            const double topF = row - lift;
            const double surfaceY = soft ? std::floor(topF) : jsRound(topF);
            const int top = static_cast<int>(std::max(0.0, surfaceY));
            const double topCov = soft && surfaceY >= 0 ? 1 - (topF - surfaceY) : 1;
            const int yStart = std::max(ty0, top), yEnd = std::min(ty1, row + 1);
            const int colH = std::max(1, row + 1 - top);
            if (AL && row < ty1) AL[PI(col, row)] = static_cast<float>(lift);
            if (ink) {
                for (int fy = yStart; fy < yEnd; fy++) {
                    const double frac = static_cast<double>(fy - top) / colH;
                    const double kk = std::max(inkE, hatchK(col, fy, clampd(inkD + frac * 0.4, 0, 0.85)));
                    const long long o = PI(col, fy) * 4;
                    if (AD) AD[PI(col, fy)] = static_cast<int16_t>(row);
                    if (AN) { const long long p2 = 2 * PI(col, fy); AN[p2] = nqx; AN[p2 + 1] = nqy; }
                    if (fy == top && topCov < 0.999) {
                        over(o, kPaper[0] * paperN * (1 - kk) + kInk[0] * kk, kPaper[1] * paperN * (1 - kk) + kInk[1] * kk,
                             kPaper[2] * paperN * (1 - kk) + kInk[2] * kk, topCov);
                        continue;
                    }
                    inkMix(o, kk, paperN); out[o + 3] = 255;
                }
                continue;
            }
            const double earthK = tc < kBioStart ? 0.10 : 0;
            for (int fy = yStart; fy < yEnd; fy++) {
                const double frac = static_cast<double>(fy - top) / colH;
                const double earth = earthK * frac;
                const double ls = shade * (1 - frac * 0.22) + nz;
                const long long o = PI(col, fy) * 4;
                const double r = (cr * (1 - earth) + 78 * earth) * ls * tr;
                const double gg = (cg * (1 - earth) + 58 * earth) * ls * tg;
                const double b = (cb * (1 - earth) + 42 * earth) * ls * tb;
                if (AD) AD[PI(col, fy)] = static_cast<int16_t>(row);
                if (AN) { const long long p2 = 2 * PI(col, fy); AN[p2] = nqx; AN[p2 + 1] = nqy; }
                if (fy == top && topCov < 0.999) { over(o, r, gg, b, topCov); continue; }
                out[o] = u8c(r); out[o + 1] = u8c(gg); out[o + 2] = u8c(b); out[o + 3] = 255;
            }
        }
    }

    if (ink && AD && inkEdges) inkEdgePass(g, out, *aux, tx0, ty0, tx1, ty1, dst);
}

// Tinta: contorno das cristas (o pixel de cima é chão bem mais ao fundo)
void inkEdgePass(const Geom& g, uint8_t* out, const Aux& aux, int tx0, int ty0, int tx1, int ty1, const Dst* dst)
{
    const int s = g.step;
    const int OW = dst ? dst->w : g.W, OX = dst ? dst->x0 : 0, OY = dst ? dst->y0 : 0;
    auto PI = [&](int x, int y) -> long long { return static_cast<long long>(y - OY) * OW + (x - OX); };
    const int16_t* AD = aux.depth.data();
    {
        const double T0 = 4.0 / s, T1 = 10.0 / s;
        const int xa = std::max(tx0, OX + 1), xb = std::min(tx1, OX + OW - 1);
        for (int y = std::max(ty0, OY + 1); y < ty1; y++) for (int x = xa; x < xb; x++) {
            const long long p = PI(x, y);
            const int r = AD[p];
            if (r < 0) continue;
            double e = 0;
            const int up = AD[p - OW], lfv = AD[p - 1], rt = AD[p + 1];
            if (up >= 0) e = smoothstep(T0, T1, r - up);
            if (lfv >= 0) e = std::max(e, smoothstep(T0, T1, r - lfv) * 0.8);
            if (rt >= 0) e = std::max(e, smoothstep(T0, T1, r - rt) * 0.8);
            if (e < 0.03) continue;
            const long long o = p * 4;
            const double kk = e * 0.92;
            for (int c = 0; c < 3; c++) out[o + c] = u8c(out[o + c] + (kInk[c] - out[o + c]) * kk);
        }
    }
}

Render render(World& w, const View& v, const RealmPaint* realms, bool withAux, int threads, const std::atomic<bool>* cancel)
{
    CARTO_PROF("render");
    Render r;
    r.g = viewGeom(w, v);
    r.data.assign(static_cast<size_t>(r.g.W) * r.g.H * 4, 0);
    if (withAux) r.aux.resize(static_cast<size_t>(r.g.W) * r.g.H);
    r.g.cancel = cancel;
    paintBands(w, r.g, r.data.data(), 0, 0, r.g.W, r.g.H, withAux ? &r.aux : nullptr, realms, threads);
    r.g.cancel = nullptr;
    return r;
}

// Faixas horizontais independentes (o pintor sabe pintar qualquer retângulo e
// lê a vizinhança que precisa), uma fila por núcleo; o contorno da Tinta roda
// no fim, quando todas as linhas existem. Resultado idêntico ao de uma faixa só.
void paintBands(World& w, const Geom& g, uint8_t* out, int tx0, int ty0, int tx1, int ty1, Aux* aux,
                const RealmPaint* realms, int threads)
{
    CARTO_PROF("paintBands");
    ensureTempWave(w);
    noiseTiles();
    if (threads <= 0) threads = static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    // faixas VERTICAIS: a coluna de relevo só sobe (até extF px), então faixa horizontal
    // fina repetia a pintura das linhas de baixo; de lado a sobra é só a folga dM
    const int cols = tx1 - tx0;
    const int bands = std::max(1, std::min(cols / 48, threads * 2));
    if (bands <= 1 || threads <= 1) {
        paintRegion(w, g, out, tx0, ty0, tx1, ty1, aux, nullptr, realms, true);
        return;
    }
    const DistField df = makeDistField(w, g, tx0, ty0, tx1, ty1, threads);
    std::atomic<int> next{ 0 };
    auto work = [&]() {
        for (int b; (b = next.fetch_add(1)) < bands;) {
            const int x0 = tx0 + cols * b / bands, x1 = tx0 + cols * (b + 1) / bands;
            paintRegion(w, g, out, x0, ty0, x1, ty1, aux, nullptr, realms, false, &df);
        }
    };
    std::vector<std::thread> pool;
    for (int t = 1; t < std::min(threads, bands); t++) pool.emplace_back(work);
    work();
    for (std::thread& t : pool) t.join();
    if (g.style == Style::Ink && aux) inkEdgePass(g, out, *aux, tx0, ty0, tx1, ty1, nullptr);
}

// ── hora do dia ──
namespace {
const double kSunElMax = 77.4;
std::array<double, 3> L0()
{
    const double l[3] = { -0.55, -0.75, 1.8 };
    const double n = jsHypot3(l[0], l[1], l[2]);
    return { l[0] / n, l[1] / n, l[2] / n };
}
}
Sun sunAt(double hour)
{
    const double th = M_PI * (hour - 6) / 12;
    const double el = kSunElMax * std::sin(th);
    const double e = std::max(0.0, el) * M_PI / 180;
    return { { std::cos(th) * std::cos(e), -std::sin(th) * std::cos(e), std::sin(e) }, el, smoothstep(-5, 3, el) };
}
SpriteSun spriteSun(double hour)
{
    const Sun s = sunAt(hour);
    const double w = s.sunW;
    const auto l0 = L0();
    double v[3] = { l0[0] + (s.L[0] - l0[0]) * w, l0[1] + (s.L[1] - l0[1]) * w, l0[2] + (s.L[2] - l0[2]) * w };
    double n = jsHypot3(v[0], v[1], v[2]); if (n == 0) n = 1;
    return { { v[0] / n, v[1] / n, v[2] / n }, w > 0.3 && s.L[0] > 0.12 };
}
std::vector<float> sunLUT(double hour, Style style)
{
    if (style == Style::Atlas || style == Style::Ink) return {};
    const Sun s = sunAt(hour);
    if (std::fabs(hour - kHourDefault) < 1e-3 || s.sunW <= 0) return {};
    const auto l0 = L0();
    const double u = clampd(1 - s.el / 62.6, 0, 1);
    const double D = 0.28 + 0.5 * u, A = 1.029 * (1 - 0.08 * u) - D * s.L[2], neg = 0.45 * u;
    const double a0 = style == Style::Rich ? 0.80 : 0.76;
    std::vector<float> lut(65536);
    for (int iy = 0; iy < 256; iy++) for (int ix = 0; ix < 256; ix++) {
        const double nx = (ix - 128) / 127.0, ny = (iy - 128) / 127.0, nz = std::sqrt(std::max(0.0, 1 - nx * nx - ny * ny));
        const double d0 = std::max(0.0, nx * l0[0] + ny * l0[1] + nz * l0[2]);
        const double d1 = nx * s.L[0] + ny * s.L[1] + nz * s.L[2];
        const double r = (A + D * (d1 >= 0 ? d1 : d1 * neg)) / (a0 + 0.28 * d0);
        lut[(iy << 8) | ix] = static_cast<float>(1 + (r - 1) * s.sunW);
    }
    return lut;
}
void relight(const uint8_t* src, const int8_t* nrm, const int16_t* depth, uint8_t* dst, int W,
             int x0, int y0, int x1, int y1, const std::vector<float>& lut)
{
    for (int y = y0; y < y1; y++) {
        long long p = static_cast<long long>(y) * W + x0;
        for (int x = x0; x < x1; x++, p++) {
            const long long o = p * 4;
            if (lut.empty() || depth[p] < 0) { dst[o] = src[o]; dst[o + 1] = src[o + 1]; dst[o + 2] = src[o + 2]; dst[o + 3] = src[o + 3]; continue; }
            const double r = lut[((nrm[2 * p + 1] + 128) << 8) | (nrm[2 * p] + 128)];
            dst[o] = u8c(src[o] * r); dst[o + 1] = u8c(src[o + 1] * r); dst[o + 2] = u8c(src[o + 2] * r); dst[o + 3] = src[o + 3];
        }
    }
}
namespace {
struct TodKey { double h; std::array<double, 3> mul; double night; std::array<double, 4> warm; };
const TodKey kTod[] = {
    { 0,    { 74, 90, 146 }, 1,    { 0, 0, 0, 0 } },
    { 4.6,  { 74, 90, 146 }, 1,    { 0, 0, 0, 0 } },
    { 5.4,  { 132, 122, 172 }, 0.8, { 255, 150, 150, 0.10 } },
    { 6.2,  { 238, 188, 178 }, 0.3, { 255, 168, 118, 0.22 } },
    { 7.4,  { 255, 238, 222 }, 0,   { 255, 204, 150, 0.07 } },
    { 9.0,  { 255, 255, 255 }, 0,   { 255, 220, 170, 0 } },
    { 16.0, { 255, 255, 255 }, 0,   { 255, 200, 140, 0 } },
    { 17.2, { 255, 238, 214 }, 0,   { 255, 190, 120, 0.10 } },
    { 18.1, { 248, 186, 160 }, 0.2, { 255, 140, 70, 0.26 } },
    { 18.9, { 140, 116, 160 }, 0.75, { 230, 110, 90, 0.12 } },
    { 19.8, { 74, 90, 146 }, 1,    { 0, 0, 0, 0 } },
    { 24,   { 74, 90, 146 }, 1,    { 0, 0, 0, 0 } },
};
}
TimeOfDay timeOfDay(double hour)
{
    const double h = std::fmod(std::fmod(hour, 24.0) + 24, 24.0);
    const int n = static_cast<int>(sizeof(kTod) / sizeof(kTod[0]));
    int k = 0; while (k < n - 2 && kTod[k + 1].h <= h) k++;
    const TodKey& a = kTod[k]; const TodKey& b = kTod[k + 1];
    const double t = smoothstep(0, 1, (h - a.h) / std::max(1e-6, b.h - a.h));
    TimeOfDay o;
    for (int i = 0; i < 3; i++) o.mul[i] = a.mul[i] + (b.mul[i] - a.mul[i]) * t;
    for (int i = 0; i < 4; i++) o.warm[i] = a.warm[i] + (b.warm[i] - a.warm[i]) * t;
    o.night = a.night + (b.night - a.night) * t;
    o.sun = sunAt(h);
    return o;
}
int hourPhase(double h)
{
    return (h < 4.6 || h >= 19.8) ? 0 : h < 6.2 ? 1 : h < 11.5 ? 2 : h < 13 ? 3 : h < 17.2 ? 4 : h < 18.9 ? 5 : 6;
}

// ── camada de clima ──
const ClimateKind kClimate[4] = {
    { "cold", { 214, 232, 250 } }, { "wet", { 62, 150, 84 } }, { "mild", { 196, 204, 104 } }, { "arid", { 232, 164, 84 } },
};
void paintClimate(World& w, const Geom& g, uint8_t* out, int tx0, int ty0, int tx1, int ty1)
{
    const int W = g.W, s = g.step, WW = w.W, WH = w.H;
    const std::vector<float>& wt = ensureTempWave(w);
    const double dux = g.ca * s, duy = -g.sa * s, dvx = g.sa * s, dvy = g.ca * s;
    const double bqx = g.ccx - g.cu * dux - g.cv * dvx, bqy = g.ccy - g.cu * duy - g.cv * dvy;
    const int x0 = std::max(0, tx0 - 1), y0 = std::max(0, ty0 - 1), x1 = std::min(W, tx1 + 1), y1 = std::min(g.H, ty1 + 1);
    const int bw = x1 - x0, bh = y1 - y0;
    std::vector<int8_t> ids(static_cast<size_t>(bw) * bh);
    for (int v = 0; v < bh; v++) {
        const int V = v + y0;
        double qx = bqx + x0 * dux + V * dvx, qy = bqy + x0 * duy + V * dvy;
        for (int u = 0; u < bw; u++, qx += dux, qy += duy) {
            if (qx < 0 || qy < 0 || qx > WW - 1 || qy > WH - 1) { ids[static_cast<size_t>(v) * bw + u] = -1; continue; }
            const int ix = std::min(WW - 2, toInt32(qx)), iy = std::min(WH - 2, toInt32(qy));
            const double fx = qx - ix, fy = qy - iy;
            const size_t j = static_cast<size_t>(iy) * WW + ix;
            const double w00 = (1 - fx) * (1 - fy), w10 = fx * (1 - fy), w01 = (1 - fx) * fy, w11 = fx * fy;
            const double m = w.m[j] * w00 + w.m[j + 1] * w10 + w.m[j + WW] * w01 + w.m[j + WW + 1] * w11;
            const double k = w.k[j] * w00 + w.k[j + 1] * w10 + w.k[j + WW] * w01 + w.k[j + WW + 1] * w11;
            const double tv = wt[j] * w00 + wt[j + 1] * w10 + wt[j + WW] * w01 + wt[j + WW + 1] * w11;
            ids[static_cast<size_t>(v) * bw + u] = static_cast<int8_t>(biomeId(0.15, latCold(w, qy, tv / 0.09), m, k + tv * 0.4));
        }
    }
    for (int y = ty0; y < ty1; y++) for (int x = tx0; x < tx1; x++) {
        const size_t i = static_cast<size_t>(y - y0) * bw + (x - x0);
        const int id = ids[i];
        const size_t o = (static_cast<size_t>(y) * W + x) * 4;
        if (id < 0) { out[o + 3] = 0; continue; }
        const RGB& c = kClimate[id].c;
        const bool edge = (x > x0 && ids[i - 1] != id) || (x < x1 - 1 && ids[i + 1] != id) ||
                          (y > y0 && ids[i - bw] != id) || (y < y1 - 1 && ids[i + bw] != id);
        const double kk = edge ? 0.55 : 1;
        out[o] = u8c(c[0] * kk); out[o + 1] = u8c(c[1] * kk); out[o + 2] = u8c(c[2] * kk);
        out[o + 3] = edge ? 210 : 118;
    }
}

// ── nuvens ──
Clouds clouds(const World& w, int seed, int ds)
{
    CARTO_PROF("nuvens");
    const int W = w.W, Hs = w.H;
    const double U = W / 1500.0, svgH = w.H / U;
    const int pad = padFor(w);
    ds = std::max(1, ds);
    const int FW = W + 2 * pad, FH = Hs + 2 * pad;        // em px do mundo
    const int CW = (FW + ds - 1) / ds, CH = (FH + ds - 1) / ds;   // na grade amostrada
    std::vector<float> A(static_cast<size_t>(CW) * CH, 0.0f);
    SeededRand rng(toUint32(static_cast<double>(seed) * 7919 + 4321));
    const double svgW = 1500, margin = 0.08;
    struct Blob { double dx, dy, rx, ry; };
    for (int ci = 0; ci < 9; ci++) {
        const int side = static_cast<int>(std::floor(rng.next() * 4));
        double cx, cy;
        if (side == 0)      { cx = svgW * (margin + rng.next() * (1 - 2 * margin)); cy = svgH * (rng.next() * 0.18); }
        else if (side == 1) { cx = svgW * (margin + rng.next() * (1 - 2 * margin)); cy = svgH * (0.82 + rng.next() * 0.18); }
        else if (side == 2) { cx = svgW * (rng.next() * 0.14); cy = svgH * (margin + rng.next() * (1 - 2 * margin)); }
        else                { cx = svgW * (0.86 + rng.next() * 0.14); cy = svgH * (margin + rng.next() * (1 - 2 * margin)); }
        const double sz = svgW * (0.035 + rng.next() * 0.07);
        const int nB = 5 + static_cast<int>(std::floor(rng.next() * 4));
        std::vector<Blob> blobs;
        for (int b = 0; b < nB; b++) {
            Blob bl;
            bl.dx = b == 0 ? 0 : (rng.next() - 0.5) * sz * 3.2;
            bl.dy = (rng.next() - 0.5) * sz * 0.6 / 0.65;
            bl.rx = sz * (0.7 + rng.next() * 0.6);
            bl.ry = sz * (0.32 + rng.next() * 0.28) / 0.65;
            blobs.push_back(bl);
        }
        const double op = 0.38 + rng.next() * 0.18;
        const double pcx = cx * U + pad, pcy = cy * U + pad;
        double bx0 = std::numeric_limits<double>::infinity(), by0 = bx0, bx1 = -bx0, by1 = -bx0;
        for (const Blob& b : blobs) {
            bx0 = std::min(bx0, b.dx - b.rx); bx1 = std::max(bx1, b.dx + b.rx);
            by0 = std::min(by0, b.dy - b.ry); by1 = std::max(by1, b.dy + b.ry);
        }
        const double disp = 30;
        const int X0 = static_cast<int>(clampd(std::floor((pcx + (bx0 - disp) * U) / ds), 0, CW - 1)), X1 = static_cast<int>(clampd(std::ceil((pcx + (bx1 + disp) * U) / ds), 0, CW - 1));
        const int Y0 = static_cast<int>(clampd(std::floor((pcy + (by0 - disp) * U) / ds), 0, CH - 1)), Y1 = static_cast<int>(clampd(std::ceil((pcy + (by1 + disp) * U) / ds), 0, CH - 1));
        // linhas independentes (cada pixel só é escrito pela própria linha): uma fatia por núcleo
        auto rows = [&](int ya, int yb) { for (int gy = ya; gy <= yb; gy++) for (int gx = X0; gx <= X1; gx++) {
            const int px = gx * ds, py = gy * ds;
            const double sx = (px - pcx) / U, sy = (py - pcy) / U;
            const double wx = sx + cx, wy = sy + cy;
            const double qx = sx + disp * 2 * (fbm(wx * 0.018, wy * 0.018, 3, seed + 23) - 0.5) * 1.6;
            const double qy = sy + disp * 2 * (fbm(wx * 0.018 + 31.7, wy * 0.018 + 17.3, 3, seed + 24) - 0.5) * 1.6 / 0.65;
            double q = std::numeric_limits<double>::infinity();
            for (const Blob& b : blobs) {
                const double ex = (qx - b.dx) / b.rx, ey = (qy - b.dy) / b.ry;
                const double v = ex * ex + ey * ey;
                if (v < q) q = v;
            }
            if (q >= 1) continue;
            double a = smoothstep(0, 0.55, 1 - q);
            a *= 1 - 0.30 * fbm(wx * 0.05, wy * 0.05 * 0.65, 3, seed + 7);
            a *= op;
            const size_t i = static_cast<size_t>(gy) * CW + gx;
            A[i] = static_cast<float>(a + A[i] * (1 - a));
        } };
        const int nt = std::max(1, std::min(static_cast<int>(std::thread::hardware_concurrency()), (Y1 - Y0 + 1) / 16));
        if (nt <= 1) rows(Y0, Y1);
        else {
            std::vector<std::thread> th;
            for (int t = 0; t < nt; t++) {
                const int a = Y0 + (Y1 - Y0 + 1) * t / nt, b = Y0 + (Y1 - Y0 + 1) * (t + 1) / nt - 1;
                if (a <= b) th.emplace_back(rows, a, b);
            }
            for (std::thread& t : th) t.join();
        }
    }
    Clouds c; c.W = CW; c.H = CH; c.pad = pad; c.ds = ds; c.rgba.assign(static_cast<size_t>(CW) * CH * 4, 0);
    for (size_t i = 0; i < A.size(); i++) {
        if (A[i] <= 0) continue;
        c.rgba[i * 4] = 245; c.rgba[i * 4 + 1] = 242; c.rgba[i * 4 + 2] = 236; c.rgba[i * 4 + 3] = u8c(A[i] * 255.0);
    }
    return c;
}

const RGB kOcean = { 26, 61, 88 };
std::vector<uint8_t> oceanTile(int size, int seed)
{
    std::vector<uint8_t> d(static_cast<size_t>(size) * size * 4);
    SeededRand rand(seed);
    for (int i = 0; i < size * size; i++) {
        const double r1 = rand.next(), r2 = rand.next();
        const double n = (r1 * 0.6 + r2 * 0.4) * 0.06 - 0.03;
        d[i * 4] = u8c(kOcean[0] + n * 255); d[i * 4 + 1] = u8c(kOcean[1] + n * 255); d[i * 4 + 2] = u8c(kOcean[2] + n * 255); d[i * 4 + 3] = 255;
    }
    return d;
}

} // namespace carto
