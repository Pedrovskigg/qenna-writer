// Cenas urbanas dos temas Animados: a cidade (e as variações Paulista,
// trem, porto), a cidadezinha, o cyberpunk da vinheta, o anos 80 e a
// placa de neon do Neon Rain. Portado de city_v.js/fx.js do Weather Lab.

#include "AnimBackgroundEngines.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>

#include <algorithm>
#include <vector>

namespace AnimBg {

using namespace fx;

namespace {

QPen roundPen(const QColor& c, double w)
{
    QPen pen(c, w);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    return pen;
}

void disc(QPainter& p, double x, double y, double r, const QColor& c)
{
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(QPointF(x, y), r, r);
}

// Fatia de um cache em resolução física (o reflexo na água usa isso).
void slice(QPainter& p, const QPixmap& pm, double px, double srcY, double dx, double dy, double w, double h)
{
    p.drawPixmap(QRectF(dx, dy, w, h), pm, QRectF(0, srcY * px, w * px, h * px));
}

// x de uma peça fixa dentro de cada pedaço de mesa à vista.
double inLeft(const Ctx& c, double frac, double fallback)
{
    const QRectF& l = c.zones.left;
    return l.width() >= 120 ? l.left() + l.width() * frac : fallback;
}
double inRight(const Ctx& c, double frac, double fallback)
{
    const QRectF& r = c.zones.right;
    return r.width() >= 120 ? r.left() + r.width() * frac : fallback;
}

// ============================================================ cidade (e variações)

class City : public Layer {
public:
    City(bool towers, bool helis, bool train, bool water) : m_towers(towers), m_helis(helis), m_train(train), m_water(water) {}

    void init(Ctx& c) override
    {
        m_cache.pm = m_cache.dark = m_cache.lit = QPixmap();
        m_builtFor.clear();
        m_helisList.clear();
        m_trainOn = false;
        m_nextTrain = 1.5;
        m_cars.clear();
        const int n = count(9, c);
        for (int i = 0; i < n; ++i) { m_cars.push_back(newCar(1, true, c)); m_cars.push_back(newCar(-1, true, c)); }
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_toggle += dt;
        m_nextPlane -= dt;
        for (Car& car : m_cars) {
            car.x += car.dir * car.v * dt;
            if (car.x > c.W + 50 || car.x < -50) car = newCar(car.dir, false, c);
        }
        if (!m_helis && m_nextPlane <= 0 && !m_plane.on) { m_plane = { true, -40, R(60, 180), R(40, 60) }; m_nextPlane = R(14, 30); }
        if (m_plane.on) { m_plane.x += m_plane.v * dt; m_plane.y -= 4 * dt; if (m_plane.x > c.W + 40) m_plane.on = false; }
        if (m_train) {
            m_nextTrain -= dt;
            if (!m_trainOn && m_nextTrain <= 0) {
                m_trainDir = rnd() < 0.5 ? 1 : -1;
                m_trainX = m_trainDir > 0 ? -620 : c.W + 20;
                m_trainV = R(230, 300);
                m_trainOn = true;
            }
            if (m_trainOn) {
                m_trainX += m_trainDir * m_trainV * dt;
                if (m_trainX > c.W + 40 || m_trainX < -660) { m_trainOn = false; m_nextTrain = R(6, 13); }
            }
        }
        if (m_helis) {
            m_nextHeli -= dt;
            if (m_nextHeli <= 0 && m_helisList.size() < 2) {
                const int dir = rnd() < 0.5 ? 1 : -1;
                m_helisList.push_back({ dir, dir > 0 ? -30.0 : c.W + 30, R(90, 250), R(30, 55), R(0, 6.28), rnd() < 0.5 });
                m_nextHeli = R(8, 18);
            }
            for (Heli& h : m_helisList) h.x += h.dir * h.v * dt;
            m_helisList.erase(std::remove_if(m_helisList.begin(), m_helisList.end(), [&](const Heli& h) { return h.x < -60 || h.x > c.W + 60; }), m_helisList.end());
        }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double ROAD = road(c), TR = ROAD - 92;
        skyTOD(p, pal, c, mix(pal.app, Qt::black, 0.35), mix(pal.app, hex(m_towers ? "#4a3a3a" : "#5a3a40"), 0.35));
        if (m_builtFor != c.tod || m_cache.pm.isNull() || qAbs(m_cache.px - c.px) > 0.01) { build(pal, c); m_builtFor = c.tod; }
        vfill(p, QRectF(0, ROAD - 260, c.W, 260), alpha(hex("#ff9a5a"), 0), alpha(hex("#ff9a5a"), 0.12 * (1 - m_L)), ROAD - 260, ROAD);
        if (m_toggle > 0.25 && !m_wins.empty()) {
            m_toggle = 0;
            QPainter x(&m_cache.pm);
            for (int i = 0; i < 3; ++i) {
                Win& w = m_wins[size_t(RI(int(m_wins.size())))];
                w.on = !w.on && rnd() < qMin(1.0, w.p / qMax(0.05, 1 - w.p));   // a cidade fica tão acesa quanto nasceu
                copyRect(x, w.on ? m_cache.lit : m_cache.dark, QRectF(w.x, w.y, w.w, w.h));
            }
        }
        blit(p, m_cache.pm, 0, 0, c.W, c.H);
        if (m_water) {
            // água: o reflexo da cidade, de cabeça pra baixo, tremendo em faixas
            p.fillRect(QRectF(0, ROAD, c.W, c.H - ROAD), mix(mix(pal.app, Qt::black, 0.45), hex("#3f6f8f"), m_L * 0.6));
            p.setOpacity(0.42);
            for (double y = ROAD; y < c.H; y += 3) {
                const double d = y - ROAD, src = ROAD - d * 1.3 - 3, sh = std::sin(m_t * 2.2 + y * 0.21) * (1.5 + d * 0.05);
                if (src > 0) slice(p, m_cache.pm, m_cache.px, src, sh, y, c.W, 3);
            }
            p.setOpacity(1);
            vfill(p, QRectF(0, ROAD, c.W, c.H - ROAD), alpha(pal.app, 0), alpha(pal.app, 0.45), ROAD, c.H);
        }
        for (const Ant& a : m_ants) {
            const double k = std::pow(qMax(0.0, std::sin(m_t * 2.2 + a.ph)), 6);
            disc(p, a.x, a.y, 2.8, alpha(hex("#ff3b3b"), (0.3 + 0.7 * k) * (1 - 0.5 * m_L)));
            if (k > 0.3) disc(p, a.x, a.y, 12, alpha(hex("#ff3b3b"), 0.22 * k));
        }
        if (m_trainOn) {
            const double cw = 82;
            const int n = 7;
            const QColor dark = mix(pal.app, Qt::black, 0.5), lit = alpha(hex("#ffe2a8"), 0.9 * (1 - 0.7 * m_L));
            for (int i = 0; i < n; ++i) {
                const double cx = m_trainX + i * (cw + 4);
                if (cx > c.W + 10 || cx + cw < -10) continue;
                p.fillRect(QRectF(cx, TR - 24, cw, 22), dark);
                for (double wx = cx + 6; wx < cx + cw - 8; wx += 11) p.fillRect(QRectF(wx, TR - 19, 7, 7), lit);
            }
            const double fx = m_trainDir > 0 ? m_trainX + n * (cw + 4) : m_trainX;
            QRadialGradient hg(QPointF(fx, TR - 12), 70);
            hg.setColorAt(0, alpha(hex("#fff6d8"), 0.5 * (1 - 0.6 * m_L)));
            hg.setColorAt(1, alpha(hex("#fff6d8"), 0));
            p.fillRect(QRectF(fx - 70, TR - 82, 140, 140), hg);
        }
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.setOpacity(1 - 0.65 * m_L);
        for (const Car& car : m_cars) {
            const QColor col = hex(car.dir > 0 ? "#fff4d6" : "#ff4a3a");
            const double len = car.dir > 0 ? 46 : 14, x1 = car.x + car.dir * len;
            QLinearGradient g(car.x, 0, x1, 0);
            g.setColorAt(0, alpha(col, car.dir > 0 ? 0.35 : 0.5));
            g.setColorAt(1, alpha(col, 0));
            p.fillRect(QRectF(qMin(car.x, x1), car.lane - 2, len, 4), g);
            p.fillRect(QRectF(car.x - 1.5, car.lane - 1.5, 3, 3), alpha(col, 0.95));
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setOpacity(1);
        for (const Heli& h : m_helisList) {
            const double y = h.y + std::sin(m_t * 1.3 + h.ph) * 4;
            const QColor dk = mix(pal.app, Qt::black, 0.6);
            if (h.beam && m_L < 0.5) {
                QLinearGradient g(0, y, 0, y + 300);
                g.setColorAt(0, alpha(hex("#fff6e0"), 0.16));
                g.setColorAt(1, alpha(hex("#fff6e0"), 0));
                QPainterPath beam(QPointF(h.x - 2, y + 4));
                beam.lineTo(h.x - 40 - h.dir * 30, y + 300);
                beam.lineTo(h.x + 40 - h.dir * 30, y + 300);
                beam.lineTo(h.x + 2, y + 4);
                beam.closeSubpath();
                p.fillPath(beam, g);
            }
            p.setPen(Qt::NoPen);
            p.setBrush(dk);
            p.drawEllipse(QPointF(h.x, y), 9, 4.5);
            p.fillRect(QRectF(qMin(h.x, h.x - h.dir * 20), y - 1, 12, 2), dk);
            const double r = 14 * std::abs(std::sin(m_t * 40));
            p.setPen(QPen(dk, 1));
            p.drawLine(QPointF(h.x - r, y - 6), QPointF(h.x + r, y - 6));
            const bool b = int(std::floor(m_t * 1.6 + h.ph)) % 2 == 0;
            disc(p, h.x, y + 5, 1.8, alpha(hex("#ff3b3b"), b ? 0.95 : 0.25));
            if (std::sin(m_t * 9 + h.ph) > 0.92) disc(p, h.x - h.dir * 20, y, 1.8, alpha(Qt::white, 0.95));
        }
        if (m_plane.on) {
            const bool b = int(std::floor(m_t * 1.4)) % 2 == 0;
            p.fillRect(QRectF(m_plane.x, m_plane.y, 2, 2), alpha(Qt::white, b ? 0.95 : 0.3));
            p.fillRect(QRectF(m_plane.x - 8, m_plane.y + 1, 2, 2), alpha(hex("#ff4040"), b ? 0.3 : 0.9));
        }
    }

private:
    struct Win { double x, y, w, h; bool on; QColor c, bg; double p; };   // p: chance de nascer acesa
    struct Ant { double x, y, ph; };
    struct Car { int dir; double x, v, lane; };
    struct Plane { bool on = false; double x = 0, y = 0, v = 0; };
    struct Heli { int dir; double x, y, v, ph; bool beam; };

    double road(const Ctx& c) const { return m_water ? c.H * 0.8 : c.H - 52; }
    static QColor warm()
    {
        static const char* W[5] = { "#ffcf7a", "#ffd99a", "#ffe7b8", "#f4b860", "#bfe0ff" };
        return hex(W[RI(5)]);
    }
    Car newCar(int dir, bool any, const Ctx& c) const
    {
        const double ROAD = road(c), DECK = ROAD - 30;
        const double lane = m_water ? (dir > 0 ? DECK - 3 : DECK - 7) : (dir > 0 ? ROAD + 12 : ROAD + 24);
        return { dir, any ? R(0, c.W) : (dir > 0 ? -40.0 : c.W + 40), R(70, 140), lane };
    }
    // montado apagado e aceso (ver copyRect); o cache começa apagado e recebe as acesas do sorteio
    void build(const Pal& pal, const Ctx& c)
    {
        m_L = LVopen(pal, c);
        {
            Seeded sd(0xC17E5EEDu);
            m_cache.dark = scene(pal, c, false);
        }
        {
            Seeded sd(0xC17E5EEDu);
            m_cache.lit = scene(pal, c, true);
        }
        m_cache.pm = m_cache.dark;
        QPainter x(&m_cache.pm);
        for (const Win& w : m_wins)
            if (w.on) copyRect(x, m_cache.lit, QRectF(w.x, w.y, w.w, w.h));
        m_cache.px = c.px;
    }
    QPixmap scene(const Pal& pal, const Ctx& c, bool lit)
    {
        const double ROAD = road(c), TR = ROAD - 92, DECK = ROAD - 30;
        QPixmap pm = layer(c, c.W, c.H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing);
        m_wins.clear();
        m_ants.clear();
        const QColor haze = c.tod == QLatin1String("day") ? hex("#8fa4b8") : c.tod == QLatin1String("dusk") ? hex("#6a4a62") : QColor(Qt::black);
        const QColor tint = m_towers ? mix(pal.app, hex("#5a5e66"), 0.18) : pal.app;
        auto colK = [&](int k) {
            static const double D[3] = { 0.05, 0.25, 0.45 }, HZ[3] = { 0.7, 0.5, 0.3 };
            const QColor base = mix(tint, Qt::black, D[k]);
            if (c.tod == QLatin1String("theme") || c.tod == QLatin1String("night")) return base;
            return mix(base, haze, qMin(1.0, HZ[k] * m_L * 1.6));
        };
        for (int k = 0; k < 3; ++k) {
            double bx = R(-30, 0);
            const QColor col = colK(k);
            const double sc = m_water ? 0.75 : 1;
            static const double OFF[3] = { -30, -10, 8 };
            while (bx < c.W) {
                const double bw = (k == 0 ? R(40, 80) : k == 1 ? R(50, 100) : R(60, 130)) * (m_towers ? 0.85 : 1);
                const double bh = (k == 0 ? R(160, 340) : k == 1 ? R(120, 420) : R(90, 360)) * sc * (m_towers ? 1.1 : 1);
                const double by = ROAD - bh + OFF[k] * sc;
                x.fillRect(QRectF(bx, by, bw, ROAD - by + 20), col);
                if (rnd() < 0.3) x.fillRect(QRectF(bx + bw * 0.3, by - 14, bw * 0.4, 14), col);
                if (bh > (m_towers ? 220 : 300) && k > 0) {
                    m_ants.push_back({ bx + bw / 2, by - 40, R(0, 6.28) });
                    x.fillRect(QRectF(bx + bw / 2 - 1, by - 40, 2, 26), col);
                }
                if (k > 0) {
                    const double s = k == 2 ? 11 : 8, ww = k == 2 ? 5 : 3.5, wh = k == 2 ? 6 : 4.5;
                    const QColor bg = m_L > 0.5 ? mix(col, hex("#c8d8e6"), 0.18) : col;
                    for (double wy = by + 10; wy < ROAD - 14; wy += s)
                        for (double wx = bx + 6; wx < bx + bw - 6; wx += s) {
                            const double pOn = (m_towers ? 0.24 : 0.32) * (1 - 0.85 * m_L);
                            const bool on = rnd() < pOn;
                            Win w{ wx, wy, ww, wh, on, (m_towers && rnd() < 0.4) ? hex("#e8f0ff") : warm(), bg, pOn };
                            m_wins.push_back(w);
                            if (lit) x.fillRect(QRectF(w.x, w.y, w.w, w.h), w.c);
                        }
                }
                bx += bw + (k == 0 ? R(0, 6) : k == 1 ? R(2, 14) : R(8, 30));
            }
            // torres de TV da Paulista, nos pedaços de mesa à vista
            if (m_towers && k == 1) {
                const double txs[4][2] = { { inLeft(c, 0.35, c.W * 0.12), R(440, 520) }, { inLeft(c, 0.8, c.W * 0.23), R(380, 450) },
                                           { inRight(c, 0.3, c.W * 0.82), R(420, 500) }, { inRight(c, 0.75, c.W * 0.94), R(380, 460) } };
                const QColor tc = mix(col, hex("#9aa0aa"), c.tod == QLatin1String("day") ? 0.05 : 0.22);
                x.setPen(QPen(tc, 2.2));
                x.setBrush(Qt::NoBrush);
                for (const auto& tw : txs) {
                    const double tx = tw[0], th = tw[1], top = ROAD - th;
                    auto half = [&](double yy) { return 14 * (yy - top) / th + 2; };
                    QPainterPath legs(QPointF(tx - half(ROAD), ROAD));
                    legs.lineTo(tx - 2, top);
                    legs.lineTo(tx + 2, top);
                    legs.lineTo(tx + half(ROAD), ROAD);
                    x.drawPath(legs);
                    for (double yy = top + 20; yy < ROAD; yy += 20) {
                        const double a = half(yy), b = half(yy + 20);
                        x.drawLine(QPointF(tx - a, yy), QPointF(tx + b, yy + 20));
                        x.drawLine(QPointF(tx + a, yy), QPointF(tx - b, yy + 20));
                        x.drawLine(QPointF(tx - a, yy), QPointF(tx + a, yy));
                    }
                    x.fillRect(QRectF(tx - 1, top - 50, 2, 50), tc);
                    const double ph = R(0, 6.28);
                    for (double yy : { top - 50, top, top + th * 0.35 }) m_ants.push_back({ tx, yy, ph });
                }
                x.setPen(Qt::NoPen);
            }
        }
        if (!m_water) {
            x.fillRect(QRectF(0, ROAD, c.W, c.H - ROAD), mix(pal.app, Qt::black, 0.55));
            const QColor rail = mix(pal.app, Qt::black, 0.35);
            x.fillRect(QRectF(0, ROAD - 3, c.W, 3), rail);
            for (double px = 40; px < c.W; px += 160) x.fillRect(QRectF(px, ROAD, 10, c.H - ROAD), rail);
        }
        if (m_train) {
            const QColor tcol = mix(pal.app, Qt::black, 0.6);
            x.fillRect(QRectF(0, TR, c.W, 9), tcol);
            for (double px = 70; px < c.W; px += 130) x.fillRect(QRectF(px, TR + 9, 12, ROAD - TR - 9), tcol);
        }
        if (m_water) {
            // ponte pênsil iluminada, uma torre em cada pedaço de mesa à vista
            const QColor bc = mix(mix(pal.app, hex("#7d8ea0"), 0.22), Qt::black, 0.2);
            const QColor lights = alpha(hex("#ffe6b0"), 0.95 * (1 - 0.8 * m_L));
            const double top = DECK - 190, b1 = inLeft(c, 0.55, c.W * 0.17), b2 = inRight(c, 0.5, c.W * 0.87);
            x.fillRect(QRectF(0, DECK, c.W, 9), bc);
            for (double bt : { b1, b2 }) {
                x.fillRect(QRectF(bt - 9, top, 6, DECK - top + 60), bc);
                x.fillRect(QRectF(bt + 3, top, 6, DECK - top + 60), bc);
                x.fillRect(QRectF(bt - 9, top + 30, 18, 5), bc);
                x.fillRect(QRectF(bt - 9, top + 100, 18, 5), bc);
                m_ants.push_back({ bt, top - 4, 0 });
            }
            auto cable = [&](double xx) {
                if (xx < b1) return top + (DECK - top) * std::pow((b1 - xx) / b1, 1.6) * 0.95;
                if (xx > b2) return top + (DECK - top) * std::pow((xx - b2) / (c.W - b2), 1.6) * 0.95;
                const double u = (xx - (b1 + b2) / 2) / ((b2 - b1) / 2);
                return DECK - 12 - (DECK - 12 - top) * u * u;
            };
            QPainterPath cab(QPointF(0, cable(0)));
            for (double xx = 6; xx <= c.W; xx += 6) cab.lineTo(xx, cable(xx));
            x.setBrush(Qt::NoBrush);
            x.setPen(QPen(bc, 2));
            x.drawPath(cab);
            x.setPen(QPen(bc, 1));
            for (double xx = 10; xx < c.W; xx += 18) x.drawLine(QPointF(xx, cable(xx)), QPointF(xx, DECK));
            x.setPen(Qt::NoPen);
            x.setBrush(lights);
            for (double xx = 8; xx < c.W; xx += 22) x.drawEllipse(QPointF(xx, cable(xx)), 2.2, 2.2);
            for (double xx = 4; xx < c.W; xx += 30) x.drawEllipse(QPointF(xx, DECK - 1), 1.8, 1.8);
            x.setCompositionMode(QPainter::CompositionMode_Plus);
            QLinearGradient lg(0, DECK - 30, 0, DECK + 10);
            lg.setColorAt(0, alpha(hex("#ffc46b"), 0));
            lg.setColorAt(1, alpha(hex("#ffc46b"), 0.18 * (1 - m_L)));
            x.fillRect(QRectF(0, DECK - 30, c.W, 40), lg);
        }
        x.end();
        return pm;
    }

    struct { QPixmap pm, dark, lit; double px = 0; } m_cache;
    bool m_towers, m_helis, m_train, m_water;
    QString m_builtFor;
    std::vector<Win> m_wins;
    std::vector<Ant> m_ants;
    std::vector<Car> m_cars;
    std::vector<Heli> m_helisList;
    Plane m_plane;
    double m_t = 0, m_toggle = 0, m_nextPlane = 4, m_nextHeli = 1, m_L = 0;
    bool m_trainOn = false;
    int m_trainDir = 1;
    double m_trainX = 0, m_trainV = 0, m_nextTrain = 2;
};

// ============================================================ cidadezinha

class Town : public Layer {
public:
    void init(Ctx&) override { m_pm = m_dark = m_lit = QPixmap(); m_smoke.clear(); }
    void step(double dt, Ctx&) override
    {
        m_t += dt;
        m_toggle += dt;
        if (!m_chimneys.empty() && rnd() < dt * m_chimneys.size() * 1.6) {
            const Chim& ch = m_chimneys[size_t(RI(int(m_chimneys.size())))];
            m_smoke.push_back({ ch.x, ch.y, 3 * ch.s, 0.35, R(4, 10), -R(12, 20) * ch.s, ch.s });
        }
        for (Puff& s : m_smoke) {
            s.x += (s.vx + std::sin(m_t * 0.7 + s.y * 0.05) * 4) * dt;
            s.y += s.vy * dt;
            s.r += 7 * s.s * dt;
            s.a -= 0.07 * dt;
        }
        m_smoke.erase(std::remove_if(m_smoke.begin(), m_smoke.end(), [](const Puff& s) { return s.a <= 0; }), m_smoke.end());
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        skyTOD(p, pal, c, mix(pal.app, Qt::black, 0.3), mix(pal.app, hex("#2a3550"), 0.3));
        if (m_builtFor != c.tod || m_pm.isNull() || qAbs(m_px - c.px) > 0.01) { build(pal, c); m_builtFor = c.tod; }
        if (m_toggle > 0.6 && !m_wins.empty()) {
            m_toggle = 0;
            Win& w = m_wins[size_t(RI(int(m_wins.size())))];
            w.on = !w.on && rnd() < 1 - 0.85 * m_L;
            QPainter x(&m_pm);
            copyRect(x, w.on ? m_lit : m_dark, QRectF(w.x, w.y, w.w, w.h));
        }
        blit(p, m_pm, 0, 0, c.W, c.H);
        const QColor sc = c.tod == QLatin1String("day") ? hex("#e8edf2") : mix(pal.app, hex("#c8ccd6"), 0.5);
        for (const Puff& s : m_smoke) disc(p, s.x, s.y, s.r, alpha(sc, qMax(0.0, s.a)));
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const QPointF& l : m_lamps) {
            QRadialGradient g(l, 90);
            g.setColorAt(0, alpha(hex("#ffc46b"), 0.5 * (1 - 0.8 * m_L)));
            g.setColorAt(1, alpha(hex("#ffc46b"), 0));
            p.fillRect(QRectF(l.x() - 90, l.y() - 90, 180, 180), g);
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    struct Chim { double x, y, s; };
    struct Puff { double x, y, r, a, vx, vy, s; };
    struct Win { double x, y, w, h; bool on; QColor bg; };
    // montado apagado e aceso (ver copyRect); o cache começa apagado e recebe as acesas do sorteio
    void build(const Pal& pal, const Ctx& c)
    {
        m_L = LVopen(pal, c);
        {
            Seeded sd(0x70A15EEDu);
            m_dark = scene(pal, c, false);
        }
        {
            Seeded sd(0x70A15EEDu);
            m_lit = scene(pal, c, true);
        }
        m_pm = m_dark;
        QPainter x(&m_pm);
        for (const Win& w : m_wins)
            if (w.on) copyRect(x, m_lit, QRectF(w.x, w.y, w.w, w.h));
        m_px = c.px;
    }
    QPixmap scene(const Pal& pal, const Ctx& c, bool lit)
    {
        const double STREET = c.H - 40;
        QPixmap pm = layer(c, c.W, c.H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing);
        x.setPen(Qt::NoPen);
        m_chimneys.clear();
        m_lamps.clear();
        m_wins.clear();
        const QColor haze = c.tod == QLatin1String("day") ? hex("#9fb2c4") : c.tod == QLatin1String("dusk") ? hex("#6a4a62") : QColor(Qt::black);
        // morros atrás
        QPainterPath hills(QPointF(0, c.H));
        for (double xx = 0; xx <= c.W + 20; xx += 20) hills.lineTo(xx, c.H * 0.5 + std::sin(xx * 0.004) * 40 + std::sin(xx * 0.011) * 14);
        hills.lineTo(c.W + 20, c.H);
        x.fillPath(hills, mix(mix(pal.app, Qt::black, 0.1), haze, m_L * 0.6));
        // igreja ao fundo
        const QColor cc = mix(mix(pal.app, Qt::black, 0.25), haze, m_L * 0.5);
        const double cx = inRight(c, 0.6, c.W * 0.9), cy = c.H * 0.5 + 10;
        x.fillRect(QRectF(cx - 16, cy - 120, 32, 140), cc);
        QPainterPath spire(QPointF(cx - 20, cy - 120));
        spire.lineTo(cx, cy - 190);
        spire.lineTo(cx + 20, cy - 120);
        spire.closeSubpath();
        x.fillPath(spire, cc);
        x.fillRect(QRectF(cx - 1, cy - 210, 2, 22), cc);
        x.fillRect(QRectF(cx - 7, cy - 203, 14, 2), cc);
        for (int k = 0; k < 3; ++k) {
            static const double D[3] = { 0.2, 0.38, 0.55 }, HZ[3] = { 0.5, 0.35, 0.2 }, SC[3] = { 0.55, 0.75, 1 };
            const QColor col = mix(mix(pal.app, Qt::black, D[k]), haze, m_L * HZ[k]);
            const double base = k == 0 ? c.H * 0.66 : k == 1 ? c.H * 0.8 : STREET, sc = SC[k];
            double hx = R(-30, 0);
            while (hx < c.W) {
                const double w = R(70, 120) * sc, hh = R(50, 80) * sc, roof = R(26, 46) * sc, top = base - hh;
                x.fillRect(QRectF(hx, top, w, hh + 30), col);
                QPainterPath rf(QPointF(hx - 6 * sc, top + 1));
                rf.lineTo(hx + w / 2, top - roof);
                rf.lineTo(hx + w + 6 * sc, top + 1);
                rf.closeSubpath();
                x.fillPath(rf, col);
                if (rnd() < 0.75) {
                    const double chx = hx + w * R(0.6, 0.78), chy = top - roof * 0.55;
                    x.fillRect(QRectF(chx, chy - 18 * sc, 9 * sc, 22 * sc), col);
                    m_chimneys.push_back({ chx + 4.5 * sc, chy - 18 * sc, sc });
                }
                const int wn = qMax(1, int(std::floor(w / (30 * sc))));
                for (int i = 0; i < wn; ++i) {
                    Win wv{ hx + (i + 0.5) * w / wn - 6 * sc, top + hh * 0.3, 12 * sc, 14 * sc, rnd() < 0.6 * (1 - 0.85 * m_L), mix(col, Qt::black, 0.2) };
                    m_wins.push_back(wv);
                    x.fillRect(QRectF(wv.x, wv.y, wv.w, wv.h), lit ? hex("#ffc870") : wv.bg);
                }
                hx += w + R(8, 30) * sc;
            }
        }
        const QColor street = mix(pal.app, Qt::black, 0.62);
        x.fillRect(QRectF(0, STREET, c.W, c.H - STREET), street);
        for (double lx = 60; lx < c.W; lx += 170) {
            x.fillRect(QRectF(lx - 1.5, STREET - 70, 3, 70), street);
            x.fillRect(QRectF(lx - 1.5, STREET - 70, 12, 3), street);
            m_lamps.push_back(QPointF(lx + 10, STREET - 66));
        }
        x.end();
        return pm;
    }
    QPixmap m_pm, m_dark, m_lit;
    double m_px = 0, m_L = 0, m_t = 0, m_toggle = 0;
    QString m_builtFor;
    std::vector<Chim> m_chimneys;
    std::vector<Puff> m_smoke;
    std::vector<QPointF> m_lamps;
    std::vector<Win> m_wins;
};

// ============================================================ cyberpunk (a vinheta do índice ilustrado, animada)
// Mesma receita da vinheta (MsVignette, família Cyberpunk): prédios num tom só,
// faixas finas de neon (A = matiz+160, B = matiz-70), letreiros só de contorno,
// antenas e carros voadores.

class Cyberpunk : public Layer {
public:
    void init(Ctx& c) override
    {
        m_hue = c.pal.app.hslHueF() >= 0 ? c.pal.app.hslHueF() * 360 : 270;
        c.hasNeonCols = true;
        c.neonA = hsl(m_hue + 160, 1, 0.62);
        c.neonB = hsl(m_hue - 70, 1, 0.65);
        m_pm = m_dark = m_lit = QPixmap();
        m_cars.clear();
        const int n = count(9, c);
        for (int i = 0; i < n; ++i) m_cars.push_back(newCar(true, c));
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_toggle += dt;
        for (Car& car : m_cars) {
            car.x += car.dir * car.v * dt;
            if (car.x < -40 || car.x > c.W + 40) car = newCar(false, c);
        }
        for (Sign& s : m_signs) {
            if (s.glitch > 0) s.glitch -= dt;
            else if (rnd() < dt * 0.12) s.glitch = R(0.15, 0.6);
        }
    }
    void paint(QPainter& p, const Pal&, Ctx& c) override
    {
        const double h = m_hue;
        if (m_pm.isNull() || qAbs(m_px - c.px) > 0.01) build(c);
        QRadialGradient bg(QPointF(c.W / 2, c.H / 2), c.W * 0.7, QPointF(c.W * 0.4, c.H * 0.3));
        bg.setColorAt(0, hsl(h, 0.32, 0.24));
        bg.setColorAt(1, hsl(h + 20, 0.30, 0.10));
        p.fillRect(QRectF(0, 0, c.W, c.H), bg);
        // janelas que apagam e acendem
        if (m_toggle > 0.2 && !m_wins.empty()) {
            m_toggle = 0;
            QPainter x(&m_pm);
            for (int i = 0; i < 3; ++i) {
                Win& w = m_wins[size_t(RI(int(m_wins.size())))];
                w.on = !w.on;
                copyRect(x, w.on ? m_lit : m_dark, QRectF(w.x, w.y, 3, 4));
            }
        }
        blit(p, m_pm, 0, 0, c.W, c.H);
        // faixa de neon que zumbe: apaga por um instante
        for (const Strip& s : m_strips)
            if (std::sin(m_t * 13 + s.ph * 7) > 0.985 || (std::sin(m_t * 0.7 + s.ph) > 0.97 && rnd() < 0.5))
                p.fillRect(s.r.adjusted(-1, -1, 1, 1), s.shade);
        // letreiros: só o contorno, com halo, e às vezes dão defeito
        p.setBrush(Qt::NoBrush);
        for (const Sign& s : m_signs) {
            if (s.glitch > 0 && rnd() < 0.6) continue;
            const double a = 0.85 + 0.15 * std::sin(m_t * 3 + s.ph);
            p.setPen(QPen(alpha(s.c, 0.18 * a), 9));
            p.drawRect(s.r);
            p.setPen(QPen(alpha(s.c, 0.35 * a), 4.5));
            p.drawRect(s.r);
            p.setPen(QPen(alpha(s.c, a), 2));
            p.drawRect(s.r);
            for (int i = 0; i < 2; ++i)
                p.fillRect(QRectF(s.r.left() + 7, s.r.top() + 8 + i * (s.r.height() * 0.42), (s.r.width() - 14) * (0.5 + 0.4 * std::sin(m_t * 0.8 + i + s.ph)), 2),
                           alpha(s.c, 0.55 * a));
        }
        const QColor B = hsl(h - 70, 1, 0.65), A = hsl(h + 160, 1, 0.62);
        for (const Ant& a : m_ants) {
            const double k = std::pow(qMax(0.0, std::sin(m_t * 2 + a.ph)), 4);
            disc(p, a.x, a.y, 2.6, alpha(B, 0.35 + 0.65 * k));
        }
        // carros voadores: o traço B com o farolzinho A
        for (const Car& car : m_cars) {
            p.fillRect(QRectF(car.x - 16, car.y - 2, 32, 8), alpha(B, 0.2));
            p.fillRect(QRectF(car.x - 14, car.y, 28, 4), B);
            disc(p, car.x + car.dir * 14, car.y + 2, 2.6, A);
            QLinearGradient g(car.x, 0, car.x - car.dir * 60, 0);
            g.setColorAt(0, alpha(B, 0.35));
            g.setColorAt(1, alpha(B, 0));
            p.fillRect(QRectF(qMin(car.x, car.x - car.dir * 60), car.y + 1, 60, 2), g);
        }
    }

private:
    struct Strip { QRectF r; QColor shade; double ph; };
    struct Sign { QRectF r; QColor c; double glitch, ph; };
    struct Win { double x, y; bool on; QColor shade; };
    struct Ant { double x, y, ph; };
    struct Car { int dir; double x, y, v; };
    static Car newCar(bool any, const Ctx& c)
    {
        const int dir = rnd() < 0.5 ? 1 : -1;
        return { dir, any ? R(0, c.W) : (dir > 0 ? -30.0 : c.W + 30), R(60, c.H * 0.55), R(70, 170) };
    }
    // montado apagado e aceso (ver copyRect) com a mesma semente, sorteada a cada montagem
    void build(const Ctx& c)
    {
        const quint32 seed = QRandomGenerator::global()->generate();
        {
            Seeded sd(seed);
            m_dark = scene(c, false);
        }
        {
            Seeded sd(seed);
            m_lit = scene(c, true);
        }
        m_pm = m_dark;
        QPainter x(&m_pm);
        for (const Win& w : m_wins)
            if (w.on) copyRect(x, m_lit, QRectF(w.x, w.y, 3, 4));
        m_px = c.px;
    }
    QPixmap scene(const Ctx& c, bool lit)
    {
        const double h = m_hue;
        QPixmap pm = layer(c, c.W, c.H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing);
        m_strips.clear();
        m_signs.clear();
        m_wins.clear();
        m_ants.clear();
        const QColor A = hsl(h + 160, 1, 0.62), B = hsl(h - 70, 1, 0.65), win = hsl(h, 0.6, 0.7, 0.8), faint = hsl(h, 0.40, 0.55, 0.45);
        // três planos: o de trás mais claro e liso, o da frente com todos os detalhes da vinheta
        for (int k = 0; k < 3; ++k) {
            double bx = R(-20, 0);
            while (bx < c.W) {
                const double w = k == 0 ? R(50, 90) : k == 1 ? R(45, 85) : R(40, 80);
                const double bh = k == 0 ? R(380, 640) : k == 1 ? R(300, 600) : R(180, 520), top = c.H - bh;
                static const double SH[3] = { 0.2, 0.14, 0.08 };
                const QColor shade = hsl(h, 0.30, SH[k] + rnd() * 0.06);
                x.fillRect(QRectF(bx, top, w, bh), shade);
                if (k > 0) {
                    const QColor strip = rnd() < 0.5 ? A : B;
                    auto neonRect = [&](const QRectF& r) {
                        x.fillRect(r.adjusted(-5, -5, 5, 5), alpha(strip, 0.08));
                        x.fillRect(r.adjusted(-2.2, -2.2, 2.2, 2.2), alpha(strip, 0.18));
                        x.fillRect(r.adjusted(-1, -1, 1, 1), alpha(strip, 0.35));
                        x.fillRect(r, strip);
                        m_strips.push_back({ r, shade, R(0, 6.28) });
                    };
                    if (rnd() < 0.7) neonRect(QRectF(bx, top + R(30, bh * 0.5), w, 2.4));
                    if (rnd() < 0.4) neonRect(QRectF(bx + w - 2.6, top, 2.6, bh * R(0.3, 0.8)));
                    for (double yy = top + 14; yy < c.H - 6; yy += 11)
                        for (double xx = bx + 5; xx < bx + w - 5; xx += 8)
                            if (rnd() < 0.18) { m_wins.push_back({ xx, yy, true, shade }); x.fillRect(QRectF(xx, yy, 3, 4), lit ? win : shade); }
                    if (rnd() < 0.35) {
                        const double ah = R(26, 70);
                        x.setPen(QPen(faint, 1.4));
                        x.drawLine(QPointF(bx + w / 2, top), QPointF(bx + w / 2, top - ah));
                        x.setPen(Qt::NoPen);
                        m_ants.push_back({ bx + w / 2, top - ah, R(0, 6.28) });
                    }
                    if (k == 2 && rnd() < 0.3) {
                        const double sw = w * 0.9, sh = R(26, 46);
                        m_signs.push_back({ QRectF(bx + (w - sw) / 2, top + R(40, bh * 0.6), sw, sh), rnd() < 0.5 ? A : B, 0, R(0, 6.28) });
                    }
                }
                bx += w * 0.95;
            }
            // névoa entre os planos, na cor do fundo
            static const double MA[3] = { 0.35, 0.2, 0 };
            if (MA[k] > 0) vfill(x, QRectF(0, 0, c.W, c.H), hsl(h, 0.32, 0.24, 0), hsl(h, 0.32, 0.24, MA[k]), c.H * 0.35, c.H);
        }
        x.end();
        return pm;
    }
    QPixmap m_pm, m_dark, m_lit;
    double m_px = 0, m_hue = 270, m_t = 0, m_toggle = 0;
    std::vector<Strip> m_strips;
    std::vector<Sign> m_signs;
    std::vector<Win> m_wins;
    std::vector<Ant> m_ants;
    std::vector<Car> m_cars;
};

// ============================================================ anos 80

class RetroGrid : public Layer {
public:
    void init(Ctx&) override { m_sky.pm = QPixmap(); m_stars.pm = QPixmap(); m_floor.pm = QPixmap(); }
    void step(double dt, Ctx& c) override { m_t += dt * (0.7 + 0.3 * INT(c)); }
    void paint(QPainter& p, const Pal&, Ctx& c) override
    {
        const double HZ = c.H * 0.6, SX = anchorX(c, 0.5), SR = 120, SY = HZ - 125;
        QLinearGradient g(0, 0, 0, HZ);
        g.setColorAt(0, hex("#12061f"));
        g.setColorAt(0.6, hex("#4a1260"));
        g.setColorAt(1, hex("#ff4f8b"));
        p.fillRect(QRectF(0, 0, c.W, HZ), g);
        if (m_stars.pm.isNull() || qAbs(m_stars.px - c.px) > 0.01) {
            QPixmap pm = layer(c, c.W, HZ);
            QPainter x(&pm);
            for (int i = 0; i < int(std::lround(140 * c.W / 1440)); ++i) x.fillRect(QRectF(R(0, c.W), R(0, HZ * 0.6), 1.3, 1.3), alpha(Qt::white, R(0.2, 0.7)));
            x.end();
            m_stars.pm = pm;
            m_stars.px = c.px;
        }
        blit(p, m_stars.pm, 0, 0, c.W, HZ);
        // sol listrado: as faixas descem devagar
        p.save();
        QPainterPath sun;
        sun.addEllipse(QPointF(SX, SY), SR, SR);
        p.setClipPath(sun, Qt::IntersectClip);
        QLinearGradient sg(0, SY - SR, 0, SY + SR);
        sg.setColorAt(0, hex("#ffe66b"));
        sg.setColorAt(0.6, hex("#ff7a4f"));
        sg.setColorAt(1, hex("#ff2f8f"));
        p.fillRect(QRectF(SX - SR, SY - SR, SR * 2, SR * 2), sg);
        for (int i = 0; i < 7; ++i) {
            const double y = SY + 6 + std::fmod(i * 16 + m_t * 8, 112.0), hh = 2 + (y - SY) * 0.08;
            p.fillRect(QRectF(SX - SR, y, SR * 2, hh), hex("#4a1260"));
        }
        p.restore();
        QRadialGradient gl(QPointF(SX, SY), SR * 2.4);
        gl.setColorAt(0, alpha(hex("#ff5fa8"), 0.35));
        gl.setColorAt(0.8 / 2.4, alpha(hex("#ff5fa8"), 0.35));
        gl.setColorAt(1, alpha(hex("#ff5fa8"), 0));
        p.fillRect(QRectF(0, 0, c.W, HZ), gl);
        // silhueta da cidade na frente do sol
        if (m_sky.pm.isNull() || qAbs(m_sky.px - c.px) > 0.01) {
            QPixmap pm = layer(c, c.W, HZ);
            QPainter x(&pm);
            double bx = 0;
            while (bx < c.W) {
                const double bw = R(24, 60), bh = R(24, 95);
                x.fillRect(QRectF(bx, HZ - bh, bw, bh), hex("#1a0628"));
                if (rnd() < 0.3) x.fillRect(QRectF(bx + bw / 2 - 1, HZ - bh - 24, 2, 24), hex("#1a0628"));
                bx += bw + R(0, 6);
            }
            x.end();
            m_sky.pm = pm;
            m_sky.px = c.px;
        }
        blit(p, m_sky.pm, 0, 0, c.W, HZ);
        // chão quadriculado em perspectiva, correndo na sua direção
        // O chão e as linhas que fogem pro horizonte não andam: imagem pronta.
        if (m_floor.pm.isNull() || qAbs(m_floor.px - c.px) > 0.01) {
            QPixmap pm = layer(c, c.W, c.H - HZ);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing);
            x.fillRect(QRectF(0, 0, c.W, c.H - HZ), hex("#12051c"));
            const double vx = c.W / 2;
            QVector<QLineF> rays;
            const int span = int(std::ceil(c.W / 2 / 160)) + 6;
            for (int i = -span * 4; i <= span * 4; ++i) {
                const double bottom = vx + i * 160;
                if (bottom < -c.W || bottom > c.W * 2) continue;
                rays << QLineF(vx + i * 14, 0, bottom, c.H - HZ);
            }
            x.setPen(QPen(alpha(hex("#ff3ea5"), 0.22), 5));
            x.drawLines(rays);
            x.setPen(QPen(alpha(hex("#ff3ea5"), 0.85), 1.4));
            x.drawLines(rays);
            x.end();
            m_floor.pm = pm;
            m_floor.px = c.px;
        }
        blit(p, m_floor.pm, 0, HZ, c.W, c.H - HZ);
        const double off = std::fmod(m_t * 0.6, 1.0);
        QVector<QLineF> rows;
        for (int j = 0; j < 14; ++j) {
            const double z = (j + 1 - off) / 14, y = HZ + (c.H - HZ) * std::pow(z, 2.2);
            rows << QLineF(0, y, c.W, y);
        }
        p.setPen(QPen(alpha(hex("#ff3ea5"), 0.22), 5));
        p.drawLines(rows);
        p.setPen(QPen(alpha(hex("#ff3ea5"), 0.85), 1.4));
        p.drawLines(rows);
        vfill(p, QRectF(0, HZ, c.W, 60), alpha(hex("#ff4f8b"), 0.45), alpha(hex("#ff4f8b"), 0), HZ, HZ + 60);
    }

private:
    struct { QPixmap pm; double px = 0; } m_sky, m_stars, m_floor;
    double m_t = 0;
};

// ============================================================ placa de neon (Neon Rain)

class NeonSign : public Layer {
public:
    void init(Ctx&) override { m_glyphs.clear(); m_open.clear(); }
    void step(double dt, Ctx&) override
    {
        m_t += dt;
        m_nextBuzz -= dt;
        if (m_nextBuzz <= 0) { m_buzz = R(0.4, 1.6); m_nextBuzz = R(3, 8); }
        if (m_buzz > 0) m_buzz -= dt;
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const QColor pink = hex("#ff4fa3"), cyan = hex("#37e2f0");
        skyTOD(p, pal, c, mix(pal.app, Qt::black, 0.3), mix(pal.app, hex("#1a0d24"), 0.3));
        if (m_glyphs.empty() || qAbs(m_px - c.px) > 0.01) {
            m_glyphs.clear();
            m_open.clear();
            for (QChar ch : QStringLiteral("HOTEL")) m_glyphs.push_back(glyph(c, ch, pink, 54));
            for (QChar ch : QStringLiteral("OPEN")) m_open.push_back(glyph(c, ch, cyan, 26));
            m_px = c.px;
        }
        const double sx = anchorX(c, 0.78), broke = m_buzz > 0 && rnd() < 0.55;
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(alpha(pink, 0.25), 2));
        p.drawRect(QRectF(sx - 40, 120, 80, 5 * 64 + 20));
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (size_t i = 0; i < m_glyphs.size(); ++i) {
            p.setOpacity(i == 1 && broke ? 0.08 : 0.95);
            blit(p, m_glyphs[i], sx - 54, 130 + i * 64 - 22, 108, 108);
        }
        const double ox = c.zones.right.width() >= 160 ? c.zones.right.left() + c.zones.right.width() * 0.35 : c.W - 140;
        for (size_t i = 0; i < m_open.size(); ++i) {
            p.setOpacity(0.9);
            blit(p, m_open[i], ox + i * 26 - 26, c.H - 36 - 26, 52, 52);
        }
        p.setOpacity(1);
        // reflexo no chão molhado
        const double on = broke ? 0.75 : 1;
        const double spots[2][3] = { { sx, 0, 0.28 * on }, { ox + 40, 1, 0.2 } };
        for (const auto& s : spots) {
            const QColor col = s[1] == 0 ? pink : cyan;
            QRadialGradient g(QPointF(s[0], c.H), 260);
            g.setColorAt(0, alpha(col, s[2]));
            g.setColorAt(1, alpha(col, 0));
            p.fillRect(QRectF(s[0] - 260, c.H - 260, 520, 260), g);
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    static QPixmap glyph(const Ctx& c, QChar ch, const QColor& col, double size)
    {
        QPixmap pm = layer(c, size * 2, size * 2);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing);
        QFont f(QStringLiteral("IBM Plex Sans"));
        f.setPixelSize(int(size));
        f.setWeight(QFont::Bold);
        QPainterPath path;
        path.addText(0, 0, f, QString(ch));
        const QRectF b = path.boundingRect();
        path.translate(size - b.center().x(), size - b.center().y());
        x.setBrush(Qt::NoBrush);
        const double glows[3][2] = { { size * 0.3, 0.07 }, { size * 0.16, 0.16 }, { size * 0.07, 0.4 } };
        for (const auto& g : glows) { x.setPen(roundPen(alpha(col, g[1]), g[0])); x.drawPath(path); }
        x.setPen(roundPen(col, 3));
        x.drawPath(path);
        x.setPen(roundPen(mix(col, Qt::white, 0.7), 1.2));
        x.drawPath(path);
        x.end();
        return pm;
    }
    std::vector<QPixmap> m_glyphs, m_open;
    double m_px = 0, m_t = 0, m_buzz = 0, m_nextBuzz = 2;
};

} // namespace

std::unique_ptr<Layer> makeCity() { return std::make_unique<City>(false, false, false, false); }
std::unique_ptr<Layer> makeTown() { return std::make_unique<Town>(); }
std::unique_ptr<Layer> makeCyberpunk() { return std::make_unique<Cyberpunk>(); }
std::unique_ptr<Layer> makeRetroGrid() { return std::make_unique<RetroGrid>(); }
std::unique_ptr<Layer> makeNeonSign() { return std::make_unique<NeonSign>(); }

} // namespace AnimBg
