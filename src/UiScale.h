#pragma once

#include <QObject>

namespace UiScale {

// Fator de escala das barras (TopToolbar, LeftBar) e seus ícones. Independente
// do tema/fonte do editor — só afeta a "chrome" da UI. Persistido em QSettings
// (global, não por projeto).
class Manager : public QObject {
    Q_OBJECT
public:
    static Manager* instance();

    qreal scale() const { return m_scale; }
    void setScale(qreal factor);

    static qreal minScale() { return 0.75; }
    static qreal maxScale() { return 1.5; }
    static qreal defaultScale() { return 1.0; }

signals:
    void scaleChanged();

private:
    Manager();
    void load();
    void save() const;

    qreal m_scale = 1.0;
};

// Free function para callsites curtos.
qreal scale();

// Zoom do app inteiro (painéis, gavetas, textos, janelas), diferente da escala
// acima, que só mexe nas barras. Vira o QT_SCALE_FACTOR, que o Qt só lê antes
// do QApplication nascer: muda ao reabrir o app.
//
// Existe por causa do Mac: a tela do MacBook é bem mais densa que um monitor
// de PC, e o mesmo tamanho em pixels sai fisicamente menor (a Marley lia as
// pastas e os títulos apertando os olhos). Lá o padrão já nasce maior.
qreal appZoom();                 // o salvo, ou o padrão da plataforma
qreal defaultAppZoom();
void setAppZoom(qreal factor);   // grava; vale na próxima abertura
inline qreal minAppZoom() { return 0.8; }
inline qreal maxAppZoom() { return 1.6; }
// Chamada no main(), ANTES do QApplication: lê o salvo direto do QSettings
// (o app ainda não tem nome) e liga o QT_SCALE_FACTOR.
void applyAppZoomBeforeApp(const QString& orgAndAppName);

} // namespace UiScale
