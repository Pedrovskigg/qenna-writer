#include "GalaxyCanvas.h"
#include "GalaxyBlackHole.h"
#include "GalaxyModels.h"

#include <QFontDatabase>
#include <QPointer>
#include <QThreadPool>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>

namespace galaxy {

namespace {
const V3 FRONT = [] { const double x = 0.18, y = 0.14, z = 0.97, n = std::sqrt(x * x + y * y + z * z); return V3 { x / n, y / n, z / n }; }();
inline QColor rgbaC(double r, double g, double b, double a = 1)
{
    const auto c = [](double v) { return int(v < 0 ? 0 : v > 255 ? 255 : v); };
    return QColor(c(r), c(g), c(b), int(clamp(a, 0, 1) * 255 + 0.5));
}
inline QColor rgb3(const std::array<int, 3>& c, double a = 1) { return rgbaC(c[0], c[1], c[2], a); }
inline void ellipseAt(QPainter& p, double x, double y, double r, double cT) { p.drawEllipse(QPointF(x, y), r, std::max(0.5, r * cT)); }
std::array<int, 3> typeColor(Type t)
{
    switch (t) {
    case Type::Temperado: return { 70, 150, 110 }; case Type::Oceano: return { 50, 110, 180 }; case Type::Deserto: return { 200, 150, 90 }; case Type::Arido: return { 140, 132, 124 };
    case Type::Lava: return { 210, 90, 50 }; case Type::Gelado: return { 205, 222, 234 }; case Type::Toxico: return { 200, 178, 76 }; case Type::Anao: return { 166, 156, 146 };
    case Type::Gigante: return { 202, 165, 122 }; case Type::Quente: return { 196, 106, 64 }; case Type::GeloG: return { 111, 179, 204 }; case Type::Subnetuno: return { 147, 166, 204 };
    case Type::Rochosa: return { 154, 148, 142 }; case Type::Gelada: return { 214, 228, 236 }; case Type::Vulcanica: return { 220, 190, 90 }; case Type::Temperada: return { 70, 150, 110 };
    default: return { 150, 150, 150 };
    }
}
Body liteBody(const Body& b)
{
    Body c;
    c.body = b.body; c.type = b.type; c.look = b.look;
    if (b.star) c.star = std::make_unique<Star>(*b.star);
    return c;
}
} // namespace

Canvas::Canvas(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);
    m_clock.start();
    m_lastT = m_clock.elapsed();
    connect(&m_timer, &QTimer::timeout, this, &Canvas::tick);
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.start(16);
}
Canvas::~Canvas() = default;

void Canvas::setDoc(Doc* d)
{
    doc = d;
    clearCaches();
    undoStack.clear(); redoStack.clear();
    sel = QStringLiteral("GAL");
    focusKey.clear(); m_focusShown.clear(); m_focusAmt = 0;
    if (doc && doc->galImg.width() > 0) m_galSmall = doc->galImg.scaled(700, 700, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    cam.x = 0; cam.y = 0; cam.h = 0; cam.tilt = 0; cam.yaw = 0; cam.follow.clear();
    m_fly.reset(); m_tiltAnim.reset();
    cam.z = fitZ();
    m_dirty = true;
    update();
}
void Canvas::clearCaches()
{
    m_tex.clear(); m_thumbs.clear(); m_thumbQ.clear(); m_artThumbs.clear(); m_ringTex.clear(); m_live.clear();
    m_terr.clear(); m_prims.clear(); m_toned.clear(); m_stamps.clear();
    m_tr.key.clear(); m_tr.sys.clear(); m_tr.routes.clear();
    m_fx.key.clear(); m_bgKey.clear(); m_art.key.clear(); m_art.per.clear(); m_art.gal = nullptr;
    m_previews.clear();
}
void Canvas::docChanged()
{
    m_tr.key.clear(); m_tr.sys.clear(); m_fx.key.clear(); m_prims.clear();
    m_dirty = true;
    update();
}
void Canvas::invalidateBody(const Body& b)
{
    const QString k = texKey(b);
    m_tex.remove(k); m_live.remove(k); m_ringTex.remove(k);
    const QString pre = k + QLatin1Char(':');
    for (auto it = m_thumbs.begin(); it != m_thumbs.end();) it = it.key().startsWith(pre) ? m_thumbs.erase(it) : std::next(it);
    m_dirty = true;
}
void Canvas::refreshAfterEdit(Body* b)
{
    if (b) invalidateBody(*b);
    m_tr.key.clear(); m_tr.sys.clear();
    m_dirty = true;
    update();
}

// ── câmera ──
QPointF Canvas::viewCenter() const
{
    const double left = width() > 760 ? padLeft : 0, bottom = height() - padBottom;
    return QPointF((left + width()) / 2.0, bottom / 2.0);
}
double Canvas::fitZ() const
{
    const double left = width() > 760 ? padLeft : 0, bottom = height() - padBottom;
    return std::max(10.0, std::min(width() - left - 40, bottom - 40) / 2.25);
}
void Canvas::proj(double x, double y, double h, double& ox, double& oy) const
{
    const QPointF c = viewCenter();
    const double dx = x - cam.x, dy = y - cam.y, cy = std::cos(cam.yaw), sy = std::sin(cam.yaw);
    const double X = dx * cy - dy * sy, Y = dx * sy + dy * cy;
    ox = c.x() + X * cam.z;
    oy = c.y() - (Y * std::cos(cam.tilt) + (h - cam.h) * std::sin(cam.tilt)) * cam.z;
}
void Canvas::toWorld(double sx, double sy, double& wx, double& wy) const
{
    const QPointF ct = viewCenter();
    const double X = (sx - ct.x()) / cam.z, Y = -(sy - ct.y()) / (cam.z * std::max(0.2, std::cos(cam.tilt)));
    const double c = std::cos(cam.yaw), s = std::sin(cam.yaw);
    wx = cam.x + X * c + Y * s;
    wy = cam.y - X * s + Y * c;
}
// Voo como num mapa de ruas: se o destino é longe, afasta no meio do caminho e aproxima de novo.
void Canvas::flyTo(double x, double y, double z, double ms, const QString& follow, double h)
{
    const double z0 = cam.z, x0 = cam.x, y0 = cam.y, h0 = cam.h;
    const double d = std::hypot(x - x0, y - y0);
    const double span = std::max(1.0, (d * std::min(z0, z)) / (width() * 0.45));
    auto f = std::make_unique<Fly>();
    f->t0 = m_clock.elapsed(); f->ms = ms + std::min(700.0, std::log2(span) * 120); f->x0 = x0; f->y0 = y0; f->z0 = z0; f->h0 = h0; f->x = x; f->y = y; f->z = z; f->h = h; f->bump = std::log(span);
    m_fly = std::move(f);
    cam.follow = follow;
}
double Canvas::heightOf(const QString& key) const { const Ref o = doc->resolve(key); return o.sys ? o.sys->layH : 0; }
bool Canvas::stepFly(qint64 now)
{
    bool hasP = false;
    double fx = 0, fy = 0;
    if (!cam.follow.isEmpty()) {
        const Ref o = doc->resolve(cam.follow);
        if (o.kind == RefKind::Planet) { fx = o.body->layX; fy = o.body->layY; hasP = true; }
        else if (o.kind == RefKind::Moon) { fx = o.moon->layX; fy = o.moon->layY; hasP = true; }
        else if (o.kind == RefKind::Build && o.build && !structOf(o.build->type)->mega) { buildPos(*o.build, o.sys, o.host, fx, fy); hasP = true; }
    }
    if (!m_fly) {
        if (hasP) { cam.x = fx; cam.y = fy; cam.h = heightOf(cam.follow); return true; }
        return false;
    }
    if (hasP) { m_fly->x = fx; m_fly->y = fy; }
    const double k = std::min(1.0, double(now - m_fly->t0) / m_fly->ms), e = k < 0.5 ? 4 * k * k * k : 1 - std::pow(-2 * k + 2, 3) / 2;
    const double lz = std::log(m_fly->z0) + (std::log(m_fly->z) - std::log(m_fly->z0)) * e - m_fly->bump * std::sin(PI_D * e) * 0.9;
    cam.z = std::exp(lz);
    cam.x = m_fly->x0 + (m_fly->x - m_fly->x0) * e; cam.y = m_fly->y0 + (m_fly->y - m_fly->y0) * e; cam.h = m_fly->h0 + (m_fly->h - m_fly->h0) * e;
    if (k >= 1) m_fly.reset();
    return true;
}
void Canvas::setTilt(double t, double yaw)
{
    auto a = std::make_unique<TiltAnim>();
    a->t0 = m_clock.elapsed(); a->from = cam.tilt; a->to = t; a->yf = cam.yaw; a->yt = yaw;
    m_tiltAnim = std::move(a);
}
bool Canvas::stepTilt(qint64 now)
{
    if (!m_tiltAnim) return false;
    const double k = std::min(1.0, double(now - m_tiltAnim->t0) / 600), e = 1 - std::pow(1 - k, 3);
    cam.tilt = m_tiltAnim->from + (m_tiltAnim->to - m_tiltAnim->from) * e;
    cam.yaw = m_tiltAnim->yf + (m_tiltAnim->yt - m_tiltAnim->yf) * e;
    if (k >= 1) m_tiltAnim.reset();
    emit cameraMoved();
    return true;
}
void Canvas::frameOfKey(const QString& key, double& x, double& y, double& z)
{
    const Ref o = doc->resolve(key);
    const double left = width() > 760 ? padLeft : 0, bottom = height() - padBottom, m = std::min(width() - left, bottom);
    x = 0; y = 0; z = fitZ();
    switch (o.kind) {
    case RefKind::Region: {
        double x0 = 9, x1 = -9, y0 = 9, y1 = -9;
        // O enquadramento é do território inteiro, com a fronteira longe da etiqueta e dos botões.
        const double pad = 0.06 + 0.012;
        for (int id : o.region->systems) { const System& s = *doc->gal->systems[size_t(id)]; x0 = std::min(x0, s.x - pad); x1 = std::max(x1, s.x + pad); y0 = std::min(y0, s.y - pad); y1 = std::max(y1, s.y + pad); }
        x = (x0 + x1) / 2; y = (y0 + y1) / 2; z = std::min((width() - left - 150) / (x1 - x0), (bottom - 130) / (y1 - y0));
        break;
    }
    case RefKind::System: x = o.sys->x; y = o.sys->y; z = (m * 0.42) / o.sys->layR; break;
    case RefKind::Planet: x = o.body->layX; y = o.body->layY; z = (m * 0.3) / o.body->layExt; break;
    case RefKind::Moon: x = o.moon->layX; y = o.moon->layY; z = (m * 0.12) / std::max(o.moon->layR, 1e-6); break;
    case RefKind::Comet: case RefKind::Belt: frameOfKey(keyS(*o.sys), x, y, z); break;
    case RefKind::Build: {
        const StructDef* def = structOf(o.build->type);
        if (def->mega) { frameOfKey(o.host ? keyP(*o.sys, *o.host) : keyS(*o.sys), x, y, z); break; }
        buildPos(*o.build, o.sys, o.host, x, y);
        z = (m * 0.2) / buildRadiusOf(*o.build, o.sys, o.host);
        break;
    }
    case RefKind::Group: frameOfKey(o.group->parent, x, y, z); break;
    default: break;
    }
}
void Canvas::select(const QString& key0, bool flyThere)
{
    QString key = key0;
    if (!doc->resolve(key)) key = QStringLiteral("GAL");
    sel = key;
    const Ref o = doc->resolve(key);
    // Clicar numa região foca nela; ir pra fora dela sai do foco; descer pra dentro mantém.
    if (flyThere && o.kind == RefKind::Region && !o.region->systems.isEmpty()) setFocus2(key);
    else if (!focusKey.isEmpty() && !doc->pathOf(key).contains(focusKey)) setFocus2(QString());
    if (flyThere) {
        double x, y, z;
        frameOfKey(key, x, y, z);
        const bool follow = o.kind == RefKind::Planet || o.kind == RefKind::Moon || (o.kind == RefKind::Build && !structOf(o.build->type)->mega);
        flyTo(x, y, std::min(zMax(), std::max(zMin(), z)), 950, follow ? key : QString(), heightOf(key));
    }
    m_dirty = true;
    emit selectionChanged(sel);
    update();
}
void Canvas::fitMap()
{
    double x, y, z;
    frameOfKey(focusKey.isEmpty() ? QStringLiteral("GAL") : focusKey, x, y, z);
    cam.follow.clear();
    flyTo(x, y, std::max(zMin(), z), 800);
}
void Canvas::setFocus2(const QString& key)
{
    if (key == focusKey) return;
    if (!key.isEmpty() && !focusKey.isEmpty()) m_focusAmt = std::min(m_focusAmt, 0.35);   // de um império direto pro outro: refoca rápido
    focusKey = key;
    if (!key.isEmpty()) m_focusShown = key;
    m_dirty = true;
    emit focusChanged();
}
bool Canvas::stepFocus(double dt)
{
    const double target = focusKey.isEmpty() ? 0 : 1;
    if (m_focusAmt == target) return false;
    const double step = dt / 0.48;
    m_focusAmt = target > m_focusAmt ? std::min(1.0, m_focusAmt + step) : std::max(0.0, m_focusAmt - step);
    if (m_focusAmt == 0) m_focusShown.clear();
    return true;
}
void Canvas::setView(const QString& m)
{
    if (skinOf(m)) artMode = m;
    view = m;
    m_sk = skinOf(m);
    if (m_sk) { const Skin* sk = m_sk; galaxy::setTone([sk](uint8_t* d, int w, int h) { stylize(d, w, h, *sk, true); }, sk->id); }
    else galaxy::setTone(nullptr, QString());
    m_fx.key.clear(); m_tr.key.clear(); m_bgKey.clear(); m_art.key.clear();
    m_dirty = true;
    update();
}
void Canvas::tick()
{
    if (!doc || !doc->gal || cdOpen() || !isVisible()) return;
    const qint64 now = m_clock.elapsed();
    const double dt = std::min(0.1, double(now - m_lastT) / 1000);
    m_lastT = now;
    if (play) doc->simT += dt;
    const bool a = stepTilt(now), b = stepFly(now), c = stepFocus(dt);
    const bool moving = a || b || c;
    if (moving) { emit cameraMoved(); }
    if (play || moving || m_dirty || !m_thumbQ.empty()) { m_dirty = false; update(); }
}
void Canvas::tickNow() { tick(); repaint(); }
void Canvas::resizeEvent(QResizeEvent*)
{
    m_dirty = true;
    if (doc && doc->gal && cam.z < zMin()) cam.z = fitZ();
}

QColor Canvas::accent(double a) const { return m_sk ? m_sk->selC(a) : QColor(140, 200, 255, int(a * 255 + 0.5)); }
QFont Canvas::uiFont(int weight, double px) const
{
    QFont f = font();
    f.setPixelSize(int(px + 0.5));
    f.setWeight(QFont::Weight(weight));
    return f;
}

// ── texturas e miniaturas ──
// As texturas grandes saem em segundo plano. Miniatura é pintada de frente, e a sombra (lado de lá da estrela)
// vai por cima a cada quadro; de perto, o planeta vira esfera de verdade, girando, com cidades no lado da noite.
QString Canvas::texKey(const Body& b) const { return QStringLiteral("%1:%2:%3").arg(int(b.body)).arg(b.look.seed).arg(b.texRev); }
void Canvas::requestTexture(const Body& b, int w, TexEntry& e, const QString& k)
{
    if (m_texRunning >= 2) return;
    e.pend = w;
    m_texRunning++;
    auto lite = std::make_shared<Body>(liteBody(b));
    QPointer<Canvas> self(this);
    QThreadPool::globalInstance()->start([self, lite, k, w]() {
        auto tex = std::make_shared<Texture>(bodyTexture(*lite, w, w / 2));
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, tex, k, w]() {
            if (!self) return;
            self->m_texRunning--;
            auto it = self->m_tex.find(k);
            if (it != self->m_tex.end()) { if (w > it->bw) { it->best = tex; it->bw = w; } it->pend = 0; }
            self->m_dirty = true;
        }, Qt::QueuedConnection);
    });
}
const Texture& Canvas::textureOf(const Body& b, int w)
{
    const QString k = texKey(b);
    TexEntry& e = m_tex[k];
    if (!e.best) { e.best = std::make_shared<Texture>(bodyTexture(b, 128, 64)); e.bw = 128; }
    if (w > e.bw && w > e.pend) requestTexture(b, w, e, k);
    return *e.best;
}
const QImage* Canvas::thumb(const Body& b, int D)
{
    const QString lk = texKey(b) + QLatin1Char(':') + QString::number(D);
    const int want = D <= 40 ? 128 : D <= 110 ? 256 : 512;
    ThumbEntry& t = m_thumbs[lk];
    if (!t.img.isNull() && (t.texW >= want || t.queued)) return &t.img;
    if (!t.queued) { t.queued = true; m_thumbQ.push_back({ lk, &b, D, want }); }
    return t.img.isNull() ? nullptr : &t.img;
}
void Canvas::pumpThumbs(double budgetMs)
{
    QElapsedTimer t0;
    t0.start();
    while (!m_thumbQ.empty() && double(t0.elapsed()) < budgetMs) {
        const ThumbJob j = m_thumbQ.front();
        m_thumbQ.pop_front();
        auto it = m_thumbs.find(j.key);
        if (it == m_thumbs.end()) continue;
        const Texture& tex = textureOf(*j.b, j.want);
        const int tw = tex.w;
        const SphereTable tab = sphereTable(j.D, 0, 0.3, j.b->frame.ax[2] != 0 || j.b->frame.ax[0] != 0 ? j.b->frame : frameOf(0, 0));
        QImage img(j.D, j.D, QImage::Format_RGBA8888);
        img.fill(Qt::transparent);
        shadeSphere(img.bits(), tab, tex, 0.6, &FRONT, atmOf(*j.b));
        it->img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        it->texW = tw; it->gen++;
        it->queued = false;
        if (tw < j.want) m_dirty = true;
        m_dirty = true;
    }
}
QImage Canvas::smallBall(const Body& b, int px, const V3& L, double dprV)
{
    const int D = int(px * std::min(2.0, dprV) + 0.5);
    const SphereTable tab = sphereTable(D, 0, 20 * PI_D / 180, b.frame.ax[2] != 0 || b.frame.ax[0] != 0 ? b.frame : frameOf(0, 0));
    QImage img(D, D, QImage::Format_RGBA8888);
    img.fill(Qt::transparent);
    const double n = std::sqrt(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]);
    const V3 Ln { L[0] / n, L[1] / n, L[2] / n };
    shadeSphere(img.bits(), tab, textureOf(b, 128), 0.6, &Ln, atmOf(b));
    QImage out = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    out.setDevicePixelRatio(D / double(px));
    return out;
}
// Esfera viva: só pros corpos grandes na tela (no máximo dois por quadro).
void Canvas::drawLive(QPainter& p, const Body& b, double x, double y, double r, double ux, double uy)
{
    const int D = int(std::min(560.0, std::max(64.0, std::floor((r * 2 * std::min(m_dpr, 1.25)) / 32 + 0.5) * 32)));
    const QString k = texKey(b);
    const double pitch = std::floor((b.spunByUser ? b.pitchU : 0.3) * 40 + 0.5) / 40;
    LiveEntry& L = m_live[k];
    if (L.D != D || L.pitch != pitch) {
        L.D = D; L.pitch = pitch;
        L.tab = sphereTable(D, 0, pitch, b.frame.ax[2] != 0 || b.frame.ax[0] != 0 ? b.frame : frameOf(0, 0));
        L.img = QImage(D, D, QImage::Format_RGBA8888);
    }
    const Texture& tex = textureOf(b, D > 300 ? 1024 : 512);
    const double vz = 0.32, n = std::sqrt(ux * ux + uy * uy + vz * vz);
    const V3 v { ux / n, -uy / n, vz / n };
    const Atm* atm = atmOf(b);
    shadeSphere(L.img.bits(), L.tab, tex, b.spinU + doc->simT * SPEED_SPIN * (b.retro ? -1 : 1), &v, atm);
    if (m_sk) stylize(L.img.bits(), D, D, *m_sk, false, 0.14);
    if (!m_sk && atm) {
        QRadialGradient gr(x, y, r * 1.07);
        const double t0 = 0.98 / 1.07;
        const auto ac = atm->c;
        gr.setColorAt(t0, rgbaC(ac[0] * 255, ac[1] * 255, ac[2] * 255, 0.26 * atm->k));
        gr.setColorAt(1, rgbaC(ac[0] * 255, ac[1] * 255, ac[2] * 255, 0));
        gr.setColorAt(0, QColor(0, 0, 0, 0));
        gr.setColorAt(t0 - 0.0001, QColor(0, 0, 0, 0));
        p.setPen(Qt::NoPen); p.setBrush(gr);
        p.drawEllipse(QPointF(x, y), r * 1.07, r * 1.07);
    }
    p.drawImage(QRectF(x - r, y - r, r * 2, r * 2), L.img);
}
void Canvas::drawBody(QPainter& p, const Body& b, double x, double y, double r, double ux, double uy)
{
    if (m_sk) { drawArtBody(p, b, x, y, r, ux, uy); return; }
    if (r < 4) {
        p.setPen(Qt::NoPen); p.setBrush(rgb3(typeColor(b.type)));
        p.drawEllipse(QPointF(x, y), r, r);
        shadeOver(p, x, y, r, ux, uy, 0.75);
        return;
    }
    if (r > 70 && m_liveBudget > 0 && x + r > 0 && x - r < width() && y + r > 0 && y - r < height()) {
        m_liveBudget--;
        drawLive(p, b, x, y, r, ux, uy);
        m_liveHits.push_back({ const_cast<Body*>(&b), nullptr, x, y, r });
        return;
    }
    const double want = std::min(512.0, r * 2 * std::min(m_dpr, 1.5));
    int D = 512;
    for (int v : { 16, 32, 64, 128, 256, 512 }) if (v >= want) { D = v; break; }
    const QImage* img = thumb(b, D);
    if (img) p.drawImage(QRectF(x - r, y - r, r * 2, r * 2), *img);
    else { p.setPen(Qt::NoPen); p.setBrush(rgb3(typeColor(b.type))); p.drawEllipse(QPointF(x, y), r, r); }
    shadeOver(p, x, y, r, ux, uy, 0.88);
}
// Sombra do lado oposto à estrela, por cima da miniatura.
void Canvas::shadeOver(QPainter& p, double x, double y, double r, double ux, double uy, double k)
{
    QLinearGradient g(x + ux * r, y + uy * r, x - ux * r, y - uy * r);
    g.setColorAt(0, QColor(2, 3, 8, 0)); g.setColorAt(0.42, rgbaC(2, 3, 8, 0.04)); g.setColorAt(0.62, rgbaC(2, 3, 8, 0.6 * k)); g.setColorAt(1, rgbaC(2, 3, 8, k));
    p.setPen(Qt::NoPen); p.setBrush(g);
    p.drawEllipse(QPointF(x, y), r + 0.5, r + 0.5);
}
// Anéis de verdade (textura do núcleo), em duas metades: a de trás antes do planeta, a da frente depois.
const QImage& Canvas::ringTex(const Body& b)
{
    const QString k = texKey(b);
    auto it = m_ringTex.find(k);
    if (it != m_ringTex.end()) return it.value();
    const Rings& rg = *b.rings;
    const int S = 512, n = 48;
    QImage img(S, S, QImage::Format_RGBA8888);
    img.fill(Qt::transparent);
    const double k0 = rg.inner / rg.outer;
    for (int j = 0; j < S; j++) {
        uchar* d = img.scanLine(j);
        for (int i = 0; i < S; i++) {
            const double x = ((i + 0.5) / S) * 2 - 1, y = ((j + 0.5) / S) * 2 - 1, rr = std::hypot(x, y);
            if (rr > 1 || rr < k0) continue;
            const double t = ((rr - k0) / (1 - k0)) * (n - 1);
            const int i0 = int(t);
            const double f = t - i0;
            double v = double(rg.bands[size_t(i0)]) * (1 - f) + double(rg.bands[size_t(std::min(n - 1, i0 + 1))]) * f;
            v *= 0.8 + 0.2 * std::sin(rr * 300) * std::sin(rr * 71);
            d[i * 4] = uchar(rg.tint[0] * 255); d[i * 4 + 1] = uchar(rg.tint[1] * 255); d[i * 4 + 2] = uchar(rg.tint[2] * 255); d[i * 4 + 3] = uchar(std::min(1.0, v * rg.alpha) * 255);
        }
    }
    return m_ringTex.insert(k, img.convertToFormat(QImage::Format_ARGB32_Premultiplied)).value();
}
void Canvas::ringOn(QPainter& g, const Body& b, double x, double y, double r, bool front, double ky)
{
    const double rx = b.rings->outer * r;
    g.save();
    g.translate(x, y);
    g.rotate(-0.32 * 180 / PI_D);
    g.setClipRect(front ? QRectF(-rx - 2, 0, rx * 2 + 4, rx + 2) : QRectF(-rx - 2, -rx - 2, rx * 2 + 4, rx + 2), Qt::IntersectClip);
    g.scale(1, ky);
    g.drawImage(QRectF(-rx, -rx, rx * 2, rx * 2), ringTex(b));
    g.restore();
}
void Canvas::drawRing(QPainter& p, const Body& b, double x, double y, double r, bool front)
{
    const double rx = b.rings->outer * r, ky = std::max(0.2, std::min(0.55, std::fabs(std::sin(((b.tilt != 0 ? b.tilt : 20) + 18) * PI_D / 180)))) * (0.75 + 0.25 * std::cos(cam.tilt));
    p.save();
    p.translate(x, y);
    p.rotate(-0.32 * 180 / PI_D);
    p.setClipRect(front ? QRectF(-rx - 2, 0, rx * 2 + 4, rx + 2) : QRectF(-rx - 2, -rx - 2, rx * 2 + 4, rx + 2), Qt::IntersectClip);
    if (m_sk) {
        // O anel em traço: umas poucas voltas, mais cheias perto do planeta.
        const double k0 = b.rings->inner / b.rings->outer;
        const int n = std::max(3, std::min(9, int(std::floor(rx / 12 + 0.5))));
        QPainterPath band;
        band.addEllipse(QPointF(0, 0), rx, rx * ky);
        band.addEllipse(QPointF(0, 0), rx * k0, rx * k0 * ky);
        p.setPen(Qt::NoPen);
        QColor pc = m_sk->paperC(); pc.setAlphaF(0.45);
        p.setBrush(pc);
        p.drawPath(band);
        QPen pen(m_sk->inkC(0.8)); pen.setWidthF(0.8);
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        for (int i = 0; i <= n; i++) { const double q = rx * (k0 + (1 - k0) * (double(i) / n)); p.drawEllipse(QPointF(0, 0), q, q * ky); }
        p.restore();
        return;
    }
    p.scale(1, ky);
    p.drawImage(QRectF(-rx, -rx, rx * 2, rx * 2), ringTex(b));
    p.restore();
}
const QImage& Canvas::glow(const std::array<int, 3>& c)
{
    const QString k = QStringLiteral("%1,%2,%3").arg(c[0]).arg(c[1]).arg(c[2]);
    auto it = m_glow.find(k);
    if (it != m_glow.end()) return it.value();
    QImage g(64, 64, QImage::Format_ARGB32_Premultiplied);
    g.fill(Qt::transparent);
    QPainter x(&g);
    QRadialGradient gr(32, 32, 32);
    gr.setColorAt(0, QColor(255, 255, 255, 255)); gr.setColorAt(0.18, rgb3(c, 0.95)); gr.setColorAt(0.45, rgb3(c, 0.28)); gr.setColorAt(1, rgb3(c, 0));
    x.fillRect(g.rect(), gr);
    x.end();
    return m_glow.insert(k, g).value();
}
void Canvas::ring(QPainter& p, double x, double y, double r)
{
    QPen pen(accent(0.95)); pen.setWidthF(1.5);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(x, y), r, r);
}

// ── desenho ──
void Canvas::paintEvent(QPaintEvent*)
{
    if (!doc || !doc->gal) { QPainter p(this); p.fillRect(rect(), QColor(3, 5, 10)); return; }
    m_dpr = devicePixelRatioF();
    const int PW = int(width() * m_dpr + 0.5), PH = int(height() * m_dpr + 0.5);
    if (m_frame.width() != PW || m_frame.height() != PH) { m_frame = QImage(PW, PH, QImage::Format_ARGB32_Premultiplied); m_frame.setDevicePixelRatio(m_dpr); m_fx.key.clear(); m_bgKey.clear(); m_art.key.clear(); }
    {
        QPainter p(&m_frame);
        render(p);
        drawLinks(p);
    }
    QPainter w(this);
    w.drawImage(QRectF(0, 0, width(), height()), m_frame);
}
void Canvas::render(QPainter& p)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.fillRect(rect(), m_sk ? m_sk->paperC() : QColor(3, 5, 10));
    m_hits.clear(); m_liveHits.clear(); m_ringHits.clear(); m_liveBudget = 2; m_art.budget = 1;
    beginFrame(7);   // quanto tempo por quadro os modelos 3D podem gastar se atualizando
    m_selPath = doc->pathOf(sel);
    Galaxy& g = *doc->gal;
    const double z = cam.z, zoomK = z / fitZ(), cT = std::cos(cam.tilt);
    Region* fr = nullptr;
    if (m_focusAmt > 0) for (const auto& r : doc->regions) if (r->key == m_focusShown) fr = r.get();
    std::vector<Label> labels;
    if (fr) paintFocus(p, *fr, zoomK, cT, labels);
    else {
        drawBackdrop(p, zoomK);
        if (layTerr()) { std::vector<Region*> all; for (const auto& r : doc->regions) all.push_back(r.get()); drawRegions(p, zoomK, all); }
    }
    m_openSystems.clear();
    for (const auto& s : g.systems) if (!s->gone && (!fr || fr->systems.contains(s->id))) drawSystemNode(p, *s, labels, z, cT, fr ? 4000 : 0);
    if (!fr) drawDeepBuilds(p, labels, z, cT);
    if (layTraffic()) drawTraffic(p, fr, z, cT, zoomK);
    drawGhost(p, z, cT);
    drawMapExtras(p, labels, z, cT);
    if (m_drag && m_drag->hasGhostOrb) {
        QPen pen(accent(0.8)); pen.setWidthF(1.2); pen.setDashPattern({ 4.0 / 1.2, 5.0 / 1.2 });
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        ellipseAt(p, m_drag->gx, m_drag->gy, m_drag->gr, m_drag->gc);
    }
    if (layTerr()) for (const auto& r : doc->regions) {
        if (zoomK > 18) continue;
        if (fr && m_focusAmt > 0.98) continue;
        if (r->systems.isEmpty()) continue;
        double sumX = 0, minY = 1e9;
        for (int id : r->systems) { const System& s = *g.systems[size_t(id)]; double px, py; proj(s.x, s.y, 0, px, py); sumX += px; minY = std::min(minY, py); }
        const double x = sumX / r->systems.size();
        QString spaced;
        const QString up = r->name.toUpper();
        for (int i = 0; i < up.size(); i++) { if (i) spaced += QLatin1Char(' '); spaced += up[i]; }
        Label l; l.text = spaced; l.x = x; l.y = std::max(24.0, minY - 0.06 * z * cT - 12); l.center = true; l.pri = 5000 - zoomK * 100;
        const QFont f = uiFont(600, 13);
        l.font = f; l.color = regionInk(*r); l.key = r->key; l.alpha = (1 - smooth(8, 18, zoomK)) * (fr ? 1 - m_focusAmt : 1); l.hasAlpha = true;
        labels.push_back(l);
    }
    placeLabels(p, labels);
    pumpThumbs(10);
}
// O fundo do mapa: a galáxia pintada e as estrelas de perto.
void Canvas::drawBackdrop(QPainter& p, double zoomK)
{
    if (m_sk) { drawArtBackdrop(p, zoomK); return; }
    const QPointF vc = viewCenter();
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9").arg(cam.x, 0, 'g', 12).arg(cam.y, 0, 'g', 12).arg(cam.z, 0, 'g', 12).arg(cam.h, 0, 'g', 10).arg(cam.tilt, 0, 'g', 10).arg(cam.yaw, 0, 'g', 10).arg(vc.x()).arg(vc.y()).arg(view + QString::number(skyClean) + QString::number(m_frame.width()));
    if (key != m_bgKey) {
        m_bgKey = key;
        if (m_bg.size() != m_frame.size()) m_bg = QImage(m_frame.size(), QImage::Format_RGB32);
        m_bg.setDevicePixelRatio(1);
        m_bg.fill(QColor(3, 5, 10));
        QPainter g(&m_bg);
        g.setRenderHint(QPainter::SmoothPixmapTransform, true);
        // A galáxia em si: imagem pintada pelo núcleo, que some quando você está dentro de um sistema.
        const double gAlpha = (1 - smooth(5, 28, zoomK) * 0.98) * (view == QLatin1String("atlas") ? 0.2 : skyClean ? 0.42 : 1);
        if (gAlpha > 0.005) {
            const double E = EXT;
            double p0x, p0y, p1x, p1y, p2x, p2y;
            proj(-E, E, 0, p0x, p0y); proj(E, E, 0, p1x, p1y); proj(-E, -E, 0, p2x, p2y);
            // de perto usa a imagem grande; de longe, a menor evita o chuvisco de reduzir demais
            const bool small = (p1x - p0x) * m_dpr < 800 && !m_galSmall.isNull();
            const QImage& src = small ? m_galSmall : doc->galImg;
            const double T = src.width();
            g.setCompositionMode(QPainter::CompositionMode_Plus);
            g.setOpacity(gAlpha);
            g.setTransform(QTransform(m_dpr * (p1x - p0x) / T, m_dpr * (p1y - p0y) / T, m_dpr * (p2x - p0x) / T, m_dpr * (p2y - p0y) / T, m_dpr * p0x, m_dpr * p0y));
            g.drawImage(0, 0, src);
            g.resetTransform();
            g.setOpacity(1);
            g.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        g.end();
        // Estrelas de perto: escritas direto na imagem, com luz somada.
        const double fade = smooth(1.4, 3, zoomK);
        if (fade > 0) {
            static const int DC[5][3] = { { 0xa9, 0xc4, 0xff }, { 0xd7, 0xe3, 0xff }, { 0xff, 0xf4, 0xe6 }, { 0xff, 0xd9, 0xa8 }, { 0xff, 0xb9, 0x8a } };
            const DetailStars& d = doc->gal->detail;
            const int W = m_bg.width(), H = m_bg.height();
            uchar* bits = m_bg.bits();
            const qsizetype bpl = m_bg.bytesPerLine();
            const double cy = std::cos(cam.yaw), sy = std::sin(cam.yaw), ct = std::cos(cam.tilt);
            const double hk = cam.h * std::sin(cam.tilt);
            for (int i = 0; i < d.n; i++) {
                const double dx = d.xs[size_t(i)] - cam.x, dy = d.ys[size_t(i)] - cam.y;
                const double X = dx * cy - dy * sy, Y = dx * sy + dy * cy;
                const double sx = (vc.x() + X * cam.z) * m_dpr, syy = (vc.y() - (Y * ct - hk) * cam.z) * m_dpr;
                if (sx < 1 || syy < 1 || sx >= W - 2 || syy >= H - 2) continue;
                const int* c = DC[d.cs[size_t(i)]];
                const double a = fade * (0.3 + d.bs[size_t(i)] * 0.25);
                const int sz = d.bs[size_t(i)] >= 2 ? 2 : 1;
                for (int yy = 0; yy < sz; yy++) {
                    QRgb* row = reinterpret_cast<QRgb*>(bits + bpl * (int(syy) + yy));
                    for (int xx = 0; xx < sz; xx++) {
                        QRgb& q = row[int(sx) + xx];
                        q = qRgb(std::min(255, qRed(q) + int(c[0] * a)), std::min(255, qGreen(q) + int(c[1] * a)), std::min(255, qBlue(q) + int(c[2] * a)));
                    }
                }
            }
        }
    }
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(QRectF(0, 0, width(), height()), m_bg);
    p.restore();
}
// Um sistema no mapa: ponto de longe, esquema de órbitas de perto.
void Canvas::drawSystemNode(QPainter& p, System& s, std::vector<Label>& labels, double z, double cT, double boost)
{
    double x, y;
    proj(s.x, s.y, s.layH, x, y);
    const double Rpx = s.layR * z;
    if (x < -Rpx - 40 || y < -Rpx - 40 || x > width() + Rpx + 40 || y > height() + Rpx + 40) return;
    const double open = smooth(36, 90, Rpx);
    const auto c = sysColor(s);
    const QString k = keyS(s);
    const bool isSel = m_selPath.contains(k);
    // Pino de altura quando o mapa está inclinado.
    if (cam.tilt > 0.05 && std::fabs(s.layH) > 0.002 && open < 0.5) {
        double bx, by;
        proj(s.x, s.y, 0, bx, by);
        QPen pen(m_sk ? m_sk->inkC(0.3) : QColor(140, 200, 255, 51)); pen.setWidthF(1);
        p.setPen(pen);
        p.drawLine(QPointF(x, y), QPointF(bx, by));
        p.fillRect(QRectF(bx - 1, by - 1, 2, 2), m_sk ? m_sk->inkC(0.4) : QColor(140, 200, 255, 77));
    }
    if (open < 1) {
        const double sz = s.isCore ? 16 : 7 + std::min(6.0, (s.stars[0].radius != 0 ? s.stars[0].radius : 1) * 1.2);
        p.setOpacity(1 - open);
        if (m_sk) drawArtStarFar(p, s, x, y);
        else {
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            p.drawImage(QRectF(x - sz * 1.6, y - sz * 1.6, sz * 3.2, sz * 3.2), glow(c));
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        p.setOpacity(1);
        if (sel == k) ring(p, x, y, 9);
        m_hits.push_back({ k, x, y, 9 });
    }
    if (open > 0) { doc->place(s); drawSystemOpen(p, s, x, y, open, z, cT, labels); if (open > 0.3) m_openSystems.push_back(&s); }
    const bool t = doc->touched(k);
    const double pri = 100 + (t ? 600 : 0) + (isSel ? 2000 : 0) + std::min(40.0, double(planetsOf(s).size()) * 4) + livingCount(s) * 15 + (s.isCore ? 300 : 0) + boost;
    if (layNames()) {
        Label l; l.text = doc->labelOf(k);
        const bool isOpen = open > 0.5;
        l.x = isOpen ? x : x + 9; l.y = isOpen ? y - s.layStar * z * 1.8 - 10 : y + 4; l.center = isOpen; l.pri = isOpen ? pri + 3000 : pri;
        l.font = uiFont(t ? 600 : 500, isOpen ? 13 : 11.5);
        l.color = t ? QColor(0xe4, 0xeb, 0xf5) : QColor(200, 214, 234, 209); l.key = k;
        labels.push_back(l);
    }
}
void Canvas::drawStar(QPainter& p, const System& s, double x, double y, double sr, double cT)
{
    if (m_sk) { drawArtStarOpen(p, s, x, y, sr, cT); return; }
    const auto c = sysColor(s);
    const Star& st = s.stars[0];
    const double simT = doc->simT;
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    p.setPen(Qt::NoPen);
    if (s.isCore || st.kind == StarKind::BuracoNegro) {
        // Buraco negro: a luz do disco curvada em volta da sombra.
        double a, b;
        blackHoleAxis(s, a, b);
        paintBlackHole(p, QPointF(x, y), sr * (s.isCore ? 1 : 0.8) * 0.62, a, b, cam.tilt, cam.yaw, simT, m_exporting ? nullptr : this);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        return;
    }
    if (s.kind == StarKind::Binaria && s.stars.size() > 1) {
        // Estrela dupla: uma girando em volta da outra, bem devagar.
        const double a = simT * SPEED_BINARY, d = sr * 0.9;
        for (int i = 0; i < 2; i++) {
            const double ox = std::cos(a + i * PI_D) * d, oy = std::sin(a + i * PI_D) * d * cT, rr = sr * (i ? 0.75 : 1);
            p.drawImage(QRectF(x + ox - rr * 1.8, y + oy - rr * 1.8, rr * 3.6, rr * 3.6), glow(s.stars[size_t(i)].color));
        }
    } else p.drawImage(QRectF(x - sr * 2.2, y - sr * 2.2, sr * 4.4, sr * 4.4), glow(c));
    if (st.kind == StarKind::Neutrons && st.pulsar) {
        // Pulsar: dois feixes girando.
        const double a = simT * SPEED_PULSAR, L = sr * 9;
        for (int sgn : { 1, -1 }) {
            const double dx = std::cos(a) * sgn, dy = std::sin(a) * sgn * cT;
            QLinearGradient gr(x, y, x + dx * L, y + dy * L);
            gr.setColorAt(0, rgbaC(170, 210, 255, 0.55)); gr.setColorAt(1, rgbaC(170, 210, 255, 0));
            p.setBrush(gr);
            QPolygonF poly; poly << QPointF(x, y) << QPointF(x + dx * L - dy * L * 0.08, y + dy * L + dx * L * 0.08) << QPointF(x + dx * L + dy * L * 0.08, y + dy * L - dx * L * 0.08);
            p.drawPolygon(poly);
        }
    }
    if (st.kind == StarKind::AnaBranca) {
        // Anã branca: a nebulosa planetária em volta, um anel colorido.
        p.save();
        p.translate(x, y);
        p.scale(1, std::max(0.3, cT));
        QRadialGradient gr(0, 0, sr * 5.5);
        const double t0 = 2.2 / 5.5, span = 1 - t0;
        gr.setColorAt(0, QColor(120, 200, 255, 0)); gr.setColorAt(t0, QColor(120, 200, 255, 0)); gr.setColorAt(t0 + span * 0.5, rgbaC(120, 220, 210, 0.18)); gr.setColorAt(t0 + span * 0.8, rgbaC(220, 110, 160, 0.12)); gr.setColorAt(1, QColor(220, 110, 160, 0));
        p.setBrush(gr);
        p.drawEllipse(QPointF(0, 0), sr * 5.5, sr * 5.5);
        p.restore();
    }
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
}
void Canvas::drawSystemOpen(QPainter& p, System& s, double x, double y, double open, double z, double cT, std::vector<Label>& labels)
{
    const QString k = keyS(s);
    const double simT = doc->simT;
    p.setOpacity(open);
    const auto P = [&](double wx, double wy, double& ox, double& oy) { proj(wx, wy, s.layH, ox, oy); };
    const double deep = 1 - smooth(2500, 6000, s.layR * z);
    const auto planets = planetsOf(s);
    // Órbitas e zona habitável.
    if (layOrb()) {
        QPen pen(m_sk ? m_sk->inkC(0.42 * (0.5 + 0.5 * deep)) : rgbaC(150, 180, 220, 0.16 * (0.5 + 0.5 * deep)));
        pen.setWidthF(m_sk ? 0.8 : 1);
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        for (Body* q : planets) ellipseAt(p, x, y, q->layOrb * z, cT);
        if (s.hasHz && !planets.empty()) {
            const double r0 = std::max(s.layR * 0.12, doc->toR(s, s.hz[0])), r1 = std::min(s.layR * 1.05, doc->toR(s, s.hz[1]));
            const double a = 0.06 * deep;
            if (r1 > r0 && a > 0.003) {
                QPainterPath band;
                band.addEllipse(QPointF(x, y), r1 * z, r1 * z * cT);
                band.addEllipse(QPointF(x, y), r0 * z, r0 * z * cT);
                p.setPen(Qt::NoPen);
                p.setBrush(m_sk ? m_sk->hzC(a * (m_sk->mono ? 0.9 : 1.8)) : rgbaC(111, 214, 154, a));
                p.drawPath(band);
            }
        }
    }
    // Cinturões de asteroides e discos gelados: poeira girando devagar.
    for (size_t bi = 0; bi < s.belts.size(); bi++) {
        const Belt& belt = s.belts[bi];
        const double r0 = doc->toR(s, belt.a0), r1 = doc->toR(s, belt.a1);
        const int n = std::min(260, int(std::floor(belt.n / 3.0 + 0.5)));
        const QColor c = m_sk ? m_sk->inkC(!belt.kuiper ? 0.6 : 0.42) : !belt.kuiper ? rgbaC(200, 185, 165, 0.55) : rgbaC(190, 220, 240, 0.45);
        for (int i = 0; i < n; i++) {
            const double h1 = std::sin(i * 12.9898 + double(bi) * 78.233) * 43758.5453, u = h1 - std::floor(h1), h2 = std::sin(i * 39.346 + double(bi) * 11.135) * 24634.6345, v = h2 - std::floor(h2);
            const double rr = r0 + (r1 - r0) * u, a = v * TAU + simT * SPEED_BELT * std::pow(r0 / rr, 1.5);
            double px, py;
            P(s.x + std::cos(a) * rr, s.y + std::sin(a) * rr, px, py);
            p.fillRect(QRectF(px, py, 1.2, 1.2), c);
        }
    }
    // Estrela, entre as metades do que a envolve.
    const double sr = std::max(4.0, s.layStar * z);
    std::vector<Build*> megas, here;
    if (sprites && view != QLatin1String("terrain")) { here = doc->buildsIn(s); for (Build* b : here) if (b->host < 0 && structOf(b->type)->mega) megas.push_back(b); }
    for (Build* b : megas) drawStarMega(p, *b, s, x, y, z, cT, open, 'b');
    drawStar(p, s, x, y, sr, cT);
    m_hits.push_back({ k, x, y, std::max(10.0, sr) });
    for (Build* b : megas) {
        if (b->type != "mundoAnel") drawStarMega(p, *b, s, x, y, z, cT, open, 'f');
        if (layNames() && open > 0.5 && (doc->userNames.contains(b->key) || sel == b->key)) {
            const double below = b->type == "mundoAnel" ? megaRadius(*b, s) * z * std::cos(ringPitch()) + s.layR * 0.05 * b->scale * z * std::sin(ringPitch()) : megaRadius(*b, s) * z * cT;
            Label l; l.text = doc->labelOf(b->key); l.x = x; l.y = y + below + 16; l.center = true; l.pri = 1200; l.font = uiFont(600, 12); l.color = QColor(0xe4, 0xeb, 0xf5); l.key = b->key;
            labels.push_back(l);
        }
    }
    std::vector<Build*> ringFront;
    for (Build* b : megas) if (b->type == "mundoAnel") ringFront.push_back(b);
    const auto flushRing = [&](double orb, bool all) {
        for (size_t i = 0; i < ringFront.size();) {
            Build* b = ringFront[i];
            if (!all && orb <= megaRadius(*b, s)) { i++; continue; }
            drawStarMega(p, *b, s, x, y, z, cT, open, 'f');
            p.setOpacity(open);
            ringFront.erase(ringFront.begin() + long(i));
        }
    };
    if (tool == Tool::Build) for (size_t bi = 0; bi < s.belts.size(); bi++) {
        const Belt& belt = s.belts[bi];
        m_ringHits.push_back({ QStringLiteral("K%1.%2").arg(s.id).arg(belt.uid ? belt.uid : int(bi) + 1), x, y, doc->toR(s, belt.a0) * z, doc->toR(s, belt.a1) * z, cT, 0 });
    }
    for (Build* b : megas) if (b->type == "mundoAnel") m_ringHits.push_back({ b->key, x, y, (megaRadius(*b, s) - s.layR * 0.05) * z, (megaRadius(*b, s) + s.layR * 0.05) * z, std::cos(ringPitch()), 0 });
    // Estrelas S em volta do núcleo.
    if (s.isCore) {
        const int all = int(s.bodies.size());
        for (int i = 0; i < all; i++) {
            const Body& b = *s.bodies[size_t(i)];
            const double rr = s.layR * (0.28 + 0.65 * (i + 1) / all), a = b.M0 + simT * SPEED_SSTAR * std::pow(0.3 / (rr / s.layR), 1.5);
            double px, py;
            P(s.x + std::cos(a) * rr, s.y + std::sin(a) * rr, px, py);
            if (m_sk) { p.setPen(Qt::NoPen); p.setBrush(m_sk->inkC()); p.drawEllipse(QPointF(px, py), 1.8, 1.8); continue; }
            p.setCompositionMode(QPainter::CompositionMode_Plus);
            p.drawImage(QRectF(px - 6, py - 6, 12, 12), glow(b.star ? b.star->color : std::array<int, 3> { 255, 230, 200 }));
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
    }
    // Cometas: órbita esticada e cauda apontando pra longe da estrela.
    for (const auto& cp : s.bodies) {
        if (cp->body != BodyKind::Cometa) continue;
        const Body& cm = *cp;
        Body orbit;
        orbit.a = cm.a; orbit.e = cm.e; orbit.w = cm.w; orbit.M0 = cm.M0 + simT * SPEED_COMET; orbit.P = cm.P;
        double r, th;
        orbitPos(orbit, 0, r, th);
        const double R = doc->toR(s, r);
        double cx2, cy2;
        P(s.x + std::cos(th) * R, s.y + std::sin(th) * R, cx2, cy2);
        const double dx = cx2 - x, dy = cy2 - y;
        double dn = std::hypot(dx, dy);
        if (dn == 0) dn = 1;
        const double tl = std::min(80.0, s.layGap * z * 1.4) * std::min(1.6, 0.3 / std::max(0.08, r / cm.a));
        QLinearGradient gr(cx2, cy2, cx2 + (dx / dn) * tl, cy2 + (dy / dn) * tl);
        gr.setColorAt(0, m_sk ? m_sk->inkC(0.75) : rgbaC(200, 230, 255, 0.7)); gr.setColorAt(1, m_sk ? m_sk->inkC(0) : rgbaC(200, 230, 255, 0));
        QPen pen(QBrush(gr), m_sk ? 1.2 : 2);
        p.setPen(pen);
        p.drawLine(QPointF(cx2, cy2), QPointF(cx2 + (dx / dn) * tl, cy2 + (dy / dn) * tl));
        p.setPen(Qt::NoPen); p.setBrush(m_sk ? m_sk->inkC() : QColor(0xea, 0xf4, 0xff));
        p.drawEllipse(QPointF(cx2, cy2), 1.8, 1.8);
        const QString ck = QStringLiteral("C%1.%2").arg(s.id).arg(cm.idx);
        if (sel == ck) ring(p, cx2, cy2, 7);
        m_hits.push_back({ ck, cx2, cy2, 7 });
        if (layNames() && sel == ck) { Label l; l.text = doc->labelOf(ck); l.x = cx2; l.y = cy2 + 18; l.center = true; l.pri = 3000; l.font = uiFont(500, 11); l.color = QColor(200, 214, 234, 217); l.key = ck; labels.push_back(l); }
    }
    // Planetas (com a sombra do lado de lá da estrela) e, de perto, as luas.
    const double showNames = open * smooth(110, 170, s.layR * z);
    for (Body* pp : planets) {
        flushRing(pp->layOrb, false);
        const QString pk = keyP(s, *pp);
        double px, py;
        P(pp->layX, pp->layY, px, py);
        const double pr = std::max(2.2, pp->layR * z);
        if (px < -pr * 8 || py < -pr * 8 || px > width() + pr * 8 || py > height() + pr * 8) continue;
        const double dx = x - px, dy = y - py;
        double dn = std::hypot(dx, dy);
        if (dn == 0) dn = 1;
        const double ux = dx / dn, uy = dy / dn;
        std::vector<Build*> hosted;
        const int uid = pp->uid ? pp->uid : pp->idx;
        for (Build* b : here) if (b->host == uid) hosted.push_back(b);
        const double mOpen = smooth(9, 22, pr);
        const auto drawMoons = [&](bool behind) {
            for (const auto& mp : pp->moons) {
                const Body& m = *mp;
                const QString mk = keyM(s, *pp, m);
                double mx, my;
                P(m.layX, m.layY, mx, my);
                const double mr = std::max(1.6, m.layR * z);
                if ((my < py) != behind) continue;
                p.setOpacity(open * mOpen);
                const double mdx = x - mx, mdy = y - my;
                double mdn = std::hypot(mdx, mdy);
                if (mdn == 0) mdn = 1;
                drawBody(p, m, mx, my, mr, mdx / mdn, mdy / mdn);
                if (sel == mk) ring(p, mx, my, mr + 4);
                m_hits.push_back({ mk, mx, my, std::max(7.0, mr + 2) });
                if (layNames() && mr > 3.5) {
                    const bool t = doc->touched(mk);
                    Label l; l.text = doc->labelOf(mk); l.x = mx; l.y = my + mr + 12; l.center = true; l.pri = 800 + (t ? 600 : 0) + (sel == mk ? 3000 : 0); l.font = uiFont(t ? 600 : 500, 11); l.color = QColor(200, 214, 234, 204); l.key = mk; l.alpha = mOpen * smooth(3.5, 8, mr); l.hasAlpha = true;
                    labels.push_back(l);
                }
            }
            p.setOpacity(open);
        };
        if (mOpen > 0) {
            if (layOrb()) {
                p.setOpacity(open * mOpen);
                QPen pen(m_sk ? m_sk->inkC(0.28) : rgbaC(150, 180, 220, 0.14));
                p.setPen(pen); p.setBrush(Qt::NoBrush);
                const int nm = int(pp->moons.size());
                const bool tight = m_sk && nm > 2 && (pp->moons[size_t(nm - 1)]->layOrb - pp->moons[0]->layOrb) * z / (nm - 1) < 5;
                for (int i = 0; i < nm; i++) { if (tight && i && i < nm - 1) continue; ellipseAt(p, px, py, pp->moons[size_t(i)]->layOrb * z, cT); }
                p.setOpacity(open);
            }
            drawMoons(true);
        }
        for (Build* b : hosted) if (b->type == "anelOrbital") {
            const double Rr = orbRingR(pr, b->scale), Pp = ringPitch(), La = orbRingLight(s, *pp);
            toned(p, b->key + QLatin1Char('b'), px, py, Rr * 1.35, [&](QPainter& q) { drawOrbitalRing(q, px, py, Rr, Pp, b->seed, simT, 'b', open, m_dpr, pr, La); });
        }
        if (pp->rings && pr > 5) drawRing(p, *pp, px, py, pr, false);
        drawBody(p, *pp, px, py, pr, ux, uy);
        if (pp->rings && pr > 5) drawRing(p, *pp, px, py, pr, true);
        for (Build* b : hosted) if (b->type == "anelOrbital") {
            const double Rr = orbRingR(pr, b->scale), Pp = ringPitch(), La = orbRingLight(s, *pp);
            toned(p, b->key + QLatin1Char('f'), px, py, Rr * 1.35, [&](QPainter& q) { drawOrbitalRing(q, px, py, Rr, Pp, b->seed, simT, 's', open, m_dpr, pr, La); drawOrbitalRing(q, px, py, Rr, Pp, b->seed, simT, 'f', open, m_dpr, pr, La); });
            m_ringHits.push_back({ b->key, px, py, Rr * 0.9, Rr * 1.1, std::cos(Pp), -0.32 });
            if (sel == b->key) {
                p.save(); p.translate(px, py); p.rotate(-0.32 * 180 / PI_D);
                QPen pen(accent(0.85)); pen.setWidthF(1.2); pen.setDashPattern({ 5.0 / 1.2, 5.0 / 1.2 });
                p.setPen(pen); p.setBrush(Qt::NoBrush);
                p.drawEllipse(QPointF(0, 0), Rr * 1.14, Rr * 1.14 * std::cos(Pp));
                p.restore();
            }
        }
        if (sel == pk) ring(p, px, py, pr + 5);
        m_hits.push_back({ pk, px, py, std::max(8.0, pr + 2) });
        if (isLiving(pp->type) && pr > 3 && pr < 60) {
            QPen pen(m_sk ? m_sk->hzC(0.8) : rgbaC(111, 214, 154, 0.55)); pen.setWidthF(1);
            p.setPen(pen); p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(px, py), pr + 2.5, pr + 2.5);
        }
        if (layNames() && showNames > 0) {
            const bool t = doc->touched(pk);
            Label l; l.text = doc->labelOf(pk); l.x = px; l.y = py + pr + 14; l.center = true; l.pri = 1000 + (t ? 600 : 0) + (sel == pk ? 3000 : 0) + (isLiving(pp->type) ? 80 : 0); l.font = uiFont(t ? 600 : 500, 12); l.color = t ? QColor(0xe4, 0xeb, 0xf5) : QColor(200, 214, 234, 217); l.key = pk; l.alpha = showNames; l.hasAlpha = true;
            labels.push_back(l);
        }
        if (mOpen > 0) drawMoons(false);
        for (Build* b : hosted) if (b->type != "anelOrbital") drawOrbitBuild(p, *b, s, px, py, pr, z, cT, open, labels, showNames);
    }
    flushRing(0, true);
    // O que orbita a estrela sem ser colosso.
    for (Build* b : here) if (b->host < 0 && !structOf(b->type)->mega) drawOrbitBuild(p, *b, s, x, y, 0, z, cT, open, labels, showNames);
    p.setOpacity(1);
}

// ── o que foi construído ──
double Canvas::megaRadius(const Build& b, const System& s) const
{
    return b.type == "mundoAnel" ? b.rr * s.layR : b.type == "dyson" ? s.layR * 0.15 * b.scale : b.type == "enxame" ? s.layR * 0.17 * b.scale : s.layR * 0.21 * b.scale;
}
void Canvas::drawStarMega(QPainter& p, Build& b, System& s, double x, double y, double z, double cT, double alpha, char part)
{
    const double R = megaRadius(b, s) * z, simT = doc->simT;
    const bool sel2 = sel == b.key;
    const double bigR = R * (b.type == "sifao" ? 1.7 : 1.3) + s.layR * 0.06 * z;
    const QString tid = b.key + QLatin1Char(part);
    if (b.type == "dyson") toned(p, tid, x, y, bigR, [&](QPainter& q) { drawDyson(q, x, y, R, cT, b.seed, simT, part, alpha); });
    else if (b.type == "mundoAnel") toned(p, tid, x, y, bigR, [&](QPainter& q) { drawRingworld(q, x, y, R, s.layR * 0.05 * b.scale * z, ringPitch(), b.seed, simT, part, alpha); });
    else if (part == 'f' && b.type == "enxame") toned(p, tid, x, y, bigR, [&](QPainter& q) { drawSwarm(q, x, y, R, cT, b.seed, simT, alpha); });
    else if (part == 'f' && b.type == "sifao") { const auto c = sysColor(s); toned(p, tid, x, y, bigR, [&](QPainter& q) { drawSiphon(q, x, y, R, cT, b.seed, simT, c, alpha, m_dpr, cam.yaw); }); }
    if (part == 'f') {
        if (b.type != "mundoAnel") m_hits.push_back({ b.key, x, y - R * cT * 0.9, std::max(8.0, R * 0.2) });
        if (sel2) {
            QPen pen(accent(0.8)); pen.setWidthF(1.2); pen.setDashPattern({ 5.0 / 1.2, 5.0 / 1.2 });
            p.setPen(pen); p.setBrush(Qt::NoBrush);
            ellipseAt(p, x, y, R + 8, b.type == "mundoAnel" ? std::cos(ringPitch()) : cT);
        }
    }
}
// Posição de agora de uma peça em órbita (ou solta no espaço).
void Canvas::buildPos(const Build& b, const System* s, const Body* host, double& x, double& y) const
{
    if (b.deep) { x = b.deepX; y = b.deepY; return; }
    const double a = b.a0 + b.w * doc->simT;
    if (host) { x = host->layX + std::cos(a) * b.rp * host->layR; y = host->layY + std::sin(a) * b.rp * host->layR; return; }
    x = s->x + std::cos(a) * b.rr * s->layR;
    y = s->y + std::sin(a) * b.rr * s->layR;
}
double Canvas::buildRadiusOf(const Build& b, const System* s, const Body* host) const
{
    const StructDef* def = structOf(b.type);
    const double size = def->size != 0 ? def->size : 0.02;
    if (b.deep) return size * 0.75 * b.scale * worldScale;
    if (host) return std::max(host->layR * 0.16, size * s->layR * 0.65) * b.scale * worldScale;
    return size * 1.4 * s->layR * b.scale * worldScale;
}
void Canvas::drawOrbitBuild(QPainter& p, Build& b, System& s, double cx, double cy, double pr, double z, double cT, double alpha, std::vector<Label>& labels, double showNames)
{
    Body* host = nullptr;
    if (b.host >= 0) for (Body* q : planetsOf(s)) if ((q->uid ? q->uid : q->idx) == b.host) host = q;
    const double simT = doc->simT;
    if (b.type == "elevador") {
        const double ang = b.a0 + simT * 0.02, H = pr * 2.2 * b.scale;
        toned(p, b.key, cx, cy, (pr + H) * 1.25, [&](QPainter& q) { drawElevator(q, cx, cy, pr, ang, H, cT, simT, alpha, m_dpr, b.key); });
        const double hx = cx + std::cos(ang) * (pr + H), hy = cy + std::sin(ang) * (pr * 0.97 + H * cT);
        if (sel == b.key) ring(p, hx, hy, std::max(6.0, H * 0.08));
        m_hits.push_back({ b.key, hx, hy, 9 });
        return;
    }
    double wx, wy, px, py;
    buildPos(b, &s, host, wx, wy);
    proj(wx, wy, s.layH, px, py);
    const double r = buildRadiusOf(b, &s, host) * z;
    if (px < -r - 20 || py < -r - 20 || px > width() + r + 20 || py > height() + r + 20) return;
    // Órbita fininha enquanto o catálogo está aberto ou a peça está escolhida, pra dar pra ver onde ela anda.
    if (layOrb() && (tool == Tool::Build || sel == b.key) && !host) {
        p.setOpacity(alpha * 0.3);
        QPen pen(m_sk ? m_sk->inkC() : rgbaC(150, 180, 220, 0.5)); pen.setWidthF(1); pen.setDashPattern({ 3, 5 });
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        ellipseAt(p, cx, cy, b.rr * s.layR * z, cT);
        p.setOpacity(alpha);
    }
    paintPiece(p, b, px, py, r, cx - px, cy - py, alpha);
    if (sel == b.key) ring(p, px, py, std::max(6.0, r + 4));
    m_hits.push_back({ b.key, px, py, std::max(8.0, r) });
    if (layNames() && showNames > 0 && r > 2.5) {
        const bool named = doc->userNames.contains(b.key);
        Label l; l.text = doc->labelOf(b.key); l.x = px; l.y = py + std::max(r, 3.0) + 13; l.center = true; l.pri = 950 + (doc->touched(b.key) ? 600 : 0) + (sel == b.key ? 3000 : 0); l.font = uiFont(named ? 600 : 500, 11); l.color = QColor(200, 214, 234, 217); l.key = b.key; l.alpha = showNames; l.hasAlpha = true;
        labels.push_back(l);
    }
}
double Canvas::pieceRot(const Build& b) const { return b.rot0 + structOf(b.type)->spin * doc->simT + cam.yaw + b.spinU; }
// A peça em si: de longe um pontinho aceso, de perto a arte com luz da estrela.
void Canvas::paintPiece(QPainter& p, Build& b, double px, double py, double r, double toX, double toY, double alpha)
{
    double dn = std::hypot(toX, toY);
    if (dn == 0) dn = 1;
    const double lx = toX / dn, ly2 = toY / dn;
    if (r < 2.5) {
        p.setOpacity(alpha);
        p.fillRect(QRectF(px - 1, py - 1, 2, 2), m_sk ? m_sk->inkC() : QColor(0xcf, 0xd8, 0xe6));
        p.setOpacity(1);
        return;
    }
    const double rot = pieceRot(b), pitch = piecePitch(b);
    drawModel(p, b.type, b.seed, px, py, r, rot, pitch, lx, ly2, alpha, m_dpr, b.key);
    if (!m_sk) drawAnimated(p, b.type, b.seed, px, py, r, rot, pitch, doc->simT, alpha, b.flash);
    // De perto, arrastar gira a peça, como nos planetas.
    if (r > 24) m_liveHits.push_back({ nullptr, &b, px, py, r });
    p.setOpacity(1);
}
void Canvas::drawDeepBuilds(QPainter& p, std::vector<Label>& labels, double z, double)
{
    if (!sprites || view == QLatin1String("terrain")) return;
    for (const auto& bp : doc->builds) {
        Build& b = *bp;
        if (!b.deep) continue;
        double px, py;
        proj(b.deepX, b.deepY, 0, px, py);
        const double r = buildRadiusOf(b, nullptr, nullptr) * z;
        if (px < -r - 20 || py < -r - 20 || px > width() + r + 20 || py > height() + r + 20) continue;
        if (r < 5) {
            QPen pen(m_sk ? m_sk->inkC(0.85) : rgbaC(190, 220, 255, 0.75)); pen.setWidthF(1);
            p.save(); p.translate(px, py); p.rotate(45);
            p.setPen(pen); p.setBrush(Qt::NoBrush);
            p.drawRect(QRectF(-3, -3, 6, 6));
            p.restore();
        } else paintPiece(p, b, px, py, r, -0.7, -0.7, 1);
        if (sel == b.key) ring(p, px, py, std::max(7.0, r + 4));
        m_hits.push_back({ b.key, px, py, std::max(8.0, r) });
        if (layNames()) { Label l; l.text = doc->labelOf(b.key); l.x = px; l.y = py + std::max(r, 3.0) + 14; l.center = true; l.pri = 700 + (sel == b.key ? 3000 : 0); l.font = uiFont(500, 11); l.color = QColor(200, 214, 234, 217); l.key = b.key; labels.push_back(l); }
    }
}
// Nomes: os mais importantes primeiro; um nome que encostaria em outro já posto não aparece.
void Canvas::placeLabels(QPainter& p, std::vector<Label>& labels)
{
    std::stable_sort(labels.begin(), labels.end(), [](const Label& a, const Label& b) { return a.pri > b.pri; });
    std::vector<QRectF> placed;
    const QString halo = m_sk ? m_sk->id : QStringLiteral("d");
    if (m_stamps.size() > 3000) m_stamps.clear();
    for (Label& l : labels) {
        double alpha = l.hasAlpha ? l.alpha : 1;
        if (alpha < 0.05) continue;
        if (doc->hidden.contains(l.key)) { if (tool != Tool::Names) continue; alpha = 0.32; }
        if (l.x < -100 || l.x > width() + 100 || l.y < -20 || l.y > height() + 20) continue;
        QFont font = l.font;
        if (m_sk) {
            font = QFont(QStringLiteral("IM FELL English SC"));
            font.setPixelSize(l.font.pixelSize() + 1);
            font.setWeight(l.font.weight());
        }
        const QFontMetricsF fm(font);
        const double w = fm.horizontalAdvance(l.text), h = 14;
        const double x0 = l.center ? l.x - w / 2 : l.x;
        const QRectF box(x0 - 4, l.y - h, w + 8, h + 4);
        bool hit = false;
        for (const QRectF& b : placed) if (box.intersects(b)) { hit = true; break; }
        if (hit) continue;
        placed.push_back(box);
        // Contornar texto a cada quadro é caro: cada nome vira um carimbo guardado.
        QColor col = l.color;
        if (m_sk && (l.color.rgb() == QColor(0xe4, 0xeb, 0xf5).rgb() || l.color.rgb() == QColor(0xf2, 0xe6, 0xc8).rgb() || l.color.alpha() < 255)) col = m_sk->inkC();
        const QString sk = QStringLiteral("%1|%2|%3|%4|%5|%6").arg(l.text, halo, font.family()).arg(font.pixelSize()).arg(font.weight()).arg(col.rgba());
        auto it = m_stamps.find(sk);
        if (it == m_stamps.end()) {
            const int iw = int(w) + 10, ih = int(fm.height()) + 8;
            QImage st(int(iw * m_dpr), int(ih * m_dpr), QImage::Format_ARGB32_Premultiplied);
            st.fill(Qt::transparent);
            st.setDevicePixelRatio(m_dpr);
            QPainter q(&st);
            q.setRenderHint(QPainter::Antialiasing);
            q.setRenderHint(QPainter::TextAntialiasing);
            QPainterPath path;
            path.addText(4, 4 + fm.ascent(), font, l.text);
            QPen pen(m_sk ? m_sk->haloC(0.9) : QColor(3, 5, 10, 217)); pen.setWidthF(3); pen.setJoinStyle(Qt::RoundJoin);
            q.setPen(pen); q.setBrush(Qt::NoBrush);
            q.drawPath(path);
            q.setPen(Qt::NoPen); q.setBrush(col);
            q.drawPath(path);
            q.end();
            it = m_stamps.insert(sk, st);
        }
        p.setOpacity(alpha);
        p.drawImage(QPointF(x0 - 4, l.y - fm.ascent() - 4), it.value());
        p.setOpacity(1);
        Hit ht; ht.key = l.key; ht.label = true; ht.box = box;
        m_hits.push_back(ht);
    }
}
void Canvas::drawMapExtras(QPainter& p, std::vector<Label>& labels, double z, double cT)
{
    // Moldura da galáxia.
    if (frameOn) {
        double x, y;
        proj(0, 0, 0, x, y);
        QPen pen(m_sk ? m_sk->inkC(0.55) : rgbaC(226, 234, 244, 0.5)); pen.setWidthF(1.2); pen.setDashPattern({ 5, 5 });
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        ellipseAt(p, x, y, EXT * z, cT);
    }
    // Nomes soltos.
    if (layNames()) for (const FreeLabel& l : doc->labels) {
        double x, y;
        proj(l.x, l.y, 0, x, y);
        Label q; q.text = l.text; q.x = x; q.y = y; q.center = true; q.pri = 2500;
        QFont f(QStringLiteral("IM FELL English SC")); f.setPixelSize(13); f.setWeight(QFont::DemiBold);
        q.font = f; q.color = QColor(0xf2, 0xe6, 0xc8); q.key = l.key;
        labels.push_back(q);
    }
    // A régua.
    if (tool == Tool::Ruler && !rulerPts.empty()) {
        QPolygonF pts;
        for (const QPointF& q : rulerPts) { double x, y; proj(q.x(), q.y(), 0, x, y); pts << QPointF(x, y); }
        double total = 0;
        const QColor rc = m_sk ? m_sk->selC() : QColor(0x8c, 0xc8, 0xff);
        QPen pen(rc); pen.setWidthF(1.6); pen.setDashPattern({ 6 / 1.6, 4 / 1.6 });
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        QPolygonF line = pts;
        const bool live = !rulerDone && m_hasPointer;
        double lx = 0, ly2 = 0, ex = pts.back().x(), ey = pts.back().y();
        if (live) { toWorld(m_pointer.x(), m_pointer.y(), lx, ly2); double x, y; proj(lx, ly2, 0, x, y); line << QPointF(x, y); ex = x; ey = y; }
        p.drawPolyline(line);
        for (size_t i = 1; i < rulerPts.size(); i++) total += std::hypot(rulerPts[i].x() - rulerPts[i - 1].x(), rulerPts[i].y() - rulerPts[i - 1].y());
        if (live) total += std::hypot(lx - rulerPts.back().x(), ly2 - rulerPts.back().y());
        p.setPen(Qt::NoPen); p.setBrush(rc);
        for (const QPointF& q : pts) p.drawEllipse(q, 3.5, 3.5);
        const QString txt = fmtLy(total, doc->gal->ly);
        const QFont f = uiFont(600, 12);
        const QFontMetricsF fm(f);
        const double tw = fm.horizontalAdvance(txt);
        QPen bp(rgbaC(196, 214, 240, 0.3)); bp.setWidthF(1);
        p.setPen(bp); p.setBrush(QColor(0x0b, 0x10, 0x1b));
        p.drawRoundedRect(QRectF(ex + 10, ey - 26, tw + 16, 22), 8, 8);
        p.setPen(QColor(0xe4, 0xeb, 0xf5)); p.setFont(f);
        p.drawText(QRectF(ex + 18, ey - 26, tw + 4, 22), Qt::AlignLeft | Qt::AlignVCenter, txt);
    }
    // O círculo do pincel de território.
    if (tool == Tool::Realm && realmMode != RealmMode::Seed && m_hasPointer) {
        QPen pen(realmMode == RealmMode::Neutral ? rgbaC(255, 170, 150, 0.8) : accent(0.85)); pen.setWidthF(1.3);
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        p.drawEllipse(m_pointer, realmBrush, realmBrush);
    }
}
QString fmtLy(double u, double ly)
{
    const double v = u * (ly / 2);
    const QLocale loc;
    if (v >= 1000) return QCoreApplication::translate("galaxy::Canvas", "%1 mil anos-luz").arg(loc.toString(v / 1000, 'f', v < 10000 ? 1 : 0));
    return QCoreApplication::translate("galaxy::Canvas", "%1 anos-luz").arg(loc.toString(v, 'f', v < 10 ? 1 : 0));
}

QImage Canvas::exportImage(double scale, bool withTitle)
{
    const int PW = int(width() * scale + 0.5), PH = int(height() * scale + 0.5);
    const double savedD = m_dpr;
    QImage savedF = m_frame;
    m_dpr = scale;
    m_frame = QImage(PW, PH, QImage::Format_ARGB32_Premultiplied);
    m_frame.setDevicePixelRatio(scale);
    m_fx.key.clear(); m_bgKey.clear(); m_art.key.clear(); m_toned.clear();
    beginFrame(4000);
    {
        QPainter p(&m_frame);
        m_exporting = true;
        render(p);
        m_exporting = false;
        if (withTitle) {
            const QColor fg = m_sk ? m_sk->inkC() : QColor(0xf2, 0xf6, 0xfc), shadow = m_sk ? m_sk->paperC() : QColor(0, 0, 0, 204);
            QFont f(QStringLiteral("IM FELL English SC")); f.setPixelSize(26);
            p.setFont(f);
            p.setPen(shadow); p.drawText(QPointF(25, 41), doc->labelOf(QStringLiteral("GAL")));
            p.setPen(fg); p.drawText(QPointF(24, 40), doc->labelOf(QStringLiteral("GAL")));
            int n = 0;
            for (const auto& s : doc->gal->systems) if (!s->gone) n++;
            const QLocale loc;
            const QString sub = QCoreApplication::translate("galaxy::Canvas", "%1 anos-luz · %2 sistemas").arg(loc.toString(doc->gal->ly, 'f', 0)).arg(n);
            p.setFont(uiFont(400, 13));
            p.setPen(shadow); p.drawText(QPointF(25, 61), sub);
            p.setPen(m_sk ? m_sk->inkC() : QColor(0xc8, 0xd2, 0xe0)); p.drawText(QPointF(24, 60), sub);
        }
    }
    QImage out = m_frame;
    out.setDevicePixelRatio(1);
    m_dpr = savedD;
    m_frame = savedF;
    m_fx.key.clear(); m_bgKey.clear(); m_art.key.clear(); m_toned.clear();
    m_dirty = true;
    return out;
}

} // namespace galaxy
