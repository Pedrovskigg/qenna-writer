#pragma once

// Janela do Mapa (o gerador de mundos do Map Lab dentro do Qenna). Abre pelo
// botão Mapa da LeftBar, como a Lousa. A interface mora em cima do mapa:
// título no canto, pílula de cima, o Astrolábio e o anel de ferramentas
// (botão direito no mapa ou a pílula da ferramenta).

#include "CartoCanvas.h"
#include "CartoStore.h"

#include <QHash>
#include <QPointer>
#include <QWidget>

#include <deque>
#include <functional>
#include <memory>

class QButtonGroup;
class QThreadPool;
class QHBoxLayout;
class QVBoxLayout;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;
class QToolButton;

namespace carto {

class Astrolabe;
class Cartela;
class Explorer;
class Ring;
class Rose;
struct ProjectLink;

QString buildKindName(const QString& kind);   // "Castelo", "Vila", "Estrada"...

class Panel : public QWidget {
    Q_OBJECT
public:
    explicit Panel(QWidget* parent = nullptr);
    ~Panel() override;
    void setProjectRoot(const QString& root);
    void setProjectLink(const ProjectLink& link);   // Explorar: o projeto (gavetas, grupos, Criador de Mundos)

signals:
    void closeRequested();

protected:
    void resizeEvent(QResizeEvent*) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;
    void closeEvent(QCloseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    struct Undo {
        enum Kind { Patch, Full, Layers } kind = Patch;
        std::shared_ptr<World> world;                     // Full: o mundo inteiro (gerar, área)
        std::shared_ptr<Doc::Layers> layers;              // Full (gerar): as camadas do mapa antigo
        Snapshot before, after;                           // Patch: o pedaço da pincelada
        int riverId = 0;                                  // Patch: o rio que a pincelada criou
        River river;
        std::shared_ptr<Doc::Layers> lBefore, lAfter;     // Layers: territórios, construções e nomes
    };

    void buildUi();
    void buildBrushCard();
    // docas das camadas (CartoPanelDocks.cpp)
    void buildRealmCard();
    void buildBuildCard();
    void buildNamesCard();
    void syncRealmCard();
    void syncBuildCard();
    void syncNamesCard();
    // Cartela, modo Cartela, exportar (CartoPanelUi.cpp)
    void buildCartelaUi();
    void buildExportCard();
    void applyUiMode();
    void syncCartela();
    void exportPng();
    void cartelaPicked(int i);
    QString regionName();
    void flash(const QString& msg);
    QFrame* newCard(QLabel** title, const QString& text);
    QToolButton* newChoice(QWidget* parent, const QString& icon, const QString& text, const QString& tip);
    QHBoxLayout* newSeg(QWidget* parent, const QStringList& values, const QStringList& labels, const QStringList& tips, QHash<QString, QToolButton*>& out,
                        const std::function<void(const QString&)>& onPick);
    void buildGenPanel();
    void buildSettings();
    void buildWelcome();
    void applyTheme();
    void positionOverlays();
    void placeDock();
    void syncUi();

    // mapas
    void ensureMap();
    void openMap(const QString& id);
    void newMap(bool ask, bool blank = false);
    QString fullLockedWhy() const;
    void pickBlankSize(int size);
    void askBlankSize();
    void showMapMenu();
    void scheduleSave();
    void saveNow();
    QString autoTitle() const;

    // ferramentas e vista
    void pickTool(const QString& id, QPoint ringCenter);
    void setTool(const QString& id);
    void setBrush(Brush b);
    void setMode(const QString& mode);
    void setClimate(bool on);
    void setPlaying(bool on);
    void openRingAt(QPoint at);
    void generateFromPanel();
    QString toolLabel() const;
    void ensureView(const QString& v);
    void rotateSelected(double d);
    void deleteSelected();
    void pushUndo(Undo u);
    void restore(bool undoing);

    Store m_store;
    QString m_mapId;
    Doc m_doc;
    bool m_hasWorld = false;
    MapViewState m_view;
    std::deque<Undo> m_undo, m_redo;

    Canvas* m_canvas = nullptr;
    Explorer* m_explore = nullptr;
    Astrolabe* m_astro = nullptr;
    Ring* m_ring = nullptr;
    QFrame* m_titlePill = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_sub = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QToolButton* m_mapsBtn = nullptr;
    QFrame* m_topPill = nullptr;
    QToolButton* m_undoBtn = nullptr;
    QToolButton* m_redoBtn = nullptr;
    QToolButton* m_genBtn = nullptr;
    QToolButton* m_fitBtn = nullptr;
    QToolButton* m_setBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;
    QFrame* m_hourPill = nullptr;
    QToolButton* m_playBtn = nullptr;
    QLabel* m_hourLabel = nullptr;
    QFrame* m_toolPill = nullptr;
    QToolButton* m_toolBtn = nullptr;
    QLabel* m_hint = nullptr;
    // doca das opções da ferramenta
    QFrame* m_dock = nullptr;
    QFrame* m_brushCard = nullptr;
    QLabel* m_brushTitle = nullptr;
    QHash<int, QToolButton*> m_brushBtns;
    QSlider* m_sizeSlider = nullptr;
    QLabel* m_sizeValue = nullptr;
    QSlider* m_strSlider = nullptr;
    QLabel* m_strValue = nullptr;
    QFrame* m_legend = nullptr;
    // interface: Astrolábio (padrão) ou Cartela
    QString m_ui = QStringLiteral("astro");
    bool m_cartelaOn = true;
    Cartela* m_cartela = nullptr;
    Rose* m_rose = nullptr;
    QFrame* m_cbar = nullptr;
    QHash<QString, QToolButton*> m_cbarBtns;
    QFrame* m_cview = nullptr;
    QSlider* m_cPitch = nullptr;
    QSlider* m_cHour = nullptr;
    QLabel* m_cPitchV = nullptr;
    QLabel* m_cHourV = nullptr;
    QToolButton* m_cPlay = nullptr;
    QWidget* m_viewSeg = nullptr;
    QHash<QString, QToolButton*> m_viewBtns;
    QToolButton* m_rulerBtn = nullptr;
    QToolButton* m_exportBtn = nullptr;
    QHash<QString, QToolButton*> m_uiBtns;
    QToolButton* m_cartelaChk = nullptr;
    QToolButton* m_spritesChk = nullptr;
    // exportar
    QFrame* m_export = nullptr;
    QHash<QString, QToolButton*> m_expScaleBtns;
    QToolButton* m_expCartela = nullptr;
    QLabel* m_expSize = nullptr;
    double m_expScale = 2;
    bool m_expWithCartela = true;
    bool m_dockFromCartela = false;
    QString m_flash;
    QTimer* m_flashTimer = nullptr;
    // territórios
    QFrame* m_realmCard = nullptr;
    QLabel* m_realmTitle = nullptr;
    QVBoxLayout* m_realmList = nullptr;
    QHash<QString, QToolButton*> m_realmToolBtns;
    QSlider* m_reachSlider = nullptr;
    QLabel* m_reachValue = nullptr;
    QWidget* m_realmSizeRow = nullptr;
    QSlider* m_realmSize = nullptr;
    QLabel* m_realmSizeValue = nullptr;
    QLabel* m_realmHint = nullptr;
    int m_realmListSig = -1;
    // construir
    QFrame* m_buildCard = nullptr;
    QLabel* m_buildTitle = nullptr;
    QHash<QString, QToolButton*> m_buildBtns, m_scaleBtns, m_csizeBtns, m_cstyleBtns, m_rmodeBtns, m_rkindBtns;
    QWidget* m_csizeRow = nullptr;
    QWidget* m_cstyleRow = nullptr;
    QWidget* m_roadRow = nullptr;
    QWidget* m_buildSizeRow = nullptr;
    QSlider* m_buildSize = nullptr;
    QLabel* m_buildSizeValue = nullptr;
    QFrame* m_selBox = nullptr;
    QLabel* m_selKind = nullptr;
    QLineEdit* m_selName = nullptr;
    QToolButton* m_showGen = nullptr;
    QLabel* m_buildHint = nullptr;
    // nomes
    QFrame* m_namesCard = nullptr;
    QLabel* m_namesTitle = nullptr;
    QToolButton* m_namesAuto = nullptr;
    QHash<QString, QToolButton*> m_densBtns, m_nstyleBtns;
    QFrame* m_nameBox = nullptr;
    QLabel* m_nameKind = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QSlider* m_nameSize = nullptr;
    QLabel* m_nameSizeValue = nullptr;
    QToolButton* m_nameReset = nullptr;
    QToolButton* m_nameDel = nullptr;
    QLabel* m_namesHint = nullptr;
    std::shared_ptr<Doc::Layers> m_nameSizeBefore;
    QPoint m_dockAt;
    bool m_dockFromRing = false, m_dockUser = false;
    QPoint m_dockGrab;
    bool m_dockDragging = false;
    // gerar
    QFrame* m_gen = nullptr;
    QButtonGroup* m_genType = nullptr;
    QSlider* m_genWater = nullptr;
    QLabel* m_genWaterValue = nullptr;
    QComboBox* m_genSize = nullptr;
    QButtonGroup* m_genClimate = nullptr;
    QSpinBox* m_genSeed = nullptr;
    QToolButton* m_genInfo = nullptr;   // gerar territórios e locais
    // ajustes
    QFrame* m_settings = nullptr;
    // boas-vindas (só na primeira vez que o mapa abre) e o tamanho do mapa em branco
    QFrame* m_welcome = nullptr;
    QFrame* m_sizeCard = nullptr;
    bool m_askSize = false;   // o mapa em branco ainda espera o tamanho
    QButtonGroup* m_styleGroup = nullptr;

    QString m_tool = QStringLiteral("pan");
    Brush m_brush = Brush::Land;
    QHash<int, double> m_brushSizes;   // tamanho por pincel (o rio é fino)
    double m_strength = 0.5;
    QTimer* m_saveTimer = nullptr;
    QThreadPool* m_savePool = nullptr;    // gravações em segundo plano, uma de cada vez e em ordem
    void flushSaves();
    QTimer* m_playTimer = nullptr;
    qint64 m_playLast = 0;
    QList<QPair<QToolButton*, QString>> m_icons;
};

} // namespace carto
