#pragma once

#include <QHash>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <functional>

class QToolButton;
class QVBoxLayout;
class QLabel;
class QTimer;
class LeftBarLabels;

// RightBar (13/11/2026, ideia do Pe): a irmã da LeftBar, do outro lado da folha.
// A esquerda guarda o que você escreve (gavetas, mapa, linha do tempo); a direita
// guarda as ferramentas do projeto: Criador de Mundos, Referência, Pensário,
// Lembretes, Estatísticas e a assistente. Ligada, esses seis botões saem da
// barra de cima (ver TopToolbar::setProjectButtonsInRightBar).
//
// Mesmo desenho da LeftBar: mesma largura, mesma moldura, mesmos botões. Fica
// SEMPRE no lado oposto ao da LeftBar.
class RightBar : public QWidget {
    Q_OBJECT
public:
    enum Tool { Construtor, Reference, Pensario, Reminders, Statistics, Assistant };
    Q_ENUM(Tool)

    explicit RightBar(QWidget* parent = nullptr);

    static bool enabledInSettings();
    static void setEnabledInSettings(bool on);

    Qt::Edge barSide() const { return m_side; }
    void setBarSide(Qt::Edge side);

    QToolButton* toolButton(Tool t) const { return m_buttons.value(int(t)); }
    QRect toolGlobalRect(Tool t) const;

    // Quem diz se o painel de cada botão está aberto é a MainWindow; a barra
    // pergunta quando precisa acender ou apagar um botão.
    std::function<bool(Tool)> isToolOpen;
    void refreshActiveStates();
    // Acompanha um painel: abrir e fechar acendem e apagam o botão.
    void watchPanel(Tool t, QWidget* panel);

    void setReminderBadge(bool active);
    void pulsePensarioBadge();

    // Modo focado: igual à LeftBar, some sem sair do layout (a folha não pula)
    // e volta com o mouse em cima.
    void setChromeHidden(bool hidden);
    bool chromeHidden() const { return m_chromeHidden; }
    void setAutoHide(bool on);

    // Emendada embaixo da faixa de ponta a ponta, como a LeftBar.
    void setJoinedTop(bool on);

    // Etiquetas: a mesma faixa de nomes da LeftBar (e a mesma chave nas
    // Configurações, "Nomes na barra lateral"), abrindo pra dentro da tela.
    bool labelsUnderMouse() const;

signals:
    void toolTriggered(RightBar::Tool tool);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QToolButton* makeButton(Tool t, const QString& icon, const QString& tooltip);
    QString panelStyle() const;
    void applyTheme();
    void applyUiScale();
    void applyContentVisibility();
    void positionBadges();
    void showLabels();
    void refreshLabels();
    void hideLabels(bool animated);
    void maybeHideLabels();
    void updateLabelsGeometry();
    LeftBarLabels* m_labels = nullptr;
    QTimer* m_labelsTimer = nullptr;
    int m_labelsWidth = 140;
    int m_labelsHeight = 0;

    Qt::Edge m_side = Qt::RightEdge;
    bool m_chromeHidden = false;
    bool m_autoHide = false;
    bool m_joinedTop = false;
    QVBoxLayout* m_layout = nullptr;
    QHash<int, QToolButton*> m_buttons;
    QHash<QObject*, int> m_watched;
    QList<QWidget*> m_separators;
    QLabel* m_reminderBadge = nullptr;
    QLabel* m_pensarioBadge = nullptr;
    QTimer* m_hideTimer = nullptr;
};
