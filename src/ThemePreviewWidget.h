#pragma once

#include "Theme.h"

#include <QPixmap>
#include <QWidget>

// Widget que renderiza um "mini-app" usando as cores e a imagem de fundo do
// tema recebido — usado tanto nos cards do ThemesPanel quanto no preview
// ao vivo do ThemeEditorDialog. Não interage com o estado real do app.
// O texto Lorem Ipsum é desenhado dentro de uma "página" centralizada,
// com a sombra/opacidade do tema, reproduzindo a sensação do editor.
class ThemePreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit ThemePreviewWidget(QWidget* parent = nullptr);

    void setTheme(const Theme::MiraTheme& theme);
    const Theme::MiraTheme& theme() const { return m_theme; }

    void setShowChrome(bool show); // se true, desenha barrinhas simulando toolbar/leftbar
    void setLoremText(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildBackgroundPixmap();

    Theme::MiraTheme m_theme;
    QString m_lorem;
    bool m_showChrome = true;

    // Cache do background renderizado pro tamanho atual.
    QPixmap m_bgSource;
    QPixmap m_bgScaled;
    QSize   m_bgScaledFor;
    QString m_bgPath;
    int     m_bgMode = -1;
};
