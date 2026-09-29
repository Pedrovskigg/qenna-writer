#pragma once
// Capa rápida: a capa feita dentro do Qenna, sem o Cover Creator. Uma foto dos
// temas (ou uma cor, ou uma imagem do computador), o degradê e o grão do fundo
// dos temas (BackgroundOverlay) e textos (título, autor e o que mais o autor
// quiser) com fonte, estilo, cor e posição. O mesmo render() desenha a prévia
// da folha, a imagem que vira a capa do projeto (coverFull, e coverBg sem o
// texto) e o PNG de "Salvar como imagem" — então o que se vê é o que se grava.
// As coordenadas dos textos são frações da capa (0..1), e os tamanhos são na
// capa de referência de 240×360: vale em qualquer resolução.

#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

namespace QuickCover {

constexpr qreal kRefW = 240.0;
constexpr qreal kRefH = 360.0;

struct Text {
    QString role;          // "title" | "author" | "extra"
    QString text;          // só "extra" usa: título e autor vêm dos campos do projeto
    QString family;
    qreal   size = 36;     // px na capa de referência
    QPointF pos{0.5, 0.72};
    bool    bold = false;
    bool    italic = false;
    bool    underline = false;
    QColor  color{QStringLiteral("#f6efe3")};
    qreal   spacing = 0;   // em "em"
    bool    upper = false;
};

struct Spec {
    // foto: nome do arquivo em theme-images; "custom" = customImage; vazio = cor
    QString image;
    QString customImage;   // data URL (imagem do computador)
    QColor  bg{QStringLiteral("#7a1f24")};
    int     fadeType = 0;  // Theme::BackgroundOverlayType
    QColor  fadeColor{Qt::black};
    int     fadeOpacity = 75;
    int     fadeSize = 55;
    int     grain = 0;
    int     grainSize = 2;
    QVector<Text> texts;
};

// Capa inicial: cor, título grande embaixo, autor em versalete no pé.
Spec defaults(const QString& family);

QJsonObject toJson(const Spec& s);
Spec fromJson(const QJsonObject& o, const QString& fallbackFamily);

// Desenha a capa em size. withText = false é a versão sem letras (a textura
// do menu principal). hitRects (opcional) recebe o retângulo de cada texto, na
// capa de referência, na ordem de s.texts.
QImage render(const Spec& s, const QSize& size, bool withText,
              const QString& title, const QString& author,
              QVector<QRectF>* hitRects = nullptr);

// data URL JPEG (o formato das capas do projeto).
QString toDataUrl(const QImage& img);

// As fontes embarcadas no Qenna (livres de direitos): as únicas que a capa
// rápida oferece. main.cpp registra no início.
void setBundledFamilies(const QStringList& families);
QStringList bundledFamilies();

// Fotos dos temas (a pasta theme-images do app), em ordem.
QStringList imageNames();
// Miniatura cortada no formato da capa (cache em memória; a primeira leitura
// decodifica já reduzida, então é rápida).
QPixmap thumbnail(const QString& name, const QSize& size);

} // namespace QuickCover
