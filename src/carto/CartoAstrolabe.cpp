#include "CartoAstrolabe.h"

#include "CartoPaint.h"
#include "../Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <cmath>

namespace carto {

namespace {
constexpr double kSize = 236, kMargin = 12;     // o instrumento + a sombra em volta
constexpr double RH = 106, RT = 88, RD = 72;     // aro da hora, arco da inclinação, disco das vistas
constexpr double DEG = M_PI / 180;
struct ViewSpot { const char* mode; double dx, dy; };
const ViewSpot kViews[4] = { { "full", 0, -50 }, { "atlas", 50, 0 }, { "ink", 0, 50 }, { "terrain", -50, 0 } };

QColor mix(const QColor& a, const QColor& b, double t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}
QString viewName(const QString& m)
{
    if (m == QLatin1String("full")) return Astrolabe::tr("Completo");
    if (m == QLatin1String("atlas")) return Astrolabe::tr("Atlas");
    if (m == QLatin1String("ink")) return Astrolabe::tr("Tinta");
    return Astrolabe::tr("Terreno");
}
}

Astrolabe::Astrolabe(QWidget* parent) : QWidget(parent)
{
    setFixedSize(int(kSize + 2 * kMargin), int(kSize + 2 * kMargin));
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setToolTip(tr("Arraste no meio pra girar · clique na agulha pro norte · arco da direita inclina · arraste o sol pra mudar a hora"));
}

void Astrolabe::setState(double ang, double pitch, double hour, const QString& mode)
{
    m_ang = ang; m_pitch = pitch; m_hour = hour; m_mode = mode;
    update();
}

void Astrolabe::setLockedMode(const QString& mode)
{
    if (m_locked == mode) return;
    m_locked = mode;
    update();
}

QPointF Astrolabe::local(QPointF p) const { return p - QPointF(kMargin + kSize / 2, kMargin + kSize / 2 - 2); }

void Astrolabe::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor panel = Theme::toColor(Theme::panelBackground()), line = Theme::toColor(Theme::panelBorder()), ink = Theme::toColor(Theme::textPrimary()),
                 muted = Theme::toColor(Theme::textMuted()), accent = Theme::toColor(Theme::accentDefault());
    const QColor panel2 = mix(panel, ink, 0.07);
    QColor accentSoft = accent; accentSoft.setAlphaF(0.2);
    const QPointF C(kMargin + kSize / 2, kMargin + kSize / 2 - 2);
    auto pt = [&](double r, double a) { return C + QPointF(r * std::cos(a), r * std::sin(a)); };

    // sombra macia embaixo do instrumento
    QRadialGradient sh(C + QPointF(0, 7), 124);
    sh.setColorAt(0.86, QColor(0, 0, 0, 70));
    sh.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setPen(Qt::NoPen); p.setBrush(sh);
    p.drawEllipse(C + QPointF(0, 7), 124, 124);

    p.setBrush(panel); p.setPen(QPen(line, 1));
    p.drawEllipse(C, 116, 116);

    // aro da hora: trilho, o dia (6h a 18h) e as marcas a cada 3h
    QPen track(panel2, 13); track.setCapStyle(Qt::FlatCap);
    p.setPen(track); p.setBrush(Qt::NoBrush);
    p.drawEllipse(C, RH, RH);
    QPen day(QColor(242, 178, 74, 72), 13); day.setCapStyle(Qt::FlatCap);
    p.setPen(day);
    p.drawArc(QRectF(C.x() - RH, C.y() - RH, 2 * RH, 2 * RH), 0, 180 * 16);
    p.setPen(QPen(line, 1.2));
    for (int k = 0; k < 8; k++) {
        const double a = (-90 - (k * 3 - 12) * 15) * DEG;
        p.drawLine(pt(RH - 6, a), pt(RH + 6, a));
    }

    // arco da inclinação (direita): de cima (90°) a baixo (20°)
    const double pa = ((90 - m_pitch) / 70 * 120 - 60) * DEG;
    const QRectF rt(C.x() - RT, C.y() - RT, 2 * RT, 2 * RT);
    QPen tt(panel2, 10); tt.setCapStyle(Qt::RoundCap);
    p.setPen(tt);
    p.drawArc(rt, int(60 * 16), int(-120 * 16));
    QPen ta(accent, 10); ta.setCapStyle(Qt::RoundCap);
    QColor ac = accent; ac.setAlphaF(0.85); ta.setColor(ac);
    p.setPen(ta);
    p.drawArc(rt, int(-pa / DEG * 16), int((-60 + pa / DEG) * 16));
    p.setPen(QPen(accent, 2.5)); p.setBrush(panel);
    p.drawEllipse(pt(RT, pa), 7, 7);

    // disco das vistas
    p.setPen(QPen(line, 1)); p.setBrush(panel2);
    p.drawEllipse(C, RD, RD);
    for (const ViewSpot& v : kViews) {
        const QString m = QString::fromLatin1(v.mode);
        const bool on = m == m_mode;
        const QRectF r(C.x() + v.dx - 31, C.y() + v.dy - 10, 62, 20);
        if (on) { p.setPen(Qt::NoPen); p.setBrush(accentSoft); p.drawRoundedRect(r, 10, 10); }
        QFont vf = font(); vf.setPixelSize(11); vf.setBold(on);
        QColor off = muted; off.setAlphaF(0.35);
        p.setFont(vf); p.setPen(on ? accent : m == m_locked ? off : muted);
        p.drawText(r, Qt::AlignCenter, viewName(m));
    }

    // o número da inclinação fica por cima do disco
    QFont f = font(); f.setPixelSize(11); f.setBold(true);
    p.setFont(f); p.setPen(ink);
    const QPointF lp = pt(RT - 21, pa);
    p.drawText(QRectF(lp.x() - 18, lp.y() - 8, 36, 16), Qt::AlignCenter, QStringLiteral("%1°").arg(qRound(m_pitch)));

    // agulha (gira com o mapa)
    p.save();
    p.translate(C);
    p.rotate(m_ang / DEG);
    QPainterPath north; north.moveTo(0, -30); north.lineTo(7, 0); north.lineTo(0, -5); north.lineTo(-7, 0); north.closeSubpath();
    QPainterPath south; south.moveTo(0, 30); south.lineTo(7, 0); south.lineTo(0, 5); south.lineTo(-7, 0); south.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(accent); p.drawPath(north);
    QColor ms = muted; ms.setAlphaF(0.5); p.setBrush(ms); p.drawPath(south);
    p.restore();
    p.setBrush(ink); p.drawEllipse(C, 3.5, 3.5);

    // o sol (ou a lua) no aro
    const double ha = (-90 - (m_hour - 12) * 15) * DEG;
    const QPointF sp = pt(RH, ha);
    const bool night = timeOfDay(m_hour).night > 0.5;
    p.setPen(QPen(panel, 3));
    if (night) {
        p.setBrush(QColor(223, 230, 242)); p.drawEllipse(sp, 9, 9);
        p.setPen(Qt::NoPen); p.setBrush(panel2); p.drawEllipse(sp + QPointF(3.5, -2.5), 6.5, 6.5);
    } else {
        p.setBrush(QColor(242, 178, 74)); p.drawEllipse(sp, 9, 9);
    }
}

QString Astrolabe::viewAt(QPointF l) const
{
    for (const ViewSpot& v : kViews)
        if (QRectF(v.dx - 31, v.dy - 11, 62, 22).contains(l)) return QString::fromLatin1(v.mode);
    return QString();
}

void Astrolabe::mousePressEvent(QMouseEvent* e)
{
    const QPointF l = local(e->position());
    const double d = std::hypot(l.x(), l.y()), a = std::atan2(l.y(), l.x());
    if (d > 118) { e->ignore(); return; }
    const QString v = d <= 76 ? viewAt(l) : QString();
    if (!v.isEmpty()) { emit modeRequested(v); return; }   // vista apagada: o painel recusa e avisa
    m_moved = false;
    if (d > 97) m_drag = Drag::Hour;
    else if (d > 76 && std::fabs(a) < 75 * DEG) m_drag = Drag::Tilt;
    else if (d <= 76) { m_drag = Drag::Rot; m_a0 = a; m_ang0 = m_ang; m_near = d < 30; }
    else { m_drag = Drag::None; return; }
    if (m_drag != Drag::Rot) dragTo(l);
}

void Astrolabe::dragTo(QPointF l)
{
    const double a = std::atan2(l.y(), l.x());
    if (m_drag == Drag::Hour) emit hourDragged(12 - (a / DEG + 90) / 15, false);
    else if (m_drag == Drag::Tilt) {
        const double d = std::max(-60.0, std::min(60.0, a / DEG));
        emit pitchDragged(90 - (d + 60) / 120 * 70, true);
    } else if (m_drag == Drag::Rot) {
        double d = a - m_a0; d = std::atan2(std::sin(d), std::cos(d));
        if (!m_moved && std::fabs(d) < 0.04) return;
        m_moved = true;
        emit angleDragged(m_ang0 + d, true);
    }
}

void Astrolabe::mouseMoveEvent(QMouseEvent* e)
{
    const QPointF l = local(e->position());
    if (m_drag != Drag::None) { dragTo(l); return; }
    const double d = std::hypot(l.x(), l.y());
    setCursor(d <= 76 && !viewAt(l).isEmpty() ? Qt::PointingHandCursor : d <= 118 ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void Astrolabe::mouseReleaseEvent(QMouseEvent*)
{
    const Drag d = m_drag;
    m_drag = Drag::None;
    if (d == Drag::Rot && !m_moved && m_near) emit northRequested();
    else if (d == Drag::Rot && m_moved) emit angleDragged(m_ang, false);
    else if (d == Drag::Tilt) emit pitchDragged(m_pitch, false);
    else if (d == Drag::Hour) emit hourDragged(m_hour, true);
}

} // namespace carto
