#pragma once

#include "MemoriesStore.h"

#include <QPoint>
#include <QString>
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
    void setElementsStore(ElementsStore* s) { m_elements = s; }

signals:
    // Pedido pra abrir um documento no editor e saltar até o trecho comentado.
    void openMarkerRequested(QString docKey, int start, int end, QString text);
    // Pedido pra abrir a fonte de uma memória no editor (com o trecho marcado).
    void openMemoryInEditorRequested(MemoriesStore::Memory mem);
    // Pedido pra abrir a fonte de uma memória no RefMenu (com o trecho marcado).
    void openMemoryInRefRequested(MemoriesStore::Memory mem);

public slots:
    void refresh();

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyTheme();

private:
    enum class Tab { Comments = 0, Notes = 1, Names = 2, Memories = 3 };
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
                             const QString& text, QWidget* parent);
    // Clique numa memória → menu (abrir no editor / no refmenu).
    void showMemoryActions(const QString& memId, const QPoint& globalPos);
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

    bool m_dragging = false;
    QPoint m_dragOffset;

    // Resize por card: alça no rodapé que ajusta a altura visível do trecho.
    QWidget* m_resizingGrip = nullptr;
    QLabel* m_gripQuote = nullptr;
    int m_gripStartY = 0;
    int m_gripStartH = 0;
};
