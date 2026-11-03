#pragma once

// As fotos do painel e do Caderno: o planeta em 3D e a peça em 3D, que giram sozinhos e giram no arrasto.

#include "GalaxyCanvas.h"

#include <QImage>
#include <QTimer>
#include <QWidget>

namespace galaxy {

class PlanetView : public QWidget {
public:
    PlanetView(Canvas* canvas, const Body* body, bool zoomable, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    Canvas* m_canvas;
    const Body* m_body;
    bool m_zoomable;
    double m_spin = 0.6, m_pitch = 0.25, m_zoom = 1;
    bool m_drag = false;
    QPointF m_last;
    QTimer m_timer;
    int m_tabD = 0;
    double m_tabPitch = -9;
    SphereTable m_tab;
    QImage m_img;
};

class ModelView : public QWidget {
public:
    ModelView(Canvas* canvas, const std::string& type, uint32_t seed, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    Canvas* m_canvas;
    std::string m_type;
    uint32_t m_seed;
    double m_rot = 0.5, m_pitch = 0.78;
    bool m_drag = false;
    QPointF m_last;
    QTimer m_timer;
};

// a peça ou o colosso sozinho num quadro: nas cartinhas e na foto do painel
void paintModelArt(QPainter& g, const std::string& id, uint32_t seed, int D, double t, double rot, double pitch, const QString& key = QString());
// a arte de uma cartinha do catálogo
QImage artImage(Canvas* canvas, const std::string& id, ItemKind kind, int px, uint32_t seed = 91, Body* preview = nullptr);

} // namespace galaxy
