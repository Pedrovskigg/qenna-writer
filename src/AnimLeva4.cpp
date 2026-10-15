// Temas Animados, leva 4 (mais cidades): Veneza, Paris, metrô República,
// lanchonete, cruzamento visto de cima e Los Angeles do morro. Portado de
// leva4a.js / leva4b.js.

#include "AnimKit.h"

#include <QFont>
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

// ============================================================ Veneza: canal, palazzi e gôndola

class Canal : public Layer {
public:
    void init(Ctx&) override { m_back = QImage(); m_g = -1e9; m_next = 0.5; }
    void resized(Ctx& c) override { init(c); }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        if (m_g < -1e8) { m_next -= dt; if (m_next <= 0) m_g = -260; }
        else { m_g += 26 * dt; if (m_g > c.W + 60) { m_g = -1e9; m_next = R(6, 14); } }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double WL = c.H * 0.6, o = zx(c, 45) - 45;
        const QSize sz(int(c.W * c.px), int(c.H * c.px));
        if (m_back.size() != sz) build(pal, c, WL, o, sz);
        p.drawImage(QRectF(0, 0, c.W, c.H), m_back);
        // água: o reflexo do canal em faixas que tremem
        vfill(p, QRectF(0, WL, c.W, c.H - WL), mix(pal.app, hex("#14202e"), 0.4), mix(pal.app, hex("#04060a"), 0.5), WL, c.H);
        p.setOpacity(0.5);
        const double k = c.px;
        for (double y = WL; y < c.H; y += 3) {
            const double d = y - WL, src = WL - d * 1.15 - 3, sh = std::sin(m_t * 1.7 + y * 0.17) * (1.5 + d * 0.045);
            if (src > 0) p.drawImage(QRectF(sh, y, c.W, 3), m_back, QRectF(0, src * k, c.W * k, 3 * k));
        }
        p.setOpacity(1);
        QLinearGradient dk(0, WL, 0, c.H);
        dk.setColorAt(0, alpha(pal.app, 0.1));
        dk.setColorAt(1, alpha(pal.app, 0.55));
        p.fillRect(QRectF(0, WL, c.W, c.H - WL), dk);
        // lampião da ponte
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        glow(p, 200 + o, WL - 164, 120, hex("#ffcf8a"), 0.45);
        p.fillRect(QRectF(195 + o, WL - 168, 10, 8), hex("#fff0c8"));
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // postes de amarração listrados, com o reflexo
        const struct { double x; const char* c; } POLES[] = { { 110 + o, "#2a4a9a" }, { 345 + o, "#b0302a" }, { rx(c, 1150), "#2a4a9a" }, { rx(c, 1330), "#b0302a" } };
        for (const auto& pl : POLES) {
            for (double y = WL - 120; y < WL + 26; y += 14) p.fillRect(QRectF(pl.x - 4, y, 8, 14), mix(int(std::floor((y - WL) / 14)) % 2 ? hex(pl.c) : hex("#e8e0d0"), pal.app, 0.35));
            p.setOpacity(0.35);
            for (double y = WL + 26; y < WL + 150; y += 14) p.fillRect(QRectF(pl.x - 4 + std::sin(m_t * 2 + y * 0.2) * 3, y, 8, 12), mix(int(std::floor((y - WL) / 14)) % 2 ? hex(pl.c) : hex("#e8e0d0"), pal.app, 0.4));
            p.setOpacity(1);
        }
        // gôndola com o gondoleiro remando
        if (m_g > -1e8) {
            const double gx = m_g, gy = WL + 70, st = std::sin(m_t * 1.3);
            auto hull = [&](double s) {
                QPainterPath h(QPointF(gx, gy - 14 * s));
                h.quadTo(gx + 20, gy + 4 * s, gx + 120, gy + 6 * s);
                h.quadTo(gx + 210, gy + 4 * s, gx + 240, gy - 22 * s);
                h.lineTo(gx + 236, gy - 24 * s);
                h.quadTo(gx + 200, gy - 2 * s, gx + 120, gy - 2 * s);
                h.quadTo(gx + 30, gy - 2 * s, gx + 4, gy - 16 * s);
                h.closeSubpath();
                return h;
            };
            p.fillPath(hull(-1).translated(0, 14), alpha(Qt::black, 0.35));
            p.fillPath(hull(1), hex("#08080a"));
            p.fillRect(QRectF(gx + 232, gy - 34, 4, 12), hex("#c8c0a8"));
            for (int i = 0; i < 4; ++i) p.fillRect(QRectF(gx + 236, gy - 32 + i * 3, 6, 1.5), hex("#c8c0a8"));
            const double mx = gx + 34, my = gy - 6, gh = 70;
            Fig f;
            f.hair = Fig::Hat; f.band = hex("#b8302a"); f.stripes = hex("#e8e0d0"); f.arm = Fig::Oar; f.st = st; f.lean = -0.05 + st * 0.04;
            const double hx = mx + (0.14 + st * 0.05) * gh, hy = my - (0.66 - st * 0.06) * gh;
            p.setPen(pen(hex("#1a1208"), 2.6));
            p.drawLine(QPointF(hx + 14, hy - 26), QPointF(mx - 34 + st * 16, gy + 28));
            figure(p, mx, my, gh, f);
            p.fillRect(QRectF(gx + 150, gy - 18, 5, 6), alpha(hex("#ffcf8a"), 0.9));
        }
    }

private:
    void build(const Pal& pal, const Ctx& c, double WL, double o, const QSize& sz)
    {
        static const char* PC[] = { "#c88a5a", "#d8b07a", "#b86a4a", "#e0c090", "#a8604a", "#c8a08a" };
        m_back = QImage(sz, QImage::Format_ARGB32_Premultiplied);
        m_back.setDevicePixelRatio(c.px);
        m_back.fill(Qt::transparent);
        QPainter x(&m_back);
        x.setRenderHint(QPainter::Antialiasing, true);
        vfill(x, QRectF(0, 0, c.W, WL), mix(pal.app, hex("#0a1028"), 0.45), mix(pal.app, hex("#5a3a58"), 0.4), 0, WL);
        x.drawPixmap(0, 0, starLayer(c, 90, WL * 0.5, 0.4));
        for (double bx = -10; bx < c.W;) {
            const double bw = R(100, 170), top = WL - R(180, 340);
            const QColor col = mix(hex(PC[RI(6)]), pal.app, 0.55);
            x.fillRect(QRectF(bx, top, bw, WL - top + 2), col);
            for (int i = 0; i < 40; ++i) x.fillRect(QRectF(bx + R(0, bw), top + R(0, WL - top), R(6, 20), R(4, 10)), alpha(Qt::black, 0.12));
            x.fillRect(QRectF(bx - 3, top - 7, bw + 6, 7), mix(col, Qt::black, 0.35));
            const int rows = qMax(1, int((WL - top - 60) / 58)), cols = qMax(2, int(bw / 34));
            for (int r = 0; r < rows; ++r)
                for (int k = 0; k < cols; ++k) {
                    const double wx = bx + (k + 0.5) * bw / cols - 8, wy = top + 22 + r * 58;
                    QPainterPath w(QPointF(wx, wy + 34));
                    w.lineTo(wx, wy + 10);
                    w.quadTo(wx, wy, wx + 8, wy - 4);
                    w.quadTo(wx + 16, wy, wx + 16, wy + 10);
                    w.lineTo(wx + 16, wy + 34);
                    w.closeSubpath();
                    x.fillPath(w, rnd() < 0.42 ? hex("#ffcf8a") : mix(col, Qt::black, 0.55));
                    if (r == 1 && k % 2 == 0 && rnd() < 0.4) x.fillRect(QRectF(wx - 6, wy + 34, 28, 4), mix(col, Qt::white, 0.25));
                }
            x.fillRect(QRectF(bx + bw * 0.4, WL - 36, bw * 0.2, 38), mix(col, Qt::black, 0.65));
            bx += bw + R(0, 3);
        }
        // pontezinha de pedra com lampião
        const QColor st = mix(hex("#d8ccb8"), pal.app, 0.45);
        QPainterPath br(QPointF(20 + o, WL + 4));
        br.lineTo(20 + o, WL - 62);
        br.quadTo(200 + o, WL - 100, 380 + o, WL - 62);
        br.lineTo(380 + o, WL + 4);
        br.lineTo(330 + o, WL + 4);
        br.quadTo(200 + o, WL - 72, 70 + o, WL + 4);
        br.closeSubpath();
        x.fillPath(br, st);
        x.setPen(QPen(mix(st, Qt::black, 0.3), 2));
        x.setBrush(Qt::NoBrush);
        QPainterPath rail(QPointF(20 + o, WL - 84));
        rail.quadTo(200 + o, WL - 122, 380 + o, WL - 84);
        x.drawPath(rail);
        for (double bx2 = 30; bx2 < 380; bx2 += 14) { const double t = (bx2 - 20) / 360, y0 = WL - 62 - 38 * 4 * t * (1 - t); x.drawLine(QPointF(bx2 + o, y0), QPointF(bx2 + o, y0 - 22)); }
        x.fillRect(QRectF(198 + o, WL - 160, 4, 62), hex("#14100c"));
        x.fillRect(QRectF(192 + o, WL - 170, 16, 12), hex("#14100c"));
    }
    QImage m_back;
    double m_t = 0, m_g = -1e9, m_next = 2;
};

// ============================================================ Paris: telhados de zinco e a torre

class Mansard : public Layer {
public:
    void init(Ctx&) override { m_back.reset(); m_smoke.clear(); m_sparkle = 0; m_nextSp = 4; }
    void step(double dt, Ctx&) override
    {
        m_t += dt;
        if (m_sparkle > 0) m_sparkle -= dt;
        else { m_nextSp -= dt; if (m_nextSp <= 0) { m_sparkle = 9; m_nextSp = R(30, 45); } }
        if (!m_chimneys.empty() && rnd() < dt * 2.5) { const QPointF ch = m_chimneys[size_t(RI(int(m_chimneys.size())))]; m_smoke.push_back({ ch.x(), ch.y(), 4, 0.16, R(4, 10) }); }
        for (auto& q : m_smoke) { q[1] -= 16 * dt; q[0] += (q[4] + std::sin(m_t + q[1] * 0.05) * 4) * dt; q[2] += 7 * dt; q[3] -= 0.035 * dt; }
        m_smoke.erase(std::remove_if(m_smoke.begin(), m_smoke.end(), [](const std::array<double, 5>& q) { return q[3] <= 0; }), m_smoke.end());
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double tx = zx(c, 250), TB = c.H * 0.62;
        if (!m_back.valid(c)) build(pal, c, tx, TB);
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // farol do topo girando
        const double th = m_t * 0.8, cs = std::cos(th), ly = TB - TH - 26;
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        const double len = 900 * std::abs(cs), dir = cs >= 0 ? 1 : -1, spread = 14 + 50 * (1 - std::abs(cs));
        if (len > 20) {
            QLinearGradient g(tx, ly, tx + dir * len, ly);
            g.setColorAt(0, alpha(hex("#fff2c8"), 0.35));
            g.setColorAt(1, alpha(hex("#fff2c8"), 0));
            QPolygonF beam;
            beam << QPointF(tx, ly - 2) << QPointF(tx + dir * len, ly - spread) << QPointF(tx + dir * len, ly + spread) << QPointF(tx, ly + 2);
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawPolygon(beam);
        }
        glow(p, tx, TB - TH * 0.5, 300, hex("#ffb050"), 0.14);
        // a cintilação: a torre pisca inteira por uns segundos
        if (m_sparkle > 0) {
            const double a = qMin(1.0, qMin(m_sparkle, 9 - m_sparkle));
            QVector<QRectF> sp;
            for (const QPointF& q : m_pts) if (rnd() < 0.18) sp.append(QRectF(q.x() - 1, q.y() - 1, 2.4, 2.4));
            p.setPen(Qt::NoPen);
            p.setBrush(alpha(Qt::white, a));
            p.drawRects(sp);
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        for (const auto& q : m_smoke) { p.setOpacity(q[3] * 1.8); blit(p, smokeDot(), q[0] - q[2], q[1] - q[2], q[2] * 2, q[2] * 2); }
        p.setOpacity(1);
    }

private:
    static constexpr double TH = 440;
    static double tw(double h) { return 105 * std::pow(1 - h, 2.3) + 3.5; }
    void build(const Pal& pal, const Ctx& c, double tx, double TB)
    {
        m_chimneys.clear();
        m_pts.clear();
        QPixmap pm = layer(c, c.W, c.H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing, true);
        sky(x, c, mix(pal.app, hex("#081030"), 0.45), mix(pal.app, hex("#2a3a6a"), 0.4));
        x.drawPixmap(0, 0, starLayer(c, 110, c.H * 0.5, 0.45));
        disc(x, rx(c, 1240), 110, 26, hex("#eef0f8"));
        // a torre, acesa em dourado, com treliça
        QPainterPath tower(QPointF(tx - tw(0), TB));
        for (double h = 0; h <= 1.001; h += 0.02) tower.lineTo(tx - tw(h), TB - h * TH);
        tower.lineTo(tx, TB - TH - 30);
        for (double h = 1; h >= -0.001; h -= 0.02) tower.lineTo(tx + tw(h), TB - h * TH);
        tower.lineTo(tx + tw(0) - 30, TB);
        tower.quadTo(tx, TB - 120, tx - tw(0) + 30, TB);
        tower.closeSubpath();
        QLinearGradient g(0, TB - TH, 0, TB);
        g.setColorAt(0, hex("#f2c46a"));
        g.setColorAt(1, hex("#c8782a"));
        x.fillPath(tower, g);
        x.save();
        x.setClipPath(tower, Qt::IntersectClip);
        QVector<QLineF> lat;
        for (int k = -60; k < 60; ++k) { lat.append(QLineF(tx + k * 9, TB - TH, tx + k * 9 + TH * 0.4, TB)); lat.append(QLineF(tx + k * 9, TB - TH, tx + k * 9 - TH * 0.4, TB)); }
        x.setPen(QPen(alpha(hex("#3a1a08"), 0.55), 1.2));
        x.drawLines(lat);
        x.restore();
        for (double h : { 0.14, 0.36, 0.82 }) x.fillRect(QRectF(tx - tw(h) - 6, TB - h * TH - 4, tw(h) * 2 + 12, 6), hex("#ffd88a"));
        for (int i = 0; i < 260; ++i) { const double h = std::pow(rnd(), 0.8), w = tw(h); m_pts.push_back(QPointF(tx + R(-w, w), TB - h * TH)); }
        // telhados de zinco: duas fileiras, com águas-furtadas acesas e chaminés de vaso
        const double rows[2][3] = { { TB + 10, 0.7, 0.25 }, { c.H * 0.86, 1, 0.45 } };
        for (const auto& row : rows) {
            const double base = row[0], sc = row[1], dk = row[2];
            for (double bx = -20; bx < c.W;) {
                const double bw = R(150, 240) * sc, top = base - R(40, 80) * sc;
                const QColor zinc = mix(mix(hex("#5a6478"), pal.app, 0.4), Qt::black, dk), stone = mix(mix(hex("#c8b89a"), pal.app, 0.55), Qt::black, dk);
                x.fillRect(QRectF(bx, base, bw, c.H - base), stone);
                for (double wx = bx + 16 * sc; wx < bx + bw - 30 * sc; wx += 40 * sc) x.fillRect(QRectF(wx, base + 18 * sc, 18 * sc, 30 * sc), rnd() < 0.45 ? hex("#ffc870") : mix(stone, Qt::black, 0.5));
                QPolygonF roof;
                roof << QPointF(bx, base) << QPointF(bx + 14 * sc, top) << QPointF(bx + bw - 14 * sc, top) << QPointF(bx + bw, base);
                x.setPen(Qt::NoPen);
                x.setBrush(zinc);
                x.drawPolygon(roof);
                QVector<QLineF> seams;
                for (double sx = bx + 10; sx < bx + bw; sx += 7 * sc) seams.append(QLineF(sx, base, sx + (sx < bx + bw / 2 ? 6 : -6) * sc, top));
                x.setPen(QPen(alpha(Qt::black, 0.2), 1));
                x.drawLines(seams);
                for (double dx = bx + 30 * sc; dx < bx + bw - 40 * sc; dx += 52 * sc) {
                    const double dy = top + (base - top) * 0.35;
                    x.fillRect(QRectF(dx - 2, dy - 4, 24 * sc, 28 * sc), mix(zinc, Qt::black, 0.2));
                    QPolygonF dr;
                    dr << QPointF(dx - 4, dy - 2) << QPointF(dx + 10 * sc, dy - 14 * sc) << QPointF(dx + 24 * sc, dy - 2);
                    x.setPen(Qt::NoPen);
                    x.setBrush(mix(zinc, Qt::black, 0.2));
                    x.drawPolygon(dr);
                    x.fillRect(QRectF(dx + 3 * sc, dy, 14 * sc, 18 * sc), rnd() < 0.5 ? hex("#ffcf8a") : mix(zinc, Qt::black, 0.5));
                }
                const double cx = bx + R(20, qMax(21.0, bw - 60)), cw = R(30, 50) * sc, ch = R(26, 40) * sc;
                x.fillRect(QRectF(cx, top - ch, cw, ch + 4), mix(mix(hex("#8a4a32"), pal.app, 0.5), Qt::black, dk));
                for (int k = 0; k < int(cw / (8 * sc)); ++k) x.fillRect(QRectF(cx + 2 + k * 8 * sc, top - ch - 9 * sc, 5 * sc, 9 * sc), mix(mix(hex("#b0603a"), pal.app, 0.45), Qt::black, dk));
                if (rnd() < 0.5) m_chimneys.push_back(QPointF(cx + cw / 2, top - ch - 10 * sc));
                bx += bw + R(2, 10);
            }
        }
        x.end();
        m_back.set(c, pm);
    }
    Cache m_back;
    std::vector<QPointF> m_pts, m_chimneys;
    std::vector<std::array<double, 5>> m_smoke;
    double m_t = 0, m_sparkle = 0, m_nextSp = 5;
};

// ============================================================ metrô: estação República, Linha 4-Amarela

class Metro : public Layer {
public:
    void init(Ctx&) override { m_back.reset(); m_st = 0; m_idle = 1.5; }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_flick = rnd() < 0.06 ? 1 : m_flick * 0.7;
        if (m_st == 0) { m_idle -= dt; if (m_idle <= 0) { m_st = 1; m_x = c.W + 200; m_v = 520; m_tt = 0; } return; }
        m_tt += dt;
        if (m_st == 1) { const double d = m_x - STOP; m_v = qBound(24.0, std::sqrt(2 * 110 * qMax(0.0, d)), 520.0); m_x -= m_v * dt; if (m_x <= STOP + 1) { m_x = STOP; m_st = 2; m_tt = 0; } }
        else if (m_st == 2) { if (m_tt > 7) { m_st = 3; m_tt = 0; m_v = 0; } }
        else { m_v += 120 * dt; m_x -= m_v * dt; if (m_x + N * (CAR + 10) < -40) { m_st = 0; m_idle = R(8, 16); } }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        if (!m_back.valid(c)) build(pal, c);
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // lâmpadas do teto, uma falhando
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (int i = 0; i * 190 < c.W; ++i) {
            const double lx = 40 + i * 190, on = i == 2 ? 1 - m_flick : 1;
            p.fillRect(QRectF(lx, 48, 120, 5), alpha(hex("#f4fff8"), 0.9 * on));
            QLinearGradient g(0, 53, 0, 220);
            g.setColorAt(0, alpha(hex("#e8fff0"), 0.12 * on));
            g.setColorAt(1, alpha(hex("#e8fff0"), 0));
            p.fillRect(QRectF(lx - 40, 53, 200, 170), g);
        }
        if (m_st == 1 && m_x > c.W - 200) glow(p, c.W, TOP + 140, 400, hex("#fff6d8"), 0.5);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        const double open = m_st == 2 ? smooth(0.8, 1.6, m_tt) * (1 - smooth(5.5, 6.4, m_tt)) : 0;
        if (m_st != 0) {
            const double blur = qMin(1.0, m_v / 400);
            const QColor body = mix(hex("#e4e6ea"), pal.app, 0.2);
            for (int k = 0; k < N; ++k) {
                const double cx = m_x + k * (CAR + 10);
                if (cx > c.W + 20 || cx + CAR < -20) continue;
                QPainterPath car(QPointF(cx + 10, TOP - 10));
                car.lineTo(cx + CAR - 10, TOP - 10);
                car.quadTo(cx + CAR, TOP - 10, cx + CAR, TOP + 10);
                car.lineTo(cx + CAR, EDGE - 30);
                car.lineTo(cx, EDGE - 30);
                car.lineTo(cx, TOP + 10);
                car.quadTo(cx, TOP - 10, cx + 10, TOP - 10);
                p.fillPath(car, body);
                p.fillRect(QRectF(cx, TOP + 150, CAR, 12), hex("#f2c200"));
                p.fillRect(QRectF(cx, TOP + 162, CAR, 4), hex("#2a2a2e"));
                // janelas com gente: uns de vez em quando segurando a alça
                const double offs[4] = { 8, 158, 214, 368 };
                for (int w = 0; w < 4; ++w) {
                    const double wx = cx + offs[w];
                    const QRectF win(wx, TOP + 30, 46, 70);
                    p.fillRect(win, alpha(hex("#fff4d8"), 0.92));
                    p.fillRect(QRectF(wx, TOP + 36, 46, 2), alpha(hex("#3a3a40"), 0.6));
                    const int sd = (k * 7 + w * 3) % 6;
                    if (sd < 4) {
                        p.save();
                        p.setClipRect(win, Qt::IntersectClip);
                        Fig f;
                        f.hair = hairAt(k * 3 + w);
                        f.build = 0.9 + (sd % 3) * 0.1;
                        f.arm = sd == 1 && k % 2 == 0 ? Fig::Up : Fig::Down;
                        f.col = mix(hex("#fff4d8"), hex("#1e1820"), 0.85);
                        figure(p, wx + 12 + sd * 7, TOP + 172, 132, f);
                        p.restore();
                    }
                }
                // portas: abrem pros lados quando para
                for (double dx : { CAR * 0.25, CAR * 0.75 }) {
                    const double dw = 44, gap = open * 30, hh = EDGE - 50 - TOP - 20;
                    const QColor dc = mix(body, Qt::black, 0.12);
                    p.fillRect(QRectF(cx + dx - dw - gap, TOP + 20, dw, hh), dc);
                    p.fillRect(QRectF(cx + dx + gap, TOP + 20, dw, hh), dc);
                    p.fillRect(QRectF(cx + dx - dw - gap + 8, TOP + 34, dw - 16, 50), alpha(hex("#fff4d8"), 0.85));
                    p.fillRect(QRectF(cx + dx + gap + 8, TOP + 34, dw - 16, 50), alpha(hex("#fff4d8"), 0.85));
                    if (gap > 1) p.fillRect(QRectF(cx + dx - gap, TOP + 20, gap * 2, hh), alpha(hex("#fff4d8"), 0.95));
                }
                for (double wx : { 40.0, CAR - 40 }) disc(p, cx + wx, EDGE - 28, 10, hex("#1a1a1e"));
                if (blur > 0.2) p.fillRect(QRectF(cx, TOP - 10, CAR, EDGE - TOP - 20), alpha(pal.app, 0.25 * blur));
            }
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            QLinearGradient g(0, EDGE, 0, EDGE + 120);
            g.setColorAt(0, alpha(hex("#fff4d8"), 0.14));
            g.setColorAt(1, alpha(hex("#fff4d8"), 0));
            p.fillRect(QRectF(qMax(0.0, m_x), EDGE, qMin(c.W, N * (CAR + 10)), 120), g);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        // portas de plataforma (Linha 4): parede de vidro na beira, com as portas no lugar das do trem
        const double PT = TOP - 34, PB = EDGE + 2;
        p.fillRect(QRectF(0, PT - 16, c.W, 16), hex("#2a2c30"));
        std::vector<double> doors;
        for (int k = 0; k < N; ++k) for (double f : { 0.25, 0.75 }) doors.push_back(STOP + k * (CAR + 10) + CAR * f);
        doors.push_back(c.W + 200);
        double x0 = 0;
        for (double dx : doors) {
            const double l = dx - 52, r = dx + 52;
            if (l > x0) { p.fillRect(QRectF(x0, PT, l - x0, PB - PT), alpha(hex("#dfe8ee"), 0.07)); p.fillRect(QRectF(l - 4, PT, 4, PB - PT), hex("#3a3c40")); }
            if (dx < c.W + 100) {
                const double g2 = open * 44;
                p.fillRect(QRectF(dx - 48 - g2, PT, 48, PB - PT), alpha(hex("#dfe8ee"), 0.1));
                p.fillRect(QRectF(dx + g2, PT, 48, PB - PT), alpha(hex("#dfe8ee"), 0.1));
                p.setPen(QPen(hex("#5a5c60"), 2));
                p.setBrush(Qt::NoBrush);
                p.drawRect(QRectF(dx - 48 - g2, PT, 48, PB - PT));
                p.drawRect(QRectF(dx + g2, PT, 48, PB - PT));
                const QColor led = open > 0.05 && open < 0.95 ? hex(int(m_t * 6) % 2 ? "#ff4a3a" : "#3a1a18") : open >= 0.95 ? hex("#4aff7a") : hex("#1a3a20");
                p.fillRect(QRectF(dx - 6, PT - 11, 12, 5), led);
            }
            x0 = r;
            if (x0 > c.W) break;
        }
        // reflexo no vidro
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        QVector<QLineF> refl;
        for (double xx = -200; xx < c.W; xx += 260) refl.append(QLineF(xx, PB, xx + 160, PT));
        p.setPen(QPen(alpha(Qt::white, 0.05), 18));
        p.drawLines(refl);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    static constexpr double TOP = 300, EDGE = 560, CAR = 420, STOP = 40;
    static constexpr int N = 6;
    void build(const Pal& pal, const Ctx& c)
    {
        QPixmap pm = layer(c, c.W, c.H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing, true);
        // parede de concreto em painéis, faixa amarela da Linha 4 e as placas
        x.fillRect(QRectF(0, 0, c.W, TOP + 40), mix(hex("#b8b6b0"), pal.app, 0.45));
        QVector<QLineF> seams;
        for (double xx = 0; xx < c.W; xx += 160) seams.append(QLineF(xx, 54, xx, TOP + 40));
        seams.append(QLineF(0, 230, c.W, 230));
        x.setPen(QPen(alpha(Qt::black, 0.14), 1.5));
        x.drawLines(seams);
        for (int i = 0, n = int(900 * c.W / 1440); i < n; ++i) x.fillRect(QRectF(R(0, c.W), R(54, TOP + 40), R(2, 6), R(2, 6)), alpha(rnd() < 0.5 ? Qt::black : Qt::white, R(0.02, 0.05)));
        x.fillRect(QRectF(0, 168, c.W, 26), mix(hex("#f2c200"), pal.app, 0.15));
        QFont f(QStringLiteral("IBM Plex Sans"));
        f.setWeight(QFont::Bold);
        for (double sx : { zx(c, 70), rx(c, 1120) }) {
            x.fillRect(QRectF(sx, 100, 280, 54), hex("#26282c"));
            disc(x, sx + 30, 127, 17, hex("#f2c200"));
            f.setPixelSize(22);
            x.setFont(f);
            x.setPen(hex("#1a1a1a"));
            x.drawText(QRectF(sx + 13, 110, 34, 34), Qt::AlignCenter, QStringLiteral("4"));
            f.setPixelSize(27);
            x.setFont(f);
            x.setPen(Qt::white);
            x.drawText(QRectF(sx + 50, 100, 220, 54), Qt::AlignCenter, QStringLiteral("REPÚBLICA"));
        }
        x.fillRect(QRectF(0, 0, c.W, 54), mix(pal.app, hex("#2a2a30"), 0.4));
        // fosso dos trilhos
        x.fillRect(QRectF(0, TOP + 40, c.W, EDGE - TOP - 40), mix(pal.app, hex("#050506"), 0.65));
        x.fillRect(QRectF(0, EDGE - 46, c.W, 4), mix(pal.app, hex("#6a6a70"), 0.35));
        x.fillRect(QRectF(0, EDGE - 26, c.W, 4), mix(pal.app, hex("#6a6a70"), 0.35));
        // plataforma com a faixa amarela
        x.fillRect(QRectF(0, EDGE, c.W, c.H - EDGE), mix(pal.app, hex("#3a3a40"), 0.5));
        x.fillRect(QRectF(0, EDGE + 6, c.W, 10), hex("#e8c23a"));
        for (double xx = 0; xx < c.W; xx += 8) x.fillRect(QRectF(xx, EDGE + 8, 3, 6), alpha(Qt::black, 0.25));
        x.end();
        m_back.set(c, pm);
    }
    Cache m_back;
    int m_st = 0;
    double m_t = 0, m_flick = 0, m_idle = 1.5, m_x = 0, m_v = 0, m_tt = 0;
};

// ============================================================ lanchonete da madrugada

class Diner : public Layer {
public:
    void init(Ctx&) override { m_back.reset(); m_front.reset(); m_car = -1e9; m_next = 5; }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_sip = std::fmod(m_sip + dt, 9.0);
        m_next -= dt;
        if (m_car < -1e8 && m_next <= 0) { m_car = c.W + 60; m_cv = R(160, 230); }
        if (m_car > -1e8) { m_car -= m_cv * dt; if (m_car < -300) { m_car = -1e9; m_next = R(8, 16); } }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double o = zx(c, 45) - 45, DX0 = 30 + o, DX1 = 410 + o, WY0 = 360, WY1 = 480, BY0 = 300, BY1 = 560, ST = 600;
        if (!m_back.valid(c)) build(pal, c, DX0, DX1, WY0, WY1, BY0, BY1, ST);
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // garçonete atrás do balcão, andando com a jarra de café
        const double wv = std::cos(m_t * 0.22), wx = DX0 + 60 + (DX1 - DX0 - 120) * (0.5 + 0.5 * std::sin(m_t * 0.22));
        Fig wo; wo.pose = Fig::Walk; wo.ph = m_t * 2.6; wo.hair = Fig::Pony; wo.arm = Fig::Hold; wo.side = wv > 0 ? 1 : -1; wo.build = 0.9; wo.col = hex("#5a3038");
        figure(p, wx, WY1 + 18, 112, wo);
        p.fillRect(QRectF(wx - 9, WY1 + 18 - 112 * 0.6, 18, 20), hex("#f4f0e8"));
        const QPointF jug = figureHand(wx, WY1 + 18, 112, wo);
        QPolygonF j;
        j << QPointF(jug.x() - 6, jug.y()) << QPointF(jug.x() + 6, jug.y()) << QPointF(jug.x() + 8, jug.y() + 14) << QPointF(jug.x() - 8, jug.y() + 14);
        p.setPen(Qt::NoPen);
        p.setBrush(alpha(hex("#3a2010"), 0.9));
        p.drawPolygon(j);
        // o balcão esconde as pernas dela
        p.fillRect(QRectF(DX0, WY1 - 34, DX1 - DX0, 10), hex("#8a3a2a"));
        p.fillRect(QRectF(DX0, WY1 - 36, DX1 - DX0, 3), hex("#d8d8d8"));
        p.fillRect(QRectF(DX0, WY1 - 24, DX1 - DX0, 24), hex("#b07850"));
        for (double sx = DX0 + 30; sx < DX1 - 20; sx += 44) { p.fillRect(QRectF(sx - 9, WY1 - 18, 18, 6), hex("#c8302a")); p.fillRect(QRectF(sx - 1.5, WY1 - 12, 3, 12), hex("#9a9aa0")); }
        // três no balcão, de costas pra rua; o do meio toma café de vez em quando
        const double sits[3] = { DX0 + 74, DX0 + 206, DX0 + 294 };
        const Fig::Hair hs[3] = { Fig::Cap, Fig::Short, Fig::Long };
        const double bs[3] = { 1.12, 1, 0.9 };
        for (int k = 0; k < 3; ++k) {
            const double lift = k == 1 ? qMax(0.0, std::sin(qMin(1.0, m_sip / 2) * M_PI)) : 0;
            Fig f; f.pose = Fig::Sit; f.hair = hs[k]; f.arm = k == 1 ? Fig::Cup : Fig::Counter; f.lift = lift; f.build = bs[k]; f.tilt = k == 2 ? -0.08 : 0; f.col = hex("#2a1814");
            figure(p, sits[k], WY1 - 20, 96, f);
            if (k == 1) { const double hx = sits[k] + lerp(0.1, 0.045, lift) * 96, hy = WY1 - 20 + 0.47 * 96 - lerp(0.62, 0.86, lift) * 96; p.fillRect(QRectF(hx - 4, hy - 9, 8, 9), hex("#f4f0e8")); }
        }
        blit(p, m_front.pm, 0, 0, c.W, c.H);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        // a luz do janelão na calçada e o reflexo na rua molhada
        QLinearGradient lg(0, BY1, 0, ST + 120);
        lg.setColorAt(0, alpha(hex("#ffcf8a"), 0.3));
        lg.setColorAt(1, alpha(hex("#ffcf8a"), 0));
        p.fillRect(QRectF(DX0 - 40, BY1, DX1 - DX0 + 80, ST + 120 - BY1), lg);
        for (int k = 0; k < 10; ++k) p.fillRect(QRectF(DX0 + 10 + std::sin(m_t * 2 + k) * 3, ST + 6 + k * 9, DX1 - DX0 - 20, 3), alpha(hex("#ffcf8a"), 0.14 - k * 0.012));
        // letreiro de neon: Diner vermelho e OPEN 24H azul que falha
        const double openOn = std::sin(m_t * 9) * std::sin(m_t * 2.3) > -0.85 ? 1 : 0.1;
        const double ny = BY0 - 62;
        QFont ft(QStringLiteral("Lora"));
        ft.setItalic(true);
        ft.setWeight(QFont::Bold);
        ft.setPixelSize(46);
        QPainterPath dn;
        dn.addText(0, 0, ft, QStringLiteral("Diner"));
        const QRectF db = dn.boundingRect();
        dn.translate((DX0 + DX1) / 2 - db.center().x(), ny - db.center().y());
        p.setBrush(Qt::NoBrush);
        p.setPen(pen(alpha(hex("#ff3a3a"), 0.35), 8));
        p.drawPath(dn);
        p.setPen(pen(hex("#ff6a5a"), 2));
        p.drawPath(dn);
        QFont fo(QStringLiteral("IBM Plex Sans"));
        fo.setWeight(QFont::Bold);
        fo.setPixelSize(18);
        QPainterPath op;
        op.addText(0, 0, fo, QStringLiteral("OPEN 24H"));
        const QRectF ob = op.boundingRect();
        op.translate((DX0 + DX1) / 2 - ob.center().x(), BY0 - 22 - ob.center().y());
        p.setPen(pen(alpha(hex("#4ad8ff"), 0.4 * openOn), 5));
        p.drawPath(op);
        p.setPen(pen(alpha(hex("#8ae8ff"), openOn), 1.5));
        p.drawPath(op);
        glow(p, (DX0 + DX1) / 2, ny, 220, hex("#ff4a4a"), 0.2);
        if (m_car > -1e8) {
            const double cy = ST + 40;
            QLinearGradient hg(m_car, 0, m_car - 280, 0);
            hg.setColorAt(0, alpha(hex("#fff4d6"), 0.35));
            hg.setColorAt(1, alpha(hex("#fff4d6"), 0));
            QPolygonF beam;
            beam << QPointF(m_car, cy - 3) << QPointF(m_car - 280, cy - 26) << QPointF(m_car - 280, cy + 22) << QPointF(m_car, cy + 4);
            p.setPen(Qt::NoPen);
            p.setBrush(hg);
            p.drawPolygon(beam);
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        if (m_car > -1e8) {
            p.fillRect(QRectF(m_car - 2, ST + 26, 64, 18), hex("#060608"));
            p.fillRect(QRectF(m_car + 14, ST + 16, 30, 12), hex("#060608"));
            p.fillRect(QRectF(m_car + 58, ST + 32, 4, 3), alpha(hex("#ff3020"), 0.9));
        }
        // vapor do café de quem está mais perto
        for (int i = 0; i < 3; ++i) p.fillRect(QRectF(DX0 + 230 + std::sin(m_t * 2 + i) * 3, WY1 - 60 - std::fmod(m_t * 14 + i * 10, 30.0), 2, 6), alpha(Qt::white, 0.12));
    }

private:
    void build(const Pal& pal, const Ctx& c, double DX0, double DX1, double WY0, double WY1, double BY0, double BY1, double ST)
    {
        QPixmap pm = layer(c, c.W, c.H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing, true);
        sky(x, c, mix(pal.app, hex("#05070e"), 0.5), mix(pal.app, hex("#14182a"), 0.4));
        const QColor bd = mix(pal.app, Qt::black, 0.4);
        for (double bx = DX1 + 10; bx < c.W;) { const double bw = R(80, 160), bh = R(200, 420); x.fillRect(QRectF(bx, ST - bh, bw, bh), bd); bx += bw + R(2, 8); }
        x.fillRect(QRectF(0, BY0 - 160, DX0, BY1), mix(pal.app, Qt::black, 0.3));
        x.fillRect(QRectF(0, BY1, c.W, ST - BY1), mix(pal.app, hex("#1a1a22"), 0.45));
        x.fillRect(QRectF(0, ST, c.W, c.H - ST), mix(pal.app, hex("#0c0c12"), 0.5));
        // dentro: parede do fundo
        QLinearGradient g(0, WY0, 0, WY1);
        g.setColorAt(0, hex("#f4d8a8"));
        g.setColorAt(1, hex("#e8b878"));
        x.fillRect(QRectF(DX0, WY0, DX1 - DX0, WY1 - WY0), g);
        x.fillRect(QRectF(DX0, WY0 + 30, DX1 - DX0, 6), hex("#c8a070"));
        for (int i = 0; i < 6; ++i) x.fillRect(QRectF(DX0 + 20 + i * 60, WY0 + 8, 40, 18), hex("#a8805a"));
        x.end();
        m_back.set(c, pm);
        // o vagão: lata, janelão, teto arredondado, com o vão da janela vazado
        QPixmap fp = layer(c, c.W, c.H);
        QPainter f(&fp);
        f.setRenderHint(QPainter::Antialiasing, true);
        QLinearGradient sg(0, BY0, 0, BY1);
        sg.setColorAt(0, hex("#c8ccd4"));
        sg.setColorAt(0.5, hex("#8a8e98"));
        sg.setColorAt(1, hex("#5a5e68"));
        QPainterPath box(QPointF(DX0, BY1));
        box.lineTo(DX0, BY0 + 20);
        box.quadTo(DX0, BY0, DX0 + 20, BY0);
        box.lineTo(DX1 - 20, BY0);
        box.quadTo(DX1, BY0, DX1, BY0 + 20);
        box.lineTo(DX1, BY1);
        box.closeSubpath();
        f.fillPath(box, sg);
        QVector<QLineF> rib;
        for (double y = BY0 + 30; y < BY1; y += 8) if (y <= WY0 - 6 || y >= WY1 + 6) rib.append(QLineF(DX0, y, DX1, y));
        f.setPen(QPen(alpha(Qt::white, 0.35), 1));
        f.drawLines(rib);
        f.fillRect(QRectF(DX0, WY1 + 10, DX1 - DX0, 8), hex("#c8302a"));
        f.setCompositionMode(QPainter::CompositionMode_Clear);
        f.fillRect(QRectF(DX0 + 10, WY0, DX1 - DX0 - 20, WY1 - WY0), Qt::transparent);
        f.setCompositionMode(QPainter::CompositionMode_SourceOver);
        for (double mx = DX0 + 10; mx <= DX1 - 10; mx += 62) f.fillRect(QRectF(mx - 2, WY0, 4, WY1 - WY0), hex("#5a5e68"));
        f.fillRect(QRectF(DX0 + 60, BY0 - 70, 4, 70), hex("#14141a"));
        f.fillRect(QRectF(DX1 - 64, BY0 - 70, 4, 70), hex("#14141a"));
        f.end();
        m_front.set(c, fp);
    }
    Cache m_back, m_front;
    double m_t = 0, m_sip = 0, m_next = 5, m_car = -1e9, m_cv = 200;
};

// ============================================================ cruzamento visto de cima (chuva de guarda-chuvas)

class Scramble : public Layer {
public:
    void init(Ctx& c) override
    {
        m_back.reset();
        m_cars.clear();
        for (int i = 0, n = int(std::lround(14 * INT(c))); i < n; ++i) m_cars.push_back(newCar(true, c));
        m_peds.clear();
        const QPointF C = center(c);
        static const char* UMB[] = { "#e8402a", "#2a7ad8", "#f2c23a", "#2a2a30", "#e8e8e8", "#3aa85a", "#8a3ad8", "#ff8ac8", "#1a1a1e", "#c8c8d0" };
        for (int i = 0, n = int(std::lround(150 * qMin(1.5, INT(c)))); i < n; ++i) {
            Ped q;
            q.sx = rnd() < 0.5 ? -1 : 1; q.sy = rnd() < 0.5 ? -1 : 1;
            const QPointF pt = corner(C, q.sx, q.sy);
            q.x = q.tx = pt.x(); q.y = q.ty = pt.y();
            q.v = R(34, 50); q.c = hex(UMB[RI(10)]); q.ph = R(0, 6.28);
            m_peds.push_back(q);
        }
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        const double ph = std::fmod(m_t, 24.0);
        const bool carsGo = ph < 13, walk = ph >= 14 && ph < 23;
        const QPointF C = center(c);
        for (Car& car : m_cars) {
            const double pos = car.hor ? car.x : car.y, mid = car.hor ? C.x() : C.y(), stopAt = mid - car.dir * (RW + 26);
            const bool before = car.dir > 0 ? pos < stopAt : pos > stopAt;
            const bool blocked = !carsGo && before && std::abs(pos - stopAt) < 40;
            if (!blocked) { if (car.hor) car.x += car.dir * car.v * dt; else car.y += car.dir * car.v * dt; }
            if (car.x < -80 || car.x > c.W + 80 || car.y < -80 || car.y > c.H + 80) car = newCar(false, c);
        }
        // pedestres: no verde, cada um atravessa pra uma esquina (às vezes na diagonal)
        if (walk) for (Ped& q : m_peds) {
            if (q.x == q.tx && q.y == q.ty && rnd() < dt * 2) {
                const double r = rnd();
                int nsx = q.sx, nsy = q.sy;
                if (r < 0.4) nsx = -q.sx; else if (r < 0.8) nsy = -q.sy; else { nsx = -q.sx; nsy = -q.sy; }
                const QPointF t = corner(C, nsx, nsy);
                q.tx = t.x(); q.ty = t.y(); q.sx = nsx; q.sy = nsy;
            }
        }
        for (Ped& q : m_peds) {
            const double dx = q.tx - q.x, dy = q.ty - q.y, d = std::hypot(dx, dy);
            if (d < 1) { q.x = q.tx; q.y = q.ty; continue; }
            const double st = qMin(d, q.v * dt);
            q.x += dx / d * st + std::sin(m_t * 3 + q.ph) * 0.3;
            q.y += dy / d * st;
        }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const QPointF C = center(c);
        const double cx = C.x(), cy = C.y();
        if (!m_back.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing, true);
            x.fillRect(QRectF(0, 0, c.W, c.H), mix(pal.app, hex("#3a3a40"), 0.4));
            const QColor blk = mix(pal.app, hex("#1a1a20"), 0.5);
            x.fillRect(QRectF(0, 0, cx - RW - 60, cy - RW - 60), blk);
            x.fillRect(QRectF(cx + RW + 60, 0, c.W, cy - RW - 60), blk);
            x.fillRect(QRectF(0, cy + RW + 60, cx - RW - 60, c.H), blk);
            x.fillRect(QRectF(cx + RW + 60, cy + RW + 60, c.W, c.H), blk);
            const QColor asph = mix(pal.app, hex("#14141a"), 0.55);
            x.fillRect(QRectF(0, cy - RW, c.W, RW * 2), asph);
            x.fillRect(QRectF(cx - RW, 0, RW * 2, c.H), asph);
            const QColor lane = alpha(hex("#e8e0a0"), 0.55);
            for (double xx = 0; xx < c.W; xx += 40) if (std::abs(xx - cx) > RW + 40) x.fillRect(QRectF(xx, cy - 1, 22, 2), lane);
            for (double yy = 0; yy < c.H; yy += 40) if (std::abs(yy - cy) > RW + 40) x.fillRect(QRectF(cx - 1, yy, 2, 22), lane);
            const QColor zebra = alpha(hex("#f4f4f0"), 0.75);
            for (double k = -RW + 6; k < RW; k += 14) {
                x.fillRect(QRectF(cx - RW - 38, cy + k, 30, 8), zebra);
                x.fillRect(QRectF(cx + RW + 8, cy + k, 30, 8), zebra);
                x.fillRect(QRectF(cx + k, cy - RW - 38, 8, 30), zebra);
                x.fillRect(QRectF(cx + k, cy + RW + 8, 8, 30), zebra);
            }
            for (double a : { 45.0, -45.0 }) {
                x.save();
                x.translate(cx, cy);
                x.rotate(a);
                for (double k = -RW * 1.3; k < RW * 1.3; k += 14) x.fillRect(QRectF(k, -22, 8, 44), zebra);
                x.restore();
            }
            x.setPen(Qt::NoPen);
            x.setBrush(mix(pal.app, hex("#1a3a24"), 0.5));
            for (int i = 0; i < 26; ++i) {
                const double tx = R(0, c.W), ty = rnd() < 0.5 ? cy - RW - R(8, 30) : cy + RW + R(8, 30);
                if (std::abs(tx - cx) < RW + 50) continue;
                const double r = R(10, 16);
                x.drawEllipse(QPointF(tx, ty), r, r);
            }
            x.end();
            m_back.set(c, pm);
        }
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // carros vistos de cima, com farol e lanterna
        for (const Car& car : m_cars) {
            p.save();
            p.translate(car.x, car.y);
            p.rotate(car.hor ? (car.dir > 0 ? 0 : 180) : (car.dir > 0 ? 90 : -90));
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            QLinearGradient g(20, 0, 80, 0);
            g.setColorAt(0, alpha(hex("#fff4d6"), 0.3));
            g.setColorAt(1, alpha(hex("#fff4d6"), 0));
            QPolygonF beam;
            beam << QPointF(20, -6) << QPointF(80, -18) << QPointF(80, 18) << QPointF(20, 6);
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawPolygon(beam);
            p.fillRect(QRectF(-22, -8, 3, 4), alpha(hex("#ff2a1a"), 0.8));
            p.fillRect(QRectF(-22, 4, 3, 4), alpha(hex("#ff2a1a"), 0.8));
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.fillRect(QRectF(-20, -9, 40, 18), mix(car.col, pal.app, 0.15));
            p.fillRect(QRectF(-6, -8, 16, 16), alpha(Qt::black, 0.35));
            p.fillRect(QRectF(-18, -8, 36, 3), alpha(Qt::white, 0.18));
            p.restore();
        }
        // pedestres: guarda-chuva na chuva, cabeça e ombro no seco
        const bool rain = c.rain != QLatin1String("none");
        p.setPen(Qt::NoPen);
        for (const Ped& q : m_peds) {
            if (rain) {
                p.setPen(Qt::NoPen);
                p.setBrush(mix(q.c, pal.app, 0.15));
                p.drawEllipse(QPointF(q.x, q.y), 7.5, 7.5);
                QVector<QLineF> ribs;
                for (int k = 0; k < 4; ++k) { const double a = k * M_PI / 4; ribs.append(QLineF(q.x - std::cos(a) * 7.5, q.y - std::sin(a) * 7.5, q.x + std::cos(a) * 7.5, q.y + std::sin(a) * 7.5)); }
                p.setPen(QPen(alpha(Qt::black, 0.25), 0.7));
                p.drawLines(ribs);
            } else {
                p.setPen(Qt::NoPen);
                p.setBrush(mix(q.c, hex("#2a2a30"), 0.6));
                p.drawEllipse(QPointF(q.x, q.y), 6, 3.5);
                p.setBrush(hex("#14100c"));
                p.drawEllipse(QPointF(q.x, q.y), 2.8, 2.8);
            }
        }
    }

private:
    static constexpr double RW = 70;
    struct Car { bool hor; int dir; double x, y, v; QColor col; };
    struct Ped { double x = 0, y = 0, tx = 0, ty = 0, v = 40, ph = 0; int sx = 1, sy = 1; QColor c; };
    static QPointF center(const Ctx& c) { return QPointF(zx(c, 230), 330); }
    static QPointF corner(QPointF C, int sx, int sy) { return QPointF(C.x() + sx * (RW + R(14, 46)), C.y() + sy * (RW + R(14, 46))); }
    static Car newCar(bool any, const Ctx& c)
    {
        static const char* CC[] = { "#e8e8ea", "#1a1a1e", "#c8302a", "#2a4a8a", "#e8c23a", "#8a8a90" };
        const QPointF C = center(c);
        const bool hor = rnd() < 0.6;
        const int dir = rnd() < 0.5 ? 1 : -1;
        const double lane = dir * (rnd() < 0.5 ? 20 : 48);
        if (hor) return { true, dir, any ? R(-60, c.W) : dir > 0 ? -60.0 : c.W + 60, C.y() + lane, R(90, 130), hex(CC[RI(6)]) };
        return { false, dir, C.x() - lane, any ? R(-60, c.H) : dir > 0 ? -60.0 : c.H + 60, R(90, 130), hex(CC[RI(5)]) };
    }
    Cache m_back;
    std::vector<Car> m_cars;
    std::vector<Ped> m_peds;
    double m_t = 0;
};

// ============================================================ Mulholland: a grade de luzes vista do morro

class Mulholland : public Layer {
public:
    void init(Ctx& c) override
    {
        m_back.reset();
        m_cars.clear();
        for (int i = 0, n = count(110, c); i < n; ++i) m_cars.push_back(newCar(true));
        m_heli = -1e9;
        m_next = 4;
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        for (auto& car : m_cars) { car[1] += -car[2] * car[3] * dt * (car[1] * 0.15 + 0.2); if (car[1] < 0.6 || car[1] > 40) car = newCar(false); }
        if (m_heli < -1e8) { m_next -= dt; if (m_next <= 0) { m_heli = -40; m_hy = R(80, 160); } }
        else { m_heli += 32 * dt; if (m_heli > c.W + 40) { m_heli = -1e9; m_next = R(18, 30); } }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double HZ = c.H * 0.4, F = 210, VX = zx(c, 320);
        auto P = [&](double X, double z) { return QPointF(VX + X / z * F, HZ + 1 / z * F); };
        if (!m_back.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing, true);
            vfill(x, QRectF(0, 0, c.W, HZ), mix(pal.app, hex("#0a0c20"), 0.45), mix(pal.app, hex("#6a3a4a"), 0.4), 0, HZ);
            x.drawPixmap(0, 0, starLayer(c, 120, HZ * 0.7, 0.5));
            fillProfile(x, c, profile(7, 0.5), HZ - 18, 22, mix(pal.app, hex("#1a1424"), 0.5), 6);
            x.fillRect(QRectF(0, HZ, c.W, c.H - HZ), mix(pal.app, hex("#06060a"), 0.55));
            // a grade: ruas que fogem pro horizonte e as transversais, em lotes por cor/brilho
            QVector<QRectF> bk[3][6];
            auto put = [&](int col, double a, const QRectF& r) { bk[col][qBound(0, int(a * 6), 5)].append(r); };
            for (double X = -42; X <= 42.01; X += 0.6) {
                const bool big = std::abs(X / 3 - std::round(X / 3)) < 0.01;
                for (double z = 0.6; z < 40; z *= big ? 1.02 : 1.04) {
                    const QPointF s = P(X, z);
                    if (s.x() < -5 || s.x() > c.W + 5 || s.y() > c.H) continue;
                    const double sz = qMax(0.9, 2.6 / std::sqrt(z)), a = (big ? 0.95 : 0.7) * qMin(1.0, 4 / z + 0.35);
                    const int col = big ? (rnd() < 0.5 ? 2 : 1) : (rnd() < 0.9 ? 0 : 2);
                    put(col, a, QRectF(s.x(), s.y(), sz, sz));
                }
            }
            for (double z = 0.7; z < 40; z *= 1.09)
                for (double X = -42; X <= 42; X += 0.12) {
                    const QPointF s = P(X, z);
                    if (s.x() < 0 || s.x() > c.W || rnd() >= 0.55) continue;
                    const double sz = qMax(0.8, 1.8 / std::sqrt(z));
                    put(0, 0.6 * qMin(1.0, 4 / z + 0.3), QRectF(s.x(), s.y(), sz, sz));
                }
            static const char* GC[] = { "#ffa040", "#ffc070", "#fff2d8" };
            x.setPen(Qt::NoPen);
            for (int col = 0; col < 3; ++col) for (int i = 0; i < 6; ++i) if (!bk[col][i].isEmpty()) { x.setBrush(alpha(hex(GC[col]), (i + 0.5) / 6)); x.drawRects(bk[col][i]); }
            QLinearGradient glow2(0, HZ - 60, 0, HZ + 40);
            glow2.setColorAt(0, alpha(hex("#ff9a50"), 0));
            glow2.setColorAt(1, alpha(hex("#ff9a50"), 0.18));
            x.fillRect(QRectF(0, HZ - 60, c.W, 100), glow2);
            // o centro com prédios no horizonte, à esquerda
            const QColor bc = mix(pal.app, hex("#0a0a12"), 0.55);
            for (int i = 0; i < 14; ++i) {
                const double bx = zx(c, 230) + R(-90, 90), bh = R(30, 110), bw = R(10, 20);
                x.fillRect(QRectF(bx, HZ + 4 - bh, bw, bh), bc);
                for (double y = HZ - bh + 6; y < HZ; y += 5) for (double k = 2; k < bw - 2; k += 4) if (rnd() < 0.35) x.fillRect(QRectF(bx + k, y, 1.5, 2), alpha(hex("#fff2d8"), 0.7));
            }
            // o morro na frente, com o carro antigo estacionado no mirante
            const double o = zx(c, 45) - 45;
            QPainterPath hill(QPointF(-10, c.H));
            hill.lineTo(-10, c.H - 130);
            hill.quadTo(250 + o, c.H - 170, 520 + o, c.H - 110);
            hill.quadTo(700 + o, c.H - 80, c.W + 10, c.H - 60);
            hill.lineTo(c.W + 10, c.H);
            hill.closeSubpath();
            const QColor dark = mix(pal.app, hex("#030305"), 0.7);
            x.fillPath(hill, dark);
            const double cx = 120 + o, cy = c.H - 150;
            QPainterPath car(QPointF(cx, cy));
            car.lineTo(cx + 6, cy - 22);
            car.quadTo(cx + 20, cy - 30, cx + 50, cy - 30);
            car.lineTo(cx + 70, cy - 46); car.lineTo(cx + 110, cy - 46); car.lineTo(cx + 120, cy - 30); car.lineTo(cx + 190, cy - 26);
            car.quadTo(cx + 210, cy - 20, cx + 212, cy);
            car.closeSubpath();
            car.addEllipse(QPointF(cx + 40, cy), 14, 14);
            car.addEllipse(QPointF(cx + 170, cy), 14, 14);
            car.setFillRule(Qt::WindingFill);
            x.fillPath(car, dark);
            QPainterPath rim(QPointF(cx + 6, cy - 22));
            rim.quadTo(cx + 20, cy - 30, cx + 50, cy - 30);
            rim.lineTo(cx + 70, cy - 46); rim.lineTo(cx + 110, cy - 46); rim.lineTo(cx + 120, cy - 30); rim.lineTo(cx + 190, cy - 26);
            rim.quadTo(cx + 210, cy - 20, cx + 212, cy);
            x.setPen(QPen(alpha(hex("#ffb070"), 0.5), 1.2));
            x.setBrush(Qt::NoBrush);
            x.drawPath(rim);
            x.end();
            m_back.set(c, pm);
        }
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // carros correndo pelas avenidas
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        QVector<QRectF> wh[4], rd[4];
        for (const auto& car : m_cars) {
            const QPointF s = P(car[0], car[1]);
            if (s.x() < 0 || s.x() > c.W || s.y() > c.H - 60) continue;
            const int lvl = qBound(0, int(qMin(1.0, 4 / car[1] + 0.3) * 4), 3);
            (car[2] > 0 ? wh : rd)[lvl].append(QRectF(s.x(), s.y(), 1.8, 1.8));
        }
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 4; ++i) {
            if (!wh[i].isEmpty()) { p.setBrush(alpha(hex("#fff8e8"), (i + 0.5) / 4)); p.drawRects(wh[i]); }
            if (!rd[i].isEmpty()) { p.setBrush(alpha(hex("#ff3a2a"), (i + 0.5) / 4)); p.drawRects(rd[i]); }
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // palmeiras balançando
        const double o = zx(c, 45) - 45;
        const double palms[3][3] = { { 360 + o, 330, 0 }, { 420 + o, 270, 1.3 }, { 60 + o, 300, 2.1 } };
        p.setBrush(Qt::NoBrush);
        for (const auto& pm : palms) {
            const double px = pm[0], h = pm[1], by = c.H - 140 + (pm[0] - o > 300 ? 20 : 0), top = by - h, sw = std::sin(m_t * 0.7 + pm[2]) * 6;
            p.setPen(pen(hex("#040406"), 5));
            QPainterPath trunk(QPointF(px, by));
            trunk.quadTo(px + 8, by - h * 0.5, px + sw, top);
            p.drawPath(trunk);
            p.setPen(pen(hex("#040406"), 3));
            for (int i = 0; i < 8; ++i) {
                const double a = -M_PI / 2 + (i / 7.0 - 0.5) * 3.6 + std::sin(m_t * 1.1 + i + pm[2]) * 0.06, l = 46 + (i % 3) * 8;
                QPainterPath f(QPointF(px + sw, top));
                f.quadTo(px + sw + std::cos(a) * l * 0.6, top + std::sin(a) * l * 0.6 - 10, px + sw + std::cos(a) * l, top + std::sin(a) * l + 18);
                p.drawPath(f);
            }
        }
        // helicóptero com holofote
        if (m_heli > -1e8) {
            const double hx = m_heli, hy = m_hy + std::sin(m_t) * 3;
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            QLinearGradient g(0, hy, 0, hy + 320);
            g.setColorAt(0, alpha(hex("#fff6e0"), 0.18));
            g.setColorAt(1, alpha(hex("#fff6e0"), 0));
            QPolygonF beam;
            beam << QPointF(hx - 2, hy + 4) << QPointF(hx - 50 + std::sin(m_t * 0.7) * 40, hy + 320) << QPointF(hx + 30 + std::sin(m_t * 0.7) * 40, hy + 320) << QPointF(hx + 2, hy + 4);
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawPolygon(beam);
            p.fillRect(QRectF(hx - 1, hy + 4, 2.5, 2.5), alpha(hex("#ff3030"), int(m_t * 1.5) % 2 ? 0.9 : 0.2));
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.setBrush(hex("#05050a"));
            p.drawEllipse(QPointF(hx, hy), 8, 4);
            p.fillRect(QRectF(hx - 18, hy - 1, 10, 2), hex("#05050a"));
        }
    }

private:
    static std::array<double, 4> newCar(bool any)
    {
        const double X = std::round(R(-36, 36) / 3) * 3, dir = rnd() < 0.5 ? 1 : -1;
        return { X + (dir > 0 ? 0.07 : -0.07), any ? R(0.6, 40) : dir > 0 ? 40.0 : 0.6, dir, R(1.5, 3) };
    }
    Cache m_back;
    std::vector<std::array<double, 4>> m_cars;
    double m_t = 0, m_heli = -1e9, m_hy = 100, m_next = 4;
};

} // namespace

std::unique_ptr<Layer> makeLeva4(const QString& id)
{
    if (id == QLatin1String("canal")) return std::make_unique<Canal>();
    if (id == QLatin1String("mansard")) return std::make_unique<Mansard>();
    if (id == QLatin1String("metro")) return std::make_unique<Metro>();
    if (id == QLatin1String("diner")) return std::make_unique<Diner>();
    if (id == QLatin1String("scramble")) return std::make_unique<Scramble>();
    if (id == QLatin1String("mulholland")) return std::make_unique<Mulholland>();
    return nullptr;
}

} // namespace AnimBg
