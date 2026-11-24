#pragma once
// A faixa de recados do menu principal (logo abaixo do título "Biblioteca") e a
// janela que mostra os recados inteiros. Os dados vêm do RecadosFeed (Recados.h).
// Desenho aprovado pelo Pe no Lab "Mural de Recados" (24/11/2026), opção C.

#include <QDialog>
#include <QFrame>

class QLabel;
class QToolButton;
class QPushButton;
class ElidedLabel;

class RecadosStrip : public QFrame {
    Q_OBJECT
public:
    explicit RecadosStrip(QWidget* parent = nullptr);

signals:
    void readRequested();

private:
    void refresh();
    void applyTheme();
    void step(int delta);

    QLabel* m_pin = nullptr;
    QLabel* m_eyebrow = nullptr;
    QPushButton* m_title = nullptr;
    ElidedLabel* m_body = nullptr;
    QToolButton* m_prev = nullptr;
    QLabel* m_count = nullptr;
    QToolButton* m_next = nullptr;
    QPushButton* m_read = nullptr;
    QToolButton* m_close = nullptr;
    int m_page = 0;
};

class RecadosDialog : public QDialog {
    Q_OBJECT
public:
    explicit RecadosDialog(QWidget* parent = nullptr);

private:
    void applyTheme();
};
