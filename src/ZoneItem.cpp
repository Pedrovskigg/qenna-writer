#include "ZoneItem.h"

#include <QColorDialog>
#include <QInputDialog>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>

// ── Resize handle defs (mirrors Mira 1 RESIZE_HANDLES) ───────────────────────
namespace {
struct HandleDef {
    qreal rx, ry;           // position as fraction of (w, h)
    Qt::CursorShape cursor;
    bool top, bot, left, right;
};
constexpr HandleDef kHandles[8] = {
    {0,   0,   Qt::SizeFDiagCursor, true,  false, true,  false}, // NW
    {0.5, 0,   Qt::SizeVerCursor,   true,  false, false, false}, // N
    {1,   0,   Qt::SizeBDiagCursor, true,  false, false, true }, // NE
    {0,   0.5, Qt::SizeHorCursor,   false, false, true,  false}, // W
    {1,   0.5, Qt::SizeHorCursor,   false, false, false, true }, // E
    {0,   1,   Qt::SizeBDiagCursor, false, true,  true,  false}, // SW
    {0.5, 1,   Qt::SizeVerCursor,   false, true,  false, false}, // S
    {1,   1,   Qt::SizeFDiagCursor, false, true,  false, true }, // SE
};
constexpr qreal kHandleR  = 7.0;   // raio do círculo de handle
constexpr qreal kHandleHit = 14.0; // raio de hit-test
constexpr qreal kCtrlH    = 50.0;  // altura da faixa de controles no topo
constexpr qreal kMinSize  = 80.0;
constexpr qreal kRadius   = 10.0;
} // namespace

// ─── Constructor ────────────────────────────────────────────────────────────

ZoneItem::ZoneItem(const CanvasZone& data, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_data(data)
{
    setZValue(-2.0);          // atrás de cards (-0) e conexões (-1)
    setAcceptHoverEvents(true);
    setFlag(ItemIsMovable, false);
    setFlag(ItemIsSelectable, false);
    setPos(m_data.x, m_data.y);
}

void ZoneItem::setZoneData(const CanvasZone& d)
{
    prepareGeometryChange();
    setPos(d.x, d.y);
    m_data = d;
    update();
}

// ─── Geometria ───────────────────────────────────────────────────────────────

QRectF ZoneItem::boundingRect() const
{
    return QRectF(-kHandleR, -kHandleR,
                  m_data.width  + kHandleR * 2,
                  m_data.height + kHandleR * 2);
}

QPainterPath ZoneItem::shape() const
{
    QPainterPath p;
    p.addRect(QRectF(0, 0, m_data.width, m_data.height));
    return p;
}

// ─── Pintura ─────────────────────────────────────────────────────────────────

void ZoneItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    const qreal w  = m_data.width;
    const qreal h  = m_data.height;
    const QColor& clr = m_data.color;
    p->setRenderHint(QPainter::Antialiasing);

    // Fundo: transparente ao passar o mouse, leve tint fora
    if (!m_hovered) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(clr.red(), clr.green(), clr.blue(), 10));
        p->drawRoundedRect(QRectF(0, 0, w, h), kRadius, kRadius);
    }

    // Borda dashed
    QPen dashedPen(clr, 2, Qt::DashLine);
    dashedPen.setDashPattern({6, 4});
    p->setPen(dashedPen);
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(QRectF(0, 0, w, h), kRadius, kRadius);

    // ── Título centralizado (grande, 45% opacity) ──────────────────────────
    if (!m_data.title.isEmpty()) {
        const qreal fontSize = qBound(14.0, h * 0.18, 52.0);
        p->setFont(QFont(QStringLiteral("Segoe UI"), qRound(fontSize), QFont::Bold));
        const QColor lblColor(clr.red(), clr.green(), clr.blue(), 115); // ~45%
        p->setPen(lblColor);
        p->drawText(QRectF(12, kCtrlH, w - 24, h - kCtrlH - 8),
                    Qt::AlignCenter | Qt::TextWordWrap,
                    m_data.title.toUpper());
    }

    // ── Controles no topo-direito ─────────────────────────────────────────
    const QColor ctrlClr(clr.red(), clr.green(), clr.blue(), 179); // ~70%
    const QColor mutedClr(clr.red(), clr.green(), clr.blue(), 100);

    // Grip (3×3 pontos)
    p->setPen(Qt::NoPen);
    p->setBrush(mutedClr);
    const qreal gx = w - 90, gy = 14.0;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            p->drawEllipse(QPointF(gx + col * 5, gy + row * 5), 1.5, 1.5);

    // Nome da zona (à esquerda do color dot)
    p->setFont(QFont(QStringLiteral("Segoe UI"), 12, QFont::DemiBold));
    p->setPen(ctrlClr);
    p->drawText(QRectF(w - 195, 4, 100, kCtrlH - 4),
                Qt::AlignVCenter | Qt::AlignRight,
                m_data.title.isEmpty() ? tr("Área") : m_data.title);

    // Color dot
    p->setPen(QPen(QColor(255,255,255,76), 1.5));
    p->setBrush(clr);
    p->drawEllipse(QPointF(w - 34, kCtrlH / 2.0), 9, 9);

    // × button
    p->setPen(QPen(mutedClr, 1.4, Qt::SolidLine, Qt::RoundCap));
    const qreal xx = w - 14, xy = kCtrlH / 2.0;
    p->drawLine(QPointF(xx-4, xy-4), QPointF(xx+4, xy+4));
    p->drawLine(QPointF(xx+4, xy-4), QPointF(xx-4, xy+4));

    // ── 8 handles de resize (visíveis no hover) ────────────────────────────
    if (m_hovered) {
        for (const HandleDef& hd : kHandles) {
            const QPointF hpt(hd.rx * w, hd.ry * h);
            p->setPen(QPen(QColor(255,255,255,204), 2));
            p->setBrush(clr);
            p->drawEllipse(hpt, kHandleR, kHandleR);
        }
    }
}

// ─── Hit-test helpers ────────────────────────────────────────────────────────

bool ZoneItem::isOnGrip(const QPointF& p) const
{
    return QRectF(m_data.width - 100, 0, 30, kCtrlH).contains(p);
}
bool ZoneItem::isOnDelete(const QPointF& p) const
{
    return QRectF(m_data.width - 22, 0, 22, kCtrlH).contains(p);
}
bool ZoneItem::isOnColorDot(const QPointF& p) const
{
    return QLineF(p, QPointF(m_data.width - 34, kCtrlH / 2.0)).length() <= 14;
}
int ZoneItem::handleAt(const QPointF& p) const
{
    const qreal w = m_data.width, h = m_data.height;
    for (int i = 0; i < 8; ++i) {
        const QPointF hpt(kHandles[i].rx * w, kHandles[i].ry * h);
        if (QLineF(p, hpt).length() <= kHandleHit) return i;
    }
    return -1;
}

void ZoneItem::emitData()
{
    CanvasZone d = m_data;
    const QPointF sc = pos();
    d.x = sc.x(); d.y = sc.y();
    emit dataChanged(d);
}

// ─── Mouse events ────────────────────────────────────────────────────────────

void ZoneItem::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { e->ignore(); return; }
    const QPointF lp = e->pos();

    if (isOnDelete(lp)) {
        emit removeRequested(m_data.id);
        e->accept(); return;
    }
    if (isOnColorDot(lp)) {
        QColor nc = QColorDialog::getColor(m_data.color, nullptr, tr("Cor da área"));
        if (nc.isValid()) {
            m_data.color = nc;
            update();
            emitData();
        }
        e->accept(); return;
    }
    const int hi = handleAt(lp);
    if (m_hovered && hi >= 0) {
        m_resizing     = true;
        m_resizeHandle = hi;
        m_pressScene   = e->scenePos();
        m_pressOrigin  = pos();
        m_pressSize    = QSizeF(m_data.width, m_data.height);
        setCursor(kHandles[hi].cursor);
        e->accept(); return;
    }
    if (isOnGrip(lp) || lp.y() < kCtrlH) {
        m_dragging     = true;
        m_pressScene   = e->scenePos();
        m_pressOrigin  = pos();
        setCursor(Qt::ClosedHandCursor);
        e->accept(); return;
    }
    e->ignore();
}

void ZoneItem::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
    if (m_dragging) {
        const QPointF delta = e->scenePos() - m_pressScene;
        setPos(m_pressOrigin + delta);
        e->accept(); return;
    }
    if (m_resizing) {
        const QPointF d = e->scenePos() - m_pressScene;
        const HandleDef& hd = kHandles[m_resizeHandle];
        qreal nx = m_pressOrigin.x(), ny = m_pressOrigin.y();
        qreal nw = m_pressSize.width(), nh = m_pressSize.height();
        if (hd.right)  nw = qMax(kMinSize, nw + d.x());
        if (hd.bot)    nh = qMax(kMinSize, nh + d.y());
        if (hd.left)   { nx += d.x(); nw = qMax(kMinSize, nw - d.x()); }
        if (hd.top)    { ny += d.y(); nh = qMax(kMinSize, nh - d.y()); }
        prepareGeometryChange();
        setPos(nx, ny);
        m_data.width  = nw;
        m_data.height = nh;
        update();
        e->accept(); return;
    }
    e->ignore();
}

void ZoneItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
    if (m_dragging || m_resizing) {
        m_dragging = m_resizing = false;
        setCursor(Qt::ArrowCursor);
        emitData();
        e->accept(); return;
    }
    e->ignore();
}

void ZoneItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)
{
    // Double-click no título → editar nome
    const QString name = QInputDialog::getText(
        nullptr, tr("Nome da área"), tr("Nome:"),
        QLineEdit::Normal, m_data.title);
    if (!name.isNull()) {
        m_data.title = name.trimmed();
        update();
        emitData();
    }
    e->accept();
}

// ─── Hover ───────────────────────────────────────────────────────────────────

void ZoneItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    m_hovered = true;
    update();
}
void ZoneItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    m_hovered = false;
    update();
}

// ─── Context menu ─────────────────────────────────────────────────────────────

void ZoneItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* e)
{
    QMenu menu;
    menu.addAction(tr("Renomear área"), this, [this]() {
        const QString name = QInputDialog::getText(
            nullptr, tr("Nome da área"), tr("Nome:"),
            QLineEdit::Normal, m_data.title);
        if (!name.isNull()) { m_data.title = name.trimmed(); update(); emitData(); }
    });
    menu.addAction(tr("Cor..."), this, [this]() {
        QColor nc = QColorDialog::getColor(m_data.color, nullptr, tr("Cor da área"));
        if (nc.isValid()) { m_data.color = nc; update(); emitData(); }
    });
    menu.addSeparator();
    menu.addAction(tr("Remover área"), this, [this]() { emit removeRequested(m_data.id); });
    menu.exec(e->screenPos());
}
