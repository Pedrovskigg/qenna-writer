#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include "ScreenplayFormat.h"

class GrannaBadge;
class QHBoxLayout;
class QLabel;
class QToolButton;
class QVBoxLayout;

// Faixa fixa no topo da "folha" do editor mostrando qual documento está sendo
// editado (capítulo/cena + variação). Existe porque com a TopToolbar na lateral
// não há mais onde mostrar o título — ver TopToolbar::positionDocTitle, que
// esconde o bloco de título no modo vertical de propósito.
//
// Por que um widget no topo do layout da editorColumn, e não um overlay dentro
// do viewport do editor: o texto rola DENTRO do editor, não dentro da folha,
// então uma faixa irmã do editor fica imóvel de graça — sem reposicionar a cada
// scroll, sem risco de piscar, e nenhum texto passa por baixo dela por
// construção.
//
// ALTURA FIXA de propósito, não por preguiça: o subtítulo é recalculado
// conforme o usuário rola (MainWindow liga refreshDocTitle ao valueChanged do
// scrollbar). Se a faixa crescesse ao ganhar subtítulo, a folha inteira daria
// um pulo no meio da rolagem.
class DocHeaderBar : public QWidget {
    Q_OBJECT
public:
    explicit DocHeaderBar(QWidget* parent = nullptr);

    // Mesma assinatura de TopToolbar::setDocumentTitle — o MainWindow alimenta
    // os dois pelo mesmo ponto, sem lógica duplicada.
    void setDocumentTitle(const QString& title, const QString& subtitle);

    // Foto do elemento (Element::image, data-URL) quando o documento aberto é
    // um personagem/cenário. Vazio esconde o avatar e a faixa volta a ser só
    // texto — é o mesmo caminho de capítulo/cena, não um estado especial.
    void setDocumentAvatar(const QString& imageDataUrl);
    // Documento é de um elemento (personagem/cenário): a foto aceita duplo
    // clique pra trocar. Sem foto, o lugar dela fica reservado e um círculo
    // tracejado aparece só com o mouse sobre a faixa.
    void setAvatarEditable(bool editable);

    void setSceneVarButtonVisible(bool visible);

    // Roteiro: guia no canto direito — em que elemento está a linha do cursor
    // e pra onde o Enter, o Tab e o Shift+Tab levam. Fora do roteiro, some.
    // Fica fora do layout (posição fixa no canto) pra não tirar o título do centro.
    void setScreenplayGuide(bool visible,
                            ScreenplayElement element = ScreenplayElement::Action,
                            bool emptyLine = false);
    // Para ancorar a VariationBar, como o botão equivalente da toolbar.
    QRect sceneVarButtonGlobalRect() const;

    // Aviso do Granna: quem disse a fala que acabou de ser escrita. Aparece no
    // lugar da foto do doc de personagem (à esquerda do título), por cima do
    // layout — o título não se mexe — e some sozinho:
    //  Certain  = foto, desliza de trás do título e volta (650 ms);
    //  Probable = foto com contorno tracejado na cor da voz, mesmo gesto (1 s);
    //  Unknown  = (?) com um anel se desenhando em volta (1,5 s).
    // Mouse em cima segura o aviso; clicar emite grannaMarkClicked().
    // sticky: em vez de sumir, faz só a entrada e FICA (cursor parado numa fala
    // que já existe) até releaseGrannaMark(), que faz a saída.
    enum class GrannaMark { Certain, Probable, Unknown };
    void showGrannaMark(GrannaMark mark, const QString& imageDataUrl, const QString& name,
                        const QColor& voice, const QString& toolTip, bool sticky = false);
    void releaseGrannaMark();
    void hideGrannaMark();
    // Popup "Quem disse?" aberto: o aviso fica parado e inteiro até soltar.
    void holdGrannaMark(bool hold);
    QRect grannaMarkGlobalRect() const;

signals:
    void sceneVarRequested();
    void avatarChangeRequested();
    void grannaMarkClicked();
    // Um aviso que some sozinho (não o fixo) terminou de sumir.
    void grannaAnnounceEnded();

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void applyTheme();
    void applyUiScale();
    void relayoutText();
    void refreshAvatar();
    void refreshGuide();
    int guideReserve() const;
    void placeGrannaMark();
    int grannaPad() const;
    int grannaSlide() const;

    QLabel* m_avatar = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QToolButton* m_varButton = nullptr;
    QWidget* m_guide = nullptr;
    QLabel* m_guideElement = nullptr;
    QLabel* m_guideKeys = nullptr;
    bool m_guideWanted = false;
    ScreenplayElement m_guideEl = ScreenplayElement::Action;
    bool m_guideEmpty = false;
    // Coluna do texto e linha do subtítulo: guardadas porque o alinhamento
    // delas muda conforme o avatar aparece (centralizado sem foto, à esquerda
    // da foto com ela).
    QVBoxLayout* m_textCol = nullptr;
    QHBoxLayout* m_subRow = nullptr;
    // Texto cru, sem elipse — o elidido é recalculado a cada resize, então o
    // original precisa sobreviver.
    QString m_rawTitle;
    QString m_rawSubtitle;
    // Data-URL cru: o pixmap é re-renderizado a cada mudança de escala da UI.
    QString m_avatarDataUrl;
    bool m_subtitleWanted = false;
    bool m_varWanted = false;
    bool m_avatarEditable = false;
    bool m_hover = false;
    QColor m_placeholderColor;
    QColor m_grannaAccent;
    GrannaBadge* m_granna = nullptr;
};
