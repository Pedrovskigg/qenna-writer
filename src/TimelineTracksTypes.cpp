#include "TimelineTracksTypes.h"

#include "Theme.h"

#include <QCoreApplication>
#include <QFontDatabase>
#include <QPainterPathStroker>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace Tracks {

namespace {

// Família disponível com fallback — as fontes do protótipo vêm embarcadas em
// assets/fonts (Source Serif 4, IBM Plex Mono); o fallback só existe pra
// instalação quebrada não cair em Times.
QString pickFamily(const QStringList& wanted)
{
    const QStringList have = QFontDatabase::families();
    for (const QString& w : wanted)
        if (have.contains(w, Qt::CaseInsensitive)) return w;
    return wanted.isEmpty() ? QString() : wanted.last();
}
} // namespace

Palette Palette::current()
{
    Palette p;
    p.page    = QColor(Theme::editorBackground());
    p.panel   = QColor(Theme::panelBackground());
    p.border  = QColor(Theme::panelBorder());
    p.ink     = QColor(Theme::textPrimary());
    p.bright  = QColor(Theme::textBright());
    p.muted   = QColor(Theme::textMuted());
    p.app     = QColor(Theme::appBackground());
    p.accent  = QColor(Theme::accentDefault());
    p.warning = QColor(Theme::accentWarning());
    p.info    = QColor(Theme::accentInfo());
    p.success = QColor(Theme::accentSuccess());
    p.danger  = QColor(Theme::accentDanger());
    if (!p.page.isValid())  p.page  = p.app;
    if (!p.panel.isValid()) p.panel = p.app;
    if (!p.border.isValid()) p.border = mix(p.ink, p.panel, 0.2);
    p.dim   = mix(p.ink, p.panel, 0.64);
    p.faint = alpha(p.ink, 0.14);
    return p;
}

BranchSpan branchSpan(const Data& d, int li)
{
    BranchSpan b;
    if (li < 0 || li >= d.lanes.size()) return b;
    const Lane& L = d.lanes[li];
    QList<int> cols;
    QSet<QString> bcast;
    for (const Event& e : d.events) {
        if (e.laneId != L.id || e.col < 0) continue;
        if (!cols.contains(e.col)) cols << e.col;
        for (const QString& c : e.castIds) bcast.insert(c);
    }
    if (cols.isEmpty()) return b;
    std::sort(cols.begin(), cols.end());
    b.first = cols.first();
    b.last  = cols.last();
    b.parent = L.parentId.isEmpty() ? -1 : d.laneIndex(L.parentId);
    if (b.parent < 0 || b.parent == li) { b.parent = -1; return b; }
    QList<int> pcols;
    for (const Event& e : d.events)
        if (e.laneId == L.parentId && e.col >= 0 && !pcols.contains(e.col)) pcols << e.col;
    std::sort(pcols.begin(), pcols.end());
    for (int c : pcols) if (c < b.first) b.from = c;
    for (int c : pcols) {
        if (c <= b.last) continue;
        for (const Event& e : d.events) {
            if (e.laneId != L.parentId || e.col != c) continue;
            for (const QString& id : e.castIds)
                if (bcast.contains(id)) { b.to = c; break; }
            if (b.to >= 0) break;
        }
        if (b.to >= 0) break;
    }
    return b;
}

QFont serifFont(qreal px, int weight, bool italic)
{
    static const QString fam = pickFamily({ QStringLiteral("Source Serif 4"),
                                            QStringLiteral("Lora"), QStringLiteral("Georgia") });
    QFont f(fam);
    f.setPixelSize(qMax(1, qRound(px)));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    return f;
}

QFont monoFont(qreal px, int weight)
{
    static const QString fam = pickFamily({ QStringLiteral("IBM Plex Mono"),
                                            QStringLiteral("Consolas"), QStringLiteral("Courier New") });
    QFont f(fam);
    f.setPixelSize(qMax(1, qRound(px)));
    f.setWeight(QFont::Weight(weight));
    return f;
}

QFont uiFont(qreal px, int weight)
{
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(qMax(1, qRound(px)));
    f.setWeight(QFont::Weight(weight));
    return f;
}

QStringList wrapLines(const QString& text, const QFont& font, qreal width, int maxLines)
{
    QStringList out;
    const QFontMetricsF fm(font);
    const QStringList words = text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString cur;
    int i = 0;
    for (; i < words.size(); ++i) {
        const QString cand = cur.isEmpty() ? words[i] : cur + QLatin1Char(' ') + words[i];
        if (fm.horizontalAdvance(cand) <= width || cur.isEmpty()) {
            cur = cand;
            continue;
        }
        out << cur;
        cur = words[i];
        if (out.size() == maxLines - 1) break;
    }
    if (out.size() == maxLines - 1 && i < words.size()) {
        // última linha: junta o resto e elide
        QStringList rest = words.mid(i);
        cur = rest.join(QLatin1Char(' '));
    }
    if (!cur.isEmpty()) out << fm.elidedText(cur, Qt::ElideRight, width);
    for (QString& l : out)
        if (fm.horizontalAdvance(l) > width) l = fm.elidedText(l, Qt::ElideRight, width);
    return out;
}

void drawHaloText(QPainter* p, const QString& line, const QFont& font,
                  qreal cx, qreal baseline, const QColor& fill, const QColor& halo,
                  Qt::Alignment align)
{
    if (line.isEmpty()) return;
    const QFontMetricsF fm(font);
    const qreal w = fm.horizontalAdvance(line);
    qreal x = cx;
    if (align & Qt::AlignHCenter) x = cx - w / 2.0;
    else if (align & Qt::AlignRight) x = cx - w;
    QPainterPath path;
    path.addText(QPointF(x, baseline), font, line);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    QPen hp(alpha(halo, 0.92), 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p->setPen(hp);
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);
    p->restore();
    p->save();
    p->setPen(fill);
    p->setFont(font);
    p->drawText(QPointF(x, baseline), line);
    p->restore();
}

void drawKnot(QPainter* p, const QPointF& c, qreal r, const QColor& col, const QColor& bg,
              bool hollow, bool sel, bool hover, bool small)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const qreal ring = small ? 1.6 : 2.0;
    const qreal gap  = small ? 2.5 : 3.0;
    p->setPen(Qt::NoPen);
    if (sel) {
        p->setBrush(alpha(col, 0.38));
        p->drawEllipse(c, r + (small ? 6.0 : 7.0), r + (small ? 6.0 : 7.0));
    }
    p->setBrush(bg);
    p->drawEllipse(c, r + gap, r + gap);         // recorte na cor do fundo
    p->setPen(QPen(col, ring));
    p->setBrush(bg);
    p->drawEllipse(c, r - ring / 2.0, r - ring / 2.0);
    if (!hollow) {
        const qreal core = r * (hover ? (small ? 0.50 : 0.50) : (small ? 0.34 : 0.38));
        p->setPen(Qt::NoPen);
        p->setBrush(col);
        p->drawEllipse(c, core, core);
    }
    p->restore();
}

QString fmtGap(qreal days)
{
    const int d = qRound(days);
    if (d <= 1) return QString();
    if (d < 60) return QCoreApplication::translate("TimelineTracks", "+%1 dias").arg(d);
    if (d < 730) {
        const int m = qRound(days / 30.0);
        return m == 1 ? QCoreApplication::translate("TimelineTracks", "+1 mês") : QCoreApplication::translate("TimelineTracks", "+%1 meses").arg(m);
    }
    const int y = qRound(days / 365.0);
    return y == 1 ? QCoreApplication::translate("TimelineTracks", "+1 ano") : QCoreApplication::translate("TimelineTracks", "+%1 anos").arg(y);
}

QString gapWord(qreal days)
{
    const QString g = fmtGap(days);
    if (!g.isEmpty()) return g.mid(1);
    return qRound(days) == 1 ? QCoreApplication::translate("TimelineTracks", "1 dia") : QCoreApplication::translate("TimelineTracks", "mesmo dia");
}

void drawAvatar(QPainter* p, const QRectF& r, const Character& c, const QColor& ink)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath clip; clip.addEllipse(r);
    if (!c.avatar.isNull()) {
        p->setClipPath(clip);
        const QImage img = c.avatar.scaled(r.size().toSize() * 2, Qt::KeepAspectRatioByExpanding,
                                           Qt::SmoothTransformation);
        const QRectF src((img.width() - r.width() * 2) / 2.0, (img.height() - r.height() * 2) / 2.0,
                         r.width() * 2, r.height() * 2);
        p->drawImage(r, img, src);
        p->setClipping(false);
        p->setPen(QPen(c.color, 1.2));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(r.adjusted(0.6, 0.6, -0.6, -0.6));
    } else {
        p->setPen(Qt::NoPen);
        p->setBrush(c.color);
        p->drawEllipse(r);
        static const QRegularExpression article(QStringLiteral(
            "^(o|a|os|as|the|el|la|los|las|il|lo|le|les|l')\\s+"), QRegularExpression::CaseInsensitiveOption);
        QString n = c.name.trimmed();
        n.remove(article);
        const QString init = n.isEmpty() ? QStringLiteral("?") : n.left(1).toUpper();
        p->setPen(ink);
        p->setFont(uiFont(r.height() * 0.5, QFont::Bold));
        p->drawText(r, Qt::AlignCenter, init);
    }
    p->restore();
}

} // namespace Tracks
