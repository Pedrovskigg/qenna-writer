#include "Theme.h"

#include <QSettings>

namespace Theme {

Manager* Manager::instance()
{
    static Manager* mgr = new Manager();
    return mgr;
}

Manager::Manager()
{
    loadBundled();
    loadFromSettings();
}

void Manager::loadBundled()
{
    {
        MiraTheme t;
        // Mantemos o id "full-black" pra não invalidar QSettings antigos,
        // mas o nome de exibição é "Grafite" — clássico do Mira 1.
        t.id = QStringLiteral("full-black");
        t.name = QStringLiteral("Grafite");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#0a0a0a");  // R10 G10 B10
        t.panelBackground  = QStringLiteral("#0d0d0d");  // R13 G13 B13
        t.panelBorder      = QStringLiteral("#2a2a2a");
        t.textPrimary      = QStringLiteral("#e0e0e0");  // R224 G224 B224
        t.textMuted        = QStringLiteral("#888888");
        t.textBright       = QStringLiteral("#f5f5f5");
        t.hoverOverlay     = QStringLiteral("rgba(255,255,255,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(255,255,255,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(255,255,255,0.10)");
        t.accentDefault    = QStringLiteral("#3a8c7a");
        t.editorBackground = QStringLiteral("#101010");  // R16 G16 B16
        t.editorTextColor  = QStringLiteral("#e0e0e0");  // R224 G224 B224
        t.pageWidth = 820;
        t.pageVerticalMargin = 10;
        // Sombra sutil em fundo escuro: só pra dar volume.
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,160)");
        t.pageShadowRadius = 22;
        t.pageShadowOffset = 4;
        m_themes.append(t);
    }
    {
        MiraTheme t;
        t.id = QStringLiteral("full-white");
        t.name = QStringLiteral("Full White");
        t.bundled = true;
        // Branco de verdade — sem tons de creme/amarelo. Contraste da página
        // contra os painéis vem da sombra projetada e de uma diferença sutil
        // de cinza, nada de matiz quente.
        t.appBackground    = QStringLiteral("#dcdcdc");
        t.panelBackground  = QStringLiteral("#f4f4f4");
        t.panelBorder      = QStringLiteral("#b8b8b8");
        t.textPrimary      = QStringLiteral("#1a1a1a");
        t.textMuted        = QStringLiteral("#6e6e6e");
        t.textBright       = QStringLiteral("#0a0a0a");
        t.hoverOverlay     = QStringLiteral("rgba(0,0,0,0.05)");
        t.pressedOverlay   = QStringLiteral("rgba(0,0,0,0.03)");
        t.subtleBorder     = QStringLiteral("rgba(0,0,0,0.10)");
        t.accentDefault    = QStringLiteral("#3a8c7a");
        t.editorBackground = QStringLiteral("#ffffff");
        t.editorTextColor  = QStringLiteral("#141414");
        t.pageWidth = 820;
        t.pageVerticalMargin = 10;
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,90)");
        t.pageShadowRadius = 28;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
}

void Manager::loadFromSettings()
{
    QSettings s;
    const QString id = s.value(QStringLiteral("theme/currentId"),
                               QStringLiteral("full-black")).toString();
    for (int i = 0; i < m_themes.size(); ++i) {
        if (m_themes.at(i).id == id) {
            m_currentIndex = i;
            return;
        }
    }
    m_currentIndex = 0;
}

void Manager::saveToSettings() const
{
    QSettings s;
    s.setValue(QStringLiteral("theme/currentId"),
               m_themes.at(m_currentIndex).id);
}

const MiraTheme& Manager::current() const
{
    return m_themes.at(m_currentIndex);
}

const QList<MiraTheme>& Manager::available() const
{
    return m_themes;
}

void Manager::setCurrent(const QString& id)
{
    for (int i = 0; i < m_themes.size(); ++i) {
        if (m_themes.at(i).id == id) {
            if (i == m_currentIndex) return;
            m_currentIndex = i;
            saveToSettings();
            emit themeChanged();
            return;
        }
    }
}

// ---- API legada ----
QString appBackground()     { return Manager::instance()->current().appBackground; }
QString panelBackground()   { return Manager::instance()->current().panelBackground; }
QString panelBorder()       { return Manager::instance()->current().panelBorder; }
QString panelBorderRadius() { return QStringLiteral("10px"); }
QString textPrimary()       { return Manager::instance()->current().textPrimary; }
QString textMuted()         { return Manager::instance()->current().textMuted; }
QString textBright()        { return Manager::instance()->current().textBright; }
QString hoverOverlay()      { return Manager::instance()->current().hoverOverlay; }
QString pressedOverlay()    { return Manager::instance()->current().pressedOverlay; }
QString subtleBorder()      { return Manager::instance()->current().subtleBorder; }
QString accentDefault()     { return Manager::instance()->current().accentDefault; }

QString panelQss(const QString& objectName)
{
    return QStringLiteral(R"(
        #%1 {
            background: %2;
            border: 1px solid %3;
            border-radius: %4;
        }
    )").arg(objectName, panelBackground(), panelBorder(), panelBorderRadius());
}

int pageWidth()              { return Manager::instance()->current().pageWidth; }
int pageVerticalMargin()     { return Manager::instance()->current().pageVerticalMargin; }
QString editorBackground()   { return Manager::instance()->current().editorBackground; }
QString editorTextColor()    { return Manager::instance()->current().editorTextColor; }
bool pageShadowEnabled()     { return Manager::instance()->current().pageShadowEnabled; }
QString pageShadowColor()    { return Manager::instance()->current().pageShadowColor; }
int pageShadowRadius()       { return Manager::instance()->current().pageShadowRadius; }
int pageShadowOffset()       { return Manager::instance()->current().pageShadowOffset; }

} // namespace Theme
