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

private:
    void buildUi();
    void accept() override;

    ElementsStore* m_store;
    QString m_docKey;
    QList<QPair<QString, QCheckBox*>> m_checks; // elementId -> checkbox
};
