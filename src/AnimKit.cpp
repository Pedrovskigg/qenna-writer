#include "AnimKit.h"

#include <QRadialGradient>

#include <cmath>

namespace AnimBg {
namespace kit {

QPen pen(const QColor& c, double w)
{
    QPen p(c, w);
    p.setCapStyle(Qt::RoundCap);
    p.setJoinStyle(Qt::RoundJoin);
    return p;
}

void disc(QPainter& p, double x, double y, double r, const QColor& c)
{
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(QPointF(x, y), r, r);
}

void glow(QPainter& p, double x, double y, double r, const QColor& c, double a)
{
    if (r <= 0.5 || a <= 0.003) return;
    QRadialGradient g(QPointF(x, y), r);
    g.setColorAt(0, alpha(c, a));
    g.setColorAt(1, alpha(c, 0));
    p.fillRect(QRectF(x - r, y - r, r * 2, r * 2), g);
}

double Profile::at(double u) const
{
    if (pts.size() < 2) return 0;
    const double j = qBound(0.0, u, 1.0) * (pts.size() - 1);
    const size_t lo = size_t(std::floor(j));
    const double f = j - lo;
    const size_t hi = qMin(pts.size() - 1, lo + 1);
    return pts[lo] + (pts[hi] - pts[lo]) * f;
}

Profile profile(int it, double rough)
{
    Profile f;
    f.pts = { R(-0.3, 0.3), R(-0.3, 0.3) };
    double a = 0.8;
    for (int i = 0; i < it; ++i, a *= rough) {
        std::vector<double> nx;
        nx.reserve(f.pts.size() * 2);
        for (size_t j = 0; j + 1 < f.pts.size(); ++j) {
            nx.push_back(f.pts[j]);
            nx.push_back((f.pts[j] + f.pts[j + 1]) / 2 + R(-a, a));
        }
        nx.push_back(f.pts.back());
        f.pts.swap(nx);
    }
    return f;
}

void fillProfile(QPainter& p, const Ctx& c, const Profile& f, double base, double amp, const QColor& col, double step)
{
    QPainterPath path(QPointF(-10, c.H + 10));
    for (double x = -10; x <= c.W + 10; x += step) path.lineTo(x, base + f.at(x / c.W) * amp);
    path.lineTo(c.W + 10, c.H + 10);
    path.closeSubpath();
    p.fillPath(path, col);
}

QPixmap starLayer(const Ctx& c, int n, double yMax, double aMax)
{
    QPixmap pm = layer(c, c.W, c.H);
    QPainter x(&pm);
    const int count = int(std::lround(n * c.W / 1440.0));
    for (int i = 0; i < count; ++i) {
        const double s = rnd() < 0.08 ? 1.8 : R(0.7, 1.3);
        x.fillRect(QRectF(R(0, c.W), R(0, yMax), s, s), alpha(Qt::white, R(0.12, aMax)));
    }
    return pm;
}

const QPixmap& smokeDot()
{
    static const QPixmap pm = softDot(hex("#8a90a0"), 0.1);
    return pm;
}

double zx(const Ctx& c, double labX)
{
    const QRectF& l = c.zones.left;
    if (l.width() >= 120) return labX - 45 + l.left();
    const QRectF& r = c.zones.right;
    if (r.width() >= 120) return labX - 45 + r.left();
    return labX - 45 + c.W * 0.03;
}

Fig::Hair hairAt(int i)
{
    static const Fig::Hair H[] = { Fig::Short, Fig::Long, Fig::Bun, Fig::Cap, Fig::Pony, Fig::Curly, Fig::Short, Fig::Long };
    return H[((i % 8) + 8) % 8];
}

// ------------------------------------------------------------ gente

namespace {

QPainterPath topHalf(double cx, double cy, double rx, double ry)
{
    QPainterPath h;
    h.moveTo(cx + rx, cy);
    h.arcTo(QRectF(cx - rx, cy - ry, rx * 2, ry * 2), 0, 180);
    h.closeSubpath();
    return h;
}

QPainterPath ell(double cx, double cy, double rx, double ry)
{
    QPainterPath e;
    e.addEllipse(QPointF(cx, cy), rx, ry);
    return e;
}

} // namespace

void figure(QPainter& p, double x, double y, double h, const Fig& o)
{
    const double b = o.build, ph = o.ph;
    const int side = o.side;
    const double walk = o.pose == Fig::Walk ? std::sin(ph) : 0;
    p.save();
    p.translate(x, y);
    if (o.pose == Fig::Sit) p.translate(0, 0.47 * h);
    else if (o.pose == Fig::Bust) p.translate(0, 0.5 * h);
    p.scale(h, h);
    p.rotate(o.lean * 180 / M_PI);
    const double bob = o.pose == Fig::Walk ? std::abs(std::cos(ph)) * 0.012 : 0;
    p.translate(0, -bob);
    const QColor col = o.col;
    const double SW = 0.116 * b, WA = 0.078 * b, HP = 0.095 * b;
    auto line = [&](double x0, double y0, double x1, double y1, double w) {
        p.setPen(pen(col, w));
        p.drawLine(QPointF(x0, y0), QPointF(x1, y1));
    };
    // pernas
    if (o.pose == Fig::Stand || o.pose == Fig::Walk) {
        if (o.skirt) {
            QPainterPath sk(QPointF(-HP, -0.47));
            sk.lineTo(-HP - 0.03, -0.25);
            sk.lineTo(HP + 0.03, -0.25);
            sk.lineTo(HP, -0.47);
            sk.closeSubpath();
            p.fillPath(sk, col);
        }
        for (int sgn : { -1, 1 }) {
            const double sw = walk * sgn, hx = sgn * 0.048 * b, kx = hx + sw * 0.045, fx = hx + sw * 0.085, kneeY = -0.25 + std::abs(sw) * 0.01;
            line(hx, o.skirt ? -0.3 : -0.47, kx, kneeY, o.skirt ? 0.045 : 0.082 * b);
            line(kx, kneeY, fx, -0.025, o.skirt ? 0.04 : 0.062 * b);
            p.fillPath(ell(fx, -0.012, 0.038, 0.016), col);
        }
    }
    // tronco
    QPainterPath torso(QPointF(-0.028, -0.848));
    torso.quadTo(-SW * 0.62, -0.842, -SW, -0.792);
    torso.quadTo(-SW - 0.012, -0.69, -WA, -0.575);
    torso.quadTo(-HP - 0.008, -0.52, -HP, -0.455);
    torso.quadTo(0, -0.43, HP, -0.455);
    torso.quadTo(HP + 0.008, -0.52, WA, -0.575);
    torso.quadTo(SW + 0.012, -0.69, SW, -0.792);
    torso.quadTo(SW * 0.62, -0.842, 0.028, -0.848);
    torso.closeSubpath();
    if (o.pose == Fig::Bust) {
        p.save();
        p.setClipRect(QRectF(-1, -2, 2, 1.5), Qt::IntersectClip);
    }
    p.fillPath(torso, col);
    if (o.stripes.isValid()) {
        p.save();
        p.setClipPath(torso, Qt::IntersectClip);
        for (double yy = -0.8; yy < -0.46; yy += 0.06) p.fillRect(QRectF(-0.2, yy, 0.4, 0.024), o.stripes);
        p.restore();
    }
    // pescoço e cabeça
    p.fillRect(QRectF(-0.024, -0.885, 0.048, 0.05), col);
    p.save();
    p.translate(0.004 * side, -0.928);
    p.rotate(o.tilt * 180 / M_PI);
    p.fillPath(ell(0, 0, 0.056, 0.069), col);
    p.restore();
    // cabelo e chapéu
    QPainterPath hair;
    switch (o.hair) {
    case Fig::Short: hair = topHalf(0, -0.948, 0.06, 0.054); break;
    case Fig::Long:
        hair.moveTo(-0.062, -0.95);
        hair.quadTo(-0.08, -0.84, -0.07, -0.775);
        hair.lineTo(0.07, -0.775);
        hair.quadTo(0.08, -0.84, 0.062, -0.95);
        hair.quadTo(0, -1.02, -0.062, -0.95);
        hair.closeSubpath();
        break;
    case Fig::Bun: hair = topHalf(0, -0.95, 0.06, 0.052); hair.addEllipse(QPointF(0, -1.005), 0.03, 0.03); break;
    case Fig::Pony:
        hair = topHalf(0, -0.95, 0.06, 0.052);
        hair.moveTo(-0.04 * side, -0.97);
        hair.quadTo(-0.1 * side, -0.93, -0.085 * side, -0.84);
        hair.lineTo(-0.06 * side, -0.9);
        hair.closeSubpath();
        break;
    case Fig::Curly:
        for (int i = 0; i < 7; ++i) { const double a = M_PI + i / 6.0 * M_PI; hair.addEllipse(QPointF(std::cos(a) * 0.058, -0.94 + std::sin(a) * 0.06), 0.026, 0.026); }
        break;
    case Fig::Cap: hair = topHalf(0, -0.955, 0.06, 0.05); hair.addEllipse(QPointF(0.045 * side, -0.958), 0.055, 0.012); break;
    case Fig::Hat: hair.addEllipse(QPointF(0, -0.975), 0.105, 0.016); hair.addRect(QRectF(-0.058, -1.03, 0.116, 0.058)); break;
    }
    hair.setFillRule(Qt::WindingFill);
    p.fillPath(hair, col);
    if (o.band.isValid()) p.fillRect(QRectF(-0.058, -0.992, 0.116, 0.014), o.band);
    // braços: ombro -> cotovelo -> mão
    auto arm = [&](int sgn, double ex, double ey, double hx, double hy) {
        const double sx = sgn * (SW - 0.018), sy = -0.785;
        line(sx, sy, ex, ey, 0.05 * b);
        line(ex, ey, hx, hy, 0.04 * b);
        p.fillPath(ell(hx, hy, 0.024, 0.024), col);
    };
    for (int sgn : { -1, 1 }) {
        Fig::Arm act = Fig::Down;
        bool oar2 = false;
        if (sgn == side) act = o.arm;
        else if (o.arm == Fig::Oar) oar2 = true;
        else if (o.arm == Fig::Counter) act = Fig::Counter;
        const double sw = -walk * sgn;
        if (oar2) { arm(sgn, sgn * (SW + 0.04), -0.68, sgn * -0.02 + o.st * 0.03, -0.6 + o.st * 0.05); continue; }
        switch (act) {
        case Fig::Down: arm(sgn, sgn * (SW + 0.012) + sw * 0.03, -0.62, sgn * (SW + 0.004) + sw * 0.07, -0.47); break;
        case Fig::Up: arm(sgn, sgn * (SW + 0.05), -0.95, sgn * (SW - 0.01), -1.13); break;
        case Fig::Umbrella: arm(sgn, sgn * (SW + 0.03), -0.66, sgn * 0.04, -0.82); break;
        case Fig::Cup: { const double l = o.lift; arm(sgn, sgn * (SW + 0.035), lerp(-0.6, -0.68, l), sgn * lerp(0.1, 0.045, l), lerp(-0.62, -0.86, l)); break; }
        case Fig::Hold: arm(sgn, sgn * (SW + 0.03), -0.62, sgn * (SW + 0.06), -0.56); break;
        case Fig::Counter: arm(sgn, sgn * (SW + 0.004), -0.63, sgn * (SW - 0.035), -0.56); break;
        case Fig::Oar: arm(sgn, sgn * (SW + 0.08), -0.72 + o.st * 0.03, sgn * (0.14 + o.st * 0.05), -0.66 + o.st * 0.06); break;
        }
    }
    if (o.pose == Fig::Bust) p.restore();
    p.restore();
}

QPointF figureHand(double x, double y, double h, const Fig& o)
{
    const double SW = 0.116 * o.build;
    if (o.arm == Fig::Up) return QPointF(x + o.side * (SW - 0.01) * h, y - 1.13 * h);
    if (o.arm == Fig::Umbrella) return QPointF(x + o.side * 0.04 * h, y - 0.82 * h);
    if (o.arm == Fig::Hold) return QPointF(x + o.side * (SW + 0.06) * h, y - 0.56 * h);
    return QPointF(x, y);
}

} // namespace kit
} // namespace AnimBg
