#pragma once

#include <QDialog>
#include <QPair>
#include <QString>
#include <QList>

class ElementsStore;
class QCheckBox;

// Diálogo de marcação manual de "elementos presentes" num capítulo ou cena.
// Lista todos os elementos do projeto (personagem/cenário/objeto), agrupados
// por tipo, com checkbox pré-marcado conforme ElementsStore::docElements.
class ElementsPresentDialog : public QDialog {
    Q_OBJECT
public:
    ElementsPresentDialog(ElementsStore* store, const QString& docKey, QWidget* parent = nullptr);

signals:
    // Emitido quando o usuário desmarca um elemento que estava presente —
    // deixa o chamador registrar a recusa junto da detecção automática
    // (mesmo mecanismo do "ignorar" do PresencePopup), pra remoção manual
    // não ser desfeita pelo próximo scan que encontrar o nome no texto.
    void elementRemoved(QString elementId, QString docKey);

private:
    void buildUi();
    void accept() override;

    ElementsStore* m_store;
    QString m_docKey;
    QList<QPair<QString, QCheckBox*>> m_checks; // elementId -> checkbox
};
