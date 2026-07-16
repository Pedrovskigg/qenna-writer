#pragma once

#include <QFrame>
#include <QString>

class QLabel;
class QScrollArea;
class ProjectModel;

// Popup read-only que aparece quando o mouse para em cima do botão Info da
// LeftBar. Mostra capa + nome + autor + gêneros + sinopse. Some quando o
// mouse sai do botão e do próprio popup.
class ProjectInfoHover : public QFrame {
    Q_OBJECT
public:
    explicit ProjectInfoHover(ProjectModel* model, QWidget* parent = nullptr);

    // Atualiza textos/capa a partir do model e mostra junto da âncora (canto
    // sup. direito do botão). Reaplicado a cada show.
    void presentNear(const QPoint& anchorGlobalTopRight);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

signals:
    void hoverLeft();

private slots:
    void applyPanelStyle();

private:
    void buildUi();
    void refreshFromModel();

    ProjectModel* m_model;
    QLabel* m_cover = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_authorLabel = nullptr;
    QLabel* m_genresLabel = nullptr;
    QLabel* m_synopsisLabel = nullptr;
    QScrollArea* m_synopsisScroll = nullptr;
};
