#include "GalaxyWidgets.h"
#include "GalaxyModels.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QWheelEvent>

namespace galaxy {

namespace {
void artStar(QPainter& g, double x, double y, double s)
{
    QRadialGradient q(x, y, s);
    q.setColorAt(0, QColor(255, 255, 255)); q.setColorAt(0.3, QColor(0xff, 0xe6, 0xb0)); q.setColorAt(1, QColor(255, 190, 110, 0));
    g.setPen(Qt::NoPen); g.setBrush(q);
    g.drawEllipse(QPointF(x, y), s, s);
}
void artPlanet(QPainter& g, double x, double y, double s)
{
    QRadialGradient gr(QPointF(x, y), s, QPointF(x - s * 0.35, y - s * 0.35));
    gr.setColorAt(0, QColor(0x6d, 0x9f, 0xbe)); gr.setColorAt(0.6, QColor(0x35, 0x61, 0x7f)); gr.setColorAt(1, QColor(0x0d, 0x1c, 0x2a));
    g.setPen(Qt::NoPen); g.setBrush(gr);
    g.drawEllipse(QPointF(x, y), s, s);
}
const V3 PHOTO_LIGHT = [] { const double x = -0.62, y = 0.34, z = 0.7, n = std::sqrt(x * x + y * y + z * z); return V3 { x / n, y / n, z / n }; }();
inline Frame frameFor(const Body& b) { return b.frame.ax[0] != 0 || b.frame.ax[1] != 0 || b.frame.ax[2] != 0 ? b.frame : frameOf(0, 0); }
}

void paintModelArt(QPainter& g, const std::string& id, uint32_t seed, int D, double t, double rot, double pitch, const QString& key)
{
    const double mid = D / 2.0, cT = std::max(0.05, std::cos(clampPitch(pitch)));
    if (id == "dyson") { drawDyson(g, mid, mid, D * 0.42, cT, seed, t, 'b'); artStar(g, mid, mid, D * 0.2); drawDyson(g, mid, mid, D * 0.42, cT, seed, t, 'f'); }
    else if (id == "enxame") { artStar(g, mid, mid, D * 0.22); drawSwarm(g, mid, mid, D * 0.42, cT, seed, t); }
    else if (id == "mundoAnel") { drawRingworld(g, mid, mid, D * 0.42, D * 0.075, pitch, seed, t, 'b'); artStar(g, mid, mid, D * 0.13); drawRingworld(g, mid, mid, D * 0.42, D * 0.075, pitch, seed, t, 'f'); }
    else if (id == "sifao") { artStar(g, mid, mid, D * 0.15); drawSiphon(g, mid, mid, D * 0.38, cT, seed, t, { 255, 214, 150 }, 1, 1, rot); }
    else if (id == "anelOrbital") {
        const double pr = D * 0.28, Rr = D * 0.46, La = 2.3 + rot * 0.15;
        drawOrbitalRing(g, mid, mid, Rr, pitch, seed, t, 'b', 1, 1, pr, La);
        artPlanet(g, mid, mid, pr);
        drawOrbitalRing(g, mid, mid, Rr, pitch, seed, t, 's', 1, 1, pr, La);
        drawOrbitalRing(g, mid, mid, Rr, pitch, seed, t, 'f', 1, 1, pr, La);
    } else if (id == "elevador") {
        artPlanet(g, D * 0.34, D * 0.68, D * 0.28);
        drawElevator(g, D * 0.34, D * 0.68, D * 0.28, -0.8, D * 0.52, 1, t, 1, 1, QStringLiteral("art"));
    } else {
        drawModel(g, id, seed, mid, mid, D * 0.48, rot, pitch, -0.7, -0.7, 1, 1, key);
        drawAnimated(g, id, seed, mid, mid, D * 0.48, rot, pitch, t);
    }
}
QImage artImage(Canvas* canvas, const std::string& id, ItemKind kind, int px, uint32_t seed, Body* preview)
{
    const int D = px * 2;
    QImage out(D, D, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter g(&out);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::SmoothPixmapTransform);
    const double mid = D / 2.0;
    if ((kind == ItemKind::Planet || kind == ItemKind::Moon) && preview) {
        const int S2 = int(D * 0.86 + 0.5);
        const SphereTable tab = sphereTable(S2, 0, 0.3, frameFor(*preview));
        QImage img(S2, S2, QImage::Format_RGBA8888);
        img.fill(Qt::transparent);
        const double n = std::sqrt(0.55 * 0.55 + 0.3 * 0.3 + 0.78 * 0.78);
        const V3 L { -0.55 / n, 0.3 / n, 0.78 / n };
        shadeSphere(img.bits(), tab, canvas->textureOf(*preview, D > 120 ? 512 : 256), 0.6, &L, atmOf(*preview));
        g.drawImage((D - S2) / 2, (D - S2) / 2, img);
    } else if (kind == ItemKind::Star) artStar(g, mid, mid, D * 0.46);
    else if (kind == ItemKind::Comet) {
        const double x = D * 0.68, y = D * 0.32;
        QLinearGradient q(x, y, D * 0.1, D * 0.85);
        q.setColorAt(0, QColor(210, 235, 255, 242)); q.setColorAt(1, QColor(160, 210, 255, 0));
        QPolygonF tail; tail << QPointF(x + D * 0.03, y - D * 0.05) << QPointF(D * 0.1, D * 0.85) << QPointF(x - D * 0.05, y + D * 0.03);
        g.setPen(Qt::NoPen); g.setBrush(q);
        g.drawPolygon(tail);
        g.setBrush(QColor(0xf2, 0xf8, 0xff));
        g.drawEllipse(QPointF(x, y), D * 0.05, D * 0.05);
    } else if (kind == ItemKind::Belt) {
        const bool rock = id == "asteroides";
        Rng r(uint32_t(id.size() * 37));
        artStar(g, mid, mid, D * 0.1);
        for (int i = 0; i < 260; i++) {
            const double a = r() * TAU, q = D * (0.3 + r() * 0.16), s = D * (0.004 + r() * 0.009), al = 0.5 + r() * 0.5;
            g.fillRect(QRectF(mid + std::cos(a) * q, mid + std::sin(a) * q * 0.62, s, s), rock ? QColor(205, 190, 170, int(al * 255)) : QColor(190, 222, 242, int(al * 255)));
        }
    } else if (kind == ItemKind::Struct) {
        beginFrame(1000);
        paintModelArt(g, id, seed, D, 9, 0.5, 0.78);
    }
    g.end();
    out.setDevicePixelRatio(2);
    return out;
}

// ── o planeta ──
PlanetView::PlanetView(Canvas* canvas, const Body* body, bool zoomable, QWidget* parent) : QWidget(parent), m_canvas(canvas), m_body(body), m_zoomable(zoomable)
{
    setCursor(Qt::OpenHandCursor);
    setAttribute(Qt::WA_TranslucentBackground);
    connect(&m_timer, &QTimer::timeout, this, [this]() { if (!m_drag) m_spin += 0.033 * 0.12; if (isVisible()) update(); });
    m_timer.start(33);
}
void PlanetView::paintEvent(QPaintEvent*)
{
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::SmoothPixmapTransform);
    const Body& b = *m_body;
    const double q = std::min(devicePixelRatioF(), 1.5), W2 = width() * q, H2 = height() * q;
    const int D = int(std::min(1100.0, std::floor(std::min(W2 / (b.rings ? b.rings->outer * 1.08 : 1), H2) * (b.rings ? 0.8 : 0.86) * m_zoom + 0.5)));
    if (D < 4) return;
    const Texture& tex = m_canvas->textureOf(b, D > 300 ? 1024 : 512);
    if (m_tabD != D || m_tabPitch != m_pitch) {
        m_tab = sphereTable(D, 0, m_pitch, frameFor(b));
        m_tabD = D; m_tabPitch = m_pitch;
        m_img = QImage(D, D, QImage::Format_RGBA8888);
    }
    const Atm* atm = atmOf(b);
    shadeSphere(m_img.bits(), m_tab, tex, m_spin, &PHOTO_LIGHT, atm);
    const double R = D / 2.0 / q, cx = width() / 2.0, cy = height() / 2.0;
    if (atm) {
        QRadialGradient gr(cx, cy, R * 1.08);
        const double t0 = 0.98 / 1.08;
        QColor c = QColor::fromRgbF(float(atm->c[0]), float(atm->c[1]), float(atm->c[2]));
        c.setAlphaF(float(clamp(0.26 * atm->k, 0, 1)));
        QColor c0 = c; c0.setAlpha(0);
        gr.setColorAt(0, c0); gr.setColorAt(std::max(0.0, t0 - 0.001), c0); gr.setColorAt(t0, c); gr.setColorAt(1, c0);
        g.setPen(Qt::NoPen); g.setBrush(gr);
        g.drawEllipse(QPointF(cx, cy), R * 1.08, R * 1.08);
    }
    const double ringK = std::max(0.12, std::min(0.62, std::fabs(std::sin(m_pitch + 0.38))));
    if (b.rings) m_canvas->ringOn(g, b, cx, cy, R, false, ringK);
    g.drawImage(QRectF(cx - R, cy - R, R * 2, R * 2), m_img);
    if (b.rings) m_canvas->ringOn(g, b, cx, cy, R, true, ringK);
}
void PlanetView::mousePressEvent(QMouseEvent* e) { m_drag = true; m_last = e->position(); setCursor(Qt::ClosedHandCursor); }
void PlanetView::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_drag) return;
    m_spin += (e->position().x() - m_last.x()) * 0.01;
    m_pitch = std::max(-1.3, std::min(1.3, m_pitch + (e->position().y() - m_last.y()) * 0.007));
    m_last = e->position();
    update();
}
void PlanetView::mouseReleaseEvent(QMouseEvent*) { m_drag = false; setCursor(Qt::OpenHandCursor); }
void PlanetView::wheelEvent(QWheelEvent* e)
{
    if (!m_zoomable) { e->ignore(); return; }
    m_zoom = std::max(0.55, std::min(1.8, m_zoom * std::pow(1.0015, e->angleDelta().y() / 1.2)));
    update();
}

// ── a peça ──
ModelView::ModelView(Canvas* canvas, const std::string& type, uint32_t seed, QWidget* parent) : QWidget(parent), m_canvas(canvas), m_type(type), m_seed(seed)
{
    setCursor(Qt::OpenHandCursor);
    setAttribute(Qt::WA_TranslucentBackground);
    connect(&m_timer, &QTimer::timeout, this, [this]() { if (!m_drag) m_rot += 0.033 * 0.14; if (isVisible()) update(); });
    m_timer.start(33);
}
void ModelView::paintEvent(QPaintEvent*)
{
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::SmoothPixmapTransform);
    const int D = int(std::min(width(), height()) * 0.94);
    if (D < 4) return;
    beginFrame(14);
    g.translate((width() - D) / 2.0, (height() - D) / 2.0);
    paintModelArt(g, m_type, m_seed, D, m_canvas->simTime(), m_rot, m_pitch, QStringLiteral("photo:%1:%2").arg(QString::fromStdString(m_type)).arg(m_seed));
}
void ModelView::mousePressEvent(QMouseEvent* e) { m_drag = true; m_last = e->position(); setCursor(Qt::ClosedHandCursor); }
void ModelView::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_drag) return;
    m_rot += (e->position().x() - m_last.x()) * 0.01;
    m_pitch = clampPitch(m_pitch + (e->position().y() - m_last.y()) * 0.008);
    m_last = e->position();
    update();
}
void ModelView::mouseReleaseEvent(QMouseEvent*) { m_drag = false; setCursor(Qt::OpenHandCursor); }

} // namespace galaxy
