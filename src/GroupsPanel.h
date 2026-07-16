#pragma once

#include <QFrame>
#include <QString>

class ProjectModel;
class QLabel;
class QPushButton;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

// Painel flutuante de Grupos. Mostra o picker de grupos no topo e lista
// todos os documentos marcados com o grupo selecionado, de qualquer gaveta.
// Aberto pelo botão Groups da LeftBar.
class GroupsPanel : public QFrame {
    Q_OBJECT
public:
    explicit GroupsPanel(ProjectModel* model, QWidget* parent = nullptr);

    void showNear(const QRect& anchorGlobal);

signals:
    void itemActivated(QString drawerKey, QString itemId);
    void closeRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private slots:
    void onGroupsChanged();
    void applyTheme();

private:
    void buildUi();
    void rebuildGroupChips();
    void rebuildItemList();
    void showGroupContextMenu(const QString& groupId, const QPoint& globalPos);
    void showEditGroupDialog(const QString& groupId);

    ProjectModel* m_model;
    QString m_selectedGroupId;

    QWidget* m_header     = nullptr;
    QLabel*  m_title      = nullptr;
    QToolButton* m_newBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;

    QWidget*     m_chipsWrap  = nullptr;
    QScrollArea* m_chipsScroll = nullptr;

    QScrollArea* m_listScroll = nullptr;
    QWidget*     m_listHost   = nullptr;
    QVBoxLayout* m_listLay    = nullptr;

    // Drag-para-mover o painel pelo header
    bool  m_dragging   = false;
    QPoint m_dragOffset;
};
