#pragma once
// Esboço da Lousa (2026-10-10, pedido do Pe; concept "Pincel na Lousa"):
// uma folha pra desenhar com pincel de verdade (libmypaint), com camadas.
// Abre por cima da Lousa, com o quadro apagado em volta; Pronto salva e a
// folha volta a ser um card no quadro.
//
// No disco, dentro do projeto: lousa-esbocos/<id do card>/sketch.json (as
// camadas: nome, visível, opacidade, ordem) e um PNG por camada. O card guarda
// só a imagem pronta (papel + camadas visíveis) pra aparecer no quadro.

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QPointer>
#include <QWidget>
#include <memory>
#include <vector>

#include "LousaInk.h"
#include "SketchBrushes.h"
#include "SketchEngine.h"

class QLabel;
class QScrollArea;
class QSlider;
class QToolButton;
class QVBoxLayout;
class SketchEditor;

struct SketchLayer {
    QString id;
    QString name;
    bool    visible = true;
    qreal   opacity = 1.0;
    std::unique_ptr<SketchSurface> surf;
    QImage  cache;              // a camada pronta pra pintar na tela
};

// A folha em si: papel + camadas, e a caneta/mouse desenhando na camada ativa.
class SketchCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit SketchCanvas(SketchEditor* editor);
    QRectF sheetRect() const;           // onde a folha aparece no widget
    QRectF fitRect() const;             // onde ela fica sem zoom (os painéis se ancoram aqui)
    qreal  scale() const;               // px de tela por px da folha

    // Zoom (1 = folha inteira na tela) e arrasto da folha.
    qreal zoom() const { return m_zoom; }
    void  zoomAt(qreal zoom, const QPointF& anchorWidget);   // mantém o ponto embaixo do cursor
    void  zoomBy(qreal factor);                              // em volta do centro
    void  fitToScreen();
    void  setPanKey(bool held);         // Espaço segurado: arrastar move a folha
    static constexpr qreal kMinZoom = 0.25;
    static constexpr qreal kMaxZoom = 8.0;

signals:
    void strokeFinished();
    void zoomChanged(qreal zoom);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void tabletEvent(QTabletEvent*) override;
    void leaveEvent(QEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    void begin(const QPointF& w, qreal pressure, bool eraser, qreal xt, qreal yt);
    void move(const QPointF& w, qreal pressure, qreal xt, qreal yt);
    void end();
    // dtOverride > 0: tempo do segmento (o fim do traço não tem leitura da caneta).
    void paintAt(const QPointF& sheetPt, qreal pressure, qreal xt, qreal yt, qreal dtOverride = 0);
    QPointF m_lastPainted;
    QPointF toSheet(const QPointF& w) const;
    SketchEditor* m_ed;
    bool    m_drawing = false;
    bool    m_tabletDown = false;
    bool    m_eraserStroke = false;
    QElapsedTimer m_clock;
    LousaInk::Stabilizer m_stab;
    int     m_stabStrength = 0;
    QPointF m_raw;                      // onde a caneta está (folha)
    qreal   m_lastPressure = 0.5;
    QPointF m_hover;                    // contorno do pincel (widget)
    bool    m_hoverOn = false;
    qreal   fitScale() const;
    void    clampPan();
    void    panBy(const QPointF& d);
    qreal   m_zoom = 1.0;
    QPointF m_pan;                      // deslocamento da folha a partir do centro (px de tela)
    bool    m_panKey = false;
    bool    m_panning = false;
    QPointF m_panLast;
};

class SketchEditor : public QWidget
{
    Q_OBJECT
public:
    static constexpr int kSheetW = 1000;
    static constexpr int kSheetH = 1300;
    static QColor paperColor() { return QColor(0xfd, 0xfb, 0xf6); }

    explicit SketchEditor(QWidget* parent = nullptr);
    ~SketchEditor() override;

    // Abre a folha do card (cria duas camadas se ainda não existe).
    void open(const QString& projectRoot, const QString& cardId, const QString& title);
    // Papel + camadas visíveis, no tamanho da folha.
    QImage composite(bool withPaper = true) const;
    static QString sketchDir(const QString& projectRoot, const QString& cardId);
    // A folha salva no disco, montada: com o papel ou com fundo transparente.
    static QImage renderFromDisk(const QString& projectRoot, const QString& cardId, bool withPaper);
    // Exportar PNG: pergunta com papel ou transparente e onde salvar.
    static void exportDialog(QWidget* parent, const QImage& withPaper, const QImage& transparent,
                             const QString& suggestedName, const QPoint& globalPos);

    // Usados pela folha (SketchCanvas)
    SketchLayer* activeLayer();
    SketchBrush& brush() { return *m_brush; }
    SketchBrush& eraserBrush() { return *m_eraser; }
    void  layerChanged(SketchLayer* L, const QRect& dirty);   // repinta o cache
    void  pushStrokeUndo(SketchLayer* L, SketchSurface::TileSnapshot before);
    qreal brushDiameter() const { return m_diameter; }
    const std::vector<std::shared_ptr<SketchLayer>>& layers() const { return m_layers; }

signals:
    // Pronto: a folha foi salva. `image` = papel + camadas visíveis.
    void finished(const QString& cardId, const QImage& image);
    void photoRequested(const QImage& image);   // "Usar como foto do personagem"

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    struct UndoStep {
        enum Kind { Stroke, AddLayer, RemoveLayer, MoveLayer } kind = Stroke;
        std::shared_ptr<SketchLayer> layer;
        SketchSurface::TileSnapshot before, after;
        int index = 0, from = 0, to = 0;
    };
    void buildUi();
    void layoutUi();
    void refreshZoomLabel();
    QLabel* m_zoomLbl = nullptr;
    void load();
    bool save();
    void done();
    void undo();
    void redo();
    void applyStep(UndoStep& s, bool undoing);
    void refreshLayers();
    void refreshBrushes();
    void refreshSwatches();
    void selectBrush(const QString& id);
    void applyBrush();
    std::shared_ptr<SketchLayer> newLayer(const QString& name);
    void addLayer();
    void removeLayer();
    void moveLayer(int delta);
    void clearLayer();
    void importBrushes();
    void applyTheme();

    QString m_root, m_cardId, m_title;
    std::vector<std::shared_ptr<SketchLayer>> m_layers;   // de baixo pra cima
    int m_active = 0;
    int m_layerSeq = 0;
    std::unique_ptr<SketchBrush> m_brush, m_eraser;
    SketchBrushes::Info m_brushInfo;
    QColor m_color;
    qreal  m_diameter = 5;
    qreal  m_opacity = 1.0;
    std::vector<UndoStep> m_undo, m_redo;

    SketchCanvas* m_canvas = nullptr;
    QWidget*  m_caption = nullptr;
    QLabel*   m_titleLbl = nullptr;
    QWidget*  m_layersBox = nullptr;
    QVBoxLayout* m_layerRows = nullptr;
    QSlider*  m_layerOp = nullptr;
    QLabel*   m_layerOpVal = nullptr;
    QToolButton *m_layUp = nullptr, *m_layDown = nullptr, *m_layAdd = nullptr, *m_layDel = nullptr;
    QWidget*  m_side = nullptr;
    QScrollArea* m_sideScroll = nullptr;
    QVBoxLayout* m_brushRows = nullptr;
    QVBoxLayout* m_userRows = nullptr;
    QLabel*   m_importNote = nullptr;
    QList<QPair<QToolButton*, QColor>> m_swatches;
    QToolButton* m_pick = nullptr;
    QSlider*  m_sizeSlider = nullptr;
    QLabel*   m_sizeVal = nullptr;
    QSlider*  m_opSlider = nullptr;
    QLabel*   m_opVal = nullptr;
    QSlider*  m_stabSlider = nullptr;
    QLabel*   m_stabVal = nullptr;
};
