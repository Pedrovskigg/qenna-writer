#pragma once
// Barra da Caneta da Lousa: aparece em cima da Doca enquanto a Caneta está
// ligada. Quatro pontas (Caneta, Marcador, Marca-texto, Borracha), as cores da
// cartela e o seletor do Qenna, espessura e estabilizador. A última escolha
// fica guardada no app.

#include <QColor>
#include <QFrame>
#include <QList>
#include <QPair>

class QLabel;
class QSlider;
class QToolButton;

class LousaInkBar : public QFrame
{
    Q_OBJECT
public:
    explicit LousaInkBar(QWidget* parent = nullptr);

    QString tool() const { return m_tool; }      // "pen" | "marker" | "highlight" | "eraser"
    QColor  color() const { return m_color; }
    qreal   size() const { return m_size; }
    void    setTool(const QString& tool);
    void    applyTheme();

signals:
    void toolChanged();          // ponta, cor ou espessura mudaram
    void undoRequested();
    void clearRequested();
    void doneRequested();

private:
    void refreshTools();
    void refreshSwatches();
    void refreshDot();
    void save() const;

    QString m_tool = QStringLiteral("pen");
    QColor  m_color;
    qreal   m_size = 4;
    QList<QPair<QToolButton*, QString>> m_tools;
    QList<QPair<QToolButton*, QColor>>  m_swatches;
    QToolButton* m_pick = nullptr;
    QLabel*  m_dot = nullptr;
    QSlider* m_sizeSlider = nullptr;
    QSlider* m_stabSlider = nullptr;
    QLabel*  m_stabValue = nullptr;
    QList<QToolButton*> m_textBtns;
    QList<QFrame*> m_seps;
};
