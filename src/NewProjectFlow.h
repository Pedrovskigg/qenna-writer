#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;

// "Nova ideia" virando projeto: o nome (NewIdeaNameDialog) e a pasta-pai
// (NewProjectFolderDialog; o app cria <parent>/<nome>). O Novo projeto normal
// é uma folha só: ver NewProjectSheet.

// Wizard enxuto usado por "Nova Ideia" ao formalizar um rascunho em projeto:
// só pede o nome (autor/gêneros/sinopse/capa ficam pra editar depois, se o
// usuário quiser) — a etapa de pasta segue reaproveitando NewProjectFolderDialog.
class NewIdeaNameDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewIdeaNameDialog(QWidget* parent = nullptr);
    QString projectName() const;

private:
    void applyDialogStyle();

    QLineEdit* m_nameEdit = nullptr;
    QPushButton* m_continueBtn = nullptr;
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
