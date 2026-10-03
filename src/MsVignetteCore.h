#pragma once
// Peças comuns das vinhetas (MsVignette.cpp e MsVignetteLeva3.cpp): o mesmo sorteio e a mesma paleta dos concepts.

#include <QColor>
#include <QString>
#include <algorithm>
#include <cmath>

namespace MsVignetteDetail {

// Mesmo sorteio do concept (FNV-1a + mulberry32), pra o desenho de cada
// capítulo ser o que o autor viu na prévia.
inline quint32 hashStr(const QString& s) {
    quint32 h = 2166136261u;
    for (const QChar c : s) { h ^= c.unicode(); h *= 16777619u; }
    return h;
}

struct Rng {
    quint32 a;
    double next() {
        a += 0x6D2B79F5u;
        quint32 t = (a ^ (a >> 15)) * (1u | a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return double(t ^ (t >> 14)) / 4294967296.0;
    }
    double operator()() { return next(); }
    double R(double lo, double hi) { return lo + next() * (hi - lo); }
};

inline int jsRound(double v) { return int(std::floor(v + 0.5)); }

inline QColor hsl(double h, double s, double l, double a = 1.0) {
    h = std::fmod(h, 360.0);
    if (h < 0) h += 360.0;
    return QColor::fromHslF(float(h / 360.0), float(std::clamp(s, 0.0, 1.0)), float(std::clamp(l, 0.0, 1.0)),
                            float(std::clamp(a, 0.0, 1.0)));
}

// Paleta "Viva" (concept vinhetas-leva1, aprovada 2026-09-28): usa a cor da
// Parte de verdade — matiz E saturação. A antiga fixava a saturação em ~.30 e
// tudo saía lavado. Cor apagada continua apagada; viva continua viva.
struct Pal {
    double h = 0, S = 0;
    bool gray = false;
    QColor bg0, bg1, ink, ink2, faint, ring;
    // tons de papel (leva 3: azulejo, cordel, mapa, folhas, porcelana)
    QColor paper, paper2, paperInk, parch, land;
    QColor shade(double l, double a = 1.0) const { return hsl(h, S * .75, std::min(1.0, l + .04), a); }
    // O "s" pedido pelo desenho é ignorado de propósito (como no concept): a
    // saturação vem da cor da Parte.
    QColor glow(double dh, double /*s*/, double l, double a) const {
        return hsl(h + dh * .8, gray ? S : std::clamp(S + .1, 0.0, 1.0), l, a);
    }
};

inline Pal paletteFor(const QColor& base) {
    Pal P;
    const QColor c = base.isValid() ? base.toHsl() : QColor(Qt::gray).toHsl();
    const double hue = c.hslHueF();
    P.h = hue < 0 ? 0.0 : hue * 360.0;
    const double s = std::max(0.0, double(c.hslSaturationF()));
    P.gray = s < .12;
    P.S = P.gray ? s : std::clamp(s, .45, 1.0);
    const double h = P.h, S = P.S;
    P.bg0 = hsl(h, S * .85, .30);
    P.bg1 = hsl(h + 14, S * .8, .11);
    P.ink = hsl(h, P.gray ? S : std::max(S, .6), .72);
    P.ink2 = hsl(h + 28, P.gray ? S : std::clamp(S + .15, 0.0, 1.0), .78);
    P.faint = hsl(h, S * .8, .6, .5);
    P.ring = hsl(h, S, .62, .8);
    P.paper = hsl(h, S * .22, .88);
    P.paper2 = hsl(h, S * .25, .74);
    P.paperInk = hsl(h, std::clamp(S * .85, 0.0, 1.0), .24);
    P.parch = hsl(h + 22, S * .32, .82);
    P.land = hsl(h + 22, S * .4, .66);
    return P;
}


}
