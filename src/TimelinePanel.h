#pragma once

#include "PresenceTypes.h"
#include "TimelineTracksTypes.h"
#include "TimelineTypes.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <functional>

class TimelineScene;
class TimelineView;
class ProjectModel;
class TerritorioStore;
class ElementsStore;
class TimelineBranchPopup;
class TimelineTracksView;
class TimelineBraidView;
class TimelineInspector;
class QStackedWidget;
class QButtonGroup;
class QPropertyAnimation;
class QLineEdit;

class TimelinePanel : public QWidget {
    Q_OBJECT
public:
    explicit TimelinePanel(QWidget* parent = nullptr);

    void setProjectRoot(const QString& root);
    void setProjectModel(ProjectModel* model);
    void setTerritorioStore(TerritorioStore* store) { m_territorioStore = store; }

    // Eventos com placeId == territorioId — usado pelo "O que aconteceu
    // aqui" do Criador de Mundos (filtro puro, não duplica dado).
    QList<TimelineEvent> eventsForPlace(const QString& territorioId) const;
    // Zera placeId de todo evento que apontava pro território excluído —
    // chamado pelo MainWindow ao excluir um Território no Criador de Mundos,
    // pra não deixar placeId órfão apontando pra um território inexistente.
    void clearPlaceReferences(const QString& territorioId);
    void setElementsStore(ElementsStore* store);
    // Resolve o texto de um doc vinculado (capítulo/cena/gaveta) p/ a descrição.
    void setDocTextResolver(std::function<QString(const QString&)> resolver);

    // Análise de presença por capítulo/cena (mesma do Drawer). Usada pra gerar
    // as trilhas automáticas de personagem com granularidade de cena.
    using PresenceProvider = std::function<void(
        const QStringList&, QHash<QString, CharPresenceResult>*, int*, int*)>;
    void setPresenceProvider(PresenceProvider fn) { m_presenceProvider = std::move(fn); }

    // Abre o popup de novo evento já pré-preenchido (ex.: criado a partir de um
    // trecho selecionado no editor). Usado pelo MainWindow.
    // title vazio = sugere a partir das primeiras palavras da descrição.
    // origin: "lousa" / "" (ver TimelineEvent::origin). Sem âncora → "Soltos".
    void promptNewEvent(const QString& description, const QString& marker,
                        const QString& title = QString(), const QString& origin = QString());
    // Evento criado a partir de uma seleção no editor: nasce no capítulo/cena
    // de onde veio (e na linha dele), na ordem do texto.
    void promptNewEventFromEditor(const QString& description, const QString& marker,
                                  const QString& chapterId, int sceneIndex,
                                  int textPos, int paragraph);

    // Capítulo/cena aberto no editor — selo "✎" na régua e manuscrito padrão.
    void setEditorLocation(const QString& manuscriptId, const QString& chapterId, int sceneIndex);

    // Força a resincronização das trilhas automáticas a partir do model,
    // mesmo com o painel escondido (os signals internos só resincronizam
    // se isVisible()). Usado depois de edições em lote feitas fora daqui
    // (ex.: Gerador de Timeline em Configurações).
    void refreshFromModel();

signals:
    void closeRequested();
    void exportEventAsDoc(TimelineEvent data);
    // "Abrir no editor" do painel lateral. textPos >= 0 = posiciona o cursor.
    void openInEditorRequested(const QString& manuscriptId, const QString& chapterId,
                               int sceneIndex, int textPos);
    void generatorRequested(); // menu "⋯" → Gerador de Timeline

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event)   override;
    void keyPressEvent(QKeyEvent* event)  override;
    void showEvent(QShowEvent* event)     override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    void applyTheme();
    void createTimeline();
    void editTimeline(const QString& id);
    void deleteTimeline(const QString& id);
    void createEventAt(const QPointF& scenePos);
    QString commitEvent(TimelineEvent e, const QPointF& scenePos); // cria o evento já resolvido; devolve o id
    void openEditPopup(const QString& eventId);
    void onExportEventAsDoc(const TimelineEvent& event);

private:
    void buildUi();
    // Dialog compartilhado de nome+cor+importância. Edita `def` in-place.
    // isNew muda só os textos (título/botão). Retorna true se confirmado.
    bool editTimelineDef(TimelineDef& def, bool isNew);
    void save() const;
    void load();
    void toggleViewMode();
    void toggleAxisMode();
    void refreshModeButtons();
    // Foco / Filtro por linha
    void rebuildFocusMenu();                 // repopula o menu com as linhas atuais
    void refreshFocusButtons();              // texto/estado dos botões de foco
    void setFocusTimeline(const QString& id); // id vazio = mostrar tudo
    void setFocusDepth(int depth);
    // Trilhas de personagem (legado) — toggle na topbar
    void toggleLegacyCharacterTracks(bool checked);
    // Filtro por personagem — repopula o menu e aplica o filtro selecionado
    void rebuildCharFilterMenu();
    void setCharFilter(const QString& characterId); // vazio = sem filtro
    // Filtro por território (M7) — mesmo padrão do filtro de personagem
    void rebuildTerritorioFilterMenu();
    void setPlaceFilter(const QString& territorioId); // vazio = sem filtro
    // Gera/atualiza as trilhas automáticas de personagem a partir do detector
    // de presença + roles. askSecondary = perguntar sobre papéis secundários.
    // Também gera conexões automáticas de copresença (mesma cena) entre
    // personagens elegíveis.
    void syncCharacterTimelines(bool askSecondary);
    // Gera/atualiza as trilhas automáticas "História"/"Flashback" a partir do
    // timeMarker dos capítulos + storyStartMarker do manuscrito.
    void syncStoryTimeline();

    // Capítulo/cena "batido" na Narrativa (não-Flashback), com o suficiente
    // pra detectar ramificações automáticas. Montado por syncStoryTimeline(),
    // consumido por syncAutoBranches() — struct de classe (não local a uma
    // função) só pra poder trafegar entre os dois métodos.
    struct AutoHit {
        int rank = 0; QString evId; QString title; QString marker;
        QString summary; QString docKey; QString linkedSceneId; QString manuscriptId;
        bool povOther = false;
    };

    // Ramificações automáticas da Timeline: detecta POV alternado/narrativa
    // fora de ordem dentro da Narrativa (nunca dentro do Flashback) e separa
    // em trilhas paralelas (`TimelineDef` kind=Parallel) sem exigir marcação
    // manual. Ver plano/memória "timeline-auto-branching-design". Chamado
    // logo depois de montar os eventos de story:main em syncStoryTimeline(),
    // mutando os mesmos events/conns/defs antes do push final pra cena.
    void syncAutoBranches(const QList<AutoHit>& mainHits,
                          const QHash<QString, QStringList>& presentByEvId,
                          QList<TimelineEvent>& events,
                          QList<TimelineConn>& conns,
                          QList<TimelineDef>& defs);
    // Persistência de decisões do popup de ambiguidade (evId -> branchId
    // escolhido), por projeto — nunca repergunta pro mesmo evento.
    QHash<QString, QString> loadBranchAssignments() const;
    void saveBranchAssignment(const QString& evId, const QString& branchId);
    // Enfileira o popup de desambiguação (só se o painel estiver visível —
    // ver comentário em syncAutoBranches). Enquanto não decidido, o hit fica
    // na ramificação de origem (reabsorvido, decisão não persistida ainda).
    void enqueueBranchAmbiguity(const QString& evId, const QString& evTitle,
                                const QStringList& candidateIds,
                                const QStringList& candidateLabels);

    // ── Timeline nova (Trilhos / Trança / painel lateral) ─────────────────────
    enum class NewMode { Tracks, Braid, Engine };
    void buildNewUi(QWidget* body);
    void applyNewTheme();
    void refreshNewUi();                       // reconstrói os dados e repinta
    Tracks::Data buildTracksData() const;
    QString currentManuscriptId() const;
    void setNewMode(NewMode m);
    void setLegacyUi(bool legacy);
    void selectEvent(const QString& id);
    void setInspectorOpen(bool open);
    void applyTracksFilter();
    void rebuildNewMenus();
    void refreshNewChips();
    void collapseSearch(bool clear);
    void onEventDropped(const QString& id, const QString& laneId, int col);
    void openEventInEditor(const QString& id);
    void fillMarker(const QString& id, const QString& marker);
    void undoLaneMove(const QString& id);
    void editTracksEvent(const QString& id);
    void createAtColumn(const QString& laneId, int col);
    void removeManualEvent(const QString& id);
    void openLaneColor(const QString& laneId, const QPoint& globalPos);
    void resolveThemeBoundLaneColors(); // linhas presas a uma cor do tema
    // Evento vivo (cena) por id; nullptr se não existe (ex.: capítulo sem data).
    const TimelineEvent* liveEvent(const QString& id) const;
    // Grava a mudança num evento vivo (cena + lista local) e salva.
    void updateLiveEvent(const TimelineEvent& e);

    bool      m_legacyUi = false;
    bool      m_staleWhileHidden = false;
    NewMode   m_newMode  = NewMode::Tracks;
    QString   m_msId;                          // manuscrito mostrado na Timeline nova
    QString   m_editorMsId, m_editorChapterId;
    int       m_editorScene = -1;
    QString   m_sel;                           // evento selecionado (painel aberto)
    Tracks::Data   m_tracksData;
    Tracks::Filter m_tf;
    // Linha escolhida pelo usuário pra eventos de capítulo/cena (arraste),
    // por id "story:..." — salva no timeline.json, a detecção não desfaz.
    QHash<QString, QString> m_laneOverrides;
    QHash<QString, QString> m_autoLaneOf;      // linha automática antes do override
    QHash<QString, QStringList> m_presentByEvId; // "story:..." → nomes presentes
    mutable QHash<QString, QImage> m_avatarCache;

    QWidget*            m_newTop      = nullptr;
    QWidget*            m_newBottom   = nullptr;
    QStackedWidget*     m_stack       = nullptr;
    TimelineTracksView* m_tracks      = nullptr;
    TimelineBraidView*  m_braid       = nullptr;
    TimelineInspector*  m_inspector   = nullptr;
    QWidget*            m_inspHolder  = nullptr;
    class QVariantAnimation* m_inspAnim = nullptr;
    class QToolButton*  m_msBtn       = nullptr;
    class QToolButton*  m_segTracks   = nullptr;
    class QToolButton*  m_segBraid    = nullptr;
    QWidget*            m_searchBox   = nullptr;
    class QToolButton*  m_searchBtn   = nullptr;
    QLineEdit*          m_searchEdit  = nullptr;
    QPropertyAnimation* m_searchAnim  = nullptr;
    class QToolButton*  m_chipChar    = nullptr;
    class QToolButton*  m_chipPlace   = nullptr;
    class QToolButton*  m_moreBtn     = nullptr;
    class QLabel*       m_statsLbl    = nullptr;
    class QToolButton*  m_noDateBtn   = nullptr;
    QWidget*            m_densityBox  = nullptr;
    QButtonGroup*       m_densityGrp  = nullptr;
    class QToolButton*  m_btnNewUi    = nullptr; // na barra antiga: volta pra UI nova

    TimelineScene*  m_scene        = nullptr;
    TimelineView*   m_view         = nullptr;
    QWidget*        m_toolbar      = nullptr;
    class QToolButton* m_btnView   = nullptr;
    class QToolButton* m_btnAxis   = nullptr;
    class QToolButton* m_btnFocus  = nullptr;
    class QToolButton* m_btnDepth  = nullptr;
    class QToolButton* m_btnAdd    = nullptr;   // "+" flutuante sobre o canvas
    class QMenu*       m_focusMenu = nullptr;
    class QToolButton* m_btnLegacyChars  = nullptr; // checkable — trilhas de personagem (legado)
    class QToolButton* m_btnCharFilter   = nullptr; // popup — filtro por personagem presente
    class QMenu*       m_charFilterMenu  = nullptr;
    QString            m_charFilterId;              // Element::id filtrado; vazio = sem filtro
    class QToolButton* m_btnPlaceFilter  = nullptr; // popup — filtro por território (M7)
    class QMenu*       m_placeFilterMenu = nullptr;
    QString            m_placeFilterId;              // TerritorioStore::Territorio::id filtrado; vazio = sem filtro

    QString         m_projectRoot;
    ProjectModel*   m_projectModel  = nullptr;
    TerritorioStore* m_territorioStore = nullptr;
    ElementsStore*  m_elementsStore = nullptr;
    std::function<QString(const QString&)> m_docTextResolver;
    PresenceProvider m_presenceProvider;

    // Popup de desambiguação de ramificações — fila, canto inferior-direito,
    // mesmo padrão do PresencePopup. Criado sob demanda (só quando o
    // algoritmo de fato encontra um caso ambíguo).
    TimelineBranchPopup* m_branchPopup = nullptr;
    TimelineBranchPopup* ensureBranchPopup();

    // dados — preenchidos nas etapas seguintes
    QList<TimelineDef>   m_timelines;
    QList<TimelineEvent> m_events;
    QList<TimelineConn>  m_connections;
};
