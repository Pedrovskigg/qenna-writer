#include "CharacterImageGenDialog.h"

#include "CharacterImageGenService.h"
#include "ClickableImageLabel.h"
#include "GeneratedImageGallery.h"
#include "Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kPreviewSide = 260;
constexpr int kReferenceThumbSide = 48;
}

CharacterImageGenDialog::CharacterImageGenDialog(const QString& characterName,
                                                  const QString& characterContext,
                                                  QWidget* parent)
    : QDialog(parent)
    , m_characterName(characterName)
    , m_characterContext(characterContext)
{
    setModal(true);
    setWindowTitle(characterName.isEmpty()
        ? tr("Gerar imagem")
        : tr("Gerar imagem — %1").arg(characterName));
    setMinimumWidth(420);

    setStyleSheet(QStringLiteral(R"(
        QDialog { background: %1; border: 1px solid %2; }
        QLabel#cigHint, QLabel#cigStatus, QLabel#cigCorrectionLog, QLabel#cigPromptText { color: %3; font-size: 11px; }
        QToolButton#cigPromptToggle {
            background: transparent; border: none; color: %3; font-size: 11px;
            text-align: left; padding: 2px 0;
        }
        QToolButton#cigPromptToggle:hover { color: %4; }
        QLabel#cigFieldLabel { color: %4; font-size: 12px; font-weight: 600; }
        QCheckBox#cigRawPromptCheck { color: %4; font-size: 12px; }
        QPlainTextEdit, QComboBox, QLineEdit {
            background: %5; color: %4; border: 1px solid %6;
            border-radius: 6px; padding: 6px 8px; font-size: 12px;
        }
        QPlainTextEdit:focus, QComboBox:focus, QLineEdit:focus { border-color: %7; }
        QLabel#cigPreview {
            background: %5; border: 1px solid %6; border-radius: 8px; color: %3;
        }
        QFrame#cigCorrectionSection { border-top: 1px solid %6; }
        QPushButton {
            background: transparent; color: %4; border: 1px solid %2;
            border-radius: 6px; padding: 6px 16px; font-size: 12px;
        }
        QPushButton:hover { background: %8; }
        QPushButton:disabled { color: %9; border-color: %2; }
        QPushButton#cigGenerate { border-color: %10; }
        QPushButton#cigUse { background: %10; color: %11; border-color: %10; }
        QPushButton#cigUse:hover { background: %10; }
        QPushButton#cigCorrectionBtn { padding: 6px 12px; }
    )").arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textMuted(),
           Theme::textBright(), Theme::inputBackground(), Theme::subtleBorder(),
           Theme::focusBorder(), Theme::hoverOverlay(), Theme::disabledText(),
           Theme::accentDefault(), Theme::textBright()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    auto* hint = new QLabel(tr(
        "Descreva livremente a cena — a Mira escreve o prompt de imagem "
        "final por você."), this);
    hint->setObjectName(QStringLiteral("cigHint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_rawPromptCheck = new QCheckBox(tr("✍️ Usar meu próprio prompt (sem reescrita da IA)"), this);
    m_rawPromptCheck->setObjectName(QStringLiteral("cigRawPromptCheck"));
    m_rawPromptCheck->setCursor(Qt::PointingHandCursor);
    root->addWidget(m_rawPromptCheck);

    // ---- Modo normal: descrição livre + referência (a IA engenha o prompt) ----
    m_guidedSection = new QWidget(this);
    auto* guidedLay = new QVBoxLayout(m_guidedSection);
    guidedLay->setContentsMargins(0, 0, 0, 0);
    guidedLay->setSpacing(10);

    auto* descLabel = new QLabel(tr("Descrição"), m_guidedSection);
    descLabel->setObjectName(QStringLiteral("cigFieldLabel"));
    guidedLay->addWidget(descLabel);

    m_descriptionEdit = new QPlainTextEdit(m_guidedSection);
    m_descriptionEdit->setPlaceholderText(tr("ex.: sentada num café, segurando uma xícara, olhando pela janela"));
    m_descriptionEdit->setFixedHeight(64);
    guidedLay->addWidget(m_descriptionEdit);

    // Imagem de referência (opcional) — a IA a descreve em palavras e
    // incorpora no prompt (não é edição de imagem de verdade, ver
    // CharacterImageGenService::generate).
    auto* referenceRow = new QHBoxLayout();
    referenceRow->setSpacing(8);

    m_referenceThumb = new QLabel(m_guidedSection);
    m_referenceThumb->setObjectName(QStringLiteral("cigPreview"));
    m_referenceThumb->setFixedSize(kReferenceThumbSide, kReferenceThumbSide);
    m_referenceThumb->setAlignment(Qt::AlignCenter);
    m_referenceThumb->setVisible(false);
    referenceRow->addWidget(m_referenceThumb);

    m_referenceBtn = new QPushButton(tr("📎 Anexar imagem de referência"), m_guidedSection);
    m_referenceBtn->setCursor(Qt::PointingHandCursor);
    connect(m_referenceBtn, &QPushButton::clicked, this, &CharacterImageGenDialog::pickReferenceImage);
    referenceRow->addWidget(m_referenceBtn);

    m_referenceClearBtn = new QPushButton(tr("Remover"), m_guidedSection);
    m_referenceClearBtn->setCursor(Qt::PointingHandCursor);
    m_referenceClearBtn->setVisible(false);
    connect(m_referenceClearBtn, &QPushButton::clicked, this, &CharacterImageGenDialog::clearReferenceImage);
    referenceRow->addWidget(m_referenceClearBtn);

    referenceRow->addStretch(1);
    guidedLay->addLayout(referenceRow);

    root->addWidget(m_guidedSection);

    // ---- Modo manual: prompt final pronto, sem reescrita ----
    m_rawSection = new QWidget(this);
    auto* rawLay = new QVBoxLayout(m_rawSection);
    rawLay->setContentsMargins(0, 0, 0, 0);
    rawLay->setSpacing(6);

    auto* rawLabel = new QLabel(tr("Prompt final"), m_rawSection);
    rawLabel->setObjectName(QStringLiteral("cigFieldLabel"));
    rawLay->addWidget(rawLabel);

    m_rawPromptEdit = new QPlainTextEdit(m_rawSection);
    m_rawPromptEdit->setPlaceholderText(tr("Cole ou escreva aqui o prompt exato — vai direto pra API de imagem, sem passar pela Mira."));
    m_rawPromptEdit->setFixedHeight(96);
    rawLay->addWidget(m_rawPromptEdit);

    m_rawSection->setVisible(false);
    root->addWidget(m_rawSection);

    connect(m_rawPromptCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_guidedSection->setVisible(!checked);
        m_rawSection->setVisible(checked);
        m_styleSection->setVisible(!checked);
    });

    auto* optionsRow = new QHBoxLayout();
    optionsRow->setSpacing(8);

    auto addCombo = [this, optionsRow](const QString& label) {
        auto* col = new QVBoxLayout();
        auto* lbl = new QLabel(label, this);
        lbl->setObjectName(QStringLiteral("cigFieldLabel"));
        col->addWidget(lbl);
        auto* combo = new QComboBox(this);
        col->addWidget(combo);
        optionsRow->addLayout(col);
        return combo;
    };

    // Estilo só se aplica no modo normal (é direção de arte pro passo de
    // engenharia de prompt) — fica num container próprio pra poder sumir
    // inteiro (label + combo) no modo manual.
    m_styleSection = new QWidget(this);
    auto* styleCol = new QVBoxLayout(m_styleSection);
    styleCol->setContentsMargins(0, 0, 0, 0);
    styleCol->setSpacing(0);
    auto* styleLbl = new QLabel(tr("Estilo"), m_styleSection);
    styleLbl->setObjectName(QStringLiteral("cigFieldLabel"));
    styleCol->addWidget(styleLbl);
    m_styleCombo = new QComboBox(m_styleSection);
    styleCol->addWidget(m_styleCombo);
    for (auto preset : { ImageStylePreset::Default, ImageStylePreset::Photorealistic,
                         ImageStylePreset::DigitalRealism, ImageStylePreset::DigitalIllustration,
                         ImageStylePreset::Anime, ImageStylePreset::Cartoon }) {
        m_styleCombo->addItem(imageStylePresetLabel(preset), imageStylePresetKey(preset));
    }
    optionsRow->addWidget(m_styleSection);

    m_modelCombo = addCombo(tr("Modelo"));
    m_modelCombo->addItem(QStringLiteral("GPT Image 1 Mini"), QStringLiteral("gpt-image-1-mini"));
    m_modelCombo->addItem(QStringLiteral("GPT Image 1"), QStringLiteral("gpt-image-1"));

    m_qualityCombo = addCombo(tr("Qualidade"));
    m_qualityCombo->addItem(tr("Baixa"), QStringLiteral("low"));
    m_qualityCombo->addItem(tr("Média"), QStringLiteral("medium"));
    m_qualityCombo->addItem(tr("Alta"), QStringLiteral("high"));

    m_sizeCombo = addCombo(tr("Tamanho"));
    m_sizeCombo->addItem(tr("Quadrado"), QStringLiteral("1024x1024"));
    m_sizeCombo->addItem(tr("Retrato"), QStringLiteral("1024x1536"));
    m_sizeCombo->addItem(tr("Paisagem"), QStringLiteral("1536x1024"));

    root->addLayout(optionsRow);

    // Última escolha lembrada — sem valor "recomendado" imposto, o usuário
    // decide livremente (settings ficam também acessíveis em Configurações).
    QSettings settings;
    auto restoreCombo = [&settings](QComboBox* combo, const QString& key, const QString& fallback) {
        const QString saved = settings.value(key, fallback).toString();
        const int idx = combo->findData(saved);
        combo->setCurrentIndex(idx >= 0 ? idx : 0);
    };
    restoreCombo(m_modelCombo, QStringLiteral("ai/imageModel"), QStringLiteral("gpt-image-1-mini"));
    restoreCombo(m_qualityCombo, QStringLiteral("ai/imageQuality"), QStringLiteral("medium"));
    restoreCombo(m_sizeCombo, QStringLiteral("ai/imageSize"), QStringLiteral("1024x1024"));

    m_previewLabel = new ClickableImageLabel(this);
    m_previewLabel->setObjectName(QStringLiteral("cigPreview"));
    m_previewLabel->setFixedSize(kPreviewSide, kPreviewSide);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setText(tr("A prévia aparece aqui"));
    m_previewLabel->setVisible(false);
    root->addWidget(m_previewLabel, 0, Qt::AlignHCenter);

    // Prompt engenhado pela IA — recolhível, mesmo padrão do "▸ Ver
    // pesquisas" do chat da Mira. Some até a 1ª geração terminar.
    m_promptToggleBtn = new QToolButton(this);
    m_promptToggleBtn->setObjectName(QStringLiteral("cigPromptToggle"));
    m_promptToggleBtn->setText(tr("▸ Ver prompt gerado"));
    m_promptToggleBtn->setCheckable(true);
    m_promptToggleBtn->setCursor(Qt::PointingHandCursor);
    m_promptToggleBtn->setVisible(false);
    root->addWidget(m_promptToggleBtn);

    m_promptLabel = new QLabel(this);
    m_promptLabel->setObjectName(QStringLiteral("cigPromptText"));
    m_promptLabel->setWordWrap(true);
    m_promptLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_promptLabel->setVisible(false);
    root->addWidget(m_promptLabel);

    connect(m_promptToggleBtn, &QToolButton::toggled, this, [this](bool checked) {
        m_promptLabel->setVisible(checked);
        m_promptToggleBtn->setText(checked
            ? tr("▾ Ver prompt gerado") : tr("▸ Ver prompt gerado"));
    });

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("cigStatus"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setVisible(false);
    root->addWidget(m_statusLabel);

    // Chatzinho de correção — só aparece depois da 1ª geração bem-sucedida.
    m_correctionSection = new QFrame(this);
    m_correctionSection->setObjectName(QStringLiteral("cigCorrectionSection"));
    auto* correctionLay = new QVBoxLayout(m_correctionSection);
    correctionLay->setContentsMargins(0, 10, 0, 0);
    correctionLay->setSpacing(6);

    auto* correctionLabel = new QLabel(tr("Pedir correção"), m_correctionSection);
    correctionLabel->setObjectName(QStringLiteral("cigFieldLabel"));
    correctionLay->addWidget(correctionLabel);

    m_correctionLog = new QLabel(m_correctionSection);
    m_correctionLog->setObjectName(QStringLiteral("cigCorrectionLog"));
    m_correctionLog->setWordWrap(true);
    m_correctionLog->setVisible(false);
    correctionLay->addWidget(m_correctionLog);

    auto* correctionRow = new QHBoxLayout();
    correctionRow->setSpacing(6);
    m_correctionEdit = new QLineEdit(m_correctionSection);
    m_correctionEdit->setPlaceholderText(tr("ex.: deixa o cabelo mais escuro, ela sorrindo"));
    connect(m_correctionEdit, &QLineEdit::returnPressed, this, &CharacterImageGenDialog::applyCorrection);
    correctionRow->addWidget(m_correctionEdit, 1);

    m_correctionBtn = new QPushButton(tr("Aplicar"), m_correctionSection);
    m_correctionBtn->setObjectName(QStringLiteral("cigCorrectionBtn"));
    m_correctionBtn->setCursor(Qt::PointingHandCursor);
    connect(m_correctionBtn, &QPushButton::clicked, this, &CharacterImageGenDialog::applyCorrection);
    correctionRow->addWidget(m_correctionBtn);
    correctionLay->addLayout(correctionRow);

    m_correctionSection->setVisible(false);
    root->addWidget(m_correctionSection);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch(1);

    m_cancelBtn = new QPushButton(tr("Cancelar"), this);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(m_cancelBtn);

    m_generateBtn = new QPushButton(tr("Gerar"), this);
    m_generateBtn->setObjectName(QStringLiteral("cigGenerate"));
    m_generateBtn->setCursor(Qt::PointingHandCursor);
    connect(m_generateBtn, &QPushButton::clicked, this, &CharacterImageGenDialog::startGeneration);
    btnRow->addWidget(m_generateBtn);

    m_useBtn = new QPushButton(tr("Usar esta imagem"), this);
    m_useBtn->setObjectName(QStringLiteral("cigUse"));
    m_useBtn->setCursor(Qt::PointingHandCursor);
    m_useBtn->setEnabled(false);
    connect(m_useBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(m_useBtn);

    root->addLayout(btnRow);
}

void CharacterImageGenDialog::setInitialDescription(const QString& text)
{
    m_descriptionEdit->setPlainText(text);
}

void CharacterImageGenDialog::setBusy(bool busy)
{
    m_rawPromptCheck->setEnabled(!busy);
    m_descriptionEdit->setEnabled(!busy);
    m_rawPromptEdit->setEnabled(!busy);
    m_styleCombo->setEnabled(!busy);
    m_modelCombo->setEnabled(!busy);
    m_qualityCombo->setEnabled(!busy);
    m_sizeCombo->setEnabled(!busy);
    m_generateBtn->setEnabled(!busy);
    m_cancelBtn->setEnabled(!busy);
    m_correctionEdit->setEnabled(!busy);
    m_correctionBtn->setEnabled(!busy);
    m_referenceBtn->setEnabled(!busy);
    m_referenceClearBtn->setEnabled(!busy);
    if (!busy) m_useBtn->setEnabled(m_hasResult);
    else m_useBtn->setEnabled(false);
    m_statusLabel->setVisible(busy);
    if (busy) m_statusLabel->setText(tr("Gerando imagem… pode levar até 1 minuto."));
}

void CharacterImageGenDialog::startGeneration()
{
    const bool raw = m_rawPromptCheck->isChecked();
    QString description;
    QString rawPrompt;
    if (raw) {
        rawPrompt = m_rawPromptEdit->toPlainText().trimmed();
        if (rawPrompt.isEmpty()) {
            QMessageBox::information(this, windowTitle(), tr("Cole ou escreva o prompt final que você quer usar."));
            return;
        }
    } else {
        description = m_descriptionEdit->toPlainText().trimmed();
        if (description.isEmpty()) {
            QMessageBox::information(this, windowTitle(), tr("Descreva a imagem que você quer gerar."));
            return;
        }
    }

    const QString modelKey = m_modelCombo->currentData().toString();
    const QString qualityKey = m_qualityCombo->currentData().toString();
    const QString sizeKey = m_sizeCombo->currentData().toString();

    // Persiste a escolha atual — sem impor "recomendado", só lembra a última.
    QSettings settings;
    settings.setValue(QStringLiteral("ai/imageModel"), modelKey);
    settings.setValue(QStringLiteral("ai/imageQuality"), qualityKey);
    settings.setValue(QStringLiteral("ai/imageSize"), sizeKey);

    setBusy(true);
    m_rawPromptModeActive = raw;

    // Um "Gerar" (em vez de correção) é sempre um começo novo — descarta
    // qualquer serviço/conversa de correção anterior e esconde o chatzinho
    // até essa nova geração terminar.
    if (m_service) { m_service->deleteLater(); m_service = nullptr; }
    m_hasResult = false;
    m_correctionSection->setVisible(false);
    m_correctionLog->setVisible(false);
    m_correctionLog->clear();
    m_correctionEdit->clear();

    m_promptToggleBtn->setVisible(false);
    m_promptLabel->clear();

    m_service = new CharacterImageGenService(this);
    connect(m_service, &CharacterImageGenService::imageReady, this, &CharacterImageGenDialog::onImageReady);
    connect(m_service, &CharacterImageGenService::errorOccurred, this, &CharacterImageGenDialog::onServiceError);
    connect(m_service, &CharacterImageGenService::promptEngineered, this, &CharacterImageGenDialog::onPromptEngineered);

    if (raw) {
        // Sem engenharia de prompt nenhuma — vai direto pra API de imagem
        // com o texto exatamente como o autor escreveu.
        m_service->generateFromRawPrompt(rawPrompt, modelKey, qualityKey, sizeKey);
    } else {
        const ImageStylePreset style = imageStylePresetFromKey(m_styleCombo->currentData().toString());
        m_service->generate(description, style, m_characterContext, modelKey, qualityKey, sizeKey,
                             m_referenceImage);
    }
}

void CharacterImageGenDialog::pickReferenceImage()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Selecionar imagem de referência"),
        QString(), tr("Imagens (*.png *.jpg *.jpeg *.gif *.bmp *.webp)"));
    if (path.isEmpty()) return;

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage img = reader.read();
    if (img.isNull()) {
        QMessageBox::warning(this, windowTitle(), tr("Não foi possível carregar essa imagem."));
        return;
    }

    m_referenceImage = img;
    m_referenceThumb->setPixmap(QPixmap::fromImage(img).scaled(
        kReferenceThumbSide, kReferenceThumbSide, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_referenceThumb->setVisible(true);
    m_referenceClearBtn->setVisible(true);
    m_referenceBtn->setText(tr("📎 Trocar imagem de referência"));
}

void CharacterImageGenDialog::clearReferenceImage()
{
    m_referenceImage = QImage();
    m_referenceThumb->setVisible(false);
    m_referenceThumb->setPixmap(QPixmap());
    m_referenceClearBtn->setVisible(false);
    m_referenceBtn->setText(tr("📎 Anexar imagem de referência"));
}

void CharacterImageGenDialog::applyCorrection()
{
    const QString correction = m_correctionEdit->text().trimmed();
    if (correction.isEmpty() || !m_service) return;

    setBusy(true);
    m_statusLabel->setText(tr("Ajustando a imagem conforme sua correção…"));

    const QString existingLog = m_correctionLog->text();
    m_correctionLog->setText(existingLog.isEmpty()
        ? tr("• %1").arg(correction)
        : existingLog + QStringLiteral("\n") + tr("• %1").arg(correction));
    m_correctionLog->setVisible(true);
    m_correctionEdit->clear();

    m_service->refine(correction);
}

void CharacterImageGenDialog::onImageReady(const QImage& img)
{
    m_resultImage = img;
    m_hasResult = true;
    m_previewLabel->setVisible(true);
    m_previewLabel->setPixmap(QPixmap::fromImage(img).scaled(
        kPreviewSide, kPreviewSide, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    const QString suggested = m_characterName.isEmpty() ? QStringLiteral("imagem") : m_characterName;
    m_previewLabel->setFullImage(img, suggested);

    // Salva CADA rodada (não só a que o autor aceitar no fim) — o autor
    // pode querer voltar a uma versão anterior depois. Modo manual não tem
    // prompt "engenhado" (m_promptLabel fica vazio); usa o texto bruto.
    const QString promptUsed = m_rawPromptModeActive
        ? m_rawPromptEdit->toPlainText().trimmed() : m_promptLabel->text();
    GeneratedImageGallery::save(m_projectRoot, img, m_characterName, promptUsed);

    // No modo manual não há prompt "da IA" pra corrigir via conversa — o
    // autor edita o próprio texto e gera de novo.
    m_correctionSection->setVisible(!m_rawPromptModeActive);
    setBusy(false);
}

void CharacterImageGenDialog::onServiceError(const QString& err)
{
    setBusy(false);
    QMessageBox::warning(this, windowTitle(), tr("Erro ao gerar imagem: %1").arg(err));
}

void CharacterImageGenDialog::onPromptEngineered(const QString& prompt)
{
    m_promptLabel->setText(prompt);
    m_promptToggleBtn->setVisible(true);
}
