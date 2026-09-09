#include "AnchorUtils.h"

#include <QtGlobal>

namespace AnchorUtils {

Qt::Edge growthEdgeForBarSide(Qt::Edge barSide)
{
    switch (barSide) {
    case Qt::TopEdge:    return Qt::BottomEdge; // popup cresce pra baixo
    case Qt::BottomEdge: return Qt::TopEdge;    // popup cresce pra cima
    case Qt::LeftEdge:   return Qt::RightEdge;  // popup cresce pra direita
    case Qt::RightEdge:  return Qt::LeftEdge;   // popup cresce pra esquerda
    }
    return Qt::BottomEdge;
}

QPoint positionNear(const QRect& anchorGlobal, const QSize& popupSize,
                     Qt::Edge barSide, const QRect& screenAvail, int gap)
{
    const Qt::Edge growth = growthEdgeForBarSide(barSide);
    QPoint pos;

    if (growth == Qt::BottomEdge || growth == Qt::TopEdge) {
        // Cresce verticalmente — eixo perpendicular (X) é clampado, eixo de
        // crescimento (Y) inverte de lado se não couber.
        pos.setX(anchorGlobal.left());
        if (pos.x() + popupSize.width() > screenAvail.right())
            pos.setX(anchorGlobal.right() - popupSize.width());
        if (pos.x() < screenAvail.left())
            pos.setX(screenAvail.left() + 4);

        if (growth == Qt::BottomEdge) {
            pos.setY(anchorGlobal.bottom() + gap);
            if (pos.y() + popupSize.height() > screenAvail.bottom()) {
                const int flipped = anchorGlobal.top() - gap - popupSize.height();
                pos.setY(flipped >= screenAvail.top()
                             ? flipped
                             : qMax(screenAvail.top(), screenAvail.bottom() - popupSize.height()));
            }
        } else {
            pos.setY(anchorGlobal.top() - gap - popupSize.height());
            if (pos.y() < screenAvail.top()) {
                const int flipped = anchorGlobal.bottom() + gap;
                pos.setY(flipped + popupSize.height() <= screenAvail.bottom()
                             ? flipped
                             : qMax(screenAvail.top(), screenAvail.bottom() - popupSize.height()));
            }
        }
    } else {
        // Cresce horizontalmente (Left/Right) — eixo perpendicular (Y) é
        // clampado, eixo de crescimento (X) inverte de lado se não couber.
        pos.setY(anchorGlobal.top());
        if (pos.y() + popupSize.height() > screenAvail.bottom())
            pos.setY(anchorGlobal.bottom() - popupSize.height());
        if (pos.y() < screenAvail.top())
            pos.setY(screenAvail.top() + 4);

        if (growth == Qt::RightEdge) {
            pos.setX(anchorGlobal.right() + gap);
            if (pos.x() + popupSize.width() > screenAvail.right()) {
                const int flipped = anchorGlobal.left() - gap - popupSize.width();
                pos.setX(flipped >= screenAvail.left()
                             ? flipped
                             : qMax(screenAvail.left(), screenAvail.right() - popupSize.width()));
            }
        } else {
            pos.setX(anchorGlobal.left() - gap - popupSize.width());
            if (pos.x() < screenAvail.left()) {
                const int flipped = anchorGlobal.right() + gap;
                pos.setX(flipped + popupSize.width() <= screenAvail.right()
                             ? flipped
                             : qMax(screenAvail.left(), screenAvail.right() - popupSize.width()));
            }
        }
    }

    return pos;
}

} // namespace AnchorUtils
