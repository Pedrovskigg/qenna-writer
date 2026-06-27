#include "Theme.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace Theme {

namespace {

// Resolve o caminho de uma imagem bundled em assets/theme-images/.
// Tenta primeiro ao lado do exe (deploy) e depois DEV_ASSETS_DIR (dev).
// Vazio se não encontrar.
QString bundledImagePath(const QString& filename)
{
    const QString prod = QCoreApplication::applicationDirPath()
        + QStringLiteral("/assets/theme-images/") + filename;
    if (QFile::exists(prod)) return prod;
#ifdef DEV_ASSETS_DIR
    const QString dev = QString::fromUtf8(DEV_ASSETS_DIR)
        + QStringLiteral("/theme-images/") + filename;
    if (QFile::exists(dev)) return dev;
#endif
    return QString();
}

} // namespace

Manager* Manager::instance()
{
    static Manager* mgr = new Manager();
    return mgr;
}

Manager::Manager()
{
    loadBundled();
    m_bundledCount = m_themes.size();
    loadCustomThemes();
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

        t.hoverStrong         = QStringLiteral("rgba(255,255,255,0.12)");
        t.borderStrong        = QStringLiteral("rgba(255,255,255,0.20)");
        t.focusBorder         = QStringLiteral("rgba(255,255,255,0.30)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText        = QStringLiteral("rgba(255,255,255,0.30)");
        t.selectionRing       = QStringLiteral("#ffffff");

        t.accentSuccess           = QStringLiteral("#7BC592");
        t.accentSuccessSoft       = QStringLiteral("rgba(120,200,140,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(123,197,146,0.50)");
        t.accentDanger            = QStringLiteral("#e05555");
        t.accentDangerSoft        = QStringLiteral("rgba(224,85,85,0.12)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(224,85,85,0.40)");
        t.accentWarning           = QStringLiteral("#d66060");
        t.accentInfo              = QStringLiteral("#4a9eff");
        t.accentInfoSoft          = QStringLiteral("rgba(74,158,255,0.30)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(74,158,255,0.60)");

        t.editorBackground = QStringLiteral("#101010");  // R16 G16 B16
        t.editorTextColor  = QStringLiteral("#e0e0e0");  // R224 G224 B224
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
        t.hoverOverlay     = QStringLiteral("rgba(0,0,0,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(0,0,0,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(0,0,0,0.12)");
        t.accentDefault    = QStringLiteral("#2f7565");

        t.hoverStrong         = QStringLiteral("rgba(0,0,0,0.10)");
        t.borderStrong        = QStringLiteral("rgba(0,0,0,0.22)");
        t.focusBorder         = QStringLiteral("rgba(0,0,0,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.04)");
        t.disabledText        = QStringLiteral("rgba(0,0,0,0.32)");
        t.selectionRing       = QStringLiteral("#1a1a1a");

        // Verdes/vermelhos mais escuros pra terem contraste sobre o branco.
        t.accentSuccess           = QStringLiteral("#2f8a4f");
        t.accentSuccessSoft       = QStringLiteral("rgba(47,138,79,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(47,138,79,0.45)");
        t.accentDanger            = QStringLiteral("#b73030");
        t.accentDangerSoft        = QStringLiteral("rgba(183,48,48,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(183,48,48,0.45)");
        t.accentWarning           = QStringLiteral("#b03a3a");
        t.accentInfo              = QStringLiteral("#1f6fd6");
        t.accentInfoSoft          = QStringLiteral("rgba(31,111,214,0.18)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(31,111,214,0.55)");

        t.editorBackground = QStringLiteral("#ffffff");
        t.editorTextColor  = QStringLiteral("#141414");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,90)");
        t.pageShadowRadius = 28;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Solarized Dark — paleta clássica do Ethan Schoonover. Fundo
        // azul-petróleo escuro, texto bege esverdeado. Accent em ciano.
        MiraTheme t;
        t.id = QStringLiteral("solarized-dark");
        t.name = QStringLiteral("Solarized Dark");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#002b36");  // base03
        t.panelBackground  = QStringLiteral("#073642");  // base02
        t.panelBorder      = QStringLiteral("#0f4754");
        t.textPrimary      = QStringLiteral("#93a1a1");  // base1
        t.textMuted        = QStringLiteral("#586e75");  // base01
        t.textBright       = QStringLiteral("#eee8d5");  // base2
        t.hoverOverlay     = QStringLiteral("rgba(238,232,213,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(238,232,213,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(238,232,213,0.10)");
        t.accentDefault    = QStringLiteral("#2aa198");  // cyan

        t.hoverStrong         = QStringLiteral("rgba(238,232,213,0.12)");
        t.borderStrong        = QStringLiteral("rgba(238,232,213,0.20)");
        t.focusBorder         = QStringLiteral("rgba(238,232,213,0.30)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.25)");
        t.disabledText        = QStringLiteral("rgba(238,232,213,0.30)");
        t.selectionRing       = QStringLiteral("#fdf6e3");

        t.accentSuccess           = QStringLiteral("#859900");  // green
        t.accentSuccessSoft       = QStringLiteral("rgba(133,153,0,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(133,153,0,0.50)");
        t.accentDanger            = QStringLiteral("#dc322f");  // red
        t.accentDangerSoft        = QStringLiteral("rgba(220,50,47,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(220,50,47,0.45)");
        t.accentWarning           = QStringLiteral("#cb4b16");  // orange
        t.accentInfo              = QStringLiteral("#268bd2");  // blue
        t.accentInfoSoft          = QStringLiteral("rgba(38,139,210,0.25)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(38,139,210,0.55)");

        t.editorBackground = QStringLiteral("#073642");
        t.editorTextColor  = QStringLiteral("#93a1a1");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,160)");
        t.pageShadowRadius = 22;
        t.pageShadowOffset = 4;
        m_themes.append(t);
    }
    {
        // Solarized Light — mesma paleta, polos invertidos. Fundo creme
        // claro, texto cinza-azulado. Accents idênticos ao dark.
        MiraTheme t;
        t.id = QStringLiteral("solarized-light");
        t.name = QStringLiteral("Solarized Light");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#eee8d5");  // base2
        t.panelBackground  = QStringLiteral("#fdf6e3");  // base3
        t.panelBorder      = QStringLiteral("#d8d2c0");
        t.textPrimary      = QStringLiteral("#586e75");  // base01
        t.textMuted        = QStringLiteral("#93a1a1");  // base1
        t.textBright       = QStringLiteral("#002b36");  // base03
        t.hoverOverlay     = QStringLiteral("rgba(0,43,54,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(0,43,54,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(0,43,54,0.12)");
        t.accentDefault    = QStringLiteral("#2aa198");

        t.hoverStrong         = QStringLiteral("rgba(0,43,54,0.10)");
        t.borderStrong        = QStringLiteral("rgba(0,43,54,0.22)");
        t.focusBorder         = QStringLiteral("rgba(0,43,54,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,43,54,0.04)");
        t.disabledText        = QStringLiteral("rgba(0,43,54,0.32)");
        t.selectionRing       = QStringLiteral("#002b36");

        t.accentSuccess           = QStringLiteral("#5e7300");
        t.accentSuccessSoft       = QStringLiteral("rgba(94,115,0,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,115,0,0.50)");
        t.accentDanger            = QStringLiteral("#b32421");
        t.accentDangerSoft        = QStringLiteral("rgba(179,36,33,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(179,36,33,0.45)");
        t.accentWarning           = QStringLiteral("#a23a10");
        t.accentInfo              = QStringLiteral("#1e6ca8");
        t.accentInfoSoft          = QStringLiteral("rgba(30,108,168,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(30,108,168,0.55)");

        t.editorBackground = QStringLiteral("#fdf6e3");
        t.editorTextColor  = QStringLiteral("#073642");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,43,54,80)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Sépia — inspirado no FocusWriter "Old Paper". Papel envelhecido,
        // texto marrom escuro. Sensação de máquina de escrever.
        MiraTheme t;
        t.id = QStringLiteral("sepia");
        t.name = QStringLiteral("Sépia");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#e8dcc0");
        t.panelBackground  = QStringLiteral("#f4ecd8");
        t.panelBorder      = QStringLiteral("#c8b896");
        t.textPrimary      = QStringLiteral("#5b4636");
        t.textMuted        = QStringLiteral("#8a7355");
        t.textBright       = QStringLiteral("#2e2118");
        t.hoverOverlay     = QStringLiteral("rgba(91,70,54,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(91,70,54,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(91,70,54,0.14)");
        t.accentDefault    = QStringLiteral("#9a5a2b");  // terracota

        t.hoverStrong         = QStringLiteral("rgba(91,70,54,0.10)");
        t.borderStrong        = QStringLiteral("rgba(91,70,54,0.24)");
        t.focusBorder         = QStringLiteral("rgba(91,70,54,0.34)");
        t.inputBackground     = QStringLiteral("rgba(91,70,54,0.05)");
        t.disabledText        = QStringLiteral("rgba(91,70,54,0.32)");
        t.selectionRing       = QStringLiteral("#2e2118");

        t.accentSuccess           = QStringLiteral("#5e7a3a");  // verde-oliva
        t.accentSuccessSoft       = QStringLiteral("rgba(94,122,58,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,122,58,0.45)");
        t.accentDanger            = QStringLiteral("#a83333");
        t.accentDangerSoft        = QStringLiteral("rgba(168,51,51,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(168,51,51,0.45)");
        t.accentWarning           = QStringLiteral("#a83333");
        t.accentInfo              = QStringLiteral("#3a6b8a");  // azul-acinzentado
        t.accentInfoSoft          = QStringLiteral("rgba(58,107,138,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(58,107,138,0.55)");

        t.editorBackground = QStringLiteral("#f4ecd8");
        t.editorTextColor  = QStringLiteral("#3a2e22");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(91,70,54,90)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Âmbar — mel dourado. O mais quente e amarelo da família clara;
        // luz de fim de tarde sobre a página, dourado e convidativo.
        MiraTheme t;
        t.id = QStringLiteral("amber");
        t.name = QStringLiteral("Âmbar");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#ecdcae");
        t.panelBackground  = QStringLiteral("#f6ead0");
        t.panelBorder      = QStringLiteral("#cdb784");
        t.textPrimary      = QStringLiteral("#5a4422");
        t.textMuted        = QStringLiteral("#8d7242");
        t.textBright       = QStringLiteral("#2f2210");
        t.hoverOverlay     = QStringLiteral("rgba(90,68,34,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(90,68,34,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(90,68,34,0.14)");
        t.accentDefault    = QStringLiteral("#b5722a");  // âmbar profundo

        t.hoverStrong         = QStringLiteral("rgba(90,68,34,0.10)");
        t.borderStrong        = QStringLiteral("rgba(90,68,34,0.24)");
        t.focusBorder         = QStringLiteral("rgba(90,68,34,0.34)");
        t.inputBackground     = QStringLiteral("rgba(90,68,34,0.05)");
        t.disabledText        = QStringLiteral("rgba(90,68,34,0.32)");
        t.selectionRing       = QStringLiteral("#2f2210");

        t.accentSuccess           = QStringLiteral("#5e7a3a");
        t.accentSuccessSoft       = QStringLiteral("rgba(94,122,58,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,122,58,0.45)");
        t.accentDanger            = QStringLiteral("#a83333");
        t.accentDangerSoft        = QStringLiteral("rgba(168,51,51,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(168,51,51,0.45)");
        t.accentWarning           = QStringLiteral("#b5722a");
        t.accentInfo              = QStringLiteral("#3a6b8a");
        t.accentInfoSoft          = QStringLiteral("rgba(58,107,138,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(58,107,138,0.55)");

        t.editorBackground = QStringLiteral("#f6ead0");
        t.editorTextColor  = QStringLiteral("#3a2c14");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(90,68,34,90)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Pergaminho — creme pálido e arejado. Baixo contraste, o mais suave
        // da família clara; descanso pros olhos em sessões longas.
        MiraTheme t;
        t.id = QStringLiteral("parchment");
        t.name = QStringLiteral("Pergaminho");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#f0e8d6");
        t.panelBackground  = QStringLiteral("#f8f2e6");
        t.panelBorder      = QStringLiteral("#d8cbb0");
        t.textPrimary      = QStringLiteral("#5f5240");
        t.textMuted        = QStringLiteral("#998a72");
        t.textBright       = QStringLiteral("#382e20");
        t.hoverOverlay     = QStringLiteral("rgba(70,60,45,0.05)");
        t.pressedOverlay   = QStringLiteral("rgba(70,60,45,0.035)");
        t.subtleBorder     = QStringLiteral("rgba(70,60,45,0.12)");
        t.accentDefault    = QStringLiteral("#a07850");  // bronze suave

        t.hoverStrong         = QStringLiteral("rgba(70,60,45,0.09)");
        t.borderStrong        = QStringLiteral("rgba(70,60,45,0.20)");
        t.focusBorder         = QStringLiteral("rgba(70,60,45,0.30)");
        t.inputBackground     = QStringLiteral("rgba(70,60,45,0.04)");
        t.disabledText        = QStringLiteral("rgba(70,60,45,0.30)");
        t.selectionRing       = QStringLiteral("#382e20");

        t.accentSuccess           = QStringLiteral("#5e7a3a");
        t.accentSuccessSoft       = QStringLiteral("rgba(94,122,58,0.13)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,122,58,0.42)");
        t.accentDanger            = QStringLiteral("#a84a40");
        t.accentDangerSoft        = QStringLiteral("rgba(168,74,64,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(168,74,64,0.42)");
        t.accentWarning           = QStringLiteral("#c08a40");
        t.accentInfo              = QStringLiteral("#4a7390");
        t.accentInfoSoft          = QStringLiteral("rgba(74,115,144,0.15)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(74,115,144,0.50)");

        t.editorBackground = QStringLiteral("#f8f2e6");
        t.editorTextColor  = QStringLiteral("#423827");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(95,82,64,70)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 4;
        m_themes.append(t);
    }
    {
        // Caramelo — bege torrado mais profundo. Aconchego denso, tom de
        // doce-de-leite; mais saturado e "fechado" que o Sépia.
        MiraTheme t;
        t.id = QStringLiteral("caramel");
        t.name = QStringLiteral("Caramelo");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#e3cda3");
        t.panelBackground  = QStringLiteral("#eedcbc");
        t.panelBorder      = QStringLiteral("#c2a574");
        t.textPrimary      = QStringLiteral("#523c24");
        t.textMuted        = QStringLiteral("#856444");
        t.textBright       = QStringLiteral("#2c1d0e");
        t.hoverOverlay     = QStringLiteral("rgba(82,60,36,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(82,60,36,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(82,60,36,0.14)");
        t.accentDefault    = QStringLiteral("#a85d24");  // caramelo queimado

        t.hoverStrong         = QStringLiteral("rgba(82,60,36,0.10)");
        t.borderStrong        = QStringLiteral("rgba(82,60,36,0.24)");
        t.focusBorder         = QStringLiteral("rgba(82,60,36,0.34)");
        t.inputBackground     = QStringLiteral("rgba(82,60,36,0.05)");
        t.disabledText        = QStringLiteral("rgba(82,60,36,0.32)");
        t.selectionRing       = QStringLiteral("#2c1d0e");

        t.accentSuccess           = QStringLiteral("#5e7335");
        t.accentSuccessSoft       = QStringLiteral("rgba(94,115,53,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,115,53,0.45)");
        t.accentDanger            = QStringLiteral("#a33030");
        t.accentDangerSoft        = QStringLiteral("rgba(163,48,48,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(163,48,48,0.45)");
        t.accentWarning           = QStringLiteral("#a85d24");
        t.accentInfo              = QStringLiteral("#3a6580");
        t.accentInfoSoft          = QStringLiteral("rgba(58,101,128,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(58,101,128,0.55)");

        t.editorBackground = QStringLiteral("#eedcbc");
        t.editorTextColor  = QStringLiteral("#38280f");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(82,60,36,95)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Nord — paleta ártica do Arctic Ice Studio. Cinzas azulados frios
        // com accents em ciano-glaciar e auroras.
        MiraTheme t;
        t.id = QStringLiteral("nord");
        t.name = QStringLiteral("Nord");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#2e3440");  // nord0
        t.panelBackground  = QStringLiteral("#3b4252");  // nord1
        t.panelBorder      = QStringLiteral("#4c566a");  // nord3
        t.textPrimary      = QStringLiteral("#d8dee9");  // nord4
        t.textMuted        = QStringLiteral("#7b8394");
        t.textBright       = QStringLiteral("#eceff4");  // nord6
        t.hoverOverlay     = QStringLiteral("rgba(216,222,233,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(216,222,233,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(216,222,233,0.10)");
        t.accentDefault    = QStringLiteral("#88c0d0");  // nord8 frost ciano

        t.hoverStrong         = QStringLiteral("rgba(216,222,233,0.12)");
        t.borderStrong        = QStringLiteral("rgba(216,222,233,0.22)");
        t.focusBorder         = QStringLiteral("rgba(216,222,233,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.22)");
        t.disabledText        = QStringLiteral("rgba(216,222,233,0.30)");
        t.selectionRing       = QStringLiteral("#eceff4");

        t.accentSuccess           = QStringLiteral("#a3be8c");  // nord14 aurora green
        t.accentSuccessSoft       = QStringLiteral("rgba(163,190,140,0.20)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(163,190,140,0.55)");
        t.accentDanger            = QStringLiteral("#bf616a");  // nord11 aurora red
        t.accentDangerSoft        = QStringLiteral("rgba(191,97,106,0.15)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(191,97,106,0.50)");
        t.accentWarning           = QStringLiteral("#d08770");  // nord12 aurora orange
        t.accentInfo              = QStringLiteral("#81a1c1");  // nord9 frost azul
        t.accentInfoSoft          = QStringLiteral("rgba(129,161,193,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(129,161,193,0.55)");

        t.editorBackground = QStringLiteral("#3b4252");
        t.editorTextColor  = QStringLiteral("#d8dee9");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,160)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // High Contrast — acessibilidade. Preto puro, texto branco puro,
        // accents saturados ao extremo. Sem sutilezas.
        MiraTheme t;
        t.id = QStringLiteral("high-contrast");
        t.name = QStringLiteral("Alto Contraste");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#000000");
        t.panelBackground  = QStringLiteral("#0a0a0a");
        t.panelBorder      = QStringLiteral("#ffffff");
        t.textPrimary      = QStringLiteral("#ffffff");
        t.textMuted        = QStringLiteral("#bdbdbd");
        t.textBright       = QStringLiteral("#ffffff");
        t.hoverOverlay     = QStringLiteral("rgba(255,255,255,0.15)");
        t.pressedOverlay   = QStringLiteral("rgba(255,255,255,0.08)");
        t.subtleBorder     = QStringLiteral("rgba(255,255,255,0.40)");
        t.accentDefault    = QStringLiteral("#ffff00");  // amarelo puro

        t.hoverStrong         = QStringLiteral("rgba(255,255,255,0.25)");
        t.borderStrong        = QStringLiteral("rgba(255,255,255,0.70)");
        t.focusBorder         = QStringLiteral("#ffff00");
        t.inputBackground     = QStringLiteral("rgba(255,255,255,0.06)");
        t.disabledText        = QStringLiteral("rgba(255,255,255,0.45)");
        t.selectionRing       = QStringLiteral("#ffff00");

        t.accentSuccess           = QStringLiteral("#00ff7f");  // verde saturado
        t.accentSuccessSoft       = QStringLiteral("rgba(0,255,127,0.20)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(0,255,127,0.70)");
        t.accentDanger            = QStringLiteral("#ff4040");  // vermelho saturado
        t.accentDangerSoft        = QStringLiteral("rgba(255,64,64,0.20)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(255,64,64,0.70)");
        t.accentWarning           = QStringLiteral("#ffa500");  // laranja
        t.accentInfo              = QStringLiteral("#00d0ff");  // ciano vibrante
        t.accentInfoSoft          = QStringLiteral("rgba(0,208,255,0.25)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(0,208,255,0.70)");

        t.editorBackground = QStringLiteral("#000000");
        t.editorTextColor  = QStringLiteral("#ffffff");
        t.pageShadowEnabled = false;  // não combina com o estilo limpo
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,0)");
        t.pageShadowRadius = 0;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }
    {
        // Snow Storm — variante clara oficial do Nord (Polar Night → Snow Storm).
        // Mesma família, polos invertidos: fundo branco-glaciar, texto Polar Night.
        MiraTheme t;
        t.id = QStringLiteral("nord-light");
        t.name = QStringLiteral("Snow Storm");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#d8dee9");  // nord4
        t.panelBackground  = QStringLiteral("#e5e9f0");  // nord5
        t.panelBorder      = QStringLiteral("#bcc4d2");
        t.textPrimary      = QStringLiteral("#3b4252");  // nord1
        t.textMuted        = QStringLiteral("#6c7080");
        t.textBright       = QStringLiteral("#2e3440");  // nord0
        t.hoverOverlay     = QStringLiteral("rgba(46,52,64,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(46,52,64,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(46,52,64,0.12)");
        t.accentDefault    = QStringLiteral("#5e81ac");  // nord10 deep frost

        t.hoverStrong         = QStringLiteral("rgba(46,52,64,0.10)");
        t.borderStrong        = QStringLiteral("rgba(46,52,64,0.22)");
        t.focusBorder         = QStringLiteral("rgba(46,52,64,0.32)");
        t.inputBackground     = QStringLiteral("rgba(46,52,64,0.04)");
        t.disabledText        = QStringLiteral("rgba(46,52,64,0.32)");
        t.selectionRing       = QStringLiteral("#2e3440");

        // Auroras escurecidas pra ter contraste no fundo claro.
        t.accentSuccess           = QStringLiteral("#6e8a4f");
        t.accentSuccessSoft       = QStringLiteral("rgba(110,138,79,0.16)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(110,138,79,0.50)");
        t.accentDanger            = QStringLiteral("#9d3e47");
        t.accentDangerSoft        = QStringLiteral("rgba(157,62,71,0.12)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(157,62,71,0.50)");
        t.accentWarning           = QStringLiteral("#aa5536");
        t.accentInfo              = QStringLiteral("#5e81ac");
        t.accentInfoSoft          = QStringLiteral("rgba(94,129,172,0.18)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(94,129,172,0.55)");

        t.editorBackground = QStringLiteral("#eceff4");  // nord6
        t.editorTextColor  = QStringLiteral("#2e3440");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(46,52,64,80)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Nord Royal — Nord com a aurora purple como accent default e o yellow
        // entrando no mix. Tom mais místico/literário, ainda dentro da família.
        MiraTheme t;
        t.id = QStringLiteral("nord-royal");
        t.name = QStringLiteral("Nord Royal");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#2e3440");
        t.panelBackground  = QStringLiteral("#3b4252");
        t.panelBorder      = QStringLiteral("#4c566a");
        t.textPrimary      = QStringLiteral("#d8dee9");
        t.textMuted        = QStringLiteral("#7b8394");
        t.textBright       = QStringLiteral("#eceff4");
        t.hoverOverlay     = QStringLiteral("rgba(216,222,233,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(216,222,233,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(216,222,233,0.10)");
        t.accentDefault    = QStringLiteral("#b48ead");  // nord15 aurora purple

        t.hoverStrong         = QStringLiteral("rgba(216,222,233,0.12)");
        t.borderStrong        = QStringLiteral("rgba(216,222,233,0.22)");
        t.focusBorder         = QStringLiteral("rgba(216,222,233,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.22)");
        t.disabledText        = QStringLiteral("rgba(216,222,233,0.30)");
        t.selectionRing       = QStringLiteral("#eceff4");

        t.accentSuccess           = QStringLiteral("#a3be8c");  // aurora green
        t.accentSuccessSoft       = QStringLiteral("rgba(163,190,140,0.20)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(163,190,140,0.55)");
        t.accentDanger            = QStringLiteral("#bf616a");  // aurora red
        t.accentDangerSoft        = QStringLiteral("rgba(191,97,106,0.15)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(191,97,106,0.50)");
        t.accentWarning           = QStringLiteral("#ebcb8b");  // nord13 aurora yellow
        t.accentInfo              = QStringLiteral("#88c0d0");  // nord8 frost cyan
        t.accentInfoSoft          = QStringLiteral("rgba(136,192,208,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(136,192,208,0.55)");

        t.editorBackground = QStringLiteral("#3b4252");
        t.editorTextColor  = QStringLiteral("#d8dee9");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,160)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Tokyo Night — paleta do Enkia. Fundo azul-marinho profundo,
        // texto lavanda, accents em ciano elétrico e magenta. Vibe noturna
        // urbana, kanji em neon de Shibuya.
        MiraTheme t;
        t.id = QStringLiteral("tokyo-night");
        t.name = QStringLiteral("Tokyo Night");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#1a1b26");
        t.panelBackground  = QStringLiteral("#24283b");
        t.panelBorder      = QStringLiteral("#3b4261");
        t.textPrimary      = QStringLiteral("#a9b1d6");  // fg_dark
        t.textMuted        = QStringLiteral("#565f89");  // comment
        t.textBright       = QStringLiteral("#c0caf5");  // fg
        t.hoverOverlay     = QStringLiteral("rgba(192,202,245,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(192,202,245,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(192,202,245,0.10)");
        t.accentDefault    = QStringLiteral("#7aa2f7");  // blue

        t.hoverStrong         = QStringLiteral("rgba(192,202,245,0.12)");
        t.borderStrong        = QStringLiteral("rgba(192,202,245,0.22)");
        t.focusBorder         = QStringLiteral("rgba(192,202,245,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText        = QStringLiteral("rgba(192,202,245,0.30)");
        t.selectionRing       = QStringLiteral("#c0caf5");

        t.accentSuccess           = QStringLiteral("#9ece6a");  // green
        t.accentSuccessSoft       = QStringLiteral("rgba(158,206,106,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(158,206,106,0.55)");
        t.accentDanger            = QStringLiteral("#f7768e");  // red
        t.accentDangerSoft        = QStringLiteral("rgba(247,118,142,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(247,118,142,0.50)");
        t.accentWarning           = QStringLiteral("#ff9e64");  // orange
        t.accentInfo              = QStringLiteral("#bb9af7");  // magenta/lavanda
        t.accentInfoSoft          = QStringLiteral("rgba(187,154,247,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(187,154,247,0.55)");

        t.editorBackground = QStringLiteral("#1f2335");  // bg meio termo
        t.editorTextColor  = QStringLiteral("#c0caf5");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,170)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Dracula — paleta oficial (draculatheme.com). Roxo elétrico sobre
        // cinza-azulado escuro, accents em ciano/verde/rosa.
        MiraTheme t;
        t.id = QStringLiteral("dracula");
        t.name = QStringLiteral("Dracula");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#282a36");  // background
        t.panelBackground  = QStringLiteral("#343746");
        t.panelBorder      = QStringLiteral("#44475a");  // current line
        t.textPrimary      = QStringLiteral("#f8f8f2");  // foreground
        t.textMuted        = QStringLiteral("#6272a4");  // comment
        t.textBright       = QStringLiteral("#ffffff");
        t.hoverOverlay     = QStringLiteral("rgba(248,248,242,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(248,248,242,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(248,248,242,0.10)");
        t.accentDefault    = QStringLiteral("#bd93f9");  // purple

        t.hoverStrong         = QStringLiteral("rgba(248,248,242,0.12)");
        t.borderStrong        = QStringLiteral("rgba(248,248,242,0.22)");
        t.focusBorder         = QStringLiteral("rgba(248,248,242,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.25)");
        t.disabledText        = QStringLiteral("rgba(248,248,242,0.30)");
        t.selectionRing       = QStringLiteral("#ffffff");

        t.accentSuccess           = QStringLiteral("#50fa7b");  // green
        t.accentSuccessSoft       = QStringLiteral("rgba(80,250,123,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(80,250,123,0.55)");
        t.accentDanger            = QStringLiteral("#ff5555");  // red
        t.accentDangerSoft        = QStringLiteral("rgba(255,85,85,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(255,85,85,0.50)");
        t.accentWarning           = QStringLiteral("#ffb86c");  // orange
        t.accentInfo              = QStringLiteral("#8be9fd");  // cyan
        t.accentInfoSoft          = QStringLiteral("rgba(139,233,253,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(139,233,253,0.55)");

        t.editorBackground = QStringLiteral("#21222c");
        t.editorTextColor  = QStringLiteral("#f8f8f2");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,170)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Gruvbox Dark — paleta retrô warm. Marrom-mostarda, accent amarelo
        // dourado, fundo bege-petróleo. Sensação de máquina de escrever moderna.
        MiraTheme t;
        t.id = QStringLiteral("gruvbox-dark");
        t.name = QStringLiteral("Gruvbox");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#282828");  // bg0
        t.panelBackground  = QStringLiteral("#3c3836");  // bg1
        t.panelBorder      = QStringLiteral("#504945");  // bg2
        t.textPrimary      = QStringLiteral("#ebdbb2");  // fg1
        t.textMuted        = QStringLiteral("#a89984");  // gray
        t.textBright       = QStringLiteral("#fbf1c7");  // fg0
        t.hoverOverlay     = QStringLiteral("rgba(235,219,178,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(235,219,178,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(235,219,178,0.10)");
        t.accentDefault    = QStringLiteral("#d79921");  // yellow

        t.hoverStrong         = QStringLiteral("rgba(235,219,178,0.12)");
        t.borderStrong        = QStringLiteral("rgba(235,219,178,0.22)");
        t.focusBorder         = QStringLiteral("rgba(235,219,178,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.22)");
        t.disabledText        = QStringLiteral("rgba(235,219,178,0.30)");
        t.selectionRing       = QStringLiteral("#fbf1c7");

        t.accentSuccess           = QStringLiteral("#98971a");  // green
        t.accentSuccessSoft       = QStringLiteral("rgba(152,151,26,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(152,151,26,0.50)");
        t.accentDanger            = QStringLiteral("#cc241d");  // red
        t.accentDangerSoft        = QStringLiteral("rgba(204,36,29,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(204,36,29,0.50)");
        t.accentWarning           = QStringLiteral("#d65d0e");  // orange
        t.accentInfo              = QStringLiteral("#458588");  // blue
        t.accentInfoSoft          = QStringLiteral("rgba(69,133,136,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(69,133,136,0.55)");

        t.editorBackground = QStringLiteral("#32302f");
        t.editorTextColor  = QStringLiteral("#ebdbb2");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,160)");
        t.pageShadowRadius = 22;
        t.pageShadowOffset = 4;
        m_themes.append(t);
    }
    {
        // Catppuccin Mocha — paleta pastel-dark moderna (catppuccin.com).
        // Lilás suave sobre azul-marinho fosco. Vibe acolhedora, cores macias.
        MiraTheme t;
        t.id = QStringLiteral("catppuccin-mocha");
        t.name = QStringLiteral("Catppuccin Mocha");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#1e1e2e");  // base
        t.panelBackground  = QStringLiteral("#313244");  // surface0
        t.panelBorder      = QStringLiteral("#45475a");  // surface1
        t.textPrimary      = QStringLiteral("#cdd6f4");  // text
        t.textMuted        = QStringLiteral("#7f849c");  // overlay1
        t.textBright       = QStringLiteral("#f5e0dc");  // rosewater
        t.hoverOverlay     = QStringLiteral("rgba(205,214,244,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(205,214,244,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(205,214,244,0.10)");
        t.accentDefault    = QStringLiteral("#cba6f7");  // mauve

        t.hoverStrong         = QStringLiteral("rgba(205,214,244,0.12)");
        t.borderStrong        = QStringLiteral("rgba(205,214,244,0.22)");
        t.focusBorder         = QStringLiteral("rgba(205,214,244,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.25)");
        t.disabledText        = QStringLiteral("rgba(205,214,244,0.30)");
        t.selectionRing       = QStringLiteral("#f5e0dc");

        t.accentSuccess           = QStringLiteral("#a6e3a1");  // green
        t.accentSuccessSoft       = QStringLiteral("rgba(166,227,161,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(166,227,161,0.55)");
        t.accentDanger            = QStringLiteral("#f38ba8");  // red/pink
        t.accentDangerSoft        = QStringLiteral("rgba(243,139,168,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(243,139,168,0.50)");
        t.accentWarning           = QStringLiteral("#f9e2af");  // yellow
        t.accentInfo              = QStringLiteral("#89b4fa");  // blue
        t.accentInfoSoft          = QStringLiteral("rgba(137,180,250,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(137,180,250,0.55)");

        t.editorBackground = QStringLiteral("#181825");  // mantle
        t.editorTextColor  = QStringLiteral("#cdd6f4");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,170)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Mármore — claro frio, branco-azulado. Alternativa polida ao Full White,
        // mais "premium". Texto azul-marinho profundo, accent em azul-marinho.
        MiraTheme t;
        t.id = QStringLiteral("marble");
        t.name = QStringLiteral("Mármore");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#e8eaef");
        t.panelBackground  = QStringLiteral("#f5f6fa");
        t.panelBorder      = QStringLiteral("#c4c8d4");
        t.textPrimary      = QStringLiteral("#2a2e3b");
        t.textMuted        = QStringLiteral("#5e6478");
        t.textBright       = QStringLiteral("#15171f");
        t.hoverOverlay     = QStringLiteral("rgba(15,20,40,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(15,20,40,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(15,20,40,0.12)");
        t.accentDefault    = QStringLiteral("#2c4f78");  // azul-marinho frio

        t.hoverStrong         = QStringLiteral("rgba(15,20,40,0.10)");
        t.borderStrong        = QStringLiteral("rgba(15,20,40,0.22)");
        t.focusBorder         = QStringLiteral("rgba(15,20,40,0.32)");
        t.inputBackground     = QStringLiteral("rgba(15,20,40,0.04)");
        t.disabledText        = QStringLiteral("rgba(15,20,40,0.32)");
        t.selectionRing       = QStringLiteral("#15171f");

        t.accentSuccess           = QStringLiteral("#2f7355");
        t.accentSuccessSoft       = QStringLiteral("rgba(47,115,85,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(47,115,85,0.50)");
        t.accentDanger            = QStringLiteral("#a0353a");
        t.accentDangerSoft        = QStringLiteral("rgba(160,53,58,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(160,53,58,0.50)");
        t.accentWarning           = QStringLiteral("#a85a14");
        t.accentInfo              = QStringLiteral("#2a5fa3");
        t.accentInfoSoft          = QStringLiteral("rgba(42,95,163,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(42,95,163,0.55)");

        t.editorBackground = QStringLiteral("#ffffff");
        t.editorTextColor  = QStringLiteral("#15171f");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(15,20,40,90)");
        t.pageShadowRadius = 28;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Vermelho Velado — dark vinho/burgundy. Atmosfera literária, gótica
        // suave, romance noir. Accent em vermelho-vinho.
        MiraTheme t;
        t.id = QStringLiteral("velvet-red");
        t.name = QStringLiteral("Vermelho Velado");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#1a0d0e");
        t.panelBackground  = QStringLiteral("#2a1518");
        t.panelBorder      = QStringLiteral("#4a2528");
        t.textPrimary      = QStringLiteral("#e8d6cf");
        t.textMuted        = QStringLiteral("#8e6968");
        t.textBright       = QStringLiteral("#f5e8e3");
        t.hoverOverlay     = QStringLiteral("rgba(232,214,207,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(232,214,207,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(232,214,207,0.10)");
        t.accentDefault    = QStringLiteral("#b04050");  // vinho

        t.hoverStrong         = QStringLiteral("rgba(232,214,207,0.12)");
        t.borderStrong        = QStringLiteral("rgba(232,214,207,0.22)");
        t.focusBorder         = QStringLiteral("rgba(232,214,207,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText        = QStringLiteral("rgba(232,214,207,0.30)");
        t.selectionRing       = QStringLiteral("#f5e8e3");

        t.accentSuccess           = QStringLiteral("#7ba05b");  // verde-musgo
        t.accentSuccessSoft       = QStringLiteral("rgba(123,160,91,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(123,160,91,0.50)");
        t.accentDanger            = QStringLiteral("#d04848");
        t.accentDangerSoft        = QStringLiteral("rgba(208,72,72,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(208,72,72,0.50)");
        t.accentWarning           = QStringLiteral("#c87838");
        t.accentInfo              = QStringLiteral("#8a93b8");  // cinza-azulado suave
        t.accentInfoSoft          = QStringLiteral("rgba(138,147,184,0.20)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(138,147,184,0.55)");

        t.editorBackground = QStringLiteral("#221013");
        t.editorTextColor  = QStringLiteral("#e8d6cf");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,180)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Veredas — floresta com sol coando pelos galhos. Verde profundo
        // com dourado-terroso de accent. Vibe matinal, calmaria de mata.
        MiraTheme t;
        t.id = QStringLiteral("veredas");
        t.name = QStringLiteral("Veredas");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#1a1f1a");
        t.panelBackground  = QStringLiteral("#232a23");
        t.panelBorder      = QStringLiteral("#3a4438");
        t.textPrimary      = QStringLiteral("#d4cfb8");
        t.textMuted        = QStringLiteral("#8a8a72");
        t.textBright       = QStringLiteral("#f4ecd8");
        t.hoverOverlay     = QStringLiteral("rgba(212,207,184,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(212,207,184,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(212,207,184,0.10)");
        t.accentDefault    = QStringLiteral("#b89968");  // dourado terroso

        t.hoverStrong         = QStringLiteral("rgba(212,207,184,0.12)");
        t.borderStrong        = QStringLiteral("rgba(212,207,184,0.22)");
        t.focusBorder         = QStringLiteral("rgba(212,207,184,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText        = QStringLiteral("rgba(212,207,184,0.30)");
        t.selectionRing       = QStringLiteral("#f4ecd8");

        t.accentSuccess           = QStringLiteral("#7da855");
        t.accentSuccessSoft       = QStringLiteral("rgba(125,168,85,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(125,168,85,0.50)");
        t.accentDanger            = QStringLiteral("#b85040");
        t.accentDangerSoft        = QStringLiteral("rgba(184,80,64,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(184,80,64,0.50)");
        t.accentWarning           = QStringLiteral("#c89860");
        t.accentInfo              = QStringLiteral("#6f9a8f");
        t.accentInfoSoft          = QStringLiteral("rgba(111,154,143,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(111,154,143,0.55)");

        t.editorBackground = QStringLiteral("#1f2620");
        t.editorTextColor  = QStringLiteral("#e8e0c8");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,180)");
        t.pageShadowRadius = 28;
        t.pageShadowOffset = 6;
        t.backgroundImage  = bundledImagePath(QStringLiteral("pexels-njeromin-14367404.jpg"));
        t.backgroundMode   = BgZoom;
        t.editorOpacity    = 100;
        m_themes.append(t);
    }
    {
        // Tokyo Velvet — skyline noturno asiático com luzes laranjas/vermelhas.
        // Carmim quente sobre azul-meia-noite. Vibe noir urbana.
        MiraTheme t;
        t.id = QStringLiteral("tokyo-velvet");
        t.name = QStringLiteral("Tokyo Velvet");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#0e1422");
        t.panelBackground  = QStringLiteral("#161e2f");
        t.panelBorder      = QStringLiteral("#2a3548");
        t.textPrimary      = QStringLiteral("#d8d3c8");
        t.textMuted        = QStringLiteral("#7a7160");
        t.textBright       = QStringLiteral("#f0e8d9");
        t.hoverOverlay     = QStringLiteral("rgba(216,211,200,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(216,211,200,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(216,211,200,0.10)");
        t.accentDefault    = QStringLiteral("#d97a3b");  // laranja quente

        t.hoverStrong         = QStringLiteral("rgba(216,211,200,0.12)");
        t.borderStrong        = QStringLiteral("rgba(216,211,200,0.22)");
        t.focusBorder         = QStringLiteral("rgba(216,211,200,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText        = QStringLiteral("rgba(216,211,200,0.30)");
        t.selectionRing       = QStringLiteral("#f0e8d9");

        t.accentSuccess           = QStringLiteral("#7fa056");
        t.accentSuccessSoft       = QStringLiteral("rgba(127,160,86,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(127,160,86,0.50)");
        t.accentDanger            = QStringLiteral("#c84846");
        t.accentDangerSoft        = QStringLiteral("rgba(200,72,70,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(200,72,70,0.50)");
        t.accentWarning           = QStringLiteral("#d97a3b");
        t.accentInfo              = QStringLiteral("#5e7faa");
        t.accentInfoSoft          = QStringLiteral("rgba(94,127,170,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(94,127,170,0.55)");

        t.editorBackground = QStringLiteral("#131826");
        t.editorTextColor  = QStringLiteral("#e8e0c8");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,200)");
        t.pageShadowRadius = 28;
        t.pageShadowOffset = 6;
        t.backgroundImage  = bundledImagePath(QStringLiteral("pexels-rui-wang-16615369-11216105.jpg"));
        t.backgroundMode   = BgZoom;
        t.editorOpacity    = 100;
        m_themes.append(t);
    }
    {
        // Constelar — via láctea com nebulosa magenta/rosa. Preto profundo
        // com accent magenta estelar. Vibe space opera, contemplação noturna.
        MiraTheme t;
        t.id = QStringLiteral("constelar");
        t.name = QStringLiteral("Constelar");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#07050d");
        t.panelBackground  = QStringLiteral("#100b1c");
        t.panelBorder      = QStringLiteral("#28203f");
        t.textPrimary      = QStringLiteral("#d4c8e0");
        t.textMuted        = QStringLiteral("#756680");
        t.textBright       = QStringLiteral("#f0e6f5");
        t.hoverOverlay     = QStringLiteral("rgba(212,200,224,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(212,200,224,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(212,200,224,0.10)");
        t.accentDefault    = QStringLiteral("#c862a8");  // magenta estelar

        t.hoverStrong         = QStringLiteral("rgba(212,200,224,0.12)");
        t.borderStrong        = QStringLiteral("rgba(212,200,224,0.22)");
        t.focusBorder         = QStringLiteral("rgba(212,200,224,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.35)");
        t.disabledText        = QStringLiteral("rgba(212,200,224,0.30)");
        t.selectionRing       = QStringLiteral("#f0e6f5");

        t.accentSuccess           = QStringLiteral("#80b888");
        t.accentSuccessSoft       = QStringLiteral("rgba(128,184,136,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(128,184,136,0.50)");
        t.accentDanger            = QStringLiteral("#d56477");
        t.accentDangerSoft        = QStringLiteral("rgba(213,100,119,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(213,100,119,0.50)");
        t.accentWarning           = QStringLiteral("#d8a040");
        t.accentInfo              = QStringLiteral("#7a8ebf");
        t.accentInfoSoft          = QStringLiteral("rgba(122,142,191,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(122,142,191,0.55)");

        t.editorBackground = QStringLiteral("#0a0712");
        t.editorTextColor  = QStringLiteral("#e0d8e8");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,210)");
        t.pageShadowRadius = 32;
        t.pageShadowOffset = 7;
        t.backgroundImage  = bundledImagePath(QStringLiteral("pexels-samara-hammer-209546937-11737041.jpg"));
        t.backgroundMode   = BgZoom;
        t.editorOpacity    = 100;
        m_themes.append(t);
    }
    {
        // Maré — visão aérea de oceano turquesa. Papel claro flutuando sobre
        // água. Inverte o esquema: editor claro, painéis dark teal.
        MiraTheme t;
        t.id = QStringLiteral("mare");
        t.name = QStringLiteral("Maré");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#0a2027");
        t.panelBackground  = QStringLiteral("#0d2a32");
        t.panelBorder      = QStringLiteral("#1a3e48");
        t.textPrimary      = QStringLiteral("#c8e0dc");
        t.textMuted        = QStringLiteral("#6c8c8e");
        t.textBright       = QStringLiteral("#ecf6f4");
        t.hoverOverlay     = QStringLiteral("rgba(200,224,220,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(200,224,220,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(200,224,220,0.10)");
        t.accentDefault    = QStringLiteral("#2ca598");  // turquesa

        t.hoverStrong         = QStringLiteral("rgba(200,224,220,0.12)");
        t.borderStrong        = QStringLiteral("rgba(200,224,220,0.22)");
        t.focusBorder         = QStringLiteral("rgba(200,224,220,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.25)");
        t.disabledText        = QStringLiteral("rgba(200,224,220,0.30)");
        t.selectionRing       = QStringLiteral("#ecf6f4");

        t.accentSuccess           = QStringLiteral("#6fae6f");
        t.accentSuccessSoft       = QStringLiteral("rgba(111,174,111,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(111,174,111,0.50)");
        t.accentDanger            = QStringLiteral("#c84a48");
        t.accentDangerSoft        = QStringLiteral("rgba(200,74,72,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(200,74,72,0.50)");
        t.accentWarning           = QStringLiteral("#c89048");
        t.accentInfo              = QStringLiteral("#4a90a8");
        t.accentInfoSoft          = QStringLiteral("rgba(74,144,168,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(74,144,168,0.55)");

        t.editorBackground = QStringLiteral("#f6fbfa");  // papel claro flutuando sobre água
        t.editorTextColor  = QStringLiteral("#0d2a32");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(10,32,39,150)");
        t.pageShadowRadius = 32;
        t.pageShadowOffset = 8;
        t.backgroundImage  = bundledImagePath(QStringLiteral("pexels-damodigital-2274725.jpg"));
        t.backgroundMode   = BgZoom;
        t.editorOpacity    = 100;
        m_themes.append(t);
    }
    {
        // Brisa — céu de nuvens fofas em dia claro. Papel branco com nuvem
        // aparecendo atrás. Vibe leve, contemplativa, escrita ao ar livre.
        MiraTheme t;
        t.id = QStringLiteral("brisa");
        t.name = QStringLiteral("Brisa");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#d8e4ed");
        t.panelBackground  = QStringLiteral("#ebf2f7");
        t.panelBorder      = QStringLiteral("#b4c2d0");
        t.textPrimary      = QStringLiteral("#283545");
        t.textMuted        = QStringLiteral("#5e6c80");
        t.textBright       = QStringLiteral("#0c1421");
        t.hoverOverlay     = QStringLiteral("rgba(12,20,33,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(12,20,33,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(12,20,33,0.12)");
        t.accentDefault    = QStringLiteral("#4a7da8");  // azul-céu fechado

        t.hoverStrong         = QStringLiteral("rgba(12,20,33,0.10)");
        t.borderStrong        = QStringLiteral("rgba(12,20,33,0.22)");
        t.focusBorder         = QStringLiteral("rgba(12,20,33,0.32)");
        t.inputBackground     = QStringLiteral("rgba(12,20,33,0.04)");
        t.disabledText        = QStringLiteral("rgba(12,20,33,0.32)");
        t.selectionRing       = QStringLiteral("#0c1421");

        t.accentSuccess           = QStringLiteral("#4f8a5e");
        t.accentSuccessSoft       = QStringLiteral("rgba(79,138,94,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(79,138,94,0.50)");
        t.accentDanger            = QStringLiteral("#b53e3e");
        t.accentDangerSoft        = QStringLiteral("rgba(181,62,62,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(181,62,62,0.50)");
        t.accentWarning           = QStringLiteral("#c97540");
        t.accentInfo              = QStringLiteral("#3f6fa0");
        t.accentInfoSoft          = QStringLiteral("rgba(63,111,160,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(63,111,160,0.55)");

        t.editorBackground = QStringLiteral("#ffffff");
        t.editorTextColor  = QStringLiteral("#1a2230");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(12,20,33,80)");
        t.pageShadowRadius = 30;
        t.pageShadowOffset = 7;
        t.backgroundImage  = bundledImagePath(QStringLiteral("pexels-vinixhc-19924751.jpg"));
        t.backgroundMode   = BgZoom;
        t.editorOpacity    = 100;
        m_themes.append(t);
    }
    {
        // Rosé Pine — paleta oficial (rosepinetheme.com). Roxo-pino sobre
        // base ardósia, accents iris/rose/foam. Vibe romance gótico calmo.
        MiraTheme t;
        t.id = QStringLiteral("rose-pine");
        t.name = QStringLiteral("Rosé Pine");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#191724");  // base
        t.panelBackground  = QStringLiteral("#1f1d2e");  // surface
        t.panelBorder      = QStringLiteral("#26233a");  // overlay
        t.textPrimary      = QStringLiteral("#e0def4");  // text
        t.textMuted        = QStringLiteral("#6e6a86");  // subtle
        t.textBright       = QStringLiteral("#ffffff");
        t.hoverOverlay     = QStringLiteral("rgba(224,222,244,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(224,222,244,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(224,222,244,0.10)");
        t.accentDefault    = QStringLiteral("#c4a7e7");  // iris

        t.hoverStrong         = QStringLiteral("rgba(224,222,244,0.12)");
        t.borderStrong        = QStringLiteral("rgba(224,222,244,0.22)");
        t.focusBorder         = QStringLiteral("rgba(224,222,244,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.25)");
        t.disabledText        = QStringLiteral("rgba(224,222,244,0.30)");
        t.selectionRing       = QStringLiteral("#ffffff");

        t.accentSuccess           = QStringLiteral("#9ccfd8");  // foam
        t.accentSuccessSoft       = QStringLiteral("rgba(156,207,216,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(156,207,216,0.55)");
        t.accentDanger            = QStringLiteral("#eb6f92");  // love
        t.accentDangerSoft        = QStringLiteral("rgba(235,111,146,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(235,111,146,0.50)");
        t.accentWarning           = QStringLiteral("#f6c177");  // gold
        t.accentInfo              = QStringLiteral("#31748f");  // pine
        t.accentInfoSoft          = QStringLiteral("rgba(49,116,143,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(49,116,143,0.55)");

        t.editorBackground = QStringLiteral("#1a1828");
        t.editorTextColor  = QStringLiteral("#e0def4");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,170)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Café — marrom puro warm, atmosfera de biblioteca antiga. Accent
        // em caramelo, vibe noir clássica sem o vinho do Vermelho Velado.
        MiraTheme t;
        t.id = QStringLiteral("cafe");
        t.name = QStringLiteral("Café");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#1a1310");
        t.panelBackground  = QStringLiteral("#25201c");
        t.panelBorder      = QStringLiteral("#3d3328");
        t.textPrimary      = QStringLiteral("#d8c8b0");
        t.textMuted        = QStringLiteral("#8a7560");
        t.textBright       = QStringLiteral("#f0e6d8");
        t.hoverOverlay     = QStringLiteral("rgba(216,200,176,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(216,200,176,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(216,200,176,0.10)");
        t.accentDefault    = QStringLiteral("#c89868");  // caramelo

        t.hoverStrong         = QStringLiteral("rgba(216,200,176,0.12)");
        t.borderStrong        = QStringLiteral("rgba(216,200,176,0.22)");
        t.focusBorder         = QStringLiteral("rgba(216,200,176,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText        = QStringLiteral("rgba(216,200,176,0.30)");
        t.selectionRing       = QStringLiteral("#f0e6d8");

        t.accentSuccess           = QStringLiteral("#7ba055");
        t.accentSuccessSoft       = QStringLiteral("rgba(123,160,85,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(123,160,85,0.50)");
        t.accentDanger            = QStringLiteral("#b85040");
        t.accentDangerSoft        = QStringLiteral("rgba(184,80,64,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(184,80,64,0.50)");
        t.accentWarning           = QStringLiteral("#d8a040");
        t.accentInfo              = QStringLiteral("#6e8aa8");
        t.accentInfoSoft          = QStringLiteral("rgba(110,138,168,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(110,138,168,0.55)");

        t.editorBackground = QStringLiteral("#221a15");
        t.editorTextColor  = QStringLiteral("#e8d8c0");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,180)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Tangerine — escuro com laranja queimado vibrante. Cores de
        // pôr-do-sol denso, ímpeto, energia de escrita rápida.
        MiraTheme t;
        t.id = QStringLiteral("tangerine");
        t.name = QStringLiteral("Tangerine");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#1a1208");
        t.panelBackground  = QStringLiteral("#261810");
        t.panelBorder      = QStringLiteral("#3a2818");
        t.textPrimary      = QStringLiteral("#d8c8b0");
        t.textMuted        = QStringLiteral("#8a6e58");
        t.textBright       = QStringLiteral("#f0e0c8");
        t.hoverOverlay     = QStringLiteral("rgba(216,200,176,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(216,200,176,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(216,200,176,0.10)");
        t.accentDefault    = QStringLiteral("#e8744a");  // laranja queimado

        t.hoverStrong         = QStringLiteral("rgba(216,200,176,0.12)");
        t.borderStrong        = QStringLiteral("rgba(216,200,176,0.22)");
        t.focusBorder         = QStringLiteral("rgba(216,200,176,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText        = QStringLiteral("rgba(216,200,176,0.30)");
        t.selectionRing       = QStringLiteral("#f0e0c8");

        t.accentSuccess           = QStringLiteral("#7e9e58");
        t.accentSuccessSoft       = QStringLiteral("rgba(126,158,88,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(126,158,88,0.50)");
        t.accentDanger            = QStringLiteral("#c84048");
        t.accentDangerSoft        = QStringLiteral("rgba(200,64,72,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(200,64,72,0.50)");
        t.accentWarning           = QStringLiteral("#d8a840");
        t.accentInfo              = QStringLiteral("#6e8aa8");
        t.accentInfoSoft          = QStringLiteral("rgba(110,138,168,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(110,138,168,0.55)");

        t.editorBackground = QStringLiteral("#1f1610");
        t.editorTextColor  = QStringLiteral("#e8d8c0");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,190)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Camafeu — claro rosado quente sutil, alternativa "luz natural" pro
        // Sépia. Texto marrom-vinho escuro sobre fundo creme rosado.
        MiraTheme t;
        t.id = QStringLiteral("camafeu");
        t.name = QStringLiteral("Camafeu");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#f4e8e0");
        t.panelBackground  = QStringLiteral("#faf0e8");
        t.panelBorder      = QStringLiteral("#d4c0b0");
        t.textPrimary      = QStringLiteral("#4a2e28");
        t.textMuted        = QStringLiteral("#8a6258");
        t.textBright       = QStringLiteral("#2a1814");
        t.hoverOverlay     = QStringLiteral("rgba(74,46,40,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(74,46,40,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(74,46,40,0.14)");
        t.accentDefault    = QStringLiteral("#a85048");  // rosa-velho

        t.hoverStrong         = QStringLiteral("rgba(74,46,40,0.10)");
        t.borderStrong        = QStringLiteral("rgba(74,46,40,0.24)");
        t.focusBorder         = QStringLiteral("rgba(74,46,40,0.34)");
        t.inputBackground     = QStringLiteral("rgba(74,46,40,0.05)");
        t.disabledText        = QStringLiteral("rgba(74,46,40,0.32)");
        t.selectionRing       = QStringLiteral("#2a1814");

        t.accentSuccess           = QStringLiteral("#5e7838");
        t.accentSuccessSoft       = QStringLiteral("rgba(94,120,56,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,120,56,0.50)");
        t.accentDanger            = QStringLiteral("#a83838");
        t.accentDangerSoft        = QStringLiteral("rgba(168,56,56,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(168,56,56,0.50)");
        t.accentWarning           = QStringLiteral("#b06820");
        t.accentInfo              = QStringLiteral("#5a708a");
        t.accentInfoSoft          = QStringLiteral("rgba(90,112,138,0.18)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(90,112,138,0.55)");

        t.editorBackground = QStringLiteral("#ffffff");
        t.editorTextColor  = QStringLiteral("#3a221c");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(74,46,40,90)");
        t.pageShadowRadius = 28;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Aurora — galáxia rosa-turquesa em céu noturno. Accent rosa-aurora
        // com toques de turquesa (success). Vibe contemplativa, sci-fi soft.
        MiraTheme t;
        t.id = QStringLiteral("aurora");
        t.name = QStringLiteral("Aurora");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#0a0814");
        t.panelBackground  = QStringLiteral("#120e1f");
        t.panelBorder      = QStringLiteral("#2a2240");
        t.textPrimary      = QStringLiteral("#d8c8e0");
        t.textMuted        = QStringLiteral("#6e6080");
        t.textBright       = QStringLiteral("#f0e0f0");
        t.hoverOverlay     = QStringLiteral("rgba(216,200,224,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(216,200,224,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(216,200,224,0.10)");
        t.accentDefault    = QStringLiteral("#d894b8");  // rosa-aurora

        t.hoverStrong         = QStringLiteral("rgba(216,200,224,0.12)");
        t.borderStrong        = QStringLiteral("rgba(216,200,224,0.22)");
        t.focusBorder         = QStringLiteral("rgba(216,200,224,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.32)");
        t.disabledText        = QStringLiteral("rgba(216,200,224,0.30)");
        t.selectionRing       = QStringLiteral("#f0e0f0");

        t.accentSuccess           = QStringLiteral("#88c8b8");  // turquesa
        t.accentSuccessSoft       = QStringLiteral("rgba(136,200,184,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(136,200,184,0.50)");
        t.accentDanger            = QStringLiteral("#d56477");
        t.accentDangerSoft        = QStringLiteral("rgba(213,100,119,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(213,100,119,0.50)");
        t.accentWarning           = QStringLiteral("#d8a040");
        t.accentInfo              = QStringLiteral("#88a8d0");
        t.accentInfoSoft          = QStringLiteral("rgba(136,168,208,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(136,168,208,0.55)");

        t.editorBackground = QStringLiteral("#0c0a15");
        t.editorTextColor  = QStringLiteral("#e4d8e8");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,210)");
        t.pageShadowRadius = 32;
        t.pageShadowOffset = 7;
        t.backgroundImage  = bundledImagePath(QStringLiteral("pexels-moments-11738628.jpg"));
        t.backgroundMode   = BgZoom;
        t.editorOpacity    = 100;
        m_themes.append(t);
    }
    {
        // Hibisco — cidade neon rosa/roxo asiática. Vibrante, cyberpunk soft.
        // Accent magenta neon, painéis violeta escuro.
        MiraTheme t;
        t.id = QStringLiteral("hibisco");
        t.name = QStringLiteral("Hibisco");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#100818");
        t.panelBackground  = QStringLiteral("#1a1024");
        t.panelBorder      = QStringLiteral("#2c1e3a");
        t.textPrimary      = QStringLiteral("#e8d0e8");
        t.textMuted        = QStringLiteral("#806e88");
        t.textBright       = QStringLiteral("#f8e8f0");
        t.hoverOverlay     = QStringLiteral("rgba(232,208,232,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(232,208,232,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(232,208,232,0.10)");
        t.accentDefault    = QStringLiteral("#e060a8");  // magenta neon

        t.hoverStrong         = QStringLiteral("rgba(232,208,232,0.12)");
        t.borderStrong        = QStringLiteral("rgba(232,208,232,0.22)");
        t.focusBorder         = QStringLiteral("rgba(232,208,232,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.32)");
        t.disabledText        = QStringLiteral("rgba(232,208,232,0.30)");
        t.selectionRing       = QStringLiteral("#f8e8f0");

        t.accentSuccess           = QStringLiteral("#88c098");
        t.accentSuccessSoft       = QStringLiteral("rgba(136,192,152,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(136,192,152,0.50)");
        t.accentDanger            = QStringLiteral("#d85870");
        t.accentDangerSoft        = QStringLiteral("rgba(216,88,112,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(216,88,112,0.50)");
        t.accentWarning           = QStringLiteral("#d8a060");
        t.accentInfo              = QStringLiteral("#b888d8");  // lilás
        t.accentInfoSoft          = QStringLiteral("rgba(184,136,216,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(184,136,216,0.55)");

        t.editorBackground = QStringLiteral("#14101c");
        t.editorTextColor  = QStringLiteral("#f0d8e8");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,200)");
        t.pageShadowRadius = 30;
        t.pageShadowOffset = 6;
        t.backgroundImage  = bundledImagePath(QStringLiteral("pexels-what-640488395-19558160.jpg"));
        t.backgroundMode   = BgZoom;
        t.editorOpacity    = 100;
        m_themes.append(t);
    }
    {
        // Cerração — pinheiros densos sob névoa. Verde-musgo profundo,
        // ar saturado de mata fria. Vibe noir florestal.
        MiraTheme t;
        t.id = QStringLiteral("cerracao");
        t.name = QStringLiteral("Cerração");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#0e1310");
        t.panelBackground  = QStringLiteral("#161c18");
        t.panelBorder      = QStringLiteral("#2a342e");
        t.textPrimary      = QStringLiteral("#c8d4c8");
        t.textMuted        = QStringLiteral("#7a8478");
        t.textBright       = QStringLiteral("#e8efe8");
        t.hoverOverlay     = QStringLiteral("rgba(200,212,200,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(200,212,200,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(200,212,200,0.10)");
        t.accentDefault    = QStringLiteral("#6a9078");  // verde-musgo névoa

        t.hoverStrong         = QStringLiteral("rgba(200,212,200,0.12)");
        t.borderStrong        = QStringLiteral("rgba(200,212,200,0.22)");
        t.focusBorder         = QStringLiteral("rgba(200,212,200,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText        = QStringLiteral("rgba(200,212,200,0.30)");
        t.selectionRing       = QStringLiteral("#e8efe8");

        t.accentSuccess           = QStringLiteral("#80a868");
        t.accentSuccessSoft       = QStringLiteral("rgba(128,168,104,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(128,168,104,0.50)");
        t.accentDanger            = QStringLiteral("#b85048");
        t.accentDangerSoft        = QStringLiteral("rgba(184,80,72,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(184,80,72,0.50)");
        t.accentWarning           = QStringLiteral("#c89860");
        t.accentInfo              = QStringLiteral("#6a8898");
        t.accentInfoSoft          = QStringLiteral("rgba(106,136,152,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(106,136,152,0.55)");

        t.editorBackground = QStringLiteral("#131913");
        t.editorTextColor  = QStringLiteral("#d8e0d8");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,180)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 5;
        t.backgroundImage  = bundledImagePath(QStringLiteral("pexels-griffinw-34480262.jpg"));
        t.backgroundMode   = BgZoom;
        t.editorOpacity    = 100;
        m_themes.append(t);
    }
    {
        // Petróleo — marina noite com reflexo profundo. Preto-azulado denso,
        // accent azul-reflexo. Imersão noturna, contemplação aquática.
        MiraTheme t;
        t.id = QStringLiteral("petroleo");
        t.name = QStringLiteral("Petróleo");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#05070e");
        t.panelBackground  = QStringLiteral("#0a0e1a");
        t.panelBorder      = QStringLiteral("#18203a");
        t.textPrimary      = QStringLiteral("#c8d4e0");
        t.textMuted        = QStringLiteral("#6a7888");
        t.textBright       = QStringLiteral("#e8eff8");
        t.hoverOverlay     = QStringLiteral("rgba(200,212,224,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(200,212,224,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(200,212,224,0.10)");
        t.accentDefault    = QStringLiteral("#4a90c8");  // azul-reflexo

        t.hoverStrong         = QStringLiteral("rgba(200,212,224,0.12)");
        t.borderStrong        = QStringLiteral("rgba(200,212,224,0.22)");
        t.focusBorder         = QStringLiteral("rgba(200,212,224,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.35)");
        t.disabledText        = QStringLiteral("rgba(200,212,224,0.30)");
        t.selectionRing       = QStringLiteral("#e8eff8");

        t.accentSuccess           = QStringLiteral("#70a890");
        t.accentSuccessSoft       = QStringLiteral("rgba(112,168,144,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(112,168,144,0.50)");
        t.accentDanger            = QStringLiteral("#c85858");
        t.accentDangerSoft        = QStringLiteral("rgba(200,88,88,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(200,88,88,0.50)");
        t.accentWarning           = QStringLiteral("#c89048");
        t.accentInfo              = QStringLiteral("#7898c0");
        t.accentInfoSoft          = QStringLiteral("rgba(120,152,192,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(120,152,192,0.55)");

        t.editorBackground = QStringLiteral("#080c14");
        t.editorTextColor  = QStringLiteral("#d8e0e8");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,220)");
        t.pageShadowRadius = 30;
        t.pageShadowOffset = 7;
        t.backgroundImage  = bundledImagePath(QStringLiteral("pexels-pixabay-316093.jpg"));
        t.backgroundMode   = BgZoom;
        t.editorOpacity    = 100;
        m_themes.append(t);
    }
    {
        // Âmbar Noturno — a alma quente do Âmbar em versão escura. Mel dourado
        // sobre marrom-quase-preto; calor sem o brilho da página clara.
        MiraTheme t;
        t.id = QStringLiteral("amber-night");
        t.name = QStringLiteral("Âmbar Noturno");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#1c1408");
        t.panelBackground  = QStringLiteral("#271c0e");
        t.panelBorder      = QStringLiteral("#3d2e16");
        t.textPrimary      = QStringLiteral("#e6d2a6");
        t.textMuted        = QStringLiteral("#9a8050");
        t.textBright       = QStringLiteral("#fbeccb");
        t.hoverOverlay     = QStringLiteral("rgba(230,210,166,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(230,210,166,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(230,210,166,0.10)");
        t.accentDefault    = QStringLiteral("#e0a23c");  // mel dourado

        t.hoverStrong         = QStringLiteral("rgba(230,210,166,0.12)");
        t.borderStrong        = QStringLiteral("rgba(230,210,166,0.22)");
        t.focusBorder         = QStringLiteral("rgba(230,210,166,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText        = QStringLiteral("rgba(230,210,166,0.30)");
        t.selectionRing       = QStringLiteral("#fbeccb");

        t.accentSuccess           = QStringLiteral("#8aa84e");
        t.accentSuccessSoft       = QStringLiteral("rgba(138,168,78,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(138,168,78,0.50)");
        t.accentDanger            = QStringLiteral("#c8553f");
        t.accentDangerSoft        = QStringLiteral("rgba(200,85,63,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(200,85,63,0.50)");
        t.accentWarning           = QStringLiteral("#e0a23c");
        t.accentInfo              = QStringLiteral("#6f9ac0");
        t.accentInfoSoft          = QStringLiteral("rgba(111,154,192,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(111,154,192,0.55)");

        t.editorBackground = QStringLiteral("#221809");
        t.editorTextColor  = QStringLiteral("#ecdaab");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,185)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Trigo — campo de trigo ao sol. Claro dourado-esverdeado, mais "vegetal"
        // que o Âmbar; calor de palha seca.
        MiraTheme t;
        t.id = QStringLiteral("wheat");
        t.name = QStringLiteral("Trigo");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#e9e1c4");
        t.panelBackground  = QStringLiteral("#f3eed8");
        t.panelBorder      = QStringLiteral("#cdc59c");
        t.textPrimary      = QStringLiteral("#54502f");
        t.textMuted        = QStringLiteral("#8a8456");
        t.textBright       = QStringLiteral("#2c2912");
        t.hoverOverlay     = QStringLiteral("rgba(84,80,47,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(84,80,47,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(84,80,47,0.14)");
        t.accentDefault    = QStringLiteral("#9a7b2e");  // dourado-oliva

        t.hoverStrong         = QStringLiteral("rgba(84,80,47,0.10)");
        t.borderStrong        = QStringLiteral("rgba(84,80,47,0.24)");
        t.focusBorder         = QStringLiteral("rgba(84,80,47,0.34)");
        t.inputBackground     = QStringLiteral("rgba(84,80,47,0.05)");
        t.disabledText        = QStringLiteral("rgba(84,80,47,0.32)");
        t.selectionRing       = QStringLiteral("#2c2912");

        t.accentSuccess           = QStringLiteral("#5e7a3a");
        t.accentSuccessSoft       = QStringLiteral("rgba(94,122,58,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,122,58,0.45)");
        t.accentDanger            = QStringLiteral("#a83333");
        t.accentDangerSoft        = QStringLiteral("rgba(168,51,51,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(168,51,51,0.45)");
        t.accentWarning           = QStringLiteral("#9a7b2e");
        t.accentInfo              = QStringLiteral("#3a6b8a");
        t.accentInfoSoft          = QStringLiteral("rgba(58,107,138,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(58,107,138,0.55)");

        t.editorBackground = QStringLiteral("#f3eed8");
        t.editorTextColor  = QStringLiteral("#353117");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(84,80,47,90)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Oceano — azul profundo de mar aberto. Escuro com accent azul vivo;
        // calmo e espaçoso.
        MiraTheme t;
        t.id = QStringLiteral("ocean");
        t.name = QStringLiteral("Oceano");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#0c1a2a");
        t.panelBackground  = QStringLiteral("#102438");
        t.panelBorder      = QStringLiteral("#1d3550");
        t.textPrimary      = QStringLiteral("#c2d4e6");
        t.textMuted        = QStringLiteral("#6d8aa6");
        t.textBright       = QStringLiteral("#eaf2fb");
        t.hoverOverlay     = QStringLiteral("rgba(194,212,230,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(194,212,230,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(194,212,230,0.10)");
        t.accentDefault    = QStringLiteral("#3d9be0");  // azul-oceano

        t.hoverStrong         = QStringLiteral("rgba(194,212,230,0.12)");
        t.borderStrong        = QStringLiteral("rgba(194,212,230,0.22)");
        t.focusBorder         = QStringLiteral("rgba(194,212,230,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText        = QStringLiteral("rgba(194,212,230,0.30)");
        t.selectionRing       = QStringLiteral("#eaf2fb");

        t.accentSuccess           = QStringLiteral("#3fae8f");
        t.accentSuccessSoft       = QStringLiteral("rgba(63,174,143,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(63,174,143,0.50)");
        t.accentDanger            = QStringLiteral("#e0556a");
        t.accentDangerSoft        = QStringLiteral("rgba(224,85,106,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(224,85,106,0.50)");
        t.accentWarning           = QStringLiteral("#e0a23c");
        t.accentInfo              = QStringLiteral("#3d9be0");
        t.accentInfoSoft          = QStringLiteral("rgba(61,155,224,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(61,155,224,0.55)");

        t.editorBackground = QStringLiteral("#0e2032");
        t.editorTextColor  = QStringLiteral("#d6e4f0");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,190)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Floresta — verde de mata fechada. Escuro com accent verde-folha;
        // orgânico e tranquilo.
        MiraTheme t;
        t.id = QStringLiteral("forest");
        t.name = QStringLiteral("Floresta");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#0e1a12");
        t.panelBackground  = QStringLiteral("#14241a");
        t.panelBorder      = QStringLiteral("#20382a");
        t.textPrimary      = QStringLiteral("#c2d8c4");
        t.textMuted        = QStringLiteral("#6f8a74");
        t.textBright       = QStringLiteral("#e8f4ea");
        t.hoverOverlay     = QStringLiteral("rgba(194,216,196,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(194,216,196,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(194,216,196,0.10)");
        t.accentDefault    = QStringLiteral("#4fae5e");  // verde-folha

        t.hoverStrong         = QStringLiteral("rgba(194,216,196,0.12)");
        t.borderStrong        = QStringLiteral("rgba(194,216,196,0.22)");
        t.focusBorder         = QStringLiteral("rgba(194,216,196,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.26)");
        t.disabledText        = QStringLiteral("rgba(194,216,196,0.30)");
        t.selectionRing       = QStringLiteral("#e8f4ea");

        t.accentSuccess           = QStringLiteral("#6fbf5a");
        t.accentSuccessSoft       = QStringLiteral("rgba(111,191,90,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(111,191,90,0.50)");
        t.accentDanger            = QStringLiteral("#d8584a");
        t.accentDangerSoft        = QStringLiteral("rgba(216,88,74,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(216,88,74,0.50)");
        t.accentWarning           = QStringLiteral("#d8a23c");
        t.accentInfo              = QStringLiteral("#4f9ad0");
        t.accentInfoSoft          = QStringLiteral("rgba(79,154,208,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(79,154,208,0.55)");

        t.editorBackground = QStringLiteral("#112018");
        t.editorTextColor  = QStringLiteral("#d2e6d4");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,185)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Ametista — roxo de pedra preciosa. Escuro com accent violeta;
        // misterioso, noturno.
        MiraTheme t;
        t.id = QStringLiteral("amethyst");
        t.name = QStringLiteral("Ametista");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#160f24");
        t.panelBackground  = QStringLiteral("#1f1633");
        t.panelBorder      = QStringLiteral("#322447");
        t.textPrimary      = QStringLiteral("#d2c6e6");
        t.textMuted        = QStringLiteral("#8574a6");
        t.textBright       = QStringLiteral("#efe8fb");
        t.hoverOverlay     = QStringLiteral("rgba(210,198,230,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(210,198,230,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(210,198,230,0.10)");
        t.accentDefault    = QStringLiteral("#a06ce0");  // violeta

        t.hoverStrong         = QStringLiteral("rgba(210,198,230,0.12)");
        t.borderStrong        = QStringLiteral("rgba(210,198,230,0.22)");
        t.focusBorder         = QStringLiteral("rgba(210,198,230,0.32)");
        t.inputBackground     = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText        = QStringLiteral("rgba(210,198,230,0.30)");
        t.selectionRing       = QStringLiteral("#efe8fb");

        t.accentSuccess           = QStringLiteral("#6fb58a");
        t.accentSuccessSoft       = QStringLiteral("rgba(111,181,138,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(111,181,138,0.50)");
        t.accentDanger            = QStringLiteral("#e0567a");
        t.accentDangerSoft        = QStringLiteral("rgba(224,86,122,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(224,86,122,0.50)");
        t.accentWarning           = QStringLiteral("#e0a23c");
        t.accentInfo              = QStringLiteral("#6f8ae0");
        t.accentInfoSoft          = QStringLiteral("rgba(111,138,224,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(111,138,224,0.55)");

        t.editorBackground = QStringLiteral("#1b1230");
        t.editorTextColor  = QStringLiteral("#ddd2ef");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,190)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Lavanda — claro lilás suave. Versão luminosa do roxo; delicado e
        // calmo, alternativa colorida pros temas claros.
        MiraTheme t;
        t.id = QStringLiteral("lavender");
        t.name = QStringLiteral("Lavanda");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#ece8f4");
        t.panelBackground  = QStringLiteral("#f5f2fb");
        t.panelBorder      = QStringLiteral("#d2c8e2");
        t.textPrimary      = QStringLiteral("#4e4660");
        t.textMuted        = QStringLiteral("#897ea0");
        t.textBright       = QStringLiteral("#2c2440");
        t.hoverOverlay     = QStringLiteral("rgba(60,50,80,0.05)");
        t.pressedOverlay   = QStringLiteral("rgba(60,50,80,0.035)");
        t.subtleBorder     = QStringLiteral("rgba(60,50,80,0.12)");
        t.accentDefault    = QStringLiteral("#8a5fc0");  // lavanda

        t.hoverStrong         = QStringLiteral("rgba(60,50,80,0.09)");
        t.borderStrong        = QStringLiteral("rgba(60,50,80,0.20)");
        t.focusBorder         = QStringLiteral("rgba(60,50,80,0.30)");
        t.inputBackground     = QStringLiteral("rgba(60,50,80,0.04)");
        t.disabledText        = QStringLiteral("rgba(60,50,80,0.30)");
        t.selectionRing       = QStringLiteral("#2c2440");

        t.accentSuccess           = QStringLiteral("#5e8a55");
        t.accentSuccessSoft       = QStringLiteral("rgba(94,138,85,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,138,85,0.45)");
        t.accentDanger            = QStringLiteral("#b04a5a");
        t.accentDangerSoft        = QStringLiteral("rgba(176,74,90,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(176,74,90,0.45)");
        t.accentWarning           = QStringLiteral("#c08a40");
        t.accentInfo              = QStringLiteral("#5a6fc0");
        t.accentInfoSoft          = QStringLiteral("rgba(90,111,192,0.15)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(90,111,192,0.50)");

        t.editorBackground = QStringLiteral("#f5f2fb");
        t.editorTextColor  = QStringLiteral("#38304a");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(78,70,96,70)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 4;
        m_themes.append(t);
    }
    {
        // Esmeralda — claro verde-menta. Verde luminoso e fresco; alternativa
        // colorida clara, ar de jardim ao amanhecer.
        MiraTheme t;
        t.id = QStringLiteral("emerald");
        t.name = QStringLiteral("Esmeralda");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#e0eee4");
        t.panelBackground  = QStringLiteral("#eef7f0");
        t.panelBorder      = QStringLiteral("#c2d8c8");
        t.textPrimary      = QStringLiteral("#2f4a3c");
        t.textMuted        = QStringLiteral("#6f8a78");
        t.textBright       = QStringLiteral("#163026");
        t.hoverOverlay     = QStringLiteral("rgba(47,74,60,0.05)");
        t.pressedOverlay   = QStringLiteral("rgba(47,74,60,0.035)");
        t.subtleBorder     = QStringLiteral("rgba(47,74,60,0.12)");
        t.accentDefault    = QStringLiteral("#1f9e6e");  // esmeralda

        t.hoverStrong         = QStringLiteral("rgba(47,74,60,0.09)");
        t.borderStrong        = QStringLiteral("rgba(47,74,60,0.20)");
        t.focusBorder         = QStringLiteral("rgba(47,74,60,0.30)");
        t.inputBackground     = QStringLiteral("rgba(47,74,60,0.04)");
        t.disabledText        = QStringLiteral("rgba(47,74,60,0.30)");
        t.selectionRing       = QStringLiteral("#163026");

        t.accentSuccess           = QStringLiteral("#2f8a4f");
        t.accentSuccessSoft       = QStringLiteral("rgba(47,138,79,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(47,138,79,0.45)");
        t.accentDanger            = QStringLiteral("#b04a40");
        t.accentDangerSoft        = QStringLiteral("rgba(176,74,64,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(176,74,64,0.45)");
        t.accentWarning           = QStringLiteral("#c08a40");
        t.accentInfo              = QStringLiteral("#3a7390");
        t.accentInfoSoft          = QStringLiteral("rgba(58,115,144,0.15)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(58,115,144,0.50)");

        t.editorBackground = QStringLiteral("#eef7f0");
        t.editorTextColor  = QStringLiteral("#20382c");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(47,74,60,70)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 4;
        m_themes.append(t);
    }

    // ===================== CLAROS (neutros / frios) =====================
    {
        // Névoa — cinza frio e nebuloso. Neutro, calmo, manhã enevoada.
        MiraTheme t;
        t.id = QStringLiteral("nevoa");
        t.name = QStringLiteral("Névoa");
        t.bundled = true;
        t.appBackground = QStringLiteral("#e7eaee");
        t.panelBackground = QStringLiteral("#f1f3f6");
        t.panelBorder = QStringLiteral("#cdd3da");
        t.textPrimary = QStringLiteral("#46505c");
        t.textMuted = QStringLiteral("#7e8893");
        t.textBright = QStringLiteral("#232a33");
        t.hoverOverlay = QStringLiteral("rgba(50,60,72,0.05)");
        t.pressedOverlay = QStringLiteral("rgba(50,60,72,0.035)");
        t.subtleBorder = QStringLiteral("rgba(50,60,72,0.12)");
        t.accentDefault = QStringLiteral("#5a7a96");
        t.hoverStrong = QStringLiteral("rgba(50,60,72,0.09)");
        t.borderStrong = QStringLiteral("rgba(50,60,72,0.20)");
        t.focusBorder = QStringLiteral("rgba(50,60,72,0.30)");
        t.inputBackground = QStringLiteral("rgba(50,60,72,0.04)");
        t.disabledText = QStringLiteral("rgba(50,60,72,0.30)");
        t.selectionRing = QStringLiteral("#232a33");
        t.accentSuccess = QStringLiteral("#4a8a5e");
        t.accentSuccessSoft = QStringLiteral("rgba(74,138,94,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(74,138,94,0.45)");
        t.accentDanger = QStringLiteral("#b04a4a");
        t.accentDangerSoft = QStringLiteral("rgba(176,74,74,0.10)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(176,74,74,0.45)");
        t.accentWarning = QStringLiteral("#c08a40");
        t.accentInfo = QStringLiteral("#4a7a9a");
        t.accentInfoSoft = QStringLiteral("rgba(74,122,154,0.15)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(74,122,154,0.50)");
        t.editorBackground = QStringLiteral("#f1f3f6");
        t.editorTextColor = QStringLiteral("#2e353f");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(70,80,92,70)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 4;
        m_themes.append(t);
    }
    {
        // Gelo — azul-branco glacial. Frio cristalino, ar de inverno limpo.
        MiraTheme t;
        t.id = QStringLiteral("ice");
        t.name = QStringLiteral("Gelo");
        t.bundled = true;
        t.appBackground = QStringLiteral("#e4edf2");
        t.panelBackground = QStringLiteral("#f0f6fa");
        t.panelBorder = QStringLiteral("#c6d6e0");
        t.textPrimary = QStringLiteral("#38525e");
        t.textMuted = QStringLiteral("#6f8a98");
        t.textBright = QStringLiteral("#18303a");
        t.hoverOverlay = QStringLiteral("rgba(40,70,82,0.05)");
        t.pressedOverlay = QStringLiteral("rgba(40,70,82,0.035)");
        t.subtleBorder = QStringLiteral("rgba(40,70,82,0.12)");
        t.accentDefault = QStringLiteral("#2f93b8");
        t.hoverStrong = QStringLiteral("rgba(40,70,82,0.09)");
        t.borderStrong = QStringLiteral("rgba(40,70,82,0.20)");
        t.focusBorder = QStringLiteral("rgba(40,70,82,0.30)");
        t.inputBackground = QStringLiteral("rgba(40,70,82,0.04)");
        t.disabledText = QStringLiteral("rgba(40,70,82,0.30)");
        t.selectionRing = QStringLiteral("#18303a");
        t.accentSuccess = QStringLiteral("#2f9a7a");
        t.accentSuccessSoft = QStringLiteral("rgba(47,154,122,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(47,154,122,0.45)");
        t.accentDanger = QStringLiteral("#c0504e");
        t.accentDangerSoft = QStringLiteral("rgba(192,80,78,0.10)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(192,80,78,0.45)");
        t.accentWarning = QStringLiteral("#c89040");
        t.accentInfo = QStringLiteral("#2f93b8");
        t.accentInfoSoft = QStringLiteral("rgba(47,147,184,0.15)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(47,147,184,0.50)");
        t.editorBackground = QStringLiteral("#f0f6fa");
        t.editorTextColor = QStringLiteral("#243e48");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(56,82,94,70)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 4;
        m_themes.append(t);
    }
    {
        // Sálvia — verde-acinzentado pálido. Herbal, sereno, luz de jardim.
        MiraTheme t;
        t.id = QStringLiteral("sage");
        t.name = QStringLiteral("Sálvia");
        t.bundled = true;
        t.appBackground = QStringLiteral("#e6ece2");
        t.panelBackground = QStringLiteral("#f1f5ec");
        t.panelBorder = QStringLiteral("#cad6c0");
        t.textPrimary = QStringLiteral("#44503c");
        t.textMuted = QStringLiteral("#7d8a72");
        t.textBright = QStringLiteral("#232c1c");
        t.hoverOverlay = QStringLiteral("rgba(52,62,46,0.05)");
        t.pressedOverlay = QStringLiteral("rgba(52,62,46,0.035)");
        t.subtleBorder = QStringLiteral("rgba(52,62,46,0.12)");
        t.accentDefault = QStringLiteral("#6e9a5a");
        t.hoverStrong = QStringLiteral("rgba(52,62,46,0.09)");
        t.borderStrong = QStringLiteral("rgba(52,62,46,0.20)");
        t.focusBorder = QStringLiteral("rgba(52,62,46,0.30)");
        t.inputBackground = QStringLiteral("rgba(52,62,46,0.04)");
        t.disabledText = QStringLiteral("rgba(52,62,46,0.30)");
        t.selectionRing = QStringLiteral("#232c1c");
        t.accentSuccess = QStringLiteral("#5e9a4e");
        t.accentSuccessSoft = QStringLiteral("rgba(94,154,78,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,154,78,0.45)");
        t.accentDanger = QStringLiteral("#b05448");
        t.accentDangerSoft = QStringLiteral("rgba(176,84,72,0.10)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(176,84,72,0.45)");
        t.accentWarning = QStringLiteral("#b88a3c");
        t.accentInfo = QStringLiteral("#4a8088");
        t.accentInfoSoft = QStringLiteral("rgba(74,128,136,0.15)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(74,128,136,0.50)");
        t.editorBackground = QStringLiteral("#f1f5ec");
        t.editorTextColor = QStringLiteral("#2e3826");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(68,80,60,70)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 4;
        m_themes.append(t);
    }
    {
        // Quartzo — rosa-acinzentado pálido. Delicado, quartzo rosa lapidado.
        MiraTheme t;
        t.id = QStringLiteral("quartz");
        t.name = QStringLiteral("Quartzo");
        t.bundled = true;
        t.appBackground = QStringLiteral("#f0e9ec");
        t.panelBackground = QStringLiteral("#f8f3f5");
        t.panelBorder = QStringLiteral("#ddccd2");
        t.textPrimary = QStringLiteral("#5a4a50");
        t.textMuted = QStringLiteral("#99868f");
        t.textBright = QStringLiteral("#382a30");
        t.hoverOverlay = QStringLiteral("rgba(70,55,62,0.05)");
        t.pressedOverlay = QStringLiteral("rgba(70,55,62,0.035)");
        t.subtleBorder = QStringLiteral("rgba(70,55,62,0.12)");
        t.accentDefault = QStringLiteral("#b06a82");
        t.hoverStrong = QStringLiteral("rgba(70,55,62,0.09)");
        t.borderStrong = QStringLiteral("rgba(70,55,62,0.20)");
        t.focusBorder = QStringLiteral("rgba(70,55,62,0.30)");
        t.inputBackground = QStringLiteral("rgba(70,55,62,0.04)");
        t.disabledText = QStringLiteral("rgba(70,55,62,0.30)");
        t.selectionRing = QStringLiteral("#382a30");
        t.accentSuccess = QStringLiteral("#5e8a5e");
        t.accentSuccessSoft = QStringLiteral("rgba(94,138,94,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,138,94,0.45)");
        t.accentDanger = QStringLiteral("#b84a5a");
        t.accentDangerSoft = QStringLiteral("rgba(184,74,90,0.10)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(184,74,90,0.45)");
        t.accentWarning = QStringLiteral("#c08a48");
        t.accentInfo = QStringLiteral("#6a7aa0");
        t.accentInfoSoft = QStringLiteral("rgba(106,122,160,0.15)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(106,122,160,0.50)");
        t.editorBackground = QStringLiteral("#f8f3f5");
        t.editorTextColor = QStringLiteral("#423036");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(90,74,80,70)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 4;
        m_themes.append(t);
    }
    {
        // Pérola — neutro perolado refinado. Cinza-claro com brilho suave.
        MiraTheme t;
        t.id = QStringLiteral("pearl");
        t.name = QStringLiteral("Pérola");
        t.bundled = true;
        t.appBackground = QStringLiteral("#ecebe7");
        t.panelBackground = QStringLiteral("#f6f5f1");
        t.panelBorder = QStringLiteral("#d6d4cc");
        t.textPrimary = QStringLiteral("#4c4a44");
        t.textMuted = QStringLiteral("#8a8780");
        t.textBright = QStringLiteral("#2a2824");
        t.hoverOverlay = QStringLiteral("rgba(48,46,42,0.05)");
        t.pressedOverlay = QStringLiteral("rgba(48,46,42,0.035)");
        t.subtleBorder = QStringLiteral("rgba(48,46,42,0.12)");
        t.accentDefault = QStringLiteral("#7e8896");
        t.hoverStrong = QStringLiteral("rgba(48,46,42,0.09)");
        t.borderStrong = QStringLiteral("rgba(48,46,42,0.20)");
        t.focusBorder = QStringLiteral("rgba(48,46,42,0.30)");
        t.inputBackground = QStringLiteral("rgba(48,46,42,0.04)");
        t.disabledText = QStringLiteral("rgba(48,46,42,0.30)");
        t.selectionRing = QStringLiteral("#2a2824");
        t.accentSuccess = QStringLiteral("#5e8a52");
        t.accentSuccessSoft = QStringLiteral("rgba(94,138,82,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,138,82,0.45)");
        t.accentDanger = QStringLiteral("#b04a48");
        t.accentDangerSoft = QStringLiteral("rgba(176,74,72,0.10)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(176,74,72,0.45)");
        t.accentWarning = QStringLiteral("#bc8a3c");
        t.accentInfo = QStringLiteral("#5a7a96");
        t.accentInfoSoft = QStringLiteral("rgba(90,122,150,0.15)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(90,122,150,0.50)");
        t.editorBackground = QStringLiteral("#f6f5f1");
        t.editorTextColor = QStringLiteral("#36342e");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(76,74,68,68)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 4;
        m_themes.append(t);
    }

    // ===================== AMARELADOS (quentes) =====================
    {
        // Canela — marrom-especiaria escuro. Quente e picante, casca de canela.
        MiraTheme t;
        t.id = QStringLiteral("cinnamon");
        t.name = QStringLiteral("Canela");
        t.bundled = true;
        t.appBackground = QStringLiteral("#1e1410");
        t.panelBackground = QStringLiteral("#2a1c15");
        t.panelBorder = QStringLiteral("#402a1e");
        t.textPrimary = QStringLiteral("#e2c8b0");
        t.textMuted = QStringLiteral("#9a7860");
        t.textBright = QStringLiteral("#f4ddc8");
        t.hoverOverlay = QStringLiteral("rgba(226,200,176,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(226,200,176,0.04)");
        t.subtleBorder = QStringLiteral("rgba(226,200,176,0.10)");
        t.accentDefault = QStringLiteral("#c86a3a");
        t.hoverStrong = QStringLiteral("rgba(226,200,176,0.12)");
        t.borderStrong = QStringLiteral("rgba(226,200,176,0.22)");
        t.focusBorder = QStringLiteral("rgba(226,200,176,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText = QStringLiteral("rgba(226,200,176,0.30)");
        t.selectionRing = QStringLiteral("#f4ddc8");
        t.accentSuccess = QStringLiteral("#8a9e4e");
        t.accentSuccessSoft = QStringLiteral("rgba(138,158,78,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(138,158,78,0.50)");
        t.accentDanger = QStringLiteral("#d05540");
        t.accentDangerSoft = QStringLiteral("rgba(208,85,64,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(208,85,64,0.50)");
        t.accentWarning = QStringLiteral("#d8923c");
        t.accentInfo = QStringLiteral("#7090b0");
        t.accentInfoSoft = QStringLiteral("rgba(112,144,176,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(112,144,176,0.55)");
        t.editorBackground = QStringLiteral("#261810");
        t.editorTextColor = QStringLiteral("#ecd4bc");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,185)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Mostarda — creme dijon claro. Amarelo terroso, acolhedor.
        MiraTheme t;
        t.id = QStringLiteral("mustard");
        t.name = QStringLiteral("Mostarda");
        t.bundled = true;
        t.appBackground = QStringLiteral("#ece2bc");
        t.panelBackground = QStringLiteral("#f5efd2");
        t.panelBorder = QStringLiteral("#ccbd84");
        t.textPrimary = QStringLiteral("#565024");
        t.textMuted = QStringLiteral("#8d8246");
        t.textBright = QStringLiteral("#2e2a0e");
        t.hoverOverlay = QStringLiteral("rgba(86,80,36,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(86,80,36,0.04)");
        t.subtleBorder = QStringLiteral("rgba(86,80,36,0.14)");
        t.accentDefault = QStringLiteral("#b8902a");
        t.hoverStrong = QStringLiteral("rgba(86,80,36,0.10)");
        t.borderStrong = QStringLiteral("rgba(86,80,36,0.24)");
        t.focusBorder = QStringLiteral("rgba(86,80,36,0.34)");
        t.inputBackground = QStringLiteral("rgba(86,80,36,0.05)");
        t.disabledText = QStringLiteral("rgba(86,80,36,0.32)");
        t.selectionRing = QStringLiteral("#2e2a0e");
        t.accentSuccess = QStringLiteral("#5e7a3a");
        t.accentSuccessSoft = QStringLiteral("rgba(94,122,58,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,122,58,0.45)");
        t.accentDanger = QStringLiteral("#a83a33");
        t.accentDangerSoft = QStringLiteral("rgba(168,58,51,0.10)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(168,58,51,0.45)");
        t.accentWarning = QStringLiteral("#b8902a");
        t.accentInfo = QStringLiteral("#3a6b8a");
        t.accentInfoSoft = QStringLiteral("rgba(58,107,138,0.16)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(58,107,138,0.55)");
        t.editorBackground = QStringLiteral("#f5efd2");
        t.editorTextColor = QStringLiteral("#393413");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(86,80,36,90)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Terracota — barro cozido claro. Laranja-tijolo terroso, mediterrâneo.
        MiraTheme t;
        t.id = QStringLiteral("terracotta");
        t.name = QStringLiteral("Terracota");
        t.bundled = true;
        t.appBackground = QStringLiteral("#ecd9cc");
        t.panelBackground = QStringLiteral("#f5e8dd");
        t.panelBorder = QStringLiteral("#ceb09c");
        t.textPrimary = QStringLiteral("#5a3e30");
        t.textMuted = QStringLiteral("#936c58");
        t.textBright = QStringLiteral("#321f14");
        t.hoverOverlay = QStringLiteral("rgba(90,62,48,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(90,62,48,0.04)");
        t.subtleBorder = QStringLiteral("rgba(90,62,48,0.14)");
        t.accentDefault = QStringLiteral("#c05a3a");
        t.hoverStrong = QStringLiteral("rgba(90,62,48,0.10)");
        t.borderStrong = QStringLiteral("rgba(90,62,48,0.24)");
        t.focusBorder = QStringLiteral("rgba(90,62,48,0.34)");
        t.inputBackground = QStringLiteral("rgba(90,62,48,0.05)");
        t.disabledText = QStringLiteral("rgba(90,62,48,0.32)");
        t.selectionRing = QStringLiteral("#321f14");
        t.accentSuccess = QStringLiteral("#5e7a44");
        t.accentSuccessSoft = QStringLiteral("rgba(94,122,68,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,122,68,0.45)");
        t.accentDanger = QStringLiteral("#b03a30");
        t.accentDangerSoft = QStringLiteral("rgba(176,58,48,0.10)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(176,58,48,0.45)");
        t.accentWarning = QStringLiteral("#c87a3a");
        t.accentInfo = QStringLiteral("#4a6b88");
        t.accentInfoSoft = QStringLiteral("rgba(74,107,136,0.16)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(74,107,136,0.55)");
        t.editorBackground = QStringLiteral("#f5e8dd");
        t.editorTextColor = QStringLiteral("#3c281c");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(90,62,48,90)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Damasco — pêssego suave claro. Quente e macio, luz de amanhecer.
        MiraTheme t;
        t.id = QStringLiteral("apricot");
        t.name = QStringLiteral("Damasco");
        t.bundled = true;
        t.appBackground = QStringLiteral("#f2e2d2");
        t.panelBackground = QStringLiteral("#f9eee2");
        t.panelBorder = QStringLiteral("#ddc4ac");
        t.textPrimary = QStringLiteral("#5e4636");
        t.textMuted = QStringLiteral("#9a7860");
        t.textBright = QStringLiteral("#3a2818");
        t.hoverOverlay = QStringLiteral("rgba(94,70,54,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(94,70,54,0.04)");
        t.subtleBorder = QStringLiteral("rgba(94,70,54,0.13)");
        t.accentDefault = QStringLiteral("#e08a5a");
        t.hoverStrong = QStringLiteral("rgba(94,70,54,0.10)");
        t.borderStrong = QStringLiteral("rgba(94,70,54,0.22)");
        t.focusBorder = QStringLiteral("rgba(94,70,54,0.32)");
        t.inputBackground = QStringLiteral("rgba(94,70,54,0.05)");
        t.disabledText = QStringLiteral("rgba(94,70,54,0.31)");
        t.selectionRing = QStringLiteral("#3a2818");
        t.accentSuccess = QStringLiteral("#6e9a52");
        t.accentSuccessSoft = QStringLiteral("rgba(110,154,82,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(110,154,82,0.45)");
        t.accentDanger = QStringLiteral("#c0544a");
        t.accentDangerSoft = QStringLiteral("rgba(192,84,74,0.10)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(192,84,74,0.45)");
        t.accentWarning = QStringLiteral("#d88a48");
        t.accentInfo = QStringLiteral("#5a82a0");
        t.accentInfoSoft = QStringLiteral("rgba(90,130,160,0.15)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(90,130,160,0.50)");
        t.editorBackground = QStringLiteral("#f9eee2");
        t.editorTextColor = QStringLiteral("#422f20");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(94,70,54,80)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Conhaque — âmbar-licor escuro e encorpado. Profundo, aveludado.
        MiraTheme t;
        t.id = QStringLiteral("cognac");
        t.name = QStringLiteral("Conhaque");
        t.bundled = true;
        t.appBackground = QStringLiteral("#1c1208");
        t.panelBackground = QStringLiteral("#28190d");
        t.panelBorder = QStringLiteral("#3e2a14");
        t.textPrimary = QStringLiteral("#e0cba6");
        t.textMuted = QStringLiteral("#9a7c50");
        t.textBright = QStringLiteral("#f4e3c2");
        t.hoverOverlay = QStringLiteral("rgba(224,203,166,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(224,203,166,0.04)");
        t.subtleBorder = QStringLiteral("rgba(224,203,166,0.10)");
        t.accentDefault = QStringLiteral("#d08a3c");
        t.hoverStrong = QStringLiteral("rgba(224,203,166,0.12)");
        t.borderStrong = QStringLiteral("rgba(224,203,166,0.22)");
        t.focusBorder = QStringLiteral("rgba(224,203,166,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(224,203,166,0.30)");
        t.selectionRing = QStringLiteral("#f4e3c2");
        t.accentSuccess = QStringLiteral("#8a9e4e");
        t.accentSuccessSoft = QStringLiteral("rgba(138,158,78,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(138,158,78,0.50)");
        t.accentDanger = QStringLiteral("#c85540");
        t.accentDangerSoft = QStringLiteral("rgba(200,85,64,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(200,85,64,0.50)");
        t.accentWarning = QStringLiteral("#d8923c");
        t.accentInfo = QStringLiteral("#7090b0");
        t.accentInfoSoft = QStringLiteral("rgba(112,144,176,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(112,144,176,0.55)");
        t.editorBackground = QStringLiteral("#241608");
        t.editorTextColor = QStringLiteral("#ecd8b2");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,190)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }

    // ===================== ESCUROS (neutros) =====================
    {
        // Carvão — carvão neutro puro. Cinza-grafite sem viés de cor.
        MiraTheme t;
        t.id = QStringLiteral("charcoal");
        t.name = QStringLiteral("Carvão");
        t.bundled = true;
        t.appBackground = QStringLiteral("#151515");
        t.panelBackground = QStringLiteral("#1e1e1e");
        t.panelBorder = QStringLiteral("#323232");
        t.textPrimary = QStringLiteral("#cccccc");
        t.textMuted = QStringLiteral("#808080");
        t.textBright = QStringLiteral("#f0f0f0");
        t.hoverOverlay = QStringLiteral("rgba(204,204,204,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(204,204,204,0.04)");
        t.subtleBorder = QStringLiteral("rgba(204,204,204,0.10)");
        t.accentDefault = QStringLiteral("#6f8aa0");
        t.hoverStrong = QStringLiteral("rgba(204,204,204,0.12)");
        t.borderStrong = QStringLiteral("rgba(204,204,204,0.22)");
        t.focusBorder = QStringLiteral("rgba(204,204,204,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText = QStringLiteral("rgba(204,204,204,0.30)");
        t.selectionRing = QStringLiteral("#f0f0f0");
        t.accentSuccess = QStringLiteral("#6fa85e");
        t.accentSuccessSoft = QStringLiteral("rgba(111,168,94,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(111,168,94,0.50)");
        t.accentDanger = QStringLiteral("#c85a5a");
        t.accentDangerSoft = QStringLiteral("rgba(200,90,90,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(200,90,90,0.50)");
        t.accentWarning = QStringLiteral("#c89548");
        t.accentInfo = QStringLiteral("#6f8aa0");
        t.accentInfoSoft = QStringLiteral("rgba(111,138,160,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(111,138,160,0.55)");
        t.editorBackground = QStringLiteral("#1a1a1a");
        t.editorTextColor = QStringLiteral("#d8d8d8");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,195)");
        t.pageShadowRadius = 28;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Ardósia — cinza-azulado de ardósia. Frio, sóbrio, pedra polida.
        MiraTheme t;
        t.id = QStringLiteral("slate");
        t.name = QStringLiteral("Ardósia");
        t.bundled = true;
        t.appBackground = QStringLiteral("#12161c");
        t.panelBackground = QStringLiteral("#1a2029");
        t.panelBorder = QStringLiteral("#2c3542");
        t.textPrimary = QStringLiteral("#c0c8d2");
        t.textMuted = QStringLiteral("#74808e");
        t.textBright = QStringLiteral("#e8edf3");
        t.hoverOverlay = QStringLiteral("rgba(192,200,210,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(192,200,210,0.04)");
        t.subtleBorder = QStringLiteral("rgba(192,200,210,0.10)");
        t.accentDefault = QStringLiteral("#5a7da0");
        t.hoverStrong = QStringLiteral("rgba(192,200,210,0.12)");
        t.borderStrong = QStringLiteral("rgba(192,200,210,0.22)");
        t.focusBorder = QStringLiteral("rgba(192,200,210,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText = QStringLiteral("rgba(192,200,210,0.30)");
        t.selectionRing = QStringLiteral("#e8edf3");
        t.accentSuccess = QStringLiteral("#5fa080");
        t.accentSuccessSoft = QStringLiteral("rgba(95,160,128,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(95,160,128,0.50)");
        t.accentDanger = QStringLiteral("#c85a64");
        t.accentDangerSoft = QStringLiteral("rgba(200,90,100,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(200,90,100,0.50)");
        t.accentWarning = QStringLiteral("#c89548");
        t.accentInfo = QStringLiteral("#5a7da0");
        t.accentInfoSoft = QStringLiteral("rgba(90,125,160,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(90,125,160,0.55)");
        t.editorBackground = QStringLiteral("#161c24");
        t.editorTextColor = QStringLiteral("#d2dae2");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,195)");
        t.pageShadowRadius = 28;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Ônix — preto-quase-total com brilho frio. Profundo e elegante.
        MiraTheme t;
        t.id = QStringLiteral("onyx");
        t.name = QStringLiteral("Ônix");
        t.bundled = true;
        t.appBackground = QStringLiteral("#0c0d10");
        t.panelBackground = QStringLiteral("#14161b");
        t.panelBorder = QStringLiteral("#262a32");
        t.textPrimary = QStringLiteral("#c4cad4");
        t.textMuted = QStringLiteral("#6e7682");
        t.textBright = QStringLiteral("#eef1f6");
        t.hoverOverlay = QStringLiteral("rgba(196,202,212,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(196,202,212,0.04)");
        t.subtleBorder = QStringLiteral("rgba(196,202,212,0.10)");
        t.accentDefault = QStringLiteral("#8a9ac0");
        t.hoverStrong = QStringLiteral("rgba(196,202,212,0.12)");
        t.borderStrong = QStringLiteral("rgba(196,202,212,0.22)");
        t.focusBorder = QStringLiteral("rgba(196,202,212,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(196,202,212,0.30)");
        t.selectionRing = QStringLiteral("#eef1f6");
        t.accentSuccess = QStringLiteral("#5fa088");
        t.accentSuccessSoft = QStringLiteral("rgba(95,160,136,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(95,160,136,0.50)");
        t.accentDanger = QStringLiteral("#c85a64");
        t.accentDangerSoft = QStringLiteral("rgba(200,90,100,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(200,90,100,0.50)");
        t.accentWarning = QStringLiteral("#c09048");
        t.accentInfo = QStringLiteral("#8a9ac0");
        t.accentInfoSoft = QStringLiteral("rgba(138,154,192,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(138,154,192,0.55)");
        t.editorBackground = QStringLiteral("#101216");
        t.editorTextColor = QStringLiteral("#d6dce4");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,210)");
        t.pageShadowRadius = 30;
        t.pageShadowOffset = 7;
        m_themes.append(t);
    }
    {
        // Chumbo — cinza-metal médio. Industrial, gunmetal sóbrio.
        MiraTheme t;
        t.id = QStringLiteral("lead");
        t.name = QStringLiteral("Chumbo");
        t.bundled = true;
        t.appBackground = QStringLiteral("#1c1e22");
        t.panelBackground = QStringLiteral("#25282e");
        t.panelBorder = QStringLiteral("#383c44");
        t.textPrimary = QStringLiteral("#c6cad0");
        t.textMuted = QStringLiteral("#80858d");
        t.textBright = QStringLiteral("#eceef2");
        t.hoverOverlay = QStringLiteral("rgba(198,202,208,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(198,202,208,0.04)");
        t.subtleBorder = QStringLiteral("rgba(198,202,208,0.10)");
        t.accentDefault = QStringLiteral("#8a98a8");
        t.hoverStrong = QStringLiteral("rgba(198,202,208,0.12)");
        t.borderStrong = QStringLiteral("rgba(198,202,208,0.22)");
        t.focusBorder = QStringLiteral("rgba(198,202,208,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.26)");
        t.disabledText = QStringLiteral("rgba(198,202,208,0.30)");
        t.selectionRing = QStringLiteral("#eceef2");
        t.accentSuccess = QStringLiteral("#6fa05e");
        t.accentSuccessSoft = QStringLiteral("rgba(111,160,94,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(111,160,94,0.50)");
        t.accentDanger = QStringLiteral("#c85a5a");
        t.accentDangerSoft = QStringLiteral("rgba(200,90,90,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(200,90,90,0.50)");
        t.accentWarning = QStringLiteral("#c49348");
        t.accentInfo = QStringLiteral("#6f8898");
        t.accentInfoSoft = QStringLiteral("rgba(111,136,152,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(111,136,152,0.55)");
        t.editorBackground = QStringLiteral("#212429");
        t.editorTextColor = QStringLiteral("#d8dade");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,185)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Meia-Noite — azul-noturno quase neutro. Profundidade silenciosa.
        MiraTheme t;
        t.id = QStringLiteral("midnight");
        t.name = QStringLiteral("Meia-Noite");
        t.bundled = true;
        t.appBackground = QStringLiteral("#0a0c14");
        t.panelBackground = QStringLiteral("#11141f");
        t.panelBorder = QStringLiteral("#20263a");
        t.textPrimary = QStringLiteral("#bcc4d6");
        t.textMuted = QStringLiteral("#6a7488");
        t.textBright = QStringLiteral("#e6ecf6");
        t.hoverOverlay = QStringLiteral("rgba(188,196,214,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(188,196,214,0.04)");
        t.subtleBorder = QStringLiteral("rgba(188,196,214,0.10)");
        t.accentDefault = QStringLiteral("#5a6ea8");
        t.hoverStrong = QStringLiteral("rgba(188,196,214,0.12)");
        t.borderStrong = QStringLiteral("rgba(188,196,214,0.22)");
        t.focusBorder = QStringLiteral("rgba(188,196,214,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(188,196,214,0.30)");
        t.selectionRing = QStringLiteral("#e6ecf6");
        t.accentSuccess = QStringLiteral("#5f9a86");
        t.accentSuccessSoft = QStringLiteral("rgba(95,154,134,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(95,154,134,0.50)");
        t.accentDanger = QStringLiteral("#c05a6a");
        t.accentDangerSoft = QStringLiteral("rgba(192,90,106,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(192,90,106,0.50)");
        t.accentWarning = QStringLiteral("#c08e48");
        t.accentInfo = QStringLiteral("#5a6ea8");
        t.accentInfoSoft = QStringLiteral("rgba(90,110,168,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(90,110,168,0.55)");
        t.editorBackground = QStringLiteral("#0e1018");
        t.editorTextColor = QStringLiteral("#ccd4e2");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,205)");
        t.pageShadowRadius = 28;
        t.pageShadowOffset = 7;
        m_themes.append(t);
    }

    // ===================== COLORIDOS (vibrantes) =====================
    {
        // Fúcsia — magenta-rosa vibrante no escuro. Ousado, elétrico.
        MiraTheme t;
        t.id = QStringLiteral("fuchsia");
        t.name = QStringLiteral("Fúcsia");
        t.bundled = true;
        t.appBackground = QStringLiteral("#1a0f18");
        t.panelBackground = QStringLiteral("#261426");
        t.panelBorder = QStringLiteral("#3d2240");
        t.textPrimary = QStringLiteral("#e6cae0");
        t.textMuted = QStringLiteral("#a074a0");
        t.textBright = QStringLiteral("#f8e6f4");
        t.hoverOverlay = QStringLiteral("rgba(230,202,224,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(230,202,224,0.04)");
        t.subtleBorder = QStringLiteral("rgba(230,202,224,0.10)");
        t.accentDefault = QStringLiteral("#e0479e");
        t.hoverStrong = QStringLiteral("rgba(230,202,224,0.12)");
        t.borderStrong = QStringLiteral("rgba(230,202,224,0.22)");
        t.focusBorder = QStringLiteral("rgba(230,202,224,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText = QStringLiteral("rgba(230,202,224,0.30)");
        t.selectionRing = QStringLiteral("#f8e6f4");
        t.accentSuccess = QStringLiteral("#6fb88a");
        t.accentSuccessSoft = QStringLiteral("rgba(111,184,138,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(111,184,138,0.50)");
        t.accentDanger = QStringLiteral("#e0556a");
        t.accentDangerSoft = QStringLiteral("rgba(224,85,106,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(224,85,106,0.50)");
        t.accentWarning = QStringLiteral("#e0a23c");
        t.accentInfo = QStringLiteral("#9a6ee0");
        t.accentInfoSoft = QStringLiteral("rgba(154,110,224,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(154,110,224,0.55)");
        t.editorBackground = QStringLiteral("#200f1e");
        t.editorTextColor = QStringLiteral("#eed4e8");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,190)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Vulcão — lava laranja-vermelha no escuro. Calor intenso, ímpeto.
        MiraTheme t;
        t.id = QStringLiteral("volcano");
        t.name = QStringLiteral("Vulcão");
        t.bundled = true;
        t.appBackground = QStringLiteral("#170c08");
        t.panelBackground = QStringLiteral("#24130c");
        t.panelBorder = QStringLiteral("#3d2014");
        t.textPrimary = QStringLiteral("#eccab0");
        t.textMuted = QStringLiteral("#a07658");
        t.textBright = QStringLiteral("#fbe2cc");
        t.hoverOverlay = QStringLiteral("rgba(236,202,176,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(236,202,176,0.04)");
        t.subtleBorder = QStringLiteral("rgba(236,202,176,0.10)");
        t.accentDefault = QStringLiteral("#f0602a");
        t.hoverStrong = QStringLiteral("rgba(236,202,176,0.12)");
        t.borderStrong = QStringLiteral("rgba(236,202,176,0.22)");
        t.focusBorder = QStringLiteral("rgba(236,202,176,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(236,202,176,0.30)");
        t.selectionRing = QStringLiteral("#fbe2cc");
        t.accentSuccess = QStringLiteral("#9aae44");
        t.accentSuccessSoft = QStringLiteral("rgba(154,174,68,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(154,174,68,0.50)");
        t.accentDanger = QStringLiteral("#e84838");
        t.accentDangerSoft = QStringLiteral("rgba(232,72,56,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(232,72,56,0.50)");
        t.accentWarning = QStringLiteral("#f09030");
        t.accentInfo = QStringLiteral("#7088b0");
        t.accentInfoSoft = QStringLiteral("rgba(112,136,176,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(112,136,176,0.55)");
        t.editorBackground = QStringLiteral("#1f1009");
        t.editorTextColor = QStringLiteral("#f0d6bc");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,195)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Turquesa — teal vivo no escuro. Tropical noturno, vibrante e fresco.
        MiraTheme t;
        t.id = QStringLiteral("turquoise");
        t.name = QStringLiteral("Turquesa");
        t.bundled = true;
        t.appBackground = QStringLiteral("#07181a");
        t.panelBackground = QStringLiteral("#0d2528");
        t.panelBorder = QStringLiteral("#16403e");
        t.textPrimary = QStringLiteral("#b8dcd8");
        t.textMuted = QStringLiteral("#5f8a86");
        t.textBright = QStringLiteral("#e0f6f2");
        t.hoverOverlay = QStringLiteral("rgba(184,220,216,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(184,220,216,0.04)");
        t.subtleBorder = QStringLiteral("rgba(184,220,216,0.10)");
        t.accentDefault = QStringLiteral("#1fc0b0");
        t.hoverStrong = QStringLiteral("rgba(184,220,216,0.12)");
        t.borderStrong = QStringLiteral("rgba(184,220,216,0.22)");
        t.focusBorder = QStringLiteral("rgba(184,220,216,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText = QStringLiteral("rgba(184,220,216,0.30)");
        t.selectionRing = QStringLiteral("#e0f6f2");
        t.accentSuccess = QStringLiteral("#3fc090");
        t.accentSuccessSoft = QStringLiteral("rgba(63,192,144,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(63,192,144,0.50)");
        t.accentDanger = QStringLiteral("#e0606a");
        t.accentDangerSoft = QStringLiteral("rgba(224,96,106,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(224,96,106,0.50)");
        t.accentWarning = QStringLiteral("#e0aa3c");
        t.accentInfo = QStringLiteral("#2f9ec0");
        t.accentInfoSoft = QStringLiteral("rgba(47,158,192,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(47,158,192,0.55)");
        t.editorBackground = QStringLiteral("#0a2023");
        t.editorTextColor = QStringLiteral("#cceeea");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,190)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Neon — verde-limão néon sobre quase-preto. Cyberpunk, energia crua.
        MiraTheme t;
        t.id = QStringLiteral("neon");
        t.name = QStringLiteral("Neon");
        t.bundled = true;
        t.appBackground = QStringLiteral("#0b0e08");
        t.panelBackground = QStringLiteral("#14180e");
        t.panelBorder = QStringLiteral("#283318");
        t.textPrimary = QStringLiteral("#cdd8b8");
        t.textMuted = QStringLiteral("#7a8a5e");
        t.textBright = QStringLiteral("#ecf6d8");
        t.hoverOverlay = QStringLiteral("rgba(205,216,184,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(205,216,184,0.04)");
        t.subtleBorder = QStringLiteral("rgba(205,216,184,0.10)");
        t.accentDefault = QStringLiteral("#9ee03c");
        t.hoverStrong = QStringLiteral("rgba(205,216,184,0.12)");
        t.borderStrong = QStringLiteral("rgba(205,216,184,0.22)");
        t.focusBorder = QStringLiteral("rgba(205,216,184,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(205,216,184,0.30)");
        t.selectionRing = QStringLiteral("#ecf6d8");
        t.accentSuccess = QStringLiteral("#7ce04a");
        t.accentSuccessSoft = QStringLiteral("rgba(124,224,74,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(124,224,74,0.50)");
        t.accentDanger = QStringLiteral("#e85870");
        t.accentDangerSoft = QStringLiteral("rgba(232,88,112,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(232,88,112,0.50)");
        t.accentWarning = QStringLiteral("#e0c83c");
        t.accentInfo = QStringLiteral("#4ac0e0");
        t.accentInfoSoft = QStringLiteral("rgba(74,192,224,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(74,192,224,0.55)");
        t.editorBackground = QStringLiteral("#101409");
        t.editorTextColor = QStringLiteral("#dce8c4");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,195)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }
    {
        // Borgonha — vinho tinto encorpado. Rico, dramático, veludo escuro.
        MiraTheme t;
        t.id = QStringLiteral("burgundy");
        t.name = QStringLiteral("Borgonha");
        t.bundled = true;
        t.appBackground = QStringLiteral("#1a0c10");
        t.panelBackground = QStringLiteral("#260f16");
        t.panelBorder = QStringLiteral("#3e1a24");
        t.textPrimary = QStringLiteral("#e6c2cc");
        t.textMuted = QStringLiteral("#a06e7a");
        t.textBright = QStringLiteral("#f8e0e6");
        t.hoverOverlay = QStringLiteral("rgba(230,194,204,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(230,194,204,0.04)");
        t.subtleBorder = QStringLiteral("rgba(230,194,204,0.10)");
        t.accentDefault = QStringLiteral("#c0395a");
        t.hoverStrong = QStringLiteral("rgba(230,194,204,0.12)");
        t.borderStrong = QStringLiteral("rgba(230,194,204,0.22)");
        t.focusBorder = QStringLiteral("rgba(230,194,204,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(230,194,204,0.30)");
        t.selectionRing = QStringLiteral("#f8e0e6");
        t.accentSuccess = QStringLiteral("#8aae5e");
        t.accentSuccessSoft = QStringLiteral("rgba(138,174,94,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(138,174,94,0.50)");
        t.accentDanger = QStringLiteral("#e0506a");
        t.accentDangerSoft = QStringLiteral("rgba(224,80,106,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(224,80,106,0.50)");
        t.accentWarning = QStringLiteral("#e0a23c");
        t.accentInfo = QStringLiteral("#9a6ec0");
        t.accentInfoSoft = QStringLiteral("rgba(154,110,192,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(154,110,192,0.55)");
        t.editorBackground = QStringLiteral("#200d13");
        t.editorTextColor = QStringLiteral("#eed2da");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,195)");
        t.pageShadowRadius = 26;
        t.pageShadowOffset = 6;
        m_themes.append(t);
    }

    // ===================== EXPERIMENTAIS (brincando com sombra e tom) =======
    {
        // Tinta — preto sobre papel, puro e clássico. Sombra dura e curta, como
        // uma folha real pousada na mesa. Cores contidas, tipográficas.
        MiraTheme t;
        t.id = QStringLiteral("ink");
        t.name = QStringLiteral("Tinta");
        t.bundled = true;
        t.appBackground = QStringLiteral("#f4f3ef");
        t.panelBackground = QStringLiteral("#fbfaf7");
        t.panelBorder = QStringLiteral("#d4d2ca");
        t.textPrimary = QStringLiteral("#1a1a18");
        t.textMuted = QStringLiteral("#6a6a64");
        t.textBright = QStringLiteral("#000000");
        t.hoverOverlay = QStringLiteral("rgba(20,20,18,0.05)");
        t.pressedOverlay = QStringLiteral("rgba(20,20,18,0.035)");
        t.subtleBorder = QStringLiteral("rgba(20,20,18,0.12)");
        t.accentDefault = QStringLiteral("#2a2a28");
        t.hoverStrong = QStringLiteral("rgba(20,20,18,0.10)");
        t.borderStrong = QStringLiteral("rgba(20,20,18,0.22)");
        t.focusBorder = QStringLiteral("rgba(20,20,18,0.34)");
        t.inputBackground = QStringLiteral("rgba(20,20,18,0.04)");
        t.disabledText = QStringLiteral("rgba(20,20,18,0.34)");
        t.selectionRing = QStringLiteral("#000000");
        t.accentSuccess = QStringLiteral("#2f7a44");
        t.accentSuccessSoft = QStringLiteral("rgba(47,122,68,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(47,122,68,0.45)");
        t.accentDanger = QStringLiteral("#a82828");
        t.accentDangerSoft = QStringLiteral("rgba(168,40,40,0.10)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(168,40,40,0.45)");
        t.accentWarning = QStringLiteral("#9a6a1a");
        t.accentInfo = QStringLiteral("#2a4a8a");
        t.accentInfoSoft = QStringLiteral("rgba(42,74,138,0.14)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(42,74,138,0.50)");
        t.editorBackground = QStringLiteral("#fbfaf7");
        t.editorTextColor = QStringLiteral("#121210");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,150)");
        t.pageShadowRadius = 14;
        t.pageShadowOffset = 7;
        m_themes.append(t);
    }
    {
        // Nanquim — branco sobre preto absoluto. Mono escuro, nanquim invertido.
        MiraTheme t;
        t.id = QStringLiteral("sumi");
        t.name = QStringLiteral("Nanquim");
        t.bundled = true;
        t.appBackground = QStringLiteral("#0a0a0a");
        t.panelBackground = QStringLiteral("#141414");
        t.panelBorder = QStringLiteral("#2a2a2a");
        t.textPrimary = QStringLiteral("#e6e6e6");
        t.textMuted = QStringLiteral("#7a7a7a");
        t.textBright = QStringLiteral("#ffffff");
        t.hoverOverlay = QStringLiteral("rgba(230,230,230,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(230,230,230,0.04)");
        t.subtleBorder = QStringLiteral("rgba(230,230,230,0.10)");
        t.accentDefault = QStringLiteral("#6a727c");
        t.hoverStrong = QStringLiteral("rgba(230,230,230,0.12)");
        t.borderStrong = QStringLiteral("rgba(230,230,230,0.22)");
        t.focusBorder = QStringLiteral("rgba(230,230,230,0.34)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(230,230,230,0.30)");
        t.selectionRing = QStringLiteral("#ffffff");
        t.accentSuccess = QStringLiteral("#6fae5e");
        t.accentSuccessSoft = QStringLiteral("rgba(111,174,94,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(111,174,94,0.50)");
        t.accentDanger = QStringLiteral("#d05555");
        t.accentDangerSoft = QStringLiteral("rgba(208,85,85,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(208,85,85,0.50)");
        t.accentWarning = QStringLiteral("#c89548");
        t.accentInfo = QStringLiteral("#8aa0c0");
        t.accentInfoSoft = QStringLiteral("rgba(138,160,192,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(138,160,192,0.55)");
        t.editorBackground = QStringLiteral("#101010");
        t.editorTextColor = QStringLiteral("#ededed");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,220)");
        t.pageShadowRadius = 30;
        t.pageShadowOffset = 7;
        m_themes.append(t);
    }
    {
        // Vaga-lume — escuro com a página acesa por um halo verde-limão.
        // A sombra vira brilho (offset 0, raio grande, cor viva).
        MiraTheme t;
        t.id = QStringLiteral("firefly");
        t.name = QStringLiteral("Vaga-lume");
        t.bundled = true;
        t.appBackground = QStringLiteral("#0c0f0a");
        t.panelBackground = QStringLiteral("#141a10");
        t.panelBorder = QStringLiteral("#243018");
        t.textPrimary = QStringLiteral("#d2dcc0");
        t.textMuted = QStringLiteral("#7a8a64");
        t.textBright = QStringLiteral("#eef6dc");
        t.hoverOverlay = QStringLiteral("rgba(210,220,192,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(210,220,192,0.04)");
        t.subtleBorder = QStringLiteral("rgba(210,220,192,0.10)");
        t.accentDefault = QStringLiteral("#b6e84a");
        t.hoverStrong = QStringLiteral("rgba(210,220,192,0.12)");
        t.borderStrong = QStringLiteral("rgba(210,220,192,0.22)");
        t.focusBorder = QStringLiteral("rgba(210,220,192,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText = QStringLiteral("rgba(210,220,192,0.30)");
        t.selectionRing = QStringLiteral("#eef6dc");
        t.accentSuccess = QStringLiteral("#8ce04a");
        t.accentSuccessSoft = QStringLiteral("rgba(140,224,74,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(140,224,74,0.50)");
        t.accentDanger = QStringLiteral("#e06a5a");
        t.accentDangerSoft = QStringLiteral("rgba(224,106,90,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(224,106,90,0.50)");
        t.accentWarning = QStringLiteral("#e0c040");
        t.accentInfo = QStringLiteral("#5ab0d0");
        t.accentInfoSoft = QStringLiteral("rgba(90,176,208,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(90,176,208,0.55)");
        t.editorBackground = QStringLiteral("#101409");
        t.editorTextColor = QStringLiteral("#dce8c4");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(150,224,90,110)");
        t.pageShadowRadius = 42;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }
    {
        // Halo — escuro azul-noite com a página envolta num brilho ciano.
        MiraTheme t;
        t.id = QStringLiteral("halo");
        t.name = QStringLiteral("Halo");
        t.bundled = true;
        t.appBackground = QStringLiteral("#0a0e16");
        t.panelBackground = QStringLiteral("#111726");
        t.panelBorder = QStringLiteral("#1e2a44");
        t.textPrimary = QStringLiteral("#c2cee0");
        t.textMuted = QStringLiteral("#6e7e96");
        t.textBright = QStringLiteral("#e8effa");
        t.hoverOverlay = QStringLiteral("rgba(194,206,224,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(194,206,224,0.04)");
        t.subtleBorder = QStringLiteral("rgba(194,206,224,0.10)");
        t.accentDefault = QStringLiteral("#4aa6ff");
        t.hoverStrong = QStringLiteral("rgba(194,206,224,0.12)");
        t.borderStrong = QStringLiteral("rgba(194,206,224,0.22)");
        t.focusBorder = QStringLiteral("rgba(194,206,224,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(194,206,224,0.30)");
        t.selectionRing = QStringLiteral("#e8effa");
        t.accentSuccess = QStringLiteral("#3fae8f");
        t.accentSuccessSoft = QStringLiteral("rgba(63,174,143,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(63,174,143,0.50)");
        t.accentDanger = QStringLiteral("#e0606a");
        t.accentDangerSoft = QStringLiteral("rgba(224,96,106,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(224,96,106,0.50)");
        t.accentWarning = QStringLiteral("#e0a23c");
        t.accentInfo = QStringLiteral("#4aa6ff");
        t.accentInfoSoft = QStringLiteral("rgba(74,166,255,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(74,166,255,0.55)");
        t.editorBackground = QStringLiteral("#0e1422");
        t.editorTextColor = QStringLiteral("#d4e0f0");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(74,166,255,120)");
        t.pageShadowRadius = 46;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }
    {
        // Sakura — branco-rosado de cerejeira. Sombra rosa macia sob o papel.
        MiraTheme t;
        t.id = QStringLiteral("sakura");
        t.name = QStringLiteral("Sakura");
        t.bundled = true;
        t.appBackground = QStringLiteral("#f6e6ec");
        t.panelBackground = QStringLiteral("#fcf1f4");
        t.panelBorder = QStringLiteral("#e6c8d2");
        t.textPrimary = QStringLiteral("#5a3a46");
        t.textMuted = QStringLiteral("#a07682");
        t.textBright = QStringLiteral("#3a2028");
        t.hoverOverlay = QStringLiteral("rgba(90,58,70,0.05)");
        t.pressedOverlay = QStringLiteral("rgba(90,58,70,0.035)");
        t.subtleBorder = QStringLiteral("rgba(90,58,70,0.12)");
        t.accentDefault = QStringLiteral("#e06a92");
        t.hoverStrong = QStringLiteral("rgba(90,58,70,0.09)");
        t.borderStrong = QStringLiteral("rgba(90,58,70,0.20)");
        t.focusBorder = QStringLiteral("rgba(90,58,70,0.30)");
        t.inputBackground = QStringLiteral("rgba(90,58,70,0.04)");
        t.disabledText = QStringLiteral("rgba(90,58,70,0.30)");
        t.selectionRing = QStringLiteral("#3a2028");
        t.accentSuccess = QStringLiteral("#5e8a5e");
        t.accentSuccessSoft = QStringLiteral("rgba(94,138,94,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,138,94,0.45)");
        t.accentDanger = QStringLiteral("#c84a5a");
        t.accentDangerSoft = QStringLiteral("rgba(200,74,90,0.10)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(200,74,90,0.45)");
        t.accentWarning = QStringLiteral("#c88a48");
        t.accentInfo = QStringLiteral("#6a7aa8");
        t.accentInfoSoft = QStringLiteral("rgba(106,122,168,0.15)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(106,122,168,0.50)");
        t.editorBackground = QStringLiteral("#fcf1f4");
        t.editorTextColor = QStringLiteral("#422832");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(224,106,146,90)");
        t.pageShadowRadius = 30;
        t.pageShadowOffset = 5;
        m_themes.append(t);
    }
    {
        // Cobre — metal quente escuro. Sombra cobreada densa, brilho de forja.
        MiraTheme t;
        t.id = QStringLiteral("copper");
        t.name = QStringLiteral("Cobre");
        t.bundled = true;
        t.appBackground = QStringLiteral("#161009");
        t.panelBackground = QStringLiteral("#221710");
        t.panelBorder = QStringLiteral("#3a2818");
        t.textPrimary = QStringLiteral("#e6c8a8");
        t.textMuted = QStringLiteral("#a07c58");
        t.textBright = QStringLiteral("#f6dcb8");
        t.hoverOverlay = QStringLiteral("rgba(230,200,168,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(230,200,168,0.04)");
        t.subtleBorder = QStringLiteral("rgba(230,200,168,0.10)");
        t.accentDefault = QStringLiteral("#d07a3c");
        t.hoverStrong = QStringLiteral("rgba(230,200,168,0.12)");
        t.borderStrong = QStringLiteral("rgba(230,200,168,0.22)");
        t.focusBorder = QStringLiteral("rgba(230,200,168,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(230,200,168,0.30)");
        t.selectionRing = QStringLiteral("#f6dcb8");
        t.accentSuccess = QStringLiteral("#8a9e4e");
        t.accentSuccessSoft = QStringLiteral("rgba(138,158,78,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(138,158,78,0.50)");
        t.accentDanger = QStringLiteral("#cc5440");
        t.accentDangerSoft = QStringLiteral("rgba(204,84,64,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(204,84,64,0.50)");
        t.accentWarning = QStringLiteral("#d8923c");
        t.accentInfo = QStringLiteral("#7090b0");
        t.accentInfoSoft = QStringLiteral("rgba(112,144,176,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(112,144,176,0.55)");
        t.editorBackground = QStringLiteral("#1e140b");
        t.editorTextColor = QStringLiteral("#ecd4b0");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(120,70,30,150)");
        t.pageShadowRadius = 30;
        t.pageShadowOffset = 7;
        m_themes.append(t);
    }
    {
        // Menta — verde-água claro e refrescante. Limpo, hortelã ao amanhecer.
        MiraTheme t;
        t.id = QStringLiteral("mint");
        t.name = QStringLiteral("Menta");
        t.bundled = true;
        t.appBackground = QStringLiteral("#e0f0e8");
        t.panelBackground = QStringLiteral("#eef8f2");
        t.panelBorder = QStringLiteral("#c2dccc");
        t.textPrimary = QStringLiteral("#2e4a40");
        t.textMuted = QStringLiteral("#6e8a7e");
        t.textBright = QStringLiteral("#163028");
        t.hoverOverlay = QStringLiteral("rgba(46,74,64,0.05)");
        t.pressedOverlay = QStringLiteral("rgba(46,74,64,0.035)");
        t.subtleBorder = QStringLiteral("rgba(46,74,64,0.12)");
        t.accentDefault = QStringLiteral("#20b89a");
        t.hoverStrong = QStringLiteral("rgba(46,74,64,0.09)");
        t.borderStrong = QStringLiteral("rgba(46,74,64,0.20)");
        t.focusBorder = QStringLiteral("rgba(46,74,64,0.30)");
        t.inputBackground = QStringLiteral("rgba(46,74,64,0.04)");
        t.disabledText = QStringLiteral("rgba(46,74,64,0.30)");
        t.selectionRing = QStringLiteral("#163028");
        t.accentSuccess = QStringLiteral("#2f9a5e");
        t.accentSuccessSoft = QStringLiteral("rgba(47,154,94,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(47,154,94,0.45)");
        t.accentDanger = QStringLiteral("#b04a48");
        t.accentDangerSoft = QStringLiteral("rgba(176,74,72,0.10)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(176,74,72,0.45)");
        t.accentWarning = QStringLiteral("#c08a40");
        t.accentInfo = QStringLiteral("#3a8a90");
        t.accentInfoSoft = QStringLiteral("rgba(58,138,144,0.15)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(58,138,144,0.50)");
        t.editorBackground = QStringLiteral("#eef8f2");
        t.editorTextColor = QStringLiteral("#203a30");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(46,84,70,70)");
        t.pageShadowRadius = 24;
        t.pageShadowOffset = 4;
        m_themes.append(t);
    }
    {
        // Eclipse — preto absoluto com a página coroada por um anel âmbar.
        // Drama total: a sombra é a corona do eclipse.
        MiraTheme t;
        t.id = QStringLiteral("eclipse");
        t.name = QStringLiteral("Eclipse");
        t.bundled = true;
        t.appBackground = QStringLiteral("#060606");
        t.panelBackground = QStringLiteral("#0f0e0c");
        t.panelBorder = QStringLiteral("#2a241a");
        t.textPrimary = QStringLiteral("#d8d2c4");
        t.textMuted = QStringLiteral("#807a6a");
        t.textBright = QStringLiteral("#f4efe2");
        t.hoverOverlay = QStringLiteral("rgba(216,210,196,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(216,210,196,0.04)");
        t.subtleBorder = QStringLiteral("rgba(216,210,196,0.10)");
        t.accentDefault = QStringLiteral("#f0a830");
        t.hoverStrong = QStringLiteral("rgba(216,210,196,0.12)");
        t.borderStrong = QStringLiteral("rgba(216,210,196,0.22)");
        t.focusBorder = QStringLiteral("rgba(216,210,196,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.34)");
        t.disabledText = QStringLiteral("rgba(216,210,196,0.30)");
        t.selectionRing = QStringLiteral("#f4efe2");
        t.accentSuccess = QStringLiteral("#8aae5e");
        t.accentSuccessSoft = QStringLiteral("rgba(138,174,94,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(138,174,94,0.50)");
        t.accentDanger = QStringLiteral("#d05548");
        t.accentDangerSoft = QStringLiteral("rgba(208,85,72,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(208,85,72,0.50)");
        t.accentWarning = QStringLiteral("#f0a830");
        t.accentInfo = QStringLiteral("#7090b0");
        t.accentInfoSoft = QStringLiteral("rgba(112,144,176,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(112,144,176,0.55)");
        t.editorBackground = QStringLiteral("#0c0b09");
        t.editorTextColor = QStringLiteral("#e4ddcc");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(240,168,48,90)");
        t.pageShadowRadius = 40;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }

    // ===================== FORA DA CAIXA =====================
    {
        // Noite Escura — quase preto, mas a página flutua num halo cinza-prata
        // de luar. Frio, silencioso, lua cheia sobre o papel.
        MiraTheme t;
        t.id = QStringLiteral("deep-night");
        t.name = QStringLiteral("Noite Escura");
        t.bundled = true;
        t.appBackground = QStringLiteral("#08090d");
        t.panelBackground = QStringLiteral("#101218");
        t.panelBorder = QStringLiteral("#20242e");
        t.textPrimary = QStringLiteral("#b8c0cc");
        t.textMuted = QStringLiteral("#686f7c");
        t.textBright = QStringLiteral("#e4e8f0");
        t.hoverOverlay = QStringLiteral("rgba(184,192,204,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(184,192,204,0.04)");
        t.subtleBorder = QStringLiteral("rgba(184,192,204,0.10)");
        t.accentDefault = QStringLiteral("#8a93a6");
        t.hoverStrong = QStringLiteral("rgba(184,192,204,0.12)");
        t.borderStrong = QStringLiteral("rgba(184,192,204,0.22)");
        t.focusBorder = QStringLiteral("rgba(184,192,204,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.32)");
        t.disabledText = QStringLiteral("rgba(184,192,204,0.30)");
        t.selectionRing = QStringLiteral("#e4e8f0");
        t.accentSuccess = QStringLiteral("#5f9a86");
        t.accentSuccessSoft = QStringLiteral("rgba(95,154,134,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(95,154,134,0.50)");
        t.accentDanger = QStringLiteral("#c05a64");
        t.accentDangerSoft = QStringLiteral("rgba(192,90,100,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(192,90,100,0.50)");
        t.accentWarning = QStringLiteral("#c08e48");
        t.accentInfo = QStringLiteral("#7a8aa6");
        t.accentInfoSoft = QStringLiteral("rgba(122,138,166,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(122,138,166,0.55)");
        t.editorBackground = QStringLiteral("#0c0e14");
        t.editorTextColor = QStringLiteral("#cdd4e0");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(200,212,235,100)");
        t.pageShadowRadius = 46;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }
    {
        // Palco — app preto absoluto, mas a página é BRANCA, iluminada como um
        // holofote no escuro do teatro. Chrome escuro + folha clara.
        MiraTheme t;
        t.id = QStringLiteral("stage");
        t.name = QStringLiteral("Palco");
        t.bundled = true;
        t.appBackground = QStringLiteral("#000000");
        t.panelBackground = QStringLiteral("#0c0c0c");
        t.panelBorder = QStringLiteral("#242424");
        t.textPrimary = QStringLiteral("#d0d0d0");
        t.textMuted = QStringLiteral("#767676");
        t.textBright = QStringLiteral("#f4f4f4");
        t.hoverOverlay = QStringLiteral("rgba(208,208,208,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(208,208,208,0.04)");
        t.subtleBorder = QStringLiteral("rgba(208,208,208,0.10)");
        t.accentDefault = QStringLiteral("#e0b050");
        t.hoverStrong = QStringLiteral("rgba(208,208,208,0.12)");
        t.borderStrong = QStringLiteral("rgba(208,208,208,0.22)");
        t.focusBorder = QStringLiteral("rgba(208,208,208,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.40)");
        t.disabledText = QStringLiteral("rgba(208,208,208,0.30)");
        t.selectionRing = QStringLiteral("#f4f4f4");
        t.accentSuccess = QStringLiteral("#6fae5e");
        t.accentSuccessSoft = QStringLiteral("rgba(111,174,94,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(111,174,94,0.50)");
        t.accentDanger = QStringLiteral("#d05555");
        t.accentDangerSoft = QStringLiteral("rgba(208,85,85,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(208,85,85,0.50)");
        t.accentWarning = QStringLiteral("#e0b050");
        t.accentInfo = QStringLiteral("#7a90b8");
        t.accentInfoSoft = QStringLiteral("rgba(122,144,184,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(122,144,184,0.55)");
        t.editorBackground = QStringLiteral("#ffffff");
        t.editorTextColor = QStringLiteral("#161616");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(255,255,255,70)");
        t.pageShadowRadius = 40;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }
    {
        // Avesso — o oposto do Palco: app claro e arejado, mas a página é PRETA
        // com texto claro. A folha vira um retângulo de breu sobre a mesa clara.
        MiraTheme t;
        t.id = QStringLiteral("inverso");
        t.name = QStringLiteral("Avesso");
        t.bundled = true;
        t.appBackground = QStringLiteral("#eceae4");
        t.panelBackground = QStringLiteral("#f6f4ee");
        t.panelBorder = QStringLiteral("#d2cfc6");
        t.textPrimary = QStringLiteral("#44423c");
        t.textMuted = QStringLiteral("#86837a");
        t.textBright = QStringLiteral("#221f1a");
        t.hoverOverlay = QStringLiteral("rgba(50,48,42,0.05)");
        t.pressedOverlay = QStringLiteral("rgba(50,48,42,0.035)");
        t.subtleBorder = QStringLiteral("rgba(50,48,42,0.12)");
        t.accentDefault = QStringLiteral("#c0512a");
        t.hoverStrong = QStringLiteral("rgba(50,48,42,0.09)");
        t.borderStrong = QStringLiteral("rgba(50,48,42,0.20)");
        t.focusBorder = QStringLiteral("rgba(50,48,42,0.30)");
        t.inputBackground = QStringLiteral("rgba(50,48,42,0.04)");
        t.disabledText = QStringLiteral("rgba(50,48,42,0.30)");
        t.selectionRing = QStringLiteral("#221f1a");
        t.accentSuccess = QStringLiteral("#4a8a52");
        t.accentSuccessSoft = QStringLiteral("rgba(74,138,82,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(74,138,82,0.45)");
        t.accentDanger = QStringLiteral("#b04438");
        t.accentDangerSoft = QStringLiteral("rgba(176,68,56,0.10)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(176,68,56,0.45)");
        t.accentWarning = QStringLiteral("#b8842c");
        t.accentInfo = QStringLiteral("#4a6e96");
        t.accentInfoSoft = QStringLiteral("rgba(74,110,150,0.15)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(74,110,150,0.50)");
        t.editorBackground = QStringLiteral("#14140f");
        t.editorTextColor = QStringLiteral("#e6e2d6");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(40,38,30,130)");
        t.pageShadowRadius = 28;
        t.pageShadowOffset = 7;
        m_themes.append(t);
    }
    {
        // Anáglifo — 3D retrô. Vermelho e ciano em tensão sobre o quase-preto,
        // a página com halo ciano que "desencaixa" do accent vermelho.
        MiraTheme t;
        t.id = QStringLiteral("anaglyph");
        t.name = QStringLiteral("Anáglifo");
        t.bundled = true;
        t.appBackground = QStringLiteral("#0a0a0c");
        t.panelBackground = QStringLiteral("#141318");
        t.panelBorder = QStringLiteral("#2c2030");
        t.textPrimary = QStringLiteral("#d6cdd2");
        t.textMuted = QStringLiteral("#807888");
        t.textBright = QStringLiteral("#f2eaf0");
        t.hoverOverlay = QStringLiteral("rgba(214,205,210,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(214,205,210,0.04)");
        t.subtleBorder = QStringLiteral("rgba(214,205,210,0.10)");
        t.accentDefault = QStringLiteral("#ff4858");
        t.hoverStrong = QStringLiteral("rgba(214,205,210,0.12)");
        t.borderStrong = QStringLiteral("rgba(214,205,210,0.22)");
        t.focusBorder = QStringLiteral("rgba(214,205,210,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(214,205,210,0.30)");
        t.selectionRing = QStringLiteral("#f2eaf0");
        t.accentSuccess = QStringLiteral("#4ad0c0");
        t.accentSuccessSoft = QStringLiteral("rgba(74,208,192,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(74,208,192,0.50)");
        t.accentDanger = QStringLiteral("#ff4858");
        t.accentDangerSoft = QStringLiteral("rgba(255,72,88,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(255,72,88,0.50)");
        t.accentWarning = QStringLiteral("#e0a23c");
        t.accentInfo = QStringLiteral("#46d6e6");
        t.accentInfoSoft = QStringLiteral("rgba(70,214,230,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(70,214,230,0.55)");
        t.editorBackground = QStringLiteral("#100f13");
        t.editorTextColor = QStringLiteral("#e2dae0");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(70,214,230,120)");
        t.pageShadowRadius = 38;
        t.pageShadowOffset = 4;
        m_themes.append(t);
    }
    {
        // Voltagem — campo azul-elétrico cortado por vermelho em brasa. Choque
        // de complementares, a página pulsa num brilho azul.
        MiraTheme t;
        t.id = QStringLiteral("voltage");
        t.name = QStringLiteral("Voltagem");
        t.bundled = true;
        t.appBackground = QStringLiteral("#08102a");
        t.panelBackground = QStringLiteral("#0e1838");
        t.panelBorder = QStringLiteral("#1c2c5a");
        t.textPrimary = QStringLiteral("#c2cce6");
        t.textMuted = QStringLiteral("#6a7aa0");
        t.textBright = QStringLiteral("#e6ecfa");
        t.hoverOverlay = QStringLiteral("rgba(194,204,230,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(194,204,230,0.04)");
        t.subtleBorder = QStringLiteral("rgba(194,204,230,0.10)");
        t.accentDefault = QStringLiteral("#ff3a52");
        t.hoverStrong = QStringLiteral("rgba(194,204,230,0.12)");
        t.borderStrong = QStringLiteral("rgba(194,204,230,0.22)");
        t.focusBorder = QStringLiteral("rgba(194,204,230,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(194,204,230,0.30)");
        t.selectionRing = QStringLiteral("#e6ecfa");
        t.accentSuccess = QStringLiteral("#3fb088");
        t.accentSuccessSoft = QStringLiteral("rgba(63,176,136,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(63,176,136,0.50)");
        t.accentDanger = QStringLiteral("#ff3a52");
        t.accentDangerSoft = QStringLiteral("rgba(255,58,82,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(255,58,82,0.50)");
        t.accentWarning = QStringLiteral("#e0a23c");
        t.accentInfo = QStringLiteral("#3a7aff");
        t.accentInfoSoft = QStringLiteral("rgba(58,122,255,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(58,122,255,0.55)");
        t.editorBackground = QStringLiteral("#0a1430");
        t.editorTextColor = QStringLiteral("#d4dcf0");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(58,122,255,120)");
        t.pageShadowRadius = 40;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }
    {
        // Outrun — synthwave anos 80. Roxo profundo, magenta e ciano, a página
        // sob um pôr-do-sol magenta. Neon retrô.
        MiraTheme t;
        t.id = QStringLiteral("outrun");
        t.name = QStringLiteral("Outrun");
        t.bundled = true;
        t.appBackground = QStringLiteral("#120a22");
        t.panelBackground = QStringLiteral("#1c1036");
        t.panelBorder = QStringLiteral("#341e5a");
        t.textPrimary = QStringLiteral("#ddd0ee");
        t.textMuted = QStringLiteral("#8a78b0");
        t.textBright = QStringLiteral("#f4e8ff");
        t.hoverOverlay = QStringLiteral("rgba(221,208,238,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(221,208,238,0.04)");
        t.subtleBorder = QStringLiteral("rgba(221,208,238,0.10)");
        t.accentDefault = QStringLiteral("#ff3ca0");
        t.hoverStrong = QStringLiteral("rgba(221,208,238,0.12)");
        t.borderStrong = QStringLiteral("rgba(221,208,238,0.22)");
        t.focusBorder = QStringLiteral("rgba(221,208,238,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(221,208,238,0.30)");
        t.selectionRing = QStringLiteral("#f4e8ff");
        t.accentSuccess = QStringLiteral("#3ce0c0");
        t.accentSuccessSoft = QStringLiteral("rgba(60,224,192,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(60,224,192,0.50)");
        t.accentDanger = QStringLiteral("#ff5a78");
        t.accentDangerSoft = QStringLiteral("rgba(255,90,120,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(255,90,120,0.50)");
        t.accentWarning = QStringLiteral("#ffb43c");
        t.accentInfo = QStringLiteral("#46c8ff");
        t.accentInfoSoft = QStringLiteral("rgba(70,200,255,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(70,200,255,0.55)");
        t.editorBackground = QStringLiteral("#160c28");
        t.editorTextColor = QStringLiteral("#e6d8f4");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(255,60,160,120)");
        t.pageShadowRadius = 44;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }
    {
        // Fósforo — monitor CRT verde. Tudo é fósforo verde sobre o breu, a
        // página brilha como uma tela ligada. Terminal retrô puro.
        MiraTheme t;
        t.id = QStringLiteral("phosphor");
        t.name = QStringLiteral("Terminal");
        t.bundled = true;
        t.appBackground = QStringLiteral("#020602");
        t.panelBackground = QStringLiteral("#07120a");
        t.panelBorder = QStringLiteral("#123a1c");
        t.textPrimary = QStringLiteral("#44dc6a");
        t.textMuted = QStringLiteral("#2a8a44");
        t.textBright = QStringLiteral("#7cff9a");
        t.hoverOverlay = QStringLiteral("rgba(68,220,106,0.07)");
        t.pressedOverlay = QStringLiteral("rgba(68,220,106,0.05)");
        t.subtleBorder = QStringLiteral("rgba(68,220,106,0.12)");
        t.accentDefault = QStringLiteral("#33ff66");
        t.hoverStrong = QStringLiteral("rgba(68,220,106,0.14)");
        t.borderStrong = QStringLiteral("rgba(68,220,106,0.24)");
        t.focusBorder = QStringLiteral("rgba(68,220,106,0.34)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.40)");
        t.disabledText = QStringLiteral("rgba(68,220,106,0.32)");
        t.selectionRing = QStringLiteral("#7cff9a");
        t.accentSuccess = QStringLiteral("#33ff66");
        t.accentSuccessSoft = QStringLiteral("rgba(51,255,102,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(51,255,102,0.50)");
        t.accentDanger = QStringLiteral("#ff5a5a");
        t.accentDangerSoft = QStringLiteral("rgba(255,90,90,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(255,90,90,0.50)");
        t.accentWarning = QStringLiteral("#d8e040");
        t.accentInfo = QStringLiteral("#40e0c0");
        t.accentInfoSoft = QStringLiteral("rgba(64,224,192,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(64,224,192,0.55)");
        t.editorBackground = QStringLiteral("#040a05");
        t.editorTextColor = QStringLiteral("#4ce070");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(60,255,110,110)");
        t.pageShadowRadius = 40;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }
    {
        // Fósforo Âmbar — o irmão do Fósforo: CRT âmbar. Laranja-mel sobre o
        // breu, brilho de tela de aeroporto antigo.
        MiraTheme t;
        t.id = QStringLiteral("phosphor-amber");
        t.name = QStringLiteral("Terminal Âmbar");
        t.bundled = true;
        t.appBackground = QStringLiteral("#060402");
        t.panelBackground = QStringLiteral("#14100a");
        t.panelBorder = QStringLiteral("#3a2c12");
        t.textPrimary = QStringLiteral("#f0a830");
        t.textMuted = QStringLiteral("#9a6c20");
        t.textBright = QStringLiteral("#ffc850");
        t.hoverOverlay = QStringLiteral("rgba(240,168,48,0.07)");
        t.pressedOverlay = QStringLiteral("rgba(240,168,48,0.05)");
        t.subtleBorder = QStringLiteral("rgba(240,168,48,0.12)");
        t.accentDefault = QStringLiteral("#ffb030");
        t.hoverStrong = QStringLiteral("rgba(240,168,48,0.14)");
        t.borderStrong = QStringLiteral("rgba(240,168,48,0.24)");
        t.focusBorder = QStringLiteral("rgba(240,168,48,0.34)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.40)");
        t.disabledText = QStringLiteral("rgba(240,168,48,0.32)");
        t.selectionRing = QStringLiteral("#ffc850");
        t.accentSuccess = QStringLiteral("#b0d040");
        t.accentSuccessSoft = QStringLiteral("rgba(176,208,64,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(176,208,64,0.50)");
        t.accentDanger = QStringLiteral("#ff6a4a");
        t.accentDangerSoft = QStringLiteral("rgba(255,106,74,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(255,106,74,0.50)");
        t.accentWarning = QStringLiteral("#ffb030");
        t.accentInfo = QStringLiteral("#d09040");
        t.accentInfoSoft = QStringLiteral("rgba(208,144,64,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(208,144,64,0.55)");
        t.editorBackground = QStringLiteral("#0c0904");
        t.editorTextColor = QStringLiteral("#f0a830");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(255,176,48,105)");
        t.pageShadowRadius = 40;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }

    // ============ FUNDO COLORIDO (UI vibrante + página escura + halo) ========
    {
        // Sirene — homenagem ao FocusWriter do Pe. Chrome azul-elétrico, página
        // preta flutuando num halo azul, texto vermelho-sangue. Tensão de luz
        // de viatura: vermelho e azul que não deixam você dormir.
        MiraTheme t;
        t.id = QStringLiteral("siren");
        t.name = QStringLiteral("Sirene");
        t.bundled = true;
        t.appBackground = QStringLiteral("#16278f");
        t.panelBackground = QStringLiteral("#101d6e");
        t.panelBorder = QStringLiteral("#2840b0");
        t.textPrimary = QStringLiteral("#ccd2f4");
        t.textMuted = QStringLiteral("#8088c0");
        t.textBright = QStringLiteral("#f0f2ff");
        t.hoverOverlay = QStringLiteral("rgba(204,210,244,0.07)");
        t.pressedOverlay = QStringLiteral("rgba(204,210,244,0.05)");
        t.subtleBorder = QStringLiteral("rgba(204,210,244,0.12)");
        t.accentDefault = QStringLiteral("#f03a44");
        t.hoverStrong = QStringLiteral("rgba(204,210,244,0.14)");
        t.borderStrong = QStringLiteral("rgba(204,210,244,0.24)");
        t.focusBorder = QStringLiteral("rgba(204,210,244,0.34)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(204,210,244,0.32)");
        t.selectionRing = QStringLiteral("#f0f2ff");
        t.accentSuccess = QStringLiteral("#46c08a");
        t.accentSuccessSoft = QStringLiteral("rgba(70,192,138,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(70,192,138,0.50)");
        t.accentDanger = QStringLiteral("#f03a44");
        t.accentDangerSoft = QStringLiteral("rgba(240,58,68,0.16)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(240,58,68,0.55)");
        t.accentWarning = QStringLiteral("#e0a83c");
        t.accentInfo = QStringLiteral("#4a8aff");
        t.accentInfoSoft = QStringLiteral("rgba(74,138,255,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(74,138,255,0.55)");
        t.editorBackground = QStringLiteral("#08080e");
        t.editorTextColor = QStringLiteral("#c83a3a");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(50,90,255,170)");
        t.pageShadowRadius = 52;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }
    {
        // Brasa — chrome vermelho-sangue, página preta no halo escarlate, texto
        // dourado de brasa viva. Forja, perigo, calor que arde.
        MiraTheme t;
        t.id = QStringLiteral("ember-red");
        t.name = QStringLiteral("Brasa");
        t.bundled = true;
        t.appBackground = QStringLiteral("#6e1418");
        t.panelBackground = QStringLiteral("#500e12");
        t.panelBorder = QStringLiteral("#8a2424");
        t.textPrimary = QStringLiteral("#f0c8a8");
        t.textMuted = QStringLiteral("#b07868");
        t.textBright = QStringLiteral("#ffe0c0");
        t.hoverOverlay = QStringLiteral("rgba(240,200,168,0.07)");
        t.pressedOverlay = QStringLiteral("rgba(240,200,168,0.05)");
        t.subtleBorder = QStringLiteral("rgba(240,200,168,0.12)");
        t.accentDefault = QStringLiteral("#f0a830");
        t.hoverStrong = QStringLiteral("rgba(240,200,168,0.14)");
        t.borderStrong = QStringLiteral("rgba(240,200,168,0.24)");
        t.focusBorder = QStringLiteral("rgba(240,200,168,0.34)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.32)");
        t.disabledText = QStringLiteral("rgba(240,200,168,0.32)");
        t.selectionRing = QStringLiteral("#ffe0c0");
        t.accentSuccess = QStringLiteral("#9aae44");
        t.accentSuccessSoft = QStringLiteral("rgba(154,174,68,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(154,174,68,0.50)");
        t.accentDanger = QStringLiteral("#ff5a4a");
        t.accentDangerSoft = QStringLiteral("rgba(255,90,74,0.16)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(255,90,74,0.55)");
        t.accentWarning = QStringLiteral("#f0a830");
        t.accentInfo = QStringLiteral("#c88a90");
        t.accentInfoSoft = QStringLiteral("rgba(200,138,144,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(200,138,144,0.55)");
        t.editorBackground = QStringLiteral("#0c0606");
        t.editorTextColor = QStringLiteral("#d8a648");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(230,40,40,150)");
        t.pageShadowRadius = 48;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }
    {
        // Radioativo — chrome verde-tóxico, página preta no halo radioativo,
        // texto verde-limão. Reator, laboratório, perigo biológico.
        MiraTheme t;
        t.id = QStringLiteral("radioactive");
        t.name = QStringLiteral("Radioativo");
        t.bundled = true;
        t.appBackground = QStringLiteral("#1e4a12");
        t.panelBackground = QStringLiteral("#143408");
        t.panelBorder = QStringLiteral("#2e5a18");
        t.textPrimary = QStringLiteral("#c8e0a0");
        t.textMuted = QStringLiteral("#84a060");
        t.textBright = QStringLiteral("#e6ffc0");
        t.hoverOverlay = QStringLiteral("rgba(200,224,160,0.07)");
        t.pressedOverlay = QStringLiteral("rgba(200,224,160,0.05)");
        t.subtleBorder = QStringLiteral("rgba(200,224,160,0.12)");
        t.accentDefault = QStringLiteral("#aaff33");
        t.hoverStrong = QStringLiteral("rgba(200,224,160,0.14)");
        t.borderStrong = QStringLiteral("rgba(200,224,160,0.24)");
        t.focusBorder = QStringLiteral("rgba(200,224,160,0.34)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.32)");
        t.disabledText = QStringLiteral("rgba(200,224,160,0.32)");
        t.selectionRing = QStringLiteral("#e6ffc0");
        t.accentSuccess = QStringLiteral("#7ce04a");
        t.accentSuccessSoft = QStringLiteral("rgba(124,224,74,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(124,224,74,0.50)");
        t.accentDanger = QStringLiteral("#ff5a5a");
        t.accentDangerSoft = QStringLiteral("rgba(255,90,90,0.16)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(255,90,90,0.55)");
        t.accentWarning = QStringLiteral("#e0d040");
        t.accentInfo = QStringLiteral("#4ac0a0");
        t.accentInfoSoft = QStringLiteral("rgba(74,192,160,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(74,192,160,0.55)");
        t.editorBackground = QStringLiteral("#060a04");
        t.editorTextColor = QStringLiteral("#9ae04a");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(140,255,40,150)");
        t.pageShadowRadius = 50;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }
    {
        // Realeza — chrome roxo-real, página preta no halo violeta, texto
        // dourado. Manto de rei, ópera, opulência noturna.
        MiraTheme t;
        t.id = QStringLiteral("royalty");
        t.name = QStringLiteral("Realeza");
        t.bundled = true;
        t.appBackground = QStringLiteral("#3a1470");
        t.panelBackground = QStringLiteral("#2a0e54");
        t.panelBorder = QStringLiteral("#4e2090");
        t.textPrimary = QStringLiteral("#ddc8f0");
        t.textMuted = QStringLiteral("#9a7ec0");
        t.textBright = QStringLiteral("#f4e8ff");
        t.hoverOverlay = QStringLiteral("rgba(221,200,240,0.07)");
        t.pressedOverlay = QStringLiteral("rgba(221,200,240,0.05)");
        t.subtleBorder = QStringLiteral("rgba(221,200,240,0.12)");
        t.accentDefault = QStringLiteral("#e0b040");
        t.hoverStrong = QStringLiteral("rgba(221,200,240,0.14)");
        t.borderStrong = QStringLiteral("rgba(221,200,240,0.24)");
        t.focusBorder = QStringLiteral("rgba(221,200,240,0.34)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(221,200,240,0.32)");
        t.selectionRing = QStringLiteral("#f4e8ff");
        t.accentSuccess = QStringLiteral("#6fc08a");
        t.accentSuccessSoft = QStringLiteral("rgba(111,192,138,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(111,192,138,0.50)");
        t.accentDanger = QStringLiteral("#e0566a");
        t.accentDangerSoft = QStringLiteral("rgba(224,86,106,0.16)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(224,86,106,0.55)");
        t.accentWarning = QStringLiteral("#e0b040");
        t.accentInfo = QStringLiteral("#8a6ee0");
        t.accentInfoSoft = QStringLiteral("rgba(138,110,224,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(138,110,224,0.55)");
        t.editorBackground = QStringLiteral("#0a0710");
        t.editorTextColor = QStringLiteral("#d8b850");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(150,80,255,160)");
        t.pageShadowRadius = 50;
        t.pageShadowOffset = 0;
        m_themes.append(t);
    }

    // ============ MESAS (imagem de fundo: a página pousa na madeira) =========
    {
        // Escrivaninha — a mesa clássica: nogueira média e quente, papel creme
        // pousado por cima. A madeira aparece nas margens, ao redor da folha.
        MiraTheme t;
        t.id = QStringLiteral("desk-oak");
        t.name = QStringLiteral("Escrivaninha");
        t.bundled = true;
        t.appBackground = QStringLiteral("#241810");
        t.panelBackground = QStringLiteral("#2e1f13");
        t.panelBorder = QStringLiteral("#4a3320");
        t.textPrimary = QStringLiteral("#e0cba8");
        t.textMuted = QStringLiteral("#a8855f");
        t.textBright = QStringLiteral("#f6e6c8");
        t.hoverOverlay = QStringLiteral("rgba(224,203,168,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(224,203,168,0.04)");
        t.subtleBorder = QStringLiteral("rgba(224,203,168,0.10)");
        t.accentDefault = QStringLiteral("#d09850");
        t.hoverStrong = QStringLiteral("rgba(224,203,168,0.12)");
        t.borderStrong = QStringLiteral("rgba(224,203,168,0.22)");
        t.focusBorder = QStringLiteral("rgba(224,203,168,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(224,203,168,0.30)");
        t.selectionRing = QStringLiteral("#f6e6c8");
        t.accentSuccess = QStringLiteral("#8aa84e");
        t.accentSuccessSoft = QStringLiteral("rgba(138,168,78,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(138,168,78,0.50)");
        t.accentDanger = QStringLiteral("#c85540");
        t.accentDangerSoft = QStringLiteral("rgba(200,85,64,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(200,85,64,0.50)");
        t.accentWarning = QStringLiteral("#d8923c");
        t.accentInfo = QStringLiteral("#7090b0");
        t.accentInfoSoft = QStringLiteral("rgba(112,144,176,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(112,144,176,0.55)");
        t.editorBackground = QStringLiteral("#f2e8d0");
        t.editorTextColor = QStringLiteral("#3a2c1a");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,200)");
        t.pageShadowRadius = 30;
        t.pageShadowOffset = 8;
        t.backgroundImage = bundledImagePath(QStringLiteral("wood-2.jpg"));
        t.backgroundMode = BgZoom;
        t.editorOpacity = 100;
        m_themes.append(t);
    }
    {
        // Mogno — mesa nobre de nogueira avermelhada, papel pergaminho quente.
        MiraTheme t;
        t.id = QStringLiteral("desk-mahogany");
        t.name = QStringLiteral("Mogno");
        t.bundled = true;
        t.appBackground = QStringLiteral("#1e1410");
        t.panelBackground = QStringLiteral("#281a14");
        t.panelBorder = QStringLiteral("#432a1e");
        t.textPrimary = QStringLiteral("#e2c8b4");
        t.textMuted = QStringLiteral("#a87e68");
        t.textBright = QStringLiteral("#f4e0cc");
        t.hoverOverlay = QStringLiteral("rgba(226,200,180,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(226,200,180,0.04)");
        t.subtleBorder = QStringLiteral("rgba(226,200,180,0.10)");
        t.accentDefault = QStringLiteral("#c87a4a");
        t.hoverStrong = QStringLiteral("rgba(226,200,180,0.12)");
        t.borderStrong = QStringLiteral("rgba(226,200,180,0.22)");
        t.focusBorder = QStringLiteral("rgba(226,200,180,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText = QStringLiteral("rgba(226,200,180,0.30)");
        t.selectionRing = QStringLiteral("#f4e0cc");
        t.accentSuccess = QStringLiteral("#8aa84e");
        t.accentSuccessSoft = QStringLiteral("rgba(138,168,78,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(138,168,78,0.50)");
        t.accentDanger = QStringLiteral("#c85040");
        t.accentDangerSoft = QStringLiteral("rgba(200,80,64,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(200,80,64,0.50)");
        t.accentWarning = QStringLiteral("#d8923c");
        t.accentInfo = QStringLiteral("#7090b0");
        t.accentInfoSoft = QStringLiteral("rgba(112,144,176,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(112,144,176,0.55)");
        t.editorBackground = QStringLiteral("#efe2cc");
        t.editorTextColor = QStringLiteral("#38271a");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,200)");
        t.pageShadowRadius = 30;
        t.pageShadowOffset = 8;
        t.backgroundImage = bundledImagePath(QStringLiteral("wood-3.jpg"));
        t.backgroundMode = BgZoom;
        t.editorOpacity = 100;
        m_themes.append(t);
    }
    {
        // Bambu — mesa clara de bambu dourado, folha quase branca. Arejada,
        // luz de manhã, a mais luminosa das mesas.
        MiraTheme t;
        t.id = QStringLiteral("desk-bamboo");
        t.name = QStringLiteral("Bambu");
        t.bundled = true;
        t.appBackground = QStringLiteral("#3a2c14");
        t.panelBackground = QStringLiteral("#463418");
        t.panelBorder = QStringLiteral("#6a5028");
        t.textPrimary = QStringLiteral("#ecdcb8");
        t.textMuted = QStringLiteral("#b09864");
        t.textBright = QStringLiteral("#fbf0d4");
        t.hoverOverlay = QStringLiteral("rgba(236,220,184,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(236,220,184,0.04)");
        t.subtleBorder = QStringLiteral("rgba(236,220,184,0.10)");
        t.accentDefault = QStringLiteral("#c89030");
        t.hoverStrong = QStringLiteral("rgba(236,220,184,0.12)");
        t.borderStrong = QStringLiteral("rgba(236,220,184,0.22)");
        t.focusBorder = QStringLiteral("rgba(236,220,184,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.26)");
        t.disabledText = QStringLiteral("rgba(236,220,184,0.30)");
        t.selectionRing = QStringLiteral("#fbf0d4");
        t.accentSuccess = QStringLiteral("#6e9a3e");
        t.accentSuccessSoft = QStringLiteral("rgba(110,154,62,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(110,154,62,0.50)");
        t.accentDanger = QStringLiteral("#c05040");
        t.accentDangerSoft = QStringLiteral("rgba(192,80,64,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(192,80,64,0.50)");
        t.accentWarning = QStringLiteral("#c88a30");
        t.accentInfo = QStringLiteral("#5a82a0");
        t.accentInfoSoft = QStringLiteral("rgba(90,130,160,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(90,130,160,0.55)");
        t.editorBackground = QStringLiteral("#fbf6e8");
        t.editorTextColor = QStringLiteral("#3a3018");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(60,40,15,150)");
        t.pageShadowRadius = 28;
        t.pageShadowOffset = 7;
        t.backgroundImage = bundledImagePath(QStringLiteral("wood-6.jpg"));
        t.backgroundMode = BgZoom;
        t.editorOpacity = 100;
        m_themes.append(t);
    }
    {
        // Vigília — escrever de madrugada numa mesa de ébano. A folha é escura e
        // translúcida: a veia da madeira queimada vaza pelo papel. Luz baixa.
        MiraTheme t;
        t.id = QStringLiteral("desk-ebony");
        t.name = QStringLiteral("Vigília");
        t.bundled = true;
        t.appBackground = QStringLiteral("#141009");
        t.panelBackground = QStringLiteral("#1c160e");
        t.panelBorder = QStringLiteral("#322618");
        t.textPrimary = QStringLiteral("#d4c4a4");
        t.textMuted = QStringLiteral("#8a7656");
        t.textBright = QStringLiteral("#ece0c8");
        t.hoverOverlay = QStringLiteral("rgba(212,196,164,0.06)");
        t.pressedOverlay = QStringLiteral("rgba(212,196,164,0.04)");
        t.subtleBorder = QStringLiteral("rgba(212,196,164,0.10)");
        t.accentDefault = QStringLiteral("#c89858");
        t.hoverStrong = QStringLiteral("rgba(212,196,164,0.12)");
        t.borderStrong = QStringLiteral("rgba(212,196,164,0.22)");
        t.focusBorder = QStringLiteral("rgba(212,196,164,0.32)");
        t.inputBackground = QStringLiteral("rgba(0,0,0,0.34)");
        t.disabledText = QStringLiteral("rgba(212,196,164,0.30)");
        t.selectionRing = QStringLiteral("#ece0c8");
        t.accentSuccess = QStringLiteral("#8aa84e");
        t.accentSuccessSoft = QStringLiteral("rgba(138,168,78,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(138,168,78,0.50)");
        t.accentDanger = QStringLiteral("#c85540");
        t.accentDangerSoft = QStringLiteral("rgba(200,85,64,0.14)");
        t.accentDangerBorderSoft = QStringLiteral("rgba(200,85,64,0.50)");
        t.accentWarning = QStringLiteral("#d8923c");
        t.accentInfo = QStringLiteral("#7090b0");
        t.accentInfoSoft = QStringLiteral("rgba(112,144,176,0.22)");
        t.accentInfoBorderSoft = QStringLiteral("rgba(112,144,176,0.55)");
        t.editorBackground = QStringLiteral("#1a150e");
        t.editorTextColor = QStringLiteral("#ddd0b6");
        t.pageShadowEnabled = true;
        t.pageShadowColor = QStringLiteral("rgba(0,0,0,210)");
        t.pageShadowRadius = 32;
        t.pageShadowOffset = 7;
        t.backgroundImage = bundledImagePath(QStringLiteral("wood-1.jpg"));
        t.backgroundMode = BgZoom;
        t.editorOpacity = 100;
        m_themes.append(t);
    }

    // ---- Estampados — temas com imagem de fundo ----
    // A página do editor é SEMPRE opaca (editorOpacity 100): a foto aparece só
    // nas margens, em volta da folha, sem nunca atrapalhar a leitura. Helper
    // local pra não repetir os ~28 campos por tema; deriva os overlays rgba do
    // tom-base `tint` ("r,g,b" — claro pra UI escura, escuro pra UI clara).
    auto estampado = [&](const QString& id, const QString& name, const QString& image,
                         bool dark, const QString& tint,
                         const QString& appBg, const QString& panelBg, const QString& panelBorder,
                         const QString& textPrim, const QString& textMut, const QString& textBr,
                         const QString& accent, const QString& editorBg, const QString& editorText,
                         const QString& shadow, int shadowRadius, int shadowOffset) {
        MiraTheme t;
        t.id = id;
        t.name = name;
        t.bundled = true;
        t.appBackground    = appBg;
        t.panelBackground  = panelBg;
        t.panelBorder      = panelBorder;
        t.textPrimary      = textPrim;
        t.textMuted        = textMut;
        t.textBright       = textBr;
        t.hoverOverlay     = QStringLiteral("rgba(%1,0.06)").arg(tint);
        t.pressedOverlay   = QStringLiteral("rgba(%1,0.04)").arg(tint);
        t.subtleBorder     = QStringLiteral("rgba(%1,0.10)").arg(tint);
        t.accentDefault    = accent;
        t.hoverStrong      = QStringLiteral("rgba(%1,0.12)").arg(tint);
        t.borderStrong     = QStringLiteral("rgba(%1,0.22)").arg(tint);
        t.focusBorder      = QStringLiteral("rgba(%1,0.32)").arg(tint);
        t.inputBackground  = dark ? QStringLiteral("rgba(0,0,0,0.30)")
                                  : QStringLiteral("rgba(0,0,0,0.05)");
        t.disabledText     = QStringLiteral("rgba(%1,0.30)").arg(tint);
        t.selectionRing    = textBr;
        t.accentSuccess           = QStringLiteral("#7da855");
        t.accentSuccessSoft       = QStringLiteral("rgba(125,168,85,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(125,168,85,0.50)");
        t.accentDanger            = QStringLiteral("#c8504a");
        t.accentDangerSoft        = QStringLiteral("rgba(200,80,74,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(200,80,74,0.50)");
        t.accentWarning           = QStringLiteral("#c89860");
        t.accentInfo              = QStringLiteral("#5e8fbf");
        t.accentInfoSoft          = QStringLiteral("rgba(94,143,191,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(94,143,191,0.55)");
        t.editorBackground = editorBg;
        t.editorTextColor  = editorText;
        t.pageShadowEnabled = true;
        t.pageShadowColor   = shadow;
        t.pageShadowRadius  = shadowRadius;
        t.pageShadowOffset  = shadowOffset;
        t.backgroundImage   = bundledImagePath(image);
        t.backgroundMode    = BgZoom;
        t.editorOpacity     = 100;
        m_themes.append(t);
    };

    // Galáxias e cosmos
    estampado(QStringLiteral("via-lactea"), QStringLiteral("Via Láctea"),
              QStringLiteral("galaxy-texture.jpg"), true, QStringLiteral("196,204,224"),
              QStringLiteral("#060812"), QStringLiteral("#0c1020"), QStringLiteral("#20283f"),
              QStringLiteral("#c4cce0"), QStringLiteral("#6a7390"), QStringLiteral("#eef2fb"),
              QStringLiteral("#6c7cff"), QStringLiteral("#0a0e1a"), QStringLiteral("#d8deec"),
              QStringLiteral("rgba(0,0,0,210)"), 32, 7);

    estampado(QStringLiteral("cosmos"), QStringLiteral("Cosmos"),
              QStringLiteral("galaxy-texture-2.jpg"), true, QStringLiteral("191,224,221"),
              QStringLiteral("#061014"), QStringLiteral("#0a1820"), QStringLiteral("#18313a"),
              QStringLiteral("#bfe0dd"), QStringLiteral("#6a8a8c"), QStringLiteral("#e8f6f4"),
              QStringLiteral("#34c0b0"), QStringLiteral("#081418"), QStringLiteral("#d4e8e6"),
              QStringLiteral("rgba(0,0,0,200)"), 32, 7);

    estampado(QStringLiteral("firmamento"), QStringLiteral("Firmamento"),
              QStringLiteral("pexels-francian0-12940327.jpg"), true, QStringLiteral("207,196,224"),
              QStringLiteral("#08060f"), QStringLiteral("#0e0b1a"), QStringLiteral("#261d3a"),
              QStringLiteral("#cfc4e0"), QStringLiteral("#746688"), QStringLiteral("#efe8f6"),
              QStringLiteral("#9a6cd8"), QStringLiteral("#0a0814"), QStringLiteral("#ddd4ea"),
              QStringLiteral("rgba(0,0,0,210)"), 32, 7);

    estampado(QStringLiteral("quasar"), QStringLiteral("Quasar"),
              QStringLiteral("pexels-incrediblerafa-4737522.jpg"), true, QStringLiteral("198,206,224"),
              QStringLiteral("#060810"), QStringLiteral("#0c0f1c"), QStringLiteral("#202a40"),
              QStringLiteral("#c6cee0"), QStringLiteral("#6e7690"), QStringLiteral("#eceffa"),
              QStringLiteral("#e88a2e"), QStringLiteral("#080b16"), QStringLiteral("#d8deec"),
              QStringLiteral("rgba(0,0,0,215)"), 34, 7);

    estampado(QStringLiteral("abismo"), QStringLiteral("Abismo"),
              QStringLiteral("pexels-enginakyurt-6138036.jpg"), true, QStringLiteral("192,204,224"),
              QStringLiteral("#050a14"), QStringLiteral("#0a1322"), QStringLiteral("#182640"),
              QStringLiteral("#c0cce0"), QStringLiteral("#6a7690"), QStringLiteral("#e6eefa"),
              QStringLiteral("#d8a83a"), QStringLiteral("#07101e"), QStringLiteral("#d6dfee"),
              QStringLiteral("rgba(0,0,0,215)"), 34, 7);

    // Cidades e metrópoles
    estampado(QStringLiteral("metropole"), QStringLiteral("Metrópole"),
              QStringLiteral("pexels-2150015030-31016869.jpg"), true, QStringLiteral("194,210,230"),
              QStringLiteral("#08111e"), QStringLiteral("#0d1a2c"), QStringLiteral("#1e3048"),
              QStringLiteral("#c2d2e6"), QStringLiteral("#6a7c94"), QStringLiteral("#e8f0fa"),
              QStringLiteral("#4aa8e0"), QStringLiteral("#0a1726"), QStringLiteral("#d6e2f0"),
              QStringLiteral("rgba(0,0,0,200)"), 30, 7);

    estampado(QStringLiteral("vertigem"), QStringLiteral("Vertigem"),
              QStringLiteral("pexels-apyfz-30136066.jpg"), true, QStringLiteral("212,200,230"),
              QStringLiteral("#0a0818"), QStringLiteral("#120e24"), QStringLiteral("#2a2046"),
              QStringLiteral("#d4c8e6"), QStringLiteral("#7a6a92"), QStringLiteral("#f0e8fa"),
              QStringLiteral("#d24aa8"), QStringLiteral("#0d0a1c"), QStringLiteral("#e2d8ee"),
              QStringLiteral("rgba(0,0,0,210)"), 32, 7);

    estampado(QStringLiteral("distrito"), QStringLiteral("Distrito"),
              QStringLiteral("pexels-digital-phase-2150191459-31008030.jpg"), true, QStringLiteral("192,218,218"),
              QStringLiteral("#08161a"), QStringLiteral("#0c2024"), QStringLiteral("#184048"),
              QStringLiteral("#c0dada"), QStringLiteral("#688688"), QStringLiteral("#e6f4f4"),
              QStringLiteral("#e0524a"), QStringLiteral("#0a1c20"), QStringLiteral("#d6e8e6"),
              QStringLiteral("rgba(0,0,0,200)"), 30, 7);

    estampado(QStringLiteral("cidade-baixa"), QStringLiteral("Cidade Baixa"),
              QStringLiteral("pexels-einfoto-2130505.jpg"), true, QStringLiteral("214,204,198"),
              QStringLiteral("#0e0c0c"), QStringLiteral("#161212"), QStringLiteral("#2e2622"),
              QStringLiteral("#d6ccc6"), QStringLiteral("#8a7c74"), QStringLiteral("#f2ebe6"),
              QStringLiteral("#c25a44"), QStringLiteral("#14100e"), QStringLiteral("#e4dcd4"),
              QStringLiteral("rgba(0,0,0,210)"), 30, 7);

    estampado(QStringLiteral("babilonia"), QStringLiteral("Babilônia"),
              QStringLiteral("pexels-jimmy-liao-3615017-16705982.jpg"), true, QStringLiteral("224,208,192"),
              QStringLiteral("#100a08"), QStringLiteral("#1a120c"), QStringLiteral("#34261a"),
              QStringLiteral("#e0d0c0"), QStringLiteral("#927e6c"), QStringLiteral("#f4e8d8"),
              QStringLiteral("#e07a30"), QStringLiteral("#140d09"), QStringLiteral("#e6d8c8"),
              QStringLiteral("rgba(0,0,0,210)"), 32, 7);

    estampado(QStringLiteral("concreto"), QStringLiteral("Concreto"),
              QStringLiteral("pexels-water-white-1436785-5132764.jpg"), true, QStringLiteral("205,210,216"),
              QStringLiteral("#14161a"), QStringLiteral("#1c1f24"), QStringLiteral("#32373e"),
              QStringLiteral("#cdd2d8"), QStringLiteral("#828891"), QStringLiteral("#eef1f4"),
              QStringLiteral("#7a8a9a"), QStringLiteral("#1a1d22"), QStringLiteral("#dce0e6"),
              QStringLiteral("rgba(0,0,0,205)"), 30, 7);

    estampado(QStringLiteral("horizonte"), QStringLiteral("Horizonte"),
              QStringLiteral("pexels-kaique-lopes-3899395-9304147.jpg"), false, QStringLiteral("30,42,54"),
              QStringLiteral("#dce8f2"), QStringLiteral("#eaf2f8"), QStringLiteral("#c2d4e2"),
              QStringLiteral("#2c3a48"), QStringLiteral("#748494"), QStringLiteral("#16202c"),
              QStringLiteral("#3a8ec8"), QStringLiteral("#f8fbfe"), QStringLiteral("#1e2a36"),
              QStringLiteral("rgba(40,60,80,80)"), 26, 6);

    estampado(QStringLiteral("cidade-clara"), QStringLiteral("Cidade Clara"),
              QStringLiteral("pexels-wenchengphoto-6650574.jpg"), false, QStringLiteral("40,38,32"),
              QStringLiteral("#e8e6e0"), QStringLiteral("#f4f2ec"), QStringLiteral("#d4d0c6"),
              QStringLiteral("#38362f"), QStringLiteral("#8a8578"), QStringLiteral("#20201a"),
              QStringLiteral("#d8923a"), QStringLiteral("#fbf9f3"), QStringLiteral("#2a2820"),
              QStringLiteral("rgba(70,60,40,80)"), 26, 6);

    // Natureza — florestas, mar, céu
    estampado(QStringLiteral("selva"), QStringLiteral("Selva"),
              QStringLiteral("pexels-dongdilac-29556194.jpg"), true, QStringLiteral("196,220,194"),
              QStringLiteral("#0a140c"), QStringLiteral("#102014"), QStringLiteral("#1e3a24"),
              QStringLiteral("#c4dcc2"), QStringLiteral("#6c8a6c"), QStringLiteral("#e8f4e6"),
              QStringLiteral("#5aa84a"), QStringLiteral("#0d1c10"), QStringLiteral("#d6e8d2"),
              QStringLiteral("rgba(0,0,0,200)"), 30, 7);

    estampado(QStringLiteral("clareira"), QStringLiteral("Clareira"),
              QStringLiteral("pexels-elif-lale-a-1708563292-32445422.jpg"), true, QStringLiteral("202,220,200"),
              QStringLiteral("#0c160e"), QStringLiteral("#122016"), QStringLiteral("#213824"),
              QStringLiteral("#cadcc8"), QStringLiteral("#708c70"), QStringLiteral("#ecf6e8"),
              QStringLiteral("#88c060"), QStringLiteral("#f4faef"), QStringLiteral("#16240f"),
              QStringLiteral("rgba(10,24,12,150)"), 30, 7);

    estampado(QStringLiteral("alvorada"), QStringLiteral("Alvorada"),
              QStringLiteral("pexels-hoang-hai-72150707-8468801.jpg"), true, QStringLiteral("224,210,188"),
              QStringLiteral("#161009"), QStringLiteral("#20180e"), QStringLiteral("#3a2c18"),
              QStringLiteral("#e0d2bc"), QStringLiteral("#94836a"), QStringLiteral("#f4ead6"),
              QStringLiteral("#d89a40"), QStringLiteral("#faf3e6"), QStringLiteral("#2a2014"),
              QStringLiteral("rgba(20,14,6,150)"), 30, 7);

    estampado(QStringLiteral("ressaca"), QStringLiteral("Ressaca"),
              QStringLiteral("pexels-aulsh99-2860703.jpg"), true, QStringLiteral("191,216,230"),
              QStringLiteral("#06141f"), QStringLiteral("#0a1e2e"), QStringLiteral("#16384c"),
              QStringLiteral("#bfd8e6"), QStringLiteral("#688398"), QStringLiteral("#e6f2f8"),
              QStringLiteral("#2a9ad8"), QStringLiteral("#f2f9fc"), QStringLiteral("#0a1e2e"),
              QStringLiteral("rgba(6,20,31,160)"), 32, 8);

    estampado(QStringLiteral("estratosfera"), QStringLiteral("Estratosfera"),
              QStringLiteral("pexels-kumud-tripathi-434373-10556427.jpg"), false, QStringLiteral("32,44,56"),
              QStringLiteral("#dde8f0"), QStringLiteral("#ecf3f8"), QStringLiteral("#c6d6e2"),
              QStringLiteral("#2e3c48"), QStringLiteral("#76838f"), QStringLiteral("#18222c"),
              QStringLiteral("#5a9ad0"), QStringLiteral("#f9fcfe"), QStringLiteral("#202c38"),
              QStringLiteral("rgba(50,70,90,80)"), 26, 6);

    // Estampa decorativa
    estampado(QStringLiteral("azulejo"), QStringLiteral("Azulejo"),
              QStringLiteral("pattern-texture-10.jpg"), false, QStringLiteral("40,40,56"),
              QStringLiteral("#ede6da"), QStringLiteral("#f6f1e8"), QStringLiteral("#d8cdb8"),
              QStringLiteral("#3a3a44"), QStringLiteral("#8a8275"), QStringLiteral("#1e1e26"),
              QStringLiteral("#e07a2e"), QStringLiteral("#fbf7ef"), QStringLiteral("#2a2a30"),
              QStringLiteral("rgba(60,50,30,90)"), 28, 6);

    // ---- 20 temas novos: 5 claros, 5 amarelados, 5 escuros, 5 coloridos ----

    // ── CLAROS ──────────────────────────────────────────────────────────────
    {
        // Neve — branco glacial com toque azul-frio. O mais limpo e clínico
        // dos claros. Accent aço-azulado. Escrita de precisão.
        MiraTheme t;
        t.id   = QStringLiteral("neve");
        t.name = QStringLiteral("Neve");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#edf0f5");
        t.panelBackground  = QStringLiteral("#f8f9fc");
        t.panelBorder      = QStringLiteral("#d0d4de");
        t.textPrimary      = QStringLiteral("#2a2d3a");
        t.textMuted        = QStringLiteral("#6e7288");
        t.textBright       = QStringLiteral("#141620");
        t.hoverOverlay     = QStringLiteral("rgba(20,22,32,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(20,22,32,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(20,22,32,0.12)");
        t.accentDefault    = QStringLiteral("#4a6fa8");
        t.hoverStrong      = QStringLiteral("rgba(20,22,32,0.10)");
        t.borderStrong     = QStringLiteral("rgba(20,22,32,0.22)");
        t.focusBorder      = QStringLiteral("rgba(20,22,32,0.32)");
        t.inputBackground  = QStringLiteral("rgba(20,22,32,0.04)");
        t.disabledText     = QStringLiteral("rgba(20,22,32,0.32)");
        t.selectionRing    = QStringLiteral("#141620");
        t.accentSuccess           = QStringLiteral("#357a58");
        t.accentSuccessSoft       = QStringLiteral("rgba(53,122,88,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(53,122,88,0.50)");
        t.accentDanger            = QStringLiteral("#a03040");
        t.accentDangerSoft        = QStringLiteral("rgba(160,48,64,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(160,48,64,0.50)");
        t.accentWarning           = QStringLiteral("#b87030");
        t.accentInfo              = QStringLiteral("#2e6098");
        t.accentInfoSoft          = QStringLiteral("rgba(46,96,152,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(46,96,152,0.55)");
        t.editorBackground = QStringLiteral("#ffffff");
        t.editorTextColor  = QStringLiteral("#1a1d28");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(20,22,32,80)");
        t.pageShadowRadius  = 28;
        t.pageShadowOffset  = 6;
        m_themes.append(t);
    }
    {
        // Linho — tecido natural bege-neutro. Sem polarizar pra quente nem frio.
        // Accent terra queimada. Leitura longa sem fadiga.
        MiraTheme t;
        t.id   = QStringLiteral("linho");
        t.name = QStringLiteral("Linho");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#ede8e0");
        t.panelBackground  = QStringLiteral("#f5f0e8");
        t.panelBorder      = QStringLiteral("#d4cdc2");
        t.textPrimary      = QStringLiteral("#4a4540");
        t.textMuted        = QStringLiteral("#8a847c");
        t.textBright       = QStringLiteral("#2a2520");
        t.hoverOverlay     = QStringLiteral("rgba(42,37,32,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(42,37,32,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(42,37,32,0.12)");
        t.accentDefault    = QStringLiteral("#7a6a5a");
        t.hoverStrong      = QStringLiteral("rgba(42,37,32,0.10)");
        t.borderStrong     = QStringLiteral("rgba(42,37,32,0.22)");
        t.focusBorder      = QStringLiteral("rgba(42,37,32,0.32)");
        t.inputBackground  = QStringLiteral("rgba(42,37,32,0.04)");
        t.disabledText     = QStringLiteral("rgba(42,37,32,0.32)");
        t.selectionRing    = QStringLiteral("#2a2520");
        t.accentSuccess           = QStringLiteral("#4e7a3a");
        t.accentSuccessSoft       = QStringLiteral("rgba(78,122,58,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(78,122,58,0.50)");
        t.accentDanger            = QStringLiteral("#9a3838");
        t.accentDangerSoft        = QStringLiteral("rgba(154,56,56,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(154,56,56,0.50)");
        t.accentWarning           = QStringLiteral("#a86828");
        t.accentInfo              = QStringLiteral("#3a6888");
        t.accentInfoSoft          = QStringLiteral("rgba(58,104,136,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(58,104,136,0.55)");
        t.editorBackground = QStringLiteral("#faf6f0");
        t.editorTextColor  = QStringLiteral("#2a2520");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(42,37,32,80)");
        t.pageShadowRadius  = 26;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }
    {
        // Giz — matte acinzentado puro. Sem temperatura definida. Minimalismo
        // absoluto, texto quase-preto sobre quase-branco.
        MiraTheme t;
        t.id   = QStringLiteral("giz");
        t.name = QStringLiteral("Giz");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#e8e8e8");
        t.panelBackground  = QStringLiteral("#f4f4f4");
        t.panelBorder      = QStringLiteral("#d0d0d0");
        t.textPrimary      = QStringLiteral("#303030");
        t.textMuted        = QStringLiteral("#787878");
        t.textBright       = QStringLiteral("#181818");
        t.hoverOverlay     = QStringLiteral("rgba(24,24,24,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(24,24,24,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(24,24,24,0.12)");
        t.accentDefault    = QStringLiteral("#505050");
        t.hoverStrong      = QStringLiteral("rgba(24,24,24,0.10)");
        t.borderStrong     = QStringLiteral("rgba(24,24,24,0.22)");
        t.focusBorder      = QStringLiteral("rgba(24,24,24,0.32)");
        t.inputBackground  = QStringLiteral("rgba(24,24,24,0.04)");
        t.disabledText     = QStringLiteral("rgba(24,24,24,0.32)");
        t.selectionRing    = QStringLiteral("#181818");
        t.accentSuccess           = QStringLiteral("#407040");
        t.accentSuccessSoft       = QStringLiteral("rgba(64,112,64,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(64,112,64,0.50)");
        t.accentDanger            = QStringLiteral("#903030");
        t.accentDangerSoft        = QStringLiteral("rgba(144,48,48,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(144,48,48,0.50)");
        t.accentWarning           = QStringLiteral("#a06020");
        t.accentInfo              = QStringLiteral("#305880");
        t.accentInfoSoft          = QStringLiteral("rgba(48,88,128,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(48,88,128,0.55)");
        t.editorBackground = QStringLiteral("#fafafa");
        t.editorTextColor  = QStringLiteral("#1a1a1a");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(24,24,24,75)");
        t.pageShadowRadius  = 26;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }
    {
        // Quartzo — quartzo-rosa suave. Blush delicado, texto vinho-escuro.
        // Accent rosa-antigo. Feminino sem ser estridnete.
        MiraTheme t;
        t.id   = QStringLiteral("quartzo");
        t.name = QStringLiteral("Quartzo");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#f0e8ec");
        t.panelBackground  = QStringLiteral("#faf2f5");
        t.panelBorder      = QStringLiteral("#d8c8ce");
        t.textPrimary      = QStringLiteral("#4a3038");
        t.textMuted        = QStringLiteral("#8a6878");
        t.textBright       = QStringLiteral("#2a1820");
        t.hoverOverlay     = QStringLiteral("rgba(74,48,56,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(74,48,56,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(74,48,56,0.12)");
        t.accentDefault    = QStringLiteral("#c86888");
        t.hoverStrong      = QStringLiteral("rgba(74,48,56,0.10)");
        t.borderStrong     = QStringLiteral("rgba(74,48,56,0.22)");
        t.focusBorder      = QStringLiteral("rgba(74,48,56,0.32)");
        t.inputBackground  = QStringLiteral("rgba(74,48,56,0.04)");
        t.disabledText     = QStringLiteral("rgba(74,48,56,0.32)");
        t.selectionRing    = QStringLiteral("#2a1820");
        t.accentSuccess           = QStringLiteral("#4e7848");
        t.accentSuccessSoft       = QStringLiteral("rgba(78,120,72,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(78,120,72,0.50)");
        t.accentDanger            = QStringLiteral("#a83848");
        t.accentDangerSoft        = QStringLiteral("rgba(168,56,72,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(168,56,72,0.50)");
        t.accentWarning           = QStringLiteral("#b06030");
        t.accentInfo              = QStringLiteral("#5a5898");
        t.accentInfoSoft          = QStringLiteral("rgba(90,88,152,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(90,88,152,0.55)");
        t.editorBackground = QStringLiteral("#fff8fa");
        t.editorTextColor  = QStringLiteral("#2a1820");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(74,48,56,80)");
        t.pageShadowRadius  = 28;
        t.pageShadowOffset  = 6;
        m_themes.append(t);
    }
    {
        // Bambu — zen claro verde-bege. Respiração, calma, jardim japonês.
        // Accent verde-musgo discreto. O mais vegetal dos claros.
        MiraTheme t;
        t.id   = QStringLiteral("bambu");
        t.name = QStringLiteral("Bambu");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#e8ede0");
        t.panelBackground  = QStringLiteral("#f4f8ec");
        t.panelBorder      = QStringLiteral("#c8d4b8");
        t.textPrimary      = QStringLiteral("#384830");
        t.textMuted        = QStringLiteral("#788468");
        t.textBright       = QStringLiteral("#202e18");
        t.hoverOverlay     = QStringLiteral("rgba(32,46,24,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(32,46,24,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(32,46,24,0.12)");
        t.accentDefault    = QStringLiteral("#5a8040");
        t.hoverStrong      = QStringLiteral("rgba(32,46,24,0.10)");
        t.borderStrong     = QStringLiteral("rgba(32,46,24,0.22)");
        t.focusBorder      = QStringLiteral("rgba(32,46,24,0.32)");
        t.inputBackground  = QStringLiteral("rgba(32,46,24,0.04)");
        t.disabledText     = QStringLiteral("rgba(32,46,24,0.32)");
        t.selectionRing    = QStringLiteral("#202e18");
        t.accentSuccess           = QStringLiteral("#4a7838");
        t.accentSuccessSoft       = QStringLiteral("rgba(74,120,56,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(74,120,56,0.50)");
        t.accentDanger            = QStringLiteral("#904038");
        t.accentDangerSoft        = QStringLiteral("rgba(144,64,56,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(144,64,56,0.50)");
        t.accentWarning           = QStringLiteral("#a87828");
        t.accentInfo              = QStringLiteral("#3a7070");
        t.accentInfoSoft          = QStringLiteral("rgba(58,112,112,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(58,112,112,0.55)");
        t.editorBackground = QStringLiteral("#f8fbf0");
        t.editorTextColor  = QStringLiteral("#1e2c18");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(32,46,24,75)");
        t.pageShadowRadius  = 26;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }

    // ── AMARELADOS ──────────────────────────────────────────────────────────
    {
        // Mel — favo de mel dourado intenso. Mais quente que o Âmbar, mais
        // saturado que o Caramelo. Accent laranja-escuro rico.
        MiraTheme t;
        t.id   = QStringLiteral("mel");
        t.name = QStringLiteral("Mel");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#f0d890");
        t.panelBackground  = QStringLiteral("#f8e8a8");
        t.panelBorder      = QStringLiteral("#d4bc6c");
        t.textPrimary      = QStringLiteral("#4a3810");
        t.textMuted        = QStringLiteral("#8a7040");
        t.textBright       = QStringLiteral("#2a2008");
        t.hoverOverlay     = QStringLiteral("rgba(42,32,8,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(42,32,8,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(42,32,8,0.14)");
        t.accentDefault    = QStringLiteral("#c88020");
        t.hoverStrong      = QStringLiteral("rgba(42,32,8,0.10)");
        t.borderStrong     = QStringLiteral("rgba(42,32,8,0.24)");
        t.focusBorder      = QStringLiteral("rgba(42,32,8,0.34)");
        t.inputBackground  = QStringLiteral("rgba(42,32,8,0.05)");
        t.disabledText     = QStringLiteral("rgba(42,32,8,0.32)");
        t.selectionRing    = QStringLiteral("#2a2008");
        t.accentSuccess           = QStringLiteral("#5a7828");
        t.accentSuccessSoft       = QStringLiteral("rgba(90,120,40,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(90,120,40,0.45)");
        t.accentDanger            = QStringLiteral("#b03020");
        t.accentDangerSoft        = QStringLiteral("rgba(176,48,32,0.12)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(176,48,32,0.45)");
        t.accentWarning           = QStringLiteral("#c86018");
        t.accentInfo              = QStringLiteral("#3a6878");
        t.accentInfoSoft          = QStringLiteral("rgba(58,104,120,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(58,104,120,0.50)");
        t.editorBackground = QStringLiteral("#fdf4c8");
        t.editorTextColor  = QStringLiteral("#362a0c");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(42,32,8,95)");
        t.pageShadowRadius  = 26;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }
    {
        // Areia — deserto árido. Bege-areia neutro-quente, accent ocre-médio.
        // Vibe arqueológica, papiro, mapa antigo.
        MiraTheme t;
        t.id   = QStringLiteral("areia");
        t.name = QStringLiteral("Areia");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#e8d8b8");
        t.panelBackground  = QStringLiteral("#f4e8c8");
        t.panelBorder      = QStringLiteral("#ccc0a0");
        t.textPrimary      = QStringLiteral("#504030");
        t.textMuted        = QStringLiteral("#907868");
        t.textBright       = QStringLiteral("#2c2218");
        t.hoverOverlay     = QStringLiteral("rgba(44,34,24,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(44,34,24,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(44,34,24,0.14)");
        t.accentDefault    = QStringLiteral("#b88840");
        t.hoverStrong      = QStringLiteral("rgba(44,34,24,0.10)");
        t.borderStrong     = QStringLiteral("rgba(44,34,24,0.24)");
        t.focusBorder      = QStringLiteral("rgba(44,34,24,0.34)");
        t.inputBackground  = QStringLiteral("rgba(44,34,24,0.05)");
        t.disabledText     = QStringLiteral("rgba(44,34,24,0.32)");
        t.selectionRing    = QStringLiteral("#2c2218");
        t.accentSuccess           = QStringLiteral("#5e7838");
        t.accentSuccessSoft       = QStringLiteral("rgba(94,120,56,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(94,120,56,0.45)");
        t.accentDanger            = QStringLiteral("#a84030");
        t.accentDangerSoft        = QStringLiteral("rgba(168,64,48,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(168,64,48,0.45)");
        t.accentWarning           = QStringLiteral("#b86820");
        t.accentInfo              = QStringLiteral("#4a6880");
        t.accentInfoSoft          = QStringLiteral("rgba(74,104,128,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(74,104,128,0.50)");
        t.editorBackground = QStringLiteral("#f8f0e0");
        t.editorTextColor  = QStringLiteral("#3c3028");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(44,34,24,85)");
        t.pageShadowRadius  = 26;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }
    {
        // Palha — amarelo-palha quase-branco. O mais arejado dos amarelados.
        // Luz de celeiro, papel antigo não encardido.
        MiraTheme t;
        t.id   = QStringLiteral("palha");
        t.name = QStringLiteral("Palha");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#f4edd4");
        t.panelBackground  = QStringLiteral("#fdf8e8");
        t.panelBorder      = QStringLiteral("#e0d5b4");
        t.textPrimary      = QStringLiteral("#5a5030");
        t.textMuted        = QStringLiteral("#9a8e68");
        t.textBright       = QStringLiteral("#342e18");
        t.hoverOverlay     = QStringLiteral("rgba(52,46,24,0.05)");
        t.pressedOverlay   = QStringLiteral("rgba(52,46,24,0.035)");
        t.subtleBorder     = QStringLiteral("rgba(52,46,24,0.12)");
        t.accentDefault    = QStringLiteral("#a89040");
        t.hoverStrong      = QStringLiteral("rgba(52,46,24,0.09)");
        t.borderStrong     = QStringLiteral("rgba(52,46,24,0.20)");
        t.focusBorder      = QStringLiteral("rgba(52,46,24,0.30)");
        t.inputBackground  = QStringLiteral("rgba(52,46,24,0.04)");
        t.disabledText     = QStringLiteral("rgba(52,46,24,0.30)");
        t.selectionRing    = QStringLiteral("#342e18");
        t.accentSuccess           = QStringLiteral("#5a7830");
        t.accentSuccessSoft       = QStringLiteral("rgba(90,120,48,0.13)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(90,120,48,0.42)");
        t.accentDanger            = QStringLiteral("#a04030");
        t.accentDangerSoft        = QStringLiteral("rgba(160,64,48,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(160,64,48,0.42)");
        t.accentWarning           = QStringLiteral("#b07820");
        t.accentInfo              = QStringLiteral("#4a6870");
        t.accentInfoSoft          = QStringLiteral("rgba(74,104,112,0.15)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(74,104,112,0.50)");
        t.editorBackground = QStringLiteral("#fffce8");
        t.editorTextColor  = QStringLiteral("#342e18");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(52,46,24,75)");
        t.pageShadowRadius  = 24;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }
    {
        // Açafrão — dark amarelado intenso. Fundo sépia-queimado escuro, texto
        // ouro-claro, accent açafrão brilhante. Épico, oriental, ancestral.
        MiraTheme t;
        t.id   = QStringLiteral("acafrao");
        t.name = QStringLiteral("Açafrão");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#2a1e08");
        t.panelBackground  = QStringLiteral("#342808");
        t.panelBorder      = QStringLiteral("#584418");
        t.textPrimary      = QStringLiteral("#e0c880");
        t.textMuted        = QStringLiteral("#906840");
        t.textBright       = QStringLiteral("#f8e8b0");
        t.hoverOverlay     = QStringLiteral("rgba(224,200,128,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(224,200,128,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(224,200,128,0.10)");
        t.accentDefault    = QStringLiteral("#e8a020");
        t.hoverStrong      = QStringLiteral("rgba(224,200,128,0.12)");
        t.borderStrong     = QStringLiteral("rgba(224,200,128,0.22)");
        t.focusBorder      = QStringLiteral("rgba(224,200,128,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText     = QStringLiteral("rgba(224,200,128,0.30)");
        t.selectionRing    = QStringLiteral("#f8e8b0");
        t.accentSuccess           = QStringLiteral("#80a840");
        t.accentSuccessSoft       = QStringLiteral("rgba(128,168,64,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(128,168,64,0.50)");
        t.accentDanger            = QStringLiteral("#c84830");
        t.accentDangerSoft        = QStringLiteral("rgba(200,72,48,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(200,72,48,0.50)");
        t.accentWarning           = QStringLiteral("#e8a020");
        t.accentInfo              = QStringLiteral("#5888a0");
        t.accentInfoSoft          = QStringLiteral("rgba(88,136,160,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(88,136,160,0.55)");
        t.editorBackground = QStringLiteral("#2e2210");
        t.editorTextColor  = QStringLiteral("#e8d890");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,190)");
        t.pageShadowRadius  = 26;
        t.pageShadowOffset  = 6;
        m_themes.append(t);
    }
    {
        // Manteiga — amarelo-creme pálido suavíssimo. O mais delicado dos
        // amarelados. Quase-branco com alma quente. Escrita matinal, leveza.
        MiraTheme t;
        t.id   = QStringLiteral("manteiga");
        t.name = QStringLiteral("Manteiga");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#f8f4d8");
        t.panelBackground  = QStringLiteral("#fdfae8");
        t.panelBorder      = QStringLiteral("#e8e0c0");
        t.textPrimary      = QStringLiteral("#585040");
        t.textMuted        = QStringLiteral("#9a9070");
        t.textBright       = QStringLiteral("#383020");
        t.hoverOverlay     = QStringLiteral("rgba(56,48,32,0.05)");
        t.pressedOverlay   = QStringLiteral("rgba(56,48,32,0.035)");
        t.subtleBorder     = QStringLiteral("rgba(56,48,32,0.12)");
        t.accentDefault    = QStringLiteral("#b0a040");
        t.hoverStrong      = QStringLiteral("rgba(56,48,32,0.09)");
        t.borderStrong     = QStringLiteral("rgba(56,48,32,0.20)");
        t.focusBorder      = QStringLiteral("rgba(56,48,32,0.30)");
        t.inputBackground  = QStringLiteral("rgba(56,48,32,0.04)");
        t.disabledText     = QStringLiteral("rgba(56,48,32,0.30)");
        t.selectionRing    = QStringLiteral("#383020");
        t.accentSuccess           = QStringLiteral("#5a7830");
        t.accentSuccessSoft       = QStringLiteral("rgba(90,120,48,0.13)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(90,120,48,0.42)");
        t.accentDanger            = QStringLiteral("#a04838");
        t.accentDangerSoft        = QStringLiteral("rgba(160,72,56,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(160,72,56,0.42)");
        t.accentWarning           = QStringLiteral("#b08020");
        t.accentInfo              = QStringLiteral("#4a6878");
        t.accentInfoSoft          = QStringLiteral("rgba(74,104,120,0.15)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(74,104,120,0.50)");
        t.editorBackground = QStringLiteral("#fefcf0");
        t.editorTextColor  = QStringLiteral("#383020");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(56,48,32,70)");
        t.pageShadowRadius  = 24;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }

    // ── ESCUROS ─────────────────────────────────────────────────────────────
    {
        // Carvão — cinza-carvão neutro-frio. Sem cor, sem drama. O mais
        // funcional dos escuros. Accent aço fosco.
        MiraTheme t;
        t.id   = QStringLiteral("carvao");
        t.name = QStringLiteral("Carvão");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#1e2024");
        t.panelBackground  = QStringLiteral("#282c30");
        t.panelBorder      = QStringLiteral("#3c4048");
        t.textPrimary      = QStringLiteral("#c4c8d0");
        t.textMuted        = QStringLiteral("#5c6068");
        t.textBright       = QStringLiteral("#e8ecf0");
        t.hoverOverlay     = QStringLiteral("rgba(196,200,208,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(196,200,208,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(196,200,208,0.10)");
        t.accentDefault    = QStringLiteral("#8098b8");
        t.hoverStrong      = QStringLiteral("rgba(196,200,208,0.12)");
        t.borderStrong     = QStringLiteral("rgba(196,200,208,0.22)");
        t.focusBorder      = QStringLiteral("rgba(196,200,208,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.25)");
        t.disabledText     = QStringLiteral("rgba(196,200,208,0.30)");
        t.selectionRing    = QStringLiteral("#e8ecf0");
        t.accentSuccess           = QStringLiteral("#5a9870");
        t.accentSuccessSoft       = QStringLiteral("rgba(90,152,112,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(90,152,112,0.50)");
        t.accentDanger            = QStringLiteral("#c05060");
        t.accentDangerSoft        = QStringLiteral("rgba(192,80,96,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(192,80,96,0.50)");
        t.accentWarning           = QStringLiteral("#c09050");
        t.accentInfo              = QStringLiteral("#6888b0");
        t.accentInfoSoft          = QStringLiteral("rgba(104,136,176,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(104,136,176,0.55)");
        t.editorBackground = QStringLiteral("#242830");
        t.editorTextColor  = QStringLiteral("#c8ccd4");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,170)");
        t.pageShadowRadius  = 24;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }
    {
        // Obsidiana — pedra vulcânica pura. Preto-esverdeado profundo,
        // accent esmeralda elétrico. Força, densidade, mistério mineral.
        MiraTheme t;
        t.id   = QStringLiteral("obsidiana");
        t.name = QStringLiteral("Obsidiana");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#0a0c0a");
        t.panelBackground  = QStringLiteral("#101410");
        t.panelBorder      = QStringLiteral("#1c241c");
        t.textPrimary      = QStringLiteral("#b8d0b8");
        t.textMuted        = QStringLiteral("#4a5c4a");
        t.textBright       = QStringLiteral("#d8f0d8");
        t.hoverOverlay     = QStringLiteral("rgba(184,208,184,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(184,208,184,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(184,208,184,0.10)");
        t.accentDefault    = QStringLiteral("#00c87a");
        t.hoverStrong      = QStringLiteral("rgba(184,208,184,0.12)");
        t.borderStrong     = QStringLiteral("rgba(184,208,184,0.22)");
        t.focusBorder      = QStringLiteral("rgba(184,208,184,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.35)");
        t.disabledText     = QStringLiteral("rgba(184,208,184,0.30)");
        t.selectionRing    = QStringLiteral("#d8f0d8");
        t.accentSuccess           = QStringLiteral("#40b870");
        t.accentSuccessSoft       = QStringLiteral("rgba(64,184,112,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(64,184,112,0.50)");
        t.accentDanger            = QStringLiteral("#c05858");
        t.accentDangerSoft        = QStringLiteral("rgba(192,88,88,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(192,88,88,0.50)");
        t.accentWarning           = QStringLiteral("#c09040");
        t.accentInfo              = QStringLiteral("#40a8b0");
        t.accentInfoSoft          = QStringLiteral("rgba(64,168,176,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(64,168,176,0.55)");
        t.editorBackground = QStringLiteral("#0c100c");
        t.editorTextColor  = QStringLiteral("#c4dcc4");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,200)");
        t.pageShadowRadius  = 26;
        t.pageShadowOffset  = 6;
        m_themes.append(t);
    }
    {
        // Nimbo — nuvem de tempestade. Azul-cinza escuro denso, accent azul-
        // tempestade. Antes da chuva. Concentração pesada, escrita intensa.
        MiraTheme t;
        t.id   = QStringLiteral("nimbo");
        t.name = QStringLiteral("Nimbo");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#141820");
        t.panelBackground  = QStringLiteral("#1c2230");
        t.panelBorder      = QStringLiteral("#2a3040");
        t.textPrimary      = QStringLiteral("#b0b8cc");
        t.textMuted        = QStringLiteral("#485060");
        t.textBright       = QStringLiteral("#d0d8ec");
        t.hoverOverlay     = QStringLiteral("rgba(176,184,204,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(176,184,204,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(176,184,204,0.10)");
        t.accentDefault    = QStringLiteral("#6878b0");
        t.hoverStrong      = QStringLiteral("rgba(176,184,204,0.12)");
        t.borderStrong     = QStringLiteral("rgba(176,184,204,0.22)");
        t.focusBorder      = QStringLiteral("rgba(176,184,204,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText     = QStringLiteral("rgba(176,184,204,0.30)");
        t.selectionRing    = QStringLiteral("#d0d8ec");
        t.accentSuccess           = QStringLiteral("#5a9878");
        t.accentSuccessSoft       = QStringLiteral("rgba(90,152,120,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(90,152,120,0.50)");
        t.accentDanger            = QStringLiteral("#b85860");
        t.accentDangerSoft        = QStringLiteral("rgba(184,88,96,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(184,88,96,0.50)");
        t.accentWarning           = QStringLiteral("#b89050");
        t.accentInfo              = QStringLiteral("#5888b8");
        t.accentInfoSoft          = QStringLiteral("rgba(88,136,184,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(88,136,184,0.55)");
        t.editorBackground = QStringLiteral("#161a24");
        t.editorTextColor  = QStringLiteral("#b8c0d4");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,185)");
        t.pageShadowRadius  = 26;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }
    {
        // Profundo — fundo do oceano. O mais azul-preto da família escura.
        // Accent ciano-abissal. Silêncio submarino, pressão criativa.
        MiraTheme t;
        t.id   = QStringLiteral("profundo");
        t.name = QStringLiteral("Profundo");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#060c14");
        t.panelBackground  = QStringLiteral("#0c1420");
        t.panelBorder      = QStringLiteral("#162030");
        t.textPrimary      = QStringLiteral("#a8c0d0");
        t.textMuted        = QStringLiteral("#406070");
        t.textBright       = QStringLiteral("#c8e0f0");
        t.hoverOverlay     = QStringLiteral("rgba(168,192,208,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(168,192,208,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(168,192,208,0.10)");
        t.accentDefault    = QStringLiteral("#2888b8");
        t.hoverStrong      = QStringLiteral("rgba(168,192,208,0.12)");
        t.borderStrong     = QStringLiteral("rgba(168,192,208,0.22)");
        t.focusBorder      = QStringLiteral("rgba(168,192,208,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.35)");
        t.disabledText     = QStringLiteral("rgba(168,192,208,0.30)");
        t.selectionRing    = QStringLiteral("#c8e0f0");
        t.accentSuccess           = QStringLiteral("#3a9870");
        t.accentSuccessSoft       = QStringLiteral("rgba(58,152,112,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(58,152,112,0.50)");
        t.accentDanger            = QStringLiteral("#b05060");
        t.accentDangerSoft        = QStringLiteral("rgba(176,80,96,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(176,80,96,0.50)");
        t.accentWarning           = QStringLiteral("#b08848");
        t.accentInfo              = QStringLiteral("#1878b0");
        t.accentInfoSoft          = QStringLiteral("rgba(24,120,176,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(24,120,176,0.55)");
        t.editorBackground = QStringLiteral("#080e18");
        t.editorTextColor  = QStringLiteral("#b0c8d8");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,210)");
        t.pageShadowRadius  = 30;
        t.pageShadowOffset  = 7;
        m_themes.append(t);
    }

    // ── COLORIDOS ───────────────────────────────────────────────────────────
    {
        // Lavanda — roxo-lilás escuro vibrante. Fundo uva, accent violeta
        // elétrico. Fantasia, magia, romance gótico moderno.
        MiraTheme t;
        t.id   = QStringLiteral("lavanda");
        t.name = QStringLiteral("Lavanda");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#1e1428");
        t.panelBackground  = QStringLiteral("#281c38");
        t.panelBorder      = QStringLiteral("#402c58");
        t.textPrimary      = QStringLiteral("#d0c0e0");
        t.textMuted        = QStringLiteral("#7060a0");
        t.textBright       = QStringLiteral("#f0e0ff");
        t.hoverOverlay     = QStringLiteral("rgba(208,192,224,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(208,192,224,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(208,192,224,0.10)");
        t.accentDefault    = QStringLiteral("#b060e8");
        t.hoverStrong      = QStringLiteral("rgba(208,192,224,0.12)");
        t.borderStrong     = QStringLiteral("rgba(208,192,224,0.22)");
        t.focusBorder      = QStringLiteral("rgba(208,192,224,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText     = QStringLiteral("rgba(208,192,224,0.30)");
        t.selectionRing    = QStringLiteral("#f0e0ff");
        t.accentSuccess           = QStringLiteral("#70b890");
        t.accentSuccessSoft       = QStringLiteral("rgba(112,184,144,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(112,184,144,0.50)");
        t.accentDanger            = QStringLiteral("#e06080");
        t.accentDangerSoft        = QStringLiteral("rgba(224,96,128,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(224,96,128,0.50)");
        t.accentWarning           = QStringLiteral("#d09040");
        t.accentInfo              = QStringLiteral("#80a0d8");
        t.accentInfoSoft          = QStringLiteral("rgba(128,160,216,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(128,160,216,0.55)");
        t.editorBackground = QStringLiteral("#221630");
        t.editorTextColor  = QStringLiteral("#e0d0f0");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,200)");
        t.pageShadowRadius  = 28;
        t.pageShadowOffset  = 6;
        m_themes.append(t);
    }
    {
        // Citrino — neon amarelo-verde sobre preto-floresta. Accent lima
        // elétrico saturado. Sci-fi, hacker, energia pura.
        MiraTheme t;
        t.id   = QStringLiteral("citrino");
        t.name = QStringLiteral("Citrino");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#101808");
        t.panelBackground  = QStringLiteral("#182008");
        t.panelBorder      = QStringLiteral("#283810");
        t.textPrimary      = QStringLiteral("#d0e0a0");
        t.textMuted        = QStringLiteral("#6a8040");
        t.textBright       = QStringLiteral("#f0f8c0");
        t.hoverOverlay     = QStringLiteral("rgba(208,224,160,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(208,224,160,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(208,224,160,0.10)");
        t.accentDefault    = QStringLiteral("#c8e000");
        t.hoverStrong      = QStringLiteral("rgba(208,224,160,0.12)");
        t.borderStrong     = QStringLiteral("rgba(208,224,160,0.22)");
        t.focusBorder      = QStringLiteral("rgba(208,224,160,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText     = QStringLiteral("rgba(208,224,160,0.30)");
        t.selectionRing    = QStringLiteral("#f0f8c0");
        t.accentSuccess           = QStringLiteral("#60d060");
        t.accentSuccessSoft       = QStringLiteral("rgba(96,208,96,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(96,208,96,0.50)");
        t.accentDanger            = QStringLiteral("#e84040");
        t.accentDangerSoft        = QStringLiteral("rgba(232,64,64,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(232,64,64,0.50)");
        t.accentWarning           = QStringLiteral("#e8c800");
        t.accentInfo              = QStringLiteral("#40c0c0");
        t.accentInfoSoft          = QStringLiteral("rgba(64,192,192,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(64,192,192,0.55)");
        t.editorBackground = QStringLiteral("#121a08");
        t.editorTextColor  = QStringLiteral("#d8e8b0");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,200)");
        t.pageShadowRadius  = 26;
        t.pageShadowOffset  = 6;
        m_themes.append(t);
    }
    {
        // Ferrugem — laranja-ferrugem neon sobre marrom-queimado escuro.
        // Accent rust vibrante. Pós-apocalíptico, calor industrial, urgência.
        MiraTheme t;
        t.id   = QStringLiteral("ferrugem");
        t.name = QStringLiteral("Ferrugem");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#1a0e08");
        t.panelBackground  = QStringLiteral("#241408");
        t.panelBorder      = QStringLiteral("#402010");
        t.textPrimary      = QStringLiteral("#e0c0a0");
        t.textMuted        = QStringLiteral("#806040");
        t.textBright       = QStringLiteral("#f8e0c0");
        t.hoverOverlay     = QStringLiteral("rgba(224,192,160,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(224,192,160,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(224,192,160,0.10)");
        t.accentDefault    = QStringLiteral("#e84820");
        t.hoverStrong      = QStringLiteral("rgba(224,192,160,0.12)");
        t.borderStrong     = QStringLiteral("rgba(224,192,160,0.22)");
        t.focusBorder      = QStringLiteral("rgba(224,192,160,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText     = QStringLiteral("rgba(224,192,160,0.30)");
        t.selectionRing    = QStringLiteral("#f8e0c0");
        t.accentSuccess           = QStringLiteral("#88a840");
        t.accentSuccessSoft       = QStringLiteral("rgba(136,168,64,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(136,168,64,0.50)");
        t.accentDanger            = QStringLiteral("#e84820");
        t.accentDangerSoft        = QStringLiteral("rgba(232,72,32,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(232,72,32,0.50)");
        t.accentWarning           = QStringLiteral("#e8a030");
        t.accentInfo              = QStringLiteral("#6890b0");
        t.accentInfoSoft          = QStringLiteral("rgba(104,144,176,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(104,144,176,0.55)");
        t.editorBackground = QStringLiteral("#1c1008");
        t.editorTextColor  = QStringLiteral("#e8c8a8");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,200)");
        t.pageShadowRadius  = 28;
        t.pageShadowOffset  = 6;
        m_themes.append(t);
    }
    {
        // Sálvia — verde-sálvia escuro calmo. Accent verde-hortelã luminoso.
        // Natureza zen, botânico, escrita introspectiva.
        MiraTheme t;
        t.id   = QStringLiteral("salvia");
        t.name = QStringLiteral("Sálvia");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#101c14");
        t.panelBackground  = QStringLiteral("#162018");
        t.panelBorder      = QStringLiteral("#263828");
        t.textPrimary      = QStringLiteral("#c0d8c0");
        t.textMuted        = QStringLiteral("#5a7860");
        t.textBright       = QStringLiteral("#e0f0e0");
        t.hoverOverlay     = QStringLiteral("rgba(192,216,192,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(192,216,192,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(192,216,192,0.10)");
        t.accentDefault    = QStringLiteral("#5ac870");
        t.hoverStrong      = QStringLiteral("rgba(192,216,192,0.12)");
        t.borderStrong     = QStringLiteral("rgba(192,216,192,0.22)");
        t.focusBorder      = QStringLiteral("rgba(192,216,192,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText     = QStringLiteral("rgba(192,216,192,0.30)");
        t.selectionRing    = QStringLiteral("#e0f0e0");
        t.accentSuccess           = QStringLiteral("#60c878");
        t.accentSuccessSoft       = QStringLiteral("rgba(96,200,120,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(96,200,120,0.50)");
        t.accentDanger            = QStringLiteral("#c06060");
        t.accentDangerSoft        = QStringLiteral("rgba(192,96,96,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(192,96,96,0.50)");
        t.accentWarning           = QStringLiteral("#c0a040");
        t.accentInfo              = QStringLiteral("#40a8b0");
        t.accentInfoSoft          = QStringLiteral("rgba(64,168,176,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(64,168,176,0.55)");
        t.editorBackground = QStringLiteral("#121e16");
        t.editorTextColor  = QStringLiteral("#c8dcc8");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,190)");
        t.pageShadowRadius  = 26;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }
    {
        // Éter — ciano etéreo sobre preto-abissal. Accent ciano-neon frio.
        // O mais frio e tecnológico dos coloridos. Matrix, terminal, sinapse.
        MiraTheme t;
        t.id   = QStringLiteral("eter");
        t.name = QStringLiteral("Éter");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#080e14");
        t.panelBackground  = QStringLiteral("#0e1620");
        t.panelBorder      = QStringLiteral("#182438");
        t.textPrimary      = QStringLiteral("#a0c8d8");
        t.textMuted        = QStringLiteral("#406070");
        t.textBright       = QStringLiteral("#c0e8f8");
        t.hoverOverlay     = QStringLiteral("rgba(160,200,216,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(160,200,216,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(160,200,216,0.10)");
        t.accentDefault    = QStringLiteral("#00d8e8");
        t.hoverStrong      = QStringLiteral("rgba(160,200,216,0.12)");
        t.borderStrong     = QStringLiteral("rgba(160,200,216,0.22)");
        t.focusBorder      = QStringLiteral("rgba(160,200,216,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.35)");
        t.disabledText     = QStringLiteral("rgba(160,200,216,0.30)");
        t.selectionRing    = QStringLiteral("#c0e8f8");
        t.accentSuccess           = QStringLiteral("#00d878");
        t.accentSuccessSoft       = QStringLiteral("rgba(0,216,120,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(0,216,120,0.50)");
        t.accentDanger            = QStringLiteral("#f05060");
        t.accentDangerSoft        = QStringLiteral("rgba(240,80,96,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(240,80,96,0.50)");
        t.accentWarning           = QStringLiteral("#e0c000");
        t.accentInfo              = QStringLiteral("#40a0f8");
        t.accentInfoSoft          = QStringLiteral("rgba(64,160,248,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(64,160,248,0.55)");
        t.editorBackground = QStringLiteral("#0a1018");
        t.editorTextColor  = QStringLiteral("#b0d0e0");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,210)");
        t.pageShadowRadius  = 30;
        t.pageShadowOffset  = 7;
        m_themes.append(t);
    }

    // ---- Temas criativos: Daguerreótipo, CRT Verde, Bauhaus, Nocturno,
    //      Papyrus, VHS, Wabi-Sabi, Manuscrito, Limão, Brutalismo ----
    {
        // Daguerreótipo — fotografia victoriana em chapa de prata.
        // Fundo muito escuro sépia-metálico, texto creme-oxidado, zero accent brilhante.
        // Monotonal como uma imagem do século XIX.
        MiraTheme t;
        t.id   = QStringLiteral("daguerreotipo");
        t.name = QStringLiteral("Daguerreótipo");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#1a1510");
        t.panelBackground  = QStringLiteral("#1e1a13");
        t.panelBorder      = QStringLiteral("#3a3020");
        t.textPrimary      = QStringLiteral("#c8b89a");
        t.textMuted        = QStringLiteral("#7a6a50");
        t.textBright       = QStringLiteral("#e0d0b0");
        t.hoverOverlay     = QStringLiteral("rgba(200,184,154,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(200,184,154,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(200,184,154,0.10)");
        t.accentDefault    = QStringLiteral("#9a8060");
        t.hoverStrong      = QStringLiteral("rgba(200,184,154,0.12)");
        t.borderStrong     = QStringLiteral("rgba(200,184,154,0.22)");
        t.focusBorder      = QStringLiteral("rgba(200,184,154,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText     = QStringLiteral("rgba(200,184,154,0.30)");
        t.selectionRing    = QStringLiteral("#c8b89a");
        t.accentSuccess           = QStringLiteral("#8a9a70");
        t.accentSuccessSoft       = QStringLiteral("rgba(138,154,112,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(138,154,112,0.50)");
        t.accentDanger            = QStringLiteral("#9a5040");
        t.accentDangerSoft        = QStringLiteral("rgba(154,80,64,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(154,80,64,0.50)");
        t.accentWarning           = QStringLiteral("#a07840");
        t.accentInfo              = QStringLiteral("#6a7a9a");
        t.accentInfoSoft          = QStringLiteral("rgba(106,122,154,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(106,122,154,0.55)");
        t.editorBackground = QStringLiteral("#211c14");
        t.editorTextColor  = QStringLiteral("#c8b89a");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,180)");
        t.pageShadowRadius  = 20;
        t.pageShadowOffset  = 4;
        m_themes.append(t);
    }
    {
        // CRT Verde — terminal de fósforo verde anos 70/80.
        // Preto absoluto + texto verde P1 (528nm), monocromático cirúrgico.
        // Nenhuma outra cor existe neste universo.
        MiraTheme t;
        t.id   = QStringLiteral("crt-verde");
        t.name = QStringLiteral("CRT Verde");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#020602");
        t.panelBackground  = QStringLiteral("#040904");
        t.panelBorder      = QStringLiteral("#0a1f0a");
        t.textPrimary      = QStringLiteral("#33cc33");
        t.textMuted        = QStringLiteral("#1a661a");
        t.textBright       = QStringLiteral("#66ff66");
        t.hoverOverlay     = QStringLiteral("rgba(51,204,51,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(51,204,51,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(51,204,51,0.10)");
        t.accentDefault    = QStringLiteral("#00ff41");
        t.hoverStrong      = QStringLiteral("rgba(51,204,51,0.12)");
        t.borderStrong     = QStringLiteral("rgba(51,204,51,0.22)");
        t.focusBorder      = QStringLiteral("rgba(51,204,51,0.35)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.40)");
        t.disabledText     = QStringLiteral("rgba(51,204,51,0.30)");
        t.selectionRing    = QStringLiteral("#00ff41");
        t.accentSuccess           = QStringLiteral("#00ff41");
        t.accentSuccessSoft       = QStringLiteral("rgba(0,255,65,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(0,255,65,0.50)");
        t.accentDanger            = QStringLiteral("#ff4040");
        t.accentDangerSoft        = QStringLiteral("rgba(255,64,64,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(255,64,64,0.50)");
        t.accentWarning           = QStringLiteral("#cccc00");
        t.accentInfo              = QStringLiteral("#33cc33");
        t.accentInfoSoft          = QStringLiteral("rgba(51,204,51,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(51,204,51,0.55)");
        t.editorBackground = QStringLiteral("#010401");
        t.editorTextColor  = QStringLiteral("#33cc33");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,30,0,200)");
        t.pageShadowRadius  = 20;
        t.pageShadowOffset  = 4;
        m_themes.append(t);
    }
    {
        // Bauhaus — escola alemã 1919. Branco gelo + preto puro + vermelho cirúrgico.
        // Regra: um único accent. Tudo o mais é estrutura.
        MiraTheme t;
        t.id   = QStringLiteral("bauhaus");
        t.name = QStringLiteral("Bauhaus");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#f2f0ec");
        t.panelBackground  = QStringLiteral("#eeece8");
        t.panelBorder      = QStringLiteral("#c8c4be");
        t.textPrimary      = QStringLiteral("#111111");
        t.textMuted        = QStringLiteral("#666660");
        t.textBright       = QStringLiteral("#000000");
        t.hoverOverlay     = QStringLiteral("rgba(0,0,0,0.05)");
        t.pressedOverlay   = QStringLiteral("rgba(0,0,0,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(0,0,0,0.10)");
        t.accentDefault    = QStringLiteral("#cc2200");
        t.hoverStrong      = QStringLiteral("rgba(0,0,0,0.12)");
        t.borderStrong     = QStringLiteral("rgba(0,0,0,0.22)");
        t.focusBorder      = QStringLiteral("rgba(0,0,0,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.04)");
        t.disabledText     = QStringLiteral("rgba(0,0,0,0.30)");
        t.selectionRing    = QStringLiteral("#cc2200");
        t.accentSuccess           = QStringLiteral("#226600");
        t.accentSuccessSoft       = QStringLiteral("rgba(34,102,0,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(34,102,0,0.45)");
        t.accentDanger            = QStringLiteral("#cc2200");
        t.accentDangerSoft        = QStringLiteral("rgba(204,34,0,0.12)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(204,34,0,0.45)");
        t.accentWarning           = QStringLiteral("#cc8800");
        t.accentInfo              = QStringLiteral("#003399");
        t.accentInfoSoft          = QStringLiteral("rgba(0,51,153,0.14)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(0,51,153,0.45)");
        t.editorBackground = QStringLiteral("#fafaf8");
        t.editorTextColor  = QStringLiteral("#111111");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,60)");
        t.pageShadowRadius  = 16;
        t.pageShadowOffset  = 3;
        m_themes.append(t);
    }
    {
        // Nocturno — lógica invertida. Painel azul-petróleo escuro + editor creme pálido.
        // A área de escrita é a parte mais clara; a moldura é a mais escura.
        MiraTheme t;
        t.id   = QStringLiteral("nocturno");
        t.name = QStringLiteral("Nocturno");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#1a2030");
        t.panelBackground  = QStringLiteral("#141c28");
        t.panelBorder      = QStringLiteral("#2a3448");
        t.textPrimary      = QStringLiteral("#a8b8d0");
        t.textMuted        = QStringLiteral("#566070");
        t.textBright       = QStringLiteral("#d0dff0");
        t.hoverOverlay     = QStringLiteral("rgba(168,184,208,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(168,184,208,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(168,184,208,0.10)");
        t.accentDefault    = QStringLiteral("#5b9bd5");
        t.hoverStrong      = QStringLiteral("rgba(168,184,208,0.12)");
        t.borderStrong     = QStringLiteral("rgba(168,184,208,0.22)");
        t.focusBorder      = QStringLiteral("rgba(168,184,208,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText     = QStringLiteral("rgba(168,184,208,0.30)");
        t.selectionRing    = QStringLiteral("#5b9bd5");
        t.accentSuccess           = QStringLiteral("#6aaa80");
        t.accentSuccessSoft       = QStringLiteral("rgba(106,170,128,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(106,170,128,0.50)");
        t.accentDanger            = QStringLiteral("#cc6666");
        t.accentDangerSoft        = QStringLiteral("rgba(204,102,102,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(204,102,102,0.50)");
        t.accentWarning           = QStringLiteral("#c8a060");
        t.accentInfo              = QStringLiteral("#5b9bd5");
        t.accentInfoSoft          = QStringLiteral("rgba(91,155,213,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(91,155,213,0.55)");
        t.editorBackground = QStringLiteral("#f5f0e8");  // editor CLARO — inversão
        t.editorTextColor  = QStringLiteral("#2a3040");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,200)");
        t.pageShadowRadius  = 28;
        t.pageShadowOffset  = 6;
        m_themes.append(t);
    }
    {
        // Papyrus — manuscrito medieval em pergaminho real.
        // Amarelo queimado envelhecido, tinta ferrogálica marrom-escura, ouro velho.
        MiraTheme t;
        t.id   = QStringLiteral("papyrus");
        t.name = QStringLiteral("Papyrus");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#e8d8a0");
        t.panelBackground  = QStringLiteral("#e0d090");
        t.panelBorder      = QStringLiteral("#b8a060");
        t.textPrimary      = QStringLiteral("#3a2010");
        t.textMuted        = QStringLiteral("#7a5830");
        t.textBright       = QStringLiteral("#1a0c04");
        t.hoverOverlay     = QStringLiteral("rgba(58,32,16,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(58,32,16,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(58,32,16,0.10)");
        t.accentDefault    = QStringLiteral("#8b5e00");
        t.hoverStrong      = QStringLiteral("rgba(58,32,16,0.12)");
        t.borderStrong     = QStringLiteral("rgba(58,32,16,0.22)");
        t.focusBorder      = QStringLiteral("rgba(58,32,16,0.32)");
        t.inputBackground  = QStringLiteral("rgba(58,32,16,0.06)");
        t.disabledText     = QStringLiteral("rgba(58,32,16,0.30)");
        t.selectionRing    = QStringLiteral("#8b5e00");
        t.accentSuccess           = QStringLiteral("#4a7a30");
        t.accentSuccessSoft       = QStringLiteral("rgba(74,122,48,0.16)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(74,122,48,0.45)");
        t.accentDanger            = QStringLiteral("#8a2010");
        t.accentDangerSoft        = QStringLiteral("rgba(138,32,16,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(138,32,16,0.45)");
        t.accentWarning           = QStringLiteral("#8b5e00");
        t.accentInfo              = QStringLiteral("#2a4a7a");
        t.accentInfoSoft          = QStringLiteral("rgba(42,74,122,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(42,74,122,0.45)");
        t.editorBackground = QStringLiteral("#f0e4b0");
        t.editorTextColor  = QStringLiteral("#2e1a08");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(80,40,0,100)");
        t.pageShadowRadius  = 18;
        t.pageShadowOffset  = 4;
        m_themes.append(t);
    }
    {
        // VHS — fita magnética anos 80. Azul-marinho saturado, texto branco levemente cyan,
        // accent magenta elétrico. Estética de videoclipe de cassete.
        MiraTheme t;
        t.id   = QStringLiteral("vhs");
        t.name = QStringLiteral("VHS");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#0a0a1e");
        t.panelBackground  = QStringLiteral("#0d0d24");
        t.panelBorder      = QStringLiteral("#1e1e44");
        t.textPrimary      = QStringLiteral("#c8d8f0");
        t.textMuted        = QStringLiteral("#5060a0");
        t.textBright       = QStringLiteral("#e0eeff");
        t.hoverOverlay     = QStringLiteral("rgba(200,216,240,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(200,216,240,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(200,216,240,0.10)");
        t.accentDefault    = QStringLiteral("#ff00cc");
        t.hoverStrong      = QStringLiteral("rgba(200,216,240,0.12)");
        t.borderStrong     = QStringLiteral("rgba(200,216,240,0.22)");
        t.focusBorder      = QStringLiteral("rgba(255,0,204,0.40)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.35)");
        t.disabledText     = QStringLiteral("rgba(200,216,240,0.30)");
        t.selectionRing    = QStringLiteral("#ff00cc");
        t.accentSuccess           = QStringLiteral("#00ffcc");
        t.accentSuccessSoft       = QStringLiteral("rgba(0,255,204,0.16)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(0,255,204,0.45)");
        t.accentDanger            = QStringLiteral("#ff2244");
        t.accentDangerSoft        = QStringLiteral("rgba(255,34,68,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(255,34,68,0.45)");
        t.accentWarning           = QStringLiteral("#ffcc00");
        t.accentInfo              = QStringLiteral("#00ccff");
        t.accentInfoSoft          = QStringLiteral("rgba(0,204,255,0.20)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(0,204,255,0.50)");
        t.editorBackground = QStringLiteral("#08081a");
        t.editorTextColor  = QStringLiteral("#ccd8f0");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,40,220)");
        t.pageShadowRadius  = 24;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }
    {
        // Wabi-Sabi — estética japonesa da beleza imperfeita.
        // Cinza neutro frio, quase monocromático, com um único accent musgo discreto.
        // Sem nada gritante: silêncio visual.
        MiraTheme t;
        t.id   = QStringLiteral("wabi-sabi");
        t.name = QStringLiteral("Wabi-Sabi");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#2c2c28");
        t.panelBackground  = QStringLiteral("#303030");
        t.panelBorder      = QStringLiteral("#484840");
        t.textPrimary      = QStringLiteral("#b8b0a0");
        t.textMuted        = QStringLiteral("#686058");
        t.textBright       = QStringLiteral("#d0c8b8");
        t.hoverOverlay     = QStringLiteral("rgba(184,176,160,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(184,176,160,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(184,176,160,0.10)");
        t.accentDefault    = QStringLiteral("#7a8a6a");
        t.hoverStrong      = QStringLiteral("rgba(184,176,160,0.12)");
        t.borderStrong     = QStringLiteral("rgba(184,176,160,0.22)");
        t.focusBorder      = QStringLiteral("rgba(184,176,160,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.28)");
        t.disabledText     = QStringLiteral("rgba(184,176,160,0.30)");
        t.selectionRing    = QStringLiteral("#b8b0a0");
        t.accentSuccess           = QStringLiteral("#7a8a6a");
        t.accentSuccessSoft       = QStringLiteral("rgba(122,138,106,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(122,138,106,0.50)");
        t.accentDanger            = QStringLiteral("#9a6050");
        t.accentDangerSoft        = QStringLiteral("rgba(154,96,80,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(154,96,80,0.50)");
        t.accentWarning           = QStringLiteral("#9a8860");
        t.accentInfo              = QStringLiteral("#607080");
        t.accentInfoSoft          = QStringLiteral("rgba(96,112,128,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(96,112,128,0.55)");
        t.editorBackground = QStringLiteral("#2a2a26");
        t.editorTextColor  = QStringLiteral("#b8b0a0");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,160)");
        t.pageShadowRadius  = 18;
        t.pageShadowOffset  = 3;
        m_themes.append(t);
    }
    {
        // Manuscrito — caderno de anotações em papel azul-pálido (papier azuré).
        // Fundo azul giz muito suave, texto grafite-escuro, accent tinta-azul real.
        MiraTheme t;
        t.id   = QStringLiteral("manuscrito");
        t.name = QStringLiteral("Manuscrito");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#dce8f0");
        t.panelBackground  = QStringLiteral("#d4e2ec");
        t.panelBorder      = QStringLiteral("#a8c0d4");
        t.textPrimary      = QStringLiteral("#2a3040");
        t.textMuted        = QStringLiteral("#6070a0");
        t.textBright       = QStringLiteral("#0a1428");
        t.hoverOverlay     = QStringLiteral("rgba(42,48,64,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(42,48,64,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(42,48,64,0.10)");
        t.accentDefault    = QStringLiteral("#1a4a8a");
        t.hoverStrong      = QStringLiteral("rgba(42,48,64,0.12)");
        t.borderStrong     = QStringLiteral("rgba(42,48,64,0.22)");
        t.focusBorder      = QStringLiteral("rgba(26,74,138,0.40)");
        t.inputBackground  = QStringLiteral("rgba(42,48,64,0.05)");
        t.disabledText     = QStringLiteral("rgba(42,48,64,0.30)");
        t.selectionRing    = QStringLiteral("#1a4a8a");
        t.accentSuccess           = QStringLiteral("#2a6a40");
        t.accentSuccessSoft       = QStringLiteral("rgba(42,106,64,0.16)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(42,106,64,0.45)");
        t.accentDanger            = QStringLiteral("#8a2020");
        t.accentDangerSoft        = QStringLiteral("rgba(138,32,32,0.12)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(138,32,32,0.45)");
        t.accentWarning           = QStringLiteral("#7a5000");
        t.accentInfo              = QStringLiteral("#1a4a8a");
        t.accentInfoSoft          = QStringLiteral("rgba(26,74,138,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(26,74,138,0.45)");
        t.editorBackground = QStringLiteral("#eef4f8");
        t.editorTextColor  = QStringLiteral("#1e2838");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(20,40,80,80)");
        t.pageShadowRadius  = 18;
        t.pageShadowOffset  = 4;
        m_themes.append(t);
    }
    {
        // Limão — monocromático extremo: preto absoluto + verde-limão saturado.
        // Nenhuma cor intermediária. Ousado, inesperado, impossível de ignorar.
        MiraTheme t;
        t.id   = QStringLiteral("limao");
        t.name = QStringLiteral("Limão");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#050500");
        t.panelBackground  = QStringLiteral("#080800");
        t.panelBorder      = QStringLiteral("#1a1a00");
        t.textPrimary      = QStringLiteral("#ccee00");
        t.textMuted        = QStringLiteral("#667700");
        t.textBright       = QStringLiteral("#eeff00");
        t.hoverOverlay     = QStringLiteral("rgba(204,238,0,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(204,238,0,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(204,238,0,0.10)");
        t.accentDefault    = QStringLiteral("#ccee00");
        t.hoverStrong      = QStringLiteral("rgba(204,238,0,0.14)");
        t.borderStrong     = QStringLiteral("rgba(204,238,0,0.24)");
        t.focusBorder      = QStringLiteral("rgba(204,238,0,0.40)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.40)");
        t.disabledText     = QStringLiteral("rgba(204,238,0,0.28)");
        t.selectionRing    = QStringLiteral("#ccee00");
        t.accentSuccess           = QStringLiteral("#88cc00");
        t.accentSuccessSoft       = QStringLiteral("rgba(136,204,0,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(136,204,0,0.50)");
        t.accentDanger            = QStringLiteral("#ee4400");
        t.accentDangerSoft        = QStringLiteral("rgba(238,68,0,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(238,68,0,0.50)");
        t.accentWarning           = QStringLiteral("#ddaa00");
        t.accentInfo              = QStringLiteral("#44ee88");
        t.accentInfoSoft          = QStringLiteral("rgba(68,238,136,0.20)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(68,238,136,0.50)");
        t.editorBackground = QStringLiteral("#030300");
        t.editorTextColor  = QStringLiteral("#ccee00");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,20,0,220)");
        t.pageShadowRadius  = 20;
        t.pageShadowOffset  = 4;
        m_themes.append(t);
    }
    {
        // Brutalismo — web brutalismo europeu. Branco de cal cru, preto absoluto,
        // accent amarelo saturado agressivo. Sem arredondamentos visuais na alma.
        MiraTheme t;
        t.id   = QStringLiteral("brutalismo");
        t.name = QStringLiteral("Brutalismo");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#f0ece0");
        t.panelBackground  = QStringLiteral("#e8e4d8");
        t.panelBorder      = QStringLiteral("#b0a880");
        t.textPrimary      = QStringLiteral("#0a0a0a");
        t.textMuted        = QStringLiteral("#505040");
        t.textBright       = QStringLiteral("#000000");
        t.hoverOverlay     = QStringLiteral("rgba(0,0,0,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(0,0,0,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(0,0,0,0.12)");
        t.accentDefault    = QStringLiteral("#ddcc00");
        t.hoverStrong      = QStringLiteral("rgba(0,0,0,0.14)");
        t.borderStrong     = QStringLiteral("rgba(0,0,0,0.28)");
        t.focusBorder      = QStringLiteral("rgba(221,204,0,0.60)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.04)");
        t.disabledText     = QStringLiteral("rgba(0,0,0,0.28)");
        t.selectionRing    = QStringLiteral("#ddcc00");
        t.accentSuccess           = QStringLiteral("#228800");
        t.accentSuccessSoft       = QStringLiteral("rgba(34,136,0,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(34,136,0,0.45)");
        t.accentDanger            = QStringLiteral("#dd2200");
        t.accentDangerSoft        = QStringLiteral("rgba(221,34,0,0.12)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(221,34,0,0.45)");
        t.accentWarning           = QStringLiteral("#ddcc00");
        t.accentInfo              = QStringLiteral("#0044cc");
        t.accentInfoSoft          = QStringLiteral("rgba(0,68,204,0.12)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(0,68,204,0.40)");
        t.editorBackground = QStringLiteral("#faf8f0");
        t.editorTextColor  = QStringLiteral("#0a0a0a");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,70)");
        t.pageShadowRadius  = 14;
        t.pageShadowOffset  = 3;
        m_themes.append(t);
    }

    // ---- Novos temas (Gruvbox, Tokyo, Meia-Noite, Everforest, One Dark) ----
    {
        // Gruvbox Claro — variante clara oficial do Gruvbox. Creme-marfim quente,
        // texto sépia escuro, accent amarelo-terra. Máquina de escrever diurna.
        MiraTheme t;
        t.id   = QStringLiteral("gruvbox-light");
        t.name = QStringLiteral("Gruvbox Claro");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#ebdbb2");  // bg1
        t.panelBackground  = QStringLiteral("#fbf1c7");  // bg0
        t.panelBorder      = QStringLiteral("#d5c4a1");  // bg2
        t.textPrimary      = QStringLiteral("#504945");  // fg2
        t.textMuted        = QStringLiteral("#928374");  // gray
        t.textBright       = QStringLiteral("#282828");  // fg0
        t.hoverOverlay     = QStringLiteral("rgba(40,40,40,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(40,40,40,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(40,40,40,0.14)");
        t.accentDefault    = QStringLiteral("#b57614");  // yellow escuro pra ter contraste
        t.hoverStrong      = QStringLiteral("rgba(40,40,40,0.10)");
        t.borderStrong     = QStringLiteral("rgba(40,40,40,0.24)");
        t.focusBorder      = QStringLiteral("rgba(40,40,40,0.34)");
        t.inputBackground  = QStringLiteral("rgba(40,40,40,0.05)");
        t.disabledText     = QStringLiteral("rgba(40,40,40,0.32)");
        t.selectionRing    = QStringLiteral("#282828");
        t.accentSuccess           = QStringLiteral("#79740e");  // green
        t.accentSuccessSoft       = QStringLiteral("rgba(121,116,14,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(121,116,14,0.45)");
        t.accentDanger            = QStringLiteral("#9d0006");  // red
        t.accentDangerSoft        = QStringLiteral("rgba(157,0,6,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(157,0,6,0.45)");
        t.accentWarning           = QStringLiteral("#af3a03");  // orange
        t.accentInfo              = QStringLiteral("#076678");  // blue
        t.accentInfoSoft          = QStringLiteral("rgba(7,102,120,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(7,102,120,0.55)");
        t.editorBackground = QStringLiteral("#f9f5d7");  // bg0_s
        t.editorTextColor  = QStringLiteral("#3c3836");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(40,40,40,85)");
        t.pageShadowRadius  = 26;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }
    {
        // Gruvbox Duro — variante hard do Gruvbox Dark. Fundo bg0_h mais escuro
        // que o Gruvbox padrão, contraste máximo dentro da família.
        MiraTheme t;
        t.id   = QStringLiteral("gruvbox-hard");
        t.name = QStringLiteral("Gruvbox Duro");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#1d2021");  // bg0_h
        t.panelBackground  = QStringLiteral("#282828");  // bg0
        t.panelBorder      = QStringLiteral("#3c3836");  // bg1
        t.textPrimary      = QStringLiteral("#ebdbb2");  // fg1
        t.textMuted        = QStringLiteral("#928374");  // gray
        t.textBright       = QStringLiteral("#fbf1c7");  // fg0
        t.hoverOverlay     = QStringLiteral("rgba(235,219,178,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(235,219,178,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(235,219,178,0.10)");
        t.accentDefault    = QStringLiteral("#d79921");  // yellow
        t.hoverStrong      = QStringLiteral("rgba(235,219,178,0.12)");
        t.borderStrong     = QStringLiteral("rgba(235,219,178,0.22)");
        t.focusBorder      = QStringLiteral("rgba(235,219,178,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText     = QStringLiteral("rgba(235,219,178,0.30)");
        t.selectionRing    = QStringLiteral("#fbf1c7");
        t.accentSuccess           = QStringLiteral("#98971a");  // green
        t.accentSuccessSoft       = QStringLiteral("rgba(152,151,26,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(152,151,26,0.50)");
        t.accentDanger            = QStringLiteral("#cc241d");  // red
        t.accentDangerSoft        = QStringLiteral("rgba(204,36,29,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(204,36,29,0.50)");
        t.accentWarning           = QStringLiteral("#d65d0e");  // orange
        t.accentInfo              = QStringLiteral("#458588");  // blue
        t.accentInfoSoft          = QStringLiteral("rgba(69,133,136,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(69,133,136,0.55)");
        t.editorBackground = QStringLiteral("#1d2021");
        t.editorTextColor  = QStringLiteral("#ebdbb2");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,180)");
        t.pageShadowRadius  = 24;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }
    {
        // Meia-Noite — navy escuro puro. Azul-meia-noite sobre preto-azulado,
        // accent azul-elétrico frio. Concentração máxima, sem drama.
        MiraTheme t;
        t.id   = QStringLiteral("meia-noite");
        t.name = QStringLiteral("Meia-Noite");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#080c14");
        t.panelBackground  = QStringLiteral("#0e1220");
        t.panelBorder      = QStringLiteral("#1a2236");
        t.textPrimary      = QStringLiteral("#c8d2e0");
        t.textMuted        = QStringLiteral("#4a5268");
        t.textBright       = QStringLiteral("#e8eef8");
        t.hoverOverlay     = QStringLiteral("rgba(200,210,224,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(200,210,224,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(200,210,224,0.10)");
        t.accentDefault    = QStringLiteral("#5278c8");
        t.hoverStrong      = QStringLiteral("rgba(200,210,224,0.12)");
        t.borderStrong     = QStringLiteral("rgba(200,210,224,0.22)");
        t.focusBorder      = QStringLiteral("rgba(200,210,224,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.35)");
        t.disabledText     = QStringLiteral("rgba(200,210,224,0.30)");
        t.selectionRing    = QStringLiteral("#e8eef8");
        t.accentSuccess           = QStringLiteral("#5aa880");
        t.accentSuccessSoft       = QStringLiteral("rgba(90,168,128,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(90,168,128,0.50)");
        t.accentDanger            = QStringLiteral("#c84858");
        t.accentDangerSoft        = QStringLiteral("rgba(200,72,88,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(200,72,88,0.50)");
        t.accentWarning           = QStringLiteral("#c89848");
        t.accentInfo              = QStringLiteral("#4a8ad0");
        t.accentInfoSoft          = QStringLiteral("rgba(74,138,208,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(74,138,208,0.55)");
        t.editorBackground = QStringLiteral("#0a0f1c");
        t.editorTextColor  = QStringLiteral("#d0dae8");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,200)");
        t.pageShadowRadius  = 28;
        t.pageShadowOffset  = 6;
        m_themes.append(t);
    }
    {
        // Meia-Noite Índigo — mesma família midnight mas com roxo-índigo profundo.
        // Accent violeta-elétrico, painéis azul-púrpura escuro.
        MiraTheme t;
        t.id   = QStringLiteral("meia-noite-indigo");
        t.name = QStringLiteral("Meia-Noite Índigo");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#090810");
        t.panelBackground  = QStringLiteral("#100e1a");
        t.panelBorder      = QStringLiteral("#1e1a30");
        t.textPrimary      = QStringLiteral("#c8c0e0");
        t.textMuted        = QStringLiteral("#4e4870");
        t.textBright       = QStringLiteral("#e8e0f8");
        t.hoverOverlay     = QStringLiteral("rgba(200,192,224,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(200,192,224,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(200,192,224,0.10)");
        t.accentDefault    = QStringLiteral("#7060d8");
        t.hoverStrong      = QStringLiteral("rgba(200,192,224,0.12)");
        t.borderStrong     = QStringLiteral("rgba(200,192,224,0.22)");
        t.focusBorder      = QStringLiteral("rgba(200,192,224,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.35)");
        t.disabledText     = QStringLiteral("rgba(200,192,224,0.30)");
        t.selectionRing    = QStringLiteral("#e8e0f8");
        t.accentSuccess           = QStringLiteral("#60a888");
        t.accentSuccessSoft       = QStringLiteral("rgba(96,168,136,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(96,168,136,0.50)");
        t.accentDanger            = QStringLiteral("#c85868");
        t.accentDangerSoft        = QStringLiteral("rgba(200,88,104,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(200,88,104,0.50)");
        t.accentWarning           = QStringLiteral("#c09040");
        t.accentInfo              = QStringLiteral("#8890d8");
        t.accentInfoSoft          = QStringLiteral("rgba(136,144,216,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(136,144,216,0.55)");
        t.editorBackground = QStringLiteral("#0b0916");
        t.editorTextColor  = QStringLiteral("#d4cce8");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,200)");
        t.pageShadowRadius  = 28;
        t.pageShadowOffset  = 6;
        m_themes.append(t);
    }
    {
        // Meia-Noite Carmesim — midnight com acento carmesim neon. Tensão,
        // urgência, thriller. Preto-vinho com faísca vermelha.
        MiraTheme t;
        t.id   = QStringLiteral("meia-noite-carmesim");
        t.name = QStringLiteral("Meia-Noite Carmesim");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#0e0810");
        t.panelBackground  = QStringLiteral("#160c16");
        t.panelBorder      = QStringLiteral("#2c1828");
        t.textPrimary      = QStringLiteral("#d8c0cc");
        t.textMuted        = QStringLiteral("#6a4858");
        t.textBright       = QStringLiteral("#f0e0e8");
        t.hoverOverlay     = QStringLiteral("rgba(216,192,204,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(216,192,204,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(216,192,204,0.10)");
        t.accentDefault    = QStringLiteral("#d84070");
        t.hoverStrong      = QStringLiteral("rgba(216,192,204,0.12)");
        t.borderStrong     = QStringLiteral("rgba(216,192,204,0.22)");
        t.focusBorder      = QStringLiteral("rgba(216,192,204,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.35)");
        t.disabledText     = QStringLiteral("rgba(216,192,204,0.30)");
        t.selectionRing    = QStringLiteral("#f0e0e8");
        t.accentSuccess           = QStringLiteral("#70a880");
        t.accentSuccessSoft       = QStringLiteral("rgba(112,168,128,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(112,168,128,0.50)");
        t.accentDanger            = QStringLiteral("#e04060");
        t.accentDangerSoft        = QStringLiteral("rgba(224,64,96,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(224,64,96,0.50)");
        t.accentWarning           = QStringLiteral("#d08040");
        t.accentInfo              = QStringLiteral("#8888c8");
        t.accentInfoSoft          = QStringLiteral("rgba(136,136,200,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(136,136,200,0.55)");
        t.editorBackground = QStringLiteral("#0f0912");
        t.editorTextColor  = QStringLiteral("#e0ccd8");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,210)");
        t.pageShadowRadius  = 30;
        t.pageShadowOffset  = 7;
        m_themes.append(t);
    }
    {
        // Madrugada — pré-amanhecer monocromático. Azul-cinza fundo, texto
        // quase branco. O mais contido da família noturna. Escrita silenciosa.
        MiraTheme t;
        t.id   = QStringLiteral("madrugada");
        t.name = QStringLiteral("Madrugada");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#0c0e16");
        t.panelBackground  = QStringLiteral("#121520");
        t.panelBorder      = QStringLiteral("#1e2232");
        t.textPrimary      = QStringLiteral("#b8bcd0");
        t.textMuted        = QStringLiteral("#484c64");
        t.textBright       = QStringLiteral("#d8dcf0");
        t.hoverOverlay     = QStringLiteral("rgba(184,188,208,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(184,188,208,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(184,188,208,0.10)");
        t.accentDefault    = QStringLiteral("#6878b8");
        t.hoverStrong      = QStringLiteral("rgba(184,188,208,0.12)");
        t.borderStrong     = QStringLiteral("rgba(184,188,208,0.20)");
        t.focusBorder      = QStringLiteral("rgba(184,188,208,0.30)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.30)");
        t.disabledText     = QStringLiteral("rgba(184,188,208,0.30)");
        t.selectionRing    = QStringLiteral("#d8dcf0");
        t.accentSuccess           = QStringLiteral("#5a9070");
        t.accentSuccessSoft       = QStringLiteral("rgba(90,144,112,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(90,144,112,0.50)");
        t.accentDanger            = QStringLiteral("#b85060");
        t.accentDangerSoft        = QStringLiteral("rgba(184,80,96,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(184,80,96,0.50)");
        t.accentWarning           = QStringLiteral("#b88850");
        t.accentInfo              = QStringLiteral("#5870a8");
        t.accentInfoSoft          = QStringLiteral("rgba(88,112,168,0.20)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(88,112,168,0.55)");
        t.editorBackground = QStringLiteral("#0e111a");
        t.editorTextColor  = QStringLiteral("#c8ccdf");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,190)");
        t.pageShadowRadius  = 26;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }
    {
        // Tokyo Storm — variante mais saturada e tempestuosa do Tokyo Night.
        // Fundo um tom mais claro, accent azul mais vibrante. Mais energia.
        MiraTheme t;
        t.id   = QStringLiteral("tokyo-storm");
        t.name = QStringLiteral("Tokyo Storm");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#24283b");  // panel do TN vira bg
        t.panelBackground  = QStringLiteral("#2f3549");
        t.panelBorder      = QStringLiteral("#454870");
        t.textPrimary      = QStringLiteral("#b4bcdb");
        t.textMuted        = QStringLiteral("#606890");
        t.textBright       = QStringLiteral("#ccd4f5");
        t.hoverOverlay     = QStringLiteral("rgba(204,212,245,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(204,212,245,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(204,212,245,0.10)");
        t.accentDefault    = QStringLiteral("#90b4f8");
        t.hoverStrong      = QStringLiteral("rgba(204,212,245,0.12)");
        t.borderStrong     = QStringLiteral("rgba(204,212,245,0.22)");
        t.focusBorder      = QStringLiteral("rgba(204,212,245,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.25)");
        t.disabledText     = QStringLiteral("rgba(204,212,245,0.30)");
        t.selectionRing    = QStringLiteral("#ccd4f5");
        t.accentSuccess           = QStringLiteral("#9ece6a");
        t.accentSuccessSoft       = QStringLiteral("rgba(158,206,106,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(158,206,106,0.55)");
        t.accentDanger            = QStringLiteral("#f7768e");
        t.accentDangerSoft        = QStringLiteral("rgba(247,118,142,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(247,118,142,0.50)");
        t.accentWarning           = QStringLiteral("#ff9e64");
        t.accentInfo              = QStringLiteral("#bb9af7");
        t.accentInfoSoft          = QStringLiteral("rgba(187,154,247,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(187,154,247,0.55)");
        t.editorBackground = QStringLiteral("#292d3e");
        t.editorTextColor  = QStringLiteral("#c0caf5");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,160)");
        t.pageShadowRadius  = 24;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }
    {
        // Tokyo Day — variante clara da família Tokyo Night. Lavanda suave sobre
        // branco-azulado, accent azul-royal. Produtividade diurna, mesma vibe urbana.
        MiraTheme t;
        t.id   = QStringLiteral("tokyo-day");
        t.name = QStringLiteral("Tokyo Day");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#d5d6db");
        t.panelBackground  = QStringLiteral("#e1e2e7");
        t.panelBorder      = QStringLiteral("#b8bac4");
        t.textPrimary      = QStringLiteral("#343b58");
        t.textMuted        = QStringLiteral("#6a6f8a");
        t.textBright       = QStringLiteral("#1f2336");
        t.hoverOverlay     = QStringLiteral("rgba(31,35,54,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(31,35,54,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(31,35,54,0.12)");
        t.accentDefault    = QStringLiteral("#2e5fbe");
        t.hoverStrong      = QStringLiteral("rgba(31,35,54,0.10)");
        t.borderStrong     = QStringLiteral("rgba(31,35,54,0.22)");
        t.focusBorder      = QStringLiteral("rgba(31,35,54,0.32)");
        t.inputBackground  = QStringLiteral("rgba(31,35,54,0.04)");
        t.disabledText     = QStringLiteral("rgba(31,35,54,0.32)");
        t.selectionRing    = QStringLiteral("#1f2336");
        t.accentSuccess           = QStringLiteral("#4d8a3a");
        t.accentSuccessSoft       = QStringLiteral("rgba(77,138,58,0.14)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(77,138,58,0.50)");
        t.accentDanger            = QStringLiteral("#b03050");
        t.accentDangerSoft        = QStringLiteral("rgba(176,48,80,0.10)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(176,48,80,0.50)");
        t.accentWarning           = QStringLiteral("#c06820");
        t.accentInfo              = QStringLiteral("#6040a8");
        t.accentInfoSoft          = QStringLiteral("rgba(96,64,168,0.16)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(96,64,168,0.55)");
        t.editorBackground = QStringLiteral("#f5f6fa");
        t.editorTextColor  = QStringLiteral("#1f2336");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(31,35,54,80)");
        t.pageShadowRadius  = 28;
        t.pageShadowOffset  = 6;
        m_themes.append(t);
    }
    {
        // Everforest — floresta perene escura (sainnhe/everforest). Verde-musgo
        // sobre cinza-esverdeado. Accent verde-folha. Calm, natureza, foco.
        MiraTheme t;
        t.id   = QStringLiteral("everforest");
        t.name = QStringLiteral("Everforest");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#2d353b");
        t.panelBackground  = QStringLiteral("#343f44");
        t.panelBorder      = QStringLiteral("#475258");
        t.textPrimary      = QStringLiteral("#d3c6aa");
        t.textMuted        = QStringLiteral("#7a8478");
        t.textBright       = QStringLiteral("#fdf6e3");
        t.hoverOverlay     = QStringLiteral("rgba(211,198,170,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(211,198,170,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(211,198,170,0.10)");
        t.accentDefault    = QStringLiteral("#83c092");  // green
        t.hoverStrong      = QStringLiteral("rgba(211,198,170,0.12)");
        t.borderStrong     = QStringLiteral("rgba(211,198,170,0.22)");
        t.focusBorder      = QStringLiteral("rgba(211,198,170,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.22)");
        t.disabledText     = QStringLiteral("rgba(211,198,170,0.30)");
        t.selectionRing    = QStringLiteral("#fdf6e3");
        t.accentSuccess           = QStringLiteral("#a7c080");  // green2
        t.accentSuccessSoft       = QStringLiteral("rgba(167,192,128,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(167,192,128,0.50)");
        t.accentDanger            = QStringLiteral("#e67e80");  // red
        t.accentDangerSoft        = QStringLiteral("rgba(230,126,128,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(230,126,128,0.50)");
        t.accentWarning           = QStringLiteral("#e69875");  // orange
        t.accentInfo              = QStringLiteral("#7fbbb3");  // blue
        t.accentInfoSoft          = QStringLiteral("rgba(127,187,179,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(127,187,179,0.55)");
        t.editorBackground = QStringLiteral("#2d353b");
        t.editorTextColor  = QStringLiteral("#d3c6aa");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,160)");
        t.pageShadowRadius  = 22;
        t.pageShadowOffset  = 4;
        m_themes.append(t);
    }
    {
        // One Dark — Atom One Dark Pro. Cinza-azulado fundo, texto lavanda
        // suave, accent azul-ciano. O clássico dos editores de código.
        MiraTheme t;
        t.id   = QStringLiteral("one-dark");
        t.name = QStringLiteral("One Dark");
        t.bundled = true;
        t.appBackground    = QStringLiteral("#21252b");
        t.panelBackground  = QStringLiteral("#282c34");
        t.panelBorder      = QStringLiteral("#3e4452");
        t.textPrimary      = QStringLiteral("#abb2bf");
        t.textMuted        = QStringLiteral("#5c6370");
        t.textBright       = QStringLiteral("#dde4f0");
        t.hoverOverlay     = QStringLiteral("rgba(171,178,191,0.06)");
        t.pressedOverlay   = QStringLiteral("rgba(171,178,191,0.04)");
        t.subtleBorder     = QStringLiteral("rgba(171,178,191,0.10)");
        t.accentDefault    = QStringLiteral("#61afef");  // blue
        t.hoverStrong      = QStringLiteral("rgba(171,178,191,0.12)");
        t.borderStrong     = QStringLiteral("rgba(171,178,191,0.22)");
        t.focusBorder      = QStringLiteral("rgba(171,178,191,0.32)");
        t.inputBackground  = QStringLiteral("rgba(0,0,0,0.25)");
        t.disabledText     = QStringLiteral("rgba(171,178,191,0.30)");
        t.selectionRing    = QStringLiteral("#dde4f0");
        t.accentSuccess           = QStringLiteral("#98c379");  // green
        t.accentSuccessSoft       = QStringLiteral("rgba(152,195,121,0.18)");
        t.accentSuccessBorderSoft = QStringLiteral("rgba(152,195,121,0.55)");
        t.accentDanger            = QStringLiteral("#e06c75");  // red
        t.accentDangerSoft        = QStringLiteral("rgba(224,108,117,0.14)");
        t.accentDangerBorderSoft  = QStringLiteral("rgba(224,108,117,0.50)");
        t.accentWarning           = QStringLiteral("#e5c07b");  // yellow
        t.accentInfo              = QStringLiteral("#c678dd");  // purple
        t.accentInfoSoft          = QStringLiteral("rgba(198,120,221,0.22)");
        t.accentInfoBorderSoft    = QStringLiteral("rgba(198,120,221,0.55)");
        t.editorBackground = QStringLiteral("#282c34");
        t.editorTextColor  = QStringLiteral("#abb2bf");
        t.pageShadowEnabled = true;
        t.pageShadowColor   = QStringLiteral("rgba(0,0,0,170)");
        t.pageShadowRadius  = 24;
        t.pageShadowOffset  = 5;
        m_themes.append(t);
    }

    // ---- Categorias pro filtro do painel de Temas ----
    // light = claros neutros/frios | warm = amarelados/quentes |
    // dark = escuros neutros | colorful = paletas vibrantes (azul/verde/roxo…)
    static const QHash<QString, QString> kCategory = {
        { QStringLiteral("full-white"),       QStringLiteral("light") },
        { QStringLiteral("solarized-light"),  QStringLiteral("light") },
        { QStringLiteral("nord-light"),       QStringLiteral("light") },
        { QStringLiteral("marble"),           QStringLiteral("light") },
        { QStringLiteral("brisa"),            QStringLiteral("estampados") },
        { QStringLiteral("sepia"),            QStringLiteral("warm") },
        { QStringLiteral("amber"),            QStringLiteral("warm") },
        { QStringLiteral("parchment"),        QStringLiteral("warm") },
        { QStringLiteral("caramel"),          QStringLiteral("warm") },
        { QStringLiteral("camafeu"),          QStringLiteral("warm") },
        { QStringLiteral("cafe"),             QStringLiteral("warm") },
        { QStringLiteral("tangerine"),        QStringLiteral("warm") },
        { QStringLiteral("gruvbox-dark"),     QStringLiteral("warm") },
        { QStringLiteral("amber-night"),      QStringLiteral("warm") },
        { QStringLiteral("wheat"),            QStringLiteral("warm") },
        { QStringLiteral("full-black"),       QStringLiteral("dark") },
        { QStringLiteral("solarized-dark"),   QStringLiteral("dark") },
        { QStringLiteral("nord"),             QStringLiteral("dark") },
        { QStringLiteral("high-contrast"),    QStringLiteral("dark") },
        { QStringLiteral("tokyo-night"),      QStringLiteral("dark") },
        { QStringLiteral("cerracao"),         QStringLiteral("estampados") },
        { QStringLiteral("petroleo"),         QStringLiteral("estampados") },
        { QStringLiteral("dracula"),          QStringLiteral("colorful") },
        { QStringLiteral("catppuccin-mocha"), QStringLiteral("colorful") },
        { QStringLiteral("nord-royal"),       QStringLiteral("colorful") },
        { QStringLiteral("velvet-red"),       QStringLiteral("colorful") },
        { QStringLiteral("veredas"),          QStringLiteral("estampados") },
        { QStringLiteral("tokyo-velvet"),     QStringLiteral("estampados") },
        { QStringLiteral("constelar"),        QStringLiteral("estampados") },
        { QStringLiteral("mare"),             QStringLiteral("estampados") },
        { QStringLiteral("rose-pine"),        QStringLiteral("colorful") },
        { QStringLiteral("aurora"),           QStringLiteral("estampados") },
        { QStringLiteral("hibisco"),          QStringLiteral("estampados") },
        { QStringLiteral("ocean"),            QStringLiteral("colorful") },
        { QStringLiteral("forest"),           QStringLiteral("colorful") },
        { QStringLiteral("amethyst"),         QStringLiteral("colorful") },
        { QStringLiteral("lavender"),         QStringLiteral("colorful") },
        { QStringLiteral("emerald"),          QStringLiteral("colorful") },
        { QStringLiteral("nevoa"),            QStringLiteral("light") },
        { QStringLiteral("ice"),              QStringLiteral("light") },
        { QStringLiteral("sage"),             QStringLiteral("light") },
        { QStringLiteral("quartz"),           QStringLiteral("light") },
        { QStringLiteral("pearl"),            QStringLiteral("light") },
        { QStringLiteral("cinnamon"),         QStringLiteral("warm") },
        { QStringLiteral("mustard"),          QStringLiteral("warm") },
        { QStringLiteral("terracotta"),       QStringLiteral("warm") },
        { QStringLiteral("apricot"),          QStringLiteral("warm") },
        { QStringLiteral("cognac"),           QStringLiteral("warm") },
        { QStringLiteral("charcoal"),         QStringLiteral("dark") },
        { QStringLiteral("slate"),            QStringLiteral("dark") },
        { QStringLiteral("onyx"),             QStringLiteral("dark") },
        { QStringLiteral("lead"),             QStringLiteral("dark") },
        { QStringLiteral("midnight"),         QStringLiteral("dark") },
        { QStringLiteral("fuchsia"),          QStringLiteral("colorful") },
        { QStringLiteral("volcano"),          QStringLiteral("colorful") },
        { QStringLiteral("turquoise"),        QStringLiteral("colorful") },
        { QStringLiteral("neon"),             QStringLiteral("colorful") },
        { QStringLiteral("burgundy"),         QStringLiteral("colorful") },
        { QStringLiteral("ink"),              QStringLiteral("light") },
        { QStringLiteral("sumi"),             QStringLiteral("dark") },
        { QStringLiteral("firefly"),          QStringLiteral("colorful") },
        { QStringLiteral("halo"),             QStringLiteral("colorful") },
        { QStringLiteral("sakura"),           QStringLiteral("colorful") },
        { QStringLiteral("copper"),           QStringLiteral("warm") },
        { QStringLiteral("mint"),             QStringLiteral("colorful") },
        { QStringLiteral("eclipse"),          QStringLiteral("dark") },
        { QStringLiteral("deep-night"),       QStringLiteral("dark") },
        { QStringLiteral("stage"),            QStringLiteral("dark") },
        { QStringLiteral("inverso"),          QStringLiteral("light") },
        { QStringLiteral("anaglyph"),         QStringLiteral("colorful") },
        { QStringLiteral("voltage"),          QStringLiteral("colorful") },
        { QStringLiteral("outrun"),           QStringLiteral("colorful") },
        { QStringLiteral("phosphor"),         QStringLiteral("colorful") },
        { QStringLiteral("phosphor-amber"),   QStringLiteral("warm") },
        { QStringLiteral("siren"),            QStringLiteral("colorful") },
        { QStringLiteral("ember-red"),        QStringLiteral("colorful") },
        { QStringLiteral("radioactive"),      QStringLiteral("colorful") },
        { QStringLiteral("royalty"),          QStringLiteral("colorful") },
        { QStringLiteral("desk-oak"),         QStringLiteral("estampados") },
        { QStringLiteral("desk-mahogany"),    QStringLiteral("estampados") },
        { QStringLiteral("desk-bamboo"),      QStringLiteral("estampados") },
        { QStringLiteral("desk-ebony"),       QStringLiteral("estampados") },
        // Estampados novos (helper estampado() acima)
        { QStringLiteral("via-lactea"),       QStringLiteral("estampados") },
        { QStringLiteral("cosmos"),           QStringLiteral("estampados") },
        { QStringLiteral("firmamento"),       QStringLiteral("estampados") },
        { QStringLiteral("quasar"),           QStringLiteral("estampados") },
        { QStringLiteral("abismo"),           QStringLiteral("estampados") },
        { QStringLiteral("metropole"),        QStringLiteral("estampados") },
        { QStringLiteral("vertigem"),         QStringLiteral("estampados") },
        { QStringLiteral("distrito"),         QStringLiteral("estampados") },
        { QStringLiteral("cidade-baixa"),     QStringLiteral("estampados") },
        { QStringLiteral("babilonia"),        QStringLiteral("estampados") },
        { QStringLiteral("concreto"),         QStringLiteral("estampados") },
        { QStringLiteral("horizonte"),        QStringLiteral("estampados") },
        { QStringLiteral("cidade-clara"),     QStringLiteral("estampados") },
        { QStringLiteral("selva"),            QStringLiteral("estampados") },
        { QStringLiteral("clareira"),         QStringLiteral("estampados") },
        { QStringLiteral("alvorada"),         QStringLiteral("estampados") },
        { QStringLiteral("ressaca"),          QStringLiteral("estampados") },
        { QStringLiteral("estratosfera"),     QStringLiteral("estampados") },
        { QStringLiteral("azulejo"),          QStringLiteral("estampados") },
        // Claros
        { QStringLiteral("neve"),                 QStringLiteral("light")     },
        { QStringLiteral("linho"),                QStringLiteral("light")     },
        { QStringLiteral("giz"),                  QStringLiteral("light")     },
        { QStringLiteral("quartzo"),              QStringLiteral("light")     },
        { QStringLiteral("bambu"),                QStringLiteral("light")     },
        // Amarelados
        { QStringLiteral("mel"),                  QStringLiteral("warm")      },
        { QStringLiteral("areia"),                QStringLiteral("warm")      },
        { QStringLiteral("palha"),                QStringLiteral("warm")      },
        { QStringLiteral("acafrao"),              QStringLiteral("warm")      },
        { QStringLiteral("manteiga"),             QStringLiteral("warm")      },
        // Escuros
        { QStringLiteral("carvao"),               QStringLiteral("dark")      },
        { QStringLiteral("obsidiana"),            QStringLiteral("dark")      },
        { QStringLiteral("nimbo"),                QStringLiteral("dark")      },
        { QStringLiteral("profundo"),             QStringLiteral("dark")      },
        // Coloridos
        { QStringLiteral("lavanda"),              QStringLiteral("colorful")  },
        { QStringLiteral("citrino"),              QStringLiteral("colorful")  },
        { QStringLiteral("ferrugem"),             QStringLiteral("colorful")  },
        { QStringLiteral("salvia"),               QStringLiteral("colorful")  },
        { QStringLiteral("eter"),                 QStringLiteral("colorful")  },
        // Gruvbox / Tokyo / Noite
        { QStringLiteral("gruvbox-light"),        QStringLiteral("warm")      },
        { QStringLiteral("gruvbox-hard"),         QStringLiteral("dark")      },
        { QStringLiteral("meia-noite"),           QStringLiteral("dark")      },
        { QStringLiteral("meia-noite-indigo"),    QStringLiteral("dark")      },
        { QStringLiteral("meia-noite-carmesim"),  QStringLiteral("colorful")  },
        { QStringLiteral("madrugada"),            QStringLiteral("dark")      },
        { QStringLiteral("tokyo-storm"),          QStringLiteral("dark")      },
        { QStringLiteral("tokyo-day"),            QStringLiteral("light")     },
        { QStringLiteral("everforest"),           QStringLiteral("dark")      },
        { QStringLiteral("one-dark"),             QStringLiteral("dark")      },
        // Temas criativos
        { QStringLiteral("daguerreotipo"),        QStringLiteral("warm")      },
        { QStringLiteral("crt-verde"),            QStringLiteral("colorful")  },
        { QStringLiteral("bauhaus"),              QStringLiteral("light")     },
        { QStringLiteral("nocturno"),             QStringLiteral("dark")      },
        { QStringLiteral("papyrus"),              QStringLiteral("warm")      },
        { QStringLiteral("vhs"),                  QStringLiteral("colorful")  },
        { QStringLiteral("wabi-sabi"),            QStringLiteral("dark")      },
        { QStringLiteral("manuscrito"),           QStringLiteral("light")     },
        { QStringLiteral("limao"),                QStringLiteral("colorful")  },
        { QStringLiteral("brutalismo"),           QStringLiteral("light")     },
    };
    for (MiraTheme& t : m_themes)
        t.category = kCategory.value(t.id, QStringLiteral("colorful"));
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

namespace {

QJsonObject themeToJson(const MiraTheme& t)
{
    QJsonObject o;
    o["id"] = t.id;
    o["name"] = t.name;
    o["appBackground"] = t.appBackground;
    o["panelBackground"] = t.panelBackground;
    o["panelBorder"] = t.panelBorder;
    o["textPrimary"] = t.textPrimary;
    o["textMuted"] = t.textMuted;
    o["textBright"] = t.textBright;
    o["hoverOverlay"] = t.hoverOverlay;
    o["pressedOverlay"] = t.pressedOverlay;
    o["subtleBorder"] = t.subtleBorder;
    o["accentDefault"] = t.accentDefault;
    o["hoverStrong"] = t.hoverStrong;
    o["borderStrong"] = t.borderStrong;
    o["focusBorder"] = t.focusBorder;
    o["inputBackground"] = t.inputBackground;
    o["disabledText"] = t.disabledText;
    o["selectionRing"] = t.selectionRing;
    o["accentSuccess"] = t.accentSuccess;
    o["accentSuccessSoft"] = t.accentSuccessSoft;
    o["accentSuccessBorderSoft"] = t.accentSuccessBorderSoft;
    o["accentDanger"] = t.accentDanger;
    o["accentDangerSoft"] = t.accentDangerSoft;
    o["accentDangerBorderSoft"] = t.accentDangerBorderSoft;
    o["accentWarning"] = t.accentWarning;
    o["accentInfo"] = t.accentInfo;
    o["accentInfoSoft"] = t.accentInfoSoft;
    o["accentInfoBorderSoft"] = t.accentInfoBorderSoft;
    o["editorBackground"] = t.editorBackground;
    o["editorTextColor"] = t.editorTextColor;
    o["pageShadowEnabled"] = t.pageShadowEnabled;
    o["pageShadowColor"] = t.pageShadowColor;
    o["pageShadowRadius"] = t.pageShadowRadius;
    o["pageShadowOffset"] = t.pageShadowOffset;
    o["backgroundImage"] = t.backgroundImage;
    o["backgroundMode"] = t.backgroundMode;
    o["editorOpacity"] = t.editorOpacity;
    return o;
}

MiraTheme themeFromJson(const QJsonObject& o)
{
    MiraTheme t;
    t.bundled = false;
    t.id = o.value("id").toString();
    t.name = o.value("name").toString();
    t.appBackground = o.value("appBackground").toString();
    t.panelBackground = o.value("panelBackground").toString();
    t.panelBorder = o.value("panelBorder").toString();
    t.textPrimary = o.value("textPrimary").toString();
    t.textMuted = o.value("textMuted").toString();
    t.textBright = o.value("textBright").toString();
    t.hoverOverlay = o.value("hoverOverlay").toString();
    t.pressedOverlay = o.value("pressedOverlay").toString();
    t.subtleBorder = o.value("subtleBorder").toString();
    t.accentDefault = o.value("accentDefault").toString();
    t.hoverStrong = o.value("hoverStrong").toString();
    t.borderStrong = o.value("borderStrong").toString();
    t.focusBorder = o.value("focusBorder").toString();
    t.inputBackground = o.value("inputBackground").toString();
    t.disabledText = o.value("disabledText").toString();
    t.selectionRing = o.value("selectionRing").toString();
    t.accentSuccess = o.value("accentSuccess").toString();
    t.accentSuccessSoft = o.value("accentSuccessSoft").toString();
    t.accentSuccessBorderSoft = o.value("accentSuccessBorderSoft").toString();
    t.accentDanger = o.value("accentDanger").toString();
    t.accentDangerSoft = o.value("accentDangerSoft").toString();
    t.accentDangerBorderSoft = o.value("accentDangerBorderSoft").toString();
    t.accentWarning = o.value("accentWarning").toString();
    t.accentInfo = o.value("accentInfo").toString();
    t.accentInfoSoft = o.value("accentInfoSoft").toString();
    t.accentInfoBorderSoft = o.value("accentInfoBorderSoft").toString();
    t.editorBackground = o.value("editorBackground").toString();
    t.editorTextColor = o.value("editorTextColor").toString();
    t.pageShadowEnabled = o.value("pageShadowEnabled").toBool(false);
    t.pageShadowColor = o.value("pageShadowColor").toString(QStringLiteral("rgba(0,0,0,140)"));
    t.pageShadowRadius = o.value("pageShadowRadius").toInt(24);
    t.pageShadowOffset = o.value("pageShadowOffset").toInt(6);
    t.backgroundImage = o.value("backgroundImage").toString();
    t.backgroundMode = o.value("backgroundMode").toInt(BgZoom);
    t.editorOpacity = o.value("editorOpacity").toInt(100);
    return t;
}

} // namespace

void Manager::loadCustomThemes()
{
    QSettings s;
    const QString raw = s.value(QStringLiteral("theme/customThemes")).toString();
    if (raw.isEmpty()) return;
    const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    if (!doc.isArray()) return;
    for (const QJsonValue& v : doc.array()) {
        if (!v.isObject()) continue;
        MiraTheme t = themeFromJson(v.toObject());
        if (t.id.isEmpty()) continue;
        m_themes.append(t);
    }
}

void Manager::saveCustomThemes() const
{
    QJsonArray arr;
    for (int i = m_bundledCount; i < m_themes.size(); ++i) {
        arr.append(themeToJson(m_themes.at(i)));
    }
    QSettings s;
    s.setValue(QStringLiteral("theme/customThemes"),
               QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

QList<MiraTheme> Manager::bundledThemes() const
{
    QList<MiraTheme> out;
    for (int i = 0; i < m_bundledCount && i < m_themes.size(); ++i) {
        out.append(m_themes.at(i));
    }
    return out;
}

QList<MiraTheme> Manager::customThemes() const
{
    QList<MiraTheme> out;
    for (int i = m_bundledCount; i < m_themes.size(); ++i) {
        out.append(m_themes.at(i));
    }
    return out;
}

bool Manager::isCustom(const QString& id) const
{
    for (int i = m_bundledCount; i < m_themes.size(); ++i) {
        if (m_themes.at(i).id == id) return true;
    }
    return false;
}

QString Manager::uniqueCustomId(const QString& base) const
{
    for (int n = 1; n < 10000; ++n) {
        const QString candidate = QStringLiteral("%1-%2").arg(base).arg(n);
        bool taken = false;
        for (const auto& t : m_themes) {
            if (t.id == candidate) { taken = true; break; }
        }
        if (!taken) return candidate;
    }
    return QStringLiteral("custom-x");
}

QString Manager::upsertCustom(const MiraTheme& theme)
{
    MiraTheme t = theme;
    t.bundled = false;
    if (t.id.isEmpty()) {
        t.id = uniqueCustomId();
    }
    // Não permite sobrescrever bundled — força novo id.
    for (int i = 0; i < m_bundledCount; ++i) {
        if (m_themes.at(i).id == t.id) {
            t.id = uniqueCustomId();
            break;
        }
    }
    bool replaced = false;
    for (int i = m_bundledCount; i < m_themes.size(); ++i) {
        if (m_themes.at(i).id == t.id) {
            m_themes[i] = t;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        m_themes.append(t);
    }
    saveCustomThemes();
    emit customThemesChanged();
    // Se o tema atualizado é o em uso, reaplica.
    if (m_themes.at(m_currentIndex).id == t.id) {
        emit themeChanged();
    }
    return t.id;
}

bool Manager::removeCustom(const QString& id)
{
    for (int i = m_bundledCount; i < m_themes.size(); ++i) {
        if (m_themes.at(i).id == id) {
            const bool wasCurrent = (i == m_currentIndex);
            m_themes.removeAt(i);
            if (wasCurrent) {
                m_currentIndex = 0;
                saveToSettings();
                emit themeChanged();
            } else if (i < m_currentIndex) {
                --m_currentIndex;
            }
            saveCustomThemes();
            emit customThemesChanged();
            return true;
        }
    }
    return false;
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

QString hoverStrong()              { return Manager::instance()->current().hoverStrong; }
QString borderStrong()             { return Manager::instance()->current().borderStrong; }
QString focusBorder()              { return Manager::instance()->current().focusBorder; }
QString inputBackground()          { return Manager::instance()->current().inputBackground; }
QString disabledText()             { return Manager::instance()->current().disabledText; }
QString selectionRing()            { return Manager::instance()->current().selectionRing; }

QString accentSuccess()            { return Manager::instance()->current().accentSuccess; }
QString accentSuccessSoft()        { return Manager::instance()->current().accentSuccessSoft; }
QString accentSuccessBorderSoft()  { return Manager::instance()->current().accentSuccessBorderSoft; }
QString accentDanger()             { return Manager::instance()->current().accentDanger; }
QString accentDangerSoft()         { return Manager::instance()->current().accentDangerSoft; }
QString accentDangerBorderSoft()   { return Manager::instance()->current().accentDangerBorderSoft; }
QString accentWarning()            { return Manager::instance()->current().accentWarning; }
QString accentInfo()               { return Manager::instance()->current().accentInfo; }
QString accentInfoSoft()           { return Manager::instance()->current().accentInfoSoft; }
QString accentInfoBorderSoft()     { return Manager::instance()->current().accentInfoBorderSoft; }

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

QString editorBackground()   { return Manager::instance()->current().editorBackground; }
QString editorTextColor()    { return Manager::instance()->current().editorTextColor; }
bool pageShadowEnabled()     { return Manager::instance()->current().pageShadowEnabled; }
QString pageShadowColor()    { return Manager::instance()->current().pageShadowColor; }
int pageShadowRadius()       { return Manager::instance()->current().pageShadowRadius; }
int pageShadowOffset()       { return Manager::instance()->current().pageShadowOffset; }
QString backgroundImage()    { return Manager::instance()->current().backgroundImage; }
int backgroundMode()         { return Manager::instance()->current().backgroundMode; }
int editorOpacity()          { return Manager::instance()->current().editorOpacity; }

QString globalStyleSheet()
{
    // Vivia em main.cpp como um QString gigante hard-coded em dark. Movido pra
    // cá pra reagir ao tema corrente. Cobre cor de fundo da janela, TopToolbar,
    // menus globais, scrollbar, popups de fonte, dialog/overlay de imagem.
    const QString appBg      = appBackground();
    const QString panelBg    = panelBackground();
    const QString panelBd    = panelBorder();
    const QString txtPrim    = textPrimary();
    const QString txtMuted   = textMuted();
    const QString txtBright  = textBright();
    const QString hover      = hoverOverlay();
    const QString hoverStr   = hoverStrong();
    const QString border     = subtleBorder();
    const QString inputBg    = inputBackground();
    const QString selBg      = pressedOverlay();
    const QString accentBg   = accentDefault();

    return QStringLiteral(R"(
        QMainWindow {
            background-color: %1;
        }
        #editorContainer {
            background-color: %1;
        }
        QTextEdit {
            background-color: %2;
            border: none;
            padding: 80px 100px;
            selection-background-color: %13;
        }
        #topToolbarHolder {
            background: transparent;
        }
        #topToolbar {
            background-color: %2;
            border: 1px solid %3;
            border-radius: 14px;
        }
        #topToolbar QToolButton {
            background: transparent;
            color: %5;
            border: none;
            padding: 4px 6px;
            font-size: 12px;
            border-radius: 6px;
        }
        #topToolbar QToolButton:hover {
            color: %6;
            background-color: %7;
        }
        #topToolbar QToolButton:checked {
            color: %6;
            background-color: %8;
        }
        #topToolbar QToolButton::menu-indicator {
            image: none;
            width: 0;
        }
        #topToolbar QFrame#ttbVSep {
            background-color: %3;
            border: none;
            margin: 0 4px;
        }
        #topToolbar QToolButton#ttbFont {
            min-width: 130px;
            color: %4;
            padding: 4px 8px;
        }
        #topToolbar QToolButton#ttbFont:hover {
            color: %6;
        }
        #topToolbar QToolButton#ttbSize,
        #topToolbar QToolButton#ttbLineHeight {
            min-width: 60px;
            padding: 4px 6px;
        }
        #topToolbar QToolButton#ttbIndent {
            font-size: 14px;
            min-width: 24px;
            padding: 4px 0;
        }
        #ttbSizeStepper QToolButton#ttbSizeStep {
            background: %8;
            color: %4;
            border: none;
            border-radius: 4px;
            font-size: 16px;
            font-weight: bold;
        }
        #ttbSizeStepper QToolButton#ttbSizeStep:hover {
            background: %7;
            color: %6;
        }
        #ttbSizeStepper QLineEdit#ttbSizeValue {
            background: %10;
            color: %6;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 2px 0;
            font-size: 15px;
            font-weight: bold;
            selection-background-color: %12;
        }
        #ttbSizeStepper QLineEdit#ttbSizeValue:focus {
            border: 1px solid %12;
        }
        QMenu {
            background-color: %2;
            color: %4;
            border: 1px solid %3;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px;
        }
        QMenu::item:selected {
            background-color: %8;
            color: %6;
            border-radius: 3px;
        }
        QMenu::separator {
            height: 1px;
            background: %9;
            margin: 4px 8px;
        }
        #fontPickerPopup {
            background-color: %2;
            border: 1px solid %3;
        }
        #fontPickerList {
            background-color: %2;
            color: %4;
            border: none;
            outline: none;
            padding: 4px;
        }
        #fontPickerList::item {
            padding: 8px 12px;
            border-radius: 3px;
        }
        #fontPickerList::item:hover {
            background-color: %7;
            color: %6;
        }
        #fontPickerList::item:selected {
            background-color: %8;
            color: %6;
        }
        #imageInsertDialog {
            background-color: %2;
        }
        #imageInsertDialog QLabel {
            color: %4;
            font-size: 12px;
        }
        #imageInsertDialog #imagePreview {
            background-color: %10;
            border: 1px solid %3;
            border-radius: 4px;
            color: %5;
        }
        #imageInsertDialog QRadioButton {
            color: %4;
            spacing: 6px;
            font-size: 12px;
        }
        #imageInsertDialog QRadioButton::indicator {
            width: 14px;
            height: 14px;
            border-radius: 7px;
            border: 1px solid %3;
            background: %10;
        }
        #imageInsertDialog QRadioButton::indicator:checked {
            background: %4;
            border-color: %4;
        }
        #imageInsertDialog QSlider::groove:horizontal {
            background: %8;
            height: 4px;
            border-radius: 2px;
        }
        #imageInsertDialog QSlider::handle:horizontal {
            background: %4;
            width: 14px;
            height: 14px;
            margin: -6px 0;
            border-radius: 7px;
        }
        #imageInsertDialog QSpinBox {
            background: %10;
            color: %6;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 4px 6px;
        }
        #imageInsertDialog QPushButton {
            background: %8;
            color: %4;
            border: none;
            padding: 8px 18px;
            border-radius: 4px;
            font-size: 12px;
        }
        #imageInsertDialog QPushButton:hover {
            background: %7;
            color: %6;
        }
        #imageInsertDialog QPushButton:default {
            background: %12;
            color: %6;
        }
        #imageInsertDialog QPushButton:default:hover {
            background: %12;
        }
        #imageOverlay {
            background-color: %2;
            border: 1px solid %3;
            border-radius: 6px;
        }
        #imageOverlay QToolButton#imgOverlayBtn {
            background: transparent;
            color: %4;
            border: none;
            padding: 4px 8px;
            border-radius: 3px;
            font-size: 14px;
            min-width: 22px;
        }
        #imageOverlay QToolButton#imgOverlayBtn:hover {
            background-color: %7;
            color: %6;
        }
        #imageOverlay QToolButton#imgOverlayBtn[active="true"] {
            background-color: %12;
            color: %6;
        }
        #imageOverlay QLabel#imgOverlayWidth {
            color: %6;
            font-size: 12px;
            font-weight: bold;
        }
        #imageOverlay QFrame#imgOverlaySep {
            color: %3;
        }
        QScrollBar:vertical {
            background: %1;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %8;
            border-radius: 5px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: %7;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
        }
        QScrollBar:horizontal {
            background: %1;
            height: 10px;
            margin: 0;
        }
        QScrollBar::handle:horizontal {
            background: %8;
            border-radius: 5px;
            min-width: 30px;
        }
        QScrollBar::handle:horizontal:hover {
            background: %7;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
        }
    )")
        .arg(appBg,       // %1
             panelBg,     // %2
             panelBd,     // %3
             txtPrim,     // %4
             txtMuted,    // %5
             txtBright,   // %6
             hover,       // %7
             hoverStr,    // %8
             border,      // %9
             inputBg)     // %10
        .arg(selBg,       // %11 (não usado; reservado)
             accentBg,    // %12
             accentBg);   // %13
}

} // namespace Theme
