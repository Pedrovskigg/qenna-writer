#include "GroupBadge.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace GroupBadge {

void draw(QPainter& p, QPointF c, double r, const QColor& color, bool social)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QColor col = color.isValid() ? color : QColor(150, 150, 150);
    const double k = r / 8.0;   // o desenho foi feito num selo de 16 px
    if (social) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(col, 1.5 * std::max(1.0, k)));
        p.drawEllipse(c, r - 0.75, r - 0.75);
        // duas pessoas: a da frente maior, a de trás à direita
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(QPointF(c.x() - 1.6 * k, c.y() - 1.8 * k), 1.75 * k, 1.75 * k);
        QPainterPath body;
        body.moveTo(c.x() - 4.6 * k, c.y() + 3.6 * k);
        body.cubicTo(c.x() - 4.4 * k, c.y() + 0.6 * k, c.x() + 1.2 * k, c.y() + 0.6 * k, c.x() + 1.4 * k, c.y() + 3.6 * k);
        body.closeSubpath();
        p.drawPath(body);
        QColor back = col; back.setAlphaF(col.alphaF() * 0.65);
        p.setBrush(back);
        p.drawEllipse(QPointF(c.x() + 2.4 * k, c.y() - 2.4 * k), 1.35 * k, 1.35 * k);
        QPainterPath b2;
        b2.moveTo(c.x() + 1.9 * k, c.y() - 0.2 * k);
        b2.cubicTo(c.x() + 3.8 * k, c.y() - 0.6 * k, c.x() + 5.0 * k, c.y() + 0.6 * k, c.x() + 5.0 * k, c.y() + 2.4 * k);
        b2.lineTo(c.x() + 2.6 * k, c.y() + 2.4 * k);
        b2.closeSubpath();
        p.drawPath(b2);
    } else {
        p.setBrush(col);
        p.setPen(QPen(QColor(0, 0, 0, 60), 1));
        p.drawEllipse(c, r, r);
        // o alfinete, na cor que lê em cima da bolinha
        const QColor ink = col.lightnessF() > 0.62 ? QColor(30, 26, 22, 215) : QColor(255, 255, 255, 235);
        QPainterPath pin;
        const double cx = c.x(), cy = c.y() - 1.1 * k;
        pin.moveTo(cx, cy + 5.0 * k);
        pin.cubicTo(cx - 1.6 * k, cy + 2.6 * k, cx - 3.4 * k, cy + 1.2 * k, cx - 3.4 * k, cy - 0.6 * k);
        pin.arcTo(QRectF(cx - 3.4 * k, cy - 4.0 * k, 6.8 * k, 6.8 * k), 180, -180);
        pin.cubicTo(cx + 3.4 * k, cy + 1.2 * k, cx + 1.6 * k, cy + 2.6 * k, cx, cy + 5.0 * k);
        pin.addEllipse(QPointF(cx, cy - 0.6 * k), 1.25 * k, 1.25 * k);
        pin.setFillRule(Qt::OddEvenFill);
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        p.drawPath(pin);
    }
    p.restore();
}

} // namespace GroupBadge
