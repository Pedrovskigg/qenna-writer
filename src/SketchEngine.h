#pragma once
// Motor do Esboço da Lousa (2026-10-10): a ponte entre o Qenna e o libmypaint
// (o motor de pincéis do MyPaint, licença ISC, em third_party/libmypaint).
//
// SketchSurface: uma camada da folha. Pixels em blocos de 64×64, no formato do
//   libmypaint (RGBA pré-multiplicado, 15 bits por canal); começa transparente.
//   Vira QImage pra aparecer na tela e guarda só os blocos mexidos pro desfazer.
// SketchBrush: um pincel .myb (o formato do MyPaint, do GIMP e do OpenToonz).
//   O .myb é lido com o Qt (sem json-c) e passado pro motor configuração por
//   configuração. Tamanho, opacidade e cor do Qenna entram por cima do pincel.

#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>

struct MyPaintBrush;
struct MyPaintSurface;
struct SketchTiledSurface;   // a superfície do libmypaint (SketchEngine.cpp)

class SketchSurface
{
public:
    SketchSurface(int width, int height);
    ~SketchSurface();
    SketchSurface(const SketchSurface&) = delete;
    SketchSurface& operator=(const SketchSurface&) = delete;

    int width() const { return m_w; }
    int height() const { return m_h; }
    MyPaintSurface* surface() const;

    // Um traço é um "atômico" do libmypaint: começa, pinta, termina. O fim
    // devolve a área mexida (em pixels da folha).
    void  beginAtomic();
    QRect endAtomic();

    // A camada como imagem (ARGB32 pré-multiplicado), inteira ou só um pedaço.
    QImage toImage() const;
    void   renderInto(QImage& target, const QRect& area) const;
    void   loadImage(const QImage& image);   // PNG salvo → camada
    void   clear();
    bool   isEmpty() const;

    // Desfazer: guarda e devolve os blocos que cobrem `area`.
    struct TileSnapshot { QHash<int, QByteArray> tiles; };
    TileSnapshot snapshot(const QRect& area) const;
    TileSnapshot snapshotAll() const;
    void restore(const TileSnapshot& snap);
    static int tileSize();
    // Gravação pro desfazer de um traço: cada bloco que o pincel pede é
    // copiado como estava, antes da primeira pincelada nele.
    void startRecording();
    TileSnapshot takeRecording();

private:
    int m_w = 0, m_h = 0;
    SketchTiledSurface* m_s = nullptr;
};

class SketchBrush
{
public:
    SketchBrush();
    ~SketchBrush();
    SketchBrush(const SketchBrush&) = delete;
    SketchBrush& operator=(const SketchBrush&) = delete;

    // Lê um .myb (JSON). Falso se não for um pincel do MyPaint.
    bool loadMyb(const QByteArray& json, QString* error = nullptr);

    void setColor(const QColor& c);
    // Diâmetro em pixels da folha (o tamanho natural do pincel é baseDiameter()).
    void setDiameter(qreal px);
    qreal baseDiameter() const { return m_baseDiameter; }
    // 0..1, multiplica a opacidade do próprio pincel.
    void setOpacity(qreal op);
    // Borracha: o mesmo pincel apagando (configuração "eraser" do MyPaint).
    void setEraser(bool on);

    void newStroke();
    // dtime: segundos desde o ponto anterior (o MyPaint usa velocidade).
    void strokeTo(SketchSurface& s, qreal x, qreal y, qreal pressure,
                  qreal xtilt = 0, qreal ytilt = 0, qreal dtime = 0.01);

private:
    void applyOverrides();
    MyPaintBrush* m_b = nullptr;
    qreal  m_baseRadiusLog = 1.0;   // radius_logarithmic do .myb
    qreal  m_baseDiameter = 5.4;
    qreal  m_baseOpaque = 1.0;
    qreal  m_baseEraser = 0.0;
    qreal  m_diameter = 0;          // 0 = o do pincel
    qreal  m_opacity = 1.0;
    bool   m_eraser = false;
    QColor m_color = Qt::black;
};
