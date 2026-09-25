#include "MsVignette.h"

#include <QCoreApplication>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>

namespace {

// Mesmo sorteio do concept (FNV-1a + mulberry32), pra o desenho de cada
// capítulo ser o que o autor viu na prévia.
quint32 hashStr(const QString& s) {
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

int jsRound(double v) { return int(std::floor(v + 0.5)); }

QColor hsl(double h, double s, double l, double a = 1.0) {
    h = std::fmod(h, 360.0);
    if (h < 0) h += 360.0;
    return QColor::fromHslF(float(h / 360.0), float(s), float(l), float(a));
}

const char* const kIds[MsVignette::FamilyCount] = {
    "branches", "roots", "flames", "city", "stars", "mountains", "coral",
    "cracks", "mandala", "lightning", "waves", "circles", "rays" };

void drawFamily(QPainter& g, int fam, Rng& rnd, double grow, double VW, double VH, double hue) {
    const double CX = VW / 2, CY = VH / 2, wide = VW / 104.0, PI = M_PI;
    const QColor ink = hsl(hue, .55, .70), ink2 = hsl(hue + 35, .70, .72), faint = hsl(hue, .40, .55, .45);
    auto pen = [&](const QColor& c, double w) {
        QPen p(c, w);
        p.setCapStyle(Qt::RoundCap);
        p.setJoinStyle(Qt::RoundJoin);
        g.setPen(p);
        g.setBrush(Qt::NoBrush);
    };
    auto dot = [&](double x, double y, double r, const QColor& c) {
        g.setPen(Qt::NoPen);
        g.setBrush(c);
        g.drawEllipse(QPointF(x, y), r, r);
    };
    auto spots = [&](int n) {
        QList<double> xs;
        for (int i = 0; i < n; ++i) xs << VW * (i + .5) / n + rnd.R(-8, 8);
        return xs;
    };

    switch (fam) {
    case MsVignette::Branches: {
        const int maxD = 2 + jsRound(grow * 5);
        std::function<void(double, double, double, double, int)> br = [&](double x, double y, double len, double ang, int d) {
            const double x2 = x + std::cos(ang) * len, y2 = y + std::sin(ang) * len;
            pen(ink, std::max(.5, d * .7));
            g.drawLine(QPointF(x, y), QPointF(x2, y2));
            if (d <= 0) { const double r = rnd.R(.8, 2); dot(x2, y2, r, ink2); return; }
            const int n = rnd() < .3 ? 3 : 2;
            for (int i = 0; i < n; ++i) {
                const double l2 = len * rnd.R(.62, .82);
                const double a2 = ang + rnd.R(-.75, .75);
                br(x2, y2, l2, a2, d - 1);
            }
        };
        const QList<double> xs = spots(std::max(1, jsRound(wide)));
        for (int i = 0; i < xs.size(); ++i) {
            const double len = rnd.R(20, 28) * (i ? .8 : 1);
            const double ang = -PI / 2 + rnd.R(-.15, .15);
            br(xs.at(i), VH, len, ang, maxD - (i ? 1 : 0));
        }
        break;
    }
    case MsVignette::Roots: {
        const int maxD = 2 + jsRound(grow * 5);
        std::function<void(double, double, double, double, int)> rt = [&](double x, double y, double len, double ang, int d) {
            const double x2 = x + std::cos(ang) * len, y2 = y + std::sin(ang) * len;
            const double cx = (x + x2) / 2 + rnd.R(-6, 6);
            const double cy = (y + y2) / 2 + rnd.R(-6, 6);
            pen(ink, std::max(.4, d * .6));
            QPainterPath path(QPointF(x, y));
            path.quadTo(QPointF(cx, cy), QPointF(x2, y2));
            g.drawPath(path);
            if (d > 0) {
                const int n = 2 + (rnd() < .4 ? 1 : 0);
                for (int i = 0; i < n; ++i) {
                    const double l2 = len * rnd.R(.6, .8);
                    const double a2 = ang + rnd.R(-.9, .9);
                    rt(x2, y2, l2, a2, d - 1);
                }
            }
        };
        g.fillRect(QRectF(0, 0, VW, 14), faint);
        for (double x : spots(std::max(1, jsRound(wide)))) {
            const double len = rnd.R(16, 22);
            rt(x, 14, len, PI / 2, maxD);
        }
        break;
    }
    case MsVignette::Flames: {
        const int n = jsRound((3 + grow * 5) * wide);
        for (int k = 0; k < 3; ++k) for (int i = 0; i < n; ++i) {
            const double x = 14 + ((VW - 28) / std::max(1, n - 1)) * i + rnd.R(-5, 5);
            const double h = (25 + rnd.R(0, 50) * grow + 12) * (1 - k * .25);
            const double w = rnd.R(6, 12) * (1 - k * .2);
            const QColor fill = k == 0 ? faint : k == 1 ? hsl(hue + 30, .70, .60, .55) : hsl(hue + 50, .90, .80, .70);
            QPainterPath path(QPointF(x - w, VH));
            const double c1 = rnd.R(-8, 8);
            const double c2 = rnd.R(-4, 4);
            path.cubicTo(QPointF(x - w, VH - h * .5), QPointF(x + c1, VH - h * .8), QPointF(x + c2, VH - h));
            const double c3 = rnd.R(-6, 6);
            path.cubicTo(QPointF(x + c3, VH - h * .7), QPointF(x + w, VH - h * .45), QPointF(x + w, VH));
            g.setPen(Qt::NoPen);
            g.setBrush(fill);
            g.drawPath(path);
        }
        break;
    }
    case MsVignette::City: {
        { const double sx = rnd.R(20, VW - 20); const double sy = rnd.R(16, 30); const double sr = rnd.R(5, 9); dot(sx, sy, sr, ink2); }
        const int n = std::max(1, jsRound((5 + grow * 9) * wide));
        double x = -2;
        const double bw = VW / n;
        for (int i = 0; i < n + 3 && x < VW; ++i) {
            const double w = bw * rnd.R(.7, 1.3);
            const double h = 14 + rnd.R(0, 58) * grow + rnd.R(0, 12);
            const QRectF b(x, VH - h, w, h);
            g.fillRect(b, hsl(hue, .30, (14 + rnd() * 8) / 100.0));
            pen(faint, .6);
            g.drawRect(b);
            const QColor lit = hsl(hue + 40, .80, .75);
            for (double yy = VH - h + 4; yy < VH - 4; yy += 5)
                for (double xx = x + 2; xx < x + w - 2; xx += 4)
                    if (rnd() < .25) g.fillRect(QRectF(xx, yy, 1.6, 2), lit);
            x += w * .92;
        }
        break;
    }
    case MsVignette::Stars: {
        const int n = jsRound((6 + grow * 24) * std::sqrt(wide));
        struct P { double x, y, r; };
        QList<P> pts;
        for (int i = 0; i < n; ++i) {
            const double a = rnd() * 7, r = std::sqrt(rnd());
            const double pr = rnd.R(.4, 1.6);
            pts << P{ CX + std::cos(a) * r * (CX - 6), CY + std::sin(a) * r * (CY - 6), pr };
        }
        if (pts.isEmpty()) break;
        const int k = std::min(int(n), 4 + jsRound(grow * 7));
        QList<bool> used(pts.size(), false);
        used[0] = true;
        int cur = 0;
        QPainterPath path(QPointF(pts[0].x, pts[0].y));
        for (int s2 = 1; s2 < k; ++s2) {
            int best = -1; double bd = 1e9;
            for (int i = 0; i < pts.size(); ++i) {
                if (used[i]) continue;
                const double d = std::pow(pts[i].x - pts[cur].x, 2) + std::pow(pts[i].y - pts[cur].y, 2);
                if (d < bd) { bd = d; best = i; }
            }
            if (best < 0) break;
            used[best] = true;
            cur = best;
            path.lineTo(pts[best].x, pts[best].y);
        }
        pen(faint, .7);
        g.drawPath(path);
        for (int i = 0; i < pts.size(); ++i) dot(pts[i].x, pts[i].y, pts[i].r, used[i] ? ink2 : ink);
        break;
    }
    case MsVignette::Mountains: {
        { const double sx = rnd.R(20, VW - 20); const double sy = rnd.R(16, 34); const double sr = rnd.R(6, 11); dot(sx, sy, sr, ink2); }
        const int layers = 2 + jsRound(grow * 2);
        for (int l = 0; l < layers; ++l) {
            const double base = VH * .48 + l * (VH * .38 / layers);
            const double y0 = base + rnd.R(-10, 10);
            const double y1 = base + rnd.R(-10, 10);
            QList<QPointF> pts{ QPointF(0, y0), QPointF(VW, y1) };
            double amp = 22 * (0.5 + grow * .8);
            for (int d = 0; d < 5 + (wide > 1.3 ? 1 : 0); ++d, amp *= .55)
                for (int i = pts.size() - 1; i > 0; --i) {
                    const QPointF a = pts[i - 1], b = pts[i];
                    pts.insert(i, QPointF((a.x() + b.x()) / 2, (a.y() + b.y()) / 2 + rnd.R(-amp, amp)));
                }
            QPainterPath path(QPointF(0, VH));
            for (const QPointF& p : pts) path.lineTo(p);
            path.lineTo(VW, VH);
            g.setPen(Qt::NoPen);
            g.setBrush(hsl(hue, .30, (30 - l * 6) / 100.0));
            g.drawPath(path);
        }
        break;
    }
    case MsVignette::Coral:
    case MsVignette::Cracks:
    case MsVignette::Lightning: {
        // As três crescem por passeio com ramos; a espessura corrente é
        // herdada do ramo filho, como no concept.
        double curW = 1;
        QColor col = fam == MsVignette::Lightning ? ink2 : ink;
        auto strokeGlow = [&](const QPainterPath& path) {
            if (fam == MsVignette::Lightning) {
                QColor glow = ink2; glow.setAlphaF(.22);
                pen(glow, curW + 3.5);
                g.drawPath(path);
            }
            pen(col, curW);
            g.drawPath(path);
        };
        if (fam == MsVignette::Coral) {
            std::function<void(double, double, double, int, double)> walk = [&](double x, double y, double a, int life, double w) {
                curW = w;
                QPainterPath path(QPointF(x, y));
                for (int i = 0; i < life; ++i) {
                    a += rnd.R(-.35, .35);
                    x += std::cos(a) * 2.2; y += std::sin(a) * 2.2;
                    path.lineTo(x, y);
                    if (rnd() < .07 && w > .7) {
                        strokeGlow(path);
                        const double a2 = a + rnd.R(-.9, .9);
                        walk(x, y, a2, life - i, w * .75);
                        path = QPainterPath(QPointF(x, y));
                    }
                }
                strokeGlow(path);
                dot(x, y, 1.4, ink2);
            };
            const int stems = jsRound((3 + grow * 4) * wide);
            for (int i = 0; i < stems; ++i) {
                const double x = 12 + rnd() * (VW - 24);
                const double a = -PI / 2 + rnd.R(-.4, .4);
                walk(x, VH, a, 14 + jsRound(grow * 26), 2.2);
            }
        } else if (fam == MsVignette::Cracks) {
            std::function<void(double, double, double, int, double)> crack = [&](double x, double y, double a, int life, double w) {
                curW = w;
                QPainterPath path(QPointF(x, y));
                for (int i = 0; i < life; ++i) {
                    a += rnd.R(-.6, .6);
                    x += std::cos(a) * 4; y += std::sin(a) * 4;
                    path.lineTo(x, y);
                    if (rnd() < .18 && life - i > 2) {
                        strokeGlow(path);
                        const double a2 = a + rnd.R(-1.2, 1.2);
                        crack(x, y, a2, jsRound((life - i) * .6), w * .6);
                        path = QPainterPath(QPointF(x, y));
                    }
                }
                strokeGlow(path);
            };
            const int seeds = jsRound((2 + grow * 3) * wide);
            for (int i = 0; i < seeds; ++i) {
                const double x = rnd.R(14, VW - 14);
                const double y = rnd.R(14, VH - 14);
                const double a = rnd() * 7;
                crack(x, y, a, 6 + jsRound(grow * 12), 1.6);
            }
        } else {
            std::function<void(double, double, double, int, double)> bolt = [&](double x, double y, double a, int life, double w) {
                curW = w;
                QPainterPath path(QPointF(x, y));
                for (int i = 0; i < life; ++i) {
                    x += std::cos(a + rnd.R(-.8, .8)) * 7;
                    y += std::sin(a + rnd.R(-.8, .8)) * 7;
                    path.lineTo(x, y);
                    if (rnd() < .25 * grow && w > .6) {
                        strokeGlow(path);
                        const double a2 = a + rnd.R(-.9, .9);
                        bolt(x, y, a2, jsRound((life - i) * .5), w * .55);
                        path = QPainterPath(QPointF(x, y));
                    }
                }
                strokeGlow(path);
            };
            const QList<double> xs = spots(std::max(1, jsRound(wide * .8)));
            for (int i = 0; i < xs.size(); ++i) {
                const double a = PI / 2 + rnd.R(-.3, .3);
                bolt(xs.at(i), 0, a, 14, i ? 1.6 : 2.2);
            }
        }
        break;
    }
    case MsVignette::Mandala: {
        const int n = 5 + int(std::floor(rnd() * 8));
        const int layers = 2 + jsRound(grow * 4);
        const double rad = std::min(CX, CY) - 4;
        auto flower = [&](double cx, double cy, double rr, int ls) {
            g.save();
            g.translate(cx, cy);
            for (int l = ls; l >= 1; --l) {
                const double r = rr * (.2 + .8 * l / ls);
                const double w = rnd.R(.25, .6);
                const double shape = rnd();
                const QColor fill = (l % 2) ? faint : hsl(hue + 30, .60, .60, .35);
                const QColor line = (l % 2) ? ink : ink2;
                for (int i = 0; i < n; ++i) {
                    g.save();
                    g.rotate(qRadiansToDegrees(i / double(n) * PI * 2 + l * .3));
                    QPainterPath path(QPointF(0, 0));
                    if (shape < .5) {
                        path.quadTo(QPointF(r * w, r * .5), QPointF(0, r));
                        path.quadTo(QPointF(-r * w, r * .5), QPointF(0, 0));
                    } else {
                        path.lineTo(r * w * .6, r * .6);
                        path.lineTo(0, r);
                        path.lineTo(-r * w * .6, r * .6);
                        path.closeSubpath();
                    }
                    QPen p(line, .8);
                    p.setJoinStyle(Qt::RoundJoin);
                    g.setPen(p);
                    g.setBrush(fill);
                    g.drawPath(path);
                    g.restore();
                }
            }
            dot(0, 0, std::max(1.5, rr * .07), ink2);
            g.restore();
        };
        flower(CX, CY, rad, layers);
        if (wide > 1.3) {
            flower(CX * .3, CY * 1.3, rad * .55, std::max(1, layers - 2));
            flower(VW - CX * .3, CY * .7, rad * .5, std::max(1, layers - 2));
        }
        break;
    }
    case MsVignette::Waves: {
        const int n = 4 + jsRound(grow * 7);
        for (int i = 0; i < n; ++i) {
            const double y0 = 12 + i * ((VH - 24) / n);
            const double amp = rnd.R(2, 8);
            const double f = rnd.R(.05, .14);
            const double ph = rnd() * 7;
            const double w = rnd.R(.8, 1.8);
            QPainterPath path;
            for (double x = 0; x <= VW; x += 2) {
                const QPointF pt(x, y0 + std::sin(x * f + ph) * amp + std::sin(x * f * 2.3) * amp * .3);
                if (x == 0) path.moveTo(pt); else path.lineTo(pt);
            }
            pen((i % 3) ? ink : ink2, w);
            g.drawPath(path);
        }
        break;
    }
    case MsVignette::Circles: {
        const int n = 4 + jsRound(grow * 7);
        const double cx = rnd.R(VW * .38, VW * .62);
        const double cy = rnd.R(40, 64);
        const double rmax = std::max(44.0, CX * .9);
        for (int i = 0; i < n; ++i) {
            const double w = rnd.R(.8, 2.4);
            const double a = rnd() * 7;
            const double sweep = rnd.R(1.5, 5.8);
            const double r = 6 + i * (rmax / n);
            const QRectF box(cx - r, cy - r, 2 * r, 2 * r);
            QPainterPath path;
            // canvas mede o ângulo no sentido horário; o Qt, no anti-horário
            path.arcMoveTo(box, -qRadiansToDegrees(a));
            path.arcTo(box, -qRadiansToDegrees(a), -qRadiansToDegrees(sweep));
            pen((i % 2) ? ink : ink2, w);
            g.drawPath(path);
        }
        break;
    }
    case MsVignette::Rays: {
        const int n = 6 + jsRound(grow * 14);
        const double cx = rnd.R(30, VW - 30);
        const double cy = rnd.R(30, 74);
        const double reach = std::max(1.0, wide * .9);
        for (int i = 0; i < n; ++i) {
            const double a = i / double(n) * PI * 2 + rnd.R(-.1, .1);
            const double l = rnd.R(20, 60) * (.5 + grow * .6) * reach;
            const double w = rnd.R(.6, 1.8);
            QPainterPath path(QPointF(cx, cy));
            path.quadTo(QPointF(cx + std::cos(a + .4) * l * .5, cy + std::sin(a + .4) * l * .5),
                        QPointF(cx + std::cos(a) * l, cy + std::sin(a) * l));
            pen((i % 3) ? ink : ink2, w);
            g.drawPath(path);
        }
        dot(cx, cy, 2.5, ink2);
        break;
    }
    default: break;
    }
}

double hueOf(const QColor& base) {
    const double h = base.hslHueF();
    return h < 0 ? 30.0 : h * 360.0;
}

QPainterPath shapePath(QSize size, bool circle) {
    QPainterPath clip;
    const QRectF r(0, 0, size.width(), size.height());
    if (circle) {
        const double d = std::min(r.width(), r.height());
        clip.addEllipse(QRectF((r.width() - d) / 2, (r.height() - d) / 2, d, d));
    } else {
        clip.addRoundedRect(r, 5.0 * size.width() / 96.0, 5.0 * size.width() / 96.0);
    }
    return clip;
}

void paintRing(QPainter& p, QSize size, double hue) {
    const double d = std::min(size.width(), size.height());
    const double lw = std::max(0.75, 1.5 * d / 104.0);
    QPen pen(hsl(hue, .40, .55, .55), lw);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF((size.width() - d) / 2 + lw / 2, (size.height() - d) / 2 + lw / 2, d - lw, d - lw));
}

}  // namespace

namespace MsVignette {

QString familyId(int family) {
    return (family >= 0 && family < FamilyCount) ? QString::fromLatin1(kIds[family]) : QString();
}

int familyFromId(const QString& id) {
    for (int i = 0; i < FamilyCount; ++i)
        if (id == QLatin1String(kIds[i])) return i;
    return -1;
}

QString familyName(int family) {
    static const char* const names[FamilyCount] = {
        QT_TRANSLATE_NOOP("MsVignette", "Galhos"),
        QT_TRANSLATE_NOOP("MsVignette", "Raízes"),
        QT_TRANSLATE_NOOP("MsVignette", "Chamas"),
        QT_TRANSLATE_NOOP("MsVignette", "Cidade"),
        QT_TRANSLATE_NOOP("MsVignette", "Constelação"),
        QT_TRANSLATE_NOOP("MsVignette", "Montanhas"),
        QT_TRANSLATE_NOOP("MsVignette", "Coral"),
        QT_TRANSLATE_NOOP("MsVignette", "Rachaduras"),
        QT_TRANSLATE_NOOP("MsVignette", "Mandala"),
        QT_TRANSLATE_NOOP("MsVignette", "Relâmpago"),
        QT_TRANSLATE_NOOP("MsVignette", "Ondas"),
        QT_TRANSLATE_NOOP("MsVignette", "Círculos"),
        QT_TRANSLATE_NOOP("MsVignette", "Raios") };
    if (family < 0 || family >= FamilyCount) return QString();
    return QCoreApplication::translate("MsVignette", names[family]);
}

int autoFamily(const QString& chapterId) {
    return int(hashStr(chapterId + QStringLiteral("#f")) % FamilyCount);
}

QList<int> autoFamilies(const QStringList& chapterIds) {
    QList<int> out;
    for (int j = 0; j < chapterIds.size(); ++j) {
        int f = autoFamily(chapterIds.at(j));
        while (j && f == out.at(j - 1)) f = (f + 1) % FamilyCount;
        out << f;
    }
    return out;
}

QList<int> familiesForBook(const QStringList& chapterIds, const QStringList& chapterFamilies,
                           const QString& bookFamily) {
    QList<int> out = autoFamilies(chapterIds);
    const int book = familyFromId(bookFamily);
    for (int j = 0; j < out.size(); ++j) {
        const int own = j < chapterFamilies.size() ? familyFromId(chapterFamilies.at(j)) : -1;
        if (own >= 0) out[j] = own;
        else if (book >= 0) out[j] = book;
    }
    return out;
}

QPixmap render(const QString& seedId, int words, const QColor& base, int family,
               QSize size, qreal dpr, bool circle) {
    // O Índice e a Temporadas refazem a lista a cada mudança; o desenho só
    // muda com a semente, a cor, a família, o tamanho e as palavras.
    static QHash<QString, QPixmap> cache;
    const QString key = seedId + QLatin1Char('|') + QString::number(words) + QLatin1Char('|')
        + base.name() + QLatin1Char('|') + QString::number(family) + QLatin1Char('|')
        + QString::number(size.width()) + QLatin1Char('x') + QString::number(size.height())
        + QLatin1Char('|') + QString::number(dpr) + (circle ? QStringLiteral("|c") : QStringLiteral("|r"));
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) return it.value();

    QPixmap pm(size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const double hue = hueOf(base);
    p.setClipPath(shapePath(size, circle));
    const double W = size.width(), H = size.height();
    QRadialGradient bg(QPointF(W / 2, H / 2), std::max(W, H) * .7, QPointF(W * .4, H * .3));
    bg.setColorAt(0, hsl(hue, .32, .24));
    bg.setColorAt(1, hsl(hue + 20, .30, .10));
    p.fillRect(QRectF(0, 0, W, H), bg);

    const double s = std::min(W, H) / 104.0;
    p.save();
    p.scale(s, s);
    Rng rnd{ hashStr(seedId) };
    const double grow = std::clamp(std::log10(words + 1.0) / std::log10(15000.0), 0.12, 1.0);
    drawFamily(p, family < 0 ? autoFamily(seedId) : family, rnd, grow, W / s, H / s, hue);
    p.restore();
    p.setClipping(false);
    if (circle) paintRing(p, size, hue);
    p.end();

    if (cache.size() > 400) cache.clear();
    cache.insert(key, pm);
    return pm;
}

QPixmap renderImage(const QPixmap& image, const QColor& base, QSize size, qreal dpr, bool circle) {
    QPixmap pm(size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setClipPath(shapePath(size, circle));
    QPixmap sc = image.scaled(size * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    sc.setDevicePixelRatio(dpr);
    p.drawPixmap(QPointF((size.width() - sc.width() / dpr) / 2.0, (size.height() - sc.height() / dpr) / 2.0), sc);
    p.setClipping(false);
    if (circle) paintRing(p, size, hueOf(base));
    return pm;
}

}
