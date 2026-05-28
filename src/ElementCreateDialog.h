#pragma once

#include <QDialog>
#include <QString>

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
    void setInitial(const QString& title, const QString& role, const QString& imageDataUrl, bool narrator = false);

    QString title() const;
    QString role() const;
    QString imageDataUrl() const { return m_imageDataUrl; }
    bool narrator() const;

private slots:
    void pickImage();
    void clearImage();

private:
    void buildUi();
    void updatePreview();
    QString m_elementType;
    QString m_imageDataUrl;

    QLineEdit* m_titleEdit;
    QComboBox* m_roleCombo;
    QCheckBox* m_narratorCheck;
    QLabel* m_imagePreview;
    QPushButton* m_pickImageBtn;
    QPushButton* m_clearImageBtn;
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;
};
