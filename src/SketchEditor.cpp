#include "SketchEditor.h"

#include "ColorPopover.h"
#include "IconUtils.h"
#include "Theme.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QStandardPaths>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QSlider>
#include <QTabletEvent>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <cmath>

namespace {

const char* const kPalette[] = { "#2e2a24", "#ffffff", "#c0392b", "#d97757", "#e0b43a",
                                 "#4e9a5b", "#2a9d8f", "#3d6fc4", "#8a5cc2", "#d4588f" };

// Tamanho: o slider vai de 0 a 100 e o diâmetro de 1 a 300 px (escala de log,
// pra ter controle fino nos pincéis pequenos).
constexpr qreal kMaxDiameter = 300.0;
int   sliderFromDiameter(qreal d) { return qBound(0, int(std::round(std::log(qMax(1.0, d)) / std::log(kMaxDiameter) * 100.0)), 100); }
qreal diameterFromSlider(int v)   { return std::pow(kMaxDiameter, v / 100.0); }

QIcon editorIcon(const QString& name)
{
    return IconUtils::loadToolbarIcon(QStringLiteral(":/icons/lousa/%1.svg").arg(name),
                                      QColor(Theme::textPrimary()), QColor(Theme::textBright()),
                                      QColor(Theme::accentDefault()));
}

QString uid() { return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8); }

} // namespace

// ═══════════════════════════════ A folha ═════════════════════════════════════

SketchCanvas::SketchCanvas(SketchEditor* editor)
    : QWidget(editor), m_ed(editor)
{
    setAttribute(Qt::WA_TabletTracking, true);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
}

qreal SketchCanvas::fitScale() const
{
    return qMin(width() / qreal(SketchEditor::kSheetW), height() / qreal(SketchEditor::kSheetH));
}

qreal SketchCanvas::scale() const
{
    return fitScale() * m_zoom;
}

QRectF SketchCanvas::sheetRect() const
{
    const qreal s = scale();
    const QSizeF sz(SketchEditor::kSheetW * s, SketchEditor::kSheetH * s);
    return QRectF(QPointF((width() - sz.width()) / 2.0, (height() - sz.height()) / 2.0) + m_pan, sz);
}

QRectF SketchCanvas::fitRect() const
{
    const qreal s = fitScale();
    const QSizeF sz(SketchEditor::kSheetW * s, SketchEditor::kSheetH * s);
    return QRectF(QPointF((width() - sz.width()) / 2.0, (height() - sz.height()) / 2.0), sz);
}

void SketchCanvas::clampPan()
{
    // A folha pode sair da tela, mas sempre sobra um pedaço dela à vista.
    const QRectF r = sheetRect();
    const qreal keep = 120;
    QPointF fix;
    if (r.right() < keep) fix.rx() = keep - r.right();
    if (r.left() > width() - keep) fix.rx() = (width() - keep) - r.left();
    if (r.bottom() < keep) fix.ry() = keep - r.bottom();
    if (r.top() > height() - keep) fix.ry() = (height() - keep) - r.top();
    m_pan += fix;
}

void SketchCanvas::panBy(const QPointF& d)
{
    m_pan += d;
    clampPan();
    update();
}

void SketchCanvas::zoomAt(qreal zoom, const QPointF& anchor)
{
    zoom = qBound(kMinZoom, zoom, kMaxZoom);
    if (qFuzzyCompare(zoom, m_zoom)) return;
    const QPointF sheetPt = toSheet(anchor);   // o ponto embaixo do cursor fica parado
    m_zoom = zoom;
    const QRectF r = sheetRect();
    m_pan += anchor - (r.topLeft() + sheetPt * scale());
    clampPan();
    update();
    emit zoomChanged(m_zoom);
}

void SketchCanvas::zoomBy(qreal factor)
{
    zoomAt(m_zoom * factor, QPointF(width() / 2.0, height() / 2.0));
}

void SketchCanvas::fitToScreen()
{
    m_zoom = 1.0;
    m_pan = QPointF();
    update();
    emit zoomChanged(m_zoom);
}

void SketchCanvas::setPanKey(bool held)
{
    m_panKey = held;
    if (!m_drawing) setCursor(held ? Qt::OpenHandCursor : Qt::CrossCursor);
}

void SketchCanvas::wheelEvent(QWheelEvent* e)
{
    const QPoint px = e->pixelDelta();
    const QPoint ang = e->angleDelta();
    if (e->modifiers() & Qt::ControlModifier) {
        const qreal steps = ang.y() / 120.0;
        zoomAt(m_zoom * std::pow(1.15, steps), e->position());
    } else {
        QPointF d = !px.isNull() ? QPointF(px) : QPointF(ang) / 120.0 * 60.0;
        if (e->modifiers() & Qt::ShiftModifier) d = QPointF(d.y(), d.x());
        panBy(d);
    }
    e->accept();
}

void SketchCanvas::resizeEvent(QResizeEvent*)
{
    clampPan();
}

QPointF SketchCanvas::toSheet(const QPointF& w) const
{
    const QRectF r = sheetRect();
    return (w - r.topLeft()) / scale();
}

void SketchCanvas::paintEvent(QPaintEvent* e)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF r = sheetRect();
    // sombra da folha
    for (int i = 6; i >= 1; --i) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(20, 14, 8, 10));
        p.drawRoundedRect(r.adjusted(-i, -i + 4, i, i + 6), 4 + i, 4 + i);
    }
    p.setBrush(SketchEditor::paperColor());
    p.drawRoundedRect(r, 3, 3);
    p.save();
    p.setClipRect(r.intersected(QRectF(e->rect())));
    p.setRenderHint(QPainter::SmoothPixmapTransform, scale() < 2.0);
    for (const auto& L : m_ed->layers()) {
        if (!L->visible || L->opacity <= 0.001) continue;
        p.setOpacity(L->opacity);
        p.drawImage(r, L->cache);
    }
    p.restore();
    // contorno do pincel onde a caneta está
    if (m_hoverOn && r.contains(m_hover)) {
        const qreal rad = qMax(1.5, m_ed->brushDiameter() * scale() / 2.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 170), 2.4));
        p.drawEllipse(m_hover, rad, rad);
        p.setPen(QPen(QColor(40, 34, 28, 190), 1.0));
        p.drawEllipse(m_hover, rad, rad);
    }
}

void SketchCanvas::begin(const QPointF& w, qreal pressure, bool eraser, qreal xt, qreal yt)
{
    SketchLayer* L = m_ed->activeLayer();
    if (!L) return;
    m_drawing = true;
    m_eraserStroke = eraser;
    m_stabStrength = LousaInk::stabilizerSetting();
    const QPointF sp = toSheet(w);
    m_stab.reset(sp);
    m_raw = sp;
    m_lastPainted = sp;
    m_lastPressure = pressure;
    m_clock.start();
    L->surf->startRecording();
    SketchBrush& b = m_eraserStroke ? m_ed->eraserBrush() : m_ed->brush();
    b.newStroke();
    // Põe o pincel exatamente onde a caneta encostou, com pressão zero e um
    // "tempo parado" longo. Vários pincéis (Lápis, Carvão, Marcador) suavizam o
    // caminho a partir de onde o pincel estava ("slow tracking"); o app do
    // MyPaint vai contando onde a caneta passa por cima do papel, nós não. Sem
    // isso, o traço novo nascia do fim do anterior (ou do canto da folha) e
    // cortava o papel em linha reta até o ponto do clique.
    L->surf->beginAtomic();
    b.strokeTo(*L->surf, sp.x(), sp.y(), 0.0, xt, yt, 10.0);
    m_ed->layerChanged(L, L->surf->endAtomic());
    paintAt(sp, pressure, xt, yt);
}

void SketchCanvas::paintAt(const QPointF& sp, qreal pressure, qreal xt, qreal yt, qreal dtOverride)
{
    SketchLayer* L = m_ed->activeLayer();
    if (!L) return;
    SketchBrush& b = m_eraserStroke ? m_ed->eraserBrush() : m_ed->brush();
    // O MyPaint lê velocidade pelo tempo entre os pontos; tempo quase zero
    // vira velocidade absurda, e pincel que afina com a velocidade some.
    const qreal dt = dtOverride > 0 ? dtOverride : qMax(0.002, m_clock.nsecsElapsed() / 1e9);
    m_clock.restart();
    m_lastPainted = sp;
    L->surf->beginAtomic();
    b.strokeTo(*L->surf, sp.x(), sp.y(), pressure, xt, yt, dt);
    const QRect dirty = L->surf->endAtomic();
    m_ed->layerChanged(L, dirty);
    if (!dirty.isEmpty()) {
        const QRectF r = sheetRect();
        const qreal s = scale();
        update(QRectF(r.topLeft() + QPointF(dirty.topLeft()) * s, QSizeF(dirty.size()) * s).toAlignedRect().adjusted(-2, -2, 2, 2));
    }
}

void SketchCanvas::move(const QPointF& w, qreal pressure, qreal xt, qreal yt)
{
    const QRect old = QRectF(m_hover - QPointF(200, 200), QSizeF(400, 400)).toAlignedRect();
    m_hover = w;
    m_hoverOn = true;
    if (!m_drawing) { update(old); update(QRectF(w - QPointF(200, 200), QSizeF(400, 400)).toAlignedRect()); return; }
    const QPointF sp = toSheet(w);
    m_raw = sp;
    m_lastPressure = pressure;
    const QPointF pt = m_stab.feed(sp, m_stabStrength, scale());
    paintAt(pt, pressure, xt, yt);
    update(old);
}

void SketchCanvas::end()
{
    if (!m_drawing) return;
    m_drawing = false;
    SketchLayer* L = m_ed->activeLayer();
    if (!L) return;
    // O traço alcança onde a caneta parou, e a caneta "sai" do papel. Sem
    // leitura da caneta nesse pedaço: o tempo sai da distância, numa
    // velocidade de mão (uns 600 px da folha por segundo).
    const qreal gap = QLineF(m_lastPainted, m_raw).length();
    if (m_stabStrength > 0 && gap > 0.5) paintAt(m_raw, m_lastPressure * 0.6, 0, 0, qMax(0.004, gap / 600.0));
    paintAt(m_raw, 0.0, 0, 0, 0.01);
    m_ed->pushStrokeUndo(L, L->surf->takeRecording());
    emit strokeFinished();
}

void SketchCanvas::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::MiddleButton || (e->button() == Qt::LeftButton && m_panKey)) {
        m_panning = true;
        m_panLast = e->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (e->button() != Qt::LeftButton || m_tabletDown) return;
    begin(e->position(), 0.6, false, 0, 0);
}

void SketchCanvas::mouseMoveEvent(QMouseEvent* e)
{
    if (m_panning) {
        panBy(e->position() - m_panLast);
        m_panLast = e->position();
        return;
    }
    if (m_tabletDown) return;
    move(e->position(), 0.6, 0, 0);
}

void SketchCanvas::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_panning && (e->button() == Qt::MiddleButton || e->button() == Qt::LeftButton)) {
        m_panning = false;
        setCursor(m_panKey ? Qt::OpenHandCursor : Qt::CrossCursor);
        return;
    }
    if (e->button() != Qt::LeftButton || m_tabletDown) return;
    end();
}

void SketchCanvas::tabletEvent(QTabletEvent* e)
{
    const qreal xt = qBound(-1.0, e->xTilt() / 60.0, 1.0);
    const qreal yt = qBound(-1.0, e->yTilt() / 60.0, 1.0);
    const qreal pr = qBound(0.0, e->pressure(), 1.0);
    if (m_panKey || m_panning) {
        if (e->type() == QEvent::TabletPress) { m_panning = true; m_panLast = e->position(); setCursor(Qt::ClosedHandCursor); }
        else if (e->type() == QEvent::TabletMove && m_panning) { panBy(e->position() - m_panLast); m_panLast = e->position(); }
        else if (e->type() == QEvent::TabletRelease) { m_panning = false; setCursor(m_panKey ? Qt::OpenHandCursor : Qt::CrossCursor); }
        e->accept();
        return;
    }
    switch (e->type()) {
    case QEvent::TabletPress:
        if (e->button() == Qt::LeftButton || (e->buttons() & Qt::LeftButton)) {
            m_tabletDown = true;
            begin(e->position(), pr, e->pointerType() == QPointingDevice::PointerType::Eraser, xt, yt);
        }
        break;
    case QEvent::TabletMove:
        move(e->position(), pr, xt, yt);
        break;
    case QEvent::TabletRelease:
        if (m_tabletDown) { m_tabletDown = false; end(); }
        break;
    default:
        break;
    }
    e->accept();
}

void SketchCanvas::leaveEvent(QEvent*)
{
    m_hoverOn = false;
    update();
}

// ═══════════════════════════════ O editor ════════════════════════════════════

SketchEditor::SketchEditor(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setFocusPolicy(Qt::StrongFocus);
    m_brush  = std::make_unique<SketchBrush>();
    m_eraser = std::make_unique<SketchBrush>();
    QSettings st;
    m_color = QColor(st.value(QStringLiteral("sketch/color"), QStringLiteral("#2e2a24")).toString());
    if (!m_color.isValid()) m_color = QColor(QStringLiteral("#2e2a24"));
    m_opacity = qBound(0.1, st.value(QStringLiteral("sketch/opacity"), 1.0).toDouble(), 1.0);
    for (const SketchBrushes::Info& i : SketchBrushes::builtins())
        if (i.eraser) { m_eraser->loadMyb(SketchBrushes::readMyb(i)); m_eraser->setEraser(true); }
    buildUi();
    selectBrush(st.value(QStringLiteral("sketch/brush"), QStringLiteral("lapis")).toString());
}

SketchEditor::~SketchEditor() = default;

QString SketchEditor::sketchDir(const QString& projectRoot, const QString& cardId)
{
    return QDir::cleanPath(projectRoot + QStringLiteral("/lousa-esbocos/") + cardId);
}

SketchLayer* SketchEditor::activeLayer()
{
    if (m_active < 0 || m_active >= int(m_layers.size())) return nullptr;
    return m_layers[size_t(m_active)].get();
}

std::shared_ptr<SketchLayer> SketchEditor::newLayer(const QString& name)
{
    auto L = std::make_shared<SketchLayer>();
    L->id = uid();
    L->name = name.isEmpty() ? tr("Camada %1").arg(++m_layerSeq) : name;
    L->surf = std::make_unique<SketchSurface>(kSheetW, kSheetH);
    L->cache = QImage(kSheetW, kSheetH, QImage::Format_ARGB32_Premultiplied);
    L->cache.fill(Qt::transparent);
    return L;
}

void SketchEditor::layerChanged(SketchLayer* L, const QRect& dirty)
{
    if (!L || dirty.isEmpty()) return;
    L->surf->renderInto(L->cache, dirty);
}

void SketchEditor::pushStrokeUndo(SketchLayer* L, SketchSurface::TileSnapshot before)
{
    if (before.tiles.isEmpty()) return;
    UndoStep s;
    s.kind = UndoStep::Stroke;
    for (const auto& sp : m_layers) if (sp.get() == L) s.layer = sp;
    s.before = std::move(before);
    m_undo.push_back(std::move(s));
    if (m_undo.size() > 60) m_undo.erase(m_undo.begin());
    m_redo.clear();
    refreshLayers();   // miniatura
}

void SketchEditor::applyStep(UndoStep& s, bool undoing)
{
    switch (s.kind) {
    case UndoStep::Stroke: {
        if (!s.layer) return;
        // guarda o estado atual dos mesmos blocos pra ida e volta
        SketchSurface::TileSnapshot cur;
        for (auto it = s.before.tiles.cbegin(); it != s.before.tiles.cend(); ++it) {
            const int tw = (kSheetW + SketchSurface::tileSize() - 1) / SketchSurface::tileSize();
            const int tx = it.key() % tw, ty = it.key() / tw;
            const SketchSurface::TileSnapshot one = s.layer->surf->snapshot(
                QRect(tx * SketchSurface::tileSize(), ty * SketchSurface::tileSize(), 1, 1));
            for (auto o = one.tiles.cbegin(); o != one.tiles.cend(); ++o) cur.tiles.insert(o.key(), o.value());
        }
        s.layer->surf->restore(undoing ? s.before : s.after);
        if (undoing) s.after = std::move(cur); else s.before = std::move(cur);
        s.layer->cache.fill(Qt::transparent);
        s.layer->surf->renderInto(s.layer->cache, s.layer->cache.rect());
        break;
    }
    case UndoStep::AddLayer:
        if (undoing) {
            m_layers.erase(m_layers.begin() + s.index);
            m_active = qBound(0, s.index - 1, int(m_layers.size()) - 1);
        } else {
            m_layers.insert(m_layers.begin() + s.index, s.layer);
            m_active = s.index;
        }
        break;
    case UndoStep::RemoveLayer:
        if (undoing) {
            m_layers.insert(m_layers.begin() + s.index, s.layer);
            m_active = s.index;
        } else {
            m_layers.erase(m_layers.begin() + s.index);
            m_active = qBound(0, s.index - 1, int(m_layers.size()) - 1);
        }
        break;
    case UndoStep::MoveLayer: {
        const int a = undoing ? s.to : s.from, b = undoing ? s.from : s.to;
        std::swap(m_layers[size_t(a)], m_layers[size_t(b)]);
        m_active = b;
        break;
    }
    }
    refreshLayers();
    m_canvas->update();
}

void SketchEditor::undo()
{
    if (m_undo.empty()) return;
    UndoStep s = std::move(m_undo.back());
    m_undo.pop_back();
    applyStep(s, true);
    m_redo.push_back(std::move(s));
}

void SketchEditor::redo()
{
    if (m_redo.empty()) return;
    UndoStep s = std::move(m_redo.back());
    m_redo.pop_back();
    applyStep(s, false);
    m_undo.push_back(std::move(s));
}

// ── Abrir e salvar ───────────────────────────────────────────────────────────

void SketchEditor::open(const QString& projectRoot, const QString& cardId, const QString& title)
{
    m_root = projectRoot;
    m_cardId = cardId;
    m_title = title;
    m_titleLbl->setText(title.isEmpty() ? tr("Esboço") : tr("Esboço · %1").arg(title));
    load();
    refreshLayers();
    m_canvas->update();
    setFocus();
}

void SketchEditor::load()
{
    m_layers.clear();
    m_undo.clear();
    m_redo.clear();
    m_layerSeq = 0;
    const QString dir = sketchDir(m_root, m_cardId);
    QFile f(dir + QStringLiteral("/sketch.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        for (const QJsonValue& v : root.value(QStringLiteral("layers")).toArray()) {
            const QJsonObject o = v.toObject();
            auto L = newLayer(o.value(QStringLiteral("name")).toString());
            L->id = o.value(QStringLiteral("id")).toString(L->id);
            L->visible = o.value(QStringLiteral("visible")).toBool(true);
            L->opacity = qBound(0.0, o.value(QStringLiteral("opacity")).toDouble(1.0), 1.0);
            QImage img(dir + QStringLiteral("/") + L->id + QStringLiteral(".png"));
            if (!img.isNull()) {
                L->surf->loadImage(img);
                L->surf->renderInto(L->cache, L->cache.rect());
            }
            m_layers.push_back(L);
        }
        m_layerSeq = int(m_layers.size());
        m_active = qBound(0, root.value(QStringLiteral("active")).toInt(0), int(m_layers.size()) - 1);
    }
    if (m_layers.empty()) {
        m_layers.push_back(newLayer(tr("Rascunho")));
        m_layers.push_back(newLayer(tr("Arte")));
        m_layerSeq = 2;
        m_active = 0;
    }
}

bool SketchEditor::save()
{
    if (m_root.isEmpty() || m_cardId.isEmpty()) return false;
    const QString dir = sketchDir(m_root, m_cardId);
    QDir().mkpath(dir);
    QJsonArray arr;
    QSet<QString> keep;
    for (const auto& L : m_layers) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), L->id);
        o.insert(QStringLiteral("name"), L->name);
        o.insert(QStringLiteral("visible"), L->visible);
        o.insert(QStringLiteral("opacity"), L->opacity);
        arr.append(o);
        const QString file = L->id + QStringLiteral(".png");
        keep.insert(file);
        L->cache.save(dir + QStringLiteral("/") + file, "PNG");
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("width"), kSheetW);
    root.insert(QStringLiteral("height"), kSheetH);
    root.insert(QStringLiteral("active"), m_active);
    root.insert(QStringLiteral("layers"), arr);
    QFile f(dir + QStringLiteral("/sketch.json"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    // Camada apagada: o PNG dela vai pra uma subpasta, não some (lixeira obrigatória).
    for (const QFileInfo& fi : QDir(dir).entryInfoList({ QStringLiteral("*.png") }, QDir::Files)) {
        if (keep.contains(fi.fileName())) continue;
        QDir().mkpath(dir + QStringLiteral("/apagadas"));
        QFile::rename(fi.absoluteFilePath(), dir + QStringLiteral("/apagadas/") + fi.fileName());
    }
    return true;
}

QImage SketchEditor::composite(bool withPaper) const
{
    QImage img(kSheetW, kSheetH, QImage::Format_ARGB32_Premultiplied);
    img.fill(withPaper ? paperColor() : QColor(Qt::transparent));
    QPainter p(&img);
    for (const auto& L : m_layers) {
        if (!L->visible) continue;
        p.setOpacity(L->opacity);
        p.drawImage(0, 0, L->cache);
    }
    p.end();
    return img;
}

QImage SketchEditor::renderFromDisk(const QString& projectRoot, const QString& cardId, bool withPaper)
{
    const QString dir = sketchDir(projectRoot, cardId);
    QFile f(dir + QStringLiteral("/sketch.json"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    QImage img(root.value(QStringLiteral("width")).toInt(kSheetW), root.value(QStringLiteral("height")).toInt(kSheetH),
               QImage::Format_ARGB32_Premultiplied);
    img.fill(withPaper ? paperColor() : QColor(Qt::transparent));
    QPainter p(&img);
    for (const QJsonValue& v : root.value(QStringLiteral("layers")).toArray()) {
        const QJsonObject o = v.toObject();
        if (!o.value(QStringLiteral("visible")).toBool(true)) continue;
        const QImage layer(dir + QStringLiteral("/") + o.value(QStringLiteral("id")).toString() + QStringLiteral(".png"));
        if (layer.isNull()) continue;
        p.setOpacity(qBound(0.0, o.value(QStringLiteral("opacity")).toDouble(1.0), 1.0));
        p.drawImage(0, 0, layer);
    }
    p.end();
    return img;
}

void SketchEditor::exportDialog(QWidget* parent, const QImage& withPaper, const QImage& transparent,
                                const QString& suggestedName, const QPoint& globalPos)
{
    if (withPaper.isNull()) return;
    QMenu menu(parent);
    QAction* paper = menu.addAction(tr("PNG com o papel"));
    QAction* clear = menu.addAction(tr("PNG com fundo transparente"));
    QAction* chosen = menu.exec(globalPos);
    if (!chosen) return;
    QString name = suggestedName.trimmed();
    if (name.isEmpty()) name = tr("Esboço");
    for (const QChar ch : QStringLiteral("\\/:*?\"<>|")) name.replace(ch, QLatin1Char('-'));
    const QString start = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + QStringLiteral("/") + name + QStringLiteral(".png");
    const QString path = QFileDialog::getSaveFileName(parent, tr("Exportar desenho"), start, tr("Imagem PNG (*.png)"));
    if (path.isEmpty()) return;
    (chosen == clear ? transparent : withPaper).save(path.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive) ? path : path + QStringLiteral(".png"), "PNG");
    Q_UNUSED(paper);
}

void SketchEditor::done()
{
    save();
    emit finished(m_cardId, composite());
}

// ── Camadas ──────────────────────────────────────────────────────────────────

void SketchEditor::addLayer()
{
    auto L = newLayer(QString());
    const int at = m_active + 1;
    m_layers.insert(m_layers.begin() + at, L);
    m_active = at;
    UndoStep s; s.kind = UndoStep::AddLayer; s.layer = L; s.index = at;
    m_undo.push_back(std::move(s)); m_redo.clear();
    refreshLayers();
}

void SketchEditor::removeLayer()
{
    if (m_layers.size() <= 1) return;
    UndoStep s; s.kind = UndoStep::RemoveLayer; s.layer = m_layers[size_t(m_active)]; s.index = m_active;
    m_layers.erase(m_layers.begin() + m_active);
    m_active = qBound(0, m_active - 1, int(m_layers.size()) - 1);
    m_undo.push_back(std::move(s)); m_redo.clear();
    refreshLayers();
    m_canvas->update();
}

void SketchEditor::moveLayer(int delta)
{
    const int to = m_active + delta;
    if (to < 0 || to >= int(m_layers.size())) return;
    std::swap(m_layers[size_t(m_active)], m_layers[size_t(to)]);
    UndoStep s; s.kind = UndoStep::MoveLayer; s.from = m_active; s.to = to;
    m_undo.push_back(std::move(s)); m_redo.clear();
    m_active = to;
    refreshLayers();
    m_canvas->update();
}

void SketchEditor::clearLayer()
{
    SketchLayer* L = activeLayer();
    if (!L || L->surf->isEmpty()) return;
    UndoStep s; s.kind = UndoStep::Stroke;
    s.layer = m_layers[size_t(m_active)];
    s.before = L->surf->snapshotAll();
    L->surf->clear();
    L->cache.fill(Qt::transparent);
    m_undo.push_back(std::move(s)); m_redo.clear();
    refreshLayers();
    m_canvas->update();
}

void SketchEditor::refreshLayers()
{
    if (!m_layerRows) return;
    while (QLayoutItem* it = m_layerRows->takeAt(0)) {
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        delete it;
    }
    const qreal dpr = devicePixelRatioF();
    for (int i = int(m_layers.size()) - 1; i >= 0; --i) {
        const auto& L = m_layers[size_t(i)];
        auto* row = new QWidget(m_layersBox);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);
        auto* eye = new QToolButton(row);
        eye->setObjectName(QStringLiteral("sketchEye"));
        eye->setCursor(Qt::PointingHandCursor);
        eye->setFixedSize(26, 26);
        eye->setText(L->visible ? QString() : QString());
        eye->setIcon(editorIcon(L->visible ? QStringLiteral("eye") : QStringLiteral("eye-off")));
        eye->setIconSize(QSize(16, 16));
        eye->setToolTip(L->visible ? tr("Esconder camada") : tr("Mostrar camada"));
        connect(eye, &QToolButton::clicked, this, [this, i]() {
            m_layers[size_t(i)]->visible = !m_layers[size_t(i)]->visible;
            refreshLayers();
            m_canvas->update();
        });
        auto* btn = new QToolButton(row);
        btn->setObjectName(QStringLiteral("sketchLayer"));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setCheckable(true);
        btn->setChecked(i == m_active);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setFixedHeight(50);
        QPixmap thumb(QSize(36, 46) * dpr);
        thumb.setDevicePixelRatio(dpr);
        thumb.fill(paperColor());
        {
            QPainter tp(&thumb);
            tp.setRenderHint(QPainter::SmoothPixmapTransform);
            tp.drawImage(QRectF(0, 0, 36, 46), L->cache);
            tp.setPen(QColor(0, 0, 0, 40));
            tp.drawRect(QRectF(0.5, 0.5, 35, 45));
        }
        btn->setIcon(QIcon(thumb));
        btn->setIconSize(QSize(36, 46));
        btn->setText(L->name);
        btn->setProperty("hiddenLayer", !L->visible);
        connect(btn, &QToolButton::clicked, this, [this, i]() {
            m_active = i;
            refreshLayers();
        });
        h->addWidget(eye);
        h->addWidget(btn, 1);
        m_layerRows->addWidget(row);
        row->show();   // já, pro painel medir o tamanho com ela
    }
    if (SketchLayer* L = activeLayer()) {
        QSignalBlocker bl(m_layerOp);
        m_layerOp->setValue(int(std::round(L->opacity * 100)));
        m_layerOpVal->setText(QStringLiteral("%1%").arg(m_layerOp->value()));
    }
    m_layUp->setEnabled(m_active < int(m_layers.size()) - 1);
    m_layDown->setEnabled(m_active > 0);
    m_layDel->setEnabled(m_layers.size() > 1);
    m_layersBox->adjustSize();
    layoutUi();
}

// ── Pincéis ──────────────────────────────────────────────────────────────────

void SketchEditor::selectBrush(const QString& id)
{
    m_brushInfo = SketchBrushes::find(id);
    m_brush->loadMyb(SketchBrushes::readMyb(m_brushInfo));
    // Sempre: sem isso, o pincel escolhido depois da Borracha continuava apagando.
    m_brush->setEraser(m_brushInfo.eraser);
    QSettings st;
    st.setValue(QStringLiteral("sketch/brush"), m_brushInfo.id);
    const qreal def = m_brushInfo.diameter > 0 ? m_brushInfo.diameter : m_brush->baseDiameter();
    m_diameter = qBound(1.0, st.value(QStringLiteral("sketch/size/") + m_brushInfo.id, def).toDouble(), kMaxDiameter);
    if (m_sizeSlider) {
        QSignalBlocker bl(m_sizeSlider);
        m_sizeSlider->setValue(sliderFromDiameter(m_diameter));
        m_sizeVal->setText(QString::number(qRound(m_diameter)));
    }
    applyBrush();
    refreshBrushes();
}

void SketchEditor::applyBrush()
{
    m_brush->setColor(m_color);
    m_brush->setDiameter(m_diameter);
    m_brush->setOpacity(m_opacity);
    m_eraser->setDiameter(qMax(8.0, m_diameter));
}

void SketchEditor::refreshBrushes()
{
    if (!m_brushRows) return;
    auto fill = [this](QVBoxLayout* lay, const QList<SketchBrushes::Info>& list) {
        while (QLayoutItem* it = lay->takeAt(0)) {
            if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
            delete it;
        }
        const qreal dpr = devicePixelRatioF();
        const QColor ink(QStringLiteral("#2e2a24"));
        for (const SketchBrushes::Info& i : list) {
            auto* b = new QToolButton(m_side);
            b->setObjectName(QStringLiteral("sketchBrush"));
            b->setCursor(Qt::PointingHandCursor);
            b->setCheckable(true);
            b->setChecked(i.id == m_brushInfo.id);
            b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            b->setFixedHeight(36);
            const QImage pv = SketchBrushes::preview(i, QSize(74, 26), i.id == QStringLiteral("aquarela") ? QColor(QStringLiteral("#3d6fc4")) : ink, dpr);
            QPixmap pm(QSize(74, 26) * dpr);
            pm.setDevicePixelRatio(dpr);
            pm.fill(Qt::transparent);
            { QPainter pp(&pm); pp.drawImage(QRectF(0, 0, 74, 26), pv); }
            b->setIcon(QIcon(pm));
            b->setIconSize(QSize(74, 26));
            b->setText(i.sub.isEmpty() ? i.name : i.name + QStringLiteral("\n") + i.sub);
            const QString id = i.id;
            connect(b, &QToolButton::clicked, this, [this, id]() { selectBrush(id); });
            lay->addWidget(b);
            b->show();
        }
    };
    fill(m_brushRows, SketchBrushes::builtins());
    fill(m_userRows, SketchBrushes::imported());
}

void SketchEditor::importBrushes()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Importar pincéis"), QString(),
                                                            tr("Pincéis do MyPaint (*.myb *.zip)"));
    if (files.isEmpty()) return;
    QString err;
    const int n = SketchBrushes::importFiles(files, &err);
    QString msg = n > 0 ? tr("%n pincel(is) importado(s).", nullptr, n) : QString();
    if (!err.isEmpty()) msg += (msg.isEmpty() ? QString() : QStringLiteral(" ")) + err;
    m_importNote->setText(msg);
    m_importNote->setVisible(!msg.isEmpty());
    refreshBrushes();
}

void SketchEditor::refreshSwatches()
{
    const QString ring = Theme::accentDefault();
    bool matched = false;
    for (auto& [b, c] : m_swatches) {
        const bool on = c.name() == m_color.name();
        matched |= on;
        b->setStyleSheet(QStringLiteral("QToolButton { background: %1; border: %2 solid %3; border-radius: 9px; }")
                         .arg(c.name(), on ? QStringLiteral("2px") : QStringLiteral("1.5px"),
                              on ? ring : QStringLiteral("rgba(0,0,0,0.22)")));
    }
    const QString fill = matched ? QStringLiteral("qconicalgradient(cx:0.5, cy:0.5, angle:0, stop:0 #e05555, stop:0.2 #e0b43a,"
                                                  " stop:0.4 #4e9a5b, stop:0.6 #3d6fc4, stop:0.8 #8a5cc2, stop:1 #e05555)")
                                 : m_color.name();
    m_pick->setStyleSheet(QStringLiteral("QToolButton { background: %1; border: 2px solid %2; border-radius: 9px; }")
                          .arg(fill, matched ? QStringLiteral("rgba(0,0,0,0.22)") : ring));
}

// ── Interface ────────────────────────────────────────────────────────────────

void SketchEditor::buildUi()
{
    m_canvas = new SketchCanvas(this);

    // Título da folha, em cima dela
    m_caption = new QWidget(this);
    auto* ch = new QHBoxLayout(m_caption);
    ch->setContentsMargins(0, 0, 0, 0);
    ch->setSpacing(10);
    m_titleLbl = new QLabel(m_caption);
    m_titleLbl->setObjectName(QStringLiteral("sketchTitle"));
    auto* photo = new QToolButton(m_caption);
    photo->setObjectName(QStringLiteral("sketchCapBtn"));
    photo->setCursor(Qt::PointingHandCursor);
    photo->setText(tr("Usar como foto do personagem"));
    connect(photo, &QToolButton::clicked, this, [this]() { emit photoRequested(composite()); });
    auto* exportBtn = new QToolButton(m_caption);
    exportBtn->setObjectName(QStringLiteral("sketchCapBtn"));
    exportBtn->setCursor(Qt::PointingHandCursor);
    exportBtn->setText(tr("Exportar…"));
    exportBtn->setToolTip(tr("Salvar o desenho como PNG, com o papel ou com fundo transparente"));
    connect(exportBtn, &QToolButton::clicked, this, [this, exportBtn]() {
        exportDialog(this, composite(true), composite(false), m_title,
                     exportBtn->mapToGlobal(QPoint(0, exportBtn->height())));
    });
    ch->addWidget(m_titleLbl);
    ch->addWidget(photo);
    ch->addWidget(exportBtn);
    ch->addSpacing(14);
    auto zoomBtn = [this](const QString& text, const QString& tip) {
        auto* b = new QToolButton(m_caption);
        b->setObjectName(QStringLiteral("sketchCapBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setText(text);
        b->setToolTip(tip);
        return b;
    };
    auto* zOut = zoomBtn(QStringLiteral("−"), tr("Afastar (Ctrl+−, ou Ctrl+rodinha)"));
    m_zoomLbl = new QLabel(m_caption);
    m_zoomLbl->setObjectName(QStringLiteral("sketchTitle"));
    m_zoomLbl->setMinimumWidth(42);
    m_zoomLbl->setAlignment(Qt::AlignCenter);
    auto* zIn = zoomBtn(QStringLiteral("+"), tr("Aproximar (Ctrl++, ou Ctrl+rodinha)"));
    auto* zFit = zoomBtn(tr("Ajustar"), tr("A folha inteira na tela (Ctrl+0)"));
    ch->addWidget(zOut); ch->addWidget(m_zoomLbl); ch->addWidget(zIn); ch->addWidget(zFit);
    connect(zOut, &QToolButton::clicked, this, [this]() { m_canvas->zoomBy(1.0 / 1.25); });
    connect(zIn, &QToolButton::clicked, this, [this]() { m_canvas->zoomBy(1.25); });
    connect(zFit, &QToolButton::clicked, this, [this]() { m_canvas->fitToScreen(); });
    connect(m_canvas, &SketchCanvas::zoomChanged, this, [this]() { refreshZoomLabel(); });
    refreshZoomLabel();
    ch->addStretch(1);

    // ── Camadas (à esquerda) ──
    m_layersBox = new QFrame(this);
    m_layersBox->setObjectName(QStringLiteral("sketchPanel"));
    auto* lv = new QVBoxLayout(m_layersBox);
    lv->setContentsMargins(10, 10, 10, 10);
    lv->setSpacing(8);
    auto* lh = new QHBoxLayout();
    lh->setSpacing(2);
    auto* lt = new QLabel(tr("CAMADAS"), m_layersBox);
    lt->setObjectName(QStringLiteral("sketchLbl"));
    lh->addWidget(lt);
    lh->addStretch(1);
    auto iconBtn = [this](const QString& icon, const QString& tip) {
        auto* b = new QToolButton(m_layersBox);
        b->setObjectName(QStringLiteral("sketchIconBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setIcon(editorIcon(icon));
        b->setIconSize(QSize(16, 16));
        b->setFixedSize(26, 26);
        b->setToolTip(tip);
        return b;
    };
    m_layUp   = iconBtn(QStringLiteral("layer-up"), tr("Subir camada"));
    m_layDown = iconBtn(QStringLiteral("layer-down"), tr("Descer camada"));
    m_layAdd  = iconBtn(QStringLiteral("plus"), tr("Nova camada"));
    m_layDel  = iconBtn(QStringLiteral("trash"), tr("Apagar camada (Ctrl+Z desfaz)"));
    connect(m_layUp, &QToolButton::clicked, this, [this]() { moveLayer(+1); });
    connect(m_layDown, &QToolButton::clicked, this, [this]() { moveLayer(-1); });
    connect(m_layAdd, &QToolButton::clicked, this, [this]() { addLayer(); });
    connect(m_layDel, &QToolButton::clicked, this, [this]() { removeLayer(); });
    lh->addWidget(m_layUp); lh->addWidget(m_layDown); lh->addWidget(m_layAdd); lh->addWidget(m_layDel);
    lv->addLayout(lh);
    m_layerRows = new QVBoxLayout();
    m_layerRows->setSpacing(2);
    lv->addLayout(m_layerRows);
    auto* oh = new QHBoxLayout();
    auto* ol = new QLabel(tr("Opacidade"), m_layersBox);
    ol->setObjectName(QStringLiteral("sketchSmall"));
    m_layerOp = new QSlider(Qt::Horizontal, m_layersBox);
    m_layerOp->setObjectName(QStringLiteral("sketchSlider"));
    m_layerOp->setRange(0, 100);
    m_layerOpVal = new QLabel(m_layersBox);
    m_layerOpVal->setObjectName(QStringLiteral("sketchSmall"));
    m_layerOpVal->setFixedWidth(34);
    m_layerOpVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    connect(m_layerOp, &QSlider::valueChanged, this, [this](int v) {
        if (SketchLayer* L = activeLayer()) { L->opacity = v / 100.0; m_canvas->update(); }
        m_layerOpVal->setText(QStringLiteral("%1%").arg(v));
    });
    oh->addWidget(ol); oh->addWidget(m_layerOp, 1); oh->addWidget(m_layerOpVal);
    lv->addLayout(oh);
    m_layersBox->setFixedWidth(216);

    // ── Pincéis, cor e tamanhos (à direita) ──
    m_sideScroll = new QScrollArea(this);
    m_sideScroll->setObjectName(QStringLiteral("sketchSideScroll"));
    m_sideScroll->setWidgetResizable(true);
    m_sideScroll->setFrameShape(QFrame::NoFrame);
    m_sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_side = new QFrame();
    m_side->setObjectName(QStringLiteral("sketchPanel"));
    auto* sv = new QVBoxLayout(m_side);
    sv->setContentsMargins(12, 12, 12, 12);
    sv->setSpacing(8);
    auto label = [this](const QString& t) {
        auto* l = new QLabel(t, m_side);
        l->setObjectName(QStringLiteral("sketchLbl"));
        return l;
    };
    sv->addWidget(label(tr("PINCÉIS DA QENNA")));
    m_brushRows = new QVBoxLayout();
    m_brushRows->setSpacing(1);
    sv->addLayout(m_brushRows);
    sv->addWidget(label(tr("MEUS PINCÉIS")));
    m_userRows = new QVBoxLayout();
    m_userRows->setSpacing(1);
    sv->addLayout(m_userRows);
    auto* imp = new QFrame(m_side);
    imp->setObjectName(QStringLiteral("sketchImport"));
    auto* iv = new QVBoxLayout(imp);
    iv->setContentsMargins(10, 8, 10, 8);
    iv->setSpacing(6);
    auto* it = new QLabel(tr("Pincéis do MyPaint (.myb) ou um pacote em .zip. Valem em todos os projetos."), imp);
    it->setObjectName(QStringLiteral("sketchSmall"));
    it->setWordWrap(true);
    auto* ib = new QToolButton(imp);
    ib->setObjectName(QStringLiteral("sketchTextBtn"));
    ib->setCursor(Qt::PointingHandCursor);
    ib->setText(tr("Importar pincéis…"));
    connect(ib, &QToolButton::clicked, this, &SketchEditor::importBrushes);
    m_importNote = new QLabel(imp);
    m_importNote->setObjectName(QStringLiteral("sketchSmall"));
    m_importNote->setWordWrap(true);
    m_importNote->hide();
    iv->addWidget(it); iv->addWidget(ib, 0, Qt::AlignLeft); iv->addWidget(m_importNote);
    sv->addWidget(imp);

    sv->addWidget(label(tr("COR")));
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    int k = 0;
    for (const char* hex : kPalette) {
        auto* b = new QToolButton(m_side);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(18, 18);
        const QColor c(QString::fromLatin1(hex));
        connect(b, &QToolButton::clicked, this, [this, c]() {
            m_color = c;
            QSettings().setValue(QStringLiteral("sketch/color"), c.name());
            if (m_brushInfo.eraser) selectBrush(QStringLiteral("lapis"));
            applyBrush(); refreshSwatches();
        });
        grid->addWidget(b, k / 6, k % 6);
        m_swatches.append({ b, c });
        ++k;
    }
    m_pick = new QToolButton(m_side);
    m_pick->setCursor(Qt::PointingHandCursor);
    m_pick->setFixedSize(18, 18);
    m_pick->setToolTip(tr("Outra cor"));
    connect(m_pick, &QToolButton::clicked, this, [this]() {
        const QColor nc = ColorPopover::getColor(m_color, this, tr("Cor do pincel"));
        if (!nc.isValid()) return;
        m_color = nc;
        QSettings().setValue(QStringLiteral("sketch/color"), nc.name());
        applyBrush(); refreshSwatches();
    });
    grid->addWidget(m_pick, k / 6, k % 6);
    sv->addLayout(grid);

    auto sliderRow = [this, sv](const QString& name, QSlider*& s, QLabel*& val) {
        auto* row = new QHBoxLayout();
        auto* l = new QLabel(name, m_side);
        l->setObjectName(QStringLiteral("sketchSmall"));
        l->setFixedWidth(84);
        s = new QSlider(Qt::Horizontal, m_side);
        s->setObjectName(QStringLiteral("sketchSlider"));
        val = new QLabel(m_side);
        val->setObjectName(QStringLiteral("sketchSmall"));
        val->setFixedWidth(36);
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(l); row->addWidget(s, 1); row->addWidget(val);
        sv->addLayout(row);
    };
    sliderRow(tr("Tamanho"), m_sizeSlider, m_sizeVal);
    m_sizeSlider->setRange(0, 100);
    connect(m_sizeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_diameter = qBound(1.0, diameterFromSlider(v), kMaxDiameter);
        m_sizeVal->setText(QString::number(qRound(m_diameter)));
        QSettings().setValue(QStringLiteral("sketch/size/") + m_brushInfo.id, m_diameter);
        applyBrush();
    });
    sliderRow(tr("Opacidade"), m_opSlider, m_opVal);
    m_opSlider->setRange(10, 100);
    m_opSlider->setValue(int(std::round(m_opacity * 100)));
    m_opVal->setText(QStringLiteral("%1%").arg(m_opSlider->value()));
    connect(m_opSlider, &QSlider::valueChanged, this, [this](int v) {
        m_opacity = v / 100.0;
        m_opVal->setText(QStringLiteral("%1%").arg(v));
        QSettings().setValue(QStringLiteral("sketch/opacity"), m_opacity);
        applyBrush();
    });
    sliderRow(tr("Estabilizador"), m_stabSlider, m_stabVal);
    m_stabSlider->setRange(0, 100);
    m_stabSlider->setValue(LousaInk::stabilizerSetting());
    m_stabVal->setText(QStringLiteral("%1%").arg(m_stabSlider->value()));
    m_stabSlider->setToolTip(tr("Quanto maior, mais o traço vem atrás da caneta, e mais liso sai. Vale pra Caneta e pro Esboço."));
    connect(m_stabSlider, &QSlider::valueChanged, this, [this](int v) {
        LousaInk::setStabilizerSetting(v);
        m_stabVal->setText(QStringLiteral("%1%").arg(v));
    });

    auto* foot = new QHBoxLayout();
    foot->setSpacing(4);
    auto textBtn = [this, foot](const QString& t, const QString& tip, bool primary) {
        auto* b = new QToolButton(m_side);
        b->setObjectName(primary ? QStringLiteral("sketchDone") : QStringLiteral("sketchTextBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setText(t);
        b->setToolTip(tip);
        foot->addWidget(b);
        return b;
    };
    connect(textBtn(tr("Desfazer"), tr("Desfazer (Ctrl+Z)"), false), &QToolButton::clicked, this, [this]() { undo(); });
    connect(textBtn(tr("Limpar camada"), tr("Apaga o desenho da camada escolhida (Ctrl+Z desfaz)"), false), &QToolButton::clicked, this, [this]() { clearLayer(); });
    foot->addStretch(1);
    connect(textBtn(tr("Pronto"), tr("Salvar e voltar pra Lousa (Esc)"), true), &QToolButton::clicked, this, [this]() { done(); });
    sv->addSpacing(4);
    sv->addLayout(foot);
    m_sideScroll->setWidget(m_side);
    m_sideScroll->setFixedWidth(300);

    for (QWidget* w : { static_cast<QWidget*>(m_layersBox), static_cast<QWidget*>(m_sideScroll) }) {
        auto* sh = new QGraphicsDropShadowEffect(w);
        sh->setBlurRadius(30);
        sh->setOffset(0, 10);
        sh->setColor(QColor(0, 0, 0, 110));
        w->setGraphicsEffect(sh);
    }
    applyTheme();
    refreshSwatches();
}

void SketchEditor::applyTheme()
{
    const QColor a(Theme::accentDefault());
    const QString soft = QStringLiteral("rgba(%1,%2,%3,0.18)").arg(a.red()).arg(a.green()).arg(a.blue());
    setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#sketchPanel { background: %1; border: 1px solid %2; border-radius: 12px; }"
        "QScrollArea#sketchSideScroll { background: transparent; border: none; }"
        "QScrollArea#sketchSideScroll > QWidget > QWidget { background: transparent; }"
        "QLabel#sketchLbl { color: %8; font-size: 10.5px; font-weight: 700; letter-spacing: 1.2px; background: transparent; }"
        "QLabel#sketchSmall { color: %3; font-size: 11.5px; background: transparent; }"
        "QLabel#sketchTitle { color: #ffffff; font-size: 13px; font-weight: 600; background: transparent; }"
        "QToolButton#sketchCapBtn { background: rgba(255,255,255,0.16); color: #ffffff; border: none;"
        "  border-radius: 11px; padding: 3px 11px; font-size: 12px; }"
        "QToolButton#sketchCapBtn:hover { background: rgba(255,255,255,0.28); }"
        "QToolButton#sketchBrush, QToolButton#sketchLayer { background: transparent; border: none; border-radius: 7px;"
        "  color: %3; font-size: 12px; text-align: left; padding: 2px 6px; }"
        "QToolButton#sketchBrush:hover, QToolButton#sketchLayer:hover { background: %4; }"
        "QToolButton#sketchBrush:checked, QToolButton#sketchLayer:checked { background: %5; color: %6; font-weight: 600; }"
        "QToolButton#sketchLayer[hiddenLayer=\"true\"] { color: %8; }"
        "QToolButton#sketchIconBtn, QToolButton#sketchEye { background: transparent; border: none; border-radius: 6px; }"
        "QToolButton#sketchIconBtn:hover, QToolButton#sketchEye:hover { background: %4; }"
        "QToolButton#sketchIconBtn:disabled { opacity: 0.35; }"
        "QFrame#sketchImport { background: transparent; border: 1.5px dashed %2; border-radius: 8px; }"
        "QToolButton#sketchTextBtn { background: %4; border: none; border-radius: 7px; color: %3; font-size: 12px; padding: 6px 10px; }"
        "QToolButton#sketchTextBtn:hover { background: %5; color: %6; }"
        "QToolButton#sketchDone { background: %7; border: none; border-radius: 7px; color: #ffffff;"
        "  font-size: 12px; font-weight: 600; padding: 6px 14px; }"
        "QToolButton#sketchDone:hover { background: %9; }"
        "QSlider#sketchSlider::groove:horizontal { background: %2; height: 4px; border-radius: 2px; }"
        "QSlider#sketchSlider::sub-page:horizontal { background: %7; border-radius: 2px; }"
        "QSlider#sketchSlider::handle:horizontal { background: %7; width: 12px; margin: -5px 0; border-radius: 6px; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(), Theme::hoverStrong(),
          soft, Theme::textBright(), a.name(), Theme::textMuted(), a.darker(112).name())));
}

void SketchEditor::layoutUi()
{
    if (!m_canvas) return;
    const int m = 24;
    const int sideW = m_sideScroll->width();
    const int layW = m_layersBox->width();
    const QRect area(m + layW + m, m + 34, width() - (m + layW + m) - (m + sideW + m), height() - (m + 34) - m);
    m_canvas->setGeometry(area);
    const QRectF sr = m_canvas->fitRect().translated(area.topLeft());
    m_caption->adjustSize();
    m_caption->move(int(sr.left()), int(sr.top()) - m_caption->height() - 8);
    m_layersBox->adjustSize();
    m_layersBox->move(int(sr.left()) - m_layersBox->width() - 20, int(sr.top()) + 140);
    if (m_layersBox->x() < m) m_layersBox->move(m, m_layersBox->y());
    m_side->adjustSize();
    const int sideH = qMin(height() - 2 * m, m_side->sizeHint().height() + 2);
    m_sideScroll->setFixedHeight(sideH);
    m_sideScroll->move(int(sr.right()) + 20, qMax(m, int(sr.top()) - 30));
    if (m_sideScroll->geometry().right() > width() - m) m_sideScroll->move(width() - m - sideW, m_sideScroll->y());
    if (m_sideScroll->geometry().bottom() > height() - m) m_sideScroll->move(m_sideScroll->x(), height() - m - sideH);
}

void SketchEditor::resizeEvent(QResizeEvent*)
{
    layoutUi();
}

void SketchEditor::refreshZoomLabel()
{
    if (m_zoomLbl) m_zoomLbl->setText(QStringLiteral("%1%").arg(qRound(m_canvas->zoom() * 100)));
}

void SketchEditor::keyReleaseEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) { m_canvas->setPanKey(false); return; }
    QWidget::keyReleaseEvent(e);
}

void SketchEditor::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(18, 14, 10, 150));   // a Lousa apagada em volta
}

void SketchEditor::mousePressEvent(QMouseEvent* e)
{
    e->accept();   // clicar no escuro não fecha nem passa pra Lousa
}

void SketchEditor::keyPressEvent(QKeyEvent* e)
{
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    const bool shift = e->modifiers() & Qt::ShiftModifier;
    if (e->key() == Qt::Key_Escape) { done(); return; }
    if (e->key() == Qt::Key_Space) { if (!e->isAutoRepeat()) m_canvas->setPanKey(true); return; }
    if (ctrl && (e->key() == Qt::Key_Plus || e->key() == Qt::Key_Equal)) { m_canvas->zoomBy(1.25); return; }
    if (ctrl && e->key() == Qt::Key_Minus) { m_canvas->zoomBy(1.0 / 1.25); return; }
    if (ctrl && e->key() == Qt::Key_0) { m_canvas->fitToScreen(); return; }
    if (ctrl && e->key() == Qt::Key_Z && !shift) { undo(); return; }
    if (ctrl && (e->key() == Qt::Key_Y || (e->key() == Qt::Key_Z && shift))) { redo(); return; }
    if (e->key() == Qt::Key_BracketLeft || e->key() == Qt::Key_BracketRight) {
        m_sizeSlider->setValue(m_sizeSlider->value() + (e->key() == Qt::Key_BracketRight ? 4 : -4));
        return;
    }
    e->accept();
}
