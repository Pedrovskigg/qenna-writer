#pragma once
// Uma fatia do Canvas 2D do navegador por cima do QPainter, só o que as vinhetas usam.
//
// As vinhetas da gaveta nascem como concepts em JavaScript (concepts/manuscritos/*.html) e o Pe aprova o
// desenho de lá. Com a mesma API dos dois lados, o port é quase linha a linha e o app desenha o que ele viu.
// Diferenças que importam e estão tratadas aqui:
//  - no canvas o caminho é transformado na hora em que cada ponto entra (não na hora do fill/stroke);
//  - arc()/ellipse() usam ângulo em radianos no sentido horário da tela;
//  - fill() usa a regra "nonzero" (Qt usa par-ímpar por padrão);
//  - sombra com desfoque (shadowBlur) não existe no QPainter: vira um halo de traços largos e transparentes.

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QGradient>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <QVector>

class MsCanvas {
public:
    explicit MsCanvas(QPainter& p);
    ~MsCanvas();

    // estado
    void save();
    void restore();
    void translate(double x, double y);
    void rotate(double rad);
    void scale(double sx, double sy);
    QTransform getTransform() const { return m_st.T; }   // em relação à base (as unidades da vinheta)

    QBrush fillStyle;
    QBrush strokeStyle;
    double lineWidth = 1;
    Qt::PenCapStyle lineCap = Qt::RoundCap;
    Qt::PenJoinStyle lineJoin = Qt::RoundJoin;
    void setLineDash(const QVector<double>& d) { m_st.dash = d; }
    double globalAlpha = 1;
    QColor shadowColor = QColor(0, 0, 0, 0);
    double shadowBlur = 0;      // em unidades da vinheta (no JS era blur*s)
    double shadowOffsetX = 0, shadowOffsetY = 0;
    void setFont(const QString& family, double px, int weight = QFont::Normal) { m_st.font = QFont(family); m_st.font.setWeight(QFont::Weight(weight)); m_fontPx = px; }
    enum Align { Left, Center } textAlign = Left;

    // caminho
    void beginPath();
    void moveTo(double x, double y);
    void lineTo(double x, double y);
    void quadraticCurveTo(double cx, double cy, double x, double y);
    void bezierCurveTo(double c1x, double c1y, double c2x, double c2y, double x, double y);
    void arc(double cx, double cy, double r, double a0, double a1, bool anticlockwise = false);
    void ellipse(double cx, double cy, double rx, double ry, double rot, double a0, double a1, bool anticlockwise = false);
    void rect(double x, double y, double w, double h);
    void roundRect(double x, double y, double w, double h, double r);
    void roundRect(double x, double y, double w, double h, double tl, double tr, double br, double bl);
    void closePath();

    void fill(bool evenOdd = false);
    void stroke();
    void clip();
    void fillRect(double x, double y, double w, double h);
    void strokeRect(double x, double y, double w, double h);
    void fillText(const QString& s, double x, double y);
    void strokeText(const QString& s, double x, double y);

    static QBrush linear(double x0, double y0, double x1, double y1, const QVector<QPair<double, QColor>>& stops);
    static QBrush radial(double x0, double y0, double r0, double x1, double y1, double r1, const QVector<QPair<double, QColor>>& stops);

    QPainter& painter() { return m_p; }

private:
    struct State {
        QTransform T;
        QVector<double> dash;
        QBrush fill, strokeB;
        double lw, alpha, sb, sox, soy;
        Qt::PenCapStyle cap;
        Qt::PenJoinStyle join;
        QColor sc;
        QFont font;
        Align align;
    };
    void pushStyle(State& s) const;
    void pullStyle(const State& s);
    QPointF map(double x, double y) const { return m_st.T.map(QPointF(x, y)); }
    void drawShadow(const QPainterPath& basePath, bool asStroke, double baseWidth);
    QPen makePen() const;

    QPainter& m_p;
    QTransform m_base;
    double m_fontPx = 10;
    State m_st;
    QVector<State> m_stack;
    QPainterPath m_path;    // em coordenadas da base (já transformado, como no canvas)
};
