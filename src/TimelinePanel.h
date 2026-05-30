#pragma once

#include "TimelineTypes.h"

#include <QList>
#include <QString>
#include <QWidget>

class TimelineScene;
class TimelineView;
class ProjectModel;

class TimelinePanel : public QWidget {
    Q_OBJECT
public:
    explicit TimelinePanel(QWidget* parent = nullptr);

    void setProjectRoot(const QString& root);
    void setProjectModel(ProjectModel* model);

signals:
    void closeRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event)   override;

private slots:
    void applyTheme();
    void createTimeline();
    void createEventAt(const QPointF& scenePos);
    void openEditPopup(const QString& eventId);

private:
    void buildUi();
    void save() const;
    void load();

    TimelineScene*  m_scene        = nullptr;
    TimelineView*   m_view         = nullptr;
    QWidget*        m_toolbar      = nullptr;

    QString         m_projectRoot;
    ProjectModel*   m_projectModel = nullptr;

    // dados — preenchidos nas etapas seguintes
    QList<TimelineDef>   m_timelines;
    QList<TimelineEvent> m_events;
    QList<TimelineConn>  m_connections;
};
