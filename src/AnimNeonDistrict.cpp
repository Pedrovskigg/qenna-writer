// Neon District (rework de 15/10): megaestruturas com volume e letreiros de holograma.
// Mesma receita das cidades novas (três planos com névoa, parede lateral virada pro meio,
// canto escuro, topo pegando a luz do céu), com cara de cyberpunk: torres de vários formatos
// (degraus, diagonal, cilindro, agulha com mirante, coroa, balanço, gêmeas) e fachadas
// (grade, faixas, aletas, painéis, LED), passarelas, LED nas quinas, heliponto. Os letreiros
// são hologramas projetados por um emissor: translúcidos, com varredura, cor desalinhada,
// faixa de luz descendo e, de vez em quando, um rasgo. Portado de neon_district.js.

#include "AnimKit.h"

#include <QFontMetricsF>
#include <QtMath>
#include <QImage>
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

QFont jpFont(double px)
{
    QFont f;
    f.setFamilies({ QStringLiteral("Yu Gothic"), QStringLiteral("Meiryo"), QStringLiteral("MS Gothic"), QStringLiteral("Microsoft YaHei") });
    f.setPixelSize(qMax(1, int(std::lround(px))));
    f.setWeight(QFont::Bold);
    return f;
}
QFont enFont(double px)
{
    QFont f;
    f.setFamilies({ QStringLiteral("Bahnschrift"), QStringLiteral("IBM Plex Sans"), QStringLiteral("Arial") });
    f.setPixelSize(qMax(1, int(std::lround(px))));
    f.setWeight(QFont::Bold);
    return f;
}
// texto centrado num ponto, como contorno (pra brilho e preenchimento)
QPainterPath textAt(const QFont& f, const QString& s, double cx, double cy)
{
    const QFontMetricsF fm(f);
    QPainterPath p;
    p.addText(cx - fm.horizontalAdvance(s) / 2, cy + (fm.ascent() - fm.descent()) / 2, f, s);
    return p;
}

class NeonDistrict : public Layer {
public:
    void init(Ctx& c) override
    {
        m_hue = c.pal.app.hslHueF() >= 0 ? c.pal.app.hslHueF() * 360 : 270;
        c.hasNeonCols = true;
        c.neonA = hsl(m_hue + 160, 1, 0.62);
        c.neonB = hsl(m_hue - 70, 1, 0.65);
        m_layers.clear();
        m_cars.clear();
        const int n = qMax(1, int(std::lround(12 * INT(c))));
        for (int i = 0; i < n; ++i) m_cars.push_back(newCar(true, c));
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        for (Car& car : m_cars) {
            car.x += car.dir * car.v * dt;
            if (car.x < -60 || car.x > c.W + 60) car = newCar(false, c);
        }
        for (Holo& h : m_holos) {
            if (h.tear > 0) h.tear -= dt;
            else if (rnd() < dt * 0.12) h.tear = R(0.08, 0.25);
        }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        Q_UNUSED(pal);
        if (m_layers.size() != 3 || qAbs(m_px - c.px) > 0.01) build(c);
        const Cols C = cols();
        // céu: violeta fundo com o brilho magenta da cidade embaixo
        QLinearGradient sg(0, 0, 0, c.H);
        sg.setColorAt(0, hsl(m_hue, 0.4, 0.06));
        sg.setColorAt(0.6, hsl(m_hue + 10, 0.4, 0.14));
        sg.setColorAt(1, hsl(m_hue + 40, 0.55, 0.26));
        p.fillRect(QRectF(0, 0, c.W, c.H), sg);
        for (int k = 0; k < 3; ++k) {
            blit(p, m_layers[size_t(k)], 0, 0, c.W, c.H);
            // LED que zumbe: apaga por um instante
            for (const Strip& s : m_strips)
                if (s.k == k && (std::sin(m_t * 13 + s.ph * 7) > 0.985 || (std::sin(m_t * 0.7 + s.ph) > 0.97 && rnd() < 0.5)))
                    p.fillRect(s.r.adjusted(-1, -1, 1, 1), s.shade);
            for (const Ant& a : m_ants)
                if (a.k == k) disc(p, a.x, a.y, 2.2, alpha(hex("#ff3a4a"), 0.35 + 0.65 * std::pow(qMax(0.0, std::sin(m_t * 2 + a.ph)), 4)));
            for (const Pad& pd : m_pads)
                if (pd.k == k)
                    for (int i = 0; i < 8; ++i) {
                        const double an = i / 8.0 * 6.28;
                        const bool on = int(std::floor(m_t * 4 + pd.ph)) % 8 == i;
                        p.fillRect(QRectF(pd.x + std::cos(an) * pd.r * 0.9 - 1, pd.y + std::sin(an) * 4 - 1, 2, 2), alpha(on ? QColor(Qt::white) : C.A, on ? 0.95 : 0.4));
                    }
            for (const Screen& s : m_screens) if (s.k == k) drawScreen(p, s);
            for (const Holo& h : m_holos) if (h.k == k) drawHolo(p, h);
            for (const Car& car : m_cars) if (car.k == k) drawCar(p, car, C);
        }
        // neon refletido no asfalto molhado: faixas da cor de cada holograma, tremendo
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const Holo& h : m_holos) {
            if (h.k < 2) continue;
            const double yy = c.H - 46, w = qMax(6.0, h.w * 0.5);
            for (int i = 0; i < 8; ++i) {
                const double off = std::sin(m_t * 3 + i * 1.7 + h.ph) * 2.5;
                p.fillRect(QRectF(h.x + h.w / 2 - w / 2 + off, yy + i * 5.5, w, 2.5), alpha(h.col, 0.12 * (1 - i / 8.0)));
            }
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

private:
    struct Cols { QColor A, B, M, base, haze, rim; };
    enum Shape { Block, Stepped, Slant, Cylinder, Needle, Crown, Cantilever, Twin };
    enum Skin { Grid, Bands, Fins, Panels, Mesh };
    struct Bld {
        double x, w, top, step1, inset, tone, lit;
        Shape shape = Block;
        Skin skin = Grid;
        double pod = 0, shaft = 0, crown = 0, over = 0, twin = 0;
    };
    struct Tier { double x, w, top, h = 0; bool slant = false; double yl = 0, yr = 0; };
    struct Strip { QRectF r; int k; double ph; QColor shade; };
    struct Ant { double x, y, ph; int k; };
    struct Pad { double x, y, r; int k; double ph; };
    struct Holo { double x, y, w, h; int k; QColor col; QImage main, ghost; double pad, emX, emY, ph, tear, spd; };
    struct Screen { double x, y, w, h; int k; QColor col; int kind; double ph; QString txt; };
    struct Car { int dir, k; double s, x, y, v; };

    Cols cols() const
    {
        return { hsl(m_hue + 160, 1, 0.62), hsl(m_hue - 70, 1, 0.65), hsl(m_hue + 60, 1, 0.62),
                 hsl(m_hue, 0.3, 0.16), hsl(m_hue + 20, 0.45, 0.32), hsl(m_hue + 50, 0.8, 0.7) };
    }

    // ---------------------------------------------------------------- cenário
    // silhueta de cada formato (pra recortar a fachada) e os blocos retos (pra parede lateral)
    QPainterPath shapeOf(Bld& b, std::vector<Tier>& tiers, const Ctx& c) const
    {
        const double H = c.H, bx = b.x, w = b.w, top = b.top, VX = c.W / 2;
        QPainterPath q;
        auto rect = [&](double x0, double y0, double ww) { q.addRect(QRectF(x0, y0, ww, H - y0)); tiers.push_back({ x0, ww, y0 }); };
        switch (b.shape) {
        case Stepped: { const double s = R(40, 90); rect(bx, top + 2 * s, w); rect(bx + w * 0.11, top + s, w * 0.78); rect(bx + w * 0.22, top, w * 0.56); break; }
        case Slant: {
            // o lado alto fica pro meio da mesa
            const double cut = w * R(0.4, 0.9);
            const bool up = bx + w / 2 < VX;
            const double yl = up ? top + cut : top, yr = up ? top : top + cut;
            QPolygonF poly;
            poly << QPointF(bx, H) << QPointF(bx, yl) << QPointF(bx + w, yr) << QPointF(bx + w, H);
            q.addPolygon(poly);
            q.closeSubpath();
            Tier t{ bx, w, qMax(yl, yr) };
            t.slant = true; t.yl = yl; t.yr = yr;
            tiers.push_back(t);
            break;
        }
        case Cylinder:
            q.addRect(QRectF(bx, top + w * 0.12, w, H - top));
            q.addEllipse(QPointF(bx + w / 2, top + w * 0.12), w / 2, w * 0.12);
            break;
        case Needle: {
            const double sw = qMax(10.0, w * 0.28), sx = bx + (w - sw) / 2;
            b.pod = top + R(30, 60);
            b.shaft = sw;
            rect(sx, top, sw);
            q.addRect(QRectF(bx + w * 0.1, H - R(90, 160), w * 0.8, 200));
            break;
        }
        case Crown: {
            const double ch = w * R(0.45, 0.8);
            rect(bx, top + ch, w);
            QPolygonF tri;
            tri << QPointF(bx, top + ch) << QPointF(bx + w / 2, top) << QPointF(bx + w, top + ch);
            q.addPolygon(tri);
            q.closeSubpath();
            b.crown = ch;
            break;
        }
        case Cantilever: {
            const double ch = R(50, 110), lw = w * 0.68;
            rect(bx + (w - lw) / 2, top + ch, lw);
            q.addRect(QRectF(bx, top, w, ch));
            Tier t{ bx, w, top };
            t.h = ch;
            tiers.push_back(t);
            b.over = ch;
            break;
        }
        case Twin: { const double tw = w * 0.42; rect(bx, top + R(0, 40), tw); rect(bx + w - tw, top, tw); b.twin = tw; break; }
        default:
            if (b.step1 > 0) { rect(bx, top + b.step1, w); rect(bx + w * b.inset, top, w * (1 - 2 * b.inset)); }
            else rect(bx, top, w);
        }
        q.setFillRule(Qt::WindingFill);
        return q;
    }

    void skin(QPainter& x, const Bld& b, int k, const Cols& C, const Ctx& c) const
    {
        const double H = c.H, bx = b.x, w = b.w, top = b.top, s = 8 + 2 * k, lit = b.lit;
        static const double LA[3] = { 0.35, 0.55, 0.75 };
        const QColor cool = hex("#cfe8ff");
        x.setPen(Qt::NoPen);
        if (b.skin == Bands) {
            // faixas de janela corridas, acesas aos pedaços
            for (double wy = top + 6; wy < H; wy += 10 + 2 * k) {
                x.fillRect(QRectF(bx, wy, w, 3 + k), alpha(Qt::black, 0.32));
                for (double wx = bx; wx < bx + w; wx += 12)
                    if (rnd() < lit * 1.6) x.fillRect(QRectF(wx, wy, 12, 3 + k), alpha(rnd() < 0.75 ? cool : C.B, LA[k] - 0.05));
            }
        } else if (b.skin == Fins) {
            // aletas verticais, algumas com fio de luz
            for (double fx = bx + 3; fx < bx + w - 2; fx += 5 + k) {
                x.fillRect(QRectF(fx, top, 2, H - top), alpha(Qt::black, 0.3));
                x.fillRect(QRectF(fx + 2, top, 1, H - top), alpha(Qt::white, 0.05));
            }
            if (k > 0)
                for (int i = 0; i < 2; ++i) {
                    const double fx = bx + 3 + std::floor(R(0, (w - 6) / (5 + k))) * (5 + k), y0 = top + R(0, 60), hh = (H - top) * R(0.3, 0.8);
                    x.fillRect(QRectF(fx - 1.5, y0, 4.2, hh), alpha(C.B, 0.12));
                    x.fillRect(QRectF(fx, y0, 1.2, hh), alpha(C.B, 0.6));
                }
        } else if (b.skin == Panels) {
            // painéis industriais, respiros e poucas janelas
            const double ph = 26 + 6 * k, pw = qMax(16.0, w / qMax(1.0, std::round(w / (22 + 6 * k))));
            x.setBrush(Qt::NoBrush);
            x.setPen(QPen(alpha(Qt::black, 0.35), 1));
            for (double py = top + 4; py < H; py += ph)
                for (double px = bx; px < bx + w - 2; px += pw) x.drawRect(QRectF(px + 0.5, py + 0.5, pw - 1, ph - 1));
            x.setPen(Qt::NoPen);
            for (double py = top + 4; py < H; py += ph)
                for (double px = bx; px < bx + w - 2; px += pw) {
                    const double r = rnd();
                    if (r < 0.18) { for (int i = 0; i < 4; ++i) x.fillRect(QRectF(px + 4, py + 6 + i * 3, pw - 8, 1.2), alpha(Qt::black, 0.4)); }
                    else if (r < 0.18 + lit) x.fillRect(QRectF(px + pw / 2 - 2, py + ph / 2 - 2, 4, 3), alpha(rnd() < 0.6 ? cool : C.A, LA[k]));
                }
        } else if (b.skin == Mesh) {
            // fachada de LED: um telão de pontinhos num degradê de duas cores
            const bool bm = rnd() < 0.5;
            const QColor c1 = bm ? C.B : C.M, c2 = bm ? C.M : C.A;
            const double d = 3 + k * 0.5;
            static const double MA[3] = { 0.18, 0.3, 0.42 };
            for (double py = top + 4; py < H; py += d) {
                const double t = (py - top) / (H - top);
                const QColor mc = mix(c1, c2, t);
                for (double px = bx + 2; px < bx + w - 2; px += d)
                    if (rnd() < 0.85) x.fillRect(QRectF(px, py, d * 0.55, d * 0.55), alpha(mc, MA[k] * (0.6 + 0.4 * rnd())));
            }
        } else {
            // grade de vidro: montantes, lajes e janelas frias
            QVector<QRectF> mull, slab;
            for (double mx = bx + 6; mx < bx + w - 3; mx += s) mull.append(QRectF(mx, top, 1, H - top));
            for (double wy = top + 9; wy < H; wy += 9 + 2 * k) slab.append(QRectF(bx, wy, w, 1));
            x.setBrush(alpha(Qt::black, 0.18));
            x.drawRects(mull);
            x.setBrush(alpha(Qt::black, 0.14 + 0.05 * k));
            x.drawRects(slab);
            for (double wy = top + 5; wy < H - 4; wy += 9 + 2 * k)
                for (double wx = bx + 3; wx < bx + w - 5; wx += s) {
                    const double r = rnd();
                    if (r < lit) x.fillRect(QRectF(wx, wy, 3 + k, 3 + k * 0.5), alpha(rnd() < 0.7 ? cool : C.B, LA[k]));
                    else if (r < lit + 0.02) x.fillRect(QRectF(wx, wy, 3 + k, 3 + k * 0.5), alpha(C.M, 0.5));
                }
        }
        x.setBrush(Qt::NoBrush);
    }

    void tower(QPainter& x, Bld& b, int k, const Cols& C, const Ctx& c)
    {
        static const Shape SH[9] = { Block, Block, Stepped, Slant, Cylinder, Needle, Crown, Cantilever, Twin };
        static const Skin SK[6] = { Grid, Grid, Bands, Fins, Panels, Mesh };
        b.shape = SH[RI(9)];
        b.skin = b.shape == Needle ? Grid : SK[RI(6)];
        const double H = c.H, VX = c.W / 2, cx = b.x + b.w / 2;
        const int side = cx < VX ? 1 : -1;
        static const double DK[3] = { 0.0, 0.08, 0.2 };
        const QColor base = mix(C.base, b.tone > 0 ? hex("#5a6a8a") : QColor(Qt::black), std::abs(b.tone) + DK[k]);
        std::vector<Tier> tiers;
        const QPainterPath q = shapeOf(b, tiers, c);
        x.setPen(Qt::NoPen);
        // paredes laterais dos blocos retos, viradas pro meio da mesa
        for (const Tier& t : tiers) {
            const double sw = qMin(6.0 + 10 * k, std::abs(t.x + t.w / 2 - VX) * (0.012 + 0.01 * k));
            if (sw <= 1.5) continue;
            const double ex = side > 0 ? t.x + t.w : t.x, ox = ex + side * sw, drop = sw * 0.4;
            const double ty = t.slant ? (side > 0 ? t.yr : t.yl) : t.top, bot = t.h > 0 ? t.top + t.h : H;
            QLinearGradient sg(0, ty, 0, bot);
            sg.setColorAt(0, mix(base, Qt::black, 0.4));
            sg.setColorAt(1, mix(base, Qt::black, 0.75));
            QPolygonF sp;
            sp << QPointF(ex, ty) << QPointF(ox, ty + drop) << QPointF(ox, bot) << QPointF(ex, bot);
            x.setBrush(sg);
            x.drawPolygon(sp);
            x.fillRect(QRectF(side > 0 ? ex - 1 : ex, ty, 1, bot - ty), alpha(C.rim, 0.08 + 0.04 * k));
        }
        // fachada recortada na silhueta; o cilindro ganha sombreado redondo
        x.save();
        x.setClipPath(q, Qt::IntersectClip);
        if (b.shape == Cylinder) {
            QLinearGradient g(b.x, 0, b.x + b.w, 0);
            g.setColorAt(0, mix(base, Qt::black, 0.5));
            g.setColorAt(0.35 - side * 0.1, mix(base, C.rim, 0.18));
            g.setColorAt(1, mix(base, Qt::black, 0.65));
            x.fillRect(QRectF(b.x - 2, b.top - 4, b.w + 4, H - b.top + 4), g);
        } else {
            QLinearGradient g(0, b.top, 0, H);
            g.setColorAt(0, mix(base, C.rim, 0.12));
            g.setColorAt(0.2, base);
            g.setColorAt(1, mix(base, Qt::black, 0.55));
            x.fillRect(QRectF(b.x - 2, b.top - 4, b.w + 4, H - b.top + 4), g);
        }
        skin(x, b, k, C, c);
        if (b.shape != Cylinder) {
            vfill(x, QRectF(b.x - 2, b.top, b.w + 4, H - b.top), alpha(Qt::black, 0), alpha(Qt::black, 0.45), b.top, H);
        } else {
            QLinearGradient g(b.x, 0, b.x + b.w, 0);
            g.setColorAt(0, alpha(Qt::black, 0.35));
            g.setColorAt(0.4, alpha(Qt::black, 0));
            g.setColorAt(1, alpha(Qt::black, 0.4));
            x.fillRect(QRectF(b.x, b.top, b.w, H - b.top), g);
        }
        // canto escuro nos blocos
        for (const Tier& t : tiers)
            for (int e = 0; e < 2; ++e) {
                const double ex = e ? t.x + t.w : t.x, dir = e ? -1 : 1, aw = 8 + 5 * k;
                QLinearGradient g(ex, 0, ex + dir * aw, 0);
                g.setColorAt(0, alpha(Qt::black, 0.3 + 0.1 * k));
                g.setColorAt(1, alpha(Qt::black, 0));
                x.fillRect(QRectF(qMin(ex, ex + dir * aw), t.top, aw, H - t.top), g);
            }
        x.restore();
        x.setPen(Qt::NoPen);
        // detalhes de cada formato
        if (b.over > 0) {
            x.fillRect(QRectF(b.x + b.w * 0.16, b.top + b.over, b.w * 0.68, 10), alpha(Qt::black, 0.5));
            for (double lx = b.x + 6; lx < b.x + b.w - 4; lx += 10) x.fillRect(QRectF(lx, b.top + b.over - 2, 4, 1.5), alpha(C.A, 0.6));
        }
        if (b.crown > 0) {
            x.setPen(QPen(alpha(C.rim, 0.35), 1));
            for (int i = 1; i < 4; ++i) x.drawLine(QPointF(b.x + b.w / 2, b.top), QPointF(b.x + b.w * i / 4, b.top + b.crown));
            x.setPen(Qt::NoPen);
        }
        if (b.twin > 0)
            for (double yy : { b.top + 40, b.top + R(120, 220) }) {
                x.fillRect(QRectF(b.x + b.twin, yy, b.w - 2 * b.twin, 8), mix(base, Qt::black, 0.2));
                for (double wx = b.x + b.twin + 2; wx < b.x + b.w - b.twin - 3; wx += 5) x.fillRect(QRectF(wx, yy + 3, 3, 2), alpha(hex("#cfe8ff"), 0.5));
            }
        const Tier tt = tiers.empty() ? Tier{ b.x, b.w, b.top } : tiers.back();
        if (b.shape != Cylinder && b.shape != Slant) x.fillRect(QRectF(tt.x, tt.top, tt.w, 1.5), mix(base, C.rim, 0.3));
        // LED nas quinas e numa laje (zumbem no desenho); a agulha não ganha, pra não cortar o mirante
        if (k > 0 && b.skin != Mesh && b.pod <= 0) {
            const QColor lc = rnd() < 0.5 ? C.A : C.B;
            const Tier t0 = tiers.empty() ? Tier{ b.x, b.w, b.top } : tiers.front();
            auto put = [&](const QRectF& r) {
                x.fillRect(r.adjusted(-3, -3, 3, 3), alpha(lc, 0.12));
                x.fillRect(r.adjusted(-1.5, -1.5, 1.5, 1.5), alpha(lc, 0.3));
                x.fillRect(r, lc);
                m_strips.push_back({ r, k, R(0, 6.28), base });
            };
            if (rnd() < 0.5) put(QRectF(rnd() < 0.5 ? t0.x : t0.x + t0.w - 2.2, t0.top + R(10, 40), 2.2, (H - t0.top) * R(0.3, 0.7)));
            if (rnd() < 0.4) put(QRectF(t0.x, t0.top + R(30, (H - t0.top) * 0.5), t0.w, 2));
        }
        if (b.pod > 0) mirante(x, b, k, side, base, C);
        // pináculo com luz, ou heliponto com o anel piscando
        const double peak = (b.crown > 0 || b.shape == Cylinder) ? b.top : tt.top;
        if (b.shape == Needle || b.crown > 0 || rnd() < 0.35) {
            const double sh = R(24, 80);
            x.setPen(QPen(mix(base, C.rim, 0.25), 1.4 + k * 0.4));
            x.drawLine(QPointF(b.x + b.w / 2, peak), QPointF(b.x + b.w / 2, peak - sh));
            x.setPen(Qt::NoPen);
            m_ants.push_back({ b.x + b.w / 2, peak - sh, R(0, 6.28), k });
        } else if (k > 0 && tt.w > 50 && b.shape != Slant && b.shape != Twin && rnd() < 0.5) {
            const double pw = qMin(tt.w * 0.8, 70.0), py = tt.top - 5, pcx = tt.x + tt.w / 2;
            x.fillRect(QRectF(pcx - 4, py, 8, 5), mix(base, Qt::black, 0.3));
            x.setBrush(mix(base, Qt::black, 0.3));
            x.drawEllipse(QPointF(pcx, py), pw / 2, 5);
            x.setBrush(Qt::NoBrush);
            x.setPen(QPen(alpha(C.A, 0.5), 1));
            x.drawEllipse(QPointF(pcx, py), pw * 0.3, 3);
            x.setPen(Qt::NoPen);
            m_pads.push_back({ pcx, py, pw / 2, k, R(0, 6.28) });
        }
    }

    // o mirante da torre agulha: saia cônica, barriga, anel com sombreado redondo, janelas na curva e cúpula
    void mirante(QPainter& x, const Bld& b, int k, int side, const QColor& base, const Cols& C) const
    {
        const double cx = b.x + b.w / 2, py = b.pod, hs = b.shaft / 2, pw = qMin(qMax(b.shaft * 1.7, 18.0), b.w * 0.55), bh = 9 + 2 * k;
        auto ring = [&](double light, double d0, double d1) {
            QLinearGradient g(cx - pw, 0, cx + pw, 0);
            g.setColorAt(0, mix(base, Qt::black, d0));
            g.setColorAt(qBound(0.05, 0.4 - side * 0.1, 0.95), mix(base, C.rim, light));
            g.setColorAt(1, mix(base, Qt::black, d1));
            return g;
        };
        QPolygonF skirt;
        skirt << QPointF(cx - hs, py + bh + 26) << QPointF(cx - pw * 0.8, py + bh) << QPointF(cx + pw * 0.8, py + bh) << QPointF(cx + hs, py + bh + 26);
        x.setPen(Qt::NoPen);
        x.setBrush(ring(0.1, 0.55, 0.7));
        x.drawPolygon(skirt);
        x.setPen(QPen(alpha(Qt::black, 0.35), 0.8));
        for (int i = -2; i <= 2; ++i) x.drawLine(QPointF(cx + i * hs * 0.45, py + bh + 26), QPointF(cx + i * pw * 0.35, py + bh));
        x.setPen(Qt::NoPen);
        x.setBrush(mix(base, Qt::black, 0.6));
        x.drawEllipse(QPointF(cx, py + bh), pw, 4.5);
        x.fillRect(QRectF(cx - pw, py, pw * 2, bh), ring(0.28, 0.45, 0.6));
        static const double KA[3] = { 0.6, 0.8, 1 };
        for (int i = -9; i <= 9; ++i) {
            const double an = i / 9.0 * 1.35, wx = cx + std::sin(an) * pw * 0.94, ww = std::cos(an) * 4.2 + 0.6;
            x.fillRect(QRectF(wx - ww / 2, py + bh * 0.3, ww, bh * 0.4), alpha(i % 3 == 0 ? C.B : hex("#cfe8ff"), (0.35 + 0.5 * std::cos(an)) * KA[k]));
        }
        x.fillRect(QRectF(cx - pw, py, pw * 2, 1), alpha(C.rim, 0.35));
        QPainterPath dome;
        dome.moveTo(cx - pw * 0.62, py);
        dome.arcTo(QRectF(cx - pw * 0.62, py - (7 + k), pw * 1.24, (7 + k) * 2), 180, -180);
        dome.closeSubpath();
        x.fillPath(dome, ring(0.2, 0.35, 0.55));
        x.setBrush(Qt::NoBrush);
    }

    // passarela suspensa entre duas torres vizinhas: tubo com a faixa de janelas acesa
    void bridge(QPainter& x, const Bld& a, const Bld& b, int k, const Cols& C, const Ctx& c) const
    {
        const double x0 = a.x + a.w, x1 = b.x, top = qMax(a.top + a.step1, b.top + b.step1) + R(30, 160);
        if (x1 - x0 < 6 || x1 - x0 > 60 || top > c.H - 120) return;
        static const double DK[3] = { 0.05, 0.15, 0.25 };
        const double h = 8 + 3 * k;
        const QColor base = mix(C.base, Qt::black, DK[k]);
        vfill(x, QRectF(x0 - 2, top, x1 - x0 + 4, h), mix(base, C.rim, 0.25), mix(base, Qt::black, 0.4), top, top + h);
        for (double wx = x0 + 2; wx < x1 - 3; wx += 5)
            if (rnd() < 0.7) x.fillRect(QRectF(wx, top + h * 0.35, 3, h * 0.3), alpha(hex("#cfe8ff"), 0.55));
        x.fillRect(QRectF(x0, top + h, x1 - x0, 2), alpha(Qt::black, 0.4));
    }

    void build(const Ctx& c)
    {
        const Cols C = cols();
        m_layers.clear();
        m_holos.clear();
        m_screens.clear();
        m_strips.clear();
        m_ants.clear();
        m_pads.clear();
        m_px = c.px;
        int screenKind = 0;
        std::vector<QRectF> busy[3];
        static const double LW[3][2] = { { 34, 70 }, { 44, 86 }, { 56, 110 } }, LH[3][2] = { { 400, 660 }, { 320, 600 }, { 200, 520 } }, LIT[3] = { 0.12, 0.16, 0.2 };
        const double VX = c.W / 2;
        for (int k = 0; k < 3; ++k) {
            QPixmap pm = layer(c, c.W, c.H);
            QPainter x(&pm);
            x.setRenderHint(QPainter::Antialiasing);
            std::vector<Bld> row;
            for (double bx = R(-20, 0); bx < c.W + 20;) {
                const double w = R(LW[k][0], LW[k][1]), h = R(LH[k][0], LH[k][1]);
                Bld b{ bx, w, c.H - h, rnd() < 0.5 ? R(30, h * 0.35) : 0, R(0.12, 0.22), R(-0.1, 0.12), LIT[k] * R(0.4, 1.5) };
                row.push_back(b);
                bx += w + (k == 2 ? R(4, 22) : R(0, 8));
            }
            std::vector<size_t> order(row.size());
            for (size_t i = 0; i < row.size(); ++i) order[i] = i;
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return std::abs(row[a].x + row[a].w / 2 - VX) > std::abs(row[b].x + row[b].w / 2 - VX); });
            for (size_t i : order) tower(x, row[i], k, C, c);
            if (k > 0)
                for (size_t i = 0; i + 1 < row.size(); ++i)
                    if (rnd() < 0.35) bridge(x, row[i], row[i + 1], k, C, c);
            // névoa violeta entre os planos, só sobre os prédios (o plano de trás some, o do meio esmaece)
            if (k < 2) {
                static const double A0[2] = { 0.25, 0.08 }, A1[2] = { 0.55, 0.3 };
                x.setCompositionMode(QPainter::CompositionMode_SourceAtop);
                vfill(x, QRectF(0, 0, c.W, c.H), alpha(C.haze, A0[k]), alpha(C.haze, A1[k]), 0, c.H);
                x.setCompositionMode(QPainter::CompositionMode_SourceOver);
            }
            // a rua lá embaixo: asfalto molhado
            if (k == 2) {
                vfill(x, QRectF(0, c.H - 48, c.W, 48), mix(C.base, Qt::black, 0.55), mix(C.base, Qt::black, 0.8), c.H - 48, c.H);
                x.fillRect(QRectF(0, c.H - 48, c.W, 1), alpha(C.rim, 0.15));
            }
            x.end();
            m_layers.push_back(pm);
            // hologramas e telões presos nas torres deste plano, sem encavalar
            if (k > 0)
                for (const Bld& b : row) {
                    if (b.w < 40) continue;
                    auto free = [&](const QRectF& r) {
                        for (const QRectF& q2 : busy[k])
                            if (!(r.left() > q2.right() + 8 || r.right() < q2.left() - 8 || r.top() > q2.bottom() + 14 || r.bottom() < q2.top() - 14)) return false;
                        busy[k].push_back(r);
                        return true;
                    };
                    if (rnd() < (k == 2 ? 0.7 : 0.45)) { Holo h = makeHolo(b, k, C, false, c); if (free(QRectF(h.x, h.y, h.w, h.h))) m_holos.push_back(std::move(h)); }
                    if (k == 2 && b.w > 70 && rnd() < 0.3) { Holo h = makeHolo(b, k, C, true, c); if (free(QRectF(h.x, h.y, h.w, h.h))) m_holos.push_back(std::move(h)); }
                    if (b.w > 64 && rnd() < (k == 2 ? 0.22 : 0.18)) {
                        Screen s = makeScreen(b, k, C, screenKind, c);
                        if (free(QRectF(s.x, s.y, s.w, s.h))) { m_screens.push_back(s); ++screenKind; }
                    }
                }
        }
    }

    // ---------------------------------------------------------------- hologramas
    // a imagem do letreiro pronta: preenchimento translúcido, borda fina, cantinhos de interface,
    // texto com brilho e linhas de varredura
    QImage holoSprite(const QString& txt, bool jp, double fs, bool vert, const QColor& col, double w, double h, double pad, double px) const
    {
        QImage img(qMax(1, int(std::ceil((w + pad * 2) * px))), qMax(1, int(std::ceil((h + pad * 2) * px))), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter x(&img);
        x.setRenderHint(QPainter::Antialiasing);
        x.scale(px, px);
        x.translate(pad, pad);
        vfill(x, QRectF(0, 0, w, h), alpha(col, 0.26), alpha(col, 0.08), 0, h);
        x.setPen(QPen(alpha(col, 0.75), 1));
        x.setBrush(Qt::NoBrush);
        x.drawRect(QRectF(0.5, 0.5, w - 1, h - 1));
        x.setPen(Qt::NoPen);
        x.fillRect(QRectF(0, 0, 6, 1.5), alpha(col, 0.9));
        x.fillRect(QRectF(w - 6, h - 1.5, 6, 1.5), alpha(col, 0.9));
        const QFont f = jp ? jpFont(fs) : enFont(fs);
        QPainterPath t;
        if (vert) {
            for (int i = 0; i < txt.size(); ++i) t.addPath(textAt(f, txt.mid(i, 1), w / 2, 6 + (i + 0.5) * fs * 1.08));
        } else {
            t = textAt(f, txt, w / 2, h / 2);
        }
        // brilho: o contorno largo e fraco, depois o texto cheio e o miolo claro
        x.setBrush(Qt::NoBrush);
        x.setPen(pen(alpha(col, 0.12), 7));
        x.drawPath(t);
        x.setPen(pen(alpha(col, 0.3), 3.5));
        x.drawPath(t);
        x.setPen(Qt::NoPen);
        x.fillPath(t, alpha(col, 0.9));
        x.fillPath(t.translated(0, 0), alpha(mix(col, Qt::white, 0.3), 0.6));
        // linhas de varredura: escurece uma linha sim, outra não
        x.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        for (double yy = -pad; yy < h + pad; yy += 2.5) x.fillRect(QRectF(-pad, yy, w + pad * 2, 1), QColor(0, 0, 0, 90));
        x.end();
        return img;
    }

    Holo makeHolo(const Bld& b, int k, const Cols& C, bool wide, const Ctx& c) const
    {
        static const char* JP[8] = { "ラーメン", "ホテル", "バー", "カラオケ", "薬局", "寿司", "電気", "ネオン" };
        static const char* EN[8] = { "HOTEL", "RAMEN", "BAR", "24H", "KARAOKE", "NOODLES", "OPEN", "CYBER" };
        const QColor pal4[4] = { C.A, C.B, C.M, C.B };
        const QColor col = pal4[RI(4)];
        const bool jp = rnd() < 0.55;
        const QString txt = QString::fromUtf8(jp ? JP[RI(8)] : EN[RI(8)]);
        const double sc = k == 2 ? 1 : 0.7;
        const bool vert = jp && !wide && rnd() < 0.7;
        double fs, w, h;
        if (vert) {
            fs = R(17, 22) * sc;
            w = fs * 1.5;
            h = txt.size() * fs * 1.08 + 12;
        } else {
            fs = (wide ? R(22, 28) : R(15, 19)) * sc;
            w = QFontMetricsF(jp ? jpFont(fs) : enFont(fs)).horizontalAdvance(txt) + 16 * sc;
            h = fs * 1.7;
        }
        // flutua na frente da fachada, perto da quina, projetado por um emissor preso no prédio
        const bool left = rnd() < 0.5;
        const double x = vert ? (left ? b.x - w * 0.35 : b.x + b.w - w * 0.65) : b.x + R(-10, qMax(-9.0, b.w - w + 10));
        const double y0 = qMax(b.top + b.step1 + 20, c.H * (k == 2 ? 0.42 : 0.3)), y = R(y0, qMax(y0 + 1, c.H - 60 - h));
        const double pad = 16;
        Holo hl;
        hl.x = x; hl.y = y; hl.w = w; hl.h = h; hl.k = k; hl.col = col; hl.pad = pad;
        hl.main = holoSprite(txt, jp, fs, vert, col, w, h, pad, c.px);
        hl.ghost = holoSprite(txt, jp, fs, vert, col == C.M ? C.B : C.M, w, h, pad, c.px);
        hl.emX = x + w / 2;
        hl.emY = vert ? y + h + 10 : y + h + 8;
        hl.ph = R(0, 6.28);
        hl.tear = 0;
        hl.spd = R(0.25, 0.5);
        return hl;
    }

    Screen makeScreen(const Bld& b, int k, const Cols& C, int kind, const Ctx& c) const
    {
        static const char* JP[8] = { "ラーメン", "ホテル", "バー", "カラオケ", "薬局", "寿司", "電気", "ネオン" };
        static const char* EN[8] = { "HOTEL", "RAMEN", "BAR", "24H", "KARAOKE", "NOODLES", "OPEN", "CYBER" };
        const QColor pal3[3] = { C.B, C.M, C.A };
        const double w = qMin(b.w - 10, R(56, 90)) * (k == 2 ? 1 : 0.9), h = w * R(0.6, 0.85), x = b.x + (b.w - w) / 2;
        const double y0 = qMax(b.top + b.step1 + 30, c.H * 0.25), y = R(y0, qMax(y0 + 1, c.H * 0.6));
        const QString txt = QString::fromUtf8(JP[RI(8)]) + QLatin1Char(' ') + QString::fromUtf8(EN[RI(8)]) + QLatin1Char(' ') + QString::fromUtf8(JP[RI(8)]);
        return { x, y, w, h, k, pal3[RI(3)], kind % 4, R(0, 6.28), txt };
    }

    void drawHolo(QPainter& p, const Holo& s) const
    {
        const double flick = 0.75 + 0.15 * std::sin(m_t * 7 + s.ph) + 0.1 * std::sin(m_t * 23 + s.ph * 3), drop = rnd() < 0.012 ? 0.25 : 1;
        const double a = flick * drop * (s.k == 2 ? 1 : 0.7), pad = s.pad;
        p.setPen(Qt::NoPen);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        // cone de luz do emissor até a imagem
        QLinearGradient cg(0, s.emY, 0, s.y + s.h * 0.5);
        cg.setColorAt(0, alpha(s.col, 0.22 * a));
        cg.setColorAt(1, alpha(s.col, 0));
        QPolygonF cone;
        cone << QPointF(s.emX - 2, s.emY) << QPointF(s.emX + 2, s.emY) << QPointF(s.x + s.w, s.y + s.h * 0.5) << QPointF(s.x, s.y + s.h * 0.5);
        p.setBrush(cg);
        p.drawPolygon(cone);
        // cor desalinhada (fantasma) e a imagem principal; no rasgo, as faixas escorregam
        const QRectF full(s.x - pad, s.y - pad, s.w + pad * 2, s.h + pad * 2);
        p.setOpacity(0.35 * a);
        p.drawImage(full.translated(-1.5 - (s.tear > 0 ? 3 : 0), 0), s.ghost);
        p.setOpacity(a);
        if (s.tear > 0) {
            const int n = 4;
            const double sh = full.height() / n, ih = double(s.main.height()) / n;
            for (int i = 0; i < n; ++i)
                p.drawImage(QRectF(full.left() + (rnd() - 0.5) * 10, full.top() + i * sh, full.width(), sh), s.main, QRectF(0, i * ih, s.main.width(), ih));
        } else {
            p.drawImage(full, s.main);
        }
        // faixa de luz descendo pela imagem
        const double by = s.y + std::fmod(m_t * s.spd + s.ph, 1.3) * s.h;
        p.setOpacity(0.5 * a);
        QLinearGradient bg(0, by - 6, 0, by + 6);
        bg.setColorAt(0, alpha(s.col, 0));
        bg.setColorAt(0.5, alpha(Qt::white, 0.5));
        bg.setColorAt(1, alpha(s.col, 0));
        p.fillRect(QRectF(s.x, by - 6, s.w, 12).intersected(QRectF(s.x, s.y, s.w, s.h)), bg);
        p.setOpacity(1);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // o emissor: caixinha no prédio com a lente acesa
        p.fillRect(QRectF(s.emX - 4, s.emY, 8, 4), hex("#0a0812"));
        p.fillRect(QRectF(s.emX - 1.5, s.emY - 0.5, 3, 1.5), alpha(s.col, 0.9 * a));
        p.setBrush(Qt::NoBrush);
    }

    void drawScreen(QPainter& p, const Screen& s) const
    {
        const double a = (0.8 + 0.2 * std::sin(m_t * 5 + s.ph)) * (s.k == 2 ? 1 : 0.7);
        const QRectF r(s.x, s.y, s.w, s.h);
        const double cx = s.x + s.w / 2, cy = s.y + s.h / 2, t = m_t + s.ph;
        const QColor ink = alpha(mix(s.col, Qt::white, 0.4), 0.85 * a);
        p.save();
        p.setClipRect(r, Qt::IntersectClip);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        vfill(p, r, alpha(s.col, 0.16 * a), alpha(s.col, 0.05 * a), s.y, s.y + s.h);
        p.setBrush(Qt::NoBrush);
        QPen ip(ink, 1.3);
        ip.setJoinStyle(Qt::RoundJoin);
        p.setPen(ip);
        if (s.kind == 0) {
            // a carpa nadando em círculo, rabo balançando
            const double ang = t * 0.6, rr = qMin(s.w, s.h) * 0.25, L = qMin(s.w, s.h) * 0.32, tw = std::sin(t * 6) * 0.35;
            p.save();
            p.translate(cx + std::cos(ang) * rr, cy + std::sin(ang) * rr * 0.6);
            p.rotate(qRadiansToDegrees(ang + M_PI / 2));
            QPainterPath body(QPointF(L * 0.5, 0));
            body.quadTo(0, -L * 0.22, -L * 0.4, 0);
            body.quadTo(0, L * 0.22, L * 0.5, 0);
            p.drawPath(body);
            p.translate(-L * 0.4, 0);
            p.rotate(qRadiansToDegrees(tw));
            QPolygonF tail;
            tail << QPointF(0, 0) << QPointF(-L * 0.22, -L * 0.16) << QPointF(-L * 0.22, L * 0.16) << QPointF(0, 0);
            p.drawPolyline(tail);
            p.restore();
        } else if (s.kind == 2) {
            // cubo de arame girando
            const double Rr = qMin(s.w, s.h) * 0.24, ay = t * 0.9, ax = 0.5;
            static const int V[8][3] = { { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 }, { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 } };
            static const int E[12][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
            QPointF P[8];
            for (int i = 0; i < 8; ++i) {
                const double X = V[i][0] * std::cos(ay) - V[i][2] * std::sin(ay), Z = V[i][0] * std::sin(ay) + V[i][2] * std::cos(ay), Y = V[i][1] * std::cos(ax) - Z * std::sin(ax);
                P[i] = QPointF(cx + X * Rr, cy + Y * Rr);
            }
            for (const auto& e : E) p.drawLine(P[e[0]], P[e[1]]);
        } else if (s.kind == 3) {
            // onda de áudio e as barrinhas
            QPainterPath wv;
            for (int i = 0; i <= 40; ++i) {
                const double xx = s.x + 4 + i * (s.w - 8) / 40, yy = cy + std::sin(i * 0.5 + t * 3) * s.h * 0.18 * std::sin(i * 0.13 + t);
                if (i) wv.lineTo(xx, yy); else wv.moveTo(xx, yy);
            }
            p.drawPath(wv);
            p.setPen(Qt::NoPen);
            for (int i = 0; i < 8; ++i) {
                const double bh = s.h * 0.12 * (0.5 + 0.5 * std::sin(t * 4 + i));
                p.fillRect(QRectF(s.x + 6 + i * (s.w - 12) / 8, s.y + s.h - 6 - bh, (s.w - 12) / 8 - 2, bh), ink);
            }
        } else {
            // texto rolando
            const QFont f = jpFont(s.h * 0.26);
            const QFontMetricsF fm(f);
            const double tw2 = fm.horizontalAdvance(s.txt) + 20, off = std::fmod(t * 30, tw2);
            p.setFont(f);
            for (double xx = s.x - off; xx < s.x + s.w; xx += tw2) p.drawText(QPointF(xx, cy + (fm.ascent() - fm.descent()) / 2), s.txt);
        }
        // varredura
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setPen(Qt::NoPen);
        for (double yy = s.y; yy < s.y + s.h; yy += 2.5) p.fillRect(QRectF(s.x, yy, s.w, 1), alpha(Qt::black, 0.22));
        p.restore();
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(alpha(s.col, 0.7 * a), 1));
        p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
        p.setPen(Qt::NoPen);
        p.fillRect(QRectF(s.x, s.y, 8, 1.5), alpha(s.col, a));
        p.fillRect(QRectF(s.x + s.w - 8, s.y + s.h - 1.5, 8, 1.5), alpha(s.col, a));
    }

    // ---------------------------------------------------------------- carros voadores, em três distâncias
    Car newCar(bool any, const Ctx& c) const
    {
        static const double S[3] = { 0.45, 0.7, 1 }, Y0[3] = { 90, 130, 170 }, Y1[3] = { 0.5, 0.6, 0.7 };
        const int dir = rnd() < 0.5 ? 1 : -1, k = RI(3);
        return { dir, k, S[k], any ? R(0, c.W) : (dir > 0 ? -40.0 : c.W + 40), R(Y0[k], c.H * Y1[k]), R(60, 150) * S[k] };
    }
    void drawCar(QPainter& p, const Car& car, const Cols& C) const
    {
        static const double FADE[3] = { 0.55, 0.8, 1 };
        const double s = car.s, d = car.dir, x = car.x, y = car.y;
        const QColor body = mix(C.base, hex("#8a90a8"), 0.25);
        p.setOpacity(FADE[car.k]);
        p.setPen(Qt::NoPen);
        // rastro e brilho do propulsor embaixo
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        QLinearGradient tg(x, 0, x - d * 70 * s, 0);
        tg.setColorAt(0, alpha(C.B, 0.35));
        tg.setColorAt(1, alpha(C.B, 0));
        p.fillRect(QRectF(qMin(x, x - d * 70 * s), y + 2 * s, 70 * s, 2 * s), tg);
        glow(p, x, y + 6 * s, 14 * s, C.B, 0.5);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // corpo em cápsula, com o teto pegando a luz do céu e a cabine acesa
        QLinearGradient bg(0, y - 5 * s, 0, y + 5 * s);
        bg.setColorAt(0, mix(body, C.rim, 0.35));
        bg.setColorAt(1, mix(body, Qt::black, 0.5));
        QPainterPath cap;
        cap.addRoundedRect(QRectF(x - 15 * s, y - 4 * s, 30 * s, 8 * s), 4 * s, 4 * s);
        p.fillPath(cap, bg);
        QPainterPath cab;
        cab.addRoundedRect(QRectF(x + d * 3 * s - (d > 0 ? 0 : 9 * s), y - 3 * s, 9 * s, 3 * s), 1.5 * s, 1.5 * s);
        p.fillPath(cab, alpha(C.B, 0.7));
        // farol na frente com o facho, lanterna atrás
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.fillRect(QRectF(x + d * 14 * s - 1, y - 1, 2.4 * s, 2 * s), alpha(Qt::white, 0.95));
        QLinearGradient hg(x + d * 15 * s, 0, x + d * 70 * s, 0);
        hg.setColorAt(0, alpha(C.A, 0.35));
        hg.setColorAt(1, alpha(C.A, 0));
        QPolygonF beam;
        beam << QPointF(x + d * 15 * s, y - 1) << QPointF(x + d * 70 * s, y - 8 * s) << QPointF(x + d * 70 * s, y + 8 * s) << QPointF(x + d * 15 * s, y + 1);
        p.setBrush(hg);
        p.drawPolygon(beam);
        p.setBrush(Qt::NoBrush);
        p.fillRect(QRectF(x - d * 15 * s - 1, y - 1, 2 * s, 2 * s), alpha(hex("#ff3a4a"), 0.9));
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setOpacity(1);
    }

    double m_hue = 270, m_t = 0, m_px = 0;
    std::vector<QPixmap> m_layers;
    std::vector<Holo> m_holos;
    std::vector<Screen> m_screens;
    std::vector<Strip> m_strips;
    std::vector<Ant> m_ants;
    std::vector<Pad> m_pads;
    std::vector<Car> m_cars;
};

} // namespace

std::unique_ptr<Layer> makeNeonDistrict() { return std::make_unique<NeonDistrict>(); }

} // namespace AnimBg
