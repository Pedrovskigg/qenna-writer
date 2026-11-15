#pragma once

#include <QColor>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include <functional>

class QTimer;

// Peças desenhadas do painel do contador em modo full (Contador Lab, 15/11/2026):
// abas de texto, botões segmentados, a barra fina da meta e as barrinhas dos
// últimos dias. Sem Q_OBJECT: avisam por std::function.

// Cor de um dia pela meta: vazio = trilho; meta parcial = acento bem fraco;
// 1 a 5 vezes a meta = acento cada vez mais forte. Usada nas barrinhas e no mês.
QColor counterLevelColor(int stars, double fraction);

// Abas de texto sublinhadas ("Meta · Sprint · Mês").
class CounterTabBar : public QWidget {
public:
    explicit CounterTabBar(QWidget* parent = nullptr);
    void setTabs(const QStringList& labels);
    void setCurrent(int index);
    int current() const { return m_current; }
    // Pontinho pulsando ao lado da aba (o sprint correndo enquanto se olha outra aba).
    void setPulse(int index);
    std::function<void(int)> onChanged;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QFont tabFont() const;
    QVector<QRectF> tabRects() const;
    int tabAt(const QPoint& pos) const;
    QStringList m_labels;
    int m_current = 0;
    int m_hover = -1;
    int m_pulseIndex = -1;
    qreal m_phase = 0;
    QTimer* m_pulseTimer = nullptr;
};

// Botões segmentados ("Capítulos | Gavetas | Tudo").
class CounterSegmented : public QWidget {
public:
    explicit CounterSegmented(QWidget* parent = nullptr);
    void setOptions(const QStringList& labels, const QStringList& tooltips = {});
    void setCurrent(int index);
    int current() const { return m_current; }
    std::function<void(int)> onChanged;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    bool event(QEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    int indexAt(const QPoint& pos) const;
    QStringList m_labels;
    QStringList m_tips;
    int m_current = 0;
    int m_hover = -1;
};

// Barra fina de progresso (5 px). Fica verde quando a meta é batida.
class CounterThinBar : public QWidget {
public:
    explicit CounterThinBar(QWidget* parent = nullptr);
    void setValue(double fraction, bool done);
    QSize sizeHint() const override { return { 100, 5 }; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    double m_value = 0;
    bool m_done = false;
};

// Barrinhas dos últimos dias: a altura e a cor dizem quanto da meta saiu.
class CounterDayStrip : public QWidget {
public:
    struct Day {
        double fraction = 0;   // progresso / meta daquele dia
        int stars = 0;         // vezes que a meta foi batida (0–5)
        bool offDay = false;   // folga marcada
        bool today = false;
        QString tooltip;
    };
    explicit CounterDayStrip(QWidget* parent = nullptr);
    void setDays(const QVector<Day>& days);
    std::function<void()> onClicked;
    QSize sizeHint() const override { return { 228, 26 }; }

protected:
    bool event(QEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    QRectF barSlot(int i) const;
    QVector<Day> m_days;
};
