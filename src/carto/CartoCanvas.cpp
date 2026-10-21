#include "CartoCanvas.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QThreadPool>
#include <QWheelEvent>

#include <algorithm>
#include <chrono>

namespace carto {

namespace {
// azulejo do mar (grão de ±3%); com tinta, o grão vira papel ou o azul claro do Atlas
QImage makeTile(const RGB* tint)
{
    std::vector<uint8_t> t = oceanTile(256, 99);
    if (tint) for (size_t i = 0; i < t.size(); i += 4) {
        const double n = t[i] - kOcean[0];
        t[i] = u8c((*tint)[0] + n * 0.6); t[i + 1] = u8c((*tint)[1] + n * 0.6); t[i + 2] = u8c((*tint)[2] + n * 0.6);
    }
    return QImage(t.data(), 256, 256, 256 * 4, QImage::Format_RGBA8888).copy();
}
using Clock = std::chrono::steady_clock;
inline double msSince(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }
}

Canvas::Canvas(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_pool = new QThreadPool(this);
    m_pool->setMaxThreadCount(1);
    m_settle.setSingleShot(true);
    m_settle.setInterval(220);
    connect(&m_settle, &QTimer::timeout, this, &Canvas::requestFull);
    m_strokeTick.setSingleShot(true);
    m_strokeTick.setInterval(0);
    connect(&m_strokeTick, &QTimer::timeout, this, [this]() {
        if (!m_stroke || !m_stroke->pending) return;
        paintBox(*m_stroke->pending, true);
        m_stroke->pending.reset();
        update();
    });
    const RGB paper = { 236, 228, 207 }, atlas = { 170, 198, 206 };
    m_oceanTile = makeTile(nullptr);
    m_paperTile = makeTile(&paper);
    m_atlasTile = makeTile(&atlas);
}

Canvas::~Canvas()
{
    m_pool->waitForDone();
}

double Canvas::sq() const { View v; v.pitch = m_pitch; return cameraOf(v).sq; }

View Canvas::currentView(int step) const
{
    View v;
    v.ang = m_ang; v.pitch = m_pitch; v.step = step;
    v.style = m_mode == QLatin1String("atlas") ? Style::Atlas : m_mode == QLatin1String("ink") ? Style::Ink
            : m_style == QLatin1String("mira") ? Style::Mira : Style::Rich;
    v.info = infoOn() && m_doc;
    return v;
}

// territórios pintados no mapa: a camada de informações em dia com o terreno.
// Refazer a camada troca o objeto que a pintura de fundo lê: espera ela antes.
const RealmPaint* Canvas::prepareRealms()
{
    if (!infoOn() || !m_doc) return nullptr;
    if (!m_doc->info || m_doc->infoVer != m_doc->ver) waitIdle();
    return &m_doc->ensureInfo();
}

void Canvas::setDoc(Doc* d, bool refit)
{
    waitIdle();
    m_doc = d;
    m_world = d ? &d->world : nullptr;
    World* w = m_world;
    m_base.reset();
    m_present = QImage();
    m_clouds = QImage();
    sceneChanged();
    m_selArt.clear(); m_selName.clear();
    m_roadFrom.reset(); m_bridgeStart.reset();
    if (!w) { update(); return; }
    const Clouds c = clouds(*w, w->seed);
    m_clouds = QImage(c.rgba.data(), c.W, c.H, c.W * 4, QImage::Format_RGBA8888).copy();
    m_cloudPad = c.pad;
    if (refit) fit();
    paintPreview(3);
    requestFull();
}

void Canvas::layersReplaced()
{
    sceneChanged();
    if (m_world) requestFull();
    update();
}

void Canvas::worldReplaced()
{
    if (!m_world) return;
    sceneChanged();
    const Clouds c = clouds(*m_world, m_world->seed);
    m_clouds = QImage(c.rgba.data(), c.W, c.H, c.W * 4, QImage::Format_RGBA8888).copy();
    m_cloudPad = c.pad;
    requestFull();
}

void Canvas::waitIdle()
{
    m_settle.stop();
    m_pool->waitForDone();
    // a pintura que acabou de terminar ainda está na fila: entrega agora (senão ela chega depois e apaga a edição)
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
}

// ── pintura ──
void Canvas::paintPreview(int step)
{
    if (!m_world) return;
    waitIdle();
    const auto t0 = Clock::now();
    const RealmPaint* realms = prepareRealms();
    auto r = std::make_shared<Render>(carto::render(*m_world, currentView(step), realms));
    m_timing = tr("prévia em %1 ms").arg(qRound(msSince(t0)));
    adoptBase(r);
}

void Canvas::requestFull()
{
    if (!m_world) return;
    m_settle.stop();
    const RealmPaint* realms = prepareRealms();
    const int gen = ++m_gen;
    World* w = m_world;
    const View v = currentView(1);
    QPointer<Canvas> self(this);
    m_jobs++;
    emit statusChanged();
    m_pool->start([self, gen, w, v, realms]() {
        const auto t0 = Clock::now();
        auto r = std::make_shared<Render>(carto::render(*w, v, realms));
        const double ms = msSince(t0);
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, gen, r, ms]() {
            if (!self) return;
            self->m_jobs--;
            if (gen == self->m_gen) {
                self->m_timing = tr("pintou em %1 ms").arg(qRound(ms));
                self->adoptBase(r);
            }
            emit self->statusChanged();
        }, Qt::QueuedConnection);
    });
}

void Canvas::adoptBase(std::shared_ptr<Render> r)
{
    m_base = std::move(r);
    if (!m_base) return;
    const Geom& g = m_base->g;
    if (m_present.width() != g.W || m_present.height() != g.H)
        m_present = QImage(g.W, g.H, QImage::Format_ARGB32_Premultiplied);
    refreshLight();
    if (m_showClimate) refreshClimate();
    update();
}

void Canvas::refreshLight()
{
    if (!m_base) return;
    m_lut = sunLUT(m_hour, currentView(1).style);
    m_litHour = m_hour;
    presentRect(0, 0, m_base->g.W, m_base->g.H);
}

void Canvas::presentRect(int x0, int y0, int x1, int y1)
{
    if (!m_base || x1 <= x0 || y1 <= y0) return;
    const Geom& g = m_base->g;
    const uint8_t* src = m_base->data.data();
    if (!m_lut.empty() && !m_base->aux.nrm.empty()) {
        if (m_lit.size() != m_base->data.size()) m_lit.assign(m_base->data.size(), 0);
        relight(m_base->data.data(), m_base->aux.nrm.data(), m_base->aux.depth.data(), m_lit.data(), g.W, x0, y0, x1, y1, m_lut);
        src = m_lit.data();
    }
    const QImage wrap(src, g.W, g.H, g.W * 4, QImage::Format_RGBA8888);
    QPainter p(&m_present);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    const QRect rc(x0, y0, x1 - x0, y1 - y0);
    p.drawImage(rc, wrap, rc);
}

void Canvas::refreshClimate()
{
    if (!m_base || !m_world) return;
    const Geom& g = m_base->g;
    std::vector<uint8_t> raw(static_cast<size_t>(g.W) * g.H * 4, 0);
    paintClimate(*m_world, g, raw.data(), 0, 0, g.W, g.H);
    m_climate = QImage(raw.data(), g.W, g.H, g.W * 4, QImage::Format_RGBA8888).copy();
}

void Canvas::paintBox(const std::array<int, 4>& box, bool live)
{
    if (!m_base || !m_world || m_base->g.step != 1) return;
    const Geom& g = m_base->g;
    const auto v = dirtyToView(g, { double(box[0]), double(box[1]), double(box[2]), double(box[3]) }, live);
    if (v[2] <= v[0] || v[3] <= v[1]) return;
    paintRegion(*m_world, g, m_base->data.data(), v[0], v[1], v[2], v[3], &m_base->aux, nullptr, g.info && m_doc ? m_doc->info.get() : nullptr);
    presentRect(v[0], v[1], v[2], v[3]);
    if (m_showClimate && !m_climate.isNull())   // a camada de clima é barata: refaz só o pedaço, direto na imagem
        paintClimate(*m_world, g, m_climate.bits(), v[0], v[1], v[2], v[3]);
}

void Canvas::repaintBox(const std::array<int, 4>& box)
{
    waitIdle();
    paintBox(box, false);
    update();
}

// ── vista ──
void Canvas::fit()
{
    if (!m_world || width() < 10) return;
    const double s = sq();
    double x0 = 1e18, x1 = -1e18, y0 = 1e18, y1 = -1e18;
    const double cs[4][2] = { { 0, 0 }, { double(m_world->W), 0 }, { 0, double(m_world->H) }, { double(m_world->W), double(m_world->H) } };
    for (const auto& c : cs) {
        const auto p = worldToPlane(*m_world, m_ang, c[0], c[1]);
        x0 = std::min(x0, p[0]); x1 = std::max(x1, p[0]); y0 = std::min(y0, p[1]); y1 = std::max(y1, p[1]);
    }
    const double bw = x1 - x0, bh = (y1 - y0) * s;
    m_fitZoom = std::min(width() / (bw * 1.06), (height() - 150) / (bh * 1.1));
    m_zoom = m_fitZoom;
    m_panX = width() / 2.0 - m_zoom * (x0 + x1) / 2;
    m_panY = 70 + (height() - 150) / 2.0 - m_zoom * s * (y0 + y1) / 2;
    update();
    emit viewChanged();
}

void Canvas::zoomBy(double f, QPointF at)
{
    if (at.x() < 0) at = QPointF(width() / 2.0, height() / 2.0);
    const double nz = std::max(m_fitZoom * 0.35, std::min(std::max(m_fitZoom * 14, 7.0), m_zoom * f));
    m_panX = at.x() - (at.x() - m_panX) * (nz / m_zoom);
    m_panY = at.y() - (at.y() - m_panY) * (nz / m_zoom);
    m_zoom = nz;
    update();
    emit viewChanged();
}

QPointF Canvas::screenToPlane(QPointF s) const { return QPointF((s.x() - m_panX) / m_zoom, (s.y() - m_panY) / m_zoom / sq()); }
std::array<double, 2> Canvas::screenToWorld(QPointF s) const
{
    const QPointF p = screenToPlane(s);
    return planeToWorld(*m_world, m_ang, p.x(), p.y());
}

void Canvas::setView(double ang, double pitch, bool quick)
{
    if (!m_world) return;
    const QPointF c(width() / 2.0, height() / 2.0);
    const auto q = screenToWorld(c);
    m_ang = ang;
    m_pitch = std::round(std::max(20.0, std::min(90.0, pitch)));
    const auto n = worldToPlane(*m_world, m_ang, q[0], q[1]);
    m_panX = c.x() - m_zoom * n[0];
    m_panY = c.y() - m_zoom * n[1] * sq();
    if (quick) {
        const int step = m_world->W * m_world->H > 4e6 ? 5 : 3;
        paintPreview(step);
        m_settle.start();
    } else requestFull();
    emit viewChanged();
}

void Canvas::setMode(const QString& mode)
{
    if (m_mode == mode) return;
    m_mode = mode;
    if (m_base) requestFull();
    update();
    emit viewChanged();
}
void Canvas::setTerrainStyle(const QString& style)
{
    if (m_style == style) return;
    m_style = style;
    if (m_base) requestFull();
    emit viewChanged();
}
void Canvas::setHour(double h, bool force)
{
    m_hour = std::fmod(std::fmod(h, 24.0) + 24, 24.0);
    // o sol anda devagar: reacende o relevo a cada ~5 min do dia (ou na hora, se pedir)
    if (force || std::fabs(m_hour - m_litHour) > 0.08) refreshLight();
    update();
    emit hourChanged(m_hour);
}
void Canvas::setShowClimate(bool on)
{
    m_showClimate = on;
    if (on) refreshClimate();
    update();
}
void Canvas::setTool(Tool t)
{
    if (t != m_tool) {
        if (t != Tool::Build) { m_selArt.clear(); m_bridgeStart.reset(); m_roadFrom.reset(); }
        if (t != Tool::Names) m_selName.clear();
        m_toolMsg.clear();
        m_trail.clear();
    }
    m_tool = t;
    setCursor(t == Tool::Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
    update();
}

// ── desenho na tela ──
void Canvas::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    paintMap(p, size(), false);
}

// a vista atual numa imagem, sem os botões (pro livro): scale = 2 ou 3 sai nítido
// nos traços, letras e sprites; o relevo é esticado (o pintor vai até 1 px do mundo)
QImage Canvas::exportImage(double scale, const std::function<void(QPainter&)>& extra, const std::vector<QRectF>& avoid)
{
    m_avoid = avoid;
    waitIdle();
    QImage img(qRound(width() * scale), qRound(height() * scale), QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(scale);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    paintMap(p, size(), true);
    m_avoid.clear();
    if (extra) extra(p);
    p.end();
    return img;
}

void Canvas::paintMap(QPainter& p, QSize sz, bool exporting)
{
    const bool atlas = m_mode == QLatin1String("atlas"), ink = m_mode == QLatin1String("ink");
    QBrush sea(atlas ? m_atlasTile : ink ? m_paperTile : m_oceanTile);
    QTransform seaT; seaT.translate(m_panX, m_panY); seaT.scale(m_zoom, m_zoom);
    sea.setTransform(seaT);
    p.fillRect(QRect(QPoint(0, 0), sz), sea);
    if (!m_base) return;
    const Geom& g = m_base->g;
    const double s = g.step;
    const QRectF dst(-g.cu * s, -g.cv * s * g.sq, g.W * s, g.H * s * g.sq);
    p.save();
    p.translate(m_panX, m_panY);
    p.scale(m_zoom, m_zoom);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(dst, m_present);
    if (m_showClimate && !m_climate.isNull()) p.drawImage(dst, m_climate);
    p.restore();

    // cena: plantações e estradas no chão, depois o que fica de pé
    SceneState st = sceneState();
    st.vw = sz.width(); st.vh = sz.height();
    if (exporting) {
        st.namesTool = false; st.selName.clear(); st.avoid = m_avoid;
        if (st.info()) { m_scene.drawFields(p, st); m_scene.drawRoads(p, st); }
        if (!m_showClimate) m_scene.drawSprites(p, st);
    } else drawGroundLayer(p, st, sz);

    // nuvens: moram no mundo; giram com a mesa e a câmera achata
    const double cop = (m_showClimate || atlas || ink) ? 0
        : std::max(0.0, std::min(1.0, (1.4 - m_zoom / m_fitZoom) / 0.7)) * (m_tool == Tool::Brush && !exporting ? 0.35 : 1);
    if (cop > 0.01 && !m_clouds.isNull() && m_world) {
        p.save();
        p.translate(m_panX, m_panY);
        p.scale(m_zoom, m_zoom);
        p.scale(1, g.sq);
        p.rotate(g.ang * 180 / M_PI);
        p.translate(-m_world->W / 2.0, -m_world->H / 2.0);
        p.setOpacity(cop);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(QPointF(-m_cloudPad, -m_cloudPad), m_clouds);
        p.restore();
    }

    // céu da hora: multiplica a cor, acende o lado do sol e escurece os cantos de noite
    if (!atlas && !ink && !m_showClimate) {
        const TimeOfDay T = timeOfDay(m_hour);
        const QRectF R(QPointF(0, 0), QSizeF(sz));
        p.save();
        if (T.mul[0] < 254.5 || T.mul[1] < 254.5 || T.mul[2] < 254.5) {
            p.setCompositionMode(QPainter::CompositionMode_Multiply);
            p.fillRect(R, QColor(qRound(T.mul[0]), qRound(T.mul[1]), qRound(T.mul[2])));
        }
        if (T.warm[3] > 0.004) {
            const double sx = T.sun.L[0] >= 0 ? R.width() : 0;
            QLinearGradient gr(QPointF(sx, R.height() * 0.35), QPointF(R.width() - sx, R.height() * 0.65));
            gr.setColorAt(0, QColor(qRound(T.warm[0]), qRound(T.warm[1]), qRound(T.warm[2]), qRound(T.warm[3] * 255)));
            gr.setColorAt(0.75, QColor(qRound(T.warm[0]), qRound(T.warm[1]), qRound(T.warm[2]), 0));
            p.setCompositionMode(QPainter::CompositionMode_Screen);
            p.fillRect(R, gr);
        }
        if (T.night > 0.01) {
            QRadialGradient vg(R.center(), std::hypot(R.width(), R.height()) * 0.6, R.center());
            const double r0 = std::min(R.width(), R.height()) * 0.35 / (std::hypot(R.width(), R.height()) * 0.6);
            vg.setColorAt(0, Qt::white);
            vg.setColorAt(r0, Qt::white);
            vg.setColorAt(1, QColor(qRound(255 - 110 * T.night), qRound(255 - 100 * T.night), qRound(255 - 70 * T.night)));
            p.setCompositionMode(QPainter::CompositionMode_Multiply);
            p.fillRect(R, vg);
        }
        p.restore();
        if (T.night > 0.01) m_scene.drawLights(p, st, T.night);   // janelas, tochas e faróis acesos
    }

    // nomes por cima de tudo (legíveis até de noite)
    if (st.info()) m_scene.drawLabels(p, st);

    // ferramentas
    if (exporting) return;
    p.setRenderHint(QPainter::Antialiasing);
    drawToolOverlay(p, st);
    if (m_tool == Tool::Ruler) drawRuler(p, st);
    if (m_drag == Drag::Area) {
        const QRectF r = m_sel.normalized();
        p.fillRect(r, QColor(185, 164, 240, 30));
        QPen pen(QColor(185, 164, 240, 230), 1.5, Qt::DashLine);
        p.setPen(pen);
        p.drawRect(r.adjusted(0.5, 0.5, 0, 0));
    }
    if (m_tool == Tool::Brush && m_hasHover && m_drag != Drag::Pan && m_drag != Drag::Turn) {
        const double r = m_brushSize * m_zoom;
        const bool inv = QGuiApplication::keyboardModifiers() & Qt::ShiftModifier;
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 0, 0, 115), 3));
        p.drawEllipse(m_hover, r, r * sq());
        p.setPen(QPen(inv ? QColor(217, 122, 95, 242) : QColor(240, 236, 226, 242), 1.4));
        p.drawEllipse(m_hover, r, r * sq());
    }
}

// plantações, estradas e sprites numa imagem guardada com folga de 1/4 da tela em volta:
// arrastando o mapa, a imagem só se desloca; refaz quando a folga acaba ou algo muda
void Canvas::drawGroundLayer(QPainter& p, const SceneState& st, QSize sz)
{
    if (!m_doc || (!st.info() && m_showClimate)) return;
    const Info* in = m_doc->info.get();
    const SpriteSun sun = spriteSun(m_hour);
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9")
                            .arg(reinterpret_cast<quintptr>(m_base.get())).arg(m_ang, 0, 'g', 12).arg(m_pitch).arg(m_zoom, 0, 'g', 12).arg(m_fitZoom, 0, 'g', 12)
                            .arg(m_mode).arg(m_showClimate).arg(m_opts.sprites).arg(m_doc->ver)
        + QStringLiteral("|%1|%2|%3|%4|%5|%6|%7|%8x%9").arg(m_doc->infoVer).arg(in ? in->user.ver : -1).arg(in ? in->user.showGen : true)
              .arg(worldScale(*m_world)).arg(sun.L[0], 0, 'f', 3).arg(sun.L[1], 0, 'f', 3).arg(m_sceneRev).arg(sz.width()).arg(sz.height());
    const int M = qRound(std::max(sz.width(), sz.height()) * 0.25);
    const double dx = m_panX - m_cachePanX, dy = m_panY - m_cachePanY;
    if (key != m_sceneKey || std::fabs(dx) > m_cacheMargin || std::fabs(dy) > m_cacheMargin || m_sceneCache.isNull()) {
        const double dpr = devicePixelRatioF();
        QImage img(qRound((sz.width() + 2 * M) * dpr), qRound((sz.height() + 2 * M) * dpr), QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(dpr);
        img.fill(Qt::transparent);
        QPainter q(&img);
        q.translate(M, M);
        SceneState s2 = st;
        s2.cull = M + 60;
        if (s2.info()) { m_scene.drawFields(q, s2); m_scene.drawRoads(q, s2); }
        if (!m_showClimate) m_scene.drawSprites(q, s2);
        q.end();
        m_sceneCache = img;
        m_sceneKey = key;
        m_cachePanX = m_panX; m_cachePanY = m_panY;
        m_cacheMargin = M;
    }
    p.drawImage(QPointF(m_panX - m_cachePanX - m_cacheMargin, m_panY - m_cachePanY - m_cacheMargin), m_sceneCache);
}

void Canvas::resizeEvent(QResizeEvent*)
{
    const bool keep = std::fabs(m_zoom - m_fitZoom) < 1e-9;
    if (keep || !m_base) fit();
}

// ── mouse ──
void Canvas::mousePressEvent(QMouseEvent* e)
{
    setFocus();
    m_press = m_last = e->position();
    m_moved = false;
    if (!m_world) return;
    if (e->button() == Qt::RightButton) {
        m_drag = Drag::Turn; m_ang0 = m_ang; m_pitch0 = m_pitch;
        return;
    }
    const bool wantsPan = e->button() == Qt::MiddleButton || (e->button() == Qt::LeftButton && ((e->modifiers() & Qt::AltModifier) || m_tool == Tool::Pan));
    if (wantsPan) {
        m_drag = Drag::Pan; m_panX0 = m_panX; m_panY0 = m_panY;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (e->button() != Qt::LeftButton) return;
    if (m_tool == Tool::Ruler) {
        // régua: cada clique marca um ponto; depois de terminada, o próximo clique começa outra
        if (m_rulerDone) { m_ruler.clear(); m_rulerDone = false; }
        const auto q = screenToWorld(e->position());
        m_ruler.push_back({ q[0], q[1] });
        update();
        return;
    }
    if (m_tool == Tool::Realm || m_tool == Tool::Build || m_tool == Tool::Names) {
        if (toolPress(e)) m_drag = Drag::Tool;
        update();
        return;
    }
    if (m_tool == Tool::Area) { m_drag = Drag::Area; m_sel = QRectF(m_press, m_press); update(); return; }
    if (m_tool == Tool::Brush) { m_drag = Drag::Stroke; startStroke(e->position(), e->modifiers() & Qt::ShiftModifier); }
}

void Canvas::mouseMoveEvent(QMouseEvent* e)
{
    const QPointF pos = e->position();
    m_hover = pos; m_hasHover = true;
    switch (m_drag) {
    case Drag::Turn: {
        const double dx = pos.x() - m_press.x(), dy = pos.y() - m_press.y();
        if (!m_moved && std::hypot(dx, dy) < 5) return;
        m_moved = true;
        setView(m_ang0 + dx * 0.006, m_pitch0 - dy * 0.25, true);
        return;
    }
    case Drag::Pan:
        m_panX = m_panX0 + pos.x() - m_press.x();
        m_panY = m_panY0 + pos.y() - m_press.y();
        update();
        emit viewChanged();
        return;
    case Drag::Area:
        m_sel.setBottomRight(pos);
        update();
        return;
    case Drag::Stroke:
        moveStroke(pos);
        return;
    case Drag::Tool:
        toolMove(e);
        return;
    default:
        if (m_tool != Tool::Pan && m_tool != Tool::Area) { toolMove(e); update(); }
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent* e)
{
    const Drag d = m_drag;
    m_drag = Drag::None;
    if (m_tool == Tool::Pan) setCursor(Qt::OpenHandCursor);
    switch (d) {
    case Drag::Turn:
        if (m_moved) { m_settle.stop(); requestFull(); }
        else emit ringRequested(e->position().toPoint());
        break;
    case Drag::Area: {
        const QRectF r = m_sel.normalized();
        update();
        const QPointF a = screenToPlane(r.topLeft()), b = screenToPlane(r.bottomRight());
        const PlaneRect pr{ a.x(), a.y(), b.x() - a.x(), b.y() - a.y() };
        if (pr.w < 40 || pr.h < 40 || !m_world) break;
        waitIdle();
        auto before = std::make_shared<World>(*m_world);
        if (addArea(*m_world, pr, m_ang, static_cast<int>(QRandomGenerator::global()->bounded(99999)))) {
            m_doc->touch();
            emit areaAdded(before);
            worldReplaced();
        }
        break;
    }
    case Drag::Stroke:
        endStroke();
        break;
    case Drag::Tool:
        toolRelease(e);
        update();
        break;
    default:
        break;
    }
}

void Canvas::wheelEvent(QWheelEvent* e)
{
    const double dy = -e->angleDelta().y() * 100.0 / 120.0;
    if (e->modifiers() & Qt::ShiftModifier) { setView(m_ang + (dy ? dy : -e->angleDelta().x() * 100.0 / 120.0) * 0.003, m_pitch, true); return; }
    zoomBy(std::pow(1.0018, -dy), e->position());
}

void Canvas::leaveEvent(QEvent*)
{
    m_hasHover = false;
    if (m_tool != Tool::Pan) update();
}

SceneState Canvas::sceneState() const
{
    SceneState st;
    st.doc = m_doc;
    st.g = m_base ? &m_base->g : nullptr;
    st.aux = m_base ? &m_base->aux : nullptr;
    st.ang = m_ang; st.pitch = m_pitch; st.zoom = m_zoom; st.panX = m_panX; st.panY = m_panY; st.fitZoom = m_fitZoom;
    st.mode = m_mode; st.hour = m_hour; st.sprites = m_opts.sprites;
    st.vw = width(); st.vh = height();
    st.namesAuto = m_opts.namesAuto; st.density = m_opts.density;
    st.namesTool = m_tool == Tool::Names; st.selName = m_selName;
    return st;
}

// ── pincelada ──
void Canvas::startStroke(QPointF pos, bool inverse)
{
    if (!m_world) return;
    waitIdle();
    if (!m_base || m_base->g.step != 1) {
        // precisa da pintura cheia pra repintar só o pedaço
        auto r = std::make_shared<Render>(carto::render(*m_world, currentView(1), prepareRealms()));
        adoptBase(r);
    }
    m_stroke = std::make_unique<Stroke>();
    m_stroke->h = m_world->h; m_stroke->m = m_world->m; m_stroke->k = m_world->k;
    const auto q = screenToWorld(pos);
    m_stroke->lastX = q[0]; m_stroke->lastY = q[1];
    m_stroke->st.r = m_brushSize; m_stroke->st.strength = m_brushStr; m_stroke->st.inverse = inverse; m_stroke->st.riverLen = 0;
    dab(q[0], q[1], 0, 0);
    m_strokeTick.start();
}

void Canvas::dab(double qx, double qy, double nx, double ny)
{
    double x = qx, y = qy;
    if (m_brush == Brush::River) { const double o = riverOffset(*m_world, m_stroke->st.riverLen, m_stroke->st.r); x += nx * o; y += ny * o; }
    const auto b = applyDab(*m_world, m_brush, x, y, m_stroke->st);
    if (m_brush == Brush::River) m_stroke->river.push_back({ x, y });
    if (!b) return;
    auto uni = [](std::optional<std::array<int, 4>>& a, const std::array<int, 4>& c) {
        if (!a) { a = c; return; }
        (*a)[0] = std::min((*a)[0], c[0]); (*a)[1] = std::min((*a)[1], c[1]); (*a)[2] = std::max((*a)[2], c[2]); (*a)[3] = std::max((*a)[3], c[3]);
    };
    uni(m_stroke->box, *b);
    uni(m_stroke->pending, *b);
}

void Canvas::moveStroke(QPointF pos)
{
    if (!m_stroke) return;
    const auto q = screenToWorld(pos);
    const double lx = m_stroke->lastX, ly = m_stroke->lastY;
    const double d = std::hypot(q[0] - lx, q[1] - ly);
    const double spacing = std::max(1.5, m_stroke->st.r * 0.25);
    if (d < spacing) return;
    const int n = static_cast<int>(std::ceil(d / spacing));
    const double nx = -(q[1] - ly) / d, ny = (q[0] - lx) / d;
    for (int i = 1; i <= n; i++) {
        m_stroke->st.riverLen += d / n;
        dab(lx + (q[0] - lx) * i / n, ly + (q[1] - ly) * i / n, nx, ny);
    }
    m_stroke->lastX = q[0]; m_stroke->lastY = q[1];
    if (!m_strokeTick.isActive()) m_strokeTick.start();
}

void Canvas::endStroke()
{
    if (!m_stroke) return;
    m_strokeTick.stop();
    std::unique_ptr<Stroke> s = std::move(m_stroke);
    if (!s->box) { update(); return; }
    m_doc->touch();
    int riverId = 0;
    if (s->river.size() >= 4) {
        River rv; rv.id = ++m_doc->riverSeq; rv.pts = resample(s->river, 6);
        m_doc->rivers.push_back(rv);
        riverId = rv.id;
    }
    const auto& b = *s->box;
    Snapshot before = snapshotRect(*m_world, s->h, s->m, s->k, b[0], b[1], b[2], b[3]);
    Snapshot after = snapshotRect(*m_world, m_world->h, m_world->m, m_world->k, b[0], b[1], b[2], b[3]);
    const auto t0 = Clock::now();
    paintBox(b, false);            // passada exata: acerta o mar raso longe da pincelada
    m_timing = tr("acabamento em %1 ms").arg(qRound(msSince(t0)));
    update();
    emit strokeFinished(before, after, riverId);
    emit statusChanged();
}

} // namespace carto
