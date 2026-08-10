#pragma once

#include <QFrame>
#include <QString>

class QLabel;
class QScrollArea;
class ProjectModel;

// Popup read-only que aparece quando o mouse para em cima do botão Info da
// LeftBar. Mostra capa + nome + autor + gêneros + sinopse do manuscrito
// ativo (fallback pro projeto quando o manuscrito não tem dados próprios —
// ver ProjectModel::manuscriptEffective*). Some quando o mouse sai do botão
// e do próprio popup.
class ProjectInfoHover : public QFrame {
    Q_OBJECT
public:
    explicit ProjectInfoHover(ProjectModel* model, QWidget* parent = nullptr);

    // Atualiza textos/capa a partir do model e mostra junto da âncora (canto
    // sup. direito do botão). Reaplicado a cada show. manuscriptIdOverride
    // vazio (padrão) mostra o manuscrito ATIVO do projeto; passar um id
    // mostra esse manuscrito específico (ex.: hover num item do seletor).
    void presentNear(const QPoint& anchorGlobalTopRight, const QString& manuscriptIdOverride = QString());

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

signals:
    void hoverLeft();

private slots:
    void applyPanelStyle();

private:
    void buildUi();
    void refreshFromModel(const QString& manuscriptIdOverride = QString());

    ProjectModel* m_model;
    QLabel* m_cover = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_seriesLabel = nullptr; // "Do projeto: X" — só quando o manuscrito ativo tem identidade própria
    QLabel* m_authorLabel = nullptr;
    QLabel* m_genresLabel = nullptr;
    QLabel* m_synopsisLabel = nullptr;
    QScrollArea* m_synopsisScroll = nullptr;
};
