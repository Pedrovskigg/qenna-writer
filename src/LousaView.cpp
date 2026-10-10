#include "LousaView.h"
#include "LousaScene.h"
#include "CardItem.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QKeyEvent>
#include <QMimeData>
#include <QUrl>
#include <QVariantAnimation>
#include <QMouseEvent>
#include <QPen>
#include <QScrollBar>
#include <QTabletEvent>
#include <QTimer>
#include <QWheelEvent>
#include <cmath>

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
    viewport()->setAttribute(Qt::WA_TabletTracking, true);

    // Segurar a caneta parada no fim do traço: vira forma perfeita.
    m_inkHold = new QTimer(this);
    m_inkHold->setSingleShot(true);
    m_inkHold->setInterval(520);
    connect(m_inkHold, &QTimer::timeout, this, [this]() { inkSnapNow(); });
}

// ── Caneta ───────────────────────────────────────────────────────────────────

void LousaView::setInkMode(bool on)
{
    if (m_inkMode == on) return;
    if (m_inkDrawing) inkEnd();
    m_inkMode = on;
    // Riscando, os cards não reagem ao mouse: o traço passa por cima de tudo.
    setInteractive(!on);
    viewport()->setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

void LousaView::setInkTool(const QString& tool, const QColor& color, qreal size)
{
    m_inkTool  = tool;
    m_inkColor = color;
    m_inkSize  = size;
}

QPointF LousaView::toScene(const QPointF& viewPos) const
{
    return viewportTransform().inverted().map(viewPos);
}

qreal LousaView::mousePressure(const QPointF& viewPos)
{
    // Mouse não tem pressão: rápido afina, devagar engrossa.
    const qreal d = QLineF(viewPos, m_inkLastView).length();
    const qreal target = qBound(0.25, 1.0 - d / 38.0, 0.95);
    m_inkMouseP = m_inkMouseP * 0.7 + target * 0.3;
    return m_inkMouseP;
}

void LousaView::inkBegin(const QPointF& viewPos, qreal pressure, bool eraser, bool straight)
{
    m_inkDrawing  = true;
    m_inkErasing  = eraser || m_inkTool == QStringLiteral("eraser");
    m_inkStraight = straight;
    m_inkSnapped  = false;
    m_inkLastView = viewPos;
    m_inkMouseP   = 0.55;
    const QPointF sp = toScene(viewPos);
    m_inkRaw = sp;
    if (m_inkErasing) {
        emit inkEraseStarted();
        if (auto* sc = qobject_cast<LousaScene*>(scene())) {
            const QString id = sc->inkAt(sp, 8.0 / m_zoom);
            if (!id.isEmpty()) emit inkErased(id);
        }
        return;
    }
    m_inkStabStrength = LousaInk::stabilizerSetting();
    m_inkStab.reset(sp);
    inkShowString(m_inkStabStrength > 0 && !straight);
    m_ink = CanvasInk();
    m_ink.tool  = m_inkTool;
    m_ink.color = m_inkColor;
    m_ink.size  = m_inkSize;
    m_ink.pts.append({ float(sp.x()), float(sp.y()), float(pressure) });
    if (!m_inkPreview) {
        m_inkPreview = new InkItem(m_ink);
        m_inkPreview->setPreview(true);
        scene()->addItem(m_inkPreview);
    } else {
        m_inkPreview->setInkData(m_ink);
    }
    m_inkHold->start();
}

void LousaView::inkMove(const QPointF& viewPos, qreal pressure)
{
    if (!m_inkDrawing) return;
    const QPointF sp = toScene(viewPos);
    m_inkLastView = viewPos;
    if (m_inkErasing) {
        if (auto* sc = qobject_cast<LousaScene*>(scene())) {
            const QString id = sc->inkAt(sp, 8.0 / m_zoom);
            if (!id.isEmpty()) emit inkErased(id);
        }
        return;
    }
    if (m_inkSnapped) return;
    m_inkRaw = sp;
    if (m_inkStraight) {
        const InkPoint a = m_ink.pts.first();
        m_ink.pts = { a, { float(sp.x()), float(sp.y()), float(pressure) } };
    } else {
        const QPointF pt = m_inkStab.feed(sp, m_inkStabStrength, m_zoom);
        if (m_inkString) m_inkString->setLine(QLineF(pt, sp));
        const InkPoint& last = m_ink.pts.last();
        if (std::hypot(pt.x() - last.x, pt.y() - last.y) * m_zoom < 1.2) return;
        m_ink.pts.append({ float(pt.x()), float(pt.y()), float(pressure) });
    }
    if (m_inkPreview) m_inkPreview->setInkData(m_ink);
    m_inkHold->start();
}

void LousaView::inkShowString(bool on)
{
    if (!on) {
        if (m_inkString) { scene()->removeItem(m_inkString); delete m_inkString; m_inkString = nullptr; }
        return;
    }
    if (!m_inkString) {
        m_inkString = new QGraphicsLineItem();
        QColor c = m_inkColor;
        c.setAlphaF(0.55);
        QPen pen(c, 1.2, Qt::DashLine);
        pen.setCosmetic(true);
        pen.setDashPattern({ 3, 3 });
        m_inkString->setPen(pen);
        m_inkString->setZValue(InkItem::kZ + 1);
        scene()->addItem(m_inkString);
    }
    m_inkString->setLine(QLineF(m_inkRaw, m_inkRaw));
}

void LousaView::inkSnapNow()
{
    if (!m_inkDrawing || m_inkErasing || m_inkSnapped || m_inkStraight) return;
    if (m_ink.tool == QStringLiteral("highlight")) return;   // marca-texto segue a mão
    if (!LousaInk::snapShape(m_ink.pts)) return;
    m_inkSnapped = true;
    inkShowString(false);
    if (m_inkPreview) m_inkPreview->setInkData(m_ink);
}

void LousaView::inkEnd()
{
    if (!m_inkDrawing) return;
    m_inkDrawing = false;
    m_inkHold->stop();
    inkShowString(false);
    if (m_inkErasing) { m_inkErasing = false; emit inkEraseFinished(); return; }
    // O traço alcança onde a caneta parou (o estabilizador deixa ele pra trás).
    if (!m_inkSnapped && !m_inkStraight && m_inkStabStrength > 0 && !m_ink.pts.isEmpty()) {
        const InkPoint last = m_ink.pts.last();
        if (std::hypot(m_inkRaw.x() - last.x, m_inkRaw.y() - last.y) > 0.5)
            m_ink.pts.append({ float(m_inkRaw.x()), float(m_inkRaw.y()), last.p });
    }
    if (m_inkPreview) {
        scene()->removeItem(m_inkPreview);
        delete m_inkPreview;
        m_inkPreview = nullptr;
    }
    if (!m_ink.pts.isEmpty()) emit inkStrokeFinished(m_ink);
    m_ink = CanvasInk();
}

bool LousaView::viewportEvent(QEvent* event)
{
    if (m_inkMode) {
        switch (event->type()) {
        case QEvent::TabletPress: {
            auto* t = static_cast<QTabletEvent*>(event);
            if (t->button() == Qt::LeftButton || t->buttons() & Qt::LeftButton) {
                m_tabletDown = true;
                const bool eraser = t->pointerType() == QPointingDevice::PointerType::Eraser;
                inkBegin(t->position(), qBound(0.0, t->pressure(), 1.0), eraser,
                         t->modifiers() & Qt::ShiftModifier);
            }
            event->accept();
            return true;
        }
        case QEvent::TabletMove: {
            auto* t = static_cast<QTabletEvent*>(event);
            if (m_tabletDown) inkMove(t->position(), qBound(0.0, t->pressure(), 1.0));
            event->accept();
            return true;
        }
        case QEvent::TabletRelease:
            if (m_tabletDown) { m_tabletDown = false; inkEnd(); }
            event->accept();
            return true;
        default:
            break;
        }
    }
    return QGraphicsView::viewportEvent(event);
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
    // Caneta: o botão esquerdo risca (a caneta da mesa chega como tablet).
    if (m_inkMode && event->button() == Qt::LeftButton) {
        if (!m_tabletDown) inkBegin(event->position(), 0.55, false, event->modifiers() & Qt::ShiftModifier);
        event->accept();
        return;
    }
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
    if (m_inkMode && m_inkDrawing && !m_tabletDown) {
        inkMove(event->position(), mousePressure(event->position()));
        event->accept();
        return;
    }
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
    if (m_inkMode && event->button() == Qt::LeftButton) {
        if (!m_tabletDown) inkEnd();
        event->accept();
        return;
    }
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
