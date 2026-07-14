#pragma once

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
    QString panelBorder;
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
};

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
QString panelBorderRadius();
QString textPrimary();
QString textMuted();
QString textBright();
QString hoverOverlay();
QString pressedOverlay();
QString subtleBorder();
QString accentDefault();
QString panelQss(const QString& objectName);

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

} // namespace Theme
