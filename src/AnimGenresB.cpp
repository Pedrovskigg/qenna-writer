// Temas Animados de gênero (leva 7, 16–19/10): Hoard (fantasia), Rooftop (cyberpunk) e Citadel (ficção científica).
// Portados de hoard.js, rooftop.js e citadel.js do Weather Lab, do mesmo jeito que AnimGenresA.cpp: o cenário
// parado em imagens prontas com semente fixa, por quadro só o que se mexe. Só noite.

#include "AnimCanvas.h"

#include <QImage>

#include <functional>
#include <thread>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace AnimBg {
namespace {

using namespace cv;

QString sceneKey(const Pal& pal, const Ctx& c)
{
    return QString::number(zx(c, 45), 'f', 1) + QLatin1Char('|') + QString::number(c.W, 'f', 0) + QLatin1Char('|') + QString::number(c.px, 'f', 2) + pal.app.name();
}

// ============================================================ Hoard
// O fundo de uma caverna, quase tudo sombra; um facho de luar entra por uma fenda no teto e cai sobre uma montanha
// de ouro, e no topo do monte uma espada cravada, com runas que acendem. No Foco, o esqueleto de um aventureiro
// sentado contra a parede. Vida: o brilho correndo pela lâmina, as runas, o ouro piscando, a poeira no facho e,
// de vez em quando, uma moeda que escorrega do monte e rola até o chão.
class Hoard : public Layer {
public:
    void init(Ctx&) override { m_key.clear(); m_coins.clear(); m_nextCoin = 1.5; }
    void step(double dt, Ctx&) override
    {
        m_t += dt;
        if (m_key.isEmpty()) return;
        m_nextCoin -= dt;
        if (m_nextCoin <= 0) {
            const int side = rnd() < 0.5 ? -1 : 1;
            const double x0 = m_mx + side * R(10, 60);
            m_coins.push_back({ x0, top(x0) - 2, side * R(18, 34), 0, 0, R(0, 6), 0 });
            m_nextCoin = R(4, 11);
        }
        for (Coin& c : m_coins) {
            c.t += dt; c.spin += dt * 14;
            if (c.rest == 0) {
                c.x += c.vx * dt;
                const bool off = std::abs(c.x - m_mx) > 225;
                const double g = off ? m_my + 20 : qMin(m_my + 20, top(c.x)) - 2;
                c.vy += 260 * dt; c.y += c.vy * dt;
                if (c.y > g) { c.y = g; c.vy = -std::abs(c.vy) * 0.25; if (off) { c.vx *= 0.9; if (std::abs(c.vx) < 4) c.rest = c.t; } }
            }
        }
        m_coins.erase(std::remove_if(m_coins.begin(), m_coins.end(), [](const Coin& c) { return c.rest != 0 && c.t - c.rest >= 3; }), m_coins.end());
        for (Mote& m : m_motes) m.v = std::fmod(m.v + dt * 0.012, 1.0);
    }
    void paint(QPainter& painter, const Pal& pal, Ctx& c) override
    {
        const QString key = sceneKey(pal, c);
        if (key != m_key) {
            Seeded sd(0x40A2Du);
            build(pal, c);
            m_key = key;
        }
        Cv x(painter);
        x.image(m_back, 0, 0);
        const double mx = m_mx, my = m_my, sx0 = mx + 120, sx1 = mx + 10;
        // o facho de luar caindo da fenda no teto até o ouro
        x.lighter();
        x.fillStyle(lin(sx0, 0, sx1, my - 120, { { 0, rgba(MOON, 0.16) }, { 1, rgba(MOON, 0.05) } }));
        x.begin(); x.moveTo(sx0 - 14, 0); x.lineTo(sx0 + 14, 0); x.lineTo(sx1 + 90, my - 100); x.lineTo(sx1 - 80, my - 100); x.fill();
        glowAt(x, sx0, 0, 40, MOON, 0.4);
        x.over();
        x.image(m_gold, 0, 0);
        x.lighter(); x.save(); x.translate(mx + 10, my - 120); x.scale(1, 0.45); glowAt(x, 0, 0, 120, "#ffe0a0", 0.22); x.restore(); x.over();
        // a espada cravada no topo do monte, dentro do facho
        {
            const double sx = mx + 14, sy = top(mx + 14) + 6, L = 120, k = std::fmod(m_t * 0.18, 1.6);
            x.save(); x.translate(sx, sy); x.rotate(0.06);
            x.fillStyle(rgba("#000000", 0.35)); x.begin(); x.ellipse(-10, 4, 24, 4, 0, 0, TAU); x.fill();
            x.fillStyle(lin(-5, 0, 5, 0, { { 0, hex("#5a6470") }, { 0.45, hex("#d8e0ea") }, { 0.55, hex("#9aa6b4") }, { 1, hex("#3a424c") } }));
            x.begin(); x.moveTo(-5, 0); x.lineTo(-5, -L); x.lineTo(5, -L); x.lineTo(5, 0); x.fill();
            x.fillStyle(rgba("#3a424c", 0.6)); x.fillRect(-1, -L + 4, 2, L - 6);
            for (int r = 0; r < 6; ++r) {
                const double a = 0.25 + 0.75 * qMax(0.0, std::sin(m_t * 0.7 - r * 0.6)), y = -L + 18 + r * 14;
                x.strokeStyle(rgba("#7ad8ff", 0.6 * a)); x.lineWidth(1);
                x.begin(); x.moveTo(-2.5, y); x.lineTo(2.5, y + 4); x.moveTo(2.5, y); x.lineTo(-1, y + 6); x.stroke();
                if (a > 0.6) { x.lighter(); glowAt(x, 0, y + 3, 10, "#7ad8ff", 0.25 * a); x.over(); }
            }
            if (k < 1) {
                const double y = -L * k;
                x.lighter();
                x.fillStyle(vgrad(y - 14, y, rgba("#ffffff", 0), rgba("#ffffff", 0.7))); x.fillRect(-5, y - 14, 10, 14);
                x.fillStyle(vgrad(y, y + 14, rgba("#ffffff", 0.7), rgba("#ffffff", 0))); x.fillRect(-5, y, 10, 14);
                x.over();
            }
            x.fillStyle(hex("#c8962a")); x.begin(); x.moveTo(-26, -L - 2); x.quadTo(0, -L + 4, 26, -L - 2); x.lineTo(24, -L - 8); x.quadTo(0, -L - 3, -24, -L - 8); x.fill();
            x.fillStyle(hex("#ffe08a")); x.fillRect(-20, -L - 7, 40, 1.4);
            x.fillStyle(hex("#3a2214")); x.fillRect(-4, -L - 34, 8, 27);
            x.strokeStyle(hex("#6a4a2a")); x.lineWidth(1);
            for (double y = -L - 32; y < -L - 8; y += 4) x.line(-4, y, 4, y + 3);
            x.fillStyle(hex("#c8962a")); x.begin(); x.arc(0, -L - 39, 7, 0, TAU); x.fill();
            x.fillStyle(hex("#c82a2a")); x.begin(); x.arc(0, -L - 39, 3.6, 0, TAU); x.fill();
            x.lighter(); glowAt(x, 0, -L - 39, 14 + 4 * std::sin(m_t * 1.1), "#ff3a2a", 0.35); glowAt(x, 0, -L * 0.5, 70, "#c8e0ff", 0.08); x.over();
            x.restore();
        }
        // as moedas rolando
        for (const Coin& cn : m_coins) {
            const double w = 3.2 * std::abs(std::cos(cn.spin)) + 0.6, a = cn.rest != 0 ? qMax(0.0, 1 - (cn.t - cn.rest) / 3) * 0.5 + 0.5 : 1;
            x.fillStyle(rgba("#f0c860", a)); x.begin(); x.ellipse(cn.x, cn.y, w, 3.2, 0, 0, TAU); x.fill();
            if (std::cos(cn.spin) > 0.8) { x.lighter(); glowAt(x, cn.x, cn.y, 6, "#fff4c8", 0.5 * a); x.over(); }
        }
        // brilhos no ouro e a poeira no facho
        x.lighter();
        for (const Glint& g : m_glints) {
            const double k = std::pow(qMax(0.0, std::sin(m_t * g.f + g.ph)), 12);
            if (k < 0.02) continue;
            glowAt(x, g.x, g.y, 6 * k + 1, "#fff4c8", 0.8 * k);
            x.fillStyle(rgba("#ffffff", k)); x.fillRect(g.x - 4 * k, g.y - 0.4, 8 * k, 0.8); x.fillRect(g.x - 0.4, g.y - 4 * k, 0.8, 8 * k);
        }
        for (const Mote& m : m_motes) {
            const double t = m.v, y = t * (my - 100), w = 14 + t * 80, xx = sx0 + (sx1 - sx0) * t + (m.u - 0.5) * 2 * w + std::sin(m_t * 0.5 + m.ph) * 4;
            x.fillStyle(rgba("#e8f0ff", 0.5 * std::sin(t * M_PI))); x.fillRect(xx, y, m.s, m.s);
        }
        x.over();
    }

private:
    const QColor GOLD = hex("#e8b84a"), MOON = hex("#a8c0e8");
    struct Coin { double x, y, vx, vy, t, spin, rest; };
    struct Glint { double x, y, ph, f; };
    struct Mote { double u, v, s, ph; };

    double top(double xx) const
    {
        return m_my - 175 * std::pow(qMax(0.0, 1 - std::pow((xx - m_mx) / 190, 2)), 0.8) - m_prof.at((xx - m_mx + 200) / 400) * 14;
    }

    void build(const Pal& pal, const Ctx& c)
    {
        const double W = c.W, H = c.H;
        m_mx = zx(c, 230); m_my = 600;
        const double mx = m_mx, my = m_my;
        m_back = layer(c, W, H);
        {
            QPainter qp(&m_back);
            Cv x(qp);
            // a caverna: quase preta, paredes com relevo pegando um pouco do brilho do ouro
            x.fillStyle(vgrad(0, H, mix(pal.app, hex("#06070c"), 0.6), mix(pal.app, hex("#0a0806"), 0.6))); x.fillRect(0, 0, W, H);
            const QColor rock = mix(pal.app, hex("#1a1814"), 0.5);
            const int nR = int(60 * W / 1440.0);
            for (int i = 0; i < nR; ++i) {
                const double cx = R(-50, W + 50), cy = R(-50, H + 50), rx = R(40, 160), ry = R(30, 120), a = R(0.15, 0.4);
                const Profile pr = profile(6, 0.6);
                x.fillStyle(rgba(rock, a)); x.begin();
                for (int k = 0; k <= 24; ++k) { const double t = k / 24.0 * 6.283, r = 1 + pr.at(k / 24.0) * 0.3; x.lineTo(cx + std::cos(t) * rx * r, cy + std::sin(t) * ry * r); }
                x.fill();
            }
            // estalactites no teto
            const QColor stal = mix(pal.app, hex("#0c0c10"), 0.6);
            for (double xx = -10; xx < W + 10; xx += R(14, 40)) {
                const double l = R(20, 110), w = R(6, 16);
                x.fillStyle(stal); x.begin(); x.moveTo(xx - w, 0); x.quadTo(xx - w * 0.3, l * 0.6, xx + R(-2, 2), l); x.quadTo(xx + w * 0.3, l * 0.6, xx + w, 0); x.fill();
                x.strokeStyle(rgba(GOLD, 0.06)); x.lineWidth(1); x.begin(); x.moveTo(xx + w * 0.6, 4); x.quadTo(xx + w * 0.2, l * 0.6, xx, l); x.stroke();
            }
            x.lighter(); glowAt(x, mx, my - 60, 420, "#c88a30", 0.12); x.over();
            // o chão
            x.fillStyle(vgrad(600, H, mix(pal.app, hex("#0e0b07"), 0.6), mix(pal.app, hex("#050403"), 0.6)));
            x.begin(); x.moveTo(-10, H); x.lineTo(-10, 612); x.quadTo(W / 2, 596, W + 10, 620); x.lineTo(W + 10, H); x.fill();
            // no Foco: o esqueleto de um aventureiro sentado contra a parede, à direita, com a espada no colo
            {
                const double sx = W - 210, sy = 640;
                const QColor bone = mix(pal.app, hex("#a89a80"), 0.5);
                x.strokeStyle(bone); x.fillStyle(bone); x.lineWidth(3); x.roundCaps();
                x.begin(); x.arc(sx, sy - 84, 10, 0, TAU); x.fill();
                x.fillStyle(hex("#0a0806")); x.begin(); x.arc(sx - 3.5, sy - 85, 2.6, 0, TAU); x.fill(); x.begin(); x.arc(sx + 3.5, sy - 85, 2.6, 0, TAU); x.fill();
                x.begin(); x.moveTo(sx, sy - 74); x.lineTo(sx + 2, sy - 30); x.stroke();
                for (int k = 0; k < 5; ++k) {
                    x.begin();
                    x.moveTo(sx + 1, sy - 66 + k * 7); x.quadTo(sx - 12, sy - 62 + k * 7, sx - 10, sy - 56 + k * 7);
                    x.moveTo(sx + 1, sy - 66 + k * 7); x.quadTo(sx + 14, sy - 62 + k * 7, sx + 12, sy - 56 + k * 7);
                    x.stroke();
                }
                x.begin();
                x.moveTo(sx - 10, sy - 66); x.lineTo(sx - 18, sy - 40); x.lineTo(sx - 4, sy - 26);
                x.moveTo(sx + 12, sy - 66); x.lineTo(sx + 20, sy - 42); x.lineTo(sx + 8, sy - 26);
                x.moveTo(sx + 2, sy - 30); x.lineTo(sx - 24, sy - 22); x.lineTo(sx - 40, sy);
                x.moveTo(sx + 2, sy - 30); x.lineTo(sx + 26, sy - 20); x.lineTo(sx + 36, sy);
                x.stroke();
                x.strokeStyle(mix(pal.app, hex("#6a7078"), 0.5)); x.lineWidth(2.5); x.line(sx - 30, sy - 24, sx + 34, sy - 30);
                x.fillStyle(mix(pal.app, hex("#3a3a40"), 0.5)); x.begin(); x.arc(sx + 50, sy - 6, 12, M_PI, 0); x.lineTo(sx + 62, sy); x.lineTo(sx + 38, sy); x.fill();
                x.fillStyle(rgba("#000000", 0.4)); x.begin(); x.ellipse(sx, sy + 2, 60, 6, 0, 0, TAU); x.fill();
            }
        }
        // o monte de ouro
        m_prof = profile(8, 0.55);
        m_gold = layer(c, W, H);
        {
            QPainter qp(&m_gold);
            Cv g(qp);
            g.fillStyle(vgrad(my - 190, my + 34, hex("#b8862a"), hex("#3a2610")));
            g.begin(); g.moveTo(mx - 230, my + 22);
            for (double xx = mx - 230; xx <= mx + 230; xx += 3) g.lineTo(xx, qMin(my + 20, top(xx)));
            for (double xx = mx + 230; xx >= mx - 230; xx -= 6) g.lineTo(xx, my + 22 + std::sin(xx * 0.09) * 4 + std::sin(xx * 0.23) * 3);
            g.fill();
            // milhares de moedas, mais claras perto do facho
            const QColor dull = hex("#7a5418"), shine = hex("#f0c860"), black = hex("#000000");
            for (int i = 0; i < 2600; ++i) {
                const double xx = mx + R(-220, 220), t = top(xx);
                if (t > my + 15) continue;
                const double y = R(t, my + 20), lit = qMax(0.0, 1 - std::hypot(xx - (mx + 20), y - (my - 150)) / 220), r = R(2, 4.2);
                g.fillStyle(mix(mix(dull, shine, rnd() * 0.6 + lit * 0.4), black, qMax(0.0, (y - t) / 220) * 0.4));
                g.begin(); g.ellipse(xx, y, r, r * 0.45, R(-0.3, 0.3), 0, TAU); g.fill();
                if (rnd() < 0.3) { g.fillStyle(rgba("#fff4c8", 0.4 + lit * 0.4)); g.fillRect(xx - r * 0.5, y - r * 0.25, r * 0.6, 0.8); }
            }
            for (int i = 0; i < 420; ++i) {
                const int side = rnd() < 0.5 ? -1 : 1;
                const double d = std::pow(rnd(), 1.8) * 120, xx = mx + side * (200 + d), y = my + 14 + R(-6, 14) + d * 0.08, r = R(1.8, 3.6);
                g.fillStyle(mix(dull, GOLD, R(0.2, 0.8))); g.begin(); g.ellipse(xx, y, r, r * 0.42, R(-0.3, 0.3), 0, TAU); g.fill();
                if (rnd() < 0.25) { g.fillStyle(rgba("#fff4c8", 0.4)); g.fillRect(xx - r * 0.4, y - r * 0.2, r * 0.5, 0.7); }
            }
            // as peças grandes: a taça, a coroa, o escudo e um baú aberto
            {
                const double cx = mx - 110, cy = my - 40;
                g.fillStyle(hex("#d8a840")); g.begin(); g.moveTo(cx - 8, cy - 16); g.quadTo(cx, cy, cx + 8, cy - 16); g.fill();
                g.fillRect(cx - 1.5, cy - 6, 3, 10); g.fillRect(cx - 6, cy + 3, 12, 2.5);
                g.fillStyle(hex("#c82a3a")); g.begin(); g.arc(cx, cy - 10, 1.8, 0, TAU); g.fill();
            }
            {
                const double cx = mx + 140, cy = my - 22;
                g.fillStyle(hex("#e8c050")); g.begin(); g.moveTo(cx - 12, cy); g.lineTo(cx - 12, cy - 8); g.lineTo(cx - 7, cy - 4); g.lineTo(cx - 3, cy - 12); g.lineTo(cx + 1, cy - 4); g.lineTo(cx + 6, cy - 12); g.lineTo(cx + 9, cy - 4); g.lineTo(cx + 13, cy - 8); g.lineTo(cx + 13, cy); g.fill();
                const struct { double dx; const char* col; } J[3] = { { -6, "#3a8ac8" }, { 1, "#c82a3a" }, { 8, "#3ac87a" } };
                for (const auto& j : J) { g.fillStyle(hex(j.col)); g.begin(); g.arc(cx + j.dx, cy - 3, 1.5, 0, TAU); g.fill(); }
            }
            {
                g.save(); g.translate(mx - 70, my - 105); g.rotate(-0.4);
                g.fillStyle(hex("#6a1a18")); g.begin(); g.ellipse(0, 0, 18, 22, 0, 0, TAU); g.fill();
                g.strokeStyle(hex("#d8a840")); g.lineWidth(2.5); g.stroke();
                g.begin(); g.moveTo(0, -18); g.lineTo(0, 18); g.moveTo(-15, 0); g.lineTo(15, 0); g.stroke();
                g.restore();
            }
            {
                const double cx = mx + 175, cy = my + 4;
                g.fillStyle(hex("#4a2a14")); g.fillRect(cx - 22, cy - 22, 44, 22);
                g.fillStyle(hex("#6a3a1a")); g.save(); g.translate(cx, cy - 22); g.rotate(-0.5); g.fillRect(-22, -18, 44, 18); g.restore();
                g.strokeStyle(hex("#d8a840")); g.lineWidth(2); g.strokeRect(cx - 22, cy - 22, 44, 22);
                g.fillStyle(hex("#f0c860"));
                for (int k = 0; k < 18; ++k) { g.begin(); g.ellipse(cx + R(-18, 18), cy - 22 + R(-6, 2), 3, 1.3, 0, 0, TAU); g.fill(); }
            }
        }
        m_glints.clear();
        for (int i = 0; i < 50; ++i) { const double xx = mx + R(-170, 190); m_glints.push_back({ xx, R(top(xx) + 4, my), R(0, 6.28), R(0.3, 1.2) }); }
        m_motes.clear();
        for (int i = 0; i < 50; ++i) m_motes.push_back({ rnd(), rnd(), R(0.6, 1.6), R(0, 6.28) });
    }

    QString m_key;
    QPixmap m_back, m_gold;
    Profile m_prof;
    double m_mx = 230, m_my = 600;
    std::vector<Coin> m_coins;
    std::vector<Glint> m_glints;
    std::vector<Mote> m_motes;
    double m_t = 0, m_nextCoin = 1.5;
};


// ============================================================ Rooftop
// Sozinho no meio de milhões: alguém de capuz sentado na mureta de um telhado, de costas, com um gato do lado, na
// chuva; atrás, o letreiro gigante do prédio visto pelo avesso. Lá embaixo e até o horizonte, a megacidade de neon
// na névoa, e uma barca de propaganda passando devagar com o painel holográfico aceso. Vida: a chuva, a brasa e a
// fumaça do cigarro, a barca e o holograma trocando de anúncio, os carros voadores, o trem elevado, os holofotes,
// as luzes piscando, o vapor do respiro, o rabo do gato, os respingos na mureta.
class Rooftop : public Layer {
public:
    void init(Ctx&) override { m_key.clear(); m_cars.clear(); m_nextCar = 0.3; m_smoke.clear(); m_splashes.clear(); m_train = false; m_nextTrain = 2; m_steam.clear(); }
    void step(double dt, Ctx& c) override
    {
        if (m_key.isEmpty()) { m_t += dt; return; }
        const double W = c.W, H = c.H;
        m_cat.tail = std::sin(m_t * 1.1) * 0.5 + std::sin(m_t * 2.7) * 0.15;
        m_cat.nextLook -= dt;
        const double lt = m_cat.nextLook < 0 && m_cat.nextLook > -3 ? 1 : 0;
        m_cat.look += (lt - m_cat.look) * qMin(1.0, dt * 3);
        if (m_cat.nextLook < -3) m_cat.nextLook = R(6, 14);
        m_cat.ear = rnd() < dt * 0.4 ? 1 : m_cat.ear * 0.8;
        m_nextTrain -= dt;
        if (!m_train && m_nextTrain <= 0) { m_train = true; m_trainX = W + 20; m_trainV = R(120, 180); }
        if (m_train) { m_trainX -= m_trainV * dt; if (m_trainX < -260) { m_train = false; m_nextTrain = R(12, 25); } }
        m_signFlick = rnd() < dt * 0.6 ? R(0.15, 0.5) : qMax(0.0, m_signFlick - dt);
        if (rnd() < dt * 6) m_steam.push_back({ m_vent.x() + R(-6, 6), m_vent.y() - 18, 0, R(2, 3.5), R(4, 8), 0 });
        for (Puff& v : m_steam) { v.t += dt; v.y -= 18 * dt; v.x -= 6 * dt; }
        m_steam.erase(std::remove_if(m_steam.begin(), m_steam.end(), [](const Puff& v) { return v.t >= v.life; }), m_steam.end());
        m_t += dt;
        m_blimpX += 6 * dt;
        if (m_blimpX > W + 200) m_blimpX = -200;
        m_nextCar -= dt;
        if (m_nextCar <= 0) {
            const int dir = rnd() < 0.5 ? 1 : -1;
            const double d = rnd();
            m_cars.push_back({ dir, dir > 0 ? -30.0 : W + 30, HZ - 40 + d * 220, (60 + d * 140) * R(0.8, 1.2), 0.4 + d, neon() });
            m_nextCar = R(0.4, 1.4);
        }
        for (Car& cr : m_cars) cr.x += cr.dir * cr.v * dt;
        m_cars.erase(std::remove_if(m_cars.begin(), m_cars.end(), [W](const Car& cr) { return cr.x <= -40 || cr.x >= W + 40; }), m_cars.end());
        for (Drop& r : m_rain) {
            r.y += (500 + 400 * r.z) * dt; r.x -= 60 * r.z * dt;
            if (r.y > H) { r.y = R(-80, 0); r.x = R(-100, W + 100); if (rnd() < 0.3) m_splashes.push_back({ R(0, W), 0 }); }
        }
        for (Splash& s : m_splashes) s.t += dt;
        m_splashes.erase(std::remove_if(m_splashes.begin(), m_splashes.end(), [](const Splash& s) { return s.t >= 0.3; }), m_splashes.end());
        if (rnd() < dt * (2 + m_ember[2] * 10)) m_smoke.push_back({ m_ember[0], m_ember[1], 0, R(2, 3.5), R(2, 4), R(-6, 2) });
        for (Puff& s : m_smoke) { s.t += dt; s.y -= 14 * dt; s.x += (s.vx - 8) * dt; }
        m_smoke.erase(std::remove_if(m_smoke.begin(), m_smoke.end(), [](const Puff& s) { return s.t >= s.life; }), m_smoke.end());
    }
    void paint(QPainter& painter, const Pal& pal, Ctx& c) override
    {
        const QString key = sceneKey(pal, c);
        if (key != m_key) {
            Seeded sd(0x200F7u);
            m_panel = QPixmap();
            build(pal, c);
            m_key = key;
        }
        Cv x(painter);
        x.image(m_back, 0, 0);
        // as luzes da cidade piscando e os carros voadores
        x.lighter();
        for (const Blink& b : m_blinks) if (std::sin(m_t * 2 + b.ph) > 0.7) glowAt(x, b.x, b.y, 5, b.c, 0.6 * 0.85);   // o apagado já está no fundo
        for (const Car& cr : m_cars) {
            glowAt(x, cr.x, cr.y, 4 * cr.s + 2, cr.c, 0.4);
            x.fillStyle(rgba("#ffffff", 0.9)); x.fillRect(cr.x - 0.8 * cr.s, cr.y - 0.5 * cr.s, 1.6 * cr.s, cr.s);
            x.fillStyle(rgba(cr.c, 0.2)); x.fillRect(cr.x, cr.y - 0.3 * cr.s, -cr.dir * 30 * cr.s, 0.6 * cr.s);
        }
        // holofotes varrendo o céu baixo
        // holofotes varrendo o céu baixo: o facho pronto numa imagem (meia resolução, é só luz difusa), girado a cada quadro
        {
            if (m_beam.isNull()) {
                m_beam = QPixmap(260, 40);
                m_beam.fill(Qt::transparent);
                QPainter bq(&m_beam);
                bq.setRenderHint(QPainter::Antialiasing, true);
                bq.scale(0.5, 0.5);
                QPainterPath bp;
                bp.moveTo(0, 40 - 3); bp.lineTo(520, 0); bp.lineTo(520, 80); bp.lineTo(0, 40 + 3); bp.closeSubpath();
                bq.fillPath(bp, lin(0, 0, 520, 0, { { 0, rgba("#e8d0ff", 0.12) }, { 1, rgba("#e8d0ff", 0) } }));
            }
            for (const Beam& b : m_beams) {
                const double a = -M_PI / 2 + std::sin(m_t * b.s + b.ph) * 0.5, L = 520, bx = b.x, by = HZ + 30, ex = bx + std::cos(a) * L, ey = by + std::sin(a) * L;
                x.save(); x.translate(bx, by); x.rotate(a); x.image(m_beam, 0, -40, 520, 80); x.restore();
                glowAt(x, ex, qMax(20.0, ey), 60, "#e8d0ff", 0.05);
            }
        }
        x.over();
        // o trem elevado cruzando
        if (m_train) {
            for (int k = 0; k < 6; ++k) {
                const double tx = m_trainX + k * 40;
                x.fillStyle(mix(pal.app, hex("#1a1424"), 0.5)); x.fillRect(tx, m_railY - 9, 37, 9);
                x.fillStyle(rgba("#e8f0ff", 0.85)); for (int w = 0; w < 6; ++w) x.fillRect(tx + 3 + w * 6, m_railY - 7, 4, 3);
            }
            x.lighter(); glowAt(x, m_trainX + 120, m_railY - 4, 140, "#c8e0ff", 0.08); if (rnd() < 0.3) glowAt(x, m_trainX + R(0, 240), m_railY + 1, 8, "#a8d8ff", 0.8); x.over();
        }
        drawBarge(x, pal, c);
        x.image(m_front, 0, 0);
        // o letreiro de costas aceso: os tubos (prontos numa imagem) e o halo rosa banhando a pessoa
        {
            const double on = m_signFlick > 0 && rnd() < 0.6 ? 0.15 : 1;
            x.lighter(); x.alphaG(on); x.image(m_signLit, 0, 0); x.alphaG(1);
            glowAt(x, m_sign.x() + SIGN_LW * 2, m_sign.y() + 34, 220, PINK, 0.14 * on); x.over();
        }
        // o vapor do respiro
        for (const Puff& v : m_steam) { const double k = v.t / v.life; x.fillStyle(rgba("#d8c8e0", 0.12 * std::sin(M_PI * k))); x.begin(); x.arc(v.x, v.y, v.r * (1 + k), 0, TAU); x.fill(); }
        drawSitter(x, pal);
        // o gato sentado na mureta ao lado, de costas, o rabo pendurado do lado de fora balançando
        {
            const double cx = m_sit.x() + 58, cy = m_sit.y();
            const QColor col = mix(pal.app, hex("#07060a"), 0.6);
            x.fillStyle(col); x.strokeStyle(col); x.save(); x.roundCaps();
            x.lineWidth(3.4); x.begin(); x.moveTo(cx + 6, cy - 2); x.quadTo(cx + 14 + m_cat.tail * 6, cy + 10, cx + 10 + m_cat.tail * 12, cy + 26); x.stroke();
            x.restore();
            x.begin(); x.ellipse(cx, cy - 11, 9, 12, 0, 0, TAU); x.fill(); x.begin(); x.ellipse(cx, cy - 3, 11, 5, 0, 0, TAU); x.fill();
            const double hx = cx + m_cat.look * -3, hy = cy - 26;
            x.begin(); x.ellipse(hx, hy, 7, 6.2, 0, 0, TAU); x.fill();
            for (int sg : { -1, 1 }) { x.begin(); x.moveTo(hx + sg * 6.5, hy - 2); x.lineTo(hx + sg * (4.5 + m_cat.ear * 1.5 * (sg > 0 ? 1 : 0)), hy - 11); x.lineTo(hx + sg * 1.5, hy - 5); x.fill(); }
            x.lighter(); x.strokeStyle(rgba(CYAN, 0.35)); x.lineWidth(1);
            x.begin(); x.ellipse(cx, cy - 11, 9, 12, 0, -1.4, 0.6); x.stroke(); x.begin(); x.ellipse(hx, hy, 7, 6.2, 0, -1.6, 0.2); x.stroke();
            x.strokeStyle(rgba(PINK, 0.35)); x.begin(); x.ellipse(cx, cy - 11, 9, 12, 0, 2.6, 3.9); x.stroke(); x.over();
        }
        // a brasa do cigarro e a fumaça subindo na chuva
        x.lighter(); glowAt(x, m_ember[0], m_ember[1], 6 + m_ember[2] * 10, "#ff6a20", 0.5 + m_ember[2] * 0.4); x.over();
        x.fillStyle(rgba("#ffb060", 0.8 + m_ember[2] * 0.2)); x.fillRect(m_ember[0] - 0.8, m_ember[1] - 0.8, 1.6, 1.6);
        for (const Puff& s : m_smoke) { const double k = s.t / s.life; x.fillStyle(rgba("#c8b8d0", 0.14 * std::sin(M_PI * k))); x.begin(); x.arc(s.x, s.y, s.r + k * 8, 0, TAU); x.fill(); }
        { const double on = std::sin(m_t * 2.6) > 0.7 ? 1 : 0.1; x.lighter(); glowAt(x, m_antenna.x(), m_antenna.y(), 12, "#ff2a1a", 0.8 * on); x.over(); }
        // a chuva, agrupada por profundidade (cada grupo de uma vez), e os respingos na mureta
        {
            constexpr int N = 8;
            QVector<QLineF> b[2][N];
            for (const Drop& r : m_rain) {
                const int zi = qBound(0, int((r.z - 0.3) / 0.7 * N), N - 1);
                b[r.y < LEDGE ? 0 : 1][zi].append(QLineF(r.x, r.y, r.x - 3 * r.z, r.y + r.l * r.z));
            }
            painter.setRenderHint(QPainter::Antialiasing, true);
            for (int s = 0; s < 2; ++s)
                for (int i = 0; i < N; ++i) {
                    if (b[s][i].isEmpty()) continue;
                    const double z = 0.3 + (i + 0.5) / N * 0.7;
                    // traço cosmético (1 pixel, o caminho rápido do Qt) com a espessura do Lab compensada no alfa
                    QPen pen(rgba(s == 0 ? "#e8c8e8" : "#c8c8d8", qMin(1.0, (0.15 + 0.35 * z) * (0.6 + z * 0.6) * c.px)), 0);
                    pen.setCosmetic(true);
                    painter.setPen(pen);
                    painter.drawLines(b[s][i]);
                }
        }
        x.strokeStyle(rgba("#e8d8f0", 0.5)); x.lineWidth(0.8);
        for (const Splash& s : m_splashes) { const double k = s.t / 0.3; x.begin(); x.ellipse(s.x, LEDGE + 1, 2 + k * 5, 0.8 + k, 0, M_PI, 0); x.stroke(); }
    }

private:
    static constexpr double HZ = 418, LEDGE = 560, SIGN_LW = 52;
    const QColor PINK = hex("#ff4ab8"), CYAN = hex("#3ae8ff");
    struct Blink { double x, y, ph; QColor c; };
    struct Car { int dir; double x, y, v, s; QColor c; };
    struct Drop { double x, y, z, l; };
    struct Splash { double x, t; };
    struct Puff { double x, y, t, life, r, vx; };
    struct Beam { double x, ph, s; };
    struct Cat { double tail = 0, look = 0, nextLook = 5, ear = 0; };

    static QColor neon() { return pickHex({ "#ff3aa8", "#3af0ff", "#ffd23a", "#ff5a3a", "#a85aff", "#3aff9a" }); }

    void build(const Pal& pal, const Ctx& c)
    {
        const double W = c.W, H = c.H, sw = W / 1440.0;
        m_sit = QPointF(zx(c, 232), LEDGE);
        m_back = layer(c, W, H);
        {
            QPainter qp(&m_back);
            Cv x(qp);
            // céu baixo e encardido, aceso por baixo pela cidade
            x.fillStyle(vgrad(0, HZ + 40, mix(pal.app, hex("#0a0614"), 0.5), mix(pal.app, hex("#4a1a4a"), 0.4))); x.fillRect(0, 0, W, H);
            const QPixmap cl = softDot(mix(pal.app, hex("#3a1a3a"), 0.4), 0.3);
            for (int i = 0; i < 40; ++i) { const double cx = R(-100, W + 100), cy = R(0, 200), w = R(200, 420); x.alphaG(R(0.2, 0.4)); x.image(cl, cx - w / 2, cy - w * 0.12, w, w * 0.24); }
            x.alphaG(1);
            m_blinks.clear();
            auto win = [&](double x0, double y0, double w, double h, double a, double step) {
                for (double wy = y0 + step; wy < y0 + h - step; wy += step * 2)
                    for (double wx = x0 + step; wx < x0 + w - step; wx += step * 2)
                        if (rnd() < 0.5) { x.fillStyle(rgba(pickHex({ "#ffc890", "#7ab0ff", "#ff9ad8", "#e8f0ff" }), a * R(0.4, 1))); x.fillRect(wx, wy, step, step * 1.3); }
            };
            // torres gigantes lá longe, na névoa
            const double TW[8][3] = { { 60, 34, 90 }, { 150, 22, 170 }, { 300, 46, 40 }, { 420, 26, 200 }, { 760, 60, 20 }, { 980, 30, 150 }, { 1150, 50, 60 }, { 1300, 28, 120 } };
            for (const auto& tw : TW) {
                const double tx = tw[0] * sw, w = tw[1], top = tw[2];
                x.fillStyle(mix(pal.app, hex("#3a1e44"), 0.45)); x.fillRect(tx, top, w, HZ - top + 10);
                x.begin(); x.moveTo(tx, top); x.lineTo(tx + w / 2, top - w * 0.8); x.lineTo(tx + w, top); x.fill(); x.fillRect(tx + w / 2 - 1, top - w * 0.8 - 30, 2, 30);
                for (double yy = top + 10; yy < HZ; yy += 14) { x.fillStyle(rgba(pickHex({ "#ff9ad8", "#7ab0ff", "#ffc890" }), 0.25)); x.fillRect(tx + 3, yy, w - 6, 2); }
                if (rnd() < 0.6) { const QColor nc = neon(); x.fillStyle(rgba(nc, 0.5)); x.fillRect(tx + w * 0.2, top + 30, 3, R(40, 90)); glowAt(x, tx + w * 0.2, top + 60, 30, nc, 0.12); }
                m_blinks.push_back({ tx + w / 2, top - w * 0.8 - 30, R(0, 6.28), hex("#ff2a2a") });
            }
            x.fillStyle(vgrad(0, HZ + 10, rgba("#6a2a6a", 0.1), rgba("#8a3a7a", 0.55))); x.fillRect(0, 0, W, HZ + 10);
            // a faixa de cidade no horizonte, compacta
            for (double xx = -10; xx < W + 10;) { const double w = R(8, 26), h = R(6, 28); x.fillStyle(mix(pal.app, hex("#2a1838"), 0.45)); x.fillRect(xx, HZ - h, w - 1, h + 8); win(xx, HZ - h, w, h, 0.45, 1.4); xx += w; }
            // as fileiras de quarteirões abaixo do horizonte, cada uma maior
            for (int row = 0; row < 9; ++row) {
                const double t = row / 8.0, y = HZ + 8 + std::pow(t, 1.5) * (LEDGE - HZ + 30), sc = 0.25 + t * 1.6;
                const QColor col = mix(mix(pal.app, hex("#1a1026"), 0.5), hex("#6a2a6a"), 0.25 * (1 - t));
                x.lighter(); x.fillStyle(vgrad(y - 3 * sc, y + 2 * sc, rgba("#ffb060", 0), rgba("#ff9a50", 0.22 * (1 - t * 0.5)))); x.fillRect(0, y - 3 * sc, W, 5 * sc); x.over();
                for (double xx = R(-40, 0); xx < W + 20;) {
                    const double w = R(30, 80) * sc, hf = R(6, 26) * sc, d = R(6, 14) * sc, top = y - hf - d * 0.5;
                    if (rnd() < 0.15) { xx += w * 0.4; continue; }
                    x.fillStyle(mix(col, hex("#ffffff"), 0.04)); x.begin(); x.moveTo(xx, top); x.lineTo(xx + w, top); x.lineTo(xx + w - d * 0.3, top - d * 0.5); x.lineTo(xx + d * 0.3, top - d * 0.5); x.fill();
                    x.fillStyle(col); x.fillRect(xx, top, w - 1, hf);
                    win(xx, top, w, hf, 0.55, qMax(1.0, 2.2 * sc));
                    if (rnd() < 0.3) { x.fillStyle(mix(col, hex("#000000"), 0.2)); x.fillRect(xx + w * 0.3, top - d * 0.5 - 5 * sc, 6 * sc, 5 * sc); }
                    if (rnd() < 0.18) { const QColor nc = neon(); x.fillStyle(rgba(nc, 0.7)); x.fillRect(xx + w * 0.1, top - d * 0.5 - 7 * sc, w * 0.5, 2.5 * sc); glowAt(x, xx + w * 0.35, top - d * 0.5 - 6 * sc, 14 * sc, nc, 0.2); }
                    if (rnd() < 0.2) m_blinks.push_back({ xx + w * 0.8, top - d * 0.5 - 2, R(0, 6.28), rnd() < 0.7 ? hex("#ff2a2a") : hex("#ffffff") });
                    xx += w + R(1, 6) * sc;
                }
                x.fillStyle(rgba("#6a2a6a", 0.12 * (1 - t))); x.fillRect(0, y - 40 * sc, W, 40 * sc);
            }
            x.lighter(); glowAt(x, W / 2, HZ + 60, 800, "#ff4ab8", 0.07);
            for (const Blink& bl : m_blinks) glowAt(x, bl.x, bl.y, 5, bl.c, 0.6 * 0.15);   // o estado apagado das luzes, de uma vez
            x.over();
            // o trilho elevado cruzando a cidade
            m_railY = HZ + 62;
            x.strokeStyle(mix(pal.app, hex("#1a1020"), 0.55)); x.lineWidth(3); x.line(-10, m_railY, W + 10, m_railY);
            x.lineWidth(1.5); for (double xx = 10; xx < W; xx += 46) x.line(xx, m_railY, xx, m_railY + 40);
        }
        // o telhado da gente
        m_front = layer(c, W, H);
        {
            QPainter qp(&m_front);
            Cv f(qp);
            const QColor conc = mix(pal.app, hex("#2a2630"), 0.5), iron = mix(pal.app, hex("#1a1620"), 0.55);
            f.fillStyle(vgrad(LEDGE, H, mix(pal.app, hex("#14121a"), 0.55), mix(pal.app, hex("#08070c"), 0.6))); f.fillRect(0, LEDGE + 18, W, H - LEDGE);
            for (int i = 0; i < 9; ++i) {
                const double px = R(0, W), py = R(LEDGE + 40, H - 10), w = R(20, 60);
                f.fillStyle(rgba("#5a2050", 0.4)); f.begin(); f.ellipse(px, py, w, w * 0.12, 0, 0, TAU); f.fill();
                f.fillStyle(rgba(rnd() < 0.5 ? PINK : CYAN, 0.12)); f.begin(); f.ellipse(px, py - 1, w * 0.6, w * 0.05, 0, 0, TAU); f.fill();
            }
            f.fillStyle(conc); f.fillRect(0, LEDGE, W, 20); f.fillStyle(mix(conc, hex("#ff9ad8"), 0.15)); f.fillRect(0, LEDGE, W, 3);
            f.fillStyle(rgba("#000000", 0.3)); for (double xx = 0; xx < W; xx += 90) f.fillRect(xx, LEDGE, 1.5, 20);
            for (int i = 0; i < 30; ++i) { f.fillStyle(rgba("#000000", R(0.05, 0.15))); f.fillRect(R(0, W), LEDGE + R(3, 16), R(10, 40), R(2, 6)); }
            // a caixa d'água sobre pernas, à esquerda
            {
                const double tx = zx(c, 70), ty = LEDGE - 4;
                f.fillStyle(iron); for (double lx : { -22.0, 22.0 }) f.fillRect(tx + lx - 2, ty - 70, 4, 70 + 18);
                f.begin(); f.moveTo(tx - 22, ty - 40); f.lineTo(tx + 22, ty - 10); f.moveTo(tx + 22, ty - 40); f.lineTo(tx - 22, ty - 10); f.strokeStyle(iron); f.lineWidth(1.5); f.stroke();
                f.fillStyle(lin(tx - 32, 0, tx + 32, 0, { { 0, mix(pal.app, hex("#3a2a30"), 0.5) }, { 0.7, mix(pal.app, hex("#1e161c"), 0.55) }, { 1, mix(pal.app, hex("#5a2a50"), 0.45) } }));
                f.fillRect(tx - 32, ty - 150, 64, 82); f.begin(); f.moveTo(tx - 36, ty - 150); f.lineTo(tx, ty - 176); f.lineTo(tx + 36, ty - 150); f.fill();
                f.strokeStyle(rgba("#000000", 0.35)); f.lineWidth(1); for (int k = 1; k < 4; ++k) f.line(tx - 32, ty - 150 + k * 20, tx + 32, ty - 150 + k * 20);
                f.strokeStyle(rgba(PINK, 0.4)); f.line(tx + 32, ty - 150, tx + 32, ty - 68);
            }
            // o andaime do letreiro gigante do próprio telhado (as letras acendem a cada quadro)
            {
                const double bx = zx(c, 40), by = LEDGE - 250, n = 4, lw = SIGN_LW;
                f.strokeStyle(iron); f.lineWidth(2);
                f.begin();
                for (int k = 0; k <= n; ++k) { f.moveTo(bx + k * lw, by - 10); f.lineTo(bx + k * lw, LEDGE); }
                f.moveTo(bx, by + 74); f.lineTo(bx + n * lw, by + 74); f.moveTo(bx, by - 10); f.lineTo(bx + n * lw, by - 10);
                f.stroke();
                f.lineWidth(1); f.begin();
                for (int k = 0; k < n; ++k) { f.moveTo(bx + k * lw, by + 74); f.lineTo(bx + (k + 1) * lw, LEDGE); f.moveTo(bx + (k + 1) * lw, by + 74); f.lineTo(bx + k * lw, LEDGE); }
                f.stroke();
                m_sign = QPointF(bx, by);
                // as letras ao contrário em tubos (vistas de trás): o escuro fica na frente, o aceso numa imagem própria
                m_signLit = layer(c, W, H);
                QPainter lq(&m_signLit);
                Cv l(lq);
                const QStringList fam = { QStringLiteral("Arial Black"), QStringLiteral("IBM Plex Sans"), QStringLiteral("Arial") };
                f.setFont(70, 900, fam); f.align = 0; f.baseline = 1;
                l.setFont(70, 900, fam); l.align = 0; l.baseline = 1;
                l.lighter();
                const QString letters = QStringLiteral("LETO");
                for (int k = 0; k < letters.size(); ++k) {
                    const QString ch = letters.mid(k, 1);
                    const double cx = bx + k * lw + lw / 2, cy = by + 34;
                    f.save(); f.translate(cx, cy); f.scale(-1, 1); f.strokeStyle(rgba("#3a1a30", 0.9)); f.lineWidth(6); f.strokeText(ch, 0, 0); f.restore();
                    l.save(); l.translate(cx, cy); l.scale(-1, 1);
                    l.strokeStyle(rgba(PINK, 0.25)); l.lineWidth(9); l.strokeText(ch, 0, 0); l.strokeStyle(rgba("#ffd0ec", 0.8)); l.lineWidth(2); l.strokeText(ch, 0, 0);
                    l.restore();
                }
            }
            // o respiro de vapor no chão do telhado
            m_vent = QPointF(zx(c, 150), LEDGE + 70);
            f.fillStyle(mix(pal.app, hex("#2a2630"), 0.5)); f.fillRect(m_vent.x() - 10, m_vent.y() - 14, 20, 14);
            f.fillStyle(mix(pal.app, hex("#3a3640"), 0.5)); f.fillRect(m_vent.x() - 13, m_vent.y() - 18, 26, 5);
            // a antena com a luz piscando, à direita da pessoa
            {
                const double ax = zx(c, 340);
                f.strokeStyle(iron); f.lineWidth(2); f.line(ax, LEDGE, ax, LEDGE - 150); f.lineWidth(1);
                for (int k = 0; k < 4; ++k) f.line(ax - 10 + k, LEDGE - 140 + k * 20, ax + 10 - k, LEDGE - 140 + k * 20);
                m_antenna = QPointF(ax, LEDGE - 152);
            }
        }
        m_rain.clear();
        for (int i = 0; i < int(260 * sw); ++i) m_rain.push_back({ R(-100, W + 100), R(-H, H), R(0.3, 1), R(10, 22) });
        m_blimpX = zx(c, 210);
        m_beams = { { 520 * sw, 0, 0.21 }, { 900 * sw, 2, 0.17 }, { 1250 * sw, 4, 0.26 }, { 200 * sw, 1, 0.19 } };
    }

    // a barca de propaganda: casco anguloso flutuando com os propulsores acesos, projetando pra cima um painel holográfico
    void drawBarge(Cv& x, const Pal& pal, const Ctx& c)
    {
        const double by = 205 + std::sin(m_t * 0.8) * 3, tilt = std::sin(m_t * 0.5) * 0.02;
        x.save(); x.translate(m_blimpX, by); x.rotate(tilt);
        const double PW = 190, PH = 92, py = -40 - PH;
        x.fillStyle(mix(pal.app, hex("#14121c"), 0.55)); x.begin(); x.moveTo(-110, -6); x.lineTo(-80, -16); x.lineTo(90, -16); x.lineTo(124, -4); x.lineTo(96, 10); x.lineTo(-90, 10); x.close(); x.fill();
        x.fillStyle(mix(pal.app, hex("#22202c"), 0.5)); x.begin(); x.moveTo(-80, -16); x.lineTo(90, -16); x.lineTo(80, -22); x.lineTo(-66, -22); x.fill();
        x.fillStyle(mix(pal.app, hex("#0a0a10"), 0.6)); x.begin(); x.moveTo(-60, 10); x.lineTo(70, 10); x.lineTo(52, 20); x.lineTo(-44, 20); x.fill();
        x.strokeStyle(rgba("#000000", 0.4)); x.lineWidth(1); for (double xx : { -50.0, -10.0, 30.0, 70.0 }) x.line(xx, -16, xx - 4, 10);
        x.lighter();
        x.strokeStyle(rgba(CYAN, 0.8)); x.lineWidth(1.2); x.begin(); x.moveTo(-110, -6); x.lineTo(-80, -16); x.lineTo(90, -16); x.lineTo(124, -4); x.stroke();
        x.strokeStyle(rgba(PINK, 0.5)); x.line(-90, 10, 96, 10);
        for (int k = 0; k < 9; ++k) { const double on = std::sin(m_t * 2 + k * 0.7) > -0.3 ? 1 : 0.2; x.fillStyle(rgba("#c8f0ff", 0.7 * on)); x.fillRect(-60 + k * 14, -9, 6, 2); }
        for (double tx : { -58.0, 0.0, 58.0 }) {
            const double fl = 0.75 + 0.25 * std::sin(m_t * 17 + tx);
            glowAt(x, tx, 22, 16, CYAN, 0.6 * fl); x.fillStyle(rgba("#e8fcff", 0.9)); x.fillRect(tx - 5, 19, 10, 2);
            x.fillStyle(vgrad(22, 22 + 70 * fl, rgba(CYAN, 0.28), rgba(CYAN, 0)));
            x.begin(); x.moveTo(tx - 5, 22); x.lineTo(tx + 5, 22); x.lineTo(tx + 12, 22 + 70 * fl); x.lineTo(tx - 12, 22 + 70 * fl); x.fill();
        }
        const double bl = std::sin(m_t * 3) > 0.6 ? 1 : 0.1;
        glowAt(x, 124, -4, 8, "#ff2a2a", 0.9 * bl); glowAt(x, -110, -6, 7, "#3aff8a", 0.8 * (1 - bl * 0.9));
        for (double ex : { -70.0, 70.0 }) { x.fillStyle(rgba(CYAN, 0.9)); x.fillRect(ex - 4, -24, 8, 3); glowAt(x, ex, -23, 10, CYAN, 0.6); }
        x.fillStyle(vgrad(-24, py + PH, rgba(CYAN, 0.22), rgba(CYAN, 0.02)));
        x.begin(); x.moveTo(-74, -24); x.lineTo(-66, -24); x.lineTo(-PW / 2 + 6, py + PH); x.lineTo(-PW / 2, py + PH); x.fill();
        x.begin(); x.moveTo(74, -24); x.lineTo(66, -24); x.lineTo(PW / 2 - 6, py + PH); x.lineTo(PW / 2, py + PH); x.fill();
        // o painel holográfico: sem moldura, translúcido; o conteúdo troca a cada 7 s com um rasgo
        const int scene = int(m_t / 7) % 3;
        const double tin = std::fmod(m_t, 7.0);
        const bool glitch = tin < 0.35 || rnd() < 0.012;
        const double on = std::sin(m_t * 41) > -0.9 ? 1 : 0.4;
        const QColor col = scene == 0 ? PINK : scene == 1 ? CYAN : hex("#ffd23a");
        // o conteúdo vai numa imagem reta (recorte inclinado obrigaria o Qt a mascarar cada desenho) e entra inclinado
        if (m_panel.isNull()) m_panel = layer(c, PW, PH);
        m_panel.fill(Qt::transparent);
        {
            QPainter pq(&m_panel);
            Cv y(pq);
            y.lighter(); y.translate(PW / 2, -py);
            struct Pass { double off; QColor c; double a; };
            std::vector<Pass> passes;
            if (glitch) passes = { { -3, PINK, 0.5 }, { 3, CYAN, 0.5 } }; else passes = { { 0, col, 1 } };
            const QStringList sans = { QStringLiteral("IBM Plex Sans"), QStringLiteral("Arial") }, mono = { QStringLiteral("IBM Plex Mono"), QStringLiteral("Consolas") }, cjk = { QStringLiteral("Microsoft JhengHei"), QStringLiteral("MS Gothic"), QStringLiteral("Hiragino Sans"), QStringLiteral("PingFang SC") };
            for (const Pass& ps : passes) {
                y.save(); y.translate(ps.off + (glitch ? R(-6, 6) : 0), 0); y.alphaG(ps.a * on);
                const QColor c = ps.c;
                y.fillStyle(vgrad(py, py + PH, rgba(c, 0.10), rgba(c, 0.22))); y.fillRect(-PW / 2, py, PW, PH);
                if (scene == 0) {
                    y.fillStyle(rgba(c, 0.95)); y.setFont(26, 800, sans); y.align = -1; y.baseline = 1; y.fillText(QStringLiteral("NEO·SAKE"), -PW / 2 + 14, py + PH / 2 - 8);
                    y.setFont(10, 600, mono); y.fillStyle(rgba("#ffffff", 0.7)); y.fillText(QStringLiteral("TASTE THE FUTURE"), -PW / 2 + 16, py + PH / 2 + 16);
                    const double cw = 18 * std::abs(std::cos(m_t * 1.6)) + 3;
                    y.strokeStyle(rgba(c, 0.95)); y.lineWidth(1.5); y.strokeRect(PW / 2 - 36 - cw / 2, py + 22, cw, 48);
                    y.begin(); y.ellipse(PW / 2 - 36, py + 22, cw / 2, 3, 0, 0, TAU); y.stroke();
                }
                if (scene == 1) {
                    const double a0 = m_t * 0.9;
                    y.strokeStyle(rgba(c, 0.95)); y.fillStyle(rgba(c, 0.35)); y.lineWidth(1.5);
                    for (int k = 0; k < 2; ++k) {
                        const double a = a0 + k * M_PI, fx = std::cos(a) * 52, fy = py + PH / 2 + std::sin(a) * 18, dir = -std::sin(a) > 0 ? 1 : -1, sw = std::sin(m_t * 8) * 3;
                        y.save(); y.translate(fx, fy); y.scale(dir, 1);
                        y.begin(); y.moveTo(18, 0); y.quadTo(6, -9, -10, -3); y.lineTo(-20, -9 + sw); y.lineTo(-17, 0); y.lineTo(-20, 9 + sw); y.lineTo(-10, 3); y.quadTo(6, 9, 18, 0); y.fill(); y.stroke();
                        y.restore();
                    }
                    y.setFont(16, 700, cjk); y.align = 0; y.baseline = 1; y.fillStyle(rgba("#ffffff", 0.8)); y.fillText(QStringLiteral("鯉  KOI BANK"), 0, py + 14);
                }
                if (scene == 2) {
                    y.setFont(30, 700, cjk); y.align = 0; y.baseline = 1; y.fillStyle(rgba(c, 0.95)); y.fillText(QStringLiteral("夢を買え"), 0, py + PH / 2 - 6);
                    y.setFont(10, 600, mono); y.fillStyle(rgba("#ffffff", 0.7)); y.fillText(QStringLiteral("BUY A DREAM · 24/7"), 0, py + PH / 2 + 22);
                    for (int k = 0; k < 14; ++k) { const double ky = py + std::fmod(m_t * 30 + k * 23, PH); y.fillStyle(rgba(c, 0.25)); y.fillRect(-PW / 2 + 6 + k * 13, ky, 2, 6); }
                }
                y.restore();
            }
            y.fillStyle(rgba("#ffffff", 0.05)); for (double ly = py; ly < py + PH; ly += 2) y.fillRect(-PW / 2, ly, PW, 0.7);
            y.fillStyle(rgba(col, 0.25)); y.fillRect(-PW / 2, py + std::fmod(m_t * 40, PH), PW, 2);
        }
        x.image(m_panel, -PW / 2, py);
        x.strokeStyle(rgba(col, 0.8 * on)); x.lineWidth(1.5);
        const double CRN[4][4] = { { -PW / 2, py, 1, 1 }, { PW / 2, py, -1, 1 }, { PW / 2, py + PH, -1, -1 }, { -PW / 2, py + PH, 1, -1 } };
        for (const auto& k : CRN) { x.begin(); x.moveTo(k[0], k[1] + k[3] * 12); x.lineTo(k[0], k[1]); x.lineTo(k[0] + k[2] * 12, k[1]); x.stroke(); }
        glowAt(x, 0, py + PH / 2, 170, col, 0.08 * on);
        x.over(); x.restore();
    }

    // a pessoa sentada na mureta, de costas, capuz levantado, pernas penduradas do outro lado
    void drawSitter(Cv& x, const Pal& pal)
    {
        const double sx = m_sit.x(), sy = m_sit.y(), br = std::sin(m_t * 0.9) * 0.5;
        const QColor col = mix(pal.app, hex("#06050a"), 0.6), coat = mix(pal.app, hex("#120e18"), 0.55);
        x.save(); x.translate(sx, sy); x.scale(1.45, 1.45); x.translate(-sx, -sy);
        x.fillStyle(coat); x.begin(); x.moveTo(sx - 14, sy - 46 + br); x.quadTo(sx - 24, sy - 20, sx - 24, sy + 2); x.lineTo(sx + 22, sy + 2); x.quadTo(sx + 22, sy - 20, sx + 14, sy - 46 + br); x.close(); x.fill();
        x.fillStyle(col); x.begin(); x.moveTo(sx - 16, sy - 42 + br); x.quadTo(sx, sy - 50 + br, sx + 16, sy - 42 + br); x.lineTo(sx + 14, sy - 30); x.lineTo(sx - 14, sy - 30); x.fill();
        x.fillStyle(coat); x.begin(); x.moveTo(sx - 9, sy - 46 + br); x.quadTo(sx - 12, sy - 66 + br, sx + 1, sy - 70 + br); x.quadTo(sx + 13, sy - 66 + br, sx + 10, sy - 46 + br); x.close(); x.fill();
        const double puff = std::pow(qMax(0.0, std::sin(m_t * 0.35)), 6), hx = sx + 18 + puff * -10, hy = sy - 24 - puff * 34;
        x.strokeStyle(coat); x.lineWidth(6); x.roundCaps(); x.begin(); x.moveTo(sx + 14, sy - 40 + br); x.quadTo(sx + 24, sy - 28, hx, hy); x.stroke();
        m_ember = { sx + (hx + 3 - sx) * 1.45, sy + (hy - 1 - sy) * 1.45, puff };
        x.lighter();
        x.strokeStyle(rgba(PINK, 0.45)); x.lineWidth(1.4);
        x.begin(); x.moveTo(sx - 9, sy - 46 + br); x.quadTo(sx - 12, sy - 66 + br, sx + 1, sy - 70 + br); x.stroke();
        x.begin(); x.moveTo(sx - 16, sy - 42 + br); x.quadTo(sx - 24, sy - 20, sx - 24, sy + 1); x.stroke();
        x.strokeStyle(rgba(CYAN, 0.35)); x.begin(); x.moveTo(sx + 1, sy - 70 + br); x.quadTo(sx + 13, sy - 66 + br, sx + 10, sy - 46 + br); x.stroke();
        x.over(); x.restore();
    }

    QString m_key;
    QPixmap m_back, m_front, m_signLit, m_panel, m_beam;
    QPointF m_sit, m_sign, m_vent, m_antenna;
    std::vector<Blink> m_blinks;
    std::vector<Car> m_cars;
    std::vector<Drop> m_rain;
    std::vector<Splash> m_splashes;
    std::vector<Puff> m_smoke, m_steam;
    std::vector<Beam> m_beams;
    Cat m_cat;
    std::array<double, 3> m_ember { { 0, 0, 0 } };
    double m_t = 0, m_nextCar = 0.3, m_nextTrain = 2, m_trainX = 0, m_trainV = 150, m_railY = 480, m_signFlick = 0, m_blimpX = 210;
    bool m_train = false;
};


// ============================================================ Citadel
// A foto tirada rente à atmosfera: o planeta toma quase a mesa toda (pintado pixel a pixel, uma vez: continentes de
// ruído fractal, oceano, nuvens, luz do sol, reflexo no mar, a névoa azul da borda), e na frente, à esquerda, a
// estação em três quartos montada em 3D: anel habitacional com treliça, cubo, eixo com módulos, treliça, painéis
// solares cor de cobre, janela panorâmica com gente andando. Duas naves acoplam de lado pelo colar do dorso: chegam
// de trás da folha com os motores, freiam com os retrofoguetes do nariz, descem com os propulsores de manobra,
// ficam um tempo, desacoplam e vão embora acelerando com a chama longa e o rastro. Reflexos do sol no metal,
// o brilho correndo pelos painéis, as luzes de navegação, a lua bem longe no respiro de cima à direita. Só noite.
class Citadel : public Layer {
public:
    void init(Ctx&) override { m_key.clear(); }
    void step(double dt, Ctx&) override
    {
        m_t += dt;
        if (m_key.isEmpty()) return;
        for (Dock& d : m_docks) {
            ShipSt st;
            const V3 fw { -1, 0, 0 }, up = mul(d.axis, -1), rt = cross(fw, up);
            if (shipState(d, st) && st.eng > 0.2) {
                const int n = int(70 * st.eng * dt + rnd());
                for (int k = 0; k < n; ++k) {
                    const double sg = rnd() < 0.5 ? -1 : 1;
                    const V3 p0 = add(st.c3, add(mul(fw, -45), add(mul(rt, sg * 17), mul(up, -3))));
                    d.parts.push_back({ p0, add(mul(fw, -R(70, 140)), V3 { R(-8, 8), R(-8, 8), R(-8, 8) }), 0, R(0.5, 1.1) });
                }
            }
            for (Part& pa : d.parts) { pa.t += dt; pa.p = add(pa.p, mul(pa.v, dt)); }
            d.parts.erase(std::remove_if(d.parts.begin(), d.parts.end(), [](const Part& p) { return p.t >= p.life; }), d.parts.end());
        }
        for (Dock& d : m_docks) {
            d.t += dt;
            if (d.t < d.len) continue;
            d.t = 0; d.ph = (d.ph + 1) % 6; d.len = phaseLen(d.ph);
        }
        for (Walker& w : m_walkers) {
            if (w.wait > 0) { w.wait -= dt; continue; }
            w.u += w.v * dt;
            if (w.u < 0.04 || w.u > 0.96) { w.v *= -1; w.u = qBound(0.04, w.u, 0.96); w.wait = R(1, 4); }
            if (rnd() < dt * 0.1) w.wait = R(2, 6);
        }
    }
    void paint(QPainter& painter, const Pal& pal, Ctx& c) override
    {
        const QString key = sceneKey(pal, c);
        if (key != m_key) {
            Seeded sd(0xC17AD5u);
            build(pal, c);
            m_key = key;
        }
        Cv x(painter);
        x.image(m_back, 0, 0);
        x.image(m_hull, 0, 0);
        // o reflexo do sol correndo pelos painéis e as estrelas de reflexo da estação
        x.lighter();
        for (int i = 0; i < int(m_panels.size()); ++i) {
            const double u = std::fmod(m_t * 0.07 + i * 0.03, 1.7);
            if (u > 1.1) continue;
            const auto& q = m_panels[i];
            x.fillStyle(lin(q[0].x(), q[0].y(), q[3].x(), q[3].y(), { { qMax(0.0, u - 0.12), rgba("#fff0d8", 0) }, { qBound(0.0, u, 1.0), rgba("#fff0d8", 0.32) }, { qMin(1.0, u + 0.12), rgba("#fff0d8", 0) } }));
            quad(x, q[0], q[1], q[2], q[3]); x.fill();
        }
        for (const Glint& g : m_glints) starGlint(x, g.p.x(), g.p.y(), g.sz, std::pow(0.5 + 0.5 * std::sin(m_t * 0.9 + g.ph), 3) * 0.9);
        x.over();
        // a gente andando na janela panorâmica
        for (const Walker& w : m_walkers) {
            Fig o; o.pose = w.wait > 0 ? Fig::Stand : Fig::Walk; o.ph = m_t * 5 + w.ph; o.col = rgba("#2a1a10", 0.9); o.side = w.v > 0 ? 1 : -1;
            figure(painter, m_bandA.x() + (m_bandB.x() - m_bandA.x()) * w.u, m_bandA.y() + (m_bandB.y() - m_bandA.y()) * w.u, m_bandH, o);
        }
        // as naves, de trás pra frente
        std::vector<std::pair<double, const Dock*>> order;
        for (const Dock& d : m_docks) { ShipSt st; order.push_back({ shipState(d, st) ? cam(st.c3).z : 1e9, &d }); }
        std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        for (const auto& o : order) drawDock(x, painter, pal, *o.second);
        // as luzes de navegação
        x.lighter();
        for (int i = 0; i < int(m_navs.size()); ++i) {
            const Nav& n = m_navs[i];
            const double on = n.kind == 2 ? (std::fmod(m_t + i * 0.37, 1.6) < 0.08 ? 1 : 0) : (std::sin(m_t * 2.2 + i * 1.1) > 0.55 ? 1 : 0.12);
            if (on <= 0) continue;
            glowAt(x, n.p.x(), n.p.y(), n.kind == 2 ? 14 : 8, n.kind == 0 ? "#ff3a2a" : n.kind == 1 ? "#3aff8a" : "#ffffff", 0.9 * on);
        }
        x.over();
    }

private:
    struct V3 { double x, y, z; };
    struct SP { double x, y, s; QPointF q() const { return QPointF(x, y); } };
    struct Part { V3 p, v; double t, life; };
    struct Dock { V3 port, axis; QColor stripe; int ph; double t, len, phs; std::vector<Part> parts; };
    struct ShipSt { V3 c3 { 0, 0, 0 }; double eng = 0, retro = 0, rcsBelly = 0, rcsUp = 0, alpha = 1; };
    struct Walker { double u, v, ph, wait; };
    struct Nav { QPointF p; int kind; };
    struct Glint { QPointF p; double sz, ph; };
    struct Item { double z; std::function<void(Cv&)> f; };
    const QColor CY = hex("#5ac8ff");

    static V3 add(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    static V3 sub(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    static V3 mul(V3 a, double k) { return { a.x * k, a.y * k, a.z * k }; }
    static double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    static V3 norm(V3 a) { return mul(a, 1 / std::sqrt(qMax(1e-9, dot(a, a)))); }
    static V3 cross(V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
    static V3 LV() { static const V3 v = norm(V3 { -0.55, -0.62, -0.56 }); return v; }
    static V3 HV() { static const V3 v = norm(V3 { LV().x, LV().y, LV().z - 1 }); return v; }

    // a câmera: guinada 55°, mergulho 35° (três quartos, de cima), perspectiva suave
    static V3 cam(V3 p)
    {
        static const double ca = std::cos(55 * M_PI / 180), sa = std::sin(55 * M_PI / 180), cb = std::cos(35 * M_PI / 180), sb = std::sin(35 * M_PI / 180);
        const double x1 = p.x * ca + p.z * sa, z1 = -p.x * sa + p.z * ca, y1 = p.y;
        return { x1, y1 * cb - z1 * sb, y1 * sb + z1 * cb };
    }
    SP P(V3 p) const { const V3 q = cam(p); const double s = 1200 / (q.z + 900); return { m_O.x() + q.x * s, m_O.y() + q.y * s, s }; }
    static double lum(V3 n) { return 0.16 + 0.84 * qMax(0.0, dot(cam(n), LV())); }
    static double specW(V3 n) { return std::pow(qMax(0.0, dot(cam(n), HV())), 24); }
    double phaseLen(int ph) const { switch (ph) { case 0: return R(4, 12); case 1: return 11; case 2: return 6; case 3: return R(14, 24); case 4: return 6; default: return 9; } }

    static void quad(Cv& x, QPointF a, QPointF b, QPointF c, QPointF d) { x.begin(); x.moveTo(a.x(), a.y()); x.lineTo(b.x(), b.y()); x.lineTo(c.x(), c.y()); x.lineTo(d.x(), d.y()); x.close(); }
    // a estrela de reflexo: o sol batendo num ponto de metal (miolo + quatro raios finos)
    static void starGlint(Cv& x, double gx, double gy, double sz, double a)
    {
        if (a <= 0.01) return;
        glowAt(x, gx, gy, sz * 0.55, "#ffffff", a * 0.7); glowAt(x, gx, gy, sz * 0.2, "#ffffff", a);
        const double R4[4][3] = { { 1, 0, 1 }, { 0, 1, 0.75 }, { 0.7, 0.7, 0.35 }, { 0.7, -0.7, 0.35 } };
        x.lineWidth(1.1);
        for (const auto& r : R4) {
            const double L2 = sz * r[2];
            x.strokeStyle(lin(gx - r[0] * L2, gy - r[1] * L2, gx + r[0] * L2, gy + r[1] * L2, { { 0, rgba("#ffffff", 0) }, { 0.5, rgba("#ffffff", qMin(1.0, a * 0.9)) }, { 1, rgba("#ffffff", 0) } }));
            x.line(gx - r[0] * L2, gy - r[1] * L2, gx + r[0] * L2, gy + r[1] * L2);
        }
    }
    // o jato de exaustão em 3D: cone afunilando na direção do escape, miolo, borda colorida, brilho no bocal
    void plume(Cv& x, V3 b3, V3 dir3, double len, double w0, double k, const QColor& core, const QColor& outer) const
    {
        if (k <= 0.02) return;
        const SP a = P(b3), b = P(add(b3, mul(dir3, len)));
        const double dx = b.x - a.x, dy = b.y - a.y, L = qMax(0.001, std::hypot(dx, dy)), nx = -dy / L, ny = dx / L;
        const struct { double wk; QColor col; double al; } LY[2] = { { 1, outer, 0.5 }, { 0.42, core, 0.95 } };
        for (const auto& l : LY) {
            const double wa = w0 * l.wk * a.s, wb = w0 * l.wk * 0.22 * b.s;
            x.fillStyle(lin(a.x, a.y, b.x, b.y, { { 0, rgba(l.col, qMin(1.0, l.al * k)) }, { 0.3, rgba(l.col, qMin(1.0, l.al * k * 0.6)) }, { 1, rgba(l.col, 0) } }));
            x.begin(); x.moveTo(a.x + nx * wa, a.y + ny * wa); x.lineTo(b.x + nx * wb, b.y + ny * wb); x.lineTo(b.x - nx * wb, b.y - ny * wb); x.lineTo(a.x - nx * wa, a.y - ny * wa); x.close(); x.fill();
        }
        glowAt(x, a.x, a.y, w0 * 2.4 * a.s, outer, 0.6 * k); glowAt(x, a.x, a.y, w0 * 0.9 * a.s, core, 0.9 * k);
    }

    // um cilindro entre A e B (pode afunilar): gradiente de luz atravessando com o reflexo do sol, a tampa virada pra câmera
    void drawCyl(Cv& x, V3 A, V3 B, double r, const QColor& dk, const QColor& lt, double r2 = -1) const
    {
        const SP a = P(A), b = P(B);
        double dx = b.x - a.x, dy = b.y - a.y;
        const double len = std::hypot(dx, dy);
        if (len < 0.01) { dx = 1; dy = 0; } else { dx /= len; dy /= len; }
        const double px = -dy, py = dx, rA = r * a.s, rB = (r2 < 0 ? r : r2) * b.s, rm = (rA + rB) / 2;
        const V3 ax = norm(cam(sub(B, A))), L = LV(), Hh = HV();
        auto colK = [&](double k) {
            V3 n { px * k, py * k, -std::sqrt(qMax(0.0, 1 - k * k)) };
            n = norm(sub(n, mul(ax, dot(n, ax))));
            const double l = 0.16 + 0.84 * qMax(0.0, dot(n, L)), sp = std::pow(qMax(0.0, dot(n, Hh)), 28);
            return mix(mix(dk, lt, l), Qt::white, qMin(0.85, sp * 0.95));
        };
        const double mx = (a.x + b.x) / 2, my = (a.y + b.y) / 2;
        QLinearGradient g(mx - px * rm, my - py * rm, mx + px * rm, my + py * rm);
        for (int i = 0; i <= 10; ++i) g.setColorAt(i / 10.0, colK(-1 + i * 0.2));
        const bool nearB = ax.z < 0;
        const SP far = nearB ? a : b, nr = nearB ? b : a;
        const double rFar = nearB ? rA : rB, rNr = nearB ? rB : rA, rot = std::atan2(py, px), minor = std::abs(ax.z);
        x.fillStyle(g);
        x.begin(); x.ellipse(far.x, far.y, rFar, qMax(0.3, rFar * minor), rot, 0, TAU); x.fill();
        quad(x, QPointF(a.x + px * rA, a.y + py * rA), QPointF(b.x + px * rB, b.y + py * rB), QPointF(b.x - px * rB, b.y - py * rB), QPointF(a.x - px * rA, a.y - py * rA)); x.fill();
        x.fillStyle(mix(dk, lt, lum(norm(nearB ? sub(B, A) : sub(A, B))) * 0.9));
        x.begin(); x.ellipse(nr.x, nr.y, rNr, qMax(0.3, rNr * minor), rot, 0, TAU); x.fill();
        x.strokeStyle(rgba("#000000", 0.25)); x.lineWidth(0.7); x.stroke();
    }

    // ---------- o espaço, o sol, a lua e o planeta (pintado pixel a pixel)
    void space(Cv& x, const Pal& pal, const Ctx& c) const
    {
        const double W = c.W, H = c.H;
        x.fillStyle(vgrad(0, H, mix(pal.app, hex("#02040a"), 0.6), mix(pal.app, hex("#060a16"), 0.55))); x.fillRect(0, 0, W, H);
        const QPixmap nb = softDot(hex("#4a3a98"), 0.2), nb2 = softDot(hex("#2a6a98"), 0.2);
        for (int i = 0; i < 70; ++i) { const double t = rnd(), s = R(80, 220); x.alphaG(0.03); x.image(i % 2 ? nb : nb2, t * W - s / 2, 40 + std::sin(t * 4) * 60 + R(-40, 40) - s / 2, s, s); }
        x.alphaG(1);
        const int nStars = int(1300 * W / 1440.0);
        for (int i = 0; i < nStars; ++i) { const double s = rnd() < 0.04 ? R(1.3, 2) : R(0.5, 1.1); x.fillStyle(rgba(pickHex({ "#ffffff", "#c8dcff", "#ffe8d0" }), R(0.15, 0.8))); x.fillRect(R(0, W), R(0, H), s, s); }
        // o sol entrando pelo canto de cima à esquerda, com fantasmas de lente bem leves
        glowAt(x, -40, -60, 760, "#fff2dc", 0.2); glowAt(x, -40, -60, 220, "#ffffff", 0.4);
        x.lighter();
        const struct { double t, r; const char* c; double a; } GH[3] = { { 0.32, 26, "#a8d8ff", 0.05 }, { 0.5, 12, "#c8ffd8", 0.06 }, { 0.74, 46, "#a8c8ff", 0.035 } };
        for (const auto& g : GH) glowAt(x, -40 + (W * 0.55 + 40) * g.t, -60 + (H * 0.7 + 60) * g.t, g.r, g.c, g.a);
        x.over();
        // a lua, bem longe: pequena, sombreada como esfera
        {
            const double mx = W - 150, my = 72, mr = 9;
            glowAt(x, mx, my, 34, "#c8d0e0", 0.06);
            x.save(); x.begin(); x.arc(mx, my, mr, 0, TAU); x.clip();
            x.fillStyle(hex("#23262c")); x.fillRect(mx - mr, my - mr, mr * 2, mr * 2);
            x.fillStyle(rad(mx - mr * 0.45, my - mr * 0.4, 0, mx - mr * 0.2, my - mr * 0.2, mr * 1.15, { { 0, hex("#d4d6da") }, { 0.6, hex("#9a9ea6") }, { 1, rgba("#9a9ea6", 0) } }));
            x.fillRect(mx - mr, my - mr, mr * 2, mr * 2);
            for (int k = 0; k < 6; ++k) { x.fillStyle(rgba("#000000", 0.18)); x.begin(); x.arc(mx + R(-mr, mr) * 0.7, my + R(-mr, mr) * 0.7, R(0.8, 2), 0, TAU); x.fill(); }
            x.restore();
        }
        // o planeta, rente à atmosfera
        const double PCX = zx(c, 900), PCY = 1050, PR = 930, TILT = -0.38;
        const V3 SUNV = norm(V3 { -0.62, -0.5, 0.6 });
        x.fillStyle(rad(PCX, PCY, PR - 6, PCX, PCY, PR + 120, { { 0, rgba("#d8f0ff", 0.55) }, { 0.05, rgba("#9ad0ff", 0.34) }, { 0.18, rgba("#6aa8f0", 0.15) }, { 0.5, rgba("#4a88e0", 0.05) }, { 1, rgba("#4a88e0", 0) } }));
        x.begin(); x.arc(PCX, PCY, PR + 120, 0, TAU); x.fill();
        paintPlanet(x, W, H, PCX, PCY, PR, TILT, SUNV);
        // a borda acesa do lado do sol, as camadas da atmosfera e o filete quente do horizonte
        {
            const double gx0 = PCX + SUNV.x * PR, gy0 = PCY + SUNV.y * PR, gx1 = PCX - SUNV.x * PR, gy1 = PCY - SUNV.y * PR;
            x.strokeStyle(lin(gx0, gy0, gx1, gy1, { { 0, rgba("#bfe4ff", 0.85) }, { 0.55, rgba("#6aa8e8", 0.25) }, { 0.8, rgba("#6aa8e8", 0) } }));
            x.lineWidth(2.5); x.begin(); x.arc(PCX, PCY, PR + 0.5, M_PI, 2 * M_PI); x.stroke();
            x.lighter();
            const double LW[3][2] = { { 34, 0.12 }, { 14, 0.2 }, { 5, 0.3 } };
            for (const auto& l : LW) {
                x.strokeStyle(lin(gx0, gy0, gx1, gy1, { { 0, rgba("#cfeaff", l[1]) }, { 0.6, rgba("#7ab8f0", l[1] * 0.35) }, { 0.85, rgba("#7ab8f0", 0) } }));
                x.lineWidth(l[0]); x.begin(); x.arc(PCX, PCY, PR + l[0] * 0.35, M_PI, 2 * M_PI); x.stroke();
            }
            x.strokeStyle(rgba("#ffb890", 0.18)); x.lineWidth(2); x.begin(); x.arc(PCX, PCY, PR + 4, M_PI * 1.05, M_PI * 1.95); x.stroke();
            x.over();
        }
    }

    // a superfície: ruído de valor 3D (fbm) com distorção de domínio; cor por altitude e latitude, oceano raso e fundo,
    // nuvens, luz lambert, reflexo no mar, cidades no escuro, a névoa de fresnel na borda. Dividido entre os núcleos.
    static void paintPlanet(Cv& x, double W, double H, double PCX, double PCY, double PR, double TILT, V3 SUNV)
    {
        std::array<uint8_t, 512> perm;
        {
            std::array<int, 256> pp;
            for (int k = 0; k < 256; ++k) pp[k] = k;
            for (int k = 255; k > 0; --k) { const int r2 = qMin(k, int(rnd() * (k + 1))); std::swap(pp[k], pp[r2]); }
            for (int k = 0; k < 512; ++k) perm[k] = uint8_t(pp[k & 255]);
        }
        const double SC = 0.8;
        const int yTop = qMax(0, int(std::floor((PCY - PR) * SC))), cw = int(std::ceil(W * SC)), chh = qMax(1, int(std::ceil(H * SC)) - yTop);
        QImage img(cw, chh, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        uchar* const bits = img.bits();          // pega o ponteiro antes das threads (scanLine() pode querer separar a imagem)
        const qsizetype bpl = img.bytesPerLine();
        auto rows = [&](int r0, int r1) {
            auto hsh = [&](int i, int j, int k) { return perm[(perm[(perm[i & 255] + (j & 255)) & 511] + (k & 255)) & 511] * (1.0 / 255); };
            auto vn = [&](double x0, double y0, double z0) {
                const double fx = std::floor(x0), fy = std::floor(y0), fz = std::floor(z0);
                const int xi = int(fx), yi = int(fy), zi = int(fz);
                const double xf = x0 - fx, yf = y0 - fy, zf = z0 - fz, u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf), w = zf * zf * (3 - 2 * zf);
                auto l1 = [](double a, double b, double t) { return a + (b - a) * t; };
                return l1(l1(l1(hsh(xi, yi, zi), hsh(xi + 1, yi, zi), u), l1(hsh(xi, yi + 1, zi), hsh(xi + 1, yi + 1, zi), u), v),
                          l1(l1(hsh(xi, yi, zi + 1), hsh(xi + 1, yi, zi + 1), u), l1(hsh(xi, yi + 1, zi + 1), hsh(xi + 1, yi + 1, zi + 1), u), v), w);
            };
            auto fbm = [&](double x0, double y0, double z0, int oct) {
                double sm = 0, am = 0.5, fq = 1, nm = 0;
                for (int o = 0; o < oct; ++o) { sm += am * vn(x0 * fq + o * 17.3, y0 * fq + o * 9.1, z0 * fq + o * 5.7); nm += am; fq *= 2.07; am *= 0.5; }
                return sm / nm;
            };
            auto sst = [](double e0, double e1, double v) { const double t = qBound(0.0, (v - e0) / (e1 - e0), 1.0); return t * t * (3 - 2 * t); };
            struct C3 { double r, g, b; };
            auto mixc = [](C3 a, C3 b, double t) { return C3 { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; };
            const C3 GREEN { 58, 88, 42 }, OLIVE { 104, 104, 60 }, DRY { 156, 134, 90 }, ROCK { 112, 104, 92 }, SNOW { 232, 238, 244 }, SHALLOW { 40, 118, 172 }, DEEP { 10, 46, 96 }, CLOUD { 246, 249, 252 }, AIR { 150, 202, 255 };
            const double PITCH = -0.6, cpi = std::cos(PITCH), spi = std::sin(PITCH), ct = std::cos(TILT), st = std::sin(TILT), S0 = SUNV.x, S1 = SUNV.y, S2 = SUNV.z;
            const double hl = std::sqrt(S0 * S0 + S1 * S1 + (S2 + 1) * (S2 + 1)), H0 = S0 / hl, H1 = S1 / hl, H2 = (S2 + 1) / hl;
            for (int py = r0; py < r1; ++py) {
                const double Y = (py + yTop + 0.5) / SC, dy = (Y - PCY) / PR;
                QRgb* line = reinterpret_cast<QRgb*>(bits + py * bpl);
                for (int px = 0; px < cw; ++px) {
                    const double X = (px + 0.5) / SC, dx = (X - PCX) / PR, r2 = dx * dx + dy * dy;
                    if (r2 >= 1) continue;
                    const double vz = std::sqrt(1 - r2), bx = dx * ct + dy * st, by0 = -dx * st + dy * ct, by = by0 * cpi - vz * spi, bz = by0 * spi + vz * cpi;
                    const double qx = bx + (fbm(bx * 1.5 + 11, by * 1.5, bz * 1.5, 3) - 0.5) * 0.9, qy = by + (fbm(bx * 1.5, by * 1.5 + 23, bz * 1.5, 3) - 0.5) * 0.9, qz = bz + (fbm(bx * 1.5, by * 1.5, bz * 1.5 + 37, 3) - 0.5) * 0.9;
                    const double h = fbm(qx * 1.35, qy * 1.35, qz * 1.35, 7) - 0.507, lat = std::abs(by) + (fbm(qx * 4, qy * 4, qz * 4, 3) - 0.5) * 0.14;
                    const bool land = h > 0;
                    C3 col;
                    if (land) {
                        const double e = qMin(1.0, h / 0.2), det = fbm(qx * 9, qy * 9, qz * 9, 3);
                        col = mixc(GREEN, OLIVE, sst(0.35, 0.65, det)); col = mixc(col, DRY, sst(0.45, 0.8, det * 0.6 + e * 0.5 + (1 - lat) * 0.15) * 0.8); col = mixc(col, ROCK, sst(0.55, 0.9, e) * 0.7);
                        col = mixc(col, SNOW, sst(0.72, 0.86, lat + e * 0.25)); col = mixc(col, C3 { col.r * 0.8, col.g * 0.85, col.b * 0.8 }, fbm(qx * 22, qy * 22, qz * 22, 2) * 0.6);
                    } else { col = mixc(SHALLOW, DEEP, sst(0, 0.12, -h)); col = mixc(col, SNOW, sst(0.86, 0.95, lat) * 0.85); }
                    const double c1 = fbm(qx * 2.4 + 5, qy * 3.1, qz * 2.4, 6), c2 = fbm(bx * 8 + 3, by * 11, bz * 8, 3);
                    const double cloud = qMin(1.0, sst(0.478, 0.61, c1) * (0.5 + 0.6 * c2) + sst(0.6, 0.74, c2) * 0.45 * sst(0.42, 0.56, c1));
                    const double lam = qMax(0.0, dx * S0 + dy * S1 + vz * S2), diff = 0.1 + 0.9 * lam, sh = diff * (1 - cloud * 0.25);
                    col = { col.r * sh, col.g * sh, col.b * sh };
                    if (!land) { const double sp = std::pow(qMax(0.0, dx * H0 + dy * H1 + vz * H2), 70) * 0.55 * (1 - cloud); col = { col.r + 255 * sp, col.g + 240 * sp, col.b + 220 * sp }; }
                    col = mixc(col, C3 { CLOUD.r * (0.2 + 0.8 * lam), CLOUD.g * (0.2 + 0.8 * lam), CLOUD.b * (0.22 + 0.78 * lam) }, cloud * 0.95);
                    if (land && lam < 0.08 && cloud < 0.4) { const double ci = sst(0.7, 0.85, fbm(qx * 40, qy * 40, qz * 40, 2)) * (1 - lam / 0.08); col = { col.r + 255 * ci, col.g + 170 * ci, col.b + 80 * ci }; }
                    const double fres = std::pow(1 - vz, 2.4);
                    col = mixc(col, C3 { AIR.r * (0.25 + 0.75 * lam), AIR.g * (0.25 + 0.75 * lam), AIR.b * (0.3 + 0.7 * lam) }, qMin(0.9, fres * 1.1));
                    const double a = qMin(1.0, (1 - std::sqrt(r2)) * PR * SC * 1.5);
                    line[px] = qRgba(int(qMin(255.0, col.r) * a), int(qMin(255.0, col.g) * a), int(qMin(255.0, col.b) * a), int(255 * a));
                }
            }
        };
        const int nT = qBound(1, int(std::thread::hardware_concurrency()), 8);
        std::vector<std::thread> th;
        for (int i = 0; i < nT; ++i) th.emplace_back(rows, chh * i / nT, chh * (i + 1) / nT);
        for (std::thread& t : th) t.join();
        x.p.save();
        x.p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        x.p.drawImage(QRectF(0, yTop / SC, cw / SC, chh / SC), img);
        x.p.restore();
    }

    // ---------- a estação
    void build(const Pal& pal, const Ctx& c)
    {
        const double W = c.W, H = c.H;
        m_O = QPointF(zx(c, 215), 270);
        m_back = layer(c, W, H);
        { QPainter qp(&m_back); Cv x(qp); space(x, pal, c); }
        const QColor dk = mix(pal.app, hex("#141a24"), 0.5), lt = mix(pal.app, hex("#e4eaf2"), 0.5), dk2 = mix(dk, Qt::black, 0.3), lt2 = mix(pal.app, hex("#b8c2d0"), 0.45);
        std::vector<Item> items;
        auto zOf = [](V3 q) { return cam(q).z; };
        auto cyl = [&](V3 A, V3 B, double r, double bias, QColor d0, QColor l0, std::function<void(Cv&)> extra = nullptr) {
            items.push_back({ (zOf(A) + zOf(B)) / 2 + bias, [=](Cv& x) { drawCyl(x, A, B, r, d0, l0); if (extra) extra(x); } });
        };
        auto lit = [this](Cv& x, V3 q3, double w, double h, bool warm) {
            const SP q = P(q3);
            x.fillStyle(rgba(warm ? "#ffd8a0" : "#c8e0ff", 0.85)); x.fillRect(q.x - w * q.s / 2, q.y - h * q.s / 2, w * q.s, h * q.s);
        };
        m_navs.clear(); m_panels.clear(); m_glints.clear();
        // o anel habitacional: faixa larga com a borda de estrutura e a treliça por dentro
        {
            const int N = 120;
            const double Rr = 130, hw = 10, th = 8;
            auto Q = [](double xx, double t, double rr) { return V3 { xx, rr * std::cos(t), rr * std::sin(t) }; };
            for (int k = 0; k < N; ++k) {
                const double t0 = k * TAU / N, t1 = (k + 1) * TAU / N, tm = (t0 + t1) / 2;
                const V3 n { 0, std::cos(tm), std::sin(tm) };
                const bool outer = cam(n).z < 0, win1 = rnd() < 0.55, win2 = rnd() < 0.35, warm = rnd() < 0.7;
                items.push_back({ zOf(Q(0, tm, Rr)), [=](Cv& x) {
                    x.fillStyle(mix(dk, lt, lum(V3 { 1, 0, 0 }) * 0.85)); quad(x, P(Q(hw, t0, Rr)).q(), P(Q(hw, t1, Rr)).q(), P(Q(hw, t1, Rr - th)).q(), P(Q(hw, t0, Rr - th)).q()); x.fill();
                    const double rr = outer ? Rr : Rr - th;
                    const SP b0 = P(Q(-hw, t0, rr)), b1 = P(Q(hw, t0, rr));
                    const V3 nn = outer ? n : mul(n, -1);
                    x.fillStyle(mix(mix(dk, lt, outer ? lum(nn) : lum(nn) * 0.8), Qt::white, qMin(0.7, specW(nn) * 0.8)));
                    quad(x, b0.q(), b1.q(), P(Q(hw, t1, rr)).q(), P(Q(-hw, t1, rr)).q()); x.fill();
                    if (k % 6 == 0) { x.strokeStyle(rgba("#000000", 0.3)); x.lineWidth(0.6); x.line(b0.x, b0.y, b1.x, b1.y); }
                    if (win1) lit(x, Q(-3, tm, rr + (outer ? 0.2 : -0.2)), 2.2, 1.4, warm);
                    if (win2) lit(x, Q(4, tm, rr + (outer ? 0.2 : -0.2)), 2.2, 1.4, !warm);
                    const double sc = P(Q(0, tm, 110)).s;
                    const SP r0 = P(Q(0, t0, 108)), r1 = P(Q(0, t1, 108)), e0 = P(Q(k % 2 ? hw : -hw, t0, Rr - th));
                    x.strokeStyle(mix(dk, lt2, 0.45)); x.lineWidth(qMax(0.6, 0.9 * sc));
                    x.begin(); x.moveTo(r0.x, r0.y); x.lineTo(r1.x, r1.y); x.moveTo(e0.x, e0.y); x.lineTo(r1.x, r1.y); x.stroke();
                } });
            }
            for (double ang : { M_PI / 4, 3 * M_PI / 4, 5 * M_PI / 4, 7 * M_PI / 4 })
                cyl(V3 { 0, 26 * std::cos(ang), 26 * std::sin(ang) }, V3 { 0, 112 * std::cos(ang), 112 * std::sin(ang) }, 2.6, 0, dk, lt2);
            {   // os pontos do anel que mais pegam sol viram estrelas de reflexo
                std::vector<std::pair<double, double>> cand;
                for (int k = 0; k < 72; ++k) { const double t = k / 72.0 * TAU; const V3 n { 0, std::cos(t), std::sin(t) }; if (cam(n).z < 0) cand.push_back({ specW(n), t }); }
                std::sort(cand.begin(), cand.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
                for (int i = 0; i < qMin(2, int(cand.size())); ++i) { const SP q = P(Q(hw * 0.4, cand[i].second, Rr + 0.5)); m_glints.push_back({ q.q(), 20 * q.s, R(0, 6.28) }); }
                { const SP q = P(V3 { -12, -27, -6 }); m_glints.push_back({ q.q(), 14 * q.s, R(0, 6.28) }); }
                { const SP q = P(V3 { 96, -15, -15 }); m_glints.push_back({ q.q(), 16 * q.s, R(0, 6.28) }); }
            }
            m_navs.push_back({ P(V3 { hw, -Rr - 2, 0 }).q(), 0 }); m_navs.push_back({ P(V3 { hw, Rr + 2, 0 }).q(), 1 });
            m_navs.push_back({ P(V3 { hw, 0, -Rr - 2 }).q(), 2 }); m_navs.push_back({ P(V3 { hw, 0, Rr + 2 }).q(), 2 });
        }
        // o cubo no centro do anel, com flanges e a antena
        {
            std::vector<std::pair<V3, bool>> wins;
            for (double xx = -40; xx <= 40; xx += 8) for (double t : { 0.6, 0.0, -0.5 }) if (rnd() < 0.5) wins.push_back({ V3 { xx, -std::sin(t) * 24.3, -std::cos(t) * 24.3 }, true });
            cyl(V3 { -46, 0, 0 }, V3 { 46, 0, 0 }, 24, 0, dk, lt, [=](Cv& x) { for (const auto& w : wins) lit(x, w.first, 1.8, 1.3, true); });
        }
        for (double fx : { -30.0, 0.0, 30.0 }) cyl(V3 { fx - 2.5, 0, 0 }, V3 { fx + 2.5, 0, 0 }, 28, -0.5 - fx * 0.001, dk2, lt2);
        cyl(V3 { -20, -24, 0 }, V3 { -20, -44, 0 }, 1.4, -1, dk, lt2);
        items.push_back({ zOf(V3 { -20, -46, 0 }) - 1, [=](Cv& x) {
            const SP q = P(V3 { -20, -47, 0 });
            x.fillStyle(mix(dk, lt, 0.8)); x.begin(); x.ellipse(q.x, q.y, 11 * q.s, 4.5 * q.s, -0.35, 0, TAU); x.fill();
            x.fillStyle(mix(dk, lt, 0.35)); x.begin(); x.ellipse(q.x + q.s, q.y + 0.6 * q.s, 7.5 * q.s, 2.8 * q.s, -0.35, 0, TAU); x.fill();
        } });
        // o eixo e os módulos
        cyl(V3 { 46, 0, 0 }, V3 { 250, 0, 0 }, 7, 0.5, dk, lt2);
        {
            std::vector<std::pair<V3, bool>> wins;
            for (double xx = 74; xx <= 122; xx += 4.5)
                for (double t : { 1.0, 0.5, 0.0, -0.5 }) { if (std::abs(xx - 100) < 8 && t == 0.0) continue; if (rnd() < 0.55) wins.push_back({ V3 { xx, -std::sin(t) * 22.3, -std::cos(t) * 22.3 }, rnd() < 0.75 }); }
            cyl(V3 { 70, 0, 0 }, V3 { 125, 0, 0 }, 22, 0, dk, lt, [=](Cv& x) { for (const auto& w : wins) lit(x, w.first, 1.8, 1.3, w.second); });
        }
        for (double sx : { 82.0, 112.0 }) cyl(V3 { sx - 1, 0, 0 }, V3 { sx + 1, 0, 0 }, 22.8, -0.3, dk2, dk);
        // a treliça entre os módulos
        items.push_back({ zOf(V3 { 148, 0, 0 }), [=](Cv& x) {
            const double s = P(V3 { 148, 0, 0 }).s, C4[4][2] = { { -9, -9 }, { 9, -9 }, { 9, 9 }, { -9, 9 } };
            x.strokeStyle(mix(dk, lt2, 0.6)); x.lineWidth(qMax(0.7, 1.1 * s)); x.begin();
            for (const auto& q : C4) { const SP a = P(V3 { 125, q[0], q[1] }), b = P(V3 { 170, q[0], q[1] }); x.moveTo(a.x, a.y); x.lineTo(b.x, b.y); }
            for (int k = 0; k < 4; ++k) {
                const double x0 = 125 + k * 11.25, x1 = x0 + 11.25;
                for (int e = 0; e < 4; ++e) { const auto& q0 = C4[e]; const auto& q1 = C4[(e + 1) % 4]; const SP a = P(V3 { k % 2 ? x0 : x1, q0[0], q0[1] }), b = P(V3 { k % 2 ? x1 : x0, q1[0], q1[1] }); x.moveTo(a.x, a.y); x.lineTo(b.x, b.y); }
            }
            x.stroke();
        } });
        auto W3 = [](double xx, double th) { return V3 { xx, -std::sin(th) * 17.3, -std::cos(th) * 17.3 }; };
        cyl(V3 { 170, 0, 0 }, V3 { 215, 0, 0 }, 17, 0, dk, lt, [=](Cv& x) {
            // a janela panorâmica no alto-frente do módulo, com o vidro aceso
            const SP a0 = P(W3(174, 1.0)), a1 = P(W3(211, 1.0)), a2 = P(W3(211, 0.55)), a3 = P(W3(174, 0.55));
            x.fillStyle(vgrad(a0.y, a3.y, hex("#ffe4b8"), hex("#d8904a"))); quad(x, a0.q(), a1.q(), a2.q(), a3.q()); x.fill();
            x.strokeStyle(mix(dk, lt2, 0.3)); x.lineWidth(0.8); x.begin();
            for (double xx = 180; xx < 211; xx += 6) { const SP p0 = P(W3(xx, 1.0)), p1 = P(W3(xx, 0.55)); x.moveTo(p0.x, p0.y); x.lineTo(p1.x, p1.y); }
            x.stroke();
        });
        { const SP a = P(W3(176, 0.57)), b = P(W3(209, 0.57)), top = P(W3(176, 1.0)); m_bandA = a.q(); m_bandB = b.q(); m_bandH = std::abs(a.y - top.y) * 0.85; }
        {
            std::vector<V3> wins;
            for (double xx = 242; xx <= 268; xx += 4) for (double t : { 0.8, 0.3 }) if (rnd() < 0.5) wins.push_back(V3 { xx, -std::sin(t) * 14.3, -std::cos(t) * 14.3 });
            cyl(V3 { 238, 0, 0 }, V3 { 272, 0, 0 }, 14, 0, dk, lt, [=](Cv& x) { for (const V3& w : wins) lit(x, w, 1.6, 1.2, true); });
        }
        cyl(V3 { 272, 0, 0 }, V3 { 282, 0, 0 }, 6, -0.2, dk, lt2);
        items.push_back({ zOf(V3 { 290, 0, 0 }), [=](Cv& x) { const SP a = P(V3 { 282, 0, 0 }), b = P(V3 { 304, 0, 0 }); x.strokeStyle(mix(dk, lt2, 0.6)); x.lineWidth(1); x.line(a.x, a.y, b.x, b.y); } });
        m_navs.push_back({ P(V3 { 304, 0, 0 }).q(), 2 });
        // os painéis solares: dois pares, cobre, com a grade e o reflexo do sol
        const QColor cu0 = mix(pal.app, hex("#2a1810"), 0.4), cu1 = mix(pal.app, hex("#d8885a"), 0.45);
        for (double pxs : { 148.0, 232.0 }) {
            cyl(V3 { pxs, -152, 0 }, V3 { pxs, 152, 0 }, 1.6, 0, dk, lt2);
            for (int sg : { -1, 1 })
                for (int half = 0; half < 2; ++half) {
                    const double xa = pxs - 13 + half * 14, xb = xa + 12, y0 = sg * 24, y1 = sg * 150;
                    items.push_back({ zOf(V3 { (xa + xb) / 2, (y0 + y1) / 2, 0 }), [=](Cv& x) {
                        const QPointF a = P(V3 { xa, y0, 0 }).q(), b = P(V3 { xb, y0, 0 }).q(), c2 = P(V3 { xb, y1, 0 }).q(), d = P(V3 { xa, y1, 0 }).q();
                        const double l = lum(V3 { 0, 0, -1 });
                        x.fillStyle(lin(a.x(), a.y(), d.x(), d.y(), { { 0, mix(cu0, cu1, l) }, { 0.5, mix(cu0, cu1, l * 0.7) }, { 1, mix(cu0, cu1, l * 0.9) } }));
                        quad(x, a, b, c2, d); x.fill();
                        x.strokeStyle(rgba("#000000", 0.35)); x.lineWidth(0.6); x.begin();
                        for (int k = 1; k < 16; ++k) { const double u = k / 16.0; const QPointF p0 = a + (d - a) * u, p1 = b + (c2 - b) * u; x.moveTo(p0.x(), p0.y()); x.lineTo(p1.x(), p1.y()); }
                        { const QPointF p0 = (a + b) / 2, p1 = (d + c2) / 2; x.moveTo(p0.x(), p0.y()); x.lineTo(p1.x(), p1.y()); }
                        x.stroke();
                        x.strokeStyle(mix(dk, lt2, 0.5)); x.lineWidth(0.8); quad(x, a, b, c2, d); x.stroke();
                        x.lighter(); x.fillStyle(lin(a.x(), a.y(), d.x(), d.y(), { { 0, rgba("#ffd8a8", 0) }, { 0.35, rgba("#ffd8a8", 0.16) }, { 0.5, rgba("#ffd8a8", 0) } }));
                        quad(x, a, b, c2, d); x.fill(); x.over();
                    } });
                    m_panels.push_back({ P(V3 { xa, y0, 0 }).q(), P(V3 { xb, y0, 0 }).q(), P(V3 { xb, y1, 0 }).q(), P(V3 { xa, y1, 0 }).q() });
                }
            m_navs.push_back({ P(V3 { pxs, -153, 0 }).q(), 0 }); m_navs.push_back({ P(V3 { pxs, 153, 0 }).q(), 1 });
        }
        // os portos de acoplamento: dois, com o anel do colar (a nave encaixa de lado, pelo colar no dorso)
        m_docks.clear();
        const struct { V3 base, ax; const char* stripe; } PORTS[2] = { { { 100, 0, -21 }, { 0, 0, -1 }, "#e8a040" }, { { 192, 0, -16 }, { 0, 0, -1 }, "#d84a3a" } };
        for (int i = 0; i < 2; ++i) {
            const V3 end = add(PORTS[i].base, mul(PORTS[i].ax, 8));
            cyl(PORTS[i].base, end, 6.5, -0.6, dk, lt);
            cyl(add(PORTS[i].base, mul(PORTS[i].ax, 5.5)), end, 8, -0.7, dk2, lt2);
            Dock d { end, PORTS[i].ax, hex(PORTS[i].stripe), 0, 0, R(3, 6), R(0, 6.28), {} };
            if (i == 0) { d.ph = 3; d.len = R(16, 22); d.t = R(0, 6); } else { d.ph = 1; d.len = 11; d.t = 5; }
            m_docks.push_back(d);
        }
        // desenha tudo de trás pra frente
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.z > b.z; });
        m_hull = layer(c, W, H);
        { QPainter qp(&m_hull); Cv x(qp); for (const Item& it : items) it.f(x); }
        m_walkers.clear();
        for (int i = 0; i < 4; ++i) m_walkers.push_back({ R(0.1, 0.9), R(0.02, 0.05) * (rnd() < 0.5 ? 1 : -1), R(0, 6.28), R(0, 4) });
    }

    // ---------- as naves
    bool shipState(const Dock& d, ShipSt& st) const
    {
        if (d.ph == 0) return false;
        const double u = d.len > 0 ? qBound(0.0, d.t / d.len, 1.0) : 0, Hh = 70, back = 125;
        auto sm = [](double k) { return k * k * (3 - 2 * k); };
        auto pulse = [&](double rate, double duty) { return std::fmod(m_t * rate + d.phs, 1.0) < duty ? 1.0 : 0.0; };
        const V3 fw { -1, 0, 0 }, dock = add(d.port, mul(d.axis, 14));
        st = ShipSt();
        if (d.ph == 1) {          // chega com os motores, depois freia com os retrofoguetes do nariz
            const double e = 1 - std::pow(1 - u, 2.6);
            st.c3 = add(add(dock, mul(d.axis, Hh)), mul(fw, -(1 - e) * 700));
            st.eng = u < 0.42 ? 0.7 : qMax(0.0, 0.7 - (u - 0.42) * 8);
            st.retro = u > 0.4 && u < 0.95 ? std::sin(M_PI * (u - 0.4) / 0.55) : 0;
            if (u > 0.9) st.rcsBelly = pulse(3, 0.25) * 0.8;
        } else if (d.ph == 2) {   // desce de lado: empurra no começo, freia no fim
            st.c3 = add(dock, mul(d.axis, Hh * (1 - sm(u))));
            if (u < 0.3) st.rcsUp = pulse(2.5, 0.35); else if (u > 0.68) st.rcsBelly = pulse(3, 0.3);
        } else if (d.ph == 3) {
            st.c3 = dock;
        } else if (d.ph == 4) {   // desacopla: empurra pra longe da estação, freia no fim
            st.c3 = add(dock, mul(d.axis, back * sm(u)));
            if (u < 0.35) st.rcsBelly = pulse(2.5, 0.4); else if (u > 0.72) st.rcsUp = pulse(3, 0.3);
        } else {                  // vai embora acelerando
            st.c3 = add(add(dock, mul(d.axis, back)), mul(fw, u * u * 1100));
            st.eng = qMin(1.0, 0.45 + u * 3);
            st.alpha = u > 0.65 ? qMax(0.0, 1 - (u - 0.65) / 0.35) : 1;
        }
        return true;
    }

    void drawDock(Cv& x, QPainter& painter, const Pal& pal, const Dock& d) const
    {
        Q_UNUSED(painter);
        {   // a luz do porto: âmbar piscando na chegada, verde acoplada
            const SP q = P(add(d.port, mul(d.axis, 0.5)));
            x.lighter();
            if (d.ph == 1 || d.ph == 2) glowAt(x, q.x, q.y, 10 * q.s, "#ffb050", std::sin(m_t * 6) > 0 ? 0.9 : 0.15);
            else if (d.ph == 3) glowAt(x, q.x, q.y, 10 * q.s, "#3aff8a", 0.8);
            else glowAt(x, q.x, q.y, 6 * q.s, "#c8e0ff", 0.35);
            x.over();
        }
        ShipSt st;
        if (!shipState(d, st)) return;
        // o rastro de partículas do escape, ficando pra trás
        x.lighter();
        for (const Part& pa : d.parts) { const SP q = P(pa.p); const double k = pa.t / pa.life; glowAt(x, q.x, q.y, (2 + k * 7) * q.s, k < 0.3 ? hex("#e8f8ff") : CY, (1 - k) * 0.32 * st.alpha); }
        x.over();
        drawShip(x, pal, st, mul(d.axis, -1), d.stripe, d.phs);
    }

    // a nave em 3D: casco, nariz com a cabine, asas, deriva, duas naceles, faixa colorida, janelas, colar no dorso, luzes
    void drawShip(Cv& x, const Pal& pal, const ShipSt& st, V3 up, const QColor& stripe, double phs) const
    {
        const V3 fw { -1, 0, 0 }, rt = cross(fw, up), c3 = st.c3;
        auto at = [&](double f2, double r2, double u2) { return add(c3, add(mul(fw, f2), add(mul(rt, r2), mul(up, u2)))); };
        const QColor dk = mix(pal.app, hex("#161c26"), 0.5), lt = mix(pal.app, hex("#eef2f6"), 0.55), dk2 = mix(dk, Qt::black, 0.3), lt2 = mix(pal.app, hex("#b8c2d0"), 0.45);
        const QColor sdk = mix(stripe, Qt::black, 0.55), slt = mix(stripe, Qt::white, 0.15);
        std::vector<Item> parts;
        auto zOf = [](V3 q) { return cam(q).z; };
        auto cyl = [&](V3 A, V3 B, double r, double r2, QColor d0, QColor l0, double bias, std::function<void(Cv&)> extra = nullptr) {
            parts.push_back({ (zOf(A) + zOf(B)) / 2 + bias, [=](Cv& y) { drawCyl(y, A, B, r, d0, l0, r2); if (extra) extra(y); } });
        };
        const double side = cam(rt).z < 0 ? 1 : -1;
        auto plate = [&](std::vector<V3> pts, V3 n3, QColor d0, QColor l0) {
            double z = 0; for (const V3& q : pts) z += zOf(q);
            parts.push_back({ z / pts.size(), [=](Cv& y) {
                const V3 nn = cam(n3).z < 0 ? n3 : mul(n3, -1);
                y.fillStyle(mix(d0, l0, 0.16 + 0.84 * qMax(0.0, dot(cam(nn), LV()))));
                y.begin(); bool first = true; for (const V3& q : pts) { const SP s2 = P(q); if (first) y.moveTo(s2.x, s2.y); else y.lineTo(s2.x, s2.y); first = false; } y.close(); y.fill();
                y.strokeStyle(rgba("#000000", 0.3)); y.lineWidth(0.6); y.stroke();
            } });
        };
        for (int sg : { -1, 1 }) plate({ at(-8, sg * 8, -1), at(-28, sg * 8, -1), at(-34, sg * 30, -4), at(-24, sg * 31, -4) }, up, dk, lt2);
        plate({ at(-14, 0, -8), at(-36, 0, -8), at(-40, 0, -22), at(-32, 0, -22) }, rt, dk, lt2);
        for (int sg : { -1, 1 }) cyl(at(-42, sg * 17, -3), at(-18, sg * 17, -3), 5.5, 5, dk, lt, -0.2);
        std::vector<double> winF;
        for (int f2 = -26; f2 <= 18; f2 += 4) if (f2 > 8 || int((phs * 1000 + f2 * 37)) % 10 < 7) winF.push_back(f2);
        cyl(at(-38, 0, 0), at(26, 0, 0), 9, 9, dk, lt, 0, [=](Cv& y) {
            for (double f2 : winF) { const SP q = P(at(f2, side * 9.3, 1.5)); y.fillStyle(rgba("#ffd8a0", 0.9)); y.fillRect(q.x - 0.9 * q.s, q.y - 0.6 * q.s, 1.8 * q.s, 1.2 * q.s); }
        });
        cyl(at(-6, 0, 0), at(4, 0, 0), 9.25, 9.25, sdk, slt, -0.3);
        for (double f2 : { -30.0, 12.0 }) cyl(at(f2 - 0.8, 0, 0), at(f2 + 0.8, 0, 0), 9.35, 9.35, dk2, dk, -0.35);
        cyl(at(26, 0, 0), at(42, 0, 0), 9, 2.4, dk, lt, -0.1, [=](Cv& y) {
            const SP a = P(at(27, side * 8.6, 2.5)), b = P(at(33, side * 6.6, 2.5)), c2 = P(at(33, side * 6.6, -2)), d = P(at(27, side * 8.6, -2));
            y.fillStyle(vgrad(a.y, d.y, hex("#bfe6ff"), hex("#3a7ab8"))); quad(y, a.q(), b.q(), c2.q(), d.q()); y.fill();
        });
        cyl(at(4, 0, 8), at(4, 0, 14), 5.2, 5.2, dk, lt, -0.4);
        std::sort(parts.begin(), parts.end(), [](const Item& a, const Item& b) { return a.z > b.z; });
        x.save(); x.alphaG(st.alpha);
        if (st.retro > 0.02) {    // os retrofoguetes do nariz (pra frente, atrás do casco)
            x.lighter();
            const double fl = 0.8 + 0.2 * std::sin(m_t * 31 + phs);
            for (int sg : { -1, 1 }) plume(x, at(37, sg * 6, 0), norm(add(fw, mul(rt, sg * 0.35))), 34 * st.retro * fl, 3.2, st.retro, Qt::white, hex("#ffb070"));
            x.over();
        }
        for (const Item& it : parts) it.f(x);
        x.lighter();
        for (int sg : { -1, 1 }) {
            const V3 b3 = at(-43.5, sg * 17, -3);
            const double fl = 0.82 + 0.18 * std::sin(m_t * 29 + sg * 1.7 + phs) + 0.06 * std::sin(m_t * 61 + sg);
            const SP q = P(b3);
            if (st.eng > 0.02) { plume(x, b3, mul(fw, -1), (26 + 60 * st.eng) * fl, 5.2, qMin(1.0, 0.35 + st.eng * 0.8), hex("#f0fbff"), CY); glowAt(x, q.x, q.y, 40 * st.eng * q.s, CY, 0.16 * st.eng); }
            else glowAt(x, q.x, q.y, 4 * q.s, "#5a8ab8", 0.3);
        }
        if (st.rcsBelly > 0) for (double f2 : { -22.0, 22.0 }) plume(x, at(f2, 0, -9), mul(up, -1), 18, 2.2, st.rcsBelly, Qt::white, hex("#cfe6ff"));
        if (st.rcsUp > 0) for (double f2 : { -22.0, 22.0 }) for (int sg : { -1, 1 }) plume(x, at(f2, sg * 9, 3), norm(add(mul(up, 0.55), mul(rt, sg * 0.85))), 16, 2, st.rcsUp, Qt::white, hex("#cfe6ff"));
        const double bl = std::sin(m_t * 4 + phs) > 0.4 ? 1 : 0.12;
        const SP wl = P(at(-24, -31, -4)), wr = P(at(-24, 31, -4)), tail = P(at(-38, 0, -22));
        glowAt(x, wl.x, wl.y, 5 * wl.s, "#ff3a2a", 0.9 * bl); glowAt(x, wr.x, wr.y, 5 * wr.s, "#3aff8a", 0.9 * bl);
        if (std::fmod(m_t + phs, 1.4) < 0.07) glowAt(x, tail.x, tail.y, 12 * tail.s, "#ffffff", 0.9);
        {   // o reflexo do sol no casco, discreto
            double best = -1; V3 bp = c3;
            for (int k = 0; k < 16; ++k) { const double th = k / 16.0 * TAU; const V3 n = add(mul(rt, std::cos(th)), mul(up, std::sin(th))); const double sp = dot(cam(n), HV()); if (sp > best) { best = sp; bp = at(8, std::cos(th) * 9.2, std::sin(th) * 9.2); } }
            const SP q = P(bp);
            starGlint(x, q.x, q.y, 11 * q.s, std::pow(qMax(0.0, best), 10) * (0.3 + 0.2 * std::sin(m_t * 1.7 + phs)));
        }
        x.over();
        x.restore();
    }

    QString m_key;
    QPixmap m_back, m_hull;
    QPointF m_O, m_bandA, m_bandB;
    double m_bandH = 6;
    std::vector<Dock> m_docks;
    std::vector<Walker> m_walkers;
    std::vector<Nav> m_navs;
    std::vector<Glint> m_glints;
    std::vector<std::array<QPointF, 4>> m_panels;
    double m_t = 0;
};

} // namespace

std::unique_ptr<Layer> makeGenresB(const QString& id)
{
    if (id == QLatin1String("citadel")) return std::make_unique<Citadel>();
    if (id == QLatin1String("rooftop")) return std::make_unique<Rooftop>();
    if (id == QLatin1String("hoard")) return std::make_unique<Hoard>();
    return nullptr;
}

} // namespace AnimBg
