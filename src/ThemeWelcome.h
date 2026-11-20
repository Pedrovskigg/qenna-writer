#pragma once

#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QRectF>
#include <QStringList>
#include <QTimer>
#include <QWidget>

// Escolha do tema no primeiro projeto (19/11/2026, ideia do Pe; lab
// P38TPcxBuWw89UtP4xFyj7). Uma camada por cima da janela inteira com oito
// temas em quatro grupos (Escuros, Claros, Amarelados, Estampados). Passar o
// mouse num cartão mostra a janela do Qenna desenhada com aquele tema atrás e
// a própria janela de escolha veste as cores dele; tirar o mouse da janela de
// escolha some com ela pra ver o tema inteiro. Não tem "pular": o "Ignorar"
// sorteia um dos oito numa roletinha. Nada é aplicado de verdade antes da
// escolha: quem aplica é a MainWindow, no sinal chosen.
class ThemeWelcome : public QWidget
{
    Q_OBJECT
public:
    explicit ThemeWelcome(QWidget* parent);

    // Os oito, na ordem dos grupos (dois por grupo).
    static QStringList themeIds();
    void open();

signals:
    void chosen(const QString& themeId, bool random);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent*) override;
    void keyPressEvent(QKeyEvent* e) override;
    bool event(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override;

private:
    struct Pal {
        QColor panel, line, ink, muted, accent, app, hover, subtle, onAccent;
    };
    static Pal palFor(const QString& themeId);
    static Pal mix(const Pal& a, const Pal& b, qreal t);

    void relayout();
    int cardAt(const QPointF& p) const;
    void setFocusTheme(const QString& id);
    void pick(int card);
    void finish(const QString& id, bool random);
    void startSpin();
    void spinStep();
    void kick();
    void tick();
    QString shownName() const;
    QPixmap sceneFor(const QString& id, const QSize& size, bool thumb);

    QStringList m_ids;
    QStringList m_names;
    int m_picked = -1;
    int m_hover = -1;
    int m_spinCard = -1;
    bool m_spinning = false;
    bool m_done = false;
    int m_spinStep = 0;
    int m_spinTarget = 0;

    // Fundo: o tema em foco (vazio = o app de verdade, só escurecido) e o anterior, pra cruzar.
    QString m_focus;
    QString m_prevFocus;
    qreal m_bgT = 1.0;          // 0 = anterior, 1 = atual

    // Valores que deslizam (um timer só puxa todos pro alvo).
    qreal m_dim = 0.0, m_dimTarget = 0.55;
    qreal m_away = 0.0, m_awayTarget = 0.0;
    qreal m_show = 0.0, m_showTarget = 1.0;   // entrada e saída da camada inteira
    Pal m_pal, m_palTarget;
    QTimer m_anim;
    QTimer m_spinTimer;

    // Geometria (coordenadas da camada).
    QRectF m_dlg, m_title, m_sub, m_footLine, m_more, m_skip, m_go;
    QList<QRectF> m_cards;      // o cartão inteiro (área de clique)
    QList<QRectF> m_shots;      // a miniatura dentro dele
    QList<QRectF> m_heads;      // títulos dos grupos
    bool m_overSkip = false, m_overGo = false;

    QHash<QString, QPixmap> m_sceneCache;   // id + tamanho → janela desenhada
};
