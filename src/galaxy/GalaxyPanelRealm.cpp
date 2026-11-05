// Territórios (a doca) e Nomes (o campo que abre em cima do mapa).

#include "GalaxyPanel.h"

#include "GalaxyHab.h"

#include "../Theme.h"
#include "../carto/CartoIcons.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace galaxy {

namespace {
const QStringList REGION_COLORS = { QStringLiteral("#e0a458"), QStringLiteral("#6fb3e8"), QStringLiteral("#9bc49a"), QStringLiteral("#c98bd9"), QStringLiteral("#e07a6a"), QStringLiteral("#d6c46a"), QStringLiteral("#7fd1c7") };
}

void Panel::buildRealmDock()
{
    m_dock = new QFrame(this);
    m_dock->setObjectName(QStringLiteral("cartoCard"));
    m_dock->setFixedWidth(320);
    auto* l = new QVBoxLayout(m_dock);
    l->setContentsMargins(14, 12, 14, 14);
    l->setSpacing(8);
    m_dockTitle = new QLabel(tr("Territórios"), m_dock);
    m_dockTitle->setObjectName(QStringLiteral("cartoCardTitle"));
    m_dockTitle->setCursor(Qt::OpenHandCursor);
    m_dockTitle->installEventFilter(this);
    l->addWidget(m_dockTitle);
    m_dockBody = new QWidget(m_dock);
    l->addWidget(m_dockBody);
    m_dock->hide();

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName(QStringLiteral("gmNameEdit"));
    m_nameEdit->setFixedWidth(200);
    m_nameEdit->setAlignment(Qt::AlignCenter);
    m_nameEdit->hide();
    m_nameEdit->installEventFilter(this);
    connect(m_nameEdit, &QLineEdit::editingFinished, this, [this]() {
        if (m_nameEdit->isHidden()) return;
        m_nameEdit->hide();
        if (m_nameFn) { const auto fn = m_nameFn; m_nameFn = nullptr; fn(m_nameEdit->text().trimmed()); }
        scheduleSave();
        m_canvas->setFocus();
        m_canvas->update();
    });
}
void Panel::renderRealmDock()
{
    Canvas& C = *m_canvas;
    bool found = false;
    for (const auto& r : m_doc.regions) if (r->key == C.realmActive) found = true;
    if (!found) C.realmActive = m_doc.regions.empty() ? QString() : m_doc.regions[0]->key;

    m_dockBody->hide();
    m_dockBody->deleteLater();
    m_dockBody = new QWidget(m_dock);
    static_cast<QVBoxLayout*>(m_dock->layout())->addWidget(m_dockBody);
    auto* l = new QVBoxLayout(m_dockBody);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);
    const QColor muted = Theme::toColor(Theme::textMuted()), accent = Theme::toColor(Theme::accentDefault());

    for (const auto& up : m_doc.regions) {
        Region* r = up.get();
        auto* row = new QFrame(m_dockBody);
        row->setObjectName(QStringLiteral("cartoRow"));
        row->setProperty("sel", r->key == C.realmActive);
        row->setCursor(Qt::PointingHandCursor);
        row->setProperty("gmRegion", r->key);
        row->installEventFilter(this);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(6, 3, 3, 3);
        h->setSpacing(6);
        auto* sw = new QPushButton(row);
        sw->setFixedSize(16, 16);
        sw->setCursor(Qt::PointingHandCursor);
        sw->setFocusPolicy(Qt::NoFocus);
        sw->setToolTip(tr("Trocar a cor"));
        sw->setStyleSheet(QStringLiteral("QPushButton { background: %1; border: none; border-radius: 8px; }").arg(r->color));
        connect(sw, &QPushButton::clicked, this, [this, r]() {
            const int i = REGION_COLORS.indexOf(r->color);
            r->color = REGION_COLORS[(i + 1) % REGION_COLORS.size()];
            m_canvas->docChanged();
            renderRealmDock();
            scheduleSave();
        });
        auto* nm = new QLineEdit(r->name, row);
        nm->setObjectName(QStringLiteral("cartoRowName"));
        connect(nm, &QLineEdit::editingFinished, this, [this, r, nm]() {
            if (nm->text().trimmed().isEmpty() || nm->text().trimmed() == r->name) return;
            r->name = nm->text().trimmed();
            syncFocusBar();
            scheduleSave();
            m_canvas->update();
        });
        auto* n = new QLabel(QString::number(r->systems.size()), row);
        n->setObjectName(QStringLiteral("cartoNote"));
        auto* del = new QToolButton(row);
        del->setObjectName(QStringLiteral("cartoBtn"));
        del->setFixedSize(26, 26);
        del->setIconSize(QSize(15, 15));
        del->setIcon(carto::icon(QStringLiteral("trash"), muted, accent, 15));
        del->setCursor(Qt::PointingHandCursor);
        del->setFocusPolicy(Qt::NoFocus);
        del->setToolTip(tr("Desfazer o território"));
        const QString key = r->key;
        connect(del, &QToolButton::clicked, this, [this, key]() {
            size_t i = 0;
            for (; i < m_doc.regions.size(); i++) if (m_doc.regions[i]->key == key) break;
            if (i >= m_doc.regions.size()) return;
            auto held = std::make_shared<std::unique_ptr<Region>>(std::move(m_doc.regions[i]));
            m_doc.regions.erase(m_doc.regions.begin() + long(i));
            const QString name = (*held)->name;
            Doc* d = &m_doc;
            m_canvas->docChanged();
            m_canvas->pushUndo(tr("%1 desfeito.").arg(name),
                [d, held, i] { d->regions.insert(d->regions.begin() + long(std::min(i, d->regions.size())), std::move(*held)); },
                [d, held, key] { for (size_t j = 0; j < d->regions.size(); j++) if (d->regions[j]->key == key) { *held = std::move(d->regions[j]); d->regions.erase(d->regions.begin() + long(j)); break; } });
            renderRealmDock();
        });
        h->addWidget(sw); h->addWidget(nm, 1); h->addWidget(n); h->addWidget(del);
        l->addWidget(row);
    }
    auto* add = new QToolButton(m_dockBody);
    add->setObjectName(QStringLiteral("cartoLink"));
    add->setText(tr("+ Novo território"));
    add->setCursor(Qt::PointingHandCursor);
    add->setFocusPolicy(Qt::NoFocus);
    connect(add, &QToolButton::clicked, this, [this]() {
        QString color;
        for (const QString& c : REGION_COLORS) { bool used = false; for (const auto& r : m_doc.regions) if (r->color == c) used = true; if (!used) { color = c; break; } }
        if (color.isEmpty()) color = REGION_COLORS[int(m_doc.regions.size()) % REGION_COLORS.size()];
        Region* r = m_doc.newRegion(tr("Território %1").arg(m_doc.regions.size() + 1), color, {});
        m_canvas->realmActive = r->key;
        m_canvas->realmMode = RealmMode::Seed;
        renderRealmDock();
        scheduleSave();
        syncUi();
    });
    l->addWidget(add, 0, Qt::AlignLeft);
    auto* cap = new QLabel(tr("FERRAMENTA"), m_dockBody);
    cap->setObjectName(QStringLiteral("cartoCaption"));
    l->addWidget(cap);
    auto* seg = new QHBoxLayout();
    seg->setSpacing(4);
    const QList<std::tuple<RealmMode, QString, QString>> modes = {
        { RealmMode::Seed, tr("Semear"), tr("Clique pra plantar o território; Shift+clique tira") },
        { RealmMode::Paint, tr("Pintar"), tr("Pinte área pro território selecionado: os sistemas embaixo entram nele") },
        { RealmMode::Neutral, tr("Neutro"), tr("Pinte espaço sem dono: os sistemas embaixo saem do território") },
    };
    for (const auto& [v, label, tip] : modes) {
        auto* b = new QToolButton(m_dockBody);
        b->setObjectName(QStringLiteral("cartoSeg"));
        b->setText(label);
        b->setToolTip(tip);
        b->setCheckable(true);
        b->setChecked(C.realmMode == v);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        const RealmMode mode = v;
        connect(b, &QToolButton::clicked, this, [this, mode]() { m_canvas->realmMode = mode; QTimer::singleShot(0, this, [this]() { renderRealmDock(); syncUi(); }); m_canvas->update(); });
        seg->addWidget(b);
    }
    l->addLayout(seg);
    auto* sr = new QHBoxLayout();
    sr->setSpacing(6);
    const bool seed = C.realmMode == RealmMode::Seed;
    auto* scap = new QLabel(seed ? tr("ALCANCE") : tr("PINCEL"), m_dockBody);
    scap->setObjectName(QStringLiteral("cartoCaption"));
    auto* s = new QSlider(Qt::Horizontal, m_dockBody);
    s->setFocusPolicy(Qt::NoFocus);
    auto* val = new QLabel(m_dockBody);
    val->setObjectName(QStringLiteral("cartoValue"));
    const auto fmtReach = [this](int v) { return v <= 20 ? tr("só ele") : v >= 400 ? tr("sem limite") : fmtLy(v / 1000.0, m_doc.gal->ly); };
    if (seed) { s->setRange(20, 400); s->setValue(C.realmReach); val->setText(fmtReach(C.realmReach)); }
    else { s->setRange(6, 80); s->setValue(C.realmBrush); val->setText(QString::number(C.realmBrush)); }
    connect(s, &QSlider::valueChanged, this, [this, seed, val, fmtReach](int v) {
        if (seed) { m_canvas->realmReach = v; val->setText(fmtReach(v)); }
        else { m_canvas->realmBrush = v; val->setText(QString::number(v)); m_canvas->update(); }
    });
    sr->addWidget(scap); sr->addWidget(s, 1); sr->addWidget(val);
    l->addLayout(sr);
    auto* note = new QLabel(C.realmMode == RealmMode::Paint ? tr("Pinte área pro território selecionado: os sistemas embaixo do pincel entram nele.")
                            : C.realmMode == RealmMode::Neutral ? tr("Pinte espaço sem dono: os sistemas embaixo do pincel saem do território.")
                                                                : m_doc.opts.organic ? tr("Clique em qualquer ponto do céu: o sistema mais perto aparece no território selecionado, com os vizinhos dentro do alcance. Shift+clique tira.")
                                                                                     : tr("Clique num sistema pra plantar o território selecionado: ele leva junto os vizinhos sem dono dentro do alcance. Shift+clique tira."), m_dockBody);
    note->setObjectName(QStringLiteral("cartoNote"));
    note->setWordWrap(true);
    l->addWidget(note);
    // Autopopular, por território.
    Region* active = nullptr;
    for (const auto& q : m_doc.regions) if (q->key == C.realmActive) active = q.get();
    if (active) {
        Region* r = active;
        auto* pop = new QToolButton(m_dockBody);
        pop->setObjectName(QStringLiteral("cartoCheck"));
        pop->setCheckable(true);
        pop->setChecked(r->autoPop);
        pop->setText(tr("Autopopular"));
        pop->setToolTip(tr("Os mundos das classes escolhidas ganham gente, satélites, estações e frotas ao entrar neste território. Mundo que você já editou fica como está."));
        pop->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        pop->setIcon(carto::icon(r->autoPop ? QStringLiteral("check") : QStringLiteral("box"), muted, accent));
        pop->setCursor(Qt::PointingHandCursor);
        pop->setFocusPolicy(Qt::NoFocus);
        pop->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(pop, &QToolButton::clicked, this, [this, r](bool on) {
            r->autoPop = on;
            if (on) m_canvas->populateRegion(*r);
            scheduleSave();
            QTimer::singleShot(0, this, [this]() { renderRealmDock(); });
        });
        l->addWidget(pop);
        if (r->autoPop) {
            for (int rowStart : { 1, 4 }) {
                auto* classes = new QHBoxLayout();
                classes->setSpacing(4);
                for (int n = rowStart; n < rowStart + 3; n++) {
                    auto* b = new QToolButton(m_dockBody);
                    b->setObjectName(QStringLiteral("cartoSeg"));
                    b->setText(habLabel(n));
                    b->setToolTip(habDesc(n));
                    b->setCheckable(true);
                    b->setChecked(r->popMask & (1 << n));
                    b->setCursor(Qt::PointingHandCursor);
                    b->setFocusPolicy(Qt::NoFocus);
                    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                    connect(b, &QToolButton::clicked, this, [this, r, n](bool on) {
                        if (on) r->popMask |= 1 << n; else r->popMask &= ~(1 << n);
                        if (on) m_canvas->populateRegion(*r);
                        scheduleSave();
                    });
                    classes->addWidget(b);
                }
                l->addLayout(classes);
            }
            auto* popNote = new QLabel(tr("Desligar para de povoar, mas não tira o que já foi povoado. Ctrl+Z desfaz."), m_dockBody);
            popNote->setObjectName(QStringLiteral("cartoNote"));
            popNote->setWordWrap(true);
            l->addWidget(popNote);
        }
    }
    m_dockBody->show();
    placeDock();
}

// ── nomes ──
void Panel::openNameEdit(double sx, double sy, const QString& value, const std::function<void(const QString&)>& fn)
{
    m_nameFn = fn;
    m_nameEdit->setText(value);
    m_nameEdit->move(std::max(8, std::min(width() - m_nameEdit->width() - 8, int(sx) - m_nameEdit->width() / 2)), int(sy) - m_nameEdit->height() / 2);
    m_nameEdit->show();
    m_nameEdit->raise();
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}
void Panel::nameClick(double mx, double my, const QString& key)
{
    Canvas& C = *m_canvas;
    if (key.startsWith(QLatin1Char('L'))) {
        for (FreeLabel& l : m_doc.labels) if (l.key == key) {
            C.nameSel = key;
            const QString k = key;
            openNameEdit(mx, my, l.text, [this, k](const QString& v) { if (v.isEmpty()) return; for (FreeLabel& q : m_doc.labels) if (q.key == k) q.text = v; });
            return;
        }
    }
    const Ref o = m_doc.resolve(key);
    if (o && o.kind != RefKind::Galaxy) {
        C.nameSel = key;
        openNameEdit(mx, my, m_doc.labelOf(key), [this, key](const QString& v) {
            const Ref q = m_doc.resolve(key);
            if (q.kind == RefKind::Region) { if (!v.isEmpty()) q.region->name = v; }
            else if (q.kind == RefKind::Group) { if (!v.isEmpty()) q.group->name = v; }
            else if (!v.isEmpty() && v != m_doc.genName(key)) m_doc.userNames.insert(key, v);
            else m_doc.userNames.remove(key);
            if (q.kind == RefKind::Region) m_canvas->docChanged();
        });
        return;
    }
    double wx, wy;
    C.toWorld(mx, my, wx, wy);
    C.nameSel.clear();
    openNameEdit(mx, my, QString(), [this, wx, wy](const QString& v) {
        if (v.isEmpty()) return;
        const FreeLabel l { QStringLiteral("L%1").arg(m_doc.seq++), wx, wy, v };
        m_doc.labels.push_back(l);
        m_canvas->nameSel = l.key;
        Doc* d = &m_doc;
        const QString k = l.key;
        const auto drop = [d, k] { d->labels.erase(std::remove_if(d->labels.begin(), d->labels.end(), [&](const FreeLabel& q) { return q.key == k; }), d->labels.end()); };
        m_canvas->pushUndo(tr("%1 no mapa.").arg(v), drop, [d, l] { d->labels.push_back(l); });
    });
}
void Panel::deleteName(const QString& k)
{
    if (k.startsWith(QLatin1Char('L'))) {
        for (size_t i = 0; i < m_doc.labels.size(); i++) if (m_doc.labels[i].key == k) {
            const FreeLabel l = m_doc.labels[i];
            m_doc.labels.erase(m_doc.labels.begin() + long(i));
            m_canvas->nameSel.clear();
            Doc* d = &m_doc;
            m_canvas->pushUndo(tr("%1 saiu do mapa.").arg(l.text), [d, l] { d->labels.push_back(l); },
                [d, k] { d->labels.erase(std::remove_if(d->labels.begin(), d->labels.end(), [&](const FreeLabel& q) { return q.key == k; }), d->labels.end()); });
            break;
        }
    } else if (m_doc.hidden.contains(k)) m_doc.hidden.remove(k);
    else m_doc.hidden.insert(k);
    scheduleSave();
    m_canvas->update();
}

} // namespace galaxy
