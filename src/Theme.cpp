#include "Theme.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
        t.editorOpacity    = 86;
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
        t.editorOpacity    = 82;
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
        t.editorOpacity    = 78;
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
        t.editorOpacity    = 88;
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
        t.editorOpacity    = 76;
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
        t.editorOpacity    = 80;
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
        t.editorOpacity    = 80;
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
        t.editorOpacity    = 84;
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
        t.editorOpacity    = 78;
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
        #ttbSizeStepper QLabel#ttbSizeValue {
            color: %6;
            font-size: 15px;
            font-weight: bold;
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
