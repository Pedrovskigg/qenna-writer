#pragma once

// O mapa da galáxia: câmera, desenho de tudo o que está no céu e o mouse (porte do index.html do Lab).
// Pinta numa imagem própria e só copia pra tela; os botões e cartões moram no Panel, por cima.

#include "GalaxyDoc.h"
#include "GalaxyViews.h"
#include "GalaxyStructures.h"

#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QTimer>
#include <QWidget>

#include <deque>
#include <map>
#include <functional>
#include <memory>
#include <unordered_map>

namespace galaxy {

enum class Tool { Pan, Realm, Build, Names, Explore, Edit, Ruler, Caderno };
enum class RealmMode { Seed, Paint, Neutral };

// Uma conexão no mapa: arco entre dois lugares, com o nome no meio.
struct LinkView { QString id, a, b, label; QColor color; bool solid = false; };
struct Hit { QString key; double x = 0, y = 0, r = 0; bool label = false; QRectF box; };
struct RingHit { QString key; double x, y, r0, r1, cT, roll; };
struct LiveHit { Body* b; Build* build; double x, y, r; };

struct PlaceItem { std::string id; ItemKind kind; QString name; bool art = false; };
struct Snap {
    bool ok = false; QString msg;
    System* sys = nullptr; Body* host = nullptr;
    double hx = 0, hy = 0, hr = 0, rp = 0, ang = 0, sang = 0, cx = 0, cy = 0, rr = 0, sx = 0, sy = 0, wx = 0, wy = 0, mx = 0, my = 0;
    bool deep = false;
};

class Canvas : public QWidget {
    Q_OBJECT
public:
    explicit Canvas(QWidget* parent = nullptr);
    ~Canvas() override;

    struct Cam { double x = 0, y = 0, z = 400, h = 0, tilt = 0, yaw = 0; QString follow; } cam;
    struct Layers { bool terr = true, names = true, orb = true, traffic = false; } ly;

    // A vista Céu mostra só o céu, sem desligar nada.
    bool onlySky() const { return view == QLatin1String("terrain"); }
    bool layTerr() const { return ly.terr && !onlySky(); }
    bool layNames() const { return ly.names && !onlySky(); }
    bool layOrb() const { return ly.orb && !onlySky(); }
    bool layTraffic() const { return ly.traffic && !onlySky(); }

    Doc* doc = nullptr;
    void setDoc(Doc* d);
    void docChanged();          // algo mudou por fora: refaz os caches do que depende disso

    // estado que a interface mexe
    QString view = QStringLiteral("full");
    QString artMode = QStringLiteral("ink");
    bool skyClean = false, sprites = true, frameOn = false, play = true;
    int trafficLevel = 2;
    double worldScale = 1;      // escala das construções
    Tool tool = Tool::Pan;
    QString sel = QStringLiteral("GAL");
    QString focusKey;
    double padLeft = 0, padBottom = 0;   // o espaço que painel e catálogo ocupam

    void setView(const QString& m);
    const Skin* skin() const { return m_sk; }
    void setTilt(double t, double yaw);
    void setTiltNow(double t) { cam.tilt = t; update(); }
    void flyTo(double x, double y, double z, double ms = 950, const QString& follow = QString(), double h = 0);
    void frameOfKey(const QString& key, double& x, double& y, double& z);
    double heightOf(const QString& key) const;
    void select(const QString& key, bool flyThere);
    void setLinks(std::vector<LinkView> links);
    void frameLink(const QString& a, const QString& b, double top, double bottom);
    QString linkAt(double x, double y) const;
    bool pickingLink = false;   // "Nova conexão": o próximo clique escolhe o outro lado
    void fitMap();
    void setFocus2(const QString& key);
    double fitZ() const;
    double zMin() const { return fitZ() * 0.6; }
    double zMax() const { return fitZ() * 9000; }
    QPointF viewCenter() const;
    void proj(double x, double y, double h, double& ox, double& oy) const;
    void toWorld(double sx, double sy, double& wx, double& wy) const;
    double bodyHeight(const System& s) const { return s.layH; }
    bool cdOpen() const { return tool == Tool::Caderno; }

    // construção (GalaxyCanvasTools.cpp)
    std::unique_ptr<PlaceItem> placing;
    QString buildSize = QStringLiteral("m");
    void armItem(const PlaceItem* it);
    Snap snapFor(const PlaceItem& it, double mx, double my);
    void placeClick(double mx, double my);
    void buildClick(double mx, double my);
    void removeThing(const QString& k);
    void refreshAfterEdit(Body* b = nullptr);
    QString ghostMessage() const { return m_hasGhost ? m_ghost.msg : QString(); }
    Body* previewOf(const PlaceItem& it) { return previewBody(it); }
    void ringOn(QPainter& g, const Body& b, double x, double y, double r, bool front, double ky);
    bool removable(const QString& k) const;
    double buildRadiusOf(const Build& b, const System* s, const Body* host) const;
    void buildPos(const Build& b, const System* s, const Body* host, double& x, double& y) const;

    // territórios
    RealmMode realmMode = RealmMode::Seed;
    QString realmActive;
    int realmReach = 20, realmBrush = 22;
    void realmClick(double mx, double my, bool shift);
    void assignSystems(const std::vector<System*>& list, Region* r, const QString& label);

    // régua e nomes
    std::vector<QPointF> rulerPts;
    bool rulerDone = false;
    QString nameSel;

    // desfazer
    struct UndoEntry { std::function<void()> undo, redo; QString label; };
    std::vector<UndoEntry> undoStack, redoStack;
    void pushUndo(const QString& label, std::function<void()> undo, std::function<void()> redo = nullptr);
    void undo();
    void redo();

    // esferas e texturas: o painel também usa
    const Texture& textureOf(const Body& b, int w);
    QImage smallBall(const Body& b, int px, const V3& L = V3 { -0.62, 0.3, 0.72 }, double dpr = 1);
    void invalidateBody(const Body& b);

    // exportar
    QImage exportImage(double scale, bool withTitle);

    // tráfego: o que o teste e o painel leem
    struct Stop { Body* p = nullptr; Body* host = nullptr; bool minor = false; Build* b = nullptr; };
    std::vector<Stop> stopsOf(System& s) const;

    QString pickAt(double x, double y);
    const std::vector<Hit>& hits() const { return m_hits; }
    double simTime() const { return doc ? doc->simT : 0; }
    void setSimTime(double t) { if (doc) doc->simT = t; update(); }
    void tickNow();    // pra teste: um quadro

signals:
    void selectionChanged(const QString& key);
    void linkClicked(const QString& id);
    void linkPicked(const QString& key);
    void clickedPlace(const QString& key);       // Explorar/Editar: o clique escolheu um lugar
    void requestRing(const QPointF& pos, const QString& target);
    void hintChanged();
    void toast(const QString& text, bool withUndo);
    void cameraMoved();
    void toolDockChanged();
    void historyChanged();
    void nameClickRequested(double mx, double my, const QString& key, bool isLabel);
    void focusChanged();
    void pressed();                              // o mouse desceu no mapa: fecha o que estiver aberto
    void tipChanged(const QString& key, const QPointF& at);
    void peekChanged(const QString& key);        // o mouse parou em cima de outro lugar (vale também em Mover)
    void docEdited();                            // algo mudou no documento: hora de marcar pra salvar

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    struct Label { QString text; double x, y; bool center; double pri; QFont font; QColor color; QString key; double alpha = 1; bool hasAlpha = false; };
    struct DrawCtx { QPainter* p; double z, cT; };

    // quadro
    void tick();
    bool stepFly(qint64 now);
    bool stepTilt(qint64 now);
    bool stepFocus(double dt);
    void render(QPainter& p);
    void drawBackdrop(QPainter& p, double zoomK);
    void drawSystemNode(QPainter& p, System& s, std::vector<Label>& labels, double z, double cT, double boost);
    void drawStar(QPainter& p, const System& s, double x, double y, double sr, double cT);
    void drawSystemOpen(QPainter& p, System& s, double x, double y, double open, double z, double cT, std::vector<Label>& labels);
    void drawBody(QPainter& p, const Body& b, double x, double y, double r, double ux, double uy);
    void drawLive(QPainter& p, const Body& b, double x, double y, double r, double ux, double uy);
    void shadeOver(QPainter& p, double x, double y, double r, double ux, double uy, double k);
    void drawRing(QPainter& p, const Body& b, double x, double y, double r, bool front);
    const QImage& ringTex(const Body& b);
    const QImage& glow(const std::array<int, 3>& c);
    void ring(QPainter& p, double x, double y, double r);
    void drawStarMega(QPainter& p, Build& b, System& s, double x, double y, double z, double cT, double alpha, char part);
    void drawOrbitBuild(QPainter& p, Build& b, System& s, double cx, double cy, double pr, double z, double cT, double alpha, std::vector<Label>& labels, double showNames);
    void paintPiece(QPainter& p, Build& b, double px, double py, double r, double toX, double toY, double alpha);
    void drawDeepBuilds(QPainter& p, std::vector<Label>& labels, double z, double cT);
    void drawGhost(QPainter& p, double z, double cT);
    void drawMapExtras(QPainter& p, std::vector<Label>& labels, double z, double cT);
    void placeLabels(QPainter& p, std::vector<Label>& labels);
    double megaRadius(const Build& b, const System& s) const;
    double ringPitch() const { return std::min(1.35, cam.tilt + 0.5); }
    double orbRingR(double pr, double scale) const { return pr * (1.28 + 0.34 * scale); }
    double orbRingLight(const System& s, const Body& p) const { return std::atan2(s.y - p.layY, s.x - p.layX) + cam.yaw + 0.32; }
    double pieceRot(const Build& b) const;
    double piecePitch(const Build& b) const { return 0.78 + cam.tilt * 0.7 + b.pitchU; }
    QColor accent(double a) const;
    QFont uiFont(int weight, double px) const;

    // tráfego (GalaxyCanvasTraffic.cpp)
    struct Route { System* a; System* b; int id; QString color; bool gate; };
    struct Ship { int i, kind; double size, T, ph; };
    struct TrafficState { QString key; std::vector<Route> routes; std::unordered_map<int, std::pair<int, std::vector<Ship>>> sys; } m_tr;
    std::vector<Build*> portalsOf(const System& s) const;
    int trafficScore(System& s) const;
    void trafficModel();
    const std::vector<Ship>& shipsOf(System& s);
    void drawTraffic(QPainter& p, Region* fr, double z, double cT, double zoomK);
    void drawLanes(QPainter& p, Region* fr, double z, double zoomK);
    void drawSystemShips(QPainter& p, System& s, double z, double cT);
    void drawJump(QPainter& p, double px, double py, double dx, double dy, double k, const std::array<int, 3>& c, double size);
    std::vector<System*> m_openSystems;

    // territórios e foco (GalaxyCanvasRegions.cpp)
    struct RegionPrims { int n = 0; std::vector<System*> pts; struct Link { double x, y, dx, dy, l; }; std::vector<Link> links; };
    struct TerrLayer { QString key; QImage img, mask; int x = 0, y = 0, w = 0, h = 0; bool empty = true; std::vector<float> field; };
    QHash<QString, RegionPrims> m_prims;
    QHash<QString, TerrLayer> m_terr;
    const RegionPrims& regionPrims(const Region& r);
    double regionDist(const RegionPrims& P, double x, double y) const;
    TerrLayer& territoryLayer(const Region& r);
    void drawRegions(QPainter& p, double zoomK, const std::vector<Region*>& list, double mul = 1);
    QString regionAt(double sx, double sy);
    void paintFocus(QPainter& p, Region& r, double zoomK, double cT, std::vector<Label>& labels);
    bool inFocusArea(double sx, double sy);
    QColor regionInk(const Region& r) const;
    struct FX { QString key; QImage blur, patch; } m_fx;
    double m_focusAmt = 0;
    QString m_focusShown;

    // vistas de arte (GalaxyCanvasArt.cpp)
    const Skin* m_sk = nullptr;
    struct ArtAssets { QImage wash, tile; std::vector<CurveSegs> curves; Stipple stip; bool hasStip = false; };
    struct ArtState { QString key; QImage layer; Field field; const QImage* gal = nullptr; qint64 galKey = 0; QHash<QString, std::shared_ptr<ArtAssets>> per; int budget = 1; } m_art;
    struct TonedSheet { QImage img; qint64 at = 0; QString sk; double dx = 0, dy = 0, sw = 0, sh = 0, R = 0; };
    QHash<QString, TonedSheet> m_toned;
    ArtAssets& artAssets();
    void drawArtBackdrop(QPainter& p, double zoomK);
    void drawGraticule(QPainter& g, double a, double b, double cc, double d, double ox, double oy, double zoomK);
    void drawRhumbs(QPainter& g, double a, double b, double cc, double d, double ox, double oy, double zoomK);
    void drawArtStarFar(QPainter& p, const System& s, double x, double y);
    void artSun(QPainter& p, double x, double y, double r, const std::array<int, 3>& c);
    void drawArtStarOpen(QPainter& p, const System& s, double x, double y, double sr, double cT);
    const QImage* artThumb(const Body& b, int D);
    void drawArtBody(QPainter& p, const Body& b, double x, double y, double r, double ux, double uy);
    void toned(QPainter& p, const QString& id, double x, double y, double R, const std::function<void(QPainter&)>& fn);

    // camadas e caches
    QImage m_frame, m_bg;
    QString m_bgKey;
    QImage m_galSmall;
    double m_dpr = 1;
    bool m_exporting = false;
    struct TexEntry { std::shared_ptr<Texture> best; int bw = 0, pend = 0; uint32_t rev = 0; };
    QHash<QString, TexEntry> m_tex;
    struct ThumbJob { QString key; const Body* b; int D; int want; };
    struct ThumbEntry { QImage img; int texW = 0; bool queued = false; int gen = 0; };
    QHash<QString, ThumbEntry> m_thumbs;
    std::deque<ThumbJob> m_thumbQ;
    struct ArtThumb { QImage img; int gen = 0; };
    QHash<QString, ArtThumb> m_artThumbs;
    QHash<QString, QImage> m_ringTex;
    QHash<QString, QImage> m_glow;
    struct LiveEntry { int D = 0; double pitch = 0; SphereTable tab; QImage img; };
    QHash<QString, LiveEntry> m_live;
    int m_liveBudget = 0;
    QHash<QString, QImage> m_stamps;
    QString texKey(const Body& b) const;
    const QImage* thumb(const Body& b, int D);
    void pumpThumbs(double budgetMs);
    void requestTexture(const Body& b, int w, TexEntry& e, const QString& k);
    int m_texRunning = 0;

    // quadro e entrada
    std::vector<Hit> m_hits;
    QString m_peek;
    void setPeek(const QString& key) { if (key != m_peek) { m_peek = key; emit peekChanged(key); } }
    std::vector<LinkView> m_links;
    std::vector<std::pair<QRectF, QString>> m_linkHits;
    bool anchorOf(const QString& key, double& x, double& y, double& h) const;
    void drawLinks(QPainter& p);
    std::vector<RingHit> m_ringHits;
    std::vector<LiveHit> m_liveHits;
    QStringList m_selPath;
    QString m_hover;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastT = 0;
    bool m_dirty = true;
    struct Fly { qint64 t0; double ms, x0, y0, z0, h0, x, y, z, h, bump; };
    std::unique_ptr<Fly> m_fly;
    struct TiltAnim { qint64 t0; double from, to, yf, yt; };
    std::unique_ptr<TiltAnim> m_tiltAnim;
    struct Drag {
        double x = 0, y = 0, wx = 0, wy = 0, tilt = 0, yaw = 0, lx = 0, ly = 0;
        bool moved = false, orbit = false, pan = false, paint = false;
        Body* sphere = nullptr;
        Build* piece = nullptr;
        QString moveKey;
        Build* moveBuild = nullptr;
        Body* movePlanet = nullptr;
        System* moveSys = nullptr;
        bool radial = false;
        double moveRr = 0, moveAng = 0;
        bool hasGhostOrb = false;
        double gx = 0, gy = 0, gr = 0, gc = 0;
        Build was;
        FreeLabel* label = nullptr;
        QHash<int, QString> painted;
    };
    std::unique_ptr<Drag> m_drag;
    QPointF m_pointer;
    bool m_hasPointer = false;
    Snap m_ghost;
    bool m_hasGhost = false;
    void updateGhost(double mx, double my);
    void moveDrag(double mx, double my);
    void moveDrop();
    void realmPaintAt(double mx, double my);
    void realmPaintEnd();
    void applySnap(Build& b, const Snap& G);
    void rulerClick(double mx, double my);
    void movableAt(double mx, double my, Drag& d);
    void clearCaches();
    void afterHistory();
    Body* previewBody(const PlaceItem& it);
    struct Preview { std::shared_ptr<System> sys; Body* body = nullptr; };
    std::map<std::string, Preview> m_previews;
    QHash<int, QString> m_painted;
};

QString fmtLy(double units, double ly);
QString bodyTypeName(Type t);

} // namespace galaxy
