#pragma once
// Fundo da folha (sugestão da Marley, 30/10; Lab V9Pv62DSYPG9PnAe1mNSGH):
// a seção "Fundo da folha" das folhas de capítulo e de cena, e a pintura do
// fundo no editor, por baixo do texto e por cima da cor da página.

#include "ProjectModel.h"

#include <QColor>
#include <QList>
#include <QRect>
#include <QWidget>

namespace Theme { struct MiraTheme; }
class QButtonGroup;
class QCheckBox;
class QLabel;
class QPainter;
class QSlider;
class QToolButton;

namespace PageBg {

// O fundo do tema atual (a "imagem da folha" do Personalizar), ou vazio.
PageBackground themeBackground();
PageBackground themeBackground(const Theme::MiraTheme& t);
// O fundo que pinta: o próprio; "nenhum" = nada; vazio = o do tema.
PageBackground resolve(const PageBackground& bg);

// Uma faixa do documento (coordenadas do documento) e o fundo dela: no
// capítulo, uma por cena; o fundo troca na quebra, esfumando.
struct Region {
    int y0 = 0, y1 = 0;
    PageBackground bg;
};

// Pinta as faixas numa tela do tamanho canvas (o editor inteiro, margens
// incluídas), só dentro de clip. yOffset leva o y do documento pra tela (o topo
// da área de texto menos a rolagem). A primeira faixa sobe e a última desce
// até a borda: as margens de cima e de baixo ficam com o fundo da cena delas.
void paint(QPainter& p, const QSize& canvas, int yOffset, const QList<Region>& regions, const QRect& clip);
bool anyFixed(const QList<Region>& regions);
// Escolhe uma imagem do disco e devolve a data URL (JPEG, até 1800 px); vazio = cancelou.
QString pickImage(QWidget* parent);

} // namespace PageBg

// A seção da folha de capítulo/cena. inherited = o que vem de cima e
// inheritFrom de onde ("book", "theme" ou "chapter"); chapterLevel mostra
// "Usar em todos os capítulos"; "tirar do livro" só quando vem do livro.
class PageBgField : public QWidget {
    Q_OBJECT
public:
    PageBgField(const PageBackground& current, const PageBackground& inherited, const QString& inheritFrom,
                bool chapterLevel, const QString& previewTitle, QWidget* parent = nullptr);
    PageBackground value() const { return m_bg; }
    bool useForAll() const;
    bool clearInherited() const { return m_clearInherited; }

signals:
    void sizeChanged();

private:
    void refresh();
    void pickImage();
    void pickColor();
    PageBackground shown() const;   // o que a faixa mostra (o próprio ou o herdado)

    PageBackground m_bg, m_inherited;
    bool m_chapterLevel = false;
    QString m_inheritFrom;
    bool m_clearInherited = false;
    bool m_more = false;
    QString m_title;

    class PageBgStrip* m_strip = nullptr;
    QWidget* m_emptyRow = nullptr;
    QToolButton* m_addImage = nullptr;
    QToolButton* m_addColor = nullptr;
    QWidget* m_links = nullptr;
    QToolButton* m_lnOther = nullptr;      // usar outra imagem
    QToolButton* m_lnColor = nullptr;      // usar uma cor
    QToolButton* m_lnNone = nullptr;       // nenhum aqui
    QToolButton* m_lnInherit = nullptr;    // voltar a herdar
    QToolButton* m_lnClearBook = nullptr;  // tirar do livro
    QWidget* m_opRow = nullptr;
    QSlider* m_op = nullptr;
    QLabel* m_opVal = nullptr;
    QToolButton* m_moreBtn = nullptr;
    QWidget* m_moreBox = nullptr;
    QSlider* m_blur = nullptr;
    QButtonGroup* m_fit = nullptr;
    QButtonGroup* m_scroll = nullptr;
    QCheckBox* m_all = nullptr;
};
