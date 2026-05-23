#pragma once

#include <QList>
#include <QObject>
#include <QString>

namespace Theme {

// Bundle de propriedades visuais de um tema. Cores em #rrggbb ou rgba(...).
// Layout da página (largura/margens) NÃO é responsabilidade do tema — fica
// em EditorLayout.
struct MiraTheme {
    QString id;
    QString name;
    bool bundled = true;

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

    // Editor — "página" de escrita (apenas cor; largura/margens vivem em EditorLayout)
    QString editorBackground;
    QString editorTextColor;

    // Sombra projetada da página (estilo FocusWriter). Quando enabled, o
    // editorColumn ganha QGraphicsDropShadowEffect.
    bool pageShadowEnabled = false;
    QString pageShadowColor = QStringLiteral("rgba(0,0,0,140)");
    int pageShadowRadius = 24;
    int pageShadowOffset = 6;
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

signals:
    void themeChanged();

private:
    Manager();
    void loadBundled();
    void loadFromSettings();
    void saveToSettings() const;

    QList<MiraTheme> m_themes;
    int m_currentIndex = 0;
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

// Novos acessores específicos do editor.
QString editorBackground();
QString editorTextColor();
bool pageShadowEnabled();
QString pageShadowColor();
int pageShadowRadius();
int pageShadowOffset();

} // namespace Theme
