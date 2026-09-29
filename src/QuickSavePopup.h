#pragma once

#include <QFrame>
#include <QPointer>
#include <QString>
#include <QVector>

#include <functional>

class QLabel;
class QLineEdit;
class QScrollArea;
class QTextEdit;
class QTimer;
class QToolButton;
class QVBoxLayout;

// Jogo rápido a partir de um trecho selecionado: "Criar documento disso" (em
// qual gaveta) e "Criar evento da linha do tempo" (em qual linha). Uma listinha
// logo abaixo da seleção, sem foco próprio — o cursor continua no editor, como
// no menu de seleção. Um clique salva com o nome tirado das primeiras palavras
// e o popup diz "✓ Salvo em X" e some. Pra dar outro nome, clique duplo no
// título de cima: ele vira campo (só aí o popup pega o teclado), Enter confirma
// e o clique na lista salva com o nome novo. Esc, clicar fora ou voltar a
// digitar fecham.
class QuickSavePopup : public QFrame
{
    Q_OBJECT
public:
    struct Target {
        QString key;
        QString title;
        QString color;
        QString iconId;   // :/icons/elements/<id>.svg; vazio = bolinha na cor
        QString hint;     // em cinza à direita ("da cena")
    };

    explicit QuickSavePopup(QTextEdit* editor, QWidget* parent = nullptr);

    // headerFmt tem um %1 pro nome ("Salvar “%1” em"). footer: linha cinza
    // embaixo (o "quando" do evento); vazio = sem.
    void presentAt(const QPoint& globalAnchor, const QString& headerFmt, const QString& name,
                   const QVector<Target>& targets, const QString& footer = QString());
    // O nome com que salvar (o sugerido, ou o que a pessoa digitou).
    QString name() const { return m_name; }
    // Depois do clique: a mensagem por um instante e some. actionText = link
    // pequeno ao lado ("ver"), que chama action.
    void flash(const QString& text, const QString& actionText = QString(),
               std::function<void()> action = {});

signals:
    void chosen(const QString& key);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void hideEvent(QHideEvent* e) override;

private:
    void applyTheme();
    void clearRows();
    void keepOnScreen();
    void refreshHeader();
    void startRename();
    void finishRename(bool keep);
    void setTakesFocus(bool on);

    QPointer<QTextEdit> m_editor;
    QString m_headerFmt;
    QString m_name;
    bool m_tookFocus = false;
    bool m_reflagging = false;
    QLabel* m_header = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QScrollArea* m_rows = nullptr;
    QVBoxLayout* m_rowsLay = nullptr;
    QLabel* m_footer = nullptr;
    QWidget* m_doneRow = nullptr;
    QLabel* m_done = nullptr;
    QToolButton* m_doneAction = nullptr;
    std::function<void()> m_action;
    QTimer* m_hideTimer = nullptr;
};
