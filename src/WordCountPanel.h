#pragma once

#include <QWidget>

class QLabel;
class QTimer;
class QToolButton;
class QPushButton;
class QFrame;
class QSpinBox;
class QStackedWidget;
class CounterTabBar;
class CounterSegmented;
class CounterThinBar;
class CounterDayStrip;
class QScrollArea;
class WordCounter;
class EditorHost;
class ProjectModel;
class WordCounterCalendar;
class MiniCounterWidget;
class CounterFaceWidget;
class QVBoxLayout;

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

    // Altura disponível na tela (da base da TopToolbar até a base do container).
    // Limita a altura do scroll para o conteúdo não passar atrás da toolbar.
    void setAvailableHeight(int h);

    // Palavras do trecho selecionado no editor (0 = sem seleção). Enquanto
    // houver seleção, o espaço de "Palavras" do contador mostra a seleção.
    void setSelectionWords(int n);

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
    void updateScrollSizing();   // ajusta a altura do scroll (min(conteúdo, disponível))
    void startSprint();
    void stopSprint();
    void tickSprint();
    void openCompactContextMenu(const QPoint& globalPos);
    // Estilo do contador minimizado ("classic" = cards; os outros são o
    // MiniCounterWidget). Preferência global do app, não do projeto.
    void setCompactStyle(const QString& style);
    // Quem aparece: mini (compacto + estilo enxuto) ou o scroll com os cards.
    void updateBodyVisibility();
    void openStatsDialog(QWidget* anchor);
    // Tamanho do contador (85/100/120/140/165%). Vale no compacto, nos
    // estilos normais e nos mini; o modo full abre sempre em 100%.
    void setCompactScale(int percent);
    void applyScale();
    void openGoalResetTimeDialog();

    WordCounter* m_counter;
    EditorHost* m_host;
    ProjectModel* m_model;

    QToolButton* m_toggleButton;
    QScrollArea* m_scrollArea = nullptr;   // envolve m_body + m_fullBody
    QWidget* m_scrollContent = nullptr;    // widget interno do scroll
    int m_maxBodyHeight = 0;               // teto de altura (0 = sem limite)
    QFrame* m_body;          // cards (sempre visível em compact e full)
    QFrame* m_fullBody;      // scope + meta (só visível em full)
    // Títulos completos dos slots ("Palavras (atual)"): cards do clássico e tooltips.
    QString m_slot1Title;
    QString m_slot2Title;
    int m_selWords = 0;
    MiniCounterWidget* m_mini = nullptr;
    QString m_compactStyle;
    int m_scalePercent = 100;
    // Corpo do contador no tamanho normal, todo desenhado (clássico incluso).
    CounterFaceWidget* m_face = nullptr;

    static constexpr int kPanelWidth = 256;
    static constexpr int kScrollBarW = 8;   // gutter reservado para a barra de rolagem

    // Modo full em abas (Contador Lab, 15/11/2026): Meta · Sprint · Mês,
    // mais a página das folgas, que abre de dentro da Meta.
    enum Page { PageGoal = 0, PageSprint = 1, PageMonth = 2, PageFolgas = 3 };
    QWidget* buildGoalPage();
    QWidget* buildSprintPage();
    QWidget* buildMonthPage();
    QWidget* buildFolgasPage();
    void showPage(int page);
    void refreshSprintPage();
    void openGoalResetMenu();
    void stepFolgaCadence(int delta);
    QFrame* makeStepper(QSpinBox* spin, QWidget* parent);

    CounterTabBar* m_tabs = nullptr;
    QStackedWidget* m_stack = nullptr;

    // Meta
    QLabel* m_goalNumLabel = nullptr;     // "0 / 550 palavras"
    QLabel* m_goalResetLine = nullptr;    // "reinicia em X" (duplo clique = horário)
    CounterThinBar* m_goalBar = nullptr;
    QLabel* m_statNum[4] = {};
    QLabel* m_statCap[4] = {};
    CounterDayStrip* m_strip = nullptr;
    CounterSegmented* m_goalScopeSeg = nullptr;
    CounterSegmented* m_goalTypeSeg = nullptr;
    QSpinBox* m_goalValueSpin = nullptr;
    QToolButton* m_folgaLink = nullptr;

    // Folgas (página)
    QLabel* m_folgaStatus = nullptr;
    QLabel* m_folgaValue = nullptr;       // "7 dias" no seletor ‹ ›
    QLabel* m_folgaLockLine = nullptr;

    // Sprint de escrita
    QTimer* m_sprintTimer = nullptr;
    bool m_sprintActive = false;
    bool m_sprintEnded = false;           // mostra o resultado até mexer em algo
    int m_sprintWritten = 0;
    int m_sprintSecondsLeft = 0;
    int m_sprintStartSession = 0;
    QLabel* m_sprintBig = nullptr;        // 25:00 / palavras do sprint encerrado
    QLabel* m_sprintCaption = nullptr;
    QWidget* m_sprintSetup = nullptr;     // duração + meta
    QWidget* m_sprintRun = nullptr;       // palavras + barra
    QLabel* m_sprintWordsLabel = nullptr;
    CounterThinBar* m_sprintBar = nullptr;
    QPushButton* m_sprintActionBtn = nullptr;
    QSpinBox* m_sprintMinutesSpin = nullptr;
    QSpinBox* m_sprintWordsSpin = nullptr;

    // Calendário (aba Mês)
    WordCounterCalendar* m_calendar = nullptr;

    bool m_expanded = true;
    bool m_fullMode = false;
    bool m_updatingFromSettings = false;

    // Cheat code dev: Ctrl + "lazyass" (toggle). Desbloqueia o combo da cadência.
    bool m_lazyassMode = false;
    QString m_lazyBuffer;
    QLabel* m_lazyassLabel = nullptr;
    void setLazyassMode(bool on);
};
