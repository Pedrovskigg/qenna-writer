#pragma once

#include "EditorHost.h"

#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QToolButton;
class QHBoxLayout;
class QVBoxLayout;
class QTimer;
class DocCache;
class ProjectModel;

// Painel flutuante de busca global no projeto (Ctrl+Shift+F). Mostra resultados
// agrupados em "Direto" (match no título) e "Citado em..." (match no conteúdo).
// Filtros: Todos / Capítulos / Cenas / Gavetas.
class GlobalSearchPanel : public QWidget {
    Q_OBJECT
public:
    enum ResultType {
        RChapter,
        RScene,
        RManuscript,
        RDrawerItem,
    };

    struct Result {
        ResultType type;
        QString title;
        QString matchType;     // "title" | "content"
        // Chapter/Scene
        QString manuscriptId;
        QString chapterId;
        QString chapterTitle;
        int sceneIndex = -1;
        // DrawerItem
        QString drawerKey;
        QString drawerTitle;
        QString itemId;
    };

    GlobalSearchPanel(ProjectModel* model, DocCache* cache,
                      const QString& projectRoot, QWidget* parent);

    void openPanel();
    void closePanel();
    void setProjectRoot(const QString& root) { m_projectRoot = root; }
    void applyTheme();

signals:
    void openRequested(EditorHost::ViewMode viewMode, QString query);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onQueryChanged(const QString& q);
    void onFilterClicked();
    void onResultActivated(QListWidgetItem* item);
    void runSearch();

private:
    void rebuildFilterRow();
    void rebuildResultList();
    bool resultPassesFilter(const Result& r) const;
    QString typeMeta(const Result& r) const;
    QString htmlToPlainText(const QString& html) const;
    QString readItemHtml(const QString& drawerKey, const QString& itemId,
                        const QString& fileRel) const;
    QString readChapterHtml(const QString& manuscriptId, const QString& chapterId,
                            const QString& fileRel) const;

    ProjectModel* m_model = nullptr;
    DocCache* m_cache = nullptr;
    QString m_projectRoot;

    QLineEdit* m_input = nullptr;
    QToolButton* m_close = nullptr;
    QWidget* m_filterRow = nullptr;
    QHBoxLayout* m_filterLay = nullptr;
    QListWidget* m_resultList = nullptr;
    QLabel* m_emptyLabel = nullptr;

    QString m_filter = QStringLiteral("all");
    QString m_query;
    QVector<Result> m_results;

    QTimer* m_debounce = nullptr;
};
