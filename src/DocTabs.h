#pragma once

// Abas dos documentos abertos (15/11/2026, Abas Lab, opção "Fichário de baixo" aprovada pelo Pe): abinhas
// pequenas presas na beira de baixo, no canto da RightBar. O segredo é dele: ficam escondidas e só sobem
// quando o mouse chega no canto (com um atraso curto, pra não pular quando o mouse só passa por ali).
// A lista vem do EditorHost (os documentos abertos, na ordem em que foram abertos); fechar só tira da lista.

#include <QColor>
#include <QPixmap>
#include <QString>
#include <QVector>
#include <QWidget>

#include <functional>

class QTimer;
class QVariantAnimation;

class DocTabs : public QWidget {
public:
    struct Tab {
        enum Kind { Chapter, Element, Doc };
        Kind kind = Doc;
        QString title;
        QString mono;          // capítulo: o número
        QColor color;          // ficha: a cor da inicial; documento: a cor da gaveta
        QPixmap icon;          // documento: o ícone da gaveta; ficha: a foto (vazia = inicial)
        bool active = false;
    };

    explicit DocTabs(QWidget* parent);
    void setTabs(const QVector<Tab>& tabs);
    int tabCount() const { return m_tabs.size(); }
    // tamanho que o widget quer (a zona invisível do canto inclui as abas e uma folga em cima e à esquerda)
    QSize zoneSize() const;

    std::function<void(int)> activate;     // índice na lista inteira
    std::function<void(int)> close;
    std::function<void(QPoint)> showHidden; // o "+N": lista das mais antigas, a partir desse ponto (global)

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    struct Slot { int index; QRectF rect; QRectF closeRect; };
    QVector<Slot> layoutSlots(QRectF* moreRect) const;
    int tabWidth(const Tab& t) const;
    void setRevealTarget(bool up);
    qreal s(qreal v) const;

    QVector<Tab> m_tabs;
    int m_hot = -1;           // aba sob o mouse
    bool m_hotClose = false;
    bool m_hotMore = false;
    int m_pressed = -1;
    qreal m_reveal = 0;       // 0 escondidas, 1 de pé
    QTimer* m_showDelay = nullptr;
    QTimer* m_hideDelay = nullptr;
    QVariantAnimation* m_anim = nullptr;
};
