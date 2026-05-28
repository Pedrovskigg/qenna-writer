#pragma once

#include <QWidget>

class QLabel;
class QToolButton;
class QPushButton;
class QFrame;
class QSpinBox;
class QComboBox;
class QProgressBar;
class WordCounter;
class EditorHost;
class ProjectModel;
class WordCounterCalendar;

// Painel flutuante no canto inferior esquerdo. Três estados:
//   collapsed → só o triângulo ▼/▲
//   compact   → triângulo + cards (Palavras, Caracteres)
//   full      → tudo: scope picker, meta diária, status, configs
class WordCountPanel : public QWidget {
    Q_OBJECT
public:
    WordCountPanel(WordCounter* counter, EditorHost* host, ProjectModel* model, QWidget* parent = nullptr);

    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }
    void setFullMode(bool full);
    bool isFullMode() const { return m_fullMode; }

public slots:
    void refresh();
    void refreshFromSettings();

signals:
    void geometryChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyThemeStyle();

private:
    void buildUi();
    void updateToggleArrow();
    QFrame* buildScopeSection();
    QFrame* buildGoalSection();
    void openCompactContextMenu(const QPoint& globalPos);

    WordCounter* m_counter;
    EditorHost* m_host;
    ProjectModel* m_model;

    QToolButton* m_toggleButton;
    QFrame* m_body;          // cards (sempre visível em compact e full)
    QFrame* m_fullBody;      // scope + meta (só visível em full)
    QLabel* m_metaLine;
    QLabel* m_slot1Value;
    QLabel* m_slot1Title;
    QLabel* m_slot2Value;
    QLabel* m_slot2Title = nullptr;
    QProgressBar* m_compactGoalBar = nullptr;
    QLabel* m_compactGoalResetLabel = nullptr;

    // Scope picker
    QPushButton* m_scopeManuscriptBtn;
    QPushButton* m_scopeActiveBtn;
    QPushButton* m_scopeDrawersBtn;
    QPushButton* m_scopeAllBtn;

    // Goal
    QLabel* m_goalStatus;        // "Hoje: 0 / 15 min (0%)"
    QProgressBar* m_goalProgress;
    QLabel* m_goalResetLine;     // "Reinicia em X h Y min"
    QComboBox* m_goalScopeCombo;
    QComboBox* m_goalTypeCombo;
    QSpinBox* m_goalValueSpin;
    QLabel* m_goalValueLabel;    // "Palavras" ou "Minutos"
    QToolButton* m_goalResetBtn;
    QToolButton* m_folgaBtn = nullptr;

    // Stats line
    QLabel* m_statStreak = nullptr;
    QLabel* m_statRecord = nullptr;
    QLabel* m_statToday = nullptr;
    QLabel* m_statPages = nullptr;
    QLabel* m_statAvgWords = nullptr;
    QLabel* m_statAvgTime = nullptr;

    // Folgas panel (inline)
    QFrame* m_folgaPanel = nullptr;
    QComboBox* m_folgaEveryCombo = nullptr;
    QLabel* m_folgaStatus = nullptr;
    QLabel* m_folgaLockLine = nullptr;

    // Calendário
    QToolButton* m_calendarToggleBtn = nullptr;
    WordCounterCalendar* m_calendar = nullptr;
    bool m_calendarVisible = false;

    bool m_expanded = true;
    bool m_fullMode = false;
    bool m_updatingFromSettings = false;

    // Cheat code dev: Ctrl + "lazyass" (toggle). Desbloqueia o combo da cadência.
    bool m_lazyassMode = false;
    QString m_lazyBuffer;
    QLabel* m_lazyassLabel = nullptr;
    void setLazyassMode(bool on);
};
