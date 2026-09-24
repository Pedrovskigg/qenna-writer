#pragma once

#include <QVector>
#include <QWidget>

// Contador minimizado: a ALTERNATIVA de tamanho pra quem acha o contador
// normal grande demais (o normal é o m_body clássico ou o CounterFaceWidget).
// Um widget só, desenhado com QPainter, com os estilos:
//
//   line        "702 palavras · 0min" numa linha, meta num fio de 2px na borda
//   columns     dois blocos valor/rótulo lado a lado, barra de 3px embaixo
//   ring        anel da meta à esquerda, os dois valores empilhados ao lado
//   pill        a própria pílula enche de cor conforme a meta avança
//   ringlet     pílula com um anel de 16px na ponta
//   odometer    dígitos em casinhas; cresce além de 5 casas, espaço a cada 3
//   bricks-mini a linha com 10 tijolinhos de 10% embaixo
//   week-mini   7 barrinhas (uma por dia) + os dois valores empilhados
//   ruler       régua com marcas a cada 10% e uma seta que anda
//   card-mini   ficha pautada de duas linhas, IBM Plex Mono, sem meta
//   tube        tubo de 4px na borda esquerda que enche de baixo pra cima
//
// Não guarda estado de contagem: o WordCountPanel empurra os textos, o
// percentual e a semana via setData() a cada refresh. O "Reinicia em…" vai
// no tooltip.
class MiniCounterWidget : public QWidget {
    Q_OBJECT
public:
    explicit MiniCounterWidget(QWidget* parent = nullptr);

    static bool isMiniStyle(const QString& key);

    void setStyleKey(const QString& key);
    QString styleKey() const { return m_style; }

    // value/unit de cada slot; unit vazio = o valor já se explica (ex.: "12min").
    // unitShort é o rótulo curto (duas colunas, ficha): nunca vazio.
    struct Slot { QString value; QString unit; QString unitShort; };
    // week: 7 valores, o último é hoje; weekGoal na mesma unidade (0 = sem meta).
    void setData(const Slot& s1, const Slot& s2, int goalPercent, bool showGoal,
                 const QVector<int>& week = {}, int weekGoal = 0);

    // Escala do desenho inteiro (1.0 = tamanho de referência).
    void setScale(qreal scale);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

signals:
    void clicked();
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    QSize baseSize() const;   // tamanho em escala 1.0
    QFont valueFont(int px) const;
    QFont labelFont(int px) const;
    int lineContentWidth() const;
    // Dígitos do slot 1 pro odômetro; vazio se o valor não for só número
    // (ex.: "12min"), e aí o odômetro cai no desenho da linha.
    QString odometerDigits() const;
    int odometerCellsWidth() const;
    int stackedTextWidth() const;   // valor 1 (12px) sobre valor 2 (10px)

    QString m_style = QStringLiteral("columns");
    Slot m_s1, m_s2;
    int m_pct = 0;
    bool m_showGoal = true;
    QVector<int> m_week;
    int m_weekGoal = 0;
    qreal m_scale = 1.0;
};
