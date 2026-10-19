// Temas Animados de gênero (leva 7, 16–17/10): Eve of Battle (medieval) e Outpost (ficção científica).
// Portados de evebattle.js e outpost.js do Weather Lab, quase linha a linha, pela camada de canvas
// (AnimCanvas.h). O cenário parado vai em imagens prontas, montadas com semente fixa; por quadro só o
// que se mexe. Só noite.

#include "AnimCanvas.h"

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

// a língua de fogo em três camadas (o flame() do Lab)
void flame(Cv& x, double fx, double fy, double s, double t)
{
    static const struct { double k; const char* col; double a, ph; } L[3] = { { 1, "#ff5a10", 0.8, 0 }, { 0.75, "#ff9a30", 0.9, 1.3 }, { 0.45, "#ffe8a0", 0.95, 2.1 } };
    for (const auto& l : L) {
        const double h = (0.9 + 0.15 * std::sin(t * 11 + l.ph) + 0.1 * std::sin(t * 23 + l.ph)) * s * l.k, w = 0.38 * s * l.k, lean = std::sin(t * 2.7 + l.ph) * 0.12 * s + 0.1 * s;
        x.fillStyle(rgba(l.col, l.a));
        x.begin(); x.moveTo(fx - w, fy); x.quadTo(fx - w * 1.2, fy - h * 0.5, fx + lean * l.k, fy - h); x.quadTo(fx + w * 1.2, fy - h * 0.5, fx + w, fy); x.close(); x.fill();
    }
}

// ============================================================ Eve of Battle
// A noite antes da batalha: no alto de uma colina (um canto escuro, embaixo à esquerda), uma fogueira pequena,
// o estandarte cravado e um cavaleiro sentado de costas numa pedra, com o cavalo ao lado; e o vale inteiro, até o
// horizonte, coberto de fogueiras e barracas do exército inimigo. Lá longe, o castelo que vai ser atacado.
// Vida: o fogo, as fagulhas e a fumaça, o estandarte no vento, o cavalo (cabeça, orelha, rabo, pata), o capim,
// as fogueiras do vale tremendo, a patrulha de tochas cruzando, as nuvens passando.
class EveBattle : public Layer {
public:
    void init(Ctx&) override
    {
        m_key.clear();
        m_sparks.clear();
        m_smoke.clear();
        m_star.on = false;
        m_nextStar = R(8, 16);
        m_head = 0;
        m_nextNod = R(3, 7);
        m_patrol.on = false;
        m_nextPatrol = 3;
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_tail = std::sin(m_t * 1.3) * 0.6 + std::sin(m_t * 3.7) * 0.2;
        m_cloudOff += 4 * dt;
        m_ear = rnd() < dt * 0.5 ? 1 : m_ear * std::pow(0.1, dt);
        m_nextHoof -= dt;
        m_hoof = m_nextHoof < 0 && m_nextHoof > -6 ? qMin(1.0, -m_nextHoof * 2) * qMin(1.0, (6 + m_nextHoof) * 2) : 0;
        if (m_nextHoof < -6) m_nextHoof = R(12, 25);
        if (m_key.isEmpty()) return;
        if (rnd() < dt * 4) m_smoke.push_back({ m_fire.x() + R(-4, 4), m_fire.y() - 26, 0, R(3, 5), R(3, 6), 0, 0 });
        for (Puff& q : m_smoke) { q.t += dt; q.y -= 16 * dt; q.x += (6 + q.t * 4) * dt; }
        m_smoke.erase(std::remove_if(m_smoke.begin(), m_smoke.end(), [](const Puff& q) { return q.t >= q.life; }), m_smoke.end());
        m_nextPatrol -= dt;
        if (!m_patrol.on && m_nextPatrol <= 0) {
            const double d = R(0.25, 0.6);
            m_patrol.on = true;
            m_patrol.dir = rnd() < 0.5 ? 1 : -1;
            m_patrol.x = m_patrol.dir > 0 ? -30 : c.W + 30;
            m_patrol.y = HZ + 8 + d * 160;
            m_patrol.s = 0.5 + d * 1.6;
            m_patrol.n = 4 + RI(5);
            m_patrol.v = 6 + d * 10;
        }
        if (m_patrol.on) { m_patrol.x += m_patrol.dir * m_patrol.v * dt; if (m_patrol.x < -120 || m_patrol.x > c.W + 120) { m_patrol.on = false; m_nextPatrol = R(10, 25); } }
        m_nextNod -= dt;
        const double target = m_nextNod < 0 && m_nextNod > -3.5 ? 1 : 0;
        m_head += (target - m_head) * qMin(1.0, dt * 1.5);
        if (m_nextNod < -3.5) m_nextNod = R(6, 14);
        if (rnd() < dt * 10) m_sparks.push_back({ m_fire.x() + R(-5, 5), m_fire.y() - 14, 0, R(1, 2.6), 0, R(4, 18), -R(22, 48) });
        for (Puff& s : m_sparks) { s.t += dt; s.x += s.vx * dt; s.y += s.vy * dt; s.vy += 4 * dt; }
        m_sparks.erase(std::remove_if(m_sparks.begin(), m_sparks.end(), [](const Puff& q) { return q.t >= q.life; }), m_sparks.end());
        m_nextStar -= dt;
        if (!m_star.on && m_nextStar <= 0) m_star = { true, R(zx(c, 60), zx(c, 400)), R(30, 200), 0 };
        if (m_star.on) { m_star.t += dt; if (m_star.t > 0.7) { m_star.on = false; m_nextStar = R(25, 60); } }
    }
    void paint(QPainter& painter, const Pal& pal, Ctx& c) override
    {
        const QString key = sceneKey(pal, c);
        if (key != m_key) {
            Seeded sd(0xEBA77u);
            build(pal, c);
            m_key = key;
        }
        Cv x(painter);
        x.image(m_back, 0, 0);
        { const double o = std::fmod(m_cloudOff, c.W * 2); x.image(m_cloud, -o, 0); x.image(m_cloud, c.W * 2 - o, 0); }
        if (m_star.on) {
            const double k = m_star.t / 0.7;
            x.strokeStyle(rgba("#ffffff", 0.6 * std::sin(k * M_PI))); x.lineWidth(1);
            x.line(m_star.x + k * 80, m_star.y + k * 30, m_star.x + k * 80 - 60, m_star.y + k * 30 - 22);
        }
        x.lighter();
        for (int i = 0; i < int(m_castLights.size()); ++i) {
            const QPointF& q = m_castLights[i];
            const double f = 0.75 + 0.25 * std::sin(m_t * (5 + i % 4) + i * 1.7);
            glowAt(x, q.x() + 0.8, q.y() + 1, 6, "#ffa040", 0.4 * f);
            x.fillStyle(rgba("#ffc070", 0.85 * f)); x.fillRect(q.x(), q.y(), 1.6, 2.2);
        }
        for (const Camp& cp : m_camps) {
            const double fl = 0.7 + 0.3 * std::sin(m_t * cp.f + cp.ph) * std::sin(m_t * cp.f * 0.37 + cp.ph * 2);
            glowAt(x, cp.x, cp.y, 5 * cp.s + 2, "#ff8030", 0.32 * fl);
            x.fillStyle(rgba("#ffd090", 0.75 + 0.25 * fl)); x.fillRect(cp.x - 0.5 * cp.s, cp.y - 0.7 * cp.s, cp.s, cp.s * 0.9);
        }
        x.over();
        // a patrulha de tochas andando em fila pelo vale
        if (m_patrol.on) {
            x.lighter();
            for (int k = 0; k < m_patrol.n; ++k) {
                const double px = m_patrol.x - m_patrol.dir * k * 9 * m_patrol.s, py = m_patrol.y + std::sin(m_t * 6 + k) * 0.6, f = 0.75 + 0.25 * std::sin(m_t * 13 + k * 2);
                glowAt(x, px, py, 7 * m_patrol.s, "#ff9a40", 0.45 * f);
                x.fillStyle(rgba("#ffe0a0", 0.95)); x.fillRect(px - 0.6, py - 0.8, 1.2, 1.4);
            }
            x.over();
        }
        // o capim balançando no vento
        x.lineWidth(1); x.cap = Qt::FlatCap;
        const QColor gr = mix(pal.app, hex("#0a0a0c"), 0.6), grLit = hex("#5a3418");
        for (const Grass& g : m_grass) {
            const double sw = std::sin(m_t * 1.9 + g.x * 0.04) * 2.5 + std::sin(m_t * 3.3 + g.ph) * 1;
            x.strokeStyle(g.lit > 0.05 ? mix(gr, grLit, g.lit * 0.6) : gr);
            x.begin(); x.moveTo(g.x, g.y); x.quadTo(g.x + sw * 0.4, g.y - g.h * 0.5, g.x + g.lean + sw, g.y - g.h); x.stroke();
        }
        drawBanner(x, pal);
        drawHorse(x, pal);
        x.image(m_front, 0, 0);
        const double fl = 0.8 + 0.12 * std::sin(m_t * 9) + 0.08 * std::sin(m_t * 21);
        x.lighter();
        x.save(); x.translate(m_fire.x(), m_fire.y()); x.scale(1, 0.32); glowAt(x, 0, 0, 170, FIRE, 0.3 * fl); x.restore();
        glowAt(x, m_fire.x(), m_fire.y() - 16, 120, FIRE, 0.15 * fl);
        x.over();
        flame(x, m_fire.x(), m_fire.y() - 4, 24, m_t);
        for (const Puff& q : m_smoke) {
            const double k = q.t / q.life;
            x.fillStyle(rgba("#8a8088", 0.13 * std::sin(M_PI * k)));
            x.begin(); x.arc(q.x, q.y, q.r + k * 14, 0, TAU); x.fill();
        }
        drawKnight(x, pal, fl);
        x.lighter();
        for (const Puff& s : m_sparks) { x.fillStyle(rgba("#ffb060", 0.9 * (1 - s.t / s.life))); x.fillRect(s.x, s.y, 1.5, 1.5); }
        x.over();
    }

private:
    static constexpr double HZ = 392;
    const QColor FIRE = hex("#ff8a30");
    struct Camp { double x, y, s, ph, f; };
    struct Grass { double x, y, h, lean, ph, lit; };
    struct Puff { double x, y, t, life, r, vx, vy; };
    struct Star { bool on = false; double x = 0, y = 0, t = 0; };
    struct Patrol { bool on = false; int dir = 1, n = 4; double x = 0, y = 0, s = 1, v = 8; };

    double hillY(double xx) const
    {
        return xx < m_edge ? 578 - std::sin((xx - m_o) * 0.011 + 1) * 8 - qMax(0.0, m_o + 120 - xx) * 0.12 : 578 + std::pow((xx - m_edge) / 34, 2) * 26;
    }
    double depth(double y, const Ctx& c) const { return qMax(0.0, (y - HZ) / (c.H - HZ)); }

    void build(const Pal& pal, const Ctx& c)
    {
        const double W = c.W, H = c.H;
        m_o = zx(c, 45) - 45;
        m_edge = zx(c, 345);
        m_back = layer(c, W, H);
        {
            QPainter qp(&m_back);
            Cv x(qp);
            x.fillStyle(vgrad(0, HZ + 30, mix(pal.app, hex("#03050c"), 0.55), mix(pal.app, hex("#1c2236"), 0.45))); x.fillRect(0, 0, W, HZ + 30);
            for (int i = 0; i < 650; ++i) {
                const double sy = std::pow(rnd(), 1.15) * HZ, f = 1 - qBound(0.0, (sy - (HZ - 160)) / 150, 1.0);
                x.fillStyle(rgba("#ffffff", R(0.1, 0.65) * f));
                const double s = rnd() < 0.05 ? 1.7 : R(0.6, 1.2);
                x.fillRect(R(0, W), sy, s, s);
            }
            const double mx = zx(c, 120), my = 110;
            glowAt(x, mx, my, 120, "#c8d0e8", 0.12);
            x.fillStyle(hex("#ece8dc")); x.begin(); x.arc(mx, my, 13, 0, TAU); x.fill();
            x.fillStyle(mix(pal.app, hex("#060a16"), 0.5)); x.begin(); x.arc(mx + 6, my - 3, 12.5, 0, TAU); x.fill();
            const QPixmap cd = softDot(mix(pal.app, hex("#2a3048"), 0.45), 0.3);
            for (int i = 0; i < 22; ++i) { const double cx = R(-100, W + 100), cy = R(150, 290), w = R(160, 380); x.alphaG(R(0.2, 0.45)); x.image(cd, cx - w / 2, cy - w * 0.05, w, w * 0.1); }
            x.alphaG(1);
            // o céu tingido de laranja pelo acampamento
            x.lighter(); x.fillStyle(vgrad(HZ - 140, HZ + 10, rgba("#ff7a30", 0), rgba("#ff7a30", 0.16))); x.fillRect(0, HZ - 140, W, 150); x.over();
            // serras no horizonte, e o castelo sitiado no platô de uma delas (o pé enterrado no morro, nada flutuando)
            const Profile pr = profile(8, 0.55), rough = profile(9, 0.7);
            const double castX = W - 260;
            auto plateau = [&](double xx) { const double d = std::abs(xx - castX); if (d < 74) return 1.0; if (d > 250) return 0.0; return 0.5 + 0.5 * std::cos((d - 74) / 176 * M_PI); };
            auto ridgeY = [&](double xx) { const double k = plateau(xx); return HZ - 18 - pr.at(xx / W) * 22 * (1 - k) - k * (60 + pr.at(castX / W) * 22) - (k > 0 && k < 1 ? rough.at(xx / W * 6) * 5 * k : 0); };
            const QColor rangeCol = mix(pal.app, hex("#121624"), 0.5);
            auto fillRange = [&](double dy, double from, double to) {
                x.fillStyle(rangeCol); x.begin(); x.moveTo(from, HZ + 20);
                for (double xx = from; xx <= to; xx += 3) x.lineTo(xx, ridgeY(xx) + dy);
                x.lineTo(to, HZ + 20); x.fill();
            };
            // uma serra mais longe, lavada pela névoa, pra dar fundo
            {
                const Profile far = profile(8, 0.6);
                x.fillStyle(mix(pal.app, hex("#1c2236"), 0.45)); x.begin(); x.moveTo(-10, HZ + 10);
                for (double xx = -10; xx <= W + 10; xx += 4) x.lineTo(xx, HZ - 34 - far.at(xx / W) * 26);
                x.lineTo(W + 10, HZ + 10); x.fill();
            }
            fillRange(0, -10, W + 10);
            const double cx = castX, cy = ridgeY(castX) + 1;
            const QColor stone = mix(pal.app, hex("#0c0e18"), 0.55), rim = rgba("#a8b4d8", 0.14);
            m_castLights.clear();
            auto wallTop = [&](double x0, double x1, double y) { x.fillStyle(stone); for (double xx = x0; xx < x1 - 1; xx += 4) x.fillRect(xx, y - 2.5, 2.2, 2.6); };
            auto tower = [&](double tx, double w, double h, bool cone) {
                x.fillStyle(stone); x.fillRect(tx - w / 2, cy - h, w, h + 14);
                if (cone) { x.begin(); x.moveTo(tx - w / 2 - 2, cy - h); x.lineTo(tx, cy - h - w * 1.35); x.lineTo(tx + w / 2 + 2, cy - h); x.fill(); x.strokeStyle(rim); x.lineWidth(1); x.line(tx - w / 2 - 2, cy - h, tx, cy - h - w * 1.35); }
                else wallTop(tx - w / 2, tx + w / 2 + 1, cy - h);
                x.strokeStyle(rim); x.lineWidth(1); x.line(tx - w / 2, cy - h, tx - w / 2, cy);
            };
            // a muralha com ameias, o portão aceso, as torres e o torreão com o estandarte (30% maior que no Lab: o reino em volta pedia)
            const double CK = 1.3;
            x.save(); x.translate(cx, cy); x.scale(CK, CK); x.translate(-cx, -cy);
            x.fillStyle(stone); x.fillRect(cx - 64, cy - 18, 128, 32); wallTop(cx - 64, cx + 64, cy - 18);
            x.strokeStyle(rim); x.lineWidth(1); x.line(cx - 64, cy - 18, cx + 64, cy - 18);
            tower(cx + 20, 26, 46, false);
            tower(cx + 26, 11, 70, true);
            tower(cx - 64, 13, 32, true); tower(cx + 64, 13, 30, true);
            tower(cx - 30, 10, 28, true); tower(cx - 12, 9, 26, false); tower(cx + 2, 9, 26, false);
            x.fillStyle(rgba("#ff9a40", 0.5)); x.begin(); x.moveTo(cx - 9, cy + 1); x.lineTo(cx - 9, cy - 8); x.quadTo(cx - 5, cy - 13, cx - 1, cy - 8); x.lineTo(cx - 1, cy + 1); x.fill();
            m_castLights.push_back(QPointF(cx - 5, cy - 4));
            {
                const double px = cx + 26, py = cy - 70 - 11 * 1.35;
                x.strokeStyle(stone); x.lineWidth(1); x.line(px, py, px, py - 12);
                x.fillStyle(mix(pal.app, hex("#7a1a18"), 0.6)); x.begin(); x.moveTo(px, py - 12); x.quadTo(px + 6, py - 13, px + 11, py - 10); x.lineTo(px + 6, py - 8.5); x.lineTo(px + 11, py - 6.5); x.quadTo(px + 6, py - 8, px, py - 7); x.fill();
            }
            for (const auto& w2 : { QPointF(-31, -20), QPointF(-65, -24), QPointF(19, -38), QPointF(25, -58), QPointF(25, -30), QPointF(63, -20), QPointF(-42, -10), QPointF(40, -9), QPointF(12, -12) })
                m_castLights.push_back(QPointF(cx + w2.x(), cy + w2.y()));
            x.restore();
            for (QPointF& q : m_castLights) q = QPointF(cx + (q.x() - cx) * CK, cy + (q.y() - cy) * CK);
            // o pé do castelo enterrado: a encosta passa por cima da base
            fillRange(5, castX - 140, castX + 140);
            // o reino: a cidade cobrindo as encostas até o castelo, a catedral e a muralha de fora no pé do morro
            {
                const double wallY = HZ - 15, gateX = cx + 26;
                // a rua principal subindo do portão até o castelo (passa por baixo das casas)
                {
                    const double pts[][2] = { { -5, 4 }, { 36, 13 }, { -22, 27 }, { 44, 42 }, { -6, 60 }, { 26, wallY - cy } };
                    const int n = 6;
                    x.strokeStyle(rgba("#8a7a6a", 0.12)); x.lineWidth(1.6); x.begin();
                    x.moveTo(cx + pts[0][0], cy + pts[0][1]);
                    for (int k = 1; k < n - 1; ++k) x.quadTo(cx + pts[k][0], cy + pts[k][1], cx + (pts[k][0] + pts[k + 1][0]) / 2, cy + (pts[k][1] + pts[k + 1][1]) / 2);
                    x.lineTo(cx + pts[n - 1][0], cy + pts[n - 1][1]);
                    x.stroke();
                }
                const QColor roofC = mix(stone, hex("#05060a"), 0.35), lampC = hex("#ffc070");
                std::vector<QPointF> windows;
                auto house = [&](double hx, double hy) {
                    const double w = R(6, 11), h = R(4, 7.5), rh = R(3, 6);
                    x.fillStyle(mix(stone, hex("#1e1c26"), R(0, 0.45))); x.fillRect(hx - w / 2, hy - h, w, h + 5);
                    x.fillStyle(roofC);
                    if (rnd() < 0.7) { x.begin(); x.moveTo(hx - w / 2 - 1, hy - h); x.lineTo(hx, hy - h - rh); x.lineTo(hx + w / 2 + 1, hy - h); x.fill(); }
                    else { x.begin(); x.moveTo(hx - w / 2 - 1, hy - h); x.lineTo(hx - w / 2 + 2, hy - h - rh * 0.7); x.lineTo(hx + w / 2 - 2, hy - h - rh * 0.7); x.lineTo(hx + w / 2 + 1, hy - h); x.fill(); }
                    if (rnd() < 0.15) x.fillRect(hx + w * 0.2, hy - h - rh - 1, 1.5, rh * 0.8);
                    if (rnd() < 0.18) { x.strokeStyle(rim); x.lineWidth(0.8); x.line(hx - w / 2 - 1, hy - h, hx, hy - h - rh); }
                    if (rnd() < 0.2) windows.push_back(QPointF(hx + R(-w / 2 + 1.5, w / 2 - 2.5), hy - h + R(1, h - 1)));
                };
                // a catedral, numa encosta à esquerda do castelo
                const double catX = cx - 132, catY = qMax(ridgeY(catX) + 22, cy + 30);
                bool catDone = false;
                auto cathedral = [&]() {
                    const double bx = catX, by = catY;
                    x.fillStyle(stone); x.fillRect(bx - 16, by - 16, 40, 22);
                    x.fillStyle(roofC); x.begin(); x.moveTo(bx - 17, by - 16); x.lineTo(bx + 4, by - 26); x.lineTo(bx + 25, by - 16); x.fill();
                    x.fillStyle(stone); x.fillRect(bx - 22, by - 40, 11, 46);
                    x.begin(); x.moveTo(bx - 23, by - 40); x.lineTo(bx - 16.5, by - 72); x.lineTo(bx - 10, by - 40); x.fill();
                    x.strokeStyle(rim); x.lineWidth(1); x.line(bx - 23, by - 40, bx - 16.5, by - 72); x.line(bx - 16.5, by - 72, bx - 16.5, by - 78); x.line(bx - 19, by - 76, bx - 14, by - 76);
                    x.fillStyle(rgba("#ffb060", 0.6)); x.begin(); x.arc(bx + 4, by - 9, 2.6, 0, TAU); x.fill();
                    for (int k = 0; k < 4; ++k) { x.fillStyle(rgba("#ffb060", 0.45)); x.fillRect(bx - 6 + k * 7, by - 6, 1.6, 4); }
                    m_castLights.push_back(QPointF(bx - 17.3, by - 34));
                    windows.push_back(QPointF(bx + 3.2, by - 10));
                };
                // as casas em fileiras: de cima (mais longe) pra baixo (mais perto), cobrindo umas às outras
                for (double hy = cy + 8; hy < wallY - 2; hy += R(6, 8)) {
                    if (!catDone && hy > catY - 4) { cathedral(); catDone = true; }
                    for (double hx = castX - 270 + R(0, 8); hx < castX + 270; hx += R(8, 14)) {
                        if (hy < ridgeY(hx) + 5) continue;                                   // acima da crista: céu
                        if (std::abs(hx - cx) < 92 && hy < cy + 16) continue;                 // o pé do castelo
                        if (std::abs(hx - catX + 3) < 26 && std::abs(hy - catY) < 10) continue;
                        if (rnd() > 0.15 + 0.7 * plateau(hx)) continue;                     // mais rala nas pontas
                        house(hx, hy);
                    }
                }
                if (!catDone) cathedral();
                // as janelas acesas da cidade (umas tremem a cada quadro, as outras ficam na imagem)
                x.lighter();
                for (const QPointF& wn : windows) {
                    if (rnd() < 0.12) { m_castLights.push_back(wn); continue; }
                    glowAt(x, wn.x() + 0.6, wn.y() + 0.8, 4.5, "#ffa040", 0.25);
                    x.fillStyle(rgba(lampC, 0.75)); x.fillRect(wn.x(), wn.y(), 1.3, 1.6);
                }
                x.over();
                // a muralha de fora, no pé do morro: ameias, torres, o portão aceso de frente pro exército;
                // pedra mais clara (luar) que a cidade atrás, pra recortar na frente dela
                const QColor wallC = mix(stone, hex("#3a3c4c"), 0.45), wallTopC = mix(stone, hex("#4a4c5c"), 0.5);
                double xl = castX, xr = castX;
                while (xl > castX - 300 && ridgeY(xl) < wallY - 12) xl -= 2;
                while (xr < castX + 300 && ridgeY(xr) < wallY - 12) xr += 2;
                xl += 6; xr -= 6;
                x.fillStyle(wallC); x.fillRect(xl, wallY - 11, xr - xl, 20); x.fillStyle(wallTopC); for (double xx = xl; xx < xr - 1; xx += 4) x.fillRect(xx, wallY - 13.5, 2.2, 2.6);
                x.fillStyle(vgrad(wallY - 11, wallY + 9, rgba("#000000", 0), rgba("#000000", 0.35))); x.fillRect(xl, wallY - 11, xr - xl, 20);
                x.strokeStyle(rgba("#a8b4d8", 0.22)); x.lineWidth(1); x.line(xl, wallY - 11, xr, wallY - 11);
                for (double tx = xl; tx <= xr + 1; tx += (xr - xl) / std::round((xr - xl) / 46)) {
                    if (std::abs(tx - gateX) < 20) continue;
                    x.fillStyle(wallC); x.fillRect(tx - 4.5, wallY - 21, 9, 30);
                    x.begin(); x.moveTo(tx - 6, wallY - 21); x.lineTo(tx, wallY - 33); x.lineTo(tx + 6, wallY - 21); x.fill();
                    x.strokeStyle(rim); x.line(tx - 6, wallY - 21, tx, wallY - 33);
                    m_castLights.push_back(QPointF(tx - 0.8, wallY - 17));
                }
                for (double gx : { gateX - 9, gateX + 9 }) { x.fillStyle(wallC); x.fillRect(gx - 5, wallY - 25, 10, 34); x.fillStyle(wallTopC); for (double xx = gx - 5; xx < gx + 5; xx += 4) x.fillRect(xx, wallY - 27.5, 2.2, 2.6); }
                x.fillStyle(rgba("#ff9a40", 0.55)); x.begin(); x.moveTo(gateX - 4, wallY + 6); x.lineTo(gateX - 4, wallY - 6); x.quadTo(gateX, wallY - 11, gateX + 4, wallY - 6); x.lineTo(gateX + 4, wallY + 6); x.fill();
                x.lighter(); glowAt(x, gateX, wallY + 2, 26, "#ff8a30", 0.25); x.over();
                m_castLights.push_back(QPointF(gateX - 10, wallY - 20)); m_castLights.push_back(QPointF(gateX + 8, wallY - 20));
            }
            x.fillStyle(vgrad(HZ - 40, HZ + 10, rgba("#8a7a8a", 0), rgba("#8a7a8a", 0.16))); x.fillRect(0, HZ - 40, W, 50);
            // o vale: chão escuro descendo, com dobras de terreno
            x.fillStyle(lin(0, HZ, 0, H, { { 0, mix(pal.app, hex("#22222e"), 0.5) }, { 110 / (H - HZ), mix(pal.app, hex("#0e0f18"), 0.55) }, { 1, mix(pal.app, hex("#07080c"), 0.6) } })); x.fillRect(0, HZ, W, H - HZ);
            {   // a névoa no pé dos morros: o vale e a serra se fundem em vez de cortar reto
                const QColor mist = mix(pal.app, hex("#5a4650"), 0.45);
                x.fillStyle(vgrad(HZ - 24, HZ, rgba(mist, 0), rgba(mist, 0.3))); x.fillRect(0, HZ - 24, W, 24);
                x.fillStyle(vgrad(HZ, HZ + 44, rgba(mist, 0.42), rgba(mist, 0))); x.fillRect(0, HZ, W, 44);
            }
            for (int k = 0; k < 7; ++k) {
                const double base = HZ + 12 + std::pow(k / 7.0, 1.6) * 330;
                const Profile pr2 = profile(7, 0.6);
                x.fillStyle(rgba("#000000", 0.15)); x.begin(); x.moveTo(-10, H);
                for (double xx = -10; xx <= W + 10; xx += 6) x.lineTo(xx, base - pr2.at(xx / W) * (6 + k * 3));
                x.lineTo(W + 10, H); x.fill();
            }
            // o acampamento: barracas em roda em volta das fogueiras, menores e mais juntas no horizonte
            m_camps.clear();
            const int groups = int(190 * W / 1440.0);
            const QColor canvas = mix(pal.app, hex("#1c1612"), 0.55), haze = mix(pal.app, hex("#262330"), 0.5), flagC = mix(pal.app, hex("#4a1614"), 0.6);
            struct TentD { double x, y, w, h; int side, kind; double warm; bool door; QColor col; };
            for (int g = 0; g < groups; ++g) {
                const double d = std::pow(rnd(), 1.5), gy = HZ + 6 + d * (H - HZ - 10), gx = R(-40, W + 40);
                if (gy > hillY(gx) - 4) continue;
                const double dep = depth(gy, c), s = 0.35 + std::pow(dep, 1.1) * 2.4;
                const int nf = 1 + RI(3);
                for (int k = 0; k < nf; ++k) { const double fx = gx + R(-12, 12) * s, fy = gy + R(-2, 2) * s; if (fy > hillY(fx) - 4) continue; m_camps.push_back({ fx, fy, s, R(0, 6.28), R(4, 9) }); }
                std::vector<TentD> ts;
                const int n = 3 + RI(6);
                const double a0 = R(0, TAU);
                for (int k = 0; k < n; ++k) {
                    const double a = a0 + k * TAU / n + R(-0.25, 0.25), rr = R(15, 28) * s, tx = gx + std::cos(a) * rr, ty = gy + std::sin(a) * rr * 0.32, w = R(3.2, 5) * s;
                    if (ty > hillY(tx) - 2) continue;
                    ts.push_back({ tx, ty, w, w * R(0.9, 1.15), gx > tx ? 1 : -1, rnd() < 0.62 ? 0 : 1, qMax(0.0, 1 - std::abs(tx - gx) / (34 * s)), rnd() < 0.3, mix(canvas, haze, qMax(0.0, 0.85 - dep * 1.6)) });
                }
                std::sort(ts.begin(), ts.end(), [](const TentD& a, const TentD& b) { return a.y < b.y; });
                for (const TentD& t : ts) {
                    const double tx = t.x, ty = t.y, w = t.w, h = t.h, sd = t.side, lit = (0.12 + 0.3 * t.warm) * (0.4 + 0.6 * qMin(1.0, dep * 2.5));
                    if (t.kind == 0) {
                        // barraca cônica de lona, com o mastro saindo da ponta
                        x.fillStyle(t.col); x.begin(); x.moveTo(tx - w, ty); x.quadTo(tx - w * 0.42, ty - h * 0.42, tx, ty - h); x.quadTo(tx + w * 0.42, ty - h * 0.42, tx + w, ty); x.quadTo(tx, ty + w * 0.14, tx - w, ty); x.fill();
                        x.fillStyle(vgrad(ty - h, ty, rgba("#ff9a40", lit * 0.35), rgba("#ffa850", lit))); x.begin(); x.moveTo(tx, ty - h); x.quadTo(tx + sd * w * 0.42, ty - h * 0.42, tx + sd * w, ty); x.quadTo(tx + sd * w * 0.5, ty + w * 0.12, tx, ty + w * 0.07); x.close(); x.fill();
                        if (w > 3.5) { x.strokeStyle(rgba("#000000", 0.3)); x.lineWidth(0.6); x.line(tx - sd * w * 0.3, ty + w * 0.05, tx, ty - h); }
                        x.strokeStyle(t.col); x.lineWidth(qMax(0.6, w * 0.08)); x.line(tx, ty - h, tx, ty - h - h * 0.2);
                        if (t.door) { x.fillStyle(rgba("#ffb060", 0.35 + 0.35 * t.warm)); x.begin(); x.moveTo(tx + sd * w * 0.12, ty + w * 0.07); x.lineTo(tx + sd * w * 0.3, ty - h * 0.36); x.lineTo(tx + sd * w * 0.48, ty + w * 0.06); x.fill(); }
                    } else {
                        // barraca de cumeeira vista de três quartos: a empena virada pro fogo, o telhado indo pra trás
                        const double gcx = tx + sd * w * 0.35, gw = w * 0.55, ay = ty - h * 0.8, bx = -sd * w * 0.95, by = -h * 0.14;
                        x.fillStyle(t.col); x.begin(); x.moveTo(gcx, ay); x.lineTo(gcx + bx, ay + by); x.lineTo(gcx - sd * gw + bx, ty + by); x.lineTo(gcx - sd * gw, ty); x.close(); x.fill();
                        x.fillStyle(rgba("#ffa850", lit * 0.35)); x.begin(); x.moveTo(gcx, ay); x.lineTo(gcx + bx, ay + by); x.lineTo(gcx + bx * 0.5 - sd * gw * 0.3, ay + by * 0.5 + (ty - ay) * 0.3); x.close(); x.fill();
                        x.fillStyle(mix(t.col, hex("#c87a3a"), 0.15 + lit * 0.9)); x.begin(); x.moveTo(gcx - gw, ty); x.lineTo(gcx, ay); x.lineTo(gcx + gw, ty); x.close(); x.fill();
                        x.strokeStyle(t.door ? rgba("#ffc070", 0.7) : rgba("#000000", 0.4)); x.lineWidth(qMax(0.6, w * 0.1)); x.line(gcx, ay + 1, gcx, ty);
                    }
                }
                // um estandarte fincado no meio de alguns grupos
                if (s > 0.6 && rnd() < 0.18) {
                    const double px = gx + R(-8, 8) * s, py = gy - 2 * s, ph = 11 * qMin(s, 1.6);
                    x.strokeStyle(canvas); x.lineWidth(qMax(0.6, 0.5 * s)); x.line(px, py, px, py - ph);
                    x.fillStyle(flagC); x.begin(); x.moveTo(px, py - ph); x.quadTo(px + 3 * s, py - ph - 0.6 * s, px + 6 * s, py - ph + 0.5 * s); x.lineTo(px + 4 * s, py - ph + 2 * s); x.lineTo(px + 6 * s, py - ph + 3.6 * s); x.quadTo(px + 3 * s, py - ph + 2.8 * s, px, py - ph + 3.2 * s); x.fill();
                }
            }
            // a fumaça parada sobre o vale
            const QPixmap sd = softDot(mix(pal.app, hex("#3a3038"), 0.4), 0.3);
            for (int i = 0; i < 70; ++i) { const double cx = R(0, W), cy = HZ + R(0, 120), w = R(140, 320); x.alphaG(R(0.1, 0.2)); x.image(sd, cx - w / 2, cy - w * 0.06, w, w * 0.12); }
            x.alphaG(1);
            x.lighter(); x.save(); x.translate(W * 0.5, HZ + 90); x.scale(1, 0.25); glowAt(x, 0, 0, W * 0.7, "#ff7a30", 0.08); x.restore(); x.over();
            // a colina da frente, quase preta, com o fio de luar na crista
            x.fillStyle(mix(pal.app, hex("#05060a"), 0.6)); x.begin(); x.moveTo(-10, H);
            for (double xx = -10; xx <= W + 10; xx += 3) x.lineTo(xx, hillY(xx));
            x.lineTo(W + 10, H); x.fill();
            x.strokeStyle(rgba("#a8b4d8", 0.18)); x.lineWidth(1); x.begin();
            for (double xx = -10; xx <= m_edge; xx += 3) x.lineTo(xx, hillY(xx));
            x.stroke();
        }
        m_grass.clear();
        for (int i = 0; i < 220; ++i) { const double xx = R(-10, zx(c, 375)); m_grass.push_back({ xx, hillY(xx) + 1, R(6, 20), R(1, 7), R(0, 6.28), qMax(0.0, 1 - std::abs(xx - zx(c, 178)) / 140) }); }
        // as nuvens que passam devagar
        m_cloud = layer(c, W * 2, 300);
        {
            QPainter qp(&m_cloud);
            Cv x(qp);
            const QPixmap cd = softDot(mix(pal.app, hex("#3a3a52"), 0.45), 0.3), rim = softDot(hex("#ffb088"), 0.25);
            for (int i = 0; i < 30; ++i) {
                const double cx = R(0, W * 2), cy = R(40, 260), w = R(160, 380);
                x.alphaG(R(0.25, 0.45)); x.image(cd, cx - w / 2, cy - w * 0.06, w, w * 0.12);
                x.alphaG(R(0.04, 0.08)); x.image(rim, cx - w * 0.4, cy, w * 0.8, w * 0.05);
            }
        }
        // posições
        m_fire = QPointF(zx(c, 196), hillY(zx(c, 196)) + 2);
        m_knight = QPointF(zx(c, 262), hillY(zx(c, 262)) + 2);
        m_horse = QPointF(zx(c, 112), hillY(zx(c, 112)) + 3);
        m_banner = QPointF(zx(c, 328), hillY(zx(c, 328)) + 1);
        // a pedra, as lenhas e o círculo de pedras da fogueira, o escudo, o elmo
        m_front = layer(c, W, H);
        {
            QPainter qp(&m_front);
            Cv f(qp);
            const double kx = m_knight.x(), ky = m_knight.y();
            f.fillStyle(mix(pal.app, hex("#16141c"), 0.55)); f.begin(); f.moveTo(kx - 30, ky + 1); f.quadTo(kx - 32, ky - 22, kx - 10, ky - 26); f.quadTo(kx + 18, ky - 28, kx + 28, ky - 12); f.lineTo(kx + 30, ky + 1); f.fill();
            f.fillStyle(rgba("#ff9a40", 0.25)); f.begin(); f.moveTo(kx - 30, ky); f.quadTo(kx - 32, ky - 22, kx - 10, ky - 26); f.lineTo(kx - 14, ky - 20); f.quadTo(kx - 26, ky - 16, kx - 24, ky); f.fill();
            const double fx = m_fire.x(), fy = m_fire.y();
            for (int k = 0; k < 9; ++k) {
                const double a = M_PI + k / 8.0 * M_PI, rx0 = fx + std::cos(a) * 20, ry0 = fy + std::sin(a) * 4 + 1;
                f.fillStyle(mix(pal.app, hex("#2a2420"), 0.5)); f.begin(); f.ellipse(rx0, ry0, 5, 3.2, 0, 0, TAU); f.fill();
                f.fillStyle(rgba("#ffa040", 0.4)); f.begin(); f.ellipse(rx0 + (rx0 < fx ? 2 : -2), ry0 - 1.2, 2.4, 1.2, 0, 0, TAU); f.fill();
            }
            f.strokeStyle(hex("#2a1a10")); f.lineWidth(4); f.roundCaps(); f.begin(); f.moveTo(fx - 13, fy); f.lineTo(fx + 10, fy - 7); f.moveTo(fx + 13, fy); f.lineTo(fx - 9, fy - 8); f.stroke();
            const double shx = kx + 40, shy = hillY(shx) - 1;
            f.fillStyle(mix(pal.app, hex("#3a1a18"), 0.5)); f.begin(); f.moveTo(shx - 12, shy - 30); f.lineTo(shx + 12, shy - 30); f.quadTo(shx + 12, shy - 6, shx, shy + 2); f.quadTo(shx - 12, shy - 6, shx - 12, shy - 30); f.fill();
            f.strokeStyle(rgba("#d8b060", 0.5)); f.lineWidth(1.5); f.begin(); f.moveTo(shx, shy - 27); f.lineTo(shx, shy - 3); f.moveTo(shx - 9, shy - 18); f.lineTo(shx + 9, shy - 18); f.stroke();
            const double hx = kx - 42, hy = hillY(hx) - 1;
            f.fillStyle(mix(pal.app, hex("#4a4a54"), 0.5)); f.begin(); f.arc(hx, hy - 8, 9, M_PI, 0); f.lineTo(hx + 9, hy); f.lineTo(hx - 9, hy); f.fill();
            f.fillStyle(hex("#0a0a0e")); f.fillRect(hx - 7, hy - 8, 14, 2);
            f.fillStyle(rgba("#ffb060", 0.35)); f.begin(); f.arc(hx, hy - 8, 9, M_PI, M_PI * 1.4); f.lineTo(hx - 4, hy - 8); f.fill();
            f.fillStyle(hex("#8a1a18")); f.begin(); f.moveTo(hx, hy - 17); f.quadTo(hx + 12, hy - 25, hx + 14, hy - 13); f.lineTo(hx + 3, hy - 15); f.fill();
        }
    }

    // o cavalo de lado, olhando o vale
    void drawHorse(Cv& x, const Pal& pal)
    {
        const double s = 1.5, nod = m_head, wind = std::sin(m_t * 1.7) * 0.5 + std::sin(m_t * 3.1) * 0.25;
        const QColor base = mix(pal.app, hex("#1c1820"), 0.5), dark = mix(base, Qt::black, 0.35);
        x.save(); x.translate(m_horse.x(), m_horse.y()); x.scale(s, s); x.roundCaps();
        auto leg = [&](double x0, double y0, double kx, double ky, double fx, double fy, double w0, double w1, const QColor& col, double lift) {
            const double fx2 = fx + lift * 3, fy2 = fy - lift * 4;
            x.fillStyle(col); x.begin(); x.moveTo(x0 - w0, y0); x.quadTo(kx - w1 * 1.6, (y0 + ky) / 2, kx - w1 * 1.3, ky); x.lineTo(fx2 - w1, fy2 - 4); x.lineTo(fx2 - w1 * 1.2, fy2 - 1);
            x.lineTo(fx2 + w1 * 1.6, fy2 - 1); x.lineTo(fx2 + w1, fy2 - 4); x.lineTo(kx + w1 * 1.3, ky); x.quadTo(kx + w1 * 1.6, (y0 + ky) / 2, x0 + w0, y0); x.close(); x.fill();
            x.fillStyle(hex("#0a0808")); x.begin(); x.moveTo(fx2 - w1 * 1.3, fy2 - 1.5); x.lineTo(fx2 + w1 * 1.7, fy2 - 1.5); x.lineTo(fx2 + w1 * 2.1, fy2 + 0.5); x.lineTo(fx2 - w1 * 1.3, fy2 + 0.5); x.fill();
        };
        leg(18, -36, 19, -16, 20, 0, 4.5, 1.8, dark, 0); leg(-30, -40, -37, -19, -34, 0, 7, 1.8, dark, 0);
        // o rabo em mechas
        for (int k = 0; k < 9; ++k) {
            const double sp = (k - 4) * 0.6, len = 34 + std::abs(sp) * 2, sw = wind * (6 + k * 0.6) + m_tail * 4;
            x.strokeStyle(k % 2 ? dark : mix(base, Qt::black, 0.2)); x.lineWidth(2.2 - std::abs(sp) * 0.15);
            x.begin(); x.moveTo(-40, -55); x.bezTo(-48 + sp, -52, -50 + sw * 0.5 + sp, -36, -46 + sw + sp * 1.5, -55 + len); x.stroke();
        }
        auto body = [&]() { x.begin(); x.moveTo(-41, -46); x.bezTo(-44, -58, -38, -66, -26, -64); x.bezTo(-14, -61, -4, -60, 6, -64);
            x.bezTo(14, -66, 22, -60, 28, -52); x.bezTo(33, -45, 31, -38, 24, -35); x.bezTo(10, -31, -12, -31, -26, -35); x.bezTo(-34, -36, -39, -40, -41, -46); x.close(); };
        body(); x.fillStyle(lin(0, -66, 0, -32, { { 0, mix(base, hex("#a8b4d8"), 0.14) }, { 0.5, base }, { 1, dark } })); x.fill();
        x.strokeStyle(rgba("#000000", 0.25)); x.lineWidth(1); x.begin(); x.moveTo(-34, -60); x.quadTo(-24, -48, -30, -38); x.moveTo(14, -60); x.quadTo(10, -48, 16, -36); x.stroke();
        // a luz da fogueira só no contorno do peito (tingir o corpo inteiro deixava o peito com cara de remendo marrom)
        x.lighter(); x.strokeStyle(rgba(FIRE, 0.45)); x.lineWidth(1.2); x.begin(); x.moveTo(28, -52); x.bezTo(33, -45, 31, -38, 24, -35); x.stroke();
        x.strokeStyle(rgba("#a8b4d8", 0.25)); x.lineWidth(1); x.begin(); x.moveTo(-41, -48); x.bezTo(-44, -58, -38, -66, -26, -64); x.bezTo(-14, -61, -4, -60, 6, -64); x.stroke();
        x.over();
        leg(22, -38, 26, -17, 26, 0, 5, 2, base, 0); leg(-24, -42, -31, -20, -28 + m_hoof * 2, 0, 8, 2, base, m_hoof);
        // pescoço e cabeça
        x.save(); x.translate(20, -55); x.rotate(nod * 0.62); x.scale(0.76, 0.76);
        x.begin(); x.moveTo(-14, -6); x.bezTo(-8, -18, 4, -30, 16, -36); x.bezTo(20, -38, 24, -38, 27, -35);
        x.bezTo(30, -31, 36, -22, 42, -12); x.bezTo(44, -8, 42, -4, 38, -5);
        x.bezTo(34, -6, 30, -8, 26, -12); x.bezTo(24, -6, 18, -6, 16, -12);
        x.bezTo(12, -2, 10, 6, 10, 10); x.lineTo(-6, 10); x.close();
        x.fillStyle(lin(-10, -36, 20, 10, { { 0, mix(base, hex("#a8b4d8"), 0.12) }, { 1, base } })); x.fill();
        x.fillStyle(base); x.begin(); x.moveTo(22, -36); x.quadTo(21 - m_ear * 2, -46, 24 - m_ear * 3, -47); x.quadTo(26, -42, 26, -36); x.fill();
        x.fillStyle(dark); x.begin(); x.moveTo(19, -35); x.quadTo(17, -43, 19, -44); x.quadTo(22, -40, 22, -35); x.fill();
        x.fillStyle(hex("#050505")); x.begin(); x.ellipse(29, -27, 2, 1.4, 0.6, 0, TAU); x.fill();
        x.fillStyle(rgba(FIRE, 0.8)); x.fillRect(29.4, -27.8, 0.9, 0.9);
        x.fillStyle(rgba("#000000", 0.6)); x.begin(); x.ellipse(40, -9, 1.6, 1, 0.5, 0, TAU); x.fill();
        x.strokeStyle(mix(pal.app, hex("#3a2214"), 0.55)); x.lineWidth(1.2); x.begin(); x.moveTo(25, -33); x.lineTo(36, -9); x.moveTo(26, -14); x.lineTo(38, -12); x.moveTo(30, -12); x.quadTo(10, -2, -8, -10); x.stroke();
        for (int k = 0; k < 14; ++k) {
            const double u = k / 13.0, cx = -12 + u * 36, cy = -6 - u * 30 - std::sin(u * 2.6) * 2, l = 7 + (1 - u) * 5;
            x.strokeStyle(k % 2 ? dark : mix(base, Qt::black, 0.15)); x.lineWidth(1.8);
            x.begin(); x.moveTo(cx, cy); x.quadTo(cx - 4 - wind * 2, cy + 2, cx - 6 - wind * 4 - u * 2, cy + l * 0.5 - wind); x.stroke();
        }
        x.lighter();
        x.strokeStyle(rgba(FIRE, 0.5)); x.lineWidth(1.2); x.begin(); x.moveTo(27, -35); x.bezTo(30, -31, 36, -22, 42, -12); x.bezTo(44, -8, 42, -4, 38, -5); x.stroke();
        x.strokeStyle(rgba("#a8b4d8", 0.22)); x.begin(); x.moveTo(-14, -6); x.bezTo(-8, -18, 4, -30, 16, -36); x.stroke();
        x.over(); x.restore();
        // manta, franja, sela e estribo
        x.fillStyle(mix(pal.app, hex("#5a1210"), 0.4)); x.begin(); x.moveTo(-18, -63); x.quadTo(-4, -64, 10, -64); x.lineTo(12, -40); x.quadTo(-4, -37, -20, -40); x.close(); x.fill();
        x.strokeStyle(rgba("#d8b060", 0.75)); x.lineWidth(1.4); x.begin(); x.moveTo(12, -42); x.quadTo(-4, -39, -20, -42); x.stroke();
        x.lineWidth(0.7);
        for (double xx = -19; xx < 12; xx += 2) { const double yy = -40.5 + std::pow((xx + 4) / 16, 2) * 1.5; x.line(xx, yy, xx + wind * 0.6, yy + 3); }
        // a sela: aba de couro sobre a manta, assento baixo entre a patilha (atrás) e o cepilho (na frente),
        // barrigueira passando por baixo, e o estribo pendurado na loro
        {
            const QColor leather = mix(pal.app, hex("#2e1c12"), 0.55), leatherD = mix(leather, Qt::black, 0.35);
            x.fillStyle(leatherD); x.begin(); x.moveTo(-11, -62); x.quadTo(-12, -53, -9, -51); x.lineTo(6, -51); x.quadTo(9, -53, 8, -62); x.close(); x.fill();
            x.strokeStyle(rgba("#000000", 0.35)); x.lineWidth(0.6); x.begin(); x.moveTo(-9.5, -59); x.quadTo(-10, -54, -8, -52.5); x.lineTo(5, -52.5); x.stroke();
            x.fillStyle(leatherD); x.fillRect(-2.2, -51, 3.4, 14.5);
            x.fillStyle(leather); x.begin(); x.moveTo(-15, -62); x.quadTo(-16, -67.5, -12.5, -68.5); x.quadTo(-5, -63, 3, -64);
            x.quadTo(7, -65, 8.5, -68.5); x.quadTo(11, -66.5, 10.5, -62); x.quadTo(-2, -60, -15, -62); x.fill();
            x.strokeStyle(rgba(FIRE, 0.45)); x.lineWidth(0.9); x.begin(); x.moveTo(-12.5, -68.5); x.quadTo(-5, -63, 3, -64); x.quadTo(7, -65, 8.5, -68.5); x.stroke();
            x.strokeStyle(rgba("#a8b4d8", 0.2)); x.lineWidth(0.8); x.begin(); x.moveTo(-15, -62); x.quadTo(-16, -67.5, -12.5, -68.5); x.stroke();
            const double sw = wind * 0.4;
            x.strokeStyle(leatherD); x.lineWidth(1.1); x.line(1.5, -58, 2 + sw, -45);
            x.strokeStyle(mix(pal.app, hex("#8a8a90"), 0.55)); x.lineWidth(1); x.begin();
            x.moveTo(0.2 + sw, -45.5); x.lineTo(3.8 + sw, -45.5); x.lineTo(4.4 + sw, -42.2); x.lineTo(-0.4 + sw, -42.2); x.close(); x.stroke();
            x.strokeStyle(rgba(FIRE, 0.5)); x.lineWidth(0.8); x.line(4.4 + sw, -45.3, 4.4 + sw, -42.4);
        }
        x.restore();
    }

    // o estandarte: pano em faixas com luz e sombra, rabo de andorinha, franja, o leão no losango
    void drawBanner(Cv& x, const Pal& pal)
    {
        const double bx = m_banner.x(), by = m_banner.y(), PH = 152, L = 58, Hh = 36, y0 = by - PH + 6, x0 = bx - 1, T = m_t;
        x.strokeStyle(mix(pal.app, hex("#2a1e14"), 0.5)); x.lineWidth(2.8); x.roundCaps(); x.line(bx, by + 2, bx - 2, by - PH);
        x.strokeStyle(rgba(FIRE, 0.35)); x.lineWidth(1); x.line(bx - 1.2, by, bx - 3, by - PH + 20);
        x.fillStyle(mix(pal.app, hex("#9a9aa0"), 0.5)); x.begin(); x.moveTo(bx - 5, by - PH); x.lineTo(bx - 2, by - PH - 14); x.lineTo(bx + 1, by - PH); x.fill();
        x.strokeStyle(mix(pal.app, hex("#8a7a50"), 0.5)); x.lineWidth(2); x.line(bx - 3, y0 - 1, bx + L * 0.9, y0 - 3 + std::sin(T * 3.2 - 3.6) * 2);
        auto wave = [&](double u) { return std::sin(T * 3.2 - u * 4.6) * 7.5 * u + std::sin(T * 5.3 - u * 8) * 2 * u; };
        auto dwave = [&](double u) { return (wave(u + 0.01) - wave(u - 0.01)) / 0.02; };
        auto P2 = [&](double u, double v) { return QPointF(x0 + u * L * (1 - 0.05 * std::abs(std::sin(T * 3.2 - u * 4.6))), y0 + v * Hh * (1 - 0.1 * u) + wave(u) + v * dwave(u) * 0.08); };
        auto cut = [](double u) { return u > 0.8 ? (u - 0.8) / 0.2 * 0.5 : 0.0; };
        const int N = 22;
        for (int k = 0; k < N; ++k) {
            const double u0 = double(k) / N, u1 = double(k + 1) / N, sl = dwave((u0 + u1) / 2), shade = qBound(-1.0, -sl / 14, 1.0);
            const QPointF a = P2(u0, cut(u0)), b = P2(u1, cut(u1)), cc = P2(u1, 1 - cut(u1)), d = P2(u0, 1 - cut(u0));
            const QColor col = shade > 0 ? mix(hex("#8a1a14"), hex("#c84030"), shade * 0.6) : mix(hex("#8a1a14"), hex("#3a0806"), -shade * 0.7);
            x.fillStyle(lin(0, a.y(), 0, d.y(), { { 0, mix(col, hex("#ffb060"), 0.12) }, { 1, mix(col, Qt::black, 0.25) } }));
            x.begin(); x.moveTo(a.x(), a.y()); x.lineTo(b.x() + 0.5, b.y()); x.lineTo(cc.x() + 0.5, cc.y()); x.lineTo(d.x(), d.y()); x.fill();
            if (cut(u0) < 0.5) {
                x.strokeStyle(rgba("#d8b060", 0.8)); x.lineWidth(1.3); x.begin(); x.moveTo(a.x(), a.y()); x.lineTo(b.x(), b.y()); x.moveTo(d.x(), d.y()); x.lineTo(cc.x(), cc.y()); x.stroke();
                x.lineWidth(0.7);
                for (int q = 0; q < 2; ++q) { const double fu = u0 + (u1 - u0) * q / 2; const QPointF fp = P2(fu, 1 - cut(fu)); x.line(fp.x(), fp.y(), fp.x() - 1 + std::sin(T * 6 + fu * 9), fp.y() + 4); }
            }
        }
        auto em = [&](std::initializer_list<QPointF> pts) { x.begin(); for (const QPointF& q : pts) { const QPointF r = P2(q.x(), q.y()); x.lineTo(r.x(), r.y()); } x.close(); };
        x.fillStyle(rgba("#d8b060", 0.9)); em({ { 0.3, 0.12 }, { 0.5, 0.5 }, { 0.3, 0.88 }, { 0.1, 0.5 } }); x.fill();
        x.fillStyle(mix(hex("#7a1612"), Qt::black, 0.2)); em({ { 0.3, 0.2 }, { 0.44, 0.5 }, { 0.3, 0.8 }, { 0.16, 0.5 } }); x.fill();
        x.fillStyle(rgba("#e8c070", 0.95)); em({ { 0.27, 0.68 }, { 0.25, 0.48 }, { 0.29, 0.36 }, { 0.27, 0.28 }, { 0.32, 0.31 }, { 0.34, 0.42 }, { 0.33, 0.55 }, { 0.36, 0.66 } }); x.fill();
        x.lighter(); x.fillStyle(lin(0, y0, 0, y0 + Hh + 10, { { 0, rgba(FIRE, 0) }, { 1, rgba(FIRE, 0.18) } }));
        em({ { 0, 0 }, { 0.82, 0 }, { 1, 0 }, { 0.82, 0.5 }, { 1, 1 }, { 0, 1 } }); x.fill(); x.over();
    }

    // o cavaleiro sentado de costas
    void drawKnight(Cv& x, const Pal& pal, double fl)
    {
        const double kx = m_knight.x(), ky = m_knight.y(), seat = ky - 25, br = std::sin(m_t * 0.9) * 0.6;
        const QColor col = mix(pal.app, hex("#08080c"), 0.55), cloak = mix(pal.app, hex("#1c0c0c"), 0.5);
        auto cloakPath = [&]() { x.begin(); x.moveTo(kx - 17, seat - 44 + br); x.bezTo(kx - 26, seat - 20, kx - 34, seat + 4, kx - 36, ky + 1); x.lineTo(kx + 30, ky + 1); x.bezTo(kx + 28, seat + 2, kx + 24, seat - 22, kx + 17, seat - 44 + br); x.close(); };
        x.fillStyle(cloak); cloakPath(); x.fill();
        x.strokeStyle(rgba("#000000", 0.35)); x.lineWidth(1.2);
        for (double dx : { -14.0, -4.0, 7.0, 16.0 }) { x.begin(); x.moveTo(kx + dx * 0.5, seat - 36); x.quadTo(kx + dx * 0.9, seat - 6, kx + dx * 1.5, ky); x.stroke(); }
        x.fillStyle(col); x.begin(); x.moveTo(kx - 18, seat - 42 + br); x.quadTo(kx, seat - 50 + br, kx + 18, seat - 42 + br); x.lineTo(kx + 14, seat - 20); x.lineTo(kx - 14, seat - 20); x.fill();
        for (int sg : { -1, 1 }) {
            x.fillStyle(mix(pal.app, hex("#3a3a44"), 0.5)); x.begin(); x.ellipse(kx + sg * 17, seat - 41 + br, 9, 5.5, sg * 0.35, 0, TAU); x.fill();
            x.fillStyle(rgba("#ffffff", 0.06)); x.begin(); x.ellipse(kx + sg * 17, seat - 43 + br, 6, 2, sg * 0.35, 0, TAU); x.fill();
        }
        x.fillStyle(col); x.fillRect(kx - 4, seat - 54 + br, 8, 8); x.begin(); x.ellipse(kx + 1, seat - 60 + br, 8, 9.5, 0.1, 0, TAU); x.fill();
        x.fillStyle(mix(pal.app, hex("#120c08"), 0.55)); x.begin(); x.ellipse(kx + 1, seat - 62 + br, 8.6, 9, 0.1, M_PI * 0.95, M_PI * 2.05); x.fill();
        x.strokeStyle(col); x.lineWidth(5); x.roundCaps(); x.begin(); x.moveTo(kx + 19, seat - 38); x.quadTo(kx + 26, seat - 22, kx + 22, seat - 12); x.stroke();
        x.lighter();
        x.fillStyle(lin(kx - 36, 0, kx + 10, 0, { { 0, rgba(FIRE, 0.32 * fl) }, { 1, rgba(FIRE, 0) } })); cloakPath(); x.fill();
        x.strokeStyle(rgba(FIRE, 0.45 * fl)); x.lineWidth(1.5);
        x.begin(); x.arc(kx + 1, seat - 60 + br, 9, 2.3, 4.0); x.stroke();
        x.begin(); x.ellipse(kx - 17, seat - 41 + br, 9, 5.5, -0.35, 2.4, 4.2); x.stroke();
        x.over();
    }

    QString m_key;
    QPixmap m_back, m_front, m_cloud;
    std::vector<Camp> m_camps;
    std::vector<QPointF> m_castLights;
    std::vector<Grass> m_grass;
    std::vector<Puff> m_sparks, m_smoke;
    Star m_star;
    Patrol m_patrol;
    QPointF m_fire, m_knight, m_horse, m_banner;
    double m_o = 0, m_edge = 345;
    double m_t = 0, m_tail = 0, m_cloudOff = 0, m_ear = 0, m_hoof = 0, m_nextHoof = 9, m_head = 0, m_nextNod = 4, m_nextStar = 10, m_nextPatrol = 3;
};


// ============================================================ Outpost
// Um planeta de gelo, plano e vazio: dois sóis se pondo atrás das montanhas, um gigante com anéis nascendo no céu
// (lavado pelo ar, nuvens passando na frente), e contra a luz uma cúpula pequena com a janela quente acesa, a antena
// girando, o veículo parado e o rastro de esteira sumindo no horizonte. No Foco, o casco de uma nave antiga
// enterrada no gelo. Vida: a neve correndo rente ao chão, os flocos no vento, a parabólica, a luz vermelha,
// a sombra de alguém passando na janela.
class Outpost : public Layer {
public:
    void init(Ctx&) override { m_key.clear(); }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_dish += dt * 0.5;
        if (m_key.isEmpty()) return;
        for (Flake& f : m_flakes) {
            f.x -= (90 + 160 * f.z) * dt;
            f.y += (8 + std::sin(m_t * 2 + f.ph) * 12) * dt * f.z;
            if (f.x < -10) { f.x = c.W + 10; f.y = R(0, c.H); }
            if (f.y > c.H) f.y = 0;
        }
        for (Snake& s : m_snakes) {
            s.x -= s.v * dt;
            if (s.x + s.l < -10) { s.x = c.W + R(0, 200); s.y = HZ + std::pow(rnd(), 1.2) * (c.H - HZ); }
        }
    }
    void paint(QPainter& painter, const Pal& pal, Ctx& c) override
    {
        const QString key = sceneKey(pal, c);
        if (key != m_key) {
            Seeded sd(0x0B7905u);
            build(pal, c);
            m_key = key;
        }
        Cv x(painter);
        x.image(m_back, 0, 0);
        for (const Snake& s : m_snakes) {
            const double d = (s.y - HZ) / (c.H - HZ);
            x.fillStyle(lin(s.x, 0, s.x + s.l * (0.4 + d), 0, { { 0, rgba("#eef2ff", s.a) }, { 1, rgba("#eef2ff", 0) } }));
            x.fillRect(s.x, s.y - 1 - d * 2, s.l * (0.4 + d), 1 + d * 2.5);
        }
        x.image(m_front, 0, 0);
        const double dx = m_dome.x(), dy = m_dome.y(), r = 56;
        {
            const double wx = dx - 14, wy = dy - 22, who = std::sin(m_t * 0.13) > 0.75 ? std::sin(m_t * 0.13 * 6) : 0;
            x.fillStyle(rad(wx, wy, 0, wx, wy, 9, { { 0, hex("#fff0c8") }, { 1, hex("#e8903a") } })); x.begin(); x.arc(wx, wy, 9, 0, TAU); x.fill();
            if (who != 0) { x.save(); x.begin(); x.arc(wx, wy, 9, 0, TAU); x.clip(); x.fillStyle(rgba("#3a1a0a", 0.7)); x.begin(); x.ellipse(wx + who * 6, wy + 2, 4, 8, 0, 0, TAU); x.fill(); x.restore(); }
            x.lighter(); glowAt(x, wx, wy, 70, WARM, 0.22); x.save(); x.translate(wx - 10, dy + 6); x.scale(1, 0.25); glowAt(x, 0, 0, 80, WARM, 0.25); x.restore(); x.over();
        }
        {
            const double tx = dx + 30, ty = dy - r - 46, cs = std::cos(m_dish), w = 14 * std::abs(cs) + 2;
            x.fillStyle(mix(pal.app, hex("#8a94b0"), 0.5)); x.begin(); x.ellipse(tx + cs * 3, ty + 6, w, 12, 0.2, 0, TAU); x.fill();
            x.fillStyle(cs > 0 ? rgba(SUN, 0.4) : rgba("#000000", 0.3)); x.begin(); x.ellipse(tx + cs * 3, ty + 6, w * 0.6, 8, 0.2, 0, TAU); x.fill();
            x.strokeStyle(mix(pal.app, hex("#1a2030"), 0.5)); x.lineWidth(1); x.line(tx + cs * 3, ty + 6, tx + cs * 12, ty + 2);
            const double on = std::sin(m_t * 2.4) > 0.75 ? 1 : 0.1;
            x.lighter(); glowAt(x, tx, ty - 4, 12, "#ff2a1a", 0.8 * on); x.over();
            x.fillStyle(rgba("#ff6050", on)); x.fillRect(tx - 1.2, ty - 5.2, 2.4, 2.4);
        }
        for (const Flake& f : m_flakes) {
            const double l = 4 + f.z * 14;
            x.fillStyle(lin(f.x, 0, f.x + l, 0, { { 0, rgba("#f4f6ff", 0.15 + 0.4 * f.z) }, { 1, rgba("#f4f6ff", 0) } }));
            x.fillRect(f.x, f.y, l, qMax(0.7, f.z * 1.4));
        }
    }

private:
    static constexpr double HZ = 470;
    const QColor SUN = hex("#ff9a50"), WARM = hex("#ffc070");
    struct Flake { double x, y, z, ph; };
    struct Snake { double x, y, l, v, a; };

    void build(const Pal& pal, const Ctx& c)
    {
        const double W = c.W, H = c.H;
        const QPointF s1(zx(c, 318), HZ - 26), s2(zx(c, 356), HZ - 92);
        m_dome = QPointF(zx(c, 190), 572);
        m_back = layer(c, W, H);
        {
            QPainter qp(&m_back);
            Cv x(qp);
            x.fillStyle(lin(0, 0, 0, HZ, { { 0, mix(pal.app, hex("#0a0c22"), 0.45) }, { 0.45, mix(pal.app, hex("#2a2450"), 0.4) }, { 0.8, mix(pal.app, hex("#7a4a6a"), 0.35) }, { 1, mix(pal.app, hex("#d8865a"), 0.3) } }));
            x.fillRect(0, 0, W, HZ + 4);
            for (int i = 0; i < 260; ++i) { const double sy = std::pow(rnd(), 1.6) * HZ * 0.6; x.fillStyle(rgba("#ffffff", R(0.1, 0.5) * (1 - sy / (HZ * 0.6)))); x.fillRect(R(0, W), sy, R(0.6, 1.3), R(0.6, 1.3)); }
            // o gigante com anéis, numa camada à parte: tingido com o céu e composto translúcido
            const double pcx = zx(c, 182), pcy = HZ - 108, pr = 84, tilt = -0.2;
            QPixmap pl = layer(c, W, H);
            {
                QPainter pq(&pl);
                Cv y(pq);
                auto ring = [&](double a0, double a1) {
                    y.save(); y.translate(pcx, pcy); y.rotate(tilt);
                    for (double k = 1.3; k <= 2.15; k += 0.012) {
                        const bool gap = k > 1.66 && k < 1.72;
                        const double dens = gap ? 0 : (0.35 + 0.35 * std::sin(k * 23) * std::sin(k * 7)) * (k < 1.42 ? (k - 1.3) / 0.12 : k > 2.0 ? (2.15 - k) / 0.15 : 1);
                        if (dens <= 0.02) continue;
                        y.strokeStyle(rgba(k < 1.66 ? "#e8dcb8" : "#cbbc98", qMin(0.75, dens))); y.lineWidth(1.4);
                        y.begin(); y.ellipse(0, 0, pr * k, pr * k * 0.17, 0, a0, a1); y.stroke();
                    }
                    y.restore();
                };
                ring(M_PI, 2 * M_PI);
                y.save(); y.begin(); y.arc(pcx, pcy, pr, 0, TAU); y.clip();
                y.fillStyle(lin(pcx - pr, pcy - pr, pcx + pr * 0.7, pcy + pr * 0.5, { { 0, hex("#5a5048") }, { 0.55, hex("#d8c498") }, { 1, hex("#f0e0b8") } })); y.fillRect(pcx - pr, pcy - pr, pr * 2, pr * 2);
                y.save(); y.translate(pcx, pcy); y.rotate(tilt);
                for (int k = -9; k <= 9; ++k) {
                    const double yy = k * pr / 9, w = std::sqrt(qMax(0.0, pr * pr - yy * yy));
                    const bool third = k % 3 == 0;
                    y.fillStyle(rgba(third ? "#8a6a48" : (k % 2 ? "#fff4d8" : "#b89870"), third ? 0.28 : 0.14));
                    y.begin(); y.ellipse(0, yy, w, pr / 22 + (third ? 3 : 0), 0, 0, TAU); y.fill();
                }
                y.fillStyle(rgba("#1a1420", 0.35)); y.begin(); y.ellipse(0, pr * 0.2, pr * 1.2, 5, 0, 0, TAU); y.fill();
                y.restore();
                y.fillStyle(rad(pcx + pr * 0.55, pcy + pr * 0.35, pr * 0.2, pcx + pr * 0.3, pcy + pr * 0.2, pr * 1.55, { { 0, rgba("#000000", 0) }, { 0.7, rgba("#000000", 0.25) }, { 1, rgba("#0a0c22", 0.75) } }));
                y.fillRect(pcx - pr, pcy - pr, pr * 2, pr * 2);
                y.restore();
                ring(0, M_PI);
                y.fillStyle(rgba(mix(pal.app, hex("#2a2450"), 0.4), 0.3)); y.begin(); y.arc(pcx, pcy, pr + 0.5, 0, TAU); y.fill();
                {   // a névoa do horizonte, com as laterais esfumadas (na mesa larga o corte reto aparecia)
                    QPixmap hz = layer(c, W, H);
                    QPainter hq(&hz);
                    Cv h(hq);
                    const double hx0 = pcx - pr * 2.3, hw = pr * 4.6;
                    h.fillStyle(vgrad(pcy + 10, HZ + 2, rgba("#7a4a6a", 0), rgba("#9a6070", 0.6))); h.fillRect(hx0, pcy + 10, hw, HZ + 2 - pcy - 10);
                    h.destOut(); h.fillStyle(lin(hx0, 0, hx0 + hw, 0, { { 0, rgba("#000000", 1) }, { 0.3, rgba("#000000", 0) }, { 0.7, rgba("#000000", 0) }, { 1, rgba("#000000", 1) } })); h.fillRect(hx0, pcy + 10, hw, HZ + 2 - pcy - 10);
                    hq.end();
                    y.image(hz, 0, 0);
                }
                y.lighter(); y.strokeStyle(rgba("#ffd8a0", 0.18)); y.lineWidth(2); y.begin(); y.arc(pcx, pcy, pr, -0.9, 0.5); y.stroke(); y.over();
                y.atop(); y.fillStyle(vgrad(pcy - pr, HZ, rgba(mix(pal.app, hex("#3a3060"), 0.4), 0.42), rgba("#a86a78", 0.62))); y.fillRect(0, 0, W, H); y.over();
            }
            x.lighter(); glowAt(x, pcx, pcy, pr * 1.5, "#c8a0c0", 0.08); x.over();
            x.alphaG(0.72); x.image(pl, 0, 0); x.alphaG(1);
            const QPixmap cd = softDot(mix(pal.app, hex("#3a2c48"), 0.35), 0.35), cl = softDot(hex("#ffb090"), 0.3);
            for (int i = 0; i < 26; ++i) {
                const double cx = pcx + R(-pr * 2.4, pr * 2.4), cy = pcy + R(pr * 0.15, pr * 1.1), w = R(70, 190), h = w * R(0.12, 0.24);
                x.alphaG(R(0.5, 0.85)); x.image(cd, cx - w / 2, cy - h / 2, w, h);
                x.alphaG(R(0.1, 0.22)); x.image(cl, cx - w * 0.4, cy + h * 0.15, w * 0.8, h * 0.4);
            }
            for (int i = 0; i < 6; ++i) { const double cx = pcx + R(-pr * 2, pr * 2), cy = pcy + R(-pr * 0.7, pr * 0.1), w = R(140, 260); x.alphaG(R(0.22, 0.4)); x.image(cd, cx - w / 2, cy - 3, w, 6); }
            x.alphaG(1);
            // os sóis
            x.lighter(); glowAt(x, s1.x(), s1.y(), 420, SUN, 0.25); glowAt(x, s1.x(), s1.y(), 120, "#ffc890", 0.35); glowAt(x, s2.x(), s2.y(), 80, "#e8f0ff", 0.3); x.over();
            x.fillStyle(hex("#ffd8a8")); x.begin(); x.arc(s1.x(), s1.y(), 30, 0, TAU); x.fill();
            x.fillStyle(hex("#ffffff")); x.begin(); x.arc(s2.x(), s2.y(), 8, 0, TAU); x.fill();
            { const QPixmap cd2 = softDot(hex("#ffb88a"), 0.3); for (int i = 0; i < 10; ++i) { const double cx = s1.x() + R(-300, 300), cy = HZ - R(70, 160), w = R(140, 300); x.alphaG(R(0.1, 0.2)); x.image(cd2, cx - w / 2, cy - 4, w, 8); } x.alphaG(1); }
            // montanhas de gelo
            const struct { double amp, base; const char* col; double rimA; } MT[2] = { { 46, HZ - 2, "#4a4a6a", 0.35 }, { 24, HZ + 2, "#2e3450", 0.25 } };
            for (const auto& m : MT) {
                const Profile pr2 = profile(9, 0.6), pk = profile(5, 0.7);
                // um vale na frente do sol grande: no Lab o sorteio às vezes escondia o pôr do sol atrás de um pico
                auto ridge = [&](double xx) { const double v = 0.4 + 0.6 * qMin(1.0, std::abs(xx - s1.x()) / 150); return m.base - v * (m.amp * (0.35 + 0.65 * std::abs(pk.at(xx / W * 1.3))) + pr2.at(xx / W) * m.amp * 0.35); };
                x.fillStyle(mix(pal.app, hex(m.col), 0.4)); x.begin(); x.moveTo(-10, HZ + 6); for (double xx = -10; xx <= W + 10; xx += 3) x.lineTo(xx, ridge(xx)); x.lineTo(W + 10, HZ + 6); x.fill();
                x.strokeStyle(rgba(SUN, m.rimA)); x.lineWidth(1.2); x.begin(); for (double xx = -10; xx <= W + 10; xx += 3) x.lineTo(xx, ridge(xx)); x.stroke();
                x.fillStyle(vgrad(m.base - m.amp, HZ + 6, rgba("#c8a0b0", 0), rgba("#c8a0b0", 0.12))); x.fillRect(0, m.base - m.amp, W, HZ + 6 - m.base + m.amp);
            }
            // o gelo e o reflexo dos sóis
            x.fillStyle(vgrad(HZ, H, mix(pal.app, hex("#9aa0c0"), 0.3), mix(pal.app, hex("#2a3450"), 0.4))); x.fillRect(0, HZ, W, H - HZ);
            x.lighter(); x.save(); x.translate(s1.x(), HZ + 10); x.scale(1, 0.18); glowAt(x, 0, 0, 260, SUN, 0.35); x.restore();
            for (int i = 0; i < 160; ++i) { const double d = std::pow(rnd(), 1.3), yy = HZ + 4 + d * 200, w = R(4, 30) * (1 + d), xx = s1.x() + R(-1, 1) * (20 + d * 90); x.fillStyle(rgba("#ffc8a0", R(0.05, 0.18) * (1 - d * 0.7))); x.fillRect(xx - w / 2, yy, w, 1 + d); }
            x.over();
            // sastrugi
            const int nS = int(900 * W / 1440.0);
            for (int i = 0; i < nS; ++i) {
                const double d = std::pow(rnd(), 1.4), yy = HZ + 4 + d * (H - HZ), xx = R(-20, W + 20), l = 4 + d * 40, a = R(0.05, 0.14);
                x.strokeStyle(rgba("#000000", a * 1.2)); x.lineWidth(0.6 + d); x.begin(); x.moveTo(xx, yy); x.quadTo(xx + l / 2, yy - 1 - d * 2, xx + l, yy); x.stroke();
                x.strokeStyle(rgba("#ffd8c0", a)); x.begin(); x.moveTo(xx, yy - 0.8 - d); x.quadTo(xx + l / 2, yy - 2 - d * 3, xx + l, yy - 0.8 - d); x.stroke();
            }
            // o rastro da esteira
            const double dx = m_dome.x(), dy = m_dome.y();
            auto track = [&](double t) { const double u = 1 - t; return QPointF(dx + 70 + std::sin(t * 3.2) * 70 * u - t * 40, dy + 10 - std::pow(t, 0.55) * (dy + 10 - HZ - 2)); };
            for (int off : { -1, 1 }) {
                x.strokeStyle(rgba("#141a2c", 0.4)); x.lineWidth(0.5); x.begin();
                for (int k = 0; k <= 80; ++k) { const double t = k / 80.0, w = (1 - std::pow(t, 0.4)) * 7 + 0.4; const QPointF q = track(t); x.lineTo(q.x() + off * w, q.y()); }
                x.stroke();
            }
            // o casco da nave antiga enterrada (à direita; aparece no Foco)
            {
                const double cx = W - 250, cy = HZ + 46;
                x.fillStyle(mix(pal.app, hex("#2a3040"), 0.45)); x.begin(); x.moveTo(cx - 130, cy + 6); x.quadTo(cx - 60, cy - 46, cx + 60, cy - 36); x.lineTo(cx + 120, cy - 20); x.lineTo(cx + 140, cy + 8); x.close(); x.fill();
                x.strokeStyle(rgba("#000000", 0.3)); x.lineWidth(1); for (int k = 0; k < 9; ++k) { const double xx = cx - 100 + k * 26; x.line(xx, cy + 4, xx + 6, cy - 30 + std::abs(k - 4) * 3); }
                x.fillStyle(rgba(SUN, 0.3)); x.begin(); x.moveTo(cx - 60, cy - 46); x.quadTo(cx, cy - 46, cx + 60, cy - 36); x.lineTo(cx + 56, cy - 32); x.quadTo(cx, cy - 40, cx - 60, cy - 42); x.fill();
                x.fillStyle(mix(pal.app, hex("#dfe6f0"), 0.6)); x.begin(); x.moveTo(cx - 140, cy + 10); x.quadTo(cx, cy - 8, cx + 150, cy + 12); x.lineTo(cx + 150, cy + 14); x.lineTo(cx - 140, cy + 14); x.fill();
                x.strokeStyle(mix(pal.app, hex("#2a3040"), 0.45)); x.lineWidth(3); x.line(cx + 40, cy - 36, cx + 64, cy - 90);
            }
        }
        // a cúpula e o que tem em volta
        m_front = layer(c, W, H);
        {
            QPainter qp(&m_front);
            Cv f(qp);
            const QColor metal = mix(pal.app, hex("#4a5470"), 0.45), dark = mix(pal.app, hex("#1a2030"), 0.5);
            const double dx = m_dome.x(), dy = m_dome.y(), cx = dx, cy = dy, r = 56;
            f.fillStyle(rgba("#141a30", 0.45)); f.begin(); f.ellipse(dx - 70, dy + 4, 120, 9, 0.02, 0, TAU); f.fill();
            f.fillStyle(vgrad(dy - 34, dy, mix(metal, hex("#ffb080"), 0.1), dark)); f.begin(); f.moveTo(dx - 120, dy); f.lineTo(dx - 120, dy - 26); f.quadTo(dx - 120, dy - 34, dx - 110, dy - 34); f.lineTo(dx - 40, dy - 34); f.lineTo(dx - 40, dy); f.fill();
            f.strokeStyle(rgba("#000000", 0.3)); f.lineWidth(1); for (double xx : { dx - 100, dx - 80, dx - 60 }) f.line(xx, dy - 34, xx, dy);
            f.fillStyle(dark); f.fillRect(dx - 96, dy - 24, 12, 24); f.fillStyle(rgba(WARM, 0.8)); f.fillRect(dx - 93, dy - 21, 6, 4);
            f.fillStyle(rad(cx - r * 0.4, cy - r * 0.5, 4, cx, cy, r * 1.2, { { 0, mix(metal, hex("#c8d0e8"), 0.25) }, { 0.7, metal }, { 1, dark } }));
            f.begin(); f.arc(cx, cy, r, M_PI, 0); f.close(); f.fill();
            f.save(); f.begin(); f.arc(cx, cy, r, M_PI, 0); f.close(); f.clip();
            f.strokeStyle(rgba("#000000", 0.25)); f.lineWidth(1);
            for (int k = 1; k < 6; ++k) { const double a = k / 6.0; f.begin(); f.ellipse(cx, cy, std::abs(r * std::cos(a * M_PI / 2 + M_PI / 2)) + 0.01, r, 0, M_PI, 0); f.stroke(); }
            for (int k = 1; k < 4; ++k) { const double yy = cy - r * std::sin(k / 4.0 * M_PI / 2); f.line(cx - r, yy, cx + r, yy); }
            f.fillStyle(lin(cx, 0, cx + r, 0, { { 0.6, rgba(SUN, 0) }, { 1, rgba(SUN, 0.45) } })); f.fillRect(cx, cy - r, r, r);
            f.restore();
            f.fillStyle(dark); f.begin(); f.arc(cx - 14, cy - 22, 11, 0, TAU); f.fill();
            f.fillStyle(mix(pal.app, hex("#c8d0e0"), 0.55)); f.begin(); f.moveTo(dx - 130, dy + 3); f.quadTo(dx - 80, dy - 9, dx - 30, dy - 4); f.quadTo(dx + 10, dy - 12, dx + 64, dy + 3); f.close(); f.fill();
            f.strokeStyle(dark); f.lineWidth(2.5); f.line(cx + 26, cy - r * 0.84, cx + 30, cy - r - 46);
            f.lineWidth(1); f.begin(); f.moveTo(cx + 30, cy - r - 46); f.lineTo(cx + 10, cy - r + 6); f.moveTo(cx + 30, cy - r - 46); f.lineTo(cx + 50, cy - r + 12); f.stroke();
            for (int k = 0; k < 3; ++k) {
                const double px = dx + 84 + k * 22, py = dy - 2;
                f.strokeStyle(dark); f.lineWidth(1.5); f.line(px, py, px, py - 14);
                f.fillStyle(mix(pal.app, hex("#2a3a6a"), 0.4)); f.begin(); f.moveTo(px - 10, py - 14); f.lineTo(px + 10, py - 18); f.lineTo(px + 10, py - 8); f.lineTo(px - 10, py - 4); f.fill();
                f.fillStyle(rgba(SUN, 0.35)); f.begin(); f.moveTo(px - 10, py - 14); f.lineTo(px + 10, py - 18); f.lineTo(px + 10, py - 16); f.lineTo(px - 10, py - 12); f.fill();
            }
            {
                const double vx = dx + 70, vy = dy + 14;
                f.fillStyle(dark); f.begin(); f.moveTo(vx - 26, vy); f.lineTo(vx - 24, vy - 16); f.lineTo(vx + 6, vy - 16); f.lineTo(vx + 14, vy - 26); f.lineTo(vx + 28, vy - 26); f.lineTo(vx + 32, vy - 12); f.lineTo(vx + 32, vy); f.fill();
                f.fillStyle(mix(pal.app, hex("#5a6a8a"), 0.4)); f.begin(); f.moveTo(vx + 16, vy - 24); f.lineTo(vx + 26, vy - 24); f.lineTo(vx + 29, vy - 14); f.lineTo(vx + 13, vy - 14); f.fill();
                f.fillStyle(hex("#0a0c14")); f.begin(); f.path.addRoundedRect(QRectF(vx - 28, vy - 4, 62, 10), 5, 5); f.fresh = false; f.fill();
                f.fillStyle(mix(pal.app, hex("#3a4058"), 0.5)); for (int k = 0; k < 6; ++k) { f.begin(); f.arc(vx - 22 + k * 10, vy + 1, 3, 0, TAU); f.fill(); }
                f.fillStyle(rgba(SUN, 0.4)); f.fillRect(vx + 28, vy - 25, 3, 13);
            }
        }
        m_flakes.clear();
        for (int i = 0; i < 90; ++i) m_flakes.push_back({ R(0, W), R(0, H), R(0.3, 1), R(0, 6.28) });
        m_snakes.clear();
        for (int i = 0; i < int(70 * W / 1440.0); ++i) m_snakes.push_back({ R(0, W), HZ + std::pow(rnd(), 1.2) * (H - HZ), R(30, 120), R(30, 90), R(0.08, 0.22) });
    }

    QString m_key;
    QPixmap m_back, m_front;
    QPointF m_dome;
    std::vector<Flake> m_flakes;
    std::vector<Snake> m_snakes;
    double m_t = 0, m_dish = 0;
};

} // namespace

std::unique_ptr<Layer> makeGenresA(const QString& id)
{
    if (id == QLatin1String("outpost")) return std::make_unique<Outpost>();
    if (id == QLatin1String("evebattle")) return std::make_unique<EveBattle>();
    return nullptr;
}

} // namespace AnimBg
