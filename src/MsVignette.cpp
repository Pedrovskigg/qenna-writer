#include "MsVignette.h"

#include <QCoreApplication>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QTransform>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>

#include "MsVignetteCore.h"
#include "MsVignetteLeva3.h"

namespace {

using namespace MsVignetteDetail;

const char* const kIds[MsVignette::FamilyCount] = {
    "branches", "roots", "flames", "city", "stars", "mountains", "coral",
    "cracks", "mandala", "lightning", "waves", "circles", "rays",
    "forest", "garden", "flock", "rain", "river", "galaxy", "crystals", "aurora", "dunes", "web",
    "ruins", "lighthouse", "castle", "cyberpunk", "medieval", "steampunk", "feudal-japan", "orient",
    "blizzard",
    "candle", "hourglass", "key", "compass", "pocket-watch", "inkwell", "cup", "books", "lantern", "guitar",
    "road", "noir", "mansion", "planet", "fleet", "sailboat", "wings", "ferris-wheel", "hill", "train",
    "azulejo", "maze", "cordel", "origami", "treasure-map", "chess", "blades", "firearms" };

// Elipse girada do canvas (ellipse(x, y, rx, ry, rot)).
QPainterPath ellipsePath(double x, double y, double rx, double ry, double rot) {
    QPainterPath e;
    e.addEllipse(QPointF(0, 0), rx, ry);
    QTransform t;
    t.translate(x, y);
    t.rotateRadians(rot);
    return t.map(e);
}

// arc(cx, cy, r, a0, a1) do canvas (sentido horário, ângulos em radianos),
// emendado no ponto atual como no canvas.
void canvasArc(QPainterPath& path, double cx, double cy, double r, double a0, double a1) {
    double sweep = a1 - a0;
    if (sweep < 0) sweep += 2 * M_PI;
    const QRectF box(cx - r, cy - r, 2 * r, 2 * r);
    if (path.elementCount() == 0) path.arcMoveTo(box, -qRadiansToDegrees(a0));
    path.arcTo(box, -qRadiansToDegrees(a0), -qRadiansToDegrees(sweep));
}

void drawFamily(QPainter& g, int fam, Rng& rnd, double grow, double VW, double VH, const Pal& P) {
    const double CX = VW / 2, CY = VH / 2, wide = VW / 104.0, PI = M_PI;
    const QColor ink = P.ink, ink2 = P.ink2, faint = P.faint;
    auto pen = [&](const QColor& c, double w) {
        QPen p(c, w);
        p.setCapStyle(Qt::RoundCap);
        p.setJoinStyle(Qt::RoundJoin);
        g.setPen(p);
        g.setBrush(Qt::NoBrush);
    };
    auto dot = [&](double x, double y, double r, const QColor& c) {
        g.setPen(Qt::NoPen);
        g.setBrush(c);
        g.drawEllipse(QPointF(x, y), r, r);
    };
    // Preenchimento com a regra "nonzero" do canvas: a padrão do Qt (par-ímpar)
    // abriria buraco onde os triângulos de um pinheiro se sobrepõem.
    auto fill = [&](QPainterPath path, const QColor& c) {
        path.setFillRule(Qt::WindingFill);
        g.setPen(Qt::NoPen);
        g.setBrush(c);
        g.drawPath(path);
    };
    auto stroke = [&](const QPainterPath& path, const QColor& c, double w) {
        pen(c, w);
        g.drawPath(path);
    };
    auto spots = [&](int n) {
        QList<double> xs;
        for (int i = 0; i < n; ++i) xs << VW * (i + .5) / n + rnd.R(-8, 8);
        return xs;
    };

    switch (fam) {
    case MsVignette::Branches: {
        const int maxD = 2 + jsRound(grow * 5);
        std::function<void(double, double, double, double, int)> br = [&](double x, double y, double len, double ang, int d) {
            const double x2 = x + std::cos(ang) * len, y2 = y + std::sin(ang) * len;
            pen(ink, std::max(.5, d * .7));
            g.drawLine(QPointF(x, y), QPointF(x2, y2));
            if (d <= 0) { const double r = rnd.R(.8, 2); dot(x2, y2, r, ink2); return; }
            const int n = rnd() < .3 ? 3 : 2;
            for (int i = 0; i < n; ++i) {
                const double l2 = len * rnd.R(.62, .82);
                const double a2 = ang + rnd.R(-.75, .75);
                br(x2, y2, l2, a2, d - 1);
            }
        };
        const QList<double> xs = spots(std::max(1, jsRound(wide)));
        for (int i = 0; i < xs.size(); ++i) {
            const double len = rnd.R(20, 28) * (i ? .8 : 1);
            const double ang = -PI / 2 + rnd.R(-.15, .15);
            br(xs.at(i), VH, len, ang, maxD - (i ? 1 : 0));
        }
        break;
    }
    case MsVignette::Roots: {
        const int maxD = 2 + jsRound(grow * 5);
        std::function<void(double, double, double, double, int)> rt = [&](double x, double y, double len, double ang, int d) {
            const double x2 = x + std::cos(ang) * len, y2 = y + std::sin(ang) * len;
            const double cx = (x + x2) / 2 + rnd.R(-6, 6);
            const double cy = (y + y2) / 2 + rnd.R(-6, 6);
            pen(ink, std::max(.4, d * .6));
            QPainterPath path(QPointF(x, y));
            path.quadTo(QPointF(cx, cy), QPointF(x2, y2));
            g.drawPath(path);
            if (d > 0) {
                const int n = 2 + (rnd() < .4 ? 1 : 0);
                for (int i = 0; i < n; ++i) {
                    const double l2 = len * rnd.R(.6, .8);
                    const double a2 = ang + rnd.R(-.9, .9);
                    rt(x2, y2, l2, a2, d - 1);
                }
            }
        };
        g.fillRect(QRectF(0, 0, VW, 14), faint);
        for (double x : spots(std::max(1, jsRound(wide)))) {
            const double len = rnd.R(16, 22);
            rt(x, 14, len, PI / 2, maxD);
        }
        break;
    }
    case MsVignette::Flames: {
        const int n = jsRound((3 + grow * 5) * wide);
        for (int k = 0; k < 3; ++k) for (int i = 0; i < n; ++i) {
            const double x = 14 + ((VW - 28) / std::max(1, n - 1)) * i + rnd.R(-5, 5);
            const double h = (25 + rnd.R(0, 50) * grow + 12) * (1 - k * .25);
            const double w = rnd.R(6, 12) * (1 - k * .2);
            const QColor fill = k == 0 ? faint : k == 1 ? P.glow(30, .7, .6, .55) : P.glow(50, .9, .8, .7);
            QPainterPath path(QPointF(x - w, VH));
            const double c1 = rnd.R(-8, 8);
            const double c2 = rnd.R(-4, 4);
            path.cubicTo(QPointF(x - w, VH - h * .5), QPointF(x + c1, VH - h * .8), QPointF(x + c2, VH - h));
            const double c3 = rnd.R(-6, 6);
            path.cubicTo(QPointF(x + c3, VH - h * .7), QPointF(x + w, VH - h * .45), QPointF(x + w, VH));
            g.setPen(Qt::NoPen);
            g.setBrush(fill);
            g.drawPath(path);
        }
        break;
    }
    case MsVignette::City: {
        { const double sx = rnd.R(20, VW - 20); const double sy = rnd.R(16, 30); const double sr = rnd.R(5, 9); dot(sx, sy, sr, ink2); }
        const int n = std::max(1, jsRound((5 + grow * 9) * wide));
        double x = -2;
        const double bw = VW / n;
        for (int i = 0; i < n + 3 && x < VW; ++i) {
            const double w = bw * rnd.R(.7, 1.3);
            const double h = 14 + rnd.R(0, 58) * grow + rnd.R(0, 12);
            const QRectF b(x, VH - h, w, h);
            g.fillRect(b, P.shade((14 + rnd() * 8) / 100.0));
            pen(faint, .6);
            g.drawRect(b);
            const QColor lit = P.glow(40, .8, .75, 1);
            for (double yy = VH - h + 4; yy < VH - 4; yy += 5)
                for (double xx = x + 2; xx < x + w - 2; xx += 4)
                    if (rnd() < .25) g.fillRect(QRectF(xx, yy, 1.6, 2), lit);
            x += w * .92;
        }
        break;
    }
    case MsVignette::Stars: {
        const int n = jsRound((6 + grow * 24) * std::sqrt(wide));
        struct P { double x, y, r; };
        QList<P> pts;
        for (int i = 0; i < n; ++i) {
            const double a = rnd() * 7, r = std::sqrt(rnd());
            const double pr = rnd.R(.4, 1.6);
            pts << P{ CX + std::cos(a) * r * (CX - 6), CY + std::sin(a) * r * (CY - 6), pr };
        }
        if (pts.isEmpty()) break;
        const int k = std::min(int(n), 4 + jsRound(grow * 7));
        QList<bool> used(pts.size(), false);
        used[0] = true;
        int cur = 0;
        QPainterPath path(QPointF(pts[0].x, pts[0].y));
        for (int s2 = 1; s2 < k; ++s2) {
            int best = -1; double bd = 1e9;
            for (int i = 0; i < pts.size(); ++i) {
                if (used[i]) continue;
                const double d = std::pow(pts[i].x - pts[cur].x, 2) + std::pow(pts[i].y - pts[cur].y, 2);
                if (d < bd) { bd = d; best = i; }
            }
            if (best < 0) break;
            used[best] = true;
            cur = best;
            path.lineTo(pts[best].x, pts[best].y);
        }
        pen(faint, .7);
        g.drawPath(path);
        for (int i = 0; i < pts.size(); ++i) dot(pts[i].x, pts[i].y, pts[i].r, used[i] ? ink2 : ink);
        break;
    }
    case MsVignette::Mountains: {
        { const double sx = rnd.R(20, VW - 20); const double sy = rnd.R(16, 34); const double sr = rnd.R(6, 11); dot(sx, sy, sr, ink2); }
        const int layers = 2 + jsRound(grow * 2);
        for (int l = 0; l < layers; ++l) {
            const double base = VH * .48 + l * (VH * .38 / layers);
            const double y0 = base + rnd.R(-10, 10);
            const double y1 = base + rnd.R(-10, 10);
            QList<QPointF> pts{ QPointF(0, y0), QPointF(VW, y1) };
            double amp = 22 * (0.5 + grow * .8);
            for (int d = 0; d < 5 + (wide > 1.3 ? 1 : 0); ++d, amp *= .55)
                for (int i = pts.size() - 1; i > 0; --i) {
                    const QPointF a = pts[i - 1], b = pts[i];
                    pts.insert(i, QPointF((a.x() + b.x()) / 2, (a.y() + b.y()) / 2 + rnd.R(-amp, amp)));
                }
            QPainterPath path(QPointF(0, VH));
            for (const QPointF& p : pts) path.lineTo(p);
            path.lineTo(VW, VH);
            g.setPen(Qt::NoPen);
            g.setBrush(P.shade((30 - l * 6) / 100.0));
            g.drawPath(path);
        }
        break;
    }
    case MsVignette::Coral:
    case MsVignette::Cracks:
    case MsVignette::Lightning: {
        // As três crescem por passeio com ramos; a espessura corrente é
        // herdada do ramo filho, como no concept.
        double curW = 1;
        QColor col = fam == MsVignette::Lightning ? ink2 : ink;
        auto strokeGlow = [&](const QPainterPath& path) {
            if (fam == MsVignette::Lightning) {
                QColor glow = ink2; glow.setAlphaF(.22);
                pen(glow, curW + 3.5);
                g.drawPath(path);
            }
            pen(col, curW);
            g.drawPath(path);
        };
        if (fam == MsVignette::Coral) {
            std::function<void(double, double, double, int, double)> walk = [&](double x, double y, double a, int life, double w) {
                curW = w;
                QPainterPath path(QPointF(x, y));
                for (int i = 0; i < life; ++i) {
                    a += rnd.R(-.35, .35);
                    x += std::cos(a) * 2.2; y += std::sin(a) * 2.2;
                    path.lineTo(x, y);
                    if (rnd() < .07 && w > .7) {
                        strokeGlow(path);
                        const double a2 = a + rnd.R(-.9, .9);
                        walk(x, y, a2, life - i, w * .75);
                        path = QPainterPath(QPointF(x, y));
                    }
                }
                strokeGlow(path);
                dot(x, y, 1.4, ink2);
            };
            const int stems = jsRound((3 + grow * 4) * wide);
            for (int i = 0; i < stems; ++i) {
                const double x = 12 + rnd() * (VW - 24);
                const double a = -PI / 2 + rnd.R(-.4, .4);
                walk(x, VH, a, 14 + jsRound(grow * 26), 2.2);
            }
        } else if (fam == MsVignette::Cracks) {
            std::function<void(double, double, double, int, double)> crack = [&](double x, double y, double a, int life, double w) {
                curW = w;
                QPainterPath path(QPointF(x, y));
                for (int i = 0; i < life; ++i) {
                    a += rnd.R(-.6, .6);
                    x += std::cos(a) * 4; y += std::sin(a) * 4;
                    path.lineTo(x, y);
                    if (rnd() < .18 && life - i > 2) {
                        strokeGlow(path);
                        const double a2 = a + rnd.R(-1.2, 1.2);
                        crack(x, y, a2, jsRound((life - i) * .6), w * .6);
                        path = QPainterPath(QPointF(x, y));
                    }
                }
                strokeGlow(path);
            };
            const int seeds = jsRound((2 + grow * 3) * wide);
            for (int i = 0; i < seeds; ++i) {
                const double x = rnd.R(14, VW - 14);
                const double y = rnd.R(14, VH - 14);
                const double a = rnd() * 7;
                crack(x, y, a, 6 + jsRound(grow * 12), 1.6);
            }
        } else {
            std::function<void(double, double, double, int, double)> bolt = [&](double x, double y, double a, int life, double w) {
                curW = w;
                QPainterPath path(QPointF(x, y));
                for (int i = 0; i < life; ++i) {
                    x += std::cos(a + rnd.R(-.8, .8)) * 7;
                    y += std::sin(a + rnd.R(-.8, .8)) * 7;
                    path.lineTo(x, y);
                    if (rnd() < .25 * grow && w > .6) {
                        strokeGlow(path);
                        const double a2 = a + rnd.R(-.9, .9);
                        bolt(x, y, a2, jsRound((life - i) * .5), w * .55);
                        path = QPainterPath(QPointF(x, y));
                    }
                }
                strokeGlow(path);
            };
            const QList<double> xs = spots(std::max(1, jsRound(wide * .8)));
            for (int i = 0; i < xs.size(); ++i) {
                const double a = PI / 2 + rnd.R(-.3, .3);
                bolt(xs.at(i), 0, a, 14, i ? 1.6 : 2.2);
            }
        }
        break;
    }
    case MsVignette::Mandala: {
        const int n = 5 + int(std::floor(rnd() * 8));
        const int layers = 2 + jsRound(grow * 4);
        const double rad = std::min(CX, CY) - 4;
        auto flower = [&](double cx, double cy, double rr, int ls) {
            g.save();
            g.translate(cx, cy);
            for (int l = ls; l >= 1; --l) {
                const double r = rr * (.2 + .8 * l / ls);
                const double w = rnd.R(.25, .6);
                const double shape = rnd();
                const QColor fill = (l % 2) ? faint : P.glow(30, .6, .6, .35);
                const QColor line = (l % 2) ? ink : ink2;
                for (int i = 0; i < n; ++i) {
                    g.save();
                    g.rotate(qRadiansToDegrees(i / double(n) * PI * 2 + l * .3));
                    QPainterPath path(QPointF(0, 0));
                    if (shape < .5) {
                        path.quadTo(QPointF(r * w, r * .5), QPointF(0, r));
                        path.quadTo(QPointF(-r * w, r * .5), QPointF(0, 0));
                    } else {
                        path.lineTo(r * w * .6, r * .6);
                        path.lineTo(0, r);
                        path.lineTo(-r * w * .6, r * .6);
                        path.closeSubpath();
                    }
                    QPen p(line, .8);
                    p.setJoinStyle(Qt::RoundJoin);
                    g.setPen(p);
                    g.setBrush(fill);
                    g.drawPath(path);
                    g.restore();
                }
            }
            dot(0, 0, std::max(1.5, rr * .07), ink2);
            g.restore();
        };
        flower(CX, CY, rad, layers);
        if (wide > 1.3) {
            flower(CX * .3, CY * 1.3, rad * .55, std::max(1, layers - 2));
            flower(VW - CX * .3, CY * .7, rad * .5, std::max(1, layers - 2));
        }
        break;
    }
    case MsVignette::Waves: {
        const int n = 4 + jsRound(grow * 7);
        for (int i = 0; i < n; ++i) {
            const double y0 = 12 + i * ((VH - 24) / n);
            const double amp = rnd.R(2, 8);
            const double f = rnd.R(.05, .14);
            const double ph = rnd() * 7;
            const double w = rnd.R(.8, 1.8);
            QPainterPath path;
            for (double x = 0; x <= VW; x += 2) {
                const QPointF pt(x, y0 + std::sin(x * f + ph) * amp + std::sin(x * f * 2.3) * amp * .3);
                if (x == 0) path.moveTo(pt); else path.lineTo(pt);
            }
            pen((i % 3) ? ink : ink2, w);
            g.drawPath(path);
        }
        break;
    }
    case MsVignette::Circles: {
        const int n = 4 + jsRound(grow * 7);
        const double cx = rnd.R(VW * .38, VW * .62);
        const double cy = rnd.R(40, 64);
        const double rmax = std::max(44.0, CX * .9);
        for (int i = 0; i < n; ++i) {
            const double w = rnd.R(.8, 2.4);
            const double a = rnd() * 7;
            const double sweep = rnd.R(1.5, 5.8);
            const double r = 6 + i * (rmax / n);
            const QRectF box(cx - r, cy - r, 2 * r, 2 * r);
            QPainterPath path;
            // canvas mede o ângulo no sentido horário; o Qt, no anti-horário
            path.arcMoveTo(box, -qRadiansToDegrees(a));
            path.arcTo(box, -qRadiansToDegrees(a), -qRadiansToDegrees(sweep));
            pen((i % 2) ? ink : ink2, w);
            g.drawPath(path);
        }
        break;
    }
    case MsVignette::Rays: {
        const int n = 6 + jsRound(grow * 14);
        const double cx = rnd.R(30, VW - 30);
        const double cy = rnd.R(30, 74);
        const double reach = std::max(1.0, wide * .9);
        for (int i = 0; i < n; ++i) {
            const double a = i / double(n) * PI * 2 + rnd.R(-.1, .1);
            const double l = rnd.R(20, 60) * (.5 + grow * .6) * reach;
            const double w = rnd.R(.6, 1.8);
            QPainterPath path(QPointF(cx, cy));
            path.quadTo(QPointF(cx + std::cos(a + .4) * l * .5, cy + std::sin(a + .4) * l * .5),
                        QPointF(cx + std::cos(a) * l, cy + std::sin(a) * l));
            pen((i % 3) ? ink : ink2, w);
            g.drawPath(path);
        }
        dot(cx, cy, 2.5, ink2);
        break;
    }
    // ===== 2026-09-29: as 19 do concept vinhetas-leva1 (casos 13–31) =====
    // Os sorteios seguem a ordem do JS, um por instrução: argumento de função
    // em C++ não tem ordem de avaliação garantida, e um R() fora do lugar
    // muda o desenho inteiro dali pra frente.
    case MsVignette::Forest: {
        { const double x = rnd.R(18, VW - 18); const double y = rnd.R(14, 26); const double r = rnd.R(4, 7); dot(x, y, r, ink2); }
        const int rows = 1 + jsRound(grow * 3);
        for (int r = 0; r < rows; ++r) {
            const double y = VH * .62 + r * (VH * .38 / std::max(1, rows)) * .9;
            const int n = jsRound((3 + grow * 6) * wide * (1 + r * .25));
            const double sc = .7 + r * .25;
            for (int i = 0; i < n; ++i) {
                const double x = VW * (i + .5) / n + rnd.R(-5, 5);
                const double h = rnd.R(18, 30) * sc * (.6 + grow * .5);
                const double w = h * rnd.R(.32, .42);
                QPainterPath tree;
                for (int t = 0; t < 3; ++t) {
                    const double ty = y - h + t * h * .28, tw = w * (.55 + t * .25);
                    tree.moveTo(x, ty);
                    tree.lineTo(x + tw, ty + h * .45);
                    tree.lineTo(x - tw, ty + h * .45);
                    tree.closeSubpath();
                }
                fill(tree, r == rows - 1 ? P.shade(.16 + r * .02) : P.shade(.2 + r * .04, .9));
                stroke(tree, faint, .5);
                g.fillRect(QRectF(x - .8, y - h * .1, 1.6, h * .2), P.shade(.12));
            }
        }
        break;
    }
    case MsVignette::Garden: {
        const int n = jsRound((3 + grow * 7) * wide);
        g.fillRect(QRectF(0, VH - 8, VW, 8), faint);
        for (int i = 0; i < n; ++i) {
            const double x = 10 + (VW - 20) * (i + .5) / n + rnd.R(-4, 4);
            const double h = rnd.R(22, 52) * (.5 + grow * .6);
            const double tx = x + rnd.R(-8, 8), ty = VH - 8 - h;
            QPainterPath stem(QPointF(x, VH - 8));
            const double cx = x + rnd.R(-10, 10);
            stem.quadTo(QPointF(cx, VH - 8 - h * .5), QPointF(tx, ty));
            stroke(stem, ink, .9);
            { const double lx = x + rnd.R(-5, 5); const double rot = rnd.R(-1, 1); fill(ellipsePath(lx, VH - 8 - h * .4, 3.4, 1.4, rot), faint); }
            const int petals = 5 + int(std::floor(rnd() * 3));
            const double pr = rnd.R(2.4, 4.6) * (.6 + grow * .6);
            for (int k = 0; k < petals; ++k) {
                const double a = k / double(petals) * PI * 2 + rnd();
                fill(ellipsePath(tx + std::cos(a) * pr, ty + std::sin(a) * pr, pr * .9, pr * .5, a),
                     k % 2 ? ink2 : P.glow(20, .8, .72, .95));
            }
            dot(tx, ty, pr * .45, P.glow(50, .9, .85, 1));
        }
        break;
    }
    case MsVignette::Flock: {
        { const double x = rnd.R(VW * .55, VW - 16); const double y = rnd.R(18, 34); const double r = rnd.R(8, 13); dot(x, y, r, P.glow(40, .6, .82, .9)); }
        const int n = jsRound((3 + grow * 14) * std::sqrt(wide));
        const double lx = rnd.R(18, VW * .4);
        const double ly = rnd.R(40, 70);
        for (int i = 0; i < n; ++i) {
            const int side = i % 2 ? 1 : -1, k = (i + 1) / 2;
            double x = lx + k * rnd.R(7, 10);
            x += rnd.R(-2, 2);
            double y = ly + side * k * rnd.R(4, 6);
            y += rnd.R(-2, 2);
            const double w = rnd.R(2.6, 4.6) * (1 - k * .03);
            QPainterPath bird(QPointF(x - w, y - w * .4));
            bird.quadTo(QPointF(x - w * .4, y - w * .9), QPointF(x, y));
            bird.quadTo(QPointF(x + w * .4, y - w * .9), QPointF(x + w, y - w * .4));
            stroke(bird, i == 0 ? ink2 : ink, .9);
        }
        break;
    }
    case MsVignette::Rain: {
        const int n = jsRound((18 + grow * 70) * wide);
        pen(faint, .7);
        for (int i = 0; i < n; ++i) {
            const double x = rnd.R(-10, VW);
            const double y = rnd.R(-6, VH - 18);
            const double l = rnd.R(4, 9);
            g.drawLine(QPointF(x, y), QPointF(x + l * .35, y + l));
        }
        const int rip = jsRound((2 + grow * 6) * wide);
        for (int i = 0; i < rip; ++i) {
            const double x = rnd.R(10, VW - 10);
            const double y = rnd.R(VH - 16, VH - 4);
            const double r = rnd.R(2, 6);
            stroke(ellipsePath(x, y, r, r * .3, 0), i % 2 ? ink : ink2, .7);
            if (grow > .4) stroke(ellipsePath(x, y, r * 1.8, r * .55, 0), faint, .7);
        }
        break;
    }
    case MsVignette::River: {
        // Como no canvas: depois de um afluente, o resto do leito sai com o
        // traço do último afluente desenhado.
        QColor curC = ink2;
        double curW = 1;
        std::function<void(double, double, double, int, double, int)> river =
            [&](double x, double y, double ang, int len, double w, int d) {
            curC = d ? ink : ink2;
            curW = w;
            QPainterPath path(QPointF(x, y));
            for (int i = 0; i < len; ++i) {
                ang += rnd.R(-.35, .35);
                ang = ang * .9 + PI / 2 * .1;
                x += std::cos(ang) * 3.2;
                y += std::sin(ang) * 3.2;
                path.lineTo(x, y);
                if (d < 2 && rnd() < .05 * grow * 3) {
                    stroke(path, curC, curW);
                    const double sign = rnd() < .5 ? -1 : 1;
                    const double turn = rnd.R(.6, 1.1);
                    river(x, y, ang + sign * turn, jsRound((len - i) * .45), w * .55, d + 1);
                    path = QPainterPath(QPointF(x, y));
                }
            }
            stroke(path, curC, curW);
        };
        for (int i = 0; i < jsRound(8 * wide); ++i) {
            const double x = rnd.R(0, VW);
            const double y = rnd.R(0, VH);
            const double rx = rnd.R(4, 10);
            const double ry = rnd.R(1.5, 3);
            fill(ellipsePath(x, y, rx, ry, 0), faint);
        }
        for (double x : spots(std::max(1, jsRound(wide * .7))))
            river(x, -4, PI / 2, 20 + jsRound(grow * 20), 2.6 + grow * 1.6, 0);
        break;
    }
    case MsVignette::Galaxy: {
        const int arms = 2 + jsRound(grow * 2), pts = jsRound(30 + grow * 140);
        const double cx = CX + rnd.R(-8, 8);
        const double cy = CY + rnd.R(-6, 6);
        const double rot = rnd() * 7;
        for (int i = 0; i < pts; ++i) {
            const int arm = i % arms;
            const double t = std::pow(rnd(), .7);
            const double a = rot + arm / double(arms) * PI * 2 + t * 4.2 + rnd.R(-.25, .25);
            const double r = t * (std::min(CX, CY) - 6) * (1 + (wide - 1) * .5);
            const double x = cx + std::cos(a) * r * (1 + (wide - 1) * .6), y = cy + std::sin(a) * r * .62;
            const double pr = rnd.R(.3, 1.1) * (1.2 - t * .5);
            dot(x, y, pr, rnd() < .2 ? ink2 : ink);
        }
        fill(ellipsePath(cx, cy, 6, 3.8, 0), P.glow(40, .7, .85, .6));
        dot(cx, cy, 2, P.glow(50, .9, .92, 1));
        break;
    }
    case MsVignette::Crystals: {
        const int n = jsRound((3 + grow * 7) * wide);
        for (int i = 0; i < n; ++i) {
            const double x = 12 + (VW - 24) * (i + .5) / n + rnd.R(-6, 6);
            const double h = rnd.R(14, 34) * (.5 + grow * .8) * (i % 3 ? 1 : 1.3);
            const double w = rnd.R(4, 8);
            const double lean = rnd.R(-.35, .35);
            const double tipx = x + std::sin(lean) * h, tipy = VH - std::cos(lean) * h;
            QPainterPath c(QPointF(x - w, VH));
            c.lineTo(tipx - w * .6, tipy + w);
            c.lineTo(tipx, tipy - 2);
            c.lineTo(tipx + w * .6, tipy + w);
            c.lineTo(x + w, VH);
            c.closeSubpath();
            fill(c, P.shade(.28 + rnd() * .14, .95));
            stroke(c, ink, .8);
            pen(ink2, .6);
            g.drawLine(QPointF(x, VH), QPointF(tipx, tipy - 2));
        }
        break;
    }
    case MsVignette::Aurora: {
        const int stars = jsRound((10 + grow * 30) * wide);
        for (int i = 0; i < stars; ++i) {
            const double x = rnd.R(0, VW);
            const double y = rnd.R(0, VH * .7);
            const double r = rnd.R(.3, .9);
            dot(x, y, r, faint);
        }
        const int bands = 2 + jsRound(grow * 3);
        for (int b = 0; b < bands; ++b) {
            const double y0 = rnd.R(22, 52);
            const double amp = rnd.R(6, 14);
            const double f = rnd.R(.03, .07);
            const double ph = rnd() * 7;
            const double hgt = rnd.R(18, 36) * (.6 + grow * .5);
            const QColor top = b % 2 ? P.glow(60, .8, .72, .55) : P.glow(-40, .8, .68, .55);
            const QColor bottom = P.glow(0, .8, .6, 0);
            for (double x = 0; x <= VW; x += 1.6) {
                const double y = y0 + std::sin(x * f + ph) * amp;
                QLinearGradient gr(QPointF(0, y), QPointF(0, y + hgt));
                gr.setColorAt(0, top);
                gr.setColorAt(1, bottom);
                g.fillRect(QRectF(x, y, 1.7, hgt), gr);
            }
        }
        QPainterPath ground(QPointF(0, VH));
        for (double x = 0; x <= VW; x += 6) ground.lineTo(x, VH - 8 - std::abs(std::sin(x * .07 + rnd())) * 6);
        ground.lineTo(VW, VH);
        fill(ground, P.shade(.1));
        break;
    }
    case MsVignette::Dunes: {
        { const double x = rnd.R(VW * .2, VW * .8); const double y = rnd.R(28, 44); const double r = rnd.R(8, 14); dot(x, y, r, P.glow(30, .8, .78, .9)); }
        const int layers = 2 + jsRound(grow * 3);
        for (int l = 0; l < layers; ++l) {
            const double base = VH * .5 + l * (VH * .45 / layers);
            const double amp = rnd.R(5, 12);
            const double f = rnd.R(.03, .06);
            const double ph = rnd() * 7;
            auto yAt = [&](double x) { return base + std::sin(x * f + ph) * amp + std::sin(x * f * 2.1 + ph) * amp * .3; };
            QPainterPath dune(QPointF(0, VH));
            for (double x = 0; x <= VW; x += 2) dune.lineTo(x, yAt(x));
            dune.lineTo(VW, VH);
            fill(dune, P.shade(.34 - l * .05));
            QPainterPath crest(QPointF(0, yAt(0) + 1.2));
            for (double x = 2; x <= VW; x += 2) crest.lineTo(x, yAt(x) + 1.2);
            stroke(crest, faint, .5);
        }
        break;
    }
    case MsVignette::Web: {
        const double cx = rnd.R(VW * .35, VW * .65);
        const double cy = rnd.R(38, 62);
        const int spokes = 8 + int(std::floor(rnd() * 5));
        const double reach = std::max(60.0, VW * .7);
        QList<double> ang;
        for (int i = 0; i < spokes; ++i) ang << i / double(spokes) * PI * 2 + rnd.R(-.12, .12);
        pen(faint, .7);
        for (double a : ang) g.drawLine(QPointF(cx, cy), QPointF(cx + std::cos(a) * reach, cy + std::sin(a) * reach));
        const int turns = 3 + jsRound(grow * 10);
        for (int t = 1; t <= turns; ++t) {
            const double r = t * (std::min(CX, CY) * 1.1 / turns + 1.5);
            QPainterPath ring;
            for (int i = 0; i < ang.size(); ++i) {
                const double rr = r * rnd.R(.92, 1.06);
                const QPointF pt(cx + std::cos(ang[i]) * rr, cy + std::sin(ang[i]) * rr);
                if (i) ring.lineTo(pt); else ring.moveTo(pt);
            }
            ring.closeSubpath();
            stroke(ring, ink, .8);
        }
        const int dew = jsRound(3 + grow * 8);
        for (int i = 0; i < dew; ++i) {
            const double a = ang[int(std::floor(rnd() * spokes))];
            const double r = rnd.R(8, std::min(CX, CY));
            const double pr = rnd.R(.6, 1.3);
            dot(cx + std::cos(a) * r, cy + std::sin(a) * r, pr, ink2);
        }
        break;
    }
    case MsVignette::Ruins: {
        g.fillRect(QRectF(0, VH - 10, VW, 10), faint);
        const int n = jsRound((3 + grow * 5) * wide);
        for (int i = 0; i < n; ++i) {
            const double x = 10 + (VW - 20) * (i + .5) / n;
            const double full = rnd.R(30, 58);
            const bool broken = rnd() < .45;
            const double h = broken ? full * rnd.R(.3, .75) : full;
            const double w = rnd.R(4.5, 6.5);
            g.fillRect(QRectF(x - w / 2, VH - 10 - h, w, h), P.shade(.3 + rnd() * .08));
            pen(faint, .5);
            for (int k = 1; k < 3; ++k)
                g.drawLine(QPointF(x - w / 2 + k * w / 3, VH - 10), QPointF(x - w / 2 + k * w / 3, VH - 10 - h));
            if (!broken) {
                g.fillRect(QRectF(x - w / 2 - 1.5, VH - 10 - h - 2, w + 3, 2), ink);
            } else {
                QPainterPath top(QPointF(x - w / 2, VH - 10 - h));
                top.lineTo(x, VH - 10 - h - rnd.R(2, 5));
                top.lineTo(x + w / 2, VH - 10 - h);
                fill(top, P.shade(.3));
            }
        }
        if (grow > .35) {
            const double ax = rnd.R(VW * .3, VW * .7);
            const double aw = rnd.R(16, 22);
            const double ah = rnd.R(34, 46);
            QPainterPath arch(QPointF(ax - aw, VH - 10));
            arch.lineTo(ax - aw, VH - 10 - ah * .6);
            canvasArc(arch, ax, VH - 10 - ah * .6, aw, PI, 0);
            arch.lineTo(ax + aw, VH - 10);
            stroke(arch, ink2, 2);
        }
        for (int i = 0; i < jsRound(grow * 6); ++i) {
            const double x = rnd.R(4, VW - 4);
            const double y = VH - rnd.R(3, 9);
            const double r = rnd.R(.8, 1.8);
            dot(x, y, r, P.shade(.4));
        }
        break;
    }
    case MsVignette::Lighthouse: {
        const int stars = jsRound((6 + grow * 18) * wide);
        for (int i = 0; i < stars; ++i) {
            const double x = rnd.R(0, VW);
            const double y = rnd.R(0, VH * .55);
            const double r = rnd.R(.3, .9);
            dot(x, y, r, faint);
        }
        const double tx = rnd.R(VW * .22, VW * .4);
        const double ty = VH - 26;
        const double th = rnd.R(34, 42);
        const double bw = 6, beam = (40 + grow * 70) * std::max(1.0, wide);
        const double a0 = rnd.R(-.35, -.05);
        QLinearGradient gr(QPointF(tx, ty - th), QPointF(tx + std::cos(a0) * beam, ty - th + std::sin(a0) * beam));
        gr.setColorAt(0, P.glow(45, .9, .85, .7));
        gr.setColorAt(1, P.glow(45, .9, .85, 0));
        QPainterPath light(QPointF(tx, ty - th));
        light.lineTo(tx + std::cos(a0 - .12) * beam, ty - th + std::sin(a0 - .12) * beam);
        light.lineTo(tx + std::cos(a0 + .12) * beam, ty - th + std::sin(a0 + .12) * beam);
        light.closeSubpath();
        g.setPen(Qt::NoPen);
        g.setBrush(gr);
        g.drawPath(light);
        QPainterPath rock(QPointF(tx - 20, VH));
        rock.quadTo(QPointF(tx - 12, ty - 4), QPointF(tx, ty));
        rock.quadTo(QPointF(tx + 14, ty - 2), QPointF(tx + 22, VH));
        fill(rock, P.shade(.14));
        QPainterPath tower(QPointF(tx - bw, ty));
        tower.lineTo(tx - bw * .6, ty - th);
        tower.lineTo(tx + bw * .6, ty - th);
        tower.lineTo(tx + bw, ty);
        fill(tower, ink);
        for (int k = 1; k < 4; ++k) g.fillRect(QRectF(tx - bw, ty - th * k / 4, bw * 2, 1.6), P.shade(.2));
        dot(tx, ty - th - 2, 2.4, P.glow(45, .9, .9, 1));
        const int waves = 2 + jsRound(grow * 3);
        for (int w = 0; w < waves; ++w) {
            QPainterPath wave(QPointF(0, VH - 4 - w * 4 + std::sin(w) * 1.3));
            for (double x = 2; x <= VW; x += 2) wave.lineTo(x, VH - 4 - w * 4 + std::sin(x * .18 + w) * 1.3);
            stroke(wave, w % 2 ? ink : faint, .8);
        }
        break;
    }
    case MsVignette::Castle: {
        const double hillY = VH - 14;
        QPainterPath hill(QPointF(0, VH));
        hill.quadTo(QPointF(VW * .5, hillY - 10), QPointF(VW, VH));
        fill(hill, P.shade(.14));
        const double cx = CX + rnd.R(-6, 6);
        const double baseY = hillY - 2, scale = .7 + grow * .6;
        const double wallW = std::min(VW - 16, (34 + grow * 40) * scale), wallH = 12 * scale;
        const QColor stone = P.shade(.3), stone2 = P.shade(.24);
        auto merlons = [&](double x, double y, double w, double h) {
            for (double m = x; m < x + w - 1; m += 4) g.fillRect(QRectF(m, y - h * .35, 2.4, h * .35), stone);
        };
        g.fillRect(QRectF(cx - wallW / 2, baseY - wallH, wallW, wallH), stone2);
        merlons(cx - wallW / 2, baseY - wallH, wallW, 6 * scale);
        QPainterPath gate(QPointF(cx - 4 * scale, baseY));
        gate.lineTo(cx - 4 * scale, baseY - 6 * scale);
        canvasArc(gate, cx, baseY - 6 * scale, 4 * scale, PI, 0);
        gate.lineTo(cx + 4 * scale, baseY);
        fill(gate, P.shade(.1));
        const int towers = 2 + jsRound(grow * 3);
        const double tw = 7 * scale;
        for (int t = 0; t < towers; ++t) {
            const double x = cx - wallW / 2 + wallW * (t / double(towers - 1));
            const double h = wallH + rnd.R(8, 16) * scale * (t % 2 ? 1 : 1.25);
            g.fillRect(QRectF(x - tw / 2, baseY - h, tw, h), stone);
            merlons(x - tw / 2 - 1, baseY - h, tw + 2, 5 * scale);
            if (grow > .45) {
                QPainterPath roof(QPointF(x - tw / 2 - 1, baseY - h - 1));
                roof.lineTo(x, baseY - h - 8 * scale);
                roof.lineTo(x + tw / 2 + 1, baseY - h - 1);
                fill(roof, P.shade(.2));
            }
            if (rnd() < .6) g.fillRect(QRectF(x - .8, baseY - h * .6, 1.6, 2.6), P.glow(40, .9, .75, 1));
        }
        const double kh = (22 + grow * 30) * scale, kw = 11 * scale;
        const double top = baseY - wallH - kh;
        g.fillRect(QRectF(cx - kw / 2, top, kw, kh), stone);
        merlons(cx - kw / 2 - 1, top, kw + 2, 6 * scale);
        for (int w = 0; w < jsRound(1 + grow * 3); ++w)
            g.fillRect(QRectF(cx - 1, top + 6 + w * 6, 2, 3), P.glow(40, .9, .75, 1));
        pen(ink, .6);
        g.drawLine(QPointF(cx, top - 1), QPointF(cx, top - 9 * scale));
        QPainterPath flag(QPointF(cx, top - 9 * scale));
        flag.lineTo(cx + 6 * scale, top - 7 * scale);
        flag.lineTo(cx, top - 5 * scale);
        fill(flag, ink2);
        break;
    }
    case MsVignette::Cyberpunk: {
        const QColor neonA = P.glow(160, 1, .62, 1), neonB = P.glow(-70, 1, .65, 1);
        // O shadowBlur do canvas vira um halo em duas camadas.
        auto neonRect = [&](const QRectF& r, const QColor& c) {
            QColor h = c;
            h.setAlphaF(.14);
            g.fillRect(r.adjusted(-2.2, -2.2, 2.2, 2.2), h);
            h.setAlphaF(.3);
            g.fillRect(r.adjusted(-1, -1, 1, 1), h);
            g.fillRect(r, c);
        };
        const int n = jsRound((5 + grow * 10) * wide);
        double x = -2;
        const double bw = VW / n;
        for (int i = 0; i < n + 2 && x < VW; ++i) {
            const double w = bw * rnd.R(.6, 1.1);
            double h = 24 + rnd.R(0, 62) * grow;
            h += rnd.R(0, 10);
            g.fillRect(QRectF(x, VH - h, w, h), P.shade(.08 + rnd() * .07));
            const QColor strip = rnd() < .5 ? neonA : neonB;
            if (rnd() < .7) { const double y = VH - h + rnd.R(4, h * .5); neonRect(QRectF(x, y, w, 1), strip); }
            if (rnd() < .4) { const double sh = h * rnd.R(.3, .8); neonRect(QRectF(x + w - 1.2, VH - h, 1.2, sh), strip); }
            const QColor win = P.glow(0, .6, .7, .8);
            for (double yy = VH - h + 6; yy < VH - 3; yy += 4)
                for (double xx = x + 1.5; xx < x + w - 1.5; xx += 3)
                    if (rnd() < .18) g.fillRect(QRectF(xx, yy, 1.2, 1.4), win);
            if (rnd() < .35) {
                pen(faint, .5);
                g.drawLine(QPointF(x + w / 2, VH - h), QPointF(x + w / 2, VH - h - rnd.R(4, 10)));
                dot(x + w / 2, VH - h - 10, .7, neonB);
            }
            if (grow > .4 && rnd() < .25) {
                const double sw = w * .9;
                const double sh = rnd.R(4, 7);
                const double sy = VH - h + rnd.R(6, h * .6);
                const QColor c = rnd() < .5 ? neonA : neonB;
                const QRectF box(x + (w - sw) / 2, sy, sw, sh);
                QColor halo = c;
                halo.setAlphaF(.25);
                pen(halo, 2.4);
                g.drawRect(box);
                pen(c, .7);
                g.drawRect(box);
            }
            x += w * .95;
        }
        for (int c = 0; c < jsRound(grow * 6 * wide); ++c) {
            const double cx = rnd.R(6, VW - 6);
            const double cy = rnd.R(12, VH * .55);
            g.fillRect(QRectF(cx - 2, cy, 4, 1.2), neonB);
            dot(cx + 2, cy + .6, .6, neonA);
        }
        break;
    }
    case MsVignette::Medieval: {
        g.fillRect(QRectF(0, VH - 8, VW, 8), faint);
        const int n = jsRound((4 + grow * 8) * wide);
        const double row = VW / n;
        const int church = int(std::floor(rnd() * n));
        for (int i = 0; i < n; ++i) {
            const double x = i * row + rnd.R(-2, 2);
            const double w = row * rnd.R(.85, 1.05);
            const double h = rnd.R(12, 22) * (.6 + grow * .6);
            const double ry = VH - 8 - h;
            if (i == church && grow > .3) {
                const double th = h + 26 * grow + 10;
                g.fillRect(QRectF(x + w * .25, VH - 8 - th, w * .5, th), P.shade(.26));
                QPainterPath spire(QPointF(x + w * .2, VH - 8 - th));
                spire.lineTo(x + w * .5, VH - 8 - th - 14);
                spire.lineTo(x + w * .8, VH - 8 - th);
                fill(spire, P.shade(.18));
                QPainterPath cross(QPointF(x + w * .5, VH - 8 - th - 14));
                cross.lineTo(x + w * .5, VH - 8 - th - 19);
                cross.moveTo(x + w * .5 - 2, VH - 8 - th - 17);
                cross.lineTo(x + w * .5 + 2, VH - 8 - th - 17);
                stroke(cross, ink2, .7);
                continue;
            }
            g.fillRect(QRectF(x, ry, w, h), P.shade(.34 + rnd() * .06));
            pen(P.shade(.14), .8);
            g.drawRect(QRectF(x, ry, w, h));
            QPainterPath beams(QPointF(x, ry + h * .45));
            beams.lineTo(x + w, ry + h * .45);
            beams.moveTo(x, ry);
            beams.lineTo(x + w, ry + h * .45);
            g.drawPath(beams);
            QPainterPath roof(QPointF(x - 2, ry));
            roof.lineTo(x + w / 2, ry - rnd.R(8, 14));
            roof.lineTo(x + w + 2, ry);
            fill(roof, P.shade(.16));
            if (rnd() < .6) g.fillRect(QRectF(x + w * .3, ry + h * .6, 2, 2.4), P.glow(40, .85, .72, 1));
        }
        break;
    }
    case MsVignette::Steampunk: {
        g.fillRect(QRectF(0, VH - 6, VW, 6), faint);
        const int n = jsRound((3 + grow * 5) * wide);
        const double row = VW / n;
        for (int i = 0; i < n; ++i) {
            const double x = i * row;
            const double w = row * rnd.R(.8, 1);
            const double h = rnd.R(16, 30) * (.6 + grow * .6);
            const QColor body = P.shade(.22 + rnd() * .06);
            QPainterPath shed(QPointF(x, VH - 6));
            shed.lineTo(x, VH - 6 - h);
            for (int t = 0; t < 3; ++t) {
                shed.lineTo(x + w * (t + .5) / 3, VH - 6 - h - 5);
                shed.lineTo(x + w * (t + 1) / 3, VH - 6 - h);
            }
            shed.lineTo(x + w, VH - 6);
            fill(shed, body);
            if (rnd() < .7) {
                const double cx = x + w * rnd.R(.2, .8);
                const double ch = rnd.R(10, 24) * (.6 + grow * .6);
                g.fillRect(QRectF(cx - 1.8, VH - 6 - h - ch, 3.6, ch), P.shade(.18));
                for (int p = 0; p < jsRound(2 + grow * 4); ++p) {
                    const double px = cx + p * 3 + rnd.R(-1, 1);
                    dot(px, VH - 6 - h - ch - 4 - p * 4, 2 + p * .9, faint);
                }
            }
            for (int k = 0; k < 3; ++k)
                if (rnd() < .6) g.fillRect(QRectF(x + w * (k + .3) / 3, VH - 6 - h * .5, 2.2, 3), P.glow(35, .9, .72, 1));
        }
        auto gear = [&](double gx, double gy, double r, int teeth) {
            QPainterPath cog;
            for (int t = 0; t < teeth * 2; ++t) {
                const double a = t / double(teeth * 2) * PI * 2, rr = t % 2 ? r : r + 2;
                const QPointF pt(gx + std::cos(a) * rr, gy + std::sin(a) * rr);
                if (t) cog.lineTo(pt); else cog.moveTo(pt);
            }
            cog.closeSubpath();
            stroke(cog, ink, 1);
            QPainterPath hub;
            hub.addEllipse(QPointF(gx, gy), r * .35, r * .35);
            stroke(hub, ink, 1);
        };
        { const double gx = rnd.R(14, VW * .4); const double gy = rnd.R(16, 30); const double r = rnd.R(6, 9); gear(gx, gy, r, 8); }
        if (grow > .3) { const double gx = rnd.R(VW * .6, VW - 14); const double gy = rnd.R(20, 34); const double r = rnd.R(4, 6); gear(gx, gy, r, 6); }
        if (grow > .55) {
            const double zx = rnd.R(24, VW - 30);
            const double zy = rnd.R(10, 22);
            const QPainterPath zep = ellipsePath(zx, zy, 16, 5, 0);
            fill(zep, P.shade(.4));
            stroke(zep, ink2, .6);
            g.fillRect(QRectF(zx - 4, zy + 5, 8, 2.6), P.shade(.22));
        }
        break;
    }
    case MsVignette::FeudalJapan: {
        const double fx = rnd.R(VW * .55, VW * .85);
        QPainterPath fuji(QPointF(fx - 38, VH - 18));
        fuji.lineTo(fx - 8, VH - 58);
        fuji.lineTo(fx + 8, VH - 58);
        fuji.lineTo(fx + 38, VH - 18);
        fill(fuji, P.shade(.3, .8));
        QPainterPath cap(QPointF(fx - 14, VH - 48));
        cap.lineTo(fx - 8, VH - 58);
        cap.lineTo(fx + 8, VH - 58);
        cap.lineTo(fx + 14, VH - 48);
        cap.lineTo(fx + 6, VH - 50);
        cap.lineTo(fx, VH - 47);
        cap.lineTo(fx - 6, VH - 50);
        fill(cap, P.glow(0, .3, .92, .9));
        { const double x = rnd.R(14, VW * .4); const double y = rnd.R(14, 26); const double r = rnd.R(6, 9); dot(x, y, r, P.glow(-20, .8, .62, .95)); }
        g.fillRect(QRectF(0, VH - 12, VW, 12), P.shade(.14));
        const double px = rnd.R(VW * .22, VW * .42);
        const int floors = 2 + jsRound(grow * 3);
        const double fw = 16, fh = 8;
        double y = VH - 12;
        for (int f = 0; f < floors; ++f) {
            const double w = fw * (1 - f * .12);
            g.fillRect(QRectF(px - w * .35, y - fh, w * .7, fh), P.shade(.22));
            QPainterPath eave(QPointF(px - w * .75, y - fh + 1));
            eave.quadTo(QPointF(px - w * .4, y - fh - 1), QPointF(px, y - fh - 5));
            eave.quadTo(QPointF(px + w * .4, y - fh - 1), QPointF(px + w * .75, y - fh + 1));
            eave.lineTo(px, y - fh - 2);
            fill(eave, P.shade(.12));
            if (rnd() < .7) g.fillRect(QRectF(px - 1, y - fh * .6, 2, 2.4), P.glow(30, .9, .72, 1));
            y -= fh + 3;
        }
        pen(ink, .8);
        g.drawLine(QPointF(px, y + 3), QPointF(px, y - 6));
        if (grow > .3) {
            const double tx = rnd.R(VW * .6, VW - 14), ty = VH - 12;
            QPainterPath torii(QPointF(tx - 6, ty));
            torii.lineTo(tx - 6, ty - 14);
            torii.moveTo(tx + 6, ty);
            torii.lineTo(tx + 6, ty - 14);
            torii.moveTo(tx - 9, ty - 14);
            torii.quadTo(QPointF(tx, ty - 16), QPointF(tx + 9, ty - 14));
            torii.moveTo(tx - 7, ty - 10.5);
            torii.lineTo(tx + 7, ty - 10.5);
            stroke(torii, P.glow(-10, .9, .6, 1), 1.6);
        }
        const int bl = jsRound((4 + grow * 16) * wide);
        for (int b = 0; b < bl; ++b) {
            const double x = rnd.R(2, VW - 2);
            const double yy = rnd.R(VH * .35, VH - 14);
            const double r = rnd.R(.6, 1.4);
            dot(x, yy, r, P.glow(-40, .7, .82, .9));
        }
        break;
    }
    case MsVignette::Orient: {
        const double mx = rnd.R(VW * .62, VW - 14);
        const double my = rnd.R(14, 24);
        {
            // Crescente: o disco da lua menos um disco deslocado, recortado no próprio disco.
            QPainterPath disc;
            disc.addEllipse(QPointF(mx, my), 6, 6);
            QPainterPath moon;
            moon.setFillRule(Qt::OddEvenFill);
            moon.addEllipse(QPointF(mx, my), 6, 6);
            moon.addEllipse(QPointF(mx + 2.6, my - 1), 5.2, 5.2);
            g.save();
            g.setClipPath(disc, Qt::IntersectClip);
            g.setPen(Qt::NoPen);
            g.setBrush(P.glow(40, .6, .85, .95));
            g.drawPath(moon);
            g.restore();
        }
        g.fillRect(QRectF(0, VH - 10, VW, 10), P.shade(.14));
        const int n = jsRound((2 + grow * 4) * wide);
        const double row = VW / n;
        for (int i = 0; i < n; ++i) {
            const double x = i * row + row * .5 + rnd.R(-4, 4);
            const double bw = row * rnd.R(.55, .8);
            const double bh = rnd.R(10, 18) * (.6 + grow * .5);
            const double dr = bw * rnd.R(.36, .5);
            const QColor wall = P.shade(.3 + rnd() * .06);
            g.fillRect(QRectF(x - bw / 2, VH - 10 - bh, bw, bh), wall);
            const double y0 = VH - 10 - bh;
            QPainterPath dome(QPointF(x - dr, y0));
            dome.cubicTo(QPointF(x - dr, y0 - dr * 1.3), QPointF(x, y0 - dr * 1.6), QPointF(x, y0 - dr * 1.9));
            dome.cubicTo(QPointF(x, y0 - dr * 1.6), QPointF(x + dr, y0 - dr * 1.3), QPointF(x + dr, y0));
            fill(dome, wall);
            for (int a = 0; a < 3; ++a) {
                const double ax = x - bw / 2 + bw * (a + .5) / 3;
                QPainterPath door(QPointF(ax - 1.6, VH - 10));
                door.lineTo(ax - 1.6, VH - 10 - bh * .4);
                canvasArc(door, ax, VH - 10 - bh * .4, 1.6, PI, 0);
                door.lineTo(ax + 1.6, VH - 10);
                fill(door, P.shade(.12));
            }
            if (rnd() < .55) g.fillRect(QRectF(x - .8, VH - 10 - bh * .75, 1.6, 2.2), P.glow(40, .9, .72, 1));
            if (grow > .25 && rnd() < .6) {
                const double side = rnd() < .5 ? -1 : 1;
                const double mxx = x + side * (bw / 2 + 3);
                const double mh = bh + rnd.R(14, 26) * (.5 + grow * .6);
                const QColor mc = P.shade(.34);
                g.fillRect(QRectF(mxx - 1.4, VH - 10 - mh, 2.8, mh), mc);
                g.fillRect(QRectF(mxx - 2.4, VH - 10 - mh * .7, 4.8, 1.3), mc);
                QPainterPath tip(QPointF(mxx - 1.6, VH - 10 - mh));
                tip.lineTo(mxx, VH - 10 - mh - 6);
                tip.lineTo(mxx + 1.6, VH - 10 - mh);
                fill(tip, mc);
            }
        }
        break;
    }
    case MsVignette::Blizzard: {
        auto snow = [&](double a) { return P.glow(0, .18, .93, a); };
        const int trees = jsRound((2 + grow * 4) * wide);
        for (int i = 0; i < trees; ++i) {
            const double x = VW * (i + .5) / trees + rnd.R(-7, 7);
            const double h = rnd.R(18, 34);
            const double w = h * .36;
            const double y = VH - 12 - rnd.R(0, 8);
            QPainterPath tree;
            for (int t = 0; t < 3; ++t) {
                const double ty = y - h + t * h * .3, tw = w * (.5 + t * .28);
                tree.moveTo(x, ty);
                tree.lineTo(x + tw, ty + h * .42);
                tree.lineTo(x - tw, ty + h * .42);
                tree.closeSubpath();
            }
            fill(tree, P.shade(.12));
            for (int t = 0; t < 3; ++t) {
                const double ty = y - h + t * h * .3, tw = w * (.5 + t * .28);
                QPainterPath cap(QPointF(x, ty));
                cap.lineTo(x + tw * .45, ty + h * .19);
                cap.lineTo(x - tw * .45, ty + h * .19);
                cap.closeSubpath();
                fill(cap, snow(.9));
            }
        }
        const int drifts = 1 + jsRound(grow * 2);
        for (int d = 0; d < drifts; ++d) {
            const double base = VH - 6 - d * 6 - grow * 8;
            const double amp = rnd.R(3, 7);
            const double f = rnd.R(.03, .06);
            const double ph = rnd() * 7;
            QPainterPath drift(QPointF(0, VH));
            for (double x = 0; x <= VW; x += 2)
                drift.lineTo(x, base - std::sin(x * f + ph) * amp - std::sin(x * f * 2.3) * amp * .3);
            drift.lineTo(VW, VH);
            fill(drift, snow(d ? .75 : .95));
        }
        // o vento: quanto maior o capítulo, mais riscos inclinados
        const double wind = rnd.R(.35, .6);
        const int streaks = jsRound(grow * grow * 50 * wide);
        pen(snow(.35), .6);
        for (int i = 0; i < streaks; ++i) {
            const double x = rnd.R(-10, VW);
            const double y = rnd.R(0, VH - 14);
            const double l = rnd.R(5, 12);
            g.drawLine(QPointF(x, y), QPointF(x + l, y + l * wind));
        }
        const int flakes = jsRound((20 + grow * 120) * wide);
        for (int i = 0; i < flakes; ++i) {
            const double x = rnd.R(0, VW);
            const double y = rnd.R(0, VH - 8);
            double r = rnd.R(.35, 1.5);
            r *= rnd() < .1 ? 1.6 : 1;
            const double a = rnd.R(.55, 1);
            dot(x, y, r, snow(a));
        }
        break;
    }
    default: break;
    }
}


QPainterPath shapePath(QSize size, bool circle) {
    QPainterPath clip;
    const QRectF r(0, 0, size.width(), size.height());
    if (circle) {
        const double d = std::min(r.width(), r.height());
        clip.addEllipse(QRectF((r.width() - d) / 2, (r.height() - d) / 2, d, d));
    } else {
        clip.addRoundedRect(r, 5.0 * size.width() / 96.0, 5.0 * size.width() / 96.0);
    }
    return clip;
}

void paintRing(QPainter& p, QSize size, const Pal& P) {
    const double d = std::min(size.width(), size.height());
    const double lw = std::max(0.75, 1.5 * d / 104.0);
    QPen pen(P.ring, lw);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF((size.width() - d) / 2 + lw / 2, (size.height() - d) / 2 + lw / 2, d - lw, d - lw));
}

}  // namespace

namespace MsVignette {

QString familyId(int family) {
    return (family >= 0 && family < FamilyCount) ? QString::fromLatin1(kIds[family]) : QString();
}

int familyFromId(const QString& id) {
    for (int i = 0; i < FamilyCount; ++i)
        if (id == QLatin1String(kIds[i])) return i;
    return -1;
}

QString familyName(int family) {
    static const char* const names[FamilyCount] = {
        QT_TRANSLATE_NOOP("MsVignette", "Galhos"),
        QT_TRANSLATE_NOOP("MsVignette", "Raízes"),
        QT_TRANSLATE_NOOP("MsVignette", "Chamas"),
        QT_TRANSLATE_NOOP("MsVignette", "Cidade"),
        QT_TRANSLATE_NOOP("MsVignette", "Constelação"),
        QT_TRANSLATE_NOOP("MsVignette", "Montanhas"),
        QT_TRANSLATE_NOOP("MsVignette", "Coral"),
        QT_TRANSLATE_NOOP("MsVignette", "Rachaduras"),
        QT_TRANSLATE_NOOP("MsVignette", "Mandala"),
        QT_TRANSLATE_NOOP("MsVignette", "Relâmpago"),
        QT_TRANSLATE_NOOP("MsVignette", "Ondas"),
        QT_TRANSLATE_NOOP("MsVignette", "Círculos"),
        QT_TRANSLATE_NOOP("MsVignette", "Raios"),
        QT_TRANSLATE_NOOP("MsVignette", "Floresta"),
        QT_TRANSLATE_NOOP("MsVignette", "Jardim"),
        QT_TRANSLATE_NOOP("MsVignette", "Revoada"),
        QT_TRANSLATE_NOOP("MsVignette", "Chuva"),
        QT_TRANSLATE_NOOP("MsVignette", "Rio"),
        QT_TRANSLATE_NOOP("MsVignette", "Galáxia"),
        QT_TRANSLATE_NOOP("MsVignette", "Cristais"),
        QT_TRANSLATE_NOOP("MsVignette", "Aurora"),
        QT_TRANSLATE_NOOP("MsVignette", "Dunas"),
        QT_TRANSLATE_NOOP("MsVignette", "Teia"),
        QT_TRANSLATE_NOOP("MsVignette", "Ruínas"),
        QT_TRANSLATE_NOOP("MsVignette", "Farol"),
        QT_TRANSLATE_NOOP("MsVignette", "Castelo"),
        QT_TRANSLATE_NOOP("MsVignette", "Cyberpunk"),
        QT_TRANSLATE_NOOP("MsVignette", "Medieval"),
        QT_TRANSLATE_NOOP("MsVignette", "Steampunk"),
        QT_TRANSLATE_NOOP("MsVignette", "Japão feudal"),
        QT_TRANSLATE_NOOP("MsVignette", "Oriente"),
        QT_TRANSLATE_NOOP("MsVignette", "Nevasca"),
        QT_TRANSLATE_NOOP("MsVignette", "Vela"),
        QT_TRANSLATE_NOOP("MsVignette", "Ampulheta"),
        QT_TRANSLATE_NOOP("MsVignette", "Chave"),
        QT_TRANSLATE_NOOP("MsVignette", "Bússola"),
        QT_TRANSLATE_NOOP("MsVignette", "Relógio de bolso"),
        QT_TRANSLATE_NOOP("MsVignette", "Pena e tinteiro"),
        QT_TRANSLATE_NOOP("MsVignette", "Xícara"),
        QT_TRANSLATE_NOOP("MsVignette", "Pilha de livros"),
        QT_TRANSLATE_NOOP("MsVignette", "Lampião"),
        QT_TRANSLATE_NOOP("MsVignette", "Guitarra"),
        QT_TRANSLATE_NOOP("MsVignette", "Estrada"),
        QT_TRANSLATE_NOOP("MsVignette", "Noir"),
        QT_TRANSLATE_NOOP("MsVignette", "Mansão"),
        QT_TRANSLATE_NOOP("MsVignette", "Planeta"),
        QT_TRANSLATE_NOOP("MsVignette", "Frota"),
        QT_TRANSLATE_NOOP("MsVignette", "Veleiro"),
        QT_TRANSLATE_NOOP("MsVignette", "Asas"),
        QT_TRANSLATE_NOOP("MsVignette", "Roda-gigante"),
        QT_TRANSLATE_NOOP("MsVignette", "Morro"),
        QT_TRANSLATE_NOOP("MsVignette", "Trem"),
        QT_TRANSLATE_NOOP("MsVignette", "Azulejo"),
        QT_TRANSLATE_NOOP("MsVignette", "Labirinto"),
        QT_TRANSLATE_NOOP("MsVignette", "Cordel"),
        QT_TRANSLATE_NOOP("MsVignette", "Origami"),
        QT_TRANSLATE_NOOP("MsVignette", "Mapa do tesouro"),
        QT_TRANSLATE_NOOP("MsVignette", "Xadrez"),
        QT_TRANSLATE_NOOP("MsVignette", "Lâminas"),
        QT_TRANSLATE_NOOP("MsVignette", "Armas de fogo") };
    if (family < 0 || family >= FamilyCount) return QString();
    return QCoreApplication::translate("MsVignette", names[family]);
}

QList<FamilyGroup> familyGroups() {
    QList<int> classic;
    for (int f = 0; f < kAutoFamilyCount; ++f) classic << f;
    QList<FamilyGroup> groups = {
        { QCoreApplication::translate("MsVignette", "Clássicos"), classic },
        { QCoreApplication::translate("MsVignette", "Natureza e céu"),
          { Forest, Garden, Flock, Rain, River, Galaxy, Crystals, Aurora, Dunes, Web, Blizzard } },
        { QCoreApplication::translate("MsVignette", "Lugares"),
          { Ruins, Lighthouse, Castle, Cyberpunk, Medieval, Steampunk, FeudalJapan, Orient } },
        { QCoreApplication::translate("MsVignette", "Objetos"),
          { Candle, Hourglass, Key, Compass, PocketWatch, Inkwell, Cup, Books, Lantern, Guitar } },
        { QCoreApplication::translate("MsVignette", "Cenas"),
          { Road, Noir, Mansion, Planet, Fleet, Sailboat, Wings, FerrisWheel, Hill, Train } },
        { QCoreApplication::translate("MsVignette", "Feitos à mão"),
          { Azulejo, Maze, Cordel, Origami, TreasureMap, Chess } },
        { QCoreApplication::translate("MsVignette", "Arsenal"), { Blades, Firearms } },
    };
    // as famílias da leva 3 entram uma leva por vez: grupo sem nada portado não aparece
    QList<FamilyGroup> out;
    for (FamilyGroup g : groups) {
        QList<int> keep;
        for (int f : g.families) if (f < kFirstLeva3Family || MsVignetteLeva3::isPorted(f)) keep << f;
        if (!keep.isEmpty()) { g.families = keep; out << g; }
    }
    return out;
}

int autoFamily(const QString& chapterId) {
    return int(hashStr(chapterId + QStringLiteral("#f")) % kAutoFamilyCount);
}

QList<int> autoFamilies(const QStringList& chapterIds) {
    QList<int> out;
    for (int j = 0; j < chapterIds.size(); ++j) {
        int f = autoFamily(chapterIds.at(j));
        while (j && f == out.at(j - 1)) f = (f + 1) % kAutoFamilyCount;
        out << f;
    }
    return out;
}

QList<int> familiesForBook(const QStringList& chapterIds, const QStringList& chapterFamilies,
                           const QString& bookFamily) {
    QList<int> out = autoFamilies(chapterIds);
    const int book = familyFromId(bookFamily);
    for (int j = 0; j < out.size(); ++j) {
        const int own = j < chapterFamilies.size() ? familyFromId(chapterFamilies.at(j)) : -1;
        if (own >= 0) out[j] = own;
        else if (book >= 0) out[j] = book;
    }
    return out;
}

QPixmap render(const QString& seedId, int words, const QColor& base, int family,
               QSize size, qreal dpr, bool circle) {
    // O Índice e a Temporadas refazem a lista a cada mudança; o desenho só
    // muda com a semente, a cor, a família, o tamanho e as palavras.
    static QHash<QString, QPixmap> cache;
    const QString key = seedId + QLatin1Char('|') + QString::number(words) + QLatin1Char('|')
        + base.name() + QLatin1Char('|') + QString::number(family) + QLatin1Char('|')
        + QString::number(size.width()) + QLatin1Char('x') + QString::number(size.height())
        + QLatin1Char('|') + QString::number(dpr) + (circle ? QStringLiteral("|c") : QStringLiteral("|r"));
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) return it.value();

    QPixmap pm(size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const int fam = family < 0 ? autoFamily(seedId) : family;
    if (fam >= kFirstLeva3Family) {
        // leva 3: port linha a linha do concept, desenhado pelo MsCanvas (MsVignetteLeva3.cpp)
        if (MsVignetteLeva3::render(p, size, seedId, words, base, fam, !circle)) {
            p.end();
            if (cache.size() > 400) cache.clear();
            cache.insert(key, pm);
            return pm;
        }
        family = autoFamily(seedId);   // família ainda não portada (projeto aberto numa versão mais nova): cai no automático
    }
    const Pal P = paletteFor(base);
    p.setClipPath(shapePath(size, circle));
    const double W = size.width(), H = size.height();
    QRadialGradient bg(QPointF(W / 2, H / 2), std::max(W, H) * .7, QPointF(W * .4, H * .3));
    bg.setColorAt(0, P.bg0);
    bg.setColorAt(1, P.bg1);
    p.fillRect(QRectF(0, 0, W, H), bg);

    const double s = std::min(W, H) / 104.0;
    p.save();
    p.scale(s, s);
    Rng rnd{ hashStr(seedId) };
    const double grow = std::clamp(std::log10(words + 1.0) / std::log10(15000.0), 0.12, 1.0);
    drawFamily(p, family < 0 ? autoFamily(seedId) : family, rnd, grow, W / s, H / s, P);
    p.restore();
    p.setClipping(false);
    if (circle) paintRing(p, size, P);
    p.end();

    if (cache.size() > 400) cache.clear();
    cache.insert(key, pm);
    return pm;
}

QPixmap renderImage(const QPixmap& image, const QColor& base, QSize size, qreal dpr, bool circle) {
    QPixmap pm(size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setClipPath(shapePath(size, circle));
    QPixmap sc = image.scaled(size * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    sc.setDevicePixelRatio(dpr);
    p.drawPixmap(QPointF((size.width() - sc.width() / dpr) / 2.0, (size.height() - sc.height() / dpr) / 2.0), sc);
    p.setClipping(false);
    if (circle) paintRing(p, size, paletteFor(base));
    return pm;
}

}
