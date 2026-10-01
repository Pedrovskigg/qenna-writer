#include "ScreenplayBreakdown.h"

#include "AvatarUtils.h"
#include "ElementsStore.h"
#include "Exporter.h"
#include "SceneUtils.h"
#include "ScreenplayFormat.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTextBlock>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {

constexpr int kStripH = 44;
constexpr int kCastH = 36;
constexpr int kLocH = 30;
constexpr int kGap = 5;
constexpr int kLegendH = 22;

// Hora da cena → noite? Palavras de "continuação" herdam da cena anterior
// (é como a produção lê CONTÍNUO). Checadas antes: "MAIS TARDE" não é tarde.
bool classifyNight(const QString& time, bool previousNight)
{
    const QString t = time.toUpper();
    static const QStringList inherit = {
        QStringLiteral("CONTÍNUO"), QStringLiteral("CONTINUO"), QStringLiteral("CONTINUOUS"),
        QStringLiteral("MAIS TARDE"), QStringLiteral("MOMENTOS DEPOIS"), QStringLiteral("LATER"),
        QStringLiteral("MOMENTS LATER"), QStringLiteral("SAME"), QStringLiteral("MESMO"),
        QStringLiteral("MÁS TARDE"), QStringLiteral("CONTINUA"), QStringLiteral("PLUS TARD"),
        QStringLiteral("SUITE"), QStringLiteral("DOPO"), QStringLiteral("CONTINUAZIONE"),
        QStringLiteral("CONTINU"), QStringLiteral("PIÙ TARDI"), QStringLiteral("PEU APRÈS"),
        QStringLiteral("DESPUÉS"),
    };
    for (const QString& w : inherit) if (t.contains(w)) return previousNight;
    static const QStringList night = {
        QStringLiteral("NOITE"), QStringLiteral("MADRUGADA"), QStringLiteral("ANOITECER"),
        QStringLiteral("NIGHT"), QStringLiteral("DUSK"), QStringLiteral("NOCHE"),
        QStringLiteral("NUIT"), QStringLiteral("SOIR"), QStringLiteral("NOTTE"), QStringLiteral("SERA"),
    };
    for (const QString& w : night) if (t.contains(w)) return true;
    if (t.isEmpty()) return previousNight;
    return false;
}

QColor stripColor(bool exterior, bool interior, bool night)
{
    // Cores de praxe das tiras de produção: INT dia branco, EXT dia amarelo,
    // INT noite azul, EXT noite verde. INT./EXT. conta como externa.
    const bool ext = exterior;
    Q_UNUSED(interior);
    if (!night) return ext ? QColor(0xf3, 0xdd, 0x6a) : QColor(0xfb, 0xfa, 0xf6);
    return ext ? QColor(0xa7, 0xd3, 0x9b) : QColor(0xa9, 0xc3, 0xea);
}

}

namespace ScreenplayBreakdownData {

QString eighths(qreal pages)
{
    const int e = qMax(1, int(std::lround(pages * 8)));
    const int whole = e / 8, rest = e % 8;
    if (rest == 0) return QString::number(whole);
    return whole > 0 ? QStringLiteral("%1 %2/8").arg(whole).arg(rest) : QStringLiteral("%1/8").arg(rest);
}

Data compute(const QList<ChapterInput>& chapters, const QList<Element>& elements)
{
    Data d;

    // Deixa (MAIÚSCULAS) → personagem: nome completo, primeiro nome e apelidos.
    QHash<QString, int> cueToElement;
    for (int i = 0; i < elements.size(); ++i) {
        const Element& e = elements.at(i);
        if (e.type != QLatin1String("character")) continue;
        const QString full = e.name.simplified().toUpper();
        if (full.isEmpty()) continue;
        cueToElement.insert(full, i);
        const QString first = full.section(QLatin1Char(' '), 0, 0);
        if (!cueToElement.contains(first)) cueToElement.insert(first, i);
        for (const QString& a : e.aliases) {
            const QString al = a.simplified().toUpper();
            if (!al.isEmpty()) cueToElement.insert(al, i);
        }
    }

    QStringList htmls;
    for (const ChapterInput& c : chapters) htmls << c.html;
    const QVector<qreal> lengths = Exporter::screenplaySceneLengths(htmls);

    QHash<QString, int> castIndex;   // elementId ou "figurante:<deixa>" → linha
    QMap<QString, LocationRow> locs;
    bool prevNight = false;

    for (const ChapterInput& ch : chapters) {
        const QStringList segs = SceneUtils::splitHtmlIntoScenes(ch.html);
        for (int si = 0; si < segs.size(); ++si) {
            QTextDocument doc;
            doc.setHtml(segs.at(si));
            SceneRow* current = d.scenes.isEmpty() ? nullptr : &d.scenes.last();
            for (QTextBlock b = doc.begin(); b.isValid(); b = b.next()) {
                const QString text = b.text().trimmed();
                if (text.isEmpty() || ScreenplayFormat::isSceneBreak(b)) continue;
                const ScreenplayElement el = ScreenplayFormat::detect(b.blockFormat(), text);
                if (el == ScreenplayElement::Scene && ScreenplayFormat::isSceneHeading(text)) {
                    SceneRow s;
                    s.number = d.scenes.size() + 1;
                    s.heading = text.simplified().toUpper();
                    s.location = ScreenplayFormat::sceneLocation(text);
                    s.interior = ScreenplayFormat::sceneIsInterior(text);
                    s.exterior = ScreenplayFormat::sceneIsExterior(text);
                    s.night = classifyNight(ScreenplayFormat::sceneTime(text), prevNight);
                    prevNight = s.night;
                    s.pages = s.number - 1 < lengths.size() ? lengths.at(s.number - 1) : 0;
                    s.manuscriptId = ch.manuscriptId;
                    s.chapterId = ch.chapterId;
                    s.sceneIndex = si;
                    s.chapterHasScenes = segs.size() > 1;
                    d.scenes.append(s);
                    current = &d.scenes.last();
                    d.totalPages += s.pages;

                    LocationRow& lr = locs[s.location];
                    lr.location = s.location;
                    ++lr.scenes;
                    if (s.night) ++lr.night; else ++lr.day;
                    continue;
                }
                if (el != ScreenplayElement::Character) continue;
                const QString cue = ScreenplayFormat::cueName(text).toUpper();
                if (cue.isEmpty()) continue;
                const int ei = cueToElement.value(cue, -1);
                const QString key = ei >= 0 ? elements.at(ei).id : QStringLiteral("figurante:") + cue;
                int ci = castIndex.value(key, -1);
                if (ci < 0) {
                    CastRow r;
                    if (ei >= 0) {
                        r.name = elements.at(ei).name.simplified();
                        r.elementId = elements.at(ei).id;
                        r.image = elements.at(ei).image;
                    } else {
                        r.name = cue;
                    }
                    d.cast.append(r);
                    ci = int(d.cast.size()) - 1;
                    castIndex.insert(key, ci);
                }
                ++d.cast[ci].lines;
                ++d.totalLines;
                if (current) {
                    d.cast[ci].scenes.insert(current->number);
                    // Como o roteiro chama a pessoa ("CIDA" → "Cida"), não o nome da ficha.
                    QStringList words = cue.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
                    for (QString& w : words) if (!w.isEmpty()) w[0] = w.at(0).toUpper();
                    const QString shown = words.join(QLatin1Char(' '));
                    if (!current->speakerKeys.contains(key)) {
                        current->speakerKeys.append(key);
                        current->speakers.append(shown);
                    }
                }
            }
        }
    }

    // Elenco com mais falas primeiro; figurantes depois do elenco.
    std::stable_sort(d.cast.begin(), d.cast.end(), [](const CastRow& a, const CastRow& b) {
        if (a.elementId.isEmpty() != b.elementId.isEmpty()) return !a.elementId.isEmpty();
        return a.lines > b.lines;
    });
    for (auto it = locs.cbegin(); it != locs.cend(); ++it) d.locations.append(it.value());
    std::stable_sort(d.locations.begin(), d.locations.end(),
                     [](const LocationRow& a, const LocationRow& b) { return a.scenes > b.scenes; });
    return d;
}

}

using namespace ScreenplayBreakdownData;

ScreenplayBreakdown::ScreenplayBreakdown(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    auto* head = new QWidget(this);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(0, 2, 0, 6);
    hl->setSpacing(4);
    m_summary = new QLabel(head);
    m_summary->setObjectName(QStringLiteral("stBreakdownSummary"));
    hl->addWidget(m_summary, 1);
    auto makeBtn = [&](const QString& text, View v) {
        auto* b = new QToolButton(head);
        b->setObjectName(QStringLiteral("stChemSortBtn"));
        b->setText(text);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, [this, v]() {
            if (v != View::Scenes) m_focusCast = -1;
            setView(v);
        });
        hl->addWidget(b);
        return b;
    };
    m_btnCast = makeBtn(tr("Elenco"), View::Cast);
    m_btnScenes = makeBtn(tr("Cenas"), View::Scenes);
    m_btnLocations = makeBtn(tr("Locais"), View::Locations);
    lay->addWidget(head);
    lay->addStretch(1);
    setView(View::Scenes);
}

void ScreenplayBreakdown::setData(const Data& data)
{
    m_data = data;
    if (m_focusCast >= m_data.cast.size()) m_focusCast = -1;
    const QLocale loc;
    m_summary->setText(tr("%1 cenas · %2 págs · %3 falas")
                           .arg(m_data.scenes.size())
                           .arg(loc.toString(qMax<qreal>(0, std::ceil(m_data.totalPages - 1e-6)), 'f', 0))
                           .arg(m_data.totalLines));
    relayout();
}

void ScreenplayBreakdown::setView(View v)
{
    m_view = v;
    m_btnCast->setChecked(v == View::Cast);
    m_btnScenes->setChecked(v == View::Scenes);
    m_btnLocations->setChecked(v == View::Locations);
    relayout();
}

int ScreenplayBreakdown::contentHeight() const
{
    int rows = 0, h = 0;
    switch (m_view) {
    case View::Scenes:    rows = int(m_data.scenes.size()); h = kStripH; break;
    case View::Cast:      rows = int(m_data.cast.size()); h = kCastH; break;
    case View::Locations: rows = int(m_data.locations.size()); h = kLocH; break;
    }
    const int legend = m_view == View::Scenes && rows > 0 ? kLegendH + kGap : 0;
    return rows > 0 ? rows * (h + kGap) + legend : 40;
}

void ScreenplayBreakdown::relayout()
{
    QWidget* head = layout() && layout()->itemAt(0) ? layout()->itemAt(0)->widget() : nullptr;
    m_headerH = head ? head->sizeHint().height() : 30;
    setFixedHeight(m_headerH + contentHeight());
    m_rowRects.clear();
    const int rh = m_view == View::Scenes ? kStripH : m_view == View::Cast ? kCastH : kLocH;
    const int n = m_view == View::Scenes ? int(m_data.scenes.size())
                : m_view == View::Cast ? int(m_data.cast.size()) : int(m_data.locations.size());
    for (int i = 0; i < n; ++i) m_rowRects.append(QRect(0, m_headerH + i * (rh + kGap), width(), rh));
    m_hover = -1;
    update();
}

void ScreenplayBreakdown::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    relayout();
}

void ScreenplayBreakdown::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor text(Theme::textPrimary());
    const QColor bright(Theme::textBright());
    const QColor muted(Theme::textMuted());
    const QColor accent(Theme::accentDefault());
    // As cores de borda do tema vêm como "rgba(...)", que o QColor(QString) não lê.
    const QColor line = Theme::toColor(Theme::subtleBorder());
    QFont mono(QStringLiteral("Courier New"));
    mono.setPixelSize(12);
    mono.setBold(true);
    QFont small = font();
    small.setPixelSize(11);
    QFont normal = font();
    normal.setPixelSize(12);

    const int n = int(m_rowRects.size());
    if (n == 0) {
        p.setPen(muted);
        p.setFont(normal);
        p.drawText(QRect(0, m_headerH, width(), 40), Qt::AlignLeft | Qt::AlignVCenter,
                   tr("Nenhuma cena ainda. Comece um cabeçalho com INT. ou EXT."));
        return;
    }

    if (m_view == View::Scenes) {
        const QSet<int> focus = m_focusCast >= 0 ? m_data.cast.at(m_focusCast).scenes : QSet<int>();
        for (int i = 0; i < n; ++i) {
            const SceneRow& s = m_data.scenes.at(i);
            const QRect r = m_rowRects.at(i);
            p.setOpacity(m_focusCast >= 0 && !focus.contains(s.number) ? 0.3 : 1.0);
            QPainterPath path;
            path.addRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
            p.fillPath(path, stripColor(s.exterior, s.interior, s.night));
            p.setPen(QPen(i == m_hover ? QColor(0, 0, 0, 120) : QColor(0, 0, 0, 40), 1));
            p.drawPath(path);
            const QColor ink(0x1d, 0x1c, 0x1a);
            p.setPen(ink);
            p.setFont(mono);
            p.drawText(QRect(r.left() + 8, r.top(), 26, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       QString::number(s.number));
            p.setFont(small);
            const QString len = eighths(s.pages);
            const int lenW = p.fontMetrics().horizontalAdvance(len) + 10;
            p.drawText(QRect(r.right() - lenW - 4, r.top(), lenW, r.height()), Qt::AlignRight | Qt::AlignVCenter, len);
            const int tx = r.left() + 36, tw = r.width() - 36 - lenW - 10;
            p.setFont(mono);
            p.drawText(QRect(tx, r.top() + 6, tw, 16), Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(s.heading, Qt::ElideRight, tw));
            p.setFont(small);
            QColor whoInk = ink;
            whoInk.setAlpha(170);
            p.setPen(whoInk);
            const QString who = s.speakers.isEmpty() ? tr("sem falas") : s.speakers.join(QStringLiteral(", "));
            p.drawText(QRect(tx, r.top() + 23, tw, 15), Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(who, Qt::ElideRight, tw));
        }
        p.setOpacity(1.0);
        // Legenda das cores.
        int x = 0;
        const int y = m_rowRects.last().bottom() + kGap + 4;
        p.setFont(small);
        const struct { bool ext, night; QString label; } keys[] = {
            { false, false, tr("INT dia") }, { true, false, tr("EXT dia") },
            { false, true, tr("INT noite") }, { true, true, tr("EXT noite") },
        };
        for (const auto& k : keys) {
            p.setPen(QColor(0, 0, 0, 60));
            p.setBrush(stripColor(k.ext, !k.ext, k.night));
            p.drawRoundedRect(QRectF(x + 0.5, y + 1.5, 11, 11), 2, 2);
            p.setPen(muted);
            const int w = p.fontMetrics().horizontalAdvance(k.label);
            p.drawText(QRect(x + 16, y, w + 4, 14), Qt::AlignLeft | Qt::AlignVCenter, k.label);
            x += 16 + w + 14;
        }
        return;
    }

    if (m_view == View::Cast) {
        int maxLines = 1;
        for (const CastRow& c : m_data.cast) maxLines = qMax(maxLines, c.lines);
        for (int i = 0; i < n; ++i) {
            const CastRow& c = m_data.cast.at(i);
            const QRect r = m_rowRects.at(i);
            if (i == m_hover || i == m_focusCast) {
                QColor hl = accent;
                hl.setAlpha(i == m_focusCast ? 46 : 22);
                p.setPen(Qt::NoPen);
                p.setBrush(hl);
                p.drawRoundedRect(QRectF(r), 5, 5);
            }
            const int av = 24;
            const QRect ar(r.left() + 6, r.center().y() - av / 2, av, av);
            if (c.elementId.isEmpty()) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(muted, 1.2, Qt::DashLine));
                p.drawEllipse(QRectF(ar).adjusted(1, 1, -1, -1));
            } else {
                const qreal dpr = devicePixelRatioF();
                QPixmap pm = AvatarUtils::circularAvatar(c.image, c.name, c.elementId, qRound(av * dpr));
                pm.setDevicePixelRatio(dpr);
                p.drawPixmap(ar, pm);
            }
            const int meterW = 64;
            const QRect meter(r.right() - meterW - 6, r.center().y() - 3, meterW, 6);
            p.setPen(Qt::NoPen);
            p.setBrush(line);
            p.drawRoundedRect(QRectF(meter), 3, 3);
            p.setBrush(accent);
            p.drawRoundedRect(QRectF(meter.left(), meter.top(), meter.width() * c.lines / qreal(maxLines), meter.height()), 3, 3);

            QList<int> sc = c.scenes.values();
            std::sort(sc.begin(), sc.end());
            QStringList scs;
            for (int v : sc) scs << QString::number(v);
            const int tx = ar.right() + 10, tw = meter.left() - tx - 8;
            p.setPen(bright);
            p.setFont(mono);
            p.drawText(QRect(tx, r.top() + 3, tw, 16), Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(c.name.toUpper(), Qt::ElideRight, tw));
            p.setPen(muted);
            p.setFont(small);
            const QString sub = (c.lines == 1 ? tr("1 fala") : tr("%1 falas").arg(c.lines))
                + QStringLiteral(" · ") + tr("cenas %1").arg(scs.join(QStringLiteral(", ")));
            p.drawText(QRect(tx, r.top() + 19, tw, 14), Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(sub, Qt::ElideRight, tw));
        }
        return;
    }

    // Locais
    for (int i = 0; i < n; ++i) {
        const LocationRow& l = m_data.locations.at(i);
        const QRect r = m_rowRects.at(i);
        p.setPen(line);
        if (i > 0) p.drawLine(r.left(), r.top() - kGap / 2, r.right(), r.top() - kGap / 2);
        p.setFont(small);
        QStringList parts;
        parts << (l.scenes == 1 ? tr("1 cena") : tr("%1 cenas").arg(l.scenes));
        if (l.day > 0) parts << tr("%1 de dia").arg(l.day);
        if (l.night > 0) parts << tr("%1 à noite").arg(l.night);
        const QString right = parts.join(QStringLiteral(" · "));
        const int rw = p.fontMetrics().horizontalAdvance(right) + 6;
        p.setPen(muted);
        p.drawText(QRect(r.right() - rw, r.top(), rw, r.height()), Qt::AlignRight | Qt::AlignVCenter, right);
        p.setPen(text);
        p.setFont(mono);
        const int tw = r.width() - rw - 10;
        p.drawText(QRect(r.left() + 2, r.top(), tw, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(l.location, Qt::ElideRight, tw));
    }
}

void ScreenplayBreakdown::mouseMoveEvent(QMouseEvent* event)
{
    int hit = -1;
    for (int i = 0; i < m_rowRects.size(); ++i)
        if (m_rowRects.at(i).contains(event->pos())) { hit = i; break; }
    const bool clickable = m_view != View::Locations && hit >= 0;
    setCursor(clickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (hit != m_hover) { m_hover = hit; update(); }
    if (m_view == View::Scenes && hit >= 0)
        setToolTip(tr("Abrir a cena %1").arg(m_data.scenes.at(hit).number));
    else
        setToolTip(QString());
    QWidget::mouseMoveEvent(event);
}

void ScreenplayBreakdown::leaveEvent(QEvent* event)
{
    m_hover = -1;
    update();
    QWidget::leaveEvent(event);
}

void ScreenplayBreakdown::mousePressEvent(QMouseEvent* event)
{
    int hit = -1;
    for (int i = 0; i < m_rowRects.size(); ++i)
        if (m_rowRects.at(i).contains(event->pos())) { hit = i; break; }
    if (hit < 0) { QWidget::mousePressEvent(event); return; }
    if (m_view == View::Scenes) {
        const SceneRow& s = m_data.scenes.at(hit);
        emit sceneActivated(s.manuscriptId, s.chapterId, s.sceneIndex, s.chapterHasScenes);
    } else if (m_view == View::Cast) {
        // Personagem → as cenas dele acesas na tira de produção.
        m_focusCast = (m_focusCast == hit) ? -1 : hit;
        setView(View::Scenes);
    }
}
