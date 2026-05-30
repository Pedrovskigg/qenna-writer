#pragma once

#include <QHash>
#include <QList>
#include <QPoint>
#include <QString>
#include <QWidget>

class QFrame;

class QVBoxLayout;
class QToolButton;
class QEvent;
class ProjectModel;

class LeftBar : public QWidget {
    Q_OBJECT
public:
    enum FixedAction {
        Info,
        Whiteboard,
        Timeline,
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

    // Acesso pontual a um botão fixo (pra plugar hover/tooltips externos).
    QToolButton* fixedButton(FixedAction action) const;

    // Modo focado: esconde botões/separadores e torna o fundo transparente,
    // mantendo a largura (não sai do layout) pra não deslocar o editor. A
    // imagem de fundo do tema aparece atrás.
    void setChromeHidden(bool hidden);

signals:
    void fixedActionTriggered(LeftBar::FixedAction action);
    void drawerSelected(QString drawerKey);
    void newDrawerRequested();
    void drawerContextRequested(QString drawerKey, QPoint globalPos);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void rebuildDrawerButtons();
    void applyTheme();

private:
    int drawerInsertIndexAt(const QPoint& posInBar) const;
    void updateDropIndicator(int targetIndex);
    void clearDropIndicator();
    void startDrawerDrag(QToolButton* btn, const QString& drawerKey);
    QToolButton* makeFixedButton(const QString& iconResource,
                                 const QString& placeholderLetter,
                                 const QString& tooltip,
                                 FixedAction action);
    QToolButton* makeDrawerButton(const QString& drawerKey,
                                  const QString& title,
                                  const QString& color,
                                  const QString& iconId);
    QToolButton* makeNewDrawerButton();
    QToolButton* m_newDrawerBtn = nullptr;
    QList<QFrame*> m_groupSeparators;
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

    // Drag state
    QPoint m_dragStartPos;
    QString m_pressedDrawerKey;                      // drawerKey do botão sob press
    QWidget* m_dropIndicator = nullptr;
    int m_dropTargetIndex = -1;
};
