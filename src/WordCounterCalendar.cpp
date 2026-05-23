#include "WordCounterCalendar.h"

#include "Theme.h"
#include "WordCounter.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDate>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString dayKey(const QDate& d) {
    return d.toString(QStringLiteral("yyyy-MM-dd"));
}
}

WordCounterCalendar::WordCounterCalendar(WordCounter* counter, QWidget* parent)
    : QWidget(parent)
    , m_counter(counter)
    , m_prevBtn(nullptr)
    , m_nextBtn(nullptr)
    , m_todayBtn(nullptr)
    , m_monthLabel(nullptr)
    , m_grid(nullptr)
    , m_currentMonth(QDate::currentDate().addDays(1 - QDate::currentDate().day()))
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("wcpCalendar"));
    buildUi();
    if (m_counter) {
        connect(m_counter, &WordCounter::progressChanged, this, &WordCounterCalendar::refresh);
        connect(m_counter, &WordCounter::settingsChanged, this, &WordCounterCalendar::refresh);
    }
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &WordCounterCalendar::applyThemeStyle);
    refresh();
}

void WordCounterCalendar::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 10, 12, 12);
    outer->setSpacing(6);

    // Header: ← Mês de Ano  [Hoje]  →
    auto* header = new QHBoxLayout();
    header->setSpacing(4);

    m_prevBtn = new QToolButton(this);
    m_prevBtn->setObjectName(QStringLiteral("wcpCalNav"));
    m_prevBtn->setText(QStringLiteral("‹"));
    m_prevBtn->setCursor(Qt::PointingHandCursor);
    connect(m_prevBtn, &QToolButton::clicked, this, [this]() { shiftMonth(-1); });

    m_monthLabel = new QLabel(QString(), this);
    m_monthLabel->setObjectName(QStringLiteral("wcpCalMonth"));
    m_monthLabel->setAlignment(Qt::AlignCenter);

    m_todayBtn = new QToolButton(this);
    m_todayBtn->setObjectName(QStringLiteral("wcpCalToday"));
    m_todayBtn->setText(tr("Hoje"));
    m_todayBtn->setCursor(Qt::PointingHandCursor);
    connect(m_todayBtn, &QToolButton::clicked, this, &WordCounterCalendar::goToToday);

    m_nextBtn = new QToolButton(this);
    m_nextBtn->setObjectName(QStringLiteral("wcpCalNav"));
    m_nextBtn->setText(QStringLiteral("›"));
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    connect(m_nextBtn, &QToolButton::clicked, this, [this]() { shiftMonth(1); });

    header->addWidget(m_prevBtn);
    header->addWidget(m_monthLabel, /*stretch=*/1);
    header->addWidget(m_todayBtn);
    header->addWidget(m_nextBtn);
    outer->addLayout(header);

    // Grid: 7 cols (D S T Q Q S S) + 6 linhas máx
    m_grid = new QGridLayout();
    m_grid->setSpacing(2);
    m_grid->setContentsMargins(0, 0, 0, 0);

    static const QStringList dow = { tr("D"), tr("S"), tr("T"), tr("Q"), tr("Q"), tr("S"), tr("S") };
    for (int c = 0; c < 7; ++c) {
        auto* lbl = new QLabel(dow[c], this);
        lbl->setObjectName(QStringLiteral("wcpCalDow"));
        lbl->setAlignment(Qt::AlignCenter);
        m_grid->addWidget(lbl, 0, c);
    }
    outer->addLayout(m_grid);

    applyThemeStyle();
}

void WordCounterCalendar::applyThemeStyle()
{
    const QString bgPanel    = Theme::panelBackground();
    const QString bgCard     = Theme::appBackground();
    const QString bgHover    = Theme::hoverOverlay();
    const QString border     = Theme::panelBorder();
    const QString borderSub  = Theme::subtleBorder();
    const QString txtPrimary = Theme::textPrimary();
    const QString txtMuted   = Theme::textMuted();
    const QString txtBright  = Theme::textBright();
    const QString accent     = Theme::accentDefault();

    setStyleSheet(QStringLiteral(R"(
        QWidget#wcpCalendar {
            background: %1;
            border: 1px solid %4;
            border-radius: 6px;
        }
        QLabel#wcpCalMonth {
            color: %8; font-size: 12px; font-weight: 600;
        }
        QLabel#wcpCalDow {
            color: %7; font-size: 10px; font-weight: 600;
        }
        QToolButton#wcpCalNav, QToolButton#wcpCalToday {
            background: %2; color: %6;
            border: 1px solid %5; border-radius: 3px;
            padding: 2px 6px; min-height: 18px;
        }
        QToolButton#wcpCalNav:hover, QToolButton#wcpCalToday:hover {
            background: %3; color: %8;
        }
        QLabel#wcpCalDay {
            background: %2;
            border: 1px solid %5;
            border-radius: 3px;
            color: %6;
            padding: 2px;
            min-height: 30px;
        }
        QLabel#wcpCalDay[isToday="true"] {
            border: 1px solid %9;
        }
        QLabel#wcpCalDay[isEmpty="true"] {
            background: transparent;
            border: 1px solid transparent;
        }
        QLabel#wcpCalDay[offType="legit"] {
            border: 1px dashed %6;
            background: %3;
        }
        QLabel#wcpCalDay[offType="stolen"] {
            border: 1px dashed #d66060;
        }
    )")
        .arg(bgPanel,    // 1
             bgCard,     // 2
             bgHover,    // 3
             border,     // 4
             borderSub,  // 5
             txtPrimary, // 6
             txtMuted,   // 7
             txtBright,  // 8
             accent)     // 9
    );
    // refresh rebui​lda os labels dos dias (que têm cor inline) com cores do tema.
    refresh();
}

void WordCounterCalendar::shiftMonth(int delta)
{
    m_currentMonth = m_currentMonth.addMonths(delta);
    m_currentMonth = QDate(m_currentMonth.year(), m_currentMonth.month(), 1);
    refresh();
}

void WordCounterCalendar::goToToday()
{
    const QDate today = QDate::currentDate();
    m_currentMonth = QDate(today.year(), today.month(), 1);
    refresh();
}

int WordCounterCalendar::starsForDay(const QString& key) const
{
    if (!m_counter) return 0;
    const auto s = m_counter->settings();
    const QJsonObject day = s.progress.value(key).toObject();
    if (day.isEmpty()) return 0;

    const QString type = day.value(QStringLiteral("goalType")).toString(s.goalType);
    const int tWords = day.value(QStringLiteral("goalTargetWords")).toInt(s.goalTargetWords);
    const int tMinutes = day.value(QStringLiteral("goalTargetMinutes")).toInt(s.goalTargetMinutes);

    if (type == QStringLiteral("time")) {
        const qint64 timeMs = static_cast<qint64>(day.value(QStringLiteral("timeMs")).toDouble(0));
        const qint64 target = static_cast<qint64>(tMinutes) * 60000;
        if (target <= 0) return 0;
        return qBound(0, static_cast<int>(timeMs / target), 5);
    }
    const int words = day.value(QStringLiteral("words")).toInt(0);
    if (tWords <= 0) return 0;
    return qBound(0, words / tWords, 5);
}

void WordCounterCalendar::refresh()
{
    if (!m_monthLabel || !m_grid) return;

    // Header label: "Maio de 2026"
    QLocale loc(QLocale::Portuguese, QLocale::Brazil);
    const QString monthName = loc.monthName(m_currentMonth.month(), QLocale::LongFormat);
    m_monthLabel->setText(tr("%1 de %2").arg(monthName).arg(m_currentMonth.year()));

    // Limpa células antigas (qualquer label de dia). Os DOW (row 0) têm objectName
    // "wcpCalDow" e ficam intactos.
    // NÃO usar setParent(nullptr) aqui — isso promove o widget a top-level
    // (janela nativa do Windows) por um event loop antes de deleteLater destruir.
    // Resultado: 31 janelinhas brancas piscam ao marcar folga com calendário aberto.
    const auto oldCells = findChildren<QLabel*>(QStringLiteral("wcpCalDay"));
    for (auto* c : oldCells) {
        c->hide();
        m_grid->removeWidget(c);
        c->deleteLater();
    }

    const QDate firstOfMonth = m_currentMonth;
    // dayOfWeek do Qt: segunda=1...domingo=7. Quero domingo como col 0.
    int firstDow = firstOfMonth.dayOfWeek() % 7; // segunda=1→1, domingo=7→0
    const int daysInMonth = firstOfMonth.daysInMonth();
    const QDate today = QDate::currentDate();

    int row = 1;
    int col = firstDow;
    // Preenche slots antes do dia 1 com células vazias.
    for (int i = 0; i < firstDow; ++i) {
        auto* empty = new QLabel(QString(), this);
        empty->setObjectName(QStringLiteral("wcpCalDay"));
        empty->setAttribute(Qt::WA_StyledBackground, true);
        empty->setProperty("isEmpty", "true");
        m_grid->addWidget(empty, row, i);
    }
    for (int d = 1; d <= daysInMonth; ++d) {
        const QDate date(m_currentMonth.year(), m_currentMonth.month(), d);
        const QString key = dayKey(date);
        const int stars = starsForDay(key);

        QString starsStr;
        for (int s = 0; s < stars; ++s) starsStr += QStringLiteral("★");

        auto* cell = new QLabel(this);
        cell->setObjectName(QStringLiteral("wcpCalDay"));
        cell->setAttribute(Qt::WA_StyledBackground, true);
        cell->setAlignment(Qt::AlignCenter);
        cell->setTextFormat(Qt::RichText);
        cell->setContextMenuPolicy(Qt::CustomContextMenu);
        cell->setProperty("dateKey", key);
        const bool isToday = (date == today);
        const auto offType = m_counter ? m_counter->offDayType(key) : WordCounter::OffDayType::None;
        cell->setProperty("isToday", isToday ? "true" : "false");
        cell->setProperty("offType",
            offType == WordCounter::OffDayType::Legit ? "legit" :
            offType == WordCounter::OffDayType::Stolen ? "stolen" : "none");

        const QString numColor = isToday ? Theme::textBright() : Theme::textPrimary();
        // Estrela dourada mantém cor própria (semântica de "ouro" não muda por tema).
        const QString starColor = QStringLiteral("#e6c45a");
        QString moon;
        if (offType == WordCounter::OffDayType::Legit)
            moon = QStringLiteral(" <span style='color:%1;'>☾</span>").arg(Theme::textBright());
        else if (offType == WordCounter::OffDayType::Stolen)
            moon = QStringLiteral(" <span style='color:#d66060;'>☾</span>");

        cell->setText(QStringLiteral(
            "<div style='color:%1; font-size:11px; font-weight:600; line-height:1;'>%2%3</div>"
            "<div style='color:%4; font-size:10px; line-height:1; margin-top:2px;'>%5</div>")
            .arg(numColor).arg(d).arg(moon).arg(starColor).arg(starsStr));

        // Context menu: marcar / folga roubada / desmarcar
        connect(cell, &QLabel::customContextMenuRequested, this, [this, cell, key](const QPoint& pos) {
            if (!m_counter) return;
            QMenu menu(cell);
            const auto cur = m_counter->offDayType(key);
            if (cur == WordCounter::OffDayType::None) {
                const int rem = m_counter->remainingFolgas();
                if (rem > 0) {
                    auto* a = menu.addAction(tr("Marcar como folga ☾"));
                    connect(a, &QAction::triggered, this, [this, key]() {
                        m_counter->setOffDay(key, WordCounter::OffDayType::Legit);
                    });
                } else {
                    auto* info = menu.addAction(tr("Sem folgas disponíveis"));
                    info->setEnabled(false);
                    auto* stolen = menu.addAction(tr("Marcar folga roubada ☾"));
                    QFont f = stolen->font(); f.setItalic(true); stolen->setFont(f);
                    connect(stolen, &QAction::triggered, this, [this, key]() {
                        m_counter->setOffDay(key, WordCounter::OffDayType::Stolen);
                    });
                }
            } else {
                auto* a = menu.addAction(tr("Desmarcar folga"));
                connect(a, &QAction::triggered, this, [this, key]() {
                    m_counter->setOffDay(key, WordCounter::OffDayType::None);
                });
            }
            menu.exec(cell->mapToGlobal(pos));
        });

        m_grid->addWidget(cell, row, col);
        ++col;
        if (col >= 7) { col = 0; ++row; }
    }
    emit geometryChanged();
}
