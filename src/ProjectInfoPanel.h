#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class ProjectModel;

// Painel de Informações da obra — invocado pelo botão Info da LeftBar.
// Edita nome do projeto, autor, gêneros, sinopse e capa. Persiste em
// data.projectDetails (compat Mira 1) via ProjectModel::setProjectDetails.
class ProjectInfoPanel : public QDialog {
    Q_OBJECT
public:
    explicit ProjectInfoPanel(ProjectModel* model, QWidget* parent = nullptr);

public slots:
    void loadFromModel();

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onPickCover();
    void onClearCover();
    void onSave();
    void applyDialogStyle();

private:
    void buildUi();
    void updateCoverPreview();

    ProjectModel* m_model;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_authorEdit = nullptr;
    QLineEdit* m_genresEdit = nullptr;
    QPlainTextEdit* m_synopsisEdit = nullptr;
    QLabel* m_coverPreview = nullptr;
    QPushButton* m_pickCoverBtn = nullptr;
    QPushButton* m_clearCoverBtn = nullptr;
    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QString m_coverDataUrl;
};
