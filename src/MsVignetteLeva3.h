#pragma once
// Vinhetas da leva 3 (famílias 32 em diante): port linha a linha do concept aprovado
// (C:\mira-writing\concepts\manuscritos\vinhetas-leva3.html), desenhado pelo MsCanvas.
// Entram no app uma leva por vez; isPorted() diz quais já estão aqui.

#include <QColor>
#include <QPainter>
#include <QSize>
#include <QString>

namespace MsVignetteLeva3 {

bool isPorted(int family);

// Desenha a vinheta inteira (fundo, recorte e anel) em coordenadas lógicas do painter.
// Devolve false se a família ainda não foi portada.
bool render(QPainter& p, QSize size, const QString& seedId, int words, const QColor& base, int family, bool rect);

}
