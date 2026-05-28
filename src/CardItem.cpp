#include "CardItem.h"

#include <QColorDialog>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QStyleOption>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QTextDocument>
#include <QTextOption>

namespace {

const QColor kNotePalette[] = {
    QColor(QStringLiteral("#ffd060")), QColor(QStringLiteral("#ffb347")),
    QColor(QStringLiteral("#ff8fab")), QColor(QStringLiteral("#ff6b6b")),
    QColor(QStringLiteral("#a8e6cf")), QColor(QStringLiteral("#87ceeb")),
    QColor(QStringLiteral("#c9b1ff")), QColor(QStringLiteral("#f8f8f8")),
};

// Subclasse com boundingRect fixo — garante que toda a área do card captura
// cliques para edição, independente do tamanho real do texto.
class BodyTextItem : public QGraphicsTextItem {
public:
    qreal bodyW = 180;
    qreal bodyH = 100;

    explicit BodyTextItem(QGraphicsItem* p) : QGraphicsTextItem(p) {}

    QRectF boundingRect() const override {
        QRectF r = QGraphicsTextItem::boundingRect();
        r.setWidth(qMax(r.width(),  bodyW));
        r.setHeight(qMax(r.height(), bodyH));
        return r;
    }
    QPainterPath shape() const override {
        QPainterPath p;
        p.addRect(boundingRect());
        return p;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override {
        QStyleOptionGraphicsItem opt = *option;
        opt.state &= ~QStyle::State_Selected;
        opt.state &= ~QStyle::State_HasFocus;
        QGraphicsTextItem::paint(painter, &opt, widget);
    }
};

bool calcIsDark(const QColor& c)
{
    return (c.red() * 299 + c.green() * 587 + c.blue() * 114) / 1000 < 128;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

CardItem::CardItem(const CanvasCard& data, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_data(data)
{
    setFlag(ItemIsMovable, false);
    setFlag(ItemSendsGeometryChanges);
    setFlag(ItemIsFocusable);
    setAcceptHoverEvents(true);
    setPos(m_data.x, m_data.y);

    auto* bti  = new BodyTextItem(this);
    m_textItem = bti;
    m_textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
    m_textItem->setAcceptHoverEvents(false); // hover fica no CardItem
    m_textItem->document()->setPlainText(m_data.content);

    QTextOption opt;
    opt.setAlignment(Qt::AlignLeft);
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_textItem->document()->setDefaultTextOption(opt);
    m_textItem->document()->setDocumentMargin(0);

    updateTextItem();
    applyTextColor();

    connect(m_textItem->document(), &QTextDocument::contentsChanged, this, [this]() {
        m_data.content = m_textItem->document()->toPlainText();
        emit dataChanged(m_data);
    });
}

CanvasCard CardItem::cardData() const
{
    CanvasCard d = m_data;
    const QPointF p = pos();
    d.x = p.x();
    d.y = p.y();
    return d;
}

void CardItem::syncFromData()
{
    setPos(m_data.x, m_data.y);
    prepareGeometryChange();
    updateTextItem();
    applyTextColor();
    update();
}

// ── Geometria ──────────────────────────────────────────────────────────────

QRectF CardItem::boundingRect() const
{
    return QRectF(-kShadow, -kShadow,
                  m_data.width  + kShadow * 2,
                  m_data.height + kShadow * 2);
}

QPainterPath CardItem::shape() const
{
    QPainterPath p;
    p.addRoundedRect(QRectF(0, 0, m_data.width, m_data.height), kRadius, kRadius);
    return p;
}

void CardItem::updateTextItem()
{
    if (!m_textItem) return;
    constexpr qreal padL = 10.0, padR = 10.0, padTop = 4.0, padBot = 21.0;
    const qreal tw = qMax(10.0, m_data.width  - padL - padR);
    const qreal th = qMax(10.0, m_data.height - kHeaderH - padTop - padBot);
    if (auto* bti = static_cast<BodyTextItem*>(m_textItem)) {
        bti->bodyW = tw;
        bti->bodyH = th;
    }
    m_textItem->setTextWidth(tw);
    m_textItem->setPos(padL, kHeaderH + padTop);
}

// ── Cores ──────────────────────────────────────────────────────────────────

bool   CardItem::isDark() const { return calcIsDark(m_data.color); }

QColor CardItem::contrastColor() const
{
    return isDark() ? QColor(255, 255, 255, 220) : QColor(0, 0, 0, 180);
}

void CardItem::applyTextColor()
{
    if (!m_textItem) return;
    m_textItem->setDefaultTextColor(contrastColor());
}

// ── Pintura ────────────────────────────────────────────────────────────────

void CardItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    const qreal w = m_data.width;
    const qreal h = m_data.height;
    const QColor& bg = m_data.color;

    p->setRenderHint(QPainter::Antialiasing);

    // Sombra
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 40));
    p->drawRoundedRect(QRectF(3, 5, w, h), kRadius, kRadius);

    // Fundo
    p->setBrush(bg);
    p->drawRoundedRect(QRectF(0, 0, w, h), kRadius, kRadius);

    // Separador do header
    const QColor sep = isDark() ? QColor(255,255,255,30) : QColor(0,0,0,18);
    p->setPen(QPen(sep, 1));
    p->drawLine(QPointF(1, kHeaderH), QPointF(w - 1, kHeaderH));

    // Dobra de canto
    const qreal f = kFoldSize;
    p->setPen(Qt::NoPen);
    p->setBrush(isDark() ? QColor(0,0,0,70) : QColor(0,0,0,36));
    p->drawPolygon(QPolygonF() << QPointF(w-f,h) << QPointF(w,h) << QPointF(w,h-f));
    p->setBrush(isDark() ? QColor(255,255,255,18) : QColor(255,255,255,60));
    p->drawPolygon(QPolygonF() << QPointF(w-f,h) << QPointF(w,h-f) << QPointF(w-f,h-f));
    p->setPen(QPen(isDark() ? QColor(255,255,255,30) : QColor(0,0,0,30), 0.8));
    p->drawLine(QPointF(w-f, h), QPointF(w, h-f));

    // Header: grip (6 pontos)
    const QColor tc    = contrastColor();
    const QColor muted = QColor(tc.red(), tc.green(), tc.blue(), 90);
    p->setPen(Qt::NoPen);
    p->setBrush(muted);
    const qreal gx = 8.0, gy = (kHeaderH - 9.0) / 2.0;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
            p->drawEllipse(QPointF(gx + col*4.5, gy + row*4.5), 1.2, 1.2);

    // Botão ×
    const QColor xCol = m_hoverDelete ? QColor(220,60,60) : muted;
    p->setPen(QPen(xCol, 1.4, Qt::SolidLine, Qt::RoundCap));
    const qreal xx = w-13.0, xy = kHeaderH/2.0, xr = 4.0;
    p->drawLine(QPointF(xx-xr,xy-xr), QPointF(xx+xr,xy+xr));
    p->drawLine(QPointF(xx+xr,xy-xr), QPointF(xx-xr,xy+xr));

    // Placeholder quando vazio
    if (m_textItem && m_textItem->document()->toPlainText().isEmpty()) {
        constexpr qreal padL = 10.0, padTop = 4.0;
        const QColor ph(tc.red(), tc.green(), tc.blue(), 80);
        p->setPen(ph);
        p->setFont(QFont(QStringLiteral("Segoe UI"), 12));
        const QRectF phRect(padL, kHeaderH + padTop,
                            w - 2*padL, h - kHeaderH - padTop - kFoldSize);
        p->drawText(phRect, Qt::AlignLeft | Qt::AlignTop, tr("Escreva aqui..."));
    }

    // Resize handle (3 linhas diagonais — igual Mira 1)
    const qreal opacity = isDark() ? 0.35 : 0.25;
    const QColor rhCol(tc.red(), tc.green(), tc.blue(),
                       int((m_hoverResize ? 0.7 : opacity) * 255));
    p->setPen(QPen(rhCol, 1.5, Qt::SolidLine, Qt::RoundCap));
    const qreal ox = w-3-10, oy = h-3-10;
    p->drawLine(QPointF(ox+2,oy+9), QPointF(ox+9,oy+2));
    p->drawLine(QPointF(ox+5,oy+9), QPointF(ox+9,oy+5));
    p->drawLine(QPointF(ox+8,oy+9), QPointF(ox+9,oy+8));
}

// ── Hit-test ────────────────────────────────────────────────────────────────

bool CardItem::isOnDeleteBtn(const QPointF& p) const
{
    return QRectF(m_data.width-21, 0, 21, kHeaderH).contains(p);
}

bool CardItem::isOnResizeZone(const QPointF& p) const
{
    return QRectF(m_data.width-17, m_data.height-17, 17, 17).contains(p);
}

// ── Mouse events ────────────────────────────────────────────────────────────

void CardItem::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { e->ignore(); return; }

    if (isOnDeleteBtn(e->pos())) {
        emit deleteRequested(m_data.id);
        e->accept();
        return;
    }
    if (isOnResizeZone(e->pos())) {
        m_resizing   = true;
        m_pressScene = e->scenePos();
        m_pressSize  = QSizeF(m_data.width, m_data.height);
        e->accept();
        return;
    }
    // Drag pelo header
    if (e->pos().y() < kHeaderH) {
        m_dragging        = true;
        m_pressScene      = e->scenePos();
        m_pressItemOrigin = pos();
        setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    e->ignore();
}

void CardItem::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
    if (m_dragging) {
        setPos(m_pressItemOrigin + (e->scenePos() - m_pressScene));
        e->accept();
        return;
    }
    if (m_resizing) {
        const QPointF d = e->scenePos() - m_pressScene;
        prepareGeometryChange();
        m_data.width  = qMax(kMinW, m_pressSize.width()  + d.x());
        m_data.height = qMax(kMinH, m_pressSize.height() + d.y());
        updateTextItem();
        update();
        e->accept();
        return;
    }
    e->ignore();
}

void CardItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
    if (m_dragging || m_resizing) {
        m_dragging = m_resizing = false;
        setCursor(Qt::ArrowCursor);
        const QPointF p = pos();
        m_data.x = p.x(); m_data.y = p.y();
        emit dataChanged(m_data);
        e->accept();
        return;
    }
    e->ignore();
}

// ── Hover ────────────────────────────────────────────────────────────────────

void CardItem::hoverMoveEvent(QGraphicsSceneHoverEvent* e)
{
    const QPointF lp = e->pos();
    const bool onDel    = isOnDeleteBtn(lp);
    const bool onResize = isOnResizeZone(lp);
    const bool onHeader = lp.y() < kHeaderH;

    if (onDel != m_hoverDelete || onResize != m_hoverResize) {
        m_hoverDelete = onDel;
        m_hoverResize = onResize;
        update();
    }
    if (onDel)         setCursor(Qt::PointingHandCursor);
    else if (onResize) setCursor(Qt::SizeFDiagCursor);
    else if (onHeader) setCursor(Qt::OpenHandCursor);
    else               setCursor(Qt::ArrowCursor);
}

void CardItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    if (m_hoverDelete || m_hoverResize) { m_hoverDelete = m_hoverResize = false; update(); }
    setCursor(Qt::ArrowCursor);
}

// ── Context menu ─────────────────────────────────────────────────────────────

void CardItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* e)
{
    QMenu menu;
    for (const QColor& c : kNotePalette) {
        QPixmap px(14, 14); px.fill(c);
        auto* act = menu.addAction(QIcon(px), QString());
        act->setData(c.name());
    }
    menu.addSeparator();
    auto* custom = menu.addAction(tr("Cor personalizada..."));

    QAction* chosen = menu.exec(e->screenPos());
    if (!chosen) return;
    QColor nc = (chosen == custom)
        ? QColorDialog::getColor(m_data.color, nullptr, tr("Cor do card"))
        : QColor(chosen->data().toString());
    if (!nc.isValid()) return;
    prepareGeometryChange();
    m_data.color = nc;
    applyTextColor();
    update();
    emit dataChanged(m_data);
}
