#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QCheckBox;
class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;

// Diálogo de criação/edição de elemento narrativo (personagem, cenário, objeto).
// Campos exibidos variam conforme o tipo do drawer.
class ElementCreateDialog : public QDialog {
    Q_OBJECT
public:
    // elementType: "character" | "setting" | "object" | "" (vazio = item comum)
    ElementCreateDialog(const QString& elementType, QWidget* parent = nullptr);

    // Pré-preencher pra modo de edição.
    void setInitial(const QString& title, const QString& role, const QString& imageDataUrl,
                    bool narrator = false, const QString& trackMode = QString(),
                    const QStringList& aliases = QStringList());

    QString title() const;
    QString role() const;
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

    QLineEdit* m_titleEdit;
    QLineEdit* m_aliasesEdit = nullptr;
    QComboBox* m_roleCombo;
    QComboBox* m_trackCombo = nullptr;
    QCheckBox* m_narratorCheck;
    QLabel* m_imagePreview;
    QPushButton* m_pickImageBtn;
    QPushButton* m_clearImageBtn;
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;
};
