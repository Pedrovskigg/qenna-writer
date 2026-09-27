#pragma once

#include "Theme.h"

#include <QCache>
#include <QColor>
#include <QList>
#include <QObject>
#include <QPixmap>
#include <QRectF>
#include <QSet>
#include <QString>

class QPainter;
class QThreadPool;

// Desenho do Qenna "em miniatura" pra escolher tema: a miniatura da grade,
// a janela inteira (TopToolbar, LeftBar, folha, Referência, contador, gaveta
// de Personagens) e a folha sozinha em tamanho de leitura. Tudo pintado com
// QPainter a partir das cores do MiraTheme — nenhum widget por tema, então o
// painel abre na hora mesmo com 300+ temas.
//
// A janela é desenhada numa cena lógica de 1440x758 (a tela 1918x1010 do
// usuário a 75%) e escalada pro retângulo pedido.
namespace ThemeScene {

enum Scene { Writing = 0, Drawers = 1, Editor = 2 };

constexpr qreal kSceneW = 1440.0;
constexpr qreal kSceneH = 758.0;

// Miniatura da grade (a do "Ao vivo"): fundo, topbar, lateral, folha com
// linhas de texto e o destaque. `bg` pode ser nulo enquanto a foto carrega.
void paintThumb(QPainter& p, const QRectF& r, const Theme::MiraTheme& t, const QPixmap& bg, qreal radius);

// Cena da prévia. Writing/Drawers desenham a janela; Editor desenha a folha.
void paintScene(QPainter& p, const QRectF& r, const Theme::MiraTheme& t, Scene scene, const QPixmap& bg);

// Partes da cena que o Criador de Temas edita ao clicar.
enum class Group { Background, Panels, Icons, Sheet, Title, Text, Accent };
struct Region {
    Group group;
    QString panel;   // Theme::PanelKey, só pra Panels
    QRectF rect;     // já em coordenadas de `r` (o retângulo pintado)
};
// Regiões clicáveis da cena pintada em `r`, em ordem de prioridade: a
// primeira que contém o ponto é a que o clique edita. A mesma geometria da
// pintura, então o realce cai exatamente em cima do que se vê.
QList<Region> regions(Scene scene, const QRectF& r);

// Contraste WCAG (1..21) entre duas cores.
double contrast(const QColor& a, const QColor& b);
// Quão parecidos são dois temas (fundo, folha, texto, destaque; foto pesa).
double distance(const Theme::MiraTheme& a, const Theme::MiraTheme& b);

// Fotos dos temas estampados, decodificadas já reduzidas numa thread à parte
// (DCT scaling do libjpeg) e guardadas em cache. get() devolve nulo e agenda a
// leitura quando ainda não tem; ready() avisa quando chegou.
class ImageCache : public QObject {
    Q_OBJECT
public:
    static ImageCache* instance();
    // cap = maior lado em px. Use kThumbCap pra grade e kSceneCap pra prévia.
    QPixmap get(const QString& path, int cap);

    static constexpr int kThumbCap = 360;
    static constexpr int kSceneCap = 1440;

signals:
    void ready(const QString& path, int cap);

private:
    ImageCache();
    void store(const QString& key, const QString& path, int cap, const QImage& img);

    QCache<QString, QPixmap> m_small;
    QCache<QString, QPixmap> m_large;
    QSet<QString> m_pending;
    QThreadPool* m_pool;
};

} // namespace ThemeScene
