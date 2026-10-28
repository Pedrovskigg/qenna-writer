#include "CartoRing.h"

#include "CartoIcons.h"
#include "../Theme.h"

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <cmath>

namespace carto {

namespace {
constexpr double R0 = 54, R1 = 136;
QColor mixc(const QColor& a, const QColor& b, double t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, 1.0);
}
}

Ring::Ring(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    hide();
    if (parent) parent->installEventFilter(this);
}

bool Ring::eventFilter(QObject* o, QEvent* e)
{
    if (o == parentWidget() && e->type() == QEvent::Resize && isVisible()) setGeometry(parentWidget()->rect());
    return QWidget::eventFilter(o, e);
}

void Ring::openAt(QPoint at, const QString& active, const QString& centerTitle, const QString& centerSub)
{
    QWidget* p = parentWidget();
    setGeometry(p->rect());
    const int R = 150;
    m_c = QPoint(std::max(R + 6, std::min(p->width() - R - 6, at.x())), std::max(R + 6, std::min(p->height() - R - 6, at.y())));
    m_active = active; m_title = centerTitle; m_sub = centerSub; m_hover = -1;
    show();
    raise();
    setFocus();
    update();
}

int Ring::sliceAt(QPointF p) const
{
    const QPointF d = p - QPointF(m_c);
    const double r = std::hypot(d.x(), d.y());
    if (r < R0 || r > R1 + 6 || m_items.isEmpty()) return -1;
    const int n = m_items.size();
    // fatia k centrada em k/n·2π − π/2
    double a = std::atan2(d.y(), d.x()) + M_PI / 2 + M_PI / n;
    a = std::fmod(a + 4 * M_PI, 2 * M_PI);
    return std::min(n - 1, static_cast<int>(a / (2 * M_PI / n)));
}

void Ring::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor panel = Theme::toColor(Theme::panelBackground()), line = Theme::toColor(Theme::panelBorder()), ink = Theme::toColor(Theme::textPrimary()),
                 muted = Theme::toColor(Theme::textMuted()), accent = Theme::toColor(Theme::accentDefault());
    const QColor panel2 = mixc(panel, ink, 0.07);
    QColor soft = accent; soft.setAlphaF(0.2);
    const QPointF C(m_c);

    QRadialGradient sh(C + QPointF(0, 10), R1 + 26);
    sh.setColorAt(0.82, QColor(0, 0, 0, 90));
    sh.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setPen(Qt::NoPen); p.setBrush(sh);
    p.drawEllipse(C + QPointF(0, 10), R1 + 26, R1 + 26);
    p.setBrush(panel); p.setPen(QPen(line, 1));
    p.drawEllipse(C, R1 + 6, R1 + 6);

    const int n = m_items.size();
    for (int k = 0; k < n; k++) {
        const RingItem& it = m_items[k];
        const double a0 = (double(k) / n) * 2 * M_PI - M_PI / 2 - M_PI / n + 0.025;
        const double a1 = (double(k + 1) / n) * 2 * M_PI - M_PI / 2 - M_PI / n - 0.025;
        const bool on = it.id == m_active, hov = k == m_hover && it.enabled;
        QPainterPath path;
        path.moveTo(C + QPointF(R0 * std::cos(a0), R0 * std::sin(a0)));
        path.lineTo(C + QPointF(R1 * std::cos(a0), R1 * std::sin(a0)));
        path.arcTo(QRectF(C.x() - R1, C.y() - R1, 2 * R1, 2 * R1), -a0 * 180 / M_PI, -(a1 - a0) * 180 / M_PI);
        path.lineTo(C + QPointF(R0 * std::cos(a1), R0 * std::sin(a1)));
        path.arcTo(QRectF(C.x() - R0, C.y() - R0, 2 * R0, 2 * R0), -a1 * 180 / M_PI, (a1 - a0) * 180 / M_PI);
        path.closeSubpath();
        p.setBrush(on ? soft : hov ? panel2 : QColor(Qt::transparent));
        p.setPen(on ? QPen(accent, 1.2) : QPen(line, 1));
        p.drawPath(path);
        const double am = (a0 + a1) / 2;
        const QPointF ic = C + QPointF(96 * std::cos(am), 96 * std::sin(am));
        QColor col = on ? accent : ink;
        if (!it.enabled) col = muted;
        if (!it.enabled) col.setAlphaF(0.55);
        p.drawPixmap(QPointF(ic.x() - 11, ic.y() - 19), iconPixmap(it.icon, col, 22));
        QFont f = font(); f.setPixelSize(11); f.setWeight(on ? QFont::Bold : QFont::Medium);
        p.setFont(f); p.setPen(col);
        p.drawText(QRectF(ic.x() - 46, ic.y() + 7, 92, 16), Qt::AlignCenter, it.label);
    }
    p.setPen(QPen(line, 1)); p.setBrush(panel2);
    p.drawEllipse(C, R0 - 4, R0 - 4);
    QFont tf = font(); tf.setPixelSize(12); tf.setBold(true);
    p.setFont(tf); p.setPen(ink);
    p.drawText(QRectF(C.x() - 48, C.y() - (m_sub.isEmpty() ? 9 : 16), 96, 18), Qt::AlignCenter, m_title);
    if (!m_sub.isEmpty()) {
        QFont sf = font(); sf.setPixelSize(11);
        p.setFont(sf); p.setPen(muted);
        p.drawText(QRectF(C.x() - 48, C.y() + 2, 96, 16), Qt::AlignCenter, m_sub);
    }
}

void Ring::mousePressEvent(QMouseEvent* e)
{
    const int k = sliceAt(e->position());
    hide();
    if (k >= 0 && m_items[k].enabled) emit picked(m_items[k].id, m_c);
}

void Ring::mouseMoveEvent(QMouseEvent* e)
{
    const int k = sliceAt(e->position());
    if (k != m_hover) { m_hover = k; update(); }
    setCursor(k >= 0 && m_items[k].enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void Ring::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) { hide(); return; }
    QWidget::keyPressEvent(e);
}

} // namespace carto
