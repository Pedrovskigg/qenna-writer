#include "CounterFaceWidget.h"

#include "Theme.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {
constexpr int kPad = 12;          // respiro lateral, igual ao m_body clássico
constexpr int kTop = 10;
constexpr int kHeaderH = 22;
constexpr int kBricks = 20;

const QStringList& faceStyles()
{
    static const QStringList s = {
        QStringLiteral("classic"), QStringLiteral("face-ring"), QStringLiteral("face-brickring"),
        QStringLiteral("face-ringbricks"), QStringLiteral("face-week"),
        QStringLiteral("face-bricks"), QStringLiteral("face-card"),
    };
    return s;
}

struct Palette {
    QColor t1, t2, border, inset, insetBorder, track, acc, accMid, rule, margin;
};

Palette themePalette()
{
    Palette p;
    p.t1 = Theme::toColor(Theme::textBright());
    p.t2 = Theme::toColor(Theme::textMuted());
    p.border = Theme::toColor(Theme::panelBorder());
    p.inset = Theme::toColor(Theme::appBackground());
    p.inset.setAlpha(150);
    p.insetBorder = Theme::toColor(Theme::subtleBorder());
    p.track = p.t1; p.track.setAlpha(22);
    p.acc = Theme::toColor(Theme::accentDefault());
    p.accMid = p.acc; p.accMid.setAlpha(140);
    p.rule = p.t1; p.rule.setAlpha(16);
    p.margin = Theme::toColor(Theme::accentDanger()); p.margin.setAlpha(95);
    return p;
}

QFont px(const QFont& base, qreal size, QFont::Weight w = QFont::Normal)
{
    QFont f = base;
    f.setPixelSize(qRound(size));
    f.setWeight(w);
    return f;
}

// Fonte da ficha: a IBM Plex Mono embarcada só tem Regular e Medium estáticos
// (peso fora deles vira negrito falso no FreeType), então nada de Bold aqui.
QFont mono(qreal size, bool medium)
{
    QFont f(QStringLiteral("IBM Plex Mono"));
    f.setStyleHint(QFont::TypeWriter);
    f.setPixelSize(qRound(size));
    f.setWeight(medium ? QFont::Medium : QFont::Normal);
    return f;
}

void text(QPainter& p, const QFont& f, const QColor& c, qreal x, qreal baseline, const QString& s)
{
    p.setFont(f);
    p.setPen(c);
    p.drawText(QPointF(x, baseline), s);
}

void textRight(QPainter& p, const QFont& f, const QColor& c, qreal right, qreal baseline, const QString& s)
{
    p.setFont(f);
    p.setPen(c);
    p.drawText(QPointF(right - QFontMetricsF(f).horizontalAdvance(s), baseline), s);
}

void textCenter(QPainter& p, const QFont& f, const QColor& c, qreal cx, qreal baseline, const QString& s)
{
    p.setFont(f);
    p.setPen(c);
    p.drawText(QPointF(cx - QFontMetricsF(f).horizontalAdvance(s) / 2.0, baseline), s);
}

// Baseline que centraliza verticalmente um texto de fonte f em cy.
qreal midBaseline(const QFont& f, qreal cy)
{
    const QFontMetricsF m(f);
    return cy + (m.ascent() - m.descent()) / 2.0;
}

void brickRow(QPainter& p, const QRectF& r, int lit, const Palette& pal)
{
    const qreal gap = 2;
    const qreal w = (r.width() - gap * (kBricks - 1)) / kBricks;
    p.setPen(Qt::NoPen);
    for (int i = 0; i < kBricks; ++i) {
        p.setBrush(i < lit ? pal.acc : pal.track);
        p.drawRoundedRect(QRectF(r.left() + i * (w + gap), r.top(), w, r.height()), 1.5, 1.5);
    }
}
}

CounterFaceWidget::CounterFaceWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("counterFace"));
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, qOverload<>(&QWidget::update));
}

bool CounterFaceWidget::isFaceStyle(const QString& key)
{
    return faceStyles().contains(key);
}

void CounterFaceWidget::setStyleKey(const QString& key)
{
    if (m_style == key) return;
    m_style = key;
    updateGeometry();
    update();
}

void CounterFaceWidget::setData(const Data& d)
{
    const bool resize = m_style == QLatin1String("classic")
        && (d.fullLabel1 != m_d.fullLabel1 || d.fullLabel2 != m_d.fullLabel2 || d.showGoal != m_d.showGoal);
    m_d = d;
    if (resize) updateGeometry();
    update();
}

void CounterFaceWidget::setScale(qreal scale)
{
    if (qFuzzyCompare(m_scale, scale)) return;
    m_scale = scale;
    updateGeometry();
    update();
}

QSize CounterFaceWidget::sizeHint() const
{
    const QSize b = baseSize();
    return { qRound(b.width() * m_scale), qRound(b.height() * m_scale) };
}

namespace {
// Geometria dos cards do clássico: dois de (230 - 8) / 2 de largura, com o
// título quebrando linha como o QLabel com wordWrap fazia.
constexpr qreal kClassicCardW = 111;
constexpr qreal kClassicCardPadX = 12;
constexpr qreal kClassicCardPadY = 8;

qreal classicTitleHeight(const QFont& f, const QString& t)
{
    return QFontMetricsF(f).boundingRect(QRectF(0, 0, kClassicCardW - 2 * kClassicCardPadX, 1000),
                                         Qt::TextWordWrap, t).height();
}
}

QSize CounterFaceWidget::baseSize() const
{
    if (m_style == QLatin1String("classic")) {
        const QFont base = font();
        const QFont tf = px(base, 11), vf = px(base, 15, QFont::DemiBold);
        const qreal titleH = qMax(classicTitleHeight(tf, m_d.fullLabel1), classicTitleHeight(tf, m_d.fullLabel2));
        const qreal cardH = kClassicCardPadY + titleH + 2 + QFontMetricsF(vf).height() + kClassicCardPadY;
        qreal h = kTop + kHeaderH + 8 + cardH;
        if (m_d.showGoal) h += 8 + 5 + 8 + QFontMetricsF(px(base, 11)).height();
        return { 254, qCeil(h + 12) };
    }
    int h = 142;
    if (m_style == QLatin1String("face-week"))            h = 160;
    else if (m_style == QLatin1String("face-bricks"))     h = 134;
    else if (m_style == QLatin1String("face-ringbricks")) h = 148;
    else if (m_style == QLatin1String("face-card"))       h = 124;
    return { 254, h };
}

void CounterFaceWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.scale(m_scale, m_scale);
    const Palette pal = themePalette();
    const QFont base = font();
    const qreal W = width() / m_scale;
    const qreal H = height() / m_scale;
    const qreal right = W - kPad;
    const bool card = m_style == QLatin1String("face-card");

    // ---- Ficha: pauta + margem, sangrando até a borda do painel ----
    const qreal cardLeft = 40;    // texto começa depois da margem
    const qreal cardHeaderH = 30;
    if (card) {
        const qreal r = qMax(0, Theme::panelRadius() - 1);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(0, 0, W, H), r, r);
        p.save();
        p.setClipPath(clip);
        p.setPen(QPen(pal.rule, 1));
        for (qreal y = cardHeaderH + 24; y < H; y += 24)
            p.drawLine(QPointF(0, y + 0.5), QPointF(W, y + 0.5));
        p.setPen(QPen(pal.margin, 1));
        p.drawLine(QPointF(30.5, 0), QPointF(30.5, H));
        p.drawLine(QPointF(0, cardHeaderH + 0.5), QPointF(W, cardHeaderH + 0.5));
        p.restore();
    }

    // ---- Cabeçalho: título + "Estatísticas ›" ----
    {
        const qreal cy = card ? cardHeaderH / 2.0 + 1 : kTop + kHeaderH / 2.0;
        const QFont tf = card ? mono(12.5, true) : px(base, 12, QFont::DemiBold);
        text(p, tf, pal.t1, card ? cardLeft : kPad, midBaseline(tf, cy), m_d.title);
        const QFont sf = px(base, 11);
        const qreal sw = QFontMetricsF(sf).horizontalAdvance(m_d.statsText);
        m_statsRect = QRectF(right - sw - 4, cy - 11, sw + 8, 22);
        textRight(p, sf, m_statsHover ? pal.t1 : pal.t2, right, midBaseline(sf, cy), m_d.statsText);
    }

    const QFont labelF = px(base, 11);
    const QFont valueF = px(base, 15, QFont::DemiBold);
    const QFont footF = px(base, 11);

    // Duas linhas "rótulo ........ valor" centradas em cy, com filete no meio.
    auto statRows = [&](qreal x0, qreal cy, qreal gapHalf) {
        const qreal y1 = cy - gapHalf, y2 = cy + gapHalf;
        text(p, labelF, pal.t2, x0, midBaseline(labelF, y1), m_d.label1);
        textRight(p, valueF, pal.t1, right, midBaseline(valueF, y1), m_d.value1);
        p.setPen(QPen(pal.insetBorder, 1));
        p.drawLine(QPointF(x0, cy + 0.5), QPointF(right, cy + 0.5));
        text(p, labelF, pal.t2, x0, midBaseline(labelF, y2), m_d.label2);
        textRight(p, valueF, pal.t1, right, midBaseline(valueF, y2), m_d.value2);
    };

    // Anel liso ou em tijolinhos, com o % e "da meta" dentro.
    auto ring = [&](QPointF c, qreal r, qreal stroke, bool bricks, qreal pctSize) {
        const QRectF box(c.x() - r, c.y() - r, 2 * r, 2 * r);
        p.setBrush(Qt::NoBrush);
        if (bricks) {
            const qreal step = 360.0 / kBricks, gap = 3.2;
            const int lit = m_d.pct / 5;
            for (int i = 0; i < kBricks; ++i) {
                p.setPen(QPen(i < lit ? pal.acc : pal.track, stroke, Qt::SolidLine, Qt::FlatCap));
                const qreal start = 90 - (i * step + gap / 2);          // Qt: 0° às 3h, anti-horário
                p.drawArc(box, qRound(start * 16), qRound(-(step - gap) * 16));
            }
        } else {
            p.setPen(QPen(pal.track, stroke));
            p.drawEllipse(box);
            if (m_d.pct > 0) {
                p.setPen(QPen(pal.acc, stroke, Qt::SolidLine, Qt::RoundCap));
                p.drawArc(box, 90 * 16, -qRound(360.0 * 16 * m_d.pct / 100.0));
            }
        }
        const QFont pf = px(base, pctSize, QFont::DemiBold);
        const QFont cf = px(base, 9);
        const QString pct = QString::number(m_d.pct) + QLatin1Char('%');
        textCenter(p, pf, pal.t1, c.x(), c.y() + 1, pct);
        textCenter(p, cf, pal.t2, c.x(), c.y() + 1 + QFontMetricsF(cf).ascent() + 1, m_d.pctCaption);
    };

    const qreal bodyTop = kTop + kHeaderH + 8;

    if (m_style == QLatin1String("classic")) {
        const QFont tf = px(base, 11);
        const qreal titleH = qMax(classicTitleHeight(tf, m_d.fullLabel1), classicTitleHeight(tf, m_d.fullLabel2));
        const qreal cardH = kClassicCardPadY + titleH + 2 + QFontMetricsF(valueF).height() + kClassicCardPadY;
        const qreal cr = Theme::controlRadius();
        auto cardAt = [&](qreal x, const QString& title, const QString& value) {
            const QRectF c(x, bodyTop, kClassicCardW, cardH);
            QPainterPath cp;
            cp.addRoundedRect(c.adjusted(0.5, 0.5, -0.5, -0.5), cr, cr);
            p.fillPath(cp, pal.inset);
            p.setPen(QPen(pal.insetBorder, 1));
            p.drawPath(cp);
            p.setFont(tf);
            p.setPen(pal.t2);
            p.drawText(QRectF(x + kClassicCardPadX, bodyTop + kClassicCardPadY,
                              kClassicCardW - 2 * kClassicCardPadX, titleH),
                       Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, title);
            text(p, valueF, pal.t1, x + kClassicCardPadX,
                 bodyTop + kClassicCardPadY + titleH + 2 + QFontMetricsF(valueF).ascent(), value);
        };
        cardAt(kPad, m_d.fullLabel1, m_d.value1);
        cardAt(kPad + kClassicCardW + 8, m_d.fullLabel2, m_d.value2);
        if (m_d.showGoal) {
            const QRectF bar(kPad, bodyTop + cardH + 8, right - kPad, 5);
            QPainterPath bp;
            bp.addRoundedRect(bar.adjusted(0.5, 0.5, -0.5, -0.5), 2.5, 2.5);
            p.fillPath(bp, pal.inset);
            p.setPen(QPen(pal.insetBorder, 1));
            p.drawPath(bp);
            if (m_d.pct > 0) {
                QPainterPath fp;
                fp.addRoundedRect(QRectF(bar.left() + 1, bar.top() + 1,
                                         qMax<qreal>(3, (bar.width() - 2) * m_d.pct / 100.0), bar.height() - 2), 2, 2);
                p.fillPath(fp, pal.acc);
            }
            textCenter(p, footF, pal.t2, W / 2.0, bar.bottom() + 8 + QFontMetricsF(footF).ascent(), m_d.goalLine);
        }
    } else if (m_style == QLatin1String("face-ring") || m_style == QLatin1String("face-brickring")) {
        const bool bricks = m_style == QLatin1String("face-brickring");
        const qreal size = bricks ? 68 : 66;
        const QPointF c(kPad + size / 2.0, bodyTop + 2 + size / 2.0);
        ring(c, bricks ? 29 : 28, bricks ? 6 : 5, bricks, 15);
        statRows(kPad + size + 14, c.y(), 13);
        const qreal footY = bodyTop + 2 + size + 8;
        textCenter(p, footF, pal.t2, W / 2.0, footY + QFontMetricsF(footF).ascent(), m_d.goalLine);
    } else if (m_style == QLatin1String("face-ringbricks")) {
        const qreal size = 60;
        const QPointF c(kPad + size / 2.0, bodyTop + 1 + size / 2.0);
        ring(c, 25, 5, false, 14);
        statRows(kPad + size + 14, c.y(), 12);
        const qreal by = bodyTop + 1 + size + 8;
        // Um tijolo por dia, o último é hoje: acende quem bateu a meta; hoje
        // fica só contornado até bater.
        const qreal gap = 2, w = (right - kPad - gap * (kBricks - 1)) / kBricks;
        p.setPen(Qt::NoPen);
        for (int i = 0; i < kBricks && i < m_d.days.size(); ++i) {
            const QRectF b(kPad + i * (w + gap), by, w, 7);
            const bool on = m_d.days.at(i);
            if (i == m_d.days.size() - 1 && !on) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(pal.acc, 1));
                p.drawRoundedRect(b.adjusted(0.5, 0.5, -0.5, -0.5), 1.5, 1.5);
                p.setPen(Qt::NoPen);
            } else {
                p.setBrush(on ? pal.acc : pal.track);
                p.drawRoundedRect(b, 1.5, 1.5);
            }
        }
        const qreal fb = by + 7 + 6 + QFontMetricsF(footF).ascent();
        text(p, footF, pal.t2, kPad, fb, m_d.goalLine);
        textRight(p, footF, pal.t1, right, fb, m_d.daysCaption);
    } else if (m_style == QLatin1String("face-week")) {
        // Linha: "702 palavras   38min hoje"
        const QFont uf = px(base, 10.5);
        const qreal lb = bodyTop + QFontMetricsF(valueF).ascent();
        qreal x = kPad;
        auto pair = [&](const QString& v, const QString& u) {
            text(p, valueF, pal.t1, x, lb, v);
            x += QFontMetricsF(valueF).horizontalAdvance(v) + 4;
            text(p, uf, pal.t2, x, lb, u);
            x += QFontMetricsF(uf).horizontalAdvance(u) + 14;
        };
        pair(m_d.value1, m_d.unit1);
        pair(m_d.value2, m_d.unit2);

        const qreal chartTop = bodyTop + 26, chartH = 44;
        const int n = qMax(1, m_d.week.size());
        const qreal gap = 5, cw = (right - kPad - gap * (n - 1)) / n;
        int maxV = qMax(1, static_cast<int>(m_d.weekGoal * 1.25));
        for (int v : m_d.week) maxV = qMax(maxV, v);
        const qreal bottom = chartTop + chartH;
        p.setPen(Qt::NoPen);
        for (int i = 0; i < m_d.week.size(); ++i) {
            const bool today = i == m_d.week.size() - 1;
            const int v = m_d.week.at(i);
            const qreal h = qMax<qreal>(2, chartH * v / maxV);
            const QRectF col(kPad + i * (cw + gap), bottom - h, cw, h);
            QColor c = pal.track;
            if (today) c = pal.acc;
            else if (m_d.weekGoal > 0 && v >= m_d.weekGoal) c = pal.accMid;
            QPainterPath path;
            path.addRoundedRect(col, 3, 3);
            p.fillPath(path, c);
        }
        if (m_d.weekGoal > 0) {
            const qreal gy = bottom - chartH * m_d.weekGoal / maxV;
            QColor gc = pal.t2; gc.setAlpha(115);
            QPen dash(gc, 1, Qt::DashLine);
            dash.setDashPattern({ 3, 3 });
            p.setPen(dash);
            p.drawLine(QPointF(kPad, gy + 0.5), QPointF(right, gy + 0.5));
        }
        const QFont df = px(base, 9);
        const QFont dfToday = px(base, 9, QFont::DemiBold);
        const qreal db = bottom + 3 + QFontMetricsF(df).ascent();
        for (int i = 0; i < m_d.weekDays.size() && i < n; ++i) {
            const bool today = i == m_d.weekDays.size() - 1;
            textCenter(p, today ? dfToday : df, today ? pal.t1 : pal.t2,
                       kPad + i * (cw + gap) + cw / 2.0, db, m_d.weekDays.at(i));
        }
        textCenter(p, footF, pal.t2, W / 2.0, db + 8 + QFontMetricsF(footF).ascent(), m_d.goalLine);
    } else if (m_style == QLatin1String("face-bricks")) {
        const QRectF box(kPad, bodyTop, right - kPad, 44);
        const qreal cr = Theme::controlRadius();
        QPainterPath bp;
        bp.addRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), cr, cr);
        p.fillPath(bp, pal.inset);
        p.setPen(QPen(pal.insetBorder, 1));
        p.drawPath(bp);
        const qreal mid = box.center().x();
        p.drawLine(QPointF(mid + 0.5, box.top() + 1), QPointF(mid + 0.5, box.bottom() - 1));
        const qreal tb = box.top() + 7 + QFontMetricsF(labelF).ascent();
        const qreal vb = tb + 3 + QFontMetricsF(valueF).ascent();
        text(p, labelF, pal.t2, box.left() + 11, tb, m_d.label1);
        text(p, valueF, pal.t1, box.left() + 11, vb, m_d.value1);
        text(p, labelF, pal.t2, mid + 11, tb, m_d.label2);
        text(p, valueF, pal.t1, mid + 11, vb, m_d.value2);
        const qreal by = box.bottom() + 9;
        brickRow(p, QRectF(kPad, by, right - kPad, 7), m_d.pct / 5, pal);
        const qreal fb = by + 7 + 6 + QFontMetricsF(footF).ascent();
        text(p, footF, pal.t2, kPad, fb, m_d.goalLine);
        textRight(p, footF, pal.t1, right, fb, m_d.fraction);
    } else if (card) {
        const QFont lf = mono(11.5, false);
        const QFont vf = mono(15, true);
        auto row = [&](int k, const QString& l, const QString& v) {
            const qreal bottom = cardHeaderH + 24 * (k + 1);
            text(p, lf, pal.t2, cardLeft, bottom - 7, l.toLower());
            textRight(p, vf, pal.t1, right, bottom - 6, v);
        };
        row(0, m_d.label1, m_d.value1);
        row(1, m_d.label2, m_d.value2);
        const qreal my = cardHeaderH + 48 + 8;
        const QRectF meter(cardLeft, my, right - cardLeft, 4);
        p.fillRect(meter, pal.track);
        p.fillRect(QRectF(meter.left(), meter.top(), meter.width() * m_d.pct / 100.0, 4), pal.acc);
        text(p, mono(11, false), pal.t2, cardLeft, my + 4 + 16, m_d.goalLine);
    }
}

void CounterFaceWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && m_statsRect.contains(e->position() / m_scale)) {
        m_statsPressed = true;
        e->accept();
        return;
    }
    e->ignore(); // sobe pro m_body, que alterna o modo full
}

void CounterFaceWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_statsPressed) {
        m_statsPressed = false;
        if (m_statsRect.contains(e->position() / m_scale)) emit statsClicked();
        e->accept();
        return;
    }
    e->ignore();
}

void CounterFaceWidget::mouseMoveEvent(QMouseEvent* e)
{
    const bool hover = m_statsRect.contains(e->position() / m_scale);
    if (hover != m_statsHover) {
        m_statsHover = hover;
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void CounterFaceWidget::leaveEvent(QEvent* e)
{
    if (m_statsHover) {
        m_statsHover = false;
        update();
    }
    QWidget::leaveEvent(e);
}
