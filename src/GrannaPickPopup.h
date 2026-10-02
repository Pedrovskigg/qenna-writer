#pragma once

#include <QColor>
#include <QFrame>
#include <QPixmap>
#include <QString>
#include <QVector>

// "Quem disse?" — abre ao clicar no aviso do Granna no DocHeader. Mostra a
// fala, o palpite do Granna (se houver) e os rostos: quem está na cena
// primeiro, o resto do elenco embaixo, mais apagado. Embaixo, Figurante e
// Não é fala. Não grava nada: só avisa a escolha (MainWindow aplica no
// DialogueStore).
class GrannaPickPopup : public QFrame {
    Q_OBJECT
public:
    struct Person {
        QString id;
        QString name;
        QPixmap face;  // circular, já no tamanho do rosto (kFace)
        QColor voice;
    };

    GrannaPickPopup(const QString& quote, const QString& hint,
                    const QVector<Person>& scene, const QVector<Person>& others,
                    const QString& suggestedId, QWidget* parent = nullptr);

    // Tamanho do rosto em pixels lógicos (pra quem monta as Person).
    static int faceSize();

    // Abre com o topo-esquerdo perto de `anchorGlobal` (embaixo do aviso),
    // sem sair da tela.
    void popupBelow(const QRect& anchorGlobal);

signals:
    void characterChosen(const QString& characterId);
    void extraChosen();
    void notSpeechChosen();
    void closed();

protected:
    void hideEvent(QHideEvent* event) override;

private:
    QWidget* m_firstFace = nullptr;
};
