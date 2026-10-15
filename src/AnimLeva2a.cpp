// Temas Animados, leva 2 (parte 1): areia, poça, nuvens, trigo, fundo do mar,
// maré luminosa, lanternas, órbita e vitrola. Portado de leva2a.js / leva2b.js.

#include "AnimKit.h"

#include <QCoreApplication>
#include <QImage>
#include <QPolygonF>
#include <QLinearGradient>
#include <QRadialGradient>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace AnimBg {
namespace {

using namespace kit;

// bando de pássaros em V
struct Flock {
    double x = -40, y = 0, v = 50;
    std::vector<std::array<double, 3>> b;
    static Flock make(const Ctx& c)
    {
        Flock f;
        const int n = 3 + RI(5);
        f.y = R(c.H * 0.1, c.H * 0.3);
        f.v = R(50, 75);
        for (int i = 0; i < n; ++i) f.b.push_back({ -(i * R(14, 22)), (i % 2 ? 1.0 : -1.0) * std::ceil(i / 2.0) * R(8, 12), R(0, 6.28) });
        return f;
    }
    void paint(QPainter& p, double t, const QColor& col) const
    {
        p.setPen(pen(col, 1.4));
        p.setBrush(Qt::NoBrush);
        for (const auto& q : b) {
            const double x = this->x + q[0], y = this->y + q[1], w = std::sin(t * 7 + q[2]) * 3.5;
            QPainterPath path(QPointF(x - 6, y - w));
            path.quadTo(x - 3, y - 1, x, y);
            path.quadTo(x + 3, y - 1, x + 6, y - w);
            p.drawPath(path);
        }
    }
};

// ============================================================ tempestade de areia

class Sandstorm : public Layer {
public:
    void init(Ctx& c) override
    {
        m_g.assign(count(520, c), G());
        for (G& g : m_g) spawn(g, true, c);
        m_w.clear();
        for (auto& f : m_fs) f = profile(6, 0.42);
        m_fs[2] = profile(7, 0.48);
        m_dunes.reset();
        m_dust.reset();
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        const double gu = gust(m_t);
        m_off[0] += 70 * (0.4 + gu) * dt;
        m_off[1] += 150 * (0.4 + gu) * dt;
        for (G& g : m_g) {
            g.x += g.v * gu * dt;
            g.y += (g.v * gu * 0.05 + std::sin(m_t * 3 + g.x * 0.01) * 12) * dt;
            if (g.x > c.W + 40) spawn(g, false, c);
        }
        const double n = dt * 40 * gu * INT(c) * c.W / 1440;
        const int k = int(n) + (rnd() < n - int(n) ? 1 : 0);
        for (int i = 0; i < k; ++i) {
            const double u = rnd();
            m_w.push_back({ u * c.W, crest(c, u), 0, R(80, 200), R(-25, -60), R(1.0, 2.0) });
        }
        for (Wisp& w : m_w) { w.t += dt; w.x += w.vx * gu * dt; w.y += w.vy * dt; w.vy += 22 * dt; }
        m_w.erase(std::remove_if(m_w.begin(), m_w.end(), [](const Wisp& w) { return w.t >= w.l; }), m_w.end());
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double gu = gust(m_t);
        const QColor top = mix(pal.app, hex("#6e4526"), 0.5), bot = mix(pal.app, hex("#c48a4c"), 0.62), sand = mix(hex("#ecc890"), pal.app, 0.15);
        sky(p, c, top, bot);
        // sol fosco atrás da poeira
        const double sx = anchorX(c, 0.53), sy = 165, sa = 0.3 + 0.5 * (1 - gu);
        glow(p, sx, sy, 240, hex("#fff1d0"), sa * 0.45);
        disc(p, sx, sy, 36, alpha(hex("#fff4dc"), sa));
        if (!m_dunes.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing, true);
            const double base[3] = { c.H * 0.6, c.H * 0.72, c.H * 0.86 }, amp[3] = { 46, 56, 64 };
            const QColor cols[3] = { mix(bot, hex("#9a6436"), 0.35), mix(pal.app, hex("#8a5630"), 0.55), mix(pal.app, hex("#4a2e18"), 0.55) };
            for (int k = 0; k < 3; ++k) {
                QPixmap one = layer(c, c.W, c.H);
                QPainter y(&one);
                y.setRenderHint(QPainter::Antialiasing, true);
                fillProfile(y, c, m_fs[k], base[k], amp[k], cols[k], 6);
                // crista acesa pelo sol e o lado da sombra descendo dela
                QPainterPath ridge;
                for (double xx = 0; xx <= c.W; xx += 6) {
                    const QPointF q(xx, base[k] + m_fs[k].at(xx / c.W) * amp[k]);
                    if (xx == 0) ridge.moveTo(q); else ridge.lineTo(q);
                }
                y.setPen(pen(alpha(hex("#ffd9a0"), 0.22), 1.5));
                y.drawPath(ridge);
                y.setCompositionMode(QPainter::CompositionMode_SourceAtop);
                QLinearGradient g(0, base[k] - amp[k], 0, base[k] + amp[k] * 1.6);
                g.setColorAt(0, alpha(Qt::black, 0));
                g.setColorAt(1, alpha(Qt::black, 0.25));
                y.fillRect(QRectF(0, base[k] - amp[k], c.W, amp[k] * 2.6), g);
                y.end();
                x.drawPixmap(QRectF(0, 0, c.W, c.H), one, QRectF(QPointF(0, 0), QSizeF(one.size())));
            }
            x.end();
            m_dunes.set(c, pm);
        }
        blit(p, m_dunes.pm, 0, 0, c.W, c.H);
        // véu de poeira: quanto mais forte a rajada, menos se vê
        p.fillRect(QRectF(0, 0, c.W, c.H), alpha(mix(hex("#c99052"), pal.app, 0.25), 0.08 + 0.2 * gu));
        if (!m_dust.valid(c)) {
            QPixmap pm = layer(c, c.W, 460);
            QPainter x(&pm);
            const QPixmap dot = softDot(mix(hex("#dcae70"), pal.app, 0.15), 0.15);
            const int n = int(std::lround(80 * c.W / 1440));
            for (int i = 0; i < n; ++i) {
                const double w = R(220, 560), h = w * R(0.22, 0.42), cx = R(0, c.W), cy = R(110, 350);
                for (double ox : { 0.0, -c.W, c.W }) { x.setOpacity(R(0.25, 0.5)); blit(x, dot, cx + ox - w / 2, cy - h / 2, w, h); }
            }
            x.end();
            m_dust.set(c, pm);
        }
        const double ys[2] = { 30, 320 }, as[2] = { 0.22, 0.34 };
        for (int i = 0; i < 2; ++i) {
            const double off = std::fmod(m_off[i], c.W);
            p.setOpacity(as[i] * (0.5 + gu * 0.7));
            blit(p, m_dust.pm, off - c.W, ys[i], c.W, 460);
            blit(p, m_dust.pm, off, ys[i], c.W, 460);
        }
        p.setOpacity(1);
        // fiapos saindo da crista
        LineBatch lb;
        for (const Wisp& w : m_w) { const double k = w.t / w.l; lb.add(w.x, w.y, w.x - 14 - 20 * k, w.y + 3, 0.35 * (1 - k)); }
        lb.flush(p, sand, 1.2);
        // grãos voando, em três camadas de profundidade
        const double Z[3][4] = { { 0, 0.45, 0.14, 0.7 }, { 0.45, 0.75, 0.26, 1 }, { 0.75, 1.01, 0.42, 1.4 } };
        for (const auto& z : Z) {
            QVector<QLineF> ls;
            for (const G& g : m_g) if (g.z >= z[0] && g.z < z[1]) ls.append(QLineF(g.x, g.y, g.x - g.l * (0.5 + gu), g.y - g.l * 0.05));
            p.setPen(pen(alpha(sand, z[2] * (0.6 + 0.5 * gu)), z[3]));
            p.drawLines(ls);
        }
    }

private:
    struct G { double x = 0, y = 0, z = 0, v = 0, l = 0; };
    struct Wisp { double x, y, t, vx, vy, l; };
    static double gust(double t) { return 0.6 + 0.4 * std::sin(t * 0.29) * std::sin(t * 0.11 + 1.3); }
    double crest(const Ctx& c, double u) const { return c.H * 0.86 + m_fs[2].at(u) * 64; }
    void spawn(G& g, bool any, const Ctx& c) const
    {
        g.z = std::pow(rnd(), 1.5) * 0.85 + 0.15;
        g.x = any ? R(-60, c.W) : R(-200, -10);
        g.y = R(-20, c.H);
        g.v = 260 + 700 * g.z;
        g.l = 5 + 26 * g.z;
    }
    std::vector<G> m_g;
    std::vector<Wisp> m_w;
    Profile m_fs[3];
    Cache m_dunes, m_dust;
    double m_t = 0, m_off[2] = { 0, 0 };
};

// ============================================================ poça (chuva vista de cima)

class Ripples : public Layer {
public:
    void init(Ctx& c) override
    {
        m_rings.clear();
        m_ground.reset();
        m_leaves.clear();
        for (int i = 0; i < 2; ++i) m_leaves.push_back(newLeaf(c));
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        const double n = dt * 20 * INT(c) * c.W / 1440;
        const int k = int(n) + (rnd() < n - int(n) ? 1 : 0);
        for (int i = 0; i < k; ++i) m_rings.push_back({ R(0, c.W), R(0, c.H), 0, R(0.45, 1), R(1.2, 1.9) });
        for (Ring& r : m_rings) r.t += dt;
        m_rings.erase(std::remove_if(m_rings.begin(), m_rings.end(), [](const Ring& r) { return r.t >= r.d; }), m_rings.end());
        const double x0 = zx(c, 80), x1 = zx(c, 370);
        for (Leaf& l : m_leaves) {
            // a onda que passa empurra a folha
            for (const Ring& r : m_rings) {
                const double dx = l.x - r.x, dy = (l.y - r.y) / 0.62, d = std::hypot(dx, dy), Rr = 5 + 70 * (r.t / r.d) * r.z;
                if (d > 1 && std::abs(d - Rr) < 7) { l.vx += dx / d * 1.6; l.vy += dy / d * 1.0; l.vr += R(-0.05, 0.05); }
            }
            l.vx *= std::pow(0.6, dt); l.vy *= std::pow(0.6, dt); l.vr *= std::pow(0.7, dt);
            if (l.x < x0) l.vx += 6 * dt;
            if (l.x > x1) l.vx -= 6 * dt;
            if (l.y < 90) l.vy += 6 * dt;
            if (l.y > 580) l.vy -= 6 * dt;
            l.x += l.vx * dt; l.y += l.vy * dt; l.r += l.vr * dt;
        }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        if (!m_ground.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            vfill(x, QRectF(0, 0, c.W, c.H), mix(pal.app, Qt::black, 0.25), mix(pal.app, hex("#1a2430"), 0.2), 0, c.H);
            const int n = int(std::lround(5000 * c.W / 1440));
            for (int i = 0; i < n; ++i) x.fillRect(QRectF(R(0, c.W), R(0, c.H), R(1, 2.5), R(1, 2.5)), alpha(rnd() < 0.5 ? Qt::white : Qt::black, R(0.015, 0.05)));
            x.end();
            m_ground.set(c, pm);
        }
        blit(p, m_ground.pm, 0, 0, c.W, c.H);
        // reflexos: brilho borrado e uma pilha de tracinhos que tremem
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        const struct { double x, y; const char* col; double s; } LIGHTS[] = {
            { zx(c, 215), 200, "#ffae4a", 1 }, { rx(c, 1270), 150, "#ff4a5a", 0.75 }, { zx(c, 330), 500, "#4ad8ff", 0.6 },
            { rx(c, 1210), 640, "#ffd890", 0.6 }, { zx(c, 120), 420, "#ff8a3a", 0.45 } };
        for (const auto& L : LIGHTS) {
            const QColor col = hex(L.col);
            QPixmap& sp = m_refl[L.col];
            if (sp.isNull()) sp = softDot(col, 0.2);
            p.setOpacity(0.22 * L.s);
            blit(p, sp, L.x - 70, L.y - 170, 140, 340);
            const int N = 26;
            for (int j = -N; j <= N; ++j) {
                const double u = std::abs(j) / double(N), y = L.y + j * 6.5 + std::sin(m_t * 1.1 + j) * 1.2;
                const double w = std::pow(1 - u, 0.8) * 30 * (0.55 + 0.45 * std::sin(m_t * 2.3 + j * 1.7 + L.x)), off = std::sin(m_t * 1.6 + j * 0.7 + L.x) * 5 * (0.4 + u);
                if (w < 1.5) continue;
                p.setOpacity((0.85 - 0.7 * u) * L.s);
                p.fillRect(QRectF(L.x - w / 2 + off, y, w, 2.2), mix(col, Qt::white, 0.35 * (1 - u)));
            }
        }
        p.setOpacity(1);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // anéis: dois por gota, em perspectiva
        const QColor rc = mix(Qt::white, pal.app, 0.3);
        p.setBrush(Qt::NoBrush);
        for (const Ring& r : m_rings) {
            const double k = r.t / r.d, Rr = 5 + 70 * k * r.z, a = std::pow(1 - k, 1.6) * 0.7 * r.z;
            p.setPen(QPen(alpha(rc, a), 1.1));
            p.drawEllipse(QPointF(r.x, r.y), Rr, Rr * 0.62);
            if (Rr > 14) { p.setPen(QPen(alpha(rc, a * 0.6), 1.1)); p.drawEllipse(QPointF(r.x, r.y), Rr * 0.62, Rr * 0.62 * 0.62); }
            if (r.t < 0.1) disc(p, r.x, r.y, 1.6, alpha(rc, 0.6 * r.z));
        }
        // folhas boiando
        for (const Leaf& l : m_leaves) {
            p.save();
            p.translate(l.x, l.y);
            p.rotate(l.r * 180 / M_PI);
            p.setPen(Qt::NoPen);
            p.setBrush(alpha(Qt::black, 0.3));
            p.drawEllipse(QPointF(3, 4), l.s * 0.45, l.s);
            QPainterPath leaf(QPointF(0, -l.s));
            leaf.quadTo(l.s * 0.8, -l.s * 0.2, 0, l.s);
            leaf.quadTo(-l.s * 0.8, -l.s * 0.2, 0, -l.s);
            p.fillPath(leaf, mix(l.c, pal.app, 0.25));
            p.setPen(QPen(alpha(Qt::black, 0.3), 0.8));
            p.drawLine(QPointF(0, -l.s * 0.9), QPointF(0, l.s * 1.3));
            p.restore();
        }
    }

private:
    struct Ring { double x, y, t, z, d; };
    struct Leaf { double x, y, r, vr, vx, vy, s; QColor c; };
    Leaf newLeaf(const Ctx& c) const
    {
        static const char* C[] = { "#c8552a", "#d9862e", "#b8392b", "#c79a34" };
        return { R(zx(c, 90), zx(c, 360)), R(110, 560), R(0, 6.28), R(-0.08, 0.08), R(-3, 3), R(-3, 3), R(12, 16), hex(C[RI(4)]) };
    }
    std::vector<Ring> m_rings;
    std::vector<Leaf> m_leaves;
    std::map<const char*, QPixmap> m_refl;
    Cache m_ground;
    double m_t = 0;
};

// ============================================================ nuvens em timelapse

class Clouds : public Layer {
public:
    void init(Ctx& c) override
    {
        m_clouds.clear();
        for (int k = 0; k < 3; ++k) {
            const int n = int(std::lround(LAY[k][0] * qMin(1.4, INT(c)) * c.W / 1440));
            for (int i = 0; i < qMax(1, n); ++i) m_clouds.push_back(newCloud(k, true, c));
        }
        m_mode.clear();
        m_stars.reset();
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        const double sp = 0.6 + 0.4 * INT(c);
        for (Cl& cl : m_clouds) { cl.x += LAY[cl.k][2] * sp * dt; if (cl.x > c.W + 40) cl = newCloud(cl.k, false, c); }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const QString m = c.tod == QLatin1String("theme") ? (pal.dark ? QStringLiteral("night") : QStringLiteral("day")) : c.tod;
        const char* const* C = m == QLatin1String("day") ? DAY : m == QLatin1String("dusk") ? DUSK : NIGHT;
        if (m_mode != m) {
            m_mode = m;
            m_sprites.clear();
            for (int i = 0; i < 12; ++i) m_sprites.push_back(cloudSprite(mix(hex(C[0]), pal.app, 0.06), mix(hex(C[1]), pal.app, 0.12)));
        }
        sky(p, c, mix(hex(C[2]), pal.app, 0.15), mix(hex(C[3]), pal.app, 0.1));
        const double sx = anchorX(c, 0.53);
        if (m == QLatin1String("night")) {
            if (!m_stars.valid(c)) m_stars.set(c, starLayer(c, 220, c.H * 0.8, 0.55));
            blit(p, m_stars.pm, 0, 0, c.W, c.H);
            glow(p, sx, 120, 160, hex("#dfe8ff"), 0.35);
            disc(p, sx, 120, 22, hex("#eef2fb"));
        } else {
            const bool dusk = m == QLatin1String("dusk");
            const double sy = dusk ? c.H * 0.82 : 110, r = dusk ? 520 : 300;
            QRadialGradient g(QPointF(sx, sy), r);
            g.setColorAt(0, alpha(hex(dusk ? "#ffb070" : "#fffbe8"), dusk ? 0.55 : 0.7));
            g.setColorAt(1, alpha(hex(dusk ? "#ffb070" : "#fffbe8"), 0));
            p.fillRect(QRectF(0, 0, c.W, c.H), g);
        }
        for (const Cl& cl : m_clouds) {
            const QImage& s = m_sprites[cl.i % m_sprites.size()];
            const double sc = LAY[cl.k][1], br = 1 + 0.05 * std::sin(m_t * 0.25 + cl.ph);
            p.setOpacity(LAY[cl.k][5]);
            p.drawImage(QRectF(cl.x, cl.y + s.height() * sc * (1 - br), s.width() * sc, s.height() * sc * br), s);
        }
        p.setOpacity(1);
    }

private:
    struct Cl { int k; double x, y; int i; double ph; };
    static constexpr double LAY[3][6] = { { 8, 0.42, 7, 30, 270, 0.75 }, { 6, 0.72, 15, 110, 470, 0.9 }, { 4, 1.15, 28, 300, 690, 1 } };
    static constexpr const char* DAY[4] = { "#ffffff", "#a9b9cc", "#4f8fcf", "#cfe2f0" };
    static constexpr const char* DUSK[4] = { "#ffd6b8", "#76577c", "#33366e", "#ef9c70" };
    static constexpr const char* NIGHT[4] = { "#5c6884", "#161e30", "#060b16", "#1b2840" };
    Cl newCloud(int k, bool any, const Ctx& c) const
    {
        const double* L = LAY[k];
        return { k, any ? R(-500, c.W) : -520 * L[1], R(L[3], L[4]) - 150 * L[1], RI(12), R(0, 6.28) };
    }
    static QImage cloudSprite(const QColor& hi, const QColor& sh)
    {
        const int w = int(R(280, 480)), h = int(w * R(0.38, 0.5));
        QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter x(&img);
        x.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QPixmap dS = softDot(sh, 0.5), dH = softDot(hi, 0.5);
        struct B { double x, y, r; };
        std::vector<B> bl;
        for (int i = 0, n = w / 8; i < n; ++i) {
            const double u = rnd(), hump = std::sin(u * M_PI), r = (0.16 + 0.3 * hump * R(0.6, 1.1)) * h;
            bl.push_back({ w * (0.1 + 0.8 * u), h * 0.8 - r * R(0.2, 0.8) * (0.4 + hump * 0.9), r });
        }
        x.setOpacity(0.95);
        for (const B& b : bl) blit(x, dS, b.x - b.r, b.y - b.r * 0.75, b.r * 2, b.r * 2);
        x.setOpacity(0.9);
        for (const B& b : bl) blit(x, dH, b.x - b.r * 0.88, b.y - b.r * 1.12, b.r * 1.76, b.r * 1.76);
        x.setOpacity(1);
        // base mais reta, como nuvem de tempo bom
        x.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        QLinearGradient g(0, h * 0.76, 0, h);
        g.setColorAt(0, QColor(0, 0, 0, 0));
        g.setColorAt(1, QColor(0, 0, 0, 255));
        x.fillRect(QRectF(0, h * 0.76, w, h * 0.24), g);
        return img;
    }
    std::vector<Cl> m_clouds;
    std::vector<QImage> m_sprites;
    QString m_mode;
    Cache m_stars;
    double m_t = 0;
};
constexpr double Clouds::LAY[3][6];
constexpr const char* Clouds::DAY[4];
constexpr const char* Clouds::DUSK[4];
constexpr const char* Clouds::NIGHT[4];

// ============================================================ campo de trigo

class Prairie : public Layer {
public:
    void init(Ctx& c) override
    {
        m_rows.clear();
        const double FY = c.H * 0.6;
        for (int k = 0, Rr = 24; k < Rr; ++k) {
            const double d = k / double(Rr - 1), y = FY + (c.H + 30 - FY) * std::pow(d, 1.7), sp = 4 + 14 * d, h = 5 + 50 * std::pow(d, 1.3);
            Row row;
            row.d = d;
            for (double x = R(-sp, 0); x < c.W + sp; x += sp * R(0.7, 1.3)) row.st.push_back({ x, y + R(-2, 2) * (1 + d * 4), h * R(0.75, 1.2) });
            m_rows.push_back(row);
        }
        m_back.reset();
        m_far = profile(7, 0.5);
        m_near = profile(7, 0.5);
        m_hasBirds = false;
        m_nextBirds = 5;
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_blade += dt * (1.2 + 3 * wind(m_t, wx(c), c));
        m_nextBirds -= dt;
        if (!m_hasBirds && m_nextBirds <= 0) { m_birds = Flock::make(c); m_hasBirds = true; }
        if (m_hasBirds) { m_birds.x += m_birds.v * dt; if (m_birds.x > c.W + 160) { m_hasBirds = false; m_nextBirds = R(14, 28); } }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const QString m = c.tod == QLatin1String("theme") ? QStringLiteral("dusk") : c.tod;
        const char* const* PP = m == QLatin1String("day") ? DAY : m == QLatin1String("night") ? NIGHT : GOLD;
        QColor P[10];
        for (int i = 0; i < 10; ++i) P[i] = mix(hex(PP[i]), pal.app, 0.12);
        const double FY = c.H * 0.6, X = wx(c), WTOP = FY - 175;
        vfill(p, QRectF(0, 0, c.W, FY + 4), P[0], P[1], 0, FY);
        // sol (ou lua) baixo, atrás do cata-vento
        const bool day = m == QLatin1String("day"), night = m == QLatin1String("night");
        const double sunY = day ? 120 : night ? 130 : FY - 60, sunX = X - (day || night ? 120 : 110);
        glow(p, sunX, sunY, night ? 150 : 420, P[9], night ? 0.3 : 0.6);
        disc(p, sunX, sunY, night ? 18 : 34, alpha(P[9], 0.95));
        const QString key = m + QString::number(int(X));
        if (!m_back.valid(c, key)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing, true);
            fillProfile(x, c, m_far, FY - 40, 40, mix(P[2], pal.app, 0.1), 6);
            // mesa de pedra (butte) no fundo, à direita
            QPainterPath butte(QPointF(rx(c, 1080), FY));
            butte.lineTo(rx(c, 1120), FY - 120); butte.lineTo(rx(c, 1132), FY - 128); butte.lineTo(rx(c, 1250), FY - 126);
            butte.lineTo(rx(c, 1262), FY - 116); butte.lineTo(rx(c, 1310), FY); butte.closeSubpath();
            x.fillPath(butte, mix(P[2], P[3], 0.4));
            fillProfile(x, c, m_near, FY - 6, 16, mix(P[3], pal.app, 0.1), 6);
            // torre do cata-vento: treliça afinando
            const double base = FY + 46;
            auto half = [&](double y) { return 4 + 26 * (y - WTOP) / (base - WTOP); };
            x.setPen(pen(P[8], 3));
            x.drawLine(QPointF(X - half(base), base), QPointF(X - 4, WTOP));
            x.drawLine(QPointF(X + half(base), base), QPointF(X + 4, WTOP));
            x.setPen(pen(P[8], 1.3));
            for (double y = WTOP + 18; y < base; y += 24) {
                const double a = half(y), y2 = qMin(base, y + 24), b2 = half(y2);
                x.drawLine(QPointF(X - a, y), QPointF(X + a, y));
                x.drawLine(QPointF(X - a, y), QPointF(X + b2, y2));
                x.drawLine(QPointF(X + a, y), QPointF(X - b2, y2));
            }
            x.fillRect(QRectF(X - 6, WTOP - 6, 12, 10), P[8]);
            x.setPen(pen(P[8], 2));
            x.drawLine(QPointF(X, WTOP - 2), QPointF(X + 52, WTOP - 4));
            QPainterPath vane(QPointF(X + 40, WTOP - 4));
            vane.lineTo(X + 74, WTOP - 22); vane.lineTo(X + 80, WTOP + 10); vane.lineTo(X + 46, WTOP + 2); vane.closeSubpath();
            x.fillPath(vane, P[8]);
            x.end();
            m_back.set(c, pm, key);
        }
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // roda do cata-vento
        p.save();
        p.translate(X, WTOP);
        p.rotate(m_blade * 180 / M_PI);
        p.setPen(QPen(P[8], 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(0, 0), 36, 36);
        QPainterPath blades;
        for (int i = 0; i < 18; ++i) {
            const double a = i / 18.0 * 2 * M_PI, ca = std::cos(a), sa = std::sin(a);
            auto pt = [&](double u, double v) { return QPointF(u * ca - v * sa, u * sa + v * ca); };
            blades.moveTo(pt(6, -1.5)); blades.lineTo(pt(40, -4.5)); blades.lineTo(pt(40, 3)); blades.lineTo(pt(6, 1.5)); blades.closeSubpath();
        }
        blades.addEllipse(QPointF(0, 0), 6, 6);
        p.fillPath(blades, P[8]);
        p.restore();
        // chão do campo
        vfill(p, QRectF(0, FY - 2, c.W, c.H - FY + 2), P[4], P[5], FY, c.H);
        // espigas: 4 profundidades × 3 forças de vento, um lote de traços por grupo
        QVector<QLineF> bk[12];
        for (const Row& r : m_rows) {
            const int dg = qMin(3, int(r.d * 4));
            for (const auto& s : r.st) {
                const double w = wind(m_t, s[0], c), bend = w * s[2] * 0.5;
                const int gb = w < 0.45 ? 0 : w < 0.9 ? 1 : 2;
                bk[dg * 3 + gb].append(QLineF(s[0], s[1], s[0] + bend, s[1] - s[2] + bend * 0.25));
            }
        }
        static const double LW[4] = { 0.8, 1.1, 1.6, 2.2 };
        p.save();
        p.setRenderHint(QPainter::Antialiasing, false);   // milhares de traços finos: sem antialias fica 5x mais barato
        for (int dg = 0; dg < 4; ++dg)
            for (int gb = 0; gb < 3; ++gb) {
                if (bk[dg * 3 + gb].isEmpty()) continue;
                QPen pn(mix(mix(P[6], P[7], gb / 2.0), P[4], (3 - dg) / 3.0 * 0.45), LW[dg]);
                pn.setCapStyle(Qt::FlatCap);
                p.setPen(pn);
                p.drawLines(bk[dg * 3 + gb]);
            }
        p.restore();
        if (m_hasBirds) m_birds.paint(p, m_t, alpha(P[8], 0.8));
    }

private:
    struct Row { double d; std::vector<std::array<double, 3>> st; };
    static constexpr const char* GOLD[10] = { "#3a2b45", "#f0a25e", "#8a5a66", "#5c3c44", "#b07a34", "#5a3a16", "#e2ae58", "#ffdc92", "#1c1216", "#ffe6a8" };
    static constexpr const char* DAY[10] = { "#5f9bd2", "#dce9ef", "#93a7b8", "#71889a", "#c39f4a", "#76571f", "#dcbc66", "#f8e4a6", "#3a332c", "#ffffff" };
    static constexpr const char* NIGHT[10] = { "#070b16", "#1c2a44", "#1a2234", "#121826", "#283040", "#0f141c", "#76829a", "#b4c0d6", "#06080d", "#e8eef8" };
    static double wx(const Ctx& c) { return zx(c, 330); }
    static double wind(double t, double x, const Ctx& c)
    {
        const double a = INT(c);
        return a * (0.25 + 0.75 * std::pow(0.5 + 0.5 * std::sin(t * 0.8 - x * 0.0055), 3)) + 0.1 * a * std::sin(t * 2.6 + x * 0.05);
    }
    std::vector<Row> m_rows;
    Profile m_far, m_near;
    Cache m_back;
    Flock m_birds;
    bool m_hasBirds = false;
    double m_t = 0, m_blade = 0, m_nextBirds = 5;
};
constexpr const char* Prairie::GOLD[10];
constexpr const char* Prairie::DAY[10];
constexpr const char* Prairie::NIGHT[10];

// ============================================================ fundo do mar

class DeepSea : public Layer {
public:
    void init(Ctx& c) override
    {
        m_jelly.clear();
        const int n = qMax(3, int(std::lround(6 * INT(c) * c.W / 1440)));
        for (int i = 0; i < n; ++i) m_jelly.push_back(newJelly(true, c));
        m_snow.clear();
        for (int i = 0, k = count(220, c); i < k; ++i) m_snow.push_back({ R(0, c.W), R(0, c.H), R(0.3, 1), R(0, 6.28) });
        m_bub.clear();
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        for (J& j : m_jelly) {
            const double pulse = std::pow(qMax(0.0, std::sin(m_t * j.f * 6.28 + j.ph)), 2);
            j.y += (7 - 30 * pulse) * dt * (j.s / 30);
            j.x += (j.dx + std::sin(m_t * 0.2 + j.ph) * 5) * dt * 0.5;
            if (j.y < -j.s * 4) j = newJelly(false, c);
        }
        for (auto& s : m_snow) { s[1] += (4 + 8 * s[2]) * dt; s[0] += std::sin(m_t * 0.3 + s[3]) * 3 * dt; if (s[1] > c.H) { s[1] = -4; s[0] = R(0, c.W); } }
        const double vents[2][2] = { { anchorX(c, 0.53) + 80, 0.9 }, { rx(c, 1300), 0.5 } };
        for (const auto& v : vents) if (rnd() < dt * 3 * v[1] * INT(c)) m_bub.push_back({ v[0] + R(-8, 8), c.H + 5, R(1.5, 4.5), R(30, 50), R(0, 6.28) });
        for (auto& b : m_bub) { b[3] += 16 * dt; b[1] -= b[3] * dt; b[0] += std::sin(m_t * 4 + b[4]) * 14 * dt; }
        m_bub.erase(std::remove_if(m_bub.begin(), m_bub.end(), [](const std::array<double, 5>& b) { return b[1] < -10; }), m_bub.end());
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        QLinearGradient g(0, 0, 0, c.H);
        g.setColorAt(0, mix(pal.app, hex("#0f5566"), 0.5));
        g.setColorAt(0.55, mix(pal.app, hex("#06222c"), 0.4));
        g.setColorAt(1, mix(pal.app, Qt::black, 0.5));
        p.fillRect(QRectF(0, 0, c.W, c.H), g);
        // feixes de luz vindos da superfície
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (int i = 0; i * 240 < c.W + 240; ++i) {
            const double x0 = 80 + i * 240 + std::sin(m_t * 0.11 + i * 1.7) * 40, w = 30 + 50 * (0.5 + 0.5 * std::sin(i * 2.3)), a = 0.05 + 0.04 * std::sin(m_t * 0.3 + i * 1.1);
            QLinearGradient lg(0, 0, 0, c.H * 0.85);
            lg.setColorAt(0, alpha(hex("#bff6ff"), a));
            lg.setColorAt(1, alpha(hex("#bff6ff"), 0));
            QPainterPath ray(QPointF(x0, 0));
            ray.lineTo(x0 + w, 0); ray.lineTo(x0 + w * 2.6 + 160, c.H * 0.85); ray.lineTo(x0 + 160, c.H * 0.85); ray.closeSubpath();
            p.fillPath(ray, lg);
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // neve marinha
        for (const auto& s : m_snow) p.fillRect(QRectF(s[0], s[1], 0.8 + s[2] * 1.2, 0.8 + s[2] * 1.2), alpha(hex("#d8f2ff"), 0.35 * (0.25 + 0.55 * s[2])));
        // bolhas
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(alpha(hex("#d8f6ff"), 0.5), 1));
        for (const auto& b : m_bub) p.drawEllipse(QPointF(b[0], b[1]), b[2], b[2]);
        // águas-vivas
        for (const J& j : m_jelly) {
            const double k = std::pow(qMax(0.0, std::sin(m_t * j.f * 6.28 + j.ph)), 2), bw = j.s * (1 - 0.2 * k), bh = j.s * 0.78 * (1 + 0.14 * k), x = j.x, y = j.y;
            QPixmap& gl = m_glow[j.c.rgb()];
            if (gl.isNull()) gl = softDot(j.c, 0.2);
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            p.setOpacity(0.28);
            blit(p, gl, x - j.s * 3, y - j.s * 2.4, j.s * 6, j.s * 6);
            p.setOpacity(1);
            // tentáculos
            p.setBrush(Qt::NoBrush);
            p.setPen(pen(alpha(j.c, 0.32), 1));
            for (int i = 0; i < 7; ++i) {
                const double bx = x + (i / 6.0 - 0.5) * bw * 1.7, L = j.s * (2.4 + (i % 3) * 0.5);
                QPainterPath t(QPointF(bx, y + 2));
                for (int s = 1; s <= 10; ++s) { const double u = s / 10.0; t.lineTo(bx + std::sin(m_t * 1.7 - u * 4 + i) * u * j.s * 0.35 + (k * -0.2) * u * (bx - x), y + 2 + L * u * (1 - 0.15 * k)); }
                p.drawPath(t);
            }
            p.setPen(pen(alpha(mix(j.c, Qt::white, 0.3), 0.4), 2.6));
            for (int i = -1; i <= 1; i += 2) {
                QPainterPath t(QPointF(x + i * 3, y));
                for (int s = 1; s <= 8; ++s) { const double u = s / 8.0; t.lineTo(x + i * 3 + std::sin(m_t * 1.2 - u * 3 + i) * u * j.s * 0.25, y + j.s * 1.5 * u); }
                p.drawPath(t);
            }
            // sino
            QLinearGradient bg(0, y - bh, 0, y + 3);
            bg.setColorAt(0, alpha(mix(j.c, Qt::white, 0.35), 0.6));
            bg.setColorAt(1, alpha(j.c, 0.18));
            QPainterPath bell(QPointF(x - bw, y));
            bell.cubicTo(x - bw, y - bh * 1.25, x + bw, y - bh * 1.25, x + bw, y);
            for (int i = 4; i >= 0; --i) bell.quadTo(x + bw * ((i + 0.5) / 2.5 - 1), y + 4, x + bw * (i / 2.5 - 1), y);
            p.fillPath(bell, bg);
            QPainterPath rim(QPointF(x - bw, y));
            rim.cubicTo(x - bw, y - bh * 1.25, x + bw, y - bh * 1.25, x + bw, y);
            p.setPen(pen(alpha(mix(j.c, Qt::white, 0.5), 0.55), 1.2));
            p.drawPath(rim);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
    }

private:
    struct J { double x, y, s, ph, f; QColor c; double dx; };
    J newJelly(bool any, const Ctx& c) const
    {
        static const char* C[] = { "#ff7ad9", "#9f8cff", "#5ee6ff", "#ffb36b" };
        const QPointF v = visPoint(c);
        return { v.x(), any ? R(40, c.H - 40) : c.H + 80, R(18, 42), R(0, 6.28), R(0.45, 0.8), hex(C[RI(4)]), R(-6, 6) };
    }
    std::vector<J> m_jelly;
    std::vector<std::array<double, 4>> m_snow;
    std::vector<std::array<double, 5>> m_bub;
    std::map<QRgb, QPixmap> m_glow;
    double m_t = 0;
};

// ============================================================ maré luminosa

class Glowtide : public Layer {
public:
    void init(Ctx&) override
    {
        m_waves.clear();
        for (double z = 0.06; z < 1; z += 0.13) m_waves.push_back(newWave(z));
        m_washes.clear();
        m_washes.push_back({ 1, R(0, 6.28), R(0, 6.28), 0.01 });
        m_sky.reset();
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        for (W& w : m_waves) {
            w.z += w.sp * (0.4 + w.z) * dt * (0.7 + 0.3 * INT(c));
            if (w.z >= 1) { m_washes.push_back({ 0, w.ph, w.ph2, w.k }); w = newWave(0); }
        }
        for (Wash& s : m_washes) s.t += dt;
        m_washes.erase(std::remove_if(m_washes.begin(), m_washes.end(), [](const Wash& s) { return s.t >= 4; }), m_washes.end());
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double HZ = c.H * 0.44, SH = c.H * 0.8;
        const QColor GL = hex("#3ef0ff");
        if (!m_sky.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing, true);
            vfill(x, QRectF(0, 0, c.W, HZ), mix(pal.app, Qt::black, 0.4), mix(pal.app, hex("#0f2236"), 0.35), 0, HZ);
            const QPixmap dot = softDot(hex("#b8c4ff"), 0.2);
            x.setOpacity(0.05);
            for (int i = 0; i < 120; ++i) { const double k = rnd(), bx = lerp(-80, c.W * 0.7, k), by = lerp(HZ, -80, k) + R(-60, 60), s = R(60, 150); blit(x, dot, bx - s / 2, by - s / 2, s, s); }
            x.setOpacity(1);
            x.drawPixmap(0, 0, starLayer(c, 380, HZ, 0.7));
            vfill(x, QRectF(0, HZ, c.W, SH - HZ + 4), mix(pal.app, hex("#0c2232"), 0.45), mix(pal.app, hex("#040a10"), 0.4), HZ, SH);
            vfill(x, QRectF(0, SH, c.W, c.H - SH), mix(pal.app, hex("#1a1c22"), 0.35), mix(pal.app, hex("#0a0b0e"), 0.4), SH, c.H);
            // ponta de terra no horizonte
            const double o = zx(c, 45) - 45;
            QPainterPath land(QPointF(-10 + o, HZ + 2));
            land.quadTo(120 + o, HZ - 34, 260 + o, HZ - 6);
            land.quadTo(330 + o, HZ - 2, 380 + o, HZ + 2);
            land.closeSubpath();
            x.fillPath(land, mix(pal.app, Qt::black, 0.5));
            x.end();
            m_sky.set(c, pm);
        }
        blit(p, m_sky.pm, 0, 0, c.W, c.H);
        // coqueiros balançando
        const QColor dk = mix(pal.app, Qt::black, 0.5);
        const double o = zx(c, 45) - 45;
        for (const auto& pt : { std::array<double, 3>{ 70, 70, 0.25 }, std::array<double, 3>{ 118, 92, -0.12 }, std::array<double, 3>{ 176, 60, 0.35 } }) {
            const double px = pt[0] + o, by = HZ - 18 - (pt[0] - 60) * 0.05, tx = px + pt[2] * pt[1], ty = by - pt[1];
            p.setBrush(Qt::NoBrush);
            p.setPen(pen(dk, 3));
            QPainterPath trunk(QPointF(px, by));
            trunk.quadTo(px + pt[2] * pt[1] * 0.2, by - pt[1] * 0.6, tx, ty);
            p.drawPath(trunk);
            p.setPen(pen(dk, 2));
            for (int i = 0; i < 7; ++i) {
                const double a = -M_PI / 2 + (i / 6.0 - 0.5) * 3.4 + std::sin(m_t * 0.8 + i + px) * 0.05, l = 26 + (i % 3) * 5;
                QPainterPath f(QPointF(tx, ty));
                f.quadTo(tx + std::cos(a) * l * 0.6, ty + std::sin(a) * l * 0.6 - 6, tx + std::cos(a) * l, ty + std::sin(a) * l + 10);
                p.drawPath(f);
            }
        }
        // ondas: a crista acende quando chega perto da praia
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        LineBatch dim, b1, b2;
        for (const W& w : m_waves) {
            const double y = HZ + (SH - HZ) * std::pow(w.z, 1.7), amp = 1.5 + 12 * w.z, g = smooth(0.4, 0.95, w.z);
            auto crestY = [&](double x) { return y + std::sin(x * w.k + w.ph) * amp * 0.5 + std::sin(x * w.k * 2.7 + w.ph2) * amp * 0.25; };
            const double dimA = 0.06 + 0.1 * w.z;
            if (g < 0.05) { for (double x = 0; x < c.W; x += 24) dim.add(x, crestY(x), x + 24, crestY(x + 24), dimA); continue; }
            const double fh = 34 * w.z;
            QPainterPath foam;
            for (double x = 0; x < c.W; x += 24) {
                const double br = g * qMax(0.0, 0.3 + 0.7 * std::sin(x * 0.011 + w.ph * 3) * std::sin(x * 0.0041 + w.ph2 + m_t * 0.2)), y0 = crestY(x), y1 = crestY(x + 24);
                if (br < 0.04) { dim.add(x, y0, x + 24, y1, dimA); continue; }
                QPolygonF q;
                q << QPointF(x, y0) << QPointF(x + 24, y1) << QPointF(x + 24, y1 + fh) << QPointF(x, y0 + fh);
                foam.addPolygon(q);
                b1.add(x, y0, x + 24, y1, 0.32 * br);
                b2.add(x, y0, x + 24, y1, br);
            }
            QLinearGradient fg(0, y - amp, 0, y + fh);
            fg.setColorAt(0, alpha(GL, 0.2 * g));
            fg.setColorAt(1, alpha(GL, 0));
            p.setRenderHint(QPainter::Antialiasing, false);   // espuma e halo são borrados: sem antialias
            p.fillPath(foam, fg);
            b1.flush(p, GL, 6 * w.z + 1);
            p.setRenderHint(QPainter::Antialiasing, true);
            b2.flush(p, hex("#c8fbff"), 2);
        }
        dim.flush(p, hex("#9fb8d0"), 1);
        // espuma acesa subindo na areia e voltando
        for (const Wash& s : m_washes) {
            const double k = s.t / 4, reach = std::sin(qMin(1.0, k * 1.8) * M_PI / 2) * 46 * (1 - 0.4 * k), a = std::pow(1 - k, 1.5);
            for (double x = 0; x < c.W; x += 16) {
                const double br = qMax(0.0, 0.3 + 0.7 * std::sin(x * 0.011 + s.ph * 3) * std::sin(x * 0.0041 + s.ph2)), y = SH + reach + std::sin(x * s.k * 2 + s.ph) * 6;
                if (br * a < 0.03) continue;
                p.fillRect(QRectF(x, SH - 4, 16, y - SH + 4), alpha(GL, 0.26 * br * a));
                p.fillRect(QRectF(x, y - 1.5, 16, 3), alpha(hex("#bffaff"), 0.8 * br * a));
            }
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    struct W { double z, ph, ph2, k, sp; };
    struct Wash { double t, ph, ph2, k; };
    static W newWave(double z) { return { z, R(0, 6.28), R(0, 6.28), R(0.006, 0.012), R(0.045, 0.06) }; }
    std::vector<W> m_waves;
    std::vector<Wash> m_washes;
    Cache m_sky;
    double m_t = 0;
};

// ============================================================ lanternas no céu

class Lanterns : public Layer {
public:
    void init(Ctx& c) override
    {
        m_l.assign(count(46, c), L());
        for (L& l : m_l) spawn(l, true, c);
        m_back.reset();
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        for (L& l : m_l) { l.y -= l.v * dt; l.x += (5 + std::sin(m_t * 0.4 + l.ph) * 6) * l.z * dt; if (l.y < -30) spawn(l, false, c); }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double HZ = c.H * 0.7;
        if (!m_back.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing, true);
            QLinearGradient g(0, 0, 0, HZ);
            g.setColorAt(0, mix(pal.app, hex("#0a0f2a"), 0.45));
            g.setColorAt(0.7, mix(pal.app, hex("#3a2450"), 0.4));
            g.setColorAt(1, mix(pal.app, hex("#b0603a"), 0.45));
            x.fillRect(QRectF(0, 0, c.W, HZ), g);
            x.drawPixmap(0, 0, starLayer(c, 160, HZ * 0.6, 0.45));
            fillProfile(x, c, profile(7, 0.5), HZ - 34, 40, mix(pal.app, hex("#2a1c34"), 0.4), 6);
            fillProfile(x, c, profile(7, 0.55), HZ - 8, 18, mix(pal.app, Qt::black, 0.4), 6);
            x.fillRect(QRectF(0, HZ, c.W, c.H - HZ), mix(pal.app, Qt::black, 0.4));
            vfill(x, QRectF(0, HZ + 6, c.W, c.H - HZ), mix(pal.app, hex("#4a2c3a"), 0.35), mix(pal.app, hex("#06070c"), 0.45), HZ, c.H);
            x.end();
            m_back.set(c, pm);
            m_glow = softDot(hex("#ffa548"), 0.25);
        }
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // brilho na água
        LineBatch wb;
        for (int i = 0; i < 26; ++i) { const double y = HZ + 12 + i * 6.5 + std::sin(m_t * 0.6 + i) * 1.5, x0 = std::fmod(i * 377 + m_t * 6, c.W); wb.add(x0, y, x0 + 40 + i * 4, y, 0.05); }
        wb.flush(p, Qt::white, 1);
        std::vector<const L*> sorted;
        for (const L& l : m_l) sorted.push_back(&l);
        std::sort(sorted.begin(), sorted.end(), [](const L* a, const L* b) { return a->z < b->z; });
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const L* l : sorted) {
            const double ry = 2 * HZ - l->y + 10;
            if (ry > c.H || ry < HZ + 6) continue;
            const double f = 0.8 + 0.2 * std::sin(m_t * 7 + l->fl), s = l->s * 3;
            p.setOpacity(0.25 * f * l->z);
            blit(p, m_glow, l->x - s / 2 + std::sin(m_t * 2 + ry * 0.1) * 3, ry - s, s, s * 2);
        }
        for (const L* l : sorted) {
            const double f = 0.82 + 0.18 * std::sin(m_t * 6 + l->fl) * std::sin(m_t * 2.3 + l->ph), x = l->x + std::sin(m_t * 0.8 + l->ph) * 2 * l->z, y = l->y, s = l->s;
            p.setOpacity(0.35 * f);
            blit(p, m_glow, x - s * 3, y - s * 3, s * 6, s * 6);
            p.setOpacity(1);
            QLinearGradient bg(0, y - s * 0.7, 0, y + s * 0.7);
            bg.setColorAt(0, alpha(hex("#ff8a30"), 0.85 * f));
            bg.setColorAt(1, alpha(hex("#ffe7a8"), f));
            QPainterPath body(QPointF(x - s * 0.5, y - s * 0.7));
            body.quadTo(x, y - s * 0.85, x + s * 0.5, y - s * 0.7);
            body.lineTo(x + s * 0.38, y + s * 0.65);
            body.lineTo(x - s * 0.38, y + s * 0.65);
            body.closeSubpath();
            p.fillPath(body, bg);
        }
        p.setOpacity(1);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    struct L { double x = 0, y = 0, z = 0, y0 = 0, v = 0, ph = 0, s = 0, fl = 0; };
    void spawn(L& l, bool any, const Ctx& c) const
    {
        l.z = std::pow(rnd(), 0.8) * 0.8 + 0.2;
        l.x = rnd() < 0.72 ? R(zx(c, 40), zx(c, 470)) : R(rx(c, 980), rx(c, 1420));
        l.y0 = c.H * 0.7 - 4 + 26 * l.z;
        l.y = any ? R(-40, l.y0) : l.y0;
        l.v = 7 + 16 * l.z;
        l.ph = R(0, 6.28);
        l.s = 3 + 12 * l.z;
        l.fl = R(0, 6.28);
    }
    std::vector<L> m_l;
    Cache m_back;
    QPixmap m_glow;
    double m_t = 0;
};

// ============================================================ órbita baixa

class Orbit : public Layer {
public:
    void init(Ctx& c) override
    {
        geom(c);
        m_cities.clear();
        m_clouds.clear();
        const int nc = int(std::lround(120 * c.W / 1440));
        for (int i = 0; i < nc; ++i) {
            const double a = R(A0, A1), b = R(m_bMin, m_bMax);
            const int n = int(R(8, 70));
            const bool big = n > 50;
            for (int k = 0; k < n; ++k) m_cities.push_back({ a + R(-1, 1) * R(0, 0.012), b + R(-1, 1) * R(0, 0.016) * (big ? 1.4 : 1), R(0.5, 1) * (big ? 1.3 : 1), double(rnd() < 0.15) });
        }
        for (int i = 0; i < 26; ++i) m_clouds.push_back({ R(A0, A1), R(m_bMin, m_bMax), R(0.02, 0.05), R(0.4, 1) });
        m_flashes.clear();
        m_stars.reset();
        m_hasSat = false;
    }
    void resized(Ctx& c) override { init(c); }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        const double d = OM * dt * (0.6 + 0.4 * INT(c));
        auto wrap = [](double a) { const double span = A1 - A0; double v = std::fmod(a - A0, span); if (v < 0) v += span; return A0 + v; };
        for (auto& q : m_cities) q[0] = wrap(q[0] + d);
        for (auto& q : m_clouds) q[0] = wrap(q[0] + d);
        if (rnd() < dt * 0.5 * INT(c)) { const int nseq = 1 + RI(3); m_flashes.push_back({ double(RI(int(m_clouds.size()))), 0, R(-0.008, 0.008), double(nseq) }); }
        for (auto& f : m_flashes) f[1] += dt;
        m_flashes.erase(std::remove_if(m_flashes.begin(), m_flashes.end(), [](const std::array<double, 4>& f) { return f[1] >= 0.6; }), m_flashes.end());
        m_nextSat -= dt;
        if (!m_hasSat && m_nextSat <= 0) { m_sat = QPointF(-10, R(30, 200)); m_satV = R(30, 45); m_hasSat = true; }
        if (m_hasSat) { m_sat += QPointF(m_satV * dt, 3 * dt); if (m_sat.x() > c.W + 10) { m_hasSat = false; m_nextSat = R(20, 40); } }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        sky(p, c, mix(pal.app, Qt::black, 0.5), mix(pal.app, hex("#04060c"), 0.4));
        if (!m_stars.valid(c)) m_stars.set(c, starLayer(c, 420, c.H, 0.7));
        blit(p, m_stars.pm, 0, 0, c.W, c.H);
        if (m_hasSat) p.fillRect(QRectF(m_sat.x(), m_sat.y(), 1.6, 1.6), alpha(Qt::white, 0.8));
        if (!m_dot) { m_cdot = softDot(hex("#3a4a66"), 0.3); m_ldot = softDot(hex("#d6e6ff"), 0.3); m_dot = true; }
        const QRectF orb(m_cx - m_R, m_cy - m_R, m_R * 2, m_R * 2);
        auto arc = [&](double rr, const QColor& col, double w) {
            p.setPen(QPen(col, w));
            p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(m_cx - rr, m_cy - rr, rr * 2, rr * 2), 0, 180 * 16);
        };
        // atmosfera: brilho verde (airglow) e a linha azul fina
        arc(m_R + 6, alpha(hex("#3a7bd5"), 0.12), 16);
        arc(m_R + 15, alpha(hex("#7dff9a"), 0.16), 3);
        QRadialGradient eg(QPointF(m_cx, m_cy), m_R);
        eg.setColorAt(0.9, mix(pal.app, hex("#02040a"), 0.6));
        eg.setColorAt(0.97, mix(pal.app, hex("#0c1626"), 0.5));
        eg.setColorAt(1, mix(pal.app, hex("#2a4a7a"), 0.4));
        p.setPen(Qt::NoPen);
        p.setBrush(eg);
        p.drawEllipse(orb);
        arc(m_R + 1, alpha(hex("#7ab8ff"), 0.6), 2.2);
        // amanhecer chegando pela direita
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        const QPointF dawn = proj(M_PI / 2, m_bMax * 0.97);
        QRadialGradient dg(dawn, 380);
        dg.setColorAt(0, alpha(hex("#ff9a4a"), 0.35));
        dg.setColorAt(0.4, alpha(hex("#4a7ad0"), 0.12));
        dg.setColorAt(1, alpha(hex("#4a7ad0"), 0));
        p.fillRect(QRectF(dawn.x() - 380, dawn.y() - 380, 760, 760), dg);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // nuvens escuras, achatadas perto do horizonte
        for (const auto& q : m_clouds) {
            double z;
            const QPointF s = proj(q[0], q[1], &z);
            if (z < 0.01 || s.y() > c.H + 40) continue;
            const double w = m_R * q[2], h = w * z * 1.4;
            p.setOpacity(q[3] * 0.55);
            blit(p, m_cdot, s.x() - w / 2, s.y() - h / 2, w, h);
        }
        p.setOpacity(1);
        // luzes das cidades, em lotes por cor e brilho
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        QVector<QRectF> bk[2][6];
        for (const auto& q : m_cities) {
            double z;
            const QPointF s = proj(q[0], q[1], &z);
            if (z < 0.004 || s.y() > c.H + 4 || s.x() < -4 || s.x() > c.W + 4) continue;
            const double k = qMin(1.0, z * 6), a = 0.85 * q[2] * k, sz = 1.1 + q[2];
            bk[int(q[3])][qBound(0, int(a * 6), 5)].append(QRectF(s.x(), s.y(), sz, sz * qMax(0.35, k)));
        }
        p.setPen(Qt::NoPen);
        for (int col = 0; col < 2; ++col)
            for (int i = 0; i < 6; ++i) {
                if (bk[col][i].isEmpty()) continue;
                p.setBrush(alpha(hex(col ? "#fff2d0" : "#ffbf6a"), (i + 0.5) / 6));
                p.drawRects(bk[col][i]);
            }
        // relâmpagos dentro das nuvens
        for (const auto& f : m_flashes) {
            const auto& cl = m_clouds[size_t(f[0]) % m_clouds.size()];
            double z;
            const QPointF s = proj(cl[0], cl[1] + f[2], &z);
            if (z < 0.01) continue;
            double a = 0;
            const double seq[3] = { 0, 0.12, 0.3 };
            for (int i = 0; i < int(f[3]); ++i) { const double k = (f[1] - seq[i]) / 0.06; if (k > 0 && k < 2) a = qMax(a, std::exp(-k * 1.8)); }
            if (a > 0.02) { const double sz = 80 * qMax(0.3, z * 3); p.setOpacity(a * 0.8); blit(p, m_ldot, s.x() - sz / 2, s.y() - sz * z * 0.6, sz, sz * qMax(0.3, z * 1.2)); }
        }
        p.setOpacity(1);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    static constexpr double OM = 0.012, A0 = 0.6, A1 = M_PI / 2 + 0.03;
    void geom(const Ctx& c)
    {
        m_cx = zx(c, 518);
        m_cy = c.H + 1120;
        m_R = 1500;
        m_bMin = std::asin(qBound(-1.0, (0 - m_cx) / m_R, 1.0));
        m_bMax = std::asin(qBound(-1.0, (c.W - m_cx) / m_R, 1.0));
    }
    QPointF proj(double a, double b, double* z = nullptr) const
    {
        const double X = std::sin(b), Y = std::cos(b) * std::sin(a), Z = std::cos(b) * std::cos(a);
        if (z) *z = Z;
        return QPointF(m_cx + m_R * X, m_cy - m_R * Y);
    }
    std::vector<std::array<double, 4>> m_cities, m_clouds, m_flashes;
    Cache m_stars;
    QPixmap m_cdot, m_ldot;
    bool m_dot = false;
    double m_t = 0, m_cx = 0, m_cy = 0, m_R = 1500, m_bMin = 0, m_bMax = 0;
    bool m_hasSat = false;
    QPointF m_sat;
    double m_satV = 0, m_nextSat = 5;
};

// ============================================================ toca-discos

// A capa de "P.H.L - The Spring Is Coming" (foto do Tommy deitado no violão do Pe) mora ao lado
// da música, na pasta dos sons; o painel de som só lê áudio, então ela não vira faixa.
const QImage& phlCover()
{
    static const QImage img = [] {
        const QString name = QStringLiteral("/ambience-sounds/P.H.L - The Spring Is Coming.jpg");
        QImage im(QCoreApplication::applicationDirPath() + name);
#ifdef DEV_ASSETS_DIR
        if (im.isNull()) im = QImage(QString::fromUtf8(DEV_ASSETS_DIR) + name);
#endif
        return im.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }();
    return img;
}

class Turntable : public Layer {
public:
    void init(Ctx& c) override
    {
        m_desk.reset();
        m_rec = QImage();
        m_motes.clear();
        for (int i = 0; i < 50; ++i) m_motes.push_back({ R(zx(c, 40), zx(c, 420)), R(40, 640), R(0, 6.28), R(0.4, 1) });
    }
    void step(double dt, Ctx&) override
    {
        m_t += dt;
        // o braço anda devagar pro centro; no fim levanta e volta
        if (m_lift > 0) { m_lift -= dt; m_arm = qMax(0.0, m_arm - dt * 0.5); }
        else { m_arm += dt / 150; if (m_arm >= 1) m_lift = 2.5; }
        for (auto& m : m_motes) { m[0] += std::sin(m_t * 0.2 + m[2]) * 3 * dt; m[1] += (std::cos(m_t * 0.17 + m[2]) * 2 + 1.5) * dt; if (m[1] > 660) m[1] = 40; }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double cx = zx(c, 215), cy = 345;
        if (!m_desk.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing, true);
            x.fillRect(QRectF(0, 0, c.W, c.H), mix(pal.app, Qt::black, 0.1));
            // veio da madeira da mesa
            for (int i = 0; i < 220; ++i) {
                const double y = R(0, c.H);
                x.setPen(QPen(alpha(rnd() < 0.5 ? Qt::black : Qt::white, R(0.02, 0.05)), R(0.6, 2.2)));
                QPainterPath g(QPointF(0, y));
                for (double xx = 60; xx <= c.W + 60; xx += 60) g.lineTo(xx, y + std::sin(xx * 0.004 + i) * 6);
                x.drawPath(g);
            }
            // a vitrola: base de madeira e tampo
            const double bx = cx - 175, by = cy - 165, bw = 360, bh = 320;
            x.fillRect(QRectF(bx + 8, by + 12, bw, bh), alpha(Qt::black, 0.45));
            QLinearGradient wg(bx, by, bx + bw, by + bh);
            wg.setColorAt(0, hex("#5a3a22"));
            wg.setColorAt(1, hex("#3a2414"));
            x.fillRect(QRectF(bx, by, bw, bh), wg);
            x.fillRect(QRectF(bx + 10, by + 10, bw - 20, bh - 20), hex("#2a2a2e"));
            x.fillRect(QRectF(bx + 10, by + 10, bw - 20, 3), alpha(Qt::white, 0.04));
            // prato com os pontinhos do estroboscópio
            x.setPen(Qt::NoPen);
            x.setBrush(hex("#9a9aa0"));
            x.drawEllipse(QPointF(cx, cy), RR + 8, RR + 8);
            for (int i = 0; i < 120; ++i) { const double a = i / 120.0 * 2 * M_PI; x.fillRect(QRectF(cx + std::cos(a) * (RR + 4) - 1, cy + std::sin(a) * (RR + 4) - 1, 2, 2), hex("#5a5a60")); }
            x.setBrush(hex("#bdbdc2"));
            x.drawEllipse(QPointF(bx + 34, by + bh - 34), 12, 12);
            x.fillRect(QRectF(bx + 60, by + bh - 40, 22, 12), hex("#1a1a1e"));
            x.fillRect(QRectF(bx + 88, by + bh - 40, 22, 12), hex("#1a1a1e"));
            x.end();
            m_desk.set(c, pm);
            m_rec = record(pal);
            m_mote = softDot(hex("#ffd9a0"), 0.4);
        }
        blit(p, m_desk.pm, 0, 0, c.W, c.H);
        // luz quente do abajur
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        const double lx = zx(c, 60);
        QRadialGradient lg(QPointF(lx, 40), 620);
        lg.setColorAt(0, alpha(hex("#ffcf8a"), 0.16));
        lg.setColorAt(1, alpha(hex("#ffcf8a"), 0));
        p.fillRect(QRectF(0, 0, c.W, c.H), lg);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // disco girando (33⅓ rpm)
        p.save();
        p.translate(cx, cy);
        p.rotate(m_t * 33.333 / 60 * 360);
        const double rs = RR * 2 + 4;
        p.drawImage(QRectF(-rs / 2, -rs / 2, rs, rs), m_rec);
        p.restore();
        // o reflexo NÃO gira: duas cunhas de luz paradas sobre o vinil
        p.save();
        QPainterPath ring;
        ring.addEllipse(QPointF(cx, cy), RR - 2, RR - 2);
        ring.addEllipse(QPointF(cx, cy), 44, 44);
        ring.setFillRule(Qt::OddEvenFill);
        p.setClipPath(ring, Qt::IntersectClip);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (double a0 : { -0.9, M_PI - 0.9 }) {
            QRadialGradient g(QPointF(cx, cy), RR);
            g.setColorAt(44.0 / RR, alpha(Qt::white, 0.03));
            g.setColorAt(1, alpha(hex("#ffe8c8"), 0.13));
            QPainterPath w(QPointF(cx, cy));
            w.arcTo(QRectF(cx - RR, cy - RR, RR * 2, RR * 2), -(a0 - 0.16) * 180 / M_PI, -0.32 * 180 / M_PI);
            w.closeSubpath();
            p.fillPath(w, g);
        }
        p.restore();
        // braço: pivô no canto, cabeça percorrendo o disco
        const double px = cx + 150, py = cy - 115, ang = lerp(2.18, 2.5, m_arm) - (m_lift > 0 ? 0.06 : 0), L = 205;
        const double hx = px + std::cos(ang) * L, hy = py + std::sin(ang) * L, up = m_lift > 0 ? 4 : 0;
        p.setPen(pen(alpha(Qt::black, 0.4), 6));
        p.drawLine(QPointF(px + 6, py + 9), QPointF(hx + 8 + up, hy + 10 + up));
        disc(p, px, py, 15, hex("#c8c8ce"));
        disc(p, px, py, 7, hex("#7a7a80"));
        p.save();
        p.translate(px, py);
        p.rotate((ang + M_PI) * 180 / M_PI);
        p.fillRect(QRectF(10, -7, 30, 14), hex("#2a2a2e"));
        p.restore();
        p.setPen(pen(hex("#d4d4da"), 4));
        p.drawLine(QPointF(px, py), QPointF(hx, hy));
        p.setPen(pen(alpha(Qt::white, 0.7), 1.2));
        p.drawLine(QPointF(px - 1, py - 1), QPointF(hx - 1, hy - 1));
        p.save();
        p.translate(hx, hy);
        p.rotate((ang + 0.35) * 180 / M_PI);
        p.fillRect(QRectF(-4, -8, 24, 16), hex("#1e1e22"));
        p.fillRect(QRectF(20, -2, 6, 4), hex("#e0e0e6"));
        p.restore();
        disc(p, cx - RR - 18, cy + RR - 6, 3, alpha(hex("#ff5a3a"), 0.9));
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const auto& m : m_motes) {
            const double a = (0.3 + 0.3 * std::sin(m_t * 1.1 + m[2])) * m[3] * qMax(0.0, 1 - std::hypot(m[0] - lx, m[1] - 40) / 700), s = 3 * m[3];
            if (a < 0.01) continue;
            p.setOpacity(a);
            blit(p, m_mote, m[0] - s, m[1] - s, s * 2, s * 2);
        }
        p.setOpacity(1);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    static constexpr double RR = 128;
    static QImage record(const Pal& pal)
    {
        const int s = int(RR * 2 + 4) * 2;   // em dobro, pra girar sem serrilhar
        QImage img(s, s, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter x(&img);
        x.setRenderHint(QPainter::Antialiasing, true);
        x.setRenderHint(QPainter::SmoothPixmapTransform, true);
        x.scale(2, 2);
        const double o = RR + 2;
        x.setPen(Qt::NoPen);
        x.setBrush(hex("#0c0c0e"));
        x.drawEllipse(QPointF(o, o), RR, RR);
        x.setBrush(Qt::NoBrush);
        // sulcos e faixas de silêncio entre as músicas
        for (double r = 46; r < RR - 4; r += 1.3) {
            bool gap = false;
            for (double g : { 70.0, 88.0, 103.0, 117.0 }) if (std::abs(r - g) < 1.2) gap = true;
            x.setPen(QPen(alpha(gap ? Qt::black : Qt::white, gap ? 0.6 : R(0.02, 0.06)), gap ? 1.4 : 0.7));
            x.drawEllipse(QPointF(o, o), r, r);
        }
        x.setPen(QPen(alpha(Qt::white, 0.1), 1));
        x.drawEllipse(QPointF(o, o), RR - 1, RR - 1);
        // selo do P.H.L: a própria capa, recortada em círculo
        if (!phlCover().isNull()) {
            const double lr = 46;
            QPainterPath circ;
            circ.addEllipse(QPointF(o, o), lr, lr);
            x.save();
            x.setClipPath(circ, Qt::IntersectClip);
            x.drawImage(QRectF(o - lr, o - lr, lr * 2, lr * 2), phlCover());
            QRadialGradient edge(QPointF(o, o), lr);
            edge.setColorAt(0.82, alpha(Qt::black, 0));
            edge.setColorAt(1, alpha(Qt::black, 0.35));
            x.fillRect(QRectF(o - lr, o - lr, lr * 2, lr * 2), edge);
            x.restore();
            x.setPen(QPen(alpha(Qt::black, 0.6), 0.8));
            x.setBrush(Qt::NoBrush);
            x.drawEllipse(QPointF(o, o), lr, lr);
            x.setPen(Qt::NoPen);
            x.setBrush(hex("#d8d8d8"));
            x.drawEllipse(QPointF(o, o), 3, 3);
            x.end();
            return img;
        }
        x.setPen(Qt::NoPen);
        x.setBrush(mix(pal.acc, hex("#8a2a1a"), 0.2));
        x.drawEllipse(QPointF(o, o), 42, 42);
        QPainterPath lbl;
        lbl.addEllipse(QPointF(o, o), 42, 42);
        lbl.addEllipse(QPointF(o, o), 30, 30);
        lbl.setFillRule(Qt::OddEvenFill);
        x.fillPath(lbl, alpha(Qt::black, 0.18));
        x.setPen(mix(hex("#fff4e0"), pal.acc, 0.15));
        QFont f(QStringLiteral("IBM Plex Sans"));
        f.setPixelSize(9);
        f.setWeight(QFont::DemiBold);
        x.setFont(f);
        x.drawText(QRectF(o - 40, o - 24, 80, 14), Qt::AlignCenter, QStringLiteral("P.H.L"));
        f.setPixelSize(6);
        f.setWeight(QFont::Normal);
        x.setFont(f);
        x.drawText(QRectF(o - 40, o + 13, 80, 9), Qt::AlignCenter, QStringLiteral("THE SPRING IS COMING"));
        x.drawText(QRectF(o - 40, o + 21, 80, 9), Qt::AlignCenter, QStringLiteral("LADO A  ·  33⅓"));
        x.setPen(Qt::NoPen);
        x.setBrush(hex("#d8d8d8"));
        x.drawEllipse(QPointF(o, o), 3, 3);
        x.end();
        return img;
    }
    Cache m_desk;
    QImage m_rec;
    QPixmap m_mote;
    std::vector<std::array<double, 4>> m_motes;
    double m_t = 0, m_arm = 0, m_lift = 0;
};

} // namespace

std::unique_ptr<Layer> makeLeva2a(const QString& id)
{
    if (id == QLatin1String("sand")) return std::make_unique<Sandstorm>();
    if (id == QLatin1String("ripples")) return std::make_unique<Ripples>();
    if (id == QLatin1String("clouds")) return std::make_unique<Clouds>();
    if (id == QLatin1String("prairie")) return std::make_unique<Prairie>();
    if (id == QLatin1String("deepsea")) return std::make_unique<DeepSea>();
    if (id == QLatin1String("glowtide")) return std::make_unique<Glowtide>();
    if (id == QLatin1String("lanterns")) return std::make_unique<Lanterns>();
    if (id == QLatin1String("orbit")) return std::make_unique<Orbit>();
    if (id == QLatin1String("turntable")) return std::make_unique<Turntable>();
    return nullptr;
}

} // namespace AnimBg
