#pragma once

#include "LousaTypes.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QWidget>

class LousaScene;
class LousaView;
class QKeyEvent;
class QLabel;
class QListWidget;
class QPushButton;
class QToolButton;

class LousaPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LousaPanel(QWidget* parent = nullptr);

    void setProjectRoot(const QString& root);
    void setProjectModel(class ProjectModel* model);
    void setElementsStore(class ElementsStore* store);
    void refreshDocCards();
    void refreshEmptyState();

signals:
    void closeRequested();
    // Pedido para criar um evento na Timeline a partir de um card de texto.
    void createTimelineEventRequested(const QString& title, const QString& description);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private slots:
    void onPickColor();
    void applyTheme();

private:
    void buildUi();
    void buildHelpPanel();
    void positionHelpPanel();
    void toggleHelp();
    void reloadIcons();
    void save() const;
    void load();
    void updateColorBtn();
    CanvasCard nextCardData(const QString& type) const;

    // Undo/redo
    struct BoardState {
        QList<CanvasCard>       cards;
        QList<CanvasConnection> connections;
        QList<CanvasZone>       zones;
    };
    BoardState captureState() const;
    void       applyState(const BoardState& s);
    void       pushUndo();
    void       undo();
    void       redo();

    // Stash
    void buildStashPanel();
    void positionStashPanel();
    void toggleStash();
    void refreshStashList();
    void refreshStashBtn();
    void restoreFromStash(int index);
    void stashSelectedCards();
    void deleteSelectedCards();   // remoção permanente (com confirmação)

    // Mapa de áreas (tecla F)
    void buildMapPanel();
    void positionMapPanel();
    void toggleMap();
    void refreshMapList();

    // Exportação de áreas para gavetas
    void exportZones(const QList<CanvasZone>& zones);
    void exportSelectedZone();   // botão da toolbar: exporta a área selecionada

    // Criar documento a partir de um card (post-it/comentário/imagem)
    void createDocFromCard(const CanvasCard& c);

    QList<QPair<QToolButton*, QString>> m_iconBindings;

    LousaScene*  m_scene        = nullptr;
    LousaView*   m_view         = nullptr;
    QWidget*     m_toolbar      = nullptr;
    QToolButton* m_colorBtn     = nullptr;
    QLabel*      m_emptyLabel   = nullptr;
    QLabel*      m_zoomLabel    = nullptr;
    QWidget*     m_helpPanel    = nullptr;
    bool         m_helpOpen     = false;

    // Botões de criação (acessados pelos atalhos de teclado)
    QToolButton* m_btnNote = nullptr;
    QToolButton* m_btnCmt  = nullptr;
    QToolButton* m_btnImg  = nullptr;
    QToolButton* m_btnDoc  = nullptr;
    QToolButton* m_btnChar = nullptr;
    QToolButton* m_btnText = nullptr;
    QToolButton* m_btnZone = nullptr;

    QString             m_projectRoot;
    class ProjectModel* m_projectModel  = nullptr;
    class ElementsStore* m_elementsStore = nullptr;

    QList<BoardState>      m_undo;
    QList<BoardState>      m_redo;
    QHash<QString, QString> m_contentStore;  // image content fora dos snapshots
    bool                   m_loading = false;

    // Stash
    QList<CanvasCard> m_stash;
    QWidget*     m_stashPanel = nullptr;
    QListWidget* m_stashList  = nullptr;
    QToolButton* m_stashBtn   = nullptr;
    bool         m_stashOpen  = false;

    // Mapa de áreas
    QWidget*     m_mapPanel = nullptr;
    QListWidget* m_mapList  = nullptr;
    QToolButton* m_mapBtn   = nullptr;
    bool         m_mapOpen  = false;

    QString m_cutCardId;   // card recortado (Ctrl+X), aguardando colar

    // Preview flutuante de card (hover)
    QWidget* m_cardPreview      = nullptr;
    QLabel*  m_previewTitle     = nullptr;
    QWidget* m_previewDivider   = nullptr;
    class QTextEdit* m_previewBody = nullptr;
    void buildCardPreview();
    void showCardPreview(const CanvasCard& data, const QPoint& screenPos);
    void hideCardPreview();
};
