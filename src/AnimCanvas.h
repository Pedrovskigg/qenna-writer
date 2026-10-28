#pragma once

// Uma camada fina por cima do QPainter que fala a língua do canvas do Weather Lab (beginPath, moveTo,
// quadraticCurveTo, arc, ellipse, fill, stroke, globalAlpha, 'lighter'...). Os temas Animados nascem no
// Lab em JavaScript; com isto o porte pro C++ é quase linha a linha, e a arte sai igual.
// Só os arquivos de temas novos incluem (AnimGenres*.cpp).

#include "AnimKit.h"

#include <QFontMetricsF>
#include <QHash>
#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>

#include <cmath>
#include <vector>

namespace AnimBg {
namespace cv {

using namespace kit;

constexpr double TAU = 6.283185307179586;

// cor do Lab: '#rrggbb' com alfa (o rgba() do JS)
inline QColor rgba(const char* h, double a) { return alpha(hex(h), a); }
inline QColor rgba(const QColor& c, double a) { return alpha(c, a); }

class Cv {
public:
    explicit Cv(QPainter& painter) : p(painter)
    {
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        path.setFillRule(Qt::WindingFill);
        base = p.opacity();
    }

    QPainter& p;
    QBrush fill_ = QBrush(Qt::black), stroke_ = QBrush(Qt::black);
    double lw = 1;
    Qt::PenCapStyle cap = Qt::FlatCap;
    Qt::PenJoinStyle join = Qt::MiterJoin;
    double ga = 1;                 // globalAlpha
    QFont font;
    int align = -1;                // -1 left, 0 center, 1 right
    int baseline = 0;              // 0 alphabetic, 1 middle
    QPainterPath path;
    bool fresh = true;             // o caminho ainda não tem ponto (o QPainterPath nasce com um MoveTo(0,0) escondido)

    // ---- estado
    void fillStyle(const QBrush& b) { fill_ = b; }
    void fillStyle(const QColor& c) { fill_ = QBrush(c); }
    void strokeStyle(const QBrush& b) { stroke_ = b; }
    void strokeStyle(const QColor& c) { stroke_ = QBrush(c); }
    void lineWidth(double w) { lw = w; }
    void roundCaps() { cap = Qt::RoundCap; join = Qt::RoundJoin; }
    void alphaG(double a) { ga = a; p.setOpacity(base * a); }
    void lighter() { p.setCompositionMode(QPainter::CompositionMode_Plus); }
    void over() { p.setCompositionMode(QPainter::CompositionMode_SourceOver); }
    void atop() { p.setCompositionMode(QPainter::CompositionMode_SourceAtop); }
    void destOut() { p.setCompositionMode(QPainter::CompositionMode_DestinationOut); }
    void destOver() { p.setCompositionMode(QPainter::CompositionMode_DestinationOver); }

    struct State { QBrush f, s; double lw, ga; Qt::PenCapStyle cap; Qt::PenJoinStyle join; QFont font; int align, baseline; };
    std::vector<State> stack;
    void save() { stack.push_back({ fill_, stroke_, lw, ga, cap, join, font, align, baseline }); p.save(); }
    void restore()
    {
        p.restore();
        if (stack.empty()) return;
        const State s = stack.back();
        stack.pop_back();
        fill_ = s.f; stroke_ = s.s; lw = s.lw; ga = s.ga; cap = s.cap; join = s.join; font = s.font; align = s.align; baseline = s.baseline;
    }
    void translate(double x, double y) { p.translate(x, y); }
    void rotate(double a) { p.rotate(a * 180.0 / M_PI); }
    void scale(double x, double y) { p.scale(x, y); }

    // ---- caminho
    void begin() { path = QPainterPath(); path.setFillRule(Qt::WindingFill); fresh = true; }
    void moveTo(double x, double y) { path.moveTo(x, y); fresh = false; }
    void lineTo(double x, double y) { if (fresh) moveTo(x, y); else path.lineTo(x, y); }
    void quadTo(double cx, double cy, double x, double y) { if (fresh) moveTo(cx, cy); path.quadTo(cx, cy, x, y); }
    void bezTo(double c1x, double c1y, double c2x, double c2y, double x, double y) { if (fresh) moveTo(c1x, c1y); path.cubicTo(c1x, c1y, c2x, c2y, x, y); }
    void close() { path.closeSubpath(); }
    void rect(double x, double y, double w, double h) { path.addRect(QRectF(x, y, w, h).normalized()); fresh = false; }
    // elipse do canvas (rotação e ângulos em radianos, sentido horário na tela); liga com linha ao ponto atual
    void ellipse(double cx, double cy, double rx, double ry, double rot, double a0, double a1, bool ccw = false)
    {
        double sweep = a1 - a0;
        if (!ccw) { if (sweep >= TAU) sweep = TAU; else { while (sweep < 0) sweep += TAU; } }
        else { if (-sweep >= TAU) sweep = -TAU; else { while (sweep > 0) sweep -= TAU; } }
        const int n = qMax(8, int(std::ceil(std::abs(sweep) / TAU * qMax(24.0, (rx + ry) * 0.9))));
        const double cr = std::cos(rot), sr = std::sin(rot);
        for (int i = 0; i <= n; ++i) {
            const double t = a0 + sweep * i / n, ex = std::cos(t) * rx, ey = std::sin(t) * ry;
            lineTo(cx + ex * cr - ey * sr, cy + ex * sr + ey * cr);
        }
    }
    void arc(double cx, double cy, double r, double a0, double a1, bool ccw = false) { ellipse(cx, cy, r, r, 0, a0, a1, ccw); }
    void fill() { p.fillPath(path, fill_); }
    void fillEvenOdd() { path.setFillRule(Qt::OddEvenFill); p.fillPath(path, fill_); }
    void stroke() { QPen pen(stroke_, lw, Qt::SolidLine, cap, join); p.strokePath(path, pen); }
    void clip() { p.setClipPath(path, Qt::IntersectClip); }
    void clipEvenOdd() { path.setFillRule(Qt::OddEvenFill); p.setClipPath(path, Qt::IntersectClip); }

    // ---- atalhos
    void fillRect(double x, double y, double w, double h) { p.fillRect(QRectF(x, y, w, h).normalized(), fill_); }
    void strokeRect(double x, double y, double w, double h) { begin(); rect(x, y, w, h); stroke(); }
    void line(double x0, double y0, double x1, double y1) { begin(); moveTo(x0, y0); lineTo(x1, y1); stroke(); }
    void image(const QPixmap& pm, double x, double y) { if (pm.isNull()) return; const double d = pm.devicePixelRatio(); p.drawPixmap(QRectF(x, y, pm.width() / d, pm.height() / d), pm, QRectF(pm.rect())); }
    void image(const QPixmap& pm, double x, double y, double w, double h) { if (pm.isNull()) return; p.drawPixmap(QRectF(x, y, w, h), pm, QRectF(pm.rect())); }

    // ---- texto
    void setFont(double px, int weight, const QStringList& fam = { QStringLiteral("IBM Plex Sans"), QStringLiteral("Arial") }, bool italic = false)
    {
        QFont f;
        f.setFamilies(fam);
        f.setPixelSize(qMax(1, int(std::lround(px))));
        f.setWeight(QFont::Weight(weight));
        f.setItalic(italic);
        font = f;
    }
    QPointF textOrigin(const QString& s, double x, double y) const
    {
        const QFontMetricsF fm(font);
        const double w = fm.horizontalAdvance(s);
        const double bx = align < 0 ? x : align == 0 ? x - w / 2 : x - w;
        const double by = baseline == 1 ? y + (fm.ascent() - fm.descent()) / 2 : y;
        return QPointF(bx, by);
    }
    double measure(const QString& s) const { return QFontMetricsF(font).horizontalAdvance(s); }
    void fillText(const QString& s, double x, double y)
    {
        // drawText usa os glifos em cache: bem mais barato que montar o contorno a cada quadro
        p.save();
        p.setFont(font);
        p.setPen(QPen(fill_, 0));
        p.drawText(textOrigin(s, x, y), s);
        p.restore();
    }
    void strokeText(const QString& s, double x, double y)
    {
        QPainterPath tp;
        tp.addText(textOrigin(s, x, y), font, s);
        QPen pen(stroke_, lw, Qt::SolidLine, cap, join);
        p.strokePath(tp, pen);
    }

private:
    double base = 1;
};

// gradientes do canvas
inline QLinearGradient lin(double x0, double y0, double x1, double y1, std::initializer_list<std::pair<double, QColor>> st)
{
    QLinearGradient g(x0, y0, x1, y1);
    for (const auto& s : st) g.setColorAt(s.first, s.second);
    return g;
}
inline QLinearGradient vgrad(double y0, double y1, const QColor& c0, const QColor& c1) { return lin(0, y0, 0, y1, { { 0, c0 }, { 1, c1 } }); }
inline QRadialGradient rad(double x0, double y0, double r0, double x1, double y1, double r1, std::initializer_list<std::pair<double, QColor>> st)
{
    QRadialGradient g(QPointF(x1, y1), r1, QPointF(x0, y0), r0);
    for (const auto& s : st) g.setColorAt(s.first, s.second);
    return g;
}

// o glowAt do Lab (miolo 1, 0,4 em 35%, 0 na borda) desenhado de um sprite pronto: centenas por quadro custam pouco
inline const QPixmap& glowSprite(QRgb rgb)
{
    static QHash<QRgb, QPixmap> cache;
    auto it = cache.find(rgb);
    if (it != cache.end()) return *it;
    QPixmap pm(128, 128);
    pm.fill(Qt::transparent);
    QPainter g(&pm);
    QRadialGradient rg(QPointF(64, 64), 64);
    const QColor c = QColor::fromRgb(rgb);
    rg.setColorAt(0, alpha(c, 1));
    rg.setColorAt(0.35, alpha(c, 0.4));
    rg.setColorAt(1, alpha(c, 0));
    g.fillRect(pm.rect(), rg);
    g.end();
    return *cache.insert(rgb, pm);
}
inline void glowAt(Cv& x, double gx, double gy, double r, const QColor& col, double a)
{
    if (a <= 0.003 || r < 0.5) return;
    const double old = x.ga;
    x.alphaG(old * qMin(1.0, a));
    x.p.drawPixmap(QRectF(gx - r, gy - r, r * 2, r * 2), glowSprite(col.rgb()), QRectF(0, 0, 128, 128));
    x.alphaG(old);
}
inline void glowAt(Cv& x, double gx, double gy, double r, const char* col, double a) { glowAt(x, gx, gy, r, hex(col), a); }

// pick() pra listas de cor do Lab
inline QColor pickHex(std::initializer_list<const char*> l)
{
    const int i = RI(int(l.size()));
    return hex(*(l.begin() + i));
}

} // namespace cv
} // namespace AnimBg
