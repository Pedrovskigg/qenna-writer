// Cidade com volume dos temas Animados (Night Train, Paulista e Harbor Lights):
// três planos com névoa entre eles, parede lateral virada pro meio da mesa,
// quina pegando luz, canto escuro onde um prédio encosta no outro, entulho no
// telhado e cada prédio acendendo do seu jeito. O que fica na frente (viaduto,
// trem, rua, torres de TV, helicópteros, a ponte) tem o mesmo acabamento.
// A cidade é sorteada sempre com a mesma semente: na Hora real só a luz muda.
// Portado de city_v.js (depthCity, depthViaduct, depthTrain, depthBridge...).

#include "AnimKit.h"

#include <QLinearGradient>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>
#include <vector>

namespace AnimBg {
namespace {

using namespace kit;

double inLeftZ(const Ctx& c, double frac, double fallback)
{
    const QRectF& l = c.zones.left;
    return l.width() >= 120 ? l.left() + l.width() * frac : fallback;
}
double inRightZ(const Ctx& c, double frac, double fallback)
{
    const QRectF& r = c.zones.right;
    return r.width() >= 120 ? r.left() + r.width() * frac : fallback;
}

QColor warmC()
{
    static const char* W[5] = { "#ffcf7a", "#ffd99a", "#ffe7b8", "#f4b860", "#bfe0ff" };
    return hex(W[RI(5)]);
}

// Número estável de 0 a 1 (as janelas acesas do trem não piscam de um quadro pro outro).
double hsh(double a, double b)
{
    const double s = std::sin(a * 127.1 + b * 311.7) * 43758.5453;
    return s - std::floor(s);
}

class CityDepth : public Layer {
public:
    CityDepth(bool towers, bool helis, bool train, bool water) : m_towers(towers), m_helis(helis), m_train(train), m_water(water) {}

    void init(Ctx& c) override
    {
        m_cityPm = QPixmap();
        m_prev = QPixmap();
        m_bridge = QPixmap();
        m_fade = 0;
        m_key.clear();
        m_helisList.clear();
        m_trainOn = false;
        m_nextTrain = 1.5;
        m_cars.clear();
        if (!m_water || m_bridgeOn()) {
            const int n = count(9, c);
            for (int i = 0; i < n; ++i) { m_cars.push_back(newCar(1, true, c)); m_cars.push_back(newCar(-1, true, c)); }
        }
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_toggle += dt;
        m_nextPlane -= dt;
        m_fade = qMax(0.0, m_fade - dt / 1.5);
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
                m_trainSeed = R(1, 99);
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
        const double ROAD = road(c);
        skyTOD(p, pal, c, mix(pal.app, Qt::black, 0.35), mix(pal.app, hex(m_towers ? "#4a3a3a" : "#5a3a40"), 0.35));
        // Na Hora real a cidade é refeita a cada 15 min de relógio (mesma semente, só a luz muda);
        // quando a hora anda um passo, a antiga some num fade. Ao abrir ou pular de hora, troca direto.
        const bool real = c.tod == QLatin1String("real");
        const QString key = real ? QStringLiteral("real") + QString::number(int(std::floor(c.hour * 4))) : c.tod;
        if (key != m_key || m_cityPm.isNull() || qAbs(m_cityPx - c.px) > 0.01) {
            int stepN = 0;
            if (real && m_key.startsWith(QLatin1String("real")) && !m_cityPm.isNull() && qAbs(m_cityPx - c.px) < 0.01)
                stepN = ((key.mid(4).toInt() - m_key.mid(4).toInt()) % 96 + 96) % 96;
            if (stepN == 1) { m_prev = m_cityPm; m_fade = 1; }
            else { m_prev = QPixmap(); m_fade = 0; }
            {
                Seeded sd(seed());
                build(pal, c);
            }
            m_key = key;
        }
        vfill(p, QRectF(0, ROAD - 260, c.W, 260), alpha(hex("#ff9a5a"), 0), alpha(hex("#ff9a5a"), 0.12 * (1 - m_L)), ROAD - 260, ROAD);
        if (m_toggle > 0.25 && !m_wins.empty()) {
            m_toggle = 0;
            QPainter x(&m_cityPm);
            for (int i = 0; i < 3; ++i) {
                Win& w = m_wins[size_t(RI(int(m_wins.size())))];
                w.on = !w.on && rnd() < 1 - 0.85 * m_L;
                if (w.on) w.c = warmC();
                x.fillRect(QRectF(w.x, w.y, w.w, w.h), w.on ? w.c : w.bg);
            }
        }
        blit(p, m_cityPm, 0, 0, c.W, c.H);
        if (m_fade > 0 && !m_prev.isNull()) {
            p.setOpacity(m_fade);
            blit(p, m_prev, 0, 0, c.W, c.H);
            p.setOpacity(1);
        }
        if (m_water) paintWater(p, pal, c);
        for (const Ant& a : m_ants) {
            const double k = std::pow(qMax(0.0, std::sin(m_t * 2.2 + a.ph)), 6);
            disc(p, a.x, a.y, 2.8, alpha(hex("#ff3b3b"), (0.3 + 0.7 * k) * (1 - 0.5 * m_L)));
            if (k > 0.3) disc(p, a.x, a.y, 12, alpha(hex("#ff3b3b"), 0.22 * k));
        }
        if (m_trainOn) paintTrain(p, pal, c);
        paintCars(p, pal, c);
        for (const Heli& h : m_helisList) paintHeli(p, pal, c, h);
        if (m_plane.on) {
            const bool b = int(std::floor(m_t * 1.4)) % 2 == 0;
            p.fillRect(QRectF(m_plane.x, m_plane.y, 2, 2), alpha(Qt::white, b ? 0.95 : 0.3));
            p.fillRect(QRectF(m_plane.x - 8, m_plane.y + 1, 2, 2), alpha(hex("#ff4040"), b ? 0.3 : 0.9));
        }
    }

private:
    struct Win { double x, y, w, h; bool on; QColor c, bg; };
    struct Ant { double x, y, ph; };
    struct Car { int dir; double x, v, lane; QColor col; };
    struct Plane { bool on = false; double x = 0, y = 0, v = 0; };
    struct Heli { int dir; double x, y, v, ph; bool beam; };
    struct Bld { double x, w, y, h, tone, lit; bool band, pent; };

    bool m_bridgeOn() const { return m_water; }
    double road(const Ctx& c) const { return m_water ? c.H * 0.8 : c.H - 52; }
    double deck(const Ctx& c) const { return m_water ? road(c) + 28 : road(c) - 30; }   // a ponte vem pra frente
    double waterLine(const Ctx& c) const { return deck(c) + 62; }
    quint32 seed() const { return 0x9E3779B9u ^ (quint32((m_towers ? 1 : 0) | (m_helis ? 2 : 0) | (m_train ? 4 : 0) | (m_water ? 8 : 0)) * 2654435761u); }
    QColor rimC(const Ctx& c) const { return todPick(c, hex("#ffffff"), hex("#ffb08a"), hex("#ff9a6a")); }
    QColor conc(const Pal& pal) const { return mix(mix(pal.app, hex("#5a606a"), 0.35 + 0.3 * m_L), Qt::black, 0.2 * (1 - m_L)); }

    Car newCar(int dir, bool any, const Ctx& c) const
    {
        static const char* COL[6] = { "#8a2a2a", "#2a3a5a", "#c8c8cc", "#1a1a1e", "#5a5a60", "#d8d0c0" };
        const double ROAD = road(c), DECK = deck(c);
        const double lane = m_water ? (dir > 0 ? DECK - 3 : DECK - 7) : (dir > 0 ? ROAD + 12 : ROAD + 24);
        return { dir, any ? R(0, c.W) : (dir > 0 ? -40.0 : c.W + 40), R(70, 140), lane, hex(COL[RI(6)]) };
    }

    // ---------------------------------------------------------------- cenário (sempre com a mesma semente)
    void build(const Pal& pal, const Ctx& c)
    {
        m_L = LVopen(pal, c);
        QPixmap pm = layer(c, c.W, c.H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing);
        x.setPen(Qt::NoPen);
        m_wins.clear();
        m_ants.clear();
        const QColor haze = todPick(c, hex("#8fa4b8"), hex("#6a4a62"), QColor(Qt::black));
        const QColor tint = m_towers ? mix(pal.app, hex("#5a5e66"), 0.18) : pal.app;
        QColor cols[3];
        for (int k = 0; k < 3; ++k) {
            static const double D[3] = { 0.05, 0.25, 0.45 }, HZ[3] = { 0.7, 0.5, 0.3 };
            const QColor base = mix(tint, Qt::black, D[k]);
            cols[k] = (c.tod == QLatin1String("theme") || c.tod == QLatin1String("night")) ? base : mix(base, haze, qMin(1.0, HZ[k] * m_L * 1.6));
        }
        buildings(x, pal, c, cols);
        if (!m_water) roadBed(x, pal, c);
        if (m_train) viaduct(x, pal, c);
        x.end();
        m_cityPm = pm;
        m_cityPx = c.px;
        if (m_water) {
            m_bridge = layer(c, c.W, c.H);
            QPainter b(&m_bridge);
            b.setRenderHint(QPainter::Antialiasing);
            bridge(b, pal, c);
        }
    }

    void buildings(QPainter& x, const Pal& pal, const Ctx& c, const QColor (&cols)[3])
    {
        const double ROAD = road(c), VX = c.W / 2, wsc = m_water ? 0.75 : 1, L = m_L;
        const QColor hz = todPick(c, mix(pal.app, hex("#8fa4b8"), 0.7), mix(pal.app, hex("#7a5070"), 0.5), mix(pal.app, hex("#5a3a48"), 0.45));
        const QColor rim = rimC(c);
        auto hazeOver = [&](double a0, double a1) { vfill(x, QRectF(0, 0, c.W, ROAD + 20), alpha(hz, a0), alpha(hz, a1), 0, ROAD); };
        static const double OFF[3] = { -30, -10, 8 }, CK[3] = { 0.1, 0.2, 0.26 };
        for (int k = 0; k < 3; ++k) {
            // concreto um pouco mais claro que a silhueta lisa, pra fachada ler no tema escuro
            const QColor col = mix(cols[k], hex("#4a5464"), CK[k] * todPick(c, 0.4, 1.0, 1.0));
            std::vector<Bld> row;
            for (double bx = R(-30, 0); bx < c.W;) {
                Bld b;
                b.w = (k == 0 ? R(40, 80) : k == 1 ? R(50, 100) : R(60, 130)) * (m_towers ? 0.85 : 1);
                b.h = (k == 0 ? R(160, 340) : k == 1 ? R(120, 420) : R(90, 360)) * (m_towers ? 1.1 : 1) * wsc;
                b.x = bx;
                b.y = ROAD - b.h + OFF[k] * wsc;
                b.tone = R(-0.12, 0.14);
                b.lit = R(0.3, 1.4);
                b.band = rnd() < 0.3;
                b.pent = rnd() < 0.3;
                row.push_back(b);
                bx += b.w + (k == 0 ? R(0, 6) : k == 1 ? R(2, 14) : R(8, 30));
            }
            // de fora pra dentro: quem está mais perto do meio passa por cima da lateral do vizinho
            std::stable_sort(row.begin(), row.end(), [&](const Bld& a, const Bld& b) { return std::abs(a.x + a.w / 2 - VX) > std::abs(b.x + b.w / 2 - VX); });
            for (const Bld& b : row) {
                const QColor base = mix(col, b.tone > 0 ? hex("#9aa4b4") : QColor(Qt::black), std::abs(b.tone));
                const double cx = b.x + b.w / 2, bot = ROAD + 20;
                const int side = cx < VX ? 1 : -1;
                const QColor topC = mix(base, rim, 0.07 + 0.03 * k), botC = mix(base, Qt::black, 0.4);
                auto faceAt = [&](double y) {
                    const double t = (y - b.y) / (bot - b.y);
                    return t < 0.18 ? mix(topC, base, t / 0.18) : mix(base, botC, (t - 0.18) / 0.82);
                };
                // parede lateral, com a linha do telhado fugindo pro horizonte
                const double sw = qMin(5.0 + 9 * k, std::abs(cx - VX) * (0.01 + 0.008 * k));
                if (sw > 1.5) {
                    const double ex = side > 0 ? b.x + b.w : b.x, ox = ex + side * sw, drop = sw * 0.4;
                    QPolygonF sp;
                    sp << QPointF(ex, b.y) << QPointF(ox, b.y + drop) << QPointF(ox, bot) << QPointF(ex, bot);
                    QLinearGradient sg(0, b.y, 0, bot);
                    sg.setColorAt(0, mix(base, Qt::black, 0.38));
                    sg.setColorAt(1, mix(base, Qt::black, 0.72));
                    x.setBrush(sg);
                    x.drawPolygon(sp);
                    // sorteio sempre igual (acesa ou não), senão a cidade muda com a hora
                    if (k > 0)
                        for (double wy = b.y + 12; wy < ROAD - 14; wy += 10) {
                            const double r = rnd();
                            const QColor wc = warmC();
                            if (r >= 0.22 * b.lit * (1 - 0.85 * L) * lateK(c)) continue;
                            const double x0 = ex + side * sw * 0.3, x1 = ex + side * sw * 0.65, y0 = wy + drop * 0.3, y1 = wy + drop * 0.65;
                            QPolygonF wp;
                            wp << QPointF(x0, y0) << QPointF(x1, y1) << QPointF(x1, y1 + 4) << QPointF(x0, y0 + 4);
                            x.setBrush(alpha(wc, 0.5));
                            x.drawPolygon(wp);
                        }
                    x.fillRect(QRectF(side > 0 ? ex - 1 : ex, b.y, 1, bot - b.y), alpha(rim, 0.06 + 0.03 * k));
                }
                // fachada: topo pegando o brilho do céu, pé afundando no escuro
                QLinearGradient fg(0, b.y, 0, bot);
                fg.setColorAt(0, topC);
                fg.setColorAt(0.18, base);
                fg.setColorAt(1, botC);
                x.fillRect(QRectF(b.x, b.y, b.w, bot - b.y), fg);
                // casinha de máquinas, caixa-d'água e antena no telhado
                const QColor roof = mix(base, Qt::black, 0.2);
                if (b.pent) {
                    x.fillRect(QRectF(b.x + b.w * 0.3, b.y - 14, b.w * 0.4, 14), roof);
                    x.fillRect(QRectF(b.x + b.w * 0.3, b.y - 14, b.w * 0.4, 1), alpha(rim, 0.08));
                }
                if (rnd() < 0.4) {
                    const double tw = R(8, 14), tx = b.x + R(3, b.w - tw - 3);
                    x.fillRect(QRectF(tx, b.y - 9, tw, 9), roof);
                }
                x.fillRect(QRectF(b.x, b.y, b.w, 1.5), mix(base, rim, 0.18));
                if (b.h > (m_towers ? 220 : 300) && k > 0) {
                    m_ants.push_back({ cx, b.y - 40, R(0, 6.28) });
                    x.fillRect(QRectF(cx - 1, b.y - 40, 2, 26), roof);
                }
                // janelas: vidro escuro em todas; as acesas entram por cima (e apagam/acendem sem deixar marca)
                if (k == 0) {
                    const QColor wc0 = alpha(warmC(), 0.35 * (1 - 0.85 * L));
                    QVector<QRectF> rs;
                    for (double wy = b.y + 8; wy < ROAD - 10; wy += 7)
                        for (double wx = b.x + 4; wx < b.x + b.w - 4; wx += 7)
                            if (rnd() < 0.18 * b.lit) rs.append(QRectF(wx, wy, 2, 2.5));
                    x.setBrush(wc0);
                    x.drawRects(rs);
                } else {
                    const double s = k == 2 ? 11 : 8, ww = k == 2 ? 5 : 3.5, wh = k == 2 ? 6 : 4.5;
                    if (k == 2)
                        for (double wy = b.y + 10 + wh + 2; wy < ROAD; wy += s) x.fillRect(QRectF(b.x, wy, b.w, 1), alpha(Qt::black, 0.12));
                    for (double wy = b.y + 10; wy < ROAD - 14; wy += s) {
                        const QColor bg = mix(faceAt(wy), Qt::black, 0.3);
                        if (b.band) {
                            // escritório: faixa contínua de vidro por andar, acesa aos pedaços
                            x.fillRect(QRectF(b.x + 5, wy, b.w - 10, wh), bg);
                            for (double wx = b.x + 5; wx < b.x + b.w - 5 - s; wx += s) {
                                const bool on = rnd() < 0.28 * b.lit * (1 - 0.85 * L) * lateK(c);
                                const bool cool = rnd() < 0.5;
                                const QColor wc = warmC();
                                const Win w{ wx, wy, s, wh, on, cool ? hex("#e8f0ff") : wc, bg };
                                m_wins.push_back(w);
                                if (w.on) x.fillRect(QRectF(w.x, w.y, w.w, w.h), w.c);
                            }
                        } else {
                            for (double wx = b.x + 6; wx < b.x + b.w - 6; wx += s) {
                                const bool on = rnd() < (m_towers ? 0.26 : 0.32) * b.lit * (1 - 0.85 * L) * lateK(c);
                                const bool cool = rnd() < 0.4;
                                const QColor wc = warmC();
                                const Win w{ wx, wy, ww, wh, on, (m_towers && cool) ? hex("#e8f0ff") : wc, bg };
                                m_wins.push_back(w);
                                x.fillRect(QRectF(w.x, w.y, w.w, w.h), w.on ? w.c : w.bg);
                                if (w.on && k == 2) x.fillRect(QRectF(wx, wy, ww, 1), alpha(Qt::black, 0.3));
                            }
                        }
                    }
                }
                // canto escuro onde um prédio encosta no outro
                for (int e = 0; e < 2; ++e) {
                    const double ex = e ? b.x + b.w : b.x, dir = e ? -1 : 1, aw = 8 + 5 * k;
                    QLinearGradient g(ex, 0, ex + dir * aw, 0);
                    g.setColorAt(0, alpha(Qt::black, 0.3 + 0.1 * k));
                    g.setColorAt(1, alpha(Qt::black, 0));
                    x.fillRect(QRectF(qMin(ex, ex + dir * aw), b.y, aw, bot - b.y), g);
                }
            }
            if (m_towers && k == 1) tvTowers(x, c, col);
            // névoa entre os planos: o fundo some, o meio esmaece
            const double hk = todPick(c, 0.6, 1.0, 1.0);
            if (k == 0) hazeOver(0.12 * hk, 0.5 * hk);
            if (k == 1) hazeOver(0.04 * hk, 0.26 * hk);
        }
    }

    // torres de TV da Paulista nos pedaços de mesa à vista: perna do lado da luz, perna na sombra,
    // plataforma com as antenas de micro-ondas e o mastro listrado
    void tvTowers(QPainter& x, const Ctx& c, const QColor& col)
    {
        const double ROAD = road(c);
        const double txs[4][2] = { { inLeftZ(c, 0.35, c.W * 0.12), R(440, 520) }, { inLeftZ(c, 0.8, c.W * 0.23), R(380, 450) },
                                   { inRightZ(c, 0.3, c.W * 0.82), R(420, 500) }, { inRightZ(c, 0.75, c.W * 0.94), R(380, 460) } };
        const QColor tc = mix(mix(col, hex("#9aa0aa"), todPick(c, 0.1, 0.28, 0.28)), Qt::black, 0.05);
        const QColor lit = mix(tc, rimC(c), 0.35), shd = mix(tc, Qt::black, 0.35);
        x.setBrush(Qt::NoBrush);
        for (const auto& tw : txs) {
            const double tx = tw[0], th = tw[1], top = ROAD - th;
            auto half = [&](double yy) { return 14 * (yy - top) / th + 2; };
            x.setPen(QPen(lit, 2.4));
            x.drawLine(QPointF(tx - half(ROAD), ROAD), QPointF(tx - 2, top));
            x.setPen(QPen(shd, 2.4));
            x.drawLine(QPointF(tx + 2, top), QPointF(tx + half(ROAD), ROAD));
            x.setPen(QPen(tc, 1.3));
            for (double yy = top + 20; yy < ROAD; yy += 20) {
                const double a = half(yy), b = half(yy + 20);
                x.drawLine(QPointF(tx - a, yy), QPointF(tx + b, yy + 20));
                x.drawLine(QPointF(tx + a, yy), QPointF(tx - b, yy + 20));
                x.drawLine(QPointF(tx - a, yy), QPointF(tx + a, yy));
            }
            x.setPen(Qt::NoPen);
            const double py = top + th * 0.3, pw = half(py) * 2 + 10;
            x.fillRect(QRectF(tx - pw / 2, py, pw, 7), shd);
            x.fillRect(QRectF(tx - pw / 2, py, pw, 1.2), lit);
            x.setBrush(mix(tc, hex("#e8e8ec"), 0.35));
            for (double dx : { -pw / 2 - 3, pw / 2 - 1 }) x.drawEllipse(QPointF(tx + dx + 2, py - 3), 2.5, 4);
            for (int i = 0; i < 5; ++i) x.fillRect(QRectF(tx - 1.2, top - 50 + i * 10, 2.4, 10), i % 2 ? mix(hex("#e8e8ec"), tc, 0.45) : mix(hex("#c83a2a"), tc, 0.4));
            const double ph = R(0, 6.28);
            for (double yy : { top - 50, top, py }) m_ants.push_back({ tx, yy, ph });
        }
        x.setBrush(Qt::NoBrush);
    }

    // rua: asfalto, calçada com meio-fio, faixa pintada e postes com o brilho no chão
    void roadBed(QPainter& x, const Pal& pal, const Ctx& c)
    {
        const double ROAD = road(c), lampA = 1 - 0.85 * m_L;
        const QColor cc = conc(pal), rim = rimC(c);
        vfill(x, QRectF(0, ROAD, c.W, c.H - ROAD), mix(mix(pal.app, hex("#22242a"), 0.5), hex("#6a6e76"), m_L * 0.5), mix(pal.app, Qt::black, 0.5), ROAD, c.H);
        x.fillRect(QRectF(0, ROAD - 3, c.W, 5), mix(cc, Qt::black, 0.1));
        x.fillRect(QRectF(0, ROAD - 3, c.W, 1), mix(cc, rim, 0.25));
        x.fillRect(QRectF(0, ROAD + 2, c.W, 2), alpha(Qt::black, 0.4));
        QVector<QRectF> dashes;
        for (double lx = 0; lx < c.W; lx += 50) dashes.append(QRectF(lx, ROAD + 18, 24, 1.5));
        x.setPen(Qt::NoPen);
        x.setBrush(alpha(hex("#d8c070"), 0.35 + 0.2 * m_L));
        x.drawRects(dashes);
        for (double px = 40; px < c.W; px += 160) {
            x.fillRect(QRectF(px - 1, ROAD - 64, 2.5, 62), mix(cc, Qt::black, 0.45));
            x.fillRect(QRectF(px - 1, ROAD - 64, 12, 2), mix(cc, Qt::black, 0.45));
            x.fillRect(QRectF(px + 7, ROAD - 64, 7, 3), mix(cc, Qt::black, 0.3));
            if (lampA > 0.05) {
                x.setCompositionMode(QPainter::CompositionMode_Plus);
                QRadialGradient g(QPointF(px + 10, ROAD - 60), 46);
                g.setColorAt(0, alpha(hex("#ffd28a"), 0.3 * lampA));
                g.setColorAt(1, alpha(hex("#ffd28a"), 0));
                x.fillRect(QRectF(px - 36, ROAD - 106, 92, 92), g);
                x.save();
                x.translate(px + 10, ROAD + 14);
                x.scale(1, 0.25);
                QRadialGradient pool(QPointF(0, 0), 60);
                pool.setColorAt(0, alpha(hex("#ffd28a"), 0.2 * lampA));
                pool.setColorAt(1, alpha(hex("#ffd28a"), 0));
                x.fillRect(QRectF(-60, -60, 120, 120), pool);
                x.restore();
                x.fillRect(QRectF(px + 8, ROAD - 61, 5, 1.5), alpha(hex("#fff4d0"), 0.95 * lampA));
                x.setCompositionMode(QPainter::CompositionMode_SourceOver);
            }
        }
    }

    // mureta do viaduto: desenhada no cenário e de novo por cima do trem (esconde as rodas)
    void parapet(QPainter& x, const Pal& pal, const Ctx& c) const
    {
        const double TR = road(c) - 92;
        const QColor cc = conc(pal);
        vfill(x, QRectF(0, TR - 6, c.W, 6), mix(cc, rimC(c), 0.22), mix(cc, Qt::black, 0.2), TR - 6, TR);
        QVector<QRectF> j;
        for (double jx = 20; jx < c.W; jx += 65) j.append(QRectF(jx, TR - 6, 1, 6));
        x.setPen(Qt::NoPen);
        x.setBrush(alpha(Qt::black, 0.3));
        x.drawRects(j);
        x.setBrush(Qt::NoBrush);
    }

    void viaduct(QPainter& x, const Pal& pal, const Ctx& c)
    {
        const double ROAD = road(c), TR = ROAD - 92, lampA = 1 - 0.85 * m_L;
        const QColor cc = conc(pal), dk = mix(cc, Qt::black, 0.45), rim = rimC(c);
        // sombra que o tabuleiro joga nos prédios atrás
        vfill(x, QRectF(0, TR + 12, c.W, 48), alpha(Qt::black, 0.4), alpha(Qt::black, 0), TR + 12, TR + 60);
        for (double px = 70; px < c.W + 70; px += 130) {
            // pilar: lado da luz, lado da sombra, capitel e sapata
            QLinearGradient g(px - 7, 0, px + 7, 0);
            g.setColorAt(0, mix(cc, rim, 0.12));
            g.setColorAt(0.45, cc);
            g.setColorAt(1, dk);
            x.fillRect(QRectF(px - 7, TR + 12, 14, ROAD - TR - 12), g);
            x.fillRect(QRectF(px - 12, TR + 12, 24, 7), mix(cc, Qt::black, 0.15));
            vfill(x, QRectF(px - 7, TR + 19, 14, 21), alpha(Qt::black, 0.45), alpha(Qt::black, 0), TR + 19, TR + 40);
            x.fillRect(QRectF(px - 10, ROAD - 4, 20, 4), mix(cc, Qt::black, 0.25));
            // luminária embaixo do vão, com o cone de luz até a rua
            const double lx = px + 65;
            x.fillRect(QRectF(lx - 4, TR + 12, 8, 3), mix(cc, Qt::black, 0.3));
            if (lampA > 0.05) {
                x.setCompositionMode(QPainter::CompositionMode_Plus);
                QLinearGradient cg(0, TR + 15, 0, ROAD + 10);
                cg.setColorAt(0, alpha(hex("#ffcf8a"), 0.16 * lampA));
                cg.setColorAt(1, alpha(hex("#ffcf8a"), 0));
                QPolygonF cone;
                cone << QPointF(lx - 4, TR + 15) << QPointF(lx + 4, TR + 15) << QPointF(lx + 42, ROAD + 10) << QPointF(lx - 42, ROAD + 10);
                x.setPen(Qt::NoPen);
                x.setBrush(cg);
                x.drawPolygon(cone);
                x.fillRect(QRectF(lx - 3, TR + 15, 6, 1.5), alpha(hex("#fff0c8"), 0.9 * lampA));
                x.setCompositionMode(QPainter::CompositionMode_SourceOver);
            }
        }
        // tabuleiro: viga de concreto com o topo pegando luz, juntas e a aba de baixo
        QLinearGradient g(0, TR, 0, TR + 12);
        g.setColorAt(0, mix(cc, rim, 0.18));
        g.setColorAt(0.3, cc);
        g.setColorAt(1, dk);
        x.fillRect(QRectF(0, TR, c.W, 12), g);
        for (double jx = 20; jx < c.W; jx += 65) x.fillRect(QRectF(jx, TR, 1, 12), alpha(Qt::black, 0.25));
        x.fillRect(QRectF(0, TR + 11, c.W, 1.5), alpha(Qt::black, 0.5));
        // catenária: postes, braço e o fio caindo um pouco entre eles
        const QColor wc = mix(cc, Qt::black, 0.5);
        for (double cp = 30; cp < c.W + 130; cp += 130) {
            x.fillRect(QRectF(cp - 1.5, TR - 56, 3, 50), wc);
            x.fillRect(QRectF(cp - 1.5, TR - 54, 16, 2), wc);
        }
        QPainterPath wire;
        for (double cp = 30; cp < c.W + 130; cp += 130) {
            wire.moveTo(cp + 12, TR - 50);
            wire.quadTo(cp + 77, TR - 46, cp + 142, TR - 50);
        }
        x.setBrush(Qt::NoBrush);
        x.setPen(QPen(alpha(wc, 0.9), 1));
        x.drawPath(wire);
        x.setPen(Qt::NoPen);
        parapet(x, pal, c);
    }

    // ponte pênsil em primeiro plano, 1,6x maior: torres com holofote e base de pedra na linha d'água,
    // tabuleiro com treliça e guarda-corpo, cabo com brilho e as luzes com halo
    void bridge(QPainter& x, const Pal& pal, const Ctx& c)
    {
        const double DECK = deck(c), WL = waterLine(c), top = DECK - 330, S = 1.6, la = 0.95 * (1 - 0.8 * m_L);
        const double b1 = inLeftZ(c, 0.55, c.W * 0.17), b2 = inRightZ(c, 0.5, c.W * 0.87);
        const QColor cc = mix(mix(pal.app, hex("#7d8ea0"), 0.24), Qt::black, 0.15), rim = rimC(c);
        const QColor lit = mix(cc, rim, 0.3), shd = mix(cc, Qt::black, 0.5);
        auto cable = [&](double xx) {
            if (xx < b1) return top + (DECK - top) * std::pow((b1 - xx) / b1, 1.6) * 0.95;
            if (xx > b2) return top + (DECK - top) * std::pow((xx - b2) / (c.W - b2), 1.6) * 0.95;
            const double u = (xx - (b1 + b2) / 2) / ((b2 - b1) / 2);
            return DECK - 12 * S - (DECK - 12 * S - top) * u * u;
        };
        x.setPen(Qt::NoPen);
        for (double bt : { b1, b2 }) {
            // base de pedra na água
            QLinearGradient g(bt - 30, 0, bt + 30, 0);
            g.setColorAt(0, mix(cc, rim, 0.15));
            g.setColorAt(0.5, cc);
            g.setColorAt(1, shd);
            x.fillRect(QRectF(bt - 30, WL - 18, 60, 18), g);
            x.fillRect(QRectF(bt - 30, WL - 18, 60, 1.5), lit);
            for (double yy = WL - 12; yy < WL; yy += 6) x.fillRect(QRectF(bt - 30, yy, 60, 1), alpha(Qt::black, 0.25));
            // pernas
            for (double lx : { bt - 21, bt + 7 }) {
                QLinearGradient lg(lx, 0, lx + 14, 0);
                lg.setColorAt(0, mix(lit, rim, 0.2));
                lg.setColorAt(0.35, cc);
                lg.setColorAt(1, shd);
                x.fillRect(QRectF(lx, top, 14, WL - 18 - top), lg);
            }
            // travessas e o topo
            for (double by : { top + 48, top + 160 }) {
                x.fillRect(QRectF(bt - 21, by, 42, 11), cc);
                x.fillRect(QRectF(bt - 21, by, 42, 1.6), lit);
                x.fillRect(QRectF(bt - 7, by + 11, 14, 5), alpha(Qt::black, 0.45));
            }
            x.fillRect(QRectF(bt - 24, top - 7, 48, 8), shd);
            x.fillRect(QRectF(bt - 24, top - 7, 48, 1.5), lit);
            // holofote no pé da torre, lavando as pernas de baixo pra cima
            if (la > 0.05) {
                x.setCompositionMode(QPainter::CompositionMode_Plus);
                vfill(x, QRectF(bt - 21, top, 42, WL - 18 - top), alpha(hex("#ffd8a0"), 0.04 * la), alpha(hex("#ffd8a0"), 0.4 * la), top, WL - 18);
                x.setCompositionMode(QPainter::CompositionMode_SourceOver);
            }
            m_ants.push_back({ bt, top - 10, 0 });
        }
        // tabuleiro: viga, treliça e banzo de baixo
        QLinearGradient dg(0, DECK, 0, DECK + 16);
        dg.setColorAt(0, lit);
        dg.setColorAt(0.35, cc);
        dg.setColorAt(1, shd);
        x.fillRect(QRectF(0, DECK, c.W, 16), dg);
        for (double xx = 10; xx < c.W; xx += 80) x.fillRect(QRectF(xx, DECK, 1, 16), alpha(Qt::black, 0.3));
        QPainterPath truss;
        for (double xx = 0; xx < c.W; xx += 22) {
            truss.moveTo(xx, DECK + 16);
            truss.lineTo(xx + 11, DECK + 32);
            truss.lineTo(xx + 22, DECK + 16);
        }
        x.setBrush(Qt::NoBrush);
        x.setPen(QPen(shd, 1.6));
        x.drawPath(truss);
        x.setPen(Qt::NoPen);
        x.fillRect(QRectF(0, DECK + 31, c.W, 3), shd);
        x.fillRect(QRectF(0, DECK + 31, c.W, 1), alpha(lit, 0.5));
        // guarda-corpo
        const QColor rail = mix(cc, Qt::black, 0.15);
        x.fillRect(QRectF(0, DECK - 8, c.W, 1.5), rail);
        for (double xx = 0; xx < c.W; xx += 16) x.fillRect(QRectF(xx, DECK - 8, 1.2, 8), rail);
        // pendurais e o cabo principal, com o fio de luz por cima
        x.setPen(QPen(alpha(cc, 0.85), 1.2));
        for (double xx = 10; xx < c.W; xx += 24) x.drawLine(QPointF(xx, cable(xx)), QPointF(xx, DECK - 8));
        QPainterPath cab(QPointF(0, cable(0))), hi(QPointF(0, cable(0) - 1.5));
        for (double xx = 5; xx <= c.W; xx += 5) { cab.lineTo(xx, cable(xx)); hi.lineTo(xx, cable(xx) - 1.5); }
        x.setPen(QPen(shd, 4.5));
        x.drawPath(cab);
        x.setPen(QPen(lit, 1.6));
        x.drawPath(hi);
        x.setPen(Qt::NoPen);
        // luzes do cabo e do tabuleiro, com halo
        if (la > 0.05) {
            x.setCompositionMode(QPainter::CompositionMode_Plus);
            auto dot = [&](double xx, double yy, double r) {
                glow(x, xx, yy, r * 4, hex("#ffe6b0"), 0.35 * la);
                disc(x, xx, yy, r, alpha(hex("#fff2d0"), la));
            };
            for (double xx = 8; xx < c.W; xx += 32) dot(xx, cable(xx) - 1, 3);
            for (double xx = 4; xx < c.W; xx += 40) dot(xx, DECK - 9, 2.4);
            vfill(x, QRectF(0, DECK - 45, c.W, 61), alpha(hex("#ffc46b"), 0), alpha(hex("#ffc46b"), 0.16 * (1 - m_L)), DECK - 45, DECK + 16);
            x.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
    }

    // ---------------------------------------------------------------- por quadro
    void paintWater(QPainter& p, const Pal& pal, const Ctx& c)
    {
        const double ROAD = road(c), WL = waterLine(c), la = 1 - 0.8 * m_L;
        // água: o reflexo da cidade, de cabeça pra baixo, tremendo em faixas
        p.fillRect(QRectF(0, ROAD, c.W, c.H - ROAD), mix(mix(pal.app, Qt::black, 0.45), hex("#3f6f8f"), m_L * 0.6));
        p.setOpacity(0.42);
        for (double y = ROAD; y < c.H; y += 3) {
            const double d = y - ROAD, src = ROAD - d * 1.3 - 3, sh = std::sin(m_t * 2.2 + y * 0.21) * (1.5 + d * 0.05);
            if (src > 0) p.drawPixmap(QRectF(sh, y, c.W, 3), m_cityPm, QRectF(0, src * m_cityPx, c.W * m_cityPx, 3 * m_cityPx));
        }
        p.setOpacity(1);
        vfill(p, QRectF(0, ROAD, c.W, c.H - ROAD), alpha(pal.app, 0), alpha(pal.app, 0.45), ROAD, c.H);
        // cais lá atrás
        const QColor cc = conc(pal);
        p.fillRect(QRectF(0, ROAD - 2, c.W, 5), mix(cc, Qt::black, 0.15));
        p.fillRect(QRectF(0, ROAD - 2, c.W, 1), mix(cc, rimC(c), 0.25));
        p.fillRect(QRectF(0, ROAD + 3, c.W, 3), alpha(Qt::black, 0.35));
        if (m_bridge.isNull()) return;
        // a ponte com o próprio reflexo e as luzes dela tremendo na água
        p.save();
        p.setClipRect(QRectF(0, WL, c.W, c.H - WL), Qt::IntersectClip);
        p.setOpacity(0.3);
        p.translate(0, WL * 2);
        p.scale(1, -1);
        blit(p, m_bridge, 0, 0, c.W, c.H);
        p.restore();
        if (la > 0.05) {
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            for (double xx = 4; xx < c.W; xx += 40)
                for (int j = 0; j < 14; ++j) {
                    const double y = WL + 3 + j * 4.5, k = 1 - j / 14.0, f = 0.5 + 0.5 * std::sin(m_t * 2.6 + xx * 0.7 + j * 1.3), w = 4 + 9 * f * (0.4 + j / 14.0);
                    p.fillRect(QRectF(xx - w / 2 + std::sin(m_t * 1.7 + j + xx) * 2.5, y, w, 1.8), alpha(hex("#ffd8a0"), 0.5 * k * f * la));
                }
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        blit(p, m_bridge, 0, 0, c.W, c.H);
    }

    void paintTrain(QPainter& p, const Pal& pal, const Ctx& c)
    {
        const double TR = road(c) - 92, cw = 82, gap = 4, top = TR - 33, bh = 27, sd = m_trainSeed, L = m_L;
        const int n = 7, dir = m_trainDir;
        const QColor body = mix(mix(pal.app, hex("#a8b0bc"), 0.3 + 0.4 * L), Qt::black, 0.12 * (1 - L));
        const QColor glass = mix(body, Qt::black, 0.6), stripe = mix(hex("#c83a2a"), body, 0.35), dkr = mix(body, Qt::black, 0.55), rim = rimC(c);
        const double warm = 1 - 0.7 * L;
        p.setPen(Qt::NoPen);
        for (int i = 0; i < n; ++i) {
            const double cx = m_trainX + i * (cw + gap);
            if (cx > c.W + 10 || cx + cw < -10) continue;
            const bool lead = dir > 0 ? i == n - 1 : i == 0, tail = dir > 0 ? i == 0 : i == n - 1;
            // engate
            if (i < n - 1) p.fillRect(QRectF(cx + cw, top + bh - 9, gap, 4), dkr);
            // caixa do vagão: teto pegando luz, barriga no escuro; o da frente com o nariz inclinado
            QLinearGradient g(0, top, 0, top + bh);
            g.setColorAt(0, mix(body, rim, 0.3));
            g.setColorAt(0.25, body);
            g.setColorAt(1, mix(body, Qt::black, 0.35));
            QPainterPath box;
            if (lead && dir > 0) {
                box.moveTo(cx, top + 3); box.lineTo(cx + cw - 14, top); box.quadTo(cx + cw - 2, top + 2, cx + cw, top + 15);
                box.lineTo(cx + cw, top + bh); box.lineTo(cx, top + bh);
            } else if (lead) {
                box.moveTo(cx + cw, top + 3); box.lineTo(cx + 14, top); box.quadTo(cx + 2, top + 2, cx, top + 15);
                box.lineTo(cx, top + bh); box.lineTo(cx + cw, top + bh);
            } else {
                box.addRoundedRect(QRectF(cx, top, cw, bh + 3), 3, 3);
            }
            box.closeSubpath();
            p.save();
            p.setClipRect(QRectF(cx - 1, top - 1, cw + 2, bh + 1));
            p.fillPath(box, g);
            p.restore();
            // ar-condicionado no teto
            p.fillRect(QRectF(cx + 18, top - 3, 16, 3), mix(body, Qt::black, 0.25));
            p.fillRect(QRectF(cx + cw - 34, top - 3, 16, 3), mix(body, Qt::black, 0.25));
            // faixa de janelas: vidro escuro, as acesas com gente dentro de vez em quando
            const double wy = top + 6, wh = 10;
            p.fillRect(QRectF(cx + (lead && dir < 0 ? 12 : 5), wy, cw - (lead ? 17 : 10), wh), glass);
            int k = 0;
            for (double wx = cx + 7; wx < cx + cw - 12; wx += 11, ++k) {
                if (lead && ((dir > 0 && wx > cx + cw - 22) || (dir < 0 && wx < cx + 12))) continue;
                if (hsh(sd + i, k) < 0.15) continue;
                if (L > 0.6) {
                    p.fillRect(QRectF(wx, wy + 1, 8, wh - 2), mix(glass, hex("#8a9aaa"), 0.3));
                } else {
                    QLinearGradient lg(0, wy, 0, wy + wh);
                    lg.setColorAt(0, alpha(hex("#fff2cc"), 0.95 * warm));
                    lg.setColorAt(1, alpha(hex("#ffcf8a"), 0.8 * warm));
                    p.fillRect(QRectF(wx, wy + 1, 8, wh - 2), lg);
                }
                if (hsh(sd * 3 + i, k) < 0.35) {
                    const QColor ppl = alpha(hex("#1a1410"), 0.75 * warm + 0.2);
                    const double hx = wx + 2 + hsh(i, k + sd) * 4;
                    disc(p, hx, wy + 4.5, 1.6, ppl);
                    p.fillRect(QRectF(hx - 2.4, wy + 6, 4.8, wh - 7), ppl);
                }
            }
            // portas, a faixa vermelha e a saia
            for (double dx : { 0.33, 0.66 }) {
                const double d = cx + cw * dx;
                p.fillRect(QRectF(d - 4, top + 4, 1, bh - 6), alpha(Qt::black, 0.35));
                p.fillRect(QRectF(d + 4, top + 4, 1, bh - 6), alpha(Qt::black, 0.35));
            }
            p.fillRect(QRectF(cx + (lead && dir < 0 ? 4 : 0), top + 19, cw - (lead ? 4 : 0), 2.5), stripe);
            p.fillRect(QRectF(cx + 2, top + bh - 3, cw - 4, 3), dkr);
            // truques (rodas), que a mureta esconde pela metade
            p.fillRect(QRectF(cx + 8, top + bh, 18, 6), mix(body, Qt::black, 0.7));
            p.fillRect(QRectF(cx + cw - 26, top + bh, 18, 6), mix(body, Qt::black, 0.7));
            // pantógrafo no segundo vagão, encostando no fio
            if (i == 1) {
                const double px = cx + cw / 2;
                QPainterPath pg(QPointF(px - 8, top - 1));
                pg.lineTo(px + 2, TR - 42);
                pg.lineTo(px - 6, TR - 49);
                pg.moveTo(px - 12, TR - 49);
                pg.lineTo(px + 2, TR - 49);
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(dkr, 1.2));
                p.drawPath(pg);
                p.setPen(Qt::NoPen);
            }
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            // luz das janelas caindo na mureta
            if (warm > 0.2) p.fillRect(QRectF(cx, top + bh, cw, 8), alpha(hex("#ffcf8a"), 0.07 * warm));
            // faróis na frente, lanterna vermelha atrás
            if (lead) {
                const double fx = dir > 0 ? cx + cw - 2 : cx + 2;
                p.fillRect(QRectF(fx - 1.5, top + 17, 3, 3), alpha(hex("#fff8e0"), 0.95));
                glow(p, fx, top + 18, 70, hex("#fff6d8"), 0.5 * (1 - 0.6 * L));
                QLinearGradient bg(fx, 0, fx + dir * 160, 0);
                bg.setColorAt(0, alpha(hex("#fff6d8"), 0.22 * (1 - 0.6 * L)));
                bg.setColorAt(1, alpha(hex("#fff6d8"), 0));
                QPolygonF beam;
                beam << QPointF(fx, top + 17) << QPointF(fx + dir * 160, top + 6) << QPointF(fx + dir * 160, top + 32) << QPointF(fx, top + 20);
                p.setBrush(bg);
                p.drawPolygon(beam);
            }
            if (tail) {
                const double rx = dir > 0 ? cx + 2 : cx + cw - 2;
                p.fillRect(QRectF(rx - 1.5, top + 17, 3, 3), alpha(hex("#ff3020"), 0.9));
                glow(p, rx, top + 18, 18, hex("#ff3020"), 0.4);
            }
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        p.setBrush(Qt::NoBrush);
        parapet(p, pal, c);
    }

    void paintCars(QPainter& p, const Pal& pal, const Ctx& c)
    {
        Q_UNUSED(pal);
        // carroceria e cabine (na ponte, na escala do tabuleiro), com o farol/lanterna por cima
        const double q = m_water ? 0.8 : 1;
        const QColor rim = rimC(c);
        for (const Car& car : m_cars) {
            const double len = 30 * q, x0 = car.dir > 0 ? car.x - len : car.x, y = car.lane;
            const QColor col = mix(car.col, Qt::black, 0.5 * (1 - m_L));
            p.fillRect(QRectF(x0 + 1, y + 3 * q, len, 2 * q), alpha(Qt::black, 0.35));
            p.fillRect(QRectF(x0, y - 4 * q, len, 7 * q), col);
            p.fillRect(QRectF(x0 + 8 * q, y - 8 * q, 14 * q, 4 * q), mix(col, Qt::black, 0.25));
            p.fillRect(QRectF(x0 + 9 * q, y - 7 * q, 12 * q, 2.5 * q), alpha(mix(hex("#8a9aaa"), col, 0.4), 0.8));
            p.fillRect(QRectF(x0, y - 4 * q, len, 1), mix(col, rim, 0.25));
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
    }

    // helicóptero com corpo, cabine de vidro, cauda, esquis e o disco do rotor
    void paintHeli(QPainter& p, const Pal& pal, const Ctx& c, const Heli& h)
    {
        const double y = h.y + std::sin(m_t * 1.3 + h.ph) * 4, d = h.dir;
        if (h.beam && m_L < 0.5) {
            QLinearGradient g(0, y, 0, y + 300);
            g.setColorAt(0, alpha(hex("#fff6e0"), 0.16));
            g.setColorAt(1, alpha(hex("#fff6e0"), 0));
            QPolygonF beam;
            beam << QPointF(h.x - 2, y + 4) << QPointF(h.x - 40 - d * 30, y + 300) << QPointF(h.x + 40 - d * 30, y + 300) << QPointF(h.x + 2, y + 4);
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawPolygon(beam);
        }
        const QColor bc = mix(mix(pal.app, hex("#5a606a"), 0.3 + 0.35 * m_L), Qt::black, 0.25 * (1 - m_L)), dk = mix(bc, Qt::black, 0.45);
        p.setPen(Qt::NoPen);
        p.setBrush(bc);
        QPolygonF boom;
        boom << QPointF(h.x - d * 6, y - 2.5) << QPointF(h.x - d * 25, y - 2) << QPointF(h.x - d * 25, y) << QPointF(h.x - d * 6, y + 2.5);
        p.drawPolygon(boom);
        QPolygonF fin;
        fin << QPointF(h.x - d * 22, y - 2) << QPointF(h.x - d * 27, y - 8) << QPointF(h.x - d * 26, y);
        p.drawPolygon(fin);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(alpha(dk, 0.6), 0.8));
        p.drawEllipse(QPointF(h.x - d * 26, y - 4), 3.5, 3.5);
        p.setPen(QPen(dk, 1));
        p.drawLine(QPointF(h.x - 8, y + 8), QPointF(h.x + 9, y + 8));
        p.drawLine(QPointF(h.x - 4, y + 4), QPointF(h.x - 5, y + 8));
        p.drawLine(QPointF(h.x + 4, y + 4), QPointF(h.x + 5, y + 8));
        p.setPen(Qt::NoPen);
        QLinearGradient g(0, y - 6, 0, y + 6);
        g.setColorAt(0, mix(bc, rimC(c), 0.35));
        g.setColorAt(0.4, bc);
        g.setColorAt(1, dk);
        p.setBrush(g);
        p.drawEllipse(QPointF(h.x, y), 10.5, 5.8);
        // cabine de vidro na frente
        QPainterPath cab;
        cab.addEllipse(QPointF(h.x + d * 5, y - 0.8), 5, 3.8);
        QPainterPath front;
        front.addRect(d > 0 ? QRectF(h.x + 2, y - 6, 12, 12) : QRectF(h.x - 14, y - 6, 12, 12));
        p.fillPath(cab.intersected(front), alpha(mix(hex("#7ab0d8"), bc, 0.3), 0.85));
        p.fillRect(QRectF(h.x + d * 5 - 1.5, y - 3.5, 3, 0.8), alpha(Qt::white, 0.35));
        // mastro e o disco borrado do rotor, com uma pá passando
        p.fillRect(QRectF(h.x - 1, y - 8.5, 2, 3), dk);
        p.setBrush(alpha(dk, 0.3));
        p.drawEllipse(QPointF(h.x, y - 9), 24, 1.6);
        const double r = 24 * std::abs(std::sin(m_t * 40));
        p.setPen(QPen(alpha(dk, 0.8), 1));
        p.drawLine(QPointF(h.x - r, y - 9), QPointF(h.x + r, y - 9));
        p.setPen(Qt::NoPen);
        const bool b = int(std::floor(m_t * 1.6 + h.ph)) % 2 == 0;
        disc(p, h.x, y + 5, 1.8, alpha(hex("#ff3b3b"), b ? 0.95 : 0.25));
        if (std::sin(m_t * 9 + h.ph) > 0.92) disc(p, h.x - d * 20, y, 1.8, alpha(Qt::white, 0.95));
    }

    bool m_towers, m_helis, m_train, m_water;
    QPixmap m_cityPm, m_prev, m_bridge;
    double m_cityPx = 0, m_fade = 0;
    QString m_key;
    std::vector<Win> m_wins;
    std::vector<Ant> m_ants;
    std::vector<Car> m_cars;
    std::vector<Heli> m_helisList;
    Plane m_plane;
    double m_t = 0, m_toggle = 0, m_nextPlane = 4, m_nextHeli = 1, m_L = 0;
    bool m_trainOn = false;
    int m_trainDir = 1;
    double m_trainX = 0, m_trainV = 0, m_nextTrain = 2, m_trainSeed = 1;
};

} // namespace

std::unique_ptr<Layer> makeCityDepth(bool towers, bool helis, bool train, bool water)
{
    return std::make_unique<CityDepth>(towers, helis, train, water);
}

} // namespace AnimBg
