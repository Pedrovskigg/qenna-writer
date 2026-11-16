#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

class QTextEdit;
class QTimer;
class DocCache;
class ProjectModel;

class EditorHost : public QObject {
    Q_OBJECT
public:
    enum ViewModeType {
        Disabled,
        ChapterDoc,
        SceneDoc,
        DrawerDoc,
        ManuscriptDoc,
    };

    struct ViewMode {
        ViewModeType type = Disabled;
        QString manuscriptId;
        QString chapterId;
        int sceneIndex = -1;
        QString itemId;

        bool operator==(const ViewMode& other) const;
        bool operator!=(const ViewMode& other) const { return !(*this == other); }
    };

    EditorHost(QTextEdit* editor, DocCache* cache, ProjectModel* model, QObject* parent = nullptr);

    QTextEdit* editor() const { return m_editor; }
    ViewMode viewMode() const { return m_viewMode; }

    void setProjectRoot(const QString& root);
    QString projectRoot() const { return m_projectRoot; }
    QString activeKey() const;
    static QString keyFor(const ViewMode& vm);

    void setViewMode(const ViewMode& vm);
    void disable() { setViewMode(ViewMode{}); }

    void syncEditorToCache();
    // Recarrega uma alteração externa já sincronizada, sem gravar o texto antigo. — Mira
    void reloadFromCache();

    void setFlushDebounceMs(int ms);

    // Voltar e Avançar entre os documentos abertos (15/11/2026, pedido do Pe: "tô no capítulo 14, abro um doc
    // pela gaveta e pra voltar tenho que achar o capítulo e rolar até onde estava"). Cada documento lembra o
    // cursor e a rolagem (também de uma sessão pra outra), então voltar cai exatamente onde parou.
    bool canGoBack() const;
    bool canGoForward() const;
    bool goBack();
    bool goForward();
    void rememberSpot();      // guarda o cursor e a rolagem do documento aberto (antes de fechar o projeto/app)

    // Abas (15/11/2026): os documentos abertos, na ordem em que foram abertos (um por documento; a cena
    // reaproveita a aba do capítulo). Fechar só tira da lista. Muda junto com historyChanged().
    QVector<ViewMode> openDocs() const;
    void openDocAt(int index);
    void closeDocAt(int index);

    // Variação da cena ativa. Operam quando viewMode é SceneDoc.
    QString createVariationForCurrentScene(const QString& label = QString());
    bool switchVariationForCurrentScene(const QString& variationId);
    bool deleteVariationForCurrentScene(const QString& variationId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void viewModeChanged();
    void historyChanged();    // Voltar/Avançar ficaram (in)disponíveis
    void contentFlushed(QString key);
    void contentLoaded();
    void hrInserted(QString chapterId); // atalho ---- confirmado; dispara sync imediato

private slots:
    void onEditorTextChanged();
    void flushPendingEditorContent();

private:
    void loadIntoEditor(const QString& html);
    void replaceContent(const QString& html);
    void normalizeRefAnchors();
    QString hydrateFromModel(const ViewMode& vm) const;
    bool tryConvertHrShortcut();
    bool tryPasteClean();
    struct Spot { int cursor = 0; int anchor = 0; int scroll = 0; };
    static QString spotKey(const ViewMode& vm);
    void saveSpot();
    void restoreSpot();
    void loadSpots();
    void persistSpots() const;
    bool isOpenable(const ViewMode& vm) const;
    bool travel(QVector<ViewMode>& from, QVector<ViewMode>& to);

    QTextEdit* m_editor;
    DocCache* m_cache;
    ProjectModel* m_model;
    ViewMode m_viewMode;
    QTimer* m_flushTimer;
    QString m_projectRoot;
    bool m_loadingContent = false;
    QHash<QString, Spot> m_spots;
    QVector<ViewMode> m_back, m_forward;
    QVector<ViewMode> m_open;      // as abas
    void openTarget(const ViewMode& vm);
    bool m_travelling = false;     // Voltar/Avançar em curso: a troca não entra no histórico como visita nova
    bool m_spotPending = false;    // a rolagem guardada ainda vai ser aplicada (a mão do usuário cancela)
};
