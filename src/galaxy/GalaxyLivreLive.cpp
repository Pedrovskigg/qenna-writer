// Criação Livre: o mundo girando na janela da ficha, com quem orbita em volta (luas mais longe, construções em
// órbita baixa) e o anel orbital e o elevador abraçando o planeta, como no mapa. Arrastar gira, a rodinha aproxima,
// dois cliques voltam ao normal, clicar numa lua ou construção abre ela. Enquanto o autor digita, o giro espera.

#include "GalaxyLivre.h"
#include "GalaxyCanvas.h"
#include "GalaxyDoc.h"
#include "GalaxyModels.h"
#include "GalaxyStructures.h"
#include "GalaxyWidgets.h"

#include <QElapsedTimer>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QWheelEvent>

namespace galaxy {

namespace {
constexpr int LIVE_MAX = 440;
constexpr double ZOOM_MIN = 0.6, ZOOM_MAX = 4;
const V3 FRONT = [] { const double x = -0.52, y = 0.36, z = 0.78, n = std::sqrt(x * x + y * y + z * z); return V3 { x / n, y / n, z / n }; }();
inline Frame frameFor(const Body& b) { return b.frame.ax[0] != 0 || b.frame.ax[1] != 0 || b.frame.ax[2] != 0 ? b.frame : frameOf(0, 0); }
}

LiveWorld::LiveWorld(Livre* owner, const QString& key, QWidget* parent) : QWidget(parent), m_owner(owner), m_key(key)
{
    m_world = owner->isWorld(key);
    m_zoom = owner->m_liveZoom; m_spin = owner->m_liveSpin; m_pitch = owner->m_livePitch; m_rot = owner->m_liveRot; m_orbT = owner->m_liveOrbT;
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    m_clock.start();
    connect(&m_timer, &QTimer::timeout, this, [this]() { if (isVisible()) update(); });
    m_timer.start(33);
}
void LiveWorld::setZoom(double z)
{
    m_zoom = std::clamp(z, ZOOM_MIN, ZOOM_MAX);
    m_owner->m_liveZoom = m_zoom;
    if (zoomChanged) zoomChanged(m_zoom);
    update();
}
void LiveWorld::paintEvent(QPaintEvent*)
{
    const qint64 now = m_clock.elapsed();
    const double dt = std::min(0.05, double(now - m_lastMs) / 1000.0);
    m_lastMs = now;
    // Enquanto o autor digita, o planeta espera: a digitação não disputa com o desenho
    if (!m_drag && !m_owner->typingNow()) { m_spin += dt * 0.11; m_rot += dt * 0.14; m_orbT += dt; }
    if (m_owner->m_liveKey == m_key) { m_owner->m_liveSpin = m_spin; m_owner->m_livePitch = m_pitch; m_owner->m_liveRot = m_rot; m_owner->m_liveOrbT = m_orbT; }
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::SmoothPixmapTransform);
    QElapsedTimer t0;
    t0.start();
    if (m_world) paintWorld(g); else paintBuild(g);
    // quadro caro (zoom, PC fraco): o giro sozinho anda a meia velocidade de quadros
    m_ms = m_ms * 0.9 + double(t0.elapsed()) * 0.1;
    const int want = !m_drag && m_ms > 12 ? 66 : 33;
    if (m_timer.interval() != want) m_timer.setInterval(want);
}
void LiveWorld::paintWorld(QPainter& g)
{
    Body* b = m_owner->bodyOf(m_key);
    if (!b) return;
    Canvas* canvas = m_owner->canvas();
    const Livre::OrbitPlan orb = m_owner->orbitPlan(m_key);
    const double W = width(), H = height(), q = std::min(devicePixelRatioF(), 1.5);
    const bool hasR = b->rings != nullptr;
    const double room = orb.outer > 0 ? std::min(W * 0.94 / orb.outer, H * 0.62) : 1e9;
    const double Dshow = std::max(8.0, std::min({ hasR ? (W / (b->rings->outer * 1.08)) * 0.96 : W * 0.68, H * 0.8, room }) * m_zoom);
    const int D = std::max(4, std::min(int(Dshow * q), LIVE_MAX));
    if (m_tabD != D || m_tabPitch != m_pitch || m_tabRev != uint32_t(quintptr(b))) {
        m_tab = sphereTable(D, 0, m_pitch, frameFor(*b));
        m_tabD = D; m_tabPitch = m_pitch; m_tabRev = uint32_t(quintptr(b));
        m_img = QImage(D, D, QImage::Format_RGBA8888);
    }
    const Texture& tex = canvas->textureOf(*b, m_zoom > 1.6 ? 1024 : 512);
    const Atm* atm = atmOf(*b);
    m_img.fill(Qt::transparent);
    shadeSphere(m_img.bits(), m_tab, tex, m_spin, &FRONT, atm);
    const double cx = W / 2, cy = H / 2, R = Dshow / 2;
    if (atm) {
        QRadialGradient gr(cx, cy, R * 1.09);
        QColor c = QColor::fromRgbF(float(atm->c[0]), float(atm->c[1]), float(atm->c[2]));
        c.setAlphaF(float(clamp(0.3 * atm->k, 0, 1)));
        QColor c0 = c; c0.setAlpha(0);
        gr.setColorAt(0, c0); gr.setColorAt(0.889, c0); gr.setColorAt(0.89, c); gr.setColorAt(1, c0);
        g.setPen(Qt::NoPen); g.setBrush(gr);
        g.drawEllipse(QPointF(cx, cy), R * 1.09, R * 1.09);
    }
    const double kk = std::max(0.2, std::min(0.6, std::fabs(std::sin(m_pitch + 0.2))));
    // quem orbita
    struct Sprite { QString key; QPointF at; double size; bool front, moon, ghost; };
    std::vector<Sprite> sprites;
    const double ca = std::cos(-0.32), sa = std::sin(-0.32);
    for (const Livre::Orbiter& o : orb.list) {
        const double a = o.phase + m_orbT * o.speed, x = std::cos(a) * o.rr * R, y = std::sin(a) * o.rr * R * kk;
        double size = o.moon ? R * 0.34 : R * 0.42;
        size = std::max(o.moon ? 12.0 : 16.0, std::min(o.moon ? 96.0 : 120.0, size));
        sprites.push_back({ o.key, QPointF(cx + x * ca - y * sa, cy + x * sa + y * ca), size, std::sin(a) > 0, o.moon, m_owner->isGhost(o.key) });
    }
    m_hits.clear();
    for (const Sprite& s : sprites) m_hits.push_back({ s.key, s.ghost, s.at, s.size / 2 });
    const bool names = Dshow < W * 1.3;
    const QColor nameC(236, 241, 250, 225), ghostC(230, 185, 128, 230);
    const auto drawName = [&](const QString& text, QPointF at, bool ghost) {
        QFont f = font(); f.setPixelSize(11); f.setWeight(QFont::DemiBold);
        g.setFont(f);
        const QRectF r(at.x() - 80, at.y(), 160, 16);
        g.setPen(QColor(0, 0, 0, 170));
        g.drawText(r.translated(0, 1), Qt::AlignHCenter | Qt::AlignTop, text);
        g.setPen(ghost ? ghostC : nameC);
        g.drawText(r, Qt::AlignHCenter | Qt::AlignTop, text);
    };
    const auto drawSprite = [&](const Sprite& s) {
        const int bucket = std::max(16, int(std::ceil(s.size / 8) * 8));
        const QPixmap img = s.moon ? m_owner->ball(s.key, bucket) : m_owner->modelPix(m_owner->buildType(s.key), m_owner->buildSeed(s.key), bucket);
        g.save();
        g.setOpacity(s.ghost ? 0.5 : 1);
        g.drawPixmap(QRectF(s.at.x() - s.size / 2, s.at.y() - s.size / 2, s.size, s.size), img, QRectF(0, 0, img.width(), img.height()));
        g.setOpacity(1);
        if (s.ghost) { g.setPen(QPen(ghostC, 1.2, Qt::DashLine)); g.setBrush(Qt::NoBrush); g.drawEllipse(s.at, s.size / 2 + 3, s.size / 2 + 3); }
        if (names) drawName(m_owner->label(s.key), QPointF(s.at.x(), s.at.y() + s.size / 2 + 2), s.ghost);
        g.restore();
    };
    // anel orbital e elevador: abraçam o planeta
    const double t = canvas->simTime() + double(m_clock.elapsed()) / 1000.0;
    const double P = std::max(0.2, std::min(1.35, 0.5 + m_pitch * 0.8));
    const auto wrapAt = [&](const Livre::Wrap& w, double& Rr, double& ang, double& Hh) {
        Rr = R * (1.28 + 0.34 * (1 + 0.45 * w.i));
        ang = double(m_owner->buildSeed(w.key) % 628) / 100 + m_orbT * 0.02;
        Hh = R * 0.9;
    };
    const auto drawWrap = [&](const Livre::Wrap& w, bool back) {
        double Rr, ang, Hh;
        wrapAt(w, Rr, ang, Hh);
        const double alpha = m_owner->isGhost(w.key) ? 0.5 : 1;
        const uint32_t seed = m_owner->buildSeed(w.key);
        if (w.ring) {
            if (back) drawOrbitalRing(g, cx, cy, Rr, P, seed, t, 'b', alpha, 1, R, 2.4);
            else { drawOrbitalRing(g, cx, cy, Rr, P, seed, t, 's', alpha, 1, R, 2.4); drawOrbitalRing(g, cx, cy, Rr, P, seed, t, 'f', alpha, 1, R, 2.4); }
        } else if (!back) drawElevator(g, cx, cy, R, ang, Hh, std::cos(P), t, alpha, 1, QStringLiteral("live-elev:") + w.key);
    };
    // as órbitas tracejadas
    if (!orb.list.empty()) {
        g.save();
        g.translate(cx, cy);
        g.rotate(-0.32 * 180 / PI_D);
        QPen pen(QColor(190, 210, 240, 56), 1, Qt::DashLine);
        pen.setDashPattern({ 3, 5 });
        g.setPen(pen);
        g.setBrush(Qt::NoBrush);
        for (const Livre::Orbiter& o : orb.list) g.drawEllipse(QPointF(0, 0), o.rr * R, o.rr * R * kk);
        g.restore();
    }
    for (const Sprite& s : sprites) if (!s.front) drawSprite(s);
    for (const Livre::Wrap& w : orb.wraps) drawWrap(w, true);
    if (hasR) canvas->ringOn(g, *b, cx, cy, R, false, kk);
    g.drawImage(QRectF(cx - R, cy - R, R * 2, R * 2), m_img);
    if (hasR) canvas->ringOn(g, *b, cx, cy, R, true, kk);
    for (const Livre::Wrap& w : orb.wraps) drawWrap(w, false);
    for (const Sprite& s : sprites) if (s.front) drawSprite(s);
    for (const Livre::Wrap& w : orb.wraps) {
        double Rr, ang, Hh;
        wrapAt(w, Rr, ang, Hh);
        QPointF pt;
        if (w.ring) { const double y = Rr * std::cos(P), r = -0.32; pt = QPointF(cx - y * std::sin(r), cy + y * std::cos(r)); }
        else pt = QPointF(cx + std::cos(ang) * (R + Hh), cy + std::sin(ang) * (R * 0.97 + Hh * std::cos(P)));
        m_hits.push_back({ w.key, m_owner->isGhost(w.key), pt, 12 });
        if (names) drawName(m_owner->label(w.key), QPointF(pt.x(), pt.y() + 6), m_owner->isGhost(w.key));
    }
}
void LiveWorld::paintBuild(QPainter& g)
{
    const std::string type = m_owner->buildType(m_key);
    if (type.empty()) return;
    const double W = width(), H = height();
    const int D = std::max(8, int(std::min(W, H) * 0.94 * m_zoom));
    beginFrame(14);
    g.save();
    g.translate((W - D) / 2.0, (H - D) / 2.0);
    const uint32_t seed = m_owner->buildSeed(m_key);
    paintModelArt(g, type, seed, D, m_owner->canvas()->simTime() + double(m_clock.elapsed()) / 1000.0, m_rot, m_pitch, QStringLiteral("live:%1:%2").arg(QString::fromStdString(type)).arg(seed));
    g.restore();
    m_hits.clear();
}
QString LiveWorld::hitAt(QPointF p) const
{
    const Hit* best = nullptr;
    double bd = 1e9;
    for (const Hit& h : m_hits) {
        const double d = std::hypot(h.at.x() - p.x(), h.at.y() - p.y());
        if (d <= h.r + 6 && d < bd) { bd = d; best = &h; }
    }
    return best && !best->ghost ? best->key : QString();
}
void LiveWorld::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    m_drag = true;
    m_last = m_press = e->position();
    setCursor(Qt::ClosedHandCursor);
}
void LiveWorld::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_drag) { setCursor(hitAt(e->position()).isEmpty() ? Qt::OpenHandCursor : Qt::PointingHandCursor); return; }
    const double dx = e->position().x() - m_last.x(), dy = e->position().y() - m_last.y();
    m_spin += dx * 0.01;
    m_rot += dx * 0.01;
    if (m_world) m_pitch = std::max(-1.2, std::min(1.2, m_pitch + dy * 0.006));
    else m_pitch = clampPitch(m_pitch + dy * 0.008);
    m_last = e->position();
    update();
}
void LiveWorld::mouseReleaseEvent(QMouseEvent* e)
{
    const bool click = m_drag && (e->position() - m_press).manhattanLength() < 5;
    m_drag = false;
    setCursor(Qt::OpenHandCursor);
    if (!click) return;
    const QString k = hitAt(e->position());
    if (!k.isEmpty()) m_owner->select(k);
}
void LiveWorld::mouseDoubleClickEvent(QMouseEvent*) { setZoom(1); }
void LiveWorld::wheelEvent(QWheelEvent* e)
{
    setZoom(m_zoom * std::pow(1.0018, e->angleDelta().y()));
    e->accept();
}

} // namespace galaxy
