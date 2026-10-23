#pragma once

// Catálogo do Construir, no jeito do modo construção do The Sims: uma faixa baixa
// embaixo do mapa, com as categorias à esquerda e cartinhas com o desenho de
// verdade de cada item (o mesmo do mapa, numa câmera de vitrine). As variações
// são amostrinhas na cartinha (estilo do castelo); em cima da faixa só aparece o
// que vale pro item escolhido (tamanho do castelo, estrada, pincel). O artefato
// selecionado ganha uma pílula flutuando em cima dele (nome, girar, apagar).
// A faixa só mostra e muda as opções da mesa; quem edita o mapa é a mesa.

#include <QHash>
#include <QList>
#include <QObject>
#include <QPair>
#include <QPixmap>
#include <QString>

#include <functional>

class QFrame;
class QHBoxLayout;
class QLabel;
class QAbstractButton;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSlider;
class QToolButton;
class QWidget;

namespace carto {

class Canvas;
struct Doc;

class BuildBar : public QObject {
    Q_OBJECT
public:
    BuildBar(QWidget* host, Canvas* canvas, Doc* doc);
    static constexpr int kHeight = 158;

    void setActive(bool on);                 // a ferramenta Construir ligou ou desligou
    bool active() const { return m_active; }
    void sync();                             // estado da mesa → faixa
    void place(int left, int right, int bottom);   // a faixa entre left e right, com o pé em bottom
    QRect geometry() const;
    void placeSel();                         // a pílula do selecionado (depois de cada quadro da mesa)
    void applyTheme();
    bool owns(const QWidget* w) const;       // o foco está no nome do selecionado
    bool escape();                           // fecha a engrenagem; true se fechou algo

    std::function<void()> changed;                 // a faixa mudou uma opção: o painel se ressincroniza
    std::function<void(double)> setScale;          // escala do mundo
    std::function<void(double)> rotate;            // gira o selecionado
    std::function<void()> remove;                  // apaga o selecionado

protected:
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    struct Cat { QString id, name, icon; QStringList items; };
    void pickCat(const QString& id);
    void pickKind(const QString& kind);
    void pickStyle(const QString& style);
    void renderRow();
    void syncCtx();
    void setNarrow(bool on);
    QPixmap thumb(const QString& kind, const QString& style) const;
    QString castleStyle() const;

    QWidget* m_host;
    Canvas* m_canvas;
    Doc* m_doc;
    bool m_active = false, m_narrow = false;
    QList<Cat> m_cats;
    QString m_cat = QStringLiteral("uni"), m_rowCat, m_lastKind;

    QFrame* m_bar = nullptr;
    QWidget* m_catCol = nullptr;
    QHash<QString, QPushButton*> m_catBtns;
    QLabel* m_title = nullptr;
    QWidget* m_castleRow = nullptr;
    QWidget* m_roadRow = nullptr;
    QWidget* m_brushRow = nullptr;
    QLabel* m_sizeCap = nullptr;
    QLabel* m_brushCap = nullptr;
    QHash<QString, QToolButton*> m_sizeBtns, m_rmodeBtns, m_rkindBtns, m_scaleBtns;
    QSlider* m_brush = nullptr;
    QLabel* m_brushValue = nullptr;
    QToolButton* m_selectBtn = nullptr;
    QToolButton* m_gearBtn = nullptr;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_rowBody = nullptr;
    QHBoxLayout* m_rowLay = nullptr;
    QList<QWidget*> m_cards;
    // engrenagem: escala do mundo e cidades sugeridas
    QFrame* m_gear = nullptr;
    QToolButton* m_showGen = nullptr;
    // pílula do selecionado
    QFrame* m_sel = nullptr;
    QLineEdit* m_selName = nullptr;
    QString m_selId;
    bool m_placeQueued = false;

    QList<QPair<QAbstractButton*, QString>> m_icons;
    mutable QHash<QString, QPixmap> m_thumbs;
};

} // namespace carto
