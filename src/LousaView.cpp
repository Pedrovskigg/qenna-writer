#include "LousaView.h"
#include "LousaScene.h"
#include "CardItem.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGraphicsRectItem>
#include <QKeyEvent>
#include <QMimeData>
#include <QUrl>
#include <QVariantAnimation>
#include <QMouseEvent>
#include <QPen>
#include <QScrollBar>
#include <QWheelEvent>

LousaView::LousaView(LousaScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
{
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::NoAnchor);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setCacheMode(QGraphicsView::CacheBackground);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setInteractive(true);
    setMouseTracking(true);
    setAcceptDrops(true);
}

// Tipo do que se arrasta pro quadro ("character:<itemId>", "doc:<itemId>",
// "chapter:<msId>:<chId>", "stash:<índice>", "new:<tipo>").
static const char* kLousaMime = "application/x-qenna-lousa-item";

void LousaView::zoomBy(qreal factor)
{
    const QPointF center = mapToScene(viewport()->rect().center());
    m_zoom = qBound(0.15, m_zoom * factor, 4.0);
    QTransform t;
    t.scale(m_zoom, m_zoom);
    setTransform(t);
    centerOn(center);
    emit zoomChanged(m_zoom);
    emit viewportMoved();
}

void LousaView::centerOnAnimated(const QPointF& scenePos)
{
    const QPointF from = mapToScene(viewport()->rect().center());
    auto* anim = new QVariantAnimation(this);
    anim->setDuration(260);
    anim->setStartValue(from);
    anim->setEndValue(scenePos);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        centerOn(v.toPointF());
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void LousaView::setConnectMode(bool on)
{
    if (m_connectMode == on) return;
    m_connectMode = on;
    m_connectFrom.clear();
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
    if (auto* sc = qobject_cast<LousaScene*>(scene())) sc->clearCardSelection();
    emit connectModeChanged(on);
}

void LousaView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    emit viewportMoved();
}

void LousaView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !itemAt(event->pos()) && !m_planMode) {
        emit backgroundDoubleClicked(mapToScene(event->pos()));
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

// Imagem arrastada do Windows (arquivo) ou de outro programa (imagem crua).
static bool hasDroppableImage(const QMimeData* md)
{
    if (md->hasImage()) return true;
    for (const QUrl& u : md->urls()) {
        if (!u.isLocalFile()) continue;
        const QString s = u.toLocalFile().toLower();
        for (const char* ext : { ".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif" })
            if (s.endsWith(QLatin1String(ext))) return true;
    }
    return false;
}

void LousaView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(QString::fromLatin1(kLousaMime))
        || hasDroppableImage(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragEnterEvent(event);
}

void LousaView::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat(QString::fromLatin1(kLousaMime))
        || hasDroppableImage(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragMoveEvent(event);
}

void LousaView::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasFormat(QString::fromLatin1(kLousaMime))) {
        const QString payload = QString::fromUtf8(event->mimeData()->data(QString::fromLatin1(kLousaMime)));
        emit itemDropped(payload, mapToScene(event->position().toPoint()));
        event->acceptProposedAction();
        return;
    }
    if (hasDroppableImage(event->mimeData())) {
        const QPointF at = mapToScene(event->position().toPoint());
        QList<QImage> images;
        for (const QUrl& u : event->mimeData()->urls()) {
            if (!u.isLocalFile()) continue;
            QImage img(u.toLocalFile());
            if (!img.isNull()) images << img;
        }
        if (images.isEmpty() && event->mimeData()->hasImage())
            images << qvariant_cast<QImage>(event->mimeData()->imageData());
        for (int i = 0; i < images.size(); ++i)
            emit imageDropped(images.at(i), at + QPointF(24.0 * i, 24.0 * i));
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dropEvent(event);
}

void LousaView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_connectMode) {
        setConnectMode(false);
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

QPointF LousaView::scrollPos() const
{
    return {qreal(horizontalScrollBar()->value()),
            qreal(verticalScrollBar()->value())};
}

void LousaView::applyZoomAndPan(qreal zoom, qreal panX, qreal panY)
{
    m_zoom = qBound(0.15, zoom, 4.0);
    QTransform t;
    t.scale(m_zoom, m_zoom);
    setTransform(t);
    horizontalScrollBar()->setValue(qRound(panX));
    verticalScrollBar()->setValue(qRound(panY));
}

void LousaView::fitSceneRect(const QRectF& r)
{
    if (r.width() <= 0 || r.height() <= 0) return;
    constexpr qreal pad = 80.0;
    const QRectF target = r.adjusted(-pad, -pad, pad, pad);
    const qreal sx = viewport()->width()  / target.width();
    const qreal sy = viewport()->height() / target.height();
    m_zoom = qBound(0.15, qMin(sx, sy), 4.0);
    QTransform t;
    t.scale(m_zoom, m_zoom);
    setTransform(t);
    centerOn(r.center());
    emit zoomChanged(m_zoom);
    emit viewportMoved();
}

void LousaView::wheelEvent(QWheelEvent* event)
{
    // Regra do Mira 1: se o cursor está sobre um card com conteúdo rolável,
    // o wheel rola o texto do card; caso contrário, dá zoom no canvas.
    for (QGraphicsItem* it = itemAt(event->position().toPoint()); it; it = it->parentItem()) {
        if (auto* card = dynamic_cast<CardItem*>(it)) {
            if (card->wheelScroll(event->angleDelta().y())) { event->accept(); return; }
            break;
        }
    }

    const qreal step     = event->angleDelta().y() > 0 ? 1.12 : (1.0 / 1.12);
    const qreal newZoom  = qBound(0.15, m_zoom * step, 4.0);
    if (qFuzzyCompare(newZoom, m_zoom)) { event->accept(); return; }

    const QPointF scenePos = mapToScene(event->position().toPoint());
    m_zoom = newZoom;
    QTransform t;
    t.scale(m_zoom, m_zoom);
    setTransform(t);

    // Mantém o ponto sob o cursor estático.
    const QPointF newViewPos = mapFromScene(scenePos);
    const QPointF delta      = event->position() - newViewPos;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - qRound(delta.x()));
    verticalScrollBar()->setValue(verticalScrollBar()->value()   - qRound(delta.y()));

    emit zoomChanged(m_zoom);
    emit viewportMoved();
    event->accept();
}

void LousaView::setPlanMode(bool on)
{
    m_planMode = on;
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
    if (!on && m_planGhost) {
        scene()->removeItem(m_planGhost);
        delete m_planGhost;
        m_planGhost = nullptr;
    }
    m_drawing = false;
}

void LousaView::mousePressEvent(QMouseEvent* event)
{
    // Plan mode: arrastar no canvas vazio cria zona
    if (m_planMode && event->button() == Qt::LeftButton) {
        m_drawing   = true;
        m_drawStart = mapToScene(event->pos());
        if (!m_planGhost) {
            m_planGhost = new QGraphicsRectItem();
            m_planGhost->setZValue(300);
            QPen ghost(QColor(QStringLiteral("#6ea8fe")), 2, Qt::DashLine);
            ghost.setDashPattern({6, 4});
            m_planGhost->setPen(ghost);
            m_planGhost->setBrush(QColor(110, 168, 254, 18));
            scene()->addItem(m_planGhost);
        }
        m_planGhost->setRect(QRectF(m_drawStart, QSizeF(0, 0)));
        event->accept();
        return;
    }

    // Ligar: primeiro card, depois o segundo
    if (m_connectMode && event->button() == Qt::LeftButton) {
        // O adesivo não recebe linha: pega o card que estiver embaixo dele.
        CardItem* card = nullptr;
        for (QGraphicsItem* hit : items(event->pos())) {
            for (QGraphicsItem* it = hit; it && !card; it = it->parentItem()) {
                auto* ci = dynamic_cast<CardItem*>(it);
                if (ci && !ci->isSticker()) card = ci;
            }
            if (card) break;
        }
        if (!card) { setConnectMode(false); event->accept(); return; }
        auto* sc = qobject_cast<LousaScene*>(scene());
        if (m_connectFrom.isEmpty()) {
            m_connectFrom = card->cardData().id;
            if (sc) sc->selectOnlyCard(card);
        } else if (card->cardData().id != m_connectFrom) {
            const QString from = m_connectFrom;
            setConnectMode(false);
            emit connectPicked(from, card->cardData().id);
        }
        event->accept();
        return;
    }

    // Pan com botão do meio, ou arrastar o fundo vazio com botão esquerdo.
    const bool isMiddle = (event->button() == Qt::MiddleButton);
    const bool isBgLeft = (event->button() == Qt::LeftButton) && !itemAt(event->pos());
    if (isBgLeft) {
        if (auto* sc = qobject_cast<LousaScene*>(scene()))
            sc->clearAllSelection();   // clicar no vazio desmarca tudo
    }
    if (isMiddle || isBgLeft) {
        m_panning = true;
        m_panLast = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void LousaView::setBrushMode(bool on)
{
    m_brushMode = on;
    setCursor(on ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void LousaView::mouseMoveEvent(QMouseEvent* event)
{
    // Brush select: enquanto Shift+S está segurado, passar o mouse seleciona cards.
    if (m_brushMode) {
        for (QGraphicsItem* it = itemAt(event->pos()); it; it = it->parentItem()) {
            if (auto* card = dynamic_cast<CardItem*>(it)) {
                if (auto* sc = qobject_cast<LousaScene*>(scene()))
                    sc->addCardToSelection(card);
                break;
            }
        }
    }

    if (m_drawing && m_planGhost) {
        const QPointF cur = mapToScene(event->pos());
        m_planGhost->setRect(QRectF(m_drawStart, cur).normalized());
        event->accept();
        return;
    }
    if (m_panning) {
        const QPoint delta = event->pos() - m_panLast;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value()   - delta.y());
        m_panLast = event->pos();
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void LousaView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_drawing && event->button() == Qt::LeftButton) {
        m_drawing = false;
        const QRectF zone = m_planGhost ? m_planGhost->rect() : QRectF();
        if (m_planGhost) {
            scene()->removeItem(m_planGhost);
            delete m_planGhost;
            m_planGhost = nullptr;
        }
        setPlanMode(false);
        if (zone.width() > 50 && zone.height() > 50)
            emit zoneDrawn(zone);
        event->accept();
        return;
    }
    if (m_panning && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
