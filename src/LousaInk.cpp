#include "LousaInk.h"

#include "Theme.h"

#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QJsonArray>
#include <QPainter>
#include <QPainterPathStroker>
#include <QSettings>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {

// Espessura de cada ponta a partir do tamanho de referência.
qreal widthFor(const CanvasInk& ink)
{
    if (ink.tool == QStringLiteral("marker"))    return ink.size * 1.9;
    if (ink.tool == QStringLiteral("highlight")) return ink.size * 4.5;
    return ink.size;
}

// Caminho suave pelos pontos (curvas pelos pontos médios).
QPainterPath smoothPath(const QVector<InkPoint>& pts)
{
    QPainterPath path;
    if (pts.isEmpty()) return path;
    path.moveTo(pts[0].x, pts[0].y);
    if (pts.size() == 1) { path.lineTo(pts[0].x + 0.01, pts[0].y); return path; }
    if (pts.size() == 2) { path.lineTo(pts[1].x, pts[1].y); return path; }
    for (int i = 1; i < pts.size() - 1; ++i) {
        const QPointF c(pts[i].x, pts[i].y);
        const QPointF n((pts[i].x + pts[i + 1].x) / 2.0, (pts[i].y + pts[i + 1].y) / 2.0);
        path.quadTo(c, n);
    }
    path.lineTo(pts.last().x, pts.last().y);
    return path;
}

} // namespace

namespace LousaInk {

QJsonObject toJson(const CanvasInk& ink)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), ink.id);
    o.insert(QStringLiteral("tool"), ink.tool);
    o.insert(QStringLiteral("color"), ink.color.name(QColor::HexArgb));
    o.insert(QStringLiteral("size"), ink.size);
    // x, y, pressão em sequência, com uma casa (o arquivo da lousa não incha)
    QJsonArray a;
    for (const InkPoint& p : ink.pts) {
        a.append(std::round(p.x * 10.0) / 10.0);
        a.append(std::round(p.y * 10.0) / 10.0);
        a.append(std::round(p.p * 100.0) / 100.0);
    }
    o.insert(QStringLiteral("pts"), a);
    return o;
}

CanvasInk fromJson(const QJsonObject& o)
{
    CanvasInk ink;
    ink.id    = o.value(QStringLiteral("id")).toString();
    ink.tool  = o.value(QStringLiteral("tool")).toString(QStringLiteral("pen"));
    ink.color = QColor(o.value(QStringLiteral("color")).toString(QStringLiteral("#c0392b")));
    ink.size  = o.value(QStringLiteral("size")).toDouble(4.0);
    const QJsonArray a = o.value(QStringLiteral("pts")).toArray();
    ink.pts.reserve(a.size() / 3);
    for (int i = 0; i + 2 < a.size(); i += 3)
        ink.pts.append({ float(a[i].toDouble()), float(a[i + 1].toDouble()), float(a[i + 2].toDouble()) });
    return ink;
}

QRectF bounds(const CanvasInk& ink)
{
    if (ink.pts.isEmpty()) return {};
    float x0 = ink.pts[0].x, x1 = x0, y0 = ink.pts[0].y, y1 = y0;
    for (const InkPoint& p : ink.pts) {
        x0 = std::min(x0, p.x); x1 = std::max(x1, p.x);
        y0 = std::min(y0, p.y); y1 = std::max(y1, p.y);
    }
    const qreal pad = widthFor(ink) / 2.0 + 2.0;
    return QRectF(QPointF(x0, y0), QPointF(x1, y1)).adjusted(-pad, -pad, pad, pad);
}

QPainterPath hitPath(const CanvasInk& ink, qreal extra)
{
    QPainterPathStroker st;
    st.setWidth(widthFor(ink) + extra * 2.0);
    st.setCapStyle(Qt::RoundCap);
    st.setJoinStyle(Qt::RoundJoin);
    return st.createStroke(smoothPath(ink.pts));
}

void paint(QPainter* p, const CanvasInk& ink)
{
    if (ink.pts.isEmpty()) return;
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setBrush(Qt::NoBrush);
    if (ink.tool == QStringLiteral("pen")) {
        // A espessura segue a pressão, trecho a trecho.
        QPen pen(ink.color);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        if (ink.pts.size() == 1) {
            p->setPen(Qt::NoPen);
            p->setBrush(ink.color);
            const qreal r = ink.size * (0.35 + 0.65 * ink.pts[0].p) / 2.0;
            p->drawEllipse(QPointF(ink.pts[0].x, ink.pts[0].y), r, r);
        } else {
            for (int i = 1; i < ink.pts.size(); ++i) {
                const InkPoint& a = ink.pts[i - 1];
                const InkPoint& b = ink.pts[i];
                pen.setWidthF(std::max(0.6, ink.size * (0.3 + 0.7 * (a.p + b.p) / 2.0)));
                p->setPen(pen);
                p->drawLine(QPointF(a.x, a.y), QPointF(b.x, b.y));
            }
        }
    } else {
        // Marcador e marca-texto: espessura fixa, um caminho só (passar por cima
        // de si mesmo não escurece).
        const bool hl = ink.tool == QStringLiteral("highlight");
        QColor c = ink.color;
        c.setAlphaF(hl ? 0.38 : 0.92);
        QPen pen(c, widthFor(ink), Qt::SolidLine, hl ? Qt::FlatCap : Qt::RoundCap, Qt::RoundJoin);
        if (hl) p->setCompositionMode(QPainter::CompositionMode_Multiply);
        p->setPen(pen);
        p->drawPath(smoothPath(ink.pts));
    }
    p->restore();
}

void translate(CanvasInk& ink, const QPointF& d)
{
    for (InkPoint& p : ink.pts) { p.x += float(d.x()); p.y += float(d.y()); }
}

bool snapShape(QVector<InkPoint>& pts, QString* name)
{
    if (pts.size() < 3) return false;
    float x0 = pts[0].x, x1 = x0, y0 = pts[0].y, y1 = y0;
    qreal len = 0, pSum = 0;
    for (int i = 0; i < pts.size(); ++i) {
        x0 = std::min(x0, pts[i].x); x1 = std::max(x1, pts[i].x);
        y0 = std::min(y0, pts[i].y); y1 = std::max(y1, pts[i].y);
        if (i) len += std::hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
        pSum += pts[i].p;
    }
    if (len < 40) return false;
    const qreal w = x1 - x0, h = y1 - y0;
    const InkPoint first = pts.first(), last = pts.last();
    const float p = float(pSum / pts.size());
    const bool closed = std::hypot(first.x - last.x, first.y - last.y) < std::max(w, h) * 0.35 && len > (w + h) * 1.2;
    QVector<InkPoint> out;
    if (closed) {
        const qreal cx = (x0 + x1) / 2.0, cy = (y0 + y1) / 2.0;
        qreal rx = w / 2.0, ry = h / 2.0;
        QString n = QStringLiteral("oval");
        if (std::abs(rx - ry) < std::max(rx, ry) * 0.12) { rx = ry = (rx + ry) / 2.0; n = QStringLiteral("circle"); }
        for (int t = 0; t <= 72; ++t) {
            const qreal a = t / 72.0 * 2.0 * M_PI;
            out.append({ float(cx + std::cos(a) * rx), float(cy + std::sin(a) * ry), p });
        }
        if (name) *name = n;
    } else {
        out.append({ first.x, first.y, p });
        out.append({ last.x, last.y, p });
        if (name) *name = QStringLiteral("line");
    }
    pts = out;
    return true;
}

int stabilizerSetting()
{
    return qBound(0, QSettings().value(QStringLiteral("lousa/stabilizer"), 30).toInt(), 100);
}

void setStabilizerSetting(int v)
{
    QSettings().setValue(QStringLiteral("lousa/stabilizer"), qBound(0, v, 100));
}

qreal Stabilizer::stringLengthPx(int strength)
{
    // 100% = um fio de 64 px; a curva cresce devagar no começo, pra os
    // valores baixos continuarem sutis.
    const qreal s = qBound(0, strength, 100) / 100.0;
    return 64.0 * s * (0.35 + 0.65 * s);
}

QPointF Stabilizer::feed(const QPointF& raw, int strength, qreal pxPerUnit)
{
    if (!m_has || strength <= 0) { m_pos = raw; m_has = true; return raw; }
    const qreal r = stringLengthPx(strength) / qMax(0.01, pxPerUnit);
    const QPointF d = raw - m_pos;
    const qreal len = std::hypot(d.x(), d.y());
    if (len > r) m_pos += d * ((len - r) / len);   // o fio esticou: a ponta vem atrás
    return m_pos;
}

} // namespace LousaInk

// ── InkItem ──────────────────────────────────────────────────────────────────

InkItem::InkItem(const CanvasInk& data, QGraphicsItem* parent)
    : QGraphicsObject(parent), m_data(data)
{
    setZValue(kZ);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    rebuildGeometry();
}

void InkItem::rebuildGeometry()
{
    prepareGeometryChange();
    m_bounds = LousaInk::bounds(m_data).adjusted(-6, -6, 6, 6);   // o tracejado da seleção
    m_hit    = LousaInk::hitPath(m_data, 5.0);
}

void InkItem::setInkData(const CanvasInk& data)
{
    m_data = data;
    rebuildGeometry();
    update();
}

void InkItem::setInkSelected(bool on)
{
    if (m_selected == on) return;
    m_selected = on;
    update();
}

void InkItem::setPreview(bool on)
{
    m_preview = on;
    setAcceptedMouseButtons(on ? Qt::NoButton : Qt::LeftButton);
    setAcceptHoverEvents(!on);
}

QRectF InkItem::boundingRect() const { return m_bounds; }

QPainterPath InkItem::shape() const
{
    if (m_preview) return {};
    if (m_selected) {   // marcado, a caixa toda arrasta
        QPainterPath p;
        p.addRect(LousaInk::bounds(m_data).adjusted(-4, -4, 4, 4));
        return p;
    }
    return m_hit;
}

void InkItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    LousaInk::paint(p, m_data);
    if (m_selected || (m_hovered && !m_preview)) {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QColor a(Theme::accentDefault());
        if (!m_selected) a.setAlphaF(0.45);
        QPen pen(a, 1.5, Qt::DashLine);
        pen.setCosmetic(true);
        pen.setDashPattern({ 5, 4 });
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(LousaInk::bounds(m_data).adjusted(-4, -4, 4, 4), 4, 4);
        p->restore();
    }
}

void InkItem::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { e->ignore(); return; }
    emit pressed(this);
    m_dragging = true;
    m_moved = false;
    m_pressScene = m_lastScene = e->scenePos();
    setCursor(Qt::ClosedHandCursor);
    e->accept();
}

void InkItem::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
    if (!m_dragging) return;
    if (!m_moved) {
        if (QLineF(m_pressScene, e->scenePos()).length() < 3) return;
        m_moved = true;
        emit gestureStarted();
    }
    LousaInk::translate(m_data, e->scenePos() - m_lastScene);
    m_lastScene = e->scenePos();
    rebuildGeometry();
    update();
}

void InkItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
    Q_UNUSED(e);
    if (m_dragging && m_moved) emit moved(this);
    m_dragging = false;
    setCursor(Qt::OpenHandCursor);
}

void InkItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    m_hovered = true;
    setCursor(Qt::OpenHandCursor);
    update();
}

void InkItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    m_hovered = false;
    unsetCursor();
    update();
}
