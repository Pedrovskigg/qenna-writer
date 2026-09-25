#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSet>
#include <QString>
#include <QWidget>

class QComboBox;
class QHBoxLayout;
class QLabel;
class QMenu;
class QVBoxLayout;
class QScrollArea;
class QToolButton;
class QPushButton;
class QTimer;
class ProjectModel;
class DialogueStore;
class WordCounter;
class ElementsStore;
class ProjectInfoHover;
struct Chapter;
struct ManuscriptPart;

class ManuscriptPanel : public QWidget {
    Q_OBJECT
public:
    // Estilos da gaveta (arranjo). As ferramentas valem em todos.
    enum class Style { Classic, Rail, Spines, Showcase, Toc, Spine, Grid, Mosaic, Journey,
                       TitlePage, Reader, Illustrated, Seasons, Store, Box };
    enum Tool { ToolStatus, ToolHover, ToolParts, ToolResume, ToolStory, ToolPov,
                ToolVariations, ToolRhythm, ToolCount };

    // "Onde parei": o MainWindow grava a trilha e entrega pronta.
    struct ResumeEntry {
        QString manuscriptId;
        QString chapterId;
        int sceneIndex = -1;
        QString sentence;
        int position = -1;
        QDateTime when;
    };

    explicit ManuscriptPanel(ProjectModel* model, QWidget* parent = nullptr);

    void open();
    void closePanel();
    bool isPanelOpen() const { return isVisible(); }
    // Altura escolhida arrastando a borda de baixo (ou o canto). Sem isso, a
    // gaveta ocupa a altura toda, como sempre.
    bool heightIsUserSet() const { return m_desiredHeight > 0; }
    int desiredHeight() const { return m_desiredHeight; }

    // Fontes pra barrinha de proporção diálogo/narração por capítulo — sem
    // as duas, a barra simplesmente não aparece (painel continua utilizável
    // durante a construção, antes do MainWindow acabar de ligar tudo).
    void setDialogueStore(DialogueStore* store);
    void setWordCounter(WordCounter* counter);
    // Presença dos personagens (ficha no hover) e narradores (POV).
    void setElementsStore(ElementsStore* store);

    // Capítulo/cena aberto no editor — marca a linha e alimenta Mosaico,
    // Jornada e Ritmo. sceneIndex -1 = capítulo inteiro.
    void setCurrentLocation(const QString& chapterId, int sceneIndex);
    void setResumeTrail(const QList<ResumeEntry>& trail);
    // Leitor: até onde o cursor já chegou em cada capítulo (0…1), na revisão atual.
    void setVisitedProgress(const QHash<QString, double>& visited);
    // Cascata das linhas (gaveta abrindo, troca de estilo ou de livro).
    void playIntro(int delayMs = 70);

signals:
    void chapterActivated(QString manuscriptId, QString chapterId);
    void sceneActivated(QString manuscriptId, QString chapterId, int sceneIndex);
    void newChapterRequested(QString manuscriptId);
    void newManuscriptRequested();
    void renameManuscriptRequested(QString manuscriptId);
    void deleteManuscriptRequested(QString manuscriptId);
    void previewEreaderRequested(QString manuscriptId);
    void statsRequested(QString manuscriptId);
    void panelClosed();
    // O estilo mudou a largura da gaveta — o MainWindow reposiciona.
    void widthChanged();
    // Context menus
    void renameChapterRequested(QString chapterId);
    void deleteChapterRequested(QString chapterId);
    void renameSceneRequested(QString chapterId, int sceneIndex);
    void deleteSceneRequested(QString chapterId, int sceneIndex);
    void createVariationRequested(QString chapterId, int sceneIndex);
    void switchVariationRequested(QString manuscriptId, QString chapterId, int sceneIndex, QString variationId);
    void openChapterInRefMenuRequested(QString manuscriptId, QString chapterId);
    void openSceneInRefMenuRequested(QString manuscriptId, QString chapterId, int sceneIndex);
    void elementsPresentRequested(QString manuscriptId, QString chapterId);
    void sceneElementsPresentRequested(QString manuscriptId, QString chapterId, int sceneIndex);
    // "Continuar" do Onde parei: abre o lugar e põe o cursor na frase.
    void resumeRequested(QString manuscriptId, QString chapterId, int sceneIndex,
                         QString sentence, int position);
    // Leitor: "Começar nova revisão" — zera o quanto de cada capítulo já foi visto.
    void resetVisitedRequested(QString manuscriptId);
    // Clique na pilula de diálogo/narração de um capítulo — pede a janela de
    // estatísticas (ChapterStatsDialog), montada por quem escuta (MainWindow).
    // anchorGlobalPos: canto (borda direita do painel, topo da pilula clicada)
    // em coordenadas globais, pra a janela abrir ao lado do painel, alinhada
    // com o capítulo clicado, em vez de em cima do cursor.
    void chapterStatsRequested(QString manuscriptId, QString chapterId, QPoint anchorGlobalPos);
    // Drag&drop reorder
    void reorderChapterRequested(QString chapterId, int targetIndex);
    void reorderSceneRequested(QString chapterId, int srcIndex, int targetIndex);
    // Cena arrastada pra DENTRO de outro capítulo — soltando sobre uma cena de
    // lá (entra naquela posição) ou sobre o cabeçalho do capítulo (entra no
    // fim). Quem executa é MainWindow::moveSceneAcrossChapters, o mesmo método
    // que o Outline usa.
    void moveSceneToChapterRequested(QString srcChapterId, int srcIndex,
                                     QString dstChapterId, int dstIndex);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(class QDragEnterEvent* event) override;
    void dragMoveEvent(class QDragMoveEvent* event) override;
    void dragLeaveEvent(class QDragLeaveEvent* event) override;
    void dropEvent(class QDropEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onManuscriptsChanged();
    void onChaptersChanged();
    void onComboChanged(int index);
    void applyTheme();

private:
    // ---- montagem ----
    void rebuildList();          // refaz tudo (nome antigo: é o ponto de entrada)
    void rebuildHeader();
    void rebuildRail();
    void rebuildTop();
    void rebuildBody();
    void rebuildBottom();
    void buildListView(const QList<Chapter>& chs);
    void buildTocView(const QList<Chapter>& chs);
    void buildSpineView(const QList<Chapter>& chs);
    void buildGridView(const QList<Chapter>& chs);
    void buildMosaicView(const QList<Chapter>& chs);
    void buildJourneyView(const QList<Chapter>& chs);
    void buildTitlePageView(const QList<Chapter>& chs);
    void buildReaderView(const QList<Chapter>& chs);
    void buildIllustratedView(const QList<Chapter>& chs);
    void buildSeasonsView(const QList<Chapter>& chs);
    void buildStoreView(const QList<Chapter>& chs);
    void buildBoxView(const QList<Chapter>& chs);
    QWidget* makeBoxTop();
    QWidget* makeTitlePageHead();
    QWidget* makeStoreHead();
    QWidget* makeReaderTop();
    QWidget* makeSeasonsTop();
    QWidget* makeSagaMinis(QWidget* parent, QSize size, bool withLabel);
    QHash<QString, int> pageStarts(const QList<Chapter>& reading) const;
    QHash<QString, int> vignetteFamilies() const;
    QColor vignetteColor(const QString& chapterId) const;
    QPixmap chapterVignette(const Chapter& c, int family, QSize size, bool circle) const;
    void showVignettePicker(const QString& chapterId, const QPoint& globalPos);
    bool lastResumeHere(ResumeEntry* out) const;
    QString scenesText(const Chapter& c) const;
    void selectManuscript(const QString& id);
    QWidget* makeListChapterRow(const Chapter& c, const QColor& partColor);
    QWidget* makeListSceneRow(const Chapter& c, int sceneIdx, const QColor& partColor);
    QWidget* makePartHeader(const ManuscriptPart& part, int chapterCount, int words);
    QWidget* makeAddRow(const QString& text, const QString& kind);
    QWidget* wrapInPart(QWidget* row, const QColor& partColor);
    QWidget* makeStatusDot(const Chapter& c);
    QWidget* makeVariationBadge(const Chapter& c, int sceneIdx);
    QWidget* makeResumeCard();
    QWidget* makeStatusBar(const QList<Chapter>& chs);
    QWidget* makeStoryBar(const QList<Chapter>& chs);
    QWidget* makePovBar(const QList<Chapter>& chs);
    QWidget* makeRhythmBar(const QList<Chapter>& chs);
    QWidget* makeBookBlock(bool withCover);
    void applyStyleWidth();

    // ---- dados ----
    QString activeManuscriptId() const;
    void syncCombo();
    QList<Chapter> readingChapters() const;               // do manuscrito ativo, na ordem
    QList<Chapter> displayChapters() const;               // leitura ou história + filtros
    QString chapterRowText(const Chapter& c) const;
    QString chapterShortLabel(const Chapter& c) const;
    QString chapterTitleOnly(const Chapter& c) const;
    QString sceneTitle(const Chapter& c, int idx) const;
    int chapterWords(const QString& chapterId) const;
    int sceneWords(const QString& chapterId, int idx) const;
    double dialogueFraction(const QString& chapterId) const;
    QColor statusColor(const Chapter& c) const;             // cor dos estilos desenhados
    bool statusVisible() const;                            // ferramenta ligada
    QList<ManuscriptPart> validParts(const QList<Chapter>& chs) const;
    QHash<QString, int> partIndexByChapter(const QList<Chapter>& chs, const QList<ManuscriptPart>& parts) const;
    bool partsActive() const;
    void setParts(const QList<ManuscriptPart>& parts);
    void startPartAt(const QString& chapterId);
    void removePartStart(const QString& chapterId);
    void askNewPart(const QPoint& globalPos);
    void cycleStatus(const QString& chapterId);
    // Ordem da história: posição na história de cada capítulo da leitura e
    // quem está fora da maior subsequência crescente (os saltos de verdade).
    struct StoryOrder { QList<int> storyPos; QList<bool> jump; QList<int> byStory; int marked = 0; };
    StoryOrder storyOrder(const QList<Chapter>& chs) const;
    bool storyMode() const;
    // POV
    QString povOf(const Chapter& c) const;
    QString suggestedPov(const Chapter& c) const;
    QStringList narratorsInOrder(const QList<Chapter>& chs) const;
    QColor povColor(const QString& elementId, const QStringList& order) const;
    QString elementName(const QString& elementId) const;
    QPixmap elementAvatar(const QString& elementId, const QColor& color, int size) const;
    bool povBarVisible(const QList<Chapter>& chs) const;
    bool isCurrent(const QString& chapterId, int sceneIdx = -1) const;

    // ---- ferramentas / estilo ----
    void setStyle(Style s);
    void setTool(Tool t, bool on);
    bool tool(Tool t) const { return m_tools[t]; }
    void loadSettings();
    void showStyleMenu();
    static QString styleId(Style s);
    static Style styleFromId(const QString& id);
    static QString toolId(Tool t);

    // ---- hover ----
    void armHover(const QString& chapterId, const QRect& globalRect);
    void disarmHover();
    void showHoverCard();

    // ---- menus / arraste ----
    void showManuscriptContextMenu(const QString& manuscriptId, const QPoint& globalPos);
    void showChapterContextMenu(const QString& manuscriptId, const QString& chapterId, const QPoint& globalPos);
    void showSceneContextMenu(const QString& manuscriptId, const QString& chapterId, int sceneIndex, const QPoint& globalPos);
    void showPartContextMenu(const QString& partId, const QPoint& globalPos);
    void startChapterDrag(QWidget* sourceBtn, const QString& chapterId);
    void startSceneDrag(QWidget* sourceBtn, const QString& chapterId, int sceneIndex);
    void clearDropIndicator();
    void showDropIndicatorAt(QWidget* target, bool before);
    void wireRow(QWidget* w, const QString& kind, const QString& chapterId, int sceneIdx = -1);

    void applyHeaderStyles();
    void clearLayout(class QLayout* lay);

    ProjectModel* m_model;
    DialogueStore* m_dialogueStore = nullptr;
    WordCounter* m_wordCounter = nullptr;
    ElementsStore* m_elementsStore = nullptr;
    QComboBox* m_combo;
    // Tooltip rica dos itens do popup do combo (não o hover ativo da LeftBar
    // — instância própria, mostra o manuscrito sob o mouse no popup).
    ProjectInfoHover* m_comboItemHover = nullptr;
    QTimer* m_comboHoverOpenTimer = nullptr;
    QTimer* m_comboHoverCloseTimer = nullptr;
    QString m_comboHoverPendingManuscriptId;
    QVBoxLayout* m_listLayout;
    QScrollArea* m_scroll;
    QWidget* m_header = nullptr;
    QLabel* m_headerTitle = nullptr;
    QToolButton* m_styleBtn = nullptr;
    QToolButton* m_addMsBtn = nullptr;
    QWidget* m_actionBar = nullptr;
    QPushButton* m_createChapterBtn = nullptr;
    QList<QToolButton*> m_headerIconBtns;
    QWidget* m_rail = nullptr;
    QVBoxLayout* m_railLayout = nullptr;
    QWidget* m_top = nullptr;
    QVBoxLayout* m_topLayout = nullptr;
    QWidget* m_bottom = nullptr;
    QVBoxLayout* m_bottomLayout = nullptr;
    QLabel* m_rhythmTip = nullptr;

    // Estilo e ferramentas (globais, QSettings)
    Style m_style = Style::Rail;
    bool m_tools[ToolCount] = {};
    bool m_storyMode = false;         // Ordem da história: Leitura (false) | História
    QString m_statusFilter;           // "" = todos; "none" = sem status
    QString m_povFilter;              // Element::id
    int m_povReading = 0;             // "Ler a linha": 0 = ainda não começou
    QSet<QString> m_collapsedParts;
    QString m_curChapterId;
    int m_curScene = -1;
    QList<ResumeEntry> m_resume;
    QHash<QString, double> m_visited;
    bool m_pickerBookMode = false;    // Trocar desenho: "O livro todo" (true) ou só o capítulo

    // Alça de largura (borda direita), uma largura por estilo
    QWidget* m_resizeHandle = nullptr;        // borda direita: largura
    QWidget* m_resizeHandleBottom = nullptr;  // borda de baixo: altura
    QWidget* m_resizeHandleCorner = nullptr;  // canto: as duas
    int m_resizeAxes = 0;                     // 1 = largura, 2 = altura, 3 = as duas
    int m_resizeStartX = 0;
    int m_resizeStartY = 0;
    int m_resizeStartW = 0;
    int m_resizeStartH = 0;
    int m_desiredHeight = 0;
    void loadBookColors();

    // Hover
    QTimer* m_hoverTimer = nullptr;
    QString m_hoverChapterId;
    QRect m_hoverRect;
    QLabel* m_hoverCard = nullptr;

    // Drag state
    QPoint m_dragStartPos;
    QString m_pressedChapterId;   // se != "" e sceneIdx == -1 → drag de capítulo
    int m_pressedSceneIndex = -1; // se >= 0 → drag de cena (em m_pressedChapterId)
    QWidget* m_dropIndicator = nullptr;
};
