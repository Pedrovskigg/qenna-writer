// Criação Livre: o minimapa (e o mapa em tela grande). Ver GalaxyLivreMap.h.

#include "GalaxyLivreMap.h"
#include "GalaxyLivre.h"
#include "GalaxyDoc.h"
#include "GalaxyPanel.h"

#include "../TerritorioStore.h"
#include "../Theme.h"
#include "../carto/CartoIcons.h"

#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadialGradient>
#include <QSettings>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace galaxy {

namespace {
// a altura de cada coisa acima ou abaixo do chão da galáxia (o que dá o 3D)
double hOf(const QString& key) { return (double(qHash(key) % 1000u) / 1000.0 - 0.5) * 0.042; }
double phaseOf(const QString& key) { return double(qHash(key + QStringLiteral("o")) % 6283u) / 1000.0; }
double worldPx(const Body& b)
{
    if (b.body == BodyKind::Lua) return 28;
    switch (b.type) {
    case Type::Gigante: case Type::Quente: return 76;
    case Type::GeloG: case Type::Subnetuno: return 66;
    case Type::Anao: return 34;
    default: return 56;
    }
}
QString shortName(QString n)
{
    static const QStringList pre = { QStringLiteral("Império Matriarcal de "), QStringLiteral("Império de "), QStringLiteral("Império "), QStringLiteral("República "), QStringLiteral("Reino de "), QStringLiteral("Reino ") };
    for (const QString& p : pre) if (n.startsWith(p, Qt::CaseInsensitive)) return n.mid(p.size());
    return n;
}
QColor terrInk(const QString& color)
{
    const QColor c(color.isEmpty() ? QStringLiteral("#8e9db4") : color);
    return QColor(int(c.red() + (255 - c.red()) * 0.55), int(c.green() + (255 - c.green()) * 0.55), int(c.blue() + (255 - c.blue()) * 0.55));
}
QString fieldSig(const TerrField& F, const QString& color)
{
    double a = 0, b = 0;
    for (const auto& p : F.pts) { a += p[0] * 3.1 + p[1] * 7.7; }
    for (const auto& s : F.stamps) { b += s[0] * 5.3 + s[1] * 1.9 + s[2] * 11.0; }
    return QStringLiteral("%1,%2,%3,%4,%5").arg(F.pts.size()).arg(a, 0, 'g', 12).arg(F.stamps.size()).arg(b, 0, 'g', 12).arg(color);
}
} // namespace

LivreMap::LivreMap(Livre* owner, Doc* doc, bool big, QWidget* parent) : QWidget(parent), m_owner(owner), m_doc(doc), m_big(big)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::ClickFocus);
    setToolTip(QString());
    m_clock.start();
    m_anim.setInterval(16);
    connect(&m_anim, &QTimer::timeout, this, &LivreMap::step);
    m_pulse.setInterval(40);
    connect(&m_pulse, &QTimer::timeout, this, [this]() {
        if (!m_hasHere || !isVisible()) return;
        const double s = std::max(9.0, m_hereR) * 2.5 + 6;
        update(QRectF(m_here.x() - s, m_here.y() - s, 2 * s, 2 * s).toAlignedRect());
    });
    m_settle.setSingleShot(true);
    m_settle.setInterval(200);
    connect(&m_settle, &QTimer::timeout, this, [this]() { m_dirty = true; update(); });
    m_peekTimer.setSingleShot(true);
    m_peekTimer.setInterval(160);
    connect(&m_peekTimer, &QTimer::timeout, this, [this]() { if (!m_hover.isEmpty() && !m_down.on) m_owner->showPeek(m_hot, m_hover); });
    m_hot = new QWidget(this);
    m_hot->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_hot->resize(12, 12);
    m_hot->hide();
    // os botões em cima do mapa
    m_ctl = new QWidget(this);
    auto* h = new QHBoxLayout(m_ctl);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(big ? 5 : 3);
    const auto mk = [&](const QString& text, const QString& tip) {
        auto* b = new QToolButton(m_ctl);
        b->setObjectName(QStringLiteral("lvMapBtn"));
        b->setProperty("big", big);
        b->setText(text);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        h->addWidget(b);
        return b;
    };
    m_b3d = mk(QStringLiteral("3D"), QCoreApplication::translate("galaxy::Livre", "Ver de cima"));
    connect(m_b3d, &QToolButton::clicked, this, [this]() { setTilt(cam.tilt > 0.2 ? 0 : 0.95); });
    auto* zo = mk(QStringLiteral("−"), QCoreApplication::translate("galaxy::Livre", "Afastar"));
    connect(zo, &QToolButton::clicked, this, [this]() { flyTo({ cam.x, cam.y, std::max(cam.z / 2.2, fitZ() * 0.5), cam.h }, 380); });
    auto* zi = mk(QStringLiteral("+"), QCoreApplication::translate("galaxy::Livre", "Aproximar"));
    connect(zi, &QToolButton::clicked, this, [this]() { flyTo({ cam.x, cam.y, std::min(cam.z * 2.2, fitZ() * 600), cam.h }, 380); });
    auto* fa = mk(QString(), QCoreApplication::translate("galaxy::Livre", "Enquadrar tudo"));
    fa->setIcon(carto::icon(QStringLiteral("fit"), QColor(223, 230, 240), QColor(Theme::accentDefault()), big ? 15 : 12));
    connect(fa, &QToolButton::clicked, this, [this]() { fitAll(); });
    syncButtons();
}

double LivreMap::fitZ() const { return std::max(40.0, double(std::min(width(), height()))) / 2.47; }
void LivreMap::proj(double x, double y, double h, double& sx, double& sy) const
{
    const double dx = x - cam.x, dy = y - cam.y, co = std::cos(cam.yaw), si = std::sin(cam.yaw);
    const double X = dx * co - dy * si, Y = dx * si + dy * co;
    sx = width() / 2.0 + X * cam.z;
    sy = height() / 2.0 - (Y * std::cos(cam.tilt) + (h - cam.h) * std::sin(cam.tilt)) * cam.z;
}
void LivreMap::toWorld(double sx, double sy, double& wx, double& wy) const
{
    const double X = (sx - width() / 2.0) / cam.z, Y = ((height() / 2.0 - sy) / cam.z + cam.h * std::sin(cam.tilt)) / std::max(0.2, std::cos(cam.tilt));
    const double co = std::cos(cam.yaw), si = std::sin(cam.yaw);
    wx = cam.x + X * co + Y * si;
    wy = cam.y - X * si + Y * co;
}
std::vector<LivreMap::Anchor> LivreMap::anchors() const
{
    std::vector<Anchor> out;
    if (!m_doc->gal) return out;
    for (System* s : m_owner->livreSystems()) {
        Anchor a;
        a.key = keyS(*s); a.sys = s; a.x = s->x; a.y = s->y; a.h = hOf(a.key); a.ghost = m_owner->isGhost(a.key);
        out.push_back(a);
    }
    // mundo solto: mora num sistema escondido, mas no mapa aparece ele mesmo
    for (const auto& s : m_doc->gal->systems) {
        if (s->isCore || !m_doc->isSolo(*s)) continue;
        for (Body* p : planetsOf(*s)) {
            Anchor a;
            a.key = keyP(*s, *p); a.sys = s.get(); a.x = s->x; a.y = s->y; a.h = hOf(a.key); a.ghost = m_owner->isGhost(a.key); a.solo = true;
            out.push_back(a);
        }
    }
    return out;
}
QString LivreMap::anchorOf(const QString& key) const
{
    QString c = key;
    for (int guard = 0; guard < 16 && !c.isEmpty(); guard++) {
        const Livre::LK t = m_owner->kindOf(c);
        if (t == Livre::LK::Sys) { System* s = m_owner->sysOf(c); if (s && !m_doc->isSolo(*s)) return c; }
        if (t == Livre::LK::Planet && m_owner->solarKey(c).isEmpty()) return c;
        c = m_owner->parentKey(c);
    }
    return QString();
}
bool LivreMap::posOf(const QString& anchor, double& x, double& y, double& h) const
{
    System* s = m_owner->sysOf(anchor);
    if (!s) return false;
    x = s->x; y = s->y; h = hOf(anchor);
    return true;
}
double LivreMap::sysR(const Anchor& a) const
{
    if (a.solo || !a.sys) return 0.0069;
    return (0.008 + 0.0017 * std::min<double>(double(planetsOf(*a.sys).size()), 12)) * 1.15;
}
LivreMap::Frame LivreMap::frameAll() const
{
    if (!m_doc->gal) return { 0, 0, fitZ(), 0 };
    double x0 = 9, y0 = 9, x1 = -9, y1 = -9;
    bool any = false;
    for (const Anchor& a : anchors()) { x0 = std::min(x0, a.x); y0 = std::min(y0, a.y); x1 = std::max(x1, a.x); y1 = std::max(y1, a.y); any = true; }
    for (const auto& r : m_doc->regions) {
        const TerrField F = terrField(*m_doc, *r);
        double a, b, c, d;
        if (terrBox(F, a, b, c, d)) { x0 = std::min(x0, a); y0 = std::min(y0, b); x1 = std::max(x1, c); y1 = std::max(y1, d); any = true; }
    }
    if (!any) return { 0, 0, fitZ(), 0 };
    const double span = std::max({ x1 - x0, y1 - y0, 0.345 });
    return { (x0 + x1) / 2, (y0 + y1) / 2, std::clamp(std::min(width(), height()) / (span * 1.15), fitZ() * 0.6, fitZ() * 6), 0 };
}
LivreMap::Frame LivreMap::frameOf(const QString& sel) const
{
    const double m = std::min(width(), height());
    if (!m_doc->gal) return { 0, 0, fitZ(), 0 };
    if (sel.isEmpty() || sel == QLatin1String("GAL")) return frameAll();
    if (m_owner->kindOf(sel) == Livre::LK::Terr) {
        const Region* r = m_doc->resolve(sel).region;
        double a, b, c, d;
        if (!r || !terrBox(terrField(*m_doc, *r), a, b, c, d)) return frameAll();
        const double span = std::max({ c - a, d - b, 0.14 });
        return { (a + c) / 2, (b + d) / 2, m / (span * 1.2), 0 };
    }
    const QString an = anchorOf(sel);
    if (an.isEmpty()) { const QString t = m_owner->ancestor(sel, Livre::LK::Terr); return t.isEmpty() ? frameAll() : frameOf(t); }
    double x, y, h;
    if (!posOf(an, x, y, h)) return frameAll();
    Anchor a; a.key = an; a.sys = m_owner->sysOf(an); a.solo = m_owner->kindOf(an) == Livre::LK::Planet;
    const double R = a.solo ? 0.023 : sysR(a);
    return { x, y, m * (m_big ? 0.36 : 0.4) / R, h };
}
void LivreMap::flyTo(const Frame& f, double ms)
{
    const double d = std::hypot(f.x - cam.x, f.y - cam.y);
    const double span = std::max(1.0, (d * std::min(cam.z, f.z)) / (std::max(width(), 120) * 0.45));
    m_fly = { true, m_clock.elapsed(), ms + std::min(700.0, std::log2(span) * 120), cam.x, cam.y, cam.z, cam.h, f.x, f.y, f.z, f.h, std::log(span) };
    if (!m_anim.isActive()) m_anim.start();
}
void LivreMap::setTilt(double t)
{
    m_tiltA = { true, m_clock.elapsed(), cam.tilt, t };
    if (!m_anim.isActive()) m_anim.start();
}
void LivreMap::step()
{
    const qint64 now = m_clock.elapsed();
    bool anim = false;
    if (m_fly.on) {
        const double k = std::min(1.0, double(now - m_fly.t0) / m_fly.ms), e = k < 0.5 ? 4 * k * k * k : 1 - std::pow(-2 * k + 2, 3) / 2;
        cam.z = std::exp(std::log(m_fly.z0) + (std::log(m_fly.z) - std::log(m_fly.z0)) * e - m_fly.bump * std::sin(TAU / 2 * e) * 0.9);
        cam.x = m_fly.x0 + (m_fly.x - m_fly.x0) * e; cam.y = m_fly.y0 + (m_fly.y - m_fly.y0) * e; cam.h = m_fly.h0 + (m_fly.h - m_fly.h0) * e;
        if (k >= 1) m_fly.on = false;
        anim = true;
    }
    if (m_tiltA.on) {
        const double k = std::min(1.0, double(now - m_tiltA.t0) / 600.0), e = 1 - std::pow(1 - k, 3);
        cam.tilt = m_tiltA.from + (m_tiltA.to - m_tiltA.from) * e;
        if (k >= 1) m_tiltA.on = false;
        anim = true;
        syncButtons();
    }
    m_dirty = true;
    update();
    if (!anim) { m_anim.stop(); m_settle.start(); }
}
bool LivreMap::busy() const { return m_fly.on || m_tiltA.on || m_down.moved || (m_wheelAt.isValid() && m_wheelAt.elapsed() < 160); }
void LivreMap::syncButtons()
{
    const bool on = cam.tilt > 0.2;
    m_b3d->setProperty("on", on);
    m_b3d->setToolTip(on ? QCoreApplication::translate("galaxy::Livre", "Ver de cima") : QCoreApplication::translate("galaxy::Livre", "Inclinar (3D)"));
    m_b3d->style()->unpolish(m_b3d);
    m_b3d->style()->polish(m_b3d);
}
void LivreMap::layoutButtons()
{
    m_ctl->adjustSize();
    const int pad = m_big ? 16 : 6;
    m_ctl->move(width() - m_ctl->width() - pad, height() - m_ctl->height() - pad);
    m_ctl->raise();
}
void LivreMap::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    layoutButtons();
    if (!m_started && width() > 20 && height() > 20) { m_started = true; frameOn(m_lastSel); }
    m_dirty = true;
}

void LivreMap::follow(const QString& sel)
{
    if (m_started && sel == m_lastSel) return;
    m_lastSel = sel;
    if (!m_started) { if (width() > 20 && height() > 20) { m_started = true; frameOn(sel); } return; }
    flyTo(frameOf(sel));
}
void LivreMap::frameOn(const QString& sel)
{
    m_lastSel = sel;
    const Frame f = frameOf(sel);
    cam.x = f.x; cam.y = f.y; cam.z = f.z; cam.h = f.h;
    m_fly.on = false;
    m_dirty = true;
    update();
}
void LivreMap::fitAll() { flyTo(frameAll()); }

// ── desenhar ──
void LivreMap::render()
{
    const double dpr = devicePixelRatioF();
    const int W = width(), H = height();
    const QSize S(std::max(1, int(std::lround(W * dpr))), std::max(1, int(std::lround(H * dpr))));
    if (m_frame.size() != S) m_frame = QImage(S, QImage::Format_RGB32);
    m_frame.setDevicePixelRatio(dpr);
    m_frame.fill(QColor(3, 5, 10));
    m_hits.clear();
    m_hasHere = false;
    if (!m_doc->gal || !cam.z) return;
    const double zoomK = cam.z / fitZ(), cT = std::cos(cam.tilt), sT = std::sin(cam.tilt);
    const bool isBusy = busy();
    const QString sel = m_owner->m_sel;
    const QString focusT = m_owner->kindOf(sel) == Livre::LK::Terr ? sel : QString();
    {
        QPainter g(&m_frame);
        g.setRenderHint(QPainter::SmoothPixmapTransform);
        // a galáxia, bem apagada (este mapa é sempre Atlas: o normal fica ilegível nesse tamanho)
        const double gA = (1 - smooth(6, 60, zoomK) * 0.92) * 0.12;
        if (gA > 0.003 && !m_doc->galImg.isNull()) {
            if (m_galKey != m_doc->galImg.cacheKey()) { m_galKey = m_doc->galImg.cacheKey(); m_galSmall = m_doc->galImg.scaled(700, 700, Qt::IgnoreAspectRatio, Qt::SmoothTransformation); }
            double p0x, p0y, p1x, p1y, p2x, p2y;
            proj(-EXT, EXT, 0, p0x, p0y); proj(EXT, EXT, 0, p1x, p1y); proj(-EXT, -EXT, 0, p2x, p2y);
            const QImage& src = (p1x - p0x) * dpr < 800 ? m_galSmall : m_doc->galImg;
            const double T = src.width();
            g.setCompositionMode(QPainter::CompositionMode_Plus);
            g.setOpacity(gA);
            g.setTransform(QTransform((p1x - p0x) / T, (p1y - p0y) / T, (p2x - p0x) / T, (p2y - p0y) / T, p0x, p0y));
            g.drawImage(0, 0, src);
            g.resetTransform();
            g.setOpacity(1);
            g.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
    }
    // estrelas de perto, bem apagadas: o que importa é o que você criou
    const double fade = smooth(1.4, 3.2, zoomK) * 0.15;
    if (fade > 0.003) {
        static const int DC[5][3] = { { 0xa9, 0xc4, 0xff }, { 0xd7, 0xe3, 0xff }, { 0xff, 0xf4, 0xe6 }, { 0xff, 0xd9, 0xa8 }, { 0xff, 0xb9, 0x8a } };
        const DetailStars& d = m_doc->gal->detail;
        uchar* bits = m_frame.bits();
        const qsizetype bpl = m_frame.bytesPerLine();
        const double co = std::cos(cam.yaw), si = std::sin(cam.yaw), hk = cam.h * sT;
        for (int i = 0; i < d.n; i++) {
            const double dx = d.xs[size_t(i)] - cam.x, dy = d.ys[size_t(i)] - cam.y;
            const double X = dx * co - dy * si, Y = dx * si + dy * co;
            const double sx = (W / 2.0 + X * cam.z) * dpr, sy = (H / 2.0 - (Y * cT - hk) * cam.z) * dpr;
            if (sx < 1 || sy < 1 || sx >= S.width() - 2 || sy >= S.height() - 2) continue;
            const int* c = DC[d.cs[size_t(i)] % 5];
            const double a = fade * (0.3 + d.bs[size_t(i)] * 0.25);
            QRgb& q = reinterpret_cast<QRgb*>(bits + bpl * int(sy))[int(sx)];
            q = qRgb(std::min(255, qRed(q) + int(c[0] * a)), std::min(255, qGreen(q) + int(c[1] * a)), std::min(255, qBlue(q) + int(c[2] * a)));
        }
    }
    QPainter g(&m_frame);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::SmoothPixmapTransform);
    // territórios: Atlas, a 1,5x, mais forte quando é o seu
    const double tk = 1 - smooth(8, 60, zoomK);
    struct TLab { QString text, color; double x, y, a; };
    std::vector<TLab> tlabels;
    if (tk > 0.02) {
        double ox, oy, ex, ey, fx, fy;
        proj(0, 0, 0, ox, oy); proj(1, 0, 0, ex, ey); proj(0, 1, 0, fx, fy);
        const double A[6] = { ox * dpr, oy * dpr, ex * dpr, ey * dpr, fx * dpr, fy * dpr };
        const QString camKey = QStringLiteral("%1|%2|%3|%4|%5|%6|%7x%8|%9").arg(cam.x, 0, 'g', 12).arg(cam.y, 0, 'g', 12).arg(cam.z, 0, 'g', 12).arg(cam.h, 0, 'g', 10).arg(cam.tilt, 0, 'g', 10).arg(cam.yaw, 0, 'g', 10).arg(S.width()).arg(S.height()).arg(int(isBusy));
        QSet<QString> seen;
        for (const auto& r : m_doc->regions) {
            const TerrField F = terrField(*m_doc, *r);
            if (terrEmpty(F)) continue;
            seen.insert(r->key);
            const QString key = camKey + QLatin1Char('|') + fieldSig(F, r->color);
            TerrCache& c = m_terr[r->key];
            if (c.key != key) { c.key = key; c.im = terrImage(*m_doc, F, QColor(r->color.isEmpty() ? QStringLiteral("#8e9db4") : r->color), A, S.width(), S.height(), cam.z * dpr, dpr, isBusy); }
            const bool mine = !sel.isEmpty() && (sel == r->key || m_owner->isInside(sel, r->key));
            if (!c.im.empty) {
                g.setOpacity(std::min(1.0, tk * (mine ? 1 : 0.72) * 1.5) * (!focusT.isEmpty() && focusT != r->key ? 0.4 : 1));
                g.drawImage(QRectF(c.im.x / dpr, c.im.y / dpr, c.im.img.width() / dpr, c.im.img.height() / dpr), c.im.img);
                g.setOpacity(1);
            }
            double cx, cy;
            if (terrCenter(F, cx, cy)) { double sx, sy; proj(cx, cy, 0, sx, sy); tlabels.push_back({ shortName(r->name).toUpper(), r->color, sx, sy, !focusT.isEmpty() && focusT != r->key ? 0.5 : 1 }); }
        }
        for (auto it = m_terr.begin(); it != m_terr.end();) it = seen.contains(it.key()) ? std::next(it) : m_terr.erase(it);
    }
    // conexões
    {
        const auto pointOf = [&](const QString& k, double& sx, double& sy) {
            if (k.isEmpty()) return false;
            if (m_owner->kindOf(k) == Livre::LK::Terr) {
                const Region* r = m_doc->resolve(k).region;
                double cx, cy;
                if (!r || !terrCenter(terrField(*m_doc, *r), cx, cy)) return false;
                proj(cx, cy, 0, sx, sy);
                return true;
            }
            const QString an = anchorOf(k);
            double x, y, h;
            if (an.isEmpty() || !posOf(an, x, y, h)) return false;
            proj(x, y, h, sx, sy);
            return true;
        };
        QPen pen(QColor(140, 200, 255, 153), 1.2);
        pen.setDashPattern({ 4 / 1.2, 3 / 1.2 });
        g.setPen(pen);
        for (const auto& [ka, kb] : m_owner->mapLinks()) {
            double ax, ay, bx, by;
            if (!pointOf(ka, ax, ay) || !pointOf(kb, bx, by)) continue;
            if (std::hypot(ax - bx, ay - by) < 2) continue;
            g.drawLine(QPointF(ax, ay), QPointF(bx, by));
        }
        g.setPen(Qt::NoPen);
    }
    // o que nasceu aqui, de trás pra frente
    struct Lab { QString text; double x, y; int pri; double a; bool big; };
    std::vector<Lab> labels;
    const QString hereKey = m_owner->kindOf(sel) == Livre::LK::Terr ? QString() : sel;
    const QString hereAnchor = hereKey.isEmpty() ? QString() : anchorOf(hereKey);
    struct Item { Anchor a; double tx, ty, bx, by; };
    std::vector<Item> items;
    for (const Anchor& a : anchors()) { Item it { a, 0, 0, 0, 0 }; proj(a.x, a.y, a.h, it.tx, it.ty); proj(a.x, a.y, 0, it.bx, it.by); items.push_back(it); }
    std::sort(items.begin(), items.end(), [](const Item& m, const Item& n) { return m.by < n.by; });
    const QColor gold(242, 209, 124, 242);
    for (const Item& it : items) {
        const Anchor& a = it.a;
        const double tx = it.tx, ty = it.ty;
        if (tx < -200 || ty < -200 || tx > W + 200 || ty > H + 200) continue;
        const double R = sysR(a) * cam.z;
        // haste até o chão
        const double stemA = std::min(1.0, sT * 1.5) * (1 - smooth(12, 50, zoomK));
        if (sT > 0.15 && std::fabs(a.h) > 0.002 && stemA > 0.02) {
            g.setPen(QPen(QColor(200, 215, 240, int(255 * 0.25 * stemA)), 1));
            g.drawLine(QPointF(it.bx, it.by), QPointF(tx, ty));
            g.setPen(Qt::NoPen);
            g.setBrush(QColor(200, 215, 240, 89));
            g.drawEllipse(QPointF(it.bx, it.by), 2.2, 2.2 * cT);
        }
        if (!a.solo) {
            const double open = smooth(16, 46, R), sr = std::clamp(3.5 + R * 0.05, 3.5, 22.0);
            if (a.ghost) {
                QPen dp(QColor(230, 236, 245, 140), 1);
                dp.setDashPattern({ 2, 2 });
                g.setPen(dp); g.setBrush(Qt::NoBrush);
                g.drawEllipse(QPointF(tx, ty), 4.5, 4.5);
                g.setPen(Qt::NoPen);
            } else {
                QRadialGradient gr(QPointF(tx, ty), sr * 2.4);
                if (a.sys->kind == StarKind::BuracoNegro) {
                    gr.setColorAt(0, QColor(0, 0, 0)); gr.setColorAt(0.36, QColor(0, 0, 0)); gr.setColorAt(0.42, QColor(255, 176, 112, 242)); gr.setColorAt(1, QColor(255, 120, 50, 0));
                } else {
                    const QColor c = Livre::starShown(a.sys->kind);
                    QColor c1 = c, c2 = c; c1.setAlphaF(0.35); c2.setAlphaF(0);
                    gr.setColorAt(0, Qt::white); gr.setColorAt(0.14, c); gr.setColorAt(0.36, c1); gr.setColorAt(1, c2);
                }
                g.setBrush(gr);
                g.drawEllipse(QPointF(tx, ty), sr * 2.4, sr * 2.4);
            }
            m_hits.push_back({ a.key, QPointF(tx, ty), std::max(8.0, sr * 1.2), false });
            if (a.key == hereKey) { m_here = QPointF(tx, ty); m_hereR = sr * 2.2; m_hasHere = true; }
            bool anyCap = false;
            // de perto, o sistema abre: órbitas e planetas
            std::vector<Body*> ps = planetsOf(*a.sys);
            std::sort(ps.begin(), ps.end(), [](const Body* x, const Body* y) { return x->a < y->a; });
            for (Body* p : ps) if (m_owner->isCapital(keyP(*a.sys, *p))) anyCap = true;
            if (open > 0.01 && !a.ghost) {
                const int n = int(ps.size());
                const double rOut = sysR(a);
                for (int k = 0; k < n; k++) {
                    Body* p = ps[size_t(k)];
                    const QString pk = keyP(*a.sys, *p);
                    const double r = rOut * (0.28 + 0.72 * (k + 1) / std::max(1, n)), rx = r * cam.z;
                    g.setPen(QPen(QColor(200, 215, 240, int(255 * 0.22 * open)), 1));
                    g.setBrush(Qt::NoBrush);
                    g.drawEllipse(QPointF(tx, ty), rx, rx * cT);
                    g.setPen(Qt::NoPen);
                    const double ph = phaseOf(pk);
                    double wx, wy;
                    proj(a.x + std::cos(ph) * r, a.y + std::sin(ph) * r, a.h, wx, wy);
                    const double px = std::clamp(R * 0.14 * (worldPx(*p) / 56), 3.0, 72.0) * (0.6 + 0.4 * open);
                    const bool ghost = m_owner->isGhost(pk);
                    g.setOpacity(open * (ghost ? 0.45 : 1));
                    if (px >= 6) {
                        const int q = px <= 14 ? 16 : px <= 30 ? 32 : 64;
                        g.drawPixmap(QRectF(wx - px / 2, wy - px / 2, px, px), m_owner->ball(pk, q), QRectF());
                    } else { g.setBrush(QColor(170, 180, 195)); g.drawEllipse(QPointF(wx, wy), px / 2, px / 2); }
                    if (m_owner->isCapital(pk)) { g.setPen(QPen(gold, 1.3)); g.setBrush(Qt::NoBrush); g.drawEllipse(QPointF(wx, wy), px / 2 + 3, px / 2 + 3); g.setPen(Qt::NoPen); }
                    g.setOpacity(1);
                    const bool herePk = !hereKey.isEmpty() && (hereKey == pk || m_owner->isInside(hereKey, pk));
                    if (open > 0.5 && !ghost) { m_hits.push_back({ pk, QPointF(wx, wy), std::max(7.0, px / 2 + 2), false }); labels.push_back({ m_owner->label(pk), wx + px / 2 + (herePk ? 12 : 5), wy, 2, open, false }); }
                    if (!hereKey.isEmpty() && (hereKey == pk || m_owner->isInside(hereKey, pk))) { m_here = QPointF(wx, wy); m_hereR = px / 2 + 7; m_hasHere = true; }
                }
            }
            if (anyCap && !a.ghost && open < 0.5) { g.setPen(QPen(gold, 1.2)); g.setBrush(Qt::NoBrush); g.drawEllipse(QPointF(tx, ty), sr + 3.5, sr + 3.5); g.setPen(Qt::NoPen); }
            const bool showName = m_big ? zoomK > 0.9 : (zoomK > 2.2 || open > 0.3);
            if (showName) labels.push_back({ m_owner->label(a.key), tx + (a.key == hereKey ? std::max(sr + 6, sr * 2.2 + 6) : sr + 6), ty, open > 0.3 ? 3 : 1, a.ghost ? 0.5 : 1.0, open > 0.3 });
        } else {
            // mundo solto: a bolinha dele
            const double px = std::clamp(R * 0.42, 4.0, 72.0);
            g.setOpacity(a.ghost ? 0.45 : 1);
            if (px >= 6) {
                const int q = px <= 14 ? 16 : px <= 30 ? 32 : 64;
                g.drawPixmap(QRectF(tx - px / 2, ty - px / 2, px, px), m_owner->ball(a.key, q), QRectF());
            } else { g.setPen(QPen(QColor(3, 5, 10, 230), 1)); g.setBrush(QColor(170, 180, 195)); g.drawEllipse(QPointF(tx, ty), px / 2, px / 2); g.setPen(Qt::NoPen); }
            g.setOpacity(1);
            if (m_owner->isCapital(a.key) && !a.ghost) { g.setPen(QPen(gold, 1.2)); g.setBrush(Qt::NoBrush); g.drawEllipse(QPointF(tx, ty), px / 2 + 3, px / 2 + 3); g.setPen(Qt::NoPen); }
            m_hits.push_back({ a.key, QPointF(tx, ty), std::max(7.0, px / 2 + 2), false });
            const bool here = !hereKey.isEmpty() && (hereKey == a.key || m_owner->isInside(hereKey, a.key));
            if (here) { m_here = QPointF(tx, ty); m_hereR = px / 2 + 7; m_hasHere = true; }
            if (m_big ? zoomK > 0.9 : (zoomK > 2.2 || px > 18)) labels.push_back({ m_owner->label(a.key), tx + px / 2 + (here ? 12 : 5), ty, px > 18 ? 3 : 1, a.ghost ? 0.5 : 1.0, false });
        }
        if (!m_hasHere && a.key == hereAnchor) { m_here = QPointF(tx, ty); m_hereR = 10; m_hasHere = true; }
    }
    // nomes, sem encavalar: os das coisas primeiro, depois os dos territórios
    std::vector<QRectF> boxes;
    const auto put = [&](const QString& text, double x, double y, const QFont& f, const QColor& color, double alpha, bool center) {
        const QFontMetricsF fm(f);
        const double tw = fm.horizontalAdvance(text), th = 14;
        double lx = center ? x - tw / 2 : x;
        if (!center && lx + tw > W - 4) lx = x - tw - 20;
        const QRectF box(lx, y - th / 2, tw, th);
        for (const QRectF& b : boxes) if (b.intersects(box)) return;
        boxes.push_back(box);
        g.setFont(f);
        g.setOpacity(alpha);
        g.setPen(QColor(0, 0, 0, 200));
        g.drawText(QPointF(lx + 0.8, y + fm.ascent() / 2 - 1 + 0.8), text);
        g.setPen(color);
        g.drawText(QPointF(lx, y + fm.ascent() / 2 - 1), text);
        g.setOpacity(1);
    };
    std::stable_sort(labels.begin(), labels.end(), [](const Lab& m, const Lab& n) { return m.pri > n.pri; });
    QFont fs = font(); fs.setPixelSize(11); fs.setWeight(QFont::Medium);
    QFont fb = font(); fb.setPixelSize(12); fb.setWeight(QFont::DemiBold);
    for (const Lab& l : labels) put(l.text, l.x, l.y, l.big ? fb : fs, QColor(236, 241, 250), 0.9 * l.a, false);
    const double tAl = 1 - smooth(8, 22, zoomK);
    if (tAl > 0.02) {
        QFont ft(QStringLiteral("Saira Semi Condensed"));
        ft.setPixelSize(m_big ? 13 : 11);
        ft.setWeight(QFont::DemiBold);
        if (m_big) ft.setLetterSpacing(QFont::AbsoluteSpacing, 3);
        for (const TLab& t : tlabels) put(t.text, t.x, t.y, ft, terrInk(t.color), tAl * t.a, true);
    }
    g.end();
    if (m_hasHere && !m_pulse.isActive()) m_pulse.start();
    if (!m_hasHere) m_pulse.stop();
}
void LivreMap::paintEvent(QPaintEvent*)
{
    if (m_dirty) { m_dirty = false; render(); }
    QPainter p(this);
    p.drawImage(QRectF(0, 0, width(), height()), m_frame);
    if (m_hasHere) {
        // você está aqui: o anel que pulsa
        const QColor acc(Theme::accentDefault());
        const double s = std::max(9.0, m_hereR), ph = double(m_clock.elapsed() % 1800) / 1800.0;
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(Qt::NoBrush);
        QColor glow = acc; glow.setAlphaF(0.35);
        p.setPen(QPen(glow, 5));
        p.drawEllipse(m_here, s, s);
        p.setPen(QPen(acc, 2));
        p.drawEllipse(m_here, s, s);
        QColor pc = acc; pc.setAlphaF(0.8 * (1 - ph));
        p.setPen(QPen(pc, 2));
        p.drawEllipse(m_here, s * (1 + 1.3 * ph), s * (1 + 1.3 * ph));
    }
}

LivreMap::Hit LivreMap::hitAt(QPointF p) const
{
    const Hit* best = nullptr;
    double bd = 1e9;
    for (const Hit& h : m_hits) { const double d = std::hypot(h.at.x() - p.x(), h.at.y() - p.y()); if (d < h.r + 3 && d < bd) { bd = d; best = &h; } }
    if (best) return *best;
    if (!m_doc->gal || cam.z / fitZ() > 25) return {};
    double wx, wy;
    toWorld(p.x(), p.y(), wx, wy);
    for (const auto& r : m_doc->regions) {
        const TerrField F = terrField(*m_doc, *r);
        if (!terrEmpty(F) && terrDist(F, wx, wy) < 0) return { r->key, p, 0, true };
    }
    return {};
}
void LivreMap::mousePressEvent(QMouseEvent* e)
{
    double wx, wy;
    toWorld(e->position().x(), e->position().y(), wx, wy);
    m_down = { true, false, e->button() == Qt::RightButton || (e->modifiers() & Qt::ControlModifier), e->position(), wx, wy, cam.yaw, cam.tilt, e->button() };
    m_fly.on = false;
    m_peekTimer.stop();
}
void LivreMap::mouseMoveEvent(QMouseEvent* e)
{
    const QPointF p = e->position();
    if (m_down.on) {
        if (!m_down.moved && QLineF(p, m_down.at).length() > 4) { m_down.moved = true; m_owner->hidePeek(); m_hover.clear(); setCursor(m_down.rot ? Qt::SizeAllCursor : Qt::ClosedHandCursor); }
        if (!m_down.moved) return;
        if (m_down.rot) {
            cam.yaw = m_down.yaw + (p.x() - m_down.at.x()) * 0.008;
            cam.tilt = std::clamp(m_down.tilt - (p.y() - m_down.at.y()) * 0.006, 0.0, 1.3);
            syncButtons();
        } else {
            double wx, wy;
            toWorld(p.x(), p.y(), wx, wy);
            cam.x += m_down.wx - wx; cam.y += m_down.wy - wy;
        }
        m_dirty = true;
        update();
        return;
    }
    const Hit h = hitAt(p);
    setCursor(h.key.isEmpty() ? Qt::OpenHandCursor : Qt::PointingHandCursor);
    const QString k = h.area ? QString() : h.key;
    if (k == m_hover) return;
    m_hover = k;
    m_peekTimer.stop();
    m_owner->hidePeek();
    if (k.isEmpty()) return;
    m_hot->move(int(h.at.x()) - 6, int(h.at.y()) - 6);
    m_peekTimer.start();
}
void LivreMap::mouseReleaseEvent(QMouseEvent* e)
{
    const Down d = m_down;
    m_down = {};
    if (d.moved) { setCursor(Qt::OpenHandCursor); m_settle.start(); return; }
    if (!d.on || d.btn != Qt::LeftButton || e->button() != Qt::LeftButton) return;
    const Hit h = hitAt(e->position());
    if (h.key.isEmpty()) return;
    m_owner->hidePeek();
    m_hover.clear();
    if (picked) picked(h.key);
}
void LivreMap::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (hitAt(e->position()).key.isEmpty()) fitAll();
}
void LivreMap::wheelEvent(QWheelEvent* e)
{
    e->accept();
    m_fly.on = false;
    m_owner->hidePeek();
    m_hover.clear();
    const QPointF p = e->position();
    double w0x, w0y, w1x, w1y;
    toWorld(p.x(), p.y(), w0x, w0y);
    cam.z = std::clamp(cam.z * std::pow(1.0018, e->angleDelta().y()), fitZ() * 0.5, fitZ() * 600);
    if (cam.z / fitZ() < 4) cam.h *= 0.85;
    toWorld(p.x(), p.y(), w1x, w1y);
    cam.x += w0x - w1x; cam.y += w0y - w1y;
    m_wheelAt.start();
    m_settle.start();
    m_dirty = true;
    update();
}
void LivreMap::leaveEvent(QEvent*)
{
    m_hover.clear();
    m_peekTimer.stop();
    m_owner->hidePeek();
}
void LivreMap::contextMenuEvent(QContextMenuEvent* e) { e->accept(); }

// ── na Criação Livre: a seção do pé da esquerda e a tela grande ──
void Livre::buildMapSection(QVBoxLayout* into)
{
    m_mapSec = new QWidget(m_leftCol);
    m_mapSec->setObjectName(QStringLiteral("lvMapSec"));
    m_mapSec->setAttribute(Qt::WA_StyledBackground, true);
    auto* v = new QVBoxLayout(m_mapSec);
    v->setContentsMargins(10, 7, 10, 10);
    v->setSpacing(6);
    auto* head = new QHBoxLayout();
    head->setSpacing(6);
    m_mapFold = new QPushButton(m_mapSec);
    m_mapFold->setObjectName(QStringLiteral("lvMapTitle"));
    m_mapFold->setCursor(Qt::PointingHandCursor);
    connect(m_mapFold, &QPushButton::clicked, this, [this]() {
        const bool closed = !QSettings().value(QStringLiteral("galaxy/livreMapClosed"), false).toBool();
        QSettings().setValue(QStringLiteral("galaxy/livreMapClosed"), closed);
        syncMap();
    });
    head->addWidget(m_mapFold);
    m_mapSub = new QLabel(m_mapSec);
    m_mapSub->setObjectName(QStringLiteral("lvMapSub"));
    m_mapSub->setMinimumWidth(10);
    head->addWidget(m_mapSub, 1);
    auto* ex = new QToolButton(m_mapSec);
    ex->setObjectName(QStringLiteral("lvMapExp"));
    ex->setIcon(carto::icon(QStringLiteral("expand"), QColor(Theme::textMuted()), QColor(Theme::accentDefault()), 13));
    ex->setToolTip(tr("Ver em tela grande"));
    ex->setCursor(Qt::PointingHandCursor);
    connect(ex, &QToolButton::clicked, this, [this]() { openBigMap(); });
    head->addWidget(ex);
    v->addLayout(head);
    m_map = new LivreMap(this, m_doc, false, m_mapSec);
    m_map->setFixedHeight(200);
    m_map->picked = [this](const QString& k) { select(k); };
    v->addWidget(m_map);
    into->addWidget(m_mapSec);
}
std::vector<std::pair<QString, QString>> Livre::mapLinks() const
{
    std::vector<std::pair<QString, QString>> out;
    auto* places = m_panel->m_link.places;
    if (!places) return out;
    for (const auto& l : places->links()) out.push_back({ m_panel->keyOfPlace(l.fromTerritorioId), m_panel->keyOfPlace(l.toTerritorioId) });
    return out;
}
void Livre::syncMap()
{
    if (!m_mapSec || !m_doc->gal) return;
    const bool closed = QSettings().value(QStringLiteral("galaxy/livreMapClosed"), false).toBool();
    m_mapFold->setText((closed ? QStringLiteral("▸  ") : QStringLiteral("▾  ")) + tr("Mapa"));
    m_mapFold->setToolTip(closed ? tr("Mostrar o mapa") : tr("Dobrar o mapa"));
    int ns = 0, nw = 0;
    for (System* s : livreSystems()) if (!isGhost(keyS(*s))) ns++;
    for (const auto& s : m_doc->gal->systems) if (!s->isCore && m_doc->isSolo(*s)) for (Body* p : planetsOf(*s)) if (!isGhost(keyP(*s, *p))) nw++;
    QStringList bits;
    if (ns) bits << (ns == 1 ? tr("1 sistema") : tr("%1 sistemas").arg(ns));
    if (nw) bits << (nw == 1 ? tr("1 mundo solto") : tr("%1 mundos soltos").arg(nw));
    m_mapSub->setText(bits.join(QStringLiteral(" · ")));
    m_map->setVisible(!closed);
    if (!closed) { m_map->follow(m_sel); m_map->changed(); }
    if (m_bigMap && m_bigMapView) {
        m_bigMapView->follow(m_sel);
        m_bigMapView->changed();
        const bool any = !m_sel.isEmpty();
        m_bigMapGo->setVisible(any);
        if (any) m_bigMapGo->setText(tr("Abrir %1").arg(label(m_sel)) + QStringLiteral(" ›"));
    }
}
namespace {
QPixmap legendMark(const QString& kind)
{
    QPixmap pm(28, 28);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c(7, 7);
    if (kind == QLatin1String("star")) {
        QRadialGradient gr(c, 6); gr.setColorAt(0, Qt::white); gr.setColorAt(0.3, QColor(255, 207, 90)); gr.setColorAt(1, QColor(255, 207, 90, 0));
        p.setPen(Qt::NoPen); p.setBrush(gr); p.drawEllipse(c, 6, 6);
    } else if (kind == QLatin1String("world")) {
        p.setPen(QPen(QColor(4, 6, 11), 1)); p.setBrush(QColor(127, 178, 230)); p.drawEllipse(c, 3.5, 3.5);
    } else if (kind == QLatin1String("cap")) {
        p.setPen(QPen(QColor(242, 209, 124), 1.4)); p.setBrush(Qt::NoBrush); p.drawEllipse(c, 4.5, 4.5);
    } else if (kind == QLatin1String("ghost")) {
        QPen pen(QColor(230, 236, 245, 190), 1); pen.setDashPattern({ 2, 2 }); p.setPen(pen); p.setBrush(Qt::NoBrush); p.drawEllipse(c, 4.5, 4.5);
    } else {
        QPen pen(QColor(140, 200, 255, 210), 1.4); pen.setDashPattern({ 2, 1.6 }); p.setPen(pen); p.drawLine(QPointF(0.5, 7), QPointF(13.5, 7));
    }
    return pm;
}
} // namespace
void Livre::openBigMap()
{
    if (m_bigMap) return;
    closePop();
    hidePeek();
    m_bigMap = new QFrame(this);
    m_bigMap->setObjectName(QStringLiteral("lvMapOv"));
    auto* v = new QVBoxLayout(m_bigMap);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto* bar = new QWidget(m_bigMap);
    bar->setObjectName(QStringLiteral("lvMapBar"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(16, 9, 12, 9);
    h->setSpacing(10);
    auto* title = new QLabel(tr("Mapa da galáxia"), bar);
    title->setObjectName(QStringLiteral("lvMapBigTitle"));
    h->addWidget(title);
    auto* sub = new QLabel(tr("só o que você criou"), bar);
    sub->setObjectName(QStringLiteral("lvMapSub"));
    h->addWidget(sub);
    h->addStretch(1);
    const std::pair<const char*, const char*> legend[] = { { "star", QT_TR_NOOP("Sistema") }, { "world", QT_TR_NOOP("Mundo solto") }, { "cap", QT_TR_NOOP("Capital") }, { "ghost", QT_TR_NOOP("Sugerido") }, { "link", QT_TR_NOOP("Conexão") } };
    for (const auto& [k, t] : legend) {
        auto* ic = new QLabel(bar);
        ic->setPixmap(legendMark(QString::fromLatin1(k)));
        h->addWidget(ic);
        auto* tl = new QLabel(tr(t), bar);
        tl->setObjectName(QStringLiteral("lvMapSub"));
        h->addWidget(tl);
        h->addSpacing(6);
    }
    auto* x = new QToolButton(bar);
    x->setObjectName(QStringLiteral("lvMapExp"));
    x->setIcon(carto::icon(QStringLiteral("close"), QColor(Theme::textPrimary()), QColor(Theme::accentDefault()), 14));
    x->setToolTip(tr("Fechar (Esc)"));
    x->setCursor(Qt::PointingHandCursor);
    connect(x, &QToolButton::clicked, this, [this]() { QTimer::singleShot(0, this, &Livre::closeBigMap); });
    h->addWidget(x);
    v->addWidget(bar);
    m_bigMapView = new LivreMap(this, m_doc, true, m_bigMap);
    m_bigMapView->picked = [this](const QString& k) { m_sel = valid(k) ? k : QString(); render(); };
    v->addWidget(m_bigMapView, 1);
    auto* foot = new QWidget(m_bigMap);
    foot->setObjectName(QStringLiteral("lvMapFoot"));
    foot->setAttribute(Qt::WA_StyledBackground, true);
    auto* fh = new QHBoxLayout(foot);
    fh->setContentsMargins(16, 8, 16, 10);
    fh->setSpacing(12);
    fh->addStretch(1);
    auto* hint = new QLabel(tr("Clique pra ir até lá. Rodinha aproxima, arrastar move, botão direito (ou Ctrl) gira e inclina, dois cliques enquadram tudo."), foot);
    hint->setObjectName(QStringLiteral("lvMapHint"));
    fh->addWidget(hint);
    m_bigMapGo = new QPushButton(foot);
    m_bigMapGo->setObjectName(QStringLiteral("lvMapGo"));
    m_bigMapGo->setCursor(Qt::PointingHandCursor);
    connect(m_bigMapGo, &QPushButton::clicked, this, [this]() { QTimer::singleShot(0, this, &Livre::closeBigMap); });
    fh->addWidget(m_bigMapGo);
    fh->addStretch(1);
    v->addWidget(foot);
    m_bigMap->setGeometry(rect());
    m_bigMap->show();
    m_bigMap->raise();
    syncMap();
    m_bigMapView->setFocus();
}
void Livre::closeBigMap()
{
    if (!m_bigMap) return;
    hidePeek();
    m_bigMap->hide();
    m_bigMap->deleteLater();
    m_bigMap = nullptr;
    m_bigMapView = nullptr;
    m_bigMapGo = nullptr;
}

} // namespace galaxy
