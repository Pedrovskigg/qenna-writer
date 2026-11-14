#pragma once

#include <QGraphicsEffect>
#include <QList>
#include <QObject>
#include <QPointer>

#include <functional>

class QTimer;
class QVariantAnimation;
class QWidget;

// Relevo (13/11/2026, ideia do Pe, nascida no Topbar Lab): com a chave ligada
// nas Configurações, as barras (de cima, LeftBar e RightBar) perdem a caixa
// quando o mouse não está nelas e o que tem dentro fica gravado na mesa, em
// baixo-relevo. O mouse chega, a barra "acorda" inteira. Nada sai do lugar.
namespace Relevo {
bool enabled();
void setEnabled(bool on);

class Notifier : public QObject {
    Q_OBJECT
public:
    static Notifier* instance();
signals:
    void changed(bool on);
};
}

// Pinta a barra como gravação: a silhueta do conteúdo na cor da mesa, com uma
// linha de sombra em cima e uma de luz embaixo.
class RelevoEffect : public QGraphicsEffect {
    Q_OBJECT
public:
    using QGraphicsEffect::QGraphicsEffect;
    std::function<void()> beforeDraw;
    // A gravação de uma imagem do conteúdo.
    static QImage engrave(const QImage& src);
    // Transição em três camadas (t = 0 acordada, 1 gravada): o painel some
    // primeiro, e os ícones nunca desaparecem, só trocam a cor pela gravação.
    struct Layers { QPixmap panel, ink, engraved; };
    void setBlend(const Layers& layers, qreal t);
    void setBlendT(qreal t) { m_t = t; update(); }
    void clearBlend() { m_blending = false; m_layers = Layers(); update(); }
protected:
    void draw(QPainter* painter) override;
private:
    bool m_blending = false;
    Layers m_layers;
    qreal m_t = 1.0;
};

// Liga o Relevo numa barra: dorme quando o mouse sai, acorda quando ele chega.
class RelevoController : public QObject {
    Q_OBJECT
public:
    explicit RelevoController(QWidget* bar);
    // O mouse está em algo que é continuação da barra (as etiquetas da LeftBar).
    std::function<bool()> extraHover;
    // A barra pode dormir agora? (ex.: não no meio do modo focado)
    std::function<bool()> canRest;
    void refresh();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void wake();
    void rest();
    bool mouseOver() const;
    void stripBackgrounds();
    void restoreBackgrounds();
    void watch(QObject* o);
    void scheduleRepaint();
    bool ensureEffect();
    // As três camadas da barra agora (sem o efeito). Sai com os fundos tirados.
    RelevoEffect::Layers captureLayers();
    void animateTo(qreal target);
    QPointer<QVariantAnimation> m_anim;
    qreal m_t = 0.0;   // 0 acordada, 1 dormindo

    QWidget* m_bar = nullptr;
    QPointer<RelevoEffect> m_effect;
    QTimer* m_restTimer = nullptr;
    QTimer* m_repaintTimer = nullptr;
    bool m_resting = false;
    QList<QPointer<QWidget>> m_stripped;
};
