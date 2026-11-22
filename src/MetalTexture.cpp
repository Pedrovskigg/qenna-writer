#include "MetalTexture.h"

#include <QCoreApplication>
#include <QHash>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QVector>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <random>

// Porte do metal.js do Weather Lab (C:\mira-writing\anim-lab), com as mesmas receitas.
namespace Metal {
namespace {

struct Rng {
    std::mt19937 g;
    explicit Rng(quint32 seed) : g(seed) {}
    double u() { return std::uniform_real_distribution<double>(0.0, 1.0)(g); }
    double operator()(double a, double b) { return a + (b - a) * u(); }
};

QColor alpha(const QColor& c, double a)
{
    QColor o = c;
    o.setAlphaF(std::clamp(a, 0.0, 1.0));
    return o;
}
QColor mixc(const QColor& a, const QColor& b, double k)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * k, a.greenF() + (b.greenF() - a.greenF()) * k,
                            a.blueF() + (b.blueF() - a.blueF()) * k);
}
const QColor kWhite(255, 255, 255), kBlack(0, 0, 0);

// Ruído cinza (128 ± amp), com um desvio por linha; esticado na horizontal vira o risco do escovado.
QImage noise(int w, int h, int amp, int rowBias, Rng& r)
{
    QImage img(qMax(1, w), qMax(1, h), QImage::Format_RGB32);
    for (int y = 0; y < img.height(); ++y) {
        const double rb = (r.u() - 0.5) * rowBias;
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const int v = std::clamp(int(128 + rb + (r.u() - 0.5) * amp), 0, 255);
            line[x] = qRgb(v, v, v);
        }
    }
    return img;
}

void overlayNoise(QPainter& p, double w, double h, const QImage& n, double opacity,
                  QPainter::CompositionMode mode = QPainter::CompositionMode_Overlay)
{
    p.save();
    p.setCompositionMode(mode);
    p.setOpacity(opacity);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(QRectF(0, 0, w, h), n);
    p.restore();
}

void brushed(QPainter& p, double w, double h, const QColor& base, double k, Rng& r)
{
    const QRectF all(0, 0, w, h);
    p.fillRect(all, base);
    // reflexo anisotrópico: o risco é horizontal, então a luz vira faixas verticais
    static const double wide[][2] = { { 0, -0.22 }, { 0.12, 0.2 }, { 0.24, 0 }, { 0.4, 0.12 }, { 0.52, -0.08 },
                                      { 0.66, 0.16 }, { 0.8, -0.04 }, { 0.92, 0.14 }, { 1, -0.18 } };
    static const double narrow[][2] = { { 0, -0.2 }, { 0.28, 0.22 }, { 0.5, 0.02 }, { 0.75, 0.14 }, { 1, -0.18 } };
    QLinearGradient g(0, 0, w, 0);
    if (w > 500) for (const auto& s : wide) g.setColorAt(s[0], s[1] > 0 ? alpha(kWhite, s[1] * k) : alpha(kBlack, -s[1] * k));
    else for (const auto& s : narrow) g.setColorAt(s[0], s[1] > 0 ? alpha(kWhite, s[1] * k) : alpha(kBlack, -s[1] * k));
    p.fillRect(all, g);
    QLinearGradient vg(0, 0, 0, h);
    vg.setColorAt(0, alpha(kWhite, 0.06 * k));
    vg.setColorAt(1, alpha(kBlack, 0.12 * k));
    p.fillRect(all, vg);
    overlayNoise(p, w, h, noise(qMax(6, qRound(w / 16)), qRound(h), 70, 50, r), 0.5 * k);
}

void rivet(QPainter& p, double x, double y, double rad)
{
    p.setPen(Qt::NoPen);
    p.setBrush(alpha(kBlack, 0.45));
    p.drawEllipse(QPointF(x + rad * 0.35, y + rad * 0.45), rad * 1.1, rad * 1.1);
    QRadialGradient g(QPointF(x, y), rad * 1.05, QPointF(x - rad * 0.4, y - rad * 0.45));
    g.setColorAt(0, alpha(kWhite, 0.75));
    g.setColorAt(0.35, alpha(QColor(0xaa, 0xb4, 0xbe), 0.55));
    g.setColorAt(1, alpha(QColor(0x10, 0x14, 0x18), 0.85));
    p.setBrush(g);
    p.drawEllipse(QPointF(x, y), rad, rad);
}

// Uma placa com chanfro opcional nos cantos (ch = sup-esq, sup-dir, inf-dir, inf-esq), bisel e rebites na borda.
void plateShape(QPainter& p, double x, double y, double w, double h, const double ch[4], double rivR, double rivStep, Rng& r)
{
    const QPointF pts[8] = { { x + ch[0], y }, { x + w - ch[1], y }, { x + w, y + ch[1] }, { x + w, y + h - ch[2] },
                             { x + w - ch[2], y + h }, { x + ch[3], y + h }, { x, y + h - ch[3] }, { x, y + ch[0] } };
    QPainterPath path;
    path.moveTo(pts[0]);
    for (int i = 1; i < 8; ++i) path.lineTo(pts[i]);
    path.closeSubpath();
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(alpha(kBlack, 0.55), 3));
    p.drawPath(path);
    p.save();
    p.setClipPath(path, Qt::IntersectClip);
    p.fillRect(QRectF(x, y, w, h), alpha(r.u() < 0.5 ? kWhite : kBlack, r(0.02, 0.07)));
    p.setPen(QPen(alpha(kWhite, 0.16), 2));
    p.drawPolyline(QVector<QPointF>{ pts[6], pts[7], pts[0], pts[1], pts[2] });
    p.setPen(QPen(alpha(kBlack, 0.4), 2));
    p.drawPolyline(QVector<QPointF>{ pts[2], pts[3], pts[4], pts[5], pts[6] });
    const int scratches = int(w * h / 30000) + 1;
    for (int i = 0; i < scratches; ++i) {
        p.setPen(QPen(alpha(kWhite, r(0.03, 0.07)), 0.7));
        const double sx = r(x, x + w), sy = r(y, y + h);
        QPainterPath s;
        s.moveTo(sx, sy);
        s.quadTo(sx + r(-30, 30), sy + r(-8, 8), sx + r(-60, 60), sy + r(-14, 14));
        p.drawPath(s);
    }
    p.restore();
    if (rivR <= 0) return;
    const double ins = rivR * 3.2;
    auto along = [&](double ax, double ay, double bx, double by) {
        const double L = std::hypot(bx - ax, by - ay);
        const int n = qMax(1, qRound(L / rivStep));
        for (int i = 0; i <= n; ++i) rivet(p, ax + (bx - ax) * i / n, ay + (by - ay) * i / n, rivR);
    };
    along(x + ins + ch[0] * 0.6, y + ins, x + w - ins - ch[1] * 0.6, y + ins);
    if (h > rivStep * 2.2) {
        along(x + ins, y + ins + rivStep, x + ins, y + h - ins - rivStep);
        along(x + w - ins, y + ins + rivStep, x + w - ins, y + h - ins - rivStep);
    }
    if (h > ins * 4) along(x + ins + ch[3] * 0.6, y + h - ins, x + w - ins - ch[2] * 0.6, y + h - ins);
}

void plates(QPainter& p, double w, double h, bool desk, Rng& r)
{
    if (desk) {
        // grade de placas grandes, desencontradas, com chanfro aqui e ali (cara de blindagem)
        double y = -20;
        while (y < h) {
            const double rh = r(130, 210);
            double x = -r(0, 120);
            while (x < w) {
                const double pw = r(170, 340);
                double ch[4];
                for (double& c : ch) c = r.u() < 0.3 ? r(14, 34) : 0;
                plateShape(p, x + 2, y + 2, pw - 4, rh - 4, ch, 3, 26, r);
                x += pw;
            }
            y += rh;
        }
        return;
    }
    // painel: uma placa por trecho, as emendas atravessando o lado comprido
    const bool wideP = w >= h;
    const double L = wideP ? w : h;
    const int n = qMax(1, qRound(L / 220));
    const double seg = L / n, cap = std::min(w, h) * 0.25;
    for (int i = 0; i < n; ++i) {
        double ch[4] = { i == 0 ? 8.0 : 0.0, wideP && i == n - 1 ? 8.0 : 0.0, i == n - 1 ? 8.0 : 0.0, !wideP && i == 0 ? 8.0 : 0.0 };
        for (double& c : ch) c = std::min(c, cap);
        if (wideP) plateShape(p, i * seg + 1.5, 1.5, seg - 3, h - 3, ch, h > 60 ? 2.2 : 0, 22, r);
        else plateShape(p, 1.5, i * seg + 1.5, w - 3, seg - 3, ch, w > 60 ? 2.2 : 0, 22, r);
    }
    // painel estreito (barra de cima, barra lateral): só um rebite em cada ponta
    if (std::min(w, h) <= 60) {
        const double m = std::min(w, h) / 2;
        if (wideP) { rivet(p, 8, m, 2); rivet(p, w - 8, m, 2); }
        else { rivet(p, m, 8, 2); rivet(p, m, h - 8, 2); }
    }
}

// Lascas na tinta (camada `pl`): primeiro a borda escura, depois o buraco que mostra o aço; e os riscos.
void chips(QPainter& pl, double x0, double y0, double w, double h, const QColor& base, double density, Rng& r)
{
    struct Blob { double x, y, rad; QVector<double> k; };
    QVector<Blob> blobs;
    auto add = [&](double x, double y, double rad) {
        Blob b{ x0 + x, y0 + y, rad, {} };
        for (int i = 0; i < 9; ++i) b.k << r(0.55, 1.1);
        blobs << b;
    };
    const double edge = 2 * (w + h);
    const int n = qRound(edge / 22 * density);
    for (int i = 0; i < n; ++i) {
        const double t = r.u() * edge;
        double x, y;
        if (t < w) { x = t; y = 0; }
        else if (t < w + h) { x = w; y = t - w; }
        else if (t < 2 * w + h) { x = t - w - h; y = h; }
        else { x = 0; y = t - 2 * w - h; }
        add(x + r(-1.5, 1.5), y + r(-1.5, 1.5), r.u() < 0.15 ? r(4, 7) : r(1.2, 3.5));
    }
    const double corners[4][2] = { { 0, 0 }, { w, 0 }, { 0, h }, { w, h } };
    for (const auto& c : corners) for (int i = 0; i < 3; ++i) add(c[0] + r(-8, 8), c[1] + r(-8, 8), r(3, 8));
    const int inner = qRound(w * h / 45000 * density);
    for (int i = 0; i < inner; ++i) add(r(0, w), r(0, h), r(0.8, 2.2));
    auto shape = [](const Blob& b, double scale) {
        QPainterPath s;
        for (int i = 0; i < 9; ++i) {
            const double a = i / 9.0 * 2 * M_PI, rr = b.rad * scale * b.k[i];
            const QPointF q(b.x + std::cos(a) * rr, b.y + std::sin(a) * rr);
            i ? s.lineTo(q) : s.moveTo(q);
        }
        s.closeSubpath();
        return s;
    };
    pl.save();
    pl.setClipRect(QRectF(x0, y0, w, h));
    pl.setPen(Qt::NoPen);
    pl.setBrush(mixc(base, kBlack, 0.4));
    for (const Blob& b : blobs) pl.drawPath(shape(b, 1.3));
    pl.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    pl.setBrush(kBlack);
    for (const Blob& b : blobs) pl.drawPath(shape(b, 1.0));
    const int scr = qRound(w * h / 9000 * density);
    for (int i = 0; i < scr; ++i) {
        QPen pen(alpha(kBlack, r(0.5, 0.9)), r(0.6, 1.3));
        pen.setCapStyle(Qt::RoundCap);
        pl.setPen(pen);
        const double sx = x0 + r(0, w), sy = y0 + r(0, h), a = r(-0.6, 0.6), len = r(10, 46);
        pl.drawLine(QPointF(sx, sy), QPointF(sx + std::cos(a) * len, sy + std::sin(a) * len));
    }
    pl.restore();
}

void paintCoat(QImage& img, double w, double h, qreal dpr, const QColor& base, bool desk, Rng& r)
{
    QImage coat(img.size(), QImage::Format_ARGB32_Premultiplied);
    coat.setDevicePixelRatio(dpr);
    coat.fill(Qt::transparent);
    QPainter x(&coat);
    x.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient g(0, 0, 0, h);
    g.setColorAt(0, mixc(base, kWhite, 0.1));
    g.setColorAt(0.5, base);
    g.setColorAt(1, mixc(base, kBlack, 0.25));
    x.fillRect(QRectF(0, 0, w, h), g);
    overlayNoise(x, w, h, noise(qRound(w / 3), qRound(h / 3), 26, 0, r), 0.35);
    QLinearGradient gl(0, 0, w, h);
    gl.setColorAt(0.3, alpha(kWhite, 0));
    gl.setColorAt(0.42, alpha(kWhite, 0.1));
    gl.setColorAt(0.5, alpha(kWhite, 0));
    x.fillRect(QRectF(0, 0, w, h), gl);
    QVector<double> seams;
    if (desk) {
        // na mesa, a tinta é em chapas: cada emenda lasca
        double y = 0;
        while (y < h) {
            const double rh = r(150, 230);
            double xx = -r(0, 100);
            while (xx < w) { const double pw = r(200, 360); chips(x, xx, y, pw, rh, base, 0.7, r); xx += pw; }
            y += rh;
            seams << y;
        }
    } else {
        chips(x, 0, 0, w, h, base, 1.0, r);
    }
    x.end();
    QPainter p(&img);
    p.drawImage(QRectF(0, 0, w, h), coat);
    for (double y : seams) p.fillRect(QRectF(0, y - 1, w, 2.5), alpha(kBlack, 0.45));
}

void chrome(QPainter& p, double w, double h, const QColor& b, bool desk, Rng& r)
{
    const bool tall = h > w * 1.8;
    QLinearGradient g = tall ? QLinearGradient(0, 0, w, 0) : QLinearGradient(0, 0, 0, h);
    g.setColorAt(0, mixc(b, kWhite, 0.4));
    g.setColorAt(0.3, mixc(b, kWhite, 0.18));
    g.setColorAt(0.47, mixc(b, kWhite, 0.07));
    g.setColorAt(0.5, mixc(b, kBlack, 0.62));
    g.setColorAt(0.56, mixc(b, kBlack, 0.35));
    g.setColorAt(0.78, mixc(b, kWhite, 0.06));
    g.setColorAt(1, mixc(b, kBlack, 0.45));
    p.fillRect(QRectF(0, 0, w, h), g);
    p.save();
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    for (const auto& [o, a] : { std::pair<double, double>{ 0.18, 0.1 }, { 0.62, 0.07 } }) {
        QLinearGradient dg(0, 0, w, h * 0.6);
        dg.setColorAt(std::max(0.0, o - 0.06), alpha(kWhite, 0));
        dg.setColorAt(o, alpha(kWhite, a));
        dg.setColorAt(std::min(1.0, o + 0.06), alpha(kWhite, 0));
        p.fillRect(QRectF(0, 0, w, h), dg);
    }
    p.restore();
    overlayNoise(p, w, h, noise(qRound(w / 4), qRound(h / 4), 14, 0, r), 0.25);
    if (desk) {
        for (double xx = 240; xx < w; xx += 240) {
            p.fillRect(QRectF(xx, 0, 2, h), alpha(kBlack, 0.45));
            p.fillRect(QRectF(xx + 2, 0, 1, h), alpha(kWhite, 0.18));
        }
    }
    p.fillRect(QRectF(0, 0, w, 1), alpha(kWhite, 0.4));
    p.fillRect(QRectF(0, h - 1, w, 1), alpha(kBlack, 0.5));
}

// Queima do titânio: modo "color" (matiz e saturação da faixa, luminância do aço), e depois soft-light.
void heatTint(QImage& img, double w, double h, qreal dpr, bool desk)
{
    QImage band(img.size(), QImage::Format_ARGB32_Premultiplied);
    band.setDevicePixelRatio(dpr);
    {
        QPainter b(&band);
        const bool wideP = w >= h;
        QLinearGradient g = wideP ? QLinearGradient(0, 0, w, 0) : QLinearGradient(0, 0, 0, h);
        static const std::pair<double, QRgb> stops[] = {
            { 0, 0x808080 }, { 0.12, 0xc8a24a }, { 0.22, 0xb06a2a }, { 0.32, 0x7a3a8a }, { 0.42, 0x2a5ab8 }, { 0.5, 0x4a7ac8 },
            { 0.58, 0x2a5ab8 }, { 0.68, 0x7a3a8a }, { 0.78, 0xb06a2a }, { 0.88, 0xc8a24a }, { 1, 0x808080 } };
        for (const auto& [o, c] : stops) g.setColorAt(o, QColor::fromRgb(c));
        b.fillRect(QRectF(0, 0, w, h), g);
    }
    const double a = desk ? 0.3 : 0.36;
    auto lum = [](double r, double g, double b) { return 0.3 * r + 0.59 * g + 0.11 * b; };
    for (int y = 0; y < img.height(); ++y) {
        auto* d = reinterpret_cast<QRgb*>(img.scanLine(y));
        const auto* s = reinterpret_cast<const QRgb*>(band.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const double br = qRed(d[x]), bg = qGreen(d[x]), bb = qBlue(d[x]);
            double cr = qRed(s[x]), cg = qGreen(s[x]), cb = qBlue(s[x]);
            const double delta = lum(br, bg, bb) - lum(cr, cg, cb);
            cr += delta; cg += delta; cb += delta;
            const double l = lum(cr, cg, cb), n = std::min({ cr, cg, cb }), m = std::max({ cr, cg, cb });
            if (n < 0) { cr = l + (cr - l) * l / (l - n); cg = l + (cg - l) * l / (l - n); cb = l + (cb - l) * l / (l - n); }
            if (m > 255) { cr = l + (cr - l) * (255 - l) / (m - l); cg = l + (cg - l) * (255 - l) / (m - l); cb = l + (cb - l) * (255 - l) / (m - l); }
            d[x] = qRgba(std::clamp(int(br + (cr - br) * a), 0, 255), std::clamp(int(bg + (cg - bg) * a), 0, 255),
                         std::clamp(int(bb + (cb - bb) * a), 0, 255), qAlpha(d[x]));
        }
    }
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_SoftLight);
    p.setOpacity(0.28);
    p.drawImage(QRectF(0, 0, w, h), band);
}

QImage build(const QString& kind, const QColor& base, const QSize& size, qreal dpr, bool desk, quint32 seed)
{
    const double w = size.width(), h = size.height();
    QImage img(QSize(qMax(1, qRound(w * dpr)), qMax(1, qRound(h * dpr))), QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(base);
    Rng r(seed);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        if (kind == QLatin1String("brushed")) brushed(p, w, h, base, 1.0, r);
        else if (kind == QLatin1String("plate")) { brushed(p, w, h, base, 0.55, r); plates(p, w, h, desk, r); }
        else if (kind == QLatin1String("paint")) brushed(p, w, h, desk ? QColor(0x6a, 0x70, 0x78) : QColor(0x90, 0x98, 0xa0), 0.8, r);
        else if (kind == QLatin1String("chrome")) chrome(p, w, h, base, desk, r);
        else if (kind == QLatin1String("heat")) brushed(p, w, h, base, 0.9, r);
    }
    if (kind == QLatin1String("paint")) paintCoat(img, w, h, dpr, base, desk, r);
    else if (kind == QLatin1String("heat")) heatTint(img, w, h, dpr, desk);
    return img;
}

} // namespace

QStringList kinds()
{
    return { QStringLiteral("brushed"), QStringLiteral("plate"), QStringLiteral("paint"), QStringLiteral("chrome"),
             QStringLiteral("heat") };
}

bool isValid(const QString& kind) { return kinds().contains(kind); }

QString label(const QString& kind)
{
    if (kind == QLatin1String("brushed")) return QCoreApplication::translate("Metal", "Aço escovado");
    if (kind == QLatin1String("plate"))   return QCoreApplication::translate("Metal", "Chapa de blindagem");
    if (kind == QLatin1String("paint"))   return QCoreApplication::translate("Metal", "Chapa pintada gasta");
    if (kind == QLatin1String("chrome"))  return QCoreApplication::translate("Metal", "Cromo");
    if (kind == QLatin1String("heat"))    return QCoreApplication::translate("Metal", "Titânio queimado");
    return QCoreApplication::translate("Metal", "Nenhum (cor lisa)");
}

QImage texture(const QString& kind, const QColor& base, const QSize& size, qreal dpr, bool desk)
{
    if (!isValid(kind) || size.width() < 2 || size.height() < 2) return {};
    static QHash<QString, QImage> cache;
    const QString key = QStringLiteral("%1|%2|%3x%4|%5|%6").arg(kind, base.name(QColor::HexArgb)).arg(size.width()).arg(size.height())
                            .arg(dpr, 0, 'f', 2).arg(desk ? 1 : 0);
    auto it = cache.constFind(key);
    if (it != cache.constEnd()) return *it;
    if (cache.size() > 40) cache.clear();
    // Semente fixa por material e tamanho: a mesma chapa sempre que o painel volta pro mesmo tamanho.
    const QImage img = build(kind, base, size, dpr, desk, quint32(qHash(key.section(QLatin1Char('|'), 0, 2))));
    cache.insert(key, img);
    return img;
}

} // namespace Metal
