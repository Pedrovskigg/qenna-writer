#pragma once

#include <QRectF>

class QPainter;

namespace Theme { struct MiraTheme; }

// Overlay do fundo do app (portado do Mira Cover, src/canvas/overlay.js):
// degradê por cima da cor ou da foto da mesa — embaixo, em cima, os dois ou
// vinheta — e grão de filme. Pinta só a mesa; painéis e página ficam por cima.
// O fundo real (BackgroundWidget) e as prévias (ThemeScene) chamam o mesmo
// paint(), então o que o Criador mostra é o que o app pinta.
namespace BackgroundOverlay {

// true quando o tema tem degradê ou grão — o fundo precisa ser pintado mesmo
// sem foto.
bool active(const Theme::MiraTheme& t);

// Degradê + grão sobre r, na ordem da capa (degradê primeiro, grão por cima).
void paint(QPainter& p, const QRectF& r, const Theme::MiraTheme& t);

} // namespace BackgroundOverlay
