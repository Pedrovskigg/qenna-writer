#include "AmbienceArt.h"

#include <QHash>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <cmath>

namespace {

using C = AmbienceManager::Category;

// mulberry32, igual ao do lab, pra mesma cena sair do mesmo nome.
struct Rng {
    quint32 s;
    double operator()()
    {
        s += 0x6D2B79F5u;
        quint32 t = s;
        t = (t ^ (t >> 15)) * (1u | t);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return double(t ^ (t >> 14)) / 4294967296.0;
    }
};

quint32 fnv(const QString& s)
{
    quint32 h = 2166136261u;
    for (const QChar ch : s) { h ^= ch.unicode(); h *= 16777619u; }
    return h;
}

struct Ctx {
    QPainter& p;
    double W, H, t;
    Rng r;
};

void grad(Ctx& c, const QColor& a, const QColor& b)
{
    QLinearGradient g(0, 0, 0, c.H);
    g.setColorAt(0, a);
    g.setColorAt(1, b);
    c.p.fillRect(QRectF(0, 0, c.W, c.H), g);
}

void stars(Ctx& c, int n)
{
    const double sz = c.W / 160.0 + 0.5;
    for (int i = 0; i < n; ++i) {
        const double sx = c.r() * c.W, sy = c.r() * c.H * 0.7, ph = c.r() * 6;
        QColor w(255, 255, 255);
        w.setAlphaF(0.35 + 0.65 * std::abs(std::sin(c.t * 1.3 + ph)));
        c.p.fillRect(QRectF(sx, sy, sz, sz), w);
    }
}

void hills(Ctx& c, double y, double amp, const QColor& col)
{
    const double ph = c.r() * 6;
    QPainterPath path(QPointF(0, c.H));
    for (int i = 0; i <= 24; ++i)
        path.lineTo(i / 24.0 * c.W, y + std::sin(i * 0.55 + ph) * amp + std::sin(i * 1.7 + ph * 2) * amp * 0.3);
    path.lineTo(c.W, c.H);
    c.p.fillPath(path, col);
}

void waves(Ctx& c, double y, const QColor& col, double t, double amp)
{
    QPainterPath path(QPointF(0, c.H));
    for (int i = 0; i <= 40; ++i)
        path.lineTo(i / 40.0 * c.W, y + std::sin(i * 0.8 + t * 1.6) * amp);
    path.lineTo(c.W, c.H);
    c.p.fillPath(path, col);
}

void pine(Ctx& c, double cx, double base, double h, const QColor& col)
{
    QPainterPath path(QPointF(cx, base - h));
    path.lineTo(cx + h * 0.28, base);
    path.lineTo(cx - h * 0.28, base);
    path.closeSubpath();
    c.p.fillPath(path, col);
}

void moon(Ctx& c, double cx, double cy, double rr, const QColor& col, const QColor& glow)
{
    QRadialGradient g(QPointF(cx, cy), rr * 3.2);
    g.setColorAt(0.5 / 3.2, glow);
    g.setColorAt(1, Qt::transparent);
    c.p.fillRect(QRectF(cx - rr * 4, cy - rr * 4, rr * 8, rr * 8), g);
    c.p.setPen(Qt::NoPen);
    c.p.setBrush(col);
    c.p.drawEllipse(QPointF(cx, cy), rr, rr);
}

void branch(Ctx& c, double px, double py, double len, double ang, int d)
{
    if (d == 0) return;
    const double nx = px + std::cos(ang) * len, ny = py + std::sin(ang) * len;
    QPen pen(QColor(7, 5, 11), d * 0.45, Qt::SolidLine, Qt::RoundCap);
    c.p.setPen(pen);
    c.p.drawLine(QPointF(px, py), QPointF(nx, ny));
    branch(c, nx, ny, len * 0.72, ang - 0.45 - c.r() * 0.3, d - 1);
    branch(c, nx, ny, len * 0.7, ang + 0.4 + c.r() * 0.3, d - 1);
}

QColor rgba(int r, int g, int b, double a)
{
    QColor q(r, g, b);
    q.setAlphaF(a);
    return q;
}

void nature(Ctx& c, const QString& n)
{
    const double W = c.W, H = c.H;
    const bool night = n.contains(QLatin1String("Night")) || n.contains(QLatin1String("Campfire"));
    if (night) {
        grad(c, QColor(0x0f1d33), QColor(0x2a3f5c));
        stars(c, 22);
        moon(c, W * 0.74, H * 0.24, W * 0.07, QColor(0xf3efd8), rgba(243, 239, 216, 0.25));
    } else {
        grad(c, QColor(0xd6ecd9), QColor(0x86bfa0));
    }
    if (n.contains(QLatin1String("Sea"))) {
        waves(c, H * 0.72, QColor(0x4f8fae), c.t, H * 0.025);
        waves(c, H * 0.8, QColor(0x2f6a8a), c.t + 1, H * 0.02);
    } else {
        hills(c, H * 0.62, H * 0.05, night ? QColor(0x24394f) : QColor(0x5f9c7c));
        hills(c, H * 0.75, H * 0.04, night ? QColor(0x182a3b) : QColor(0x3c7258));
        for (int i = 0; i < 7; ++i) {
            const double x = c.r() * W, base = H * (0.8 + c.r() * 0.2), h = H * (0.2 + c.r() * 0.2);
            pine(c, x, base, h, night ? QColor(0x0d1a26) : QColor(0x21483a));
        }
    }
    if (n.contains(QLatin1String("Campfire"))) {
        const double fx = W * 0.5, fy = H * 0.86, f = 1 + std::sin(c.t * 9) * 0.08;
        QRadialGradient g(QPointF(fx, fy), W * 0.32);
        g.setColorAt(0, rgba(255, 170, 80, 0.55));
        g.setColorAt(1, Qt::transparent);
        c.p.fillRect(QRectF(0, 0, W, H), g);
        QPainterPath big(QPointF(fx - W * 0.05, fy));
        big.quadTo(fx, fy - H * 0.2 * f, fx + W * 0.05, fy);
        c.p.fillPath(big, QColor(0xffb347));
        QPainterPath small(QPointF(fx - W * 0.02, fy));
        small.quadTo(fx, fy - H * 0.09 * f, fx + W * 0.02, fy);
        c.p.fillPath(small, QColor(0xfff1b8));
    }
    if (!night) {
        c.p.setPen(QPen(rgba(40, 60, 50, 0.7), W / 45.0, Qt::SolidLine, Qt::RoundCap));
        c.p.setBrush(Qt::NoBrush);
        for (int i = 0; i < 3; ++i) {
            const double bx = W * (0.2 + c.r() * 0.6) + std::sin(c.t + i) * W * 0.02;
            const double by = H * (0.18 + c.r() * 0.2);
            QPainterPath b(QPointF(bx - W * 0.035, by - H * 0.015));
            b.quadTo(bx, by + H * 0.01, bx, by);
            b.quadTo(bx, by + H * 0.01, bx + W * 0.035, by - H * 0.015);
            c.p.drawPath(b);
        }
    }
}

void rainSea(Ctx& c, const QString& n)
{
    const double W = c.W, H = c.H;
    const bool sea = n.contains(QLatin1String("Sea"));
    const bool storm = n.contains(QLatin1String("Thunder")) || n.contains(QLatin1String("Angry"));
    grad(c, storm ? QColor(0x3a4458) : QColor(0x71849b), storm ? QColor(0x1a2130) : QColor(0x33445a));
    if (storm && std::fmod(c.t * 0.7 + c.r(), 3.0) < 0.12)
        c.p.fillRect(QRectF(0, 0, W, H), rgba(230, 240, 255, 0.35));
    c.p.setPen(Qt::NoPen);
    c.p.setBrush(storm ? QColor(0x2a3344) : QColor(0x56677d));
    for (int i = 0; i < 5; ++i) {
        const double cx = W * (i / 4.0) + c.r() * W * 0.1, rr = W * (0.14 + c.r() * 0.08);
        c.p.drawEllipse(QPointF(cx, H * 0.12), rr, rr);
    }
    if (storm) {
        c.p.setPen(QPen(QColor(0xe9f1ff), W / 35.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        c.p.setBrush(Qt::NoBrush);
        const double lx = W * (0.3 + c.r() * 0.4);
        const QPointF pts[] = {{lx, H * 0.2}, {lx - W * 0.05, H * 0.38}, {lx + W * 0.02, H * 0.38}, {lx - W * 0.04, H * 0.58}};
        c.p.drawPolyline(pts, 4);
    }
    if (!sea || n == QLatin1String("Angry Sea")) {
        c.p.setPen(QPen(rgba(215, 230, 250, 0.55), std::max(0.7, W / 70.0)));
        const int cnt = (n.contains(QLatin1String("Heavy")) || n.contains(QLatin1String("Thunder"))) ? 46 : 24;
        for (int i = 0; i < cnt; ++i) {
            const double rx = c.r() * W * 1.2;
            const double ry = std::fmod(c.r() * H + c.t * H * 0.9, H);
            c.p.drawLine(QPointF(rx, ry), QPointF(rx - W * 0.03, ry + H * 0.08));
        }
    }
    if (sea) {
        waves(c, H * 0.66, QColor(0x3e6c8c), c.t, H * (storm ? 0.05 : 0.025));
        waves(c, H * 0.76, QColor(0x28506e), c.t + 1.3, H * (storm ? 0.045 : 0.02));
        waves(c, H * 0.86, QColor(0x1a3a54), c.t + 2.1, H * 0.018);
    } else {
        c.p.fillRect(QRectF(0, H * 0.86, W, H * 0.14), rgba(20, 30, 45, 0.55));
    }
}

void places(Ctx& c, const QString& n)
{
    const double W = c.W, H = c.H;
    if (n.contains(QLatin1String("Space"))) {
        grad(c, QColor(0x05060f), QColor(0x1d1546));
        stars(c, 40);
        c.p.setPen(Qt::NoPen);
        c.p.setBrush(QColor(0xc98bdb));
        c.p.drawEllipse(QPointF(W * 0.6, H * 0.62), W * 0.2, W * 0.2);
        c.p.save();
        c.p.translate(W * 0.6, H * 0.62);
        c.p.rotate(-20);
        c.p.setPen(QPen(rgba(255, 220, 255, 0.6), W / 30.0));
        c.p.setBrush(Qt::NoBrush);
        c.p.drawEllipse(QPointF(0, 0), W * 0.34, W * 0.07);
        c.p.restore();
        if (n.contains(QLatin1String("Engine"))) {
            c.p.save();
            c.p.translate(W * 0.28 + std::sin(c.t) * W * 0.02, H * 0.3);
            c.p.rotate(-28.6);
            c.p.fillRect(QRectF(-W * 0.08, -H * 0.025, W * 0.16, H * 0.05), QColor(0xdfe6f5));
            c.p.fillRect(QRectF(-W * 0.12, -H * 0.015, W * 0.04, H * 0.03), QColor(0x7fd3ff));
            c.p.restore();
        }
    } else if (n.contains(QLatin1String("Cyber"))) {
        grad(c, QColor(0x14062a), QColor(0x3b0d4d));
        c.p.setPen(QPen(rgba(255, 60, 200, 0.55), 0.6));
        for (int i = 0; i < 9; ++i) {
            const double yy = H * 0.6 + (i * i) * H * 0.006 + std::fmod(c.t * H * 0.02, H * 0.04);
            c.p.drawLine(QPointF(0, yy), QPointF(W, yy));
        }
        for (int i = -6; i <= 6; ++i)
            c.p.drawLine(QPointF(W / 2 + i * W * 0.04, H * 0.6), QPointF(W / 2 + i * W * 0.25, H));
        QPainterPath sun;
        sun.moveTo(W / 2 - W * 0.16, H * 0.5);
        sun.arcTo(QRectF(W / 2 - W * 0.16, H * 0.5 - W * 0.16, W * 0.32, W * 0.32), 180, -180);
        sun.closeSubpath();
        c.p.fillPath(sun, QColor(0xff5fd2));
    } else if (n.contains(QLatin1String("Cinematic"))) {
        grad(c, QColor(0x1e1714), QColor(0x0c0907));
        QRadialGradient g(QPointF(W * 0.5, H * 0.1), W * 0.6, QPointF(W * 0.5, H * 0.1));
        g.setColorAt(0, rgba(255, 214, 150, 0.6));
        g.setColorAt(1, Qt::transparent);
        QPainterPath beam(QPointF(W * 0.45, 0));
        beam.lineTo(W * 0.55, 0);
        beam.lineTo(W * 0.85, H);
        beam.lineTo(W * 0.15, H);
        beam.closeSubpath();
        c.p.fillPath(beam, g);
        c.p.fillRect(QRectF(W * 0.3, H * 0.72, W * 0.4, H * 0.06), QColor(0x3a2a20));
    } else {
        grad(c, QColor(0x1b2245), QColor(0x5a3d6a));
        stars(c, 10);
        for (int i = 0; i < 12; ++i) {
            const double bw = W * (0.06 + c.r() * 0.08), bh = H * (0.2 + c.r() * 0.45);
            const double bx = i / 12.0 * W * 1.05 - W * 0.03;
            c.p.fillRect(QRectF(bx, H - bh, bw, bh), (i % 2) ? QColor(0x121732) : QColor(0x0d1128));
            for (double wy = H - bh + H * 0.03; wy < H - H * 0.04; wy += H * 0.05)
                for (double wx = bx + W * 0.012; wx < bx + bw - W * 0.015; wx += W * 0.025)
                    if (c.r() > 0.55)
                        c.p.fillRect(QRectF(wx, wy, W * 0.01, H * 0.015), rgba(255, 210, 122, 0.85));
        }
        if (n.contains(QLatin1String("Traffic"))) {
            for (int i = 0; i < 5; ++i) {
                double cx = std::fmod(c.r() * W + c.t * W * 0.25 * ((i % 2) ? 1 : -1), W);
                if (cx < 0) cx += W;
                c.p.fillRect(QRectF(cx, H * 0.94, W * 0.03, H * 0.015), (i % 2) ? QColor(0xff5a4a) : QColor(0xfff3c4));
            }
        }
    }
}

void calm(Ctx& c, const QString& n)
{
    const double W = c.W, H = c.H;
    const double ph = c.r();
    grad(c, QColor::fromHslF(std::fmod(28 + ph * 20, 360) / 360.0, 0.75, 0.80),
         QColor::fromHslF(std::fmod(350 + ph * 20, 360) / 360.0, 0.45, 0.58));
    moon(c, W * (0.3 + ph * 0.4), H * 0.42, W * 0.13, rgba(255, 244, 220, 0.95), rgba(255, 230, 190, 0.35));
    hills(c, H * 0.66, H * 0.04, rgba(150, 80, 90, 0.55));
    hills(c, H * 0.76, H * 0.03, rgba(110, 55, 72, 0.7));
    if (n.contains(QLatin1String("Piano"))) {
        const double kw = W / 9.0;
        for (int i = 0; i < 9; ++i)
            c.p.fillRect(QRectF(i * kw + 0.5, H * 0.82, kw - 1, H * 0.18), QColor(0xf8f1e6));
        for (int i : {0, 1, 3, 4, 5, 7})
            c.p.fillRect(QRectF(i * kw + kw * 0.68, H * 0.82, kw * 0.6, H * 0.11), QColor(0x2b1f22));
    }
    if (n.contains(QLatin1String("Lighthouse"))) {
        QPainterPath tower(QPointF(W * 0.72, H * 0.76));
        tower.lineTo(W * 0.78, H * 0.76);
        tower.lineTo(W * 0.765, H * 0.45);
        tower.lineTo(W * 0.735, H * 0.45);
        tower.closeSubpath();
        c.p.fillPath(tower, QColor(0xf4ece0));
        const double a = std::sin(c.t * 0.8) * 0.4;
        QPainterPath beam(QPointF(W * 0.75, H * 0.46));
        beam.lineTo(0, H * (0.3 + a * 0.3));
        beam.lineTo(0, H * (0.42 + a * 0.3));
        beam.closeSubpath();
        c.p.fillPath(beam, rgba(255, 240, 180, 0.35));
    }
}

void adventure(Ctx& c, const QString& n)
{
    const double W = c.W, H = c.H;
    grad(c, QColor(0xffc27a), QColor(0xd9573c));
    moon(c, W * 0.7, H * 0.34, W * 0.11, QColor(0xfff0c9), rgba(255, 240, 200, 0.4));
    if (n.contains(QLatin1String("Pirate"))) {
        waves(c, H * 0.72, QColor(0x7a3346), c.t, H * 0.02);
        waves(c, H * 0.82, QColor(0x4d1f33), c.t + 1, H * 0.02);
        const double sx = W * 0.32, sy = H * 0.72 + std::sin(c.t * 1.6) * H * 0.01;
        QPainterPath hull(QPointF(sx - W * 0.14, sy));
        hull.lineTo(sx + W * 0.14, sy);
        hull.lineTo(sx + W * 0.1, sy + H * 0.05);
        hull.lineTo(sx - W * 0.1, sy + H * 0.05);
        hull.closeSubpath();
        c.p.fillPath(hull, QColor(0x3a1826));
        QPainterPath sail(QPointF(sx, sy - H * 0.3));
        sail.lineTo(sx + W * 0.12, sy - H * 0.04);
        sail.lineTo(sx, sy - H * 0.04);
        sail.closeSubpath();
        c.p.fillPath(sail, QColor(0xfbe6cf));
    } else {
        QPainterPath m(QPointF(0, H * 0.8));
        for (int i = 0; i <= 8; ++i)
            m.lineTo(i / 8.0 * W, H * ((i % 2) ? 0.42 + c.r() * 0.12 : 0.62 + c.r() * 0.1));
        m.lineTo(W, H);
        m.lineTo(0, H);
        m.closeSubpath();
        c.p.fillPath(m, QColor(0x8a3a44));
        hills(c, H * 0.8, H * 0.03, QColor(0x4a1d2c));
    }
}

void suspense(Ctx& c, const QString& n)
{
    const double W = c.W, H = c.H;
    grad(c, QColor(0x2c1a3c), QColor(0x0c0911));
    moon(c, W * 0.68, H * 0.28, W * 0.1, QColor(0xece4ff), rgba(200, 180, 255, 0.25));
    branch(c, W * 0.28, H, H * 0.3, -M_PI / 2 - 0.1, 6);
    c.p.setPen(Qt::NoPen);
    c.p.setBrush(rgba(200, 185, 235, 0.10));
    for (int i = 0; i < 3; ++i) {
        const double fy = H * (0.66 + i * 0.1);
        const double off = std::fmod(c.t * W * 0.04 * (i + 1), W);
        c.p.drawEllipse(QPointF(off, fy), W * 0.5, H * 0.05);
        c.p.drawEllipse(QPointF(off - W, fy), W * 0.5, H * 0.05);
    }
    if (n.contains(QLatin1String("Grave")) || n.contains(QLatin1String("Tomb"))) {
        c.p.setBrush(QColor(0x16101f));
        for (int i = 0; i < 3; ++i) {
            const double gx = W * (0.55 + i * 0.14), gy = H * 0.88;
            c.p.drawRect(QRectF(gx, gy - H * 0.12, W * 0.08, H * 0.14));
            c.p.drawPie(QRectF(gx, gy - H * 0.12 - W * 0.04, W * 0.08, W * 0.08), 0, 180 * 16);
        }
    }
}

// Música que o usuário trouxe, sem capa: um disco.
void mine(Ctx& c)
{
    const double W = c.W, H = c.H;
    const double hue = c.r();
    grad(c, QColor::fromHslF(hue, 0.35, 0.32), QColor::fromHslF(std::fmod(hue + 0.08, 1.0), 0.40, 0.16));
    const QPointF ctr(W * 0.5, H * 0.52);
    const double R = W * 0.34;
    c.p.setPen(Qt::NoPen);
    c.p.setBrush(QColor(0x141216));
    c.p.drawEllipse(ctr, R, R);
    c.p.setBrush(Qt::NoBrush);
    c.p.setPen(QPen(QColor(255, 255, 255, 22), 0.6));
    for (double rr = R * 0.45; rr < R; rr += R * 0.09) c.p.drawEllipse(ctr, rr, rr);
    c.p.save();
    c.p.translate(ctr);
    c.p.rotate(std::fmod(c.t * 40, 360));
    c.p.setPen(QPen(QColor(255, 255, 255, 40), W / 60.0));
    c.p.drawArc(QRectF(-R * 0.8, -R * 0.8, R * 1.6, R * 1.6), 30 * 16, 40 * 16);
    c.p.restore();
    c.p.setPen(Qt::NoPen);
    c.p.setBrush(QColor::fromHslF(hue, 0.55, 0.62));
    c.p.drawEllipse(ctr, R * 0.3, R * 0.3);
    c.p.setBrush(QColor(0x141216));
    c.p.drawEllipse(ctr, R * 0.05, R * 0.05);
}

const QImage& coverFor(const QString& path)
{
    static QHash<QString, QImage> cache;
    auto it = cache.find(path);
    if (it == cache.end()) {
        QImage img(path);
        // A vinheta nunca passa de ~110 px: guarda reduzida.
        if (!img.isNull() && (img.width() > 256 || img.height() > 256))
            img = img.scaled(256, 256, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        it = cache.insert(path, img);
    }
    return it.value();
}

} // namespace

void AmbienceArt::paint(QPainter& p, const QRectF& rect, const AmbienceManager::Track& track, double t)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipRect(rect, Qt::IntersectClip);
    if (!track.coverPath.isEmpty()) {
        const QImage& img = coverFor(track.coverPath);
        if (!img.isNull()) {
            // Recorte central quadrado (cover).
            const double s = std::min(img.width() / rect.width(), img.height() / rect.height());
            const QRectF src((img.width() - rect.width() * s) / 2, (img.height() - rect.height() * s) / 2,
                             rect.width() * s, rect.height() * s);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawImage(rect, img, src);
            p.restore();
            return;
        }
    }
    p.translate(rect.topLeft());
    Ctx c{p, rect.width(), rect.height(), t, Rng{fnv(track.id.toLower())}};
    switch (track.category) {
    case C::Nature: nature(c, track.name); break;
    case C::RainSea: rainSea(c, track.name); break;
    case C::Places: places(c, track.name); break;
    case C::Calm: calm(c, track.name); break;
    case C::Adventure: adventure(c, track.name); break;
    case C::Suspense: suspense(c, track.name); break;
    case C::Mine: mine(c); break;
    }
    p.restore();
}
