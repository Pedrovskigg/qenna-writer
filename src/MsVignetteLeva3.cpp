#include "MsVignetteLeva3.h"

#include "MsCanvas.h"
#include "MsVignette.h"
#include "MsVignetteCore.h"
#include "MsGuitarOutlines.h"

#include <QPointF>
#include <QVector>
#include <cmath>
#include <functional>

// Port do concept: cada "case" do JS vira um "case" aqui, com os mesmos números e na mesma ordem de sorteio.
// Atenção ao portar: em C++ a ordem de avaliação dos argumentos não é garantida, então toda chamada que
// sorteia mais de um valor guarda cada sorteio numa variável antes (no JS a ordem é da esquerda pra direita).

using namespace MsVignetteDetail;

namespace {

constexpr double PI = M_PI;

struct SubR {
    Rng r;
    double operator()(double a, double b) { return r.R(a, b); }
};

using Pts = QVector<QPointF>;

void drawFamily(MsCanvas& g, int fam, const QString& id, Rng& rnd, double grow, double VW, double VH, double s, const Pal& P, bool rect, int words) {
    const double CX = VW / 2, CY = VH / 2, wide = VW / 104;
    const QColor ink = P.ink, ink2 = P.ink2, faint = P.faint;
    auto R = [&](double a, double b) { return rnd.R(a, b); };
    // sorteio próprio de cada peça: a vela 3 é sempre a mesma vela, apareça quando aparecer
    auto sub = [&](const QString& k) { return SubR{ Rng{ hashStr(id + QLatin1Char('|') + k) } }; };
    auto dot = [&](double x, double y, double r, const QBrush& c) {
        g.fillStyle = c; g.beginPath(); g.arc(x, y, std::max(.01, r), 0, 7); g.fill();
    };
    auto line = [&](double x1, double y1, double x2, double y2, const QBrush& c, double w) {
        g.strokeStyle = c; g.lineWidth = w; g.beginPath(); g.moveTo(x1, y1); g.lineTo(x2, y2); g.stroke();
    };
    auto halo = [&](double x, double y, double r, double dh, double l, double a) {
        dot(x, y, r, MsCanvas::radial(x, y, 0, x, y, r, { { 0, P.glow(dh, 0, l, a) }, { 1, P.glow(dh, 0, l, 0) } }));
    };
    auto flame = [&](double x, double y, double h, double w) {
        g.save(); g.shadowColor = P.glow(40, 0, .7, 1); g.shadowBlur = 6 * s;
        g.fillStyle = P.glow(40, 0, .72, 1); g.beginPath(); g.moveTo(x, y - h);
        g.quadraticCurveTo(x + w * 1.6, y - h * .35, x, y); g.quadraticCurveTo(x - w * 1.6, y - h * .35, x, y - h); g.fill();
        g.shadowBlur = 0; g.fillStyle = P.glow(55, 0, .93, 1); g.beginPath(); g.moveTo(x, y - h * .55);
        g.quadraticCurveTo(x + w * .8, y - h * .2, x, y); g.quadraticCurveTo(x - w * .8, y - h * .2, x, y - h * .55); g.fill();
        g.restore();
    };
    auto stars = [&](int n, double maxY) {
        for (int i = 0; i < n; ++i) { const double x = R(0, VW), y = R(0, maxY), r = R(.3, .9); dot(x, y, r, faint); }
    };
    auto poly = [&](const Pts& pts, const QBrush& c) {
        g.fillStyle = c; g.beginPath();
        for (int i = 0; i < pts.size(); ++i) { if (i) g.lineTo(pts[i].x(), pts[i].y()); else g.moveTo(pts[i].x(), pts[i].y()); }
        g.closePath(); g.fill();
    };

    switch (fam) {
    /* ================= OBJETOS ================= */
    case MsVignette::Candle: {  // vela: mais velas acesas e mais cera escorrida com as palavras
        const double tableY = VH - 22, gap = 12 + (wide - 1) * 12;
        const int n = 1 + jsRound(grow * 4);
        const int order[5] = { 0, -1, 1, -2, 2 };
        const QColor wax2 = P.glow(10, 0, .84, 1);
        struct Drip { double x, l; };
        struct Cand { double x, h, w, fl; Drip drips[4]; };
        Cand C[5];
        for (int k = 0; k < 5; ++k) {
            SubR r = sub(QStringLiteral("v%1").arg(k));
            Cand& c = C[k];
            c.x = CX + order[k] * gap + r(-2, 2);
            c.h = k ? r(14, 30) : r(30, 40);
            c.w = k ? r(7, 9.5) : 11;
            c.fl = r(.85, 1.15);
            for (auto& d : c.drips) { d.x = r(.12, .88); d.l = r(4, 12); }
        }
        for (int k = 0; k < n; ++k) halo(C[k].x, tableY - C[k].h - 6, 10 + grow * 16, 40, .72, .32);
        g.fillStyle = P.shade(.12); g.fillRect(0, tableY, VW, VH - tableY); g.fillStyle = P.shade(.2); g.fillRect(0, tableY, VW, 1.2);
        for (int k = n - 1; k >= 0; --k) {
            const Cand& c = C[k];
            const double x0 = c.x - c.w / 2, top = tableY - c.h;
            g.fillStyle = P.glow(10, 0, .8, .55); g.beginPath(); g.ellipse(c.x, tableY + .6, c.w * .85, 1.8, 0, 0, 7); g.fill();
            g.fillStyle = ink; g.beginPath(); g.roundRect(x0, top, c.w, c.h, 1.5, 1.5, 0, 0); g.fill();
            g.fillStyle = P.shade(.4, .35); g.fillRect(x0 + c.w * .66, top + 1, c.w * .34, c.h - 1);
            const int nd = jsRound(grow * 4);
            for (int d = 0; d < nd; ++d) {
                const double dx = x0 + c.drips[d].x * c.w, l = c.drips[d].l * (.45 + grow * .7);
                line(dx, top + .5, dx, top + l, wax2, 1.7); dot(dx, top + l, 1.15, wax2);
            }
            g.fillStyle = wax2; g.fillRect(x0, top, c.w, 1.6);
            line(c.x, top, c.x, top - 3, P.shade(.06), .8);
            flame(c.x, top - 2.6, (7 + grow * 3) * c.fl, 2.6 * c.fl);
        }
        break;
    }
    case MsVignette::Hourglass: {  // ampulheta: a areia de cima desce pra de baixo conforme o capítulo cresce
        const double cx = CX;
        auto hw = [](double d) { return 1.6 + 15 * std::pow(std::min(1.0, d / 31), .6); };
        g.fillStyle = P.shade(.12); g.fillRect(0, CY + 38, VW, VH);
        auto glass = [&]() {
            g.beginPath();
            for (int d = 31; d >= 0; --d) g.lineTo(cx - hw(d), CY - d);
            for (int d = 0; d <= 31; ++d) g.lineTo(cx - hw(d), CY + d);
            for (int d = 31; d >= 0; --d) g.lineTo(cx + hw(d), CY + d);
            for (int d = 0; d <= 31; ++d) g.lineTo(cx + hw(d), CY - d);
            g.closePath();
        };
        glass(); g.fillStyle = P.glow(0, 0, .8, .1); g.fill();
        const QColor sand = P.glow(35, 0, .66, 1), sand2 = P.glow(30, 0, .52, 1);
        g.save(); glass(); g.clip();
        const double dTop = 2 + 27 * (1 - grow);
        g.fillStyle = sand; g.beginPath(); g.moveTo(cx - 20, CY - dTop);
        g.quadraticCurveTo(cx, CY - dTop + std::min(6.0, dTop * .5), cx + 20, CY - dTop);
        g.lineTo(cx + 20, CY + .5); g.lineTo(cx - 20, CY + .5); g.fill();
        const double hb = 3 + grow * 25;
        g.beginPath(); g.moveTo(cx - 20, CY + 32);
        for (double x = cx - 20; x <= cx + 20; x += 1)
            g.lineTo(x, CY + 31 - hb * std::max(0.0, 1 - std::pow((x - cx) / 16, 2) * .8));
        g.lineTo(cx + 20, CY + 32); g.fill();
        SubR gr = sub(QStringLiteral("gr"));
        for (int i = 0; i < 14; ++i) {
            const double x = cx + gr(-12, 12);
            const double y = CY + 31 - gr(0, 1) * hb * .8 * (1 - std::pow((x - cx) / 16, 2) * .8);
            dot(x, y, .45, sand2);
        }
        if (grow < .98) line(cx, CY, cx, CY + 31 - hb, sand, .9);
        g.restore();
        glass(); g.strokeStyle = P.glow(0, 0, .86, .7); g.lineWidth = .9; g.stroke();
        line(cx - 11, CY - 24, cx - 6, CY - 10, P.glow(0, 0, .97, .35), 1.2);
        const QColor wood = P.shade(.32), wood2 = P.shade(.22);
        g.fillStyle = wood; g.beginPath(); g.roundRect(cx - 24, CY - 38, 48, 5, 1.5); g.fill();
        g.beginPath(); g.roundRect(cx - 24, CY + 33, 48, 5, 1.5); g.fill();
        for (double px : { cx - 20, cx + 20 }) {
            g.fillStyle = wood2; g.fillRect(px - 1.3, CY - 33, 2.6, 66);
            dot(px, CY - 20, 2, wood2); dot(px, CY + 20, 2, wood2); dot(px, CY, 1.7, wood2);
        }
        const int spill = jsRound(grow * 14 * (wide - 1));
        for (int i = 0; i < spill; ++i) {
            SubR r = sub(QStringLiteral("sp%1").arg(i));
            const int side = i % 2 ? 1 : -1;
            const double x = cx + side * r(30, VW / 2);
            const double y = r(CY + 40, VH - 4);
            const double rad = r(.4, .8);
            dot(x, y, rad, sand);
        }
        break;
    }
    case MsVignette::Key: {  // chave antiga: o anel ganha volutas, os dentes aparecem e o molho ganha chaves
        struct Tooth { double w, h; };
        Tooth teeth[6];
        for (auto& t : teeth) { t.w = R(2.2, 3.4); t.h = R(3, 9); }
        const double a0 = R(-.65, -.25), kx = CX + 12, ky = CY - 6, ringR = 13;
        const double bowX = kx + std::cos(a0) * -22, bowY = ky + std::sin(a0) * -22;
        const double rcx = bowX - std::cos(a0) * (ringR - 3), rcy = bowY - std::sin(a0) * (ringR - 3);
        auto key = [&](double x, double y, double ang, double sc, const QColor& col, int orn, int nt) {
            g.save(); g.translate(x, y); g.rotate(ang); g.scale(sc, sc); g.strokeStyle = col; g.fillStyle = col;
            g.lineWidth = 3; g.beginPath(); g.arc(-22, 0, 7.5, 0, 7); g.stroke();
            g.lineWidth = 1.5;
            for (int p = 0; p < orn; ++p) {
                const double a = double(p) / orn * PI * 2;
                g.beginPath(); g.arc(-22 + std::cos(a) * 10.5, std::sin(a) * 10.5, 2.6, 0, 7); g.stroke();
            }
            g.fillRect(-14.5, -1.8, 40, 3.6); g.fillRect(-13, -3.6, 2.6, 7.2); g.fillRect(-9, -3, 1.6, 6);
            double tx = 25.5;
            for (int t = 0; t < nt; ++t) { tx -= teeth[t].w; g.fillRect(tx, 1.6, teeth[t].w + .2, teeth[t].h); }
            g.restore();
        };
        const int extra = jsRound(grow * 2);
        for (int i = 0; i < extra; ++i) {
            SubR r = sub(QStringLiteral("k%1").arg(i));
            const double b = PI * (i ? .98 : .58) + r(-.1, .1), d = ringR + 22 * .72;
            const int orn = 3 + jsRound(r(0, 3));
            const int nt = 2 + jsRound(r(0, 4));
            key(rcx + std::cos(b) * d, rcy + std::sin(b) * d, b, .72, P.shade(.38), orn, nt);
        }
        g.strokeStyle = ink2; g.lineWidth = 1.6; g.beginPath(); g.arc(rcx, rcy, ringR, 0, 7); g.stroke();
        key(kx, ky, a0, 1, ink, 3 + jsRound(grow * 5), 2 + jsRound(grow * 4));
        // o anel passa por dentro do olho da chave
        g.strokeStyle = ink2; g.lineWidth = 1.6; g.beginPath(); g.arc(rcx, rcy, ringR, a0 - .5, a0 + .5); g.stroke();
        break;
    }
    case MsVignette::Compass: {  // bússola de bolso: a rosa ganha pontas e a escala ganha marcas
        const double cx = CX, cy = CY + 3, r = 33;
        g.strokeStyle = P.glow(0, 0, .7, .12); g.lineWidth = .5;
        for (double x = std::fmod(CX, 12); x < VW; x += 12) { g.beginPath(); g.moveTo(x, 0); g.lineTo(x, VH); g.stroke(); }
        for (double y = 4; y < VH; y += 12) { g.beginPath(); g.moveTo(0, y); g.lineTo(VW, y); g.stroke(); }
        const QColor brass = P.glow(40, 0, .62, 1), brass2 = P.glow(35, 0, .42, 1);
        g.strokeStyle = brass; g.lineWidth = 2.4; g.beginPath(); g.arc(cx, cy - r - 9, 4, 0, 7); g.stroke();
        g.fillStyle = brass2; g.fillRect(cx - 3.5, cy - r - 5.5, 7, 4);
        dot(cx, cy, r + 4, brass2); dot(cx, cy, r + 2.4, brass); dot(cx, cy, r, P.shade(.06));
        g.strokeStyle = faint; g.lineWidth = .6; g.beginPath(); g.arc(cx, cy, r - 6.5, 0, 7); g.stroke();
        const int ticks = 16 * (1 + jsRound(grow * 3));
        for (int i = 0; i < ticks; ++i) {
            const double a = double(i) / ticks * PI * 2;
            const bool major = i % (ticks / 4) == 0, mid = i % (ticks / 16) == 0;
            const double l = major ? 5 : mid ? 3.2 : 1.6;
            line(cx + std::cos(a) * (r - 1), cy + std::sin(a) * (r - 1), cx + std::cos(a) * (r - 1 - l), cy + std::sin(a) * (r - 1 - l),
                 major ? ink2 : ink, major ? 1.2 : .6);
        }
        auto point = [&](double a, double len, double wd, const QColor& c1, const QColor& c2) {
            const double tx = cx + std::cos(a) * len, ty = cy + std::sin(a) * len, nx = -std::sin(a) * wd, ny = std::cos(a) * wd;
            poly({ { cx, cy }, { cx + nx, cy + ny }, { tx, ty } }, c1);
            poly({ { cx, cy }, { cx - nx, cy - ny }, { tx, ty } }, c2);
        };
        const double L = r - 8;
        if (grow > .6) for (int i = 0; i < 8; ++i) point(PI / 8 + i * PI / 4, L * .45, 2.4, P.shade(.42), P.shade(.28));
        if (grow > .3) for (int i = 0; i < 4; ++i) point(PI / 4 + i * PI / 2, L * .68, 3.6, ink, P.shade(.45));
        for (int i = 0; i < 4; ++i)
            point(-PI / 2 + i * PI / 2, L, 4.6, i == 0 ? ink2 : ink, i == 0 ? P.glow(30, 0, .6, 1) : P.shade(.5));
        const double na = -PI / 2 + R(-.8, .8), nl = r - 11;
        poly({ { cx + std::cos(na) * nl, cy + std::sin(na) * nl }, { cx - std::sin(na) * 1.8, cy + std::cos(na) * 1.8 },
               { cx + std::sin(na) * 1.8, cy - std::cos(na) * 1.8 } }, P.glow(-30, 0, .6, 1));
        poly({ { cx - std::cos(na) * nl, cy - std::sin(na) * nl }, { cx - std::sin(na) * 1.8, cy + std::cos(na) * 1.8 },
               { cx + std::sin(na) * 1.8, cy - std::cos(na) * 1.8 } }, P.paper);
        dot(cx, cy, 2.2, brass);
        g.strokeStyle = P.glow(0, 0, .97, .16); g.lineWidth = 3; g.beginPath(); g.arc(cx, cy, r - 5, PI * 1.1, PI * 1.45); g.stroke();
        break;
    }
    case MsVignette::PocketWatch: {  // relógio de bolso: a corrente se alonga e o mostrador se abre nas engrenagens
        const double cx = CX + R(-4, 4), cy = CY + 7, r = 25;
        const QColor gold = P.glow(40, 0, .62, 1), gold2 = P.glow(35, 0, .42, 1);
        const double p0[2] = { cx, cy - r - 8 };
        double p1[2], p2[2], p3[2];
        p1[0] = cx + R(-30, -14); p1[1] = cy - r - R(20, 34);
        p2[0] = cx + R(18, 40); p2[1] = cy - r - R(24, 40);
        { const double m = R(30, 46); const double sg = rnd() < .5 ? -1 : 1; p3[0] = cx + m * sg; p3[1] = R(-6, 10); }
        auto bz = [&](double t, int i) { const double u = 1 - t; return u * u * u * p0[i] + 3 * u * u * t * p1[i] + 3 * u * t * t * p2[i] + t * t * t * p3[i]; };
        const int links = jsRound(6 + grow * 30);
        const double span = .2 + grow * .8;
        g.strokeStyle = gold; g.lineWidth = .9;
        for (int i = 0; i < links; ++i) {
            const double t = double(i) / links * span, x = bz(t, 0), y = bz(t, 1), t2 = std::min(1.0, t + .01);
            const double a = std::atan2(bz(t2, 1) - y, bz(t2, 0) - x);
            g.beginPath(); g.ellipse(x, y, 2.4, i % 2 ? 1.3 : .55, a, 0, 7); g.stroke();
        }
        g.fillStyle = gold2; g.fillRect(cx - 3, cy - r - 6, 6, 5); g.strokeStyle = gold; g.lineWidth = 1.4;
        g.beginPath(); g.arc(cx, cy - r - 9, 3.2, 0, 7); g.stroke();
        dot(cx, cy, r + 3, gold2); dot(cx, cy, r + 1.6, gold); dot(cx, cy, r - 1.2, P.paper);
        for (int i = 0; i < 12; ++i) {
            const double a = double(i) / 12 * PI * 2, l = i % 3 ? 2.4 : 4;
            line(cx + std::cos(a) * (r - 3), cy + std::sin(a) * (r - 3), cx + std::cos(a) * (r - 3 - l), cy + std::sin(a) * (r - 3 - l),
                 P.paperInk, i % 3 ? .6 : 1.1);
        }
        if (grow > .35)
            for (int i = 0; i < 60; ++i) {
                if (i % 5 == 0) continue;
                const double a = double(i) / 60 * PI * 2;
                dot(cx + std::cos(a) * (r - 3.4), cy + std::sin(a) * (r - 3.4), .28, P.paperInk);
            }
        const double ha = R(0, PI * 2), ma = R(0, PI * 2), sa = R(0, PI * 2);
        if (grow > .2) {
            const double sx = cx, sy = cy + r * .45;
            g.strokeStyle = P.paperInk; g.lineWidth = .5; g.beginPath(); g.arc(sx, sy, 5, 0, 7); g.stroke();
            line(sx, sy, sx + std::cos(sa) * 4, sy + std::sin(sa) * 4, P.paperInk, .5);
        }
        if (grow > .55) {
            const double wx = cx, wy = cy - r * .45, wr = 6.5;
            dot(wx, wy, wr, P.shade(.08)); g.save(); g.beginPath(); g.arc(wx, wy, wr, 0, 7); g.clip();
            auto gear = [&](double x, double y, double rr, int n, double rot) {
                g.beginPath();
                for (int t = 0; t < n * 2; ++t) {
                    const double a = rot + double(t) / (n * 2) * PI * 2, q = t % 2 ? rr : rr + 1.3;
                    g.lineTo(x + std::cos(a) * q, y + std::sin(a) * q);
                }
                g.closePath(); g.strokeStyle = gold; g.lineWidth = .6; g.stroke(); dot(x, y, .8, gold);
            };
            gear(wx - 1.5, wy + .5, 3.6, 10, ha); gear(wx + 4.2, wy - 3, 2.6, 8, ma); g.restore();
            g.strokeStyle = gold2; g.lineWidth = .8; g.beginPath(); g.arc(wx, wy, wr, 0, 7); g.stroke();
        }
        line(cx, cy, cx + std::cos(ha) * r * .48, cy + std::sin(ha) * r * .48, P.paperInk, 1.8);
        line(cx, cy, cx + std::cos(ma) * r * .74, cy + std::sin(ma) * r * .74, P.paperInk, 1);
        dot(cx, cy, 1.6, gold2);
        g.strokeStyle = P.glow(0, 0, 1, .35); g.lineWidth = 2; g.beginPath(); g.arc(cx, cy, r - 5, PI * 1.15, PI * 1.4); g.stroke();
        break;
    }
    case MsVignette::Inkwell: {  // pena e tinteiro: a folha se enche de linhas conforme o capítulo cresce
        const double pw = 50 + (wide - 1) * 34, ph = 62, rot = R(-.2, -.06);
        g.save(); g.translate(CX - 8, CY + 2); g.rotate(rot);
        g.fillStyle = P.shade(.02, .45); g.fillRect(-pw / 2 + 2, -ph / 2 + 2, pw, ph); g.fillStyle = P.paper; g.fillRect(-pw / 2, -ph / 2, pw, ph);
        const int rows = 1 + jsRound(grow * 9);
        g.strokeStyle = P.paperInk; g.lineWidth = .6;
        for (int k = 0; k < rows; ++k) {
            SubR r = sub(QStringLiteral("l%1").arg(k));
            const double y = -ph / 2 + 8 + k * 5.4;
            const double end = k == rows - 1 ? r(.3, .75) : r(.86, 1);
            const double stop = -pw / 2 + 6 + (pw - 12) * end;
            double x = -pw / 2 + 6 + (k == 0 ? r(4, 8) : 0);
            int up = 1;
            g.beginPath(); g.moveTo(x, y);
            while (x < stop) {
                x += r(.9, 2); up = -up;
                g.lineTo(x, y + up * r(.3, 1.5));
                if (r(0, 1) < .08) { x += r(1.6, 3); g.moveTo(x, y); }
            }
            g.stroke();
        }
        g.restore();
        const double ix = CX + 20, iy = CY + 26;
        double tip[2], ctl[2];
        tip[0] = ix + R(10, 18); tip[1] = iy - R(56, 62);
        ctl[0] = ix + R(-5, 3); ctl[1] = iy - 36;
        auto sp = [&](double t) { const double u = 1 - t;
            return QPointF(u * u * (ix - 1) + 2 * u * t * ctl[0] + t * t * tip[0], u * u * (iy - 15) + 2 * u * t * ctl[1] + t * t * tip[1]); };
        Pts L, Rt;
        for (int i = 0; i <= 24; ++i) {
            const double t = .3 + .7 * i / 24;
            const QPointF p = sp(t), p2 = sp(std::min(1.0, t + .01));
            const double a = std::atan2(p2.y() - p.y(), p2.x() - p.x()), w = std::pow(std::sin((t - .3) / .7 * PI), .7);
            L.append({ p.x() + std::cos(a - PI / 2) * w * 7, p.y() + std::sin(a - PI / 2) * w * 7 });
            Rt.append({ p.x() + std::cos(a + PI / 2) * w * 3.6, p.y() + std::sin(a + PI / 2) * w * 3.6 });
        }
        Pts quill = L;
        for (int i = Rt.size() - 1; i >= 0; --i) quill.append(Rt[i]);
        poly(quill, ink);
        g.strokeStyle = P.shade(.4, .55); g.lineWidth = .35;
        for (int i = 2; i < 22; i += 2) {
            const QPointF p = sp(.3 + .7 * i / 24);
            g.beginPath(); g.moveTo(p.x(), p.y());
            g.lineTo(L[i].x() + (tip[0] - p.x()) * .08, L[i].y() + (tip[1] - p.y()) * .08); g.stroke();
        }
        g.strokeStyle = P.paper; g.lineWidth = .7; g.beginPath();
        for (int i = 0; i <= 20; ++i) { const QPointF p = sp(i / 20.0); if (i) g.lineTo(p.x(), p.y()); else g.moveTo(p.x(), p.y()); }
        g.stroke();
        g.fillStyle = P.shade(.04); g.beginPath(); g.roundRect(ix - 10, iy - 12, 20, 14, 4); g.fill(); g.fillRect(ix - 5, iy - 16, 10, 5);
        g.fillStyle = P.shade(.22); g.fillRect(ix - 6, iy - 17, 12, 2);
        line(ix - 6.5, iy - 9, ix - 6.5, iy - 3, P.glow(0, 0, .92, .35), 1.2);
        const int spl = jsRound(grow * 6);
        for (int i = 0; i < spl; ++i) {
            SubR r = sub(QStringLiteral("sp%1").arg(i));
            const double x = CX + r(-30, 8);
            const double y = CY + r(14, 30);
            const double rad = r(.4, 1.3);
            dot(x, y, rad, P.paperInk);
        }
        break;
    }
    case MsVignette::Cup: {  // xícara: o vapor sobe mais alto e a mesa ganha marcas de café com as palavras
        const double ty = CY + 14;
        g.fillStyle = P.shade(.13); g.fillRect(0, ty, VW, VH - ty);
        const int rings = jsRound(grow * 4);
        for (int i = 0; i < rings; ++i) {
            SubR r = sub(QStringLiteral("m%1").arg(i));
            const int side = i % 2 ? 1 : -1;
            const double x = CX + side * r(27, 30 + (wide - 1) * 40);
            const double y = r(ty + 10, VH - 8);
            const double rx = r(7, 9), ry = r(2.6, 3.4), b0 = r(0, 1), b1 = r(4.6, 6.2);
            g.strokeStyle = P.shade(.3, .75); g.lineWidth = 1.2; g.beginPath(); g.ellipse(x, y, rx, ry, 0, b0, b1); g.stroke();
        }
        g.fillStyle = P.paper2; g.beginPath(); g.ellipse(CX, ty + 15, 31, 7, 0, 0, 7); g.fill();
        g.fillStyle = P.paper; g.beginPath(); g.ellipse(CX, ty + 13.6, 29, 5.6, 0, 0, 7); g.fill();
        auto body = [&]() {
            g.beginPath(); g.moveTo(CX - 17, ty - 14); g.lineTo(CX + 17, ty - 14);
            g.bezierCurveTo(CX + 17, ty + 2, CX + 13, ty + 10, CX + 8, ty + 11.5); g.lineTo(CX - 8, ty + 11.5);
            g.bezierCurveTo(CX - 13, ty + 10, CX - 17, ty + 2, CX - 17, ty - 14); g.closePath();
        };
        g.strokeStyle = P.paper; g.lineWidth = 3.4; g.beginPath(); g.arc(CX + 18.5, ty - 3, 6, -1.3, 1.5); g.stroke();
        body(); g.fillStyle = P.paper; g.fill();
        g.save(); body(); g.clip(); g.fillStyle = P.shade(.4, .28); g.fillRect(CX + 5, ty - 16, 20, 30);
        const double band = rnd();
        if (band < .6) { g.fillStyle = ink2; g.fillRect(CX - 20, ty - 7, 40, 2.2); if (band < .25) g.fillRect(CX - 20, ty - 3.4, 40, .8); }
        g.restore();
        g.fillStyle = P.paper2; g.beginPath(); g.ellipse(CX, ty - 14, 17, 4.2, 0, 0, 7); g.fill();
        g.fillStyle = P.shade(.1); g.beginPath(); g.ellipse(CX, ty - 13.6, 14.6, 3.3, 0, 0, 7); g.fill();
        g.strokeStyle = P.glow(30, 0, .5, .8); g.lineWidth = .8; g.beginPath(); g.ellipse(CX, ty - 13.6, 11, 2.3, 0, 0, 7); g.stroke();
        const int curls = 2 + jsRound(grow * 2), steps = jsRound(10 + grow * 26);
        const double xs[4] = { -6, 5, -1, 10 };
        for (int i = 0; i < curls; ++i) {
            SubR r = sub(QStringLiteral("st%1").arg(i));
            const double ph = r(0, 6), x0 = CX + xs[i];
            g.lineWidth = 1.6;
            for (int j = 0; j < steps; ++j) {
                const double t = double(j) / steps, y = ty - 17 - j * 1.6, x = x0 + std::sin(j * .32 + ph) * 3 * (t + .3);
                const double y2 = y - 1.6, x2 = x0 + std::sin((j + 1) * .32 + ph) * 3 * (double(j + 1) / steps + .3);
                g.strokeStyle = P.glow(0, 0, .94, .5 * (1 - t)); g.beginPath(); g.moveTo(x, y); g.lineTo(x2, y2); g.stroke();
            }
        }
        break;
    }
    case MsVignette::Books: {  // pilha de livros: a cada trecho, mais um livro na pilha
        const double base = VH - 16;
        const int n = 2 + jsRound(grow * 8);
        g.fillStyle = P.shade(.12); g.fillRect(0, base, VW, VH - base);
        const QColor cols[6] = { P.shade(.3), P.shade(.44), P.glow(30, 0, .5, 1), P.glow(-30, 0, .46, 1), ink, P.shade(.22) };
        const int side = wide > 1.2 ? jsRound((wide - 1) * 10) : 0;
        for (int i = 0; i < side; ++i) {
            SubR r = sub(QStringLiteral("u%1").arg(i));
            const double x = CX + (i % 2 ? 1 : -1) * (30 + std::floor(i / 2) * 6.2);
            const double h = r(26, 40), w = r(4.6, 6);
            g.fillStyle = cols[int(std::floor(r(0, 6)))]; g.fillRect(x - w / 2, base - h, w, h);
            g.fillStyle = P.paper; g.globalAlpha = .55; g.fillRect(x - w / 2, base - h + 4, w, .8); g.fillRect(x - w / 2, base - 6, w, .8); g.globalAlpha = 1;
        }
        double y = base;
        for (int i = 0; i < n; ++i) {
            SubR r = sub(QStringLiteral("b%1").arg(i));
            const double th = r(5, 7.5), w = r(36, 50) * (i ? 1 : 1.06), x = CX + r(-6, 6) - w / 2;
            const bool pages = r(0, 1) < .28;
            if (pages) {
                g.fillStyle = P.paper; g.fillRect(x, y - th, w, th); g.strokeStyle = P.paper2; g.lineWidth = .4;
                for (int l = 1; l < 4; ++l) { g.beginPath(); g.moveTo(x + 1, y - th + l * th / 4); g.lineTo(x + w - 1, y - th + l * th / 4); g.stroke(); }
                g.fillStyle = cols[int(std::floor(r(0, 6)))]; g.fillRect(x, y - th, w, 1); g.fillRect(x, y - 1, w, 1);
            } else {
                g.fillStyle = cols[int(std::floor(r(0, 6)))]; g.beginPath(); g.roundRect(x, y - th, w, th, 1); g.fill();
                g.fillStyle = P.glow(40, 0, .75, .85); g.fillRect(x + 3, y - th, 1, th); g.fillRect(x + w - 4, y - th, 1, th);
                g.fillStyle = P.paper; g.globalAlpha = .7; g.fillRect(x + w * .3, y - th / 2 - .6, w * r(.2, .4), 1.2); g.globalAlpha = 1;
            }
            y -= th;
        }
        if (grow > .55) {
            const double gx = CX + R(-6, 6);
            g.strokeStyle = P.paper; g.lineWidth = .9;
            g.beginPath(); g.arc(gx - 4.5, y - 3, 3.2, 0, 7); g.stroke();
            g.beginPath(); g.arc(gx + 4.5, y - 3, 3.2, 0, 7); g.stroke();
            g.beginPath(); g.moveTo(gx - 1.3, y - 3.6); g.quadraticCurveTo(gx, y - 4.6, gx + 1.3, y - 3.6); g.stroke();
        }
        break;
    }
    case MsVignette::Lantern: {  // lampião: a luz alcança mais longe e junta mais vaga-lumes com as palavras
        const double cx = CX, hy = CY - 14, fy = hy + 21;
        halo(cx, fy, 18 + grow * 42, 40, .7, .45);
        line(cx, -2, cx, hy - 15, P.shade(.36), .9);
        g.strokeStyle = P.shade(.36); g.lineWidth = 1.2; g.beginPath(); g.arc(cx, hy - 16, 2.6, 0, 7); g.stroke();
        poly({ { cx - 14, hy - 1 }, { cx - 6, hy - 11 }, { cx + 6, hy - 11 }, { cx + 14, hy - 1 } }, P.shade(.18));
        g.fillStyle = P.shade(.26); g.fillRect(cx - 14, hy - 2, 28, 2.4);
        g.fillStyle = P.glow(40, 0, .76, .3); g.fillRect(cx - 11, hy, 22, 29);
        flame(cx, fy + 5, 11, 3);
        for (double bx : { cx - 11, cx - 3.7, cx + 3.7, cx + 11 })
            line(bx, hy, bx, hy + 29, P.shade(.16), (bx == cx - 11 || bx == cx + 11) ? 1.8 : .8);
        line(cx - 11, hy + 14.5, cx + 11, hy + 14.5, P.shade(.16), .6);
        poly({ { cx - 13, hy + 29 }, { cx + 13, hy + 29 }, { cx + 9, hy + 35 }, { cx - 9, hy + 35 } }, P.shade(.2));
        const int ff = jsRound((2 + grow * 14) * wide);
        for (int i = 0; i < ff; ++i) {
            SubR r = sub(QStringLiteral("f%1").arg(i));
            const double x = r(4, VW - 4);
            const double y = r(6, VH - 6);
            if (std::abs(x - cx) < 15 && y > hy - 12 && y < hy + 36) continue;
            halo(x, y, 3.2, 50, .8, .55); dot(x, y, .7, P.glow(50, 0, .9, 1));
        }
        break;
    }
    /* ================= CENAS ================= */
    case MsVignette::Road: {  // estrada à noite: mais postes, mais carros e a cidade acendendo no horizonte
        const double hz = VH * .44, vx = CX + R(-12, 12), bw = 46 + (wide - 1) * 40;
        stars(jsRound(16 * wide), hz - 6);
        halo(vx, hz, 50 + grow * 30, 30, .6, .12 + grow * .3);
        if (grow > .25) {
            const int nb = jsRound((6 + grow * 14) * wide);
            for (int i = 0; i < nb; ++i) {
                SubR r = sub(QStringLiteral("b%1").arg(i));
                const double x = vx + r(-1, 1) * VW * .5, w = r(2, 5), h = r(2, 9) * grow;
                g.fillStyle = P.shade(.12); g.fillRect(x, hz - h, w, h);
                if (r(0, 1) < .5) dot(x + w / 2, hz - h * .5, .45, P.glow(40, 0, .8, 1));
            }
        }
        g.fillStyle = P.shade(.07); g.fillRect(0, hz, VW, VH - hz);
        auto X = [&](double u, double o) { return vx + (CX - vx) * u + o * (1.2 + (bw - 1.2) * u); };
        auto Y = [&](double u) { return hz + (VH - hz) * u; };
        poly({ { X(0, -1), Y(0) }, { X(0, 1), Y(0) }, { X(1, 1), Y(1) }, { X(1, -1), Y(1) } }, P.shade(.15));
        g.strokeStyle = faint; g.lineWidth = .6; g.beginPath();
        g.moveTo(X(0, -.92), Y(0)); g.lineTo(X(1, -.92), Y(1)); g.moveTo(X(0, .92), Y(0)); g.lineTo(X(1, .92), Y(1)); g.stroke();
        for (int k = 0; k < 9; ++k) {
            const double u0 = std::pow(k / 9.0, 1.6), u1 = std::pow((k + .45) / 9, 1.6), w0 = .2 + u0 * 1.6, w1 = .2 + u1 * 1.6;
            poly({ { X(u0, 0) - w0 / 2, Y(u0) }, { X(u0, 0) + w0 / 2, Y(u0) }, { X(u1, 0) + w1 / 2, Y(u1) }, { X(u1, 0) - w1 / 2, Y(u1) } }, ink2);
        }
        const int np = 2 + jsRound(grow * 4);
        for (int k = 0; k < np; ++k) {
            const double u = std::pow((k + .7) / (np + .2), 1.5);
            for (int side : { -1, 1 }) {
                const double x = X(u, side * 1.25), y = Y(u), h = 4 + u * 40, lx = x - side * (2 + u * 8), ly = y - h;
                g.save(); g.globalAlpha = .14 + .12 * grow;
                poly({ { lx, ly }, { lx - 3 - u * 14, y }, { lx + 3 + u * 14, y } },
                     MsCanvas::linear(0, ly, 0, y, { { 0, P.glow(40, 0, .8, 1) }, { 1, P.glow(40, 0, .8, 0) } }));
                g.restore();
                line(x, y, x, ly, P.shade(.3), .4 + u * 1.2); line(x, ly, lx, ly, P.shade(.3), .3 + u);
                dot(lx, ly + .5, .5 + u * 1.4, P.glow(40, 0, .86, 1));
            }
        }
        const int nc = jsRound(grow * 3);
        for (int i = 0; i < nc; ++i) {
            SubR r = sub(QStringLiteral("c%1").arg(i));
            const double u = r(.15, .85);
            const bool toward = i % 2 == 0;
            const double x = X(u, toward ? -.5 : .5), y = Y(u) - .5, sp = .6 + u * 4, rr = .4 + u * 1.5;
            if (toward) { halo(x, y, 4 + u * 14, 40, .85, .5); dot(x - sp, y, rr, P.glow(40, 0, .96, 1)); dot(x + sp, y, rr, P.glow(40, 0, .96, 1)); }
            else { dot(x - sp, y, rr * .8, P.glow(-45, 0, .55, 1)); dot(x + sp, y, rr * .8, P.glow(-45, 0, .55, 1)); }
        }
        break;
    }
    case MsVignette::Noir: {  // noir: luz da persiana, fumaça que sobe e chuva no vidro
        const double ang = R(.32, .55), fx = CX + R(-16, 10);
        const int nb = 5 + int(std::floor(rnd() * 3));
        const double off = R(-30, -18), gap = R(10, 13);
        g.fillStyle = P.shade(.02, .5); g.fillRect(0, 0, VW, VH);
        const int drops = jsRound(grow * grow * 46 * wide);
        for (int i = 0; i < drops; ++i) {
            SubR r = sub(QStringLiteral("d%1").arg(i));
            const double x = r(0, VW), y = r(0, VH), l = r(2, 7);
            line(x, y, x + .3, y + l, P.glow(0, 0, .85, .22), .5); dot(x + .3, y + l, .6, P.glow(0, 0, .9, .35));
        }
        g.fillStyle = P.shade(0);
        g.beginPath(); g.moveTo(fx - 26, VH); g.bezierCurveTo(fx - 24, VH - 24, fx - 18, VH - 34, fx - 6, VH - 37); g.lineTo(fx - 4, VH - 42);
        g.lineTo(fx + 4, VH - 42); g.lineTo(fx + 6, VH - 37); g.bezierCurveTo(fx + 18, VH - 34, fx + 24, VH - 24, fx + 26, VH); g.closePath(); g.fill();
        g.beginPath(); g.ellipse(fx, VH - 48, 6.4, 7.4, 0, 0, 7); g.fill();
        g.beginPath(); g.ellipse(fx, VH - 53.5, 13, 2.6, -.06, 0, 7); g.fill();
        g.beginPath(); g.moveTo(fx - 7, VH - 54); g.lineTo(fx - 6, VH - 62); g.quadraticCurveTo(fx, VH - 65, fx + 6, VH - 62); g.lineTo(fx + 7, VH - 54); g.fill();
        const double cx0 = fx + 5.5, cy0 = VH - 44;
        line(cx0, cy0, cx0 + 6, cy0 + .6, P.paper, 1); halo(cx0 + 6.3, cy0 + .6, 4, -30, .6, .6); dot(cx0 + 6.3, cy0 + .6, .9, P.glow(-30, 0, .62, 1));
        const double smoke = 10 + grow * 46, ph = R(0, 6);
        for (int j = 0; j < smoke; ++j) {
            const double t = j / 56.0, y = cy0 - j * 1.2, x = cx0 + 6.5 + std::sin(j * .22 + ph) * (2 + t * 6) + t * 4;
            dot(x, y, .6 + t * 1.2, P.glow(0, 0, .88, .24 * (1 - t)));
        }
        g.save(); g.translate(CX, CY); g.rotate(-ang); g.fillStyle = P.glow(40, 0, .8, .14);
        for (int i = 0; i < nb; ++i) g.fillRect(-VW, off + i * gap, VW * 2, gap * .48);
        g.restore();
        break;
    }
    case MsVignette::Mansion: {  // mansão assombrada: a casa ganha alas e torres, os morcegos saem da lua
        const double mx = CX + R(-26, 26), my = R(20, 30);
        halo(mx, my, 26, 40, .8, .25); dot(mx, my, 12, P.glow(40, 0, .86, .95));
        dot(mx - 3, my - 2, 2.2, P.glow(40, 0, .78, .6)); dot(mx + 4, my + 3, 1.6, P.glow(40, 0, .78, .6));
        const double hillY = VH - 20;
        g.fillStyle = P.shade(.06); g.beginPath(); g.moveTo(0, VH); g.lineTo(0, hillY + 6); g.quadraticCurveTo(CX, hillY - 10, VW, hillY + 6); g.lineTo(VW, VH); g.fill();
        const double hx = CX + R(-6, 6), base = hillY - 2;
        const QColor dark = P.shade(.01), win = P.glow(45, 0, .72, 1);
        const int wingSide = rnd() < .5 ? -1 : 1;
        auto block = [&](double x, double w, double h, double roofH) {
            g.fillStyle = dark; g.fillRect(x - w / 2, base - h, w, h);
            poly({ { x - w / 2 - 2, base - h }, { x, base - h - roofH }, { x + w / 2 + 2, base - h } }, dark);
        };
        auto tower = [&](double x, double h) {
            g.fillStyle = dark; g.fillRect(x - 4.5, base - h, 9, h);
            poly({ { x - 6, base - h }, { x, base - h - 14 }, { x + 6, base - h } }, dark); line(x, base - h - 14, x, base - h - 19, dark, .7);
            g.fillStyle = win; g.fillRect(x - 1, base - h + 5, 2, 3);
        };
        const int wings = jsRound(grow * 2);
        for (int i = 0; i < wings; ++i) {
            const int side = i == 0 ? wingSide : -wingSide;
            block(hx + side * 22, 16, 14, 7); g.fillStyle = win;
            if (sub(QStringLiteral("ala%1").arg(i))(0, 1) < .6) g.fillRect(hx + side * 22 - 1.5, base - 9, 3, 3.4);
        }
        if (grow > .35) tower(hx - wingSide * 15, 34);
        if (grow > .7) tower(hx + wingSide * 31, 26);
        block(hx, 28, 22, 13);
        SubR wr = sub(QStringLiteral("w"));
        for (int r = 0; r < 2; ++r)
            for (int c = 0; c < 4; ++c) {
                const bool lit = wr(0, 1) < .35;
                g.fillStyle = lit ? win : P.shade(.14); g.fillRect(hx - 10 + c * 6, base - 18 + r * 8, 2.6, 4);
            }
        g.fillStyle = P.shade(.14); g.fillRect(hx - 2, base - 6, 4, 6);
        SubR tr = sub(QStringLiteral("tree"));
        const double tx = hx + (wingSide > 0 ? -1 : 1) * tr(34, 40);
        std::function<void(double, double, double, double, int)> branch = [&](double x, double y, double len, double a, int d) {
            if (d == 0 || len < 2) return;
            const double x2 = x + std::cos(a) * len, y2 = y + std::sin(a) * len;
            line(x, y, x2, y2, dark, std::max(.5, d * .8));
            const double aL = a - tr(.3, .7);
            branch(x2, y2, len * .72, aL, d - 1);
            const double aR = a + tr(.3, .7);
            branch(x2, y2, len * .68, aR, d - 1);
        };
        const double a0 = -PI / 2 + tr(-.2, .2);
        branch(tx, hillY + 2, 16, a0, 5);
        const int bats = jsRound(grow * 9 * wide);
        for (int i = 0; i < bats; ++i) {
            SubR r = sub(QStringLiteral("bt%1").arg(i));
            const double x = mx + r(-30, 30) * wide, y = my + r(-14, 26), sz = r(1.6, 3);
            g.strokeStyle = dark; g.lineWidth = 1; g.beginPath(); g.moveTo(x - sz * 1.6, y - sz * .4);
            g.quadraticCurveTo(x - sz * .8, y + sz * .3, x, y); g.quadraticCurveTo(x + sz * .8, y + sz * .3, x + sz * 1.6, y - sz * .4); g.stroke();
        }
        break;
    }
    case MsVignette::Planet: {  // planeta com anéis: ganha luas e o anel ganha faixas com as palavras
        stars(jsRound(26 * wide), VH);
        const double px = CX + R(-6, 6), py = CY + R(-4, 4), pr = 19, tilt = R(-.55, -.15);
        const int bands = 1 + jsRound(grow * 3);
        auto ring = [&](bool front) {
            g.save(); g.translate(px, py); g.rotate(tilt);
            for (int b = 0; b < bands; ++b) {
                const double rx = pr * (1.4 + b * .22), ry = rx * .26;
                g.strokeStyle = b % 2 ? P.glow(30, 0, .75, .75) : P.glow(0, 0, .8, .9); g.lineWidth = 2.4 - b * .3;
                g.beginPath(); g.ellipse(0, 0, rx, ry, 0, front ? 0 : PI, front ? PI : 2 * PI); g.stroke();
            }
            g.restore();
        };
        ring(false);
        dot(px, py, pr, MsCanvas::radial(px - 6, py - 7, 2, px, py, pr, { { 0, P.glow(10, 0, .66, 1) }, { 1, P.shade(.12) } }));
        g.save(); g.beginPath(); g.arc(px, py, pr, 0, 7); g.clip(); g.translate(px, py); g.rotate(tilt);
        SubR st = sub(QStringLiteral("p"));
        for (int k = 0; k < 5; ++k) {
            g.fillStyle = k % 2 ? P.shade(.2, .35) : P.glow(20, 0, .7, .18);
            const double y = -pr + st(0, 2 * pr), h = st(1.5, 4);
            g.fillRect(-pr, y, 2 * pr, h);
        }
        g.restore();
        ring(true);
        const int moons = jsRound(grow * 4);
        for (int i = 0; i < moons; ++i) {
            SubR r = sub(QStringLiteral("m%1").arg(i));
            const double a = r(0, 2 * PI), d = r(32, 44), mr = r(1.6, 4.2);
            const double x = px + std::cos(a) * d * std::sqrt(wide), y = py + std::sin(a) * d * .75;
            dot(x, y, mr, MsCanvas::radial(x - mr * .4, y - mr * .4, .3, x, y, mr, { { 0, P.glow(-20, 0, .85, 1) }, { 1, P.shade(.2) } }));
        }
        break;
    }
    case MsVignette::Fleet: {  // frota: uma nave capitânia e caças que chegam conforme o capítulo cresce
        stars(jsRound(30 * wide), VH * .8);
        const double plx = CX + R(-10, 10);
        dot(plx, VH + 78, 96, MsCanvas::radial(plx - 20, VH + 20, 10, plx, VH + 78, 96, { { 0, P.glow(10, 0, .5, 1) }, { 1, P.shade(.06) } }));
        g.strokeStyle = P.glow(0, 0, .75, .55); g.lineWidth = 1.6; g.beginPath(); g.arc(plx, VH + 78, 96, 0, 7); g.stroke();
        const int dir = rnd() < .5 ? -1 : 1;
        const double sx = CX + R(-6, 6), sy = CY - 8, Ls = 54, tail = sx - dir * Ls / 2, nose = sx + dir * Ls / 2;
        const double trail = 6 + grow * 24;
        for (double oy : { -5.0, -1.0, 3.0 }) {
            g.strokeStyle = MsCanvas::linear(tail, 0, tail - dir * trail, 0, { { 0, P.glow(40, 0, .8, .8) }, { 1, P.glow(40, 0, .8, 0) } });
            g.lineWidth = 1.6; g.beginPath(); g.moveTo(tail, sy + oy); g.lineTo(tail - dir * trail, sy + oy); g.stroke();
        }
        poly({ { nose, sy + 1 }, { tail, sy - 9 }, { tail, sy + 8 } }, P.shade(.3));
        poly({ { nose, sy + 1 }, { tail, sy - 9 }, { tail, sy - 1 } }, P.shade(.44));
        const double bx = tail + dir * 8;
        g.fillStyle = P.shade(.38); g.fillRect(bx - 4, sy - 14, 8, 5); g.fillRect(bx - 1.5, sy - 17, 3, 3);
        g.strokeStyle = P.shade(.22); g.lineWidth = .4;
        for (int k = 1; k < 4; ++k) {
            const double t = k / 4.0;
            g.beginPath(); g.moveTo(tail + dir * Ls * t, sy - 9 + 10 * t - 1); g.lineTo(tail + dir * Ls * t, sy + 8 - 7 * t); g.stroke();
        }
        g.save(); g.shadowColor = P.glow(40, 0, .8, 1); g.shadowBlur = 4 * s;
        for (double oy : { -5.0, -1.0, 3.0 }) dot(tail, sy + oy, 1.3, P.glow(40, 0, .92, 1));
        g.restore();
        for (int k = 0; k < 6; ++k) dot(tail + dir * (6 + k * 5), sy - 3 + k * .4, .35, ink2);
        const int nf = jsRound(grow * 7 * wide);
        for (int i = 0; i < nf; ++i) {
            SubR r = sub(QStringLiteral("f%1").arg(i));
            const double x = r(6, VW - 6), y = r(10, VH * .72), sz = r(2.4, 4);
            if (std::abs(y - sy) < 13 && std::abs(x - sx) < Ls / 2 + 5) continue;
            line(x - dir * sz * .6, y, x - dir * sz * 3.4, y, P.glow(40, 0, .8, .45), .6);
            poly({ { x + dir * sz, y }, { x - dir * sz * .8, y - sz * .6 }, { x - dir * sz * .5, y }, { x - dir * sz * .8, y + sz * .6 } }, ink);
        }
        break;
    }
    case MsVignette::Sailboat: {  // veleiro: o navio ganha mastros e iça mais velas com as palavras
        const double wl = CY + 24, cx = CX + R(-4, 4);
        const double sunSide = rnd() < .5 ? -1 : 1;
        const double sunX = cx + sunSide * R(26, 34);
        { const double sy = R(16, 26), sr = R(4, 6); dot(sunX, sy, sr, P.glow(30, 0, .82, .85)); }
        g.fillStyle = P.shade(.13); g.fillRect(0, wl, VW, VH - wl);
        for (int w = 0; w < 5; ++w) {
            g.strokeStyle = w % 2 ? faint : P.glow(0, 0, .8, .35); g.lineWidth = .7; g.beginPath();
            const double y = wl + 4 + w * 5;
            for (double x = 0; x <= VW; x += 2) g.lineTo(x, y + std::sin(x * .25 + w * 1.7));
            g.stroke();
        }
        const int masts = 1 + jsRound(grow * 2);
        const double mo[3] = { 0, -15, 14 }, mh[3] = { 44, 35, 33 };
        poly({ { cx - 30, wl - 9 }, { cx + 28, wl - 10 }, { cx + 21, wl + 1 }, { cx - 22, wl + 1 } }, P.shade(.18));
        g.fillStyle = ink2; g.fillRect(cx - 27, wl - 6.5, 50, 1.1);
        for (int m = 0; m < masts; ++m) {
            const double x = cx + mo[m], top = wl - 9 - mh[m], k1 = mh[m] / 50;
            SubR r = sub(QStringLiteral("s%1").arg(m));
            line(x, wl - 9, x, top, P.shade(.32), 1.3);
            const int ns = std::min(3, 1 + jsRound(grow * 2));
            for (int k = 0; k < ns; ++k) {
                const double sw = (19 - k * 4.5) * (m ? .8 : 1), y0 = wl - 13 - k * 12 * k1, sh = 10 * k1;
                g.fillStyle = P.paper; g.beginPath(); g.moveTo(x - sw / 2, y0 - sh); g.lineTo(x + sw / 2, y0 - sh);
                g.quadraticCurveTo(x + sw / 2 + 2, y0 - sh / 2, x + sw / 2, y0); g.quadraticCurveTo(x, y0 + 2.5, x - sw / 2, y0);
                g.quadraticCurveTo(x - sw / 2 + 2, y0 - sh / 2, x - sw / 2, y0 - sh); g.fill();
                line(x - sw / 2, y0 - sh, x + sw / 2, y0 - sh, P.shade(.32), .8);
            }
            poly({ { x, top }, { x + (r(0, 1) < .5 ? 7 : -7), top + 2 }, { x, top + 4 } }, ink2);
        }
        if (grow > .5) poly({ { cx + 1, wl - 9 - mh[0] + 8 }, { cx + 31, wl - 10 }, { cx + 1, wl - 13 } }, P.paper2);
        for (int k = 0; k < 3; ++k) line(cx - 18 + k * 12, wl + 3 + k * 1.5, cx - 10 + k * 12, wl + 3 + k * 1.5, P.glow(0, 0, .8, .3), .6);
        const int gulls = jsRound(grow * 4 * wide);
        for (int i = 0; i < gulls; ++i) {
            SubR r = sub(QStringLiteral("g%1").arg(i));
            const double x = r(6, VW - 6), y = r(8, 30), sz = r(1.6, 2.6);
            g.strokeStyle = ink; g.lineWidth = .8; g.beginPath(); g.moveTo(x - sz * 1.5, y - sz * .5);
            g.quadraticCurveTo(x - sz * .6, y - sz, x, y); g.quadraticCurveTo(x + sz * .6, y - sz, x + sz * 1.5, y - sz * .5); g.stroke();
        }
        break;
    }
    case MsVignette::Wings: {  // asas: camadas de penas que se abrem com as palavras
        const double cy = CY + 2;
        const int rows = 2 + jsRound(grow * 3);
        const double spread = R(.9, 1.1);
        if (rnd() < .6) {
            g.save(); g.shadowColor = P.glow(40, 0, .8, 1); g.shadowBlur = 6 * s; g.strokeStyle = P.glow(40, 0, .82, .85); g.lineWidth = 1.8;
            g.beginPath(); g.ellipse(CX, cy - 30, 10, 2.8, 0, 0, 7); g.stroke(); g.restore();
        }
        halo(CX, cy - 4, 26, 30, .8, .25);
        for (int side : { -1, 1 }) {
            auto bone = [&](double t) { return QPointF(CX + side * (4 + t * 30 * spread), cy - 6 - std::sin(t * PI * .8) * 16 + t * 4); };
            for (int r = 0; r < rows; ++r) {
                const int nf = 9 - r;
                const double len0 = 26 - r * 4.6;
                for (int j = nf - 1; j >= 0; --j) {
                    const double t = .08 + double(j) / (nf - 1) * .92;
                    const QPointF b = bone(t);
                    const double a = PI / 2 - side * (.15 + t * 1.15), len = len0 * (.55 + .55 * t);
                    const double fx = b.x() + std::cos(a) * len * .5, fy = b.y() + std::sin(a) * len * .5;
                    g.fillStyle = r == 0 ? P.shade(.58) : r == 1 ? P.glow(0, 0, .78, 1) : P.glow(10 * r, 0, .84 + r * .02, 1);
                    g.beginPath(); g.ellipse(fx, fy, len * .5, 2.6 + r * .4, a, 0, 7); g.fill();
                    line(b.x(), b.y(), b.x() + std::cos(a) * len * .9, b.y() + std::sin(a) * len * .9, P.shade(.3, .5), .4);
                }
            }
        }
        break;
    }
    case MsVignette::FerrisWheel: {  // roda-gigante: mais cabines e mais luzes acesas com as palavras
        const double cx = CX + R(-6, 6), cy = CY - 6, r = 32, rot = R(0, PI / 6);
        stars(jsRound(14 * wide), VH * .6);
        g.fillStyle = P.shade(.1); g.fillRect(0, VH - 9, VW, 9);
        const int tents = wide > 1.2 ? jsRound((wide - 1) * 4) : 0;
        for (int i = 0; i < tents; ++i) {
            const double x = CX + (i % 2 ? 1 : -1) * (46 + (i / 2) * 20), y = VH - 9;
            poly({ { x - 8, y }, { x, y - 12 }, { x + 8, y } }, i % 2 ? ink2 : P.glow(-40, 0, .55, 1)); line(x, y - 12, x, y - 15, ink, .6);
        }
        line(cx, cy, cx - 20, VH - 9, P.shade(.32), 2.2); line(cx, cy, cx + 20, VH - 9, P.shade(.32), 2.2);
        line(cx - 12, cy + 26, cx + 12, cy + 26, P.shade(.32), 1);
        g.strokeStyle = P.shade(.4); g.lineWidth = 1.2; g.beginPath(); g.arc(cx, cy, r, 0, 7); g.stroke();
        g.lineWidth = .6; g.beginPath(); g.arc(cx, cy, r * .82, 0, 7); g.stroke();
        for (int i = 0; i < 12; ++i) { const double a = rot + i * PI / 6; line(cx, cy, cx + std::cos(a) * r, cy + std::sin(a) * r, P.shade(.36), .5); }
        int perm[12];
        for (int i = 0; i < 12; ++i) perm[i] = i;
        for (int i = 11; i > 0; --i) { const int j = int(std::floor(rnd() * (i + 1))); std::swap(perm[i], perm[j]); }
        const int lights = 12 * (1 + jsRound(grow * 3));
        g.save(); g.shadowColor = P.glow(40, 0, .8, 1); g.shadowBlur = 3 * s * grow;
        for (int i = 0; i < lights; ++i) {
            const double a = rot + double(i) / lights * PI * 2;
            dot(cx + std::cos(a) * r, cy + std::sin(a) * r, .8, i % 2 ? ink2 : P.glow(40, 0, .82, 1));
        }
        g.restore();
        const int cabins = 4 + jsRound(grow * 8);
        const QColor cc[4] = { ink2, P.glow(60, 0, .62, 1), P.glow(-60, 0, .62, 1), ink };
        for (int k = 0; k < cabins; ++k) {
            const int i = perm[k];
            const double a = rot + i * PI / 6, x = cx + std::cos(a) * r, y = cy + std::sin(a) * r;
            line(x, y, x, y + 2.4, P.shade(.4), .6); g.fillStyle = cc[i % 4]; g.beginPath(); g.roundRect(x - 3.2, y + 2.4, 6.4, 5.2, 1.6); g.fill();
            g.fillStyle = P.glow(45, 0, .9, .9); g.fillRect(x - 2, y + 3.6, 4, 1.5);
        }
        dot(cx, cy, 2.6, P.shade(.45));
        break;
    }
    case MsVignette::Hill: {  // morro: as casinhas sobem o morro conforme o capítulo cresce
        const double hx = CX + R(-10, 10), sp = 34 * std::max(1.0, wide * .8), peak = 62;
        auto hy = [&](double x) { return VH - 10 - peak * std::exp(-std::pow((x - hx) / sp, 2)); };
        stars(jsRound(12 * wide), VH * .4);
        { const double mx = R(12, VW - 12), my = R(12, 22), mr = R(4, 6); dot(mx, my, mr, P.glow(40, 0, .86, .9)); }
        g.fillStyle = P.shade(.1); g.beginPath(); g.moveTo(0, VH);
        for (double x = 0; x <= VW; x += 2) g.lineTo(x, hy(x));
        g.lineTo(VW, VH); g.fill();
        struct House { double x, y, w, h, lit; int col; bool tank, ant; };
        QVector<House> C;
        for (int row = 0; row < 14; ++row) {
            const double y = VH - 5 - row * 5.6;
            for (double x = 1; x < VW - 2; x += R(6.5, 9)) {
                House hs;
                hs.x = x; hs.y = y; hs.w = R(4.5, 7); hs.h = R(4.6, 6.6); hs.col = int(std::floor(rnd() * 6)); hs.lit = rnd();
                hs.tank = rnd() < .22; hs.ant = rnd() < .12;
                if (y - hs.h > hy(x + hs.w / 2) - 1.5) C.append(hs);
            }
        }
        const int n = jsRound(C.size() * (.15 + grow * .85));
        const QColor cols[6] = { P.shade(.24), P.shade(.3), P.shade(.36), P.shade(.44), P.paper2, P.glow(30, 0, .5, .9) };
        const QVector<House> vis = C.mid(0, n);
        for (int i = vis.size() - 1; i >= 0; --i) {
            const House& c = vis[i];
            g.fillStyle = cols[c.col]; g.fillRect(c.x, c.y - c.h, c.w, c.h);
            g.fillStyle = P.shade(.04, .55); g.fillRect(c.x, c.y - c.h, c.w, .9); g.fillRect(c.x + c.w - .7, c.y - c.h, .7, c.h);
            if (c.lit < .45) { g.fillStyle = P.glow(40, 0, .84, 1); g.fillRect(c.x + c.w * .3, c.y - c.h * .62, 1.6, 1.8); }
            else if (c.lit < .75) { g.fillStyle = P.shade(.06); g.fillRect(c.x + c.w * .3, c.y - c.h * .62, 1.6, 1.8); }
            if (c.tank) { g.fillStyle = ink2; g.fillRect(c.x + c.w - 3.4, c.y - c.h - 2.2, 2.8, 2.2); }
            if (c.ant) line(c.x + 1.5, c.y - c.h, c.x + 1.5, c.y - c.h - 4, P.shade(.4), .4);
        }
        if (grow > .4 && vis.size() > 4) {
            const int wn = jsRound(grow * 5);
            g.strokeStyle = P.shade(.02, .6); g.lineWidth = .35;
            for (int i = 0; i < wn; ++i) {
                SubR r = sub(QStringLiteral("fio%1").arg(i));
                const House& a = vis[int(std::floor(r(0, vis.size())))];
                const double x1 = a.x + a.w / 2, y1 = a.y - a.h;
                const double dist = r(12, 26);
                const double sg = r(0, 1) < .5 ? -1 : 1;
                const double x2 = x1 + dist * sg, y2 = y1 + r(-3, 3);
                g.beginPath(); g.moveTo(x1, y1); g.quadraticCurveTo((x1 + x2) / 2, std::max(y1, y2) + 2.5, x2, y2); g.stroke();
            }
        }
        if (grow > .5) {
            SubR r = sub(QStringLiteral("pipa"));
            const double kx = r(14, VW - 14), ky = r(10, 28);
            const double ex = kx + r(-14, 14), ey = ky + r(22, 30);
            line(kx, ky + 4, ex, ey, P.glow(0, 0, .85, .3), .3);
            poly({ { kx, ky - 4 }, { kx + 3.2, ky }, { kx, ky + 4.5 }, { kx - 3.2, ky } }, P.glow(-40, 0, .6, 1));
            line(kx - 3.2, ky, kx + 3.2, ky, P.shade(.1), .3); line(kx, ky - 4, kx, ky + 4.5, P.shade(.1), .3);
            g.strokeStyle = P.glow(-40, 0, .7, .8); g.lineWidth = .5; g.beginPath(); g.moveTo(kx, ky + 4.5);
            for (int t = 1; t <= 10; ++t) g.lineTo(kx + std::sin(t * .9) * 1.6, ky + 4.5 + t * 1.2);
            g.stroke();
        }
        break;
    }
    case MsVignette::Train: {  // trem no viaduto: mais vagões atravessam conforme o capítulo cresce
        double mp = R(0, 6);
        g.fillStyle = P.shade(.2); g.beginPath(); g.moveTo(0, VH);
        for (double x = 0; x <= VW; x += 4) g.lineTo(x, CY - 8 - std::abs(std::sin(x * .035 + mp)) * 20 - std::sin(x * .11) * 4);
        g.lineTo(VW, VH); g.fill();
        mp = R(0, 6);
        g.fillStyle = P.shade(.15); g.beginPath(); g.moveTo(0, VH);
        for (double x = 0; x <= VW; x += 4) g.lineTo(x, CY + 4 - std::abs(std::sin(x * .05 + mp)) * 12);
        g.lineTo(VW, VH); g.fill();
        const double deck = CY + 16, span = 18, start = CX - span * std::ceil(CX / span);
        g.fillStyle = P.shade(.3); g.fillRect(0, deck, VW, 4);
        for (double x0 = start; x0 < VW; x0 += span) {
            const double x1 = x0 + span, mid = (x0 + x1) / 2, ar = span / 2 - 3;
            g.beginPath(); g.moveTo(x0, deck + 4); g.lineTo(x1, deck + 4); g.lineTo(x1, VH); g.lineTo(x1 - 3, VH); g.lineTo(x1 - 3, deck + 8 + ar);
            g.arc(mid, deck + 8 + ar, ar, 0, PI, true); g.lineTo(x0 + 3, VH); g.lineTo(x0, VH); g.closePath(); g.fill();
        }
        line(0, deck - .4, VW, deck - .4, P.shade(.45), .6);
        const double ty = deck - 1, lx = CX - 28 + R(-6, 6);
        const int nc = 1 + jsRound(grow * 6);
        for (int i = 0; i < nc; ++i) {
            const double x = lx + 23 + i * 17;
            g.fillStyle = i % 2 ? P.glow(20, 0, .42, 1) : P.shade(.24); g.fillRect(x, ty - 10, 15, 8);
            g.fillStyle = P.shade(.12); g.fillRect(x, ty - 11, 15, 1.4);
            for (int w = 0; w < 3; ++w) {
                // como no concept: o sorteio de cada janela recomeça do mesmo ponto (o vagão inteiro acende junto)
                g.fillStyle = (sub(QStringLiteral("j%1").arg(i))(0, 1) < .7 || w == 1) ? P.glow(45, 0, .82, 1) : P.shade(.1);
                g.fillRect(x + 2 + w * 4.4, ty - 8, 2.6, 2.6);
            }
            dot(x + 3, ty - 1.4, 1.3, P.shade(.06)); dot(x + 12, ty - 1.4, 1.3, P.shade(.06));
        }
        g.fillStyle = P.shade(.08); g.fillRect(lx + 2, ty - 8, 13, 6);
        poly({ { lx, ty - 1.5 }, { lx + 2.5, ty - 8 }, { lx + 2.5, ty - 1.5 } }, P.shade(.08));
        g.fillRect(lx + 13, ty - 13, 8, 11); g.fillRect(lx + 5, ty - 13, 3, 5); g.fillRect(lx + 12, ty - 14, 10, 1.4);
        g.fillStyle = P.glow(45, 0, .82, 1); g.fillRect(lx + 15, ty - 11, 3.5, 3);
        halo(lx + 1.5, ty - 6, 7, 40, .85, .6); dot(lx + 1.5, ty - 6, 1.1, P.glow(45, 0, .95, 1));
        for (double wx : { lx + 5, lx + 10, lx + 17 }) dot(wx, ty - 1.6, 1.6, P.shade(.04));
        const int puffs = 3 + jsRound(grow * 7);
        for (int k = 0; k < puffs; ++k)
            dot(lx + 6.5 + k * 4.4, ty - 16 - k * 1.4 - std::sin(k) * 1.5, 2 + k * .5, P.glow(0, 0, .9, std::max(.06, .5 - k * .045)));
        break;
    }
    case MsVignette::Guitar: {  // guitarra: cada faixa de palavras destrava um instrumento. Medidas reais em cm (escala, trastes pela
                                // regra 2^(n/12)); a madeira e o acabamento têm a cor de verdade, quem segue a cor da Parte é o cenário
        static const int kTiers[] = { 0, 300, 800, 1500, 2500, 4000, 6000, 9000, 12000 };
        int t = 0;
        for (int i = 0; i < 9; ++i) if (words >= kTiers[i]) t = i;
        using Stops = QVector<QPair<double, QColor>>;
        auto hex = [](const char* h) { return QColor(QString::fromLatin1(h)); };
        auto rgba = [](int r, int gg, int b, double a) { QColor c(r, gg, b); c.setAlphaF(a); return c; };
        auto crescent = [&](double mx, double my, double r) {
            g.save(); g.beginPath(); g.arc(mx, my, r, 0, 7); g.clip();
            g.fillStyle = P.glow(40, 0, .86, .95); g.beginPath(); g.arc(mx, my, r, 0, 7); g.arc(mx + r * .42, my - r * .25, r * .9, 0, 7); g.fill(true);
            g.restore();
        };
        auto lin = [](double x0, double y0, double x1, double y1, const Stops& st) { return MsCanvas::linear(x0, y0, x1, y1, st); };
        auto rad = [](double x, double y, double r0, double r1, const Stops& st) { return MsCanvas::radial(x, y, r0, x, y, r1, st); };
        auto radAt = [](double x, double y, double r0, double r1, const Stops& st, double x1, double y1) { return MsCanvas::radial(x, y, r0, x1, y1, r1, st); };
        auto pathPoly = [&](const Pts& pts) {
            g.beginPath();
            for (int i = 0; i < pts.size(); ++i) { if (i) g.lineTo(pts[i].x(), pts[i].y()); else g.moveTo(pts[i].x(), pts[i].y()); }
            g.closePath();
        };
        auto pathSmooth = [&](const Pts& pts) {
            const int n = pts.size();
            g.beginPath(); g.moveTo(pts[0].x(), pts[0].y());
            for (int i = 0; i < n; ++i) {
                const QPointF p0 = pts[(i - 1 + n) % n], p1 = pts[i], p2 = pts[(i + 1) % n], p3 = pts[(i + 2) % n];
                g.bezierCurveTo(p1.x() + (p2.x() - p0.x()) / 6, p1.y() + (p2.y() - p0.y()) / 6, p2.x() - (p3.x() - p1.x()) / 6, p2.y() - (p3.y() - p1.y()) / 6, p2.x(), p2.y());
            }
            g.closePath();
        };
        auto rr = [&](double x, double y, double w, double h, double r, const QBrush& c) { g.fillStyle = c; g.beginPath(); g.roundRect(x, y, w, h, r); g.fill(); };
        auto chrome = [&](double y0, double y1) { return lin(0, y0, 0, y1, { { 0, hex("#f6f7f9") }, { .45, hex("#a3a8af") }, { .55, hex("#d2d5da") }, { 1, hex("#5b6068") } }); };
        auto goldH = [&](double y0, double y1) { return lin(0, y0, 0, y1, { { 0, hex("#fdeaa8") }, { .5, hex("#c99c3c") }, { 1, hex("#7b5817") } }); };
        enum Knob { KGold, KAmber, KBlack, KChrome, KCream };
        auto knob = [&](double x, double y, double r, Knob kind) {
            Stops st;
            switch (kind) {
            case KGold:   st = { { 0, hex("#fbe2a0") }, { .6, hex("#c79233") }, { 1, hex("#6f4a12") } }; break;
            case KAmber:  st = { { 0, hex("#f7d38a") }, { .55, hex("#c98b2b") }, { 1, hex("#5a3510") } }; break;
            case KBlack:  st = { { 0, hex("#6a6a6a") }, { .5, hex("#1d1d1d") }, { 1, hex("#050505") } }; break;
            case KChrome: st = { { 0, hex("#ffffff") }, { .5, hex("#a9adb3") }, { 1, hex("#4f545b") } }; break;
            default:      st = { { 0, hex("#ffffff") }, { .6, hex("#ece6d6") }, { 1, hex("#a59c86") } }; break;
            }
            dot(x + .25, y + .3, r * 1.05, rgba(0, 0, 0, .35));
            dot(x, y, r, radAt(x - r * .35, y - r * .35, .05, r, st, x, y));
        };
        auto singleCoil = [&](double y, double a, const QColor& cover) {
            g.save(); g.translate(0, y); g.rotate(a); rr(-3.6, -.95, 7.2, 1.9, .95, cover);
            g.strokeStyle = rgba(0, 0, 0, .25); g.lineWidth = .12; g.beginPath(); g.roundRect(-3.6, -.95, 7.2, 1.9, .95); g.stroke();
            for (int i = 0; i < 6; ++i) dot((i - 2.5) * 1.04, 0, .2, hex("#9aa0a8"));
            g.restore();
        };
        auto humbucker = [&](double y, const QColor& ring, bool goldCov) {
            rr(-4.3, y - 2.45, 8.6, 4.9, .9, ring);
            rr(-3.6, y - 1.9, 7.2, 3.8, .45, goldCov ? goldH(y - 1.9, y + 1.9) : chrome(y - 1.9, y + 1.9));
            for (int i = 0; i < 6; ++i) dot((i - 2.5) * 1.04, y - .8, .22, goldCov ? hex("#8a6420") : hex("#6d737b"));
        };
        auto tom = [&](double y) {
            knob(-4.3, y, .62, KChrome); knob(4.3, y, .62, KChrome); rr(-4, y - .6, 8, 1.2, .35, chrome(y - .7, y + .7));
            for (int i = 0; i < 6; ++i) rr((i - 2.5) * 1.05 - .3, y - .45, .6, .9, .15, hex("#7c8189"));
        };
        auto stopbar = [&](double y) {
            knob(-4.1, y, .75, KChrome); knob(4.1, y, .75, KChrome); rr(-4.5, y - .75, 9, 1.5, .75, chrome(y - .75, y + .75));
        };
        auto binding = [&](const std::function<void()>& path, const QColor& c) {
            path(); g.strokeStyle = c; g.lineWidth = .55; g.stroke(); path(); g.strokeStyle = rgba(0, 0, 0, .55); g.lineWidth = .14; g.stroke();
        };
        auto edgeShade = [&](const std::function<void()>& path, double a) {
            g.save(); path(); g.clip(); path(); g.strokeStyle = rgba(0, 0, 0, a); g.lineWidth = 2.2; g.stroke(); g.restore();
        };
        auto edgeX = [](const Pts& poly, double y, int side) {
            double best = side < 0 ? 1e9 : -1e9;
            for (int i = 0; i < poly.size(); ++i) {
                const QPointF p1 = poly[i], p2 = poly[(i + 1) % poly.size()];
                if ((p1.y() <= y && p2.y() > y) || (p2.y() <= y && p1.y() > y)) {
                    const double x = p1.x() + (y - p1.y()) / (p2.y() - p1.y()) * (p2.x() - p1.x());
                    best = side < 0 ? std::min(best, x) : std::max(best, x);
                }
            }
            return best;
        };
        auto inPoly = [](const Pts& poly, double x, double y) {
            bool c = false;
            for (int i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
                const QPointF pi = poly[i], pj = poly[j];
                if ((pi.y() > y) != (pj.y() > y) && x < (pj.x() - pi.x()) * (y - pi.y()) / (pj.y() - pi.y()) + pi.x()) c = !c;
            }
            return c;
        };
        // tarraxas: os botões aparecem na silhueta como calombos na borda da cabeça; acha os mais salientes
        struct Bump { double y, x, v; };
        auto bumps = [&](const Pts& poly, int side, int n) {
            QVector<double> ys, xs;
            double ymin = 1e9;
            for (const QPointF& p : poly) ymin = std::min(ymin, p.y());
            for (double y = -.8; y > ymin + .8; y -= .12) {
                const double x = edgeX(poly, y, side);
                if (std::abs(x) < 1e8) { ys << y; xs << x * side; }
            }
            QVector<Bump> pk;
            for (int i = 3; i < xs.size() - 3; ++i)
                if (xs[i] >= std::max({ xs[i - 1], xs[i - 2], xs[i - 3], xs[i + 1], xs[i + 2], xs[i + 3] })) pk << Bump{ ys[i], xs[i] * side, xs[i] };
            std::stable_sort(pk.begin(), pk.end(), [](const Bump& a, const Bump& b) { return a.v > b.v; });
            QVector<Bump> sel;
            for (const Bump& p : pk) {
                bool far = true;
                for (const Bump& q : sel) if (std::abs(q.y - p.y) <= 1.3) far = false;
                if (far) sel << p;
                if (sel.size() == n) break;
            }
            if (sel.size() < n) {
                sel.clear();
                for (int i = 0; i < n; ++i) { const double y = -3.4 - i * (-ymin - 6.5) / std::max(1, n - 1); sel << Bump{ y, edgeX(poly, y, side), 0 }; }
            }
            std::stable_sort(sel.begin(), sel.end(), [](const Bump& a, const Bump& b) { return a.y > b.y; });
            return sel;
        };
        auto toPts = [](const double* d, int n) { Pts p; p.reserve(n); for (int i = 0; i < n; ++i) p << QPointF(d[i * 2], d[i * 2 + 1]); return p; };
        const MsGuitarOutlines::Outline O = MsGuitarOutlines::get(t);
        const Pts body = toPts(O.body, O.nBody), head = toPts(O.head, O.nHead);
        const double bc = (O.bodyTop + O.bottom) / 2;

        // as fichas de cada instrumento (mesmos números do concept)
        enum Inlay { INone, IDots, IBlack, ITrap, IBlock };
        struct Spec {
            double L, boardEnd, nutW, jw; QColor board; Inlay inlay; bool head3; QColor headC; bool slotHead; QColor wood; double sp;
            bool nylon; double anchor; QColor keyC; bool gold; QColor grainC; QColor bind; bool bolt, maple; QColor guardC; bool guardEdge;
            bool flame, flameHead; double edgeA; bool pearl;
        };
        const QColor none;
        const Spec SPECS[9] = {
            { 65, 41.8, 5.2, 6, hex("#2b1a12"), INone, true, hex("#6a3a1a"), true, hex("#7a4a26"), 1.15, true, 66.8, hex("#f1ead8"), true,
              rgba(120, 70, 20, .16), hex("#4a2a16"), false, false, none, false, false, false, .3, false },
            { 64.5, 41.4, 4.4, 5.5, hex("#24160f"), IDots, true, hex("#17110c"), false, hex("#6b3d1f"), 1.15, false, 66.2, none, false,
              rgba(60, 25, 5, .16), hex("#f2ead6"), false, false, none, false, false, false, .3, false },
            { 71.7, 51.4, 4.2, 5.6, hex("#e6c48a"), IBlack, false, none, false, hex("#e2bf85"), 1.08, false, 73.6, none, false,
              rgba(110, 60, 15, .22), none, true, true, hex("#141414"), false, false, false, .3, false },
            { 69.6, 49.6, 4.2, 5.6, hex("#33200f"), IDots, false, none, false, hex("#e2bf85"), 1.06, false, 71, none, false,
              rgba(60, 20, 5, .14), none, true, false, hex("#f3efe4"), true, false, false, .3, false },
            { 68.2, 50.8, 4.3, 5.6, hex("#2a180f"), ITrap, true, hex("#14110f"), false, hex("#5a2a14"), 1.05, false, 72.6, none, false,
              rgba(255, 220, 150, .07), hex("#efe4c4"), false, false, hex("#f0e6c8"), false, true, false, .3, false },
            { 64.1, 46.8, 4.3, 5.6, hex("#241510"), IBlock, true, hex("#14110f"), false, hex("#7a1e12"), 1.05, false, 68.6, none, false,
              rgba(255, 180, 150, .06), hex("#efe4c4"), false, false, hex("#141414"), false, false, false, .3, false },
            { 62.9, 45.6, 4.3, 5.6, hex("#241510"), IDots, false, hex("#16120f"), false, hex("#c99a4c"), 1.05, false, 67.3, none, false,
              rgba(110, 60, 15, .2), none, false, false, none, false, false, false, .3, false },
            { 66, 42.6, 4.3, 5.6, hex("#241510"), IDots, true, hex("#14110f"), false, hex("#e9e4d6"), 1.05, false, 72, none, false,
              none, none, false, false, hex("#111111"), false, false, false, .3, false },
            { 64.8, 44.9, 4.2, 5.6, hex("#211712"), IDots, false, none, false, hex("#e4c48c"), 1.06, false, 66.6, none, true,
              none, none, true, false, none, false, false, true, .07, true },
        };
        const Spec& S = SPECS[t];
        Pts guard = O.nGuard ? toPts(O.guard, O.nGuard) : Pts();
        if (t == 5) guard = { { 4.4, 47.3 }, { 6.4, 44.9 }, { 9, 44.1 }, { 11, 45.7 }, { 11.4, 49.5 }, { 10.6, 53.9 }, { 8.6, 56.7 }, { 6.2, 56.9 }, { 4.8, 53.7 } };
        if (S.pearl && guard.isEmpty()) { const double cy = 58; for (const QPointF& p : body) guard << QPointF(1 + (p.x() - 1) * .82, std::min(66.5, cy + (p.y() - cy) * .82)); }
        auto finish = [&]() -> QBrush {
            switch (t) {
            case 0: return lin(-18, 36, 18, 84, { { 0, hex("#efd59e") }, { 1, hex("#d4a560") } });
            case 1: return rad(0, bc + 5, .5, 28, { { 0, hex("#f0bf66") }, { .55, hex("#b9651f") }, { .84, hex("#4a210f") }, { 1, hex("#1d0b04") } });
            case 2: return lin(-16, 46, 16, 87, { { 0, hex("#f2c977") }, { .5, hex("#e0a447") }, { 1, hex("#b5752a") } });
            case 3: return rad(0, bc + 4, .5, 21, { { 0, hex("#f1b142") }, { .5, hex("#c04a19") }, { .78, hex("#4a160a") }, { 1, hex("#140603") } });
            case 4: return rad(1, bc + 5, .5, 21, { { 0, hex("#f4bb45") }, { .45, hex("#d8701f") }, { .72, hex("#9b1c14") }, { 1, hex("#3a0605") } });
            case 5: return rad(0, bc, .5, 25, { { 0, hex("#d8463a") }, { .55, hex("#9c1f15") }, { 1, hex("#3a0604") } });
            case 6: return lin(-26, 38, 26, 88, { { 0, hex("#ecc983") }, { .5, hex("#d6a85a") }, { 1, hex("#a87430") } });
            case 7: return lin(-18, 44, 18, 87, { { 0, hex("#fbf8f0") }, { .6, hex("#ece6d6") }, { 1, hex("#c9c1ab") } });
            default: return rad(-3, bc - 8, 1, 46, { { 0, hex("#fbf8e6") }, { .7, hex("#f2edd2") }, { 1, hex("#e4ddbc") } });
            }
        };
        // botão de volume/tom: se cair fora do corpo, anda pra dentro até caber
        auto kn = [&](double x, double y, double r, Knob kind) {
            double xx = x;
            for (int i = 0; i < 40 && !inPoly(body, xx, y); ++i) xx -= (xx > 0 ? 1 : xx < 0 ? -1 : 0) * .6;
            knob(xx, y, r, kind);
        };
        auto parts = [&]() {
            switch (t) {
            case 0: {
                const double sh = 46.2;
                dot(0, sh, 4.2, rad(0, sh, .5, 4.2, { { 0, hex("#3a2616") }, { 1, hex("#0d0805") } }));
                const QColor rose[3] = { hex("#6e3f22"), hex("#e7d09a"), hex("#2f6a4a") };
                for (int kk = 0; kk < 36; ++kk) {
                    const double a = kk / 36.0 * PI * 2;
                    g.fillStyle = rose[kk % 3]; g.beginPath(); g.arc(0, sh, 6, a, a + PI / 18); g.arc(0, sh, 5, a + PI / 18, a, true); g.fill();
                }
                g.strokeStyle = hex("#2a170c"); g.lineWidth = .22;
                for (double r2 : { 4.5, 4.85, 6.25, 6.6 }) { g.beginPath(); g.arc(0, sh, r2, 0, 7); g.stroke(); }
                rr(-9, 63.1, 18, 3.6, 1.2, hex("#3a2116")); rr(-4.4, 65.5, 8.8, 1.6, .4, hex("#51301e")); rr(-4, 64.7, 8, .42, .2, hex("#f2ecdc"));
                break;
            }
            case 1: {
                const double sh = 46.9;
                dot(0, sh, 5, rad(0, sh, .5, 5, { { 0, hex("#3a2616") }, { 1, hex("#0d0805") } }));
                g.strokeStyle = hex("#f0e6cf"); g.lineWidth = .35;
                for (double r2 : { 5.6, 6.5, 6.9 }) { g.beginPath(); g.arc(0, sh, r2, 0, 7); g.stroke(); }
                const Pts pg = { { 5.2, 42.9 }, { 9.4, 44.1 }, { 11.8, 49.3 }, { 10.8, 56.1 }, { 7, 59.3 }, { 4.6, 55.3 }, { 5.6, 50.1 } };
                g.save(); pathSmooth(pg); g.clip(); g.fillStyle = hex("#2c140a"); g.fillRect(0, 40, 16, 24);
                SubR r = sub(QStringLiteral("tort"));
                for (int kk = 0; kk < 30; ++kk) {
                    const double x = r(4, 12), y = r(42, 60), rad2 = r(.4, 1.5), a = r(.25, .6);
                    dot(x, y, rad2, rgba(170, 90, 30, a));
                }
                g.restore();
                const double by = 64.5;
                g.fillStyle = hex("#2a170e"); g.beginPath(); g.moveTo(-8.4, by - .1); g.quadraticCurveTo(-7.4, by - 2.5, -4, by - 2.2); g.lineTo(4, by - 2.2);
                g.quadraticCurveTo(7.4, by - 2.5, 8.4, by - .1); g.quadraticCurveTo(7.4, by + 2.9, 4, by + 2.6); g.lineTo(-4, by + 2.6);
                g.quadraticCurveTo(-7.4, by + 2.9, -8.4, by - .1); g.fill();
                g.save(); g.translate(0, by); g.rotate(-.05); rr(-3.6, -.25, 7.2, .5, .2, hex("#f2ecdc")); g.restore();
                for (int i = 0; i < 6; ++i) dot((i - 2.5) * 1.15, by + 1.6, .42, hex("#efe8d6"));
                break;
            }
            case 2:
                rr(-3.7, 54, 7.4, 2.4, .5, chrome(54, 56.4));
                rr(-4.6, 63.8, 9.2, 10.4, .6, chrome(63.8, 74.2)); rr(-4.1, 64.4, 8.2, 9.2, .4, hex("#c4c8ce")); singleCoil(67.9, -.16, hex("#20201f"));
                for (int kk = 0; kk < 3; ++kk) rr(-3.4 + kk * 2.35, 71.2, 2.1, 1, .45, chrome(71.2, 72.2));
                g.save(); g.translate(11.6, 74); g.rotate(1.05); rr(-5, -1.2, 10, 2.4, 1.2, chrome(-1.2, 1.2)); g.restore();
                kn(10.4, 71.9, .95, KChrome); kn(12.8, 76.1, .95, KChrome); dot(9.2, 69.6, .45, hex("#111111"));
                break;
            case 3:
                singleCoil(51.25, 0, hex("#f1ede2")); singleCoil(58.45, 0, hex("#f1ede2")); singleCoil(65, -.17, hex("#f1ede2"));
                rr(-3.9, 67.6, 7.8, 4.4, .4, chrome(67.6, 72));
                for (int i = 0; i < 6; ++i) rr((i - 2.5) * 1.06 - .38, 68.7, .76, 1.8, .2, chrome(68.7, 70.5));
                g.strokeStyle = chrome(68, 78); g.lineWidth = .38; g.beginPath(); g.moveTo(3.9, 70.6); g.quadraticCurveTo(7, 72.2, 8.6, 77.6); g.stroke();
                kn(6.6, 63.8, 1, KCream); kn(9, 67.8, 1, KCream); kn(10.2, 72.2, 1, KCream);
                g.save(); g.translate(9.6, 59.4); g.rotate(-.5); rr(-.9, -.25, 1.8, .5, .2, hex("#222222")); rr(.1, -.5, .9, 1, .3, hex("#f1ede2")); g.restore();
                g.fillStyle = chrome(69, 72); g.beginPath(); g.ellipse(13, 70, .9, 1.6, -.6, 0, 7); g.fill();
                break;
            case 4:
                humbucker(53.8, hex("#efe6cc"), false); humbucker(64, hex("#efe6cc"), false); tom(68.2); stopbar(72.6);
                kn(7.2, 73.4, 1.2, KAmber); kn(12, 71.6, 1.2, KAmber); kn(8.8, 78.4, 1.2, KAmber); kn(13.4, 76.6, 1.2, KAmber);
                dot(-9.6, 49, 1.3, hex("#efe6cc")); dot(-9.6, 49, .5, hex("#c8ccd2"));
                break;
            case 5: {
                auto fh = [&](double x) {
                    const double s2 = x > 0 ? 1 : x < 0 ? -1 : 0;
                    g.strokeStyle = hex("#140605"); g.lineWidth = .9; g.beginPath(); g.moveTo(x + 1.8 * s2, 57.8);
                    g.bezierCurveTo(x + 4.4 * s2, 62, x - 4.4 * s2, 66.6, x - 1.8 * s2, 70.8); g.stroke();
                    dot(x + 1.8 * s2, 57.8, .8, hex("#140605")); dot(x - 1.8 * s2, 70.8, .8, hex("#140605")); line(x - 1.6, 64, x + 1.6, 64.6, hex("#140605"), .3);
                };
                fh(-10.5); fh(10.5); humbucker(49.7, hex("#111111"), false); humbucker(59.9, hex("#111111"), false); tom(64.1); stopbar(68.6);
                kn(9.6, 74.6, 1.2, KAmber); kn(14.2, 72.6, 1.2, KAmber); kn(11.2, 79.4, 1.2, KAmber); kn(15.4, 77.4, 1.2, KAmber);
                dot(12.6, 51.6, 1.2, hex("#111111")); dot(12.6, 51.6, .45, hex("#c8ccd2"));
                break;
            }
            case 6:
                humbucker(48.3, hex("#111111"), false); humbucker(58.7, hex("#111111"), false); tom(62.9); stopbar(67.3);
                kn(-9, 70, 1.15, KBlack); kn(-12.6, 73.6, 1.15, KBlack); kn(-16.2, 77.2, 1.15, KBlack); dot(9.4, 47, 1.1, hex("#111111"));
                break;
            case 7:
                humbucker(45.5, hex("#111111"), false); humbucker(61.85, hex("#111111"), false); tom(66);
                g.fillStyle = chrome(69, 75); g.beginPath(); g.moveTo(-4.6, 69.4); g.lineTo(4.6, 69.4); g.lineTo(2.6, 74.4); g.lineTo(-2.6, 74.4); g.closePath(); g.fill();
                kn(11.4, 72.6, 1.1, KBlack); kn(14.2, 77.4, 1.1, KBlack); kn(11.2, 80.4, 1.1, KBlack); dot(-11, 74, 1.1, hex("#111111"));
                break;
            default: {  // Tagima TW-61 do Pe; cada peça no lugar medido na foto dela (já endireitada: o braço estava 2,7° torto na foto)
                const auto& Q = MsGuitarOutlines::kLuciana;
                auto mh = [&](double y) {
                    rr(-4.7, y - 2, 9.4, 4, .5, hex("#4a2c18")); rr(-3.9, y - 1.5, 7.8, 3, .5, goldH(y - 1.5, y + 1.5)); line(0, y - 1.4, 0, y + 1.4, hex("#8a6420"), .12);
                    for (int i = 0; i < 6; ++i) { dot((i - 2.5) * 1.05, y - .7, .24, hex("#7a5716")); dot((i - 2.5) * 1.05, y + .7, .24, hex("#7a5716")); }
                };
                mh(Q.neckPU); mh(Q.bridgePU);
                const double by = Q.bridge;
                rr(-4.4, by - 2.6, 8.8, 4.6, .6, goldH(by - 2.6, by + 2));
                for (int i = 0; i < 6; ++i) { const double x = (i - 2.5) * 1.06; rr(x - .42, by - 1.9, .84, 2.9, .2, goldH(by - 1.9, by + 1)); line(x, by + 1, x, by + 1.8, hex("#6d4d14"), .2); }
                const double ax = Q.armX, ay = Q.armY, ex = Q.armTipX, ey = Q.armTipY;
                g.strokeStyle = goldH(ey, ay); g.lineWidth = .42; g.beginPath(); g.moveTo(ax, ay); g.lineTo(ax + .9, ay - 1.4); g.lineTo(ex, ey); g.stroke();
                dot(ax, ay, .7, goldH(ay - .6, ay + .6));
                const double ks[3][2] = { { Q.k1x, Q.k1y }, { Q.k2x, Q.k2y }, { Q.k3x, Q.k3y } };
                for (const auto& kp : ks) {
                    const double x = kp[0], y = kp[1];
                    kn(x, y, 1.2, KGold); g.strokeStyle = rgba(90, 60, 10, .6); g.lineWidth = .08;
                    for (int k2 = 0; k2 < 14; ++k2) {
                        const double a2 = k2 / 14.0 * PI * 2;
                        g.beginPath(); g.moveTo(x + std::cos(a2) * .8, y + std::sin(a2) * .8); g.lineTo(x + std::cos(a2) * 1.15, y + std::sin(a2) * 1.15); g.stroke();
                    }
                }
                const double tx = Q.toggleX, ty = Q.toggleY;
                dot(tx, ty, 1.6, hex("#efe6cc")); g.strokeStyle = rgba(0, 0, 0, .25); g.lineWidth = .1; g.beginPath(); g.arc(tx, ty, 1.6, 0, 7); g.stroke();
                line(tx, ty, tx - .5, ty - 1.2, goldH(ty - 1.4, ty + .4), .35); dot(tx - .5, ty - 1.2, .28, hex("#f4dc8c"));
                const double jx = Q.jackX, jy = Q.jackY;
                dot(jx, jy, .95, goldH(jy - .9, jy + .9)); dot(jx, jy, .45, hex("#120e08")); knob(Q.strapX, Q.strapY, .55, KGold);
                break;
            }
            }
        };
        const double saddleY = S.L, boardEnd = S.boardEnd;
        auto fretY = [&](double n) { return S.L * (1 - std::pow(2, -n / 12)); };
        auto bodyPath = [&]() { pathPoly(body); };
        auto headPath = [&]() { pathPoly(head); };
        double bTop = 1e9, bottomY = -1e9, topY = 1e9;
        for (const QPointF& p : body) { bTop = std::min(bTop, p.y()); bottomY = std::max(bottomY, p.y()); }
        for (const QPointF& p : head) topY = std::min(topY, p.y());
        const double len = bottomY - topY;
        // onde e como: encostada, levemente inclinada, com o pé no chão
        const double floorY = VH - 9;
        const double a = .2 + R(-.04, .04), k = 93 / len;   // a guitarra inteira no quadro, cabeça incluída: é ela que diz o modelo
        const double gx0 = CX + R(-4, 4) + ((topY + bottomY) / 2) * k * std::sin(a), gy0 = floorY - bottomY * k * std::cos(a);

        // cenário
        auto wall = [&](double l) { g.fillStyle = P.shade(l); g.fillRect(0, 0, VW, floorY); };
        auto floorW = [&](double l) { g.fillStyle = P.shade(l); g.fillRect(0, floorY, VW, VH - floorY); g.fillStyle = P.shade(0, .35); g.fillRect(0, floorY, VW, 1); };
        auto amp = [&](double x, double w, double h, const QColor& tol, const QColor& grille, bool top) {
            const double y = floorY - h;
            rr(x + 1, y + 1, w, h, 1.4, P.shade(0, .4)); rr(x, y, w, h, 1.4, tol); rr(x + 1.6, y + 1.6 + (top ? 3.4 : 0), w - 3.2, h - 3.2 - (top ? 3.4 : 0), .8, grille);
            if (top) { rr(x + 1.6, y + 1.2, w - 3.2, 2.2, .4, P.shade(.1)); for (int i = 0; i < 5; ++i) dot(x + 3.4 + i * (w - 6.8) / 4, y + 2.3, .5, P.paper2); }
        };
        switch (t) {
        case 0: {  // violão clássico: quarto à noite, janela, estante de partitura
            wall(.18);
            for (double x = 3; x < VW; x += 6) line(x, 0, x, floorY, P.shade(.15, .6), .6);
            const double side = rnd() < .5 ? -1 : 1;
            const double wx = CX + side * R(26, 34), wy = 18;
            halo(wx, wy + 10, 40, 0, .85, .25); rr(wx - 12, wy - 2, 24, 26, 1, P.shade(.06)); rr(wx - 10.6, wy - .6, 21.2, 23.2, .6, P.glow(0, 0, .7, .55));
            line(wx, wy - .6, wx, wy + 22.6, P.shade(.06), 1); line(wx - 10.6, wy + 11, wx + 10.6, wy + 11, P.shade(.06), 1);
            crescent(wx + 4, wy + 6, 3.4);
            g.save(); g.globalAlpha = .16; poly({ { wx - 10, wy + 22 }, { wx + 10, wy + 22 }, { wx + 26, floorY + 10 }, { wx + 2, floorY + 10 } }, P.glow(0, 0, .9, 1)); g.restore();
            floorW(.1);
            for (double y = floorY + 3; y < VH; y += 3.4) line(0, y, VW, y, P.shade(.06, .6), .4);
            const double sx = wx < CX ? CX + 30 : CX - 30;
            line(sx, floorY, sx, floorY - 28, P.shade(.04), .9); line(sx - 5, floorY, sx, floorY - 6, P.shade(.04), .7); line(sx + 5, floorY, sx, floorY - 6, P.shade(.04), .7);
            g.save(); g.translate(sx, floorY - 32); g.rotate(-.12); rr(-8, -6, 16, 10, .6, P.shade(.04)); rr(-7, -5.4, 14, 8.6, .3, P.paper);
            for (int kk = 0; kk < 4; ++kk) for (int j = 0; j < 5; ++j) line(-6, -4 + kk * 2, 6, -4 + kk * 2, P.paperInk, .12);
            g.restore();
            const int pg = jsRound(grow * 3);
            for (int i = 0; i < pg; ++i) {
                SubR r = sub(QStringLiteral("pt%1").arg(i));
                const double px = r(6, VW - 6), py = floorY + r(3, 8), pa = r(-.5, .5);
                g.save(); g.translate(px, py); g.rotate(pa); rr(-4, -2.6, 8, 5.2, .3, P.paper); g.restore();
            }
            break;
        }
        case 1: {  // violão de aço: fogueira na praia, à noite
            stars(jsRound(26 * wide), VH * .5);
            const double hz = VH * .52;
            g.fillStyle = P.shade(.12); g.fillRect(0, hz, VW, floorY - hz);
            const double mx = CX + R(-30, 30), my = R(12, 22);
            dot(mx, my, 4, P.glow(40, 0, .88, .95));
            for (int kk = 0; kk < 6; ++kk) line(mx - 3 + kk * .4, hz + 2 + kk * 2.6, mx + 3 - kk * .4, hz + 2 + kk * 2.6, P.glow(40, 0, .85, .4), .6);
            g.fillStyle = P.glow(30, 0, .36, 1); g.beginPath(); g.moveTo(0, VH); g.lineTo(0, floorY - 3); g.quadraticCurveTo(CX, floorY - 7, VW, floorY - 2); g.lineTo(VW, VH); g.fill();
            const double side = rnd() < .5 ? -1 : 1;
            const double fx = CX + side * R(28, 34);
            halo(fx, floorY - 6, 40, 40, .7, .55);
            line(fx - 7, floorY - 1, fx + 6, floorY - 4, hex("#3a2414"), 2.2); line(fx - 6, floorY - 4, fx + 7, floorY - 1, hex("#2e1c10"), 2.2);
            flame(fx, floorY - 4, 13 + grow * 5, 4.4); flame(fx - 3, floorY - 3, 8, 2.6); flame(fx + 3.2, floorY - 3, 9, 2.8);
            const int sp = jsRound(4 + grow * 14);
            for (int i = 0; i < sp; ++i) {
                SubR r = sub(QStringLiteral("fa%1").arg(i));
                const double x = fx + r(-8, 8), y = floorY - r(14, 46), rr2 = r(.3, .7);
                dot(x, y, rr2, P.glow(45, 0, .85, 1));
            }
            break;
        }
        case 2: {  // telecaster: palco de bar, tijolo e néon
            wall(.16);
            for (double y = 0; y < floorY; y += 4.4) {
                const int off = jsRound(y / 4.4) % 2 ? 5 : 0;
                for (double x = -10 + off; x < VW; x += 10) { g.strokeStyle = P.shade(.1); g.lineWidth = .5; g.strokeRect(x, y, 10, 4.4); }
            }
            const double side = rnd() < .5 ? -1 : 1;
            const double nx = CX + side * R(22, 30);
            g.save(); g.shadowColor = ink2; g.shadowBlur = 6 * s; g.strokeStyle = ink2; g.lineWidth = 1.1;
            g.setFont(QStringLiteral("Georgia"), 9, QFont::Bold); g.textAlign = MsCanvas::Center; g.strokeText(QStringLiteral("LIVE"), nx, 24);
            g.strokeStyle = P.glow(-50, 0, .75, 1); g.shadowColor = P.glow(-50, 0, .75, 1); g.beginPath();
            for (int i = 0; i <= 10; ++i) { const double an = -PI / 2 + i / 10.0 * PI * 2, r2 = i % 2 ? 2.6 : 6; g.lineTo(nx + std::cos(an) * r2, 36 + std::sin(an) * r2); }
            g.stroke(); g.restore();
            floorW(.08);
            const double ax = nx < CX ? CX + 18 : CX - 38;
            amp(ax, 20, 17, hex("#c9a86a"), hex("#6e5a3c"), true);
            const int bt = jsRound(grow * 4);
            for (int i = 0; i < bt; ++i) {
                const double bx = nx + (i - 1.5) * 4.4, by = 46;
                rr(bx - 1, by - 6, 2, 6, .6, P.glow(30 * i, 0, .5, .8)); rr(bx - .4, by - 8, .8, 2.4, .3, P.glow(30 * i, 0, .5, .8));
            }
            if (bt) line(nx - 10, 46.4, nx + 10, 46.4, P.shade(.3), .8);
            break;
        }
        case 3: {  // stratocaster: garagem
            wall(.22);
            for (double x = 0; x < VW; x += 3) line(x, 0, x, floorY, P.shade(.17), .8);
            const double bx = CX + R(-20, 20);
            line(bx, 0, bx, 14, P.shade(.04), .5); halo(bx, 17, 46, 40, .85, .4); dot(bx, 17, 2.4, P.glow(40, 0, .95, 1));
            const int np = 1 + jsRound(grow * 3);
            for (int i = 0; i < np; ++i) {
                SubR r = sub(QStringLiteral("ps%1").arg(i));
                const double px = CX + (i % 2 ? 1 : -1) * r(24, 38) * std::sqrt(wide), py = r(18, 34);
                g.save(); g.translate(px, py); const double ra = r(-.08, .08); g.rotate(ra);
                rr(-7, -9, 14, 18, .4, P.paper); const double h1 = r(-60, 60); rr(-6, -8, 12, 10, .2, P.glow(h1, 0, .45, 1));
                const double h2 = r(-60, 60); dot(0, -3, 2.6, P.glow(h2, 0, .75, 1)); rr(-5, 4, 10, 1.2, .2, P.paperInk); g.restore();
            }
            floorW(.14);
            for (int kk = 0; kk < 3; ++kk) { g.strokeStyle = P.shade(.02); g.lineWidth = .7; g.beginPath(); g.ellipse(CX + 30, floorY + 5, 6 - kk * 1.4, 2 - kk * .4, 0, 0, 7); g.stroke(); }
            amp(CX - 44, 22, 20, hex("#191919"), hex("#9a9a96"), true);
            break;
        }
        case 4: {  // les paul: palco com paredão de caixas
            wall(.08);
            auto cab = [&](double x, double y, double w, double h) {
                rr(x, y, w, h, .8, hex("#111111")); rr(x + 1, y + 1, w - 2, h - 2, .4, hex("#2a2a2a"));
                g.fillStyle = rgba(220, 220, 210, .12);
                for (double yy = y + 1.6; yy < y + h - 1.6; yy += 1.2)
                    for (double xx = x + 1.6 + (jsRound(yy) % 2) * .6; xx < x + w - 1.6; xx += 1.2) g.fillRect(xx, yy, .5, .5);
                g.strokeStyle = hex("#d8d2c0"); g.lineWidth = .4; g.strokeRect(x + .6, y + .6, w - 1.2, h - 1.2); rr(x + w / 2 - 3.4, y + 2, 6.8, 1.8, .3, hex("#c9a24a"));
            };
            const double cw = 26;
            const int n = int(std::ceil(VW / cw)) + 1;
            for (int i = 0; i < n; ++i) {
                const double x = CX - cw / 2 + (i - n / 2) * cw;
                cab(x, floorY - 24, cw - 1, 24); cab(x, floorY - 48, cw - 1, 24);
                rr(x + 2, floorY - 58, cw - 5, 9, .8, hex("#111111")); rr(x + 3, floorY - 56, cw - 7, 1.6, .3, hex("#c9a24a"));
                for (int kk = 0; kk < 5; ++kk) dot(x + 5 + kk * 3.6, floorY - 52, .7, hex("#dddddd"));
            }
            const int sp = 1 + jsRound(grow * 2);
            for (int i = 0; i < sp; ++i) {
                const double x = CX + (i - (sp - 1) / 2.0) * 30;
                g.save(); g.globalAlpha = .22;
                poly({ { x - 2, 0 }, { x + 2, 0 }, { x + 18, floorY }, { x - 18, floorY } }, lin(0, 0, 0, floorY, { { 0, P.glow(40, 0, .9, 1) }, { 1, P.glow(40, 0, .9, 0) } }));
                g.restore();
            }
            floorW(.03);
            g.save(); g.translate(CX - 30, floorY + 5); g.rotate(-.2); rr(-5, -3, 10, 6, .3, P.paper); g.restore();
            line(CX + 24, floorY + 2, CX + 32, floorY + 8, P.glow(0, 0, .85, .6), 1.2); line(CX + 32, floorY + 2, CX + 24, floorY + 8, P.glow(0, 0, .85, .6), 1.2);
            break;
        }
        case 5: {  // es-335: clube de jazz, cortina e microfone antigo
            const double cw = 7;
            for (double x = -cw; x < VW + cw; x += cw) {
                g.fillStyle = lin(x, 0, x + cw, 0, { { 0, P.glow(-40, 0, .16, 1) }, { .5, P.glow(-40, 0, .34, 1) }, { 1, P.glow(-40, 0, .14, 1) } });
                g.fillRect(x, 0, cw, floorY);
            }
            halo(CX, floorY, 46, 30, .85, .4); floorW(.06);
            g.fillStyle = P.glow(40, 0, .8, .18); g.beginPath(); g.ellipse(CX, floorY + 5, 40, 7, 0, 0, 7); g.fill();
            const double side = rnd() < .5 ? -1 : 1;
            const double mx = CX + side * R(30, 36);
            line(mx, floorY, mx, floorY - 44, P.shade(.5), .8); line(mx - 6, floorY, mx + 6, floorY, P.shade(.5), 1);
            rr(mx - 2.6, floorY - 54, 5.2, 9, 2.4, chrome(-54 + floorY, -45 + floorY));
            for (int kk = 0; kk < 5; ++kk) line(mx - 2.2, floorY - 52.6 + kk * 1.6, mx + 2.2, floorY - 52.6 + kk * 1.6, hex("#555555"), .25);
            const int bk = jsRound((4 + grow * 12) * wide);
            for (int i = 0; i < bk; ++i) {
                SubR r = sub(QStringLiteral("bk%1").arg(i));
                const double x = r(0, VW), y = r(4, 40), rr2 = r(1.4, 3.4), hu = r(-40, 40);
                dot(x, y, rr2, P.glow(hu, 0, .75, .2));
            }
            break;
        }
        case 6: {  // explorer: palco de metal, bateria, fumaça e canhões de luz
            wall(.05);
            const double kx = CX + R(-6, 6), ky = floorY - 12;
            dot(kx, ky, 12, hex("#141414")); g.strokeStyle = P.shade(.4); g.lineWidth = .8; g.beginPath(); g.arc(kx, ky, 12, 0, 7); g.stroke(); dot(kx, ky, 6, P.glow(-40, 0, .4, .8));
            const double cym[4][3] = { { kx - 22, ky - 16, 9 }, { kx + 22, ky - 18, 10 }, { kx - 12, ky - 26, 7 }, { kx + 14, ky - 28, 8 } };
            for (const auto& c : cym) {
                g.fillStyle = P.glow(40, 0, .62, .9); g.beginPath(); g.ellipse(c[0], c[1], c[2], 1.4, c[0] < kx ? .2 : -.2, 0, 7); g.fill();
                line(c[0], c[1], c[0], floorY, P.shade(.4), .4);
            }
            const int nb = 2 + jsRound(grow * 4);
            for (int i = 0; i < nb; ++i) {
                SubR r = sub(QStringLiteral("bm%1").arg(i));
                const double x = r(0, VW), hu = r(-60, 60);
                const QColor c = P.glow(hu, 0, .7, 1);
                const double o1 = r(-30, 30), o2 = r(-30, 30);
                g.save(); g.globalAlpha = .24;
                poly({ { x - 1.4, 0 }, { x + 1.4, 0 }, { x + o1 + 12, floorY }, { x + o2 - 12, floorY } }, lin(0, 0, 0, floorY, { { 0, c }, { 1, P.glow(0, 0, .7, 0) } }));
                g.restore();
            }
            floorW(.03);
            for (int i = 0; i < 14; ++i) {
                SubR r = sub(QStringLiteral("fog%1").arg(i));
                const double x = r(0, VW), y = floorY + r(-6, 6), rr2 = r(6, 12);
                dot(x, y, rr2, P.glow(0, 0, .9, .06));
            }
            break;
        }
        case 7: {  // flying v: arena, plateia com celulares e lasers
            wall(.04);
            for (int row = 0; row < 3; ++row) {
                const double y = 30 + row * 12;
                g.fillStyle = P.shade(.08 + row * .02); g.beginPath(); g.moveTo(0, y + 10); g.quadraticCurveTo(CX, y - 4, VW, y + 10); g.lineTo(VW, y + 14); g.lineTo(0, y + 14); g.fill();
            }
            const int ph = jsRound((30 + grow * 90) * wide);
            for (int i = 0; i < ph; ++i) {
                SubR r = sub(QStringLiteral("cel%1").arg(i));
                const double x = r(0, VW), y = r(26, 62), rr2 = r(.3, .6), hu = r(-20, 40), al = r(.5, 1);
                dot(x, y, rr2, P.glow(hu, 0, .9, al));
            }
            const int nl = 2 + jsRound(grow * 4);
            for (int i = 0; i < nl; ++i) {
                SubR r = sub(QStringLiteral("lz%1").arg(i));
                const double ex = r(-10, VW + 10), ey = r(30, 70);
                g.save(); g.shadowColor = P.glow(-30, 0, .6, 1); g.shadowBlur = 3 * s; line(CX, 6, ex, ey, P.glow(i % 2 ? -30 : 60, 0, .65, .7), .5); g.restore();
            }
            for (int i = 0; i < 5; ++i) dot(CX - 24 + i * 12, 4, 1.6, P.glow(40, 0, .92, 1));
            floorW(.06); rr(0, floorY, VW, 2, .2, P.shade(.2));
            break;
        }
        default: {  // luciana: o cantinho de casa, abajur, espuma na parede e o gato dormindo no tapete
            wall(.2);
            for (double y = 6; y < floorY - 10; y += 8)
                for (double x = 4 + (jsRound(y / 8) % 2) * 4; x < VW; x += 8) poly({ { x - 3.4, y + 3 }, { x, y - 3 }, { x + 3.4, y + 3 } }, P.shade(.14));
            const double side = rnd() < .5 ? -1 : 1;
            const double lx = CX + side * R(30, 36);
            halo(lx, floorY - 34, 50, 40, .85, .5); line(lx, floorY, lx, floorY - 30, P.shade(.06), .8);
            poly({ { lx - 6, floorY - 30 }, { lx + 6, floorY - 30 }, { lx + 3.6, floorY - 38 }, { lx - 3.6, floorY - 38 } }, P.glow(40, 0, .78, 1));
            const int nl = jsRound((6 + grow * 14) * wide);
            g.strokeStyle = P.shade(.06); g.lineWidth = .3; g.beginPath(); g.moveTo(0, 6);
            for (double x = 0; x <= VW; x += 4) g.lineTo(x, 6 + std::sin(x / VW * PI) * 8);
            g.stroke();
            for (int i = 0; i < nl; ++i) { const double x = (i + .5) * VW / nl; dot(x, 6 + std::sin(x / VW * PI) * 8 + 1, .8, P.glow(40, 0, .88, 1)); }
            floorW(.1);
            g.fillStyle = P.glow(-30, 0, .36, 1); g.beginPath(); g.ellipse(CX, floorY + 5, 34, 5, 0, 0, 7); g.fill();
            const double cx = CX + (lx < CX ? 16 : -16), cy = floorY + 2;
            g.fillStyle = P.shade(.02); g.beginPath(); g.ellipse(cx, cy, 7, 3.4, 0, 0, 7); g.fill(); dot(cx + (lx < CX ? 6 : -6), cy - 1, 2.6, P.shade(.02));
            const double sd = lx < CX ? 1 : -1;
            poly({ { cx + sd * 4.4, cy - 2.6 }, { cx + sd * 5, cy - 5.6 }, { cx + sd * 6, cy - 3 } }, P.shade(.02));
            poly({ { cx + sd * 6.8, cy - 3 }, { cx + sd * 8, cy - 5.4 }, { cx + sd * 8.2, cy - 2.2 } }, P.shade(.02));
            g.strokeStyle = P.shade(.02); g.lineWidth = 1.3; g.beginPath(); g.moveTo(cx - sd * 6.6, cy + .6); g.quadraticCurveTo(cx - sd * 2, cy + 5, cx + sd * 4, cy + 2.6); g.stroke();
            break;
        }
        }

        // a guitarra
        g.save(); g.translate(gx0, gy0); g.rotate(a); g.scale(k, k);
        g.save(); g.translate(1.6, 1.2); g.fillStyle = rgba(0, 0, 0, .4); bodyPath(); g.fill(); headPath(); g.fill();
        pathPoly({ { -S.nutW / 2, 0 }, { S.nutW / 2, 0 }, { S.jw / 2, boardEnd }, { -S.jw / 2, boardEnd } }); g.fill(); g.restore();
        // corpo
        bodyPath(); g.fillStyle = finish(); g.fill();
        if (S.flame) {
            g.save(); bodyPath(); g.clip();
            SubR r = sub(QStringLiteral("flame"));
            for (int i = 0; i < 22; ++i) {
                const double y = r(bTop + 4, bottomY - 2);
                g.strokeStyle = rgba(255, 215, 150, r(.05, .13)); g.lineWidth = r(.4, 1.2);
                const double d1 = r(-3, 3), d2 = r(-3, 3), d3 = r(-2, 2);
                g.beginPath(); g.moveTo(-18, y); g.bezierCurveTo(-6, y + d1, 6, y + d2, 18, y + d3); g.stroke();
            }
            g.restore();
        }
        if (S.grainC.isValid()) {
            SubR r = sub(QStringLiteral("gr%1").arg(t));
            g.save(); bodyPath(); g.clip(); g.strokeStyle = S.grainC;
            for (int i = 0; i < 46; ++i) {
                const double x = r(-26, 26);
                g.lineWidth = r(.06, .18);
                const double d1 = r(-1.5, 1.5), d2 = r(-1.5, 1.5), d3 = r(-1, 1);
                g.beginPath(); g.moveTo(x, bTop - 2);
                g.bezierCurveTo(x + d1, bTop + (bottomY - bTop) * .35, x + d2, bTop + (bottomY - bTop) * .7, x + d3, bottomY + 2); g.stroke();
            }
            g.restore();
        }
        edgeShade(bodyPath, S.edgeA);
        g.save(); bodyPath(); g.clip();
        g.fillStyle = lin(-22, bTop, 18, bottomY, { { 0, rgba(255, 255, 255, 0) }, { .42, rgba(255, 255, 255, .2) }, { .52, rgba(255, 255, 255, .08) }, { 1, rgba(255, 255, 255, 0) } });
        g.fillRect(-60, bTop - 5, 120, bottomY - bTop + 10); g.restore();
        if (S.bind.isValid()) binding(bodyPath, S.bind);
        else { bodyPath(); g.strokeStyle = rgba(0, 0, 0, .6); g.lineWidth = .2; g.stroke(); }
        // escudo
        if (!guard.isEmpty()) {
            auto gp = [&]() { pathPoly(guard); };
            g.save(); g.translate(.25, .3); gp(); g.fillStyle = rgba(0, 0, 0, .35); g.fill(); g.restore();
            gp();
            if (S.pearl) {
                g.strokeStyle = hex("#f3f1ea"); g.lineWidth = .9; g.stroke(); gp(); g.strokeStyle = hex("#151515"); g.lineWidth = .5; g.stroke(); gp();
                g.fillStyle = lin(-12, 38, 14, 74, { { 0, hex("#121a6e") }, { .3, hex("#2333b8") }, { .5, hex("#3a4ee0") }, { .7, hex("#1d2a9c") }, { 1, hex("#0f155e") } });
                g.fill();
                g.save(); gp(); g.clip();
                SubR r = sub(QStringLiteral("perola"));
                for (int i = 0; i < 90; ++i) {
                    const bool light = r(0, 1) < .55;
                    const double al = r(.15, .45);
                    g.fillStyle = light ? rgba(120, 140, 255, al) : rgba(8, 10, 60, al);
                    const double ex = r(-15, 16), ey = r(36, 76), erx = r(.5, 1.8), ery = r(.3, 1), er = r(0, PI);
                    g.beginPath(); g.ellipse(ex, ey, erx, ery, er, 0, 7); g.fill();
                }
                g.restore();
            } else {
                g.fillStyle = S.guardC; g.fill();
            }
            if (S.guardEdge) { gp(); g.strokeStyle = rgba(0, 0, 0, .55); g.lineWidth = .22; g.stroke(); }
            QPointF gc(0, 0);
            for (const QPointF& p : guard) gc += p / guard.size();
            const int step = std::max(1, int(guard.size() / 9));
            for (int i = 0; i < guard.size(); ++i) {
                if (i % step) continue;
                const QPointF p = guard[i];
                dot(p.x() + (gc.x() - p.x()) * .06, p.y() + (gc.y() - p.y()) * .06, .2, S.gold ? hex("#c99c3c") : hex("#a9aeb5"));
            }
        }
        // braço e escala
        auto wAt = [&](double y) { return S.nutW + (S.jw - S.nutW) * std::min(1.0, std::max(0.0, y / boardEnd)); };
        if (S.bolt) { pathPoly({ { -S.nutW / 2, 0 }, { S.nutW / 2, 0 }, { S.jw / 2, boardEnd + 1.4 }, { -S.jw / 2, boardEnd + 1.4 } }); g.fillStyle = S.wood; g.fill(); }
        pathPoly({ { -S.nutW / 2, 0 }, { S.nutW / 2, 0 }, { S.jw / 2, boardEnd }, { -S.jw / 2, boardEnd } });
        g.fillStyle = lin(-3, 0, 3, 0, { { 0, S.board }, { .5, S.maple ? hex("#f1d39c") : S.board }, { 1, S.board } }); g.fill();
        if (!S.maple) {
            g.save(); pathPoly({ { -S.nutW / 2, 0 }, { S.nutW / 2, 0 }, { S.jw / 2, boardEnd }, { -S.jw / 2, boardEnd } }); g.clip();
            SubR r = sub(QStringLiteral("rose"));
            for (int i = 0; i < 14; ++i) {
                const bool warm = r(0, 1) < .5;
                g.strokeStyle = warm ? rgba(120, 70, 40, .35) : rgba(10, 5, 2, .35); g.lineWidth = r(.05, .15);
                const double x = r(-3, 3), dx = r(-.4, .4);
                g.beginPath(); g.moveTo(x, 0); g.lineTo(x + dx, boardEnd); g.stroke();
            }
            g.restore();
        }
        if (S.bind.isValid() && !S.nylon && t != 1) { line(-S.nutW / 2, 0, -S.jw / 2, boardEnd, S.bind, .18); line(S.nutW / 2, 0, S.jw / 2, boardEnd, S.bind, .18); }
        for (int n : { 3, 5, 7, 9, 12, 15, 17, 19, 21 }) {
            const double y0 = fretY(n - 1), y1 = fretY(n), yc = (y0 + y1) / 2;
            if (y1 > boardEnd) break;
            const double w = wAt(yc);
            if (S.inlay == IDots || S.inlay == IBlack) {
                const QColor c = S.inlay == IBlack ? hex("#1a1612") : hex("#eee8da");
                if (n == 12) { dot(-w * .22, yc, .3, c); dot(w * .22, yc, .3, c); } else dot(0, yc, .3, c);
            } else if (S.inlay == IBlock) {
                rr(-w * .36, yc - (y1 - y0) * .3, w * .72, (y1 - y0) * .6, .1, hex("#efe9da"));
            } else if (S.inlay == ITrap) {
                poly({ { -w * .36, yc - (y1 - y0) * .26 }, { w * .36, yc - (y1 - y0) * .34 }, { w * .36, yc + (y1 - y0) * .26 }, { -w * .36, yc + (y1 - y0) * .34 } }, hex("#efe9da"));
            }
        }
        for (int n = 1; n <= 24; ++n) {
            const double y = fretY(n);
            if (y > boardEnd - .15) break;
            const double w = wAt(y);
            g.fillStyle = lin(0, y - .1, 0, y + .1, { { 0, hex("#f2f3f5") }, { 1, hex("#7b8088") } }); g.fillRect(-w / 2, y - .1, w, .2);
        }
        // ferragens e captadores
        parts();
        // cabeça: a silhueta real; as tarraxas saem dos calombos da própria silhueta
        const bool hw = S.gold;
        headPath(); g.fillStyle = S.headC.isValid() ? QBrush(S.headC) : lin(-7, 0, 4, 0, { { 0, hex("#d6ad6c") }, { 1, hex("#ecca8e") } }); g.fill();
        if (S.flameHead) {
            g.save(); headPath(); g.clip();
            SubR r = sub(QStringLiteral("flamecab"));
            for (int i = 0; i < 16; ++i) {
                const double y = r(-20, -1);
                g.strokeStyle = rgba(120, 70, 20, r(.08, .2)); g.lineWidth = r(.2, .6);
                const double d1 = r(-1.4, 1.4), d2 = r(-1.4, 1.4);
                g.beginPath(); g.moveTo(-8, y); g.bezierCurveTo(-4, y + d1, 0, y + d2, 4, y); g.stroke();
            }
            g.restore();
        }
        if (S.slotHead) {
            double r0 = 1e9;
            for (const QPointF& p : head) r0 = std::min(r0, p.y());
            rr(-2.2, r0 + 3, 1.6, -r0 - 6.5, .8, hex("#0d0805")); rr(.6, r0 + 3, 1.6, -r0 - 6.5, .8, hex("#0d0805"));
        }
        headPath(); g.strokeStyle = rgba(0, 0, 0, .45); g.lineWidth = .18; g.stroke();
        if (S.headC.isValid() && !S.slotHead) {
            g.save(); headPath(); g.clip();
            g.fillStyle = lin(-6, 0, 6, -14, { { 0, rgba(255, 255, 255, 0) }, { .5, rgba(255, 255, 255, .12) }, { 1, rgba(255, 255, 255, 0) } });
            g.fillRect(-10, -30, 20, 30); g.restore();
        }
        struct Post { double kx, ky, px, py; };
        QVector<Post> posts;
        if (S.head3) {
            const QVector<Bump> L3 = bumps(head, -1, 3), R3 = bumps(head, 1, 3);
            const Bump order[6] = { L3[2], L3[1], L3[0], R3[0], R3[1], R3[2] };
            for (int i = 0; i < 6; ++i) posts << Post{ order[i].x, order[i].y, order[i].x + (i < 3 ? 2.6 : -2.6), order[i].y };
            if (!S.slotHead && S.headC.isValid()) rr(-2.4, posts[0].py - 1.2, 4.8, 1, .3, hex("#e9dfbf"));
        } else {
            for (const Bump& b : bumps(head, -1, 6)) posts << Post{ b.x, b.y, b.x + 2.8, b.y };
            g.strokeStyle = rgba(20, 10, 0, .75); g.lineWidth = .35; g.beginPath();
            const double ly = posts[4].py;
            g.moveTo(-.8, ly + 1.4); g.bezierCurveTo(.2, ly, -1.6, ly - 1.4, -.4, ly - 3); g.stroke();
        }
        for (const Post& p : posts) {
            const QBrush kc = (S.head3 && !S.slotHead) ? QBrush(hw ? hex("#e6c46a") : hex("#efe7cf"))
                            : S.slotHead ? QBrush(S.keyC) : (hw ? goldH(p.ky - .8, p.ky + .8) : chrome(p.ky - .8, p.ky + .8));
            g.fillStyle = kc; g.beginPath(); g.ellipse(p.kx + (p.kx < p.px ? .5 : -.5), p.ky, 1.1, .75, 0, 0, 7); g.fill();
            line(p.kx + (p.kx < p.px ? 1.2 : -1.2), p.ky, p.px, p.py, hw ? hex("#b88a2a") : hex("#8d9299"), .4);
            if (!S.slotHead) knob(p.px, p.py, .6, hw ? KGold : KChrome);
        }
        // pestana
        g.fillStyle = hex("#efe9d8"); g.fillRect(-S.nutW / 2, -.4, S.nutW, .45);
        // cordas: da tarraxa à pestana, da pestana ao cavalete e ao arremate
        const double wds[6] = { .13, .115, .1, .08, .065, .055 };
        for (int i = 0; i < 6; ++i) {
            const double xn = -S.nutW / 2 + .42 + i * (S.nutW - .84) / 5, xb = (i - 2.5) * S.sp;
            const Post& p = posts[i];
            const double wd = wds[i];
            const QColor col = S.nylon ? (i < 3 ? hex("#e6e1d0") : rgba(255, 255, 250, .75)) : (i < 4 ? hex("#cfc8b2") : hex("#e8eaee"));
            line(xn, 0, p.px, p.py, col, wd); line(xn, 0, xb, saddleY, col, wd); line(xb, saddleY, xb, S.anchor, col, wd);
            g.strokeStyle = rgba(255, 255, 255, .35); g.lineWidth = wd * .4; g.beginPath(); g.moveTo(xn - .02, 0); g.lineTo(xb - .02, saddleY); g.stroke();
        }
        g.restore();
        break;
    }
    /* ================= FEITOS À MÃO ================= */
    case MsVignette::Azulejo: {  // azulejo: o ladrilho ganha camadas de desenho com as palavras
        g.fillStyle = P.paper; g.fillRect(0, 0, VW, VH);
        const double T = 26;
        const QColor b1 = P.paperInk, b2 = P.glow(0, 0, .48, 1);
        const double x0 = CX - T / 2 - std::ceil((CX - T / 2) / T) * T, y0 = CY - T / 2 - std::ceil((CY - T / 2) / T) * T;
        struct Layer { int type; double r; int n; double w; bool fill; };
        Layer Ls[4];
        for (int k = 0; k < 4; ++k) {
            Layer& l = Ls[k];
            if (k == 0) { const double t = rnd(); l.type = t < .5 ? 0 : 3; }
            else l.type = int(std::floor(rnd() * 5));
            l.r = k == 0 ? R(7, 10) : R(3, 11);
            l.n = 4 * (1 + int(std::floor(rnd() * 3)));
            l.w = R(.6, 1.4);
            l.fill = rnd() < .5;
        }
        const int layers = 1 + jsRound(grow * 3);
        const double cr = R(.26, .4);
        for (double ty = y0; ty < VH; ty += T)
            for (double tx = x0; tx < VW; tx += T) {
                const double cx = tx + T / 2, cy = ty + T / 2;
                g.save(); g.beginPath(); g.rect(tx, ty, T, T); g.clip();
                g.strokeStyle = b1; g.lineWidth = 1.1;
                const double qs[4][2] = { { tx, ty }, { tx + T, ty }, { tx, ty + T }, { tx + T, ty + T } };
                for (const auto& q : qs) {
                    g.beginPath(); g.arc(q[0], q[1], T * cr, 0, 7); g.stroke();
                    if (layers > 2) { g.beginPath(); g.arc(q[0], q[1], T * cr * .55, 0, 7); g.fillStyle = b2; g.fill(); }
                }
                for (int k = 0; k < layers; ++k) {
                    const Layer& l = Ls[k];
                    const QColor c = k % 2 ? b2 : b1;
                    g.strokeStyle = c; g.fillStyle = c; g.lineWidth = l.w;
                    if (l.type == 0) {
                        for (int i = 0; i < l.n; ++i) {
                            const double a = double(i) / l.n * PI * 2;
                            g.beginPath(); g.ellipse(cx + std::cos(a) * l.r * .55, cy + std::sin(a) * l.r * .55, l.r * .5, l.r * .2, a, 0, 7);
                            if (l.fill) g.fill(); else g.stroke();
                        }
                    } else if (l.type == 1) {
                        g.beginPath(); g.moveTo(cx, cy - l.r); g.lineTo(cx + l.r, cy); g.lineTo(cx, cy + l.r); g.lineTo(cx - l.r, cy); g.closePath();
                        if (l.fill) g.fill(); else g.stroke();
                    } else if (l.type == 2) {
                        for (int i = 0; i < l.n; ++i) {
                            const double a = double(i) / l.n * PI * 2;
                            dot(cx + std::cos(a) * l.r, cy + std::sin(a) * l.r, l.w * .9, c);
                        }
                    } else if (l.type == 3) {
                        g.beginPath();
                        for (int i = 0; i <= l.n * 2; ++i) {
                            const double a = double(i) / (l.n * 2) * PI * 2, rr = i % 2 ? l.r * .45 : l.r;
                            g.lineTo(cx + std::cos(a) * rr, cy + std::sin(a) * rr);
                        }
                        g.closePath();
                        if (l.fill) g.fill(); else g.stroke();
                    } else {
                        g.beginPath(); g.arc(cx, cy, l.r, 0, 7); g.stroke();
                    }
                }
                dot(cx, cy, 1.2, b1);
                g.restore();
                g.strokeStyle = P.paper2; g.lineWidth = .8; g.strokeRect(tx, ty, T, T);
            }
        g.fillStyle = MsCanvas::linear(0, 0, VW, VH, { { 0, QColor(255, 255, 255, 46) }, { .5, QColor(255, 255, 255, 0) }, { 1, QColor(255, 255, 255, 15) } });
        g.fillRect(0, 0, VW, VH);
        break;
    }
    case MsVignette::Maze: {  // labirinto: o caminho avança até o centro conforme o capítulo cresce
        const double cs = 9;
        const int cols = int(std::floor((VW - 6) / cs)), rows = int(std::floor((VH - 6) / cs));
        const double ox = (VW - cols * cs) / 2, oy = (VH - rows * cs) / 2;
        const int mr = rows / 2;
        auto idx = [cols](int c, int r) { return r * cols + c; };
        QVector<int> E(cols * rows, 0);
        QVector<bool> seen(cols * rows, false);
        QVector<QPair<int, int>> stack{ { 0, mr } };
        seen[idx(0, mr)] = true;
        while (!stack.isEmpty()) {
            const int c = stack.last().first, r = stack.last().second;
            QVector<QPair<int, int>> nb;
            for (const auto& o : { QPair<int, int>{ c + 1, r }, QPair<int, int>{ c - 1, r }, QPair<int, int>{ c, r + 1 }, QPair<int, int>{ c, r - 1 } })
                if (o.first >= 0 && o.second >= 0 && o.first < cols && o.second < rows && !seen[idx(o.first, o.second)]) nb << o;
            if (nb.isEmpty()) { stack.removeLast(); continue; }
            const auto pick = nb[int(std::floor(rnd() * nb.size()))];
            const int a = pick.first, b = pick.second;
            if (a > c) E[idx(c, r)] |= 1; else if (a < c) E[idx(a, b)] |= 1; else if (b > r) E[idx(c, r)] |= 2; else E[idx(a, b)] |= 2;
            seen[idx(a, b)] = true;
            stack << pick;
        }
        const int Gc = cols / 2, G = idx(Gc, mr), S0 = idx(0, mr);
        QVector<int> prev(cols * rows, -1);
        QVector<int> q{ S0 };
        prev[S0] = S0;
        for (int qi = 0; qi < q.size(); ++qi) {
            const int i = q[qi];
            if (i == G) break;
            const int c = i % cols, r = i / cols;
            QVector<int> opts;
            if (E[i] & 1) opts << i + 1;
            if (c > 0 && (E[i - 1] & 1)) opts << i - 1;
            if (E[i] & 2) opts << i + cols;
            if (r > 0 && (E[i - cols] & 2)) opts << i - cols;
            for (int j : opts) if (prev[j] < 0) { prev[j] = i; q << j; }
        }
        QVector<int> path;
        for (int i = G; i != S0; i = prev[i]) path.prepend(i);
        path.prepend(S0);
        g.strokeStyle = P.glow(0, 0, .8, .75); g.lineWidth = 1.3; g.beginPath();
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c) {
                const double x = ox + c * cs, y = oy + r * cs;
                const int e = E[idx(c, r)];
                if (!(e & 1) && c < cols - 1) { g.moveTo(x + cs, y); g.lineTo(x + cs, y + cs); }
                if (!(e & 2) && r < rows - 1) { g.moveTo(x, y + cs); g.lineTo(x + cs, y + cs); }
            }
        g.moveTo(ox, oy + mr * cs); g.lineTo(ox, oy); g.lineTo(ox + cols * cs, oy); g.lineTo(ox + cols * cs, oy + rows * cs);
        g.lineTo(ox, oy + rows * cs); g.lineTo(ox, oy + (mr + 1) * cs); g.stroke();
        const double gx = ox + Gc * cs + cs / 2, gy = oy + mr * cs + cs / 2;
        const int reach = std::max(2, int(std::ceil(path.size() * grow)));
        const bool done = reach >= path.size();
        halo(gx, gy, done ? 10 : 6, 40, .8, done ? .7 : .35);
        auto ctr = [&](int i) { return QPointF(ox + (i % cols) * cs + cs / 2, oy + (i / cols) * cs + cs / 2); };
        g.save(); g.shadowColor = ink2; g.shadowBlur = 4 * s; g.strokeStyle = ink2; g.lineWidth = 2; g.beginPath();
        g.moveTo(ox - 6, oy + mr * cs + cs / 2);
        for (int k = 0; k < reach && k < path.size(); ++k) { const QPointF pt = ctr(path[k]); g.lineTo(pt.x(), pt.y()); }
        g.stroke(); g.restore();
        const QPointF end = ctr(path[std::min(reach, int(path.size())) - 1]);
        dot(end.x(), end.y(), 2.4, P.glow(40, 0, .92, 1));
        if (!done) { g.strokeStyle = P.glow(40, 0, .85, .8); g.lineWidth = .8; g.beginPath(); g.arc(gx, gy, 2.6, 0, 7); g.stroke(); }
        break;
    }
    case MsVignette::Cordel: {  // xilogravura de cordel: o sertão ganha mandacarus, pássaros e goivadas com as palavras
        g.fillStyle = P.paper; g.fillRect(0, 0, VW, VH);
        const QColor K = P.paperInk;
        const double sx = CX + R(-26, 26), sy = R(24, 32);
        const int nr = 12 + 2 * int(std::floor(rnd() * 3));
        for (int i = 0; i < nr; ++i) {
            const double a = double(i) / nr * PI * 2, l = i % 2 ? 16 : 22, w = .13;
            poly({ { sx + std::cos(a - w) * 12, sy + std::sin(a - w) * 12 }, { sx + std::cos(a) * l, sy + std::sin(a) * l },
                   { sx + std::cos(a + w) * 12, sy + std::sin(a + w) * 12 } }, K);
        }
        dot(sx, sy, 10, K);
        g.strokeStyle = P.paper; g.lineWidth = 1; g.beginPath(); g.arc(sx, sy, 6.5, 0, 7); g.stroke();
        dot(sx, sy, 2.4, P.paper);
        const double gy = VH - 24;
        g.fillStyle = K; g.beginPath(); g.moveTo(0, VH);
        for (double x = 0; x <= VW; x += 3) g.lineTo(x, gy + std::sin(x * .07) * 3);
        g.lineTo(VW, VH); g.fill();
        const int cuts = jsRound((8 + grow * 30) * wide);
        g.strokeStyle = P.paper; g.lineWidth = .8;
        for (int i = 0; i < cuts; ++i) {
            SubR r = sub(QStringLiteral("g%1").arg(i));
            const double x = r(0, VW), y = r(gy + 5, VH - 3), l = r(3, 8), dy = r(-.6, .6);
            g.beginPath(); g.moveTo(x, y); g.lineTo(x + l, y + dy); g.stroke();
        }
        const int nc = 1 + jsRound(grow * 3);
        const int lanes[5] = { 0, -1, 1, -2, 2 };
        for (int i = 0; i < nc; ++i) {
            SubR r = sub(QStringLiteral("c%1").arg(i));
            const double x = CX + lanes[i] * 19 * std::pow(wide, .6) + r(-4, 4);
            const double h = i ? r(20, 32) : r(36, 46);
            const double base = gy + 2, w = i ? 4.6 : 6.5;
            line(x, base, x, base - h, K, w);
            const int arms = 1 + int(std::floor(r(0, 2.99)));
            for (int a = 0; a < arms; ++a) {
                const int side = a % 2 ? 1 : -1;
                const double ay = base - h * r(.35, .6), aw = r(6, 10), ah = r(8, 14);
                g.strokeStyle = K; g.lineWidth = w * .75; g.beginPath();
                g.moveTo(x, ay); g.lineTo(x + side * aw, ay); g.lineTo(x + side * aw, ay - ah); g.stroke();
                line(x + side * aw, ay - 2, x + side * aw, ay - ah + 1, P.paper, .5);
            }
            line(x, base - 3, x, base - h + 2, P.paper, .5);
        }
        const int birds = jsRound(grow * 4 * wide);
        for (int i = 0; i < birds; ++i) {
            SubR r = sub(QStringLiteral("p%1").arg(i));
            const double x = r(8, VW - 8), y = r(10, gy - 30), k = r(.7, 1.1);
            g.fillStyle = K; g.beginPath(); g.moveTo(x - 6 * k, y - 1 * k);
            g.quadraticCurveTo(x - 3 * k, y - 4 * k, x, y); g.quadraticCurveTo(x + 3 * k, y - 4 * k, x + 6 * k, y - 1 * k);
            g.quadraticCurveTo(x + 3 * k, y - 1.5 * k, x, y + 1.4 * k); g.quadraticCurveTo(x - 3 * k, y - 1.5 * k, x - 6 * k, y - 1 * k);
            g.fill();
        }
        g.strokeStyle = K; g.lineWidth = 1.6;
        if (rect) g.strokeRect(4, 4, VW - 8, VH - 8);
        else { g.beginPath(); g.arc(CX, CY, 46, 0, 7); g.stroke(); }
        break;
    }
    case MsVignette::Origami: {  // origami: mais fios de tsurus pendurados com as palavras
        auto crane = [&](double x, double y, double sc, double dir, double hue) {
            const QColor Lc = P.glow(hue, 0, .76, 1), Dc = P.glow(hue, 0, .56, 1), Mc = P.glow(hue, 0, .66, 1);
            g.save(); g.translate(x, y); g.scale(sc * dir, sc);
            poly({ { -3, -1 }, { -6, -18 }, { 3, -1.5 } }, Dc);                    // asa de trás
            poly({ { -5, .5 }, { -17, -10 }, { -2, -1.5 } }, Mc);                  // cauda
            poly({ { 4, .5 }, { 15, -11 }, { 2, -1.5 } }, Mc);                     // pescoço
            poly({ { 15, -11 }, { 18.6, -7.6 }, { 14.4, -9 } }, Dc);               // cabeça dobrada
            poly({ { -7, 0 }, { 0, -2.6 }, { 0, 3.4 } }, Mc);
            poly({ { 0, -2.6 }, { 7, 0 }, { 0, 3.4 } }, Lc);                       // corpo
            poly({ { -2, -1.5 }, { 1, -19 }, { 5, -1 } }, Lc);
            line(-.5, -1.5, 1, -19, Dc, .35);                                      // asa da frente e a dobra
            g.restore();
        };
        const int lanes[7] = { 0, -1, 1, -2, 2, -3, 3 };
        const double sx = 23;
        const int ns = std::min(7, 2 + jsRound(grow * 3) + jsRound((wide - 1) * 3));
        const double hues[5] = { 0, 25, -25, 45, -45 };
        for (int i = 0; i < ns; ++i) {
            SubR r = sub(QStringLiteral("s%1").arg(i));
            const double x = CX + lanes[i] * sx + r(-2, 2);
            const int extra = r(0, 1) < .4 ? 1 : 0;
            const int per = std::min(3, 1 + jsRound(grow * 2) + extra);
            const double hue = hues[int(std::floor(r(0, 5)))];
            QVector<double> ys;
            for (int k = 0; k < per; ++k) ys << 26 + k * 28 + r(-3, 3) + (i % 2) * 12;
            line(x, -2, x, ys.last(), P.glow(0, 0, .85, .4), .4);
            for (int k = 0; k < per; ++k) { const double dir = r(0, 1) < .5 ? 1 : -1; crane(x, ys[k], .92, dir, hue + k * 8); }
        }
        break;
    }
    case MsVignette::TreasureMap: {  // mapa do tesouro: a trilha avança rumo ao X conforme o capítulo cresce
        g.fillStyle = P.parch; g.fillRect(0, 0, VW, VH);
        for (int i = 0; i < 5; ++i) {
            const double x = R(0, VW), y = R(0, VH), rr = R(8, 22);
            dot(x, y, rr, MsCanvas::radial(x, y, 0, x, y, rr, { { 0, P.glow(20, 0, .45, .12) }, { 1, P.glow(20, 0, .45, 0) } }));
        }
        const QColor K = P.paperInk;
        auto blob = [&](const std::function<double(double, double)>& r, double cx, double cy, double rad, double k, double amp) {
            const double ph0 = r(0, 7), ph1 = r(0, 7), ph2 = r(0, 7);
            Pts pts;
            for (int i = 0; i < 48; ++i) {
                const double a = i / 48.0 * PI * 2;
                const double rr = rad * (1 + amp * (std::sin(a * k + ph0) * .5 + std::sin(a * (k + 2) + ph1) * .3 + std::sin(a * (k + 5) + ph2) * .2));
                pts << QPointF(cx + std::cos(a) * rr * std::pow(wide, .3), cy + std::sin(a) * rr * .85);
            }
            return pts;
        };
        auto shape = [&](const Pts& pts) {
            g.beginPath();
            for (int i = 0; i < pts.size(); ++i) { if (i) g.lineTo(pts[i].x(), pts[i].y()); else g.moveTo(pts[i].x(), pts[i].y()); }
            g.closePath();
        };
        const double kMain = 2 + std::floor(rnd() * 2);   // no JS esse sorteio sai antes dos de dentro do blob
        const Pts main = blob([&](double a, double b) { return R(a, b); }, CX, CY + 2, 28, kMain, .22);
        const int rip = 1 + jsRound(grow * 2);
        for (int k = rip; k >= 1; --k) {
            g.save(); g.translate(CX, CY + 2); g.scale(1 + k * .12, 1 + k * .12); g.translate(-CX, -CY - 2); shape(main); g.restore();
            g.strokeStyle = P.glow(0, 0, .4, .25); g.lineWidth = .6; g.stroke();
        }
        shape(main); g.fillStyle = P.land; g.fill(); g.strokeStyle = K; g.lineWidth = 1; g.stroke();
        const int mts = 2 + int(std::floor(rnd() * 3));
        for (int i = 0; i < mts; ++i) {
            const double x = CX + R(-14, 14), y = CY + R(-10, 8);
            g.strokeStyle = K; g.lineWidth = .8; g.beginPath(); g.moveTo(x - 4, y + 2); g.lineTo(x, y - 3); g.lineTo(x + 4, y + 2); g.stroke();
        }
        const int i0 = int(std::floor(rnd() * 48));
        const double sa = std::atan2(main[i0].y() - CY - 2, main[i0].x() - CX);
        const double ea = sa + PI + R(-.6, .6);
        const QPointF p0(CX + (main[i0].x() - CX) * .9, CY + 2 + (main[i0].y() - CY - 2) * .9);
        const QPointF p3(CX + std::cos(ea) * 14, CY + 2 + std::sin(ea) * 11);
        const double p1x = CX + R(-18, 18), p1y = CY + R(-14, 14), p2x = CX + R(-18, 18), p2y = CY + R(-14, 14);
        auto bz = [&](double t) {
            const double u = 1 - t;
            return QPointF(u * u * u * p0.x() + 3 * u * u * t * p1x + 3 * u * t * t * p2x + t * t * t * p3.x(),
                           u * u * u * p0.y() + 3 * u * u * t * p1y + 3 * u * t * t * p2y + t * t * t * p3.y());
        };
        const int N = 60;
        const double stop = std::max(.15, grow);
        g.strokeStyle = P.glow(-30, 0, .4, 1); g.lineWidth = 1; g.setLineDash({ 2.2, 1.8 }); g.beginPath();
        for (int i = 0; i <= N * stop; ++i) { const QPointF pt = bz(double(i) / N); if (i) g.lineTo(pt.x(), pt.y()); else g.moveTo(pt.x(), pt.y()); }
        g.stroke(); g.setLineDash({});
        line(p3.x() - 2.6, p3.y() - 2.6, p3.x() + 2.6, p3.y() + 2.6, P.glow(-30, 0, .42, 1), 1.6);
        line(p3.x() + 2.6, p3.y() - 2.6, p3.x() - 2.6, p3.y() + 2.6, P.glow(-30, 0, .42, 1), 1.6);
        const int isl = jsRound(grow * 3 * wide);
        for (int i = 0; i < isl; ++i) {
            SubR r = sub(QStringLiteral("i%1").arg(i));
            const double a = r(0, PI * 2), d = r(38, 46), rad = r(3, 6);
            const Pts pts = blob([&r](double lo, double hi) { return r(lo, hi); }, CX + std::cos(a) * d * wide, CY + std::sin(a) * d * .8, rad, 2, .3);
            shape(pts); g.fillStyle = P.land; g.fill(); g.strokeStyle = K; g.lineWidth = .7; g.stroke();
        }
        const int waves = jsRound((3 + grow * 6) * wide);
        g.strokeStyle = P.glow(0, 0, .38, .5); g.lineWidth = .6;
        for (int i = 0; i < waves; ++i) {
            SubR r = sub(QStringLiteral("w%1").arg(i));
            const double x = r(4, VW - 8), y = r(6, VH - 6);
            if (std::hypot((x - CX) / std::pow(wide, .3), (y - CY) / .85) < 36) continue;
            g.beginPath(); g.moveTo(x, y); g.quadraticCurveTo(x + 1.5, y - 1.6, x + 3, y); g.quadraticCurveTo(x + 4.5, y + 1.6, x + 6, y); g.stroke();
        }
        const double rx = CX + 30 * std::pow(wide, 1.4), ry = CY - 28;
        poly({ { rx, ry - 7 }, { rx + 1.6, ry }, { rx, ry + 7 }, { rx - 1.6, ry } }, K);
        poly({ { rx - 7, ry }, { rx, ry - 1.6 }, { rx + 7, ry }, { rx, ry + 1.6 } }, K);
        dot(rx, ry, 1.2, P.parch);
        break;
    }
    case MsVignette::Chess: {  // xadrez: mais peças no tabuleiro com as palavras
        const double far = CY - 10, nearY = VH - 4;
        const int nr = 6, nc = 12;
        auto yAt = [&](double k) { return far + (nearY - far) * std::pow(k / nr, 1.25); };
        auto scAt = [&](double k) { return .5 + .5 * (k / nr); };
        const double cw = 15 * std::max(1.0, wide * .8);
        for (int k = 0; k < nr; ++k) {
            const double y0 = yAt(k), y1 = yAt(k + 1), s0 = scAt(k), s1 = scAt(k + 1);
            for (int c = 0; c < nc; ++c) {
                const double cc = c - nc / 2.0;
                poly({ { CX + cc * cw * s0, y0 }, { CX + (cc + 1) * cw * s0, y0 }, { CX + (cc + 1) * cw * s1, y1 }, { CX + cc * cw * s1, y1 } },
                     (c + k) % 2 ? P.shade(.16) : P.shade(.42));
            }
        }
        g.fillStyle = MsCanvas::linear(0, far, 0, far + 22, { { 0, P.bg1 }, { 1, QColor(0, 0, 0, 0) } });
        g.fillRect(0, far - 1, VW, 23);
        auto piece = [&](double x, double y, double sc, int t, bool w) {
            g.save(); g.translate(x, y); g.scale(sc, sc);
            g.fillStyle = w ? P.paper : P.shade(0); g.strokeStyle = w ? P.paperInk : ink; g.lineWidth = .6 / sc;
            auto fs = [&]() { g.fill(); g.stroke(); };
            g.beginPath(); g.ellipse(0, -1, 5, 1.8, 0, 0, 7); fs();
            auto bodyTo = [&](double top) { g.beginPath(); g.moveTo(-3.2, -2); g.lineTo(-1.4, top); g.lineTo(1.4, top); g.lineTo(3.2, -2); g.closePath(); fs(); };
            if (t == 0) { bodyTo(-9); g.beginPath(); g.arc(0, -11, 2.6, 0, 7); fs(); }
            else if (t == 1) {
                g.beginPath(); g.rect(-2.8, -12, 5.6, 10); fs();
                g.beginPath(); g.moveTo(-3.8, -12); g.lineTo(-3.8, -15.5); g.lineTo(-2.3, -15.5); g.lineTo(-2.3, -14); g.lineTo(-.75, -14);
                g.lineTo(-.75, -15.5); g.lineTo(.75, -15.5); g.lineTo(.75, -14); g.lineTo(2.3, -14); g.lineTo(2.3, -15.5); g.lineTo(3.8, -15.5);
                g.lineTo(3.8, -12); g.closePath(); fs();
            } else if (t == 2) {
                bodyTo(-11); g.beginPath(); g.ellipse(0, -14, 2.6, 3.8, 0, 0, 7); fs(); g.beginPath(); g.arc(0, -18.6, 1, 0, 7); fs();
            } else if (t == 3) {
                bodyTo(-12); g.beginPath(); g.moveTo(-3, -12);
                for (int i = 0; i < 5; ++i) { g.lineTo(-3 + i * 1.5 + .75, -16.5); g.lineTo(-3 + (i + 1) * 1.5, -13.2); }
                g.lineTo(3, -12); g.closePath(); fs(); g.beginPath(); g.arc(0, -17.4, .9, 0, 7); fs();
            } else if (t == 4) {
                bodyTo(-13); g.beginPath(); g.rect(-3, -15, 6, 2.2); fs(); g.beginPath(); g.rect(-.6, -20, 1.2, 5); g.rect(-2, -18.4, 4, 1.1); fs();
            } else {
                g.beginPath(); g.moveTo(-3.2, -2); g.lineTo(-3, -8); g.quadraticCurveTo(-3, -13, 0, -16); g.lineTo(2.6, -16.6); g.lineTo(4.6, -12.6);
                g.lineTo(3.6, -11.4); g.lineTo(1.2, -12); g.quadraticCurveTo(3.4, -7, 3.2, -2); g.closePath(); fs();
            }
            g.restore();
        };
        struct Pc { int c, k, t; bool w; };
        QVector<Pc> PCS;
        QVector<int> used;
        while (PCS.size() < 12) {
            const int c = 2 + int(std::floor(rnd() * 8));
            const int k = int(std::floor(rnd() * nr));
            const int key = c * 100 + k;
            if (used.contains(key)) continue;
            used << key;
            const int t = int(std::floor(rnd() * 6));
            const bool w = rnd() < .5;
            PCS << Pc{ c, k, t, w };
        }
        const int np = 2 + jsRound(grow * 8);
        QVector<Pc> vis = PCS.mid(0, np);
        std::stable_sort(vis.begin(), vis.end(), [](const Pc& a, const Pc& b) { return a.k < b.k; });
        for (const Pc& p : vis) {
            const double s0 = scAt(p.k + .5), y = yAt(p.k + .62), x = CX + (p.c - nc / 2.0 + .5) * cw * s0;
            piece(x, y, s0 * 1.2, p.t, p.w);
        }
        break;
    }
    /* ================= ARSENAL ================= */
    case MsVignette::Blades:
    case MsVignette::Firearms: {  // arsenal: cada faixa de palavras destrava a próxima arma; o sorteio só escolhe o ângulo
        const bool blades = fam == MsVignette::Blades;
        static const int kBlades[] = { 0, 300, 800, 1500, 2500, 4000, 6000, 9000 };
        static const int kGuns[] = { 0, 300, 700, 1200, 1800, 2600, 3600, 5000, 7000, 10000 };
        int t = 0;
        if (blades) { for (int i = 0; i < 8; ++i) if (words >= kBlades[i]) t = i; }
        else        { for (int i = 0; i < 10; ++i) if (words >= kGuns[i]) t = i; }
        // close: a arma passa da borda do quadro
        double Z = 1.3, K = 1;
        auto sc = [&](double len) { K = Z * 84 / len; g.scale(K, K); };
        auto rr = [&](double x, double y, double w, double h, double r, const QBrush& c) { g.fillStyle = c; g.beginPath(); g.roundRect(x, y, w, h, r); g.fill(); };
        auto steel = [&](double y0, double y1) {
            return MsCanvas::linear(0, y0, 0, y1, { { 0, P.paper }, { .55, P.glow(0, 0, .7, 1) }, { 1, P.shade(.4) } });
        };
        auto hl = [&](double x1, double y1, double x2, double y2) { line(x1, y1, x2, y2, P.glow(0, 0, .9, .45), .5); };
        const QColor dark2 = P.shade(.12), wood = P.glow(35, 0, .42, 1), wood2 = P.glow(30, 0, .3, 1),
                     gold = P.glow(40, 0, .62, 1), wrap = ink2, metal = P.shade(.32);
        auto guard = [&](double x, double y, double r, const QBrush& c, double w) {
            g.strokeStyle = c; g.lineWidth = w; g.beginPath(); g.arc(x, y, r, 0, PI); g.stroke();
        };
        auto BL = [&](int k) {
            switch (k) {
            case 0:  // punhal
                sc(58); rr(-26, -2.4, 15, 4.8, 1.5, dark2);
                for (int i = 0; i < 4; ++i) line(-24 + i * 3.5, -2.4, -22.5 + i * 3.5, 2.4, wrap, .9);
                dot(-27, 0, 2.6, gold); rr(-11.5, -6, 2.6, 12, 1, gold);
                poly({ { -9, -3.6 }, { 18, -1.6 }, { 27, 0 }, { 18, 1.6 }, { -9, 3.6 } }, steel(-3.6, 3.6));
                line(-8, 0, 20, 0, P.shade(.45, .6), .6);
                break;
            case 1: {  // katana
                sc(106); rr(-51, -2.6, 28, 5.2, 1.6, dark2);
                for (int i = 0; i < 7; ++i)
                    poly({ { -49.5 + i * 3.8, 0 }, { -47.6 + i * 3.8, -2.6 }, { -45.7 + i * 3.8, 0 }, { -47.6 + i * 3.8, 2.6 } }, wrap);
                rr(-52.5, -2.8, 2.4, 5.6, 1, gold);
                g.fillStyle = gold; g.beginPath(); g.ellipse(-22, 0, 1.7, 6.4, 0, 0, 7); g.fill();
                rr(-20.6, -2.6, 3, 5.2, .6, gold);
                g.fillStyle = steel(-6, 3); g.beginPath(); g.moveTo(-17.6, -2.3);
                g.quadraticCurveTo(16, -4.6, 47, -8.2); g.quadraticCurveTo(51, -8.2, 51.5, -7.4);
                g.quadraticCurveTo(48, -3.2, 44, -3.6); g.quadraticCurveTo(14, -.8, -17.6, 2.3); g.closePath(); g.fill();
                auto q = [](double a, double b, double c, double u) { return (1 - u) * (1 - u) * a + 2 * (1 - u) * u * b + u * u * c; };
                g.strokeStyle = P.glow(0, 0, .97, .75); g.lineWidth = .5; g.beginPath();
                for (double u = 0; u <= .92; u += .03) {
                    const double x = q(-17.6, 14, 44, u), ye = q(2.3, -.8, -3.6, u), yb = q(-2.3, -4.6, -8.2, u);
                    const double y = ye - (ye - yb) * .34 + std::sin(u * 60) * .35;
                    if (u != 0) g.lineTo(x, y); else g.moveTo(x, y);
                }
                g.stroke();
                break;
            }
            case 2:  // espada europeia
                sc(108); dot(-51, 0, 3, gold); rr(-48, -2.2, 17, 4.4, 1.2, wood2);
                for (int i = 0; i < 5; ++i) line(-46 + i * 3.2, -2.2, -44 + i * 3.2, 2.2, P.shade(.06, .6), .6);
                rr(-31.5, -12, 3, 24, 1.4, gold); dot(-30, -12, 1.8, gold); dot(-30, 12, 1.8, gold);
                poly({ { -28.5, -3.4 }, { 42, -2.4 }, { 53, 0 }, { 42, 2.4 }, { -28.5, 3.4 } }, steel(-3.4, 3.4));
                rr(-27, -.6, 52, 1.2, .6, P.shade(.4, .7));
                break;
            case 3:  // khopesh
                sc(94); rr(-50, -2.4, 22, 4.8, 1.4, wood2); dot(-51, 0, 2.6, gold); rr(-28.5, -3.4, 3, 6.8, 1, gold);
                g.fillStyle = MsCanvas::linear(0, -16, 0, 6, { { 0, P.glow(40, 0, .8, 1) }, { 1, P.glow(30, 0, .42, 1) } });
                g.beginPath(); g.moveTo(-25.5, -2.2); g.lineTo(2, -2.6); g.bezierCurveTo(12, -18, 34, -20, 38, -2);
                g.bezierCurveTo(39, 4, 36, 7, 33, 8.5); g.bezierCurveTo(33, -6, 20, -10, 8, 2.6); g.lineTo(-25.5, 2.2); g.closePath(); g.fill();
                g.strokeStyle = P.glow(40, 0, .93, .85); g.lineWidth = .6; g.beginPath(); g.moveTo(34, 8);
                g.bezierCurveTo(37, 6, 39, 3, 38, -2); g.bezierCurveTo(34, -19, 13, -17, 3, -2.6); g.stroke();
                break;
            case 4:  // sabre chinês
                sc(108); g.strokeStyle = gold; g.lineWidth = 1.3; g.beginPath(); g.arc(-51.5, 0, 2.4, 0, 7); g.stroke();
                g.strokeStyle = P.glow(-40, 0, .55, 1); g.lineWidth = 1.6; g.beginPath(); g.moveTo(-53.5, 1); g.bezierCurveTo(-58, 6, -54, 12, -59, 17); g.stroke();
                g.lineWidth = 1; g.beginPath(); g.moveTo(-53.5, 1); g.bezierCurveTo(-56, 8, -51, 12, -55, 18); g.stroke();
                rr(-49, -2.4, 21, 4.8, 1.4, dark2);
                for (int i = 0; i < 5; ++i) line(-47 + i * 3.8, -2.4, -45 + i * 3.8, 2.4, wrap, .8);
                g.fillStyle = gold; g.beginPath(); g.ellipse(-27.5, 0, 2, 5.4, 0, 0, 7); g.fill();
                g.fillStyle = steel(-7, 4); g.beginPath(); g.moveTo(-25.5, -2.4); g.quadraticCurveTo(18, -3, 48, -6.6);
                g.quadraticCurveTo(53.5, -5.6, 52, -2.4); g.quadraticCurveTo(46, 2.4, 38, 3.2); g.quadraticCurveTo(8, 3, -25.5, 2.4); g.closePath(); g.fill();
                line(-24, -1.2, 44, -5.2, P.shade(.4, .5), .6);
                break;
            case 5: {  // alabarda
                sc(108); rr(-54, -1.6, 88, 3.2, 1.4, wood);
                for (int i = 0; i < 3; ++i) rr(-40 + i * 24, -2, 1.8, 4, .4, gold);
                rr(-56, -2, 3, 4, .8, metal);
                g.save(); g.translate(34, 0); g.scale(1.35, 1.35); g.translate(-34, 0);
                const QBrush st = steel(-12, 18);
                poly({ { 42, -1.5 }, { 58, 0 }, { 42, 1.5 } }, st);
                g.fillStyle = st; g.beginPath(); g.moveTo(32, 1.3); g.lineTo(42, 1.3); g.bezierCurveTo(45, 7, 44, 15, 39, 19);
                g.bezierCurveTo(37, 13, 34, 8, 32, 1.3); g.closePath(); g.fill();
                poly({ { 34, -1.3 }, { 40, -1.3 }, { 45, -10 } }, st);
                rr(31, -2.6, 12, 5.2, 1, metal); dot(34, 0, .7, gold); dot(40, 0, .7, gold); g.restore();
                break;
            }
            case 6:  // montante
                sc(118); dot(-55, 0, 3.4, gold); rr(-52, -2.4, 22, 4.8, 1.4, dark2);
                for (int i = 0; i < 6; ++i) line(-50 + i * 3.4, -2.4, -48 + i * 3.4, 2.4, wrap, .8);
                rr(-30.5, -14, 3.2, 28, 1.4, gold);
                g.strokeStyle = gold; g.lineWidth = 1.6; g.beginPath(); g.arc(-26.4, -14, 2.6, PI, PI * 2.3); g.stroke();
                g.beginPath(); g.arc(-26.4, 14, 2.6, -PI * .3, PI); g.stroke();
                poly({ { -27.5, -3 }, { -14, -3 }, { -13, -6 }, { -11, -3.8 }, { 46, -2.6 }, { 58, 0 }, { 46, 2.6 }, { -11, 3.8 }, { -13, 6 }, { -14, 3 }, { -27.5, 3 } }, steel(-4, 4));
                rr(-10, -.6, 48, 1.2, .6, P.shade(.4, .7));
                break;
            default: {  // foice
                sc(112); g.strokeStyle = wood2; g.lineWidth = 3; g.beginPath(); g.moveTo(-54, 6); g.quadraticCurveTo(-6, 4, 44, -4); g.stroke();
                line(-30, 5, -29, -3, wood, 2.2); line(6, 1.8, 7, -6, wood, 2.2);
                g.fillStyle = MsCanvas::linear(0, -32, 0, 0, { { 0, P.paper }, { 1, P.shade(.4) } });
                g.beginPath(); g.moveTo(44, -6); g.bezierCurveTo(30, -30, -4, -34, -28, -22); g.bezierCurveTo(-6, -24, 22, -20, 40, -1); g.closePath(); g.fill();
                g.save(); g.shadowColor = P.glow(0, 0, .9, 1); g.shadowBlur = 3 * s; g.strokeStyle = P.glow(0, 0, .97, .9); g.lineWidth = .7;
                g.beginPath(); g.moveTo(-28, -22); g.bezierCurveTo(-6, -24, 22, -20, 40, -1); g.stroke(); g.restore();
                rr(40, -8, 7, 7, 1.5, metal);
                break;
            }
            }
        };
        auto sh = [&](double l, double a = 1) { return P.shade(l, a); };
        const QColor gd = sh(.04), gd2 = sh(.1), gwood = P.glow(35, 0, .3, 1), gwood2 = P.glow(30, 0, .22, 1);
        auto GN = [&](int k) {
            switch (k) {
            case 0:  // pistola 9 mm
                sc(66); poly({ { -20, -4 }, { -8, -4 }, { -11, 15 }, { -23, 15 } }, gd);
                for (int i = 0; i < 4; ++i) line(-20 - i * .6, 1 + i * 3.4, -10 - i * .6, 1 + i * 3.4, sh(.16), .5);
                rr(-22, -6, 40, 4, 1, gd); guard(-2, -1.6, 4.2, gd, 1.6); line(-3, -2, -4, 2.4, metal, 1.2);
                rr(-24, -13, 46, 7.4, 1.4, sh(.2));
                for (int i = 0; i < 5; ++i) line(-21 + i * 1.6, -12, -21 + i * 1.6, -7, sh(.06), .5);
                rr(20, -11.4, 3, 2.6, .4, sh(.04)); hl(-23, -12.6, 21, -12.6);
                break;
            case 1:  // desert eagle
                sc(74); poly({ { -22, -4 }, { -9, -4 }, { -12, 16 }, { -25, 16 } }, gd);
                rr(-24, -6, 42, 4.4, 1, sh(.1)); guard(-2, -1.4, 4.6, sh(.1), 1.8); line(-3, -2, -4, 2.6, metal, 1.3);
                poly({ { -26, -15 }, { 30, -15 }, { 33, -11 }, { 30, -6 }, { -26, -6 } }, steel(-15, -6));
                line(-26, -11.2, 30, -11.2, sh(.4, .6), .6);
                for (int i = 0; i < 4; ++i) line(-23 + i * 1.8, -14, -23 + i * 1.8, -7, sh(.3), .5);
                rr(-28.5, -14, 3, 4, 1, metal); rr(26, -17, 2.4, 2.2, .4, metal); rr(-20, -17, 3, 2.2, .4, metal);
                break;
            case 2:  // uzi
                sc(78); g.strokeStyle = metal; g.lineWidth = 1.6; g.beginPath(); g.moveTo(-22, -8); g.lineTo(-40, -6); g.lineTo(-40, 3); g.lineTo(-30, 0); g.stroke();
                poly({ { -7, -2 }, { 2, -2 }, { 1, 24 }, { -8, 24 } }, sh(.04)); poly({ { -8, -2 }, { 3, -2 }, { 1, 13 }, { -10, 13 } }, gd2);
                rr(-24, -12, 46, 10, 1.6, sh(.18));
                for (int i = 0; i < 6; ++i) line(-20 + i * 6, -11, -17 + i * 6, -11, sh(.06), .6);
                rr(22, -9, 9, 3.4, .8, gd); guard(8, -2, 4, gd2, 1.6); rr(-22, -15, 4, 3, .6, metal); rr(18, -15, 3, 3, .6, metal); hl(-23, -11.6, 21, -11.6);
                break;
            case 3:  // ump
                sc(86); g.strokeStyle = sh(.1); g.lineWidth = 2; g.strokeRect(-46, -10, 20, 9);
                poly({ { -16, -3 }, { -7, -3 }, { -10, 13 }, { -19, 12 } }, gd); poly({ { 2, -3 }, { 10, -3 }, { 12, 17 }, { 4, 18 } }, gd2); guard(-4, -2.4, 3.6, gd, 1.6);
                rr(-27, -13, 52, 10, 1.8, sh(.14));
                for (int i = 0; i < 12; ++i) rr(-20 + i * 3.4, -15.2, 2, 2.4, .3, sh(.06));
                rr(25, -10, 9, 3, .6, sh(.06)); hl(-26, -12.6, 24, -12.6);
                break;
            case 4:  // mp5
                sc(94); poly({ { -24, -12 }, { -46, -10 }, { -47, 2 }, { -38, 2 }, { -24, -3 } }, gd); poly({ { -14, -4 }, { -6, -4 }, { -9, 11 }, { -18, 10 } }, gd);
                g.fillStyle = gd2; g.beginPath(); g.moveTo(-1, -4); g.lineTo(6, -4); g.quadraticCurveTo(10, 6, 15, 16); g.lineTo(8, 18); g.quadraticCurveTo(3, 7, -1, -4); g.fill();
                rr(-25, -13, 40, 8, 3.6, sh(.2)); poly({ { 14, -11 }, { 30, -10 }, { 30, -5 }, { 14, -4 } }, gd); rr(29, -11, 14, 2.6, .6, metal);
                poly({ { 38, -11 }, { 40, -17 }, { 42, -11 } }, metal);
                g.strokeStyle = metal; g.lineWidth = 1.2; g.beginPath(); g.arc(-22, -15, 2.4, PI, 0); g.stroke(); guard(-3, -3, 3.6, gd, 1.6); hl(-24, -12.6, 14, -12.6);
                break;
            case 5:  // m4
                sc(104); rr(-36, -10, 16, 3.2, 1, gd2); poly({ { -36, -13 }, { -48, -12 }, { -49, 1 }, { -36, -1 } }, gd); poly({ { -14, -3 }, { -7, -3 }, { -10, 11 }, { -17, 10 } }, gd);
                g.fillStyle = sh(.14); g.beginPath(); g.moveTo(-3, -3); g.lineTo(4, -3); g.quadraticCurveTo(6, 6, 8, 15); g.lineTo(1, 16); g.quadraticCurveTo(0, 6, -3, -3); g.fill();
                rr(-21, -13, 34, 10, 1.4, sh(.2)); rr(13, -13, 24, 8, 1.4, sh(.1));
                for (int i = 0; i < 5; ++i) rr(15 + i * 4.4, -10.6, 2.6, 3, .6, sh(.03));
                rr(37, -10.2, 16, 2.2, .5, metal); poly({ { 34, -13 }, { 36, -19 }, { 38, -13 } }, sh(.1)); rr(52, -11, 3, 4, .6, metal);
                for (int i = 0; i < 8; ++i) rr(-19 + i * 3.6, -15.4, 2, 2.4, .3, sh(.06));
                guard(-5, -3, 3.4, sh(.1), 1.4); hl(-20, -12.6, 36, -12.6);
                break;
            case 6:  // ak-47
                sc(106); poly({ { -18, -11 }, { -50, -6 }, { -50, 5 }, { -44, 5 }, { -18, -3 } }, gwood); poly({ { -12, -3 }, { -5, -3 }, { -8, 11 }, { -15, 10 } }, gwood2);
                g.fillStyle = sh(.16); g.beginPath(); g.moveTo(-2, -4); g.lineTo(6, -4); g.bezierCurveTo(9, 4, 13, 11, 19, 17); g.lineTo(12, 20); g.bezierCurveTo(7, 13, 2, 5, -2, -4); g.fill();
                rr(-19, -13, 34, 9, 1.2, sh(.24)); rr(15, -12.5, 18, 6.5, 2, gwood); rr(15, -15, 18, 2.4, 1, sh(.18));
                rr(33, -10.6, 20, 2.2, .5, metal); poly({ { 47, -10.6 }, { 48.5, -16 }, { 50, -10.6 } }, metal); rr(52, -11.4, 3, 3.6, .6, metal);
                poly({ { -10, -13 }, { -4, -17 }, { 4, -13 } }, sh(.2)); guard(-6, -3.6, 3.4, sh(.2), 1.4); hl(-18, -12.6, 14, -12.6);
                break;
            case 7:  // scar
                sc(106); poly({ { -22, -14 }, { -46, -13 }, { -48, -4 }, { -44, 1 }, { -34, 1 }, { -30, -6 }, { -22, -6 } }, sh(.1)); poly({ { -15, -3 }, { -8, -3 }, { -11, 11 }, { -18, 10 } }, gd);
                poly({ { 0, -4 }, { 7, -4 }, { 8, 14 }, { 1, 15 } }, sh(.14)); rr(-21, -7, 30, 5, 1, sh(.12)); rr(-22, -15, 56, 9, 1.4, sh(.24));
                for (int i = 0; i < 14; ++i) rr(-20 + i * 3.8, -17.4, 2.2, 2.4, .3, gd);
                rr(34, -12.4, 16, 2.6, .5, metal); rr(48, -13.6, 6, 5, 1, gd); guard(-4, -2.4, 3.4, sh(.12), 1.4); hl(-21, -14.6, 33, -14.6);
                break;
            case 8:  // sniper
                sc(120); line(24, -6, 16, 12, metal, 1.4); line(24, -6, 32, 12, metal, 1.4);
                poly({ { -22, -13 }, { -50, -11 }, { -52, 3 }, { -44, 3 }, { -40, -3 }, { -22, -3 } }, sh(.1)); poly({ { -14, -3 }, { -7, -3 }, { -10, 10 }, { -17, 9 } }, gd); rr(-2, -4, 9, 12, 1, sh(.12));
                rr(-24, -14, 48, 11, 1.4, sh(.22)); rr(24, -10.6, 36, 3, .6, metal); rr(58, -13, 7, 8, 1, sh(.14));
                for (int i = 0; i < 3; ++i) rr(59.2 + i * 2, -12, 1, 6, .2, sh(.04));
                rr(-12, -22, 28, 4.6, 2.3, sh(.06)); rr(-16, -23, 6, 6.6, 1.6, sh(.1)); rr(12, -23.6, 7, 7.8, 1.6, sh(.1)); rr(-4, -18, 3, 4, .4, sh(.1)); rr(6, -18, 3, 4, .4, sh(.1));
                dot(18.4, -19.7, 1.6, P.glow(-30, 0, .62, .85)); guard(-5, -2.6, 3.4, sh(.12), 1.4); hl(-23, -13.6, 23, -13.6);
                break;
            default:  // bazuca
                sc(124); rr(-50, -4, 84, 6, 2, sh(.18)); rr(-12, -5.4, 22, 8.8, 2.4, gwood); poly({ { -50, -6.5 }, { -56, -8 }, { -56, 4 }, { -50, 2.5 } }, sh(.14));
                poly({ { -6, 2 }, { 0, 2 }, { -2, 13 }, { -8, 12 } }, gd); poly({ { 16, 2 }, { 22, 2 }, { 20, 12 }, { 14, 11 } }, gd); rr(4, -11, 5, 6, 1, sh(.12));
                rr(34, -3, 8, 4, 1, P.glow(30, 0, .4, 1)); poly({ { 40, -1 }, { 46, -9 }, { 55, -9 }, { 66, -1 }, { 55, 7 }, { 46, 7 } }, P.glow(0, 0, .62, 1));
                poly({ { 46, -9 }, { 55, -9 }, { 66, -1 }, { 40, -1 } }, P.glow(0, 0, .74, 1)); poly({ { 66, -1 }, { 70, -2 }, { 70, 0 } }, metal); hl(-49, -3.6, 33, -3.6);
                break;
            }
        };
        const double angJ = blades ? R(-.12, .12) : R(-.06, .06);
        const double ang = blades ? -.62 + angJ : -.14 + angJ;
        // cenário de cada lâmina (atrás dela, sem girar)
        auto crescent = [&](double mx, double my, double r) {
            g.save(); g.beginPath(); g.arc(mx, my, r, 0, 7); g.clip();
            g.fillStyle = P.glow(40, 0, .86, .95); g.beginPath(); g.arc(mx, my, r, 0, 7); g.arc(mx + r * .42, my - r * .25, r * .9, 0, 7); g.fill(true);
            g.restore();
        };
        auto bird = [&](double x, double y, double k, const QBrush& c) {
            g.fillStyle = c; g.beginPath(); g.moveTo(x - 6 * k, y - 1 * k); g.quadraticCurveTo(x - 3 * k, y - 4 * k, x, y);
            g.quadraticCurveTo(x + 3 * k, y - 4 * k, x + 6 * k, y - 1 * k); g.quadraticCurveTo(x + 3 * k, y - 1.5 * k, x, y + 1.4 * k);
            g.quadraticCurveTo(x - 3 * k, y - 1.5 * k, x - 6 * k, y - 1 * k); g.fill();
        };
        auto BD = [&](int k) {
            switch (k) {
            case 0: {  // punhal: telhados à noite e lua fina
                stars(jsRound(16 * wide), VH * .6);
                const double mx = CX + R(14, 26), my = R(16, 26);
                crescent(mx, my, 11);
                g.fillStyle = P.shade(.05); g.beginPath(); g.moveTo(0, VH);
                double x = -4;
                while (x < VW) {
                    const double w = R(14, 24), h = R(14, 26);
                    g.lineTo(x, VH - h); const double peak = R(4, 9); g.lineTo(x + w / 2, VH - h - peak); g.lineTo(x + w, VH - h); x += w;
                }
                g.lineTo(VW, VH); g.fill();
                const int lit = jsRound(2 + grow * 5);
                for (int i = 0; i < lit; ++i) {
                    SubR r = sub(QStringLiteral("jn%1").arg(i));
                    const double wx = r(4, VW - 4), wy = VH - r(4, 12);
                    g.fillStyle = P.glow(40, 0, .78, 1); g.fillRect(wx, wy, 1.6, 2);
                }
                break;
            }
            case 1: {  // katana: sol grande, galho de cerejeira e pétalas
                const double sx = CX + R(-16, -6), sy = CY - 8;
                dot(sx, sy, 30, P.glow(-40, 0, .5, .45)); dot(sx, sy, 24, P.glow(-40, 0, .58, .8));
                g.strokeStyle = P.shade(.03); g.lineWidth = 2.6; g.beginPath();
                const double by = R(8, 16);
                g.moveTo(VW + 2, by); g.quadraticCurveTo(VW * .75, by + 10, VW * .5, by + 4); g.stroke();
                g.lineWidth = 1.2; g.beginPath(); g.moveTo(VW * .68, by + 7); g.quadraticCurveTo(VW * .62, by + 16, VW * .56, by + 20); g.stroke();
                for (int i = 0; i < 14; ++i) {
                    SubR r = sub(QStringLiteral("fl%1").arg(i));
                    const double fx = VW * r(.5, 1), fy = by + r(-3, 20), fr = r(1.2, 2.2);
                    dot(fx, fy, fr, P.glow(-60, 0, .82, .95));
                }
                const int pet = jsRound((8 + grow * 26) * wide);
                for (int i = 0; i < pet; ++i) {
                    SubR r = sub(QStringLiteral("sk%1").arg(i));
                    const double a = r(.6, 1);
                    g.fillStyle = P.glow(-60, 0, .85, a); g.beginPath();
                    const double ex = r(0, VW), ey = r(0, VH), rot = r(0, PI);
                    g.ellipse(ex, ey, 1.7, 1, rot, 0, 7); g.fill();
                }
                break;
            }
            case 2: {  // espada europeia: escudo de brasão atrás
                const double cx = CX + R(-4, 4), cy = CY - 4;
                auto shield = [&]() {
                    g.beginPath(); g.moveTo(cx - 24, cy - 28); g.lineTo(cx + 24, cy - 28); g.lineTo(cx + 24, cy + 2);
                    g.quadraticCurveTo(cx + 22, cy + 24, cx, cy + 34); g.quadraticCurveTo(cx - 22, cy + 24, cx - 24, cy + 2); g.closePath();
                };
                shield(); g.fillStyle = P.shade(.22); g.fill(); g.save(); shield(); g.clip(); g.fillStyle = P.glow(-40, 0, .42, 1);
                const int kk = int(std::floor(rnd() * 3));
                if (kk == 0) { g.fillRect(cx - 26, cy - 30, 26, 30); g.fillRect(cx, cy, 26, 40); }
                else if (kk == 1) {
                    g.beginPath(); g.moveTo(cx - 26, cy + 14); g.lineTo(cx, cy - 8); g.lineTo(cx + 26, cy + 14); g.lineTo(cx + 26, cy + 24);
                    g.lineTo(cx, cy + 2); g.lineTo(cx - 26, cy + 24); g.fill();
                } else { g.save(); g.translate(cx, cy); g.rotate(.7); g.fillRect(-60, -6, 120, 12); g.restore(); }
                g.restore(); shield(); g.strokeStyle = P.glow(40, 0, .62, 1); g.lineWidth = 1.8; g.stroke();
                const int fl = jsRound(grow * 4);
                for (int i = 0; i < fl; ++i) {
                    SubR r = sub(QStringLiteral("fd%1").arg(i));
                    const double x = cx + r(-14, 14), y = cy + r(-20, 18);
                    g.strokeStyle = P.glow(40, 0, .66, .9); g.lineWidth = .8; g.beginPath(); g.arc(x, y, 1.6, 0, 7); g.stroke();
                }
                break;
            }
            case 3: {  // khopesh: sol com raios, pirâmides e dunas
                const double sx = CX + R(12, 24), sy = R(18, 26);
                for (int i = 0; i < 11; ++i) {
                    const double a = PI * .18 + i / 10.0 * PI * .64;
                    line(sx, sy, sx + std::cos(a) * 34, sy + std::sin(a) * 34, P.glow(40, 0, .75, .22), 1.2);
                }
                dot(sx, sy, 9, P.glow(40, 0, .82, .95));
                auto pyr = [&](double x, double b, double hh) {
                    poly({ { x - hh * .85, b }, { x, b - hh }, { x + hh * .85, b } }, P.shade(.18));
                    poly({ { x, b - hh }, { x + hh * .85, b }, { x + hh * .2, b } }, P.shade(.28));
                };
                pyr(CX - 22, VH - 16, 30); pyr(CX + 14, VH - 16, 20);
                if (wide > 1.2) pyr(CX + 44, VH - 16, 14);
                g.fillStyle = P.shade(.12); g.beginPath(); g.moveTo(0, VH);
                for (double x = 0; x <= VW; x += 3) g.lineTo(x, VH - 16 + std::sin(x * .06 + R(0, .01)) * 2.6);
                g.lineTo(VW, VH); g.fill();
                break;
            }
            case 4: {  // sabre chinês: montanhas de névoa e lanternas
                for (int l = 0; l < 3; ++l) {
                    g.fillStyle = P.shade(.26 - l * .08, .92); g.beginPath(); g.moveTo(0, VH);
                    double x = -6;
                    while (x < VW + 6) {
                        const double w = R(12, 20), hh = R(26, 46) - l * 9;
                        g.lineTo(x, VH - 8 - l * 6);
                        g.quadraticCurveTo(x + w * .05, VH - hh - l * 6, x + w * .5, VH - hh - l * 6 - 4);
                        g.quadraticCurveTo(x + w * .95, VH - hh - l * 6, x + w, VH - 8 - l * 6);
                        x += w * .8;
                    }
                    g.lineTo(VW, VH); g.fill();
                    g.fillStyle = P.glow(0, 0, .9, .06); g.fillRect(0, VH - 26 - l * 6, VW, 14);
                }
                const double lanterns[2][2] = { { CX - 30, 22 }, { CX + 30, 16 } };
                for (const auto& L : lanterns) {
                    const double lx = L[0], ly = L[1];
                    line(lx, 0, lx, ly - 6, P.shade(.4), .5); halo(lx, ly, 12, -40, .6, .5);
                    g.fillStyle = P.glow(-40, 0, .52, 1); g.beginPath(); g.ellipse(lx, ly, 4.4, 5.6, 0, 0, 7); g.fill();
                    g.fillStyle = P.shade(.06); g.fillRect(lx - 2.4, ly - 6.4, 4.8, 1.6); g.fillRect(lx - 2.4, ly + 4.8, 4.8, 1.6);
                    line(lx, ly + 6.4, lx, ly + 11, P.glow(-40, 0, .6, 1), .8);
                }
                break;
            }
            case 5: {  // alabarda: muralha e estandartes
                g.fillStyle = P.shade(.16); g.fillRect(0, VH - 28, VW, 28);
                for (double x = 0; x < VW; x += 9) g.fillRect(x, VH - 33, 5, 5);
                g.strokeStyle = P.shade(.08, .6); g.lineWidth = .4;
                for (double y = VH - 22; y < VH; y += 6) { g.beginPath(); g.moveTo(0, y); g.lineTo(VW, y); g.stroke(); }
                const QColor bc[2] = { ink2, P.glow(-40, 0, .46, 1) };
                const double bxs[2] = { CX - 26, CX + 26 };
                for (int i = 0; i < 2; ++i) {
                    const double bx = bxs[i];
                    line(bx - 8, 4, bx + 8, 4, P.shade(.4), 1);
                    g.fillStyle = bc[i]; g.beginPath(); g.moveTo(bx - 7, 4); g.lineTo(bx + 7, 4); g.lineTo(bx + 7, 44); g.lineTo(bx, 38); g.lineTo(bx - 7, 44); g.closePath(); g.fill();
                    g.strokeStyle = P.glow(40, 0, .66, 1); g.lineWidth = 1; g.beginPath();
                    if (i) { g.moveTo(bx, 12); g.lineTo(bx, 30); g.moveTo(bx - 4, 18); g.lineTo(bx + 4, 18); }
                    else g.arc(bx, 20, 4, 0, 7);
                    g.stroke();
                }
                break;
            }
            case 6: {  // montante: campo de batalha, lâminas fincadas, fumaça e estandarte rasgado
                halo(CX, VH - 6, 70, 20, .6, .3);
                const int n = 3 + jsRound(grow * 4);
                for (int i = 0; i < n; ++i) {
                    SubR r = sub(QStringLiteral("ms%1").arg(i));
                    const double x = r(4, VW - 4), b = VH - 12 + r(-2, 3), a = -PI / 2 + r(-.35, .35), l = r(14, 26);
                    line(x, b, x + std::cos(a) * l, b + std::sin(a) * l, P.shade(.03), 1.4);
                    const double cx = x + std::cos(a) * l * .78, cy = b + std::sin(a) * l * .78;
                    line(cx - std::sin(a) * -3.6, cy + std::cos(a) * -3.6, cx + std::sin(a) * -3.6, cy - std::cos(a) * -3.6, P.shade(.03), 1.2);
                }
                g.fillStyle = P.shade(.06); g.beginPath(); g.moveTo(0, VH);
                for (double x = 0; x <= VW; x += 4) g.lineTo(x, VH - 12 + std::sin(x * .09) * 1.6);
                g.lineTo(VW, VH); g.fill();
                const double side = rnd() < .5 ? -1 : 1;
                const double px = CX + side * R(28, 36);
                line(px, VH - 12, px, 12, P.shade(.03), 1.2);
                g.fillStyle = P.glow(-40, 0, .4, 1); g.beginPath(); g.moveTo(px, 13); g.lineTo(px + 16, 15); g.lineTo(px + 12, 19); g.lineTo(px + 17, 23);
                g.lineTo(px + 9, 25); g.lineTo(px + 13, 29); g.lineTo(px, 30); g.closePath(); g.fill();
                break;
            }
            default: {  // foice: lua cheia, corvos e almas subindo
                const double mx = CX + R(-10, 10), my = CY - 10;
                halo(mx, my, 48, 0, .85, .3); dot(mx, my, 26, P.glow(0, 0, .86, .92));
                for (int i = 0; i < 6; ++i) {
                    SubR r = sub(QStringLiteral("cr%1").arg(i));
                    const double x = mx + r(-16, 16), y = my + r(-16, 16), rad = r(1.5, 4);
                    dot(x, y, rad, P.glow(0, 0, .76, .5));
                }
                const int crows = 2 + jsRound(grow * 5);
                for (int i = 0; i < crows; ++i) {
                    SubR r = sub(QStringLiteral("cv%1").arg(i));
                    const double x = mx + r(-34, 34) * wide, y = my + r(-24, 18), k2 = r(.6, 1.1);
                    bird(x, y, k2, P.shade(.02));
                }
                const int souls = jsRound(grow * 14 * wide);
                for (int i = 0; i < souls; ++i) {
                    SubR r = sub(QStringLiteral("al%1").arg(i));
                    const double x = r(4, VW - 4), y = r(30, VH - 4);
                    halo(x, y, 3.2, 0, .9, .5); dot(x, y, .7, P.glow(0, 0, .95, 1));
                }
                g.fillStyle = P.shade(.04); g.beginPath(); g.moveTo(0, VH);
                for (double x = 0; x <= VW; x += 2) { const double j = R(.4, 1.2); g.lineTo(x, VH - 8 - (std::fmod(x, 6) < 3 ? 3 : 0) * j); }
                g.lineTo(VW, VH); g.fill();
                break;
            }
            }
        };
        // cenário de cada arma de fogo: o lugar onde ela está conta a história
        auto bill = [&](double x, double y, double w, double h, double a) {
            g.save(); g.translate(x, y); g.rotate(a); g.fillStyle = P.glow(60, 0, .62, 1); g.fillRect(-w / 2, -h / 2, w, h);
            g.strokeStyle = P.glow(60, 0, .4, .9); g.lineWidth = .5; g.strokeRect(-w / 2 + 1, -h / 2 + 1, w - 2, h - 2);
            g.fillStyle = P.paper; g.fillRect(-1.2, -h / 2, 2.4, h); g.restore();
        };
        auto shell = [&](double x, double y, double a) {
            g.save(); g.translate(x, y); g.rotate(a); g.fillStyle = P.glow(40, 0, .62, 1); g.beginPath(); g.roundRect(-2.4, -1, 4.8, 2, 1); g.fill();
            g.fillStyle = P.glow(40, 0, .45, 1); g.fillRect(1.4, -1, 1, 2); g.restore();
        };
        auto pines = [&](int n, double base, double hmin, double hmax, const QBrush& c) {
            for (int i = 0; i < n; ++i) {
                const double x = R(-4, VW + 4), h = R(hmin, hmax), w = h * .32;
                g.fillStyle = c; g.beginPath();
                for (int tt = 0; tt < 3; ++tt) {
                    const double ty = base - h + tt * h * .28, tw = w * (.45 + tt * .3);
                    g.moveTo(x, ty); g.lineTo(x + tw, ty + h * .42); g.lineTo(x - tw, ty + h * .42); g.closePath();
                }
                g.fill();
            }
        };
        bool hasLZ = false;
        QPointF LZ;
        const QPointF door(CX + 16, CY - 8);
        auto GB = [&](int k) {
            switch (k) {
            case 0: {  // pistola: mesa de investigação
                g.fillStyle = P.shade(.3); g.fillRect(0, 0, VW, VH);
                for (double y = 3; y < VH; y += 5.5) {
                    g.strokeStyle = P.shade(.24, .8); g.lineWidth = .5; g.beginPath();
                    const double ph = R(0, 6);
                    for (double x = 0; x <= VW; x += 4) g.lineTo(x, y + std::sin(x * .05 + ph) * 1.2);
                    g.stroke();
                }
                halo(CX - 30, CY - 34, 72, 40, .82, .3);
                g.save(); g.translate(CX - 18, CY - 12); g.rotate(-.22); g.fillStyle = P.shade(.02, .35); g.fillRect(-15, -19, 32, 40);
                g.fillStyle = P.paper; g.fillRect(-16, -20, 32, 40);
                g.strokeStyle = P.paperInk; g.lineWidth = .4;
                for (int kk = 0; kk < 9; ++kk) { g.beginPath(); g.moveTo(-12, -15 + kk * 3.6); g.lineTo(kk % 4 == 3 ? 2 : 12, -15 + kk * 3.6); g.stroke(); }
                g.strokeStyle = P.glow(-40, 0, .5, 1); g.lineWidth = .7; g.beginPath(); g.ellipse(3, -4.2, 8, 2.6, 0, 0, 7); g.stroke(); g.restore();
                g.save(); g.translate(CX + 16, CY - 18); g.rotate(.12); g.fillStyle = P.shade(.02, .35); g.fillRect(-19, -14, 40, 30);
                g.fillStyle = P.glow(35, 0, .64, 1); g.fillRect(-20, -15, 40, 30); g.fillRect(-20, -18.5, 14, 4);
                g.fillStyle = P.paper; g.fillRect(-15, -11, 15, 18); g.fillStyle = P.shade(.18); g.fillRect(-13.5, -9.5, 12, 11);
                dot(-7.5, -6, 2.2, P.shade(.5)); g.fillStyle = P.shade(.5); g.beginPath(); g.ellipse(-7.5, 1.5, 4, 3, 0, PI, 0); g.fill();
                line(-14, -12.5, -14, -5, P.shade(.6), 1.1);
                g.fillStyle = P.paperInk; g.fillRect(3, -8, 13, 1.2); g.fillRect(3, -4, 10, 1.2); g.fillRect(3, 0, 13, 1.2); g.fillRect(3, 4, 8, 1.2);
                g.fillStyle = P.glow(-40, 0, .5, 1); g.fillRect(-18, 9, 10, 3.4); g.restore();
                dot(CX - 36, CY + 26, 8.4, P.shade(.02, .35)); dot(CX - 37, CY + 25, 8, P.paper); dot(CX - 37, CY + 25, 6, P.shade(.1));
                dot(CX - 38.5, CY + 23.5, 1.4, P.glow(30, 0, .5, .8)); line(CX - 29, CY + 25, CX - 25, CY + 25, P.paper, 2.4);
                const int nb = 1 + jsRound(grow * 5);
                for (int i = 0; i < nb; ++i) {
                    SubR r = sub(QStringLiteral("bl%1").arg(i));
                    const double x = r(CX - 14, CX + 44), y = r(CY + 28, CY + 42), a = r(0, PI);
                    shell(x, y, a);
                }
                g.strokeStyle = P.glow(30, 0, .2, .6); g.lineWidth = 1; g.beginPath(); g.ellipse(CX + 36, CY + 30, 6, 6, 0, .4, 5.6); g.stroke();
                break;
            }
            case 1: {  // desert eagle: mesa de pôquer
                g.fillStyle = MsCanvas::radial(CX, CY, 8, CX, CY, 84, { { 0, P.glow(-20, 0, .36, 1) }, { 1, P.glow(-20, 0, .14, 1) } });
                g.fillRect(0, 0, VW, VH);
                halo(CX, CY - 30, 60, 40, .8, .22);
                g.strokeStyle = P.glow(30, 0, .22, 1); g.lineWidth = 8; g.beginPath(); g.ellipse(CX, CY + 8, VW * .55 + 24, 60, 0, 0, 7); g.stroke();
                auto card = [&](double x, double y, double a, bool red, const QString& lab) {
                    g.save(); g.translate(x, y); g.rotate(a); g.fillStyle = P.shade(.02, .4); g.fillRect(-5.5, -8, 13, 17);
                    g.fillStyle = P.paper; g.beginPath(); g.roundRect(-6.5, -9, 13, 17, 1.4); g.fill();
                    const QColor c = red ? P.glow(-40, 0, .5, 1) : P.paperInk;
                    g.fillStyle = c; g.setFont(QStringLiteral("Georgia"), 5, QFont::Bold); g.textAlign = MsCanvas::Left; g.fillText(lab, -5, -3.4);
                    dot(0, 2, 2.3, c); g.restore();
                };
                card(CX - 32, CY - 22, -.42, true, QStringLiteral("A")); card(CX - 23, CY - 25, -.14, false, QStringLiteral("A"));
                card(CX - 14, CY - 24, .18, true, QStringLiteral("K"));
                auto chip = [&](double x, double y, const QColor& c) {
                    dot(x + .6, y + .8, 4.2, P.shade(.02, .4)); dot(x, y, 4.2, c);
                    g.strokeStyle = P.paper; g.lineWidth = 1; g.setLineDash({ 1.4, 1.6 }); g.beginPath(); g.arc(x, y, 3.2, 0, 7); g.stroke(); g.setLineDash({});
                    dot(x, y, 1.7, c);
                };
                const QColor cols[4] = { ink2, P.glow(-40, 0, .48, 1), P.paper2, P.shade(.12) };
                const int nc = 3 + jsRound(grow * 10);
                for (int i = 0; i < nc; ++i) {
                    SubR r = sub(QStringLiteral("ch%1").arg(i));
                    const double x = r(CX + 4, CX + 46), y = r(CY - 38, CY - 8);
                    const int ci = int(std::floor(r(0, 4)));
                    chip(x, y, cols[ci]);
                }
                dot(CX + 36, CY + 28, 7, P.shade(.06)); dot(CX + 36, CY + 28, 5, P.shade(.14));
                g.save(); g.translate(CX + 36, CY + 28); g.rotate(-.5); g.fillStyle = P.glow(30, 0, .36, 1); g.fillRect(-2, -1.4, 14, 2.8); g.restore();
                dot(CX + 36 + 12 * std::cos(-.5), CY + 28 + 12 * std::sin(-.5), 1.4, P.glow(-30, 0, .6, 1));
                for (int kk = 0; kk < 6; ++kk) dot(CX + 48 + kk * 1.5, CY + 20 - kk * 3, 1.2 + kk * .5, P.glow(0, 0, .92, .24 - kk * .03));
                const int nbl = jsRound(grow * 4);
                for (int i = 0; i < nbl; ++i) {
                    SubR r = sub(QStringLiteral("bi%1").arg(i));
                    const double x = r(CX - 44, CX - 14), y = r(CY + 18, CY + 38), a = r(-.6, .6);
                    bill(x, y, 16, 8, a);
                }
                break;
            }
            case 2: {  // uzi: a bolsa do assalto
                g.fillStyle = P.shade(.3); g.fillRect(0, 0, VW, VH);
                for (double x = std::fmod(CX, 20) - 10; x < VW; x += 20) line(x, 0, x, VH, P.shade(.22), .6);
                for (double y = 6; y < VH; y += 20) line(0, y, VW, y, P.shade(.22), .6);
                halo(CX, CY, 60, 0, .75, .18);
                g.fillStyle = P.shade(.02, .4); g.beginPath(); g.ellipse(CX + 4, CY + 5, 41, 27, -.08, 0, 7); g.fill();
                g.fillStyle = P.shade(.06); g.beginPath(); g.ellipse(CX + 2, CY + 2, 40, 26, -.08, 0, 7); g.fill();
                g.fillStyle = P.shade(.14); g.beginPath(); g.ellipse(CX + 2, CY, 32, 17, -.08, 0, 7); g.fill();
                g.strokeStyle = ink2; g.lineWidth = 1.2; g.setLineDash({ .8, 1 }); g.beginPath(); g.ellipse(CX + 2, CY, 33.5, 18.5, -.08, 0, 7); g.stroke(); g.setLineDash({});
                const int ns = 3 + jsRound(grow * 8);
                for (int i = 0; i < ns; ++i) {
                    SubR r = sub(QStringLiteral("ms%1").arg(i));
                    const double x = CX + 2 + r(-22, 22), y = CY + r(-10, 10), a = r(-.3, .3);
                    bill(x, y, 15, 8, a);
                }
                g.strokeStyle = P.shade(.04); g.lineWidth = 2.6; g.beginPath(); g.moveTo(CX - 18, CY - 14); g.quadraticCurveTo(CX - 6, CY - 34, CX + 8, CY - 15); g.stroke();
                const int sp = jsRound(grow * 4);
                for (int i = 0; i < sp; ++i) {
                    SubR r = sub(QStringLiteral("sp%1").arg(i));
                    const double x = r(6, VW - 6), y = r(CY + 26, VH - 6), a = r(-1, 1);
                    bill(x, y, 15, 8, a);
                }
                break;
            }
            case 3: {  // ump: no capô da viatura, de madrugada, com o giroflex
                g.fillStyle = P.shade(.06);
                const int nbld = jsRound(6 * wide);
                for (int i = 0; i < nbld; ++i) {
                    const double x = i * VW / nbld - 2, w = R(14, 22), h = R(30, 56);
                    g.fillRect(x, CY + 6 - h, w, h);
                    for (int kk = 0; kk < 4; ++kk)
                        if (rnd() < .5) {
                            g.fillStyle = P.glow(40, 0, .76, 1);
                            const double wx = x + R(2, w - 4), wy = CY + 6 - h + R(4, h - 6);
                            g.fillRect(wx, wy, 1.8, 2.2); g.fillStyle = P.shade(.06);
                        }
                }
                halo(CX - 30, CY - 8, 46, -60, .6, .55); halo(CX + 30, CY - 8, 46, 60, .6, .55);
                g.save(); g.translate(CX, CY - 22); g.rotate(-.16); g.fillStyle = ink2; g.fillRect(-VW, -3.2, VW * 2, 6.4); g.fillStyle = P.shade(.04);
                for (double x = -VW; x < VW; x += 10) { g.beginPath(); g.moveTo(x, -3.2); g.lineTo(x + 4, -3.2); g.lineTo(x + 1, 3.2); g.lineTo(x - 3, 3.2); g.fill(); }
                g.restore();
                g.fillStyle = MsCanvas::linear(0, CY + 4, 0, VH, { { 0, P.shade(.36) }, { 1, P.shade(.1) } });
                g.beginPath(); g.moveTo(-2, CY + 12); g.quadraticCurveTo(CX, CY, VW + 2, CY + 12); g.lineTo(VW + 2, VH); g.lineTo(-2, VH); g.fill();
                g.strokeStyle = P.glow(0, 0, .9, .45); g.lineWidth = .8; g.beginPath(); g.moveTo(4, CY + 11); g.quadraticCurveTo(CX, CY + 1.2, VW - 4, CY + 11); g.stroke();
                g.fillStyle = P.glow(-60, 0, .65, .35); g.beginPath(); g.ellipse(CX - 24, CY + 22, 14, 3, 0, 0, 7); g.fill();
                g.fillStyle = P.glow(60, 0, .65, .35); g.beginPath(); g.ellipse(CX + 26, CY + 24, 14, 3, 0, 0, 7); g.fill();
                break;
            }
            case 4: {  // mp5: corredor escuro, porta arrombada, fumaça
                const double vx = door.x(), vy = door.y();
                poly({ { 0, 0 }, { VW, 0 }, { vx + 10, vy - 14 }, { vx - 10, vy - 14 } }, P.shade(.05));
                poly({ { 0, 0 }, { vx - 10, vy - 14 }, { vx - 10, vy + 16 }, { 0, VH } }, P.shade(.1));
                poly({ { VW, 0 }, { vx + 10, vy - 14 }, { vx + 10, vy + 16 }, { VW, VH } }, P.shade(.08));
                poly({ { 0, VH }, { vx - 10, vy + 16 }, { vx + 10, vy + 16 }, { VW, VH } }, P.shade(.14));
                halo(vx, vy, 40, 40, .85, .5); g.fillStyle = P.glow(40, 0, .88, 1); g.fillRect(vx - 7, vy - 12, 14, 28);
                poly({ { vx + 7, vy - 12 }, { vx + 13, vy - 15 }, { vx + 13, vy + 19 }, { vx + 7, vy + 16 } }, P.shade(.04));
                g.save(); g.globalAlpha = .22;
                poly({ { vx - 7, vy + 16 }, { vx + 7, vy + 16 }, { vx + 34, VH }, { vx - 34, VH } },
                     MsCanvas::linear(0, vy + 16, 0, VH, { { 0, P.glow(40, 0, .85, 1) }, { 1, P.glow(40, 0, .85, 0) } }));
                g.restore();
                const int sm = jsRound((14 + grow * 30) * wide);
                for (int i = 0; i < sm; ++i) {
                    SubR r = sub(QStringLiteral("fm%1").arg(i));
                    const double x = r(0, VW), y = r(0, VH), rad = r(4, 10);
                    dot(x, y, rad, P.glow(0, 0, .85, .035));
                }
                const int others = jsRound(grow * 2);
                for (int i = 0; i < others; ++i) {
                    SubR r = sub(QStringLiteral("lz%1").arg(i));
                    const double y0 = r(20, VH - 10), ex = vx + r(-4, 4), ey = vy + r(-6, 8);
                    g.save(); g.shadowColor = P.glow(-30, 0, .6, 1); g.shadowBlur = 3 * s; line(-2, y0, ex, ey, P.glow(-30, 0, .62, .7), .5); g.restore();
                }
                break;
            }
            case 5: {  // m4: encostado na trincheira, sinalizador no céu
                halo(CX, VH, 80, 30, .6, .35);
                const double fx = CX + R(10, 30), fy = R(12, 22);
                g.strokeStyle = P.glow(0, 0, .9, .25); g.lineWidth = 1; g.beginPath(); g.moveTo(CX - 50, VH * .4); g.quadraticCurveTo(CX - 10, fy - 30, fx, fy); g.stroke();
                halo(fx, fy, 26, -30, .7, .6); dot(fx, fy, 2.2, P.glow(-30, 0, .95, 1));
                const int fl = jsRound(grow * 3);
                for (int i = 0; i < fl; ++i) {
                    SubR r = sub(QStringLiteral("fl%1").arg(i));
                    const double x = r(6, VW - 6), y = r(8, 30);
                    halo(x, y, 8, -30, .7, .4); dot(x, y, 1, P.glow(-30, 0, .95, 1));
                }
                const double wy = CY - 14;
                for (double x = 6; x < VW; x += 26) { line(x - 3, wy - 6, x + 3, wy + 6, P.shade(.04), 1.2); line(x + 3, wy - 6, x - 3, wy + 6, P.shade(.04), 1.2); }
                g.strokeStyle = P.shade(.04); g.lineWidth = .6; g.beginPath();
                for (double x = 0; x <= VW; x += 1) g.lineTo(x, wy - 1 + std::sin(x * 1.2) * 1.4);
                g.stroke();
                for (int row = 0; row < 8; ++row) {
                    const double y = CY - 8 + row * 7.2;
                    const int off = row % 2 ? 8 : 0;
                    for (int x = -16 + off; x < VW + 8; x += 16) {
                        SubR r = sub(QStringLiteral("sb%1_%2").arg(row).arg(x));
                        g.fillStyle = P.shade(.02, .5); g.beginPath(); g.roundRect(x + .8, y + 1, 15.4, 7.2, 3.4); g.fill();
                        g.fillStyle = P.shade(r(.26, .36)); g.beginPath(); g.roundRect(x, y, 15.4, 7.2, 3.4); g.fill();
                        line(x + 3, y + 3.6, x + 12.4, y + 3.6, P.shade(.18, .6), .4);
                    }
                }
                break;
            }
            case 6: {  // ak-47: parede em ruínas no fim de tarde
                g.fillStyle = MsCanvas::linear(0, 0, 0, VH, { { 0, P.glow(10, 0, .3, 0) }, { 1, P.glow(30, 0, .55, .6) } });
                g.fillRect(0, 0, VW, VH);
                halo(CX + 26, CY, 52, 35, .78, .6); dot(CX + 26, CY, 16, P.glow(35, 0, .82, 1));
                auto top = [&](double x) { return CY - 22 + std::abs(std::sin(x * .09 + 1.3)) * 8 + (x > CX + 10 && x < CX + 30 ? 18 : 0); };
                g.fillStyle = P.shade(.3); g.beginPath(); g.moveTo(-2, VH);
                for (double x = -2; x <= VW + 2; x += 3) g.lineTo(x, top(x) + R(-1.5, 1.5));
                g.lineTo(VW + 2, VH); g.fill();
                for (double y = CY - 20; y < VH; y += 5) {
                    const int off = jsRound((y - CY) / 5) % 2 ? 6 : 0;
                    for (double x = -6 + off; x < VW; x += 12)
                        if (y > top(x + 6) + 2) { g.strokeStyle = P.shade(.22); g.lineWidth = .5; g.strokeRect(x, y, 12, 5); }
                }
                const int nh = 4 + jsRound(grow * 10);
                for (int i = 0; i < nh; ++i) {
                    SubR r = sub(QStringLiteral("bh%1").arg(i));
                    const double x = r(4, VW - 4), y = r(CY - 10, VH - 16);
                    if (y < top(x) + 3) continue;
                    dot(x, y, 1.4, P.shade(.5)); dot(x, y, .9, P.shade(.04));
                }
                g.fillStyle = P.glow(30, 0, .42, 1); g.beginPath(); g.moveTo(0, VH);
                for (double x = 0; x <= VW; x += 3) g.lineTo(x, VH - 12 + std::sin(x * .07) * 2);
                g.lineTo(VW, VH); g.fill();
                for (int i = 0; i < 6; ++i) {
                    SubR r = sub(QStringLiteral("rb%1").arg(i));
                    const double a = r(0, VW), b = r(0, VW), c = r(0, VW);
                    poly({ { a, VH - 11 }, { b + 3, VH - 15 }, { c + 6, VH - 11 } }, P.shade(.22));
                }
                break;
            }
            case 7: {  // scar: tronco caído na floresta, névoa
                pines(jsRound(9 * wide), CY + 10, 40, 64, P.shade(.12)); g.fillStyle = P.glow(0, 0, .9, .08); g.fillRect(0, CY - 6, VW, 18);
                pines(jsRound(6 * wide), CY + 22, 34, 52, P.shade(.06));
                const int ff = jsRound(grow * 12 * wide);
                for (int i = 0; i < ff; ++i) {
                    SubR r = sub(QStringLiteral("ff%1").arg(i));
                    const double x = r(4, VW - 4), y = r(10, CY + 10);
                    halo(x, y, 3, 50, .8, .5); dot(x, y, .6, P.glow(50, 0, .92, 1));
                }
                const double ly = CY + 16;
                g.fillStyle = MsCanvas::linear(0, ly - 6, 0, ly + 8, { { 0, P.shade(.34) }, { 1, P.shade(.14) } });
                g.beginPath(); g.roundRect(-10, ly - 7, VW - 4, 14, 7); g.fill();
                for (int i = 0; i < 10; ++i) {
                    SubR r = sub(QStringLiteral("bk%1").arg(i));
                    const double x = r(0, VW - 20), y1 = ly - 4 + r(0, 8), x2 = x + r(6, 14), y2 = ly - 4 + r(0, 8);
                    line(x, y1, x2, y2, P.glow(30, 0, .16, .8), .5);
                }
                g.fillStyle = P.shade(.46); g.beginPath(); g.ellipse(VW - 14, ly, 4, 7, 0, 0, 7); g.fill();
                g.strokeStyle = P.shade(.28); g.lineWidth = .4;
                for (int kk = 1; kk < 4; ++kk) { g.beginPath(); g.ellipse(VW - 14, ly, kk, kk * 1.7, 0, 0, 7); g.stroke(); }
                g.fillStyle = P.shade(.04); g.fillRect(0, ly + 7, VW, VH);
                const int ngr = jsRound(5 * wide);
                for (int i = 0; i < ngr; ++i) {
                    const double x = R(0, VW), y = VH - 2;
                    g.strokeStyle = P.shade(.2); g.lineWidth = .8;
                    for (int kk = -3; kk <= 3; ++kk) { g.beginPath(); g.moveTo(x, y); g.quadraticCurveTo(x + kk * 3, y - 10, x + kk * 5, y - 12 + std::abs(kk)); g.stroke(); }
                }
                break;
            }
            case 8: {  // sniper: parapeito de um telhado, a cidade lá embaixo
                const double cmx = CX + R(-30, -14), cmy = R(14, 22);
                crescent(cmx, cmy, 8);
                const int farN = jsRound(10 * wide);
                for (int i = 0; i < farN; ++i) {
                    const double x = i * VW / farN, w = VW / farN * .9, h = R(14, 30);
                    g.fillStyle = P.shade(.14); g.fillRect(x, CY + 14 - h, w, h);
                    const int lit = jsRound((2 + grow * 6) * (.5 + rnd()));
                    for (int kk = 0; kk < lit; ++kk) {
                        g.fillStyle = P.glow(40, 0, .76, 1);
                        const double wx = x + R(1, w - 2), wy = CY + 14 - h + R(2, h - 3);
                        g.fillRect(wx, wy, 1.2, 1.4);
                    }
                }
                const int nearN = jsRound(4 * wide);
                for (int i = 0; i < nearN; ++i) {
                    const double x = R(0, VW), w = R(10, 16), h = R(36, 58);
                    g.fillStyle = P.shade(.08); g.fillRect(x, CY + 20 - h, w, h);
                    for (int kk = 0; kk < 5; ++kk)
                        if (rnd() < .5) {
                            g.fillStyle = P.glow(40, 0, .76, 1);
                            const double wx = x + R(2, w - 3), wy = CY + 20 - h + R(3, h - 4);
                            g.fillRect(wx, wy, 1.4, 1.6);
                        }
                }
                const double tside = rnd() < .5 ? -1 : 1;
                const double tx = CX + tside * R(26, 36);
                g.fillStyle = P.shade(.04); g.fillRect(tx - 6, CY - 18, 12, 12);
                g.beginPath(); g.moveTo(tx - 7, CY - 18); g.lineTo(tx, CY - 24); g.lineTo(tx + 7, CY - 18); g.fill();
                line(tx - 5, CY - 6, tx - 6, CY + 14, P.shade(.04), 1); line(tx + 5, CY - 6, tx + 6, CY + 14, P.shade(.04), 1);
                g.fillStyle = P.shade(.24); g.fillRect(0, CY + 18, VW, VH); g.fillStyle = P.shade(.34); g.fillRect(0, CY + 16, VW, 4);
                for (double y = CY + 24; y < VH; y += 5) {
                    const int off = jsRound((y - CY) / 5) % 2 ? 6 : 0;
                    for (double x = -6 + off; x < VW; x += 12) { g.strokeStyle = P.shade(.18); g.lineWidth = .5; g.strokeRect(x, y, 12, 5); }
                }
                break;
            }
            default: {  // bazuca: o tanque em chamas ao fundo
                const double tx = CX + 20, ty = CY + 4;
                halo(tx, ty - 8, 56, 30, .62, .55);
                for (int i = 0; i < 14; ++i) {
                    SubR r = sub(QStringLiteral("fu%1").arg(i));
                    dot(tx + r(-8, 8) + i * 1.6, ty - 14 - i * 4.2, 4 + i * .8, P.shade(.06, .75));
                }
                g.fillStyle = P.shade(.04); g.beginPath(); g.roundRect(tx - 24, ty + 2, 48, 9, 4.5); g.fill();
                poly({ { tx - 20, ty + 2 }, { tx - 16, ty - 6 }, { tx + 18, ty - 6 }, { tx + 22, ty + 2 } }, P.shade(.04));
                g.beginPath(); g.roundRect(tx - 10, ty - 13, 20, 8, 3); g.fill();
                g.save(); g.translate(tx - 10, ty - 10); g.rotate(.32); g.fillRect(-24, -1.2, 24, 2.4); g.restore();
                for (int kk = 0; kk < 6; ++kk) dot(tx - 19 + kk * 7.6, ty + 6.5, 2.6, P.shade(.12));
                const int fl = 3 + jsRound(grow * 4);
                for (int i = 0; i < fl; ++i) {
                    SubR r = sub(QStringLiteral("fo%1").arg(i));
                    const double fx = tx + r(-16, 16), fy = ty - r(4, 9), fh = r(8, 16) * (.7 + grow * .4), fw = r(2.4, 4);
                    flame(fx, fy, fh, fw);
                }
                g.fillStyle = P.shade(.1); g.beginPath(); g.moveTo(0, VH);
                for (double x = 0; x <= VW; x += 4) g.lineTo(x, VH - 14 + std::sin(x * .13) * 3 + (x < CX ? -4 : 0));
                g.lineTo(VW, VH); g.fill();
                for (int i = 0; i < 10; ++i) {
                    SubR r = sub(QStringLiteral("rb%1").arg(i));
                    const double x = r(0, VW), y = r(VH - 16, VH - 2);
                    const double ax = r(2, 6), ay = r(2, 5), bx = r(5, 9), shv = r(.14, .24);
                    poly({ { x, y }, { x + ax, y - ay }, { x + bx, y } }, P.shade(shv));
                }
                break;
            }
            }
        };
        // onde e como a arma fica: deslocamento, ângulo, zoom e se está apoiada (sombra) ou no escuro (contorno de luz)
        static const double GP[10][5] = { { 0, 8, -.12, 1.15, 0 }, { -4, 10, .1, 1.15, 0 }, { 0, 6, -.3, 1.1, 0 }, { -6, 14, -.06, 1.12, 0 }, { -16, 20, -.25, 1.05, 1 },
                                          { -2, 6, -1.0, 1.0, 1 }, { 4, 8, -1.12, 1.0, 1 }, { -4, 6, -.06, 1.12, 0 }, { -4, 10, 0, 1.12, 1 }, { -10, 22, -.26, 1.1, 1 } };
        static const double MZ[10][2] = { { 23, -9.7 }, { 33, -10.5 }, { 31, -7.3 }, { 34, -8.5 }, { 43, -9.7 }, { 55, -9 }, { 55, -9.6 }, { 54, -11.1 }, { 65, -9 }, { -56, -1 } };
        auto gunFx = [&](int k) {
            if (k == 4) { const QPointF pp = g.getTransform().map(QPointF(MZ[4][0], MZ[4][1])); LZ = QPointF(pp.x() / s, pp.y() / s); hasLZ = true; }
            if (k == 8) dot(18.4, -19.7, 1.2 / K, P.glow(0, 0, .97, 1));
        };
        // efeitos que andam junto da arma (já no espaço dela)
        static const double TIPS[8][2] = { { 27, 0 }, { 51.5, -7.4 }, { 53, 0 }, { 38, -2 }, { 52.5, -3.4 }, { 66, 0 }, { 58, 0 }, { -28, -22 } };
        auto bladeFx = [&](int k) {
            const double x = TIPS[k][0], y = TIPS[k][1], sz = 9.6 / K;
            const QColor c = P.glow(0, 0, .98, 1);
            g.save(); g.shadowColor = c; g.shadowBlur = 6 * s;
            poly({ { x - sz, y }, { x, y - sz * .16 }, { x + sz, y }, { x, y + sz * .16 } }, c);
            poly({ { x, y - sz }, { x + sz * .16, y }, { x, y + sz }, { x - sz * .16, y } }, c);
            g.restore();
        };
        if (blades) {
            BD(t); g.save(); g.translate(CX - 3, CY + 4); g.rotate(ang); BL(t); bladeFx(t); g.restore();
        } else {
            const double dx = GP[t][0], dy = GP[t][1], ga = GP[t][2], gz = GP[t][3];
            const bool rim = GP[t][4] != 0;
            Z = gz;
            GB(t);
            g.save(); g.translate(CX + dx, CY + dy); g.rotate(ga + (ang + .14) * .5);
            if (rim) { g.shadowColor = P.glow(0, 0, .9, .9); g.shadowBlur = 4.5 * s; }
            else { g.shadowColor = P.shade(0, .65); g.shadowBlur = 2.5 * s; g.shadowOffsetX = 1.8 * s; g.shadowOffsetY = 2.2 * s; }
            GN(t);
            g.shadowColor = QColor(0, 0, 0, 0); g.shadowBlur = 0; g.shadowOffsetX = 0; g.shadowOffsetY = 0;
            gunFx(t);
            g.restore();
            if (hasLZ) {
                g.save(); g.shadowColor = P.glow(-30, 0, .6, 1); g.shadowBlur = 3 * s;
                line(LZ.x(), LZ.y(), door.x() - 2, door.y() + 4, P.glow(-30, 0, .62, .85), .6); g.restore();
                dot(door.x() - 2, door.y() + 4, 1.3, P.glow(-30, 0, .7, 1));
            }
        }
        break;
    }
    default: break;
    }
}

}  // namespace

namespace MsVignetteLeva3 {

bool isPorted(int family) {
    // leva 3, parte 1 (2026-10-03): Objetos, menos a Guitarra (que vem sozinha depois); parte 2: Cenas;
    // parte 3 (2026-10-08): Feitos à mão
    return (family >= MsVignette::Candle && family <= MsVignette::Lantern)
        || (family >= MsVignette::Road && family <= MsVignette::Train)
        || (family >= MsVignette::Azulejo && family <= MsVignette::Chess)
        || family == MsVignette::Blades || family == MsVignette::Firearms    // parte 4 (2026-10-08): Arsenal
        || family == MsVignette::Guitar;                                       // parte 5 (2026-10-08): Guitarra
}

bool render(QPainter& p, QSize size, const QString& seedId, int words, const QColor& base, int family, bool rect) {
    if (!isPorted(family)) return false;
    const double W = size.width(), H = size.height();
    const double s = std::min(W, H) / 104, VW = W / s, VH = H / s;
    Rng rnd{ hashStr(seedId) };
    const double grow = std::clamp(std::log10(words + 1.0) / std::log10(15000.0), 0.12, 1.0);
    const Pal P = paletteFor(base);
    MsCanvas g(p);
    g.save();
    g.beginPath();
    if (rect) g.roundRect(0, 0, W, H, 5 * (W / 96)); else g.arc(W / 2, H / 2, std::min(W, H) / 2, 0, 7);
    g.clip();
    g.fillStyle = MsCanvas::radial(W * .4, H * .3, 4, W / 2, H / 2, std::max(W, H) * .7, { { 0, P.bg0 }, { 1, P.bg1 } });
    g.fillRect(0, 0, W, H);
    g.scale(s, s);
    g.lineCap = Qt::RoundCap; g.lineJoin = Qt::RoundJoin; g.strokeStyle = P.ink; g.fillStyle = P.ink;
    drawFamily(g, family, seedId, rnd, grow, VW, VH, s, P, rect, words);
    g.restore();
    if (!rect) {
        g.strokeStyle = P.ring; g.lineWidth = 1.5 * s;
        g.beginPath(); g.arc(W / 2, H / 2, std::min(W, H) / 2 - s, 0, 7); g.stroke();
    }
    return true;
}

}
