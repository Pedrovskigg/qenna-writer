#pragma once
// Cursor suave do editor (o "deslizar" do Word): o cursor não pula, desliza
// até a posição nova em ~75ms, e a letra nova surge num fade de ~95ms (no fim
// da linha, onde se escreve). O cursor nativo do QTextEdit fica com largura 0
// e este desenha o próprio, por cima do viewport, na cor do texto do tema.
// Configurações › Interface liga/desliga.

#include <QColor>
#include <QElapsedTimer>
#include <QList>
#include <QPixmap>
#include <QPointer>
#include <QRectF>
#include <QWidget>

class QTextEdit;
class QTextDocument;
class QTimer;
class QVariantAnimation;

class SmoothCaret : public QWidget {
public:
    static SmoothCaret* install(QTextEdit* editor);
    // Outro editor com o mesmo efeito (a caixa de teste das Configurações).
    static SmoothCaret* attach(QTextEdit* editor);
    static bool enabledSetting();
    static void setEnabledSetting(bool on);
    // Tempos escolhidos pelo usuário (0 = sem deslize / letra na hora).
    static int glideMs();
    static int fadeMs();
    static void setGlideMs(int ms);
    static void setFadeMs(int ms);
    static constexpr int kDefaultGlideMs = 75;
    static constexpr int kDefaultFadeMs = 95;
    ~SmoothCaret() override;
    // Instância ativa (a do editor principal), pra o interruptor valer na hora.
    static SmoothCaret* instance();
    void setActive(bool on);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent*) override;

private:
    explicit SmoothCaret(QTextEdit* editor);
    QRectF caretRect() const;          // onde o cursor está, em coordenadas do viewport
    void retarget(bool animate);
    void watchDocument();
    void restartBlink();
    bool shouldDraw() const;
    void updateArea(const QRectF& a, const QRectF& b);
    // Letra nova entrando num fade: depois da tecla, a letra nasce coberta e a
    // cobertura vai saindo. Página de cor lisa (todos os temas de fábrica):
    // a cobertura é a própria cor da página (ou a do marcador debaixo da
    // letra), e vale em qualquer ponto da linha. Página translúcida: uma foto
    // do vazio tirada antes da tecla — só no fim da linha, onde o vazio existe.
    void prepareLetterFade();
    void startLetterFade();
    // Confere, a cada letra, o que o liga/desliga da opção conserta (tamanho e
    // ordem da camada); caixa-preta em smoothcaret.log quando algo sai errado.
    void ensureLayer();
    void diag(const QString& reason) const;
    static QString rectText(const QRect& r);
    bool m_typedLetter = false;
    // Cor lisa que a página pinta por trás do texto na altura y (viewport);
    // inválida se a página é translúcida.
    QColor solidPageColor(qreal y) const;
    struct Patch { QRectF rect; QPixmap pm; QColor fill; QPointF origin; QElapsedTimer clock; };
    QList<Patch> m_patches;
    QTimer* m_fadeTick = nullptr;
    mutable bool m_grabbing = false;
    bool m_pending = false;
    QColor m_pendingFill;
    QPixmap m_pendingPm;
    QPointF m_pendingOrigin;
    QRectF m_pendingFrom;
    int m_pendingPos = -1;
    // Só desliza quando quem moveu o cursor foi o teclado (digitar, apagar,
    // setas). Clique, abrir capítulo, busca, "Continuar": teleporta.
    bool m_keyMove = false;

    QPointer<QTextEdit> m_ed;
    QPointer<QTextDocument> m_doc;
    QRectF m_cur, m_from, m_to;
    QVariantAnimation* m_anim = nullptr;
    QTimer* m_blink = nullptr;
    bool m_on = true;          // fase do pisca
    bool m_active = false;
    int m_nativeWidth = 1;
    // Rolagem: posições guardadas em coordenadas do DOCUMENTO; na hora de
    // desenhar, desconta a rolagem atual. Nada acumula.
    QPointF scrollOffset() const;
    QRectF m_fromDoc;
};
