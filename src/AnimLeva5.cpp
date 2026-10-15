// Temas Animados, leva 5: John & Mary (o cigarro e o café de madrugada).
// Portado de leva5.js (makeJohnMary) e do makeWindow com opções do fx.js.

#include "AnimKit.h"

#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

namespace AnimBg {
namespace {

using namespace kit;

// Arco de elipse com os ângulos do canvas (y pra baixo, de a0 até a1 em linha reta).
void arcTo(QPainterPath& path, double cx, double cy, double rx2, double ry2, double a0, double a1, bool move)
{
    const int n = qMax(6, int(std::abs(a1 - a0) * 14));
    for (int i = 0; i <= n; ++i) {
        const double a = a0 + (a1 - a0) * i / n;
        const QPointF pt(cx + std::cos(a) * rx2, cy + std::sin(a) * ry2);
        if (i == 0 && move) path.moveTo(pt); else path.lineTo(pt);
    }
}

// ============================================================ a vidraça da janela
// A mesma vidraça na chuva do Windowpane, só que dentro de um vão e em escala menor,
// com as luzes da rua lá fora em âmbar e mel.
class Pane {
public:
    QRectF r;
    double sc = 0.6;

    void init(const Ctx& c)
    {
        static const char* COLS[5] = { "#f2b45a", "#e8a04a", "#f6d48a", "#d88a3a", "#ffcf7a" };
        m_t = 0;
        m_still.reset();
        m_trail.clear();
        if (m_sprites.empty()) for (const char* b : COLS) m_sprites.push_back(softDot(hex(b), 0.55));
        m_bokeh.clear();
        for (int i = 0; i < 22; ++i) m_bokeh.push_back({ R(r.left(), r.right()), R(r.top(), r.bottom()), R(30, 95) * sc, RI(5), R(0, 6.28), R(0.18, 0.4) });
        m_runs.assign(size_t(qMax(1, int(std::lround(5 * INT(c))))), Run{});
        for (Run& d : m_runs) newRun(d);
    }
    void step(double dt)
    {
        m_t += dt;
        for (Run& d : m_runs) {
            // Gota de vidro anda aos trancos: para, junta água, desce de novo.
            d.go -= dt;
            if (d.go <= 0) {
                d.v = d.v > 0 ? 0 : R(60, 190) * (d.r / 5);
                d.go = d.v > 0 ? R(0.3, 1.2) : R(0.4, 2.5);
            }
            if (d.v > 0) {
                d.y += d.v * dt;
                d.x += std::sin(m_t * 3 + d.wob) * 10 * dt;
                if (rnd() < 0.8) m_trail.push_back({ d.x + R(-1.5, 1.5), d.y - d.r, R(1, 2.6), 0 });
            }
            if (d.y > r.bottom() + 10) { newRun(d); d.y = r.top() + R(-30, -10); }
        }
        for (Trail& t : m_trail) t.t += dt;
        m_trail.erase(std::remove_if(m_trail.begin(), m_trail.end(), [](const Trail& t) { return t.t >= 5; }), m_trail.end());
    }
    // Pinta já recortado no vão (quem chama faz o clip).
    void paint(QPainter& p, const Pal& pal, const Ctx& c)
    {
        vfill(p, r, mix(pal.app, Qt::black, 0.25), mix(pal.app, hex("#1a1410"), 0.2), 0, c.H);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for (const Bokeh& b : m_bokeh) {
            p.setOpacity(b.a * (0.8 + 0.2 * std::sin(m_t * 0.4 + b.ph)));
            blit(p, m_sprites[size_t(b.c)], b.x - b.r + std::sin(m_t * 0.05 + b.ph) * 20 * sc, b.y - b.r, b.r * 2, b.r * 2);
        }
        p.setOpacity(1);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.fillRect(r, alpha(pal.app, 0.25));
        if (!m_still.valid(c, pal.app.name())) buildStill(pal, c);
        blit(p, m_still.pm, r.left(), r.top(), r.width(), r.height());
        const QColor tc = mix(pal.app, Qt::white, 0.6);
        p.setPen(Qt::NoPen);
        for (const Trail& t : m_trail) disc(p, t.x, t.y, t.r, alpha(tc, 0.4 * (1 - t.t / 5)));
        const QColor body = alpha(mix(pal.app, Qt::white, 0.5), 0.42);
        for (const Run& d : m_runs) {
            p.setBrush(alpha(Qt::black, 0.25));
            p.drawEllipse(QPointF(d.x, d.y + d.r * 0.3), d.r * 0.92, d.r * 1.12);
            p.setBrush(body);
            p.drawEllipse(QPointF(d.x, d.y), d.r * 0.9, d.r * 1.1);
            p.setBrush(alpha(Qt::white, 0.8));
            p.drawEllipse(QPointF(d.x - d.r * 0.32, d.y - d.r * 0.45), d.r * 0.26, d.r * 0.26);
        }
        p.setBrush(Qt::NoBrush);
    }

private:
    struct Bokeh { double x, y, r; int c; double ph, a; };
    struct Run { double x = 0, y = 0, r = 0, v = 0, go = 0, wob = 0; };
    struct Trail { double x, y, r, t; };
    void newRun(Run& d) const
    {
        d.x = R(r.left(), r.right()); d.y = R(r.top() - 40, r.top() + r.height() * 0.5); d.r = R(5, 9) * sc; d.v = 0; d.go = R(0, 1.5); d.wob = R(0, 6.28);
    }
    void buildStill(const Pal& pal, const Ctx& c)
    {
        QPixmap pm = layer(c, r.width(), r.height());
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing);
        x.setPen(Qt::NoPen);
        x.translate(-r.left(), -r.top());
        // vidro embaçado e as gotinhas paradas: sombra embaixo, corpo translúcido, brilho pequeno em cima
        x.fillRect(r, alpha(mix(pal.app, Qt::white, 0.35), 0.12));
        for (int i = 0; i < 150; ++i) {
            const double px = R(r.left(), r.right()), py = R(r.top(), r.bottom()), rr = (std::pow(rnd(), 2) * 4.5 + 1.2) * sc, ry = rr * R(0.85, 1.15);
            x.setBrush(alpha(Qt::black, 0.28));
            x.drawEllipse(QPointF(px, py + rr * 0.3), rr, ry);
            QRadialGradient g(QPointF(px, py), rr, QPointF(px, py + rr * 0.35));
            g.setColorAt(0, alpha(mix(pal.app, Qt::white, 0.55), 0.35));
            g.setColorAt(1, alpha(mix(pal.app, Qt::white, 0.3), 0.12));
            x.setBrush(g);
            x.drawEllipse(QPointF(px, py), rr * 0.92, ry * 0.92);
            x.save();
            x.translate(px - rr * 0.25, py - ry * 0.45);
            x.rotate(qRadiansToDegrees(-0.5));
            x.setBrush(alpha(Qt::white, 0.55));
            x.drawEllipse(QPointF(0, 0), rr * 0.3, rr * 0.18);
            x.restore();
        }
        x.end();
        m_still.set(c, pm, pal.app.name());
    }
    std::vector<Bokeh> m_bokeh;
    std::vector<Run> m_runs;
    std::vector<Trail> m_trail;
    std::vector<QPixmap> m_sprites;
    Cache m_still;
    double m_t = 0;
};

// ============================================================ John & Mary
// Natureza-morta de madrugada na faixa de mesa da esquerda: mesa de madeira sob o abajur,
// o cinzeiro de vidro com o cigarro apoiado no sulco queimando devagar (a cinza cresce e cai),
// o fio de fumaça subindo e se enrolando, a caneca de café soltando vapor e, na parede,
// uma janela com chuva e as luzes da rua.
class JohnMary : public Layer {
public:
    void init(Ctx& c) override
    {
        const double c0 = C(c);
        m_win.r = QRectF(QPointF(c0 - 112, 150), QPointF(c0 + 112, 452));
        m_win.init(c);
        for (int i = 0; i < 60; ++i) m_win.step(1 / 30.0);
        m_back.reset();
        for (auto& s : m_smoke) s.clear();
        for (auto& s : m_steam) s.clear();
        m_bits.clear();
        m_t = 0;
        m_emitS = m_emitV = 0;
        m_fade = 1;
        m_burn = R(4, 24);
        m_ash = qMin(m_burn, R(2, 10));
        // já abre com a fumaça subindo inteira: simula uns 12 segundos antes
        for (int i = 0; i < 360; ++i) step(1 / 30.0, c);
    }
    void step(double dt, Ctx& c) override
    {
        m_t += dt;
        m_win.step(dt);
        // queima devagar: ~1 px a cada 9 s; no fim, um cigarro novo (a cena some e volta)
        m_burn += dt / 9;
        m_ash += dt / 9;
        if (m_burn > FULL - 6) {
            m_fade -= dt * 0.5;
            if (m_fade <= 0) { m_burn = 0; m_ash = 0; m_fade = 0.0001; }
        } else if (m_fade < 1) {
            m_fade = qMin(1.0, m_fade + dt * 0.5);
        }
        const Cig k = cig(c);
        // a cinza comprida cai no cinzeiro
        if (m_ash > 16) {
            for (int i = 0; i < 5; ++i) m_bits.push_back({ k.ex - R(2, 14), k.ey, R(-6, 6), R(0, 20), 0, R(1, 2.2) });
            m_ash = 1.5;
        }
        for (Bit& b : m_bits) {
            b.t += dt;
            b.vy += 160 * dt;
            b.x += b.vx * dt;
            b.y = qMin(AY + 8, b.y + b.vy * dt);
        }
        m_bits.erase(std::remove_if(m_bits.begin(), m_bits.end(), [](const Bit& b) { return b.t >= 30; }), m_bits.end());
        // fumaça: dois fios contínuos que nascem na brasa
        m_emitS += dt;
        while (m_emitS > 1 / 45.0) {
            m_emitS -= 1 / 45.0;
            for (int st = 0; st < 2; ++st) m_smoke[st].push_back({ k.ex + (st ? 0.5 : -0.5), k.ey - 2, 0, R(9, 12), m_t });
        }
        const double spd = 0.8 + 0.2 * INT(c);
        for (int st = 0; st < 2; ++st) {
            const double sd = st ? 2.3 : 0, sgn = st ? 1 : -1;
            for (Puff& q : m_smoke[st]) {
                q.age += dt;
                const double turb = qMin(1.0, q.age / 3.2);      // começa liso, vai ficando turbulento
                q.y -= (36 - q.age * 1.6) * dt * spd;
                // os fios nascem juntos e se separam com a altura; turbulência miúda por cima do balanço largo
                q.x += (std::sin(q.y * 0.031 + m_t * 0.7 + q.ph * 0.25) * (2 + 9 * turb) + std::sin(m_t * 0.23) * 5 * turb
                        + std::sin(q.y * 0.083 - m_t * 1.3 + sd) * 6 * turb * turb + std::sin(q.y * 0.16 + m_t * 2.1 + sd * 1.7) * 4 * turb
                        + sgn * 3 * turb * turb) * dt;
            }
            auto& v = m_smoke[st];
            v.erase(std::remove_if(v.begin(), v.end(), [](const Puff& q) { return q.age >= q.life; }), v.end());
        }
        // vapor do café: fios mais largos, que somem rápido
        m_emitV += dt;
        while (m_emitV > 1 / 20.0) {
            m_emitV -= 1 / 20.0;
            for (int i = 0; i < 2; ++i) m_steam[i].push_back({ MX(c) + (i ? 9 : -10) + R(-1.5, 1.5), MY - 66, 0, R(3, 4.2), m_t });
        }
        for (int lane = 0; lane < 2; ++lane) {
            for (Puff& q : m_steam[lane]) {
                q.age += dt;
                q.y -= (24 - q.age * 2.5) * dt;
                q.x += (std::sin(q.y * 0.05 + m_t * 0.8 + lane * 2.6) * (1.5 + q.age * 4.5) + (lane ? 2.5 : -2.5) * q.age) * dt;
            }
            auto& v = m_steam[lane];
            v.erase(std::remove_if(v.begin(), v.end(), [](const Puff& q) { return q.age >= q.life; }), v.end());
        }
    }
    void paint(QPainter& p, const Pal& pal, Ctx& c) override
    {
        const double cx = C(c), ax = AX(c);
        if (!m_back.valid(c, pal.app.name())) buildBack(pal, c);
        blit(p, m_back.pm, 0, 0, c.W, c.H);
        p.setRenderHint(QPainter::Antialiasing, true);

        // a janela: vidraça na chuva recortada no vão, moldura de madeira, cruzeta e peitoril
        {
            const QRectF& w = m_win.r;
            const double x0 = w.left(), y0 = w.top(), x1 = w.right(), y1 = w.bottom();
            const QColor wd = mix(pal.app, hex("#4a2e1a"), 0.5), wl = mix(wd, hex("#ffd8a0"), 0.25);
            p.save();
            p.setClipRect(w);
            m_win.paint(p, pal, c);
            QLinearGradient ig(0, y0, 0, y1);
            ig.setColorAt(0, alpha(Qt::black, 0.25));
            ig.setColorAt(0.15, alpha(Qt::black, 0));
            ig.setColorAt(0.85, alpha(Qt::black, 0));
            ig.setColorAt(1, alpha(Qt::black, 0.3));
            p.fillRect(w, ig);
            p.restore();
            const double f = 12, mid = (x0 + x1) / 2, my = y0 + (y1 - y0) * 0.42;
            p.fillRect(QRectF(x0 - f, y0 - f, x1 - x0 + f * 2, f), wd);
            p.fillRect(QRectF(x0 - f, y1, x1 - x0 + f * 2, f), wd);
            p.fillRect(QRectF(x0 - f, y0 - f, f, y1 - y0 + f * 2), wd);
            p.fillRect(QRectF(x1, y0 - f, f, y1 - y0 + f * 2), wd);
            p.fillRect(QRectF(mid - 3, y0, 6, y1 - y0), wd);
            p.fillRect(QRectF(x0, my - 3, x1 - x0, 6), wd);
            p.fillRect(QRectF(x0, y0, x1 - x0, 3), alpha(Qt::black, 0.35));
            p.fillRect(QRectF(x0, y0, 3, y1 - y0), alpha(Qt::black, 0.35));
            // peitoril: tábua mais larga, com o canto de cima pegando a luz
            p.fillRect(QRectF(x0 - f - 10, y1 + f, x1 - x0 + f * 2 + 20, 9), wd);
            p.fillRect(QRectF(x0 - f - 10, y1 + f, x1 - x0 + f * 2 + 20, 1.5), wl);
            p.fillRect(QRectF(x0 - f - 6, y1 + f + 9, x1 - x0 + f * 2 + 12, 4), alpha(Qt::black, 0.4));
            p.fillRect(QRectF(x0 - f, y0 - f, 2, y1 - y0 + f * 2), wl);
            p.fillRect(QRectF(x0 - f, y0 - f, x1 - x0 + f * 2, 1.5), wl);
        }

        // luz do abajur (fora do quadro, em cima à esquerda) e a poça de luz na mesa
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        {
            QRadialGradient lg(QPointF(cx - 90, 360), 460);
            lg.setColorAt(0, alpha(hex("#ffc477"), 0.22));
            lg.setColorAt(1, alpha(hex("#ffc477"), 0));
            p.fillRect(QRectF(cx - 550, 0, 920, c.H), lg);
            QRadialGradient pool(QPointF(cx, AY + 10), 230);
            pool.setColorAt(0, alpha(hex("#ffcf8a"), 0.14));
            pool.setColorAt(1, alpha(hex("#ffcf8a"), 0));
            p.fillRect(QRectF(cx - 230, AY - 220, 460, 460), pool);
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);

        // cinzas que caíram
        p.setPen(Qt::NoPen);
        p.setBrush(alpha(hex("#8a8580"), 0.9));
        for (const Bit& b : m_bits) p.drawEllipse(QPointF(b.x, b.y), b.r * 1.4, b.r * 0.7);

        // o cigarro: filtro laranja, papel branco, a cinza e a brasa
        {
            const Cig k = cig(c);
            p.save();
            p.setOpacity(qMax(0.0, m_fade));
            p.translate(k.fx, k.fy);
            p.rotate(qRadiansToDegrees(-k.a));
            const double r = 3.3;
            p.fillRect(QRectF(-FILTER - k.L, r + 1, FILTER + k.L, 2.2), alpha(Qt::black, 0.35));
            QLinearGradient fg(0, -r, 0, r);
            fg.setColorAt(0, hex("#e8a860"));
            fg.setColorAt(0.5, hex("#c8823a"));
            fg.setColorAt(1, hex("#7a4a20"));
            p.fillRect(QRectF(-FILTER, -r, FILTER, r * 2), fg);
            for (int i = 0; i < 9; ++i) p.fillRect(QRectF(-FILTER + 1 + i * 2.1, -r + 0.6 + (i % 3) * 1.8, 0.8, 0.8), alpha(hex("#5a3010"), 0.35));
            QLinearGradient pg(0, -r, 0, r);
            pg.setColorAt(0, hex("#fbf8f2"));
            pg.setColorAt(0.55, hex("#e6e0d6"));
            pg.setColorAt(1, hex("#9a948a"));
            const double paper = qMax(0.0, k.L - m_ash);
            p.fillRect(QRectF(-FILTER - paper, -r, paper, r * 2), pg);
            p.fillRect(QRectF(-FILTER - 1.2, -r, 1.2, r * 2), alpha(hex("#e8c87a"), 0.6));
            QLinearGradient ag(0, -r, 0, r);
            ag.setColorAt(0, hex("#b8b2aa"));
            ag.setColorAt(1, hex("#5a5650"));
            const double al = qMin(m_ash, k.L);
            p.fillRect(QRectF(-FILTER - k.L, -r * 0.92, al, r * 1.84), ag);
            for (double i = 0; i < al; i += 2.4) p.fillRect(QRectF(-FILTER - k.L + i, -r * 0.9, 0.7, r * 1.8), alpha(hex("#3a3632"), 0.5));
            // brasa: pulsa devagar, como quem respira
            const double glow = 0.75 + 0.25 * std::sin(m_t * 1.7) * std::sin(m_t * 0.63 + 1);
            const double bx = -FILTER - paper;
            p.fillRect(QRectF(bx - 1.6, -r * 0.95, 2.4, r * 1.9), alpha(hex("#ff5a1a"), 0.9 * glow));
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            QRadialGradient eg(QPointF(bx, 0), 14);
            eg.setColorAt(0, alpha(hex("#ff8a3a"), 0.55 * glow));
            eg.setColorAt(1, alpha(hex("#ff8a3a"), 0));
            p.fillRect(QRectF(bx - 14, -14, 28, 28), eg);
            p.restore();
        }

        // a parte da frente da borda do cinzeiro passa por cima do cigarro
        {
            QPainterPath rim;
            arcTo(rim, ax, AY, 52, 14, 0.1, M_PI - 0.1, true);
            p.setBrush(Qt::NoBrush);
            p.setPen(pen(alpha(hex("#dce8ee"), 0.45), 2.2));
            p.drawPath(rim);
        }

        // fumaça e vapor: cada fio é um traço contínuo, quebrado só quando muda a transparência
        // ou a grossura (traço por segmento deixa emenda e dá textura de hachura)
        const QColor sc = mix(hex("#d8dde4"), pal.app, 0.1);
        static const double PASS[2][2] = { { 2.6, 0.05 }, { 1, 0.26 } };
        for (const auto& ps : PASS)
            for (int st = 0; st < 2; ++st)
                ribbon(p, m_smoke[st], sc,
                       [&](const Puff& q) { return std::pow(1 - q.age / q.life, 2.4) * ps[1] * qMin(1.0, q.age * 3); },
                       [&](const Puff& q) { return (0.6 + q.age * 1.2) * ps[0]; });
        for (int lane = 0; lane < 2; ++lane)
            ribbon(p, m_steam[lane], hex("#f4efe8"),
                   [](const Puff& q) { return std::pow(std::sin(q.age / q.life * M_PI), 1.5) * 0.07; },
                   [](const Puff& q) { return 1.5 + q.age * 3.2; });
        p.setPen(Qt::NoPen);
    }

private:
    struct Puff { double x, y, age, life, ph; };
    struct Bit { double x, y, vx, vy, t, r; };
    struct Cig { double a, fx, fy, ex, ey, L; };

    static constexpr double TY = 560;                 // linha onde a mesa encontra a parede
    static constexpr double AY = 606, MY = 612;       // topo do cinzeiro, base da caneca
    static constexpr double FULL = 64, FILTER = 20;   // comprimento do papel e do filtro
    // a composição inteira cabe na faixa de mesa entre a barra lateral e a folha
    static double C(const Ctx& c) { return zx(c, 250); }
    static double AX(const Ctx& c) { return C(c) - 62; }
    static double MX(const Ctx& c) { return C(c) + 92; }

    // o cigarro apoiado no sulco da frente-direita: filtro pra fora, brasa pra dentro do cinzeiro.
    // L = papel que ainda não queimou + cinza pendurada; a brasa fica entre os dois.
    Cig cig(const Ctx& c) const
    {
        const double a = -0.16, fx = AX(c) + 70, fy = AY + 2, paper = FULL - m_burn, L = paper + m_ash, d = FILTER + paper;
        return { a, fx, fy, fx - std::cos(a) * d, fy + std::sin(a) * d, L };
    }

    template <class A, class Wd>
    static void ribbon(QPainter& p, const std::vector<Puff>& pts, const QColor& col, A alphaOf, Wd widthOf)
    {
        QPainterPath path;
        bool open = false;
        int ka = -1, kw = -1;
        auto flush = [&]() {
            if (!open) return;
            p.setPen(pen(alpha(col, ka / 60.0), kw / 2.0));
            p.drawPath(path);
            path = QPainterPath();
            open = false;
        };
        for (size_t i = 1; i < pts.size(); ++i) {
            const Puff& a0 = pts[i - 1];
            const Puff& b0 = pts[i];
            if (std::abs(a0.ph - b0.ph) > 0.2) { flush(); ka = -1; continue; }
            const double al = alphaOf(b0);
            if (al < 0.004) { flush(); ka = -1; continue; }
            const int na = int(std::lround(al * 60)), nw = int(std::lround(widthOf(b0) * 2));
            if (na != ka || nw != kw || !open) {
                flush();
                ka = na; kw = nw;
                path.moveTo(a0.x, a0.y);
                open = true;
            }
            path.lineTo(b0.x, b0.y);
        }
        flush();
    }

    void buildBack(const Pal& pal, const Ctx& c)
    {
        const double ax = AX(c), mx = MX(c);
        QPixmap pm = layer(c, c.W, c.H);
        QPainter x(&pm);
        x.setRenderHint(QPainter::Antialiasing, true);
        x.setPen(Qt::NoPen);
        // parede escura e quente
        vfill(x, QRectF(0, 0, c.W, TY), mix(pal.app, Qt::black, 0.35), mix(pal.app, hex("#2a1c14"), 0.4), 0, TY);
        // tampo da mesa: madeira escura com veio
        vfill(x, QRectF(0, TY, c.W, c.H - TY), mix(pal.app, hex("#3a2416"), 0.55), mix(pal.app, hex("#1a0f08"), 0.5), TY, c.H);
        x.fillRect(QRectF(0, TY - 2, c.W, 3), alpha(Qt::black, 0.5));
        x.setBrush(Qt::NoBrush);
        for (int i = 0; i < 26; ++i) {
            const double y = TY + 6 + i * 7.5 + R(-2, 2);
            x.setPen(QPen(alpha(rnd() < 0.5 ? QColor(Qt::black) : hex("#ffd8a8"), R(0.03, 0.08)), R(0.6, 1.6)));
            QPainterPath gr(QPointF(0, y));
            for (double xx = 0; xx <= c.W + 40; xx += 40) gr.lineTo(xx, y + std::sin(xx * 0.006 + i) * 2.5);
            x.drawPath(gr);
        }
        x.setPen(Qt::NoPen);

        // a caneca: cerâmica creme, a luz do abajur vindo da esquerda
        {
            const double mw = 54, mh = 64, mt = MY - mh;
            x.setBrush(alpha(Qt::black, 0.45));
            x.drawEllipse(QPointF(mx + 10, MY + 2), 40, 7);
            x.setBrush(Qt::NoBrush);
            QPainterPath h1, h2;
            arcTo(h1, mx + mw / 2 + 4, mt + 30, 13, 17, -1.3, 1.3, true);
            arcTo(h2, mx + mw / 2 + 4, mt + 30, 10, 14, -1.2, 1.2, true);
            x.setPen(QPen(mix(hex("#8a7c6c"), pal.app, 0.25), 7));
            x.drawPath(h1);
            x.setPen(QPen(alpha(Qt::black, 0.25), 2));
            x.drawPath(h2);
            x.setPen(Qt::NoPen);
            QLinearGradient mg(mx - mw / 2, 0, mx + mw / 2, 0);
            mg.setColorAt(0, mix(hex("#e8dccb"), pal.app, 0.12));
            mg.setColorAt(0.4, mix(hex("#c8baa6"), pal.app, 0.22));
            mg.setColorAt(1, mix(hex("#5a4e42"), pal.app, 0.3));
            QPainterPath body(QPointF(mx - mw / 2, mt));
            body.lineTo(mx - mw / 2, MY - 6);
            body.quadTo(mx - mw / 2, MY, mx - mw / 2 + 8, MY);
            body.lineTo(mx + mw / 2 - 8, MY);
            body.quadTo(mx + mw / 2, MY, mx + mw / 2, MY - 6);
            body.lineTo(mx + mw / 2, mt);
            body.closeSubpath();
            x.fillPath(body, mg);
            x.setBrush(mix(hex("#e0d4c2"), pal.app, 0.15));
            x.drawEllipse(QPointF(mx, mt), mw / 2, 7);
            x.setBrush(hex("#2a170c"));
            x.drawEllipse(QPointF(mx, mt + 1.2), mw / 2 - 4, 5);
            x.setBrush(alpha(hex("#c88a4a"), 0.35));
            x.drawEllipse(QPointF(mx - 6, mt + 0.4), 10, 1.8);
        }

        // o cinzeiro de vidro: corpo, borda grossa com os sulcos
        {
            const double rw = 52, rh = 14, hb = 16;
            x.setBrush(alpha(Qt::black, 0.45));
            x.drawEllipse(QPointF(ax + 8, AY + hb + 4), rw + 6, rh);
            QLinearGradient gb(ax - rw, 0, ax + rw, 0);
            gb.setColorAt(0, alpha(hex("#c8d4dc"), 0.55));
            gb.setColorAt(0.5, alpha(hex("#6a7a84"), 0.45));
            gb.setColorAt(1, alpha(hex("#2a3238"), 0.6));
            QPainterPath side;
            arcTo(side, ax, AY + hb, rw - 3, rh - 2, 0, M_PI, true);
            side.lineTo(ax - rw, AY);
            arcTo(side, ax, AY, rw, rh, M_PI, 0, false);
            side.closeSubpath();
            x.fillPath(side, gb);
            x.setBrush(alpha(hex("#1a1e22"), 0.85));
            x.drawEllipse(QPointF(ax, AY + 1), rw - 9, rh - 5);
            x.setBrush(alpha(hex("#5a5450"), 0.6));
            for (int i = 0; i < 40; ++i) x.drawEllipse(QPointF(ax + R(-34, 34), AY + 2 + R(-4, 4)), R(1, 3), R(0.6, 1.4));
            x.setBrush(Qt::NoBrush);
            QPainterPath back;
            arcTo(back, ax, AY, rw, rh, M_PI * 1.05, M_PI * 1.95, true);
            x.setPen(QPen(alpha(hex("#e8f2f8"), 0.55), 1.4));
            x.drawPath(back);
            x.setPen(QPen(alpha(hex("#e8f2f8"), 0.25), 1));
            x.drawEllipse(QPointF(ax, AY), rw - 9, rh - 5);
        }
        x.end();
        m_back.set(c, pm, pal.app.name());
    }

    Pane m_win;
    Cache m_back;
    std::vector<Puff> m_smoke[2], m_steam[2];
    std::vector<Bit> m_bits;
    double m_t = 0, m_emitS = 0, m_emitV = 0, m_burn = 0, m_ash = 0, m_fade = 1;
};

} // namespace

std::unique_ptr<Layer> makeLeva5(const QString& id)
{
    if (id == QLatin1String("johnmary")) return std::make_unique<JohnMary>();
    return nullptr;
}

} // namespace AnimBg
