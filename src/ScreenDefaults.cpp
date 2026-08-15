#include "ScreenDefaults.h"

#include "EditorLayout.h"

#include <QGuiApplication>
#include <QScreen>

namespace ScreenDefaults {

namespace {

int primaryScreenWidth()
{
    if (QScreen* screen = QGuiApplication::primaryScreen())
        return screen->availableGeometry().width();
    return 1920; // fallback razoável se não houver tela (ex.: ambiente headless)
}

}

int recommendedPageWidth()
{
    // "Chute" pra tela grande — igual ao default histórico do app.
    constexpr int kIdealPageWidth = 960;
    constexpr int kMinPageWidthFallback = 560;
    // LeftBar + margens do container + o painel de contagem flutuante (ver
    // WordCountPanel, kPanelWidth=256+scrollbar), que fica colado no canto
    // inferior esquerdo — sem essa reserva, numa tela menor a página larga
    // demais deixa o painel por cima do próprio texto em vez da margem.
    constexpr int kChromeReserve = 640;

    const int screenW = primaryScreenWidth();
    const int fit = screenW - kChromeReserve;
    const int target = qMax(kMinPageWidthFallback, qMin(kIdealPageWidth, fit));
    return qBound(EditorLayout::Manager::minPageWidth(), target,
                  EditorLayout::Manager::maxPageWidth());
}

qreal recommendedFontSize()
{
    const int screenW = primaryScreenWidth();
    if (screenW >= 1920) return 16.0;
    if (screenW >= 1440) return 15.0;
    if (screenW >= 1280) return 14.0;
    return 13.0;
}

} // namespace ScreenDefaults
