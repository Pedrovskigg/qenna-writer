#include "DocTabs.h"

#include "Theme.h"
#include "UiScale.h"

#include <QCoreApplication>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QVariantAnimation>

#include <algorithm>

namespace {
constexpr int kMaxVisible = 6;     // mais que isso, as mais antigas viram "+N"
constexpr qreal kTabH = 17, kTabHOn = 20, kGap = 2, kRight = 8, kMaxW = 92;
constexpr qreal kZoneTop = 34, kZoneLeft = 60;   // a zona invisível do canto: um pouco maior que as abas
QColor withAlpha(QColor c, qreal a) { c.setAlphaF(std::clamp(a, 0.0, 1.0)); return c; }
}

DocTabs::DocTabs(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    m_showDelay = new QTimer(this);
    m_showDelay->setSingleShot(true);
    m_showDelay->setInterval(150);   // o mouse só passando pelo canto não levanta as abas
    connect(m_showDelay, &QTimer::timeout, this, [this]() { setRevealTarget(true); });
    m_hideDelay = new QTimer(this);
    m_hideDelay->setSingleShot(true);
    m_hideDelay->setInterval(260);
    connect(m_hideDelay, &QTimer::timeout, this, [this]() { setRevealTarget(false); });
    m_anim = new QVariantAnimation(this);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_reveal = v.toReal(); update(); });
}

// 1,1: o tamanho do lab ficou pequeno demais no app (pedido do Pe, +10% em tudo: aba, letra, ícone e zona)
qreal DocTabs::s(qreal v) const { return v * UiScale::scale() * 1.1; }

void DocTabs::setTabs(const QVector<Tab>& tabs)
{
    m_tabs = tabs;
    m_hot = -1;
    m_hotClose = m_hotMore = false;
    update();
}

int DocTabs::tabWidth(const Tab& t) const
{
    QFont f = font();
    f.setPixelSize(qMax(8, qRound(s(10))));
    const qreal text = QFontMetricsF(f).horizontalAdvance(t.title);
    const qreal w = s(6) + s(11) + s(4) + text + s(2) + s(12) + s(2);
    return qRound(std::min(w, s(kMaxW)));
}

QSize DocTabs::zoneSize() const
{
    QRectF more;
    const auto sl = layoutSlots(&more);
    qreal w = s(kRight);
    for (const Slot& x : sl) w += x.rect.width() + s(kGap);
    if (!more.isNull()) w += more.width() + s(kGap);
    return QSize(qRound(w + s(kZoneLeft)), qRound(s(kZoneTop)));
}

QVector<DocTabs::Slot> DocTabs::layoutSlots(QRectF* moreRect) const
{
    QVector<Slot> out;
    const int n = m_tabs.size(), first = std::max(0, n - kMaxVisible);
    // da direita pra esquerda: a mais nova encosta no canto
    qreal right = width() - s(kRight);
    const qreal drop = (1 - m_reveal) * (s(kTabHOn) + 2);
    for (int i = n - 1; i >= first; --i) {
        const Tab& t = m_tabs[i];
        const qreal w = tabWidth(t);
        const bool hotOrOn = t.active || i == m_hot;
        const qreal h = s(t.active ? kTabHOn : (i == m_hot ? kTabH + 2 : kTabH));
        (void)hotOrOn;
        const QRectF r(right - w, height() - h + drop, w, h + 1);
        out.prepend({ i, r, QRectF(r.right() - s(14), r.top() + (h - s(12)) / 2, s(12), s(12)) });
        right -= w + s(kGap);
    }
    if (moreRect) {
        *moreRect = QRectF();
        if (first > 0) {
            QFont f = font();
            f.setPixelSize(qMax(8, qRound(s(10))));
            const qreal w = QFontMetricsF(f).horizontalAdvance(QStringLiteral("+%1").arg(first)) + s(10);
            *moreRect = QRectF(right - w, height() - s(kTabH) + drop, w, s(kTabH));
        }
    }
    return out;
}

void DocTabs::setRevealTarget(bool up)
{
    const qreal target = up ? 1.0 : 0.0;
    if (qFuzzyCompare(m_reveal + 1, target + 1) && m_anim->state() != QAbstractAnimation::Running) return;
    m_anim->stop();
    m_anim->setDuration(up ? 220 : 180);
    m_anim->setStartValue(m_reveal);
    m_anim->setEndValue(target);
    m_anim->start();
}

void DocTabs::enterEvent(QEnterEvent* e)
{
    QWidget::enterEvent(e);
    m_hideDelay->stop();
    if (m_reveal < 1) m_showDelay->start();
}

void DocTabs::leaveEvent(QEvent* e)
{
    QWidget::leaveEvent(e);
    m_showDelay->stop();
    m_hideDelay->start();
    m_hot = -1;
    m_hotClose = m_hotMore = false;
    update();
}

void DocTabs::mouseMoveEvent(QMouseEvent* e)
{
    int hot = -1;
    bool hotClose = false, hotMore = false;
    if (m_reveal > 0.6) {
        QRectF more;
        for (const Slot& x : layoutSlots(&more)) {
            if (x.rect.contains(e->position())) { hot = x.index; hotClose = x.closeRect.adjusted(-2, -3, 2, 3).contains(e->position()); }
        }
        hotMore = !more.isNull() && more.contains(e->position());
    }
    if (hot != m_hot || hotClose != m_hotClose || hotMore != m_hotMore) {
        m_hot = hot; m_hotClose = hotClose; m_hotMore = hotMore;
        setCursor(hot >= 0 || hotMore ? Qt::PointingHandCursor : Qt::ArrowCursor);
        setToolTip(hot >= 0 ? (hotClose ? QCoreApplication::translate("DocTabs", "Fechar") : m_tabs[hot].title) : QString());
        update();
    }
}

void DocTabs::mousePressEvent(QMouseEvent* e)
{
    m_pressed = (e->button() == Qt::LeftButton || e->button() == Qt::MiddleButton) ? m_hot : -1;
    if (e->button() == Qt::LeftButton && m_hotMore && showHidden) {
        QRectF more;
        layoutSlots(&more);
        showHidden(mapToGlobal(more.topLeft().toPoint()));
    }
    e->accept();
}

void DocTabs::mouseReleaseEvent(QMouseEvent* e)
{
    const int i = m_pressed;
    m_pressed = -1;
    if (i < 0 || i != m_hot || i >= m_tabs.size()) return;
    // botão do meio fecha, como numa aba de navegador
    if (e->button() == Qt::MiddleButton || m_hotClose) { if (close) close(i); }
    else if (e->button() == Qt::LeftButton && activate) activate(i);
}

void DocTabs::paintEvent(QPaintEvent*)
{
    if (m_reveal <= 0.001 || m_tabs.isEmpty()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(std::min(1.0, m_reveal * 1.4));
    const QColor panel(Theme::chromeBackground()), border(Theme::panelBorder());
    const QColor muted(Theme::textMuted()), primary(Theme::textPrimary()), bright(Theme::textBright()), accent(Theme::accentDefault());
    QFont f = font();
    f.setPixelSize(qMax(8, qRound(s(10))));
    f.setWeight(QFont::Medium);
    QFont mono(QStringLiteral("Lora"));
    mono.setPixelSize(qMax(7, qRound(s(8.5))));
    mono.setBold(true);

    QRectF more;
    for (const Slot& x : layoutSlots(&more)) {
        const Tab& t = m_tabs[x.index];
        const bool hot = x.index == m_hot;
        const QRectF r = x.rect.adjusted(0.5, 0.5, -0.5, 0);
        const qreal rad = s(6);
        QPainterPath path;
        path.moveTo(r.left(), r.bottom());
        path.lineTo(r.left(), r.top() + rad);
        path.quadTo(r.left(), r.top(), r.left() + rad, r.top());
        path.lineTo(r.right() - rad, r.top());
        path.quadTo(r.right(), r.top(), r.right(), r.top() + rad);
        path.lineTo(r.right(), r.bottom());
        p.setPen(QPen(border, 1));
        p.setBrush(withAlpha(panel, t.active ? 1.0 : 0.86));
        p.drawPath(path);
        if (t.active) {
            p.setPen(QPen(accent, s(1.5), Qt::SolidLine, Qt::FlatCap));
            p.drawLine(QPointF(r.left() + rad * 0.6, r.top() + 0.8), QPointF(r.right() - rad * 0.6, r.top() + 0.8));
        }
        // ícone: o número do capítulo, a inicial (ou a foto) da ficha, o ícone da gaveta
        const qreal is = s(11), ix = r.left() + s(6), iy = r.center().y() - is / 2;
        const QRectF ir(ix, iy, is, is);
        if (t.kind == Tab::Chapter) {
            p.setFont(mono);
            p.setPen(t.active ? bright : muted);
            p.drawText(ir.adjusted(-2, 0, 2, 0), Qt::AlignCenter, t.mono);
        } else if (t.kind == Tab::Element && !t.icon.isNull()) {
            QPainterPath clip;
            clip.addEllipse(ir);
            p.save();
            p.setClipPath(clip);
            p.drawPixmap(ir, t.icon, QRectF(t.icon.rect()));
            p.restore();
        } else if (t.kind == Tab::Element) {
            p.setPen(Qt::NoPen);
            p.setBrush(withAlpha(t.color, 0.2));
            p.drawEllipse(ir);
            QFont ini = f;
            ini.setPixelSize(qMax(6, qRound(s(7))));
            ini.setBold(true);
            p.setFont(ini);
            p.setPen(t.color);
            p.drawText(ir, Qt::AlignCenter, t.title.left(1).toUpper());
        } else if (!t.icon.isNull()) {
            const QRectF dr(ix + s(0.5), r.center().y() - s(5), s(10), s(10));
            p.drawPixmap(dr, t.icon, QRectF(t.icon.rect()));
        }
        // nome (o × aparece no lugar do fim com o mouse em cima)
        p.setFont(f);
        p.setPen(t.active ? bright : hot ? primary : muted);
        const QRectF tr(ix + is + s(4), r.top(), r.right() - s(16) - (ix + is + s(4)) + (hot ? 0 : s(12)), r.height());
        p.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter, QFontMetricsF(f).elidedText(t.title, Qt::ElideRight, tr.width()));
        if (hot) {
            const QRectF cr = x.closeRect;
            if (m_hotClose) { p.setPen(Qt::NoPen); p.setBrush(withAlpha(primary, 0.12)); p.drawRoundedRect(cr, s(3), s(3)); }
            p.setPen(QPen(m_hotClose ? primary : muted, s(1.1), Qt::SolidLine, Qt::RoundCap));
            const qreal c = s(2.6);
            p.drawLine(cr.center() + QPointF(-c, -c), cr.center() + QPointF(c, c));
            p.drawLine(cr.center() + QPointF(-c, c), cr.center() + QPointF(c, -c));
        }
    }
    if (!more.isNull()) {
        p.setFont(f);
        p.setPen(m_hotMore ? primary : muted);
        p.drawText(more, Qt::AlignCenter, QStringLiteral("+%1").arg(std::max(0, int(m_tabs.size()) - kMaxVisible)));
    }
}
