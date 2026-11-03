// Construir: o catálogo embaixo (no jeito do Mapa) e a pílula em cima do selecionado.

#include "GalaxyPanel.h"
#include "GalaxyWidgets.h"

#include "../Theme.h"
#include "../carto/CartoAstrolabe.h"
#include "../carto/CartoCartela.h"
#include "../carto/CartoIcons.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace galaxy {

namespace {
constexpr int BAR_H = 158;
inline QString cat(const char* s) { return QCoreApplication::translate("galaxy::Catalog", s); }
const char* catIcon(const std::string& ic)
{
    if (ic == "planet") return "galPlanet";
    if (ic == "moon") return "galMoon";
    if (ic == "station") return "galStation";
    if (ic == "hull") return "galHull";
    return "galRing";
}
bool itemHasSize(const PlaceItem* it)
{
    if (!it) return true;
    if (it->kind == ItemKind::Planet) return it->art;
    const StructDef* d = it->kind == ItemKind::Struct ? structOf(it->id) : nullptr;
    return d && d->sizes;
}
}

void Panel::buildBuildBar()
{
    m_buildBar = makeFloat(QStringLiteral("cartoBuild"));
    auto* root = new QHBoxLayout(m_buildBar);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    m_buildCats = new QWidget(m_buildBar);
    m_buildCats->setObjectName(QStringLiteral("cartoCatCol"));
    m_buildCats->setAttribute(Qt::WA_StyledBackground, true);
    m_buildCats->setFixedWidth(152);
    auto* cl = new QVBoxLayout(m_buildCats);
    cl->setContentsMargins(6, 6, 6, 6);
    cl->setSpacing(2);
    for (const CatalogCat& c : catalog()) {
        auto* b = new QPushButton(cat(c.name), m_buildCats);
        b->setObjectName(QStringLiteral("cartoCat"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setFixedHeight(26);
        b->setIconSize(QSize(15, 15));
        b->setProperty("gmIcon", QString::fromLatin1(catIcon(c.icon)));
        const std::string id = c.id;
        connect(b, &QPushButton::clicked, this, [this, id]() { m_buildCat = id; renderBuildBar(); });
        m_catBtns.insert(c.id, b);
        cl->addWidget(b);
    }
    cl->addStretch();
    root->addWidget(m_buildCats);
    auto* main = new QVBoxLayout();
    main->setContentsMargins(12, 8, 8, 6);
    main->setSpacing(4);
    auto* head = new QHBoxLayout();
    head->setSpacing(6);
    m_buildTitle = new QLabel(m_buildBar);
    m_buildTitle->setObjectName(QStringLiteral("cartoBarTitle"));
    head->addWidget(m_buildTitle);
    head->addStretch();
    m_sizeRow = new QWidget(m_buildBar);
    auto* sr = new QHBoxLayout(m_sizeRow);
    sr->setContentsMargins(0, 0, 0, 0);
    sr->setSpacing(6);
    auto* cap = new QLabel(tr("TAMANHO"), m_sizeRow);
    cap->setObjectName(QStringLiteral("cartoCaption"));
    sr->addWidget(cap);
    auto* seg = new QFrame(m_sizeRow);
    seg->setObjectName(QStringLiteral("cartoMiniSeg"));
    auto* segl = new QHBoxLayout(seg);
    segl->setContentsMargins(2, 2, 2, 2);
    segl->setSpacing(0);
    const QList<QPair<QString, QString>> sizes = { { "p", tr("Pequeno") }, { "m", tr("Médio") }, { "g", tr("Grande") }, { "c", tr("Colossal") } };
    for (const auto& [v, l] : sizes) {
        auto* b = new QToolButton(seg);
        b->setObjectName(QStringLiteral("cartoMini"));
        b->setText(l);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        const QString val = v;
        connect(b, &QToolButton::clicked, this, [this, val]() { m_canvas->buildSize = val; syncBuildChecks(); m_canvas->update(); });
        m_sizeBtns.insert(v, b);
        segl->addWidget(b);
    }
    sr->addWidget(seg);
    head->addWidget(m_sizeRow);
    m_selectBtn = makeBtn(m_buildBar, QStringLiteral("cursor"), tr("Selecione uma peça pra arrastar, girar, renomear ou apagar"));
    m_selectBtn->setCheckable(true);
    connect(m_selectBtn, &QToolButton::clicked, this, [this]() { applyItem(nullptr); });
    head->addWidget(m_selectBtn);
    m_gearBtn = makeBtn(m_buildBar, QStringLiteral("gear"), tr("Escala das construções"));
    m_gearBtn->setCheckable(true);
    connect(m_gearBtn, &QToolButton::clicked, this, [this]() { m_gear->setVisible(!m_gear->isVisible()); renderBuildBar(); });
    head->addWidget(m_gearBtn);
    main->addLayout(head);
    m_cardScroll = new QScrollArea(m_buildBar);
    m_cardScroll->setObjectName(QStringLiteral("cartoCardScroll"));
    m_cardScroll->setFrameShape(QFrame::NoFrame);
    m_cardScroll->setWidgetResizable(true);
    m_cardScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_cardScroll->viewport()->setAutoFillBackground(false);
    m_cardScroll->viewport()->installEventFilter(this);
    m_cardBody = new QWidget(m_cardScroll);
    m_cardBody->setObjectName(QStringLiteral("cartoCardRow"));
    m_cardLay = new QHBoxLayout(m_cardBody);
    m_cardLay->setContentsMargins(0, 0, 0, 0);
    m_cardLay->setSpacing(4);
    m_cardLay->addStretch();
    m_cardScroll->setWidget(m_cardBody);
    main->addWidget(m_cardScroll, 1);
    root->addLayout(main, 1);
    m_buildBar->hide();

    // a engrenagem: a escala de tudo que é construído
    m_gear = new QFrame(this);
    m_gear->setObjectName(QStringLiteral("cartoCard"));
    m_gear->setFixedWidth(330);
    auto* gl = new QVBoxLayout(m_gear);
    gl->setContentsMargins(14, 12, 14, 14);
    gl->setSpacing(8);
    auto* gc = new QLabel(tr("ESCALA DAS CONSTRUÇÕES"), m_gear);
    gc->setObjectName(QStringLiteral("cartoCaption"));
    gl->addWidget(gc);
    auto* gr = new QHBoxLayout();
    gr->setSpacing(4);
    const QList<std::array<QString, 3>> scales = { { "1", tr("Normal"), tr("Tudo que é construído no tamanho de sempre") }, { "0.7", tr("Menor"), tr("Construções menores: o mesmo sistema parece maior") }, { "0.5", tr("Mínima"), tr("Ainda menores") } };
    for (const auto& s : scales) {
        auto* b = new QToolButton(m_gear);
        b->setObjectName(QStringLiteral("cartoSeg"));
        b->setText(s[1]); b->setToolTip(s[2]);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        const double v = s[0].toDouble();
        connect(b, &QToolButton::clicked, this, [this, v]() { m_canvas->worldScale = v; syncBuildChecks(); m_canvas->docChanged(); });
        m_scaleBtns.insert(s[0], b);
        gr->addWidget(b);
    }
    gl->addLayout(gr);
    m_gear->hide();

    // a pílula em cima do selecionado: nome, girar, tamanho e apagar
    m_selPill = makeFloat(QStringLiteral("cartoSelPill"));
    auto* pl = new QHBoxLayout(m_selPill);
    pl->setContentsMargins(10, 3, 4, 3);
    pl->setSpacing(2);
    m_selName = new QLineEdit(m_selPill);
    m_selName->setObjectName(QStringLiteral("cartoSelName"));
    m_selName->setFixedWidth(140);
    connect(m_selName, &QLineEdit::editingFinished, this, [this]() {
        if (m_pillKey.isEmpty()) return;
        const QString v = m_selName->text().trimmed();
        if (v.isEmpty() || v == m_doc.genName(m_pillKey)) m_doc.userNames.remove(m_pillKey);
        else m_doc.userNames.insert(m_pillKey, v);
        m_selName->clearFocus();
        m_canvas->setFocus();
        scheduleSave();
        m_canvas->update();
    });
    pl->addWidget(m_selName);
    m_selRot = new QWidget(m_selPill);
    auto* rl = new QHBoxLayout(m_selRot);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(2);
    auto* rotL = makeBtn(m_selRot, QStringLiteral("rotl"), tr("Girar (Q)"));
    auto* rotR = makeBtn(m_selRot, QStringLiteral("rotr"), tr("Girar (E)"));
    connect(rotL, &QToolButton::clicked, this, [this]() { rotatePiece(-PI_D / 12); });
    connect(rotR, &QToolButton::clicked, this, [this]() { rotatePiece(PI_D / 12); });
    rl->addWidget(rotL); rl->addWidget(rotR);
    pl->addWidget(m_selRot);
    m_selSizeRow = new QWidget(m_selPill);
    auto* szl = new QHBoxLayout(m_selSizeRow);
    szl->setContentsMargins(4, 0, 0, 0);
    szl->setSpacing(4);
    m_selSize = new QSlider(Qt::Horizontal, m_selSizeRow);
    m_selSize->setRange(40, 400);
    m_selSize->setSingleStep(5);
    m_selSize->setFixedWidth(96);
    m_selSize->setFocusPolicy(Qt::NoFocus);
    m_selSize->setToolTip(tr("Tamanho (teclas [ e ]; duplo clique volta pra 100%)"));
    m_selSize->installEventFilter(this);
    connect(m_selSize, &QSlider::valueChanged, this, [this](int v) {
        const Ref o = m_doc.resolve(m_pillKey);
        if (o.kind != RefKind::Build) return;
        o.build->scale = v / 100.0;
        m_selSizeV->setText(QStringLiteral("%1%").arg(v));
        scheduleSave();
        m_canvas->update();
    });
    szl->addWidget(m_selSize);
    m_selSizeV = new QLabel(m_selSizeRow);
    m_selSizeV->setObjectName(QStringLiteral("cartoValue"));
    m_selSizeV->setMinimumWidth(34);
    szl->addWidget(m_selSizeV);
    pl->addWidget(m_selSizeRow);
    auto* del = makeBtn(m_selPill, QStringLiteral("trash"), tr("Apagar (Delete)"));
    connect(del, &QToolButton::clicked, this, [this]() { if (!m_pillKey.isEmpty()) m_canvas->removeThing(m_pillKey); });
    pl->addWidget(del);
    m_selPill->hide();
}
QPixmap Panel::itemArt(const CatalogItem& it, int px)
{
    const QString key = QStringLiteral("%1:%2:%3").arg(QString::fromStdString(it.id)).arg(int(it.kind)).arg(px);
    auto f = m_artCache.find(key);
    if (f != m_artCache.end()) return f.value();
    Body* preview = nullptr;
    if (it.kind == ItemKind::Planet || it.kind == ItemKind::Moon) {
        PlaceItem pi;
        pi.id = it.id; pi.kind = it.kind;
        preview = m_canvas->previewOf(pi);
    }
    const QPixmap pm = QPixmap::fromImage(artImage(m_canvas, it.id, it.kind, px, 91, preview));
    m_artCache.insert(key, pm);
    return pm;
}
QPixmap Panel::modelArt(const std::string& type, uint32_t seed, int px)
{
    const QString key = QStringLiteral("m:%1:%2:%3").arg(QString::fromStdString(type)).arg(seed).arg(px);
    auto f = m_artCache.find(key);
    if (f != m_artCache.end()) return f.value();
    const QPixmap pm = QPixmap::fromImage(artImage(m_canvas, type, ItemKind::Struct, px, seed));
    m_artCache.insert(key, pm);
    return pm;
}
void Panel::applyItem(const CatalogItem* it)
{
    if (!it) { m_canvas->armItem(nullptr); renderBuildBar(); syncUi(); return; }
    const PlaceItem* cur = m_canvas->placing.get();
    if (cur && cur->id == it->id && cur->kind == it->kind) { m_canvas->armItem(nullptr); renderBuildBar(); syncUi(); return; }
    PlaceItem pi;
    pi.id = it->id; pi.kind = it->kind; pi.name = cat(it->name); pi.art = it->art;
    m_canvas->armItem(&pi);
    renderBuildBar();
    syncUi();
}
void Panel::renderBuildBar()
{
    if (m_tool != QLatin1String("build")) { m_buildBar->hide(); m_gear->hide(); return; }
    const QColor muted = Theme::toColor(Theme::textMuted()), a = Theme::toColor(Theme::accentDefault());
    for (auto it = m_catBtns.begin(); it != m_catBtns.end(); ++it) { it.value()->setChecked(it.key() == m_buildCat); it.value()->setIcon(carto::icon(it.value()->property("gmIcon").toString(), muted, a, 15)); }
    const CatalogCat* c = nullptr;
    for (const CatalogCat& q : catalog()) if (q.id == m_buildCat) c = &q;
    if (!c) return;
    m_buildTitle->setText(cat(c->name));
    for (QToolButton* b : m_cards) b->deleteLater();
    m_cards.clear();
    for (const CatalogItem& it : c->items) {
        auto* b = new QToolButton(m_cardBody);
        b->setObjectName(QStringLiteral("gmCard"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setFixedSize(86, 100);
        b->setIconSize(QSize(58, 58));
        b->setText(b->fontMetrics().elidedText(cat(it.name), Qt::ElideRight, 80));
        const StructDef* d = structOf(it.id);
        const BodyDef* bd = bodyDef(it.id);
        const char* desc = d ? d->desc : bd ? bd->desc : nullptr;
        b->setToolTip(desc ? cat(desc) : cat(it.name));
        const CatalogItem* ip = &it;
        connect(b, &QToolButton::clicked, this, [this, ip]() { applyItem(ip); });
        m_cardLay->insertWidget(m_cardLay->count() - 1, b);
        m_cards.append(b);
    }
    // A arte sai aos poucos, pra abrir o catálogo não travar o mapa.
    const QString catId = QString::fromStdString(m_buildCat);
    auto next = std::make_shared<std::function<void(int)>>();
    *next = [this, c, catId, next](int i) {
        if (i >= int(c->items.size()) || i >= m_cards.size() || QString::fromStdString(m_buildCat) != catId || m_tool != QLatin1String("build")) return;
        m_cards[i]->setIcon(QIcon(itemArt(c->items[size_t(i)], 58)));
        QTimer::singleShot(0, this, [next, i]() { (*next)(i + 1); });
    };
    (*next)(0);
    m_buildBar->show();
    syncBuildChecks();
    placeBuildBar();
}
void Panel::syncBuildChecks()
{
    const PlaceItem* p = m_canvas->placing.get();
    const CatalogCat* c = nullptr;
    for (const CatalogCat& q : catalog()) if (q.id == m_buildCat) c = &q;
    if (c) for (int i = 0; i < m_cards.size() && i < int(c->items.size()); i++) m_cards[i]->setChecked(p && p->id == c->items[size_t(i)].id && p->kind == c->items[size_t(i)].kind);
    m_sizeRow->setVisible(itemHasSize(p));
    for (auto it = m_sizeBtns.begin(); it != m_sizeBtns.end(); ++it) it.value()->setChecked(it.key() == m_canvas->buildSize);
    m_selectBtn->setChecked(!p);
    m_gearBtn->setChecked(m_gear->isVisible());
    for (auto it = m_scaleBtns.begin(); it != m_scaleBtns.end(); ++it) it.value()->setChecked(std::fabs(it.key().toDouble() - m_canvas->worldScale) < 1e-6);
    if (m_gear->isVisible()) {
        m_gear->adjustSize();
        const QPoint g = m_gearBtn->mapTo(this, QPoint(m_gearBtn->width(), 0));
        m_gear->move(std::max(8, g.x() - m_gear->width()), g.y() - m_gear->height() - 8);
        m_gear->raise();
    }
}
void Panel::placeBuildBar()
{
    if (m_tool != QLatin1String("build")) return;
    const int W = width(), H = height();
    const bool cart = m_ui == QLatin1String("cartela");
    const int panelRight = 16;
    int left = cart ? panelRight + m_rose->width() + 12 : panelRight;
    const int right = (cart ? m_cview->x() : m_toolPill->x()) - 12;
    int w = std::min(900, right - left), y = H - 14 - BAR_H;
    if (w < 460) { left = 16; w = std::min(900, W - 32); y = (cart ? m_cview->y() : m_astro->y()) - 8 - BAR_H; }
    const bool narrow = w < 660;
    if (narrow != m_buildNarrow) {
        m_buildNarrow = narrow;
        m_buildCats->setFixedWidth(narrow ? 46 : 152);
        for (const CatalogCat& c : catalog()) if (QPushButton* b = m_catBtns.value(c.id)) b->setText(narrow ? QString() : cat(c.name));
        const QStringList full = { tr("Pequeno"), tr("Médio"), tr("Grande"), tr("Colossal") };
        const QStringList brief = { tr("Peq."), tr("Méd."), tr("Gde."), tr("Col.") };
        const QStringList ks = { "p", "m", "g", "c" };
        for (int i = 0; i < 4; i++) m_sizeBtns.value(ks[i])->setText(narrow ? brief[i] : full[i]);
    }
    m_buildBar->setGeometry(left, y, w, BAR_H);
    m_buildBar->raise();
    if (m_gear->isVisible()) syncBuildChecks();
}
void Panel::syncSelPill(bool force)
{
    const QString k = m_tool == QLatin1String("build") && m_canvas->removable(m_canvas->sel) ? m_canvas->sel : QString();
    if (k == m_pillKey && !force) return;
    m_pillKey = k;
    if (k.isEmpty()) { m_selPill->hide(); return; }
    const Ref o = m_doc.resolve(k);
    if (!m_selName->hasFocus()) m_selName->setText(m_doc.labelOf(k));
    const bool piece = o.kind == RefKind::Build;
    m_selRot->setVisible(piece && !structOf(o.build->type)->mega);
    m_selSizeRow->setVisible(piece);
    if (piece) {
        m_selSize->blockSignals(true);
        m_selSize->setValue(qRound(o.build->scale * 100));
        m_selSize->blockSignals(false);
        m_selSizeV->setText(QStringLiteral("%1%").arg(qRound(o.build->scale * 100)));
    }
    m_selPill->adjustSize();
}
void Panel::placeSelPill()
{
    syncSelPill(false);
    if (m_pillKey.isEmpty()) return;
    const Hit* h = nullptr;
    for (const Hit& q : m_canvas->hits()) if (q.key == m_pillKey && !q.label) { h = &q; break; }
    if (!h) { m_selPill->hide(); return; }
    const int w = m_selPill->width(), ph = m_selPill->height();
    const QPoint at(std::max(8, std::min(width() - w - 8, int(h->x - w / 2.0))), std::max(60, int(h->y - h->r) - ph - 10));
    if (m_selPill->pos() != at) m_selPill->move(at);
    if (!m_selPill->isVisible()) { m_selPill->show(); m_selPill->raise(); }
}

} // namespace galaxy
