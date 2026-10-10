#pragma once

#include "LousaInk.h"
#include "LousaTypes.h"

#include <QDialog>
#include <QImage>
#include <QHash>
#include <QList>
#include <QPair>
#include <QPointer>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

#include <functional>

class LousaScene;
class LousaView;
class LousaDock;
class LousaActionBar;
class LousaMinimap;
class LousaCheatSheet;
class LousaSearchBar;
class CardItem;
class QKeyEvent;
class QLabel;
class QListWidget;
class QPushButton;
class QScrollArea;
class QTimer;
class QToolButton;

// Metadados de uma lousa (board) do projeto — os dados visuais (cards/
// zonas/conexões) ficam no arquivo apontado por `file`, não aqui.
struct LousaBoardMeta {
    QString id;
    QString name;
    QString file;   // nome do JSON dentro do projectRoot, ex. "canvas.json"
};

class LousaPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LousaPanel(QWidget* parent = nullptr);

    void setProjectRoot(const QString& root);
    void setProjectModel(class ProjectModel* model);
    void setElementsStore(class ElementsStore* store);
    // Quem lê o HTML de um documento/capítulo (o editor pode ter mudanças
    // ainda não gravadas). Chave = DocCache::itemKey / chapterKey.
    void setHtmlProvider(std::function<QString(const QString& docKey)> provider);
    void refreshDocCards();
    void refreshEmptyState();

    // Múltiplas lousas — usado pelo MainWindow para montar o seletor
    // acionado a partir da LeftBar quando há mais de uma lousa no projeto.
    QVector<LousaBoardMeta> boardList() const { return m_boards; }
    QString activeBoardId() const { return m_activeBoardId; }
    void switchToBoard(const QString& boardId);
    // Seletor com miniatura de cada lousa. Devolve o id ("" = cancelou).
    QString pickBoard(QWidget* parent, const QPoint& globalPos) const;

signals:
    void closeRequested();
    // Pedido para criar um evento na Timeline a partir de um card de texto.
    void createTimelineEventRequested(const QString& title, const QString& description);
    // Abrir um documento/capítulo no editor (DocCache::itemKey / chapterKey).
    void openDocKeyRequested(const QString& docKey);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* o, QEvent* e) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void applyTheme();

private:
    void buildUi();
    void buildChrome();
    void showBoardMenu();
    void toggleStash();
    void refreshStashUi();
    void reloadIcons();
    void save() const;
    void load();
    CanvasCard nextCardData(const QString& type) const;
    CanvasCard cardAt(const QString& type, const QPointF& center) const;
    QPointF viewCenter() const;

    // Criar coisas no quadro
    void createFromTool(const QString& kind);
    void createText(const QPointF& at);
    void createSymbol(const QPointF* at = nullptr);
    void createImage();
    void createImageCard(const QImage& img, const QPointF* at);   // foto com moldura e legenda
    void placeImage(const QImage& img, const QPointF* at);        // transparente vira adesivo
    void placeSticker(const QImage& img, const QPointF* at, const QString& outline,
                      qreal longest, bool remember);
    void showStickerPicker();
    void showNoteStyle(CardItem* card, const QPoint& globalPos);
    void setStickerLayer(CardItem* card, const QString& how);    // "front" | "back" | "behind"
    bool pasteImageFromClipboard();
    void pickDocForBoard();
    void pickCharacterForBoard();
    void placeFromPayload(const QString& payload, const QPointF* at);
    CardItem* placeCard(const CanvasCard& c);   // pushUndo já feito por quem chama
    QColor boardInk() const;                    // cor de texto que aparece no fundo atual

    // Múltiplas lousas — manifesto (lousas.json) + arquivo de dados por board.
    QString boardsManifestPath() const;
    QString activeBoardFile() const;
    int     boardIndexOf(const QString& boardId) const;
    void    loadBoardsManifest();
    void    saveBoardsManifest() const;
    void    createNewBoard();
    void    renameBoard(const QString& boardId);
    void    deleteBoard(const QString& boardId);
    void    rebuildTabs();

    // Undo/redo
    struct BoardState {
        QList<CanvasCard>       cards;
        QList<CanvasConnection> connections;
        QList<CanvasZone>       zones;
        QList<CanvasInk>        inks;
    };
    BoardState captureState() const;
    void       applyState(const BoardState& s);
    void       pushUndo();
    void       undo();
    void       redo();
    void       refreshUndoButtons();

    // Gaveta da lousa (cards guardados) — mora na bandeja, aba Guardados
    void restoreFromStash(int index, const QPointF* at = nullptr);
    void stashSelectedCards();
    void deleteSelectedCards();   // remoção permanente (com confirmação)

    // Bandeja
    void scheduleTrayRefresh();
    void refreshTray();

    // Barra de ações
    void scheduleActionBar();
    void refreshActionBar();
    void positionActionBar();
    void onAction(const QString& id, const QPoint& globalPos);
    void alignSelection(const QString& how);

    // Linhas
    void editConnectionLabel(const QString& id);
    void showConnectionMenu(const QString& id, const QPoint& screenPos);
    void updateConnection(const QString& id, const std::function<void(CanvasConnection&)>& change);
    QList<QColor> connectionPalette() const;
    void askNewConnection(const QString& fromId, const QString& toId);

    // Mapa de áreas (tecla F)
    void buildMapPanel();
    void positionOverlays();
    void toggleMap();
    void refreshMapList();

    // Exportação de áreas para gavetas
    void exportZones(const QList<CanvasZone>& zones);
    void exportSelectedZone();   // exporta a área selecionada
    void exportBoardAsImage();   // H: escolhe o pedaço e vê antes

    // Fundo e cards (A)
    void showBoardLook();
    void applyLookPrefs();
    void reclaimInput();      // a Lousa volta a ser a janela ativa depois de popup

    // Busca (J)
    void runSearch(const QString& text);
    void stepSearch(int delta);

    // Templates de layout inicial (só oferecidos com a lousa vazia)
    void showTemplatePicker();
    void applyTemplate(const QString& id);

    // Criar documento a partir de um card (post-it/comentário/imagem)
    void createDocFromCard(const CanvasCard& c);
    void openCard(const CanvasCard& c);
    // Pôster de personagem/cenário/objeto flutuando no quadro (sem modal).
    void openElementSheet(const QString& type, const QString& title,
                          const std::function<void(class ElementCreateDialog*)>& onAccept);
    void newCharacterOnBoard(const QPointF* at = nullptr);
    QPointer<QDialog> m_elementSheet;

    QString htmlFor(const QString& docKey) const;
    void scheduleProjectRefresh();   // projeto mudou: atualiza agora ou ao abrir
    bool m_projectDirty = false;

    LousaScene*  m_scene        = nullptr;
    LousaView*   m_view         = nullptr;
    class QFrame* m_leftBar     = nullptr;   // canto de cima à esquerda (lousa, desfazer)
    class QFrame* m_rightBar    = nullptr;   // canto de cima à direita (zoom, fundo…)
    QToolButton* m_boardPill    = nullptr;
    QToolButton* m_stashChip    = nullptr;
    QToolButton* m_areasChip    = nullptr;
    QWidget*     m_stashPanel   = nullptr;
    QListWidget* m_stashList    = nullptr;
    bool         m_stashOpen    = false;
    QToolButton* m_undoBtn      = nullptr;
    QToolButton* m_redoBtn      = nullptr;
    QToolButton* m_zoomOutBtn   = nullptr;
    QToolButton* m_zoomLabel    = nullptr;
    QToolButton* m_zoomInBtn    = nullptr;
    QToolButton* m_fitBtn       = nullptr;
    QToolButton* m_lookBtn      = nullptr;
    QToolButton* m_exportBtn    = nullptr;
    QToolButton* m_helpBtn      = nullptr;
    QToolButton* m_closeBtn     = nullptr;
    QList<QPair<QToolButton*, QString>> m_iconBindings;

    LousaDock*       m_dock      = nullptr;
    class LousaInkBar* m_inkBar  = nullptr;   // barra da Caneta, em cima da Doca
    bool             m_inkEraseUndone = false;   // a borracha já empilhou o desfazer deste gesto
    void             setInkMode(bool on);
    void             syncInkTool();
    // Esboço: folha pra desenhar (SketchEditor por cima da Lousa)
    QPointer<class SketchEditor> m_sketchEditor;
    void             createSketch();
    void             openSketchEditor(const QString& cardId);
    void             useSketchAsPhoto(const QImage& image);
    LousaActionBar*  m_actionBar = nullptr;
    LousaMinimap*    m_minimap   = nullptr;
    LousaCheatSheet* m_cheat     = nullptr;
    LousaSearchBar*  m_search    = nullptr;
    QTimer*          m_trayTimer = nullptr;
    QTimer*          m_barTimer  = nullptr;

    QWidget*     m_emptyStateBox   = nullptr;
    QLabel*      m_emptyLabel      = nullptr;
    QPushButton* m_useTemplateBtn  = nullptr;

    QString             m_projectRoot;
    class ProjectModel* m_projectModel  = nullptr;
    class ElementsStore* m_elementsStore = nullptr;
    std::function<QString(const QString&)> m_htmlProvider;

    QList<BoardState>      m_undo;
    QList<BoardState>      m_redo;
    QHash<QString, QString> m_contentStore;  // image content fora dos snapshots
    bool                   m_loading = false;

    QList<CanvasCard> m_stash;

    // Mapa de áreas
    QWidget*     m_mapPanel = nullptr;
    QListWidget* m_mapList  = nullptr;
    bool         m_mapOpen  = false;

    // Múltiplas lousas
    QVector<LousaBoardMeta> m_boards;
    QString      m_activeBoardId;

    // Fundo, inclinação, minimapa
    bool m_minimapOn = true;

    // Busca
    QList<QPair<int, QString>> m_searchHits;   // (0 = card, 1 = área), id
    int m_searchIndex = -1;

    QString m_cutCardId;   // card recortado (Ctrl+X), aguardando colar

    // Preview flutuante de card (hover)
    QWidget* m_cardPreview      = nullptr;
    QLabel*  m_previewTitle     = nullptr;
    QWidget* m_previewDivider   = nullptr;
    class QTextEdit* m_previewBody = nullptr;
    void buildCardPreview();
    void showCardPreview(const CanvasCard& data, const QPoint& screenPos);
    void hideCardPreview();
    QString m_previewCardType;
    int     m_previewCardFontSize = 0;
};
