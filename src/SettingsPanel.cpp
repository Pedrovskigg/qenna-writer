#include "SettingsPanel.h"

#include "AboutDialog.h"
#include "EditorLayout.h"
#include "MiraPersonality.h"
#include "Theme.h"
#include "UiScale.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace {
// Menor folha de altura fixa oferecida pelo slider. Abaixo disso vira inútil;
// o extremo direito do slider é "Tela cheia" (preenche a janela).
constexpr int kMinSheetHeight = 240;

// Endpoint fixo de cada provedor + um modelo de exemplo (vira placeholder,
// não sobrescreve o campo de Modelo — só o Endpoint é preenchido de fato).
// Todos falam o formato OpenAI chat/completions, que é o único que o
// AIClient sabe montar; "custom" não tem baseUrl (não mexe no campo).
struct AiProviderInfo {
    const char* key;
    const char* baseUrl;
    const char* modelHint;
};
constexpr AiProviderInfo kAiProviders[] = {
    { "openai",    "https://api.openai.com/v1",                          "gpt-4o-mini" },
    { "anthropic", "https://api.anthropic.com/v1",                       "claude-sonnet-5" },
    { "gemini",    "https://generativelanguage.googleapis.com/v1beta/openai", "gemini-2.5-flash" },
    { "xai",       "https://api.x.ai/v1",                                "grok-4" },
    { "custom",    "",                                                   "llama3.2" },
};
}

SettingsPanel::SettingsPanel(QWidget* parent)
    : QDialog(parent)
    , m_spellCheck(new QCheckBox(tr("Ativar corretor ortográfico"), this))
    , m_langCombo(new QComboBox(this))
{
    setObjectName(QStringLiteral("settingsPanel"));
    setWindowTitle(tr("Configurações"));
    setModal(false);
    resize(830, 620);
    setMinimumWidth(830);
    setMaximumHeight(680);

    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &SettingsPanel::applyTheme);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);
    auto* title = new QLabel(tr("Configurações"), this);
    title->setObjectName(QStringLiteral("settingsTitle"));
    titleRow->addWidget(title, 1);
    auto* infoBtn = new QPushButton(QStringLiteral("ⓘ"), this);
    infoBtn->setObjectName(QStringLiteral("settingsInfoBtn"));
    infoBtn->setCursor(Qt::PointingHandCursor);
    infoBtn->setFixedSize(24, 24);
    infoBtn->setToolTip(tr("Sobre o Qenna Writer"));
    connect(infoBtn, &QPushButton::clicked, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });
    titleRow->addWidget(infoBtn);
    root->addLayout(titleRow);

    // ---- Seção: Interface ----
    auto* uiGroup = new QGroupBox(tr("Interface"), this);
    auto* uiLayout = new QVBoxLayout(uiGroup);
    uiLayout->setContentsMargins(14, 8, 14, 14);
    uiLayout->setSpacing(10);

    auto* uiScaleRow = new QHBoxLayout();
    uiScaleRow->addWidget(new QLabel(tr("Tamanho da interface"), uiGroup));
    m_uiScaleSlider = new QSlider(Qt::Horizontal, uiGroup);
    m_uiScaleSlider->setRange(qRound(UiScale::Manager::minScale() * 100),
                               qRound(UiScale::Manager::maxScale() * 100));
    m_uiScaleSlider->setSingleStep(5);
    m_uiScaleSlider->setPageStep(10);
    m_uiScaleSlider->setMinimumWidth(180);
    m_uiScaleSlider->setValue(qRound(UiScale::scale() * 100));
    uiScaleRow->addWidget(m_uiScaleSlider, 1);
    m_uiScaleValue = new QLabel(uiGroup);
    m_uiScaleValue->setObjectName(QStringLiteral("pageValueLabel"));
    m_uiScaleValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_uiScaleValue->setText(tr("%1%").arg(m_uiScaleSlider->value()));
    m_uiScaleValue->setMinimumWidth(40);
    uiScaleRow->addWidget(m_uiScaleValue);
    uiLayout->addLayout(uiScaleRow);

    connect(m_uiScaleSlider, &QSlider::valueChanged, this, [this](int v) {
        m_uiScaleValue->setText(tr("%1%").arg(v));
        UiScale::Manager::instance()->setScale(v / 100.0);
    });
    connect(UiScale::Manager::instance(), &UiScale::Manager::scaleChanged, this, [this]() {
        const int pct = qRound(UiScale::scale() * 100);
        const QSignalBlocker block(m_uiScaleSlider);
        m_uiScaleSlider->setValue(pct);
        m_uiScaleValue->setText(tr("%1%").arg(pct));
    });

    auto* uiScaleHint = new QLabel(
        tr("Ajusta o tamanho da barra de ferramentas, da barra lateral e dos "
           "ícones — útil em telas menores ou de resolução mais alta."),
        uiGroup);
    uiScaleHint->setObjectName(QStringLiteral("settingsHint"));
    uiScaleHint->setWordWrap(true);
    uiLayout->addWidget(uiScaleHint);

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
    // O comprimento vai de uma folha curta (kMinSheetHeight) até o extremo direito,
    // que significa "Tela cheia" (preenche a janela dinamicamente, armazenado como 0).
    m_pageHeightSlider = makeSlider(kMinSheetHeight,
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
        tr("Define o tamanho da \"folha\" e o respiro interno entre a borda e o "
           "texto. No comprimento máximo (\"Tela cheia\") a folha preenche a "
           "janela inteira e acompanha o seu tamanho; arrastando para a esquerda, "
           "a folha ganha uma altura fixa e o fundo aparece em volta. Vale para "
           "todos os projetos."),
        pageGroup);
    m_pageHint->setObjectName(QStringLiteral("settingsHint"));
    m_pageHint->setWordWrap(true);
    pageLayout->addWidget(m_pageHint);

    syncPageLayoutFromManager();

    // ---- Seção: Assistente de IA ----
    // Personalidade (nome/tom/dureza/descrição livre) saiu daqui — mora só no
    // MiraPersonalityDialog agora (clique no nome da assistente no cabeçalho
    // do chat), pra não duplicar a mesma configuração em dois lugares.
    auto* aiGroup = new QGroupBox(tr("Assistente de IA"), this);
    auto* aiLayout = new QVBoxLayout(aiGroup);
    aiLayout->setContentsMargins(14, 8, 14, 14);
    aiLayout->setSpacing(6);

    auto* aiInfoRow = new QHBoxLayout();
    aiInfoRow->setContentsMargins(0, 0, 0, 0);
    aiInfoRow->addStretch(1);
    auto* aiInfoBtn = new QPushButton(QStringLiteral("?"), aiGroup);
    aiInfoBtn->setObjectName(QStringLiteral("settingsInfoBtn"));
    aiInfoBtn->setCursor(Qt::PointingHandCursor);
    aiInfoBtn->setFixedSize(20, 20);
    aiInfoBtn->setToolTip(tr("O que é isso?"));
    connect(aiInfoBtn, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, tr("Assistente de IA"),
            tr("O Qenna Writer conta com uma configuração completa para uso "
               "de um assistente de IA para escrita. Ele pode fazer "
               "revisões, leituras críticas, discutir sobre o projeto, "
               "pesquisar nele e muito mais.\n\n"
               "Como o Qenna Writer é uma ferramenta gratuita, o uso dessa "
               "ferramenta exige que o usuário possua sua própria chave de "
               "API.\n"
               "Caso você não tenha uma chave de API ou simplesmente não "
               "queira usar o assistente, sem problemas. O app não exige e "
               "jamais exigirá ele para nenhuma função específica."));
    });
    aiInfoRow->addWidget(aiInfoBtn);
    aiLayout->addLayout(aiInfoRow);

    // Cada provedor tem endpoint fixo compatível com o formato OpenAI
    // (chat/completions + Authorization: Bearer), que é o único formato que
    // o AIClient fala. "Local" (Ollama/LM Studio etc.) ainda não tem suporte
    // de verdade — item fica desabilitado no combo (ver abaixo) até isso
    // existir; a chave "custom" dele hoje só existe pra não sobrescrever
    // o Endpoint quando alguém configurou algo fora da lista na mão.
    aiLayout->addWidget(new QLabel(tr("Provedor:"), aiGroup));
    m_aiProviderCombo = new QComboBox(aiGroup);
    m_aiProviderCombo->addItem(QStringLiteral("OpenAI"), QStringLiteral("openai"));
    m_aiProviderCombo->addItem(QStringLiteral("Anthropic (Claude)"), QStringLiteral("anthropic"));
    m_aiProviderCombo->addItem(QStringLiteral("Google (Gemini)"), QStringLiteral("gemini"));
    m_aiProviderCombo->addItem(QStringLiteral("xAI (Grok)"), QStringLiteral("xai"));
    m_aiProviderCombo->addItem(tr("Local — em breve"), QStringLiteral("custom"));
    // Ainda não dá suporte a modelo local — item fica visível (avisa que vem
    // por aí) mas desabilitado, pra não deixar selecionar algo que não
    // funciona ainda.
    if (auto* model = qobject_cast<QStandardItemModel*>(m_aiProviderCombo->model())) {
        if (auto* item = model->item(m_aiProviderCombo->count() - 1)) {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }
    }
    aiLayout->addWidget(m_aiProviderCombo);

    aiLayout->addWidget(new QLabel(tr("Chave de API:"), aiGroup));
    m_aiApiKeyEdit = new QLineEdit(aiGroup);
    m_aiApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_aiApiKeyEdit->setPlaceholderText(tr("sk-..."));
    aiLayout->addWidget(m_aiApiKeyEdit);

    aiLayout->addWidget(new QLabel(tr("Endpoint (URL base):"), aiGroup));
    m_aiBaseUrlEdit = new QLineEdit(aiGroup);
    aiLayout->addWidget(m_aiBaseUrlEdit);

    aiLayout->addWidget(new QLabel(tr("Modelo:"), aiGroup));
    m_aiModelEdit = new QLineEdit(aiGroup);
    aiLayout->addWidget(m_aiModelEdit);

    auto* aiHint = new QLabel(
        tr("Usada pela assistente de revisão ao selecionar um trecho no editor. "
           "A chave fica salva localmente em texto puro, sem criptografia — "
           "não compartilhe seu computador/config com quem não deva ver essa chave."),
        aiGroup);
    aiHint->setObjectName(QStringLiteral("settingsHint"));
    aiHint->setWordWrap(true);
    aiLayout->addWidget(aiHint);

    auto* aiProviderHint = new QLabel(
        tr("Trocar o provedor preenche o Endpoint automaticamente (Anthropic, "
           "Gemini e xAI oferecem endpoints compatíveis com o formato OpenAI, "
           "que é o único que este app fala). Suporte a modelo local ainda "
           "está a caminho."),
        aiGroup);
    aiProviderHint->setObjectName(QStringLiteral("settingsHint"));
    aiProviderHint->setWordWrap(true);
    aiLayout->addWidget(aiProviderHint);

    m_aiAutoScanCheck = new QCheckBox(
        tr("Ler documentos automaticamente na 1ª vez que abrir um projeto"), aiGroup);
    aiLayout->addWidget(m_aiAutoScanCheck);
    auto* aiAutoScanHint = new QLabel(
        tr("Roda o mesmo scan do botão \"Ler documentos do projeto\" (uma "
           "chamada de API por documento) sozinho, em segundo plano, só na "
           "primeira vez que um projeto sem resumo salvo ainda é aberto. "
           "Desligado por padrão — liga sob sua responsabilidade, é custo "
           "de API real."),
        aiGroup);
    aiAutoScanHint->setObjectName(QStringLiteral("settingsHint"));
    aiAutoScanHint->setWordWrap(true);
    aiLayout->addWidget(aiAutoScanHint);

    {
        QSettings settings;
        m_aiApiKeyEdit->setText(settings.value(QStringLiteral("ai/apiKey")).toString());
        const QString savedBaseUrl = settings.value(QStringLiteral("ai/baseUrl"),
            QStringLiteral("https://api.openai.com/v1")).toString();
        m_aiBaseUrlEdit->setText(savedBaseUrl);
        m_aiModelEdit->setText(settings.value(QStringLiteral("ai/model"),
            QStringLiteral("gpt-4o-mini")).toString());
        m_aiAutoScanCheck->setChecked(settings.value(QStringLiteral("ai/autoScanNewProjects"), false).toBool());

        // Provedor inicial deduzido do Endpoint salvo (não de uma chave própria
        // em QSettings) — assim quem já tinha um Endpoint customizado antes
        // desta seção existir abre o painel com "Personalizado" já selecionado,
        // em vez de "OpenAI" por padrão.
        int providerIdx = m_aiProviderCombo->count() - 1; // último item = "custom"
        const char* providerModelHint = "llama3.2";
        for (int i = 0; i < m_aiProviderCombo->count(); ++i) {
            const QString key = m_aiProviderCombo->itemData(i).toString();
            for (const AiProviderInfo& info : kAiProviders) {
                if (key != QLatin1String(info.key)) continue;
                if (key != QLatin1String("custom") && savedBaseUrl == QLatin1String(info.baseUrl)) {
                    providerIdx = i;
                    providerModelHint = info.modelHint;
                }
            }
        }
        QSignalBlocker blocker(m_aiProviderCombo);
        m_aiProviderCombo->setCurrentIndex(providerIdx);
        m_aiModelEdit->setPlaceholderText(QString::fromUtf8(providerModelHint));
    }
    connect(m_aiApiKeyEdit, &QLineEdit::editingFinished, this, [this]() {
        QSettings settings;
        settings.setValue(QStringLiteral("ai/apiKey"), m_aiApiKeyEdit->text().trimmed());
    });
    connect(m_aiBaseUrlEdit, &QLineEdit::editingFinished, this, [this]() {
        QSettings settings;
        settings.setValue(QStringLiteral("ai/baseUrl"), m_aiBaseUrlEdit->text().trimmed());
    });
    connect(m_aiModelEdit, &QLineEdit::editingFinished, this, [this]() {
        QSettings settings;
        settings.setValue(QStringLiteral("ai/model"), m_aiModelEdit->text().trimmed());
    });
    connect(m_aiAutoScanCheck, &QCheckBox::toggled, this, [](bool checked) {
        QSettings settings;
        settings.setValue(QStringLiteral("ai/autoScanNewProjects"), checked);
    });
    connect(m_aiProviderCombo, qOverload<int>(&QComboBox::activated), this, [this](int idx) {
        const QString key = m_aiProviderCombo->itemData(idx).toString();
        for (const AiProviderInfo& info : kAiProviders) {
            if (key != QLatin1String(info.key)) continue;
            m_aiModelEdit->setPlaceholderText(QString::fromUtf8(info.modelHint));
            if (key == QLatin1String("custom")) return;
            m_aiBaseUrlEdit->setText(QString::fromUtf8(info.baseUrl));
            QSettings().setValue(QStringLiteral("ai/baseUrl"), QString::fromUtf8(info.baseUrl));
            return;
        }
    });

    // ---- Seção: Geração de imagem de personagem ----
    auto* imgGenGroup = new QGroupBox(tr("Geração de imagem de personagem"), this);
    auto* imgGenLayout = new QVBoxLayout(imgGenGroup);
    imgGenLayout->setContentsMargins(14, 8, 14, 14);
    imgGenLayout->setSpacing(6);

    imgGenLayout->addWidget(new QLabel(tr("Modelo:"), imgGenGroup));
    m_imgModelCombo = new QComboBox(imgGenGroup);
    m_imgModelCombo->addItem(QStringLiteral("GPT Image 1 Mini"), QStringLiteral("gpt-image-1-mini"));
    m_imgModelCombo->addItem(QStringLiteral("GPT Image 1"), QStringLiteral("gpt-image-1"));
    imgGenLayout->addWidget(m_imgModelCombo);

    imgGenLayout->addWidget(new QLabel(tr("Qualidade:"), imgGenGroup));
    m_imgQualityCombo = new QComboBox(imgGenGroup);
    m_imgQualityCombo->addItem(tr("Baixa"), QStringLiteral("low"));
    m_imgQualityCombo->addItem(tr("Média"), QStringLiteral("medium"));
    m_imgQualityCombo->addItem(tr("Alta"), QStringLiteral("high"));
    imgGenLayout->addWidget(m_imgQualityCombo);

    imgGenLayout->addWidget(new QLabel(tr("Tamanho:"), imgGenGroup));
    m_imgSizeCombo = new QComboBox(imgGenGroup);
    m_imgSizeCombo->addItem(tr("Quadrado"), QStringLiteral("1024x1024"));
    m_imgSizeCombo->addItem(tr("Retrato"), QStringLiteral("1024x1536"));
    m_imgSizeCombo->addItem(tr("Paisagem"), QStringLiteral("1536x1024"));
    imgGenLayout->addWidget(m_imgSizeCombo);

    auto* imgGenHint = new QLabel(
        tr("Ponto de partida do diálogo de geração (a escolha feita lá "
           "atualiza estes campos) e também o que a %1 usa quando gera uma "
           "imagem sozinha durante o chat, sem abrir diálogo nenhum. A "
           "geração de imagem sempre usa a API oficial da OpenAI "
           "(api.openai.com) com a Chave de API acima, independente do "
           "Endpoint configurado pro chat.").arg(miraAssistantName()),
        imgGenGroup);
    imgGenHint->setObjectName(QStringLiteral("settingsHint"));
    imgGenHint->setWordWrap(true);
    imgGenLayout->addWidget(imgGenHint);

    {
        QSettings settings;
        auto restoreCombo = [&settings](QComboBox* combo, const QString& key, const QString& fallback) {
            const QString saved = settings.value(key, fallback).toString();
            const int idx = combo->findData(saved);
            combo->setCurrentIndex(idx >= 0 ? idx : 0);
        };
        restoreCombo(m_imgModelCombo, QStringLiteral("ai/imageModel"), QStringLiteral("gpt-image-1-mini"));
        restoreCombo(m_imgQualityCombo, QStringLiteral("ai/imageQuality"), QStringLiteral("medium"));
        restoreCombo(m_imgSizeCombo, QStringLiteral("ai/imageSize"), QStringLiteral("1024x1024"));
    }
    connect(m_imgModelCombo, &QComboBox::currentIndexChanged, this, [this]() {
        QSettings settings;
        settings.setValue(QStringLiteral("ai/imageModel"), m_imgModelCombo->currentData().toString());
    });
    connect(m_imgQualityCombo, &QComboBox::currentIndexChanged, this, [this]() {
        QSettings settings;
        settings.setValue(QStringLiteral("ai/imageQuality"), m_imgQualityCombo->currentData().toString());
    });
    connect(m_imgSizeCombo, &QComboBox::currentIndexChanged, this, [this]() {
        QSettings settings;
        settings.setValue(QStringLiteral("ai/imageSize"), m_imgSizeCombo->currentData().toString());
    });

    // ---- Seção: Detecção de personagens ----
    auto* detectGroup = new QGroupBox(tr("Detecção de personagens"), this);
    auto* detectLayout = new QVBoxLayout(detectGroup);
    detectLayout->setContentsMargins(14, 8, 14, 14);
    detectLayout->setSpacing(8);

    m_detectionCheck = new QCheckBox(tr("Detectar personagens automaticamente"), detectGroup);
    detectLayout->addWidget(m_detectionCheck);

    m_detectionAllCheck = new QCheckBox(tr("Marcar todos sem confirmar"), detectGroup);
    detectLayout->addWidget(m_detectionAllCheck);

    auto* detectHint = new QLabel(
        tr("Quando ativado, o app detecta nomes de personagens no texto e sugere marcar a presença deles na cena."),
        detectGroup);
    detectHint->setObjectName(QStringLiteral("settingsHint"));
    detectHint->setWordWrap(true);
    detectLayout->addWidget(detectHint);

    m_rescanScenesBtn = new QPushButton(tr("Detectar presença por cena em todos os capítulos"), detectGroup);
    detectLayout->addWidget(m_rescanScenesBtn);

    auto* rescanHint = new QLabel(
        tr("Preenche a presença por CENA (não só por capítulo) usando quem já está "
           "confirmado — útil pra capítulos antigos que você ainda não reabriu "
           "nesta versão. Roda um capítulo por vez, não trava o app."),
        detectGroup);
    rescanHint->setObjectName(QStringLiteral("settingsHint"));
    rescanHint->setWordWrap(true);
    detectLayout->addWidget(rescanHint);

    connect(m_detectionCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_detectionAllCheck->setEnabled(checked);
        emit detectionEnabledChanged(checked);
    });
    connect(m_detectionAllCheck, &QCheckBox::toggled, this, [this](bool checked) {
        emit detectionMarkAllChanged(checked);
    });
    connect(m_rescanScenesBtn, &QPushButton::clicked, this, [this]() {
        emit rescanAllScenesRequested();
    });

    // ---- Seção: Menções (@) ----
    auto* mentionGroup = new QGroupBox(tr("Menções (@)"), this);
    auto* mentionLayout = new QVBoxLayout(mentionGroup);
    mentionLayout->setContentsMargins(14, 8, 14, 14);
    mentionLayout->setSpacing(8);

    m_mentionManuscriptsCheck = new QCheckBox(tr("Permitir marcar documentos do manuscrito"), mentionGroup);
    mentionLayout->addWidget(m_mentionManuscriptsCheck);

    auto* mentionHint = new QLabel(
        tr("Por padrão, @ só sugere documentos das gavetas (personagens, locais etc.). "
           "Ative para incluir também capítulos e cenas do manuscrito."),
        mentionGroup);
    mentionHint->setObjectName(QStringLiteral("settingsHint"));
    mentionHint->setWordWrap(true);
    mentionLayout->addWidget(mentionHint);

    connect(m_mentionManuscriptsCheck, &QCheckBox::toggled, this, [this](bool checked) {
        emit mentionManuscriptsEnabledChanged(checked);
    });

    // ---- Seção: Navegação ----
    auto* navGroup = new QGroupBox(tr("Navegação"), this);
    auto* navLayout = new QVBoxLayout(navGroup);
    navLayout->setContentsMargins(14, 8, 14, 14);
    navLayout->setSpacing(8);

    m_autoNavCheck = new QCheckBox(tr("Navegar automaticamente entre capítulos"), navGroup);
    navLayout->addWidget(m_autoNavCheck);

    auto* navHint = new QLabel(
        tr("Ao chegar no início ou fim de um capítulo, manter o scroll pressionado "
           "na borda por 2 segundos avança ou retrocede automaticamente para o próximo."),
        navGroup);
    navHint->setObjectName(QStringLiteral("settingsHint"));
    navHint->setWordWrap(true);
    navLayout->addWidget(navHint);

    connect(m_autoNavCheck, &QCheckBox::toggled, this, [this](bool checked) {
        emit autoNavEnabledChanged(checked);
    });

    // ---- Seção: Capítulos ----
    auto* chapterGroup = new QGroupBox(tr("Capítulos"), this);
    auto* chapterLayout = new QVBoxLayout(chapterGroup);
    chapterLayout->setContentsMargins(14, 8, 14, 14);
    chapterLayout->setSpacing(8);

    m_romanNumeralsCheck = new QCheckBox(tr("Usar numerais romanos para os capítulos"), chapterGroup);
    chapterLayout->addWidget(m_romanNumeralsCheck);

    auto* romanHint = new QLabel(
        tr("Troca o número que aparece antes do título do capítulo (ex.: \"3 - "
           "A Batalha\" vira \"III - A Batalha\") na barra lateral do manuscrito."),
        chapterGroup);
    romanHint->setObjectName(QStringLiteral("settingsHint"));
    romanHint->setWordWrap(true);
    chapterLayout->addWidget(romanHint);

    connect(m_romanNumeralsCheck, &QCheckBox::toggled, this, [this](bool checked) {
        emit romanChapterNumbersChanged(checked);
    });

    // ---- Seção: Linha do tempo ----
    auto* timelineGroup = new QGroupBox(tr("Linha do tempo"), this);
    auto* timelineLayout = new QVBoxLayout(timelineGroup);
    timelineLayout->setContentsMargins(14, 8, 14, 14);
    timelineLayout->setSpacing(8);

    m_scenePopupCheck = new QCheckBox(tr("Mostrar popup ao criar cena via \"----\""), timelineGroup);
    timelineLayout->addWidget(m_scenePopupCheck);

    auto* timelineHint = new QLabel(
        tr("Ao dividir o capítulo numa cena nova, pergunta o marcador temporal e "
           "o resumo que alimentam a linha do tempo. Se desligado, defina isso "
           "manualmente pelo clique direito na cena."),
        timelineGroup);
    timelineHint->setObjectName(QStringLiteral("settingsHint"));
    timelineHint->setWordWrap(true);
    timelineLayout->addWidget(timelineHint);

    connect(m_scenePopupCheck, &QCheckBox::toggled, this, [this](bool checked) {
        emit showScenePopupOnHrChanged(checked);
    });

    auto* timelineGenBtn = new QPushButton(tr("Abrir Gerador de Timeline…"), timelineGroup);
    timelineLayout->addWidget(timelineGenBtn);

    auto* timelineGenHint = new QLabel(
        tr("Preenche marcador temporal e resumo de vários capítulos/cenas de uma vez — "
           "útil pra colocar um manuscrito antigo (de antes da Timeline orgânica) em dia "
           "de uma tacada só, em vez de editar capítulo por capítulo."),
        timelineGroup);
    timelineGenHint->setObjectName(QStringLiteral("settingsHint"));
    timelineGenHint->setWordWrap(true);
    timelineLayout->addWidget(timelineGenHint);

    connect(timelineGenBtn, &QPushButton::clicked, this, [this]() {
        emit timelineGeneratorRequested();
    });

    // ---- Seção: Memória ----
    auto* memGroup = new QGroupBox(tr("Memória"), this);
    auto* memLayout = new QVBoxLayout(memGroup);
    memLayout->setContentsMargins(14, 8, 14, 14);
    memLayout->setSpacing(8);

    auto* memRow = new QHBoxLayout;
    memRow->setSpacing(8);
    auto* memLabel = new QLabel(tr("Documentos simultâneos na RAM:"), memGroup);
    m_maxDocsSpinBox = new QSpinBox(memGroup);
    m_maxDocsSpinBox->setRange(1, 20);
    m_maxDocsSpinBox->setValue(6);
    m_maxDocsSpinBox->setFixedWidth(60);
    memRow->addWidget(memLabel);
    memRow->addWidget(m_maxDocsSpinBox);
    memRow->addStretch();
    memLayout->addLayout(memRow);

    auto* memHint = new QLabel(
        tr("O app mantém os documentos abertos recentemente na memória para troca rápida. "
           "Ao atingir o limite, o mais antigo (sem edições pendentes) é descarregado automaticamente."),
        memGroup);
    memHint->setObjectName(QStringLiteral("settingsHint"));
    memHint->setWordWrap(true);
    memLayout->addWidget(memHint);

    connect(m_maxDocsSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        emit maxDocsChanged(v);
    });

    // ---- Seção: Meta diária ----
    auto* goalGroup = new QGroupBox(tr("Meta diária"), this);
    auto* goalLayout = new QVBoxLayout(goalGroup);
    goalLayout->setContentsMargins(14, 8, 14, 14);
    goalLayout->setSpacing(8);

    m_unifiedGoalCheck = new QCheckBox(tr("Meta unificada entre todos os projetos"), goalGroup);
    goalLayout->addWidget(m_unifiedGoalCheck);

    auto* goalHint = new QLabel(
        tr("Quando ativado, a meta e o progresso do dia passam a ser somados entre "
           "todos os projetos abertos no Qenna Writer, em vez de contar isolado "
           "por projeto — útil pra quem escreve em mais de um no mesmo dia."),
        goalGroup);
    goalHint->setObjectName(QStringLiteral("settingsHint"));
    goalHint->setWordWrap(true);
    goalLayout->addWidget(goalHint);

    connect(m_unifiedGoalCheck, &QCheckBox::toggled, this, [this](bool checked) {
        emit unifiedGoalEnabledChanged(checked);
    });

    // ---- Seção: Backup completo de projeto ----
    auto* backupGroup = new QGroupBox(tr("Backup de projeto"), this);
    auto* backupLayout = new QVBoxLayout(backupGroup);
    backupLayout->setContentsMargins(14, 8, 14, 14);
    backupLayout->setSpacing(8);

    m_backupModeCombo = new QComboBox(backupGroup);
    m_backupModeCombo->addItem(tr("Desligado"), 0);
    m_backupModeCombo->addItem(tr("Automático"), 1);
    m_backupModeCombo->addItem(tr("Só lembrete"), 2);
    backupLayout->addWidget(m_backupModeCombo);

    auto* backupHint = new QLabel(
        tr("Zipa a pasta inteira do projeto (manuscritos, fichas, lousas, tudo) "
           "periodicamente. Escolha uma pasta de destino FORA da pasta do "
           "projeto e, se possível, fora de Documentos — outro disco, pendrive "
           "ou uma pasta sincronizada na nuvem protegem de verdade contra "
           "perder o projeto e o backup juntos."),
        backupGroup);
    backupHint->setObjectName(QStringLiteral("settingsHint"));
    backupHint->setWordWrap(true);
    backupLayout->addWidget(backupHint);

    m_backupFolderRow = new QWidget(backupGroup);
    auto* folderRowLay = new QHBoxLayout(m_backupFolderRow);
    folderRowLay->setContentsMargins(0, 0, 0, 0);
    folderRowLay->setSpacing(8);
    m_backupFolderEdit = new QLineEdit(m_backupFolderRow);
    m_backupFolderEdit->setReadOnly(true);
    m_backupFolderEdit->setPlaceholderText(tr("Nenhuma pasta escolhida"));
    m_backupFolderBtn = new QPushButton(tr("Escolher pasta…"), m_backupFolderRow);
    folderRowLay->addWidget(m_backupFolderEdit, 1);
    folderRowLay->addWidget(m_backupFolderBtn);
    backupLayout->addWidget(m_backupFolderRow);

    m_backupIntervalRow = new QWidget(backupGroup);
    auto* intervalRowLay = new QHBoxLayout(m_backupIntervalRow);
    intervalRowLay->setContentsMargins(0, 0, 0, 0);
    intervalRowLay->setSpacing(8);
    intervalRowLay->addWidget(new QLabel(tr("A cada:"), m_backupIntervalRow));
    m_backupIntervalCombo = new QComboBox(m_backupIntervalRow);
    m_backupIntervalCombo->addItem(tr("1 dia"), 1440);
    m_backupIntervalCombo->addItem(tr("2 dias"), 2880);
    m_backupIntervalCombo->addItem(tr("3 dias"), 4320);
    m_backupIntervalCombo->addItem(tr("4 dias"), 5760);
    m_backupIntervalCombo->addItem(tr("5 dias"), 7200);
    m_backupIntervalCombo->addItem(tr("6 dias"), 8640);
    m_backupIntervalCombo->addItem(tr("7 dias"), 10080);
    intervalRowLay->addWidget(m_backupIntervalCombo);
    intervalRowLay->addStretch();
    backupLayout->addWidget(m_backupIntervalRow);

    auto* runRow = new QHBoxLayout();
    runRow->setSpacing(8);
    m_backupRunBtn = new QPushButton(tr("Fazer backup agora"), backupGroup);
    m_backupStatusLabel = new QLabel(backupGroup);
    m_backupStatusLabel->setObjectName(QStringLiteral("settingsHint"));
    runRow->addWidget(m_backupRunBtn);
    runRow->addWidget(m_backupStatusLabel, 1);
    backupLayout->addLayout(runRow);

    connect(m_backupModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        const int mode = m_backupModeCombo->itemData(idx).toInt();
        if (m_backupIntervalRow) m_backupIntervalRow->setVisible(mode != 0);
        if (m_blockSignals) return;
        emit backupModeChanged(mode);
    });
    connect(m_backupIntervalCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_blockSignals) return;
        emit backupIntervalMinutesChanged(m_backupIntervalCombo->itemData(idx).toInt());
    });
    connect(m_backupFolderBtn, &QPushButton::clicked, this, [this]() {
        const QString chosen = QFileDialog::getExistingDirectory(this,
            tr("Escolher pasta de destino do backup"), m_backupFolderEdit->text(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (chosen.isEmpty()) return;
        m_backupFolderEdit->setText(chosen);
        emit backupFolderChanged(chosen);
    });
    connect(m_backupRunBtn, &QPushButton::clicked, this, [this]() {
        emit backupRunNowRequested();
    });

    // Montagem em duas colunas — dentro de um scroll, porque a coluna direita
    // já não cabe mais numa janela de altura razoável (5 grupos empilhados).
    auto* colsWidget = new QWidget(this);
    auto* cols = new QHBoxLayout(colsWidget);
    cols->setContentsMargins(0, 0, 0, 0);
    cols->setSpacing(16);

    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(10);
    leftCol->addWidget(uiGroup);
    leftCol->addWidget(spellGroup);
    leftCol->addWidget(pageGroup);
    leftCol->addWidget(aiGroup);
    leftCol->addWidget(imgGenGroup);
    leftCol->addStretch();

    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(10);
    rightCol->addWidget(detectGroup);
    rightCol->addWidget(mentionGroup);
    rightCol->addWidget(navGroup);
    rightCol->addWidget(chapterGroup);
    rightCol->addWidget(goalGroup);
    rightCol->addWidget(timelineGroup);
    rightCol->addWidget(memGroup);
    rightCol->addWidget(backupGroup);
    rightCol->addStretch();

    cols->addLayout(leftCol, 1);
    cols->addLayout(rightCol, 1);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("settingsScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(colsWidget);
    root->addWidget(scrollArea, 1);

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
                m_pageHeightValue->setText(pageHeightLabelText(v));
                if (m_blockLayoutSignals) return;
                // Extremo direito = "Tela cheia" → armazena 0 (preenche a janela
                // dinamicamente). Valores menores são folhas de altura fixa.
                const bool full = (v >= m_pageHeightSlider->maximum());
                layoutMgr->setPageHeight(full ? 0 : v);
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
    // ph == 0 (Tela cheia) → extremo direito do slider. ph fixo → posição direta.
    m_pageHeightSlider->setValue(mgr->pageHeight() <= 0
                                 ? m_pageHeightSlider->maximum()
                                 : mgr->pageHeight());
    m_hMarginSlider->setValue(mgr->horizontalMargin());
    m_vMarginSlider->setValue(mgr->verticalMargin());
    m_pageWidthValue->setText(QStringLiteral("%1 px").arg(mgr->pageWidth()));
    m_pageHeightValue->setText(pageHeightLabelText(m_pageHeightSlider->value()));
    m_hMarginValue->setText(QStringLiteral("%1 px").arg(mgr->horizontalMargin()));
    m_vMarginValue->setText(QStringLiteral("%1 px").arg(mgr->verticalMargin()));
    m_blockLayoutSignals = false;
}

QString SettingsPanel::pageHeightLabelText(int v) const
{
    // Extremo direito do slider = preenche a janela (sem altura fixa).
    if (v <= 0 || (m_pageHeightSlider && v >= m_pageHeightSlider->maximum()))
        return tr("Tela cheia");
    return QStringLiteral("%1 px").arg(v);
}

void SettingsPanel::setPageHeightMaximum(int px)
{
    if (!m_pageHeightSlider || px <= 0) return;
    m_blockLayoutSignals = true;
    m_pageHeightSlider->setMaximum(px);
    m_blockLayoutSignals = false;
    // Reexibe o valor atual já dentro do novo teto (setMaximum pode tê-lo grampeado).
    syncPageLayoutFromManager();
}

void SettingsPanel::setRescanScenesButtonText(const QString& text)
{
    if (m_rescanScenesBtn) m_rescanScenesBtn->setText(text);
}

void SettingsPanel::setRescanScenesButtonEnabled(bool enabled)
{
    if (m_rescanScenesBtn) m_rescanScenesBtn->setEnabled(enabled);
}

void SettingsPanel::setBackupMode(int mode)
{
    if (!m_backupModeCombo) return;
    m_blockSignals = true;
    const int idx = m_backupModeCombo->findData(mode);
    if (idx >= 0) m_backupModeCombo->setCurrentIndex(idx);
    if (m_backupIntervalRow) m_backupIntervalRow->setVisible(mode != 0);
    m_blockSignals = false;
}

void SettingsPanel::setBackupFolder(const QString& folder)
{
    if (m_backupFolderEdit) m_backupFolderEdit->setText(folder);
}

void SettingsPanel::setBackupIntervalMinutes(int minutes)
{
    if (!m_backupIntervalCombo) return;
    m_blockSignals = true;
    const int idx = m_backupIntervalCombo->findData(minutes);
    if (idx >= 0) m_backupIntervalCombo->setCurrentIndex(idx);
    m_blockSignals = false;
}

void SettingsPanel::setBackupStatusText(const QString& text)
{
    if (m_backupStatusLabel) m_backupStatusLabel->setText(text);
}

void SettingsPanel::setBackupRunButtonEnabled(bool enabled)
{
    if (m_backupRunBtn) m_backupRunBtn->setEnabled(enabled);
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
        #settingsScrollArea, #settingsScrollArea > QWidget > QWidget {
            background: transparent;
            border: none;
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
        #settingsPanel QLineEdit {
            background: %3;
            color: %6;
            border: 1px solid %2;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 12px;
            font-weight: normal;
            min-height: 22px;
        }
        #settingsPanel QLineEdit:focus {
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
        QPushButton#settingsInfoBtn {
            background: transparent;
            color: %5;
            border: 1px solid %2;
            border-radius: 12px;
            font-size: 13px;
            padding: 0;
        }
        QPushButton#settingsInfoBtn:hover {
            color: %6;
            border-color: %9;
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

bool SettingsPanel::detectionEnabled() const
{
    return m_detectionCheck ? m_detectionCheck->isChecked() : true;
}

bool SettingsPanel::detectionMarkAll() const
{
    return m_detectionAllCheck ? m_detectionAllCheck->isChecked() : false;
}

void SettingsPanel::setDetectionEnabled(bool enabled)
{
    if (m_detectionCheck) {
        m_detectionCheck->setChecked(enabled);
        if (m_detectionAllCheck) m_detectionAllCheck->setEnabled(enabled);
    }
}

void SettingsPanel::setDetectionMarkAll(bool markAll)
{
    if (m_detectionAllCheck) m_detectionAllCheck->setChecked(markAll);
}

bool SettingsPanel::autoNavEnabled() const
{
    return m_autoNavCheck ? m_autoNavCheck->isChecked() : true;
}

void SettingsPanel::setAutoNavEnabled(bool enabled)
{
    if (m_autoNavCheck) m_autoNavCheck->setChecked(enabled);
}

bool SettingsPanel::unifiedGoalEnabled() const
{
    return m_unifiedGoalCheck ? m_unifiedGoalCheck->isChecked() : false;
}

void SettingsPanel::setUnifiedGoalEnabled(bool enabled)
{
    if (m_unifiedGoalCheck) m_unifiedGoalCheck->setChecked(enabled);
}

bool SettingsPanel::showScenePopupOnHr() const
{
    return m_scenePopupCheck ? m_scenePopupCheck->isChecked() : true;
}

void SettingsPanel::setShowScenePopupOnHr(bool enabled)
{
    if (m_scenePopupCheck) m_scenePopupCheck->setChecked(enabled);
}

bool SettingsPanel::romanChapterNumbers() const
{
    return m_romanNumeralsCheck ? m_romanNumeralsCheck->isChecked() : false;
}

void SettingsPanel::setRomanChapterNumbers(bool enabled)
{
    if (m_romanNumeralsCheck) m_romanNumeralsCheck->setChecked(enabled);
}

int SettingsPanel::maxDocs() const
{
    return m_maxDocsSpinBox ? m_maxDocsSpinBox->value() : 6;
}

void SettingsPanel::setMaxDocs(int n)
{
    if (m_maxDocsSpinBox) m_maxDocsSpinBox->setValue(qBound(1, n, 20));
}

bool SettingsPanel::mentionManuscriptsEnabled() const
{
    return m_mentionManuscriptsCheck ? m_mentionManuscriptsCheck->isChecked() : false;
}

void SettingsPanel::setMentionManuscriptsEnabled(bool enabled)
{
    if (m_mentionManuscriptsCheck) m_mentionManuscriptsCheck->setChecked(enabled);
}
