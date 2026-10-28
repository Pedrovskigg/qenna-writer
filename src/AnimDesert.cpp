// Temas Animados do deserto (16/10): Last Exit (o posto solitário, visto do outro lado da estrada)
// e Route 66 (o posto com o letreiro de neon dos anos 50, a estrada sumindo entre as mesas).
// Portados de lastexit.js e route66.js do Weather Lab. As duas cenas são desenhadas em perspectiva
// de verdade: o mundo é medido em metros (X pra direita, Y pra cima, Z pra frente) e projetado
// por uma câmera a 2 m do chão. O cenário parado é montado uma vez (com semente fixa) em imagens
// prontas; por quadro só as luzes, as mariposas e o que passa na estrada. Só noite.

#include "AnimKit.h"

#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>

namespace AnimBg {
namespace {

using namespace kit;

// ============================================================ perspectiva

struct V3 { double x, y, z; };
using Pts = std::vector<V3>;

struct Cam {
    double VX = 120, HY = 468, F = 420, CH = 2.0, ZN = 0.9;
    QPointF P(double X, double Y, double Z) const { return QPointF(VX + F * X / Z, HY + F * (CH - Y) / Z); }
    double sxX(double sx, double Z) const { return (sx - VX) * Z / F; }   // X do mundo que cai num x da tela
    // corta no plano Z = ZN (o que fica atrás da câmera não pode ser projetado)
    Pts clip(const Pts& pts) const
    {
        Pts out;
        const size_t n = pts.size();
        for (size_t i = 0; i < n; ++i) {
            const V3& a = pts[i];
            const V3& b = pts[(i + 1) % n];
            const bool ia = a.z >= ZN, ib = b.z >= ZN;
            if (ia) out.push_back(a);
            if (ia != ib) {
                const double t = (ZN - a.z) / (b.z - a.z);
                out.push_back({ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, ZN });
            }
        }
        return out;
    }
    bool poly(const Pts& pts, QPolygonF& out) const
    {
        const Pts q = clip(pts);
        out.clear();
        for (const V3& v : q) out << P(v.x, v.y, v.z);
        return q.size() > 2;
    }
    void face(QPainter& p, const Pts& pts, const QBrush& b) const
    {
        QPolygonF q;
        if (!poly(pts, q)) return;
        p.setPen(Qt::NoPen);
        p.setBrush(b);
        p.drawPolygon(q);
    }
};

Pts fX(double X, double y0, double y1, double z0, double z1) { return { { X, y0, z0 }, { X, y1, z0 }, { X, y1, z1 }, { X, y0, z1 } }; }
Pts fZ(double Z, double x0, double x1, double y0, double y1) { return { { x0, y0, Z }, { x1, y0, Z }, { x1, y1, Z }, { x0, y1, Z } }; }
Pts fY(double Y, double x0, double x1, double z0, double z1) { return { { x0, Y, z0 }, { x1, Y, z0 }, { x1, Y, z1 }, { x0, Y, z1 } }; }

QLinearGradient vgrad(double y0, double y1, const QColor& c0, const QColor& c1)
{
    QLinearGradient g(0, y0, 0, y1);
    g.setColorAt(0, c0);
    g.setColorAt(1, c1);
    return g;
}

// brilho radial com o miolo mais concentrado (igual ao glowAt do Lab)
void glowAt(QPainter& p, double x, double y, double r, const QColor& c, double a)
{
    if (a <= 0.003 || r < 0.5) return;
    QRadialGradient g(QPointF(x, y), r);
    g.setColorAt(0, alpha(c, a));
    g.setColorAt(0.35, alpha(c, a * 0.4));
    g.setColorAt(1, alpha(c, 0));
    p.fillRect(QRectF(x - r, y - r, r * 2, r * 2), g);
}

// fonte: 0 = sans, 1 = mono, 2 = black (Arial Black)
QFont fnt(double px, int weight, int fam = 0, bool italic = false)
{
    QFont f;
    if (fam == 1) f.setFamilies({ QStringLiteral("IBM Plex Mono"), QStringLiteral("Consolas") });
    else if (fam == 2) f.setFamilies({ QStringLiteral("Arial Black"), QStringLiteral("IBM Plex Sans"), QStringLiteral("Arial") });
    else f.setFamilies({ QStringLiteral("IBM Plex Sans"), QStringLiteral("Arial") });
    f.setPixelSize(qMax(1, int(std::lround(px))));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    return f;
}
// texto com o meio da letra em y (como o textBaseline 'middle' do canvas); align: -1 esquerda, 0 centro, 1 direita
void text(QPainter& p, const QString& s, double x, double y, const QColor& col, const QFont& f, int align = 0)
{
    p.setFont(f);
    const QFontMetricsF fm(f);
    const double w = fm.horizontalAdvance(s);
    const double bx = align < 0 ? x : align == 0 ? x - w / 2 : x - w;
    p.setPen(col);
    p.drawText(QPointF(bx, y + (fm.ascent() - fm.descent()) / 2), s);
    p.setPen(Qt::NoPen);
}

// ============================================================ céu, mesas, chão (as duas cenas)

struct Twinkle { double x, y, a, f, ph; QColor c; };

struct SkyOpt {
    double stop3 = 0.8;        // onde o degradê chega no tom do horizonte
    double bandH = 90, bandA = 0.32;
    bool cityGlow = true;
    double ax, ay, bx, by;     // a faixa da Via Láctea
    int blobs = 150, stars = 900, bandStars = 320, twinkles = 26;
    double mx, my, mr, cut;    // lua: centro, raio e de que lado a sombra come (−1 esquerda, +1 direita)
};

// pinta o céu inteiro no cache e devolve a cor da névoa do horizonte
QColor paintSky(QPainter& x, const Cam& cam, const Pal& pal, const Ctx& c, const SkyOpt& o, std::vector<Twinkle>& tw)
{
    const double HY = cam.HY;
    const QColor top = mix(pal.app, hex("#02040b"), 0.55), mid = mix(pal.app, hex("#0a1030"), 0.45), hz = mix(pal.app, hex("#2c2648"), 0.4), warm = mix(pal.app, hex("#563846"), 0.42);
    QLinearGradient g(0, 0, 0, HY);
    g.setColorAt(0, top);
    g.setColorAt(0.5, mid);
    g.setColorAt(o.stop3, hz);
    g.setColorAt(1, warm);
    x.fillRect(QRectF(0, 0, c.W, HY + 2), g);
    x.fillRect(QRectF(0, HY - o.bandH, c.W, o.bandH + 2), vgrad(HY - o.bandH, HY + 2, alpha(hex("#7a5a7a"), 0), alpha(hex("#8a5a68"), o.bandA)));
    if (o.cityGlow) {
        x.save();
        x.translate(cam.VX + 40, HY);
        x.scale(1, 0.22);
        glowAt(x, 0, 0, 380, hex("#c87a5a"), 0.16);
        x.restore();
    }
    // Via Láctea: nuvem de manchas claras com a poeira escura no meio
    const QPixmap mw = softDot(hex("#b8c4ff"), 0.2), dust = softDot(mix(pal.app, hex("#04060e"), 0.5), 0.3);
    const QRectF src(0, 0, mw.width(), mw.height());
    for (int i = 0; i < o.blobs; ++i) {
        const double t = rnd(), s = R(40, 120), off = R(-40, 40);
        x.setOpacity(0.03 + 0.03 * rnd());
        x.drawPixmap(QRectF(o.ax + (o.bx - o.ax) * t - s / 2 + off * 0.6, o.ay + (o.by - o.ay) * t - s / 2 + off, s, s), mw, src);
    }
    for (int i = 0; i < 45; ++i) {
        const double t = rnd(), s = R(14, 40);
        x.setOpacity(0.18);
        x.drawPixmap(QRectF(o.ax + (o.bx - o.ax) * t - s / 2 + R(-6, 6), o.ay + (o.by - o.ay) * t - s / 2 + R(-8, 8), s, s * 0.6), dust, src);
    }
    x.setOpacity(1);
    // estrelas: muitas, coloridas, somem perto do horizonte
    static const char* COLS[] = { "#ffffff", "#ffffff", "#d4e2ff", "#fff0d8", "#ffd8bc", "#c8d8ff" };
    auto putStar = [&](double sx, double sy, bool big) {
        const double fade = 1 - smooth(HY - 150, HY - 8, sy), s = big ? R(1.5, 2.2) : R(0.6, 1.25);
        x.fillRect(QRectF(sx, sy, s, s), alpha(hex(pick(COLS)), R(0.12, big ? 0.95 : 0.7) * fade));
    };
    for (int i = 0; i < o.stars; ++i) putStar(R(0, c.W), std::pow(rnd(), 1.1) * HY, rnd() < 0.05);
    for (int i = 0; i < o.bandStars; ++i) {
        const double t = rnd(), off = R(-30, 30);
        putStar(o.ax + (o.bx - o.ax) * t + off * 0.6, o.ay + (o.by - o.ay) * t + off, false);
    }
    tw.clear();
    for (int i = 0; i < o.twinkles; ++i) tw.push_back({ R(0, c.W), R(20, HY - 120), R(0.5, 0.95), R(1.5, 4), R(0, 6.28), hex(pick(COLS)) });
    // lua crescente, com a luz cinzenta da Terra no resto do disco
    glowAt(x, o.mx, o.my, 90, hex("#c8d4ff"), 0.1);
    QPixmap m = layer(c, 40, 40);
    {
        QPainter mp(&m);
        mp.setRenderHint(QPainter::Antialiasing, true);
        mp.setPen(Qt::NoPen);
        mp.setBrush(hex("#f4f0e2"));
        mp.drawEllipse(QPointF(20, 20), o.mr, o.mr);
        mp.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        mp.setBrush(Qt::black);
        mp.drawEllipse(QPointF(20 + 6 * o.cut, 17), o.mr * 0.98, o.mr * 0.98);
        mp.setCompositionMode(QPainter::CompositionMode_DestinationOver);
        mp.setBrush(alpha(hex("#8a94b4"), 0.27));
        mp.drawEllipse(QPointF(20, 20), o.mr, o.mr);
    }
    blit(x, m, o.mx - 20, o.my - 20, 40, 40);
    return mix(hz, hex("#6a4a64"), 0.35);
}

// árvore de Josué: tronco que bifurca, cada ponta com um tufo espetado
void joshuaRec(QPainter& x, double s, double x0, double y0, double ang, double len, double w, int d, const QColor& col)
{
    const double x1 = x0 + std::cos(ang) * len, y1 = y0 + std::sin(ang) * len;
    x.setPen(pen(col, w));
    x.setBrush(Qt::NoBrush);
    QPainterPath b;
    b.moveTo(x0, y0);
    b.quadTo(x0 + std::cos(ang) * len * 0.5 + R(-2, 2), y0 + std::sin(ang) * len * 0.5, x1, y1);
    x.drawPath(b);
    if (d <= 0) {
        const double r = s * 0.42;
        x.setPen(pen(col, qMax(0.6, r * 0.12)));
        QVector<QLineF> ls;
        for (int k = 0; k < 16; ++k) {
            const double a = -M_PI / 2 + (k / 15.0 - 0.5) * 3.6 + R(-0.1, 0.1), rr = r * R(0.7, 1.1);
            ls << QLineF(x1, y1, x1 + std::cos(a) * rr, y1 + std::sin(a) * rr * 0.9);
        }
        x.drawLines(ls);
        return;
    }
    static const int ONE_OR_TWO[] = { 1, 2, 2 };
    const int n = d > 1 ? 2 : pick(ONE_OR_TWO);
    for (int k = 0; k < n; ++k)
        joshuaRec(x, s, x1, y1, ang + (n == 1 ? R(-0.4, 0.4) : (k ? 1 : -1) * R(0.35, 0.8)) - 0.05, len * R(0.6, 0.85), w * 0.72, d - 1, col);
}
void joshua(QPainter& x, const Cam& cam, double X, double Z, double h, const QColor& col, const QColor& rim = QColor())
{
    const double s = cam.F / Z;
    const QPointF b = cam.P(X, 0, Z);
    joshuaRec(x, s, b.x(), b.y(), -M_PI / 2 + R(-0.1, 0.1), h * 0.45 * s, qMax(1.0, 0.32 * s), 2 + (h > 6 ? 1 : 0), col);
    if (rim.isValid()) {
        x.setPen(pen(rim, qMax(0.5, 0.06 * s)));
        x.drawLine(QPointF(b.x() + 0.14 * s, b.y()), QPointF(b.x() + 0.12 * s, b.y() - h * 0.4 * s));
    }
    x.setPen(Qt::NoPen);
}

// chão do deserto: lavado perto do horizonte, escuro perto; cascalho, moitas de creosoto e pedras
// sorteados na tela (servem em qualquer largura), fora do que onLot diz ser estrada ou posto
void paintGround(QPainter& x, const Cam& cam, const Pal& pal, const Ctx& c, const QColor& hz, const std::function<bool(double, double)>& onLot, int bushes, int rocks)
{
    const double HY = cam.HY, H = c.H, F = cam.F, CH = cam.CH;
    x.fillRect(QRectF(0, HY, c.W, H - HY), vgrad(HY, H, mix(hz, hex("#1a1622"), 0.4), mix(pal.app, hex("#07070b"), 0.6)));
    const QColor dk = alpha(Qt::black, 0.3), lt = alpha(hex("#9a94a8"), 0.08);
    for (int i = 0; i < 5000; ++i) {
        const double sy = HY + 1 + std::pow(rnd(), 1.4) * (H - HY), Z = F * CH / (sy - HY), sx = R(0, c.W);
        if (onLot(cam.sxX(sx, Z), Z)) continue;
        const double s = qMin(2.2, 0.06 * F / Z);
        x.fillRect(QRectF(sx, sy, s, s * 0.6), rnd() < 0.5 ? dk : lt);
    }
    const QColor bush = mix(pal.app, hex("#0a0c0a"), 0.55);
    for (int i = 0; i < bushes; ++i) {
        const double sy = HY + 2 + std::pow(rnd(), 1.8) * (H - HY), Z = F * CH / (sy - HY), sx = R(-20, c.W + 20);
        if (onLot(cam.sxX(sx, Z), Z)) continue;
        const double h = qMin(18.0, R(0.35, 0.8) * F / Z);
        if (h < 1.6) { x.fillRect(QRectF(sx, sy - 1, 1.5, 1), alpha(Qt::black, 0.4)); continue; }
        QVector<QLineF> ls;
        const int n = 7 + int(h / 3);
        for (int k = 0; k < n; ++k) {
            const double a = -M_PI / 2 + (double(k) / (n - 1) - 0.5) * 2.3 + R(-0.15, 0.15), l = h * R(0.6, 1);
            ls << QLineF(sx + R(-h * 0.1, h * 0.1), sy, sx + std::cos(a) * l * 1.1, sy + std::sin(a) * l);
        }
        x.setPen(pen(bush, qMax(0.6, h * 0.05)));
        x.drawLines(ls);
        x.setPen(Qt::NoPen);
        x.fillRect(QRectF(sx - h * 0.5, sy - 0.5, h, qMax(1.0, h * 0.08)), alpha(Qt::black, 0.3));
        if (h > 4) {
            x.setPen(pen(alpha(hex("#8a98c0"), 0.16), 0.6));
            x.drawLine(QPointF(sx + h * 0.2, sy - h * 0.7), QPointF(sx + h * 0.45, sy - h * 0.45));
            x.setPen(Qt::NoPen);
        }
    }
    const QColor rock = mix(pal.app, hex("#1c1a20"), 0.5), rockTop = alpha(hex("#8a90b0"), 0.2);
    for (int i = 0; i < rocks; ++i) {
        const double sy = HY + 3 + std::pow(rnd(), 1.5) * (H - HY), Z = F * CH / (sy - HY), sx = R(0, c.W);
        if (onLot(cam.sxX(sx, Z), Z)) continue;
        const double r = qMin(9.0, R(0.12, 0.3) * F / Z);
        if (r < 1) continue;
        QPolygonF q;
        q << QPointF(sx - r, sy) << QPointF(sx - r * 0.6, sy - r * 0.7) << QPointF(sx + r * 0.3, sy - r * 0.8) << QPointF(sx + r, sy);
        x.setPen(Qt::NoPen);
        x.setBrush(rock);
        x.drawPolygon(q);
        x.fillRect(QRectF(sx - r * 0.5, sy - r * 0.78, r * 0.8, qMax(0.6, r * 0.12)), rockTop);
    }
}

// tumbleweed: galhos secos enrolados, com a borda pegando a luz
QPixmap tumbleSprite(bool litTopRight)
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter t(&pm);
    t.setRenderHint(QPainter::Antialiasing, true);
    static const char* OUT[] = { "#8a7458", "#6a5a44", "#a08a68" };
    for (int i = 0; i < 90; ++i) {
        const double a0 = R(0, 6.28), r0 = R(2, 27), a1 = a0 + R(0.6, 2.2), r1 = R(4, 29);
        const bool outer = (r0 + r1) / 2 > 18;
        t.setPen(pen(outer ? alpha(hex(pick(OUT)), 0.8) : alpha(hex("#3a3024"), 0.9), R(0.6, 1.4)));
        QPainterPath b;
        b.moveTo(32 + std::cos(a0) * r0, 32 + std::sin(a0) * r0);
        b.quadTo(32 + std::cos((a0 + a1) / 2) * R(8, 30), 32 + std::sin((a0 + a1) / 2) * R(8, 30), 32 + std::cos(a1) * r1, 32 + std::sin(a1) * r1);
        t.drawPath(b);
    }
    t.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    QLinearGradient g = litTopRight ? QLinearGradient(52, 12, 12, 52) : QLinearGradient(12, 12, 52, 52);
    if (litTopRight) {
        g.setColorAt(0, alpha(hex("#d8e8e4"), 0.35));
        g.setColorAt(0.5, alpha(Qt::black, 0));
        g.setColorAt(1, alpha(Qt::black, 0.45));
    } else {
        g.setColorAt(0, alpha(Qt::black, 0.5));
        g.setColorAt(1, alpha(hex("#d8e8e4"), 0.3));
    }
    t.fillRect(QRectF(0, 0, 64, 64), g);
    return pm;
}

// ---- veículos (as medidas são as mesmas nas duas cenas)
struct ProfPt { double d, Y; bool g = false; };
struct Part { double d0; bool box; std::vector<ProfPt> prof; };
struct Kind {
    double L, w;
    std::vector<Part> parts;
    std::vector<QPointF> win;                 // (d, Y) da janela lateral
    double pillar;                            // coluna entre as janelas (0 = sem)
    std::vector<double> wheels;
    double wr;
    std::vector<QPointF> head, tail;          // (u, Y): deslocamento lateral e altura
};
const Kind& kindOf(int k)
{
    static const Kind K[3] = {
        { 4.7, 1.82, { { 0, false, { { 0, 0.32 }, { 0, 0.74 }, { 0.12, 0.86 }, { 1.0, 0.95 }, { 1.7, 1.0, true }, { 2.45, 1.41 }, { 3.35, 1.43, true }, { 4.2, 1.03 }, { 4.7, 0.96 }, { 4.72, 0.34 } } } },
          { { 1.85, 1.02 }, { 2.5, 1.36 }, { 3.3, 1.37 }, { 4.02, 1.04 } }, 2.95, { 0.85, 3.85 }, 0.33, { { -0.62, 0.68 }, { 0.62, 0.68 } }, { { -0.7, 0.86 }, { 0.7, 0.86 } } },
        { 5.4, 1.95, { { 0, false, { { 0, 0.42 }, { 0, 0.95 }, { 0.1, 1.08 }, { 1.5, 1.14 }, { 1.85, 1.18, true }, { 2.4, 1.8 }, { 3.25, 1.82 }, { 3.3, 1.22 }, { 5.35, 1.22 }, { 5.4, 0.45 } } } },
          { { 1.95, 1.21 }, { 2.45, 1.74 }, { 3.15, 1.75 }, { 3.2, 1.23 } }, 2.75, { 0.95, 4.4 }, 0.4, { { -0.7, 0.88 }, { 0.7, 0.88 } }, { { -0.85, 0.95 }, { 0.85, 0.95 } } },
        { 21, 2.5, { { 0, false, { { 0, 0.5 }, { 0, 1.3 }, { 0.15, 1.85 }, { 1.85, 1.98 }, { 2.0, 2.05, true }, { 2.3, 2.95 }, { 3.5, 3.0 }, { 3.6, 3.55 }, { 5.4, 3.55 }, { 5.5, 1.3 }, { 6.3, 1.2 }, { 6.3, 0.6 } } },
                     { 5.8, true, { { 0, 1.2 }, { 0, 4.1 }, { 15.2, 4.1 }, { 15.2, 1.2 } } } },
          { { 2.05, 2.08 }, { 2.35, 2.85 }, { 3.22, 2.88 }, { 3.22, 2.08 } }, 0, { 1.0, 4.9, 5.9, 18.2, 19.5 }, 0.52, { { -0.9, 1.1 }, { 0.9, 1.1 } }, { { -1.05, 1.25 }, { 1.05, 1.25 } } },
    };
    return K[qBound(0, k, 2)];
}
enum { Car = 0, Pickup = 1, Truck = 2 };
int randomKind() { const double r = rnd(); return r < 0.3 ? Truck : r < 0.55 ? Pickup : Car; }
QColor randomCarColor(bool withRust)
{
    static const char* A[] = { "#5a1e1a", "#1e2a40", "#2e3034", "#c8c8c0", "#3a3a2a", "#6a5a3a", "#1a1a1c", "#2a4a5a", "#7a2a1a" };
    return hex(A[RI(withRust ? 9 : 8)]);
}
QColor randomTrailer()
{
    static const char* A[] = { "#c8ccd0", "#e8e4dc", "#8a2a20", "#2a3a5a" };
    return hex(pick(A));
}

// ============================================================ Last Exit (o solitário)
// A gente está do outro lado da estrada, olhando de frente. Lá no fundo, pequeno, o posto aceso no
// meio do nada: a cobertura de luz fria com uma luminária falhando, as mariposas, uma ilha com duas
// bombas, a lojinha com uma janela acesa e o orelhão com a sua lâmpada; a plaquinha GAS que às vezes
// apaga. Fora da luz, um carro parado no escuro, e de vez em quando a brasa de um cigarro acende lá
// dentro. Raramente um carro ou uma carreta passa (de lado pra gente); às vezes um tumbleweed.
class LastExit : public Layer {
public:
    void init(Ctx&) override
    {
        m_key.clear();
        m_veh.clear();
        m_nextVeh = R(6, 10);
        m_weed.on = false;
        m_nextWeed = R(20, 35);
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_flick = rnd() < 0.06 ? 1 : m_flick * 0.6;
        if (m_signOff > 0) m_signOff -= dt; else if (rnd() < dt * 0.04) m_signOff = R(0.3, 1.6);
        for (Moth& m : m_moths) m.a += m.f * dt;
        if (m_key.isEmpty()) return;                              // o cenário ainda não foi montado (falta a posição do posto)
        m_nextVeh -= dt;
        if (m_nextVeh <= 0) { m_veh.push_back(spawn(c)); m_nextVeh = R(25, 60); }
        for (Veh& v : m_veh) v.x += v.dir * v.v * dt;
        m_veh.erase(std::remove_if(m_veh.begin(), m_veh.end(), [](const Veh& v) { return v.dir > 0 ? v.x > v.end : v.x < v.end; }), m_veh.end());
        m_nextWeed -= dt;
        if (!m_weed.on && m_nextWeed <= 0) m_weed = { true, m_cam.sxX(-60, 18), R(17.2, 18.6), R(2.5, 4), 0, R(0.32, 0.45) };
        if (m_weed.on) {
            m_weed.X += m_weed.v * dt * (0.7 + 0.5 * std::sin(m_t * 1.3));
            m_weed.b += dt;
            if (m_weed.X > m_cam.sxX(c.W + 60, m_weed.Z)) { m_weed.on = false; m_nextWeed = R(45, 100); }
        }
        m_nextStar -= dt;
        if (!m_star.on && m_nextStar <= 0) m_star = { true, R(m_cam.VX - 40, m_cam.VX + 280), R(30, 260), R(0.35, 0.7), 0 };
        if (m_star.on) { m_star.t += dt; if (m_star.t > 0.7) { m_star.on = false; m_nextStar = R(18, 45); } }
        // a brasa do cigarro: acende devagar (a tragada), apaga devagar, some por um tempo
        m_emberNext -= dt;
        if (m_emberNext <= 0) { m_emberT += dt; if (m_emberT > 3.2) { m_emberT = 0; m_emberNext = R(9, 26); } }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const QString key = QString::number(zx(c, 120), 'f', 1) + QLatin1Char('|') + QString::number(c.W, 'f', 0) + QLatin1Char('|') + QString::number(c.px, 'f', 2) + pal.app.name();
        if (key != m_key) {
            Seeded sd(0x1A57E7u);
            build(pal, c);
            m_key = key;
        }
        const Cam& cam = m_cam;
        const double HY = cam.HY, F = cam.F;
        p.setPen(Qt::NoPen);
        blit(p, m_back, 0, 0, c.W, c.H);
        for (const Twinkle& s : m_tw)
            p.fillRect(QRectF(s.x, s.y, 1.6, 1.6), alpha(s.c, s.a * (0.55 + 0.45 * std::sin(m_t * s.f + s.ph)) * (1 - smooth(HY - 150, HY - 8, s.y))));
        if (m_star.on) {
            const double k = m_star.t / 0.7;
            p.setPen(pen(alpha(Qt::white, m_star.a * std::sin(k * M_PI)), 1.1));
            p.drawLine(QPointF(m_star.x + k * 90, m_star.y + k * 40), QPointF(m_star.x + k * 90 - 63, m_star.y + k * 40 - 28));
            p.setPen(Qt::NoPen);
        }
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        glowAt(p, m_ranch.x(), m_ranch.y(), 6, hex("#ffc070"), 0.5);
        p.fillRect(QRectF(m_ranch.x() - 0.6, m_ranch.y() - 0.6, 1.2, 1.2), alpha(hex("#ffd8a0"), 0.9));
        // a brasa no carro parado
        if (m_emberNext <= 0) {
            const double k = m_emberT < 1 ? smooth(0, 1, m_emberT) : 1 - smooth(1, 3.2, m_emberT);
            const QPointF e = cam.P(wx(m_parked, 2.55), 1.12, m_parked.z - 0.4);
            glowAt(p, e.x(), e.y(), 6, hex("#ff6a20"), 0.5 * k);
            p.fillRect(QRectF(e.x() - 0.7, e.y() - 0.7, 1.4, 1.4), alpha(hex("#ffb060"), k));
        }
        // luminárias (uma falhando) e a névoa acesa embaixo da cobertura
        const double lit = 1 - 0.6 * m_flick;
        for (int i = 0; i < int(m_lights.size()); ++i) {
            const double on = i == 5 ? 1 - m_flick : 1, lx = m_lights[i].x(), lz = m_lights[i].y();
            cam.face(p, fY(C.y0 - 0.002, lx - 0.24, lx + 0.24, lz - 0.5, lz + 0.5), alpha(hex("#f4fffa"), 0.95 * on));
            const QPointF g = cam.P(lx, C.y0, lz);
            glowAt(p, g.x(), g.y() + 1, 22 * 24 / lz, hex("#e8fff4"), 0.3 * on);
        }
        {
            QPolygonF q;
            if (cam.poly({ { C.x0, C.y0, C.z0 }, { C.x1, C.y0, C.z0 }, { C.x1 + 0.8, 0, C.z0 - 0.5 }, { C.x0 - 0.8, 0, C.z0 - 0.5 } }, q)) {
                const double ya = cam.P(0, C.y0, C.z0).y(), yb = cam.P(0, 0, C.z0).y();
                p.setBrush(vgrad(ya, yb, alpha(hex("#e8fff4"), 0.09 * (0.6 + 0.4 * lit)), alpha(hex("#e8fff4"), 0.015)));
                p.drawPolygon(q);
            }
        }
        // a plaquinha GAS e o preço (às vezes apaga)
        const double so = m_signOff > 0 && rnd() < 0.7 ? 0.12 : 1;
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        {
            const double s = F / SIGN.z;
            const QPointF a = cam.P(SIGN.x - 1.1, 7.6, SIGN.z), b = cam.P(SIGN.x + 1.1, 5.9, SIGN.z);
            const double mid = a.y() + (b.y() - a.y()) * 0.52, cx = (a.x() + b.x()) / 2;
            p.fillRect(QRectF(a.x(), a.y(), b.x() - a.x(), mid - a.y()), alpha(mix(pal.app, hex("#f4efe2"), 0.95), 0.12 + 0.85 * so));
            p.fillRect(QRectF(a.x(), mid, b.x() - a.x(), b.y() - mid), hex("#0a0a0c"));
            text(p, QStringLiteral("GAS"), cx, (a.y() + mid) / 2 + 0.5, alpha(hex("#c8302a"), 0.25 + 0.75 * so), fnt(0.62 * s, 900, 2));
            text(p, QStringLiteral("4.29"), cx - 2, (mid + b.y()) / 2 + 0.5, alpha(hex("#ff4a2a"), 0.1 + 0.85 * so), fnt(0.5 * s, 700, 1));
            text(p, QStringLiteral("9"), cx + 0.75 * s, (mid + b.y()) / 2 - 0.12 * s, alpha(hex("#ff4a2a"), 0.1 + 0.85 * so), fnt(0.24 * s, 700, 1));
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            glowAt(p, cx, (a.y() + b.y()) / 2, 3.2 * s, hex("#ffe0d0"), 0.16 * so);
        }
        glowAt(p, m_booth.x(), m_booth.y(), 14, hex("#e8f4ff"), 0.45);
        p.fillRect(QRectF(m_booth.x() - 1, m_booth.y() - 0.5, 2, 1), Qt::white);
        { const QPointF a = cam.P(ST.x0 + 5.75, 1.35, ST.z0 - 0.8); glowAt(p, a.x(), a.y(), 12, hex("#ff4a3a"), 0.25); }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // mariposas
        const QColor mc = alpha(hex("#fff8e8"), 0.8);
        for (const Moth& m : m_moths)
            p.fillRect(QRectF(m.cx + std::cos(m.a) * m.r + std::sin(m.a * 2.3 + m.ph) * 2, m.cy + std::sin(m.a * 1.3 + m.ph) * m.r * 0.5, 1.2, 1.2), mc);
        // tumbleweed rolando pela beira do pátio
        if (m_weed.on) {
            const double hop = std::abs(std::sin(m_weed.b * 2.6)) * 0.3, s = F / m_weed.Z, r = m_weed.r * s;
            const QPointF sp = cam.P(m_weed.X, m_weed.r + hop, m_weed.Z), gp = cam.P(m_weed.X, 0, m_weed.Z);
            p.setBrush(alpha(Qt::black, 0.3));
            p.drawEllipse(gp, r * 0.9, r * 0.2);
            p.save();
            p.translate(sp);
            p.rotate(qRadiansToDegrees(m_weed.X / m_weed.r));
            p.setOpacity(0.8);
            blit(p, m_tumble, -r, -r, r * 2, r * 2);
            p.restore();
        }
        // veículos (a faixa de lá primeiro): facho e luzes de lá, o corpo, e as luzes do lado de cá por cima
        std::sort(m_veh.begin(), m_veh.end(), [](const Veh& a, const Veh& b) { return a.z > b.z; });
        for (const Veh& v : m_veh) {
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            vehLights(p, v);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            drawVeh(p, v, pal);
            const Kind& K = kindOf(v.kind);
            const double s = F / v.z, zz = v.z - K.w / 2 + 0.25;
            const bool frontVis = v.dir * (0 - v.x) > 0, rearVis = -v.dir * (0 - wx(v, K.L)) > 0;
            const QPointF h = cam.P(v.x, K.head[0].y(), zz), t = cam.P(wx(v, K.L), K.tail[0].y(), zz);
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            glowAt(p, h.x(), h.y(), (frontVis ? 1.4 : 0.6) * s, hex("#fff4dc"), frontVis ? 0.55 : 0.3);
            glowAt(p, t.x(), t.y(), 0.5 * s, hex("#ff2010"), rearVis ? 0.55 : 0.35);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.fillRect(QRectF(h.x() - 1, h.y() - 1, 2, 2), Qt::white);
            p.fillRect(QRectF(t.x() - 1, t.y() - 1, 2, 2), hex("#ff6050"));
        }
    }

private:
    struct Box { double x0, x1, z0, z1, y0, y1; };
    struct Moth { double cx, cy, r, a, f, ph; };
    struct Veh { int dir; int kind; double z, v, x, end; QColor col, trailer; };
    struct Weed { bool on = false; double X = 0, Z = 18, v = 3, b = 0, r = 0.4; };
    struct Star { bool on = false; double x = 0, y = 0, a = 0, t = 0; };

    static double wx(const Veh& v, double d) { return v.x - v.dir * d; }
    static double lane(int dir) { return dir > 0 ? 10.7 : 14.3; }

    Veh spawn(const Ctx& c) const
    {
        Veh v;
        v.dir = rnd() < 0.5 ? 1 : -1;
        v.kind = randomKind();
        v.z = lane(v.dir);
        v.v = R(19, 25) * (v.kind == Truck ? 0.85 : 1);
        const double x0 = m_cam.sxX(-120, v.z), x1 = m_cam.sxX(c.W + 120, v.z), L = kindOf(v.kind).L;
        v.x = v.dir > 0 ? x0 - 40 : x1 + 40;
        v.end = v.dir > 0 ? x1 + L + 40 : x0 - L - 40;
        v.col = randomCarColor(false);
        v.trailer = randomTrailer();
        return v;
    }

    void place(const Ctx& c)
    {
        m_cam.VX = zx(c, 120);
        const double cx = m_cam.sxX(zx(c, 250), 24);      // o centro do posto na faixa de mesa à vista
        C = { cx - 4.3, cx + 4.3, 22, 29, 4.5, 5.2 };
        ISL = { cx - 3.2, cx + 3.2, 25.1, 25.9, 0, 0.18 };
        PUMPS[0] = cx - 1.45; PUMPS[1] = cx + 1.45;
        COLS[0] = cx - 2.7; COLS[1] = cx + 2.7;
        ST = { cx - 6.8, cx + 1.6, 35, 42, 0, 3.3 };
        SIGN = { cx - 6.4, 19 };
        m_lights.clear();
        for (double lz : { 23.6, 27.2 })
            for (int k : { -3, -1, 1, 3 }) m_lights.push_back(QPointF(cx + k * 1.05, lz));
    }

    void build(const Pal& pal, const Ctx& c)
    {
        place(c);
        const Cam& cam = m_cam;
        const double HY = cam.HY, F = cam.F;
        m_back = layer(c, c.W, c.H);
        QPainter x(&m_back);
        x.setRenderHint(QPainter::Antialiasing, true);
        SkyOpt so;
        so.stop3 = 0.82; so.bandH = 80; so.bandA = 0.26; so.cityGlow = false;
        so.ax = cam.VX - 200; so.ay = HY - 30; so.bx = cam.VX + 600; so.by = -100;
        so.blobs = 160; so.stars = 1000; so.bandStars = 340; so.twinkles = 28;
        so.mx = cam.VX + 70; so.my = 120; so.mr = 9; so.cut = 1;
        const QColor hz = paintSky(x, cam, pal, c, so, m_tw);
        mesas(x, pal, c, hz);
        m_ranch = QPointF(c.W - 210, HY - 1);            // uma luz de fazenda muito longe: a única outra coisa acesa no mundo
        poles(x, pal, c);
        const double RZ0 = 9, RZ1 = 16;
        paintGround(x, cam, pal, c, hz, [&](double X, double Z) {
            return (Z > RZ0 - 1.2 && Z < RZ1 + 1.2) || (X > SIGN.x - 2.5 && X < C.x1 + 4 && Z > RZ1 + 1 && Z < ST.z1 + 1);
        }, 300, 120);
        joshua(x, cam, cam.sxX(cam.VX - 50, 60), 60, 6, mix(pal.app, hex("#0a0a10"), 0.6));
        const QColor jt = mix(pal.app, hex("#08080c"), 0.6), jr = alpha(hex("#8a98c8"), 0.22);
        joshua(x, cam, cam.sxX(c.W - 70, 24), 24, 7.5, jt, jr);
        joshua(x, cam, cam.sxX(c.W - 300, 50), 50, 6, jt, jr);
        apron(x, pal);
        store(x, pal);
        // o carro parado fora da luz, de faróis apagados
        m_parked = { 1, Car, 31, 0, cam.sxX(zx(c, 396), 31), 0, hex("#16161a"), QColor() };
        drawVeh(x, m_parked, pal);
        // a luz no chão: cada luminária, a cobertura toda, a plaquinha
        x.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const QPointF& l : m_lights) pool(x, l.x(), l.y(), 5, hex("#cfe8e0"), 0.1);
        pool(x, (C.x0 + C.x1) / 2, 25.5, 12, hex("#bfe0d8"), 0.07);
        pool(x, SIGN.x, SIGN.z, 5, hex("#ffd8c8"), 0.06);
        x.setCompositionMode(QPainter::CompositionMode_SourceOver);
        cam.face(x, fY(0.012, ISL.x0, ISL.x1, ISL.z1, ISL.z1 + 0.6), alpha(Qt::black, 0.15));
        canopy(x, pal);
        signPole(x, pal);
        road(x, pal, c);
        x.end();
        m_tumble = tumbleSprite(false);
        m_moths.clear();
        for (int i = 0; i < 16; ++i) {
            const QPointF l = m_lights[RI(int(m_lights.size()))], q = cam.P(l.x(), C.y0 - 0.4, l.y());
            m_moths.push_back({ q.x(), q.y(), R(2, 9), R(0, 6.28), R(2, 5) * (rnd() < 0.5 ? 1 : -1), R(0, 6.28) });
        }
        const QPointF sc = cam.P(SIGN.x, 6.75, SIGN.z);
        for (int i = 0; i < 10; ++i) m_moths.push_back({ sc.x() + R(-10, 10), sc.y() + R(-8, 8), R(5, 18), R(0, 6.28), R(1.5, 4) * (rnd() < 0.5 ? 1 : -1), R(0, 6.28) });
        (void)F;
    }

    // um círculo de luz no chão em perspectiva, com degradê do centro pra borda
    void pool(QPainter& x, double X0, double Z0, double Rr, const QColor& col, double a) const
    {
        Pts pts;
        for (int k = 0; k < 28; ++k) { const double t = k / 28.0 * 6.28; pts.push_back({ X0 + std::cos(t) * Rr, 0.01, Z0 + std::sin(t) * Rr }); }
        QPolygonF q;
        if (!m_cam.poly(pts, q)) return;
        const QPointF ctr = m_cam.P(X0, 0, Z0), e = m_cam.P(X0 + Rr, 0, Z0);
        QRadialGradient g(ctr, qMax(1.0, std::abs(e.x() - ctr.x())));
        g.setColorAt(0, alpha(col, a));
        g.setColorAt(0.5, alpha(col, a * 0.45));
        g.setColorAt(1, alpha(col, 0));
        x.setPen(Qt::NoPen);
        x.setBrush(g);
        x.drawPolygon(q);
    }

    // mesas bem longe e baixas: o horizonte quase reto
    void mesas(QPainter& x, const Pal& pal, const Ctx& c, const QColor& hz) const
    {
        const double HY = m_cam.HY;
        struct B { double cx, w, h, tw, cw, tk; Profile prof; };
        auto butte = [](double cx, double w, double h) { B b; b.prof = profile(5, 0.6); b.cx = cx; b.w = w; b.h = h; b.tw = w * R(0.2, 0.3); b.cw = R(4, 9); b.tk = R(0.25, 0.38); return b; };
        auto hAt = [](const B& b, double xx) {
            const double d = qMin(xx - b.cx + b.w / 2, b.cx + b.w / 2 - xx);
            if (d <= 0) return 0.0;
            if (d < b.tw) return b.h * b.tk * std::pow(d / b.tw, 1.7);
            if (d < b.tw + b.cw) return b.h * (b.tk + (1 - b.tk) * std::pow((d - b.tw) / b.cw, 0.6));
            return b.h + b.prof.at((xx - b.cx + b.w / 2) / b.w) * 1.5;
        };
        auto plane = [&](const std::vector<B>& list, const QColor& col, double rimA) {
            auto f = [&](double xx) { double h = 0; for (const B& b : list) h = qMax(h, hAt(b, xx)); return HY + 1 - h; };
            QPolygonF q;
            q << QPointF(-10, HY + 4);
            for (double xx = -10; xx <= c.W + 10; xx += 1.5) q << QPointF(xx, f(xx));
            q << QPointF(c.W + 10, HY + 4);
            x.setPen(Qt::NoPen);
            x.setBrush(col);
            x.drawPolygon(q);
            const QColor rc = alpha(hex("#b0a0c8"), rimA);
            for (double xx = 0; xx < c.W; xx += 2) {
                const double y = f(xx), y3 = f(xx + 3);
                if (y < HY - 2 && y3 > y + 1.5) x.fillRect(QRectF(xx, y, 2, qMin(8.0, y3 - y)), rc);
            }
        };
        std::vector<B> far;
        for (double cx = -60; cx < c.W + 120; cx += R(90, 220))
            if (rnd() < 0.7) far.push_back(butte(cx, R(60, 200), R(4, 14)));
        plane(far, mix(hz, hex("#1a1226"), 0.45), 0.08);
        x.fillRect(QRectF(0, HY - 20, c.W, 22), vgrad(HY - 20, HY + 2, alpha(hz, 0), alpha(hz, 0.3)));
        std::vector<B> mid = { butte(m_cam.VX - 30, 120, 22), butte(c.W - 330, 150, 30), butte(c.W - 120, 60, 18) };
        plane(mid, mix(pal.app, hex("#1e1018"), 0.5), 0.3);
        x.fillRect(QRectF(0, HY - 40, c.W, 42), vgrad(HY - 40, HY + 2, alpha(hz, 0), alpha(hz, 0.18)));
    }

    // a linha de postes atravessando o deserto atrás do posto, fios caindo entre um e outro
    void poles(QPainter& x, const Pal& pal, const Ctx& c) const
    {
        const double Z = 70, s = m_cam.F / Z;
        const QColor col = mix(pal.app, hex("#06060a"), 0.6);
        std::vector<std::pair<QPointF, QPointF>> tops;
        for (double X = m_cam.sxX(-40, Z); X < m_cam.sxX(c.W + 60, Z); X += 45) {
            const QPointF b = m_cam.P(X, 0, Z), t = m_cam.P(X, 9.5, Z);
            x.fillRect(QRectF(b.x() - 0.7, t.y(), 1.4, b.y() - t.y()), col);
            x.fillRect(QRectF(b.x() - 1.1 * s, t.y() + 0.3 * s, 2.2 * s, 1), col);
            tops.push_back({ m_cam.P(X - 0.9, 9.1, Z), m_cam.P(X + 0.9, 9.1, Z) });
        }
        x.setPen(pen(alpha(mix(pal.app, hex("#2a2e3e"), 0.4), 0.8), 0.6));
        x.setBrush(Qt::NoBrush);
        for (int k = 0; k < 2; ++k) {
            QPainterPath w;
            for (size_t i = 0; i + 1 < tops.size(); ++i) {
                const QPointF a = k ? tops[i].second : tops[i].first, b = k ? tops[i + 1].second : tops[i + 1].first;
                w.moveTo(a);
                w.quadTo((a.x() + b.x()) / 2, a.y() + 7, b.x(), b.y());
            }
            x.drawPath(w);
        }
        x.setPen(Qt::NoPen);
    }

    void road(QPainter& x, const Pal& pal, const Ctx& c) const
    {
        const Cam& cam = m_cam;
        const double XL = -3000, XR = 3000, RZ0 = 9, RZ1 = 16;
        const QColor gravel = mix(pal.app, hex("#1a1716"), 0.5);
        cam.face(x, fY(0, XL, XR, RZ0 - 1.4, RZ0), gravel);
        cam.face(x, fY(0, XL, XR, RZ1, RZ1 + 1.4), gravel);
        cam.face(x, fY(0, XL, XR, RZ0, RZ1), vgrad(cam.P(0, 0, RZ1).y(), cam.P(0, 0, RZ0).y(), mix(pal.app, hex("#22222c"), 0.45), mix(pal.app, hex("#121216"), 0.4)));
        for (double cz : { 9.9, 11.5, 13.5, 15.1 }) cam.face(x, fY(0, XL, XR, cz - 0.3, cz + 0.3), alpha(hex("#9aa0b8"), 0.035));
        // trincas e remendos
        for (int i = 0; i < 40; ++i) {
            const double Z = R(RZ0 + 0.3, RZ1 - 0.3);
            double px = cam.sxX(R(0, c.W), Z), pz = Z;
            QPainterPath t;
            t.moveTo(cam.P(px, 0, pz));
            for (int k = 0; k < 5; ++k) {
                px += R(0.3, 0.9) * (rnd() < 0.5 ? 1 : -1);
                pz = qBound(RZ0 + 0.1, pz + R(-0.25, 0.25), RZ1 - 0.1);
                t.lineTo(cam.P(px, 0, pz));
            }
            x.setPen(pen(alpha(Qt::black, 0.3), qMax(0.35, qMin(1.2, 0.02 * cam.F / Z))));
            x.setBrush(Qt::NoBrush);
            x.drawPath(t);
        }
        for (int i = 0; i < 10; ++i) {
            const double Z = R(RZ0 + 0.5, RZ1 - 2), X = cam.sxX(R(0, c.W), Z);
            cam.face(x, fY(0.001, X, X + R(1, 3), Z, Z + R(0.6, 1.4)), alpha(Qt::black, 0.15));
        }
        for (double ez : { RZ0 + 0.25, RZ1 - 0.35 }) cam.face(x, fY(0.002, XL, XR, ez, ez + 0.12), alpha(hex("#e8e8f0"), 0.26));
        const double z0 = 12.4;
        for (double X = std::floor(cam.sxX(-40, z0) / 12) * 12; X < cam.sxX(c.W + 40, z0); X += 12) cam.face(x, fY(0.002, X, X + 3, z0, z0 + 0.14), alpha(hex("#e0b84a"), 0.55));
    }

    // pátio de concreto gasto (a entrada e a saída cortam o acostamento)
    void apron(QPainter& x, const Pal& pal) const
    {
        const Cam& cam = m_cam;
        const double RZ1 = 16, x0 = SIGN.x - 1.5, x1 = C.x1 + 3;
        const QColor conc = mix(pal.app, hex("#2a2c32"), 0.42);
        cam.face(x, fY(0, x0, x1, RZ1 + 1.4, ST.z0), conc);
        cam.face(x, fY(0.001, x0 + 1, x0 + 4, RZ1, RZ1 + 1.4), conc);
        cam.face(x, fY(0.001, x1 - 4, x1 - 1, RZ1, RZ1 + 1.4), conc);
        QVector<QLineF> j;
        for (double X = x0; X <= x1; X += 4) j << QLineF(cam.P(X, 0, RZ1 + 1.4), cam.P(X, 0, ST.z0));
        for (double Z = RZ1 + 1.4; Z <= ST.z0; Z += 4) j << QLineF(cam.P(x0, 0, Z), cam.P(x1, 0, Z));
        x.setPen(pen(alpha(Qt::black, 0.2), 0.7));
        x.drawLines(j);
        x.setPen(Qt::NoPen);
        for (int i = 0; i < 12; ++i) {
            const double X = R(x0, x1), Z = R(RZ1 + 2, ST.z0 - 1);
            cam.face(x, fY(0.002, X, X + R(0.6, 1.6), Z, Z + R(0.4, 1)), alpha(Qt::black, 0.12));
        }
        // capim nascendo nas rachaduras da borda
        QVector<QLineF> g;
        for (int i = 0; i < 30; ++i) {
            const QPointF s = cam.P(R(x0, x1), 0, RZ1 + 1.4 + R(0, 0.4));
            for (int k = 0; k < 3; ++k) g << QLineF(s, QPointF(s.x() + R(-2, 2), s.y() - R(1.5, 3.5)));
        }
        x.setPen(pen(alpha(mix(pal.app, hex("#0c0e0a"), 0.5), 0.9), 0.6));
        x.drawLines(g);
        x.setPen(Qt::NoPen);
    }

    void store(QPainter& x, const Pal& pal)
    {
        const Cam& cam = m_cam;
        const double z0 = ST.z0, s = cam.F / z0, X0 = ST.x0, X1 = ST.x1, Y1 = ST.y1;
        const QColor wall = mix(pal.app, hex("#5a4e44"), 0.45);
        cam.face(x, fZ(z0, X0, X1, 0, Y1), vgrad(cam.P(0, Y1, z0).y(), cam.P(0, 0, z0).y(), mix(wall, hex("#9ab0b0"), 0.1), mix(wall, Qt::black, 0.3)));
        cam.face(x, fX(X0, 0, Y1, z0, ST.z1), mix(pal.app, hex("#2a2420"), 0.5));
        cam.face(x, fZ(z0 - 0.01, X0, X1, Y1 - 0.25, Y1), mix(wall, Qt::white, 0.1));
        cam.face(x, fZ(z0 - 0.01, X0, X1, 0, 0.25), alpha(Qt::black, 0.3));
        for (int i = 0; i < 10; ++i) { const double a = R(X0, X1 - 1), b = R(0.3, Y1 - 0.6); cam.face(x, fZ(z0 - 0.005, a, a + R(0.4, 1.4), b, b + R(0.2, 0.6)), alpha(Qt::black, 0.08)); }
        // a única janela acesa (com persiana), a porta de vidro e a outra janela escura
        const double wxx = X0 + 1.2, dx = X0 + 4.0;
        cam.face(x, fZ(z0 - 0.01, wxx - 0.08, wxx + 2.08, 0.92, 2.28), mix(wall, Qt::black, 0.4));
        cam.face(x, fZ(z0 - 0.02, wxx, wxx + 2, 1.0, 2.2), vgrad(cam.P(0, 2.2, z0).y(), cam.P(0, 1.0, z0).y(), hex("#ffe6b0"), hex("#e8a050")));
        for (double yy = 1.08; yy < 2.2; yy += 0.12) cam.face(x, fZ(z0 - 0.03, wxx, wxx + 2, yy, yy + 0.04), alpha(hex("#7a4a20"), 0.45));
        cam.face(x, fZ(z0 - 0.01, dx - 0.08, dx + 1.08, 0, 2.18), mix(wall, Qt::black, 0.45));
        cam.face(x, fZ(z0 - 0.02, dx, dx + 1, 0, 2.1), vgrad(cam.P(0, 2.1, z0).y(), cam.P(0, 0, z0).y(), hex("#fff0c8"), hex("#d89048")));
        cam.face(x, fZ(z0 - 0.03, dx, dx + 1, 1.0, 1.06), alpha(hex("#c8c8c8"), 0.7));
        cam.face(x, fZ(z0 - 0.01, X0 + 5.7, X1 - 0.4, 0.9, 2.2), mix(pal.app, hex("#14161c"), 0.5));
        // placa velha em cima da porta, apagada
        cam.face(x, fZ(z0 - 0.04, X0 + 0.6, X0 + 6.4, 2.55, 3.1), mix(pal.app, hex("#3a2a24"), 0.5));
        { const QPointF a = cam.P(X0 + 3.5, 2.83, z0); text(x, QStringLiteral("MARKET"), a.x(), a.y() + 0.5, alpha(mix(pal.app, hex("#c8b090"), 0.5), 0.75), fnt(0.36 * s, 800)); }
        // máquina de refrigerante acesa ao lado da porta
        cam.face(x, fZ(z0 - 0.75, X0 + 5.3, X0 + 6.2, 0, 1.85), hex("#9a201a"));
        cam.face(x, fZ(z0 - 0.76, X0 + 5.4, X0 + 6.1, 1.0, 1.7), hex("#ff5a48"));
        cam.face(x, fX(X0 + 5.3, 0, 1.85, z0 - 0.75, z0), hex("#5a1410"));
        x.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const auto& ab : { std::make_pair(wxx, wxx + 2), std::make_pair(dx, dx + 1), std::make_pair(X0 + 5.3, X0 + 6.2) })
            cam.face(x, { { ab.first, 0.01, z0 }, { ab.second, 0.01, z0 }, { ab.second + 0.8, 0.01, z0 - 3.5 }, { ab.first - 0.8, 0.01, z0 - 3.5 } },
                     vgrad(cam.P(0, 0, z0).y(), cam.P(0, 0, z0 - 3.5).y(), alpha(hex("#ffb060"), 0.22), alpha(hex("#ffb060"), 0)));
        x.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // o orelhão sozinho na ponta do pátio, com a lâmpadinha dele
        const double bx = X0 - 1.6, bz = z0 - 1.5;
        cam.face(x, fX(bx, 0, 2.1, bz, bz + 0.9), mix(pal.app, hex("#4a5058"), 0.5));
        cam.face(x, fZ(bz, bx, bx + 0.9, 0, 2.1), mix(pal.app, hex("#6a7078"), 0.5));
        cam.face(x, fZ(bz - 0.01, bx + 0.12, bx + 0.78, 0.3, 1.9), alpha(hex("#d8e8f0"), 0.35));
        cam.face(x, fZ(bz - 0.02, bx + 0.3, bx + 0.6, 1.1, 1.5), mix(pal.app, hex("#1a1a1e"), 0.4));
        cam.face(x, fZ(bz - 0.02, bx, bx + 0.9, 2.1, 2.3), hex("#2a5ac8"));
        m_booth = cam.P(bx + 0.45, 2.35, bz - 0.3);
    }

    void canopy(QPainter& x, const Pal& pal) const
    {
        const Cam& cam = m_cam;
        const QColor white = mix(pal.app, hex("#eef2f2"), 0.82), red = hex("#c8302a");
        cam.face(x, fZ(ISL.z0, ISL.x0, ISL.x1, 0, 0.18), mix(pal.app, hex("#d8b030"), 0.4));
        cam.face(x, fY(0.18, ISL.x0, ISL.x1, ISL.z0, ISL.z1), mix(pal.app, hex("#b8bcc0"), 0.6));
        for (double cx : COLS) {
            cam.face(x, fZ(25.3, cx - 0.17, cx + 0.17, 0.18, C.y0), mix(pal.app, hex("#c8cccc"), 0.85));
            cam.face(x, fZ(25.29, cx - 0.17, cx + 0.17, 0.18, 0.8), red);
            cam.face(x, fX(cx > 0 ? cx - 0.17 : cx + 0.17, 0.18, C.y0, 25.3, 25.7), mix(pal.app, hex("#9aa0a0"), 0.8));
        }
        for (double px : PUMPS) {
            cam.face(x, fX(px > 0 ? px - 0.5 : px + 0.5, 0.18, 1.85, 25.25, 25.75), mix(pal.app, hex("#a8aeb0"), 0.8));
            cam.face(x, fZ(25.25, px - 0.5, px + 0.5, 0.18, 1.85), mix(pal.app, hex("#dde2e2"), 0.85));
            cam.face(x, fZ(25.24, px - 0.5, px + 0.5, 0.18, 0.45), mix(pal.app, hex("#6a7074"), 0.6));
            cam.face(x, fZ(25.23, px - 0.55, px + 0.55, 1.85, 2.15), hex("#d8382e"));
            cam.face(x, fZ(25.22, px - 0.55, px + 0.55, 1.97, 2.03), alpha(Qt::white, 0.8));
            cam.face(x, fZ(25.22, px - 0.3, px + 0.3, 1.35, 1.65), mix(pal.app, hex("#1e2226"), 0.4));
            cam.face(x, fZ(25.21, px - 0.22, px + 0.22, 1.45, 1.57), alpha(hex("#8affc0"), 0.7));
            for (double k : { -0.38, 0.38 }) cam.face(x, fZ(25.21, px + k - 0.06, px + k + 0.06, 0.9, 1.2), hex("#14161a"));
            const QPointF a = cam.P(px - 0.38, 1.0, 25.2), b = cam.P(px - 0.45, 0.35, 25.0), e = cam.P(px - 0.2, 0.9, 25.2);
            QPainterPath hose;
            hose.moveTo(a);
            hose.quadTo(b.x(), b.y() + 3, e.x(), e.y());
            x.setPen(pen(hex("#0c0d10"), 1));
            x.setBrush(Qt::NoBrush);
            x.drawPath(hose);
            x.setPen(Qt::NoPen);
        }
        // forro aceso visto de baixo, testeira branca com a faixa vermelha
        cam.face(x, fY(C.y0, C.x0, C.x1, C.z0, C.z1), vgrad(cam.P(0, C.y0, C.z0).y(), cam.P(0, C.y0, C.z1).y(), mix(pal.app, hex("#c4ccca"), 0.85), mix(pal.app, hex("#9aa4a2"), 0.8)));
        for (const QPointF& l : m_lights) cam.face(x, fY(C.y0 - 0.001, l.x() - 0.3, l.x() + 0.3, l.y() - 0.6, l.y() + 0.6), mix(pal.app, hex("#6a7270"), 0.6));
        const double sideX = C.x0 > 0 ? C.x0 : C.x1;
        auto fascia = [&](const std::function<Pts(double, double)>& pts, const QColor& base) {
            cam.face(x, pts(C.y0, C.y1), base);
            cam.face(x, pts(4.68, 4.95), red);
            cam.face(x, pts(C.y1 - 0.04, C.y1), alpha(Qt::white, 0.5));
            cam.face(x, pts(C.y0, C.y0 + 0.04), alpha(Qt::black, 0.3));
        };
        fascia([&](double a, double b) { return fX(sideX, a, b, C.z0, C.z1); }, mix(white, hex("#9aa0a8"), 0.25));
        fascia([&](double a, double b) { return fZ(C.z0, C.x0, C.x1, a, b); }, white);
        for (int i = 0; i < 8; ++i) { const double X = R(C.x0 + 0.3, C.x1 - 0.3); cam.face(x, fZ(C.z0 - 0.005, X, X + R(0.05, 0.15), C.y0 + 0.05, C.y0 + R(0.2, 0.5)), alpha(hex("#6a3a1a"), 0.35)); }
    }

    // a plaquinha GAS no poste (a caixa luminosa acende a cada quadro)
    void signPole(QPainter& x, const Pal& pal) const
    {
        const Cam& cam = m_cam;
        const QColor steel = mix(pal.app, hex("#2a2c34"), 0.5);
        const QPointF a = cam.P(SIGN.x - 0.12, 6.0, SIGN.z), b = cam.P(SIGN.x + 0.12, 0, SIGN.z);
        x.fillRect(QRectF(a, b), vgrad(a.y(), b.y(), mix(steel, hex("#ff8a70"), 0.12), steel));
        cam.face(x, fZ(SIGN.z - 0.3, SIGN.x - 0.4, SIGN.x + 0.4, 0, 0.4), mix(pal.app, hex("#7a7c80"), 0.5));
        cam.face(x, fZ(SIGN.z - 0.31, SIGN.x - 0.4, SIGN.x + 0.4, 0.3, 0.4), hex("#c8a020"));
        const QPointF e = cam.P(SIGN.x - 1.1, 7.6, SIGN.z), g = cam.P(SIGN.x + 1.1, 5.9, SIGN.z);
        x.fillRect(QRectF(e.x() - 2, e.y() - 2, g.x() - e.x() + 4, g.y() - e.y() + 4), mix(pal.app, hex("#3a3c44"), 0.4));
    }

    // um veículo de lado pra gente (andando em X)
    void drawVeh(QPainter& p, const Veh& v, const Pal& pal) const
    {
        const Cam& cam = m_cam;
        const Kind& K = kindOf(v.kind);
        const double zn = v.z - K.w / 2;
        const QColor base = mix(v.col, Qt::black, 0.6);
        const bool frontVis = v.dir * (0 - v.x) > 0, rearVis = -v.dir * (0 - wx(v, K.L)) > 0;
        const QColor sideC = mix(base, pal.app, 0.35), topC = mix(base, hex("#8a9ab8"), 0.12), glassC = mix(hex("#0a0e18"), hex("#5a6a8a"), 0.15);
        std::vector<const Part*> parts;
        for (const Part& pt : K.parts) parts.push_back(&pt);
        if (frontVis) std::reverse(parts.begin(), parts.end());
        for (const Part* part : parts) {
            const auto& pr = part->prof;
            const size_t n = pr.size();
            const double w2 = part->box ? 1.3 : K.w / 2, a = v.z - w2, b = v.z + w2;
            const QColor endC = part->box ? mix(v.trailer, Qt::black, 0.55) : mix(base, Qt::black, 0.15);
            if (frontVis) cam.face(p, fX(wx(v, part->d0 + pr[0].d), pr[0].Y, part->box ? 4.1 : pr[1].Y, a, b), endC);
            if (rearVis) cam.face(p, fX(wx(v, part->d0 + pr[n - 1].d), pr[n - 1].Y, part->box ? 4.1 : pr[n - 2].Y, a, b), part->box ? mix(v.trailer, Qt::black, 0.5) : endC);
            if (!part->box)
                for (size_t i = 1; i + 2 < n; ++i) {
                    const double X1 = wx(v, part->d0 + pr[i].d), X2 = wx(v, part->d0 + pr[i + 1].d), Y1 = pr[i].Y, Y2 = pr[i + 1].Y, tx = X2 - X1, ty = Y2 - Y1;
                    const double nx = v.dir > 0 ? ty : -ty, ny = v.dir > 0 ? -tx : tx;
                    if (nx * (0 - (X1 + X2) / 2) + ny * (cam.CH - (Y1 + Y2) / 2) <= 0) continue;
                    cam.face(p, { { X1, Y1, a + 0.05 }, { X2, Y2, a + 0.05 }, { X2, Y2, b - 0.05 }, { X1, Y1, b - 0.05 } }, pr[i].g ? glassC : topC);
                }
            Pts side;
            for (const ProfPt& q : pr) side.push_back({ wx(v, part->d0 + q.d), q.Y, a });
            cam.face(p, side, part->box ? mix(mix(v.trailer, Qt::black, 0.6), pal.app, 0.2) : sideC);
            if (part->box) {
                QVector<QLineF> ls;
                for (double d = 0.8; d < 15; d += 0.8) ls << QLineF(cam.P(wx(v, part->d0 + d), 1.25, a), cam.P(wx(v, part->d0 + d), 4.05, a));
                p.setPen(pen(alpha(Qt::black, 0.25), 0.6));
                p.drawLines(ls);
                p.setPen(Qt::NoPen);
            }
        }
        Pts win;
        for (const QPointF& q : K.win) win.push_back({ wx(v, q.x()), q.y(), zn - 0.002 });
        cam.face(p, win, glassC);
        if (K.pillar > 0) {
            const double pa = wx(v, K.pillar), pb = wx(v, K.pillar + 0.12);
            cam.face(p, fZ(zn - 0.003, qMin(pa, pb), qMax(pa, pb), K.win[0].y(), K.win[1].y()), sideC);
        }
        // a luz de trás recortando o teto
        {
            const auto& top = K.parts[0].prof;
            QPainterPath t;
            for (size_t i = 1; i + 1 < top.size(); ++i) {
                const QPointF q = cam.P(wx(v, top[i].d), top[i].Y, zn);
                if (i == 1) t.moveTo(q); else t.lineTo(q);
            }
            p.setPen(pen(alpha(hex("#b8d0d0"), 0.18), 1));
            p.setBrush(Qt::NoBrush);
            p.drawPath(t);
            p.setPen(Qt::NoPen);
        }
        for (double d : K.wheels) {
            const QPointF cc = cam.P(wx(v, d), K.wr, zn - 0.01);
            const double rr = K.wr * cam.F / zn;
            p.setBrush(hex("#050506"));
            p.drawEllipse(cc, rr, rr);
            p.setBrush(alpha(hex("#5a5a60"), 0.5));
            p.drawEllipse(cc, rr * 0.45, rr * 0.45);
        }
    }

    void vehLights(QPainter& p, const Veh& v) const
    {
        const Cam& cam = m_cam;
        const Kind& K = kindOf(v.kind);
        const double s = cam.F / v.z, fx = v.x, rxx = wx(v, K.L);
        const bool frontVis = v.dir * (0 - v.x) > 0, rearVis = -v.dir * (0 - rxx) > 0;
        // facho no asfalto à frente
        const double zA = v.z - K.w / 2 - 0.3, zB = v.z + K.w / 2 + 0.3, xB = fx + v.dir * 32;
        const double ax = cam.P(fx, 0, v.z).x(), bx = cam.P(xB, 0, v.z).x();
        for (const auto& sa : { std::make_pair(3.2, 0.06), std::make_pair(1.6, 0.1) }) {
            QLinearGradient g(ax, 0, bx, 0);
            g.setColorAt(0, alpha(hex("#fff2d0"), sa.second));
            g.setColorAt(1, alpha(hex("#fff2d0"), 0));
            cam.face(p, { { fx, 0, zA + 0.5 }, { xB, 0, zA - sa.first * 0.4 }, { xB, 0, zB + sa.first }, { fx, 0, zB - 0.5 } }, g);
        }
        // o farol e a lanterna do lado de lá (os do lado de cá vão por cima do carro)
        {
            const double zz = v.z + K.w / 2 - 0.25;
            const QPointF h = cam.P(fx, K.head[0].y(), zz), t = cam.P(rxx, K.tail[0].y(), zz);
            glowAt(p, h.x(), h.y(), (frontVis ? 1.2 : 0.6) * s, hex("#fff4dc"), frontVis ? 0.4 : 0.2);
            glowAt(p, t.x(), t.y(), 0.4 * s, hex("#ff2010"), rearVis ? 0.4 : 0.2);
        }
        if (v.kind == Truck) {
            const double zn = v.z - K.w / 2;
            auto lamp = [&](const QPointF& q, double r, const QColor& g, const QColor& core, double cs) {
                glowAt(p, q.x(), q.y(), r * s, g, 0.5);
                p.fillRect(QRectF(q.x() - cs / 2, q.y() - cs / 2, cs, cs), core);
            };
            for (double d = 7; d < 21; d += 3) lamp(cam.P(wx(v, d), 1.35, zn - 0.01), 0.25, hex("#ffa020"), hex("#ffd080"), 1.6);
            for (double d : { 6.0, 20.9 }) lamp(cam.P(wx(v, d), 4.1, zn), 0.25, d > 20 ? hex("#ff2010") : hex("#ffa020"), d > 20 ? hex("#ff6050") : hex("#ffd080"), 1.6);
            for (int k = 0; k < 5; ++k) lamp(cam.P(wx(v, 3.62), 3.57, zn + 0.6 + k * 0.32), 0.15, hex("#ffa020"), hex("#ffd080"), 1.4);
        }
        const QPointF gp = cam.P(rxx - v.dir * 1.5, 0, v.z);
        p.save();
        p.translate(gp);
        p.scale(1, 0.25);
        glowAt(p, 0, 0, 2.2 * s, hex("#ff2010"), 0.14);
        p.restore();
    }

    Cam m_cam;
    Box C {}, ISL {}, ST {};
    struct { double x, z; } SIGN { 0, 19 };
    double PUMPS[2] {}, COLS[2] {};
    std::vector<QPointF> m_lights;           // (X, Z) de cada luminária
    QString m_key;
    QPixmap m_back, m_tumble;
    std::vector<Twinkle> m_tw;
    std::vector<Moth> m_moths;
    std::vector<Veh> m_veh;
    Veh m_parked { 1, Car, 31, 0, 0, 0, QColor(), QColor() };
    Weed m_weed;
    Star m_star;
    QPointF m_ranch, m_booth;
    double m_t = 0, m_flick = 0, m_signOff = 0, m_nextVeh = 7, m_nextWeed = 25, m_nextStar = 12, m_emberT = 0, m_emberNext = 6;
};

// ============================================================ Route 66
// Nasceu como o primeiro rework do Last Exit. A estrada some no horizonte no vão entre duas mesas
// (a estrada de Monument Valley); o posto fica à direita dela: cobertura com o forro aceso e a lateral
// virada pra estrada, duas ilhas de bomba, a lojinha quente ao fundo com o balconista, e o letreiro
// de neon dos anos 50 (o X queima de vez em quando, a estrela pisca, a seta de lâmpadas corre).
// Carros e carretas vêm do horizonte com os faróis crescendo, ou passam por nós e somem com a
// lanterna vermelha. Um outdoor velho diz LAST GAS · NEXT 112 MI. Às vezes um tumbleweed.
class Route66 : public Layer {
public:
    void init(Ctx&) override
    {
        m_key.clear();
        m_veh.clear();
        m_first = true;
        m_nextVeh = R(8, 14);
        m_weed.on = false;
        m_nextWeed = R(10, 20);
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_flick = rnd() < 0.06 ? 1 : m_flick * 0.6;
        if (m_signOff > 0) m_signOff -= dt; else if (rnd() < dt * 0.05) m_signOff = R(0.3, 1.6);
        m_openFlick = rnd() < 0.01 ? 1 : m_openFlick * 0.8;
        for (Moth& m : m_moths) m.a += m.f * dt;
        if (m_first) { m_veh.push_back(spawn(true)); m_first = false; }
        m_nextVeh -= dt;
        if (m_nextVeh <= 0) { m_veh.push_back(spawn(false)); m_nextVeh = R(10, 24); }
        for (Veh& v : m_veh) v.z += v.dir * v.v * dt;
        m_veh.erase(std::remove_if(m_veh.begin(), m_veh.end(), [](const Veh& v) { return v.dir < 0 ? v.z <= -kindOf(v.kind).L - 2 : v.z >= 1500; }), m_veh.end());
        m_nextWeed -= dt;
        if (!m_weed.on && m_nextWeed <= 0) m_weed = { true, -14, R(10.2, 12.5), R(3, 5), 0, R(0.36, 0.5) };
        if (m_weed.on) {
            m_weed.X += m_weed.v * dt * (0.8 + 0.4 * std::sin(m_t * 1.7));
            m_weed.b += dt;
            if (m_weed.X > 28) { m_weed.on = false; m_nextWeed = R(40, 90); }
        }
        // balconista: fica no caixa, às vezes vai até a prateleira e volta
        m_clerkWait -= dt;
        if (m_clerkWait <= 0 && std::abs(m_clerkX - m_clerkTo) < 0.01) {
            m_clerkTo = m_clerkTo < 10 ? R(11.2, 16.5) : 9.3;
            m_clerkWait = m_clerkTo < 10 ? R(8, 20) : R(2, 5);
        }
        if (std::abs(m_clerkX - m_clerkTo) > 0.01) m_clerkX += (m_clerkTo > m_clerkX ? 1 : -1) * qMin(std::abs(m_clerkTo - m_clerkX), 1.1 * dt);
        const double VX = m_cam.VX;
        m_nextStar -= dt;
        if (!m_star.on && m_nextStar <= 0) m_star = { true, R(VX - 40, VX + 280), R(30, 200), R(0.35, 0.7), 0 };
        if (m_star.on) { m_star.t += dt; if (m_star.t > 0.7) { m_star.on = false; m_nextStar = R(20, 50); } }
        m_nextSat -= dt;
        if (!m_sat.on && m_nextSat <= 0) m_sat = { true, VX - 80, R(40, 160), 0, 0 };
        if (m_sat.on) { m_sat.x += 9 * dt; m_sat.y += 1.2 * dt; if (m_sat.x > VX + 340) { m_sat.on = false; m_nextSat = R(40, 90); } }
        (void)c;
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const QString key = QString::number(zx(c, 120), 'f', 1) + QLatin1Char('|') + QString::number(c.W, 'f', 0) + QLatin1Char('|') + QString::number(c.px, 'f', 2) + pal.app.name();
        if (key != m_key) {
            Seeded sd(0x66A66u);
            build(pal, c);
            m_key = key;
        }
        const Cam& cam = m_cam;
        const double HY = cam.HY, F = cam.F;
        p.setPen(Qt::NoPen);
        blit(p, m_back, 0, 0, c.W, c.H);
        for (const Twinkle& s : m_tw)
            p.fillRect(QRectF(s.x, s.y, 1.6, 1.6), alpha(s.c, s.a * (0.55 + 0.45 * std::sin(m_t * s.f + s.ph)) * (1 - smooth(HY - 150, HY - 8, s.y))));
        if (m_sat.on) p.fillRect(QRectF(m_sat.x, m_sat.y, 1.2, 1.2), alpha(Qt::white, 0.6));
        if (m_star.on) {
            const double k = m_star.t / 0.7;
            p.setPen(pen(alpha(Qt::white, m_star.a * std::sin(k * M_PI)), 1.1));
            p.drawLine(QPointF(m_star.x + k * 90, m_star.y + k * 40), QPointF(m_star.x + k * 90 - 63, m_star.y + k * 40 - 28));
            p.setPen(Qt::NoPen);
        }
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        glowAt(p, m_sideLamp.x(), m_sideLamp.y() + 8, 26, hex("#ff9a3a"), 0.22);
        glowAt(p, m_sideLamp.x(), m_sideLamp.y(), 5, hex("#ffd090"), 0.8);
        glowAt(p, m_bill.x(), m_bill.y(), 10, hex("#ffe0a8"), 0.1);
        for (size_t i = 0; i < m_towers.size(); ++i) {
            const double on = std::sin(m_t * 2.2 + i * 1.7) > 0.2 ? 1 : 0.08;
            glowAt(p, m_towers[i].x(), m_towers[i].y(), 9, hex("#ff2a1a"), 0.7 * on);
            p.fillRect(QRectF(m_towers[i].x() - 0.8, m_towers[i].y() - 0.8, 1.6, 1.6), alpha(hex("#ff6050"), on));
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // dentro da loja: o balconista, depois balcão/caixilhos/cartazes por cima, e o OPEN de neon
        {
            const bool walking = std::abs(m_clerkX - m_clerkTo) > 0.01;
            const QPointF cp = cam.P(m_clerkX, 0.1, ST.z0 + 0.9);
            Fig f;
            f.pose = walking ? Fig::Walk : Fig::Stand;
            f.ph = m_t * 7;
            f.col = alpha(hex("#3a2216"), 0.85);
            f.hair = Fig::Cap;
            f.arm = walking ? Fig::Down : Fig::Counter;
            f.side = -1;
            figure(p, cp.x(), cp.y(), 1.72 * F / (ST.z0 + 0.9), f);
            p.setPen(Qt::NoPen);
        }
        blit(p, m_glass, m_glassR.x(), m_glassR.y(), m_glassR.width(), m_glassR.height());
        {
            const double on = m_openFlick > 0.5 ? 0.2 : 1, s = F / ST.z0;
            const QPointF o = cam.P(15.5, 1.25, ST.z0);
            p.setPen(pen(alpha(hex("#3a8aff"), 0.9 * on), 1));
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRectF(o.x() - 0.75 * s, o.y() - 0.25 * s, 1.5 * s, 0.5 * s));
            text(p, QStringLiteral("OPEN"), o.x(), o.y() + 0.5, alpha(hex("#ff4a3a"), on), fnt(0.36 * s, 800));
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            glowAt(p, o.x(), o.y(), 0.9 * s, hex("#ff4a3a"), 0.35 * on);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        // veículos longe (atrás do letreiro), com o facho no asfalto
        std::sort(m_veh.begin(), m_veh.end(), [](const Veh& a, const Veh& b) { return a.z > b.z; });
        auto drawV = [&](bool nearOnes) {
            for (const Veh& v : m_veh) if ((v.z <= SZ) == nearOnes && v.z < 260) drawBody(p, v, pal);
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            for (const Veh& v : m_veh) if ((v.z <= SZ) == nearOnes) drawLights(p, v);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        };
        drawV(false);
        blit(p, m_far, 0, 0, c.W, c.H);
        // luminárias do forro (uma falhando) e a névoa acesa embaixo
        const double lit = 1 - 0.55 * m_flick;
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.setOpacity(0.75 + 0.25 * lit);
        blit(p, m_air, m_airR.x(), m_airR.y(), m_airR.width(), m_airR.height());
        p.setOpacity(1);
        for (int i = 0; i < int(m_lights.size()); ++i) {
            const double on = i == 7 ? 1 - m_flick : 1, lx = m_lights[i].x(), lz = m_lights[i].y();
            cam.face(p, fY(C.y0 - 0.002, lx - 0.26, lx + 0.26, lz - 0.62, lz + 0.62), alpha(hex("#f4fffa"), 0.95 * on));
            const QPointF g = cam.P(lx, C.y0, lz);
            glowAt(p, g.x(), g.y() + 2, 34 * 10 / lz, hex("#e8fff4"), 0.32 * on);
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // tumbleweed rolando pela estrada (na frente das ilhas)
        if (m_weed.on && m_weed.Z < 12) {
            const double hop = std::abs(std::sin(m_weed.b * 3.1)) * 0.35, s = F / m_weed.Z, r = m_weed.r * s;
            const QPointF sp = cam.P(m_weed.X, m_weed.r + hop, m_weed.Z), gp = cam.P(m_weed.X, 0, m_weed.Z);
            p.setBrush(alpha(Qt::black, 0.3));
            p.drawEllipse(gp, r * 0.9, r * 0.18);
            p.save();
            p.translate(sp);
            p.rotate(qRadiansToDegrees(m_weed.X / m_weed.r));
            p.setOpacity(0.6 + (m_weed.X > 0 ? 0.3 : 0.1));
            blit(p, m_tumble, -r, -r, r * 2, r * 2);
            p.restore();
        }
        blit(p, m_near, m_nearR.x(), m_nearR.y(), m_nearR.width(), m_nearR.height());
        // letreiro: painel (com o X falhando), dígitos de LED, lâmpadas da seta correndo, estrela piscando
        const bool broken = m_signOff > 0 && rnd() < 0.75;
        blit(p, broken ? m_boardOff : m_boardOn, m_boardR.x(), m_boardR.y(), m_boardR.width(), m_boardR.height());
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        blit(p, m_boardGlow, m_boardR.x(), m_boardR.y(), m_boardR.width(), m_boardR.height());
        for (const auto& ty : { std::make_pair(QStringLiteral("4.29"), 5.8), std::make_pair(QStringLiteral("4.89"), 5.3) }) {
            text(p, ty.first, sgx(2.3), sgy(ty.second) + 1, alpha(hex("#ff4a2a"), 0.95), fnt(0.4 * SS(), 700, 1), 1);
            text(p, QStringLiteral("9"), sgx(2.5), sgy(ty.second + 0.08), alpha(hex("#ff4a2a"), 0.95), fnt(0.2 * SS(), 700, 1), 1);
        }
        glowAt(p, sgx(1.9), sgy(5.55), 1.2 * SS(), hex("#ff3a1a"), 0.12);
        glowAt(p, sgx(1.4), sgy(6.6), 2.0 * SS(), hex("#fff0d8"), 0.12);
        const int st = int(std::floor(m_t * 9));
        for (int i = 0; i < int(m_bulbs.size()); ++i) {
            const double on = (i + st) % 3 == 0 ? 1 : 0.25, X = m_boardR.x() + m_bulbs[i].x(), Y = m_boardR.y() + m_bulbs[i].y();
            glowAt(p, X, Y, 5, hex("#ffd890"), 0.45 * on);
            p.fillRect(QRectF(X - 1, Y - 1, 2, 2), alpha(hex("#fff4d0"), 0.35 + 0.65 * on));
        }
        for (int i = 0; i < int(m_starTips.size()); ++i) {
            const double on = int(std::floor(m_t * 3)) % 8 == i ? 1 : 0.5, X = m_boardR.x() + m_starTips[i].x(), Y = m_boardR.y() + m_starTips[i].y();
            glowAt(p, X, Y, 6 * on + 2, hex("#fff2b0"), 0.6 * on);
            p.fillRect(QRectF(X - 1, Y - 1, 2, 2), hex("#fffbe8"));
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        drawV(true);
        // mariposas
        const QColor mc = alpha(hex("#fff8e8"), 0.8);
        for (const Moth& m : m_moths)
            p.fillRect(QRectF(m.cx + std::cos(m.a) * m.r + std::sin(m.a * 2.3 + m.ph) * 3, m.cy + std::sin(m.a * 1.3 + m.ph) * m.r * 0.5, 1.4, 1.4), mc);
    }

private:
    struct Moth { double cx, cy, r, a, f, ph; };
    struct Veh { int dir; int kind; double z, v, X; QColor col, trailer; };
    struct Weed { bool on = false; double X = 0, Z = 11, v = 4, b = 0, r = 0.45; };
    struct Star { bool on = false; double x = 0, y = 0, a = 0, t = 0; };
    struct Sat { bool on = false; double x = 0, y = 0, a = 0, t = 0; };
    struct BoxC { double x0, x1, z0, z1, y0, y1; };

    // o lugar (metros)
    static constexpr double RL = -6.2, RR = 0.8, SZ = 9;
    const BoxC C { 2.6, 11.2, 10, 24, 4.6, 5.45 };
    const BoxC ST { 6, 21, 27.5, 40, 0, 3.7 };
    double SS() const { return m_cam.F / SZ; }
    double sgx(double X) const { return m_cam.VX + m_cam.F * X / SZ; }
    double sgy(double Y) const { return m_cam.HY + m_cam.F * (m_cam.CH - Y) / SZ; }
    static QPointF arrowAt(double t)
    {
        const double u = 1 - t, ax = 3.45, ay = 8.05, bx = 4.55, by = 7.55, cx = 4.4, cy = 6.25;
        return QPointF(u * u * ax + 2 * u * t * bx + t * t * cx, u * u * ay + 2 * u * t * by + t * t * cy);
    }

    Veh spawn(bool first) const
    {
        Veh v;
        v.dir = rnd() < 0.55 ? -1 : 1;
        v.kind = randomKind();
        v.v = R(24, 32) * (v.kind == Truck ? 0.82 : 1);
        v.X = v.dir < 0 ? -4.35 : -0.9;
        v.col = randomCarColor(true);
        v.trailer = randomTrailer();
        v.z = v.dir < 0 ? (first ? R(250, 450) : 1200) : (first ? R(60, 160) : -kindOf(v.kind).L - 4);
        return v;
    }
    // Z do mundo de um ponto d (a partir da frente do veículo)
    static double vz(const Veh& v, double d) { return v.dir < 0 ? v.z + d : v.z + (kindOf(v.kind).L - d); }

    // um pedaço de imagem só do tamanho do que ele cobre (economiza memória)
    QPixmap sub(const Ctx& c, const QRectF& r, QRectF& out) const
    {
        out = QRectF(std::floor(r.x()), std::floor(r.y()), std::ceil(r.width()) + 1, std::ceil(r.height()) + 1);
        return layer(c, out.width(), out.height());
    }

    void build(const Pal& pal, const Ctx& c)
    {
        m_cam.VX = zx(c, 120);
        const Cam& cam = m_cam;
        const double HY = cam.HY, F = cam.F, VX = cam.VX;
        m_lights.clear();
        for (double lz : { 11.2, 14.0, 16.8, 19.6, 22.4 })
            for (double lx : { 4.0, 6.9, 9.8 }) m_lights.push_back(QPointF(lx, lz));
        m_back = layer(c, c.W, c.H);
        {
            QPainter x(&m_back);
            x.setRenderHint(QPainter::Antialiasing, true);
            SkyOpt so;
            so.ax = VX - 160; so.ay = HY - 40; so.bx = VX + 560; so.by = -80;
            so.mx = VX + 228; so.my = 92; so.mr = 10; so.cut = -1;
            const QColor hz = paintSky(x, cam, pal, c, so, m_tw);
            mesas(x, pal, c, hz);
            m_towers = { tower(x, pal, c.W - 150, 320, 60), tower(x, pal, VX + 300, 380, 50) };
            paintGround(x, cam, pal, c, hz, [](double X, double Z) { return (X > RL - 1.6 && X < RR + 0.5) || (X > 1.1 && X < 18.2 && Z > 6.8 && Z < 40.2); }, 420, 160);
            const QColor jl = mix(pal.app, hex("#0a0a10"), 0.6), jt = mix(pal.app, hex("#08080c"), 0.6), jr = alpha(hex("#8a98c8"), 0.25);
            for (const auto& j : { std::array<double, 3> { VX - 34, 95, 5.5 }, std::array<double, 3> { VX - 58, 140, 6 }, std::array<double, 3> { VX - 20, 210, 5 } })
                joshua(x, cam, cam.sxX(j[0], j[1]), j[1], j[2], jl);
            for (const auto& j : { std::array<double, 3> { c.W - 60, 17, 7.5 }, std::array<double, 3> { c.W - 250, 34, 6 }, std::array<double, 3> { c.W - 330, 60, 5 }, std::array<double, 3> { c.W - 170, 90, 5.5 } })
                joshua(x, cam, cam.sxX(j[0], j[1]), j[1], j[2], jt, jr);
            poles(x, pal);
            billboard(x, pal, c);
            road(x, pal);
            apron(x, pal);
            store(x, pal, c);
            // a luz no chão: cada luminária e a cobertura toda
            x.setCompositionMode(QPainter::CompositionMode_Plus);
            for (const QPointF& l : m_lights) pool(x, l.x(), l.y(), 5.5, hex("#cfe8e0"), 0.09);
            pool(x, 6.5, 17, 16, hex("#bfe0d8"), 0.07);
            x.setCompositionMode(QPainter::CompositionMode_SourceOver);
            for (const auto& isl : { std::make_pair(8.4, 9.4), std::make_pair(4.6, 5.6) }) cam.face(x, fY(0.012, isl.first - 0.5, isl.first, 12.2, 20.8), alpha(Qt::black, 0.18));
        }
        m_far = layer(c, c.W, c.H);
        {
            QPainter x(&m_far);
            x.setRenderHint(QPainter::Antialiasing, true);
            canopy(x, pal);
        }
        {
            const QPointF foot = cam.P(1.0, 0, 8.7);
            m_near = sub(c, QRectF(QPointF(sgx(0.0) - 6, sgy(7.3) - 6), QPointF(sgx(2.8) + 12, foot.y() + 6)), m_nearR);
            QPainter x(&m_near);
            x.setRenderHint(QPainter::Antialiasing, true);
            x.translate(-m_nearR.topLeft());
            signNear(x, pal);
        }
        buildBoard(pal, c);
        {
            QPolygonF q1, q2;
            cam.poly({ { C.x0, C.y0, C.z0 }, { C.x1, C.y0, C.z0 }, { C.x1 + 1.5, 0, C.z0 - 1 }, { C.x0 - 1.5, 0, C.z0 - 1 } }, q1);
            cam.poly({ { C.x0, C.y0, C.z0 }, { C.x0, C.y0, C.z1 }, { C.x0 - 1.5, 0, C.z1 }, { C.x0 - 1.5, 0, C.z0 - 1 } }, q2);
            m_air = sub(c, (q1.boundingRect() | q2.boundingRect()).intersected(QRectF(0, 0, c.W, c.H)), m_airR);
            QPainter a(&m_air);
            a.setRenderHint(QPainter::Antialiasing, true);
            a.translate(-m_airR.topLeft());
            a.setPen(Qt::NoPen);
            a.setBrush(vgrad(cam.P(0, C.y0, C.z0).y(), cam.P(0, 0, C.z0).y(), alpha(hex("#e8fff4"), 0.1), alpha(hex("#e8fff4"), 0.02)));
            a.drawPolygon(q1);
            a.setBrush(alpha(hex("#e8fff4"), 0.04));
            a.drawPolygon(q2);
        }
        m_tumble = tumbleSprite(true);
        m_moths.clear();
        for (int i = 0; i < 22; ++i) {
            const QPointF l = m_lights[RI(9)], q = cam.P(l.x(), C.y0 - 0.5, l.y());
            m_moths.push_back({ q.x(), q.y(), R(3, 14), R(0, 6.28), R(2, 5) * (rnd() < 0.5 ? 1 : -1), R(0, 6.28) });
        }
        for (int i = 0; i < 14; ++i) m_moths.push_back({ sgx(R(0, 3)), sgy(R(7.2, 9.3)), R(8, 30), R(0, 6.28), R(1.5, 4) * (rnd() < 0.5 ? 1 : -1), R(0, 6.28) });
        (void)F;
    }

    void pool(QPainter& x, double X0, double Z0, double Rr, const QColor& col, double a) const
    {
        Pts pts;
        for (int k = 0; k < 28; ++k) { const double t = k / 28.0 * 6.28; pts.push_back({ X0 + std::cos(t) * Rr, 0.01, Z0 + std::sin(t) * Rr }); }
        QPolygonF q;
        if (!m_cam.poly(pts, q)) return;
        const QPointF ctr = m_cam.P(X0, 0, Z0), e = m_cam.P(X0 + Rr, 0, Z0);
        QRadialGradient g(ctr, qMax(1.0, std::abs(e.x() - ctr.x())));
        g.setColorAt(0, alpha(col, a));
        g.setColorAt(0.5, alpha(col, a * 0.45));
        g.setColorAt(1, alpha(col, 0));
        x.setPen(Qt::NoPen);
        x.setBrush(g);
        x.drawPolygon(q);
    }

    // mesas/buttes: talude côncavo embaixo, paredão com ressaltos, topo chato; borda acesa do lado da lua
    struct Butte { double x0, x1, h, tw, cw, tk, jk; std::vector<std::pair<double, double>> sl, sr; Profile prof; };
    static std::vector<std::pair<double, double>> ledges()
    {
        std::vector<std::pair<double, double>> out;
        const int n = 2 + int(std::floor(R(0, 3)));
        double at = 0, lift = 0;
        for (int i = 0; i < n; ++i) { at += R(0.1, 0.4); lift += R(0.15, 0.45); out.push_back({ qMin(0.95, at), qMin(1.0, lift) }); }
        out.push_back({ 0.97, 1 });
        return out;
    }
    static Butte newButte(double cx, double w, double h, bool spire = false)
    {
        Butte b;
        b.prof = profile(5, 0.6);
        b.x0 = cx - w / 2; b.x1 = cx + w / 2; b.h = h;
        b.tw = w * (spire ? 0.32 : R(0.2, 0.3));
        b.cw = spire ? w * 0.12 : R(5, 12);
        b.tk = spire ? R(0.12, 0.2) : R(0.25, 0.38);
        b.sl = ledges(); b.sr = ledges();
        b.jk = spire ? 1 : 2.4;
        return b;
    }
    static double butteH(const std::vector<Butte>& list, double xx)
    {
        double h = 0;
        for (const Butte& b : list) {
            const double d = qMin(xx - b.x0, b.x1 - xx);
            if (d <= 0) continue;
            const bool left = xx - b.x0 < b.x1 - xx;
            double v;
            if (d < b.tw) v = b.h * b.tk * std::pow(d / b.tw, 1.7);
            else if (d < b.tw + b.cw) {
                const double k = (d - b.tw) / b.cw;
                double q = 0;
                for (const auto& st : left ? b.sl : b.sr) if (k >= st.first) q = st.second;
                v = b.h * (b.tk + (1 - b.tk) * qMax(q, k * k * 0.3));
            } else v = b.h + b.prof.at((xx - b.x0) / (b.x1 - b.x0)) * b.jk;
            h = qMax(h, v);
        }
        return h;
    }
    void mesas(QPainter& x, const Pal& pal, const Ctx& c, const QColor& hz) const
    {
        const double HY = m_cam.HY, VX = m_cam.VX;
        const QColor rim = hex("#b0a0c8");
        auto plane = [&](const std::vector<Butte>& list, const QColor& col, double rimA, bool strata) {
            auto f = [&](double xx) { return HY + 1 - butteH(list, xx); };
            double maxH = 0;
            for (const Butte& b : list) maxH = qMax(maxH, b.h);
            QPolygonF q;
            q << QPointF(-10, HY + 4);
            for (double xx = -10; xx <= c.W + 10; xx += 1) q << QPointF(xx, f(xx));
            q << QPointF(c.W + 10, HY + 4);
            x.setPen(Qt::NoPen);
            x.setBrush(vgrad(HY - maxH, HY + 2, mix(col, hex("#6a5a7a"), 0.12), col));
            x.drawPolygon(q);
            for (const Butte& b : list) {
                if (b.x1 < -10 || b.x0 > c.W + 10) continue;
                const double ytop = HY + 1 - b.h, xr = b.x1 - b.tw - b.cw, xl = b.x0 + b.tw + b.cw;
                QLinearGradient g(xr - 10, 0, xr + b.cw, 0);
                g.setColorAt(0, alpha(rim, 0));
                g.setColorAt(1, alpha(rim, rimA * 0.7));
                QPolygonF r;
                r << QPointF(xr - 10, ytop + 2) << QPointF(xr, ytop) << QPointF(xr + b.cw, HY + 1 - b.h * b.tk) << QPointF(xr - 10, HY + 1 - b.h * b.tk);
                x.setBrush(g);
                x.drawPolygon(r);
                const QColor rc = alpha(rim, rimA * 0.6);
                for (double xx = xl + (xr - xl) * 0.45; xx < xr; xx += 2) if (rnd() < 0.7) x.fillRect(QRectF(xx, f(xx), 2, 0.8), rc);
                if (strata) {
                    const QColor sc = alpha(Qt::black, 0.12);
                    for (int k = 1; k < 4; ++k) {
                        const double yy = ytop + b.h * (1 - b.tk) * k / 4 + R(-1, 1);
                        for (double xx = xl; xx < xr; xx += R(3, 9)) x.fillRect(QRectF(xx, yy + R(-0.6, 0.6), R(2, 7), 0.7), sc);
                    }
                }
                x.fillRect(QRectF(xl - 1, ytop, qMin(6.0, (xr - xl) * 0.1), b.h * (1 - b.tk)), alpha(Qt::black, 0.22));
            }
        };
        auto haze = [&](double a0, double a1, double h) { x.fillRect(QRectF(0, HY - h, c.W, h + 2), vgrad(HY - h, HY + 2, alpha(hz, a0), alpha(hz, a1))); };
        std::vector<Butte> farL;
        for (double cx = -60; cx < c.W + 120; cx += R(60, 150)) farL.push_back(newButte(cx, R(70, 180), R(6, 20)));
        plane(farL, mix(hz, hex("#1a1226"), 0.45), 0.12, false);
        haze(0, 0.35, 24);
        // as duas Mittens de cada lado do sumidouro: a estrada morre no vão entre elas
        std::vector<Butte> midL = { newButte(VX - 70, 110, 70), newButte(VX - 150, 70, 38), newButte(VX + 50, 46, 62), newButte(VX + 82, 14, 54, true), newButte(VX + 160, 70, 30) };
        for (double cx = VX + 330; cx < c.W + 120; cx += R(140, 280)) midL.push_back(newButte(cx, R(70, 200), R(18, 58)));
        for (double cx = VX - 240; cx > -200; cx -= R(120, 200)) midL.push_back(newButte(cx, R(60, 140), R(14, 34)));
        plane(midL, mix(pal.app, hex("#1e1018"), 0.5), 0.42, true);
        haze(0, 0.22, 70);
        // dunas baixas na frente
        const Profile nf = profile(7, 0.55);
        QPolygonF q;
        q << QPointF(-10, HY + 4);
        for (double xx = -10; xx <= c.W + 10; xx += 4) q << QPointF(xx, HY + 1 - qMax(0.0, nf.at(xx / c.W) * 7 + 2));
        q << QPointF(c.W + 10, HY + 4);
        x.setBrush(mix(pal.app, hex("#0a0910"), 0.6));
        x.drawPolygon(q);
    }

    QPointF tower(QPainter& x, const Pal& pal, double sx, double Z, double h) const
    {
        const double X = m_cam.sxX(sx, Z), w = 2.4 * m_cam.F / Z;
        const QPointF b = m_cam.P(X, 0, Z), t = m_cam.P(X, h, Z);
        QVector<QLineF> ls;
        ls << QLineF(b.x() - w, b.y(), t.x(), t.y()) << QLineF(b.x() + w, b.y(), t.x(), t.y());
        for (int k = 1; k < 9; ++k) {
            const double tt = k / 9.0, yy = b.y() + (t.y() - b.y()) * tt, ww = w * (1 - tt);
            ls << QLineF(b.x() - ww, yy, b.x() + ww, yy - (b.y() - t.y()) / 9);
        }
        x.setPen(pen(alpha(mix(pal.app, hex("#2a2c3a"), 0.5), 0.9), 0.7));
        x.drawLines(ls);
        x.setPen(Qt::NoPen);
        return QPointF(t.x(), t.y() - 1);
    }

    void poles(QPainter& x, const Pal& pal) const
    {
        const double X = -8.8, F = m_cam.F;
        const QColor col = mix(pal.app, hex("#06060a"), 0.6);
        std::vector<std::array<QPointF, 3>> tops;
        for (double Z = 18; Z < 700; Z += 42) {
            const double s = F / Z;
            const QPointF b = m_cam.P(X, 0, Z), t = m_cam.P(X, 9.5, Z);
            x.fillRect(QRectF(b.x() - qMax(0.5, 0.14 * s), t.y(), qMax(1.0, 0.28 * s), b.y() - t.y()), col);
            x.fillRect(QRectF(b.x() - 1.1 * s, t.y() + 0.3 * s, 2.2 * s, qMax(0.6, 0.12 * s)), col);
            tops.push_back({ m_cam.P(X - 0.9, 9.1, Z), m_cam.P(X + 0.9, 9.1, Z), m_cam.P(X, 9.5, Z) });
        }
        x.setPen(pen(alpha(mix(pal.app, hex("#2a2e3e"), 0.4), 0.8), 0.6));
        x.setBrush(Qt::NoBrush);
        for (int k = 0; k < 3; ++k) {
            QPainterPath w;
            for (size_t i = 0; i + 1 < tops.size(); ++i) {
                const QPointF a = tops[i][k], b = tops[i + 1][k];
                const double sag = (a.y() < 0 ? 0 : 1) * (b.x() - a.x() == 0 ? 0 : std::abs(a.y() - b.y()) * 0.25 + 18 * (F / (18 + 42 * double(i))) / 20);
                w.moveTo(a);
                w.quadTo((a.x() + b.x()) / 2, (a.y() + b.y()) / 2 + sag, b.x(), b.y());
            }
            x.drawPath(w);
        }
        x.setPen(Qt::NoPen);
    }

    void billboard(QPainter& x, const Pal& pal, const Ctx& c)
    {
        const double Z = 26, X = m_cam.sxX(c.W - 260, Z), s = m_cam.F / Z, bw = 9, bh = 3.6, y0 = 3.2;
        const QColor wood = mix(pal.app, hex("#2a2018"), 0.5);
        auto at = [&](double u, double Y) { return m_cam.P(X + u, Y, Z); };
        for (double u : { -bw * 0.35, 0.0, bw * 0.35 }) {
            const QPointF a = at(u - 0.15, y0 + bh), b = at(u + 0.15, 0);
            x.fillRect(QRectF(a, b), wood);
            x.setPen(pen(wood, qMax(0.6, 0.08 * s)));
            x.drawLine(at(u - 0.15, 0.4), at(u + 1.3, y0 - 0.2));
            x.setPen(Qt::NoPen);
        }
        x.fillRect(QRectF(at(-bw / 2 - 0.2, y0 - 0.25), at(bw / 2 + 0.2, y0 - 0.1)), wood);
        // painel com a pintura velha descascando
        const QPointF A = at(-bw / 2, y0 + bh), D = at(bw / 2, y0);
        const double a = A.x(), b = A.y(), cc = D.x(), d = D.y();
        x.fillRect(QRectF(a - 2, b - 2, cc - a + 4, d - b + 4), mix(pal.app, hex("#5a5248"), 0.55));
        x.fillRect(QRectF(a, b, cc - a, d - b), mix(pal.app, hex("#c8b890"), 0.62));
        x.fillRect(QRectF(a, b + (d - b) * 0.68, cc - a, (d - b) * 0.32), mix(pal.app, hex("#a83a2a"), 0.55));
        text(x, QStringLiteral("LAST GAS"), (a + cc) / 2, b + (d - b) * 0.36, mix(pal.app, hex("#2a2420"), 0.45), fnt(1.5 * s, 900, 2));
        text(x, QStringLiteral("NEXT 112 MI"), (a + cc) / 2, b + (d - b) * 0.84, mix(pal.app, hex("#f0e4c8"), 0.6), fnt(0.7 * s, 800));
        static const char* PEEL[] = { "#000000", "#8a7a60" };
        for (int i = 0; i < 40; ++i) x.fillRect(QRectF(R(a, cc - 8), R(b, d - 3), R(3, 16), R(1, 4)), alpha(hex(pick(PEEL)), R(0.08, 0.25)));
        // as lâmpadas em cima (uma queimada) e a luz no painel
        for (double u : { -bw * 0.25, bw * 0.25 }) {
            const QPointF e = at(u, y0 + bh + 0.6);
            x.fillRect(QRectF(e.x() - 3, e.y() - 1, 6, 3), mix(pal.app, hex("#2a2a30"), 0.5));
            x.setPen(pen(mix(pal.app, hex("#2a2a30"), 0.5), 1));
            x.drawLine(e, QPointF(e.x(), b));
            x.setPen(Qt::NoPen);
        }
        const QPointF e = at(-bw * 0.25, y0 + bh + 0.5);
        x.save();
        x.setClipRect(QRectF(a, b, cc - a, d - b));
        x.setCompositionMode(QPainter::CompositionMode_Plus);
        QRadialGradient lg(QPointF(e.x(), b), (cc - a) * 0.6);
        lg.setColorAt(0, alpha(hex("#ffd890"), 0.3));
        lg.setColorAt(1, alpha(hex("#ffd890"), 0));
        x.fillRect(QRectF(a, b, cc - a, d - b), lg);
        x.restore();
        x.fillRect(QRectF(a, b, cc - a, d - b), vgrad(b, d, alpha(Qt::black, 0), alpha(Qt::black, 0.25)));
        m_bill = QPointF(e.x(), e.y() + 2);
    }

    void road(QPainter& x, const Pal& pal) const
    {
        const Cam& cam = m_cam;
        const double ZF = 4000, HY = cam.HY;
        const QColor gravel = mix(pal.app, hex("#1a1716"), 0.5);
        cam.face(x, { { RL - 1.5, 0, 2 }, { RL, 0, 2 }, { RL, 0, ZF }, { RL - 1.5, 0, ZF } }, gravel);
        cam.face(x, { { RR, 0, 2 }, { 1.2, 0, 2 }, { 1.2, 0, ZF }, { RR, 0, ZF } }, gravel);
        cam.face(x, { { RL, 0, 2 }, { RR, 0, 2 }, { RR, 0, ZF }, { RL, 0, ZF } }, vgrad(HY, kSceneH, mix(pal.app, hex("#2a2838"), 0.45), mix(pal.app, hex("#121216"), 0.4)));
        for (double cc : { -5.15, -3.35, -1.85, -0.05 }) cam.face(x, { { cc - 0.35, 0, 2 }, { cc + 0.35, 0, 2 }, { cc + 0.35, 0, ZF }, { cc - 0.35, 0, ZF } }, alpha(hex("#9aa0b8"), 0.035));
        // trincas de piche
        x.setBrush(Qt::NoBrush);
        for (int i = 0; i < 26; ++i) {
            const double Z = 6 + std::pow(rnd(), 1.4) * 90, s = cam.F / Z;
            double px = R(RL + 0.4, RR - 0.4), pz = Z;
            QPainterPath t;
            t.moveTo(cam.P(px, 0, pz));
            for (int k = 0; k < 5; ++k) { px += R(-0.25, 0.25); pz += R(0.3, 0.8); t.lineTo(cam.P(px, 0, pz)); }
            x.setPen(pen(alpha(Qt::black, 0.28), qMax(0.35, qMin(1.4, 0.02 * s))));
            x.drawPath(t);
        }
        x.setPen(Qt::NoPen);
        for (int i = 0; i < 6; ++i) { const double Z = R(6, 60), X = R(RL + 0.5, RR - 1.5); cam.face(x, fY(0, X, X + R(0.8, 1.6), Z, Z + R(1.2, 3)), alpha(Qt::black, 0.16)); }
        for (double ex : { RL + 0.25, RR - 0.3 }) cam.face(x, { { ex, 0, 2 }, { ex + 0.14, 0, 2 }, { ex + 0.14, 0, ZF }, { ex, 0, ZF } }, alpha(hex("#e8e8f0"), 0.42));
        for (double Z = 2; Z < 900; Z += 12) cam.face(x, fY(0, -2.6, -2.45, Z, Z + 3), alpha(hex("#e0b84a"), 0.55));
    }

    void apron(QPainter& x, const Pal& pal) const
    {
        const Cam& cam = m_cam;
        const QColor conc = mix(pal.app, hex("#2a2c32"), 0.42);
        cam.face(x, fY(0, 1.2, 18, 7, 40), conc);
        cam.face(x, fY(0, 0.8, 18, 6.6, 7), mix(conc, Qt::black, 0.3));
        QVector<QLineF> j;
        for (double X = 1.2; X <= 18; X += 4) j << QLineF(cam.P(X, 0, 7), cam.P(X, 0, 40));
        for (double Z = 8; Z <= 40; Z += 4) j << QLineF(cam.P(1.2, 0, Z), cam.P(18, 0, Z));
        x.setPen(pen(alpha(Qt::black, 0.2), 0.7));
        x.drawLines(j);
        x.setPen(Qt::NoPen);
        // manchas de óleo onde os carros param, marcas de pneu
        for (const QPointF& o : { QPointF(3.6, 13.6), QPointF(3.8, 18.4), QPointF(7.3, 14), QPointF(7.0, 18.8), QPointF(3.4, 15.9) })
            for (int k = 0; k < 3; ++k) cam.face(x, fY(0, o.x() - R(0.3, 0.7), o.x() + R(0.3, 0.7), o.y() - R(0.3, 0.8), o.y() + R(0.3, 0.8)), alpha(Qt::black, 0.12));
        x.setPen(pen(alpha(Qt::black, 0.2), 1.4));
        x.setBrush(Qt::NoBrush);
        for (double off : { 0.0, 1.55 }) {
            QPainterPath t;
            for (double tt = 0; tt <= 1.0001; tt += 0.05) {
                const QPointF q = cam.P(1.4 + off + tt * 2.2, 0, 6 + tt * 7 + std::sin(tt * 3) * 0.6);
                if (tt == 0) t.moveTo(q); else t.lineTo(q);
            }
            x.drawPath(t);
        }
        x.setPen(Qt::NoPen);
        // meio-fio entre a estrada e o pátio, com as duas entradas abertas
        for (const auto& zz : { std::make_pair(10.5, 26.0), std::make_pair(32.0, 40.0) }) {
            cam.face(x, fX(1.2, 0, 0.15, zz.first, zz.second), mix(conc, Qt::white, 0.12));
            cam.face(x, fY(0.15, 1.2, 1.45, zz.first, zz.second), mix(conc, Qt::white, 0.2));
        }
        for (double X = 8; X <= 20; X += 2.7) cam.face(x, fY(0.005, X, X + 0.1, 23.5, 27), alpha(hex("#e8e8e8"), 0.28));
    }

    void store(QPainter& x, const Pal& pal, const Ctx& c)
    {
        const Cam& cam = m_cam;
        const double z0 = ST.z0, s = cam.F / z0, X0 = ST.x0, X1 = ST.x1, Y1 = ST.y1;
        const QColor wall = mix(pal.app, hex("#6a5a4c"), 0.42), side = mix(pal.app, hex("#3a322c"), 0.5);
        // lateral virada pra estrada, no escuro (só um vitrô aceso)
        cam.face(x, fX(X0, 0, Y1, z0, ST.z1), vgrad(cam.P(0, Y1, z0).y(), cam.P(0, 0, z0).y(), mix(side, hex("#a8b8b4"), 0.1), mix(side, Qt::black, 0.2)));
        for (int i = 0; i < 14; ++i) { const double zz = R(z0, ST.z1 - 1), yy = R(0.2, Y1 - 0.6); cam.face(x, fX(X0 - 0.001, yy, yy + R(0.2, 0.6), zz, zz + R(0.4, 1.4)), alpha(Qt::black, 0.07)); }
        cam.face(x, fX(X0 - 0.002, 0, 0.25, z0, ST.z1), alpha(Qt::black, 0.25));
        cam.face(x, fX(X0 - 0.002, Y1 - 0.18, Y1, z0, ST.z1), mix(side, Qt::white, 0.12));
        cam.face(x, fX(X0 - 0.003, 0, 2.1, 33.2, 34.2), mix(pal.app, hex("#3a3a40"), 0.5));
        cam.face(x, fX(X0 - 0.004, 0.95, 1.0, 33.3, 33.5), alpha(hex("#c8c8c8"), 0.5));
        cam.face(x, fX(X0 - 0.003, 1.4, 2.1, 30.2, 31.6), alpha(hex("#ffcf8a"), 0.5));
        cam.face(x, fX(X0 - 0.004, 1.74, 1.78, 30.2, 31.6), alpha(Qt::black, 0.4));
        // caçamba de lixo encostada e a escada de marinheiro até o telhado
        cam.face(x, fX(X0 - 1.7, 0, 1.3, 35.5, 37.5), mix(pal.app, hex("#2a4a3a"), 0.55));
        cam.face(x, fZ(35.5, X0 - 1.7, X0 - 0.1, 0, 1.3), mix(pal.app, hex("#1e3a2c"), 0.55));
        cam.face(x, fY(1.3, X0 - 1.75, X0 - 0.05, 35.45, 37.55), mix(pal.app, hex("#14241c"), 0.55));
        {
            QVector<QLineF> ls;
            for (double zz : { 37.6, 38.0 }) ls << QLineF(cam.P(X0 - 0.1, 0.4, zz), cam.P(X0 - 0.1, Y1 + 0.5, zz));
            for (double yy = 0.6; yy < Y1 + 0.4; yy += 0.3) ls << QLineF(cam.P(X0 - 0.1, yy, 37.6), cam.P(X0 - 0.1, yy, 38.0));
            x.setPen(pen(alpha(mix(pal.app, hex("#6a6a70"), 0.5), 0.8), 0.7));
            x.drawLines(ls);
            x.setPen(Qt::NoPen);
        }
        m_sideLamp = cam.P(X0 - 0.2, 2.5, 33.7);
        x.fillRect(QRectF(m_sideLamp.x() - 2, m_sideLamp.y() - 1, 4, 2.5), mix(pal.app, hex("#2a2a30"), 0.5));
        // fachada: reboco recebendo luz da cobertura em cima e da vitrine embaixo
        cam.face(x, fZ(z0, X0, X1, 0, Y1), vgrad(cam.P(0, Y1, z0).y(), cam.P(0, 0, z0).y(), mix(wall, hex("#c8d8d4"), 0.12), mix(wall, Qt::black, 0.25)));
        // vitrine: o interior quente, prateleiras com produtos, luz de teto
        const double gx0 = 8, gx1 = 18, gy0 = 0.15, gy1 = 2.55;
        cam.face(x, fZ(z0, gx0, gx1, gy0, gy1), vgrad(cam.P(0, gy1, z0).y(), cam.P(0, gy0, z0).y(), hex("#fff2cc"), hex("#e8a85a")));
        auto box = [&](double a0, double a1, double b0, double b1, const QColor& col) { x.fillRect(QRectF(cam.P(a0, b1, z0 + 0.6), cam.P(a1, b0, z0 + 0.6)), col); };
        for (double X = 8.3; X < 17.8; X += 0.12) if (rnd() < 0.15) box(X, X + 0.04, 2.3, 2.42, alpha(Qt::white, 0.5));
        static const char* PROD[] = { "#c83a2a", "#2a6ac8", "#e8c84a", "#3a9a5a", "#f0f0f0", "#e87a2a", "#8a3ac8" };
        for (double Y : { 0.55, 1.0, 1.45, 1.9 }) {
            box(10.8, 17.6, Y - 0.04, Y, alpha(hex("#7a5030"), 0.55));
            for (double X = 10.85; X < 17.5; X += R(0.08, 0.16)) box(X, X + 0.07, Y, Y + R(0.18, 0.34), alpha(hex(pick(PROD)), 0.55));
        }
        box(8.2, 10.2, 1.85, 2.3, alpha(hex("#3a2a20"), 0.45));
        static const char* CIG[] = { "#e8e8e8", "#c83a2a", "#d8c070" };
        for (double X = 8.3; X < 10.1; X += 0.09) box(X, X + 0.06, 1.9, 2.2, alpha(hex(pick(CIG)), 0.5));
        // marquise e a faixa luminosa em cima
        cam.face(x, fY(2.75, X0, X1, z0 - 0.9, z0), mix(wall, Qt::black, 0.45));
        cam.face(x, fZ(z0 - 0.9, X0, X1, 2.75, 2.95), mix(wall, Qt::white, 0.15));
        const QPointF l0 = cam.P(8.2, 3.6, z0), l1 = cam.P(17.8, 3.05, z0);
        x.fillRect(QRectF(l0, l1), hex("#c8302a"));
        x.fillRect(QRectF(l0.x(), l0.y(), l1.x() - l0.x(), 1), alpha(Qt::white, 0.25));
        text(x, QStringLiteral("FOOD  ·  ICE  ·  COLD BEER"), (l0.x() + l1.x()) / 2, (l0.y() + l1.y()) / 2 + 0.5, hex("#fff4e4"), fnt(0.4 * s, 800));
        // caixa de gelo e a máquina de refrigerante na calçada
        cam.face(x, fZ(z0 - 0.8, 6.4, 7.9, 0, 1.15), mix(pal.app, hex("#d8e0e8"), 0.75));
        cam.face(x, fX(6.4, 0, 1.15, z0 - 0.8, z0), mix(pal.app, hex("#a8b0b8"), 0.6));
        { const QPointF a = cam.P(7.15, 0.75, z0 - 0.8); text(x, QStringLiteral("ICE"), a.x(), a.y(), hex("#2a6ac8"), fnt(0.32 * s, 800)); }
        cam.face(x, fZ(z0 - 0.7, 18.6, 19.6, 0, 1.85), hex("#b8241e"));
        cam.face(x, fZ(z0 - 0.7, 18.7, 19.5, 1.05, 1.75), hex("#ff5a48"));
        cam.face(x, fZ(z0 - 0.7, 18.75, 19.0, 0.3, 0.95), alpha(Qt::black, 0.5));
        for (double X : { 9.5, 15.0 }) cam.face(x, fZ(32, X, X + 1.6, Y1, Y1 + 0.55), mix(pal.app, hex("#3a3e48"), 0.5));
        // a luz da vitrine caindo no pátio
        x.setCompositionMode(QPainter::CompositionMode_Plus);
        cam.face(x, { { gx0, 0, z0 }, { gx1, 0, z0 }, { gx1 + 1.5, 0, z0 - 6 }, { gx0 - 1.5, 0, z0 - 6 } },
                 vgrad(cam.P(0, 0, z0).y(), cam.P(0, 0, z0 - 6).y(), alpha(hex("#ffb860"), 0.28), alpha(hex("#ffb860"), 0)));
        x.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // por cima do balconista (imagem à parte): balcão, caixilhos, cartazes no vidro, porta
        m_glass = sub(c, QRectF(cam.P(X0, Y1, z0), cam.P(X1, 0, z0)).adjusted(-4, -4, 40, 40), m_glassR);
        QPainter g(&m_glass);
        g.setRenderHint(QPainter::Antialiasing, true);
        g.translate(-m_glassR.topLeft());
        auto gb = [&](double a0, double a1, double b0, double b1, const QColor& col) { g.fillRect(QRectF(cam.P(a0, b1, z0), cam.P(a1, b0, z0)), col); };
        gb(8.2, 10.6, 0.15, 1.05, hex("#6a4a34"));
        gb(8.2, 10.6, 1.0, 1.08, hex("#c8a888"));
        gb(9.6, 9.95, 1.08, 1.32, hex("#2a2a2e"));
        const QColor frame = mix(pal.app, hex("#2a2a30"), 0.5);
        for (double X : { 8.0, 10.5, 12.0, 13.8, 15.5, 18.0 }) gb(X - 0.07, X + 0.07, gy0, gy1, frame);
        gb(gx0, gx1, gy1 - 0.06, gy1 + 0.06, frame);
        gb(gx0, gx1, gy0 - 0.05, gy0 + 0.08, frame);
        gb(12.0, 13.8, 0.95, 1.0, alpha(hex("#c0c0c8"), 0.8));
        gb(10.7, 11.6, 1.3, 2.1, alpha(hex("#2a7ac8"), 0.7));
        gb(16.2, 17.4, 1.5, 2.2, alpha(hex("#e8c84a"), 0.75));
        gb(14.2, 15.2, 0.4, 0.9, alpha(hex("#c8302a"), 0.7));
        { const QPointF a = cam.P(11.15, 1.75, z0); text(g, QStringLiteral("LOTTO"), a.x(), a.y(), alpha(Qt::white, 0.85), fnt(0.22 * s, 800)); }
        { const QPointF a = cam.P(16.8, 1.85, z0); text(g, QStringLiteral("COFFEE"), a.x(), a.y(), alpha(hex("#3a2a10"), 0.9), fnt(0.22 * s, 800)); }
        g.setCompositionMode(QPainter::CompositionMode_Plus);
        g.setPen(Qt::NoPen);
        g.setBrush(alpha(Qt::white, 0.05));
        for (double X : { 9.2, 14.6 }) {
            const QPointF a = cam.P(X, gy1, z0), b = cam.P(X + 0.8, 0, z0);
            QPolygonF q;
            q << a << QPointF(a.x() + 8, a.y()) << QPointF(b.x() + 8, a.y() + 30) << QPointF(b.x(), a.y() + 30);
            g.drawPolygon(q);
        }
    }

    // a cobertura, as ilhas e as bombas (camada da frente, transparente)
    void canopy(QPainter& x, const Pal& pal) const
    {
        const Cam& cam = m_cam;
        const QColor white = mix(pal.app, hex("#eef2f2"), 0.85), red = hex("#c8302a");
        for (const auto& isl : { std::make_pair(8.4, 9.4), std::make_pair(4.6, 5.6) }) {
            const double x0 = isl.first, x1 = isl.second, xc = (x0 + x1) / 2;
            cam.face(x, fX(x0, 0, 0.2, 12, 21), mix(pal.app, hex("#9a9ca0"), 0.6));
            cam.face(x, fZ(12, x0, x1, 0, 0.2), mix(pal.app, hex("#e0b030"), 0.4));
            cam.face(x, fY(0.2, x0, x1, 12, 21), mix(pal.app, hex("#b8bcc0"), 0.6));
            // coisas da ilha, da mais longe pra mais perto
            struct It { int k; double z0, z1; };
            for (const It& it : { It { 3, 20.6, 20.8 }, It { 0, 17.7, 19 }, It { 1, 15.85, 16.25 }, It { 2, 14.8, 15.25 }, It { 0, 13, 14.3 }, It { 3, 12.2, 12.4 } }) {
                if (it.k == 1) {
                    cam.face(x, fX(xc - 0.2, 0.2, C.y0, it.z0, it.z1), mix(pal.app, hex("#d8dcdc"), 0.85));
                    cam.face(x, fZ(it.z0, xc - 0.2, xc + 0.2, 0.2, C.y0), mix(pal.app, hex("#c0c4c4"), 0.8));
                    cam.face(x, fX(xc - 0.2, 0.2, 0.9, it.z0, it.z1), red);
                    cam.face(x, fZ(it.z0, xc - 0.2, xc + 0.2, 0.2, 0.9), mix(red, Qt::black, 0.15));
                } else if (it.k == 2) {
                    cam.face(x, fX(xc - 0.17, 0.2, 1.0, it.z0, it.z1), mix(pal.app, hex("#3a4a40"), 0.6));
                    cam.face(x, fZ(it.z0, xc - 0.17, xc + 0.17, 0.2, 1.0), mix(pal.app, hex("#2a3830"), 0.6));
                    cam.face(x, fY(1.0, xc - 0.17, xc + 0.17, it.z0, it.z1), mix(pal.app, hex("#1a2420"), 0.6));
                } else if (it.k == 3) {
                    cam.face(x, fX(xc - 0.08, 0.2, 1.05, it.z0, it.z1), hex("#e8b828"));
                    cam.face(x, fZ(it.z0, xc - 0.08, xc + 0.08, 0.2, 1.05), hex("#c89818"));
                    cam.face(x, fX(xc - 0.081, 0.75, 0.85, it.z0, it.z1), alpha(Qt::white, 0.8));
                    cam.face(x, fY(1.05, xc - 0.08, xc + 0.08, it.z0, it.z1), hex("#f0d060"));
                } else pump(x, pal, xc, it.z0, it.z1);
            }
        }
        // forro: painéis com a grade fininha (as luminárias acendem por cima, a cada quadro)
        cam.face(x, fY(C.y0, C.x0, C.x1, C.z0, C.z1), vgrad(cam.P(0, C.y0, C.z0).y(), cam.P(0, C.y0, C.z1).y(), mix(pal.app, hex("#c4ccca"), 0.85), mix(pal.app, hex("#9aa4a2"), 0.8)));
        {
            QVector<QLineF> ls;
            for (double X = C.x0; X <= C.x1 + 1e-6; X += 1.2) ls << QLineF(cam.P(X, C.y0, C.z0), cam.P(X, C.y0, C.z1));
            for (double Z = C.z0; Z <= C.z1 + 1e-6; Z += 1.4) ls << QLineF(cam.P(C.x0, C.y0, Z), cam.P(C.x1, C.y0, Z));
            x.setPen(pen(alpha(Qt::black, 0.1), 0.6));
            x.drawLines(ls);
            x.setPen(Qt::NoPen);
        }
        for (const QPointF& l : m_lights) cam.face(x, fY(C.y0 - 0.001, l.x() - 0.32, l.x() + 0.32, l.y() - 0.7, l.y() + 0.7), mix(pal.app, hex("#6a7270"), 0.6));
        // testeira: lateral virada pra estrada e a frente, iluminadas por dentro, faixa vermelha
        auto fascia = [&](const std::function<Pts(double, double)>& pts, const QColor& base) {
            cam.face(x, pts(C.y0, C.y1), base);
            cam.face(x, pts(4.82, 5.14), red);
            cam.face(x, pts(4.74, 4.78), mix(red, Qt::black, 0.4));
            cam.face(x, pts(C.y1 - 0.04, C.y1), alpha(Qt::white, 0.5));
            cam.face(x, pts(C.y0, C.y0 + 0.05), alpha(Qt::black, 0.3));
        };
        fascia([&](double a, double b) { return fX(C.x0, a, b, C.z0, C.z1); }, mix(white, hex("#9aa0a8"), 0.22));
        fascia([&](double a, double b) { return fZ(C.z0, C.x0, C.x1, a, b); }, white);
        // o nome na faixa vermelha da frente
        const QPointF n = cam.P(C.x0 + 0.35, 4.98, C.z0);
        const double s = cam.F / C.z0;
        text(x, QStringLiteral("LAST EXIT"), n.x(), n.y() + 0.5, Qt::white, fnt(0.27 * s, 900, 2, true), -1);
        text(x, QStringLiteral("GAS · DIESEL · FOOD"), n.x() + 0.27 * s * 6.2, n.y() + 0.5, alpha(Qt::white, 0.85), fnt(0.2 * s, 700), -1);
    }

    void pump(QPainter& x, const Pal& pal, double xc, double z0, double z1) const
    {
        const Cam& cam = m_cam;
        const double X0 = xc - 0.3, X1 = xc + 0.3;
        const QColor dark = mix(pal.app, hex("#1e2226"), 0.4);
        cam.face(x, fX(X0, 0.2, 2.0, z0, z1), mix(pal.app, hex("#dde2e2"), 0.85));
        cam.face(x, fZ(z0, X0, X1, 0.2, 2.0), mix(pal.app, hex("#aeb4b6"), 0.8));
        cam.face(x, fX(X0, 0.2, 0.5, z0, z1), mix(pal.app, hex("#6a7074"), 0.6));
        cam.face(x, fZ(z0, X0, X1, 0.2, 0.5), mix(pal.app, hex("#55595c"), 0.6));
        // testeira da bomba: vermelha acesa por dentro
        cam.face(x, fX(X0 - 0.05, 2.0, 2.35, z0 - 0.05, z1 + 0.05), hex("#d8382e"));
        cam.face(x, fZ(z0 - 0.05, X0 - 0.05, X1 + 0.05, 2.0, 2.35), hex("#b02a22"));
        cam.face(x, fX(X0 - 0.051, 2.13, 2.2, z0 - 0.05, z1 + 0.05), alpha(Qt::white, 0.8));
        cam.face(x, fY(2.35, X0 - 0.05, X1 + 0.05, z0 - 0.05, z1 + 0.05), hex("#8a1a14"));
        // visor, botões dos combustíveis, leitor do cartão
        cam.face(x, fX(X0 - 0.002, 1.42, 1.78, z0 + 0.12, z0 + 0.72), dark);
        cam.face(x, fX(X0 - 0.003, 1.5, 1.7, z0 + 0.18, z0 + 0.66), hex("#2a5a40"));
        cam.face(x, fX(X0 - 0.004, 1.62, 1.67, z0 + 0.24, z0 + 0.55), alpha(hex("#8affc0"), 0.85));
        cam.face(x, fX(X0 - 0.004, 1.53, 1.57, z0 + 0.36, z0 + 0.6), alpha(hex("#8affc0"), 0.6));
        static const char* GR[] = { "#3aa84a", "#e8b828", "#2a6ac8" };
        for (int k = 0; k < 3; ++k) cam.face(x, fX(X0 - 0.003, 1.2, 1.3, z0 + 0.16 + k * 0.2, z0 + 0.3 + k * 0.2), hex(GR[k]));
        cam.face(x, fX(X0 - 0.003, 0.95, 1.12, z0 + 0.16, z0 + 0.42), dark);
        // bicos no coldre e a mangueira fazendo a barriga
        for (const auto& hh : { std::make_pair(z1 - 0.18, 1.12), std::make_pair(z1 - 0.42, 1.0) }) {
            const double hz = hh.first, hy = hh.second;
            cam.face(x, fX(X0 - 0.01, hy - 0.12, hy + 0.1, hz - 0.1, hz + 0.08), hex("#14161a"));
            const QPointF a = cam.P(X0 - 0.05, 1.9, z1 + 0.02), b = cam.P(X0 - 0.12, 0.55, z1 + 0.15), e = cam.P(X0 - 0.06, hy - 0.1, hz);
            QPainterPath h;
            h.moveTo(a);
            h.quadTo(b.x(), b.y() + 6, e.x(), e.y());
            x.setPen(pen(hex("#0c0d10"), qMax(0.8, 0.04 * cam.F / z0)));
            x.setBrush(Qt::NoBrush);
            x.drawPath(h);
            x.setPen(Qt::NoPen);
        }
    }

    // o letreiro (plano Z = 9, de frente pra gente): sapata, poste, preços e a caixa GAS · FOOD
    void signNear(QPainter& x, const Pal& pal) const
    {
        const Cam& cam = m_cam;
        const QColor steel = mix(pal.app, hex("#2a2c34"), 0.5);
        cam.face(x, fX(1.0, 0, 0.5, 8.7, 9.3), mix(pal.app, hex("#7a7c80"), 0.5));
        cam.face(x, fZ(8.7, 1.0, 1.8, 0, 0.5), mix(pal.app, hex("#9a9ca0"), 0.5));
        cam.face(x, fZ(8.7, 1.0, 1.8, 0.38, 0.5), hex("#d8a820"));
        cam.face(x, fY(0.5, 1.0, 1.8, 8.7, 9.3), mix(pal.app, hex("#b0b2b6"), 0.5));
        // poste: perfil I, com a escadinha de manutenção
        const QPointF p0 = cam.P(1.25, 7.2, SZ), p1 = cam.P(1.55, 0.5, SZ);
        x.fillRect(QRectF(p0, p1), vgrad(p0.y(), p1.y(), mix(steel, hex("#ff6a50"), 0.18), steel));
        x.fillRect(QRectF(p0.x() + (p1.x() - p0.x()) * 0.42, p0.y(), (p1.x() - p0.x()) * 0.16, p1.y() - p0.y()), alpha(Qt::black, 0.35));
        x.fillRect(QRectF(p0.x(), p0.y(), 1.2, p1.y() - p0.y()), alpha(Qt::white, 0.07));
        {
            QVector<QLineF> ls;
            const double lx = p1.x() + 2;
            ls << QLineF(lx, p0.y() + 40, lx, p1.y() - 30) << QLineF(lx + 5, p0.y() + 40, lx + 5, p1.y() - 30);
            for (double yy = p0.y() + 44; yy < p1.y() - 30; yy += 7) ls << QLineF(lx, yy, lx + 5, yy);
            x.setPen(pen(alpha(mix(steel, Qt::white, 0.15), 0.7), 0.8));
            x.drawLines(ls);
            x.setPen(Qt::NoPen);
        }
        auto box = [&](double X0, double X1, double Y0, double Y1, const QColor& col) { x.fillRect(QRectF(QPointF(sgx(X0), sgy(Y1)), QPointF(sgx(X1), sgy(Y0))), col); };
        // placa de preços: fundo preto, rótulos brancos (os dígitos de LED acendem a cada quadro)
        box(0.15, 2.65, 4.95, 6.15, mix(pal.app, hex("#3a3c44"), 0.4));
        box(0.22, 2.58, 5.02, 6.08, hex("#08080a"));
        text(x, QStringLiteral("REGULAR"), sgx(0.32), sgy(5.8), alpha(hex("#e8e8ea"), 0.85), fnt(0.2 * SS(), 700), -1);
        text(x, QStringLiteral("DIESEL"), sgx(0.32), sgy(5.3), alpha(hex("#e8e8ea"), 0.85), fnt(0.2 * SS(), 700), -1);
        x.fillRect(QRectF(sgx(0.22), sgy(5.56), sgx(2.58) - sgx(0.22), 1.5), alpha(Qt::black, 0.5));
        // caixa luminosa GAS · FOOD
        box(0.08, 2.72, 6.2, 7.0, mix(pal.app, hex("#3a3c44"), 0.4));
        box(0.14, 2.66, 6.26, 6.94, hex("#f6f0e2"));
        text(x, QStringLiteral("GAS · FOOD"), sgx(1.4), sgy(6.6) + 1, hex("#c8302a"), fnt(0.34 * SS(), 900));
        box(0.14, 2.66, 6.26, 6.32, alpha(Qt::white, 0.6));
        box(1.2, 1.6, 6.15, 6.2, steel);
        box(1.2, 1.6, 7.0, 7.12, steel);
    }

    // o painel de neon (duas versões: inteiro e com o X queimado) e o halo aditivo
    void buildBoard(const Pal& pal, const Ctx& c)
    {
        const QRectF br(QPointF(sgx(-1.2), sgy(11.4)), QPointF(sgx(5.6), sgy(4.9)));
        QRectF r;
        m_boardOn = sub(c, br, r);
        m_boardOff = layer(c, r.width(), r.height());
        m_boardGlow = layer(c, r.width(), r.height());
        m_boardR = r;
        const double SSv = SS();
        // a seta: as duas bordas da chapa curvada e a ponta
        const double wA = 0.24;
        auto edge = [&](double sgn) {
            std::vector<QPointF> pts;
            for (int i = 0; i <= 20; ++i) {
                const double t = i / 20.0;
                const QPointF a = arrowAt(t), a2 = arrowAt(qMin(1.0, t + 0.01)), a1 = arrowAt(qMax(0.0, t - 0.01));
                const double dx = a2.x() - a1.x(), dy = a2.y() - a1.y(), l = std::hypot(dx, dy);
                pts.push_back(QPointF(a.x() - dy / l * wA * sgn, a.y() + dx / l * wA * sgn));
            }
            return pts;
        };
        const std::vector<QPointF> L1 = edge(1), L2 = edge(-1);
        const QPointF tip = arrowAt(1), pre = arrowAt(0.98);
        const double dX = tip.x() - pre.x(), dY = tip.y() - pre.y(), dl = std::hypot(dX, dY), ux = dX / dl, uy = dY / dl;
        const QPointF head[3] = { QPointF(tip.x() - uy * 0.55, tip.y() + ux * 0.55), QPointF(tip.x() + ux * 0.7, tip.y() + uy * 0.7), QPointF(tip.x() + uy * 0.55, tip.y() - ux * 0.55) };
        auto S = [&](const QPointF& q) { return QPointF(sgx(q.x()), sgy(q.y())); };
        m_bulbs.clear();
        for (int i = 1; i < int(L1.size()) - 1; i += 2) m_bulbs.push_back(S(L1[i]) - r.topLeft());
        for (int i = 1; i < int(L2.size()) - 1; i += 2) m_bulbs.push_back(S(L2[i]) - r.topLeft());
        for (int k = 0; k < 3; ++k) {
            const QPointF a = head[k], b = head[(k + 1) % 3];
            for (double t = 0; t < 0.999; t += 0.25) m_bulbs.push_back(S(QPointF(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t)) - r.topLeft());
        }
        const QPointF starC(sgx(3.14), sgy(10.55));
        const double RS = 0.62 * SSv;
        m_starTips.clear();
        for (int k = 0; k < 8; ++k) {
            const double a = -M_PI / 2 + k * M_PI / 4, rr = k % 2 == 0 ? RS : RS * 0.62;
            m_starTips.push_back(QPointF(starC.x() + std::cos(a) * rr, starC.y() + std::sin(a) * rr) - r.topLeft());
        }
        // tubo de neon: camadas de brilho + o tubo + o miolo branco (apagado: tubo escuro)
        auto tube = [](QPainter& x, const QPainterPath& path, const QColor& col, bool on, double lw) {
            if (on) {
                const struct { double w, a; QColor c; } L[5] = { { lw * 9, 0.05, col }, { lw * 5.5, 0.1, col }, { lw * 3, 0.28, col }, { lw * 1.4, 1, mix(col, Qt::white, 0.15) }, { lw * 0.45, 0.95, Qt::white } };
                for (const auto& l : L) x.strokePath(path, pen(alpha(l.c, l.a), l.w));
            } else {
                x.strokePath(path, pen(alpha(mix(col, Qt::black, 0.6), 0.9), lw));
                x.strokePath(path, pen(alpha(Qt::white, 0.1), lw * 0.35));
            }
        };
        for (int v = 0; v < 2; ++v) {
            const bool broken = v == 1;
            QPainter x(broken ? &m_boardOff : &m_boardOn);
            x.setRenderHint(QPainter::Antialiasing, true);
            x.translate(-r.topLeft());
            x.setPen(Qt::NoPen);
            // mastro da estrela
            x.fillRect(QRectF(sgx(3.08), sgy(10.25), 0.12 * SSv, sgy(9.4) - sgy(10.25)), mix(pal.app, hex("#2a2c34"), 0.5));
            // chapa: moldura de metal, esmalte verde-água, sombra da espessura embaixo
            QPolygonF Q;
            for (const QPointF& q : { QPointF(-0.55, 7.15), QPointF(3.2, 7.15), QPointF(3.75, 9.4), QPointF(0.0, 9.4) }) Q << S(q);
            x.setBrush(mix(pal.app, hex("#1a1c22"), 0.4));
            x.drawPolygon(Q.translated(0, 4));
            x.setPen(QPen(mix(pal.app, hex("#4a4c54"), 0.4), 7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            x.setBrush(Qt::NoBrush);
            x.drawPolygon(Q);
            x.setPen(Qt::NoPen);
            QLinearGradient g(0, Q[3].y(), 0, Q[0].y());
            g.setColorAt(0, hex("#16605e"));
            g.setColorAt(1, hex("#0c3c40"));
            x.setBrush(g);
            x.drawPolygon(Q);
            // a seta
            QPolygonF arrow;
            for (const QPointF& q : L1) arrow << S(q);
            for (auto it = L2.rbegin(); it != L2.rend(); ++it) arrow << S(*it);
            x.setBrush(hex("#6a1c18"));
            x.drawPolygon(arrow);
            QPolygonF hd;
            for (const QPointF& q : head) hd << S(q);
            x.drawPolygon(hd);
            x.setPen(pen(alpha(Qt::black, 0.35), 1));
            x.setBrush(Qt::NoBrush);
            x.drawPolygon(hd);
            x.setPen(Qt::NoPen);
            // contorno creme por dentro da chapa
            QPolygonF inner;
            const double ix[4] = { 8, -8, -8, 8 }, iy[4] = { -7, -7, 7, 7 };
            for (int i = 0; i < 4; ++i) inner << QPointF(Q[i].x() + ix[i], Q[i].y() + iy[i]);
            QPainterPath ip;
            ip.addPolygon(inner);
            ip.closeSubpath();
            tube(x, ip, hex("#fff0c8"), true, 2);
            // LAST / EXIT em neon vermelho (o X queimado fica escuro)
            const QFont f = fnt(0.82 * SSv, 900, 2, true);
            const QFontMetricsF fm(f);
            auto word = [&](const QString& txt, double X, double Y, QChar off) {
                double cx = sgx(X);
                const double base = sgy(Y) + (fm.ascent() - fm.descent()) / 2;
                for (const QChar ch : txt) {
                    const bool on = !(broken && ch == off);
                    QPainterPath gp;
                    gp.addText(QPointF(cx, base), f, QString(ch));
                    if (on) x.fillPath(gp, alpha(hex("#ff3a2c"), 0.35));
                    tube(x, gp, hex("#ff3a2c"), on, 1.6);
                    cx += fm.horizontalAdvance(ch) + 1;
                }
            };
            word(QStringLiteral("LAST"), 0.55, 8.72, QChar());
            word(QStringLiteral("EXIT"), 0.32, 7.78, QLatin1Char('X'));
            // estrela de neon amarela
            QPainterPath sp;
            for (int k = 0; k < 16; ++k) {
                const double a = -M_PI / 2 + k * M_PI / 8, rr = k % 4 == 0 ? RS : k % 2 == 0 ? RS * 0.62 : RS * 0.22;
                const QPointF q(starC.x() + std::cos(a) * rr, starC.y() + std::sin(a) * rr);
                if (k == 0) sp.moveTo(q); else sp.lineTo(q);
            }
            sp.closeSubpath();
            x.fillPath(sp, alpha(hex("#5a4a10"), 0.8));
            tube(x, sp, hex("#ffd84a"), true, 1.6);
        }
        QPainter gx(&m_boardGlow);
        gx.translate(-r.topLeft());
        glowAt(gx, sgx(1.6), sgy(8.3), 2.8 * SSv, hex("#ff3a2c"), 0.22);
        glowAt(gx, sgx(3.14), sgy(10.55), 1.3 * SSv, hex("#ffd84a"), 0.2);
    }

    // um veículo andando em Z (vindo do horizonte ou indo embora)
    static bool visTop(const Cam& cam, const Pts& q, int dir)
    {
        const Pts cq = cam.clip(q);
        if (cq.size() < 3) return false;
        double a = 0;
        for (size_t i = 0; i < cq.size(); ++i) {
            const QPointF p1 = cam.P(cq[i].x, cq[i].y, cq[i].z), p2 = cam.P(cq[(i + 1) % cq.size()].x, cq[(i + 1) % cq.size()].y, cq[(i + 1) % cq.size()].z);
            a += p1.x() * p2.y() - p2.x() * p1.y();
        }
        return dir < 0 ? a < 0 : a > 0;
    }
    void drawBody(QPainter& p, const Veh& v, const Pal& pal) const
    {
        const Cam& cam = m_cam;
        const Kind& K = kindOf(v.kind);
        const QColor base = mix(v.col, Qt::black, 0.55);
        const double lightK = qBound(0.0, 1 - std::abs(v.z + K.L / 2 - 17) / 14, 1.0);   // perto da cobertura pega a luz fria
        const QColor sideC = mix(mix(base, pal.app, 0.3), hex("#a8c8c4"), 0.28 * lightK), topC = mix(mix(v.col, Qt::black, 0.35), hex("#8a9ab8"), 0.18 + 0.2 * lightK);
        const QColor glassC = mix(hex("#0a0e18"), hex("#6a7a9a"), 0.12 + 0.25 * lightK);
        std::vector<const Part*> parts;
        for (const Part& pt : K.parts) parts.push_back(&pt);
        if (v.dir < 0) std::reverse(parts.begin(), parts.end());
        for (const Part* part : parts) {
            const double w = part->box ? 1.3 : K.w / 2, pl = v.X - w, pr = v.X + w;
            const auto& prof = part->prof;
            const size_t n = prof.size();
            auto Zd = [&](double d) { return vz(v, part->d0 + d); };
            // lado direito (se a câmera estiver à direita dele)
            if (pr < 0) {
                Pts side;
                for (const ProfPt& q : prof) side.push_back({ pr, q.Y, Zd(q.d) });
                cam.face(p, side, part->box ? mix(mix(v.trailer, Qt::black, 0.5), hex("#a8c8c4"), 0.25 * lightK) : sideC);
                if (!part->box) {
                    Pts win;
                    for (const QPointF& q : K.win) win.push_back({ pr + 0.002, q.y(), vz(v, q.x()) });
                    cam.face(p, win, glassC);
                } else {
                    QVector<QLineF> ls;
                    for (double d = 1; d < 15; d += 0.8) ls << QLineF(cam.P(pr, 1.25, Zd(d)), cam.P(pr, 4.05, Zd(d)));
                    p.setPen(pen(alpha(Qt::black, 0.25), 0.7));
                    p.drawLines(ls);
                    p.setPen(Qt::NoPen);
                }
            }
            // tampas de cima e para-brisas, de longe pra perto
            struct Seg { double z; Pts q; bool g; };
            std::vector<Seg> segs;
            for (size_t i = 1; i + 2 < n; ++i) {
                const double z1 = Zd(prof[i].d), z2 = Zd(prof[i + 1].d);
                segs.push_back({ (z1 + z2) / 2, { { pl + 0.05, prof[i].Y, z1 }, { pr - 0.05, prof[i].Y, z1 }, { pr - 0.05, prof[i + 1].Y, z2 }, { pl + 0.05, prof[i + 1].Y, z2 } }, prof[i].g });
            }
            std::sort(segs.begin(), segs.end(), [](const Seg& a, const Seg& b) { return a.z > b.z; });
            for (const Seg& s : segs) if (visTop(cam, s.q, v.dir)) cam.face(p, s.q, s.g ? glassC : part->box ? mix(v.trailer, Qt::black, 0.4) : topC);
            // a ponta mais perto (frente ou traseira)
            const double dEnd = v.dir < 0 ? prof[0].d : prof[n - 1].d, yTop = v.dir < 0 ? prof[1].Y : prof[n - 2].Y, yBot = v.dir < 0 ? prof[0].Y : prof[n - 1].Y, ze = Zd(dEnd);
            if (part->box) {
                if (v.dir > 0) {
                    cam.face(p, fZ(ze + 1.6, pl + 0.1, pr - 0.1, 0.05, 1.2), hex("#050506"));
                    for (double u : { pl + 0.12, pr - 0.62 }) { cam.face(p, fZ(ze + 1.0, u, u + 0.5, 0.02, 1.0), hex("#0a0a0c")); cam.face(p, fZ(ze + 1.0, u + 0.05, u + 0.45, 0.45, 0.5), alpha(hex("#6a6a70"), 0.5)); }
                    for (double u : { pl + 0.15, pr - 0.6 }) cam.face(p, fZ(ze + 0.5, u, u + 0.45, 0.25, 0.8), hex("#060607"));
                    cam.face(p, fZ(ze, pl, pr, 1.2, 4.1), mix(v.trailer, Qt::black, 0.45));
                    cam.face(p, fZ(ze - 0.01, pl, pr, 1.2, 1.32), mix(v.trailer, Qt::black, 0.65));
                    if (ze > cam.ZN) {
                        p.setPen(pen(alpha(Qt::black, 0.45), 1));
                        p.drawLine(cam.P(v.X, 4.05, ze), cam.P(v.X, 1.25, ze));
                        QVector<QLineF> rods;
                        for (double u : { -0.75, -0.35, 0.35, 0.75 }) rods << QLineF(cam.P(v.X + u, 3.9, ze), cam.P(v.X + u, 1.4, ze));
                        p.setPen(pen(alpha(hex("#c8c8d0"), 0.35), qMax(0.6, 0.04 * cam.F / ze)));
                        p.drawLines(rods);
                        p.setPen(Qt::NoPen);
                    }
                    for (int k = 0; k < 8; ++k) cam.face(p, fZ(ze - 0.3, pl + 0.25 + k * (K.w - 0.5) / 8, pl + 0.25 + (k + 1) * (K.w - 0.5) / 8, 0.42, 0.56), k % 2 ? hex("#e8e8e8") : hex("#c8201a"));
                    for (int k = 0; k < 10; ++k) cam.face(p, fZ(ze - 0.02, pl + 0.05 + k * (K.w - 0.1) / 10, pl + 0.05 + (k + 0.5) * (K.w - 0.1) / 10, 1.22, 1.3), k % 2 ? hex("#e8e8e8") : hex("#c8201a"));
                }
            } else if (v.dir < 0 && v.kind == Truck) {
                cam.face(p, fZ(ze, pl + 0.05, pr - 0.05, yBot, yTop), mix(base, Qt::black, 0.2));
                cam.face(p, fZ(ze - 0.01, v.X - 0.55, v.X + 0.55, 0.75, yTop + 0.45), mix(pal.app, hex("#9aa0a8"), 0.4));
                cam.face(p, fZ(ze - 0.02, v.X - 0.48, v.X + 0.48, 0.8, yTop + 0.4), hex("#14161a"));
                if (ze > cam.ZN) {
                    QVector<QLineF> gr;
                    for (double u = -0.4; u <= 0.41; u += 0.1) gr << QLineF(cam.P(v.X + u, yTop + 0.38, ze), cam.P(v.X + u, 0.84, ze));
                    p.setPen(pen(alpha(hex("#b8c0c8"), 0.6), qMax(0.5, 0.03 * cam.F / ze)));
                    p.drawLines(gr);
                    p.setPen(Qt::NoPen);
                }
                cam.face(p, fZ(ze - 0.05, pl - 0.05, pr + 0.05, 0.42, 0.68), mix(pal.app, hex("#c8ccd0"), 0.35));
                cam.face(p, fZ(ze - 0.06, pl - 0.05, pr + 0.05, 0.6, 0.64), alpha(Qt::white, 0.4));
            } else if (v.dir < 0 || K.parts.size() == 1) {
                cam.face(p, fZ(ze, pl + 0.02, pr - 0.02, yBot, yTop), mix(base, Qt::black, 0.2));
                cam.face(p, fZ(ze - 0.02, pl, pr, yBot, yBot + 0.18), mix(base, Qt::black, 0.5));
                if (v.dir > 0) cam.face(p, fZ(ze - 0.03, v.X - 0.26, v.X + 0.26, yBot + 0.22, yBot + 0.38), alpha(hex("#e8e4d0"), 0.75));
                else cam.face(p, fZ(ze - 0.03, v.X - 0.45, v.X + 0.45, yBot + 0.2, yTop - 0.12), alpha(Qt::black, 0.45));
            }
        }
        // rodas
        if (v.X + K.w / 2 < 0) {
            p.setBrush(hex("#060608"));
            for (double d : K.wheels) {
                Pts ring;
                for (int k = 0; k <= 12; ++k) { const double a = k / 12.0 * 6.28; ring.push_back({ v.X + K.w / 2 + 0.01, K.wr + std::sin(a) * K.wr, vz(v, d) + std::cos(a) * K.wr }); }
                cam.face(p, ring, hex("#060608"));
            }
        }
    }

    void drawLights(QPainter& p, const Veh& v) const
    {
        const Cam& cam = m_cam;
        const Kind& K = kindOf(v.kind);
        auto dot = [&](double X, double Y, double Z, const QColor& col, const QColor& core, double r) {
            if (Z < cam.ZN + 0.2) return;
            const QPointF q = cam.P(X, Y, Z);
            const double s = cam.F / Z, cs = qMax(0.9, 0.12 * s);
            glowAt(p, q.x(), q.y(), qMax(2.5, r * s), col, 0.55);
            p.fillRect(QRectF(q.x() - cs / 2, q.y() - cs / 2, cs, cs), core);
        };
        if (v.dir < 0) {
            const double ze = v.z, fade = qMin(1.0, 80 / qMax(80.0, ze)), zA = ze, zB = qMax(cam.ZN + 0.3, ze - 38);
            // facho no asfalto vindo na nossa direção
            if (zA > cam.ZN + 0.5) {
                const QPointF a = cam.P(v.X, 0, zA), b = cam.P(v.X, 0, zB);
                for (const auto& w : { std::array<double, 3> { 0.8, 4.2, 0.05 }, std::array<double, 3> { 0.7, 2.6, 0.08 }, std::array<double, 3> { 0.55, 1.4, 0.1 } }) {
                    QLinearGradient g(a, b);
                    g.setColorAt(0, alpha(hex("#fff2d0"), w[2] * (0.4 + 0.6 * fade)));
                    g.setColorAt(1, alpha(hex("#fff2d0"), 0));
                    cam.face(p, { { v.X - w[0], 0, zA }, { v.X + w[0], 0, zA }, { v.X + w[1], 0, zB }, { v.X - w[1], 0, zB } }, g);
                }
            }
            for (const QPointF& h : K.head) dot(v.X + h.x(), h.y(), ze, hex("#fff4dc"), Qt::white, 0.5);
            for (double u : { -K.w / 2 + 0.05, K.w / 2 - 0.05 }) dot(v.X + u, K.head[0].y() + 0.05, ze + 0.05, hex("#ffa020"), alpha(hex("#ffc060"), 0.9), 0.12);
            if (v.kind == Truck) {
                for (int k = -2; k <= 2; ++k) dot(v.X + k * 0.22, 3.58, ze + 3.6, hex("#ffa020"), hex("#ffd080"), 0.1);
                for (double u : { -1.25, 1.25 }) dot(v.X + u, 4.1, ze + 5.8, hex("#ffa020"), hex("#ffd080"), 0.1);
                if (v.X + 1.25 < 0) for (double d = 7; d < 21; d += 3.4) dot(v.X + 1.26, 1.35, ze + d, hex("#ffa020"), hex("#ffc060"), 0.08);
            }
            // brilho dos faróis quando chegam perto
            if (ze < 60 && ze > 2) {
                const QPointF q = cam.P(v.X, K.head[0].y(), ze);
                const double k = 1 - ze / 60;
                p.fillRect(QRectF(q.x() - 70 * k, q.y() - 0.5, 140 * k, 1), alpha(hex("#fff6e0"), 0.08 * k));
                glowAt(p, q.x(), q.y(), 18 + 60 * k, hex("#fff4dc"), 0.2 * k);
            }
        } else {
            const double ze = v.z, zA = ze + K.L, zB = zA + 45;
            if (zA > cam.ZN) {
                const QPointF a = cam.P(v.X, 0, zA), b = cam.P(v.X, 0, zB);
                for (const auto& w : { std::make_pair(3.6, 0.05), std::make_pair(1.8, 0.08) }) {
                    QLinearGradient g(a, b);
                    g.setColorAt(0, alpha(hex("#fff2d0"), w.second));
                    g.setColorAt(1, alpha(hex("#fff2d0"), 0));
                    cam.face(p, { { v.X - 0.8, 0, zA }, { v.X + 0.8, 0, zA }, { v.X + w.first, 0, zB }, { v.X - w.first, 0, zB } }, g);
                }
            }
            if (ze > cam.ZN + 0.3) {
                const QPointF q = cam.P(v.X, 0, ze);
                p.save();
                p.translate(q);
                p.scale(1, 0.25);
                glowAt(p, 0, 0, 1.8 * cam.F / ze, hex("#ff2010"), 0.18);
                p.restore();
            }
            for (const QPointF& t : K.tail) dot(v.X + t.x(), t.y(), ze, hex("#ff2010"), hex("#ff6050"), 0.3);
            if (v.kind == Truck) {
                for (double u : { -1.2, 1.2 }) dot(v.X + u, 4.05, ze, hex("#ff2010"), hex("#ff6050"), 0.1);
                if (v.X + 1.25 < 0) for (double d = 1; d < 15; d += 3.4) dot(v.X + 1.26, 1.35, ze + d, hex("#ffa020"), hex("#ffc060"), 0.08);
            }
        }
    }

    Cam m_cam;
    QString m_key;
    QPixmap m_back, m_far, m_near, m_glass, m_air, m_boardOn, m_boardOff, m_boardGlow, m_tumble;
    QRectF m_nearR, m_glassR, m_airR, m_boardR;
    std::vector<QPointF> m_lights, m_bulbs, m_starTips, m_towers;
    std::vector<Twinkle> m_tw;
    std::vector<Moth> m_moths;
    std::vector<Veh> m_veh;
    QPointF m_sideLamp, m_bill;
    Weed m_weed;
    Star m_star;
    Sat m_sat;
    bool m_first = true;
    double m_t = 0, m_flick = 0, m_signOff = 0, m_openFlick = 0, m_nextVeh = 6, m_nextWeed = 14, m_nextStar = 9, m_nextSat = 30;
    double m_clerkX = 9.3, m_clerkTo = 9.3, m_clerkWait = 4;
};

} // namespace

std::unique_ptr<Layer> makeDesert(const QString& id)
{
    if (id == QLatin1String("lastexit")) return std::make_unique<LastExit>();
    if (id == QLatin1String("route66")) return std::make_unique<Route66>();
    return nullptr;
}

} // namespace AnimBg
