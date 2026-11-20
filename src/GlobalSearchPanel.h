#pragma once

#include "EditorHost.h"

#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

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
// Começando com "#", busca por tag livre (15/11/2026): o projeto inteiro, não só
// o mapa (documentos de gaveta, lugares e sistemas do Criador de Mundos e as
// seções deles, memórias do Pensário).
class GlobalSearchPanel : public QWidget {
    Q_OBJECT
public:
    enum ResultType {
        RChapter,
        RScene,
        RManuscript,
        RDrawerItem,
        RTagName,     // uma tag do projeto (clicar refina a busca nela)
        RTagged,      // algo que tem a tag (abre no lugar dele)
    };

    // Algo com tag, montado pela MainWindow (que conhece todos os armazéns).
    struct TagEntry {
        QString title;
        QString meta;          // "Personagens", "Lugar · Criador de Mundos"...
        QStringList tags;
        std::function<void()> open;
    };
    std::function<QVector<TagEntry>()> tagEntries;

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
        // Tags
        QString tagName;
        int tagCount = 0;
        int entryIndex = -1;
        QString meta;
    };

    GlobalSearchPanel(ProjectModel* model, DocCache* cache,
                      const QString& projectRoot, QWidget* parent);

    void openPanel();
    // Abre já com um texto na busca (a gaveta de Manuscritos, estilo Comando,
    // oferece "buscar no texto do livro" quando o nome não acha nada).
    void openPanelWith(const QString& query);
    void closePanel();
    // Pendurada no campo de busca da barra de cima (19/11/2026): sem o campo
    // próprio nem o ✕; quem digita é a barra (setQuery) e as setas e o Enter
    // chegam por navKey. A MainWindow dá a largura e a posição.
    void setAttached(bool on);
    bool isAttached() const { return m_attached; }
    void setQuery(const QString& query);
    void navKey(int key);
    void setProjectRoot(const QString& root) { m_projectRoot = root; }
    void applyTheme();

signals:
    void openRequested(EditorHost::ViewMode viewMode, QString query);
    // Pendurada: clicar numa tag refina a busca; a barra de cima mostra o texto novo.
    void queryEdited(const QString& query);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onQueryChanged(const QString& q);
    void onFilterClicked();
    void onResultActivated(QListWidgetItem* item);
    void runSearch();
    void runTagSearch(const QString& needle);

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
    QVector<TagEntry> m_tagEntries;
    bool m_tagMode = false;
    bool m_attached = false;

    QTimer* m_debounce = nullptr;
};
