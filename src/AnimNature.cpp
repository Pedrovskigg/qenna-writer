// Temas Animados de natureza (rework de 15/10): Lighthouse, Moor Fog, Firefly Meadow e Stargazer.
// Portados de lighthouse.js, moor.js, meadow.js e stargazer.js do Weather Lab. O cenário parado
// de cada um é sorteado sempre com a mesma semente (na Hora real só a luz muda) e refeito a cada
// 15 min de relógio; o que se mexe é desenhado a cada quadro.

#include "AnimKit.h"

#include <QImage>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace AnimBg {
namespace {

using namespace kit;

QString todKey(const Ctx& c)
{
    return (c.tod == QLatin1String("real") ? QStringLiteral("real") + QString::number(int(std::floor(c.hour * 4))) : c.tod) + QLatin1Char('#') + QString::number(c.px, 'f', 2);
}

// gradiente horizontal com o lado da luz (usado em torre, casa, pedra)
QLinearGradient shadeH(const QColor& c0, double x0, double w, const QColor& rim, double lit = 0.12, double d0 = 0.45, double d1 = 0.25)
{
    QLinearGradient g(x0, 0, x0 + w, 0);
    g.setColorAt(0, mix(c0, Qt::black, d0));
    g.setColorAt(0.62, mix(c0, rim, lit));
    g.setColorAt(1, mix(c0, Qt::black, d1));
    return g;
}

// ============================================================ farol
// O farol de verdade num penhasco de rocha (costa irregular), a casa do faroleiro, o mar com ondas
// em perspectiva e o caminho da lua, ondas quebrando no pé da rocha com borrifo; o facho girando
// (dois fachos opostos, em cone, sem salto na virada), a maresia acendendo dentro dele.
class Lighthouse2 : public Layer {
public:
    void init(Ctx& c) override
    {
        m_back.reset();
        m_key.clear();
        m_stars = QPixmap();
        m_sprays.clear();
        m_smoke.clear();
        makeRows(c);
        m_motes.clear();
        for (int i = 0; i < 140; ++i) m_motes.push_back({ R(-200, c.W), R(ly(c) - 170, ly(c) + 170), R(-6, 6), R(0, 6.28), R(0.6, 1.6) });
        m_mist.clear();
        for (int i = 0; i < 22; ++i) m_mist.push_back({ R(-100, c.W), R(sea(c) - 70, sea(c) + 30), R(60, 160), R(4, 12), R(0.05, 0.12) });
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt * (0.7 + 0.3 * INT(c));
        m_nextSpray -= dt;
        if (m_nextSpray <= 0 && !m_edge.empty()) {
            const double by = sea(c) + R(60, 170), bx = edgeX(by) + R(2, 12);
            for (int i = 0; i < 26; ++i) m_sprays.push_back({ bx + R(-14, 14), by, R(-40, 30), -R(60, 160), 0, R(0.9, 1.8), R(1.2, 3) });
            m_nextSpray = R(2.5, 5) / INT(c);
        }
        for (Spray& s : m_sprays) { s.t += dt; s.vy += 220 * dt; s.x += s.vx * dt; s.y += s.vy * dt; }
        m_sprays.erase(std::remove_if(m_sprays.begin(), m_sprays.end(), [](const Spray& s) { return s.t >= s.life; }), m_sprays.end());
        for (Mist& m : m_mist) { m.x += m.v * dt; if (m.x - m.r > c.W) m.x = -m.r; }
        for (Mote& m : m_motes) { m.x += (m.v + 4) * dt; m.y += std::sin(m_t * 0.7 + m.ph) * 3 * dt; if (m.x > c.W + 200) m.x = -200; }
        // fumaça da chaminé, puxada pelo vento pra direita
        if (m_hasDyn && rnd() < dt * 10) m_smoke.push_back({ m_chim.x() + R(-1, 1), m_chim.y(), 0, R(3, 5) });
        for (Smoke& s : m_smoke) { s.t += dt; s.y -= 9 * dt; s.x += (5 + s.t * 3) * dt; }
        m_smoke.erase(std::remove_if(m_smoke.begin(), m_smoke.end(), [](const Smoke& s) { return s.t >= s.life; }), m_smoke.end());
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const QString key = todKey(c);
        if (key != m_key || !m_back.valid(c)) {
            Seeded sd(0x11687u);
            build(pal, c);
            m_key = key;
        }
        const double L = m_L, night = 1 - L, lx = lxOf(c), LY = ly(c), SEA = sea(c);
        const double MX = rx(c, 1180), MY = 140;
        p.setPen(Qt::NoPen);
        skyTOD(p, pal, c, mix(pal.app, Qt::black, 0.3), mix(pal.app, hex("#1c3348"), 0.3));
        // estrelas e lua (somem com o dia)
        if (m_stars.isNull() || qAbs(m_starsPx - c.px) > 0.01) {
            m_stars = layer(c, c.W, c.H);
            m_starsPx = c.px;
            QPainter x(&m_stars);
            for (int i = 0; i < 220; ++i) x.fillRect(QRectF(R(0, c.W), R(0, SEA * 0.9), R(0.8, 1.6), R(0.8, 1.6)), alpha(Qt::white, R(0.15, 0.6)));
        }
        p.setOpacity(night);
        blit(p, m_stars, 0, 0, c.W, c.H);
        p.setOpacity(1);
        const double moonA = night * 0.95;
        if (moonA > 0.02) {
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            glow(p, MX, MY, 120, hex("#e8eef8"), 0.25 * moonA);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            disc(p, MX, MY, 18, alpha(hex("#f0f2f6"), moonA));
            disc(p, MX - 5, MY - 3, 4, alpha(Qt::black, 0.08 * moonA));
            disc(p, MX + 6, MY + 5, 3, alpha(Qt::black, 0.08 * moonA));
        }
        // o mar: claro e enevoado no horizonte, escuro perto
        vfill(p, QRectF(0, SEA, c.W, c.H - SEA), mix(mix(pal.app, todPick(c, hex("#8ab0cc"), hex("#a87a80"), hex("#1c2c3c")), 0.55), Qt::white, 0.04),
              mix(mix(pal.app, todPick(c, hex("#2a5a7a"), hex("#3a2a3a"), hex("#060c14")), 0.5), Qt::black, 0.25), SEA, c.H);
        // os dois fachos da lente: perto da virada o facho encurta até zero e some (o lado só troca invisível);
        // força e largura acompanham continuamente o quanto ele vira pra gente
        const double th = m_t * 0.9, beamA = qMax(0.0, 1 - 1.3 * L);
        const Beam B1 = beamGeo(th), B2 = beamGeo(th + M_PI);
        auto inBeam = [&](double px, double py, const Beam& B) {
            const double d = (px - lx) * B.dir;
            if (d <= 0 || d > B.len) return 0.0;
            const double hw = 3 + B.half * d / B.len, pp = std::abs(py - LY);
            return pp > hw ? 0.0 : (1 - pp / hw) * (1 - d / B.len) * B.k;
        };
        const double bx1 = lx + B1.dir * qMin(B1.len, 900.0) * 0.65, bx2 = lx + B2.dir * qMin(B2.len, 900.0) * 0.65;
        const QColor beamCol = mix(pal.acc, Qt::white, 0.68), glint = todPick(c, hex("#fff2d0"), hex("#ffc080"), hex("#dfe8f8")), wave = mix(pal.app, hex("#dfe8f0"), 0.5);
        // ondas: traços agrupados por cor, grossura e transparência
        {
            LineBatch lb[3][4];
            for (const Row& r : m_rows) {
                const int wb = qBound(0, int((r.k - 0.2) / 1.6 * 4), 3);
                for (const Seg& s : r.segs) {
                    const double xx = std::fmod(s.x + m_t * s.sp, c.W + 80) - 40, sw = std::sin(m_t * 1.3 + s.ph + r.y * 0.05);
                    const double nearM = std::exp(-std::pow((xx - MX) / (60 + r.k * 90), 2));
                    const double nb = (std::exp(-std::pow((xx - bx1) / (70 + r.k * 110), 2)) * B1.k + std::exp(-std::pow((xx - bx2) / (70 + r.k * 110), 2)) * B2.k) * beamA * 0.7;
                    const double a = (0.05 + 0.08 * qMax(0.0, sw)) * (0.6 + r.k * 0.4) + nearM * (0.35 + 0.25 * sw) * (0.4 + 0.6 * night + 0.6 * L) + nb * (0.3 + 0.3 * sw);
                    const int ci = (nb > nearM && nb > 0.15) ? 2 : nearM > 0.2 ? 1 : 0;
                    const double y = r.y + sw * r.k;
                    lb[ci][wb].add(xx, y, xx + s.len, y, qMin(0.85, a));
                }
            }
            const QColor cols[3] = { wave, glint, beamCol };
            for (int ci = 0; ci < 3; ++ci)
                for (int wb = 0; wb < 4; ++wb) lb[ci][wb].flush(p, cols[ci], 0.6 + (0.2 + (wb + 0.5) / 4 * 1.6) * 0.9);
        }
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // espuma em volta das pedras soltas
        p.setBrush(Qt::NoBrush);
        for (const Rock& r : m_rocks)
            for (int i = 0; i < 3; ++i) {
                p.setPen(QPen(alpha(hex("#eef4f8"), 0.25 + 0.2 * std::sin(m_t * 2 + i + r.x)), 1.2));
                const double rr = r.r * (1.1 + i * 0.25 + 0.1 * std::sin(m_t * 1.7 + i));
                p.drawArc(QRectF(r.x - rr, r.y + 3 + i * 2 - (2 + i), rr * 2, (2 + i) * 2), 180 * 16, 180 * 16);
            }
        // espuma batendo na beira da rocha: um traço quebrado e uns respingos, seguindo a costa irregular
        {
            int n = 0;
            for (double y = SEA + 44; y < c.H; y += 9 + (n * 7) % 9, ++n) {
                const double t = (y - (kTopY + 70)) / (c.H - kTopY - 70), ex = edgeX(y), ph = n * 2.39;
                const double surge = qMax(0.0, std::sin(m_t * (1.1 + (n % 3) * 0.25) + ph)), len = (6 + (n * 13) % 20) * (0.4 + surge) * (0.6 + t * 0.7);
                if (surge < 0.1) continue;
                p.setPen(pen(alpha(hex("#eef4f8"), 0.12 + 0.3 * surge), 1.5 + t * 1.5));
                p.drawLine(QPointF(ex - 2, y), QPointF(ex + len * 0.45, y - 1));
                p.drawLine(QPointF(ex + len * 0.6, y), QPointF(ex + len, y + 1));
                p.setPen(Qt::NoPen);
                for (int j = 0; j < 3; ++j) disc(p, ex + len * (0.2 + 0.35 * j) + std::sin(ph + j) * 3, y - 3 - j * 1.5 * surge, 0.8 + t, alpha(hex("#f2f6fa"), 0.25 * surge));
            }
        }
        // borrifo das ondas quebrando
        for (const Spray& s : m_sprays) disc(p, s.x, s.y, s.r * (1 + s.t), alpha(hex("#f2f6fa"), 0.7 * (1 - s.t / s.life)));
        // névoa
        if (m_mistDot.isNull() || m_mistCol != mix(pal.app, hex("#e8eef4"), 0.6)) { m_mistCol = mix(pal.app, hex("#e8eef4"), 0.6); m_mistDot = softDot(m_mistCol, 0.05); m_beamDot = softDot(beamCol, 0.05); }
        for (const Mist& m : m_mist) {
            p.setOpacity(m.a * (0.6 + 0.4 * night));
            blit(p, m_mistDot, m.x - m.r, m.y - m.r, m.r * 2, m.r * 2);
        }
        p.setOpacity(1);
        if (beamA > 0.02) {
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            // o facho em cone: dez camadas finas, da larga e fraca à estreita e forte, sumindo com a distância
            for (const Beam* B : { &B2, &B1 }) {
                if (B->len < 40) continue;
                for (int li = 0; li < 10; ++li) {
                    const double wk = std::pow(0.75, li), a2 = 0.08 + li * 0.009;
                    QLinearGradient g(lx, LY, lx + B->dir * B->len, LY);
                    g.setColorAt(0, alpha(beamCol, a2 * B->k * beamA));
                    g.setColorAt(0.25, alpha(beamCol, a2 * 0.75 * B->k * beamA));
                    g.setColorAt(1, alpha(beamCol, 0));
                    QPolygonF cone;
                    cone << QPointF(lx, LY - 3 * wk) << QPointF(lx + B->dir * B->len, LY - B->half * wk) << QPointF(lx + B->dir * B->len, LY + B->half * wk) << QPointF(lx, LY + 3 * wk);
                    p.setBrush(g);
                    p.drawPolygon(cone);
                }
            }
            // miolo quase branco no eixo
            for (const Beam* B : { &B1, &B2 }) {
                if (B->len < 40 || B->k < 0.02) continue;
                QLinearGradient g(lx, LY, lx + B->dir * B->len * 0.8, LY);
                g.setColorAt(0, alpha(hex("#fffaf0"), 0.55 * B->k * beamA));
                g.setColorAt(1, alpha(hex("#fffaf0"), 0));
                QPolygonF core;
                core << QPointF(lx, LY - 1.5) << QPointF(lx + B->dir * B->len * 0.8, LY - B->half * 0.05) << QPointF(lx + B->dir * B->len * 0.8, LY + B->half * 0.05) << QPointF(lx, LY + 1.5);
                p.setBrush(g);
                p.drawPolygon(core);
            }
            p.setBrush(Qt::NoBrush);
            // a névoa acende onde o facho passa
            for (const Mist& m : m_mist) {
                const double v = qMax(inBeam(m.x, m.y, B1), inBeam(m.x, m.y, B2));
                if (v < 0.02) continue;
                p.setOpacity(qMin(1.0, 0.28 * v * beamA * 2.5));
                blit(p, m_beamDot, m.x - m.r, m.y - m.r, m.r * 2, m.r * 2);
            }
            p.setOpacity(1);
            // poeira de maresia brilhando dentro do facho
            for (const Mote& m : m_motes) {
                const double v = qMax(inBeam(m.x, m.y, B1), inBeam(m.x, m.y, B2));
                if (v < 0.03) continue;
                p.fillRect(QRectF(m.x, m.y, m.s * 1.3, m.s * 1.3), alpha(hex("#fff8e8"), qMin(1.0, 1.6 * v * beamA * (0.6 + 0.4 * std::sin(m_t * 3 + m.ph)))));
            }
            // reflexo do facho na água, embaixo de onde ele varre
            for (int i = 0; i < 2; ++i) {
                const Beam& B = i ? B2 : B1;
                const double bx = i ? bx2 : bx1;
                if (B.k < 0.02) continue;
                p.save();
                p.translate(bx, SEA + 30);
                p.scale(1, 0.35);
                glow(p, 0, 0, 220, beamCol, 0.1 * beamA * B.k);
                p.restore();
            }
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        paintDyn(p, pal, c, lx, beamA, th);
        if (beamA > 0.02) {
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            double flashA = 0;
            for (const Beam* B : { &B1, &B2 }) if (B->sn > 0) flashA = qMax(flashA, std::pow(qMax(0.0, 1 - std::abs(B->c) * 3), 2));
            glow(p, lx, LY, 28, hex("#fffbf0"), 0.95 * beamA);
            glow(p, lx, LY, 95, beamCol, 0.32 * beamA);
            if (flashA > 0.02) {
                glow(p, lx, LY, 60 + 300 * flashA, hex("#fff8e8"), 0.7 * flashA * beamA);
                p.fillRect(QRectF(0, 0, c.W, c.H), alpha(beamCol, 0.11 * flashA * beamA));
            }
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
    }

private:
    struct Seg { double x, len, ph, sp; };
    struct Row { double y, k; std::vector<Seg> segs; };
    struct Spray { double x, y, vx, vy, t, life, r; };
    struct Mist { double x, y, r, v, a; };
    struct Mote { double x, y, v, ph, s; };
    struct Rock { double x, y, r; };
    struct Smoke { double x, y, t, life; };
    struct Beam { double c, sn, dir, len, half, k; };

    static constexpr double kTopY = 505, kTW = 26;                    // topo do penhasco, meia-largura da base da torre
    static constexpr double kPL = 24, kTB = 170, kWRM = 10, kLGH = 26;  // embasamento, corpo, sala de vigia, vidro da lanterna
    static constexpr double kG0 = kTopY - 6, kTTop = kG0 - kPL - kTB, kTWT = kTW * 0.64, kLW = kTWT * 0.86, kLTop = kTTop - 2 - kWRM - kLGH;

    static double sea(const Ctx& c) { return std::round(c.H * 0.6); }
    static double lxOf(const Ctx& c) { return zx(c, 215); }
    static double ly(const Ctx&) { return kLTop + kLGH / 2; }
    static Beam beamGeo(double a)
    {
        const double cc = std::cos(a), sn = std::sin(a), ac = std::abs(cc), vis = smooth(0.02, 0.35, ac);
        return { cc, sn, cc >= 0 ? 1.0 : -1.0, 1500 * ac, (44 + 150 * (1 - ac) * (0.95 + 0.25 * sn)) * (0.35 + 0.65 * vis), (0.75 + 0.25 * sn) * vis };
    }
    double edgeX(double y) const
    {
        for (size_t i = 1; i < m_edge.size(); ++i)
            if (m_edge[i].y() >= y) {
                const QPointF a = m_edge[i - 1], b = m_edge[i];
                return lerp(a.x(), b.x(), (y - a.y()) / qMax(1.0, b.y() - a.y()));
            }
        return m_edge.empty() ? 0 : m_edge.back().x();
    }
    void makeRows(const Ctx& c)
    {
        m_rows.clear();
        const int N = 46;
        const double SEA = sea(c);
        for (int i = 0; i < N; ++i) {
            const double t = std::pow(double(i) / N, 1.7), y = SEA + 2 + t * (c.H - SEA - 2), k = 0.2 + t * 1.6;
            Row r{ y, k, {} };
            for (int j = 0; j < 14 + i * 0.4; ++j) r.segs.push_back({ R(0, c.W), R(12, 40) * k, R(0, 6.28), R(4, 12) * k });
            m_rows.push_back(r);
        }
    }

    // ---------------------------------------------------------------- cenário parado
    void build(const Pal& pal, const Ctx& c)
    {
        m_L = c.tod == QLatin1String("theme") ? 0 : LV(pal, c);
        const double L = m_L, lx = lxOf(c), SEA = sea(c), H = c.H, W = c.W;
        QPixmap pm = layer(c, W, H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing);
        x.setPen(Qt::NoPen);
        const QColor rock = mix(mix(pal.app, Qt::black, 0.35), todPick(c, hex("#6a6460"), hex("#4a3a3a"), hex("#2a3038")), 0.45);
        const QColor rim = todPick(c, hex("#fff4dc"), hex("#ffb07a"), hex("#c8d8f0"));
        // promontório distante no horizonte, à direita, apagado pela névoa
        {
            QPainterPath pr(QPointF(W * 0.62, SEA + 1));
            for (double xx = W * 0.62; xx <= W + 10; xx += 10) pr.lineTo(xx, SEA - 6 - 26 * smooth(W * 0.62, W * 0.8, xx) - 8 * std::sin(xx * 0.02));
            pr.lineTo(W + 10, SEA + 1);
            pr.closeSubpath();
            x.fillPath(pr, mix(mix(pal.app, todPick(c, hex("#8aa0b4"), hex("#6a5070"), hex("#141e2a")), 0.6), Qt::black, 0.1));
        }
        // a costa: borda sorteada em dezenas de pontos (degraus, reentrâncias, ondulação)
        m_edge.clear();
        m_edge.push_back(QPointF(lx + 45, kTopY - 4));
        for (double y = kTopY - 4; y < H + 10;) {
            const double t = (y - kTopY) / (H - kTopY);
            y = qMin(H + 10, y + R(7, 18));
            const double target = lerp(lx + 52, lx + 140, std::pow(qMax(0.0, t), 0.8)), r = rnd();
            const double bump = r < 0.16 ? R(8, 18) : r < 0.3 ? -R(5, 12) : 0;
            m_edge.push_back(QPointF(target + std::sin(y * 0.045) * 12 + std::sin(y * 0.13 + 1.7) * 5 + R(-6, 6) + bump, y));
        }
        QPolygonF cliff;
        cliff << QPointF(-40, H + 10) << QPointF(-40, kTopY - 20) << QPointF(lx - 140, kTopY - 30) << QPointF(lx - 70, kTopY - 6);
        for (const QPointF& e : m_edge) cliff << e;
        QPainterPath cliffPath;
        cliffPath.addPolygon(cliff);
        cliffPath.closeSubpath();
        {
            QLinearGradient cg(0, kTopY - 40, 0, H);
            cg.setColorAt(0, mix(rock, rim, 0.12));
            cg.setColorAt(1, mix(rock, Qt::black, 0.5));
            x.fillPath(cliffPath, cg);
        }
        x.save();
        x.setClipPath(cliffPath, Qt::IntersectClip);
        // blocos de rocha: arredondados, topo pegando luz e pé na sombra, de trás pra frente
        struct Blk { double x, y, r; };
        std::vector<Blk> blocks;
        for (int i = 0; i < 18; ++i) blocks.push_back({ R(-40, lx + 120), R(kTopY + 10, H + 30), R(28, 64) });
        std::stable_sort(blocks.begin(), blocks.end(), [](const Blk& a, const Blk& b) { return a.y < b.y; });
        for (const Blk& k : blocks) {
            QPolygonF pts;
            for (int i = 0; i < 7; ++i) { const double an = i / 7.0 * 6.28 + R(-0.25, 0.25), rr = k.r * R(0.75, 1.05); pts << QPointF(k.x + std::cos(an) * rr, k.y + std::sin(an) * rr * 0.7); }
            QLinearGradient g(k.x + k.r * 0.6, k.y - k.r * 0.6, k.x - k.r * 0.6, k.y + k.r * 0.6);
            g.setColorAt(0, mix(rock, rim, 0.13));
            g.setColorAt(0.5, mix(rock, Qt::black, 0.05));
            g.setColorAt(1, mix(rock, Qt::black, 0.28));
            x.setPen(Qt::NoPen);
            x.setBrush(g);
            x.drawPolygon(pts);
            x.setBrush(Qt::NoBrush);
            x.setPen(QPen(alpha(Qt::black, 0.2), 1.2));
            x.drawPolyline(QPolygonF(pts.mid(1, 4)));
            x.setPen(QPen(alpha(rim, 0.14), 1));
            QPolygonF hi;
            hi << pts[5] << pts[6] << pts[0];
            x.drawPolyline(hi);
        }
        // estratos e fendas
        x.setPen(QPen(alpha(Qt::black, 0.12), 1));
        for (int i = 0; i < 9; ++i) {
            const double y0 = kTopY + 14 + i * 26 + R(-6, 6);
            QPainterPath sl(QPointF(-40, y0));
            for (double xx = -40; xx < lx + 260; xx += 24) sl.lineTo(xx, y0 + std::sin(xx * 0.03 + i) * 4 + (xx - lx) * 0.08);
            x.drawPath(sl);
        }
        x.setPen(QPen(alpha(Qt::black, 0.4), 1));
        for (int i = 0; i < 6; ++i) {
            double fx = R(lx - 120, lx + 90), fy = R(kTopY + 10, H - 60);
            QPainterPath cr(QPointF(fx, fy));
            for (int j = 0; j < 5; ++j) { fx += R(-6, 6); fy += R(8, 18); cr.lineTo(fx, fy); }
            x.drawPath(cr);
        }
        x.setPen(Qt::NoPen);
        // a sombra do lado de lá da luz
        {
            QLinearGradient sg(lx - 160, 0, lx + 260, 0);
            sg.setColorAt(0, alpha(Qt::black, 0.45));
            sg.setColorAt(0.6, alpha(Qt::black, 0));
            x.fillRect(QRectF(-40, kTopY - 40, lx + 300, H), sg);
        }
        x.restore();
        // a borda de cima iluminada; o alto de cada saliência pegando luz e sombra embaixo das reentrâncias
        x.setBrush(Qt::NoBrush);
        x.setPen(QPen(alpha(rim, 0.35), 1.4));
        {
            QPolygonF top;
            top << cliff[1] << cliff[2] << cliff[3] << cliff[4];
            x.drawPolyline(top);
        }
        for (size_t i = 1; i < m_edge.size(); ++i) {
            const QPointF a = m_edge[i - 1], b = m_edge[i];
            if (b.x() - a.x() > 5) {
                x.setPen(QPen(alpha(rim, 0.3), 1.2));
                x.drawLine(QPointF(a.x() - 6, a.y()), b);
            } else if (a.x() - b.x() > 4) {
                QPolygonF sh;
                sh << a << b << QPointF(b.x() - 10, b.y()) << QPointF(a.x() - 12, a.y() + 2);
                x.setPen(Qt::NoPen);
                x.setBrush(alpha(Qt::black, 0.35));
                x.drawPolygon(sh);
                x.setBrush(Qt::NoBrush);
            }
        }
        // capim no topo
        {
            const QColor grass = mix(mix(pal.app, hex("#1e3a24"), 0.5), todPick(c, hex("#5a7a3a"), hex("#4a4a2a"), hex("#1a2a1e")), 0.5);
            QPainterPath gp;
            for (double xx = lx - 150; xx < lx + 45; xx += 2.2) {
                const double yb = xx < lx - 70 ? lerp(kTopY - 30, kTopY - 6, (xx - lx + 140) / 70) : lerp(kTopY - 6, kTopY - 4, (xx - lx + 70) / 115);
                gp.moveTo(xx, yb + 2);
                gp.lineTo(xx + R(-2, 2), yb - R(3, 9));
            }
            x.setPen(QPen(grass, 1));
            x.drawPath(gp);
        }
        x.setPen(Qt::NoPen);
        compound(x, pal, c, lx, L, rim);
        // pedras soltas no mar, perto do pé da rocha
        m_rocks.clear();
        for (int i = 0; i < 6; ++i) {
            const double y = i < 2 ? H - R(30, 110) : SEA + 40 + i * R(14, 26);
            m_rocks.push_back({ edgeX(y) + (i < 2 ? R(4, 12) : 30 + i * R(22, 44)), y, i < 2 ? R(12, 20) : R(6, 14) });
        }
        for (const Rock& r : m_rocks) {
            QLinearGradient g(r.x - r.r, 0, r.x + r.r, 0);
            g.setColorAt(0, mix(rock, Qt::black, 0.4));
            g.setColorAt(0.7, mix(rock, rim, 0.12));
            g.setColorAt(1, mix(rock, Qt::black, 0.3));
            QPainterPath rp(QPointF(r.x - r.r, r.y + 3));
            rp.quadTo(r.x - r.r * 0.6, r.y - r.r * 0.9, r.x + r.r * 0.1, r.y - r.r * 0.8);
            rp.quadTo(r.x + r.r * 0.9, r.y - r.r * 0.5, r.x + r.r, r.y + 3);
            rp.closeSubpath();
            x.fillPath(rp, g);
        }
        x.end();
        m_back.set(c, pm);
    }

    // a casa do faroleiro, o corredor, o embasamento, a torre gasta, a galeria, a cúpula e o muro de pedra
    void compound(QPainter& x, const Pal& pal, const Ctx& c, double lx, double L, const QColor& rim)
    {
        const double night = 1 - 0.85 * L;
        const QColor warm = alpha(hex("#ffd28a"), 0.9 * night + 0.08);
        const QColor white = mix(pal.app, todPick(c, hex("#f4f0e8"), hex("#f0d8c0"), hex("#c8ccd2")), 0.25), red = mix(pal.app, todPick(c, hex("#c8302a"), hex("#b02a20"), hex("#7a2a28")), 0.25);
        const QColor stone = mix(mix(pal.app, Qt::black, 0.2), todPick(c, hex("#9a948a"), hex("#7a6a62"), hex("#3a4048")), 0.5), metal = mix(pal.app, hex("#1a1e24"), 0.4);
        // ---- casa do faroleiro
        const double hx = lx - 124, hw = 70, hb = kTopY - 9, hh = 30, ht = hb - hh;
        m_varal = QRectF(hx - 34, hb - 22, 28, 0);
        x.setPen(QPen(mix(metal, Qt::black, 0.2), 1.4));
        x.drawLine(QPointF(hx - 34, hb + 2), QPointF(hx - 34, hb - 24));
        x.drawLine(QPointF(hx - 6, hb + 2), QPointF(hx - 6, hb - 24));
        x.setPen(Qt::NoPen);
        x.fillRect(QRectF(hx - 2, hb - 3, hw + 4, 14), mix(stone, Qt::black, 0.25));
        for (double sx = hx; sx < hx + hw; sx += 7) x.fillRect(QRectF(sx, hb - 3 + (int(sx / 7) % 2) * 5, 1, 5), alpha(Qt::black, 0.25));
        x.fillRect(QRectF(hx, ht, hw, hh), shadeH(white, hx, hw, rim, 0.1, 0.3, 0.12));
        for (int i = 0; i < 5; ++i) x.fillRect(QRectF(hx + R(0, hw - 10), ht + R(2, hh - 8), R(6, 14), R(3, 7)), alpha(Qt::black, 0.08));
        for (double wx : { hx + 8, hx + 44 }) {
            x.fillRect(QRectF(wx - 1.5, ht + 6.5, 15, 15), mix(white, Qt::black, 0.35));
            x.fillRect(QRectF(wx, ht + 8, 12, 12), warm);
            x.fillRect(QRectF(wx, ht + 8, 4, 12), alpha(mix(hex("#c86a3a"), pal.app, 0.3), 0.55));
            x.fillRect(QRectF(wx + 5.4, ht + 8, 1.2, 12), mix(white, Qt::black, 0.45));
            x.fillRect(QRectF(wx, ht + 13.4, 12, 1.2), mix(white, Qt::black, 0.45));
            x.fillRect(QRectF(wx - 2, ht + 20.5, 16, 2), mix(white, rim, 0.3));
        }
        x.fillRect(QRectF(hx + 25.5, hb - 21, 13, 21), mix(white, Qt::black, 0.3));
        x.fillRect(QRectF(hx + 27, hb - 19.5, 10, 19.5), mix(red, Qt::black, 0.45));
        x.fillRect(QRectF(hx + 40, hb - 22, 1.2, 4), metal);
        x.fillRect(QRectF(hx + 39, hb - 19, 3.4, 4), metal);
        m_lamp = QPointF(hx + 40.7, hb - 17);
        // telhado de telhas, beiral com sombra na parede
        const double rw = hw + 12, rx0 = hx - 6, rh = 20;
        const QColor roofC = mix(pal.app, todPick(c, hex("#9a4a32"), hex("#7a3a2a"), hex("#2e2420")), 0.35);
        {
            QLinearGradient rg(0, ht - rh, 0, ht);
            rg.setColorAt(0, mix(roofC, rim, 0.12));
            rg.setColorAt(1, mix(roofC, Qt::black, 0.3));
            QPolygonF roof;
            roof << QPointF(rx0, ht + 1) << QPointF(rx0 + 10, ht - rh) << QPointF(rx0 + rw - 10, ht - rh) << QPointF(rx0 + rw, ht + 1);
            x.setBrush(rg);
            x.drawPolygon(roof);
            QPainterPath tiles;
            for (double ty = ht - rh + 4; ty < ht; ty += 4) {
                const double f = (ty - (ht - rh)) / rh, xa = lerp(rx0 + 10, rx0, f), xb = lerp(rx0 + rw - 10, rx0 + rw, f);
                tiles.moveTo(xa, ty);
                tiles.lineTo(xb, ty);
                for (double tx = xa + (int(ty / 4) % 2) * 3; tx < xb; tx += 6) { tiles.moveTo(tx, ty); tiles.lineTo(tx, ty - 4); }
            }
            x.setBrush(Qt::NoBrush);
            x.setPen(QPen(alpha(Qt::black, 0.25), 0.8));
            x.drawPath(tiles);
            x.setPen(Qt::NoPen);
            x.fillRect(QRectF(hx, ht, hw, 3), alpha(Qt::black, 0.35));
            x.fillRect(QRectF(rx0 + 10, ht - rh, rw - 20, 1.2), mix(roofC, rim, 0.25));
        }
        const double cx0 = hx + hw - 18;
        x.fillRect(QRectF(cx0, ht - rh - 10, 9, 18), shadeH(stone, cx0, 9, rim));
        x.fillRect(QRectF(cx0 - 1, ht - rh - 12, 11, 3), mix(stone, Qt::black, 0.4));
        m_chim = QPointF(cx0 + 4.5, ht - rh - 12);
        m_hasDyn = true;
        // ---- corredor coberto da casa até o farol
        const double cxA = hx + hw, cxB = lx - kTW - 10, cTop = kG0 - 22;
        x.fillRect(QRectF(cxA, cTop, cxB - cxA, kG0 - cTop), shadeH(white, cxA, cxB - cxA, rim, 0.08, 0.35, 0.2));
        {
            QPolygonF cr;
            cr << QPointF(cxA - 2, cTop + 1) << QPointF(cxA + 3, cTop - 6) << QPointF(cxB + 2, cTop - 6) << QPointF(cxB + 4, cTop + 1);
            x.setBrush(mix(roofC, Qt::black, 0.15));
            x.drawPolygon(cr);
            x.setBrush(Qt::NoBrush);
        }
        x.fillRect(QRectF((cxA + cxB) / 2 - 3, cTop + 5, 6, 7), warm);
        x.fillRect(QRectF(cxA, cTop + 1, cxB - cxA, 2), alpha(Qt::black, 0.3));
        // ---- embasamento octogonal de pedra, blocos aparentes e escadinha
        const double pw = kTW + 10;
        {
            QPolygonF oct;
            oct << QPointF(lx - pw, kG0) << QPointF(lx - pw, kG0 - kPL + 4) << QPointF(lx - pw + 6, kG0 - kPL) << QPointF(lx + pw - 6, kG0 - kPL) << QPointF(lx + pw, kG0 - kPL + 4) << QPointF(lx + pw, kG0);
            x.setBrush(mix(stone, Qt::black, 0.35));
            x.drawPolygon(oct);
            x.setBrush(Qt::NoBrush);
        }
        x.fillRect(QRectF(lx - pw + 6, kG0 - kPL, (pw - 6) * 2, kPL), shadeH(stone, lx - pw + 6, (pw - 6) * 2, rim, 0.15, 0.3, 0.2));
        for (int r = 0; r < 4; ++r) {
            const double yy = kG0 - kPL + r * 6;
            x.fillRect(QRectF(lx - pw, yy, pw * 2, 0.8), alpha(Qt::black, 0.3));
            for (double bx = lx - pw + (r % 2) * 5; bx < lx + pw; bx += 10) x.fillRect(QRectF(bx, yy, 0.8, 6), alpha(Qt::black, 0.3));
        }
        x.fillRect(QRectF(lx - pw + 6, kG0 - kPL, (pw - 6) * 2, 1.2), mix(stone, rim, 0.25));
        for (int i = 0; i < 3; ++i) x.fillRect(QRectF(lx - 12 + i * 2, kG0 + i * 2.5 - 2, 24 - i * 4, 2.5), mix(stone, i % 2 ? QColor(Qt::black) : rim, 0.15));
        auto arch = [&](double ax, double ay, double w, double h) {
            QPainterPath ap(QPointF(ax, ay + h));
            ap.lineTo(ax, ay + w / 2);
            ap.arcTo(QRectF(ax, ay, w, w), 180, -180);
            ap.lineTo(ax + w, ay + h);
            ap.closeSubpath();
            return ap;
        };
        x.fillPath(arch(lx - 8.5, kG0 - 21, 17, 21), mix(stone, rim, 0.2));
        x.fillPath(arch(lx - 6.5, kG0 - 19, 13, 19), mix(red, Qt::black, 0.5));
        {
            QPolygonF can;
            can << QPointF(lx - 12, kG0 - 22) << QPointF(lx, kG0 - 28) << QPointF(lx + 12, kG0 - 22);
            x.setBrush(metal);
            x.drawPolygon(can);
            x.setBrush(Qt::NoBrush);
        }
        // ---- a torre: cilindro que afina, listras na curva, desgaste, janelinhas em espiral
        const double base = kG0 - kPL;
        auto twd = [&](double t) { return lerp(kTW, kTWT, t); };
        QPainterPath tower;
        tower.moveTo(lx - kTW, base);
        tower.lineTo(lx - kTWT, kTTop);
        tower.lineTo(lx + kTWT, kTTop);
        tower.lineTo(lx + kTW, base);
        tower.closeSubpath();
        x.save();
        x.setClipPath(tower, Qt::IntersectClip);
        x.fillRect(QRectF(lx - kTW, kTTop, kTW * 2, kTB), shadeH(white, lx - kTW, kTW * 2, rim));
        for (int i = 0; i < 4; ++i) {
            const double t0 = (i * 2 + 1) / 8.0, t1 = (i * 2 + 2) / 8.0, y0 = base - t0 * kTB, y1 = base - t1 * kTB, w0 = twd(t0), w1 = twd(t1);
            QPainterPath band(QPointF(lx - w0 - 2, y0));
            band.quadTo(lx, y0 + 5, lx + w0 + 2, y0);
            band.lineTo(lx + w1 + 2, y1);
            band.quadTo(lx, y1 + 5, lx - w1 - 2, y1);
            band.closeSubpath();
            x.fillPath(band, shadeH(red, lx - kTW, kTW * 2, rim, 0.1, 0.5, 0.3));
        }
        for (int i = 0; i < 22; ++i) {
            x.save();
            x.translate(lx + R(-kTW, kTW), R(kTTop + 10, base - 4));
            x.rotate(qRadiansToDegrees(R(0, 3)));
            x.setBrush(rnd() < 0.5 ? alpha(Qt::white, R(0.04, 0.09)) : alpha(Qt::black, R(0.06, 0.14)));
            x.drawEllipse(QPointF(0, 0), R(2, 6), R(1.5, 4));
            x.restore();
        }
        x.setBrush(Qt::NoBrush);
        for (int i = 0; i < 9; ++i) {
            const double sx = lx + R(-kTWT, kTWT), sl = R(30, 110);
            vfill(x, QRectF(sx, kTTop, R(1.2, 3), sl), alpha(i % 3 ? hex("#6a3a1a") : hex("#e8e4d8"), i % 3 ? 0.32 : 0.12), alpha(hex("#6a3a1a"), 0), kTTop, kTTop + sl);
        }
        vfill(x, QRectF(lx - kTW, kTTop, kTW * 2, 16), alpha(Qt::black, 0.45), alpha(Qt::black, 0), kTTop, kTTop + 16);
        x.restore();
        x.setPen(Qt::NoPen);
        const double wins[4][2] = { { 0.2, 0.32 }, { 0.42, -0.36 }, { 0.64, 0.3 }, { 0.84, -0.3 } };
        for (const auto& wv : wins) {
            const double yy = base - wv[0] * kTB, ww = twd(wv[0]), wx = lx + wv[1] * ww * 2 - 2.5;
            x.fillRect(QRectF(wx - 1, yy - 1, 7, 10), mix(white, Qt::black, 0.5));
            x.fillRect(QRectF(wx, yy, 5, 8), warm);
            x.fillRect(QRectF(wx - 1.5, yy + 8.5, 8, 1.2), mix(white, rim, 0.2));
        }
        // ---- galeria: consolas, laje, grade dupla
        const double gw = kTWT + 10;
        x.setBrush(metal);
        for (int i = -3; i <= 3; ++i) {
            const double bx = lx + i * gw / 3.4;
            QPolygonF br;
            br << QPointF(bx - 1.5, kTTop - 1) << QPointF(bx + 1.5, kTTop - 1) << QPointF(bx + 0.5, kTTop + 9) << QPointF(bx - 0.5, kTTop + 9);
            x.drawPolygon(br);
        }
        x.setBrush(mix(metal, rim, 0.15));
        x.drawEllipse(QPointF(lx, kTTop - 1), gw, 3);
        x.setBrush(Qt::NoBrush);
        x.fillRect(QRectF(lx - gw, kTTop - 1, gw * 2, 3), mix(metal, Qt::black, 0.3));
        {
            QPainterPath rail;
            rail.moveTo(lx - gw, kTTop - 13); rail.lineTo(lx + gw, kTTop - 13);
            rail.moveTo(lx - gw, kTTop - 7); rail.lineTo(lx + gw, kTTop - 7);
            for (double gx = lx - gw; gx <= lx + gw + 0.1; gx += gw / 6) { rail.moveTo(gx, kTTop - 14); rail.lineTo(gx, kTTop - 1); }
            x.setPen(QPen(metal, 1));
            x.drawPath(rail);
            x.setPen(Qt::NoPen);
        }
        x.fillRect(QRectF(lx - kTWT * 0.95, kTTop - 2 - kWRM, kTWT * 1.9, kWRM), shadeH(red, lx - kTWT * 0.95, kTWT * 1.9, rim, 0.1, 0.5, 0.3));
        x.fillRect(QRectF(lx - kLW - 2, kLTop + kLGH - 1, (kLW + 2) * 2, 2), mix(metal, Qt::black, 0.2));
        x.fillRect(QRectF(lx - kLW - 2, kLTop - 1, (kLW + 2) * 2, 2), mix(metal, Qt::black, 0.2));
        // cúpula com nervuras, bola de ventilação, para-raios e cata-vento
        const double dTop = kLTop - 18;
        {
            QPainterPath dome(QPointF(lx - kLW - 3, kLTop));
            dome.quadTo(lx - kLW * 0.6, dTop, lx, dTop);
            dome.quadTo(lx + kLW * 0.6, dTop, lx + kLW + 3, kLTop);
            dome.closeSubpath();
            x.fillPath(dome, shadeH(red, lx - kLW - 3, (kLW + 3) * 2, rim, 0.15, 0.55, 0.35));
            QPainterPath ribs;
            for (double f : { -0.6, -0.25, 0.25, 0.6 }) { ribs.moveTo(lx + f * (kLW + 3), kLTop); ribs.quadTo(lx + f * kLW * 0.7, dTop + 4, lx, dTop); }
            x.setPen(QPen(alpha(Qt::black, 0.25), 0.8));
            x.drawPath(ribs);
            x.setPen(Qt::NoPen);
        }
        disc(x, lx, dTop - 3, 3.2, metal);
        disc(x, lx + 1, dTop - 4, 1.2, alpha(rim, 0.3));
        x.fillRect(QRectF(lx - 0.6, dTop - 22, 1.2, 19), metal);
        x.setPen(QPen(metal, 1));
        x.drawLine(QPointF(lx - 8, dTop - 14), QPointF(lx + 7, dTop - 14));
        x.drawLine(QPointF(lx + 7, dTop - 14), QPointF(lx + 4, dTop - 17));
        x.drawLine(QPointF(lx + 7, dTop - 14), QPointF(lx + 4, dTop - 11));
        x.setPen(Qt::NoPen);
        x.fillRect(QRectF(lx - 9, dTop - 16, 3, 4), metal);
        // ---- muro de pedra seca na beira do penhasco
        for (double mx = lx - 150; mx < lx + 44; mx += 5.5) {
            const double yb = mx < lx - 70 ? lerp(kTopY - 30, kTopY - 6, (mx - lx + 140) / 70) : lerp(kTopY - 6, kTopY - 4, (mx - lx + 70) / 115);
            for (int r = 0; r < 2; ++r) {
                const double sx = mx + (r % 2) * 2.7, sy = yb + 6 - r * 4.5, sw = R(4.5, 6.5);
                QPainterPath st;
                st.addRoundedRect(QRectF(sx, sy, sw, 4.5), 1.8, 1.8);
                x.fillPath(st, mix(stone, rnd() < 0.5 ? QColor(Qt::black) : rim, R(0.05, 0.25)));
                x.fillRect(QRectF(sx, sy + 3.6, sw, 0.9), alpha(Qt::black, 0.25));
            }
        }
    }

    // o que se mexe no conjunto: fumaça, lampião, roupa no varal e a lanterna com a lente girando
    void paintDyn(QPainter& p, const Pal& pal, const Ctx& c, double lx, double beamA, double th)
    {
        Q_UNUSED(c);
        const double night = 1 - 0.85 * m_L;
        const QColor metal = mix(pal.app, hex("#1a1e24"), 0.4);
        for (const Smoke& s : m_smoke) { const double k = s.t / s.life; disc(p, s.x, s.y, 2 + k * 9, alpha(mix(pal.app, hex("#d8dce0"), 0.5), 0.16 * (1 - k))); }
        if (night > 0.2) {
            const double f = 0.85 + 0.15 * std::sin(m_t * 9) * std::sin(m_t * 2.3);
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            glow(p, m_lamp.x(), m_lamp.y(), 26, hex("#ffcf7a"), 0.5 * night * f);
            p.fillRect(QRectF(m_lamp.x() - 1, m_lamp.y() - 1, 2, 3), alpha(hex("#fff2c8"), 0.95 * night));
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        // roupa no varal balançando
        {
            const double x0 = m_varal.left(), x1 = m_varal.right(), y = m_varal.top(), sag = 4;
            QPainterPath ln(QPointF(x0, y));
            ln.quadTo((x0 + x1) / 2, y + sag * 2, x1, y);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(alpha(hex("#e8e4d8"), 0.6), 0.7));
            p.drawPath(ln);
            p.setPen(Qt::NoPen);
            static const char* CL[4] = { "#e8e4d8", "#4a6a9a", "#c8504a", "#e8d070" };
            for (int i = 0; i < 4; ++i) {
                const double t = (i + 0.7) / 4.6, cx = lerp(x0, x1, t), cy = y + sag * 4 * t * (1 - t);
                p.save();
                p.translate(cx, cy);
                p.rotate(qRadiansToDegrees(std::sin(m_t * 2.2 + i * 1.3) * 0.18));
                p.fillRect(QRectF(-2.5, 0, 5, 7 + (i % 2) * 2), mix(mix(hex(CL[i]), pal.app, 0.35), Qt::black, 0.3 * (1 - m_L)));
                p.restore();
            }
        }
        // a lanterna: vidro, a lente de Fresnel com os anéis e o brilho girando com o facho, os montantes em losango
        const bool on = beamA > 0.1;
        {
            QLinearGradient glass(lx - kLW, 0, lx + kLW, 0);
            glass.setColorAt(0, alpha(on ? hex("#e8b860") : hex("#2a3440"), 0.95));
            glass.setColorAt(0.5, alpha(on ? hex("#fff0c8") : hex("#6a8094"), 0.95));
            glass.setColorAt(1, alpha(on ? hex("#e8b860") : hex("#2a3440"), 0.95));
            p.fillRect(QRectF(lx - kLW, kLTop, kLW * 2, kLGH), glass);
            const double lw2 = kLW * 0.62, hot = std::sin(th) * lw2;
            QLinearGradient lg(lx - lw2, 0, lx + lw2, 0);
            const QColor lc = on ? hex("#fff2c8") : hex("#a8b8c4");
            lg.setColorAt(0, alpha(lc, on ? 0.55 : 0.25));
            lg.setColorAt(qBound(0.0, 0.5 + hot / (lw2 * 2), 1.0), alpha(on ? QColor(Qt::white) : hex("#dfe8f0"), on ? 1 : 0.55));
            lg.setColorAt(1, alpha(lc, on ? 0.55 : 0.25));
            p.setBrush(lg);
            p.drawEllipse(QPointF(lx, kLTop + kLGH / 2), lw2, kLGH / 2 - 2);
            p.setBrush(Qt::NoBrush);
            QPainterPath rings;
            for (int i = 1; i < 7; ++i) {
                const double yy = kLTop + 2 + i * (kLGH - 4) / 7, ww = lw2 * std::sqrt(qMax(0.0, 1 - std::pow((yy - kLTop - kLGH / 2) / (kLGH / 2 - 2), 2)));
                rings.moveTo(lx - ww, yy);
                rings.lineTo(lx + ww, yy);
            }
            p.setPen(QPen(alpha(on ? hex("#a8783a") : hex("#4a5a6a"), 0.55), 0.7));
            p.drawPath(rings);
            p.save();
            p.setClipRect(QRectF(lx - kLW, kLTop, kLW * 2, kLGH), Qt::IntersectClip);
            QPainterPath lat;
            for (double d = -kLGH; d < kLW * 2 + kLGH; d += 8) {
                lat.moveTo(lx - kLW + d, kLTop); lat.lineTo(lx - kLW + d - kLGH, kLTop + kLGH);
                lat.moveTo(lx - kLW + d - kLGH, kLTop); lat.lineTo(lx - kLW + d, kLTop + kLGH);
            }
            p.setPen(QPen(on ? alpha(mix(metal, hex("#ffd28a"), 0.25), 0.85) : metal, 1.1));
            p.drawPath(lat);
            p.setPen(Qt::NoPen);
            if (!on) p.fillRect(QRectF(lx - kLW * 0.6, kLTop, 3, kLGH), alpha(Qt::white, 0.18));
            p.restore();
            p.setPen(Qt::NoPen);
            p.fillRect(QRectF(lx - kLW, kLTop, 1.5, kLGH), metal);
            p.fillRect(QRectF(lx + kLW - 1.5, kLTop, 1.5, kLGH), metal);
        }
    }

    Cache m_back;
    QString m_key;
    QPixmap m_stars, m_mistDot, m_beamDot;
    QColor m_mistCol;
    double m_starsPx = 0, m_L = 0, m_t = 0, m_nextSpray = 1;
    std::vector<QPointF> m_edge;
    std::vector<Row> m_rows;
    std::vector<Spray> m_sprays;
    std::vector<Mist> m_mist;
    std::vector<Mote> m_motes;
    std::vector<Rock> m_rocks;
    std::vector<Smoke> m_smoke;
    QPointF m_chim, m_lamp;
    QRectF m_varal;
    bool m_hasDyn = false;
};

// ============================================================ charneca
// Colinas de urze em quatro planos com a neblina passando entre eles; árvores retorcidas pelo vento,
// círculo de pedras, muro de pedra seca, ruína com chaminé; urze e capim alto balançando, pedras com
// musgo, poça de turfa; sol pálido (ou lua) atrás da névoa e corvos (um pousado na árvore da frente).
class Moor2 : public Layer {
public:
    void init(Ctx&) override
    {
        m_key.clear();
        m_planes.clear();
        m_fog = QPixmap();
        m_crows.on = false;
        m_nextCrows = R(3, 6);
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_nextCrows -= dt;
        if (!m_crows.on && m_nextCrows <= 0) {
            m_crows = { true, -40, R(c.H * 0.16, c.H * 0.34), R(45, 70), {} };
            const int n = 3 + RI(4);
            for (int i = 0; i < n; ++i) m_crows.b.push_back({ -(i * R(16, 26)), R(-14, 14), R(0, 6.28) });
        }
        if (m_crows.on) { m_crows.x += m_crows.v * dt; m_crows.y += std::sin(m_t * 0.6) * 4 * dt; if (m_crows.x > c.W + 200) { m_crows.on = false; m_nextCrows = R(14, 28); } }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const QString key = todKey(c);
        if (key != m_key || m_planes.size() != 4) {
            Seeded sd(0x300a2u);
            build(pal, c);
            m_key = key;
        }
        p.setPen(Qt::NoPen);
        skyTOD(p, pal, c, mix(pal.app, Qt::white, pal.dark ? 0.06 : 0.2), pal.app);
        // o sol pálido (ou a lua) borrado atrás da névoa
        const double sx = zx(c, 300), sy = c.H * 0.28;
        const QColor sc = todPick(c, hex("#fff8e8"), hex("#ffd0a0"), hex("#e8eef8"));
        {
            QRadialGradient g(QPointF(sx, sy), 160);
            g.setColorAt(0, alpha(sc, 0.35));
            g.setColorAt(0.2, alpha(sc, 0.18));
            g.setColorAt(1, alpha(sc, 0));
            p.fillRect(QRectF(sx - 160, sy - 160, 320, 320), g);
            disc(p, sx, sy, 22, alpha(sc, 0.55));
        }
        // corvos voando
        if (m_crows.on) {
            const QColor ck = mix(pal.app, Qt::black, 0.55);
            for (const Crow& b : m_crows.b) {
                const double x0 = m_crows.x + b.dx, y0 = m_crows.y + b.dy, f = std::sin(m_t * 6 + b.ph) * 4;
                QPainterPath w(QPointF(x0 - 9, y0 - f));
                w.quadTo(x0 - 4, y0 - 3, x0, y0);
                w.quadTo(x0 + 4, y0 - 3, x0 + 9, y0 - f);
                w.quadTo(x0 + 4, y0 + 0.5, x0, y0 + 1.5);
                w.quadTo(x0 - 4, y0 + 0.5, x0 - 9, y0 - f);
                p.fillPath(w, ck);
            }
        }
        // os planos com a névoa passando entre eles (mais rápida perto)
        for (int k = 0; k < 4; ++k) {
            static const double UP[4] = { 40, 80, 160, 300 };
            const double top = qMax(0.0, Y0(c, k) - AMP(k) * 1.5 - UP[k]), px = c.px;
            p.drawPixmap(QRectF(0, top, c.W, c.H - top), m_planes[size_t(k)], QRectF(0, top * px, c.W * px, (c.H - top) * px));
            if (k < 3) {
                static const double V[3] = { 7, 11, 16 }, DY[3] = { 150, 150, 140 }, A[3] = { 0.55, 0.45, 0.4 };
                const double off = std::fmod(m_t * V[k] * INT(c), c.W), y = Y0(c, k + 1) - DY[k];
                p.setOpacity(A[k]);
                blit(p, m_fog, off - c.W, y, c.W, 300);
                blit(p, m_fog, off, y, c.W, 300);
                p.setOpacity(1);
            }
        }
        // urze e capim alto balançando
        {
            const QColor ink = mix(pal.app, Qt::black, 0.62), hc = mix(todPick(c, hex("#8a5a7a"), hex("#7a4a68"), hex("#3a2e3a")), pal.app, 0.35);
            QVector<QLineF> grass, heath;
            for (const Tuft& t : m_tufts) {
                const double wv = std::sin(m_t * 1.3 + t.x * 0.01) * 0.25 + std::sin(m_t * 2.6 + t.ph) * 0.08;
                QVector<QLineF>& v = t.heather ? heath : grass;
                for (const auto& b : t.blades) {
                    const QPointF a(t.x + b[0], t.y), m(t.x + b[0] + (b[2] + wv) * b[1] * 0.3, t.y - b[1] * 0.55), e(t.x + b[0] + (b[2] + wv) * b[1], t.y - b[1]);
                    v.append(QLineF(a, m));
                    v.append(QLineF(m, e));
                }
            }
            p.setBrush(Qt::NoBrush);
            p.setPen(pen(ink, 1.1));
            p.drawLines(grass);
            p.setPen(pen(hc, 1.6));
            p.drawLines(heath);
            p.setPen(Qt::NoPen);
        }
        // o corvo pousado no galho da árvore da frente, olhando pros lados e dando uns pulinhos
        if (m_hasPerch) {
            const double turn = std::sin(m_t * 0.4) > 0.7 ? -1 : 1, hop = qMax(0.0, std::sin(m_t * 0.9)) > 0.97 ? 2 : 0;
            const QColor ck = mix(pal.app, Qt::black, 0.7);
            p.save();
            p.translate(m_perch.x(), m_perch.y() - hop);
            p.scale(turn, 1);
            p.setBrush(ck);
            p.save(); p.rotate(qRadiansToDegrees(-0.3)); p.drawEllipse(QPointF(0, -5), 4.5, 3.2); p.restore();
            p.drawEllipse(QPointF(3.5, -8.5), 2.2, 2.2);
            QPolygonF beak, tail;
            beak << QPointF(5.5, -8.8) << QPointF(8.5, -8) << QPointF(5.5, -7.6);
            tail << QPointF(-3.5, -4) << QPointF(-9, -1) << QPointF(-3, -2.5);
            p.drawPolygon(beak);
            p.drawPolygon(tail);
            p.setBrush(Qt::NoBrush);
            p.fillRect(QRectF(-0.5, -2.5, 0.8, 2.5), ck);
            p.fillRect(QRectF(1, -2.5, 0.8, 2.5), ck);
            p.restore();
        }
        // névoa rente ao chão, na frente
        vfill(p, QRectF(0, c.H - 120, c.W, 120), alpha(m_fogCol, 0), alpha(m_fogCol, 0.18), c.H - 120, c.H);
    }

private:
    struct Crow { double dx, dy, ph; };
    struct Flock { bool on = false; double x = 0, y = 0, v = 0; std::vector<Crow> b; };
    struct Tuft { double x, y; std::vector<std::array<double, 3>> blades; double ph; bool heather; };

    static double Y0(const Ctx& c, int k) { static const double F[4] = { 0.5, 0.58, 0.68, 0.82 }; return c.H * F[k]; }
    static double AMP(int k) { static const double A[4] = { 18, 26, 34, 26 }; return A[k]; }
    double hillAt(const Ctx& c, int k, double x) const
    {
        const auto& ph = m_ph[size_t(k)];
        return Y0(c, k) + std::sin(x * 0.004 + ph[0]) * AMP(k) + std::sin(x * 0.011 + ph[1]) * AMP(k) * 0.35 + std::sin(x * 0.027 + ph[2]) * AMP(k) * 0.12;
    }
    // árvore de charneca: tronco curvo que afina, galhos tortos pro lado do vento
    void gnarl(QPainter& x, double x0, double y0, double ang, double len, double w, int d, const QColor& col, double lean)
    {
        if (d > 6 || len < 3) return;
        const double bend = R(-0.25, 0.25) + lean * 0.12, x1 = x0 + std::cos(ang) * len, y1 = y0 + std::sin(ang) * len;
        const double cx = x0 + std::cos(ang + bend) * len * 0.55, cy = y0 + std::sin(ang + bend) * len * 0.55;
        const double nx = -std::sin(ang), ny = std::cos(ang), w1 = w * 0.68;
        QPainterPath br(QPointF(x0 + nx * w / 2, y0 + ny * w / 2));
        br.quadTo(cx + nx * (w + w1) / 4, cy + ny * (w + w1) / 4, x1 + nx * w1 / 2, y1 + ny * w1 / 2);
        br.lineTo(x1 - nx * w1 / 2, y1 - ny * w1 / 2);
        br.quadTo(cx - nx * (w + w1) / 4, cy - ny * (w + w1) / 4, x0 - nx * w / 2, y0 - ny * w / 2);
        br.closeSubpath();
        x.fillPath(br, col);
        const int n = d < 2 ? 2 : (rnd() < 0.75 ? 2 : 1);
        for (int i = 0; i < n; ++i) {
            const double da = (i == 0 ? -1 : 1) * R(0.25, 0.7) + lean * 0.18;
            gnarl(x, x1, y1, ang + da, len * R(0.62, 0.8), w1, d + 1, col, lean);
        }
        if (m_wantPerch && d == 4 && !m_hasPerch && rnd() < 0.15 && w < 3) { m_perch = QPointF(x1, y1); m_hasPerch = true; }
    }
    void build(const Pal& pal, const Ctx& c)
    {
        const double W = c.W, H = c.H;
        for (auto& ph : m_ph) ph = { R(0, 6.28), R(0, 6.28), R(0, 6.28) };
        const QColor fogC = todPick(c, mix(pal.app, hex("#e8ecee"), 0.55), mix(pal.app, hex("#d8b8b0"), 0.45), mix(pal.app, hex("#9aa4b0"), 0.35));
        const QColor moor = todPick(c, hex("#5a5248"), hex("#4a3a40"), hex("#22262c")), heather = todPick(c, hex("#7a5a6a"), hex("#6a4058"), hex("#3a2e3a"));
        m_planes.clear();
        m_hasPerch = false;
        m_wantPerch = false;
        for (int k = 0; k < 4; ++k) {
            static const double FOGK[4] = { 0.62, 0.45, 0.25, 0.06 }, SC[4] = { 0.4, 0.6, 0.85, 1.2 };
            const double fogK = FOGK[k];
            QPixmap pm = layer(c, W, H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing);
            x.setPen(Qt::NoPen);
            const QColor base = mix(mix(mix(pal.app, Qt::black, 0.25 + 0.12 * k), moor, 0.35), fogC, fogK);
            {
                QPainterPath hill(QPointF(-10, H + 10));
                for (double xx = -10; xx <= W + 10; xx += 6) hill.lineTo(xx, hillAt(c, k, xx));
                hill.lineTo(W + 10, H + 10);
                hill.closeSubpath();
                QLinearGradient g(0, Y0(c, k) - AMP(k), 0, H);
                g.setColorAt(0, mix(base, fogC, 0.12));
                g.setColorAt(1, mix(base, Qt::black, 0.25 * (1 - fogK)));
                x.fillPath(hill, g);
            }
            // urze: manchas arroxeadas e escuras pelo morro (mais miúdo longe)
            const QColor hz = mix(heather, fogC, fogK), dz = mix(base, Qt::black, 0.25);
            for (int i = 0; i < 260 * (k + 1); ++i) {
                const double xx = R(-10, W + 10), top = hillAt(c, k, xx), yy = top + std::pow(rnd(), 1.4) * (H - top);
                x.setBrush(alpha(rnd() < 0.5 ? hz : dz, R(0.15, 0.4)));
                x.drawEllipse(QPointF(xx, yy), R(2, 6) * SC[k], R(1, 2.5) * SC[k]);
            }
            x.setBrush(Qt::NoBrush);
            const QColor ink = mix(mix(pal.app, Qt::black, 0.5 + 0.1 * k), fogC, fogK * 0.95);
            if (k == 0) {
                // ruína da casinha com a chaminé, lá longe
                const double rxx = zx(c, 120), ry = hillAt(c, 0, rxx) + 2;
                x.fillRect(QRectF(rxx - 14, ry - 12, 28, 12), ink);
                QPolygonF top;
                top << QPointF(rxx - 14, ry - 12) << QPointF(rxx - 4, ry - 20) << QPointF(rxx + 2, ry - 15) << QPointF(rxx + 6, ry - 18) << QPointF(rxx + 14, ry - 12);
                x.setBrush(ink);
                x.drawPolygon(top);
                x.setBrush(Qt::NoBrush);
                x.fillRect(QRectF(rxx + 8, ry - 24, 4, 12), ink);
                x.fillRect(QRectF(rxx - 8, ry - 9, 4, 5), alpha(fogC, 0.5));
                x.fillRect(QRectF(rxx + 3, ry - 9, 4, 5), alpha(fogC, 0.5));
            } else if (k == 1) {
                // o círculo de pedras erguidas no alto da colina
                const double cx = zx(c, 320), cy = hillAt(c, 1, cx) + 4;
                struct St { double x, y, h, w; bool back; };
                std::vector<St> stones;
                for (int i = 0; i < 9; ++i) { const double a = i / 9.0 * 6.28 + R(-0.15, 0.15); stones.push_back({ cx + std::cos(a) * 34, cy + std::sin(a) * 6, R(10, 18) * (0.85 + 0.15 * std::sin(a)), R(4, 6.5), std::sin(a) < 0 }); }
                std::stable_sort(stones.begin(), stones.end(), [](const St& a, const St& b) { return a.y < b.y; });
                for (const St& st : stones) {
                    const QColor sc2 = mix(ink, fogC, st.back ? 0.12 : 0);
                    QLinearGradient sg(st.x - st.w, 0, st.x + st.w, 0);
                    sg.setColorAt(0, mix(sc2, fogC, 0.18));
                    sg.setColorAt(1, mix(sc2, Qt::black, 0.2));
                    QPainterPath sp(QPointF(st.x - st.w, st.y));
                    sp.lineTo(st.x - st.w * 0.8, st.y - st.h);
                    sp.quadTo(st.x, st.y - st.h - 3, st.x + st.w * 0.8, st.y - st.h + 1);
                    sp.lineTo(st.x + st.w, st.y);
                    sp.closeSubpath();
                    x.fillPath(sp, sg);
                }
                for (double tx : { zx(c, 80), zx(c, 210), rx(c, 1240), rx(c, 1380) }) gnarl(x, tx, hillAt(c, 1, tx) + 2, -M_PI / 2 + 0.15, R(14, 22), 3, 0, ink, 1);
            } else if (k == 2) {
                // o muro de pedra seca acompanhando o relevo, e duas árvores médias
                for (double xx = -10; xx < W + 10; xx += 4.5) {
                    const double yy = hillAt(c, 2, xx) + 26 + std::sin(xx * 0.008) * 8;
                    for (int r = 0; r < 2; ++r) {
                        QPainterPath st;
                        st.addRoundedRect(QRectF(xx + (r % 2) * 2.2, yy - r * 3.6, R(3.5, 5), 3.6), 1.4, 1.4);
                        x.fillPath(st, mix(ink, rnd() < 0.5 ? QColor(Qt::black) : fogC, R(0.05, 0.25)));
                    }
                }
                for (double tx : { zx(c, 175), rx(c, 1300) }) gnarl(x, tx, hillAt(c, 2, tx) + 4, -M_PI / 2 + 0.2, R(34, 46), 6, 0, ink, 1);
            } else {
                // a árvore grande da frente (com o corvo), pedras com musgo e a poça de turfa
                const double tx = zx(c, 120);
                m_wantPerch = true;
                gnarl(x, tx, hillAt(c, 3, tx) + 10, -M_PI / 2 + 0.22, 92, 13, 0, ink, 1);
                m_wantPerch = false;
                const double rocks[4][2] = { { zx(c, 260), 16 }, { zx(c, 292), 9 }, { rx(c, 1180), 20 }, { rx(c, 1215), 11 } };
                for (const auto& rr : rocks) {
                    const double rxx = rr[0], r = rr[1], ry = hillAt(c, 3, rxx) + 22;
                    QLinearGradient rg(rxx - r, ry - r, rxx + r, ry);
                    rg.setColorAt(0, mix(ink, fogC, 0.25));
                    rg.setColorAt(1, mix(ink, Qt::black, 0.3));
                    QPainterPath rp(QPointF(rxx + r, ry + 2));
                    rp.lineTo(rxx - r, ry + 2);
                    rp.lineTo(rxx - r, ry);
                    rp.arcTo(QRectF(rxx - r, ry - r * 0.62, r * 2, r * 1.24), 180, -180);
                    rp.closeSubpath();
                    x.fillPath(rp, rg);
                    x.save();
                    x.translate(rxx - r * 0.2, ry - r * 0.45);
                    x.rotate(qRadiansToDegrees(-0.2));
                    x.setBrush(alpha(mix(hex("#5a7a3a"), pal.app, 0.4), 0.55));
                    x.drawEllipse(QPointF(0, 0), r * 0.6, r * 0.18);
                    x.restore();
                    x.setBrush(Qt::NoBrush);
                }
                // poça: o céu refletido, com a borda escura
                const double px = zx(c, 360), py = H - 34;
                x.setBrush(mix(ink, Qt::black, 0.2));
                x.drawEllipse(QPointF(px, py), 46, 9);
                QLinearGradient pg(0, py - 8, 0, py + 8);
                pg.setColorAt(0, mix(fogC, Qt::white, 0.15));
                pg.setColorAt(1, mix(fogC, Qt::black, 0.25));
                x.setBrush(pg);
                x.drawEllipse(QPointF(px, py), 42, 7);
                x.setBrush(Qt::NoBrush);
            }
            x.end();
            m_planes.push_back(pm);
        }
        // urze e capim alto da frente, que balançam
        m_tufts.clear();
        for (double xx = -10; xx < W + 10; xx += R(5, 14)) {
            const double yy = hillAt(c, 3, xx) + R(4, 40);
            if (yy > H - 4) continue;
            Tuft t{ xx, yy, {}, R(0, 6.28), rnd() < 0.35 };
            const int nb = 3 + RI(4);
            for (int i = 0; i < nb; ++i) t.blades.push_back({ R(-4, 4), R(8, 22), R(-0.4, 0.4) });
            m_tufts.push_back(t);
        }
        // a névoa que desliza entre os planos: imagem pronta de manchas
        m_fogCol = todPick(c, mix(pal.app, hex("#f0f2f4"), 0.6), mix(pal.app, hex("#e8c8c0"), 0.5), mix(pal.app, hex("#a8b2be"), 0.4));
        m_fog = layer(c, W, 300);
        QPainter x(&m_fog);
        const QPixmap dot = softDot(m_fogCol, 0.2);
        for (int i = 0; i < 80; ++i) {
            const double w = R(160, 420), h = w * R(0.22, 0.4), cx = R(0, W), cy = R(80, 220);
            for (double ox : { 0.0, -W, W }) { x.setOpacity(R(0.25, 0.5)); blit(x, dot, cx + ox - w / 2, cy - h / 2, w, h); }
        }
    }

    QString m_key;
    std::vector<QPixmap> m_planes;
    QPixmap m_fog;
    QColor m_fogCol;
    std::array<std::array<double, 3>, 4> m_ph{};
    std::vector<Tuft> m_tufts;
    Flock m_crows;
    QPointF m_perch;
    bool m_hasPerch = false, m_wantPerch = false;
    double m_t = 0, m_nextCrows = 4;
};

// ============================================================ campina dos vaga-lumes
// Lua crescente e estrelas; duas linhas de mata na bruma; um carvalho com a copa em folhagem miúda
// e a borda pegando o luar; capim em perspectiva, margaridas, cerca; a lagoa com a lua e os
// vaga-lumes refletidos, taboas; vaga-lumes em profundidade. De dia eles somem.
class Meadow2 : public Layer {
public:
    void init(Ctx& c) override
    {
        m_key.clear();
        m_back.reset();
        m_stars = QPixmap();
        m_flies.clear();
        const double HZ = c.H * 0.56;
        const int n = qMax(1, int(std::lround(90 * INT(c))));
        for (int i = 0; i < n; ++i) {
            const double y = R(HZ - 40, c.H - 20), z = qMax(0.25, (y - HZ + 40) / (c.H - HZ + 40));
            m_flies.push_back({ R(0, c.W), y, R(20, 70) * z, R(10, 30) * z, R(0.06, 0.2), R(0.1, 0.26), R(0, 6.28), R(0.5, 1.2), z });
        }
    }
    void step(double dt, Ctx&) override
    {
        m_t += dt;
        for (Fly& f : m_flies) f.cx += 5 * dt * std::sin(m_t * 0.05 + f.ph);
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const QString key = todKey(c);
        if (key != m_key || !m_back.valid(c)) {
            Seeded sd(0x3ead02u);
            build(pal, c);
            m_key = key;
        }
        const double night = 1 - m_L, HZ = c.H * 0.56;
        const QColor moonC = todPick(c, hex("#fff6dc"), hex("#ffc890"), hex("#e8eefc"));
        p.setPen(Qt::NoPen);
        skyTOD(p, pal, c, mix(pal.app, Qt::black, 0.2), mix(pal.app, hex("#1a2a2a"), 0.3));
        if (m_stars.isNull() || qAbs(m_starsPx - c.px) > 0.01) {
            m_stars = layer(c, c.W, c.H);
            m_starsPx = c.px;
            QPainter x(&m_stars);
            for (int i = 0; i < 260; ++i) x.fillRect(QRectF(R(0, c.W), R(0, HZ * 0.9), R(0.8, 1.5), R(0.8, 1.5)), alpha(Qt::white, R(0.15, 0.6)));
        }
        p.setOpacity(night);
        blit(p, m_stars, 0, 0, c.W, c.H);
        p.setOpacity(1);
        // lua crescente (de dia some)
        const double mx = rx(c, 1210), my = 120, ma = night * 0.95;
        if (ma > 0.03) {
            glow(p, mx, my, 110, moonC, 0.22 * ma);
            disc(p, mx, my, 16, alpha(moonC, ma));
            disc(p, mx + 7, my - 4, 14, mix(pal.app, Qt::black, 0.2));
        }
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        // lua e céu tremendo na lagoa
        const double px = zx(c, 330), PY = c.H * 0.8, PW = 92, PH = 16;
        {
            p.save();
            QPainterPath pond;
            pond.addEllipse(QPointF(px, PY), PW, PH);
            p.setClipPath(pond, Qt::IntersectClip);
            for (int i = 0; i < 9; ++i) {
                const double yy = PY - PH + 4 + i * 3.4, w = (14 + i * 3) * (0.7 + 0.3 * std::sin(m_t * 2 + i));
                p.fillRect(QRectF(px + 30 - w / 2 + std::sin(m_t * 1.4 + i) * 3, yy, w, 1.4), alpha(moonC, (0.35 - i * 0.03) * night + 0.05));
            }
            p.restore();
        }
        // taboas balançando
        {
            const QColor reedC = mix(pal.app, Qt::black, 0.55);
            QPainterPath stems;
            for (const Reed& r : m_reeds) {
                const double sw = std::sin(m_t * 1.1 + r.ph) * 3 + std::sin(m_t * 0.4) * 2;
                stems.moveTo(r.x, r.y);
                stems.quadTo(r.x + sw * 0.3, r.y - r.h * 0.6, r.x + sw, r.y - r.h);
                if (r.cat) {
                    p.save();
                    p.translate(r.x + sw * 0.9, r.y - r.h * 0.82);
                    p.rotate(qRadiansToDegrees(sw * 0.03));
                    p.setBrush(reedC);
                    p.drawEllipse(QPointF(0, 0), 2, 6);
                    p.setBrush(Qt::NoBrush);
                    p.restore();
                }
            }
            p.setPen(pen(reedC, 1.3));
            p.drawPath(stems);
            p.setPen(Qt::NoPen);
        }
        // vaga-lumes: brilho, o capim aceso em volta e o reflexo na lagoa
        if (m_fly.isNull()) m_fly = softDot(mix(hex("#d8f56a"), pal.acc, 0.25));
        const double dim = qMax(0.0, 1 - 1.2 * m_L);
        if (dim > 0.02) {
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            for (const Fly& f : m_flies) {
                const double x = f.cx + std::sin(m_t * f.fx * 6.28 + f.ph) * f.ax, y = f.cy + std::sin(m_t * f.fy * 6.28 + f.ph * 1.7) * f.ay;
                const double g = std::pow(qMax(0.0, std::sin(m_t * f.bl * 2.2 + f.ph)), 1.2);
                if (g < 0.02) continue;
                const double big = 70 * f.z, s = 24 * f.z;
                p.setOpacity(g * 0.18 * dim * f.z);
                blit(p, m_fly, x - big / 2, y - big / 2, big, big);
                p.setOpacity(g * (0.55 + 0.45 * f.z) * dim);
                blit(p, m_fly, x - s / 2, y - s / 2, s, s);
                p.setOpacity(qMin(1.0, g * 1.2) * dim);
                disc(p, x, y, 1.3 * f.z + 0.3, hex("#fbffe0"));
                if (std::abs(x - px) < PW * 0.9 && y < PY && y > PY - 120) {
                    const double ry = PY + (PY - y) * 0.18;
                    if (std::abs(ry - PY) < PH * 0.8) { p.setOpacity(g * 0.5 * dim); blit(p, m_fly, x - s / 2, ry - s / 4, s, s / 2); }
                }
            }
            p.setOpacity(1);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        // capim da frente balançando com a brisa
        {
            const QColor gc = mix(todPick(c, mix(pal.app, hex("#3a6a2a"), 0.6), mix(pal.app, hex("#3a3a20"), 0.5), mix(pal.app, hex("#081008"), 0.5)), Qt::black, 0.2);
            QVector<QLineF> gl;
            for (const Blade& b : m_blades) {
                const double wv = std::sin(m_t * 0.9 + b.x * 0.012) * 0.22 + std::sin(m_t * 2.1 + b.ph) * 0.05;
                const QPointF a(b.x, b.y + 2), m(b.x + (b.lean + wv) * b.h * 0.3, b.y - b.h * 0.5), e(b.x + (b.lean + wv) * b.h, b.y - b.h);
                gl.append(QLineF(a, m));
                gl.append(QLineF(m, e));
            }
            p.setBrush(Qt::NoBrush);
            p.setPen(pen(gc, 1.5));
            p.drawLines(gl);
            p.setPen(Qt::NoPen);
        }
    }

private:
    struct Fly { double cx, cy, ax, ay, fx, fy, ph, bl, z; };
    struct Reed { double x, y, h, ph; bool cat; };
    struct Blade { double x, y, h, ph, lean; };

    void build(const Pal& pal, const Ctx& c)
    {
        m_L = c.tod == QLatin1String("theme") ? 0 : LV(pal, c);
        const double W = c.W, H = c.H, HZ = H * 0.56, night = 1 - m_L;
        QPixmap pm = layer(c, W, H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing);
        x.setPen(Qt::NoPen);
        const QColor moonC = todPick(c, hex("#fff6dc"), hex("#ffc890"), hex("#e8eefc"));
        const QColor haze = todPick(c, mix(pal.app, hex("#c8dcc0"), 0.55), mix(pal.app, hex("#c89a8a"), 0.45), mix(pal.app, hex("#4a6058"), 0.5));
        const QColor grass = todPick(c, mix(pal.app, hex("#4a7a3a"), 0.6), mix(pal.app, hex("#4a4a2a"), 0.5), mix(pal.app, hex("#2a4a30"), 0.5));
        // duas linhas de mata ao fundo: copas arredondadas, a de trás mais apagada
        for (int k = 0; k < 2; ++k) {
            const double y0 = HZ - 10 + k * 14;
            const QColor col = mix(mix(grass, Qt::black, 0.3 + 0.15 * k), haze, k ? 0.3 : 0.55);
            QPainterPath ml(QPointF(-10, H));
            ml.lineTo(-10, y0);
            for (double xx = -10; xx < W + 20; xx += R(10, 22)) { const double r = R(8, 20) * (1 + k * 0.4); ml.lineTo(xx, y0 - r * R(0.6, 1.4) + std::sin(xx * 0.01 + k) * 8); }
            ml.lineTo(W + 20, H);
            ml.closeSubpath();
            for (double xx = -10; xx < W + 20; xx += R(14, 26)) { const double r = R(9, 18) * (1 + k * 0.4); ml.addEllipse(QPointF(xx, y0 - r * 0.4 + std::sin(xx * 0.01 + k) * 8), r, r); }
            ml.setFillRule(Qt::WindingFill);
            x.fillPath(ml, col);
        }
        // a campina: degradê do horizonte pra frente
        vfill(x, QRectF(0, HZ + 6, W, H - HZ), mix(grass, haze, 0.35), mix(grass, Qt::black, 0.35), HZ, H);
        // bruma assentada no pé da mata
        {
            QLinearGradient bg(0, HZ - 30, 0, HZ + 40);
            bg.setColorAt(0, alpha(haze, 0));
            bg.setColorAt(0.5, alpha(haze, 0.45));
            bg.setColorAt(1, alpha(haze, 0));
            x.fillRect(QRectF(0, HZ - 30, W, 70), bg);
        }
        // capim parado em perspectiva
        {
            LineBatch dark, light;
            for (int i = 0; i < 2400; ++i) {
                const double t = std::pow(rnd(), 1.6), y = HZ + 8 + t * (H * 0.86 - HZ), h = 2 + t * 14, xx = R(0, W);
                (rnd() < 0.5 ? dark : light).add(xx, y, xx + R(-2, 2) * (1 + t), y - h, R(0.05, 0.3) * 2.3);
            }
            dark.flush(x, mix(grass, Qt::black, 0.3), 1.1);
            light.flush(x, mix(grass, haze, 0.3), 1.1);
        }
        // margaridas pegando a luz da lua
        const QColor petal = todPick(c, hex("#ffffff"), hex("#ffe8d0"), hex("#dfe6f0"));
        for (int i = 0; i < 260; ++i) {
            const double t = std::pow(rnd(), 1.3), y = HZ + 12 + t * (H - HZ - 20), xx = R(0, W), r = 0.6 + t * 1.8;
            disc(x, xx, y, r, alpha(petal, 0.35 + 0.4 * t));
            if (t > 0.5) disc(x, xx, y, r * 0.35, alpha(hex("#e8c840"), 0.6));
        }
        // a cerca de madeira atravessando em diagonal
        {
            const QColor wood = mix(mix(grass, hex("#3a2a1a"), 0.5), Qt::black, 0.3 * night);
            const QPointF f0(zx(c, 20), H * 0.7), f1(zx(c, 440), H * 0.63);
            for (int i = 0; i <= 9; ++i) {
                const double t = i / 9.0, fx = lerp(f0.x(), f1.x(), t), fy = lerp(f0.y(), f1.y(), t), ph = lerp(30, 18, t);
                x.fillRect(QRectF(fx - 1.6 * (1 - t * 0.4), fy - ph, 3.2 * (1 - t * 0.4), ph), wood);
                x.fillRect(QRectF(fx + 0.6, fy - ph, 0.9, ph), alpha(moonC, 0.18));
            }
            x.setPen(QPen(wood, 2));
            for (double hgt : { 0.8, 0.45 }) x.drawLine(QPointF(f0.x(), f0.y() - 30 * hgt), QPointF(f1.x(), f1.y() - 18 * hgt));
            x.setPen(Qt::NoPen);
        }
        // o carvalho: tronco grosso com a raiz, galhos e a copa de folhagem miúda com a borda ao luar
        {
            const double ox = zx(c, 140), oy = H * 0.67;
            const QColor trunk = mix(mix(grass, hex("#1a120a"), 0.55), Qt::black, 0.3 * night), leaf = mix(mix(grass, Qt::black, 0.28 + 0.12 * night), haze, 0.06);
            QPainterPath tr(QPointF(ox - 30, oy + 2));
            tr.quadTo(ox - 14, oy - 8, ox - 13, oy - 40);
            tr.quadTo(ox - 12, oy - 70, ox - 9, oy - 92);
            tr.lineTo(ox + 11, oy - 92);
            tr.quadTo(ox + 13, oy - 60, ox + 14, oy - 36);
            tr.quadTo(ox + 16, oy - 6, ox + 32, oy + 2);
            tr.closeSubpath();
            x.fillPath(tr, trunk);
            x.fillRect(QRectF(ox + 6, oy - 90, 3, 84), alpha(moonC, 0.1 * night + 0.05));
            const double brs[7][3] = { { -2.5, 74, 9 }, { -0.7, 80, 9 }, { -1.7, 66, 7 }, { -2.1, 54, 5 }, { -1.15, 58, 5 }, { -2.9, 46, 4 }, { -0.3, 50, 4 } };
            for (const auto& b : brs) {
                QPainterPath bp(QPointF(ox, oy - 86));
                bp.quadTo(ox + std::cos(b[0]) * b[1] * 0.5, oy - 86 + std::sin(b[0]) * b[1] * 0.35, ox + std::cos(b[0]) * b[1], oy - 86 + std::sin(b[0]) * b[1]);
                x.setBrush(Qt::NoBrush);
                x.setPen(pen(trunk, b[2]));
                x.drawPath(bp);
            }
            x.setPen(Qt::NoPen);
            const double lobes[5][4] = { { 0, -150, 95, 62 }, { -62, -128, 52, 42 }, { 66, -130, 55, 44 }, { -24, -186, 58, 40 }, { 34, -182, 54, 38 } };
            std::vector<std::array<double, 3>> leaves;
            for (const auto& lb : lobes)
                for (int i = 0; i < 70; ++i) { const double a = R(0, 6.28), rr = std::sqrt(rnd()); leaves.push_back({ ox + lb[0] + std::cos(a) * lb[2] * rr, oy + lb[1] + std::sin(a) * lb[3] * rr, R(7, 13) }); }
            auto blob = [&](double dx, double dy, const QColor& col) {
                QPainterPath bp;
                bp.setFillRule(Qt::WindingFill);
                for (const auto& l : leaves) bp.addEllipse(QPointF(l[0] + dx, l[1] + dy), l[2], l[2]);
                x.fillPath(bp, col);
            };
            blob(0, 0, mix(leaf, moonC, 0.22));
            blob(-3, 4, leaf);
            for (int i = 0; i < 70; ++i) { const auto& l = leaves[size_t(RI(int(leaves.size())))]; disc(x, l[0] + R(-4, 4), l[1] + R(2, 6), R(2, 5), alpha(Qt::black, R(0.15, 0.35))); }
            for (int i = 0; i < 90; ++i) { const auto& l = leaves[size_t(RI(int(leaves.size())))]; disc(x, l[0] + R(0, 6), l[1] - R(2, 7), R(1, 2.5), alpha(moonC, R(0.05, 0.14))); }
            x.setBrush(alpha(Qt::black, 0.25));
            x.drawEllipse(QPointF(ox + 20, oy + 4), 80, 8);
            x.setBrush(Qt::NoBrush);
        }
        // a lagoa: margem escura e o céu refletido (a lua tremida é desenhada a cada quadro)
        const double px = zx(c, 330), PY = H * 0.8, PW = 92, PH = 16;
        x.setBrush(mix(grass, Qt::black, 0.45));
        x.drawEllipse(QPointF(px, PY), PW + 6, PH + 3);
        {
            QLinearGradient wg(0, PY - PH, 0, PY + PH);
            wg.setColorAt(0, mix(mix(pal.app, Qt::black, 0.3), haze, 0.25));
            wg.setColorAt(1, mix(pal.app, Qt::black, 0.5));
            x.setBrush(wg);
            x.drawEllipse(QPointF(px, PY), PW, PH);
        }
        x.setBrush(Qt::NoBrush);
        x.end();
        m_back.set(c, pm);
        // taboas na margem
        m_reeds.clear();
        for (int i = 0; i < 16; ++i) {
            const int side = i < 9 ? -1 : 1;
            const double a = side < 0 ? R(2.4, 3.6) : R(-0.5, 0.6);
            m_reeds.push_back({ px + std::cos(a) * (PW + R(-4, 6)), PY + std::sin(a) * PH + R(0, 6), R(22, 46), R(0, 6.28), rnd() < 0.5 });
        }
        // capim da frente, que balança
        m_blades.clear();
        for (int i = 0; i < 260; ++i) { const double y = H - R(0, H * 0.14); m_blades.push_back({ R(-10, W + 10), y, R(14, 36) * (0.6 + (y - H * 0.86) / (H * 0.14) * 0.6), R(0, 6.28), R(-0.3, 0.3) }); }
    }

    QString m_key;
    Cache m_back;
    QPixmap m_stars, m_fly;
    double m_starsPx = 0, m_L = 0, m_t = 0;
    std::vector<Fly> m_flies;
    std::vector<Reed> m_reeds;
    std::vector<Blade> m_blades;
};

// ============================================================ céu estrelado (Stargazer)
// A Via Láctea (miolo cremoso, faixas escuras de poeira, aglomerados, tons de nebulosa), estrelas de
// várias cores cintilando, estrela cadente, satélite e avião; cordilheira com o brilho do ar no
// horizonte; colina com a barraca acesa, telescópio e um pinheiro solitário.
class Stargazer : public Layer {
public:
    void init(Ctx& c) override
    {
        m_sky.reset();
        m_stars.clear();
        const double HZ = c.H * 0.7;
        static const char* SC[5] = { "#ffffff", "#ffffff", "#cfe0ff", "#ffe8c0", "#ffd0b0" };
        const int n = qMax(1, int(std::lround(380 * INT(c))));
        for (int i = 0; i < n; ++i) m_stars.push_back({ R(0, c.W), R(0, HZ), std::pow(rnd(), 3) * 1.7 + 0.35, R(0, 6.28), R(0.5, 2.5), hex(SC[RI(5)]) });
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_next -= dt;
        m_nextSat -= dt;
        m_nextPlane -= dt;
        const double HZ = c.H * 0.7;
        if (m_next <= 0) {
            const QPointF v = visPoint(c);
            m_shoot = { true, v.x() - 120, qMax(10.0, v.y() - 200), R(260, 420), R(120, 220), 0, 0.9 };
            m_next = R(18, 45);
        }
        if (m_shoot.on) { m_shoot.t += dt; if (m_shoot.t > m_shoot.d) m_shoot.on = false; }
        if (!m_sat.on && m_nextSat <= 0) m_sat = { true, R(0, c.W * 0.5), R(40, HZ * 0.5), R(14, 22), R(4, 9), 0 };
        if (m_sat.on) { m_sat.x += m_sat.vx * dt; m_sat.y += m_sat.vy * dt; m_sat.t += dt; if (m_sat.t > 40) { m_sat.on = false; m_nextSat = R(20, 40); } }
        if (!m_plane.on && m_nextPlane <= 0) m_plane = { true, c.W + 20, R(60, HZ * 0.6), R(28, 40) };
        if (m_plane.on) { m_plane.x -= m_plane.v * dt; m_plane.y -= 2 * dt; if (m_plane.x < -30) { m_plane.on = false; m_nextPlane = R(30, 60); } }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        if (!m_sky.valid(c)) {
            Seeded sd(0x57a26u);
            build(pal, c);
        }
        p.setPen(Qt::NoPen);
        blit(p, m_sky.pm, 0, 0, c.W, c.H);
        // estrelas cintilando, em cores diferentes (as maiores com a cruz de brilho)
        for (const Star& s : m_stars) {
            const double a = 0.35 + 0.65 * std::pow(0.5 + 0.5 * std::sin(m_t * s.f + s.ph), 2);
            disc(p, s.x, s.y, s.r, alpha(s.c, a));
            if (s.r > 1.5) {
                p.fillRect(QRectF(s.x - s.r * 3, s.y - 0.3, s.r * 6, 0.6), alpha(s.c, a * 0.25));
                p.fillRect(QRectF(s.x - 0.3, s.y - s.r * 3, 0.6, s.r * 6), alpha(s.c, a * 0.25));
            }
        }
        if (m_sat.on) { const double a = std::min({ 1.0, m_sat.t / 2, (40 - m_sat.t) / 2 }); p.fillRect(QRectF(m_sat.x, m_sat.y, 1.6, 1.6), alpha(Qt::white, 0.8 * qMax(0.0, a))); }
        if (m_plane.on) {
            const bool b = int(std::floor(m_t * 1.3)) % 2 == 0;
            p.fillRect(QRectF(m_plane.x, m_plane.y, 2, 2), alpha(Qt::white, b ? 0.95 : 0.25));
            p.fillRect(QRectF(m_plane.x + 7, m_plane.y + 1, 2, 2), alpha(hex("#ff4040"), b ? 0.25 : 0.9));
        }
        if (m_shoot.on) {
            const double k = m_shoot.t / m_shoot.d, x = lerp(m_shoot.x, m_shoot.x + m_shoot.dx, k), y = lerp(m_shoot.y, m_shoot.y + m_shoot.dy, k), a = std::sin(k * M_PI);
            QLinearGradient g(x, y, x - m_shoot.dx * 0.25, y - m_shoot.dy * 0.25);
            g.setColorAt(0, alpha(Qt::white, a));
            g.setColorAt(1, alpha(Qt::white, 0));
            p.setPen(QPen(QBrush(g), 1.8, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(x, y), QPointF(x - m_shoot.dx * 0.25, y - m_shoot.dy * 0.25));
            p.setPen(Qt::NoPen);
        }
        blit(p, m_fg, 0, 0, c.W, c.H);
        // a luz da barraca no chão em volta, tremendo de leve como lanterna
        const double f = 0.85 + 0.15 * std::sin(m_t * 7) * std::sin(m_t * 2.3);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        glow(p, m_tent.x(), m_tent.y() - 8, 90, hex("#ffb060"), 0.22 * f);
        p.save();
        p.translate(m_tent.x(), m_tent.y() + 4);
        p.scale(1, 0.22);
        glow(p, 0, 0, 110, hex("#ffb060"), 0.25 * f);
        p.restore();
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    struct Star { double x, y, r, ph, f; QColor c; };
    struct Shoot { bool on = false; double x = 0, y = 0, dx = 0, dy = 0, t = 0, d = 0.9; };
    struct Sat { bool on = false; double x = 0, y = 0, vx = 0, vy = 0, t = 0; };
    struct Plane { bool on = false; double x = 0, y = 0, v = 0; };

    void build(const Pal& pal, const Ctx& c)
    {
        const double W = c.W, H = c.H, HZ = H * 0.7;
        // eixo da Via Láctea: da montanha à esquerda subindo pro alto, atravessando a faixa de mesa à vista
        const QPointF A0(zx(c, 110), HZ + 60), A1(zx(c, 680), -220);
        const double nx0 = -(A1.y() - A0.y()), ny0 = A1.x() - A0.x(), nl = std::hypot(nx0, ny0), nx = nx0 / nl, ny = ny0 / nl;
        auto along = [&](double t, double d) { return QPointF(lerp(A0.x(), A1.x(), t) + nx * d, lerp(A0.y(), A1.y(), t) + ny * d); };
        QPixmap pm = layer(c, W, H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing);
        x.setPen(Qt::NoPen);
        {
            QLinearGradient g(0, 0, 0, HZ);
            g.setColorAt(0, mix(pal.app, Qt::black, 0.35));
            g.setColorAt(0.75, mix(pal.app, hex("#1b1a3a"), 0.35));
            g.setColorAt(1, mix(pal.app, hex("#2a3048"), 0.4));
            x.fillRect(QRectF(0, 0, W, H), g);
            QLinearGradient ag(0, HZ - 120, 0, HZ);
            ag.setColorAt(0, alpha(hex("#5aa078"), 0));
            ag.setColorAt(0.7, alpha(hex("#5aa078"), 0.08));
            ag.setColorAt(1, alpha(hex("#d88a4a"), 0.14));
            x.fillRect(QRectF(0, HZ - 120, W, 120), ag);
        }
        // brilho difuso: miolo cremoso e tons de nebulosa
        const QPixmap cream = softDot(mix(hex("#f4e6c8"), pal.app, 0.2), 0.15), blue = softDot(mix(hex("#9ab0e8"), pal.app, 0.2), 0.15), pink = softDot(mix(hex("#e8a0c0"), pal.app, 0.25), 0.15);
        for (int i = 0; i < 620; ++i) {
            const double t = rnd(), d = (rnd() + rnd() + rnd() - 1.5) * 95, core = std::exp(-std::pow((t - 0.38) / 0.2, 2));
            const QPointF q = along(t, d);
            const double s = R(40, 140) * (1 + core * 0.6);
            x.setOpacity((0.05 + 0.11 * core) * qMax(0.0, 1 - std::abs(d) / 240));
            const QPixmap& dot = (core > 0.4 && rnd() < 0.7) ? cream : (rnd() < 0.5 ? blue : pink);
            blit(x, dot, q.x() - s / 2, q.y() - s / 2, s, s);
        }
        x.setOpacity(1);
        // aglomerados densos de estrelinhas dentro da faixa
        {
            QVector<QRectF> wh, cr;
            for (int i = 0; i < 12000; ++i) {
                const double t = rnd(), d = (rnd() + rnd() + rnd() + rnd() - 2) * 80;
                const QPointF q = along(t, d);
                if (q.y() > HZ) continue;
                (rnd() < 0.7 ? wh : cr).append(QRectF(q.x(), q.y(), R(0.4, 1.1), R(0.4, 1.1)));
            }
            x.setBrush(alpha(Qt::white, 0.35));
            x.drawRects(wh);
            x.setBrush(alpha(hex("#ffe8c8"), 0.35));
            x.drawRects(cr);
            x.setBrush(Qt::NoBrush);
        }
        // faixas escuras de poeira cortando o miolo
        const QColor dust = mix(pal.app, Qt::black, 0.4);
        for (int lane = 0; lane < 3; ++lane) {
            static const double OFF[3] = { -14, 10, 30 }, WD[3] = { 26, 16, 12 };
            for (int i = 0; i < 70; ++i) {
                const double t = R(0.12, 0.85), wob = std::sin(t * 18 + lane * 2) * 18 + std::sin(t * 41 + lane) * 7, s = WD[lane] * R(1, 2.2);
                const QPointF q = along(t, OFF[lane] + wob);
                QRadialGradient dg(q, s);
                dg.setColorAt(0, alpha(dust, 0.22));
                dg.setColorAt(1, alpha(Qt::black, 0));
                x.fillRect(QRectF(q.x() - s, q.y() - s, s * 2, s * 2), dg);
            }
        }
        x.end();
        m_sky.set(c, pm);
        // primeiro plano: cordilheira, colina, pinheiro, barraca e telescópio
        m_fg = layer(c, W, H);
        QPainter y(&m_fg);
        y.setRenderHint(QPainter::Antialiasing);
        y.setPen(Qt::NoPen);
        for (int k = 0; k < 2; ++k) {
            static const double DY[2] = { 30, 6 }, AM[2] = { 70, 40 }, FR[2] = { 0.006, 0.009 }, DK[2] = { 0.35, 0.55 };
            QPainterPath mt(QPointF(-10, H));
            for (double xx = -10; xx <= W + 10; xx += 6) mt.lineTo(xx, HZ - DY[k] - std::abs(std::sin(xx * FR[k] + k * 2)) * AM[k] - std::sin(xx * 0.03 + k) * 8 - R(0, 3));
            mt.lineTo(W + 10, H);
            mt.closeSubpath();
            y.fillPath(mt, mix(pal.app, Qt::black, DK[k]));
        }
        const QColor hc = mix(pal.app, Qt::black, 0.72);
        auto hill = [&](double xx) { return H - 120 + std::sin(xx * 0.004 + 0.6) * 30 + std::sin(xx * 0.013) * 8; };
        {
            QPainterPath hp(QPointF(-10, H));
            for (double xx = -10; xx <= W + 10; xx += 6) hp.lineTo(xx, hill(xx));
            hp.lineTo(W + 10, H);
            hp.closeSubpath();
            y.fillPath(hp, hc);
            QPainterPath gp;
            for (double xx = 0; xx < W; xx += 2.5) { const double yy = hill(xx); gp.moveTo(xx, yy + 1); gp.lineTo(xx + R(-1.5, 1.5), yy - R(2, 7)); }
            y.setPen(QPen(hc, 1));
            y.drawPath(gp);
            y.setPen(Qt::NoPen);
        }
        const double tx = zx(c, 260);
        // pinheiro solitário
        {
            const double px = tx - 150, py = hill(px);
            QPainterPath pt(QPointF(px - 2, py));
            pt.lineTo(px - 2, py - 20);
            for (int i = 0; i < 8; ++i) { const double yy = py - 20 - i * 12, w = 22 * (1 - i / 9.0); pt.lineTo(px - w, yy); pt.lineTo(px - w * 0.4, yy - 5); }
            pt.lineTo(px, py - 122);
            for (int i = 7; i >= 0; --i) { const double yy = py - 20 - i * 12, w = 22 * (1 - i / 9.0); pt.lineTo(px + w * 0.4, yy - 5); pt.lineTo(px + w, yy); }
            pt.lineTo(px + 2, py - 20);
            pt.lineTo(px + 2, py);
            pt.closeSubpath();
            y.fillPath(pt, hc);
        }
        // a barraca: lona acesa por dentro, a porta aberta, as cordinhas
        {
            const double ty = hill(tx) + 3, tw = 62, th = 44;
            QLinearGradient tg(0, ty - th, 0, ty);
            tg.setColorAt(0, mix(hex("#c86a2a"), pal.app, 0.35));
            tg.setColorAt(1, mix(hex("#ffb060"), pal.app, 0.1));
            QPolygonF tent, sh, door;
            tent << QPointF(tx - tw / 2, ty) << QPointF(tx, ty - th) << QPointF(tx + tw / 2, ty);
            sh << QPointF(tx, ty - th) << QPointF(tx + tw / 2, ty) << QPointF(tx + tw * 0.2, ty);
            door << QPointF(tx - 7, ty) << QPointF(tx, ty - th * 0.62) << QPointF(tx + 5, ty);
            y.setBrush(tg); y.drawPolygon(tent);
            y.setBrush(alpha(Qt::black, 0.25)); y.drawPolygon(sh);
            y.setBrush(mix(hex("#fff0c0"), pal.app, 0.05)); y.drawPolygon(door);
            y.setBrush(Qt::NoBrush);
            y.setPen(QPen(mix(pal.app, Qt::black, 0.7), 1));
            y.drawLine(QPointF(tx - tw / 2, ty), QPointF(tx - tw / 2 - 10, ty + 2));
            y.drawLine(QPointF(tx + tw / 2, ty), QPointF(tx + tw / 2 + 10, ty + 2));
            y.setPen(Qt::NoPen);
            m_tent = QPointF(tx, ty);
        }
        // o telescópio no tripé, apontado pra Via Láctea
        {
            const double sx = tx + 70, sy = hill(sx) + 2;
            y.setPen(QPen(hc, 1.6));
            y.drawLine(QPointF(sx - 9, sy), QPointF(sx, sy - 22));
            y.drawLine(QPointF(sx, sy - 22), QPointF(sx + 9, sy));
            y.drawLine(QPointF(sx, sy), QPointF(sx, sy - 22));
            y.setPen(Qt::NoPen);
            y.save();
            y.translate(sx, sy - 24);
            y.rotate(qRadiansToDegrees(-0.75));
            y.fillRect(QRectF(-10, -3.5, 30, 7), hc);
            y.fillRect(QRectF(18, -4.5, 4, 9), hc);
            y.fillRect(QRectF(-4, -6, 6, 3), hc);
            y.restore();
        }
    }

    Cache m_sky;
    QPixmap m_fg;
    QPointF m_tent;
    std::vector<Star> m_stars;
    Shoot m_shoot;
    Sat m_sat;
    Plane m_plane;
    double m_t = 0, m_next = 6, m_nextSat = 8, m_nextPlane = 20;
};

} // namespace

std::unique_ptr<Layer> makeNature(const QString& id)
{
    if (id == QLatin1String("light")) return std::make_unique<Lighthouse2>();
    if (id == QLatin1String("moor")) return std::make_unique<Moor2>();
    if (id == QLatin1String("meadow")) return std::make_unique<Meadow2>();
    if (id == QLatin1String("stars")) return std::make_unique<Stargazer>();
    return nullptr;
}

} // namespace AnimBg
