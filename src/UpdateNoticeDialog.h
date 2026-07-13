#pragma once

#include <QDialog>

// Aviso modal bloqueante, exibido no startup, avisando que o app não
// receberá mais atualizações nesta versão por causa da troca de nome/repo.
// Não tem botão de fechar/X, Esc não fecha — só sai via o botão "Entendi".
class UpdateNoticeDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateNoticeDialog(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void buildUi();
    bool m_acknowledged = false;
};
