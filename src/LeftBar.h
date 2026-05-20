#pragma once

#include <QHash>
#include <QString>
#include <QWidget>

class QVBoxLayout;
class QToolButton;
class ProjectModel;

class LeftBar : public QWidget {
    Q_OBJECT
public:
    enum FixedAction {
        Info,
        Whiteboard,
        Manuscripts,
        Consistency,
    };
    Q_ENUM(FixedAction)

    explicit LeftBar(ProjectModel* model, QWidget* parent = nullptr);

    static int barWidth();

    bool isMirrored() const { return m_mirrored; }
    void setMirrored(bool mirrored);

    // Indica visualmente qual painel está aberto. Limpar (None / vazio) deixa
    // todos os botões desligados.
    void setActiveFixedAction(FixedAction action);
    void setActiveDrawer(const QString& drawerKey);
    void clearSelection();

signals:
    void fixedActionTriggered(LeftBar::FixedAction action);
    void drawerSelected(QString drawerKey);
    void newDrawerRequested();

private slots:
    void rebuildDrawerButtons();

private:
    QToolButton* makeFixedButton(const QString& iconResource,
                                 const QString& placeholderLetter,
                                 const QString& tooltip,
                                 FixedAction action);
    QToolButton* makeDrawerButton(const QString& drawerKey,
                                  const QString& title,
                                  const QString& color,
                                  const QString& iconId);
    QToolButton* makeNewDrawerButton();
    void applyMirrorStyle();
    void refreshActiveStates();

    ProjectModel* m_model;
    QVBoxLayout* m_drawerLayout;
    QVBoxLayout* m_rootLayout;
    bool m_mirrored = false;

    QHash<int, QToolButton*> m_fixedButtons;        // FixedAction → button
    QHash<QString, QToolButton*> m_drawerButtons;   // drawerKey → button
    int m_activeFixed = -1;                          // -1 = nenhum
    QString m_activeDrawer;                          // vazio = nenhum
};
