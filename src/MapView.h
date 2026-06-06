#pragma once

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QWidget>

#include <functional>

class MapPinsStore;
class QVariantAnimation;

// Mapa-múndi do Pensário (Fase 1): desenha países, fronteiras e cidades/capitais
// do Natural Earth com QPainter, projeção equiretangular, adotando o tema.
// Zoom com a roda (no cursor) e arrastar pra mover.
class MapView : public QWidget {
    Q_OBJECT
public:
    explicit MapView(QWidget* parent = nullptr);

    // Navega o mapa até uma região (bounds em lon/lat) ou um ponto (cidade).
    void flyToBounds(const QRectF& lonlat);
    void flyToPoint(double lon, double lat);

    // Modo régua: cliques marcam 2 pontos e medem distância + diferença de fuso.
    void setRulerMode(bool on);
    bool rulerMode() const { return m_ruler; }

    // Pins de referência do projeto.
    void setPinsStore(MapPinsStore* s);
    void setAddPinMode(bool on); // clique no mapa cria um pin no ponto

    // Modo de exibição: texturizado (físico, cores próprias) ou simples (tema).
    void setTextured(bool on);
    bool textured() const { return m_textured; }

    // Projeção: plano (equiretangular) ou globo 3D (ortográfica).
    void setGlobeMode(bool on);
    bool globeMode() const { return m_globe; }

signals:
    // Clique numa feição: título, subtítulo, linhas "Rótulo: valor" e o ISO do
    // país (pra bandeira; vazio = sem bandeira).
    void featureClicked(QString title, QString subtitle, QStringList details, QString flagIso);
    void pinRequestedAt(double lon, double lat); // clique no modo "fixar pin"
    void pinClicked(QString id);                 // clique num pin existente

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QPointF project(double lon, double lat) const; // graus -> pixels
    QPointF screenToLonLat(const QPointF& s) const; // pixels -> graus
    void fitToWidget();                              // enquadra o mundo inteiro
    void hitTest(const QPoint& pos);                 // clique -> featureClicked
    // Identifica a feição sob um ponto (cidade > estado > país) e emite a ficha.
    // `proj` mapeia lon/lat -> tela e diz se o ponto está visível (pro globo).
    void pickFeatureAt(const QPointF& lonlat, const QPoint& screenPos, double r,
                       const std::function<QPointF(double, double, bool*)>& proj);
    void emitCountryFicha(int idx);                  // monta e emite a ficha do país
    int countryAt(const QPointF& lonlat) const;      // índice do país sob o ponto
    int stateAt(const QPointF& lonlat) const;        // índice do estado sob o ponto

    // Globo (projeção ortográfica).
    double globeRadius() const;
    double globeDetailRatio() const; // equivalente ao "r" de zoom do mapa plano
    double scaleToGlobeZoom(double scale) const; // escala do plano -> zoom do globo
    void startGlobeFly(double lon, double lat, double zoom); // anima o giro até o alvo
    QPointF projectGlobe(double lon, double lat, bool* visible) const;
    QPointF invGlobe(const QPointF& screen, bool* ok) const;
    void paintGlobe(QPainter& p);
    const QImage& relief() const;  // textura de relevo equiretangular (lazy)
    const QImage& oceanTex() const; // textura de profundidade do oceano (lazy)
    void buildGlobeShade();        // projeta relevo + oceano no disco (cache)

    double m_scale = 1.0;   // pixels por grau
    double m_fitScale = 1.0; // escala que enquadra o mundo (zoom out mínimo)
    QPointF m_offset;       // translação em pixels
    bool m_fitted = false;
    bool m_dragging = false;
    QPoint m_lastMouse;
    QPoint m_pressPos;

    bool m_ruler = false;
    int m_rulerCount = 0;   // 0 = vazio, 1 = ponto A, 2 = A e B
    QPointF m_rulerA;       // lon/lat
    QPointF m_rulerB;

    MapPinsStore* m_pins = nullptr;
    bool m_addPin = false;
    bool m_textured = true;

    int m_hoverCountry = -1; // hover ao vivo
    int m_hoverState = -1;
    QPoint m_hoverPos;

    bool m_globe = false;
    double m_globeLon0 = -50.0; // centro de visão (graus)
    double m_globeLat0 = 12.0;
    double m_globeZoom = 1.0;   // multiplicador do raio

    // Relevo (hillshade) + oceano (profundidade) e cache das projeções no globo.
    mutable QImage m_relief;
    mutable bool m_reliefTried = false;
    mutable QImage m_oceanTex;
    mutable bool m_oceanTried = false;
    QImage m_globeShade;
    QImage m_globeOcean;
    double m_shadeLon0 = 1e9, m_shadeLat0 = 1e9, m_shadeZoom = -1.0;
    QSize m_shadeSize;

    // Animação de "voo" do globo até um lugar (busca/navegação).
    QVariantAnimation* m_flyAnim = nullptr;
    double m_flyLonFrom = 0.0, m_flyLonDelta = 0.0;
    double m_flyLatFrom = 0.0, m_flyLatTo = 0.0;
    double m_flyZoomFrom = 0.0, m_flyZoomTo = 0.0;
};
