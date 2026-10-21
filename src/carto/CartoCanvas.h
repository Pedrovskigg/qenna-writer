#pragma once

// A mesa do mapa: mostra o mundo pintado pelo núcleo, com zoom, rotação e
// inclinação, nuvens, a luz da hora, a cena por cima (estradas, sprites,
// nomes) e as ferramentas. A pintura cheia roda em segundo plano (a janela
// nunca trava); girando, aparece uma prévia em resolução menor e a nitidez
// volta quando a mão para.

#include "CartoDoc.h"
#include "CartoEdit.h"
#include "CartoPaint.h"
#include "CartoScene.h"

#include <QImage>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

class QThreadPool;

namespace carto {

enum class Tool { Pan, Area, Brush, Realm, Build, Names, Ruler };

// opções das ferramentas (a doca do painel mostra e muda)
struct ToolOpts {
    // territórios: seed · paint · neutral
    int realmSel = 0;
    QString realmTool = QStringLiteral("seed");
    double realmBrush = 22;
    // construir
    QString buildKind = QStringLiteral("castle");   // select castle tower lighthouse windmill temple ruins bridge village fields wall road erase
    int castleSize = 1;
    double buildBrush = 14;
    QString roadMode = QStringLiteral("route");      // route · hand
    QString roadKind = QStringLiteral("road");       // trail · road · royal
    // nomes
    bool namesAuto = true;
    QString density = QStringLiteral("normal");      // few · normal · many
    QString nameStyle = QStringLiteral("land");      // land · water
    // vista
    bool sprites = true;
};

class Canvas : public QWidget {
    Q_OBJECT
public:
    explicit Canvas(QWidget* parent = nullptr);
    ~Canvas() override;

    // o mapa (de quem chama; o canvas só aponta)
    void setDoc(Doc* d, bool refit);
    Doc* doc() const { return m_doc; }
    World* world() const { return m_world; }
    void worldReplaced();             // mundo trocado inteiro (gerar, área, desfazer): repinta tudo
    void layersReplaced();            // territórios mudaram (desfazer): repinta
    void repaintBox(const std::array<int, 4>& box);   // pedaço do mundo mudou (desfazer pincelada)
    void waitIdle();                  // espera a pintura de fundo (antes de mexer no mundo)

    // vista
    double angle() const { return m_ang; }
    double pitch() const { return m_pitch; }
    double hour() const { return m_hour; }
    QString mode() const { return m_mode; }   // full · atlas · ink · terrain
    QString terrainStyle() const { return m_style; }   // rich · mira
    void setView(double ang, double pitch, bool quick);
    void presetView(double ang, double pitch) { m_ang = ang; m_pitch = std::round(std::max(20.0, std::min(90.0, pitch))); }   // antes de pôr o mapa
    void setMode(const QString& mode);
    void setTerrainStyle(const QString& style);
    void setHour(double hour, bool force);
    void setShowClimate(bool on);
    bool showClimate() const { return m_showClimate; }
    void fit();
    void zoomBy(double f, QPointF at = QPointF(-1, -1));
    double zoom() const { return m_zoom; }
    double fitZoom() const { return m_fitZoom; }
    double sq() const;

    // ferramentas
    void setTool(Tool t);
    Tool tool() const { return m_tool; }
    void setBrush(Brush b) { m_brush = b; update(); }
    Brush brush() const { return m_brush; }
    void setBrushSize(double r) { m_brushSize = r; update(); }
    double brushSize() const { return m_brushSize; }
    void setBrushStrength(double s) { m_brushStr = s; }
    ToolOpts& opts() { return m_opts; }
    void optsChanged();               // a doca mudou uma opção
    QString selArt() const { return m_selArt; }
    void setSelArt(const QString& id);
    Art* selectedArt();
    QString selName() const { return m_selName; }
    void setSelName(const QString& ref);
    QString toolMessage() const { return m_toolMsg; }
    bool consumeNewName() { const bool v = m_newName; m_newName = false; return v; }   // nome livre acabou de nascer: a doca foca o texto
    void cancelAction();              // Esc: larga a estrada encadeada, a ponte no meio
    // edições com desfazer (a doca e o teclado usam): foto antes, muda, foto depois
    void realmEdit(const std::function<void(Info&)>& fn);
    void userEdit(const std::function<bool(Info&)>& fn);
    void nameEdit(const std::function<bool(LabelLayer&)>& fn);
    Scene& scene() { return m_scene; }
    QImage exportImage(double scale, const std::function<void(QPainter&)>& extra = {}, const std::vector<QRectF>& avoid = {});
    // régua: distância e tempo de viagem
    void clearRuler() { m_ruler.clear(); m_rulerDone = false; update(); }
    double leaguePx() const;           // quantos px da tela dá uma légua (1 légua = 6 km; 1 px do mundo = 0,6 km na escala Normal)
    SceneState sceneState() const;

    // tela ↔ mundo
    QPointF screenToPlane(QPointF s) const;
    std::array<double, 2> screenToWorld(QPointF s) const;
    QString timing() const { return m_timing; }
    bool busy() const { return m_jobs.load() > 0; }

signals:
    void viewChanged();                       // ângulo, inclinação, zoom ou modo
    void hourChanged(double hour);
    void ringRequested(QPoint at);            // clique com o botão direito (sem arrastar)
    void strokeFinished(Snapshot before, Snapshot after, int riverId);
    void areaAdded(std::shared_ptr<World> before);
    void layersEdited(std::shared_ptr<Doc::Layers> before);   // território, construção ou nome mudou
    void selectionChanged();                  // artefato, nome ou território selecionado
    void statusChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    View currentView(int step) const;
    const RealmPaint* prepareRealms();
    void paintPreview(int step);
    void requestFull();
    void adoptBase(std::shared_ptr<Render> r);
    void presentRect(int x0, int y0, int x1, int y1);
    void refreshLight();
    void refreshClimate();
    void paintBox(const std::array<int, 4>& box, bool live);
    void startStroke(QPointF pos, bool inverse);
    void moveStroke(QPointF pos);
    void endStroke();
    void dab(double qx, double qy, double nx, double ny);
    bool infoOn() const { return m_mode != QLatin1String("terrain"); }
    void layersDone(std::shared_ptr<Doc::Layers> before, bool repaint);

    // ferramentas das camadas (CartoCanvasTools.cpp)
    bool toolPress(QMouseEvent* e);
    bool toolMove(QMouseEvent* e);
    bool toolRelease(QMouseEvent* e);
    void drawToolOverlay(QPainter& p, const SceneState& st);
    void paintMap(QPainter& p, QSize sz, bool exporting);
    void drawRuler(QPainter& p, const SceneState& st);
    void drawGroundLayer(QPainter& p, const SceneState& st, QSize sz);
    void sceneChanged() { m_scene.invalidate(); m_sceneRev++; }
    void realmDown(QPointF pos, bool shift);
    void buildDown(QPointF pos);
    void namesDown(QPointF pos);
    std::optional<std::array<int, 2>> seedNear(QPointF s) const;
    Art* artNear(QPointF s);
    std::array<double, 2> artExtent(const Art& a) const;
    std::optional<Pt> snapRoad(QPointF s);
    void startRoadAt(const Pt& p);
    void buildDab(double x, double y);

    Doc* m_doc = nullptr;
    World* m_world = nullptr;
    Scene m_scene;
    ToolOpts m_opts;
    // pintura atual (pode ser a prévia, com step > 1)
    std::shared_ptr<Render> m_base;
    QImage m_present;                // o que vai pra tela (com a luz da hora)
    std::vector<uint8_t> m_lit;
    std::vector<float> m_lut;
    double m_litHour = kHourDefault;
    QImage m_climate;
    QImage m_clouds;
    int m_cloudPad = 0;
    QImage m_oceanTile, m_paperTile, m_atlasTile;

    // vista
    double m_ang = 0, m_pitch = 40, m_hour = kHourDefault;
    QString m_mode = QStringLiteral("full"), m_style = QStringLiteral("rich");
    bool m_showClimate = false;
    double m_zoom = 1, m_fitZoom = 1, m_panX = 0, m_panY = 0;

    // pintura em segundo plano
    QThreadPool* m_pool = nullptr;
    std::atomic<int> m_jobs{ 0 };
    int m_gen = 0;
    QTimer m_settle;
    QString m_timing;

    // mouse
    Tool m_tool = Tool::Pan;
    Brush m_brush = Brush::Land;
    double m_brushSize = 36, m_brushStr = 0.5;
    enum class Drag { None, Pan, Turn, Area, Stroke, Tool } m_drag = Drag::None;
    QPointF m_press, m_last, m_hover;
    bool m_hasHover = false, m_moved = false;
    double m_ang0 = 0, m_pitch0 = 0, m_panX0 = 0, m_panY0 = 0;
    QRectF m_sel;
    // pincelada
    struct Stroke {
        std::vector<float> h, m, k;   // o mundo antes
        std::optional<std::array<int, 4>> box, pending;
        double lastX = 0, lastY = 0;
        DabState st;
        Path river;                   // o rio guarda o traço (pro nome correr junto)
    };
    std::unique_ptr<Stroke> m_stroke;
    QTimer m_strokeTick;

    // estado das ferramentas das camadas
    QString m_selArt, m_selName, m_toolMsg;
    std::shared_ptr<Doc::Layers> m_editBefore;   // foto antes do arrasto em curso
    std::vector<QPointF> m_trail;                // rastro na tela (pincel de território, muralha, estrada, nome)
    Path m_trailWorld;
    Pt m_lastWorld{};
    struct SeedDrag { int r, k; double x, y; };
    std::optional<SeedDrag> m_seedDrag;
    bool m_rstroke = false, m_bstroke = false;
    struct ArtDrag { QString id; double ox, oy; bool moved; };
    std::optional<ArtDrag> m_artDrag;
    std::optional<Pt> m_bridgeStart;
    std::unique_ptr<Router> m_roadFrom;
    int m_roadFromInfoVer = -1;
    struct NameDrag { QString ref; double x0, y0; bool moved; int edit; int custom; double bx, by; Path pts; };
    std::optional<NameDrag> m_nameDrag;
    bool m_nameNew = false;
    bool m_newName = false;
    std::vector<QRectF> m_avoid;   // lugares que os nomes evitam no PNG
    // camada da cena guardada (plantações, estradas, sprites) com folga em volta: arrastar o mapa só desloca
    QImage m_sceneCache;
    QString m_sceneKey;
    double m_cachePanX = 0, m_cachePanY = 0;
    int m_cacheMargin = 0;
    int m_sceneRev = 0;
    Path m_ruler;                  // pontos da régua (mundo)
    bool m_rulerDone = false;
    double m_nameNewLen = 0;
};

} // namespace carto
