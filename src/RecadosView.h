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
class QVariantAnimation;
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
    int m_page = 0;
};

// O lugar da faixa na linha do título. Segura a faixa e, quando a pessoa
// fecha no botão do canto, faz ela deslizar pra direita, pra dentro do botão
// (e voltar ao abrir). O espaço continua reservado, então nada pula na tela.
class RecadosSlot : public QWidget {
    Q_OBJECT
public:
    explicit RecadosSlot(QWidget* parent = nullptr);
    RecadosStrip* strip() const { return m_strip; }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    void sync(bool animate);
    void place();

    RecadosStrip* m_strip = nullptr;
    QVariantAnimation* m_anim = nullptr;
    qreal m_reveal = 1.0;   // 1 = faixa inteira à vista, 0 = recolhida
};

// Botão no canto direito da linha do título (ideia do Pe, 27/11/2026): fecha e
// abre a faixa do Mensageiro. Aberta, mostra as setinhas de recolher; fechada,
// mostra o alfinete da faixa, com o pontinho quando há mensagem não lida.
// Fechar vale até chegar mensagem nova (aí a faixa volta sozinha).
class RecadosToggle : public QToolButton {
    Q_OBJECT
public:
    explicit RecadosToggle(QWidget* parent = nullptr);

private:
    void refresh();
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
    // As regras da casa (texto do Pe, 25/11/2026): aparecem no primeiro "Enviar".
    // true = a pessoa concordou (fica lembrado em recados/rulesOk).
    bool confirmRules();
};
