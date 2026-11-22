#include "WordCountPanel.h"

#include "EditorHost.h"
#include "MiniCounterWidget.h"
#include "CounterFaceWidget.h"
#include "CounterPanelParts.h"
#include "PanelMotion.h"
#include "ProjectModel.h"
#include "Theme.h"
#include "WordCounter.h"
#include "WordCounterCalendar.h"
#include "WritingStatsDialog.h"

#include <QApplication>
#include <QScreen>
#include <QCursor>
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
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
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

// Cor do tema em #rrggbb, pro rich text (que não entende rgba()).
QString hex(const QString& css) { return Theme::toColor(css).name(); }

// Rótulo de campo em versalete ("CONTA", "POR DIA"). A fonte vai no código
// porque QSS não tem letter-spacing; o QSS só dá a cor.
QLabel* makeCap(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("wcpCap"));
    QFont f = l->font();
    f.setPixelSize(10);
    f.setWeight(QFont::DemiBold);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    l->setFont(f);
    return l;
}

QToolButton* makeLink(const QString& text, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(QStringLiteral("wcpLink"));
    b->setText(text);
    b->setAutoRaise(true);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

// Play e stop desenhados (sem emoji nem caractere que o Windows pinta colorido).
QIcon glyphIcon(bool play, const QColor& c)
{
    const qreal dpr = 2.0;
    QPixmap pm(QSize(12, 12) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    if (play) {
        QPainterPath path;
        path.moveTo(2.5, 1.5);
        path.lineTo(10.5, 6);
        path.lineTo(2.5, 10.5);
        path.closeSubpath();
        p.drawPath(path);
    } else {
        p.drawRoundedRect(QRectF(2, 2, 8, 8), 1.5, 1.5);
    }
    return QIcon(pm);
}

// Cadências de folga, na ordem do seletor ‹ ›. 0 = ilimitado, 999 = nunca.
const int kFolgaOptions[] = { 0, 1, 2, 3, 5, 7, 10, 15, 999 };
constexpr int kFolgaCount = int(sizeof(kFolgaOptions) / sizeof(kFolgaOptions[0]));
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

    // Full body — só visível em full mode. Emenda no rosto: o painel vira um
    // só, com as abas logo abaixo do anel (Contador Lab, opção "Abas").
    m_fullBody = new QFrame(this);
    m_fullBody->setObjectName(QStringLiteral("wcpFullBody"));
    m_fullBody->setAttribute(Qt::WA_StyledBackground, true);
    m_fullBody->setFixedWidth(kPanelWidth);
    auto* fullLayout = new QVBoxLayout(m_fullBody);
    fullLayout->setContentsMargins(0, 0, 0, 0);
    fullLayout->setSpacing(0);

    m_tabs = new CounterTabBar(m_fullBody);
    m_tabs->setTabs({ tr("Meta"), tr("Sprint"), tr("Mês") });
    m_tabs->onChanged = [this](int i) {
        QSettings().setValue(QStringLiteral("wordCounter/fullTab"), i);
        showPage(i);
    };
    fullLayout->addWidget(m_tabs);

    // A pilha tem a altura da página mais alta: trocar de aba não mexe no painel.
    m_stack = new QStackedWidget(m_fullBody);
    m_stack->setObjectName(QStringLiteral("wcpStack"));
    m_stack->addWidget(buildGoalPage());
    m_stack->addWidget(buildSprintPage());
    m_stack->addWidget(buildMonthPage());
    m_stack->addWidget(buildFolgasPage());
    fullLayout->addWidget(m_stack);

    const int tab = qBound(0, QSettings().value(QStringLiteral("wordCounter/fullTab"), 0).toInt(), 2);
    m_tabs->setCurrent(tab);
    m_stack->setCurrentIndex(tab);

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
    // Cor própria do contador (Criador de Temas) e a opacidade dos painéis.
    const QString bgPanel    = Theme::panelBackgroundCssFor(Theme::PanelKey::Counter);
    const QString bgCard     = Theme::chromeBackground();
    const QString bgHover    = Theme::hoverOverlay();
    const QString border     = Theme::panelBorderFor(Theme::PanelKey::Counter);
    const QString borderSub  = Theme::subtleBorder();
    const QString txtPrimary = Theme::textPrimary();
    const QString txtMuted   = Theme::textMuted();
    const QString txtBright  = Theme::textBright();
    const QString accent     = Theme::accentDefault();
    // Controles de 26 px: raio no máximo 12, senão o QSS desenha torto.
    const QString ctrlRadius = QStringLiteral("%1px").arg(qMin(Theme::controlRadius(), 12));

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
        QFrame#wcpBody, QFrame#wcpFullBody {
            background: %1;
            border: @panel-border-w solid %4;
            border-color: @border-colors-counter;
            border-radius: @radius-panel;
        }
        QFrame#wcpBody[joined="true"] {
            border-bottom: none;
            border-bottom-left-radius: 0;
            border-bottom-right-radius: 0;
        }
        QFrame#wcpFullBody {
            border-top: 1px solid %5;
            border-top-left-radius: 0;
            border-top-right-radius: 0;
        }
        QStackedWidget#wcpStack, QStackedWidget#wcpStack > QWidget { background: transparent; }
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
        QLabel#wcpSectionTitle { color: %8; font-size: 13px; font-weight: 600; }
        QLabel#wcpGoalNum { color: %6; font-size: 12px; }
        QLabel#wcpMeta { color: %7; font-size: 11px; }
        QLabel#wcpCap { color: %7; }
        QLabel#wcpStatNum { color: %8; font-size: 14px; font-weight: 600; }
        QLabel#wcpStatCap { color: %7; font-size: 10px; }
        QLabel#wcpSprintBig { color: %8; font-size: 38px; font-weight: 600; }
        QToolButton#wcpLink {
            background: transparent; border: none; padding: 0;
            color: %7; font-size: 12px;
        }
        QToolButton#wcpLink:hover { color: %8; }
        QFrame#wcpStepper { background: %2; border: none; border-radius: %11; }
        QFrame#wcpStepper QSpinBox {
            background: transparent; border: none; padding: 0;
            color: %8; font-size: 12px; font-weight: 600; min-height: 26px;
            selection-background-color: %3; selection-color: %8;
        }
        QLabel#wcpFolgaValue { color: %8; font-size: 12px; font-weight: 600; }
        QLabel#wcpFolgaValue[lazyass="true"] { color: %10; }
        QToolButton#wcpStepBtn {
            background: transparent; border: none; padding: 0;
            color: %7; font-size: 14px;
            min-width: 26px; max-width: 26px; min-height: 26px; max-height: 26px;
            border-radius: %11;
        }
        QToolButton#wcpStepBtn:hover { color: %8; background: %3; }
        QPushButton#wcpPrimaryBtn {
            background: %9; color: #ffffff; border: none; border-radius: %11;
            font-size: 12px; font-weight: 600; padding: 7px 12px;
        }
        QPushButton#wcpSoftBtn {
            background: %3; color: %6; border: none; border-radius: %11;
            font-size: 12px; font-weight: 600; padding: 7px 12px;
        }
        QPushButton#wcpSoftBtn:hover { background: %12; color: %8; }
        QLabel#wcpLazyass {
            color: %10;
            font-size: 11px;
            font-style: italic;
        }
    )"))
        .arg(bgPanel,    // 1
             bgCard,     // 2
             bgHover,    // 3
             border,     // 4
             borderSub,  // 5
             txtPrimary, // 6
             txtMuted,   // 7
             txtBright,  // 8
             accent)     // 9
        .arg(Theme::accentWarning(), // 10
             ctrlRadius,             // 11
             Theme::hoverStrong())   // 12
    );
    // Os ícones e o rich text levam cor do tema embutida.
    refreshSprintPage();
    refresh();
}

QFrame* WordCountPanel::makeStepper(QSpinBox* spin, QWidget* parent)
{
    // "− 550 +": o número continua digitável; os botões andam um passo.
    auto* box = new QFrame(parent);
    box->setObjectName(QStringLiteral("wcpStepper"));
    box->setAttribute(Qt::WA_StyledBackground, true);
    box->setFixedHeight(26);
    auto* h = new QHBoxLayout(box);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    auto mk = [box, spin](const QString& text, int dir) {
        auto* b = new QToolButton(box);
        b->setObjectName(QStringLiteral("wcpStepBtn"));
        b->setText(text);
        b->setAutoRepeat(true);
        b->setAutoRepeatDelay(350);
        b->setAutoRepeatInterval(80);
        b->setCursor(Qt::PointingHandCursor);
        QObject::connect(b, &QToolButton::clicked, spin, [spin, dir]() { spin->stepBy(dir); });
        return b;
    };
    spin->setParent(box);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setAlignment(Qt::AlignCenter);
    spin->setFrame(false);
    spin->setMinimumWidth(30);
    spin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    h->addWidget(mk(QStringLiteral("\u2212"), -1));
    h->addWidget(spin, 1);
    h->addWidget(mk(QStringLiteral("+"), 1));
    return box;
}

QWidget* WordCountPanel::buildGoalPage()
{
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(14, 12, 14, 13);
    lay->setSpacing(0);

    // "0 / 550 palavras" ............ "reinicia em 13h 12min"
    auto* top = new QHBoxLayout();
    top->setSpacing(6);
    m_goalNumLabel = new QLabel(page);
    m_goalNumLabel->setObjectName(QStringLiteral("wcpGoalNum"));
    m_goalNumLabel->setTextFormat(Qt::RichText);
    top->addWidget(m_goalNumLabel, 1, Qt::AlignBottom);
    m_goalResetLine = new QLabel(page);
    m_goalResetLine->setObjectName(QStringLiteral("wcpMeta"));
    m_goalResetLine->setTextFormat(Qt::RichText);
    m_goalResetLine->setCursor(Qt::PointingHandCursor);
    m_goalResetLine->installEventFilter(this);
    top->addWidget(m_goalResetLine, 0, Qt::AlignRight | Qt::AlignBottom);
    lay->addLayout(top);
    lay->addSpacing(6);
    m_goalBar = new CounterThinBar(page);
    lay->addWidget(m_goalBar);
    lay->addSpacing(13);

    // Quatro números: sequência, recorde, média/dia, tempo por dia.
    auto* stats = new QGridLayout();
    stats->setHorizontalSpacing(0);
    stats->setVerticalSpacing(1);
    const QString caps[4] = { tr("sequência"), tr("recorde"), tr("média/dia"), tr("por dia") };
    const QString tips[4] = {
        tr("Dias seguidos batendo a meta"),
        tr("A maior sequência que você já fez"),
        tr("Média de palavras nos dias em que você escreveu"),
        tr("Média de tempo nos dias em que você escreveu"),
    };
    for (int i = 0; i < 4; ++i) {
        m_statNum[i] = new QLabel(QStringLiteral("0"), page);
        m_statNum[i]->setObjectName(QStringLiteral("wcpStatNum"));
        m_statNum[i]->setAlignment(Qt::AlignCenter);
        m_statNum[i]->setToolTip(tips[i]);
        m_statCap[i] = new QLabel(caps[i], page);
        m_statCap[i]->setObjectName(QStringLiteral("wcpStatCap"));
        m_statCap[i]->setAlignment(Qt::AlignCenter);
        m_statCap[i]->setToolTip(tips[i]);
        stats->addWidget(m_statNum[i], 0, i);
        stats->addWidget(m_statCap[i], 1, i);
        stats->setColumnStretch(i, 1);
    }
    lay->addLayout(stats);
    lay->addSpacing(12);

    // Últimos 14 dias (pedido dele no lab: "fica perfeito").
    auto openMonth = [this]() {
        QSettings().setValue(QStringLiteral("wordCounter/fullTab"), int(PageMonth));
        showPage(PageMonth);
    };
    auto* stripHead = new QHBoxLayout();
    stripHead->setSpacing(6);
    stripHead->addWidget(makeCap(tr("Últimos 14 dias"), page));
    stripHead->addStretch();
    auto* monthLink = makeLink(tr("Mês ›"), page);
    connect(monthLink, &QToolButton::clicked, this, openMonth);
    stripHead->addWidget(monthLink);
    lay->addLayout(stripHead);
    lay->addSpacing(6);
    m_strip = new CounterDayStrip(page);
    m_strip->onClicked = openMonth;
    lay->addWidget(m_strip);
    lay->addSpacing(13);

    // O que conta pra meta.
    lay->addWidget(makeCap(tr("Conta"), page));
    lay->addSpacing(5);
    m_goalScopeSeg = new CounterSegmented(page);
    m_goalScopeSeg->setOptions({ tr("Capítulos"), tr("Gavetas"), tr("Tudo") },
                               { tr("Somente capítulos"), tr("Documentos das gavetas"),
                                 tr("Capítulos e documentos das gavetas") });
    m_goalScopeSeg->onChanged = [this](int i) {
        if (m_updatingFromSettings || !m_counter) return;
        static const char* const kScopes[] = { "manuscript", "all-items", "all-items-manuscript" };
        m_counter->setGoalScope(QString::fromLatin1(kScopes[qBound(0, i, 2)]));
    };
    lay->addWidget(m_goalScopeSeg);
    lay->addSpacing(11);

    auto* two = new QGridLayout();
    two->setHorizontalSpacing(8);
    two->setVerticalSpacing(5);
    two->addWidget(makeCap(tr("Tipo"), page), 0, 0);
    two->addWidget(makeCap(tr("Por dia"), page), 0, 1);
    m_goalTypeSeg = new CounterSegmented(page);
    m_goalTypeSeg->setOptions({ tr("Palavras"), tr("Tempo") });
    m_goalTypeSeg->onChanged = [this](int i) {
        if (m_updatingFromSettings || !m_counter) return;
        m_counter->setGoalType(i == 1 ? QStringLiteral("time") : QStringLiteral("words"));
        refreshFromSettings();
        refresh();
    };
    two->addWidget(m_goalTypeSeg, 1, 0);
    m_goalValueSpin = new QSpinBox(page);
    m_goalValueSpin->setRange(1, 100000);
    m_goalValueSpin->setSingleStep(50);
    connect(m_goalValueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_updatingFromSettings || !m_counter) return;
        if (m_counter->settings().goalType == QStringLiteral("time")) {
            m_counter->setGoalTargetMinutes(v);
        } else {
            m_counter->setGoalTargetWords(v);
        }
    });
    two->addWidget(makeStepper(m_goalValueSpin, page), 1, 1);
    two->setColumnStretch(0, 1);
    two->setColumnStretch(1, 1);
    lay->addLayout(two);
    lay->addStretch(1);
    lay->addSpacing(12);

    // Rodapé: folgas e o menu do reset (horário e reiniciar o dia).
    auto* foot = new QHBoxLayout();
    foot->setSpacing(6);
    m_folgaLink = makeLink(QString(), page);
    connect(m_folgaLink, &QToolButton::clicked, this, [this]() { showPage(PageFolgas); });
    foot->addWidget(m_folgaLink);
    foot->addStretch();
    auto* resetLink = makeLink(tr("Reset ›"), page);
    resetLink->setToolTip(tr("Horário do reset e reiniciar o dia"));
    connect(resetLink, &QToolButton::clicked, this, &WordCountPanel::openGoalResetMenu);
    foot->addWidget(resetLink);
    lay->addLayout(foot);
    return page;
}

QWidget* WordCountPanel::buildMonthPage()
{
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(14, 10, 14, 13);
    lay->setSpacing(0);
    m_calendar = new WordCounterCalendar(m_counter, page);
    lay->addWidget(m_calendar);
    lay->addStretch(1);
    return page;
}

QWidget* WordCountPanel::buildFolgasPage()
{
    // Abre pelo link "Folgas" da aba Meta; "Pronto" volta pra ela.
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(14, 12, 14, 13);
    lay->setSpacing(0);

    auto* head = new QHBoxLayout();
    auto* title = new QLabel(tr("Folgas"), page);
    title->setObjectName(QStringLiteral("wcpSectionTitle"));
    head->addWidget(title);
    head->addStretch();
    auto* done = makeLink(tr("Pronto"), page);
    connect(done, &QToolButton::clicked, this, [this]() { showPage(PageGoal); });
    head->addWidget(done);
    lay->addLayout(head);
    lay->addSpacing(10);

    m_folgaStatus = new QLabel(page);
    m_folgaStatus->setObjectName(QStringLiteral("wcpGoalNum"));
    m_folgaStatus->setWordWrap(true);
    lay->addWidget(m_folgaStatus);
    lay->addSpacing(12);

    lay->addWidget(makeCap(tr("A cada"), page));
    lay->addSpacing(5);
    auto* picker = new QFrame(page);
    picker->setObjectName(QStringLiteral("wcpStepper"));
    picker->setAttribute(Qt::WA_StyledBackground, true);
    picker->setFixedHeight(26);
    auto* ph = new QHBoxLayout(picker);
    ph->setContentsMargins(0, 0, 0, 0);
    ph->setSpacing(0);
    auto mk = [this, picker](const QString& text, int dir) {
        auto* b = new QToolButton(picker);
        b->setObjectName(QStringLiteral("wcpStepBtn"));
        b->setText(text);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, [this, dir]() { stepFolgaCadence(dir); });
        return b;
    };
    m_folgaValue = new QLabel(picker);
    m_folgaValue->setObjectName(QStringLiteral("wcpFolgaValue"));
    m_folgaValue->setAlignment(Qt::AlignCenter);
    ph->addWidget(mk(QStringLiteral("‹"), -1));
    ph->addWidget(m_folgaValue, 1);
    ph->addWidget(mk(QStringLiteral("›"), 1));
    lay->addWidget(picker);
    lay->addSpacing(8);

    m_folgaLockLine = new QLabel(page);
    m_folgaLockLine->setObjectName(QStringLiteral("wcpMeta"));
    m_folgaLockLine->setWordWrap(true);
    lay->addWidget(m_folgaLockLine);

    m_lazyassLabel = new QLabel(QString(), page);
    m_lazyassLabel->setObjectName(QStringLiteral("wcpLazyass"));
    m_lazyassLabel->setVisible(false);
    lay->addWidget(m_lazyassLabel);
    lay->addSpacing(12);

    auto* hint = new QLabel(tr("Pra folgar num dia, clique nele com o botão direito na aba Mês."), page);
    hint->setObjectName(QStringLiteral("wcpMeta"));
    hint->setWordWrap(true);
    lay->addWidget(hint);
    lay->addStretch(1);
    return page;
}

void WordCountPanel::showPage(int page)
{
    if (!m_stack) return;
    // Sair das folgas desliga o lazyass (no Mira 1, fechar o painel F fazia isso).
    if (m_stack->currentIndex() == PageFolgas && page != PageFolgas) setLazyassMode(false);
    m_stack->setCurrentIndex(page);
    if (m_tabs) m_tabs->setCurrent(page == PageFolgas ? int(PageGoal) : page);
    refreshSprintPage();   // o pontinho da aba Sprint depende da aba aberta
}

void WordCountPanel::stepFolgaCadence(int delta)
{
    if (!m_counter) return;
    const int cur = m_counter->settings().offDayEvery;
    int idx = 0;
    for (int i = 0; i < kFolgaCount; ++i)
        if (kFolgaOptions[i] == cur) idx = i;
    const int next = qBound(0, idx + delta, kFolgaCount - 1);
    if (next == idx) return;
    const int v = kFolgaOptions[next];
    if (m_counter->isOffDayChangeLocked() && !m_lazyassMode) {
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
    if (answer != QMessageBox::Yes) return;
    m_counter->setOffDayEvery(v);
    refreshFromSettings();
}

void WordCountPanel::openGoalResetMenu()
{
    if (!m_counter) return;
    QMenu menu(this);
    auto* timeAct = menu.addAction(tr("Horário do reset… (%1)")
        .arg(m_counter->goalResetTime().toString(QStringLiteral("HH:mm"))));
    connect(timeAct, &QAction::triggered, this, &WordCountPanel::openGoalResetTimeDialog);
    auto* resetAct = menu.addAction(tr("Reiniciar o dia da meta"));
    connect(resetAct, &QAction::triggered, this, [this]() {
        if (m_counter) m_counter->resetGoalDay();
    });
    menu.exec(QCursor::pos());
}

void WordCountPanel::updateToggleArrow()
{
    if (!m_toggleButton) return;
    m_toggleButton->setText(m_expanded ? QStringLiteral("▼") : QStringLiteral("▲"));
}

void WordCountPanel::setExpanded(bool expanded)
{
    if (m_expanded == expanded) return;
    // O contador sobe do chão ao abrir e afunda nele ao ocultar.
    PanelMotion::morph(this, [this, expanded]() {
        m_expanded = expanded;
        updateBodyVisibility();
        updateToggleArrow();
        updateScrollSizing();
        adjustSize();
        emit geometryChanged();
    });
}

void WordCountPanel::setFullMode(bool full)
{
    if (m_fullMode == full) return;
    PanelMotion::morph(this, [this, full]() {
        m_fullMode = full;
        updateBodyVisibility();
        updateScrollSizing();
        adjustSize();
        emit geometryChanged();
    });
    // As folgas são um desvio da aba Meta: fechar o painel volta pra ela.
    if (!full && m_stack && m_stack->currentIndex() == PageFolgas) showPage(PageGoal);
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
    // Aberto, o rosto e as abas são um painel só (o rosto perde os cantos de baixo).
    if (m_body) {
        const bool joined = m_expanded && m_fullMode;
        if (m_body->property("joined").toBool() != joined) {
            m_body->setProperty("joined", joined);
            m_body->style()->unpolish(m_body);
            m_body->style()->polish(m_body);
        }
    }

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
    // Cheat code global: Ctrl + l-a-z-y-a-s-s. Só checa com a página das folgas aberta.
    if (event->type() == QEvent::KeyPress && m_fullBody && m_fullBody->isVisible()
        && m_stack && m_stack->currentIndex() == PageFolgas) {
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
    if (m_folgaValue) {
        m_folgaValue->setProperty("lazyass", on ? "true" : "false");
        m_folgaValue->style()->unpolish(m_folgaValue);
        m_folgaValue->style()->polish(m_folgaValue);
    }
}

void WordCountPanel::refreshFromSettings()
{
    if (!m_counter) return;
    m_updatingFromSettings = true;
    const auto s = m_counter->settings();


    // O que conta pra meta e o tipo.
    if (m_goalScopeSeg) {
        int idx = 0;
        if (s.goalScope == QStringLiteral("all-items")) idx = 1;
        else if (s.goalScope == QStringLiteral("all-items-manuscript")) idx = 2;
        m_goalScopeSeg->setCurrent(idx);
    }
    const bool goalIsTime = s.goalType == QStringLiteral("time");
    if (m_goalTypeSeg) m_goalTypeSeg->setCurrent(goalIsTime ? 1 : 0);
    if (m_goalValueSpin) {
        m_goalValueSpin->setRange(1, goalIsTime ? 1440 : 100000);
        m_goalValueSpin->setSingleStep(goalIsTime ? 5 : 50);
        m_goalValueSpin->setSuffix(goalIsTime ? tr(" min") : QString());
        const int v = goalIsTime ? s.goalTargetMinutes : s.goalTargetWords;
        m_goalValueSpin->setValue(qMax(1, v));
    }

    // Folgas: o link da aba Meta e a página.
    {
        const int every = s.offDayEvery;
        QString value;
        if (every == 0)        value = tr("0 (ilimitado)");
        else if (every == 999) value = tr("Nunca");
        else                   value = tr("%1 dias").arg(every);
        if (m_folgaValue) m_folgaValue->setText(value);
        if (m_folgaLink) {
            if (every == 0)        m_folgaLink->setText(tr("Folgas: ilimitadas"));
            else if (every == 999) m_folgaLink->setText(tr("Folgas: nunca"));
            else                   m_folgaLink->setText(tr("Folgas: a cada %1 dias").arg(every));
        }
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

void WordCountPanel::setSelectionWords(int n)
{
    n = qMax(0, n);
    if (n == m_selWords) return;
    m_selWords = n;
    refresh();
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

    QString v1 = slotValue(s.compactSlot1, s.compactSlot1Scope);
    QString v2 = slotValue(s.compactSlot2, s.compactSlot2Scope);

    // Seleção no editor: o espaço que mostra palavras (o primeiro, se nenhum
    // mostrar) passa a contar só o trecho selecionado, até a seleção sumir.
    const auto isWords = [](const QString& m) {
        return m == QStringLiteral("words") || m == QStringLiteral("words-session");
    };
    const int selSlot = m_selWords <= 0 ? 0
        : isWords(s.compactSlot1) ? 1 : isWords(s.compactSlot2) ? 2 : 1;
    const QString selValue = loc.toString(m_selWords);
    QString title1 = m_slot1Title, title2 = m_slot2Title;
    if (selSlot == 1) { v1 = selValue; title1 = tr("Seleção"); }
    if (selSlot == 2) { v2 = selValue; title2 = tr("Seleção"); }

    int goalPct = 0;
    if (s.goalType == QStringLiteral("time")) {
        const qint64 tMs = m_counter->progressTimeMs();
        const int target = qMax(1, s.goalTargetMinutes);
        goalPct = static_cast<int>((tMs * 100.0) / (target * 60000.0));
    } else {
        const int w = m_counter->progressWords();
        const int target = qMax(1, s.goalTargetWords);
        goalPct = static_cast<int>((w * 100.0) / target);
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
        // Roteiro: 1 página no formato padrão ≈ 1 minuto de tela.
        if (m_counter->isScreenplay()) {
            const QString withMin = tr("Páginas · ~%1 min").arg(m_counter->estimatedPages());
            if (s.compactSlot1 == QStringLiteral("pages")) d.label1 = withMin;
            if (s.compactSlot2 == QStringLiteral("pages")) d.label2 = withMin;
        }
        d.value1 = v1;
        d.value2 = v2;
        d.fullLabel1 = title1;
        d.fullLabel2 = title2;
        d.showGoal = s.compactShowGoalBar;
        d.unit1 = unitShort(s.compactSlot1);
        d.unit2 = unitShort(s.compactSlot2);
        if (selSlot == 1) { d.label1 = title1; d.unit1 = tr("selecionadas"); }
        if (selSlot == 2) { d.label2 = title2; d.unit2 = tr("selecionadas"); }
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
            .arg(title1, v1, title2, v2));
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
        MiniCounterWidget::Slot a{ v1, unitFor(s.compactSlot1, false), unitFor(s.compactSlot1, true) };
        MiniCounterWidget::Slot b{ v2, unitFor(s.compactSlot2, false), unitFor(s.compactSlot2, true) };
        if (selSlot == 1) a = { v1, tr("selecionadas"), tr("selecionadas") };
        if (selSlot == 2) b = { v2, tr("selecionadas"), tr("selecionadas") };
        m_mini->setData(a, b, goalPct, s.compactShowGoalBar, week, weekGoal);

        QString tip = QStringLiteral("%1: %2\n%3: %4")
            .arg(title1, v1, title2, v2);
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

    // Aba Meta (modo full)
    const bool met = m_counter->isGoalMet();
    if (m_goalNumLabel) {
        QString num, rest;
        if (goalIsTime) {
            num = loc.toString(m_counter->progressTimeMs() / 60000);
            rest = tr("/ %1 minutos").arg(loc.toString(s.goalTargetMinutes));
        } else {
            num = loc.toString(m_counter->progressWords());
            rest = tr("/ %1 palavras").arg(loc.toString(s.goalTargetWords));
        }
        m_goalNumLabel->setText(QStringLiteral(
            "<span style='color:%1; font-size:14px; font-weight:600;'>%2</span>"
            " <span style='color:%3;'>%4</span>")
            .arg(hex(Theme::textBright()), num.toHtmlEscaped(), hex(Theme::textMuted()), rest.toHtmlEscaped()));
    }
    if (m_goalBar) m_goalBar->setValue(goalPct / 100.0, met);
    if (m_goalResetLine) {
        const QString rem = fmtDuration(m_counter->goalDayRemainingMs());
        if (met) {
            m_goalResetLine->setText(QStringLiteral("<span style='color:%1;'>%2</span>")
                .arg(hex(Theme::accentSuccess()), tr("meta batida").toHtmlEscaped()));
        } else {
            m_goalResetLine->setText(tr("reinicia em %1").arg(rem).toHtmlEscaped());
        }
        m_goalResetLine->setToolTip(tr("Reinicia em %1").arg(rem) + QStringLiteral("\n")
                                    + tr("Duplo clique para escolher o horário do reset"));
    }

    // Quatro números
    if (m_statNum[0]) {
        int activeDays = 0, wordsPerDay = 0, minutesPerDay = 0;
        m_counter->writingAverages(activeDays, wordsPerDay, minutesPerDay);
        m_statNum[0]->setText(loc.toString(m_counter->currentStreak()));
        m_statNum[1]->setText(loc.toString(m_counter->longestStreak()));
        m_statNum[2]->setText(loc.toString(wordsPerDay));
        m_statNum[3]->setText(tr("%1 min").arg(minutesPerDay));
    }

    // Últimos 14 dias (o dia da meta é a referência, como na Semana).
    if (m_strip) {
        QVector<CounterDayStrip::Day> days;
        for (int i = 13; i >= 0; --i) {
            const QDate d = goalDay.addDays(-i);
            const QString key = d.toString(QStringLiteral("yyyy-MM-dd"));
            const QJsonObject o = s.progress.value(key).toObject();
            CounterDayStrip::Day day;
            day.today = (i == 0);
            day.offDay = m_counter->offDayType(key) != WordCounter::OffDayType::None;
            double frac = 0;
            if (!o.isEmpty()) {
                const QString type = o.value(QStringLiteral("goalType")).toString(s.goalType);
                if (type == QStringLiteral("time")) {
                    const double tMs = o.value(QStringLiteral("timeMs")).toDouble(0);
                    const int tMin = o.value(QStringLiteral("goalTargetMinutes")).toInt(s.goalTargetMinutes);
                    frac = tMin > 0 ? tMs / (tMin * 60000.0) : 0;
                } else {
                    const int w = o.value(QStringLiteral("words")).toInt(0);
                    const int tw = o.value(QStringLiteral("goalTargetWords")).toInt(s.goalTargetWords);
                    frac = tw > 0 ? double(w) / tw : 0;
                }
            }
            day.fraction = frac;
            day.stars = qBound(0, int(frac), 5);
            QString what;
            if (day.offDay)       what = tr("Folga");
            else if (o.isEmpty()) what = tr("Nenhuma escrita");
            else                  what = tr("%1% da meta").arg(int(frac * 100));
            day.tooltip = (i == 0 ? tr("Hoje") : loc.toString(d, QStringLiteral("d MMM")))
                          + QStringLiteral(" · ") + what;
            days.append(day);
        }
        m_strip->setDays(days);
    }

    if (m_sprintActive) refreshSprintPage();

    setToolTip(tr("Clique para abrir a meta diária\nBotão direito para personalizar o contador"));
}

QWidget* WordCountPanel::buildSprintPage()
{
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(14, 12, 14, 13);
    lay->setSpacing(0);

    m_sprintBig = new QLabel(page);
    m_sprintBig->setObjectName(QStringLiteral("wcpSprintBig"));
    m_sprintBig->setAlignment(Qt::AlignCenter);
    lay->addSpacing(2);
    lay->addWidget(m_sprintBig);
    m_sprintCaption = new QLabel(page);
    m_sprintCaption->setObjectName(QStringLiteral("wcpMeta"));
    m_sprintCaption->setAlignment(Qt::AlignCenter);
    m_sprintCaption->setWordWrap(true);
    lay->addWidget(m_sprintCaption);
    lay->addSpacing(16);

    // Parado: duração e meta. A escolha fica salva (vale pra todos os projetos).
    QSettings qs;
    m_sprintSetup = new QWidget(page);
    auto* grid = new QGridLayout(m_sprintSetup);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(5);
    grid->addWidget(makeCap(tr("Duração"), m_sprintSetup), 0, 0);
    grid->addWidget(makeCap(tr("Meta"), m_sprintSetup), 0, 1);
    m_sprintMinutesSpin = new QSpinBox(m_sprintSetup);
    m_sprintMinutesSpin->setRange(1, 120);
    m_sprintMinutesSpin->setSingleStep(5);
    m_sprintMinutesSpin->setSuffix(tr(" min"));
    m_sprintMinutesSpin->setValue(qBound(1, qs.value(QStringLiteral("wordCounter/sprintMinutes"), 25).toInt(), 120));
    m_sprintWordsSpin = new QSpinBox(m_sprintSetup);
    m_sprintWordsSpin->setRange(0, 10000);
    m_sprintWordsSpin->setSingleStep(50);
    m_sprintWordsSpin->setValue(qBound(0, qs.value(QStringLiteral("wordCounter/sprintWords"), 300).toInt(), 10000));
    grid->addWidget(makeStepper(m_sprintMinutesSpin, m_sprintSetup), 1, 0);
    grid->addWidget(makeStepper(m_sprintWordsSpin, m_sprintSetup), 1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    connect(m_sprintMinutesSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        QSettings().setValue(QStringLiteral("wordCounter/sprintMinutes"), v);
        m_sprintEnded = false;
        refreshSprintPage();
    });
    connect(m_sprintWordsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        QSettings().setValue(QStringLiteral("wordCounter/sprintWords"), v);
        m_sprintEnded = false;
        refreshSprintPage();
    });
    lay->addWidget(m_sprintSetup);

    // Correndo: palavras do sprint e a barra.
    m_sprintRun = new QWidget(page);
    auto* run = new QVBoxLayout(m_sprintRun);
    run->setContentsMargins(0, 0, 0, 0);
    run->setSpacing(6);
    m_sprintWordsLabel = new QLabel(m_sprintRun);
    m_sprintWordsLabel->setObjectName(QStringLiteral("wcpGoalNum"));
    m_sprintWordsLabel->setTextFormat(Qt::RichText);
    run->addWidget(m_sprintWordsLabel);
    m_sprintBar = new CounterThinBar(m_sprintRun);
    run->addWidget(m_sprintBar);
    m_sprintRun->setVisible(false);
    lay->addWidget(m_sprintRun);

    lay->addStretch(1);
    m_sprintActionBtn = new QPushButton(page);
    m_sprintActionBtn->setCursor(Qt::PointingHandCursor);
    m_sprintActionBtn->setIconSize(QSize(12, 12));
    connect(m_sprintActionBtn, &QPushButton::clicked, this, [this]() {
        if (m_sprintActive) stopSprint(); else startSprint();
    });
    lay->addWidget(m_sprintActionBtn);

    // Timer interno (1 segundo)
    m_sprintTimer = new QTimer(this);
    m_sprintTimer->setInterval(1000);
    connect(m_sprintTimer, &QTimer::timeout, this, &WordCountPanel::tickSprint);

    return page;
}

void WordCountPanel::refreshSprintPage()
{
    if (!m_sprintBig || !m_sprintActionBtn) return;
    const QLocale loc = statsLocale();
    const int goal = m_sprintWordsSpin ? m_sprintWordsSpin->value() : 300;
    const int minutes = m_sprintMinutesSpin ? m_sprintMinutesSpin->value() : 25;
    auto mmss = [](int secs) {
        secs = qMax(0, secs);
        return QStringLiteral("%1:%2").arg(secs / 60, 2, 10, QLatin1Char('0'))
                                      .arg(secs % 60, 2, 10, QLatin1Char('0'));
    };

    if (m_sprintActive) {
        const int written = m_counter
            ? qMax(0, m_counter->sessionWordsManuscript() - m_sprintStartSession) : 0;
        m_sprintBig->setText(mmss(m_sprintSecondsLeft));
        m_sprintCaption->setText(tr("restam de %1 min").arg(minutes));
        m_sprintWordsLabel->setText(QStringLiteral(
            "<span style='color:%1; font-size:14px; font-weight:600;'>%2</span>"
            " <span style='color:%3;'>%4</span>")
            .arg(hex(Theme::textBright()), loc.toString(written), hex(Theme::textMuted()),
                 tr("/ %1 palavras").arg(loc.toString(goal)).toHtmlEscaped()));
        m_sprintBar->setValue(goal > 0 ? double(written) / goal : 1.0, written >= goal);
    } else if (m_sprintEnded) {
        m_sprintBig->setText(loc.toString(m_sprintWritten));
        m_sprintCaption->setText(m_sprintWritten >= goal ? tr("palavras no sprint · meta batida")
                                                         : tr("palavras no sprint"));
    } else {
        m_sprintBig->setText(mmss(minutes * 60));
        m_sprintCaption->setText(tr("escreva sem parar até o tempo acabar"));
    }
    if (m_sprintSetup) m_sprintSetup->setVisible(!m_sprintActive);
    if (m_sprintRun) m_sprintRun->setVisible(m_sprintActive);

    const QString objName = m_sprintActive ? QStringLiteral("wcpSoftBtn") : QStringLiteral("wcpPrimaryBtn");
    m_sprintActionBtn->setText(m_sprintActive ? tr("Encerrar sprint") : tr("Iniciar sprint"));
    m_sprintActionBtn->setIcon(glyphIcon(!m_sprintActive,
        m_sprintActive ? Theme::toColor(Theme::textPrimary()) : QColor(Qt::white)));
    if (m_sprintActionBtn->objectName() != objName) {
        m_sprintActionBtn->setObjectName(objName);
        m_sprintActionBtn->style()->unpolish(m_sprintActionBtn);
        m_sprintActionBtn->style()->polish(m_sprintActionBtn);
    }

    if (m_tabs) {
        const bool pulse = m_sprintActive && m_stack && m_stack->currentIndex() != PageSprint;
        m_tabs->setPulse(pulse ? int(PageSprint) : -1);
    }
}

void WordCountPanel::startSprint()
{
    if (!m_counter || m_sprintActive) return;
    m_sprintActive = true;
    m_sprintEnded = false;
    m_sprintSecondsLeft = (m_sprintMinutesSpin ? m_sprintMinutesSpin->value() : 25) * 60;
    m_sprintStartSession = m_counter->sessionWordsManuscript();
    m_sprintTimer->start();
    refreshSprintPage();
}

void WordCountPanel::stopSprint()
{
    if (!m_sprintActive) return;
    m_sprintTimer->stop();
    m_sprintActive = false;
    m_sprintWritten = m_counter
        ? qMax(0, m_counter->sessionWordsManuscript() - m_sprintStartSession)
        : 0;
    m_sprintEnded = true;
    refreshSprintPage();
}

void WordCountPanel::tickSprint()
{
    if (!m_sprintActive) return;
    --m_sprintSecondsLeft;
    if (m_sprintSecondsLeft <= 0) {
        m_sprintSecondsLeft = 0;
        stopSprint();
        return;
    }
    refreshSprintPage();
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
