#pragma once

#include "TimelineTypes.h"

#include <QList>
#include <QString>
#include <QWidget>

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

signals:
    void closeRequested();
    void exportEventAsDoc(TimelineEvent data);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event)   override;

private slots:
    void applyTheme();
    void createTimeline();
    void createEventAt(const QPointF& scenePos);
    void openEditPopup(const QString& eventId);
    void onExportEventAsDoc(const TimelineEvent& event);

private:
    void buildUi();
    void save() const;
    void load();
    void toggleViewMode();
    void toggleAxisMode();
    void refreshModeButtons();
    // Gera/atualiza as trilhas automáticas de personagem a partir do detector
    // de presença + roles. askSecondary = perguntar sobre papéis secundários.
    void syncCharacterTimelines(bool askSecondary);

    TimelineScene*  m_scene        = nullptr;
    TimelineView*   m_view         = nullptr;
    QWidget*        m_toolbar      = nullptr;
    class QToolButton* m_btnView   = nullptr;
    class QToolButton* m_btnAxis   = nullptr;

    QString         m_projectRoot;
    ProjectModel*   m_projectModel  = nullptr;
    ElementsStore*  m_elementsStore = nullptr;

    // dados — preenchidos nas etapas seguintes
    QList<TimelineDef>   m_timelines;
    QList<TimelineEvent> m_events;
    QList<TimelineConn>  m_connections;
};
