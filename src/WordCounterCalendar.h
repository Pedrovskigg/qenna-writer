#pragma once

#include <QDate>
#include <QWidget>

class QGridLayout;
class QLabel;
class QToolButton;
class WordCounter;

// Calendário mensal do WordCounter. Mostra estrelas (1-5) por dia, com base
// em quantas vezes a meta diária foi batida (floor(progress / target)).
class WordCounterCalendar : public QWidget {
    Q_OBJECT
public:
    explicit WordCounterCalendar(WordCounter* counter, QWidget* parent = nullptr);

public slots:
    void refresh();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyThemeStyle();

signals:
    void geometryChanged();

private:
    void buildUi();
    void shiftMonth(int delta);
    void goToToday();
    int starsForDay(const QString& dateKey) const;
    QString tooltipForDay(const QString& dateKey) const;
    void showDayDetails(const QString& dateKey);

    WordCounter* m_counter;
    QToolButton* m_prevBtn;
    QToolButton* m_nextBtn;
    QToolButton* m_todayBtn;
    QLabel* m_monthLabel;
    QGridLayout* m_grid;
    QDate m_currentMonth;  // 1º dia do mês mostrado
};
