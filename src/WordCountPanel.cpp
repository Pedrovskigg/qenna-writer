#include "WordCountPanel.h"

#include "EditorHost.h"
#include "ProjectModel.h"
#include "Theme.h"
#include "WordCounter.h"
#include "WordCounterCalendar.h"

#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString fmtDuration(qint64 ms) {
    if (ms <= 0) return QStringLiteral("0min");
    const qint64 totalMin = ms / 60000;
    const qint64 h = totalMin / 60;
    const qint64 m = totalMin % 60;
    if (h > 0) return QStringLiteral("%1h %2min").arg(h).arg(m);
    return QStringLiteral("%1min").arg(m);
}
}

WordCountPanel::WordCountPanel(WordCounter* counter, EditorHost* host, ProjectModel* model, QWidget* parent)
    : QWidget(parent)
    , m_counter(counter)
    , m_host(host)
    , m_model(model)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("wordCountPanel"));
    buildUi();
    updateToggleArrow();
    if (m_counter) {
        connect(m_counter, &WordCounter::countsChanged, this, &WordCountPanel::refresh);
        connect(m_counter, &WordCounter::progressChanged, this, &WordCountPanel::refresh);
        connect(m_counter, &WordCounter::settingsChanged, this, &WordCountPanel::refreshFromSettings);
    }
    if (m_host) {
        connect(m_host, &EditorHost::viewModeChanged, this, &WordCountPanel::refresh);
    }
    // Captura global de teclado pra detectar Ctrl + "lazyass".
    qApp->installEventFilter(this);
    refreshFromSettings();
    refresh();
}

void WordCountPanel::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->setAlignment(Qt::AlignLeft);

    m_toggleButton = new QToolButton(this);
    m_toggleButton->setObjectName(QStringLiteral("wcpToggle"));
    m_toggleButton->setCursor(Qt::PointingHandCursor);
    m_toggleButton->setAutoRaise(true);
    m_toggleButton->setFixedSize(22, 18);
    m_toggleButton->setToolTip(tr("Mostrar/ocultar contagem"));
    connect(m_toggleButton, &QToolButton::clicked, this, [this]() { setExpanded(!m_expanded); });

    auto* toggleRow = new QHBoxLayout();
    toggleRow->setContentsMargins(4, 0, 0, 0);
    toggleRow->setSpacing(0);
    toggleRow->addWidget(m_toggleButton);
    toggleRow->addStretch();
    outer->addLayout(toggleRow);

    // Cards (compact body) — clicáveis pra abrir/fechar full mode.
    m_body = new QFrame(this);
    m_body->setObjectName(QStringLiteral("wcpBody"));
    m_body->setAttribute(Qt::WA_StyledBackground, true);
    m_body->setCursor(Qt::PointingHandCursor);
    m_body->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_body->installEventFilter(this);
    auto* bodyLayout = new QVBoxLayout(m_body);
    bodyLayout->setContentsMargins(12, 10, 12, 12);
    bodyLayout->setSpacing(8);

    auto* headerRow = new QHBoxLayout();
    headerRow->setSpacing(0);
    auto* contadorTitle = new QLabel(tr("Contador"), m_body);
    contadorTitle->setObjectName(QStringLiteral("wcpSectionTitle"));
    headerRow->addWidget(contadorTitle);
    headerRow->addStretch();
    bodyLayout->addLayout(headerRow);

    auto* cards = new QHBoxLayout();
    cards->setSpacing(8);
    cards->setContentsMargins(0, 0, 0, 0);

    auto makeCard = [this](const QString& title, QLabel** outValue, QLabel** outTitle) -> QFrame* {
        auto* card = new QFrame(m_body);
        card->setObjectName(QStringLiteral("wcpCard"));
        card->setAttribute(Qt::WA_StyledBackground, true);
        auto* lay = new QVBoxLayout(card);
        lay->setContentsMargins(12, 8, 12, 8);
        lay->setSpacing(2);
        auto* lblTitle = new QLabel(title, card);
        lblTitle->setObjectName(QStringLiteral("wcpCardTitle"));
        lblTitle->setWordWrap(true);
        auto* lblValue = new QLabel(QStringLiteral("0"), card);
        lblValue->setObjectName(QStringLiteral("wcpCardValue"));
        lay->addWidget(lblTitle);
        lay->addWidget(lblValue);
        *outValue = lblValue;
        *outTitle = lblTitle;
        return card;
    };

    cards->addWidget(makeCard(tr("Palavras"),   &m_slot1Value, &m_slot1Title));
    cards->addWidget(makeCard(tr("Caracteres"), &m_slot2Value, &m_slot2Title));
    bodyLayout->addLayout(cards);

    // Barra de progresso compacta da meta diária
    m_compactGoalBar = new QProgressBar(m_body);
    m_compactGoalBar->setObjectName(QStringLiteral("wcpProgress"));
    m_compactGoalBar->setRange(0, 100);
    m_compactGoalBar->setValue(0);
    m_compactGoalBar->setTextVisible(false);
    m_compactGoalBar->setFixedHeight(5);
    m_compactGoalBar->setVisible(false);
    bodyLayout->addWidget(m_compactGoalBar);

    m_compactGoalResetLabel = new QLabel(QString(), m_body);
    m_compactGoalResetLabel->setObjectName(QStringLiteral("wcpMeta"));
    m_compactGoalResetLabel->setAlignment(Qt::AlignCenter);
    m_compactGoalResetLabel->setVisible(false);
    bodyLayout->addWidget(m_compactGoalResetLabel);

    outer->addWidget(m_body);

    // Full body — só visível em full mode.
    m_fullBody = new QFrame(this);
    m_fullBody->setObjectName(QStringLiteral("wcpFullBody"));
    m_fullBody->setAttribute(Qt::WA_StyledBackground, true);
    auto* fullLayout = new QVBoxLayout(m_fullBody);
    fullLayout->setContentsMargins(0, 8, 0, 0);
    fullLayout->setSpacing(8);
    fullLayout->addWidget(buildScopeSection());
    fullLayout->addWidget(buildGoalSection());

    // Toggle "Exibir calendário" + calendário (hidden by default)
    m_calendarToggleBtn = new QToolButton(m_fullBody);
    m_calendarToggleBtn->setObjectName(QStringLiteral("wcpCalToggleBtn"));
    m_calendarToggleBtn->setText(QStringLiteral("▸ ") + tr("Exibir calendário"));
    m_calendarToggleBtn->setCursor(Qt::PointingHandCursor);
    m_calendarToggleBtn->setAutoRaise(true);
    connect(m_calendarToggleBtn, &QToolButton::clicked, this, [this]() {
        m_calendarVisible = !m_calendarVisible;
        if (m_calendar) m_calendar->setVisible(m_calendarVisible);
        m_calendarToggleBtn->setText((m_calendarVisible ? QStringLiteral("▾ ") : QStringLiteral("▸ ")) + tr("Exibir calendário"));
        // Adia para o próximo ciclo do event loop: o layout recalcula o sizeHint
        // correto ANTES do adjustSize, evitando que m_body estique para preencher
        // o espaço sobrante do calendário que acabou de ser ocultado.
        QMetaObject::invokeMethod(this, [this]() {
            adjustSize();
            emit geometryChanged();
        }, Qt::QueuedConnection);
    });
    fullLayout->addWidget(m_calendarToggleBtn);

    m_calendar = new WordCounterCalendar(m_counter, m_fullBody);
    m_calendar->setVisible(false);
    connect(m_calendar, &WordCounterCalendar::geometryChanged, this, [this]() {
        adjustSize();
        emit geometryChanged();
    });
    fullLayout->addWidget(m_calendar);

    m_fullBody->setVisible(false);
    outer->addWidget(m_fullBody);

    applyThemeStyle();

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &WordCountPanel::applyThemeStyle);
}

void WordCountPanel::applyThemeStyle()
{
    // Stylesheet inteira derivada do Theme::, com fallback semântico em vermelho
    // pra "lazyass" (warning) que não muda por tema.
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
        QWidget#wordCountPanel { background: transparent; }
        QFrame#wcpBody, QFrame#wcpFullBody, QFrame#wcpSection {
            background: %1;
            border: 1px solid %4;
            border-radius: 6px;
        }
        QToolButton#wcpToggle {
            background: %1;
            color: %6;
            border: 1px solid %4;
            border-bottom: none;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            font-size: 10px;
            padding: 0;
        }
        QToolButton#wcpToggle:hover { color: %8; }
        QLabel#wcpSectionTitle {
            color: %8; font-size: 12px; font-weight: 600;
        }
        QLabel#wcpMeta { color: %7; font-size: 11px; }
        QFrame#wcpCard {
            background: %2;
            border: 1px solid %5;
            border-radius: 4px;
        }
        QLabel#wcpCardTitle { color: %7; font-size: 11px; }
        QLabel#wcpCardValue { color: %8; font-size: 15px; font-weight: 600; }
        QPushButton#wcpScopeBtn {
            background: %2;
            color: %6;
            border: 1px solid %5;
            border-radius: 4px;
            padding: 6px 10px;
            text-align: left;
        }
        QPushButton#wcpScopeBtn:hover { background: %3; color: %8; }
        QPushButton#wcpScopeBtn[selected="true"] {
            background: %3;
            color: %8;
            border-color: %9;
        }
        QProgressBar#wcpProgress {
            background: %2;
            border: 1px solid %5;
            border-radius: 3px;
            height: 6px;
            text-align: center;
            color: transparent;
        }
        QProgressBar#wcpProgress::chunk {
            background: %9;
            border-radius: 2px;
        }
        QComboBox, QSpinBox {
            background: %2;
            color: %8;
            border: 1px solid %5;
            border-radius: 3px;
            padding: 3px 6px;
            min-height: 26px;
        }
        QComboBox::drop-down { border: none; width: 16px; }
        QComboBox[lazyass="true"] {
            border: 1px solid %10;
            color: %10;
        }
        QLabel#wcpLazyass {
            color: %10;
            font-size: 11px;
            font-style: italic;
        }
        QFrame#wcpFolgaPanel {
            background: %2;
            border: 1px solid %5;
            border-radius: 4px;
        }
        QToolButton#wcpCalToggleBtn {
            background: transparent;
            color: %7;
            border: none;
            font-size: 11px;
            padding: 4px 0;
            text-align: left;
        }
        QToolButton#wcpCalToggleBtn:hover { color: %6; }
        QToolButton#wcpSpinBtn {
            background: %2;
            color: %6;
            border: 1px solid %5;
            border-radius: 3px;
            font-size: 8px;
            min-width: 20px;
            max-width: 24px;
            min-height: 12px;
            max-height: 14px;
            padding: 0;
        }
        QToolButton#wcpSpinBtn:hover { background: %3; color: %8; }
        QComboBox QAbstractItemView {
            background: %1;
            color: %8;
            selection-background-color: %3;
            border: 1px solid %4;
        }
        QToolButton#wcpResetBtn {
            background: %2;
            color: %6;
            border: 1px solid %5;
            border-radius: 3px;
            padding: 3px 6px;
            min-width: 22px;
        }
        QToolButton#wcpResetBtn:hover { background: %3; color: %8; }
    )")
        .arg(bgPanel,    // 1
             bgCard,     // 2
             bgHover,    // 3
             border,     // 4
             borderSub,  // 5
             txtPrimary, // 6
             txtMuted,   // 7
             txtBright,  // 8
             accent,     // 9
             Theme::accentWarning()) // 10
    );
}

QFrame* WordCountPanel::buildScopeSection()
{
    auto* section = new QFrame(m_fullBody);
    section->setObjectName(QStringLiteral("wcpSection"));
    section->setAttribute(Qt::WA_StyledBackground, true);
    auto* lay = new QVBoxLayout(section);
    lay->setContentsMargins(12, 10, 12, 12);
    lay->setSpacing(6);

    auto* title = new QLabel(tr("Contar palavras"), section);
    title->setObjectName(QStringLiteral("wcpSectionTitle"));
    lay->addWidget(title);

    auto makeBtn = [this, section](const QString& text, const QString& scopeKey) {
        auto* b = new QPushButton(text, section);
        b->setObjectName(QStringLiteral("wcpScopeBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setCheckable(false);
        connect(b, &QPushButton::clicked, this, [this, scopeKey]() {
            if (m_counter) m_counter->setScope(scopeKey);
        });
        return b;
    };

    m_scopeManuscriptBtn = makeBtn(tr("Apenas nos capítulos"), QStringLiteral("manuscript"));
    m_scopeActiveBtn     = makeBtn(tr("No documento em edição"), QStringLiteral("active"));
    m_scopeDrawersBtn    = makeBtn(tr("Documentos das gavetas"), QStringLiteral("drawers"));
    m_scopeAllBtn        = makeBtn(tr("Todos os documentos do projeto"), QStringLiteral("all"));

    lay->addWidget(m_scopeManuscriptBtn);
    lay->addWidget(m_scopeActiveBtn);
    lay->addWidget(m_scopeDrawersBtn);
    lay->addWidget(m_scopeAllBtn);

    return section;
}

QFrame* WordCountPanel::buildGoalSection()
{
    auto* section = new QFrame(m_fullBody);
    section->setObjectName(QStringLiteral("wcpSection"));
    section->setAttribute(Qt::WA_StyledBackground, true);
    auto* lay = new QVBoxLayout(section);
    lay->setContentsMargins(12, 10, 12, 12);
    lay->setSpacing(6);

    auto* headerRow = new QHBoxLayout();
    auto* title = new QLabel(tr("Meta diária"), section);
    title->setObjectName(QStringLiteral("wcpSectionTitle"));
    headerRow->addWidget(title);
    headerRow->addStretch();
    m_folgaBtn = new QToolButton(section);
    m_folgaBtn->setObjectName(QStringLiteral("wcpResetBtn"));
    m_folgaBtn->setText(QStringLiteral("F"));
    m_folgaBtn->setCursor(Qt::PointingHandCursor);
    m_folgaBtn->setToolTip(tr("Configurar folgas"));
    m_folgaBtn->setCheckable(true);
    connect(m_folgaBtn, &QToolButton::toggled, this, [this](bool on) {
        if (m_folgaPanel) {
            m_folgaPanel->setVisible(on);
            if (!on) setLazyassMode(false); // Mira 1: fechar painel F desativa lazyass
            adjustSize();
            emit geometryChanged();
        }
    });
    headerRow->addWidget(m_folgaBtn);

    m_goalResetBtn = new QToolButton(section);
    m_goalResetBtn->setObjectName(QStringLiteral("wcpResetBtn"));
    m_goalResetBtn->setText(QStringLiteral("↻"));
    m_goalResetBtn->setCursor(Qt::PointingHandCursor);
    m_goalResetBtn->setToolTip(tr("Reiniciar o dia da meta"));
    connect(m_goalResetBtn, &QToolButton::clicked, this, [this]() {
        if (m_counter) m_counter->resetGoalDay();
    });
    headerRow->addWidget(m_goalResetBtn);
    lay->addLayout(headerRow);

    // Painel de folgas (inline, escondido por default)
    m_folgaPanel = new QFrame(section);
    m_folgaPanel->setObjectName(QStringLiteral("wcpFolgaPanel"));
    m_folgaPanel->setAttribute(Qt::WA_StyledBackground, true);
    m_folgaPanel->setVisible(false);
    auto* folgaLayout = new QVBoxLayout(m_folgaPanel);
    folgaLayout->setContentsMargins(10, 8, 10, 10);
    folgaLayout->setSpacing(6);

    auto* folgaTitle = new QLabel(tr("Folgas"), m_folgaPanel);
    folgaTitle->setObjectName(QStringLiteral("wcpMeta"));
    folgaLayout->addWidget(folgaTitle);

    m_folgaStatus = new QLabel(QString(), m_folgaPanel);
    m_folgaStatus->setObjectName(QStringLiteral("wcpMeta"));
    m_folgaStatus->setWordWrap(true);
    folgaLayout->addWidget(m_folgaStatus);

    auto* everyRow = new QHBoxLayout();
    auto* everyLabel = new QLabel(tr("A cada"), m_folgaPanel);
    everyLabel->setObjectName(QStringLiteral("wcpMeta"));
    everyRow->addWidget(everyLabel);
    m_folgaEveryCombo = new QComboBox(m_folgaPanel);
    m_folgaEveryCombo->addItem(tr("0 (ilimitado)"), 0);
    for (int n : {1, 2, 3, 5, 7, 10, 15}) {
        m_folgaEveryCombo->addItem(tr("%1 dias").arg(n), n);
    }
    m_folgaEveryCombo->addItem(tr("Nunca"), 999);
    connect(m_folgaEveryCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_updatingFromSettings || !m_counter) return;
        const int v = m_folgaEveryCombo->currentData().toInt();
        if (v == m_counter->settings().offDayEvery) return;
        if (m_counter->isOffDayChangeLocked() && !m_lazyassMode) {
            // Reverte selection visual e mostra aviso (Mira 1 trava com modal — vou usar status inline).
            refreshFromSettings();
            if (m_folgaStatus) {
                m_folgaStatus->setText(tr("Bloqueado: faltam %1 dia(s) batendo a meta antes de poder mudar.")
                    .arg(m_counter->daysUntilOffDayChange()));
            }
            return;
        }
        const auto answer = QMessageBox::question(this, tr("Trocar cadência"),
            tr("Tem certeza? Você só poderá trocar de novo depois de bater %1 dias de meta.")
                .arg(v == 999 || v == 0 ? 0 : v),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            refreshFromSettings();
            return;
        }
        m_counter->setOffDayEvery(v);
        refreshFromSettings();
    });
    everyRow->addWidget(m_folgaEveryCombo);
    everyRow->addStretch();
    folgaLayout->addLayout(everyRow);

    m_folgaLockLine = new QLabel(QString(), m_folgaPanel);
    m_folgaLockLine->setObjectName(QStringLiteral("wcpMeta"));
    m_folgaLockLine->setWordWrap(true);
    folgaLayout->addWidget(m_folgaLockLine);

    m_lazyassLabel = new QLabel(QString(), m_folgaPanel);
    m_lazyassLabel->setObjectName(QStringLiteral("wcpLazyass"));
    m_lazyassLabel->setVisible(false);
    folgaLayout->addWidget(m_lazyassLabel);

    lay->addWidget(m_folgaPanel);

    m_goalStatus = new QLabel(QStringLiteral(""), section);
    m_goalStatus->setObjectName(QStringLiteral("wcpMeta"));
    lay->addWidget(m_goalStatus);

    m_goalProgress = new QProgressBar(section);
    m_goalProgress->setObjectName(QStringLiteral("wcpProgress"));
    m_goalProgress->setRange(0, 100);
    m_goalProgress->setValue(0);
    m_goalProgress->setTextVisible(false);
    m_goalProgress->setFixedHeight(8);
    lay->addWidget(m_goalProgress);

    m_goalResetLine = new QLabel(QStringLiteral(""), section);
    m_goalResetLine->setObjectName(QStringLiteral("wcpMeta"));
    lay->addWidget(m_goalResetLine);

    // Linha de estatísticas (2 rows × 3 cols)
    auto* statsGrid = new QGridLayout();
    statsGrid->setHorizontalSpacing(8);
    statsGrid->setVerticalSpacing(2);
    auto makeStat = [section](QLabel** out) {
        auto* lbl = new QLabel(QString(), section);
        lbl->setObjectName(QStringLiteral("wcpMeta"));
        *out = lbl;
        return lbl;
    };
    statsGrid->addWidget(makeStat(&m_statStreak),   0, 0);
    statsGrid->addWidget(makeStat(&m_statRecord),   0, 1);
    statsGrid->addWidget(makeStat(&m_statToday),    0, 2);
    statsGrid->addWidget(makeStat(&m_statPages),    1, 0);
    statsGrid->addWidget(makeStat(&m_statAvgWords), 1, 1);
    statsGrid->addWidget(makeStat(&m_statAvgTime),  1, 2);
    lay->addLayout(statsGrid);

    auto* selectsGrid = new QGridLayout();
    selectsGrid->setHorizontalSpacing(8);
    selectsGrid->setVerticalSpacing(2);

    auto* contaLabel = new QLabel(tr("Conta"), section);
    contaLabel->setObjectName(QStringLiteral("wcpMeta"));
    auto* tipoLabel = new QLabel(tr("Tipo"), section);
    tipoLabel->setObjectName(QStringLiteral("wcpMeta"));
    selectsGrid->addWidget(contaLabel, 0, 0);
    selectsGrid->addWidget(tipoLabel, 0, 1);

    m_goalScopeCombo = new QComboBox(section);
    m_goalScopeCombo->addItem(tr("Somente capítulos"), QStringLiteral("manuscript"));
    m_goalScopeCombo->addItem(tr("Documentos das gavetas"), QStringLiteral("all-items"));
    m_goalScopeCombo->addItem(tr("Tudo"), QStringLiteral("all-items-manuscript"));
    connect(m_goalScopeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_updatingFromSettings || !m_counter) return;
        m_counter->setGoalScope(m_goalScopeCombo->currentData().toString());
    });

    m_goalTypeCombo = new QComboBox(section);
    m_goalTypeCombo->addItem(tr("Palavras"), QStringLiteral("words"));
    m_goalTypeCombo->addItem(tr("Tempo"), QStringLiteral("time"));
    connect(m_goalTypeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_updatingFromSettings || !m_counter) return;
        m_counter->setGoalType(m_goalTypeCombo->currentData().toString());
        refreshFromSettings();
        refresh();
    });

    selectsGrid->addWidget(m_goalScopeCombo, 1, 0);
    selectsGrid->addWidget(m_goalTypeCombo, 1, 1);

    m_goalValueLabel = new QLabel(tr("Palavras"), section);
    m_goalValueLabel->setObjectName(QStringLiteral("wcpMeta"));
    selectsGrid->addWidget(m_goalValueLabel, 2, 0);

    m_goalValueSpin = new QSpinBox(section);
    m_goalValueSpin->setRange(1, 100000);
    m_goalValueSpin->setSingleStep(50);
    m_goalValueSpin->setFixedWidth(80);
    m_goalValueSpin->setAlignment(Qt::AlignCenter);
    m_goalValueSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    connect(m_goalValueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_updatingFromSettings || !m_counter) return;
        if (m_counter->settings().goalType == QStringLiteral("time")) {
            m_counter->setGoalTargetMinutes(v);
        } else {
            m_counter->setGoalTargetWords(v);
        }
    });

    auto* spinUp = new QToolButton(section);
    spinUp->setObjectName(QStringLiteral("wcpSpinBtn"));
    spinUp->setText(QStringLiteral("▲"));
    spinUp->setAutoRepeat(true);
    spinUp->setAutoRepeatInterval(80);
    spinUp->setCursor(Qt::PointingHandCursor);
    connect(spinUp, &QToolButton::clicked, this, [this]() {
        m_goalValueSpin->setValue(m_goalValueSpin->value() + m_goalValueSpin->singleStep());
    });
    auto* spinDown = new QToolButton(section);
    spinDown->setObjectName(QStringLiteral("wcpSpinBtn"));
    spinDown->setText(QStringLiteral("▼"));
    spinDown->setAutoRepeat(true);
    spinDown->setAutoRepeatInterval(80);
    spinDown->setCursor(Qt::PointingHandCursor);
    connect(spinDown, &QToolButton::clicked, this, [this]() {
        m_goalValueSpin->setValue(m_goalValueSpin->value() - m_goalValueSpin->singleStep());
    });

    auto* spinRow = new QHBoxLayout();
    spinRow->setSpacing(3);
    spinRow->setContentsMargins(0, 0, 0, 0);
    spinRow->addWidget(m_goalValueSpin);
    auto* btnsCol = new QVBoxLayout();
    btnsCol->setSpacing(2);
    btnsCol->setContentsMargins(0, 0, 0, 0);
    btnsCol->addWidget(spinUp);
    btnsCol->addWidget(spinDown);
    spinRow->addLayout(btnsCol);
    spinRow->addStretch();

    auto* spinWrap = new QWidget(section);
    spinWrap->setLayout(spinRow);
    selectsGrid->addWidget(spinWrap, 3, 0, 1, 2);

    lay->addLayout(selectsGrid);
    return section;
}

void WordCountPanel::updateToggleArrow()
{
    if (!m_toggleButton) return;
    m_toggleButton->setText(m_expanded ? QStringLiteral("▼") : QStringLiteral("▲"));
}

void WordCountPanel::setExpanded(bool expanded)
{
    if (m_expanded == expanded) return;
    m_expanded = expanded;
    if (m_body) m_body->setVisible(expanded);
    if (m_fullBody) m_fullBody->setVisible(expanded && m_fullMode);
    updateToggleArrow();
    adjustSize();
    emit geometryChanged();
}

void WordCountPanel::setFullMode(bool full)
{
    if (m_fullMode == full) return;
    m_fullMode = full;
    if (m_fullBody) m_fullBody->setVisible(m_expanded && m_fullMode);
    adjustSize();
    emit geometryChanged();
}

bool WordCountPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_body && event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            setFullMode(!m_fullMode);
            return true;
        }
    }
    if (watched == m_body && event->type() == QEvent::ContextMenu) {
        auto* ce = static_cast<QContextMenuEvent*>(event);
        openCompactContextMenu(ce->globalPos());
        return true;
    }
    // Cheat code global: Ctrl + l-a-z-y-a-s-s. Só checa enquanto o painel F está aberto.
    if (event->type() == QEvent::KeyPress && m_folgaPanel && m_folgaPanel->isVisible()) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->modifiers() & Qt::ControlModifier) {
            const QString t = ke->text().toLower();
            if (!t.isEmpty() && t[0].isLetter()) {
                m_lazyBuffer.append(t);
                if (m_lazyBuffer.size() > 12) m_lazyBuffer = m_lazyBuffer.right(12);
                if (m_lazyBuffer.endsWith(QStringLiteral("lazyass"))) {
                    setLazyassMode(!m_lazyassMode);
                    m_lazyBuffer.clear();
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void WordCountPanel::setLazyassMode(bool on)
{
    if (m_lazyassMode == on) return;
    m_lazyassMode = on;
    if (m_lazyassLabel) {
        m_lazyassLabel->setVisible(on);
        m_lazyassLabel->setText(on ? tr("🤫 lazyass mode") : QString());
    }
    if (m_folgaEveryCombo) {
        m_folgaEveryCombo->setProperty("lazyass", on ? "true" : "false");
        m_folgaEveryCombo->style()->unpolish(m_folgaEveryCombo);
        m_folgaEveryCombo->style()->polish(m_folgaEveryCombo);
    }
}

void WordCountPanel::refreshFromSettings()
{
    if (!m_counter) return;
    m_updatingFromSettings = true;
    const auto s = m_counter->settings();

    // Scope buttons
    auto setSel = [](QPushButton* b, bool sel) {
        if (!b) return;
        b->setProperty("selected", sel ? "true" : "false");
        b->style()->unpolish(b);
        b->style()->polish(b);
    };
    setSel(m_scopeManuscriptBtn, s.scope == QStringLiteral("manuscript"));
    setSel(m_scopeActiveBtn, s.scope == QStringLiteral("active"));
    setSel(m_scopeDrawersBtn, s.scope == QStringLiteral("drawers"));
    setSel(m_scopeAllBtn, s.scope == QStringLiteral("all"));

    // Goal scope combo
    if (m_goalScopeCombo) {
        const int idx = m_goalScopeCombo->findData(s.goalScope);
        if (idx >= 0) m_goalScopeCombo->setCurrentIndex(idx);
    }
    if (m_goalTypeCombo) {
        const int idx = m_goalTypeCombo->findData(s.goalType);
        if (idx >= 0) m_goalTypeCombo->setCurrentIndex(idx);
    }
    if (m_goalValueSpin && m_goalValueLabel) {
        const bool isTime = s.goalType == QStringLiteral("time");
        m_goalValueLabel->setText(isTime ? tr("Minutos") : tr("Palavras"));
        m_goalValueSpin->setRange(1, isTime ? 1440 : 100000);
        m_goalValueSpin->setSingleStep(isTime ? 5 : 50);
        const int v = isTime ? s.goalTargetMinutes : s.goalTargetWords;
        m_goalValueSpin->setValue(qMax(1, v));
    }

    if (m_folgaEveryCombo) {
        const int idx = m_folgaEveryCombo->findData(s.offDayEvery);
        m_folgaEveryCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    if (m_folgaStatus && m_counter) {
        const int rem = m_counter->remainingFolgas();
        const QString remStr = rem == 999 ? tr("ilimitado") : QString::number(rem);
        m_folgaStatus->setText(tr("Folgas disponíveis: %1").arg(remStr));
    }
    if (m_folgaLockLine && m_counter) {
        const int wait = m_counter->daysUntilOffDayChange();
        if (wait > 0) {
            m_folgaLockLine->setText(tr("🔒 Cadência travada — bata mais %1 dia(s) de meta pra destravar.").arg(wait));
        } else {
            m_folgaLockLine->setText(tr("✓ Cadência desbloqueada."));
        }
    }

    // Títulos dos slots compactos
    const QString wordScope = s.scope;
    auto slotLabel = [this, &wordScope](const QString& metric, const QString& scope) -> QString {
        if (metric == QStringLiteral("words")) {
            if (wordScope == QStringLiteral("manuscript")) return tr("Palavras (cap.)");
            if (wordScope == QStringLiteral("active"))     return tr("Palavras (atual)");
            if (wordScope == QStringLiteral("drawers"))    return tr("Palavras (gavetas)");
            return tr("Palavras (tudo)");
        }
        if (metric == QStringLiteral("words-session"))
            return scope == QStringLiteral("all") ? tr("Palavras hoje") : tr("Palavras hoje (cap.)");
        if (metric == QStringLiteral("chars"))
            return tr("Caracteres");
        if (metric == QStringLiteral("pages"))
            return tr("Páginas");
        if (metric == QStringLiteral("pages-session"))
            return scope == QStringLiteral("all") ? tr("Páginas hoje") : tr("Páginas hoje (cap.)");
        if (metric == QStringLiteral("time-session"))
            return tr("Tempo hoje");
        return tr("Palavras");
    };
    if (m_slot1Title) m_slot1Title->setText(slotLabel(s.compactSlot1, s.compactSlot1Scope));
    if (m_slot2Title) m_slot2Title->setText(slotLabel(s.compactSlot2, s.compactSlot2Scope));

    const bool showBar = s.compactShowGoalBar;
    if (m_compactGoalBar)        m_compactGoalBar->setVisible(showBar);
    if (m_compactGoalResetLabel) m_compactGoalResetLabel->setVisible(showBar);

    m_updatingFromSettings = false;
}

void WordCountPanel::refresh()
{
    if (!m_counter || !m_slot1Value || !m_slot2Value) return;

    QLocale loc(QLocale::Portuguese, QLocale::Brazil);
    const auto s = m_counter->settings();

    auto slotValue = [&](const QString& metric, const QString& scope) -> QString {
        if (metric == QStringLiteral("words"))
            return loc.toString(m_counter->countActiveScopeWords());
        if (metric == QStringLiteral("words-session")) {
            const int w = (scope == QStringLiteral("all"))
                ? m_counter->sessionWordsAll()
                : m_counter->sessionWordsManuscript();
            return loc.toString(w);
        }
        if (metric == QStringLiteral("chars"))
            return loc.toString(m_counter->countActiveScopeChars());
        if (metric == QStringLiteral("pages"))
            return loc.toString(m_counter->estimatedPages());
        if (metric == QStringLiteral("pages-session")) {
            const int w = (scope == QStringLiteral("all"))
                ? m_counter->sessionWordsAll()
                : m_counter->sessionWordsManuscript();
            return loc.toString(qMax(0, (w + 249) / 250));
        }
        if (metric == QStringLiteral("time-session"))
            return fmtDuration(m_counter->progressTimeMs());
        return loc.toString(m_counter->countActiveScopeWords());
    };

    m_slot1Value->setText(slotValue(s.compactSlot1, s.compactSlot1Scope));
    m_slot2Value->setText(slotValue(s.compactSlot2, s.compactSlot2Scope));

    // Barra de progresso compacta
    if (m_compactGoalBar && s.compactShowGoalBar) {
        const bool isTime = s.goalType == QStringLiteral("time");
        int pct = 0;
        if (isTime) {
            const qint64 tMs = m_counter->progressTimeMs();
            const int target = qMax(1, s.goalTargetMinutes);
            pct = qMin(100, static_cast<int>((tMs * 100.0) / (target * 60000.0)));
        } else {
            const int w = m_counter->progressWords();
            const int target = qMax(1, s.goalTargetWords);
            pct = qMin(100, static_cast<int>((w * 100.0) / target));
        }
        m_compactGoalBar->setValue(pct);
        if (m_compactGoalResetLabel) {
            if (m_counter->isGoalMet()) {
                m_compactGoalResetLabel->setText(tr("Meta atingida!"));
            } else {
                m_compactGoalResetLabel->setText(
                    tr("Reinicia em %1").arg(fmtDuration(m_counter->goalDayRemainingMs())));
            }
        }
    }

    // Meta diária (modo full)
    if (m_goalStatus && m_goalProgress) {
        const bool isTime = s.goalType == QStringLiteral("time");
        if (isTime) {
            const qint64 tMs = m_counter->progressTimeMs();
            const int tMin = static_cast<int>(tMs / 60000);
            const int target = qMax(1, s.goalTargetMinutes);
            const int pct = qMin(100, static_cast<int>((tMs * 100.0) / (target * 60000.0)));
            m_goalStatus->setText(tr("Hoje: %1 / %2 minutos (%3%)")
                .arg(tMin).arg(s.goalTargetMinutes).arg(pct));
            m_goalProgress->setValue(pct);
        } else {
            const int w = m_counter->progressWords();
            const int target = qMax(1, s.goalTargetWords);
            const int pct = qMin(100, static_cast<int>((w * 100.0) / target));
            m_goalStatus->setText(tr("Hoje: %1 / %2 palavras (%3%)")
                .arg(loc.toString(w)).arg(loc.toString(s.goalTargetWords)).arg(pct));
            m_goalProgress->setValue(pct);
        }
    }
    if (m_goalResetLine) {
        const qint64 rem = m_counter->goalDayRemainingMs();
        m_goalResetLine->setText(tr("Reinicia em %1").arg(fmtDuration(rem)));
    }

    // Estatísticas
    if (m_statStreak) {
        const int streak = m_counter->currentStreak();
        const int record = m_counter->longestStreak();
        const bool todayMet = m_counter->isGoalMet();
        int activeDays = 0, wordsPerDay = 0, minutesPerDay = 0;
        m_counter->writingAverages(activeDays, wordsPerDay, minutesPerDay);
        const int pages = m_counter->estimatedPages();

        m_statStreak->setText(tr("Streak: %1").arg(streak));
        m_statRecord->setText(tr("Recorde: %1").arg(record));
        m_statToday->setText(tr("Hoje: %1").arg(todayMet ? tr("sim") : tr("não")));
        m_statPages->setText(tr("Páginas: %1").arg(loc.toString(pages)));
        m_statAvgWords->setText(tr("Média: %1 palavras/dia").arg(loc.toString(wordsPerDay)));
        m_statAvgTime->setText(tr("Tempo: %1 min/dia").arg(minutesPerDay));
    }

    setToolTip(tr("Clique para abrir a meta diária\nBotão direito para personalizar o contador"));
}

void WordCountPanel::openCompactContextMenu(const QPoint& globalPos)
{
    if (!m_counter) return;
    const auto s = m_counter->settings();

    struct MetricDef {
        QString metric;
        QString scope;
        QString label;
    };
    const QList<MetricDef> defs = {
        { QStringLiteral("words"),         QString(),                   tr("Palavras") },
        { QStringLiteral("words-session"), QStringLiteral("manuscript"), tr("Palavras hoje (capítulos)") },
        { QStringLiteral("words-session"), QStringLiteral("all"),        tr("Palavras hoje (projeto)") },
        { QStringLiteral("chars"),         QString(),                   tr("Caracteres") },
        { QStringLiteral("pages"),         QString(),                   tr("Páginas") },
        { QStringLiteral("pages-session"), QStringLiteral("manuscript"), tr("Páginas hoje (capítulos)") },
        { QStringLiteral("pages-session"), QStringLiteral("all"),        tr("Páginas hoje (projeto)") },
        { QStringLiteral("time-session"),  QString(),                   tr("Tempo na sessão") },
    };

    QMenu menu(this);

    auto buildSlot = [&](const QString& header, int slot) {
        auto* hdr = menu.addAction(header);
        hdr->setEnabled(false);
        const QString curMetric = (slot == 1) ? s.compactSlot1 : s.compactSlot2;
        const QString curScope  = (slot == 1) ? s.compactSlot1Scope : s.compactSlot2Scope;
        for (const auto& def : defs) {
            auto* act = menu.addAction(QStringLiteral("   ") + def.label);
            act->setCheckable(true);
            act->setChecked(def.metric == curMetric && def.scope == curScope);
            connect(act, &QAction::triggered, this, [this, slot, def]() {
                if (!m_counter) return;
                if (slot == 1) m_counter->setCompactSlot1(def.metric, def.scope);
                else           m_counter->setCompactSlot2(def.metric, def.scope);
            });
        }
    };

    buildSlot(tr("Slot 1"), 1);
    menu.addSeparator();
    buildSlot(tr("Slot 2"), 2);
    menu.addSeparator();

    auto* goalBarAct = menu.addAction(tr("Barra de progresso da meta"));
    goalBarAct->setCheckable(true);
    goalBarAct->setChecked(s.compactShowGoalBar);
    connect(goalBarAct, &QAction::triggered, this, [this](bool checked) {
        if (m_counter) m_counter->setCompactShowGoalBar(checked);
    });

    menu.exec(globalPos);
}
