// Conexões e personagens: como no Mapa, um lugar da galáxia que precisa disso vira um Lugar do Criador de Mundos.
// A conexão é o vínculo entre dois Lugares; a "gente daqui" é a Origem e o Local atual da ficha. Tudo mora no projeto.

#include "GalaxyPanel.h"
#include "GalaxyCaderno.h"

#include "../AvatarUtils.h"
#include "../ElementsStore.h"
#include "../ProjectModel.h"
#include "../TerritorioStore.h"
#include "../Theme.h"
#include "../carto/CartoIcons.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace galaxy {

namespace {
QLabel* textOf(const QString& text, const QString& obj, QWidget* parent, bool wrap = false)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(obj);
    l->setWordWrap(wrap);
    return l;
}
QPushButton* linkBtn(const QString& text, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(QStringLiteral("gmLink"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}
QPixmap dotPixmap(const QColor& c, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(QRectF(1, 1, size - 2, size - 2));
    return pm;
}
// Uma linha do painel: figura, nome e o que está à direita ou embaixo.
QPushButton* rowOf(const QPixmap& pic, const QString& name, const QString& sub, QWidget* parent)
{
    auto* b = new QPushButton(parent);
    b->setObjectName(QStringLiteral("gmItem"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setFixedHeight(46);
    auto* h = new QHBoxLayout(b);
    h->setContentsMargins(8, 4, 8, 4);
    h->setSpacing(10);
    auto* holder = new QLabel(b);
    holder->setFixedSize(30, 30);
    holder->setAlignment(Qt::AlignCenter);
    holder->setPixmap(pic);
    holder->setAttribute(Qt::WA_TransparentForMouseEvents);
    h->addWidget(holder);
    auto* col = new QVBoxLayout();
    col->setSpacing(0);
    const int room = 250;
    auto* n = textOf(QString(), QStringLiteral("cartoValue"), b);
    n->setText(n->fontMetrics().elidedText(name, Qt::ElideRight, room));
    n->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* s = textOf(QString(), QStringLiteral("cartoNote"), b);
    s->setText(s->fontMetrics().elidedText(sub, Qt::ElideRight, room));
    s->setAttribute(Qt::WA_TransparentForMouseEvents);
    col->addWidget(n);
    col->addWidget(s);
    h->addLayout(col, 1);
    return b;
}
}

// ── o Lugar de um pedaço da galáxia ──
QString Panel::placeOfKey(const QString& k, bool create)
{
    if (!m_link.places) return QString();
    const QString have = m_doc.placeIds.value(k);
    if (!have.isEmpty() && m_link.places->territorio(have)) return have;
    const QString name = m_doc.labelOf(k).trimmed();
    for (const auto& t : m_link.places->territorios())
        if (t.name.trimmed().compare(name, Qt::CaseInsensitive) == 0) {
            m_doc.placeIds.insert(k, t.id);
            scheduleSave();
            return t.id;
        }
    if (!create) return QString();
    const QString id = m_link.places->addTerritorio(name);
    m_doc.placeIds.insert(k, id);
    m_link.places->save();
    scheduleSave();
    return id;
}
QString Panel::keyOfPlace(const QString& placeId)
{
    for (auto it = m_doc.placeIds.constBegin(); it != m_doc.placeIds.constEnd(); ++it) if (it.value() == placeId && m_doc.resolve(it.key())) return it.key();
    // Um Lugar com o mesmo nome de um território ou de um sistema batizado é ele, como no Mapa.
    const auto* t = m_link.places ? m_link.places->territorio(placeId) : nullptr;
    if (!t) return QString();
    const QString name = t->name.trimmed();
    const auto same = [&](const QString& other) { return other.trimmed().compare(name, Qt::CaseInsensitive) == 0; };
    for (const auto& r : m_doc.regions) if (same(r->name)) { m_doc.placeIds.insert(r->key, placeId); return r->key; }
    for (auto it = m_doc.userNames.constBegin(); it != m_doc.userNames.constEnd(); ++it)
        if (it.key().startsWith(QLatin1Char('S')) && same(it.value())) { m_doc.placeIds.insert(it.key(), placeId); return it.key(); }
    return QString();
}
std::vector<Panel::GLink> Panel::linksOf(const QString& k, int* offMap)
{
    std::vector<GLink> out;
    if (offMap) *offMap = 0;
    if (!m_link.places) return out;
    const Ref o = m_doc.resolve(k);
    const QColor accent = Theme::toColor(Theme::accentDefault());
    const auto here = [&](const QString& x) {
        if (x.isEmpty()) return false;
        if (x == k || o.kind == RefKind::Galaxy) return true;
        if (o.kind == RefKind::Region) { const Ref q = m_doc.resolve(x); return q.kind == RefKind::System && o.region->systems.contains(q.sys->id); }
        return false;
    };
    for (const auto& l : m_link.places->links()) {
        const QString a = keyOfPlace(l.fromTerritorioId), b = keyOfPlace(l.toTerritorioId);
        const bool ha = here(a), hb = here(b);
        if (!ha && !hb) continue;
        if (a.isEmpty() || b.isEmpty()) { if (offMap) (*offMap)++; continue; }
        GLink x;
        x.id = l.id;
        x.a = ha ? a : b; x.b = ha ? b : a;
        x.label = linkLabel(l.id);
        x.color = QColor(l.color).isValid() ? QColor(l.color) : accent;
        out.push_back(x);
    }
    return out;
}
// a conexão não tem nome próprio: a primeira linha do texto dela, ou "A e B"
QString Panel::linkLabel(const QString& linkId) const
{
    const auto* l = m_link.places ? m_link.places->link(linkId) : nullptr;
    if (!l) return QString();
    const QString text = QTextDocumentFragment::fromHtml(l->docContent).toPlainText();
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        const QString s = line.trimmed();
        if (!s.isEmpty()) return s.length() > 42 ? s.left(40) + QStringLiteral("…") : s;
    }
    const auto* a = m_link.places->territorio(l->fromTerritorioId);
    const auto* b = m_link.places->territorio(l->toTerritorioId);
    return tr("%1 e %2").arg(a ? a->name : QString(), b ? b->name : QString());
}
// Os arcos no mapa: os do lugar aberto, ou só o do lado a lado.
void Panel::syncLinkView()
{
    std::vector<LinkView> v;
    if (isOpen() && m_link.places) {
        if (!m_duoLink.isEmpty()) {
            const auto* l = m_link.places->link(m_duoLink);
            if (l) v.push_back({ m_duoLink, m_duoA, m_duoB, linkLabel(m_duoLink), QColor(l->color).isValid() ? QColor(l->color) : Theme::toColor(Theme::accentDefault()), true });
        } else if (panelShown() && !m_openDoc) {
            const RefKind kind = m_doc.resolve(m_canvas->sel).kind;
            if (kind == RefKind::Galaxy || kind == RefKind::Region || kind == RefKind::System)
                for (const GLink& x : linksOf(m_canvas->sel)) v.push_back({ x.id, x.a, x.b, x.label, x.color, false });
        }
    }
    m_canvas->setLinks(std::move(v));
}
void Panel::refreshPlaceUi()
{
    QTimer::singleShot(0, this, [this]() {
        renderPanel(true);
        if (m_caderno && m_caderno->isVisible()) m_caderno->refresh();
    });
}

// ── a seção de conexões ──
void Panel::renderLinksSection(QVBoxLayout* body, const QString& k)
{
    QWidget* parent = body->parentWidget();
    if (!m_link.places) return;
    const Ref o = m_doc.resolve(k);
    int off = 0;
    const std::vector<GLink> links = linksOf(k, &off);
    if (o.kind == RefKind::Galaxy && links.empty() && !off) return;
    const QString name = m_doc.labelOf(k);
    if (m_linkFrom == k) {
        body->addWidget(textOf(tr("Ligar %1 a…").arg(name).toUpper(), QStringLiteral("gmEyebrow"), parent));
        const bool map = !(m_caderno && m_caderno->isVisible());
        if (map) body->addWidget(textOf(tr("Clique num território ou num sistema no mapa, ou escolha aqui."), QStringLiteral("cartoNote"), parent, true));
        auto* q = new QLineEdit(parent);
        q->setObjectName(QStringLiteral("gmNameEdit"));
        q->setPlaceholderText(tr("Buscar território ou sistema"));
        body->addWidget(q);
        auto* list = new QWidget(parent);
        auto* ll = new QVBoxLayout(list);
        ll->setContentsMargins(0, 0, 0, 0);
        ll->setSpacing(2);
        body->addWidget(list);
        const auto fill = [this, ll, list, k, links](const QString& text) {
            while (QLayoutItem* it = ll->takeAt(0)) { if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); } delete it; }
            QStringList keys;
            for (const auto& r : m_doc.regions) keys << r->key;
            const QString needle = text.trimmed();
            for (const auto& s : m_doc.gal->systems) {
                if (s->gone) continue;
                const QString sk = keyS(*s);
                if (needle.isEmpty() ? m_doc.touched(sk) : true) keys << sk;
            }
            int shown = 0;
            for (const QString& x : keys) {
                if (x == k || shown >= 30) continue;
                bool linked = false;
                for (const GLink& l : links) if (l.a == x || l.b == x) linked = true;
                if (linked) continue;
                if (!needle.isEmpty() && !m_doc.labelOf(x).contains(needle, Qt::CaseInsensitive)) continue;
                QPushButton* row = itemRow(x, subShort(x, m_doc.resolve(x)), QString());
                row->setParent(list);
                disconnect(row, &QPushButton::clicked, nullptr, nullptr);
                connect(row, &QPushButton::clicked, this, [this, k, x]() { QTimer::singleShot(0, this, [this, k, x]() { connectPlaces(k, x); }); });
                ll->addWidget(row);
                shown++;
            }
            if (!shown) ll->addWidget(textOf(needle.isEmpty() ? tr("Nenhum território ou sistema com nome ainda. Use a busca.") : tr("Nada com esse nome."), QStringLiteral("cartoNote"), list, true));
        };
        fill(QString());
        connect(q, &QLineEdit::textChanged, list, fill);
        auto* c = linkBtn(tr("Cancelar"), parent);
        connect(c, &QPushButton::clicked, this, [this]() { m_linkFrom.clear(); m_canvas->pickingLink = false; refreshPlaceUi(); });
        body->addWidget(c, 0, Qt::AlignLeft);
        return;
    }
    auto* h = new QHBoxLayout();
    h->addWidget(textOf(tr("Conexões").toUpper(), QStringLiteral("gmEyebrow"), parent));
    h->addWidget(textOf(QString::number(links.size()), QStringLiteral("cartoNote"), parent));
    h->addStretch();
    body->addLayout(h);
    for (const GLink& x : links) {
        QPushButton* row = rowOf(dotPixmap(x.color, 14), x.label, o.kind == RefKind::Galaxy || x.a != k ? tr("%1 com %2").arg(m_doc.labelOf(x.a), m_doc.labelOf(x.b)) : tr("com %1").arg(m_doc.labelOf(x.b)), parent);
        const QString id = x.id, from = x.a;
        connect(row, &QPushButton::clicked, this, [this, id, from]() { QTimer::singleShot(0, this, [this, id, from]() { openDuo(id, from); }); });
        body->addWidget(row);
    }
    if (off) body->addWidget(textOf(tr("%n conexão(ões) com lugares fora deste mapa", "", off), QStringLiteral("cartoNote"), parent, true));
    if (o.kind == RefKind::Galaxy) return;
    auto* nb = linkBtn(tr("+ Nova conexão…"), parent);
    connect(nb, &QPushButton::clicked, this, [this, k]() {
        m_linkFrom = k;
        const bool map = !(m_caderno && m_caderno->isVisible());
        m_canvas->pickingLink = map;
        // pra escolher no mapa, a galáxia inteira à vista e sem o foco borrando o resto
        if (map) { m_canvas->setFocus2(QString()); m_canvas->fitMap(); }
        refreshPlaceUi();
    });
    body->addWidget(nb, 0, Qt::AlignLeft);
}
void Panel::linkPicked(const QString& key)
{
    if (m_linkFrom.isEmpty()) return;
    const Ref o = m_doc.resolve(key);
    QString to;
    if (o.kind == RefKind::Region || o.kind == RefKind::System) to = key;
    else if (o.sys) to = keyS(*o.sys);   // planeta, lua, peça: a conexão fica no sistema
    if (to.isEmpty() || to == m_linkFrom) return;
    const QString from = m_linkFrom;   // connectPlaces limpa o m_linkFrom
    connectPlaces(from, to);
}
void Panel::connectPlaces(const QString& a, const QString& b)
{
    m_linkFrom.clear();
    m_canvas->pickingLink = false;
    const QString pa = placeOfKey(a, true), pb = placeOfKey(b, true);
    if (pa.isEmpty() || pb.isEmpty() || pa == pb) { refreshPlaceUi(); return; }
    QString id;
    if (const auto* l = m_link.places->linkBetween(pa, pb)) id = l->id;
    if (id.isEmpty()) {
        const Ref o = m_doc.resolve(a);
        QColor c = Theme::toColor(Theme::accentDefault());
        if (o.kind == RefKind::Region) c = QColor(o.region->color);
        else if (const Region* r = o.sys ? m_doc.regionOfSys(*o.sys) : nullptr) c = QColor(r->color);
        id = m_link.places->addLink(pa, pb, c.name());
        m_link.places->save();
    }
    if (m_caderno && m_caderno->isVisible()) closeCaderno();
    openDuo(id, a);
    if (m_duoLink != id) return;
    m_duoEdit = 1;
    renderDuo();
    m_dcol[1].body->setFocus();
}

// ── personagens daqui ──
void Panel::renderPeopleSection(QVBoxLayout* body, const QString& k)
{
    QWidget* parent = body->parentWidget();
    if (!m_link.model || !m_link.places) return;
    const auto avatarOf = [this](const DrawerItem& it) {
        const Element* e = !it.elementId.isEmpty() && m_link.elements ? m_link.elements->findElement(it.elementId) : nullptr;
        return AvatarUtils::circularAvatar(e ? e->image : QString(), e ? e->name : it.title, e ? e->id : it.id, 30);
    };
    const QString own = placeOfKey(k, false);
    if (m_peopleFor == k) {
        body->addWidget(textOf(tr("Personagens daqui").toUpper(), QStringLiteral("gmEyebrow"), parent));
        auto* seg = new QHBoxLayout();
        seg->setSpacing(4);
        const auto pill = [&](const QString& text, bool born) {
            auto* b = new QToolButton(parent);
            b->setObjectName(QStringLiteral("cartoSeg"));
            b->setText(text);
            b->setCheckable(true);
            b->setChecked(m_peopleBorn == born);
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            connect(b, &QToolButton::clicked, this, [this, born]() { m_peopleBorn = born; refreshPlaceUi(); });
            seg->addWidget(b);
        };
        pill(tr("Mora aqui"), false);
        pill(tr("Nasceu aqui"), true);
        seg->addStretch();
        body->addLayout(seg);
        body->addWidget(textOf(tr("A ficha guarda uma Origem e um \"Está em\": escolher aqui troca o que estava nela."), QStringLiteral("cartoNote"), parent, true));
        auto* q = new QLineEdit(parent);
        q->setObjectName(QStringLiteral("gmNameEdit"));
        q->setPlaceholderText(tr("Buscar personagem"));
        body->addWidget(q);
        auto* list = new QWidget(parent);
        auto* ll = new QVBoxLayout(list);
        ll->setContentsMargins(0, 0, 0, 0);
        ll->setSpacing(2);
        body->addWidget(list);
        const auto fill = [this, ll, list, k, avatarOf](const QString& text) {
            while (QLayoutItem* it = ll->takeAt(0)) { if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); } delete it; }
            const QString needle = text.trimmed();
            const QString pid = m_doc.placeIds.value(k);
            int shown = 0, total = 0;
            for (const Drawer& dr : m_link.model->drawers())
                for (const DrawerItem& it : dr.items) {
                    if (it.isChapter || (!it.isSheet && it.elementId.isEmpty())) continue;
                    total++;
                    if (shown >= 30 || (!needle.isEmpty() && !it.title.contains(needle, Qt::CaseInsensitive))) continue;
                    if (!pid.isEmpty() && (m_peopleBorn ? it.origemTerritorioId : it.localAtualTerritorioId) == pid) continue;
                    const QString cur = m_peopleBorn ? it.origemTerritorioId : it.localAtualTerritorioId;
                    const auto* t = cur.isEmpty() ? nullptr : m_link.places->territorio(cur);
                    QPushButton* row = rowOf(avatarOf(it), it.title, t ? (m_peopleBorn ? tr("nasceu em %1") : tr("mora em %1")).arg(t->name) : dr.title, list);
                    const QString id = it.id;
                    connect(row, &QPushButton::clicked, this, [this, k, id]() {
                        QTimer::singleShot(0, this, [this, k, id]() {
                            const DrawerItem* item = m_link.model->findDrawerItem(id);
                            const QString place = placeOfKey(k, true);
                            if (!item || place.isEmpty()) return;
                            m_link.model->updateDrawerItemTerritorios(id, m_peopleBorn ? place : item->origemTerritorioId, m_peopleBorn ? item->localAtualTerritorioId : place);
                            if (m_link.save) m_link.save();
                            m_peopleFor.clear();
                            refreshPlaceUi();
                        });
                    });
                    ll->addWidget(row);
                    shown++;
                }
            if (!shown) ll->addWidget(textOf(total ? tr("Ninguém com esse nome.") : tr("Nenhuma ficha de personagem neste projeto ainda."), QStringLiteral("cartoNote"), list, true));
        };
        fill(QString());
        connect(q, &QLineEdit::textChanged, list, fill);
        auto* c = linkBtn(tr("Cancelar"), parent);
        connect(c, &QPushButton::clicked, this, [this]() { m_peopleFor.clear(); refreshPlaceUi(); });
        body->addWidget(c, 0, Qt::AlignLeft);
        return;
    }
    // quem é daqui e quem é dos lugares aqui dentro
    QHash<QString, QString> inside;
    for (auto it = m_doc.placeIds.constBegin(); it != m_doc.placeIds.constEnd(); ++it)
        if (it.key() != k && m_doc.resolve(it.key()) && m_doc.pathOf(it.key()).contains(k)) inside.insert(it.value(), it.key());
    struct Person { const DrawerItem* it; QString sub; bool mine; };
    std::vector<Person> people;
    for (const Drawer& dr : m_link.model->drawers())
        for (const DrawerItem& it : dr.items) {
            const bool born = !own.isEmpty() && it.origemTerritorioId == own, lives = !own.isEmpty() && it.localAtualTerritorioId == own;
            if (born || lives) { people.push_back({ &it, born && lives ? tr("nasceu e mora aqui") : born ? tr("nasceu aqui") : tr("mora aqui"), true }); continue; }
            if (inside.contains(it.localAtualTerritorioId)) people.push_back({ &it, tr("mora em %1").arg(m_doc.labelOf(inside.value(it.localAtualTerritorioId))), false });
            else if (inside.contains(it.origemTerritorioId)) people.push_back({ &it, tr("nasceu em %1").arg(m_doc.labelOf(inside.value(it.origemTerritorioId))), false });
        }
    auto* h = new QHBoxLayout();
    h->addWidget(textOf(tr("Personagens daqui").toUpper(), QStringLiteral("gmEyebrow"), parent));
    h->addWidget(textOf(QString::number(people.size()), QStringLiteral("cartoNote"), parent));
    h->addStretch();
    body->addLayout(h);
    for (const Person& x : people) {
        QPushButton* row = rowOf(avatarOf(*x.it), x.it->title, x.sub, parent);
        const QString id = x.it->id;
        connect(row, &QPushButton::clicked, this, [this, id]() { if (m_link.openItem) m_link.openItem(id); });
        if (x.mine) {
            auto* del = new QToolButton(row);
            del->setObjectName(QStringLiteral("gmRound"));
            del->setFixedSize(24, 24);
            del->setStyleSheet(QStringLiteral("QToolButton#gmRound { border-radius: 12px; }"));
            del->setIconSize(QSize(12, 12));
            del->setIcon(carto::icon(QStringLiteral("close"), Theme::toColor(Theme::textMuted()), Theme::toColor(Theme::accentDefault()), 12));
            del->setToolTip(tr("Tirar daqui"));
            del->setCursor(Qt::PointingHandCursor);
            del->setFocusPolicy(Qt::NoFocus);
            connect(del, &QToolButton::clicked, this, [this, id, own]() {
                QTimer::singleShot(0, this, [this, id, own]() {
                    const DrawerItem* item = m_link.model->findDrawerItem(id);
                    if (!item) return;
                    m_link.model->updateDrawerItemTerritorios(id, item->origemTerritorioId == own ? QString() : item->origemTerritorioId, item->localAtualTerritorioId == own ? QString() : item->localAtualTerritorioId);
                    if (m_link.save) m_link.save();
                    refreshPlaceUi();
                });
            });
            static_cast<QHBoxLayout*>(row->layout())->addWidget(del);
        }
        body->addWidget(row);
    }
    auto* add = linkBtn(tr("+ Personagem"), parent);
    connect(add, &QPushButton::clicked, this, [this, k]() { m_peopleFor = k; refreshPlaceUi(); });
    body->addWidget(add, 0, Qt::AlignLeft);
}

// ── lado a lado: um lado · a conexão · o outro lado ──
void Panel::buildDuo()
{
    if (m_duo) return;
    const QColor ink = Theme::toColor(Theme::textPrimary()), accent = Theme::toColor(Theme::accentDefault());
    m_duo = new QFrame(this);
    m_duo->setObjectName(QStringLiteral("cartoCard"));
    auto* l = new QVBoxLayout(m_duo);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    auto* head = new QHBoxLayout();
    head->setContentsMargins(12, 10, 10, 10);
    head->setSpacing(10);
    auto* back = linkBtn(QStringLiteral("‹ %1").arg(tr("Voltar")), m_duo);
    connect(back, &QPushButton::clicked, this, [this]() { closeDuo(); });
    head->addWidget(back);
    m_duoTitle = new QLabel(m_duo);
    m_duoTitle->setObjectName(QStringLiteral("xDuoTitle"));
    m_duoTitle->setTextFormat(Qt::RichText);
    head->addWidget(m_duoTitle, 1);
    auto* swap = linkBtn(tr("Trocar lados"), m_duo);
    connect(swap, &QPushButton::clicked, this, [this]() {
        flushDuo();
        std::swap(m_duoA, m_duoB);
        std::swap(m_dcol[0].doc, m_dcol[2].doc);
        std::swap(m_dcol[0].about, m_dcol[2].about);
        m_duoEdit = -1;
        renderDuo();
    });
    head->addWidget(swap);
    auto* x = new QToolButton(m_duo);
    x->setObjectName(QStringLiteral("gmRound"));
    x->setFixedSize(32, 32);
    x->setIconSize(QSize(16, 16));
    x->setIcon(carto::icon(QStringLiteral("close"), ink, accent, 16));
    x->setToolTip(tr("Fechar"));
    x->setCursor(Qt::PointingHandCursor);
    x->setFocusPolicy(Qt::NoFocus);
    connect(x, &QToolButton::clicked, this, [this]() { closeDuo(); });
    head->addWidget(x);
    l->addLayout(head);
    auto* sep = new QFrame(m_duo);
    sep->setObjectName(QStringLiteral("xSep"));
    sep->setFixedHeight(1);
    l->addWidget(sep);
    auto* cols = new QHBoxLayout();
    cols->setContentsMargins(0, 0, 0, 0);
    cols->setSpacing(0);
    for (int i = 0; i < 3; i++) {
        auto* c = new QFrame(m_duo);
        c->setObjectName(i == 1 ? QStringLiteral("xDuoMid") : QStringLiteral("xDuoCol"));
        auto* cl = new QVBoxLayout(c);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(0);
        auto* ch = new QHBoxLayout();
        ch->setContentsMargins(16, 10, 10, 4);
        ch->setSpacing(6);
        DuoCol& col = m_dcol[i];
        col.chip = new QLabel(c);
        col.chip->setObjectName(QStringLiteral("xChip"));
        col.chip->setTextFormat(Qt::RichText);
        ch->addWidget(col.chip, 1);
        if (i != 1) {
            col.pick = new QComboBox(c);
            col.pick->setObjectName(QStringLiteral("cartoCombo"));
            col.pick->setMaximumWidth(190);
            connect(col.pick, &QComboBox::activated, this, [this, i](int idx) {
                flushDuo();
                const auto docs = duoDocs(i == 0 ? m_duoA : m_duoB);
                if (idx >= 0 && idx < int(docs.size())) { m_dcol[i].doc = docs[size_t(idx)].first; m_dcol[i].about = !docs[size_t(idx)].first; }
                if (m_duoEdit == i) m_duoEdit = -1;
                renderDuo();
            });
            ch->addWidget(col.pick);
        }
        col.pen = new QToolButton(c);
        col.pen->setObjectName(QStringLiteral("gmRound"));
        col.pen->setFixedSize(28, 28);
        col.pen->setStyleSheet(QStringLiteral("QToolButton#gmRound { border-radius: 14px; }"));
        col.pen->setIconSize(QSize(14, 14));
        col.pen->setIcon(carto::icon(QStringLiteral("pen"), ink, accent, 14));
        col.pen->setCheckable(true);
        col.pen->setCursor(Qt::PointingHandCursor);
        col.pen->setFocusPolicy(Qt::NoFocus);
        connect(col.pen, &QToolButton::clicked, this, [this, i]() {
            flushDuo();
            m_duoEdit = m_duoEdit == i ? -1 : i;
            renderDuo();
            if (m_duoEdit == i) m_dcol[i].body->setFocus();
        });
        ch->addWidget(col.pen);
        cl->addLayout(ch);
        col.title = new QLabel(c);
        col.title->setObjectName(QStringLiteral("xColTitle"));
        col.title->setWordWrap(true);
        cl->addWidget(col.title);
        col.body = new QTextEdit(c);
        col.body->setObjectName(QStringLiteral("xBody"));
        col.body->setFrameShape(QFrame::NoFrame);
        col.body->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        col.body->document()->setDocumentMargin(14);
        connect(col.body, &QTextEdit::textChanged, this, [this, i]() { if (!m_duoLoading && m_duoEdit == i) m_duoTimer->start(); });
        cl->addWidget(col.body, 1);
        cols->addWidget(c, 1);
    }
    l->addLayout(cols, 1);
    m_duoTimer = new QTimer(this);
    m_duoTimer->setSingleShot(true);
    m_duoTimer->setInterval(600);
    connect(m_duoTimer, &QTimer::timeout, this, [this]() { writeDuo(); });
    m_duo->hide();
}
// Os documentos de um lado: os dele, os dos lugares de dentro e o "Sobre" do Lugar (nullptr).
std::vector<std::pair<DocLink*, QString>> Panel::duoDocs(const QString& k)
{
    std::vector<std::pair<DocLink*, QString>> out;
    for (DocLink* d : m_doc.docsOf(k)) out.push_back({ d, d->title });
    for (const auto& x : docsBelow(k)) for (DocLink* d : x.second) out.push_back({ d, d->title });
    out.push_back({ nullptr, tr("Sobre %1").arg(m_doc.labelOf(k)) });
    return out;
}
void Panel::openDuo(const QString& linkId, const QString& from)
{
    const auto* l = m_link.places ? m_link.places->link(linkId) : nullptr;
    if (!l) return;
    const QString a = keyOfPlace(l->fromTerritorioId), b = keyOfPlace(l->toTerritorioId);
    if (a.isEmpty() || b.isEmpty()) return;
    buildDuo();
    flushDuo();
    m_linkFrom.clear();
    m_canvas->pickingLink = false;
    hidePeek();
    m_duoLink = linkId;
    m_duoA = from == b ? b : a;
    m_duoB = m_duoA == a ? b : a;
    m_duoEdit = -1;
    // de que documento cada lado começa: o Sobre com texto, o primeiro documento, ou o Sobre vazio
    const auto start = [this](int i, const QString& k) {
        const QString pid = placeOfKey(k, true);
        const auto* t = m_link.places->territorio(pid);
        const auto docs = duoDocs(k);
        DuoCol& c = m_dcol[i];
        c.about = (t && !t->content.trimmed().isEmpty()) || docs.size() == 1;
        c.doc = c.about ? nullptr : docs.front().first;
    };
    start(0, m_duoA);
    start(2, m_duoB);
    if (m_openDoc) { docDropIfEmpty(m_openDoc); m_openDoc = nullptr; }
    m_canvas->setFocus2(QString());
    applyDuoTheme();
    renderDuo();
    m_duo->show();
    m_duo->raise();
    positionOverlays();
    renderPanel();
    // a câmera põe os dois lados na faixa livre acima do painel
    m_canvas->frameLink(m_duoA, m_duoB, 70, m_canvas->mapFrom(this, QPoint(0, m_duo->y())).y());
}
void Panel::applyDuoTheme()
{
    if (!m_duo) return;
    const QColor a = Theme::toColor(Theme::accentDefault());
    const QString soft = QStringLiteral("rgba(%1,%2,%3,0.18)").arg(a.red()).arg(a.green()).arg(a.blue());
    m_duo->setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#xSep { background: %1; border: none; }"
        "QLabel#xChip { color: %4; font-size: 11px; font-weight: 600; letter-spacing: 1px; background: transparent; }"
        "QTextEdit#xBody { background: transparent; color: %2; selection-background-color: %5; padding: 0px; margin: 0px; border: none; }"
        "QTextEdit#xBody[editing=\"true\"] { background: %3; border-radius: 8px; }"
        "QLabel#xDuoTitle { color: %2; font-family: 'IM FELL English SC', Georgia, serif; font-size: 16px; letter-spacing: 1px; background: transparent; }"
        "QFrame#xDuoCol { background: transparent; border: none; border-right: 1px solid %1; }"
        "QFrame#xDuoMid { background: %3; border: none; border-right: 1px solid %1; }"
        "QLabel#xColTitle { color: %2; font-family: Georgia, serif; font-size: 17px; font-weight: 600; padding: 0 16px 4px; background: transparent; }"
    ).arg(Theme::panelBorder(), Theme::textPrimary(), Theme::hoverOverlay(), Theme::textMuted(), soft)));
}
void Panel::renderDuo()
{
    if (m_duoLink.isEmpty() || !m_duo) return;
    const auto* l = m_link.places->link(m_duoLink);
    if (!l) { closeDuo(); return; }
    const QColor accent = Theme::toColor(Theme::accentDefault());
    const QColor lc = QColor(l->color).isValid() ? QColor(l->color) : accent;
    const QString na = m_doc.labelOf(m_duoA), nb = m_doc.labelOf(m_duoB);
    m_duoTitle->setText(QStringLiteral("%1 <span style=\"color:%3\">⟷</span> %2").arg(na.toHtmlEscaped(), nb.toHtmlEscaped(), accent.name()));
    m_duoLoading = true;
    const auto load = [this](QTextEdit* ed, const QString& html, bool editing) {
        ed->setReadOnly(!editing);
        ed->setProperty("editing", editing);
        ed->style()->unpolish(ed);
        ed->style()->polish(ed);
        ed->setHtml(html);
    };
    for (int i = 0; i < 3; i++) {
        DuoCol& c = m_dcol[i];
        const bool editing = m_duoEdit == i;
        c.pen->setChecked(editing);
        c.pen->setToolTip(editing ? tr("Concluir") : tr("Editar"));
        if (i == 1) {
            c.chip->setText(QStringLiteral("<span style=\"color:%1\">●</span>&nbsp;%2").arg(lc.name(), tr("CONEXÃO")));
            c.title->setText(tr("%1 e %2").arg(na, nb));
            c.body->setPlaceholderText(tr("Como esses dois lugares se ligam? Guerra, aliança, comércio, uma família dividida…"));
            load(c.body, l->docContent, editing);
            continue;
        }
        const QString k = i == 0 ? m_duoA : m_duoB;
        const Ref o = m_doc.resolve(k);
        const QColor dot = o.kind == RefKind::Region ? QColor(o.region->color) : lc;
        c.chip->setText(QStringLiteral("<span style=\"color:%1\">●</span>&nbsp;%2").arg(dot.name(), m_doc.labelOf(k).toUpper().toHtmlEscaped()));
        const auto docs = duoDocs(k);
        c.pick->blockSignals(true);
        c.pick->clear();
        int cur = -1;
        for (int d = 0; d < int(docs.size()); d++) {
            c.pick->addItem(docs[size_t(d)].second);
            if (c.about ? !docs[size_t(d)].first : docs[size_t(d)].first == c.doc) cur = d;
        }
        c.pick->setCurrentIndex(cur);
        c.pick->setVisible(docs.size() > 1);
        c.pick->blockSignals(false);
        if (c.about) {
            const auto* t = m_link.places->territorio(placeOfKey(k, true));
            c.title->setText(tr("Sobre %1").arg(m_doc.labelOf(k)));
            c.body->setPlaceholderText(tr("Comece a escrever sobre %1…").arg(m_doc.labelOf(k)));
            load(c.body, t ? t->content : QString(), editing);
        } else if (c.doc) {
            c.title->setText(c.doc->title);
            c.body->setPlaceholderText(QString());
            load(c.body, docHtml(c.doc), editing);
        }
    }
    m_duoLoading = false;
}
void Panel::writeDuo()
{
    const int i = m_duoEdit;
    if (i < 0 || m_duoLink.isEmpty()) return;
    QTextEdit* ed = m_dcol[i].body;
    const bool empty = ed->toPlainText().trimmed().isEmpty() && !ed->toHtml().contains(QStringLiteral("<img"));
    const QString html = empty ? QString() : ed->toHtml();
    if (i == 1) m_link.places->updateLinkContent(m_duoLink, html);
    else if (m_dcol[i].about) m_link.places->updateTerritorioContent(placeOfKey(i == 0 ? m_duoA : m_duoB, true), html);
    else if (m_dcol[i].doc) { docWriteHtml(m_dcol[i].doc, html); return; }
    m_link.places->save();
    syncLinkView();
}
void Panel::flushDuo()
{
    if (m_duoTimer && m_duoTimer->isActive()) { m_duoTimer->stop(); writeDuo(); }
}
void Panel::closeDuo()
{
    if (m_duoLink.isEmpty()) return;
    flushDuo();
    m_duoLink.clear();
    m_duoEdit = -1;
    if (m_duo) m_duo->hide();
    renderPanel(true);
    positionOverlays();
    syncTitle();
}
void Panel::placeDuo()
{
    if (!m_duo || m_duoLink.isEmpty()) return;
    const int W = width(), H = height();
    const int w = std::min(1180, W - 32), h = std::min(520, std::max(340, H / 2));
    m_duo->setFixedSize(w, h);
    m_duo->move((W - w) / 2, H - h - 56);
    m_duo->raise();
}

} // namespace galaxy
