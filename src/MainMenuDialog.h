#pragma once

#include <QDialog>
#include <QPixmap>
#include <QString>
#include <QStringList>

class QFrame;
class QLabel;
class QPushButton;
class QScrollArea;
class QTimer;
class QVBoxLayout;

// Tela inicial / menu do app. Janela menor (não fullscreen). Layout:
//   - Coluna esquerda: header "Projetos" + botões Novo/Carregar e lista
//     vertical de recentes (cards com capa + nome + autor). Cada card abre
//     o projeto.
//   - Coluna direita: logo no topo + quote literário em baixo (importado
//     do Mira 1 via Quotes::pickRandom).
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
    void autoOpenChanged(const QString& path, bool enabled);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void applyDialogStyle();

private:
    void buildUi();
    void refreshRecents();
    void rotateQuote();

    QWidget* m_recentsHolder = nullptr;
    QVBoxLayout* m_recentsLayout = nullptr;
    QScrollArea* m_recentsScroll = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QLabel* m_quoteLabel = nullptr;
    QLabel* m_logoLabel = nullptr;
    QTimer* m_quoteTimer = nullptr;
    QPushButton* m_newBtn = nullptr;
    QPushButton* m_loadBtn = nullptr;
    QStringList m_recentPaths;
    QString m_autoOpenPath;
};
