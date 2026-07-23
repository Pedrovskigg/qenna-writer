#pragma once

#include "DialogueStore.h"
#include "MemoriesStore.h"

#include <QHash>
#include <QPixmap>
#include <QPoint>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QToolButton;
class QComboBox;
class QLineEdit;
class QVBoxLayout;
class QScrollArea;
class QStackedWidget;
class ProjectModel;
class MarkerStore;
class NotesStore;
class NoteEditPopup;
class NameGenerator;
class MapPanel;
class MapPinsStore;
class ElementsStore;
struct Chapter;

// Pensário — painel "auxiliar criativo" do Mira 2 (Pensarium no i18n).
// Reúne ferramentas de apoio à escrita que não são o texto em si. Quatro abas:
// Comentários (agregador de marcadores comentados — funcional), Notas, Nomes e
// Mapa (placeholders, fatias futuras). Painel flutuante filho do container do
// editor; arrastável pelo header; geometria simples (ancora à direita ao abrir).
class PensarioPanel : public QWidget {
    Q_OBJECT
public:
    PensarioPanel(MarkerStore* markers, ProjectModel* model, NotesStore* notes,
                  QWidget* parent = nullptr);
    ~PensarioPanel() override;

    void togglePanel();
    void openPanel();
    void closePanel();
    bool isPanelOpen() const;
    void openMap();

    // Altura da TopToolbar flutuante — o painel ancora logo abaixo dela.
    void setTopInset(int px) { m_topInset = px; }
    void setMapPinsStore(MapPinsStore* s) { m_mapPins = s; }
    void setMemoriesStore(MemoriesStore* s);
    void setDialogueStore(DialogueStore* s);
    void setElementsStore(ElementsStore* s);
    // Chamado pelo MainWindow toda vez que o capítulo/cena aberto no editor
    // muda. Enquanto o usuário não mexer manualmente no filtro "Cap.: " da
    // aba Diálogos, ele acompanha o capítulo atual — abrir a aba já mostra
    // as falas de onde o usuário está, em vez de "Todos" (caro em projeto
    // grande). Depois da primeira escolha manual, para de seguir.
    void setCurrentChapterId(const QString& chapterId);
    // Projeto novo carregado: volta o filtro da aba Diálogos pro padrão
    // (segue o capítulo atual outra vez, paginação zerada) — senão a escolha
    // manual do projeto anterior (ex.: "Todos") vazava pro projeto seguinte.
    void resetDialogueFilterState();

signals:
    // Pedido pra abrir um documento no editor e saltar até o trecho comentado.
    void openMarkerRequested(QString docKey, int start, int end, QString text);
    // Pedido pra abrir a fonte de uma memória no editor (com o trecho marcado).
    void openMemoryInEditorRequested(MemoriesStore::Memory mem);
    // Pedido pra abrir a fonte de uma memória no RefMenu (com o trecho marcado).
    void openMemoryInRefRequested(MemoriesStore::Memory mem);
    // Pedido pra abrir a fonte de um diálogo detectado no editor.
    void openDialogueInEditorRequested(DialogueStore::Dialogue dlg);
    // Botão de "load" na aba Diálogos: pede pra varrer todos os capítulos
    // do projeto em lote (mesma ideia do rescan de presença por cena).
    void rescanAllDialoguesRequested();

public slots:
    void refresh();
    // Progresso do scan em lote pedido acima — quem chama (MainWindow) avisa
    // a cada capítulo processado. running=false com done==total marca o fim.
    void setDialogueScanState(bool running, int done, int total);

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyTheme();

private:
    enum class Tab { Comments = 0, Notes = 1, Names = 2, Memories = 3, Dialogues = 4 };
    enum class SortMode { Chapters, Creation };
    enum class NameCategory { Character, Place, Weapon };
    enum class Gender { Female, Male };

    void buildUi();
    void selectTab(Tab tab);
    void rebuildComments();
    QWidget* buildCommentCard(const QString& docKey, const QString& color,
                              const QString& comment, const QString& text,
                              int start, int end, const QString& origin = QString());
    QWidget* buildNotesPage();
    void rebuildNotes();
    QWidget* buildNoteCard(const QString& id, const QString& color,
                           const QString& title, const QString& text);
    void ensureNotePopup();
    void openNoteCreate();
    void openNoteEditById(const QString& id);
    void openMapPanel();
    QWidget* buildNamesPage();
    // Memórias do projeto: lista filtrável (todas / projeto / por personagem).
    QWidget* buildMemoriesPage();
    void rebuildMemories();
    QWidget* buildMemoryCard(const QString& memId, const QString& title,
                             const QString& text, const QStringList& tags, QWidget* parent);
    // Chips multi-select de tags (facet ortogonal ao filtro de destino acima).
    void rebuildMemTagChips(const QVector<MemoriesStore::Memory>& all);
    // Clique numa memória → menu (abrir no editor / no refmenu).
    void showMemoryActions(const QString& memId, const QPoint& globalPos);
    // Diálogos detectados automaticamente: lista filtrável (todos / por
    // personagem que fala) + chips de "também fala nessa cena".
    QWidget* buildDialoguesPage();
    void rebuildDialogues();
    void rebuildDialoguePresenceChips(const QVector<DialogueStore::Dialogue>& all);
    QWidget* buildDialogueCard(const DialogueStore::Dialogue& dlg, const QString& speakerName,
                               const QString& originLabel, QWidget* parent);
    // Miniatura circular do personagem (foto real se tiver; senão um círculo
    // colorido — cor derivada do id, estável — com a inicial do nome).
    // Cacheada em m_avatarCache — em manuscritos grandes, o mesmo personagem
    // aparece em centenas/milhares de cards de diálogo, e decodificar a
    // foto (base64 -> QPixmap) e redesenhar o círculo a cada card sozinho já
    // é fatia relevante do travamento ao entrar/filtrar a aba.
    QPixmap characterAvatar(const QString& elementId, int size) const;
    // Avatar placeholder para diálogo sem locutor — círculo neutro (não a
    // paleta hash-por-id de characterAvatar), pra não parecer visualmente
    // igual ao caso de personagem órfão/deletado (mesmo estilo "sem foto"
    // de characterAvatar(""), mas semanticamente outra coisa).
    QPixmap unattributedAvatar(int size) const;
    // Rótulo do locutor de um diálogo pro card/estatísticas: nome do
    // personagem, ou "Sem locutor" se characterId vazio. Ponto único pra não
    // duplicar essa checagem em vários lugares (card, "Diálogo mais longo").
    QString dialogueSpeakerLabel(const QString& characterId) const;
    // Diálogo bate no filtro de origem (capítulo/cena) atual? Extraído do
    // corpo de rebuildDialogues pra ser reaproveitado no cálculo do badge de
    // "Diálogos sem atribuição" em rebuildDialoguePresenceChips.
    bool dialogueMatchesOriginFilter(const DialogueStore::Dialogue& d) const;
    // Clique direito num card de diálogo → "Alterar locutor" → esta lista
    // (todos os personagens do projeto, não só quem já foi detectado
    // falando) — corrige atribuições erradas do detector (heurística de
    // proximidade às vezes pega quem é CITADO, não quem fala).
    void showChangeSpeakerPopup(const QString& dlgId, const QPoint& globalPos);
    void setNameCategory(NameCategory c);
    void updateGenderVisibility();
    void generateNames();
    void copyName(const QString& name);
    QWidget* buildPlaceholderPage(const QString& title, const QString& subtitle);
    QString docTitleForKey(const QString& docKey) const;
    const Chapter* chapterForKey(const QString& docKey) const;
    // Rótulo de origem completo ("Capítulo X" ou "Capítulo X • Cena Y").
    QString originLabel(const QString& docKey, int sceneIndex) const;
    // Posição do doc na ordem da obra (capítulos primeiro, na ordem; resto ao fim).
    int rankForKey(const QString& docKey) const;
    void ancorRight();

    MarkerStore* m_markers = nullptr;
    ProjectModel* m_model = nullptr;
    NotesStore* m_notesStore = nullptr;
    MemoriesStore* m_memories = nullptr;
    DialogueStore* m_dialogues = nullptr;
    ElementsStore* m_elements = nullptr;
    Tab m_tab = Tab::Comments;
    SortMode m_sortMode = SortMode::Chapters;
    int m_topInset = 0;
    bool m_positioned = false;

    QWidget* m_header = nullptr;
    QLabel* m_title = nullptr;
    QToolButton* m_sortBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;

    QToolButton* m_tabComments = nullptr;
    QToolButton* m_tabNotes = nullptr;
    QToolButton* m_tabMemories = nullptr;
    QToolButton* m_tabDialogues = nullptr;
    QToolButton* m_namesBtn = nullptr; // acesso discreto ao gerador, no header
    QToolButton* m_mapBtn = nullptr;   // acesso ao painel do mapa, no header
    MapPanel* m_mapPanel = nullptr;
    MapPinsStore* m_mapPins = nullptr;

    QStackedWidget* m_stack = nullptr;
    QScrollArea* m_commentsScroll = nullptr;
    QWidget* m_commentsInner = nullptr;
    QVBoxLayout* m_commentsLay = nullptr;

    QScrollArea* m_notesScroll = nullptr;
    QWidget* m_notesInner = nullptr;
    QVBoxLayout* m_notesLay = nullptr;
    NoteEditPopup* m_notePopup = nullptr;
    QString m_editingNoteId; // vazio = criando nova

    NameGenerator* m_nameGen = nullptr;
    NameCategory m_nameCategory = NameCategory::Character;
    QToolButton* m_catChar = nullptr;
    QToolButton* m_catPlace = nullptr;
    QToolButton* m_catWeapon = nullptr;
    QComboBox* m_styleCombo = nullptr;
    QToolButton* m_genBtn = nullptr;
    Gender m_gender = Gender::Female;
    QWidget* m_genderRow = nullptr;
    QToolButton* m_genFem = nullptr;
    QToolButton* m_genMasc = nullptr;
    QWidget* m_filterRow = nullptr;
    QLineEdit* m_prefixEdit = nullptr;
    QLineEdit* m_suffixEdit = nullptr;
    QLabel* m_nameStatus = nullptr;
    QScrollArea* m_namesScroll = nullptr;
    QWidget* m_namesInner = nullptr;
    QVBoxLayout* m_namesLay = nullptr;

    QScrollArea* m_memoriesScroll = nullptr;
    QWidget* m_memoriesInner = nullptr;
    QVBoxLayout* m_memoriesLay = nullptr;
    // Filtro da aba de Memórias: "all" | "project" | <elementId de personagem>.
    QString m_memFilter = QStringLiteral("all");
    // Facet ortogonal: tags marcadas (vazio = não filtra por tag).
    QSet<QString> m_memTagFilter;

    QScrollArea* m_dialoguesScroll = nullptr;
    QWidget* m_dialoguesInner = nullptr;
    QVBoxLayout* m_dialoguesLay = nullptr;
    // Filtro de capítulo/cena: "" (todos os capítulos) | chapterId (capítulo
    // inteiro) | "chapterId::sceneIndex" (cena específica) — combina em E com
    // o picker de "também fala" (ex.: "quem fala com a Klara no capítulo 5").
    QString m_dialogueOriginFilter;
    // Facet ortogonal: só mostra falas de cenas onde estes personagens
    // TAMBÉM têm fala salva (proxy de presença — mais barato que reescanear
    // o texto da cena, e os dados já estão no DialogueStore).
    QSet<QString> m_dialoguePresenceFilter;
    // Card de estatísticas (quem mais fala, capítulo com mais diálogo,
    // diálogo mais longo) — recolhido por padrão, atrás de um botão, pra
    // não poluir a aba com algo sempre visível.
    bool m_dialogueStatsExpanded = false;
    // Capítulo atualmente aberto no editor (setCurrentChapterId) — usado só
    // pra saber o default do filtro enquanto o usuário não escolhe um na mão.
    QString m_currentChapterId;
    bool m_dialogueOriginFilterUserSet = false;
    // Paginação do filtro "Todos os capítulos" — reseta pra kDialoguePageSize
    // toda vez que o filtro (origem ou presença) muda.
    int m_dialogueVisibleCount = 0;
    // Botão de scan em lote + estado de progresso (persistido aqui pra
    // sobreviver a um rebuildDialogues() no meio do scan — ex.: usuário mexe
    // num filtro enquanto o lote roda).
    QToolButton* m_dlgScanBtn = nullptr;
    bool m_dialogueScanRunning = false;
    int m_dialogueScanDone = 0;
    int m_dialogueScanTotal = 0;
    // Cache de characterAvatar(), chave "elementId:size". Limpa inteira a
    // cada ElementsStore::changed() — mais barato que rastrear qual
    // personagem mudou de foto, e edição de foto é rara comparado a quanto
    // a aba de Diálogos é reconstruída.
    mutable QHash<QString, QPixmap> m_avatarCache;

    bool m_dragging = false;
    QPoint m_dragOffset;

    // Resize por card: alça no rodapé que ajusta a altura visível do trecho.
    QWidget* m_resizingGrip = nullptr;
    QLabel* m_gripQuote = nullptr;
    int m_gripStartY = 0;
    int m_gripStartH = 0;
};
