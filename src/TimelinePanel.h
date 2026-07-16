#pragma once

#include "PresenceTypes.h"
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
class ElementsStore;

class TimelinePanel : public QWidget {
    Q_OBJECT
public:
    explicit TimelinePanel(QWidget* parent = nullptr);

    void setProjectRoot(const QString& root);
    void setProjectModel(ProjectModel* model);
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
    void promptNewEvent(const QString& description, const QString& marker,
                        const QString& title = QString());

signals:
    void closeRequested();
    void exportEventAsDoc(TimelineEvent data);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event)   override;

private slots:
    void applyTheme();
    void createTimeline();
    void editTimeline(const QString& id);
    void createEventAt(const QPointF& scenePos);
    void commitEvent(TimelineEvent e, const QPointF& scenePos); // cria o evento já resolvido
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
    // Gera/atualiza as trilhas automáticas de personagem a partir do detector
    // de presença + roles. askSecondary = perguntar sobre papéis secundários.
    void syncCharacterTimelines(bool askSecondary);

    TimelineScene*  m_scene        = nullptr;
    TimelineView*   m_view         = nullptr;
    QWidget*        m_toolbar      = nullptr;
    class QToolButton* m_btnView   = nullptr;
    class QToolButton* m_btnAxis   = nullptr;
    class QToolButton* m_btnFocus  = nullptr;
    class QToolButton* m_btnDepth  = nullptr;
    class QToolButton* m_btnAdd    = nullptr;   // "+" flutuante sobre o canvas
    class QMenu*       m_focusMenu = nullptr;

    QString         m_projectRoot;
    ProjectModel*   m_projectModel  = nullptr;
    ElementsStore*  m_elementsStore = nullptr;
    std::function<QString(const QString&)> m_docTextResolver;
    PresenceProvider m_presenceProvider;

    // dados — preenchidos nas etapas seguintes
    QList<TimelineDef>   m_timelines;
    QList<TimelineEvent> m_events;
    QList<TimelineConn>  m_connections;
};
