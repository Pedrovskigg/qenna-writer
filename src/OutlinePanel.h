#pragma once

#include <QFrame>
#include <QHash>
#include <QList>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QHBoxLayout;
class ProjectModel;
class WordCounter;
class DialogueStore;
class ElementsStore;

// Tudo que um card mostra, já resolvido. O card não consulta store nenhum: quem
// monta a faixa é que junta ProjectModel + WordCounter + DialogueStore +
// ElementsStore e entrega pronto. Assim o card não sabe de onde vem nada e a
// mesma classe serve pro capítulo e pra cena.
struct OutlineCardData {
    QString chapterId;
    int sceneIndex = -1;      // -1 = o card é do capítulo
    QString heading;          // "Capítulo 1" ou "Cena 2"
    QString parentHeading;    // só no card grande em modo cena: "Capítulo 1"
    QString summary;
    QString statusId;
    QString statusLabel;
    QString statusColor;
    QString timeMarker;
    int words = 0;
    int dialogues = 0;
    int chapterWords = 0;     // pra barra de peso da cena dentro do capítulo
    QStringList characters;
    bool povOther = false;
    // O texto de `summary` não é uma descrição escrita pelo usuário, e sim as
    // primeiras palavras do próprio capítulo/cena. Muda só como o card pinta:
    // prévia é apagada e em itálico, pra não passar por descrição de verdade.
    bool summaryIsPreview = false;
    // Proporção diálogo/narração. -1 = desconhecida (sem DialogueStore), e aí a
    // barra não aparece em vez de mentir um zero.
    int dialogueWords = -1;
};


// Um card. `Big` é o card do capítulo (ou da cena promovida), `Small` é um card
// da fila de cenas.
class OutlineCard final : public QFrame {
    Q_OBJECT
public:
    enum Size { Big, Small };

    OutlineCard(Size size, const OutlineCardData& data, QWidget* parent = nullptr);

    const OutlineCardData& data() const { return m_data; }
    void setSelected(bool on);
    void setDimmed(bool on);

signals:
    void clicked(QString chapterId, int sceneIndex);
    void parentHeadingClicked(QString chapterId);
    void contextMenuRequested(QString chapterId, int sceneIndex, QPoint globalPos);
    void dragStarted(QString chapterId, int sceneIndex);

protected:
    void mousePressEvent(class QMouseEvent* event) override;
    void mouseMoveEvent(class QMouseEvent* event) override;
    void mouseReleaseEvent(class QMouseEvent* event) override;
    void contextMenuEvent(class QContextMenuEvent* event) override;

private:
    void build();
    void applyFrameStyle();

    OutlineCardData m_data;
    Size m_size;
    bool m_selected = false;
    bool m_dimmed = false;
    QPoint m_pressPos;
    bool m_pressed = false;
};

// A faixa de um capítulo: card grande à esquerda, fila de cenas à direita.
// É ela que recebe o drop de um card de cena — daí sai o pedido de reordenar
// (mesma fila) ou de mover de capítulo (fila de outro).
class OutlineStrip final : public QWidget {
    Q_OBJECT
public:
    explicit OutlineStrip(QWidget* parent = nullptr);

    // focusedScene: -1 mostra o capítulo no card grande; >= 0 promove a cena.
    void setContent(const OutlineCardData& chapter,
                    const QList<OutlineCardData>& scenes,
                    int focusedScene);

    QString chapterId() const { return m_chapterId; }

signals:
    void cardClicked(QString chapterId, int sceneIndex);
    void backToChapterRequested(QString chapterId);
    void cardContextMenuRequested(QString chapterId, int sceneIndex, QPoint globalPos);
    void reorderSceneRequested(QString chapterId, int srcIndex, int targetIndex);
    void moveSceneToChapterRequested(QString srcChapterId, int srcIndex,
                                     QString dstChapterId, int dstIndex);

protected:
    void dragEnterEvent(class QDragEnterEvent* event) override;
    void dragMoveEvent(class QDragMoveEvent* event) override;
    void dragLeaveEvent(class QDragLeaveEvent* event) override;
    void dropEvent(class QDropEvent* event) override;

private:
    // Posição de inserção na fila a partir de um ponto, e o card sob o cursor.
    int insertIndexAt(const QPoint& posInStrip) const;
    void showDropMarkerAt(int index);
    void clearDropMarker();

    QString m_chapterId;
    QHBoxLayout* m_root = nullptr;
    QWidget* m_bigHost = nullptr;
    QWidget* m_queueHost = nullptr;
    QHBoxLayout* m_queueLayout = nullptr;
    QLabel* m_queueLabel = nullptr;
    QWidget* m_dropMarker = nullptr;
    QList<OutlineCard*> m_sceneCards;
    int m_sceneCount = 0;
};

// Outline Mode: o manuscrito inteiro em faixas de card.
//
// Nenhum dado é próprio daqui — resumo, marcador de tempo, POV e tipo já viviam
// em Chapter/Scene; palavras vêm do WordCounter, diálogos do DialogueStore e
// quem está em cena do ElementsStore, que tem chave por capítulo E por cena. O
// resumo, em particular, é o MESMO campo que alimenta a descrição do evento na
// Timeline: editar aqui muda lá, sem código de sincronização.
class OutlinePanel final : public QWidget {
    Q_OBJECT
public:
    explicit OutlinePanel(ProjectModel* model, QWidget* parent = nullptr);

    void setWordCounter(WordCounter* counter);
    void setDialogueStore(DialogueStore* store);
    void setElementsStore(ElementsStore* store);

    // Texto puro de um doc, por chave "ch:<chapterId>" / "sc:<sceneId>" — o
    // mesmo resolvedor que a Timeline usa. É o que permite um card ter conteúdo
    // sem o usuário ter escrito descrição nenhuma.
    using DocTextResolver = std::function<QString(const QString&)>;
    void setDocTextResolver(DocTextResolver fn) { m_docTextResolver = std::move(fn); }

    // HTML de um doc, mesmas chaves do resolvedor de texto. Alimenta o painel
    // de leitura da direita: clicar num card mostra o capítulo/cena ali, com a
    // formatação de verdade, sem sair do Outline.
    using DocHtmlResolver = std::function<QString(const QString&)>;
    void setDocHtmlResolver(DocHtmlResolver fn) { m_docHtmlResolver = std::move(fn); }
    // Pasta do projeto: o visualizador precisa dela pra achar as imagens que o
    // HTML referencia por caminho relativo.
    void setProjectRoot(const QString& root);

signals:
    void closeRequested();
    // sceneIndex < 0 = o capítulo inteiro.
    void openRequested(QString manuscriptId, QString chapterId, int sceneIndex);
    void openInRefMenuRequested(QString manuscriptId, QString chapterId, int sceneIndex);
    void chapterDialoguesRequested(QString manuscriptId, QString chapterId);
    void reorderChapterRequested(QString chapterId, int targetIndex);
    void reorderSceneRequested(QString chapterId, int srcIndex, int targetIndex);
    void moveSceneToChapterRequested(QString srcChapterId, int srcIndex,
                                     QString dstChapterId, int dstIndex);

protected:
    void closeEvent(class QCloseEvent* event) override;
    void showEvent(class QShowEvent* event) override;
    // A área das faixas aceita o card GRANDE arrastado: é assim que se
    // reordena capítulo, que é a ordem do próprio livro.
    void dragEnterEvent(class QDragEnterEvent* event) override;
    void dragMoveEvent(class QDragMoveEvent* event) override;
    void dragLeaveEvent(class QDragLeaveEvent* event) override;
    void dropEvent(class QDropEvent* event) override;

private slots:
    void applyTheme();

private:
    void rebuild();
    // Agenda uma reconstrução pro próximo giro do event loop, coalescendo várias
    // numa só. Reconstruir na hora seria um use-after-free: uma edição chama o
    // modelo, o modelo emite chaptersChanged, e a reconstrução destruiria as
    // faixas com a pilha ainda dentro do widget que disparou a edição. Todo
    // caminho vindo de sinal passa por aqui.
    void scheduleRebuild();
    void syncCombo();
    QString activeManuscriptId() const;
    void updateSummaryLine();
    OutlineCardData buildChapterCard(const class Chapter& chapter) const;
    OutlineCardData buildSceneCard(const class Chapter& chapter, int sceneIndex) const;
    QStringList charactersForDoc(const QString& docKey) const;
    // Elenco do capítulo com presença por cena, e a nota de quem sumiu — quem
    // já apareceu antes no manuscrito e não está neste capítulo.
    int dialogueWordsFor(const QString& chapterId, int sceneIndex) const;
    // Primeiras palavras do texto, pra usar no lugar de uma descrição que o
    // usuário não escreveu. Cacheado por (chave do doc + contagem de palavras):
    // extrair texto puro custa um QTextDocument por card, e a grade é remontada
    // a cada digitada no editor. A contagem mudar é o sinal barato de que o
    // texto mudou — errar pra menos aqui só significa uma prévia levemente
    // velha, nunca um dado errado.
    QString previewText(const QString& linkKey, int words, int maxChars) const;
    mutable QHash<QString, QString> m_previewCache;
    void showCardMenu(const QString& chapterId, int sceneIndex, const QPoint& globalPos);
    // Carrega no painel da direita o capítulo (sceneIndex < 0) ou a cena.
    void loadPreview(const QString& chapterId, int sceneIndex);
    void clearPreview();
    // O que está sendo lido agora, pra sobreviver à remontagem das faixas.
    QString m_previewChapterId;
    int m_previewSceneIndex = -1;
    void promptRename(const QString& chapterId, int sceneIndex);
    void promptSummary(const QString& chapterId, int sceneIndex);

    ProjectModel* m_model;
    WordCounter* m_wordCounter = nullptr;
    DialogueStore* m_dialogueStore = nullptr;
    ElementsStore* m_elementsStore = nullptr;
    DocTextResolver m_docTextResolver;
    DocHtmlResolver m_docHtmlResolver;
    QString m_projectRoot;

    QComboBox* m_combo = nullptr;
    QLabel* m_summaryLine = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QWidget* m_header = nullptr;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_stripHost = nullptr;
    class QSplitter* m_splitter = nullptr;
    QWidget* m_previewPane = nullptr;
    QLabel* m_previewTitle = nullptr;
    QLabel* m_previewMeta = nullptr;
    class QTextBrowser* m_preview = nullptr;
    QLabel* m_previewEmpty = nullptr;
    QVBoxLayout* m_stripLayout = nullptr;
    QList<OutlineStrip*> m_strips;

    // Cena promovida ao card grande, por capítulo. Sobrevive à reconstrução —
    // senão editar qualquer coisa jogaria o foco de volta pro capítulo.
    QHash<QString, int> m_focusedScene;

    // Marcador horizontal de onde o capítulo arrastado vai cair.
    int chapterInsertIndexAt(const QPoint& posInHost) const;
    void showChapterDropMarkerAt(int index);
    void clearChapterDropMarker();
    QWidget* m_chapterDropMarker = nullptr;

    bool m_rebuilding = false;
    bool m_rebuildScheduled = false;
};
