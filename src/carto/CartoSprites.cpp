#include "CartoSprites.h"

#include <QHash>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>
#include <mutex>

namespace carto::spr {

namespace {
using V3 = std::array<double, 3>;

QColor rgb(const RGB& c, double k = 1, double a = 1)
{
    auto ch = [k](double v) { return static_cast<int>(jsRound(clampd(v * k, 0, 255))); };
    return QColor(ch(c[0]), ch(c[1]), ch(c[2]), static_cast<int>(jsRound(clampd(a, 0, 1) * 255)));
}
RGB mix(const RGB& a, const RGB& b, double t) { return { a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t }; }
double frac(double x) { return x - std::floor(x); }
// xorshift32 (o mesmo do Lab)
struct Rng {
    uint32_t s;
    explicit Rng(double seed) { s = toUint32(seed); if (!s) s = 1; }
    double operator()() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s / 4294967296.0; }
};
// luz do noroeste: quanto uma direção no chão (ângulo a) olha pro sol
double facing(double a) { return 0.5 + 0.5 * (-std::cos(a) * 0.6 - std::sin(a) * 0.8); }

QPen pen(const QColor& c, double w, Qt::PenCapStyle cap = Qt::FlatCap)
{
    QPen p(c, w);
    p.setCapStyle(cap);
    p.setJoinStyle(cap == Qt::RoundCap ? Qt::RoundJoin : Qt::MiterJoin);
    return p;
}
void line(QPainter& p, QPointF a, QPointF b) { p.drawLine(a, b); }
}

// ── vegetação ──
namespace {
const double PPW[] = { 0.7, 1.05, 1.6, 2.4, 3.6, 5.4, 8.1, 12 };
constexpr int VARIANTS = 6;

FloraImg drawFlora(int type, double v, double pitchDeg, double ppw)
{
    const double th = pitchDeg * M_PI / 180, co = std::cos(th), si = std::sin(th);
    const double u = ppw * 2;                      // desenha em 2× e o carimbo reduz (traço macio)
    const int Wc = static_cast<int>(std::ceil(17 * u)), Hc = static_cast<int>(std::ceil(17 * u));
    QImage img(Wc, Hc, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter g(&img);
    g.setRenderHint(QPainter::Antialiasing);
    const double bx = Wc / 2.0, by = Hc * 0.8;      // ponto do chão (pé)
    g.translate(bx, by);
    Rng R(std::floor(v * 1e6) + type * 7919);
    // projeção: x no chão → x; profundidade no chão → y·sen θ; altura → −z·cos θ
    auto P = [&](double x, double d, double z) { return QPointF(x * u, d * si * u - z * co * u); };
    auto depthKey = [&](double d, double z) { return d * co + z * si; };
    auto lw = [&](double wd) { return std::max(0.8, wd * u); };
    auto shadow = [&](double rx, double off) {
        g.setPen(Qt::NoPen);
        g.setBrush(QColor(16, 22, 12, 43));
        g.drawEllipse(QPointF(off * u, off * 0.55 * si * u + 0.2 * u), rx * u, std::max(0.6, rx * si * u));
    };
    // tufo de folhas: esfera sombreada + folhagem salpicada (luz em cima à esquerda)
    auto lump = [&](double x, double d, double z, double r, const RGB& cl, const RGB& cd, int dots) {
        const QPointF c = P(x, d, z);
        QRadialGradient gr(c, r * 1.08 * u, QPointF(c.x() - r * 0.4 * u, c.y() - r * 0.45 * u), r * 0.08 * u);
        gr.setColorAt(0, rgb(cl, 1.08));
        gr.setColorAt(0.55, rgb(mix(cl, cd, 0.45)));
        gr.setColorAt(1, rgb(cd));
        g.setPen(Qt::NoPen);
        g.setBrush(gr);
        g.drawEllipse(c, r * u, r * u);
        if (!dots || u < 2.2) return;
        g.save();
        QPainterPath clip; clip.addEllipse(c, r * u, r * u);
        g.setClipPath(clip, Qt::IntersectClip);
        for (int k = 0; k < dots; k++) {
            const double a = R() * M_PI * 2, rr = std::sqrt(R()) * r * 0.95;
            const double dx = std::cos(a) * rr, dy = std::sin(a) * rr;
            const double lit = (-dx - dy) / (r * 1.4);                    // lado de cima/esquerda acende
            const double rad = (0.16 + R() * 0.16) * u;
            g.setBrush(lit > 0.1 ? rgb(cl, 1.22, 0.55) : rgb(cd, 0.8, 0.5));
            g.drawEllipse(QPointF(c.x() + dx * u, c.y() + dy * u), rad, rad);
        }
        g.restore();
    };
    auto trunk = [&](double z1, double wd, const RGB& c, double x1 = 0) {
        const QPointF a = P(0, 0, 0), b = P(x1, 0, z1);
        g.setPen(pen(rgb(c), lw(wd), Qt::RoundCap));
        g.drawLine(a, b);
        // raiz alargando no pé
        g.setPen(Qt::NoPen);
        g.setBrush(rgb(c, 0.85));
        g.drawEllipse(a, wd * 0.9 * u, std::max(0.5, wd * 0.35 * u * si));
    };
    static const RGB greens[6][2] = { { { 126, 158, 74 }, { 42, 74, 36 } }, { { 110, 148, 66 }, { 38, 68, 34 } }, { { 142, 160, 80 }, { 56, 80, 40 } },
                                      { { 100, 140, 72 }, { 34, 64, 38 } }, { { 132, 150, 70 }, { 60, 76, 34 } }, { { 116, 156, 86 }, { 40, 78, 44 } } };
    const int gi = std::min(VARIANTS - 1, static_cast<int>(std::floor(v * VARIANTS)));
    switch (type) {
    case Broad: {
        const RGB cl = greens[gi][0], cd = greens[gi][1];
        const int form = static_cast<int>(std::floor(v * 18)) % 3;
        const double zc = form == 2 ? 5.4 : form == 1 ? 4.0 : 4.8, Rr = form == 2 ? 1.75 : form == 1 ? 2.35 : 2.1;
        shadow(Rr * 1.1, 1.0);
        trunk(zc - Rr * 0.4, 0.5, { 92, 68, 46 });
        // galhos que entram na copa
        g.setPen(pen(rgb({ 84, 62, 42 }), lw(0.22), Qt::RoundCap));
        for (int s : { -1, 1 }) g.drawLine(P(0, 0, zc - Rr * 0.9), P(s * Rr * 0.6, 0, zc - Rr * 0.2));
        const int n = form == 2 ? 6 : 8;
        std::vector<std::array<double, 4>> lumps;
        for (int k = 0; k < n; k++) {
            const double a = double(k) / n * M_PI * 2 + R() * 0.6, e = -0.35 + R() * 1.0;
            const double rr = Rr * (0.5 + R() * 0.14);
            lumps.push_back({ std::cos(a) * Rr * 0.55 * std::cos(e), std::sin(a) * Rr * 0.55 * std::cos(e), zc + Rr * (form == 2 ? 0.75 : 0.45) * std::sin(e), rr });
        }
        lumps.push_back({ 0, 0, zc + Rr * (form == 2 ? 0.8 : 0.5), Rr * 0.62 });
        if (form == 2) lumps.push_back({ 0, 0, zc - Rr * 0.5, Rr * 0.55 });
        std::stable_sort(lumps.begin(), lumps.end(), [&](const auto& p, const auto& q) { return depthKey(p[1], p[2]) < depthKey(q[1], q[2]); });
        for (const auto& l : lumps) lump(l[0], l[1], l[2], l[3], cl, cd, 14);
        break;
    }
    case Pine: case SnowPine: {
        const bool snow = type == SnowPine;
        static const RGB cls[6] = { { 96, 142, 86 }, { 86, 130, 80 }, { 106, 144, 88 }, { 82, 124, 86 }, { 92, 136, 92 }, { 100, 132, 80 } };
        const RGB cl = cls[gi], cd = { 26, 56, 38 };
        shadow(2.0, 1.0); trunk(1.4, 0.42, { 86, 64, 44 });
        // 5 andares de galhos em leque (de cima, vira uma estrela de galhos)
        const int tiers = 5;
        for (int i = 0; i < tiers; i++) {
            const double q = double(i) / (tiers - 1);
            const double zb = 1.0 + q * 5.6, rb = 2.45 - q * 1.7, top = zb + 1.9 - q * 0.4;
            std::vector<double> fr;
            for (int j = 0; j < 9; j++) fr.push_back(j / 9.0 * M_PI * 2 + i * 0.37 + R() * 0.25);
            std::stable_sort(fr.begin(), fr.end(), [](double a, double b) { return std::sin(a) < std::sin(b); });   // de trás pra frente
            for (double a : fr) {
                const double ca = std::cos(a), sa = std::sin(a), px = -sa, pd = ca, w = rb * 0.42;
                const double tipD = rb * (0.92 + R() * 0.16);
                const QPointF B = P(0, 0, top), L = P(ca * tipD * 0.82 + px * w, sa * tipD * 0.82 + pd * w, zb + 0.05);
                const QPointF Tt = P(ca * tipD, sa * tipD, zb - 0.38), Rt = P(ca * tipD * 0.82 - px * w, sa * tipD * 0.82 - pd * w, zb + 0.05);
                const double lf = facing(a);
                QPainterPath path; path.moveTo(B); path.lineTo(L); path.lineTo(Tt); path.lineTo(Rt); path.closeSubpath();
                g.setPen(Qt::NoPen); g.setBrush(rgb(mix(cd, cl, 0.35 + 0.65 * lf)));
                g.drawPath(path);
                // nervura escura no meio do galho
                g.setPen(pen(rgb(cd, 0.9, 0.35), lw(0.12), Qt::RoundCap));
                g.drawLine(B, Tt);
                if (snow) {
                    auto M = [&](QPointF pp, double k) { return QPointF(B.x() + (pp.x() - B.x()) * k, B.y() + (pp.y() - B.y()) * k); };
                    QPainterPath sp; sp.moveTo(B); sp.lineTo(M(L, 0.7)); sp.lineTo(M(Tt, 0.62)); sp.lineTo(M(Rt, 0.7)); sp.closeSubpath();
                    g.setPen(Qt::NoPen); g.setBrush(rgb(mix({ 196, 208, 226 }, { 252, 253, 255 }, lf)));
                    g.drawPath(sp);
                }
            }
        }
        g.setPen(pen(rgb(snow ? RGB{ 240, 244, 250 } : cl), lw(0.3), Qt::RoundCap));
        g.drawLine(P(0, 0, 7.2), P(0, 0, 8.6));
        break;
    }
    case Palm: {
        shadow(2.3, 1.3);
        const double lean = (v - 0.5) * 2.4;
        const QPointF p0 = P(0, 0, 0), cc = P(lean * 0.3 - 0.4, 0, 3.6), tt = P(lean, 0, 6.6);
        QPainterPath tr; tr.moveTo(p0); tr.quadTo(cc, tt);
        g.setBrush(Qt::NoBrush);
        g.setPen(pen(rgb({ 140, 108, 72 }), lw(0.62), Qt::RoundCap));
        g.drawPath(tr);
        // anéis do tronco
        g.setPen(pen(rgb({ 96, 72, 48 }, 1, 0.7), lw(0.12), Qt::RoundCap));
        for (int k = 1; k < 9; k++) {
            const double t = k / 9.0;
            const double X = (1 - t) * (1 - t) * p0.x() + 2 * (1 - t) * t * cc.x() + t * t * tt.x(), Y = (1 - t) * (1 - t) * p0.y() + 2 * (1 - t) * t * cc.y() + t * t * tt.y();
            g.drawLine(QPointF(X - 0.32 * u, Y), QPointF(X + 0.32 * u, Y));
        }
        std::vector<double> fr;
        for (int k = 0; k < 8; k++) fr.push_back(k / 8.0 * M_PI * 2 + v * 3);
        std::stable_sort(fr.begin(), fr.end(), [](double a, double b) { return std::sin(a) < std::sin(b); });
        for (double a : fr) {
            const double L = 3.4, ca = std::cos(a), sa = std::sin(a);
            const QPointF M = P(lean + ca * L * 0.5, sa * L * 0.5, 6.6 + 0.9), E = P(lean + ca * L, sa * L, 6.6 - 1.3);
            const RGB col = mix({ 50, 98, 46 }, { 104, 160, 70 }, facing(a));
            QPainterPath fp; fp.moveTo(tt); fp.quadTo(M, E);
            g.setPen(pen(rgb(col), lw(0.28), Qt::RoundCap));
            g.drawPath(fp);
            // folíolos dos dois lados da nervura
            g.setPen(pen(rgb(col), lw(0.16), Qt::RoundCap));
            for (int k = 1; k <= 7; k++) {
                const double t = k / 8.0;
                const double X = (1 - t) * (1 - t) * tt.x() + 2 * (1 - t) * t * M.x() + t * t * E.x(), Y = (1 - t) * (1 - t) * tt.y() + 2 * (1 - t) * t * M.y() + t * t * E.y();
                const double dX = 2 * (1 - t) * (M.x() - tt.x()) + 2 * t * (E.x() - M.x()), dY = 2 * (1 - t) * (M.y() - tt.y()) + 2 * t * (E.y() - M.y());
                double dl = std::hypot(dX, dY); if (!dl) dl = 1;
                const double nx = -dY / dl, ny = dX / dl, len = (1 - t * 0.6) * 0.75 * u;
                g.drawLine(QPointF(X, Y), QPointF(X + nx * len + dX / dl * len * 0.5, Y + ny * len + dY / dl * len * 0.5));
                g.drawLine(QPointF(X, Y), QPointF(X - nx * len + dX / dl * len * 0.5, Y - ny * len + dY / dl * len * 0.5));
            }
        }
        g.setPen(Qt::NoPen); g.setBrush(rgb({ 104, 72, 40 }));
        for (int k = 0; k < 3; k++) g.drawEllipse(QPointF(tt.x() + (k - 1) * 0.32 * u, tt.y() + 0.3 * u), 0.24 * u, 0.24 * u);
        break;
    }
    case Cactus: {
        shadow(1.2, 0.7);
        const RGB cl = { 130, 168, 94 }, cd = { 54, 96, 54 };
        auto body = [&](double x, double z0, double z1, double w) {
            const QPointF a = P(x, 0, z0), b = P(x, 0, z1);
            QLinearGradient gr(QPointF(a.x() - w * u, 0), QPointF(a.x() + w * u, 0));
            gr.setColorAt(0, rgb(cl)); gr.setColorAt(1, rgb(cd));
            QPen pp(QBrush(gr), lw(w * 2)); pp.setCapStyle(Qt::RoundCap);
            g.setPen(pp);
            g.drawLine(a, b);
            // gomos e espinhos
            g.setPen(pen(rgb(cl, 1.15, 0.6), lw(0.08), Qt::RoundCap));
            for (double o : { -0.45, 0.1 }) g.drawLine(QPointF(a.x() + o * w * u, a.y()), QPointF(b.x() + o * w * u, b.y()));
            g.setPen(Qt::NoPen);
            g.setBrush(QColor(250, 244, 220, 204));
            for (int k = 0; k < 6; k++) {
                const double t = R();
                const double sx = a.x() + (R() < 0.5 ? -w : w * 0.85) * u;
                const double s = std::max(0.6, 0.1 * u);
                g.drawRect(QRectF(sx, a.y() + (b.y() - a.y()) * t, s, s));
            }
        };
        body(0, 0.2, 3.8, 0.5);
        auto arm = [&](double side, double z, double h2) {
            QPainterPath ap; ap.moveTo(P(0, 0, z)); ap.lineTo(P(side * 0.95, 0, z)); ap.lineTo(P(side * 0.95, 0, z + h2));
            g.setBrush(Qt::NoBrush);
            g.setPen(pen(rgb(mix(cl, cd, 0.35)), lw(0.62), Qt::RoundCap));
            g.drawPath(ap);
        };
        arm(-1, 1.6, 1.1);
        if (v > 0.35) arm(1, 2.1, 0.9);
        if (v > 0.7) { g.setPen(Qt::NoPen); g.setBrush(rgb({ 236, 120, 140 })); g.drawEllipse(P(0, 0, 3.95), 0.3 * u, 0.3 * u); }
        break;
    }
    case Shrub: {
        const bool dry = v < 0.6;
        const RGB cl = dry ? RGB{ 170, 160, 94 } : RGB{ 122, 152, 78 }, cd = dry ? RGB{ 96, 90, 50 } : RGB{ 50, 80, 42 };
        shadow(1.6, 0.6);
        std::vector<std::array<double, 4>> lumps;
        for (int k = 0; k < 6; k++) {
            const double a = k / 6.0 * M_PI * 2 + R();
            const double z = 0.7 + R() * 0.4, r = 0.62 + R() * 0.2;
            lumps.push_back({ std::cos(a) * 0.7, std::sin(a) * 0.6, z, r });
        }
        lumps.push_back({ 0, 0, 1.25, 0.8 });
        std::stable_sort(lumps.begin(), lumps.end(), [&](const auto& p, const auto& q) { return depthKey(p[1], p[2]) < depthKey(q[1], q[2]); });
        for (const auto& l : lumps) lump(l[0], l[1], l[2], l[3], cl, cd, 6);
        if (!dry && v > 0.8) {
            g.setPen(Qt::NoPen); g.setBrush(rgb({ 236, 210, 120 }));
            for (int k = 0; k < 5; k++) {
                const double x = (R() - 0.5) * 1.6, d = (R() - 0.5) * 1.2, z = 1.0 + R() * 0.6;
                g.drawEllipse(P(x, d, z), 0.16 * u, 0.16 * u);
            }
        }
        break;
    }
    case Rock: {
        shadow(1.8, 0.6);
        // pedra facetada: anel de base, anel de topo, faces laterais com luz própria
        const int n = 7;
        std::vector<std::array<double, 4>> base;
        std::vector<V3> top;
        const double hr = 1.0 + R() * 0.7;
        for (int k = 0; k < n; k++) {
            const double a = double(k) / n * M_PI * 2 + R() * 0.3, rr = 1.3 * (0.75 + 0.5 * R());
            base.push_back({ std::cos(a) * rr, std::sin(a) * rr, 0, a });
            const double t1 = 0.45 + R() * 0.2;
            const double t2 = 0.8 + R() * 0.3;
            top.push_back({ std::cos(a + 0.2) * rr * t1, std::sin(a + 0.2) * rr * 0.55, hr * t2 });
        }
        struct Side { V3 pts[4]; double am; };
        std::vector<Side> sides;
        for (int k = 0; k < n; k++) {
            const auto& a0 = base[k]; const auto& a1 = base[(k + 1) % n];
            const double am = (a0[3] + (k == n - 1 ? a1[3] + M_PI * 2 : a1[3])) / 2;
            sides.push_back({ { { a0[0], a0[1], 0 }, { a1[0], a1[1], 0 }, top[(k + 1) % n], top[k] }, am });
        }
        std::stable_sort(sides.begin(), sides.end(), [](const Side& p, const Side& q) { return std::sin(p.am) < std::sin(q.am); });
        static const RGB stones[3] = { { 168, 162, 152 }, { 150, 146, 140 }, { 176, 166, 150 } };
        const RGB stone = stones[static_cast<int>(std::floor(v * 3)) % 3];
        for (const Side& sd : sides) {
            if (std::sin(sd.am) * co + 0.4 * si < -0.1 && si < 0.95) continue;
            QPainterPath path;
            for (int i = 0; i < 4; i++) { const QPointF q = P(sd.pts[i][0], sd.pts[i][1], sd.pts[i][2]); i ? path.lineTo(q) : path.moveTo(q); }
            path.closeSubpath();
            g.setBrush(rgb(stone, 0.55 + 0.5 * facing(sd.am)));
            g.setPen(pen(rgb(stone, 0.4, 0.5), lw(0.06)));
            g.drawPath(path);
        }
        QPainterPath tp;
        for (int i = 0; i < n; i++) { const QPointF q = P(top[i][0], top[i][1], top[i][2]); i ? tp.lineTo(q) : tp.moveTo(q); }
        tp.closeSubpath();
        g.setBrush(rgb(stone, 1.12)); g.setPen(pen(rgb(stone, 0.45, 0.5), lw(0.06)));
        g.drawPath(tp);
        break;
    }
    case Reed: {
        for (int k = 0; k < 9; k++) {
            const double x = (k - 4) * 0.26, d = ((k * 7) % 3 - 1) * 0.25, hh = 2.0 + 1.3 * R(), lean = (R() - 0.5) * 1.0;
            const QPointF a = P(x, d, 0), m = P(x + lean * 0.4, d, hh * 0.6), b = P(x + lean, d, hh);
            QPainterPath rp; rp.moveTo(a); rp.quadTo(m, b);
            g.setBrush(Qt::NoBrush);
            g.setPen(pen(rgb(mix({ 96, 116, 58 }, { 150, 156, 84 }, R())), lw(0.16), Qt::RoundCap));
            g.drawPath(rp);
            if (k % 2 == 0) {     // taboa
                g.setPen(Qt::NoPen); g.setBrush(rgb({ 104, 74, 44 }));
                g.drawEllipse(P(x + lean * 0.8, d, hh * 0.82), 0.17 * u, std::max(0.6, 0.5 * u * std::max(0.35, co)));
            }
        }
        break;
    }
    case Dead: {
        shadow(1.4, 0.9);
        const RGB c = { 112, 100, 88 };
        trunk(5.2, 0.5, c);
        std::function<void(double, double, double, double, double, double, int)> branch = [&](double x, double d, double z, double a, double len, double w, int lvl) {
            const double ex = x + std::cos(a) * len, ed = d + std::sin(a) * len, ez = z + len * 0.75;
            g.setPen(pen(rgb(c, 0.95 - lvl * 0.05), lw(w), Qt::RoundCap));
            g.drawLine(P(x, d, z), P(ex, ed, ez));
            if (lvl < 2) { branch(ex, ed, ez, a - 0.6, len * 0.6, w * 0.6, lvl + 1); branch(ex, ed, ez, a + 0.5, len * 0.55, w * 0.6, lvl + 1); }
        };
        for (int k = 0; k < 4; k++) branch(0, 0, 2.6 + k * 0.6, k / 4.0 * M_PI * 2 + v * 4, 1.5, 0.3, 0);
        break;
    }
    }
    g.end();
    return { img, bx, by, ppw };
}
}

const FloraImg& floraImage(int type, double v, double pitchDeg, double ppw, bool mirror)
{
    static QHash<QString, FloraImg> cache;
    int b = -1;
    for (int i = 0; i < 8; i++) if (PPW[i] >= ppw) { b = i; break; }
    if (b < 0) b = 7;
    const int pb = static_cast<int>(jsRound(std::max(20.0, std::min(90.0, pitchDeg)) / 5)) * 5;
    const int vb = std::min(VARIANTS - 1, static_cast<int>(std::floor(v * VARIANTS)));
    const QString key = QStringLiteral("%1|%2|%3|%4%5").arg(type).arg(vb).arg(pb).arg(b).arg(mirror ? QStringLiteral("|m") : QString());
    auto it = cache.find(key);
    if (it != cache.end()) return it.value();
    FloraImg img;
    if (mirror) {
        const FloraImg& o = floraImage(type, v, pitchDeg, ppw, false);
        img.img = o.img.mirrored(true, false);
        img.ax = o.img.width() - o.ax; img.ay = o.ay; img.ppw = o.ppw;
    } else img = drawFlora(type, (vb + 0.5) / VARIANTS, pb, PPW[b]);
    if (cache.size() > 1500) cache.clear();
    return cache.insert(key, img).value();
}

const QImage& glowImage(const QString& kind)
{
    static QHash<QString, QImage> cache;
    auto it = cache.find(kind);
    if (it != cache.end()) return it.value();
    RGB col = { 255, 196, 112 };
    if (kind == QLatin1String("torch")) col = { 255, 150, 64 };
    else if (kind == QLatin1String("beacon")) col = { 255, 244, 210 };
    else if (kind == QLatin1String("lantern")) col = { 255, 206, 140 };
    const int n = 64;
    QImage img(n, n, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter g(&img);
    QRadialGradient gr(QPointF(n / 2.0, n / 2.0), n / 2.0);
    gr.setColorAt(0, QColor(255, 250, 236, 255));
    gr.setColorAt(0.12, rgb(col, 1, 0.95));
    gr.setColorAt(0.4, rgb(col, 1, 0.32));
    gr.setColorAt(1, rgb(col, 1, 0));
    g.fillRect(img.rect(), gr);
    g.end();
    return cache.insert(kind, img).value();
}

// ── sólidos ──
namespace {
const V3 LIGHT0 = [] { const V3 l = { -0.55, -0.75, 1.8 }; const double n = std::sqrt(l[0] * l[0] + l[1] * l[1] + l[2] * l[2]); return V3{ l[0] / n, l[1] / n, l[2] / n }; }();
V3 LIGHT = LIGHT0;

using PtFn = std::function<QPointF(double, double)>;
using Deco = std::function<void(QPainter&, const PtFn&, double)>;
struct Face {
    std::vector<V3> v;
    RGB col{};
    bool hasC = false;
    V3 c{};
    double bias = 0;
    bool flat = false;
    std::array<int, 4> q = { 0, 1, 2, 3 };
    Deco deco;
};
using Faces = std::vector<Face>;
Face face(std::vector<V3> v, const RGB& col, const V3& c, Deco deco = {}, double bias = 0)
{
    Face f; f.v = std::move(v); f.col = col; f.hasC = true; f.c = c; f.deco = std::move(deco); f.bias = bias;
    return f;
}

void drawFaces(QPainter& p, const Faces& faces, const Cam& cam)
{
    const double ca = std::cos(cam.ang), sa = std::sin(cam.ang);
    struct It { const Face* f; double k, depth; };
    std::vector<It> list;
    list.reserve(faces.size());
    for (const Face& f : faces) {
        const V3& a = f.v[0]; const V3& b = f.v[1]; const V3& c = f.v.back();
        double nx = (b[1] - a[1]) * (c[2] - a[2]) - (b[2] - a[2]) * (c[1] - a[1]);
        double ny = (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2]);
        double nz = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
        V3 m = { 0, 0, 0 };
        for (const V3& q : f.v) { m[0] += q[0]; m[1] += q[1]; m[2] += q[2]; }
        m[0] /= f.v.size(); m[1] /= f.v.size(); m[2] /= f.v.size();
        if (f.hasC && nx * (m[0] - f.c[0]) + ny * (m[1] - f.c[1]) + nz * (m[2] - f.c[2]) < 0) { nx = -nx; ny = -ny; nz = -nz; }
        double nl = std::sqrt(nx * nx + ny * ny + nz * nz); if (!nl) nl = 1;
        nx /= nl; ny /= nl; nz /= nl;
        const double px = ca * nx - sa * ny, py = sa * nx + ca * ny;
        if (!f.flat && py * cam.co + nz * cam.si <= 0.0005) continue;
        const double lit = std::max(0.0, px * LIGHT[0] + py * LIGHT[1] + nz * LIGHT[2]);
        const double mpy = sa * m[0] + ca * m[1];
        list.push_back({ &f, f.flat ? 1 : 0.6 + 0.5 * lit, mpy * cam.co + m[2] * cam.si + f.bias });
    }
    std::stable_sort(list.begin(), list.end(), [](const It& a, const It& b) { return a.depth < b.depth; });
    std::vector<QPointF> S;
    for (const It& it : list) {
        const Face& f = *it.f;
        S.clear();
        for (const V3& q : f.v) S.push_back(cam.proj(q[0], q[1], q[2]));
        QPainterPath path;
        path.moveTo(S[0]);
        for (size_t i = 1; i < S.size(); i++) path.lineTo(S[i]);
        path.closeSubpath();
        p.setBrush(rgb(f.col, it.k));
        if (cam.edges) p.setPen(pen(rgb(f.col, it.k * 0.55, 0.5), 0.6));
        else p.setPen(Qt::NoPen);
        p.drawPath(path);
        if (f.deco && cam.detail) {
            const QPointF P0 = S[f.q[0]], P1 = S[f.q[1]], P2 = S[f.q[2]], P3 = S[f.q[3]];
            const PtFn pt = [P0, P1, P2, P3](double s, double t) {
                const double bx = P0.x() + (P1.x() - P0.x()) * s, by = P0.y() + (P1.y() - P0.y()) * s;
                const double tx = P3.x() + (P2.x() - P3.x()) * s, ty = P3.y() + (P2.y() - P3.y()) * s;
                return QPointF(bx + (tx - bx) * t, by + (ty - by) * t);
            };
            p.save();
            f.deco(p, pt, it.k);
            p.restore();
        }
    }
}
void quad(QPainter& p, const PtFn& pt, double s0, double s1, double t0, double t1, const QColor& fill)
{
    QPainterPath path;
    path.moveTo(pt(s0, t0)); path.lineTo(pt(s1, t0)); path.lineTo(pt(s1, t1)); path.lineTo(pt(s0, t1)); path.closeSubpath();
    p.fillPath(path, fill);
}

// caixa (paredes + tampa) centrada em (cx, cy), girada por yaw
Faces boxFaces(double cx, double cy, double yaw, double w, double d, double z0, double z1, const RGB& col, const RGB* topCol = nullptr,
               const std::function<Deco(int, double)>& deco = {})
{
    const double c = std::cos(yaw), s = std::sin(yaw);
    auto L = [&](double lx, double ly) { return std::array<double, 2>{ cx + c * lx - s * ly, cy + s * lx + c * ly }; };
    const double hw = w / 2, hd = d / 2;
    const std::array<double, 2> pp[4] = { L(-hw, -hd), L(hw, -hd), L(hw, hd), L(-hw, hd) };
    Faces faces;
    const V3 ctr = { cx, cy, (z0 + z1) / 2 };
    for (int k = 0; k < 4; k++) {
        const auto& a = pp[k]; const auto& b = pp[(k + 1) % 4];
        faces.push_back(face({ { a[0], a[1], z0 }, { b[0], b[1], z0 }, { b[0], b[1], z1 }, { a[0], a[1], z1 } }, col, ctr,
                             deco ? deco(k, k % 2 ? d : w) : Deco()));
    }
    faces.push_back(face({ { pp[0][0], pp[0][1], z1 }, { pp[1][0], pp[1][1], z1 }, { pp[2][0], pp[2][1], z1 }, { pp[3][0], pp[3][1], z1 } },
                         topCol ? *topCol : col, ctr));
    return faces;
}
void append(Faces& a, Faces b) { for (Face& f : b) a.push_back(std::move(f)); }

const RGB ROOFS[5] = { { 172, 86, 60 }, { 150, 72, 54 }, { 94, 100, 114 }, { 126, 94, 66 }, { 160, 108, 64 } };
const RGB WALLS[5] = { { 224, 212, 186 }, { 206, 188, 158 }, { 182, 152, 114 }, { 228, 222, 206 }, { 198, 178, 150 } };
const RGB DARK = { 44, 36, 30 };

// janelas (algumas acesas), porta, enxaimel
Deco wallDeco(double len, int rows, bool door, bool timber)
{
    return [len, rows, door, timber](QPainter& p, const PtFn& pt, double k) {
        const int n = std::max(1, static_cast<int>(std::floor(len / 1.5)));
        auto lit = [&](int i, int r) { return frac(std::sin((i + 1) * 12.9 + r * 7.7 + len * 3.1) * 999) > 0.72; };
        for (int r = 0; r < rows; r++) {
            const double t0 = rows == 1 ? 0.42 : r ? 0.62 : 0.2, t1 = t0 + (rows == 1 ? 0.3 : 0.2);
            for (int i = 0; i < n; i++) {
                const double sc = (i + 0.5) / n;
                if (door && r == 0 && std::fabs(sc - 0.5) < 0.5 / n) continue;
                quad(p, pt, sc - 0.09, sc + 0.09, t0, t1, lit(i, r) ? QColor(255, 214, 120, 242) : rgb(DARK, k, 0.85));
            }
        }
        if (door) quad(p, pt, 0.43, 0.57, 0, rows == 1 ? 0.62 : 0.34, rgb({ 92, 62, 40 }, k));
        if (timber) {
            p.setPen(pen(rgb({ 78, 56, 38 }, k, 0.9), 1));
            line(p, pt(0, 0.98), pt(1, 0.98)); line(p, pt(0, 0.04), pt(1, 0.04));
            if (rows == 2) line(p, pt(0, 0.5), pt(1, 0.5));
            line(p, pt(0.02, 0), pt(0.02, 1)); line(p, pt(0.98, 0), pt(0.98, 1));
            line(p, pt(0.02, 0.04), pt(0.22, 0.5)); line(p, pt(0.98, 0.04), pt(0.78, 0.5));
        }
    };
}
// telha: fileiras paralelas ao beiral + cumeeira
void roofDeco(QPainter& p, const PtFn& pt, double)
{
    p.setPen(pen(QColor(40, 24, 18, 71), 0.8));
    for (int i = 1; i < 6; i++) line(p, pt(0, i / 6.0), pt(1, i / 6.0));
    p.setPen(pen(QColor(30, 18, 14, 128), 0.8));
    line(p, pt(0, 1), pt(1, 1));
}
Deco slitDeco(int n)
{
    return [n](QPainter& p, const PtFn& pt, double k) {
        for (int i = 0; i < n; i++) { const double s = (i + 0.5) / n; quad(p, pt, s - 0.025, s + 0.025, 0.45, 0.7, rgb(DARK, k, 0.85)); }
    };
}
void gateDeco(QPainter& p, const PtFn& pt, double k)
{
    QPainterPath path;
    path.moveTo(pt(0.4, 0)); path.lineTo(pt(0.4, 0.5));
    for (int i = 0; i <= 8; i++) { const double a = M_PI - i / 8.0 * M_PI; path.lineTo(pt(0.5 + std::cos(a) * 0.1, 0.5 + std::sin(a) * 0.18)); }
    path.lineTo(pt(0.6, 0)); path.closeSubpath();
    p.fillPath(path, rgb({ 58, 40, 28 }, k));
    p.setPen(pen(QColor(20, 14, 10, 153), 0.7));
    for (int i = 1; i < 4; i++) line(p, pt(0.4 + i * 0.05, 0), pt(0.4 + i * 0.05, 0.6));
}
void merlons(Faces& faces, double ax, double ay, double bx, double by, double z, double thick, const RGB& col)
{
    const double len = std::hypot(bx - ax, by - ay);
    const int n = std::max(2, static_cast<int>(std::floor(len / 1.25)));
    const double yaw = std::atan2(by - ay, bx - ax);
    for (int k = 0; k < n; k++) {
        const double t = (k + 0.5) / n, x = ax + (bx - ax) * t, y = ay + (by - ay) * t;
        for (Face& f : boxFaces(x, y, yaw, 0.62, thick, z, z + 0.7, col)) { f.bias = 0.05; faces.push_back(std::move(f)); }
    }
}
Faces towerFaces(double x, double y, double r, double z1, double roofH, const RGB& col, const RGB& roofCol, bool noRoof)
{
    const int n = 8;
    std::array<double, 2> ring[8];
    for (int k = 0; k < n; k++) { const double a = double(k) / n * M_PI * 2; ring[k] = { x + std::cos(a) * r, y + std::sin(a) * r }; }
    Faces faces;
    for (int k = 0; k < n; k++) {
        const auto& a = ring[k]; const auto& b = ring[(k + 1) % n];
        faces.push_back(face({ { a[0], a[1], 0 }, { b[0], b[1], 0 }, { b[0], b[1], z1 }, { a[0], a[1], z1 } }, col, { x, y, z1 / 2 }, k % 2 ? slitDeco(1) : Deco()));
        // ameias no topo da torre
        const double mx = (a[0] + b[0]) / 2, my = (a[1] + b[1]) / 2;
        for (Face& f : boxFaces(mx, my, std::atan2(b[1] - a[1], b[0] - a[0]), 0.55, 0.45, z1, z1 + 0.6, col)) { f.bias = 0.04; faces.push_back(std::move(f)); }
        if (noRoof) continue;
        // flecha (teto cônico) mais fina que a torre, nascendo de dentro das ameias
        const double rax = x + (a[0] - x) * 0.82, ray = y + (a[1] - y) * 0.82, rbx = x + (b[0] - x) * 0.82, rby = y + (b[1] - y) * 0.82;
        Face f = face({ { rax, ray, z1 + 0.3 }, { rbx, rby, z1 + 0.3 }, { x, y, z1 + 0.3 + roofH } }, roofCol, { x, y, z1 },
                      [](QPainter& p, const PtFn& pt, double) { p.setPen(pen(QColor(30, 16, 12, 77), 0.7)); for (int i = 1; i < 4; i++) line(p, pt(0, i / 4.0), pt(1, i / 4.0)); }, 0.06);
        f.q = { 0, 1, 2, 2 };
        faces.push_back(std::move(f));
    }
    return faces;
}
}

void setSun(const std::array<double, 3>& L) { LIGHT = L; }
void resetSun() { LIGHT = LIGHT0; }

// quatro tamanhos: 0 pequeno · 1 médio (torres no meio das muralhas) · 2 grande (muralha dupla) · 3 colossal
const CastleCfg kCastle[4] = {
    { 5.6, 3.6, false, 1.6, 5.8, 4.2, 7.4, { 1.15, 9.4, 3.0 }, false, false, 0, 0, 0, 0 },
    { 9.0, 4.4, true, 2.0, 7.0, 5.6, 9.4, { 1.5, 12.2, 3.6 }, true, false, 0, 0, 0, 0 },
    { 14.0, 5.2, true, 2.4, 8.4, 7.0, 11.0, { 1.8, 15.6, 4.4 }, true, true, 8.0, 6.4, 1.9, 9.0 },
    { 23.0, 6.6, true, 3.0, 10.4, 10.0, 14.0, { 2.6, 21.0, 5.6 }, true, true, 13.0, 8.2, 2.4, 11.6 },
};

void drawHouse(QPainter& p, double x, double y, double yaw, double v, int floors, bool timber, const Cam& cam, bool snowy)
{
    const double c = std::cos(yaw), s = std::sin(yaw);
    auto L = [&](double lx, double ly, double z) { return V3{ x + c * lx - s * ly, y + s * lx + c * ly, z }; };
    const bool two = floors == 2;
    const double w = (two ? 4.6 : 4.0) + v * 1.6, d = 3.0, wh = two ? 3.8 : 2.2, rh = 1.8;
    const double hw = w / 2, hd = d / 2;
    const RGB wall = WALLS[static_cast<int>(std::floor(v * 5)) % 5];
    const RGB roof = snowy ? RGB{ 236, 240, 246 } : ROOFS[static_cast<int>(std::floor(v * 9)) % 5];
    const V3 A = L(-hw, -hd, 0), B = L(hw, -hd, 0), Cc = L(hw, hd, 0), D = L(-hw, hd, 0);
    auto up = [](const V3& q, double z) { return V3{ q[0], q[1], z }; };
    const V3 R1 = L(-hw, 0, wh + rh), R2 = L(hw, 0, wh + rh);
    const int rows = two ? 2 : 1;
    const V3 center = { x, y, wh / 2 };
    Faces faces;
    faces.push_back(face({ A, B, up(B, wh), up(A, wh) }, wall, center, wallDeco(w, rows, true, timber)));
    faces.push_back(face({ Cc, D, up(D, wh), up(Cc, wh) }, wall, center, wallDeco(w, rows, false, timber)));
    Face e1 = face({ B, Cc, up(Cc, wh), R2, up(B, wh) }, wall, center, wallDeco(d, rows, false, timber)); e1.q = { 0, 1, 2, 4 }; faces.push_back(e1);
    Face e2 = face({ D, A, up(A, wh), R1, up(D, wh) }, wall, center, wallDeco(d, rows, false, timber)); e2.q = { 0, 1, 2, 4 }; faces.push_back(e2);
    faces.push_back(face({ up(A, wh - 0.05), up(B, wh - 0.05), R2, R1 }, roof, center, roofDeco, 0.01));
    faces.push_back(face({ up(Cc, wh - 0.05), up(D, wh - 0.05), R1, R2 }, roof, center, roofDeco, 0.01));
    // chaminé no telhado
    const V3 ch = L(hw * 0.45, hd * 0.32, 0);
    const RGB chTop = { 70, 56, 48 };
    for (Face& f : boxFaces(ch[0], ch[1], yaw, 0.6, 0.6, wh + rh * 0.45, wh + rh + 0.7, { 150, 120, 100 }, &chTop)) { f.bias = 0.03; faces.push_back(std::move(f)); }
    drawFaces(p, faces, cam);
}

namespace {
void wallRing(Faces& out, const std::function<std::array<double, 2>(double, double)>& L, double R, double wallH, double towerR, double towerH,
              bool mid, const RGB& stone, const RGB& roof, bool gate)
{
    const std::array<double, 2> corners[4] = { L(-R, -R), L(R, -R), L(R, R), L(-R, R) };
    std::vector<std::array<double, 2>> pts;
    for (int k = 0; k < 4; k++) {
        pts.push_back(corners[k]);
        if (mid) { const auto& q = corners[(k + 1) % 4]; pts.push_back({ (corners[k][0] + q[0]) / 2, (corners[k][1] + q[1]) / 2 }); }
    }
    static const RGB wallTop = { 180, 170, 154 };
    for (size_t k = 0; k < pts.size(); k++) {
        const auto& a = pts[k]; const auto& q = pts[(k + 1) % pts.size()];
        const double mx = (a[0] + q[0]) / 2, my = (a[1] + q[1]) / 2;
        const double len = std::hypot(q[0] - a[0], q[1] - a[1]), yaw = std::atan2(q[1] - a[1], q[0] - a[0]);
        const bool hasGate = gate && k == 0;
        for (Face& f : boxFaces(mx, my, yaw, len, 1.2, 0, wallH, stone, &wallTop, [hasGate, len](int side, double) -> Deco {
                 if (side != 0 && side != 2) return Deco();
                 if (hasGate) return gateDeco;
                 return slitDeco(std::max(2, static_cast<int>(jsRound(len / 2.2))));
             })) { f.c = { mx, my, wallH / 2 }; out.push_back(std::move(f)); }
        merlons(out, a[0], a[1], q[0], q[1], wallH, 1.2, stone);
    }
    for (const auto& pt : pts) append(out, towerFaces(pt[0], pt[1], towerR, towerH, 2.6 * towerR / 1.6, stone, roof, false));
}
}

void drawCastle(QPainter& p, double x, double y, double yaw, int size, const Cam& cam, const RGB& realmCol)
{
    const CastleCfg& cfg = kCastle[std::max(0, std::min(3, size))];
    const RGB stone = { 198, 188, 170 }, keepStone = { 208, 198, 180 };
    const RGB roof = { realmCol[0] * 0.7, realmCol[1] * 0.7, realmCol[2] * 0.7 };
    const double c = std::cos(yaw), s = std::sin(yaw);
    auto L = [&](double lx, double ly) { return std::array<double, 2>{ x + c * lx - s * ly, y + s * lx + c * ly }; };
    Faces faces;
    wallRing(faces, L, cfg.R, cfg.wallH, cfg.towerR, cfg.towerH, cfg.mid, stone, roof, true);
    if (cfg.hasInner) wallRing(faces, L, cfg.iR, cfg.iWallH, cfg.iTowerR, cfg.iTowerH, false, keepStone, roof, true);
    const auto kc0 = L(0, 0);
    const double kx = kc0[0], ky = kc0[1], K = cfg.keep / 2;
    static const RGB keepTop = { 186, 176, 160 };
    const int slits = cfg.keep > 5 ? 3 : 2;
    for (Face& f : boxFaces(kx, ky, yaw, cfg.keep, cfg.keep, 0, cfg.keepH, keepStone, &keepTop, [slits](int, double) { return slitDeco(slits); })) {
        f.c = { kx, ky, cfg.keepH / 2 }; faces.push_back(std::move(f));
    }
    const std::array<double, 2> kc[4] = { L(-K, -K), L(K, -K), L(K, K), L(-K, K) };
    for (int k = 0; k < 4; k++) merlons(faces, kc[k][0], kc[k][1], kc[(k + 1) % 4][0], kc[(k + 1) % 4][1], cfg.keepH, 0.5, keepStone);
    // torreões nos cantos da menagem (médio e grande)
    if (cfg.turrets) for (const auto& q : kc) append(faces, towerFaces(q[0], q[1], 0.9, cfg.keepH + 2.2, 2.0, keepStone, roof, false));
    append(faces, towerFaces(kx, ky, cfg.kt[0], cfg.kt[1], cfg.kt[2], keepStone, roof, false));
    drawFaces(p, faces, cam);
    const double fz = cfg.kt[1] + cfg.kt[2] + 0.2;
    const QPointF f0 = cam.proj(kx, ky, fz - 0.6), t = cam.proj(kx, ky, fz + 2.2);
    p.setPen(pen(QColor(40, 32, 26), 1));
    p.drawLine(f0, t);
    const double k = std::max(3.0, std::hypot(t.x() - f0.x(), t.y() - f0.y()) * 0.55);
    QPainterPath fl;
    fl.moveTo(t);
    fl.quadTo(QPointF(t.x() + k * 0.7, t.y() - k * 0.1), QPointF(t.x() + k * 1.4, t.y() + k * 0.3));
    fl.quadTo(QPointF(t.x() + k * 0.7, t.y() + k * 0.55), QPointF(t.x(), t.y() + k * 0.75));
    fl.closeSubpath();
    p.setBrush(rgb(realmCol));
    p.setPen(pen(rgb(realmCol, 0.6), 0.6));
    p.drawPath(fl);
}

// ── ponte: deck de tábuas com guarda-corpo, sobre pilares ──
void drawBridge(QPainter& p, const Path& pts, const Cam& cam)
{
    const double deck = 2.1, half = 1.05;
    const RGB wood = { 170, 140, 104 }, post = { 120, 94, 66 };
    Faces faces;
    for (size_t k = 0; k + 1 < pts.size(); k++) {
        const double ax = pts[k][0], ay = pts[k][1], bx = pts[k + 1][0], by = pts[k + 1][1];
        double L = std::hypot(bx - ax, by - ay); if (!L) L = 1;
        const double nx = -(by - ay) / L * half, ny = (bx - ax) / L * half;
        const V3 c = { (ax + bx) / 2, (ay + by) / 2, deck - 0.4 };
        const int planks = std::max(1, static_cast<int>(jsRound(L / 0.7)));
        faces.push_back(face({ { ax + nx, ay + ny, deck }, { bx + nx, by + ny, deck }, { bx - nx, by - ny, deck }, { ax - nx, ay - ny, deck } }, wood, c,
                             [planks](QPainter& q, const PtFn& pt, double) {
                                 q.setPen(pen(QColor(60, 40, 24, 102), 0.6));
                                 for (int i = 1; i < planks; i++) line(q, pt(double(i) / planks, 0), pt(double(i) / planks, 1));
                             }));
        for (int sgn : { 1, -1 }) {
            const double ox = nx * sgn, oy = ny * sgn;
            faces.push_back(face({ { ax + ox, ay + oy, deck - 0.8 }, { bx + ox, by + oy, deck - 0.8 }, { bx + ox, by + oy, deck }, { ax + ox, ay + oy, deck } }, { 150, 124, 92 }, c));
            // guarda-corpo: corrimão fino com um poste por trecho
            faces.push_back(face({ { ax + ox, ay + oy, deck + 0.75 }, { bx + ox, by + oy, deck + 0.75 }, { bx + ox, by + oy, deck + 0.95 }, { ax + ox, ay + oy, deck + 0.95 } }, post, c,
                                 [post](QPainter& q, const PtFn& pt, double) { q.setPen(pen(rgb(post, 0.8), 0.9)); line(q, pt(0, -3.5), pt(0, 0)); }, 0.05));
        }
    }
    double acc = 0;
    for (size_t k = 1; k + 1 < pts.size(); k++) {
        acc += std::hypot(pts[k][0] - pts[k - 1][0], pts[k][1] - pts[k - 1][1]);
        if (acc < 4.5) continue;
        acc = 0;
        const double x = pts[k][0], y = pts[k][1], yaw = std::atan2(pts[k + 1][1] - pts[k - 1][1], pts[k + 1][0] - pts[k - 1][0]);
        for (Face& f : boxFaces(x, y, yaw, 0.8, 2.6, -0.6, deck - 0.8, { 112, 90, 66 })) { f.c = { x, y, 0.6 }; faces.push_back(std::move(f)); }
    }
    drawFaces(p, faces, cam);
}

// ── píer e barco ──
void drawDock(QPainter& p, const Dock& dk, const Cam& cam)
{
    const double yaw = std::atan2(dk.y1 - dk.y0, dk.x1 - dk.x0), len = std::hypot(dk.x1 - dk.x0, dk.y1 - dk.y0);
    const double mx = (dk.x0 + dk.x1) / 2, my = (dk.y0 + dk.y1) / 2;
    const RGB top = { 176, 146, 108 };
    Faces faces = boxFaces(mx, my, yaw, len, 1.5, 0.3, 0.75, { 146, 118, 86 }, &top);
    const int planks = static_cast<int>(jsRound(len / 0.7));
    faces[4].deco = [planks](QPainter& q, const PtFn& pt, double) {
        q.setPen(pen(QColor(60, 40, 24, 102), 0.6));
        for (int i = 1; i < planks; i++) line(q, pt(double(i) / planks, 0), pt(double(i) / planks, 1));
    };
    drawFaces(p, faces, cam);
}

void drawBoat(QPainter& p, const Boat& b, const Cam& cam)
{
    const double c = std::cos(b.yaw), s = std::sin(b.yaw);
    auto L = [&](double lx, double ly, double z) { return V3{ b.x + c * lx - s * ly, b.y + s * lx + c * ly, z }; };
    static const RGB hulls[3] = { { 120, 84, 54 }, { 96, 70, 50 }, { 140, 98, 60 } };
    const RGB hull = hulls[static_cast<int>(std::floor(b.v * 3)) % 3];
    const double len = 3.4, w = 1.1, hz = 0.75;
    const V3 bow = L(len / 2 + 0.7, 0, hz), st0 = L(-len / 2, -w / 2, hz), st1 = L(-len / 2, w / 2, hz);
    const V3 md0 = L(len / 2, -w / 2, hz), md1 = L(len / 2, w / 2, hz);
    const V3 kb = L(len / 2 + 0.3, 0, 0.1), ks0 = L(-len / 2 + 0.3, -w * 0.3, 0.1), ks1 = L(-len / 2 + 0.3, w * 0.3, 0.1);
    const V3 km0 = L(len / 2 - 0.2, -w * 0.3, 0.1), km1 = L(len / 2 - 0.2, w * 0.3, 0.1);
    const V3 center = { b.x, b.y, 0.4 };
    Faces faces;
    faces.push_back(face({ st0, md0, bow, md1, st1 }, { 168, 136, 98 }, { b.x, b.y, 0 }));   // convés
    faces.push_back(face({ ks0, km0, md0, st0 }, hull, center));
    faces.push_back(face({ km0, kb, bow, md0 }, hull, center));
    faces.push_back(face({ ks1, km1, md1, st1 }, hull, center));
    faces.push_back(face({ km1, kb, bow, md1 }, hull, center));
    faces.push_back(face({ ks0, ks1, st1, st0 }, hull, center));
    drawFaces(p, faces, cam);
    // mastro e vela (a vela é um plano vertical ao longo do barco)
    const V3 m0 = L(0.2, 0, hz), m1 = L(0.2, 0, hz + 4.2);
    p.setPen(pen(QColor(70, 52, 36), 1));
    p.drawLine(cam.proj(m0[0], m0[1], m0[2]), cam.proj(m1[0], m1[1], m1[2]));
    const V3 sail[3] = { L(0.2, 0, hz + 4.0), L(0.2, 0, hz + 1.1), L(-1.6, 0, hz + 1.1) };
    QPainterPath sp;
    for (int i = 0; i < 3; i++) { const QPointF q = cam.proj(sail[i][0], sail[i][1], sail[i][2]); i ? sp.lineTo(q) : sp.moveTo(q); }
    sp.closeSubpath();
    p.setBrush(b.v > 0.5 ? QColor(244, 236, 216) : QColor(232, 214, 186));
    p.setPen(pen(QColor(80, 64, 48, 153), 0.6));
    p.drawPath(sp);
}

// ── plantação: retalho no chão com sulcos ──
void drawField(QPainter& p, const Field& f, const Cam& cam)
{
    static const RGB CROPS[4] = { { 196, 176, 110 }, { 128, 148, 82 }, { 140, 110, 80 }, { 168, 172, 102 } };
    const double c = std::cos(f.yaw), s = std::sin(f.yaw);
    auto L = [&](double lx, double ly) { return V3{ f.x + c * lx - s * ly, f.y + s * lx + c * ly, 0.05 }; };
    const double hw = f.w / 2, hd = f.d / 2;
    const RGB col = CROPS[std::max(0, std::min(3, f.crop))];
    const int rows = std::max(3, static_cast<int>(jsRound(f.d / 0.55)));
    const int crop = f.crop;
    Face fc;
    fc.v = { L(-hw, -hd), L(hw, -hd), L(hw, hd), L(-hw, hd) };
    fc.col = { col[0] * 0.94, col[1] * 0.94, col[2] * 0.94 };
    fc.flat = true;
    fc.deco = [rows, crop](QPainter& q, const PtFn& pt, double) {
        q.setPen(pen(crop == 2 ? QColor(80, 56, 36, 115) : QColor(70, 80, 30, 77), 0.7));
        for (int i = 1; i < rows; i++) line(q, pt(0, double(i) / rows), pt(1, double(i) / rows));
        // sebe (cerca viva) em volta do retalho
        QPainterPath h; h.moveTo(pt(0, 0)); h.lineTo(pt(1, 0)); h.lineTo(pt(1, 1)); h.lineTo(pt(0, 1)); h.closeSubpath();
        q.setBrush(Qt::NoBrush);
        q.setPen(pen(QColor(52, 78, 40, 191), 1.6));
        q.drawPath(h);
    };
    Cam c2 = cam; c2.detail = true;
    drawFaces(p, Faces{ fc }, c2);
}

// ── artefatos únicos ──
namespace {
Faces taperFaces(double x, double y, double r0, double r1, double z0, double z1, int n, const RGB& col, const std::function<Deco(int)>& deco = {})
{
    Faces faces;
    for (int k = 0; k < n; k++) {
        const double a0 = double(k) / n * M_PI * 2, a1 = double(k + 1) / n * M_PI * 2;
        faces.push_back(face({ { x + std::cos(a0) * r0, y + std::sin(a0) * r0, z0 }, { x + std::cos(a1) * r0, y + std::sin(a1) * r0, z0 },
                               { x + std::cos(a1) * r1, y + std::sin(a1) * r1, z1 }, { x + std::cos(a0) * r1, y + std::sin(a0) * r1, z1 } },
                             col, { x, y, (z0 + z1) / 2 }, deco ? deco(k) : Deco()));
    }
    return faces;
}
Faces coneFaces(double x, double y, double r, double z0, double h, int n, const RGB& col)
{
    Faces faces;
    for (int k = 0; k < n; k++) {
        const double a0 = double(k) / n * M_PI * 2, a1 = double(k + 1) / n * M_PI * 2;
        faces.push_back(face({ { x + std::cos(a0) * r, y + std::sin(a0) * r, z0 }, { x + std::cos(a1) * r, y + std::sin(a1) * r, z0 }, { x, y, z0 + h } }, col, { x, y, z0 }, Deco(), 0.02));
    }
    return faces;
}
void flag(QPainter& p, const Cam& cam, double x, double y, double z, const RGB& col)
{
    const QPointF f0 = cam.proj(x, y, z), t = cam.proj(x, y, z + 2.4);
    p.setPen(pen(QColor(40, 32, 26), 1));
    p.drawLine(f0, t);
    const double k = std::max(3.0, std::hypot(t.x() - f0.x(), t.y() - f0.y()) * 0.5);
    QPainterPath fl;
    fl.moveTo(t);
    fl.quadTo(QPointF(t.x() + k * 0.7, t.y() - k * 0.1), QPointF(t.x() + k * 1.4, t.y() + k * 0.3));
    fl.quadTo(QPointF(t.x() + k * 0.7, t.y() + k * 0.55), QPointF(t.x(), t.y() + k * 0.75));
    fl.closeSubpath();
    p.fillPath(fl, rgb(col));
}
}

// torre de vigia: base quadrada alta, seteiras, ameias, bandeira do território
void drawTowerArt(QPainter& p, const Art& b, const Cam& cam, const RGB& realmCol)
{
    const RGB stone = { 190, 180, 162 }, top = { 170, 160, 144 };
    Faces faces = boxFaces(b.x, b.y, b.yaw, 2.8, 2.8, 0, 8.2, stone, &top, [](int, double) { return slitDeco(1); });
    const double c = std::cos(b.yaw), s = std::sin(b.yaw);
    auto L = [&](double lx, double ly) { return std::array<double, 2>{ b.x + c * lx - s * ly, b.y + s * lx + c * ly }; };
    const std::array<double, 2> k[4] = { L(-1.4, -1.4), L(1.4, -1.4), L(1.4, 1.4), L(-1.4, 1.4) };
    for (int i = 0; i < 4; i++) merlons(faces, k[i][0], k[i][1], k[(i + 1) % 4][0], k[(i + 1) % 4][1], 8.2, 0.5, stone);
    drawFaces(p, faces, cam);
    flag(p, cam, b.x, b.y, 8.6, realmCol);
}

// farol: tronco listrado, lanterna acesa com brilho
void drawLighthouse(QPainter& p, const Art& b, const Cam& cam)
{
    const RGB white = { 236, 232, 222 }, red = { 184, 64, 52 };
    Faces faces = taperFaces(b.x, b.y, 1.6, 1.05, 0, 9, 10, white, [red](int) -> Deco {
        return [red](QPainter& q, const PtFn& pt, double kk) { for (auto band : { std::array<double, 2>{ 0.22, 0.38 }, std::array<double, 2>{ 0.62, 0.78 } }) quad(q, pt, 0, 1, band[0], band[1], rgb(red, kk)); };
    });
    append(faces, taperFaces(b.x, b.y, 1.25, 1.25, 9, 9.25, 10, { 70, 60, 54 }));
    append(faces, taperFaces(b.x, b.y, 0.85, 0.85, 9.25, 10.6, 8, { 255, 214, 120 }));
    append(faces, coneFaces(b.x, b.y, 1.05, 10.6, 1.3, 8, { 150, 48, 40 }));
    drawFaces(p, faces, cam);
    const QPointF gc = cam.proj(b.x, b.y, 9.9), up = cam.proj(b.x, b.y, 12.9);
    const double R = std::max(6.0, std::hypot(up.x() - gc.x(), up.y() - gc.y()) * 1.4);
    QRadialGradient gr(gc, R);
    gr.setColorAt(0, QColor(255, 226, 150, 140));
    gr.setColorAt(1, QColor(255, 226, 150, 0));
    p.setPen(Qt::NoPen); p.setBrush(gr);
    p.drawEllipse(gc, R, R);
}

// moinho: torre caiada, teto cônico e quatro pás de treliça viradas pro lado de "yaw"
void drawWindmill(QPainter& p, const Art& b, const Cam& cam)
{
    Faces faces = taperFaces(b.x, b.y, 1.8, 1.25, 0, 5.6, 10, { 228, 220, 200 }, [](int k) { return k % 5 == 2 ? slitDeco(1) : Deco(); });
    append(faces, coneFaces(b.x, b.y, 1.45, 5.6, 2.0, 10, { 132, 92, 60 }));
    drawFaces(p, faces, cam);
    const double dx = std::cos(b.yaw), dy = std::sin(b.yaw);
    const V3 hub = { b.x + dx * 1.5, b.y + dy * 1.5, 5.8 }, side = { -dy, dx, 0 }, upv = { 0, 0, 1 };
    auto at = [&](const V3& q, double a, double len, double w) {
        const V3 bd = { side[0] * std::cos(a) + upv[0] * std::sin(a), side[1] * std::cos(a) + upv[1] * std::sin(a), side[2] * std::cos(a) + upv[2] * std::sin(a) };
        const V3 pd = { -side[0] * std::sin(a) + upv[0] * std::cos(a), -side[1] * std::sin(a) + upv[1] * std::cos(a), -side[2] * std::sin(a) + upv[2] * std::cos(a) };
        return V3{ q[0] + bd[0] * len + pd[0] * w, q[1] + bd[1] * len + pd[1] * w, q[2] + bd[2] * len + pd[2] * w };
    };
    Faces blades;
    const QPointF hp = cam.proj(hub[0], hub[1], hub[2]);
    for (int i = 0; i < 4; i++) {
        const double a = i * M_PI / 2 + M_PI / 4 + b.v;
        Face f;
        f.v = { at(hub, a, 0.5, 0.05), at(hub, a, 3.6, 0.05), at(hub, a, 3.6, 0.75), at(hub, a, 0.9, 0.75) };
        f.col = { 214, 200, 170 }; f.flat = true; f.hasC = true; f.c = hub;
        f.deco = [](QPainter& q, const PtFn& pt, double) {
            q.setPen(pen(QColor(70, 52, 36, 191), 0.7));
            for (int j = 0; j <= 4; j++) line(q, pt(j / 4.0, 0), pt(j / 4.0, 1));
            line(q, pt(0, 0.5), pt(1, 0.5));
        };
        blades.push_back(f);
        const V3 tip = at(hub, a, 3.6, 0);
        p.setPen(pen(QColor(84, 62, 42), 1.2));
        p.drawLine(hp, cam.proj(tip[0], tip[1], tip[2]));
    }
    Cam c2 = cam; c2.detail = true;
    drawFaces(p, blades, c2);
    p.setPen(Qt::NoPen); p.setBrush(QColor(70, 52, 36));
    p.drawEllipse(hp, 1.6, 1.6);
}

// templo: plataforma, colunata, frontão
void drawTemple(QPainter& p, const Art& b, const Cam& cam)
{
    const RGB marble = { 230, 226, 214 };
    const double c = std::cos(b.yaw), s = std::sin(b.yaw);
    auto L = [&](double lx, double ly) { return std::array<double, 2>{ b.x + c * lx - s * ly, b.y + s * lx + c * ly }; };
    Faces faces = boxFaces(b.x, b.y, b.yaw, 7.2, 5.2, 0, 0.6, { 206, 200, 186 });
    auto column = [&](double lx, double ly) {
        const auto q = L(lx, ly);
        for (Face& f : boxFaces(q[0], q[1], b.yaw, 0.55, 0.55, 0.6, 3.6, marble)) { f.c = { q[0], q[1], 2 }; faces.push_back(std::move(f)); }
    };
    for (double lx : { -2.9, -1.45, 0.0, 1.45, 2.9 }) for (double ly : { -2.0, 2.0 }) column(lx, ly);
    for (double ly : { -0.7, 0.7 }) column(-2.9, ly);
    for (Face& f : boxFaces(b.x, b.y, b.yaw, 7.0, 5.0, 3.6, 4.1, { 220, 214, 200 })) { f.c = { b.x, b.y, 3.8 }; f.bias = 0.02; faces.push_back(std::move(f)); }
    auto up = [](const std::array<double, 2>& q, double z) { return V3{ q[0], q[1], z }; };
    const auto A = L(-3.5, -2.5), B = L(3.5, -2.5), Cc = L(3.5, 2.5), D = L(-3.5, 2.5);
    const V3 R1 = up(L(-3.5, 0), 5.5), R2 = up(L(3.5, 0), 5.5);
    const RGB roof = { 200, 108, 76 };
    const V3 ctr = { b.x, b.y, 4.6 };
    faces.push_back(face({ up(A, 4.1), up(B, 4.1), R2, R1 }, roof, ctr, roofDeco, 0.03));
    faces.push_back(face({ up(Cc, 4.1), up(D, 4.1), R1, R2 }, roof, ctr, roofDeco, 0.03));
    faces.push_back(face({ up(B, 4.1), up(Cc, 4.1), R2 }, marble, ctr, Deco(), 0.03));
    faces.push_back(face({ up(D, 4.1), up(A, 4.1), R1 }, marble, ctr, Deco(), 0.03));
    drawFaces(p, faces, cam);
}

// ruínas: muros quebrados de alturas diferentes, um toco de torre e pedras soltas
void drawRuins(QPainter& p, const Art& b, const Cam& cam)
{
    const RGB stone = { 168, 160, 146 };
    const double c = std::cos(b.yaw), s = std::sin(b.yaw);
    auto L = [&](double lx, double ly) { return std::array<double, 2>{ b.x + c * lx - s * ly, b.y + s * lx + c * ly }; };
    Rng R(std::floor(b.v * 1e6) + 17);
    Faces faces;
    const double sides[4][4] = { { -4, -3, 4, -3 }, { 4, -3, 4, 3 }, { 4, 3, -4, 3 }, { -4, 3, -4, -3 } };
    for (const auto& sd : sides) {
        const int n = 4;
        for (int k = 0; k < n; k++) {
            if (R() < 0.3) continue;                                   // trecho caído
            const double t = (k + 0.5) / n;
            const auto q = L(sd[0] + (sd[2] - sd[0]) * t, sd[1] + (sd[3] - sd[1]) * t);
            const double len = std::hypot(sd[2] - sd[0], sd[3] - sd[1]) / n * (0.6 + R() * 0.35), yaw = b.yaw + std::atan2(sd[3] - sd[1], sd[2] - sd[0]);
            const double hgt = 0.6 + R() * 2.6;
            for (Face& f : boxFaces(q[0], q[1], yaw, len, 0.9, 0, hgt, stone)) { f.c = { q[0], q[1], 1 }; faces.push_back(std::move(f)); }
        }
    }
    const auto tp = L(-4, -3);
    const double th = 3.2 + R() * 1.4;
    for (Face& f : towerFaces(tp[0], tp[1], 1.3, th, 0, stone, stone, true)) if (f.v[0][2] < 0.01 || R() < 0.6) faces.push_back(std::move(f));
    for (int k = 0; k < 6; k++) {
        const double lx = (R() - 0.5) * 9, ly = (R() - 0.5) * 7;
        const auto q = L(lx, ly);
        const double yaw = R() * 3;
        for (Face& f : boxFaces(q[0], q[1], yaw, 0.7, 0.6, 0, 0.45, { 150, 144, 132 })) { f.c = { q[0], q[1], 0.2 }; faces.push_back(std::move(f)); }
    }
    drawFaces(p, faces, cam);
}

// muralha: um trecho entre dois pontos (cada trecho com a câmera erguida pro seu chão)
void drawWallSeg(QPainter& p, const Pt& a, const Pt& b, const Cam& cam, double k)
{
    const RGB stone = { 196, 186, 168 }, top = { 178, 168, 152 };
    const double mx = (a[0] + b[0]) / 2, my = (a[1] + b[1]) / 2, len = std::hypot(b[0] - a[0], b[1] - a[1]) + 0.25, yaw = std::atan2(b[1] - a[1], b[0] - a[0]);
    Faces faces = boxFaces(mx, my, yaw, len, 1.1 * k, -1.6 * k, 3.4 * k, stone, &top);
    for (Face& f : boxFaces(mx, my, yaw, 1.05 * k, 1.1 * k, 3.4 * k, 4.1 * k, stone)) { f.bias = 0.05; faces.push_back(std::move(f)); }
    drawFaces(p, faces, cam);
}
void drawWallTower(QPainter& p, const Pt& at, const Cam& cam, double k)
{
    drawFaces(p, towerFaces(at[0], at[1], 1.45 * k, 5.0 * k, 0, { 192, 182, 164 }, { 0, 0, 0 }, true), cam);
}

// Atlas: símbolos chapados, no estilo de mapa de livro
void atlasGlyph(QPainter& p, int type, double X, double Y, double k)
{
    p.setPen(pen(QColor(52, 60, 40, 217), 1));
    if (type == Pine || type == SnowPine) {
        QPainterPath t; t.moveTo(X, Y - 6 * k); t.lineTo(X + 2.6 * k, Y); t.lineTo(X - 2.6 * k, Y); t.closeSubpath();
        p.setBrush(type == SnowPine ? QColor(220, 228, 226, 242) : QColor(120, 146, 100, 242));
        p.drawPath(t);
    } else if (type == Broad || type == Palm) {
        p.drawLine(QPointF(X, Y), QPointF(X, Y - 2 * k));
        p.setBrush(QColor(132, 158, 104, 242));
        p.drawEllipse(QPointF(X, Y - 3.6 * k), 2.2 * k, 2.2 * k);
    } else if (type == Cactus || type == Shrub) {
        p.setBrush(QColor(170, 160, 110, 230));
        p.drawEllipse(QPointF(X, Y - 1.2 * k), 1.3 * k, 1.3 * k);
    }
}

// Tinta: os símbolos do mapa de livro, em nanquim sobre papel (o pé do símbolo fica em X, Y)
void inkGlyph(QPainter& p, int type, double X, double Y, double k, double v)
{
    const QColor INKC(40, 33, 28), INKH(40, 33, 28, 140), PAPERC(240, 231, 208);
    const double lw = std::max(0.7, 0.72 * k);
    const QPen inkPen = pen(INKC, lw, Qt::RoundCap), halfPen = pen(INKH, lw, Qt::RoundCap);
    p.setPen(inkPen);
    p.setBrush(PAPERC);
    auto poly = [&](std::initializer_list<QPointF> pts) {
        QPainterPath path; bool first = true;
        for (const QPointF& q : pts) { if (first) path.moveTo(q); else path.lineTo(q); first = false; }
        p.setBrush(Qt::NoBrush); p.drawPath(path); p.setBrush(PAPERC);
    };
    if (type == Broad) {
        // copa redonda com sombra em meia-lua embaixo à direita; as copas se encavalam e viram mata
        const double r = 2.3 * k * (0.85 + 0.3 * v), cy = Y - r * 1.2;
        p.drawEllipse(QPointF(X, cy), r, r);
        p.setPen(halfPen); p.setBrush(Qt::NoBrush);
        const double rr = r * 0.66, cx2 = X - r * 0.12, cy2 = cy - r * 0.1;
        QPainterPath arc;
        arc.arcMoveTo(QRectF(cx2 - rr, cy2 - rr, 2 * rr, 2 * rr), -0.05 * 180);
        arc.arcTo(QRectF(cx2 - rr, cy2 - rr, 2 * rr, 2 * rr), -0.05 * 180, -(0.62 - 0.05) * 180);
        p.drawPath(arc);
        p.setPen(inkPen); p.setBrush(PAPERC);
        poly({ QPointF(X, cy + r), QPointF(X, Y + 0.5 * k) });
    } else if (type == Pine || type == SnowPine) {
        const double hh = (5.8 + 1.2 * v) * k, w = 1.9 * k, b = Y - hh * 0.16;
        QPainterPath t; t.moveTo(X, Y - hh); t.lineTo(X + w, b); t.lineTo(X - w, b); t.closeSubpath();
        p.drawPath(t);
        if (type == Pine) {
            QPainterPath h; h.moveTo(X, Y - hh); h.lineTo(X + w, b); h.lineTo(X, b); h.closeSubpath();
            p.fillPath(h, INKH);
        }
        poly({ QPointF(X, b), QPointF(X, Y + 0.5 * k) });
    } else if (type == Palm) {
        const double tx = X + 1.2 * k, ty = Y - 5.4 * k;
        p.setBrush(Qt::NoBrush);
        QPainterPath tr; tr.moveTo(X, Y); tr.quadTo(X + 0.2 * k, Y - 3 * k, tx, ty);
        p.drawPath(tr);
        for (double a : { -2.7, -2.0, -1.1, -0.4 }) {
            QPainterPath f; f.moveTo(tx, ty);
            f.quadTo(tx + std::cos(a) * 1.8 * k, ty + std::sin(a) * 1.8 * k - 0.6 * k, tx + std::cos(a) * 3 * k, ty + std::sin(a) * 1.6 * k + 1.1 * k);
            p.drawPath(f);
        }
    } else if (type == Cactus) {
        poly({ QPointF(X, Y), QPointF(X, Y - 4.6 * k) });
        poly({ QPointF(X, Y - 2 * k), QPointF(X - 1.4 * k, Y - 2 * k), QPointF(X - 1.4 * k, Y - 3.4 * k) });
        poly({ QPointF(X, Y - 2.8 * k), QPointF(X + 1.3 * k, Y - 2.8 * k), QPointF(X + 1.3 * k, Y - 3.9 * k) });
    } else if (type == Shrub) {
        p.setBrush(Qt::NoBrush);
        const double r = 1.5 * k;
        QPainterPath a; a.arcMoveTo(QRectF(X - r, Y - r, 2 * r, 2 * r), 180); a.arcTo(QRectF(X - r, Y - r, 2 * r, 2 * r), 180, -180);
        p.drawPath(a);
    } else if (type == Rock) {
        const double s = (1.4 + 0.8 * v) * k;
        QPainterPath r; r.moveTo(X - s * 1.2, Y); r.lineTo(X - s * 0.3, Y - s * 1.1); r.lineTo(X + s * 0.5, Y - s * 0.7); r.lineTo(X + s * 1.1, Y); r.closeSubpath();
        p.drawPath(r);
        poly({ QPointF(X - s * 0.3, Y - s * 1.1), QPointF(X + s * 0.1, Y) });
    } else if (type == Reed) {
        poly({ QPointF(X - 2 * k, Y), QPointF(X + 2 * k, Y) });
        for (double dx : { -1.1, 0.0, 1.1 }) poly({ QPointF(X + dx * k, Y), QPointF(X + dx * k * 1.4, Y - (dx != 0 ? 2.2 : 3) * k) });
    } else if (type == Dead) {
        poly({ QPointF(X, Y), QPointF(X, Y - 4.2 * k) });
        poly({ QPointF(X, Y - 2.4 * k), QPointF(X - 1.6 * k, Y - 4 * k) });
        poly({ QPointF(X, Y - 3.1 * k), QPointF(X + 1.4 * k, Y - 4.6 * k) });
    }
}

} // namespace carto::spr
