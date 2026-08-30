#include "ReaderPreviewPanel.h"
#include "ReaderPreviewDeviceView.h"
#include "ProjectModel.h"
#include "Theme.h"
#include "IconUtils.h"

#include <QFont>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QSlider>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kSliderResolution = 1000;
const QColor kLightBg(0xf5, 0xf0, 0xe6);
const QColor kLightFg(0x1a, 0x1a, 0x1a);
const QColor kDarkBg(0x1c, 0x1c, 0x1c);
const QColor kDarkFg(0xd8, 0xd8, 0xd8);

QString lastPageKey(const QString& manuscriptId) {
    return QStringLiteral("readerPreview/lastPage/%1").arg(manuscriptId);
}
} // namespace

ReaderPreviewPanel::ReaderPreviewPanel(ProjectModel* model, const QString& projectRoot,
                                        const Exporter::DocStyle& style, QWidget* parent)
    : QWidget(parent, Qt::Window), m_model(model), m_exporter(model, projectRoot, style) {
    setWindowTitle(tr("Preview de e-reader"));
    resize(520, 760);
    buildUi();
    loadSettings();
}

ReaderPreviewPanel::~ReaderPreviewPanel() {
    delete m_deviceDoc;
}

void ReaderPreviewPanel::buildUi() {
    setObjectName(QStringLiteral("readerPreviewPanel"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 10);
    root->setSpacing(6);

    auto* chrome = new QHBoxLayout();
    m_titleLabel = new QLabel(this);
    QFont tf = m_titleLabel->font();
    tf.setBold(true);
    m_titleLabel->setFont(tf);
    chrome->addWidget(m_titleLabel, 1);

    m_darkModeBtn = new QToolButton(this);
    m_darkModeBtn->setCheckable(true);
    m_darkModeBtn->setToolTip(tr("Modo escuro"));
    m_darkModeBtn->setCursor(Qt::PointingHandCursor);
    chrome->addWidget(m_darkModeBtn);

    m_grayscaleBtn = new QToolButton(this);
    m_grayscaleBtn->setCheckable(true);
    m_grayscaleBtn->setToolTip(tr("Preto e branco"));
    m_grayscaleBtn->setCursor(Qt::PointingHandCursor);
    chrome->addWidget(m_grayscaleBtn);

    m_helpBtn = new QToolButton(this);
    m_helpBtn->setText(QStringLiteral("?"));
    QFont helpFont = m_helpBtn->font();
    helpFont.setBold(true);
    m_helpBtn->setFont(helpFont);
    m_helpBtn->setToolTip(tr("Sobre este preview"));
    m_helpBtn->setCursor(Qt::PointingHandCursor);
    connect(m_helpBtn, &QToolButton::clicked, this, [this]() {
        QMessageBox::information(this, tr("Preview de e-reader"),
            tr("Nessa área, você consegue ter um preview de como o seu projeto ficará em um "
               "e-reader (Kindle, Kobo e etc).\n\n"
               "Não é algo 100% preciso, mas dá para se ter uma ideia. Caso queira realmente o "
               "e-pub do seu livro, você pode criá-lo e salvá-lo facilmente na área de "
               "Exportação."));
    });
    chrome->addWidget(m_helpBtn);

    m_closeBtn = new QToolButton(this);
    m_closeBtn->setToolTip(tr("Fechar"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_closeBtn, &QToolButton::clicked, this, &QWidget::hide);
    chrome->addWidget(m_closeBtn);

    root->addLayout(chrome);

    m_deviceView = new ReaderPreviewDeviceView(this);
    root->addWidget(m_deviceView, 1);

    m_positionSlider = new QSlider(Qt::Horizontal, this);
    m_positionSlider->setRange(0, kSliderResolution);
    root->addWidget(m_positionSlider);

    connect(m_darkModeBtn, &QToolButton::toggled, this, [this](bool on) {
        m_darkMode = on;
        QSettings().setValue(QStringLiteral("readerPreview/darkMode"), on);
        rebuildDocuments();
    });
    connect(m_grayscaleBtn, &QToolButton::toggled, this, [this](bool on) {
        m_grayscale = on;
        QSettings().setValue(QStringLiteral("readerPreview/grayscale"), on);
        rebuildDocuments();
    });

    connect(m_deviceView, &ReaderPreviewDeviceView::positionChanged, this, [this](double frac) {
        if (m_syncingSlider) return;
        m_positionFraction = frac;
        m_syncingSlider = true;
        m_positionSlider->setValue(qRound(frac * kSliderResolution));
        m_syncingSlider = false;
    });
    connect(m_positionSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_syncingSlider) return;
        applyPositionFraction(double(v) / kSliderResolution);
    });

    updateChromeStyle();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, &ReaderPreviewPanel::updateChromeStyle);
}

// Ícones e cores da barra de chrome seguem a paleta do TEMA DO APP (não o
// toggle de escuro/claro do conteúdo do e-reader, que é independente) — mesmo
// esquema do #topToolbar QToolButton (Theme.cpp), pra nunca ficar ilegível
// dependendo do tema ativo.
void ReaderPreviewPanel::updateChromeStyle() {
    const QColor normal(Theme::textMuted());
    const QColor hoverColor(Theme::textBright());
    const QColor checkedColor(Theme::textBright());
    const QSize iconSize(18, 18);

    m_darkModeBtn->setIcon(IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/reader-dark-mode.svg"), normal, hoverColor, checkedColor, iconSize));
    m_grayscaleBtn->setIcon(IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/reader-grayscale.svg"), normal, hoverColor, checkedColor, iconSize));
    m_closeBtn->setIcon(IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/close.svg"), normal, hoverColor, hoverColor, iconSize));

    setStyleSheet(QStringLiteral(R"(
        #readerPreviewPanel QToolButton {
            background: transparent;
            color: %1;
            border: none;
            padding: 6px;
            border-radius: 6px;
        }
        #readerPreviewPanel QToolButton:hover {
            color: %2;
            background-color: %3;
        }
        #readerPreviewPanel QToolButton:checked {
            color: %2;
            background-color: %4;
        }
    )").arg(Theme::textMuted(), Theme::textBright(), Theme::hoverOverlay(), Theme::hoverStrong()));
}

void ReaderPreviewPanel::loadSettings() {
    QSettings settings;
    m_darkMode = settings.value(QStringLiteral("readerPreview/darkMode"), false).toBool();
    m_grayscale = settings.value(QStringLiteral("readerPreview/grayscale"), false).toBool();
    m_darkModeBtn->setChecked(m_darkMode);
    m_grayscaleBtn->setChecked(m_grayscale);
}

void ReaderPreviewPanel::setManuscript(const QString& manuscriptId) {
    if (manuscriptId.isEmpty()) return;
    if (m_manuscriptId != manuscriptId) {
        savePositionForCurrentManuscript();
        m_manuscriptId = manuscriptId;
        QSettings settings;
        m_positionFraction = settings.value(lastPageKey(manuscriptId), 0.0).toDouble();
    }
    updateTitleLabel();
    rebuildDocuments();
    // A paginação só fica correta depois que o widget tem um tamanho real
    // (viewport width > 0) — na primeira exibição ainda não tem.
    QTimer::singleShot(0, this, [this]() { applyPositionFraction(m_positionFraction); });
}

void ReaderPreviewPanel::updateTitleLabel() {
    const QString title = m_model ? m_model->manuscriptEffectiveTitle(m_manuscriptId) : QString();
    const QString shown = title.isEmpty() ? tr("Preview de e-reader") : title;
    m_titleLabel->setText(shown);
    setWindowTitle(shown);
}

void ReaderPreviewPanel::rebuildDocuments() {
    if (m_manuscriptId.isEmpty()) return;

    const QColor bg = m_darkMode ? kDarkBg : kLightBg;
    const QColor fg = m_darkMode ? kDarkFg : kLightFg;

    QTextDocument* newDeviceDoc = m_exporter.buildPreviewDocument(m_manuscriptId, true, fg, bg, m_grayscale, this);

    if (!newDeviceDoc) {
        delete m_deviceDoc;
        m_deviceDoc = nullptr;
        m_deviceView->setDocument(nullptr);
        return;
    }

    // ReaderPreviewDeviceView::setDocument() não é dono do documento anterior
    // — repontar pro novo primeiro, só então liberar o antigo.
    QTextDocument* oldDeviceDoc = m_deviceDoc;
    m_deviceDoc = newDeviceDoc;

    m_deviceView->setTextColor(fg);
    m_deviceView->setBackgroundColor(bg);
    m_deviceView->setDocument(m_deviceDoc);

    delete oldDeviceDoc;

    applyPositionFraction(m_positionFraction);
}

void ReaderPreviewPanel::applyPositionFraction(double frac) {
    m_positionFraction = qBound(0.0, frac, 1.0);
    m_syncingSlider = true;
    m_positionSlider->setValue(qRound(m_positionFraction * kSliderResolution));
    m_deviceView->setPositionFraction(m_positionFraction);
    m_syncingSlider = false;
}

void ReaderPreviewPanel::savePositionForCurrentManuscript() {
    if (m_manuscriptId.isEmpty()) return;
    QSettings().setValue(lastPageKey(m_manuscriptId), m_positionFraction);
}

void ReaderPreviewPanel::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    savePositionForCurrentManuscript();
}
