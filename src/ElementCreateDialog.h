#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>

#include "SheetTemplatesStore.h"   // SheetTemplate (por valor)

class QCheckBox;
class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;
class QToolButton;

// Diálogo de criação/edição de elemento narrativo (personagem, cenário, objeto).
// Campos exibidos variam conforme o tipo do drawer.
class ElementCreateDialog : public QDialog {
    Q_OBJECT
public:
    // elementType: "character" | "setting" | "object" | "" (vazio = item comum)
    // sheetTemplates: modelos de ficha salvos, oferecidos no combo quando o tipo
    // de página escolhido for "Ficha" (só relevante pra elementType=="character").
    ElementCreateDialog(const QString& elementType, QWidget* parent = nullptr,
                        const QVector<SheetTemplate>& sheetTemplates = {});

    // Pré-preencher pra modo de edição.
    void setInitial(const QString& title, const QString& role, const QString& imageDataUrl,
                    bool narrator = false, const QString& trackMode = QString(),
                    const QStringList& aliases = QStringList());

    QString title() const;
    QString role() const;
    // true = criar a página do personagem como Ficha estruturada (só personagem).
    bool createAsSheet() const;
    // Modelo de ficha escolhido no combo (vazio = "Vazio (padrão)", sem modelo).
    QString selectedTemplateId() const;
    QString imageDataUrl() const { return m_imageDataUrl; }
    bool narrator() const;
    QString trackMode() const;  // "" auto | "on" | "off" (trilha na linha do tempo)
    QStringList aliases() const; // apelidos do personagem (para o detector de presença)

private slots:
    void pickImage();
    void clearImage();

private:
    void buildUi();
    void updatePreview();
    QString m_elementType;
    QString m_imageDataUrl;
    QVector<SheetTemplate> m_sheetTemplates;

    // Pickers inline (botão + painel de opções que expande dentro do próprio
    // diálogo) no lugar de QComboBox/QMenu: qualquer popup que abre como
    // janela top-level própria nesse diálogo específico sai com fundo
    // translúcido no Windows (bug de composição do Qt/DWM) — um painel que só
    // expande dentro do layout existente não cria janela nenhuma, então não
    // tem como sofrer desse bug. Mesmo conceito do "?" de prós/contras do
    // Construtor (ConstrutorWindow), adaptado pra escolher uma opção em vez
    // de só revelar mais texto.
    void updatePageTypeUi();  // mostra/esconde o picker de modelo conforme o tipo de página

    QLineEdit* m_titleEdit;
    QLineEdit* m_aliasesEdit = nullptr;
    QComboBox* m_roleCombo;
    QToolButton* m_trackBtn = nullptr;
    QWidget* m_trackOptions = nullptr;
    QString m_trackValue;             // "" auto | "on" | "off"
    QToolButton* m_pageTypeBtn = nullptr;
    QWidget* m_pageTypeOptions = nullptr;
    QString m_pageTypeValue = QStringLiteral("free");  // "free" | "sheet"
    QLabel* m_templateLabel = nullptr;
    QToolButton* m_templateBtn = nullptr;
    QWidget* m_templateOptions = nullptr;
    QString m_templateValue;          // id do modelo escolhido, vazio = nenhum
    QCheckBox* m_narratorCheck;
    QLabel* m_imagePreview;
    QPushButton* m_pickImageBtn;
    QPushButton* m_clearImageBtn;
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;
};
