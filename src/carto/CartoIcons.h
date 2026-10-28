#pragma once

// Ícones de traço do Mapa (os mesmos do Map Lab), pintados na cor pedida.

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace carto {

QPixmap iconPixmap(const QString& name, const QColor& color, int px, qreal dpr = 2.0);
QIcon icon(const QString& name, const QColor& normal, const QColor& active, int px = 18);

} // namespace carto
