#pragma once

#include <QFrame>

class ReadAloudController;
class QComboBox;
class QLabel;
class QSlider;
class QToolButton;

// Controles da leitura em voz alta. Mesmo molde do AmbiencePanel (QFrame
// flutuante, tema reaplicado em themeChanged), com uma diferença deliberada:
// NÃO auto-fecha ao clicar fora. O gesto central da feature é ouvir e corrigir
// o texto ao mesmo tempo — um painel que some no primeiro clique no editor
// levaria junto o botão de pausar.
class ReadAloudPanel : public QFrame {
    Q_OBJECT
public:
    explicit ReadAloudPanel(ReadAloudController* controller, QWidget* parent = nullptr);

    // Abre ancorado ao retângulo (coords globais), sem roubar o foco do
    // editor: o autor continua digitando enquanto o painel está aberto.
    void showNear(const QRect& anchorGlobal, Qt::Edge barSide = Qt::TopEdge);

    // Mensagem de rodapé ("leitura interrompida: o texto mudou").
    void setStatusText(const QString& text);

signals:
    void playPauseRequested();
    void stopRequested();
    void restartRequested();

private slots:
    void applyTheme();

private:
    void buildUi();
    void refreshFromController();

    ReadAloudController* m_ctl = nullptr;
    QToolButton* m_playBtn = nullptr;
    QToolButton* m_stopBtn = nullptr;
    QToolButton* m_restartBtn = nullptr;
    QComboBox* m_voice = nullptr;
    QSlider* m_rate = nullptr;
    QLabel* m_rateLabel = nullptr;
    QLabel* m_status = nullptr;
    bool m_syncing = false;
};
