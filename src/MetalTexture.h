#pragma once

#include <QColor>
#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>

// Material de metal dos painéis e da mesa (Metal Lab, 22/11/2026, criado pro tema
// do Transformers). O painel deixa de ser cor lisa e vira um material com textura,
// cada um com a sua regra de luz:
//   brushed  aço escovado: risco fino na horizontal, reflexo em faixas verticais
//   plate    chapa de blindagem: placas com emenda funda, bisel, chanfro e rebites
//   paint    chapa pintada e gasta: tinta lascada nas quinas, riscos mostrando o aço
//   chrome   cromo esfumaçado: espelho do ambiente (céu, horizonte escuro, chão)
//   heat     titânio queimado: aço escovado com as faixas de cor do calor
// A cor base é a do painel (ou a da mesa). A textura é gerada sob medida pro
// tamanho pedido e guardada em cache: pintar de novo não custa nada.
namespace Metal {

// Os ids, na ordem do Criador. Vazio = sem material (cor lisa, como sempre).
QStringList kinds();
bool isValid(const QString& kind);
// Nome traduzido pro Criador.
QString label(const QString& kind);

// Textura no tamanho lógico `size`, com `dpr` pixels por ponto. desk = mesa
// (placas grandes, desencontradas); senão, painel (uma placa por trecho).
// strength (1..100): abaixo de 100 a textura vai por cima da cor base só com essa opacidade.
QImage texture(const QString& kind, const QColor& base, const QSize& size, qreal dpr, bool desk, int strength = 100);

} // namespace Metal
