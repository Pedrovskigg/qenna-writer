#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

class ProjectModel;
class QComboBox;
class QGridLayout;
class QLineEdit;
class QPlainTextEdit;
class QWidget;

// "Gerador de Timeline" — painel acessível em Configurações que lista todos
// os capítulos/cenas de um manuscrito de uma vez, com campo de marcador
// ("quando se passa") e resumo por linha, salvando tudo em lote. Existe pra
// projetos antigos (criados antes da Timeline orgânica) preencherem esses
// dois campos sem abrir o diálogo de editar capítulo um por um.
class TimelineGeneratorDialog : public QDialog {
    Q_OBJECT
public:
    explicit TimelineGeneratorDialog(ProjectModel* model, QWidget* parent = nullptr);

private slots:
    void rebuildRows();
    void saveAll();

private:
    struct RowRef {
        QString chapterId;
        int sceneIndex = -1;   // -1 = a linha é do capítulo, não de uma cena
        QLineEdit* markerEdit = nullptr;
        QPlainTextEdit* summaryEdit = nullptr;
    };

    ProjectModel* m_model;
    QComboBox* m_manuscriptCombo = nullptr;
    QWidget* m_rowsContainer = nullptr;
    QGridLayout* m_rowsLayout = nullptr;
    QVector<RowRef> m_rows;
};
