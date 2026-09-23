#include "TimelineBraidView.h"

#include "Theme.h"

#include <QCoreApplication>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <algorithm>
#include <cmath>

using namespace Tracks;

namespace {
constexpr qreal kDay = 1440.0;
}

TimelineBraidView::TimelineBraidView(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void TimelineBraidView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Palette pal = Palette::current();
    p.fillRect(rect(), pal.page);
    m_paths.clear();

    const Data& d = m_data;
    const qreal W = width(), H = height();
    const qreal topY = 104, botY = std::max(topY + 220.0, H - 206.0), x0 = 70;
    const int n = d.cols.size();
    const qreal stepT = n > 1 ? (W - 150 - x0) / (n - 1) : 0;
    auto xT = [&](int c) { return x0 + c * stepT; };

    // ── escala da história (com quebras nos saltos > 1 ano) ──────────────────
    QList<qreal> times;
    for (const Event& e : d.events)
        if (!e.manual && e.col >= 0 && e.chronoOk && !times.contains(e.chrono)) times << e.chrono;
    std::sort(times.begin(), times.end());
    struct Seg { qreal w; bool brk; qreal days; };
    QList<Seg> segs;
    for (int i = 1; i < times.size(); ++i) {
        const qreal dd = (times[i] - times[i - 1]) / kDay;
        segs.append(dd > 365 ? Seg{ 64, true, dd } : Seg{ 26 + 12 * std::log2(1 + std::max(0.0, dd)), false, dd });
    }
    qreal total = 0; for (const Seg& s : segs) total += s.w;
    const qreal avail = (W - 190) - x0;
    const qreal k = total > 0 ? avail / total : 1;
    QMap<qreal, qreal> pos;
    {
        qreal acc = times.size() == 1 ? x0 + avail / 2 : x0;
        if (!times.isEmpty()) pos.insert(times[0], acc);
        for (int i = 0; i < segs.size(); ++i) { acc += segs[i].w * k; pos.insert(times[i + 1], acc); }
    }
    const qreal binX = W - 110;

    const QFont fCap = [] { QFont f = uiFont(9.5, QFont::Bold); f.setLetterSpacing(QFont::PercentageSpacing, 114); return f; }();
    const QFont fCapSub = uiFont(11);

    // região "antes do início"
    if (d.startOk && !times.isEmpty() && times.first() < d.startChrono) {
        qreal firstDay = -1;
        for (qreal t : times) if (t >= d.startChrono) { firstDay = pos.value(t); break; }
        if (firstDay < 0) firstDay = pos.value(times.last()) + 30;
        p.setPen(Qt::NoPen);
        p.setBrush(alpha(pal.info, 0.07));
        p.drawRoundedRect(QRectF(x0 - 30, botY - 26, firstDay - x0 + 10, 84), 8, 8);
        p.setFont(fCap);
        p.setPen(alpha(pal.info, 0.85));
        p.drawText(QPointF(x0 - 18, botY + 48), tr("ANTES DO INÍCIO"));
        QPen dash(alpha(pal.info, 0.5), 1, Qt::CustomDashLine);
        dash.setDashPattern({ 3, 3 });
        p.setPen(dash);
        p.drawLine(QPointF(firstDay - 14, botY - 30), QPointF(firstDay - 14, botY + 30));
    }

    // legendas dos eixos
    p.setFont(fCap); p.setPen(pal.dim);
    p.drawText(QPointF(x0 - 30, topY - 54), tr("ORDEM DE LEITURA"));
    p.drawText(QPointF(x0 - 30, botY + 72), tr("ORDEM DA HISTÓRIA"));
    p.setFont(fCapSub);
    p.drawText(QPointF(x0 - 30, topY - 40), tr("como o leitor encontra"));
    p.drawText(QPointF(x0 - 30, botY + 86), tr("quando aconteceu"));

    // eixo de cima
    const QColor axis = alpha(pal.ink, 0.28);
    if (n > 0) {
        p.setPen(QPen(axis, 1.5));
        p.drawLine(QPointF(xT(0) - 16, topY), QPointF(xT(n - 1) + 16, topY));
        const QFont fn = monoFont(11), fnb = monoFont(11, QFont::Medium);
        for (int c = 0; c < n; ++c) {
            const bool ed = c == d.editorCol;
            p.setFont(ed ? fnb : fn);
            p.setPen(ed ? pal.accent : pal.dim);
            const QString t = d.cols[c].ruler;
            p.drawText(QPointF(xT(c) - QFontMetricsF(p.font()).horizontalAdvance(t) / 2, topY - 14), t);
        }
        if (d.editorCol >= 0) {
            const QFont fe = uiFont(9.5, QFont::Bold);
            p.setFont(fe); p.setPen(pal.accent);
            const QString t = tr("no editor");
            p.drawText(QPointF(xT(d.editorCol) - QFontMetricsF(fe).horizontalAdvance(t) / 2, topY - 30), t);
        }
    }

    // eixo de baixo, com quebras
    for (int i = 0; i < segs.size(); ++i) {
        const qreal a = pos.value(times[i]), b = pos.value(times[i + 1]);
        p.setPen(QPen(axis, 1.5));
        if (segs[i].brk) {
            const qreal m = (a + b) / 2;
            p.drawLine(QPointF(a, botY), QPointF(m - 9, botY));
            p.drawLine(QPointF(m + 9, botY), QPointF(b, botY));
            p.setPen(QPen(pal.dim, 1.4));
            p.drawLine(QPointF(m - 12, botY + 6), QPointF(m - 4, botY - 6));
            p.drawLine(QPointF(m + 4, botY + 6), QPointF(m + 12, botY - 6));
            const QFont f = monoFont(10);
            p.setFont(f); p.setPen(pal.dim);
            const QString t = gapWord(segs[i].days);
            p.drawText(QPointF(m - QFontMetricsF(f).horizontalAdvance(t) / 2, botY - 14), t);
        } else {
            p.drawLine(QPointF(a, botY), QPointF(b, botY));
        }
    }
    {
        const QFont f = monoFont(10.5);
        p.setFont(f); p.setPen(pal.ink);
        for (int i = 0; i < times.size(); ++i) {
            QString mk;
            for (const Event& e : d.events) if (!e.manual && e.chronoOk && e.chrono == times[i]) { mk = e.marker; break; }
            const qreal x = pos.value(times[i]);
            const bool close = i % 2 && x - pos.value(times[i - 1]) < 52;
            p.drawText(QPointF(x - QFontMetricsF(f).horizontalAdvance(mk) / 2, botY + (close ? 34 : 20)), mk);
        }
    }

    // bandeja "sem data"
    int nd = 0;
    for (const Event& e : d.events) if (!e.manual && e.col >= 0 && e.hollow) ++nd;
    if (nd > 0) {
        QPen dash(alpha(pal.warning, 0.7), 1, Qt::CustomDashLine);
        dash.setDashPattern({ 4, 3 });
        p.setPen(dash);
        p.setBrush(alpha(pal.warning, 0.08));
        p.drawRoundedRect(QRectF(binX - 44, botY - 18, 88, 36), 8, 8);
        const QFont f = uiFont(11);
        p.setFont(f); p.setPen(pal.warning);
        const QString t = tr("sem data · %1").arg(nd);
        p.drawText(QPointF(binX - QFontMetricsF(f).horizontalAdvance(t) / 2, botY + 4), t);
    }

    // fios
    const bool hot = !m_hover.isEmpty();
    struct Knot { QPointF c; QColor col; bool hollow; bool sel; bool dim; };
    QList<Knot> knots;
    for (const Event& e : d.events) {
        if (e.manual || e.col < 0) continue;
        const Lane* L = d.lane(e.laneId);
        const qreal xa = xT(e.col);
        const qreal xb = e.hollow || !e.chronoOk ? binX : pos.value(e.chrono);
        const qreal ya = topY + 7, yb = botY - (e.hollow ? 19 : 7);
        const qreal my = (topY + botY) / 2;
        QPainterPath path(QPointF(xa, ya));
        path.cubicTo(xa, my + 10, xb, my - 10, xb, yb);
        m_paths.insert(e.id, path);
        const QColor col = e.hollow ? pal.warning : (L ? L->color : pal.accent);
        const bool on = e.id == m_sel || e.id == m_hover;
        const bool dim = m_filter.dims(e);
        p.save();
        qreal op = dim ? 0.12 : 1.0;
        if (hot && !on) op = std::min(op, 0.18);
        p.setOpacity(op);
        QPen pen(col, on ? 4.5 : 3.0, Qt::SolidLine, Qt::RoundCap);
        if (e.hollow) { pen.setStyle(Qt::CustomDashLine); pen.setDashPattern({ 3 / pen.widthF(), 4 / pen.widthF() }); }
        else if (L && L->dashed) { pen.setStyle(Qt::CustomDashLine); pen.setDashPattern({ 8 / pen.widthF(), 6 / pen.widthF() }); }
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.restore();
        knots.append({ QPointF(xa, topY), col, e.hollow, e.id == m_sel, dim });
        if (!e.hollow && e.chronoOk) knots.append({ QPointF(xb, botY), col, false, false, dim });
    }
    for (const Knot& kn : knots) {
        p.save();
        if (kn.dim) p.setOpacity(0.12);
        if (kn.sel) { p.setPen(Qt::NoPen); p.setBrush(alpha(kn.col, 0.22)); p.drawEllipse(kn.c, 10, 10); }
        drawKnot(&p, kn.c, 6.5, kn.col, pal.page, kn.hollow, false, false, false);
        p.restore();
    }

    // legenda
    {
        qreal lx = x0 - 30;
        const qreal ly = H - 34;
        const QFont f = uiFont(11.5);
        p.setFont(f);
        for (const Lane& L : d.lanes) {
            QPen pen(L.color, 3);
            if (L.dashed) { pen.setStyle(Qt::CustomDashLine); pen.setDashPattern({ 5 / 3.0, 4 / 3.0 }); }
            p.setPen(pen);
            p.drawLine(QPointF(lx, ly), QPointF(lx + 20, ly));
            p.setPen(pal.ink);
            p.drawText(QPointF(lx + 27, ly + 4), L.name);
            lx += 27 + QFontMetricsF(f).horizontalAdvance(L.name) + 26;
        }
        p.setPen(pal.dim);
        const QString t = tr("Fio que cruza pra trás é flashback. Fios que caem no mesmo ponto acontecem no mesmo dia.");
        const qreal tw = QFontMetricsF(f).horizontalAdvance(t);
        if (W - 30 - tw > lx) p.drawText(QPointF(W - 30 - tw, ly + 4), t);
    }

    // cartão de hover
    if (const Event* e = d.event(m_hover)) {
        const Lane* L = d.lane(e->laneId);
        const QString top = tr("Cap %1 · %2 · %3").arg(e->col >= 0 && e->col < n ? d.cols[e->col].ruler : QString(),
                                                      e->hollow ? tr("sem data") : e->marker,
                                                      L ? L->name : QString());
        const QFont f1 = monoFont(10.5), f2 = serifFont(13);
        const qreal w = std::min(320.0, std::max(QFontMetricsF(f1).horizontalAdvance(top),
                                                 QFontMetricsF(f2).horizontalAdvance(e->title)) + 22);
        const QRectF card(W - 16 - w, 14, w, 44);
        p.setPen(QPen(pal.border, 1));
        p.setBrush(pal.panel);
        p.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), Theme::controlRadius(), Theme::controlRadius());
        p.setFont(f1); p.setPen(pal.dim);
        p.drawText(QPointF(card.left() + 11, card.top() + 8 + QFontMetricsF(f1).ascent()), top);
        p.setFont(f2); p.setPen(pal.bright);
        p.drawText(QPointF(card.left() + 11, card.top() + 23 + QFontMetricsF(f2).ascent()),
                   QFontMetricsF(f2).elidedText(e->title, Qt::ElideRight, w - 22));
    }
}

QString TimelineBraidView::ribbonAt(const QPointF& pos) const
{
    QPainterPathStroker st;
    st.setWidth(14);
    for (auto it = m_paths.constBegin(); it != m_paths.constEnd(); ++it)
        if (st.createStroke(it.value()).contains(pos)) return it.key();
    return {};
}

void TimelineBraidView::mouseMoveEvent(QMouseEvent* e)
{
    const QString h = ribbonAt(e->position());
    if (h != m_hover) {
        m_hover = h;
        setCursor(h.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
        update();
    }
}

void TimelineBraidView::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    const QString id = ribbonAt(e->position());
    if (id.isEmpty()) emit backgroundClicked();
    else emit eventClicked(id);
}

void TimelineBraidView::leaveEvent(QEvent*)
{
    if (!m_hover.isEmpty()) { m_hover.clear(); update(); }
}
