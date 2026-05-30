#pragma once

#include "TimelineTypes.h"

#include <QDialog>

class QLineEdit;
class QToolButton;
class QLabel;

class TimelineEventPopup : public QDialog {
    Q_OBJECT
public:
    explicit TimelineEventPopup(const QList<TimelineDef>& timelines,
                                QWidget* parent = nullptr);

    // Preenche os campos com dados existentes (modo edição)
    void setEventData(const TimelineEvent& e);

    // Retorna dados preenchidos pelo usuário
    TimelineEvent eventData() const;

private slots:
    void pickColor();
    void applyTheme();

private:
    void buildUi(const QList<TimelineDef>& timelines);
    void updateColorBtn();
    void selectShape(const QString& shape);

    QLineEdit*   m_title      = nullptr;
    QLineEdit*   m_marker     = nullptr;
    QLineEdit*   m_desc       = nullptr;
    QToolButton* m_colorBtn   = nullptr;
    QColor       m_color;        // inválida = herda da timeline

    // seletores de forma
    QList<QToolButton*> m_shapeBtns;
    QString             m_shape = QStringLiteral("square");

    // timeline
    class QComboBox* m_timelineCombo = nullptr;
    QList<TimelineDef> m_timelines;

    QString m_eventId;
};
