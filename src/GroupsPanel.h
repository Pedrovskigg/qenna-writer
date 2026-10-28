#pragma once

#include <QFrame>
#include <QString>

class ProjectModel;
class QLabel;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

namespace GroupsUi { class Tree; }

// Painel flutuante de Grupos (LeftBar → Grupos). Desde 27/10 é uma árvore: à
// esquerda os grupos (um dentro do outro, Lugar ou Social; arrastar muda o pai),
// à direita o escolhido, com os documentos dele, os que vêm de cima (do pai, do
// avô...) e os grupos que moram dentro dele. Vale pra qualquer projeto, com ou
// sem mapa: o mapa só ganha um grupo com o nome dele, e os territórios nascem dentro.
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
    void select(const QString& groupId);
    void rebuildDetail();
    void showNewMenu(const QString& parentId, const QPoint& globalPos);   // Lugar ou Social (vazio = solto)
    void showGroupContextMenu(const QString& groupId, const QPoint& globalPos);
    void showActsInMenu(const QString& groupId, const QPoint& globalPos);
    void showEditGroupDialog(const QString& groupId);
    QString nextColor() const;

    ProjectModel* m_model;
    QString m_selectedGroupId;

    QWidget* m_header     = nullptr;
    QLabel*  m_title      = nullptr;
    QToolButton* m_newBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;

    QScrollArea* m_treeScroll = nullptr;
    GroupsUi::Tree* m_tree    = nullptr;
    QLabel*      m_treeFoot   = nullptr;   // dica + onde soltar pra ficar sem pai

    QScrollArea* m_detailScroll = nullptr;
    QWidget*     m_detailHost   = nullptr;
    QVBoxLayout* m_detailLay    = nullptr;

    // Drag-para-mover o painel pelo header
    bool  m_dragging   = false;
    QPoint m_dragOffset;
};
