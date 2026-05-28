#pragma once

#include "ProjectModel.h"

#include <QHash>
#include <QPainterPath>
#include <QPointF>
#include <QString>
#include <QWidget>

// Overlay transparente que desenha as linhas de vínculo entre cards de
// personagem em estilo "circuit board" (segmentos ortogonais, dots nos
// endpoints, hover label). Vive como child do listHost do DrawerListPanel
// e cobre toda a área da lista. Eventos de mouse passam por baixo
// (WA_TransparentForMouseEvents); o DrawerListPanel chama hitTestBond()
// quando recebe cliques na área e despacha pra cá.

struct BondCardPos {
    qreal x = 0;          // centro horizontal do card (no sistema do listHost)
    qreal y = 0;          // centro vertical do card
    qreal ay = 0;         // centro vertical da foto (anchor das linhas)
    qreal left = 0;
    qreal top = 0;
    qreal right = 0;
    qreal bottom = 0;
    qreal photoHalf = 35; // metade do lado da foto — endpoint das linhas na borda, não dentro
};

class BondsLayer : public QWidget {
    Q_OBJECT
public:
    explicit BondsLayer(QWidget* parent);

    void setBonds(const QList<CharacterBond>& bonds);
    void setCardPositions(const QHash<QString, BondCardPos>& positions);
    void setDrawerWidth(int w);

    // Drag preview line (em coords locais ao listHost).
    void setDragPreview(bool active, const QPointF& from, const QPointF& to);
    void clearDragPreview();

    // Hit-test pra cliques na área do listHost. Retorna o bondId encontrado,
    // ou string vazia se nenhum bond está sob a posição. Distância em px.
    QString hitTestBond(const QPointF& pos, qreal tolerance = 8.0) const;

    // Espaço lateral exigido pelos vínculos same-column (px de margem interna
    // adicional pra acomodar as lanes). Usado pelo painel pra alargar se preciso.
    int requiredSidePadding() const { return m_requiredSidePadding; }

    void setHoveredBond(const QString& bondId);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct BondGeometry {
        QString id;
        QString color;
        QString type;
        QPainterPath path;
        QList<QLineF> segments; // pra hit-test rápido
        QPointF endpoint1;
        QPointF endpoint2;
        QPointF labelPos;
    };

    void recompute();

    QList<CharacterBond> m_bonds;
    QHash<QString, BondCardPos> m_positions;
    QList<BondGeometry> m_geometry;
    QHash<QString, BondGeometry*> m_bondById; // ponteiros pra m_geometry, refeito a cada recompute
    QString m_hoveredBondId;
    bool m_dragActive = false;
    QPointF m_dragFrom;
    QPointF m_dragTo;
    int m_drawerWidth = 300;
    int m_requiredSidePadding = 0;
};
