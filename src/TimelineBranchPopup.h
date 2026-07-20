#pragma once

#include <QFrame>
#include <QList>
#include <QString>
#include <QStringList>

class QLabel;
class QVBoxLayout;

// Popup flutuante que desambigua, um caso por vez, pra qual ramificação
// (TimelineDef) um evento anômalo deve ir — usado só no resíduo em que o
// detector de ramificações automáticas não consegue decidir sozinho (empate
// de distância cronológica + elenco). Mesmo padrão visual/comportamental do
// PresencePopup: canto inferior direito, não-modal, fila um item por vez.
class TimelineBranchPopup : public QFrame {
    Q_OBJECT
public:
    explicit TimelineBranchPopup(QWidget* parent = nullptr);

    struct Ambiguity {
        QString evId;              // evento ambíguo
        QString title;             // rótulo do evento, pra exibição
        QStringList candidateIds;    // ramificações candidatas (TimelineDef::id)
        QStringList candidateLabels; // rótulo de cada candidata, mesma ordem
    };
    // Enfileira um caso novo e exibe se o popup estiver ocioso. Ignora
    // duplicata (mesmo evId já na fila).
    void enqueue(const Ambiguity& item);

signals:
    // Usuário escolheu `branchId` (um dos candidateIds) pra `evId`.
    void branchChosen(QString evId, QString branchId);

private slots:
    void applyTheme();

private:
    void buildUi();
    void showNext();
    void advance();
    void updateContent();

    QList<Ambiguity> m_queue;
    int m_current = -1;

    QLabel* m_captionLabel = nullptr;
    QLabel* m_titleLabel   = nullptr;
    QVBoxLayout* m_btnCol  = nullptr; // botões de candidato — reconstruídos por item (contagem varia)
};
