#pragma once

#include <QDialog>

#include "Exporter.h"

class ProjectModel;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;
class QRadioButton;

// Modal "Exportar projeto": árvore de manuscritos→capítulos e gavetas→itens
// (checkboxes tri-state), formato e modo do manuscrito. Espelha o painel do
// Mira 1. Emite exportRequested() com a seleção ao confirmar.
class ExportPanel : public QDialog {
    Q_OBJECT
public:
    explicit ExportPanel(ProjectModel* model, QWidget* parent = nullptr);

signals:
    void exportRequested(const Exporter::Selection& selection);

private:
    void buildTree();
    void applyTheme();
    void recomputeCount();
    void setAllChecked(bool checked);
    void onExportClicked();

    enum Role { IdRole = Qt::UserRole + 1, KindRole };
    enum Kind { KindHeader, KindGroup, KindChapter, KindItem };

    ProjectModel* m_model;
    QTreeWidget* m_tree = nullptr;
    QLabel* m_countLabel = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QList<QPushButton*> m_formatBtns;
    QRadioButton* m_singleRadio = nullptr;
    QRadioButton* m_separateRadio = nullptr;
    QRadioButton* m_markersIncludeRadio = nullptr;
    QRadioButton* m_markersRemoveRadio = nullptr;

    QString m_format = QStringLiteral("odt");
    int m_totalLeaves = 0;
    bool m_building = false;
};
