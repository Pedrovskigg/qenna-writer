#pragma once

#include "Theme.h"

#include <QDialog>
#include <QList>
#include <QString>
#include <functional>

class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

namespace CreatorDetail {
class Canvas;
class Toggle;
}

// Criador de Temas ("Faixa que muda", aprovado em 2026-09-27). A janela do
// Qenna ocupa o criador quase inteiro (a mesma prévia pintada do painel de
// Temas, com Escrita, Gavetas e Editor) e uma faixa embaixo guarda os
// controles. Clicar no app escolhe o que editar — fundo, painéis e barras,
// ícones, folha, título do capítulo, texto, destaque — e a faixa vira os
// controles daquilo; as pílulas fazem o mesmo sem clicar. Painéis podem ter
// cor própria ("só este"). Antes e depois compara com o tema de partida.
class ThemeEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit ThemeEditorDialog(const Theme::MiraTheme& base, QWidget* parent = nullptr);

    const Theme::MiraTheme& theme() const { return m_theme; }

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum Group { Background, Panels, Icons, Sheet, Title, Text, Accent };

    void buildUi();
    void applyStyle();
    void setGroup(Group g, const QString& panel = QString());
    void rebuildControls();
    void refreshPreview();
    void refreshHeader();
    void pushUndo();
    void undo();
    void setScene(int scene);

    // Campos de cor por chave ("appBackground", "iconColor", "panel:<key>:bg"...).
    QString colorOf(const QString& key) const;
    void setColor(const QString& key, const QString& value);
    QString colorLabel(const QString& key) const;
    bool colorHasAlpha(const QString& key) const;
    void pickColor(const QString& key, QWidget* anchor);

    // Blocos da faixa
    QWidget* swatch(const QString& key, const QString& label = QString());
    QWidget* slider(const QString& label, int min, int max, int value, const QString& unit,
                    const std::function<void(int)>& apply);
    void addOverlayControls();
    QWidget* link(const QString& text, const std::function<void()>& onClick);
    QWidget* hint(const QString& text, int maxWidth = 380);
    QWidget* contrastBox();
    QWidget* photoPalette();
    QWidget* imageButton();

    static QString groupName(Group g);
    static QString panelName(const QString& key);
    static QString onlyPanelText(const QString& key);

    Theme::MiraTheme m_theme;
    Theme::MiraTheme m_original;
    QList<Theme::MiraTheme> m_history;

    Group m_group = Sheet;
    QString m_panel;                  // painel clicado (PanelKey) no grupo Painéis
    QString m_activeKey = QStringLiteral("accentDefault");  // cor que recebe as cores da foto
    int m_scene = 0;
    QList<QColor> m_photoColors;
    QString m_photoFor;

    CreatorDetail::Canvas* m_canvas = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QList<QPushButton*> m_sceneTabs;
    CreatorDetail::Toggle* m_compare = nullptr;
    QPushButton* m_undoBtn = nullptr;
    QLabel* m_groupTitle = nullptr;
    QList<QPushButton*> m_pills;
    QWidget* m_controls = nullptr;
    QHBoxLayout* m_controlsLay = nullptr;
};
