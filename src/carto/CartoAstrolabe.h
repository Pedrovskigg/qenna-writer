#pragma once

// O Astrolábio: as quatro vistas nos pontos cardeais, a agulha gira o mapa
// (clique nela = norte), o arco da direita inclina, e o sol (ou a lua) anda
// pela borda com a hora (6h à direita, meio-dia em cima, 18h à esquerda).

#include <QWidget>

namespace carto {

class Astrolabe : public QWidget {
    Q_OBJECT
public:
    explicit Astrolabe(QWidget* parent = nullptr);
    void setState(double ang, double pitch, double hour, const QString& mode);
    void setLockedMode(const QString& mode);   // vista apagada (vazio = nenhuma)

signals:
    void modeRequested(const QString& mode);
    void angleDragged(double ang, bool quick);
    void pitchDragged(double pitch, bool quick);
    void hourDragged(double hour, bool final);
    void northRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    QPointF local(QPointF p) const;        // em coordenadas do instrumento (centro na origem)
    QString viewAt(QPointF l) const;
    void dragTo(QPointF l);

    double m_ang = 0, m_pitch = 40, m_hour = 14.4;
    QString m_mode = QStringLiteral("full");
    QString m_locked;
    enum class Drag { None, Hour, Tilt, Rot } m_drag = Drag::None;
    double m_a0 = 0, m_ang0 = 0;
    bool m_moved = false, m_near = false;
};

} // namespace carto
