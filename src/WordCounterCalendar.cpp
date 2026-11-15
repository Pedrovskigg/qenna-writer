#include "WordCounterCalendar.h"

#include "CounterPanelParts.h"
#include "CrashLogger.h"
#include "Theme.h"
#include "WordCounter.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString dayKey(const QDate& d) {
    return d.toString(QStringLiteral("yyyy-MM-dd"));
}

// Locale usada só pra nomes de mês/formatação numérica — precisa acompanhar
// o idioma do app, não ficar fixa em pt-BR (ver app/language no QSettings).
QLocale statsLocale() {
    QSettings qs;
    const QString lang = qs.value(QStringLiteral("app/language")).toString();
    return lang.isEmpty() ? QLocale(QLocale::Portuguese, QLocale::Brazil) : QLocale(lang);
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
        const QDate gt = goalToday();
        m_currentMonth = QDate(gt.year(), gt.month(), 1);
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
    // Mora na aba Mês do painel do contador: sem caixa própria.
    outer->setContentsMargins(0, 0, 0, 0);
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

    // "Hoje" saiu do cabeçalho: o › para no mês de hoje.
    m_todayBtn->setVisible(false);
    header->addWidget(m_prevBtn);
    header->addWidget(m_monthLabel, /*stretch=*/1);
    header->addWidget(m_nextBtn);
    outer->addLayout(header);

    // Grid: 7 cols (D S T Q Q S S) + 6 linhas máx
    m_grid = new QGridLayout();
    m_grid->setSpacing(2);
    m_grid->setContentsMargins(0, 0, 0, 0);

    // Desambiguação explícita: "Q" (Quarta/Quinta) e "S" (Segunda/Sexta/Sábado)
    // repetem o texto-fonte em português, e o tr() precisa de um contexto
    // diferente por ocorrência pra poder traduzir cada um pra uma letra distinta
    // em inglês (S M T W T F S) em vez de duplicar a mesma tradução.
    static const QStringList dow = {
        tr("D", "calendar weekday abbrev: Sunday"),
        tr("S", "calendar weekday abbrev: Monday"),
        tr("T", "calendar weekday abbrev: Tuesday"),
        tr("Q", "calendar weekday abbrev: Wednesday"),
        tr("Q", "calendar weekday abbrev: Thursday"),
        tr("S", "calendar weekday abbrev: Friday"),
        tr("S", "calendar weekday abbrev: Saturday"),
    };
    for (int c = 0; c < 7; ++c) {
        auto* lbl = new QLabel(dow[c], this);
        lbl->setObjectName(QStringLiteral("wcpCalDow"));
        lbl->setAlignment(Qt::AlignCenter);
        m_grid->addWidget(lbl, 0, c);
    }
    outer->addLayout(m_grid);

    // Rodapé: quantos dias bateram a meta + legenda das cores.
    auto* foot = new QHBoxLayout();
    foot->setSpacing(3);
    foot->setContentsMargins(0, 2, 0, 0);
    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("wcpCalLegend"));
    foot->addWidget(m_summary);
    foot->addStretch();
    auto* less = new QLabel(tr("menos"), this);
    less->setObjectName(QStringLiteral("wcpCalLegend"));
    foot->addWidget(less);
    for (int i = 0; i < 5; ++i) {
        auto* sw = new QLabel(this);
        sw->setFixedSize(9, 9);
        m_legendSwatches.append(sw);
        foot->addWidget(sw);
    }
    auto* more = new QLabel(tr("mais"), this);
    more->setObjectName(QStringLiteral("wcpCalLegend"));
    foot->addWidget(more);
    outer->addLayout(foot);

    applyThemeStyle();
}

void WordCounterCalendar::applyThemeStyle()
{
    const QString bgHover    = Theme::hoverOverlay();
    const QString borderSub  = Theme::subtleBorder();
    const QString txtPrimary = Theme::textPrimary();
    const QString txtMuted   = Theme::textMuted();
    const QString txtBright  = Theme::textBright();

    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QWidget#wcpCalendar { background: transparent; border: none; }
        QLabel#wcpCalMonth {
            color: %5; font-size: 12px; font-weight: 600;
        }
        QLabel#wcpCalDow {
            color: %4; font-size: 9px; font-weight: 600;
        }
        QLabel#wcpCalLegend { color: %4; font-size: 10px; }
        QToolButton#wcpCalNav {
            background: transparent; color: %4; border: none;
            border-radius: @radius-control; font-size: 15px;
            min-width: 26px; max-width: 26px; min-height: 24px; max-height: 24px; padding: 0;
        }
        QToolButton#wcpCalNav:hover { background: %1; color: %5; }
        QToolButton#wcpCalNav:disabled { color: %2; background: transparent; }
        QLabel#wcpCalDay {
            border: 2px solid transparent;
            border-radius: 4px;
            padding: 0;
        }
        QLabel#wcpCalDay[isToday="true"] { border: 2px solid %5; }
        QLabel#wcpCalDay[isEmpty="true"] { background: transparent; }
        QLabel#wcpCalDay[offType="legit"] { border: 1px dashed %3; }
        QLabel#wcpCalDay[offType="stolen"] { border: 1px dashed %6; }
    )"))
        // Numeração sem buraco: o arg() de vários valores preenche os menores
        // %N que existirem, e um buraco faz as cores escorregarem.
        .arg(bgHover,    // 1
             borderSub,  // 2
             txtPrimary, // 3
             txtMuted,   // 4
             txtBright,  // 5
             Theme::accentWarning()) // 6
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
    const QDate today = goalToday();
    m_currentMonth = QDate(today.year(), today.month(), 1);
    refresh();
}

QDate WordCounterCalendar::goalToday() const
{
    if (m_counter) {
        const QDate d = QDate::fromString(m_counter->currentGoalDayKey(), QStringLiteral("yyyy-MM-dd"));
        if (d.isValid()) return d;
    }
    return QDate::currentDate();
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

double WordCounterCalendar::fractionForDay(const QString& key) const
{
    if (!m_counter) return 0;
    const auto s = m_counter->settings();
    const QJsonObject day = s.progress.value(key).toObject();
    if (day.isEmpty()) return 0;
    const QString type = day.value(QStringLiteral("goalType")).toString(s.goalType);
    if (type == QStringLiteral("time")) {
        const double timeMs = day.value(QStringLiteral("timeMs")).toDouble(0);
        const int tMin = day.value(QStringLiteral("goalTargetMinutes")).toInt(s.goalTargetMinutes);
        return tMin > 0 ? timeMs / (tMin * 60000.0) : 0;
    }
    const int words = day.value(QStringLiteral("words")).toInt(0);
    const int tWords = day.value(QStringLiteral("goalTargetWords")).toInt(s.goalTargetWords);
    return tWords > 0 ? double(words) / tWords : 0;
}

void WordCounterCalendar::refresh()
{
    if (!m_monthLabel || !m_grid) return;

    // Header label: "Maio de 2026"
    QLocale loc = statsLocale();
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
    const QDate today = goalToday();
    if (m_nextBtn)
        m_nextBtn->setEnabled(m_currentMonth < QDate(today.year(), today.month(), 1));
    auto rgba = [](const QColor& c) {
        return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
    };
    for (int i = 0; i < m_legendSwatches.size(); ++i) {
        static const int kLegend[5] = { 0, 1, 2, 3, 5 };
        m_legendSwatches[i]->setStyleSheet(QStringLiteral("background:%1; border-radius:2px;")
            .arg(rgba(counterLevelColor(kLegend[i], 0))));
    }
    int metDays = 0, pastDays = 0;

    int row = 1;
    int col = firstDow;
    // Preenche slots antes do dia 1 com células vazias.
    for (int i = 0; i < firstDow; ++i) {
        auto* empty = new QLabel(QString(), this);
        empty->setObjectName(QStringLiteral("wcpCalDay"));
        empty->setAttribute(Qt::WA_StyledBackground, true);
        empty->setProperty("isEmpty", "true");
        empty->setFixedHeight(28);
        m_grid->addWidget(empty, row, i);
    }
    for (int d = 1; d <= daysInMonth; ++d) {
        const QDate date(m_currentMonth.year(), m_currentMonth.month(), d);
        const QString key = dayKey(date);
        const int stars = starsForDay(key);

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

        // Cor pela meta (quadradinhos do Contador Lab): mais forte, mais vezes a meta.
        const bool future = date > today;
        const double frac = future ? 0 : fractionForDay(key);
        if (!future) {
            ++pastDays;
            if (stars > 0) ++metDays;
        }
        const QColor bg = future ? QColor(Qt::transparent) : counterLevelColor(stars, frac);
        const QString numColor = future ? Theme::textMuted()
                               : stars >= 3 ? QStringLiteral("#ffffff")
                               : isToday ? Theme::textBright() : Theme::textPrimary();
        cell->setStyleSheet(QStringLiteral("background:%1;").arg(rgba(bg)));
        cell->setFixedHeight(28);
        QString moon;
        if (offType == WordCounter::OffDayType::Legit)
            moon = QStringLiteral("<span style='color:%1;'>&#9790;</span>").arg(Theme::toColor(Theme::textBright()).name());
        else if (offType == WordCounter::OffDayType::Stolen)
            moon = QStringLiteral("<span style='color:%1;'>&#9790;</span>").arg(Theme::toColor(Theme::accentWarning()).name());

        cell->setText(QStringLiteral(
            "<span style='color:%1; font-size:10px; font-weight:500;'>%2</span>%3")
            .arg(Theme::toColor(numColor).name()).arg(d).arg(moon));

        // Estrelas como filhos absolutos (não participam do layout). São a
        // identidade do calendário do Qenna: ficam por cima da cor da meta.
        const QString starColor = QStringLiteral("#e6c45a");
        // Padrão dado: 1=centro, 2=TL+TR, 3=TL+TR+centro, 4=4 cantos, 5=tudo.
        // starIdx: 0=TL 1=TR 2=centro 3=BL 4=BR
        static const bool kShow[6][5] = {
            { false, false, false, false, false },  // 0
            { false, false, true,  false, false },  // 1
            { true,  true,  false, false, false },  // 2
            { true,  true,  true,  false, false },  // 3
            { true,  true,  false, true,  true  },  // 4
            { true,  true,  true,  true,  true  },  // 5
        };
        const bool* show = kShow[qBound(0, stars, 5)];
        if (stars > 0) {
            const QString starQss = QStringLiteral(
                "font-size:10px;color:%1;background:transparent;border:none;").arg(starColor);
            for (int si = 0; si < 5; ++si) {
                auto* s = new QLabel(QStringLiteral("★"), cell);
                s->setObjectName(QStringLiteral("wcpCalStar"));
                s->setProperty("starIdx", si);
                s->setAttribute(Qt::WA_TransparentForMouseEvents);
                s->setAlignment(Qt::AlignCenter);
                s->setStyleSheet(starQss);
                s->setGeometry(0, 0, 11, 11); // posição real vem do QEvent::Resize
                s->setVisible(show[si]);
            }
        }

        cell->setToolTip(tooltipForDay(key));
        cell->installEventFilter(this); // clique esquerdo → detalhes do dia

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
                        CrashLogger::log(("folga roubada: " + key).toUtf8().constData());
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
    // Sempre 6 linhas: o painel não muda de altura de um mês pro outro.
    while (row <= 6) {
        auto* empty = new QLabel(QString(), this);
        empty->setObjectName(QStringLiteral("wcpCalDay"));
        empty->setAttribute(Qt::WA_StyledBackground, true);
        empty->setProperty("isEmpty", "true");
        empty->setFixedHeight(28);
        m_grid->addWidget(empty, row, col);
        ++col;
        if (col >= 7) { col = 0; ++row; }
    }
    if (m_summary) m_summary->setText(tr("%1 de %2 dias com meta").arg(metDays).arg(pastDays));
    emit geometryChanged();
}

QString WordCounterCalendar::tooltipForDay(const QString& key) const
{
    if (!m_counter) return QString();
    const auto offType = m_counter->offDayType(key);
    if (offType == WordCounter::OffDayType::Legit)  return tr("Você folgou esse dia");
    if (offType == WordCounter::OffDayType::Stolen) return tr("A folga deste dia foi trapaceada");

    const auto s = m_counter->settings();
    const QJsonObject day = s.progress.value(key).toObject();
    if (day.isEmpty()) return tr("Nenhuma escrita neste dia");

    const QString type = day.value(QStringLiteral("goalType")).toString(s.goalType);
    int pct = 0;
    if (type == QStringLiteral("time")) {
        const qint64 timeMs = static_cast<qint64>(day.value(QStringLiteral("timeMs")).toDouble(0));
        const int tMin = day.value(QStringLiteral("goalTargetMinutes")).toInt(s.goalTargetMinutes);
        const qint64 target = static_cast<qint64>(tMin) * 60000;
        pct = target > 0 ? static_cast<int>(timeMs * 100 / target) : 0;
    } else {
        const int words = day.value(QStringLiteral("words")).toInt(0);
        const int tWords = day.value(QStringLiteral("goalTargetWords")).toInt(s.goalTargetWords);
        pct = tWords > 0 ? words * 100 / tWords : 0;
    }
    return tr("%1% da meta diária").arg(pct);
}

bool WordCounterCalendar::eventFilter(QObject* watched, QEvent* event)
{
    // Reposiciona estrelas absolutas quando a célula é redimensionada pelo grid.
    if (event->type() == QEvent::Resize) {
        auto* cell = qobject_cast<QLabel*>(watched);
        if (cell && cell->objectName() == QLatin1String("wcpCalDay")) {
            const int W = cell->width(), H = cell->height();
            const int sz = 11;
            const QPoint pts[5] = {
                {1,       1      },               // TL
                {W-sz-1,  1      },               // TR
                {(W-sz)/2,(H-sz)/2},              // centro
                {1,       H-sz-1 },               // BL
                {W-sz-1,  H-sz-1 },               // BR
            };
            for (auto* s : cell->findChildren<QLabel*>(QStringLiteral("wcpCalStar"))) {
                const int idx = s->property("starIdx").toInt();
                if (idx >= 0 && idx < 5)
                    s->setGeometry(pts[idx].x(), pts[idx].y(), sz, sz);
            }
        }
        return false;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        auto* lbl = qobject_cast<QLabel*>(watched);
        if (me->button() == Qt::LeftButton && lbl
            && lbl->objectName() == QLatin1String("wcpCalDay")) {
            const QString key = lbl->property("dateKey").toString();
            if (!key.isEmpty() && lbl->rect().contains(me->pos())) {
                showDayDetails(key);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void WordCounterCalendar::showDayDetails(const QString& key)
{
    if (!m_counter) return;
    const auto s = m_counter->settings();
    const QJsonObject day = s.progress.value(key).toObject();
    const auto offType = m_counter->offDayType(key);

    const QDate date = QDate::fromString(key, QStringLiteral("yyyy-MM-dd"));
    QLocale loc = statsLocale();
    const QString dateStr = date.isValid()
        ? tr("%1 de %2 de %3").arg(date.day())
              .arg(loc.monthName(date.month(), QLocale::LongFormat)).arg(date.year())
        : key;

    const int words = day.value(QStringLiteral("words")).toInt(0);
    const qint64 timeMs = static_cast<qint64>(day.value(QStringLiteral("timeMs")).toDouble(0));
    const int stars = starsForDay(key);

    // Tempo de sessão formatado.
    const qint64 totalSec = timeMs / 1000;
    const int hrs = static_cast<int>(totalSec / 3600);
    const int mins = static_cast<int>((totalSec % 3600) / 60);
    QString timeStr;
    if (hrs > 0)        timeStr = tr("%1 h %2 min").arg(hrs).arg(mins);
    else if (mins > 0)  timeStr = tr("%1 min").arg(mins);
    else if (timeMs > 0) timeStr = tr("menos de 1 min");
    else                timeStr = tr("—");

    // % da meta.
    const QString type = day.value(QStringLiteral("goalType")).toString(s.goalType);
    int pct = 0;
    if (type == QStringLiteral("time")) {
        const int tMin = day.value(QStringLiteral("goalTargetMinutes")).toInt(s.goalTargetMinutes);
        const qint64 target = static_cast<qint64>(tMin) * 60000;
        pct = target > 0 ? static_cast<int>(timeMs * 100 / target) : 0;
    } else {
        const int tWords = day.value(QStringLiteral("goalTargetWords")).toInt(s.goalTargetWords);
        pct = tWords > 0 ? words * 100 / tWords : 0;
    }
    QString starsStr;
    for (int i = 0; i < stars; ++i) starsStr += QStringLiteral("★");

    auto* dlg = new QDialog(this);
    dlg->setObjectName(QStringLiteral("wcpDayDialog"));
    dlg->setWindowTitle(dateStr);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModal(true);

    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(18, 16, 18, 14);
    lay->setSpacing(8);

    auto* title = new QLabel(dateStr, dlg);
    title->setObjectName(QStringLiteral("wcpDayTitle"));
    lay->addWidget(title);

    auto addRow = [&](const QString& label, const QString& value) {
        auto* row = new QLabel(QStringLiteral("<span style='opacity:0.7;'>%1:</span>  <b>%2</b>")
                                   .arg(label, value.toHtmlEscaped()), dlg);
        row->setTextFormat(Qt::RichText);
        lay->addWidget(row);
    };

    if (offType == WordCounter::OffDayType::Legit) {
        addRow(tr("Status"), tr("Folga ☾"));
    } else if (offType == WordCounter::OffDayType::Stolen) {
        addRow(tr("Status"), tr("Folga roubada ☾"));
    }
    addRow(tr("Palavras escritas"), QString::number(words));
    addRow(tr("Tempo de sessão"), timeStr);
    addRow(tr("Meta do dia"), starsStr.isEmpty()
        ? tr("%1%").arg(pct)
        : tr("%1%  %2").arg(pct).arg(starsStr));

    // Documentos editados no dia (registrado a partir de quando a feature entrou).
    const QJsonArray docs = day.value(QStringLiteral("docs")).toArray();
    if (!docs.isEmpty()) {
        auto* sep = new QFrame(dlg);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::subtleBorder()));
        lay->addWidget(sep);
        auto* docsTitle = new QLabel(tr("Documentos editados:"), dlg);
        docsTitle->setStyleSheet(QStringLiteral("opacity: 0.7;"));
        lay->addWidget(docsTitle);
        for (const QJsonValue& v : docs) {
            auto* d = new QLabel(QStringLiteral("• %1")
                .arg(m_counter->docDisplayName(v.toString()).toHtmlEscaped()), dlg);
            d->setStyleSheet(QStringLiteral("margin-left: 6px;"));
            lay->addWidget(d);
        }
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    lay->addSpacing(4);
    lay->addWidget(buttons);

    dlg->setStyleSheet(Theme::qss(QStringLiteral(R"(
        #wcpDayDialog { background: %1; }
        #wcpDayDialog QLabel { color: %2; font-size: 12px; }
        #wcpDayDialog QLabel#wcpDayTitle { color: %3; font-size: 15px; font-weight: bold; }
        #wcpDayDialog QPushButton {
            background: %4; color: %2; border: none; padding: 6px 16px; border-radius: @radius-control; font-size: 12px;
        }
        #wcpDayDialog QPushButton:hover { background: %5; color: %3; }
    )")).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::textBright(),
            Theme::hoverOverlay(), Theme::hoverStrong()));

    dlg->exec();
}
