#include "GalaxyHab.h"

#include <QCoreApplication>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>

namespace galaxy {

Rgb habRgb(int n)
{
    switch (n) {
    case 1: return { 72, 214, 150 };
    case 2: return { 158, 214, 88 };
    case 3: return { 236, 200, 72 };
    case 4: return { 240, 140, 62 };
    case 5: return { 226, 72, 72 };
    case 6: return { 132, 124, 178 };
    default: return { 160, 170, 190 };
    }
}
QColor habColor(int n, double a)
{
    const Rgb c = habRgb(n);
    return QColor(c[0], c[1], c[2], int(clamp(a, 0, 1) * 255 + 0.5));
}
const char* habRoman(int n)
{
    static const char* const R[7] = { "", "I", "II", "III", "IV", "V", "VI" };
    return n >= 0 && n <= 6 ? R[n] : "";
}
QString habName(int n)
{
    switch (n) {
    case 1: return QCoreApplication::translate("galaxy::Hab", "Éden");
    case 2: return QCoreApplication::translate("galaxy::Hab", "Áspero");
    case 3: return QCoreApplication::translate("galaxy::Hab", "Marginal");
    case 4: return QCoreApplication::translate("galaxy::Hab", "Hostil");
    case 5: return QCoreApplication::translate("galaxy::Hab", "Letal");
    case 6: return QCoreApplication::translate("galaxy::Hab", "Sem chão");
    default: return QCoreApplication::translate("galaxy::Hab", "Construído");
    }
}
QString habDesc(int n)
{
    switch (n) {
    case 1: return QCoreApplication::translate("galaxy::Hab", "Habitável sem equipamento: atmosfera, temperatura e gravidade compatíveis com a vida humana.");
    case 2: return QCoreApplication::translate("galaxy::Hab", "Atmosfera respirável, mas temperatura, gravidade ou radiação exigem adaptação.");
    case 3: return QCoreApplication::translate("galaxy::Hab", "Exige máscara e proteção. Colonização possível apenas com grande investimento.");
    case 4: return QCoreApplication::translate("galaxy::Hab", "Exige traje permanente. Assentamentos apenas em ambientes fechados e pressurizados.");
    case 5: return QCoreApplication::translate("galaxy::Hab", "Incompatível com a permanência humana, mesmo com traje. Acesso limitado a missões curtas e robóticas.");
    case 6: return QCoreApplication::translate("galaxy::Hab", "Sem superfície sólida. Presença possível apenas em órbita ou em estações.");
    default: return QString();
    }
}
QString habLabel(int n) { return n ? QStringLiteral("%1 · %2").arg(QString::fromLatin1(habRoman(n)), habName(n)) : habName(0); }

QString habWhy(const HabInfo& i, const QString& sep)
{
    const QLocale loc;
    QStringList out;
    for (const HabWhy& w : i.why) {
        const QString C = loc.toString(w.v, 'f', 0);
        switch (w.code) {
        case HabWhy::Built: out << QCoreApplication::translate("galaxy::Hab", "Mundo construído: o ambiente é mantido por máquinas"); break;
        case HabWhy::NoGroundGas: out << QCoreApplication::translate("galaxy::Hab", "Sem superfície: gás em toda a extensão"); break;
        case HabWhy::NoGroundHot: out << QCoreApplication::translate("galaxy::Hab", "Sem superfície: gás em temperatura extrema"); break;
        case HabWhy::NoGroundIce: out << QCoreApplication::translate("galaxy::Hab", "Sem superfície: gás e gelo sob alta pressão"); break;
        case HabWhy::NoGroundThick: out << QCoreApplication::translate("galaxy::Hab", "Sem superfície: apenas atmosfera densa"); break;
        case HabWhy::NoGroundLava: out << QCoreApplication::translate("galaxy::Hab", "Superfície de rocha derretida"); break;
        case HabWhy::NoGroundVolcano: out << QCoreApplication::translate("galaxy::Hab", "Superfície vulcânica e rocha derretida"); break;
        case HabWhy::Breathable: out << QCoreApplication::translate("galaxy::Hab", "Atmosfera respirável"); break;
        case HabWhy::TooHot: out << QCoreApplication::translate("galaxy::Hab", "Temperatura alta, %1 °C").arg(C); break;
        case HabWhy::TooCold: out << QCoreApplication::translate("galaxy::Hab", "Temperatura baixa, %1 °C").arg(C); break;
        case HabWhy::Heavy: out << QCoreApplication::translate("galaxy::Hab", "Gravidade alta, %1 g").arg(loc.toString(w.v, 'f', 1)); break;
        case HabWhy::Light: out << QCoreApplication::translate("galaxy::Hab", "Gravidade baixa, %1 g").arg(loc.toString(w.v, 'f', w.v < 0.1 ? 3 : 1)); break;
        case HabWhy::Poison: out << QCoreApplication::translate("galaxy::Hab", "Atmosfera tóxica, %1 °C").arg(C); break;
        case HabWhy::ThinAir: out << QCoreApplication::translate("galaxy::Hab", "Atmosfera rarefeita demais para respirar"); break;
        case HabWhy::NoAir: out << QCoreApplication::translate("galaxy::Hab", "Sem atmosfera"); break;
        case HabWhy::Locked: out << QCoreApplication::translate("galaxy::Hab", "Rotação travada: um hemisfério em dia permanente"); break;
        case HabWhy::Radiation: out << QCoreApplication::translate("galaxy::Hab", "Radiação estelar intensa"); break;
        }
    }
    return out.join(sep);
}
QString atmText(const Body& b)
{
    if (b.air > 0) return QCoreApplication::translate("galaxy::Hab", "Respirável");
    if (b.air < 0) { Body q; q.atm = b.atm; if (airOf(q) == AirKind::Breathable) return QCoreApplication::translate("galaxy::Hab", "Não respirável"); }
    return QCoreApplication::translate("galaxy::Core", b.atm.c_str());
}
QLabel* habPill(int n, QWidget* parent)
{
    auto* l = new QLabel(QString::fromLatin1(habRoman(n)), parent);
    const Rgb c = habRgb(n);
    l->setStyleSheet(QStringLiteral("QLabel { background: rgb(%1,%2,%3); color: #0a0d14; border: none; border-radius: 6px; padding: 2px 6px; font-weight: 700; font-size: 11px; font-family: 'IBM Plex Mono', monospace; }").arg(c[0]).arg(c[1]).arg(c[2]));
    l->setAlignment(Qt::AlignCenter);
    l->setMinimumWidth(26);
    l->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return l;
}

HabBar::HabBar(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(8);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}
void HabBar::setCounts(const std::array<int, 7>& c) { m_c = c; update(); }
void HabBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()), 4, 4);
    p.setClipPath(clip);
    p.fillRect(rect(), QColor(255, 255, 255, 15));
    int shown = 0, sum = 0;
    for (int n = 1; n <= 6; n++) { sum += m_c[size_t(n)]; if (m_c[size_t(n)]) shown++; }
    if (!sum) return;
    // Cada classe que existe ganha ao menos uma risquinha, mesmo se forem poucos mundos.
    const double room = width() - std::max(0, shown - 1) - 2.0 * shown;
    double x = 0;
    for (int n = 1; n <= 6; n++) {
        const int v = m_c[size_t(n)];
        if (!v) continue;
        const double w = 2 + room * double(v) / sum;
        p.fillRect(QRectF(x, 0, w, height()), habColor(n));
        x += w + 1;
    }
}

} // namespace galaxy
