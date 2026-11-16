#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTextBlock>
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
        BookDoc,        // Manuscrito completo: todos os capítulos do livro numa folha só
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

    // ---- Manuscrito completo (16/11/2026, ideia do Pe) ----
    // O livro inteiro aberto no editor, capítulo atrás de capítulo, cada um
    // começando numa abertura protegida (BookSeams). Por baixo os capítulos
    // continuam separados: o que se escreve volta pro capítulo certo, e só os
    // capítulos mexidos são regravados. As ferramentas de capítulo (corretor,
    // presença, diálogos, marcadores) olham só o capítulo do cursor, o "escopo".
    bool isBook() const { return m_viewMode.type == BookDoc; }
    // Abre o livro levando o cursor junto (do capítulo ou cena aberto desse livro).
    void openBook(const QString& manuscriptId);
    // Volta a um capítulo por vez: o do cursor, no mesmo ponto do texto.
    void leaveBook();
    // Fora do livro: o próprio viewMode. No livro: o capítulo do cursor como ChapterDoc.
    ViewMode scopeViewMode() const;
    QString scopeKey() const { return keyFor(scopeViewMode()); }
    QString focusChapterId() const { return isBook() ? m_focusChapterId : QString(); }
    // Trecho do documento que é do escopo (fora do livro: o documento todo).
    int scopeStart() const;
    int scopeEnd() const;
    QString scopePlainText() const;
    bool inScope(const QTextBlock& b) const;
    // Chave do capítulo naquela posição do editor (fora do livro: activeKey()).
    QString keyAt(int position) const;
    QString chapterAt(int position) const;
    // O editor mostra esse documento? (no livro: qualquer capítulo dele)
    bool showsKey(const QString& key) const;
    // Chaves de todos os capítulos do livro aberto (vazio fora do livro).
    QStringList bookKeys() const;
    struct BookPart { QString chapterId; QString key; int firstBlock = 0; int firstPos = 0; int endPos = 0; };
    QVector<BookPart> bookParts() const;
    // No livro: põe o cursor no começo do capítulo (ou da cena) e rola até a abertura.
    bool scrollToChapter(const QString& chapterId, int sceneIndex = -1);
    // No livro: a quebra de cena k (contada no livro todo) é a quebra local de qual capítulo.
    int bookBreakLocal(int breakIndex, QString* chapterId) const;

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
    // No livro: o cursor passou pra outro capítulo (o escopo das ferramentas mudou).
    void scopeChanged(QString previousChapterId);
    // No livro: um capítulo foi gravado; doc é ele sozinho (pros marcadores).
    void chapterDocFlushed(QString key, QTextDocument* doc);
    // No livro: uma edição na abertura de capítulo foi barrada ou desfeita.
    void bookEditBlocked(QString message);

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

    // ---- livro ----
    struct Seam { QTextBlock block; QString chapterId; };
    QString buildBookHtml();
    void stampBookSeams();
    bool rescanSeams();                 // refaz m_seams; false = a costura quebrou
    void onBookContentsChange(int pos, int removed, int added);
    void checkBookStructure();
    void repairBook();
    void rebuildBook(bool keepCursor);
    void updateFocusChapter();
    void markBookDirtyRange(int from, int to);
    void flushBook();
    QString extractChapterHtml(int seamIndex, QTextDocument* out) const;
    int seamIndexAt(int position) const;
    int seamIndexOf(const QString& chapterId) const;
    int partStart(int seamIndex) const;
    int partEnd(int seamIndex) const;
    bool rangeTouchesSeam(int from, int to) const;
    bool isSeamPosition(int position) const;
    void deleteAcrossSeams(int from, int to);
    bool bookGuardKey(class QKeyEvent* key);
    bool bookGuardInputMethod(class QInputMethodEvent* ev);
    void placeCursorInChapter(const QString& chapterId, int relPos, int sceneIndex);
    void onModelChaptersChanged();
    QVector<Seam> m_seams;
    QStringList m_bookChapterIds;       // ordem esperada das aberturas
    QSet<QString> m_bookDirty;          // capítulos com texto novo ainda não gravado
    QString m_focusChapterId;
    bool m_bookRepairing = false;
    bool m_bookCheckPending = false;
    mutable QVector<QPair<QString, int>> m_breakTable;   // quebra k → (capítulo, índice local)
    mutable int m_breakTableRev = -1;
    mutable int m_breakTableBlocks = -1;
    int m_seamsBlockCount = -1;         // blockCount da última varredura da costura
    void scrollCursorToThird(bool retry);
    void replaceOpenTab(const QString& oldKey);

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
