#pragma once
// Gaveta do projeto: o botão Informações da LeftBar abre, no clique, o lugar
// onde se editam os dados do projeto — capa, nome, autor, gêneros, sinopse —
// com os números e os livros da saga. Tudo editável no próprio texto e salvo
// sozinho. Sai de trás da LeftBar como as outras gavetas (PanelMotion).

#include <QFrame>
#include <QPointer>

class ProjectModel;
class WordCounter;
class QLabel;
class QLineEdit;
class QTextEdit;
class QTimer;
class QToolButton;
class QVBoxLayout;
class QWidget;

class ProjectDrawerPanel : public QFrame {
    Q_OBJECT
public:
    explicit ProjectDrawerPanel(ProjectModel* model, QWidget* parent = nullptr);
    void setWordCounter(WordCounter* wc);

    void open();
    void closePanel();
    bool isPanelOpen() const { return isVisible(); }

signals:
    void panelClosed();
    void manuscriptActivated(QString manuscriptId);

private:
    void buildUi();
    void refresh();                 // dados do modelo → tela (sem mexer no campo em edição)
    void rebuildGenres();
    void rebuildStats();
    void rebuildBooks();
    void updateCover();
    void scheduleSave();
    void saveNow();
    void pickCover();
    QStringList genres() const;
    void setGenres(const QStringList& list);
    void fitSynopsis();
    void applyTheme();

    ProjectModel* m_model;
    QPointer<WordCounter> m_wordCounter;
    QToolButton* m_cover = nullptr;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_author = nullptr;
    QLabel* m_saved = nullptr;
    QWidget* m_genres = nullptr;
    QWidget* m_stats = nullptr;
    QTextEdit* m_synopsis = nullptr;
    QWidget* m_books = nullptr;
    QVBoxLayout* m_booksLayout = nullptr;
    QTimer* m_saveTimer = nullptr;
    QString m_coverDataUrl;
    QStringList m_genreList;
    bool m_loading = false;
};
