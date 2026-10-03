#include "MsVignetteLeva3.h"

#include "MsCanvas.h"
#include "MsVignette.h"
#include "MsVignetteCore.h"

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

void drawFamily(MsCanvas& g, int fam, const QString& id, Rng& rnd, double grow, double VW, double VH, double s, const Pal& P) {
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
    default: break;
    }
}

}  // namespace

namespace MsVignetteLeva3 {

bool isPorted(int family) {
    // leva 3, parte 1 (2026-10-03): Objetos, menos a Guitarra (que vem sozinha depois); parte 2: Cenas
    return (family >= MsVignette::Candle && family <= MsVignette::Lantern)
        || (family >= MsVignette::Road && family <= MsVignette::Train);
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
    drawFamily(g, family, seedId, rnd, grow, VW, VH, s, P);
    g.restore();
    if (!rect) {
        g.strokeStyle = P.ring; g.lineWidth = 1.5 * s;
        g.beginPath(); g.arc(W / 2, H / 2, std::min(W, H) / 2 - s, 0, 7); g.stroke();
    }
    return true;
}

}
