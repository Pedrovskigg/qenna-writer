#pragma once

#include <QDialog>
#include <QPixmap>
#include <QString>
#include <QStringList>

class QComboBox;
class QFrame;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QTimer;
class QVBoxLayout;

// Tela inicial / menu do app, no figurino do Mira 1:
//   - Topo centralizado: logo + quote literário rotativo.
//   - Botões Novo/Carregar no canto esquerdo, acima do grid.
//   - Grid HORIZONTAL de recentes (capas com leve 3D estilo "vitrine"),
//     com scroll lateral. Hover em um card escurece os demais.
//   - Seletor de idioma discreto no topo direito.
//
// Aparece automaticamente no startup quando não há projeto carregado, e
// é invocado também quando o usuário pede "Carregar projeto" pela barra.
class MainMenuDialog : public QDialog {
    Q_OBJECT
public:
    explicit MainMenuDialog(QWidget* parent = nullptr);

    void setRecentProjects(const QStringList& paths);
    void setAutoOpenPath(const QString& path);

signals:
    void newProjectRequested();
    void loadProjectRequested();
    void openRecentRequested(const QString& path);
    void removeRecentRequested(const QString& path);
    void deleteProjectRequested(const QString& path);
    void autoOpenChanged(const QString& path, bool enabled);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void applyDialogStyle();

private:
    void buildUi();
    void refreshRecents();
    void rotateQuote();
    // Hover-darken: realça o card sob o cursor escurecendo todos os outros.
    // Passar nullptr restaura todos pra opacidade cheia.
    void setHoveredCard(QWidget* hovered);
    // Abre o diálogo de edição do projeto (nome/autor/gêneros/sinopse/capa)
    // e grava as alterações direto no índice. Atualiza o grid no fim.
    void editProject(const QString& path);
    // Mostra o diálogo de confirmação com countdown; ao confirmar, emite
    // deleteProjectRequested para o MainWindow fazer a exclusão de fato.
    void confirmDeleteProject(const QString& path);

    QWidget* m_recentsHolder = nullptr;
    QHBoxLayout* m_recentsRow = nullptr;
    QScrollArea* m_recentsScroll = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QLabel* m_quoteLabel = nullptr;
    QLabel* m_logoLabel = nullptr;
    QTimer* m_quoteTimer = nullptr;
    QPushButton* m_newBtn = nullptr;
    QPushButton* m_loadBtn = nullptr;
    QComboBox* m_langCombo = nullptr;
    QList<QWidget*> m_cards;
    QStringList m_recentPaths;
    QString m_autoOpenPath;
};
