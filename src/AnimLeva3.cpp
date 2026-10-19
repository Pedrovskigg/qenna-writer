// Temas Animados, leva 3 (cidades): beco de Tóquio, fachada com escada de
// incêndio, rastros de luz, pista de pouso, Lapa, roda-gigante, Kowloon e posto
// de estrada. Portado de leva3a.js / leva3b.js.

#include "AnimKit.h"

#include <QFont>
#include <QtMath>
#include <QTransform>
#include <QFontMetricsF>
#include <QImage>
#include <QLinearGradient>
#include <QPolygonF>
#include <QRadialGradient>

#include <algorithm>
#include <array>
#include <cmath>

namespace AnimBg {
namespace {

using namespace kit;

QFont cjk(double px, bool bold = true)
{
    QFont f;
    f.setFamilies({ QStringLiteral("Yu Gothic"), QStringLiteral("Microsoft JhengHei"), QStringLiteral("MS Gothic"), QStringLiteral("Microsoft YaHei") });
    f.setPixelSize(qMax(1, int(std::lround(px))));
    f.setWeight(bold ? QFont::Bold : QFont::Normal);
    return f;
}

// ============================================================ beco das lanternas (Tóquio)

class Alley : public Layer {
public:
    void init(Ctx& c) override
    {
        m_back.reset();
        m_steam.clear();
        m_hasWalker = false;
        m_nextWalker = 3;
        m_lans.clear();
        for (int side : { -1, 1 }) for (double u = 0.06; u < 1.8; u += 0.12) m_lans.push_back({ side, u, R(0, 6.28), rnd() < 0.8 });
        m_signs.clear();
        static const char* K[] = { "酒", "居酒屋", "ラーメン", "焼鳥", "バー", "喫茶", "寿司", "営業中", "餃子" };
        static const char* N[] = { "#ff4a5a", "#ffb648", "#7ce0ff", "#ff7ad9", "#9dff8a", "#fff1c8" };
        for (int side : { -1, 1 }) for (double u = 0.12; u < 1.6; u += R(0.16, 0.3)) m_signs.push_back({ side, u, R(0.45, 0.8), QString::fromUtf8(K[RI(9)]), hex(N[RI(6)]), 0 });
        Q_UNUSED(c);
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        if (rnd() < dt * 4) m_steam.push_back({ wallX(c, -1, 0.28) + 40, yAt(c, 0.28, 0.32), 6, 0.18, R(6, 16) });
        for (auto& q : m_steam) { q[1] -= 26 * dt; q[0] += (q[4] + std::sin(m_t + q[1] * 0.04) * 6) * dt; q[2] += 10 * dt; q[3] -= 0.05 * dt; }
        m_steam.erase(std::remove_if(m_steam.begin(), m_steam.end(), [](const std::array<double, 5>& q) { return q[3] <= 0; }), m_steam.end());
        for (Sign& g : m_signs) { if (g.glitch > 0) g.glitch -= dt; else if (rnd() < dt * 0.06) g.glitch = R(0.15, 0.6); }
        m_nextWalker -= dt;
        if (!m_hasWalker && m_nextWalker <= 0) { m_wu = 1.7; m_wph = 0; m_hasWalker = true; }
        if (m_hasWalker) { m_wu -= 0.07 * dt; m_wph += dt * 4; if (m_wu < 0.15) { m_hasWalker = false; m_nextWalker = R(8, 18); } }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        if (!m_back.valid(c)) build(pal, c);
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // luz da máquina de bebida no chão
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        glow(p, wallX(c, -1, 0.42) + 43, yAt(c, 0.42, 0), 160, hex("#cfe4ff"), 0.28);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // letreiros e lanternas, do fundo pra frente
        struct It { double u; int kind; size_t i; };
        std::vector<It> items;
        for (size_t i = 0; i < m_signs.size(); ++i) items.push_back({ m_signs[i].u, 0, i });
        for (size_t i = 0; i < m_lans.size(); ++i) items.push_back({ m_lans[i].u, 1, i });
        std::sort(items.begin(), items.end(), [](const It& a, const It& b) { return a.u > b.u; });
        for (const It& it : items) {
            const double sc = 1 - s(it.u);
            if (it.kind == 0) {
                const Sign& g = m_signs[it.i];
                const double gx = wallX(c, g.side, g.u) - g.side * 52 * sc, gy = yAt(c, g.u, g.h), w = 58 * sc;
                const int n = g.txt.size();
                const double hh = n * 50 * sc + 18 * sc;
                const bool on = !(g.glitch > 0 && rnd() < 0.6);
                p.fillRect(QRectF(gx - w / 2 - 2, gy - 2, w + 4, hh + 4), mix(pal.app, Qt::black, 0.4));
                p.fillRect(QRectF(gx - w / 2, gy, w, hh), alpha(g.c, on ? 0.9 : 0.15));
                p.setFont(cjk(qMax(6.0, 38 * sc)));
                p.setPen(on ? hex("#1a1018") : alpha(g.c, 0.5));
                for (int k = 0; k < n; ++k) p.drawText(QRectF(gx - w / 2, gy + 9 * sc + k * 50 * sc, w, 50 * sc), Qt::AlignCenter, g.txt.mid(k, 1));
                if (on) {
                    p.setCompositionMode(QPainter::CompositionMode_Plus);
                    glow(p, gx, gy + hh / 2, hh, g.c, 0.25);
                    const double fy = yAt(c, g.u, 0);
                    for (int k = 0; k < 6; ++k) p.fillRect(QRectF(gx - w / 2 + std::sin(m_t * 2 + k + g.u * 9) * 2, fy + 8 + k * 6 * sc, w, 2.5 * sc + 1), alpha(g.c, 0.18));
                    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
                }
            } else {
                const Lan& l = m_lans[it.i];
                const QColor lc = hex(l.red ? "#e0402a" : "#f2e2b0");
                const double a = std::sin(m_t * 1.3 + l.ph) * 0.09, bx = wallX(c, l.side, l.u) - l.side * 34 * sc, by = yAt(c, l.u, 0.58), r = 30 * sc;
                p.setPen(QPen(Qt::black, qMax(1.0, 2 * sc)));
                p.drawLine(QPointF(wallX(c, l.side, l.u), by), QPointF(bx, by));
                p.save();
                p.translate(bx, by);
                p.rotate(a * 180 / M_PI);
                p.setCompositionMode(QPainter::CompositionMode_Plus);
                glow(p, 0, r * 1.6, r * 4, lc, 0.35);
                p.setCompositionMode(QPainter::CompositionMode_SourceOver);
                QRadialGradient bg(QPointF(0, r * 1.6), r * 1.2, QPointF(-r * 0.3, r * 1.3));
                bg.setColorAt(0, mix(lc, hex("#fff2c0"), 0.5));
                bg.setColorAt(1, lc);
                p.setPen(Qt::NoPen);
                p.setBrush(bg);
                p.drawEllipse(QPointF(0, r * 1.6), r * 0.85, r * 1.15);
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(alpha(Qt::black, 0.25), qMax(0.5, sc)));
                for (int i = -2; i <= 2; ++i) p.drawEllipse(QPointF(0, r * 1.6 + i * r * 0.36), r * 0.85 * std::sqrt(1 - std::pow(i * 0.3, 2)), r * 0.08);
                p.fillRect(QRectF(-r * 0.5, r * 0.35, r, r * 0.22), hex("#1a1010"));
                p.fillRect(QRectF(-r * 0.5, r * 2.65, r, r * 0.22), hex("#1a1010"));
                if (l.red && sc > 0.35) { p.setFont(cjk(r * 0.9)); p.setPen(alpha(hex("#1a0a08"), 0.75)); p.drawText(QRectF(-r, r * 1.1, r * 2, r), Qt::AlignCenter, QString::fromUtf8("酒")); }
                p.restore();
            }
        }
        // alguém de guarda-chuva vindo pelo beco
        if (m_hasWalker) {
            const double sc = 1 - s(m_wu), x0 = vx(c) - 150 * sc, y0 = yAt(c, m_wu, 0), hgt = 175 * sc;
            Fig f;
            f.pose = Fig::Walk;
            f.ph = m_wph;
            f.hair = Fig::Long;
            f.arm = Fig::Umbrella;
            f.build = 0.92;
            figure(p, x0, y0, hgt, f);
            const QPointF h = figureHand(x0, y0, hgt, f);
            const double top = y0 - hgt * 1.13, r = hgt * 0.42;
            p.setPen(QPen(f.col, qMax(1.0, hgt * 0.012)));
            p.drawLine(h, QPointF(h.x(), top));
            QPainterPath um(QPointF(h.x() - r, top + r * 0.32));
            um.quadTo(h.x() - r, top - r * 0.5, h.x(), top - r * 0.52);
            um.quadTo(h.x() + r, top - r * 0.5, h.x() + r, top + r * 0.32);
            for (int i = 3; i >= 0; --i) um.quadTo(h.x() - r + (i + 0.5) * r / 2, top + r * 0.12, h.x() - r + i * r / 2, top + r * 0.32);
            um.closeSubpath();
            p.fillPath(um, mix(pal.acc, Qt::black, 0.35));
            p.setPen(QPen(alpha(Qt::black, 0.35), qMax(0.6, hgt * 0.006)));
            for (int i = 1; i < 4; ++i) p.drawLine(QPointF(h.x(), top - r * 0.52), QPointF(h.x() - r + i * r / 2, top + r * 0.3));
        }
        for (const auto& q : m_steam) { p.setOpacity(q[3] * 1.8); blit(p, smokeDot(), q[0] - q[2], q[1] - q[2], q[2] * 2, q[2] * 2); }
        p.setOpacity(1);
    }

private:
    struct Lan { int side; double u, ph; bool red; };
    struct Sign { int side; double u, h; QString txt; QColor c; double glitch; };
    static double s(double u) { return 1 - 1 / (1 + u * 4); }
    // o ponto de fuga fica atrás da folha; as paredes ocupam a mesa dos dois lados
    static double vx(const Ctx& c) { return c.W / 2; }
    static double wallX(const Ctx& c, int side, double u) { return side < 0 ? lerp(-60, vx(c) - 110, s(u)) : lerp(c.W + 60, vx(c) + 110, s(u)); }
    static double yAt(const Ctx& c, double u, double h) { return lerp(lerp(c.H + 30, 300 + 80, s(u)), lerp(-300, 300 - 140, s(u)), h); }
    void build(const Pal& pal, const Ctx& c)
    {
        QPixmap pm = layer(c, c.W, c.H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing, true);
        x.fillRect(QRectF(0, 0, c.W, c.H), mix(pal.app, Qt::black, 0.3));
        QLinearGradient sg(0, 0, 0, 300);
        sg.setColorAt(0, mix(pal.app, hex("#1a1030"), 0.4));
        sg.setColorAt(1, mix(pal.app, hex("#5a2a40"), 0.4));
        QPolygonF skyp;
        skyp << QPointF(wallX(c, -1, 0), -10) << QPointF(wallX(c, -1, 3), yAt(c, 3, 1)) << QPointF(wallX(c, 1, 3), yAt(c, 3, 1)) << QPointF(wallX(c, 1, 0), -10);
        x.setPen(Qt::NoPen);
        x.setBrush(sg);
        x.drawPolygon(skyp);
        for (int side : { -1, 1 }) {
            for (double u = 0; u < 3; u += 0.05) {
                const QColor col = mix(mix(pal.app, hex("#1a1418"), 0.5), mix(pal.app, hex("#3a2a3a"), 0.4), s(u));
                QPolygonF q;
                q << QPointF(wallX(c, side, u), yAt(c, u, 0)) << QPointF(wallX(c, side, u + 0.051), yAt(c, u + 0.051, 0))
                  << QPointF(wallX(c, side, u + 0.051), yAt(c, u + 0.051, 1)) << QPointF(wallX(c, side, u), yAt(c, u, 1));
                x.setBrush(col);
                x.drawPolygon(q);
            }
            for (double u = 0.1; u < 2.4; u += 0.14)
                for (double h : { 0.1, 0.62, 0.8 }) {
                    if (rnd() < 0.35) continue;
                    const double u1 = u + 0.05;
                    const bool lit = rnd() < (h < 0.2 ? 0.45 : 0.18);
                    const QColor col = lit ? hex(h < 0.2 ? "#c8783a" : "#b09060") : mix(pal.app, Qt::black, 0.5);
                    const double hh = h < 0.2 ? 0.18 : 0.07;
                    QPolygonF q;
                    q << QPointF(wallX(c, side, u), yAt(c, u, h)) << QPointF(wallX(c, side, u1), yAt(c, u1, h))
                      << QPointF(wallX(c, side, u1), yAt(c, u1, h + hh)) << QPointF(wallX(c, side, u), yAt(c, u, h + hh));
                    x.setBrush(alpha(col, lit ? 0.55 : 0.8));
                    x.drawPolygon(q);
                }
        }
        // chão molhado
        QLinearGradient gg(0, 380, 0, c.H);
        gg.setColorAt(0, mix(pal.app, hex("#2a2030"), 0.4));
        gg.setColorAt(1, mix(pal.app, Qt::black, 0.5));
        QPolygonF fl;
        fl << QPointF(wallX(c, -1, 0), c.H + 30) << QPointF(wallX(c, -1, 3), yAt(c, 3, 0)) << QPointF(wallX(c, 1, 3), yAt(c, 3, 0)) << QPointF(wallX(c, 1, 0), c.H + 30);
        x.setBrush(gg);
        x.drawPolygon(fl);
        // fios cruzando por cima
        x.setBrush(Qt::NoBrush);
        x.setPen(QPen(alpha(Qt::black, 0.8), 1.2));
        for (int i = 0; i < 14; ++i) {
            const double u1 = R(0.05, 1.6), u2 = u1 + R(-0.2, 0.3), h1 = R(0.75, 0.95), h2 = R(0.7, 0.95);
            const QPointF a(wallX(c, -1, u1), yAt(c, u1, h1)), b(wallX(c, 1, u2), yAt(c, u2, h2));
            QPainterPath w(a);
            w.quadTo((a.x() + b.x()) / 2, qMax(a.y(), b.y()) + R(20, 60) * (1 - s(u1)), b.x(), b.y());
            x.drawPath(w);
        }
        // máquina de bebida, à esquerda
        const double mx = wallX(c, -1, 0.42) + 6, my = yAt(c, 0.42, 0), mh = 150, mw = 74;
        x.fillRect(QRectF(mx, my - mh, mw, mh), hex("#e8f2ff"));
        x.fillRect(QRectF(mx, my - mh, mw, 18), hex("#2a3a8a"));
        x.fillRect(QRectF(mx, my - 34, mw, 34), hex("#2a3a8a"));
        static const char* B[] = { "#d8402a", "#2a8ad8", "#f2c23a", "#3aa85a", "#e8e8e8", "#8a4a2a" };
        for (int r = 0; r < 3; ++r)
            for (int k = 0; k < 6; ++k) {
                x.fillRect(QRectF(mx + 6 + k * 11, my - mh + 26 + r * 30, 7, 20), hex(B[RI(6)]));
                x.fillRect(QRectF(mx + 7 + k * 11, my - mh + 49 + r * 30, 5, 2), hex("#ff4a3a"));
            }
        x.end();
        m_back.set(c, pm);
    }
    Cache m_back;
    std::vector<Lan> m_lans;
    std::vector<Sign> m_signs;
    std::vector<std::array<double, 5>> m_steam;
    bool m_hasWalker = false;
    double m_t = 0, m_nextWalker = 3, m_wu = 0, m_wph = 0;
};

// ============================================================ escada de incêndio (fachada à noite)

class Facade : public Layer {
public:
    void init(Ctx& c) override
    {
        m_wall.reset();
        m_steam.clear();
        m_wins.clear();
        for (int f = 0; f < 6; ++f)
            for (int k = 0; k < int(std::ceil(c.W / COLW)); ++k) {
                const double r = rnd();
                Win w;
                w.x = 30 + k * COLW;
                w.y = TOP + 26 + f * FH;
                w.kind = r < 0.38 ? 0 : r < 0.7 ? 1 : r < 0.86 ? 2 : 3;
                w.blinds = rnd() < 0.3; w.curtain = rnd() < 0.35; w.plant = rnd() < 0.2;
                w.nextWalk = R(2, 40);
                w.hair = hairAt(RI(8)); w.build = R(0.88, 1.12); w.still = rnd() < 0.12; w.sx = R(14, 38); w.ph = R(0, 6.28);
                m_wins.push_back(w);
            }
    }
    void step(double dt, Ctx&) override
    {
        m_t += dt;
        m_toggle += dt;
        if (m_toggle > 1.8 && !m_wins.empty()) { m_toggle = 0; Win& w = m_wins[size_t(RI(int(m_wins.size())))]; w.kind = w.kind == 0 ? (RI(4) == 0 ? 3 : RI(2) ? 1 : 2) : 0; }
        for (Win& w : m_wins) {
            if (w.kind == 3) w.tvn += (R(-1, 1) - w.tvn) * qMin(1.0, dt * 6);
            if (w.kind != 0) {
                w.nextWalk -= dt;
                if (w.walkD <= 0 && w.nextWalk <= 0) { w.walkT = 0; w.walkD = R(3, 6); w.dir = rnd() < 0.5 ? 1 : -1; }
                if (w.walkD > 0) { w.walkT += dt; if (w.walkT > w.walkD) { w.walkD = 0; w.nextWalk = R(15, 60); } }
            }
        }
        if (rnd() < dt * 3) m_steam.push_back({ 92, 40, 5, 0.2, R(4, 12) });
        for (auto& q : m_steam) { q[1] -= 20 * dt; q[0] += (q[4] + std::sin(m_t + q[1] * 0.05) * 5) * dt; q[2] += 8 * dt; q[3] -= 0.05 * dt; }
        m_steam.erase(std::remove_if(m_steam.begin(), m_steam.end(), [](const std::array<double, 5>& q) { return q[3] <= 0; }), m_steam.end());
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double ox = zx(c, 45) - 45;
        if (!m_wall.valid(c)) build(pal, c, ox);
        sky(p, c, mix(pal.app, hex("#0a0c1a"), 0.4), mix(pal.app, hex("#2a1a30"), 0.35));
        // interior das janelas
        for (const Win& w : m_wins) {
            QColor col;
            if (w.kind == 1) col = hex("#e8a85a"); else if (w.kind == 2) col = hex("#cfd8e6"); else if (w.kind == 3) col = mix(hex("#3a5aa8"), hex("#a8c8ff"), 0.5 + 0.5 * w.tvn);
            const QRectF r(w.x, w.y, WW, WH);
            p.fillRect(r, col.isValid() ? mix(col, pal.app, 0.25) : mix(pal.app, Qt::black, 0.45));
            if (!col.isValid()) continue;
            QLinearGradient g(0, w.y, 0, w.y + WH);
            g.setColorAt(0, alpha(Qt::black, 0.35));
            g.setColorAt(1, alpha(Qt::black, 0));
            p.fillRect(r, g);
            const QColor sil = mix(col, hex("#1a1014"), 0.82);
            if (w.walkD > 0) {
                const double k = w.walkT / w.walkD, px = w.dir > 0 ? lerp(w.x - 20, w.x + WW + 20, k) : lerp(w.x + WW + 20, w.x - 20, k);
                p.save();
                p.setClipRect(r, Qt::IntersectClip);
                Fig f; f.pose = Fig::Walk; f.ph = w.walkT * 5; f.hair = w.hair; f.build = w.build; f.col = sil;
                figure(p, px, w.y + WH + 46, 112, f);
                p.restore();
            } else if (w.still) {
                p.save();
                p.setClipRect(r, Qt::IntersectClip);
                Fig f; f.hair = w.hair; f.build = w.build; f.col = sil;
                f.arm = std::sin(m_t * 0.3 + w.ph) > 0.6 ? Fig::Cup : Fig::Down; f.lift = 0.9; f.tilt = std::sin(m_t * 0.4 + w.ph) * 0.08;
                figure(p, w.x + w.sx, w.y + WH + 46, 112, f);
                p.restore();
            }
            if (w.curtain) { const QColor cc = alpha(mix(col, hex("#7a2a2a"), 0.6), 0.85); p.fillRect(QRectF(w.x, w.y, 10, WH), cc); p.fillRect(QRectF(w.x + WW - 10, w.y, 10, WH), cc); }
            if (w.blinds) for (double yy = w.y + 3; yy < w.y + WH * 0.6; yy += 5) p.fillRect(QRectF(w.x, yy, WW, 2), alpha(Qt::black, 0.25));
            if (w.plant) { p.setPen(Qt::NoPen); p.setBrush(hex("#0e140e")); p.drawEllipse(QPointF(w.x + WW - 14, w.y + WH - 10), 10, 13); p.fillRect(QRectF(w.x + WW - 20, w.y + WH - 6, 12, 6), hex("#0e140e")); }
        }
        blit(p, m_wall.pm, 0, 0, c.W, c.H);
        // luz das janelas acesas vazando na parede
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const Win& w : m_wins) {
            if (w.kind == 0) continue;
            const QColor col = hex(w.kind == 3 ? "#7a9aff" : w.kind == 2 ? "#cfd8e6" : "#ffb060");
            const double a = w.kind == 3 ? 0.06 + 0.05 * w.tvn : 0.07;
            QRadialGradient g(QPointF(w.x + WW / 2, w.y + WH), 70);
            g.setColorAt(0, alpha(col, a));
            g.setColorAt(1, alpha(col, 0));
            p.fillRect(QRectF(w.x - 50, w.y + WH - 70, WW + 100, 140), g);
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        blit(p, m_iron.pm, 0, 0, c.W, c.H);
        // o gato na plataforma, rabo balançando
        const double cx = m_cat.x(), cy = m_cat.y(), sw = std::sin(m_t * 1.4) * 0.6 + std::sin(m_t * 3.1) * 0.15;
        const QColor ct = hex("#060508");
        p.setPen(Qt::NoPen);
        p.setBrush(ct);
        p.drawEllipse(QPointF(cx, cy - 13), 10, 13);
        p.drawEllipse(QPointF(cx + 1, cy - 30), 7, 7);
        QPolygonF ears;
        ears << QPointF(cx - 5, cy - 34) << QPointF(cx - 3, cy - 43) << QPointF(cx + 1, cy - 36);
        p.drawPolygon(ears);
        ears.clear();
        ears << QPointF(cx + 3, cy - 36) << QPointF(cx + 7, cy - 43) << QPointF(cx + 8, cy - 33);
        p.drawPolygon(ears);
        p.setBrush(Qt::NoBrush);
        p.setPen(pen(ct, 3.2));
        QPainterPath tail(QPointF(cx + 8, cy - 3));
        tail.cubicTo(cx + 22, cy + 2, cx + 20 + sw * 12, cy + 18, cx + 14 + sw * 22, cy + 30);
        p.drawPath(tail);
        for (const auto& q : m_steam) { p.setOpacity(q[3] * 1.8); blit(p, smokeDot(), q[0] + ox - q[2], q[1] - q[2], q[2] * 2, q[2] * 2); }
        p.setOpacity(1);
    }

private:
    static constexpr double TOP = 92, FH = 118, COLW = 118, WW = 52, WH = 76;
    struct Win { double x = 0, y = 0; int kind = 0; bool blinds = false, curtain = false, plant = false; double nextWalk = 10, walkT = 0, walkD = 0; int dir = 1; double tvn = 0;
                 Fig::Hair hair = Fig::Short; double build = 1; bool still = false; double sx = 20, ph = 0; };
    void build(const Pal& pal, const Ctx& c, double ox)
    {
        const QColor brick = mix(pal.app, hex("#6a3426"), 0.45);
        QPixmap pm = layer(c, c.W, c.H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing, true);
        x.fillRect(QRectF(0, TOP, c.W, c.H - TOP), brick);
        QVector<QLineF> mortar;
        int r = 0;
        for (double y = TOP; y < c.H; y += 7, ++r) { mortar.append(QLineF(0, y, c.W, y)); for (double xx = (r % 2) * 9; xx < c.W; xx += 18) mortar.append(QLineF(xx, y, xx, y + 7)); }
        x.setPen(QPen(alpha(Qt::black, 0.22), 1));
        x.drawLines(mortar);
        for (int i = 0, n = int(2500 * c.W / 1440); i < n; ++i) x.fillRect(QRectF(R(0, c.W), R(TOP, c.H), R(3, 9), 3), alpha(rnd() < 0.5 ? Qt::black : Qt::white, R(0.02, 0.06)));
        x.fillRect(QRectF(0, TOP - 10, c.W, 16), mix(brick, Qt::black, 0.35));
        x.fillRect(QRectF(0, TOP + 6, c.W, 3), mix(brick, Qt::white, 0.08));
        for (const Win& w : m_wins) {
            x.fillRect(QRectF(w.x - 5, w.y - 8, WW + 10, 8), mix(brick, Qt::black, 0.4));
            x.fillRect(QRectF(w.x - 6, w.y + WH, WW + 12, 6), mix(brick, hex("#e8e0d0"), 0.25));
            x.setCompositionMode(QPainter::CompositionMode_Clear);
            x.fillRect(QRectF(w.x, w.y, WW, WH), Qt::transparent);
            x.setCompositionMode(QPainter::CompositionMode_SourceOver);
            x.setPen(QPen(mix(brick, Qt::black, 0.55), 3));
            x.setBrush(Qt::NoBrush);
            x.drawRect(QRectF(w.x, w.y, WW, WH));
            x.setPen(QPen(mix(brick, Qt::black, 0.55), 2));
            x.drawLine(QPointF(w.x, w.y + WH / 2), QPointF(w.x + WW, w.y + WH / 2));
        }
        // caixa d'água no telhado
        const double tx = 270 + ox;
        const QColor dk = mix(pal.app, Qt::black, 0.55);
        x.fillRect(QRectF(tx - 36, TOP - 104, 72, 78), dk);
        QPolygonF roof;
        roof << QPointF(tx - 40, TOP - 104) << QPointF(tx, TOP - 130) << QPointF(tx + 40, TOP - 104);
        x.setPen(Qt::NoPen);
        x.setBrush(dk);
        x.drawPolygon(roof);
        for (double lx : { -34.0, 30.0, -2.0 }) x.fillRect(QRectF(tx + lx, TOP - 26, 4, 18), dk);
        x.setPen(QPen(mix(pal.app, Qt::black, 0.4), 1));
        for (double yy = TOP - 98; yy < TOP - 26; yy += 14) x.drawLine(QPointF(tx - 36, yy), QPointF(tx + 36, yy));
        x.fillRect(QRectF(86 + ox, TOP - 40, 12, 40), dk);
        x.end();
        m_wall.set(c, pm);
        // escada de incêndio: plataformas com grade e escadas em zigue-zague
        QPixmap ip = layer(c, c.W, c.H);
        QPainter y(&ip);
        y.setRenderHint(QPainter::Antialiasing, true);
        const QColor ic = hex("#0c0b0e");
        const double nx = zx(c, 175) - 40, w2 = 150;
        for (int f = 0; f < 6; ++f) {
            const double py = TOP + 26 + f * FH + WH + 8;
            y.fillRect(QRectF(nx, py, w2, 5), ic);
            y.setPen(QPen(ic, 2));
            y.drawLine(QPointF(nx, py - 34), QPointF(nx + w2, py - 34));
            QVector<QLineF> bars;
            for (double xx = nx; xx <= nx + w2; xx += 9) bars.append(QLineF(xx, py, xx, py - 34));
            y.setPen(QPen(ic, 1.2));
            y.drawLines(bars);
            if (f < 5) {
                const bool odd = f % 2;
                const double a = odd ? nx + 12 : nx + w2 - 12, b = odd ? nx + w2 - 40 : nx + 40, ny2 = py + FH, off = odd ? 14 : -14;
                y.setPen(QPen(ic, 2.5));
                y.drawLine(QPointF(a, py), QPointF(b, ny2));
                y.drawLine(QPointF(a + off, py), QPointF(b + off, ny2));
                QVector<QLineF> steps;
                for (int k = 1; k < 12; ++k) { const double u = k / 12.0; steps.append(QLineF(lerp(a, b, u), lerp(py, ny2, u), lerp(a, b, u) + off, lerp(py, ny2, u))); }
                y.setPen(QPen(ic, 1.2));
                y.drawLines(steps);
            }
        }
        y.end();
        m_iron.set(c, ip);
        m_cat = QPointF(nx + 104, TOP + 26 + 2 * FH + WH + 8);
    }
    Cache m_wall, m_iron;
    std::vector<Win> m_wins;
    std::vector<std::array<double, 5>> m_steam;
    QPointF m_cat;
    double m_t = 0, m_toggle = 0;
};

// ============================================================ rastros de luz (viaduto em longa exposição)

class Trails : public Layer {
public:
    void init(Ctx& c) override
    {
        m_back.reset();
        m_lanes.clear();
        const double o = zx(c, 45) - 45, W = c.W;
        auto bez = [](QPointF a, QPointF b, QPointF cc, QPointF d) {
            std::vector<QPointF> pts;
            for (int i = 0; i <= 240; ++i) { const double t = i / 240.0, u = 1 - t; pts.push_back(u * u * u * a + 3 * u * u * t * b + 3 * u * t * t * cc + t * t * t * d); }
            return pts;
        };
        m_lanes.push_back(bez({ -60, 690 }, { 300, 600 }, { 800, 540 }, { W + 60, 600 }));
        m_lanes.push_back(bez({ -60, 330 }, { 500, 420 }, { 900, 280 }, { W + 60, 320 }));
        std::vector<QPointF> loop;
        for (int i = 0; i <= 240; ++i) { const double a = -M_PI / 2 + i / 240.0 * 1.75 * M_PI; loop.push_back({ 235 + o + 135 * std::cos(a), 360 + 105 * std::sin(a) }); }
        m_lanes.push_back(loop);
        m_lanes.push_back(bez({ 140 + o, -60 }, { 250 + o, 260 }, { 90 + o, 470 }, { 300 + o, 820 }));
        m_lanes.push_back(bez({ rx(c, 1120), -60 }, { rx(c, 1240), 260 }, { rx(c, 1380), 420 }, { rx(c, 1180), 820 }));
        m_cars.clear();
        for (size_t k = 0; k < m_lanes.size(); ++k) for (int i = 0, n = int(std::lround(16 * INT(c))); i < n; ++i) m_cars.push_back(newCar(int(k), true));
    }
    void step(double dt, Ctx&) override
    {
        for (Car& car : m_cars) { car.t += car.dir * car.v * dt; if (car.t > 1 || car.t < 0) car = newCar(car.k, false); }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        if (!m_back.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing, true);
            sky(x, c, mix(pal.app, hex("#05060e"), 0.4), mix(pal.app, hex("#1c1428"), 0.35));
            const QColor bc = mix(pal.app, Qt::black, 0.25);
            QVector<QRectF> wins;
            for (double bx = 0; bx < c.W;) {
                const double bw = R(30, 80), bh = R(60, 220);
                x.fillRect(QRectF(bx, 260 - bh, bw, bh + 40), bc);
                for (double wy = 270 - bh; wy < 290; wy += 9) for (double wx = bx + 4; wx < bx + bw - 4; wx += 7) if (rnd() < 0.2) wins.append(QRectF(wx, wy, 2.5, 3));
                bx += bw + R(0, 5);
            }
            x.setPen(Qt::NoPen);
            x.setBrush(alpha(hex("#ffd28a"), 0.6));
            x.drawRects(wins);
            for (const auto& P : m_lanes) {
                QPolygonF pl;
                for (const QPointF& q : P) pl << q;
                x.setBrush(Qt::NoBrush);
                x.setPen(pen(mix(pal.app, Qt::black, 0.45), 30));
                x.drawPolyline(pl);
                x.setPen(pen(mix(pal.app, hex("#3a3440"), 0.4), 2));
                x.drawPolyline(pl);
                for (size_t i = 10; i < P.size(); i += 26) {
                    glow(x, P[i].x(), P[i].y() - 16, 22, hex("#ffa040"), 0.22);
                    x.fillRect(QRectF(P[i].x() - 1.5, P[i].y() - 18, 3, 3), hex("#ffd08a"));
                }
            }
            x.end();
            m_back.set(c, pm);
        }
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // rastro de cada carro: o caminho que acabou de fazer, do apagado ao aceso
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.setBrush(Qt::NoBrush);
        for (const Car& car : m_cars) {
            const bool head = car.dir > 0;
            const QColor col = hex(head ? "#fff2dc" : "#ff3a2a");
            QPolygonF pts;
            for (int i = 0; i <= 10; ++i) { const double t = car.t - car.dir * 0.09 * (i / 10.0); if (t < 0 || t > 1) break; pts << at(car.k, t, car.off); }
            if (pts.size() < 2) continue;
            QLinearGradient g(pts.first(), pts.last());
            g.setColorAt(0, alpha(col, head ? 0.9 : 0.8));
            g.setColorAt(1, alpha(col, 0));
            QPen pn(QBrush(g), head ? 2.6 : 2.2);
            pn.setCapStyle(Qt::RoundCap);
            p.setPen(pn);
            p.drawPolyline(pts);
            QLinearGradient g2(pts.first(), pts.last());
            g2.setColorAt(0, alpha(col, (head ? 0.9 : 0.8) * 0.25));
            g2.setColorAt(1, alpha(col, 0));
            QPen wide(QBrush(g2), 8);
            wide.setCapStyle(Qt::RoundCap);
            p.setPen(wide);
            p.drawPolyline(pts);
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    struct Car { int k, dir; double t, v, off; };
    QPointF at(int k, double t, double off) const
    {
        const auto& P = m_lanes[size_t(k)];
        const int n = int(P.size()) - 1;
        const int i = qBound(0, int(t * n), n - 1);
        const double f = t * n - i;
        const QPointF a = P[size_t(i)], b = P[size_t(i + 1)];
        const double dx = b.x() - a.x(), dy = b.y() - a.y(), l = std::hypot(dx, dy) > 0 ? std::hypot(dx, dy) : 1;
        return QPointF(a.x() + dx * f - dy / l * off, a.y() + dy * f + dx / l * off);
    }
    static Car newCar(int k, bool any)
    {
        const int dir = rnd() < 0.5 ? 1 : -1;
        return { k, dir, any ? rnd() : (dir > 0 ? 0.0 : 1.0), R(0.05, 0.09) * (k == 2 ? 0.8 : 1), dir > 0 ? R(4, 9) : -R(4, 9) };
    }
    Cache m_back;
    std::vector<std::vector<QPointF>> m_lanes;
    std::vector<Car> m_cars;
};

// ============================================================ pista de pouso

class Runway : public Layer {
public:
    void init(Ctx&) override { m_back.reset(); m_plane = -1; m_next = 1.5; }
    void step(double dt, Ctx&) override
    {
        m_t += dt;
        if (m_plane < 0) { m_next -= dt; if (m_next <= 0) m_plane = 0; }
        else { m_plane += dt; if (m_plane > 22) { m_plane = -1; m_next = R(8, 16); } }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double VX = zx(c, 230), VY = 400, FV = 300;
        auto P3 = [&](double X, double Y, double z) { return QPointF(VX + X / z * FV, VY + Y / z * FV); };
        if (!m_back.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing, true);
            vfill(x, QRectF(0, 0, c.W, VY), mix(pal.app, hex("#04060e"), 0.45), mix(pal.app, hex("#2a2440"), 0.35), 0, VY);
            x.drawPixmap(0, 0, starLayer(c, 120, VY * 0.7, 0.5));
            QLinearGradient g(0, VY - 60, 0, VY);
            g.setColorAt(0, alpha(hex("#ff9a5a"), 0));
            g.setColorAt(1, alpha(hex("#ff9a5a"), 0.25));
            x.fillRect(QRectF(0, VY - 60, c.W, 60), g);
            for (int i = 0, n = int(400 * c.W / 1440); i < n; ++i) x.fillRect(QRectF(R(0, c.W), VY - R(0, 10), 1.5, 1.5), alpha(hex("#ffd28a"), 0.7));
            vfill(x, QRectF(0, VY, c.W, c.H - VY), mix(pal.app, hex("#0a0c12"), 0.5), mix(pal.app, Qt::black, 0.5), VY, c.H);
            QPolygonF rw;
            rw << P3(-1.2, 1, 2) << P3(1.2, 1, 2) << P3(1.2, 1, 60) << P3(-1.2, 1, 60);
            x.setPen(Qt::NoPen);
            x.setBrush(mix(pal.app, hex("#14161c"), 0.4));
            x.drawPolygon(rw);
            // torre de controle
            const QPointF t = P3(-5, 1, 5);
            const QColor dk = mix(pal.app, Qt::black, 0.5);
            x.fillRect(QRectF(t.x() - 9, t.y() - 190, 18, 190), dk);
            x.fillRect(QRectF(t.x() - 24, t.y() - 222, 48, 10), dk);
            x.fillRect(QRectF(t.x() - 18, t.y() - 236, 36, 6), dk);
            x.fillRect(QRectF(t.x() - 20, t.y() - 212, 40, 20), alpha(hex("#9fe0c8"), 0.55));
            x.end();
            m_back.set(c, pm);
        }
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        auto dot = [&](double X, double Y, double z, const char* col, double r, double a = 1) {
            const QPointF q = P3(X, Y, z);
            const double rr = qMax(0.9, r * 1.6 / std::sqrt(z));
            disc(p, q.x(), q.y(), rr, alpha(hex(col), a));
            if (rr > 1.2) glow(p, q.x(), q.y(), rr * 5, hex(col), 0.25 * a);
        };
        for (double z = 2; z < 60; z *= 1.07) { dot(-1.3, 1, z, "#fff6e0", 2.2); dot(1.3, 1, z, "#fff6e0", 2.2); }
        for (double z = 2.3; z < 60; z *= 1.1) dot(0, 1, z, "#e8f0ff", 1.4, 0.8);
        for (double X = -1.3; X <= 1.31; X += 0.26) { dot(X, 1, 2, "#5aff7a", 2.4); dot(X, 1, 60, "#ff3a3a", 2); }
        for (double z = 2; z < 50; z *= 1.12) dot(-3.4, 1, z, "#4a7aff", 1.8, 0.8);
        // luzes de aproximação: o "coelho" correndo rumo à pista, duas vezes por segundo
        const double rab = std::fmod(m_t * 2, 1.0);
        for (int i = 0; i < 14; ++i) {
            const double z = lerp(0.55, 1.9, i / 13.0);
            for (double X : { -0.3, -0.15, 0.15, 0.3 }) dot(X, 1, z, "#fff6e0", 1.4, 0.55);
            if (std::abs(i / 13.0 - rab) < 0.04) dot(0, 1, z, "#ffffff", 5, 1);
        }
        // farol da torre: verde e branco
        const QPointF t = P3(-5, 1, 5);
        const double ba = std::pow(qMax(0.0, std::sin(m_t * 1.2 * M_PI)), 6);
        glow(p, t.x(), t.y() - 240, 60, hex(int(m_t * 1.2) % 2 ? "#5aff7a" : "#ffffff"), 0.8 * ba);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // o avião pousando: vem por cima, desce, toca e corre pela pista
        if (m_plane >= 0) {
            const double tt = m_plane;
            double z, Y;
            if (tt < 12) { const double k = tt / 12; z = lerp(0.9, 6, k); Y = lerp(-0.85, 0.94, std::pow(k, 1.1)); }
            else { const double k = (tt - 12) / 10; z = 6 + 30 * (1 - std::pow(1 - k, 2)); Y = 0.94; }
            const QPointF pp = P3(0.1, Y, z);
            const double sc = 1 / z * FV / 300, span = 360 * sc, a = qMin(1.0, (22 - tt) / 3);
            const double px = pp.x(), py = pp.y();
            p.setOpacity(a);
            const QColor pc = hex("#0a0b10");
            QPolygonF wing;
            wing << QPointF(px - span / 2, py + span * 0.02) << QPointF(px, py - span * 0.03) << QPointF(px + span / 2, py + span * 0.02) << QPointF(px, py + span * 0.035);
            p.setPen(Qt::NoPen);
            p.setBrush(pc);
            p.drawPolygon(wing);
            p.drawEllipse(QPointF(px, py), span * 0.045, span * 0.045);
            p.fillRect(QRectF(px - span * 0.008, py - span * 0.16, span * 0.016, span * 0.16), pc);
            p.fillRect(QRectF(px - span * 0.12, py - span * 0.03, span * 0.24, span * 0.012), pc);
            for (double e : { -0.18, 0.18 }) p.drawEllipse(QPointF(px + e * span, py + span * 0.045), span * 0.022, span * 0.022);
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            const struct { double ox; const char* col; double r; bool blink; } L[] = { { -0.12, "#ffffff", 9, false }, { 0.12, "#ffffff", 9, false }, { -0.5, "#ff3030", 3, false }, { 0.5, "#30ff60", 3, false }, { 0, "#ff3030", 3, true } };
            for (const auto& l : L) {
                if (l.blink && int(m_t * 1.3) % 2) continue;
                const double x = px + l.ox * span, y = py + span * 0.02, rr = qMax(1.0, l.r * sc);
                disc(p, x, y, qMax(1.0, rr * 0.4), hex(l.col));
                glow(p, x, y, rr * 6, hex(l.col), 0.6);
            }
            if (std::fmod(m_t * 1.1, 1.0) < 0.06) for (double ox : { -0.5, 0.5 }) glow(p, px + ox * span, py, 24 * sc, Qt::white, 1);
            if (tt > 6) { const QPointF g = P3(0.1, 1, z + 2.5); glow(p, g.x(), g.y(), 80 * sc * 3, hex("#fff6e0"), 0.25); }
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.setOpacity(1);
        }
    }

private:
    Cache m_back;
    double m_t = 0, m_plane = -1, m_next = 2;
};

// ============================================================ Arcos da Lapa

class Lapa : public Layer {
public:
    void init(Ctx& c) override
    {
        m_back.reset();
        m_tram = -1e9;
        m_next = 2;
        m_strings.clear();
        static const char* BC[] = { "#ffd28a", "#ffc46b", "#ffe6b8", "#ff8a5a" };
        const double strings[3][3] = { { zx(c, -20), zx(c, 230), 560 }, { zx(c, 210), zx(c, 470), 600 }, { rx(c, 1060), rx(c, 1460), 580 } };
        for (const auto& s : strings) {
            Str st { s[0], s[1], s[2], {} };
            for (int i = 0; i <= 14; ++i) st.b.push_back({ i / 14.0, R(0, 6.28), double(hex(BC[RI(4)]).rgb()) });
            m_strings.push_back(st);
        }
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        if (m_tram < -1e8) { m_next -= dt; if (m_next <= 0) m_tram = -110; }
        else { m_tram += 42 * dt; if (m_tram > c.W + 20) { m_tram = -1e9; m_next = R(10, 20); } }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double DECK = c.H * 0.36, BASE = c.H - 30, MID = c.H * 0.6;
        if (!m_back.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing, true);
            sky(x, c, mix(pal.app, hex("#0c0820"), 0.45), mix(pal.app, hex("#4a2240"), 0.4));
            x.drawPixmap(0, 0, starLayer(c, 90, c.H * 0.4, 0.4));
            // morro de Santa Teresa com luzinhas
            const Profile f = profile(7, 0.5);
            QPainterPath hill(QPointF(-10, c.H));
            for (double xx = -10; xx <= c.W + 10; xx += 6) hill.lineTo(xx, c.H * 0.3 + f.at(xx / c.W) * 50 + (xx / c.W) * 60);
            hill.lineTo(c.W + 10, c.H);
            hill.closeSubpath();
            x.fillPath(hill, mix(pal.app, hex("#120c18"), 0.5));
            static const char* LC[] = { "#ffd28a", "#ffc46b", "#f4f8ff" };
            for (int i = 0, n = int(700 * c.W / 1440); i < n; ++i) { const double xx = R(0, c.W), y0 = c.H * 0.3 + f.at(xx / c.W) * 50 + (xx / c.W) * 60; x.fillRect(QRectF(xx, R(y0 + 4, c.H * 0.75), 1.4, 1.4), alpha(hex(LC[RI(3)]), R(0.3, 0.8))); }
            // a catedral cônica lá atrás, à direita
            const double cx = rx(c, 1270), cb = c.H * 0.62;
            QPolygonF cat;
            cat << QPointF(cx - 95, cb) << QPointF(cx - 55, cb - 230) << QPointF(cx + 55, cb - 230) << QPointF(cx + 95, cb);
            x.setPen(Qt::NoPen);
            x.setBrush(mix(pal.app, hex("#0a0810"), 0.55));
            x.drawPolygon(cat);
            x.setPen(QPen(alpha(hex("#ffd28a"), 0.12), 1));
            for (int k = -4; k <= 4; ++k) x.drawLine(QPointF(cx + k * 20, cb), QPointF(cx + k * 11, cb - 230));
            // os arcos: parede inteira, depois os vãos recortados
            const QColor stone = mix(hex("#efe6d4"), pal.app, 0.3);
            QImage arc(pm.size(), QImage::Format_ARGB32_Premultiplied);
            arc.setDevicePixelRatio(c.px);
            arc.fill(Qt::transparent);
            QPainter a(&arc);
            a.setRenderHint(QPainter::Antialiasing, true);
            QLinearGradient sg(0, DECK, 0, BASE);
            sg.setColorAt(0, mix(stone, Qt::black, 0.25));
            sg.setColorAt(1, stone);
            a.fillRect(QRectF(0, DECK, c.W, BASE - DECK), sg);
            a.fillRect(QRectF(0, DECK - 8, c.W, 10), mix(stone, Qt::black, 0.15));
            a.setCompositionMode(QPainter::CompositionMode_Clear);
            auto hole = [&](double ax, double w, double top, double bottom) {
                QPainterPath h(QPointF(ax - w / 2, bottom));
                h.lineTo(ax - w / 2, top + w / 2);
                h.arcTo(QRectF(ax - w / 2, top, w, w), 180, -180);
                h.lineTo(ax + w / 2, bottom);
                h.closeSubpath();
                a.fillPath(h, Qt::transparent);
            };
            for (double ax = -20; ax < c.W + 60; ax += 74) hole(ax, 48, MID + 10, BASE + 2);
            for (double ax = -20; ax < c.W + 60; ax += 37) hole(ax, 22, DECK + 22, MID - 4);
            a.end();
            x.drawImage(QRectF(0, 0, c.W, c.H), arc);
            // trilho e fio do bonde lá em cima
            x.setPen(QPen(mix(pal.app, Qt::black, 0.5), 1));
            x.drawLine(QPointF(0, DECK - 50), QPointF(c.W, DECK - 50));
            for (double px = 30; px < c.W; px += 150) x.fillRect(QRectF(px, DECK - 56, 3, 48), mix(pal.app, Qt::black, 0.5));
            x.fillRect(QRectF(0, BASE, c.W, c.H - BASE), mix(pal.app, hex("#0a0608"), 0.6));
            x.end();
            m_back.set(c, pm);
        }
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // holofotes acendendo os arcos por baixo
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (double ax = -20; ax < c.W + 60; ax += 74) {
            QRadialGradient g(QPointF(ax + 37, BASE), 160);
            g.setColorAt(0, alpha(hex("#ffcf8a"), 0.22));
            g.setColorAt(1, alpha(hex("#ffcf8a"), 0));
            p.fillRect(QRectF(ax - 123, BASE - 160, 320, 170), g);
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // o bondinho amarelo atravessando lá em cima
        if (m_tram > -1e8) {
            const double x = m_tram, y = DECK - 10;
            p.fillRect(QRectF(x, y - 38, 100, 34), hex("#e8b81a"));
            p.fillRect(QRectF(x, y - 6, 100, 4), hex("#2a7a3a"));
            p.fillRect(QRectF(x - 3, y - 42, 106, 5), hex("#7a5a10"));
            for (int i = 0; i < 6; ++i) {
                p.fillRect(QRectF(x + 6 + i * 16, y - 32, 11, 13), alpha(hex("#fff0c0"), 0.9));
                Fig f; f.pose = Fig::Bust; f.hair = hairAt(i * 3 + 1); f.col = hex("#1a1208");
                p.save();
                p.setClipRect(QRectF(x + 6 + i * 16, y - 32, 11, 13), Qt::IntersectClip);
                figure(p, x + 11.5 + i * 16, y - 18, 22, f);
                p.restore();
            }
            p.setPen(QPen(hex("#1a1208"), 1.5));
            p.drawLine(QPointF(x + 50, y - 42), QPointF(x + 30, DECK - 50));
            for (double wx : { 14.0, 86.0 }) disc(p, x + wx, y - 2, 5, hex("#1a1208"));
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            glow(p, x + 100, y - 20, 50, hex("#fff2c0"), 0.5);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        // varal de lampadinhas de boteco, balançando
        for (const Str& s : m_strings) {
            const double sag = 34 + std::sin(m_t * 0.9 + s.x0) * 4;
            auto py = [&](double u) { return s.y0 + sag * 4 * u * (1 - u); };
            QPolygonF wire;
            for (double u = 0; u <= 1.001; u += 0.05) wire << QPointF(lerp(s.x0, s.x1, u), py(u));
            p.setPen(QPen(alpha(Qt::black, 0.8), 1));
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(wire);
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            for (const auto& b : s.b) {
                const double bx = lerp(s.x0, s.x1, b[0]), by = py(b[0]) + 6, a = 0.8 + 0.2 * std::sin(m_t * 3 + b[1]);
                glow(p, bx, by, 18, QColor::fromRgb(QRgb(b[2])), 0.5 * a);
                disc(p, bx, by, 2.4, alpha(hex("#fff4d8"), a));
            }
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
    }

private:
    struct Str { double x0, x1, y0; std::vector<std::array<double, 3>> b; };
    Cache m_back;
    std::vector<Str> m_strings;
    double m_t = 0, m_tram = -1e9, m_next = 3;
};

// ============================================================ roda-gigante no píer

class Ferris : public Layer {
public:
    void init(Ctx&) override { m_back.reset(); }
    void step(double dt, Ctx& c) override { m_t += dt; m_ang += dt * 0.09 * (0.7 + 0.3 * INT(c)); }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double cx = zx(c, 230), CY = 300, Rr = 178, WY = c.H * 0.74;
        if (!m_back.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing, true);
            vfill(x, QRectF(0, 0, c.W, WY), mix(pal.app, hex("#060a1c"), 0.45), mix(pal.app, hex("#2a2048"), 0.35), 0, WY);
            x.drawPixmap(0, 0, starLayer(c, 160, WY * 0.7, 0.5));
            const QColor dk = mix(pal.app, Qt::black, 0.3);
            for (double bx = cx + 250; bx < c.W;) { const double bw = R(20, 50), bh = R(16, 70); x.fillRect(QRectF(bx, WY - bh, bw, bh), dk); bx += bw + R(0, 6); }
            for (int i = 0, n = int(260 * c.W / 1440); i < n; ++i) x.fillRect(QRectF(R(cx + 250, c.W), WY - R(2, 60), 1.5, 1.5), alpha(hex("#ffd28a"), 0.7));
            vfill(x, QRectF(0, WY, c.W, c.H - WY), mix(pal.app, hex("#0c1428"), 0.45), mix(pal.app, hex("#03050c"), 0.5), WY, c.H);
            const QColor pier = mix(pal.app, Qt::black, 0.55);
            x.fillRect(QRectF(0, WY - 14, cx + 240, 14), pier);
            for (double px = 10; px < cx + 240; px += 40) x.fillRect(QRectF(px, WY, 6, 60), pier);
            x.setPen(QPen(pier, 6));
            x.drawLine(QPointF(cx - 110, WY - 14), QPointF(cx, CY));
            x.drawLine(QPointF(cx, CY), QPointF(cx + 110, WY - 14));
            x.setPen(QPen(pier, 2));
            x.drawLine(QPointF(cx - 70, WY - 80), QPointF(cx + 70, WY - 80));
            x.end();
            m_back.set(c, pm);
        }
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        const int mode = int(m_t / 9) % 4;
        auto bulb = [&](int i, int n, bool spoke) -> QColor {
            static const char* H[] = { "#ff4a5a", "#ffb648", "#ffe66b", "#5aff9a", "#4ad8ff", "#b06bff" };
            if (mode == 0) return hex(H[int(std::fmod(double(i) / n * 6 + m_t * 0.8, 6.0))]);
            if (mode == 1) return hex((i + int(m_t * 3)) % 2 ? "#ffe6b8" : "#ff5a8a");
            if (mode == 2) return mix(hex("#4ad8ff"), Qt::white, 0.5 + 0.5 * std::sin(m_t * 3 - (spoke ? i * 0.6 : i * 0.25)));
            return hex("#ffd28a");
        };
        for (bool mirror : { true, false }) {
            auto Y = [&](double y) { return mirror ? 2 * WY - y : y; };
            auto wv = [&](double y) { return mirror ? std::sin(y * 0.09 + m_t * 2.4) * (3 + (y - WY) * 0.02) : 0.0; };
            p.setOpacity(mirror ? 0.3 : 1);
            QPolygonF rim;
            for (int i = 0; i <= 64; ++i) { const double a = i / 64.0 * 2 * M_PI, y = Y(CY + std::sin(a) * Rr); rim << QPointF(cx + std::cos(a) * Rr + wv(y), y); }
            p.setPen(QPen(mix(pal.app, hex("#9aa0b8"), 0.35), 2));
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(rim);
            QVector<QLineF> spokes;
            for (int i = 0; i < N; ++i) { const double a = m_ang + i / double(N) * 2 * M_PI, y = Y(CY + std::sin(a) * Rr); spokes.append(QLineF(cx, Y(CY), cx + std::cos(a) * Rr + wv(y), y)); }
            p.setPen(QPen(mix(pal.app, hex("#9aa0b8"), 0.35), 1.2));
            p.drawLines(spokes);
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            for (int i = 0; i < 64; ++i) {
                const double a = m_ang + i / 64.0 * 2 * M_PI, x = cx + std::cos(a) * Rr, y = CY + std::sin(a) * Rr, yy = Y(y);
                const QColor col = bulb(i, 64, false);
                disc(p, x + wv(yy), yy, 2.2, alpha(col, mirror ? 0.4 : 1));
                if (!mirror) glow(p, x, y, 10, col, 0.35);
            }
            for (int i = 0; i < N; ++i) {
                const double a = m_ang + i / double(N) * 2 * M_PI;
                for (int k = 1; k < 7; ++k) { const double r = Rr * k / 7, x = cx + std::cos(a) * r, yy = Y(CY + std::sin(a) * r); disc(p, x + wv(yy), yy, 1.5, alpha(bulb(k, 7, true), mirror ? 0.3 : 0.85)); }
            }
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.setOpacity(1);
            if (mirror) continue;
            // cabines penduradas, sempre em pé
            static const char* GC[] = { "#d84a3a", "#2a7ad8", "#e8c23a", "#3aa85a" };
            for (int i = 0; i < N; ++i) {
                const double a = m_ang + i / double(N) * 2 * M_PI, x = cx + std::cos(a) * Rr, y = CY + std::sin(a) * Rr, sw = std::sin(m_t * 1.6 + i) * 0.05;
                p.save();
                p.translate(x, y);
                p.rotate(sw * 180 / M_PI);
                p.setPen(QPen(hex("#0a0a10"), 1.5));
                p.drawLine(QPointF(0, 0), QPointF(0, 8));
                QPainterPath cab(QPointF(-10, 8));
                cab.lineTo(10, 8); cab.lineTo(12, 24); cab.quadTo(0, 28, -12, 24); cab.closeSubpath();
                p.fillPath(cab, mix(hex(GC[i % 4]), pal.app, 0.35));
                p.fillRect(QRectF(-7, 12, 14, 6), alpha(hex("#ffe6b8"), 0.8));
                p.restore();
            }
            disc(p, cx, CY, 7, hex("#d8d8e0"));
        }
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (double px = 30; px < cx + 240; px += 90) { glow(p, px, WY - 30, 40, hex("#ffd28a"), 0.4); p.fillRect(QRectF(px - 2, WY - 32, 4, 4), hex("#fff0c8")); }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    static constexpr int N = 16;
    Cache m_back;
    double m_t = 0, m_ang = 0;
};

// ============================================================ Kowloon: prédios colados e o avião passando rente

class Kowloon : public Layer {
public:
    void init(Ctx&) override { m_back.reset(); m_jet = -1e9; m_next = 2; }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        if (m_jet < -1e8) { m_next -= dt; if (m_next <= 0) { m_jet = c.W + 500; m_jy = R(70, 130); m_jv = R(170, 210); } }
        else { m_jet -= m_jv * dt; m_jy += 9 * dt; if (m_jet < -700) { m_jet = -1e9; m_next = R(14, 24); } }
        for (Sign& s : m_signs) { if (s.glitch > 0) s.glitch -= dt; else if (rnd() < dt * 0.05) s.glitch = R(0.1, 0.5); }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        if (!m_back.valid(c)) build(pal, c);
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // letreiros: brilho na parede, braços de ferro (lâmina), sombra e a imagem pronta
        const QColor dk = mix(pal.app, hex("#0a0608"), 0.5);
        for (const Sign& s : m_signs) {
            const bool old = s.type == Old, on = old || !(s.glitch > 0 && rnd() < 0.6);
            if (on && !old) {
                p.setCompositionMode(QPainter::CompositionMode_Plus);
                glow(p, s.x + s.w / 2, s.y + s.h / 2, qMax(s.w, s.h) * 1.6, s.type == Box ? s.face : s.col, s.type == Box ? 0.1 : 0.16);
                p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            }
            if (s.blade) {
                const double a0 = s.edge, a1 = s.blade > 0 ? s.x : s.x + s.w;
                p.fillRect(QRectF(qMin(a0, a1), s.y + 7, std::abs(a1 - a0), 2.5), dk);
                p.fillRect(QRectF(qMin(a0, a1), s.y + s.h - 10, std::abs(a1 - a0), 2.5), dk);
                p.setPen(QPen(dk, 1.5));
                p.drawLine(QPointF(a0, s.y + 22), QPointF(a1, s.y + 9));
                p.setPen(Qt::NoPen);
                p.fillRect(QRectF(s.x + 5, s.y + 8, s.w, s.h), alpha(Qt::black, 0.4));
            } else {
                p.fillRect(QRectF(s.x + 2, s.y + 4, s.w + 4, s.h + 4), alpha(Qt::black, 0.4));
            }
            p.drawImage(QRectF(s.x - s.pad, s.y - s.pad, s.w + s.pad * 2, s.h + s.pad * 2), on ? s.on : s.off);
        }
        // o jato descendo pro aeroporto, rente aos telhados
        if (m_jet > -1e8) {
            const double x = m_jet, y = m_jy, L = 470;
            const QColor dk = mix(pal.app, hex("#0a0a10"), 0.55);
            QPainterPath body(QPointF(x, y));
            body.quadTo(x + 10, y - 22, x + 60, y - 24);
            body.lineTo(x + L - 40, y - 22); body.lineTo(x + L, y - 70); body.lineTo(x + L + 22, y - 70); body.lineTo(x + L + 10, y - 16);
            body.lineTo(x + L - 20, y + 6); body.lineTo(x + 40, y + 8);
            body.quadTo(x + 6, y + 8, x, y);
            p.fillPath(body, dk);
            QLinearGradient bg(0, y - 4, 0, y + 8);
            bg.setColorAt(0, alpha(hex("#ff9a5a"), 0));
            bg.setColorAt(1, alpha(hex("#ff9a5a"), 0.35));
            p.fillRect(QRectF(x + 30, y - 4, L - 50, 12), bg);
            QPolygonF wing;
            wing << QPointF(x + 170, y) << QPointF(x + 280, y + 46) << QPointF(x + 310, y + 46) << QPointF(x + 260, y - 4);
            p.setPen(Qt::NoPen);
            p.setBrush(dk);
            p.drawPolygon(wing);
            p.save();
            p.translate(x + 232, y + 26);
            p.rotate(0.35 * 180 / M_PI);
            p.drawEllipse(QPointF(0, 0), 24, 9);
            p.restore();
            p.fillRect(QRectF(x + 90, y + 6, 3, 22), dk);
            p.fillRect(QRectF(x + 250, y + 6, 4, 26), dk);
            QVector<QRectF> ws;
            for (double wx = x + 70; wx < x + L - 70; wx += 9) ws.append(QRectF(wx, y - 16, 4, 3));
            p.setBrush(alpha(hex("#ffe6b8"), 0.85));
            p.drawRects(ws);
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            glow(p, x + 92, y + 28, 70, Qt::white, 0.8);
            glow(p, x + 252, y + 32, 60, Qt::white, 0.8);
            glow(p, x + 300, y + 46, 14, hex("#ff3030"), 0.8);
            glow(p, x + L + 16, y - 60, 10, Qt::white, 0.8);
            if (int(m_t * 1.3) % 2 == 0) glow(p, x + 200, y - 26, 16, hex("#ff3030"), 0.9);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
    }

private:
    struct Lay { double t0, t1, w0, w1, dark, win, gx, gy, litA, lit; };
    struct Bld { double x, w, top; QColor wall; double lit; int jx, jy; };

    // antenas de TV no telhado: mastro e travessas
    static void antennas(QPainter& x, double bx, double bw, double top, const QColor& col, int n, int k)
    {
        QPainterPath a;
        for (int i = 0; i < n; ++i) {
            const double ax = std::round(bx + R(4, bw - 4)) + 0.5, h = R(8, 26 + k * 10);
            a.moveTo(ax, top);
            a.lineTo(ax, top - h);
            const int bars = 1 + RI(3);
            for (int j = 0; j < bars; ++j) {
                const double yy = top - h + 2 + j * 4, hw = R(3, 7 + k * 2);
                a.moveTo(ax - hw, yy);
                a.lineTo(ax + hw, yy);
            }
        }
        x.setBrush(Qt::NoBrush);
        x.setPen(QPen(col, k ? 1.2 : 0.8));
        x.drawPath(a);
        x.setPen(Qt::NoPen);
    }

    void building(QPainter& x, const Bld& b, int k, const Pal& pal, const Ctx& c)
    {
        static const char* WINC[6] = { "#ffd28a", "#ffd28a", "#fff4e0", "#cfe8d8", "#e8f0ff", "#ffb87a" };
        static const char* CLOTH[6] = { "#d84a3a", "#3a7ad8", "#e8e0d0", "#e8c23a", "#5ab08a", "#e88aa8" };
        static const char* PATCH[4] = { "#6a5a48", "#4a5a50", "#6a4a44", "#5a5040" };
        const Lay& L = LAY[k];
        const double H = c.H, bx = b.x, bw = b.w, top = b.top, cx = bx + bw / 2, VX = c.W / 2;
        const int side = cx < VX ? 1 : -1;
        const QColor base = mix(mix(pal.app, b.wall, 0.45 + 0.12 * k), Qt::black, qMax(0.0, L.dark + R(-0.08, 0.1)));
        x.setPen(Qt::NoPen);
        // parede lateral virada pro meio: mais escura, com a linha do telhado fugindo pro horizonte
        const double sw = qMin(10.0 + 12 * k, std::abs(cx - VX) * (0.014 + 0.01 * k));
        if (sw > 2) {
            const double ex = side > 0 ? bx + bw : bx, ox = ex + side * sw, drop = sw * 0.4;
            QLinearGradient sg(0, top, 0, H);
            sg.setColorAt(0, mix(base, Qt::black, 0.35));
            sg.setColorAt(1, mix(base, Qt::black, 0.7));
            QPolygonF sp;
            sp << QPointF(ex, top) << QPointF(ox, top + drop) << QPointF(ox, H) << QPointF(ex, H);
            x.setBrush(sg);
            x.drawPolygon(sp);
            // janelinhas da lateral, em perspectiva (estreitas)
            for (double wy = top + 10; wy < H; wy += L.gy)
                for (double u = 0.2; u < 0.8; u += 0.45) {
                    if (rnd() > 0.3) continue;
                    const double x0 = ex + side * sw * u, x1 = ex + side * sw * (u + 0.25), y0 = wy + drop * u, y1 = wy + drop * (u + 0.25);
                    QPolygonF wp;
                    wp << QPointF(x0, y0) << QPointF(x1, y1) << QPointF(x1, y1 + L.win + 1) << QPointF(x0, y0 + L.win + 1);
                    x.setBrush(alpha(hex(WINC[RI(6)]), L.litA * 0.55));
                    x.drawPolygon(wp);
                }
            // quina: a aresta entre a frente e a lateral pega um fio de luz
            x.fillRect(QRectF(side > 0 ? ex - 1 : ex, top, 1, H - top), alpha(hex("#ffc89a"), 0.08 + 0.04 * k));
        }
        // fachada: topo pegando o brilho do céu, pé afundando no escuro da rua
        QLinearGradient fg(0, top, 0, H);
        fg.setColorAt(0, mix(base, hex("#ffb07a"), 0.1));
        fg.setColorAt(0.2, base);
        fg.setColorAt(1, mix(base, Qt::black, 0.5));
        x.fillRect(QRectF(bx, top, bw, H - top), fg);
        // reboco remendado: um pedaço pintado de outra cor
        if (rnd() < 0.45) {
            const double py = top + R(20, (H - top) * 0.5), ph = R(60, 220), px = bx + (rnd() < 0.5 ? 0 : bw * R(0.3, 0.6));
            x.fillRect(QRectF(px, py, bw * R(0.3, 0.7), ph), alpha(hex(PATCH[RI(4)]), 0.18));
        }
        // lajes: uma linha escura por andar
        {
            QVector<QRectF> slabs;
            for (double wy = top + L.gy + b.jy - 3; wy < H; wy += L.gy + b.jy) slabs.append(QRectF(bx, wy, bw, 1));
            x.setBrush(alpha(Qt::black, 0.16 + 0.06 * k));
            x.drawRects(slabs);
        }
        // escorridos de água no concreto
        for (int i = 0, n = 2 + RI(5); i < n; ++i) {
            const double sx = bx + R(2, bw - 6), sy = top + R(4, (H - top) * 0.7), sl = R(40, 220), w = R(2, 7);
            vfill(x, QRectF(sx, sy, w, sl), alpha(Qt::black, 0.2), alpha(Qt::black, 0), sy, sy + sl);
        }
        // janelas: acesas (com a verga escura em cima), gaiolas de ferro, ar-condicionado, vidro escuro.
        // Cada prédio acende do seu jeito: uns quase apagados, outros lotados, e a grade varia um pouco.
        const double ww = L.win, wh = L.win + 1 + k, lit = b.lit, gx = L.gx + b.jx, gy = L.gy + b.jy;
        const QColor cage = mix(base, hex("#0a0808"), 0.5), acBody = mix(base, hex("#9aa0a8"), 0.35);
        for (double wy = top + 6; wy < H - 4; wy += gy)
            for (double wx = bx + 4; wx < bx + bw - ww - 3; wx += gx) {
                const double r = rnd();
                if (r < lit) {
                    const QColor wc = hex(WINC[RI(6)]);
                    x.fillRect(QRectF(wx, wy, ww, wh), alpha(wc, L.litA));
                    if (k && rnd() < 0.3) x.fillRect(QRectF(wx, wy, ww * R(0.3, 0.6), wh), alpha(mix(wc, Qt::black, 0.5), 0.6));   // cortina
                    x.fillRect(QRectF(wx, wy, ww, 1), alpha(Qt::black, 0.35));
                } else if (k && r < lit + 0.1) {
                    // gaiola de ferro saindo da janela (o clássico de Kowloon), com sombra embaixo
                    const double gx0 = wx - 2, gw = ww + 4, gh = wh + 2;
                    if (rnd() < 0.5) x.fillRect(QRectF(wx, wy, ww, wh), alpha(hex(WINC[RI(6)]), L.litA * 0.7));
                    x.fillRect(QRectF(gx0 + 1, wy + gh, gw, 2 + k), alpha(Qt::black, 0.35));
                    x.setBrush(Qt::NoBrush);
                    x.setPen(QPen(cage, 1));
                    x.drawRect(QRectF(gx0 + 0.5, wy - 1.5, gw - 1, gh));
                    for (int i = 1; i < 3; ++i) {
                        const double vx = std::round(gx0 + i * gw / 3) + 0.5;
                        x.drawLine(QPointF(vx, wy - 1), QPointF(vx, wy + gh - 1));
                    }
                    x.setPen(Qt::NoPen);
                    x.fillRect(QRectF(gx0, wy - 2, gw, 1), alpha(hex("#ffc89a"), 0.12));
                } else if (k && r < lit + 0.17) {
                    // ar-condicionado com sombra e o pingo escorrendo
                    const double aw = ww + 3, ah = qMax(4.0, wh * 0.7);
                    x.fillRect(QRectF(wx, wy + ah + 2, 1, R(8, 30)), alpha(Qt::black, 0.18));
                    x.fillRect(QRectF(wx - 1, wy + ah, aw, 2), alpha(Qt::black, 0.35));
                    x.fillRect(QRectF(wx - 2, wy, aw, ah), acBody);
                    x.fillRect(QRectF(wx - 2, wy, aw, 1), alpha(Qt::white, 0.12));
                    x.fillRect(QRectF(wx, wy + 2, aw - 4, 1), alpha(Qt::black, 0.25));
                } else {
                    x.fillRect(QRectF(wx, wy, ww, wh), alpha(Qt::black, 0.22));
                    if (rnd() < 0.25) x.fillRect(QRectF(wx, wy, ww, 1), alpha(hex("#ffd8b0"), 0.07));
                }
            }
        // canto escuro onde um prédio encosta no outro
        for (int e = 0; e < 2; ++e) {
            const double ex = e ? bx + bw : bx, dir = e ? -1 : 1, aw = 10 + 6 * k;
            QLinearGradient g(ex, 0, ex + dir * aw, 0);
            g.setColorAt(0, alpha(Qt::black, 0.35 + 0.1 * k));
            g.setColorAt(1, alpha(Qt::black, 0));
            x.fillRect(QRectF(qMin(ex, ex + dir * aw), top, aw, H - top), g);
        }
        // platibanda pegando luz e o entulho do telhado: caixa-d'água, puxadinho, antenas
        x.fillRect(QRectF(bx, top, bw, 1.5), mix(base, hex("#ffc89a"), 0.22));
        x.fillRect(QRectF(bx - 1, top - 3, bw + 2, 3), mix(base, Qt::black, 0.15));
        const QColor roof = mix(base, Qt::black, 0.25);
        if (rnd() < 0.5) {
            const double tw = R(8, 16) + k * 2, th = R(7, 12) + k * 2, tx = bx + R(4, bw - tw - 4);
            x.fillRect(QRectF(tx, top - 3 - th, tw, th), roof);
            x.fillRect(QRectF(tx + 2, top - 3, 2, 3), roof);
            x.fillRect(QRectF(tx + tw - 4, top - 3, 2, 3), roof);
            x.fillRect(QRectF(tx, top - 3 - th, tw, 1), alpha(hex("#ffc89a"), 0.1));
        }
        if (rnd() < 0.35) {
            const double sw2 = R(16, 34), sh = R(8, 16), sx = bx + R(2, bw - sw2 - 2);
            x.fillRect(QRectF(sx, top - 3 - sh, sw2, sh), roof);
            if (rnd() < 0.6) x.fillRect(QRectF(sx + R(3, sw2 - 8), top - 3 - sh + 3, 4, sh - 5), alpha(hex(WINC[RI(6)]), L.litA * 0.8));
        }
        antennas(x, bx, bw, top - 3, mix(base, Qt::black, 0.3), 2 + RI(4 + k * 2), k);
        // varais de bambu com roupa (só os de perto)
        if (k == 2)
            for (int i = 0; i < 4; ++i) {
                const double py = top + R(30, H - top - 60), px = rnd() < 0.5 ? bx : bx + bw, dir = px == bx ? -1 : 1;
                x.setPen(QPen(hex("#1a1410"), 1.2));
                x.drawLine(QPointF(px, py), QPointF(px + dir * 28, py - 4));
                x.setPen(Qt::NoPen);
                for (int j = 0; j < 3; ++j) {
                    const double cw = 5, ch = R(7, 11), cx2 = px + (dir < 0 ? -26 : 4) + j * 8;
                    x.fillRect(QRectF(cx2 + 2, py - 1, cw, ch), alpha(Qt::black, 0.3));
                    x.fillRect(QRectF(cx2, py - 2, cw, ch), mix(hex(CLOTH[RI(6)]), base, 0.45));
                }
            }
    }

    // ---------------- letreiros: comércio de verdade, com arte, espessura e desgaste ----------------
    // cada comércio: o nome, o desenho que combina e a cor do neon
    struct Biz { const char* txt; int art; const char* col; };
    enum Art { NoArt = -1, Cup, Tile, Cross, Ingot, Bat, Fish, Glass, Lotus };
    enum SignType { Neon, Box, Old };
    struct Sign {
        double x = 0, y = 0, w = 0, h = 0, fs = 0, artS = 0, edge = 0, pad = 26, glitch = 0;
        int type = Neon, art = NoArt, blade = 0, side = 1;
        bool vert = false, border = false;
        QString txt;
        QColor col, col2, face, ink;
        std::vector<std::array<double, 5>> rust;
        QImage on, off;
    };
    static const Biz& bizAt(int i)
    {
        static const Biz B[9] = { { "茶餐廳", Cup, "#ffb03a" }, { "麻雀", Tile, "#2aff9a" }, { "藥房", Cross, "#3aff7a" }, { "金行", Ingot, "#ffd23a" },
                                  { "押", Bat, "#ff3a4a" }, { "酒家", Fish, "#ff8a2a" }, { "夜總會", Glass, "#ff5ac8" }, { "大酒樓", Fish, "#ff3a4a" }, { "旅館", Lotus, "#4ad8ff" } };
        return B[i];
    }
    // o desenho de cada comércio, numa caixa de 0 a 1
    static QPainterPath artPath(int kind)
    {
        QPainterPath q;
        switch (kind) {
        case Cup:
            q.moveTo(0.2, 0.4); q.lineTo(0.27, 0.78); q.quadTo(0.5, 0.86, 0.73, 0.78); q.lineTo(0.8, 0.4); q.closeSubpath();
            q.moveTo(0.8, 0.46); q.quadTo(0.97, 0.5, 0.79, 0.68);
            q.moveTo(0.1, 0.88); q.quadTo(0.5, 0.97, 0.9, 0.88);
            q.moveTo(0.4, 0.32); q.cubicTo(0.3, 0.22, 0.48, 0.14, 0.4, 0.04);
            q.moveTo(0.6, 0.32); q.cubicTo(0.5, 0.22, 0.68, 0.14, 0.6, 0.04);
            break;
        case Tile:
            q.addRoundedRect(QRectF(0.24, 0.08, 0.52, 0.84), 0.08, 0.08);
            q.addEllipse(QPointF(0.5, 0.32), 0.1, 0.1);
            q.addEllipse(QPointF(0.5, 0.68), 0.1, 0.1);
            break;
        case Cross: {
            static const double P[12][2] = { { 0.38, 0.1 }, { 0.62, 0.1 }, { 0.62, 0.38 }, { 0.9, 0.38 }, { 0.9, 0.62 }, { 0.62, 0.62 }, { 0.62, 0.9 }, { 0.38, 0.9 }, { 0.38, 0.62 }, { 0.1, 0.62 }, { 0.1, 0.38 }, { 0.38, 0.38 } };
            q.moveTo(P[0][0], P[0][1]);
            for (int i = 1; i < 12; ++i) q.lineTo(P[i][0], P[i][1]);
            q.closeSubpath();
            break;
        }
        case Ingot:
            q.moveTo(0.06, 0.5); q.quadTo(0.5, 0.98, 0.94, 0.5); q.quadTo(0.78, 0.6, 0.66, 0.5);
            q.arcTo(QRectF(0.34, 0.34, 0.32, 0.32), 0, 180);
            q.quadTo(0.22, 0.6, 0.06, 0.5); q.closeSubpath();
            break;
        case Bat:
            q.moveTo(0.5, 0.42); q.quadTo(0.3, 0.18, 0.04, 0.32); q.quadTo(0.14, 0.46, 0.1, 0.62); q.quadTo(0.26, 0.5, 0.34, 0.64);
            q.quadTo(0.42, 0.54, 0.5, 0.7); q.quadTo(0.58, 0.54, 0.66, 0.64); q.quadTo(0.74, 0.5, 0.9, 0.62); q.quadTo(0.86, 0.46, 0.96, 0.32); q.quadTo(0.7, 0.18, 0.5, 0.42);
            q.closeSubpath();
            q.moveTo(0.45, 0.4); q.lineTo(0.43, 0.3); q.moveTo(0.55, 0.4); q.lineTo(0.57, 0.3);
            break;
        case Fish:
            q.moveTo(0.1, 0.5); q.cubicTo(0.28, 0.18, 0.6, 0.2, 0.74, 0.5); q.cubicTo(0.6, 0.8, 0.28, 0.82, 0.1, 0.5); q.closeSubpath();
            q.moveTo(0.74, 0.5); q.lineTo(0.95, 0.3); q.lineTo(0.95, 0.7); q.closeSubpath();
            q.moveTo(0.4, 0.27); q.quadTo(0.48, 0.12, 0.58, 0.27);
            q.addEllipse(QPointF(0.27, 0.45), 0.03, 0.03);
            break;
        case Glass:
            q.moveTo(0.16, 0.16); q.lineTo(0.84, 0.16); q.lineTo(0.5, 0.56); q.closeSubpath();
            q.moveTo(0.5, 0.56); q.lineTo(0.5, 0.86); q.moveTo(0.3, 0.88); q.lineTo(0.7, 0.88);
            q.addEllipse(QPointF(0.62, 0.28), 0.05, 0.05);
            q.moveTo(0.62, 0.23); q.lineTo(0.74, 0.04);
            break;
        default:
            q.moveTo(0.5, 0.8); q.cubicTo(0.3, 0.52, 0.5, 0.14, 0.5, 0.14); q.cubicTo(0.5, 0.14, 0.7, 0.52, 0.5, 0.8);
            q.moveTo(0.5, 0.8); q.cubicTo(0.14, 0.72, 0.08, 0.42, 0.24, 0.36); q.cubicTo(0.34, 0.52, 0.44, 0.66, 0.5, 0.8);
            q.moveTo(0.5, 0.8); q.cubicTo(0.86, 0.72, 0.92, 0.42, 0.76, 0.36); q.cubicTo(0.66, 0.52, 0.56, 0.66, 0.5, 0.8);
            q.moveTo(0.14, 0.9); q.lineTo(0.86, 0.9);
        }
        return q;
    }
    // texto centrado num ponto, como contorno
    static QPainterPath glyphAt(const QFont& f, const QString& s, double cx, double cy)
    {
        const QFontMetricsF fm(f);
        QPainterPath p;
        p.addText(cx - fm.horizontalAdvance(s) / 2, cy + (fm.ascent() - fm.descent()) / 2, f, s);
        return p;
    }
    // tubo de neon: brilho largo, tubo colorido e o miolo quase branco; apagado, só o vidro escuro
    static void tube(QPainter& x, const QPainterPath& path, const QColor& col, bool on, double lw)
    {
        x.setBrush(Qt::NoBrush);
        if (on) {
            x.setPen(pen(alpha(col, 0.1), lw * 5)); x.drawPath(path);
            x.setPen(pen(alpha(col, 0.28), lw * 2.6)); x.drawPath(path);
            x.setPen(pen(mix(col, Qt::white, 0.25), lw)); x.drawPath(path);
            x.setPen(pen(alpha(Qt::white, 0.85), lw * 0.4)); x.drawPath(path);
        } else {
            x.setPen(pen(alpha(mix(col, Qt::black, 0.55), 0.85), lw)); x.drawPath(path);
            x.setPen(pen(alpha(Qt::white, 0.08), lw * 0.4)); x.drawPath(path);
        }
        x.setPen(Qt::NoPen);
    }
    // um letreiro pronto numa imagem (aceso ou apagado), com folga em volta pro brilho
    QImage signImage(const Sign& g, const Pal& pal, double px, bool on) const
    {
        const double pad = g.pad, w = g.w, h = g.h;
        QImage img(qMax(1, int(std::ceil((w + pad * 2) * px))), qMax(1, int(std::ceil((h + pad * 2) * px))), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter x(&img);
        x.setRenderHint(QPainter::Antialiasing);
        x.scale(px, px);
        x.translate(pad, pad);
        x.setPen(Qt::NoPen);
        const QColor metal = mix(pal.app, hex("#2a2622"), 0.55);
        // espessura: a lateral da chapa virada pro meio da mesa
        x.fillRect(QRectF(g.side > 0 ? w : -4, 3, 4, h - 3), mix(metal, Qt::black, 0.45));
        if (g.type == Box) {
            // luminoso de acrílico: a face acende por dentro
            x.fillRect(QRectF(-3, -3, w + 6, h + 6), metal);
            vfill(x, QRectF(0, 0, w, h), on ? mix(g.face, Qt::white, 0.25) : mix(g.face, Qt::black, 0.55), on ? g.face : mix(g.face, Qt::black, 0.65), 0, h);
            if (on) x.fillRect(QRectF(0, 0, w, 2), alpha(Qt::white, 0.18));
        } else {
            // chapa escura de fundo, com a moldura de metal
            x.fillRect(QRectF(-3, -3, w + 6, h + 6), mix(metal, Qt::black, 0.25));
            vfill(x, QRectF(0, 0, w, h), mix(metal, Qt::white, 0.05), mix(metal, Qt::black, 0.4), 0, h);
            x.fillRect(QRectF(-3, -3, w + 6, 1), alpha(Qt::white, 0.07));
        }
        // ferrugem e sujeira escorrendo da moldura
        for (const auto& r : g.rust)
            vfill(x, QRectF(r[0], r[1], r[2], r[3]), alpha(g.type == Old ? hex("#6a3a1a") : QColor(Qt::black), r[4]), alpha(Qt::black, 0), r[1], r[1] + r[3]);
        // conteúdo: o desenho e o nome
        const QFont f = cjk(g.fs);
        const double a = g.artS, fs = g.fs;
        const QColor inkOn = on ? g.ink : mix(g.ink, Qt::black, 0.4);
        auto artAt = [&](double ax, double ay) {
            QPainterPath ap = QTransform().translate(ax, ay).scale(a, a).map(artPath(g.art));
            ap.setFillRule(Qt::OddEvenFill);
            if (g.type == Neon) tube(x, ap, g.col, on, 2.2);
            else x.fillPath(ap, g.type == Box ? inkOn : alpha(g.col, 0.3));
        };
        auto glyph = [&](const QString& ch, double cx, double cy) {
            const QPainterPath gp = glyphAt(f, ch, cx, cy);
            if (g.type == Neon) tube(x, gp, g.col, on, 1.6);
            else x.fillPath(gp, g.type == Box ? inkOn : alpha(g.col, 0.35));
        };
        const int n = g.txt.size();
        if (g.vert) {
            double y = 7;
            if (a > 0) { artAt((w - a) / 2, y); y += a + 4; }
            for (int i = 0; i < n; ++i) glyph(g.txt.mid(i, 1), w / 2, y + (i + 0.5) * fs * 1.12);
        } else {
            double xx = 8;
            if (a > 0) { artAt(xx, (h - a) / 2); xx += a + 5; }
            for (int i = 0; i < n; ++i) glyph(g.txt.mid(i, 1), xx + (i + 0.5) * fs * 1.12, h / 2 + 1);
        }
        // tubo de contorno em alguns neons
        if (g.type == Neon && g.border) {
            QPainterPath bp;
            bp.addRoundedRect(QRectF(3.5, 3.5, w - 7, h - 7), 4, 4);
            tube(x, bp, g.col2, on, 1.4);
        }
        x.end();
        return img;
    }
    // monta um letreiro de um comércio: tipo, sentido e tamanho saem do texto e do desenho
    Sign newSign(int type, bool vert, int side)
    {
        static const char* NC[6] = { "#ff3a4a", "#2aff9a", "#ffd23a", "#ff8a2a", "#4ad8ff", "#ff5ac8" };
        static const char* FACE[4] = { "#f4efe2", "#ffe9a8", "#f2f2f2", "#ffd0a0" };
        static const char* INK[3] = { "#b01a10", "#1a1410", "#b01a10" };
        // o mesmo comércio não se repete entre as placas vizinhas
        int bi;
        do bi = RI(9); while (std::find(m_recentBiz.begin(), m_recentBiz.end(), bi) != m_recentBiz.end());
        m_recentBiz.push_back(bi);
        if (m_recentBiz.size() > 4) m_recentBiz.erase(m_recentBiz.begin());
        const Biz& z = bizAt(bi);
        Sign g;
        g.type = type; g.vert = vert; g.side = side;
        g.txt = QString::fromUtf8(z.txt);
        const int n = g.txt.size();
        g.fs = vert ? R(17, 22) : R(15, 20);
        const bool withArt = rnd() < 0.75;
        g.art = withArt ? z.art : NoArt;
        g.artS = withArt ? (vert ? g.fs * 1.35 : g.fs * 1.25) : 0;
        g.w = vert ? g.fs * 1.55 : 16 + (g.artS > 0 ? g.artS + 5 : 0) + n * g.fs * 1.12;
        g.h = vert ? 14 + (g.artS > 0 ? g.artS + 4 : 0) + n * g.fs * 1.12 : g.fs * 1.7;
        const int nr = 2 + RI(4);
        for (int i = 0; i < nr; ++i) g.rust.push_back({ R(0, g.w - 4), R(-2, g.h * 0.6), R(2, 6), R(10, 40), type == Old ? R(0.25, 0.5) : R(0.1, 0.25) });
        g.col = hex(z.col);
        g.col2 = hex(NC[RI(6)]);
        g.border = rnd() < 0.5;
        g.face = hex(FACE[RI(4)]);
        g.ink = hex(INK[RI(3)]);
        return g;
    }

    // ---------------- outdoor no telhado: treliça, passarela, cartaz desenhado e holofotes ----------------
    static void poster(QPainter& x, int kind, double px, double py, double w, double h, const Pal& pal)
    {
        auto M = [&](const char* c) { return mix(hex(c), pal.app, 0.22); };
        x.save();
        x.setClipRect(QRectF(px, py, w, h), Qt::IntersectClip);
        x.setPen(Qt::NoPen);
        auto text = [&](const char* s, double size, const QColor& col, double lx, double cy) {
            const QFont f = cjk(size);
            const QFontMetricsF fm(f);
            QPainterPath t;
            t.addText(lx, cy + (fm.ascent() - fm.descent()) / 2, f, QString::fromUtf8(s));
            x.fillPath(t, col);
        };
        if (kind == 0) {   // refrigerante
            x.fillRect(QRectF(px, py, w, h), M("#c8261e"));
            QPainterPath wv(QPointF(px, py + h * 0.7));
            wv.cubicTo(px + w * 0.3, py + h * 0.45, px + w * 0.6, py + h * 0.95, px + w, py + h * 0.6);
            wv.lineTo(px + w, py + h * 0.75);
            wv.cubicTo(px + w * 0.6, py + h * 1.05, px + w * 0.3, py + h * 0.6, px, py + h * 0.85);
            wv.closeSubpath();
            x.fillPath(wv, M("#f4efe2"));
            const double bx = px + w * 0.78, bw = w * 0.1;
            QPainterPath bottle;
            bottle.addRoundedRect(QRectF(bx - bw / 2, py + h * 0.22, bw, h * 0.66), bw * 0.3, bw * 0.3);
            x.fillPath(bottle, M("#3a1a10"));
            x.fillRect(QRectF(bx - bw * 0.18, py + h * 0.1, bw * 0.36, h * 0.14), M("#3a1a10"));
            text("清涼", h * 0.32, M("#f4efe2"), px + w * 0.08, py + h * 0.32);
        } else if (kind == 1) {   // relógio
            vfill(x, QRectF(px, py, w, h), M("#14223a"), M("#060a14"), py, py + h);
            const double cx = px + w * 0.72, cy = py + h * 0.52, r = h * 0.34;
            x.setBrush(M("#e8e4d8"));
            x.drawEllipse(QPointF(cx, cy), r - 2, r - 2);
            x.setBrush(Qt::NoBrush);
            x.setPen(QPen(M("#d8b46a"), 2.5));
            x.drawEllipse(QPointF(cx, cy), r, r);
            x.setPen(QPen(M("#1a1a1a"), 1.6));
            x.drawLine(QPointF(cx, cy), QPointF(cx + r * 0.45, cy - r * 0.35));
            x.drawLine(QPointF(cx, cy), QPointF(cx - r * 0.1, cy - r * 0.7));
            x.setPen(Qt::NoPen);
            text("時光", h * 0.26, M("#d8b46a"), px + w * 0.07, py + h * 0.38);
            x.fillRect(QRectF(px + w * 0.07, py + h * 0.62, w * 0.3, 2), M("#8a9ab0"));
            x.fillRect(QRectF(px + w * 0.07, py + h * 0.72, w * 0.22, 2), M("#8a9ab0"));
        } else if (kind == 2) {   // companhia aérea
            QLinearGradient g(0, py, 0, py + h);
            g.setColorAt(0, M("#2a5a9a"));
            g.setColorAt(0.7, M("#e88a5a"));
            g.setColorAt(1, M("#f2c27a"));
            x.fillRect(QRectF(px, py, w, h), g);
            const double ax = px + w * 0.62, ay = py + h * 0.42, s = w * 0.0042;
            const QColor pc = M("#f4f4f4");
            QPolygonF fus, wing, tail;
            fus << QPointF(ax - 60 * s, ay + 4 * s) << QPointF(ax + 60 * s, ay - 6 * s) << QPointF(ax + 64 * s, ay) << QPointF(ax - 58 * s, ay + 10 * s);
            wing << QPointF(ax - 4 * s, ay) << QPointF(ax - 22 * s, ay + 34 * s) << QPointF(ax - 12 * s, ay + 34 * s) << QPointF(ax + 14 * s, ay);
            tail << QPointF(ax - 52 * s, ay + 4 * s) << QPointF(ax - 62 * s, ay - 16 * s) << QPointF(ax - 54 * s, ay - 16 * s) << QPointF(ax - 42 * s, ay + 2 * s);
            x.setBrush(pc);
            x.drawPolygon(fus);
            x.drawPolygon(wing);
            x.drawPolygon(tail);
            x.setBrush(Qt::NoBrush);
            text("環球", h * 0.24, M("#ffffff"), px + w * 0.06, py + h * 0.76);
        } else if (kind == 3) {   // lámen
            x.fillRect(QRectF(px, py, w, h), M("#f2c23a"));
            const double cx = px + w * 0.7, cy = py + h * 0.6, r = h * 0.3;
            QPainterPath bowl(QPointF(cx - r * 1.2, cy - r * 0.1));
            bowl.quadTo(cx, cy + r * 1.3, cx + r * 1.2, cy - r * 0.1);
            bowl.closeSubpath();
            x.fillPath(bowl, M("#c8261e"));
            x.setBrush(Qt::NoBrush);
            x.setPen(QPen(M("#fff2c8"), 2));
            for (int i = 0; i < 3; ++i) {
                QPainterPath st(QPointF(cx - r * 0.5 + i * r * 0.5, cy - r * 0.3));
                st.cubicTo(cx - r * 0.7 + i * r * 0.5, cy - r * 0.8, cx - r * 0.3 + i * r * 0.5, cy - r, cx - r * 0.5 + i * r * 0.5, cy - r * 1.4);
                x.drawPath(st);
            }
            x.setPen(QPen(M("#3a1a10"), 2));
            x.drawLine(QPointF(cx + r * 0.2, cy - r * 0.2), QPointF(cx + r * 1.4, cy - r * 1.3));
            x.drawLine(QPointF(cx + r * 0.45, cy - r * 0.15), QPointF(cx + r * 1.55, cy - r * 1.15));
            x.setPen(Qt::NoPen);
            text("麵", h * 0.5, M("#c8261e"), px + w * 0.08, py + h * 0.5);
        } else {   // cartaz de filme
            QLinearGradient g(px, py, px + w, py + h);
            g.setColorAt(0, M("#0e2a30"));
            g.setColorAt(1, M("#a8481e"));
            x.fillRect(QRectF(px, py, w, h), g);
            QRadialGradient sun(QPointF(px + w * 0.75, py + h * 0.35), h * 0.5);
            sun.setColorAt(0, alpha(hex("#ffd28a"), 0.7));
            sun.setColorAt(1, alpha(hex("#ffd28a"), 0));
            x.fillRect(QRectF(px, py, w, h), sun);
            QPainterPath hills(QPointF(px, py + h));
            for (int i = 0; i <= 8; ++i) hills.lineTo(px + i * w / 8, py + h * (0.62 + 0.18 * ((i * 37) % 5) / 5.0));
            hills.lineTo(px + w, py + h);
            hills.closeSubpath();
            x.fillPath(hills, M("#0a0a0e"));
            text("英雄", h * 0.34, M("#f4efe2"), px + w * 0.07, py + h * 0.32);
            x.fillRect(QRectF(px + w * 0.07, py + h * 0.52, w * 0.24, 2.5), M("#c8261e"));
        }
        // desbotado e sujo de baixo pra cima
        vfill(x, QRectF(px, py, w, h), alpha(Qt::black, 0.08), alpha(Qt::black, 0.32), py, py + h);
        x.restore();
    }
    static void billboard(QPainter& x, const Bld& b, const Pal& pal)
    {
        const double w = qMin(b.w - 8, R(84, 140)), h = w * R(0.42, 0.52), bx = b.x + (b.w - w) / 2, legH = R(12, 22), y = b.top - 3 - legH - h;
        const QColor steel = mix(pal.app, hex("#0a0808"), 0.55);
        // estrutura: pernas treliçadas e a passarela
        const double legs[3] = { bx + 6, bx + w / 2, bx + w - 6 };
        x.setBrush(Qt::NoBrush);
        x.setPen(QPen(steel, 1.2));
        for (double lx : legs) x.drawLine(QPointF(lx, y + h), QPointF(lx, b.top - 3));
        for (int i = 0; i < 2; ++i) {
            x.drawLine(QPointF(legs[i], y + h), QPointF(legs[i + 1], b.top - 3));
            x.drawLine(QPointF(legs[i + 1], y + h), QPointF(legs[i], b.top - 3));
        }
        x.setPen(Qt::NoPen);
        x.fillRect(QRectF(bx - 4, y + h + 4, w + 8, 2), steel);
        // cartaz com moldura
        x.fillRect(QRectF(bx - 3, y - 3, w + 6, h + 6), mix(steel, Qt::black, 0.3));
        poster(x, RI(5), bx, y, w, h, pal);
        // holofotes nos braços, lavando o cartaz de baixo pra cima
        const int n = w > 110 ? 3 : 2;
        for (int i = 0; i < n; ++i) {
            const double lx = bx + (i + 0.5) * w / n;
            x.setPen(QPen(steel, 1));
            x.drawLine(QPointF(lx, y + h + 4), QPointF(lx, y + h + 10));
            x.setPen(Qt::NoPen);
            x.fillRect(QRectF(lx - 3, y + h + 9, 6, 3), steel);
            x.save();
            x.setClipRect(QRectF(bx, y, w, h), Qt::IntersectClip);
            x.setCompositionMode(QPainter::CompositionMode_Plus);
            QRadialGradient g(QPointF(lx, y + h + 6), h * 1.1);
            g.setColorAt(0, alpha(hex("#fff2d0"), 0.35));
            g.setColorAt(1, alpha(hex("#fff2d0"), 0));
            x.fillRect(QRectF(lx - h * 1.1, y, h * 2.2, h + 10), g);
            x.restore();
            x.fillRect(QRectF(lx - 2, y + h + 8, 4, 1.5), alpha(hex("#fff6e0"), 0.95));
        }
    }
    // letreiros pequenos do plano do meio: assados no cenário, antes da névoa
    void midSigns(QPainter& x, const std::vector<Bld>& row, const Pal& pal, const Ctx& c)
    {
        for (const Bld& b : row) {
            if (b.w < 70 || rnd() > 0.45) continue;
            Sign g = newSign(rnd() < 0.6 ? Neon : Box, rnd() < 0.5, b.x + b.w / 2 < c.W / 2 ? 1 : -1);
            const QImage img = signImage(g, pal, c.px * 0.6, true);
            const double s = 0.6, w = g.w * s, h = g.h * s;
            const double sx = g.vert ? (rnd() < 0.5 ? b.x - w - 4 : b.x + b.w + 4) : b.x + (b.w - w) / 2;
            const double y0 = qMax(b.top + 30, c.H * 0.5), sy = R(y0, qMax(y0 + 1, c.H - 40 - h));
            x.drawImage(QRectF(sx - g.pad * s, sy - g.pad * s, (g.w + g.pad * 2) * s, (g.h + g.pad * 2) * s), img);
        }
    }
    // letreiros da frente: amontoados perto da rua; as lâminas na frente das placas de fachada
    void nearSigns(const std::vector<Bld>& rowIn, const Pal& pal, const Ctx& c)
    {
        struct Placed { QRectF r; int layer; };
        std::vector<Placed> placed;
        auto clash = [&](const QRectF& r, int layer) {
            for (const Placed& q : placed)
                if (q.layer == layer && !(r.left() > q.r.right() + 6 || r.right() < q.r.left() - 6 || r.top() > q.r.bottom() + 6 || r.bottom() < q.r.top() - 6)) return true;
            return false;
        };
        std::vector<Bld> row = rowIn;
        std::stable_sort(row.begin(), row.end(), [](const Bld& a, const Bld& b) { return a.x < b.x; });
        for (const Bld& b : row) {
            if (b.w < 60) continue;
            const int side = b.x + b.w / 2 < c.W / 2 ? 1 : -1;
            const int n = 1 + RI(3);
            for (int i = 0; i < n; ++i) {
                const double t = rnd();
                const int type = t < 0.55 ? Neon : t < 0.85 ? Box : Old;
                const bool blade = rnd() < 0.55;
                Sign g = newSign(type, blade || rnd() < 0.25, side);
                if (!blade && g.w > b.w - 10) continue;
                // perto da rua com mais frequência
                const double y = c.H - 30 - g.h - std::pow(rnd(), 1.7) * (c.H * 0.4);
                if (y < b.top + 30) continue;
                double x0;
                if (blade) {
                    const bool left = rnd() < 0.5;
                    g.edge = left ? b.x : b.x + b.w;
                    g.blade = left ? -1 : 1;
                    x0 = left ? g.edge - 8 - g.w : g.edge + 8;
                } else {
                    x0 = b.x + 5 + R(0, b.w - 10 - g.w);
                }
                const QRectF r(x0, y, g.w, g.h);
                if (clash(r, blade ? 1 : 0)) continue;
                placed.push_back({ r, blade ? 1 : 0 });
                g.x = x0;
                g.y = y;
                g.on = signImage(g, pal, c.px, true);
                g.off = g.type == Old ? g.on : signImage(g, pal, c.px, false);
                m_signs.push_back(std::move(g));
            }
        }
        // as placas de fachada primeiro, as lâminas por cima
        std::stable_sort(m_signs.begin(), m_signs.end(), [](const Sign& a, const Sign& b) { return (a.blade ? 1 : 0) < (b.blade ? 1 : 0); });
    }
    // lambe-lambe colado na parede perto da rua (pedido do Pe, no sigilo): vermelho, gasto, ponta rasgada
    static void lambe(QPainter& x, const std::vector<Bld>& row, const Pal& pal, const Ctx& c)
    {
        std::vector<const Bld*> ok;
        const QRectF& l = c.zones.left;
        for (const Bld& b : row) {
            const double cx = b.x + b.w / 2;
            if (b.w > 70 && (l.width() >= 120 ? (cx > l.left() + 25 && cx < l.right() - 10) : (cx > 70 && cx < 400))) ok.push_back(&b);
        }
        if (ok.empty()) for (const Bld& b : row) if (b.w > 50) ok.push_back(&b);
        if (ok.empty()) return;
        const Bld& b = *ok[size_t(RI(int(ok.size())))];
        const double w = 30, h = 40, px = b.x + R(8, b.w - w - 8), py = c.H - R(95, 150);
        x.save();
        x.translate(px + w / 2, py + h / 2);
        x.rotate(qRadiansToDegrees(R(-0.06, 0.05)));
        x.translate(-w / 2, -h / 2);
        x.setPen(Qt::NoPen);
        x.fillRect(QRectF(1.5, 2, w, h), alpha(Qt::black, 0.35));
        QPolygonF paper;
        paper << QPointF(0, 0) << QPointF(w, 0) << QPointF(w, h - 9) << QPointF(w - 6, h - 4) << QPointF(w - 9, h) << QPointF(0, h);
        x.setBrush(mix(hex("#c8201e"), pal.app, 0.25));
        x.drawPolygon(paper);
        const QColor ink = mix(hex("#f4efe2"), pal.app, 0.2);
        QFont f;
        f.setFamilies({ QStringLiteral("IBM Plex Sans"), QStringLiteral("Arial") });
        f.setWeight(QFont::ExtraBold);
        f.setPixelSize(9);
        x.fillPath(glyphAt(f, QStringLiteral("LULA"), w / 2, 9), ink);
        f.setPixelSize(18);
        x.fillPath(glyphAt(f, QStringLiteral("13"), w / 2, 25), ink);
        // estrelinha no canto
        QPolygonF star;
        for (int i = 0; i < 10; ++i) {
            const double an = -M_PI / 2 + i * M_PI / 5, r = i % 2 ? 1.4 : 3.2;
            star << QPointF(w - 6 + std::cos(an) * r, 6 + std::sin(an) * r);
        }
        x.setBrush(ink);
        x.drawPolygon(star);
        // cola enrugada e o desbotado de baixo pra cima
        x.setBrush(Qt::NoBrush);
        x.setPen(QPen(alpha(Qt::black, 0.18), 0.6));
        x.drawLine(QPointF(3, 14), QPointF(12, 16));
        x.drawLine(QPointF(18, 31), QPointF(27, 29));
        x.setPen(Qt::NoPen);
        vfill(x, QRectF(0, 0, w, h), alpha(Qt::black, 0.05), alpha(Qt::black, 0.3), 0, h);
        x.restore();
    }

    void build(const Pal& pal, const Ctx& c)
    {
        static const char* WALL[7] = { "#3a302a", "#34302c", "#3c2c2a", "#2e322e", "#3c3426", "#32302e", "#3a3430" };
        m_signs.clear();
        m_recentBiz.clear();
        QPixmap pm = layer(c, c.W, c.H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing, true);
        // céu nublado alaranjado pela cidade
        sky(x, c, mix(pal.app, hex("#1a1420"), 0.4), mix(pal.app, hex("#6a3a2a"), 0.4));
        const QColor hz = mix(pal.app, hex("#7a4a32"), 0.45);
        auto hazeOver = [&](double a0, double a1) { vfill(x, QRectF(0, 0, c.W, c.H), alpha(hz, a0), alpha(hz, a1), 0, c.H); };
        const double VX = c.W / 2;
        // três planos: o fundo some na névoa alaranjada, a frente é escura e cheia de detalhe
        for (int k = 0; k < 3; ++k) {
            const Lay& L = LAY[k];
            std::vector<Bld> row;
            for (double bx = R(-30, 0); bx < c.W + 20;) {
                const double bw = R(L.w0, L.w1);
                row.push_back({ bx, bw, R(L.t0, L.t1), hex(WALL[RI(7)]), L.lit * R(0.35, 1.4), RI(3), RI(4) - 1 });
                // de vez em quando uma viela estreita entre dois prédios da frente
                bx += bw + (k == 2 && rnd() < 0.22 ? R(4, 10) : R(0, 2));
            }
            // de fora pra dentro: quem está mais perto do meio passa por cima da lateral do vizinho
            std::stable_sort(row.begin(), row.end(), [&](const Bld& a, const Bld& b) { return std::abs(a.x + a.w / 2 - VX) > std::abs(b.x + b.w / 2 - VX); });
            for (const Bld& b : row) building(x, b, k, pal, c);
            // outdoors no topo de alguns prédios (os do meio ficam na névoa)
            if (k >= 1)
                for (const Bld& b : row)
                    if (b.w > 84 && rnd() < (k == 2 ? 0.3 : 0.2)) billboard(x, b, pal);
            if (k == 1) midSigns(x, row, pal, c);
            if (k == 2) { lambe(x, row, pal, c); nearSigns(row, pal, c); }
            // névoa entre os planos: o fundo some, o meio esmaece
            if (k == 0) hazeOver(0.3, 0.6);
            if (k == 1) hazeOver(0.1, 0.32);
        }
        // a rua lá embaixo: neblina quente subindo e a luz das lojas
        vfill(x, QRectF(0, c.H - 220, c.W, 220), alpha(hz, 0), alpha(hz, 0.35), c.H - 220, c.H);
        x.setCompositionMode(QPainter::CompositionMode_Plus);
        vfill(x, QRectF(0, c.H - 160, c.W, 160), alpha(hex("#ff9a5a"), 0), alpha(hex("#ff9a5a"), 0.1), c.H - 160, c.H);
        x.end();
        m_back.set(c, pm);
    }
    static constexpr Lay LAY[3] = { { 80, 190, 44, 92, 0.05, 3, 7, 8, 0.5, 0.3 },
                                    { 160, 280, 58, 116, 0.18, 4, 9, 11, 0.72, 0.32 },
                                    { 240, 390, 72, 140, 0.32, 6, 10, 13, 0.9, 0.34 } };
    Cache m_back;
    std::vector<Sign> m_signs;
    std::vector<int> m_recentBiz;
    double m_t = 0, m_jet = -1e9, m_jy = 100, m_jv = 190, m_next = 3;
};

} // namespace

std::unique_ptr<Layer> makeLeva3(const QString& id)
{
    if (id == QLatin1String("alley")) return std::make_unique<Alley>();
    if (id == QLatin1String("facade")) return std::make_unique<Facade>();
    if (id == QLatin1String("trails")) return std::make_unique<Trails>();
    if (id == QLatin1String("runway")) return std::make_unique<Runway>();
    if (id == QLatin1String("lapa")) return std::make_unique<Lapa>();
    if (id == QLatin1String("ferris")) return std::make_unique<Ferris>();
    if (id == QLatin1String("kowloon")) return std::make_unique<Kowloon>();
    return nullptr;
}

} // namespace AnimBg
