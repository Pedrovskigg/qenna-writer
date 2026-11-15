#pragma once

#include <QDialog>

#include "Exporter.h"

#include <functional>
#include <memory>

class ProjectModel;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;
class QCheckBox;
class QTimer;
class QToolButton;
class ExportPreviewView;

// Modal "Exportar" (rework de 15/11/2026, escolhido no Exportar Lab: a
// "Impressão"). À esquerda, a folha do que vai sair, montada pelo exportador de
// verdade, pra folhear; à direita, uma coluna quieta com formato, o que vai
// (a árvore de manuscritos→capítulos e gavetas→itens, dobrada num seletor),
// manuscrito em um arquivo ou um por capítulo, marcadores e o formato de
// editora (ou, no roteiro, a numeração das cenas). Emite exportRequested().
class ExportPanel : public QDialog {
    Q_OBJECT
public:
    explicit ExportPanel(ProjectModel* model, QWidget* parent = nullptr);
    ~ExportPanel() override;

    // A prévia monta as páginas com o exportador do projeto (pasta e estilo
    // de parágrafo). Sem isso, a folha fica vazia e o resto funciona igual.
    void setPreviewSource(const QString& projectRoot, const Exporter::DocStyle& style);
    // Palavras de cada capítulo (o contador do app, com cache): o resumo e a
    // estimativa de páginas usam.
    std::function<int(const QString& chapterId)> chapterWords;

signals:
    void exportRequested(const Exporter::Selection& selection);
    // Bíblia do universo no formato escolhido. Independe do que está marcado:
    // a bíblia é o universo inteiro, não um recorte do manuscrito.
    void bibleRequested(Exporter::Format format);

private:
    void buildTree();
    void applyTheme();
    void recomputeCount();
    void setAllChecked(bool checked);
    void onExportClicked();
    Exporter::Selection currentSelection() const;
    Exporter::Format currentFormat() const;
    void selectFormat(const QString& f);
    void setSingleDocument(bool single);
    // Liga/desliga as opções conforme formato e modo (o formato de editora só
    // existe em página fixa e num arquivo só; o roteiro troca por numerar as
    // cenas) e escreve a dica certa embaixo.
    void refreshOptions();
    void editSubmissionData();
    void setListOpen(bool open);
    void refreshPickLabel();
    void refreshSummary();
    void schedulePreview();
    void rebuildPreview();
    void stepPage(int delta);
    void refreshNav();
    int selectedWords(QString* firstManuscriptId = nullptr, int* firstManuscriptWords = nullptr) const;

    enum Role { IdRole = Qt::UserRole + 1, KindRole, MsRole };
    enum Kind { KindHeader, KindGroup, KindChapter, KindItem };

    ProjectModel* m_model;
    std::unique_ptr<Exporter> m_exporter;
    Exporter::PagePreview m_preview;
    int m_page = 0;
    QTimer* m_previewTimer = nullptr;

    ExportPreviewView* m_view = nullptr;
    QToolButton* m_prevBtn = nullptr;
    QToolButton* m_nextBtn = nullptr;
    QLabel* m_navLabel = nullptr;

    QList<QPushButton*> m_formatBtns;
    QPushButton* m_pickBtn = nullptr;
    QLabel* m_pickText = nullptr;
    QLabel* m_pickCaret = nullptr;
    QWidget* m_listBox = nullptr;
    QWidget* m_whatGroup = nullptr;
    class QVBoxLayout* m_sideLayout = nullptr;
    QTreeWidget* m_tree = nullptr;
    QLabel* m_countLabel = nullptr;
    QPushButton* m_singleBtn = nullptr;
    QPushButton* m_separateBtn = nullptr;
    QCheckBox* m_markersSwitch = nullptr;
    QWidget* m_markersRow = nullptr;
    QWidget* m_editorGroup = nullptr;
    QLabel* m_editorLabel = nullptr;
    QCheckBox* m_editorSwitch = nullptr;     // livro: formato de editora; roteiro: numerar as cenas
    QLabel* m_editorHint = nullptr;
    QPushButton* m_authorBtn = nullptr;
    QLabel* m_summary = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_bibleBtn = nullptr;

    Exporter::SubmissionInfo m_submission;
    QString m_format = QStringLiteral("pdf");
    bool m_single = true;
    bool m_listOpen = false;
    int m_totalLeaves = 0;
    bool m_building = false;
};
