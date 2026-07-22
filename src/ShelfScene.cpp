#include "ShelfScene.h"
#include "ShelfBookItem.h"

#include <QEasingCurve>
#include <QLinearGradient>
#include <QPainter>
#include <QPropertyAnimation>
#include <QtMath>

namespace {
constexpr qreal kBaseboardH = 26.0;
constexpr qreal kBookGap = 4.0;
constexpr qreal kShelfStartX = 6.0;
constexpr qreal kRowGap = kBaseboardH + 24.0; // espaço entre o chão de uma prateleira e o topo da próxima
}

ShelfScene::ShelfScene(QObject* parent) : QGraphicsScene(parent)
{
    setBackgroundBrush(QColor(0x0a, 0x0a, 0x0c));
    m_floorTexturePixmap = QPixmap(QStringLiteral(":/shelf/%1.jpg").arg(m_floorTextureId));
}

void ShelfScene::clearBooks()
{
    for (ShelfBookItem* b : m_books) removeItem(b);
    qDeleteAll(m_books);
    m_books.clear();
    m_dragPath.clear();
    m_dragOriginalIndex = -1;
    m_dragTargetIndex = -1;
    m_dragRowStart = m_dragRowEnd = -1;
    m_dragHomePos.clear();
}

ShelfBookItem* ShelfScene::addBook(const QString& path, const QString& name, const QString& author,
                                   const QString& genres, const QString& synopsis,
                                   const QPixmap& cover, const QColor& spineColor, qreal spineWidth,
                                   const ShelfSpineStyle& style)
{
    auto* book = new ShelfBookItem(path, name, author, genres, synopsis, cover, spineColor, spineWidth, style);
    connect(book, &ShelfBookItem::openRequested, this, &ShelfScene::openRequested);
    connect(book, &ShelfBookItem::editRequested, this, &ShelfScene::editRequested);
    connect(book, &ShelfBookItem::coverCreateRequested, this, &ShelfScene::coverCreateRequested);
    connect(book, &ShelfBookItem::removeRequested, this, &ShelfScene::removeRequested);
    connect(book, &ShelfBookItem::deleteRequested, this, &ShelfScene::deleteRequested);
    connect(book, &ShelfBookItem::dragStarted, this, &ShelfScene::onBookDragStarted);
    connect(book, &ShelfBookItem::draggedTo, this, &ShelfScene::onBookDraggedTo);
    connect(book, &ShelfBookItem::dragFinished, this, &ShelfScene::onBookDragFinished);
    connect(book, &ShelfBookItem::knocked, this, &ShelfScene::onBookKnocked);
    connect(book, &ShelfBookItem::hoverChanged, this, &ShelfScene::onBookHoverChanged);
    addItem(book);
    m_books.append(book);
    return book;
}

void ShelfScene::setWrapWidth(qreal w)
{
    m_wrapWidth = w;
}

void ShelfScene::computeFlowLayout(QList<QPointF>& outPos, QList<int>& outRow) const
{
    outPos.clear();
    outRow.clear();
    qreal x = kShelfStartX, y = 0.0;
    int row = 0;
    for (ShelfBookItem* b : m_books) {
        const qreal w = b->spineWidth();
        if (m_wrapWidth > 0 && x > kShelfStartX && x + w > m_wrapWidth) {
            ++row;
            x = kShelfStartX;
            y += ShelfBookItem::kBookH + kRowGap;
        }
        outPos.append(QPointF(x, y));
        outRow.append(row);
        x += w + kBookGap;
    }
}

int ShelfScene::rowCountFor(const QList<int>& rows) const
{
    return rows.isEmpty() ? 1 : rows.last() + 1;
}

int ShelfScene::indexOfPath(const QString& path) const
{
    for (int i = 0; i < m_books.size(); ++i)
        if (m_books[i]->path() == path) return i;
    return -1;
}

void ShelfScene::animateBookTo(ShelfBookItem* book, const QPointF& target)
{
    if (!book) return;
    auto* anim = new QPropertyAnimation(book, "pos", book);
    anim->setDuration(140);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setStartValue(book->pos());
    anim->setEndValue(target);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ShelfScene::relayout()
{
    QList<QPointF> pos;
    QList<int> rows;
    computeFlowLayout(pos, rows);
    for (int i = 0; i < m_books.size(); ++i) m_books[i]->setPos(pos[i]);
    applyRestStates(rows);

    const int rc = rowCountFor(rows);
    const qreal totalH = rc * ShelfBookItem::kBookH + (rc - 1) * kRowGap + kBaseboardH + 24.0;
    qreal totalW = m_wrapWidth;
    if (totalW <= 0.0) {
        qreal maxRight = 200.0;
        for (int i = 0; i < m_books.size(); ++i)
            maxRight = qMax(maxRight, pos[i].x() + m_books[i]->spineWidth());
        totalW = maxRight + kShelfStartX;
    }
    setSceneRect(0, -20, totalW, totalH);

    // Ponto de fuga compartilhado por toda a prateleira (meio da largura
    // visível) — ver ShelfBookItem::setSharedOriginX.
    const qreal originX = totalW / 2.0;
    for (ShelfBookItem* b : m_books) b->setSharedOriginX(originX);
}

void ShelfScene::applyRestStates(const QList<int>& rows)
{
    // Mesma paleta de ângulos do Mira 1 (SPINE_TILT_OFFSETS) — pequena
    // variação de "encostado no vizinho", por posição na prateleira.
    static const qreal kTilts[] = { -2.5, -1.4, -2.2, -0.9, -1.8, -2.6, -1.1, -2.0 };
    constexpr int kTiltCount = int(sizeof(kTilts) / sizeof(kTilts[0]));

    for (int i = 0; i < m_books.size(); ++i) {
        m_books[i]->setBaseTilt(kTilts[i % kTiltCount]);
        // Último livro de cada prateleira sempre mostra um pouco da capa —
        // igual o "--first" do Mira 1 (lá era o mais recente; aqui, o
        // último de cada linha, que é o que fica mais à mostra na tela).
        const bool isRowEnd = (i == m_books.size() - 1) || (i + 1 < rows.size() && rows[i + 1] != rows[i]);
        m_books[i]->setFeaturedPeek(isRowEnd);
        // Baseline de empilhamento = índice atual (esquerda→direita) — sem
        // isso, o Qt desempata sobreposições (peek permanente, ou o
        // instante em que um hover termina) pela ordem de CRIAÇÃO na cena,
        // que fica errada assim que a ordem visual muda (drag/reorder).
        m_books[i]->setRestZValue(i);
    }
}

void ShelfScene::onBookDragStarted(const QString& path)
{
    m_dragPath = path;
    m_dragOriginalIndex = indexOfPath(path);
    m_dragTargetIndex = m_dragOriginalIndex;
    m_dragHomePos.clear();
    m_dragRowStart = m_dragRowEnd = -1;
    if (m_dragOriginalIndex < 0) return;

    QList<int> rows;
    computeFlowLayout(m_dragHomePos, rows);
    const int myRow = rows[m_dragOriginalIndex];
    for (int i = 0; i < rows.size(); ++i) {
        if (rows[i] != myRow) continue;
        if (m_dragRowStart < 0) m_dragRowStart = i;
        m_dragRowEnd = i;
    }
}

void ShelfScene::onBookDraggedTo(const QString& path, qreal centerSceneX)
{
    if (path != m_dragPath || m_dragOriginalIndex < 0
        || m_dragHomePos.size() != m_books.size() || m_dragRowStart < 0) return;

    // Só considera os vizinhos DA MESMA prateleira — o arrasto só se move na
    // horizontal, então nunca "visita" outra linha de verdade.
    int newTarget = m_dragRowStart;
    for (int i = m_dragRowStart; i <= m_dragRowEnd; ++i) {
        if (i == m_dragOriginalIndex) continue;
        const qreal centerI = m_dragHomePos[i].x() + m_books[i]->spineWidth() / 2.0;
        if (centerI < centerSceneX) ++newTarget;
    }
    newTarget = qBound(m_dragRowStart, newTarget, m_dragRowEnd);
    if (newTarget == m_dragTargetIndex) return;
    m_dragTargetIndex = newTarget;

    const qreal draggedSpan = m_books[m_dragOriginalIndex]->spineWidth() + kBookGap;
    for (int i = m_dragRowStart; i <= m_dragRowEnd; ++i) {
        if (i == m_dragOriginalIndex) continue;
        qreal offset = 0.0;
        if (m_dragTargetIndex > m_dragOriginalIndex && i > m_dragOriginalIndex && i <= m_dragTargetIndex)
            offset = -draggedSpan;
        else if (m_dragTargetIndex < m_dragOriginalIndex && i >= m_dragTargetIndex && i < m_dragOriginalIndex)
            offset = draggedSpan;
        if (offset != 0.0)
            animateBookTo(m_books[i], m_dragHomePos[i] + QPointF(offset, 0));
        else
            animateBookTo(m_books[i], m_dragHomePos[i]);
    }
}

void ShelfScene::onBookDragFinished(const QString& path)
{
    if (path != m_dragPath) return;
    if (m_dragOriginalIndex >= 0 && m_dragTargetIndex >= 0
        && m_dragOriginalIndex != m_dragTargetIndex) {
        ShelfBookItem* moved = m_books.takeAt(m_dragOriginalIndex);
        m_books.insert(m_dragTargetIndex, moved);
    }
    m_dragPath.clear();
    m_dragOriginalIndex = -1;
    m_dragTargetIndex = -1;
    m_dragRowStart = m_dragRowEnd = -1;
    m_dragHomePos.clear();

    QList<QPointF> pos;
    QList<int> rows;
    computeFlowLayout(pos, rows);
    for (int i = 0; i < m_books.size(); ++i) animateBookTo(m_books[i], pos[i]);
    applyRestStates(rows);
    const int rc = rowCountFor(rows);
    const qreal totalH = rc * ShelfBookItem::kBookH + (rc - 1) * kRowGap + kBaseboardH + 24.0;
    qreal totalW = m_wrapWidth;
    if (totalW <= 0.0) {
        qreal maxRight = 200.0;
        for (int i = 0; i < m_books.size(); ++i)
            maxRight = qMax(maxRight, pos[i].x() + m_books[i]->spineWidth());
        totalW = maxRight + kShelfStartX;
    }
    setSceneRect(0, -20, totalW, totalH);
    const qreal originX = totalW / 2.0;
    for (ShelfBookItem* b : m_books) b->setSharedOriginX(originX);

    QStringList order;
    order.reserve(m_books.size());
    for (ShelfBookItem* b : m_books) order << b->path();
    emit orderChanged(order);
}

void ShelfScene::onBookKnocked(const QString& path)
{
    const int idx = indexOfPath(path);
    if (idx < 0) return;
    QList<QPointF> pos;
    QList<int> rows;
    computeFlowLayout(pos, rows);
    if (idx >= rows.size()) return;
    const int myRow = rows[idx];
    for (int i = 0; i < m_books.size(); ++i) {
        if (i == idx || rows[i] != myRow) continue;
        const int dist = qAbs(i - idx);
        if (dist > 4) continue;
        const qreal magnitude = qMax(0.0, 55.0 - dist * 14.0);
        if (magnitude <= 0.0) continue;
        m_books[i]->triggerKnock(magnitude, dist * 70);
    }
}

void ShelfScene::onBookHoverChanged(const QString& path, bool entered)
{
    if (entered) {
        m_hoveredPath = path;
    } else if (m_hoveredPath == path) {
        m_hoveredPath.clear();
    }
    for (ShelfBookItem* b : m_books)
        b->setDimmed(!m_hoveredPath.isEmpty() && b->path() != m_hoveredPath);
}

void ShelfScene::setFloorTexture(const QString& id)
{
    if (id == m_floorTextureId) return;
    m_floorTextureId = id;
    m_floorTexturePixmap = (id.isEmpty() || id == QStringLiteral("none"))
        ? QPixmap()
        : QPixmap(QStringLiteral(":/shelf/%1.jpg").arg(id));
    update();
}

QList<QPair<QString, QString>> ShelfScene::floorTextureChoices()
{
    QList<QPair<QString, QString>> list;
    list << qMakePair(QStringLiteral("none"), QStringLiteral("Nenhuma"));
    for (int i = 1; i <= 8; ++i)
        list << qMakePair(QStringLiteral("wood-%1").arg(i), QStringLiteral("Madeira %1").arg(i));
    for (int i = 1; i <= 3; ++i)
        list << qMakePair(QStringLiteral("steel-%1").arg(i), QStringLiteral("Aço %1").arg(i));
    return list;
}

void ShelfScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, QColor(0x0a, 0x0a, 0x0c));

    const qreal left = sceneRect().left();
    const qreal width = sceneRect().width();
    const bool hasTexture = !m_floorTexturePixmap.isNull();

    // Uma "prateleira" por linha: painel de fundo (com a textura escolhida,
    // se houver) + sombras internas simulando profundidade (equivalente aos
    // vários `box-shadow: inset` do Mira 1) + a tábua embaixo.
    for (qreal shelfTop = 0.0; shelfTop < sceneRect().bottom();
         shelfTop += ShelfBookItem::kBookH + kRowGap) {
        const QRectF panel(left, shelfTop, width, ShelfBookItem::kBookH);
        if (!panel.intersects(rect)) continue;

        // Teto — tira sólida e escura acima dos livros, como a "boca" de um
        // nicho de estante de verdade (igual .homeMenuShelfCeiling do Mira
        // 1). Sem isso, a prateleira parece "aberta" pra cima em vez de uma
        // peça de mobília com profundidade.
        constexpr qreal kCeilingH = 26.0;
        const QRectF ceiling(left, shelfTop - kCeilingH, width, kCeilingH);
        if (ceiling.intersects(rect)) {
            painter->fillRect(ceiling, QColor(10, 8, 6));
            painter->setPen(QPen(QColor(0, 0, 0, 160), 2));
            painter->drawLine(QPointF(ceiling.left(), ceiling.bottom()),
                              QPointF(ceiling.right(), ceiling.bottom()));
        }

        if (hasTexture) {
            const QPixmap tile = m_floorTexturePixmap.scaledToHeight(
                int(ShelfBookItem::kBookH), Qt::SmoothTransformation);
            painter->drawTiledPixmap(panel, tile);
            // Véu escuro uniforme por cima de TODA a textura — sem isso, a
            // foto de madeira/aço fica clara e chapada demais, brigando com
            // os livros. No Mira 1 isso é um overlay a 82% de opacidade
            // (darkBase = shadeHex(shelfFloorColor, 0.25)); aqui uso o mesmo
            // tom (marrom bem escuro) fixo, independente da textura escolhida.
            painter->fillRect(panel, QColor(27, 17, 9, 209));
        }

        // Sombras internas: topo (mais forte, como se a prateleira de cima
        // projetasse sombra), laterais e base — dá a profundidade que a
        // cor/textura sozinha não dá.
        QLinearGradient top(0, panel.top(), 0, panel.top() + panel.height() * 0.22);
        top.setColorAt(0.0, QColor(0, 0, 0, 170));
        top.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->fillRect(panel, top);

        // Luz de galeria — poças de luz quente espaçadas ao longo do topo,
        // como spots de vitrine. Quebra a escuridão uniforme e ilumina tanto
        // os livros quanto o vazio à direita (a prateleira é mais larga que
        // o conteúdo hoje), em vez de uma iluminação plana e sem graça.
        constexpr qreal kSpotSpacing = 380.0;
        constexpr qreal kSpotRadius = 300.0;
        for (qreal sx = kSpotSpacing * 0.5; sx < width; sx += kSpotSpacing) {
            QRadialGradient spot(QPointF(left + sx, panel.top() + 6.0), kSpotRadius);
            spot.setColorAt(0.0, QColor(255, 214, 158, 50));
            spot.setColorAt(0.55, QColor(255, 214, 158, 16));
            spot.setColorAt(1.0, QColor(255, 214, 158, 0));
            painter->fillRect(panel, spot);
        }

        // Sombra de contato: banda escura e concentrada bem junto da tábua
        // (equivalente ao `inset 0 -6px 18px rgba(0,0,0,0.6)` do Mira 1) —
        // sem ela, os livros perdem a referência de "pé no chão" e parecem
        // flutuar dentro do nicho, por mais que a geometria esteja encostada.
        QLinearGradient bottom(0, panel.bottom() - 24.0, 0, panel.bottom());
        bottom.setColorAt(0.0, QColor(0, 0, 0, 0));
        bottom.setColorAt(1.0, QColor(0, 0, 0, 165));
        painter->fillRect(panel, bottom);

        const qreal sideW = qMin<qreal>(60.0, width * 0.08);
        QLinearGradient leftG(panel.left(), 0, panel.left() + sideW, 0);
        leftG.setColorAt(0.0, QColor(0, 0, 0, 130));
        leftG.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->fillRect(QRectF(panel.left(), panel.top(), sideW, panel.height()), leftG);

        QLinearGradient rightG(panel.right() - sideW, 0, panel.right(), 0);
        rightG.setColorAt(0.0, QColor(0, 0, 0, 0));
        rightG.setColorAt(1.0, QColor(0, 0, 0, 130));
        painter->fillRect(QRectF(panel.right() - sideW, panel.top(), sideW, panel.height()), rightG);
    }
}

void ShelfScene::drawForeground(QPainter* painter, const QRectF& rect)
{
    // A tábua é desenhada no FOREGROUND (depois dos livros, não antes) —
    // ela precisa cobrir o pé dos livros por cima, como uma peça de
    // mobília real na frente deles, e não só um fundo atrás. Se ficasse no
    // drawBackground (pintado antes dos itens), qualquer coisa que vaze do
    // livro pra baixo do "chão" (miolo/páginas com o tombo de repouso, por
    // exemplo) ficaria por cima da tábua em vez de escondida atrás dela.
    const qreal left = sceneRect().left();
    const qreal width = sceneRect().width();
    const bool hasTexture = !m_floorTexturePixmap.isNull();

    for (qreal shelfTop = 0.0; shelfTop < sceneRect().bottom();
         shelfTop += ShelfBookItem::kBookH + kRowGap) {
        const QRectF board(left, shelfTop + ShelfBookItem::kBookH, width, kBaseboardH);
        if (!board.intersects(rect)) continue;

        if (hasTexture) {
            const QPixmap boardTile = m_floorTexturePixmap.scaledToHeight(
                int(kBaseboardH * 2), Qt::SmoothTransformation);
            painter->drawTiledPixmap(board, boardTile);
            painter->fillRect(board, QColor(27, 17, 9, 150));
        } else {
            QLinearGradient boardG(0, board.top(), 0, board.bottom());
            boardG.setColorAt(0.0, QColor(0x7a, 0x4d, 0x28));
            boardG.setColorAt(0.45, QColor(0x6b, 0x44, 0x23));
            boardG.setColorAt(1.0, QColor(0x4a, 0x2f, 0x18));
            painter->fillRect(board, boardG);
        }
        // Sombra no topo da própria tábua — reforça a mesma costura por
        // baixo (igual o `inset 0 -4px 8px` do `.homeMenuShelfFloor` do
        // Mira 1), pra costura ficar bem definida em vez de esmaecer.
        QLinearGradient boardTop(0, board.top(), 0, board.top() + qMin<qreal>(8.0, board.height()));
        boardTop.setColorAt(0.0, QColor(0, 0, 0, 120));
        boardTop.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->fillRect(board, boardTop);

        painter->setPen(QPen(QColor(255, 255, 255, 40), 1));
        painter->drawLine(QPointF(board.left(), board.top()), QPointF(board.right(), board.top()));
    }

    // Vinheta — escurece só a PONTA dos cantos da área visível. Raio pela
    // diagonal (não só a largura, que deixava o início do escurecimento
    // cedo demais e a vinheta comia o meio inteiro da imagem, não só as
    // quinas) e a maior parte do miolo fica 100% transparente.
    const qreal diag = qSqrt(rect.width() * rect.width() + rect.height() * rect.height());
    QRadialGradient vignette(rect.center(), diag * 0.55, rect.center());
    vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
    vignette.setColorAt(0.82, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 70));
    painter->fillRect(rect, vignette);
}
