#pragma once

#include <QString>
#include <QWidget>

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

    void setSceneVarButtonVisible(bool visible);
    // Para ancorar a VariationBar, como o botão equivalente da toolbar.
    QRect sceneVarButtonGlobalRect() const;

signals:
    void sceneVarRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyTheme();
    void applyUiScale();
    void relayoutText();
    void refreshAvatar();

    QLabel* m_avatar = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QToolButton* m_varButton = nullptr;
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
};
