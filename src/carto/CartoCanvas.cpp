#include "CartoCanvas.h"
#include "CartoProf.h"

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
    m_cacheTimer.setSingleShot(true);
    m_cacheTimer.setInterval(140);
    connect(&m_cacheTimer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    m_tileTimer.setSingleShot(true);
    m_tileTimer.setInterval(0);
    connect(&m_tileTimer, &QTimer::timeout, this, &Canvas::prefetchTile);
    m_lastMove.start();
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
    CARTO_PROF("pôr o mapa na mesa");
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
    startClouds();
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
    CARTO_PROF("mundo trocado");
    if (!m_world) return;
    sceneChanged();
    startClouds();
    requestFull();
}

void Canvas::waitIdle()
{
    CARTO_PROF("esperar pintura de fundo");
    const bool wasPending = m_settle.isActive();
    m_settle.stop();
    // a pintura que estiver no meio fica velha com a edição: para agora (e volta depois, já com a edição)
    bool cancelled = false;
    if (m_cancel && m_jobs.load() > 0) { m_cancel->store(true); cancelled = true; }
    m_pool->waitForDone();
    if (cancelled || wasPending) m_settle.start();
    // a pintura que acabou de terminar ainda está na fila: entrega agora (senão ela chega depois e apaga a edição)
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
}

// ── pintura ──
void Canvas::paintPreview(int step)
{
    CARTO_PROF("prévia (síncrona)");
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
    if (m_cancel) m_cancel->store(true);               // a anterior ficou velha
    auto cancel = std::make_shared<std::atomic<bool>>(false);
    m_cancel = cancel;
    const int gen = ++m_gen;
    World* w = m_world;
    const View v = currentView(lodStep());
    QPointer<Canvas> self(this);
    m_jobs++;
    emit statusChanged();
    m_pool->start([self, gen, w, v, realms, cancel]() {
        const auto t0 = Clock::now();
        auto r = std::make_shared<Render>(carto::render(*w, v, realms, true, 0, cancel.get()));
        const double ms = msSince(t0);
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, gen, r, ms, cancel]() {
            if (!self) return;
            self->m_jobs--;
            if (gen == self->m_gen && !cancel->load()) {
                self->m_timing = tr("pintou em %1 ms").arg(qRound(ms));
                self->adoptBase(r);
            }
            emit self->statusChanged();
        }, Qt::QueuedConnection);
    });
}

void Canvas::adoptBase(std::shared_ptr<Render> r)
{
    CARTO_PROF("adotar pintura");
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
    CARTO_PROF("luz da hora (relight)");
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
    CARTO_PROF("camada de clima");
    if (!m_base || !m_world) return;
    const Geom& g = m_base->g;
    std::vector<uint8_t> raw(static_cast<size_t>(g.W) * g.H * 4, 0);
    paintClimate(*m_world, g, raw.data(), 0, 0, g.W, g.H);
    m_climate = QImage(raw.data(), g.W, g.H, g.W * 4, QImage::Format_RGBA8888).copy();
}

void Canvas::paintBox(const std::array<int, 4>& box, bool live)
{
    CARTO_PROF("repintar pedaço");
    if (!m_base || !m_world) return;
    const Geom& g = m_base->g;
    const auto v = dirtyToView(g, { double(box[0]), double(box[1]), double(box[2]), double(box[3]) }, live);
    if (v[2] <= v[0] || v[3] <= v[1]) return;
    paintBands(*m_world, g, m_base->data.data(), v[0], v[1], v[2], v[3], &m_base->aux, g.info && m_doc ? m_doc->info.get() : nullptr);
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
    m_lastMove.restart();
    if (m_base && m_base->g.step != lodStep()) m_settle.start();   // chegou perto (ou longe): outro nível de detalhe
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
    m_lastMove.restart();
    if (quick) {
        // a imagem que existe gira e achata na hora; a nítida vem quando a mão para
        m_settle.start();
        update();
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
    setCursor(t == Tool::Pan ? Qt::OpenHandCursor : t == Tool::Explore ? Qt::ArrowCursor : Qt::CrossCursor);
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
    CARTO_PROF("desenhar a mesa");
    const bool atlas = m_mode == QLatin1String("atlas"), ink = m_mode == QLatin1String("ink");
    QBrush sea(atlas ? m_atlasTile : ink ? m_paperTile : m_oceanTile);
    QTransform seaT; seaT.translate(m_panX, m_panY); seaT.scale(m_zoom, m_zoom);
    sea.setTransform(seaT);
    p.fillRect(QRect(QPoint(0, 0), sz), sea);
    if (!m_base) return;
    const Geom& g = m_base->g;
    const bool matches = baseMatches();
    p.save();
    p.setTransform(baseTransform(), true);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(QPointF(0, 0), m_present);
    if (m_showClimate && !m_climate.isNull()) p.drawImage(QPointF(0, 0), m_climate);
    p.restore();

    // cena: plantações e estradas no chão, depois o que fica de pé
    SceneState st = sceneState();
    st.vw = sz.width(); st.vh = sz.height();
    st.quick = !exporting && m_lastMove.elapsed() < 130;
    if (exporting) {
        st.namesTool = false; st.selName.clear(); st.avoid = m_avoid;
        if (st.info()) { m_scene.drawFields(p, st); m_scene.drawRoads(p, st); }
        if (!m_showClimate) m_scene.drawSprites(p, st);
    } else drawGroundLayer(p, st, sz);
    // girando ou inclinando, a pintura ainda é da vista anterior: o que pisa no chão espera ela chegar
    const bool steady = matches || exporting;

    // nuvens: moram no mundo; giram com a mesa e a câmera achata
    const double cop = (m_showClimate || atlas || ink) ? 0
        : std::max(0.0, std::min(1.0, (1.4 - m_zoom / m_fitZoom) / 0.7)) * (m_tool == Tool::Brush && !exporting ? 0.35 : 1);
    if (cop > 0.01 && !m_clouds.isNull() && m_world) {
        p.save();
        p.translate(m_panX, m_panY);
        p.scale(m_zoom, m_zoom);
        p.scale(1, sq());
        p.rotate(m_ang * 180 / M_PI);
        p.translate(-m_world->W / 2.0, -m_world->H / 2.0);
        p.setOpacity(cop);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(QRectF(-m_cloudPad, -m_cloudPad, m_clouds.width() * m_cloudDs, m_clouds.height() * m_cloudDs), m_clouds);
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
        if (T.night > 0.01 && steady) m_scene.drawLights(p, st, T.night);   // janelas, tochas e faróis acesos
    }

    // nomes por cima de tudo (legíveis até de noite)
    if (st.info() && steady) {
        m_scene.drawLabels(p, st);
        if (m_scene.labelsPending()) m_cacheTimer.start();   // nome com carimbo emprestado: o certo vem quando a mão parar
    }

    // ferramentas
    if (exporting) return;
    p.setRenderHint(QPainter::Antialiasing);
    if (steady) drawToolOverlay(p, st);
    if (steady && m_tool == Tool::Explore) drawExplore(p, st);
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

// plantações, estradas e sprites em azulejos presos ao mapa (coordenadas da tela sem o arrasto):
// arrastando, os azulejos só se deslocam e só o que entra na tela é desenhado
namespace { qint64 tileId(int i, int j) { return (static_cast<qint64>(i) << 32) ^ static_cast<quint32>(j); } }

QString Canvas::groundKey(bool withUser) const
{
    const Info* in = m_doc ? m_doc->info.get() : nullptr;
    const SpriteSun sun = spriteSun(m_hour);
    QString k = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9")
                    .arg(reinterpret_cast<quintptr>(m_base.get())).arg(m_ang, 0, 'g', 12).arg(m_pitch).arg(m_zoom, 0, 'g', 12).arg(m_fitZoom, 0, 'g', 12)
                    .arg(m_mode).arg(m_showClimate).arg(m_opts.sprites).arg(m_doc && m_doc->info ? m_doc->infoVer : -1)
        + QStringLiteral("|%1|%2|%3|%4|%5").arg(in ? in->user.showGen : true)
              .arg(m_world ? worldScale(*m_world) : 1).arg(sun.L[0], 0, 'f', 3).arg(sun.L[1], 0, 'f', 3).arg(devicePixelRatioF());
    if (withUser) k += QStringLiteral("|v%1|u%2|r%3").arg(m_doc ? m_doc->ver : -1).arg(in ? in->user.ver : -1).arg(m_sceneRev);
    return k;
}

double Canvas::maxLift() const
{
    if (!m_base || m_base->aux.lift.empty()) return 0;
    CARTO_PROF("relevo mais alto");
    float m = 0;
    for (float v : m_base->aux.lift) m = std::max(m, v);
    return m * m_base->g.step;
}

Canvas::TileBasis Canvas::tileBasis() const
{
    TileBasis b;
    b.keyNoUser = groundKey(false);
    b.docVer = m_doc ? m_doc->ver : -1;
    if (const Info* in = m_doc ? m_doc->info.get() : nullptr) {
        b.user = in->user;
        for (const Building& o : in->buildings) b.bRealm.push_back(o.realm);
        for (const Realm& r : in->realms) b.colors.push_back(r.color);
    }
    return b;
}

namespace {
// o que mudou entre duas camadas do usuário: os pontos (do mundo) de tudo que entrou, saiu ou mexeu
bool userDiff(const UserLayer& a, const UserLayer& b, std::vector<Pt>& pts)
{
    if (a.showGen != b.showGen || a.scale != b.scale) return false;
    auto each = [&](const auto& va, const auto& vb, auto same, auto add) {
        const size_t n = std::max(va.size(), vb.size());
        for (size_t i = 0; i < n; i++) {
            if (i < va.size() && i < vb.size() && same(va[i], vb[i])) continue;
            if (i < va.size()) add(va[i]);
            if (i < vb.size()) add(vb[i]);
        }
    };
    auto path = [&](const Path& p) { pts.insert(pts.end(), p.begin(), p.end()); };
    each(a.arts, b.arts, [](const Art& x, const Art& y) { return x.id == y.id && x.kind == y.kind && x.x == y.x && x.y == y.y && x.yaw == y.yaw && x.size == y.size && x.realm == y.realm && x.v == y.v; },
         [&](const Art& x) { pts.push_back({ x.x, x.y }); });
    each(a.houses, b.houses, [](const House& x, const House& y) { return x.x == y.x && x.y == y.y && x.yaw == y.yaw && x.floors == y.floors && x.timber == y.timber && x.v == y.v; },
         [&](const House& x) { pts.push_back({ x.x, x.y }); });
    each(a.walls, b.walls, [](const Wall& x, const Wall& y) { return x.pts == y.pts; }, [&](const Wall& x) { path(x.pts); });
    each(a.fields, b.fields, [](const Field& x, const Field& y) { return x.x == y.x && x.y == y.y && x.w == y.w && x.d == y.d && x.yaw == y.yaw && x.crop == y.crop; },
         [&](const Field& x) { const double r = std::max(x.w, x.d); pts.push_back({ x.x - r, x.y - r }); pts.push_back({ x.x + r, x.y + r }); pts.push_back({ x.x - r, x.y + r }); pts.push_back({ x.x + r, x.y - r }); });
    each(a.bridges, b.bridges, [](const Path& x, const Path& y) { return x == y; }, path);
    each(a.roads, b.roads, [](const Road& x, const Road& y) { return x.kind == y.kind && x.pts == y.pts; }, [&](const Road& x) { path(x.pts); });
    return true;
}
QString roadsSig(const UserLayer& U)
{
    size_t h = qHash(U.roads.size());
    for (const Road& r : U.roads) {
        h = qHashMulti(h, r.kind, r.pts.size());
        for (const Pt& q : r.pts) h = qHashMulti(h, q[0], q[1]);
    }
    return QString::number(h, 16);
}
}

QImage Canvas::renderTile(const SceneState& st, int i, int j)
{
    CARTO_PROF("azulejo do chão");
    const double dpr = devicePixelRatioF();
    QImage img(qRound(kTile * dpr), qRound(kTile * dpr), QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::transparent);
    QPainter q(&img);
    SceneState s2 = st;
    s2.panX = -static_cast<double>(i) * kTile;
    s2.panY = -static_cast<double>(j) * kTile;
    s2.vw = s2.vh = kTile;
    s2.clip = QRectF(0, 0, kTile, kTile);
    s2.planKey = m_sceneKey;
    s2.roadKey = m_roadKey;
    if (s2.info()) { m_scene.drawFields(q, s2); m_scene.drawRoads(q, s2); }
    if (!m_showClimate) m_scene.drawSprites(q, s2);
    q.end();
    return img;
}

void Canvas::drawGroundLayer(QPainter& p, const SceneState& st, QSize sz)
{
    if (!m_doc || (!st.info() && m_showClimate)) return;
    const QString key = groundKey();
    const bool viewSame = key == m_sceneKey && m_ang == m_cacheAng && sq() == m_cacheSq && m_zoom == m_cacheZoom;
    // girando, inclinando ou no zoom: os azulejos guardados vão junto com o mapa
    if (!m_tiles.isEmpty() && !viewSame && (!baseMatches() || m_lastMove.elapsed() < 130)) {
        const QTransform T = QTransform::fromTranslate(-m_cachePanX, -m_cachePanY) * QTransform::fromScale(1 / m_cacheZoom, 1 / (m_cacheZoom * m_cacheSq))
                           * QTransform().rotateRadians(m_ang - m_cacheAng) * QTransform::fromScale(m_zoom, m_zoom * sq()) * QTransform::fromTranslate(m_panX, m_panY);
        p.save();
        p.setTransform(T, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        for (auto it = m_tiles.cbegin(); it != m_tiles.cend(); ++it) {
            const int i = static_cast<int>(it.key() >> 32), j = static_cast<int>(static_cast<qint32>(it.key() & 0xffffffff));
            p.drawImage(QPointF(m_cachePanX + static_cast<double>(i) * kTile, m_cachePanY + static_cast<double>(j) * kTile), it.value());
        }
        p.restore();
        m_tileTimer.stop();
        m_cacheTimer.start();                 // refaz quando a mão parar
        return;
    }
    if (!baseMatches()) return;               // ainda sem pintura da vista de agora e sem nada guardado
    if (!viewSame) {
        // mesma vista e mesmo mapa, só a camada do usuário mudou: refaz só os azulejos em volta do que mexeu
        bool partial = false;
        const Info* in = m_doc->info.get();
        if (m_tileBasis && !m_tiles.isEmpty() && in && m_ang == m_cacheAng && sq() == m_cacheSq && m_zoom == m_cacheZoom) {
            TileBasis now = tileBasis();
            std::vector<Pt> pts;
            const bool sameTerrain = now.docVer == m_tileBasis->docVer;
            const bool stroked = m_terrainDirty && m_terrainDirty->from == m_tileBasis->docVer && m_terrainDirty->to == now.docVer;
            if (now.keyNoUser == m_tileBasis->keyNoUser && (sameTerrain || stroked) && now.bRealm == m_tileBasis->bRealm && now.colors == m_tileBasis->colors
                && userDiff(m_tileBasis->user, now.user, pts)) {
                CARTO_PROF("azulejos: só o que mexeu");
                const double ks = worldScale(*m_world), big = (40 * ks + 12) * m_zoom + 24;
                double x0 = INFINITY, y0 = INFINITY, x1 = -INFINITY, y1 = -INFINITY;
                double up = 1.6 * big;
                if (stroked) {
                    // o pedaço pincelado, amostrado numa grade; pra cima vai até onde o relevo mais alto
                    // (de antes ou de agora) pode tapar ou destapar o que fica atrás
                    const auto& b = m_terrainDirty->box;
                    const int n = 16;
                    for (int a = 0; a <= n; a++) for (int c = 0; c <= n; c++)
                        pts.push_back({ b[0] + (b[2] - b[0]) * double(a) / n, b[1] + (b[3] - b[1]) * double(c) / n });
                    now.maxLift = std::max(m_tileBasis->maxLift, maxLift());
                    up += m_zoom * sq() * now.maxLift;
                } else now.maxLift = m_tileBasis->maxLift;
                for (const Pt& q : pts) {
                    const auto g = m_scene.ground(st, q[0], q[1]);
                    if (!g) continue;
                    const double X = g->X - st.panX, Y = g->Y - st.panY;
                    x0 = std::min(x0, X); y0 = std::min(y0, Y); x1 = std::max(x1, X); y1 = std::max(y1, Y);
                }
                if (x0 <= x1) {
                    x0 -= big; x1 += big; y0 -= up; y1 += big;
                    for (auto it = m_tiles.begin(); it != m_tiles.end();) {
                        const int i = static_cast<int>(it.key() >> 32), j = static_cast<int>(static_cast<qint32>(it.key() & 0xffffffff));
                        const bool hit = i * double(kTile) < x1 && (i + 1) * double(kTile) > x0 && j * double(kTile) < y1 && (j + 1) * double(kTile) > y0;
                        if (hit) it = m_tiles.erase(it); else ++it;
                    }
                }
                partial = true;
                m_tileBasis = std::move(now);
            }
        }
        if (!partial) {
            m_tiles.clear();
            m_tileBasis = tileBasis();
        }
        m_terrainDirty.reset();
        m_sceneKey = key;
        m_roadKey = m_tileBasis->keyNoUser + QStringLiteral("|v%1|rd").arg(m_tileBasis->docVer) + (in ? roadsSig(in->user) : QString());
        m_cacheAng = m_ang; m_cacheSq = sq(); m_cacheZoom = m_zoom;
    }
    m_cachePanX = m_panX; m_cachePanY = m_panY;
    const int i0 = static_cast<int>(std::floor(-m_panX / kTile)), i1 = static_cast<int>(std::floor((sz.width() - m_panX - 1) / kTile));
    const int j0 = static_cast<int>(std::floor(-m_panY / kTile)), j1 = static_cast<int>(std::floor((sz.height() - m_panY - 1) / kTile));
    // longe demais da tela: sai da memória
    for (auto it = m_tiles.begin(); it != m_tiles.end();) {
        const int i = static_cast<int>(it.key() >> 32), j = static_cast<int>(static_cast<qint32>(it.key() & 0xffffffff));
        if (i < i0 - 2 || i > i1 + 2 || j < j0 - 2 || j > j1 + 2) it = m_tiles.erase(it); else ++it;
    }
    for (int j = j0; j <= j1; j++)
        for (int i = i0; i <= i1; i++) {
            auto it = m_tiles.find(tileId(i, j));
            if (it == m_tiles.end()) it = m_tiles.insert(tileId(i, j), renderTile(st, i, j));
            p.drawImage(QPointF(m_panX + static_cast<double>(i) * kTile, m_panY + static_cast<double>(j) * kTile), it.value());
        }
    // a volta da tela, adiantada aos poucos (um azulejo por vez entre os quadros)
    bool ring = false;
    for (int j = j0 - 1; j <= j1 + 1 && !ring; j++)
        for (int i = i0 - 1; i <= i1 + 1 && !ring; i++) ring = !m_tiles.contains(tileId(i, j));
    if (ring && !m_tileTimer.isActive()) m_tileTimer.start();
}

void Canvas::prefetchTile()
{
    if (!m_doc || !m_base || m_tiles.isEmpty() || !baseMatches()) return;
    if (groundKey() != m_sceneKey || m_ang != m_cacheAng || sq() != m_cacheSq || m_zoom != m_cacheZoom) return;
    if (m_lastMove.elapsed() < 130 && m_drag != Drag::Pan) return;     // girando ou no zoom: espera
    SceneState st = sceneState();
    st.vw = width(); st.vh = height();
    if (!st.info() && m_showClimate) return;
    const int i0 = static_cast<int>(std::floor(-m_panX / kTile)), i1 = static_cast<int>(std::floor((width() - m_panX - 1) / kTile));
    const int j0 = static_cast<int>(std::floor(-m_panY / kTile)), j1 = static_cast<int>(std::floor((height() - m_panY - 1) / kTile));
    const double cx = (i0 + i1) / 2.0, cy = (j0 + j1) / 2.0;
    int bi = 0, bj = 0;
    double best = INFINITY;
    for (int j = j0 - 1; j <= j1 + 1; j++)
        for (int i = i0 - 1; i <= i1 + 1; i++)
            if (!m_tiles.contains(tileId(i, j))) { const double d = std::hypot(i - cx, j - cy); if (d < best) { best = d; bi = i; bj = j; } }
    if (best == INFINITY) return;
    m_tiles.insert(tileId(bi, bj), renderTile(st, bi, bj));
    m_tileTimer.start();
}

void Canvas::startClouds()
{
    if (!m_world) return;
    const int ds = std::max(1, static_cast<int>(std::floor(m_world->W / 1500.0)));
    const QString key = QStringLiteral("%1|%2|%3").arg(m_world->seed).arg(m_world->W).arg(m_world->H);
    if (key == m_cloudKey && !m_clouds.isNull()) return;
    m_cloudKey = key;
    m_clouds = QImage();
    World shape;   // as nuvens só leem o tamanho do mundo: não segura o mundo de verdade na outra linha
    shape.W = m_world->W; shape.H = m_world->H;
    const int seed = m_world->seed;
    QPointer<Canvas> self(this);
    QThreadPool::globalInstance()->start([self, shape, seed, ds, key]() {
        const Clouds c = clouds(shape, seed, ds);
        QImage img = QImage(c.rgba.data(), c.W, c.H, c.W * 4, QImage::Format_RGBA8888).copy();
        const int pad = c.pad;
        QMetaObject::invokeMethod(qApp, [self, img, pad, ds, key]() {
            if (!self || self->m_cloudKey != key) return;
            self->m_clouds = img; self->m_cloudPad = pad; self->m_cloudDs = ds;
            self->update();
        }, Qt::QueuedConnection);
    });
}

bool Canvas::baseMatches() const
{
    return m_base && m_base->g.ang == m_ang && m_base->g.sq == sq();
}

int Canvas::lodStep() const
{
    const double k = m_zoom * devicePixelRatioF();   // px da tela por px do mundo
    return k >= 0.75 ? 1 : k >= 0.42 ? 2 : k >= 0.28 ? 3 : 4;
}

QTransform Canvas::baseTransform() const
{
    const Geom& g = m_base->g;
    const double s = g.step;
    return QTransform::fromScale(s, s) * QTransform::fromTranslate(-g.cu * s, -g.cv * s) * QTransform().rotateRadians(m_ang - g.ang)
         * QTransform::fromScale(m_zoom, m_zoom * sq()) * QTransform::fromTranslate(m_panX, m_panY);
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
    // Explorar: arrastar move o mapa; clicar sem arrastar escolhe (no soltar)
    const bool wantsPan = e->button() == Qt::MiddleButton || (e->button() == Qt::LeftButton && ((e->modifiers() & Qt::AltModifier) || m_tool == Tool::Pan || m_tool == Tool::Explore));
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
        if (m_tool == Tool::Explore) { exploreMove(pos); return; }
        if (m_tool != Tool::Pan && m_tool != Tool::Area) { toolMove(e); update(); }
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent* e)
{
    const Drag d = m_drag;
    m_drag = Drag::None;
    if (m_tool == Tool::Pan) setCursor(Qt::OpenHandCursor);
    if (m_tool == Tool::Explore && d == Drag::Pan && e->button() == Qt::LeftButton) {
        setCursor(Qt::ArrowCursor);
        const QPointF dp = e->position() - m_press;
        if (std::hypot(dp.x(), dp.y()) < 4) {
            const QString link = exploreLinkAt(e->position());
            emit exploreClicked(link.isEmpty() ? realmAtScreen(e->position()) : -1, link);
        }
        exploreMove(e->position());
        return;
    }
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
    if (m_tool == Tool::Explore) { m_xhovRealm = -1; m_xhovLink.clear(); emit exploreHover(-1, QString(), QPoint()); }
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
    if (m_doc) { st.namesAuto = m_doc->labels.autoOn; st.density = m_doc->labels.density; }
    st.namesTool = m_tool == Tool::Names; st.selName = m_selName;
    return st;
}

// ── pincelada ──
void Canvas::startStroke(QPointF pos, bool inverse)
{
    CARTO_PROF("pincel: apertar");
    if (!m_world) return;
    waitIdle();
    if (!m_base || !baseMatches()) {
        // a pintura precisa ser da vista de agora pra repintar só o pedaço
        auto r = std::make_shared<Render>(carto::render(*m_world, currentView(lodStep()), prepareRealms()));
        adoptBase(r);
    }
    // o relevo mais alto de antes: quem a pincelada destapar atrás dele também se refaz
    if (m_tileBasis) m_tileBasis->maxLift = std::max(m_tileBasis->maxLift, maxLift());
    m_stroke = std::make_unique<Stroke>();
    m_stroke->bw = (m_world->W + 63) / 64;
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
    {   // guarda o "antes" dos blocos que esta carimbada pode tocar
        const World& w = *m_world;
        const double rr = m_stroke->st.r * 1.3 + 2;
        const int bx0 = std::max(0, static_cast<int>(std::floor(x - rr)) / 64), bx1 = std::min(w.W - 1, static_cast<int>(std::ceil(x + rr))) / 64;
        const int by0 = std::max(0, static_cast<int>(std::floor(y - rr)) / 64), by1 = std::min(w.H - 1, static_cast<int>(std::ceil(y + rr))) / 64;
        for (int by = by0; by <= by1; by++) for (int bx = bx0; bx <= bx1; bx++) {
            const int key = by * m_stroke->bw + bx;
            if (m_stroke->saved.count(key)) continue;
            Stroke::Blk& b = m_stroke->saved[key];
            const int X0 = bx * 64, Y0 = by * 64, X1 = std::min(w.W, X0 + 64), Y1 = std::min(w.H, Y0 + 64), ww = X1 - X0;
            for (auto* f : { &b.h, &b.m, &b.k }) f->resize(static_cast<size_t>(ww) * (Y1 - Y0));
            for (int yy = Y0; yy < Y1; yy++) {
                const size_t o = static_cast<size_t>(yy) * w.W + X0, d = static_cast<size_t>(yy - Y0) * ww;
                std::copy_n(w.h.begin() + o, ww, b.h.begin() + d);
                std::copy_n(w.m.begin() + o, ww, b.m.begin() + d);
                std::copy_n(w.k.begin() + o, ww, b.k.begin() + d);
            }
        }
    }
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
    CARTO_PROF("pincel: soltar");
    if (!m_stroke) return;
    m_strokeTick.stop();
    std::unique_ptr<Stroke> s = std::move(m_stroke);
    if (!s->box) { update(); return; }
    const bool floraFresh = m_doc->floraVer == m_doc->ver && m_doc->floraScale == worldScale(*m_world);
    const int verFrom = m_doc->ver;
    m_doc->touch();
    if (floraFresh) {
        const auto& fb = *s->box;
        updateFloraBox(*m_world, m_doc->flora, fb[0], fb[1], fb[2], fb[3]);
        m_doc->floraVer = m_doc->ver;
    }
    if (m_terrainDirty && m_terrainDirty->to == verFrom) {
        auto& d = m_terrainDirty->box;
        const auto& fb = *s->box;
        d = { std::min(d[0], fb[0]), std::min(d[1], fb[1]), std::max(d[2], fb[2]), std::max(d[3], fb[3]) };
        m_terrainDirty->to = m_doc->ver;
    } else m_terrainDirty = TerrainDirty{ verFrom, m_doc->ver, *s->box };
    int riverId = 0;
    if (s->river.size() >= 4) {
        River rv; rv.id = ++m_doc->riverSeq; rv.pts = resample(s->river, 6);
        m_doc->rivers.push_back(rv);
        riverId = rv.id;
    }
    const auto& b = *s->box;
    // o "antes": o mundo de agora com os blocos guardados por cima (fora deles nada mudou)
    Snapshot before = snapshotRect(*m_world, m_world->h, m_world->m, m_world->k, b[0], b[1], b[2], b[3]);
    {
        const int ww = b[2] - b[0];
        for (const auto& [key, blk] : s->saved) {
            const int X0 = (key % s->bw) * 64, Y0 = (key / s->bw) * 64, bwid = std::min(m_world->W, X0 + 64) - X0;
            const int bhgt = static_cast<int>(blk.h.size()) / std::max(1, bwid);
            for (int yy = std::max(Y0, b[1]); yy < std::min(Y0 + bhgt, b[3]); yy++)
                for (int xx = std::max(X0, b[0]); xx < std::min(X0 + bwid, b[2]); xx++) {
                    const size_t src = static_cast<size_t>(yy - Y0) * bwid + (xx - X0), dst = static_cast<size_t>(yy - b[1]) * ww + (xx - b[0]);
                    before.h[dst] = blk.h[src]; before.m[dst] = blk.m[src]; before.k[dst] = blk.k[src];
                }
        }
    }
    Snapshot after = snapshotRect(*m_world, m_world->h, m_world->m, m_world->k, b[0], b[1], b[2], b[3]);
    const auto t0 = Clock::now();
    paintBox(b, false);            // passada exata: acerta o mar raso longe da pincelada
    m_timing = tr("acabamento em %1 ms").arg(qRound(msSince(t0)));
    update();
    emit strokeFinished(before, after, riverId);
    emit statusChanged();
}

} // namespace carto
