#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;

// Fluxo de criação de projeto, dividido em 3 etapas modais (compat Mira 1):
//   1. NewProjectTemplateDialog — escolhe blank/basic/advanced.
//   2. NewProjectDetailsDialog  — preenche nome/autor/gêneros/sinopse.
//   3. NewProjectFolderDialog   — escolhe pasta-pai; o app cria a sub-pasta
//      <parent>/<nome> automaticamente.

class NewProjectTemplateDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewProjectTemplateDialog(QWidget* parent = nullptr);
    QString templateId() const { return m_templateId; }

private:
    void applyDialogStyle();
    QString m_templateId = QStringLiteral("blank");
    QLabel* m_hintLabel = nullptr;
};


class NewProjectDetailsDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewProjectDetailsDialog(QWidget* parent = nullptr);

    QString projectName() const;
    QString author() const;
    QString genres() const;
    QString synopsis() const;
    QString coverDataUrl() const { return m_coverDataUrl; }
    QString projectType() const;   // "book" ou "screenplay"

private:
    void applyDialogStyle();
    void onPickCover();
    void onClearCover();
    void updateCoverPreview();

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_authorEdit = nullptr;
    QLineEdit* m_genresEdit = nullptr;
    QPlainTextEdit* m_synopsisEdit = nullptr;
    QPushButton* m_continueBtn = nullptr;
    QLabel* m_coverPreview = nullptr;
    QString m_coverDataUrl;
    QRadioButton* m_bookRadio       = nullptr;
    QRadioButton* m_screenplayRadio = nullptr;
};


class NewProjectFolderDialog : public QDialog {
    Q_OBJECT
public:
    // projectName é só pra exibição da sub-pasta sugerida.
    NewProjectFolderDialog(const QString& projectName, QWidget* parent = nullptr);

    QString parentPath() const { return m_parentPath; }
    QString fullPath() const;

private:
    void applyDialogStyle();
    void updatePathDisplay();

    QString m_projectName;
    QString m_safeName;
    QString m_parentPath;
    QLabel* m_pathLabel = nullptr;
};
