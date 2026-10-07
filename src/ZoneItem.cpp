#include "ZoneItem.h"
#include "SheetDialogs.h"
#include "ColorPopover.h"

#include "Theme.h"

#include <QColorDialog>
#include <QInputDialog>
#include <QFontMetricsF>
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
constexpr qreal kHandleR  = 6.0;   // raio do círculo de handle
constexpr qreal kHandleHit = 13.0; // raio de hit-test
constexpr qreal kBandH    = 26.0;  // faixa de cima, de onde se arrasta a área
constexpr qreal kTagH     = 26.0;  // altura da etiqueta
constexpr qreal kMinSize  = 80.0;
constexpr qreal kRadius   = 12.0;

QFont tagFont(qreal k)
{
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(qMax(1, qRound(11 * k)));
    f.setWeight(QFont::Bold);
    f.setStyleStrategy(QFont::NoSubpixelAntialias);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.9 * k);
    return f;
}

bool isTopMiddle(const HandleDef& hd) { return hd.ry == 0 && hd.rx > 0 && hd.rx < 1; }
} // namespace

QColor ZoneItem::s_boardColor = QColor(0x1a, 0x1a, 0x19);
qreal  ZoneItem::s_labelScale = 1.0;

void ZoneItem::setViewZoom(const QList<ZoneItem*>& zones, qreal zoom)
{
    // Com o zoom longe a etiqueta cresce, até 4×, pra continuar legível.
    const qreal k = qBound(1.0, 1.0 / qMax(0.01, zoom), 4.0);
    if (qFuzzyCompare(k, s_labelScale)) return;
    for (ZoneItem* z : zones) z->prepareGeometryChange();
    s_labelScale = k;
    for (ZoneItem* z : zones) z->update();
}

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

void ZoneItem::setSelected(bool on)
{
    if (m_selected == on) return;
    m_selected = on;
    update();
}

void ZoneItem::setCardCount(int n)
{
    if (m_count == n) return;
    prepareGeometryChange();   // a etiqueta muda de largura
    m_count = n;
    update();
}

// ─── Geometria ───────────────────────────────────────────────────────────────

qreal ZoneItem::labelScale() const
{
    // Cresce com o zoom longe, mas só até o nome inteiro caber na área.
    if (s_labelScale <= 1.0) return 1.0;
    const QFontMetricsF fm(tagFont(1.0));
    const QString name = (m_data.title.isEmpty() ? tr("Área") : m_data.title).toUpper();
    qreal natural = 28 + fm.horizontalAdvance(name) + 14;
    if (m_count > 0) natural += 8 + fm.horizontalAdvance(QString::number(m_count));
    const qreal fit = (m_data.width - 32) / qMax(1.0, natural + 66);   // cabe junto com cor/×
    return qBound(1.0, qMin(s_labelScale, fit), 4.0);
}

QRectF ZoneItem::tagRect() const
{
    const qreal k = labelScale();
    const QFontMetricsF fm(tagFont(k));
    const QString name = (m_data.title.isEmpty() ? tr("Área") : m_data.title).toUpper();
    qreal w = 28 * k + fm.horizontalAdvance(name) + 14 * k;
    if (m_count > 0) w += 8 * k + fm.horizontalAdvance(QString::number(m_count));
    w = qMin(w, qMax(80.0 * k, m_data.width - 32 - 66 * k));
    w = qMin(w, qMax(40.0, m_data.width - 32));       // nunca passa da largura da área
    const qreal h = kTagH * k;
    return QRectF(16, -h / 2.0, w, h);
}

QRectF ZoneItem::controlsRect() const
{
    const qreal k = labelScale();
    return QRectF(m_data.width - 16 - 58 * k, -kTagH * k / 2.0, 58 * k, kTagH * k);
}

QRectF ZoneItem::boundingRect() const
{
    const qreal half = kTagH * labelScale() / 2.0;
    return QRectF(-kHandleR - 4, -half - 2,
                  m_data.width  + kHandleR * 2 + 8,
                  m_data.height + half + kHandleR + 6);
}

QPainterPath ZoneItem::shape() const
{
    // O corpo da área deixa o clique passar pro quadro (arrastar o fundo).
    // Recebem eventos: a etiqueta, os controles, a faixa de cima e as alças.
    const qreal w = m_data.width, h = m_data.height;
    QPainterPath p;
    p.addRect(QRectF(0, 0, w, kBandH));
    const QRectF tag = tagRect();
    p.addRoundedRect(tag, tag.height() / 2.0, tag.height() / 2.0);
    p.addRoundedRect(controlsRect(), tag.height() / 2.0, tag.height() / 2.0);
    for (const HandleDef& hd : kHandles) {
        if (isTopMiddle(hd)) continue;
        p.addEllipse(QPointF(hd.rx * w, hd.ry * h), kHandleHit, kHandleHit);
    }
    return p;
}

// ─── Pintura ─────────────────────────────────────────────────────────────────

void ZoneItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    const qreal w  = m_data.width;
    const qreal h  = m_data.height;
    const QColor clr = m_data.color.isValid() ? m_data.color : QColor(0x6e, 0xa8, 0xfe);
    const bool boardLight = s_boardColor.lightness() > 150;
    p->setRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::TextAntialiasing);

    // Fundo tingido e borda contínua
    p->setPen(QPen(QColor(clr.red(), clr.green(), clr.blue(), boardLight ? 150 : 118), 1.5));
    p->setBrush(QColor(clr.red(), clr.green(), clr.blue(), boardLight ? 22 : 16));
    p->drawRoundedRect(QRectF(0, 0, w, h), kRadius, kRadius);

    if (m_selected) {
        const QColor a(Theme::accentDefault());
        p->setPen(QPen(a, 2.2));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(QRectF(-3, -3, w + 6, h + 6), kRadius + 3, kRadius + 3);
    }

    // Etiqueta: bolinha, NOME e quantos cards tem dentro
    const qreal k = labelScale();
    const QRectF tag = tagRect();
    const qreal tr2 = tag.height() / 2.0;
    const QColor border(clr.red(), clr.green(), clr.blue(), boardLight ? 170 : 128);
    p->setPen(QPen(border, 1.5 * k));
    p->setBrush(s_boardColor);
    p->drawRoundedRect(tag, tr2, tr2);
    p->setPen(Qt::NoPen);
    p->setBrush(clr);
    p->drawEllipse(QPointF(tag.left() + 16 * k, tag.center().y()), 4 * k, 4 * k);

    const QColor txt = boardLight ? clr.darker(165) : clr.lighter(130);
    const QFont f = tagFont(k);
    p->setFont(f);
    const QFontMetricsF fm(f);
    const QString count = m_count > 0 ? QString::number(m_count) : QString();
    const qreal countW = count.isEmpty() ? 0.0 : fm.horizontalAdvance(count) + 8 * k;
    const qreal nameX = tag.left() + 28 * k;
    const qreal nameW = tag.right() - 14 * k - countW - nameX;
    const QString name = (m_data.title.isEmpty() ? tr("Área") : m_data.title).toUpper();
    p->setPen(txt);
    p->drawText(QRectF(nameX, tag.top(), nameW, tag.height()), Qt::AlignVCenter | Qt::AlignLeft,
                fm.elidedText(name, Qt::ElideRight, nameW));
    if (!count.isEmpty()) {
        p->setPen(QColor(txt.red(), txt.green(), txt.blue(), 150));
        p->drawText(QRectF(tag.right() - 14 * k - countW + 8 * k, tag.top(), countW, tag.height()),
                    Qt::AlignVCenter | Qt::AlignLeft, count);
    }

    // Cor e × aparecem com o mouse em cima (ou com a área marcada)
    if (m_hovered || m_selected) {
        const QRectF cr = controlsRect();
        p->setPen(QPen(border, 1.5 * k));
        p->setBrush(s_boardColor);
        p->drawRoundedRect(cr, tr2, tr2);
        p->setPen(QPen(QColor(255, 255, 255, 120), 1.2 * k));
        p->setBrush(clr);
        p->drawEllipse(QPointF(cr.left() + 18 * k, cr.center().y()), 6 * k, 6 * k);
        p->setPen(QPen(txt, 1.4 * k, Qt::SolidLine, Qt::RoundCap));
        const QPointF xc(cr.right() - 18 * k, cr.center().y());
        const qreal xs = 4 * k;
        p->drawLine(xc + QPointF(-xs, -xs), xc + QPointF(xs, xs));
        p->drawLine(xc + QPointF(xs, -xs), xc + QPointF(-xs, xs));
    }

    // Alças de tamanho (com o mouse em cima)
    if (m_hovered) {
        for (const HandleDef& hd : kHandles) {
            if (isTopMiddle(hd)) continue;   // ficaria atrás da etiqueta
            const QPointF hpt(hd.rx * w, hd.ry * h);
            p->setPen(QPen(s_boardColor, 2));
            p->setBrush(clr);
            p->drawEllipse(hpt, kHandleR, kHandleR);
        }
    }
}

// ─── Hit-test helpers ────────────────────────────────────────────────────────

bool ZoneItem::isOnDelete(const QPointF& p) const
{
    if (!m_hovered && !m_selected) return false;
    const QRectF cr = controlsRect();
    return QRectF(cr.right() - 30 * labelScale(), cr.top(), 30 * labelScale(), cr.height()).contains(p);
}
bool ZoneItem::isOnColorDot(const QPointF& p) const
{
    if (!m_hovered && !m_selected) return false;
    const QRectF cr = controlsRect();
    return QRectF(cr.left(), cr.top(), 30 * labelScale(), cr.height()).contains(p);
}
int ZoneItem::handleAt(const QPointF& p) const
{
    const qreal w = m_data.width, h = m_data.height;
    for (int i = 0; i < 8; ++i) {
        if (isTopMiddle(kHandles[i])) continue;
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

    // Qualquer clique na zona a seleciona (para exportar).
    emit zoneClicked(m_data.id);

    if (isOnDelete(lp)) {
        emit removeRequested(m_data.id);
        e->accept(); return;
    }
    if (isOnColorDot(lp)) {
        QColor nc = ColorPopover::getColor(m_data.color, nullptr, tr("Cor da área"));
        if (nc.isValid()) {
            emit gestureStarted();
            m_data.color = nc;
            update();
            emitData();
        }
        e->accept(); return;
    }
    const int hi = handleAt(lp);
    if (m_hovered && hi >= 0) {
        emit gestureStarted();
        m_resizing     = true;
        m_resizeHandle = hi;
        m_pressScene   = e->scenePos();
        m_pressOrigin  = pos();
        m_pressSize    = QSizeF(m_data.width, m_data.height);
        setCursor(kHandles[hi].cursor);
        e->accept(); return;
    }
    if (tagRect().contains(lp) || lp.y() < kBandH) {
        emit gestureStarted();
        // Shift ou Ctrl + clique: marca a área com tudo o que tem dentro, e o
        // arrasto leva junto. (Ctrl+Shift continua valendo.)
        const bool withAll = (e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier));
        if (withAll) emit contentsSelectRequested(m_data.id);
        m_moveContents = withAll || m_contentsSelected;
        m_dragging     = true;
        m_pressScene   = e->scenePos();
        m_pressOrigin  = pos();
        emit dragStartedWithContents(m_data.id, m_moveContents);
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
        emit draggedBy(m_data.id, delta);
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
    bool okName = false;
    const QString name = Sheets::askText(nullptr, tr("Nome da área"), tr("Nome da área"),
        m_data.title, &okName, QString(), false, /*allowEmpty=*/true);
    if (okName) {
        emit gestureStarted();
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
        bool okName = false;
        const QString name = Sheets::askText(nullptr, tr("Nome da área"), tr("Nome da área"),
            m_data.title, &okName, QString(), false, /*allowEmpty=*/true);
        if (okName) { emit gestureStarted(); m_data.title = name.trimmed(); update(); emitData(); }
    });
    menu.addAction(tr("Cor..."), this, [this]() {
        QColor nc = ColorPopover::getColor(m_data.color, nullptr, tr("Cor da área"));
        if (nc.isValid()) { emit gestureStarted(); m_data.color = nc; update(); emitData(); }
    });
    menu.addSeparator();
    menu.addAction(tr("Exportar área para gaveta"), this, [this]() { emit exportRequested(m_data.id); });
    menu.addSeparator();
    menu.addAction(tr("Remover área"), this, [this]() { emit removeRequested(m_data.id); });
    menu.exec(e->screenPos());
}
