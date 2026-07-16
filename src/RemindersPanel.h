#pragma once

#include <QFrame>
#include <QString>

class RemindersStore;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTimeEdit;
class QToolButton;

class RemindersPanel : public QFrame
{
    Q_OBJECT
public:
    explicit RemindersPanel(RemindersStore* store, QWidget* parent = nullptr);

    void showNear(const QRect& anchorGlobal);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onStoreChanged();
    void onAddClicked();
    void onNotifyCheckToggled(bool checked);
    void onHistoryToggleClicked();
    void applyTheme();

private:
    void buildUi();
    void rebuildActiveList();
    void rebuildHistoryList();
    void updateHistoryHeader();
    void completeItem(const QString& id);
    void removeItem(const QString& id);

    RemindersStore* m_store;

    QLineEdit*   m_input         = nullptr;
    QPushButton* m_addBtn        = nullptr;
    QCheckBox*   m_notifyCheck   = nullptr;
    QTimeEdit*   m_timeEdit      = nullptr;
    QLabel*      m_emptyLabel    = nullptr;
    QListWidget* m_activeList    = nullptr;
    QToolButton* m_historyToggle = nullptr;
    QPushButton* m_clearBtn      = nullptr;
    QListWidget* m_historyList   = nullptr;

    bool m_historyExpanded = false;
    bool m_syncing         = false;
};
