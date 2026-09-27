#include "BackgroundOverlay.h"

#include "Theme.h"

#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QVector>
#include <QtMath>

namespace BackgroundOverlay {

namespace {

// Paradas com smoothstep: um degradê linear de alfa forma uma "borda" visível
// onde começa; a curva faz o escurecimento surgir suave sem ficar fraco no meio.
void addEasedStops(QGradient& g, const QColor& rgb, qreal opacity, bool reverse)
{
    const int steps = 8;
    for (int i = 0; i <= steps; ++i) {
        const qreal t = qreal(i) / steps;
        QColor c = rgb;
        c.setAlphaF(qBound(0.0, opacity * t * t * (3 - 2 * t), 1.0));
        g.setColorAt(reverse ? 1 - t : t, c);
    }
}

// Ladrilho de ruído gerado uma vez só, com semente fixa: o fundo é repintado a
// toda hora e um ruído novo a cada vez faria a tela "cintilar".
const QPixmap& noiseTile()
{
    static const QPixmap tile = []() {
        const int size = 256;
        QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
        quint32 seed = 0x9e3779b9u;
        auto rand = [&seed]() {
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            return qreal(seed % 1000) / 1000.0;
        };
        for (int y = 0; y < size; ++y) {
            auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
            for (int x = 0; x < size; ++x) {
                const int v = qBound(0, qRound(128 + (rand() - 0.5) * 255), 255);
                line[x] = qRgb(v, v, v);
            }
        }
        return QPixmap::fromImage(img);
    }();
    return tile;
}

// Desfoque de caixa que dá a volta nas bordas, em ponto flutuante: o ladrilho
// é repetido lado a lado, então o que sai pela direita entra pela esquerda (sem
// emenda), e sem arredondar a cada passada (senão o contraste reesticado depois
// mostra os degraus como manchas sujas).
void wrapBlur(QVector<float>& v, int w, int h, int r)
{
    QVector<float> tmp(v.size());
    const float n = 2.0f * r + 1.0f;
    for (int y = 0; y < h; ++y) {
        const float* row = v.constData() + y * w;
        float sum = 0;
        for (int k = -r; k <= r; ++k) sum += row[((k % w) + w) % w];
        for (int x = 0; x < w; ++x) {
            tmp[y * w + x] = sum / n;
            sum += row[(x + r + 1) % w] - row[((x - r) % w + w) % w];
        }
    }
    for (int x = 0; x < w; ++x) {
        float sum = 0;
        for (int k = -r; k <= r; ++k) sum += tmp[(((k % h) + h) % h) * w + x];
        for (int y = 0; y < h; ++y) {
            v[y * w + x] = sum / n;
            sum += tmp[((y + r + 1) % h) * w + x] - tmp[(((y - r) % h + h) % h) * w + x];
        }
    }
}

// O ladrilho de cada tamanho de grão, feito uma vez só. Acima de 1 o ruído é
// gerado JÁ no tamanho final (nada de ampliar ruído pequeno, que vira
// quadradão) e desfocado em três passadas — quase um gaussiano —, o que deixa
// a mancha lisa. O contraste que o desfoque tira volta esticando em torno do
// cinza médio até o desvio do grão fino.
const QPixmap& grainTile(int size)
{
    static QHash<int, QPixmap> cache;
    size = qBound(1, size, 12);
    auto it = cache.find(size);
    if (it == cache.end()) {
        if (size == 1) {
            it = cache.insert(size, noiseTile());
        } else {
            const int side = qMax(256, 64 * size);   // ~64 manchas por lado: sem repetição aparente
            // Ruído fractal: a mancha do tamanho pedido MAIS camadas cada vez
            // menores e mais fracas, até o grão fino. Uma escala só dá mancha
            // borrada, sem nada dentro; grão de verdade (e perolado, pedra,
            // feltro) tem detalhe em todas as escalas ao mesmo tempo.
            QVector<float> v(side * side, 0.0f);
            quint32 seed = 0x2545f491u + quint32(size);
            auto octave = [&](int r, float amp) {
                QVector<float> o(side * side);
                for (float& f : o) {
                    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
                    f = float(seed % 1000) / 1000.0f - 0.5f;
                }
                if (r > 0) for (int pass = 0; pass < 3; ++pass) wrapBlur(o, side, side, r);
                // Normaliza cada camada antes de pesar, senão a mais borrada some.
                double dev = 0;
                for (float f : o) dev += qAbs(f);
                const float k = amp / float(qMax(1e-6, dev / o.size()));
                for (int i = 0; i < o.size(); ++i) v[i] += o[i] * k;
            };
            float amp = 1.0f;
            for (int r = qMax(1, qRound(size * 0.6)); r >= 1; r /= 2) {
                octave(r, amp);
                amp *= 0.55f;
            }
            octave(0, amp);   // o grão fino por cima de tudo
            double dev = 0;
            for (float f : v) dev += qAbs(f);
            const float gain = float(64.0 / qMax(1e-6, dev / v.size()));   // desvio médio do grão fino ≈ 64
            QImage out(side, side, QImage::Format_ARGB32_Premultiplied);
            for (int y = 0; y < side; ++y) {
                auto* line = reinterpret_cast<QRgb*>(out.scanLine(y));
                for (int x = 0; x < side; ++x) {
                    const int g = qBound(0, qRound(128 + v[y * side + x] * gain), 255);
                    line[x] = qRgb(g, g, g);
                }
            }
            it = cache.insert(size, QPixmap::fromImage(out));
        }
    }
    return it.value();
}

} // namespace

bool active(const Theme::MiraTheme& t)
{
    return (t.bgOverlayType != Theme::OverlayNone && t.bgOverlayOpacity > 0) || t.bgGrain > 0;
}

void paint(QPainter& p, const QRectF& r, const Theme::MiraTheme& t)
{
    if (!active(t) || r.isEmpty()) return;
    p.save();
    p.setClipRect(r, Qt::IntersectClip);

    const int type = t.bgOverlayType;
    const qreal opacity = qBound(0, t.bgOverlayOpacity, 100) / 100.0;
    if (type != Theme::OverlayNone && opacity > 0) {
        QColor rgb = Theme::toColor(t.bgOverlayColor);
        if (!rgb.isValid()) rgb = Qt::black;
        rgb.setAlpha(255);
        const qreal size = qBound(5, t.bgOverlaySize, 100) / 100.0;
        const qreal band = r.height() * size;
        p.setPen(Qt::NoPen);
        if (type == Theme::OverlayBottom || type == Theme::OverlayBoth) {
            QLinearGradient g(0, r.bottom() - band, 0, r.bottom());
            addEasedStops(g, rgb, opacity, false);
            p.fillRect(QRectF(r.left(), r.bottom() - band, r.width(), band), g);
        }
        if (type == Theme::OverlayTop || type == Theme::OverlayBoth) {
            QLinearGradient g(0, r.top(), 0, r.top() + band);
            addEasedStops(g, rgb, opacity, true);
            p.fillRect(QRectF(r.left(), r.top(), r.width(), band), g);
        }
        if (type == Theme::OverlayVignette) {
            // Escala o eixo Y pro degradê radial virar uma elipse com a
            // proporção da janela; nesse espaço os cantos ficam a (w/2)·√2.
            p.save();
            p.translate(r.center());
            p.scale(1, r.height() / qMax(1.0, r.width()));
            const qreal outer = (r.width() / 2) * M_SQRT2;
            const qreal inner = outer * (1 - size);
            QRadialGradient g(QPointF(0, 0), outer);
            // QRadialGradient começa no centro: as paradas vão de inner/outer a 1.
            const int steps = 8;
            const qreal from = inner / outer;
            for (int i = 0; i <= steps; ++i) {
                const qreal k = qreal(i) / steps;
                QColor c = rgb;
                c.setAlphaF(qBound(0.0, opacity * k * k * (3 - 2 * k), 1.0));
                g.setColorAt(from + (1 - from) * k, c);
            }
            if (from > 0) { QColor c = rgb; c.setAlpha(0); g.setColorAt(0, c); }
            p.fillRect(QRectF(-r.width() / 2, -r.width() / 2, r.width(), r.width()), g);
            p.restore();
        }
    }

    if (t.bgGrain > 0) {
        p.setCompositionMode(QPainter::CompositionMode_Overlay);
        p.setOpacity(qBound(0, t.bgGrain, 100) / 100.0 * 0.6);
        p.drawTiledPixmap(r, grainTile(t.bgGrainSize));
    }
    p.restore();
}

} // namespace BackgroundOverlay
