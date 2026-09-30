#pragma once
// Aviso de versão nova no menu principal: uma faixa larga no topo da área
// principal, com a versão, as novidades (recolhidas) e o "Baixar e instalar"
// com o progresso ali mesmo. Quem baixa é o MainWindow; a faixa só mostra o
// estado e pede as ações. Também serve pro "Tudo atualizado" do botão
// "Verificar atualizações", que antes caía num toast escondido atrás do menu.

#include <QFrame>

class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QToolButton;

class UpdateBanner : public QFrame {
    Q_OBJECT
public:
    explicit UpdateBanner(QWidget* parent = nullptr);

    // Versão nova do app, com as notas já em texto corrido.
    // incomplete: a instalação dessa versão parou no meio ("Concluir instalação").
    // ready: o instalador já está baixado e inteiro ("Instalar", sem baixar).
    void showAvailable(const QString& version, const QString& notes,
                       bool incomplete = false, bool ready = false);
    // Só informa (sem baixar nada), ex.: "Tudo atualizado".
    void showInfo(const QString& title, const QString& text);
    void setDownloading(int percent);
    void showError(const QString& message);
    void resetIdle();

signals:
    void downloadRequested();
    void cancelRequested();
    void dismissed();

private:
    void applyTheme();
    void setNotesOpen(bool open);
    QString modeText() const;

    QLabel* m_badge = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_sub = nullptr;
    QProgressBar* m_progress = nullptr;
    QToolButton* m_notesBtn = nullptr;
    QScrollArea* m_notesScroll = nullptr;
    QLabel* m_notes = nullptr;
    QPushButton* m_later = nullptr;
    QPushButton* m_action = nullptr;
    QPushButton* m_cancel = nullptr;
    bool m_info = false;
    bool m_downloading = false;
    bool m_incomplete = false;
    bool m_ready = false;
};
