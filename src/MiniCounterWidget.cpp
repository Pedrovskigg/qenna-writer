#include "MiniCounterWidget.h"

#include "Theme.h"

#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int kLineFont   = 12;
constexpr int kColValue   = 14;
constexpr int kColLabel   = 10;
constexpr int kRingSize   = 28;
constexpr int kRingStroke = 3;
constexpr int kGap        = 6;
const QString kDot = QStringLiteral("·");

// Odômetro
constexpr int kOdoCellW   = 11;
constexpr int kOdoCellH   = 17;
constexpr int kOdoGap     = 2;
constexpr int kOdoGroup   = 3;   // espaço extra a cada 3 casas (milhar)
constexpr int kOdoMin     = 5;

// Semana mini
constexpr int kSparkBarW  = 4;
constexpr int kSparkGap   = 2;
constexpr int kSparkH     = 22;

const QStringList& miniStyles()
{
    static const QStringList s = {
        QStringLiteral("line"), QStringLiteral("columns"), QStringLiteral("ring"),
        QStringLiteral("pill"), QStringLiteral("ringlet"), QStringLiteral("odometer"),
        QStringLiteral("bricks-mini"), QStringLiteral("week-mini"), QStringLiteral("ruler"),
        QStringLiteral("card-mini"), QStringLiteral("tube"),
    };
    return s;
}

// Raio da forma: as pílulas são pílulas, a ficha é quase reta, o resto segue
// o painel. Tema de cantos retos (raio 0) deixa tudo reto, pílula inclusive.
qreal shapeRadius(const QString& style, const QRectF& r)
{
    const int panel = Theme::panelRadius();
    if (panel <= 0) return 0;
    if (style == QLatin1String("pill") || style == QLatin1String("ring")
        || style == QLatin1String("ringlet"))
        return r.height() / 2.0;
    if (style == QLatin1String("card-mini"))
        return qMin(panel, 6);
    return qMin<qreal>(panel, r.height() / 2.0);
}

// A IBM Plex Mono embarcada só tem Regular e Medium estáticos (peso fora
// deles vira negrito falso no FreeType).
QFont mono(int px, bool medium)
{
    QFont f(QStringLiteral("IBM Plex Mono"));
    f.setStyleHint(QFont::TypeWriter);
    f.setPixelSize(px);
    f.setWeight(medium ? QFont::Medium : QFont::Normal);
    return f;
}
}

MiniCounterWidget::MiniCounterWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("miniCounter"));
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TranslucentBackground, true);
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, qOverload<>(&QWidget::update));
}

bool MiniCounterWidget::isMiniStyle(const QString& key)
{
    return miniStyles().contains(key);
}

void MiniCounterWidget::setStyleKey(const QString& key)
{
    if (m_style == key) return;
    m_style = key;
    updateGeometry();
    update();
}

void MiniCounterWidget::setData(const Slot& s1, const Slot& s2, int goalPercent, bool showGoal,
                                const QVector<int>& week, int weekGoal)
{
    const bool sizeMayChange = s1.value != m_s1.value || s2.value != m_s2.value
        || s1.unit != m_s1.unit || s2.unit != m_s2.unit
        || s1.unitShort != m_s1.unitShort || s2.unitShort != m_s2.unitShort
        || showGoal != m_showGoal;
    m_s1 = s1;
    m_s2 = s2;
    m_pct = qBound(0, goalPercent, 100);
    m_showGoal = showGoal;
    m_week = week;
    m_weekGoal = weekGoal;
    if (sizeMayChange) updateGeometry();
    update();
}

QFont MiniCounterWidget::valueFont(int px) const
{
    QFont f = font();
    f.setPixelSize(px);
    f.setWeight(QFont::DemiBold);
    return f;
}

QFont MiniCounterWidget::labelFont(int px) const
{
    QFont f = font();
    f.setPixelSize(px);
    f.setWeight(QFont::Normal);
    return f;
}

// Largura do conteúdo "v1 unidade1 · v2 unidade2" (line, pill, ringlet, bricks-mini).
int MiniCounterWidget::lineContentWidth() const
{
    const QFontMetrics vf(valueFont(kLineFont));
    const QFontMetrics lf(labelFont(kLineFont));
    int w = vf.horizontalAdvance(m_s1.value);
    if (!m_s1.unit.isEmpty()) w += kGap + lf.horizontalAdvance(m_s1.unit);
    w += kGap + lf.horizontalAdvance(kDot) + kGap;
    w += vf.horizontalAdvance(m_s2.value);
    if (!m_s2.unit.isEmpty()) w += kGap + lf.horizontalAdvance(m_s2.unit);
    return w;
}

QString MiniCounterWidget::odometerDigits() const
{
    QString digits;
    for (const QChar c : m_s1.value) {
        if (c.isDigit()) digits.append(c);
        else if (c.isLetter()) return {};   // "12min": não é número puro
    }
    return digits;
}

int MiniCounterWidget::odometerCellsWidth() const
{
    const int n = qMax(kOdoMin, static_cast<int>(odometerDigits().size()));
    return n * kOdoCellW + (n - 1) * kOdoGap + ((n - 1) / 3) * kOdoGroup;
}

int MiniCounterWidget::stackedTextWidth() const
{
    return qMax(QFontMetrics(valueFont(kLineFont)).horizontalAdvance(m_s1.value),
                QFontMetrics(labelFont(kColLabel)).horizontalAdvance(m_s2.value));
}

void MiniCounterWidget::setScale(qreal scale)
{
    if (qFuzzyCompare(m_scale, scale)) return;
    m_scale = scale;
    updateGeometry();
    update();
}

QSize MiniCounterWidget::sizeHint() const
{
    const QSize b = baseSize();
    return { qRound(b.width() * m_scale), qRound(b.height() * m_scale) };
}

QSize MiniCounterWidget::baseSize() const
{
    const int lineH = QFontMetrics(valueFont(kLineFont)).height();
    const int stackH = QFontMetrics(valueFont(kLineFont)).height()
                     + QFontMetrics(labelFont(kColLabel)).height() - 2;

    if (m_style == QLatin1String("line"))
        return { lineContentWidth() + 20, lineH + 9 + (m_showGoal ? 2 : 0) };
    if (m_style == QLatin1String("pill"))
        return { lineContentWidth() + 24, lineH + 8 };
    if (m_style == QLatin1String("ringlet"))
        return { 6 + (m_showGoal ? 16 + 7 : 0) + lineContentWidth() + 12, qMax(lineH, 16) + 8 };
    if (m_style == QLatin1String("bricks-mini"))
        return { lineContentWidth() + 20, 5 + lineH + (m_showGoal ? 5 + 5 : 0) + 7 };
    if (m_style == QLatin1String("ring")) {
        const int left = m_showGoal ? 4 + kRingSize + 8 : 12;
        return { left + stackedTextWidth() + 12, kRingSize + 8 };
    }
    if (m_style == QLatin1String("week-mini")) {
        const int spark = 7 * kSparkBarW + 6 * kSparkGap;
        return { 9 + spark + 10 + stackedTextWidth() + 11, qMax(kSparkH, stackH) + 12 };
    }
    if (m_style == QLatin1String("tube"))
        return { (m_showGoal ? 4 + 9 : 12) + stackedTextWidth() + 12, stackH + 12 };
    if (m_style == QLatin1String("ruler")) {
        const QFontMetrics vf(valueFont(kLineFont)), lf(labelFont(kLineFont));
        int row = vf.horizontalAdvance(m_s1.value) + 4 + lf.horizontalAdvance(m_s1.unit)
                + 14 + vf.horizontalAdvance(m_s2.value);
        return { qMax(150, row + 20), 5 + lineH + (m_showGoal ? 3 + 12 : 0) + 4 };
    }
    if (m_style == QLatin1String("card-mini")) {
        const QFontMetrics lf(mono(11, false)), vf(mono(12, true));
        const int labels = qMax(lf.horizontalAdvance(m_s1.unitShort), lf.horizontalAdvance(m_s2.unitShort));
        const int values = qMax(vf.horizontalAdvance(m_s1.value), vf.horizontalAdvance(m_s2.value));
        return { 22 + labels + 14 + values + 10, 4 + 36 + 5 };
    }
    if (m_style == QLatin1String("odometer")) {
        if (odometerDigits().isEmpty())
            return { lineContentWidth() + 20, lineH + 9 + (m_showGoal ? 2 : 0) };
        const QFontMetrics vf(valueFont(kLineFont)), lf(labelFont(11));
        int w = 9 + odometerCellsWidth() + 8;
        if (!m_s1.unit.isEmpty()) w += lf.horizontalAdvance(m_s1.unit) + kGap;
        w += lf.horizontalAdvance(kDot) + kGap + vf.horizontalAdvance(m_s2.value);
        if (!m_s2.unit.isEmpty()) w += kGap + lf.horizontalAdvance(m_s2.unit);
        return { w + 9, 5 + kOdoCellH + 7 };
    }
    // columns
    const QFontMetrics vf(valueFont(kColValue));
    const QFontMetrics lf(labelFont(kColLabel));
    const int c1 = qMax(vf.horizontalAdvance(m_s1.value), lf.horizontalAdvance(m_s1.unitShort));
    const int c2 = qMax(vf.horizontalAdvance(m_s2.value), lf.horizontalAdvance(m_s2.unitShort));
    const int h = 6 + vf.height() + lf.height() - 2 + (m_showGoal ? 5 + 3 : 0) + 7;
    return { 10 + c1 + 10 + 1 + 10 + c2 + 10, h };
}

void MiniCounterWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(m_scale, m_scale);
    const qreal W = width() / m_scale;
    const qreal H = height() / m_scale;

    const QColor bg     = Theme::toColor(Theme::panelBackground());
    const QColor border = Theme::toColor(Theme::panelBorder());
    const QColor t1     = Theme::toColor(Theme::textBright());
    const QColor t2     = Theme::toColor(Theme::textMuted());
    const QColor acc    = Theme::toColor(Theme::accentDefault());
    QColor track = t1;  track.setAlpha(24);
    QColor accSoft = acc; accSoft.setAlpha(52);
    QColor accMid = acc; accMid.setAlpha(140);

    const QRectF shape = QRectF(0, 0, W, H).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = shapeRadius(m_style, shape);
    QPainterPath outline;
    outline.addRoundedRect(shape, radius, radius);

    p.fillPath(outline, bg);

    // "v1 unidade1 · v2 unidade2" a partir de x, centrado em cy.
    auto drawLine = [&](qreal x, qreal cy) {
        const QFont vfont = valueFont(kLineFont);
        const QFont lfont = labelFont(kLineFont);
        const QFontMetricsF vm(vfont), lm(lfont);
        const qreal base = cy + (vm.ascent() - vm.descent()) / 2.0;
        auto put = [&](const QString& text, const QFont& f, const QColor& c, const QFontMetricsF& m) {
            p.setFont(f);
            p.setPen(c);
            p.drawText(QPointF(x, base), text);
            x += m.horizontalAdvance(text);
        };
        put(m_s1.value, vfont, t1, vm);
        if (!m_s1.unit.isEmpty()) { x += kGap; put(m_s1.unit, lfont, t2, lm); }
        x += kGap;
        QColor dot = t2; dot.setAlphaF(dot.alphaF() * 0.6);
        put(kDot, lfont, dot, lm);
        x += kGap;
        put(m_s2.value, vfont, t1, vm);
        if (!m_s2.unit.isEmpty()) { x += kGap; put(m_s2.unit, lfont, t2, lm); }
    };

    // Valor 1 (12px, forte) sobre valor 2 (10px, apagado), centrados em cy.
    auto drawStacked = [&](qreal x, qreal cy) {
        const QFont vfont = valueFont(kLineFont);
        const QFont lfont = labelFont(kColLabel);
        const QFontMetricsF vm(vfont), lm(lfont);
        const qreal top = cy - (vm.height() + lm.height() - 2) / 2.0;
        p.setFont(vfont); p.setPen(t1);
        p.drawText(QPointF(x, top + vm.ascent()), m_s1.value);
        p.setFont(lfont); p.setPen(t2);
        p.drawText(QPointF(x, top + vm.height() - 2 + lm.ascent()), m_s2.value);
    };

    // Fio de meta colado na base, recortado pela forma (line, odometer).
    auto bottomWire = [&](qreal h) {
        p.save();
        p.setClipPath(outline);
        const QRectF bar(shape.left(), shape.bottom() - h, shape.width(), h);
        p.fillRect(bar, track);
        p.fillRect(QRectF(bar.left(), bar.top(), bar.width() * m_pct / 100.0, h), acc);
        p.restore();
    };

    if (m_style == QLatin1String("pill")) {
        if (m_showGoal && m_pct > 0) {
            p.save();
            p.setClipPath(outline);
            p.fillRect(QRectF(shape.left(), shape.top(), shape.width() * m_pct / 100.0, shape.height()), accSoft);
            p.restore();
        }
        drawLine(12, H / 2.0);
    } else if (m_style == QLatin1String("line")
               || (m_style == QLatin1String("odometer") && odometerDigits().isEmpty())) {
        const qreal barH = m_showGoal ? 2 : 0;
        drawLine(10, (H - barH) / 2.0 + 0.5);
        if (m_showGoal) bottomWire(barH);
    } else if (m_style == QLatin1String("ringlet")) {
        qreal x = 12;
        if (m_showGoal) {
            const QRectF ring(6 + 1.25, (H - 16) / 2.0 + 1.25, 16 - 2.5, 16 - 2.5);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(track, 2.5));
            p.drawEllipse(ring);
            if (m_pct > 0) {
                p.setPen(QPen(acc, 2.5, Qt::SolidLine, Qt::RoundCap));
                p.drawArc(ring, 90 * 16, -static_cast<int>(360 * 16 * m_pct / 100.0));
            }
            x = 6 + 16 + 7;
        }
        drawLine(x, H / 2.0);
    } else if (m_style == QLatin1String("odometer")) {
        const QString digits = odometerDigits();
        const int n = qMax(kOdoMin, static_cast<int>(digits.size()));
        const QString padded = QString(n - digits.size(), QLatin1Char('0')) + digits;
        const int firstSig = [&] {
            for (int i = 0; i < padded.size(); ++i) if (padded.at(i) != QLatin1Char('0')) return i;
            return n - 1;   // tudo zero: só a última casa acesa
        }();
        QColor cell = Theme::toColor(Theme::appBackground()); cell.setAlpha(150);
        QColor lead = t2; lead.setAlphaF(lead.alphaF() * 0.5);
        const QFont df = mono(12, true);
        const QFontMetricsF dm(df);
        const qreal cellTop = 5;
        qreal x = 9;
        p.setFont(df);
        for (int i = 0; i < n; ++i) {
            // Espaço de milhar contado da direita: 196 384.
            if (i > 0 && (n - i) % 3 == 0) x += kOdoGroup;
            const QRectF c(x, cellTop, kOdoCellW, kOdoCellH);
            QPainterPath cp;
            cp.addRoundedRect(c, 3, 3);
            p.fillPath(cp, cell);
            p.setPen(i < firstSig ? lead : t1);
            const QString ch = padded.mid(i, 1);
            p.drawText(QPointF(c.center().x() - dm.horizontalAdvance(ch) / 2.0,
                               c.center().y() + (dm.ascent() - dm.descent()) / 2.0), ch);
            x += kOdoCellW + kOdoGap;
        }
        x += 8 - kOdoGap;
        const qreal cy = cellTop + kOdoCellH / 2.0;
        const QFont lf = labelFont(11), vf = valueFont(kLineFont);
        const QFontMetricsF lm(lf), vm(vf);
        const qreal lb = cy + (lm.ascent() - lm.descent()) / 2.0;
        const qreal vb = cy + (vm.ascent() - vm.descent()) / 2.0;
        auto put = [&](const QString& s, const QFont& f, const QFontMetricsF& m, const QColor& c, qreal base) {
            p.setFont(f); p.setPen(c);
            p.drawText(QPointF(x, base), s);
            x += m.horizontalAdvance(s);
        };
        if (!m_s1.unit.isEmpty()) { put(m_s1.unit, lf, lm, t2, lb); x += kGap; }
        QColor dot = t2; dot.setAlphaF(dot.alphaF() * 0.6);
        put(kDot, lf, lm, dot, lb);
        x += kGap;
        put(m_s2.value, vf, vm, t1, vb);
        if (!m_s2.unit.isEmpty()) { x += kGap; put(m_s2.unit, lf, lm, t2, lb); }
        if (m_showGoal) bottomWire(2);
    } else if (m_style == QLatin1String("bricks-mini")) {
        const qreal lineH = QFontMetricsF(valueFont(kLineFont)).height();
        drawLine(10, 5 + lineH / 2.0);
        if (m_showGoal) {
            const qreal top = 5 + lineH + 5, w = W - 20;
            const qreal gap = 2, bw = (w - gap * 9) / 10;
            const int lit = m_pct / 10;
            p.setPen(Qt::NoPen);
            for (int i = 0; i < 10; ++i) {
                p.setBrush(i < lit ? acc : track);
                p.drawRoundedRect(QRectF(10 + i * (bw + gap), top, bw, 5), 1, 1);
            }
        }
    } else if (m_style == QLatin1String("week-mini")) {
        const qreal bottom = (H + kSparkH) / 2.0;
        int maxV = qMax(1, static_cast<int>(m_weekGoal * 1.25));
        for (int v : m_week) maxV = qMax(maxV, v);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 7; ++i) {
            const int v = i < m_week.size() ? m_week.at(i) : 0;
            const bool today = i == 6;
            const qreal h = qMax<qreal>(2, kSparkH * v / maxV);
            QColor c = track;
            if (today) c = acc;
            else if (m_weekGoal > 0 && v >= m_weekGoal) c = accMid;
            p.setBrush(c);
            p.drawRoundedRect(QRectF(9 + i * (kSparkBarW + kSparkGap), bottom - h, kSparkBarW, h), 1, 1);
        }
        if (m_showGoal && m_weekGoal > 0) {
            QColor gc = t2; gc.setAlpha(115);
            QPen dash(gc, 1, Qt::DashLine);
            dash.setDashPattern({ 2, 2 });
            p.setPen(dash);
            const qreal gy = bottom - kSparkH * m_weekGoal / maxV;
            const qreal sparkW = 7 * kSparkBarW + 6 * kSparkGap;
            p.drawLine(QPointF(7, gy + 0.5), QPointF(9 + sparkW + 2, gy + 0.5));
        }
        drawStacked(9 + 7 * kSparkBarW + 6 * kSparkGap + 10, H / 2.0);
    } else if (m_style == QLatin1String("ruler")) {
        const QFont vf = valueFont(kLineFont), lf = labelFont(kLineFont);
        const QFontMetricsF vm(vf), lm(lf);
        const qreal base = 5 + vm.ascent();
        p.setFont(vf); p.setPen(t1);
        p.drawText(QPointF(10, base), m_s1.value);
        p.setFont(lf); p.setPen(t2);
        p.drawText(QPointF(10 + vm.horizontalAdvance(m_s1.value) + 4, base), m_s1.unit);
        p.setFont(vf); p.setPen(t1);
        p.drawText(QPointF(W - 10 - vm.horizontalAdvance(m_s2.value), base), m_s2.value);
        if (m_showGoal) {
            const qreal left = 10, w = W - 20;
            const qreal rb = 5 + vm.height() + 3 + 12;   // base da régua
            QColor tick = t2; tick.setAlpha(115);
            p.setPen(QPen(tick, 1));
            for (int i = 0; i <= 10; ++i) {
                const qreal x = qRound(left + w * i / 10.0) + 0.5;
                p.drawLine(QPointF(x, rb - 5), QPointF(x, rb));
            }
            p.fillRect(QRectF(left, rb - 2, w * m_pct / 100.0, 2), acc);
            const qreal mx = left + w * m_pct / 100.0;
            QPainterPath tri;
            tri.moveTo(mx - 4, rb - 8);
            tri.lineTo(mx + 4, rb - 8);
            tri.lineTo(mx, rb - 3);
            tri.closeSubpath();
            p.fillPath(tri, acc);
        }
    } else if (m_style == QLatin1String("card-mini")) {
        QColor rule = t1; rule.setAlpha(18);
        QColor margin = Theme::toColor(Theme::accentDanger()); margin.setAlpha(100);
        p.save();
        p.setClipPath(outline);
        p.setPen(QPen(rule, 1));
        for (int k = 1; k <= 2; ++k)
            p.drawLine(QPointF(0, 4 + 18 * k - 0.5), QPointF(W, 4 + 18 * k - 0.5));
        p.setPen(QPen(margin, 1));
        p.drawLine(QPointF(14.5, 0), QPointF(14.5, H));
        p.restore();
        const QFont lf = mono(11, false), vf = mono(12, true);
        const QFontMetricsF lm(lf), vm(vf);
        auto row = [&](int k, const QString& label, const QString& value) {
            const qreal bottom = 4 + 18 * (k + 1);
            p.setFont(lf); p.setPen(t2);
            p.drawText(QPointF(22, bottom - 5), label);
            p.setFont(vf); p.setPen(t1);
            p.drawText(QPointF(W - 10 - vm.horizontalAdvance(value), bottom - 5), value);
        };
        row(0, m_s1.unitShort, m_s1.value);
        row(1, m_s2.unitShort, m_s2.value);
    } else if (m_style == QLatin1String("tube")) {
        qreal x = 12;
        if (m_showGoal) {
            p.save();
            p.setClipPath(outline);
            p.fillRect(QRectF(0, 0, 4, H), track);
            const qreal h = H * m_pct / 100.0;
            p.fillRect(QRectF(0, H - h, 4, h), acc);
            p.restore();
            x = 4 + 9;
        }
        drawStacked(x, H / 2.0);
    } else if (m_style == QLatin1String("ring")) {
        qreal x = 12;
        if (m_showGoal) {
            const QRectF ring(4 + kRingStroke / 2.0, (H - kRingSize) / 2.0 + kRingStroke / 2.0,
                              kRingSize - kRingStroke, kRingSize - kRingStroke);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(track, kRingStroke));
            p.drawEllipse(ring);
            if (m_pct > 0) {
                p.setPen(QPen(acc, kRingStroke, Qt::SolidLine, Qt::RoundCap));
                p.drawArc(ring, 90 * 16, -static_cast<int>(360 * 16 * m_pct / 100.0));
            }
            x = 4 + kRingSize + 8;
        }
        drawStacked(x, H / 2.0);
    } else { // columns
        const QFont vfont = valueFont(kColValue);
        const QFont lfont = labelFont(kColLabel);
        const QFontMetricsF vm(vfont), lm(lfont);
        const qreal c1 = qMax(vm.horizontalAdvance(m_s1.value), lm.horizontalAdvance(m_s1.unitShort));
        const qreal top = 6;
        auto column = [&](qreal x, const Slot& s) {
            p.setFont(vfont); p.setPen(t1);
            p.drawText(QPointF(x, top + vm.ascent()), s.value);
            p.setFont(lfont); p.setPen(t2);
            p.drawText(QPointF(x, top + vm.height() - 2 + lm.ascent()), s.unitShort);
        };
        column(10, m_s1);
        const qreal sepX = 10 + c1 + 10 + 0.5;
        p.setPen(QPen(border, 1));
        p.drawLine(QPointF(sepX, top + 1), QPointF(sepX, top + vm.height() + lm.height() - 3));
        column(sepX + 0.5 + 10, m_s2);
        if (m_showGoal) {
            const QRectF bar(10, H - 7 - 3, W - 20, 3);
            p.setPen(Qt::NoPen);
            p.setBrush(track);
            p.drawRoundedRect(bar, 1.5, 1.5);
            if (m_pct > 0) {
                p.setBrush(acc);
                p.drawRoundedRect(QRectF(bar.left(), bar.top(), qMax<qreal>(3, bar.width() * m_pct / 100.0), 3), 1.5, 1.5);
            }
        }
    }

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(border, 1));
    p.drawPath(outline);
}

// Sem aceitar o press, o release vai pro pai e o clique nunca chega aqui.
void MiniCounterWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) { e->accept(); return; }
    QWidget::mousePressEvent(e);
}

void MiniCounterWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint())) {
        emit clicked();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void MiniCounterWidget::contextMenuEvent(QContextMenuEvent* e)
{
    emit contextMenuRequested(e->globalPos());
}
