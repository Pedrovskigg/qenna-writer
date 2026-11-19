// Tema Animado "Sunset in VIce" (cena oceandrive, 18/11/2026): Miami nos anos 80, a baía vista do alto no pôr do sol.
// Portado de oceandrive.js do Weather Lab, linha a linha pela camada Cv. A receita é a das cidades com volume:
// fileiras de prédios de frente, uma atrás da outra, cada uma menor, mais apagada na névoa e terminando mais à
// esquerda, seguindo a curva da orla. Cada prédio é um bloco em 2D (frente, telhado e lateral puxados até o ponto de
// fuga), o "3D mentiroso" que o Pe riscou. A água ocupa o resto e reflete cada fileira. Por quadro: o brilho do sol na
// água, as luzes de aviso, o navio, os barcos fazendo a curva, o neon dos hotéis da frente, os faróis da rua da frente,
// a perseguição pela avenida da orla (carro fugindo, viaturas, dois helicópteros, tiro), as gaivotas e o aviãozinho
// com a faixa, esses dois voando dentro da perspectiva.
//
// A cena foi desenhada no Lab numa mesa de 1440 × 758 com a coluna da esquerda começando em x 45. Aqui ela é
// deslocada (ox) pra essa coluna cair no pedaço de mesa à vista, e a água se estende até a borda direita (WR).

#include "AnimCanvas.h"

#include <QImage>
#include <QLinearGradient>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>

namespace AnimBg {
namespace {

using namespace cv;

class OceanDrive : public Layer {
public:
    void init(Ctx& c) override
    {
        m_key.clear();
        m_chase = Chase(); m_hasChase = false; m_nextChase = 6; m_shots.clear();
        m_boats.clear(); m_nextBoat = 1;
        m_hasPlane = false; m_nextPlane = 2;
        m_gulls.clear(); m_nextGull = 0.5;
        m_hasShip = false;
        m_cars.clear();
        const int n = qMax(1, int(std::lround(6 * INT(c))));
        for (int i = 0; i < n; ++i) m_cars.push_back({ rnd() < 0.5 ? 1 : -1, R(0, 800), R(70, 120) });
    }

    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        const double T = m_t;
        updateBounds(c);
        // a perseguição: começa, anda, troca tiro e some
        if (!m_hasChase) { m_nextChase -= dt; if (m_nextChase <= 0) startChase(); }
        if (m_hasChase) {
            m_chase.lead *= std::exp(-m_chase.dir * m_chase.v * dt);
            if (rnd() < dt * 4) {
                const int nc = int(m_chase.cars.size());
                const int i = rnd() < 0.4 ? 0 : 1 + RI(nc - 1);
                const int j = i == 0 ? 1 : (rnd() < 0.7 ? 0 : qMax(0, i - 1));
                m_shots.push_back({ i, j, 0 });
            }
            bool gone = true;
            for (const ChaseCar& cc : m_chase.cars) {
                const QPointF q = chasePos(cc, T);
                const double sc = chaseScale(cc);
                if (m_chase.dir > 0 ? q.y() <= kH + 30 : sc >= 0.02) { gone = false; break; }
            }
            if (gone) { m_hasChase = false; m_shots.clear(); m_nextChase = R(25, 50); }
        }
        for (Shot& s : m_shots) s.t += dt;
        m_shots.erase(std::remove_if(m_shots.begin(), m_shots.end(), [](const Shot& s) { return s.t >= 0.09; }), m_shots.end());
        // os faróis na rua da frente
        const double xe = xc(ROADY + 10);
        for (Car& cr : m_cars) { cr.x += cr.dir * cr.v * dt; if (cr.x > xe + 30) cr.x = -40; if (cr.x < -40) cr.x = xe + 30; }
        // barcos: todo barco entra pela direita, vai até perto da orla, faz a curva e volta até sumir na borda
        m_nextBoat -= dt;
        if (m_nextBoat <= 0 && m_boats.size() < 5) {
            const double y = HZ + 14 + std::pow(rnd(), 1.2) * (kH - HZ - 40), s = sOf(y);
            const bool fast = rnd() < 0.35;
            Boat b;
            b.y = b.y0 = y; b.s = s; b.dir = -1; b.face = -1; b.tp = -1;
            b.turnX = xc(y) + 30 + R(60, 260) * (0.3 + s);
            b.x = m_wr + 60; b.sp = (fast ? 160 : 70) * s + 6; b.L = 10 + 70 * s;
            b.sail = fast ? false : rnd() < 0.35;
            m_boats.push_back(b);
            m_nextBoat = R(3, 7);
        }
        for (Boat& b : m_boats) {
            if (b.tp < 0 && b.dir < 0 && b.x <= b.turnX) b.tp = 0;
            if (b.tp >= 0 && b.tp < 1) {
                b.tp = qMin(1.0, b.tp + dt / 2.2);
                const double k = M_PI * b.tp;
                b.face = -std::cos(k);
                b.x += b.face * b.sp * dt * 0.8;
                b.y = b.y0 + std::sin(k) * (4 + 26 * b.s);
                if (b.tp >= 1) { b.dir = 1; b.face = 1; }
            } else b.x += b.dir * b.sp * dt;
            b.trail.push_back({ b.x, b.y, 0 });
            for (TrailPt& q : b.trail) q.t += dt;
            b.trail.erase(std::remove_if(b.trail.begin(), b.trail.end(), [](const TrailPt& q) { return q.t >= 6; }), b.trail.end());
        }
        const double wr = m_wr;
        m_boats.erase(std::remove_if(m_boats.begin(), m_boats.end(), [wr](const Boat& b) { return (b.dir > 0 && b.x > wr + 80) || b.x <= -100; }), m_boats.end());
        // o navio de cruzeiro lá longe
        if (!m_hasShip) { m_hasShip = true; m_shipX = m_wr - 60; }
        m_shipX -= 2.2 * dt;
        if (m_shipX < m_wr * 0.62) m_shipX = m_wr + 40;
        // o aviãozinho
        m_nextPlane -= dt;
        if (!m_hasPlane && m_nextPlane <= 0) newPlane();
        if (m_hasPlane) {
            m_plane.t += dt / m_plane.dur;
            m_plane.hist.push_back(bez(qMin(1.0, m_plane.t)));
            if (m_plane.hist.size() > 900) m_plane.hist.erase(m_plane.hist.begin(), m_plane.hist.begin() + 300);
            if (m_plane.t > 1.08) { m_hasPlane = false; m_nextPlane = R(30, 50); }
        }
        // gaivotas
        m_nextGull -= dt;
        if (m_nextGull <= 0) { spawnFlock(); m_nextGull = R(6, 13); }
        for (Gull& g : m_gulls) {
            g.turn += dt * 0.35;
            g.h += std::sin(g.turn) * 0.35 * dt;
            g.X += std::cos(g.h) * g.v * dt;
            g.D = qMax(1.05, g.D + std::sin(g.h) * g.v * dt);
            g.A += std::sin(T * 0.8 + g.ph) * 0.004 * dt;
            const double glide = std::sin(T * 0.4 + g.ph) > 0.1 ? 1 : 0;
            g.flap += (glide - g.flap) * qMin(1.0, dt * 2);
            g.ph += dt * 7;
        }
        m_gulls.erase(std::remove_if(m_gulls.begin(), m_gulls.end(), [this](const Gull& g) {
            const auto q = SKP(g.X, g.D, g.A);
            return !(q[0] > -90 && q[0] < m_wr + 90 && g.D < 30 && q[2] > 0.03);
        }), m_gulls.end());
    }

    void paint(QPainter& painter, const Pal& pal, Ctx& c) override
    {
        updateBounds(c);
        const QString key = QString::number(m_ox, 'f', 1) + QLatin1Char('|') + QString::number(c.W, 'f', 0) + QLatin1Char('|') + QString::number(c.px, 'f', 2) + pal.app.name();
        if (key != m_key) {
            Seeded sd(0x0CEA4D5u);
            build(c);
            m_key = key;
        }
        const double T = m_t;
        painter.save();
        Cv x(painter);
        x.image(m_back, 0, 0);
        x.translate(m_ox, 0);
        // brilho do sol picando na água
        x.lighter();
        for (const Glint& g : m_glints) {
            const double k = sOf(g.y), hw = 10 + k * 170, gx = SUNX + std::sin(g.ph * 3.1) * hw * 0.8 + g.dx * 6, a = qMax(0.0, std::sin(T * g.s + g.ph));
            if (a < 0.4 || !isWater(gx, g.y)) continue;
            x.fillStyle(rgba("#fff0c0", (a - 0.4) * 1.3 * (1 - k * 0.4)));
            const double w = 2 + k * 16;
            x.fillRect(gx - w / 2, g.y, w, 0.8 + k * 1.4);
        }
        for (const Blink& b : m_blinks) { const double on = std::sin(T * 2.2 + b.ph) > 0.75 ? 1 : 0.1; dot(x, b.x, b.y, b.r, hex("#ff2a2a"), 0.8 * on); }
        x.over();
        // o navio de cruzeiro, todo aceso
        if (m_hasShip) {
            const double sx = m_shipX, sy = HZ + 26, L = 90, Hh = 4;
            x.fillStyle(hex("#f2e6ee")); x.begin(); x.moveTo(sx - L / 2, sy - Hh * 0.6); x.lineTo(sx + L / 2, sy - Hh); x.lineTo(sx + L / 2 - 6, sy + Hh * 0.3); x.lineTo(sx - L / 2 + 2, sy + Hh * 0.3); x.close(); x.fill();
            x.fillStyle(hex("#e8dce6")); x.fillRect(sx - L * 0.36, sy - Hh * 1.7, L * 0.7, Hh * 1.1); x.fillRect(sx - L * 0.26, sy - Hh * 2.5, L * 0.48, Hh * 0.9);
            x.fillStyle(PINK); x.fillRect(sx - L * 0.14, sy - Hh * 3.4, 5, Hh);
            x.lighter();
            for (int k = 0; k < 24; ++k) dot(x, sx - L * 0.34 + k * L * 0.028, sy - Hh * 1.2, 1.4, k % 3 ? hex("#ffd8a0") : CYAN, std::sin(T * 1.5 + k * 1.3) > -0.6 ? 0.8 : 0.3);
            x.fillStyle(rgba("#ffd8a0", 0.15)); x.fillRect(sx - L * 0.35, sy + 2, L * 0.7, 6);
            x.over();
        }
        // barcos com rastro
        for (const Boat& b : m_boats) {
            const double s = b.s;
            x.strokeStyle(rgba("#ffe8f0", 0.4)); x.lineWidth(qMax(0.6, 1.4 * s));
            for (int sg : { -1, 1 }) {
                x.begin();
                bool first = true;
                for (const TrailPt& q : b.trail) { const double yy = q.y + 2 * s + sg * q.t * 3 * s; if (first) { x.moveTo(q.x, yy); first = false; } else x.lineTo(q.x, yy); }
                x.stroke();
            }
            x.fillStyle(rgba("#ffffff", 0.35));
            for (const TrailPt& q : b.trail) if (q.t < 0.8) x.fillRect(q.x, q.y + R(-1, 2) * s * 2, qMax(1.0, 3 * s), qMax(0.6, 1.2 * s));
            const double face = std::abs(b.face) < 0.15 ? (b.face < 0 ? -0.15 : 0.15) : b.face;
            drawYacht(x, b.x, b.y, b.L, face, b.sail);
        }
        x.translate(-m_ox, 0);
        // as fileiras da frente
        x.image(m_front, 0, 0);
        x.translate(m_ox, 0);
        for (int i = 0; i < int(m_signs.size()); ++i) {
            const double on = std::sin(T * 0.8 + i * 2.3) > 0.96 && rnd() < 0.5 ? 0.15 : 1;
            drawSign(x, m_signs[i], on);
        }
        // faróis na rua da frente
        x.lighter();
        for (const Car& cr : m_cars) {
            const double y = ROADY + (cr.dir > 0 ? 10 : 2), len = cr.dir > 0 ? 46 : 14;
            const QColor col = hex(cr.dir > 0 ? "#fff4d6" : "#ff4a3a");
            x.fillStyle(QBrush(lin(cr.x, 0, cr.x + cr.dir * len, 0, { { 0, alpha(col, 0.4) }, { 1, alpha(col, 0) } })));
            x.fillRect(qMin(cr.x, cr.x + cr.dir * len), y - 2, len, 4);
            x.fillStyle(alpha(col, 0.95)); x.fillRect(cr.x - 1.5, y - 1.5, 3, 3);
        }
        x.over();
        // a perseguição por cima de tudo (a avenida está livre de prédio)
        if (m_hasChase) {
            for (const Heli& h : m_chase.helis) drawHeliSpot(x, h, T);
            for (int i = int(m_chase.cars.size()) - 1; i >= 0; --i) drawChaseCar(x, m_chase.cars[i], T);
            for (const Heli& h : m_chase.helis) drawHeliBody(x, h, T);
            x.save(); x.lighter();
            for (const Shot& q : m_shots) {
                if (q.i >= int(m_chase.cars.size()) || q.j >= int(m_chase.cars.size())) continue;
                const QPointF A = chasePos(m_chase.cars[q.i], T), B = chasePos(m_chase.cars[q.j], T);
                const double sc = chaseScale(m_chase.cars[q.i]);
                dot(x, A.x(), A.y() - 2 * sc, 2 + 9 * sc, hex("#fff0a0"), 0.9);
                dot(x, A.x(), A.y() - 2 * sc, 0.8 + 2.5 * sc, hex("#ffffff"), 1);
                const double t0 = q.t / 0.09, t1 = qMin(1.0, t0 + 0.35);
                x.strokeStyle(rgba("#fff2b0", 0.85)); x.lineWidth(qMax(0.6, sc));
                x.line(A.x() + (B.x() - A.x()) * t0, A.y() + (B.y() - A.y()) * t0, A.x() + (B.x() - A.x()) * t1, A.y() + (B.y() - A.y()) * t1);
            }
            x.restore();
        }
        // quem voa, do mais longe pro mais perto
        std::vector<std::pair<double, int>> fl;
        for (int i = 0; i < int(m_gulls.size()); ++i) fl.push_back({ m_gulls[i].D, i });
        if (m_hasPlane && !m_plane.hist.empty()) fl.push_back({ m_plane.hist.back()[1], -1 });
        std::sort(fl.begin(), fl.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        for (const auto& f : fl) { if (f.second < 0) drawAirplane(x, T); else drawGull(x, m_gulls[f.second]); }
        painter.restore();
    }

private:
    // ---------------------------------------------------------------- geometria (coordenadas do Lab)
    static constexpr double kH = 758, HZ = 300, VPX = 430, SUNX = 330, SUNY = HZ - 10, SUNR = 56;
    const QColor PINK = hex("#ff4fa8"), CYAN = hex("#3ff0e0"), GOLD = hex("#ffd25a"), VIOLET = hex("#b07aff"), HAZE = hex("#e0809a");
    static constexpr double ROADY = HZ + (kH - HZ) * 0.955;

    static double sOf(double y) { return qMax(0.0, (y - HZ) / (kH - HZ)); }
    static double xc(double y) { return 150 + 480 * std::pow(sOf(y), 0.8); }
    static double beachW(double y) { return 2 + 26 * sOf(y); }
    static double aveW(double y) { return 6 + 92 * sOf(y); }
    static double aveR(double y) { return xc(y) - beachW(y); }
    static double aveL(double y) { return aveR(y) - aveW(y); }
    static double gapW(double y) { return beachW(y) + aveW(y) + 6 + 40 * sOf(y); }
    static bool isWater(double sx, double sy) { return sy > HZ + 6 && sx > xc(sy) + 2; }
    static QColor warm() { return pickHex({ "#ffcf7a", "#ffd99a", "#ffe7b8", "#f4b860", "#bfe0ff" }); }
    static QColor pastel() { return pickHex({ "#f6c2cf", "#bfe8da", "#f6e2a6", "#d6c6f0", "#f4efe9", "#f5b3a0", "#a8dcef", "#ffd6b0" }); }

    void updateBounds(const Ctx& c)
    {
        m_ox = zx(c, 45) - 45;
        m_wr = c.W - m_ox;
        m_xl = -m_ox;
    }
    void waterClip(Cv& x) const
    {
        x.begin();
        x.moveTo(xc(HZ + 6), HZ + 6);
        for (double y = HZ + 6; y <= kH + 4; y += 4) x.lineTo(xc(y) + 2, y);
        x.lineTo(m_wr + 10, kH + 4); x.lineTo(m_wr + 10, HZ + 6); x.close();
    }

    // sprite macio (o softDot do Lab, miolo 0,25) com alfa
    void dot(Cv& x, double gx, double gy, double r, const QColor& col, double a)
    {
        if (a <= 0.01 || r < 0.3) return;
        auto it = m_sprites.find(col.rgb());
        if (it == m_sprites.end()) it = m_sprites.insert(col.rgb(), softDot(col, 0.25));
        const double old = x.ga;
        x.alphaG(old * qMin(1.0, a));
        x.p.drawPixmap(QRectF(gx - r, gy - r, r * 2, r * 2), *it, QRectF(QPointF(0, 0), QSizeF(it->size())));
        x.alphaG(old);
    }
    // faixa do chão entre x = fl(y) e x = fr(y), de y0 a y1, com a névoa da distância num degradê vertical
    void strip(Cv& x, double y0, double y1, const std::function<double(double)>& fl, const std::function<double(double)>& fr,
               const QColor& col, double st = 1)
    {
        if (y1 - y0 < 0.05) return;
        x.begin();
        x.moveTo(fl(y0), y0);
        for (double y = y0 + st; y < y1; y += st) x.lineTo(fl(y), y);
        x.lineTo(fl(y1), y1);
        x.lineTo(fr(y1), y1);
        for (double y = y1 - st; y > y0; y -= st) x.lineTo(fr(y), y);
        x.lineTo(fr(y0), y0);
        x.close();
        QLinearGradient g(0, y0, 0, y1);
        constexpr int N = 16;
        for (int i = 0; i <= N; ++i) {
            const double y = y0 + (y1 - y0) * i / N;
            g.setColorAt(double(i) / N, mix(col, HAZE, std::pow(1 - sOf(y), 2) * 0.7));
        }
        x.fillStyle(QBrush(g));
        x.fill();
    }
    static void poly(Cv& x, std::initializer_list<QPointF> pts)
    {
        x.begin();
        bool first = true;
        for (const QPointF& q : pts) { if (first) { x.moveTo(q.x(), q.y()); first = false; } else x.lineTo(q.x(), q.y()); }
        x.close();
    }

    // ---------------------------------------------------------------- prédios
    struct Bld { double x, w, y, h, fr, lit; bool hotel, beach, band; QColor roof, col; };
    struct Sign { double x, y, w, h, lw, s; QColor col; bool blade; };
    struct Opts { bool coast = false; double hotels = 0, downtown = 0; bool front = false; };

    // um bloco pequeno (recuo no topo, casa de máquinas): a mesma mentira do prédio grande
    struct Blk { QPointF A, B, A2, B2; };
    Blk block(Cv& x, double X, double Y, double w, double h, double fr, const QColor& col, const QColor& roofC, double hz)
    {
        auto toVP = [fr](double px, double py) { return QPointF(px + (VPX - px) * fr, py + (HZ - py) * fr); };
        const QPointF A(X, Y - h), B(X + w, Y - h), A2 = toVP(A.x(), A.y()), B2 = toVP(B.x(), B.y());
        const int side = X + w / 2 < VPX ? 1 : -1;
        const QPointF E = side > 0 ? B : A, E2 = side > 0 ? B2 : A2, Eb = side > 0 ? QPointF(X + w, Y) : QPointF(X, Y), Eb2 = toVP(Eb.x(), Eb.y());
        x.fillStyle(mix(col, hex("#0a0610"), 0.45)); poly(x, { E, E2, Eb2, Eb }); x.fill();
        if (A.y() > HZ + 1) {
            x.fillStyle(roofC); poly(x, { A, B, B2, A2 }); x.fill();
            x.strokeStyle(rgba("#ffb88a", 0.5 * (1 - hz))); x.lineWidth(0.8); x.line(A2.x(), A2.y(), B2.x(), B2.y());
        }
        x.fillStyle(QBrush(lin(0, Y - h, 0, Y, { { 0, mix(col, hex("#ffb08a"), 0.15 * (1 - hz)) }, { 1, mix(col, hex("#140a1e"), 0.3) } })));
        x.fillRect(X, Y - h, w, h);
        return { A, B, A2, B2 };
    }

    void drawSign(Cv& x, const Sign& sg, double on)
    {
        x.save(); x.lighter(); x.roundCaps();
        const QColor cols[2] = { sg.col, mix(sg.col, hex("#ffffff"), 0.5) };
        const double lws[2] = { sg.lw * 5, sg.lw }, as[2] = { 0.2, 0.95 };
        for (int i = 0; i < 2; ++i) {
            x.strokeStyle(alpha(cols[i], as[i] * on)); x.lineWidth(lws[i]);
            x.begin();
            x.moveTo(sg.x + 2, sg.y + 3 * sg.s); x.lineTo(sg.x + sg.w - 2, sg.y + 3 * sg.s);
            if (sg.blade) { const double bx = sg.x + sg.w * 0.9; x.moveTo(bx, sg.y + 12 * sg.s); x.lineTo(bx, sg.y + sg.h * 0.6); }
            x.stroke();
        }
        x.restore();
    }

    // uma fileira de prédios (receita do depthCity com o bloco 2D)
    void drawPlane(Cv& x, double base, double s, double x0, double x1, const Opts& o)
    {
        std::vector<Bld> row;
        const double hz = qMin(0.85, std::pow(1 - s, 2.2) * 0.9);
        for (double bx = x0 + R(-20, 0); bx < x1;) {
            const double rr = rnd();
            const bool inland = bx < x1 - o.hotels - 60 * s, tower = inland && s < 0.42 && rr < 0.3;
            double bw = R(40, 190) * s + 2, bh = (tower ? R(280, 520) : rr < 0.45 ? R(28, 62) : R(62, 150)) * s + 3;
            if (tower) bw = R(45, 90) * s + 2;
            const bool dt = o.downtown > 0 && std::abs(bx - o.downtown) < 190;
            if (dt && rnd() < 0.6) { bh *= R(3, 6.5); bw *= 0.6; }
            if (bx + bw > x1) bw = qMax(2.0, x1 - bx);
            const bool hotel = o.hotels > 0 && bx + bw > x1 - o.hotels;
            if (hotel) bh = qMin(bh, R(40, 95) * s + 3);
            const double fr0 = R(0.04, 0.14);
            // nada pode entrar na avenida: o ponto mais à direita do bloco tem que ficar à esquerda da calçada
            // na altura do topo do telhado; se não couber, o prédio baixa, depois afina, e em último caso sai
            if (o.coast) {
                auto fits = [&]() {
                    const double r = bx + bw, ext = qMax(0.0, (VPX - r) * fr0) + 4 * s + 2, yt = (base - bh) + (HZ - (base - bh)) * fr0;
                    return r + ext <= aveL(yt) - (1 + 7 * sOf(yt));
                };
                int n = 0;
                while (!fits() && bh > 18 * s + 3 && n++ < 30) bh *= 0.88;
                n = 0;
                while (!fits() && bw > 4 && n++ < 40) bw *= 0.88;
                if (!fits()) { bx += bw + 2; continue; }
            }
            Bld b;
            b.x = bx; b.w = bw; b.y = base - bh; b.h = bh; b.hotel = hotel; b.beach = bx + bw > x1 - (90 + 260 * s); b.fr = fr0;
            b.roof = pickHex({ "#f0e6e0", "#d8c8c8", "#c8806a", "#b8a8c0", "#e6d6b8", "#9ab8b0", "#d89a8a", "#f4f0ea" });
            b.col = hotel ? pastel() : mix(pastel(), hex("#8a7a90"), 0.45);
            b.band = !hotel && rnd() < 0.3; b.lit = R(0.4, 1.3);
            row.push_back(b);
            bx += bw + R(0, 10) * s;
        }
        std::stable_sort(row.begin(), row.end(), [](const Bld& a, const Bld& b) { return std::abs(a.x + a.w / 2 - VPX) > std::abs(b.x + b.w / 2 - VPX); });
        for (const Bld& b : row) drawBuilding(x, b, base, s, hz, o);
    }

    void drawBuilding(Cv& x, const Bld& b, double base, double s, double hz, const Opts& o)
    {
        const double dusk = b.hotel ? 0.42 : 0.58, cx = b.x + b.w / 2, bot = base + 2, fr = b.fr;
        const QColor base0 = mix(mix(b.col, hex("#3a1e4c"), dusk), HAZE, hz);
        const int side = cx < VPX ? 1 : -1;
        const QColor topC = mix(base0, hex("#ffb08a"), 0.28 * (1 - hz)), botC = mix(base0, hex("#140a1e"), 0.45 * (1 - hz * 0.6));
        auto toVP = [fr](double px, double py) { return QPointF(px + (VPX - px) * fr, py + (HZ - py) * fr); };
        const QPointF A(b.x, b.y), B(b.x + b.w, b.y), A2 = toVP(A.x(), A.y()), B2 = toVP(B.x(), B.y());
        const QPointF Ab(b.x, bot), Bb(b.x + b.w, bot), Ab2 = toVP(Ab.x(), Ab.y()), Bb2 = toVP(Bb.x(), Bb.y());
        // a lateral virada pro ponto de fuga, com janelinhas em fileiras que acompanham a fuga
        {
            const QPointF E = side > 0 ? B : A, E2 = side > 0 ? B2 : A2, Eb = side > 0 ? Bb : Ab, Eb2 = side > 0 ? Bb2 : Ab2;
            x.fillStyle(QBrush(lin(0, b.y, 0, bot, { { 0, mix(base0, hex("#140a1e"), 0.3) }, { 1, mix(base0, hex("#0a0610"), 0.62) } })));
            poly(x, { E, E2, Eb2, Eb }); x.fill();
            if (s > 0.12) {
                const double st2 = qMax(3.0, 11 * s);
                for (double yy = b.y + st2; yy < bot - st2; yy += st2) {
                    const double t = (yy - b.y) / (bot - b.y);
                    const QPointF p0(E.x(), E.y() + (Eb.y() - E.y()) * t), p1(E2.x(), E2.y() + (Eb2.y() - E2.y()) * t);
                    for (double k2 = 0.2; k2 < 0.85; k2 += 0.3) {
                        if (rnd() > 0.3 * b.lit) continue;
                        const QPointF q0 = p0 + (p1 - p0) * k2, q1 = p0 + (p1 - p0) * (k2 + 0.18);
                        const double hh = 4.5 * s;
                        x.fillStyle(alpha(warm(), 0.55 * (1 - hz * 0.5)));
                        poly(x, { q0, q1, QPointF(q1.x(), q1.y() + hh), QPointF(q0.x(), q0.y() + hh) }); x.fill();
                    }
                }
            }
            const double e = qMax(0.8, 1.2 * s);
            x.fillStyle(rgba("#ffb88a", 0.2 * (1 - hz)));
            poly(x, { E, E2, QPointF(E2.x(), E2.y() + e), QPointF(E.x(), E.y() + e) }); x.fill();
        }
        // o telhado (só aparece se o topo está abaixo da linha do olho)
        if (b.y > HZ + 1) {
            const QColor roofC = mix(mix(b.roof, hex("#6a4060"), 0.4), HAZE, hz);
            x.fillStyle(roofC); poly(x, { A, B, B2, A2 }); x.fill();
            x.strokeStyle(rgba("#ffb88a", 0.6 * (1 - hz))); x.lineWidth(qMax(0.6, 1.3 * s)); x.line(A2.x(), A2.y(), B2.x(), B2.y());
            x.strokeStyle(rgba("#000000", 0.18)); x.line(A.x(), A.y(), B.x(), B.y());
            auto R2 = [&](double u, double v) {
                return QPointF(A.x() + (B.x() - A.x()) * u + (A2.x() - A.x()) * v + (B2.x() - B.x() - A2.x() + A.x()) * u * v,
                               A.y() + (B.y() - A.y()) * u + (A2.y() - A.y()) * v + (B2.y() - B.y() - A2.y() + A.y()) * u * v);
            };
            auto quad = [&](double u0, double v0, double u1, double v1) { poly(x, { R2(u0, v0), R2(u1, v0), R2(u1, v1), R2(u0, v1) }); x.fill(); };
            x.fillStyle(mix(roofC, hex("#3a2040"), 0.12)); quad(0.05, 0.1, 0.95, 0.9);
            const bool tall = b.h > 170 * s;
            const double rk = rnd();
            const QColor boxC = mix(roofC, hex("#7a5a70"), 0.3);
            if (tall && rk < 0.35 && s > 0.06) {   // heliponto
                const QPointF c0 = R2(0.5, 0.5);
                const double rx = std::abs(R2(0.8, 0.5).x() - R2(0.2, 0.5).x()) / 2, ry = qMax(1.0, std::abs(R2(0.5, 0.85).y() - R2(0.5, 0.15).y()) / 2);
                x.fillStyle(mix(roofC, hex("#2a2030"), 0.45)); x.begin(); x.ellipse(c0.x(), c0.y(), rx, ry, 0, 0, TAU); x.fill();
                x.strokeStyle(rgba("#ffe8a0", 0.7)); x.lineWidth(qMax(0.6, 1.2 * s)); x.stroke();
                if (s > 0.15 && rx > 0.5) {
                    x.save(); x.translate(c0.x(), c0.y()); x.scale(1, ry / rx * 1.6);
                    x.setFont(qMax(6.0, ry * 1.4), 800, { QStringLiteral("Arial") }); x.align = 0; x.baseline = 1;
                    x.fillStyle(rgba("#ffe8a0", 0.75)); x.fillText(QStringLiteral("H"), 0, 0);
                    x.restore();
                }
            } else if (b.hotel && s > 0.12 && rk < 0.55) {   // terraço com piscina e guarda-sóis
                x.fillStyle(mix(roofC, hex("#f0d8c8"), 0.2)); quad(0.08, 0.15, 0.92, 0.85);
                x.fillStyle(hex("#46c8d8")); quad(0.15, 0.3, 0.62, 0.72);
                x.fillStyle(rgba("#d8ffff", 0.45)); quad(0.15, 0.3, 0.62, 0.4);
                for (int n = 0; n < 4; ++n) {
                    const QPointF q = R2(0.72 + (n % 2) * 0.12, 0.3 + (n >> 1) * 0.3);
                    const double r = qMax(1.0, 5 * s);
                    x.fillStyle(pickHex({ "#ff4fa8", "#3ff0e0", "#ffffff", "#ffd25a" })); x.begin(); x.ellipse(q.x(), q.y() - r * 0.8, r, r * 0.45, 0, 0, TAU); x.fill();
                    x.fillStyle(rgba("#140a1e", 0.5)); x.fillRect(q.x() - 0.3, q.y() - r * 0.8, qMax(0.6, 0.8 * s), r * 0.8);
                }
            } else if (rk < 0.15 && s > 0.08) {   // jardim no telhado
                x.fillStyle(mix(hex("#3a5a40"), HAZE, hz)); quad(0.12, 0.2, 0.88, 0.8);
                for (int n = 0; n < 6; ++n) {
                    const QPointF q = R2(R(0.18, 0.82), R(0.25, 0.75));
                    const double r = qMax(1.0, R(4, 7) * s);
                    x.fillStyle(mix(hex("#2e4a34"), HAZE, hz)); x.begin(); x.arc(q.x(), q.y() - r * 0.6, r, 0, TAU); x.fill();
                    x.fillStyle(mix(hex("#6a8a5a"), HAZE, hz)); x.begin(); x.arc(q.x() + r * 0.2, q.y() - r, r * 0.5, 0, TAU); x.fill();
                }
            }
            // recuo no topo: um bloco menor em cima do telhado
            if (s > 0.05 && b.h > 90 * s && rnd() < (tall ? 0.6 : 0.3)) {
                const double u0 = R(0.08, 0.35), u1 = qMin(0.95, u0 + R(0.35, 0.6)), v0 = R(0.15, 0.4);
                const QPointF q0 = R2(u0, v0), q1 = R2(u1, v0);
                const Blk bk = block(x, q0.x(), q0.y(), q1.x() - q0.x(), b.h * R(0.12, 0.35), b.fr * (1 - v0) * R(0.45, 0.8), base0, mix(roofC, hex("#ffffff"), 0.06), hz);
                if (s > 0.2)
                    for (double wy = bk.A.y() + 8 * s; wy < q0.y() - 6 * s; wy += 11 * s)
                        for (double wx = q0.x() + 6 * s; wx < q1.x() - 6 * s; wx += 11 * s)
                            if (rnd() < 0.35) { x.fillStyle(alpha(warm(), 0.8)); x.fillRect(wx, wy, 5 * s, 6 * s); }
            }
            // casa de máquinas e caixa-d'água
            if (s > 0.08)
                for (int n = 0; n < 3; ++n) {
                    if (rnd() >= 0.55) continue;
                    const double u = R(0.08, 0.75), v = R(0.35, 0.85);
                    const QPointF q = R2(u, v);
                    if (rnd() < 0.35 && s > 0.15) {
                        const double r = qMax(1.5, 5 * s), hh = 12 * s;
                        x.fillStyle(mix(boxC, hex("#140a1e"), 0.3));
                        x.fillRect(q.x() - r, q.y() - hh - 6 * s, r * 2, hh); x.fillRect(q.x() - r * 0.8, q.y() - 6 * s, 1, 6 * s); x.fillRect(q.x() + r * 0.8, q.y() - 6 * s, 1, 6 * s);
                        x.fillStyle(mix(boxC, hex("#ffd0b0"), 0.2)); x.begin(); x.ellipse(q.x(), q.y() - hh - 6 * s, r, r * 0.35, 0, 0, TAU); x.fill();
                    } else block(x, q.x(), q.y(), b.w * R(0.08, 0.2), (5 + R(0, 9)) * s, b.fr * 0.25, boxC, mix(boxC, hex("#ffffff"), 0.1), hz);
                }
        }
        // a fachada
        x.fillStyle(QBrush(lin(0, b.y, 0, bot, { { 0, topC }, { 0.15, base0 }, { 1, botC } }))); x.fillRect(b.x, b.y, b.w, bot - b.y);
        x.fillStyle(rgba("#ffb88a", 0.55 * (1 - hz))); x.fillRect(b.x, b.y, b.w, qMax(0.8, 1.6 * s));
        if (b.h > 200) m_blinks.push_back({ cx, b.y - 4, R(0, TAU), 2.5 });
        // janelas
        const double st = qMax(2.2, 11 * s), ww = qMax(0.9, 5 * s), wh = qMax(0.9, 6 * s);
        for (double wy = b.y + st; wy < bot - st; wy += st) {
            if (b.hotel && s > 0.25) {   // sobrancelha art déco sobre cada fileira
                x.fillStyle(mix(base0, hex("#ffffff"), 0.18)); x.fillRect(b.x + 2, wy - 2.5 * s, b.w - 4, qMax(1.0, 2.2 * s));
                x.fillStyle(rgba("#100818", 0.3)); x.fillRect(b.x + 2, wy - 0.3 * s, b.w - 4, qMax(1.0, 3 * s));
            }
            if (b.band) {
                const bool on = rnd() < 0.35 * b.lit;
                x.fillStyle(on ? alpha(warm(), 0.75) : rgba("#140a1e", 0.35)); x.fillRect(b.x + 3 * s, wy, b.w - 6 * s, wh);
                continue;
            }
            for (double wx = b.x + st * 0.6; wx < b.x + b.w - ww - 1; wx += st) {
                const bool on = rnd() < 0.3 * b.lit;
                x.fillStyle(on ? alpha(warm(), 0.85 * (1 - hz * 0.5)) : rgba("#140a1e", 0.4)); x.fillRect(wx, wy, ww, wh);
            }
        }
        // varandas viradas pra praia: na lateral que olha pro mar e na frente dos prédios da beira
        if (b.beach && s > 0.1) {
            const double st3 = qMax(3.0, 11 * s);
            const QColor slab = mix(base0, hex("#ffffff"), 0.32), glass = rgba("#bfe6ff", 0.22);
            if (side > 0) {
                for (double yy = b.y + st3; yy < bot - 4 * s; yy += st3) {
                    const double t = (yy - b.y) / (bot - b.y), out = 3.5 * s, o2 = out * (1 - fr);
                    const QPointF p0(B.x(), B.y() + (Bb.y() - B.y()) * t), p1(B2.x(), B2.y() + (Bb2.y() - B2.y()) * t);
                    x.fillStyle(rgba("#000000", 0.28));
                    poly(x, { QPointF(p0.x(), p0.y() + 1), QPointF(p1.x(), p1.y() + 1), QPointF(p1.x() + o2, p1.y() + 1 + 2.5 * s), QPointF(p0.x() + out, p0.y() + 1 + 2.5 * s) }); x.fill();
                    x.fillStyle(slab); poly(x, { p0, p1, QPointF(p1.x() + o2, p1.y()), QPointF(p0.x() + out, p0.y()) }); x.fill();
                    x.fillStyle(glass);
                    poly(x, { QPointF(p0.x() + out, p0.y()), QPointF(p1.x() + o2, p1.y()), QPointF(p1.x() + o2, p1.y() - 4 * s * (1 - fr)), QPointF(p0.x() + out, p0.y() - 4 * s) }); x.fill();
                    x.strokeStyle(rgba("#ffffff", 0.35)); x.lineWidth(qMax(0.5, 0.7 * s));
                    x.line(p0.x() + out, p0.y() - 4 * s, p1.x() + o2, p1.y() - 4 * s * (1 - fr));
                }
            }
            if (!b.hotel || rnd() < 0.5)
                for (double yy = b.y + st3 * 1.6; yy < bot - 4 * s; yy += st3) {
                    const int bays = qMax(1, int(std::lround(b.w / (34 * s))));
                    for (int k2 = 0; k2 < bays; ++k2) {
                        if (rnd() < 0.25) continue;
                        const double bx0 = b.x + 3 + k2 * (b.w - 6) / bays, bw0 = (b.w - 6) / bays - 3 * s;
                        x.fillStyle(rgba("#000000", 0.25)); x.fillRect(bx0, yy + 1.5 * s, bw0, 2.5 * s);
                        x.fillStyle(slab); x.fillRect(bx0, yy, bw0, qMax(0.8, 1.5 * s));
                        x.fillStyle(glass); x.fillRect(bx0, yy - 4 * s, bw0, 4 * s);
                        x.fillStyle(rgba("#ffffff", 0.3)); x.fillRect(bx0, yy - 4 * s, bw0, qMax(0.5, 0.6 * s));
                    }
                }
        }
        // cantos escuros onde um prédio encosta no outro
        for (int e = 0; e < 2; ++e) {
            const double ex = e == 0 ? b.x : b.x + b.w, dir = e == 0 ? 1 : -1, aw = 3 + 10 * s;
            x.fillStyle(QBrush(lin(ex, 0, ex + dir * aw, 0, { { 0, rgba("#000000", 0.3) }, { 1, rgba("#000000", 0) } })));
            x.fillRect(qMin(ex, ex + dir * aw), b.y, aw, bot - b.y);
        }
        // neon dos hotéis: contorno do telhado e lâmina vertical
        if (b.hotel && s > 0.1 && rnd() < 0.8) {
            const Sign sg { b.x, b.y, b.w, b.h, qMax(0.8, 1.6 * s), s, pickHex({ "#ff4fa8", "#3ff0e0", "#ff4fa8", "#ffd25a", "#b07aff" }), rnd() < 0.55 };
            if (o.front) m_signs.push_back(sg); else drawSign(x, sg, 1);
        }
    }

    static void smallPalm(Cv& x, double bx, double by, double s, const QColor& col)
    {
        const double h = 60 * s, tx = bx + 4 * s, ty = by - h;
        x.strokeStyle(col); x.lineWidth(qMax(0.6, 2.2 * s));
        x.begin(); x.moveTo(bx, by); x.quadTo(bx + 3 * s, by - h / 2, tx, ty); x.stroke();
        x.lineWidth(qMax(0.5, 1.8 * s));
        x.begin();
        for (int k = 0; k < 7; ++k) {
            const double a = -M_PI + k / 6.0 * M_PI + R(-0.1, 0.1), L = 22 * s;
            x.moveTo(tx, ty);
            x.quadTo(tx + std::cos(a) * L * 0.6, ty + std::sin(a) * L * 0.5 - 4 * s, tx + std::cos(a) * L, ty + std::sin(a) * L * 0.5 + 9 * s);
        }
        x.stroke();
    }

    void drawYacht(Cv& x, double bx, double by, double L, double dir, bool sail)
    {
        const double Hh = L * 0.14;
        x.save(); x.translate(bx, by); x.scale(dir, 1);
        x.fillStyle(rgba("#1a0e24", 0.25)); x.fillRect(-L / 2, Hh * 0.4, L, Hh * 0.6);
        x.fillStyle(hex("#f4e8ee")); x.begin(); x.moveTo(-L / 2, -Hh); x.lineTo(L / 2, -Hh * 0.6); x.lineTo(L / 2 - L * 0.14, Hh * 0.4); x.lineTo(-L / 2, Hh * 0.4); x.close(); x.fill();
        if (sail) { x.fillStyle(rgba("#fff4f0", 0.92)); x.begin(); x.moveTo(-L * 0.05, -Hh); x.lineTo(-L * 0.05, -Hh - L * 0.95); x.lineTo(L * 0.32, -Hh); x.fill(); }
        else {
            x.fillStyle(hex("#3a2a44")); x.fillRect(-L * 0.28, -Hh * 2.1, L * 0.45, Hh * 1.2);
            x.fillStyle(hex("#ece0e8")); x.fillRect(-L * 0.2, -Hh * 2.9, L * 0.25, Hh * 0.9);
            x.fillStyle(rgba("#ffd8a0", 0.85));
            for (int k = 0; k < 4; ++k) x.fillRect(-L * 0.25 + k * L * 0.1, -Hh * 1.8, qMax(0.7, L * 0.05), qMax(0.7, Hh * 0.4));
            x.fillStyle(PINK); x.fillRect(-L / 2, -Hh * 0.15, L * 0.95, qMax(0.6, Hh * 0.22));
        }
        x.fillStyle(rgba("#ff9a7a", 0.55)); x.fillRect(-L / 2, -Hh, L, qMax(0.5, Hh * 0.22));
        x.restore();
    }

    // reflexo de uma fileira na água: de cabeça pra baixo embaixo da base, recortado na água, em faixas tremidas.
    // O painter está deslocado em ox; as faixas copiam linhas inteiras da imagem (em pixels físicos).
    void reflect(Cv& x, const QPixmap& src, double base, double a)
    {
        const double px = src.devicePixelRatio();
        x.save(); waterClip(x); x.clip(); x.alphaG(a);
        const double lim = qMin(kH, base + (base - HZ) * 2.2 + 60);
        // fatias de meio ponto (≈ 1 pixel da tela): com 2 pontos a onda do reflexo saía em degraus
        constexpr double st = 0.5;
        for (double y = base; y < lim; y += st) {
            const double d = y - base, sy = base - d * 1.15 - 1, sh = std::sin(y * 0.37) * (0.8 + d * 0.04);
            if (sy < 0) break;
            x.p.drawPixmap(QRectF(sh - m_ox, y, src.width() / px, st), src, QRectF(0, sy * px, src.width(), st * px));
        }
        x.restore();
    }

    void build(const Ctx& c)
    {
        m_blinks.clear(); m_signs.clear();
        m_back = layer(c, c.W, kH);
        const double W0 = m_xl - 30, WR = m_wr;
        {
            QPainter qp(&m_back);
            Cv x(qp);
            x.translate(m_ox, 0);
            // ---------- céu
            x.fillStyle(QBrush(lin(0, 0, 0, HZ, { { 0, hex("#170f36") }, { 0.32, hex("#3c1b5e") }, { 0.58, hex("#8c2a6e") }, { 0.8, hex("#e4506a") }, { 0.93, hex("#ff8a4e") }, { 1, hex("#ffb45a") } })));
            x.fillRect(W0, 0, WR - W0 + 10, HZ + 2);
            for (int i = 0; i < 60; ++i) {
                const double sy = std::pow(rnd(), 1.8) * 130;
                x.fillStyle(rgba("#ffffff", R(0.15, 0.55) * (1 - sy / 130))); x.fillRect(R(W0, WR), sy, R(0.8, 1.5), R(0.8, 1.5));
            }
            x.lighter(); glowAt(x, SUNX, SUNY, 640, "#ff7a4a", 0.3); glowAt(x, SUNX, SUNY, 240, "#ffc070", 0.38); x.over();
            {
                const QPixmap body = softDot(hex("#5a2466"), 0.25), lit = softDot(hex("#ff8a7a"), 0.2), gold = softDot(hex("#ffc070"), 0.2);
                const int nCl = qMax(15, int(15 * (WR - W0) / 1470));
                for (int i = 0; i < nCl; ++i) {
                    const double cy = R(70, HZ - 60), cx = R(W0 - 150, WR + 50), w = R(220, 560), near = qMax(0.0, 1 - std::abs(cx + w / 2 - SUNX) / 900);
                    const int n = 9 + int(w / 50);
                    for (int k = 0; k < n; ++k) {
                        const double t = double(k) / (n - 1), pxx = cx + t * w + R(-15, 15), pw = w * R(0.18, 0.32) * (0.5 + std::sin(M_PI * t) * 0.7), ph = R(9, 20) * (0.5 + std::sin(M_PI * t) * 0.6), py = cy + R(-4, 4);
                        x.alphaG(R(0.35, 0.6) * (0.6 + cy / HZ * 0.5)); x.image(body, pxx - pw / 2, py - ph / 2, pw, ph);
                        x.alphaG((0.25 + near * 0.5) * R(0.5, 1)); x.image(near > 0.5 ? gold : lit, pxx - pw * 0.4, py + ph * 0.05, pw * 0.8, ph * 0.45);
                    }
                }
                x.alphaG(1);
            }
            {
                x.save(); x.begin(); x.rect(W0, 0, WR - W0 + 10, HZ + 4); x.clip();
                x.fillStyle(QBrush(lin(0, SUNY - SUNR, 0, SUNY + SUNR, { { 0, hex("#fff6c0") }, { 0.45, hex("#ffc45a") }, { 0.8, hex("#ff6a52") }, { 1, hex("#ff3a6a") } })));
                x.begin(); x.arc(SUNX, SUNY, SUNR, 0, TAU); x.fill();
                x.fillStyle(QBrush(vgrad(HZ - 40, HZ + 4, rgba("#ff7a5a", 0), rgba("#ff9a6a", 0.7)))); x.fillRect(W0, HZ - 40, WR - W0 + 10, 44);
                x.restore();
            }
            // ---------- água
            x.fillStyle(QBrush(lin(0, HZ, 0, kH, { { 0, hex("#f39a72") }, { 0.05, hex("#d8607a") }, { 0.28, hex("#8a3474") }, { 0.62, hex("#4a2058") }, { 1, hex("#26163e") } })));
            x.fillRect(W0, HZ, WR - W0 + 10, kH - HZ);
            x.save(); waterClip(x); x.clip(); x.lighter();
            // o caminho do sol na água: faixas emendadas (com o vão entre elas, viravam linhas de TV na tela grande);
            // o alfa é 2/3 do antigo pra manter o mesmo brilho médio
            for (double yy = HZ + 4; yy < kH; yy += 1) {
                const double k = sOf(yy), hw = 10 + k * 170, a = 0.37 * (1 - k * 0.85);
                x.fillStyle(QBrush(lin(SUNX - hw, 0, SUNX + hw, 0, { { 0, rgba("#ffb060", 0) }, { 0.5, rgba("#ffc878", a) }, { 1, rgba("#ffb060", 0) } })));
                x.fillRect(SUNX - hw, yy, hw * 2, 1);
            }
            x.over();
            const int nRip = int(1300 * (WR - W0) / 1470);
            for (int i = 0; i < nRip; ++i) {
                const double k = std::pow(rnd(), 1.7), yy = HZ + 4 + k * (kH - HZ), w = 3 + k * 80;
                x.fillStyle(rnd() < 0.5 ? rgba("#ffd0b0", 0.05 + k * 0.08) : rgba("#140a26", 0.06 + k * 0.12));
                x.fillRect(R(W0, WR), yy, R(w * 0.5, w), 0.6 + k * 1.6);
            }
            x.restore();
            // ---------- a outra margem: faixa baixa, arranha-céus contra o sol, península à direita
            {
                QPixmap far = layer(c, c.W, kH);
                {
                    QPainter fp(&far);
                    Cv f(fp);
                    f.translate(m_ox, 0);
                    const double yb = HZ + 6;
                    f.fillStyle(mix(hex("#5a2a5a"), HAZE, 0.4)); f.fillRect(200, yb - 2, 880, 4);
                    Opts o; o.downtown = 360;
                    drawPlane(f, yb, 0.05, 150, 1060, o);
                    const double pr = qMax(1440.0, WR + 10);
                    f.fillStyle(mix(hex("#5a2a5a"), HAZE, 0.5)); f.begin(); f.moveTo(1060, yb); f.quadTo(1200, HZ - 2, pr, HZ - 1); f.lineTo(pr, yb + 1); f.fill();
                    for (int i = 0; i < int(40 * (pr - 1080) / 360); ++i) { f.fillStyle(rgba("#ffd8a0", R(0.3, 0.8))); f.fillRect(R(1080, pr), HZ + R(-1, 3), 1, 1); }
                    f.fillStyle(QBrush(vgrad(HZ - 90, yb + 2, rgba("#ff8a7a", 0), rgba("#ff9a7a", 0.35)))); f.fillRect(W0, HZ - 90, pr - W0, 98);
                }
                x.translate(-m_ox, 0); x.image(far, 0, 0); x.translate(m_ox, 0);
                reflect(x, far, HZ + 6, 0.4);
            }
            // ---------- a terra à esquerda da orla, a areia e a avenida
            // o chão entre as fileiras pega a mesma névoa da distância que a avenida (era o papel da névoa por fileira)
            strip(x, HZ + 4, kH + 4, [W0](double) { return W0; }, [](double y) { return xc(y); }, hex("#1e1228"), 4);
            // areia, calçadas, asfalto e faixa amarela como formas contínuas que seguem a curva (as tirinhas de 2 px do
            // Lab viravam escadinha na tela grande); a névoa entra num degradê vertical, a mesma conta de antes
            const double y0 = HZ + 6, y1 = kH + 2;
            auto sw = [](double y) { return 1 + 7 * sOf(y); };
            strip(x, y0, y1, [](double y) { return xc(y) - beachW(y); }, [](double y) { return xc(y) + 2; }, mix(hex("#d8a898"), hex("#3a1e40"), 0.35));
            strip(x, y0, y1, [&](double y) { return aveL(y) - sw(y); }, [](double y) { return aveL(y); }, hex("#7a5a70"));
            strip(x, y0, y1, [](double y) { return aveL(y); }, [&](double y) { return aveR(y) - sw(y); }, hex("#2a1a30"));
            strip(x, y0, y1, [&](double y) { return aveR(y) - sw(y); }, [](double y) { return aveR(y); }, hex("#c8a0a8"));
            {
                auto band = [](double y) { return int(std::floor(std::log(sOf(y) + 0.02) * 22)); };
                auto mid = [&](double y) { return (aveL(y) + aveR(y) - sw(y)) / 2; };
                auto hw = [](double y) { return qMax(0.4, 0.8 * sOf(y)); };
                for (double y = y0; y < kH;) {
                    const int b = band(y);
                    double ye = y;
                    while (ye < kH && band(ye) == b) ye += 0.25;
                    if (((b % 2) + 2) % 2 == 0)
                        strip(x, y, ye, [&](double v) { return mid(v) - hw(v); }, [&](double v) { return mid(v) + hw(v); }, hex("#ffd25a"), 0.25);
                    y = ye;
                }
            }
            x.strokeStyle(rgba("#ffe8e0", 0.45)); x.lineWidth(1);
            x.begin();
            for (double y = HZ + 6; y <= kH; y += 1) { const double xx = xc(y) + 2; if (y == HZ + 6) x.moveTo(xx, y); else x.lineTo(xx, y); }
            x.stroke();
            // iates ancorados perto da orla e na baía (antes das fileiras: os prédios da frente passam por cima)
            {
                struct An { double x, y, L, d; bool sail; };
                std::vector<An> an;
                for (int i = 0; i < 40 && an.size() < 14; ++i) {
                    const double y = HZ + 10 + std::pow(rnd(), 1.3) * (kH - HZ - 30), s = sOf(y), xx = xc(y) + 20 + std::pow(rnd(), 1.6) * (WR - xc(y));
                    const double L = 8 + 60 * s, d = rnd() < 0.5 ? 1 : -1;
                    const bool sail = rnd() < 0.3;
                    if (!isWater(xx - 40 * s, y)) continue;
                    an.push_back({ xx, y, L, d, sail });
                }
                std::sort(an.begin(), an.end(), [](const An& a, const An& b) { return a.y < b.y; });
                for (const An& a : an) drawYacht(x, a.x, a.y, a.L, a.d, a.sail);
            }
        }
        // ---------- as fileiras da cidade, de trás pra frente; cada uma reflete na água
        m_front = layer(c, c.W, kH);
        constexpr int NQ = 14, NB = NQ - 2;
        for (int k = 0; k < NQ; ++k) {
            const double s = 0.03 * std::pow(0.92 / 0.03, k / 13.0), base = HZ + (kH - HZ) * s, xe = xc(base) - gapW(base);
            QPixmap pl = layer(c, c.W, kH);
            {
                QPainter pp(&pl);
                Cv pc(pp);
                pc.translate(m_ox, 0);
                Opts o; o.coast = true; o.hotels = 50 + 130 * s; o.downtown = k < 3 ? 90 : 0; o.front = k >= NB;
                drawPlane(pc, base, s, W0, xe, o);
                // a palmeira na beira da areia e o poste do lado da cidade
                const QColor col = mix(hex("#1c0e24"), HAZE, std::pow(1 - s, 2) * 0.7);
                smallPalm(pc, aveR(base) + 2 * s, base, s, col);
                const double lx = aveL(base) - 2 * s;
                pc.fillStyle(col); pc.fillRect(lx, base - 50 * s, qMax(0.6, 1.4 * s), 50 * s);
                pc.lighter(); dot(pc, lx, base - 50 * s, 3 + 14 * s, hex("#ffb060"), 0.55); pc.over();
            }
            QPainter tp(k >= NB ? &m_front : &m_back);
            Cv t(tp);
            t.translate(m_ox, 0);
            reflect(t, pl, base, 0.32);
            t.translate(-m_ox, 0); t.image(pl, 0, 0); t.translate(m_ox, 0);
            // (a névoa rosada que o Lab punha por cima de cada fileira saiu: somada 14 vezes e com corte seco na base,
            // virava faixas leitosas do meio pro fundo; os prédios já pegam a névoa da distância na própria cor)
        }
        // a rua da frente
        {
            QPainter fp(&m_front);
            Cv f(fp);
            f.translate(m_ox, 0);
            const double xe = xc(ROADY + 10);
            f.fillStyle(hex("#1c1024")); f.fillRect(W0, ROADY - 6, xe - W0, 30);
            f.fillStyle(rgba("#ffd25a", 0.4)); for (double xx = W0; xx < xe - 20; xx += 34) f.fillRect(xx, ROADY + 6, 18, 1.5);
            f.fillStyle(rgba("#e8c0c8", 0.18)); f.fillRect(W0, ROADY - 6, xe - W0, 1.5);
        }
        m_glints.clear();
        for (int i = 0; i < 80; ++i) m_glints.push_back({ HZ + 8 + std::pow(rnd(), 1.4) * (kH - HZ - 8), R(0, TAU), R(1.5, 4), R(-1, 1) });
    }

    // ---------------------------------------------------------------- a perseguição
    struct ChaseCar { bool cop; double off, lane, ph; };
    struct Heli { double off, lat, alt, ph; };
    struct Chase { int dir = 1; double lead = 1, v = 0.45; std::vector<ChaseCar> cars; std::vector<Heli> helis; };
    struct Shot { int i, j; double t; };

    void startChase()
    {
        const int dir = -m_lastDir;
        m_lastDir = dir;
        Chase ch;
        ch.dir = dir;
        const int n = 6 + RI(4);
        ch.cars.push_back({ false, 0, 0.5, R(0, TAU) });
        double off = 0;
        for (int i = 0; i < n; ++i) { off += R(0.75, 1.25); ch.cars.push_back({ true, off, R(0.3, 0.7), R(0, TAU) }); }
        ch.helis.push_back({ R(0.5, 1.0), -R(0.5, 0.9), R(170, 230), R(0, TAU) });
        ch.helis.push_back({ R(1.6, 2.6), R(0.9, 1.4), R(200, 260), R(0, TAU) });
        ch.lead = dir > 0 ? 46 : 0.75;
        ch.v = R(0.4, 0.5);
        m_chase = ch;
        m_hasChase = true;
    }
    double chaseD(const ChaseCar& cc) const { return m_chase.lead * std::exp(m_chase.dir * cc.off * 0.14); }
    double chaseScale(const ChaseCar& cc) const { return 0.9 / chaseD(cc); }
    // um ponto da avenida a uma distância d, na faixa do carro
    QPointF lanePt(const ChaseCar& cc, double d, double T, double* sc = nullptr) const
    {
        const double s = 0.9 / d, y = HZ + s * (kH - HZ), lane = cc.lane + std::sin(T * 2.6 + cc.ph) * 0.12;
        if (sc) *sc = s;
        return QPointF(aveL(y) + aveW(y) * lane, y);
    }
    QPointF chasePos(const ChaseCar& cc, double T) const { return lanePt(cc, chaseD(cc), T); }

    // carro na avenida, achatado pela perspectiva: o comprimento vai na direção da profundidade, a largura é uma
    // fração da largura da avenida naquele ponto, e a lataria sobe um pouco na tela
    void drawChaseCar(Cv& x, const ChaseCar& cc, double T)
    {
        const double D = chaseD(cc);
        double sc = 0;
        const QPointF Fp = lanePt(cc, qMax(0.3, D - m_chase.dir * 0.022), T), Bp = lanePt(cc, qMax(0.3, D + m_chase.dir * 0.022), T), C = lanePt(cc, D, T, &sc);
        if (sc < 0.02 || C.y() > kH + 30) return;
        const double ux = Fp.x() - Bp.x(), uy = Fp.y() - Bp.y(), wpx = aveW(C.y()) * 0.13, hb = qMax(0.6, wpx * 0.42);
        const bool flash = int(std::floor(T * 9 + cc.ph)) % 2;
        auto quad = [&](double off, double u0, double u1, double v0, double v1, const QColor& col) {
            x.save(); x.translate(C.x(), C.y() - off); x.p.setTransform(QTransform(ux, uy, wpx, 0, 0, 0), true);
            x.fillStyle(col); x.fillRect(u0, v0, u1 - u0, v1 - v0); x.restore();
        };
        auto at = [&](double u, double v, double off) { return QPointF(C.x() + ux * u + wpx * v, C.y() + uy * u - off); };
        const QColor body = hex(cc.cop ? "#1a1e30" : "#f4eef2"), sideC = hex(cc.cop ? "#0c0e18" : "#a898a8");
        quad(-0.6, -0.5, 0.5, -0.55, 0.55, rgba("#000000", 0.35));
        for (double k = 0; k < hb; k += 0.7) quad(k, -0.5, 0.5, -0.5, 0.5, sideC);
        quad(hb, -0.5, 0.5, -0.5, 0.5, body);
        if (cc.cop) quad(hb, -0.16, 0.16, -0.5, 0.5, hex("#e8e8f0")); else quad(hb, -0.5, 0.5, -0.07, 0.07, PINK);
        quad(hb * 1.5, -0.14, 0.18, -0.36, 0.36, hex(cc.cop ? "#2a3448" : "#3a2a44"));
        x.save(); x.lighter();
        const double r = 0.8 + 2.2 * sc;
        for (double v : { -0.3, 0.3 }) {
            const QPointF f = at(0.5, v, hb * 0.6), t = at(-0.5, v, hb * 0.6);
            dot(x, f.x(), f.y(), r * 1.3, hex("#fff4dc"), 0.9); dot(x, t.x(), t.y(), r, hex("#ff2a3a"), 0.8);
        }
        const QPointF ah = at(1.8, 0, 0);
        dot(x, ah.x(), ah.y(), 4 + 14 * sc, hex("#fff4dc"), 0.12);
        if (cc.cop) {
            const QColor col = hex(flash ? "#ff2a3a" : "#3a7aff");
            const QPointF top = at(0, flash ? -0.2 : 0.2, hb * 2);
            dot(x, top.x(), top.y(), 1 + 3 * sc, col, 1); dot(x, top.x(), top.y(), 3 + 10 * sc, col, 0.5);
            x.translate(C.x(), C.y()); x.scale(1, 0.45); dot(x, 0, 0, 8 + 26 * sc, col, 0.22);
        }
        x.restore();
    }
    // helicóptero da polícia acompanhando a fuga, nariz pra onde a fuga vai, holofote no carro fugindo
    std::array<double, 3> heliPos(const Heli& h, double T) const
    {
        const double D = m_chase.lead * std::exp(m_chase.dir * h.off * 0.14), sc = 0.9 / D, y = HZ + sc * (kH - HZ), w = aveW(y);
        const double xx = aveL(y) + w * (0.5 + h.lat) + std::sin(T * 0.7 + h.ph) * 10 * sc, hy = y - h.alt * sc - std::sin(T * 1.3 + h.ph) * 3 * sc;
        return { xx, hy, sc };
    }
    void drawHeliSpot(Cv& x, const Heli& h, double T)
    {
        const auto hp = heliPos(h, T);
        const double hx = hp[0], hy = hp[1], sc = hp[2];
        if (sc < 0.01 || hy > kH + 40) return;
        const ChaseCar& run = m_chase.cars[0];
        double rs = 0;
        const QPointF rp = lanePt(run, m_chase.lead, T, &rs);
        const double tx = rp.x() + std::sin(T * 1.7 + h.ph) * 8 * rs, ty = rp.y() + std::cos(T * 1.3 + h.ph) * 3 * rs, sy = hy + 4 * (0.25 + 1.6 * sc);
        const double fa = smooth(0.02, 0.1, sc), pr = 6 + 26 * rs;
        x.save(); x.lighter();
        x.fillStyle(QBrush(lin(hx, sy, tx, ty, { { 0, rgba("#eaf6ff", 0.13 * fa) }, { 1, rgba("#eaf6ff", 0.025 * fa) } })));
        x.begin(); x.moveTo(hx - 1, sy); x.lineTo(tx - pr, ty); x.lineTo(tx + pr, ty); x.lineTo(hx + 1, sy); x.fill();
        x.translate(tx, ty); x.scale(1, 0.4); dot(x, 0, 0, pr * 1.3, hex("#eaf6ff"), 0.14 * fa);
        x.restore();
    }
    void drawHeliBody(Cv& x, const Heli& h, double T)
    {
        const auto hp = heliPos(h, T);
        const double hx = hp[0], hy = hp[1], sc = hp[2];
        if (sc < 0.01 || hy > kH + 40) return;
        const double k = 1.85 * sc, face = m_chase.dir > 0 ? 1 : -1, fa = smooth(0.012, 0.06, sc);
        const QColor col = hex("#140a1c");
        x.save(); x.alphaG(fa); x.translate(hx, hy); x.scale(face * k, k); x.rotate(0.08);
        x.fillStyle(col); x.begin(); x.ellipse(0, 0, 9, 4.6, 0, 0, TAU); x.fill();
        x.begin(); x.moveTo(-5, -1.5); x.lineTo(-24, -1); x.lineTo(-24, 0.8); x.lineTo(-5, 2.5); x.fill();
        x.begin(); x.moveTo(-23, 0); x.lineTo(-26, -6); x.lineTo(-27.5, -6); x.lineTo(-26, 1); x.fill();
        x.fillStyle(rgba("#ff9a7a", 0.45)); x.begin(); x.ellipse(4, -1.5, 3.6, 2.4, 0, 0, TAU); x.fill();
        x.strokeStyle(col); x.lineWidth(0.8);
        x.begin(); x.moveTo(-6, 6.5); x.lineTo(6, 6.5); x.moveTo(-3.5, 4); x.lineTo(-4, 6.5); x.moveTo(3.5, 4); x.lineTo(4, 6.5); x.stroke();
        x.fillStyle(col); x.fillRect(-0.8, -6.5, 1.6, 2);
        const double r = 22 * std::abs(std::cos(T * 37 + h.ph));
        x.strokeStyle(rgba("#1a0e24", 0.75)); x.lineWidth(0.9); x.line(-r, -6.5, r, -6.5);
        x.strokeStyle(rgba("#1a0e24", 0.25)); x.line(-22, -6.5, 22, -6.5);
        x.begin(); x.arc(-26.5, -3, qMax(0.05, 3 * std::abs(std::sin(T * 41 + h.ph))), 0, TAU); x.stroke();
        x.restore();
        x.save(); x.lighter();
        dot(x, hx - face * 26 * k, hy - 6 * k, 0.4 + 4 * sc, hex("#ff2a2a"), (std::sin(T * 4 + h.ph) > 0.3 ? 0.9 : 0.1) * fa);
        if (std::fmod((T + h.ph) * 1.3, 1.0) < 0.08) dot(x, hx, hy + 5 * k, 0.4 + 5 * sc, hex("#ffffff"), fa);
        dot(x, hx, hy + 4 * k, 0.4 + 4 * sc, hex("#eaf6ff"), 0.85 * fa);
        x.restore();
    }

    // ---------------------------------------------------------------- quem voa (gaivotas e o aviãozinho)
    struct Gull { double X, D, A, h, v, ph, turn, flap; };
    struct Plane { double P[3][3]; double t = 0, dur = 48; std::vector<std::array<double, 3>> hist; };

    std::array<double, 3> SKP(double X, double D, double A) const
    {
        const double sc = 0.9 / D;
        return { VPX + X * sc * 1000, HZ + sc * (kH - HZ) - A * sc * 1000, sc };
    }
    static double skX(double sx, double D) { return (sx - VPX) / (0.9 / D * 1000); }

    void spawnFlock()
    {
        const double D = R(1.4, 9), X = skX(rnd() < 0.5 ? -40 : m_wr + 40, D);
        const bool fromLeft = X < skX(VPX, D);
        const double h0 = fromLeft ? R(-0.6, 0.6) : M_PI + R(-0.6, 0.6), A = R(0.12, 0.34);
        const int n = rnd() < 0.45 ? 3 + RI(3) : 1;
        for (int k = 0; k < n; ++k)
            m_gulls.push_back({ X - std::cos(h0) * k * R(0.03, 0.06), D + R(-0.08, 0.08) * D * 0.3, A + R(-0.02, 0.02), h0, R(0.07, 0.1) * std::sqrt(D), R(0, TAU), R(0, TAU), 0 });
    }
    void drawGull(Cv& x, const Gull& g)
    {
        const auto q = SKP(g.X, g.D, g.A);
        const double s = q[2] * 4.4, fa = smooth(0.06, 0.16, q[2]);
        if (s < 0.15) return;
        const double f = std::sin(g.ph) * 4 * s * g.flap + (1 - g.flap) * -1.2 * s, bank = std::sin(g.turn) * 0.25;
        x.save(); x.translate(q[0], q[1]); x.rotate(bank); x.alphaG(fa);
        x.strokeStyle(rgba("#1a0e24", 0.85)); x.roundCaps(); x.lineWidth(qMax(0.5, 1.4 * s));
        x.begin(); x.moveTo(-8 * s, -f); x.quadTo(-4 * s, -3 * s - f * 0.5, 0, 0); x.quadTo(4 * s, -3 * s - f * 0.5, 8 * s, -f); x.stroke();
        x.restore();
    }

    void newPlane()
    {
        const double r = rnd();
        double P[3][3];
        if (r < 0.4) { const double Q[3][3] = { { 3.5, 26, 0.6 }, { 1.4, 9, 0.55 }, { -1.3, 2.3, 0.64 } }; std::copy(&Q[0][0], &Q[0][0] + 9, &P[0][0]); }
        else if (r < 0.75) { const double Q[3][3] = { { skX(m_wr + 400, 4.5), 4.5, 0.55 }, { 0.4, 3.2, 0.5 }, { skX(-400, 3.8), 3.8, 0.5 } }; std::copy(&Q[0][0], &Q[0][0] + 9, &P[0][0]); }
        else { const double Q[3][3] = { { -1.3, 2.3, 0.64 }, { 0.8, 9, 0.55 }, { 3.2, 24, 0.6 } }; std::copy(&Q[0][0], &Q[0][0] + 9, &P[0][0]); }
        m_plane = Plane();
        std::copy(&P[0][0], &P[0][0] + 9, &m_plane.P[0][0]);
        m_plane.dur = R(42, 55);
        m_hasPlane = true;
    }
    std::array<double, 3> bez(double t) const
    {
        std::array<double, 3> o {};
        for (int i = 0; i < 3; ++i) o[i] = (1 - t) * (1 - t) * m_plane.P[0][i] + 2 * (1 - t) * t * m_plane.P[1][i] + t * t * m_plane.P[2][i];
        return o;
    }
    std::array<double, 3> backAlong(double dist) const
    {
        const auto& hs = m_plane.hist;
        double acc = 0;
        for (int i = int(hs.size()) - 1; i > 0; --i) {
            const auto& a = hs[i];
            const auto& b = hs[i - 1];
            const double d = std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
            if (d > 0 && acc + d >= dist) { const double k = (dist - acc) / d; return { a[0] + (b[0] - a[0]) * k, a[1] + (b[1] - a[1]) * k, a[2] + (b[2] - a[2]) * k }; }
            acc += d;
        }
        return hs.front();
    }
    void drawAirplane(Cv& x, double T)
    {
        if (m_plane.hist.size() < 2) return;
        const auto cur = SKP(m_plane.hist.back()[0], m_plane.hist.back()[1], m_plane.hist.back()[2]);
        const double px0 = cur[0], py0 = cur[1], sc = cur[2], fa = smooth(0.03, 0.08, sc);
        if (sc < 0.025) return;
        const auto pv = backAlong(0.06);
        const auto pq = SKP(pv[0], pv[1], pv[2]);
        const double dx = px0 - pq[0], dy = py0 - pq[1], dl = qMax(1e-6, std::hypot(dx, dy)), dir = dx >= 0 ? 1 : -1, flat = qMax(0.3, std::abs(dx) / dl);
        x.save(); x.alphaG(fa);
        // a faixa: fita pelos pontos do caminho, da corda até o fim, ondulando
        constexpr int N = 16;
        const double r0 = 0.07, r1 = r0 + 0.34;
        QPointF top[N + 1], bot[N + 1];
        for (int k = 0; k <= N; ++k) {
            const auto qq = backAlong(r0 + (r1 - r0) * k / N);
            const auto s3 = SKP(qq[0], qq[1], qq[2]);
            const double hh = 0.032 * s3[2] * 1000, w = std::sin(T * 5 - k * 0.5) * hh * 0.18 * (double(k) / N);
            top[k] = QPointF(s3[0], s3[1] - hh / 2 + w);
            bot[k] = QPointF(s3[0], s3[1] + hh / 2 + w);
        }
        {
            const auto rope = backAlong(r0);
            const auto rq = SKP(rope[0], rope[1], rope[2]);
            x.strokeStyle(rgba("#1a0e24", 0.7)); x.lineWidth(qMax(0.4, 0.8 * sc * 3)); x.line(px0, py0, rq[0], rq[1]);
        }
        auto ribbon = [&]() { x.begin(); x.moveTo(top[0].x(), top[0].y()); for (int k = 1; k <= N; ++k) x.lineTo(top[k].x(), top[k].y()); for (int k = N; k >= 0; --k) x.lineTo(bot[k].x(), bot[k].y()); x.close(); };
        ribbon(); x.fillStyle(hex("#f6e8ec")); x.fill();
        x.fillStyle(rgba("#ff9a7a", 0.25));
        x.begin(); x.moveTo(top[0].x(), top[0].y()); for (int k = 1; k <= N; ++k) x.lineTo(top[k].x(), top[k].y());
        for (int k = N; k >= 0; --k) x.lineTo(top[k].x(), top[k].y() + (bot[k].y() - top[k].y()) * 0.3);
        x.close(); x.fill();
        // o texto: esticado entre a ponta da esquerda e a da direita, lendo sempre da esquerda pra direita
        {
            const QPointF a = (top[0] + bot[0]) / 2, b = (top[N] + bot[N]) / 2;
            const bool ab = a.x() <= b.x();
            const QPointF L = ab ? a : b, Rr = ab ? b : a;
            const double len = std::hypot(Rr.x() - L.x(), Rr.y() - L.y());
            const double hL = ab ? bot[0].y() - top[0].y() : bot[N].y() - top[N].y(), hR = ab ? bot[N].y() - top[N].y() : bot[0].y() - top[0].y();
            if (len > 24) {
                const QString txt = QStringLiteral("CLUB CORAL ★ TONIGHT 9PM");
                const double TW = 160, TH = 12;
                x.save(); ribbon(); x.clip();
                x.translate(L.x(), L.y()); x.rotate(std::atan2(Rr.y() - L.y(), Rr.x() - L.x())); x.scale(len / TW, (hL + hR) / 2 / TH);
                x.setFont(10, 800, { QStringLiteral("Arial Black"), QStringLiteral("IBM Plex Sans"), QStringLiteral("Arial") });
                const double mw = x.measure(txt), fit = mw > TW * 0.94 ? TW * 0.94 / mw : 1;
                x.translate(TW / 2, 1); x.scale(fit, 1);
                x.align = 0; x.baseline = 1; x.fillStyle(hex("#d0306e")); x.fillText(txt, 0, 0);
                x.restore();
            }
        }
        // o avião: visto de lado, achatado quando vem de frente; um pouco inclinado na curva
        const double k = sc * 3.6;
        x.translate(px0, py0);
        x.rotate(qBound(-0.25, std::atan2(dy, std::abs(dx) + 1e-3) * dir * 0.6, 0.25));
        x.scale(dir * k * flat, k);
        x.fillStyle(hex("#1a0e24")); x.begin(); x.ellipse(0, 0, 12, 2.8, 0, 0, TAU); x.fill();
        x.save(); x.scale(1 / flat, 1); x.fillRect(-2, -8, 4, 16); x.restore();
        x.fillRect(-14, -5, 3, 5);
        x.fillStyle(rgba("#ff9a7a", 0.5)); x.fillRect(-10, -2.8, 20, 0.8);
        x.restore();
        if (std::sin(T * 5) > 0.6) { x.save(); x.lighter(); dot(x, px0, py0 + 3 * k, 1 + 4 * sc, hex("#ff3a3a"), 0.85 * fa); x.restore(); }
    }

    // ---------------------------------------------------------------- estado
    struct Blink { double x, y, ph, r; };
    struct Glint { double y, ph, s, dx; };
    struct Car { int dir; double x, v; };
    struct TrailPt { double x, y, t; };
    struct Boat { double x = 0, y = 0, y0 = 0, s = 0, face = -1, tp = -1, turnX = 0, sp = 0, L = 0; int dir = -1; bool sail = false; std::vector<TrailPt> trail; };

    QString m_key;
    QPixmap m_back, m_front;
    QHash<QRgb, QPixmap> m_sprites;
    double m_t = 0, m_ox = 0, m_wr = 1440, m_xl = 0;
    std::vector<Blink> m_blinks;
    std::vector<Glint> m_glints;
    std::vector<Sign> m_signs;
    std::vector<Car> m_cars;
    std::vector<Boat> m_boats;
    double m_nextBoat = 1;
    bool m_hasShip = false;
    double m_shipX = 1380;
    Chase m_chase;
    bool m_hasChase = false;
    double m_nextChase = 6;
    int m_lastDir = -1;
    std::vector<Shot> m_shots;
    std::vector<Gull> m_gulls;
    double m_nextGull = 0.5;
    Plane m_plane;
    bool m_hasPlane = false;
    double m_nextPlane = 2;
};

} // namespace

std::unique_ptr<Layer> makeOceanDrive(const QString& id)
{
    if (id == QLatin1String("oceandrive")) return std::make_unique<OceanDrive>();
    return nullptr;
}

} // namespace AnimBg
