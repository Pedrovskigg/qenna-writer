#pragma once

#include <QDialog>
#include <QImage>
#include <QString>

class CharacterImageGenService;
class ClickableImageLabel;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

// Diálogo modal compartilhado pelos 3 gatilhos MANUAIS de geração de imagem
// (botão na Ficha de Personagem, menu de contexto da gaveta, menu de
// contexto de seleção no editor) — não usado pela tool de chat, que gera
// direto sem UI (caminho headless). Por padrão o usuário só descreve a cena
// livremente e escolhe um preset de estilo — o prompt de imagem final é
// engenhado pela IA (CharacterImageGenService). Não faz o crop — quem
// chamar o diálogo é responsável por isso depois (via
// ImageCropDialog::cropImage), mantendo responsabilidade única.
//
// Depois da 1ª geração bem-sucedida, revela um chatzinho de correção
// (m_correctionEdit/m_correctionBtn) — o usuário explica o que quer ajustar
// em texto livre ("deixa o cabelo mais escuro") e o serviço reaproveita a
// MESMA conversa de engenharia de prompt (CharacterImageGenService::refine)
// pra gerar um prompt corrigido e uma imagem nova, sem começar do zero.
//
// Modo "meu próprio prompt" (m_rawPromptCheck): pra quando o autor não
// confia/não quer a reescrita da IA — cola o prompt final pronto (dele
// mesmo ou de outro lugar) e ele vai direto pra API de imagem, sem passar
// pela engenharia de prompt. Nesse modo estilo/descrição/referência ficam
// irrelevantes (escondidos) e o chatzinho de correção não se aplica (não
// há prompt "da IA" pra corrigir via conversa).
class CharacterImageGenDialog : public QDialog {
    Q_OBJECT
public:
    // characterName/characterContext vazios = geração livre, sem personagem alvo.
    explicit CharacterImageGenDialog(const QString& characterName,
                                      const QString& characterContext,
                                      QWidget* parent = nullptr);

    void setInitialDescription(const QString& text);
    QImage resultImage() const { return m_resultImage; }

    // Opcional — quando setado, toda imagem gerada (cada rodada, não só a
    // aceita no fim) é salva na galeria do projeto (GeneratedImageGallery).
    // Sem isso o diálogo funciona normalmente, só sem persistir na galeria.
    void setProjectRoot(const QString& root) { m_projectRoot = root; }

private:
    void startGeneration();
    void applyCorrection();
    void onImageReady(const QImage& img);
    void onServiceError(const QString& err);
    void onPromptEngineered(const QString& prompt);
    void setBusy(bool busy);
    void pickReferenceImage();
    void clearReferenceImage();

    QString m_characterName;
    QString m_characterContext;
    QString m_projectRoot;
    CharacterImageGenService* m_service = nullptr;
    bool m_hasResult = false; // já teve pelo menos uma geração bem-sucedida
    bool m_rawPromptModeActive = false; // modo da ÚLTIMA geração disparada (pode diferir do checkbox atual)
    QImage m_referenceImage; // opcional — foto de referência anexada pelo autor

    QCheckBox* m_rawPromptCheck = nullptr;
    QWidget* m_guidedSection = nullptr;   // descrição + referência (modo normal)
    QWidget* m_rawSection = nullptr;      // prompt final (modo manual)
    QPlainTextEdit* m_descriptionEdit = nullptr;
    QPlainTextEdit* m_rawPromptEdit = nullptr;
    QLabel* m_referenceThumb = nullptr;
    QPushButton* m_referenceBtn = nullptr;
    QPushButton* m_referenceClearBtn = nullptr;
    QWidget* m_styleSection = nullptr;
    QComboBox* m_styleCombo = nullptr;
    QComboBox* m_modelCombo = nullptr;
    QComboBox* m_qualityCombo = nullptr;
    QComboBox* m_sizeCombo = nullptr;
    QLabel* m_statusLabel = nullptr;
    ClickableImageLabel* m_previewLabel = nullptr;
    QToolButton* m_promptToggleBtn = nullptr;
    QLabel* m_promptLabel = nullptr;
    QPushButton* m_generateBtn = nullptr;
    QPushButton* m_useBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    QWidget* m_correctionSection = nullptr;
    QLabel* m_correctionLog = nullptr;
    QLineEdit* m_correctionEdit = nullptr;
    QPushButton* m_correctionBtn = nullptr;

    QImage m_resultImage;
};
