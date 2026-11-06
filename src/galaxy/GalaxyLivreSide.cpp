// Criação Livre: a direita (onde estou, o sistema em pé, a ficha e o Gerar) e a gaveta flutuante de cada lugar
// (documentos em árvore e conexões), com a cara da gaveta de verdade. Os seletores de tipo, classe, estrela,
// lugar, construção, cor e onde salvar também moram aqui.

#include "GalaxyLivre.h"
#include "GalaxyDoc.h"
#include "GalaxyCanvas.h"
#include "GalaxyHab.h"
#include "GalaxyPanel.h"
#include "GalaxyStructures.h"

#include "../ProjectModel.h"
#include "../TerritorioStore.h"
#include "../Theme.h"
#include "../carto/CartoIcons.h"

#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace galaxy {

namespace {
inline QString coreT(const char* s) { return QCoreApplication::translate("galaxy::Core", s); }
inline QString catT(const char* s) { return QCoreApplication::translate("galaxy::Catalog", s); }
inline QColor col(const QString& c) { return Theme::toColor(c); }
const QStringList COLORS = { QStringLiteral("#7fb2e6"), QStringLiteral("#5fc7c0"), QStringLiteral("#79c98a"), QStringLiteral("#c4cf6d"), QStringLiteral("#e0b35e"), QStringLiteral("#e08a5e"), QStringLiteral("#d96f6f"), QStringLiteral("#c77fc4"), QStringLiteral("#9587de"), QStringLiteral("#a8b4c6"), QStringLiteral("#d8cdb8") };
const std::vector<Type> PLANET_TYPES = { Type::Temperado, Type::Oceano, Type::Deserto, Type::Arido, Type::Gelado, Type::Lava, Type::Toxico, Type::Anao, Type::Gigante, Type::Quente, Type::GeloG, Type::Subnetuno, Type::Fortaleza, Type::Ecumeno, Type::Casco, Type::Forja };
const std::vector<Type> MOON_TYPES = { Type::Rochosa, Type::Gelada, Type::Vulcanica, Type::Temperada };
QLabel* lab(const QString& text, const QString& obj, QWidget* parent, bool wrap = false)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(obj);
    l->setWordWrap(wrap);
    return l;
}
void clearLay(QLayout* l)
{
    while (QLayoutItem* it = l->takeAt(0)) {
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        else if (QLayout* c = it->layout()) clearLay(c);
        delete it;
    }
}
QPushButton* btn(const QString& text, const QString& obj, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(obj);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}
QToolButton* mini(const QString& icon, const QString& tip, bool on, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(QStringLiteral("lvNavMini"));
    b->setCheckable(true);
    b->setChecked(on);
    b->setIcon(carto::icon(icon, col(Theme::textMuted()), col(Theme::accentDefault()), 14));
    b->setIconSize(QSize(14, 14));
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}
QPixmap circlePlus(const QColor& c, int size)
{
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, 1.6);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawEllipse(QRectF(2, 2, size - 4, size - 4));
    const double m = size / 2.0, L = (size - 4) * 0.34;
    p.drawLine(QPointF(m - L, m), QPointF(m + L, m));
    p.drawLine(QPointF(m, m - L), QPointF(m, m + L));
    return pm;
}
QPixmap dot(const QColor& c, int px)
{
    QPixmap pm(px * 2, px * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(QRectF(1, 1, px - 2, px - 2));
    return pm;
}
// A caixa do sistema: o sol cortado no canto de cima, como no Caderno, mas em pé.
class SunBox : public QFrame {
public:
    SunBox(const System* s, QWidget* parent) : QFrame(parent)
    {
        if (!s) return;
        m_has = true;
        m_hole = s->stars[0].kind == StarKind::BuracoNegro;
        m_c = Livre::starShown(s->stars[0].kind);
    }
protected:
    void paintEvent(QPaintEvent* e) override
    {
        QFrame::paintEvent(e);
        if (!m_has) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 12, 12);
        p.setClipPath(clip);
        const QPointF c(-6, -40);
        const double R = 96;
        QRadialGradient g(c, R);
        if (m_hole) {
            g.setColorAt(0, Qt::black); g.setColorAt(0.38, Qt::black); g.setColorAt(0.41, QColor(255, 176, 112)); g.setColorAt(0.45, QColor(255, 140, 60, 128)); g.setColorAt(0.6, QColor(255, 120, 50, 36)); g.setColorAt(0.74, QColor(255, 120, 50, 0));
        } else {
            QColor a = m_c; a.setAlpha(92);
            QColor z = m_c; z.setAlpha(0);
            g.setColorAt(0, m_c); g.setColorAt(0.44, m_c); g.setColorAt(0.45, a); g.setColorAt(0.72, z);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(c, R, R);
    }
private:
    bool m_has = false, m_hole = false;
    QColor m_c;
};
// A janela escura do planeta: o 3D por baixo, o nome, os botões e o zoom por cima.
class WinBox : public QFrame {
public:
    using QFrame::QFrame;
    QWidget *live = nullptr, *title = nullptr, *acts = nullptr, *zoom = nullptr, *ask = nullptr;
protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QFrame::resizeEvent(e);
        if (live) live->setGeometry(rect().adjusted(1, 1, -1, -1));
        if (title) { title->adjustSize(); title->move(12, 10); }
        if (zoom) { zoom->adjustSize(); zoom->move(width() - zoom->width() - 8, 8); }
        if (acts) { acts->setMaximumWidth(width() - 16); acts->adjustSize(); acts->move(8, height() - acts->height() - 8); }
        if (ask) { ask->setFixedWidth(width() - 40); ask->adjustSize(); ask->move(20, height() - ask->height() - 44); }
    }
};
// A forma do território num quadrinho: a galáxia de fundo e as manchas pintadas.
class TerrMini : public QWidget {
public:
    std::function<void(QPainter&, QRectF)> paint;
    using QWidget::QWidget;
protected:
    void paintEvent(QPaintEvent*) override { QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::SmoothPixmapTransform); if (paint) paint(p, rect()); }
};
}

QPixmap starIcon(const System* s, int px)
{
    QPixmap pm(px * 2, px * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const bool hole = s && s->stars[0].kind == StarKind::BuracoNegro;
    const QColor c = Livre::starShown(s ? s->stars[0].kind : StarKind::G);
    const QPointF m(px / 2.0, px / 2.0);
    QRadialGradient g(m, px / 2.0);
    if (hole) { g.setColorAt(0, Qt::black); g.setColorAt(0.5, Qt::black); g.setColorAt(0.58, QColor(255, 176, 112)); g.setColorAt(1, QColor(255, 120, 50, 0)); }
    else { QColor h = c; h.setAlpha(110); QColor z = c; z.setAlpha(0); g.setColorAt(0, c); g.setColorAt(0.45, c); g.setColorAt(0.5, h); g.setColorAt(1, z); }
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(m, px / 2.0, px / 2.0);
    return pm;
}
// Estilos de anel com cara própria, valendo pra qualquer planeta (o motor sozinho só varia dentro da família).
struct RingStyle { const char* id; const char* name; V3 tint; double alpha, inner, outer; int shape; };   // shape: 0 falha, 1 denso, 2 suave, 3 duplo
const RingStyle RING_STYLES[8] = {
    { "dourado", QT_TRANSLATE_NOOP("galaxy::Livre", "Dourado"), { 0.88, 0.8, 0.62 }, 0.85, 1.25, 2.3, 0 },
    { "gelo", QT_TRANSLATE_NOOP("galaxy::Livre", "Gelo"), { 0.84, 0.92, 1 }, 0.78, 1.3, 2.1, 1 },
    { "poeira", QT_TRANSLATE_NOOP("galaxy::Livre", "Poeira"), { 0.62, 0.52, 0.42 }, 0.38, 1.4, 2.5, 2 },
    { "fino", QT_TRANSLATE_NOOP("galaxy::Livre", "Fino"), { 0.88, 0.85, 0.8 }, 0.9, 1.72, 1.98, 1 },
    { "duplo", QT_TRANSLATE_NOOP("galaxy::Livre", "Duplo"), { 0.82, 0.77, 0.7 }, 0.72, 1.3, 2.45, 3 },
    { "escuro", QT_TRANSLATE_NOOP("galaxy::Livre", "Escuro"), { 0.34, 0.32, 0.36 }, 0.8, 1.35, 2.2, 0 },
    { "rosado", QT_TRANSLATE_NOOP("galaxy::Livre", "Rosado"), { 0.93, 0.64, 0.62 }, 0.62, 1.3, 2.2, 2 },
    { "esmeralda", QT_TRANSLATE_NOOP("galaxy::Livre", "Esmeralda"), { 0.5, 0.86, 0.7 }, 0.6, 1.35, 2.3, 0 },
};
std::unique_ptr<Rings> styledRings(int style, uint32_t seed)
{
    const RingStyle& st = RING_STYLES[style];
    auto R = std::make_unique<Rings>();
    Rng r(seed);
    const double ph0 = r() * 6, ph1 = r() * 6, ph2 = r() * 6, gap = 0.5 + r() * 0.2;
    const int n = int(R->bands.size());
    for (int i = 0; i < n; i++) {
        const double t = double(i) / (n - 1);
        const double nz = 0.5 + 0.22 * std::sin(t * 31 + ph0) + 0.16 * std::sin(t * 67 + ph1) + 0.12 * std::sin(t * 143 + ph2);
        double v = st.shape == 1 ? 0.72 + 0.28 * nz : st.shape == 2 ? 0.35 + 0.35 * nz : nz;
        if (st.shape == 0 && std::fabs(t - gap) < 0.04) v *= 0.08;
        if (st.shape == 3 && t > 0.36 && t < 0.6) v *= 0.04;
        v *= smooth(0, 0.1, t) * (1 - smooth(0.84, 1, t));
        R->bands[size_t(i)] = float(clamp(v, 0, 1));
    }
    for (int c = 0; c < 3; c++) R->tint[size_t(c)] = clamp(st.tint[size_t(c)] + (r() - 0.5) * 0.08, 0, 1);
    R->alpha = st.alpha; R->inner = st.inner; R->outer = st.outer;
    return R;
}

// ── onde estou ──
QWidget* Livre::crumbsEl(QWidget* parent)
{
    auto* w = new QWidget(parent);
    w->setObjectName(QStringLiteral("lvCrumbs"));
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(5);
    auto* gal = btn(tr("Galáxia"), QString(), w);
    connect(gal, &QPushButton::clicked, this, [this]() { select(QString()); });
    h->addWidget(gal);
    if (!m_sel.isEmpty()) {
        QStringList chain = pathOf(m_sel);
        chain << m_sel;
        for (int i = 0; i < chain.size(); i++) {
            h->addWidget(lab(QStringLiteral("›"), QStringLiteral("lvCrumbSep"), w));
            QString t = label(chain[i]);
            if (t.size() > 22) t = t.left(21) + QStringLiteral("…");
            if (i == chain.size() - 1) h->addWidget(lab(t, QStringLiteral("lvCrumbHere"), w));
            else { auto* b = btn(t, QString(), w); const QString k = chain[i]; connect(b, &QPushButton::clicked, this, [this, k]() { select(k); }); h->addWidget(b); }
        }
    }
    h->addStretch();
    return w;
}

// ── o sistema em pé ──
QWidget* Livre::orbitRow(const QString& w, bool moon, QWidget* parent)
{
    auto* r = btn(QString(), QStringLiteral("lvOrbit"), parent);
    const bool ghost = isGhost(w);
    r->setProperty("on", m_sel == w);
    r->setProperty("near", m_sel != w && !m_sel.isEmpty() && isInside(m_sel, w));
    auto* h = new QHBoxLayout(r);
    h->setContentsMargins(moon ? 26 : 4, 4, 8, 4);
    h->setSpacing(10);
    const Body* b = bodyOf(w);
    int px = 56;
    if (moon) px = 28;
    else if (b && (b->type == Type::Gigante || b->type == Type::Quente)) px = 76;
    else if (b && (b->type == Type::GeloG || b->type == Type::Subnetuno)) px = 66;
    else if (b && b->type == Type::Anao) px = 34;
    auto* slot = new QLabel(r);
    slot->setFixedSize(moon ? 54 : 84, std::max(px, 30) + 4);
    slot->setAlignment(Qt::AlignCenter);
    QPixmap pm = ball(w, px, true);
    if (ghost) { QPixmap faded(pm.size()); faded.setDevicePixelRatio(pm.devicePixelRatio()); faded.fill(Qt::transparent); QPainter q(&faded); q.setOpacity(0.5); q.drawPixmap(0, 0, pm); q.end(); pm = faded; }
    slot->setPixmap(pm);
    slot->setAttribute(Qt::WA_TransparentForMouseEvents);
    h->addWidget(slot);
    auto* colv = new QVBoxLayout();
    colv->setSpacing(1);
    auto* nh = new QHBoxLayout();
    nh->setSpacing(4);
    auto* nm = lab(label(w), QStringLiteral("lvOrbitName"), r);
    nm->setProperty("ghost", ghost);
    nm->setAttribute(Qt::WA_TransparentForMouseEvents);
    nh->addWidget(nm);
    if (isCapital(w)) { QWidget* s = starMark(r); s->setAttribute(Qt::WA_TransparentForMouseEvents); nh->addWidget(s); }
    nh->addStretch();
    colv->addLayout(nh);
    bool au = false;
    const int c = habOf(w, &au);
    QString sub = typeLabelOf(w);
    if (c) sub += QStringLiteral(" · ") + tr("Classe %1").arg(QString::fromLatin1(habRoman(c)));
    auto* sl = lab(sub, QStringLiteral("lvSmall"), r);
    sl->setAttribute(Qt::WA_TransparentForMouseEvents);
    colv->addWidget(sl);
    h->addLayout(colv, 1);
    if (ghost) h->addWidget(ghostActs(w, r));
    else { connect(r, &QPushButton::clicked, this, [this, w]() { select(w); }); peekable(r, w); }
    r->setMinimumHeight(std::max(px, 30) + 12);
    return r;
}
QWidget* Livre::systemView(const QString& sysKey, QWidget* parent)
{
    System* s = sysOf(sysKey);
    auto* v = new SunBox(s, parent);
    v->setObjectName(QStringLiteral("lvSysv"));
    auto* l = new QVBoxLayout(v);
    l->setContentsMargins(10, 12, 10, 10);
    l->setSpacing(4);
    auto* head = new QVBoxLayout();
    head->setContentsMargins(70, 0, 0, 6);
    head->setSpacing(0);
    auto* nm = btn(label(sysKey), QStringLiteral("lvSysName"), v);
    connect(nm, &QPushButton::clicked, this, [this, sysKey]() { select(sysKey); });
    head->addWidget(nm);
    auto* st = btn(s ? coreT(starKindLabel(s->stars[0].kind, s->stars[0].pulsar)) + QStringLiteral(" ▾") : QString(), QStringLiteral("lvSysStar"), v);
    connect(st, &QPushButton::clicked, this, [this, st, sysKey]() { pickStar(st, sysKey); });
    head->addWidget(st);
    l->addLayout(head);
    // o que o Gerar sugeriu aqui: dá pra ficar com tudo ou dispensar tudo de uma vez
    QStringList sugg;
    for (const QString& w : children(sysKey)) if (kindOf(w) == LK::Planet && isGhost(w)) sugg << w;
    if (sugg.size() > 1) {
        auto* ban = new QFrame(v);
        ban->setObjectName(QStringLiteral("lvGenBan"));
        auto* bh = new QHBoxLayout(ban);
        bh->setContentsMargins(10, 5, 6, 5);
        bh->addWidget(lab(tr("%1 vizinhos sugeridos.").arg(sugg.size()), QStringLiteral("lvGenText"), ban), 1);
        auto* y = btn(tr("Ficar com todos"), QStringLiteral("lvGenBtn"), ban);
        auto* n = btn(tr("Dispensar"), QStringLiteral("lvGenBtn"), ban);
        connect(y, &QPushButton::clicked, this, [this, sugg]() { for (const QString& w : sugg) m_doc->ghosts.remove(w); changed(); QTimer::singleShot(0, this, &Livre::render); });
        connect(n, &QPushButton::clicked, this, [this, sugg]() {
            QTimer::singleShot(0, this, [this, sugg]() {
                // de trás pra frente: cada um que sai reordena os de fora
                for (int i = int(sugg.size()) - 1; i >= 0; i--) if (valid(sugg[i])) m_canvas->removeThing(sugg[i]);
                for (const QString& w : sugg) m_doc->ghosts.remove(w);
                changed();
                toast(tr("Vizinhos dispensados."), false);
                render();
            });
        });
        bh->addWidget(y);
        bh->addWidget(n);
        l->addWidget(ban);
    }
    for (const QString& w : children(sysKey)) {
        if (kindOf(w) != LK::Planet) continue;
        l->addWidget(orbitRow(w, false, v));
        for (const QString& m : children(w)) if (kindOf(m) == LK::Moon) l->addWidget(orbitRow(m, true, v));
    }
    if (m_sysAdd) {
        auto* in = new QLineEdit(v);
        in->setObjectName(QStringLiteral("lvInline"));
        in->setPlaceholderText(tr("Nome, Enter cria"));
        in->setProperty("lvEscape", QStringLiteral("sys"));
        in->installEventFilter(this);
        connect(in, &QLineEdit::returnPressed, this, [this, in, sysKey]() {
            const QString n = in->text().trimmed();
            if (n.isEmpty()) return;
            m_sysAdd = false;
            m_sel = createWorld(n, sysKey, Type::Temperado, false);
            render();
            goBody();
        });
        l->addWidget(in);
        QTimer::singleShot(0, in, [in]() { in->setFocus(); });
    } else {
        auto* add = btn(QStringLiteral("  +   ") + tr("Novo planeta"), QStringLiteral("lvOrbit"), v);
        add->setStyleSheet(QStringLiteral("QPushButton#lvOrbit { color: %1; font-size: 12px; padding: 8px 4px; }").arg(Theme::textMuted()));
        connect(add, &QPushButton::clicked, this, [this]() { m_sysAdd = true; QTimer::singleShot(0, this, &Livre::renderRight); });
        l->addWidget(add);
    }
    return v;
}
QWidget* Livre::aloneView(const QString& world, QWidget* parent)
{
    auto* v = new SunBox(nullptr, parent);
    v->setObjectName(QStringLiteral("lvSysv"));
    auto* l = new QVBoxLayout(v);
    l->setContentsMargins(10, 12, 10, 10);
    l->setSpacing(4);
    l->addWidget(lab(tr("Sem sistema"), QStringLiteral("lvSysPlain"), v));
    const QString terr = ancestor(world, LK::Terr);
    l->addWidget(lab(terr.isEmpty() ? tr("Mundo solto") : tr("Mundo solto em %1").arg(label(terr)), QStringLiteral("lvSmall"), v));
    const QString w = kindOf(world) == LK::Moon ? parentKey(world) : world;
    l->addWidget(orbitRow(w, false, v));
    for (const QString& m : children(w)) if (kindOf(m) == LK::Moon) l->addWidget(orbitRow(m, true, v));
    return v;
}
QWidget* Livre::listRow(const QString& k, QWidget* parent)
{
    auto* r = btn(QString(), QStringLiteral("lvRow"), parent);
    r->setProperty("on", m_sel == k);
    auto* h = new QHBoxLayout(r);
    h->setContentsMargins(8, 5, 8, 5);
    h->setSpacing(8);
    h->addWidget(markOf(k, isWorld(k) ? 20 : 15, r));
    auto* nm = lab(label(k), QStringLiteral("lvRowName"), r);
    nm->setAttribute(Qt::WA_TransparentForMouseEvents);
    if (isGhost(k)) nm->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::accentWarning()));
    h->addWidget(nm, 1);
    if (isCapital(k)) h->addWidget(starMark(r));
    if (isGhost(k)) h->addWidget(ghostActs(k, r));
    else {
        const QStringList ws = kindOf(k) == LK::Terr || kindOf(k) == LK::Sys ? worldsIn(k) : QStringList();
        for (int i = 0; i < std::min<int>(8, ws.size()); i++) { auto* d = new QLabel(r); d->setFixedSize(13, 13); d->setPixmap(ball(ws[i], 13)); d->setAttribute(Qt::WA_TransparentForMouseEvents); h->addWidget(d); }
        connect(r, &QPushButton::clicked, this, [this, k]() { select(k); });
        peekable(r, k);
    }
    r->setMinimumHeight(34);
    return r;
}
QWidget* Livre::listView(const QStringList& keys, const QString& empty, QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(2);
    for (const QString& k : keys) l->addWidget(listRow(k, w));
    if (keys.isEmpty()) l->addWidget(lab(empty, QStringLiteral("lvEmpty"), w));
    return w;
}

// ── a janela 3D ──
QWidget* Livre::win3d(const QString& k, bool big, bool titled, QWidget* parent)
{
    auto* w = new WinBox(parent);
    w->setObjectName(QStringLiteral("lvWin"));
    // maior; e maior ainda quando tem lua ou construção em volta, pra elas aparecerem
    const OrbitPlan op = isWorld(k) ? orbitPlan(k) : OrbitPlan();
    const bool around = !op.list.empty() || !op.wraps.empty();
    w->setFixedHeight(big ? (around ? 440 : 380) : (around ? 340 : 272));
    if (m_liveKey != k) { m_liveKey = k; m_liveZoom = 1; m_liveSpin = 0.6; m_livePitch = isWorld(k) ? 0.28 : 0.78; m_liveRot = 0.5; m_liveOrbT = 0; }
    auto* live = new LiveWorld(this, k, w);
    w->live = live;
    m_live = live;
    if (titled) {
        auto* t = new QWidget(w);
        t->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* tv = new QVBoxLayout(t);
        tv->setContentsMargins(0, 0, 0, 0);
        tv->setSpacing(0);
        tv->addWidget(lab(tr("FICHA TÉCNICA"), QStringLiteral("lvWinSmall"), t));
        tv->addWidget(lab(label(k), QStringLiteral("lvWinName"), t));
        w->title = t;
    }
    auto* acts = new QWidget(w);
    auto* ah = new QHBoxLayout(acts);
    ah->setContentsMargins(0, 0, 0, 0);
    ah->setSpacing(4);
    const auto act = [&](const QString& text) { auto* b = btn(text, QStringLiteral("lvWinBtn"), acts); ah->addWidget(b); return b; };
    connect(act(tr("Outra variação")), &QPushButton::clicked, this, [this, k]() { reseed(k); render(); });
    if (kindOf(k) == LK::Planet) {
        connect(act(tr("+ Lua")), &QPushButton::clicked, this, [this, w, k]() { askOrbiter(w, k, QStringLiteral("lua")); });
        auto* cb = act(tr("+ Construção"));
        connect(cb, &QPushButton::clicked, this, [this, cb, k]() { pickStruct(cb, k, QString()); });
    }
    if (kindOf(k) == LK::Build) { auto* tb = act(tr("Trocar modelo")); connect(tb, &QPushButton::clicked, this, [this, tb, k]() { pickStruct(tb, parentKey(k), k); }); }
    w->acts = acts;
    auto* z = new QWidget(w);
    auto* zh = new QHBoxLayout(z);
    zh->setContentsMargins(0, 0, 0, 0);
    zh->setSpacing(2);
    auto* zm = btn(QStringLiteral("−"), QStringLiteral("lvWinBtn"), z);
    zm->setToolTip(tr("Afastar"));
    auto* zv = lab(QStringLiteral("%1%").arg(qRound(m_liveZoom * 100)), QStringLiteral("lvWinZoom"), z);
    zv->setAlignment(Qt::AlignCenter);
    auto* zp = btn(QStringLiteral("+"), QStringLiteral("lvWinBtn"), z);
    zp->setToolTip(tr("Aproximar"));
    // ver em tela grande
    auto* ex = btn(QString(), QStringLiteral("lvWinBtn"), z);
    ex->setObjectName(QStringLiteral("lvWinExpand"));
    ex->setIcon(carto::icon(QStringLiteral("expand"), QColor(230, 236, 245), col(Theme::accentDefault()), 13));
    ex->setFixedSize(28, 24);
    ex->setToolTip(tr("Ver em tela grande"));
    connect(ex, &QPushButton::clicked, this, [this, k]() { QTimer::singleShot(0, this, [this, k]() { openBigView(k); }); });
    zh->addWidget(ex);
    zh->addWidget(zm); zh->addWidget(zv); zh->addWidget(zp);
    QPointer<LiveWorld> lp(live);
    connect(zm, &QPushButton::clicked, this, [lp]() { if (lp) lp->setZoom(lp->zoom() / 1.25); });
    connect(zp, &QPushButton::clicked, this, [lp]() { if (lp) lp->setZoom(lp->zoom() * 1.25); });
    live->zoomChanged = [zv](double v) { zv->setText(QStringLiteral("%1%").arg(qRound(v * 100))); };
    w->zoom = z;
    live->setToolTip(tr("Arraste para girar. Role para aproximar; dois cliques voltam ao normal. Clique numa lua ou construção para abrir."));
    return w;
}
// + Lua dentro da janela: nome e Enter, ela nasce orbitando e você continua no planeta.
void Livre::askOrbiter(QWidget* win, const QString& world, const QString& kind)
{
    auto* w = static_cast<WinBox*>(win);
    if (w->ask) { w->ask->deleteLater(); w->ask = nullptr; }
    auto* in = new QLineEdit(w);
    in->setObjectName(QStringLiteral("lvInline"));
    in->setPlaceholderText(tr("Nome da lua, Enter cria"));
    w->ask = in;
    in->show();
    w->resize(w->size() + QSize(0, 1)); w->resize(w->size() - QSize(0, 1));
    in->setFocus();
    connect(in, &QLineEdit::returnPressed, this, [this, in, world, kind]() {
        const QString n = in->text().trimmed();
        if (n.isEmpty()) return;
        const Type t = MOON_TYPES[QRandomGenerator::global()->bounded(int(MOON_TYPES.size()))];
        Q_UNUSED(kind);
        createMoon(n, world, t, false);
        QTimer::singleShot(0, this, [this]() { render(); });
    });
    in->setProperty("lvEscape", QStringLiteral("orb"));
    connect(in, &QLineEdit::editingFinished, in, [in]() { if (!in->hasFocus()) in->deleteLater(); });
}

// ── a ficha ──
QWidget* Livre::propRows(const QString& k, bool full, QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    const auto row = [&](const QString& key, const QString& value, const QString& empty, const std::function<void(QWidget*)>& fn) {
        auto* r = btn(QString(), QStringLiteral("lvProp"), w);
        auto* h = new QHBoxLayout(r);
        h->setContentsMargins(2, 0, 2, 0);
        auto* kl = lab(key, QStringLiteral("lvPropK"), r);
        kl->setAttribute(Qt::WA_TransparentForMouseEvents);
        h->addWidget(kl);
        h->addStretch();
        auto* vl = lab(value.isEmpty() ? empty : value, QStringLiteral("lvPropV"), r);
        vl->setProperty("unset", value.isEmpty());
        vl->setAttribute(Qt::WA_TransparentForMouseEvents);
        h->addWidget(vl);
        r->setMinimumHeight(34);
        connect(r, &QPushButton::clicked, this, [r, fn]() { fn(r); });
        l->addWidget(r);
    };
    const LK t = kindOf(k);
    if (t == LK::Planet || t == LK::Moon) {
        row(tr("Tipo"), typeLabelOf(k), tr("escolher"), [this, k](QWidget* a) { pickType(a, k); });
        bool au = false;
        const int c = habOf(k, &au);
        const QString cv = c ? (full ? habLabel(c) : QString::fromLatin1(habRoman(c))) + (au ? tr(" (auto)") : QString()) : QString();
        row(tr("Classe"), cv, tr("escolher"), [this, k](QWidget* a) { pickClass(a, k); });
    }
    if (t == LK::Sys) {
        const System* s = sysOf(k);
        row(tr("Estrela"), s ? coreT(starKindLabel(s->stars[0].kind, s->stars[0].pulsar)) : QString(), tr("escolher"), [this, k](QWidget* a) { pickStar(a, k); });
    }
    if (t == LK::Place) {
        const Group* g = m_doc->resolve(k).group;
        row(tr("O que é"), g ? g->form : QString(), tr("escolher"), [this, k](QWidget* a) {
            Group* g = m_doc->resolve(k).group;
            if (!g) return;
            pickList(a, tr("O QUE É"), { tr("Cidade"), tr("Região"), tr("Palácio"), tr("Ruína"), tr("Base") }, g->form, tr("Sem definir"), [this, k](const QString& v) { if (Group* x = m_doc->resolve(k).group) { x->form = v; changed(false); render(); } });
        });
    }
    if (t == LK::Build) row(tr("O que é"), typeLabelOf(k), tr("escolher"), [this, k](QWidget* a) { pickStruct(a, parentKey(k), k); });
    if (t == LK::Planet || t == LK::Sys) {
        const QString p = parentKey(k);
        row(tr("Fica em"), p.isEmpty() ? QString() : label(p), tr("solto"), [this, k](QWidget* a) { pickWhere(a, k); });
    }
    if (t == LK::Terr) {
        const QString cap = capitalOf(k);
        row(tr("Capital"), cap.isEmpty() ? QString() : label(cap), tr("escolher"), [this, k](QWidget* a) {
            const QStringList ws = worldsIn(k);
            openPop(a, [&](QVBoxLayout* l, QFrame* p) {
                popTitle(l, p, tr("Capital"));
                if (ws.isEmpty()) l->addWidget(lab(tr("Ainda não tem nenhum mundo aqui dentro."), QStringLiteral("lvEmpty"), p, true));
                for (const QString& x : ws) popOpt(l, p, label(x), isCapital(x), QString(), [this, x]() { toggleCapital(x); }, ball(x, 16));
            });
        });
    }
    if (canBeCapital(k)) row(tr("Capital"), isCapital(k) ? tr("sim") : QString(), tr("não"), [this, k](QWidget*) { toggleCapital(k); });
    return w;
}
QWidget* Livre::fichaEl(const QString& k, QWidget* parent)
{
    auto* f = new QFrame(parent);
    f->setObjectName(QStringLiteral("lvFicha"));
    auto* l = new QVBoxLayout(f);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(8);
    const LK t = kindOf(k);
    const bool has3d = t == LK::Planet || t == LK::Moon || t == LK::Build;
    if (has3d) l->addWidget(win3d(k, false, true, f));
    else {
        auto* head = new QHBoxLayout();
        head->setSpacing(10);
        if (t == LK::Terr) {
            auto* d = btn(QString(), QString(), f);
            d->setFixedSize(46, 46);
            const Region* r = m_doc->resolve(k).region;
            d->setStyleSheet(QStringLiteral("QPushButton { background: %1; border-radius: 12px; border: none; }").arg(r ? r->color : QStringLiteral("#8e9db4")));
            d->setToolTip(tr("Trocar a cor"));
            connect(d, &QPushButton::clicked, this, [this, d, k]() { pickColor(d, k); });
            head->addWidget(d);
        } else head->addWidget(markOf(k, 22, f));
        auto* tv = new QVBoxLayout();
        tv->setSpacing(0);
        tv->addWidget(lab(tr("FICHA TÉCNICA"), QStringLiteral("lvFxEye"), f));
        tv->addWidget(lab(label(k), QStringLiteral("lvFxName"), f, true));
        head->addLayout(tv, 1);
        l->addLayout(head);
    }
    l->addWidget(propRows(k, false, f));
    if (t == LK::Planet || t == LK::Moon) {
        auto* o = btn(tr("Abrir ficha técnica completa"), QStringLiteral("lvFxOpen"), f);
        connect(o, &QPushButton::clicked, this, [this]() { m_fichaOpen = true; QTimer::singleShot(0, this, &Livre::renderRight); });
        l->addWidget(o);
    }
    return f;
}
// A ficha completa: os campos do Caderno, mas aqui quem escreve é o autor. O Gerar só preenche o que está vazio.
QWidget* Livre::looksEl(const QString& k, QWidget* parent)
{
    Body* b = bodyOf(k);
    System* s = sysOf(k);
    if (!b || !s) return nullptr;
    auto* w = new QWidget(parent);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);
    const auto touched = [this, b, s]() { m_canvas->invalidateBody(*b); b->texRev++; s->edited = true; changed(!m_doc->isSolo(*s)); };
    const auto line = [&](const QString& label, QWidget* el) {
        auto* h = new QHBoxLayout();
        h->setSpacing(10);
        auto* k2 = lab(label, QStringLiteral("lvFxKey"), w);
        k2->setFixedWidth(84);
        h->addWidget(k2);
        el->setParent(w);
        h->addWidget(el, 1);
        l->addLayout(h);
    };
    const auto slider = [&](int value, const std::function<QString(int)>& fmt, const std::function<void(int)>& apply) {
        auto* box = new QWidget();
        auto* h = new QHBoxLayout(box);
        h->setContentsMargins(0, 0, 0, 0);
        auto* s2 = new QSlider(Qt::Horizontal, box);
        s2->setRange(0, 100);
        s2->setValue(value);
        s2->setFocusPolicy(Qt::NoFocus);
        auto* o = lab(fmt(value), QStringLiteral("lvSmall"), box);
        o->setFixedWidth(70);
        connect(s2, &QSlider::valueChanged, o, [o, fmt](int v) { o->setText(fmt(v)); });
        connect(s2, &QSlider::sliderReleased, this, [this, s2, apply]() { apply(s2->value()); QTimer::singleShot(0, this, [this]() { renderLeft(); renderRight(); }); });
        h->addWidget(s2, 1);
        h->addWidget(o);
        return box;
    };
    const bool living = isLiving(b->type);
    if (living) {
        line(tr("Água"), slider(qRound(waterOf(*b) * 100), [](int v) { return QStringLiteral("%1%").arg(v); }, [b, touched](int v) { setWater(*b, v / 100.0); touched(); }));
        line(tr("Nuvens"), slider(qRound(clamp((0.36 - b->look.cloud) / 0.48, 0, 1) * 100), [](int v) { return v < 15 ? Livre::tr("Céu limpo") : v < 55 ? Livre::tr("Algumas") : v < 85 ? Livre::tr("Muitas") : Livre::tr("Fechado"); }, [b, touched](int v) { b->look.cloud = mix(0.36, -0.12, v / 100.0); touched(); }));
    }
    std::vector<LookStyle> styles;
    if (living) { for (LookStyle q : LIVING_STYLES) if (q != LookStyle::Olho || b->locked) styles.push_back(q); }
    else if (b->look.hasRock) styles = rockStylesOf(b->type);
    if (!styles.empty()) {
        auto* c = new QComboBox();
        c->setObjectName(QStringLiteral("lvCombo"));
        c->setFocusPolicy(Qt::NoFocus);
        for (LookStyle q : styles) c->addItem(coreT(lookStyleLabel(q)), int(q));
        c->setCurrentIndex(std::max(0, c->findData(int(b->look.style))));
        connect(c, &QComboBox::activated, this, [this, c, b, s, living, touched]() {
            const LookStyle q = LookStyle(c->currentData().toInt());
            if (living) restyle(*b, q); else rockRestyle(*s, *b, q);
            touched();
            QTimer::singleShot(0, this, [this]() { renderLeft(); renderRight(); });
        });
        line(tr("Paisagem"), c);
    }
    // População (mundo com chão): quanta gente; a luz do lado da noite segue a densidade, como no mapa
    const bool solid = b->body == BodyKind::Lua || !(b->type == Type::Gigante || b->type == Type::Quente || b->type == Type::GeloG || b->type == Type::Subnetuno);
    if (solid && !isArtificial(b->type)) {
        const double rE = b->body == BodyKind::Lua ? b->radiusKm / 6371 : (b->radius != 0 ? b->radius : 1);
        const double pmax = 1.6e12 * rE * rE;
        const auto toPeople = [pmax](int s2) { return s2 <= 0 ? 0.0 : std::pow(10, mix(3, std::log10(pmax), s2 / 100.0)); };
        const auto toS = [pmax](double n) { return n > 0 ? int(clamp(((std::log10(n) - 3) / (std::log10(pmax) - 3)) * 100, 1, 100) + 0.5) : 0; };
        const auto note = [rE](double n) {
            if (n <= 0) return Livre::tr("Ninguém mora aqui.");
            const double d = n / (8e9 * rE * rE);
            return d < 0.002 ? Livre::tr("Tão pouca gente pra esse tamanho que nada aparece visto do espaço.") : d < 0.05 ? Livre::tr("Cidades esparsas: pontinhos isolados no lado da noite.") : d < 0.4 ? Livre::tr("Menos gente por área que a Terra de hoje.") : d < 2.5 ? Livre::tr("Mais ou menos a densidade da Terra de hoje.") : d < 40 ? Livre::tr("Bem mais povoado que a Terra: as luzes viram redes contínuas.") : Livre::tr("Planeta-cidade: quase toda a terra firme é cidade.");
        };
        auto* noteL = lab(note(b->people), QStringLiteral("lvFxNote"), w, true);
        line(tr("População"), slider(toS(b->people), [toPeople](int v) { const double n = toPeople(v); return n > 0 ? fmtCount(n) : Livre::tr("Ninguém"); }, [b, touched, toPeople, rE](int v) { b->people = toPeople(v); b->look.pop = lightLevel(b->people, rE); touched(); }));
        l->addWidget(noteL);
    } else if (!isArtificial(b->type)) line(tr("Luzes"), slider(qRound(clamp(b->look.pop, 0, 1) * 100), [](int v) { return v ? QStringLiteral("%1%").arg(v) : Livre::tr("Apagado"); }, [b, touched](int v) { b->look.pop = v / 100.0; touched(); }));
    if (b->body == BodyKind::Planeta && !isArtificial(b->type)) {
        // o anel escolhido vendo: o planeta em oito estilos
        auto* rb = btn(QString(), QStringLiteral("lvRow"), w);
        rb->setIcon(QIcon(b->rings ? ball(k, 26, true) : QPixmap()));
        rb->setIconSize(QSize(26, 26));
        rb->setText(b->rings ? tr("Trocar o anel") : tr("Sem anéis · escolher"));
        rb->setStyleSheet(QStringLiteral("QPushButton#lvRow { font-size: 12px; padding: 3px 10px 3px 4px; text-align: left; border: 1px solid %1; border-radius: 14px; }").arg(Theme::panelBorder()));
        connect(rb, &QPushButton::clicked, this, [this, rb, k]() { pickRings(rb, k); });
        line(tr("Anéis"), rb);
    }
    // as cores que o motor usa nesse tipo de mundo
    std::vector<std::pair<QString, V3*>> colors;
    Look& L = b->look;
    if (living || L.hasFreq) { colors.push_back({ tr("Mar fundo"), &L.deep }); colors.push_back({ tr("Mar raso"), &L.shallow }); colors.push_back({ tr("Vegetação"), &L.veg }); colors.push_back({ tr("Terra seca"), &L.dry }); colors.push_back({ tr("Gelo"), &L.ice }); colors.push_back({ tr("Nuvens"), &L.cloudCol }); }
    if (L.hasRock) { colors.push_back({ tr("Superfície"), &L.tint }); colors.push_back({ tr("Base"), &L.lo }); colors.push_back({ tr("Relevo"), &L.hi }); }
    for (size_t i = 0; i < L.pal.size(); i++) colors.push_back({ tr("Faixa %1").arg(i + 1), &L.pal[i].c });
    if (!colors.empty()) {
        auto* grid = new QGridLayout();
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(6);
        int n = 0;
        for (const auto& [name, v] : colors) {
            V3* ptr = v;
            const QColor c = QColor::fromRgbF(float(clamp((*ptr)[0], 0, 1)), float(clamp((*ptr)[1], 0, 1)), float(clamp((*ptr)[2], 0, 1)));
            auto* sw = btn(name, QStringLiteral("lvRow"), w);
            sw->setIcon(QIcon(dot(c, 16)));
            sw->setIconSize(QSize(16, 16));
            sw->setStyleSheet(QStringLiteral("QPushButton#lvRow { color: %1; font-size: 11px; padding: 3px 6px; text-align: left; }").arg(Theme::textMuted()));
            connect(sw, &QPushButton::clicked, this, [this, ptr, c, touched]() {
                const QColor got = QColorDialog::getColor(c, this);
                if (!got.isValid()) return;
                *ptr = { got.redF(), got.greenF(), got.blueF() };
                touched();
                QTimer::singleShot(0, this, [this]() { renderLeft(); renderRight(); });
            });
            grid->addWidget(sw, n / 2, n % 2);
            n++;
        }
        l->addLayout(grid);
    }
    if (!l->count()) { delete w; return nullptr; }   // mundo construído: nada de aparência pra mexer
    return w;
}
QWidget* Livre::fichaFull(const QString& wk, QWidget* parent)
{
    auto* f = new QFrame(parent);
    f->setObjectName(QStringLiteral("lvFicha"));
    auto* l = new QVBoxLayout(f);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(8);
    if (!m_sel.isEmpty() && m_sel != wk) l->addWidget(lab(tr("%1 fica em %2").arg(label(m_sel), label(wk)), QStringLiteral("lvFxNote"), f, true));
    l->addWidget(win3d(wk, true, false, f));
    l->addWidget(lab(label(wk), QStringLiteral("lvFxName"), f, true));
    bool au = false;
    const int c = habOf(wk, &au);
    QString sub = typeLabelOf(wk);
    if (c) sub += QStringLiteral(" · ") + tr("Classe %1").arg(habLabel(c) + (au ? tr(" (auto)") : QString()));
    l->addWidget(lab(sub, QStringLiteral("lvSmall"), f));
    // o que o Gerar preencheu, esperando o sim
    const QHash<QString, QString> gen = m_doc->fichasGen.value(wk);
    if (!gen.isEmpty()) {
        auto* ban = new QFrame(f);
        ban->setObjectName(QStringLiteral("lvGenBan"));
        auto* bh = new QHBoxLayout(ban);
        bh->setContentsMargins(10, 6, 6, 6);
        bh->addWidget(lab(gen.size() == 1 ? tr("1 campo gerado.") : tr("%1 campos gerados.").arg(gen.size()), QStringLiteral("lvGenText"), ban), 1);
        auto* y = btn(tr("Ficar com tudo"), QStringLiteral("lvGenBtn"), ban);
        auto* n = btn(tr("Dispensar"), QStringLiteral("lvGenBtn"), ban);
        connect(y, &QPushButton::clicked, this, [this, wk]() {
            QHash<QString, QString>& mine = m_doc->fichas[wk];
            const QHash<QString, QString> g = m_doc->fichasGen.take(wk);
            for (auto it = g.constBegin(); it != g.constEnd(); ++it) if (mine.value(it.key()).trimmed().isEmpty()) mine.insert(it.key(), it.value());
            changed(false);
            QTimer::singleShot(0, this, &Livre::renderRight);
        });
        connect(n, &QPushButton::clicked, this, [this, wk]() { m_doc->fichasGen.remove(wk); changed(false); QTimer::singleShot(0, this, &Livre::renderRight); });
        bh->addWidget(y);
        bh->addWidget(n);
        l->addWidget(ban);
    }
    l->addWidget(propRows(wk, true, f));
    if (c) l->addWidget(lab(habDesc(c), QStringLiteral("lvFxNote"), f, true));
    if (QWidget* lk = looksEl(wk, f)) { l->addWidget(lab(tr("APARÊNCIA"), QStringLiteral("lvFxSec"), f)); l->addWidget(lk); }
    const bool moon = kindOf(wk) == LK::Moon;
    const QList<QPair<QString, QList<QPair<QString, QString>>>> secs = moon
        ? QList<QPair<QString, QList<QPair<QString, QString>>>> {
            { tr("Corpo"), { { "raio", tr("Raio") }, { "grav", tr("Gravidade") }, { "temp", tr("Temperatura média") }, { "atm", tr("Atmosfera") } } },
            { tr("Órbita"), { { "dist", tr("Distância do planeta") }, { "ano", tr("Uma volta") }, { "rot", tr("Rotação") } } } }
        : QList<QPair<QString, QList<QPair<QString, QString>>>> {
            { tr("Corpo"), { { "massa", tr("Massa") }, { "raio", tr("Raio") }, { "pop", tr("População") }, { "grav", tr("Gravidade") }, { "dens", tr("Densidade") }, { "temp", tr("Temperatura média") }, { "atm", tr("Atmosfera") }, { "veg", tr("Vegetação") } } },
            { tr("Órbita e rotação"), { { "dist", tr("Distância da estrela") }, { "exc", tr("Excentricidade") }, { "ano", tr("Ano") }, { "dia", tr("Dia") }, { "eixo", tr("Inclinação do eixo") } } } };
    const QHash<QString, QString> mine = m_doc->fichas.value(wk);
    for (const auto& sec : secs) {
        l->addWidget(lab(sec.first.toUpper(), QStringLiteral("lvFxSec"), f));
        for (const auto& fld : sec.second) {
            auto* h = new QHBoxLayout();
            h->setSpacing(10);
            auto* kl = lab(fld.second, QStringLiteral("lvFxKey"), f);
            kl->setFixedWidth(130);
            h->addWidget(kl);
            auto* in = new QLineEdit(f);
            in->setObjectName(QStringLiteral("lvFxIn"));
            const QString own = mine.value(fld.first), g = gen.value(fld.first);
            in->setText(own.isEmpty() ? g : own);
            in->setProperty("gen", own.isEmpty() && !g.isEmpty());
            in->setPlaceholderText(QStringLiteral("—"));
            const QString key = fld.first;
            connect(in, &QLineEdit::textEdited, this, [this, wk, key, in](const QString& v) {
                m_typing.start();
                m_doc->fichas[wk].insert(key, v);
                if (m_doc->fichasGen.contains(wk)) { m_doc->fichasGen[wk].remove(key); if (m_doc->fichasGen[wk].isEmpty()) m_doc->fichasGen.remove(wk); }
                if (in->property("gen").toBool()) { in->setProperty("gen", false); in->style()->unpolish(in); in->style()->polish(in); }
                changed(false);
            });
            h->addWidget(in, 1);
            l->addLayout(h);
        }
    }
    auto* gb = btn(tr("Gerar o que está vazio"), QStringLiteral("lvOrder"), f);
    connect(gb, &QPushButton::clicked, this, [this, wk]() { fillFicha(wk); QTimer::singleShot(0, this, &Livre::renderRight); });
    l->addWidget(gb);
    return f;
}
QWidget* Livre::ordersEl(const QString& k, QWidget* parent)
{
    // o montador do sistema toma o lugar dos pedidos enquanto está aberto pra este lugar
    if (m_cmp.on && m_cmp.forKey == (k.isEmpty() ? QStringLiteral("root") : k)) return composerEl(parent);
    if (m_cmp.on) m_cmp = Composer();
    auto* w = new QWidget(parent);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 4, 0, 0);
    l->setSpacing(5);
    l->addWidget(lab(tr("GERAR"), QStringLiteral("lvSideTitle"), w));
    for (auto& [text, fn] : orders(k)) {
        auto* b = btn(text, QStringLiteral("lvOrder"), w);
        auto run = fn;
        connect(b, &QPushButton::clicked, this, [this, run]() { QTimer::singleShot(0, this, [this, run]() { run(); changed(); render(); }); });
        l->addWidget(b);
    }
    return w;
}
QWidget* Livre::linkSide(const QString& k, QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);
    l->addWidget(lab(tr("CONEXÃO"), QStringLiteral("lvSideTitle"), w));
    if (const auto* lk = m_panel->m_link.places ? m_panel->m_link.places->link(k.mid(1)) : nullptr)
        for (const QString& pid : { lk->fromTerritorioId, lk->toTerritorioId }) { const QString ek = m_panel->keyOfPlace(pid); if (!ek.isEmpty()) l->addWidget(listRow(ek, w)); }
    l->addWidget(lab(tr("Mora no Criador de Mundos, como os vínculos entre territórios."), QStringLiteral("lvFxNote"), w, true));
    return w;
}

// ── a direita inteira ──
void Livre::renderRight()
{
    const int keep = m_right->verticalScrollBar()->value();
    QString host;
    if (m_fichaOpen) for (QString c = m_sel; !c.isEmpty(); c = parentKey(c)) if (isWorld(c)) { host = c; break; }
    m_rightCol->setFixedWidth(host.isEmpty() ? m_rightW : std::max(480, m_rightW));
    // abrir a ficha completa começa do topo dela
    const bool opening = !host.isEmpty() && !m_rightHead->isVisible();
    m_rightHead->setVisible(!host.isEmpty());
    auto* body = new QWidget();
    body->setObjectName(QStringLiteral("lvPaneBody"));
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(20, 16, 20, 20);
    v->setSpacing(16);
    v->setSpacing(12);
    v->addWidget(crumbsEl(body));
    const QString cur = m_sel;
    const LK t = kindOf(cur);
    const auto foot = [&]() {
        if (cur.isEmpty()) return;
        auto* h = new QHBoxLayout();
        h->addStretch();
        auto* x = btn(tr("Excluir"), QStringLiteral("lvQuiet"), body);
        connect(x, &QPushButton::clicked, this, [this, cur]() { QTimer::singleShot(0, this, [this, cur]() { flushSheet(); removeItem(cur); }); });
        h->addWidget(x);
        v->addLayout(h);
    };
    if (t == LK::Link) { v->addWidget(linkSide(cur, body)); foot(); v->addStretch(); m_right->setWidget(body); return; }
    if (!host.isEmpty()) { v->addWidget(fichaFull(host, body)); foot(); v->addStretch(); m_right->setWidget(body); m_right->verticalScrollBar()->setValue(opening ? 0 : keep); return; }
    const QString sysCtx = t == LK::Sys ? cur : ancestor(cur, LK::Sys);
    QString world;
    for (QString c = cur; !c.isEmpty(); c = parentKey(c)) if (kindOf(c) == LK::Planet) { world = c; break; }
    if (world.isEmpty() && isWorld(cur)) world = cur;
    const QString terr = t == LK::Terr ? cur : ancestor(cur, LK::Terr);
    if (!sysCtx.isEmpty()) v->addWidget(systemView(sysCtx, body));
    else if (!world.isEmpty()) v->addWidget(aloneView(world, body));
    else if (!terr.isEmpty()) {
        QStringList keys;
        for (const QString& c : children(terr)) if (kindOf(c) == LK::Sys || kindOf(c) == LK::Planet) keys << c;
        v->addWidget(listView(keys, tr("Nada aqui dentro ainda."), body));
    } else {
        QStringList keys;
        for (const QString& c : children(QString())) keys << c;
        v->addWidget(listView(keys, tr("A galáxia está vazia."), body));
    }
    if (!cur.isEmpty() && t != LK::Doc) v->addWidget(fichaEl(cur, body));
    if (t == LK::Terr) v->addWidget(terrMapEl(cur, body));
    if (cur.isEmpty() || (t != LK::Doc && t != LK::Place && t != LK::Build)) v->addWidget(ordersEl(cur, body));
    foot();
    v->addStretch();
    m_right->setWidget(body);
    m_right->verticalScrollBar()->setValue(keep);
}

// ── os anéis, escolhidos vendo ──
void Livre::pickRings(QWidget* anchor, const QString& k)
{
    Body* b = bodyOf(k);
    System* s = sysOf(k);
    if (!b || !s) return;
    // as prévias: o mesmo planeta com cada estilo, e o anel dele volta no fim
    std::unique_ptr<Rings> mine = b->rings ? std::make_unique<Rings>(*b->rings) : nullptr;
    std::vector<QPixmap> pics;
    std::vector<uint32_t> seeds;
    for (int i = 0; i < 8; i++) {
        const uint32_t seed = uint32_t(b->look.seed) * 7u + uint32_t(m_ringPage * 8 + i) * 977u + 13u;
        seeds.push_back(seed);
        b->rings = styledRings(i, seed);
        m_canvas->invalidateBody(*b);
        pics.push_back(ball(k, 52, true));
    }
    b->rings = mine ? std::make_unique<Rings>(*mine) : nullptr;
    m_canvas->invalidateBody(*b);
    openPop(anchor, [&](QVBoxLayout* l, QFrame* p) {
        popTitle(l, p, tr("Anéis de %1").arg(label(k)));
        auto* grid = new QGridLayout();
        grid->setSpacing(2);
        for (int i = 0; i < 8; i++) {
            auto* o = btn(QString(), QStringLiteral("lvTy"), p);
            o->setFixedSize(96, 92);
            auto* v = new QVBoxLayout(o);
            v->setContentsMargins(2, 4, 2, 2);
            v->setSpacing(2);
            auto* ic = new QLabel(o);
            ic->setPixmap(pics[size_t(i)]);
            ic->setAlignment(Qt::AlignHCenter);
            ic->setAttribute(Qt::WA_TransparentForMouseEvents);
            auto* tl = lab(tr(RING_STYLES[i].name), QStringLiteral("lvSmall"), o);
            tl->setAlignment(Qt::AlignHCenter);
            tl->setAttribute(Qt::WA_TransparentForMouseEvents);
            v->addWidget(ic);
            v->addWidget(tl);
            const uint32_t seed = seeds[size_t(i)];
            connect(o, &QPushButton::clicked, this, [this, k, i, seed]() {
                closePop();
                Body* bb = bodyOf(k);
                System* ss = sysOf(k);
                if (!bb || !ss) return;
                auto was = std::make_shared<std::unique_ptr<Rings>>(bb->rings ? std::make_unique<Rings>(*bb->rings) : nullptr);
                bb->rings = styledRings(i, seed);
                auto now = std::make_shared<Rings>(*bb->rings);
                m_canvas->invalidateBody(*bb);
                ss->edited = true;
                Canvas* cv = m_canvas;
                undoable(tr("Anel %1.").arg(tr(RING_STYLES[i].name).toLower()),
                    [bb, was, cv] { bb->rings = *was ? std::make_unique<Rings>(**was) : nullptr; cv->invalidateBody(*bb); },
                    [bb, now, cv] { bb->rings = std::make_unique<Rings>(*now); cv->invalidateBody(*bb); });
                changed(!m_doc->isSolo(*ss));
                QTimer::singleShot(0, this, &Livre::render);
            });
            grid->addWidget(o, i / 3, i % 3);
        }
        l->addLayout(grid);
        auto* acts = new QHBoxLayout();
        auto* none = btn(tr("Sem anéis"), QStringLiteral("lvPill"), p);
        connect(none, &QPushButton::clicked, this, [this, k]() {
            closePop();
            Body* bb = bodyOf(k);
            if (!bb || !bb->rings) return;
            auto was = std::make_shared<Rings>(*bb->rings);
            bb->rings.reset();
            m_canvas->invalidateBody(*bb);
            Canvas* cv = m_canvas;
            undoable(tr("Sem anéis."), [bb, was, cv] { bb->rings = std::make_unique<Rings>(*was); cv->invalidateBody(*bb); }, [bb, cv] { bb->rings.reset(); cv->invalidateBody(*bb); });
            changed();
            QTimer::singleShot(0, this, &Livre::render);
        });
        auto* more = btn(tr("Outras versões"), QStringLiteral("lvPill"), p);
        connect(more, &QPushButton::clicked, this, [this, anchor, k]() { m_ringPage++; QPointer<QWidget> a(anchor); QTimer::singleShot(0, this, [this, a, k]() { if (a) pickRings(a, k); }); });
        acts->addWidget(none);
        acts->addWidget(more);
        acts->addStretch();
        l->addLayout(acts);
    }, 320);
}
// ── o planeta em tela grande ──
void Livre::openBigView(const QString& k)
{
    closeBigView();
    closePop();
    hidePeek();
    auto* back = new QFrame(this);
    back->setObjectName(QStringLiteral("lvBigBack"));
    back->setStyleSheet(QStringLiteral("QFrame#lvBigBack { background: rgba(4,6,11,185); } QFrame#lvBigBox { background: #05070c; border: 1px solid rgba(236,241,250,30); border-radius: 14px; } QLabel#lvBigName { color: #eef2f8; font-family: 'Source Serif 4', Georgia, serif; font-size: 20px; font-weight: 600; background: transparent; } QLabel#lvBigSub { color: rgba(236,241,250,150); font-size: 12px; background: transparent; }"));
    back->setGeometry(rect());
    auto* outer = new QVBoxLayout(back);
    outer->setContentsMargins(24, 24, 24, 24);
    auto* box = new QFrame(back);
    box->setObjectName(QStringLiteral("lvBigBox"));
    box->setMaximumWidth(1100);
    auto* bv = new QVBoxLayout(box);
    bv->setContentsMargins(0, 0, 0, 0);
    bv->setSpacing(0);
    auto* head = new QHBoxLayout();
    head->setContentsMargins(18, 12, 12, 10);
    head->setSpacing(10);
    head->addWidget(lab(label(k), QStringLiteral("lvBigName"), box));
    head->addWidget(lab(whatIs(k), QStringLiteral("lvBigSub"), box));
    head->addStretch();
    auto* x = new QToolButton(box);
    x->setIcon(carto::icon(QStringLiteral("close"), QColor(230, 236, 245), col(Theme::accentDefault()), 14));
    x->setToolTip(tr("Fechar (Esc)"));
    x->setAutoRaise(true);
    x->setCursor(Qt::PointingHandCursor);
    connect(x, &QToolButton::clicked, this, [this]() { QTimer::singleShot(0, this, &Livre::closeBigView); });
    head->addWidget(x);
    bv->addLayout(head);
    QWidget* win = win3d(k, true, false, box);
    win->setFixedHeight(std::max(320, std::min(height() - 140, 760)));
    if (QWidget* e = win->findChild<QWidget*>(QStringLiteral("lvWinExpand"))) e->hide();
    bv->addWidget(win, 1);
    auto* row = new QHBoxLayout();
    row->addStretch();
    row->addWidget(box, 100);
    row->addStretch();
    outer->addStretch();
    outer->addLayout(row);
    outer->addStretch();
    back->show();
    back->raise();
    m_bigView = back;
}
void Livre::closeBigView()
{
    if (!m_bigView) return;
    m_bigView->hide();
    m_bigView->deleteLater();
    m_bigView = nullptr;
    renderRight();
}

// ── seletores ──
void Livre::pickType(QWidget* anchor, const QString& k)
{
    const Body* b = bodyOf(k);
    if (!b) return;
    const bool moon = b->body == BodyKind::Lua;
    const Type cur = b->type;
    openPop(anchor, [&](QVBoxLayout* l, QFrame* p) {
        popTitle(l, p, tr("Tipo"));
        auto* grid = new QGridLayout();
        grid->setSpacing(2);
        int n = 0;
        for (Type t : moon ? MOON_TYPES : PLANET_TYPES) {
            auto* o = btn(QString(), QStringLiteral("lvTy"), p);
            o->setProperty("on", t == cur);
            o->setFixedSize(76, 76);
            auto* v = new QVBoxLayout(o);
            v->setContentsMargins(2, 4, 2, 2);
            v->setSpacing(2);
            auto* ic = new QLabel(o);
            ic->setPixmap(typeThumb(t, 40));
            ic->setAlignment(Qt::AlignHCenter);
            ic->setAttribute(Qt::WA_TransparentForMouseEvents);
            auto* tl = lab(coreT(typeLabel(t)), QStringLiteral("lvSmall"), o, true);
            tl->setAlignment(Qt::AlignHCenter);
            tl->setAttribute(Qt::WA_TransparentForMouseEvents);
            v->addWidget(ic);
            v->addWidget(tl);
            connect(o, &QPushButton::clicked, this, [this, k, t]() { closePop(); QTimer::singleShot(0, this, [this, k, t]() { setWorldType(k, t); render(); }); });
            grid->addWidget(o, n / 4, n % 4);
            n++;
        }
        l->addLayout(grid);
    }, 336);
}
void Livre::pickClass(QWidget* anchor, const QString& k)
{
    Body* b = bodyOf(k);
    if (!b) return;
    openPop(anchor, [&](QVBoxLayout* l, QFrame* p) {
        popTitle(l, p, tr("Classe"));
        for (int n = 1; n <= 6; n++) popOpt(l, p, habLabel(n), b->habSet == n, QString(), [this, b, k, n]() { b->habSet = n; if (System* s = sysOf(k)) s->edited = true; changed(); render(); }, dot(habColor(n), 12));
        if (b->habSet) {
            auto* sep = new QFrame(p); sep->setObjectName(QStringLiteral("lvPopSep")); l->addWidget(sep);
            popOpt(l, p, tr("Automática"), false, QString(), [this, b]() { b->habSet = 0; changed(); render(); });
        }
    });
}
void Livre::pickStar(QWidget* anchor, const QString& sysKey)
{
    const System* s = sysOf(sysKey);
    if (!s) return;
    const StarKind cur = s->stars[0].kind;
    openPop(anchor, [&](QVBoxLayout* l, QFrame* p) {
        popTitle(l, p, tr("Estrela"));
        for (StarKind k : STAR_KINDS) {
            popOpt(l, p, coreT(starKindLabel(k)), k == cur, QString(), [this, sysKey, k]() { setStar(sysKey, k); render(); }, dot(starShown(k), 12));
        }
    });
}
void Livre::pickWhere(QWidget* anchor, const QString& k)
{
    const LK t = kindOf(k);
    QStringList opts;
    for (const auto& r : m_doc->regions) if (!isGhost(r->key)) opts << r->key;
    if (t == LK::Planet) for (System* s : livreSystems()) { const QString sk = keyS(*s); if (!isGhost(sk)) opts << sk; }
    openPop(anchor, [&](QVBoxLayout* l, QFrame* p) {
        popTitle(l, p, tr("Fica onde?"));
        popOpt(l, p, tr("Solto, em lugar nenhum"), parentKey(k).isEmpty(), QString(), [this, k]() { moveTo(k, QString()); render(); });
        if (!opts.isEmpty()) { auto* sep = new QFrame(p); sep->setObjectName(QStringLiteral("lvPopSep")); l->addWidget(sep); }
        for (const QString& o : opts) popOpt(l, p, label(o), parentKey(k) == o, kindName(kindOf(o)), [this, k, o]() { moveTo(k, o); render(); }, ball(o, 14));
        if (t == LK::Planet) {
            auto* sep = new QFrame(p); sep->setObjectName(QStringLiteral("lvPopSep")); l->addWidget(sep);
            popOpt(l, p, tr("Criar um sistema com este nome"), false, QString(), [this, k]() {
                const QString terr = ancestor(k, LK::Terr);
                const QString s = createSystem(tr("Sistema de %1").arg(label(k)), terr, StarKind::G, false);
                moveTo(k, s);
                render();
            });
        }
    });
}
// Catálogo primeiro: escolhe o modelo vendo, e ela nasce com nome pronto onde o clique aconteceu.
void Livre::pickStruct(QWidget* anchor, const QString& parent, const QString& replace)
{
    const bool planet = kindOf(parent) == LK::Planet;
    const std::string cur = replace.isEmpty() ? std::string() : buildType(replace);
    openPop(anchor, [&](QVBoxLayout* l, QFrame* p) {
        if (replace.isEmpty()) l->addWidget(lab((planet ? tr("Que construção vai orbitar %1?") : tr("Que construção vai ficar em %1?")).arg(label(parent)), QStringLiteral("lvPopHead"), p, true));
        for (const CatalogCat& c : catalog()) {
            if (c.id != "estacoes" && c.id != "mega") continue;
            std::vector<const CatalogItem*> items;
            for (const CatalogItem& it : c.items) {
                const StructDef* d = structOf(it.id);
                if (!d) continue;
                if (c.id == "mega" && (planet ? d->host != Host::Planet : d->host == Host::Planet)) continue;
                items.push_back(&it);
            }
            if (items.empty()) continue;
            popTitle(l, p, catT(c.name));
            auto* grid = new QGridLayout();
            grid->setSpacing(2);
            int n = 0;
            for (const CatalogItem* it : items) {
                const std::string id = it->id;
                auto* o = btn(QString(), QStringLiteral("lvTy"), p);
                o->setProperty("on", id == cur);
                o->setFixedSize(80, 82);
                o->setToolTip(catT(structOf(id)->desc));
                auto* v = new QVBoxLayout(o);
                v->setContentsMargins(2, 4, 2, 2);
                v->setSpacing(2);
                auto* ic = new QLabel(o);
                ic->setPixmap(modelPix(id, 91, 46));
                ic->setAlignment(Qt::AlignHCenter);
                ic->setAttribute(Qt::WA_TransparentForMouseEvents);
                auto* tl = lab(catT(it->name), QStringLiteral("lvSmall"), o, true);
                tl->setAlignment(Qt::AlignHCenter);
                tl->setAttribute(Qt::WA_TransparentForMouseEvents);
                v->addWidget(ic);
                v->addWidget(tl);
                connect(o, &QPushButton::clicked, this, [this, id, parent, replace]() {
                    closePop();
                    QTimer::singleShot(0, this, [this, id, parent, replace]() {
                        if (replace.isEmpty()) createBuild(id, parent, false);
                        else setBuildType(replace, id);
                        render();
                    });
                });
                grid->addWidget(o, n / 4, n % 4);
                n++;
            }
            l->addLayout(grid);
        }
    }, 352);
}
void Livre::pickConnect(QWidget* anchor, const QString& root)
{
    QSet<QString> taken;
    for (const Panel::GLink& x : m_panel->linksOf(root)) taken.insert(x.b);
    QStringList cands;
    for (const auto& r : m_doc->regions) if (r->key != root && !isGhost(r->key) && !taken.contains(r->key)) cands << r->key;
    for (System* s : livreSystems()) { const QString k = keyS(*s); if (k != root && !isGhost(k) && !taken.contains(k)) cands << k; }
    for (const QString& w : allWorlds()) if (w != root && !taken.contains(w)) cands << w;
    openPop(anchor, [&](QVBoxLayout* l, QFrame* p) {
        popTitle(l, p, tr("Conectar %1 a").arg(label(root)));
        auto* q = new QLineEdit(p);
        q->setObjectName(QStringLiteral("lvPopSearch"));
        q->setPlaceholderText(tr("Buscar planeta, sistema ou território"));
        l->addWidget(q);
        auto* box = new QWidget(p);
        auto* bl = new QVBoxLayout(box);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(1);
        l->addWidget(box);
        const auto fill = [this, bl, box, cands, root](const QString& text) {
            while (QLayoutItem* it = bl->takeAt(0)) { if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); } delete it; }
            int shown = 0;
            for (const QString& k : cands) {
                if (!text.trimmed().isEmpty() && !label(k).contains(text.trimmed(), Qt::CaseInsensitive)) continue;
                popOpt(bl, box, label(k), false, kindName(kindOf(k)), [this, root, k]() {
                    const QString x = createLink(root, k);
                    if (x.isEmpty()) return;
                    m_sel = x;
                    render();
                    goBody();
                }, ball(k, 16));
                if (++shown >= 40) break;
            }
            if (!shown) bl->addWidget(lab(tr("Nada com esse nome."), QStringLiteral("lvEmpty"), box));
        };
        fill(QString());
        connect(q, &QLineEdit::textChanged, box, fill);
        QTimer::singleShot(0, q, [q]() { q->setFocus(); });
    }, 300);
}
void Livre::pickColor(QWidget* anchor, const QString& k)
{
    Region* r = m_doc->resolve(k).region;
    if (!r) return;
    openPop(anchor, [&](QVBoxLayout* l, QFrame* p) {
        popTitle(l, p, tr("Cor"));
        auto* grid = new QGridLayout();
        grid->setSpacing(6);
        int n = 0;
        for (const QString& c : COLORS) {
            auto* d = btn(QString(), QString(), p);
            d->setFixedSize(26, 26);
            d->setStyleSheet(QStringLiteral("QPushButton { background: %1; border-radius: 13px; border: 2px solid %2; }").arg(c, c == r->color ? Theme::textBright() : QStringLiteral("transparent")));
            connect(d, &QPushButton::clicked, this, [this, k, c]() { closePop(); if (Region* x = m_doc->resolve(k).region) { x->color = c; changed(); } QTimer::singleShot(0, this, &Livre::render); });
            grid->addWidget(d, n / 6, n % 6);
            n++;
        }
        l->addLayout(grid);
    }, 220);
}
// Onde o documento mora: numa gaveta ou no Criador de Mundos, como no Explorar do mapa.
QWidget* Livre::whereForm(bool* gaveta, QString* drawer, const QString& owner, const QString& goText, const std::function<void()>& done, const std::function<void()>& cancel, QWidget* parent)
{
    auto* f = new QWidget(parent);
    auto* l = new QVBoxLayout(f);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);
    auto* seg = new QHBoxLayout();
    seg->setSpacing(0);
    auto* a = new QToolButton(f);
    auto* b = new QToolButton(f);
    for (auto* x : { a, b }) { x->setObjectName(QStringLiteral("lvSeg")); x->setCheckable(true); x->setCursor(Qt::PointingHandCursor); x->setFocusPolicy(Qt::NoFocus); x->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); seg->addWidget(x); }
    a->setText(tr("Numa gaveta"));
    b->setText(tr("No Criador de Mundos"));
    l->addLayout(seg);
    auto* drawRow = new QWidget(f);
    auto* dh = new QHBoxLayout(drawRow);
    dh->setContentsMargins(0, 0, 0, 0);
    dh->addWidget(lab(tr("Gaveta"), QStringLiteral("lvFxKey"), drawRow));
    auto* combo = new QComboBox(drawRow);
    combo->setObjectName(QStringLiteral("lvCombo"));
    combo->setFocusPolicy(Qt::NoFocus);
    if (m_panel->m_link.model) for (const Drawer& dr : m_panel->m_link.model->drawers()) combo->addItem(QIcon(dot(QColor(dr.color.isEmpty() ? Theme::accentDefault() : dr.color), 10)), dr.title, dr.key);
    combo->setCurrentIndex(std::max(0, combo->findData(*drawer)));
    if (drawer->isEmpty() && combo->count()) *drawer = combo->currentData().toString();
    connect(combo, &QComboBox::activated, f, [combo, drawer]() { *drawer = combo->currentData().toString(); });
    dh->addWidget(combo, 1);
    l->addWidget(drawRow);
    auto* note = lab(QString(), QStringLiteral("lvAskNote"), f, true);
    l->addWidget(note);
    const auto sync = [a, b, drawRow, note, gaveta, owner]() {
        a->setChecked(*gaveta); b->setChecked(!*gaveta);
        drawRow->setVisible(*gaveta);
        note->setText(*gaveta ? Livre::tr("Vai para a gaveta escolhida, ligado a %1.").arg(owner) : Livre::tr("Vira um documento do lugar %1 no Criador de Mundos.").arg(owner));
    };
    connect(a, &QToolButton::clicked, f, [gaveta, sync]() { *gaveta = true; sync(); });
    connect(b, &QToolButton::clicked, f, [gaveta, sync]() { *gaveta = false; sync(); });
    sync();
    auto* acts = new QHBoxLayout();
    acts->addStretch();
    if (cancel) { auto* no = btn(tr("Cancelar"), QStringLiteral("lvQuiet"), f); connect(no, &QPushButton::clicked, this, [this, cancel]() { QTimer::singleShot(0, this, cancel); }); acts->addWidget(no); }
    auto* go = new QPushButton(goText, f);
    go->setObjectName(QStringLiteral("lvGo"));
    go->setCursor(Qt::PointingHandCursor);
    go->setDefault(true);
    connect(go, &QPushButton::clicked, this, [this, done]() { QTimer::singleShot(0, this, done); });
    acts->addWidget(go);
    l->addLayout(acts);
    QTimer::singleShot(0, go, [go]() { go->setFocus(); });
    return f;
}
void Livre::moveDocWhere(QWidget* anchor, DocLink* d)
{
    auto gaveta = std::make_shared<bool>(d->gaveta);
    auto drawer = std::make_shared<QString>(d->drawer);
    const QString owner = label(d->owner);
    openPop(anchor, [&](QVBoxLayout* l, QFrame* p) {
        popTitle(l, p, tr("Onde este documento mora"));
        l->addWidget(whereForm(gaveta.get(), drawer.get(), owner, tr("Mover"), [this, d, gaveta, drawer]() {
            closePop();
            flushSheet();
            m_panel->docMoveTo(d, *gaveta ? QStringLiteral("g:") + *drawer : QStringLiteral("criador"));
            changed(false);
            toast(tr("Agora está em: %1.").arg(m_panel->docDestLabel(d)), false);
            m_edFor.clear();
            render();
        }, nullptr, p));
    }, 300);
}

// ── a gaveta flutuante ──
QString Livre::navRoot()
{
    for (QString c = m_sel; !c.isEmpty(); c = parentKey(c)) {
        const LK t = kindOf(c);
        if (t == LK::Planet || t == LK::Moon || t == LK::Sys || t == LK::Terr) { m_lastNavRoot = c; return c; }
    }
    if (kindOf(m_sel) == LK::Link && valid(m_lastNavRoot)) return m_lastNavRoot;
    return QString();
}
QSet<QString> Livre::navVisible(const QString& root) const
{
    QSet<QString> keep;
    const QString q = m_navQuery.trimmed();
    if (q.isEmpty()) return keep;
    const std::function<void(const QString&)> walk = [&](const QString& id) {
        for (const QString& k : inside(id)) {
            if (label(k).contains(q, Qt::CaseInsensitive)) for (QString c = k; !c.isEmpty() && c != root; c = parentKey(c)) keep.insert(c);
            walk(k);
        }
    };
    walk(root);
    keep.insert(QStringLiteral("\x01"));   // marca "tem busca"
    return keep;
}
QWidget* Livre::askCard(QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("lvAsk"));
    auto* l = new QVBoxLayout(card);
    l->setContentsMargins(10, 8, 10, 8);
    l->setSpacing(6);
    l->addWidget(lab(tr("Onde salvar “%1”?").arg(m_ask.name), QStringLiteral("lvAskTitle"), card, true));
    auto gaveta = std::make_shared<bool>(m_lastGaveta);
    auto drawer = std::make_shared<QString>(m_lastDrawer);
    const QString parentK = m_ask.parent, name = m_ask.name;
    QString owner = parentK;
    if (const DocLink* pd = docOf(parentK)) owner = pd->owner;
    l->addWidget(whereForm(gaveta.get(), drawer.get(), label(owner), tr("Criar e abrir"), [this, gaveta, drawer, parentK, name]() {
        m_lastGaveta = *gaveta;
        m_lastDrawer = *drawer;
        QSettings st;
        st.setValue(QStringLiteral("galaxy/livreGaveta"), m_lastGaveta);
        st.setValue(QStringLiteral("galaxy/livreDrawer"), m_lastDrawer);
        m_ask = {};
        m_sel = createDoc(name, parentK, *gaveta, *drawer);
        render();
        goBody();
    }, [this]() { m_ask = {}; renderNav(); }, card));
    return card;
}
void Livre::docTree(QVBoxLayout* box, QWidget* parent, const QString& id, int depth, const QSet<QString>* vis)
{
    for (const QString& k : inside(id)) {
        if (vis && !vis->contains(k)) continue;
        const bool ghost = isGhost(k);
        auto* r = btn(QString(), QStringLiteral("lvTn"), parent);
        r->setProperty("on", m_sel == k);
        auto* h = new QHBoxLayout(r);
        h->setContentsMargins(8 + depth * 14, 3, 4, 3);
        h->setSpacing(6);
        h->addWidget(markOf(k, kindOf(k) == LK::Moon ? 12 : 15, r));
        auto* nm = lab(label(k), QStringLiteral("lvTnName"), r);
        nm->setProperty("ghost", ghost);
        nm->setAttribute(Qt::WA_TransparentForMouseEvents);
        h->addWidget(nm, 1);
        if (isCapital(k)) h->addWidget(starMark(r));
        if (const DocLink* d = docOf(k)) { QWidget* tg = whereTag(d, r); tg->setAttribute(Qt::WA_TransparentForMouseEvents); h->addWidget(tg); }
        if (ghost) h->addWidget(ghostActs(k, r));
        else {
            connect(r, &QPushButton::clicked, this, [this, k]() { select(k); });
            peekable(r, k);
            // + no próprio item: documento dentro da cidade, documento dentro do documento
            auto* plus = new QToolButton(r);
            plus->setObjectName(QStringLiteral("lvRowAdd"));
            plus->setIcon(carto::icon(QStringLiteral("plus"), col(Theme::textMuted()), col(Theme::accentDefault()), 12));
            plus->setIconSize(QSize(12, 12));
            plus->setToolTip(tr("Novo documento dentro de %1").arg(label(k)));
            plus->setCursor(Qt::PointingHandCursor);
            plus->setFocusPolicy(Qt::NoFocus);
            connect(plus, &QToolButton::clicked, this, [this, k]() { m_rowAdd = k; m_ask = {}; QTimer::singleShot(0, this, &Livre::renderNav); });
            h->addWidget(plus);
        }
        r->setMinimumHeight(30);
        box->addWidget(r);
        if (m_rowAdd == k) {
            auto* in = new QLineEdit(parent);
            in->setObjectName(QStringLiteral("lvInline"));
            in->setPlaceholderText(tr("Documento em %1, Enter").arg(label(k)));
            in->setProperty("lvEscape", QStringLiteral("nav"));
            in->installEventFilter(this);
            in->setContentsMargins(22 + (depth + 1) * 14, 0, 0, 0);
            connect(in, &QLineEdit::returnPressed, this, [this, in, k]() {
                const QString n = in->text().trimmed();
                if (n.isEmpty()) return;
                m_ask = { n, k };
                m_rowAdd.clear();
                QTimer::singleShot(0, this, &Livre::renderNav);
            });
            box->addWidget(in);
            QTimer::singleShot(0, in, [in]() { in->setFocus(); });
        }
        if (!m_ask.name.isEmpty() && m_ask.parent == k) box->addWidget(askCard(parent));
        docTree(box, parent, k, depth + 1, vis);
    }
}
QWidget* Livre::navPanel(const QString& root, QWidget* parent)
{
    auto* p = new QWidget(parent);
    auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    // cabeçalho: título em serifa, lupa, fixar e fechar
    auto* head = new QWidget(p);
    head->setObjectName(QStringLiteral("lvNavHead"));
    head->setAttribute(Qt::WA_StyledBackground, true);
    auto* hv = new QVBoxLayout(head);
    hv->setContentsMargins(12, 10, 8, 10);
    hv->setSpacing(5);
    auto* hh = new QHBoxLayout();
    hh->setContentsMargins(0, 0, 0, 0);
    hv->addLayout(hh);
    hh->setSpacing(4);
    auto* title = btn(QStringLiteral("  ") + label(root), QStringLiteral("lvNavTitle"), head);
    title->setIcon(QIcon(kindOf(root) == LK::Sys ? starIcon(sysOf(root), 20) : ball(root, 20)));
    title->setIconSize(QSize(20, 20));
    connect(title, &QPushButton::clicked, this, [this, root]() { select(root); });
    hh->addWidget(title, 1);
    auto* sb = mini(QStringLiteral("galSearch"), tr("Buscar nesta gaveta"), m_navSearch, head);
    connect(sb, &QToolButton::clicked, this, [this]() { m_navSearch = !m_navSearch; if (!m_navSearch) m_navQuery.clear(); QTimer::singleShot(0, this, &Livre::renderNav); });
    hh->addWidget(sb);
    auto* pin = mini(QStringLiteral("pin"), m_navPinned ? tr("Soltar a gaveta") : tr("Fixar: não fecha ao clicar fora"), m_navPinned, head);
    connect(pin, &QToolButton::clicked, this, [this]() { m_navPinned = !m_navPinned; QTimer::singleShot(0, this, &Livre::renderNav); });
    hh->addWidget(pin);
    auto* cl = mini(QStringLiteral("close"), tr("Fechar"), false, head);
    cl->setCheckable(false);
    connect(cl, &QToolButton::clicked, this, [this]() { m_navOpen = false; m_nav->hide(); });
    hh->addWidget(cl);
    if (kindOf(root) == LK::Sys) {
        // a listra na cor do império e os mundos do sistema (clique abre, o mouse mostra a prévia)
        const QString terr = ancestor(root, LK::Terr);
        if (const Region* r = terr.isEmpty() ? nullptr : m_doc->resolve(terr).region)
            head->setStyleSheet(QStringLiteral("QWidget#lvNavHead { border-top: 4px solid %1; border-top-left-radius: 12px; border-top-right-radius: 12px; }").arg(r->color));
        QStringList ws;
        for (const QString& c : children(root)) if (kindOf(c) == LK::Planet && !isGhost(c)) ws << c;
        if (!ws.isEmpty()) {
            auto* wr = new QHBoxLayout();
            wr->setContentsMargins(30, 0, 0, 0);
            wr->setSpacing(2);
            for (int i = 0; i < std::min<int>(6, ws.size()); i++) {
                const QString wk = ws[i];
                auto* b = new QToolButton(head);
                b->setObjectName(QStringLiteral("lvNavMini"));
                b->setIcon(QIcon(ball(wk, 18)));
                b->setIconSize(QSize(18, 18));
                b->setFixedSize(24, 24);
                b->setToolTip(label(wk));
                b->setCursor(Qt::PointingHandCursor);
                b->setFocusPolicy(Qt::NoFocus);
                connect(b, &QToolButton::clicked, this, [this, wk]() { select(wk); });
                peekable(b, wk);
                wr->addWidget(b);
            }
            if (ws.size() > 6) wr->addWidget(lab(QStringLiteral("+%1").arg(ws.size() - 6), QStringLiteral("lvSmall"), head));
            wr->addStretch();
            hv->addLayout(wr);
        }
    }
    l->addWidget(head);
    auto* inner = new QWidget(p);
    auto* il = new QVBoxLayout(inner);
    il->setContentsMargins(12, 10, 12, 12);
    il->setSpacing(8);
    if (m_navSearch) {
        auto* q = new QLineEdit(inner);
        q->setObjectName(QStringLiteral("lvNavSearch"));
        q->setPlaceholderText(tr("Buscar em %1…").arg(label(root)));
        q->setText(m_navQuery);
        q->setProperty("lvEscape", QStringLiteral("navq"));
        q->setProperty("lvNavQ", true);
        q->installEventFilter(this);
        connect(q, &QLineEdit::textEdited, this, [this](const QString& v) { m_navQuery = v; QTimer::singleShot(0, this, &Livre::renderNav); });
        il->addWidget(q);
    }
    // o botão grande e as pílulas, como na gaveta
    auto* big = btn(QStringLiteral("  ") + tr("Criar novo documento"), QStringLiteral("lvNavCreate"), inner);
    big->setIcon(QIcon(circlePlus(col(Theme::accentDefault()), 18)));
    big->setIconSize(QSize(18, 18));
    connect(big, &QPushButton::clicked, this, [this]() { m_navAdd = QStringLiteral("doc"); m_ask = {}; QTimer::singleShot(0, this, &Livre::renderNav); });
    il->addWidget(big);
    auto* pills = new QHBoxLayout();
    pills->setSpacing(6);
    const LK rt = kindOf(root);
    QList<QPair<QString, QString>> list { { QStringLiteral("lugar"), tr("+ Lugar") } };
    if (rt == LK::Planet) list << qMakePair(QStringLiteral("lua"), tr("+ Lua"));
    if (rt == LK::Planet || rt == LK::Sys) list << qMakePair(QStringLiteral("construcao"), tr("+ Construção"));
    for (const auto& [kind, text] : list) {
        auto* b = btn(text, QStringLiteral("lvPill"), inner);
        b->setProperty("on", m_navAdd == kind);
        if (kind == QLatin1String("construcao")) connect(b, &QPushButton::clicked, this, [this, b, root]() { pickStruct(b, root, QString()); });
        else connect(b, &QPushButton::clicked, this, [this, kind]() { m_navAdd = kind; m_ask = {}; QTimer::singleShot(0, this, &Livre::renderNav); });
        pills->addWidget(b);
    }
    pills->addStretch();
    il->addLayout(pills);
    if (!m_navAdd.isEmpty()) {
        auto* in = new QLineEdit(inner);
        in->setObjectName(QStringLiteral("lvInline"));
        const QString ph = m_navAdd == QLatin1String("doc") ? tr("Nome do documento") : m_navAdd == QLatin1String("lua") ? tr("Nome da lua") : tr("Nome do lugar");
        in->setPlaceholderText(tr("%1, Enter cria").arg(ph));
        in->setProperty("lvEscape", QStringLiteral("nav"));
        in->installEventFilter(this);
        connect(in, &QLineEdit::returnPressed, this, [this, in, root]() {
            const QString n = in->text().trimmed();
            if (n.isEmpty()) return;
            if (m_navAdd == QLatin1String("doc")) { m_ask = { n, root }; m_navAdd.clear(); QTimer::singleShot(0, this, &Livre::renderNav); return; }
            if (m_navAdd == QLatin1String("lua")) createMoon(n, root, MOON_TYPES[QRandomGenerator::global()->bounded(int(MOON_TYPES.size()))], false);
            else createPlace(n, root, QString(), false);
            in->clear();
            QTimer::singleShot(0, this, [this]() { renderLeft(); renderRight(); renderNav(); });
        });
        il->addWidget(in);
        QTimer::singleShot(0, in, [in]() { in->setFocus(); });
    }
    if (!m_ask.name.isEmpty() && m_ask.parent == root) il->addWidget(askCard(inner));
    // documentos, em árvore
    const QSet<QString> vis = navVisible(root);
    const QSet<QString>* vp = vis.isEmpty() ? nullptr : &vis;
    int docsN = 0;
    const std::function<void(const QString&)> count = [&](const QString& id) { for (const QString& k : inside(id)) { if (kindOf(k) == LK::Doc && !isGhost(k)) docsN++; count(k); } };
    count(root);
    auto* s1 = new QHBoxLayout();
    s1->addWidget(lab(tr("DOCUMENTOS"), QStringLiteral("lvNavSec"), inner));
    s1->addStretch();
    if (docsN) s1->addWidget(lab(QString::number(docsN), QStringLiteral("lvSmall"), inner));
    il->addLayout(s1);
    auto* tree = new QVBoxLayout();
    tree->setSpacing(1);
    docTree(tree, inner, root, 0, vp);
    if (!tree->count()) tree->addWidget(lab(vp ? tr("Nada com esse nome.") : tr("Nenhum ainda."), QStringLiteral("lvEmpty"), inner));
    il->addLayout(tree);
    // conexões
    std::vector<Panel::GLink> links;
    for (const Panel::GLink& x : m_panel->linksOf(root)) if (x.a == root && (!vp || label(x.b).contains(m_navQuery.trimmed(), Qt::CaseInsensitive))) links.push_back(x);
    auto* s2 = new QHBoxLayout();
    s2->addWidget(lab(tr("CONEXÕES"), QStringLiteral("lvNavSec"), inner));
    s2->addStretch();
    if (!links.empty()) s2->addWidget(lab(QString::number(links.size()), QStringLiteral("lvSmall"), inner));
    il->addSpacing(4);
    il->addLayout(s2);
    for (const Panel::GLink& x : links) {
        const QString lk = QStringLiteral("X") + x.id;
        auto* r = btn(QString(), QStringLiteral("lvTn"), inner);
        r->setProperty("on", m_sel == lk);
        auto* h = new QHBoxLayout(r);
        h->setContentsMargins(8, 4, 8, 4);
        h->setSpacing(8);
        h->addWidget(markOf(x.b, isWorld(x.b) ? 20 : 14, r));
        auto* tv = new QVBoxLayout();
        tv->setSpacing(0);
        auto* n1 = lab(label(x.b), QStringLiteral("lvTnName"), r);
        n1->setAttribute(Qt::WA_TransparentForMouseEvents);
        tv->addWidget(n1);
        const QString type = m_doc->linkTypes.value(x.id);
        QStringList sub;
        if (!type.isEmpty()) sub << type;
        if (!x.label.isEmpty() && !x.label.contains(QStringLiteral(" e "))) sub << x.label;
        if (!sub.isEmpty()) { auto* s = lab(sub.join(QStringLiteral(" · ")), QStringLiteral("lvSmall"), r); s->setAttribute(Qt::WA_TransparentForMouseEvents); tv->addWidget(s); }
        h->addLayout(tv, 1);
        auto* ic = new QLabel(r);
        ic->setPixmap(carto::iconPixmap(QStringLiteral("link"), col(Theme::textMuted()), 13));
        ic->setAttribute(Qt::WA_TransparentForMouseEvents);
        h->addWidget(ic);
        connect(r, &QPushButton::clicked, this, [this, lk]() { select(lk); });
        il->addWidget(r);
    }
    if (links.empty()) il->addWidget(lab(tr("Nenhuma ainda."), QStringLiteral("lvEmpty"), inner));
    auto* cw = new QHBoxLayout();
    auto* cb = btn(tr("+ Conectar a…"), QStringLiteral("lvPill"), inner);
    cb->setToolTip(tr("Conectar %1 a outro planeta, sistema ou território").arg(label(root)));
    connect(cb, &QPushButton::clicked, this, [this, cb, root]() { pickConnect(cb, root); });
    cw->addWidget(cb);
    cw->addStretch();
    il->addLayout(cw);
    auto* scroll = new QScrollArea(p);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }"));
    scroll->setWidget(inner);
    l->addWidget(scroll, 1);
    return p;
}
void Livre::renderNav()
{
    const QString root = navRoot();
    // quem tinha o foco na gaveta continua com ele (a busca, por exemplo)
    QWidget* f = QApplication::focusWidget();
    const bool searching = f && f->property("lvNavQ").toBool();
    const int pos = searching ? static_cast<QLineEdit*>(f)->cursorPosition() : 0;
    if (QLayout* old = m_nav->layout()) { clearLay(old); delete old; }
    if (root.isEmpty() || !m_navOpen || !valid(root)) { m_nav->hide(); return; }
    auto* l = new QVBoxLayout(m_nav);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(navPanel(root, m_nav));
    m_nav->show();
    m_nav->raise();
    placeNav();
    if (searching) for (QLineEdit* q : m_nav->findChildren<QLineEdit*>()) if (q->property("lvNavQ").toBool()) { q->setFocus(); q->setCursorPosition(pos); }
}
void Livre::placeNav()
{
    const QString root = m_lastNavRoot;
    QWidget* anchor = nullptr;
    if (m_left->widget()) for (QWidget* w : m_left->widget()->findChildren<QWidget*>()) if (w->property("lvKey").toString() == root && w->isVisible()) { anchor = w; break; }
    const QRect lb(m_left->mapTo(this, QPoint(0, 0)), m_left->size());
    const int w = 330;
    const int top = anchor ? anchor->mapTo(this, QPoint(0, 0)).y() - 10 : lb.top();
    const int maxH = height() - 30;
    m_nav->setFixedWidth(w);
    // a altura de tudo que está dentro, até caber na tela; passou disso, rola
    const auto* sc = m_nav->findChild<QScrollArea*>();
    const int want = 58 + (sc && sc->widget() ? sc->widget()->sizeHint().height() : 300);
    const int h = std::min(maxH, std::max(200, want));
    m_nav->setFixedHeight(h);
    m_nav->move(lb.right() + 8, std::clamp(top, 10, std::max(10, height() - h - 20)));
}

// ── território com forma ──
QWidget* Livre::terrMapEl(const QString& k, QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);
    l->addWidget(lab(tr("FORMA NO MAPA"), QStringLiteral("lvSideTitle"), w));
    const Region* r = m_doc->resolve(k).region;
    const bool has = r && !r->stamps.empty();
    auto* mini2 = new TerrMini(w);
    mini2->setFixedHeight(150);
    mini2->setCursor(Qt::PointingHandCursor);
    mini2->setToolTip(tr("Desenhar %1").arg(label(k)));
    Doc* d = m_doc;
    const QString key = k;
    mini2->paint = [d, key, has](QPainter& p, QRectF rc) {
        p.setClipRect(rc);
        p.fillRect(rc, QColor(4, 6, 11));
        double x0 = -1.1, y0 = -1.1, x1 = 1.1, y1 = 1.1;
        const Region* r = nullptr;
        for (const auto& x : d->regions) if (x->key == key) r = x.get();
        if (r && !r->stamps.empty()) {
            x0 = 9; y0 = 9; x1 = -9; y1 = -9;
            for (const auto& s : r->stamps) { x0 = std::min(x0, s[0] - s[2] * 1.5); y0 = std::min(y0, s[1] - s[2] * 1.5); x1 = std::max(x1, s[0] + s[2] * 1.5); y1 = std::max(y1, s[1] + s[2] * 1.5); }
            const double cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, half = std::max({ (x1 - x0) / 2, (y1 - y0) / 2, 0.2 }) * 1.15;
            x0 = cx - half; x1 = cx + half; y0 = cy - half; y1 = cy + half;
        }
        const double sc = std::max(rc.width() / (x1 - x0), rc.height() / (y1 - y0));
        const QPointF o(rc.center().x() - (x0 + x1) / 2 * sc, rc.center().y() - (y0 + y1) / 2 * sc);
        p.setOpacity(0.55);
        p.drawImage(QRectF(o.x() - EXT * sc, o.y() - EXT * sc, 2 * EXT * sc, 2 * EXT * sc), d->galImg);
        p.setOpacity(1);
        for (const auto& x : d->regions) {
            if (x->stamps.empty()) continue;
            QColor c(x->color);
            c.setAlphaF(x->key == key ? 0.55 : 0.22);
            QPainterPath path;
            path.setFillRule(Qt::WindingFill);
            for (const auto& s : x->stamps) path.addEllipse(QPointF(o.x() + s[0] * sc, o.y() + s[1] * sc), s[2] * sc, s[2] * sc);
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawPath(path.simplified());
        }
        if (!has) {
            p.setPen(QColor(230, 236, 245, 200));
            QFont f = p.font(); f.setPixelSize(12); p.setFont(f);
            p.drawText(rc, Qt::AlignCenter, Livre::tr("Sem forma ainda"));
        }
    };
    l->addWidget(mini2);
    auto* b = btn(has ? tr("Redesenhar território") : tr("Desenhar território"), QStringLiteral("lvFxOpen"), w);
    connect(b, &QPushButton::clicked, this, [this, k]() { openDraw(k); });
    l->addWidget(b);
    mini2->installEventFilter(this);
    mini2->setProperty("lvDraw", k);
    return w;
}

} // namespace galaxy
