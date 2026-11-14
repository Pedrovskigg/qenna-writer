#pragma once

// A faixa de nomes que sai da barra com o mouse em cima (LeftBar e RightBar).
// Mora aqui desde 13/11/2026 pra RightBar usar a mesma, com o mesmo desenho.

#include "Theme.h"
#include "UiScale.h"

#include <QContextMenuEvent>
#include <QFont>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QList>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QVariantAnimation>
#include <QWidget>

#include <functional>

namespace {
[[maybe_unused]] constexpr int kLabelsShadow = 14;
[[maybe_unused]] constexpr int kLabelsOpenDelayMs = 280;
// A faixa tem a largura do nome mais comprido, entre estes limites.
[[maybe_unused]] inline int labelsMinWidthPx() { return qRound(104 * UiScale::scale()); }
[[maybe_unused]] inline int labelsMaxWidthPx() { return qRound(176 * UiScale::scale()); }
}

class LeftBarLabels : public QWidget {
public:
    struct Row {
        int y = 0, h = 0;
        QString text;
        QString count;
        bool active = false;
        std::function<void()> click;
        std::function<void(const QPoint&)> context;
    };
    struct Head { int y = 0; QString text; };

    explicit LeftBarLabels(QWidget* parent) : QWidget(parent) {
        setMouseTracking(true);
        setAttribute(Qt::WA_NoSystemBackground);
        hide();
    }

    std::function<void()> onLeave;

    void setContent(const QList<Row>& rows, const QList<Head>& heads, bool mirrored) {
        m_rows = rows;
        m_heads = heads;
        m_mirrored = mirrored;
        m_hover = -1;
        update();
    }

    void open(bool animate) {
        stopAnim();
        if (!animate) { m_reveal = 1.0; update(); return; }
        m_reveal = 0.0;
        auto* a = new QVariantAnimation(this);
        a->setDuration(170);
        a->setStartValue(0.0);
        a->setEndValue(1.0);
        a->setEasingCurve(QEasingCurve::OutCubic);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_reveal = v.toReal(); update(); });
        m_anim = a;
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }

    void close(bool animate) {
        stopAnim();
        if (!animate || !isVisible()) { hide(); return; }
        auto* a = new QVariantAnimation(this);
        a->setDuration(110);
        a->setStartValue(m_reveal);
        a->setEndValue(0.0);
        a->setEasingCurve(QEasingCurve::InCubic);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_reveal = v.toReal(); update(); });
        connect(a, &QVariantAnimation::finished, this, [this]() { hide(); m_reveal = 1.0; });
        m_anim = a;
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int W = width() - kLabelsShadow;
        const int shown = qRound(W * qBound(0.0, m_reveal, 1.0));
        if (shown <= 0) return;
        // Recorte a partir da borda da barra: a faixa "sai" dela.
        // Recorte a partir da borda da barra: a faixa "sai" dela. Layout do
        // widget: [corpo W][sombra] (ou espelhado: [sombra][corpo W]).
        const int left = m_mirrored ? kLabelsShadow : 0;
        const QRectF body(left, 0.5, W, height() - 1.0);
        const qreal visL = m_mirrored ? left + (W - shown) : left;
        p.setClipRect(QRectF(m_mirrored ? visL - kLabelsShadow : 0, 0, shown + kLabelsShadow, height()));
        // Sombra suave no lado de fora.
        const qreal sx = m_mirrored ? visL - kLabelsShadow : visL + shown;
        QLinearGradient sh(QPointF(sx, 0), QPointF(sx + kLabelsShadow, 0));
        sh.setColorAt(m_mirrored ? 1 : 0, QColor(0, 0, 0, 70));
        sh.setColorAt(m_mirrored ? 0 : 1, QColor(0, 0, 0, 0));
        p.fillRect(QRectF(sx, 8, kLabelsShadow, height() - 16), sh);

        // Fundo com os cantos arredondados só do lado de fora; o lado da barra
        // passa por baixo dela, sem borda, pra as duas parecerem uma peça só.
        const qreal r = 10;
        QPainterPath path;
        const QRectF shape = m_mirrored ? QRectF(visL, body.top(), shown + 20, body.height())
                                        : QRectF(visL - 20, body.top(), shown + 20, body.height());
        path.addRoundedRect(shape, r, r);
        QColor bg = Theme::toColor(Theme::panelBackground());
        bg.setAlphaF(qMax(bg.alphaF(), 0.97f));
        p.setPen(QPen(Theme::toColor(Theme::panelBorder()), 1));
        p.setBrush(bg);
        p.drawPath(path);

        const qreal fade = qBound(0.0, (m_reveal - 0.35) / 0.65, 1.0);
        p.setOpacity(fade);
        const qreal s = UiScale::scale();
        QFont hf(QStringLiteral("Segoe UI"));
        hf.setPixelSize(qMax(7, qRound(8.5 * s)));
        hf.setBold(true);
        hf.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
        for (const Head& h : m_heads) {
            p.setFont(hf);
            QColor c = Theme::toColor(Theme::textMuted());
            c.setAlphaF(0.85f);
            p.setPen(c);
            const QRectF tr(body.left() + 12, h.y - 7, W - 24, 14);
            p.drawText(tr, Qt::AlignVCenter | (m_mirrored ? Qt::AlignRight : Qt::AlignLeft), h.text.toUpper());
        }
        QFont nf(QStringLiteral("Segoe UI"));
        nf.setPixelSize(qMax(10, qRound(13 * s)));
        QFont cf(QStringLiteral("Segoe UI"));
        cf.setPixelSize(qMax(9, qRound(11 * s)));
        for (int i = 0; i < m_rows.size(); ++i) {
            const Row& row = m_rows.at(i);
            const QRectF rr(body.left() + 4, row.y, W - 8, row.h);
            if (i == m_hover || row.active) {
                p.setPen(Qt::NoPen);
                p.setBrush(Theme::toColor(i == m_hover ? Theme::hoverOverlay() : Theme::pressedOverlay()));
                p.drawRoundedRect(rr, 7, 7);
            }
            p.setFont(cf);
            p.setPen(Theme::toColor(Theme::textMuted()));
            const qreal cw = row.count.isEmpty() ? 0 : QFontMetricsF(cf).horizontalAdvance(row.count) + 8;
            if (cw > 0) {
                const QRectF cr = m_mirrored ? QRectF(rr.left() + 8, rr.top(), cw, rr.height())
                                             : QRectF(rr.right() - 8 - cw, rr.top(), cw, rr.height());
                p.drawText(cr, Qt::AlignVCenter | (m_mirrored ? Qt::AlignLeft : Qt::AlignRight), row.count);
            }
            QFont f = nf;
            f.setWeight(row.active ? QFont::DemiBold : QFont::Normal);
            p.setFont(f);
            p.setPen(Theme::toColor((row.active || i == m_hover) ? Theme::textBright() : Theme::textPrimary()));
            const QRectF tr = m_mirrored ? QRectF(rr.left() + 8 + cw, rr.top(), rr.width() - 16 - cw, rr.height())
                                         : QRectF(rr.left() + 8, rr.top(), rr.width() - 16 - cw, rr.height());
            p.drawText(tr, Qt::AlignVCenter | (m_mirrored ? Qt::AlignRight : Qt::AlignLeft),
                       QFontMetricsF(f).elidedText(row.text, Qt::ElideRight, tr.width()));
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        const int h = rowAt(e->position().toPoint());
        if (h != m_hover) { m_hover = h; update(); }
        setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    void mousePressEvent(QMouseEvent* e) override { m_press = rowAt(e->position().toPoint()); e->accept(); }
    void mouseReleaseEvent(QMouseEvent* e) override {
        const int r = rowAt(e->position().toPoint());
        if (e->button() == Qt::LeftButton && r >= 0 && r == m_press && m_rows.at(r).click) {
            const auto click = m_rows.at(r).click;
            click();
        }
        m_press = -1;
    }
    void contextMenuEvent(QContextMenuEvent* e) override {
        const int r = rowAt(e->pos());
        if (r >= 0 && m_rows.at(r).context) {
            const auto ctx = m_rows.at(r).context;
            ctx(e->globalPos());
        }
    }
    void leaveEvent(QEvent*) override {
        if (m_hover != -1) { m_hover = -1; update(); }
        if (onLeave) onLeave();
    }

private:
    int rowAt(const QPoint& p) const {
        const int W = width() - kLabelsShadow;
        const int left = m_mirrored ? kLabelsShadow : 0;
        if (p.x() < left || p.x() > left + W) return -1;
        for (int i = 0; i < m_rows.size(); ++i)
            if (p.y() >= m_rows.at(i).y && p.y() < m_rows.at(i).y + m_rows.at(i).h) return i;
        return -1;
    }
    void stopAnim() {
        if (m_anim) { QVariantAnimation* a = m_anim; m_anim.clear(); a->stop(); }
    }

    QList<Row> m_rows;
    QList<Head> m_heads;
    bool m_mirrored = false;
    qreal m_reveal = 1.0;
    int m_hover = -1;
    int m_press = -1;
    QPointer<QVariantAnimation> m_anim;
};

