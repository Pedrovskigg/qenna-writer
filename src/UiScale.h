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

} // namespace UiScale
