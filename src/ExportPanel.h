#pragma once

#include <QDialog>

#include "Exporter.h"

class ProjectModel;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;
class QRadioButton;
class QCheckBox;

// Modal "Exportar projeto": árvore de manuscritos→capítulos e gavetas→itens
// (checkboxes tri-state), formato e modo do manuscrito. Espelha o painel do
// Mira 1. Emite exportRequested() com a seleção ao confirmar.
class ExportPanel : public QDialog {
    Q_OBJECT
public:
    explicit ExportPanel(ProjectModel* model, QWidget* parent = nullptr);

signals:
    void exportRequested(const Exporter::Selection& selection);
    // Seleção atual corresponde a exatamente um manuscrito — ver
    // soloManuscriptOfSelection(). Não fecha o painel: usuário pode voltar
    // e ajustar a seleção antes de exportar de fato.
    void previewRequested(const QString& manuscriptId);

private:
    void buildTree();
    void applyTheme();
    void recomputeCount();
    void setAllChecked(bool checked);
    void onExportClicked();
    // "" se a seleção atual de capítulos cobrir 0 ou 2+ manuscritos distintos
    // (ambíguo pra preview, que é sempre de um manuscrito só).
    QString soloManuscriptOfSelection() const;

    enum Role { IdRole = Qt::UserRole + 1, KindRole };
    enum Kind { KindHeader, KindGroup, KindChapter, KindItem };

    ProjectModel* m_model;
    QTreeWidget* m_tree = nullptr;
    QLabel* m_countLabel = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_previewBtn = nullptr;
    QList<QPushButton*> m_formatBtns;
    QRadioButton* m_singleRadio = nullptr;
    QRadioButton* m_separateRadio = nullptr;
    QRadioButton* m_markersIncludeRadio = nullptr;
    QRadioButton* m_markersRemoveRadio = nullptr;
    QCheckBox* m_submissionCheck = nullptr;
    QPushButton* m_submissionDataBtn = nullptr;
    QLabel* m_submissionHint = nullptr;
    Exporter::SubmissionInfo m_submission;

    // Liga/desliga a opção de submissão conforme formato e modo: ela só
    // significa alguma coisa em página paginada e documento único.
    void refreshSubmissionAvailability();
    // Diálogo dos dados que o formato exige (nome legal, contato, byline).
    void editSubmissionData();

    QString m_format = QStringLiteral("odt");
    int m_totalLeaves = 0;
    bool m_building = false;
};
