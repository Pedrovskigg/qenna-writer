#pragma once

#include <QString>

namespace Quotes {

// Devolve um quote aleatório do pool (sem manter estado).
QString pickRandom();

// Avança e devolve o próximo quote do ciclo ponderado. Quotes "feature" têm
// peso 2x (compat Mira 1: buildWeightedQuoteCycle). O ciclo + ponteiro são
// persistidos em QSettings, então a sequência continua entre sessões. Quando
// esgota, reembaralha automaticamente.
QString next();

int regularCount();
int featureCount();

} // namespace Quotes
