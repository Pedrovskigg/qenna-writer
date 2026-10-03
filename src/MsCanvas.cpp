#include "MsCanvas.h"

#include <QFontMetricsF>
#include <QtMath>
#include <cmath>

MsCanvas::MsCanvas(QPainter& p) : m_p(p), m_base(p.transform()) {
    fillStyle = QBrush(Qt::black);
    strokeStyle = QBrush(Qt::black);
    m_st.font = QFont(QStringLiteral("Georgia"));
}

MsCanvas::~MsCanvas() {
    m_p.setTransform(m_base);
    m_p.setOpacity(1);
}

void MsCanvas::pushStyle(State& s) const {
    s.fill = fillStyle; s.strokeB = strokeStyle; s.lw = lineWidth; s.alpha = globalAlpha;
    s.sb = shadowBlur; s.sox = shadowOffsetX; s.soy = shadowOffsetY; s.cap = lineCap; s.join = lineJoin;
    s.sc = shadowColor; s.align = textAlign;
}

void MsCanvas::pullStyle(const State& s) {
    fillStyle = s.fill; strokeStyle = s.strokeB; lineWidth = s.lw; globalAlpha = s.alpha;
    shadowBlur = s.sb; shadowOffsetX = s.sox; shadowOffsetY = s.soy; lineCap = s.cap; lineJoin = s.join;
    shadowColor = s.sc; textAlign = s.align;
}

void MsCanvas::save() {
    State s = m_st;
    pushStyle(s);
    m_stack.push_back(s);
    m_p.save();
}

void MsCanvas::restore() {
    if (m_stack.isEmpty()) return;
    m_st = m_stack.takeLast();
    pullStyle(m_st);
    m_p.restore();
}

void MsCanvas::translate(double x, double y) { m_st.T = QTransform().translate(x, y) * m_st.T; }
void MsCanvas::rotate(double rad) { m_st.T = QTransform().rotateRadians(rad) * m_st.T; }
void MsCanvas::scale(double sx, double sy) { m_st.T = QTransform().scale(sx, sy) * m_st.T; }

// sem setFillRule aqui: num caminho vazio ele cria um moveTo(0,0) escondido e o primeiro lineTo sairia da origem
void MsCanvas::beginPath() { m_path = QPainterPath(); }

void MsCanvas::moveTo(double x, double y) { m_path.moveTo(map(x, y)); }

void MsCanvas::lineTo(double x, double y) {
    if (m_path.elementCount() == 0) m_path.moveTo(map(x, y));
    else m_path.lineTo(map(x, y));
}

void MsCanvas::quadraticCurveTo(double cx, double cy, double x, double y) {
    if (m_path.elementCount() == 0) m_path.moveTo(map(cx, cy));
    m_path.quadTo(map(cx, cy), map(x, y));
}

void MsCanvas::bezierCurveTo(double c1x, double c1y, double c2x, double c2y, double x, double y) {
    if (m_path.elementCount() == 0) m_path.moveTo(map(c1x, c1y));
    m_path.cubicTo(map(c1x, c1y), map(c2x, c2y), map(x, y));
}

namespace {
// varredura do canvas: horário quando !anticlockwise; volta completa quando passa de 2π
double canvasSweep(double a0, double a1, bool acw) {
    const double TAU = 2 * M_PI;
    double sw = a1 - a0;
    if (!acw) {
        if (sw >= TAU) return TAU;
        sw = std::fmod(sw, TAU);
        if (sw < 0) sw += TAU;
    } else {
        if (sw <= -TAU) return -TAU;
        sw = std::fmod(sw, TAU);
        if (sw > 0) sw -= TAU;
    }
    return sw;
}
}

void MsCanvas::arc(double cx, double cy, double r, double a0, double a1, bool anticlockwise) {
    ellipse(cx, cy, r, r, 0, a0, a1, anticlockwise);
}

void MsCanvas::ellipse(double cx, double cy, double rx, double ry, double rot, double a0, double a1, bool acw) {
    if (rx <= 0 || ry <= 0) return;
    const double sw = canvasSweep(a0, a1, acw);
    // arco no espaço da elipse sem rotação, depois gira/translada e passa pela transformação atual
    QPainterPath e;
    const QRectF box(-rx, -ry, 2 * rx, 2 * ry);
    e.arcMoveTo(box, -qRadiansToDegrees(a0));
    e.arcTo(box, -qRadiansToDegrees(a0), -qRadiansToDegrees(sw));
    QTransform t;
    t.translate(cx, cy);
    t.rotateRadians(rot);
    const QPainterPath mapped = (t * m_st.T).map(e);
    if (m_path.elementCount() == 0) m_path.addPath(mapped);
    else m_path.connectPath(mapped);
}

void MsCanvas::rect(double x, double y, double w, double h) {
    QPainterPath r;
    r.moveTo(x, y); r.lineTo(x + w, y); r.lineTo(x + w, y + h); r.lineTo(x, y + h); r.closeSubpath();
    m_path.addPath(m_st.T.map(r));
}

void MsCanvas::roundRect(double x, double y, double w, double h, double r) { roundRect(x, y, w, h, r, r, r, r); }

void MsCanvas::roundRect(double x, double y, double w, double h, double tl, double tr, double br, double bl) {
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }
    const double m = std::min(w, h) / 2;
    tl = std::min(tl, m); tr = std::min(tr, m); br = std::min(br, m); bl = std::min(bl, m);
    QPainterPath p;
    p.moveTo(x + tl, y);
    p.lineTo(x + w - tr, y);
    if (tr > 0) p.arcTo(QRectF(x + w - 2 * tr, y, 2 * tr, 2 * tr), 90, -90);
    p.lineTo(x + w, y + h - br);
    if (br > 0) p.arcTo(QRectF(x + w - 2 * br, y + h - 2 * br, 2 * br, 2 * br), 0, -90);
    p.lineTo(x + bl, y + h);
    if (bl > 0) p.arcTo(QRectF(x, y + h - 2 * bl, 2 * bl, 2 * bl), 270, -90);
    p.lineTo(x, y + tl);
    if (tl > 0) p.arcTo(QRectF(x, y, 2 * tl, 2 * tl), 180, -90);
    p.closeSubpath();
    m_path.addPath(m_st.T.map(p));
}

void MsCanvas::closePath() { m_path.closeSubpath(); }

QPen MsCanvas::makePen() const {
    QPen pen(strokeStyle, lineWidth);
    pen.setCapStyle(lineCap);
    pen.setJoinStyle(lineJoin);
    if (!m_st.dash.isEmpty() && lineWidth > 0) {
        QVector<qreal> d;
        for (double v : m_st.dash) d << std::max(0.01, v / lineWidth);
        pen.setDashPattern(d);
    }
    return pen;
}

void MsCanvas::drawShadow(const QPainterPath& basePath, bool asStroke, double baseWidth) {
    if (shadowColor.alpha() == 0) return;
    if (shadowBlur <= 0 && shadowOffsetX == 0 && shadowOffsetY == 0) return;
    m_p.save();
    m_p.setTransform(m_base);
    m_p.setOpacity(globalAlpha);
    const QPainterPath sp = basePath.translated(shadowOffsetX, shadowOffsetY);
    QColor c = shadowColor;
    const double a = c.alphaF();
    if (!asStroke) {
        c.setAlphaF(a * (shadowBlur > 0 ? .45 : 1.0));
        m_p.fillPath(sp, c);
    }
    if (shadowBlur > 0) {
        const double rings[3][2] = { { 1.5, .08 }, { .9, .13 }, { .4, .2 } };
        for (const auto& rg : rings) {
            QColor rc = shadowColor;
            rc.setAlphaF(a * rg[1]);
            QPen pen(rc, baseWidth + shadowBlur * rg[0]);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            m_p.strokePath(sp, pen);
        }
    }
    m_p.restore();
}

void MsCanvas::fill(bool evenOdd) {
    bool ok = false;
    const QTransform inv = m_st.T.inverted(&ok);
    if (!ok) return;
    QPainterPath base = m_path;
    base.setFillRule(evenOdd ? Qt::OddEvenFill : Qt::WindingFill);
    drawShadow(base, false, 0);
    QPainterPath local = inv.map(base);
    local.setFillRule(base.fillRule());
    m_p.save();
    m_p.setTransform(m_st.T * m_base);
    m_p.setOpacity(globalAlpha);
    m_p.fillPath(local, fillStyle);
    m_p.restore();
}

void MsCanvas::stroke() {
    bool ok = false;
    const QTransform inv = m_st.T.inverted(&ok);
    if (!ok) return;
    const double sc = std::sqrt(std::abs(m_st.T.determinant()));
    drawShadow(m_path, true, lineWidth * sc);
    m_p.save();
    m_p.setTransform(m_st.T * m_base);
    m_p.setOpacity(globalAlpha);
    m_p.strokePath(inv.map(m_path), makePen());
    m_p.restore();
}

void MsCanvas::clip() {
    m_p.setTransform(m_base);
    m_p.setClipPath(m_path, Qt::IntersectClip);
}

void MsCanvas::fillRect(double x, double y, double w, double h) {
    QPainterPath r;
    r.addRect(QRectF(x, y, w, h).normalized());
    drawShadow(m_st.T.map(r), false, 0);
    m_p.save();
    m_p.setTransform(m_st.T * m_base);
    m_p.setOpacity(globalAlpha);
    m_p.fillPath(r, fillStyle);
    m_p.restore();
}

void MsCanvas::strokeRect(double x, double y, double w, double h) {
    QPainterPath r;
    r.addRect(QRectF(x, y, w, h).normalized());
    m_p.save();
    m_p.setTransform(m_st.T * m_base);
    m_p.setOpacity(globalAlpha);
    m_p.strokePath(r, makePen());
    m_p.restore();
}

void MsCanvas::fillText(const QString& s, double x, double y) {
    QFont f = m_st.font;
    f.setPixelSize(100);
    const double k = m_fontPx / 100.0;
    const double w = QFontMetricsF(f).horizontalAdvance(s);
    m_p.save();
    m_p.setTransform(QTransform().scale(k, k) * QTransform().translate(x, y) * m_st.T * m_base);
    m_p.setOpacity(globalAlpha);
    m_p.setFont(f);
    m_p.setPen(QPen(fillStyle, 0));
    m_p.drawText(QPointF(textAlign == Center ? -w / 2 : 0, 0), s);
    m_p.restore();
}

void MsCanvas::strokeText(const QString& s, double x, double y) {
    QFont f = m_st.font;
    f.setPixelSize(100);
    const double k = m_fontPx / 100.0;
    const double w = QFontMetricsF(f).horizontalAdvance(s);
    QPainterPath tp;
    tp.addText(QPointF(textAlign == Center ? -w / 2 : 0, 0), f, s);
    const QTransform t = QTransform().scale(k, k) * QTransform().translate(x, y);
    const QPainterPath local = t.map(tp);
    drawShadow(m_st.T.map(local), true, lineWidth);
    m_p.save();
    m_p.setTransform(m_st.T * m_base);
    m_p.setOpacity(globalAlpha);
    m_p.strokePath(local, makePen());
    m_p.restore();
}

QBrush MsCanvas::linear(double x0, double y0, double x1, double y1, const QVector<QPair<double, QColor>>& stops) {
    QLinearGradient g(QPointF(x0, y0), QPointF(x1, y1));
    for (const auto& s : stops) g.setColorAt(std::clamp(s.first, 0.0, 1.0), s.second);
    return QBrush(g);
}

QBrush MsCanvas::radial(double x0, double y0, double r0, double x1, double y1, double r1, const QVector<QPair<double, QColor>>& stops) {
    QRadialGradient g(QPointF(x1, y1), r1, QPointF(x0, y0), r0);
    for (const auto& s : stops) g.setColorAt(std::clamp(s.first, 0.0, 1.0), s.second);
    return QBrush(g);
}
