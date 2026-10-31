#pragma once

#include <QFrame>
#include <QPixmap>
#include <QString>

// Abre ao clicar na foto do aviso de presença no DocHeader (o popup de
// confirmação do canto saiu em 31/10: a presença é marcada sozinha e o aviso
// só conta). Desfaz: "Não está aqui" tira deste capítulo; "Nunca detectar"
// desliga o detector pra esse personagem. Não grava nada: só avisa a escolha.
class PresenceUndoPopup : public QFrame {
    Q_OBJECT
public:
    PresenceUndoPopup(const QPixmap& face, const QString& name, QWidget* parent = nullptr);

    // Abre embaixo de `anchorGlobal` (a foto clicada), sem sair da tela.
    void popupBelow(const QRect& anchorGlobal);

signals:
    void notHereChosen();
    void neverChosen();
    void closed();

protected:
    void hideEvent(QHideEvent* event) override;

private:
    QWidget* m_first = nullptr;
};
