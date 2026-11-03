#include "GalaxyCanvas.h"

#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>

namespace galaxy {

namespace {
QColor cssOf(const Rgb& c, double a = 1) { return QColor(std::clamp(c[0], 0, 255), std::clamp(c[1], 0, 255), std::clamp(c[2], 0, 255), int(clamp(a, 0, 1) * 255 + 0.5)); }
Rgb mul(const Rgb& c, double k) { return { int(c[0] * k), int(c[1] * k), int(c[2] * k) }; }
Rgb mixRgb(const Rgb& a, const Rgb& b, double t) { return { int(a[0] + (b[0] - a[0]) * t), int(a[1] + (b[1] - a[1]) * t), int(a[2] + (b[2] - a[2]) * t) }; }
Rgb typeCol(Type t)
{
    switch (t) {
    case Type::Temperado: return { 70, 150, 110 }; case Type::Oceano: return { 50, 110, 180 }; case Type::Deserto: return { 200, 150, 90 }; case Type::Arido: return { 140, 132, 124 };
    case Type::Lava: return { 210, 90, 50 }; case Type::Gelado: return { 205, 222, 234 }; case Type::Toxico: return { 200, 178, 76 }; case Type::Anao: return { 166, 156, 146 };
    case Type::Gigante: return { 202, 165, 122 }; case Type::Quente: return { 196, 106, 64 }; case Type::GeloG: return { 111, 179, 204 }; case Type::Subnetuno: return { 147, 166, 204 };
    case Type::Rochosa: return { 154, 148, 142 }; case Type::Gelada: return { 214, 228, 236 }; case Type::Vulcanica: return { 220, 190, 90 }; case Type::Temperada: return { 70, 150, 110 };
    default: return { 150, 150, 150 };
    }
}
} // namespace

Canvas::ArtAssets& Canvas::artAssets()
{
    const qint64 gk = doc->galImg.cacheKey();
    if (m_art.galKey != gk) { m_art.field = densityField(doc->galImg); m_art.galKey = gk; m_art.per.clear(); m_art.key.clear(); }
    auto it = m_art.per.find(m_sk->id);
    if (it != m_art.per.end()) return *it.value();
    auto A = std::make_shared<ArtAssets>();
    const uint32_t seed = doc->gal->seed;
    A->wash = washImage(m_art.field, *m_sk, seed, doc->galImg);
    A->curves = contours(m_art.field, m_sk->curves);
    if (m_sk->stipN > 0) { A->stip = stipple(m_art.field, *m_sk, seed); A->hasStip = true; }
    A->tile = paperTile(*m_sk);
    m_art.per.insert(m_sk->id, A);
    return *A;
}
// O papel e a galáxia ficam numa camada só, refeita quando a câmera mexe.
void Canvas::drawArtBackdrop(QPainter& p, double zoomK)
{
    const QPointF vc = viewCenter();
    const int W = width(), H = height();
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9").arg(cam.x, 0, 'g', 12).arg(cam.y, 0, 'g', 12).arg(cam.z, 0, 'g', 12).arg(cam.h, 0, 'g', 10).arg(cam.tilt, 0, 'g', 10).arg(cam.yaw, 0, 'g', 10).arg(vc.x()).arg(vc.y()).arg(m_sk->id + QString::number(m_frame.width()) + QLatin1Char('x') + QString::number(m_frame.height()) + QString::number(doc->galImg.cacheKey()) + (m_sk->paint != SpacePaint::None ? spaceRegionKey() : QString()));
    if (key != m_art.key) {
        ArtAssets& A = artAssets();
        m_art.key = key;
        if (m_art.layer.size() != m_frame.size()) m_art.layer = QImage(m_frame.size(), QImage::Format_RGB32);
        m_art.layer.setDevicePixelRatio(1);
        QPainter g(&m_art.layer);
        g.setRenderHint(QPainter::Antialiasing, true);
        g.setRenderHint(QPainter::SmoothPixmapTransform, true);
        g.scale(m_dpr, m_dpr);
        g.fillRect(QRectF(0, 0, W, H), QBrush(A.tile));
        double ox, oy, ex0, ex1, ey0, ey1;
        proj(0, 0, 0, ox, oy); proj(1, 0, 0, ex0, ex1); proj(0, 1, 0, ey0, ey1);
        const double a = ex0 - ox, b = ey0 - ox, cc = ex1 - oy, d = ey1 - oy, E = EXT;
        if (!A.wash.isNull()) {
            const double T = A.wash.width();
            double p0x, p0y, p1x, p1y, p2x, p2y;
            proj(-E, E, 0, p0x, p0y); proj(E, E, 0, p1x, p1y); proj(-E, -E, 0, p2x, p2y);
            g.save();
            g.setOpacity(1 - smooth(5, 28, zoomK) * 0.9);
            g.setTransform(QTransform(m_dpr * (p1x - p0x) / T, m_dpr * (p1y - p0y) / T, m_dpr * (p2x - p0x) / T, m_dpr * (p2y - p0y) / T, m_dpr * p0x, m_dpr * p0y));
            g.drawImage(0, 0, A.wash);
            g.restore();
        }
        if (m_sk->graticule) drawGraticule(g, a, b, cc, d, ox, oy, zoomK);
        if (m_sk->chart) drawRhumbs(g, a, b, cc, d, ox, oy, zoomK);
        const double fadeC = 1 - smooth(5, 20, zoomK);
        if (fadeC > 0.02) {
            for (const CurveSegs& c : A.curves) {
                QPen pen(m_sk->inkC(c.L.alpha * fadeC));
                pen.setWidthF(c.L.width); pen.setCapStyle(Qt::RoundCap);
                if (c.L.dashed) pen.setDashPattern({ 2 / c.L.width, 4 / c.L.width });
                g.setPen(pen);
                QVector<QLineF> lines;
                lines.reserve(int(c.seg.size() / 4));
                for (size_t i = 0; i + 3 < c.seg.size(); i += 4) {
                    const double x0 = a * c.seg[i] + b * c.seg[i + 1] + ox, y0 = cc * c.seg[i] + d * c.seg[i + 1] + oy, x1 = a * c.seg[i + 2] + b * c.seg[i + 3] + ox, y1 = cc * c.seg[i + 2] + d * c.seg[i + 3] + oy;
                    if ((x0 < 0 && x1 < 0) || (y0 < 0 && y1 < 0) || (x0 > W && x1 > W) || (y0 > H && y1 > H)) continue;
                    lines.push_back(QLineF(x0, y0, x1, y1));
                }
                g.drawLines(lines);
            }
        }
        if (A.hasStip) {
            const double k = 1 - smooth(4, 24, zoomK);
            if (k > 0.02) {
                const QColor col = m_sk->inkC(m_sk->stipA * k);
                const double s = 0.95 + std::min(0.5, zoomK * 0.06);
                g.save();
                g.setRenderHint(QPainter::Antialiasing, false);
                for (int i = 0; i < A.stip.n; i++) {
                    const double X = A.stip.xs[size_t(i)], Y = A.stip.ys[size_t(i)], x = a * X + b * Y + ox, y = cc * X + d * Y + oy;
                    if (x >= 0 && y >= 0 && x <= W && y <= H) g.fillRect(QRectF(x, y, s, s), col);
                }
                g.restore();
            }
        }
        // As estrelas de perto viram pontinhos de pena.
        const double fade = smooth(1.4, 3, zoomK);
        if (fade > 0) {
            const DetailStars& D = doc->gal->detail;
            g.save();
            g.setRenderHint(QPainter::Antialiasing, false);
            for (int i = 0; i < D.n; i++) {
                double x, y;
                proj(D.xs[size_t(i)], D.ys[size_t(i)], 0, x, y);
                if (x < -2 || y < -2 || x > W + 2 || y > H + 2) continue;
                const double sz = 0.8 + D.bs[size_t(i)] * 0.45;
                g.fillRect(QRectF(x - sz / 2, y - sz / 2, sz, sz), m_sk->inkC(fade * (0.2 + D.bs[size_t(i)] * 0.2)));
            }
            g.restore();
        }
        if (m_sk->paint != SpacePaint::None) paintSpace(g, a, b, cc, d, ox, oy, zoomK);
        if (m_sk->vignette) {
            QRadialGradient gr(W / 2.0, H / 2.0, std::hypot(W, H) * 0.56);
            const double t0 = (std::min(W, H) * 0.42) / (std::hypot(W, H) * 0.56);
            gr.setColorAt(0, QColor(90, 56, 24, 0)); gr.setColorAt(t0, QColor(90, 56, 24, 0)); gr.setColorAt(1, QColor(90, 56, 24, 77));
            g.fillRect(QRectF(0, 0, W, H), gr);
        }
    }
    p.drawImage(QRectF(0, 0, W, H), m_art.layer);
}
// A quadrícula presa ao mapa: o passo muda com o zoom, sempre em dobros.
void Canvas::drawGraticule(QPainter& g, double a, double b, double cc, double d, double ox, double oy, double)
{
    const double E = EXT * 1.6, px = std::hypot(a, cc);
    double st = 0.125 * EXT;
    while (st * px > 150) st /= 2;
    while (st * px < 42) st *= 2;
    const auto line = [&](QVector<QLineF>& out, double x0, double y0, double x1, double y1) { out.push_back(QLineF(a * x0 + b * y0 + ox, cc * x0 + d * y0 + oy, a * x1 + b * y1 + ox, cc * x1 + d * y1 + oy)); };
    double cx, cy;
    toWorld(width() / 2.0, height() / 2.0, cx, cy);
    const double R = std::min(E * 1.4, (std::hypot(width(), height()) / px) * (cam.tilt > 0.2 ? 2.2 : 1));
    const int x0 = int(std::floor((cx - R) / st)), x1 = int(std::ceil((cx + R) / st)), y0 = int(std::floor((cy - R) / st)), y1 = int(std::ceil((cy + R) / st));
    for (const bool big : { false, true }) {
        QPen pen(m_sk->inkC(big ? 0.24 : 0.1));
        pen.setWidthF(big ? 0.9 : 0.6);
        g.setPen(pen);
        QVector<QLineF> lines;
        for (int i = x0; i <= x1; i++) if ((i % 4 == 0) == big) line(lines, i * st, y0 * st, i * st, y1 * st);
        for (int j = y0; j <= y1; j++) if ((j % 4 == 0) == big) line(lines, x0 * st, j * st, x1 * st, j * st);
        g.drawLines(lines);
    }
}
// A carta de navegador: rosas dos ventos e as linhas de rumo saindo delas.
void Canvas::drawRhumbs(QPainter& g, double a, double b, double cc, double d, double ox, double oy, double zoomK)
{
    const double E = EXT, k = 1 - smooth(4, 14, zoomK);
    if (k < 0.02) return;
    const auto P = [&](double x, double y) { return QPointF(a * x + b * y + ox, cc * x + d * y + oy); };
    std::vector<QPointF> nodes { QPointF(0, 0) };
    for (int i = 0; i < 8; i++) nodes.push_back(QPointF(std::cos((i * TAU) / 8 + 0.39) * E * 0.78, std::sin((i * TAU) / 8 + 0.39) * E * 0.78));
    g.save();
    for (const QPointF& n : nodes) {
        QPen pen(m_sk->selC(0.2 * k)); pen.setWidthF(0.6);
        g.setPen(pen);
        for (int i = 0; i < 16; i++) {
            const double t = (i * TAU) / 16;
            g.drawLine(P(n.x(), n.y()), P(n.x() + std::cos(t) * E * 2.2, n.y() + std::sin(t) * E * 2.2));
        }
    }
    // Duas rosas: uma grande, outra menor, no vazio entre os braços.
    const double roses[2][3] = { { E * 1.2, -E * 0.42, E * 0.14 }, { -E * 1.2, E * 0.48, E * 0.09 } };
    g.setOpacity(k);
    for (const auto& ro : roses) {
        const double nx = ro[0], ny = ro[1], S = ro[2];
        const auto pt = [&](double t, double q) { return P(nx + std::cos(t) * S * q, ny + std::sin(t) * S * q); };
        for (int i = 0; i < 8; i++) {
            const double t = (i * TAU) / 8, w = TAU / 16;
            const bool lng = i % 2 == 0;
            const double L = lng ? 1 : 0.62;
            const QPointF tip = pt(t, L), l = pt(t - w, 0.2), r = pt(t + w, 0.2), m = pt(t, 0);
            g.setPen(Qt::NoPen);
            g.setBrush(lng ? m_sk->inkC() : m_sk->selC());
            QPolygonF t1; t1 << m << l << tip; g.drawPolygon(t1);
            g.setBrush(m_sk->paperC());
            QPolygonF t2; t2 << m << r << tip; g.drawPolygon(t2);
            QPen pen(m_sk->inkC()); pen.setWidthF(0.7);
            g.setPen(pen); g.setBrush(Qt::NoBrush);
            QPolygonF t3; t3 << l << tip << r; g.drawPolyline(t3);
        }
        QPen pen(m_sk->inkC()); pen.setWidthF(0.8);
        g.setPen(pen); g.setBrush(Qt::NoBrush);
        QPolygonF cir;
        for (int i = 0; i <= 48; i++) cir << pt((i * TAU) / 48, 1.08);
        g.drawPolyline(cir);
    }
    g.restore();
}
// A estrela de longe: um sinal de atlas, que diz o tipo.
void Canvas::drawArtStarFar(QPainter& p, const System& s, double x, double y)
{
    if (m_sk->sign != StarSign::Atlas) { drawSpaceStar(p, s, x, y); return; }
    const Star& st = s.stars[0];
    const QColor fill = cssOf(m_sk->colored ? mul(sysColor(s), 0.72) : m_sk->ink);
    const double r = s.isCore ? 5.5 : 1.6 + std::min(1.8, (st.radius != 0 ? st.radius : 1) * 0.42);
    p.setPen(Qt::NoPen); p.setBrush(m_sk->haloC());
    p.drawEllipse(QPointF(x, y), r + 1.4, r + 1.4);
    QPen pen(fill); pen.setWidthF(1);
    if (s.isCore || st.kind == StarKind::BuracoNegro) {
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(x, y), r, r);
        p.setPen(Qt::NoPen); p.setBrush(fill);
        p.drawEllipse(QPointF(x, y), r * 0.45, r * 0.45);
        if (s.isCore) {
            p.save(); p.translate(x, y); p.rotate(-0.3 * 180 / PI_D);
            p.setPen(pen); p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(0, 0), r * 1.9, r * 0.7);
            p.restore();
        }
    } else if (st.kind == StarKind::Neutrons) {
        p.setPen(pen);
        p.drawLine(QPointF(x - r - 2, y), QPointF(x + r + 2, y));
        p.drawLine(QPointF(x, y - r - 2), QPointF(x, y + r + 2));
    } else if (s.kind == StarKind::Binaria) {
        p.setPen(Qt::NoPen); p.setBrush(fill);
        p.drawEllipse(QPointF(x - r * 0.7, y), r * 0.72, r * 0.72);
        p.drawEllipse(QPointF(x + r * 0.7, y), r * 0.72, r * 0.72);
    } else {
        p.setPen(Qt::NoPen); p.setBrush(fill);
        p.drawEllipse(QPointF(x, y), r, r);
    }
    // Mundo vivo ganha um aro: dá pra achar de longe onde mora gente.
    if (livingCount(s) && !s.isCore) {
        QPen hz(m_sk->hzC()); hz.setWidthF(1);
        p.setPen(hz); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(x, y), r + 3.2, r + 3.2);
    }
}
// A estrela de perto: sol de gravura, com raios.
void Canvas::artSun(QPainter& p, double x, double y, double r, const std::array<int, 3>& c)
{
    const Rgb col = m_sk->colored ? mul(c, 0.86) : m_sk->ink;
    const int rays = std::max(12, std::min(44, int(std::floor(r * 1.1 + 0.5))));
    const double lw = std::max(0.8, std::min(1.5, r * 0.04));
    QPen pen(cssOf(col, 0.9)); pen.setWidthF(lw);
    p.setPen(pen);
    for (int i = 0; i < rays; i++) {
        const double t = (i * TAU) / rays, L = i % 2 ? 1.55 : 1.3;
        p.drawLine(QPointF(x + std::cos(t) * r * 1.12, y + std::sin(t) * r * 1.12), QPointF(x + std::cos(t) * r * L, y + std::sin(t) * r * L));
    }
    p.setPen(Qt::NoPen);
    if (m_sk->hasSun) p.setBrush(cssOf(m_sk->sun));
    else if (m_sk->colored) {
        QRadialGradient gr(x, y, r);
        gr.setColorAt(0, cssOf(mixRgb(c, { 255, 255, 255 }, 0.6))); gr.setColorAt(1, cssOf(mixRgb(c, m_sk->paper, 0.2)));
        p.setBrush(gr);
    } else p.setBrush(m_sk->darkPaper ? cssOf(m_sk->dark) : m_sk->paperC());
    p.drawEllipse(QPointF(x, y), r, r);
    QPen edge(cssOf(col)); edge.setWidthF(lw);
    p.setPen(edge); p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(x, y), r, r);
    if (!m_sk->colored) {
        QPen in(m_sk->inkC(0.4)); in.setWidthF(0.7);
        p.setPen(in);
        p.drawEllipse(QPointF(x, y), r * 0.72, r * 0.72);
        p.drawEllipse(QPointF(x, y), r * 0.42, r * 0.42);
    }
}
void Canvas::drawArtStarOpen(QPainter& p, const System& s, double x, double y, double sr, double cT)
{
    const Star& st = s.stars[0];
    const auto c = sysColor(s);
    const Rgb inkCol = m_sk->colored ? mul(c, 0.7) : m_sk->ink;
    const double simT = doc->simT;
    if (s.isCore || st.kind == StarKind::BuracoNegro) {
        // Buraco negro: o disco em linhas, achatado com a inclinação, e o miolo cheio.
        const double R = sr * (s.isCore ? 1 : 0.8), ky = std::max(0.22, cT);
        QPen pen(cssOf(inkCol, 0.75)); pen.setWidthF(0.8);
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        for (int i = 0; i < 6; i++) { const double q = R * (0.95 + i * 0.36); p.drawEllipse(QPointF(x, y), q, q * ky); }
        p.setPen(Qt::NoPen); p.setBrush(m_sk->space ? m_sk->paperC() : m_sk->darkPaper ? cssOf(m_sk->ink) : cssOf(inkCol));
        p.drawEllipse(QPointF(x, y), R * 0.62, R * 0.62);
        if (m_sk->space) { QPen rim(cssOf(inkCol)); rim.setWidthF(1.1); p.setPen(rim); p.setBrush(Qt::NoBrush); p.drawEllipse(QPointF(x, y), R * 0.62, R * 0.62); }
        return;
    }
    if (s.kind == StarKind::Binaria && s.stars.size() > 1) {
        const double a = simT * SPEED_BINARY, d = sr * 0.9;
        for (size_t i = 0; i < s.stars.size(); i++) artSun(p, x + std::cos(a + double(i) * PI_D) * d, y + std::sin(a + double(i) * PI_D) * d * cT, sr * (i ? 0.45 : 0.6), s.stars[i].color);
        return;
    }
    artSun(p, x, y, sr * 0.72, c);
    if (st.kind == StarKind::Neutrons && st.pulsar) {
        const double a = simT * SPEED_PULSAR, L = sr * 9;
        QPen pen(cssOf(inkCol, 0.65)); pen.setWidthF(1); pen.setDashPattern({ 5, 4 });
        p.setPen(pen);
        p.drawLine(QPointF(x - std::cos(a) * L, y - std::sin(a) * L * cT), QPointF(x + std::cos(a) * L, y + std::sin(a) * L * cT));
    }
    if (st.kind == StarKind::AnaBranca) {
        QPen pen(cssOf(inkCol, 0.4)); pen.setWidthF(0.8); pen.setDashPattern({ 2 / 0.8, 4 / 0.8 });
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(x, y), sr * 3.4, sr * 3.4 * std::max(0.3, cT));
    }
}
// Na térmica, de 40 K (-233 °C) a 1200 K (+927 °C), em escala de razão.
double Canvas::heatOf(const Body& b) const
{
    if (!m_sk || !m_sk->thermal) return -1;
    const double T = b.temp != 0 ? b.temp : b.teq != 0 ? b.teq : 150;
    return std::max(0.05, std::min(1.0, std::log(T / 40) / std::log(30.0)));
}
// O corpo: a textura de verdade do planeta, passada pela técnica da vista.
const QImage* Canvas::artThumb(const Body& b, int D)
{
    const QImage* base = thumb(b, D);
    if (!base) return nullptr;
    const QString lk = texKey(b) + QLatin1Char(':') + QString::number(D);
    const int gen = m_thumbs.value(lk).gen;
    const QString k = m_sk->id + QLatin1Char(':') + lk;
    auto it = m_artThumbs.find(k);
    if (it != m_artThumbs.end() && it->gen == gen) return &it->img;
    QImage rgba = base->convertToFormat(QImage::Format_RGBA8888);
    stylize(rgba.bits(), D, D, *m_sk, false, 0, heatOf(b));
    ArtThumb e;
    e.img = rgba.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    e.gen = gen;
    return &m_artThumbs.insert(k, e)->img;
}
void Canvas::drawArtBody(QPainter& p, const Body& b, double x, double y, double r, double ux, double uy)
{
    const QColor edge = m_sk->soft ? m_sk->inkC(0.45) : m_sk->inkC();
    const QColor flat = m_sk->colored ? cssOf(typeCol(b.type)) : m_sk->darkPaper ? cssOf(m_sk->dark) : m_sk->paperC();
    if (r < 4) {
        QPen pen(edge); pen.setWidthF(0.8);
        p.setPen(pen); p.setBrush(flat);
        p.drawEllipse(QPointF(x, y), r, r);
        return;
    }
    QPen pen(edge); pen.setWidthF(std::max(0.9, std::min(1.6, r * 0.02)));
    if (r > 70 && m_liveBudget > 0 && x + r > 0 && x - r < width() && y + r > 0 && y - r < height()) {
        m_liveBudget--;
        drawLive(p, b, x, y, r, ux, uy);
        m_liveHits.push_back({ const_cast<Body*>(&b), nullptr, x, y, r });
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(x, y), r, r);
        return;
    }
    const double want = std::min(512.0, r * 2 * std::min(m_dpr, 1.5));
    int D = 512;
    for (int v : { 16, 32, 64, 128, 256, 512 }) if (v >= want) { D = v; break; }
    const QImage* img = artThumb(b, D);
    if (img) p.drawImage(QRectF(x - r, y - r, r * 2, r * 2), *img);
    else { p.setPen(Qt::NoPen); p.setBrush(m_sk->colored ? cssOf(typeCol(b.type)) : m_sk->paperC()); p.drawEllipse(QPointF(x, y), r, r); }
    if (m_sk->shadeNone) { /* a própria técnica já é a sombra */ }
    else if (m_sk->hatch) {
        // A sombra em hachura: cordas paralelas no lado de lá da estrela, cada vez mais juntas.
        const double vx = -uy, vy = ux;
        const int n = std::min(26, int(std::floor(r / 1.6)));
        QPen hp(m_sk->darkPaper ? cssOf(m_sk->dark, 0.9) : m_sk->inkC(0.82)); hp.setWidthF(std::max(0.7, std::min(1.3, r * 0.018)));
        p.setPen(hp);
        for (int i = 0; i < n; i++) {
            const double q = 0.08 + 0.92 * std::pow(double(i) / n, 0.8), t = r * q, L = std::sqrt(std::max(0.0, r * r - t * t)), cx = x - ux * t, cy = y - uy * t;
            p.drawLine(QPointF(cx - vx * L, cy - vy * L), QPointF(cx + vx * L, cy + vy * L));
        }
    } else {
        const Rgb S = m_sk->hasShadow ? m_sk->shadow : m_sk->soft ? Rgb { 66, 62, 100 } : Rgb { 60, 44, 34 };
        QLinearGradient g(x + ux * r, y + uy * r, x - ux * r, y - uy * r);
        g.setColorAt(0, cssOf(S, 0)); g.setColorAt(0.5, cssOf(S, 0.04)); g.setColorAt(1, cssOf(S, m_sk->soft ? 0.5 : 0.62));
        p.setPen(Qt::NoPen); p.setBrush(g);
        p.drawEllipse(QPointF(x, y), r + 0.5, r + 0.5);
    }
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(x, y), r, r);
}
// O que é desenhado direto na tela (colossos, anel, elevador) passa pela técnica numa folha à parte.
// A folha vale por um instante e só uma é refeita por quadro; entre uma e outra, a de antes acompanha a câmera.
void Canvas::toned(QPainter& p, const QString& id, double x, double y, double R, const std::function<void(QPainter&)>& fn)
{
    if (!m_sk) { fn(p); return; }
    const int x0 = std::max(0, int(std::floor(x - R))), y0 = std::max(0, int(std::floor(y - R))), x1 = std::min(width(), int(std::ceil(x + R))), y1 = std::min(height(), int(std::ceil(y + R)));
    if (x1 <= x0 || y1 <= y0) return;
    const qint64 now = m_clock.elapsed();
    auto it = m_toned.find(id);
    if (it == m_toned.end() || it->sk != m_sk->id || (now - it->at > 120 && m_art.budget > 0)) {
        m_art.budget--;
        const int w = int(std::ceil((x1 - x0) * m_dpr)), h = int(std::ceil((y1 - y0) * m_dpr));
        QImage img(w, h, QImage::Format_RGBA8888_Premultiplied);
        img.fill(Qt::transparent);
        {
            QPainter g(&img);
            g.setRenderHint(QPainter::Antialiasing, true);
            g.setRenderHint(QPainter::SmoothPixmapTransform, true);
            g.setTransform(QTransform(m_dpr, 0, 0, m_dpr, -x0 * m_dpr, -y0 * m_dpr));
            fn(g);
        }
        QImage straight = img.convertToFormat(QImage::Format_RGBA8888);
        stylize(straight.bits(), w, h, *m_sk, true);
        TonedSheet e;
        e.img = straight.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        e.at = now; e.sk = m_sk->id; e.dx = x0 - x; e.dy = y0 - y; e.sw = x1 - x0; e.sh = y1 - y0; e.R = R;
        it = m_toned.insert(id, e);
    }
    const double k = R / it->R;
    const double was = p.opacity();
    p.setOpacity(1);
    p.drawImage(QRectF(x + it->dx * k, y + it->dy * k, it->sw * k, it->sh * k), it->img);
    p.setOpacity(was);
}
QColor Canvas::regionInk(const Region& r) const
{
    const QColor c(r.color);
    if (!m_sk) return c;
    if (m_sk->mono) return m_sk->inkC();
    if (m_sk->darkPaper) return c;
    return QColor(int(c.red() * 0.6 + 0.5), int(c.green() * 0.6 + 0.5), int(c.blue() * 0.6 + 0.5));
}

} // namespace galaxy
