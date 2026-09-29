#include "WindowChrome.h"

#include "Theme.h"

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QMainWindow>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif
#endif

namespace WindowChrome {

namespace {

// Só janela com moldura do sistema: folhas, popups, tooltips e splash são
// sem moldura (ou nem são janelas) e não têm barra pra pintar.
bool hasSystemFrame(const QWidget* w)
{
    if (!w || !w->isWindow()) return false;
    const Qt::WindowFlags f = w->windowFlags();
    if (f.testFlag(Qt::FramelessWindowHint)) return false;
    const Qt::WindowType type = w->windowType();
    return type == Qt::Window || type == Qt::Dialog;
}

class Watcher : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (e->type() == QEvent::Show || e->type() == QEvent::WinIdChange) {
            auto* w = qobject_cast<QWidget*>(o);
            if (w && hasSystemFrame(w)) apply(w);
        }
        return false;
    }
};

} // namespace

void apply(QWidget* window)
{
#ifdef Q_OS_WIN
    if (!hasSystemFrame(window)) return;
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const QColor bg(Theme::appBackground());
    const QColor text(Theme::textPrimary());
    // a principal emenda no fundo; as outras se destacam com a borda dos painéis
    const bool main = qobject_cast<QMainWindow*>(window) != nullptr;
    const QColor border = main ? bg : QColor(Theme::panelBorder());
    const COLORREF crBg = RGB(bg.red(), bg.green(), bg.blue());
    const COLORREF crText = RGB(text.red(), text.green(), text.blue());
    const COLORREF crBorder = RGB(border.red(), border.green(), border.blue());
    // botões de minimizar/maximizar/fechar claros em tema escuro e vice-versa
    const BOOL dark = bg.lightness() < 128 ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &crBg, sizeof(crBg));
    DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &crText, sizeof(crText));
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &crBorder, sizeof(crBorder));
#else
    Q_UNUSED(window);
#endif
}

void install()
{
    static Watcher* watcher = nullptr;
    if (watcher) return;
    watcher = new Watcher(qApp);
    qApp->installEventFilter(watcher);
    // tema trocado: repinta as janelas abertas
    QObject::connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, watcher, []() {
        for (QWidget* w : QApplication::topLevelWidgets())
            if (w->isVisible() && hasSystemFrame(w)) apply(w);
    });
}

} // namespace WindowChrome
