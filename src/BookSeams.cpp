#include "BookSeams.h"

#include "SceneBreaks.h"
#include "Theme.h"
#include "TimelineTracksTypes.h"

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFrame>
#include <algorithm>

namespace BookSeams {

namespace {
// margens da abertura: o primeiro capítulo quase encosta no topo; os outros
// ganham o respiro de uma virada de capítulo
constexpr qreal kTopFirst = 30.0;
constexpr qreal kTop = 64.0;
constexpr qreal kBottom = 30.0;

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

bool isSeam(const QTextBlock& b)
{
    return b.isValid() && b.blockFormat().hasProperty(kSeamProperty);
}

QString seamChapter(const QTextBlock& b)
{
    return b.isValid() ? b.blockFormat().property(kSeamProperty).toString() : QString();
}

void applySpacing(QTextDocument* doc, bool joinPrevious)
{
    if (!doc) return;
    QList<QTextBlock> fix;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        if (!isSeam(b)) continue;
        const QTextBlockFormat f = b.blockFormat();
        const qreal top = b.blockNumber() == 0 ? kTopFirst : kTop;
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
        f.setTopMargin(b.blockNumber() == 0 ? kTopFirst : kTop);
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

    QFont fk = Tracks::uiFont(10, QFont::DemiBold);
    fk.setLetterSpacing(QFont::PercentageSpacing, 130);
    fk.setCapitalization(QFont::AllUppercase);
    const QFontMetricsF mk(fk);
    const QFont ft = Tracks::serifFont(21, QFont::Medium);
    const QFontMetricsF mt(ft);

    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setClipRect(clip);
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        if (!isSeam(b)) continue;
        const QRectF r = lay->blockBoundingRect(b).translated(-dx, -dy);
        if (r.bottom() < vis.top() - 80) continue;
        if (r.top() > vis.bottom() + 80) break;
        const Info in = info ? info(seamChapter(b)) : Info();
        const qreal cx = (colLeft + colRight) / 2.0;
        const qreal colW = std::max<qreal>(80.0, colRight - colLeft);
        // o retângulo do bloco é só a linha vazia (as margens ficam fora dele):
        // o título mora na linha, o rótulo no respiro logo acima
        const qreal lineTop = r.top();
        const qreal lineBottom = r.bottom();
        const qreal titleBase = (lineTop + lineBottom) / 2.0 + mt.capHeight() / 2.0;
        const bool hasTitle = !in.title.trimmed().isEmpty();
        // o rótulo fica no respiro, com folga acima do topo das letras do título
        const qreal ky = hasTitle ? titleBase - mt.capHeight() - 18 : (lineTop + lineBottom) / 2.0;

        const QColor lc = in.focus ? accent : muted;
        const QString label = in.label.isEmpty()
            ? QCoreApplication::translate("BookSeams", "Capítulo") : in.label;
        const qreal lw = mk.horizontalAdvance(label.toUpper());
        p.setFont(fk);
        p.setPen(lc);
        p.drawText(QPointF(cx - lw / 2.0, ky + mk.capHeight() / 2.0), label);
        const qreal gap = 14.0, len = std::min<qreal>(110.0, colW * 0.2);
        fade(p, cx - lw / 2.0 - gap - len, cx - lw / 2.0 - gap, ky, lc);
        fade(p, cx + lw / 2.0 + gap + len, cx + lw / 2.0 + gap, ky, lc);

        if (hasTitle) {
            const QString t = mt.elidedText(in.title.trimmed(), Qt::ElideRight, colW * 0.9);
            p.setFont(ft);
            p.setPen(Tracks::mix(text, muted, 0.12));
            p.drawText(QPointF(cx - mt.horizontalAdvance(t) / 2.0, titleBase), t);
        }
        QString tip = label;
        if (hasTitle) tip += QStringLiteral(" · ") + in.title.trimmed();
        tip += QStringLiteral("\n") + QCoreApplication::translate("BookSeams",
            "A abertura do capítulo: o texto dele começa logo abaixo.");
        const qreal w = std::max(lw + 2 * (gap + len), hasTitle ? mt.horizontalAdvance(in.title.trimmed()) : 0.0);
        hits.append({ QRectF(cx - w / 2.0, ky - 12, w, lineBottom + 4 - (ky - 12)).toAlignedRect(), tip });
    }
    p.restore();
    return hits;
}

} // namespace BookSeams
