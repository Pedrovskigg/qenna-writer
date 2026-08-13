#pragma once

#include "ElementsStore.h"
#include "PresenceTypes.h"

#include <QHash>
#include <QPoint>
#include <QString>
#include <QWidget>
#include <functional>

class QLabel;
class QToolButton;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;
class QHBoxLayout;
class ProjectModel;
class DocCache;
class DialogueStore;
class TerritorioStore;
class WordCounter;
struct DrawerItem;

// Painel de Estatísticas — visão global do elenco/manuscrito (Fase 1: fileira
// de avatares + barras de participação) e drill-down por personagem (foto,
// nome, doc real da ficha/gaveta embutido, resumo de presença). Segue o
// mesmo padrão de painel flutuante do PensarioPanel (QWidget filho do
// container, ancorado à direita, tema reativo). Substitui aos poucos o
// antigo Modo Consistência do DrawerListPanel — ver plano em
// stats-panel-feature (memória do projeto).
class StatsPanel : public QWidget {
    Q_OBJECT
public:
    explicit StatsPanel(ProjectModel* model, ElementsStore* elements, QWidget* parent = nullptr);

    void setDocCache(DocCache* cache) { m_cache = cache; }
    void setDialogueStore(DialogueStore* s) { m_dialogues = s; }
    void setTerritorioStore(TerritorioStore* s) { m_territorioStore = s; }
    void setWordCounter(WordCounter* wc) { m_wordCounter = wc; }
    void setProjectRoot(const QString& root) { m_projectRoot = root; }

    void togglePanel();
    void openPanel();
    void closePanel();
    bool isPanelOpen() const;

    // Altura da TopToolbar flutuante — o painel ancora logo abaixo dela
    // (mesmo raciocínio do PensarioPanel::setTopInset).
    void setTopInset(int px) { m_topInset = px; }

    // Mesma forma do PresenceProvider de DrawerListPanel/TimelinePanel —
    // fn(charNames, outResults, outTotalScenes, outTotalChapters).
    using PresenceProvider = std::function<void(
        const QStringList&,
        QHash<QString, CharPresenceResult>*,
        int* /*totalScenes*/,
        int* /*totalChapters*/)>;
    void setPresenceProvider(PresenceProvider fn) { m_presenceProvider = std::move(fn); }

public slots:
    void refresh();

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyTheme();

private:
    void buildUi();
    QWidget* buildOverviewPage();
    QWidget* buildCharacterPage();
    void rebuildOverview();
    void rebuildChapterBars();
    void rebuildOverviewStats();
    void openChapterStats(const QString& manuscriptId, const QString& chapterId);
    void rebuildCharacterPage();
    void rebuildChemistry();
    void openChemistryPopup(const QString& otherElementId);
    void showStatusPicker(const QString& itemId, const QPoint& globalPos);
    void showLocationPicker(const QString& itemId, const QPoint& globalPos);
    // isOrigem=true edita origemTerritorioId; false edita localAtualTerritorioId.
    void showTerritorioPicker(const QString& itemId, const QPoint& globalPos, bool isOrigem);
    void refreshPresenceCache();
    void openCharacter(const QString& elementId);
    void backToOverview();
    void ancorRight();
    const DrawerItem* drawerItemForElement(const QString& elementId) const;
    // Personagens "de verdade" do projeto: só quem tem um item numa gaveta de
    // personagem (drawerElementType == "character"), não qualquer Element solto
    // no ElementsStore — um elemento pode sobreviver órfão lá (sem item de
    // gaveta) depois de excluído/nunca vinculado, e não deve aparecer aqui.
    QList<Element> projectCharacters() const;

    ProjectModel* m_model = nullptr;
    ElementsStore* m_elements = nullptr;
    DocCache* m_cache = nullptr;
    DialogueStore* m_dialogues = nullptr;
    WordCounter* m_wordCounter = nullptr;
    QString m_projectRoot;

    PresenceProvider m_presenceProvider;
    QHash<QString, CharPresenceResult> m_presenceResults; // chave: nome em minúsculo
    int m_totalScenes = 0;
    int m_totalChapters = 0;

    int m_topInset = 0;
    bool m_positioned = false;

    QWidget* m_header = nullptr;
    QLabel* m_title = nullptr;
    QToolButton* m_backBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;

    QStackedWidget* m_stack = nullptr;

    // Página 0: overview
    QWidget* m_avatarStrip = nullptr;
    QHBoxLayout* m_avatarStripLay = nullptr;
    QWidget* m_participInner = nullptr;
    QVBoxLayout* m_participLay = nullptr;

    enum class ChapterMetric { Words, DialogueRatio };
    ChapterMetric m_chapterMetric = ChapterMetric::Words;
    QToolButton* m_chapterMetricBtn = nullptr;
    QWidget* m_chapterBarsHost = nullptr;
    QHBoxLayout* m_chapterBarsLay = nullptr;
    QLabel* m_overviewStatsLabel = nullptr;

    // Página 1: personagem
    QString m_currentElementId;
    QLabel* m_charPhoto = nullptr;
    QLabel* m_charName = nullptr;
    QLabel* m_charPresenceSummary = nullptr;
    QLabel* m_charDialogueSummary = nullptr;
    QPushButton* m_charStatusBtn = nullptr;
    QPushButton* m_charLocationBtn = nullptr;
    QPushButton* m_charOrigemBtn = nullptr;
    QPushButton* m_charLocalAtualBtn = nullptr;
    TerritorioStore* m_territorioStore = nullptr;
    QLabel* m_charInconsistencyWarning = nullptr;
    QWidget* m_charBondsList = nullptr;
    QVBoxLayout* m_charBondsLay = nullptr;

    enum class ChemistrySort { Scenes, Chapters, CrossDialogues };
    ChemistrySort m_chemSort = ChemistrySort::Scenes;
    QToolButton* m_chemSortBtn = nullptr;
    QWidget* m_chemList = nullptr;
    QVBoxLayout* m_chemLay = nullptr;

    // Arrastar o popup de diálogos do par (Química) pelo cabeçalho — ver
    // openChemistryPopup()/eventFilter().
    bool m_chemPopupDragging = false;
    QPoint m_chemPopupDragOffset;

    QLabel* m_charDoc = nullptr;
};
