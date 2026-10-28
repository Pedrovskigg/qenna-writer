// A barra de título no tema do Qenna, no Mac (a versão do Windows fica em
// WindowChrome.cpp). Sem isso a barra vinha clara, do sistema, em cima de tema
// escuro. Com a barra transparente, o Mac pinta ela com a cor de fundo da
// janela; a aparência clara/escura decide a cor do título e dos botões.

#import <AppKit/AppKit.h>

#include <QColor>

namespace WindowChrome {

void applyMac(void* nativeView, const QColor& bg, bool dark)
{
    NSView* view = static_cast<NSView*>(nativeView);
    NSWindow* window = view.window;
    // sem janela ainda: o WinIdChange/Show seguinte chama de novo
    if (!window) return;
    window.titlebarAppearsTransparent = YES;
    window.backgroundColor = [NSColor colorWithSRGBRed:bg.redF() green:bg.greenF() blue:bg.blueF() alpha:1.0];
    window.appearance = [NSAppearance appearanceNamed:dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
}

} // namespace WindowChrome
