#pragma once

#include <QFrame>
#include <QString>

class GlossaryStore;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTextEdit;
class QToolButton;
class QLabel;

// Painel flutuante do Glossário. Lista de termos (alfabética), busca aparece
// com 3+ termos, edição inline e botão de remover. Auto-fecha ao clicar fora.
class GlossaryPanel : public QFrame
{
    Q_OBJECT
public:
    explicit GlossaryPanel(GlossaryStore* store, QWidget* parent = nullptr);

    void showNear(const QRect& anchorGlobal);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onStoreChanged();
    void onSearchTextChanged(const QString& text);
    void onItemSelected();
    void onTermEdited();
    void onDefinitionEdited();
    void onRemoveClicked();
    void onAddClicked();
    void applyTheme();

private:
    void buildUi();
    void rebuildList();
    void selectId(const QString& id);
    void updateRightPane();

    GlossaryStore* m_store;
    QLabel* m_header = nullptr;
    QLineEdit* m_search = nullptr;
    QListWidget* m_list = nullptr;
    QLineEdit* m_termEdit = nullptr;
    QTextEdit* m_defEdit = nullptr;
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_addBtn = nullptr;

    QString m_selectedId;
    bool m_syncing = false;
};
