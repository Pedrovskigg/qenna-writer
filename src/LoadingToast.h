#pragma once

#include <QWidget>

class QLabel;
class QMovie;

// Toast de "Carregando…" reutilizável, no mesmo estilo visual dos outros
// toasts do app (ver onThemePanelRequested em MainWindow.cpp, o precedente
// direto deste widget). Hoje só mostra texto — chamado antes de um trecho
// bloqueante (ex.: MainWindow::loadProjectFrom) pra evitar a sensação de
// app travado em projetos grandes. m_imageLabel já existe pronto e oculto,
// reservado pra quando a mascote animada do Qenna existir: nesse dia, basta
// chamar setAnimation(movie) — nenhum widget novo pra criar.
class LoadingToast : public QWidget {
public:
    // hostWindow: janela top-level sobre a qual o toast é centralizado (e
    // também o Qt parent — cai automaticamente com a janela se algo não
    // fechar limpo). Sem show() automático: quem cria decide o momento.
    explicit LoadingToast(QWidget* hostWindow, const QString& text = QString());

    void setText(const QString& text);
    void setAnimation(QMovie* movie); // reservado pro futuro (mascote)

private:
    void reposition();

    QLabel* m_imageLabel = nullptr;
    QLabel* m_textLabel = nullptr;
};
