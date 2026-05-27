#include "SettingsPanel.h"

#include "EditorLayout.h"
#include "Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

SettingsPanel::SettingsPanel(QWidget* parent)
    : QDialog(parent)
    , m_spellCheck(new QCheckBox(tr("Ativar corretor ortográfico"), this))
    , m_langCombo(new QComboBox(this))
{
    setObjectName(QStringLiteral("settingsPanel"));
    setWindowTitle(tr("Configurações"));
    setModal(false);
    resize(420, 360);

    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &SettingsPanel::applyTheme);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    auto* title = new QLabel(tr("Configurações"), this);
    title->setObjectName(QStringLiteral("settingsTitle"));
    root->addWidget(title);

    // ---- Seção: Corretor ortográfico ----
    auto* spellGroup = new QGroupBox(tr("Corretor ortográfico"), this);
    auto* spellLayout = new QVBoxLayout(spellGroup);
    spellLayout->setContentsMargins(14, 8, 14, 14);
    spellLayout->setSpacing(8);

    spellLayout->addWidget(m_spellCheck);

    auto* langRow = new QVBoxLayout;
    langRow->setSpacing(4);
    auto* langLabel = new QLabel(tr("Idioma do dicionário:"), spellGroup);
    langRow->addWidget(langLabel);
    langRow->addWidget(m_langCombo);
    spellLayout->addLayout(langRow);

    m_spellHint = new QLabel(
        tr("Palavras desconhecidas ganham um sublinhado vermelho. "
           "Clique com o botão direito numa delas para ver sugestões "
           "ou adicionar ao dicionário do projeto."),
        spellGroup);
    m_spellHint->setObjectName(QStringLiteral("settingsHint"));
    m_spellHint->setWordWrap(true);
    spellLayout->addWidget(m_spellHint);

    root->addWidget(spellGroup);

    // ---- Seção: Página de escrita ----
    auto* pageGroup = new QGroupBox(tr("Página de escrita"), this);
    auto* pageLayout = new QVBoxLayout(pageGroup);
    pageLayout->setContentsMargins(14, 8, 14, 14);
    pageLayout->setSpacing(10);

    auto makeSlider = [pageGroup](int lo, int hi, int step) {
        auto* s = new QSlider(Qt::Horizontal, pageGroup);
        s->setRange(lo, hi);
        s->setSingleStep(step);
        s->setPageStep(step * 4);
        s->setMinimumWidth(180);
        return s;
    };
    auto makeValueLabel = [pageGroup]() {
        auto* l = new QLabel(pageGroup);
        l->setObjectName(QStringLiteral("pageValueLabel"));
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return l;
    };

    m_pageWidthSlider = makeSlider(EditorLayout::Manager::minPageWidth(),
                                   EditorLayout::Manager::maxPageWidth(), 20);
    m_pageHeightSlider = makeSlider(EditorLayout::Manager::minPageHeight(),
                                    EditorLayout::Manager::maxPageHeight(), 20);
    m_hMarginSlider = makeSlider(EditorLayout::Manager::minHorizontalMargin(),
                                 EditorLayout::Manager::maxHorizontalMargin(), 2);
    m_vMarginSlider = makeSlider(EditorLayout::Manager::minVerticalMargin(),
                                 EditorLayout::Manager::maxVerticalMargin(), 2);
    m_pageWidthValue = makeValueLabel();
    m_pageHeightValue = makeValueLabel();
    m_hMarginValue = makeValueLabel();
    m_vMarginValue = makeValueLabel();

    auto* grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(1, 1);

    grid->addWidget(new QLabel(tr("Largura da página"), pageGroup),       0, 0);
    grid->addWidget(m_pageWidthSlider,                                    0, 1);
    grid->addWidget(m_pageWidthValue,                                     0, 2);

    grid->addWidget(new QLabel(tr("Comprimento da página"), pageGroup),   1, 0);
    grid->addWidget(m_pageHeightSlider,                                   1, 1);
    grid->addWidget(m_pageHeightValue,                                    1, 2);

    grid->addWidget(new QLabel(tr("Margem lateral"), pageGroup),          2, 0);
    grid->addWidget(m_hMarginSlider,                                      2, 1);
    grid->addWidget(m_hMarginValue,                                       2, 2);

    grid->addWidget(new QLabel(tr("Margem topo/base"), pageGroup),        3, 0);
    grid->addWidget(m_vMarginSlider,                                      3, 1);
    grid->addWidget(m_vMarginValue,                                       3, 2);

    pageLayout->addLayout(grid);

    m_pageHint = new QLabel(
        tr("Define o tamanho da \"folha\" e o respiro interno entre a borda "
           "e o texto. O comprimento em \"Automático\" preenche a tela; com "
           "um valor fixo, a folha ganha altura definida e o fundo aparece "
           "em volta. Vale para todos os projetos."),
        pageGroup);
    m_pageHint->setObjectName(QStringLiteral("settingsHint"));
    m_pageHint->setWordWrap(true);
    pageLayout->addWidget(m_pageHint);

    root->addWidget(pageGroup);

    syncPageLayoutFromManager();

    root->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    root->addWidget(buttons);

    connect(m_spellCheck, &QCheckBox::toggled, this, &SettingsPanel::onCheckToggled);
    connect(m_langCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SettingsPanel::onLanguageChanged);

    // Layout da página — escreve direto no manager; ele emite layoutChanged()
    // e a MainWindow reage. Sem signals locais.
    auto* layoutMgr = EditorLayout::Manager::instance();
    connect(m_pageWidthSlider, &QSlider::valueChanged, this,
            [this, layoutMgr](int v) {
                m_pageWidthValue->setText(QStringLiteral("%1 px").arg(v));
                if (m_blockLayoutSignals) return;
                layoutMgr->setPageWidth(v);
            });
    connect(m_pageHeightSlider, &QSlider::valueChanged, this,
            [this, layoutMgr](int v) {
                m_pageHeightValue->setText(v <= 0 ? tr("Automático")
                                                  : QStringLiteral("%1 px").arg(v));
                if (m_blockLayoutSignals) return;
                layoutMgr->setPageHeight(v);
            });
    connect(m_hMarginSlider, &QSlider::valueChanged, this,
            [this, layoutMgr](int v) {
                m_hMarginValue->setText(QStringLiteral("%1 px").arg(v));
                if (m_blockLayoutSignals) return;
                layoutMgr->setHorizontalMargin(v);
            });
    connect(m_vMarginSlider, &QSlider::valueChanged, this,
            [this, layoutMgr](int v) {
                m_vMarginValue->setText(QStringLiteral("%1 px").arg(v));
                if (m_blockLayoutSignals) return;
                layoutMgr->setVerticalMargin(v);
            });
    // Sincronia reversa: se outro lugar mudar o layout, o slider reflete.
    connect(layoutMgr, &EditorLayout::Manager::layoutChanged,
            this, &SettingsPanel::syncPageLayoutFromManager);
}

void SettingsPanel::syncPageLayoutFromManager()
{
    if (!m_pageWidthSlider) return;
    m_blockLayoutSignals = true;
    auto* mgr = EditorLayout::Manager::instance();
    m_pageWidthSlider->setValue(mgr->pageWidth());
    m_pageHeightSlider->setValue(mgr->pageHeight());
    m_hMarginSlider->setValue(mgr->horizontalMargin());
    m_vMarginSlider->setValue(mgr->verticalMargin());
    m_pageWidthValue->setText(QStringLiteral("%1 px").arg(mgr->pageWidth()));
    m_pageHeightValue->setText(mgr->pageHeight() <= 0 ? tr("Automático")
                               : QStringLiteral("%1 px").arg(mgr->pageHeight()));
    m_hMarginValue->setText(QStringLiteral("%1 px").arg(mgr->horizontalMargin()));
    m_vMarginValue->setText(QStringLiteral("%1 px").arg(mgr->verticalMargin()));
    m_blockLayoutSignals = false;
}

void SettingsPanel::setAvailableSpellLanguages(const QList<QPair<QString, QString>>& langs)
{
    m_blockSignals = true;
    const QString prev = spellLanguage();
    m_langCombo->clear();
    for (const auto& pair : langs) {
        m_langCombo->addItem(pair.second, pair.first);
    }
    if (!prev.isEmpty()) {
        const int idx = m_langCombo->findData(prev);
        if (idx >= 0) m_langCombo->setCurrentIndex(idx);
    }
    m_blockSignals = false;
}

void SettingsPanel::setSpellEnabled(bool enabled)
{
    m_blockSignals = true;
    m_spellCheck->setChecked(enabled);
    m_langCombo->setEnabled(enabled);
    m_blockSignals = false;
}

void SettingsPanel::setSpellLanguage(const QString& code)
{
    m_blockSignals = true;
    const int idx = m_langCombo->findData(code);
    if (idx >= 0) m_langCombo->setCurrentIndex(idx);
    m_blockSignals = false;
}

bool SettingsPanel::spellEnabled() const
{
    return m_spellCheck->isChecked();
}

QString SettingsPanel::spellLanguage() const
{
    return m_langCombo->currentData().toString();
}

void SettingsPanel::onCheckToggled(bool checked)
{
    m_langCombo->setEnabled(checked);
    if (m_blockSignals) return;
    emit spellEnabledChanged(checked);
}

void SettingsPanel::onLanguageChanged(int /*index*/)
{
    if (m_blockSignals) return;
    emit spellLanguageChanged(spellLanguage());
}

void SettingsPanel::applyTheme()
{
    const QString panelBg    = Theme::panelBackground();
    const QString panelBd    = Theme::panelBorder();
    const QString txtPrim    = Theme::textPrimary();
    const QString txtMuted   = Theme::textMuted();
    const QString txtBright  = Theme::textBright();
    const QString inputBg    = Theme::inputBackground();
    const QString subtleBd   = Theme::subtleBorder();
    const QString hover      = Theme::hoverOverlay();
    const QString hoverStr   = Theme::hoverStrong();
    const QString accent     = Theme::accentDefault();

    setStyleSheet(QStringLiteral(R"(
        #settingsPanel {
            background-color: %1;
        }
        #settingsPanel QLabel {
            color: %4;
            font-size: 12px;
        }
        #settingsPanel QLabel#settingsTitle {
            color: %6;
            font-size: 18px;
            font-weight: bold;
            padding-bottom: 4px;
        }
        #settingsPanel QLabel#settingsHint {
            color: %5;
            font-size: 11px;
            font-weight: normal;
        }
        #settingsPanel QGroupBox {
            color: %4;
            border: 1px solid %2;
            border-radius: 8px;
            margin-top: 14px;
            padding-top: 14px;
            font-size: 12px;
            font-weight: bold;
        }
        #settingsPanel QGroupBox::title {
            subcontrol-origin: margin;
            left: 14px;
            padding: 0 6px;
        }
        #settingsPanel QCheckBox {
            color: %4;
            spacing: 8px;
            font-size: 12px;
            font-weight: normal;
        }
        #settingsPanel QCheckBox::indicator {
            width: 14px;
            height: 14px;
            border-radius: 3px;
            border: 1px solid %7;
            background: %3;
        }
        #settingsPanel QCheckBox::indicator:checked {
            background: %4;
            border-color: %4;
        }
        #settingsPanel QComboBox {
            background: %3;
            color: %6;
            border: 1px solid %2;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 12px;
            font-weight: normal;
            min-height: 22px;
        }
        #settingsPanel QComboBox:hover {
            border-color: %9;
        }
        #settingsPanel QComboBox QAbstractItemView {
            background: %1;
            color: %4;
            border: 1px solid %2;
            selection-background-color: %8;
            selection-color: %6;
        }
        #settingsPanel QPushButton {
            background: %8;
            color: %4;
            border: none;
            padding: 6px 18px;
            border-radius: 4px;
            font-size: 12px;
        }
        #settingsPanel QPushButton:hover {
            background: %9;
            color: %6;
        }
        #settingsPanel QSlider::groove:horizontal {
            background: %3;
            height: 4px;
            border-radius: 2px;
        }
        #settingsPanel QSlider::sub-page:horizontal {
            background: %10;
            border-radius: 2px;
        }
        #settingsPanel QSlider::handle:horizontal {
            background: %4;
            width: 12px;
            height: 12px;
            margin: -5px 0;
            border-radius: 6px;
            border: none;
        }
        #settingsPanel QSlider::handle:horizontal:hover {
            background: %6;
        }
        #settingsPanel QLabel#pageValueLabel {
            color: %6;
            font-size: 12px;
            font-weight: normal;
            min-width: 56px;
        }
    )")
        .arg(panelBg,   // 1
             panelBd,   // 2
             inputBg,   // 3
             txtPrim,   // 4
             txtMuted,  // 5
             txtBright, // 6
             subtleBd,  // 7
             hover,     // 8
             hoverStr,  // 9
             accent));  // 10
}
