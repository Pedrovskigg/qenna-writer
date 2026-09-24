#pragma once

#include <QVector>
#include <QWidget>

// Corpo do contador no tamanho normal (os enxutos são o MiniCounterWidget).
// Desenha o painel inteiro com QPainter, cabeçalho incluso, e por isso
// escala sem refazer layout (setScale):
//
//   classic          os dois cards de sempre + barra da meta + "Reinicia em…"
//   face-ring        anel da meta com % dentro, os dois dados listados ao lado
//   face-brickring   o mesmo anel, feito de 20 tijolinhos de 5%
//   face-ringbricks  anel liso + fileira de 20 tijolinhos, um por dia
//   face-week        os últimos 7 dias em colunas, com a linha da meta
//   face-bricks      os dois dados numa peça dividida + meta em 20 tijolinhos
//   face-card        ficha de fichário pautada, em IBM Plex Mono
//
// Não guarda estado de contagem: o WordCountPanel empurra tudo via setData().
// O clique no "Estatísticas ›" sai pelo sinal statsClicked; qualquer outro
// clique é ignorado e sobe pro m_body (que alterna o modo full).
class CounterFaceWidget : public QWidget {
    Q_OBJECT
public:
    explicit CounterFaceWidget(QWidget* parent = nullptr);

    static bool isFaceStyle(const QString& key);

    struct Data {
        QString title;        // "Contador"
        QString statsText;    // "Estatísticas ›"
        QString label1, value1, unit1;   // unit = rótulo curto ("palavras", "hoje")
        QString label2, value2, unit2;
        QString fullLabel1, fullLabel2;  // títulos dos cards do clássico ("Palavras (atual)")
        bool showGoal = true;            // só o clássico obedece: nos outros a meta é o desenho
        int pct = 0;
        QString pctCaption;   // "da meta"
        QString goalLine;     // "Reinicia em 1h 47min" / "Meta atingida!"
        QString fraction;     // "702 / 1.000"
        QVector<int> week;    // 7 valores, o último é hoje
        int weekGoal = 0;
        QStringList weekDays; // 7 rótulos, o último é "hoje"
        QVector<bool> days;   // 20 dias, o último é hoje (meta batida?)
        QString daysCaption;  // "15 de 20 dias"
    };
    void setStyleKey(const QString& key);
    void setData(const Data& d);
    // 1.0 = 256px de largura. O desenho é feito em coordenadas lógicas e
    // multiplicado pelo painter; o hit-rect do "Estatísticas ›" também.
    void setScale(qreal scale);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

signals:
    void statsClicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    QSize baseSize() const;   // tamanho em escala 1.0

    QString m_style = QStringLiteral("classic");
    qreal m_scale = 1.0;
    Data m_d;
    QRectF m_statsRect;       // coordenadas lógicas
    bool m_statsHover = false;
    bool m_statsPressed = false;
};
