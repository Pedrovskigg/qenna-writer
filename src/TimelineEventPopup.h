#pragma once

#include "TimelineTypes.h"

#include <QDialog>
#include <functional>

class QLineEdit;
class QPlainTextEdit;
class QToolButton;
class ProjectModel;

class TimelineEventPopup : public QDialog {
    Q_OBJECT
public:
    // model (opcional) habilita o seletor "Vincular a" (capítulo/cena/doc).
    explicit TimelineEventPopup(const QList<TimelineDef>& timelines,
                                ProjectModel* model = nullptr,
                                QWidget* parent = nullptr);

    void setEventData(const TimelineEvent& e);
    TimelineEvent eventData() const;

    // Resolve o texto de um doc vinculado p/ herdar na descrição (se vazia).
    void setDocTextResolver(std::function<QString(const QString&)> r) { m_docTextResolver = std::move(r); }

private slots:
    void pickColor();
    void applyTheme();

private:
    void buildUi(const QList<TimelineDef>& timelines);
    void updateColorBtn();

    QLineEdit*     m_title          = nullptr;
    QLineEdit*     m_marker         = nullptr;
    QPlainTextEdit* m_desc          = nullptr;
    QToolButton*   m_colorBtn       = nullptr;
    QColor         m_color;

    class QComboBox* m_timelineCombo = nullptr;
    class QComboBox* m_linkCombo     = nullptr;
    QList<TimelineDef> m_timelines;
    ProjectModel*  m_model = nullptr;
    std::function<QString(const QString&)> m_docTextResolver;

    QString m_eventId;
};
