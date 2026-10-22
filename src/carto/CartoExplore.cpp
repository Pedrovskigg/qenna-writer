#include "CartoExplore.h"

#include "CartoCanvas.h"
#include "CartoDoc.h"
#include "CartoIcons.h"

#include "../AvatarUtils.h"
#include "../DocCache.h"
#include "../DocPreview.h"
#include "../ElementsStore.h"
#include "../ProjectModel.h"
#include "../TerritorioStore.h"
#include "../Theme.h"

#include <QAbstractButton>
#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QComboBox>
#include <QMouseEvent>
#include <QStyle>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QTextFrame>
#include <QTextList>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>

namespace carto {

namespace {
constexpr int kCardW = 316, kReaderW = 360, kReaderH = 480, kTop = 86;

QString rgbHex(const RGB& c) { return QColor(static_cast<int>(c[0]), static_cast<int>(c[1]), static_cast<int>(c[2])).name(); }

// os documentos de um Lugar: na raiz primeiro, depois as pastas (caminho "A / B")
void flattenNodes(const QList<TerritorioStore::Node>& nodes, const QString& path, const QString& folderId,
                  QVector<Explorer::XDoc>& out, const QString& placeId)
{
    for (const auto& n : nodes) if (n.type == TerritorioStore::NodeType::Doc) {
        Explorer::XDoc d;
        d.kind = Explorer::XDoc::PlaceNode;
        d.id = n.id; d.title = n.name; d.folder = path; d.folderId = folderId; d.drawerKey = placeId;
        out.push_back(d);
    }
    for (const auto& n : nodes) if (n.type == TerritorioStore::NodeType::Folder)
        flattenNodes(n.children, path.isEmpty() ? n.name : path + QStringLiteral(" / ") + n.name, n.id, out, placeId);
}
const TerritorioStore::Node* findNode(const QList<TerritorioStore::Node>& nodes, const QString& id)
{
    for (const auto& n : nodes) {
        if (n.id == id) return &n;
        if (const auto* c = findNode(n.children, id)) return c;
    }
    return nullptr;
}
void collectFolders(const QList<TerritorioStore::Node>& nodes, const QString& path, QVector<QPair<QString, QString>>& out)
{
    for (const auto& n : nodes) if (n.type == TerritorioStore::NodeType::Folder) {
        const QString p = path.isEmpty() ? n.name : path + QStringLiteral(" / ") + n.name;
        out.push_back({ n.id, p });
        collectFolders(n.children, p, out);
    }
}
QString agoText(qint64 ms)
{
    if (ms <= 0) return QString();
    const qint64 m = std::max<qint64>(0, (QDateTime::currentMSecsSinceEpoch() - ms) / 60000);
    if (m < 1) return Explorer::tr("agora");
    if (m < 60) return Explorer::tr("%1 min").arg(m);
    if (m < 60 * 24) return Explorer::tr("%1 h").arg(m / 60);
    return Explorer::tr("%1 d").arg(m / (60 * 24));
}
QToolButton* iconBtn(QWidget* parent, const QString& tip)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(QStringLiteral("xIco"));
    b->setCursor(Qt::PointingHandCursor);
    b->setToolTip(tip);
    b->setIconSize(QSize(16, 16));
    b->setFixedSize(28, 28);
    return b;
}
QPushButton* textBtn(QWidget* parent, const QString& text, bool primary = false)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(primary ? QStringLiteral("xPrimary") : QStringLiteral("xBtn"));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
void clearLayout(QLayout* l)
{
    while (QLayoutItem* it = l->takeAt(0)) {
        if (QWidget* w = it->widget()) w->deleteLater();
        else if (QLayout* sub = it->layout()) clearLayout(sub);
        delete it;
    }
}
} // namespace

Explorer::Explorer(QWidget* host, Canvas* canvas, Doc* doc) : QObject(host), m_host(host), m_canvas(canvas), m_doc(doc)
{
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(700);
    connect(m_saveTimer, &QTimer::timeout, this, &Explorer::flushSave);
    m_chip = new QLabel(host);
    m_chip->setObjectName(QStringLiteral("xHover"));
    m_chip->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_chip->hide();
    buildCard();
    buildReader();
    buildDuo();
    applyTheme();
}

void Explorer::setLink(const ProjectLink& link)
{
    if (m_link.model) disconnect(m_link.model, nullptr, this, nullptr);
    if (m_link.places) disconnect(m_link.places, nullptr, this, nullptr);
    m_link = link;
    if (m_link.model) {
        connect(m_link.model, &ProjectModel::drawersChanged, this, [this]() { if (m_active) { pushView(); if (m_card->isVisible() && !m_edit) renderCard(); } });
        connect(m_link.model, &ProjectModel::groupsChanged, this, [this]() { if (m_active) { pushView(); if (m_card->isVisible() && !m_edit) renderCard(); } });
    }
    if (m_link.places)
        connect(m_link.places, &TerritorioStore::changed, this, [this]() {
            if (!m_active || m_saveTimer->isActive() || m_loading) return;
            pushView();
            if (m_card->isVisible() && m_mode == CardMode::List && !m_edit) renderCard();
        });
}

// ── dados ──
int Explorer::realmCount() const { return m_doc && m_doc->info ? static_cast<int>(m_doc->info->defs.size()) : 0; }
QString Explorer::realmName(int r) const { return r >= 0 && r < realmCount() ? m_doc->info->defs[r].name : QString(); }
QColor Explorer::realmColor(int r) const
{
    if (r < 0 || r >= realmCount()) return m_accent;
    const RGB c = m_doc->info->defs[r].color;
    return QColor(static_cast<int>(c[0]), static_cast<int>(c[1]), static_cast<int>(c[2]));
}

// o grupo do território: o ligado, senão um grupo com o mesmo nome; criado quando precisa
QString Explorer::groupOf(int r, bool create)
{
    if (!m_link.model || r < 0 || r >= realmCount()) return QString();
    RealmDef& d = m_doc->info->defs[r];
    if (!d.groupId.isEmpty() && m_link.model->findGroup(d.groupId)) return d.groupId;
    for (const Group& g : m_link.model->groups())
        if (g.title.trimmed().compare(d.name.trimmed(), Qt::CaseInsensitive) == 0) {
            d.groupId = g.id;
            if (mapEdited) mapEdited();
            return g.id;
        }
    if (!create) return QString();
    d.groupId = m_link.model->addGroup(d.name, rgbHex(d.color));
    if (mapEdited) mapEdited();
    if (m_link.save) m_link.save();
    return d.groupId;
}
QString Explorer::placeOf(int r, bool create)
{
    if (!m_link.places || r < 0 || r >= realmCount()) return QString();
    RealmDef& d = m_doc->info->defs[r];
    if (!d.placeId.isEmpty() && m_link.places->territorio(d.placeId)) return d.placeId;
    for (const auto& t : m_link.places->territorios())
        if (t.name.trimmed().compare(d.name.trimmed(), Qt::CaseInsensitive) == 0) {
            d.placeId = t.id;
            if (mapEdited) mapEdited();
            return t.id;
        }
    if (!create) return QString();
    d.placeId = m_link.places->addTerritorio(d.name);
    if (mapEdited) mapEdited();
    return d.placeId;
}

QVector<Explorer::XDoc> Explorer::drawerDocs(int r)
{
    QVector<XDoc> out;
    const QString gid = groupOf(r, false);
    if (gid.isEmpty()) return out;
    for (const Drawer& dr : m_link.model->drawers())
        for (const DrawerItem& it : dr.items) {
            if (it.markerId != gid || it.isChapter) continue;
            XDoc d;
            d.kind = XDoc::Drawer;
            d.id = it.id; d.drawerKey = dr.key; d.drawerTitle = dr.title; d.title = it.title;
            d.color = QColor(dr.color); d.sheet = it.isSheet; d.elementId = it.elementId;
            out.push_back(d);
        }
    return out;
}
QVector<Explorer::XDoc> Explorer::placeDocs(int r)
{
    QVector<XDoc> out;
    const QString pid = placeOf(r, false);
    const auto* t = pid.isEmpty() ? nullptr : m_link.places->territorio(pid);
    if (!t) return out;
    XDoc about;
    about.kind = XDoc::PlaceAbout;
    about.id = pid; about.drawerKey = pid;
    about.title = tr("Sobre %1").arg(t->name);
    out.push_back(about);
    flattenNodes(t->nodes, QString(), QString(), out, pid);
    return out;
}
int Explorer::realmOfPlace(const QString& placeId)
{
    for (int r = 0; r < realmCount(); r++) if (placeOf(r, false) == placeId) return r;
    return -1;
}
QVector<Explorer::XLink> Explorer::linksOf(int r, int* offMap)
{
    QVector<XLink> out;
    if (offMap) *offMap = 0;
    const QString pid = placeOf(r, false);
    if (pid.isEmpty()) return out;
    for (const auto& l : m_link.places->links()) {
        if (l.fromTerritorioId != pid && l.toTerritorioId != pid) continue;
        const QString other = l.fromTerritorioId == pid ? l.toTerritorioId : l.fromTerritorioId;
        const int o = realmOfPlace(other);
        if (o < 0) { if (offMap) (*offMap)++; continue; }
        XLink x;
        x.id = l.id; x.a = r; x.b = o;
        x.label = linkLabel(l.id);
        x.color = QColor(l.color).isValid() ? QColor(l.color) : m_accent;
        out.push_back(x);
    }
    return out;
}
QString Explorer::linkHtml(const QString& linkId) const
{
    const auto* l = m_link.places ? m_link.places->link(linkId) : nullptr;
    return l ? l->docContent : QString();
}
// a conexão não tem nome próprio: a primeira linha do texto dela, ou "A e B"
QString Explorer::linkLabel(const QString& linkId) const
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
QString Explorer::rawHtml(const XDoc& d) const
{
    if (d.kind == XDoc::Drawer) {
        if (m_link.syncEditor) m_link.syncEditor();
        const DrawerItem* it = m_link.model ? m_link.model->findDrawerItem(d.id) : nullptr;
        return DocPreview::resolveDrawerItemHtml(it, m_link.elements, m_link.cache, m_link.root, false);
    }
    const auto* t = m_link.places ? m_link.places->territorio(d.drawerKey) : nullptr;
    if (!t) return QString();
    if (d.kind == XDoc::PlaceAbout) return t->content;
    const auto* n = findNode(t->nodes, d.id);
    return n ? n->content : QString();
}

void Explorer::pushView()
{
    if (!m_canvas) return;
    ExploreView v;
    v.sel = m_sel; v.hov = m_hov;
    if (m_link.ok()) {
        for (int r = 0; r < realmCount(); r++) {
            int n = static_cast<int>(drawerDocs(r).size());
            const QVector<XDoc> pd = placeDocs(r);
            for (const XDoc& d : pd) if (d.kind == XDoc::PlaceNode || !rawHtml(d).trimmed().isEmpty()) n++;
            v.counts.push_back(n);
        }
        if (!m_duoLink.isEmpty() && m_duo->isVisible()) {
            v.duo = true;
            for (const XLink& l : linksOf(m_duoA)) if (l.id == m_duoLink) v.links.push_back({ l.id, l.a, l.b, l.label, l.color });
        } else if (m_links && m_sel >= 0) {
            for (const XLink& l : linksOf(m_sel)) v.links.push_back({ l.id, l.a, l.b, l.label, l.color });
        }
    }
    m_canvas->setExplore(v);
}

// ── estado ──
void Explorer::setActive(bool on)
{
    if (on == m_active) return;
    m_active = on;
    if (!on) { flushSave(); close(); m_canvas->setExplore(ExploreView()); return; }
    pushView();
}
QString Explorer::hint() const
{
    if (!m_link.ok()) return tr("Explorar: abra um projeto pra ligar os territórios aos documentos");
    if (!realmCount()) return tr("Explorar: este mapa ainda não tem territórios. Crie um em Territórios.");
    return tr("Explorar: clique num território pra ver os documentos dele · arraste pra mover · Esc fecha");
}
void Explorer::hovered(int realm, const QString& linkId, QPoint at)
{
    if (!m_active) return;
    if (realm != m_hov) { m_hov = realm; pushView(); }
    if (realm < 0 && linkId.isEmpty()) { m_chip->hide(); return; }
    const QString dot = QStringLiteral("<span style=\"color:%1\">●</span>&nbsp;");
    if (!linkId.isEmpty()) {
        const auto* l = m_link.places ? m_link.places->link(linkId) : nullptr;
        const QColor c = l && QColor(l->color).isValid() ? QColor(l->color) : m_accent;
        m_chip->setText(dot.arg(c.name()) + linkLabel(linkId).toHtmlEscaped()
                        + QStringLiteral("&nbsp;&nbsp;<span style=\"color:%1\">%2</span>").arg(m_muted.name(), tr("ler os dois lados")));
    } else {
        int count = static_cast<int>(drawerDocs(realm).size());
        for (const XDoc& d : placeDocs(realm)) if (d.kind == XDoc::PlaceNode || !rawHtml(d).trimmed().isEmpty()) count++;
        m_chip->setText(dot.arg(realmColor(realm).name()) + realmName(realm).toHtmlEscaped()
                        + QStringLiteral("&nbsp;&nbsp;<span style=\"color:%1\">%2%3</span>")
                              .arg(m_muted.name(), count ? tr("%n documento(s)", "", count) : tr("nenhum documento ainda"), realm == m_sel ? QString() : tr(" · clique pra abrir")));
    }
    m_chip->adjustSize();
    const QPoint p = m_canvas->mapTo(m_host, at);
    m_chip->move(std::min(p.x() + 16, m_host->width() - m_chip->width() - 8), p.y() + 18);
    m_chip->show();
    m_chip->raise();
}
void Explorer::clicked(int realm, const QString& linkId)
{
    if (!m_active) return;
    if (!m_link.ok()) { if (flash) flash(hint()); return; }
    if (!linkId.isEmpty()) {
        const auto* l = m_link.places->link(linkId);
        if (!l) return;
        const int a = realmOfPlace(l->fromTerritorioId), b = realmOfPlace(l->toTerritorioId);
        openDuo(linkId, m_sel == b ? b : a);
        return;
    }
    if (realm < 0) { if (!m_duo->isVisible()) close(); return; }
    if (realm != m_sel) {
        flushSave();
        m_sel = realm; m_mode = CardMode::List; m_query.clear();
        if (m_cur.valid()) { m_cur = XDoc(); m_edit = false; }
        m_duo->hide(); m_duoLink.clear();
        m_cardAt = QPoint(-1, -1);
    }
    renderCard();
    m_card->show(); m_card->raise();
    m_reader->setVisible(m_cur.valid());
    place();
    pushView();
}
void Explorer::close()
{
    flushSave();
    m_sel = -1; m_cur = XDoc(); m_edit = false; m_links = false; m_pickLink = false; m_mode = CardMode::List;
    m_duoLink.clear(); m_duoEdit = -1;
    m_card->hide(); m_reader->hide(); m_duo->hide(); m_chip->hide();
    pushView();
}
bool Explorer::escape()
{
    if (m_duo->isVisible()) { flushSave(); m_duo->hide(); m_duoLink.clear(); if (m_sel >= 0) { m_card->show(); m_reader->setVisible(m_cur.valid()); place(); } pushView(); return true; }
    if (m_cur.valid()) { flushSave(); m_cur = XDoc(); m_edit = false; m_links = false; m_reader->hide(); renderCard(); pushView(); return true; }
    if (m_mode != CardMode::List && m_card->isVisible()) { m_mode = CardMode::List; renderCard(); return true; }
    if (m_sel >= 0) { close(); return true; }
    return false;
}
void Explorer::mapReplaced()
{
    // outro mapa ou o mesmo refeito: os índices dos territórios podem ter mudado
    close();
    if (m_active) pushView();
}
bool Explorer::owns(const QWidget* w) const
{
    for (const QWidget* p = w; p; p = p->parentWidget())
        if (p == m_card || p == m_reader || p == m_duo) return true;
    return false;
}

// ── peças ──
QLabel* Explorer::caption(QWidget* parent, const QString& text, const QColor& dot)
{
    auto* l = new QLabel(parent);
    l->setObjectName(QStringLiteral("xCaption"));
    l->setTextFormat(Qt::RichText);
    l->setText((dot.isValid() ? QStringLiteral("<span style=\"color:%1\">●</span>&nbsp;").arg(dot.name()) : QString()) + text.toUpper().toHtmlEscaped());
    return l;
}
QPushButton* Explorer::row(QWidget* parent, const QString& title, const QString& right, const QColor& dot, bool selected, bool sheet, const std::function<void()>& onClick)
{
    auto* b = new QPushButton(parent);
    b->setObjectName(QStringLiteral("xRow"));
    b->setProperty("sel", selected);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedHeight(32);
    auto* h = new QHBoxLayout(b);
    h->setContentsMargins(10, 0, 10, 0);
    h->setSpacing(8);
    auto* d = new QLabel(b);
    d->setFixedSize(6, 6);
    d->setStyleSheet(QStringLiteral("background:%1; border-radius:3px;").arg((dot.isValid() ? dot : m_muted).name()));
    h->addWidget(d);
    if (sheet) {
        auto* ic = new QLabel(b);
        ic->setPixmap(iconPixmap(QStringLiteral("id"), m_muted, 14, b->devicePixelRatioF()));
        h->addWidget(ic);
    }
    auto* t = new QLabel(b);
    t->setObjectName(QStringLiteral("xRowTitle"));
    t->setText(t->fontMetrics().elidedText(title, Qt::ElideRight, sheet ? 170 : 190));
    t->setToolTip(title);
    h->addWidget(t, 1);
    auto* r = new QLabel(right, b);
    r->setObjectName(QStringLiteral("xRowRight"));
    h->addWidget(r);
    for (QWidget* w : { static_cast<QWidget*>(d), static_cast<QWidget*>(t), static_cast<QWidget*>(r) }) w->setAttribute(Qt::WA_TransparentForMouseEvents);
    connect(b, &QPushButton::clicked, this, onClick);
    return b;
}

void Explorer::buildCard()
{
    m_card = new QFrame(m_host);
    m_card->setObjectName(QStringLiteral("cartoCard"));
    m_card->setFixedWidth(kCardW);
    auto* l = new QVBoxLayout(m_card);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    auto* head = new QWidget(m_card);
    head->setObjectName(QStringLiteral("xHead"));
    head->setCursor(Qt::OpenHandCursor);
    head->installEventFilter(this);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(16, 12, 10, 10);
    hl->setSpacing(10);
    m_cardSwatch = new QLabel(head);
    m_cardSwatch->setFixedSize(14, 14);
    hl->addWidget(m_cardSwatch, 0, Qt::AlignTop);
    auto* tt = new QVBoxLayout();
    tt->setSpacing(2);
    m_cardTitle = new QLabel(head);
    m_cardTitle->setObjectName(QStringLiteral("cartoCardTitle"));
    m_cardSub = new QLabel(head);
    m_cardSub->setObjectName(QStringLiteral("cartoNote"));
    tt->addWidget(m_cardTitle);
    tt->addWidget(m_cardSub);
    hl->addLayout(tt, 1);
    auto* x = iconBtn(head, tr("Fechar"));
    x->setProperty("xicon", QStringLiteral("close"));
    connect(x, &QToolButton::clicked, this, &Explorer::close);
    hl->addWidget(x, 0, Qt::AlignTop);
    l->addWidget(head);
    auto* sep = new QFrame(m_card); sep->setObjectName(QStringLiteral("xSep")); sep->setFixedHeight(1);
    l->addWidget(sep);
    m_cardScroll = new QScrollArea(m_card);
    m_cardScroll->setObjectName(QStringLiteral("xScroll"));
    m_cardScroll->setWidgetResizable(true);
    m_cardScroll->setFrameShape(QFrame::NoFrame);
    m_cardScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_cardBody = new QWidget();
    m_cardBody->setObjectName(QStringLiteral("xBodyHost"));
    auto* bl = new QVBoxLayout(m_cardBody);
    bl->setContentsMargins(8, 8, 8, 10);
    bl->setSpacing(4);
    m_cardScroll->setWidget(m_cardBody);
    l->addWidget(m_cardScroll, 1);
    auto* sep2 = new QFrame(m_card); sep2->setObjectName(QStringLiteral("xSep")); sep2->setFixedHeight(1);
    l->addWidget(sep2);
    m_cardFoot = new QWidget(m_card);
    auto* fl = new QHBoxLayout(m_cardFoot);
    fl->setContentsMargins(12, 10, 12, 12);
    fl->setSpacing(6);
    l->addWidget(m_cardFoot);
    m_card->hide();
}

void Explorer::renderCard()
{
    if (m_sel < 0 || m_sel >= realmCount()) { m_card->hide(); return; }
    const int r = m_sel;
    const QString name = realmName(r);
    m_cardSwatch->setStyleSheet(QStringLiteral("background:%1; border-radius:4px;").arg(realmColor(r).name()));
    m_cardTitle->setText(name);
    const QVector<XDoc> dd = drawerDocs(r), pd = placeDocs(r);
    int nDocs = static_cast<int>(dd.size());
    for (const XDoc& d : pd) if (d.kind == XDoc::PlaceNode || !rawHtml(d).trimmed().isEmpty()) nDocs++;
    int off = 0;
    const int nLinks = static_cast<int>(linksOf(r, &off).size());
    m_cardSub->setText((nDocs ? tr("%n documento(s)", "", nDocs) : tr("nenhum documento ainda")) + QStringLiteral(" · ")
                       + (nLinks ? tr("%n conexão(ões)", "", nLinks) : tr("nenhuma conexão")));

    auto* bl = static_cast<QVBoxLayout*>(m_cardBody->layout());
    clearLayout(bl);
    auto* fl = static_cast<QHBoxLayout*>(m_cardFoot->layout());
    clearLayout(fl);
    auto note = [&](const QString& t) {
        auto* n = new QLabel(t, m_cardBody); n->setObjectName(QStringLiteral("cartoNote")); n->setWordWrap(true);
        n->setContentsMargins(8, 2, 8, 6); bl->addWidget(n);
    };
    auto source = [&](const QString& icon, const QString& title, const QString& sub) {
        auto* w = new QWidget(m_cardBody);
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(8, 6, 8, 2);
        h->setSpacing(7);
        auto* ic = new QLabel(w); ic->setPixmap(iconPixmap(icon, m_accent, 15, w->devicePixelRatioF()));
        auto* t = new QLabel(title, w); t->setObjectName(QStringLiteral("xSource"));
        auto* s = new QLabel(sub, w); s->setObjectName(QStringLiteral("xRowRight"));
        h->addWidget(ic); h->addWidget(t); h->addStretch(); h->addWidget(s);
        bl->addWidget(w);
    };

    if (m_mode == CardMode::List) {
        // 1) os documentos das gavetas marcados com o grupo do território
        source(QStringLiteral("tray"), tr("Das gavetas"), tr("grupo %1").arg(name));
        if (dd.isEmpty()) note(tr("Nenhum documento de gaveta neste grupo ainda."));
        QStringList keys;
        for (const XDoc& d : dd) if (!keys.contains(d.drawerKey)) keys << d.drawerKey;
        for (const QString& k : keys) {
            const XDoc* first = nullptr;
            for (const XDoc& d : dd) if (d.drawerKey == k) { first = &d; break; }
            bl->addWidget(caption(m_cardBody, first->drawerTitle, first->color));
            for (const XDoc& d : dd) if (d.drawerKey == k)
                bl->addWidget(row(m_cardBody, d.title, QString(), d.color, m_cur.same(d), d.sheet, [this, d]() { openDoc(d); }));
        }
        // 2) os documentos do próprio Lugar no Criador de Mundos
        auto* sp = new QWidget(m_cardBody); sp->setFixedHeight(6); bl->addWidget(sp);
        source(QStringLiteral("globe"), tr("Do Criador de Mundos"), tr("lugar %1").arg(name));
        if (pd.isEmpty()) note(tr("Ainda não há um Lugar com esse nome no Criador de Mundos. Ele nasce quando você criar um documento lá."));
        QString lastFolder = QStringLiteral("\x01");
        const QColor wc = m_accent;
        for (const XDoc& d : pd) {
            if (d.kind == XDoc::PlaceNode && d.folder != lastFolder && !d.folder.isEmpty()) bl->addWidget(caption(m_cardBody, d.folder));
            if (d.kind == XDoc::PlaceNode) lastFolder = d.folder;
            bl->addWidget(row(m_cardBody, d.title, QString(), wc, m_cur.same(d), false, [this, d]() { openDoc(d); }));
        }
        bl->addStretch(1);
        auto* nb = textBtn(m_cardFoot, tr("Novo documento"));
        nb->setIcon(icon(QStringLiteral("plus"), m_ink, m_accent, 15));
        connect(nb, &QPushButton::clicked, this, [this]() { m_mode = CardMode::New; m_newTitleText.clear(); renderCard(); });
        auto* ab = textBtn(m_cardFoot, tr("Adicionar existente"));
        ab->setIcon(icon(QStringLiteral("in"), m_ink, m_accent, 15));
        connect(ab, &QPushButton::clicked, this, [this]() { m_mode = CardMode::Add; m_query.clear(); renderCard(); });
        fl->addWidget(nb, 1); fl->addWidget(ab, 1);
    } else if (m_mode == CardMode::New) {
        bl->setSpacing(8);
        bl->addWidget(caption(m_cardBody, tr("Título")));
        m_newTitle = new QLineEdit(m_cardBody);
        m_newTitle->setObjectName(QStringLiteral("cartoField"));
        m_newTitle->setPlaceholderText(tr("Ex.: A Ponte dos Enforcados"));
        m_newTitle->setText(m_newTitleText);
        connect(m_newTitle, &QLineEdit::textEdited, this, [this](const QString& t) { m_newTitleText = t; });
        connect(m_newTitle, &QLineEdit::returnPressed, this, &Explorer::createDoc);
        bl->addWidget(m_newTitle);
        bl->addWidget(caption(m_cardBody, tr("Onde")));
        auto* seg = new QWidget(m_cardBody);
        auto* sl = new QHBoxLayout(seg); sl->setContentsMargins(0, 0, 0, 0); sl->setSpacing(4);
        for (const auto& [id, label, ic] : { std::tuple<QString, QString, QString>{ QStringLiteral("drawer"), tr("Numa gaveta"), QStringLiteral("tray") },
                                            std::tuple<QString, QString, QString>{ QStringLiteral("place"), tr("No Criador de Mundos"), QStringLiteral("globe") } }) {
            auto* b = new QToolButton(seg);
            b->setObjectName(QStringLiteral("cartoSeg"));
            b->setCheckable(true);
            b->setChecked(m_newWhere == id);
            b->setText(label);
            b->setIcon(icon(ic, m_muted, m_accent, 15));
            b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            b->setCursor(Qt::PointingHandCursor);
            connect(b, &QToolButton::clicked, this, [this, id = id]() { m_newWhere = id; renderCard(); });
            sl->addWidget(b);
        }
        bl->addWidget(seg);
        m_newCombo = new QComboBox(m_cardBody);
        m_newCombo->setObjectName(QStringLiteral("cartoCombo"));
        if (m_newWhere == QLatin1String("drawer")) {
            bl->addWidget(caption(m_cardBody, tr("Gaveta")));
            for (const Drawer& dr : m_link.model->drawers()) m_newCombo->addItem(dr.title, dr.key);
            const int i = m_newCombo->findData(m_newDrawer);
            if (i >= 0) m_newCombo->setCurrentIndex(i);
            connect(m_newCombo, &QComboBox::currentIndexChanged, this, [this]() { if (m_newCombo) m_newDrawer = m_newCombo->currentData().toString(); });
            bl->addWidget(m_newCombo);
            note(tr("Vai pra gaveta escolhida já no grupo %1.").arg(name));
        } else {
            bl->addWidget(caption(m_cardBody, tr("Pasta")));
            m_newCombo->addItem(tr("Sem pasta"), QString());
            QVector<QPair<QString, QString>> folders;
            const QString pid = placeOf(r, false);
            if (const auto* t = pid.isEmpty() ? nullptr : m_link.places->territorio(pid)) collectFolders(t->nodes, QString(), folders);
            for (const auto& f : folders) m_newCombo->addItem(f.second, f.first);
            const int i = m_newCombo->findData(m_newFolder);
            if (i >= 0) m_newCombo->setCurrentIndex(i);
            connect(m_newCombo, &QComboBox::currentIndexChanged, this, [this]() { if (m_newCombo) m_newFolder = m_newCombo->currentData().toString(); });
            bl->addWidget(m_newCombo);
            note(tr("Vira um documento do lugar %1 no Criador de Mundos.").arg(name));
        }
        bl->addStretch(1);
        auto* cb = textBtn(m_cardFoot, tr("Cancelar"));
        connect(cb, &QPushButton::clicked, this, [this]() { m_mode = CardMode::List; renderCard(); });
        auto* go = textBtn(m_cardFoot, tr("Criar e abrir"), true);
        connect(go, &QPushButton::clicked, this, &Explorer::createDoc);
        fl->addWidget(cb, 1); fl->addWidget(go, 1);
        QTimer::singleShot(0, this, [this]() { if (m_newTitle) m_newTitle->setFocus(); });
    } else {
        m_queryEdit = new QLineEdit(m_cardBody);
        m_queryEdit->setObjectName(QStringLiteral("cartoField"));
        m_queryEdit->setPlaceholderText(tr("Buscar nas gavetas do projeto"));
        m_queryEdit->setText(m_query);
        m_queryEdit->setClearButtonEnabled(true);
        bl->addWidget(m_queryEdit);
        auto* listHost = new QWidget(m_cardBody);
        auto* ll = new QVBoxLayout(listHost);
        ll->setContentsMargins(0, 4, 0, 0);
        ll->setSpacing(2);
        bl->addWidget(listHost);
        const QString gid = groupOf(r, false);
        auto fill = [this, ll, listHost, gid, r]() {
            clearLayout(ll);
            const QString q = m_query.trimmed();
            int shown = 0;
            for (const Drawer& dr : m_link.model->drawers())
                for (const DrawerItem& it : dr.items) {
                    if (it.isChapter || (!gid.isEmpty() && it.markerId == gid)) continue;
                    if (!q.isEmpty() && !it.title.contains(q, Qt::CaseInsensitive)) continue;
                    if (++shown > 80) break;
                    const Group* g = it.markerId.isEmpty() ? nullptr : m_link.model->findGroup(it.markerId);
                    const QString right = g ? tr("sai de %1").arg(g->title) : dr.title;
                    const QString itemId = it.id, from = g ? g->title : QString(), title = it.title;
                    ll->addWidget(row(listHost, it.title, right, QColor(dr.color), false, it.isSheet, [this, itemId, from, title, r]() {
                        const QString g2 = groupOf(r, true);
                        m_link.model->setDrawerItemGroup(itemId, g2);
                        if (m_link.save) m_link.save();
                        if (flash) flash(from.isEmpty() ? tr("“%1” agora faz parte de %2.").arg(title, realmName(r))
                                                       : tr("“%1” saiu de %2 e entrou em %3.").arg(title, from, realmName(r)));
                        m_mode = CardMode::List;
                        for (const XDoc& d : drawerDocs(r)) if (d.id == itemId) { openDoc(d); return; }
                        renderCard();
                    }));
                }
            if (!shown) { auto* n = new QLabel(tr("Nada encontrado."), listHost); n->setObjectName(QStringLiteral("cartoNote")); n->setContentsMargins(8, 4, 8, 4); ll->addWidget(n); }
        };
        fill();
        connect(m_queryEdit, &QLineEdit::textChanged, this, [this, fill](const QString& t) { m_query = t; fill(); });
        note(tr("Um documento mora em um grupo só. Trazer um de outro território tira ele de lá."));
        bl->addStretch(1);
        auto* back = textBtn(m_cardFoot, tr("Voltar"));
        connect(back, &QPushButton::clicked, this, [this]() { m_mode = CardMode::List; renderCard(); });
        fl->addWidget(back, 1);
        QTimer::singleShot(0, this, [this]() { if (m_queryEdit) m_queryEdit->setFocus(); });
    }
    applyTheme();
    // o tamanho certo só existe depois que os widgets novos se arrumam
    QTimer::singleShot(0, this, &Explorer::place);
}

void Explorer::createDoc()
{
    const int r = m_sel;
    if (r < 0) return;
    const QString title = m_newTitleText.trimmed().isEmpty() ? tr("Documento sem título") : m_newTitleText.trimmed();
    XDoc d;
    if (m_newWhere == QLatin1String("drawer")) {
        if (m_newCombo) m_newDrawer = m_newCombo->currentData().toString();
        if (m_newDrawer.isEmpty() && !m_link.model->drawers().isEmpty()) m_newDrawer = m_link.model->drawers().first().key;
        const QString gid = groupOf(r, true);
        DrawerItem it;
        it.id = ProjectModel::uid();
        it.title = title;
        it.hasInlineHtml = true;
        it.html = QStringLiteral("<p></p>");
        it.markerId = gid;
        if (!m_link.model->addDrawerItem(m_newDrawer, it)) return;
        if (m_link.save) m_link.save();
        for (const XDoc& x : drawerDocs(r)) if (x.id == it.id) d = x;
    } else {
        if (m_newCombo) m_newFolder = m_newCombo->currentData().toString();
        const QString pid = placeOf(r, true);
        const QString nodeId = m_link.places->addNode(pid, m_newFolder, TerritorioStore::NodeType::Doc, title);
        for (const XDoc& x : placeDocs(r)) if (x.id == nodeId) d = x;
    }
    m_mode = CardMode::List;
    if (!d.valid()) { renderCard(); return; }
    openDoc(d);
    setEditing(true);
    pushView();
}

// ── leitor 3×4 ──
void Explorer::buildReader()
{
    m_reader = new QFrame(m_host);
    m_reader->setObjectName(QStringLiteral("cartoCard"));
    auto* l = new QVBoxLayout(m_reader);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    auto* head = new QWidget(m_reader);
    head->setObjectName(QStringLiteral("xHead"));
    head->setCursor(Qt::OpenHandCursor);
    head->installEventFilter(this);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(14, 10, 10, 6);
    hl->setSpacing(4);
    m_rChip = new QLabel(head);
    m_rChip->setObjectName(QStringLiteral("xChip"));
    m_rChip->setTextFormat(Qt::RichText);
    hl->addWidget(m_rChip, 1);
    m_rEdit = textBtn(head, tr("Editar"));
    connect(m_rEdit, &QPushButton::clicked, this, [this]() {
        if (m_cur.sheet) { if (m_link.openItem) m_link.openItem(m_cur.id); return; }   // ficha: a janela da ficha
        setEditing(!m_edit);
    });
    hl->addWidget(m_rEdit);
    m_rWide = iconBtn(head, tr("Ampliar a leitura"));
    m_rWide->setCheckable(true);
    connect(m_rWide, &QToolButton::clicked, this, [this](bool on) { m_wide = on; renderReader(); place(); });
    m_rLinksBtn = iconBtn(head, tr("Conexões com outros territórios"));
    m_rLinksBtn->setCheckable(true);
    connect(m_rLinksBtn, &QToolButton::clicked, this, [this](bool on) { m_links = on; m_pickLink = false; renderLinksBox(); pushView(); });
    m_rOpen = iconBtn(head, QString());
    connect(m_rOpen, &QToolButton::clicked, this, [this]() {
        flushSave();
        if (m_cur.kind == XDoc::Drawer) { if (m_link.openItem) m_link.openItem(m_cur.id); }
        else if (m_link.openPlace) m_link.openPlace(m_cur.drawerKey, m_cur.kind == XDoc::PlaceNode ? m_cur.id : QString());
    });
    m_rClose = iconBtn(head, tr("Fechar"));
    connect(m_rClose, &QToolButton::clicked, this, [this]() { flushSave(); m_cur = XDoc(); m_edit = false; m_links = false; m_reader->hide(); renderCard(); pushView(); });
    for (QToolButton* b : { m_rWide, m_rLinksBtn, m_rOpen, m_rClose }) hl->addWidget(b);
    l->addWidget(head);
    m_rTitle = new QLineEdit(m_reader);
    m_rTitle->setObjectName(QStringLiteral("xTitle"));
    connect(m_rTitle, &QLineEdit::editingFinished, this, [this]() {
        if (!m_edit || !m_cur.valid() || m_rTitle->isReadOnly()) return;
        const QString t = m_rTitle->text().trimmed();
        if (t.isEmpty() || t == m_cur.title) { m_rTitle->setText(m_cur.title); return; }
        if (m_cur.kind == XDoc::Drawer) {
            const DrawerItem* it = m_link.model->findDrawerItem(m_cur.id);
            m_link.model->updateDrawerItemMeta(m_cur.id, t, it ? it->role : QString());
            if (m_link.save) m_link.save();
        } else if (m_cur.kind == XDoc::PlaceNode) {
            flushSave();
            m_loading = true;
            m_link.places->updateNode(m_cur.drawerKey, m_cur.id, t, rawHtml(m_cur));
            m_loading = false;
        }
        m_cur.title = t;
        renderCard();
    });
    l->addWidget(m_rTitle);
    // cabeçalho do elemento (foto, apelidos, papel) — fora do texto editável
    m_rEl = new QWidget(m_reader);
    auto* el = new QHBoxLayout(m_rEl);
    el->setContentsMargins(18, 2, 18, 10);
    el->setSpacing(12);
    m_rPhoto = new QLabel(m_rEl);
    m_rPhoto->setFixedSize(64, 64);
    el->addWidget(m_rPhoto);
    auto* ev = new QVBoxLayout();
    ev->setSpacing(4);
    m_rAlias = new QLabel(m_rEl);
    m_rAlias->setObjectName(QStringLiteral("xAlias"));
    m_rRole = new QLabel(m_rEl);
    m_rRole->setObjectName(QStringLiteral("xRole"));
    ev->addWidget(m_rAlias);
    ev->addWidget(m_rRole, 0, Qt::AlignLeft);
    el->addLayout(ev, 1);
    l->addWidget(m_rEl);
    // barra de formatação (editando)
    m_rTools = new QWidget(m_reader);
    auto* tl = new QHBoxLayout(m_rTools);
    tl->setContentsMargins(12, 0, 12, 6);
    tl->setSpacing(2);
    auto fmt = [this, tl](const QString& text, const QString& tip, const std::function<void(QTextEdit*)>& fn, const QString& ic = QString()) {
        auto* b = iconBtn(m_rTools, tip);
        if (ic.isEmpty()) { b->setText(text); QFont f = b->font(); f.setBold(text == QLatin1String("B")); f.setItalic(text == QLatin1String("I")); f.setUnderline(text == QLatin1String("U")); b->setFont(f); }
        else b->setProperty("xicon", ic);
        connect(b, &QToolButton::clicked, this, [this, fn]() { fn(m_rBody); m_rBody->setFocus(); });
        tl->addWidget(b);
    };
    fmt(QStringLiteral("B"), tr("Negrito"), [](QTextEdit* e) { e->setFontWeight(e->fontWeight() >= QFont::Bold ? QFont::Normal : QFont::Bold); });
    fmt(QStringLiteral("I"), tr("Itálico"), [](QTextEdit* e) { e->setFontItalic(!e->fontItalic()); });
    fmt(QStringLiteral("U"), tr("Sublinhado"), [](QTextEdit* e) { e->setFontUnderline(!e->fontUnderline()); });
    fmt(QStringLiteral("T"), tr("Subtítulo"), [](QTextEdit* e) {
        QTextCursor c = e->textCursor();
        QTextBlockFormat bf = c.blockFormat();
        const bool on = bf.headingLevel() == 3;
        bf.setHeadingLevel(on ? 0 : 3);
        c.setBlockFormat(bf);
        QTextCharFormat cf; cf.setFontWeight(on ? QFont::Normal : QFont::Bold);
        c.select(QTextCursor::BlockUnderCursor); c.mergeCharFormat(cf);
    });
    fmt(QString(), tr("Lista"), [](QTextEdit* e) {
        QTextCursor c = e->textCursor();
        if (QTextList* lst = c.currentList()) { QTextBlockFormat bf; bf.setIndent(0); lst->remove(c.block()); c.setBlockFormat(bf); }
        else c.createList(QTextListFormat::ListDisc);
    }, QStringLiteral("list"));
    fmt(QString(), tr("Imagem"), [this](QTextEdit* e) { insertImage(e); }, QStringLiteral("image"));
    tl->addStretch(1);
    l->addWidget(m_rTools);
    // conexões
    m_rLinks = new QFrame(m_reader);
    m_rLinks->setObjectName(QStringLiteral("xLinks"));
    auto* rl = new QVBoxLayout(m_rLinks);
    rl->setContentsMargins(8, 8, 8, 8);
    rl->setSpacing(2);
    auto* lw = new QWidget(m_reader);
    auto* lwl = new QVBoxLayout(lw);
    lwl->setContentsMargins(12, 0, 12, 8);
    lwl->addWidget(m_rLinks);
    l->addWidget(lw);
    m_rLinks->setProperty("wrap", QVariant::fromValue<QObject*>(lw));
    // o texto
    m_rBody = new QTextEdit(m_reader);
    m_rBody->setObjectName(QStringLiteral("xBody"));
    m_rBody->setFrameShape(QFrame::NoFrame);
    m_rBody->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_rBody->document()->setDocumentMargin(14);
    connect(m_rBody, &QTextEdit::textChanged, this, [this]() { if (!m_loading && m_edit) bodyEdited(m_rBody, m_cur); });
    l->addWidget(m_rBody, 1);
    auto* sep = new QFrame(m_reader); sep->setObjectName(QStringLiteral("xSep")); sep->setFixedHeight(1);
    l->addWidget(sep);
    auto* foot = new QWidget(m_reader);
    auto* fl = new QHBoxLayout(foot);
    fl->setContentsMargins(18, 8, 12, 10);
    m_rFoot = new QLabel(foot);
    m_rFoot->setObjectName(QStringLiteral("cartoNote"));
    fl->addWidget(m_rFoot, 1);
    m_rUngroup = new QPushButton(foot);
    m_rUngroup->setObjectName(QStringLiteral("xLinkBtn"));
    m_rUngroup->setCursor(Qt::PointingHandCursor);
    connect(m_rUngroup, &QPushButton::clicked, this, [this]() {
        if (m_cur.kind != XDoc::Drawer) return;
        flushSave();
        const QString title = m_cur.title;
        m_link.model->setDrawerItemGroup(m_cur.id, QString());
        if (m_link.save) m_link.save();
        if (flash) flash(tr("“%1” saiu de %2. Continua no projeto, na gaveta %3.").arg(title, realmName(m_sel), m_cur.drawerTitle));
        m_cur = XDoc(); m_edit = false; m_reader->hide(); renderCard(); pushView();
    });
    fl->addWidget(m_rUngroup);
    l->addWidget(foot);
    m_reader->hide();
}

void Explorer::openDoc(const XDoc& d)
{
    flushSave();
    m_cur = d; m_edit = false; m_pickLink = false;
    renderReader();
    m_reader->show(); m_reader->raise();
    renderCard();
    place();
    // o leitor só sabe a largura depois de posto: ajusta as imagens
    QTimer::singleShot(0, this, [this]() { if (m_cur.valid() && !m_edit) fitBody(m_rBody, !m_wide); });
}

void Explorer::setEditing(bool on)
{
    if (!m_cur.valid() || m_cur.sheet) return;
    if (!on) flushSave();
    m_edit = on;
    renderReader();
    if (on) { m_rBody->setFocus(); m_rBody->moveCursor(QTextCursor::End); }
}

void Explorer::renderReader()
{
    if (!m_cur.valid()) { m_reader->hide(); return; }
    const XDoc& d = m_cur;
    const bool place = d.kind != XDoc::Drawer;
    QString where = place ? tr("Criador de Mundos") : d.drawerTitle;
    if (d.kind == XDoc::PlaceNode && !d.folder.isEmpty()) where += QStringLiteral(" · ") + d.folder;
    const QColor dot = place ? m_accent : d.color;
    const int room = (m_wide ? 520 : kReaderW - 4 * 30 - 92 - 40);
    m_rChip->setText(QStringLiteral("<span style=\"color:%1\">●</span>&nbsp;%2").arg(dot.name(), m_rChip->fontMetrics().elidedText(where.toUpper(), Qt::ElideRight, room).toHtmlEscaped()));
    m_rEdit->setText(d.sheet ? tr("Editar ficha") : m_edit ? tr("Concluir") : tr("Editar"));
    m_rEdit->setObjectName(m_edit ? QStringLiteral("xPrimary") : QStringLiteral("xBtn"));
    m_rEdit->setIcon(m_edit ? QIcon() : icon(QStringLiteral("pen"), m_ink, m_accent, 14));
    m_rEdit->setToolTip(d.sheet ? tr("Abre a janela da ficha") : QString());
    m_rWide->setChecked(m_wide);
    m_rWide->setProperty("xicon", m_wide ? QStringLiteral("shrink") : QStringLiteral("expand"));
    m_rWide->setToolTip(m_wide ? tr("Voltar ao leitor pequeno") : tr("Ampliar a leitura"));
    m_rLinksBtn->setChecked(m_links);
    m_rLinksBtn->setProperty("xicon", QStringLiteral("link"));
    m_rOpen->setProperty("xicon", QStringLiteral("open"));
    m_rOpen->setToolTip(place ? tr("Abrir no Criador de Mundos") : tr("Abrir no editor"));
    m_rClose->setProperty("xicon", QStringLiteral("close"));
    m_rTitle->setText(d.title);
    m_rTitle->setReadOnly(!m_edit || d.kind == XDoc::PlaceAbout);
    m_rTitle->setProperty("editing", m_edit && d.kind != XDoc::PlaceAbout);
    // elemento (personagem com foto)
    const Element* e = (d.kind == XDoc::Drawer && !d.elementId.isEmpty() && m_link.elements) ? m_link.elements->findElement(d.elementId) : nullptr;
    m_rEl->setVisible(e && !m_edit);
    if (e) {
        m_rPhoto->setPixmap(AvatarUtils::circularAvatar(e->image, e->name, e->id, 64));
        m_rAlias->setText(e->aliases.join(QStringLiteral(" · ")));
        m_rAlias->setVisible(!e->aliases.isEmpty());
        m_rRole->setText(e->role.toUpper());
        m_rRole->setVisible(!e->role.isEmpty());
    }
    m_rTools->setVisible(m_edit);
    renderLinksBox();
    loadBody(m_rBody, d, m_edit, !m_wide);
    m_rFoot->setText(place ? tr("Lugar %1 · Criador de Mundos").arg(realmName(m_sel)) : realmName(m_sel));
    m_rUngroup->setVisible(!place);
    m_rUngroup->setText(tr("Tirar de %1").arg(realmName(m_sel)));
    applyTheme();
}

void Explorer::renderLinksBox()
{
    auto* wrap = qobject_cast<QWidget*>(m_rLinks->property("wrap").value<QObject*>());
    if (wrap) wrap->setVisible(m_links);
    m_rLinksBtn->setChecked(m_links);
    if (!m_links) return;
    auto* l = static_cast<QVBoxLayout*>(m_rLinks->layout());
    clearLayout(l);
    const int r = m_sel;
    if (m_pickLink) {
        l->addWidget(caption(m_rLinks, tr("Ligar %1 a…").arg(realmName(r))));
        const QVector<XLink> have = linksOf(r);
        int shown = 0;
        for (int o = 0; o < realmCount(); o++) {
            if (o == r) continue;
            bool linked = false;
            for (const XLink& x : have) if (x.b == o) linked = true;
            if (linked) continue;
            shown++;
            l->addWidget(row(m_rLinks, realmName(o), QString(), realmColor(o), false, false, [this, r, o]() {
                const QString pa = placeOf(r, true), pb = placeOf(o, true);
                const QString id = m_link.places->addLink(pa, pb, realmColor(r).name());
                m_pickLink = false;
                openDuo(id, r);
                m_duoEdit = 1;
                renderDuo();
            }));
        }
        if (!shown) { auto* n = new QLabel(tr("Todos os territórios já estão ligados a %1.").arg(realmName(r)), m_rLinks); n->setObjectName(QStringLiteral("cartoNote")); n->setWordWrap(true); l->addWidget(n); }
        auto* c = textBtn(m_rLinks, tr("Cancelar"));
        connect(c, &QPushButton::clicked, this, [this]() { m_pickLink = false; renderLinksBox(); });
        l->addWidget(c, 0, Qt::AlignLeft);
        applyTheme();
        return;
    }
    l->addWidget(caption(m_rLinks, tr("Conexões de %1").arg(realmName(r))));
    int off = 0;
    const QVector<XLink> links = linksOf(r, &off);
    if (links.isEmpty()) { auto* n = new QLabel(tr("Nenhuma ainda."), m_rLinks); n->setObjectName(QStringLiteral("cartoNote")); n->setContentsMargins(8, 0, 8, 2); l->addWidget(n); }
    for (const XLink& x : links)
        l->addWidget(row(m_rLinks, x.label, tr("com %1").arg(realmName(x.b)), x.color, false, false, [this, x]() { openDuo(x.id, x.a); }));
    if (off) { auto* n = new QLabel(tr("%n conexão(ões) com lugares fora deste mapa", "", off), m_rLinks); n->setObjectName(QStringLiteral("cartoNote")); n->setContentsMargins(8, 2, 8, 2); l->addWidget(n); }
    auto* nb = textBtn(m_rLinks, tr("Nova conexão…"));
    nb->setIcon(icon(QStringLiteral("plus"), m_ink, m_accent, 14));
    connect(nb, &QPushButton::clicked, this, [this]() { m_pickLink = true; renderLinksBox(); });
    l->addWidget(nb, 0, Qt::AlignLeft);
    applyTheme();
}

// ── o texto: ler (com o tema e o tamanho de leitura) ou editar (o html de verdade) ──
void Explorer::loadBody(QTextEdit* ed, const XDoc& d, bool editing, bool compact)
{
    loadHtml(ed, rawHtml(d), editing && !d.sheet, compact);
}
void Explorer::loadHtml(QTextEdit* ed, const QString& html, bool editing, bool compact)
{
    m_loading = true;
    ed->setReadOnly(!editing);
    ed->setHtml(html);
    if (!editing) {
        // linhas vazias do começo não ocupam o leitor
        QTextDocument* doc = ed->document();
        for (int guard = 0; guard < 40; guard++) {
            const QTextBlock b = doc->begin();
            if (!b.isValid() || !b.next().isValid() || !b.text().trimmed().isEmpty()) break;
            if (doc->frameAt(b.position()) != doc->rootFrame() || doc->frameAt(b.next().position()) != doc->rootFrame()) break;
            QTextCursor c(doc);
            c.setPosition(b.position());
            c.setPosition(b.next().position(), QTextCursor::KeepAnchor);
            c.removeSelectedText();
        }
        DocPreview::applyThemeTextColors(ed->document());
        DocPreview::applyPreviewFontSize(ed->document(), 11);
        fitBody(ed, compact);
    }
    ed->verticalScrollBar()->setValue(0);
    m_loading = false;
}
// leitor pequeno: imagem que flutua vira bloco; nenhuma imagem passa da largura
void Explorer::fitBody(QTextEdit* ed, bool compact)
{
    QTextDocument* doc = ed->document();
    const bool was = m_loading;
    m_loading = true;
    const double avail = std::max(120.0, ed->viewport()->width() - 2 * doc->documentMargin() - 6);
    if (compact) {
        std::function<void(QTextFrame*)> walk = [&](QTextFrame* f) {
            for (QTextFrame* c : f->childFrames()) {
                QTextFrameFormat ff = c->frameFormat();
                if (ff.position() != QTextFrameFormat::InFlow) {
                    ff.setPosition(QTextFrameFormat::InFlow);
                    ff.setLeftMargin(0); ff.setRightMargin(0); ff.setTopMargin(0); ff.setBottomMargin(10);
                    c->setFrameFormat(ff);
                }
                walk(c);
            }
        };
        walk(doc->rootFrame());
    }
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        // bloco que só tem imagem: sem recuo de parágrafo (senão ela sai cortada à direita)
        if (compact && b.text().trimmed() == QString(QChar::ObjectReplacementCharacter)) {
            QTextBlockFormat bf = b.blockFormat();
            if (bf.textIndent() != 0 || bf.leftMargin() != 0 || bf.indent() != 0) {
                bf.setTextIndent(0); bf.setLeftMargin(0); bf.setIndent(0);
                QTextCursor c(b); c.setBlockFormat(bf);
            }
        }
        for (auto it = b.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid() || !frag.charFormat().isImageFormat()) continue;
            QTextImageFormat f = frag.charFormat().toImageFormat();
            double w = f.width(), h = f.height();
            if (w <= 0 || h <= 0) {
                const QVariant res = doc->resource(QTextDocument::ImageResource, QUrl(f.name()));
                QImage img = res.value<QImage>();
                if (img.isNull()) img = res.value<QPixmap>().toImage();
                if (img.isNull()) continue;
                if (w <= 0 && h <= 0) { w = img.width(); h = img.height(); }
                else if (w <= 0) w = h * img.width() / std::max(1, img.height());
                else h = w * img.height() / std::max(1, img.width());
            }
            const double target = compact ? avail : std::min(w, avail);
            if (std::abs(w - target) < 1 && f.width() > 0) continue;
            if (!compact && w <= avail && f.width() > 0) continue;
            f.setWidth(target); f.setHeight(h * target / w);
            QTextCursor c(doc);
            c.setPosition(frag.position());
            c.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
            c.setCharFormat(f);
        }
    }
    m_loading = was;
}

void Explorer::insertImage(QTextEdit* ed)
{
    const QString path = QFileDialog::getOpenFileName(m_host, tr("Imagem"), QString(), tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (path.isEmpty()) return;
    QImage img(path);
    if (img.isNull()) return;
    if (img.width() > 1400) img = img.scaledToWidth(1400, Qt::SmoothTransformation);
    QString src;
    const bool drawer = ed == m_rBody ? m_cur.kind == XDoc::Drawer : (m_duoEdit == 0 || m_duoEdit == 2) && m_col[m_duoEdit].doc.kind == XDoc::Drawer;
    if (drawer && !m_link.root.isEmpty()) {
        // documento de gaveta: a imagem vai pra pasta imagens/ do projeto, como no editor
        QDir().mkpath(m_link.root + QStringLiteral("/imagens"));
        const QString file = m_link.root + QStringLiteral("/imagens/") + QFileInfo(path).completeBaseName() + QLatin1Char('-') + ProjectModel::uid().left(8) + QStringLiteral(".jpg");
        if (img.save(file, "JPG", 88)) src = QUrl::fromLocalFile(file).toString();
    }
    if (src.isEmpty()) {
        QByteArray bytes; QBuffer buf(&bytes); buf.open(QIODevice::WriteOnly); img.save(&buf, "JPG", 86);
        src = QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(bytes.toBase64());
        ed->document()->addResource(QTextDocument::ImageResource, QUrl(src), img);
    }
    QTextImageFormat f;
    f.setName(src);
    const double w = std::min<double>(img.width(), ed->viewport()->width() - 40);
    f.setWidth(w); f.setHeight(img.height() * w / img.width());
    ed->textCursor().insertImage(f);
}

// ── gravar ──
void Explorer::bodyEdited(QTextEdit* ed, const XDoc& d)
{
    if (m_pendingEd && m_pendingEd != ed) flushSave();
    m_pendingEd = ed;
    m_pendingDoc = d;
    m_pendingLink = (ed == m_col[1].body) ? m_duoLink : QString();
    m_saveTimer->start();
}
void Explorer::flushSave()
{
    m_saveTimer->stop();
    QTextEdit* ed = m_pendingEd;
    m_pendingEd = nullptr;
    if (!ed) return;
    const QString html = ed->toHtml();
    m_loading = true;
    if (!m_pendingLink.isEmpty()) {
        m_link.places->updateLinkContent(m_pendingLink, html);
    } else if (m_pendingDoc.kind == XDoc::Drawer && !m_pendingDoc.sheet) {
        // de fora do editor: o editor já descarregou no cache quando o doc abriu; grava no cache e avisa
        if (m_link.syncEditor) m_link.syncEditor();
        if (m_link.cache) m_link.cache->set(DocCache::itemKey(m_pendingDoc.id), html, true);
        if (m_link.itemEdited) m_link.itemEdited(m_pendingDoc.id);
    } else if (m_pendingDoc.kind == XDoc::PlaceNode) {
        QString name = m_pendingDoc.title;
        if (const auto* t = m_link.places->territorio(m_pendingDoc.drawerKey)) if (const auto* n = findNode(t->nodes, m_pendingDoc.id)) name = n->name;
        m_link.places->updateNode(m_pendingDoc.drawerKey, m_pendingDoc.id, name, html);
    } else if (m_pendingDoc.kind == XDoc::PlaceAbout) {
        m_link.places->updateTerritorioContent(m_pendingDoc.drawerKey, html);
    }
    m_loading = false;
    m_pendingLink.clear();
}

// ── lado a lado: território A · a conexão · território B ──
void Explorer::buildDuo()
{
    m_duo = new QFrame(m_host);
    m_duo->setObjectName(QStringLiteral("cartoCard"));
    auto* l = new QVBoxLayout(m_duo);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    auto* head = new QWidget(m_duo);
    head->setObjectName(QStringLiteral("xHead"));
    head->setCursor(Qt::OpenHandCursor);
    head->installEventFilter(this);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 10, 10, 10);
    hl->setSpacing(10);
    auto* back = textBtn(head, tr("Voltar"));
    back->setProperty("xicon", QStringLiteral("back"));
    connect(back, &QPushButton::clicked, this, [this]() { escape(); });
    hl->addWidget(back);
    m_dTitle = new QLabel(head);
    m_dTitle->setObjectName(QStringLiteral("xDuoTitle"));
    m_dTitle->setTextFormat(Qt::RichText);
    hl->addWidget(m_dTitle, 1);
    auto* swap = textBtn(head, tr("Trocar lados"));
    swap->setProperty("xicon", QStringLiteral("swap"));
    connect(swap, &QPushButton::clicked, this, [this]() {
        flushSave();
        std::swap(m_duoA, m_duoB);
        std::swap(m_col[0].doc, m_col[2].doc);
        m_sel = m_duoA;
        m_duoEdit = -1;
        renderDuo();
    });
    hl->addWidget(swap);
    auto* x = iconBtn(head, tr("Fechar"));
    x->setProperty("xicon", QStringLiteral("close"));
    connect(x, &QToolButton::clicked, this, &Explorer::close);
    hl->addWidget(x);
    l->addWidget(head);
    auto* sep = new QFrame(m_duo); sep->setObjectName(QStringLiteral("xSep")); sep->setFixedHeight(1);
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
        Col& col = m_col[i];
        col.chip = new QLabel(c);
        col.chip->setObjectName(QStringLiteral("xChip"));
        col.chip->setTextFormat(Qt::RichText);
        ch->addWidget(col.chip, 1);
        if (i != 1) {
            col.pick = new QComboBox(c);
            col.pick->setObjectName(QStringLiteral("cartoCombo"));
            col.pick->setMaximumWidth(190);
            connect(col.pick, &QComboBox::activated, this, [this, i](int idx) {
                flushSave();
                const int r = i == 0 ? m_duoA : m_duoB;
                QVector<XDoc> docs = drawerDocs(r); docs += placeDocs(r);
                if (idx >= 0 && idx < docs.size()) m_col[i].doc = docs[idx];
                if (m_duoEdit == i) m_duoEdit = -1;
                renderDuo();
            });
            ch->addWidget(col.pick);
        }
        col.pen = iconBtn(c, tr("Editar"));
        col.pen->setCheckable(true);
        col.pen->setProperty("xicon", QStringLiteral("pen"));
        connect(col.pen, &QToolButton::clicked, this, [this, i]() {
            if (i != 1 && m_col[i].doc.sheet) { if (m_link.openItem) m_link.openItem(m_col[i].doc.id); return; }
            flushSave();
            m_duoEdit = m_duoEdit == i ? -1 : i;
            renderDuo();
            if (m_duoEdit == i) m_col[i].body->setFocus();
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
        connect(col.body, &QTextEdit::textChanged, this, [this, i]() {
            if (m_loading || m_duoEdit != i) return;
            bodyEdited(m_col[i].body, m_col[i].doc);
        });
        cl->addWidget(col.body, 1);
        cols->addWidget(c, 1);
    }
    l->addLayout(cols, 1);
    m_duo->hide();
}

void Explorer::openDuo(const QString& linkId, int from)
{
    const auto* l = m_link.places ? m_link.places->link(linkId) : nullptr;
    if (!l) return;
    flushSave();
    const int a = realmOfPlace(l->fromTerritorioId), b = realmOfPlace(l->toTerritorioId);
    if (a < 0 || b < 0) return;
    m_duoLink = linkId;
    m_duoA = from == b ? b : a;
    m_duoB = m_duoA == a ? b : a;
    m_sel = m_duoA;
    m_duoEdit = -1;
    // de que documento cada lado começa: o aberto no leitor, o Sobre do lugar com texto, ou o primeiro
    auto pick = [this](int r, const XDoc& prefer) {
        QVector<XDoc> docs = drawerDocs(r); docs += placeDocs(r);
        for (const XDoc& d : docs) if (prefer.valid() && d.same(prefer)) return d;
        for (const XDoc& d : docs) if (d.kind == XDoc::PlaceAbout && !rawHtml(d).trimmed().isEmpty()) return d;
        return docs.isEmpty() ? XDoc() : docs.first();
    };
    m_col[0].doc = pick(m_duoA, m_cur);
    m_col[2].doc = pick(m_duoB, XDoc());
    m_card->hide(); m_reader->hide(); m_chip->hide();
    renderDuo();
    m_duo->show(); m_duo->raise();
    m_duoAt = QPoint(-1, -1);
    place();
    pushView();
    // a câmera põe os dois territórios na faixa livre acima do painel
    QTimer::singleShot(0, this, [this]() {
        const auto A = m_canvas->realmCenterOnScreen(m_duoA), B = m_canvas->realmCenterOnScreen(m_duoB);
        if (!A || !B) return;
        const QPointF top = m_canvas->mapFrom(m_host, QPoint(0, m_duo->y()));
        const QPointF target(m_canvas->width() / 2.0, (70 + top.y()) / 2.0);
        const QPointF mid = (*A + *B) / 2;
        const double lift = std::min(70.0, std::hypot(B->x() - A->x(), B->y() - A->y()) * 0.14);
        m_canvas->panBy(target.x() - mid.x(), target.y() - (mid.y() - lift));
    });
}

void Explorer::renderDuo()
{
    if (m_duoLink.isEmpty()) return;
    const auto* l = m_link.places->link(m_duoLink);
    if (!l) { m_duo->hide(); return; }
    const QColor lc = QColor(l->color).isValid() ? QColor(l->color) : m_accent;
    m_dTitle->setText(QStringLiteral("%1 <span style=\"color:%3\">⟷</span> %2").arg(realmName(m_duoA).toHtmlEscaped(), realmName(m_duoB).toHtmlEscaped(), m_accent.name()));
    for (int i = 0; i < 3; i++) {
        Col& c = m_col[i];
        const bool editing = m_duoEdit == i;
        c.pen->setChecked(editing);
        c.pen->setToolTip(editing ? tr("Concluir") : i != 1 && c.doc.sheet ? tr("Editar ficha") : tr("Editar"));
        if (i == 1) {
            c.chip->setText(QStringLiteral("<span style=\"color:%1\">●</span>&nbsp;%2").arg(lc.name(), tr("CONEXÃO")));
            c.title->setText(tr("%1 e %2").arg(realmName(m_duoA), realmName(m_duoB)));
            loadHtml(c.body, l->docContent, editing, true);
            if (editing && l->docContent.trimmed().isEmpty()) c.body->setPlaceholderText(tr("Como esses dois lugares se ligam? Guerra, aliança, comércio, uma família dividida…"));
            continue;
        }
        const int r = i == 0 ? m_duoA : m_duoB;
        c.chip->setText(QStringLiteral("<span style=\"color:%1\">●</span>&nbsp;%2").arg(realmColor(r).name(), realmName(r).toUpper().toHtmlEscaped()));
        QVector<XDoc> docs = drawerDocs(r); docs += placeDocs(r);
        c.pick->blockSignals(true);
        c.pick->clear();
        int cur = -1;
        for (int k = 0; k < docs.size(); k++) { c.pick->addItem(docs[k].title); if (c.doc.valid() && docs[k].same(c.doc)) cur = k; }
        c.pick->setCurrentIndex(cur);
        c.pick->setVisible(docs.size() > 1);
        c.pick->blockSignals(false);
        c.pen->setEnabled(c.doc.valid());
        if (!c.doc.valid()) {
            c.title->setText(QString());
            loadHtml(c.body, QStringLiteral("<p style=\"color:%1\"><i>%2</i></p>").arg(m_muted.name(), tr("%1 ainda não tem documentos.").arg(realmName(r)).toHtmlEscaped()), false, true);
            continue;
        }
        c.title->setText(c.doc.title);
        loadBody(c.body, c.doc, editing, true);
    }
    applyTheme();
}

// ── posição ──
void Explorer::place()
{
    const int W = m_host->width(), H = m_host->height();
    if (m_card->isVisible()) {
        const int x = m_cardAt.x() >= 0 ? m_cardAt.x() : 16, y = m_cardAt.y() >= 0 ? m_cardAt.y() : kTop;
        const int bottom = (m_cardAt.y() < 0 && cartelaTop && cartelaTop() > 0) ? cartelaTop() - 12 : H - 16;
        m_card->move(std::clamp(x, 8, std::max(8, W - m_card->width() - 8)), std::clamp(y, 64, std::max(64, H - 160)));
        // a altura acompanha o que tem dentro, até onde a tela (ou a Cartela) deixa
        if (m_cardBody->layout()) { m_cardBody->layout()->invalidate(); m_cardBody->layout()->activate(); }
        const int want = m_cardBody->layout() ? m_cardBody->layout()->sizeHint().height() + 64 + 58 : 400;
        m_card->setFixedHeight(std::max(220, std::min(want, bottom - m_card->y())));
    }
    if (m_reader->isVisible()) {
        const int w = m_wide ? std::min(760, std::max(kReaderW, W - (m_card->isVisible() ? m_card->geometry().right() : 0) - 28)) : kReaderW;
        m_reader->setFixedWidth(w);
        const int x = m_readerAt.x() >= 0 ? m_readerAt.x() : (m_card->isVisible() ? m_card->geometry().right() + 12 : 16);
        const int y = m_readerAt.y() >= 0 ? m_readerAt.y() : (m_card->isVisible() ? m_card->y() : kTop);
        m_reader->move(std::clamp(x, 8, std::max(8, W - w - 8)), std::clamp(y, 64, std::max(64, H - 120)));
        m_reader->setFixedHeight(m_wide ? H - m_reader->y() - 16 : std::min(kReaderH, H - m_reader->y() - 16));
    }
    if (m_duo->isVisible()) {
        const int w = std::min(1180, W - 32), h = std::min(520, std::max(340, H / 2));
        m_duo->setFixedSize(w, h);
        const int x = m_duoAt.x() >= 0 ? m_duoAt.x() : (W - w) / 2, y = m_duoAt.y() >= 0 ? m_duoAt.y() : H - h - 56;
        m_duo->move(std::clamp(x, 8, std::max(8, W - w - 8)), std::clamp(y, 64, std::max(64, H - 120)));
    }
}

void Explorer::applyTheme()
{
    m_ink = Theme::toColor(Theme::textPrimary());
    m_muted = Theme::toColor(Theme::textMuted());
    m_line = Theme::toColor(Theme::panelBorder());
    m_accent = Theme::toColor(Theme::accentDefault());
    m_panel = Theme::toColor(Theme::panelBackground());
    m_panel2 = Theme::toColor(Theme::hoverOverlay());
    const QString soft = QStringLiteral("rgba(%1,%2,%3,0.18)").arg(m_accent.red()).arg(m_accent.green()).arg(m_accent.blue());
    const QString css = Theme::qss(QStringLiteral(
        "QComboBox#cartoCombo QAbstractItemView { background: %1; color: %3; selection-background-color: %7; }"
        "QWidget#xHead { background: transparent; }"
        "QFrame#xSep { background: %2; border: none; }"
        "QScrollArea#xScroll, QWidget#xBodyHost { background: transparent; border: none; }"
        "QLabel#xCaption { color: %6; font-size: 10px; font-weight: 600; letter-spacing: 1px; padding: 6px 8px 2px; background: transparent; }"
        "QLabel#xSource { color: %3; font-size: 12px; font-weight: 600; background: transparent; }"
        "QPushButton#xRow { background: transparent; border: 1px solid transparent; border-radius: 8px; text-align: left; }"
        "QPushButton#xRow:hover { background: %4; }"
        "QPushButton#xRow[sel=\"true\"] { background: %7; border-color: %5; }"
        "QLabel#xRowTitle { color: %3; font-size: 13px; background: transparent; }"
        "QLabel#xRowRight { color: %6; font-size: 10.5px; background: transparent; }"
        "QPushButton#xBtn { background: transparent; border: 1px solid %2; border-radius: 8px; color: %3; font-size: 12px; padding: 6px 10px; }"
        "QPushButton#xBtn:hover { border-color: %5; color: %5; }"
        "QPushButton#xPrimary { background: %5; border: 1px solid %5; border-radius: 8px; color: %8; font-size: 12px; font-weight: 600; padding: 6px 10px; }"
        "QToolButton#xIco { background: transparent; border: 1px solid transparent; border-radius: 8px; color: %6; font-size: 13px; }"
        "QToolButton#xIco:hover { background: %4; color: %3; }"
        "QToolButton#xIco:checked { background: %7; border-color: %5; color: %5; }"
        "QLabel#xChip { color: %6; font-size: 11px; font-weight: 600; letter-spacing: 1px; background: transparent; }"
        "QLineEdit#xTitle { background: transparent; border: none; color: %3; font-family: Georgia, 'Times New Roman', serif; font-size: 19px; font-weight: 600; padding: 0 16px 6px; }"
        "QLineEdit#xTitle[editing=\"true\"] { background: %4; border: 1px solid %2; border-radius: 6px; margin: 0 10px 6px; padding: 3px 6px; }"
        "QLabel#xAlias { color: %3; font-family: Georgia, serif; font-style: italic; font-size: 13px; background: transparent; }"
        "QLabel#xRole { color: %5; font-size: 10px; font-weight: 600; letter-spacing: 1px; border: 1px solid %5; border-radius: 9px; padding: 1px 8px; background: transparent; }"
        "QFrame#xLinks { background: transparent; border: 1px solid %2; border-radius: 10px; }"
        "QTextEdit#xBody { background: transparent; color: %3; selection-background-color: %7; padding: 0px; margin: 0px; border: none; }"
        "QTextEdit#xBody[editing=\"true\"] { background: %4; border-radius: 8px; }"
        "QPushButton#xLinkBtn { background: transparent; border: none; color: %6; font-size: 11px; text-decoration: underline; padding: 2px; }"
        "QPushButton#xLinkBtn:hover { color: %5; }"
        "QLabel#xDuoTitle { color: %3; font-family: 'IM FELL English SC', Georgia, serif; font-size: 16px; letter-spacing: 1px; background: transparent; }"
        "QFrame#xDuoCol { background: transparent; border: none; border-right: 1px solid %2; }"
        "QFrame#xDuoMid { background: %4; border: none; border-right: 1px solid %2; }"
        "QLabel#xColTitle { color: %3; font-family: Georgia, serif; font-size: 17px; font-weight: 600; padding: 0 16px 4px; background: transparent; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(), Theme::hoverOverlay(), Theme::accentDefault(),
          Theme::textMuted(), soft, Theme::readableTextOn(m_accent).name()));
    for (QFrame* f : { m_card, m_reader, m_duo }) if (f) f->setStyleSheet(css);
    m_chip->setStyleSheet(QStringLiteral("QLabel#xHover { background: %1; color: %2; border: 1px solid %3; border-radius: 13px; padding: 5px 12px; font-size: 12px; }")
                              .arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder()));
    if (m_rBody) m_rBody->setProperty("editing", m_edit);
    for (int i = 0; i < 3; i++) if (m_col[i].body) m_col[i].body->setProperty("editing", m_duoEdit == i);
    // ícones (os botões guardam o nome na propriedade xicon)
    for (QFrame* f : { m_card, m_reader, m_duo }) {
        if (!f) continue;
        for (QAbstractButton* b : f->findChildren<QAbstractButton*>()) {
            const QString n = b->property("xicon").toString();
            if (!n.isEmpty()) b->setIcon(icon(n, m_muted, m_accent, 16));
        }
        for (QWidget* w : f->findChildren<QWidget*>()) { w->style()->unpolish(w); w->style()->polish(w); }
    }
}

bool Explorer::eventFilter(QObject* o, QEvent* e)
{
    // arrastar os painéis pelo cabeçalho
    static QPoint grab;
    auto* w = qobject_cast<QWidget*>(o);
    if (!w) return false;
    QFrame* frame = qobject_cast<QFrame*>(w->parentWidget());
    if (frame != m_card && frame != m_reader && frame != m_duo) return false;
    if (e->type() == QEvent::MouseButtonPress) {
        grab = static_cast<QMouseEvent*>(e)->globalPosition().toPoint() - frame->pos();
        return false;
    }
    if (e->type() == QEvent::MouseMove && (static_cast<QMouseEvent*>(e)->buttons() & Qt::LeftButton)) {
        const QPoint p = static_cast<QMouseEvent*>(e)->globalPosition().toPoint() - grab;
        if (frame == m_card) m_cardAt = p; else if (frame == m_reader) m_readerAt = p; else m_duoAt = p;
        place();
        return true;
    }
    return false;
}

} // namespace carto
