#include "WordCountPanel.h"

#include "EditorHost.h"
#include "MiniCounterWidget.h"
#include "CounterFaceWidget.h"
#include "ProjectModel.h"
#include "Theme.h"
#include "WordCounter.h"
#include "WordCounterCalendar.h"
#include "WritingStatsDialog.h"

#include <QApplication>
#include <QScreen>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
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
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QTimeEdit>
#include <QTimer>
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

// Locale usada só pra formatação numérica — precisa acompanhar o idioma do
// app, não ficar fixa em pt-BR (ver app/language no QSettings).
QLocale statsLocale() {
    QSettings qs;
    const QString lang = qs.value(QStringLiteral("app/language")).toString();
    return lang.isEmpty() ? QLocale(QLocale::Portuguese, QLocale::Brazil) : QLocale(lang);
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
    m_compactStyle = QSettings().value(QStringLiteral("wordCounter/compactStyle"),
                                       QStringLiteral("classic")).toString();
    m_scalePercent = QSettings().value(QStringLiteral("wordCounter/compactScale"), 100).toInt();
    if (!QList<int>{ 85, 100, 120, 140, 165 }.contains(m_scalePercent)) m_scalePercent = 100;
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
    updateScrollSizing(); // dimensiona o scroll para o conteúdo inicial (compacto)
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

    m_mini = new MiniCounterWidget(this);
    connect(m_mini, &MiniCounterWidget::clicked, this, [this]() { setFullMode(true); });
    connect(m_mini, &MiniCounterWidget::contextMenuRequested,
            this, &WordCountPanel::openCompactContextMenu);
    outer->addWidget(m_mini, 0, Qt::AlignLeft);

    // Cards (compact body) — clicáveis pra abrir/fechar full mode.
    m_body = new QFrame(this);
    m_body->setObjectName(QStringLiteral("wcpBody"));
    m_body->setAttribute(Qt::WA_StyledBackground, true);
    m_body->setCursor(Qt::PointingHandCursor);
    m_body->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_body->installEventFilter(this);
    auto* bodyLayout = new QVBoxLayout(m_body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // O corpo é todo desenhado (clássico incluso): é o que deixa o contador
    // escalar sem refazer layout de widgets.
    m_face = new CounterFaceWidget(m_body);
    connect(m_face, &CounterFaceWidget::statsClicked, this, [this]() { openStatsDialog(m_face); });
    bodyLayout->addWidget(m_face);

    // Largura fixa do painel (compact e full iguais). Impede que a grade de
    // estatísticas (3 colunas) ou um dia com 5 estrelas estiquem o painel —
    // os stats longos quebram em 2 linhas dentro da coluna, como no Mira 1.
    m_body->setFixedWidth(kPanelWidth);

    // Full body — só visível em full mode.
    m_fullBody = new QFrame(this);
    m_fullBody->setObjectName(QStringLiteral("wcpFullBody"));
    m_fullBody->setAttribute(Qt::WA_StyledBackground, true);
    m_fullBody->setFixedWidth(kPanelWidth);
    auto* fullLayout = new QVBoxLayout(m_fullBody);
    fullLayout->setContentsMargins(0, 8, 0, 0);
    fullLayout->setSpacing(8);
    fullLayout->addWidget(buildGoalSection());
    fullLayout->addWidget(buildSprintSection());

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
            updateScrollSizing();
            adjustSize();
            emit geometryChanged();
            if (m_calendarVisible) scrollToBottom(); // calendário visível por padrão
        }, Qt::QueuedConnection);
    });
    fullLayout->addWidget(m_calendarToggleBtn);

    m_calendar = new WordCounterCalendar(m_counter, m_fullBody);
    m_calendar->setVisible(false);
    connect(m_calendar, &WordCounterCalendar::geometryChanged, this, [this]() {
        updateScrollSizing();
        adjustSize();
        emit geometryChanged();
    });
    fullLayout->addWidget(m_calendar);

    m_fullBody->setVisible(false);

    // Conteúdo (cards + full body) dentro de um QScrollArea. Quando o painel fica
    // mais alto que o espaço visível (calendário aberto), o conteúdo rola em vez de
    // ser cortado fora de alcance. O toggle ▼ fica FORA do scroll (sempre visível).
    m_scrollContent = new QWidget();
    m_scrollContent->setObjectName(QStringLiteral("wcpScrollContent"));
    m_scrollContent->setAttribute(Qt::WA_StyledBackground, false);
    m_scrollContent->setFixedWidth(kPanelWidth);
    auto* scrollLay = new QVBoxLayout(m_scrollContent);
    scrollLay->setContentsMargins(0, 0, 0, 0);
    scrollLay->setSpacing(0);
    scrollLay->addWidget(m_body);
    scrollLay->addWidget(m_fullBody);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName(QStringLiteral("wcpScroll"));
    m_scrollArea->setWidget(m_scrollContent);
    // widgetResizable(false): o conteúdo mantém a largura fixa de 256 (os frames
    // não encolhem abaixo do mínimo da grade de stats); a barra de rolagem fica no
    // gutter à direita sem roubar largura do conteúdo.
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFixedWidth(kPanelWidth + kScrollBarW);
    m_scrollArea->viewport()->setAutoFillBackground(false);
    outer->addWidget(m_scrollArea);

    applyThemeStyle();
    updateBodyVisibility();

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

    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QWidget#wordCountPanel { background: transparent; }
        QScrollArea#wcpScroll { background: transparent; border: none; }
        QScrollArea#wcpScroll > QWidget > QWidget { background: transparent; }
        QWidget#wcpScrollContent { background: transparent; }
        QScrollArea#wcpScroll QScrollBar:vertical {
            background: transparent; width: 6px; margin: 0;
        }
        QScrollArea#wcpScroll QScrollBar::handle:vertical {
            background: %5; border-radius: 3px; min-height: 24px;
        }
        QScrollArea#wcpScroll QScrollBar::handle:vertical:hover { background: %7; }
        QScrollArea#wcpScroll QScrollBar::add-line:vertical,
        QScrollArea#wcpScroll QScrollBar::sub-line:vertical { height: 0; }
        QScrollArea#wcpScroll QScrollBar::add-page:vertical,
        QScrollArea#wcpScroll QScrollBar::sub-page:vertical { background: transparent; }
        QFrame#wcpBody, QFrame#wcpFullBody, QFrame#wcpSection {
            background: %1;
            border: 1px solid %4;
            border-radius: @radius-panel;
        }
        QToolButton#wcpToggle {
            background: %1;
            color: %6;
            border: 1px solid %4;
            border-bottom: none;
            border-top-left-radius: @radius-control;
            border-top-right-radius: @radius-control;
            font-size: 10px;
            padding: 0;
        }
        QToolButton#wcpToggle:hover { color: %8; }
        QLabel#wcpSectionTitle {
            color: %8; font-size: 12px; font-weight: 600;
        }
        QLabel#wcpMeta { color: %7; font-size: 11px; }
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
            border-radius: @radius-control;
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
            border-radius: @radius-control;
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
            border-radius: @radius-control;
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
            border-radius: @radius-control;
            padding: 3px 6px;
            min-width: 22px;
        }
        QToolButton#wcpResetBtn:hover { background: %3; color: %8; }
    )"))
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
            updateScrollSizing(); // recalcula a altura do scroll com o painel de folgas
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
    m_goalResetLine->setCursor(Qt::PointingHandCursor);
    m_goalResetLine->setToolTip(tr("Duplo clique para escolher o horário do reset"));
    lay->addWidget(m_goalResetLine);

    // Linha de estatísticas (2 rows × 3 cols)
    auto* statsGrid = new QGridLayout();
    statsGrid->setHorizontalSpacing(8);
    statsGrid->setVerticalSpacing(2);
    auto makeStat = [section](QLabel** out) {
        auto* lbl = new QLabel(QString(), section);
        lbl->setObjectName(QStringLiteral("wcpMeta"));
        lbl->setWordWrap(true); // stats longos quebram em 2 linhas em vez de alargar o painel
        *out = lbl;
        return lbl;
    };
    statsGrid->setColumnStretch(0, 1);
    statsGrid->setColumnStretch(1, 1);
    statsGrid->setColumnStretch(2, 1);
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
    updateBodyVisibility();
    updateToggleArrow();
    updateScrollSizing();
    adjustSize();
    emit geometryChanged();
}

void WordCountPanel::setFullMode(bool full)
{
    if (m_fullMode == full) return;
    m_fullMode = full;
    updateBodyVisibility();
    updateScrollSizing();
    adjustSize();
    emit geometryChanged();
    if (full && m_calendarVisible) scrollToBottom();
}

// O mini só existe no compacto: aberto o modo full, volta o cabeçalho
// clássico (com o botão Estatísticas) em cima das seções.
void WordCountPanel::updateBodyVisibility()
{
    // Os mini são a alternativa de tamanho: só eles trocam o corpo no
    // compacto. Clássico e os estilos padrão aparecem sempre no tamanho normal.
    const bool mini = MiniCounterWidget::isMiniStyle(m_compactStyle) && !m_fullMode;
    if (m_mini) {
        if (mini) m_mini->setStyleKey(m_compactStyle);
        m_mini->setVisible(m_expanded && mini);
    }
    if (m_scrollArea) m_scrollArea->setVisible(m_expanded && !mini);
    if (m_body) m_body->setVisible(m_expanded);
    if (m_fullBody) m_fullBody->setVisible(m_expanded && m_fullMode);

    // No modo full com um mini escolhido, o cabeçalho é o clássico.
    if (m_face)
        m_face->setStyleKey(CounterFaceWidget::isFaceStyle(m_compactStyle)
                            ? m_compactStyle : QStringLiteral("classic"));
    applyScale();
}

void WordCountPanel::applyScale()
{
    const qreal compact = m_scalePercent / 100.0;
    const qreal body = m_fullMode ? 1.0 : compact;   // o full é um formulário: fica em 100%
    const int w = qRound(kPanelWidth * body);
    if (m_mini) m_mini->setScale(compact);
    if (m_face) m_face->setScale(body);
    if (m_body) m_body->setFixedWidth(w);
    if (m_scrollContent) m_scrollContent->setFixedWidth(w);
    if (m_scrollArea) m_scrollArea->setFixedWidth(w + kScrollBarW);
}

void WordCountPanel::setCompactScale(int percent)
{
    if (m_scalePercent == percent) return;
    m_scalePercent = percent;
    QSettings().setValue(QStringLiteral("wordCounter/compactScale"), percent);
    applyScale();
    QMetaObject::invokeMethod(this, [this]() {
        updateScrollSizing();
        adjustSize();
        emit geometryChanged();
    }, Qt::QueuedConnection);
}

void WordCountPanel::openStatsDialog(QWidget* anchor)
{
    auto* dlg = new WritingStatsDialog(m_counter, this);
    dlg->show(); // show() antes de posicionar para ter a altura real do dialog

    const QRect screen = anchor->screen()->availableGeometry();
    const QPoint topLeft = anchor->mapToGlobal(QPoint(0, 0));
    int x = topLeft.x();
    int y = topLeft.y() - dlg->height() - 4; // preferência: acima do botão
    // Se não couber acima (botão colado no topo da tela), abre abaixo.
    if (y < screen.top() + 4)
        y = anchor->mapToGlobal(QPoint(0, anchor->height())).y() + 4;
    // Garante que o dialog inteiro (incluindo o botão de fechar) fique na tela.
    x = qBound(screen.left() + 4, x, screen.right()  - dlg->width()  - 4);
    y = qBound(screen.top()  + 4, y, screen.bottom() - dlg->height() - 4);
    dlg->move(x, y);
}

void WordCountPanel::setCompactStyle(const QString& style)
{
    if (m_compactStyle == style) return;
    m_compactStyle = style;
    QSettings().setValue(QStringLiteral("wordCounter/compactStyle"), style);
    updateBodyVisibility();
    refresh();
    // Adia como no calendário: o layout precisa recalcular o sizeHint do
    // corpo novo antes do scroll e do painel serem redimensionados.
    QMetaObject::invokeMethod(this, [this]() {
        updateScrollSizing();
        adjustSize();
        emit geometryChanged();
    }, Qt::QueuedConnection);
}

void WordCountPanel::setAvailableHeight(int h)
{
    h = qMax(60, h);
    if (m_maxBodyHeight == h) return;
    m_maxBodyHeight = h;
    updateScrollSizing();
    // Não emite geometryChanged: quem chama (positionWordCountPanel) reposiciona.
}

void WordCountPanel::updateScrollSizing()
{
    if (!m_scrollArea || !m_scrollContent) return;
    if (!m_expanded) { m_scrollArea->setFixedHeight(0); return; }
    if (m_scrollContent->layout()) m_scrollContent->layout()->activate();
    const int contentH = m_scrollContent->sizeHint().height();
    // widgetResizable(false): precisamos dimensionar o conteúdo manualmente.
    m_scrollContent->resize(m_scrollContent->width(), contentH);
    int h = contentH;
    if (m_maxBodyHeight > 0) h = qMin(h, m_maxBodyHeight);
    m_scrollArea->setFixedHeight(qMax(0, h));
}

void WordCountPanel::scrollToBottom()
{
    if (!m_scrollArea) return;
    // Adia: a barra só conhece o máximo após o relayout deste ciclo.
    QMetaObject::invokeMethod(this, [this]() {
        if (m_scrollArea)
            m_scrollArea->verticalScrollBar()->setValue(
                m_scrollArea->verticalScrollBar()->maximum());
    }, Qt::QueuedConnection);
}

bool WordCountPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Auto-close: clicar no editor ou em qualquer lugar fora do painel recolhe o
    // modo full (volta ao compacto). Ignora cliques no próprio painel e em janelas
    // dependentes dele (diálogo de estatísticas, calendário, menus de contexto).
    // Auto-close só vale quando NÃO há diálogo modal aberto (detalhe do dia, picker
    // de status — modais, aparecem longe do painel; clicar neles não pode recolher).
    if (event->type() == QEvent::MouseButtonPress && m_fullMode
        && !QApplication::activeModalWidget()) {
        // NÃO usar `watched`: quando o clique cai numa área não-interativa (célula
        // do calendário, fundo de frame), o press é ignorado e PROPAGA pelos pais
        // até sair do painel, fazendo o filtro ver um alvo "de fora". Em vez disso,
        // usamos o widget REALMENTE sob o cursor (widgetAt) e subimos a cadeia de
        // pais (que cruza fronteiras de janela, cobrindo o diálogo de estatísticas).
        const QPoint gp = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
        QWidget* hit = QApplication::widgetAt(gp);
        bool insidePanel = false;
        for (QWidget* p = hit; p; p = p->parentWidget()) {
            if (p == this) { insidePanel = true; break; }
        }
        if (!insidePanel)
            setFullMode(false); // não consome o evento: clique no editor segue normal
    }

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
    if (watched == m_goalResetLine && event->type() == QEvent::MouseButtonDblClick) {
        openGoalResetTimeDialog();
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
    m_slot1Title = slotLabel(s.compactSlot1, s.compactSlot1Scope);
    m_slot2Title = slotLabel(s.compactSlot2, s.compactSlot2Scope);

    updateBodyVisibility();

    m_updatingFromSettings = false;
}

void WordCountPanel::refresh()
{
    if (!m_counter) return;

    QLocale loc = statsLocale();
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

    const QString v1 = slotValue(s.compactSlot1, s.compactSlot1Scope);
    const QString v2 = slotValue(s.compactSlot2, s.compactSlot2Scope);

    int goalPct = 0;
    if (s.goalType == QStringLiteral("time")) {
        const qint64 tMs = m_counter->progressTimeMs();
        const int target = qMax(1, s.goalTargetMinutes);
        goalPct = qMin(100, static_cast<int>((tMs * 100.0) / (target * 60000.0)));
    } else {
        const int w = m_counter->progressWords();
        const int target = qMax(1, s.goalTargetWords);
        goalPct = qMin(100, static_cast<int>((w * 100.0) / target));
    }
    const QString goalLine = m_counter->isGoalMet()
        ? tr("Meta atingida!")
        : tr("Reinicia em %1").arg(fmtDuration(m_counter->goalDayRemainingMs()));


    // Histórico recente (Semana, Semana mini, Anel + tijolinhos). O dia da
    // meta (que pode virar fora da meia-noite) é a referência, não o relógio.
    const bool goalIsTime = s.goalType == QStringLiteral("time");
    QDate goalDay = QDate::fromString(m_counter->currentGoalDayKey(), QStringLiteral("yyyy-MM-dd"));
    if (!goalDay.isValid()) goalDay = QDate::currentDate();
    QVector<int> week;
    for (int i = 6; i >= 0; --i) {
        const QJsonObject o = s.progress.value(goalDay.addDays(-i).toString(QStringLiteral("yyyy-MM-dd"))).toObject();
        week.append(goalIsTime ? static_cast<int>(o.value(QStringLiteral("timeMs")).toDouble(0) / 60000)
                               : o.value(QStringLiteral("words")).toInt(0));
    }
    const int weekGoal = goalIsTime ? s.goalTargetMinutes : s.goalTargetWords;

    if (m_face) {
        auto shortTitle = [this](const QString& metric) -> QString {
            if (metric == QStringLiteral("words-session")) return tr("Palavras hoje");
            if (metric == QStringLiteral("chars"))         return tr("Caracteres");
            if (metric == QStringLiteral("pages"))         return tr("Páginas");
            if (metric == QStringLiteral("pages-session")) return tr("Páginas hoje");
            if (metric == QStringLiteral("time-session"))  return tr("Tempo hoje");
            return tr("Palavras");
        };
        auto unitShort = [this](const QString& metric) -> QString {
            if (metric == QStringLiteral("words-session")) return tr("palavras hoje");
            if (metric == QStringLiteral("chars"))         return tr("caracteres");
            if (metric == QStringLiteral("pages"))         return tr("páginas");
            if (metric == QStringLiteral("pages-session")) return tr("páginas hoje");
            if (metric == QStringLiteral("time-session"))  return tr("hoje");
            return tr("palavras");
        };
        const bool isTime = s.goalType == QStringLiteral("time");
        CounterFaceWidget::Data d;
        d.title = tr("Contador");
        d.statsText = tr("Estatísticas") + QStringLiteral(" ›");
        d.label1 = shortTitle(s.compactSlot1);
        d.label2 = shortTitle(s.compactSlot2);
        d.value1 = v1;
        d.value2 = v2;
        d.fullLabel1 = m_slot1Title;
        d.fullLabel2 = m_slot2Title;
        d.showGoal = s.compactShowGoalBar;
        d.unit1 = unitShort(s.compactSlot1);
        d.unit2 = unitShort(s.compactSlot2);
        d.pct = goalPct;
        d.pctCaption = tr("da meta");
        d.goalLine = goalLine;
        d.fraction = isTime
            ? QStringLiteral("%1 / %2 min").arg(m_counter->progressTimeMs() / 60000).arg(s.goalTargetMinutes)
            : QStringLiteral("%1 / %2").arg(loc.toString(m_counter->progressWords()), loc.toString(s.goalTargetWords));

        d.week = week;
        d.weekGoal = weekGoal;
        for (int i = 6; i >= 0; --i) {
            if (i == 0) {
                d.weekDays.append(tr("hoje"));
            } else {
                QString name = loc.dayName(goalDay.addDays(-i).dayOfWeek(), QLocale::ShortFormat).toLower();
                if (name.endsWith(QLatin1Char('.'))) name.chop(1);
                d.weekDays.append(name);
            }
        }
        int met = 0;
        for (int i = 19; i >= 0; --i) {
            const bool ok = m_counter->dayMetGoal(goalDay.addDays(-i).toString(QStringLiteral("yyyy-MM-dd")));
            d.days.append(ok);
            if (ok) ++met;
        }
        d.daysCaption = tr("%1 de %2 dias").arg(met).arg(20);
        m_face->setData(d);
        m_face->setToolTip(QStringLiteral("%1: %2\n%3: %4")
            .arg(m_slot1Title, v1, m_slot2Title, v2));
    }

    // Contador mínimo: o rótulo longo (linha/pílula) omite a unidade quando o
    // valor já se explica ("12min"); o curto (duas colunas) nunca fica vazio.
    if (m_mini) {
        auto unitFor = [this](const QString& metric, bool shortLabel) -> QString {
            if (metric == QStringLiteral("words-session")) return tr("palavras hoje");
            if (metric == QStringLiteral("chars"))         return tr("caracteres");
            if (metric == QStringLiteral("pages"))         return tr("páginas");
            if (metric == QStringLiteral("pages-session")) return tr("páginas hoje");
            if (metric == QStringLiteral("time-session"))  return shortLabel ? tr("hoje") : QString();
            return tr("palavras");
        };
        const MiniCounterWidget::Slot a{ v1, unitFor(s.compactSlot1, false), unitFor(s.compactSlot1, true) };
        const MiniCounterWidget::Slot b{ v2, unitFor(s.compactSlot2, false), unitFor(s.compactSlot2, true) };
        m_mini->setData(a, b, goalPct, s.compactShowGoalBar, week, weekGoal);

        QString tip = QStringLiteral("%1: %2\n%3: %4")
            .arg(m_slot1Title, v1, m_slot2Title, v2);
        if (s.compactShowGoalBar)
            tip += QStringLiteral("\n") + goalLine;
        m_mini->setToolTip(tip);

        // O painel é posicionado à mão: se o mini mudou de tamanho (o número
        // ganhou um dígito), redimensiona e avisa quem posiciona.
        if (m_mini->isVisible() && m_mini->size() != m_mini->sizeHint()) {
            adjustSize();
            emit geometryChanged();
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

QFrame* WordCountPanel::buildSprintSection()
{
    auto* section = new QFrame(m_fullBody);
    section->setObjectName(QStringLiteral("wcpSection"));
    section->setAttribute(Qt::WA_StyledBackground, true);
    m_sprintBody = section;
    auto* lay = new QVBoxLayout(section);
    lay->setContentsMargins(12, 10, 12, 12);
    lay->setSpacing(6);

    auto* title = new QLabel(tr("Sprint de escrita"), section);
    title->setObjectName(QStringLiteral("wcpSectionTitle"));
    lay->addWidget(title);

    // Configuração em 2 linhas (como as outras seções) — spinbox estica para
    // preencher a largura disponível sem empurrar o painel.
    auto* configGrid = new QGridLayout();
    configGrid->setSpacing(4);
    configGrid->setColumnStretch(1, 1);

    auto* durLabel = new QLabel(tr("Duração:"), section);
    durLabel->setObjectName(QStringLiteral("wcpMeta"));
    m_sprintMinutesSpin = new QSpinBox(section);
    m_sprintMinutesSpin->setRange(1, 120);
    m_sprintMinutesSpin->setValue(25);
    m_sprintMinutesSpin->setSuffix(tr(" min"));
    configGrid->addWidget(durLabel,           0, 0);
    configGrid->addWidget(m_sprintMinutesSpin, 0, 1);

    auto* metaLabel = new QLabel(tr("Meta:"), section);
    metaLabel->setObjectName(QStringLiteral("wcpMeta"));
    m_sprintWordsSpin = new QSpinBox(section);
    m_sprintWordsSpin->setRange(0, 10000);
    m_sprintWordsSpin->setValue(300);
    m_sprintWordsSpin->setSuffix(tr(" palavras"));
    m_sprintWordsSpin->setSingleStep(50);
    configGrid->addWidget(metaLabel,          1, 0);
    configGrid->addWidget(m_sprintWordsSpin,  1, 1);

    lay->addLayout(configGrid);

    // Labels de status (visíveis só durante sprint ativo)
    auto* statusRow = new QHBoxLayout();
    statusRow->setSpacing(10);

    m_sprintTimerLabel = new QLabel(section);
    m_sprintTimerLabel->setObjectName(QStringLiteral("wcpSectionTitle"));
    m_sprintTimerLabel->setVisible(false);
    statusRow->addWidget(m_sprintTimerLabel);

    m_sprintWordsLabel = new QLabel(section);
    m_sprintWordsLabel->setObjectName(QStringLiteral("wcpMeta"));
    m_sprintWordsLabel->setVisible(false);
    statusRow->addWidget(m_sprintWordsLabel);

    statusRow->addStretch();
    lay->addLayout(statusRow);

    // Botão iniciar/parar
    m_sprintActionBtn = new QPushButton(tr("Iniciar sprint"), section);
    m_sprintActionBtn->setObjectName(QStringLiteral("wcpScopeBtn"));
    m_sprintActionBtn->setCursor(Qt::PointingHandCursor);
    connect(m_sprintActionBtn, &QPushButton::clicked, this, [this]() {
        if (m_sprintActive) stopSprint(); else startSprint();
    });
    lay->addWidget(m_sprintActionBtn);

    // Timer interno (1 segundo)
    m_sprintTimer = new QTimer(this);
    m_sprintTimer->setInterval(1000);
    connect(m_sprintTimer, &QTimer::timeout, this, &WordCountPanel::tickSprint);

    return section;
}

void WordCountPanel::startSprint()
{
    if (!m_counter || m_sprintActive) return;
    m_sprintActive = true;
    m_sprintSecondsLeft = (m_sprintMinutesSpin ? m_sprintMinutesSpin->value() : 25) * 60;
    m_sprintStartSession = m_counter->sessionWordsManuscript();

    if (m_sprintMinutesSpin) m_sprintMinutesSpin->setEnabled(false);
    if (m_sprintWordsSpin)   m_sprintWordsSpin->setEnabled(false);
    if (m_sprintActionBtn)   m_sprintActionBtn->setText(tr("Encerrar sprint"));
    if (m_sprintTimerLabel)  m_sprintTimerLabel->setVisible(true);
    if (m_sprintWordsLabel)  m_sprintWordsLabel->setVisible(true);

    tickSprint();
    m_sprintTimer->start();

    updateScrollSizing();
    adjustSize();
    emit geometryChanged();
}

void WordCountPanel::stopSprint()
{
    if (!m_sprintActive) return;
    m_sprintTimer->stop();
    m_sprintActive = false;

    const int wordsWritten = m_counter
        ? qMax(0, m_counter->sessionWordsManuscript() - m_sprintStartSession)
        : 0;
    const int goalWords = m_sprintWordsSpin ? m_sprintWordsSpin->value() : 300;
    const bool goalMet = wordsWritten >= goalWords;

    if (m_sprintMinutesSpin) m_sprintMinutesSpin->setEnabled(true);
    if (m_sprintWordsSpin)   m_sprintWordsSpin->setEnabled(true);
    if (m_sprintActionBtn)   m_sprintActionBtn->setText(tr("Iniciar sprint"));

    if (m_sprintTimerLabel) {
        m_sprintTimerLabel->setText(goalMet ? tr("✓ Meta batida!") : tr("Sprint encerrado"));
    }
    if (m_sprintWordsLabel) {
        m_sprintWordsLabel->setText(tr("%1 palavras escritas").arg(wordsWritten));
    }
}

void WordCountPanel::tickSprint()
{
    if (!m_sprintActive) return;

    // Timer
    if (m_sprintTimerLabel) {
        const int m = m_sprintSecondsLeft / 60;
        const int s = m_sprintSecondsLeft % 60;
        m_sprintTimerLabel->setText(QStringLiteral("%1:%2")
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0')));
    }

    // Palavras escritas neste sprint
    if (m_sprintWordsLabel && m_counter) {
        const int written = qMax(0, m_counter->sessionWordsManuscript() - m_sprintStartSession);
        const int goal    = m_sprintWordsSpin ? m_sprintWordsSpin->value() : 300;
        m_sprintWordsLabel->setText(tr("%1 / %2 palavras").arg(written).arg(goal));
    }

    if (m_sprintSecondsLeft <= 0) {
        stopSprint();
        return;
    }
    --m_sprintSecondsLeft;
}

void WordCountPanel::openGoalResetTimeDialog()
{
    if (!m_counter) return;

    QDialog dlg(this, Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setObjectName(QStringLiteral("wcpDayDialog"));
    dlg.setModal(true);

    auto* lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(18, 16, 18, 14);
    lay->setSpacing(10);

    auto* title = new QLabel(tr("Horário do reset da meta"), &dlg);
    title->setObjectName(QStringLiteral("wcpDayTitle"));
    lay->addWidget(title);

    auto* hint = new QLabel(
        tr("O que já foi escrito hoje NÃO é apagado.\n"
           "A meta que já está em andamento só fica mais curta\n"
           "ou mais longa essa vez, pra se encaixar no horário\n"
           "novo. Dias seguintes, ela sempre vira nesse horário."), &dlg);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("opacity: 0.75; font-size: 11px;"));
    lay->addWidget(hint);

    auto* timeEdit = new QTimeEdit(m_counter->goalResetTime(), &dlg);
    timeEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    lay->addWidget(timeEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addSpacing(4);
    lay->addWidget(buttons);

    dlg.setStyleSheet(Theme::qss(QStringLiteral(R"(
        #wcpDayDialog { background: %1; border: 1px solid %6; border-radius: @radius-panel; }
        #wcpDayDialog QLabel { color: %2; font-size: 12px; }
        #wcpDayDialog QLabel#wcpDayTitle { color: %3; font-size: 15px; font-weight: bold; }
        #wcpDayDialog QTimeEdit {
            background: %6; color: %3; border: 1px solid %6; border-radius: @radius-control;
            padding: 4px 8px; font-size: 13px;
        }
        #wcpDayDialog QPushButton {
            background: %4; color: %2; border: none; padding: 6px 16px; border-radius: @radius-control; font-size: 12px;
        }
        #wcpDayDialog QPushButton:hover { background: %5; color: %3; }
    )")).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::textBright(),
            Theme::hoverOverlay(), Theme::hoverStrong(), Theme::subtleBorder()));

    if (dlg.exec() == QDialog::Accepted) {
        const QTime t = timeEdit->time();
        m_counter->setGoalResetTime(t.hour(), t.minute());
    }
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

    // Estilo mora num submenu: são onze opções, e o menu raiz é dos slots.
    QMenu* styleMenu = menu.addMenu(tr("Estilo"));
    auto addStyles = [&](const QString& header, const QList<QPair<QString, QString>>& list) {
        auto* hdr = styleMenu->addAction(header);
        hdr->setEnabled(false);
        for (const auto& st : list) {
            auto* act = styleMenu->addAction(QStringLiteral("   ") + st.second);
            act->setCheckable(true);
            act->setChecked(m_compactStyle == st.first);
            const QString key = st.first;
            connect(act, &QAction::triggered, this, [this, key]() { setCompactStyle(key); });
        }
    };
    addStyles(tr("Padrão"), {
        { QStringLiteral("classic"),         tr("Clássico") },
        { QStringLiteral("face-ring"),       tr("Anel da meta") },
        { QStringLiteral("face-brickring"),  tr("Anel de tijolinhos") },
        { QStringLiteral("face-ringbricks"), tr("Anel + tijolinhos") },
        { QStringLiteral("face-week"),       tr("Semana") },
        { QStringLiteral("face-bricks"),     tr("Tijolinhos") },
        { QStringLiteral("face-card"),       tr("Ficha de fichário") },
    });
    styleMenu->addSeparator();
    addStyles(tr("Mínimo"), {
        { QStringLiteral("line"),    tr("Linha") },
        { QStringLiteral("columns"), tr("Duas colunas") },
        { QStringLiteral("ring"),    tr("Anel") },
        { QStringLiteral("pill"),    tr("Pílula que enche") },
        { QStringLiteral("ringlet"),     tr("Anelzinho") },
        { QStringLiteral("odometer"),    tr("Odômetro") },
        { QStringLiteral("bricks-mini"), tr("Tijolinhos mini") },
        { QStringLiteral("week-mini"),   tr("Semana mini") },
        { QStringLiteral("ruler"),       tr("Régua") },
        { QStringLiteral("card-mini"),   tr("Ficha mini") },
        { QStringLiteral("tube"),        tr("Coluna") },
    });

    QMenu* sizeMenu = menu.addMenu(tr("Tamanho"));
    const QList<QPair<int, QString>> sizes = {
        { 85,  tr("Pequeno") },
        { 100, tr("Médio") },
        { 120, tr("Grande") },
        { 140, tr("Muito grande") },
        { 165, tr("Enorme") },
    };
    for (const auto& sz : sizes) {
        auto* act = sizeMenu->addAction(QStringLiteral("%1 (%2%)").arg(sz.second).arg(sz.first));
        act->setCheckable(true);
        act->setChecked(m_scalePercent == sz.first);
        const int pct = sz.first;
        connect(act, &QAction::triggered, this, [this, pct]() { setCompactScale(pct); });
    }
    menu.addSeparator();

    // Onde o contador conta: vale pra Palavras, Caracteres e Páginas dos
    // slots. Morava numa seção de quatro botões no modo full.
    QMenu* scopeMenu = menu.addMenu(tr("Contar em"));
    const QList<QPair<QString, QString>> scopes = {
        { QStringLiteral("manuscript"), tr("Apenas nos capítulos") },
        { QStringLiteral("active"),     tr("No documento em edição") },
        { QStringLiteral("drawers"),    tr("Documentos das gavetas") },
        { QStringLiteral("all"),        tr("Todos os documentos do projeto") },
    };
    for (const auto& sc : scopes) {
        auto* act = scopeMenu->addAction(sc.second);
        act->setCheckable(true);
        act->setChecked(s.scope == sc.first);
        const QString key = sc.first;
        connect(act, &QAction::triggered, this, [this, key]() {
            if (m_counter) m_counter->setScope(key);
        });
    }

    auto buildSlot = [&](const QString& header, int slot) {
        QMenu* slotMenu = menu.addMenu(header);
        const QString curMetric = (slot == 1) ? s.compactSlot1 : s.compactSlot2;
        const QString curScope  = (slot == 1) ? s.compactSlot1Scope : s.compactSlot2Scope;
        for (const auto& def : defs) {
            auto* act = slotMenu->addAction(def.label);
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
    buildSlot(tr("Slot 2"), 2);
    menu.addSeparator();

    auto* goalBarAct = menu.addAction(tr("Barra de progresso da meta"));
    goalBarAct->setCheckable(true);
    goalBarAct->setChecked(s.compactShowGoalBar);
    // Nos estilos com anel, semana ou tijolinhos, a meta é o próprio desenho.
    goalBarAct->setEnabled(m_compactStyle == QLatin1String("classic")
                           || MiniCounterWidget::isMiniStyle(m_compactStyle));
    connect(goalBarAct, &QAction::triggered, this, [this](bool checked) {
        if (m_counter) m_counter->setCompactShowGoalBar(checked);
    });

    menu.exec(globalPos);
}
