#pragma once

#include <QFrame>

class QVBoxLayout;
class QHBoxLayout;
class QToolButton;
class ProjectModel;
class EditorHost;

// Popup discreto de variações da cena atual. Antes era uma barra fixa acima
// do editor (sempre visível, ocupando layout); agora só aparece sob demanda,
// aberto pelo botão scene-var na TopToolbar — mesmo padrão visual/comporta-
// mental do MarkerPickPopup (frameless, sombra, fecha ao clicar fora/Esc).
class VariationBar : public QFrame {
    Q_OBJECT
public:
    VariationBar(ProjectModel* model, EditorHost* host, QWidget* parent = nullptr);

    // Alterna: se já visível, fecha; senão reconstrói o conteúdo e abre
    // ancorado perto de `anchorGlobal` (retângulo global do botão clicado).
    void toggleNear(const QRect& anchorGlobal);

public slots:
    // Reconstrói o conteúdo se o popup já estiver aberto; fecha sozinho se
    // saiu do SceneDoc ou a cena atual sumiu (edição em outro lugar).
    void refresh();

private slots:
    void applyTheme();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void rebuildButtons();
    void setEmptyState(bool empty);
    void applyRootStyle();

    ProjectModel* m_model;
    EditorHost* m_host;
    QVBoxLayout* m_chipsColumn;
    QHBoxLayout* m_actionsRow;
    QToolButton* m_newBtn;
    QToolButton* m_primaryBtn;
    QToolButton* m_deleteBtn;
};
