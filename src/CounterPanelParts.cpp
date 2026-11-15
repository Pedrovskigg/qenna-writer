#include "CounterPanelParts.h"

#include "Theme.h"

#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QToolTip>
#include <QtMath>

namespace {
QColor col(const QString& css) { return Theme::toColor(css); }

QColor withAlpha(QColor c, qreal a)
{
    c.setAlphaF(qBound(0.0, a, 1.0));
    return c;
}

QFont pxFont(const QFont& base, int px, QFont::Weight w)
{
    QFont f = base;
    f.setPixelSize(px);
    f.setWeight(w);
    return f;
}

// Raio dos controles, sem passar da metade da altura (temas de canto reto
// continuam retos; temas muito redondos não viram cápsula torta).
qreal ctrlRadius(qreal h)
{
    return qMin<qreal>(Theme::controlRadius(), h / 2.0);
}
}

QColor counterLevelColor(int stars, double fraction)
{
    const QColor acc = col(Theme::accentDefault());
    if (stars <= 0) {
        if (fraction > 0) return withAlpha(acc, 0.16);
        return col(Theme::subtleBorder());
    }
    static const qreal kAlpha[6] = { 0, 0.30, 0.48, 0.66, 0.84, 1.0 };
    return withAlpha(acc, kAlpha[qBound(1, stars, 5)]);
}

// ---------------------------------------------------------------- abas

CounterTabBar::CounterTabBar(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_pulseTimer = new QTimer(this);
    m_pulseTimer->setInterval(60);
    QObject::connect(m_pulseTimer, &QTimer::timeout, this, [this]() {
        m_phase += 0.06 / 1.2 * 2 * M_PI;
        update();
    });
}

void CounterTabBar::setTabs(const QStringList& labels)
{
    m_labels = labels;
    updateGeometry();
    update();
}

void CounterTabBar::setCurrent(int index)
{
    if (index == m_current) return;
    m_current = index;
    update();
}

void CounterTabBar::setPulse(int index)
{
    if (index == m_pulseIndex) return;
    m_pulseIndex = index;
    if (index >= 0) m_pulseTimer->start(); else m_pulseTimer->stop();
    update();
}

QFont CounterTabBar::tabFont() const { return pxFont(font(), 12, QFont::DemiBold); }

QSize CounterTabBar::sizeHint() const { return { 228, 35 }; }

QVector<QRectF> CounterTabBar::tabRects() const
{
    QVector<QRectF> out;
    const QFontMetricsF fm(tabFont());
    qreal x = 14;
    for (int i = 0; i < m_labels.size(); ++i) {
        qreal w = fm.horizontalAdvance(m_labels[i]);
        if (i == m_pulseIndex) w += 11;
        out.append(QRectF(x, 0, w, height()));
        x += w + 16;
    }
    return out;
}

int CounterTabBar::tabAt(const QPoint& pos) const
{
    const auto rects = tabRects();
    for (int i = 0; i < rects.size(); ++i)
        if (rects[i].adjusted(-6, 0, 6, 0).contains(pos)) return i;
    return -1;
}

void CounterTabBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor muted = col(Theme::textMuted());
    const QColor prim = col(Theme::textPrimary());
    const QColor bright = col(Theme::textBright());
    const QColor acc = col(Theme::accentDefault());

    p.setPen(QPen(col(Theme::subtleBorder()), 1));
    p.drawLine(QPointF(0, height() - 0.5), QPointF(width(), height() - 0.5));

    p.setFont(tabFont());
    const QFontMetricsF fm(tabFont());
    const auto rects = tabRects();
    for (int i = 0; i < rects.size(); ++i) {
        const QRectF r = rects[i];
        const bool on = i == m_current;
        p.setPen(on ? bright : (i == m_hover ? prim : muted));
        const qreal tw = fm.horizontalAdvance(m_labels[i]);
        p.drawText(QRectF(r.left(), 0, tw, height() - 1), Qt::AlignVCenter | Qt::AlignLeft, m_labels[i]);
        if (on) {
            p.fillRect(QRectF(r.left(), height() - 2, r.width(), 2), acc);
        }
        if (i == m_pulseIndex) {
            const qreal a = 0.65 + 0.35 * qCos(m_phase);
            p.setPen(Qt::NoPen);
            p.setBrush(withAlpha(acc, a));
            p.drawEllipse(QPointF(r.left() + tw + 8, height() / 2.0 - 0.5), 3, 3);
        }
    }
}

void CounterTabBar::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    const int i = tabAt(e->pos());
    if (i < 0 || i == m_current) return;
    m_current = i;
    update();
    if (onChanged) onChanged(i);
}

void CounterTabBar::mouseMoveEvent(QMouseEvent* e)
{
    const int i = tabAt(e->pos());
    if (i != m_hover) { m_hover = i; update(); }
}

void CounterTabBar::leaveEvent(QEvent*)
{
    if (m_hover != -1) { m_hover = -1; update(); }
}

// ---------------------------------------------------------- segmentados

CounterSegmented::CounterSegmented(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void CounterSegmented::setOptions(const QStringList& labels, const QStringList& tooltips)
{
    m_labels = labels;
    m_tips = tooltips;
    updateGeometry();
    update();
}

void CounterSegmented::setCurrent(int index)
{
    if (index == m_current) return;
    m_current = index;
    update();
}

QSize CounterSegmented::sizeHint() const { return { 110, 26 }; }

QSize CounterSegmented::minimumSizeHint() const
{
    const QFontMetrics fm(pxFont(font(), 11, QFont::DemiBold));
    int w = 4;
    for (const QString& l : m_labels) w += fm.horizontalAdvance(l) + 12;
    return { w, 26 };
}

int CounterSegmented::indexAt(const QPoint& pos) const
{
    if (m_labels.isEmpty() || !rect().contains(pos)) return -1;
    const qreal w = (width() - 4.0) / m_labels.size();
    return qBound(0, int((pos.x() - 2) / w), int(m_labels.size()) - 1);
}

bool CounterSegmented::event(QEvent* e)
{
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        const int i = indexAt(he->pos());
        if (i >= 0 && i < m_tips.size() && !m_tips[i].isEmpty())
            QToolTip::showText(he->globalPos(), m_tips[i], this);
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}

void CounterSegmented::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect());
    const qreal rad = ctrlRadius(r.height());
    p.setPen(Qt::NoPen);
    p.setBrush(col(Theme::chromeBackground()));
    p.drawRoundedRect(r, rad, rad);
    if (m_labels.isEmpty()) return;

    const qreal w = (r.width() - 4.0) / m_labels.size();
    const QColor muted = col(Theme::textMuted());
    const QColor prim = col(Theme::textPrimary());
    const QColor bright = col(Theme::textBright());
    QColor pill = col(Theme::panelBackground());
    pill.setAlpha(255);

    p.setFont(pxFont(font(), 11, QFont::DemiBold));
    for (int i = 0; i < m_labels.size(); ++i) {
        const QRectF seg(2 + i * w, 2, w, r.height() - 4);
        if (i == m_current) {
            const qreal pr = qMax<qreal>(0, rad - 2);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 34));
            p.drawRoundedRect(seg.translated(0, 1), pr, pr);
            p.setBrush(pill);
            p.drawRoundedRect(seg, pr, pr);
        }
        p.setPen(i == m_current ? bright : (i == m_hover ? prim : muted));
        p.drawText(seg, Qt::AlignCenter, m_labels[i]);
    }
}

void CounterSegmented::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    const int i = indexAt(e->pos());
    if (i < 0 || i == m_current) return;
    m_current = i;
    update();
    if (onChanged) onChanged(i);
}

void CounterSegmented::mouseMoveEvent(QMouseEvent* e)
{
    const int i = indexAt(e->pos());
    if (i != m_hover) { m_hover = i; update(); }
}

void CounterSegmented::leaveEvent(QEvent*)
{
    if (m_hover != -1) { m_hover = -1; update(); }
}

// ------------------------------------------------------------ barra fina

CounterThinBar::CounterThinBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(5);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void CounterThinBar::setValue(double fraction, bool done)
{
    fraction = qBound(0.0, fraction, 1.0);
    if (qFuzzyCompare(fraction + 1, m_value + 1) && done == m_done) return;
    m_value = fraction;
    m_done = done;
    update();
}

void CounterThinBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    const QRectF r = QRectF(rect());
    const qreal rad = r.height() / 2.0;
    p.setBrush(col(Theme::subtleBorder()));
    p.drawRoundedRect(r, rad, rad);
    if (m_value <= 0) return;
    p.setBrush(col(m_done ? Theme::accentSuccess() : Theme::accentDefault()));
    p.drawRoundedRect(QRectF(r.left(), r.top(), qMax(r.height(), r.width() * m_value), r.height()), rad, rad);
}

// ----------------------------------------------------- barrinhas dos dias

CounterDayStrip::CounterDayStrip(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(26);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
}

void CounterDayStrip::setDays(const QVector<Day>& days)
{
    m_days = days;
    update();
}

QRectF CounterDayStrip::barSlot(int i) const
{
    const int n = qMax(1, int(m_days.size()));
    const qreal gap = 3;
    // 2 px de folga em cada lado pro contorno do "hoje" não ser cortado.
    const qreal w = (width() - 4 - gap * (n - 1)) / n;
    return QRectF(2 + i * (w + gap), 2, w, height() - 4);
}

bool CounterDayStrip::event(QEvent* e)
{
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        for (int i = 0; i < m_days.size(); ++i) {
            if (barSlot(i).adjusted(-1.5, -2, 1.5, 2).contains(he->pos())) {
                QToolTip::showText(he->globalPos(), m_days[i].tooltip, this);
                return true;
            }
        }
        QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}

void CounterDayStrip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor bright = col(Theme::textBright());
    const QColor muted = col(Theme::textMuted());
    for (int i = 0; i < m_days.size(); ++i) {
        const Day& d = m_days[i];
        const QRectF slot = barSlot(i);
        // Altura: nada = um traço; meta parcial = 12–30%; 1–5 vezes a meta = 44–100%.
        qreal frac;
        if (d.stars > 0)          frac = (30 + 14 * qMin(d.stars, 5)) / 100.0;
        else if (d.fraction > 0)  frac = 0.12 + 0.18 * qMin(1.0, d.fraction);
        else                      frac = 0;
        const qreal h = qMax<qreal>(3, slot.height() * frac);
        const QRectF bar(slot.left(), slot.bottom() - h, slot.width(), h);
        p.setPen(Qt::NoPen);
        if (d.offDay && d.stars == 0) {
            const QRectF ob(slot.left() + 0.5, slot.bottom() - slot.height() * 0.4, slot.width() - 1, slot.height() * 0.4 - 0.5);
            QPen pen(withAlpha(muted, 0.8), 1, Qt::DashLine);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(ob, 2, 2);
        } else {
            p.setBrush(counterLevelColor(d.stars, d.fraction));
            p.drawRoundedRect(bar, 2, 2);
        }
        if (d.today) {
            p.setPen(QPen(bright, 1.5));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(bar.adjusted(-1.5, -1.5, 1.5, 1.5), 3, 3);
        }
    }
}

void CounterDayStrip::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->pos()) && onClicked) onClicked();
}
