// Criação Livre: a forma do território, pintada à mão por cima da galáxia. Pincel, borracha (ou Shift), mover,
// tamanho, desfazer, limpar e pronto. Os outros territórios aparecem apagados, de guia. A forma vale no mapa também.

#include "GalaxyLivre.h"
#include "GalaxyCanvas.h"
#include "GalaxyDoc.h"
#include "GalaxyOrganic.h"

#include "../Theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace galaxy {

namespace {
using Stamps = std::vector<std::array<double, 3>>;
QString shortName(QString n)
{
    static const QStringList pre = { QStringLiteral("Império Matriarcal de "), QStringLiteral("Império de "), QStringLiteral("Império "), QStringLiteral("República "), QStringLiteral("Reino de "), QStringLiteral("Reino ") };
    for (const QString& p : pre) if (n.startsWith(p, Qt::CaseInsensitive)) return n.mid(p.size());
    return n;
}
// O mapa onde se pinta: a galáxia de fundo, as manchas de cada território e os sistemas criados.
class DrawCanvas : public QWidget {
public:
    DrawCanvas(Doc* doc, const QString& key, QWidget* parent) : QWidget(parent), m_doc(doc), m_key(key)
    {
        setMouseTracking(true);
        setCursor(Qt::CrossCursor);
        Region* r = region();
        if (r && !r->stamps.empty()) {
            double x0 = 9, y0 = 9, x1 = -9, y1 = -9;
            for (const auto& s : r->stamps) { x0 = std::min(x0, s[0] - s[2] * 1.5); y0 = std::min(y0, s[1] - s[2] * 1.5); x1 = std::max(x1, s[0] + s[2] * 1.5); y1 = std::max(y1, s[1] + s[2] * 1.5); }
            const double w = std::max({ x1 - x0, y1 - y0, 0.25 });
            cx = (x0 + x1) / 2; cy = (y0 + y1) / 2; z = std::clamp(1.3 / w, 1.0, 5.0);
        }
    }
    QString tool = QStringLiteral("brush");
    double size = 0.06;
    std::vector<Stamps> hist;
    std::function<void()> edited;
    Region* region() const { for (const auto& r : m_doc->regions) if (r->key == m_key) return r.get(); return nullptr; }
protected:
    double base() const { return std::max(width(), height()) * 0.95 * z; }
    // y pra cima, como no mapa (a imagem da galáxia tem o +y no topo)
    QPointF toScreen(double x, double y) const { const double s = base() / 2; return QPointF(width() / 2.0 + (x - cx) * s, height() / 2.0 - (y - cy) * s); }
    void toWorld(QPointF p, double& x, double& y) const { const double s = base() / 2; x = cx + (p.x() - width() / 2.0) / s; y = cy - (p.y() - height() / 2.0) / s; }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.fillRect(rect(), QColor(4, 6, 11));
        const double s = base() / 2;
        const QPointF o = toScreen(0, 0);
        p.setOpacity(0.55);
        p.drawImage(QRectF(o.x() - EXT * s, o.y() - EXT * s, 2 * EXT * s, 2 * EXT * s), m_doc->galImg);
        p.setOpacity(1);
        // os territórios com a fronteira orgânica (a mesma do minimapa); o que se pinta fica em evidência
        const double dpr = devicePixelRatioF();
        const QPointF a0 = toScreen(0, 0), a1 = toScreen(1, 0), a2 = toScreen(0, 1);
        const double A[6] = { a0.x() * dpr, a0.y() * dpr, a1.x() * dpr, a1.y() * dpr, a2.x() * dpr, a2.y() * dpr };
        const int W = int(std::lround(width() * dpr)), H = int(std::lround(height() * dpr));
        for (const auto& r : m_doc->regions) {
            const TerrField F = terrField(*m_doc, *r);
            if (terrEmpty(F)) continue;
            const bool me = r->key == m_key;
            const TerrImage im = terrImage(*m_doc, F, QColor(r->color.isEmpty() ? QStringLiteral("#8e9db4") : r->color), A, W, H, s * dpr, dpr, m_paint || m_pan);
            if (!im.empty) {
                p.setOpacity(me ? 1.0 : 0.45);
                p.drawImage(QRectF(im.x / dpr, im.y / dpr, im.img.width() / dpr, im.img.height() / dpr), im.img);
                p.setOpacity(1);
            }
            double wx = 0, wy = 0;
            if (!terrCenter(F, wx, wy)) continue;
            const QPointF c = toScreen(wx, wy);
            QFont f = font(); f.setPixelSize(int(std::clamp(base() / 70, 10.0, 16.0))); f.setWeight(QFont::DemiBold); f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
            p.setFont(f);
            const QRectF tr(c.x() - 160, c.y() - 12, 320, 24);
            p.setPen(QColor(0, 0, 0, 180));
            p.drawText(tr.translated(0, 1), Qt::AlignCenter, shortName(r->name).toUpper());
            p.setPen(me ? QColor(242, 245, 250) : QColor(230, 236, 245, 128));
            p.drawText(tr, Qt::AlignCenter, shortName(r->name).toUpper());
        }
        // os sistemas que existem, de referência
        for (const auto& sy : m_doc->gal->systems) {
            if (sy->isCore || sy->gone) continue;
            const QPointF q = toScreen(sy->x, sy->y);
            const auto c = sy->stars[0].color;
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(c[0], c[1], c[2]));
            p.drawEllipse(q, 2.6, 2.6);
        }
        // o pincel
        if (m_hover && tool != QLatin1String("move")) {
            p.setPen(QPen(QColor(255, 255, 255, 160), 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            const double R = size / std::sqrt(z) * s;
            p.drawEllipse(m_mouse, R, R);
        }
    }
    void paintAt(QPointF at, bool erase)
    {
        Region* r = region();
        if (!r) return;
        double x, y;
        toWorld(at, x, y);
        const double R = size / std::sqrt(z);
        if (erase) r->stamps.erase(std::remove_if(r->stamps.begin(), r->stamps.end(), [&](const std::array<double, 3>& st) { return std::hypot(st[0] - x, st[1] - y) <= R; }), r->stamps.end());
        else if (!m_hasLast || std::hypot(m_lastX - x, m_lastY - y) > R * 0.45) { r->stamps.push_back({ x, y, R }); m_lastX = x; m_lastY = y; m_hasLast = true; }
        if (edited) edited();
        update();
    }
    void mousePressEvent(QMouseEvent* e) override
    {
        m_mouse = e->position();
        if (tool == QLatin1String("move") || e->button() == Qt::MiddleButton) { m_pan = true; m_panAt = e->position(); m_panX = cx; m_panY = cy; setCursor(Qt::ClosedHandCursor); return; }
        if (e->button() != Qt::LeftButton) return;
        if (Region* r = region()) hist.push_back(r->stamps);
        m_paint = true;
        m_hasLast = false;
        paintAt(e->position(), (e->modifiers() & Qt::ShiftModifier) || tool == QLatin1String("erase"));
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        m_mouse = e->position();
        m_hover = true;
        if (m_pan) { const double s = base() / 2; cx = m_panX - (e->position().x() - m_panAt.x()) / s; cy = m_panY + (e->position().y() - m_panAt.y()) / s; update(); return; }
        if (m_paint) paintAt(e->position(), (e->modifiers() & Qt::ShiftModifier) || tool == QLatin1String("erase"));
        else update();
    }
    void mouseReleaseEvent(QMouseEvent*) override { m_paint = false; if (m_pan) { m_pan = false; setCursor(Qt::CrossCursor); } }
    void leaveEvent(QEvent*) override { m_hover = false; update(); }
    void wheelEvent(QWheelEvent* e) override
    {
        double wx, wy;
        toWorld(e->position(), wx, wy);
        z = std::clamp(z * std::pow(1.0016, e->angleDelta().y()), 0.8, 8.0);
        // aproxima em volta do ponteiro
        double nx, ny;
        toWorld(e->position(), nx, ny);
        cx += wx - nx; cy += wy - ny;
        update();
        e->accept();
    }
private:
    Doc* m_doc;
    QString m_key;
    double cx = 0, cy = 0, z = 1;
    bool m_paint = false, m_pan = false, m_hover = false, m_hasLast = false;
    double m_lastX = 0, m_lastY = 0, m_panX = 0, m_panY = 0;
    QPointF m_mouse, m_panAt;
};
QPushButton* barBtn(const QString& text, QWidget* parent, bool checkable = false)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(QStringLiteral("lvDrawBtn"));
    b->setCheckable(checkable);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}
}

void Livre::openDraw(const QString& terr)
{
    Region* r = m_doc->resolve(terr).region;
    if (!r || m_draw) return;
    closePop();
    hidePeek();
    flushSheet();
    const Stamps before = r->stamps;
    auto* ov = new QWidget(this);
    ov->setObjectName(QStringLiteral("lvDraw"));
    ov->setAttribute(Qt::WA_StyledBackground, true);
    const QColor a = Theme::toColor(Theme::accentDefault());
    ov->setStyleSheet(QStringLiteral(
        "QWidget#lvDraw { background: #04060b; }"
        "QWidget#lvDrawBar { background: rgba(12,16,24,0.94); border-bottom: 1px solid rgba(236,241,250,0.12); }"
        "QLabel#lvDrawTitle { color: #eef2f8; font-size: 15px; font-weight: 700; }"
        "QLabel#lvDrawSmall, QLabel#lvDrawHint { color: rgba(236,241,250,0.65); font-size: 12px; }"
        "QPushButton#lvDrawBtn { background: transparent; border: 1px solid rgba(236,241,250,0.2); border-radius: 8px; color: #e6ecf5; font-size: 12px; padding: 5px 12px; }"
        "QPushButton#lvDrawBtn:hover { border-color: %1; }"
        "QPushButton#lvDrawBtn:checked { background: %2; border-color: %1; }"
        "QPushButton#lvDrawGo { background: %1; border: none; border-radius: 8px; color: white; font-size: 12px; font-weight: 700; padding: 6px 16px; }")
        .arg(a.name(), QStringLiteral("rgba(%1,%2,%3,0.3)").arg(a.red()).arg(a.green()).arg(a.blue())));
    auto* v = new QVBoxLayout(ov);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto* bar = new QWidget(ov);
    bar->setObjectName(QStringLiteral("lvDrawBar"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(16, 10, 16, 10);
    h->setSpacing(8);
    auto* sq = new QLabel(bar);
    sq->setFixedSize(14, 14);
    sq->setStyleSheet(QStringLiteral("background: %1; border-radius: 3px;").arg(r->color));
    h->addWidget(sq);
    auto* title = new QLabel(r->name, bar);
    title->setObjectName(QStringLiteral("lvDrawTitle"));
    h->addWidget(title);
    h->addSpacing(18);
    auto* cv = new DrawCanvas(m_doc, terr, ov);
    QList<QPushButton*> tools;
    for (const auto& [id, text] : { std::pair<QString, QString> { QStringLiteral("brush"), tr("Pincel") }, { QStringLiteral("erase"), tr("Borracha") }, { QStringLiteral("move"), tr("Mover") } }) {
        auto* b = barBtn(text, bar, true);
        b->setChecked(id == QLatin1String("brush"));
        const QString tid = id;
        connect(b, &QPushButton::clicked, ov, [cv, b, tid]() { cv->tool = tid; cv->setCursor(tid == QLatin1String("move") ? Qt::OpenHandCursor : Qt::CrossCursor); b->setChecked(true); });
        tools << b;
        h->addWidget(b);
    }
    for (QPushButton* b : tools) connect(b, &QPushButton::clicked, ov, [tools, b]() { for (QPushButton* x : tools) x->setChecked(x == b); });
    h->addSpacing(10);
    auto* szl = new QLabel(tr("Tamanho"), bar);
    szl->setObjectName(QStringLiteral("lvDrawSmall"));
    h->addWidget(szl);
    auto* size = new QSlider(Qt::Horizontal, bar);
    size->setRange(2, 16);
    size->setValue(6);
    size->setFixedWidth(120);
    size->setFocusPolicy(Qt::NoFocus);
    connect(size, &QSlider::valueChanged, ov, [cv](int v) { cv->size = v / 100.0; });
    h->addWidget(size);
    h->addStretch();
    auto* undo = barBtn(tr("Desfazer"), bar);
    connect(undo, &QPushButton::clicked, ov, [cv]() { if (cv->hist.empty()) return; if (Region* x = cv->region()) x->stamps = cv->hist.back(); cv->hist.pop_back(); cv->update(); });
    h->addWidget(undo);
    auto* clear = barBtn(tr("Limpar"), bar);
    connect(clear, &QPushButton::clicked, ov, [cv]() { if (Region* x = cv->region()) { cv->hist.push_back(x->stamps); x->stamps.clear(); } cv->update(); });
    h->addWidget(clear);
    auto* done = new QPushButton(tr("Pronto"), bar);
    done->setObjectName(QStringLiteral("lvDrawGo"));
    done->setCursor(Qt::PointingHandCursor);
    h->addWidget(done);
    v->addWidget(bar);
    v->addWidget(cv, 1);
    auto* hint = new QLabel(tr("Pinte arrastando. Shift apaga. Rodinha aproxima. Os outros territórios aparecem apagados para servir de guia."), ov);
    hint->setObjectName(QStringLiteral("lvDrawHint"));
    hint->setAlignment(Qt::AlignCenter);
    hint->setContentsMargins(0, 8, 0, 10);
    v->addWidget(hint);
    ov->setGeometry(rect());
    ov->show();
    ov->raise();
    m_draw = ov;
    const QString key = terr;
    const auto finish = [this, key, before]() {
        if (!m_draw) return;
        m_draw->hide();
        m_draw->deleteLater();
        m_draw = nullptr;
        Region* x = m_doc->resolve(key).region;
        if (x && x->stamps != before) {
            const Stamps after = x->stamps;
            Doc* d = m_doc;
            // Na galáxia livre os sistemas do território não têm lugar escolhido: com a forma nova, eles se
            // reespalham por ela inteira (cada um no ponto mais longe dos que já foram postos, com folga da borda).
            struct Moved { System* s; double x0, y0, x1, y1; };
            auto moved = std::make_shared<std::vector<Moved>>();
            if (m_doc->opts.free && !after.empty()) {
                QList<int> ids;
                for (int id : x->systems) { const System* s = m_doc->gal->systems[size_t(id)].get(); if (s->hand && (!s->gone || m_doc->isSolo(*s))) ids << id; }
                std::sort(ids.begin(), ids.end());
                QSet<int> waiting(ids.begin(), ids.end());
                for (int id : ids) {
                    System* s = m_doc->gal->systems[size_t(id)].get();
                    double nx = s->x, ny = s->y;
                    const bool ok = spreadSpot(*m_doc, x, nx, ny, s->seed ^ 0x9e3779b9u, waiting);   // ele mesmo ainda não conta
                    waiting.remove(id);
                    if (ok) { moved->push_back({ s, s->x, s->y, nx, ny }); s->x = nx; s->y = ny; s->rCore = std::hypot(nx, ny); }
                }
                // mundos soltos que a forma nova cobriu saem pra fora (solto não mora dentro de território)
                const TerrField F = terrField(*m_doc, *x);
                for (const auto& sy : m_doc->gal->systems) {
                    System* s = sy.get();
                    if (!m_doc->isSolo(*s) || m_doc->regionOfSys(*s) || terrDist(F, s->x, s->y) > 0.04) continue;
                    double nx = s->x, ny = s->y;
                    if (spreadSpot(*m_doc, nullptr, nx, ny, s->seed ^ 0x51ed270bu, { s->id })) { moved->push_back({ s, s->x, s->y, nx, ny }); s->x = nx; s->y = ny; s->rCore = std::hypot(nx, ny); }
                }
                if (!moved->empty()) m_doc->layoutSystems();
            }
            undoable(tr("Forma de %1 desenhada.").arg(x->name),
                [d, key, before, moved] { for (auto& q : d->regions) if (q->key == key) q->stamps = before; for (const Moved& m : *moved) { m.s->x = m.x0; m.s->y = m.y0; } if (!moved->empty()) d->layoutSystems(); d->rev++; },
                [d, key, after, moved] { for (auto& q : d->regions) if (q->key == key) q->stamps = after; for (const Moved& m : *moved) { m.s->x = m.x1; m.s->y = m.y1; } if (!moved->empty()) d->layoutSystems(); d->rev++; });
            m_doc->rev++;
            changed();
        }
        m_balls.clear();
        render();
    };
    connect(done, &QPushButton::clicked, this, finish);
    ov->setProperty("lvDrawFinish", true);
    // Esc também termina
    auto* esc = new QPushButton(ov);
    esc->setShortcut(QKeySequence(Qt::Key_Escape));
    esc->resize(0, 0);
    connect(esc, &QPushButton::clicked, this, finish);
    cv->setFocus();
}

} // namespace galaxy
