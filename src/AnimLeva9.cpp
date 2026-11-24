// Temas Animados da leva 9 (22/11/2026, filmes e jogos) e o Optimus Write (Transformers), portados do Weather Lab
// (anim-lab/leva9.js e shapeshift.js) linha a linha pela camada Cv. Os nomes só lembram as obras: nada de marca,
// personagem ou arte oficial.
//   echelon     pátio industrial de madrugada na garoa, holofotes varrendo, três pontinhos verdes no escuro
//   bullettime  Nova York na nevasca; de tempos em tempos o tempo quase para e uma bala atravessa a cena
//   shapeshift  a mesa é uma chapa de aço escovado que se desmonta e mostra o mecanismo de baixo
// As cenas foram desenhadas numa mesa de 1440 × 758: o que mora do lado esquerdo segue a coluna da esquerda (zx), o
// que mora do lado direito segue a borda direita (rx). Os botões do Lab ("Visão noturna", "Bullet time agora",
// "Transformar agora") eram só pra testar e não vêm pro app: o bullet time e a transformação acontecem sozinhos.

#include "AnimCanvas.h"
#include "MetalTexture.h"

#include <QImage>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>

namespace AnimBg {
namespace {

using namespace cv;

constexpr double kH = kSceneH;

QColor mixh(const QColor& a, const char* b, double k) { return mix(a, hex(b), k); }
QColor mixh(const char* a, const char* b, double k) { return mix(hex(a), hex(b), k); }

// bloco 2D: frente (x, y, w, h) + tampo e lateral puxando os cantos uma fração fr do caminho até o ponto de fuga (vx, vy)
void block(Cv& x, double bx, double by, double w, double h, double fr, double vx, double vy, const QColor& cFront, const QColor& cTop, const QColor& cSide)
{
    auto P = [&](double px, double py) { return QPointF(px + (vx - px) * fr, py + (vy - py) * fr); };
    const QPointF tl(bx, by), tr(bx + w, by), bl(bx, by + h), br(bx + w, by + h);
    const int side = bx + w / 2 < vx ? 1 : -1;
    {
        const QPointF a = side > 0 ? tr : tl, b = side > 0 ? br : bl, a2 = P(a.x(), a.y()), b2 = P(b.x(), b.y());
        x.fillStyle(cSide); x.begin(); x.moveTo(a.x(), a.y()); x.lineTo(a2.x(), a2.y()); x.lineTo(b2.x(), b2.y()); x.lineTo(b.x(), b.y()); x.close(); x.fill();
    }
    if (by > vy) {
        const QPointF a2 = P(tl.x(), tl.y()), b2 = P(tr.x(), tr.y());
        x.fillStyle(cTop); x.begin(); x.moveTo(tl.x(), tl.y()); x.lineTo(a2.x(), a2.y()); x.lineTo(b2.x(), b2.y()); x.lineTo(tr.x(), tr.y()); x.close(); x.fill();
    }
    x.fillStyle(cFront); x.fillRect(bx, by, w, h);
}

// um ponto da cena está num pedaço de mesa à vista?
bool visibleX(const Ctx& c, double px)
{
    const QRectF& l = c.zones.left; const QRectF& r = c.zones.right;
    if (l.width() < 120 && r.width() < 120) return true;   // prévia sem colunas: tudo vale
    return (l.width() >= 60 && px >= l.left() - 10 && px <= l.right() + 10) || (r.width() >= 60 && px >= r.left() - 10 && px <= r.right() + 10);
}

/* ===================================================================================================================
   Echelon: o pátio à noite, os holofotes e os três pontinhos verdes
   =================================================================================================================== */
class Echelon : public Layer {
public:
    void init(Ctx& c) override
    {
        m_key.clear();
        m_drops.assign(size_t(qMax(1, int(std::lround(320 * INT(c) * c.W / 1440.0)))), Drop());
        for (Drop& d : m_drops) spawnDrop(c, d, true);
        m_splash.clear();
        m_guard = false; m_guardWait = 7;
        m_g = Gog();
    }

    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        layout(c);
        m_nvLeft -= dt;
        m_nv += ((m_nvLeft > 0 ? 1.0 : 0.0) - m_nv) * qMin(1.0, dt * 5);
        if (m_nvLeft <= 0 && m_nv < 0.004) m_nv = 0;
        m_spike *= std::exp(-dt * 5);
        for (Drop& d : m_drops) { d.y += d.v * dt; d.x -= d.v * dt * 0.12; if (d.y > kH + 20) spawnDrop(c, d, false); }
        for (Splash& s : m_splash) s.age += dt;
        m_splash.erase(std::remove_if(m_splash.begin(), m_splash.end(), [](const Splash& s) { return s.age >= 0.28; }), m_splash.end());
        for (const Tower& tw : m_tw) {
            const Beam bm = beamAt(tw);
            for (int k = 0; k < 3; ++k) { const double a = R(0, 6.28), r = std::sqrt(rnd()); m_splash.push_back({ bm.gx + std::cos(a) * r * bm.rx, bm.gy + std::sin(a) * r * bm.ry, 0 }); }
        }
        if (!m_guard) { m_guardWait -= dt; if (m_guardWait < 0) { m_gdir = rnd() < 0.5 ? 1 : -1; m_gx = m_gdir > 0 ? -40 : c.W + 40; m_gph = 0; m_guard = true; } }
        else { m_gx += m_gdir * 30 * dt; m_gph += dt * 5.2; if (m_gx < -60 || m_gx > c.W + 60) { m_guard = false; m_guardWait = R(22, 40); } }
        // os três pontinhos verdes: acendem num canto escuro à vista, olham em volta e somem; com o facho perto, apagam na hora
        Gog& g = m_g;
        if (g.st == Gog::Off) {
            g.t -= dt;
            if (g.t < 0) {
                std::vector<int> ok;
                for (int i = 0; i < int(m_pos.size()); ++i) if (visibleX(c, m_pos[i].x)) ok.push_back(i);
                g.i = ok.empty() ? 0 : ok[RI(int(ok.size()))];
                g.st = Gog::In; g.t = R(7, 13); g.ox = g.oy = g.tx = g.ty = 0;
            }
        } else if (g.st == Gog::In) { g.a = qMin(1.0, g.a + dt * 1.3); if (g.a >= 1) g.st = Gog::Hold; }
        else if (g.st == Gog::Hold) { g.t -= dt; if (g.t < 0) g.st = Gog::Out; }
        else if (g.st == Gog::Out) { g.a -= dt * 1.1; if (g.a <= 0) { g.a = 0; g.st = Gog::Off; g.t = R(5, 11); } }
        else if (g.st == Gog::Hide) { g.a -= dt * 4.5; if (g.a <= 0) { g.a = 0; g.st = Gog::Off; g.t = R(3, 7); } }
        if ((g.st == Gog::In || g.st == Gog::Hold) && !m_pos.empty()) {
            const Pos& P = m_pos[size_t(g.i)];
            for (const Tower& tw : m_tw) { const Beam bm = beamAt(tw); if (std::abs(bm.gx - P.x) < bm.rx + 80 && (P.inv || std::abs(bm.gy - P.y) < 130)) g.st = Gog::Hide; }
        }
        g.nt -= dt; if (g.nt < 0) { g.tx = R(-5, 5); g.ty = R(-2, 2); g.nt = R(1.1, 2.8); }
        g.ox += (g.tx - g.ox) * qMin(1.0, dt * 2.5); g.oy += (g.ty - g.oy) * qMin(1.0, dt * 2.5);
    }

    void paint(QPainter& painter, const Pal& pal, Ctx& c) override
    {
        layout(c);
        const QString key = QString::number(m_ox, 'f', 1) + QLatin1Char('|') + QString::number(c.W, 'f', 0) + QLatin1Char('|') + QString::number(c.px, 'f', 2) + pal.app.name();
        if (key != m_key) { build(c, pal); m_key = key; }
        if (m_nv < 0.004) { scene(painter, pal, c); return; }
        // visão noturna (ao trocar de capítulo/documento, 2 s): a cena num quadro à parte, que vira o verde do óculos
        const QSize sz(qMax(1, int(std::ceil(c.W * c.px))), qMax(1, int(std::ceil(kH * c.px))));
        if (m_nvBuf.size() != sz) { m_nvBuf = QImage(sz, QImage::Format_ARGB32_Premultiplied); m_nvImg = QImage(sz, QImage::Format_RGB32); }
        m_nvBuf.setDevicePixelRatio(c.px); m_nvImg.setDevicePixelRatio(c.px);
        m_nvBuf.fill(Qt::black);
        { QPainter bp(&m_nvBuf); scene(bp, pal, c); }
        const double bright = 5.2 + m_spike * 4;
        int lut[256];
        for (int i = 0; i < 256; ++i) lut[i] = qBound(0, int(((i * bright) - 128) * 1.08 + 128), 255);
        for (int yy = 0; yy < sz.height(); ++yy) {
            const QRgb* s = reinterpret_cast<const QRgb*>(m_nvBuf.constScanLine(yy));
            QRgb* d = reinterpret_cast<QRgb*>(m_nvImg.scanLine(yy));
            for (int i = 0; i < sz.width(); ++i) {
                const int v = lut[qBound(0, (qRed(s[i]) * 54 + qGreen(s[i]) * 183 + qBlue(s[i]) * 19) >> 8, 255)];
                d[i] = qRgb(v * 0x62 / 255, v, v * 0x7e / 255);
            }
        }
        painter.drawImage(QRectF(0, 0, c.W, kH), m_nvBuf);
        painter.save();
        painter.setOpacity(m_nv);
        painter.drawImage(QRectF(0, 0, c.W, kH), m_nvImg);
        QRadialGradient vg(QPointF(c.W / 2, kH / 2), c.W * 0.7, QPointF(c.W / 2, kH / 2), kH * 0.45);
        vg.setColorAt(0, QColor(0, 10, 0, 0)); vg.setColorAt(1, QColor(0, 10, 0, 128));
        painter.fillRect(QRectF(0, 0, c.W, kH), vg);
        painter.restore();
    }

    void event(const QString& name) override
    {
        if (name == QLatin1String("document")) { m_nvLeft = 2.0; m_spike = 1; }
    }

private:
    QImage m_nvBuf, m_nvImg;
    double m_nv = 0, m_spike = 0, m_nvLeft = 0;

    void scene(QPainter& painter, const Pal& pal, Ctx& c)
    {
        const double T = m_t;
        painter.save();
        Cv x(painter);
        x.image(m_back, 0, 0);
        x.lighter();
        for (const Red& r : m_reds) {
            const double tt = std::fmod(T + r.ph, 2.4), on = tt < 1.1 ? 1 - (tt / 1.1) * 0.4 : 0.1;
            glowAt(x, r.x, r.y, 14, "#ff3030", 0.5 * on); x.fillStyle(rgba("#ff6050", on)); x.fillRect(r.x - 1.5, r.y - 1.5, 3, 3);
        }
        for (const Lamp& l : m_lamps) {
            glowAt(x, l.x, l.y, l.k == 1 ? 18 : 26, "#cfe4ff", 0.35); x.fillStyle(rgba("#eef6ff", 0.9)); x.fillRect(l.x - 2, l.y - 1, 4, 2);
        }
        {   // a luz da guarita, falhando de vez em quando
            const double gx = m_rx(1100) + 12;
            const double fl = std::sin(T * 0.7) > 0.93 ? (rnd() < 0.5 ? 0.15 : 1) : 1;
            x.fillStyle(rgba("#cfe4ff", 0.42 * fl)); x.fillRect(gx, 574, 34, 20); glowAt(x, gx + 17, 584, 60, "#cfe4ff", 0.08 * fl);
        }
        x.over();
        if (m_guard) {
            const double gy = 566, gh = 42, hx = m_gx + m_gdir * 7, hy = gy - gh * 0.52;
            x.lighter();
            x.fillStyle(QBrush(lin(hx, hy, hx + m_gdir * 130, gy + 8, { { 0, rgba("#fff2d8", 0.28) }, { 1, rgba("#fff2d8", 0.02) } })));
            x.begin(); x.moveTo(hx, hy); x.lineTo(hx + m_gdir * 140, gy - 2); x.lineTo(hx + m_gdir * 120, gy + 14); x.close(); x.fill();
            x.save(); x.translate(hx + m_gdir * 128, gy + 6); x.scale(1, 0.25); glowAt(x, 0, 0, 30, "#fff2d8", 0.25); x.restore();
            x.over();
            x.fillStyle(rgba("#000000", 0.3)); x.begin(); x.ellipse(m_gx, gy, 10, 2, 0, 0, TAU); x.fill();
            Fig f; f.pose = Fig::Walk; f.ph = m_gph; f.col = mixh(pal.app, "#030405", 0.4); f.hair = Fig::Cap; f.side = m_gdir; f.build = 1.12;
            figure(painter, m_gx, gy, gh, f);
        }
        std::vector<std::pair<const Tower*, Beam>> beams;
        for (const Tower& tw : m_tw) beams.push_back({ &tw, beamAt(tw) });
        x.lighter();
        for (const auto& [tw, bm] : beams) {
            x.fillStyle(QBrush(lin(tw->lx, tw->ly, bm.gx, bm.gy, { { 0, rgba(BEAM, 0.22) }, { 1, rgba(BEAM, 0.05) } })));
            x.begin(); addBeam(x, *tw, bm, 1); x.fill();
            x.fillStyle(QBrush(lin(tw->lx, tw->ly, bm.gx, bm.gy, { { 0, rgba(BEAM, 0.07) }, { 1, rgba(BEAM, 0.0) } })));
            x.begin(); addBeam(x, *tw, bm, 1.9); x.fill();
            x.save(); x.translate(bm.gx, bm.gy); x.scale(1, bm.ry / bm.rx); glowAt(x, 0, 0, bm.rx * 1.25, BEAM, 0.42); x.restore();
            glowAt(x, tw->lx, tw->ly, 46, BEAM, 0.55); glowAt(x, tw->lx, tw->ly, 10, hex("#ffffff"), 0.9);
        }
        // a chuva só aparece forte dentro do facho
        QVector<QLineF> lines;
        lines.reserve(int(m_drops.size()));
        for (const Drop& d : m_drops) lines.append(QLineF(d.x, d.y, d.x + d.l * 0.12, d.y - d.l));
        x.save();
        x.begin(); for (const auto& [tw, bm] : beams) addBeam(x, *tw, bm, 1); x.clip();
        painter.setPen(QPen(rgba("#e8f0ff", 0.42), 1)); painter.drawLines(lines);
        x.restore();
        x.strokeStyle(rgba("#e8f0ff", 0.4)); x.lineWidth(0.8); x.begin();
        for (const Splash& s : m_splash) { const double k = s.age / 0.28, r = 1 + k * 4; x.moveTo(s.x + r, s.y); x.ellipse(s.x, s.y, r, r * 0.3, 0, 0, TAU); }
        x.stroke();
        x.over();
        x.image(m_front, 0, 0);
        {   // o facho da torre da esquerda passando pelos caixotes acende a quina de cima
            const Beam bm = beams[0].second;
            x.lighter();
            for (const QRectF& cr : m_crates) {
                const double kk = qMax(0.0, 1 - std::abs(bm.gx - (cr.x() + cr.width() / 2)) / 230) * qMax(0.0, 1 - std::abs(bm.gy - cr.y()) / 200);
                if (kk > 0.01) { x.fillStyle(rgba(BEAM, 0.3 * kk)); x.fillRect(cr.x(), cr.y(), cr.width(), 1.5); glowAt(x, cr.x() + cr.width() / 2, cr.y(), cr.width() * 0.7, BEAM, 0.08 * kk); }
            }
            x.over();
        }
        painter.setPen(QPen(rgba("#9fb4c8", 0.07), 1)); painter.drawLines(lines);
        {   // a câmera da torre virando, com o LED vermelho
            const double ca = 0.55 + 0.4 * std::sin(T * 0.28);
            x.save(); x.translate(m_lx(124), 240); x.fillStyle(mixh(pal.app, "#080b0f", 0.5)); x.fillRect(-1.5, -8, 3, 8); x.rotate(ca);
            x.fillRect(-4, -4, 20, 8); x.fillStyle(rgba(MOON, 0.3)); x.fillRect(-4, -4, 20, 1); x.fillStyle(mixh(pal.app, "#080b0f", 0.5)); x.fillRect(14, -3, 4, 6);
            if (std::fmod(T, 1.3) < 0.14) { x.lighter(); glowAt(x, 2, -1, 7, "#ff2a2a", 0.8); x.fillStyle(hex("#ff6a5a")); x.fillRect(1, -2, 2, 2); }
            x.restore();
        }
        if (m_g.a > 0.004 && !m_pos.empty()) {
            const Pos& P = m_pos[size_t(m_g.i)];
            const double s = 5.4, sg = P.inv ? -1 : 1, cx = P.x + m_g.ox, cy = P.y + m_g.oy * sg, a = m_g.a;
            x.lighter(); glowAt(x, cx, cy, 48, GOG, 0.07 * a);
            const double pts[3][2] = { { 0, -0.75 }, { -1, 0.42 }, { 1, 0.42 } };
            for (const auto& q : pts) {
                const double px = cx + q[0] * s, py = cy + q[1] * s * sg;
                glowAt(x, px, py, 10, GOG, 0.55 * a); x.fillStyle(rgba("#e6ffd6", a)); x.begin(); x.arc(px, py, 1.7, 0, TAU); x.fill();
            }
            x.over();
        }
        painter.restore();
    }

private:
    struct Tower { double bx; int dir; double lx, ly, foot, a, b, nearY, farY, sp, ph; };
    struct Beam { double gx, gy, rx, ry; };
    struct Drop { double x = 0, y = 0, v = 0, l = 0; };
    struct Splash { double x, y, age; };
    struct Red { double x, y, ph; };
    struct Lamp { double x, y; int k; };
    struct Pos { double x, y; bool inv; };
    struct Gog { enum St { Off, In, Hold, Out, Hide } st = Off; int i = 0; double a = 0, t = 2.5, ox = 0, oy = 0, tx = 0, ty = 0, nt = 0; };

    static constexpr double BASE = 500, VY = 440;
    const QColor GOG = hex("#7dff4a"), BEAM = hex("#dfe8ff"), MOON = hex("#9ab4d4");

    double m_lx(double labX) const { return labX + m_ox; }
    double m_rx(double labX) const { return labX - 1440 + m_w; }

    void layout(const Ctx& c)
    {
        m_ox = zx(c, 45) - 45; m_w = c.W;
        const double vx = c.W / 2;
        m_vx = vx;
        m_tw = { { m_lx(104), 1, m_lx(141), 213, 640, m_lx(150), m_rx(1150), 735, 590, 0.15, 0.6 },
                 { m_rx(1337), -1, m_rx(1300), 213, 630, m_lx(260), m_rx(1330), 722, 590, 0.12, 2.6 } };
        m_crates = { QRectF(m_lx(56), 646, 124, 112), QRectF(m_lx(168), 692, 104, 66), QRectF(m_lx(84), 604, 82, 42) };
        m_pos = { { m_lx(236), 660, false }, { m_lx(290), 168, true }, { m_rx(1196), 666, false } };
    }

    Beam beamAt(const Tower& tw) const
    {
        const double u = 0.5 + 0.5 * std::sin(m_t * tw.sp + tw.ph), gx = tw.a + (tw.b - tw.a) * u, d = qMin(1.0, std::abs(gx - tw.lx) / 900);
        const double gy = lerp(tw.nearY, tw.farY, d), rx = 96 + 56 * (1 - d);
        return { gx, gy, rx, rx * 0.3 };
    }
    static void addBeam(Cv& x, const Tower& tw, const Beam& bm, double f)
    {
        x.moveTo(tw.lx - 6 * f, tw.ly - 2); x.lineTo(bm.gx - bm.rx * f, bm.gy);
        x.ellipse(bm.gx, bm.gy, bm.rx * f, bm.ry * f, 0, M_PI, 0, true);
        x.lineTo(tw.lx + 6 * f, tw.ly + 2); x.close();
    }
    void spawnDrop(const Ctx& c, Drop& d, bool any)
    {
        d.x = R(-60, c.W + 80); d.y = any ? R(-20, kH) : R(-80, -10); d.v = R(640, 900); d.l = R(9, 18);
    }

    // a instalação em três fileiras (receita do depthCity), cada prédio de um tipo
    void plant(Cv& x, const Ctx& c, const Pal& p)
    {
        const double W = c.W, VX = m_vx;
        const QColor hz = mixh(p.app, "#34465a", 0.6);
        auto hazeOver = [&](double a0, double a1) { x.fillStyle(QBrush(lin(0, 150, 0, BASE, { { 0, rgba(hz, a0) }, { 1, rgba(hz, a1) } }))); x.fillRect(0, 150, W, BASE + 30 - 150); };
        const double darkK[3] = { 0.0, 0.2, 0.4 }, greyK[3] = { 0.18, 0.24, 0.28 };
        for (int k = 0; k < 3; ++k) {
            const QColor col = mix(mixh(p.app, "#000000", darkK[k]), hex("#3e4a58"), greyK[k]);
            struct B { int type; double x, w, y, h, tone, lit; int n; };   // 0 shed, 1 block, 2 silo, 3 tank, 4 stack
            std::vector<B> row;
            double bx = R(-40, 0);
            while (bx < W) {
                const double r = rnd();
                const int type = r < 0.38 ? 0 : r < 0.6 ? 1 : r < 0.78 ? 2 : r < 0.9 ? 3 : 4;
                double bw = k == 0 ? R(60, 130) : k == 1 ? R(70, 160) : R(100, 210);
                double bh = k == 0 ? R(70, 190) : k == 1 ? R(80, 220) : R(90, 200);
                if (type == 2) { bw = k == 0 ? R(22, 34) : k == 1 ? R(28, 42) : R(36, 54); bh *= 1.5; }
                if (type == 3) { bw = k == 0 ? R(50, 80) : k == 1 ? R(60, 100) : R(80, 130); bh *= 0.45; }
                if (type == 4) { bw = k == 0 ? 8 : k == 1 ? 11 : 14; bh = k == 0 ? R(170, 260) : k == 1 ? R(200, 300) : R(240, 330); }
                const int n = type == 2 ? 1 + int(std::floor(R(0, 3))) : 1;
                const double yoff = k == 0 ? -24 : k == 1 ? -10 : 4;
                row.push_back({ type, bx, bw, BASE - bh + yoff, bh, R(-0.1, 0.12), R(0.3, 1.3), n });
                bx += bw * (type == 2 ? n * 1.06 : 1) + (k == 0 ? R(0, 10) : k == 1 ? R(4, 22) : R(10, 40));
            }
            std::sort(row.begin(), row.end(), [VX](const B& a, const B& b) { return std::abs(a.x + a.w / 2 - VX) > std::abs(b.x + b.w / 2 - VX); });
            for (const B& b : row) {
                const QColor base = mix(col, hex(b.tone > 0 ? "#8a9aac" : "#000000"), std::abs(b.tone));
                const double cx = b.x + b.w / 2, bot = BASE + 26;
                const int side = cx < VX ? 1 : -1;
                const QColor topC = mix(base, MOON, 0.1 + 0.04 * k), botC = mixh(base, "#000000", 0.45);
                if (b.type == 2 || b.type == 3) {
                    // cilindros: luz da lua pela esquerda, sombra à direita, tampo em domo/elipse
                    for (int i = 0; i < b.n; ++i) {
                        const double sx = b.x + i * b.w * 1.06;
                        x.fillStyle(QBrush(lin(sx, 0, sx + b.w, 0, { { 0, mix(base, MOON, 0.22) }, { 0.35, base }, { 1, mixh(base, "#000000", 0.6) } })));
                        x.fillRect(sx, b.y, b.w, bot - b.y);
                        x.fillStyle(mix(base, MOON, 0.16)); x.begin(); x.ellipse(sx + b.w / 2, b.y, b.w / 2, b.type == 2 ? b.w * 0.32 : b.w * 0.12, 0, M_PI, 0); x.fill();
                        x.strokeStyle(rgba("#000000", 0.25)); x.lineWidth(1);
                        for (double yy = b.y + 14; yy < bot; yy += b.type == 2 ? 16 : 9) { x.begin(); x.ellipse(sx + b.w / 2, yy, b.w / 2, 2, 0, 0, M_PI); x.stroke(); }
                        x.fillStyle(QBrush(vgrad(b.y, bot, rgba("#000000", 0), rgba("#000000", 0.45)))); x.fillRect(sx, b.y, b.w, bot - b.y);
                        if (b.type == 2 && k > 0) { x.strokeStyle(rgba("#000000", 0.45)); x.begin(); x.moveTo(sx + b.w * 0.15, bot); x.lineTo(sx + b.w * 0.85, b.y + 6); x.stroke(); }
                    }
                    if (b.type == 2 && b.n > 1 && k > 0) {
                        const double tw = b.w * 1.06 * b.n;
                        x.fillStyle(mixh(base, "#000000", 0.25)); x.fillRect(b.x, b.y - 16, tw - b.w * 0.06, 8); x.fillStyle(rgba(MOON, 0.15)); x.fillRect(b.x, b.y - 16, tw - b.w * 0.06, 1);
                    }
                    continue;
                }
                if (b.type == 4) {
                    x.fillStyle(QBrush(lin(b.x, 0, b.x + b.w, 0, { { 0, mix(base, MOON, 0.25) }, { 1, mixh(base, "#000000", 0.55) } })));
                    x.begin(); x.moveTo(b.x - b.w * 0.25, bot); x.lineTo(b.x, b.y); x.lineTo(b.x + b.w, b.y); x.lineTo(b.x + b.w * 1.25, bot); x.fill();
                    for (int i = 0; i < 2; ++i) { x.fillStyle(rgba("#7a2a24", 0.35)); x.fillRect(b.x, b.y + 6 + i * 16, b.w, 7); }
                    m_reds.push_back({ cx, b.y - 2, R(0, 2.4) });
                    continue;
                }
                // galpão / bloco: parede lateral fugindo pro meio, fachada com degradê
                const double sw = qMin(6.0 + 10 * k, std::abs(cx - VX) * (0.012 + 0.009 * k));
                if (sw > 1.5) {
                    const double ex = side > 0 ? b.x + b.w : b.x, ox = ex + side * sw, drop = sw * 0.4;
                    x.fillStyle(QBrush(vgrad(b.y, bot, mixh(base, "#000000", 0.35), mixh(base, "#000000", 0.75))));
                    x.begin(); x.moveTo(ex, b.y); x.lineTo(ox, b.y + drop); x.lineTo(ox, bot); x.lineTo(ex, bot); x.close(); x.fill();
                    x.fillStyle(rgba(MOON, 0.08 + 0.03 * k)); x.fillRect(side > 0 ? ex - 1 : ex, b.y, 1, bot - b.y);
                }
                x.fillStyle(QBrush(lin(0, b.y, 0, bot, { { 0, topC }, { 0.2, base }, { 1, botC } }))); x.fillRect(b.x, b.y, b.w, bot - b.y);
                if (b.type == 0) {
                    // telhado dente-de-serra: os vidros virados pra lua pegam um reflexo frio
                    const int n = qMax(2, int(std::lround(b.w / (k == 0 ? 16 : k == 1 ? 20 : 26))));
                    const double tw = b.w / n, th = k == 0 ? 7 : k == 1 ? 10 : 14;
                    for (int i = 0; i < n; ++i) {
                        const double tx = b.x + i * tw;
                        x.fillStyle(topC); x.begin(); x.moveTo(tx, b.y); x.lineTo(tx + tw, b.y); x.lineTo(tx + tw, b.y - th); x.close(); x.fill();
                        x.fillStyle(rgba(MOON, 0.12 + 0.05 * k)); x.begin(); x.moveTo(tx + tw, b.y); x.lineTo(tx + tw, b.y - th); x.lineTo(tx + tw - 1.5, b.y - th + 2); x.lineTo(tx + tw - 1.5, b.y); x.fill();
                    }
                    // portas de doca, uma ou outra com a luminária acesa em cima
                    if (k > 0) {
                        const double stepX = k == 1 ? 30 : 40, dw = k == 1 ? 18 : 26, dh = k == 1 ? 22 : 34;
                        for (double dx = b.x + 10; dx < b.x + b.w - 26; dx += stepX) {
                            x.fillStyle(mixh(base, "#000000", 0.5)); x.fillRect(dx, bot - 26 - dh, dw, dh);
                            x.fillStyle(rgba("#000000", 0.25)); for (double yy = bot - 26 - dh + 4; yy < bot - 26; yy += 4) x.fillRect(dx, yy, dw, 1);
                            if (rnd() < 0.5) m_lamps.push_back({ dx + dw / 2, bot - 26 - dh - 5, k });
                        }
                    }
                } else {
                    // bloco de escritório: janelas frias em grade, poucas acesas
                    const double s = k == 0 ? 7 : k == 1 ? 9 : 12, ww = k == 0 ? 2 : k == 1 ? 3.5 : 5, wh = k == 0 ? 2.5 : k == 1 ? 4 : 5.5;
                    for (double wy = b.y + 8; wy < BASE - 8; wy += s) for (double wx = b.x + 5; wx < b.x + b.w - 5; wx += s) {
                        const bool on = rnd() < 0.17 * b.lit;
                        x.fillStyle(on ? rgba(rnd() < 0.8 ? "#bcd6f0" : "#ffb868", 0.55) : rgba("#000000", 0.28)); x.fillRect(wx, wy, ww, wh);
                    }
                    x.fillStyle(mixh(base, "#000000", 0.2)); if (rnd() < 0.5) x.fillRect(b.x + b.w * 0.25, b.y - 10, b.w * 0.3, 10);
                    if (k > 0 && rnd() < 0.4) { x.fillRect(cx, b.y - 34, 2, 34); m_reds.push_back({ cx + 1, b.y - 35, R(0, 2.4) }); }
                }
                x.fillStyle(mix(base, MOON, 0.22)); x.fillRect(b.x, b.y, b.w, 1.5);
                for (int e = 0; e < 2; ++e) {
                    const double ex = e == 0 ? b.x : b.x + b.w; const int dir = e == 0 ? 1 : -1;
                    const double aw = 8 + 5 * k;
                    x.fillStyle(QBrush(lin(ex, 0, ex + dir * aw, 0, { { 0, rgba("#000000", 0.3 + 0.1 * k) }, { 1, rgba("#000000", 0) } })));
                    x.fillRect(qMin(ex, ex + dir * aw), b.y, aw, bot - b.y);
                }
            }
            // esteira de canos ligando os prédios da fileira do meio
            if (k == 1) {
                const double py = BASE - 46;
                x.fillStyle(mixh(col, "#000000", 0.35)); x.fillRect(0, py, W, 4); x.fillRect(0, py + 7, W, 3); x.fillStyle(rgba(MOON, 0.14)); x.fillRect(0, py, W, 1);
                for (double px = 30; px < W; px += 70) { x.fillStyle(mixh(col, "#000000", 0.45)); x.fillRect(px, py, 3, BASE + 26 - py); }
            }
            if (k == 0) hazeOver(0.14, 0.55);
            if (k == 1) hazeOver(0.05, 0.3);
        }
    }

    // torre de vigia com volume: perna iluminada pela lua, perna na sombra, cabine em bloco
    void tower(Cv& x, const Tower& tw, const Pal& p)
    {
        const double bx = tw.bx, top = 230, foot = tw.foot;
        const int dir = tw.dir;
        const QColor c = mixh(p.app, "#1a222c", 0.5), lit = mix(c, MOON, 0.42), shd = mixh(c, "#000000", 0.55);
        auto half = [&](double y) { return 13 + (y - top) / (foot - top) * 13; };
        x.roundCaps();
        x.lineWidth(2); x.strokeStyle(mixh(shd, "#000000", 0.3)); x.begin(); x.moveTo(bx + dir * 4, foot - 14); x.lineTo(bx + dir * 2, top); x.stroke();
        x.lineWidth(3.2); x.strokeStyle(lit); x.begin(); x.moveTo(bx - half(foot), foot); x.lineTo(bx - 13, top); x.stroke();
        x.strokeStyle(shd); x.begin(); x.moveTo(bx + half(foot), foot); x.lineTo(bx + 13, top); x.stroke();
        x.lineWidth(1.2); x.strokeStyle(c);
        for (double y = top + 8; y < foot - 8; y += 46) {
            const double a = half(y), y2 = qMin(foot, y + 46), b2 = half(y2);
            x.begin(); x.moveTo(bx - a, y); x.lineTo(bx + b2, y2); x.moveTo(bx + a, y); x.lineTo(bx - b2, y2); x.moveTo(bx - a, y); x.lineTo(bx + a, y); x.stroke();
        }
        x.strokeStyle(rgba(MOON, 0.25)); x.lineWidth(0.8); x.begin();
        for (double y = top + 10; y < foot - 4; y += 8) { const double a = half(y); x.moveTo(bx - a + 1, y); x.lineTo(bx - a + 6, y); }
        x.stroke();
        // plataforma: a laje vista de baixo, guarda-corpo, cabine em bloco com o telhado
        x.fillStyle(shd); x.begin(); x.moveTo(bx - 42, 224); x.lineTo(bx + 42, 224); x.lineTo(bx + 38, 232); x.lineTo(bx - 38, 232); x.fill();
        x.fillStyle(lit); x.fillRect(bx - 42, 222, 84, 2);
        x.strokeStyle(c); x.lineWidth(1); x.begin(); x.moveTo(bx - 42, 204); x.lineTo(bx + 42, 204);
        for (double k = -42; k <= 42; k += 10.5) { x.moveTo(bx + k, 204); x.lineTo(bx + k, 222); }
        x.stroke();
        x.strokeStyle(rgba(MOON, 0.3)); x.begin(); x.moveTo(bx - 42, 204); x.lineTo(bx, 204); x.stroke();
        const double ex = bx + (dir > 0 ? 30 : -30), sw = dir > 0 ? 9 : -9;
        x.fillStyle(shd); x.begin(); x.moveTo(ex, 184); x.lineTo(ex + sw, 187); x.lineTo(ex + sw, 204); x.lineTo(ex, 204); x.fill();
        x.fillStyle(QBrush(lin(bx - 30, 0, bx + 30, 0, { { 0, lit }, { 0.3, c }, { 1, mixh(c, "#000000", 0.3) } }))); x.fillRect(bx - 30, 184, 60, 20);
        x.fillStyle(rgba("#1a1210", 0.95)); x.fillRect(bx - 22, 189, 44, 9); x.fillStyle(rgba("#ff8a50", 0.07)); x.fillRect(bx - 22, 189, 44, 9);
        x.fillStyle(shd); x.begin(); x.moveTo(bx - 48, 184); x.lineTo(bx + 48, 184); x.lineTo(bx + 38, 170); x.lineTo(bx - 38, 170); x.fill();
        x.fillStyle(rgba(MOON, 0.35)); x.begin(); x.moveTo(bx - 48, 184); x.lineTo(bx - 38, 170); x.lineTo(bx + 38, 170); x.lineTo(bx + 37, 171.5); x.lineTo(bx - 37, 171.5); x.lineTo(bx - 46.5, 184); x.fill();
        // o holofote
        x.fillStyle(shd); x.begin(); x.arc(tw.lx, tw.ly, 8, 0, TAU); x.fill(); x.fillStyle(lit); x.begin(); x.arc(tw.lx - 2, tw.ly - 2, 3, 0, TAU); x.fill();
        x.fillStyle(c); x.fillRect(tw.lx - 2, tw.ly + 6, 4, 18);
        x.fillStyle(rgba("#000000", 0.35)); x.begin(); x.ellipse(bx, foot + 2, 40, 6, 0, 0, TAU); x.fill();
    }

    void build(const Ctx& c, const Pal& p)
    {
        Seeded sd(0xEC4E1013u);
        m_reds.clear(); m_lamps.clear();
        const double W = c.W, VX = m_vx;
        m_back = layer(c, W, kH);
        {
            QPainter qp(&m_back);
            Cv x(qp);
            x.fillStyle(QBrush(lin(0, 0, 0, BASE, { { 0, mixh(p.app, "#05070c", 0.4) }, { 0.6, mixh(p.app, "#142030", 0.25) }, { 1, mixh(p.app, "#26354a", 0.2) } })));
            x.fillRect(0, 0, W, BASE + 30);
            const double mx = m_lx(236);
            glowAt(x, mx, 96, 300, "#8fa8cc", 0.16); x.fillStyle(rgba("#c8d6ea", 0.55)); x.begin(); x.arc(mx, 96, 15, 0, TAU); x.fill();
            {
                const QPixmap cd = softDot(mixh(p.app, "#0c121c", 0.35), 0.4), cl = softDot(hex("#7f96b8"), 0.3);
                for (int i = 0; i < int(46 * W / 1440.0) + 4; ++i) {
                    const double cx = R(-100, W + 100), cy = R(20, 300), w = R(140, 380), h = w * R(0.16, 0.3);
                    x.alphaG(R(0.5, 0.92)); x.image(cd, cx - w / 2, cy - h / 2, w, h);
                }
                for (int i = 0; i < 9; ++i) { const double cx = mx + R(-200, 200), cy = 96 + R(-20, 60), w = R(80, 200); x.alphaG(R(0.12, 0.26)); x.image(cl, cx - w / 2, cy - w * 0.1, w, w * 0.2); }
                x.alphaG(1);
            }
            plant(x, c, p);
            // o pátio: concreto molhado, mais escuro perto; as faixas pintadas fogem pro ponto de fuga
            x.fillStyle(QBrush(vgrad(BASE + 20, kH, mixh(p.app, "#121a24", 0.45), mixh(p.app, "#030405", 0.5)))); x.fillRect(0, BASE + 22, W, kH - BASE);
            x.strokeStyle(rgba("#c8b060", 0.07)); x.lineWidth(2);
            for (double f : { -0.625, -0.208, 0.18, 0.82, 1.21, 1.625 }) { const double sx = f * W; x.begin(); x.moveTo(VX + (sx - VX) * 0.06, BASE + 30); x.lineTo(sx, kH + 20); x.stroke(); }
            for (int i = 0; i < 6; ++i) { const double yy = BASE + 40 + std::pow(i / 6.0, 1.8) * 230; x.strokeStyle(rgba("#c8b060", 0.03 + i * 0.008)); x.lineWidth(0.6 + i * 0.5); x.begin(); x.moveTo(0, yy); x.lineTo(W, yy); x.stroke(); }
            for (int i = 0; i < int(18 * W / 1440.0) + 2; ++i) {
                const double py = R(BASE + 50, kH - 10), d = (py - BASE) / (kH - BASE), px = R(0, W), rx = R(20, 70) * (0.4 + d);
                x.fillStyle(rgba("#000000", 0.3)); x.begin(); x.ellipse(px, py, rx, rx * 0.15, 0, 0, TAU); x.fill();
                x.fillStyle(rgba("#5a7290", 0.06)); x.begin(); x.ellipse(px, py - 0.5, rx * 0.8, rx * 0.06, 0, 0, TAU); x.fill();
            }
            // a cerca do perímetro, na base das fileiras
            const double fy0 = BASE - 4, fy1 = BASE + 28;
            const QColor fc = mixh(p.app, "#1c2632", 0.5);
            x.strokeStyle(rgba("#8aa0b8", 0.025)); x.lineWidth(0.6); x.begin();
            for (double k = -40; k < W + 40; k += 7) { x.moveTo(k, fy1); x.lineTo(k + 32, fy0); x.moveTo(k, fy0); x.lineTo(k + 32, fy1); }
            x.stroke();
            x.strokeStyle(fc); x.lineWidth(1.3); x.begin();
            for (double k = 0; k < W; k += 46) { x.moveTo(k, fy1 + 2); x.lineTo(k, fy0 - 6); x.lineTo(k + 5, fy0 - 10); }
            x.moveTo(0, fy0); x.lineTo(W, fy0); x.stroke();
            x.strokeStyle(rgba("#8aa0b8", 0.14)); x.lineWidth(0.8); x.begin(); x.moveTo(0, fy0 - 7); x.lineTo(W, fy0 - 7); x.moveTo(0, fy0 - 10); x.lineTo(W, fy0 - 10); x.stroke();
            // a guarita do lado direito, em bloco
            const double gx = m_rx(1100);
            block(x, gx, 560, 80, 58, 0.06, VX, VY, mixh(p.app, "#0e141b", 0.5), mixh(p.app, "#1a2430", 0.5), mixh(p.app, "#06090c", 0.5));
            x.fillStyle(mixh(p.app, "#06090c", 0.5)); x.begin(); x.moveTo(gx - 6, 562); x.lineTo(gx + 86, 562); x.lineTo(gx + 80, 550); x.lineTo(gx, 550); x.fill();
            x.fillStyle(rgba("#000000", 0.5)); x.fillRect(gx + 12, 574, 34, 20); x.fillRect(gx + 56, 574, 14, 44);
            for (const Tower& tw : m_tw) tower(x, tw, p);
        }
        // a frente: o cano lá em cima, os caixotes em bloco, os tambores, a caçamba do outro lado
        m_front = layer(c, W, kH);
        {
            QPainter qp(&m_front);
            Cv f(qp);
            const QColor dark = mixh(p.app, "#0a0e13", 0.5), top = mixh(p.app, "#1e2a36", 0.5), sd2 = mixh(p.app, "#040608", 0.5), edge = mixh(p.app, "#1a2430", 0.5);
            {
                const double e = m_lx(453), e2 = m_lx(436), e3 = m_lx(428);
                f.fillStyle(QBrush(lin(0, 126, 0, 145, { { 0, mixh(p.app, "#5a7088", 0.4) }, { 0.25, mixh(p.app, "#1a2430", 0.5) }, { 1, mixh(p.app, "#020304", 0.5) } })));
                f.fillRect(-10, 127, e3 + 24 + 10, 17);
                f.begin(); f.moveTo(e3, 144); f.quadTo(e, 144, e, 118); f.lineTo(e, -10); f.lineTo(e2, -10); f.lineTo(e2, 118); f.quadTo(e2, 127, e3, 127); f.fill();
            }
            for (double lbx : { 70.0, 210.0, 350.0 }) { const double bx = m_lx(lbx); f.fillStyle(dark); f.fillRect(bx - 2, -10, 4, 138); f.fillRect(bx - 8, 123, 16, 6); f.fillStyle(rgba(MOON, 0.3)); f.fillRect(bx - 2, -10, 1, 138); }
            for (double lfx = 30; lfx < 430; lfx += 92) { const double fx2 = m_lx(lfx); f.fillStyle(dark); f.fillRect(fx2, 124, 6, 23); f.fillStyle(rgba(MOON, 0.35)); f.fillRect(fx2, 124, 1, 23); }
            std::vector<QRectF> cr = m_crates;
            std::sort(cr.begin(), cr.end(), [](const QRectF& a, const QRectF& b) { return a.y() < b.y(); });
            for (const QRectF& q : cr) {
                const double cx = q.x(), cy = q.y(), cw = q.width(), ch = q.height();
                block(f, cx, cy, cw, ch, 0.05, VX, VY, dark, top, sd2);
                f.strokeStyle(edge); f.lineWidth(1.6); f.strokeRect(cx + 1, cy + 1, cw - 2, ch - 2);
                f.lineWidth(1); f.begin();
                for (double yy = cy + 14; yy < cy + ch - 4; yy += 14) { f.moveTo(cx + 3, yy); f.lineTo(cx + cw - 3, yy); }
                f.moveTo(cx + 4, cy + 4); f.lineTo(cx + cw - 4, cy + ch - 4); f.stroke();
                f.fillStyle(rgba(MOON, 0.22)); f.fillRect(cx, cy, 1.5, ch);
            }
            f.fillStyle(rgba("#8a9aa8", 0.12)); f.setFont(22, 700, { QStringLiteral("IBM Plex Mono"), QStringLiteral("Consolas") }); f.fillText(QStringLiteral("E-07"), m_lx(82), 716);
            for (const auto& lb : { std::array<double, 2>{ 318, 678 }, std::array<double, 2>{ 356, 692 } }) {
                const double bx = m_lx(lb[0]), by = lb[1];
                f.fillStyle(QBrush(lin(bx - 18, 0, bx + 18, 0, { { 0, mixh(p.app, "#3a4a5c", 0.4) }, { 0.3, dark }, { 1, mixh(p.app, "#020304", 0.5) } })));
                f.fillRect(bx - 18, by, 36, kH - by + 4);
                f.fillStyle(top); f.begin(); f.ellipse(bx, by, 18, 5.5, 0, 0, TAU); f.fill(); f.fillStyle(rgba("#000000", 0.5)); f.begin(); f.ellipse(bx, by, 13, 3.5, 0, 0, TAU); f.fill();
                f.strokeStyle(rgba("#000000", 0.5)); f.lineWidth(1.5); f.begin(); f.moveTo(bx - 18, by + 22); f.lineTo(bx + 18, by + 22); f.moveTo(bx - 18, by + 50); f.lineTo(bx + 18, by + 50); f.stroke();
            }
            const double dx = m_rx(1222);
            block(f, dx, 652, 134, 70, 0.06, VX, VY, dark, top, sd2);
            f.strokeStyle(edge); f.lineWidth(1.4); f.begin(); for (double k = 18; k < 128; k += 22) { f.moveTo(dx + k, 656); f.lineTo(dx + k, 716); } f.stroke();
            f.fillStyle(hex("#000000")); for (double k : { 14.0, 120.0 }) { f.begin(); f.arc(dx + k, 726, 6, 0, TAU); f.fill(); }
            const double tx = m_rx(1384);
            f.fillStyle(dark); f.fillRect(tx, 668, 36, 90); f.fillStyle(top); f.begin(); f.ellipse(tx + 18, 668, 18, 5.5, 0, 0, TAU); f.fill();
        }
    }

    double m_t = 0, m_ox = 0, m_w = 1440, m_vx = 720;
    QString m_key;
    QPixmap m_back, m_front;
    std::vector<Tower> m_tw;
    std::vector<QRectF> m_crates;
    std::vector<Pos> m_pos;
    std::vector<Drop> m_drops;
    std::vector<Splash> m_splash;
    std::vector<Red> m_reds;
    std::vector<Lamp> m_lamps;
    bool m_guard = false;
    double m_guardWait = 7, m_gx = 0, m_gph = 0;
    int m_gdir = 1;
    Gog m_g;
};

/* ===================================================================================================================
   Bullet Time: Nova York na nevasca, e o tempo que para
   =================================================================================================================== */
class BulletTime : public Layer {
public:
    void init(Ctx& c) override
    {
        m_key.clear();
        m_flakes.assign(size_t(qMax(1, int(std::lround(950 * INT(c) * c.W / 1440.0)))), Flake());
        for (Flake& q : m_flakes) spawnF(c, q, true);
        m_steam.clear();
        m_cars.clear();
        const int n = int(std::lround(3 * INT(c))) + 1;
        for (int i = 0; i < n; ++i) m_cars.push_back(newCar(c, 1, true));
        for (int i = 0; i < n; ++i) m_cars.push_back(newCar(c, -1, true));
    }

    void step(double dt, Ctx& c) override
    {
        layout(c);
        if (m_st == Idle) { m_stT -= dt; if (m_stT < 0) { m_st = In; m_tt = 0; } }
        else if (m_st == In) {
            m_tt += dt; const double k = smooth(0, 1, m_tt / 0.45); m_ts = lerp(1, 0.05, k); m_bt = k;
            if (m_tt >= 0.45) { m_st = Hold; m_tt = 0; m_bullet = true; m_bx = m_lx(-24); m_by = 384; m_trail.clear(); m_blast = m_bx; }
        } else if (m_st == Hold) { m_tt += dt; m_ts = 0.05; m_bt = 1; if (m_tt >= 6.2) { m_st = Out; m_tt = 0; } }
        else if (m_st == Out) {
            m_tt += dt; const double k = smooth(0, 1, m_tt / 0.7); m_ts = lerp(0.05, 1, k); m_bt = 1 - k;
            if (m_tt >= 0.7) { m_st = Idle; m_ts = 1; m_bt = 0; m_stT = R(55, 90); }
        }
        const double d = dt * m_ts;
        m_t += d; m_toggle += d;
        if (m_bullet) {
            const double v = 2200 * m_ts;
            m_bx += v * dt; m_by += v * dt * 0.06;
            while (m_blast + 8 < m_bx) { m_blast += 8; m_trail.push_back(QPointF(m_blast, m_by - (m_bx - m_blast) * 0.06)); }
            if (m_trail.size() > 140) m_trail.erase(m_trail.begin(), m_trail.begin() + int(m_trail.size() - 140));
        }
        if (m_st == Idle && m_bullet && m_bt == 0) m_bullet = false;
        const double w = wind(m_t);
        for (Flake& q : m_flakes) {
            q.y += q.v * d; q.x += (w * q.z * 0.95 + std::sin(m_t * 1.3 + q.ph) * 22 * q.z) * d;
            if (q.y > kH + 6 || q.x > c.W + 60) spawnF(c, q, false);
        }
        for (Car& cr : m_cars) { cr.x += cr.dir * cr.v * d; if (cr.x > c.W + 100 || cr.x < -100) cr = newCar(c, cr.dir, false); }
        m_steamT -= d;
        if (m_steamT < 0) { m_steamT = 0.12; m_steam.push_back({ m_lx(392) + R(-6, 6), ROAD + 40, R(-6, 6), R(-34, -22), R(8, 14), 0, R(3, 4.5) }); }
        for (Steam& s : m_steam) { s.age += d; s.x += (s.vx + w * 0.25) * d; s.y += s.vy * d; s.r += 9 * d; }
        m_steam.erase(std::remove_if(m_steam.begin(), m_steam.end(), [](const Steam& s) { return s.age >= s.life; }), m_steam.end());
        m_neonT -= d; if (m_neonT < 0) { m_neonBuzz = 0.7; m_neonT = R(4, 9); }
        m_neonBuzz = qMax(0.0, m_neonBuzz - d);
    }

    void paint(QPainter& painter, const Pal& pal, Ctx& c) override
    {
        layout(c);
        const QString key = QString::number(m_ox, 'f', 1) + QLatin1Char('|') + QString::number(c.W, 'f', 0) + QLatin1Char('|') + QString::number(c.px, 'f', 2) + pal.app.name();
        if (key != m_key) { build(c, pal); m_key = key; }
        if (m_snowSpr.isNull()) { m_snowSpr = softDot(hex("#eef2f8")); m_amberSpr = softDot(hex("#ffc070")); }
        // janelas acendendo e apagando no tempo da cena (no bullet time quase param): copia da versão acesa ou apagada
        if (m_toggle > 0.3 && !m_wins.empty()) {
            m_toggle = 0;
            QPainter wp(&m_city);
            for (int i = 0; i < 3; ++i) {
                Win& w = m_wins[size_t(RI(int(m_wins.size())))];
                w.on = !w.on;
                copyRect(wp, w.on ? m_cityOn : m_cityOff, w.r);
            }
        }
        if (m_bt > 0.003) {
            // bullet time: a cena num quadro à parte, que depois perde a cor
            if (m_buf.size() != m_city.size()) { m_buf = QImage(m_city.size(), QImage::Format_ARGB32_Premultiplied); m_buf.setDevicePixelRatio(c.px); }
            m_buf.fill(Qt::transparent);
            {
                QPainter bp(&m_buf);
                scene(bp, pal, c);
                sepia(bp, c);
            }
            painter.drawImage(QRectF(0, 0, c.W, kH), m_buf);
            painter.save(); Cv x(painter); drawBullet(x); painter.restore();
        } else {
            scene(painter, pal, c);
        }
    }

private:
    enum St { Idle, In, Hold, Out };
    struct Flake { double z = 1, x = 0, y = 0, v = 0, r = 1, ph = 0; };
    struct Car { int dir; double x, v, lane; QColor col; bool cab; };
    struct Steam { double x, y, vx, vy, r, age, life; };
    struct Win { QRectF r; bool on; };
    struct Ant { double x, y, ph; };

    static constexpr double ROAD = 648, NEAR = 722;
    const QColor AMBER = hex("#ffb24a"), NEON = hex("#ff3b3b"), SNOW = hex("#e8ecf2");

    double m_lx(double labX) const { return labX + m_ox; }
    double m_rx(double labX) const { return labX - 1440 + m_w; }
    void layout(const Ctx& c) { m_ox = zx(c, 45) - 45; m_w = c.W; m_vx = c.W / 2; }
    static double wind(double t) { return 120 + 80 * std::sin(t * 0.21) + 45 * std::sin(t * 0.57 + 1); }
    static QColor warm() { return pickHex({ "#ffcf7a", "#ffd99a", "#ffe7b8", "#f4b860", "#ffc48a" }); }

    void spawnF(const Ctx& c, Flake& q, bool any)
    {
        q.z = std::pow(rnd(), 2.2) * 0.9 + 0.1; q.x = R(-160, c.W + 40); q.y = any ? R(-10, kH) : R(-40, -6);
        q.v = 50 + 130 * q.z; q.r = 0.8 + 3.8 * q.z; q.ph = R(0, 6.28);
    }
    Car newCar(const Ctx& c, int dir, bool any)
    {
        const bool cab = rnd() < 0.28;
        return { dir, any ? R(0, c.W) : (dir > 0 ? -90.0 : c.W + 90), R(55, 95), dir > 0 ? ROAD + 50 : ROAD + 22,
                 cab ? hex("#d8a820") : pickHex({ "#5a1e1e", "#1e2a40", "#a8a8ac", "#141418", "#3a3a40", "#4a3a2a" }), cab };
    }

    // a cidade (receita do depthCity), com neve e o jeito de Nova York. mode: 0 janelas como sorteadas, 1 todas acesas, 2 todas apagadas
    void nyCity(Cv& x, const Ctx& c, const Pal& p, int mode)
    {
        const double W = c.W, VX = m_vx;
        const QColor hz = mixh(p.app, "#7e7880", 0.6), rim = hex("#ffc89a");
        auto hazeOver = [&](double a0, double a1) { x.fillStyle(QBrush(vgrad(0, ROAD, rgba(hz, a0), rgba(hz, a1)))); x.fillRect(0, 0, W, ROAD + 20); };
        const double colK[3] = { 0.05, 0.25, 0.45 }, tintK[3] = { 0.12, 0.2, 0.3 };
        const double hbx = m_lx(150), hbw = 96, hbh = 300;
        const double sgx = m_lx(268), sgy = 392, sgw = 28, sgh = 212;
        for (int k = 0; k < 3; ++k) {
            const QColor col = mix(mixh(p.app, "#000000", colK[k]), hex(k == 2 ? "#5e3c32" : "#4a5260"), tintK[k]);
            struct B { double x, w, y, h, tone, lit; bool band, tank, fe, hotel; };
            std::vector<B> row;
            double bx = R(-30, 0);
            while (bx < W) {
                const double bw = k == 0 ? R(40, 80) : k == 1 ? R(50, 100) : R(70, 135);
                const double bh = k == 0 ? R(200, 380) : k == 1 ? R(150, 400) : R(130, 300);
                const double by = ROAD - bh + (k == 0 ? -40 : k == 1 ? -14 : 0);
                const double tone = R(-0.12, 0.14), lit = R(0.3, 1.4);
                const bool band = k < 2 && rnd() < 0.25, tank = k > 0 && rnd() < 0.5, fe = k == 2 && bw > 78 && rnd() < 0.75;
                row.push_back({ bx, bw, by, bh, tone, lit, band, tank, fe, false });
                bx += bw + (k == 0 ? R(0, 6) : k == 1 ? R(2, 14) : R(4, 18));
            }
            // na fileira da frente, um prédio de tijolo fixo onde a placa do HOTEL fica presa
            if (k == 2) {
                row.erase(std::remove_if(row.begin(), row.end(), [&](const B& b) { return b.x < hbx + hbw + 8 && b.x + b.w > hbx - 8; }), row.end());
                row.push_back({ hbx, hbw, ROAD - hbh, hbh, 0.04, 1.1, false, true, true, true });
            }
            // o arranha-céu escalonado lá no fundo, com a coroa iluminada
            if (k == 0) {
                const double cx = m_lx(300);
                const QColor cc = mixh(col, "#5a6070", 0.15), lit = mix(cc, rim, 0.12);
                const double tiers[5][2] = { { 38, 190 }, { 30, 150 }, { 22, 116 }, { 14, 90 }, { 8, 74 } };
                for (const auto& t : tiers) {
                    x.fillStyle(QBrush(lin(0, t[1], 0, ROAD, { { 0, lit }, { 0.15, cc }, { 1, mixh(cc, "#000000", 0.4) } }))); x.fillRect(cx - t[0], t[1], t[0] * 2, ROAD + 20 - t[1]);
                    x.fillStyle(rgba(SNOW, 0.35)); x.fillRect(cx - t[0] - 1, t[1] - 1, t[0] * 2 + 2, 1.5);
                }
                x.fillStyle(cc); x.begin(); x.moveTo(cx - 3, 76); x.lineTo(cx, 34); x.lineTo(cx + 3, 76); x.fill();
                if (mode == 0) m_ants.push_back({ cx, 36, 0 });
                x.save(); x.lighter();
                for (int i = 1; i < 5; ++i) { x.fillStyle(QBrush(vgrad(tiers[i][1], tiers[i][1] + 30, rgba("#ffd08a", 0.0), rgba("#ffd08a", 0.22)))); x.fillRect(cx - tiers[i][0], tiers[i][1], tiers[i][0] * 2, 30); }
                x.restore();
                for (double wy = 196; wy < ROAD - 8; wy += 7) for (double wx = cx - 34; wx < cx + 34; wx += 6) if (rnd() < 0.2) { x.fillStyle(rgba(warm(), 0.35)); x.fillRect(wx, wy, 2, 2.5); }
            }
            std::sort(row.begin(), row.end(), [VX](const B& a, const B& b) { return std::abs(a.x + a.w / 2 - VX) > std::abs(b.x + b.w / 2 - VX); });
            double hotelWall = -1;
            for (const B& b : row) {
                const QColor base = mix(col, hex(b.tone > 0 ? "#9aa4b4" : "#000000"), std::abs(b.tone));
                const double cx = b.x + b.w / 2, bot = ROAD + 20;
                const int side = cx < VX ? 1 : -1;
                const QColor topC = mix(base, rim, 0.08 + 0.03 * k), botC = mixh(base, "#000000", 0.4);
                auto faceAt = [&](double y) { const double t = (y - b.y) / (bot - b.y); return t < 0.18 ? mix(topC, base, t / 0.18) : mix(base, botC, (t - 0.18) / 0.82); };
                const double sw = qMin(5.0 + 9 * k, std::abs(cx - VX) * (0.01 + 0.008 * k));
                if (sw > 1.5) {
                    const double ex = side > 0 ? b.x + b.w : b.x, ox = ex + side * sw, drop = sw * 0.4;
                    x.fillStyle(QBrush(vgrad(b.y, bot, mixh(base, "#000000", 0.38), mixh(base, "#000000", 0.72))));
                    x.begin(); x.moveTo(ex, b.y); x.lineTo(ox, b.y + drop); x.lineTo(ox, bot); x.lineTo(ex, bot); x.close(); x.fill();
                    if (k > 0) for (double wy = b.y + 12; wy < ROAD - 14; wy += 10) {
                        const QColor wc = warm();
                        if (rnd() >= 0.22 * b.lit) continue;
                        const double x0 = ex + side * sw * 0.3, x1 = ex + side * sw * 0.65, y0 = wy + drop * 0.3, y1 = wy + drop * 0.65;
                        x.fillStyle(rgba(wc, 0.5)); x.begin(); x.moveTo(x0, y0); x.lineTo(x1, y1); x.lineTo(x1, y1 + 4); x.lineTo(x0, y0 + 4); x.fill();
                    }
                    x.fillStyle(rgba(rim, 0.06 + 0.03 * k)); x.fillRect(side > 0 ? ex - 1 : ex, b.y, 1, bot - b.y);
                    x.strokeStyle(rgba(SNOW, k == 0 ? 0.3 : k == 1 ? 0.5 : 0.7)); x.lineWidth(1.5); x.begin(); x.moveTo(ex, b.y - 0.5); x.lineTo(ox, b.y + drop - 0.5); x.stroke();
                }
                x.fillStyle(QBrush(lin(0, b.y, 0, bot, { { 0, topC }, { 0.18, base }, { 1, botC } }))); x.fillRect(b.x, b.y, b.w, bot - b.y);
                if (k == 2) { x.fillStyle(mixh(base, "#000000", 0.35)); x.fillRect(b.x - 2, b.y, b.w + 4, 5); x.fillStyle(mix(base, rim, 0.2)); x.fillRect(b.x - 2, b.y + 5, b.w + 4, 1); }
                // caixa-d'água de Nova York no telhado (pernas, barril de madeira, chapéu cônico com neve)
                const QColor roof = mixh(base, "#000000", 0.25);
                if (b.tank) {
                    const double s = k == 1 ? 0.75 : 1.1, tw = 20 * s, th = 22 * s, tx = b.x + R(6, qMax(7.0, b.w - tw - 6)), ty = b.y - 12 * s;
                    x.fillStyle(roof); x.fillRect(tx + 2 * s, ty, 2 * s, 12 * s); x.fillRect(tx + tw - 4 * s, ty, 2 * s, 12 * s); x.fillRect(tx, ty - 1, tw, 2 * s);
                    x.fillStyle(QBrush(lin(tx, 0, tx + tw, 0, { { 0, mix(roof, rim, 0.15) }, { 1, mixh(roof, "#000000", 0.3) } }))); x.fillRect(tx, ty - th, tw, th);
                    x.fillStyle(rgba("#000000", 0.3)); for (double yy = ty - th + 5 * s; yy < ty; yy += 6 * s) x.fillRect(tx, yy, tw, 0.8);
                    x.fillStyle(roof); x.begin(); x.moveTo(tx - 1.5 * s, ty - th); x.lineTo(tx + tw / 2, ty - th - 10 * s); x.lineTo(tx + tw + 1.5 * s, ty - th); x.fill();
                    x.fillStyle(rgba(SNOW, 0.75)); x.begin(); x.moveTo(tx, ty - th - 0.5); x.lineTo(tx + tw / 2, ty - th - 9 * s); x.lineTo(tx + tw, ty - th - 0.5); x.lineTo(tx + tw / 2, ty - th - 6 * s); x.fill();
                } else if (rnd() < 0.4) {
                    const double tw = R(8, 14), tx = b.x + R(3, b.w - tw - 3);
                    x.fillStyle(roof); x.fillRect(tx, b.y - 9, tw, 9); x.fillStyle(rgba(SNOW, 0.5)); x.fillRect(tx, b.y - 10, tw, 1.5);
                }
                if (b.h > 300 && k > 0) { const double ph = R(0, 6.28); if (mode == 0) m_ants.push_back({ cx, b.y - 40, ph }); x.fillStyle(roof); x.fillRect(cx - 1, b.y - 40, 2, 26); }
                x.fillStyle(rgba(SNOW, k == 0 ? 0.35 : k == 1 ? 0.55 : 0.8)); x.fillRect(b.x - (k == 2 ? 2 : 0), b.y - 2, b.w + (k == 2 ? 4 : 0), 2.5);
                if (k == 0) {
                    x.fillStyle(rgba(warm(), 0.35));
                    for (double wy = b.y + 8; wy < ROAD - 10; wy += 7) for (double wx = b.x + 4; wx < b.x + b.w - 4; wx += 7) if (rnd() < 0.18 * b.lit) x.fillRect(wx, wy, 2, 2.5);
                } else {
                    const double s = k == 2 ? 12 : 8, ww = k == 2 ? 5 : 3.5, wh = k == 2 ? 7 : 4.5;
                    if (k == 2) { x.fillStyle(rgba("#000000", 0.12)); for (double wy = b.y + 12 + wh + 2; wy < ROAD; wy += s) x.fillRect(b.x, wy, b.w, 1); }
                    for (double wy = b.y + 12; wy < ROAD - 16; wy += s) {
                        const QColor bg = mixh(faceAt(wy), "#000000", 0.3);
                        if (b.band) {
                            x.fillStyle(bg); x.fillRect(b.x + 5, wy, b.w - 10, wh);
                            for (double wx = b.x + 5; wx < b.x + b.w - 5 - s; wx += s) {
                                const bool on0 = rnd() < 0.28 * b.lit;
                                const QColor wc = rnd() < 0.5 ? hex("#e8f0ff") : warm();
                                const bool on = mode == 0 ? on0 : mode == 1;
                                if (mode == 0) m_wins.push_back({ QRectF(wx, wy, s, wh), on0 });
                                if (on) { x.fillStyle(wc); x.fillRect(wx, wy, s, wh); }
                            }
                        } else for (double wx = b.x + 6; wx < b.x + b.w - 6; wx += s) {
                            const bool on0 = rnd() < 0.32 * b.lit;
                            const QColor wc = warm();
                            const bool on = mode == 0 ? on0 : mode == 1;
                            if (mode == 0) m_wins.push_back({ QRectF(wx - 1, wy, ww + 2, wh + 2.5), on0 });
                            x.fillStyle(on ? wc : bg); x.fillRect(wx, wy, ww, wh);
                            if (k == 2) { x.fillStyle(rgba(SNOW, 0.55)); x.fillRect(wx - 1, wy + wh + 1, ww + 2, 1.2); }
                            if (on && k == 2) { x.fillStyle(rgba("#000000", 0.3)); x.fillRect(wx, wy, ww, 1); }
                        }
                    }
                }
                // escada de incêndio na fachada dos prédios de tijolo da frente
                if (b.fe) {
                    const QColor iron = mixh(base, "#000000", 0.7);
                    const double x0 = b.x + b.w * 0.18, x1 = b.x + b.w * 0.82, stp = 36;
                    int i = 0;
                    for (double ly = b.y + 30; ly < ROAD - 40; ly += stp, ++i) {
                        x.fillStyle(iron); x.fillRect(x0, ly, x1 - x0, 2.2);
                        x.strokeStyle(iron); x.lineWidth(0.8); x.begin(); x.moveTo(x0, ly - 10); x.lineTo(x1, ly - 10);
                        for (double rx2 = x0; rx2 <= x1; rx2 += 4) { x.moveTo(rx2, ly - 10); x.lineTo(rx2, ly); }
                        x.stroke();
                        x.fillStyle(rgba(SNOW, 0.8)); x.fillRect(x0, ly - 1.5, x1 - x0, 1.6); x.fillRect(x0, ly - 11, x1 - x0, 1);
                        if (ly + stp < ROAD - 40) {
                            const double a = i % 2 ? x0 + 4 : x1 - 10, cc2 = i % 2 ? x1 - 10 : x0 + 4;
                            x.strokeStyle(iron); x.lineWidth(1.2); x.begin(); x.moveTo(a, ly); x.lineTo(cc2, ly + stp); x.moveTo(a + 5, ly); x.lineTo(cc2 + 5, ly + stp); x.stroke();
                        }
                    }
                }
                for (int e = 0; e < 2; ++e) {
                    const double ex = e == 0 ? b.x : b.x + b.w; const int dir = e == 0 ? 1 : -1;
                    const double aw = 8 + 5 * k;
                    x.fillStyle(QBrush(lin(ex, 0, ex + dir * aw, 0, { { 0, rgba("#000000", 0.3 + 0.1 * k) }, { 1, rgba("#000000", 0) } })));
                    x.fillRect(qMin(ex, ex + dir * aw), b.y, aw, bot - b.y);
                }
                // a placa do HOTEL fica presa na quina deste prédio; ela é desenhada depois da fileira toda, senão o vizinho a cobre
                if (b.hotel) hotelWall = b.x + b.w + qMin(23.0, std::abs(cx - VX) * 0.026);
            }
            if (k == 2 && hotelWall > 0) {
                // dois braços de ferro saindo da parede, caixa com lateral e neve em cima, e a sombra dela na fachada de trás
                const double wall = hotelWall;
                const QColor iron = hex("#050304");
                x.fillStyle(rgba("#000000", 0.35)); x.fillRect(sgx + 6, sgy + 8, sgw, sgh);
                for (double by2 : { sgy + 14, sgy + sgh - 16 }) {
                    x.fillStyle(iron); x.fillRect(wall - 2, by2, sgx - wall + 4, 3); x.fillRect(wall - 3, by2 - 4, 3, 11);
                    x.strokeStyle(iron); x.lineWidth(1.2); x.begin(); x.moveTo(wall, by2 + 10); x.lineTo(sgx, by2 + 2); x.stroke();
                    x.fillStyle(rgba(SNOW, 0.75)); x.fillRect(wall - 2, by2 - 1, sgx - wall + 4, 1.2);
                }
                block(x, sgx, sgy, sgw, sgh, 0.03, VX, 380, hex("#140909"), hex("#1e1010"), hex("#070404"));
                x.strokeStyle(hex("#2a1414")); x.lineWidth(1.5); x.strokeRect(sgx, sgy, sgw, sgh); x.fillStyle(rgba(SNOW, 0.85)); x.fillRect(sgx - 1, sgy - 3, sgw + 2, 3);
                x.save(); x.lighter(); glowAt(x, wall + 4, sgy + sgh / 2, 120, NEON, 0.06); x.restore();
            }
            // a nevasca entre os planos (mais grossa que a névoa da Paulista)
            if (k == 0) hazeOver(0.2, 0.62);
            if (k == 1) hazeOver(0.08, 0.36);
        }
    }

    void buildCity(QPixmap& pm, const Ctx& c, const Pal& p, int mode)
    {
        Seeded sd(0xB077E71Du);
        const double W = c.W;
        pm = layer(c, W, kH);
        QPainter qp(&pm);
        Cv x(qp);
        x.fillStyle(QBrush(vgrad(0, ROAD, mixh(p.app, "#0c0f15", 0.45), mixh(p.app, "#4a3c3a", 0.42)))); x.fillRect(0, 0, W, ROAD + 20);
        const QPixmap cd = softDot(mixh(p.app, "#22222a", 0.4), 0.4);
        for (int i = 0; i < int(40 * W / 1440.0) + 3; ++i) { const double cx = R(-100, W + 100), cy = R(0, 240), w = R(160, 420); x.alphaG(R(0.3, 0.7)); x.image(cd, cx - w / 2, cy - w * 0.12, w, w * 0.24); }
        x.alphaG(1);
        nyCity(x, c, p, mode);
        // a rua: neve suja, marcas de pneu, meio-fio dos dois lados
        x.fillStyle(QBrush(vgrad(ROAD, NEAR, mixh(p.app, "#5a5e6a", 0.45), mixh(p.app, "#7a7e8a", 0.4)))); x.fillRect(0, ROAD, W, NEAR - ROAD);
        x.fillStyle(mixh(p.app, "#8a8e9a", 0.35)); x.fillRect(0, ROAD - 3, W, 5); x.fillStyle(rgba(SNOW, 0.6)); x.fillRect(0, ROAD - 3, W, 1.5); x.fillStyle(rgba("#000000", 0.35)); x.fillRect(0, ROAD + 2, W, 2);
        x.strokeStyle(rgba("#24262e", 0.3));
        const double tracks[4][2] = { { ROAD + 16, 3 }, { ROAD + 24, 3 }, { ROAD + 44, 4 }, { ROAD + 54, 4 } };
        for (const auto& t : tracks) { x.lineWidth(t[1]); x.begin(); x.moveTo(0, t[0]); x.bezTo(W * 0.28, t[0] + 2, W * 0.62, t[0] - 3, W, t[0] + 1); x.stroke(); }
        x.fillStyle(QBrush(vgrad(NEAR, kH, mixh(p.app, "#aeb2be", 0.32), mixh(p.app, "#7e828e", 0.4)))); x.fillRect(0, NEAR, W, kH - NEAR);
        x.fillStyle(rgba("#2a2c34", 0.45)); x.fillRect(0, NEAR, W, 3); x.fillStyle(rgba(SNOW, 0.7)); x.fillRect(0, NEAR + 3, W, 1.5);
        for (int k = 0; k < 14; ++k) { const double fx = m_lx(420 + k * 26 - (k % 2) * 4), fy = 748 - k * 1.2 + (k % 2) * 5; x.fillStyle(rgba("#3a3c48", 0.25)); x.begin(); x.ellipse(fx, fy, 5, 2, 0, 0, TAU); x.fill(); }
        x.lighter(); x.save(); x.translate(m_lx(312), 748); x.scale(1, 0.18); glowAt(x, 0, 0, 170, AMBER, 0.32); x.restore();
        for (double px = 80; px < W; px += 240) { glowAt(x, px + 10, ROAD - 62, 40, "#ffd28a", 0.25); x.save(); x.translate(px + 10, ROAD + 8); x.scale(1, 0.25); glowAt(x, 0, 0, 70, "#ffd28a", 0.2); x.restore(); }
        x.over();
        for (double px = 80; px < W; px += 240) {
            x.fillStyle(mixh(p.app, "#000000", 0.5)); x.fillRect(px - 1, ROAD - 66, 2.5, 64); x.fillRect(px - 1, ROAD - 66, 12, 2);
            x.fillStyle(rgba("#fff4d0", 0.95)); x.fillRect(px + 8, ROAD - 63, 5, 1.5); x.fillStyle(rgba(SNOW, 0.7)); x.fillRect(px - 1, ROAD - 67, 13, 1);
        }
    }

    void build(const Ctx& c, const Pal& p)
    {
        m_wins.clear(); m_ants.clear();
        // a mesma cidade três vezes (mesma semente): como sorteada, toda acesa e toda apagada. A troca de janela copia o
        // retângulo da versão certa, então o que está na frente dela (névoa, outro prédio, a placa) continua na frente.
        buildCity(m_city, c, p, 0);
        buildCity(m_cityOn, c, p, 1);
        buildCity(m_cityOff, c, p, 2);
        const double W = c.W;
        m_front = layer(c, W, kH);
        QPainter qp(&m_front);
        Cv f(qp);
        const double L = m_ox;
        // a entrada do metrô na calçada: guarda-corpo e os dois globos verdes
        f.fillStyle(hex("#050506")); f.fillRect(L + 28, 730, 116, 30);
        f.fillStyle(QBrush(vgrad(730, 760, rgba("#ffcf8a", 0.0), rgba("#ffcf8a", 0.22)))); f.fillRect(L + 32, 734, 108, 26);
        f.strokeStyle(hex("#0c0c10")); f.lineWidth(2); f.begin(); f.moveTo(L + 28, 730); f.lineTo(L + 144, 730); for (double bx = 28; bx <= 144; bx += 10) { f.moveTo(L + bx, 730); f.lineTo(L + bx, 760); } f.stroke();
        f.fillStyle(rgba(SNOW, 0.85)); f.fillRect(L + 26, 727, 120, 2.5);
        for (double gx0 : { 34.0, 138.0 }) {
            const double gx = L + gx0;
            f.fillStyle(hex("#0c0c10")); f.fillRect(gx - 2, 692, 4, 40); f.fillStyle(hex("#2a6a3a")); f.begin(); f.arc(gx, 688, 8, 0, TAU); f.fill();
            f.fillStyle(rgba(SNOW, 0.8)); f.begin(); f.arc(gx, 684, 6, M_PI * 1.1, M_PI * 1.9); f.fill();
        }
        f.fillStyle(hex("#4a1414")); f.fillRect(L + 206, 726, 12, 22); f.fillRect(L + 202, 732, 20, 5); f.begin(); f.arc(L + 212, 726, 6, M_PI, 0); f.fill();
        f.fillStyle(rgba(SNOW, 0.85)); f.begin(); f.arc(L + 212, 724, 6, M_PI, 0); f.fill();
        // o poste da calçada de cá, cabeça de cobra com neve em cima
        f.strokeStyle(hex("#08080a")); f.lineWidth(5); f.roundCaps(); f.begin(); f.moveTo(L + 354, 752); f.lineTo(L + 354, 588); f.quadTo(L + 354, 562, L + 330, 562); f.stroke();
        f.fillStyle(hex("#08080a")); f.fillRect(L + 347, 740, 14, 14); f.begin(); f.ellipse(L + 320, 566, 18, 6, 0, 0, TAU); f.fill();
        f.fillStyle(rgba(SNOW, 0.85)); f.begin(); f.ellipse(L + 320, 561, 15, 2.5, 0, M_PI, 0); f.fill(); f.fillRect(L + 328, 558, 20, 1.8);
        // do outro lado: a viatura parada no meio-fio, com neve no teto
        policeCar(f, m_rx(1140), NEAR - 2);
        // o bar do outro lado: o térreo de um prédio da frente (fachada de tijolo escuro, vitrine e porta acesas por
        // dentro) e o letreiro BAR preso na parede em cima da porta, com a sombra dele na fachada e neve na borda
        {
            const double bx = m_rx(1296), fy = 590;
            f.fillStyle(QBrush(vgrad(fy, ROAD, hex("#1c1416"), hex("#0c0809")))); f.fillRect(bx - 16, fy, 140, ROAD - fy + 2);
            f.fillStyle(rgba("#000000", 0.25)); for (double yy = fy + 6; yy < ROAD; yy += 6) f.fillRect(bx - 16, yy, 140, 1);
            f.fillStyle(hex("#24191b")); f.fillRect(bx - 18, fy - 4, 144, 5); f.fillStyle(rgba(SNOW, 0.8)); f.fillRect(bx - 18, fy - 5, 144, 1.6);
            // vitrine: luz quente lá dentro, garrafas na prateleira, caixilho
            f.fillStyle(QBrush(vgrad(fy + 18, fy + 48, rgba("#ffb868", 0.55), rgba("#c06a2a", 0.35)))); f.fillRect(bx - 6, fy + 18, 62, 30);
            f.fillStyle(rgba("#3a1c10", 0.8)); f.fillRect(bx - 6, fy + 30, 62, 2);
            for (double gx = bx - 2; gx < bx + 52; gx += 5) { f.fillStyle(rgba(RI(3) == 0 ? "#3a7a4a" : "#5a2a18", 0.85)); f.fillRect(gx, fy + 23 + R(0, 2), 2.2, 7); }
            f.strokeStyle(hex("#0a0607")); f.lineWidth(2); f.strokeRect(bx - 6, fy + 18, 62, 30); f.lineWidth(1.2); f.begin(); f.moveTo(bx + 25, fy + 18); f.lineTo(bx + 25, fy + 48); f.stroke();
            f.fillStyle(rgba(SNOW, 0.75)); f.fillRect(bx - 7, fy + 48, 64, 1.6);
            // porta com o vidro aceso
            f.fillStyle(hex("#0a0607")); f.fillRect(bx + 70, fy + 12, 26, ROAD - fy - 12);
            f.fillStyle(QBrush(vgrad(fy + 16, ROAD, rgba("#ffc078", 0.5), rgba("#a85a22", 0.25)))); f.fillRect(bx + 73, fy + 16, 20, ROAD - fy - 18);
            f.fillStyle(hex("#0a0607")); f.fillRect(bx + 73, fy + 34, 20, 2);
            // o letreiro na parede, em cima da vitrine e da porta: sombra, presilhas, caixa com borda e neve em cima
            const double sx = bx - 4, sy = 560, sw = 104, sh = 28;
            f.fillStyle(rgba("#000000", 0.45)); f.fillRect(sx + 4, sy + 5, sw, sh);
            f.fillStyle(hex("#0a0e16")); f.fillRect(sx, sy, sw, sh);
            f.strokeStyle(hex("#1c2636")); f.lineWidth(1.5); f.strokeRect(sx + 0.75, sy + 0.75, sw - 1.5, sh - 1.5);
            f.fillStyle(hex("#2a2f38")); for (double px : { sx + 8, sx + sw - 10 }) { f.fillRect(px, sy - 4, 3, 5); f.fillRect(px, sy + sh - 1, 3, 5); }
            f.fillStyle(rgba(SNOW, 0.85)); f.fillRect(sx - 1, sy - 2, sw + 2, 2.2);
        }
    }

    // A viatura parada no meio-fio, de lado, frente pra esquerda: sedã preto e branco com volume (lataria com luz em
    // cima e sombra embaixo, portas brancas, vidros com reflexo, caixa de roda, calota, para-choque, farol, lanterna,
    // giroflex no teto) e neve no teto, no capô e no porta-malas. (cx, gy) = ponta da frente, chão.
    void policeCar(Cv& f, double cx, double gy)
    {
        auto X = [cx](double v) { return cx + v; };
        auto Y = [gy](double v) { return gy + v; };
        const double w1 = 38, w2 = 154, wy = -1, wr = 13;
        // sombra no chão, mais escura embaixo das rodas
        f.fillStyle(rgba("#000000", 0.42)); f.begin(); f.ellipse(X(96), Y(10), 102, 5, 0, 0, TAU); f.fill();
        for (double wx : { w1, w2 }) { f.fillStyle(rgba("#000000", 0.5)); f.begin(); f.ellipse(X(wx), Y(11), 18, 3, 0, 0, TAU); f.fill(); }
        // a silhueta, com as caixas de roda recortadas
        auto body = [&]() {
            f.begin();
            f.moveTo(X(6), Y(-6)); f.lineTo(X(1), Y(-9)); f.lineTo(X(0), Y(-15)); f.quadTo(X(0), Y(-22), X(6), Y(-25));
            f.lineTo(X(50), Y(-30)); f.quadTo(X(60), Y(-42), X(70), Y(-49)); f.quadTo(X(73), Y(-51), X(78), Y(-51));
            f.lineTo(X(124), Y(-51)); f.quadTo(X(130), Y(-51), X(133), Y(-48)); f.lineTo(X(150), Y(-33));
            f.lineTo(X(184), Y(-31)); f.quadTo(X(190), Y(-30), X(190), Y(-25)); f.lineTo(X(190), Y(-14)); f.quadTo(X(190), Y(-7), X(184), Y(-6));
            f.lineTo(X(w2 + 17), Y(-6)); f.arc(X(w2), Y(wy), 17, -0.3, M_PI + 0.3, true);
            f.lineTo(X(w1 + 17), Y(-6)); f.arc(X(w1), Y(wy), 17, -0.3, M_PI + 0.3, true);
            f.close();
        };
        body();
        f.fillStyle(QBrush(vgrad(Y(-51), Y(-4), hex("#33363d"), hex("#050506")))); f.fill();
        // portas brancas (só dentro da lataria), mais claras em cima
        f.save(); body(); f.clip();
        f.fillStyle(QBrush(vgrad(Y(-31), Y(-7), hex("#f2f4f7"), hex("#a9b0ba")))); f.fillRect(X(52), Y(-31), 95, 25);
        f.fillStyle(rgba("#000000", 0.22)); f.fillRect(X(52), Y(-11), 95, 5);   // a sombra que o estribo faz embaixo da porta
        f.restore();
        // linha de cintura pegando a luz, de ponta a ponta
        f.strokeStyle(rgba("#ffffff", 0.22)); f.lineWidth(1); f.begin(); f.moveTo(X(6), Y(-25)); f.lineTo(X(50), Y(-30)); f.lineTo(X(184), Y(-31)); f.stroke();
        // vidros: a estufa com reflexo, a coluna do meio preta
        f.begin(); f.moveTo(X(60), Y(-34)); f.quadTo(X(68), Y(-46), X(76), Y(-48)); f.lineTo(X(124), Y(-48)); f.lineTo(X(142), Y(-34)); f.close();
        f.fillStyle(QBrush(vgrad(Y(-48), Y(-34), hex("#2c3a4e"), hex("#121822")))); f.fill();
        f.save(); f.clip();
        f.fillStyle(rgba("#cfe0ff", 0.16)); f.begin(); f.moveTo(X(70), Y(-48)); f.lineTo(X(84), Y(-48)); f.lineTo(X(74), Y(-34)); f.lineTo(X(60), Y(-34)); f.fill();
        f.fillStyle(rgba("#cfe0ff", 0.09)); f.begin(); f.moveTo(X(110), Y(-48)); f.lineTo(X(118), Y(-48)); f.lineTo(X(108), Y(-34)); f.lineTo(X(100), Y(-34)); f.fill();
        f.restore();
        f.fillStyle(hex("#0a0b0d")); f.fillRect(X(98), Y(-48), 4, 15);
        // emendas das portas, maçanetas e o retrovisor
        f.strokeStyle(rgba("#000000", 0.45)); f.lineWidth(1); f.begin();
        for (double sx : { 52.0, 100.0, 146.0 }) { f.moveTo(X(sx), Y(-31)); f.lineTo(X(sx), Y(-8)); }
        f.stroke();
        f.fillStyle(hex("#5a6068")); f.fillRect(X(86), Y(-27), 7, 1.8); f.fillRect(X(133), Y(-27), 7, 1.8);
        f.fillStyle(hex("#0c0d10")); f.begin(); f.moveTo(X(58), Y(-35)); f.lineTo(X(64), Y(-37)); f.lineTo(X(64), Y(-33)); f.lineTo(X(58), Y(-32)); f.fill();
        // POLICE na porta
        f.setFont(8.5, 800, { QStringLiteral("IBM Plex Sans"), QStringLiteral("Arial") });
        f.align = 0; f.fillStyle(hex("#1a2a4a")); f.fillText(QStringLiteral("POLICE"), X(99), Y(-15)); f.align = -1;
        f.fillStyle(hex("#1a2a4a")); f.fillRect(X(52), Y(-24), 95, 1.6);
        // para-choques cromados, farol e lanterna
        for (const auto& b : { std::array<double, 2>{ -1, 8 }, std::array<double, 2>{ 183, 8 } }) {
            f.fillStyle(QBrush(vgrad(Y(-15), Y(-9), hex("#d4d8de"), hex("#4a4e56")))); f.fillRect(X(b[0]), Y(-15), b[1], 6);
        }
        f.fillStyle(hex("#fff4cc")); f.fillRect(X(1), Y(-22), 5, 4);
        f.fillStyle(hex("#a3141a")); f.fillRect(X(186), Y(-26), 4, 7);
        // rodas: caixa de roda escura, pneu, calota com brilho
        for (double wx : { w1, w2 }) {
            f.fillStyle(hex("#020203")); f.begin(); f.arc(X(wx), Y(wy), 16, 0, TAU); f.fill();
            f.fillStyle(hex("#0b0b0d")); f.begin(); f.arc(X(wx), Y(wy), wr, 0, TAU); f.fill();
            f.strokeStyle(hex("#202024")); f.lineWidth(1.5); f.begin(); f.arc(X(wx), Y(wy), wr - 2.5, 0, TAU); f.stroke();
            f.fillStyle(QBrush(rad(X(wx) - 2, Y(wy) - 2.5, 0.5, X(wx), Y(wy), 7.5, { { 0, hex("#e6e9ee") }, { 0.55, hex("#9aa0a8") }, { 1, hex("#4a4e56") } })));
            f.begin(); f.arc(X(wx), Y(wy), 7.5, 0, TAU); f.fill();
            f.fillStyle(hex("#2a2d33")); f.begin(); f.arc(X(wx), Y(wy), 2.2, 0, TAU); f.fill();
            f.fillStyle(rgba(SNOW, 0.5)); f.begin(); f.arc(X(wx), Y(wy), wr, M_PI * 1.15, M_PI * 1.85); f.arc(X(wx), Y(wy), wr - 1.5, M_PI * 1.85, M_PI * 1.15, true); f.fill();
        }
        // o giroflex no teto (a luz que pisca é desenhada por quadro, em cima destas lentes)
        f.fillStyle(hex("#141519")); f.fillRect(X(80), Y(-54), 40, 3);
        f.fillStyle(hex("#5a1418")); f.fillRect(X(82), Y(-57), 16, 4);
        f.fillStyle(hex("#16245a")); f.fillRect(X(100), Y(-57), 16, 4);
        f.fillStyle(hex("#c8ccd2")); f.fillRect(X(98), Y(-57), 2, 4);
        // antena no porta-malas
        f.strokeStyle(hex("#16171a")); f.lineWidth(0.8); f.begin(); f.moveTo(X(176), Y(-31)); f.lineTo(X(170), Y(-56)); f.stroke();
        // neve no teto, no capô e no porta-malas
        f.fillStyle(rgba(SNOW, 0.92));
        f.begin(); f.moveTo(X(74), Y(-51)); f.quadTo(X(100), Y(-55), X(128), Y(-51)); f.lineTo(X(126), Y(-50)); f.lineTo(X(76), Y(-50)); f.fill();
        f.begin(); f.moveTo(X(8), Y(-25.5)); f.quadTo(X(28), Y(-30), X(49), Y(-30.5)); f.lineTo(X(49), Y(-29)); f.lineTo(X(8), Y(-24.5)); f.fill();
        f.begin(); f.moveTo(X(151), Y(-33.5)); f.quadTo(X(168), Y(-35.5), X(185), Y(-31.5)); f.lineTo(X(185), Y(-30.5)); f.lineTo(X(151), Y(-32.5)); f.fill();
    }

    void drawCar(Cv& x, const Car& c)
    {
        const double q = c.dir > 0 ? 1 : 0.8, len = 74 * q, hgt = 15 * q, x0 = c.x - (c.dir > 0 ? len : 0), y = c.lane, f = c.dir > 0 ? x0 + len : x0;
        x.fillStyle(rgba("#000000", 0.35)); x.fillRect(x0 + 2, y + 1, len, 3 * q);
        x.fillStyle(c.col); x.fillRect(x0, y - hgt, len, hgt * 0.62);
        x.begin(); x.moveTo(x0 + len * 0.22, y - hgt * 0.62); x.lineTo(x0 + len * 0.32, y - hgt * 1.15); x.lineTo(x0 + len * 0.72, y - hgt * 1.15); x.lineTo(x0 + len * 0.82, y - hgt * 0.62); x.fill();
        x.fillStyle(rgba("#8a9aaa", 0.45)); x.fillRect(x0 + len * 0.34, y - hgt * 1.08, len * 0.36, hgt * 0.38);
        x.fillStyle(rgba(SNOW, 0.8)); x.fillRect(x0 + len * 0.32, y - hgt * 1.17, len * 0.4, 1.6);
        if (c.cab) {
            x.fillStyle(hex("#f4e8b0")); x.fillRect(x0 + len * 0.46, y - hgt * 1.38, len * 0.1, hgt * 0.22);
            x.fillStyle(hex("#1a1a1a")); for (int i = 0; i < 6; ++i) x.fillRect(x0 + len * (0.1 + i * 0.13), y - hgt * 0.36, len * 0.06, 1.6);
        }
        x.fillStyle(hex("#060608")); for (double wx : { x0 + len * 0.18, x0 + len * 0.8 }) { x.begin(); x.arc(wx, y, 5.5 * q, 0, TAU); x.fill(); }
        x.lighter();
        x.fillStyle(QBrush(lin(f, 0, f + c.dir * 120 * q, 0, { { 0, rgba("#fff4d6", 0.3) }, { 1, rgba("#fff4d6", 0) } })));
        x.begin(); x.moveTo(f, y - hgt * 0.45); x.lineTo(f + c.dir * 120 * q, y - hgt * 0.9); x.lineTo(f + c.dir * 120 * q, y + 4); x.close(); x.fill();
        glowAt(x, f, y - hgt * 0.4, 9, "#fff4d6", 0.8); glowAt(x, f - c.dir * len, y - hgt * 0.4, 7, "#ff3a2a", 0.7);
        x.over();
    }

    void scene(QPainter& painter, const Pal& pal, Ctx& c)
    {
        Q_UNUSED(pal);
        const double T = m_t, W = c.W;
        const double L = m_ox;
        painter.save();
        Cv x(painter);
        x.image(m_city, 0, 0);
        x.fillStyle(QBrush(vgrad(ROAD - 260, ROAD, rgba("#ff9a5a", 0), rgba("#ff9a5a", 0.1)))); x.fillRect(0, ROAD - 260, W, 260);
        for (const Ant& a0 : m_ants) {
            const double a = std::pow(qMax(0.0, std::sin(T * 2.2 + a0.ph)), 6);
            x.fillStyle(rgba("#ff3b3b", 0.3 + 0.7 * a)); x.begin(); x.arc(a0.x, a0.y, 2.6, 0, TAU); x.fill();
            if (a > 0.3) { x.fillStyle(rgba("#ff3b3b", 0.22 * a)); x.begin(); x.arc(a0.x, a0.y, 11, 0, TAU); x.fill(); }
        }
        for (const Car& cr : m_cars) if (cr.dir < 0) drawCar(x, cr);
        for (const Car& cr : m_cars) if (cr.dir > 0) drawCar(x, cr);
        x.image(m_front, 0, 0);
        x.lighter();
        for (double gx : { 34.0, 138.0 }) glowAt(x, L + gx, 688, 30, "#5aff8a", 0.25);
        {
            x.setFont(29, 600, { QStringLiteral("IBM Plex Sans"), QStringLiteral("Arial") });
            x.align = 0; x.baseline = 1;
            const double sgx = m_lx(268), sgy = 392, sgw = 28, sgh = 212, lx = sgx + sgw / 2;
            glowAt(x, lx, sgy + sgh / 2, 150, NEON, 0.12);
            const QString word = QStringLiteral("HOTEL");
            for (int i = 0; i < word.size(); ++i) {
                const double y = sgy + 26 + i * 40;
                const bool off = i == 2 && m_neonBuzz > 0 && std::sin(T * 90 / qMax(m_ts, 0.05)) > -0.2;
                if (off) continue;
                const QString ch(word[i]);
                x.lineWidth(6); x.strokeStyle(rgba(NEON, 0.3)); x.strokeText(ch, lx, y);
                x.fillStyle(rgba("#ff7a6a", 0.95)); x.fillText(ch, lx, y);
                x.lineWidth(1); x.strokeStyle(rgba("#ffe0d8", 0.7)); x.strokeText(ch, lx, y);
            }
            x.align = -1; x.baseline = 0;
        }
        {   // o giroflex da viatura e o neon do bar
            const bool red = int(std::floor(T * 2.6)) % 2 == 0;
            const QColor gc = hex(red ? "#ff2a2a" : "#2a5aff");
            glowAt(x, m_rx(red ? 1228 : 1252), NEAR - 58, 60, gc, 0.6); glowAt(x, m_rx(1240), NEAR - 60, 320, gc, 0.10);
            x.fillStyle(hex(red ? "#ff5a4a" : "#6a8aff")); x.fillRect(m_rx(red ? 1222 : 1240), NEAR - 59, 16, 4);
            x.setFont(25, 700, { QStringLiteral("IBM Plex Sans"), QStringLiteral("Arial") });
            x.align = 0; x.lineWidth(6); x.strokeStyle(rgba("#3a8aff", 0.35)); x.strokeText(QStringLiteral("BAR"), m_rx(1348), 584);
            x.fillStyle(rgba("#a8d0ff", 0.95)); x.fillText(QStringLiteral("BAR"), m_rx(1348), 584); x.align = -1; glowAt(x, m_rx(1348), 576, 110, "#3a8aff", 0.14);
        }
        const double lampX = L + 318, lampY = 566, c0 = L + 312 - 104, cy = 748, c1 = L + 312 + 104;
        {   // o cone de sódio do poste
            x.fillStyle(QBrush(vgrad(lampY, cy, rgba(AMBER, 0.24), rgba(AMBER, 0.04))));
            x.begin(); x.moveTo(lampX - 10, lampY + 4); x.lineTo(c0, cy); x.lineTo(c1, cy); x.lineTo(lampX + 10, lampY + 4); x.fill();
            glowAt(x, lampX, lampY + 6, 60, AMBER, 0.5); glowAt(x, lampX, lampY + 5, 12, "#fff0d0", 0.9);
        }
        x.over();
        for (const Steam& s : m_steam) { const double k = s.age / s.life; x.alphaG(0.14 * std::sin(M_PI * k)); x.image(m_snowSpr, s.x - s.r, s.y - s.r, s.r * 2, s.r * 2); }
        for (const Flake& q : m_flakes) { x.alphaG(0.3 + 0.62 * q.z); const double s = q.r * 2.5; x.image(m_snowSpr, q.x - s / 2, q.y - s / 2, s, s); }
        x.save();
        x.begin(); x.moveTo(lampX - 10, lampY + 4); x.lineTo(c0, cy); x.lineTo(c1, cy); x.lineTo(lampX + 10, lampY + 4); x.clip();
        x.lighter();
        for (const Flake& q : m_flakes) {
            if (q.x < c0 - 10 || q.x > c1 + 10 || q.y < lampY) continue;
            x.alphaG(0.5 + 0.5 * q.z); const double s = q.r * 2.6; x.image(m_amberSpr, q.x - s / 2, q.y - s / 2, s, s);
        }
        x.restore();
        x.alphaG(1);
        x.fillStyle(QBrush(vgrad(560, kH, rgba("#c8ccd6", 0), rgba("#c8ccd6", 0.1)))); x.fillRect(0, 560, W, kH - 560);
        painter.restore();
    }

    // o sépia do bullet time: a cor some (cinza por cima) e ganha o tom de foto velha, com a vinheta
    void sepia(QPainter& bp, const Ctx& c)
    {
        const double bt = m_bt;
        QImage gray = m_buf.convertToFormat(QImage::Format_Grayscale8);
        gray.setDevicePixelRatio(c.px);
        bp.save();
        bp.setOpacity(0.85 * bt);
        bp.drawImage(QRectF(0, 0, c.W, kH), gray);
        bp.setOpacity(1);
        bp.setCompositionMode(QPainter::CompositionMode_SoftLight);
        bp.fillRect(QRectF(0, 0, c.W, kH), rgba("#b08a52", 0.55 * bt));
        bp.setCompositionMode(QPainter::CompositionMode_SourceOver);
        QRadialGradient vg(QPointF(m_lx(300), 400), 900, QPointF(m_lx(300), 400), 120);
        vg.setColorAt(0, QColor(0, 0, 0, 0)); vg.setColorAt(1, rgba("#0a0602", 0.55 * bt));
        bp.fillRect(QRectF(0, 0, c.W, kH), vg);
        bp.restore();
    }

    // a bala de latão (a única coisa com cor no bullet time) e o rastro de ar em espiral
    void drawBullet(Cv& x)
    {
        if (!m_bullet || m_bt <= 0.01) return;
        const double bt = m_bt;
        x.lineWidth(0.9);
        for (const QPointF& t : m_trail) {
            const double dd = m_bx - t.x(); if (dd < 6) continue;
            const double a = 0.5 * std::exp(-dd / 260) * bt; if (a < 0.01) continue;
            const double ry = 3 + dd * 0.028, rx = 1.2 + dd * 0.006, oy = std::sin(dd * 0.09) * (1 + dd * 0.01);
            x.strokeStyle(rgba("#f4ead8", a)); x.begin(); x.ellipse(t.x(), t.y() + oy, rx, ry, 0.06, 0, TAU); x.stroke();
        }
        x.strokeStyle(rgba("#f4ead8", 0.22 * bt)); x.lineWidth(1); x.begin();
        x.moveTo(qMax(m_bx - 420, -30.0), m_by - qMin(420.0, m_bx + 30) * 0.06); x.lineTo(m_bx - 8, m_by); x.stroke();
        x.save(); x.translate(m_bx, m_by); x.rotate(0.06);
        x.fillStyle(QBrush(lin(0, -3, 0, 3, { { 0, hex("#ffe6a0") }, { 0.4, hex("#d8a84a") }, { 1, hex("#6a4818") } })));
        x.begin(); x.moveTo(-9, -3); x.lineTo(2, -3); x.quadTo(9, -2.4, 11, 0); x.quadTo(9, 2.4, 2, 3); x.lineTo(-9, 3); x.close(); x.fill();
        x.fillStyle(rgba("#fff8e0", 0.8)); x.fillRect(-7, -2.2, 12, 0.9);
        x.restore();
    }

    double m_t = 0, m_ox = 0, m_w = 1440, m_vx = 720, m_toggle = 0;
    double m_ts = 1, m_stT = 9, m_tt = 0, m_bt = 0, m_neonBuzz = 0, m_neonT = 3, m_steamT = 0;
    St m_st = Idle;
    QString m_key;
    QPixmap m_city, m_cityOn, m_cityOff, m_front, m_snowSpr, m_amberSpr;
    QImage m_buf;
    std::vector<Win> m_wins;
    std::vector<Ant> m_ants;
    std::vector<Flake> m_flakes;
    std::vector<Car> m_cars;
    std::vector<Steam> m_steam;
    bool m_bullet = false;
    double m_bx = 0, m_by = 0, m_blast = 0;
    std::vector<QPointF> m_trail;
};

/* ===================================================================================================================
   Optimus Write (shapeshift): a mesa que se transforma
   =================================================================================================================== */
// A mesa é UMA chapa lisa de aço escovado (pedido do Pe: sem emenda, sem rebite). Por baixo existe uma grade invisível
// de chapas (1×1, 2×1, 1×2, 2×2, 3×1) que só aparece quando as peças se soltam. De tempos em tempos, um grupo de 3 a 6
// chapas num pedaço de mesa à vista se solta, cada uma com um movimento, com atraso escalonado. No buraco aparece o
// mecanismo: cavidade funda, vigas, trilhas de cobre, cabos com pulso de energia, trem de engrenagens engatadas (só
// cinza e preto, de formatos, tamanhos e ângulos variados), biela-manivela empurrando um pistão, cilindros hidráulicos
// e um núcleo azul. Depois tudo volta na ordem inversa e encaixa sem fresta (sem estalo de luz, pedido do Pe).
class Shapeshift : public Layer {
public:
    void init(Ctx&) override { m_key.clear(); m_ev.reset(); }

    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        if (!m_ev) { m_nextEv -= dt; if (m_nextEv < 0) { startEv(c); m_nextEv = R(9, 16); } }
        else {
            Ev& e = *m_ev;
            e.t += dt;
            double open = 0;
            for (const Act& a : e.act) open += evE(e, a);
            open /= qMax<size_t>(1, e.act.size());
            e.mech.ang += dt * 1.1 * (0.3 + 1.7 * open);
            e.open = open;
            if (e.t > e.openT * 2 + e.hold + 0.6) { m_ev.reset(); m_nextEv = R(9, 16); }
        }
    }

    void paint(QPainter& painter, const Pal& pal, Ctx& c) override
    {
        const QString key = QString::number(c.W, 'f', 0) + QLatin1Char('|') + QString::number(c.px, 'f', 2) + pal.app.name();
        if (key != m_key) { build(c); m_key = key; m_ev.reset(); }
        painter.save();
        Cv x(painter);
        x.image(m_desk, 0, 0);
        if (m_ev) {
            Ev& e = *m_ev;
            std::vector<double> es;
            for (const Act& a : e.act) es.push_back(evE(e, a));
            // o mecanismo, só dentro dos buracos
            x.save();
            x.begin(); for (const Act& a : e.act) x.rect(a.pl->x, a.pl->y, a.pl->w, a.pl->h); x.clip();
            drawMech(x, e.mech, e.open);
            for (const Act& a : e.act) holeShade(x, *a.pl);
            x.restore();
            // as chapas (as mais levantadas por cima)
            std::vector<int> order(e.act.size());
            for (size_t i = 0; i < order.size(); ++i) order[i] = int(i);
            std::sort(order.begin(), order.end(), [&](int u, int v) { return es[size_t(u)] < es[size_t(v)]; });
            for (int i : order) drawPlate(x, e.act[size_t(i)], es[size_t(i)]);
            // a emenda acende quando destrava
            if (e.t < 0.5) {
                const double k = 1 - e.t / 0.5;
                x.lighter();
                x.strokeStyle(rgba(ENER, 0.8 * k)); x.lineWidth(1.5);
                for (const Act& a : e.act) x.strokeRect(a.pl->x + 0.75, a.pl->y + 0.75, a.pl->w - 1.5, a.pl->h - 1.5);
                for (const Act& a : e.act) glowAt(x, a.pl->x + a.pl->w / 2, a.pl->y + a.pl->h / 2, qMax(a.pl->w, a.pl->h) * 0.7, ENER, 0.06 * k);
                x.over();
            }
        }
        painter.restore();
    }

private:
    struct Plate { double x, y, w, h; QPixmap img; };
    struct Gear { double x, y, r; int n; double w, ph = 0; int prof = 0, body = 0; double tone = 0.25, tilt = 0, phi = 0; int spokes = 5; };
    struct Cable { std::array<double, 8> c; double off, sp; };
    struct Cyl { double x, y, a, len, ph, sp; };
    struct Led { double x, y; QColor c; double ph, sp; };
    struct Mech {
        QRectF bb; QPixmap stat; std::vector<Gear> gears, back; std::vector<Cable> cables; std::vector<Cyl> cyls; std::vector<Led> leds;
        int crankG = 0; double rp = 0, L = 0, ax = 1, ay = 0; double coreX = 0, coreY = 0, coreR = 12; double ang = 0;
    };
    struct Act { const Plate* pl; int type; int dx, dy; int rot; double d, dur; };
    struct Ev { std::vector<Act> act; double t = 0, openT = 0, hold = 4, open = 0; Mech mech; };

    static constexpr double C = 92, OX = -16, OY = -20;
    const QColor ENER = hex("#38d4ff");
    enum { Trap, Square, Round, Sharp };            // perfil do dente
    enum { Holes, Spokes, Groove, Compound };       // miolo

    static double ease(double u) { u = qBound(0.0, u, 1.0); return u < 0.5 ? 4 * u * u * u : 1 - std::pow(-2 * u + 2, 3) / 2; }
    static double evE(const Ev& e, const Act& a)
    {
        const double t = e.t;
        if (t < e.openT) return ease((t - a.d) / a.dur);
        if (t < e.openT + e.hold) return 1;
        const double tc = t - e.openT - e.hold, dRev = e.openT - (a.d + a.dur);
        return 1 - ease((tc - dRev) / a.dur);
    }

    void build(const Ctx& c)
    {
        Seeded sd(0x5A4E5F17u);
        const double W = c.W;
        m_desk = layer(c, W, kH);
        {
            QPainter qp(&m_desk);
            const QImage tex = Metal::texture(QStringLiteral("brushed"), hex("#232629"), QSize(qMax(2, int(std::ceil(W))), int(kH)), c.px, false);
            if (!tex.isNull()) qp.drawImage(QPointF(0, 0), tex);
            Cv x(qp);
            // luz geral: mais clara em cima à esquerda, escurecendo pro canto
            x.fillStyle(QBrush(lin(0, 0, W * 0.6, kH, { { 0, rgba("#ffffff", 0.06) }, { 1, rgba("#000000", 0.28) } }))); x.fillRect(0, 0, W, kH);
        }
        m_plates.clear();
        const int cols = int(std::ceil((W - OX) / C)) + 1, rows = int(std::ceil((kH - OY) / C)) + 1;
        std::vector<char> used(size_t(cols * rows), 0);
        auto at = [&](int r, int col) -> char& { return used[size_t(r * cols + col)]; };
        for (int r = 0; r < rows; ++r) for (int col = 0; col < cols; ++col) {
            if (at(r, col)) continue;
            auto fits = [&](int w, int h) { for (int i = 0; i < w; ++i) for (int j = 0; j < h; ++j) if (col + i >= cols || r + j >= rows || at(r + j, col + i)) return false; return true; };
            std::vector<std::pair<int, int>> opts;
            for (auto o : { std::pair<int, int>{ 2, 1 }, { 1, 2 }, { 2, 2 }, { 3, 1 } }) if (fits(o.first, o.second)) opts.push_back(o);
            std::pair<int, int> sz = { 1, 1 };
            if (!(rnd() < 0.38 || opts.empty())) sz = opts[size_t(RI(int(opts.size())))];
            for (int i = 0; i < sz.first; ++i) for (int j = 0; j < sz.second; ++j) at(r + j, col + i) = 1;
            Plate pl{ OX + col * C, OY + r * C, sz.first * C, sz.second * C, QPixmap() };
            const QRectF src(pl.x * c.px, pl.y * c.px, pl.w * c.px, pl.h * c.px);
            pl.img = m_desk.copy(src.toAlignedRect());
            pl.img.setDevicePixelRatio(c.px);
            m_plates.push_back(pl);
        }
        m_px = c.px;
    }

    /* ---------- o mecanismo de baixo ---------- */
    Gear newGear(double x, double y, double r, double w)
    {
        Gear g;
        g.x = x; g.y = y; g.r = r; g.w = w;
        g.n = qMax(8, int(std::lround(r / R(2.8, 4.2))));
        static const int profs[5] = { Trap, Trap, Square, Round, Sharp };
        static const int bodies[5] = { Holes, Spokes, Spokes, Groove, Compound };
        g.prof = profs[RI(5)]; g.body = bodies[RI(5)];
        g.tone = R(0.16, 0.34); g.spokes = int(std::floor(R(4, 7)));
        return g;
    }
    static void gearPath(Cv& x, double r, int n, double a, int prof)
    {
        const double th = TAU / n, rr = r * (prof == Sharp ? 0.76 : prof == Square ? 0.84 : 0.8);
        auto P = [](double ang, double rad) { return QPointF(std::cos(ang) * rad, std::sin(ang) * rad); };
        auto L = [&](const QPointF& q) { x.lineTo(q.x(), q.y()); };
        x.begin();
        for (int i = 0; i < n; ++i) {
            const double a0 = a + i * th;
            if (prof == Square) { L(P(a0, rr)); L(P(a0 + th * 0.22, rr)); L(P(a0 + th * 0.22, r)); L(P(a0 + th * 0.72, r)); L(P(a0 + th * 0.72, rr)); }
            else if (prof == Round) { L(P(a0, rr)); L(P(a0 + th * 0.2, rr)); const QPointF c1 = P(a0 + th * 0.45, r * 1.08), e = P(a0 + th * 0.7, rr); x.quadTo(c1.x(), c1.y(), e.x(), e.y()); }
            else if (prof == Sharp) { L(P(a0, rr)); L(P(a0 + th * 0.5, r)); }
            else { L(P(a0, rr)); L(P(a0 + th * 0.18, r)); L(P(a0 + th * 0.5, r)); L(P(a0 + th * 0.68, rr)); }
        }
        x.close();
    }
    static void gearFace(Cv& x, double r, int n, double a, int prof, int body, double tone, int spokes, const QColor& hole)
    {
        const QColor c0 = mixh("#000000", "#8a929c", tone + 0.12), c1 = mixh("#000000", "#8a929c", tone * 0.35);
        gearPath(x, r, n, a, prof);
        x.fillStyle(QBrush(lin(-r, -r, r, r, { { 0, c0 }, { 1, c1 } }))); x.fill();
        x.strokeStyle(rgba("#000000", 0.6)); x.lineWidth(1); x.stroke();
        x.strokeStyle(rgba("#ffffff", 0.05)); x.begin(); x.arc(0, 0, r * 0.74, 0, TAU); x.stroke();
        x.fillStyle(hole);
        if (body == Holes && r > 14) for (int k = 0; k < 5; ++k) { const double ak = a + k * TAU / 5; x.begin(); x.arc(std::cos(ak) * r * 0.5, std::sin(ak) * r * 0.5, r * 0.14, 0, TAU); x.fill(); }
        if (body == Spokes && r > 16) for (int k = 0; k < spokes; ++k) {
            const double a0 = a + k * TAU / spokes, span = TAU / spokes, gap = qMin(0.5, 7 / r);
            x.begin(); x.arc(0, 0, r * 0.66, a0 + gap, a0 + span - gap); x.arc(0, 0, r * 0.32, a0 + span - gap * 1.6, a0 + gap * 1.6, true); x.close(); x.fill();
        }
        if (body == Groove) {
            x.strokeStyle(rgba("#000000", 0.55)); x.lineWidth(2); x.begin(); x.arc(0, 0, r * 0.56, 0, TAU); x.stroke();
            x.strokeStyle(rgba("#000000", 0.35));
            for (int k = 0; k < 8; ++k) { const double ak = a + k * TAU / 8; x.begin(); x.moveTo(std::cos(ak) * r * 0.3, std::sin(ak) * r * 0.3); x.lineTo(std::cos(ak) * r * 0.5, std::sin(ak) * r * 0.5); x.stroke(); }
        }
        x.fillStyle(c1); x.begin(); x.arc(0, 0, r * 0.24, 0, TAU); x.fill();
        x.fillStyle(hole); x.begin(); x.arc(0, 0, r * 0.09, 0, TAU); x.fill();
    }
    void gear(Cv& x, const Gear& g, double a)
    {
        const QColor hole = hex("#050608");
        x.save(); x.translate(g.x, g.y);
        const double k = 1 - g.tilt;
        if (g.tilt > 0) {
            // inclinada: a espessura do disco aparece primeiro, um pouco abaixo, mais escura
            const double th = 3 + g.r * 0.12 * g.tilt;
            x.save(); x.translate(-std::sin(g.phi) * th, std::cos(g.phi) * th); x.rotate(g.phi); x.scale(1, k); x.rotate(-g.phi);
            gearPath(x, g.r, g.n, a, g.prof); x.fillStyle(mixh("#000000", "#8a929c", g.tone * 0.25)); x.fill(); x.restore();
            x.rotate(g.phi); x.scale(1, k); x.rotate(-g.phi);
        }
        gearFace(x, g.r, g.n, a, g.prof, g.body == Compound ? Holes : g.body, g.tone, g.spokes, hole);
        if (g.body == Compound) {   // uma menor, presa no mesmo eixo, por cima
            x.fillStyle(rgba("#000000", 0.45)); x.begin(); x.arc(1.5, 2, g.r * 0.56, 0, TAU); x.fill();
            gearFace(x, g.r * 0.52, qMax(7, int(std::lround(g.n * 0.52))), -a * 0.7 + 0.3, g.prof == Sharp ? Trap : Sharp, Groove, g.tone * 0.8, 4, hole);
        }
        x.restore();
    }
    static QPointF bez(const std::array<double, 8>& c, double u)
    {
        const double v = 1 - u;
        return QPointF(v * v * v * c[0] + 3 * v * v * u * c[2] + 3 * v * u * u * c[4] + u * u * u * c[6],
                       v * v * v * c[1] + 3 * v * v * u * c[3] + 3 * v * u * u * c[5] + u * u * u * c[7]);
    }
    static void rivet(Cv& x, double cx, double cy, double r)
    {
        x.fillStyle(rgba("#000000", 0.45)); x.begin(); x.arc(cx + r * 0.35, cy + r * 0.45, r * 1.1, 0, TAU); x.fill();
        x.fillStyle(QBrush(rad(cx - r * 0.4, cy - r * 0.45, r * 0.1, cx, cy, r * 1.05, { { 0, rgba("#ffffff", 0.75) }, { 0.35, rgba("#aab4be", 0.55) }, { 1, rgba("#101418", 0.85) } })));
        x.begin(); x.arc(cx, cy, r, 0, TAU); x.fill();
    }

    Mech buildMech(const QRectF& bb)
    {
        Mech m;
        m.bb = bb;
        const double w = bb.width(), h = bb.height();
        m.stat = QPixmap(qMax(1, int(std::ceil(w * m_px))), qMax(1, int(std::ceil(h * m_px))));
        m.stat.setDevicePixelRatio(m_px);
        m.stat.fill(Qt::transparent);
        {
            QPainter qp(&m.stat);
            Cv x(qp);
            // a cavidade: fundo escuro com placa traseira, vigas e trilhas de cobre
            x.fillStyle(QBrush(vgrad(0, h, hex("#0d1116"), hex("#05070a")))); x.fillRect(0, 0, w, h);
            {
                const int nw = qMax(1, int(std::lround(w / 3))), nh = qMax(1, int(std::lround(h / 3)));
                QImage n(nw, nh, QImage::Format_RGB32);
                for (int yy = 0; yy < nh; ++yy) { QRgb* d = reinterpret_cast<QRgb*>(n.scanLine(yy)); for (int i = 0; i < nw; ++i) { const int v = qBound(0, int(128 + (rnd() - 0.5) * 30), 255); d[i] = qRgb(v, v, v); } }
                qp.save(); qp.setCompositionMode(QPainter::CompositionMode_Overlay); qp.setOpacity(0.25); qp.setRenderHint(QPainter::SmoothPixmapTransform); qp.drawImage(QRectF(0, 0, w, h), n); qp.restore();
            }
            for (double yy = R(20, 50); yy < h; yy += R(70, 120)) {
                x.fillStyle(hex("#161b21")); x.fillRect(0, yy, w, 12); x.fillStyle(rgba("#8aa0b8", 0.12)); x.fillRect(0, yy, w, 1); x.fillStyle(rgba("#000000", 0.6)); x.fillRect(0, yy + 12, w, 2);
                for (double xx = 16; xx < w; xx += 34) rivet(x, xx, yy + 6, 1.6);
            }
            for (double xx = R(30, 70); xx < w; xx += R(110, 170)) { x.fillStyle(hex("#12161b")); x.fillRect(xx, 0, 10, h); x.fillStyle(rgba("#8aa0b8", 0.1)); x.fillRect(xx, 0, 1, h); }
            x.strokeStyle(rgba("#c87a3a", 0.28)); x.lineWidth(1.2);
            for (int i = 0; i < int(std::lround(w * h / 9000)); ++i) {
                double px = R(0, w), py = R(0, h);
                x.begin(); x.moveTo(px, py);
                for (int k = 0; k < 4; ++k) { if (k % 2) py += R(-50, 50); else px += R(-60, 60); x.lineTo(px, py); }
                x.stroke(); x.fillStyle(rgba("#e8a050", 0.4)); x.begin(); x.arc(px, py, 1.8, 0, TAU); x.fill();
            }
            // cabos (o pulso corre por eles a cada quadro)
            for (int i = 0; i < qMax(2, int(std::lround((w + h) / 160))); ++i) {
                const std::array<double, 8> cc = { R(-20, w * 0.3), R(0, h), R(0, w), R(-40, h + 40), R(0, w), R(-40, h + 40), R(w * 0.7, w + 20), R(0, h) };
                const std::pair<double, QColor> layers[3] = { { 7, hex("#0a0c0f") }, { 5, hex("#1c2128") }, { 1.4, rgba("#6a7a8a", 0.5) } };
                for (const auto& l : layers) { x.strokeStyle(l.second); x.lineWidth(l.first); x.begin(); x.moveTo(cc[0], cc[1]); x.bezTo(cc[2], cc[3], cc[4], cc[5], cc[6], cc[7]); x.stroke(); }
                m.cables.push_back({ cc, rnd(), R(0.25, 0.5) });
            }
        }
        // trem de engrenagens engatadas
        {
            const double r = R(28, 58);
            Gear g0 = newGear(R(r + 6, qMax(r + 7, w - r - 6)), R(r + 6, qMax(r + 7, h - r - 6)), r, 1);
            g0.body = rnd() < 0.5 ? Holes : Spokes;
            m.gears.push_back(g0);
        }
        for (int k = 0; k < 5; ++k) {
            const Gear prev = m.gears[rnd() < 0.7 ? m.gears.size() - 1 : size_t(RI(int(m.gears.size())))];
            for (int t = 0; t < 24; ++t) {
                const double r = rnd() < 0.4 ? R(9, 16) : R(16, 44), a = R(0, 6.28), d = prev.r + r - 3, gx = prev.x + std::cos(a) * d, gy = prev.y + std::sin(a) * d;
                if (gx < r - 10 || gx > w - r + 10 || gy < r - 10 || gy > h - r + 10) continue;
                bool clash = false;
                for (const Gear& q : m.gears) if (!(q.x == prev.x && q.y == prev.y) && std::hypot(q.x - gx, q.y - gy) < q.r + r + 2) { clash = true; break; }
                if (clash) continue;
                Gear g = newGear(gx, gy, r, -prev.w * prev.r / r);
                g.ph = M_PI / g.n;
                m.gears.push_back(g);
                break;
            }
        }
        // engrenagens soltas mais ao fundo, inclinadas em ângulos diferentes (giram no próprio plano)
        for (int i = 0; i < int(std::floor(R(2, 4))); ++i) {
            const double r = R(14, 52);
            Gear g = newGear(R(10, w - 10), R(10, h - 10), r, (rnd() < 0.5 ? 1 : -1) * R(0.3, 1.3));
            g.tilt = R(0.25, 0.68); g.phi = R(0, 6.28); g.tone = R(0.1, 0.22);
            m.back.push_back(g);
        }
        // biela-manivela: o pino na engrenagem grande empurra um pistão
        { const double ga = R(0, 6.28); m.crankG = 0; m.rp = m.gears[0].r * 0.55; m.L = m.gears[0].r * 2.3; m.ax = std::cos(ga); m.ay = std::sin(ga); }
        // cilindros hidráulicos soltos e o núcleo
        for (int i = 0; i < 2; ++i) {
            const double dirs[4] = { 0, M_PI / 2, M_PI, -M_PI / 2 };
            m.cyls.push_back({ R(20, w - 20), R(20, h - 20), dirs[RI(4)] + R(-0.3, 0.3), R(40, 70), R(0, 6.28), R(0.8, 1.6) });
        }
        {
            double bd = -1, bx = w / 2, by = h / 2;
            for (int t = 0; t < 30; ++t) {
                const double cx = R(30, w - 30), cy = R(30, h - 30);
                double d = 1e9;
                for (const Gear& q : m.gears) d = qMin(d, std::hypot(q.x - cx, q.y - cy) - q.r);
                if (d > bd) { bd = d; bx = cx; by = cy; }
            }
            m.coreX = bx; m.coreY = by; m.coreR = qMax(12.0, qMin(26.0, bd - 6));
        }
        for (int i = 0; i < 10; ++i) m.leds.push_back({ R(6, w - 6), R(6, h - 6), pickHex({ "#38d4ff", "#38d4ff", "#ff4a3a", "#ffb02a" }), R(0, 6.28), R(1, 4) });
        return m;
    }

    void drawMech(Cv& x, Mech& m, double open)
    {
        const double T = m_t;
        x.image(m.stat, m.bb.x(), m.bb.y(), m.bb.width(), m.bb.height());
        x.save(); x.translate(m.bb.x(), m.bb.y());
        // pulsos de energia correndo nos cabos
        x.lighter();
        for (const Cable& cb : m.cables) for (int k = 0; k < 2; ++k) {
            const double u = std::fmod(T * cb.sp * (0.5 + open) + cb.off + k * 0.5, 1.0);
            const QPointF q = bez(cb.c, u);
            glowAt(x, q.x(), q.y(), 10, ENER, 0.55);
        }
        x.over();
        // cilindros hidráulicos: o êmbolo entra e sai
        for (const Cyl& cy : m.cyls) {
            const double ext = 0.5 + 0.5 * std::sin(T * cy.sp + cy.ph);
            x.save(); x.translate(cy.x, cy.y); x.rotate(cy.a);
            x.fillStyle(hex("#1a1f26")); x.fillRect(0, -7, cy.len, 14); x.fillStyle(rgba("#9ab0c8", 0.25)); x.fillRect(0, -7, cy.len, 2);
            const double rl = cy.len * (0.4 + 0.6 * ext);
            x.fillStyle(QBrush(lin(0, -3, 0, 3, { { 0, hex("#e8eef4") }, { 0.5, hex("#8a96a2") }, { 1, hex("#3a424a") } }))); x.fillRect(cy.len, -3, rl, 6);
            x.fillStyle(hex("#2a3038")); x.begin(); x.arc(cy.len + rl, 0, 5, 0, TAU); x.fill(); x.fillStyle(hex("#07090c")); x.begin(); x.arc(cy.len + rl, 0, 2, 0, TAU); x.fill();
            x.fillStyle(hex("#2a3038")); x.begin(); x.arc(0, 0, 6, 0, TAU); x.fill();
            x.restore();
        }
        // engrenagens
        for (const Gear& g : m.back) gear(x, g, m.ang * g.w + g.ph);
        for (const Gear& g : m.gears) gear(x, g, m.ang * g.w + g.ph);
        // biela-manivela e o pistão
        {
            const Gear& g = m.gears[size_t(m.crankG)];
            const double th = m.ang * g.w;
            const QPointF pin(g.x + std::cos(th) * m.rp, g.y + std::sin(th) * m.rp);
            const double relx = pin.x() - g.x, rely = pin.y() - g.y, along = relx * m.ax + rely * m.ay, perp = -relx * m.ay + rely * m.ax;
            const double s = along + std::sqrt(qMax(0.0, m.L * m.L - perp * perp));
            const QPointF sl(g.x + m.ax * s, g.y + m.ay * s);
            const double smax = m.rp + m.L, ca = std::atan2(m.ay, m.ax);
            x.save(); x.translate(g.x, g.y); x.rotate(ca);
            x.fillStyle(hex("#151a20")); x.fillRect(smax - m.rp * 2 - 10, -11, m.rp * 2 + 46, 22); x.fillStyle(rgba("#9ab0c8", 0.2)); x.fillRect(smax - m.rp * 2 - 10, -11, m.rp * 2 + 46, 2);
            x.fillStyle(rgba("#000000", 0.5)); x.fillRect(smax - m.rp * 2 - 10, -7, m.rp * 2 + 46, 14);
            x.restore();
            x.strokeStyle(hex("#5a646e")); x.lineWidth(6); x.roundCaps(); x.begin(); x.moveTo(pin.x(), pin.y()); x.lineTo(sl.x(), sl.y()); x.stroke();
            x.strokeStyle(rgba("#ffffff", 0.25)); x.lineWidth(1.2); x.begin(); x.moveTo(pin.x(), pin.y() - 2); x.lineTo(sl.x(), sl.y() - 2); x.stroke();
            x.save(); x.translate(sl.x(), sl.y()); x.rotate(ca); x.fillStyle(QBrush(vgrad(-9, 9, hex("#c8d2dc"), hex("#3a424a")))); x.fillRect(-6, -9, 22, 18); x.restore();
            for (const QPointF& q : { pin, sl }) { x.fillStyle(hex("#2a3038")); x.begin(); x.arc(q.x(), q.y(), 4.5, 0, TAU); x.fill(); x.fillStyle(hex("#c8d2dc")); x.begin(); x.arc(q.x(), q.y(), 1.6, 0, TAU); x.fill(); }
        }
        // o núcleo: brilho azul, anéis girando em sentidos opostos, pulsando
        {
            const double pu = 0.75 + 0.25 * std::sin(T * 3.1), cx = m.coreX, cy = m.coreY, r = m.coreR;
            x.fillStyle(hex("#0a0e14")); x.begin(); x.arc(cx, cy, r + 6, 0, TAU); x.fill(); x.strokeStyle(hex("#3a444e")); x.lineWidth(2); x.stroke();
            x.lighter(); glowAt(x, cx, cy, r * 3.2, ENER, 0.32 * pu * (0.5 + open)); glowAt(x, cx, cy, r * 0.9, hex("#c8f4ff"), 0.9 * pu);
            const double rings[2][3] = { { r * 0.95, 1.6, 3 }, { r * 0.7, -2.4, 4 } };
            for (const auto& rg : rings) {
                x.strokeStyle(rgba(ENER, 0.85)); x.lineWidth(2);
                const int n = int(rg[2]);
                for (int i = 0; i < n; ++i) { const double a0 = T * rg[1] * (0.6 + open) + i * TAU / n; x.begin(); x.arc(cx, cy, rg[0], a0, a0 + TAU / n * 0.6); x.stroke(); }
            }
            x.over();
        }
        // luzinhas
        x.lighter();
        for (const Led& l : m.leds) if (std::sin(T * l.sp + l.ph) > 0.2) { glowAt(x, l.x, l.y, 6, l.c, 0.7); x.fillStyle(l.c); x.fillRect(l.x - 1, l.y - 1, 2, 2); }
        x.over();
        x.restore();
    }

    /* ---------- a coreografia ---------- */
    void startEv(const Ctx& c)
    {
        if (m_ev || m_plates.empty()) return;
        std::vector<QRectF> zones;
        for (const QRectF& z : { c.zones.left, c.zones.right }) if (z.width() >= 60 && z.height() >= 60) zones.push_back(z);
        if (zones.empty()) zones.push_back(QRectF(0, 0, c.W, kH));
        double tot = 0; for (const QRectF& z : zones) tot += z.width() * z.height();
        double r = rnd() * tot; QRectF z = zones[0];
        for (const QRectF& q : zones) { r -= q.width() * q.height(); if (r <= 0) { z = q; break; } }
        const double mx = qMin(70.0, z.width() / 2), my = qMin(70.0, z.height() / 2);
        const double cx = R(z.left() + mx, z.right() - mx), cy = R(z.top() + my, z.bottom() - my);
        std::vector<std::pair<const Plate*, double>> cand;
        auto inZ = [&](const Plate& pl) { const double px = pl.x + pl.w / 2, py = pl.y + pl.h / 2; return px > z.left() - 20 && px < z.right() + 20 && py > z.top() - 20 && py < z.bottom() + 20; };
        for (const Plate& pl : m_plates) if (inZ(pl)) cand.push_back({ &pl, std::hypot(pl.x + pl.w / 2 - cx, pl.y + pl.h / 2 - cy) });
        if (cand.empty()) for (const Plate& pl : m_plates) cand.push_back({ &pl, std::hypot(pl.x + pl.w / 2 - cx, pl.y + pl.h / 2 - cy) });
        std::sort(cand.begin(), cand.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        cand.resize(qMin(cand.size(), size_t(std::floor(R(3, 7)))));
        std::vector<int> bag = { 0, 1, 2, 3, 4, 5 };   // slide, flip, spin, split, shutter, walk
        for (int i = int(bag.size()) - 1; i > 0; --i) std::swap(bag[size_t(i)], bag[size_t(RI(i + 1))]);
        auto ev = std::make_unique<Ev>();
        for (size_t i = 0; i < cand.size(); ++i) {
            const Plate* pl = cand[i].first;
            const double dx = pl->x + pl->w / 2 - cx, dy = pl->y + pl->h / 2 - cy;
            int ddx = 0, ddy = 0;
            if (std::abs(dx) > std::abs(dy)) ddx = dx >= 0 ? 1 : -1; else ddy = dy >= 0 ? 1 : -1;
            ev->act.push_back({ pl, bag[i % bag.size()], ddx, ddy, rnd() < 0.5 ? 1 : -1, i * 0.16 + R(0, 0.08), R(0.9, 1.25) });
        }
        double openT = 0, x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
        for (const Act& a : ev->act) { openT = qMax(openT, a.d + a.dur); x0 = qMin(x0, a.pl->x); y0 = qMin(y0, a.pl->y); x1 = qMax(x1, a.pl->x + a.pl->w); y1 = qMax(y1, a.pl->y + a.pl->h); }
        ev->openT = openT; ev->hold = R(3.8, 5.2);
        {
            Seeded sd(quint32(qHash(QStringLiteral("mech%1,%2,%3").arg(x0).arg(y0).arg(RI(9)))));
            ev->mech = buildMech(QRectF(x0, y0, x1 - x0, y1 - y0));
        }
        m_ev = std::move(ev);
    }

    void backFace(Cv& x, double w, double h)
    {
        x.fillStyle(hex("#1e2329")); x.fillRect(0, 0, w, h);
        x.fillStyle(hex("#2c333b")); for (double yy = 8; yy < h - 4; yy += 14) x.fillRect(4, yy, w - 8, 5);
        x.fillStyle(rgba("#000000", 0.5)); x.fillRect(0, h - 2, w, 2); x.fillStyle(rgba("#8aa0b8", 0.2)); x.fillRect(0, 0, w, 1);
    }
    // um pedaço (sx, sy, sw, sh, em pontos da chapa) da imagem da chapa num retângulo da cena
    void part(Cv& x, const Plate& pl, double sx, double sy, double sw, double sh, double X, double Y, double w, double h)
    {
        x.p.drawPixmap(QRectF(X, Y, w, h), pl.img, QRectF(sx * m_px, sy * m_px, sw * m_px, sh * m_px));
    }
    void drawPlate(Cv& x, const Act& a, double e)
    {
        const Plate& pl = *a.pl;
        const double w = pl.w, h = pl.h;
        const int dx = a.dx, dy = a.dy;
        const double lift = e > 0.001 ? smooth(0, 0.25, e) : 0;
        auto shadow = [&](const std::function<void()>& fn) {
            if (lift <= 0.01) return;
            x.save(); x.translate(7 * lift, 10 * lift); x.alphaG(0.5 * lift); x.fillStyle(hex("#000000")); fn(); x.restore();
        };
        auto thick = [&](double fx, double fy, double fw, double fh) { x.fillStyle(hex("#16191d")); x.fillRect(fx + 2.5 * lift, fy + 3.5 * lift, fw, fh); };
        if (a.type == 0 || a.type == 5) {   // slide / walk
            double ox, oy;
            if (a.type == 0) { ox = dx * w * 0.96 * ease(e); oy = dy * h * 0.96 * ease(e); }
            else {
                const double e1 = ease(qMin(1.0, e * 2)), e2 = ease(qMax(0.0, e * 2 - 1)), px = -dy, py = dx;
                ox = px * w * 0.5 * e1 + dx * w * 0.96 * e2; oy = py * h * 0.5 * e1 + dy * h * 0.96 * e2;
            }
            const double s = 1 + 0.03 * lift, X = pl.x + ox - w * (s - 1) / 2, Y = pl.y + oy - h * (s - 1) / 2;
            shadow([&] { x.fillRect(X, Y, w * s, h * s); }); thick(X, Y, w * s, h * s); x.image(pl.img, X, Y, w * s, h * s);
        } else if (a.type == 2) {           // spin
            const double ang = ease(e) * M_PI / 2 * a.rot, s = 1 - 0.32 * ease(e) + 0.03 * lift;
            const double cx = pl.x + w / 2 + dx * w * 0.6 * ease(e), cy = pl.y + h / 2 + dy * h * 0.6 * ease(e);
            auto put = [&](const std::function<void()>& fn) { x.save(); x.translate(cx, cy); x.rotate(ang); x.scale(s, s); fn(); x.restore(); };
            shadow([&] { put([&] { x.fillRect(-w / 2, -h / 2, w, h); }); });
            put([&] { x.fillStyle(hex("#16191d")); x.fillRect(-w / 2 + 2.5, -h / 2 + 3.5, w, h); x.image(pl.img, -w / 2, -h / 2, w, h); });
        } else if (a.type == 1) {           // flip: dobra na dobradiça do lado de fora e deita por cima da vizinha
            const double th = ease(e) * M_PI * 0.97, cs = std::cos(th);
            const bool horiz = dx != 0;
            const double hx = dx > 0 ? pl.x + w : pl.x, hy = dy > 0 ? pl.y + h : pl.y;
            x.save();
            if (horiz) { x.translate(hx, 0); x.scale(qAbs(cs) < 1e-4 ? 1e-4 : cs, 1); x.translate(-hx, 0); }
            else { x.translate(0, hy); x.scale(1, qAbs(cs) < 1e-4 ? 1e-4 : cs); x.translate(0, -hy); }
            x.save(); x.translate(pl.x, pl.y);
            if (cs >= 0) { x.image(pl.img, 0, 0, w, h); x.fillStyle(rgba("#000000", 0.35 * std::sin(th))); x.fillRect(0, 0, w, h); }
            else { backFace(x, w, h); x.fillStyle(rgba("#ffffff", 0.08 * std::sin(th))); x.fillRect(0, 0, w, h); }
            x.restore();
            x.restore();
            if (e > 0.02 && e < 0.98) { x.fillStyle(hex("#0a0c0f")); if (horiz) x.fillRect(hx - 2, pl.y, 4, h); else x.fillRect(pl.x, hy - 2, w, 4); }
        } else if (a.type == 3) {           // split: parte ao meio, as metades correm pra fora
            const bool horiz = dx != 0;
            const double k = ease(e), off = (horiz ? w : h) * 0.5 * 0.96 * k;
            for (int sg : { -1, 1 }) {
                const double sx = horiz ? (sg < 0 ? 0 : w / 2) : 0, sy = horiz ? 0 : (sg < 0 ? 0 : h / 2), sw = horiz ? w / 2 : w, sh = horiz ? h : h / 2;
                const double X = pl.x + sx + (horiz ? sg * off : 0), Y = pl.y + sy + (horiz ? 0 : sg * off);
                shadow([&] { x.fillRect(X, Y, sw, sh); }); thick(X, Y, sw, sh); part(x, pl, sx, sy, sw, sh, X, Y, sw, sh);
                x.fillStyle(rgba(ENER, 0.5 * std::sin(M_PI * k)));
                if (horiz) x.fillRect(sg < 0 ? X + sw - 1.5 : X, Y, 1.5, sh); else x.fillRect(X, sg < 0 ? Y + sh - 1.5 : Y, sw, 1.5);
            }
        } else {                            // shutter: enrola como persiana pro lado de dentro
            const int n = 6;
            const bool vert = dy != 0 || dx == 0;
            const double k = ease(e), f = 1 - k;
            const bool toEnd = vert ? dy < 0 : dx < 0;
            if (vert) {
                const double sh = h / n;
                for (int i = 0; i < n; ++i) {
                    const double hi = sh * f, y = toEnd ? pl.y + i * hi : pl.y + h - (n - i) * hi;
                    if (hi < 0.3) continue;
                    part(x, pl, 0, i * sh, w, sh, pl.x, y, w, hi); x.fillStyle(rgba("#000000", 0.55 * k)); x.fillRect(pl.x, y + hi - 1.2, w, 1.2);
                }
                const double by = toEnd ? pl.y : pl.y + h - 7;
                x.fillStyle(hex("#20252b")); x.fillRect(pl.x, by, w, 7 * qMin(1.0, k * 3)); x.fillStyle(rgba("#9ab0c8", 0.25)); x.fillRect(pl.x, by, w, 1);
            } else {
                const double sw = w / n;
                for (int i = 0; i < n; ++i) {
                    const double wi = sw * f, xx = toEnd ? pl.x + i * wi : pl.x + w - (n - i) * wi;
                    if (wi < 0.3) continue;
                    part(x, pl, i * sw, 0, sw, h, xx, pl.y, wi, h); x.fillStyle(rgba("#000000", 0.55 * k)); x.fillRect(xx + wi - 1.2, pl.y, 1.2, h);
                }
                const double bx = toEnd ? pl.x : pl.x + w - 7;
                x.fillStyle(hex("#20252b")); x.fillRect(bx, pl.y, 7 * qMin(1.0, k * 3), h); x.fillStyle(rgba("#9ab0c8", 0.25)); x.fillRect(bx, pl.y, 1, h);
            }
        }
    }
    void holeShade(Cv& x, const Plate& pl)
    {
        // a parede do buraco: sombra funda em cima e à esquerda, um fio de luz embaixo
        const double s = 14;
        x.fillStyle(QBrush(vgrad(pl.y, pl.y + s, rgba("#000000", 0.85), rgba("#000000", 0)))); x.fillRect(pl.x, pl.y, pl.w, s);
        x.fillStyle(QBrush(lin(pl.x, 0, pl.x + s, 0, { { 0, rgba("#000000", 0.7) }, { 1, rgba("#000000", 0) } }))); x.fillRect(pl.x, pl.y, s, pl.h);
        x.fillStyle(QBrush(lin(pl.x + pl.w, 0, pl.x + pl.w - s * 0.6, 0, { { 0, rgba("#000000", 0.5) }, { 1, rgba("#000000", 0) } }))); x.fillRect(pl.x + pl.w - s, pl.y, s, pl.h);
        x.fillStyle(rgba("#9ab0c8", 0.18)); x.fillRect(pl.x, pl.y + pl.h - 1, pl.w, 1);
    }

    double m_t = 0, m_nextEv = 3.2, m_px = 1;
    QString m_key;
    QPixmap m_desk;
    std::vector<Plate> m_plates;
    std::unique_ptr<Ev> m_ev;
};

/* ===================================================================================================================
   Relay (inspirado em Mass Effect, 23–24/11): o relé de massa em 3D
   =================================================================================================================== */
// Portado de anim-lab/relay.js (v11). O relé é um objeto 3D de verdade, montado em quadriláteros (a cabeça em meia-lua,
// os dois braços que engordam na base e afinam até a ponta, o friso, as antenas), projetado por uma câmera em
// perspectiva e iluminado (luz fria de cima, a luz azul do núcleo, ambiente e brilho de metal), pintado do fundo pra
// frente numa imagem pronta. Visto de lado: os braços vêm da esquerda e o núcleo fica na boca da meia-lua, antes da
// folha. Por quadro: as janelinhas (risquinhos em fileiras), o núcleo com os anéis em 3D, o reflexo de lente, a poeira
// e o salto (a nave para atrás do núcleo, o relé carrega e ela vira um risco de luz pro fundo; às vezes o contrário).
class Relay : public Layer {
public:
    void init(Ctx& c) override
    {
        m_key.clear(); m_hasEv = false;
        m_dust.assign(size_t(qMax(1, int(std::lround(60 * INT(c) * c.W / 1440.0)))), Dust());
        for (Dust& d : m_dust) { d.x = R(0, c.W); d.y = R(0, kH); d.v = R(4, 14); d.a = R(0.1, 0.4); }
    }

    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        if (!m_hasEv) { m_nextEv -= dt; if (m_nextEv < 0) { m_ev = Ev(); m_ev.out = rnd() < 0.6; m_hasEv = true; } }
        else { m_ev.t += dt; double k; if (stage(m_ev, k) == End) { m_hasEv = false; m_nextEv = R(14, 26); } }
        for (Dust& d : m_dust) { d.x -= d.v * dt; if (d.x < -4) d.x = c.W + 4; }
    }

    void paint(QPainter& painter, const Pal& pal, Ctx& c) override
    {
        const QString key = QString::number(zx(c, 45), 'f', 1) + QLatin1Char('|') + QString::number(c.W, 'f', 0) + QLatin1Char('|') + QString::number(c.px, 'f', 2) + pal.app.name();
        if (key != m_key) { build(c); m_key = key; }
        const double T = m_t;
        painter.save();
        Cv x(painter);
        x.image(m_back, 0, 0);
        double charge = 0, k = 0;
        St ph = End;
        if (m_hasEv) {
            ph = stage(m_ev, k);
            charge = ph == Chg ? ease(k) : (ph == Jump || ph == Arrive) ? 1 - k * 0.6 : 0;
        }
        const P3 cp = proj(COREP);
        x.lighter();
        // as janelinhas
        for (const Win& l : m_wins) {
            const double a = qMin(1.0, l.a * (0.7 + 0.3 * std::sin(T * l.sp + l.ph)) * (1 + charge * 0.6));
            painter.setPen(QPen(rgba(l.c, a), l.r, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(l.x, l.y), QPointF(l.x2, l.y2));
            if (l.r > 1.6) glowAt(x, (l.x + l.x2) / 2, (l.y + l.y2) / 2, l.r * 3, l.c, a * 0.25);
        }
        // o corredor acendendo na carga
        if (charge > 0.01) for (int i = 0; i < 30; ++i) {
            const double t = i / 30.0;
            const P3 q = proj(V3{ -1.0 + t * (ARM_L - 0.5), 0, 0 });
            glowAt(x, q.x, q.y, 420 / q.z * (0.6 + 0.4 * charge), CORE, 0.12 * charge * (1 - t * 0.6));
        }
        // os anéis em 3D (dois giroscópios): a metade de trás antes do núcleo, a da frente depois
        const double spin = 1 + charge * 4;
        std::vector<P3> rings[2];
        const double ringDef[2][4] = { { 0.95, 0, 0.5, 2.4 }, { 0.74, 1, -0.8, 1.9 } };
        for (int r = 0; r < 2; ++r) {
            const double rr = ringDef[r][0], a0 = T * ringDef[r][2] * spin, cs = std::cos(a0), sn = std::sin(a0);
            const bool ax0 = ringDef[r][1] == 0;
            for (int i = 0; i <= 64; ++i) {
                const double t = i / 64.0 * TAU;
                V3 q = ax0 ? V3{ 0, std::cos(t) * rr, std::sin(t) * rr } : V3{ std::cos(t) * rr, std::sin(t) * rr, 0 };
                q = ax0 ? V3{ q.x * cs - q.z * sn, q.y, q.x * sn + q.z * cs } : V3{ q.x, q.y * cs - q.z * sn, q.y * sn + q.z * cs };
                rings[r].push_back(proj(add(COREP, q)));
            }
        }
        auto ringPass = [&](bool front) {
            for (int r = 0; r < 2; ++r) {
                const double w = ringDef[r][3];
                painter.setPen(QPen(rgba("#9ad8ff", front ? 0.8 : 0.3), front ? w : w * 0.7, Qt::SolidLine, Qt::RoundCap));
                for (int i = 0; i < 64; ++i) {
                    const P3& a = rings[r][size_t(i)]; const P3& b = rings[r][size_t(i + 1)];
                    if (((a.z + b.z) / 2 < cp.z) != front) continue;
                    painter.drawLine(QPointF(a.x, a.y), QPointF(b.x, b.y));
                }
            }
        };
        ringPass(false);
        const double pulse = 0.85 + 0.15 * std::sin(T * 2.3), cr = 24 * (1 + 0.35 * charge);
        glowAt(x, cp.x, cp.y, 220 * (1 + 0.4 * charge), CORE, 0.3 * pulse + 0.25 * charge);
        glowAt(x, cp.x, cp.y, cr * 2.2, "#8fd0ff", 0.75); glowAt(x, cp.x, cp.y, cr, HOT, 1);
        x.save(); x.translate(cp.x, cp.y); x.scale(1, 0.035); glowAt(x, 0, 0, 520 * (1 + 0.5 * charge), "#9ad0ff", 0.55 + 0.3 * charge); x.restore();
        ringPass(true);
        x.over();
        for (const Dust& d : m_dust) { x.fillStyle(rgba("#cfe0ff", d.a)); x.fillRect(d.x, d.y, 1.2, 1.2); }
        if (m_hasEv) {
            auto angOf = [&](const V3& a, const V3& b) { const P3 pa = proj(a), pb = proj(b); return std::atan2(pb.y - pa.y, pb.x - pa.x); };
            auto sOf = [&](const V3& q) { return qMax(0.15, 4.2 / proj(q).z); };
            if (ph == Appr) {
                const V3 q = lerp3(P_FAR, P_STOP, ease(k)); const P3 s = proj(q);
                ship(x, s.x, s.y, angOf(q, k > 0.8 ? axisAt(0.2) : P_STOP), sOf(q), 1);
            } else if (ph == Chg && m_ev.out) {
                const P3 s = proj(P_STOP);
                ship(x, s.x + std::sin(T * 9) * 0.4 * k, s.y, angOf(P_STOP, axisAt(0.2)), sOf(P_STOP), 1);
                x.lighter(); glowAt(x, s.x, s.y, 40 + 30 * k, CORE, 0.5 * k); x.over();
            } else if (ph == Jump) {
                const double e = std::pow(k, 2.2);
                x.lighter();
                const P3 h = streak(x, lerp3(P_STOP, axisAt(1), qMax(0.0, e - 0.2 - 0.25 * (1 - k))), lerp3(P_STOP, axisAt(1), e));
                glowAt(x, h.x, h.y, 160 / h.z + 4, HOT, 0.9);
                if (k < 0.25) glowAt(x, cp.x, cp.y, 400, "#cfe8ff", 0.6 * (1 - k / 0.25));
                x.over();
            } else if (ph == Arrive) {
                const double e = std::pow(k, 0.6);
                x.lighter();
                streak(x, lerp3(axisAt(1), P_STOP, qMax(0.0, e - 0.3)), lerp3(axisAt(1), P_STOP, e));
                if (k > 0.8) glowAt(x, cp.x, cp.y, 420, "#cfe8ff", 0.7 * (k - 0.8) / 0.2);
                x.over();
                if (k > 0.85) { const P3 s = proj(P_STOP); ship(x, s.x, s.y, angOf(P_STOP, P_FAR), sOf(P_STOP), 1.4); }
            } else if (ph == Leave) {
                const V3 q = lerp3(P_STOP, P_FAR, ease(k)); const P3 s = proj(q);
                ship(x, s.x, s.y, angOf(P_STOP, P_FAR), sOf(q), 1);
            }
        }
        painter.restore();
    }

private:
    struct V3 { double x, y, z; };
    struct P3 { double x, y, z; };
    struct Face { V3 p[4]; int mat; };
    struct Win { double x, y, x2, y2, r; QColor c; double ph, sp, a; };
    struct Dust { double x = 0, y = 0, v = 0, a = 0; };
    enum St { Appr, Chg, Jump, Rest, Arrive, Leave, End };
    struct Ev { bool out = true; double t = 0, appr = 4.2, chg = 2.2, jump = 0.8, rest = 1.2; };
    enum { Hull, Rib, Inner, Gap };

    static constexpr double RR = 1.6, ARM_L = 10.5;
    const QColor CORE = hex("#5ab4ff"), HOT = hex("#e4f4ff");
    const V3 COREP{ 0.25, 0, -0.9 }, P_FAR{ -2.6, -2.6, -5.5 }, P_STOP{ -1.2, 0, -0.9 };

    static V3 add(const V3& a, const V3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    static V3 sub(const V3& a, const V3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    static V3 mul(const V3& a, double k) { return { a.x * k, a.y * k, a.z * k }; }
    static double dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    static V3 cross(const V3& a, const V3& b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
    static V3 norm(const V3& a) { const double l = std::sqrt(dot(a, a)); return l > 0 ? mul(a, 1 / l) : a; }
    static V3 lerp3(const V3& a, const V3& b, double k) { return { lerp(a.x, b.x, k), lerp(a.y, b.y, k), lerp(a.z, b.z, k) }; }
    static double taper(double a, double b, double k) { return b + (a - b) * std::pow(qMax(0.0, 1 - k), 1.35); }
    static double ease(double u) { u = qBound(0.0, u, 1.0); return u < 0.5 ? 4 * u * u * u : 1 - std::pow(-2 * u + 2, 3) / 2; }
    static V3 axisAt(double t) { return { -1.2 - t * 60, 0, -0.9 }; }

    static St stage(const Ev& e, double& k)
    {
        const double a = e.appr, c = e.chg, j = e.jump;
        k = 0;
        if (e.out) {
            if (e.t < a) { k = e.t / a; return Appr; }
            if (e.t < a + c) { k = (e.t - a) / c; return Chg; }
            if (e.t < a + c + j) { k = (e.t - a - c) / j; return Jump; }
            return e.t < a + c + j + e.rest ? Rest : End;
        }
        if (e.t < c) { k = e.t / c; return Chg; }
        if (e.t < c + j) { k = (e.t - c) / j; return Arrive; }
        if (e.t < c + j + a) { k = (e.t - c - j) / a; return Leave; }
        return End;
    }

    // câmera
    V3 m_eye{}, m_f{}, m_r{}, m_u{};
    double m_s = 1, m_cx = 0, m_cy = 0;
    void makeCam(const V3& eye, const V3& target)
    {
        m_eye = eye; m_f = norm(sub(target, eye)); m_r = norm(cross(m_f, V3{ 0, 1, 0 })); m_u = cross(m_r, m_f);
    }
    P3 proj(const V3& p) const
    {
        const V3 d = sub(p, m_eye); const double z = dot(d, m_f);
        return { m_cx + dot(d, m_r) / z * m_s, m_cy - dot(d, m_u) / z * m_s, z };
    }

    // malha
    std::vector<Face> m_faces;
    void quad(const V3& a, const V3& b, const V3& c, const V3& d, int mat) { m_faces.push_back({ { a, b, c, d }, mat }); }
    void beam(double x0, double x1, const std::function<double(double)>& Y, double h0, double w0, double h1, double w1, int mat, int segs)
    {
        auto box = [&](double k, double sc, V3 out[4]) {
            const double xx = lerp(x0, x1, k), h = taper(h0, h1, k) * sc, w = taper(w0, w1, k) * sc, y = Y(k);
            out[0] = { xx, y - h, -w }; out[1] = { xx, y + h, -w }; out[2] = { xx, y + h, w }; out[3] = { xx, y - h, w };
        };
        V3 A[4], B[4];
        for (int i = 0; i < segs; ++i) {
            for (int j = 0; j < 4; ++j) {
                box((i + j * 0.965 / 4) / segs, 1, A); box((i + (j + 1) * 0.965 / 4) / segs, 1, B);
                for (int q = 0; q < 4; ++q) quad(A[q], B[q], B[(q + 1) % 4], A[(q + 1) % 4], mat);
            }
            box((i + 0.965) / segs, 0.93, A); box((i + 1.0) / segs, 0.93, B);
            for (int q = 0; q < 4; ++q) quad(A[q], B[q], B[(q + 1) % 4], A[(q + 1) % 4], Gap);
        }
        box(1, 1, A); quad(A[0], A[1], A[2], A[3], mat);
    }
    void ushape()
    {
        const double a0 = -M_PI * 0.5, a1 = M_PI * 0.5, rin = RR - 0.42, rout = RR + 0.72, dz = 0.9;
        const int n = 56;
        auto P = [](double a, double r, double z) { return V3{ std::cos(a) * r * 0.82 + 0.25, std::sin(a) * r, z }; };
        for (int i = 0; i < n; ++i) {
            const double a = a0 + (a1 - a0) * i / n, b = a0 + (a1 - a0) * (i + 1) / n;
            const int mat = i % 6 == 0 ? Rib : Hull;
            quad(P(a, rout, -dz), P(b, rout, -dz), P(b, rout, dz), P(a, rout, dz), mat);
            quad(P(a, rin, dz), P(b, rin, dz), P(b, rin, -dz), P(a, rin, -dz), Inner);
            quad(P(a, rin, dz), P(a, rout, dz), P(b, rout, dz), P(b, rin, dz), mat);
            quad(P(a, rout, -dz), P(a, rin, -dz), P(b, rin, -dz), P(b, rout, -dz), mat);
            quad(P(a, rin, dz * 0.7), P(b, rin, dz * 0.7), P(b, rin * 0.3, dz * 0.7), P(a, rin * 0.3, dz * 0.7), Inner);
        }
    }
    void antenna(double ax, double az, double h, double lean)
    {
        const double w = 0.035, y0 = RR + 0.25;
        const V3 A[4] = { { ax - w, y0, az - w }, { ax + w, y0, az - w }, { ax + w, y0, az + w }, { ax - w, y0, az + w } };
        V3 B[4];
        for (int q = 0; q < 4; ++q) B[q] = { A[q].x + lean, A[q].y + h, A[q].z };
        for (int q = 0; q < 4; ++q) quad(A[q], B[q], B[(q + 1) % 4], A[(q + 1) % 4], Hull);
    }
    void buildMesh()
    {
        m_faces.clear();
        ushape();
        for (int sg : { -1, 1 }) {
            beam(0.15, ARM_L, [sg](double) { return sg * RR; }, 0.62, 0.8, 0.07, 0.1, Hull, 9);
            beam(0.5, ARM_L - 0.6, [sg](double k) { return sg * (RR + taper(0.62, 0.07, lerp(0.5, ARM_L - 0.6, k) / ARM_L) + 0.03); }, 0.035, 0.22, 0.02, 0.04, Rib, 6);
        }
        antenna(0.45, -0.12, 2.6, 0.3); antenna(0.8, 0.14, 1.9, 0.2);
    }

    QColor shade(const Face& f) const
    {
        static const double MAT[4][3] = { { 0.34, 0.38, 0.47 }, { 0.22, 0.25, 0.32 }, { 0.14, 0.18, 0.26 }, { 0.06, 0.07, 0.1 } };
        const V3 KEY = norm(V3{ -0.4, 0.85, 0.35 });
        const V3 ctr = mul(add(add(f.p[0], f.p[1]), add(f.p[2], f.p[3])), 0.25), toEye = norm(sub(m_eye, ctr));
        V3 n = norm(cross(sub(f.p[1], f.p[0]), sub(f.p[2], f.p[0])));
        if (dot(n, toEye) < 0) n = mul(n, -1);
        const double* base = MAT[f.mat];
        const double amb = 0.22, kd = qMax(0.0, dot(n, KEY));
        const V3 h = norm(add(KEY, toEye));
        const double sp = std::pow(qMax(0.0, dot(n, h)), 22) * (f.mat == Gap ? 0.1 : 0.95);
        const V3 lc = sub(COREP, ctr);
        const double dl = std::sqrt(dot(lc, lc)), kc = qMax(0.0, dot(n, norm(lc))) * 1.3 / (1 + dl * dl * 0.9);
        const double spc[3] = { 0.75, 0.82, 0.95 }, cc[3] = { 0.2, 0.5, 1.0 };
        int out[3];
        for (int i = 0; i < 3; ++i) {
            const double v = base[i] * (amb + kd * 0.95) + sp * spc[i] + kc * cc[i] * (f.mat == Inner ? 0.85 : 0.7);
            out[i] = qBound(0, int(std::lround(255 * qMin(1.0, v))), 255);
        }
        return QColor(out[0], out[1], out[2]);
    }

    void build(const Ctx& c)
    {
        Seeded sd(0x2E1A7u);
        m_wins.clear();
        const double W = c.W, ox = zx(c, 45) - 45;
        makeCam(V3{ ARM_L + 2.5, 3.6, -7.0 }, V3{ 1.6, 0.0, 0.0 });
        buildMesh();
        {   // enquadra pela cabeça: a meia-lua com 400 de altura, encostando antes da folha; os braços saem pela esquerda
            m_s = 1; m_cx = 0; m_cy = 0;
            double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
            for (const Face& f : m_faces) for (const V3& q : f.p) if (q.x < 1.8) { const P3 p = proj(q); x0 = qMin(x0, p.x); y0 = qMin(y0, p.y); x1 = qMax(x1, p.x); y1 = qMax(y1, p.y); }
            m_s = 400 / (y1 - y0); m_cx = 345 + ox - x1 * m_s; m_cy = 330 - (y0 + y1) / 2 * m_s;
        }
        m_back = layer(c, W, kH);
        QPainter qp(&m_back);
        Cv x(qp);
        x.fillStyle(hex("#02030a")); x.fillRect(0, 0, W, kH);
        {
            const char* nc[3] = { "#1a2a6a", "#3a1a5a", "#0a3a5a" }; const double na[3] = { 0.5, 0.4, 0.35 };
            QPixmap dots[3] = { softDot(hex(nc[0]), 0.2), softDot(hex(nc[1]), 0.2), softDot(hex(nc[2]), 0.2) };
            for (int i = 0; i < int(26 * W / 1440.0) + 4; ++i) {
                const int q = RI(3); const double w = R(260, 700);
                x.alphaG(na[q] * R(0.3, 0.8)); x.image(dots[q], R(-200, W + 100) - w / 2, R(-100, kH) - w * 0.35, w, w * 0.7);
            }
            x.alphaG(1);
        }
        for (int i = 0; i < int(900 * W / 1440.0); ++i) {
            const double s = std::pow(rnd(), 3) * 1.8 + 0.3;
            x.fillStyle(rgba(pickHex({ "#ffffff", "#cfe0ff", "#ffe8d0" }), R(0.25, 0.95))); x.fillRect(R(0, W), R(0, kH), s, s);
        }
        {   // o planeta lá longe, do outro lado da folha
            const double px = W - 180, py = 230, pr = 120;
            x.fillStyle(QBrush(rad(px - pr * 0.45, py - pr * 0.3, pr * 0.1, px, py, pr, { { 0, hex("#4a6a8a") }, { 0.55, hex("#1a2a3e") }, { 1, hex("#05080e") } })));
            x.begin(); x.arc(px, py, pr, 0, TAU); x.fill();
            x.strokeStyle(rgba("#7ab4ff", 0.35)); x.lineWidth(3); x.begin(); x.arc(px, py, pr + 1, M_PI * 0.85, M_PI * 1.6); x.stroke();
            x.fillStyle(rgba("#02030a", 0.55)); x.begin(); x.arc(px + pr * 0.35, py + pr * 0.2, pr * 0.95, 0, TAU); x.fill();
        }
        const P3 cp = proj(COREP);
        x.fillStyle(QBrush(rad(cp.x, cp.y, 10, cp.x, cp.y, 480, { { 0, rgba(CORE, 0.2) }, { 1, rgba(CORE, 0) } }))); x.fillRect(0, 0, W, kH);
        // as faces, do fundo pra frente
        std::vector<std::pair<double, const Face*>> list;
        list.reserve(m_faces.size());
        for (const Face& f : m_faces) list.push_back({ (proj(f.p[0]).z + proj(f.p[1]).z + proj(f.p[2]).z + proj(f.p[3]).z) / 4, &f });
        std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        x.roundCaps();
        for (const auto& it : list) {
            const QColor col = shade(*it.second);
            x.fillStyle(col); x.strokeStyle(col); x.lineWidth(0.6);
            x.begin();
            for (int i = 0; i < 4; ++i) { const P3 q = proj(it.second->p[i]); if (i) x.lineTo(q.x, q.y); else x.moveTo(q.x, q.y); }
            x.close(); x.fill(); x.stroke();
        }
        // as janelinhas: fileiras contínuas em todas as faces que a câmera vê, cada janela um risquinho de luz
        auto win = [&](const V3& p3, const V3& q3, const V3& nrm, double w) {
            if (dot(sub(m_eye, p3), nrm) <= 0) return;
            const P3 a = proj(p3), b = proj(q3);
            const bool warm = rnd() < 0.1;
            m_wins.push_back({ a.x, a.y, b.x, b.y, qMax(0.7, w / a.z), warm ? hex("#ffc078") : pickHex({ "#e4f2ff", "#d0e8ff", "#bcdcff" }), R(0, 6.28), R(0.2, 0.9), 0.55 + 0.45 * rnd() });
        };
        auto row = [&](const std::function<void(double)>& f, double dens) {
            bool on = rnd() < 0.5 * dens; double left = R(0.2, 1.4);
            for (double t = 0.35; t < ARM_L - 0.25; t += 0.085) {
                left -= 0.085;
                if (left < 0) { on = !on; left = on ? R(0.2, 1.0) * dens : R(0.4, 1.8) / dens; }
                if (on && rnd() < 0.8) f(t);
            }
        };
        for (int sg : { -1, 1 }) {
            for (int zs : { 1, -1 }) for (double r = -0.6; r <= 0.61; r += 0.4) row([&](double t) {
                const double kk = t / ARM_L, hh = taper(0.62, 0.07, kk), ww = taper(0.8, 0.1, kk), k2 = (t + 0.05) / ARM_L;
                win(V3{ t, sg * RR + r * hh, zs * ww * 1.004 }, V3{ t + 0.05, sg * RR + r * taper(0.62, 0.07, k2), zs * taper(0.8, 0.1, k2) * 1.004 }, V3{ 0, 0, double(zs) }, 7);
            }, 0.8);
            for (int side : { -1, 1 }) row([&](double t) {
                const double kk = t / ARM_L, hh = taper(0.62, 0.07, kk), k2 = (t + 0.05) / ARM_L;
                win(V3{ t, sg * RR + side * hh * 1.004, 0 }, V3{ t + 0.05, sg * RR + side * taper(0.62, 0.07, k2) * 1.004, 0 }, V3{ 0, double(side), 0 }, 6);
            }, 0.3);
        }
        for (double rr = RR - 0.2; rr < RR + 0.6; rr += 0.32) {
            bool on = true; double left = R(0.1, 0.5);
            for (double a = -M_PI * 0.5; a <= M_PI * 0.5; a += 0.04) {
                left -= 0.04;
                if (left < 0) { on = !on; left = on ? R(0.2, 0.7) : R(0.05, 0.3); }
                if (on) win(V3{ std::cos(a) * rr * 0.82 + 0.25, std::sin(a) * rr, -0.905 }, V3{ std::cos(a + 0.025) * rr * 0.82 + 0.25, std::sin(a + 0.025) * rr, -0.905 }, V3{ 0, 0, -1 }, 8);
            }
        }
    }

    void ship(Cv& x, double sx, double sy, double ang, double s, double glow)
    {
        x.save(); x.translate(sx, sy); x.rotate(ang); x.scale(s, s);
        x.fillStyle(hex("#0e1420")); x.begin(); x.moveTo(18, 0); x.lineTo(-8, -6); x.lineTo(-14, -3); x.lineTo(-14, 3); x.lineTo(-8, 6); x.close(); x.fill();
        x.fillStyle(hex("#3a4a62")); x.begin(); x.moveTo(18, 0); x.lineTo(-8, -6); x.lineTo(-4, -1); x.close(); x.fill();
        x.fillStyle(hex("#0e1420")); x.fillRect(-10, -11, 9, 3); x.fillRect(-10, 8, 9, 3);
        x.restore();
        x.lighter();
        const double ex = sx - std::cos(ang) * 15 * s, ey = sy - std::sin(ang) * 15 * s;
        glowAt(x, ex, ey, 14 * s * glow, "#ffb060", 0.8); glowAt(x, ex, ey, 4 * s, "#fff0d8", 1);
        x.over();
    }
    P3 streak(Cv& x, const V3& a3, const V3& b3)
    {
        const P3 t = proj(a3), h = proj(b3);
        x.strokeStyle(QBrush(lin(t.x, t.y, h.x, h.y, { { 0, rgba(CORE, 0) }, { 1, rgba(HOT, 0.95) } })));
        x.lineWidth(qMax(1.0, 30 / h.z)); x.roundCaps(); x.line(t.x, t.y, h.x, h.y);
        return h;
    }

    double m_t = 0, m_nextEv = 6;
    QString m_key;
    QPixmap m_back;
    std::vector<Win> m_wins;
    std::vector<Dust> m_dust;
    bool m_hasEv = false;
    Ev m_ev;
};

} // namespace

std::unique_ptr<Layer> makeLeva9(const QString& scene)
{
    if (scene == QLatin1String("echelon")) return std::make_unique<Echelon>();
    if (scene == QLatin1String("bullettime")) return std::make_unique<BulletTime>();
    if (scene == QLatin1String("shapeshift")) return std::make_unique<Shapeshift>();
    if (scene == QLatin1String("relay")) return std::make_unique<Relay>();
    return nullptr;
}

} // namespace AnimBg
