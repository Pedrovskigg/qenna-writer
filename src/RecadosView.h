#pragma once
// A faixa de recados do menu principal (logo abaixo do título "Biblioteca") e a
// janela que mostra os recados inteiros. Os dados vêm do RecadosFeed (Recados.h).
// Desenho aprovado pelo Pe no Lab "Mural de Recados" (24/11/2026), opção C;
// no leitor, tudo em Crimson Pro (escolha dele entre 5 fontes, mesmo dia).

#include <QDialog>
#include <QFrame>
#include <QToolButton>

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

// Sininho "Recados" no topo da coluna do menu: o caminho de volta pros recados
// depois que a faixa foi fechada. Pontinho na cor de destaque quando há recado
// que a pessoa ainda não leu. Some quando não há recado nenhum.
class RecadosBell : public QToolButton {
    Q_OBJECT
public:
    explicit RecadosBell(QWidget* parent = nullptr);

signals:
    void readRequested();

private:
    void refresh();
};

struct Recado;

class RecadosDialog : public QDialog {
    Q_OBJECT
public:
    explicit RecadosDialog(QWidget* parent = nullptr);

private:
    void applyTheme();
    // Os comentários de um recado: "Comentários (3)" abre a lista e o
    // formulário (nome + comentário), que publica na hora.
    QWidget* buildComments(const Recado& r, QWidget* parent);
};
