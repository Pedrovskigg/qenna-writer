#pragma once

// Criação Livre: a terceira ferramenta do Galaxy Maps, escolhida ao criar a galáxia. Nada existe até o autor pedir.
// Prateleira de mundos à esquerda, a folha no meio, a ficha e o sistema em pé à direita, e a gaveta flutuante com os
// documentos e as conexões de cada lugar. Mora na mesma galáxia do mapa: o que nasce aqui existe lá.
// Os dados (o que é cada chave, criar, mover, gerar) estão em GalaxyLivreData.cpp; a direita e a gaveta em
// GalaxyLivreSide.cpp; o planeta girando em GalaxyLivreLive.cpp; o pincel de território em GalaxyLivreDraw.cpp.

#include "GalaxyCore.h"

#include <QElapsedTimer>
#include <QFrame>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <functional>
#include <vector>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QTextEdit;
class QToolButton;
class QVBoxLayout;
class FontPickerPopup;

namespace galaxy {

class Canvas;
class Doc;
class Panel;
class Livre;
class LivreMap;
struct DocLink;
struct Region;

// O planeta (ou a peça) girando na janela escura da ficha, com quem orbita em volta.
class LiveWorld : public QWidget {
public:
    LiveWorld(Livre* owner, const QString& key, QWidget* parent);
    void setZoom(double z);
    double zoom() const { return m_zoom; }
    std::function<void(double)> zoomChanged;

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    struct Hit { QString key; bool ghost; QPointF at; double r; };
    void paintWorld(QPainter& g);
    void paintBuild(QPainter& g);
    QString hitAt(QPointF p) const;
    Livre* m_owner;
    QString m_key;
    bool m_world = true;
    double m_spin = 0.6, m_rot = 0.5, m_pitch = 0.28, m_zoom = 1, m_orbT = 0, m_ms = 0;
    bool m_drag = false, m_flip = false;
    QPointF m_last, m_press;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastMs = 0;
    int m_tabD = 0;
    double m_tabPitch = -9;
    uint32_t m_tabRev = 0;
    SphereTable m_tab;
    QImage m_img;
    std::vector<Hit> m_hits;
};

class Livre : public QWidget {
    Q_OBJECT
public:
    Livre(Panel* panel, Canvas* canvas, Doc* doc);
    void open(const QString& key);
    void closeIt();          // grava o que está na folha
    void refresh() { render(); }
    bool escape();
    void applyTheme();
    void toast(const QString& text, bool withUndo);

    // ── o que cada chave é (GalaxyLivreData.cpp) ──
    // GAL, R (território), S (sistema), P (planeta), M (lua), G (lugar), B (construção), D<id> (documento), X<id> (conexão)
    enum class LK { None, Gal, Terr, Sys, Planet, Moon, Place, Build, Doc, Link };
    LK kindOf(const QString& k) const;
    bool valid(const QString& k) const;
    bool isWorld(const QString& k) const { const LK t = kindOf(k); return t == LK::Planet || t == LK::Moon; }
    QString label(const QString& k) const;
    QString kindName(LK t) const;
    QString parentKey(const QString& k) const;
    QStringList children(const QString& k) const;      // o que mora direto dentro
    QStringList inside(const QString& k) const;        // a árvore da gaveta: tudo menos sistemas e planetas
    QStringList pathOf(const QString& k) const;
    bool isInside(const QString& k, const QString& anc) const;
    QString ancestor(const QString& k, LK t) const;
    QStringList worldsIn(const QString& k) const;      // os mundos de um território, sistema ou da galáxia
    QStringList allWorlds() const;
    bool isGhost(const QString& k) const;
    bool isCapital(const QString& k) const;
    Body* bodyOf(const QString& k) const;
    Body* hostOf(const QString& k) const;               // o planeta de uma lua
    System* sysOf(const QString& k) const;
    DocLink* docOf(const QString& k) const;
    QString docKey(const DocLink* d) const;
    std::vector<System*> livreSystems() const;          // os sistemas que o autor criou (sem os soltos)
    QString solarKey(const QString& world) const;       // o sistema de verdade do mundo, ou vazio se ele está solto
    struct Orbiter { QString key; double rr, speed, phase; bool moon; };
    struct Wrap { QString key; bool ring; int i; };
    struct OrbitPlan { std::vector<Orbiter> list; std::vector<Wrap> wraps; double outer = 0; };
    OrbitPlan orbitPlan(const QString& world) const;
    std::string buildType(const QString& k) const;
    uint32_t buildSeed(const QString& k) const;
    QString typeLabelOf(const QString& k) const;
    int habOf(const QString& k, bool* isAuto = nullptr) const;

    // ── imagens ──
    QPixmap ball(const QString& k, int px, bool rings = false);
    QPixmap modelPix(const std::string& type, uint32_t seed, int px);
    QPixmap typeThumb(Type t, int px);
    static QColor starShown(StarKind k);   // a cor da estrela que aparece em tema claro e escuro
    Canvas* canvas() const { return m_canvas; }
    bool typingNow() const { return m_typing.isValid() && m_typing.elapsed() < 1200; }
    void select(const QString& k);

protected:
    void resizeEvent(QResizeEvent*) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    friend class LiveWorld;
    friend class LivreMap;
    // ── dados (GalaxyLivreData.cpp) ──
    QString createWorld(const QString& name, const QString& parent, Type t, bool ghost);
    QString createSystem(const QString& name, const QString& parent, StarKind kind, bool ghost);
    QString createTerritory(const QString& name);
    QString createMoon(const QString& name, const QString& planet, Type t, bool ghost);
    QString createPlace(const QString& name, const QString& parent, const QString& form, bool ghost);
    QString createBuild(const std::string& type, const QString& parent, bool ghost);
    QString createDoc(const QString& name, const QString& parent, bool gaveta, const QString& drawer);
    QString createLink(const QString& a, const QString& b);
    void moveTo(const QString& k, const QString& parent);
    bool canDrop(const QString& k, const QString& parent) const;
    void removeItem(const QString& k);
    void accept(const QString& k);
    void dismiss(const QString& k);
    bool canBeCapital(const QString& k) const;
    void toggleCapital(const QString& k);
    QString capitalOf(const QString& terr) const;
    void setWorldType(const QString& k, Type t);
    void setStar(const QString& sysKey, StarKind kind);
    void reseed(const QString& k);
    void setBuildType(const QString& k, const std::string& type);
    void rename(const QString& k, const QString& name);
    QString structName(const std::string& type, const QString& parent) const;
    std::vector<std::pair<QString, std::function<void()>>> orders(const QString& k);
    QHash<QString, QString> motorFicha(const QString& k) const;
    void fillFicha(const QString& k);
    QString ghostName(const QString& kind);
    void changed(bool map = true);    // grava; map = o mapa precisa redesenhar
    void undoable(const QString& label, std::function<void()> undo, std::function<void()> redo);
    QString sheetHtml(const QString& k) const;
    void writeSheet(const QString& k, const QString& html);

    // ── a casca (GalaxyLivre.cpp) ──
    void render();
    void renderLeft();
    void renderCenter();
    void renderRight();          // GalaxyLivreSide.cpp
    void renderNav();            // GalaxyLivreSide.cpp
    QWidget* ballEl(const QString& k, QWidget* parent);
    QWidget* addBallEl(const QString& slot, const QString& parent, QWidget* where);
    QWidget* groupEl(const QString& title, const QString& id, QWidget* parent);
    QWidget* edBar(const QString& k, QWidget* parent);
    QWidget* whereTag(const DocLink* d, QWidget* parent);
    QWidget* starMark(QWidget* parent);
    QWidget* ghostActs(const QString& k, QWidget* parent);
    QWidget* markOf(const QString& k, int px, QWidget* parent);
    void flushSheet();
    // a folha (GalaxyLivreSheet.cpp)
    QString whatIs(const QString& k) const;
    QWidget* whereLine(const QString& k, QWidget* parent);
    int mentionsOf(const QString& k) const;
    void rewriteMentions(const QString& from, const QString& to);
    void syncCount();
    void applyFocus();
    void applyAlignAndSpacing();
    void openMention();
    void closeMention();
    void insertMention(const QString& key);
    void insertImage(const QString& k, QImage im, const QString& baseName);
    // a direita: tela grande e anéis (GalaxyLivreSide.cpp)
    void openBigView(const QString& k);
    void closeBigView();
    void pickRings(QWidget* anchor, const QString& k);
    void placeGrips();
    void applyEdFont();
    void goBody();
    void focusName(const QString& k);
    // prévia ao passar o mouse
    void peekable(QWidget* w, const QString& k);
    void showPeek(QWidget* at, const QString& k);
    void showSysPeek(QWidget* at, const QString& k);
    void hidePeek();
    // balão de opções
    QFrame* openPop(QWidget* anchor, const std::function<void(QVBoxLayout*, QFrame*)>& build, int width = 0);
    void closePop();
    QPushButton* popOpt(QVBoxLayout* l, QWidget* parent, const QString& text, bool on, const QString& small, const std::function<void()>& fn, const QPixmap& mark = QPixmap());
    void popTitle(QVBoxLayout* l, QWidget* parent, const QString& text);
    void pickList(QWidget* anchor, const QString& title, const QStringList& opts, const QString& cur, const QString& none, const std::function<void(const QString&)>& fn);
    void pickType(QWidget* anchor, const QString& k);
    void pickClass(QWidget* anchor, const QString& k);
    void pickStar(QWidget* anchor, const QString& sysKey);
    void pickWhere(QWidget* anchor, const QString& k);
    void pickStruct(QWidget* anchor, const QString& parent, const QString& replace);   // replace vazio = nova construção
    void pickConnect(QWidget* anchor, const QString& root);
    void pickColor(QWidget* anchor, const QString& k);
    QWidget* whereForm(bool* gaveta, QString* drawer, const QString& owner, const QString& goText, const std::function<void()>& done, const std::function<void()>& cancel, QWidget* parent);
    void moveDocWhere(QWidget* anchor, DocLink* d);

    // ── direita e gaveta (GalaxyLivreSide.cpp) ──
    QWidget* crumbsEl(QWidget* parent);
    QWidget* systemView(const QString& sysKey, QWidget* parent);
    QWidget* aloneView(const QString& world, QWidget* parent);
    QWidget* listView(const QStringList& keys, const QString& empty, QWidget* parent);
    QWidget* orbitRow(const QString& w, bool moon, QWidget* parent);
    QWidget* listRow(const QString& k, QWidget* parent);
    QWidget* fichaEl(const QString& k, QWidget* parent);
    QWidget* fichaFull(const QString& w, QWidget* parent);
    QWidget* win3d(const QString& k, bool big, bool titled, QWidget* parent);
    QWidget* propRows(const QString& k, bool full, QWidget* parent);
    QWidget* looksEl(const QString& k, QWidget* parent);
    QWidget* ordersEl(const QString& k, QWidget* parent);
    QWidget* linkSide(const QString& k, QWidget* parent);
    QWidget* terrMapEl(const QString& k, QWidget* parent);
    QWidget* navPanel(const QString& root, QWidget* parent);
    void docTree(QVBoxLayout* box, QWidget* parent, const QString& id, int depth, const QSet<QString>* vis);
    QWidget* askCard(QWidget* parent);
    QSet<QString> navVisible(const QString& root) const;
    QString navRoot();
    void placeNav();
    void askOrbiter(QWidget* win, const QString& world, const QString& kind);

    // ── montador do sistema (GalaxyLivreCompose.cpp) ──
    // Gerar um sistema escolhendo, se quiser, a estrela, quantos planetas e o tipo de cada um. Montar já é o sim.
    struct Slot { Type type = Type::Arido; QString name; bool fixed = false; };
    struct Composer { bool on = false, shown = false; QString forKey, terr, planet, name; StarKind star = StarKind::G; std::vector<Slot> list; };
    void openComposer(const QString& k);
    QWidget* composerEl(QWidget* parent);
    void buildComposed();
    void setComposeCount(int n);
    std::vector<Slot> suggestSlots(int n);
    void pickSlotType(QWidget* anchor, int idx);
    Composer m_cmp;
public:
    static double orbitFor(const System& s, Type t, int slot);
    static double orbitRank(Type t);
private:

    // ── território desenhado (GalaxyLivreDraw.cpp) ──
    void openDraw(const QString& terr);

    // ── o minimapa (GalaxyLivreMap.cpp) ──
    void buildMapSection(QVBoxLayout* into);
    void syncMap();
    void openBigMap();
    void closeBigMap();
    std::vector<std::pair<QString, QString>> mapLinks() const;   // as conexões entre lugares da galáxia

    Panel* m_panel;
    Canvas* m_canvas;
    Doc* m_doc;
    QString m_sel, m_edFor, m_lastNavRoot, m_justMade;
    QSet<QString> m_closed;              // grupos dobrados na prateleira ("loose" = Soltos)
    bool m_fichaOpen = false, m_navOpen = false, m_navPinned = false, m_navSearch = false;
    QString m_navQuery, m_navAdd, m_rowAdd, m_shelfAdd;
    bool m_sysAdd = false;
    struct Ask { QString name, parent; } m_ask;
    bool m_lastGaveta = false;
    QString m_lastDrawer;
    QString m_fontFamily = QStringLiteral("Source Serif 4"), m_align = QStringLiteral("left");
    int m_size = 17, m_lineH = 175;
    bool m_focusMode = false, m_sheetLoading = false;
    FontPickerPopup* m_fontPop = nullptr;
    QPointer<QLabel> m_count;
    QPointer<QWidget> m_mentionAnchor;
    QFrame* m_mentionPop = nullptr;
    struct Mention { int start = -1, end = -1, idx = 0; QStringList list; } m_mention;
    int m_leftW = 292, m_rightW = 384;   // larguras arrastáveis das colunas
    QFrame* m_bigView = nullptr;         // o planeta em tela grande
    int m_ringPage = 0;
    QString m_dragKey;
    uint32_t m_nameN = 0;

    QWidget* m_cols = nullptr;
    QWidget* m_leftCol = nullptr;      // a coluna da esquerda: a prateleira (rola) e o minimapa no pé
    QScrollArea* m_left = nullptr;
    QWidget* m_mapSec = nullptr;
    QPushButton* m_mapFold = nullptr;
    QLabel* m_mapSub = nullptr;
    LivreMap* m_map = nullptr;
    QFrame* m_bigMap = nullptr;        // o mapa em tela grande
    LivreMap* m_bigMapView = nullptr;
    QPushButton* m_bigMapGo = nullptr;
    QWidget* m_center = nullptr;
    QScrollArea* m_right = nullptr;
    QWidget* m_rightCol = nullptr;     // a coluna da direita: o topo grudado (fechar a ficha) e a rolagem
    QWidget* m_rightHead = nullptr;
    QFrame* m_nav = nullptr;
    QPushButton* m_brandName = nullptr;   // o nome da galáxia: abre a lista de mapas
    QPointer<QTextEdit> m_body;
    QPointer<QLineEdit> m_name;
    QTimer m_sheetTimer;
    QString m_sheetFor;
    QElapsedTimer m_typing;
    QPointer<LiveWorld> m_live;
    // o giro da janela 3D sobrevive quando a direita se refaz
    double m_liveZoom = 1, m_liveSpin = 0.6, m_livePitch = 0.28, m_liveRot = 0.5, m_liveOrbT = 0;
    QString m_liveKey;
    QFrame* m_pop = nullptr;
    QFrame* m_peek = nullptr;
    QTimer m_peekTimer;
    QPointer<QWidget> m_peekAt;
    QString m_peekKey;
    QFrame* m_toast = nullptr;
    QLabel* m_toastText = nullptr;
    QPushButton* m_toastUndo = nullptr;
    QTimer m_toastTimer;
    QHash<QString, QPixmap> m_balls;
    QWidget* m_draw = nullptr;
};

} // namespace galaxy
