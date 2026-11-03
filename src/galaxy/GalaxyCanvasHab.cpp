// As classes de planeta no mapa: com a vista ligada, a galáxia escurece e cada sistema brilha na cor do melhor mundo que tem.

#include "GalaxyCanvas.h"
#include "GalaxyHab.h"

#include <QPainterPath>
#include <QRadialGradient>

namespace galaxy {

void Canvas::setHab(bool on)
{
    hab = on;
    if (!on) habOnly = 0;
    m_fx.key.clear(); m_bgKey.clear(); m_toned.clear();
    m_dirty = true;
    update();
}
void Canvas::setHabOnly(int n)
{
    habOnly = n;
    m_fx.key.clear();
    m_dirty = true;
    update();
}
void Canvas::habDirty() { m_habCache.clear(); }
const Canvas::SysHab& Canvas::sysHab(const System& s)
{
    auto it = m_habCache.find(s.id);
    if (it != m_habCache.end()) return *it;
    SysHab e;
    const auto see = [&](int n, const QString& k) { e.counts[size_t(n)]++; if (n && (!e.best || n < e.best)) { e.best = n; e.bestKey = k; } };
    for (Body* p : planetsOf(s)) {
        see(habInfo(*p, &s, nullptr).cls, keyP(s, *p));
        for (const auto& m : p->moons) see(habInfo(*m, &s, p).cls, keyM(s, *p, *m));
    }
    return *m_habCache.insert(s.id, e);
}
std::array<int, 7> Canvas::habTally(const std::vector<const System*>& ss)
{
    std::array<int, 7> c {};
    for (const System* s : ss) { const SysHab& h = sysHab(*s); for (size_t i = 0; i < 7; i++) c[i] += h.counts[i]; }
    return c;
}
const QImage& Canvas::habGlow(int n)
{
    auto it = m_habGlows.find(n);
    if (it != m_habGlows.end()) return *it;
    QImage c(128, 128, QImage::Format_ARGB32_Premultiplied);
    c.fill(Qt::transparent);
    {
        QPainter g(&c);
        QRadialGradient gr(64, 64, 64);
        gr.setColorAt(0, habColor(n, 0.62)); gr.setColorAt(0.35, habColor(n, 0.24)); gr.setColorAt(1, habColor(n, 0));
        g.fillRect(c.rect(), gr);
    }
    return *m_habGlows.insert(n, c);
}
// O brilho que junta os sistemas em zonas: dá pra ver de longe onde a galáxia é generosa.
void Canvas::drawHabHeat(QPainter& p, Region* fr, double z, double cT, double zoomK)
{
    const double k = 1 - smooth(9, 30, zoomK);
    if (k <= 0) return;
    static const double HEAT[7] = { 0, 0.15, 0.11, 0.07, 0.04, 0.035, 0.03 };
    p.save();
    if (!m_sk) p.setCompositionMode(QPainter::CompositionMode_Plus);
    p.setOpacity((m_sk ? 0.55 : 0.9) * k);
    for (const auto& sp : doc->gal->systems) {
        const System& s = *sp;
        if (s.gone || (fr && !fr->systems.contains(s.id))) continue;
        const SysHab& h = sysHab(s);
        const int n = habOnly ? (h.counts[size_t(habOnly)] ? habOnly : 0) : h.best <= 3 ? h.best : 0;
        if (!n) continue;
        double x, y;
        proj(s.x, s.y, s.layH, x, y);
        const double R = std::max(14.0, HEAT[n] * z);
        if (x < -R || y < -R || x > width() + R || y > height() + R) continue;
        p.drawImage(QRectF(x - R, y - R * cT, R * 2, R * 2 * cT), habGlow(n));
    }
    p.restore();
}
void Canvas::drawHabFar(QPainter& p, const System& s, double x, double y, double alpha)
{
    const SysHab& h = sysHab(s);
    const bool hit = !habOnly || h.counts[size_t(habOnly)];
    const int n = habOnly ? (hit ? habOnly : 0) : h.best;
    const double r = !n ? 1.8 : n == 1 ? 4.4 : n == 2 ? 3.8 : n == 3 ? 3.2 : 2.6;
    p.setOpacity(alpha * (n ? 1 : 0.35));
    if (n) { QPen pen(m_sk ? m_sk->paperC() : QColor(3, 5, 10, 217)); pen.setWidthF(1); p.setPen(pen); p.setBrush(habColor(n)); }
    else { p.setPen(Qt::NoPen); p.setBrush(m_sk ? m_sk->inkC(0.8) : QColor(160, 170, 190)); }
    p.drawEllipse(QPointF(x, y), r, r);
    if (n && cam.z / fitZ() > 2.4) {
        QFont f(QStringLiteral("IBM Plex Mono")); f.setPixelSize(10); f.setWeight(QFont::Bold);
        p.setFont(f);
        p.setPen(m_sk ? m_sk->inkC() : habColor(n));
        p.drawText(QRectF(x - 20, y - r - 16, 40, 14), Qt::AlignCenter, QString::fromLatin1(habRoman(n)));
    }
    p.setOpacity(alpha);
}
// De perto: um aro e o selo da classe em cada mundo.
void Canvas::habMark(QPainter& p, const Body& b, const System& s, const Body* host, double px, double py, double pr, bool moon)
{
    const int n = habInfo(b, &s, host).cls;
    if (habOnly && n != habOnly) {
        const double was = p.opacity();
        p.setOpacity(was * 0.62);
        p.setPen(Qt::NoPen); p.setBrush(m_sk ? m_sk->paperC() : QColor(3, 5, 10));
        p.drawEllipse(QPointF(px, py), pr + 0.5, pr + 0.5);
        p.setOpacity(was);
        return;
    }
    if (!n) return;
    QPen pen(habColor(n, 0.95)); pen.setWidthF(moon ? 1.2 : 1.6);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    const double ro = pr + (moon ? 2 : 3.5);
    p.drawEllipse(QPointF(px, py), ro, ro);
    if (moon && pr < 5) return;
    const QString t = QString::fromLatin1(habRoman(n));
    const double d = (pr + (moon ? 3 : 5)) * 0.72, bx = px + d, by = py - d;
    QFont f(QStringLiteral("IBM Plex Mono")); f.setPixelSize(moon ? 9 : 11); f.setWeight(QFont::Bold);
    p.setFont(f);
    const double w = std::max(moon ? 13.0 : 17.0, QFontMetricsF(f).horizontalAdvance(t) + 7), bh = moon ? 12 : 15;
    const QRectF box(bx - w / 2, by - bh / 2, w, bh);
    p.setPen(Qt::NoPen); p.setBrush(habColor(n));
    p.drawRoundedRect(box, 4, 4);
    p.setPen(QColor(0x0a, 0x0d, 0x14));
    p.drawText(box, Qt::AlignCenter, t);
}
// No PNG, a legenda acompanha a vista.
void Canvas::drawHabLegend(QPainter& p)
{
    std::vector<int> list;
    for (int n = 1; n <= 6; n++) if (!habOnly || n == habOnly) list.push_back(n);
    for (size_t i = 0; i < list.size(); i++) {
        const int n = list[i];
        const double y = height() - 24 - double(list.size() - 1 - i) * 22;
        const QRectF box(24, y - 12, 26, 16);
        p.setPen(Qt::NoPen); p.setBrush(habColor(n));
        p.drawRoundedRect(box, 4, 4);
        QFont f(QStringLiteral("IBM Plex Mono")); f.setPixelSize(11); f.setWeight(QFont::Bold);
        p.setFont(f);
        p.setPen(QColor(0x0a, 0x0d, 0x14));
        p.drawText(box, Qt::AlignCenter, QString::fromLatin1(habRoman(n)));
        p.setFont(uiFont(400, 13));
        p.setPen(m_sk ? m_sk->inkC() : QColor(0xe4, 0xeb, 0xf5));
        p.drawText(QPointF(58, y), habName(n));
    }
}

} // namespace galaxy
