#include "BookSeams.h"

#include "SceneBreaks.h"
#include "Theme.h"
#include "TimelineTracksTypes.h"

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFrame>
#include <algorithm>

namespace BookSeams {

namespace {
const QString kStyleKey = QStringLiteral("editor/chapterOpeningStyle");
const QString kHeadKey = QStringLiteral("editor/chapterOpeningInHeader");

// margens da abertura: o primeiro capítulo quase encosta no topo; os outros
// ganham o respiro de uma virada de capítulo. Os estilos mais altos que os
// Fios (o nó, o número grande, o ornamento) somam o que precisam em cima.
constexpr qreal kTopFirst = 30.0;
constexpr qreal kTop = 64.0;
constexpr qreal kBottom = 30.0;
qreal extraTop(Style s)
{
    static const qreal e[kStyleCount] = { 0, 16, 20, 12, 4, 24 };
    return e[qBound(0, int(s), kStyleCount - 1)];
}
qreal topFor(const QTextBlock& b) { return (b.blockNumber() == 0 ? kTopFirst : kTop) + extraTop(style()); }

QString tr(const char* s) { return QCoreApplication::translate("BookSeams", s); }

QString roman(int n)
{
    static const QPair<int, const char*> t[] = { { 1000, "M" }, { 900, "CM" }, { 500, "D" }, { 400, "CD" }, { 100, "C" },
        { 90, "XC" }, { 50, "L" }, { 40, "XL" }, { 10, "X" }, { 9, "IX" }, { 5, "V" }, { 4, "IV" }, { 1, "I" } };
    QString s;
    for (const auto& [v, r] : t) while (n >= v) { s += QLatin1String(r); n -= v; }
    return s;
}

// ---- peças de desenho; painter nulo = só medir ----
// espaçamento das versaletes em "em" (fração do tamanho da letra), como no lab
QFont caps(qreal px, int weight = QFont::DemiBold, qreal em = 0.14)
{
    QFont f = Tracks::uiFont(px, weight);
    f.setLetterSpacing(QFont::AbsoluteSpacing, em * px);
    return f;
}
QFont serif(const QString& family, qreal px, int weight, bool italic = false)
{
    QFont f(family.isEmpty() ? QStringLiteral("Lora") : family);
    f.setPixelSize(qMax(1, qRound(px)));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    return f;
}
qreal adv(const QFont& f, const QString& s) { return s.isEmpty() ? 0.0 : QFontMetricsF(f).horizontalAdvance(s); }
qreal capH(const QFont& f) { return QFontMetricsF(f).capHeight(); }
void put(QPainter* p, const QFont& f, const QColor& c, qreal x, qreal base, const QString& s)
{
    if (!p || s.isEmpty()) return;
    p->setFont(f);
    p->setPen(c);
    p->drawText(QPointF(x, base), s);
}
// Fio que esmaece na ponta de fora: de x0 (transparente) até x1 (cor cheia).
void fade(QPainter* p, qreal x0, qreal x1, qreal y, const QColor& c)
{
    if (!p) return;
    QLinearGradient g(QPointF(x0, y), QPointF(x1, y));
    QColor t = c; t.setAlpha(0);
    g.setColorAt(0.0, t);
    g.setColorAt(1.0, c);
    p->setPen(QPen(QBrush(g), 1.0));
    p->drawLine(QPointF(x0, y), QPointF(x1, y));
}
void rule(QPainter* p, qreal x0, qreal x1, qreal y, const QColor& c)
{
    if (!p) return;
    p->setPen(QPen(c, 1.0));
    p->drawLine(QPointF(x0, y), QPointF(x1, y));
}
// A bolinha da Timeline com o número dentro (o par da quebra de cena "Nó").
void knot(QPainter* p, const QPointF& c, qreal d, const QColor& col, const QString& num, qreal px)
{
    if (!p) return;
    p->setPen(QPen(col, 1.5));
    p->setBrush(Qt::NoBrush);
    p->drawEllipse(c, d / 2.0 - 0.75, d / 2.0 - 0.75);
    QFont f = Tracks::uiFont(px, QFont::Bold);
    while (adv(f, num) > d - 7 && f.pixelSize() > 6) f.setPixelSize(f.pixelSize() - 1);
    const qreal w = adv(f, num);
    put(p, f, col, c.x() - w / 2.0, c.y() + capH(f) / 2.0, num);
}
void dot(QPainter* p, const QPointF& c, qreal d, const QColor& col)
{
    if (!p) return;
    p->setPen(Qt::NoPen);
    p->setBrush(col);
    p->drawEllipse(c, d / 2.0, d / 2.0);
    p->setBrush(Qt::NoBrush);
}
// Fleurão: losango no meio e duas volutas, desenhado numa caixa 60 x 25.
void fleuron(QPainter* p, const QRectF& box, const QColor& col, bool mirror = false)
{
    if (!p) return;
    p->save();
    p->translate(box.topLeft());
    p->scale(box.width() / 60.0, box.height() / 25.0);
    if (mirror) { p->translate(60, 0); p->scale(-1, 1); }
    QPainterPath diamond;
    diamond.moveTo(30, 6); diamond.lineTo(34, 12.5); diamond.lineTo(30, 19); diamond.lineTo(26, 12.5); diamond.closeSubpath();
    p->setPen(Qt::NoPen);
    p->setBrush(col);
    p->drawPath(diamond);
    QPainterPath curls;
    curls.moveTo(23, 12.5); curls.cubicTo(18, 4, 8, 5, 9, 12); curls.cubicTo(10, 17, 16, 16, 15, 12.5);
    curls.moveTo(37, 12.5); curls.cubicTo(42, 4, 52, 5, 51, 12); curls.cubicTo(50, 17, 44, 16, 45, 12.5);
    QPen pen(col, 1.3 * 60.0 / box.width(), Qt::SolidLine, Qt::RoundCap);
    p->setBrush(Qt::NoBrush);
    p->setPen(pen);
    p->drawPath(curls);
    QColor soft = col; soft.setAlphaF(col.alphaF() * 0.6);
    pen.setColor(soft);
    p->setPen(pen);
    p->drawLine(QPointF(2, 12.5), QPointF(7, 12.5));
    p->drawLine(QPointF(53, 12.5), QPointF(58, 12.5));
    p->restore();
}

// ---- a abertura grande, no livro ----
QRectF drawSeam(QPainter* p, Style s, const Info& in, qreal colL, qreal colR, qreal lineTop, qreal lineBottom,
                const QColor& accent, const QColor& muted, const QColor& text)
{
    const qreal cx = (colL + colR) / 2.0;
    const qreal colW = std::max<qreal>(80.0, colR - colL);
    const qreal mid = (lineTop + lineBottom) / 2.0;
    const QColor lc = in.focus ? accent : muted;
    const QColor tc = Tracks::mix(text, muted, 0.12);
    const QString label = in.label.isEmpty() ? tr("Capítulo") : in.label;
    const QString kind = in.kind.isEmpty() ? label : in.kind;
    const bool titled = !in.title.trimmed().isEmpty();
    const QFont fk = caps(10);
    const qreal capK = capH(fk);

    QRectF box;
    auto grow = [&](const QRectF& r) { box = box.isNull() ? r : box.united(r); };
    auto centered = [&](const QFont& f, const QColor& c, qreal base, const QString& t) {
        const qreal w = adv(f, t);
        put(p, f, c, cx - w / 2.0, base, t);
        grow(QRectF(cx - w / 2.0, base - capH(f) - 3, w, capH(f) + 8));
        return w;
    };
    auto elide = [&](const QFont& f, const QString& t) {
        return QFontMetricsF(f).elidedText(t.trimmed(), Qt::ElideRight, colW * 0.9);
    };
    auto sideFades = [&](qreal halfW, qreal gap, qreal len, qreal y) {
        fade(p, cx - halfW - gap - len, cx - halfW - gap, y, lc);
        fade(p, cx + halfW + gap + len, cx + halfW + gap, y, lc);
        grow(QRectF(cx - halfW - gap - len, y - 1, 2 * (halfW + gap + len), 2));
    };

    switch (s) {
    case Knot: {
        const QFont ft = Tracks::serifFont(21, QFont::Medium);
        const qreal capT = capH(ft), tb = mid + capT / 2.0;
        const bool num = !in.number.isEmpty();
        const qreal d = num ? 34.0 : 12.0;
        const qreal kc = titled ? tb - capT - 26 : mid - 12;
        if (num) knot(p, QPointF(cx, kc), d, lc, in.number, 13);
        else dot(p, QPointF(cx, kc), d, lc);
        grow(QRectF(cx - d / 2, kc - d / 2, d, d));
        sideFades(d / 2.0, 10, std::min<qreal>(120.0, colW * 0.22), kc);
        if (titled) centered(ft, tc, tb, elide(ft, in.title));
        else centered(fk, muted, kc + d / 2.0 + 10 + capK, (num ? kind : label).toUpper());
        break;
    }
    case Numeral: {
        const bool num = !in.number.isEmpty();
        const QFont fn = serif(QString(), num ? 54 : 34, QFont::Normal, true);
        const QFont fc = caps(11.5, QFont::DemiBold, 0.2);
        if (titled) {
            const qreal tb = mid + capH(fc) / 2.0;
            const qreal nb = tb - capH(fc) - 12;
            centered(fn, lc, nb, num ? in.number : label);
            grow(QRectF(cx - 10, nb - capH(fn) - 4, 20, 4));
            centered(fc, text, tb, elide(fc, in.title.toUpper()));
        } else {
            const qreal nb = mid + capH(fn) / 2.0;
            centered(fn, lc, nb, num ? in.number : label);
            if (num) centered(fk, muted, nb - capH(fn) - 12, kind.toUpper());
        }
        break;
    }
    case Roman: {
        const bool num = in.numberInt > 0;
        QFont fr = serif(QString(), 30, QFont::Medium);
        fr.setLetterSpacing(QFont::PercentageSpacing, 108);
        const QString mark = num ? QStringLiteral("·  %1  ·").arg(roman(in.numberInt)) : label;
        if (titled) {
            const QFont fti = serif(QString(), 20, QFont::Normal, true);
            const qreal tb = mid + capH(fti) / 2.0;
            centered(fr, lc, tb - capH(fti) - 14, mark);
            centered(fti, tc, tb, elide(fti, in.title));
        } else {
            const qreal rb = mid + capH(fr) / 2.0;
            centered(fr, lc, rb, mark);
            if (num) centered(fk, muted, rb - capH(fr) - 12, kind.toUpper());
        }
        break;
    }
    case Margin: {
        const QFont f27 = Tracks::serifFont(27, QFont::Medium);
        const qreal tb = mid + capH(f27) / 2.0;
        const qreal x0 = colL;
        if (titled) {
            const qreal ly = tb - capH(f27) - 16;
            rule(p, x0, x0 + 28, ly, lc);
            put(p, fk, lc, x0 + 38, ly + capK / 2.0, label.toUpper());
            const QString t = QFontMetricsF(f27).elidedText(in.title.trimmed(), Qt::ElideRight, colW);
            put(p, f27, tc, x0, tb, t);
            grow(QRectF(x0, ly - capK, std::max(38 + adv(fk, label.toUpper()), adv(f27, t)), tb + 6 - (ly - capK)));
        } else {
            rule(p, x0, x0 + 28, tb - capH(f27) / 2.0, lc);
            put(p, f27, lc, x0 + 38, tb, label);
            grow(QRectF(x0, tb - capH(f27) - 3, 38 + adv(f27, label), capH(f27) + 9));
        }
        break;
    }
    case Fleuron: {
        const qreal fw = 64, fh = fw * 25.0 / 60.0;
        if (titled) {
            const QFont fti = serif(QString(), 21, QFont::Normal, true);
            const qreal tb = mid + capH(fti) / 2.0;
            const qreal ky = tb - capH(fti) - 14;
            centered(fk, muted, ky, label.toUpper());
            const qreal fy = ky - capK - 9 - fh / 2.0;
            fleuron(p, QRectF(cx - fw / 2, fy - fh / 2, fw, fh), lc);
            grow(QRectF(cx - fw / 2, fy - fh / 2, fw, fh));
            centered(fti, tc, tb, elide(fti, in.title));
        } else {
            const QFont fk11 = caps(11);
            const qreal lb = mid + capH(fk11) / 2.0;
            centered(fk11, lc, lb, label.toUpper());
            const qreal fy = lb - capH(fk11) - 9 - fh / 2.0;
            fleuron(p, QRectF(cx - fw / 2, fy - fh / 2, fw, fh), lc);
            grow(QRectF(cx - fw / 2, fy - fh / 2, fw, fh));
        }
        break;
    }
    case Threads:
    default: {
        const QFont ft = Tracks::serifFont(21, QFont::Medium);
        const qreal capT = capH(ft), tb = mid + capT / 2.0;
        const qreal len = std::min<qreal>(110.0, colW * 0.2), gap = 14;
        if (titled) {
            const qreal ky = tb - capT - 18;
            const qreal lw = centered(fk, lc, ky + capK / 2.0, label.toUpper());
            sideFades(lw / 2.0, gap, len, ky);
            centered(ft, tc, tb, elide(ft, in.title));
        } else {
            const qreal lw = centered(ft, lc, tb, label);
            sideFades(lw / 2.0, gap, len, tb - capT / 2.0);
        }
        break;
    }
    }
    return box;
}

// ---- o nó em miniatura, na faixa de cima ----
// Mede (p nulo) ou desenha a partir de x0. Devolve a largura.
qreal drawHead(QPainter* p, qreal x0, qreal cy, Style s, const Head& h, const HeadPalette& pal, const QString& title)
{
    const qreal k = pal.scale;
    const QFont fk = caps(9.5 * k, QFont::DemiBold, 0.14);
    const QFont ft = serif(pal.serifFamily, 15 * k, QFont::Medium);
    const qreal capK = capH(fk);
    const qreal ly = cy - 8 * k;            // linha do rótulo (centro), com duas linhas
    const qreal tb = cy + 11 * k;           // base do título, com duas linhas
    const qreal tb1 = cy + capH(ft) / 2.0;  // base do título sozinho
    const QString withScene = h.scene.isEmpty() ? h.label : h.label + QStringLiteral(" · ") + h.scene;
    const bool hasNum = h.manuscript && !h.number.isEmpty();
    const QColor tc = pal.text;

    switch (s) {
    case Knot: {
        qreal x = x0, markW = 0;
        if (hasNum) {
            const qreal d = 24 * k;
            knot(p, QPointF(x + d / 2, cy), d, pal.accent, h.number, 10.5 * k);
            markW = d;
        } else if (!h.hasAvatar) {
            const qreal d = 12 * k;
            dot(p, QPointF(x + d / 2, cy), d, pal.accent);
            markW = d;
        }
        if (markW > 0) x += markW + 11 * k;
        const QString small = h.sceneAsTitle ? h.kind : (!title.isEmpty() ? withScene : h.scene);
        const QString main = !title.isEmpty() ? title : h.label;
        const QFont fs = caps(9 * k, QFont::DemiBold, 0.14);
        if (small.isEmpty()) put(p, ft, tc, x, tb1, main);
        else {
            put(p, fs, pal.muted, x, ly + capH(fs) / 2.0, small.toUpper());
            put(p, ft, tc, x, tb, main);
        }
        return x - x0 + std::max(adv(fs, small.toUpper()), adv(ft, main));
    }
    case Numeral: {
        qreal x = x0;
        if (hasNum) {
            const QFont fn = serif(pal.serifFamily, 28 * k, QFont::Normal, true);
            put(p, fn, pal.accent, x, cy + capH(fn) / 2.0, h.number);
            x += adv(fn, h.number) + 12 * k;
            if (p) {
                QColor v = pal.muted; v.setAlphaF(0.45);
                p->setPen(QPen(v, 1.0));
                p->drawLine(QPointF(x, cy - 13 * k), QPointF(x, cy + 13 * k));
            }
            x += 1 + 12 * k;
        }
        QString words, small, sceneLine;
        if (h.manuscript && hasNum) {
            words = (h.sceneAsTitle || title.isEmpty()) ? h.kind : title;
            sceneLine = h.sceneAsTitle ? title : h.scene;
        } else if (h.manuscript) {
            words = title.isEmpty() ? h.label : title;
            sceneLine = h.sceneAsTitle ? QString() : h.scene;
        } else {
            words = title.isEmpty() ? h.label : title;
            if (!title.isEmpty()) small = h.label;
        }
        const QFont fc = caps(11 * k, QFont::DemiBold, 0.18);
        const QFont fs = caps(9 * k, QFont::DemiBold, 0.14);
        const QFont fi = serif(pal.serifFamily, 11.5 * k, QFont::Normal, true);
        const QString W = words.toUpper();
        if (!small.isEmpty()) {
            put(p, fs, pal.muted, x, ly + capH(fs) / 2.0, small.toUpper());
            put(p, fc, tc, x, cy + 9 * k, W);
        } else if (!sceneLine.isEmpty()) {
            put(p, fc, tc, x, ly + capH(fc) / 2.0, W);
            put(p, fi, pal.muted, x, tb, sceneLine);
        } else {
            put(p, fc, tc, x, cy + capH(fc) / 2.0, W);
        }
        return x - x0 + std::max({ adv(fc, W), adv(fs, small.toUpper()), adv(fi, sceneLine) });
    }
    case Roman: {
        qreal x = x0;
        const qreal gap = 10 * k;
        const QFont fti = serif(pal.serifFamily, 16 * k, QFont::Normal, true);
        const QFont fdot = serif(pal.serifFamily, 15 * k, QFont::Normal);
        const qreal base = cy + capH(fti) / 2.0;
        const QString sep = QStringLiteral("·");
        auto dotSep = [&]() { put(p, fdot, pal.muted, x, base, sep); x += adv(fdot, sep) + gap; };
        if (h.manuscript && h.numberInt > 0) {
            QFont fr = serif(pal.serifFamily, 17 * k, QFont::DemiBold);
            fr.setLetterSpacing(QFont::PercentageSpacing, 106);
            const QString r = roman(h.numberInt);
            put(p, fr, pal.accent, x, base, r);
            x += adv(fr, r) + gap;
            if (!title.isEmpty()) {
                dotSep();
                put(p, fti, tc, x, base, title);
                x += adv(fti, title);
            } else {
                const QFont fk15 = serif(pal.serifFamily, 15 * k, QFont::Normal, true);
                const QString w = h.kind.toLower();
                put(p, fk15, pal.muted, x, base, w);
                x += adv(fk15, w);
            }
            if (!h.scene.isEmpty()) {
                x += gap;
                dotSep();
                const QFont fsc = serif(pal.serifFamily, 12.5 * k, QFont::Normal, true);
                put(p, fsc, pal.muted, x, base, h.scene);
                x += adv(fsc, h.scene);
            }
        } else if (!title.isEmpty()) {
            put(p, fk, pal.accent, x, base - capH(fti) / 2.0 + capK / 2.0, h.label.toUpper());
            x += adv(fk, h.label.toUpper()) + gap;
            dotSep();
            put(p, fti, tc, x, base, title);
            x += adv(fti, title);
        } else {
            put(p, fti, pal.accent, x, base, h.label);
            x += adv(fti, h.label);
        }
        return x - x0;
    }
    case Margin: {
        qreal x = x0;
        const qreal gap = 10 * k;
        const qreal yMid = tb1 - capH(ft) / 2.0;
        rule(p, x, x + 18 * k, yMid, pal.accent);
        x += 18 * k + gap;
        if (!title.isEmpty()) {
            put(p, fk, pal.accent, x, yMid + capK / 2.0, h.label.toUpper());
            x += adv(fk, h.label.toUpper()) + gap;
            put(p, ft, tc, x, tb1, title);
            x += adv(ft, title);
        } else {
            put(p, ft, pal.accent, x, tb1, h.label);
            x += adv(ft, h.label);
        }
        if (!h.scene.isEmpty()) {
            x += gap;
            const QFont fsc = serif(pal.serifFamily, 12 * k, QFont::Normal, true);
            put(p, fsc, pal.muted, x, tb1, h.scene);
            x += adv(fsc, h.scene);
        }
        return x - x0;
    }
    case Fleuron: {
        const qreal fw = 30 * k, fh = fw * 25.0 / 60.0, gap = 10 * k;
        QString top, bottom;
        QFont ftop, fbot;
        QColor ctop, cbot;
        if (!title.isEmpty()) {
            top = withScene.toUpper(); ftop = caps(9 * k, QFont::DemiBold, 0.14); ctop = pal.muted;
            bottom = title; fbot = serif(pal.serifFamily, 15.5 * k, QFont::Normal, true); cbot = tc;
        } else {
            top = h.label.toUpper(); ftop = caps(10.5 * k, QFont::DemiBold, 0.14); ctop = pal.accent;
            bottom = h.scene; fbot = serif(pal.serifFamily, 11.5 * k, QFont::Normal, true); cbot = pal.muted;
        }
        const qreal colW = std::max(adv(ftop, top), adv(fbot, bottom));
        const qreal cxCol = x0 + fw + gap + colW / 2.0;
        fleuron(p, QRectF(x0, cy - fh / 2, fw, fh), pal.accent);
        if (bottom.isEmpty()) {
            put(p, ftop, ctop, cxCol - adv(ftop, top) / 2.0, cy + capH(ftop) / 2.0, top);
        } else {
            put(p, ftop, ctop, cxCol - adv(ftop, top) / 2.0, ly + capH(ftop) / 2.0, top);
            put(p, fbot, cbot, cxCol - adv(fbot, bottom) / 2.0, tb, bottom);
        }
        fleuron(p, QRectF(x0 + fw + gap + colW + gap, cy - fh / 2, fw, fh), pal.accent, true);
        return 2 * (fw + gap) + colW;
    }
    case Threads:
    default: {
        const qreal gap = 9 * k;
        if (title.isEmpty()) {
            const qreal len = 72 * k;
            const qreal lw = adv(ft, h.label);
            fade(p, x0, x0 + len, tb1 - capH(ft) / 2.0, pal.accent);
            put(p, ft, pal.accent, x0 + len + gap, tb1, h.label);
            fade(p, x0 + len + gap + lw + gap + len, x0 + len + gap + lw + gap, tb1 - capH(ft) / 2.0, pal.accent);
            return 2 * (len + gap) + lw;
        }
        const qreal len = 58 * k;
        const QString lab = withScene.toUpper();
        const qreal lw = adv(fk, lab);
        const qreal rowW = 2 * (len + gap) + lw;
        const qreal tw = adv(ft, title);
        const qreal W = std::max(rowW, tw);
        const qreal c = x0 + W / 2.0;
        fade(p, c - lw / 2 - gap - len, c - lw / 2 - gap, ly, pal.accent);
        put(p, fk, pal.accent, c - lw / 2, ly + capK / 2.0, lab);
        fade(p, c + lw / 2 + gap + len, c + lw / 2 + gap, ly, pal.accent);
        put(p, ft, tc, c - tw / 2, tb, title);
        return W;
    }
    }
}
} // namespace

bool isSeam(const QTextBlock& b)
{
    return b.isValid() && b.blockFormat().hasProperty(kSeamProperty);
}

QString seamChapter(const QTextBlock& b)
{
    return b.isValid() ? b.blockFormat().property(kSeamProperty).toString() : QString();
}

Style style()
{
    const int v = QSettings().value(kStyleKey, int(Threads)).toInt();
    return (v >= 0 && v < kStyleCount) ? Style(v) : Threads;
}

void setStyle(Style s)
{
    if (style() == s) return;
    QSettings().setValue(kStyleKey, int(s));
    emit notifier()->changed();
}

bool nodeInHeader() { return QSettings().value(kHeadKey, true).toBool(); }

void setNodeInHeader(bool on)
{
    if (nodeInHeader() == on) return;
    QSettings().setValue(kHeadKey, on);
    emit notifier()->changed();
}

Notifier* notifier()
{
    static Notifier n;
    return &n;
}

QString styleName(Style s)
{
    switch (s) {
    case Knot:    return tr("Nó");
    case Numeral: return tr("Numeral");
    case Roman:   return tr("Romano");
    case Margin:  return tr("Margem");
    case Fleuron: return tr("Fleurão");
    case Threads:
    default:      return tr("Fios");
    }
}

QString styleHint(Style s)
{
    switch (s) {
    case Knot:    return tr("A bolinha da Timeline com o número dentro, par da quebra de cena “Nó”.");
    case Numeral: return tr("O número grande em itálico na cor de destaque; o título em versalete.");
    case Roman:   return tr("Algarismo romano com pontos e o título em itálico, com cara de livro antigo.");
    case Margin:  return tr("Alinhado à esquerda, como abertura de revista: fio curto, rótulo e título grande.");
    case Fleuron: return tr("Ornamento de tipografia clássica em cima, rótulo e título em itálico.");
    case Threads:
    default:      return tr("Rótulo em versalete entre dois fios, título embaixo.");
    }
}

QString ownTitle(const QString& title, const QString& label)
{
    QString t = title.trimmed();
    const QString l = label.trimmed();
    if (l.isEmpty()) return t;
    if (t.compare(l, Qt::CaseInsensitive) == 0) return QString();
    // "Capítulo 10 - Fim do ato", "Capítulo 10: Fim do ato" → "Fim do ato"
    if (t.startsWith(l, Qt::CaseInsensitive)) {
        static const QRegularExpression sep(QStringLiteral("^\\s*[-–—:.|]\\s*"));
        const QString rest = t.mid(l.size());
        const QRegularExpressionMatch m = sep.match(rest);
        if (m.hasMatch()) {
            const QString r = rest.mid(m.capturedLength()).trimmed();
            if (!r.isEmpty()) return r;
        }
    }
    return t;
}

void applySpacing(QTextDocument* doc, bool joinPrevious)
{
    if (!doc) return;
    QList<QTextBlock> fix;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        if (!isSeam(b)) continue;
        const QTextBlockFormat f = b.blockFormat();
        const qreal top = topFor(b);
        if (!qFuzzyCompare(f.topMargin() + 1, top + 1) || !qFuzzyCompare(f.bottomMargin() + 1, kBottom + 1)
            || !qFuzzyIsNull(f.textIndent()) || f.alignment() != Qt::AlignHCenter)
            fix << b;
    }
    if (fix.isEmpty()) return;
    const bool wasModified = doc->isModified();
    QTextCursor c(doc);
    if (joinPrevious) c.joinPreviousEditBlock(); else c.beginEditBlock();
    for (const QTextBlock& b : std::as_const(fix)) {
        QTextCursor bc(b);
        QTextBlockFormat f = b.blockFormat();
        f.setTopMargin(topFor(b));
        f.setBottomMargin(kBottom);
        f.setTextIndent(0);
        f.setAlignment(Qt::AlignHCenter);
        bc.setBlockFormat(f);
    }
    c.endEditBlock();
    doc->setModified(wasModified);
}

QList<QPair<QRect, QString>> paint(QTextEdit* ed, QPainter& p, const QRect& clip, const InfoFn& info)
{
    QList<QPair<QRect, QString>> hits;
    if (!ed || !ed->document()) return hits;
    QTextDocument* doc = ed->document();
    QAbstractTextDocumentLayout* lay = doc->documentLayout();
    const int dx = ed->horizontalScrollBar() ? ed->horizontalScrollBar()->value() : 0;
    const int dy = ed->verticalScrollBar() ? ed->verticalScrollBar()->value() : 0;
    const QRect vis = ed->viewport()->rect();
    // A coluna do texto vem do quadro do documento: o retângulo de um bloco
    // vazio tem a largura da linha vazia (zero), e o centro dele cai na margem.
    const QRectF frame = lay->frameBoundingRect(doc->rootFrame());
    const QTextFrameFormat ff = doc->rootFrame()->frameFormat();
    const qreal colLeft = frame.left() + ff.leftMargin() + ff.border() + ff.padding() - dx;
    const qreal colRight = frame.right() - ff.rightMargin() - ff.border() - ff.padding() - dx;

    const QColor page(Theme::editorBackground());
    const QColor text(Theme::editorTextColor());
    const QColor muted = SceneBreaks::guarded(QColor(Theme::textMuted()), page, text);
    const QColor accent = SceneBreaks::guarded(QColor(Theme::accentDefault()), page, text);
    const Style st = style();

    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setClipRect(clip);
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        if (!isSeam(b)) continue;
        const QRectF r = lay->blockBoundingRect(b).translated(-dx, -dy);
        if (r.bottom() < vis.top() - 120) continue;
        if (r.top() > vis.bottom() + 120) break;
        const Info in = info ? info(seamChapter(b)) : Info();
        // o retângulo do bloco é só a linha vazia (as margens ficam fora dele):
        // a linha principal do nó mora nela, o resto sobe pro respiro de cima
        const QRectF box = drawSeam(&p, st, in, colLeft, colRight, r.top(), r.bottom(), accent, muted, text);
        QString tip = in.label.isEmpty() ? tr("Capítulo") : in.label;
        if (!in.title.trimmed().isEmpty()) tip += QStringLiteral(" · ") + in.title.trimmed();
        tip += QStringLiteral("\n") + tr("A abertura do capítulo: o texto dele começa logo abaixo.");
        tip += QStringLiteral("\n") + tr("Botão direito: estilo da abertura.");
        hits.append({ box.adjusted(-8, -6, 8, 6).toAlignedRect(), tip });
    }
    p.restore();
    return hits;
}

qreal headWidth(Style s, const Head& h, const HeadPalette& pal, qreal height)
{
    return drawHead(nullptr, 0, height / 2.0, s, h, pal, h.title);
}

QRectF paintHead(QPainter& p, const QRectF& r, Style s, const Head& h, const HeadPalette& pal)
{
    QString title = h.title;
    qreal w = drawHead(nullptr, 0, r.center().y(), s, h, pal, title);
    if (w > r.width() && !title.isEmpty()) {
        // a elipse sai do título; o resto do nó fica inteiro
        const QFont ft = serif(pal.serifFamily, 15 * pal.scale, QFont::Medium);
        const qreal tw = adv(ft, title);
        title = QFontMetricsF(ft).elidedText(title, Qt::ElideRight, std::max<qreal>(20.0, tw - (w - r.width()) - 6));
        w = drawHead(nullptr, 0, r.center().y(), s, h, pal, title);
    }
    const qreal x0 = s == Margin ? r.left() : r.center().x() - w / 2.0;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    drawHead(&p, x0, r.center().y(), s, h, pal, title);
    p.restore();
    return QRectF(x0, r.top(), w, r.height());
}

} // namespace BookSeams
