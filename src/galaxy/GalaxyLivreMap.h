#pragma once

// Criação Livre: o mapa da galáxia em miniatura, só com o que nasceu aqui. Sempre na vista Atlas (galáxia bem
// apagada, territórios fortes), em 3D como o Galaxy Maps: inclina e gira, zoom contínuo da galáxia até dentro do
// sistema, com os planetas nas órbitas. Segue o que está escolhido na prateleira. Mora no pé da coluna da
// esquerda e abre em tela grande. A forma dos territórios é a orgânica (GalaxyOrganic).

#include "GalaxyOrganic.h"

#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <functional>
#include <vector>

class QToolButton;

namespace galaxy {

class Doc;
class Livre;
struct System;

class LivreMap : public QWidget {
public:
    LivreMap(Livre* owner, Doc* doc, bool big, QWidget* parent);
    void follow(const QString& sel);        // voa até o escolhido; na primeira vez já começa lá
    void frameOn(const QString& sel);       // vai direto, sem voo
    void fitAll();
    void changed() { m_dirty = true; update(); }
    std::function<void(const QString&)> picked;

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void leaveEvent(QEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    struct Cam { double x = 0, y = 0, z = 0, h = 0, yaw = -0.4, tilt = 0.95; };
    struct Anchor { QString key; System* sys = nullptr; double x = 0, y = 0, h = 0; bool ghost = false, solo = false; };
    struct Hit { QString key; QPointF at; double r = 0; bool area = false; };
    struct Frame { double x = 0, y = 0, z = 0, h = 0; };
    struct TerrCache { QString key; TerrImage im; };

    double fitZ() const;
    void proj(double x, double y, double h, double& sx, double& sy) const;
    void toWorld(double sx, double sy, double& wx, double& wy) const;
    std::vector<Anchor> anchors() const;
    QString anchorOf(const QString& key) const;
    bool posOf(const QString& anchor, double& x, double& y, double& h) const;
    double sysR(const Anchor& a) const;
    Frame frameAll() const;
    Frame frameOf(const QString& sel) const;
    void flyTo(const Frame& f, double ms = 900);
    void setTilt(double t);
    void step();
    bool busy() const;
    void render();
    Hit hitAt(QPointF p) const;
    void layoutButtons();
    void syncButtons();

    Livre* m_owner;
    Doc* m_doc;
    bool m_big;
    Cam cam;
    struct Fly { bool on = false; qint64 t0 = 0; double ms = 0, x0 = 0, y0 = 0, z0 = 0, h0 = 0, x = 0, y = 0, z = 0, h = 0, bump = 0; } m_fly;
    struct TiltA { bool on = false; qint64 t0 = 0; double from = 0, to = 0; } m_tiltA;
    struct Down { bool on = false, moved = false, rot = false; QPointF at; double wx = 0, wy = 0, yaw = 0, tilt = 0; Qt::MouseButton btn = Qt::NoButton; } m_down;
    QElapsedTimer m_clock, m_wheelAt;
    QTimer m_anim, m_pulse, m_settle, m_peekTimer;
    bool m_dirty = true, m_started = false;
    QString m_lastSel, m_hover;
    QImage m_frame;
    std::vector<Hit> m_hits;
    QHash<QString, TerrCache> m_terr;
    QImage m_galSmall;
    qint64 m_galKey = -1;
    QPointF m_here;
    double m_hereR = 0;
    bool m_hasHere = false;
    QWidget* m_hot = nullptr;           // o ponto onde a prévia se apoia
    QWidget* m_ctl = nullptr;
    QToolButton* m_b3d = nullptr;
};

} // namespace galaxy
