#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTime>

class QTimer;

namespace Theme {

// Modos de exibição da imagem de fundo da janela. Espelham FocusWriter mas
// sem "Centered" puro fora do centro — sempre centralizado quando aplica.
enum BackgroundMode {
    BgCenter  = 0, // desenha no centro sem redimensionar (pode sobrar/cortar)
    BgTile    = 1, // repete cobrindo toda a janela
    BgStretch = 2, // estica ignorando proporção
    BgFit     = 3, // ajusta mantendo proporção (KeepAspectRatio)
    BgZoom    = 4, // amplia mantendo proporção (KeepAspectRatioByExpanding)
};

// Bundle de propriedades visuais de um tema. Cores em #rrggbb ou rgba(...).
// Layout da página (largura/margens) NÃO é responsabilidade do tema — fica
// em EditorLayout.
struct MiraTheme {
    QString id;
    QString name;
    bool bundled = true;
    // Categoria pro filtro do painel de Temas: "light" | "warm" | "dark" |
    // "colorful". Vazio = sem filtro (temas personalizados).
    QString category;

    // Cores principais (painéis, app)
    QString appBackground;
    QString panelBackground;
    // Opacidade das barras e painéis fixos da janela (0-100). Abaixo de 100 a
    // TopToolbar, a LeftBar, o painel de capítulos e as gavetas deixam ver o
    // fundo do app — cor sólida na maioria dos temas, foto nos estampados, e o
    // halo da página nos que têm pageGlowEnabled. NÃO vale para diálogos,
    // popups e toasts: esses continuam opacos, senão viram sopa por cima do
    // texto. Ver Theme::panelBackgroundCss().
    int panelOpacity = 100;
    QString panelBorder;
    // Arredondamento dos cantos de painéis/barras (px) — existia no Mira 1
    // como um slider global ("Arredondamento", 0-24px, default 14); aqui é
    // por-tema, editável no Editor de Temas. Default 10 preserva o valor que
    // já era hardcoded antes disso existir como configuração.
    int panelRadius = 10;
    QString textPrimary;
    QString textMuted;
    QString textBright;
    QString hoverOverlay;
    QString pressedOverlay;
    QString subtleBorder;
    QString accentDefault;

    // Hover/focus mais fortes — usados em botões e inputs.
    // Em tema escuro, `rgba(255,255,255,0.12+)`; em tema claro, `rgba(0,0,0,0.08+)`.
    QString hoverStrong;
    QString borderStrong;
    QString focusBorder;

    // Background de campos "inset" (textareas, combos): em tema escuro afunda
    // pra `rgba(0,0,0,0.30)`; em tema claro sobe pra um cinza neutro.
    QString inputBackground;

    // Texto e borda de elementos desabilitados.
    QString disabledText;

    // Cor de "ring" pra selecionar itens em grids/swatches — em tema escuro é
    // branco, em tema claro é preto suave.
    QString selectionRing;

    // Cores semânticas: confirm/success, danger/delete, warning, info.
    // Cada uma vem com a cor cheia + um background hover suave.
    QString accentSuccess;
    QString accentSuccessSoft;
    QString accentSuccessBorderSoft;
    QString accentDanger;
    QString accentDangerSoft;
    QString accentDangerBorderSoft;
    QString accentWarning;
    QString accentInfo;
    QString accentInfoSoft;
    QString accentInfoBorderSoft;

    // Editor — "página" de escrita (apenas cor; largura/margens vivem em EditorLayout)
    QString editorBackground;
    QString editorTextColor;

    // Sombra projetada da página (estilo FocusWriter). Quando enabled, o
    // editorColumn ganha QGraphicsDropShadowEffect.
    bool pageShadowEnabled = false;
    // Halo da página: em vez de projetar sombra, a folha ESPALHA luz por cima
    // dos painéis (MainWindow::positionPageGlow). Só faz sentido com uma
    // pageShadowColor clara, mas é uma escolha do tema, não uma dedução da cor:
    // vários temas antigos usam sombra clara sem querer brilho nenhum.
    bool pageGlowEnabled = false;
    QString pageShadowColor = QStringLiteral("rgba(0,0,0,140)");
    int pageShadowRadius = 24;
    int pageShadowOffset = 6;

    // Imagem de fundo da janela. Quando backgroundImage está vazio, o app
    // pinta normalmente; caso contrário, MainWindow desenha a imagem antes
    // dos painéis. Path absoluto, modo segue BackgroundMode.
    QString backgroundImage;
    // Default = Preencher: imagens devem sempre cobrir a tela toda. Outros modos
    // deixam áreas sem imagem ou cortam mal em janelas grandes.
    int backgroundMode = BgZoom;

    // Opacidade da página do editor (0–100). 100 = totalmente opaca (default).
    // Valores menores deixam a imagem de fundo aparecer atrás da página.
    int editorOpacity = 100;

    // --- Metadados de compartilhamento (arquivos .qtheme) ---
    // Nada disso muda a aparência do tema — é a identidade dele enquanto
    // arquivo que circula entre pessoas. Um tema criado antes desses campos
    // existirem tem tudo vazio, e isso é válido: só ganha uuid/autor quando
    // for exportado pela primeira vez. Ver ThemePackage.h.

    // Identificador global e estável, gerado uma vez (QUuid) e carregado pelo
    // tema pra sempre. Diferente de `id`, que é local ("custom-3") e colide
    // entre máquinas — dois temas distintos podem ser "custom-3" em PCs
    // diferentes, então `id` não serve pra identificar um tema no mundo.
    QString uuid;
    // Quem fez. `authorContact` é livre: site, e-mail, @ de rede social.
    QString author;
    QString authorContact;
    // O que o autor permite que façam com o tema. Texto livre, sem validação
    // — o app não é cartório, só carrega a declaração junto do arquivo.
    QString license;
    // Descrição curta opcional, pra quando o nome não basta.
    QString description;
    // Versão *do tema*, não do app: o autor corrige uma cor e republica.
    // Quem recebe consegue saber qual é a mais nova.
    int themeVersion = 1;
    // Menor versão do Qenna que entende esse tema. Vazio = qualquer uma.
    QString minAppVersion;

    // Campos do JSON que ESTA versão do app não conhece — tipicamente um tema
    // salvo por uma versão futura do Qenna, com propriedades que ainda não
    // existiam aqui. Guardamos crus e reemitimos no export, então importar num
    // Qenna antigo e reexportar não apaga o que ele não sabia ler.
    // Não são aplicados visualmente (o app não saberia o que fazer com eles),
    // apenas preservados.
    QJsonObject extras;
};

// Serialização de um tema. Usadas tanto pra persistir os customs em QSettings
// quanto pelo ThemePackage (arquivos .qtheme). O round-trip é fiel: campos que
// esta versão não conhece entram em MiraTheme::extras e voltam intactos.
QJsonObject themeToJson(const MiraTheme& t);
MiraTheme themeFromJson(const QJsonObject& o);

// Troca automática de tema por horário (dia/noite). "day"/"night" aqui
// referem-se aos dois papéis configuráveis, não a categorias de tema —
// qualquer tema (bundled ou custom) pode ser atribuído a qualquer papel.
struct AutoSwitchConfig {
    bool enabled = false;
    QString dayThemeId;
    QString nightThemeId;
    QTime dayStart = QTime(7, 0);
    QTime nightStart = QTime(19, 0);
};

// Singleton observável. Quem precisa reagir a troca de tema deve conectar
// ao sinal themeChanged() e reaplicar suas stylesheets.
class Manager : public QObject {
    Q_OBJECT
public:
    static Manager* instance();

    const MiraTheme& current() const;
    const QList<MiraTheme>& available() const;
    void setCurrent(const QString& id);

    // Custom themes — bibliotecas separadas, salvas em QSettings.
    // Visíveis em uma coluna própria no ThemesPanel.
    QList<MiraTheme> bundledThemes() const;
    QList<MiraTheme> customThemes() const;
    bool isCustom(const QString& id) const;

    // Adiciona ou substitui um custom (match por id). Retorna o id final.
    // Se for novo, gera id "custom-<n>" único.
    QString upsertCustom(const MiraTheme& theme);
    bool removeCustom(const QString& id);
    QString uniqueCustomId(const QString& base = QStringLiteral("custom")) const;

    // Troca automática por horário (dia/noite). setAutoSwitchConfig() persiste,
    // (re)inicia o timer interno e reaplica o tema esperado imediatamente se
    // enabled. Aplicar um tema manualmente (setCurrent) enquanto enabled=true
    // desliga a troca automática — evita o timer "brigar" com uma escolha manual.
    const AutoSwitchConfig& autoSwitchConfig() const { return m_autoSwitch; }
    void setAutoSwitchConfig(const AutoSwitchConfig& cfg);

    // Favoritos — marcação livre, independe de categoria/aba (bundled ou
    // custom). Persistido em QSettings, sobrevive à exclusão de um custom
    // (o id só fica "orfão" sem nenhum efeito, não precisa de limpeza).
    bool isFavorite(const QString& id) const;
    void setFavorite(const QString& id, bool favorite);

signals:
    void themeChanged();
    void customThemesChanged();
    void autoSwitchConfigChanged();
    void favoritesChanged();

private:
    Manager();
    void loadBundled();
    void loadFromSettings();
    void saveToSettings() const;
    void loadCustomThemes();
    void saveCustomThemes() const;
    void loadAutoSwitchSettings();
    void saveAutoSwitchSettings() const;
    void tickAutoSwitch();
    QString expectedAutoThemeId() const;
    void loadFavorites();
    void saveFavorites() const;

    QList<MiraTheme> m_themes;        // bundled + custom (custom no fim)
    int m_currentIndex = 0;
    int m_bundledCount = 0;           // bundled ocupam [0, m_bundledCount)

    AutoSwitchConfig m_autoSwitch;
    QTimer* m_autoSwitchTimer = nullptr;
    bool m_applyingAutoSwitch = false; // true durante tickAutoSwitch(), evita autodesligar

    QSet<QString> m_favorites;
};

// API legada — chamadas existentes (`Theme::appBackground()` etc.) seguem
// funcionando e passam a refletir o tema atual.
QString appBackground();
QString panelBackground();
QString panelBorder();
// Valor cru (px) do tema atual — usado pelo Editor de Temas (spinbox/slider).
int panelRadius();
// Mesmo valor formatado "Npx", pronto pra interpolar em QSS.
QString panelBorderRadius();

// Escala de arredondamento derivada do slider do tema.
//
// Aplicar o valor de painel em TUDO deforma a interface nos extremos: um
// input de 24px de altura com raio 24 vira uma pílula, um checkbox vira
// círculo. Então o slider governa três níveis proporcionais, e cada widget
// usa o do seu tamanho:
//
//   panelRadius()   superfícies — painéis, diálogos, popups, menus, cards
//   controlRadius() controles   — botões, inputs, combos, abas
//   itemRadius()    itens miúdos — tags, chips, swatches, linhas de lista
//
// Os dois derivados têm teto próprio: passar de certo ponto não melhora nada
// e só engorda o controle. Em raio 0 todos vão a 0 (cantos retos de verdade).
int controlRadius();
int itemRadius();
QString controlBorderRadius();
QString itemBorderRadius();

// Troca os tokens de raio de uma folha QSS pelos valores do tema atual:
//
//     @radius-panel    @radius-control    @radius-item
//
// Existe porque as folhas do app são QStringLiteral com .arg() posicional —
// parametrizar o raio como mais um "%N" obrigaria a mexer no .arg() de
// centenas de strings, com alto risco de trocar a ordem de algum argumento.
// Os tokens são substituídos ANTES do .arg(), então o uso é:
//
//     setStyleSheet(Theme::qss(QStringLiteral(R"( ... )")).arg(cor1, cor2));
//
// Escrever um token que não existe não quebra nada: sobra no texto e o Qt
// ignora a regra inválida.
QString qss(const QString& sheet);
QString textPrimary();
QString textMuted();
QString textBright();
QString hoverOverlay();
QString pressedOverlay();
QString subtleBorder();
QString accentDefault();
QString panelQss(const QString& objectName);
// Fundo de painel já com a opacidade do tema aplicada, pronto pra QSS. Devolve
// o hex cru quando panelOpacity == 100, senão um rgba(). Use nas barras fixas;
// para diálogos e popups continue usando panelBackground().
QString panelBackgroundCss();
int panelOpacity();

// Hover/focus fortes e inputs.
QString hoverStrong();
QString borderStrong();
QString focusBorder();
QString inputBackground();
QString disabledText();
QString selectionRing();

// Cores semânticas (acentos).
QString accentSuccess();
QString accentSuccessSoft();
QString accentSuccessBorderSoft();
QString accentDanger();
QString accentDangerSoft();
QString accentDangerBorderSoft();
QString accentWarning();
QString accentInfo();
QString accentInfoSoft();
QString accentInfoBorderSoft();

// Novos acessores específicos do editor.
QString editorBackground();
QString editorTextColor();
bool pageShadowEnabled();
bool pageGlowEnabled();
QString pageShadowColor();
int pageShadowRadius();
int pageShadowOffset();

// Imagem de fundo + opacidade da página.
QString backgroundImage();
int backgroundMode();
int editorOpacity();

// Stylesheet global do app — substitui o bloco hard-coded que vivia no main.cpp.
// Tem QMenu, QScrollBar, TopToolbar, FontPickerPopup, ImageInsertDialog,
// ImageOverlay etc. Reaplicar em themeChanged().
QString globalStyleSheet();

// QToolTip ignora `background-color` do QSS em vários estilos nativos
// (Windows Vista style, por exemplo) — só a borda/padding do QSS pega,
// o fundo continua preto do SO. QPalette::ToolTipBase/ToolTipText é o jeito
// confiável de recolorir tooltip nativo. Chamar junto com
// setStyleSheet(globalStyleSheet()) — no boot e em themeChanged().
void applyToolTipPalette();

} // namespace Theme
