#include "AnimBackground.h"
#include "AnimBackgroundEngines.h"
#include "AnimKit.h"

#include "Theme.h"

#include <QCache>
#include <QElapsedTimer>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QTransform>
#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>
#include <QSettings>
#include <QTime>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <functional>
#include <map>
#include <vector>

namespace AnimBg {

// ============================================================ ferramentas

namespace fx {

QColor hex(const char* s) { return QColor(QString::fromLatin1(s)); }

QColor mix(const QColor& a, const QColor& b, double k)
{
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * k),
                            float(a.greenF() + (b.greenF() - a.greenF()) * k),
                            float(a.blueF() + (b.blueF() - a.blueF()) * k));
}

QColor alpha(QColor c, double a)
{
    c.setAlphaF(float(qBound(0.0, a, 1.0)));
    return c;
}

double luma(const QColor& c) { return 0.2126 * c.redF() + 0.7152 * c.greenF() + 0.0722 * c.blueF(); }

double smooth(double a, double b, double x)
{
    const double k = qBound(0.0, (x - a) / (b - a), 1.0);
    return k * k * (3 - 2 * k);
}

double INT(const Ctx& c)
{
    static const double v[3] = { 0.45, 1.0, 1.8 };
    return v[qBound(0, c.intensity, 2)];
}

namespace { QRandomGenerator* s_rng = nullptr; }

QRandomGenerator* rng() { return s_rng ? s_rng : QRandomGenerator::global(); }

Seeded::Seeded(quint32 seed) : m_gen(seed), m_prev(s_rng) { s_rng = &m_gen; }
Seeded::~Seeded() { s_rng = m_prev; }

TodW todW(double h)
{
    // noite até 5h, aurora 6h12, dia 7h36 às 16h36, entardecer 18h, noite a partir de 19h24
    static const struct { double h; char k; } K[] = { { 0, 'n' }, { 5, 'n' }, { 6.2, 'd' }, { 7.6, 'D' }, { 16.6, 'D' }, { 18, 'd' }, { 19.4, 'n' }, { 24, 'n' } };
    h = std::fmod(std::fmod(h, 24) + 24, 24);
    for (size_t i = 0; i + 1 < std::size(K); ++i) {
        if (h < K[i].h || h > K[i + 1].h) continue;
        const double t = smooth(0, 1, (h - K[i].h) / (K[i + 1].h - K[i].h));
        TodW w;
        w.night = 0;
        auto add = [&](char k, double v) { (k == 'D' ? w.day : k == 'd' ? w.dusk : w.night) += v; };
        add(K[i].k, 1 - t);
        add(K[i + 1].k, t);
        return w;
    }
    return {};
}

QString nearestTod(double h)
{
    const TodW w = todW(h);
    if (w.day >= w.dusk && w.day >= w.night) return QStringLiteral("day");
    return w.dusk >= w.night ? QStringLiteral("dusk") : QStringLiteral("night");
}

QColor todPick(const Ctx& c, const QColor& day, const QColor& dusk, const QColor& night)
{
    if (c.tod == QLatin1String("day")) return day;
    if (c.tod == QLatin1String("dusk")) return dusk;
    if (c.tod != QLatin1String("real")) return night;
    const TodW w = todW(c.hour);
    if (w.day > 0 && w.dusk > 0) return mix(dusk, day, w.day);
    if (w.dusk > 0 && w.night > 0) return mix(night, dusk, w.dusk);
    return w.day > 0.5 ? day : w.dusk > 0.5 ? dusk : night;
}

double todPick(const Ctx& c, double day, double dusk, double night)
{
    if (c.tod == QLatin1String("day")) return day;
    if (c.tod == QLatin1String("dusk")) return dusk;
    if (c.tod != QLatin1String("real")) return night;
    const TodW w = todW(c.hour);
    return day * w.day + dusk * w.dusk + night * w.night;
}

double lateK(const Ctx& c)
{
    if (c.tod != QLatin1String("real")) return 1;
    const double h = std::fmod(c.hour, 24), hh = h < 12 ? h + 24 : h;
    return 1 - 0.55 * smooth(23, 27, hh) + 0.55 * smooth(28, 30.5, hh);
}

double LV(const Pal& p, const Ctx& c)
{
    if (c.tod == QLatin1String("real")) return todPick(c, 1.0, 0.45, 0.0);
    if (c.tod == QLatin1String("day")) return 1;
    if (c.tod == QLatin1String("dusk")) return 0.45;
    if (c.tod == QLatin1String("night")) return 0;
    return p.dark ? 0 : 1;
}

double LVopen(const Pal& p, const Ctx& c) { return c.tod == QLatin1String("theme") ? 0 : LV(p, c); }

void vfill(QPainter& p, const QRectF& r, const QColor& a, const QColor& b, double y0, double y1)
{
    QLinearGradient g(0, y0, 0, y1);
    g.setColorAt(0, a);
    g.setColorAt(1, b);
    p.fillRect(r, g);
}

void sky(QPainter& p, const Ctx& c, const QColor& top, const QColor& bottom)
{
    vfill(p, QRectF(0, 0, c.W, c.H), top, bottom, 0, c.H);
}

void skyTOD(QPainter& p, const Pal& pal, const Ctx& c, const QColor& top, const QColor& bottom)
{
    if (c.tod == QLatin1String("theme")) {
        if (top.isValid()) sky(p, c, top, bottom);
        return;
    }
    if (c.tod == QLatin1String("day")) {
        sky(p, c, mix(hex("#6fa7da"), pal.app, 0.12), mix(hex("#dde9f1"), pal.app, 0.08));
        return;
    }
    if (c.tod == QLatin1String("night")) {
        sky(p, c, mix(pal.app, hex("#02040b"), 0.65), mix(pal.app, hex("#0b1424"), 0.55));
        return;
    }
    if (c.tod == QLatin1String("real")) {
        QLinearGradient g(0, 0, 0, c.H);
        g.setColorAt(0, todPick(c, mix(hex("#6fa7da"), pal.app, 0.12), mix(hex("#29285a"), pal.app, 0.15), mix(pal.app, hex("#02040b"), 0.65)));
        g.setColorAt(0.55, todPick(c, mix(hex("#a6c8e6"), pal.app, 0.1), mix(hex("#b25f7c"), pal.app, 0.1), mix(pal.app, hex("#070c18"), 0.6)));
        g.setColorAt(1, todPick(c, mix(hex("#dde9f1"), pal.app, 0.08), mix(hex("#f2a25a"), pal.app, 0.08), mix(pal.app, hex("#0b1424"), 0.55)));
        p.fillRect(QRectF(0, 0, c.W, c.H), g);
        return;
    }
    QLinearGradient g(0, 0, 0, c.H);
    g.setColorAt(0, mix(hex("#29285a"), pal.app, 0.15));
    g.setColorAt(0.55, mix(hex("#b25f7c"), pal.app, 0.1));
    g.setColorAt(1, mix(hex("#f2a25a"), pal.app, 0.08));
    p.fillRect(QRectF(0, 0, c.W, c.H), g);
}

QPixmap layer(const Ctx& c, double w, double h)
{
    QPixmap pm(qMax(1, qCeil(w * c.px)), qMax(1, qCeil(h * c.px)));
    pm.setDevicePixelRatio(c.px);
    pm.fill(Qt::transparent);
    return pm;
}

void copyRect(QPainter& x, const QPixmap& src, const QRectF& r)
{
    const double px = src.devicePixelRatio();
    const QRect d = QRectF(r.x() * px, r.y() * px, r.width() * px, r.height() * px).toAlignedRect() & QRect(QPoint(0, 0), src.size());
    if (d.isEmpty()) return;
    x.setCompositionMode(QPainter::CompositionMode_Source);
    x.drawPixmap(QRectF(d.x() / px, d.y() / px, d.width() / px, d.height() / px), src, QRectF(d));
    x.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

QPixmap softDot(const QColor& col, double inner)
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter x(&pm);
    QRadialGradient g(32, 32, 32);
    g.setColorAt(0, alpha(col, 1));
    g.setColorAt(inner, alpha(col, 0.8));
    g.setColorAt(1, alpha(col, 0));
    x.fillRect(pm.rect(), g);
    return pm;
}

QPointF visPoint(const Ctx& c)
{
    const QRectF l = c.zones.left.adjusted(15, 30, -15, -20), r = c.zones.right.adjusted(15, 30, -15, -20);
    const bool hasL = l.width() > 20 && l.height() > 20, hasR = r.width() > 20 && r.height() > 20;
    QRectF z;
    if (hasL && hasR) z = rnd() < l.width() / (l.width() + r.width()) ? l : r;
    else if (hasL) z = l;
    else if (hasR) z = r;
    else z = QRectF(0, 0, c.W, c.H);
    return QPointF(R(z.left(), z.right()), R(z.top(), z.bottom()));
}

double anchorX(const Ctx& c, double frac)
{
    const QRectF& l = c.zones.left;
    if (l.width() >= 120) return l.left() + l.width() * frac;
    const QRectF& r = c.zones.right;
    if (r.width() >= 120) return r.left() + r.width() * (1 - frac);
    return c.W * 0.2;
}

QColor hsl(double h, double s, double l, double a)
{
    double hh = std::fmod(h, 360.0);
    if (hh < 0) hh += 360;
    return QColor::fromHslF(float(hh / 360.0), float(qBound(0.0, s, 1.0)), float(qBound(0.0, l, 1.0)), float(qBound(0.0, a, 1.0)));
}

} // namespace fx

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

// Cache que se refaz quando a resolução física muda.
struct Cached {
    QPixmap pm;
    double px = 0;
    bool valid(const Ctx& c) const { return !pm.isNull() && qAbs(px - c.px) < 0.01; }
    void set(const Ctx& c, const QPixmap& p) { pm = p; px = c.px; }
    void reset() { pm = QPixmap(); }
};

// ============================================================ chuva

class Rain : public Layer {
public:
    explicit Rain(int mode) : m_mode(mode)
    {
        static const double C[3][6] = { { 260, 650, 900, 8, 26, 0.12 },
                                        { 420, 700, 950, 10, 28, 0.32 },
                                        { 420, 330, 420, 5, 13, 0.05 } };
        for (int i = 0; i < 6; ++i) m_cfg[i] = C[mode][i];
    }
    void init(Ctx& c) override
    {
        m_drops.assign(size_t(count(m_cfg[0], c)), Drop{});
        for (Drop& d : m_drops) spawn(d, true, c);
        m_splashes.clear();
        m_nextBolt = R(1.5, 3);
    }
    void step(double dt, Ctx& c) override
    {
        const double wind = m_cfg[5];
        for (Drop& d : m_drops) {
            d.y += d.v * dt;
            d.x += d.v * wind * dt;
            const double floor = c.H - R(0, 18) * (1 - d.z);
            if (d.y > floor) {
                if (d.z > 0.55 && rnd() < 0.55) m_splashes.push_back({ d.x, floor, 0, d.z, d.c });
                spawn(d, false, c);
            }
        }
        for (Splash& s : m_splashes) s.t += dt;
        m_splashes.erase(std::remove_if(m_splashes.begin(), m_splashes.end(), [](const Splash& s) { return s.t >= 0.28; }), m_splashes.end());
        if (m_mode == 1) {
            m_nextBolt -= dt;
            if (m_nextBolt <= 0) strike(c);
            if (m_flash > 0) { m_flash += dt; if (m_flash > 0.75) m_flash = 0; }
            if (m_boltLife > 0) m_boltLife -= dt;
        }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        double fl = 0;
        if (m_flash > 0) {
            static const double seq[4][2] = { { 0, 1 }, { 0.09, 0.35 }, { 0.2, 0.8 }, { 0.34, 0.25 } };
            for (const auto& s : seq) {
                const double k = (m_flash - s[0]) / 0.09;
                if (k > 0 && k < 2.2) fl = qMax(fl, s[1] * std::exp(-k * 1.6));
            }
        }
        if (m_mode == 1) p.fillRect(QRectF(0, 0, c.W, c.H), alpha(Qt::black, 0.18 * (1 - 0.5 * LV(pal, c))));
        const QColor dropCol = c.tod == QLatin1String("day") ? mix(hex("#dde9f1"), hex("#3a4a5a"), 0.5) : pal.part;
        const QColor NA = c.hasNeonCols ? c.neonA : hex("#ff4fa3"), NB = c.hasNeonCols ? c.neonB : hex("#37e2f0");
        auto col = [&](double k) { return c.neon ? (k < 0.28 ? NA : k < 0.45 ? NB : dropCol) : dropCol; };
        const double aMul = m_mode == 2 ? (pal.dark ? 0.9 : 1.25) : (pal.dark ? 1 : 0.8);
        const double wind = m_cfg[5];
        for (const Drop& d : m_drops) {
            p.setPen(roundPen(alpha(col(d.c), (0.1 + 0.42 * d.z) * aMul), (m_mode == 2 ? 0.5 : 0.6) + (m_mode == 2 ? 0.6 : 1.1) * d.z));
            p.drawLine(QPointF(d.x, d.y), QPointF(d.x - d.l * wind, d.y - d.l));
        }
        p.setBrush(Qt::NoBrush);
        for (const Splash& s : m_splashes) {
            const double k = s.t / 0.28, r = 2 + (m_mode == 2 ? 5 : 9) * k * s.z;
            p.setPen(QPen(alpha(col(s.c), 0.45 * (1 - k) * s.z), 1));
            p.drawArc(QRectF(s.x - r, s.y - r * 0.28, 2 * r, r * 0.56), 0, 180 * 16);
        }
        vfill(p, QRectF(0, c.H - 90, c.W, 90), alpha(pal.part, 0), alpha(pal.part, pal.dark ? 0.07 : 0.12), c.H - 90, c.H);
        if (fl > 0) p.fillRect(QRectF(0, 0, c.W, c.H), alpha(mix(Qt::white, pal.acc, 0.12), fl * 0.42));
        if (m_boltLife > 0) {
            const double a = qMin(1.0, m_boltLife / 0.25);
            const QColor glow = mix(Qt::white, pal.acc, 0.3);
            for (const Seg& s : m_bolt) {
                p.setPen(roundPen(alpha(glow, a * 0.3), s.w + 6));
                p.drawLine(s.a, s.b);
            }
            for (const Seg& s : m_bolt) {
                p.setPen(roundPen(alpha(Qt::white, a * (s.w > 2 ? 1 : 0.75)), s.w));
                p.drawLine(s.a, s.b);
            }
        }
    }

private:
    struct Drop { double x = 0, y = 0, z = 0, v = 0, l = 0, c = 0; };
    struct Splash { double x, y, t, z, c; };
    struct Seg { QPointF a, b; double w; };

    void spawn(Drop& d, bool any, const Ctx& c)
    {
        d.z = std::pow(rnd(), 1.6) * 0.85 + 0.15;
        d.x = R(-120, c.W + 40);
        d.y = any ? R(-c.H, c.H) : R(-120, -20);
        d.v = m_cfg[1] + m_cfg[2] * d.z;
        d.l = m_cfg[3] + m_cfg[4] * d.z;
        d.c = rnd();
    }
    void strike(const Ctx& c)
    {
        QRectF z = (rnd() < 0.65 || c.zones.right.width() < 60) ? c.zones.left : c.zones.right;
        if (z.width() < 60) z = QRectF(0, 0, c.W, c.H);
        m_bolt.clear();
        grow(R(z.left() + 20, qMax(z.left() + 21, z.right() - 20)), -10, R(-0.25, 0.25), R(c.H * 0.55, c.H * 0.95), 2.4, 0);
        m_boltLife = 0.55;
        m_flash = 0.0001;
        static const double div[3] = { 0.7, 1, 1.6 };
        m_nextBolt = R(4, 11) / div[qBound(0, c.intensity, 2)];
    }
    void grow(double x, double y, double ang, double len, double w, int depth)
    {
        double px = x, py = y;
        const int n = int(std::lround(len / 14));
        for (int i = 0; i < n; ++i) {
            const double nx = px + std::sin(ang) * 14 + R(-9, 9), ny = py + std::cos(ang) * 14;
            m_bolt.push_back({ QPointF(px, py), QPointF(nx, ny), w });
            px = nx; py = ny;
            if (depth < 2 && rnd() < 0.07) grow(px, py, ang + R(-0.9, 0.9), len * R(0.2, 0.45), w * 0.55, depth + 1);
        }
    }

    int m_mode;
    double m_cfg[6];
    std::vector<Drop> m_drops;
    std::vector<Splash> m_splashes;
    std::vector<Seg> m_bolt;
    double m_boltLife = 0, m_flash = 0, m_nextBolt = 3;
};

// ============================================================ neve

class Snow : public Layer {
public:
    void init(Ctx& c) override
    {
        static const double k[3] = { 0.4, 1, 2 };
        m_flakes.assign(size_t(qMax(1, int(std::lround(240 * k[qBound(0, c.intensity, 2)] * c.W / 1440)))), F{});
        for (F& f : m_flakes) spawn(f, true, c);
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        const bool strong = c.intensity == 2;
        const double w = (strong ? 70 : 22) * (std::sin(m_t * 0.17) * 0.6 + std::sin(m_t * 0.43 + 1) * 0.4) + (strong ? 55 : 8);
        for (F& f : m_flakes) {
            f.y += f.v * dt;
            f.x += (w * f.z + std::sin(m_t * f.f + f.ph) * 14 * f.z) * dt;
            if (f.y > c.H + 6) spawn(f, false, c);
            if (f.x > c.W + 70) f.x = -60;
            if (f.x < -70) f.x = c.W + 60;
        }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const QColor col = pal.dark ? mix(pal.app, Qt::white, 0.9) : mix(pal.app, pal.ink, 0.38);
        if (m_spriteFor != col) { m_sprite = softDot(col); m_spriteFor = col; }
        for (const F& f : m_flakes) {
            p.setOpacity((0.35 + 0.65 * f.z) * (pal.dark ? 1 : 0.75));
            const double s = f.r * 2.6;
            blit(p, m_sprite, f.x - s / 2, f.y - s / 2, s, s);
        }
        p.setOpacity(1);
        vfill(p, QRectF(0, c.H - 70, c.W, 70), alpha(col, 0), alpha(col, pal.dark ? 0.12 : 0.16), c.H - 70, c.H);
    }

private:
    struct F { double x = 0, y = 0, z = 0, v = 0, r = 0, ph = 0, f = 0; };
    void spawn(F& f, bool any, const Ctx& c)
    {
        f.z = std::pow(rnd(), 1.8) * 0.85 + 0.15;
        f.x = R(-60, c.W + 60);
        f.y = any ? R(-20, c.H) : R(-40, -8);
        f.v = 18 + 62 * f.z;
        f.r = 1 + 3.4 * f.z;
        f.ph = R(0, 6.28);
        f.f = R(0.5, 1.4);
    }
    std::vector<F> m_flakes;
    double m_t = 0;
    QPixmap m_sprite;
    QColor m_spriteFor;
};

// ============================================================ floresta (Firewatch)

class Forest : public Layer {
public:
    void init(Ctx& c) override
    {
        m_ridges = { ridge(c, c.H * 0.43, 120, 6), ridge(c, c.H * 0.53, 90, 6) };
        m_back.reset();
        m_far.reset();
        for (int k = 0; k < 4; ++k) {
            m_layers[k].clear();
            double x = R(-40, 0);
            while (x < c.W + 60) {
                Tree t;
                t.x = x;
                t.s = shape(R(HR[k][0], HR[k][1]), k);
                t.ph = R(0, 6.28);
                t.dy = R(2, 6) * DYK[k];
                m_layers[k].push_back(t);
                x += rnd() < 0.12 ? R(SP[k][1] * 2, SP[k][1] * 4) : R(SP[k][0], SP[k][1]);
            }
        }
        m_motes.clear();
        for (int i = 0; i < 60; ++i) m_motes.push_back({ R(0, c.W), R(c.H * 0.3, c.H), R(0.3, 1), R(0, 6.28) });
        m_mist.clear();
        for (int i = 0; i < 18; ++i) { static const double MY[3] = { 0.62, 0.7, 0.8 }; const int k = i % 3; m_mist.push_back({ k, R(-100, c.W), c.H * MY[k] + R(-30, 30), R(80, 180), R(5, 14), R(0.06, 0.12) }); }
        m_ferns.clear();
        const QRectF& zl = c.zones.left;
        const QRectF& zr = c.zones.right;
        for (int i = 0; i < 9; ++i) {
            const double fx = i < 6 ? (zl.width() > 60 ? R(zl.left(), zl.right()) : R(0, c.W * 0.25)) : (zr.width() > 60 ? R(zr.left(), zr.right()) : R(c.W * 0.75, c.W));
            m_ferns.push_back({ fx, R(50, 95), rnd() < 0.5 ? R(0.5, 1.1) : M_PI - R(0.5, 1.1), R(0, 6.28) });
        }
        m_birds.active = false;
        m_nextBirds = R(2, 5);
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_nextBirds -= dt;
        if (!m_birds.active && m_nextBirds <= 0) {
            m_birds.active = true;
            m_birds.x = -40;
            m_birds.y = R(c.H * 0.12, c.H * 0.32);
            m_birds.v = R(55, 80);
            m_birds.b.clear();
            const int n = 3 + RI(5);
            for (int i = 0; i < n; ++i)
                m_birds.b.push_back({ -(i * R(14, 22)), (i % 2 ? 1 : -1) * std::ceil(i / 2.0) * R(8, 12), R(0, 6.28) });
        }
        if (m_birds.active) {
            m_birds.x += m_birds.v * dt;
            m_birds.y += std::sin(m_t * 0.5) * 3 * dt;
            if (m_birds.x > c.W + 160) { m_birds.active = false; m_nextBirds = R(12, 26); }
        }
        for (Mist& m : m_mist) { m.x += m.v * (0.5 + gust(c, m_t, m.x)) * dt; if (m.x - m.r > c.W) m.x = -m.r; }
        for (Mote& m : m_motes) {
            m.x += (18 + 40 * gust(c, m_t, m.x)) * m.z * dt;
            m.y += std::sin(m_t * 1.1 + m.ph) * 8 * dt;
            if (m.x > c.W + 10) { m.x = -10; m.y = R(c.H * 0.3, c.H); }
        }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        QColor top, hor, deep, tint, star;
        palette(pal, c, top, hor, deep, tint, star);
        const bool night = c.tod == QLatin1String("night");
        auto colorAt = [&](int i) {
            static const double F[6] = { 0.16, 0.3, 0.46, 0.62, 0.78, 0.94 };
            const double f = F[i];
            return mix(mix(hor, tint, std::sin(f * M_PI) * 0.55), deep, f);
        };
        const double sx = anchorX(c, 0.5), sy = night ? c.H * 0.2 : c.H * 0.36;
        // Céu, astro e serras não se mexem: uma imagem pronta, refeita só
        // quando muda a hora do dia ou o tamanho.
        if (!m_back.valid(c) || m_backTod != c.tod) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing);
            vfill(x, QRectF(0, 0, c.W, c.H), top, hor, 0, c.H * 0.62);
            // astro atrás da serra, no pedaço de mesa que fica à vista
            const double sr = night ? 20 : 38;
            QRadialGradient sg(QPointF(sx, sy), sr * 7);
            sg.setColorAt(0, alpha(star, night ? 0.35 : 0.55));
            sg.setColorAt(1, alpha(star, 0));
            x.fillRect(QRectF(sx - sr * 7, sy - sr * 7, sr * 14, sr * 14), sg);
            disc(x, sx, sy, sr, star);
            if (night)
                for (int i = 0; i < 90; ++i)
                    x.fillRect(QRectF(std::fmod(i * 337.0, c.W), std::fmod(i * 191.0, c.H * 0.5), 1.3, 1.3), alpha(Qt::white, 0.7));
            for (int i = 0; i < 2; ++i) {
                QPainterPath path(QPointF(0, c.H));
                for (const QPointF& pt : m_ridges[i]) path.lineTo(pt);
                path.lineTo(c.W, c.H);
                x.fillPath(path, colorAt(i));
                if (i == 1) {
                    const double tx = anchorX(c, 0.78);
                    double ty = c.H;
                    for (size_t j = 1; j < m_ridges[1].size(); ++j)
                        if (m_ridges[1][j].x() >= tx) {
                            const QPointF a = m_ridges[1][j - 1], b = m_ridges[1][j];
                            ty = a.y() + (b.y() - a.y()) * (tx - a.x()) / qMax(1.0, b.x() - a.x());
                            break;
                        }
                    lookout(x, tx, ty + 2, colorAt(1), night ? alpha(hex("#ffd28a"), 0.95) : c.tod == QLatin1String("dusk") ? alpha(hex("#ffd28a"), 0.6) : colorAt(1));
                }
                const double y0 = c.H * (0.45 + i * 0.1), y1 = c.H * (0.68 + i * 0.08);
                vfill(x, QRectF(0, y0, c.W, c.H - y0), alpha(hor, 0), alpha(hor, 0.28), y0, y1);
            }
            x.end();
            m_back.set(c, pm);
            m_backTod = c.tod;
        }
        // As fileiras: árvore por árvore (um caminho só, com todas, é muito
        // mais lento de preencher no Qt). O chão fica abaixo da borda.
        const QString spriteKey = c.tod + QLatin1Char('#') + QString::number(c.px, 'f', 2);
        if (m_mistDot.isNull() || m_mistCol != hor) { m_mistDot = softDot(hor, 0.05); m_mistCol = hor; }
        // uma fileira: a de trás desenhada direto (vai pra imagem fixa); as outras, cada pinheiro uma imagem
        // pronta (com a contraluz) que o vento só entorta. Depois, a névoa assentada no pé da fileira.
        auto row = [&](QPainter& x, int k) {
            x.setPen(Qt::NoPen);
            const QColor col = colorAt(k + 2), pg = mix(col, hor, 0.04);
            const QColor rim = alpha(mix(col, star, 0.5), night ? 0.45 : 0.9);
            const double rdx = -1.2 - k * 0.7;
            for (Tree& t : m_layers[k]) {
                const double base = hill(c, k, t.x) + t.dy, sway = gust(c, m_t, t.x) * t.s.h * SWAY[k] + std::sin(m_t * 1.6 + t.ph) * t.s.h * 0.004;
                if (k <= 1) {
                    QPainterPath tp;
                    addTree(tp, t.x, base, t.s, 0);
                    if (k == 1) x.fillPath(tp.translated(rdx, -1), rim);
                    x.fillPath(tp, pg);
                    continue;
                }
                if (t.img.isNull() || t.imgKey != spriteKey) treeSprite(t, k, c, base, pg, rim, rdx, spriteKey);
                x.save();
                x.setRenderHint(QPainter::SmoothPixmapTransform, k >= 2);
                x.setTransform(QTransform(1, 0, -sway / t.s.h, 1, t.x, base), true);
                x.drawImage(t.imgRect, t.img);
                x.restore();
            }
            if (k < 3) {
                static const double Y0[3] = { 0.5, 0.42, 0.36 }, A[3] = { 0.3, 0.24, 0.18 };
                const double y0 = c.H * Y0[k];
                vfill(x, QRectF(0, y0, c.W, c.H - y0), alpha(hor, 0), alpha(hor, A[k]), y0, c.H);
            }
        };
        // névoa correndo entre as fileiras (imagem pronta, só desliza)
        auto mist = [&](QPainter& x, int k) {
            for (const Mist& m : m_mist)
                if (m.k == k) {
                    x.setOpacity(m.a);
                    blit(x, m_mistDot, m.x - m.r, m.y - m.r, m.r * 2, m.r * 2);
                }
            x.setOpacity(1);
        };
        // As duas fileiras de trás quase não balançam: imagens fixas, refeitas só quando muda a hora ou o tamanho.
        if (!m_far.valid(c) || m_farTod != c.tod) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing);
            row(x, 0);
            x.end();
            m_far.set(c, pm);
            QPixmap pm1 = layer(c, c.W, c.H);
            QPainter x1(&pm1);
            x1.setRenderHint(QPainter::Antialiasing);
            row(x1, 1);
            x1.end();
            m_mid.set(c, pm1);
            m_farTod = c.tod;
        }
        // Céu, serras, as duas fileiras de trás e os raios de luz numa imagem só. Os raios mexem
        // bem devagar: a imagem é refeita a cada 45 quadros (e quando muda a hora ou o tamanho).
        if (!m_raysPm.valid(c) || m_raysTod != c.tod || ++m_raysAge >= 45) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing);
            blit(x, m_back.pm, 0, 0, c.W, c.H);
            blit(x, m_far.pm, 0, 0, c.W, c.H);
            blit(x, m_mid.pm, 0, 0, c.W, c.H);
            rays(x, sx, sy, star, night ? 0.3 : 1);
            x.end();
            m_raysPm.set(c, pm);
            m_raysTod = c.tod;
            m_raysAge = 0;
        }
        blit(p, m_raysPm.pm, 0, 0, c.W, c.H);
        mist(p, 0);
        mist(p, 1);
        row(p, 2);
        mist(p, 2);
        row(p, 3);
        if (m_birds.active) {
            p.setPen(roundPen(colorAt(4), 1.6));
            p.setBrush(Qt::NoBrush);
            for (const Bird& b : m_birds.b) {
                const double x = m_birds.x + b.dx, y = m_birds.y + b.dy, f = std::sin(m_t * 7 + b.ph) * 3.5;
                QPainterPath w(QPointF(x - 7, y - f));
                w.quadTo(x - 3, y - 2, x, y);
                w.quadTo(x + 3, y - 2, x + 7, y - f);
                p.drawPath(w);
            }
        }
        // samambaias no primeiro plano, balançando com a rajada
        for (const Fern& f : m_ferns) fern(p, f.x, c.H + 6, f.len, f.ang, (gust(c, m_t, f.x) * 0.25 + std::sin(m_t * 1.7 + f.ph) * 0.06) * (std::cos(f.ang) >= 0 ? 1 : -1), colorAt(5));
        // poeira no ar: brilha mais dentro dos raios de luz
        for (const Mote& m : m_motes) {
            const double r2 = night ? 0 : inRay(sx, sy, m.x, m.y);
            p.setOpacity(qMin(1.0, (0.12 + 0.3 * m.z) * (night ? 0.5 : 1) + r2 * 0.8));
            disc(p, m.x, m.y, 1.4 * m.z * (1 + r2 * 0.6), alpha(star, 0.5));
        }
        p.setOpacity(1);
    }

private:
    struct Shape { std::vector<QPointF> R, L; double h = 0, st = 0, tw = 0; };
    struct Tree { double x = 0; Shape s; double ph = 0, dy = 0; QImage img; QRectF imgRect; QString imgKey; };
    // o pinheiro parado numa imagem (só a parte que aparece acima da borda), com a contraluz junto;
    // coordenadas locais: (0, 0) é o pé, y pra cima é negativo
    static void treeSprite(Tree& t, int k, const Ctx& c, double base, const QColor& col, const QColor& rim, double rdx, const QString& key)
    {
        QPainterPath tp;
        addTree(tp, 0, 0, t.s, 0);
        QRectF br = tp.boundingRect().adjusted(rdx - 2, -3, 2, 2);
        br.setBottom(qMin(br.bottom(), c.H + 12 - base));
        if (br.height() < 2 || br.width() < 2) { t.img = QImage(1, 1, QImage::Format_ARGB32_Premultiplied); t.img.fill(Qt::transparent); t.imgRect = QRectF(0, 0, 0, 0); t.imgKey = key; return; }
        QImage img(qMax(1, int(std::ceil(br.width() * c.px))), qMax(1, int(std::ceil(br.height() * c.px))), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter x(&img);
        x.setRenderHint(QPainter::Antialiasing);
        x.scale(c.px, c.px);
        x.translate(-br.left(), -br.top());
        if (k >= 1) x.fillPath(tp.translated(rdx, -1), rim);
        x.fillPath(tp, col);
        x.end();
        t.img = img;
        t.imgRect = br;
        t.imgKey = key;
    }
    struct Mote { double x, y, z, ph; };
    struct Mist { int k; double x, y, r, v, a; };
    struct Fern { double x, len, ang, ph; };
    struct Bird { double dx, dy, ph; };
    struct Flock { bool active = false; double x = 0, y = 0, v = 0; std::vector<Bird> b; };

    // Todas plantadas abaixo da borda: só aparece do meio do tronco pra cima.
    static constexpr double BASE[4] = { 40, 90, 150, 260 }, AMP[4] = { 14, 18, 22, 26 };
    static constexpr double HR[4][2] = { { 340, 460 }, { 470, 620 }, { 640, 820 }, { 900, 1150 } };
    static constexpr double SP[4][2] = { { 22, 46 }, { 40, 85 }, { 75, 150 }, { 190, 340 } };
    static constexpr double SWAY[4] = { 0.012, 0.02, 0.03, 0.04 }, DYK[4] = { 0.5, 0.8, 1, 1.6 };

    static double gust(const Ctx& c, double t, double x)
    {
        const double amp = INT(c);
        return amp * (0.35 + 0.65 * std::pow(0.5 + 0.5 * std::sin(t * 0.55 - x * 0.0045), 3)) + amp * 0.12 * std::sin(t * 2.1 + x * 0.03);
    }
    static double hill(const Ctx& c, int k, double x)
    {
        return c.H + BASE[k] + std::sin(x * 0.0031 + k * 2.3) * AMP[k] + std::sin(x * 0.0083 + k) * AMP[k] * 0.45;
    }
    static Shape shape(double h, int k)
    {
        Shape s;
        const double r = rnd();
        const int kind = (k > 0 && r < 0.07) ? 2 : (rnd() < 0.3 ? 1 : 0);   // 0 pinheiro, 1 abeto fino, 2 árvore seca
        const int n = kind == 2 ? 4 : 7 + RI(4);
        s.st = kind == 2 ? R(0.3, 0.45) : R(0.05, 0.09);
        const double w = h * (kind == 1 ? R(0.09, 0.12) : kind == 2 ? R(0.05, 0.08) : R(0.13, 0.17));
        auto side = [&](std::vector<QPointF>& arr) {
            for (int i = 0; i < n; ++i) {
                const double a = double(i) / n, y0 = h * (s.st + (0.96 - s.st) * a), dy = h * (0.96 - s.st) / n, wi = w * std::pow(1 - a, 0.85) * R(0.86, 1.1);
                const double lift = dy * 0.35, droop = dy * R(0.05, 0.25);
                if (kind == 2) {
                    arr.push_back(QPointF(w * 0.12, y0));
                    arr.push_back(QPointF(wi, y0 + droop * 0.5));
                    arr.push_back(QPointF(w * 0.12, y0 + dy * 0.25));
                    continue;
                }
                // a camada sobe do tronco, faz o serrilhado embaixo e cai na ponta
                arr.push_back(QPointF(wi * 0.36, y0 + lift));
                arr.push_back(QPointF(wi * 0.6, y0 + R(-1, 2) - droop * 0.3));
                arr.push_back(QPointF(wi * 0.68, y0 + lift * 0.55));
                arr.push_back(QPointF(wi * 0.86, y0 - droop * 0.6));
                arr.push_back(QPointF(wi, y0 - droop + R(-2, 1)));
            }
        };
        side(s.R);
        side(s.L);
        s.h = h;
        s.tw = qMax(1.2, h * (k == 3 ? 0.03 : 0.024)) * (kind == 2 ? 0.85 : 1);
        return s;
    }
    // raios de luz saindo do sol por entre as árvores
    void rays(QPainter& x, double sx, double sy, const QColor& star, double a) const
    {
        x.setCompositionMode(QPainter::CompositionMode_Plus);
        x.setPen(Qt::NoPen);
        for (int i = 0; i < 7; ++i) {
            const double ang = 0.75 + i * 0.22 + std::sin(m_t * 0.07 + i) * 0.03, wd = 0.022 + 0.016 * std::sin(m_t * 0.13 + i * 1.7), len = 1300;
            QLinearGradient g(sx, sy, sx + std::cos(ang) * len, sy + std::sin(ang) * len);
            g.setColorAt(0, alpha(star, 0.13 * a));
            g.setColorAt(0.6, alpha(star, 0.04 * a));
            g.setColorAt(1, alpha(star, 0));
            QPolygonF ray;
            ray << QPointF(sx, sy) << QPointF(sx + std::cos(ang - wd) * len, sy + std::sin(ang - wd) * len) << QPointF(sx + std::cos(ang + wd) * len, sy + std::sin(ang + wd) * len);
            x.setBrush(g);
            x.drawPolygon(ray);
        }
        x.setBrush(Qt::NoBrush);
        x.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }
    double inRay(double sx, double sy, double x, double y) const
    {
        const double ang = std::atan2(y - sy, x - sx);
        double best = 0;
        for (int i = 0; i < 7; ++i) {
            const double c0 = 0.75 + i * 0.22 + std::sin(m_t * 0.07 + i) * 0.03, wd = 0.022 + 0.016 * std::sin(m_t * 0.13 + i * 1.7);
            best = qMax(best, 1 - std::abs(ang - c0) / wd);
        }
        return qMax(0.0, best);
    }
    // torre de vigia de incêndio na serra (a do Firewatch): pernas em treliça, cabine com a janelinha
    static void lookout(QPainter& x, double tx, double ty, const QColor& col, const QColor& lit)
    {
        x.setBrush(Qt::NoBrush);
        x.setPen(QPen(col, 1.2));
        x.drawLine(QPointF(tx - 9, ty), QPointF(tx - 5, ty - 46));
        x.drawLine(QPointF(tx + 9, ty), QPointF(tx + 5, ty - 46));
        for (int i = 0; i < 4; ++i) {
            const double ya = ty - i * 11.5, yb = ty - (i + 1) * 11.5, wa = 9 - i, wb = 8 - i;
            x.drawLine(QPointF(tx - wa, ya), QPointF(tx + wb, yb));
            x.drawLine(QPointF(tx + wa, ya), QPointF(tx - wb, yb));
        }
        x.setPen(Qt::NoPen);
        x.fillRect(QRectF(tx - 9, ty - 47, 18, 2), col);
        x.fillRect(QRectF(tx - 7, ty - 58, 14, 11), col);
        QPolygonF roof;
        roof << QPointF(tx - 10, ty - 58) << QPointF(tx, ty - 64) << QPointF(tx + 10, ty - 58);
        x.setBrush(col);
        x.drawPolygon(roof);
        x.setBrush(Qt::NoBrush);
        x.fillRect(QRectF(tx - 5, ty - 55, 10, 4), lit);
    }
    // samambaia: haste arqueando e os folíolos dos dois lados
    static void fern(QPainter& x, double x0, double y0, double len, double ang, double sway, const QColor& col)
    {
        const double dir = std::cos(ang) >= 0 ? 1 : -1, ex = x0 + dir * len * 0.85 + sway * 12, ey = y0 - len * 0.35, cx = x0 + dir * len * 0.25, cy = y0 - len * 1.05;
        auto at = [&](double t) { return QPointF((1 - t) * (1 - t) * x0 + 2 * (1 - t) * t * cx + t * t * ex, (1 - t) * (1 - t) * y0 + 2 * (1 - t) * t * cy + t * t * ey); };
        QPainterPath stem(QPointF(x0, y0));
        stem.quadTo(cx, cy, ex, ey);
        x.setBrush(Qt::NoBrush);
        x.setPen(QPen(col, 1.4));
        x.drawPath(stem);
        x.setPen(Qt::NoPen);
        QPainterPath leaves;
        for (int i = 2; i < 17; ++i) {
            const double t = i / 17.0;
            const QPointF a = at(t), b = at(t + 0.02);
            const double nx = -(b.y() - a.y()), ny = b.x() - a.x(), nlen = qMax(1e-6, std::hypot(nx, ny)), lf = len * 0.16 * std::sin(t * M_PI) + 2;
            for (double sd : { -1.0, 1.0 }) {
                const QPointF pt(a.x() + nx / nlen * lf * sd, a.y() + ny / nlen * lf * sd + lf * 0.3);
                leaves.moveTo(a);
                leaves.quadTo((a.x() + pt.x()) / 2 + (b.x() - a.x()) * 3, (a.y() + pt.y()) / 2 + (b.y() - a.y()) * 3, pt.x(), pt.y());
                leaves.quadTo((a.x() + pt.x()) / 2, (a.y() + pt.y()) / 2, a.x(), a.y());
            }
        }
        x.fillPath(leaves, col);
    }
    static void addTree(QPainterPath& path, double x, double base, const Shape& s, double sway)
    {
        auto X = [&](double px, double py) { const double q = py / s.h; return x + px + sway * q * q; };
        path.moveTo(X(s.tw * 2.6, 0), base + 4);
        path.quadTo(X(s.tw * 1.2, 0), base - s.h * 0.012, X(s.tw, s.h * 0.03), base - s.h * 0.03);
        path.lineTo(X(s.tw * 0.8, s.h * s.st), base - s.h * s.st);
        for (const QPointF& pt : s.R) path.lineTo(X(pt.x(), pt.y()), base - pt.y());
        path.lineTo(X(0, s.h), base - s.h);
        for (int i = int(s.L.size()) - 1; i >= 0; --i) path.lineTo(X(-s.L[i].x(), s.L[i].y()), base - s.L[i].y());
        path.lineTo(X(-s.tw * 0.8, s.h * s.st), base - s.h * s.st);
        path.lineTo(X(-s.tw, s.h * 0.03), base - s.h * 0.03);
        path.quadTo(X(-s.tw * 1.2, 0), base - s.h * 0.012, X(-s.tw * 2.6, 0), base + 4);
        path.closeSubpath();
    }
    static std::vector<QPointF> ridge(const Ctx& c, double yBase, double amp, int it)
    {
        std::vector<QPointF> pts = { QPointF(0, 0), QPointF(1, 0) };
        double a = amp;
        for (int i = 0; i < it; ++i, a *= 0.52) {
            std::vector<QPointF> n;
            for (size_t j = 0; j + 1 < pts.size(); ++j) {
                n.push_back(pts[j]);
                n.push_back(QPointF((pts[j].x() + pts[j + 1].x()) / 2, (pts[j].y() + pts[j + 1].y()) / 2 + R(-a, a)));
            }
            n.push_back(pts.back());
            pts = n;
        }
        for (QPointF& p : pts) p = QPointF(p.x() * c.W, yBase + p.y());
        return pts;
    }
    // [topo do céu, horizonte, mais escuro, tom puxado nos planos, astro]
    static void palette(const Pal& p, const Ctx& c, QColor& top, QColor& hor, QColor& deep, QColor& tint, QColor& star)
    {
        if (c.tod == QLatin1String("day")) { top = hex("#79acd8"); hor = hex("#eaf0ec"); deep = hex("#183a33"); tint = hex("#3f7a6a"); star = hex("#fffbe8"); return; }
        if (c.tod == QLatin1String("dusk")) { top = hex("#2b2a5c"); hor = hex("#f7a75d"); deep = hex("#24122c"); tint = hex("#b0466b"); star = hex("#ffe2b0"); return; }
        if (c.tod == QLatin1String("night")) { top = hex("#050915"); hor = hex("#2b3b5e"); deep = hex("#020409"); tint = hex("#1d2c4a"); star = hex("#e8eefc"); return; }
        top = mix(p.app, Qt::black, 0.25);
        hor = mix(mix(p.acc, hex("#f4e6b8"), 0.5), p.app, 0.15);
        deep = mix(p.app, Qt::black, 0.6);
        tint = mix(p.green, p.app, 0.35);
        star = mix(p.acc, hex("#fff6d8"), 0.7);
    }

    std::vector<std::vector<QPointF>> m_ridges;
    Cached m_back, m_far;
    QString m_backTod, m_farTod;
    int m_farAge = 0;
    std::vector<Tree> m_layers[4];
    std::vector<Mote> m_motes;
    std::vector<Mist> m_mist;
    QPixmap m_mistDot;
    Cached m_raysPm, m_mid;
    QString m_raysTod;
    int m_raysAge = 0;
    QColor m_mistCol;
    std::vector<Fern> m_ferns;
    Flock m_birds;
    double m_t = 0, m_nextBirds = 3;
};

// ============================================================ folhas e pétalas

class Leaves : public Layer {
public:
    explicit Leaves(bool petals) : m_petals(petals) {}
    void init(Ctx& c) override
    {
        m_leaves.assign(size_t(count(m_petals ? 90 : 46, c)), L{});
        for (L& l : m_leaves) spawn(l, true, c);
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        const double w = (m_petals ? 34 : 24) + 26 * std::sin(m_t * 0.3);
        for (L& l : m_leaves) {
            l.y += l.v * dt;
            l.x += (w + std::sin(m_t * 1.6 + l.ph) * 40) * l.z * dt;
            l.rot += l.vr * dt;
            if (l.y > c.H + 20 || l.x > c.W + 40) spawn(l, false, c);
        }
    }
    void paint(QPainter& p, const Pal& pal, Ctx&) override
    {
        static const char* PET[5] = { "#f6c6d3", "#f2aec2", "#fbe1e8", "#eb9bb3", "#f8d3dd" };
        static const char* AUT[4] = { "#c8552a", "#d9862e", "#b8392b", "#e0b040" };
        p.setPen(Qt::NoPen);
        for (const L& l : m_leaves) {
            const double flip = std::abs(std::sin(m_t * 1.9 + l.ph));
            p.save();
            p.translate(l.x, l.y);
            p.rotate(qRadiansToDegrees(l.rot));
            p.scale(0.25 + 0.75 * flip, 1);
            p.setOpacity(0.5 + 0.5 * l.z);
            const double s = l.s;
            QPainterPath path(QPointF(0, -s));
            QColor col;
            if (m_petals) {
                col = pal.dark ? hex(PET[l.c]) : mix(hex(PET[l.c]), hex("#c44f74"), 0.25);
                path.cubicTo(s, -s, s, s * 0.6, 0, s);
                path.cubicTo(-s, s * 0.6, -s, -s, 0, -s);
            } else {
                col = mix(l.c < 4 ? hex(AUT[l.c]) : pal.acc, pal.app, 0.15);
                path.quadTo(s * 0.75, -s * 0.2, 0, s);
                path.quadTo(-s * 0.75, -s * 0.2, 0, -s);
            }
            p.fillPath(path, col);
            if (!m_petals) {
                p.setPen(QPen(alpha(Qt::black, 0.25), 0.8));
                p.drawLine(QPointF(0, -s * 0.9), QPointF(0, s * 1.25));
                p.setPen(Qt::NoPen);
            }
            p.restore();
        }
    }

private:
    struct L { double x = 0, y = 0, z = 0, v = 0, s = 0, rot = 0, vr = 0, ph = 0; int c = 0; };
    void spawn(L& l, bool any, const Ctx& c)
    {
        l.z = R(0.35, 1);
        l.x = m_petals ? R(-200, c.W * 0.7) : R(-80, c.W);
        l.y = any ? R(-30, c.H) : R(-60, -15);
        l.v = (m_petals ? 22 : 30) + (m_petals ? 35 : 55) * l.z;
        l.s = m_petals ? 3.5 + 4 * l.z : 7 + 9 * l.z;
        l.rot = R(0, 6.28);
        l.vr = R(-2.2, 2.2);
        l.ph = R(0, 6.28);
        l.c = RI(5);
    }
    bool m_petals;
    std::vector<L> m_leaves;
    double m_t = 0;
};

// Galho de cerejeira entrando pelo canto de cima, à esquerda (cenário do Hanami).
class Branch : public Layer {
public:
    void init(Ctx&) override { m_cache.reset(); }
    void step(double, Ctx&) override {}
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        skyTOD(p, pal, c);
        if (!m_cache.valid(c)) build(pal, c);
        blit(p, m_cache.pm, 0, 0, c.W, c.H);
    }

private:
    void build(const Pal& pal, const Ctx& c)
    {
        QPixmap pm = layer(c, c.W, c.H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing);
        const QColor bark = pal.dark ? mix(pal.app, Qt::black, 0.5) : mix(pal.app, hex("#3a2a28"), 0.75);
        std::vector<QPointF> blossoms;
        std::function<void(double, double, double, double, double, int)> grow = [&](double x0, double y0, double ang, double len, double w, int d) {
            const double x1 = x0 + std::cos(ang) * len, y1 = y0 + std::sin(ang) * len;
            x.setPen(roundPen(bark, w));
            QPainterPath br(QPointF(x0, y0));
            br.quadTo((x0 + x1) / 2 + R(-12, 12), (y0 + y1) / 2 + R(-12, 12), x1, y1);
            x.drawPath(br);
            for (int i = 0; i < 3 + d * 2; ++i)
                blossoms.push_back(QPointF(x0 + (x1 - x0) * rnd() + R(-14, 14), y0 + (y1 - y0) * rnd() + R(-14, 14)));
            if (d < 4) {
                grow(x1, y1, ang + R(0.15, 0.55), len * R(0.6, 0.8), w * 0.62, d + 1);
                if (rnd() < 0.8) grow(x1, y1, ang - R(0.25, 0.7), len * R(0.5, 0.7), w * 0.55, d + 1);
            }
        };
        x.setBrush(Qt::NoBrush);
        grow(-20, 60, 0.32, 150, 13, 0);
        static const char* PK[4] = { "#f6c6d3", "#f2aec2", "#fbe1e8", "#eb9bb3" };
        x.setPen(Qt::NoPen);
        x.setOpacity(0.9);
        for (const QPointF& b : blossoms)
            for (int i = 0; i < 5; ++i) {
                x.setBrush(hex(PK[RI(4)]));
                x.drawEllipse(QPointF(b.x() + std::cos(i * 1.257) * 4, b.y() + std::sin(i * 1.257) * 4), 3.6, 3.6);
            }
        x.end();
        m_cache.set(c, pm);
    }
    Cached m_cache;
};

// Capim baixo no pé da mesa (cenário do Firefly Meadow).
class Meadow : public Layer {
public:
    void init(Ctx&) override {}
    void step(double dt, Ctx&) override { m_t += dt; }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        skyTOD(p, pal, c);
        // Linhas em lote: um caminho com centenas de curvas é bem mais lento no Qt.
        QVector<QLineF> grass;
        grass.reserve(int(c.W / 7 + 1) * 3);
        for (double x = 0; x < c.W; x += 7) {
            const double h = 18 + 22 * std::abs(std::sin(x * 12.9898)), s = std::sin(m_t * 0.9 + x * 0.02) * 4;
            auto q = [&](double t) {
                const double a = (1 - t) * (1 - t), b = 2 * (1 - t) * t, d = t * t;
                return QPointF(a * x + b * (x + s * 0.4) + d * (x + s), a * c.H + b * (c.H - h * 0.5) + d * (c.H - h));
            };
            const QPointF p0 = q(0), p1 = q(1.0 / 3), p2 = q(2.0 / 3), p3 = q(1);
            grass << QLineF(p0, p1) << QLineF(p1, p2) << QLineF(p2, p3);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(roundPen(alpha(c.tod == QLatin1String("day") ? hex("#3f6a3a") : mix(pal.app, Qt::black, 0.35), 0.9), 1.4));
        p.drawLines(grass);
    }

private:
    double m_t = 0;
};

// ============================================================ vaga-lumes

class Fireflies : public Layer {
public:
    void init(Ctx& c) override
    {
        static const double k[3] = { 0.45, 1, 2 };
        m_flies.assign(size_t(qMax(1, int(std::lround(52 * k[qBound(0, c.intensity, 2)] * c.W / 1440)))), F{});
        for (F& f : m_flies) {
            f.cx = R(0, c.W); f.cy = R(c.H * 0.25, c.H - 20); f.ax = R(30, 90); f.ay = R(14, 46);
            f.fx = R(0.08, 0.22); f.fy = R(0.12, 0.3); f.ph = R(0, 6.28); f.bl = R(0.5, 1.2); f.z = R(0.4, 1);
        }
    }
    void step(double dt, Ctx&) override
    {
        m_t += dt;
        for (F& f : m_flies) f.cx += 6 * dt * std::sin(m_t * 0.05 + f.ph);
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        if (m_sprite.isNull()) m_sprite = softDot(mix(hex("#d8f56a"), pal.acc, 0.25));
        const double dim = 1 - 0.85 * LVopen(pal, c);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const F& f : m_flies) {
            const double x = f.cx + std::sin(m_t * f.fx * 6.28 + f.ph) * f.ax, y = f.cy + std::sin(m_t * f.fy * 6.28 + f.ph * 1.7) * f.ay;
            const double glow = std::pow(qMax(0.0, std::sin(m_t * f.bl * 2.2 + f.ph)), 2);
            if (glow < 0.02) continue;
            p.setOpacity(glow * (0.55 + 0.45 * f.z) * dim);
            const double s = 26 * f.z;
            blit(p, m_sprite, x - s / 2, y - s / 2, s, s);
            p.setOpacity(qMin(1.0, glow * 1.2) * dim);
            disc(p, x, y, 1.3 * f.z, hex("#fbffe0"));
        }
        p.setOpacity(1);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    struct F { double cx = 0, cy = 0, ax = 0, ay = 0, fx = 0, fy = 0, ph = 0, bl = 0, z = 0; };
    std::vector<F> m_flies;
    double m_t = 0;
    QPixmap m_sprite;
};

// ============================================================ vidraça

class Window : public Layer {
public:
    void init(Ctx& c) override
    {
        m_t = 0;
        m_still.reset();
        m_trail.clear();
        m_bokeh.clear();
        const int nb = qMax(8, int(std::lround(38 * c.W / 1440)));
        for (int i = 0; i < nb; ++i) m_bokeh.push_back({ R(0, c.W), R(c.H * 0.2, c.H), R(30, 95), RI(5), R(0, 6.28), R(0.18, 0.4) });
        m_runs.assign(size_t(count(16, c)), Run{});
        for (Run& r : m_runs) newRun(r, c);
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        for (Run& r : m_runs) {
            // Gota de vidro anda aos trancos: para, junta água, desce de novo.
            r.go -= dt;
            if (r.go <= 0) {
                r.v = r.v > 0 ? 0 : R(60, 190) * (r.r / 5);
                r.go = r.v > 0 ? R(0.3, 1.2) : R(0.4, 2.5);
            }
            if (r.v > 0) {
                r.y += r.v * dt;
                r.x += std::sin(m_t * 3 + r.wob) * 10 * dt;
                if (rnd() < 0.8) m_trail.push_back({ r.x + R(-1.5, 1.5), r.y - r.r, R(1, 2.6), 0 });
            }
            if (r.y > c.H + 10) { newRun(r, c); r.y = R(-30, -10); }
        }
        for (Trail& t : m_trail) t.t += dt;
        m_trail.erase(std::remove_if(m_trail.begin(), m_trail.end(), [](const Trail& t) { return t.t >= 5; }), m_trail.end());
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        static const char* BOKEH[5] = { "#f2b45a", "#e8875a", "#f6d48a", "#7fb0e0", "#e0607a" };
        if (m_sprites.empty()) for (const char* b : BOKEH) m_sprites.push_back(softDot(hex(b), 0.55));
        sky(p, c, mix(pal.app, Qt::black, 0.25), mix(pal.app, hex("#1a1410"), 0.2));
        {
            const QSize lowSize(qMax(1, int(c.W / 4) + 1), int(c.H / 4) + 1);
            if (m_low.size() != lowSize) m_low = QImage(lowSize, QImage::Format_ARGB32_Premultiplied);
            m_low.fill(Qt::transparent);
            QPainter x(&m_low);
            x.setRenderHint(QPainter::SmoothPixmapTransform);
            x.scale(0.25, 0.25);
            x.setCompositionMode(QPainter::CompositionMode_Plus);
            for (const Bokeh& b : m_bokeh) {
                x.setOpacity(b.a * (0.8 + 0.2 * std::sin(m_t * 0.4 + b.ph)));
                blit(x, m_sprites[size_t(b.c)], b.x - b.r + std::sin(m_t * 0.05 + b.ph) * 20, b.y - b.r, b.r * 2, b.r * 2);
            }
            x.end();
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            p.drawImage(QRectF(0, 0, lowSize.width() * 4, lowSize.height() * 4), m_low);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        p.fillRect(QRectF(0, 0, c.W, c.H), alpha(pal.app, 0.25));
        if (!m_still.valid(c)) buildStill(pal, c);
        blit(p, m_still.pm, 0, 0, c.W, c.H);
        const QColor tc = mix(pal.app, Qt::white, 0.6);
        p.setPen(Qt::NoPen);
        for (const Trail& t : m_trail) disc(p, t.x, t.y, t.r, alpha(tc, 0.4 * (1 - t.t / 5)));
        const QColor body = alpha(mix(pal.app, Qt::white, 0.5), 0.42);
        for (const Run& r : m_runs) {
            p.setBrush(alpha(Qt::black, 0.25));
            p.drawEllipse(QPointF(r.x, r.y + r.r * 0.3), r.r * 0.92, r.r * 1.12);
            p.setBrush(body);
            p.drawEllipse(QPointF(r.x, r.y), r.r * 0.9, r.r * 1.1);
            p.setBrush(alpha(Qt::white, 0.8));
            p.drawEllipse(QPointF(r.x - r.r * 0.32, r.y - r.r * 0.45), r.r * 0.26, r.r * 0.26);
        }
    }

private:
    struct Bokeh { double x, y, r; int c; double ph, a; };
    struct Run { double x = 0, y = 0, r = 0, v = 0, go = 0, wob = 0; };
    struct Trail { double x, y, r, t; };
    static void newRun(Run& r, const Ctx& c)
    {
        r.x = R(0, c.W); r.y = R(-40, c.H * 0.5); r.r = R(5, 9); r.v = 0; r.go = R(0, 1.5); r.wob = R(0, 6.28);
    }
    void buildStill(const Pal& pal, const Ctx& c)
    {
        QPixmap pm = layer(c, c.W, c.H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing);
        x.setPen(Qt::NoPen);
        // vidro embaçado e as gotinhas paradas: sombra embaixo, corpo translúcido, brilho pequeno em cima
        x.fillRect(QRectF(0, 0, c.W, c.H), alpha(mix(pal.app, Qt::white, 0.35), 0.12));
        const int n = int(std::lround(650 * c.W / 1440));
        for (int i = 0; i < n; ++i) {
            const double px = R(0, c.W), py = R(0, c.H), r = std::pow(rnd(), 2) * 4.5 + 1.2, ry = r * R(0.85, 1.15);
            x.setBrush(alpha(Qt::black, 0.28));
            x.drawEllipse(QPointF(px, py + r * 0.3), r, ry);
            QRadialGradient g(QPointF(px, py), r, QPointF(px, py + r * 0.35));
            g.setColorAt(0, alpha(mix(pal.app, Qt::white, 0.55), 0.35));
            g.setColorAt(1, alpha(mix(pal.app, Qt::white, 0.3), 0.12));
            x.setBrush(g);
            x.drawEllipse(QPointF(px, py), r * 0.92, ry * 0.92);
            x.save();
            x.translate(px - r * 0.25, py - ry * 0.45);
            x.rotate(qRadiansToDegrees(-0.5));
            x.setBrush(alpha(Qt::white, 0.55));
            x.drawEllipse(QPointF(0, 0), r * 0.3, r * 0.18);
            x.restore();
        }
        x.end();
        m_still.set(c, pm);
    }
    std::vector<Bokeh> m_bokeh;
    std::vector<Run> m_runs;
    std::vector<Trail> m_trail;
    std::vector<QPixmap> m_sprites;
    QImage m_low;
    Cached m_still;
    double m_t = 0;
};

// ============================================================ poeira na luz

class Sunbeam : public Layer {
public:
    void init(Ctx& c) override
    {
        m_motes.assign(size_t(count(260, c)), M{});
        for (M& m : m_motes) m = { R(0, c.W), R(0, c.H), R(0.3, 1), R(0, 6.28), R(-4, 4), R(-3, 3) };
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        for (M& m : m_motes) {
            m.x += (m.vx + std::sin(m_t * 0.3 + m.ph) * 6) * dt * m.z;
            m.y += (m.vy + std::cos(m_t * 0.23 + m.ph) * 5 + 2) * dt * m.z;
            if (m.y > c.H) m.y = 0;
            if (m.y < 0) m.y = c.H;
            if (m.x > c.W) m.x = 0;
            if (m.x < 0) m.x = c.W;
        }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        if (m_sprite.isNull()) m_sprite = softDot(hex("#fff2d0"), 0.4);
        sky(p, c, mix(pal.app, Qt::black, 0.12), pal.app);
        const auto beams = beamList(c);
        const double breathe = 0.85 + 0.15 * std::sin(m_t * 0.21);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const Beam& b : beams) {
            QLinearGradient g(0, 0, 0, c.H);
            g.setColorAt(0, alpha(mix(pal.acc, Qt::white, 0.5), 0.2 * b.s * breathe));
            g.setColorAt(1, alpha(pal.acc, 0.03 * b.s));
            QPainterPath path(QPointF(b.a, 0));
            path.lineTo(b.b, 0);
            path.lineTo(b.d, c.H);
            path.lineTo(b.c, c.H);
            path.closeSubpath();
            p.fillPath(path, g);
        }
        for (const M& m : m_motes) {
            double v = 0;
            for (const Beam& b : beams) {
                const double k = m.y / c.H, l = b.a + (b.c - b.a) * k, r = b.b + (b.d - b.b) * k, mid = (l + r) / 2, hw = (r - l) / 2;
                v = qMax(v, b.s * (1 - smooth(hw * 0.55, hw, std::abs(m.x - mid))));
            }
            const double al = (0.04 + 0.85 * v) * m.z * breathe * (0.7 + 0.3 * std::sin(m_t * 1.3 + m.ph));
            if (al < 0.02) continue;
            p.setOpacity(al);
            const double s = 3 + 3.5 * m.z;
            blit(p, m_sprite, m.x - s / 2, m.y - s / 2, s, s);
        }
        p.setOpacity(1);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    struct M { double x, y, z, ph, vx, vy; };
    struct Beam { double a, b, c, d, s; };
    // Dois feixes de janela: um no pedaço esquerdo da mesa, outro no direito.
    static std::vector<Beam> beamList(const Ctx& c)
    {
        const double lx = c.zones.left.width() > 120 ? c.zones.left.left() + 25 : 70;
        return { { lx, lx + 160, lx + 140, lx + 360, 1 }, { c.W - 290, c.W - 180, c.W - 130, c.W + 20, 0.7 } };
    }
    std::vector<M> m_motes;
    double m_t = 0;
    QPixmap m_sprite;
};

// ============================================================ aurora

class Aurora : public Layer {
public:
    void init(Ctx&) override { m_stars.reset(); }
    void step(double dt, Ctx& c) override { m_t += dt * (0.6 + 0.4 * INT(c)); }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        sky(p, c, mix(pal.app, Qt::black, 0.3), mix(pal.app, hex("#10233a"), 0.35));
        if (!m_stars.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            const int n = int(std::lround(260 * c.W / 1440));
            for (int i = 0; i < n; ++i) x.fillRect(QRectF(R(0, c.W), R(0, c.H * 0.8), R(0.6, 1.6), R(0.6, 1.6)), alpha(Qt::white, R(0.15, 0.6)));
            x.end();
            m_stars.set(c, pm);
        }
        blit(p, m_stars.pm, 0, 0, c.W, c.H);
        struct Rib { double y0; const char* col; double s, ph; };
        static const Rib RIB[3] = { { 170, "#6ee7b7", 1, 0 }, { 250, "#5eead4", 0.8, 2.1 }, { 120, "#a78bfa", 0.65, 4.2 } };
        const QSize lowSize(qMax(1, int(c.W / 4) + 1), int(c.H / 4) + 1);
        if (m_low.size() != lowSize) m_low = QImage(lowSize, QImage::Format_ARGB32_Premultiplied);
        m_low.fill(Qt::transparent);
        QPainter x(&m_low);
        x.scale(0.25, 0.25);
        x.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const Rib& rb : RIB) {
            const QColor col = hex(rb.col);
            for (double xx = -10; xx < c.W + 10; xx += 8) {
                const double x0 = xx;
                const double y = rb.y0 + std::sin(x0 * 0.004 + m_t * 0.13 + rb.ph) * 46 + std::sin(x0 * 0.011 - m_t * 0.21 + rb.ph * 2) * 18;
                const double h = 230 + std::sin(x0 * 0.006 + m_t * 0.17 + rb.ph) * 80;
                const double ray = 0.5 + 0.5 * std::sin(x0 * 0.09 + m_t * 0.8 + rb.ph) * std::sin(x0 * 0.023 - m_t * 0.3);
                QLinearGradient g(0, y, 0, y + h);
                g.setColorAt(0, alpha(col, 0));
                g.setColorAt(0.72, alpha(col, 0.3 * rb.s * (0.35 + ray)));
                g.setColorAt(1, alpha(col, 0));
                x.fillRect(QRectF(x0, y, 8, h), g);
            }
        }
        x.end();
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.drawImage(QRectF(0, 0, lowSize.width() * 4, lowSize.height() * 4), m_low);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        QPainterPath mtn(QPointF(0, c.H));
        for (double x = 0; x <= c.W + 40; x += 40)
            mtn.lineTo(x, c.H - 40 - 30 * std::abs(std::sin(x * 0.0071)) - 20 * std::abs(std::sin(x * 0.023)));
        mtn.lineTo(c.W + 40, c.H);
        p.fillPath(mtn, mix(pal.app, Qt::black, 0.45));
    }

private:
    Cached m_stars;
    QImage m_low;
    double m_t = 0;
};

// ============================================================ céu estrelado

class Stars : public Layer {
public:
    explicit Stars(bool bg) : m_bg(bg) {}
    void init(Ctx& c) override
    {
        m_stars.assign(size_t(count(320, c)), S{});
        for (S& s : m_stars) s = { R(0, c.W), R(0, c.H), std::pow(rnd(), 3) * 1.6 + 0.4, R(0, 6.28), R(0.5, 2.5) };
        m_band.reset();
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_next -= dt;
        if (m_next <= 0) shoot(c);
        if (m_shoot.d > 0) { m_shoot.t += dt; if (m_shoot.t > m_shoot.d) m_shoot.d = 0; }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double vis = m_bg ? 1 : 1 - (c.tod == QLatin1String("theme") ? (pal.dark ? 0 : 0.85) : LV(pal, c));
        if (vis <= 0.02) return;
        if (m_bg) {
            sky(p, c, mix(pal.app, Qt::black, 0.25), mix(pal.app, hex("#1b1a3a"), 0.3));
            if (!m_band.valid(c)) {
                QPixmap pm = layer(c, c.W, c.H);
                QPainter x(&pm);
                const QPixmap dot = softDot(mix(pal.app, hex("#c9c4ff"), 0.5));
                x.setOpacity(0.06);
                for (int i = 0; i < 160; ++i) {
                    const double k = rnd(), bx = -100 + (c.W + 200) * k, by = c.H * 0.95 + (-60 - c.H * 0.95) * k + R(-90, 90), s = R(60, 180);
                    blit(x, dot, bx - s / 2, by - s / 2, s, s);
                }
                x.end();
                m_band.set(c, pm);
            }
            blit(p, m_band.pm, 0, 0, c.W, c.H);
        }
        p.setPen(Qt::NoPen);
        const QColor big = mix(Qt::white, pal.acc, 0.3);
        for (const S& s : m_stars) {
            p.setOpacity(vis * (0.35 + 0.65 * std::pow(0.5 + 0.5 * std::sin(m_t * s.f + s.ph), 2)));
            p.setBrush(s.r > 1.4 ? big : QColor(Qt::white));
            p.drawEllipse(QPointF(s.x, s.y), s.r, s.r);
        }
        p.setOpacity(1);
        if (m_shoot.d > 0) {
            const double k = m_shoot.t / m_shoot.d, x = m_shoot.x + m_shoot.dx * k, y = m_shoot.y + m_shoot.dy * k, a = std::sin(k * M_PI);
            QLinearGradient g(x, y, x - m_shoot.dx * 0.25, y - m_shoot.dy * 0.25);
            g.setColorAt(0, alpha(Qt::white, a * vis));
            g.setColorAt(1, alpha(Qt::white, 0));
            QPen pen(QBrush(g), 1.8);
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            p.drawLine(QPointF(x, y), QPointF(x - m_shoot.dx * 0.25, y - m_shoot.dy * 0.25));
        }
    }

private:
    struct S { double x = 0, y = 0, r = 0, ph = 0, f = 0; };
    struct Shoot { double x = 0, y = 0, dx = 0, dy = 0, t = 0, d = 0; };
    void shoot(const Ctx& c)
    {
        const QPointF v = visPoint(c);
        m_shoot = { v.x() - 120, qMax(10.0, v.y() - 200), R(260, 420), R(120, 220), 0, 0.9 };
        m_next = R(18, 45);
    }
    bool m_bg;
    std::vector<S> m_stars;
    Cached m_band;
    Shoot m_shoot;
    double m_t = 0, m_next = 6;
};

// ============================================================ charneca e neblina

class Moor : public Layer {
public:
    void init(Ctx&) override { m_cache.reset(); }
    void step(double, Ctx&) override {}
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        skyTOD(p, pal, c, mix(pal.app, Qt::white, pal.dark ? 0.06 : 0.2), pal.app);
        if (!m_cache.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing);
            const QColor dk = mix(pal.app, Qt::black, 0.45);
            // árvores secas de charneca
            std::function<void(double, double, double, double, double, int)> tree = [&](double x0, double y0, double ang, double len, double w, int d) {
                const double x1 = x0 + std::cos(ang) * len, y1 = y0 + std::sin(ang) * len;
                x.setPen(roundPen(dk, w));
                x.drawLine(QPointF(x0, y0), QPointF(x1, y1));
                if (d < 6) {
                    tree(x1, y1, ang - R(0.2, 0.6), len * R(0.65, 0.8), w * 0.68, d + 1);
                    if (rnd() < 0.7) tree(x1, y1, ang + R(0.2, 0.6), len * R(0.6, 0.75), w * 0.65, d + 1);
                }
            };
            for (double tx : { 150.0, 330.0, c.W - 250, c.W - 90 }) tree(tx, c.H - 28, -M_PI / 2 + R(-0.12, 0.12), R(70, 110), 7, 0);
            QPainterPath ground(QPointF(0, c.H));
            for (double i = 0; i <= c.W + 30; i += 30) ground.lineTo(i, c.H - 30 - 12 * std::sin(i * 0.006));
            ground.lineTo(c.W + 30, c.H);
            x.fillPath(ground, dk);
            x.end();
            m_cache.set(c, pm);
        }
        blit(p, m_cache.pm, 0, 0, c.W, c.H);
    }

private:
    Cached m_cache;
};

class Fog : public Layer {
public:
    void init(Ctx&) override { m_tex.reset(); }
    void step(double dt, Ctx&) override { m_t += dt; }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const QColor fc = (c.tod == QLatin1String("day") || !pal.dark) ? QColor(Qt::white) : mix(pal.app, Qt::white, 0.55);
        if (fc != m_texFor) { m_tex.reset(); m_texFor = fc; }
        if (!m_tex.valid(c)) {
            QPixmap pm = layer(c, c.W, 420);
            QPainter x(&pm);
            const QPixmap dot = softDot(fc, 0.2);
            for (int i = 0; i < int(std::lround(90 * c.W / 1440)); ++i) {
                const double w = R(160, 420), h = w * R(0.3, 0.5), cx = R(0, c.W), cy = R(120, 300);
                for (double ox : { 0.0, -c.W, c.W }) {
                    x.setOpacity(R(0.25, 0.5));
                    blit(x, dot, cx + ox - w / 2, cy - h / 2, w, h);
                }
            }
            x.end();
            m_tex.set(c, pm);
        }
        static const double B[3][3] = { { 60, 9, 0.35 }, { 250, 16, 0.5 }, { 420, 26, 0.65 } };
        for (const auto& b : B) {
            const double off = std::fmod(m_t * b[1] * INT(c), c.W);
            p.setOpacity(b[2]);
            blit(p, m_tex.pm, off - c.W, b[0], c.W, 420);
            blit(p, m_tex.pm, off, b[0], c.W, 420);
        }
        p.setOpacity(1);
    }

private:
    Cached m_tex;
    QColor m_texFor;
    double m_t = 0;
};

// ============================================================ lago de carpas

class Koi : public Layer {
public:
    void init(Ctx& c) override
    {
        m_fish.clear();
        const int n = qMax(2, int(std::lround(6 * INT(c))));
        for (int i = 0; i < n; ++i) m_fish.push_back(newFish(i, c));
        m_rings.clear();
        m_pads.reset();
    }
    void resized(Ctx& c) override { m_pads.reset(); for (Fish& f : m_fish) retarget(f, c); }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        for (Fish& f : m_fish) {
            const QPointF h = f.pts[0];
            double d = std::atan2(f.ty - h.y(), f.tx - h.x()) - f.ang;
            while (d > M_PI) d -= 2 * M_PI;
            while (d < -M_PI) d += 2 * M_PI;
            f.ang += qBound(-1.2 * dt, d, 1.2 * dt) + std::sin(m_t * 2 + f.ph) * 0.4 * dt;
            f.pts[0] = QPointF(h.x() + std::cos(f.ang) * f.v * dt, h.y() + std::sin(f.ang) * f.v * dt);
            const double seg = f.len / 9;
            for (size_t k = 1; k < f.pts.size(); ++k) {
                const QPointF a = f.pts[k - 1], b = f.pts[k];
                const double an = std::atan2(b.y() - a.y(), b.x() - a.x());
                f.pts[k] = QPointF(a.x() + std::cos(an) * seg, a.y() + std::sin(an) * seg);
            }
            if (std::hypot(f.tx - h.x(), f.ty - h.y()) < 30) {
                retarget(f, c);
                if (rnd() < 0.35) m_rings.push_back({ h.x(), h.y(), 0 });
            }
        }
        for (Ring& r : m_rings) r.t += dt;
        m_rings.erase(std::remove_if(m_rings.begin(), m_rings.end(), [](const Ring& r) { return r.t >= 2.4; }), m_rings.end());
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const QColor wt = mix(pal.app, hex("#2a5a58"), 0.25), wb = mix(pal.app, Qt::black, 0.25);
        if (c.tod == QLatin1String("day")) sky(p, c, mix(wt, hex("#5a9e96"), 0.45), mix(wb, hex("#2f6a66"), 0.45));
        else if (c.tod == QLatin1String("dusk")) sky(p, c, mix(wt, hex("#7a5a6a"), 0.35), mix(wb, hex("#3a2a30"), 0.3));
        else if (c.tod == QLatin1String("night")) sky(p, c, mix(wt, hex("#02060c"), 0.6), mix(wb, Qt::black, 0.5));
        else sky(p, c, wt, wb);
        // luz dançando no fundo
        p.setBrush(Qt::NoBrush);
        QVector<QLineF> light;
        for (double y = 20; y < c.H; y += 26) {
            QPointF prev(0, y + std::sin(m_t * 0.8 + y) * 6);
            for (double x = 30; x <= c.W + 30; x += 30) {
                const QPointF pt(x, y + std::sin(x * 0.02 + m_t * 0.8 + y) * 6);
                light << QLineF(prev, pt);
                prev = pt;
            }
        }
        {
            const QSize lowSize(qMax(1, int(c.W / 4) + 1), int(c.H / 4) + 1);
            if (m_low.size() != lowSize) m_low = QImage(lowSize, QImage::Format_ARGB32_Premultiplied);
            m_low.fill(Qt::transparent);
            QPainter x(&m_low);
            x.setRenderHint(QPainter::Antialiasing);
            x.scale(0.25, 0.25);
            x.setPen(QPen(alpha(Qt::white, 0.035), 2));
            x.drawLines(light);
            x.end();
            p.drawImage(QRectF(0, 0, lowSize.width() * 4, lowSize.height() * 4), m_low);
        }
        p.setPen(Qt::NoPen);
        for (const Fish& f : m_fish) {
            auto w = [&](size_t k) { return f.len * 0.16 * std::sin(M_PI * qMin(1.0, (k + 1.4) / 9.5)); };
            p.setBrush(alpha(Qt::black, 0.18));
            for (size_t k = 0; k < f.pts.size(); ++k) p.drawEllipse(f.pts[k] + QPointF(8, 10), w(k), w(k));
            const QPointF tp = f.pts[9], pp = f.pts[8];
            const double ta = std::atan2(tp.y() - pp.y(), tp.x() - pp.x()) + std::sin(m_t * 6 + f.ph) * 0.35;
            QPainterPath tail(tp);
            tail.lineTo(tp + QPointF(std::cos(ta + 0.5) * 16, std::sin(ta + 0.5) * 16));
            tail.lineTo(tp + QPointF(std::cos(ta - 0.5) * 16, std::sin(ta - 0.5) * 16));
            tail.closeSubpath();
            p.fillPath(tail, alpha(hex(f.pat[0]), 0.75));
            const QPointF f2 = f.pts[2];
            const double na = f.ang + M_PI / 2;
            for (int sgn : { 1, -1 }) {
                p.save();
                p.translate(f2.x() + std::cos(na) * w(2) * sgn, f2.y() + std::sin(na) * w(2) * sgn);
                p.rotate(qRadiansToDegrees(f.ang + sgn * 0.9));
                p.setBrush(alpha(hex(f.pat[1]), 0.6));
                p.drawEllipse(QPointF(0, 0), 7, 3.5);
                p.restore();
            }
            for (size_t k = 0; k < f.pts.size(); ++k) {
                p.setBrush(hex(f.pat[(k >> 1) % 2]));
                p.drawEllipse(f.pts[k], w(k), w(k));
            }
        }
        p.setBrush(Qt::NoBrush);
        for (const Ring& r : m_rings) {
            const double k = r.t / 2.4, rr = 6 + 60 * k;
            p.setPen(QPen(alpha(Qt::white, 0.35 * (1 - k)), 1.2));
            p.drawEllipse(QPointF(r.x, r.y), rr, rr * 0.7);
        }
        if (!m_pads.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing);
            x.setPen(Qt::NoPen);
            for (int i = 0; i < 6; ++i) {
                const QPointF at = visPoint(c);
                const double r = R(22, 40), a = R(0, 360);
                for (int pass = 0; pass < 2; ++pass) {
                    QPainterPath pad;
                    const QPointF ctr = at + (pass == 0 ? QPointF(6, 8) : QPointF());
                    pad.moveTo(ctr);
                    pad.arcTo(QRectF(ctr.x() - r, ctr.y() - r * 0.86, r * 2, r * 1.72), a + 17, 326);
                    pad.closeSubpath();
                    x.fillPath(pad, pass == 0 ? alpha(Qt::black, 0.2) : mix(pal.green, hex("#2f5b2a"), 0.4));
                }
            }
            x.end();
            m_pads.set(c, pm);
        }
        blit(p, m_pads.pm, 0, 0, c.W, c.H);
    }

private:
    struct Fish { std::vector<QPointF> pts; double ang = 0, v = 0, len = 0, tx = 0, ty = 0, ph = 0; const char* pat[2]; };
    struct Ring { double x, y, t; };
    static Fish newFish(int i, const Ctx& c)
    {
        static const char* PAT[5][2] = { { "#f08a4b", "#fff4ea" }, { "#fff4ea", "#e0452c" }, { "#f6b13d", "#f6b13d" },
                                         { "#1c1c1c", "#f08a4b" }, { "#fff4ea", "#fff4ea" } };
        Fish f;
        const QPointF p0 = visPoint(c);
        for (int k = 0; k < 10; ++k) f.pts.push_back(QPointF(p0.x() - k * 6, p0.y()));
        f.ang = R(0, 6.28); f.v = R(28, 46); f.len = R(46, 66); f.ph = R(0, 6.28);
        f.pat[0] = PAT[i % 5][0]; f.pat[1] = PAT[i % 5][1];
        retarget(f, c);
        return f;
    }
    static void retarget(Fish& f, const Ctx& c) { const QPointF t = visPoint(c); f.tx = t.x(); f.ty = t.y(); }
    std::vector<Fish> m_fish;
    std::vector<Ring> m_rings;
    Cached m_pads;
    QImage m_low;
    double m_t = 0;
};

// ============================================================ lareira e vela

class Fire : public Layer {
public:
    explicit Fire(bool candle) : m_candle(candle) {}
    void init(Ctx& c) override
    {
        m_embers.clear();
        const int n = m_candle ? qMax(2, int(std::lround(6 * INT(c)))) : count(90, c);
        for (int i = 0; i < n; ++i) {
            E e = spawn(c);
            e.life = R(0, e.max);
            e.y += e.vy * e.life;
            m_embers.push_back(e);
        }
    }
    void resized(Ctx& c) override { init(c); }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        // tremor da chama: ruído suave que segue um alvo aleatório
        m_n1 += (R(-1, 1) - m_n1) * qMin(1.0, dt * 8);
        m_n2 += (R(-1, 1) - m_n2) * qMin(1.0, dt * 2);
        for (E& e : m_embers) {
            e.life += dt;
            e.y += e.vy * dt;
            e.x += (e.vx + std::sin(m_t * 2 + e.ph) * 18) * dt;
            if (e.life > e.max) e = spawn(c);
        }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        if (m_sprite.isNull()) m_sprite = softDot(hex("#ffb050"), 0.3);
        const double fl = 0.85 + 0.1 * m_n1 + 0.08 * m_n2;
        if (m_candle) {
            sky(p, c, mix(pal.app, Qt::black, 0.35), mix(pal.app, Qt::black, 0.2));
            const double cx = cxCandle(c), fy = c.H - 150;
            QRadialGradient g(QPointF(cx, fy), 460 * fl);
            g.setColorAt(0, alpha(hex("#ffc46b"), 0.42 * fl));
            g.setColorAt(0.3, alpha(hex("#e89a3c"), 0.16 * fl));
            g.setColorAt(1, alpha(hex("#e89a3c"), 0));
            p.fillRect(QRectF(0, 0, c.W, c.H), g);
            const QColor body = mix(hex("#efe3c8"), pal.app, 0.15);
            QLinearGradient bg(cx - 15, 0, cx + 15, 0);
            bg.setColorAt(0, mix(body, Qt::black, 0.35));
            bg.setColorAt(0.45, body);
            bg.setColorAt(1, mix(body, Qt::black, 0.45));
            p.fillRect(QRectF(cx - 15, fy + 14, 30, c.H - fy), bg);
            p.setPen(Qt::NoPen);
            p.setBrush(mix(body, Qt::white, 0.2));
            p.drawEllipse(QPointF(cx, fy + 14), 15, 4);
            p.setBrush(mix(body, Qt::white, 0.1));
            p.drawEllipse(QPointF(cx + 9, fy + 34), 3.5, 14);
            p.setPen(QPen(hex("#2a2018"), 1.6));
            p.drawLine(QPointF(cx, fy + 14), QPointF(cx + 1, fy + 4));
            const double fh = 30 * fl, sw = m_n1 * 3;
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            p.setOpacity(0.5);
            blit(p, m_sprite, cx - 40, fy - 30, 80, 80);
            p.setOpacity(1);
            QLinearGradient fg(0, fy - fh, 0, fy + 8);
            fg.setColorAt(0, alpha(hex("#fff3c4"), 0.95));
            fg.setColorAt(0.6, alpha(hex("#ffb347"), 0.9));
            fg.setColorAt(1, alpha(hex("#4a6cff"), 0.6));
            QPainterPath flame(QPointF(cx + sw, fy - fh));
            flame.cubicTo(cx + 9, fy - fh * 0.35, cx + 7, fy + 8, cx, fy + 8);
            flame.cubicTo(cx - 7, fy + 8, cx - 9, fy - fh * 0.35, cx + sw, fy - fh);
            p.fillPath(flame, fg);
        } else {
            sky(p, c, mix(pal.app, Qt::black, 0.3), mix(pal.app, hex("#3a1a08"), 0.35));
            const double spots[2][3] = { { qMax(260.0, c.zones.left.center().x()), 700, 1 }, { c.W - 190, 560, 0.7 } };
            for (const auto& s : spots) {
                QRadialGradient g(QPointF(s[0], c.H + 60), s[1] * fl);
                g.setColorAt(0, alpha(hex("#ff9a3c"), 0.42 * s[2] * fl));
                g.setColorAt(0.45, alpha(hex("#c8501e"), 0.14 * s[2] * fl));
                g.setColorAt(1, alpha(hex("#c8501e"), 0));
                p.fillRect(QRectF(0, 0, c.W, c.H), g);
            }
        }
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const E& e : m_embers) {
            const double k = e.life / e.max, a = (1 - k) * (0.6 + 0.4 * std::sin(m_t * 9 + e.ph));
            p.setOpacity(qMax(0.0, a));
            const double s = e.s * 5;
            blit(p, m_sprite, e.x - s / 2, e.y - s / 2, s, s);
            disc(p, e.x, e.y, e.s * 0.5, hex(k < 0.4 ? "#fff0b0" : "#ff8a3c"));
        }
        p.setOpacity(1);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    struct E { double x = 0, y = 0, vy = 0, vx = 0, life = 0, max = 1, ph = 0, s = 1; };
    static double cxCandle(const Ctx& c) { return anchorX(c, 0.75); }
    E spawn(const Ctx& c) const
    {
        E e;
        e.x = m_candle ? cxCandle(c) + R(-2, 2) : R(0, c.W);
        e.y = m_candle ? c.H - 150 : c.H + R(0, 20);
        e.vy = m_candle ? R(-30, -55) : R(-45, -120);
        e.vx = R(-10, 10);
        e.life = 0;
        e.max = m_candle ? R(0.6, 1.4) : R(3, 7);
        e.ph = R(0, 6.28);
        e.s = m_candle ? R(1, 2) : R(1.2, 3);
        return e;
    }
    bool m_candle;
    std::vector<E> m_embers;
    double m_t = 0, m_n1 = 0, m_n2 = 0;
    QPixmap m_sprite;
};

// ============================================================ letras soltas

class Letters : public Layer {
public:
    void init(Ctx& c) override
    {
        m_letters.clear();
        const int n = count(90, c);
        for (int i = 0; i < n; ++i) { Ltr l; spawn(l, true, c); m_letters.push_back(l); }
        m_nextWord = 2;
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_nextWord -= dt;
        if (m_nextWord <= 0) {
            m_nextWord = R(4, 8);
            // A palavra desce montada por uns segundos e depois se desfaz em letras soltas.
            const QStringList& ws = words();
            const QString w = ws.at(RI(ws.size()));
            const QPointF at = visPoint(c);
            const QRectF z = c.zones.left.contains(at) ? c.zones.left : c.zones.right.contains(at) ? c.zones.right : QRectF(0, 0, c.W, c.H);
            const double size = 30, span = w.size() * size * 0.6;
            const double x0 = R(z.left() + 10, qMax(z.left() + 11, z.right() - span - 10));
            for (int i = 0; i < w.size(); ++i) {
                Ltr l;
                l.z = 1; l.ch = w.at(i); l.x = x0 + i * size * 0.6; l.y = -30; l.v = 26; l.word = 3.6; l.isWord = true;
                m_letters.push_back(l);
            }
        }
        for (Ltr& l : m_letters) {
            if (l.word > 0) {
                l.word -= dt;
                if (l.word <= 0) { l.vx = R(-30, 30); l.vr = R(-1.4, 1.4); l.v = R(30, 60); }
            }
            l.y += l.v * dt;
            l.x += (l.vx + std::sin(m_t * 0.7 + l.ph) * 6 * l.z) * dt;
            l.rot += l.vr * dt * 0.5;
        }
        m_letters.erase(std::remove_if(m_letters.begin(), m_letters.end(), [&](const Ltr& l) { return l.isWord && l.y >= c.H + 40; }), m_letters.end());
        for (Ltr& l : m_letters) if (l.y >= c.H + 40) spawn(l, false, c);
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        if (qAbs(m_glyphPx - c.px) > 0.01) { m_glyphs.clear(); m_glyphPx = c.px; }
        for (const Ltr& l : m_letters) {
            const int fs = int(std::lround(l.isWord ? 30 : 11 + 24 * l.z));
            const QPixmap& g = glyph(c, l.ch, fs, l.z > 0.7, l.isWord ? pal.acc : pal.ink);
            const double w = g.width() / c.px, h = g.height() / c.px;
            p.save();
            p.translate(l.x, l.y);
            p.rotate(qRadiansToDegrees(l.rot));
            p.setOpacity(l.isWord ? 0.8 : 0.1 + 0.32 * l.z);
            blit(p, g, -w / 2, -h / 2, w, h);
            p.restore();
        }
    }

private:
    struct Ltr { double z = 0, x = 0, y = 0, v = 0, rot = 0, vr = 0, vx = 0, word = 0, ph = 0; QChar ch; bool isWord = false; };
    void spawn(Ltr& l, bool any, const Ctx& c)
    {
        static const QString ABC = QStringLiteral("abcdefghijklmnopqrstuvwxyzáéçãõ");
        l.z = R(0.3, 1); l.ch = ABC.at(RI(ABC.size())); l.x = R(-20, c.W); l.y = any ? R(-30, c.H) : R(-60, -20);
        l.v = 16 + 34 * l.z; l.rot = R(-0.6, 0.6); l.vr = R(-0.6, 0.6); l.vx = 0; l.word = 0; l.ph = R(0, 6.28); l.isWord = false;
    }
    // As palavras saem no idioma do app.
    static const QStringList& words()
    {
        static const QHash<QString, QStringList> W = {
            { QStringLiteral("pt"), { "era", "uma", "vez", "lua", "mar", "fim", "casa", "tempo", "nome", "sonho", "noite", "carta", "chuva", "pausa", "saudade", "capítulo" } },
            { QStringLiteral("en"), { "once", "upon", "time", "moon", "sea", "end", "home", "dream", "night", "letter", "rain", "pause", "story", "chapter", "ink", "page" } },
            { QStringLiteral("es"), { "érase", "una", "vez", "luna", "mar", "fin", "casa", "tiempo", "sueño", "noche", "carta", "lluvia", "pausa", "nostalgia", "capítulo", "tinta" } },
            { QStringLiteral("it"), { "c'era", "una", "volta", "luna", "mare", "fine", "casa", "tempo", "sogno", "notte", "lettera", "pioggia", "pausa", "nostalgia", "capitolo", "inchiostro" } },
            { QStringLiteral("fr"), { "il", "était", "une", "fois", "lune", "mer", "fin", "maison", "temps", "rêve", "nuit", "lettre", "pluie", "pause", "chapitre", "encre" } },
        };
        const QString lang = QSettings().value(QStringLiteral("app/language"), QStringLiteral("pt_BR")).toString().left(2);
        auto it = W.constFind(lang);
        if (it == W.constEnd()) it = W.constFind(QStringLiteral("pt"));
        return it.value();
    }
    const QPixmap& glyph(const Ctx& c, QChar ch, int size, bool bold, const QColor& col)
    {
        const QString key = QString(ch) + QLatin1Char('|') + QString::number(size) + (bold ? QLatin1Char('b') : QLatin1Char('n')) + col.name();
        auto it = m_glyphs.find(key);
        if (it != m_glyphs.end()) return it.value();
        const double side = size * 1.6;
        QPixmap pm = layer(c, side, side);
        QPainter x(&pm);
        x.setRenderHint(QPainter::TextAntialiasing);
        x.setPen(col);
        x.setFont(font(size, bold));
        x.drawText(QRectF(0, 0, side, side), Qt::AlignCenter, QString(ch));
        x.end();
        return m_glyphs.insert(key, pm).value();
    }
    QFont font(double size, bool bold)
    {
        const int key = int(std::lround(size * 2)) * 2 + (bold ? 1 : 0);
        auto it = m_fonts.find(key);
        if (it != m_fonts.end()) return it.value();
        QFont f(QStringLiteral("Lora"));
        f.setPixelSize(qMax(1, int(std::lround(size))));
        f.setWeight(bold ? QFont::DemiBold : QFont::Normal);
        m_fonts.insert(key, f);
        return f;
    }
    std::vector<Ltr> m_letters;
    QHash<int, QFont> m_fonts;
    QHash<QString, QPixmap> m_glyphs;
    double m_glyphPx = 0;
    double m_t = 0, m_nextWord = 2;
};

// ============================================================ farol

class Lighthouse : public Layer {
public:
    void init(Ctx&) override { m_stars.reset(); }
    void step(double dt, Ctx& c) override { m_t += dt * (0.7 + 0.3 * INT(c)); }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        skyTOD(p, pal, c, mix(pal.app, Qt::black, 0.3), mix(pal.app, hex("#1c3348"), 0.3));
        const double L = LVopen(pal, c);
        if (!m_stars.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            for (int i = 0; i < int(std::lround(160 * c.W / 1440)); ++i)
                x.fillRect(QRectF(R(0, c.W), R(0, c.H * 0.7), 1.2, 1.2), alpha(Qt::white, R(0.15, 0.5)));
            x.end();
            m_stars.set(c, pm);
        }
        p.setOpacity(1 - L);
        blit(p, m_stars.pm, 0, 0, c.W, c.H);
        p.setOpacity(1);
        const double sea = c.H - 120, lx = anchorX(c, 0.7), ly = sea - 250, th = m_t * 0.9, cs = std::cos(th), toward = std::sin(th);
        // mar
        p.fillRect(QRectF(0, sea, c.W, c.H - sea), mix(mix(pal.app, Qt::black, 0.4), hex("#3f6f8f"), L * 0.7));
        const QColor glint = mix(pal.acc, Qt::white, 0.4);
        const int nGlint = int(std::lround(70 * c.W / 1440));
        for (int i = 0; i < nGlint; ++i) {
            const double y = sea + 6 + std::fmod(i * 37.0, c.H - sea - 6), x = std::fmod(i * 211.0 + m_t * (8 + i % 5), c.W + 80) - 40;
            p.setPen(QPen(alpha(glint, 0.08 + 0.12 * qMax(0.0, std::sin(m_t * 1.5 + i))), 1));
            p.drawLine(QPointF(x, y), QPointF(x + 18 + (i % 4) * 6, y));
        }
        // feixe
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.setOpacity(1 - 0.75 * L);
        const double len = 1500 * std::abs(cs), dir = cs >= 0 ? 1 : -1, spread = 38 + 80 * (1 - std::abs(cs));
        if (len > 30) {
            QLinearGradient g(lx, ly, lx + dir * len, ly);
            g.setColorAt(0, alpha(mix(pal.acc, Qt::white, 0.5), 0.4));
            g.setColorAt(1, alpha(pal.acc, 0));
            QPainterPath beam(QPointF(lx, ly - 4));
            beam.lineTo(lx + dir * len, ly - spread);
            beam.lineTo(lx + dir * len, ly + spread);
            beam.lineTo(lx, ly + 4);
            beam.closeSubpath();
            p.fillPath(beam, g);
        }
        const double flashA = toward > 0 ? std::pow(qMax(0.0, 1 - std::abs(cs) * 3), 2) : 0;
        QRadialGradient gl(QPointF(lx, ly), 60 + 260 * flashA);
        gl.setColorAt(0, alpha(hex("#fff2c8"), 0.6 + 0.4 * flashA));
        gl.setColorAt(1, alpha(pal.acc, 0));
        p.fillRect(QRectF(lx - 340, ly - 340, 680, 680), gl);
        if (flashA > 0.05) p.fillRect(QRectF(0, 0, c.W, c.H), alpha(mix(pal.acc, Qt::white, 0.6), 0.12 * flashA));
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setOpacity(1);
        // torre e rocha
        const QColor dk = mix(mix(pal.app, Qt::black, 0.55), hex("#4a5560"), L * 0.6);
        QPainterPath rock(QPointF(lx - 120, c.H));
        rock.quadTo(lx - 80, sea - 30, lx - 10, sea - 22);
        rock.quadTo(lx + 70, sea - 30, lx + 130, c.H);
        rock.closeSubpath();
        p.fillPath(rock, dk);
        QPainterPath tower(QPointF(lx - 24, sea - 20));
        tower.lineTo(lx - 13, ly + 22);
        tower.lineTo(lx + 13, ly + 22);
        tower.lineTo(lx + 24, sea - 20);
        tower.closeSubpath();
        p.fillPath(tower, mix(pal.app, hex("#d8d0c0"), 0.35));
        const QColor stripe = mix(pal.app, hex("#a83a32"), 0.55);
        auto lerp = [](double a, double b, double k) { return a + (b - a) * k; };
        for (int i = 0; i < 3; ++i) {
            const double k0 = (i * 2 + 1) / 6.0, k1 = (i * 2 + 2) / 6.0;
            const double y0 = lerp(sea - 20, ly + 22, k0), y1 = lerp(sea - 20, ly + 22, k1), w0 = lerp(24, 13, k0), w1 = lerp(24, 13, k1);
            QPainterPath s(QPointF(lx - w0, y0));
            s.lineTo(lx - w1, y1);
            s.lineTo(lx + w1, y1);
            s.lineTo(lx + w0, y0);
            s.closeSubpath();
            p.fillPath(s, stripe);
        }
        p.fillRect(QRectF(lx - 17, ly + 14, 34, 8), dk);
        p.fillRect(QRectF(lx - 12, ly - 12, 24, 4), dk);
        QPainterPath roof(QPointF(lx - 13, ly - 12));
        roof.lineTo(lx, ly - 26);
        roof.lineTo(lx + 13, ly - 12);
        roof.closeSubpath();
        p.fillPath(roof, dk);
        p.fillRect(QRectF(lx - 9, ly - 8, 18, 22), alpha(hex("#fff2c8"), 0.9));
    }

private:
    Cached m_stars;
    double m_t = 0;
};

// ============================================================ lâmpada de lava

class Lava : public Layer {
public:
    void init(Ctx& c) override
    {
        m_rw = qMax(60, int(std::lround(RH * c.W / c.H)));
        m_img = QImage(m_rw, RH, QImage::Format_ARGB32_Premultiplied);
        m_blobs.clear();
        const int n = int(std::lround(9 * qMin(1.4, INT(c)) * c.W / 1440));
        for (int i = 0; i < qMax(4, n); ++i) m_blobs.push_back({ R(0, m_rw), R(4.5, 9), R(0.025, 0.06), R(0, 6.28), R(-8, 8), R(0.02, 0.05) });
    }
    void step(double dt, Ctx&) override { m_t += dt; }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const QColor lo = pal.acc, hi = mix(pal.acc, hex("#ff3d7f"), 0.6), bg = mix(pal.app, Qt::black, 0.15);
        struct B { double x, y, r2; };
        std::vector<B> bs;
        for (const Blob& b : m_blobs)
            bs.push_back({ b.x + std::sin(m_t * b.fx * 6.28 + b.ph) * b.dx, RH / 2.0 + std::sin(m_t * b.f * 6.28 + b.ph) * (RH / 2.0 + 6), b.r * b.r });
        const double lr = lo.redF() * 255, lg = lo.greenF() * 255, lb = lo.blueF() * 255;
        const double hr = hi.redF() * 255, hg = hi.greenF() * 255, hb = hi.blueF() * 255;
        const double br = bg.redF() * 255, bgc = bg.greenF() * 255, bb = bg.blueF() * 255;
        for (int y = 0; y < RH; ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(m_img.scanLine(y));
            const double k = double(y) / RH;
            const double cr = hr + (lr - hr) * k, cg = hg + (lg - hg) * k, cb = hb + (lb - hb) * k;
            for (int x = 0; x < m_rw; ++x) {
                double f = 0;
                for (const B& b : bs) { const double dx = x - b.x, dy = (y - b.y) * 1.15; f += b.r2 / (dx * dx + dy * dy + 0.5); }
                const double a = smooth(0.85, 1.25, f), core = smooth(1.3, 3, f) * 0.25;
                auto ch = [&](double base, double col) {
                    const double off = base + (col - base) * 0.08, on = qMin(255.0, col + 255 * core);
                    return int(qBound(0.0, off + (on - off) * a, 255.0));
                };
                line[x] = qRgb(ch(br, cr), ch(bgc, cg), ch(bb, cb));
            }
        }
        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(QRectF(0, 0, c.W, c.H), m_img);
        p.restore();
        QLinearGradient g(0, 0, 0, c.H);
        g.setColorAt(0, alpha(Qt::black, 0.25));
        g.setColorAt(0.5, alpha(Qt::black, 0));
        g.setColorAt(1, alpha(Qt::black, 0.3));
        p.fillRect(QRectF(0, 0, c.W, c.H), g);
    }

private:
    static constexpr int RH = 84;
    struct Blob { double x, r, f, ph, dx, fx; };
    std::vector<Blob> m_blobs;
    QImage m_img;
    int m_rw = 160;
    double m_t = 0;
};

// Sem cenário: só o céu da hora escolhida (nos temas que são só clima).
class None : public Layer {
public:
    void init(Ctx&) override {}
    void step(double, Ctx&) override {}
    void paint(QPainter& p, const Pal& pal, Ctx& c) override { skyTOD(p, pal, c); }
};

std::unique_ptr<Layer> makeScene(const QString& id)
{
    // natureza com volume (AnimNature.cpp): farol, charneca, campina e céu estrelado
    if (auto l = makeNature(id)) return l;
    if (id == QLatin1String("pines")) return std::make_unique<Forest>();
    if (id == QLatin1String("branch")) return std::make_unique<Branch>();
    if (id == QLatin1String("meadow")) return std::make_unique<Meadow>();
    if (id == QLatin1String("window")) return std::make_unique<Window>();
    if (id == QLatin1String("sun")) return std::make_unique<Sunbeam>();
    if (id == QLatin1String("aurora")) return std::make_unique<Aurora>();
    if (id == QLatin1String("stars")) return std::make_unique<Stars>(true);
    if (id == QLatin1String("moor")) return std::make_unique<Moor>();
    if (id == QLatin1String("koi")) return std::make_unique<Koi>();
    if (id == QLatin1String("hearth")) return std::make_unique<Fire>(false);
    if (id == QLatin1String("candle")) return std::make_unique<Fire>(true);
    if (id == QLatin1String("letters")) return std::make_unique<Letters>();
    if (id == QLatin1String("light")) return std::make_unique<Lighthouse>();
    if (id == QLatin1String("lava")) return std::make_unique<Lava>();
    if (id == QLatin1String("neon")) return makeNeonSign();
    if (id == QLatin1String("city")) return makeCity();
    if (id == QLatin1String("paulista")) return makeCityDepth(true, true, false, false);
    if (id == QLatin1String("train")) return makeCityDepth(false, false, true, false);
    if (id == QLatin1String("harbor")) return makeCityDepth(false, false, false, true);
    if (id == QLatin1String("town")) return makeTown();
    if (id == QLatin1String("neoncity")) return makeNeonDistrict();
    if (id == QLatin1String("retro")) return makeRetroGrid();
    // levas 2 a 4 (AnimLeva*.cpp)
    if (auto l = makeLeva2a(id)) return l;
    if (auto l = makeLeva2b(id)) return l;
    if (auto l = makeDesert(id)) return l;
    if (auto l = makeGenresA(id)) return l;   // leva 7, gêneros (AnimGenresA/B.cpp)
    if (auto l = makeGenresB(id)) return l;   // Last Exit e Route 66 (AnimDesert.cpp)
    if (auto l = makeLeva3(id)) return l;
    if (auto l = makeLeva4(id)) return l;
    if (auto l = makeLeva5(id)) return l;
    return std::make_unique<None>();
}

std::unique_ptr<Layer> makeExtra(const QString& id)
{
    if (id == QLatin1String("stars")) return std::make_unique<Stars>(false);
    if (id == QLatin1String("fog")) return std::make_unique<Fog>();
    if (id == QLatin1String("leaves")) return std::make_unique<Leaves>(false);
    if (id == QLatin1String("petals")) return std::make_unique<Leaves>(true);
    if (id == QLatin1String("fireflies")) return std::make_unique<Fireflies>();
    if (id == QLatin1String("snow")) return std::make_unique<Snow>();
    return nullptr;
}

Pal paletteOf(const Theme::MiraTheme& t)
{
    Pal p;
    p.app = Theme::toColor(t.appBackground);
    p.page = Theme::toColor(t.editorBackground);
    p.pageInk = Theme::toColor(t.editorTextColor);
    p.ink = Theme::toColor(t.textPrimary);
    p.acc = Theme::toColor(t.accentDefault);
    p.green = Theme::toColor(t.accentSuccess.isEmpty() ? QStringLiteral("#7BC592") : t.accentSuccess);
    p.dark = luma(p.app) < 0.45;
    p.part = p.dark ? mix(p.app, Qt::white, 0.78) : mix(p.app, p.pageInk, 0.55);
    return p;
}

QString encode(const Config& c) { return c.tod + QLatin1Char('|') + c.rain + QLatin1Char('|') + c.extras.join(QLatin1Char(',')); }

} // namespace

// ============================================================ preferências e Personalizar

bool Config::operator==(const Config& o) const
{
    QStringList a = extras, b = o.extras;
    a.sort();
    b.sort();
    return tod == o.tod && rain == o.rain && a == b;
}

Notifier* Notifier::instance()
{
    static Notifier* n = new Notifier;
    return n;
}

bool enabled() { return QSettings().value(QStringLiteral("anim/enabled"), true).toBool(); }

void setEnabled(bool on)
{
    QSettings().setValue(QStringLiteral("anim/enabled"), on);
    emit Notifier::instance()->changed();
}

int intensity() { return qBound(0, QSettings().value(QStringLiteral("anim/intensity"), 1).toInt(), 2); }

void setIntensity(int level)
{
    QSettings().setValue(QStringLiteral("anim/intensity"), qBound(0, level, 2));
    emit Notifier::instance()->changed();
}

int quality()
{
    const int q = QSettings().value(QStringLiteral("anim/quality"), 100).toInt();
    return q <= 50 ? 50 : q <= 75 ? 75 : 100;
}

void setQuality(int percent)
{
    QSettings().setValue(QStringLiteral("anim/quality"), percent);
    emit Notifier::instance()->changed();
}

int fps()
{
    const int f = QSettings().value(QStringLiteral("anim/fps"), 30).toInt();
    return f <= 15 ? 15 : f <= 20 ? 20 : 30;
}

void setFps(int value)
{
    QSettings().setValue(QStringLiteral("anim/fps"), value);
    emit Notifier::instance()->changed();
}

Config factoryConfig(const Theme::MiraTheme& t)
{
    Config c;
    c.tod = t.animTod.isEmpty() ? QStringLiteral("theme") : t.animTod;
    c.rain = t.animRain.isEmpty() ? QStringLiteral("none") : t.animRain;
    c.extras = t.animExtras;
    return c;
}

Config config(const Theme::MiraTheme& t)
{
    const QString raw = QSettings().value(QStringLiteral("anim/themes/") + t.id).toString();
    if (raw.isEmpty()) return factoryConfig(t);
    const QStringList parts = raw.split(QLatin1Char('|'));
    if (parts.size() != 3) return factoryConfig(t);
    Config c;
    c.tod = parts.at(0);
    c.rain = parts.at(1);
    c.extras = parts.at(2).split(QLatin1Char(','), Qt::SkipEmptyParts);
    return c;
}

void setConfig(const QString& themeId, const Config& c)
{
    QSettings().setValue(QStringLiteral("anim/themes/") + themeId, encode(c));
    emit Notifier::instance()->changed();
}

void resetConfig(const QString& themeId)
{
    QSettings().remove(QStringLiteral("anim/themes/") + themeId);
    emit Notifier::instance()->changed();
}

double clockHour()
{
    // QENNA_ANIM_HOUR fixa a hora (pra testar sem esperar o dia passar)
    bool ok = false;
    const double fixedH = qEnvironmentVariable("QENNA_ANIM_HOUR").toDouble(&ok);
    if (ok) return fixedH;
    return QTime::currentTime().msecsSinceStartOfDay() / 3600000.0;
}

bool continuousTod(const QString& scene)
{
    return scene == QLatin1String("paulista") || scene == QLatin1String("train") || scene == QLatin1String("harbor")
        || scene == QLatin1String("light") || scene == QLatin1String("moor") || scene == QLatin1String("meadow");
}

bool todAllowed(const QString& scene)
{
    static const QStringList open = { QStringLiteral("none"), QStringLiteral("pines"), QStringLiteral("city"), QStringLiteral("light"),
                                       QStringLiteral("moor"), QStringLiteral("koi"), QStringLiteral("branch"), QStringLiteral("meadow"),
                                       QStringLiteral("paulista"), QStringLiteral("train"), QStringLiteral("harbor"), QStringLiteral("town"),
                                       QStringLiteral("clouds"), QStringLiteral("prairie") };
    return open.contains(scene);
}

QStringList extraIds()
{
    return { QStringLiteral("stars"), QStringLiteral("fog"), QStringLiteral("leaves"),
             QStringLiteral("petals"), QStringLiteral("fireflies"), QStringLiteral("snow") };
}

// ============================================================ pilha

struct Stack::Impl {
    Ctx c;
    bool real = false;   // Hora real: a hora anda junto com o relógio
    bool cont = false;   // a cena tem luz contínua (senão usa a hora fixa mais perto)
    std::unique_ptr<Layer> scene;
    std::vector<std::unique_ptr<Layer>> extras;   // na ordem de extraIds()
    std::unique_ptr<Layer> rain;

    template <class F> void each(F f)
    {
        if (scene) f(*scene);
        for (auto& e : extras) f(*e);
        if (rain) f(*rain);
    }
};

Stack::Stack() : d(std::make_unique<Impl>()) {}
Stack::~Stack() = default;

void Stack::setup(const Theme::MiraTheme& t, const Config& cfg, double sceneW, const Zones& zones)
{
    m_themeId = t.id;
    d->scene.reset();
    d->extras.clear();
    d->rain.reset();
    if (t.animScene.isEmpty()) return;
    Ctx& c = d->c;
    c.pal = paletteOf(t);
    c.W = qMax(200.0, sceneW);
    c.zones = zones;
    c.intensity = intensity();
    c.tod = todAllowed(t.animScene) ? cfg.tod : QStringLiteral("theme");
    d->real = c.tod == QLatin1String("real");
    d->cont = continuousTod(t.animScene);
    if (d->real) tickHour();
    c.rain = cfg.rain;
    c.neon = t.animScene == QLatin1String("neon") || t.animScene == QLatin1String("neoncity");
    c.hasNeonCols = false;
    d->scene = makeScene(t.animScene);
    for (const QString& id : extraIds())
        if (cfg.extras.contains(id))
            if (auto e = makeExtra(id)) d->extras.push_back(std::move(e));
    if (cfg.rain == QLatin1String("drizzle")) d->rain = std::make_unique<Rain>(2);
    else if (cfg.rain == QLatin1String("rain")) d->rain = std::make_unique<Rain>(0);
    else if (cfg.rain == QLatin1String("storm")) d->rain = std::make_unique<Rain>(1);
    d->each([&](Layer& l) { l.init(c); });
    // Avança um pouco pra primeira imagem já ter chuva e neve espalhadas.
    for (int i = 0; i < 90; ++i) step(1.0 / 30);
}

void Stack::resize(double sceneW, const Zones& zones)
{
    Ctx& c = d->c;
    const bool widthChanged = qAbs(sceneW - c.W) > 1;
    c.zones = zones;
    if (!widthChanged) return;
    c.W = qMax(200.0, sceneW);
    d->each([&](Layer& l) { l.resized(c); });
}

bool Stack::isEmpty() const { return !d->scene; }

void Stack::tickHour()
{
    Ctx& c = d->c;
    c.hour = clockHour();
    c.tod = d->cont ? QStringLiteral("real") : nearestTod(c.hour);
}

void Stack::step(double dt)
{
    dt = qBound(0.0, dt, 0.1);
    if (d->real) tickHour();
    d->each([&](Layer& l) { l.step(dt, d->c); });
}

void Stack::paint(QPainter& p, double pxPerUnit)
{
    if (!d->scene) return;
    d->c.px = qMax(0.25, pxPerUnit);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    d->each([&](Layer& l) {
        p.save();
        l.paint(p, d->c.pal, d->c);
        p.restore();
    });
}

// ============================================================ prévia

namespace {

Zones previewZones(double W)
{
    // A cena da prévia é a janela do Qenna: folha de 410 a 1076 em 1440.
    if (qAbs(W - 1440) < 2) return { QRectF(45, 0, 365, kSceneH), QRectF(1076, 0, 364, kSceneH) };
    return { QRectF(W * 0.03, 0, W * 0.22, kSceneH), QRectF(W * 0.75, 0, W * 0.25, kSceneH) };
}

struct LiveState {
    QString key;
    Stack stack;
    QElapsedTimer clock;
    qint64 lastUse = 0;
};

} // namespace

void paintPreview(QPainter& p, const QRectF& r, const Theme::MiraTheme& t, bool live)
{
    if (t.animScene.isEmpty() || r.height() < 4) return;
    const double s = r.height() / kSceneH, W = r.width() / s;
    const Config cfg = config(t);
    QString key = t.id + QLatin1Char('#') + encode(cfg) + QLatin1Char('#') + QString::number(int(W)) + QLatin1Char('#') + QString::number(intensity());
    if (cfg.tod == QLatin1String("real")) key += QLatin1Char('#') + QString::number(int(clockHour() * 4));
    p.save();
    p.setClipRect(r, Qt::IntersectClip);
    p.translate(r.topLeft());
    p.scale(s, s);
    const double dpr = p.device() ? p.device()->devicePixelRatioF() : 1.0;
    const double px = qAbs(p.worldTransform().m11()) * dpr;
    if (live) {
        // Uma animação por tema: a prévia compara dois lado a lado.
        static std::map<QString, std::unique_ptr<LiveState>> states;
        static qint64 useCounter = 0;
        auto& slot = states[t.id];
        if (!slot) slot = std::make_unique<LiveState>();
        LiveState& st = *slot;
        st.lastUse = ++useCounter;
        if (st.key != key) {
            st.key = key;
            st.stack.setup(t, cfg, W, previewZones(W));
            st.clock.start();
        }
        st.stack.step(st.clock.isValid() ? st.clock.restart() / 1000.0 : 0);
        st.stack.paint(p, px);
        while (states.size() > 4) {
            auto oldest = states.begin();
            for (auto it = states.begin(); it != states.end(); ++it)
                if (it->second->lastUse < oldest->second->lastUse) oldest = it;
            states.erase(oldest);
        }
    } else {
        // Miniatura: um quadro parado, guardado por tema e tamanho.
        static QCache<QString, QPixmap> cache(80);
        const QString ck = key + QLatin1Char('#') + QString::number(px, 'f', 2);
        QPixmap* pm = cache.object(ck);
        if (!pm) {
            Stack stack;
            stack.setup(t, cfg, W, previewZones(W));
            auto* img = new QPixmap(qMax(1, qCeil(W * px)), qMax(1, qCeil(kSceneH * px)));
            img->setDevicePixelRatio(px);
            img->fill(Qt::transparent);
            QPainter x(img);
            stack.paint(x, px);
            x.end();
            cache.insert(ck, img);
            pm = img;
        }
        p.drawPixmap(QRectF(0, 0, W, kSceneH), *pm, QRectF(QPointF(0, 0), QSizeF(pm->size())));
    }
    p.restore();
}

} // namespace AnimBg
