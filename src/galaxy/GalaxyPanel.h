#pragma once

// A janela do Galaxy Maps. Mora dentro do painel do Mapa (a galáxia é mais um mapa do projeto, na mesma lista),
// cobrindo ele inteiro. A interface é a do Mapa: título no canto, barra da direita, Astrolábio e anel de
// ferramentas; o painel de Explorar e Editar é o do Lab. As peças estão em GalaxyPanel*.cpp.

#include "GalaxyCanvas.h"
#include "../carto/CartoExplore.h"

#include <QFrame>
#include <QHash>
#include <QPixmap>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <functional>
#include <memory>

class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QScrollArea;
class QSlider;
class QTextEdit;
class QToolButton;
class QVBoxLayout;

namespace carto { class Astrolabe; class ArtPicker; class Ring; class Rose; }

namespace galaxy {

class Caderno;

class Panel : public QWidget {
    Q_OBJECT
public:
    explicit Panel(QWidget* parent = nullptr);
    ~Panel() override;

    void setProjectLink(const carto::ProjectLink& link);
    bool openFile(const QString& path, const QString& name);       // abre a galáxia do arquivo
    void startNew(const GalOpts& opts, const QString& path, const QString& name);   // galáxia nova nesse arquivo
    void regenerate(const GalOpts& opts, const QString& name);                      // troca a galáxia aberta, com desfazer
    QString displayName() { return m_doc.gal ? m_doc.labelOf(QStringLiteral("GAL")) : QString(); }
    void closeMap();                                               // salva e larga
    void saveNow();
    void applyTheme();
    QString mapPath() const { return m_path; }
    bool isOpen() const { return m_doc.gal != nullptr; }
    const GalOpts& options() const { return m_doc.opts; }
    QString mapName() const { return m_name; }
    void setMapName(const QString& n);
    int systemCount() const;
    void beginRename() { renameGalaxy(); }
    void forgetFile() { m_saveTimer->stop(); m_path.clear(); }   // o arquivo vai ser apagado: não grava mais
    bool handleKey(QKeyEvent* e);

    // o painel do Mapa preenche: a lista de mapas do projeto e o que fazer com ela
    std::function<void(QMenu&)> fillMapMenu;
    std::function<void(const QString&)> renamed;

signals:
    void regenerateRequested();

protected:
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    friend class Caderno;
    // montagem (GalaxyPanel.cpp)
    void buildUi();
    QToolButton* makeBtn(QWidget* parent, const QString& icon, const QString& tip, const QString& text = QString());
    QFrame* makeFloat(const QString& name);
    void buildSettings();
    void buildExport();
    void buildCview();
    void positionOverlays();
    void placeDock();
    void syncUi();
    void syncTitle();
    void syncCview();
    void syncHint();
    void syncTip(const QString& key, const QPointF& at);
    QString toolLabel() const;
    QString toolHint() const;
    void pickTool(const QString& id, const QString& target = QString());
    void setTool(const QString& id);
    void setViewMode(const QString& m);
    void ensureView(const QString& id);
    void openRing(QPointF at, const QString& target);
    void setPlay(bool on);
    void setTrafficOn(bool on);
    void setSpritesOn(bool on);
    void setSkyMode(const QString& v);
    void setUiMode(const QString& v);
    void showMapMenu();
    void renameGalaxy();
    void doExport();
    void scheduleSave();
    bool escapeOne();
    void deleteSelected();
    bool rotatePiece(double d);
    void scalePiece(double k);
    void flash(const QString& text);
    void showToast(const QString& text, bool withUndo);
    void syncFocusBar();
    void syncScale();
    void wireCanvas();
    void afterOpen();

    // construir (GalaxyPanelBuild.cpp)
    void buildBuildBar();
    void renderBuildBar();
    void syncBuildChecks();
    void placeBuildBar();
    void syncSelPill(bool force);
    void placeSelPill();
    QPixmap itemArt(const CatalogItem& it, int px);
    QPixmap modelArt(const std::string& type, uint32_t seed, int px);
    void applyItem(const CatalogItem* it);

    // territórios e nomes (GalaxyPanelRealm.cpp)
    void buildRealmDock();
    void renderRealmDock();
    void openNameEdit(double sx, double sy, const QString& value, const std::function<void(const QString&)>& fn);
    void nameClick(double mx, double my, const QString& key);
    void deleteName(const QString& k);

    // Explorar e Editar (GalaxyPanelSide.cpp)
    void buildSide();
    void renderPanel(bool keepScroll = false);
    void renderMini();
    void renderTabGeneral(QVBoxLayout* body, const QString& k, const Ref& o);
    void renderTabDocs(QVBoxLayout* body, const QString& k);
    void renderTabInside(QVBoxLayout* body, const QString& k, const Ref& o);
    void renderTabData(QVBoxLayout* body, const QString& k, const Ref& o);
    QWidget* photoOf(const QString& k, const Ref& o);
    QWidget* iconEl(const QString& k, int px = 30);
    QWidget* factsGrid(const std::vector<std::pair<QString, QString>>& rows);
    QWidget* statsGrid(const std::vector<std::pair<QString, QString>>& rows);
    QWidget* physSection(const Ref& o, bool flat);
    QPushButton* itemRow(const QString& k, const QString& sub, const QString& val);
    QWidget* docGrid(const std::vector<DocLink*>& ds, const QString& withNew);
    QPushButton* docCard(DocLink* d);
    struct Inside { QString label; QStringList keys; };
    Inside insideOf(const QString& k, const Ref& o);
    std::vector<std::pair<QString, QString>> quickFacts(const QString& k, const Ref& o);
    std::vector<std::pair<QString, std::vector<DocLink*>>> docsBelow(const QString& k);
    std::vector<std::pair<QString, std::vector<DocLink*>>> docsAbove(const QString& k);
    QStringList siblingsOf(const QString& k);
    QString subShort(const QString& k, const Ref& o);
    QWidget* crumbsOf(const QString& k);
    bool panelShown() const { return m_panelOpen && m_duoLink.isEmpty() && (m_tool == QLatin1String("explore") || m_tool == QLatin1String("edit")); }
    void openPanel();
    void closePanel();
    void selected(const QString& key);
    void search(const QString& q);
    void chooseResult(const QString& k);
    void startRename(QWidget* holder, const QString& k);
    void addGroup(QVBoxLayout* into, const QString& k, const QString& kind);

    // classes de planeta (GalaxyPanelHab.cpp)
    void buildHab();
    void setHabOn(bool on);
    void syncHabCard();
    void renderHabCard();
    QWidget* habSection(const QString& k, const Ref& o);
    QWidget* habEditSection(Body* b, System* s, Body* host);
    QToolButton* m_habBtn = nullptr;
    QFrame* m_habCard = nullptr;

    // prévia rápida (GalaxyPanelPeek.cpp)
    void buildPeek();
    void peekChanged(const QString& key);
    void showPeek(const QString& k);
    void hidePeek();
    void placePeek();
    void setPeekOn(bool on);
    int peopleCount(const QString& k);
    QFrame* m_peek = nullptr;
    QWidget* m_peekBody = nullptr;
    QTimer* m_peekTimer = nullptr;
    QString m_peekWant, m_peekKey;
    bool m_peekOn = true;
    QHash<QString, QPixmap> m_peekBalls;
    uint32_t m_peekBallsRev = 0;
    QToolButton* m_lyPeek = nullptr;

    // conexões e personagens (GalaxyPanelLinks.cpp)
    struct GLink { QString id, a, b, label; QColor color; };
    QString placeOfKey(const QString& k, bool create);
    QString keyOfPlace(const QString& placeId);
    std::vector<GLink> linksOf(const QString& k, int* offMap = nullptr);
    QString linkLabel(const QString& linkId) const;
    void syncLinkView();
    void refreshPlaceUi();
    void renderLinksSection(QVBoxLayout* body, const QString& k);
    void renderPeopleSection(QVBoxLayout* body, const QString& k);
    void linkPicked(const QString& key);
    void connectPlaces(const QString& a, const QString& b);
    void buildDuo();
    void openDuo(const QString& linkId, const QString& from);
    void renderDuo();
    void applyDuoTheme();
    void writeDuo();
    void flushDuo();
    void closeDuo();
    void placeDuo();
    std::vector<std::pair<DocLink*, QString>> duoDocs(const QString& k);
    struct DuoCol { QLabel* chip = nullptr; QLabel* title = nullptr; QComboBox* pick = nullptr; QToolButton* pen = nullptr; QTextEdit* body = nullptr; DocLink* doc = nullptr; bool about = false; };
    QString m_linkFrom, m_peopleFor;
    bool m_peopleBorn = false;
    QFrame* m_duo = nullptr;
    QLabel* m_duoTitle = nullptr;
    DuoCol m_dcol[3];
    QString m_duoLink, m_duoA, m_duoB;
    int m_duoEdit = -1;
    QTimer* m_duoTimer = nullptr;
    bool m_duoLoading = false;

    // documentos (GalaxyPanelDocs.cpp)
    QWidget* editorFor(DocLink* d, const std::function<void()>& back, QWidget* extraBar = nullptr);
    DocLink* newDocFor(const QString& k);
    QString docHtml(const DocLink* d) const;
    QString docPlain(const DocLink* d) const;
    QString docDestLabel(const DocLink* d);
    void docMaterialize(DocLink* d);
    void docWriteHtml(DocLink* d, const QString& html);
    void docSetTitle(DocLink* d, const QString& title);
    void docMoveTo(DocLink* d, const QString& dest);
    void docDropIfEmpty(DocLink* d);
    QString placeIdFor(const QString& owner, QString* folderId);
    void adoptProjectDocs();

    // editor de dados (GalaxyPanelEdit.cpp)
    void renderEdit(QVBoxLayout* body, const QString& k, const Ref& o);
    void edited(Body* b = nullptr);
    bool editable(const Ref& o) const;

    // o caderno
    void openCaderno(const QString& key);
    void closeCaderno();

    Doc m_doc;
    QString m_path, m_name;
    carto::ProjectLink m_link;
    Canvas* m_canvas = nullptr;
    QTimer* m_saveTimer = nullptr;
    QTimer* m_uiTimer = nullptr;
    QString m_tool = QStringLiteral("pan");
    QString m_ui = QStringLiteral("astro");
    QString m_sky = QStringLiteral("cheio");

    // chrome
    QFrame* m_titlePill = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_sub = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QToolButton* m_menuBtn = nullptr;
    QFrame* m_cbar = nullptr;
    QFrame* m_cbarSep = nullptr;
    QToolButton *m_panBtn = nullptr, *m_fitBtn = nullptr, *m_rulerBtn = nullptr, *m_cadernoBtn = nullptr, *m_exportBtn = nullptr, *m_setBtn = nullptr, *m_helpBtn = nullptr;
    QFrame* m_settings = nullptr;
    QHash<QString, QToolButton*> m_skyBtns, m_uiBtns;
    QToolButton *m_lyTerr = nullptr, *m_lyNames = nullptr, *m_lyOrb = nullptr, *m_lySprites = nullptr, *m_lyFrame = nullptr, *m_lyTraffic = nullptr;
    QComboBox* m_trafficLevel = nullptr;
    QFrame* m_export = nullptr;
    QHash<QString, QToolButton*> m_expBtns;
    QLabel* m_expSize = nullptr;
    QToolButton* m_expTitle = nullptr;
    double m_expScale = 1;
    bool m_expWithTitle = true;
    QFrame* m_cview = nullptr;
    QHash<QString, QToolButton*> m_viewBtns;
    QSlider *m_cPitch = nullptr, *m_cTime = nullptr;
    QLabel *m_cPitchV = nullptr, *m_cTimeV = nullptr;
    QToolButton* m_cPlay = nullptr;
    carto::Astrolabe* m_astro = nullptr;
    carto::ArtPicker* m_artPick = nullptr;
    carto::Ring* m_ring = nullptr;
    carto::Rose* m_rose = nullptr;
    QFrame* m_hourPill = nullptr;
    QToolButton* m_playBtn = nullptr;
    QLabel* m_timeLabel = nullptr;
    QFrame* m_toolPill = nullptr;
    QToolButton* m_toolBtn = nullptr;
    QLabel* m_hint = nullptr;
    QFrame* m_toast = nullptr;
    QLabel* m_toastText = nullptr;
    QPushButton* m_toastUndo = nullptr;
    QTimer* m_toastTimer = nullptr;
    QLabel* m_tip = nullptr;
    QFrame* m_focusBar = nullptr;
    QLabel* m_focusName = nullptr;
    QLabel* m_focusSwatch = nullptr;
    QWidget* m_scale = nullptr;
    QLabel* m_scaleText = nullptr;
    QWidget* m_scaleBar = nullptr;
    QString m_ringTarget;
    QPoint m_ringAt;
    bool m_hasRingAt = false;
    QList<QPair<QToolButton*, QString>> m_icons;

    // construir
    QFrame* m_buildBar = nullptr;
    QWidget* m_buildCats = nullptr;
    QHash<std::string, QPushButton*> m_catBtns;
    QLabel* m_buildTitle = nullptr;
    QWidget* m_sizeRow = nullptr;
    QHash<QString, QToolButton*> m_sizeBtns;
    QToolButton *m_selectBtn = nullptr, *m_gearBtn = nullptr;
    QScrollArea* m_cardScroll = nullptr;
    QWidget* m_cardBody = nullptr;
    QHBoxLayout* m_cardLay = nullptr;
    QList<QToolButton*> m_cards;
    std::string m_buildCat = "planetas";
    QFrame* m_gear = nullptr;
    QHash<QString, QToolButton*> m_scaleBtns;
    QHash<QString, QPixmap> m_artCache;
    QFrame* m_selPill = nullptr;
    QLineEdit* m_selName = nullptr;
    QWidget* m_selRot = nullptr;
    QSlider* m_selSize = nullptr;
    QLabel* m_selSizeV = nullptr;
    QWidget* m_selSizeRow = nullptr;
    QString m_pillKey;
    std::shared_ptr<Build> m_sizeBefore;
    bool m_buildNarrow = false;

    // territórios
    QFrame* m_dock = nullptr;
    QLabel* m_dockTitle = nullptr;
    QWidget* m_dockBody = nullptr;
    bool m_dockDrag = false, m_dockUser = false;
    QPoint m_dockGrab, m_dockAt;
    QLineEdit* m_nameEdit = nullptr;
    std::function<void(const QString&)> m_nameFn;

    // lado
    QWidget* m_side = nullptr;
    QFrame* m_searchBox = nullptr;
    QLineEdit* m_query = nullptr;
    QFrame* m_results = nullptr;
    QFrame* m_panelBox = nullptr;
    QScrollArea* m_pscroll = nullptr;
    QFrame* m_mini = nullptr;
    QPointer<QWidget> m_liveView;
    bool m_panelOpen = false, m_panelMini = false;
    QString m_tab = QStringLiteral("geral");
    DocLink* m_openDoc = nullptr;
    // escrever no editor do mapa: a tecla só mexe na memória; o disco espera a pessoa parar
    std::function<void()> m_editorFlush;
    bool m_docPlacesDirty = false, m_docProjectDirty = false;
    void flushDocSave();
    QString m_lastDest = QStringLiteral("criador");
    QString m_panelKey;
    QStringList m_resList;
    int m_resIdx = 0;

    Caderno* m_caderno = nullptr;
};

// fábrica de texto e número, igual ao Lab
QString nf(double n, int d = 0);
QString sig(double n);
QString celsius(double k);
QString periodText(double days);
QString fmtCount(double n);

} // namespace galaxy
