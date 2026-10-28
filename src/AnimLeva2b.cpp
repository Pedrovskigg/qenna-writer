// Temas Animados, leva 2 (parte 2): nanquim, fogueira, fogos, show e
// hiperespaço. Portado de leva2b.js / leva2c.js / leva2d.js.

#include "AnimKit.h"
#include "MsVignetteLeva3.h"

#include <QImage>
#include <QLinearGradient>
#include <QPolygonF>
#include <QRadialGradient>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace AnimBg {
namespace {

using namespace kit;

// ============================================================ nanquim na água

class InkBloom : public Layer {
public:
    void init(Ctx& c) override
    {
        m_blooms.clear();
        m_next = 0.3;
        for (int i = 0; i < 2; ++i) { m_blooms.push_back(newBloom(c)); m_blooms.back().t = -i * 2.0; }
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_next -= dt;
        if (m_next <= 0 && m_blooms.size() < 4) { m_blooms.push_back(newBloom(c)); m_next = R(3.5, 7) / qMin(1.6, INT(c)); }
        for (Bloom& b : m_blooms) {
            b.t += dt;
            if (b.t < b.drop) continue;
            QPainter x(&b.img);
            x.setRenderHint(QPainter::SmoothPixmapTransform, true);
            const QPixmap& dot = dotFor(b.col);
            const size_t n0 = b.heads.size();
            for (size_t i = 0; i < n0; ++i) {
                Head& h = b.heads[i];
                if (h.age > 11) continue;
                h.age += dt;
                // cabeças descendo e se enrolando, cada vez mais devagar; de vez em quando uma se divide
                const double curl = std::sin(h.y * 0.022 + b.seed + h.age * 0.5) * 42 + std::cos(h.x * 0.028 + b.seed) * 18;
                h.vx += (curl - h.vx) * dt * 0.8;
                h.vy *= std::pow(0.82, dt);
                h.vy += 2 * dt;
                h.x = qBound(30.0, h.x + h.vx * dt * 0.8, 390.0);
                h.y += h.vy * dt;
                const double r = (2 + h.age * 5) * h.w, a = qMin(0.5, 0.1 / (1 + h.age * 0.5)) * h.w;
                x.setOpacity(a);
                blit(x, dot, h.x - r, h.y - r, r * 2, r * 2);
                if (b.heads.size() < 40 && rnd() < dt * 1.1) b.heads.push_back({ h.x, h.y, h.vx + R(-30, 30), h.vy * R(0.6, 1), h.age, h.w * 0.75 });
            }
        }
        m_blooms.erase(std::remove_if(m_blooms.begin(), m_blooms.end(), [](const Bloom& b) { return b.t >= b.life; }), m_blooms.end());
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        m_acc = pal.acc;
        sky(p, c, mix(pal.app, Qt::white, 0.35), pal.app);
        QLinearGradient g(0, 0, c.W * 0.3, c.H);
        g.setColorAt(0, alpha(Qt::white, 0.35));
        g.setColorAt(1, alpha(Qt::white, 0));
        p.fillRect(QRectF(0, 0, c.W, c.H), g);
        for (const Bloom& b : m_blooms) {
            if (b.t < b.drop) {
                const double k = b.t / b.drop;
                p.setPen(Qt::NoPen);
                p.setBrush(alpha(b.col, 0.85));
                p.drawEllipse(QPointF(b.x + 210, b.y + 30 - (1 - k) * 160), 2.6, 5);
                continue;
            }
            const double fade = qMin(1.0, (b.life - b.t) / 5);
            // a tinta se espalha: a mesma imagem por baixo, um pouco maior e mais clara
            p.setOpacity(0.18 * fade);
            p.drawImage(QRectF(b.x - 6, b.y - 3, CW + 12, CH + 10), b.img);
            p.setOpacity(fade);
            p.drawImage(QRectF(b.x, b.y, CW, CH), b.img);
            if (b.t < b.drop + 0.6) {
                const double k = (b.t - b.drop) / 0.6;
                p.setOpacity(1 - k);
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(alpha(hex("#1d2230"), 0.25), 1));
                p.drawEllipse(QPointF(b.x + 210, b.y + 30), 6 + 40 * k, (6 + 40 * k) * 0.3);
            }
            p.setOpacity(1);
        }
    }

private:
    static constexpr int CW = 420, CH = 620;
    struct Head { double x, y, vx, vy, age, w; };
    struct Bloom { double x, y; QImage img; QColor col; std::vector<Head> heads; double t, drop, life, seed; };
    const QPixmap& dotFor(const QColor& col)
    {
        QPixmap& d = m_dots[col.rgb()];
        if (d.isNull()) d = softDot(col, 0.4);
        return d;
    }
    Bloom newBloom(const Ctx& c) const
    {
        const QPointF v = visPoint(c);
        const double x = v.x(), y = qMin(v.y(), c.H * 0.45);
        const double r = rnd();
        const QColor col = r < 0.62 ? hex("#1d2230") : r < 0.92 ? (m_acc.isValid() ? m_acc : hex("#2e3f8f")) : hex("#c23b22");
        Bloom b { x - CW / 2.0, y - 30, QImage(CW, CH, QImage::Format_ARGB32_Premultiplied), col, {}, 0, 0.35, R(16, 22), R(0, 100) };
        b.img.fill(Qt::transparent);
        for (int i = 0, n = 4 + RI(3); i < n; ++i) b.heads.push_back({ CW / 2.0 + R(-4, 4), 30, R(-50, 50), R(25, 70), 0, R(0.8, 1.3) });
        return b;
    }
    std::vector<Bloom> m_blooms;
    std::map<QRgb, QPixmap> m_dots;
    QColor m_acc;
    double m_t = 0, m_next = 0.5;
};

// ============================================================ fogueira

class Campfire : public Layer {
public:
    void init(Ctx& c) override
    {
        m_back.reset();
        m_sparks.assign(count(34, c), Spark());
        for (Spark& s : m_sparks) { spawn(s, true, c); s.y += s.vy * s.life; }
        m_smoke.clear();
        m_tongues.clear();
        for (int i = 0; i < 7; ++i) m_tongues.push_back({ (i / 6.0 - 0.5) * 34, R(40, 70) * (1 - std::abs(i / 6.0 - 0.5) * 0.9), R(9, 14), 0, R(0, 6.28) });
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_n1 += (R(-1, 1) - m_n1) * qMin(1.0, dt * 9);
        m_n2 += (R(-1, 1) - m_n2) * qMin(1.0, dt * 2.5);
        for (auto& t : m_tongues) t[3] += (R(-1, 1) - t[3]) * qMin(1.0, dt * 7);
        for (Spark& s : m_sparks) {
            s.life += dt; s.y += s.vy * dt; s.x += (s.vx + std::sin(m_t * 3 + s.ph) * 20) * dt; s.vy *= std::pow(0.8, dt);
            if (s.life > s.max) spawn(s, false, c);
        }
        const double fx = fxx(c), fy = c.H * 0.78 + 26;
        if (rnd() < dt * 3) m_smoke.push_back({ fx + R(-6, 6), fy - 70, 8, 0.12, R(4, 12) });
        for (auto& s : m_smoke) { s[1] -= 22 * dt; s[0] += (s[4] + std::sin(m_t * 0.6 + s[1] * 0.03) * 6) * dt; s[2] += 9 * dt; s[3] -= 0.018 * dt; }
        m_smoke.erase(std::remove_if(m_smoke.begin(), m_smoke.end(), [](const std::array<double, 5>& s) { return s[3] <= 0; }), m_smoke.end());
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double GY = c.H * 0.78, FY = GY + 26, fx = fxx(c), fl = 0.86 + 0.09 * m_n1 + 0.07 * m_n2;
        if (!m_back.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing, true);
            sky(x, c, mix(pal.app, hex("#04060e"), 0.5), mix(pal.app, hex("#14213a"), 0.4));
            x.drawPixmap(0, 0, starLayer(c, 320, GY * 0.75, 0.65));
            fillProfile(x, c, profile(7, 0.5), GY - 150, 60, mix(pal.app, hex("#1c2638"), 0.45), 6);
            // fileira de pinheiros: a linha da serra com pontas finas
            const Profile f = profile(8, 0.5);
            QPainterPath tl(QPointF(-10, c.H));
            for (double xx = -10; xx <= c.W + 10; xx += 5) {
                const double b = GY - 40 + f.at(xx / c.W) * 30, tree = rnd() < 0.28 ? R(18, 60) : R(0, 8);
                tl.lineTo(xx - 2.5, b - tree * 0.35); tl.lineTo(xx, b - tree); tl.lineTo(xx + 2.5, b - tree * 0.35);
            }
            tl.lineTo(c.W + 10, c.H);
            tl.closeSubpath();
            x.fillPath(tl, mix(pal.app, hex("#06090f"), 0.55));
            x.fillRect(QRectF(0, GY, c.W, c.H - GY), mix(pal.app, hex("#0a0806"), 0.6));
            // barraca
            const double tx = fx - 112;
            QPainterPath tent(QPointF(tx - 52, GY + 26));
            tent.lineTo(tx, GY - 56); tent.lineTo(tx + 52, GY + 26); tent.closeSubpath();
            x.fillPath(tent, mix(pal.app, hex("#2a2a22"), 0.5));
            QPainterPath door(QPointF(tx - 12, GY + 26));
            door.lineTo(tx, GY - 14); door.lineTo(tx + 12, GY + 26); door.closeSubpath();
            x.fillPath(door, mix(pal.app, Qt::black, 0.6));
            // sombras no chão, do lado de lá do fogo (a luz vem da fogueira)
            auto groundShadow = [&](double cx, double cy, double rx2, double ry2, double a) {
                x.save();
                x.translate(cx, cy);
                x.scale(1, ry2 / rx2);
                QRadialGradient sg(QPointF(0, 0), rx2);
                sg.setColorAt(0, alpha(Qt::black, a));
                sg.setColorAt(1, alpha(Qt::black, 0));
                x.fillRect(QRectF(-rx2, -rx2, rx2 * 2, rx2 * 2), sg);
                x.restore();
            };
            // tronco de sentar: um cilindro deitado, com casca e a ponta virada pro fogo mostrando os anéis
            const double lx = fx + 98, ly = FY + 6, lw = 82, lr = 10;
            groundShadow(lx + 14, ly + lr + 2, 70, 9, 0.55);
            x.save();
            x.translate(lx, ly);
            x.rotate(-0.04 * 180 / M_PI);
            QLinearGradient bark(0, -lr, 0, lr);
            bark.setColorAt(0, hex("#5a3a24"));
            bark.setColorAt(0.35, hex("#3a2416"));
            bark.setColorAt(1, hex("#140c07"));
            QPainterPath logBody;
            logBody.addRoundedRect(QRectF(-lw / 2, -lr, lw, lr * 2), lr * 0.5, lr);
            x.fillPath(logBody, bark);
            x.save();
            x.setClipPath(logBody, Qt::IntersectClip);
            x.setPen(QPen(alpha(Qt::black, 0.35), 0.9));
            for (int i = 0; i < 7; ++i) {
                const double yy = -lr + 2.5 + i * 2.6;
                QPainterPath bl(QPointF(-lw / 2, yy));
                for (double xx = -lw / 2; xx <= lw / 2; xx += 8) bl.lineTo(xx, yy + std::sin(xx * 0.2 + i * 1.7) * 0.8);
                x.drawPath(bl);
            }
            x.restore();
            // ponta com os anéis, acesa pelo fogo
            x.setPen(Qt::NoPen);
            x.setBrush(hex("#9a6a3e"));
            x.drawEllipse(QPointF(-lw / 2, 0), 5, lr);
            x.setBrush(Qt::NoBrush);
            x.setPen(QPen(alpha(hex("#5a3418"), 0.8), 0.6));
            for (double rr2 : { 0.3, 0.55, 0.8 }) x.drawEllipse(QPointF(-lw / 2, 0), 5 * rr2, lr * rr2);
            x.restore();
            // o violão de aço do índice ilustrado (o mesmo desenho da vinheta), encostado no tronco.
            // De noite ele é quase silhueta: escuro, com um calor de leve e só a borda virada pro fogo acesa.
            QImage gi = MsVignetteLeva3::guitarOnly(1, 0.85 * c.px);
            {
                QImage rim(gi.size(), QImage::Format_ARGB32_Premultiplied);
                rim.fill(Qt::transparent);
                const int d1 = qMax(1, int(std::lround(1.6 * c.px))), d2 = qMax(2, int(std::lround(3.4 * c.px)));
                const QColor rc = hex("#ffb060");
                for (int y = 0; y < gi.height(); ++y) {
                    const QRgb* src = reinterpret_cast<const QRgb*>(gi.constScanLine(y));
                    QRgb* dst = reinterpret_cast<QRgb*>(rim.scanLine(y));
                    for (int xx = 0; xx < gi.width(); ++xx) {
                        const int a0 = qAlpha(src[xx]);
                        if (a0 < 40) continue;
                        const int aL1 = xx - d1 >= 0 ? qAlpha(src[xx - d1]) : 0, aL2 = xx - d2 >= 0 ? qAlpha(src[xx - d2]) : 0;
                        double k = 0;
                        if (aL1 < 40) k = 0.95; else if (aL2 < 40) k = 0.45;
                        if (k <= 0) continue;
                        const int A = int(a0 * k);
                        dst[xx] = qRgba(rc.red() * A / 255, rc.green() * A / 255, rc.blue() * A / 255, A);
                    }
                }
                QPainter q(&gi);
                q.setCompositionMode(QPainter::CompositionMode_SourceAtop);
                q.fillRect(gi.rect(), QColor(6, 4, 3, 178));
                QLinearGradient warm(0, 0, gi.width(), 0);
                warm.setColorAt(0, alpha(hex("#ff8a30"), 0.32));
                warm.setColorAt(0.55, alpha(hex("#ff8a30"), 0));
                q.fillRect(gi.rect(), warm);
                q.setCompositionMode(QPainter::CompositionMode_SourceOver);
                q.drawImage(0, 0, rim);
            }
            const double gw = gi.width() / c.px, gh = gi.height() / c.px, gxb = fx + 122, gyb = FY + 19;
            groundShadow(gxb + 10, gyb, 26, 4, 0.6);
            // em pé, reto: o fundo no chão e as costas apoiadas no tronco (violão encostado não fica torto)
            x.drawImage(QRectF(gxb - gw / 2, gyb - gh + 1.5, gw, gh), gi);
            // pedras em volta do fogo: cada uma com volume, sombra embaixo e o topo pegando a luz
            {
                Profile jit = profile(4, 0.5);
                for (int i = 0; i < 11; ++i) {
                    const double a = M_PI * 0.95 + i / 10.0 * M_PI * 1.1, rx2 = 8 + 3 * std::abs(jit.at(i / 10.0)), ry2 = rx2 * 0.62;
                    const double sx = fx + std::cos(a) * 44, sy = FY + 8 - std::sin(a) * 10;
                    groundShadow(sx + 2, sy + ry2 * 0.7, rx2 * 1.2, ry2 * 0.5, 0.5);
                    x.setPen(Qt::NoPen);
                    x.setBrush(hex("#15110e"));
                    x.drawEllipse(QPointF(sx, sy), rx2, ry2);
                    QRadialGradient lit(QPointF(sx - std::cos(a) * rx2 * 0.35, sy - ry2 * 0.45), rx2 * 0.9);
                    lit.setColorAt(0, alpha(hex("#a8683a"), 0.85));
                    lit.setColorAt(1, alpha(hex("#a8683a"), 0));
                    QPainterPath st;
                    st.addEllipse(QPointF(sx, sy), rx2, ry2);
                    x.fillPath(st, lit);
                }
            }
            x.end();
            m_back.set(c, pm);
            m_glow = softDot(hex("#ffb050"), 0.3);
        }
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // a luz da fogueira em tudo
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        QRadialGradient lg(QPointF(fx, FY - 10), 360 * fl);
        lg.setColorAt(0, alpha(hex("#ff9a3c"), 0.42 * fl));
        lg.setColorAt(0.4, alpha(hex("#c8501e"), 0.14 * fl));
        lg.setColorAt(1, alpha(hex("#c8501e"), 0));
        p.fillRect(QRectF(0, 0, c.W, c.H), lg);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        for (const auto& s : m_smoke) { p.setOpacity(qMax(0.0, s[3]) * 1.4); blit(p, smokeDot(), s[0] - s[2] * 1.4, s[1] - s[2] * 1.4, s[2] * 2.8, s[2] * 2.8); }
        p.setOpacity(1);
        // lenha
        p.setPen(pen(hex("#3a2416"), 9));
        p.drawLine(QPointF(fx - 30, FY + 6), QPointF(fx + 26, FY - 6));
        p.drawLine(QPointF(fx + 30, FY + 6), QPointF(fx - 24, FY - 8));
        p.setPen(pen(alpha(hex("#ff8a3c"), 0.55 * fl), 2));
        p.drawLine(QPointF(fx - 22, FY + 1), QPointF(fx + 18, FY - 8));
        // chamas: línguas que crescem e encolhem cada uma no seu ritmo
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.setOpacity(0.5 * fl);
        blit(p, m_glow, fx - 90, FY - 120, 180, 180);
        p.setOpacity(1);
        const struct { double sc; const char* c0; const char* c1; } LAYERS[2] = { { 1, "#ff6a1a", "#c8300a" }, { 0.62, "#ffe7a0", "#ffa030" } };
        for (const auto& L : LAYERS)
            for (const auto& t : m_tongues) {
                const double h = t[1] * L.sc * (0.8 + 0.3 * t[3]) * fl, w = t[2] * L.sc, bx = fx + t[0] * L.sc, sway = std::sin(m_t * 5 + t[4]) * 4 + t[3] * 3;
                QLinearGradient g(0, FY - h, 0, FY);
                g.setColorAt(0, alpha(hex(L.c1), 0));
                g.setColorAt(0.45, alpha(hex(L.c1), 0.55));
                g.setColorAt(1, alpha(hex(L.c0), 0.9));
                QPainterPath f(QPointF(bx - w, FY));
                f.cubicTo(bx - w, FY - h * 0.5, bx + sway * 0.5, FY - h * 0.7, bx + sway, FY - h);
                f.cubicTo(bx + sway * 0.5 + w * 0.3, FY - h * 0.6, bx + w, FY - h * 0.4, bx + w, FY);
                f.closeSubpath();
                p.fillPath(f, g);
            }
        for (const Spark& s : m_sparks) {
            const double k = s.life / s.max, a = (1 - k) * (0.6 + 0.4 * std::sin(m_t * 11 + s.ph));
            if (a > 0) p.fillRect(QRectF(s.x, s.y, 1.8, 1.8), alpha(hex(k < 0.35 ? "#fff0b0" : "#ff8a3c"), a));
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    struct Spark { double x = 0, y = 0, vy = 0, vx = 0, life = 0, max = 1, ph = 0; };
    // a cena inteira (barraca, fogo, tronco e violão) cabe na faixa de mesa entre a barra lateral e a folha
    static double fxx(const Ctx& c) { return zx(c, 200); }
    void spawn(Spark& s, bool any, const Ctx& c) const
    {
        s.x = fxx(c) + R(-14, 14);
        s.y = c.H * 0.78 + 6;
        s.vy = -R(60, 150);
        s.vx = R(-12, 12);
        s.life = any ? R(0, 2.5) : 0;
        s.max = R(1.2, 3.2);
        s.ph = R(0, 6.28);
    }
    Cache m_back;
    QPixmap m_glow;
    std::vector<Spark> m_sparks;
    std::vector<std::array<double, 5>> m_smoke, m_tongues;
    double m_t = 0, m_n1 = 0, m_n2 = 0;
};

// ============================================================ fogos de réveillon

class Fireworks : public Layer {
public:
    void init(Ctx&) override
    {
        m_back.reset();
        m_rockets.clear();
        m_parts.clear();
        m_flashes.clear();
        m_smoke.clear();
        m_pend = { 0, 0.3, 0.7 };
        m_next = 1.2;
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_next -= dt;
        m_finale -= dt;
        m_cab = std::fmod(m_cab + dt / 40, 1.0);
        if (m_next <= 0) { launch(c); m_next = R(0.4, 1.3) / INT(c); }
        if (m_finale <= 0) { for (int i = 0; i < 6; ++i) m_pend.push_back(i * 0.18); m_finale = R(35, 55); }
        for (double& t : m_pend) t -= dt;
        while (!m_pend.empty() && m_pend.front() <= 0) { m_pend.erase(m_pend.begin()); launch(c); }
        for (Rocket& r : m_rockets) { r.t += dt; if (r.t >= r.d) burst(r); }
        m_rockets.erase(std::remove_if(m_rockets.begin(), m_rockets.end(), [](const Rocket& r) { return r.t >= r.d; }), m_rockets.end());
        for (Part& q : m_parts) {
            q.t += dt;
            q.vx *= std::pow(0.5, dt * q.drag);
            q.vy *= std::pow(0.5, dt * q.drag);
            q.vy += q.g * dt;
            if (q.trail) { q.h.push_back(QPointF(q.x, q.y)); if (q.h.size() > 8) q.h.erase(q.h.begin()); }
            q.x += q.vx * dt;
            q.y += q.vy * dt;
        }
        m_parts.erase(std::remove_if(m_parts.begin(), m_parts.end(), [](const Part& q) { return q.t >= q.d; }), m_parts.end());
        for (auto& f : m_flashes) f[2] += dt;
        m_flashes.erase(std::remove_if(m_flashes.begin(), m_flashes.end(), [](const std::array<double, 4>& f) { return f[2] >= 0.5; }), m_flashes.end());
        for (auto& s : m_smoke) { s[2] += 6 * dt; s[0] += 5 * dt; s[3] -= 0.008 * dt; }
        m_smoke.erase(std::remove_if(m_smoke.begin(), m_smoke.end(), [](const std::array<double, 4>& s) { return s[3] <= 0; }), m_smoke.end());
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double WY = c.H * 0.74, o = zx(c, 45) - 45;
        if (!m_back.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing, true);
            sky(x, c, mix(pal.app, hex("#04060e"), 0.45), mix(pal.app, hex("#1c1a34"), 0.4));
            x.drawPixmap(0, 0, starLayer(c, 140, WY * 0.6, 0.4));
            // Pão de Açúcar e Urca, à esquerda
            QPainterPath hills(QPointF(60 + o, WY));
            hills.cubicTo(80 + o, WY - 90, 140 + o, WY - 120, 175 + o, WY - 118);
            hills.cubicTo(205 + o, WY - 116, 225 + o, WY - 60, 240 + o, WY);
            hills.moveTo(205 + o, WY);
            hills.cubicTo(240 + o, WY - 180, 270 + o, WY - 265, 320 + o, WY - 268);
            hills.cubicTo(360 + o, WY - 270, 385 + o, WY - 180, 410 + o, WY);
            hills.setFillRule(Qt::WindingFill);
            x.fillPath(hills, mix(pal.app, hex("#05070c"), 0.6));
            // praia e prédios da orla
            const QColor bc = mix(pal.app, hex("#0a0c14"), 0.55);
            QVector<QRectF> wins;
            for (double bx = 420 + o; bx < c.W;) {
                const double bw = R(26, 60), bh = R(40, 120);
                x.fillRect(QRectF(bx, WY - bh, bw, bh), bc);
                for (double wy = WY - bh + 6; wy < WY - 6; wy += 8)
                    for (double wx = bx + 4; wx < bx + bw - 4; wx += 7) if (rnd() < 0.3) wins.append(QRectF(wx, wy, 2.5, 3));
                bx += bw + R(0, 4);
            }
            x.setPen(Qt::NoPen);
            x.setBrush(alpha(hex("#ffd28a"), 0.7));
            x.drawRects(wins);
            vfill(x, QRectF(0, WY, c.W, c.H - WY), mix(pal.app, hex("#101428"), 0.45), mix(pal.app, hex("#03040a"), 0.5), WY, c.H);
            x.setPen(QPen(alpha(hex("#ffd28a"), 0.12), 1));
            for (int i = 0; i < 30; ++i) { const double yy = WY + 4 + i * 6, a0 = R(420 + o, c.W - 200); x.drawLine(QPointF(a0, yy), QPointF(R(a0, c.W), yy)); }
            x.end();
            m_back.set(c, pm);
            m_smokeDot = softDot(hex("#8a90a8"), 0.1);
        }
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // bondinho cruzando entre os morros
        const double ax = 178 + o, ay = WY - 120, bx2 = 318 + o, by2 = WY - 270, k = m_cab < 0.5 ? m_cab * 2 : 2 - m_cab * 2;
        const QPointF ctl((ax + bx2) / 2, (ay + by2) / 2 + 22);
        p.setPen(QPen(alpha(hex("#9aa4b8"), 0.35), 0.8));
        p.setBrush(Qt::NoBrush);
        QPainterPath cable(QPointF(ax, ay));
        cable.quadTo(ctl, QPointF(bx2, by2));
        p.drawPath(cable);
        const QPointF q = cable.pointAtPercent(k);
        p.fillRect(QRectF(q.x() - 3, q.y() + 1, 6, 4), alpha(hex("#ffe0a0"), 0.9));
        for (const auto& s : m_smoke) { p.setOpacity(s[3] * 1.6); blit(p, m_smokeDot, s[0] - s[2], s[1] - s[2], s[2] * 2, s[2] * 2); }
        p.setOpacity(1);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const auto& f : m_flashes) glow(p, f[0], f[1], 260, QColor::fromRgb(QRgb(f[3])), 0.3 * (1 - f[2] / 0.5));
        drawSet(p, c, WY, false);
        drawSet(p, c, WY, true);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    struct Rocket { double x, tx, ty, t, d; int type; QColor c, c2; };
    struct Part { double x, y, vx, vy, t, d, drag, g; QColor c; bool trail, crackle; std::vector<QPointF> h; };
    void launch(const Ctx& c)
    {
        static const char* COLS[] = { "#ff4f6b", "#ffd34f", "#4fd1ff", "#b98cff", "#7dff8c", "#ffffff", "#ff9d3c", "#fff1c8" };
        static const int TYPES[] = { 0, 0, 1, 2, 3, 4 };   // peônia, chorão, anel, estalinho, palmeira
        const QPointF v = visPoint(c);
        const double WY = c.H * 0.74;
        m_rockets.push_back({ v.x() + R(-30, 30), v.x(), qMin(WY - 140, v.y()), 0, R(1.1, 1.7), TYPES[RI(6)], hex(COLS[RI(8)]), hex(COLS[RI(8)]) });
    }
    void burst(const Rocket& r)
    {
        const int n[] = { 80, 70, 48, 70, 9 };
        const double v0[] = { 150, 110, 140, 140, 170 };
        const double tilt = R(-0.6, 0.6), squash = R(0.4, 0.9);
        for (int i = 0; i < n[r.type]; ++i) {
            double vx, vy;
            if (r.type == 2) {
                const double a = double(i) / n[r.type] * 2 * M_PI, x0 = std::cos(a) * v0[2], y0 = std::sin(a) * v0[2] * squash;
                vx = x0 * std::cos(tilt) - y0 * std::sin(tilt);
                vy = x0 * std::sin(tilt) + y0 * std::cos(tilt);
            } else {
                const double a = R(0, 2 * M_PI), u = R(-1, 1), m = std::sqrt(1 - u * u) * v0[r.type] * R(0.85, 1.05);
                vx = std::cos(a) * m;
                vy = std::sin(a) * m;
            }
            Part q { r.tx, r.ty, vx, vy, 0, r.type == 1 ? R(2.6, 3.4) : r.type == 4 ? 2.2 : R(1.4, 2), r.type == 1 ? 1.4 : 1.1, r.type == 1 ? 70.0 : 45.0,
                     i % 3 == 0 && r.type == 0 ? r.c2 : r.type == 1 ? hex("#ffcf6a") : r.c, r.type == 1 || r.type == 4, r.type == 3, {} };
            m_parts.push_back(q);
        }
        m_flashes.push_back({ r.tx, r.ty, 0, double(r.c.rgb()) });
        m_smoke.push_back({ r.tx, r.ty, 60, 0.07 });
    }
    void drawSet(QPainter& p, const Ctx& c, double WY, bool mirror)
    {
        auto Y = [&](double y) { return mirror ? 2 * WY - y : y; };
        auto ok = [&](double y) { return !mirror || (2 * WY - y > WY && 2 * WY - y < c.H); };
        auto wv = [&](double y) { return mirror ? std::sin(y * 0.12 + m_t * 3) * 3 : 0.0; };
        const double am = mirror ? 0.28 : 1;
        LineBatch rk;
        for (const Rocket& r : m_rockets) {
            const double k = r.t / r.d, x = lerp(r.x, r.tx, k), y = lerp(WY, r.ty, 1 - std::pow(1 - k, 2));
            if (!ok(y)) continue;
            rk.add(x + wv(y), Y(y), x, Y(y + 18), 0.6 * am);
        }
        rk.flush(p, hex("#ffe0b0"), 1.4);
        // partículas sem rastro em lotes por cor
        std::map<QRgb, LineBatch> batches;
        for (const Part& q : m_parts) {
            if (!ok(q.y)) continue;
            const double k = q.t / q.d;
            double a = std::pow(1 - k, 1.3) * am;
            if (q.crackle && k > 0.6) a *= rnd() < 0.5 ? 1.6 : 0.1;
            a = qMin(1.0, a);
            if (q.trail && q.h.size() > 1) {
                QPainterPath tp(QPointF(q.h[0].x() + wv(q.h[0].y()), Y(q.h[0].y())));
                for (size_t i = 1; i < q.h.size(); ++i) tp.lineTo(q.h[i].x() + wv(q.h[i].y()), Y(q.h[i].y()));
                tp.lineTo(q.x, Y(q.y));
                p.setPen(pen(alpha(q.c, a), 1.3));
                p.setBrush(Qt::NoBrush);
                p.drawPath(tp);
            } else {
                batches[q.c.rgb()].add(q.x + wv(q.y), Y(q.y), q.x - q.vx * 0.05 + wv(q.y), Y(q.y - q.vy * 0.05), a);
            }
        }
        for (auto& b : batches) b.second.flush(p, QColor::fromRgb(b.first), 1.7);
    }
    Cache m_back;
    QPixmap m_smokeDot;
    std::vector<Rocket> m_rockets;
    std::vector<Part> m_parts;
    std::vector<std::array<double, 4>> m_flashes, m_smoke;
    std::vector<double> m_pend;
    double m_t = 0, m_next = 1, m_finale = 30, m_cab = 0;
};

// ============================================================ show (luzes de palco)

class Encore : public Layer {
public:
    void init(Ctx& c) override
    {
        m_crowd.clear();
        for (int r = 0; r < 3; ++r)
            for (double x = -10; x < c.W + 20; x += R(24, 38) - r * 3) {
                Member m { x, r, R(0.85, 1.15) * (1 + r * 0.22), R(0, 6.28), rnd() < 0.14, hairAt(RI(8)), R(0.88, 1.15), rnd() < 0.5 ? 1 : -1 };
                m_crowd.push_back(m);
            }
        m_phones.clear();
        for (size_t i = 0; i < m_crowd.size(); ++i) if (m_crowd[i].r < 2 && rnd() < 0.08) m_phones.push_back({ double(i), R(0, 6.28) });
        m_haze.reset();
        m_sprites.clear();
    }
    void step(double dt, Ctx&) override { m_t += dt; m_hoff += 12 * dt; }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        static const char* SONGS[5][2] = { { "#ff3ea5", "#37e2f0" }, { "#ffb648", "#ffffff" }, { "#ff3b3b", "#4a6cff" }, { "#8a5cff", "#ffb648" }, { "#37ff9a", "#37e2f0" } };
        const double beat = m_t * 96 / 60, pulse = std::pow(qMax(0.0, std::cos(std::fmod(beat, 1.0) * M_PI)), 6);
        const int song = int(m_t / 14) % 5, nx = (song + 1) % 5;
        const double mk = smooth(12.5, 14, std::fmod(m_t, 14.0));
        const QColor c1 = mix(hex(SONGS[song][0]), hex(SONGS[nx][0]), mk), c2 = mix(hex(SONGS[song][1]), hex(SONGS[nx][1]), mk);
        sky(p, c, mix(pal.app, Qt::black, 0.4), mix(pal.app, hex("#120a18"), 0.3));
        const QColor truss = mix(pal.app, hex("#2a2a30"), 0.6);
        p.fillRect(QRectF(0, 22, c.W, 6), truss);
        p.fillRect(QRectF(0, 40, c.W, 3), truss);
        QVector<QLineF> zig;
        for (double x = 0; x < c.W; x += 18) { zig.append(QLineF(x, 28, x + 9, 40)); zig.append(QLineF(x + 9, 40, x + 18, 28)); }
        p.setPen(QPen(truss, 1.2));
        p.drawLines(zig);
        if (!m_haze.valid(c)) {
            // névoa já com a opacidade final, repetida duas vezes lado a lado: um blit só por quadro
            QPixmap pm = layer(c, c.W * 2, c.H);
            QPainter x(&pm);
            const QPixmap dot = softDot(Qt::white, 0.15);
            for (int i = 0, n = int(70 * c.W / 1440); i < n; ++i) {
                const double w = R(260, 600), h = w * R(0.3, 0.5), cx = R(0, c.W), cy = R(80, c.H - 100), a = R(0.05, 0.12) * 0.6;
                for (double ox : { -c.W, 0.0, c.W, 2 * c.W }) { x.setOpacity(a); blit(x, dot, cx + ox - w / 2, cy - h / 2, w, h); }
            }
            x.end();
            m_haze.set(c, pm);
        }
        const double ho = std::fmod(m_hoff, c.W);
        p.drawPixmap(QRectF(0, 0, c.W, c.H), m_haze.pm, QRectF((c.W - ho) * c.px, 0, c.W * c.px, c.H * c.px));
        // fachos: cada cabeça móvel varre num ritmo, as cores trocam a cada música
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.setRenderHint(QPainter::Antialiasing, false);   // borda do facho já é suave pelo degradê
        std::vector<double> fix;
        for (double x = zx(c, 70); x < zx(c, 440); x += 90) fix.push_back(x);
        for (double x = rx(c, 1130); x < c.W; x += 100) fix.push_back(x);
        for (size_t i = 0; i < fix.size(); ++i) {
            const double fx = fix[i];
            const QColor col = i % 2 ? c2 : c1;
            const double ang = M_PI / 2 + std::sin(m_t * (0.35 + (i % 3) * 0.12) + i * 1.3) * 0.55 + (i % 2 ? 0.15 : -0.15), sp = 0.07 + 0.03 * std::sin(m_t * 0.5 + i), len = c.H * 0.95;
            const double a = (0.1 + 0.08 * pulse) * (0.7 + 0.3 * std::sin(m_t * 0.3 + i * 2));
            QLinearGradient g(fx, 34, fx + std::cos(ang) * len, 34 + std::sin(ang) * len);
            g.setColorAt(0, alpha(col, a * 2.2));
            g.setColorAt(0.5, alpha(col, a));
            g.setColorAt(1, alpha(col, 0));
            QPainterPath beam(QPointF(fx, 34));
            beam.lineTo(fx + std::cos(ang - sp) * len, 34 + std::sin(ang - sp) * len);
            beam.lineTo(fx + std::cos(ang + sp) * len, 34 + std::sin(ang + sp) * len);
            beam.closeSubpath();
            p.fillPath(beam, g);
            disc(p, fx, 34, 4, alpha(mix(col, Qt::white, 0.6), 0.9));
        }
        QLinearGradient bl(0, 0, 0, 260);
        bl.setColorAt(0, alpha(c1, 0.12 * pulse));
        bl.setColorAt(1, alpha(c1, 0));
        p.fillRect(QRectF(0, 0, c.W, 260), bl);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setRenderHint(QPainter::Antialiasing, true);
        // a galera: cada um é uma silhueta pronta (cabelo, ombro, braço), balançando no tempo
        if (m_sprites.size() != m_crowd.size() || qAbs(m_spritePx - c.px) > 0.01) buildSprites(pal, c);
        for (size_t i = 0; i < m_crowd.size(); ++i) {
            const Member& m = m_crowd[i];
            const double bob = std::pow(qMax(0.0, std::sin(beat * M_PI + m.ph * 0.3)), 2) * 5 * m.s, base = c.H - 70 + m.r * 34, hy = base - 28 * m.s + bob;
            const double fh = 100 * m.s;
            const Sprite& s = m_sprites[i];
            // alinhado ao pixel físico: o blit vira cópia, sem reamostrar
            const double px = std::round((m.x - s.ox) * c.px) / c.px, py = std::round((hy + 0.428 * fh - s.oy) * c.px) / c.px;
            p.drawPixmap(QPointF(px, py), s.img);
        }
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const auto& ph : m_phones) {
            const Member& m = m_crowd[size_t(ph[0])];
            const double base = c.H - 70 + m.r * 34, x = m.x + 18 + std::sin(m_t * 0.8 + ph[1]) * 6, y = base - 70 * m.s + std::sin(m_t * 1.1 + ph[1]) * 4;
            p.fillRect(QRectF(x - 3, y - 5, 6, 10), alpha(hex("#e8f0ff"), 0.85));
            glow(p, x, y, 22, hex("#cfe0ff"), 0.25);
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    struct Member { double x; int r; double s, ph; bool arm; Fig::Hair hair; double build; int side; };
    struct Sprite { QPixmap img; double ox, oy, w, h; };
    void buildSprites(const Pal& pal, const Ctx& c)
    {
        m_sprites.clear();
        m_spritePx = c.px;
        for (const Member& m : m_crowd) {
            const double fh = 100 * m.s, w = fh * 0.6, h = fh * 1.2 + c.H;   // corpo continua até o chão
            const double hh = fh * 0.75 + 140;
            Sprite s;
            s.w = w; s.h = hh; s.ox = w / 2; s.oy = fh * 0.66;
            s.img = QPixmap(qMax(1, int(w * c.px)), qMax(1, int(hh * c.px)));
            s.img.setDevicePixelRatio(c.px);
            s.img.fill(Qt::transparent);
            QPainter x(&s.img);
            x.setRenderHint(QPainter::Antialiasing, true);
            Fig f;
            f.pose = Fig::Bust;
            f.hair = m.hair;
            f.build = m.build;
            f.arm = m.arm ? Fig::Up : Fig::Down;
            f.side = m.side;
            f.col = mix(pal.app, Qt::black, 0.3 + m.r * 0.22);
            figure(x, w / 2, s.oy, fh, f);
            x.fillRect(QRectF(w / 2 - 0.09 * fh * m.build, s.oy - 2, 0.18 * fh * m.build, hh), f.col);
            Q_UNUSED(h);
            m_sprites.push_back(s);
        }
    }
    std::vector<Member> m_crowd;
    std::vector<std::array<double, 2>> m_phones;
    std::vector<Sprite> m_sprites;
    Cache m_haze;
    double m_t = 0, m_hoff = 0, m_spritePx = 0;
};

// ============================================================ hiperespaço

class Warp : public Layer {
public:
    void init(Ctx& c) override
    {
        m_st.assign(count(900, c), S());
        for (S& s : m_st) spawn(s, true);
        m_nebs.clear();
        for (int i = 0; i < 4; ++i) m_nebs.push_back(newNeb(true));
        m_far.reset();
        m_hasPlanet = false;
        m_warp = 0;
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        const double f = factor(), v = 0.2 * (0.7 + 0.3 * INT(c)) * f;
        for (S& s : m_st) { s.pz = s.z; s.z -= v * dt; if (s.z < 0.03) { spawn(s, false); s.pz = s.z; } }
        for (Neb& n : m_nebs) { n.z -= 0.025 * f * dt; if (n.z < 0.12) n = newNeb(false); }
        if (m_warp > 0) { m_warp += dt; if (m_warp > 4.6) { m_warp = 0; m_nextWarp = R(25, 45); } }
        else { m_nextWarp -= dt; if (m_nextWarp <= 0) m_warp = 0.0001; }
        m_nextPlanet -= dt;
        if (!m_hasPlanet && m_nextPlanet <= 0) {
            static const char* PC[] = { "#c8763a", "#5a8ac8", "#b89a6a", "#8a5ac8" };
            const double side = rnd() < 0.6 ? -1 : 1;
            m_pl = { side * R(1.6, 2.4), R(-0.9, 0.5), 1.4, R(0.16, 0.26), hex(PC[RI(4)]), rnd() < 0.7, R(-0.5, 0.5) };
            m_hasPlanet = true;
        }
        if (m_hasPlanet) { m_pl.z -= 0.05 * f * dt; if (m_pl.z < 0.15) { m_hasPlanet = false; m_nextPlanet = R(30, 55); } }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double cx = c.W / 2, cy = 360, Fz = 520, f = factor();
        sky(p, c, mix(pal.app, Qt::black, 0.2), mix(pal.app, hex("#070a1c"), 0.4));
        if (!m_far.valid(c)) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            const QPixmap d = softDot(hex("#8a7cff"), 0.2);
            x.setOpacity(0.035);
            for (int i = 0; i < 140; ++i) { const double k = rnd(), bx = lerp(-100, c.W + 100, k), by = lerp(c.H * 0.85, c.H * 0.1, k) + R(-80, 80), s = R(80, 220); blit(x, d, bx - s / 2, by - s / 2, s, s); }
            x.setOpacity(1);
            for (int i = 0, n = int(500 * c.W / 1440); i < n; ++i) x.fillRect(QRectF(R(0, c.W), R(0, c.H), 1, 1), alpha(Qt::white, R(0.06, 0.3)));
            x.end();
            m_far.set(c, pm);
        }
        blit(p, m_far.pm, 0, 0, c.W, c.H);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        // nebulosas vindo devagar: crescem, acendem e passam
        for (const Neb& n : m_nebs) {
            const double sx = cx + n.x / n.z * Fz, sy = cy + n.y / n.z * Fz, s = n.r / n.z * Fz, a = smooth(1.6, 1.0, n.z) * smooth(0.12, 0.45, n.z) * 0.3;
            if (a < 0.005) continue;
            p.setOpacity(a);
            blit(p, dot(n.c, 0.1), sx - s, sy - s * 0.7, s * 2, s * 1.4);
            p.setOpacity(a * 0.8);
            blit(p, dot(n.c2, 0.1), sx - s * 0.4, sy - s * 0.2, s * 1.3, s * 1.1);
        }
        p.setOpacity(1);
        // estrelas: o rastro estica com a velocidade (lotes por cor e grossura)
        const double trail = qMin(1.0, 0.04 + (f - 1) * 0.08);
        std::map<int, LineBatch> bk;
        static const char* SC[] = { "#ffffff", "#cfe0ff", "#9fbcff", "#ffe6c4", "#ffc49a" };
        for (const S& s : m_st) {
            const double x = cx + s.x / s.z * Fz, y = cy + s.y / s.z * Fz;
            if (x < -20 || x > c.W + 20 || y < -20 || y > c.H + 20) continue;
            const double near = 1 - s.z, zt = qMin(1.0, s.z + (s.pz - s.z) * (1 + 14 * trail)), x0 = cx + s.x / zt * Fz, y0 = cy + s.y / zt * Fz;
            const double a = qMin(1.0, (0.15 + near * 1.3) * (0.8 + 0.5 * s.b)), w = 0.6 + near * 2.2 * (0.6 + s.b);
            bk[s.c * 4 + qMin(3, int(w))].add(x0, y0, x, y, a);
        }
        for (auto& b : bk) {
            const QColor col = f > 3 ? mix(hex(SC[b.first / 4]), hex("#9fd8ff"), 0.5) : hex(SC[b.first / 4]);
            b.second.flush(p, col, 0.8 + (b.first % 4) * 0.9);
        }
        for (const S& s : m_st) {
            if (s.z > 0.25 || s.b < 0.5) continue;
            const double x = cx + s.x / s.z * Fz, y = cy + s.y / s.z * Fz, r = (1 - s.z) * 14;
            p.setOpacity(0.35);
            blit(p, dot(hex(SC[s.c]), 0.25), x - r, y - r, r * 2, r * 2);
        }
        p.setOpacity(1);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // planeta passando de lado
        if (m_hasPlanet) {
            const double sx = cx + m_pl.x / m_pl.z * Fz, sy = cy + m_pl.y / m_pl.z * Fz, r = m_pl.r / m_pl.z * Fz;
            p.setOpacity(smooth(1.4, 1.1, m_pl.z));
            auto ring = [&](bool front) {
                if (!m_pl.ring) return;
                p.save();
                p.translate(sx, sy);
                p.rotate(m_pl.tilt * 180 / M_PI);
                p.setPen(QPen(alpha(mix(m_pl.c, Qt::white, front ? 0.5 : 0.4), front ? 0.6 : 0.45), r * 0.12));
                p.setBrush(Qt::NoBrush);
                p.drawArc(QRectF(-r * 1.9, -r * 0.45, r * 3.8, r * 0.9), front ? 180 * 16 : 0, 180 * 16);
                p.restore();
            };
            ring(false);
            QRadialGradient g(QPointF(sx, sy), r, QPointF(sx - r * 0.45 * (m_pl.x < 0 ? 1 : -1), sy - r * 0.4));
            g.setColorAt(0, mix(m_pl.c, Qt::white, 0.35));
            g.setColorAt(0.55, m_pl.c);
            g.setColorAt(1, mix(m_pl.c, Qt::black, 0.85));
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawEllipse(QPointF(sx, sy), r, r);
            p.save();
            QPainterPath cl;
            cl.addEllipse(QPointF(sx, sy), r, r);
            p.setClipPath(cl, Qt::IntersectClip);
            for (int i = -3; i <= 3; ++i) p.fillRect(QRectF(sx - r, sy + i * r * 0.26, r * 2, r * 0.08), alpha(Qt::black, 0.12));
            p.restore();
            ring(true);
            p.setOpacity(1);
        }
        // salto: túnel azulado e um clarão no pico
        if (m_warp > 0) {
            const double k = (f - 1) / 15;
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            QRadialGradient g(QPointF(cx, cy), 900);
            g.setColorAt(0, alpha(hex("#9fd8ff"), 0));
            g.setColorAt(0.5, alpha(hex("#4a7aff"), 0.07 * k));
            g.setColorAt(1, alpha(hex("#8a5cff"), 0.12 * k));
            p.fillRect(QRectF(0, 0, c.W, c.H), g);
            if (m_warp > 1.1 && m_warp < 1.5) p.fillRect(QRectF(0, 0, c.W, c.H), alpha(hex("#dff0ff"), 0.18 * (1 - std::abs(m_warp - 1.3) / 0.2)));
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
    }

private:
    struct S { double x = 0, y = 0, z = 1, pz = 1; int c = 0; double b = 0; };
    struct Neb { double x, y, z, r; QColor c, c2; };
    struct Planet { double x, y, z, r; QColor c; bool ring; double tilt; };
    double factor() const
    {
        if (m_warp <= 0) return 1;
        const double t = m_warp;
        return t < 1.2 ? 1 + 15 * smooth(0, 1.2, t) : t < 2.6 ? 16 : 1 + 15 * (1 - smooth(2.6, 4.6, t));
    }
    static void spawn(S& s, bool any)
    {
        s.x = R(-3, 3); s.y = R(-1.7, 1.7); s.z = any ? R(0.04, 1) : 1;
        const double r = rnd() * 14;
        s.c = r < 5 ? 0 : r < 9 ? 1 : r < 11 ? 2 : r < 13 ? 3 : 4;
        s.b = std::pow(rnd(), 2.5);
    }
    static Neb newNeb(bool any)
    {
        static const char* NC[] = { "#7a4cff", "#2fb6c8", "#ff4f9a", "#3a6cff", "#c86bff" };
        return { R(-2.2, 2.2), R(-1.3, 1.3), any ? R(0.3, 1.6) : 1.6, R(0.5, 1.1), hex(NC[RI(5)]), hex(NC[RI(5)]) };
    }
    const QPixmap& dot(const QColor& col, double inner)
    {
        QPixmap& d = m_dots[col.rgb() ^ (inner > 0.2 ? 1u : 0u)];
        if (d.isNull()) d = softDot(col, inner);
        return d;
    }
    std::vector<S> m_st;
    std::vector<Neb> m_nebs;
    Planet m_pl {};
    bool m_hasPlanet = false;
    std::map<QRgb, QPixmap> m_dots;
    Cache m_far;
    double m_t = 0, m_warp = 0, m_nextWarp = 14, m_nextPlanet = 6;
};

} // namespace

std::unique_ptr<Layer> makeLeva2b(const QString& id)
{
    if (id == QLatin1String("ink")) return std::make_unique<InkBloom>();
    if (id == QLatin1String("campfire")) return std::make_unique<Campfire>();
    if (id == QLatin1String("fireworks")) return std::make_unique<Fireworks>();
    if (id == QLatin1String("encore")) return std::make_unique<Encore>();
    if (id == QLatin1String("warp")) return std::make_unique<Warp>();
    return nullptr;
}

} // namespace AnimBg
