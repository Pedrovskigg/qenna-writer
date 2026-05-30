#include "TimelineEventItem.h"

#include <QAction>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QLineF>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr qreal kLabelH    = 24.0;
constexpr qreal kLabelGap  = 8.0;
constexpr qreal kCardGap   = 12.0;
constexpr qreal kCardPad   = 12.0;
constexpr qreal kDragSlop  = 4.0;

QColor cardBg()     { return QColor(28, 28, 32, 246); }
QColor cardBody()   { return QColor(232, 232, 234); }
QColor cardMuted()  { return QColor(158, 158, 164); }
} // namespace

// ─────────────────────────────────────────────────────────────────────────────

TimelineEventItem::TimelineEventItem(const TimelineEvent& data,
                                     QGraphicsItem* parent)
    : QGraphicsObject(parent), m_data(data)
{
    setPos(data.x, data.y);
    setFlags(QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
    setZValue(1.0);
}

// ── Getters / setters ────────────────────────────────────────────────────────

void TimelineEventItem::setEventData(const TimelineEvent& d)
{
    prepareGeometryChange();
    m_data = d;
    m_scrollOffset = 0.0;
    setPos(d.x, d.y);
    update();
}

void TimelineEventItem::setTimelineColor(const QColor& c)
{
    m_timelineColor = c;
    update();
}

void TimelineEventItem::setOpen(bool open)
{
    if (m_open == open) return;
    prepareGeometryChange();
    m_open = open;
    if (!m_open) m_scrollOffset = 0.0;
    setZValue(m_open ? 50.0 : 1.0);
    update();
    emit geometryChanged(m_data.id);
}

QColor TimelineEventItem::effectiveColor() const
{
    return m_data.color.isValid() ? m_data.color : m_timelineColor;
}

QString TimelineEventItem::labelText() const
{
    QString t = m_data.title.isEmpty() ? tr("Evento") : m_data.title;
    if (!m_data.timeMarker.isEmpty())
        t += QStringLiteral("   ·   ") + m_data.timeMarker;
    return t;
}

QRectF TimelineEventItem::labelRect() const
{
    QFont f; f.setPointSizeF(8.5);
    const QFontMetrics fm(f);
    const qreal w = qMin(360.0, qreal(fm.horizontalAdvance(labelText())) + 22.0);
    return QRectF(-w / 2.0,
                  -(dotRadius() + kLabelGap + kLabelH),
                  w, kLabelH);
}

qreal TimelineEventItem::descContentH() const
{
    if (m_data.description.isEmpty()) return 0.0;
    QFont f; f.setPointSizeF(8.5);
    const QFontMetrics fm(f);
    return fm.boundingRect(QRect(0, 0, qRound(kCardW - kCardPad * 2), 100000),
                           Qt::TextWordWrap, m_data.description).height();
}

QRectF TimelineEventItem::cardRect() const
{
    QFont ft; ft.setPointSizeF(9.5); ft.setBold(true);
    const QFontMetrics fmT(ft);
    const int titleH = fmT.boundingRect(QRect(0, 0, qRound(kCardW - kCardPad * 2), 100000),
                                        Qt::TextWordWrap,
                                        m_data.title.isEmpty() ? tr("Evento") : m_data.title)
                          .height();
    const qreal headerH = kCardPad + titleH
                        + (m_data.timeMarker.isEmpty() ? 0.0 : 16.0)
                        + 10.0; // até o separador
    const qreal descH = qMax(20.0, descContentH());
    qreal h = headerH + 6.0 + descH + kCardPad;
    h = qBound(70.0, h, kCardHMax);
    return QRectF(-kCardW / 2.0, dotRadius() + kCardGap, kCardW, h);
}

qreal TimelineEventItem::descVisH() const
{
    QFont ft; ft.setPointSizeF(9.5); ft.setBold(true);
    const QFontMetrics fmT(ft);
    const int titleH = fmT.boundingRect(QRect(0, 0, qRound(kCardW - kCardPad * 2), 100000),
                                        Qt::TextWordWrap,
                                        m_data.title.isEmpty() ? tr("Evento") : m_data.title)
                          .height();
    const qreal headerH = kCardPad + titleH
                        + (m_data.timeMarker.isEmpty() ? 0.0 : 16.0)
                        + 10.0;
    return qMax(0.0, cardRect().height() - headerH - 6.0 - kCardPad);
}

bool TimelineEventItem::wheelScroll(int angleDeltaY)
{
    if (!m_open) return false;
    const qreal visH = descVisH();
    const qreal maxScroll = qMax(0.0, descContentH() - visH);
    if (maxScroll < 1.0) return false;
    const qreal delta = (angleDeltaY / 120.0) * 36.0;
    m_scrollOffset = qBound(0.0, m_scrollOffset - delta, maxScroll);
    update();
    return true;
}

void TimelineEventItem::paintScrollbar(QPainter* p, const QRectF& card) const
{
    const qreal visH = descVisH();
    const qreal contH = descContentH();
    if (contH <= visH + 1.0 || visH <= 0.0) return;

    const qreal maxScroll = contH - visH;
    const qreal thumbH = qMax(16.0, visH * visH / contH);
    const qreal trackX = card.right() - 6.0;
    const qreal trackTop = card.bottom() - kCardPad - visH;
    const qreal thumbY = trackTop + (maxScroll > 0.0 ? (m_scrollOffset / maxScroll) : 0.0)
                                      * (visH - thumbH);
    p->save();
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(255, 255, 255, 26));
    p->drawRoundedRect(QRectF(trackX, trackTop, 3.0, visH), 1.5, 1.5);
    p->setBrush(QColor(255, 255, 255, 120));
    p->drawRoundedRect(QRectF(trackX, thumbY, 3.0, thumbH), 1.5, 1.5);
    p->restore();
}

// ── Bounding / shape ─────────────────────────────────────────────────────────

QRectF TimelineEventItem::boundingRect() const
{
    QRectF r(-dotRadius() - 3, -dotRadius() - 3,
             dotRadius() * 2 + 6, dotRadius() * 2 + 6);
    r = r.united(labelRect().adjusted(-2, -2, 2, 2)); // rótulo sempre reservado
    if (m_open)
        r = r.united(cardRect().adjusted(-4, -4, 4, 8)); // sombra do card
    return r;
}

QPainterPath TimelineEventItem::shape() const
{
    QPainterPath p;
    p.addEllipse(QPointF(0, 0), dotRadius() + 3, dotRadius() + 3);
    if (m_open)
        p.addRoundedRect(cardRect(), 10, 10);
    return p;
}

// ── Paint ────────────────────────────────────────────────────────────────────

void TimelineEventItem::paint(QPainter* p,
                              const QStyleOptionGraphicsItem*,
                              QWidget*)
{
    p->setRenderHint(QPainter::Antialiasing);

    const QColor fill = effectiveColor();
    const qreal  r    = dotRadius();

    // ── Halo de hover / seleção ──────────────────────────────────────────────
    if (m_hover || isSelected() || m_open) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(fill.red(), fill.green(), fill.blue(), 60));
        p->drawEllipse(QPointF(0, 0), r + 6, r + 6);
    }

    // ── Ponto ─────────────────────────────────────────────────────────────────
    if (m_data.autoEvent) {
        // evento de presença: anel fino, miolo translúcido
        p->setBrush(QColor(fill.red(), fill.green(), fill.blue(), 120));
        p->setPen(QPen(fill, 1.4));
    } else {
        p->setBrush(fill);
        p->setPen(QPen(fill.darker(150), 1.4));
    }
    p->drawEllipse(QPointF(0, 0), r, r);

    // brilho interno discreto
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(255, 255, 255, m_data.autoEvent ? 40 : 70));
    p->drawEllipse(QPointF(-r * 0.28, -r * 0.28), r * 0.45, r * 0.45);

    // ── Rótulo de hover (acima do ponto) ──────────────────────────────────────
    if (m_hover && !m_open) {
        const QRectF lr = labelRect();
        QPainterPath pill; pill.addRoundedRect(lr, kLabelH / 2.0, kLabelH / 2.0);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(20, 20, 24, 235));
        p->drawPath(pill);
        // barrinha de acento à esquerda
        p->setBrush(fill);
        p->drawEllipse(QPointF(lr.left() + kLabelH / 2.0, lr.center().y()), 3.0, 3.0);

        QFont f; f.setPointSizeF(8.5);
        p->setFont(f);
        p->setPen(QColor(238, 238, 240));
        p->drawText(lr.adjusted(kLabelH * 0.72, 0, -10, 0),
                    Qt::AlignVCenter | Qt::AlignLeft,
                    p->fontMetrics().elidedText(labelText(), Qt::ElideRight,
                                                qRound(lr.width() - kLabelH * 0.72 - 10)));
    }

    // ── Popover (abaixo do ponto) ──────────────────────────────────────────────
    if (m_open) {
        const QRectF card = cardRect();

        // sombra
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 70));
        p->drawRoundedRect(card.adjusted(2, 4, 2, 4), 10, 10);

        // corpo
        p->setBrush(cardBg());
        p->setPen(QPen(fill, 1.5));
        p->drawRoundedRect(card, 10, 10);

        // "biquinho" ligando ao ponto
        QPainterPath beak;
        beak.moveTo(-6, card.top());
        beak.lineTo(0, card.top() - 6);
        beak.lineTo(6, card.top());
        beak.closeSubpath();
        p->setPen(Qt::NoPen);
        p->setBrush(cardBg());
        p->drawPath(beak);

        const qreal innerW = kCardW - kCardPad * 2;
        qreal y = card.top() + kCardPad;

        // título
        QFont ft; ft.setPointSizeF(9.5); ft.setBold(true);
        p->setFont(ft);
        p->setPen(fill.lighter(150));
        const QString title = m_data.title.isEmpty() ? tr("Evento") : m_data.title;
        const QRectF titleR(card.left() + kCardPad, y, innerW,
                            p->fontMetrics().boundingRect(
                                QRect(0, 0, qRound(innerW), 100000),
                                Qt::TextWordWrap, title).height());
        p->drawText(titleR, Qt::TextWordWrap | Qt::AlignLeft, title);
        y = titleR.bottom();

        // marcação temporal
        if (!m_data.timeMarker.isEmpty()) {
            QFont fm; fm.setPointSizeF(7.5);
            p->setFont(fm);
            p->setPen(cardMuted());
            p->drawText(QRectF(card.left() + kCardPad, y + 2, innerW, 14),
                        Qt::AlignLeft | Qt::AlignVCenter, m_data.timeMarker);
            y += 16;
        }

        // separador
        y += 8;
        p->setPen(QColor(255, 255, 255, 30));
        p->drawLine(QPointF(card.left() + kCardPad, y),
                    QPointF(card.right() - kCardPad, y));

        // descrição (com scroll)
        const qreal descTop = y + 4;
        const qreal visH = card.bottom() - kCardPad - descTop;
        if (visH > 4.0) {
            QFont fd; fd.setPointSizeF(8.5);
            p->setFont(fd);
            const QRectF vis(card.left() + kCardPad, descTop, innerW, visH);
            p->setClipRect(vis);
            if (m_data.description.isEmpty()) {
                p->setPen(cardMuted());
                p->drawText(vis, Qt::AlignLeft | Qt::AlignTop, tr("(sem descrição)"));
            } else {
                p->setPen(cardBody());
                p->drawText(QRectF(vis.left(), descTop - m_scrollOffset, innerW, 100000),
                            Qt::TextWordWrap | Qt::AlignLeft, m_data.description);
            }
            p->setClipping(false);
            paintScrollbar(p, card);
        }
    }
}

// ── Eventos de mouse ─────────────────────────────────────────────────────────

void TimelineEventItem::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressScenePos = e->scenePos();
        m_dragMoved = false;
    }
    QGraphicsObject::mousePressEvent(e);
}

void TimelineEventItem::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
    if ((e->scenePos() - m_pressScenePos).manhattanLength() > kDragSlop)
        m_dragMoved = true;
    QGraphicsObject::mouseMoveEvent(e);
}

void TimelineEventItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
    QGraphicsObject::mouseReleaseEvent(e);
    if (e->button() != Qt::LeftButton) return;

    if (m_dragMoved) {
        emit movedByUser(m_data.id);
        return;
    }
    // clique sem arrastar: só alterna o popover se foi no ponto (não no card)
    if (QLineF(e->pos(), QPointF(0, 0)).length() <= dotRadius() + kDragSlop) {
        setOpen(!m_open);
        emit openToggled(m_data.id, m_open);
    }
}

void TimelineEventItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)
{
    if (e->button() == Qt::LeftButton
        && QLineF(e->pos(), QPointF(0, 0)).length() <= dotRadius() + kDragSlop) {
        emit editRequested(m_data.id);
        e->accept();
        return;
    }
    QGraphicsObject::mouseDoubleClickEvent(e);
}

void TimelineEventItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    m_hover = true;
    setZValue(m_open ? 50.0 : 10.0);
    update();
}

void TimelineEventItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    m_hover = false;
    setZValue(m_open ? 50.0 : 1.0);
    update();
}

void TimelineEventItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* e)
{
    QMenu menu;
    auto* actEdit   = menu.addAction(tr("Editar evento"));
    menu.addSeparator();
    auto* actExport = menu.addAction(tr("Exportar como documento"));
    menu.addSeparator();
    auto* actDelete = menu.addAction(tr("Remover"));

    const QAction* chosen = menu.exec(e->screenPos());
    if (chosen == actEdit)   emit editRequested(m_data.id);
    if (chosen == actExport) emit exportAsDocRequested(m_data);
    if (chosen == actDelete) emit deleteRequested(m_data.id);
    e->accept();
}

QVariant TimelineEventItem::itemChange(GraphicsItemChange change,
                                       const QVariant& value)
{
    if (change == ItemPositionHasChanged) {
        m_data.x = pos().x();
        m_data.y = pos().y();
        emit positionChanged(m_data.id);
    }
    return QGraphicsObject::itemChange(change, value);
}
