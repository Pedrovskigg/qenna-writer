#pragma once

#include "CharacterDetector.h"
#include "ElementsStore.h"

#include <QFrame>
#include <QList>

class QLabel;
class QPushButton;

// Popup flutuante que confirma detecções de presença de personagem.
// Aparece no canto inferior direito do editor, processa uma detecção por vez.
class PresencePopup : public QFrame {
    Q_OBJECT
public:
    explicit PresencePopup(ElementsStore* store, QWidget* parent = nullptr);

    // Enfileira novas detecções e exibe a próxima se o popup estiver ocioso.
    void enqueue(const QList<CharacterDetection>& detections);

signals:
    void markRequested(QString elementId, QString docKey);
    void markAllActivated();          // "Sempre" — ativa markAll global + marca atual
    void ignoredNow(QString elementId, QString docKey);
    void neverRequested(QString elementId, QString docKey);

private slots:
    void applyTheme();

private:
    void buildUi();
    void showNext();
    void advance();
    void updateContent();

    ElementsStore* m_store;
    QList<CharacterDetection> m_queue;
    int m_current = -1;

    QLabel* m_photoLabel   = nullptr;
    QLabel* m_nameLabel    = nullptr;
    QLabel* m_captionLabel = nullptr;
    QPushButton* m_markBtn   = nullptr;
    QPushButton* m_alwaysBtn = nullptr;
    QPushButton* m_ignoreBtn = nullptr;
    QPushButton* m_neverBtn  = nullptr;
};
