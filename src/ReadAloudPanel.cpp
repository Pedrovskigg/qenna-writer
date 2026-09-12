#include "ReadAloudPanel.h"
#include "AnchorUtils.h"

#include "ReadAloudController.h"
#include "Theme.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QSettings>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kPanelWidth = 300;
constexpr int kGapBelowAnchor = 6;

// O slider anda em passos de 0,05x pra caber o ajuste fino que revisão pede —
// a diferença entre 1,0x e 1,1x é audível quando se ouve um capítulo inteiro.
constexpr int kRateMin = 10;   // 0,5x
constexpr int kRateMax = 40;   // 2,0x
constexpr int kRateStep = 1;   // 0,05x

double sliderToMultiplier(int v) { return v * 0.05; }
int multiplierToSlider(double m) { return int(qRound(m / 0.05)); }

// Qt trabalha em -1..1 com 0 = velocidade normal. A conversão é linear nos
// dois lados do 0 pra que 1,0x caia exatamente no meio.
double multiplierToQtRate(double m)
{
    if (m >= 1.0) return qBound(0.0, m - 1.0, 1.0);
    return qBound(-1.0, (m - 1.0) / 0.5, 0.0);
}
} // namespace

ReadAloudPanel::ReadAloudPanel(ReadAloudController* controller, QWidget* parent)
    : QFrame(parent)
    , m_ctl(controller)
{
    setObjectName(QStringLiteral("readAloudPanel"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    // Sem roubar foco: o autor tem que poder corrigir o texto com o painel
    // aberto, que é o ponto inteiro de ouvir pra revisar.
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);

    buildUi();
    applyTheme();

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &ReadAloudPanel::applyTheme);

    if (m_ctl) {
        connect(m_ctl, &ReadAloudController::stateChanged, this,
                [this](bool speaking, bool paused) {
            if (m_playBtn) {
                const bool showPlay = !speaking || paused;
                m_playBtn->setText(showPlay ? QStringLiteral("▶") : QStringLiteral("❚❚"));
                m_playBtn->setToolTip(showPlay ? tr("Continuar") : tr("Pausar"));
            }
            if (m_stopBtn) m_stopBtn->setEnabled(speaking);
        });
    }

    hide();
}

void ReadAloudPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 12);
    root->setSpacing(8);

    auto* header = new QLabel(tr("Ler em voz alta"), this);
    header->setObjectName(QStringLiteral("raHeader"));
    root->addWidget(header);

    auto* controls = new QHBoxLayout();
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(8);

    m_playBtn = new QToolButton(this);
    m_playBtn->setObjectName(QStringLiteral("raPlayBtn"));
    m_playBtn->setText(QStringLiteral("❚❚"));
    m_playBtn->setToolTip(tr("Pausar"));
    m_playBtn->setCursor(Qt::PointingHandCursor);
    m_playBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_playBtn, &QToolButton::clicked, this, &ReadAloudPanel::playPauseRequested);
    controls->addWidget(m_playBtn);

    m_stopBtn = new QToolButton(this);
    m_stopBtn->setObjectName(QStringLiteral("raStopBtn"));
    m_stopBtn->setText(QStringLiteral("■"));
    m_stopBtn->setToolTip(tr("Parar"));
    m_stopBtn->setCursor(Qt::PointingHandCursor);
    m_stopBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_stopBtn, &QToolButton::clicked, this, &ReadAloudPanel::stopRequested);
    controls->addWidget(m_stopBtn);

    m_restartBtn = new QToolButton(this);
    m_restartBtn->setObjectName(QStringLiteral("raRestartBtn"));
    m_restartBtn->setText(QStringLiteral("↻"));
    m_restartBtn->setToolTip(tr("Ler de novo a partir do cursor"));
    m_restartBtn->setCursor(Qt::PointingHandCursor);
    m_restartBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_restartBtn, &QToolButton::clicked, this, &ReadAloudPanel::restartRequested);
    controls->addWidget(m_restartBtn);

    controls->addStretch(1);
    root->addLayout(controls);

    // Velocidade
    auto* rateRow = new QHBoxLayout();
    rateRow->setContentsMargins(0, 0, 0, 0);
    rateRow->setSpacing(8);

    auto* rateCaption = new QLabel(tr("Velocidade"), this);
    rateCaption->setObjectName(QStringLiteral("raCaption"));
    rateRow->addWidget(rateCaption);

    m_rate = new QSlider(Qt::Horizontal, this);
    m_rate->setObjectName(QStringLiteral("raRate"));
    m_rate->setRange(kRateMin, kRateMax);
    m_rate->setSingleStep(kRateStep);
    m_rate->setPageStep(2);
    m_rate->setFocusPolicy(Qt::NoFocus);
    rateRow->addWidget(m_rate, 1);

    m_rateLabel = new QLabel(this);
    m_rateLabel->setObjectName(QStringLiteral("raRateLabel"));
    m_rateLabel->setFixedWidth(40);
    m_rateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rateRow->addWidget(m_rateLabel);

    root->addLayout(rateRow);

    connect(m_rate, &QSlider::valueChanged, this, [this](int v) {
        const double mult = sliderToMultiplier(v);
        if (m_rateLabel)
            m_rateLabel->setText(QLocale().toString(mult, 'f', 2) + QStringLiteral("x"));
        if (m_syncing) return;
        if (m_ctl) m_ctl->setRate(multiplierToQtRate(mult));
        QSettings().setValue(QStringLiteral("readAloud/rate"), mult);
    });

    // Voz
    m_voice = new QComboBox(this);
    m_voice->setObjectName(QStringLiteral("raVoice"));
    m_voice->setFocusPolicy(Qt::NoFocus);
    root->addWidget(m_voice);
    connect(m_voice, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (m_syncing || !m_ctl || idx < 0) return;
        m_ctl->setVoiceIndex(idx);
        QSettings().setValue(QStringLiteral("readAloud/voice"), m_voice->itemText(idx));
    });

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("raStatus"));
    m_status->setWordWrap(true);
    m_status->hide();
    root->addWidget(m_status);

    setFixedWidth(kPanelWidth);
}

void ReadAloudPanel::refreshFromController()
{
    if (!m_ctl) return;
    m_syncing = true;

    const double mult = QSettings().value(QStringLiteral("readAloud/rate"), 1.0).toDouble();
    if (m_rate) m_rate->setValue(qBound(kRateMin, multiplierToSlider(mult), kRateMax));
    m_ctl->setRate(multiplierToQtRate(mult));

    if (m_voice) {
        m_voice->clear();
        const QStringList voices = m_ctl->voiceNames();
        m_voice->addItems(voices);
        // A voz salva pode ter sumido (usuário desinstalou o pacote de idioma,
        // ou trocou o idioma do projeto) — nesse caso fica a que o motor já
        // escolheu, em vez de zerar a seleção.
        const QString saved = QSettings().value(QStringLiteral("readAloud/voice")).toString();
        int idx = saved.isEmpty() ? -1 : voices.indexOf(saved);
        if (idx < 0) idx = m_ctl->currentVoiceIndex();
        if (idx >= 0) {
            m_voice->setCurrentIndex(idx);
            m_ctl->setVoiceIndex(idx);
        }
        m_voice->setEnabled(!voices.isEmpty());
        if (voices.isEmpty()) m_voice->addItem(tr("(nenhuma voz instalada)"));
    }

    m_syncing = false;
}

void ReadAloudPanel::setStatusText(const QString& text)
{
    if (!m_status) return;
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
    adjustSize();
}

void ReadAloudPanel::showNear(const QRect& anchorGlobal, Qt::Edge barSide)
{
    refreshFromController();
    adjustSize();
    const QSize ps = size();
    QPoint pos(anchorGlobal.left(), anchorGlobal.bottom() + kGapBelowAnchor);
    const QScreen* screen = QGuiApplication::screenAt(anchorGlobal.center());
    if (screen) {
        pos = AnchorUtils::positionNear(anchorGlobal, ps, barSide,
                                        screen->availableGeometry(), kGapBelowAnchor);
    }
    move(pos);
    show();
    raise();
}

void ReadAloudPanel::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QFrame#readAloudPanel {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
        "QLabel#raHeader { color: %3; font-size: 13px; font-weight: 600; }"
        "QLabel#raCaption { color: %4; font-size: 11px; }"
        "QLabel#raRateLabel { color: %3; font-size: 11px; }"
        "QLabel#raStatus { color: %4; font-size: 10px; }"
        "QToolButton#raPlayBtn, QToolButton#raStopBtn, QToolButton#raRestartBtn {"
        "  background: transparent;"
        "  color: %3;"
        "  border: 1px solid %2;"
        "  border-radius: 16px;"
        "  min-width: 32px; max-width: 32px;"
        "  min-height: 32px; max-height: 32px;"
        "  font-size: 13px;"
        "}"
        "QToolButton#raPlayBtn:hover, QToolButton#raStopBtn:hover,"
        "QToolButton#raRestartBtn:hover { background: %5; }"
        "QToolButton#raStopBtn:disabled { color: %6; border-color: %6; }"
        "QComboBox#raVoice {"
        "  background: %7;"
        "  color: %3;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  font-size: 11px;"
        "}"
        "QComboBox#raVoice::drop-down { border: none; width: 18px; }"
        "QComboBox#raVoice QAbstractItemView {"
        "  background: %7; color: %3; border: 1px solid %2;"
        "  selection-background-color: %8;"
        "}"
        "QSlider#raRate::groove:horizontal {"
        "  background: %2; height: 4px; border-radius: 2px;"
        "}"
        "QSlider#raRate::sub-page:horizontal { background: %8; border-radius: 2px; }"
        "QSlider#raRate::handle:horizontal {"
        "  background: %3; width: 12px; margin: -5px 0; border-radius: 6px;"
        "}"
    ).arg(Theme::panelBackground(),
          Theme::panelBorder(),
          Theme::textPrimary(),
          Theme::textMuted(),
          Theme::hoverOverlay(),
          Theme::disabledText(),
          Theme::inputBackground(),
          Theme::accentDefault()));

    // QComboBox é QAbstractScrollArea: a view dele não herda o background do
    // pai por stylesheet em todos os estilos. Ver o histórico de viewport
    // transparente que já mordeu StatsPanel e AIChatPanel.
    if (m_voice && m_voice->view()) {
        m_voice->view()->setStyleSheet(
            QStringLiteral("background: %1; color: %2;")
                .arg(Theme::inputBackground(), Theme::textPrimary()));
    }
}
