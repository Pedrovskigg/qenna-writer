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

    // Botões-picker (QToolButton + QMenu) no lugar de QComboBox: no Windows, o
    // popup do QComboBox usa uma janela top-level própria que fica translúcida
    // nesse diálogo (bug de composição do Qt) — QMenu não tem esse problema
    // (mesmo padrão já usado no seletor de gaveta do Menu de Referência).
    void showTrackMenu();
    void showPageTypeMenu();
    void showTemplateMenu();
    void updatePageTypeUi();  // mostra/esconde o picker de modelo conforme o tipo de página

    QLineEdit* m_titleEdit;
    QLineEdit* m_aliasesEdit = nullptr;
    QComboBox* m_roleCombo;
    QToolButton* m_trackBtn = nullptr;
    QString m_trackValue;             // "" auto | "on" | "off"
    QToolButton* m_pageTypeBtn = nullptr;
    QString m_pageTypeValue = QStringLiteral("free");  // "free" | "sheet"
    QLabel* m_templateLabel = nullptr;
    QToolButton* m_templateBtn = nullptr;
    QString m_templateValue;          // id do modelo escolhido, vazio = nenhum
    QCheckBox* m_narratorCheck;
    QLabel* m_imagePreview;
    QPushButton* m_pickImageBtn;
    QPushButton* m_clearImageBtn;
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;
};
