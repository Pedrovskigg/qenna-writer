#include "WritingStatsDialog.h"

#include "Theme.h"
#include "WordCounter.h"

#include <QDate>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSvgRenderer>
#include <QVBoxLayout>

namespace {

// Locale usada só pra formatação numérica — precisa acompanhar o idioma do
// app, não ficar fixa em pt-BR (ver app/language no QSettings).
QLocale statsLocale() {
    QSettings qs;
    const bool en = qs.value(QStringLiteral("app/language")).toString() == QStringLiteral("en");
    return en ? QLocale(QLocale::English, QLocale::UnitedStates)
              : QLocale(QLocale::Portuguese, QLocale::Brazil);
}

QPixmap renderIcon(const QString& path, const QColor& color, int sz = 14)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QString svg = QString::fromUtf8(f.readAll());

    static const QRegularExpression fillRx(QStringLiteral("fill=\"#[0-9a-fA-F]+\""));
    static const QRegularExpression strokeRx(QStringLiteral("stroke=\"#[0-9a-fA-F]+\""));
    svg.replace(fillRx,   QStringLiteral("fill=\"%1\"").arg(color.name()));
    svg.replace(strokeRx, QStringLiteral("stroke=\"%1\"").arg(color.name()));

    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pix(sz, sz);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    renderer.render(&p);
    return pix;
}

QString fmtMs(qint64 ms)
{
    const qint64 totalMin = ms / 60000;
    const qint64 h = totalMin / 60;
    const qint64 m = totalMin % 60;
    if (h > 0) return QStringLiteral("%1h %2min").arg(h).arg(m);
    return QStringLiteral("%1min").arg(m);
}

} // namespace

WritingStatsDialog::WritingStatsDialog(WordCounter* counter, QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_counter(counter)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setObjectName(QStringLiteral("writingStatsDialog"));
    setModal(false);
    buildUi();
}

void WritingStatsDialog::buildUi()
{
    const QJsonObject& prog = m_counter ? m_counter->settings().progress : QJsonObject{};
    QLocale loc = statsLocale();
    const QString weekdays[] = {
        tr("Segunda"), tr("Terça"),   tr("Quarta"),
        tr("Quinta"),  tr("Sexta"),   tr("Sábado"),
        tr("Domingo")
    };

    // ── Cálculo das estatísticas ──────────────────────────────────────────────

    int totalDaysMet = 0;
    int recordWords  = 0;
    QString recordDate;
    qint64 totalTimeMs = 0;
    qint64 totalWords  = 0;
    QHash<int, QPair<qint64, int>> weekdayData;   // Qt::DayOfWeek → {sumWords, count}
    QHash<QString, int> docDays;                  // docKey → dias que aparece

    for (const QString& dateKey : prog.keys()) {
        const QJsonObject day = prog.value(dateKey).toObject();
        const int w    = day.value(QStringLiteral("words")).toInt(0);
        const qint64 t = static_cast<qint64>(day.value(QStringLiteral("timeMs")).toDouble(0));
        if (w == 0 && t == 0) continue;

        totalWords  += w;
        totalTimeMs += t;
        if (m_counter && m_counter->dayMetGoal(dateKey)) ++totalDaysMet;
        if (w > recordWords) { recordWords = w; recordDate = dateKey; }

        const QDate d = QDate::fromString(dateKey, Qt::ISODate);
        if (d.isValid()) {
            auto& wd = weekdayData[d.dayOfWeek()];
            wd.first += w; wd.second += 1;
        }

        for (const auto& v : day.value(QStringLiteral("docs")).toArray())
            docDays[v.toString()]++;
    }

    // Dia da semana mais produtivo
    int bestWd = -1; double bestWdAvg = 0;
    for (auto it = weekdayData.cbegin(); it != weekdayData.cend(); ++it) {
        if (it->second == 0) continue;
        const double avg = static_cast<double>(it->first) / it->second;
        if (avg > bestWdAvg) { bestWdAvg = avg; bestWd = it.key(); }
    }

    // Documento mais trabalhado
    QString topDocKey; int topDocDays = 0;
    for (auto it = docDays.cbegin(); it != docDays.cend(); ++it)
        if (it.value() > topDocDays) { topDocDays = it.value(); topDocKey = it.key(); }
    const QString topDocName = (!topDocKey.isEmpty() && m_counter)
        ? m_counter->docDisplayName(topDocKey) : QString();

    const int streak    = m_counter ? m_counter->currentStreak()  : 0;
    const int maxStreak = m_counter ? m_counter->longestStreak()  : 0;
    int activeDays = 0, wordsPerDay = 0, minutesPerDay = 0;
    if (m_counter) m_counter->writingAverages(activeDays, wordsPerDay, minutesPerDay);

    // ── UI ───────────────────────────────────────────────────────────────────

    const QColor iconColor = QColor(Theme::textMuted());
    const QColor accentColor = QColor(Theme::accentDefault());

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("statsHeader"));
    auto* headerLay = new QHBoxLayout(header);
    headerLay->setContentsMargins(14, 10, 8, 10);
    headerLay->setSpacing(6);

    auto* titleLbl = new QLabel(tr("Estatísticas"), header);
    titleLbl->setObjectName(QStringLiteral("statsTitle"));
    headerLay->addWidget(titleLbl);
    headerLay->addStretch();

    auto* closeBtn = new QPushButton(QStringLiteral("✕"), header);
    closeBtn->setObjectName(QStringLiteral("statsClose"));
    closeBtn->setFixedSize(22, 22);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    headerLay->addWidget(closeBtn);

    outer->addWidget(header);

    // Separador
    auto* sep0 = new QFrame(this);
    sep0->setFrameShape(QFrame::HLine);
    sep0->setObjectName(QStringLiteral("statsSep"));
    outer->addWidget(sep0);

    // Corpo de conteúdo
    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("statsBody"));
    auto* bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(14, 12, 14, 14);
    bodyLay->setSpacing(8);

    // Helper: linha de stat com ícone SVG
    auto addStat = [&](const QString& iconPath, const QString& label, const QString& value,
                       bool accent = false) {
        auto* row = new QHBoxLayout();
        row->setSpacing(8);

        auto* iconLbl = new QLabel(body);
        iconLbl->setFixedSize(14, 14);
        const QPixmap pix = renderIcon(iconPath, accent ? accentColor : iconColor, 14);
        if (!pix.isNull()) iconLbl->setPixmap(pix);
        row->addWidget(iconLbl);

        auto* labelLbl = new QLabel(label, body);
        labelLbl->setObjectName(QStringLiteral("statsLabel"));
        row->addWidget(labelLbl);

        row->addStretch();

        auto* valueLbl = new QLabel(value, body);
        valueLbl->setObjectName(accent
            ? QStringLiteral("statsValueAccent")
            : QStringLiteral("statsValue"));
        row->addWidget(valueLbl);

        bodyLay->addLayout(row);
    };

    // Helper: separador de seção
    auto addSection = [&](const QString& title) {
        if (bodyLay->count() > 0) {
            auto* s = new QFrame(body);
            s->setFrameShape(QFrame::HLine);
            s->setObjectName(QStringLiteral("statsSep"));
            bodyLay->addWidget(s);
        }
        auto* lbl = new QLabel(title.toUpper(), body);
        lbl->setObjectName(QStringLiteral("statsSectionTitle"));
        bodyLay->addWidget(lbl);
    };

    // — Conquistas —
    addSection(tr("Conquistas"));
    addStat(QStringLiteral(":/icons/check.svg"),
            tr("Dias com meta batida"),
            loc.toString(totalDaysMet));
    addStat(QStringLiteral(":/icons/stats-streak.svg"),
            tr("Streak atual"),
            tr("%1 dia(s)").arg(streak),
            streak >= 3);
    addStat(QStringLiteral(":/icons/elements/crown.svg"),
            tr("Maior streak"),
            tr("%1 dia(s)").arg(maxStreak));

    // — Produção —
    addSection(tr("Produção"));
    addStat(QStringLiteral(":/icons/elements/book.svg"),
            tr("Total de palavras escritas"),
            loc.toString(static_cast<int>(totalWords)));
    addStat(QStringLiteral(":/icons/stats-clock.svg"),
            tr("Tempo total de escrita"),
            fmtMs(totalTimeMs));
    if (recordWords > 0) {
        const QDate rd = QDate::fromString(recordDate, Qt::ISODate);
        const QString rdStr = rd.isValid()
            ? rd.toString(QStringLiteral("dd/MM/yy"))
            : recordDate;
        addStat(QStringLiteral(":/icons/elements/star.svg"),
                tr("Recorde diário"),
                tr("%1 pal. (%2)").arg(loc.toString(recordWords)).arg(rdStr),
                true);
    }
    addStat(QStringLiteral(":/icons/leftbar/timeline.svg"),
            tr("Dias ativos"),
            loc.toString(activeDays));
    addStat(QStringLiteral(":/icons/stats-chart.svg"),
            tr("Média por dia ativo"),
            tr("%1 palavras").arg(loc.toString(wordsPerDay)));

    // — Hábitos —
    addSection(tr("Hábitos"));
    if (bestWd >= 1 && bestWd <= 7) {
        addStat(QStringLiteral(":/icons/leftbar/timeline.svg"),
                tr("Dia mais produtivo"),
                tr("%1 (~%2 pal.)")
                    .arg(weekdays[bestWd - 1])
                    .arg(loc.toString(static_cast<int>(bestWdAvg))));
    }
    if (!topDocName.isEmpty()) {
        addStat(QStringLiteral(":/icons/elements/chapter.svg"),
                tr("Doc. mais trabalhado"),
                tr("%1 (%2d)").arg(topDocName).arg(topDocDays));
    }

    outer->addWidget(body);

    // ── Stylesheet ───────────────────────────────────────────────────────────
    setStyleSheet(QStringLiteral(R"(
        QDialog#writingStatsDialog {
            background: %1;
            border: 1px solid %3;
            border-radius: 8px;
        }
        QWidget#statsHeader, QWidget#statsBody { background: transparent; }
        QLabel#statsTitle {
            color: %5;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton#statsClose {
            background: transparent;
            color: %4;
            border: none;
            border-radius: 4px;
            font-size: 10px;
        }
        QPushButton#statsClose:hover { background: %2; color: %5; }
        QFrame#statsSep {
            background: %3;
            border: none;
            max-height: 1px;
            margin: 2px 0;
        }
        QLabel#statsSectionTitle {
            color: %6;
            font-size: 9px;
            font-weight: 800;
            letter-spacing: 0.8px;
        }
        QLabel#statsLabel {
            color: %4;
            font-size: 11px;
        }
        QLabel#statsValue {
            color: %5;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel#statsValueAccent {
            color: %6;
            font-size: 11px;
            font-weight: 700;
        }
    )")
        .arg(Theme::panelBackground(),  // 1
             Theme::hoverOverlay(),     // 2
             Theme::panelBorder(),      // 3
             Theme::textPrimary(),      // 4
             Theme::textBright(),       // 5
             Theme::accentDefault()));  // 6

    setFixedWidth(310);
    adjustSize();
}
