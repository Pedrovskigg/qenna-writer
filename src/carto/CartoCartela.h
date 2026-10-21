#pragma once

// A Cartela: a legenda do mapa desenhada no próprio mapa, em papel e nanquim
// (em qualquer vista): título, região, escala em léguas e os itens da legenda,
// que também são a paleta (clicar num item escolhe a ferramenta). A rosa dos
// ventos gira o mapa quando a Cartela é a interface escolhida.

#include <QWidget>

#include <vector>

namespace carto {

struct CartelaItem { QString icon, label; };

class Cartela : public QWidget {
    Q_OBJECT
public:
    explicit Cartela(QWidget* parent = nullptr);
    void setItems(const std::vector<CartelaItem>& items);
    void setTitle(const QString& t, const QString& sub);
    void setLeaguePx(double px);          // px da tela por légua
    void setActive(int index);            // item da ferramenta em uso (−1 = nenhum)
    void setMinimized(bool on);
    bool minimized() const { return m_min; }
    void setInteractive(bool on) { m_interactive = on; update(); }   // no PNG: sem botão de recolher
    QSize sizeHint() const override;

signals:
    void itemPicked(int index);
    void minimizeToggled(bool on);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    int itemAt(QPointF p) const;
    QRectF itemRect(int i) const;
    QRectF minRect() const;
    int itemsTop() const;
    std::vector<CartelaItem> m_items;
    QString m_title, m_sub;
    double m_leaguePx = 10;
    int m_active = -1, m_hover = -1;
    bool m_min = false, m_interactive = true;
};

class Rose : public QWidget {
    Q_OBJECT
public:
    explicit Rose(QWidget* parent = nullptr);
    void setAngle(double ang) { m_ang = ang; update(); }

signals:
    void angleDragged(double ang, bool quick);
    void northRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    double m_ang = 0, m_a0 = 0, m_ang0 = 0;
    bool m_drag = false, m_moved = false;
};

} // namespace carto
