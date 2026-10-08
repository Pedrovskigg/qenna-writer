#pragma once

#include <QString>
#include <QStringList>
#include <QColor>

// Estrutura de dados de um card na lousa. Compat com canvas.json do Mira 1.
struct CanvasCard {
    QString id;
    QString type;            // "note" | "comment" | "image" | "doc" | "character" | "chapter" | "text" | "symbol" | "sticker"
    qreal   x       = 0;
    qreal   y       = 0;
    qreal   width   = 200;
    qreal   height  = 160;
    QString title;
    QString content;
    QColor  color   = QColor(QStringLiteral("#f7d070"));
    QString linkedToConn;    // waypoint: ID da conexão à qual está ancorado ("" = livre)
    QString description;     // image: legenda/descrição
    QString photoDataUrl;    // character
    QString linkedItemId;    // doc, character; chapter: id do capítulo
    QString linkedDrawerKey; // doc, character; chapter: id do manuscrito
    // text / symbol
    int     fontSize = 0;    // 0 = default por tipo (text 18, symbol 60)
    bool    bold     = false;
    bool    italic   = false;
    qreal   rotation = 0;    // graus
    QString fontFamily;      // text: família da fonte ("" = Segoe UI)
    // text: largura em que o texto quebra a linha sozinho. 0 = sem quebra
    // (texto livre de antes do rework: cresce pro lado e só quebra no Enter).
    qreal   wrapWidth = 0;
    // note / comment: o jeito do papel (leva 2 do rework, 2026-10-07).
    // "" = o de sempre (quadrado com a dobrinha; o comentário é o balão).
    QString shape;           // "" | "index" | "strip" | "round" | "tag" | "torn" | "pinked" | "dymo"
    QString fastener;        // "" = pin | "tape" | "clip" | "staple" | "none"
    QString frame;           // "" = nenhuma | "pen" | "cut" | "corners"
    // sticker: o PNG (base64) fica em content; width/height = tamanho no quadro.
    bool    flipX    = false;
    bool    flipY    = false;
    QString outline;         // "" = recorte branco | "none" | "shadow"
    qreal   baseWidth = 0;   // largura com que entrou (100%); limita de 20% a 400%
    qreal   z        = 1;    // camada: > 0 por cima dos cards, < 0 atrás deles
    bool    locked   = false;
};

struct CanvasConnection {
    QString id;
    QString fromId;
    QString toId;
    QColor  color = QColor(QStringLiteral("#888888"));
    QStringList waypointCardIds;
    QString label;           // nome da ligação ("irmãos", "trai"…); "" = sem nome
    bool    arrow  = true;   // seta na ponta final
    bool    curved = false;  // padrão é reta, de pin a pin (quadro de investigação)
};

struct CanvasZone {
    QString id;
    qreal   x = 0, y = 0, width = 400, height = 300;
    QString title;
    QColor  color = QColor(QStringLiteral("#4a90d9"));
};
