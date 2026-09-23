#define _USE_MATH_DEFINES
#include "TimelineTracksView.h"

#include "Theme.h"

#include <QCoreApplication>

#include <QApplication>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLineF>
#include <QPair>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

using namespace Tracks;

namespace {

constexpr qreal kKnotR  = 7.5;  // bolinha de capítulo/cena (15px)
constexpr qreal kNoteR  = 5.5;  // bolinha de evento manual (11px)
constexpr qreal kCastTop = 30.0;
constexpr qreal kCastRow = 28.0;

qreal days(qreal chronoMinutes) { return chronoMinutes / 1440.0; }
} // namespace

// ═════════════════════════════════════════════════════════════════════════════
namespace TracksDetail {

// ── Divisória quase invisível entre linhas e elenco ─────────────────────────
class SplitHandle : public QSplitterHandle {
public:
    SplitHandle(Qt::Orientation o, QSplitter* parent) : QSplitterHandle(o, parent)
    {
        setMouseTracking(true);
        setCursor(Qt::SplitVCursor);
        setToolTip(QCoreApplication::translate("TimelineTracks", "Arraste para dividir o espaço"));
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const Palette pal = Palette::current();
        p.fillRect(rect(), pal.page);
        p.fillRect(QRect(0, 0, 180, height()), pal.panel);
        p.fillRect(QRect(179, 0, 1, height()), pal.border);
        const bool hot = m_hover || m_down;
        const qreal lh = hot ? 2.0 : 1.0;
        p.fillRect(QRectF(0, height() / 2.0 - lh / 2.0, width(), lh),
                   hot ? alpha(pal.accent, 0.7) : pal.faint);
    }
    void enterEvent(QEnterEvent* e) override { m_hover = true; update(); QSplitterHandle::enterEvent(e); }
    void leaveEvent(QEvent* e) override { m_hover = false; update(); QSplitterHandle::leaveEvent(e); }
    void mousePressEvent(QMouseEvent* e) override { m_down = true; update(); QSplitterHandle::mousePressEvent(e); }
    void mouseReleaseEvent(QMouseEvent* e) override { m_down = false; update(); QSplitterHandle::mouseReleaseEvent(e); }
private:
    bool m_hover = false, m_down = false;
};

class Splitter : public QSplitter {
public:
    using QSplitter::QSplitter;
protected:
    QSplitterHandle* createHandle() override { return new SplitHandle(orientation(), this); }
};

// ── Faixa da barra horizontal: calha à esquerda continua até o fim ───────────
class HbarRow : public QWidget {
public:
    using QWidget::QWidget;
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const Palette pal = Palette::current();
        p.fillRect(rect(), pal.page);
        p.fillRect(QRect(0, 0, 180, height()), pal.panel);
        p.fillRect(QRect(179, 0, 1, height()), pal.border);
    }
};

// ── Fantasma do arraste: a bolinha vai grudada no cursor, pulsando ─────────
class Ghost : public QWidget {
public:
    explicit Ghost(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        m_timer.setInterval(16);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this]() { update(); });
        hide();
    }
    static constexpr int kKnotBox = 30; // o centro da bolinha fica em (15, h/2)
    void set(const QString& title, const QString& target, const QColor& color)
    {
        m_title = title; m_target = target; m_color = color;
        const QFontMetricsF ft(serifFont(12)), fs(uiFont(11));
        const qreal lw = 10 + ft.horizontalAdvance(m_title)
                       + (m_target.isEmpty() ? 0 : 8 + fs.horizontalAdvance(m_target)) + 11;
        resize(kKnotBox + 4 + int(std::ceil(lw)) + 2, 34);
        update();
    }
    // posiciona com o centro da bolinha exatamente no ponto (coords do pai)
    void centerAt(const QPoint& pt) { move(pt.x() - kKnotBox / 2, pt.y() - height() / 2); }
protected:
    void showEvent(QShowEvent*) override { m_clock.restart(); m_timer.start(); }
    void hideEvent(QHideEvent*) override { m_timer.stop(); }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette pal = Palette::current();
        const qreal t = m_clock.isValid() ? m_clock.elapsed() / 1000.0 : 0.0;
        const qreal wave = (std::sin(t * 2.0 * M_PI * 1.4) + 1.0) / 2.0; // 0..1, ~1,4 Hz
        const QPointF c(kKnotBox / 2.0, height() / 2.0);
        // halo que respira
        p.setPen(Qt::NoPen);
        p.setBrush(alpha(m_color, 0.10 + 0.22 * (1.0 - wave)));
        const qreal hr = 7.5 + 4 + 5 * wave;
        p.drawEllipse(c, hr, hr);
        // a bolinha pisca de leve
        p.save();
        p.setOpacity(0.62 + 0.38 * (1.0 - wave));
        drawKnot(&p, c, 7.5, m_color, pal.page, false, false, true, false);
        p.restore();
        // rótulo: título + destino
        const QRectF pill(kKnotBox + 4, height() / 2.0 - 13, width() - kKnotBox - 5, 26);
        p.setPen(QPen(alpha(pal.accent, 0.6), 1));
        p.setBrush(pal.panel);
        p.drawRoundedRect(pill.adjusted(0.5, 0.5, -0.5, -0.5), Theme::controlRadius(), Theme::controlRadius());
        const QFont ft = serifFont(12);
        const QFontMetricsF fm(ft);
        qreal x = pill.left() + 10;
        p.setFont(ft);
        p.setPen(pal.bright);
        p.drawText(QPointF(x, pill.center().y() + fm.ascent() / 2.0 - 1.5), m_title);
        x += fm.horizontalAdvance(m_title) + 8;
        if (!m_target.isEmpty()) {
            const QFont fs = uiFont(11);
            p.setFont(fs);
            p.setPen(pal.accent);
            p.drawText(QPointF(x, pill.center().y() + QFontMetricsF(fs).ascent() / 2.0 - 1.5), m_target);
        }
    }
private:
    QString m_title, m_target;
    QColor  m_color;
    QTimer  m_timer;
    QElapsedTimer m_clock;
};

// ── Régua ─────────────────────────────────────────────────────────────────────
class Ruler : public QWidget {
public:
    explicit Ruler(TimelineTracksView* v) : QWidget(v), v(v)
    {
        setFixedHeight(TimelineTracksView::kRulerH);
        setMouseTracking(true);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette pal = Palette::current();
        p.fillRect(rect(), pal.page);
        const int G = TimelineTracksView::kGutter;
        p.save();
        p.setClipRect(QRect(G, 0, width() - G, height()));
        p.translate(G - v->sx(), 0);
        const qreal cw = v->colW();
        const int ec = v->m_data.editorCol;
        if (ec >= 0)
            p.fillRect(QRectF(TimelineTracksView::kPadL + ec * cw, 0, cw, height()), alpha(pal.accent, 0.045));
        if (v->m_hoverCol >= 0)
            p.fillRect(QRectF(TimelineTracksView::kPadL + v->m_hoverCol * cw, 0, cw, height()), alpha(pal.ink, 0.05));
        const QFont f = monoFont(11);
        p.setFont(f);
        const QFontMetricsF fm(f);
        for (int c = 0; c < v->m_data.cols.size(); ++c) {
            if (c == ec) continue;
            const QString t = v->m_data.cols[c].ruler;
            p.setPen(pal.dim);
            p.drawText(QPointF(v->xCol(c) - fm.horizontalAdvance(t) / 2.0, 22), t);
        }
        if (ec >= 0) {
            // selo "✎ 10" — capítulo aberto no editor
            const QString t = v->m_data.cols[ec].ruler;
            const QFont fb = monoFont(11, QFont::Medium);
            const QFontMetricsF fmb(fb);
            const qreal w = 6 + 11 + 4 + fmb.horizontalAdvance(t) + 7;
            const QRectF pill(v->xCol(ec) - w / 2.0, 8, w, 18);
            p.setPen(Qt::NoPen);
            p.setBrush(pal.accent);
            p.drawRoundedRect(pill, 9, 9);
            QPen pen(pal.app, 1.6); pen.setJoinStyle(Qt::RoundJoin);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            const qreal ix = pill.left() + 6, iy = pill.top() + 3.5, s = 11.0 / 16.0;
            QPainterPath pencil;
            pencil.moveTo(ix + 11 * s, iy + 2.5 * s);
            pencil.lineTo(ix + 13.5 * s, iy + 5 * s);
            pencil.lineTo(ix + 6 * s, iy + 12.5 * s);
            pencil.lineTo(ix + 3.5 * s, iy + 12.5 * s);
            pencil.lineTo(ix + 3.5 * s, iy + 10 * s);
            pencil.closeSubpath();
            p.drawPath(pencil);
            p.setFont(fb);
            p.setPen(pal.app);
            p.drawText(QPointF(ix + 11 + 4, pill.center().y() + fmb.ascent() / 2.0 - 1.5), t);
        }
        p.restore();
        p.fillRect(QRect(0, 0, G, height()), pal.panel);
        p.fillRect(QRect(G - 1, 0, 1, height()), pal.border);
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        const qreal cx = e->position().x() - TimelineTracksView::kGutter + v->sx();
        const int col = int(std::floor((cx - TimelineTracksView::kPadL) / v->colW()));
        v->setHoverCol(e->position().x() >= TimelineTracksView::kGutter && col >= 0
                       && col < v->m_data.cols.size() ? col : -1);
        if (col >= 0 && col < v->m_data.cols.size() && col == v->m_data.editorCol)
            setToolTip(QCoreApplication::translate("TimelineTracks", "Capítulo aberto no editor"));
        else setToolTip(QString());
    }
    void leaveEvent(QEvent*) override { v->setHoverCol(-1); }
private:
    TimelineTracksView* v;
};

// ── Base comum às duas faixas (linhas e elenco): pan, roda, coluna sob o mouse ─
class BandCanvas : public QWidget {
public:
    explicit BandCanvas(TimelineTracksView* v) : QWidget(nullptr), v(v)
    {
        setMouseTracking(true);
        setAttribute(Qt::WA_OpaquePaintEvent);
    }
protected:
    qreal toContentX(qreal wx) const { return wx - TimelineTracksView::kGutter + v->sx(); }
    int colAt(qreal wx) const
    {
        if (wx < TimelineTracksView::kGutter) return -1;
        const int c = int(std::floor((toContentX(wx) - TimelineTracksView::kPadL) / v->colW()));
        return (c >= 0 && c < v->m_data.cols.size()) ? c : -1;
    }
    void paintColumns(QPainter& p, const Palette& pal) const
    {
        const qreal cw = v->colW();
        const int ec = v->m_data.editorCol;
        if (ec >= 0)
            p.fillRect(QRectF(TimelineTracksView::kPadL + ec * cw, 0, cw, height()), alpha(pal.accent, 0.045));
        if (v->m_hoverCol >= 0)
            p.fillRect(QRectF(TimelineTracksView::kPadL + v->m_hoverCol * cw, 0, cw, height()), alpha(pal.ink, 0.05));
    }
    void paintGutterBg(QPainter& p, const Palette& pal) const
    {
        p.fillRect(QRect(0, 0, TimelineTracksView::kGutter, height()), pal.panel);
        p.fillRect(QRect(TimelineTracksView::kGutter - 1, 0, 1, height()), pal.border);
    }
    void wheelEvent(QWheelEvent* e) override
    {
        if (e->modifiers() & Qt::ControlModifier) {
            e->accept();
            if (m_wheelClock.isValid() && m_wheelClock.elapsed() < 260) return;
            m_wheelClock.restart();
            const int step = e->angleDelta().y() > 0 ? 1 : (e->angleDelta().y() < 0 ? -1 : 0);
            const int d = int(v->m_density) + step;
            if (step && d >= 0 && d <= 2) emit v->densityChangeRequested(d);
            return;
        }
        const QPoint ad = e->angleDelta();
        if ((e->modifiers() & Qt::ShiftModifier) || std::abs(ad.x()) > std::abs(ad.y())) {
            const int delta = std::abs(ad.x()) > std::abs(ad.y()) ? ad.x() : ad.y();
            v->m_hbar->setValue(v->m_hbar->value() - delta);
            e->accept();
            return;
        }
        e->ignore(); // rolagem vertical fica com a QScrollArea
    }
    // pan horizontal ao arrastar o fundo
    void beginPan(const QPointF& pos) { m_panStart = pos; m_panSx = v->sx(); m_panning = true; m_panMoved = false; }
    bool updatePan(const QPointF& pos)
    {
        if (!m_panning) return false;
        const qreal dx = pos.x() - m_panStart.x();
        if (!m_panMoved && std::abs(dx) > 4) { m_panMoved = true; setCursor(Qt::ClosedHandCursor); }
        if (m_panMoved) v->m_hbar->setValue(int(m_panSx - dx));
        return true;
    }
    // true = foi um clique (sem arrastar) no fundo
    bool endPan()
    {
        const bool wasClick = m_panning && !m_panMoved;
        m_panning = false; m_panMoved = false;
        unsetCursor();
        return wasClick;
    }

    TimelineTracksView* v;
    QElapsedTimer m_wheelClock;
    QPointF m_panStart; int m_panSx = 0; bool m_panning = false; bool m_panMoved = false;
};

// ── Faixa das linhas ─────────────────────────────────────────────────────────
class LaneCanvas : public BandCanvas {
public:
    explicit LaneCanvas(TimelineTracksView* v) : BandCanvas(v)
    {
        m_ghost = new Ghost(v);
        // Movimento sutil: um brilho percorre cada trilho, devagar. Só a camada
        // animada é redesenhada a cada quadro — o resto sai do cache.
        m_anim.setInterval(33);
        QObject::connect(&m_anim, &QTimer::timeout, this, [this]() { update(); });
        m_clock.start();
    }
    void relayoutHeight() { setMinimumHeight(int(std::ceil(v->lanesContentH()))); invalidate(); }
    // Algo mudou no desenho parado (dados, hover, seleção, rolagem, tema).
    void invalidate() { m_cacheValid = false; update(); }

protected:
    struct Hit { QRectF r; QString id; };
    struct LanePath { QPainterPath path; QColor color; qreal width = 3; bool dashed = false; qreal op = 1; QString laneId; };
    struct Knot { QPointF c; qreal r; QColor col; bool hollow; bool sel; bool small; qreal op; QString laneId; QString id; };

    void showEvent(QShowEvent* e) override { BandCanvas::showEvent(e); m_anim.start(); }
    void hideEvent(QHideEvent* e) override { BandCanvas::hideEvent(e); m_anim.stop(); }
    void resizeEvent(QResizeEvent* e) override { BandCanvas::resizeEvent(e); m_cacheValid = false; }

    void paintEvent(QPaintEvent*) override
    {
        const qreal dpr = devicePixelRatioF();
        if (!m_cacheValid || m_cache.size() != size() * dpr) {
            m_cache = QPixmap(size() * dpr);
            m_cache.setDevicePixelRatio(dpr);
            QPainter cp(&m_cache);
            renderStatic(cp);
            m_cacheValid = true;
        }
        QPainter p(this);
        p.drawPixmap(0, 0, m_cache);
        paintMotion(p);
    }

    void paintMotion(QPainter& p)
    {
        const Palette pal = Palette::current();
        const int G = TimelineTracksView::kGutter;
        const qreal t = m_clock.elapsed() / 1000.0;
        p.save();
        p.setRenderHint(QPainter::Antialiasing);
        p.setClipRect(QRect(G, 0, width() - G, height()));
        p.translate(G - v->sx(), 0);

        // brilho correndo pelo trilho (tempo passando) — bem fraco
        for (int i = 0; i < m_lanePaths.size(); ++i) {
            const LanePath& lp = m_lanePaths[i];
            if (lp.dashed) continue;
            const qreal L = lp.path.length();
            if (L < 40) continue;
            const qreal speed = 55, tail = 110, cycle = L + tail + 600;
            const qreal head = std::fmod(t * speed + i * 260.0, cycle);
            if (head > L + tail) continue;
            const QColor c = mix(QColor(Qt::white), lp.color, 0.6); // branco contrasta com qualquer linha
            p.save();
            p.setOpacity(lp.op);
            constexpr int N = 22;
            for (int k = 0; k < N; ++k) {
                qreal a = head - tail + k * tail / N, b = a + tail / N;
                if (b <= 0 || a >= L) continue;
                a = std::max<qreal>(a, 0); b = std::min(b, L);
                const qreal f = (k + 1.0) / N;
                p.setPen(QPen(alpha(c, 0.7 * f * f), lp.width, Qt::SolidLine, Qt::FlatCap));
                p.drawLine(lp.path.pointAtPercent(lp.path.percentAtLength(a)),
                           lp.path.pointAtPercent(lp.path.percentAtLength(b)));
            }
            p.restore();
            // redesenha as bolinhas que o brilho atravessou
            const qreal x0 = lp.path.pointAtPercent(lp.path.percentAtLength(std::clamp(head - tail, 0.0, L))).x() - 12;
            const qreal x1 = lp.path.pointAtPercent(lp.path.percentAtLength(std::clamp(head, 0.0, L))).x() + 12;
            for (const Knot& kn : m_knots) {
                if (kn.laneId != lp.laneId || kn.c.x() < std::min(x0, x1) || kn.c.x() > std::max(x0, x1)) continue;
                p.save();
                p.setOpacity(kn.op);
                drawKnot(&p, kn.c, kn.r, kn.col, pal.page, kn.hollow, kn.sel, false, kn.small);
                p.restore();
            }
        }

        // hover: a bolinha inteira cresce um pouco, e o ponto de dentro mais
        if (!m_hoverId.isEmpty() && !m_dragging) {
            for (const Knot& kn : m_knots) {
                if (kn.id != m_hoverId) continue;
                const qreal k = std::min(1.0, m_hoverClock.elapsed() / 140.0);
                const qreal ease = 1.0 - (1.0 - k) * (1.0 - k);
                p.save();
                p.setOpacity(kn.op);
                drawKnot(&p, kn.c, kn.r * (1.0 + 0.22 * ease), kn.col, pal.page, kn.hollow, kn.sel, true, kn.small);
                p.restore();
                break;
            }
        }

        // arraste: anel tracejado girando onde a bolinha vai cair
        if (m_dragging && m_dropLane >= 0) {
            if (const Event* ev = v->m_data.event(m_dragId)) {
                const bool dots = v->m_density == TimelineTracksView::Dots;
                const qreal x = ev->manual ? v->xCol(m_dropCol) + (dots ? 13 : 21) : v->xCol(ev->col);
                const qreal y = v->yLane(m_dropLane);
                QPen pen(alpha(v->m_data.lanes[m_dropLane].color, 0.85), 1.6, Qt::CustomDashLine, Qt::FlatCap);
                pen.setDashPattern({ 2.5, 2.0 });
                pen.setDashOffset(t * 6.0);
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QPointF(x, y), 10.5, 10.5);
            }
        }

        // pouso: uma onda curta onde o evento caiu
        if (m_landValid) {
            const qreal k = m_landClock.elapsed() / 480.0;
            if (k < 1.0 && m_landFound) {
                p.setPen(QPen(alpha(m_landColor, 0.55 * (1.0 - k)), 2.0));
                p.setBrush(Qt::NoBrush);
                const qreal r = 7.5 + 16.0 * (1.0 - (1.0 - k) * (1.0 - k));
                p.drawEllipse(m_landPos, r, r);
            } else if (k >= 1.0) {
                m_landValid = false;
                m_landId.clear();
            }
        }
        p.restore();
    }

    void renderStatic(QPainter& p)
    {
        p.setRenderHint(QPainter::Antialiasing);
        const Palette pal = Palette::current();
        const Data& d = v->m_data;
        const Filter& flt = v->m_filter;
        const qreal cw = v->colW();
        const int G = TimelineTracksView::kGutter;
        p.fillRect(rect(), pal.page);

        m_hits.clear();
        m_looseHits.clear();
        m_laneHits.clear();
        m_glyphHits.clear();
        m_lanePaths.clear();
        m_knots.clear();

        p.save();
        p.setClipRect(QRect(G, 0, width() - G, height()));
        p.translate(G - v->sx(), 0);
        paintColumns(p, pal);

        if (m_dropLane >= 0)
            p.fillRect(QRectF(0, 12 + m_dropLane * v->laneH(), v->contentW(), v->laneH()), alpha(pal.accent, 0.08));

        // ── trilhos (calculados a partir dos dados) ───────────────────────────
        auto colsOf = [&](const QString& laneId) {
            QList<int> cols;
            for (const Event& e : d.events)
                if (e.laneId == laneId && e.col >= 0 && !cols.contains(e.col)) cols << e.col;
            std::sort(cols.begin(), cols.end());
            return cols;
        };
        for (int li = 0; li < d.lanes.size(); ++li) {
            const Lane& L = d.lanes[li];
            const QList<int> cols = colsOf(L.id);
            if (cols.isEmpty()) continue;
            const qreal op = (!flt.laneId.isEmpty() && flt.laneId != L.id) ? 0.25 : 1.0;
            const qreal yB = v->yLane(li);
            p.save();
            p.setOpacity(op);
            const BranchSpan bs = branchSpan(d, li);
            if (bs.parent >= 0) {
                // ramificação: sai da linha-mãe e volta quando os elencos se reencontram
                const qreal yP = v->yLane(bs.parent);
                const int from = bs.from, to = bs.to;
                QPainterPath path;
                if (from >= 0) {
                    path.moveTo(v->xCol(from), yP);
                    path.cubicTo(v->xCol(from) + cw * .55, yP, v->xCol(from + 1) - cw * .55, yB, v->xCol(from + 1), yB);
                    path.lineTo(v->xCol(cols.first()), yB);
                } else {
                    path.moveTo(v->xCol(cols.first()), yB);
                }
                if (to >= 0) {
                    path.lineTo(v->xCol(to - 1), yB);
                    path.cubicTo(v->xCol(to - 1) + cw * .6, yB, v->xCol(to) - cw * .6, yP, v->xCol(to), yP);
                } else {
                    path.lineTo(v->xCol(cols.last()), yB);
                }
                QPen pen(L.color, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                if (L.dashed) { pen.setStyle(Qt::CustomDashLine); pen.setDashPattern({ 7 / 2.5, 6 / 2.5 }); }
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                p.drawPath(path);
                m_lanePaths.append({ path, L.color, 2.5, L.dashed, op, L.id });
            } else if (cols.size() > 1) {
                const qreal w = L.kind == QLatin1String("main") ? 3.0 : 2.5;
                QPen pen(L.color, w, Qt::SolidLine, Qt::RoundCap);
                if (L.dashed) { pen.setStyle(Qt::CustomDashLine); pen.setDashPattern({ 7 / w, 6 / w }); }
                p.setPen(pen);
                p.drawLine(QPointF(v->xCol(cols.first()), yB), QPointF(v->xCol(cols.last()), yB));
                QPainterPath straight(QPointF(v->xCol(cols.first()), yB));
                straight.lineTo(v->xCol(cols.last()), yB);
                m_lanePaths.append({ straight, L.color, w, L.dashed, op, L.id });
            }
            p.restore();
        }

        // ── saltos de tempo (só os maiores que o dobro do intervalo típico) ──
        QList<qreal> allGaps;
        QHash<QString, QList<const Event*>> dated;
        for (const Event& e : d.events)
            if (!e.manual && e.col >= 0 && e.chronoOk) dated[e.laneId] << &e;
        for (auto it = dated.begin(); it != dated.end(); ++it) {
            auto& list = it.value();
            std::sort(list.begin(), list.end(), [](const Event* a, const Event* b) { return a->col < b->col; });
            for (int i = 1; i < list.size(); ++i) allGaps << days(list[i]->chrono - list[i - 1]->chrono);
        }
        std::sort(allGaps.begin(), allGaps.end());
        const qreal thr = allGaps.isEmpty() ? 0.0 : 2.0 * allGaps[allGaps.size() / 2];
        {
            const QFont f = monoFont(10);
            for (auto it = dated.constBegin(); it != dated.constEnd(); ++it) {
                const int li = d.laneIndex(it.key());
                if (li < 0) continue;
                const auto& list = it.value();
                const qreal y = v->yLane(li) - (v->m_density == TimelineTracksView::Dots ? 12 : 10);
                const qreal op = (!flt.laneId.isEmpty() && flt.laneId != it.key()) ? 0.25 : 1.0;
                for (int i = 1; i < list.size(); ++i) {
                    const qreal g = days(list[i]->chrono - list[i - 1]->chrono);
                    const QString lab = fmtGap(g);
                    if (lab.isEmpty() || g <= thr) continue;
                    p.save();
                    p.setOpacity(op);
                    drawHaloText(&p, QCoreApplication::translate("TimelineTracks", "salto · %1").arg(lab), f,
                                 (v->xCol(list[i - 1]->col) + v->xCol(list[i]->col)) / 2.0, y,
                                 pal.ink, pal.page);
                    p.restore();
                }
            }
        }

        // ── eventos de capítulo/cena ──────────────────────────────────────────
        const bool dots = v->m_density == TimelineTracksView::Dots;
        const bool summ = v->m_density == TimelineTracksView::Summaries;
        const QFont fMk = uiFont(10.5, QFont::Medium);
        const QFont fTt = serifFont(12.5);
        const QFont fSm = serifFont(11, QFont::Normal, true);
        const QFontMetricsF mMk(fMk), mTt(fTt), mSm(fSm);
        for (const Event& e : d.events) {
            if (e.manual || e.col < 0) continue;
            const int li = d.laneIndex(e.laneId);
            if (li < 0) continue;
            const Lane& L = d.lanes[li];
            const qreal x = v->xCol(e.col), y = v->yLane(li);
            const bool sel = e.id == v->m_sel;
            const bool hov = e.id == m_hoverId;
            const QColor lc = e.hollow ? pal.warning : L.color;
            p.save();
            qreal op = flt.dims(e) ? 0.22 : 1.0;
            if (m_dragging && e.id == m_dragId) op = 0.28;
            p.setOpacity(op);
            m_knots.append({ QPointF(x, y), kKnotR, lc, e.hollow, sel, false, op, e.laneId, e.id });
            if (e.id == m_landId) { m_landPos = QPointF(x, y); m_landColor = lc; m_landFound = true; }
            if (dots) {
                drawKnot(&p, QPointF(x, y), kKnotR, lc, pal.page, e.hollow, sel, false, false);
                m_hits.append({ QRectF(x - 10, y - 10, 20, 20), e.id });
                p.restore();
                continue;
            }
            const qreal w = cw - 10;
            const QStringList tl = wrapLines(e.title, fTt, w - 10, 2);
            const QStringList sl = summ ? wrapLines(e.summary, fSm, w - 10, 3) : QStringList();
            const qreal mkTop = y + 14;
            const qreal ttTop = mkTop + 12.6 + 2;
            const qreal ttH = tl.size() * 16.0;
            const qreal smTop = ttTop + ttH + 4;
            const qreal bottom = (sl.isEmpty() ? ttTop + ttH : smTop + sl.size() * 15.4) + 6;
            const QRectF box(x - w / 2.0, y - 7, w, bottom - (y - 7));
            if (sel) {
                p.setPen(Qt::NoPen);
                p.setBrush(alpha(lc, 0.13));
                p.drawRoundedRect(box, Theme::controlRadius(), Theme::controlRadius());
            }
            drawKnot(&p, QPointF(x, y), kKnotR, lc, pal.page, e.hollow, sel, false, false);
            const QString mk = e.hollow ? QCoreApplication::translate("TimelineTracks", "sem data") : e.marker;
            drawHaloText(&p, mk, fMk, x, mkTop + (12.6 - mMk.height()) / 2.0 + mMk.ascent(),
                         lc, pal.page);
            for (int i = 0; i < tl.size(); ++i)
                drawHaloText(&p, tl[i], fTt, x, ttTop + i * 16.0 + (16.0 - mTt.height()) / 2.0 + mTt.ascent(),
                             hov ? lc : pal.bright, pal.page);
            for (int i = 0; i < sl.size(); ++i)
                drawHaloText(&p, sl[i], fSm, x, smTop + i * 15.4 + (15.4 - mSm.height()) / 2.0 + mSm.ascent(),
                             pal.dim, pal.page);
            m_hits.append({ box, e.id });
            p.restore();
        }

        // ── eventos manuais: notas dentro do compasso ─────────────────────────
        QHash<QString, int> slot;
        QList<const Event*> manual;
        for (const Event& e : d.events) if (e.manual && e.col >= 0) manual << &e;
        std::sort(manual.begin(), manual.end(), [](const Event* a, const Event* b) {
            if (a->col != b->col) return a->col < b->col;
            return a->order < b->order;
        });
        for (const Event* e : manual) {
            const int li = d.laneIndex(e->laneId);
            if (li < 0) continue;
            const QString k = e->laneId + QLatin1Char(':') + QString::number(e->col);
            const int n = slot.value(k, 0);
            slot[k] = n + 1;
            const qreal y = v->yLane(li);
            const qreal x0 = v->xCol(e->col) + (dots ? 13 : 21);
            if (n >= 3) {
                if (n == 3) {
                    int total = 0;
                    for (const Event* o : manual) if (o->laneId == e->laneId && o->col == e->col) ++total;
                    p.setFont(monoFont(9));
                    p.setPen(pal.dim);
                    p.drawText(QPointF(x0 + 3 * 15 - 4, y + 3.5), QStringLiteral("+%1").arg(total - 3));
                }
                continue;
            }
            const qreal x = x0 + n * 15;
            p.save();
            qreal op = flt.dims(*e) ? 0.22 : 1.0;
            if (m_dragging && e->id == m_dragId) op = 0.28;
            p.setOpacity(op);
            m_knots.append({ QPointF(x, y), kNoteR, d.lanes[li].color, false, e->id == v->m_sel, true, op, e->laneId, e->id });
            if (e->id == m_landId) { m_landPos = QPointF(x, y); m_landColor = d.lanes[li].color; m_landFound = true; }
            drawKnot(&p, QPointF(x, y), kNoteR, d.lanes[li].color, pal.page, false,
                     e->id == v->m_sel, false, true);
            p.restore();
            m_hits.append({ QRectF(x - 8, y - 8, 16, 16), e->id });
        }
        p.restore(); // fim do conteúdo rolável

        // ── faixa "Soltos" (fixa na horizontal) ───────────────────────────────
        if (v->hasLoose()) {
            const QRectF strip(G + 10, v->looseTop(), width() - G - 20, v->looseH() - 8);
            QPen dash(alpha(pal.ink, 0.22), 1, Qt::DashLine);
            p.setPen(dash);
            p.setBrush(alpha(pal.ink, 0.03));
            p.drawRoundedRect(strip.adjusted(0.5, 0.5, -0.5, -0.5), Theme::controlRadius(), Theme::controlRadius());
            const QFont fSrc = uiFont(10), fT = serifFont(12);
            const QFontMetricsF ms(fSrc), mt(fT);
            qreal x = strip.left() + 10;
            const qreal ch = 38, cy = strip.center().y() - ch / 2.0;
            p.save();
            p.setClipRect(strip);
            for (const Event& e : d.events) {
                if (e.col >= 0) continue;
                const QString src = e.marker.isEmpty() ? e.originShort : e.originShort + QStringLiteral(" · ") + e.marker;
                const QString t = mt.elidedText(e.title, Qt::ElideRight, 180);
                const qreal w = std::max(ms.horizontalAdvance(src), mt.horizontalAdvance(t)) + 20;
                const QRectF chip(x, cy, w, ch);
                p.save();
                if (v->m_filter.dims(e)) p.setOpacity(0.22);
                if (m_dragging && e.id == m_dragId) p.setOpacity(0.35);
                const bool sel = e.id == v->m_sel;
                p.setPen(QPen(sel ? pal.accent : (e.id == m_hoverId ? mix(pal.ink, pal.border, 0.4) : pal.border), 1));
                p.setBrush(pal.panel);
                p.drawRoundedRect(chip.adjusted(0.5, 0.5, -0.5, -0.5), Theme::controlRadius(), Theme::controlRadius());
                p.setFont(fSrc); p.setPen(pal.dim);
                p.drawText(QPointF(chip.left() + 10, chip.top() + 5 + ms.ascent()), src);
                p.setFont(fT); p.setPen(pal.bright);
                p.drawText(QPointF(chip.left() + 10, chip.top() + 5 + 12 + 1 + (15 - mt.height()) / 2 + mt.ascent()), t);
                p.restore();
                m_looseHits.append({ chip, e.id });
                x += w + 8;
            }
            const QFont fh = uiFont(11);
            const QString hint = QCoreApplication::translate("TimelineTracks", "arraste para uma linha, na altura do capítulo");
            const qreal hw = QFontMetricsF(fh).horizontalAdvance(hint);
            if (x + hw + 20 < strip.right()) {
                p.setFont(fh); p.setPen(pal.dim);
                p.drawText(QPointF(strip.right() - 10 - hw, strip.center().y() + QFontMetricsF(fh).ascent() / 2 - 1), hint);
            }
            p.restore();
        }

        // ── calha: nomes das linhas ───────────────────────────────────────────
        paintGutterBg(p, pal);
        const QFont fName = uiFont(12.5, QFont::DemiBold), fSub = uiFont(10.5);
        const QFontMetricsF mn(fName), msub(fSub);
        for (int li = 0; li < d.lanes.size(); ++li) {
            const Lane& L = d.lanes[li];
            const bool hasSub = !L.sub.isEmpty() && !dots;
            const qreal h = hasSub ? 42.8 : 27.6;
            const QRectF r(6, v->yLane(li) - 15, G - 14, h);
            const bool on = flt.laneId == L.id, off = !flt.laneId.isEmpty() && !on;
            p.save();
            if (off) p.setOpacity(0.4);
            if (on) {
                p.setPen(QPen(alpha(L.color, 0.45), 1));
                p.setBrush(alpha(L.color, 0.16));
                p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), Theme::controlRadius(), Theme::controlRadius());
            } else if (m_hoverLane == L.id) {
                p.setPen(Qt::NoPen);
                p.setBrush(alpha(pal.ink, 0.06));
                p.drawRoundedRect(r, Theme::controlRadius(), Theme::controlRadius());
            }
            // glifo da linha — clicável: abre o seletor de cor
            const qreal gx = r.left() + 8, gy = r.top() + 6 + 3;
            const QRectF glyphBox(gx - 3, gy - 3, 24, 20);
            m_glyphHits.append({ glyphBox, L.id });
            if (m_hoverGlyph == L.id) {
                p.setPen(QPen(alpha(pal.ink, 0.30), 1));
                p.setBrush(alpha(pal.ink, 0.10));
                p.drawRoundedRect(glyphBox.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
            }
            if (L.kind == QLatin1String("parallel")) {
                QPainterPath gp; gp.moveTo(gx + 1, gy + 3); gp.cubicTo(gx + 7, gy + 3, gx + 9, gy + 11, gx + 17, gy + 11);
                p.setPen(QPen(L.color, 2.5, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush); p.drawPath(gp);
            } else {
                QPen gpen(L.color, L.dashed ? 2.5 : 3.5, Qt::SolidLine, Qt::RoundCap);
                if (L.dashed) { gpen.setStyle(Qt::CustomDashLine); gpen.setDashPattern({ 4 / 2.5, 3 / 2.5 }); gpen.setCapStyle(Qt::FlatCap); }
                p.setPen(gpen);
                p.drawLine(QPointF(gx + 1, gy + 7), QPointF(gx + 17, gy + 7));
            }
            const qreal tx = gx + 18 + 9;
            p.setFont(fName); p.setPen(pal.bright);
            p.drawText(QPointF(tx, r.top() + 6 + (15.6 - mn.height()) / 2 + mn.ascent()),
                       mn.elidedText(L.name, Qt::ElideRight, r.right() - tx - 6));
            if (hasSub) {
                p.setFont(fSub); p.setPen(pal.dim);
                p.drawText(QPointF(tx, r.top() + 6 + 15.6 + 1 + (14.2 - msub.height()) / 2 + msub.ascent()),
                           msub.elidedText(L.sub, Qt::ElideRight, r.right() - tx - 6));
            }
            p.restore();
            m_laneHits.append({ r, L.id });
        }
        if (v->hasLoose()) {
            int n = 0; for (const Event& e : d.events) if (e.col < 0) ++n;
            const QRectF r(6, v->looseTop() + 12, G - 14, 42.8);
            const qreal gx = r.left() + 8, gy = r.top() + 6 + 3;
            QPainterPath inbox;
            inbox.moveTo(gx + 2, gy + 8); inbox.lineTo(gx + 6, gy + 8); inbox.lineTo(gx + 7.5, gy + 10);
            inbox.lineTo(gx + 10.5, gy + 10); inbox.lineTo(gx + 12, gy + 8); inbox.lineTo(gx + 16, gy + 8);
            inbox.moveTo(gx + 2, gy + 8); inbox.lineTo(gx + 4, gy + 3); inbox.lineTo(gx + 14, gy + 3);
            inbox.lineTo(gx + 16, gy + 8); inbox.lineTo(gx + 16, gy + 12); inbox.lineTo(gx + 2, gy + 12); inbox.closeSubpath();
            p.setPen(QPen(pal.dim, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
            p.drawPath(inbox);
            const qreal tx = gx + 18 + 9;
            p.setFont(fName); p.setPen(pal.bright);
            p.drawText(QPointF(tx, r.top() + 6 + (15.6 - mn.height()) / 2 + mn.ascent()), QCoreApplication::translate("TimelineTracks", "Soltos · %1").arg(n));
            p.setFont(fSub); p.setPen(pal.dim);
            p.drawText(QPointF(tx, r.top() + 6 + 15.6 + 1 + (14.2 - msub.height()) / 2 + msub.ascent()), QCoreApplication::translate("TimelineTracks", "sem lugar ainda"));
        }
    }

    QString hitAt(const QPointF& wpos) const
    {
        for (const auto& h : m_looseHits) if (h.r.contains(wpos)) return h.id;
        if (wpos.x() < TimelineTracksView::kGutter) return {};
        const QPointF c(toContentX(wpos.x()), wpos.y());
        for (int i = m_hits.size() - 1; i >= 0; --i) if (m_hits[i].r.contains(c)) return m_hits[i].id;
        return {};
    }
    QString glyphAt(const QPointF& wpos) const
    {
        if (wpos.x() >= TimelineTracksView::kGutter) return {};
        for (const auto& h : m_glyphHits) if (h.r.contains(wpos)) return h.id;
        return {};
    }
    QString laneLabelAt(const QPointF& wpos) const
    {
        if (wpos.x() >= TimelineTracksView::kGutter) return {};
        for (const auto& h : m_laneHits) if (h.r.contains(wpos)) return h.id;
        return {};
    }
    // Destino do arraste: linha (índice) ou Soltos (-2); -1 = fora.
    int dropLaneAt(const QPointF& wpos) const
    {
        if (wpos.x() < TimelineTracksView::kGutter || wpos.x() > width()) return -1;
        if (v->hasLoose() && wpos.y() >= v->looseTop() && wpos.y() <= v->looseTop() + v->looseH()) return -2;
        const int i = int(std::floor((wpos.y() - 12) / v->laneH()));
        return (i >= 0 && i < v->m_data.lanes.size()) ? i : -1;
    }
    int dropColAt(const QPointF& wpos) const
    {
        const int c = int(std::floor((toContentX(wpos.x()) - TimelineTracksView::kPadL) / v->colW()));
        return qBound(0, c, qMax(0, int(v->m_data.cols.size()) - 1));
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        const QPointF pos = e->position();
        m_pressGlyph = glyphAt(pos);
        if (!m_pressGlyph.isEmpty()) return;
        m_pressLane = laneLabelAt(pos);
        if (!m_pressLane.isEmpty()) return;
        const QString id = hitAt(pos);
        if (!id.isEmpty()) { m_dragId = id; m_dragStart = pos; m_dragging = false; return; }
        if (pos.x() >= TimelineTracksView::kGutter) beginPan(pos);
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        const QPointF pos = e->position();
        if (!m_dragId.isEmpty() && (e->buttons() & Qt::LeftButton)) {
            if (!m_dragging && QLineF(pos, m_dragStart).length() > 5) {
                m_dragging = true;
                if (const Event* ev = v->m_data.event(m_dragId)) m_ghost->set(ev->title, QString(), dragColor(*ev));
                m_ghost->show(); m_ghost->raise();
                setCursor(Qt::BlankCursor); // a própria bolinha vira o cursor
                invalidate();
            }
            if (m_dragging) { updateDrag(pos); return; }
        }
        if (updatePan(pos)) return;
        // hover
        v->setHoverCol(colAt(pos.x()));
        const QString hid = hitAt(pos);
        const QString hl = laneLabelAt(pos);
        const QString hg = glyphAt(pos);
        if (hg != m_hoverGlyph) { m_hoverGlyph = hg; invalidate(); }
        if (hid != m_hoverId || hl != m_hoverLane) {
            if (hid != m_hoverId) m_hoverClock.restart();
            m_hoverId = hid; m_hoverLane = hl; invalidate();
        }
        setCursor(!hid.isEmpty() ? Qt::OpenHandCursor : (!hl.isEmpty() ? Qt::PointingHandCursor : Qt::ArrowCursor));
        if (!hid.isEmpty()) {
            const Event* ev = v->m_data.event(hid);
            const bool tip = ev && (v->m_density == TimelineTracksView::Dots || ev->manual);
            if (tip) {
                QString t = ev->title;
                if (!ev->marker.isEmpty()) t += QStringLiteral(" · ") + ev->marker;
                if (ev->manual && !ev->originWhere.isEmpty()) t += QStringLiteral("\n") + ev->originWhere;
                QToolTip::showText(e->globalPosition().toPoint(), t, this);
            }
        } else if (!hg.isEmpty()) {
            QToolTip::showText(e->globalPosition().toPoint(),
                               QCoreApplication::translate("TimelineTracks", "Cor da linha"), this);
        } else if (!hl.isEmpty()) {
            if (const Lane* L = v->m_data.lane(hl))
                QToolTip::showText(e->globalPosition().toPoint(),
                                   L->tip.isEmpty() ? QCoreApplication::translate("TimelineTracks", "Clique para focar") : L->tip + QCoreApplication::translate("TimelineTracks", " · clique para focar"), this);
        } else {
            QToolTip::hideText();
        }
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        const QPointF pos = e->position();
        if (!m_pressGlyph.isEmpty()) {
            const QString id = m_pressGlyph;
            m_pressGlyph.clear();
            if (glyphAt(pos) == id) emit v->laneColorRequested(id, e->globalPosition().toPoint());
            return;
        }
        if (!m_pressLane.isEmpty()) {
            if (laneLabelAt(pos) == m_pressLane) emit v->laneClicked(m_pressLane);
            m_pressLane.clear();
            return;
        }
        if (!m_dragId.isEmpty()) {
            const QString id = m_dragId;
            m_dragId.clear();
            if (m_dragging) {
                m_dragging = false;
                m_ghost->hide();
                unsetCursor();
                const int li = dropLaneAt(mapFromGlobal(e->globalPosition()));
                const int col = dropColAt(pos);
                m_dropLane = -1;
                v->setHoverCol(-1);
                if (li >= 0) { m_landId = id; m_landValid = true; m_landFound = false; m_landClock.restart(); }
                invalidate();
                const Event* ev = v->m_data.event(id);
                if (!ev) return;
                if (li == -2 && ev->manual) emit v->eventDropped(id, QString(), -1);
                else if (li >= 0) emit v->eventDropped(id, v->m_data.lanes[li].id, ev->manual ? col : ev->col);
                return;
            }
            emit v->eventClicked(id);
            return;
        }
        if (endPan()) emit v->backgroundClicked();
    }
    void mouseDoubleClickEvent(QMouseEvent* e) override
    {
        const QPointF pos = e->position();
        if (!hitAt(pos).isEmpty() || pos.x() < TimelineTracksView::kGutter) return;
        const int li = dropLaneAt(pos);
        if (li >= 0) emit v->createAtRequested(v->m_data.lanes[li].id, dropColAt(pos));
    }
    void contextMenuEvent(QContextMenuEvent* e) override
    {
        const QString lane = laneLabelAt(e->pos());
        if (!lane.isEmpty()) { emit v->laneContextMenu(lane, e->globalPos()); return; }
        const QString id = hitAt(e->pos());
        if (!id.isEmpty()) emit v->eventContextMenu(id, e->globalPos());
    }
    void leaveEvent(QEvent*) override
    {
        v->setHoverCol(-1);
        if (!m_hoverId.isEmpty() || !m_hoverLane.isEmpty() || !m_hoverGlyph.isEmpty()) {
            m_hoverId.clear(); m_hoverLane.clear(); m_hoverGlyph.clear(); invalidate();
        }
    }

    void updateDrag(const QPointF& pos)
    {
        const Event* ev = v->m_data.event(m_dragId);
        if (!ev) return;
        const QPoint gp = mapToGlobal(pos.toPoint());
        m_ghost->centerAt(v->mapFromGlobal(gp));
        const int li = dropLaneAt(pos);
        QString label;
        const int prevLane = m_dropLane;
        m_dropLane = li >= 0 ? li : -1;
        m_dropCol = dropColAt(pos);
        if (prevLane != m_dropLane) invalidate(); // faixa de destino acende
        if (li >= 0) {
            const Lane& L = v->m_data.lanes[li];
            if (ev->manual) {
                const int c = dropColAt(pos);
                v->setHoverCol(c);
                label = QStringLiteral("→ %1 · %2").arg(L.name, v->m_data.cols.value(c).unitLabel);
            } else {
                v->setHoverCol(-1);
                label = QStringLiteral("→ %1").arg(L.name);
            }
        } else if (li == -2) {
            v->setHoverCol(-1);
            label = ev->manual ? QStringLiteral("→ %1").arg(QCoreApplication::translate("TimelineTracks", "Soltos")) : QCoreApplication::translate("TimelineTracks", "capítulo não fica solto");
        } else {
            v->setHoverCol(-1);
        }
        m_ghost->set(ev->title, label, dragColor(*ev));
        update();
    }
    QColor dragColor(const Event& ev) const
    {
        if (ev.hollow) return Palette::current().warning;
        if (const Lane* L = v->m_data.lane(ev.laneId)) return L->color;
        return Palette::current().accent;
    }

private:
    QList<Hit> m_hits, m_looseHits, m_laneHits, m_glyphHits;
    QString m_hoverId, m_hoverLane, m_pressLane, m_hoverGlyph, m_pressGlyph;
    QString m_dragId; QPointF m_dragStart; bool m_dragging = false;
    int m_dropLane = -1;
    int m_dropCol = 0;
    Ghost* m_ghost = nullptr;
    // camada parada em cache + camada animada
    QPixmap m_cache;
    bool m_cacheValid = false;
    QList<LanePath> m_lanePaths;
    QList<Knot> m_knots;
    QTimer m_anim;
    QElapsedTimer m_clock;
    QString m_landId;
    QPointF m_landPos;
    QColor m_landColor;
    bool m_landValid = false;
    bool m_landFound = false;
    QElapsedTimer m_landClock;
    QElapsedTimer m_hoverClock;
};

// ── Faixa do elenco ──────────────────────────────────────────────────────────
class CastCanvas : public BandCanvas {
public:
    explicit CastCanvas(TimelineTracksView* v) : BandCanvas(v) {}
    void relayoutHeight() { setMinimumHeight(int(std::ceil(v->castContentH()))); update(); }
protected:
    qreal yRow(int ci) const { return kCastTop + ci * kCastRow + kCastRow / 2.0; }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette pal = Palette::current();
        const Data& d = v->m_data;
        const int G = TimelineTracksView::kGutter;
        p.fillRect(rect(), pal.page);
        p.save();
        p.setClipRect(QRect(G, 0, width() - G, height()));
        p.translate(G - v->sx(), 0);
        paintColumns(p, pal);
        const QFont fAbs = monoFont(9.5);
        for (int ci = 0; ci < d.chars.size(); ++ci) {
            const Character& c = d.chars[ci];
            QList<int> cols;
            for (const Event& e : d.events)
                if (!e.manual && e.col >= 0 && e.castIds.contains(c.id) && !cols.contains(e.col)) cols << e.col;
            std::sort(cols.begin(), cols.end());
            const qreal y = yRow(ci);
            p.save();
            if (!v->m_filter.charId.isEmpty() && v->m_filter.charId != c.id) p.setOpacity(0.25);
            const bool showAbs = v->m_hoverCastChar == c.id || v->m_filter.charId == c.id;
            for (int i = 1; i < cols.size(); ++i) {
                const int a = cols[i - 1], b = cols[i], gap = b - a - 1;
                if (gap == 0) {
                    p.setPen(QPen(alpha(c.color, 0.45), 2));
                    p.drawLine(QPointF(v->xCol(a), y), QPointF(v->xCol(b), y));
                } else if (gap >= 3 && showAbs) {
                    drawHaloText(&p, QCoreApplication::translate("TimelineTracks", "ausente · %1 caps").arg(gap), fAbs,
                                 (v->xCol(a) + v->xCol(b)) / 2.0, y + 3.5, pal.dim, pal.page);
                }
            }
            for (int col : cols) {
                p.setPen(QPen(pal.page, 2));
                p.setBrush(c.color);
                p.drawEllipse(QPointF(v->xCol(col), y), 4, 4);
            }
            p.restore();
        }
        p.restore();

        paintGutterBg(p, pal);
        QFont fh = uiFont(9.5, QFont::DemiBold);
        fh.setLetterSpacing(QFont::PercentageSpacing, 113);
        p.setFont(fh); p.setPen(pal.dim);
        p.drawText(QPointF(16, 10 + QFontMetricsF(fh).ascent()), QCoreApplication::translate("TimelineTracks", "ELENCO"));
        m_rows.clear();
        const QFont fn = uiFont(12);
        const QFontMetricsF mn(fn);
        for (int ci = 0; ci < d.chars.size(); ++ci) {
            const Character& c = d.chars[ci];
            const QRectF r(6, yRow(ci) - 12, G - 14, 24);
            const bool on = v->m_filter.charId == c.id;
            if (on) {
                p.setPen(Qt::NoPen); p.setBrush(alpha(c.color, 0.18));
                p.drawRoundedRect(r, Theme::controlRadius(), Theme::controlRadius());
            } else if (m_hoverRow == c.id) {
                p.setPen(Qt::NoPen); p.setBrush(alpha(pal.ink, 0.06));
                p.drawRoundedRect(r, Theme::controlRadius(), Theme::controlRadius());
            }
            drawAvatar(&p, QRectF(r.left() + 8, r.center().y() - 9.5, 19, 19), c, pal.app);
            p.setFont(fn); p.setPen(on ? pal.bright : pal.ink);
            p.drawText(QPointF(r.left() + 8 + 19 + 8, r.center().y() + mn.ascent() / 2 - 1.5),
                       mn.elidedText(c.name, Qt::ElideRight, r.right() - (r.left() + 35) - 6));
            m_rows.append({ r, c.id });
        }
    }
    QString rowAt(const QPointF& pos) const
    {
        for (const auto& r : m_rows) if (r.first.contains(pos)) return r.second;
        return {};
    }
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        m_pressRow = rowAt(e->position());
        if (m_pressRow.isEmpty() && e->position().x() >= TimelineTracksView::kGutter) beginPan(e->position());
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (updatePan(e->position())) return;
        v->setHoverCol(colAt(e->position().x()));
        const QString r = rowAt(e->position());
        if (r != m_hoverRow) {
            m_hoverRow = r;
            v->m_hoverCastChar = r;
            setCursor(r.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
            if (!r.isEmpty()) {
                int n = 0;
                for (const Event& ev : v->m_data.events) if (!ev.manual && ev.col >= 0 && ev.castIds.contains(r)) ++n;
                setToolTip(QCoreApplication::translate("TimelineTracks", "%1 capítulos").arg(n));
            } else setToolTip(QString());
            update();
        }
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        if (!m_pressRow.isEmpty()) {
            if (rowAt(e->position()) == m_pressRow) emit v->characterClicked(m_pressRow);
            m_pressRow.clear();
            return;
        }
        if (endPan()) emit v->backgroundClicked();
    }
    void leaveEvent(QEvent*) override
    {
        v->setHoverCol(-1);
        m_hoverRow.clear(); v->m_hoverCastChar.clear();
        update();
    }
private:
    QList<QPair<QRectF, QString>> m_rows;
    QString m_hoverRow, m_pressRow;
};

} // namespace TracksDetail

// ═════════════════════════════════════════════════════════════════════════════

using namespace TracksDetail;

TimelineTracksView::TimelineTracksView(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("tlTracks"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_ruler = new Ruler(this);
    root->addWidget(m_ruler);

    m_lanes = new LaneCanvas(this);
    m_cast  = new CastCanvas(this);
    auto makeArea = [this](QWidget* canvas) {
        auto* a = new QScrollArea(this);
        a->setObjectName(QStringLiteral("tlTracksBand"));
        a->setFrameShape(QFrame::NoFrame);
        a->setWidgetResizable(true);
        a->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        a->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        a->setWidget(canvas);
        return a;
    };
    m_laneArea = makeArea(m_lanes);
    m_castArea = makeArea(m_cast);

    m_split = new Splitter(Qt::Vertical, this);
    m_split->setChildrenCollapsible(false);
    m_split->setHandleWidth(9);
    m_split->addWidget(m_laneArea);
    m_split->addWidget(m_castArea);
    m_split->setStretchFactor(0, 0);
    m_split->setStretchFactor(1, 1);
    root->addWidget(m_split, 1);
    connect(m_split, &QSplitter::splitterMoved, this, [this]() {
        m_wantLanesH = m_split->sizes().value(0);
        emit layoutChanged();
    });

    m_hbarRow = new HbarRow(this);
    m_hbarRow->setObjectName(QStringLiteral("tlTracksHbarRow"));
    m_hbarRow->setFixedHeight(12);
    auto* hl = new QHBoxLayout(m_hbarRow);
    hl->setContentsMargins(kGutter, 0, 0, 0);
    hl->setSpacing(0);
    m_hbar = new QScrollBar(Qt::Horizontal, m_hbarRow);
    m_hbar->setObjectName(QStringLiteral("tlTracksHbar"));
    hl->addWidget(m_hbar);
    root->addWidget(m_hbarRow);
    connect(m_hbar, &QScrollBar::valueChanged, this, [this]() { repaintAll(); });

    auto applyTheme = [this]() {
        const Palette pal = Palette::current();
        setStyleSheet(QStringLiteral(R"(
            QScrollBar#tlTracksHbar:horizontal { background: transparent; height: 8px; margin: 2px 0; }
            QScrollBar#tlTracksHbar::handle:horizontal { background: %1; border-radius: 3px; min-width: 40px; }
            QScrollBar#tlTracksHbar::add-line, QScrollBar#tlTracksHbar::sub-line { width: 0; height: 0; }
            QScrollBar#tlTracksHbar::add-page, QScrollBar#tlTracksHbar::sub-page { background: transparent; }
            QScrollArea#tlTracksBand { background: %2; }
            QScrollArea#tlTracksBand QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
            QScrollArea#tlTracksBand QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 30px; }
            QScrollArea#tlTracksBand QScrollBar::add-line, QScrollArea#tlTracksBand QScrollBar::sub-line { height: 0; }
            QScrollArea#tlTracksBand QScrollBar::add-page, QScrollArea#tlTracksBand QScrollBar::sub-page { background: transparent; }
        )").arg(pal.border.name(), pal.page.name()));
        repaintAll();
        m_split->update();
        m_hbarRow->update();
    };
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, applyTheme);
}

qreal TimelineTracksView::colW() const
{
    switch (m_density) { case Dots: return 62; case Summaries: return 170; default: return 118; }
}
qreal TimelineTracksView::laneH() const
{
    switch (m_density) { case Dots: return 40; case Summaries: return 140; default: return 84; }
}
qreal TimelineTracksView::lineOff() const { return m_density == Dots ? laneH() / 2.0 : 16.0; }
bool TimelineTracksView::hasLoose() const
{
    for (const auto& e : m_data.events) if (e.col < 0) return true;
    return false;
}
qreal TimelineTracksView::lanesContentH() const
{
    return 12.0 + m_data.lanes.size() * laneH() + 8.0 + (hasLoose() ? 64.0 : 0.0);
}
qreal TimelineTracksView::castContentH() const { return kCastTop + m_data.chars.size() * kCastRow + 12.0; }
int TimelineTracksView::sx() const { return m_hbar ? m_hbar->value() : 0; }

void TimelineTracksView::setData(const Tracks::Data& data)
{
    const int prevEditor = m_data.editorCol;
    const bool firstData = m_data.cols.isEmpty();
    m_data = data;
    m_lanes->relayoutHeight();
    m_cast->relayoutHeight();
    refreshScroll();
    if (firstData || m_data.editorCol != prevEditor) {
        if (m_data.editorCol >= 0) scrollToColumn(m_data.editorCol);
    }
    if (m_wantLanesH < 0 && height() > 0) {
        const int total = m_split->height();
        const int want = qBound(60, int(lanesContentH()) + 10, qMax(60, total - 110));
        m_split->setSizes({ want, qMax(60, total - want) });
    }
    repaintAll();
}

void TimelineTracksView::setFilter(const Tracks::Filter& f) { m_filter = f; repaintAll(); }
void TimelineTracksView::setSelected(const QString& id) { m_sel = id; repaintAll(); }

void TimelineTracksView::setDensity(Density d)
{
    if (d == m_density) return;
    m_density = d;
    m_lanes->relayoutHeight();
    m_cast->relayoutHeight();
    refreshScroll();
    if (m_data.editorCol >= 0) scrollToColumn(m_data.editorCol);
    if (m_wantLanesH < 0 && m_split->height() > 0) {
        // divisória nunca arrastada: acompanha a altura da nova densidade
        const int total = m_split->height();
        const int want = qBound(60, int(lanesContentH()) + 10, qMax(60, total - 110));
        m_split->setSizes({ want, qMax(60, total - want) });
    }
    repaintAll();
    emit layoutChanged();
}

int TimelineTracksView::lanesPaneHeight() const { return m_wantLanesH; }
void TimelineTracksView::setLanesPaneHeight(int h)
{
    m_wantLanesH = h;
    if (h > 0) {
        const int total = qMax(m_split->height(), h + 60);
        m_split->setSizes({ h, qMax(60, total - h) });
    }
}

void TimelineTracksView::scrollToColumn(int col)
{
    const int viewW = qMax(1, width() - kGutter);
    m_hbar->setValue(qMax(0, int(xCol(col) - viewW * 0.62)));
}

void TimelineTracksView::refreshScroll()
{
    const int viewW = qMax(1, width() - kGutter);
    const int maxV = qMax(0, int(std::ceil(contentW())) - viewW);
    m_hbar->setRange(0, maxV);
    m_hbar->setPageStep(viewW);
    m_hbar->setSingleStep(24);
    m_hbarRow->setVisible(true);
}

void TimelineTracksView::repaintAll()
{
    m_ruler->update();
    m_lanes->invalidate();
    m_cast->update();
}

void TimelineTracksView::setHoverCol(int col)
{
    if (col == m_hoverCol) return;
    m_hoverCol = col;
    repaintAll();
}

void TimelineTracksView::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    refreshScroll();
    // painel lateral abrindo encolhe a área: mantém o evento selecionado à vista
    if (const Event* sel = m_data.event(m_sel); sel && sel->col >= 0) {
        const int viewW = qMax(1, width() - kGutter);
        const qreal x = xCol(sel->col) - sx();
        if (x > viewW - colW() / 2.0 || x < colW() / 2.0)
            m_hbar->setValue(qMax(0, int(xCol(sel->col) - viewW * 0.62)));
    }
    if (m_wantLanesH > 0) {
        const int total = m_split->height();
        const int h = qBound(60, m_wantLanesH, qMax(60, total - 69));
        m_split->setSizes({ h, qMax(60, total - h) });
    } else if (!m_data.cols.isEmpty()) {
        const int total = m_split->height();
        const int want = qBound(60, int(lanesContentH()) + 10, qMax(60, total - 110));
        m_split->setSizes({ want, qMax(60, total - want) });
    }
}
