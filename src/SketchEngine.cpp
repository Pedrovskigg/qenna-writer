#include "SketchEngine.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "mypaint-brush.h"
#include "mypaint-brush-settings.h"
#include "mypaint-surface.h"
#include "mypaint-tiled-surface.h"

// ── A superfície (uma camada) ───────────────────────────────────────────────

struct SketchTiledSurface {
    MyPaintTiledSurface parent;   // tem que ser o primeiro membro
    int tilesW = 0, tilesH = 0;
    size_t tileBytes = 0;
    uint16_t* buf = nullptr;      // blocos em sequência, RGBA 15 bits pré-multiplicado
    uint16_t* nullTile = nullptr; // fora da folha: escreve aqui e joga fora
    bool recording = false;       // copiando os blocos antes do pincel mexer
    QHash<int, QByteArray> recorded;
};

namespace {
constexpr int kT = MYPAINT_TILE_SIZE;
constexpr uint32_t kOne = 1u << 15;

uint16_t* tileAt(SketchTiledSurface* s, int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= s->tilesW || ty >= s->tilesH) return nullptr;
    return s->buf + (size_t(ty) * s->tilesW + tx) * (s->tileBytes / sizeof(uint16_t));
}

void tileRequestStart(MyPaintTiledSurface* ts, MyPaintTileRequest* req)
{
    auto* s = reinterpret_cast<SketchTiledSurface*>(ts);
    uint16_t* t = tileAt(s, req->tx, req->ty);
    if (t && s->recording && !req->readonly) {
        const int key = req->ty * s->tilesW + req->tx;
        if (!s->recorded.contains(key))
            s->recorded.insert(key, QByteArray(reinterpret_cast<const char*>(t), int(s->tileBytes)));
    }
    req->buffer = t ? t : s->nullTile;
}

void tileRequestEnd(MyPaintTiledSurface* ts, MyPaintTileRequest* req)
{
    auto* s = reinterpret_cast<SketchTiledSurface*>(ts);
    if (!tileAt(s, req->tx, req->ty)) std::memset(s->nullTile, 0, s->tileBytes);
}

void destroySurface(MyPaintSurface* surf)
{
    auto* s = reinterpret_cast<SketchTiledSurface*>(surf);
    mypaint_tiled_surface_destroy(&s->parent);
    std::free(s->buf);
    std::free(s->nullTile);
    delete s;
}

inline uchar to8(uint32_t v15) { return uchar((qMin(v15, kOne) * 255u + (kOne >> 1)) >> 15); }
inline uint16_t to15(uchar v8) { return uint16_t((uint32_t(v8) * kOne + 127u) / 255u); }
} // namespace

SketchSurface::SketchSurface(int width, int height)
    : m_w(qMax(1, width)), m_h(qMax(1, height))
{
    m_s = new SketchTiledSurface();
    mypaint_tiled_surface_init(&m_s->parent, tileRequestStart, tileRequestEnd);
    m_s->parent.parent.destroy = destroySurface;
    m_s->tilesW = (m_w + kT - 1) / kT;
    m_s->tilesH = (m_h + kT - 1) / kT;
    m_s->tileBytes = size_t(kT) * kT * 4 * sizeof(uint16_t);
    m_s->buf = static_cast<uint16_t*>(std::calloc(size_t(m_s->tilesW) * m_s->tilesH, m_s->tileBytes));
    m_s->nullTile = static_cast<uint16_t*>(std::calloc(1, m_s->tileBytes));
}

SketchSurface::~SketchSurface()
{
    if (m_s) mypaint_surface_unref(reinterpret_cast<MyPaintSurface*>(m_s));
}

MyPaintSurface* SketchSurface::surface() const { return reinterpret_cast<MyPaintSurface*>(m_s); }
int SketchSurface::tileSize() { return kT; }

void SketchSurface::beginAtomic() { mypaint_surface_begin_atomic(surface()); }

QRect SketchSurface::endAtomic()
{
    MyPaintRectangle roi{ 0, 0, 0, 0 };
    mypaint_surface_end_atomic(surface(), &roi);
    if (roi.width <= 0 || roi.height <= 0) return {};
    return QRect(roi.x, roi.y, roi.width, roi.height).intersected(QRect(0, 0, m_w, m_h));
}

void SketchSurface::renderInto(QImage& img, const QRect& areaIn) const
{
    const QRect area = areaIn.intersected(QRect(0, 0, m_w, m_h)).intersected(img.rect());
    if (area.isEmpty()) return;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        const int ty = y / kT, py = y % kT;
        for (int x = area.left(); x <= area.right(); ++x) {
            const uint16_t* t = tileAt(m_s, x / kT, ty);
            const uint16_t* p = t + (size_t(py) * kT + (x % kT)) * 4;
            line[x] = qRgba(to8(p[0]), to8(p[1]), to8(p[2]), to8(p[3]));
        }
    }
}

QImage SketchSurface::toImage() const
{
    QImage img(m_w, m_h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    renderInto(img, img.rect());
    return img;
}

void SketchSurface::loadImage(const QImage& src)
{
    clear();
    const QImage im = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = qMin(m_w, im.width()), h = qMin(m_h, im.height());
    for (int y = 0; y < h; ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(im.constScanLine(y));
        const int ty = y / kT, py = y % kT;
        for (int x = 0; x < w; ++x) {
            uint16_t* p = tileAt(m_s, x / kT, ty) + (size_t(py) * kT + (x % kT)) * 4;
            const QRgb c = line[x];
            p[0] = to15(uchar(qRed(c))); p[1] = to15(uchar(qGreen(c)));
            p[2] = to15(uchar(qBlue(c))); p[3] = to15(uchar(qAlpha(c)));
        }
    }
}

void SketchSurface::clear()
{
    std::memset(m_s->buf, 0, size_t(m_s->tilesW) * m_s->tilesH * m_s->tileBytes);
}

bool SketchSurface::isEmpty() const
{
    const size_t n = size_t(m_s->tilesW) * m_s->tilesH * m_s->tileBytes / sizeof(uint16_t);
    for (size_t i = 3; i < n; i += 4) if (m_s->buf[i]) return false;
    return true;
}

SketchSurface::TileSnapshot SketchSurface::snapshot(const QRect& areaIn) const
{
    TileSnapshot snap;
    const QRect area = areaIn.intersected(QRect(0, 0, m_w, m_h));
    if (area.isEmpty()) return snap;
    for (int ty = area.top() / kT; ty <= area.bottom() / kT; ++ty)
        for (int tx = area.left() / kT; tx <= area.right() / kT; ++tx)
            if (const uint16_t* t = tileAt(m_s, tx, ty))
                snap.tiles.insert(ty * m_s->tilesW + tx,
                                  QByteArray(reinterpret_cast<const char*>(t), int(m_s->tileBytes)));
    return snap;
}

SketchSurface::TileSnapshot SketchSurface::snapshotAll() const
{
    return snapshot(QRect(0, 0, m_w, m_h));
}

void SketchSurface::startRecording()
{
    m_s->recorded.clear();
    m_s->recording = true;
}

SketchSurface::TileSnapshot SketchSurface::takeRecording()
{
    TileSnapshot snap;
    snap.tiles = std::move(m_s->recorded);
    m_s->recorded.clear();
    m_s->recording = false;
    return snap;
}

void SketchSurface::restore(const TileSnapshot& snap)
{
    for (auto it = snap.tiles.cbegin(); it != snap.tiles.cend(); ++it) {
        uint16_t* t = tileAt(m_s, it.key() % m_s->tilesW, it.key() / m_s->tilesW);
        if (t && size_t(it.value().size()) == m_s->tileBytes) std::memcpy(t, it.value().constData(), m_s->tileBytes);
    }
}

// ── O pincel ────────────────────────────────────────────────────────────────

SketchBrush::SketchBrush()
{
    m_b = mypaint_brush_new();
    mypaint_brush_from_defaults(m_b);
}

SketchBrush::~SketchBrush()
{
    if (m_b) mypaint_brush_unref(m_b);
}

bool SketchBrush::loadMyb(const QByteArray& json, QString* error)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    const QJsonObject settings = doc.object().value(QStringLiteral("settings")).toObject();
    if (doc.isNull() || settings.isEmpty()) {
        if (error) *error = doc.isNull() ? pe.errorString() : QStringLiteral("sem \"settings\"");
        return false;
    }
    mypaint_brush_from_defaults(m_b);
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        const int id = mypaint_brush_setting_from_cname(it.key().toLatin1().constData());
        if (id < 0) continue;   // configuração de outra versão do MyPaint: ignora
        const auto sid = MyPaintBrushSetting(id);
        const QJsonObject so = it.value().toObject();
        mypaint_brush_set_base_value(m_b, sid, float(so.value(QStringLiteral("base_value")).toDouble()));
        for (int i = 0; i < MYPAINT_BRUSH_INPUTS_COUNT; ++i)
            mypaint_brush_set_mapping_n(m_b, sid, MyPaintBrushInput(i), 0);
        const QJsonObject inputs = so.value(QStringLiteral("inputs")).toObject();
        for (auto in = inputs.constBegin(); in != inputs.constEnd(); ++in) {
            const int iid = mypaint_brush_input_from_cname(in.key().toLatin1().constData());
            if (iid < 0) continue;
            const QJsonArray pts = in.value().toArray();
            if (pts.size() < 2) continue;
            mypaint_brush_set_mapping_n(m_b, sid, MyPaintBrushInput(iid), int(pts.size()));
            for (int k = 0; k < pts.size(); ++k) {
                const QJsonArray xy = pts[k].toArray();
                mypaint_brush_set_mapping_point(m_b, sid, MyPaintBrushInput(iid), k,
                                                float(xy.at(0).toDouble()), float(xy.at(1).toDouble()));
            }
        }
    }
    m_baseRadiusLog = mypaint_brush_get_base_value(m_b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC);
    m_baseDiameter  = 2.0 * std::exp(m_baseRadiusLog);
    m_baseOpaque    = mypaint_brush_get_base_value(m_b, MYPAINT_BRUSH_SETTING_OPAQUE);
    m_baseEraser    = mypaint_brush_get_base_value(m_b, MYPAINT_BRUSH_SETTING_ERASER);
    applyOverrides();
    return true;
}

void SketchBrush::setColor(const QColor& c) { m_color = c; applyOverrides(); }
void SketchBrush::setDiameter(qreal px) { m_diameter = px; applyOverrides(); }
void SketchBrush::setOpacity(qreal op) { m_opacity = qBound(0.0, op, 1.0); applyOverrides(); }
void SketchBrush::setEraser(bool on) { m_eraser = on; applyOverrides(); }

void SketchBrush::applyOverrides()
{
    const qreal rlog = m_diameter > 0 ? std::log(qMax(0.5, m_diameter / 2.0)) : m_baseRadiusLog;
    mypaint_brush_set_base_value(m_b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, float(rlog));
    mypaint_brush_set_base_value(m_b, MYPAINT_BRUSH_SETTING_OPAQUE, float(m_baseOpaque * m_opacity));
    mypaint_brush_set_base_value(m_b, MYPAINT_BRUSH_SETTING_ERASER, m_eraser ? 1.0f : float(m_baseEraser));
    float h = 0, s = 0, v = 0;
    const QColor hsv = m_color.toHsv();
    h = float(qMax(0.0, hsv.hsvHueF()));
    s = float(hsv.hsvSaturationF());
    v = float(hsv.valueF());
    mypaint_brush_set_base_value(m_b, MYPAINT_BRUSH_SETTING_COLOR_H, h);
    mypaint_brush_set_base_value(m_b, MYPAINT_BRUSH_SETTING_COLOR_S, s);
    mypaint_brush_set_base_value(m_b, MYPAINT_BRUSH_SETTING_COLOR_V, v);
}

void SketchBrush::newStroke()
{
    mypaint_brush_reset(m_b);
    mypaint_brush_new_stroke(m_b);
}

void SketchBrush::strokeTo(SketchSurface& s, qreal x, qreal y, qreal pressure,
                           qreal xtilt, qreal ytilt, qreal dtime)
{
    mypaint_brush_stroke_to(m_b, s.surface(), float(x), float(y), float(qBound(0.0, pressure, 1.0)),
                            float(xtilt), float(ytilt), qMax(0.0005, dtime));
}
