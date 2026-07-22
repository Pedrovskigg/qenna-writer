#include "ShelfBookItem.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QEasingCurve>
#include <QFont>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QTimer>
#include <QTransform>
#include <QVariantAnimation>
#include <QtMath>

#include <algorithm>
#include <array>

namespace {

struct Vec3 { qreal x = 0, y = 0, z = 0; };

// Gira em torno da linha vertical x=pivotX (o centro da lombada) — corpo
// rígido inteiro (lombada+capas+páginas juntos). Ângulo positivo abre pra
// revelar a capa da frente (hinge na borda direita); negativo revela a
// contracapa (hinge na borda esquerda) — simétrico, já que o pivô fica no
// meio das duas.
Vec3 rotateYaw(const Vec3& p, qreal thetaDeg, qreal pivotX)
{
    const qreal t = qDegreesToRadians(-thetaDeg);
    const qreal c = qCos(t), s = qSin(t);
    const qreal dx = p.x - pivotX;
    return { pivotX + dx * c + p.z * s, p.y, -dx * s + p.z * c };
}

// Gira em torno da linha horizontal y=pivotY (o meio da altura) — tomba o
// livro pra frente/trás, revelando o bloco de páginas de cima ou de baixo.
Vec3 rotatePitch(const Vec3& p, qreal phiDeg, qreal pivotY)
{
    const qreal t = qDegreesToRadians(-phiDeg);
    const qreal c = qCos(t), s = qSin(t);
    const qreal dy = p.y - pivotY;
    return { p.x, pivotY + dy * c + p.z * s, -dy * s + p.z * c };
}

// Projeção de perspectiva simples (equivalente ao `perspective: Npx` do CSS):
// quanto mais negativo o Z (mais longe da câmera), menor a escala.
QPointF project(const Vec3& p, qreal focal, qreal camDist)
{
    const qreal denom = qMax<qreal>(1.0, camDist - p.z);
    const qreal scale = focal / denom;
    return QPointF(p.x * scale, p.y * scale);
}

// Desenha um pixmap inteiro mapeado num quadrilátero de destino (já projetado
// em 2D), via QTransform::quadToQuad — o mesmo princípio que o navegador usa
// pra uma face plana sob `transform: perspective() rotateY()`.
void drawFace(QPainter* p, const QPixmap& pm, const std::array<QPointF, 4>& dstQuad)
{
    if (pm.isNull()) return;
    QPolygonF dst;
    for (const auto& pt : dstQuad) dst << pt;
    // Guarda de sanidade: um quad quase degenerado (área ~0, ex. de perfil
    // exato) deixa quadToQuad instável e pode gerar uma transformação com
    // escala absurda. Nesses casos é melhor pular a face (imperceptível,
    // já que ela estaria de perfil mesmo) do que arriscar um pixmap
    // gigante cobrindo a cena.
    const qreal area = qAbs(QPolygonF(dst).boundingRect().width()
                            * QPolygonF(dst).boundingRect().height());
    if (area < 0.5 || area > 4'000'000.0) return;

    QPolygonF src;
    src << QPointF(0, 0) << QPointF(pm.width(), 0)
        << QPointF(pm.width(), pm.height()) << QPointF(0, pm.height());
    QTransform t;
    if (!QTransform::quadToQuad(src, dst, t)) return;
    p->save();
    p->setTransform(t, true);
    p->drawPixmap(0, 0, pm);
    p->restore();
}

// Versão "vibrante" de uma face — satura e clareia um pouco, pixel a pixel.
// Cacheada uma vez (na construção do livro), nunca recalculada por frame:
// no hover, o paint() só troca qual pixmap já pronto vai pra tela.
QPixmap makeVibrant(const QPixmap& src, qreal satMul, qreal valAdd)
{
    if (src.isNull()) return src;
    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = QColor::fromRgba(line[x]);
            const int a = c.alpha();
            float h, s, v;
            c.getHsvF(&h, &s, &v);
            if (h < 0.0f) h = 0.0f;
            s = qBound(0.0f, float(s * satMul), 1.0f);
            v = qBound(0.0f, float(v + valAdd), 1.0f);
            QColor nc;
            nc.setHsvF(h, s, v);
            line[x] = qRgba(nc.red(), nc.green(), nc.blue(), a);
        }
    }
    return QPixmap::fromImage(img);
}

QPixmap makePagesStrip(qreal w, qreal h, bool topEdge)
{
    QPixmap pm(qMax(1, int(w)), qMax(1, int(h)));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient g(0, 0, 0, h);
    if (topEdge) {
        g.setColorAt(0.0, QColor(0xf6, 0xf1, 0xe7));
        g.setColorAt(1.0, QColor(0xe5, 0xdd, 0xd0));
    } else {
        g.setColorAt(0.0, QColor(0xe5, 0xdd, 0xd0));
        g.setColorAt(1.0, QColor(0xf0, 0xea, 0xd8));
    }
    p.fillRect(pm.rect(), g);
    p.setPen(QPen(QColor(0, 0, 0, 18), 1));
    for (int x = 3; x < w; x += 3) p.drawLine(QPointF(x, 0), QPointF(x, h));
    return pm;
}

QPixmap makeForeedge(qreal w, qreal h)
{
    QPixmap pm(qMax(1, int(w)), qMax(1, int(h)));
    pm.fill(QColor(0xf2, 0xec, 0xe0));
    QPainter p(&pm);
    p.setPen(QPen(QColor(0, 0, 0, 14), 1));
    for (int x = 2; x < w; x += 3) p.drawLine(QPointF(x, 0), QPointF(x, h));
    return pm;
}

// Recorta uma imagem (fundo sem texto ou a própria capa) pro tamanho w×h,
// preenchendo por completo (igual `background-size: cover`) e usando
// `posX` (0-100) como `background-position-x` — 0% mostra a borda esquerda
// da imagem escalada, 100% a direita. É essa fatia horizontal ajustável que
// gera a continuidade visual com a capa quando a arte é grande o bastante.
QPixmap cropBackgroundSlice(const QPixmap& src, qreal w, qreal h, int posX)
{
    QPixmap pm(qMax(1, int(w)), qMax(1, int(h)));
    pm.fill(Qt::transparent);
    if (src.isNull()) return pm;
    QPainter p(&pm);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QPixmap scaled = src.scaled(int(w), int(h), Qt::KeepAspectRatioByExpanding,
                                      Qt::SmoothTransformation);
    const qreal overflowW = qMax<qreal>(0, scaled.width() - w);
    const qreal overflowH = qMax<qreal>(0, scaled.height() - h);
    const qreal dx = -overflowW * qBound(0, posX, 100) / 100.0;
    const qreal dy = -overflowH / 2.0;
    p.drawPixmap(QPointF(dx, dy), scaled);
    return pm;
}

// Acabamento procedural (equivalente aos `repeating-linear-gradient`/
// `radial-gradient` do Mira 1 — CSS não tem port direto, então cada padrão
// vira um laço de linhas semi-transparentes + um degradê de base). Retorna
// pixmap nulo quando "" ou "none" (sem acabamento, base pura).
QPixmap renderFinishOverlay(const QString& finish, qreal w, qreal h)
{
    if (finish.isEmpty() || finish == QStringLiteral("none")) return {};
    QPixmap pm(qMax(1, int(w)), qMax(1, int(h)));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (finish == QStringLiteral("couro")) {
        p.setPen(QPen(QColor(0, 0, 0, 30), 1));
        for (qreal off = -h; off < w + h; off += 4) {
            p.drawLine(QPointF(off, 0), QPointF(off + h, h));
            p.drawLine(QPointF(off, h), QPointF(off + h, 0));
        }
        QLinearGradient shade(0, 0, w, 0);
        shade.setColorAt(0.0, QColor(0, 0, 0, 115));
        shade.setColorAt(0.3, QColor(255, 255, 255, 30));
        shade.setColorAt(0.6, QColor(0, 0, 0, 20));
        shade.setColorAt(1.0, QColor(0, 0, 0, 100));
        p.fillRect(pm.rect(), shade);
    } else if (finish == QStringLiteral("linho")) {
        p.setPen(QPen(QColor(255, 255, 255, 14), 1));
        for (qreal y = 0; y < h; y += 3) p.drawLine(QPointF(0, y), QPointF(w, y));
        for (qreal x = 0; x < w; x += 3) p.drawLine(QPointF(x, 0), QPointF(x, h));
        QLinearGradient shade(0, 0, w, 0);
        shade.setColorAt(0.0, QColor(0, 0, 0, 70));
        shade.setColorAt(0.45, QColor(255, 255, 255, 18));
        shade.setColorAt(1.0, QColor(0, 0, 0, 55));
        p.fillRect(pm.rect(), shade);
    } else if (finish == QStringLiteral("veludo")) {
        QRadialGradient r1(w * 0.35, h * 0.25, qMax(w, h) * 0.5);
        r1.setColorAt(0.0, QColor(255, 255, 255, 38));
        r1.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(pm.rect(), r1);
        QRadialGradient r2(w * 0.7, h * 0.75, qMax(w, h) * 0.4);
        r2.setColorAt(0.0, QColor(255, 255, 255, 20));
        r2.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(pm.rect(), r2);
        QLinearGradient shade(0, 0, w, h);
        shade.setColorAt(0.0, QColor(255, 255, 255, 20));
        shade.setColorAt(0.55, QColor(0, 0, 0, 90));
        shade.setColorAt(1.0, QColor(255, 255, 255, 15));
        p.fillRect(pm.rect(), shade);
    } else if (finish == QStringLiteral("fosco")) {
        p.setPen(QPen(QColor(0, 0, 0, 13), 1));
        for (qreal off = -h; off < w + h; off += 5) p.drawLine(QPointF(off, h), QPointF(off + h, 0));
        p.setPen(QPen(QColor(255, 255, 255, 8), 1));
        for (qreal off = -h; off < w + h; off += 8) p.drawLine(QPointF(off, 0), QPointF(off + h, h));
        QLinearGradient shade(0, 0, w, 0);
        shade.setColorAt(0.0, QColor(0, 0, 0, 50));
        shade.setColorAt(0.5, QColor(255, 255, 255, 13));
        shade.setColorAt(1.0, QColor(0, 0, 0, 43));
        p.fillRect(pm.rect(), shade);
    } else if (finish == QStringLiteral("metalico")) {
        p.setPen(QPen(QColor(255, 255, 255, 14), 1));
        for (qreal y = 0; y < h; y += 3) p.drawLine(QPointF(0, y), QPointF(w, y));
        QLinearGradient shine(0, 0, w, h);
        shine.setColorAt(0.0, QColor(255, 255, 255, 80));
        shine.setColorAt(0.18, QColor(0, 0, 0, 30));
        shine.setColorAt(0.38, QColor(255, 255, 255, 46));
        shine.setColorAt(0.58, QColor(0, 0, 0, 56));
        shine.setColorAt(0.78, QColor(255, 255, 255, 70));
        shine.setColorAt(1.0, QColor(0, 0, 0, 25));
        p.fillRect(pm.rect(), shine);
    }
    return pm;
}

// Contracapa: sombra da cor da lombada + gênero/sinopse (ou aviso de vazio),
// revelada quando o livro gira pro lado oposto da capa (yaw negativo).
QPixmap renderBackCover(const QColor& spineColor, const QString& genres,
                        const QString& synopsis, qreal w, qreal h)
{
    QPixmap pm(qMax(1, int(w)), qMax(1, int(h)));
    pm.fill(spineColor.darker(150));
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient shade(0, 0, 0, h);
    shade.setColorAt(0.0, QColor(0, 0, 0, 60));
    shade.setColorAt(1.0, QColor(0, 0, 0, 120));
    p.fillRect(pm.rect(), shade);

    const QString g = genres.trimmed();
    const QString s = synopsis.trimmed();
    const QRectF textRect(w * 0.08, h * 0.08, w * 0.84, h * 0.84);
    QFont base(QStringLiteral("Alegreya"));
    base.setStyleHint(QFont::Serif);

    if (g.isEmpty() && s.isEmpty()) {
        QFont f = base;
        f.setItalic(true);
        f.setPixelSize(qMax(9, int(w * 0.075)));
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 130));
        p.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap,
                   QCoreApplication::translate("ShelfBookItem",
                       "Nenhuma sinopse ou gênero cadastrado."));
        return pm;
    }

    qreal y = textRect.top();
    if (!g.isEmpty()) {
        QFont f = base;
        f.setBold(true);
        f.setCapitalization(QFont::AllUppercase);
        f.setPixelSize(qMax(8, int(w * 0.065)));
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 175));
        const QRectF r(textRect.left(), y, textRect.width(), h * 0.14);
        p.drawText(r, Qt::AlignLeft | Qt::TextWordWrap, g);
        y += h * 0.14;
    }
    if (!s.isEmpty()) {
        QFont f = base;
        f.setPixelSize(qMax(8, int(w * 0.062)));
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 205));
        const QRectF r(textRect.left(), y, textRect.width(), qMax<qreal>(0, textRect.bottom() - y));
        p.drawText(r, Qt::AlignLeft | Qt::TextWordWrap, s);
    }
    return pm;
}

} // namespace

ShelfBookItem::ShelfBookItem(const QString& path, const QString& name, const QString& author,
                             const QString& genres, const QString& synopsis,
                             const QPixmap& cover, const QColor& spineColor,
                             qreal spineWidth, const ShelfSpineStyle& style,
                             QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_path(path)
    , m_name(name)
    , m_author(author)
    , m_genres(genres)
    , m_synopsis(synopsis)
    , m_cover(cover)
    , m_spineColor(spineColor)
    , m_spineW(spineWidth)
    , m_style(style)
{
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);
    setToolTip(name);
    // Pivô da inclinação (m_baseTilt + o tilt cosmético de arrastar) fica na
    // BASE do livro (bottom center), não no meio — igual ao Mira 1
    // (`transform-origin: bottom center` do .shelfBookOuter). É o que faz o
    // livro parecer apoiado/encostado na prateleira ao inclinar, em vez de
    // girar flutuando desconectado da tábua.
    setTransformOriginPoint(m_spineW / 2.0, kBookH);
    buildSpineFace();
    buildCoverFace();
    buildBackCoverFace();
    buildAuxFaces();
}

ShelfBookItem::~ShelfBookItem() = default;

void ShelfBookItem::buildSpineFace()
{
    const int w = qMax(1, int(m_spineW));
    const int h = int(kBookH);
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath rounded;
    rounded.addRoundedRect(0, 0, w, h, 2, 2);
    p.setClipPath(rounded);

    // Camada 1: base — cor sólida, ou uma fatia horizontal do fundo da capa
    // (sem texto, se disponível) como textura.
    const bool useCoverTexture = m_style.imageTexture == QStringLiteral("cover");
    if (useCoverTexture && (!m_style.coverBg.isNull() || !m_cover.isNull())) {
        const QPixmap& src = !m_style.coverBg.isNull() ? m_style.coverBg : m_cover;
        p.drawPixmap(0, 0, cropBackgroundSlice(src, w, h, m_style.bgPosX));
    } else {
        p.fillRect(0, 0, w, h, m_spineColor);
    }

    // Camada 2: acabamento procedural por cima da base.
    const QPixmap finish = renderFinishOverlay(m_style.finish, w, h);
    if (!finish.isNull()) p.drawPixmap(0, 0, finish);

    // Camada 2b: sombreamento cilíndrico — sugere uma lombada levemente
    // arredondada em vez de uma chapa plana (igual `.shelfBook::before` +
    // `::after` do Mira 1). Só entra sem acabamento procedural específico,
    // que já traz seu próprio relevo e substituiria este no original.
    if (m_style.finish.isEmpty() || m_style.finish == QStringLiteral("none")) {
        QLinearGradient curve(0, 0, w, 0);
        curve.setColorAt(0.0, QColor(0, 0, 0, 76));
        curve.setColorAt(0.25, QColor(255, 255, 255, 20));
        curve.setColorAt(0.60, QColor(0, 0, 0, 15));
        curve.setColorAt(1.0, QColor(0, 0, 0, 64));
        p.fillRect(0, 0, w, h, curve);

        if (w > 24) {
            p.setPen(QPen(QColor(255, 255, 255, 33), 1));
            p.drawLine(QPointF(14, 20), QPointF(14, h - 20));
        }
    }

    // Véu de legibilidade: sem isso, o título some em cima de uma capa com
    // arte movimentada. Só entra quando a base é imagem — cor sólida já tem
    // contraste suficiente por si só.
    if (useCoverTexture && (!m_style.coverBg.isNull() || !m_cover.isNull())) {
        QLinearGradient scrim(0, 0, 0, h);
        scrim.setColorAt(0.0, QColor(0, 0, 0, 70));
        scrim.setColorAt(0.5, QColor(0, 0, 0, 40));
        scrim.setColorAt(1.0, QColor(0, 0, 0, 90));
        p.fillRect(0, 0, w, h, scrim);
    }

    p.setClipping(false);
    p.setPen(QPen(QColor(0, 0, 0, 60), 1));
    p.drawPath(rounded);

    // Camada 3: texto (título + autor), fonte/cor/tamanho/orientação/posição
    // configuráveis; vazio = padrão (Alegreya, creme, vertical, centro).
    QFont font(m_style.fontFamily.isEmpty() ? QStringLiteral("Alegreya") : m_style.fontFamily);
    font.setStyleHint(QFont::Serif);
    font.setBold(true);
    const int titlePx = m_style.fontSize > 0 ? m_style.fontSize : qBound(9, int(w * 0.16), 16);
    font.setPixelSize(titlePx);
    const QColor fontColor = m_style.fontColor.isValid() ? m_style.fontColor : QColor(245, 240, 226);
    const QColor shadowColor(0, 0, 0, 150);

    const QString title = m_name.isEmpty() ? QStringLiteral("Sem nome") : m_name;
    // Desenha uma sombra 1px atrás do texto (sempre) — garante contraste em
    // cima de qualquer fundo, mesmo com o véu acima.
    auto drawTitle = [&](const QRectF& rect, Qt::Alignment align, qreal shadowDx, qreal shadowDy) {
        p.setFont(font);
        p.setPen(shadowColor);
        p.drawText(rect.translated(shadowDx, shadowDy), align | Qt::TextWordWrap, title);
        p.setPen(fontColor);
        p.drawText(rect, align | Qt::TextWordWrap, title);
    };

    if (m_style.textOrientation == QStringLiteral("horizontal")) {
        // Sem rotação: eixo local x = largura, y = altura — top/bottom usam
        // as flags verticais normalmente.
        const Qt::Alignment posAlign = m_style.textPosition == QStringLiteral("top") ? Qt::AlignTop
                                      : m_style.textPosition == QStringLiteral("bottom") ? Qt::AlignBottom
                                                                                          : Qt::AlignVCenter;
        const QRectF textRect(4, 4, w - 8, h - 8);
        drawTitle(textRect, Qt::AlignHCenter | posAlign, 1, 1);
    } else {
        // Vertical, lendo de baixo pra cima. Depois do rotate(-90), o eixo
        // local x do retângulo passa a correr ao longo do COMPRIMENTO da
        // lombada na tela (não da largura) — então "topo/baixo" usam as
        // flags HORIZONTAIS (Left/Right), e o centro-de-largura fica sempre
        // em AlignVCenter, fixo.
        const Qt::Alignment posAlign = m_style.textPosition == QStringLiteral("top") ? Qt::AlignRight
                                      : m_style.textPosition == QStringLiteral("bottom") ? Qt::AlignLeft
                                                                                          : Qt::AlignHCenter;
        p.save();
        p.translate(w / 2.0, h / 2.0);
        p.rotate(-90);
        const QRectF textRect(-(h - 12) / 2.0, -(w - 8) / 2.0, h - 12, w - 8);
        drawTitle(textRect, posAlign | Qt::AlignVCenter, 1, 1);
        p.restore();
    }

    m_spineFace = pm;
    m_spineFaceVibrant = makeVibrant(m_spineFace, 1.25, 0.10);
}

void ShelfBookItem::buildCoverFace()
{
    const int w = int(kDepth);
    const int h = int(kBookH);
    if (m_cover.isNull()) {
        QPixmap pm(w, h);
        pm.fill(m_spineColor.darker(115));
        m_coverFace = pm;
        m_coverFaceVibrant = makeVibrant(m_coverFace, 1.25, 0.10);
        return;
    }
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QPixmap scaled = m_cover.scaled(w, h, Qt::KeepAspectRatioByExpanding,
                                          Qt::SmoothTransformation);
    p.drawPixmap(QPointF(-(scaled.width() - w) / 2.0, -(scaled.height() - h) / 2.0), scaled);
    m_coverFace = pm;
    m_coverFaceVibrant = makeVibrant(m_coverFace, 1.25, 0.10);
}

void ShelfBookItem::buildBackCoverFace()
{
    m_backCoverFace = renderBackCover(m_spineColor, m_genres, m_synopsis, kDepth, kBookH);
}

void ShelfBookItem::buildAuxFaces()
{
    // Cacheadas — antes eram geradas de novo em TODO paint() (inclusive
    // durante animações a 60fps para os 11+ livros da prateleira), o que
    // pesava muito. Só dependem de largura/profundidade, fixas por livro.
    m_pagesTopFace    = makePagesStrip(m_spineW, kDepth, true);
    m_pagesBottomFace = makePagesStrip(m_spineW, kDepth, false);
    m_foreedgeFace    = makeForeedge(m_spineW, kBookH);
}

QPainterPath ShelfBookItem::shape() const
{
    // Área de interação bem mais enxuta que o boundingRect (que precisa ser
    // generoso pra não cortar a projeção 3D em ângulos extremos). Isso é o
    // que corrige o mouse "roubando" outro livro longe: por padrão um
    // QGraphicsItem usa o boundingRect inteiro como área de clique/hover, e
    // aquele era gigante e todo sobreposto com os vizinhos.
    QPainterPath p;
    const qreal m = kDepth * 0.4;
    p.addRect(-m * 0.3, -16, m_spineW + m, kBookH + 32);
    return p;
}

QRectF ShelfBookItem::boundingRect() const
{
    // Espaço generoso o bastante pra projeção 3D não cortar em ângulos
    // extremos, mas sem exagero — um boundingRect gigante deixa CADA
    // repaint (a cada frame de animação, em CADA livro) invalidando uma
    // área enorme, o que pesa muito com vários livros na tela.
    const qreal margin = kDepth * 0.85;
    return QRectF(-margin, -margin, m_spineW + margin * 2.0, kBookH + margin * 2.0);
}

void ShelfBookItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal sw = m_spineW, bh = kBookH, d = kDepth;

    // Trava o desenho pra nunca vazar abaixo do "chão" do livro (y=bh local)
    // — com o pitch de repouso somado ao yaw, os cantos do bloco de páginas
    // (pagesBottom/foreedge) podem projetar um pouco além do rodapé da
    // lombada, aparecendo como um triângulo de papel sobrando por baixo.
    // Sem limite em cima/lados: ali o balanço pra fora É intencional (capa
    // abrindo, tombo de knock etc.).
    painter->setClipRect(QRectF(-4000.0, -4000.0, 8000.0, bh + 4000.0), Qt::IntersectClip);
    const qreal pivotX = sw / 2.0;
    const qreal pivotY = bh / 2.0;

    // Profundidade recua PRA TRÁS (-Z) a partir do plano da lombada, como um
    // livro de verdade em pé. Pivô no MEIO da lombada (não na borda): yaw
    // positivo abre a dobradiça direita (capa da frente), negativo abre a
    // esquerda (contracapa) — simétrico. Pitch tomba em torno do meio da
    // altura, revelando o bloco de páginas de cima ou de baixo.
    // O miolo (páginas + fore-edge) é ENCOLHIDO por uma margem em toda
    // borda — igual o `top/left/right/bottom: 4px` do `.shelfBookPagesTop/
    // Bottom/Foreedge` do Mira 1. Sem essa folga, o bloco de páginas fica
    // exatamente do tamanho da capa/lombada (0..sw, 0..bh, 0..-d) e QUALQUER
    // rotação/arredondamento faz um canto escapar por fora, virando o
    // triângulo de papel vazando que aparecia embaixo do livro.
    constexpr qreal kMioloInset = 7.0;
    const qreal mx0 = kMioloInset, mx1 = sw - kMioloInset;
    const qreal my0 = kMioloInset, my1 = bh - kMioloInset;
    const qreal mz0 = -kMioloInset, mz1 = -(d - kMioloInset);

    struct Face { QPixmap pm; std::array<Vec3, 4> corners; bool miolo; bool outline; qreal sortZ; };
    std::array<Face, 6> faces{ {
        { m_hovered ? m_spineFaceVibrant : m_spineFace,     { Vec3{0,0,0}, Vec3{sw,0,0}, Vec3{sw,bh,0}, Vec3{0,bh,0} }, false, true, 0 },
        { m_hovered ? m_coverFaceVibrant : m_coverFace,     { Vec3{sw,0,0}, Vec3{sw,0,-d}, Vec3{sw,bh,-d}, Vec3{sw,bh,0} }, false, true, 0 },
        { m_backCoverFace, { Vec3{0,0,0}, Vec3{0,0,-d}, Vec3{0,bh,-d}, Vec3{0,bh,0} }, false, false, 0 },
        { m_pagesTopFace,    { Vec3{mx0,my0,mz0}, Vec3{mx1,my0,mz0}, Vec3{mx1,my0,mz1}, Vec3{mx0,my0,mz1} }, true, false, 0 },
        { m_pagesBottomFace, { Vec3{mx0,my1,mz0}, Vec3{mx1,my1,mz0}, Vec3{mx1,my1,mz1}, Vec3{mx0,my1,mz1} }, true, false, 0 },
        { m_foreedgeFace, { Vec3{mx0,my0,mz1}, Vec3{mx1,my0,mz1}, Vec3{mx1,my1,mz1}, Vec3{mx0,my1,mz1} }, true, false, 0 },
    } };

    // Ordem de desenho por face: média dos 4 cantos (funciona bem pra
    // lombada/capa/contracapa, que têm cantos COMPARTILHADOS na dobradiça —
    // por isso é a média, e não o canto mais próximo, que separa
    // corretamente quem fica na frente ali). O miolo (páginas/fore-edge),
    // porém, tem sua própria faixa de Z quase tão larga quanto a da capa, e
    // por construção EMPATA com ela na média — sem um empurrão a mais, ele
    // vence esse empate em vários ângulos e pinta por cima da capa (o
    // vazamento visual). `kMioloBackBias` desempata sempre a favor da capa/
    // contracapa, mantendo a ordem relativa entre as 3 faces do miolo entre
    // si (o que importa pro "espiar" de cima/baixo durante tombo/pitch).
    constexpr qreal kMioloBackBias = 40.0;
    for (auto& f : faces) {
        qreal sumZ = 0;
        for (auto& c : f.corners) {
            c = rotateYaw(c, m_yaw, pivotX);
            c = rotatePitch(c, m_pitch, pivotY);
            sumZ += c.z;
        }
        f.sortZ = sumZ / 4.0 - (f.miolo ? kMioloBackBias : 0.0);
    }
    std::sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) { return a.sortZ < b.sortZ; });

    // Ancora o PÉ do livro (base da lombada, onde ela "encosta" na tábua) num
    // ponto fixo na tela, igual o `transform-origin: bottom center` do
    // Mira 1 — sem isso, a projeção de perspectiva (que escala X/Y conforme
    // Z) faz a base subir/flutuar conforme o livro gira, em vez de ficar
    // grudada na prateleira.
    Vec3 anchor{ sw / 2.0, bh, 0.0 };
    anchor = rotateYaw(anchor, m_yaw, pivotX);
    anchor = rotatePitch(anchor, m_pitch, pivotY);
    const QPointF anchorProjected = project(anchor, kFocal, kCamDist);
    const QPointF groundFix = QPointF(sw / 2.0, bh) - anchorProjected;

    // Ponto de fuga COMPARTILHADO pela prateleira inteira (não por livro) —
    // equivalente ao `perspective` do Mira 1 estar no container, não em cada
    // `.shelfBookOuter`. Sem isso, um livro na ponta da prateleira sempre
    // projeta como se estivesse bem no centro da câmera, perdendo o ângulo
    // dramático de estar "fora de eixo" que o Mira 1 tem de graça. `off` é a
    // distância (em cena) entre a origem local deste livro e o ponto de fuga;
    // desloca pra fora antes de projetar (aplicando a perspectiva relativa ao
    // ponto compartilhado) e desfaz o deslocamento depois (voltando pro
    // espaço local de pintura do item).
    const qreal off = pos().x() - m_sharedOriginX;

    QPainterPath silhouette;   // as 6 faces — usada só pro recorte do escurecimento
    QPainterPath outline;      // só lombada+capa — usada pro contorno de hover
    silhouette.setFillRule(Qt::WindingFill);
    outline.setFillRule(Qt::WindingFill);
    for (const auto& f : faces) {
        std::array<QPointF, 4> quad;
        for (int i = 0; i < 4; ++i) {
            Vec3 shared = f.corners[i];
            shared.x += off;
            QPointF pr = project(shared, kFocal, kCamDist);
            pr.rx() -= off;
            quad[i] = pr + groundFix;
        }
        drawFace(painter, f.pm, quad);
        QPolygonF poly;
        for (const auto& pt : quad) poly << pt;
        silhouette.addPolygon(poly);
        if (f.outline) outline.addPolygon(poly);
    }

    // Feedback de hover: em vez de um véu branco por cima, a lombada/capa já
    // foram trocadas pelas versões "vibrantes" cacheadas (mais saturação e
    // brilho) lá no início da função — sem custo por frame, já que são só
    // outro QPixmap pronto entrando no lugar do normal. Contorno fino e
    // brilhante só na lombada+capa (não no miolo/contracapa, que ficava
    // parecendo uma gaiola de arame por cima do livro todo).
    constexpr bool kOutlineEnabled = false; // toggle rápido pra comparar com/sem
    if (kOutlineEnabled && m_hovered) {
        painter->save();
        painter->setPen(QPen(QColor(255, 255, 255, 190), 1.4));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(outline);
        painter->restore();
    } else if (m_dimmed) {
        // Escurece um pouco enquanto OUTRO livro está em destaque — igual o
        // hover-darken da Estante. Mesmo recorte pela silhueta real.
        painter->save();
        painter->setClipPath(silhouette, Qt::IntersectClip);
        painter->fillRect(boundingRect(), QColor(0, 0, 0, 130));
        painter->restore();
    }
}

void ShelfBookItem::animateYawTo(qreal target, int durationMs)
{
    if (m_yawAnim) { m_yawAnim->stop(); m_yawAnim->deleteLater(); m_yawAnim = nullptr; }
    m_yawAnim = new QVariantAnimation(this);
    m_yawAnim->setStartValue(m_yaw);
    m_yawAnim->setEndValue(target);
    m_yawAnim->setDuration(durationMs);
    m_yawAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_yawAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_yaw = v.toReal();
        update();
    });
    m_yawAnim->start();
}

void ShelfBookItem::animatePitchTo(qreal target, int durationMs)
{
    if (m_pitchAnim) { m_pitchAnim->stop(); m_pitchAnim->deleteLater(); m_pitchAnim = nullptr; }
    m_pitchAnim = new QVariantAnimation(this);
    m_pitchAnim->setStartValue(m_pitch);
    m_pitchAnim->setEndValue(target);
    m_pitchAnim->setDuration(durationMs);
    m_pitchAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_pitchAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_pitch = v.toReal();
        update();
    });
    m_pitchAnim->start();
}

void ShelfBookItem::springBack()
{
    const qreal targetYaw = (m_hovered || (m_featuredPeek && !m_dragging)) ? kHoverYaw : 0.0;
    animateYawTo(targetYaw, 380);
    animatePitchTo(targetYaw != 0.0 ? kRestPitch : 0.0, 380);
    if (m_yawAnim) m_yawAnim->setEasingCurve(QEasingCurve::OutBack);
    if (m_pitchAnim) m_pitchAnim->setEasingCurve(QEasingCurve::OutBack);
}

void ShelfBookItem::triggerKnock(qreal magnitude, int delayMs)
{
    QTimer::singleShot(delayMs, this, [this, magnitude]() {
        animatePitchTo(magnitude, 240);
        QTimer::singleShot(280, this, [this]() { springBack(); });
    });
}

void ShelfBookItem::setBaseTilt(qreal deg)
{
    m_baseTilt = deg;
    if (!m_dragging) setRotation(deg);
}

void ShelfBookItem::setFeaturedPeek(bool on)
{
    if (m_featuredPeek == on) return;
    m_featuredPeek = on;
    if (!m_dragging && !m_hovered) springBack();
}

void ShelfBookItem::setSharedOriginX(qreal sceneX)
{
    if (qFuzzyCompare(m_sharedOriginX + 1.0, sceneX + 1.0)) return;
    m_sharedOriginX = sceneX;
    update();
}

void ShelfBookItem::setRestZValue(qreal z)
{
    m_restZValue = z;
    if (!m_hovered && !m_dragging) setZValue(m_restZValue);
}

void ShelfBookItem::setDimmed(bool on)
{
    if (m_dimmed == on) return;
    m_dimmed = on;
    update();
}

void ShelfBookItem::hoverEnterEvent(QGraphicsSceneHoverEvent* e)
{
    m_hovered = true;
    if (!m_dragging) {
        setZValue(m_restZValue + 100.0);
        animateYawTo(kHoverYaw, 220);
        animatePitchTo(kRestPitch, 220);
        emit hoverChanged(m_path, true);
    }
    QGraphicsObject::hoverEnterEvent(e);
}

void ShelfBookItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* e)
{
    m_hovered = false;
    if (!m_dragging) {
        setZValue(m_restZValue);
        const qreal targetYaw = m_featuredPeek ? kHoverYaw : 0.0;
        animateYawTo(targetYaw, 220);
        animatePitchTo(targetYaw != 0.0 ? kRestPitch : 0.0, 220);
        emit hoverChanged(m_path, false);
    }
    QGraphicsObject::hoverLeaveEvent(e);
}

void ShelfBookItem::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) {
        // Botão direito (e outros) não conta como clique/arrasto — deixa
        // passar pro contextMenuEvent cuidar do menu.
        e->ignore();
        return;
    }
    m_pressed = true;
    m_hasMoved = false;
    m_dragging = false;
    m_pressScenePos = e->scenePos();
    m_pressItemOrigin = pos();
    m_lastMoveScenePos = e->scenePos();
    m_lastMoveDelta = QPointF(0, 0);
    e->accept();
}

void ShelfBookItem::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
    if (!m_pressed) { e->ignore(); return; }
    const QPointF totalDelta = e->scenePos() - m_pressScenePos;
    if (!m_hasMoved) {
        if (QPointF::dotProduct(totalDelta, totalDelta) < kDragThreshold * kDragThreshold) {
            e->accept();
            return;
        }
        m_hasMoved = true;
    }
    if (!m_dragging) {
        m_dragging = true;
        setZValue(m_restZValue + 1000.0);
        if (m_yawAnim)   m_yawAnim->stop();
        if (m_pitchAnim) m_pitchAnim->stop();
        emit dragStarted(m_path);
    }

    // Só a componente horizontal move o livro na prateleira (reordenar); a
    // rotação ao vivo (abaixo) usa as duas.
    setPos(m_pressItemOrigin.x() + totalDelta.x(), m_pressItemOrigin.y());
    emit draggedTo(m_path, pos().x() + m_spineW / 2.0);

    // Segura o livro feito de verdade: gira seguindo o mouse, sem animação
    // (acompanha 1:1, como no Mira 1).
    m_yaw   = qBound(-kYawMax, totalDelta.x() * kYawFactor, kYawMax);
    m_pitch = qBound(-kPitchMax, totalDelta.y() * kPitchFactor, kPitchMax);
    setRotation(m_baseTilt + qBound(-18.0, totalDelta.x() * kTiltFactor, 18.0));
    update();

    m_lastMoveDelta = e->scenePos() - m_lastMoveScenePos;
    m_lastMoveScenePos = e->scenePos();
    e->accept();
}

void ShelfBookItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
    if (m_dragging) {
        setZValue(m_restZValue);
        // Solta o tilt cosmético, voltando pra inclinação de repouso (não
        // pra zero — cada livro tem a sua, feito o Mira 1).
        auto* rotAnim = new QPropertyAnimation(this, "rotation", this);
        rotAnim->setDuration(260);
        rotAnim->setEasingCurve(QEasingCurve::OutCubic);
        rotAnim->setStartValue(rotation());
        rotAnim->setEndValue(m_baseTilt);
        rotAnim->start(QAbstractAnimation::DeleteWhenStopped);

        // Arremesso rápido e predominantemente vertical pra baixo = dominó.
        const qreal vx = m_lastMoveDelta.x(), vy = m_lastMoveDelta.y();
        const bool knock = vy > 6.0 && qAbs(vy) > qAbs(vx) * 1.2;
        if (knock) {
            animatePitchTo(kKnockPitch, 220);
            QTimer::singleShot(260, this, [this]() { springBack(); });
            emit knocked(m_path);
        } else {
            springBack();
        }
        emit dragFinished(m_path);
    } else if (m_pressed && !m_hasMoved) {
        emit openRequested(m_path);
    }
    m_pressed = false;
    m_dragging = false;
    m_hasMoved = false;
    QGraphicsObject::mouseReleaseEvent(e);
}

void ShelfBookItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* e)
{
    QMenu menu;
    QAction* aEdit   = menu.addAction(QCoreApplication::translate("ShelfBookItem", "Editar projeto"));
    QAction* aCover  = menu.addAction(QCoreApplication::translate("ShelfBookItem", "Criar capa"));
    menu.addSeparator();
    QAction* aRemove = menu.addAction(QCoreApplication::translate("ShelfBookItem", "Remover dos recentes"));
    menu.addSeparator();
    QAction* aDelete = menu.addAction(QCoreApplication::translate("ShelfBookItem", "Excluir projeto"));
    QAction* chosen = menu.exec(e->screenPos());
    if      (chosen == aEdit)   emit editRequested(m_path);
    else if (chosen == aCover)  emit coverCreateRequested(m_path);
    else if (chosen == aRemove) emit removeRequested(m_path);
    else if (chosen == aDelete) emit deleteRequested(m_path);
}
