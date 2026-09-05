#pragma once

#include <QDialog>

class QVBoxLayout;
class QWidget;
class QLabel;

// Lixeira de projetos — lista o que foi movido pra lá (ver TrashService),
// com Restaurar e Apagar definitivamente por item. É o único lugar do app
// onde um projeto pode ser apagado de verdade, sem volta.
class TrashDialog : public QDialog {
    Q_OBJECT
public:
    explicit TrashDialog(QWidget* parent = nullptr);

private:
    void rebuildList();
    void applyTheme();

    QVBoxLayout* m_listLayout = nullptr;
    QWidget*     m_listHolder = nullptr;
    QLabel*      m_emptyLabel = nullptr;
};
