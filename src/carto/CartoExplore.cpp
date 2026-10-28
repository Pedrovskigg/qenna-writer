#include "CartoExplore.h"

#include "CartoCanvas.h"
#include "CartoDoc.h"
#include "CartoIcons.h"

#include "../AvatarUtils.h"
#include "../DocCache.h"
#include "../DocPreview.h"
#include "../ElementsStore.h"
#include "../GroupBadge.h"
#include "../ProjectModel.h"
#include "../TerritorioStore.h"
#include "../Theme.h"

#include <QAbstractButton>
#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QApplication>
#include <QComboBox>
#include <QDrag>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
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
constexpr int kCardW = 336, kReaderW = 360, kReaderH = 480, kTop = 86;

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
// documento de gaveta arrastado no cartão (o id do item)
const char kDocMime[] = "application/x-qenna-drawer-item";

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
void Explorer::syncGroups(const std::vector<RealmDef>* before)
{
    if (!m_link.model || !m_doc || !m_doc->info) return;
    ProjectModel* M = m_link.model;
    bool groupsTouched = false, mapTouched = false;
    // 1) o mapa é a raiz: um grupo Lugar com o nome dele (os territórios moram dentro)
    QString mapTitle = mapName ? mapName().trimmed() : QString();
    if (mapTitle.isEmpty()) mapTitle = m_doc->displayTitle();
    bool adopt = false;
    if (m_doc->groupId.isEmpty() || !M->findGroup(m_doc->groupId)) {
        m_doc->groupId = M->addGroup(mapTitle, QStringLiteral("#9AA7B8"));
        M->setGroupMap(m_doc->groupId, true);
        groupsTouched = mapTouched = adopt = true;
    } else if (!m_mapSeen.isEmpty() && m_mapSeen != mapTitle) {
        // o mapa trocou de nome: o grupo dele acompanha
        if (const Group* g = M->findGroup(m_doc->groupId)) { M->updateGroup(g->id, mapTitle, g->color); groupsTouched = true; }
    }
    m_mapSeen = mapTitle;
    const QString root = m_doc->groupId;
    if (const Group* g = M->findGroup(root); g && !g->map) { M->setGroupMap(root, true); groupsTouched = true; }

    // 2) cada território é um grupo dentro do mapa
    for (RealmDef& d : m_doc->info->defs) {
        const QString name = d.name.trimmed();
        if (name.isEmpty()) continue;
        if (d.groupId.isEmpty()) {
            for (const Group& g : M->groups())
                if (!g.map && g.title.trimmed().compare(name, Qt::CaseInsensitive) == 0) { d.groupId = g.id; break; }
            if (d.groupId.isEmpty()) { d.groupId = M->addGroup(name, rgbHex(d.color).toUpper(), root); groupsTouched = true; }
            else if (const Group* g = M->findGroup(d.groupId); g && g->parentId.isEmpty()) { M->setGroupParent(d.groupId, root); groupsTouched = true; }
            mapTouched = true;
            continue;
        }
        // mapa que ganhou a raiz agora (feito antes da árvore): os territórios vão pra dentro dele
        if (adopt) if (const Group* g = M->findGroup(d.groupId); g && g->parentId.isEmpty()) { M->setGroupParent(d.groupId, root); groupsTouched = true; }
        // o grupo apagado lá fora não volta sozinho (o Explorar cria de novo se alguém precisar)
        if (!before || !M->findGroup(d.groupId)) continue;
        for (const RealmDef& b : *before) {
            if (b.groupId != d.groupId) continue;
            if (b.name != d.name || b.color != d.color) { M->updateGroup(d.groupId, name, rgbHex(d.color).toUpper()); groupsTouched = true; }
            break;
        }
    }

    // 3) batizou, virou lugar: construção com nome é um Lugar dentro do território
    const bool seeded = !m_artNames.isEmpty();
    for (Art& a : m_doc->info->user.arts) {
        const QString name = a.name.trimmed();
        const QString was = m_artNames.value(a.id);
        m_artNames.insert(a.id, name);
        if (name.isEmpty()) continue;
        if (const Group* g = a.groupId.isEmpty() ? nullptr : M->findGroup(a.groupId)) {
            if (seeded && !was.isEmpty() && was != name && g->title != name) { M->updateGroup(g->id, name, g->color); groupsTouched = true; }
            continue;
        }
        if (!a.groupId.isEmpty() && was == name) continue;   // grupo apagado lá fora: só volta se renomear
        const int r = a.realm >= 0 && a.realm < realmCount() ? a.realm : -1;
        const Group* parent = r >= 0 ? M->findGroup(m_doc->info->defs[r].groupId) : nullptr;
        if (!parent) parent = M->findGroup(root);
        const QString parentId = parent ? parent->id : QString(), color = parent ? parent->color : QStringLiteral("#9AA7B8");
        a.groupId.clear();
        for (const Group& o : M->groups())
            if (!o.map && o.title.trimmed().compare(name, Qt::CaseInsensitive) == 0) { a.groupId = o.id; break; }
        if (a.groupId.isEmpty()) a.groupId = M->addGroup(name, color, parentId);
        groupsTouched = mapTouched = true;
    }
    if (mapTouched && mapEdited) mapEdited();
    if (groupsTouched && m_link.save) m_link.save();
}

// ── a árvore ──
QString Explorer::rootGroup() const
{
    return m_link.model && m_doc && m_link.model->findGroup(m_doc->groupId) ? m_doc->groupId : QString();
}
int Explorer::realmOfGroup(const QString& gid) const
{
    if (gid.isEmpty()) return -1;
    for (int r = 0; r < realmCount(); r++) if (m_doc->info->defs[r].groupId == gid) return r;
    return -1;
}
const Art* Explorer::artOfGroup(const QString& gid) const
{
    if (gid.isEmpty() || !m_doc || !m_doc->info) return nullptr;
    for (const Art& a : m_doc->info->user.arts) if (a.groupId == gid && !a.name.trimmed().isEmpty()) return &a;
    return nullptr;
}
int Explorer::mappedRealm(const QString& gid) const
{
    if (!m_link.model) return -1;
    const QList<const Group*> path = m_link.model->groupPath(gid);
    for (int k = path.size() - 1; k >= 0; k--) {
        const int r = realmOfGroup(path[k]->id);
        if (r >= 0) return r;
        if (const Art* a = artOfGroup(path[k]->id)) return a->realm >= 0 && a->realm < realmCount() ? a->realm : -1;
    }
    return -1;
}
QString Explorer::groupTitle(const QString& gid) const
{
    const Group* g = m_link.model ? m_link.model->findGroup(gid) : nullptr;
    return g ? g->title : QString();
}
QVector<Explorer::XDoc> Explorer::groupDocs(const QString& gid)
{
    QVector<XDoc> out;
    if (gid.isEmpty() || !m_link.model) return out;
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
int Explorer::groupCount(const QString& gid)
{
    int n = static_cast<int>(groupDocs(gid).size());
    const int r = realmOfGroup(gid);
    if (r >= 0) for (const XDoc& d : placeDocs(r)) if (d.kind == XDoc::PlaceNode || !rawHtml(d).trimmed().isEmpty()) n++;
    return n;
}

void Explorer::selectGroup(const QString& groupId, bool fly)
{
    if (!m_link.model || !m_link.model->findGroup(groupId)) return;
    if (groupId != m_group) {
        flushSave();
        m_mode = CardMode::List; m_query.clear(); m_upOpen = false;
        if (m_cur.valid()) { m_cur = XDoc(); m_edit = false; m_links = false; }
        m_duo->hide(); m_duoLink.clear();
    }
    m_group = groupId;
    for (const Group* g : m_link.model->groupPath(groupId)) m_open.insert(g->id);
    m_sel = mappedRealm(groupId);
    renderCard();
    m_card->show(); m_card->raise();
    m_reader->setVisible(m_cur.valid());
    place();
    pushView();
    if (fly) flyToGroup(groupId);
}

// leva o mapa até o grupo: o mapa inteiro, o território ou a construção (social: o lugar de cima)
void Explorer::flyToGroup(const QString& gid)
{
    if (!m_canvas || !m_doc || !m_doc->info || !m_link.model) return;
    // o meio do pedaço livre do mapa (à direita do cartão)
    const int left = m_card->isVisible() ? m_card->geometry().right() : 0;
    const QPointF at = m_canvas->mapFrom(m_host, QPoint((left + m_host->width()) / 2, m_host->height() / 2));
    const QList<const Group*> path = m_link.model->groupPath(gid);
    for (int k = path.size() - 1; k >= 0; k--) {
        const QString id = path[k]->id;
        if (id == rootGroup()) { m_canvas->flyFit(); return; }
        if (const Art* a = artOfGroup(id)) { m_canvas->flyTo(a->x, a->y, 3.2, at); return; }
        const int r = realmOfGroup(id);
        if (r >= 0 && r < static_cast<int>(m_doc->info->realms.size()) && m_doc->info->realms[r].area) {
            m_canvas->flyTo(m_doc->info->realms[r].x, m_doc->info->realms[r].y, 1.8, at);
            return;
        }
    }
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
        if (const Art* a = artOfGroup(m_group)) v.selArt = a->id;
        if (m_doc && m_doc->info)
            for (const Art& a : m_doc->info->user.arts)
                if (!a.groupId.isEmpty() && !a.name.trimmed().isEmpty() && m_link.model->findGroup(a.groupId)) v.arts.push_back(a.id);
        if (const Group* g = m_link.model->findGroup(m_group)) {
            for (const QString& pid : g->actsIn) {
                const int r = mappedRealm(pid);
                if (r >= 0 && std::find(v.acts.begin(), v.acts.end(), r) == v.acts.end()) v.acts.push_back(r);
            }
            v.actsColor = QColor(g->color);
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
    // a árvore abre junto, no escolhido de antes (ou no mapa inteiro)
    const QString g = m_link.model && m_link.model->findGroup(m_group) ? m_group : rootGroup();
    if (!g.isEmpty()) selectGroup(g, false);
}
QString Explorer::hint() const
{
    if (!m_link.ok()) return tr("Explorar: abra um projeto pra ligar os territórios aos documentos");
    if (!realmCount()) return tr("Explorar: este mapa ainda não tem territórios. Crie um em Territórios.");
    return tr("Explorar: clique num território ou numa construção com nome · a árvore leva o mapa até o lugar · arraste pra mover · Esc fecha");
}
void Explorer::hovered(int realm, const QString& linkId, QPoint at)
{
    if (!m_active) return;
    if (realm != m_hov) { m_hov = realm; pushView(); }
    if (realm < 0 && linkId.isEmpty()) { m_chip->hide(); return; }
    const QString dot = QStringLiteral("<span style=\"color:%1\">●</span>&nbsp;");
    if (linkId.startsWith(QLatin1String("art:"))) {
        QString gid;
        if (m_doc && m_doc->info) for (const Art& a : m_doc->info->user.arts) if (QStringLiteral("art:") + a.id == linkId) gid = a.groupId;
        const Group* g = m_link.model ? m_link.model->findGroup(gid) : nullptr;
        if (!g) { m_chip->hide(); return; }
        const int count = groupCount(gid);
        m_chip->setText(dot.arg(g->color) + g->title.toHtmlEscaped()
                        + QStringLiteral("&nbsp;&nbsp;<span style=\"color:%1\">%2%3</span>")
                              .arg(m_muted.name(), count ? tr("%n documento(s)", "", count) : tr("nenhum documento ainda"), gid == m_group ? QString() : tr(" · clique pra abrir")));
    } else if (!linkId.isEmpty()) {
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
    if (linkId.startsWith(QLatin1String("art:"))) {
        if (m_doc && m_doc->info)
            for (const Art& a : m_doc->info->user.arts)
                if (QStringLiteral("art:") + a.id == linkId && m_link.model->findGroup(a.groupId)) { selectGroup(a.groupId, true); return; }
    }
    if (!linkId.isEmpty() && !linkId.startsWith(QLatin1String("art:"))) {
        const auto* l = m_link.places->link(linkId);
        if (!l) return;
        const int a = realmOfPlace(l->fromTerritorioId), b = realmOfPlace(l->toTerritorioId);
        openDuo(linkId, m_sel == b ? b : a);
        return;
    }
    if (m_duo->isVisible() && realm < 0) return;
    // fora dos territórios (o mar): o mapa inteiro
    if (realm < 0) { const QString root = rootGroup(); if (root.isEmpty()) close(); else selectGroup(root, true); return; }
    const QString gid = groupOf(realm, true);
    if (gid.isEmpty()) return;
    selectGroup(gid, true);
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
    if (m_canvas && m_card->isVisible() && m_mode == CardMode::List && !m_cur.valid() && !m_duo->isVisible() && m_link.model) {
        // Esc sobe um nível na árvore antes de fechar
        const Group* g = m_link.model->findGroup(m_group);
        if (g && g->id != rootGroup() && m_link.model->groupIsInside(g->id, rootGroup())) { selectGroup(g->parentId, true); return true; }
    }
    if (m_duo->isVisible()) { flushSave(); m_duo->hide(); m_duoLink.clear(); if (m_sel >= 0) { m_card->show(); m_reader->setVisible(m_cur.valid()); place(); } pushView(); return true; }
    if (m_cur.valid()) { flushSave(); m_cur = XDoc(); m_edit = false; m_links = false; m_reader->hide(); renderCard(); pushView(); return true; }
    if (m_mode != CardMode::List && m_card->isVisible()) { m_mode = CardMode::List; renderCard(); return true; }
    if (m_card->isVisible()) { close(); return true; }
    return false;
}
void Explorer::mapReplaced()
{
    // outro mapa ou o mesmo refeito: os índices dos territórios podem ter mudado
    close();
    m_group.clear(); m_open.clear(); m_artNames.clear(); m_mapSeen.clear();
    if (m_active) {
        pushView();
        // a raiz do mapa novo nasce no syncGroups: a árvore abre depois dele
        QTimer::singleShot(0, this, [this]() { if (m_active && !rootGroup().isEmpty() && !m_card->isVisible()) selectGroup(rootGroup(), false); });
    }
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
    m_crumbs = new QLabel(m_card);
    m_crumbs->setObjectName(QStringLiteral("xCrumbs"));
    m_crumbs->setTextFormat(Qt::RichText);
    m_crumbs->setWordWrap(true);
    m_crumbs->setContentsMargins(16, 0, 14, 10);
    connect(m_crumbs, &QLabel::linkActivated, this, [this](const QString& id) { selectGroup(id, true); });
    l->addWidget(m_crumbs);
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
    const Group* G = m_link.model ? m_link.model->findGroup(m_group) : nullptr;
    if (!G) { m_card->hide(); return; }
    const QString root = rootGroup();
    const Group* R = m_link.model->findGroup(root);
    const QString name = G->title;
    const int r = realmOfGroup(m_group);   // o escolhido é um território? (Criador de Mundos, pastas)
    m_cardSwatch->setStyleSheet(QStringLiteral("background:%1; border-radius:4px;").arg(R ? R->color : realmColor(-1).name()));
    m_cardTitle->setText(R ? R->title : name);
    m_cardSub->setText(tr("Explorar"));
    // o caminho até o escolhido, clicável
    QStringList crumbs;
    const QList<const Group*> path = m_link.model->groupPath(m_group);
    for (int k = 0; k < path.size(); k++) {
        const QString t = path[k]->title.toHtmlEscaped();
        crumbs << (k == path.size() - 1 ? QStringLiteral("<span style=\"color:%1; font-weight:600\">%2</span>").arg(m_accent.name(), t)
                                        : QStringLiteral("<a href=\"%1\" style=\"color:%2; text-decoration:none\">%3</a>").arg(path[k]->id, m_muted.name(), t));
    }
    m_crumbs->setText(crumbs.join(QStringLiteral("<span style=\"color:%1\"> › </span>").arg(m_muted.name())));
    m_crumbs->setVisible(path.size() > 1);

    auto* bl = static_cast<QVBoxLayout*>(m_cardBody->layout());
    clearLayout(bl);
    bl->setSpacing(2);
    auto* fl = static_cast<QHBoxLayout*>(m_cardFoot->layout());
    clearLayout(fl);
    auto note = [&](const QString& t) {
        auto* n = new QLabel(t, m_cardBody); n->setObjectName(QStringLiteral("cartoNote")); n->setWordWrap(true);
        n->setContentsMargins(8, 2, 8, 6); bl->addWidget(n);
    };

    if (m_mode == CardMode::List) {
        renderTree(bl);
        bl->addStretch(1);
        auto* nb = textBtn(m_cardFoot, tr("Novo documento"));
        nb->setIcon(icon(QStringLiteral("plus"), m_ink, m_accent, 15));
        nb->setToolTip(tr("Um documento novo já dentro de %1").arg(name));
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
        // o Criador de Mundos só conhece os territórios; o resto vai pra uma gaveta
        if (r < 0) m_newWhere = QStringLiteral("drawer");
        else {
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
        }
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
    } else if (m_mode == CardMode::NewGroup) {
        bl->setSpacing(8);
        const bool social = m_newKind == QLatin1String("social");
        bl->addWidget(caption(m_cardBody, tr("Nome")));
        m_newTitle = new QLineEdit(m_cardBody);
        m_newTitle->setObjectName(QStringLiteral("cartoField"));
        m_newTitle->setPlaceholderText(social ? tr("Ex.: Casa Varanis") : tr("Ex.: Porto das Brumas"));
        m_newTitle->setText(m_newTitleText);
        connect(m_newTitle, &QLineEdit::textEdited, this, [this](const QString& t) { m_newTitleText = t; });
        connect(m_newTitle, &QLineEdit::returnPressed, this, &Explorer::createGroup);
        bl->addWidget(m_newTitle);
        bl->addWidget(caption(m_cardBody, tr("Tipo")));
        auto* seg = new QWidget(m_cardBody);
        auto* sl = new QHBoxLayout(seg); sl->setContentsMargins(0, 0, 0, 0); sl->setSpacing(4);
        const qreal dpr = m_cardBody->devicePixelRatioF();
        for (const auto& [kind, label] : { std::pair<QString, QString>{ QStringLiteral("place"), tr("Lugar") },
                                           std::pair<QString, QString>{ QStringLiteral("social"), tr("Social") } }) {
            auto* b = new QToolButton(seg);
            b->setObjectName(QStringLiteral("cartoSeg"));
            b->setCheckable(true);
            b->setChecked(m_newKind == kind);
            b->setText(label);
            QPixmap pm(QSize(14, 14) * dpr);
            pm.setDevicePixelRatio(dpr);
            pm.fill(Qt::transparent);
            { QPainter p(&pm); GroupBadge::draw(p, QPointF(7, 7), 6.5, QColor(G->color), kind == QLatin1String("social")); }
            b->setIcon(QIcon(pm));
            b->setIconSize(QSize(14, 14));
            b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            b->setCursor(Qt::PointingHandCursor);
            connect(b, &QToolButton::clicked, this, [this, kind = kind]() { m_newKind = kind; renderCard(); });
            sl->addWidget(b);
        }
        bl->addWidget(seg);
        note(social ? tr("Gente: família, facção, ordem, elenco. Fica dentro de %1.").arg(name)
                    : tr("Tem chão: cidade, região, fortaleza. Fica dentro de %1.").arg(name));
        bl->addStretch(1);
        auto* cb = textBtn(m_cardFoot, tr("Cancelar"));
        connect(cb, &QPushButton::clicked, this, [this]() { m_mode = CardMode::List; renderCard(); });
        auto* go = textBtn(m_cardFoot, tr("Criar grupo"), true);
        connect(go, &QPushButton::clicked, this, &Explorer::createGroup);
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
        const QString gid = m_group;
        auto fill = [this, ll, listHost, gid]() {
            clearLayout(ll);
            const QString q = m_query.trimmed();
            int shown = 0;
            for (const Drawer& dr : m_link.model->drawers())
                for (const DrawerItem& it : dr.items) {
                    if (it.isChapter || it.markerId == gid) continue;
                    if (!q.isEmpty() && !it.title.contains(q, Qt::CaseInsensitive)) continue;
                    if (++shown > 80) break;
                    const Group* g = it.markerId.isEmpty() ? nullptr : m_link.model->findGroup(it.markerId);
                    const QString right = g ? tr("sai de %1").arg(g->title) : dr.title;
                    const QString itemId = it.id, from = g ? g->title : QString(), title = it.title;
                    ll->addWidget(row(listHost, it.title, right, QColor(dr.color), false, it.isSheet, [this, itemId, from, title, gid]() {
                        m_link.model->setDrawerItemGroup(itemId, gid);
                        if (m_link.save) m_link.save();
                        if (flash) flash(from.isEmpty() ? tr("“%1” agora faz parte de %2.").arg(title, groupTitle(gid))
                                                       : tr("“%1” saiu de %2 e entrou em %3.").arg(title, from, groupTitle(gid)));
                        m_mode = CardMode::List;
                        for (const XDoc& d : groupDocs(gid)) if (d.id == itemId) { openDoc(d); return; }
                        renderCard();
                    }));
                }
            if (!shown) { auto* n = new QLabel(tr("Nada encontrado."), listHost); n->setObjectName(QStringLiteral("cartoNote")); n->setContentsMargins(8, 4, 8, 4); ll->addWidget(n); }
        };
        fill();
        connect(m_queryEdit, &QLineEdit::textChanged, this, [this, fill](const QString& t) { m_query = t; fill(); });
        note(tr("Um documento mora em um grupo só. Trazer um de outro grupo tira ele de lá."));
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

// a árvore vertical: o mapa na raiz, os ramos abertos até o escolhido
void Explorer::renderTree(QVBoxLayout* bl)
{
    const QString root = rootGroup();
    if (root.isEmpty()) return;
    const qreal dpr = m_cardBody->devicePixelRatioF();
    std::function<void(const Group*, int)> walk = [&](const Group* g, int depth) {
        const QList<const Group*> kids = m_link.model->groupChildren(g->id);
        const bool sel = g->id == m_group, open = m_open.contains(g->id);
        auto* b = new QPushButton(m_cardBody);
        b->setObjectName(QStringLiteral("xRow"));
        b->setProperty("sel", sel);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(30);
        b->setToolTip(m_link.model->groupPathText(g->id));
        auto* h = new QHBoxLayout(b);
        h->setContentsMargins(4 + std::min(depth, 7) * 15, 0, 10, 0);
        h->setSpacing(6);
        // a setinha abre e fecha o ramo (só quem tem filhos)
        auto* chev = new QToolButton(b);
        chev->setObjectName(QStringLiteral("xChev"));
        chev->setFixedSize(18, 18);
        chev->setIconSize(QSize(12, 12));
        if (!kids.isEmpty()) {
            chev->setCursor(Qt::PointingHandCursor);
            chev->setIcon(icon(open ? QStringLiteral("chevDown") : QStringLiteral("chevRight"), m_muted, m_accent, 12));
            const QString id = g->id;
            connect(chev, &QToolButton::clicked, this, [this, id]() {
                if (m_open.contains(id)) m_open.remove(id); else m_open.insert(id);
                renderCard();
            });
        } else chev->setEnabled(false);
        h->addWidget(chev);
        auto* badge = new QLabel(b);
        QPixmap pm(QSize(16, 16) * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        { QPainter p(&pm); GroupBadge::draw(p, QPointF(8, 8), 7.5, QColor(g->color), g->social()); }
        badge->setPixmap(pm);
        h->addWidget(badge);
        auto* t = new QLabel(b);
        t->setObjectName(QStringLiteral("xRowTitle"));
        t->setProperty("sel", sel);
        const int room = kCardW - 70 - std::min(depth, 7) * 15 - 30;
        t->setText(t->fontMetrics().elidedText(g->title, Qt::ElideRight, std::max(40, room)));
        h->addWidget(t, 1);
        const int n = groupCount(g->id);
        auto* c = new QLabel(n ? QString::number(n) : QString(), b);
        c->setObjectName(QStringLiteral("xRowRight"));
        h->addWidget(c);
        for (QWidget* w : { static_cast<QWidget*>(badge), static_cast<QWidget*>(t), static_cast<QWidget*>(c) }) w->setAttribute(Qt::WA_TransparentForMouseEvents);
        // solta um documento aqui e ele muda de grupo
        b->setProperty("xDropGroup", g->id);
        b->setAcceptDrops(true);
        b->installEventFilter(this);
        const QString id = g->id;
        connect(b, &QPushButton::clicked, this, [this, id, kids]() {
            // clicar no escolhido abre/fecha o ramo; em outro, escolhe e leva o mapa
            if (id == m_group && !kids.isEmpty()) { if (m_open.contains(id)) m_open.remove(id); else m_open.insert(id); renderCard(); return; }
            selectGroup(id, true);
        });
        bl->addWidget(b);
        if (sel) renderSelected(bl, g->id, depth);
        if (open) for (const Group* k : kids) walk(k, depth + 1);
    };
    if (const Group* R = m_link.model->findGroup(root)) walk(R, 0);
}

// o escolhido aberto na árvore: avisos, "Atua em", os documentos e o que vem de cima
void Explorer::renderSelected(QVBoxLayout* bl, const QString& gid, int depth)
{
    const Group* g = m_link.model->findGroup(gid);
    if (!g) return;
    auto* box = new QWidget(m_cardBody);
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(18 + std::min(depth, 7) * 15, 2, 0, 6);
    v->setSpacing(2);
    bl->addWidget(box);
    auto note = [&](const QString& t) {
        auto* n = new QLabel(t, box); n->setObjectName(QStringLiteral("cartoNote")); n->setWordWrap(true);
        n->setContentsMargins(8, 2, 8, 4); v->addWidget(n);
    };
    auto chips = [&](const QList<QPair<const Group*, QString>>& list) {
        QHBoxLayout* line = nullptr;
        int n = 0;
        for (const auto& [o, text] : list) {
            if (!line || n == 2) { line = new QHBoxLayout(); line->setContentsMargins(6, 0, 6, 2); line->setSpacing(4); v->addLayout(line); n = 0; }
            auto* b = new QPushButton(text, box);
            b->setObjectName(QStringLiteral("xGChip"));
            b->setCursor(Qt::PointingHandCursor);
            b->setToolTip(m_link.model->groupPathText(o->id));
            QPixmap pm(QSize(12, 12) * box->devicePixelRatioF());
            pm.setDevicePixelRatio(box->devicePixelRatioF());
            pm.fill(Qt::transparent);
            { QPainter p(&pm); GroupBadge::draw(p, QPointF(6, 6), 5.5, QColor(o->color), o->social()); }
            b->setIcon(QIcon(pm));
            b->setIconSize(QSize(12, 12));
            const QString id = o->id;
            connect(b, &QPushButton::clicked, this, [this, id]() { selectGroup(id, true); });
            line->addWidget(b);
            n++;
        }
        if (line) line->addStretch(1);
    };

    const int r = realmOfGroup(gid);
    QList<QPair<const Group*, QString>> acts, actors;
    for (const QString& pid : g->actsIn) if (const Group* p = m_link.model->findGroup(pid)) acts.append({ p, p->title });
    for (const Group& o : m_link.model->groups()) if (o.actsIn.contains(gid)) actors.append({ &o, tr("%1 atua aqui").arg(o.title) });
    if (!acts.isEmpty()) { note(tr("Atua em:")); chips(acts); }
    if (!actors.isEmpty()) chips(actors);

    // os documentos dele
    const QVector<XDoc> dd = groupDocs(gid);
    for (const XDoc& d : dd)
        v->addWidget(draggable(row(box, d.title, d.drawerTitle, d.color, m_cur.same(d), d.sheet, [this, d]() { openDoc(d); }), d));
    int nPlace = 0;
    if (r >= 0) {
        const QVector<XDoc> pd = placeDocs(r);
        QString lastFolder = QStringLiteral("\x01");
        for (const XDoc& d : pd) {
            if (d.kind == XDoc::PlaceAbout && rawHtml(d).trimmed().isEmpty() && pd.size() == 1) continue;   // "Sobre X" vazio e sozinho não conta
            if (!nPlace) v->addWidget(caption(box, tr("Do Criador de Mundos")));
            if (d.kind == XDoc::PlaceNode && d.folder != lastFolder && !d.folder.isEmpty()) v->addWidget(caption(box, d.folder));
            if (d.kind == XDoc::PlaceNode) lastFolder = d.folder;
            v->addWidget(row(box, d.title, QString(), m_accent, m_cur.same(d), false, [this, d]() { openDoc(d); }));
            nPlace++;
        }
    }
    if (dd.isEmpty() && !nPlace) note(tr("Nenhum documento deste grupo ainda."));

    // vindo de cima: os documentos do pai, do avô... (o mais perto primeiro), recolhidos
    QVector<QPair<XDoc, QString>> up;
    const QList<const Group*> path = m_link.model->groupPath(gid);
    for (int k = path.size() - 2; k >= 0; k--) {
        for (const XDoc& d : groupDocs(path[k]->id)) up.append({ d, path[k]->title });
        const int ar = realmOfGroup(path[k]->id);
        if (ar >= 0) for (const XDoc& d : placeDocs(ar)) if (d.kind == XDoc::PlaceNode || !rawHtml(d).trimmed().isEmpty()) up.append({ d, path[k]->title });
    }
    if (!up.isEmpty()) {
        auto* t = new QPushButton(box);
        t->setObjectName(QStringLiteral("xUp"));
        t->setCursor(Qt::PointingHandCursor);
        t->setText(QStringLiteral("↑  ") + tr("Vindo de cima · %1").arg(up.size()) + (m_upOpen ? QStringLiteral("  ▾") : QStringLiteral("  ▸")));
        t->setToolTip(tr("Os documentos dos lugares de cima valem pra cá também"));
        connect(t, &QPushButton::clicked, this, [this]() { m_upOpen = !m_upOpen; renderCard(); });
        v->addWidget(t, 0, Qt::AlignLeft);
        if (m_upOpen)
            for (const auto& [d, from] : up)
                v->addWidget(draggable(row(box, d.title, tr("de %1").arg(from), d.kind == XDoc::Drawer ? d.color : m_accent, m_cur.same(d), d.sheet, [this, d]() { openDoc(d); }), d));
    }

    // um grupo novo dentro deste, sem sair do mapa
    auto* add = new QPushButton(box);
    add->setObjectName(QStringLiteral("xUp"));
    add->setCursor(Qt::PointingHandCursor);
    add->setText(QStringLiteral("+  ") + tr("Grupo dentro de %1").arg(add->fontMetrics().elidedText(g->title, Qt::ElideRight, 170)));
    add->setToolTip(tr("Um lugar ou um grupo social dentro de %1").arg(g->title));
    connect(add, &QPushButton::clicked, this, [this]() {
        m_mode = CardMode::NewGroup; m_newTitleText.clear(); m_newKind = QStringLiteral("place");
        renderCard();
    });
    v->addWidget(add, 0, Qt::AlignLeft);
}

void Explorer::createGroup()
{
    const QString parent = m_group;
    const Group* P = m_link.model ? m_link.model->findGroup(parent) : nullptr;
    if (!P) return;
    const QString title = m_newTitleText.trimmed();
    if (title.isEmpty()) { if (m_newTitle) m_newTitle->setFocus(); return; }
    const QString parentTitle = P->title;
    // o filho nasce com a cor do pai, como no painel de Grupos
    const QString id = m_link.model->addGroup(title, P->color, parent, m_newKind);
    if (id.isEmpty()) return;
    if (m_link.save) m_link.save();
    m_mode = CardMode::List;
    m_newTitleText.clear();
    if (flash) flash(tr("%1 agora fica dentro de %2.").arg(title, parentTitle));
    selectGroup(id, true);
}

void Explorer::createDoc()
{
    const QString gid = m_group;
    if (!m_link.model || !m_link.model->findGroup(gid)) return;
    const int r = realmOfGroup(gid);
    const QString title = m_newTitleText.trimmed().isEmpty() ? tr("Documento sem título") : m_newTitleText.trimmed();
    XDoc d;
    if (m_newWhere == QLatin1String("drawer") || r < 0) {
        if (m_newCombo) m_newDrawer = m_newCombo->currentData().toString();
        if (m_newDrawer.isEmpty() && !m_link.model->drawers().isEmpty()) m_newDrawer = m_link.model->drawers().first().key;
        DrawerItem it;
        it.id = ProjectModel::uid();
        it.title = title;
        it.hasInlineHtml = true;
        it.html = QStringLiteral("<p></p>");
        it.markerId = gid;
        if (!m_link.model->addDrawerItem(m_newDrawer, it)) return;
        if (m_link.save) m_link.save();
        for (const XDoc& x : groupDocs(gid)) if (x.id == it.id) d = x;
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
        const DrawerItem* it = m_link.model->findDrawerItem(m_cur.id);
        const QString from = groupTitle(it ? it->markerId : QString());
        m_link.model->setDrawerItemGroup(m_cur.id, QString());
        if (m_link.save) m_link.save();
        if (flash) flash(tr("“%1” saiu de %2. Continua no projeto, na gaveta %3.").arg(title, from, m_cur.drawerTitle));
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
    // de quem é o documento (pode ser de um lugar de cima, aberto pelo "Vindo de cima")
    const DrawerItem* owner = place ? nullptr : m_link.model->findDrawerItem(d.id);
    const QString ownerName = place ? realmName(realmOfPlace(d.drawerKey)) : groupTitle(owner ? owner->markerId : QString());
    m_rFoot->setText(place ? tr("Lugar %1 · Criador de Mundos").arg(ownerName) : ownerName);
    m_rUngroup->setVisible(!place && !ownerName.isEmpty());
    m_rUngroup->setText(tr("Tirar de %1").arg(ownerName));
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
        // texto que quebra linha só mede certo pela largura; o caminho (A › B) soma em cima
        int want = 400;
        if (QLayout* L = m_cardBody->layout()) {
            int body = L->sizeHint().height();
            if (L->hasHeightForWidth()) body = std::max(body, L->heightForWidth(m_cardScroll->viewport()->width()));
            want = body + 64 + 58 + (m_crumbs->isVisible() ? m_crumbs->sizeHint().height() + 4 : 0);
        }
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
        "QPushButton#xRow[drop=\"true\"] { background: %7; border: 1px dashed %5; }"
        "QLabel#xRowTitle { color: %3; font-size: 13px; background: transparent; }"
        "QLabel#xRowRight { color: %6; font-size: 10.5px; background: transparent; }"
        "QLabel#xRowTitle[sel=\"true\"] { font-weight: 600; }"
        "QLabel#xCrumbs { color: %6; font-size: 11.5px; background: transparent; }"
        "QToolButton#xChev { background: transparent; border: none; border-radius: 4px; }"
        "QToolButton#xChev:hover { background: %4; }"
        "QPushButton#xGChip { background: transparent; border: 1px solid %2; border-radius: 10px; color: %3; font-size: 11px; padding: 2px 8px 2px 5px; }"
        "QPushButton#xGChip:hover { border-color: %5; }"
        "QPushButton#xUp { background: transparent; border: none; color: %5; font-size: 11.5px; padding: 5px 8px; text-align: left; }"
        "QPushButton#xUp:hover { text-decoration: underline; }"
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

// Documento de gaveta: arrastando a linha, ele vai pra outro grupo da árvore.
// Os do Criador de Mundos moram no lugar, não num grupo: não arrastam.
QPushButton* Explorer::draggable(QPushButton* b, const XDoc& d)
{
    if (d.kind != XDoc::Drawer) return b;
    b->setProperty("xDragId", d.id);
    b->installEventFilter(this);
    return b;
}

bool Explorer::docDragEvent(QPushButton* b, QEvent* e)
{
    if (e->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(e);
        if (me->button() == Qt::LeftButton) m_dragFrom = me->position().toPoint();
        return false;
    }
    if (e->type() != QEvent::MouseMove) return false;
    auto* me = static_cast<QMouseEvent*>(e);
    if (!(me->buttons() & Qt::LeftButton) || (me->position().toPoint() - m_dragFrom).manhattanLength() < QApplication::startDragDistance()) return false;
    auto* mime = new QMimeData;
    mime->setData(QLatin1String(kDocMime), b->property("xDragId").toString().toUtf8());
    // dono = o cartão: a linha é refeita quando o documento muda de grupo
    auto* drag = new QDrag(m_card);
    const QPixmap shot = b->grab();
    QPixmap ghost(shot.size());
    ghost.setDevicePixelRatio(shot.devicePixelRatio());
    ghost.fill(Qt::transparent);
    { QPainter gp(&ghost); gp.setOpacity(0.85); gp.drawPixmap(0, 0, shot); }
    drag->setPixmap(ghost);
    drag->setHotSpot(m_dragFrom);
    drag->setMimeData(mime);
    b->setDown(false);
    drag->exec(Qt::MoveAction);
    // o cartão só se refaz depois do arrasto: refazer no meio apagaria a linha que ainda está segurando o evento
    if (m_dropDone) {
        m_dropDone = false;
        QTimer::singleShot(0, this, [this]() { renderCard(); pushView(); });
    }
    return true;
}

bool Explorer::groupDropEvent(QPushButton* b, QEvent* e)
{
    const QString gid = b->property("xDropGroup").toString();
    auto hot = [b](bool on) {
        if (b->property("drop").toBool() == on) return;
        b->setProperty("drop", on);
        b->style()->unpolish(b); b->style()->polish(b);
    };
    auto item = [this, gid](const QDropEvent* de) -> const DrawerItem* {
        const QString id = QString::fromUtf8(de->mimeData()->data(QLatin1String(kDocMime)));
        const DrawerItem* it = id.isEmpty() || !m_link.model ? nullptr : m_link.model->findDrawerItem(id);
        return it && it->markerId != gid ? it : nullptr;   // já mora aqui: nada a fazer
    };
    switch (e->type()) {
    case QEvent::DragEnter:
    case QEvent::DragMove: {
        auto* de = static_cast<QDropEvent*>(e);
        const bool ok = item(de) != nullptr;
        hot(ok);
        if (ok) { de->setDropAction(Qt::MoveAction); de->accept(); } else de->ignore();
        return true;
    }
    case QEvent::DragLeave:
        hot(false);
        return true;
    case QEvent::Drop: {
        auto* de = static_cast<QDropEvent*>(e);
        hot(false);
        const DrawerItem* it = item(de);
        if (!it) { de->ignore(); return true; }
        const QString id = it->id, title = it->title, from = it->markerId.isEmpty() ? QString() : groupTitle(it->markerId);
        m_link.model->setDrawerItemGroup(id, gid);
        if (m_link.save) m_link.save();
        if (flash) flash(from.isEmpty() ? tr("“%1” agora faz parte de %2.").arg(title, groupTitle(gid))
                                       : tr("“%1” saiu de %2 e entrou em %3.").arg(title, from, groupTitle(gid)));
        de->setDropAction(Qt::MoveAction);
        de->accept();
        m_dropDone = true;
        return true;
    }
    default:
        return false;
    }
}

bool Explorer::eventFilter(QObject* o, QEvent* e)
{
    // documento arrastado pra outro grupo
    if (auto* b = qobject_cast<QPushButton*>(o)) {
        if (b->property("xDragId").isValid()) return docDragEvent(b, e);
        if (b->property("xDropGroup").isValid()) return groupDropEvent(b, e);
    }
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
