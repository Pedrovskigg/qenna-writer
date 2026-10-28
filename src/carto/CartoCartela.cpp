#include "CartoCartela.h"

#include "CartoIcons.h"
#include "../Theme.h"

#include <QFontMetricsF>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace carto {

namespace {
const QColor kPaper(240, 231, 208), kInk(40, 33, 28), kSand(196, 132, 70);
constexpr int kW = 300, kOut = 6, kPadX = 14, kPadTop = 12, kPadBot = 10, kRowH = 23;
QFont fellFont(bool sc, bool italic, int px)
{
    QFont f;
    f.setFamilies({ sc ? QStringLiteral("IM FELL English SC") : QStringLiteral("IM FELL English"), QStringLiteral("Georgia"), QStringLiteral("serif") });
    f.setItalic(italic);
    f.setPixelSize(px);
    return f;
}
}

Cartela::Cartela(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
}

void Cartela::setItems(const std::vector<CartelaItem>& items) { m_items = items; updateGeometry(); resize(sizeHint()); update(); }
void Cartela::setTitle(const QString& t, const QString& sub)
{
    if (t == m_title && sub == m_sub) return;
    m_title = t; m_sub = sub;
    update();
}
void Cartela::setLeaguePx(double px) { if (std::fabs(px - m_leaguePx) > 1e-6) { m_leaguePx = px; update(); } }
void Cartela::setActive(int index) { if (index != m_active) { m_active = index; update(); } }
void Cartela::setMinimized(bool on) { m_min = on; resize(sizeHint()); update(); }

int Cartela::itemsTop() const { return kOut + 1 + kPadTop + 26 + 16 + 9 + 22 + 9; }

QSize Cartela::sizeHint() const
{
    if (m_min) return QSize(kW, kOut + 1 + kPadTop + 26 + 16 + kPadBot + 1 + kOut);
    const int rows = static_cast<int>((m_items.size() + 1) / 2);
    return QSize(kW, itemsTop() + rows * kRowH + kPadBot + 1 + kOut);
}

QRectF Cartela::itemRect(int i) const
{
    const double colW = (kW - 2 * (kOut + 1) - 2 * kPadX - 8) / 2.0;
    const double x = kOut + 1 + kPadX + (i % 2) * (colW + 8);
    return QRectF(x, itemsTop() + (i / 2) * kRowH, colW, kRowH - 1);
}
QRectF Cartela::minRect() const { return QRectF(width() - kOut - 1 - 26, kOut + 1 + 4, 22, 22); }

int Cartela::itemAt(QPointF p) const
{
    if (m_min) return -1;
    for (int i = 0; i < static_cast<int>(m_items.size()); i++) if (itemRect(i).contains(p)) return i;
    return -1;
}

void Cartela::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const QRectF outer = QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75);
    // sombra curta, papel, filete duplo de nanquim
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 50));
    p.drawRect(outer.translated(0, 3));
    p.setBrush(kPaper);
    p.setPen(QPen(kInk, 1.5));
    p.drawRect(outer);
    const QRectF inner = QRectF(rect()).adjusted(kOut + 0.5, kOut + 0.5, -kOut - 0.5, -kOut - 0.5);
    p.setPen(QPen(kInk, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(inner);

    // título e região
    double y = kOut + 1 + kPadTop;
    QFont tf = fellFont(true, false, 22);
    tf.setLetterSpacing(QFont::PercentageSpacing, 108);
    p.setFont(tf);
    p.setPen(kInk);
    const QFontMetricsF tfm(tf);
    const QString title = tfm.elidedText(m_title, Qt::ElideRight, inner.width() - 40);
    p.drawText(QRectF(inner.left(), y, inner.width(), 26), Qt::AlignHCenter | Qt::AlignVCenter, title);
    y += 26;
    p.setFont(fellFont(false, true, 13));
    p.drawText(QRectF(inner.left(), y, inner.width(), 16), Qt::AlignHCenter | Qt::AlignVCenter, m_sub);
    y += 16;

    if (m_interactive) {
        // recolher / abrir a legenda
        const QRectF mr = minRect();
        QPainterPath ch;
        const QPointF c = mr.center();
        if (m_min) { ch.moveTo(c.x() - 5, c.y() + 2.5); ch.lineTo(c.x(), c.y() - 2.5); ch.lineTo(c.x() + 5, c.y() + 2.5); }
        else { ch.moveTo(c.x() - 5, c.y() - 2.5); ch.lineTo(c.x(), c.y() + 2.5); ch.lineTo(c.x() + 5, c.y() - 2.5); }
        QPen cp(kInk, 2); cp.setCapStyle(Qt::RoundCap); cp.setJoinStyle(Qt::RoundJoin);
        p.strokePath(ch, cp);
    }
    if (m_min) return;

    // escala gráfica: 1 légua = 6 km
    y += 9;
    static const int NICE[] = { 1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000 };
    int N = NICE[0];
    for (int n : NICE) if (n * m_leaguePx <= 230) N = n;
    const double bw = std::round(std::max(30.0, std::min(250.0, N * m_leaguePx)));
    const double bx = (width() - bw) / 2;
    p.setPen(QPen(kInk, 1));
    for (int k = 0; k < 4; k++) {
        p.setBrush(k % 2 ? kPaper : kInk);
        p.drawRect(QRectF(bx + bw * k / 4, y, bw / 4, 7));
    }
    QFont nf = fellFont(false, false, 11);
    p.setFont(nf);
    const QFontMetricsF nfm(nf);
    const QLocale loc;
    auto num = [&](double x, const QString& t) { p.drawText(QPointF(x - nfm.horizontalAdvance(t) / 2, y + 9 + nfm.ascent()), t); };
    num(bx, QStringLiteral("0"));
    if (N % 2 == 0) num(bx + bw / 2, loc.toString(N / 2));
    const QString last = N == 1 ? tr("1 légua") : tr("%1 léguas").arg(loc.toString(N));
    num(std::min(bx + bw, width() - kOut - 4 - nfm.horizontalAdvance(last) / 2), last);

    // legenda (é também a paleta)
    QFont lf = fellFont(false, false, 14);
    p.setFont(lf);
    for (int i = 0; i < static_cast<int>(m_items.size()); i++) {
        const QRectF r = itemRect(i);
        if (i == m_active) {
            QColor soft = kSand; soft.setAlpha(56);
            p.setBrush(soft);
            p.setPen(QPen(kSand, 1.5));
            p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
        } else if (i == m_hover && m_interactive) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(40, 33, 28, 23));
            p.drawRoundedRect(r, 5, 5);
        }
        p.drawPixmap(QPointF(r.left() + 5, r.center().y() - 8.5), iconPixmap(m_items[i].icon, kInk, 17, devicePixelRatioF()));
        p.setPen(kInk);
        p.drawText(QRectF(r.left() + 29, r.top(), r.width() - 30, r.height()), Qt::AlignVCenter | Qt::AlignLeft, m_items[i].label);
    }
}

void Cartela::mousePressEvent(QMouseEvent* e)
{
    if (!m_interactive) return;
    if (minRect().contains(e->position())) { setMinimized(!m_min); emit minimizeToggled(m_min); return; }
    const int i = itemAt(e->position());
    if (i >= 0) emit itemPicked(i);
}
void Cartela::mouseMoveEvent(QMouseEvent* e)
{
    const int i = itemAt(e->position());
    if (i != m_hover) { m_hover = i; update(); }
    setCursor(i >= 0 || minRect().contains(e->position()) ? Qt::PointingHandCursor : Qt::ArrowCursor);
}
void Cartela::leaveEvent(QEvent*) { if (m_hover >= 0) { m_hover = -1; update(); } }

// ── rosa dos ventos ──
Rose::Rose(QWidget* parent) : QWidget(parent)
{
    setFixedSize(96, 96);
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::OpenHandCursor);
    setToolTip(tr("Rosa dos ventos: arraste pra girar, clique pro norte"));
}

void Rose::paintEvent(QPaintEvent*)
{
    // bússola (no lugar da Cartela): disco do painel, os 8 rumos girando com o mapa (letras sempre de pé)
    // e a agulha com o norte na cor de destaque
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor panel = Theme::toColor(Theme::panelBackground()), line = Theme::toColor(Theme::panelBorder());
    const QColor accent = Theme::toColor(Theme::accentDefault()), muted = Theme::toColor(Theme::textMuted()), ink = Theme::toColor(Theme::textPrimary());
    const double R = width() / 2.0 - 1.5;
    p.translate(width() / 2.0, height() / 2.0);
    p.setPen(QPen(line, 1));
    p.setBrush(panel);
    p.drawEllipse(QPointF(0, 0), R, R);
    // risquinhos na borda (16)
    for (int i = 0; i < 16; i++) {
        const double a = m_ang + i * M_PI / 8, sn = std::sin(a), cs = -std::cos(a);
        const double r0 = R - (i % 2 ? 3 : 5);
        p.setPen(QPen(i % 2 ? line : muted, i % 4 ? 1 : 1.4));
        p.drawLine(QPointF(sn * r0, cs * r0), QPointF(sn * (R - 1.5), cs * (R - 1.5)));
    }
    // os rumos: cardeais maiores, colaterais menores
    static const char* kDirs[8] = { QT_TR_NOOP("N"), QT_TR_NOOP("NE"), QT_TR_NOOP("L"), QT_TR_NOOP("SE"), QT_TR_NOOP("S"), QT_TR_NOOP("SO"), QT_TR_NOOP("O"), QT_TR_NOOP("NO") };
    QFont big = font(); big.setPixelSize(qRound(R * 0.25)); big.setBold(true);
    QFont small = font(); small.setPixelSize(qRound(R * 0.17)); small.setBold(true);
    for (int i = 0; i < 8; i++) {
        const bool card = i % 2 == 0;
        const double a = m_ang + i * M_PI / 4, r = R * (card ? 0.72 : 0.74);
        const QPointF c(std::sin(a) * r, -std::cos(a) * r);
        p.setFont(card ? big : small);
        p.setPen(i == 0 ? accent : card ? ink : muted);
        p.drawText(QRectF(c.x() - R * 0.3, c.y() - R * 0.15, R * 0.6, R * 0.3), Qt::AlignCenter, tr(kDirs[i]));
    }
    // agulha
    p.rotate(m_ang * 180 / M_PI);
    const double n = R * 0.48, w = R * 0.11;
    QPolygonF north; north << QPointF(0, -n) << QPointF(w, 0) << QPointF(-w, 0);
    QPolygonF south; south << QPointF(0, n) << QPointF(w, 0) << QPointF(-w, 0);
    p.setPen(Qt::NoPen);
    p.setBrush(accent); p.drawPolygon(north);
    QColor s = muted; s.setAlphaF(0.55);
    p.setBrush(s); p.drawPolygon(south);
    p.setBrush(panel); p.drawEllipse(QPointF(0, 0), w * 0.5, w * 0.5);
}

void Rose::mousePressEvent(QMouseEvent* e)
{
    const QPointF c(width() / 2.0, height() / 2.0), d = e->position() - c;
    m_a0 = std::atan2(d.y(), d.x());
    m_ang0 = m_ang;
    m_drag = true;
    m_moved = false;
    setCursor(Qt::ClosedHandCursor);
}
void Rose::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_drag) return;
    const QPointF c(width() / 2.0, height() / 2.0), d = e->position() - c;
    double a = std::atan2(d.y(), d.x()) - m_a0;
    a = std::atan2(std::sin(a), std::cos(a));
    if (!m_moved && std::fabs(a) < 0.04) return;
    m_moved = true;
    emit angleDragged(m_ang0 + a, true);
}
void Rose::mouseReleaseEvent(QMouseEvent*)
{
    if (!m_drag) return;
    m_drag = false;
    setCursor(Qt::OpenHandCursor);
    if (!m_moved) emit northRequested();
    else emit angleDragged(m_ang, false);
}

} // namespace carto
