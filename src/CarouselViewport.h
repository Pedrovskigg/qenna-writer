#pragma once

#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <functional>

class QAbstractButton;

// Janela de carrossel pra uma fileira de botões que não cabe (12/11/2026,
// ideia do Pe, irmã do carrossel do menu de seleção). Quando o conteúdo
// cabe, nada muda: os botões ficam onde sempre ficaram. Quando passa do
// espaço, a fileira gira: o miolo continua com os botões DE VERDADE (hover,
// menus, arrasto, tudo igual) e as pontas que ainda escondem botão curvam
// como um tambor, desenhadas a partir do próprio conteúdo.
//
// Mouse: o mesmo gosto do menu de seleção (o ↔ da lingueta dele vale pra
// todos os carrosséis). Solto: a fileira acompanha o mouse de ponta a ponta.
// Fixo: só as pontas giram. Com botão apertado (arrastando uma gaveta, por
// exemplo) vale sempre o fixo, pra dar pra levar a coisa até o fim da fila.
// Clicar num botão da ponta curva traz ele pro miolo.
class CarouselViewport : public QWidget
{
    Q_OBJECT

public:
    explicit CarouselViewport(Qt::Orientation orientation, QWidget *parent = nullptr);

    // O conteúdo passa a morar dentro do carrossel (reparentado).
    void setContent(QWidget *content);
    QWidget *content() const { return m_content; }

    Qt::Orientation orientation() const { return m_orientation; }
    void setOrientation(Qt::Orientation orientation);

    // Menor comprimento (no eixo) que o carrossel aceita antes de girar.
    void setMinimumLength(int px);

    // true quando o conteúdo não cabe e a fileira está girando.
    bool isOverflowing() const { return m_max > 0; }

    // Onde os botões aparecem inteiros (sem as pontas curvas), em
    // coordenadas deste widget.
    QRect flatRect() const;

    // Traz o widget (descendente do conteúdo) pro miolo.
    void ensureVisible(QWidget *w, bool animated);

    // Área de fora (coordenada global) que também conta como "mouse em cima";
    // a posição no eixo é a do próprio carrossel. A faixa de etiquetas da
    // LeftBar usa isso pra girar as gavetas enquanto o mouse anda nela.
    std::function<bool(const QPoint &globalPos)> extraHover;

    // Preferência única de todos os carrosséis (o ↔ do menu de seleção).
    static bool edgeModePreferred();
    static void setEdgeModePreferred(bool on);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void scrolled();
    void overflowChanged(bool overflowing);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    int mainOf(const QSize &s) const { return m_orientation == Qt::Horizontal ? s.width() : s.height(); }
    int crossOf(const QSize &s) const { return m_orientation == Qt::Horizontal ? s.height() : s.width(); }
    qreal mainOf(const QPointF &p) const { return m_orientation == Qt::Horizontal ? p.x() : p.y(); }
    QRect axisRect(int start, int length, int cross) const;
    int bandPx() const;

    void scheduleRelayout();
    void relayout();
    void applyGeometry();
    void paintBand(class QPainter &p, bool leading);
    // Posição no eixo deste widget → posição no conteúdo (e o cosseno da curva).
    qreal toContent(qreal u, qreal *cosOut = nullptr) const;
    bool inBand(qreal u) const;
    QAbstractButton *buttonAtContent(qreal s) const;
    void steer(qreal u);
    void kick();
    void step();

    Qt::Orientation m_orientation;
    QWidget *m_clip = nullptr;            // o miolo: recorta o conteúdo
    QPointer<QWidget> m_content;
    int m_minLength = 0;
    int m_len = 0;                        // comprimento do conteúdo no eixo
    qreal m_max = 0;                      // quanto dá pra girar (0 = cabe)
    qreal m_x = 0, m_target = 0, m_vel = 0;
    int m_bandLead = 0, m_bandTrail = 0;  // pontas curvas agora (px)
    bool m_edgeMode = false;
    bool m_relayoutPending = false;
    QPointer<QAbstractButton> m_pressed;
    QTimer m_tick;
};
