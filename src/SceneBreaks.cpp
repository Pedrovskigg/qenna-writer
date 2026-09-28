#include "SceneBreaks.h"

#include "Theme.h"
#include "TimelineTracksTypes.h"

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <cmath>

namespace SceneBreaks {

namespace {
const QString kKey = QStringLiteral("editor/sceneBreakStyle");

// margens do bloco da quebra (acima/abaixo), por estilo
qreal marginFor(Style s) { return s == Name ? 24.0 : 16.0; }

double channel(double v)
{
    v /= 255.0;
    return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}
double luminance(const QColor& c)
{
    return 0.2126 * channel(c.red()) + 0.7152 * channel(c.green()) + 0.0722 * channel(c.blue());
}

// Fio que esmaece na ponta de fora: de x0 (transparente) até x1 (cor cheia).
void fade(QPainter& p, qreal x0, qreal x1, qreal y, const QColor& c)
{
    QLinearGradient g(QPointF(x0, y), QPointF(x1, y));
    QColor t = c; t.setAlpha(0);
    g.setColorAt(0.0, t);
    g.setColorAt(1.0, c);
    p.setPen(QPen(QBrush(g), 1.0));
    p.drawLine(QPointF(x0, y), QPointF(x1, y));
}
} // namespace

Style style()
{
    return QSettings().value(kKey, int(Knot)).toInt() == int(Name) ? Name : Knot;
}

void setStyle(Style s)
{
    if (style() == s) return;
    QSettings().setValue(kKey, int(s));
    emit notifier()->styleChanged();
}

Notifier* notifier()
{
    static Notifier n;
    return &n;
}

double contrast(const QColor& a, const QColor& b)
{
    const double la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QColor guarded(const QColor& base, const QColor& page, const QColor& text, double min)
{
    if (contrast(base, page) >= min) return base;
    for (int i = 1; i <= 20; ++i) {
        const QColor c = Tracks::mix(text, base, i / 20.0);
        if (contrast(c, page) >= min) return c;
    }
    return text;
}

void hideQtRuler(QTextEdit* ed)
{
    if (!ed) return;
    QPalette p = ed->palette();
    for (auto g : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
        p.setColor(g, QPalette::WindowText, Qt::transparent);
        p.setColor(g, QPalette::Dark, Qt::transparent);
    }
    ed->setPalette(p);
}

bool qtRulerHidden(const QTextEdit* ed)
{
    if (!ed) return true;
    const QPalette& p = ed->palette();
    for (auto g : { QPalette::Active, QPalette::Inactive, QPalette::Disabled })
        if (p.color(g, QPalette::WindowText).alpha() != 0 || p.color(g, QPalette::Dark).alpha() != 0) return false;
    return true;
}

void applySpacing(QTextDocument* doc, bool joinPrevious)
{
    if (!doc) return;
    const qreal m = marginFor(style());
    QList<QTextBlock> fix;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        const QTextBlockFormat f = b.blockFormat();
        if (!f.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) continue;
        if (!qFuzzyCompare(f.topMargin(), m) || !qFuzzyCompare(f.bottomMargin(), m)
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
        f.setTopMargin(m);
        f.setBottomMargin(m);
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

    const QColor page(Theme::editorBackground());
    const QColor text(Theme::editorTextColor());
    const QColor muted = guarded(QColor(Theme::textMuted()), page, text);
    const Style st = style();

    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setClipRect(clip);
    int k = 0;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        if (!b.blockFormat().hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) continue;
        const int idx = k++;
        const QRectF r = lay->blockBoundingRect(b).translated(-dx, -dy);
        // percorre a tela visível inteira (as áreas do tooltip precisam estar
        // todas lá mesmo quando só um pedaço é repintado); o clip corta o desenho
        const QRect vis = ed->viewport()->rect();
        if (r.bottom() < vis.top() - 40 || r.top() > vis.bottom() + 40) continue;
        const Info in = info ? info(idx) : Info();
        const qreal cx = r.center().x();
        // bloco vazio: o meio do bloco; com texto, o risco fica embaixo dele
        const qreal cy = b.length() <= 1 ? r.center().y() : r.bottom() - marginFor(st) / 2.0;
        const qreal colW = r.width();

        if (!in.scene) {
            // <hr> fora de manuscrito: só um fio discreto, centrado
            const qreal half = colW * 0.17;
            fade(p, cx - half, cx, cy, muted);
            fade(p, cx + half, cx, cy, muted);
            continue;
        }

        QString tip = QCoreApplication::translate("SceneBreaks", "Cena %1").arg(in.number);
        if (!in.title.isEmpty()) tip += QStringLiteral(" · ") + in.title;
        tip += QStringLiteral(" · ") + (in.marker.isEmpty() ? QCoreApplication::translate("SceneBreaks", "sem data") : in.marker);
        if (in.flashback) tip += QStringLiteral(" · ") + QCoreApplication::translate("SceneBreaks", "Flashback");

        if (st == Knot) {
            const bool hollow = in.marker.isEmpty();
            QColor kc = hollow ? QColor(Theme::accentWarning())
                      : in.flashback ? QColor(Theme::accentInfo()) : QColor(Theme::accentDefault());
            kc = guarded(kc, page, text);
            const qreal R = 6.5, gap = 12.0, len = std::min<qreal>(90.0, colW * 0.16);
            fade(p, cx - R - gap - len, cx - R - gap, cy, muted);
            fade(p, cx + R + gap + len, cx + R + gap, cy, muted);
            Tracks::drawKnot(&p, QPointF(cx, cy), R, kc, page, hollow, false, false, false);
            hits.append({ QRectF(cx - R - gap - len, cy - 12, 2 * (R + gap + len), 24).toAlignedRect(), tip });
            continue;
        }

        // Name: "CENA 2" entre fios; embaixo, título e tempo
        QFont fk = Tracks::uiFont(10, QFont::DemiBold);
        fk.setLetterSpacing(QFont::PercentageSpacing, 124);
        fk.setCapitalization(QFont::AllUppercase);
        const QString label = QCoreApplication::translate("SceneBreaks", "Cena %1").arg(in.number);
        const QFontMetricsF mk(fk);
        const bool second = !in.title.isEmpty() || !in.marker.isEmpty();
        const qreal ky = second ? cy - 8 : cy;
        const qreal lw = mk.horizontalAdvance(label.toUpper());
        p.setFont(fk);
        p.setPen(muted);
        p.drawText(QPointF(cx - lw / 2.0, ky + mk.capHeight() / 2.0), label);
        const qreal gap = 10.0, len = 40.0;
        fade(p, cx - lw / 2.0 - gap - len, cx - lw / 2.0 - gap, ky, muted);
        fade(p, cx + lw / 2.0 + gap + len, cx + lw / 2.0 + gap, ky, muted);
        qreal w = lw + 2 * (gap + len);
        if (second) {
            const QFont ft = Tracks::serifFont(14, QFont::Normal, true);
            const QFont fm = Tracks::monoFont(11);
            const QFontMetricsF mt(ft), mm(fm);
            const QString sep = QStringLiteral("  ·  ");
            const QString mkText = in.marker.isEmpty() ? QCoreApplication::translate("SceneBreaks", "sem data") : in.marker;
            const qreal wt = in.title.isEmpty() ? 0 : mt.horizontalAdvance(in.title);
            const qreal ws = in.title.isEmpty() ? 0 : mm.horizontalAdvance(sep);
            const qreal wm = mm.horizontalAdvance(mkText);
            const qreal total = wt + ws + wm;
            qreal x = cx - total / 2.0;
            const qreal by = cy + 13;
            if (!in.title.isEmpty()) {
                p.setFont(ft);
                p.setPen(Tracks::mix(text, muted, 0.35));
                p.drawText(QPointF(x, by), in.title);
                x += wt;
                p.setFont(fm);
                p.setPen(muted);
                p.drawText(QPointF(x, by), sep);
                x += ws;
            }
            p.setFont(fm);
            p.setPen(in.marker.isEmpty() ? guarded(QColor(Theme::accentWarning()), page, text)
                     : in.flashback ? guarded(QColor(Theme::accentInfo()), page, text) : muted);
            p.drawText(QPointF(x, by), mkText);
            w = std::max(w, total);
        }
        hits.append({ QRectF(cx - w / 2.0, cy - 18, w, 38).toAlignedRect(), tip });
    }
    p.restore();
    return hits;
}

} // namespace SceneBreaks
