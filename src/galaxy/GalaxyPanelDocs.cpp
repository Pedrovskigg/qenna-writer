// Os documentos: moram no projeto de verdade. "Salvar em" escolhe uma gaveta (item com o grupo do lugar) ou o
// Criador de Mundos (um Lugar com o nome da galáxia, com pastas pelo caminho: região, sistema, planeta).
// O documento só nasce no projeto quando ganha título ou texto; vazio some ao voltar.

#include "GalaxyPanel.h"

#include "../DocCache.h"
#include "../DocPreview.h"
#include "../ProjectModel.h"
#include "../TerritorioStore.h"
#include "../carto/CartoIcons.h"
#include "../Theme.h"

#include <QBuffer>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace galaxy {

namespace {
const TerritorioStore::Node* findNode(const QList<TerritorioStore::Node>& nodes, const QString& id)
{
    for (const auto& n : nodes) {
        if (n.id == id) return &n;
        if (const auto* c = findNode(n.children, id)) return c;
    }
    return nullptr;
}
const TerritorioStore::Node* folderNamed(const QList<TerritorioStore::Node>& nodes, const QString& name)
{
    for (const auto& n : nodes) if (n.type == TerritorioStore::NodeType::Folder && n.name == name) return &n;
    return nullptr;
}
}

DocLink* Panel::newDocFor(const QString& k)
{
    auto d = std::make_unique<DocLink>();
    d->id = m_doc.seq++;
    d->owner = k;
    d->title = tr("Documento sem título");
    if (m_lastDest.startsWith(QStringLiteral("g:"))) { d->gaveta = true; d->drawer = m_lastDest.mid(2); }
    DocLink* raw = d.get();
    m_doc.docs.push_back(std::move(d));
    scheduleSave();
    return raw;
}
QString Panel::docHtml(const DocLink* d) const
{
    if (d->gaveta && !d->itemId.isEmpty() && m_link.model) {
        if (m_link.syncEditor) m_link.syncEditor();
        const DrawerItem* it = m_link.model->findDrawerItem(d->itemId);
        if (it) return DocPreview::resolveDrawerItemHtml(it, m_link.elements, m_link.cache, m_link.root, false);
    }
    if (!d->gaveta && !d->nodeId.isEmpty() && m_link.places) {
        if (const auto* t = m_link.places->territorio(d->placeId)) if (const auto* n = findNode(t->nodes, d->nodeId)) return n->content;
    }
    return d->html;
}
QString Panel::docPlain(const DocLink* d) const
{
    QTextDocument td;
    td.setHtml(docHtml(d));
    return td.toPlainText().simplified();
}
QString Panel::docDestLabel(const DocLink* d)
{
    if (d->gaveta) {
        if (m_link.model) for (const Drawer& dr : m_link.model->drawers()) if (dr.key == d->drawer) return tr("Gaveta %1").arg(dr.title);
        return tr("Gaveta");
    }
    QStringList parts { tr("Criador de Mundos") };
    for (const QString& x : m_doc.pathOf(d->owner)) if (x != QLatin1String("GAL")) parts << m_doc.labelOf(x);
    return parts.join(QStringLiteral(" › "));
}
// o Lugar da galáxia e as pastas do caminho até o dono do documento
QString Panel::placeIdFor(const QString& owner, QString* folderId)
{
    if (!m_link.places) return QString();
    const QString gname = m_doc.labelOf(QStringLiteral("GAL"));
    QString pid;
    for (const auto& t : m_link.places->territorios()) if (t.name.trimmed().compare(gname.trimmed(), Qt::CaseInsensitive) == 0) pid = t.id;
    if (pid.isEmpty()) pid = m_link.places->addTerritorio(gname);
    QString parent;
    for (const QString& x : m_doc.pathOf(owner)) {
        if (x == QLatin1String("GAL")) continue;
        const auto* t = m_link.places->territorio(pid);
        if (!t) break;
        const QList<TerritorioStore::Node>* level = &t->nodes;
        if (!parent.isEmpty()) { const auto* p = findNode(t->nodes, parent); if (!p) break; level = &p->children; }
        const QString name = m_doc.labelOf(x);
        if (const auto* f = folderNamed(*level, name)) parent = f->id;
        else parent = m_link.places->addNode(pid, parent, TerritorioStore::NodeType::Folder, name);
    }
    if (folderId) *folderId = parent;
    return pid;
}
// O documento ganhou título ou texto: agora ele existe no projeto.
void Panel::docMaterialize(DocLink* d)
{
    if (!m_link.ok()) return;
    if (d->gaveta) {
        if (!d->itemId.isEmpty()) return;
        if (d->drawer.isEmpty() && !m_link.model->drawers().isEmpty()) d->drawer = m_link.model->drawers().first().key;
        DrawerItem it;
        it.id = ProjectModel::uid();
        it.title = d->title;
        it.hasInlineHtml = true;
        it.html = QStringLiteral("<p></p>");
        if (!m_link.model->addDrawerItem(d->drawer, it)) return;
        d->itemId = it.id;
        if (m_link.save) m_link.save();
    } else {
        if (!d->nodeId.isEmpty()) return;
        QString folder;
        d->placeId = placeIdFor(d->owner, &folder);
        if (d->placeId.isEmpty()) return;
        d->nodeId = m_link.places->addNode(d->placeId, folder, TerritorioStore::NodeType::Doc, d->title);
        m_link.places->save();
    }
    scheduleSave();
}
void Panel::docWriteHtml(DocLink* d, const QString& html)
{
    docMaterialize(d);
    d->html = html;
    if (d->gaveta && !d->itemId.isEmpty()) {
        if (m_link.syncEditor) m_link.syncEditor();
        if (m_link.cache) m_link.cache->set(DocCache::itemKey(d->itemId), html, true);
        if (m_link.itemEdited) m_link.itemEdited(d->itemId);
        if (m_link.save) m_link.save();
    } else if (!d->gaveta && !d->nodeId.isEmpty()) {
        m_link.places->updateNode(d->placeId, d->nodeId, d->title, html);
        m_link.places->save();
    }
    scheduleSave();
}
void Panel::docSetTitle(DocLink* d, const QString& title)
{
    d->title = title.isEmpty() ? tr("Documento sem título") : title;
    docMaterialize(d);
    if (d->gaveta && !d->itemId.isEmpty()) {
        m_link.model->updateDrawerItemMeta(d->itemId, d->title, QString());
        if (m_link.save) m_link.save();
    } else if (!d->gaveta && !d->nodeId.isEmpty()) {
        m_link.places->updateNode(d->placeId, d->nodeId, d->title, docHtml(d));
        m_link.places->save();
    }
    scheduleSave();
}
// Trocar o "Salvar em": o texto vai junto pro lugar novo.
void Panel::docMoveTo(DocLink* d, const QString& dest)
{
    const bool wasReal = d->gaveta ? !d->itemId.isEmpty() : !d->nodeId.isEmpty();
    const QString html = wasReal ? docHtml(d) : d->html;
    if (wasReal) {
        if (d->gaveta) { m_link.model->removeDrawerItem(d->itemId); if (m_link.save) m_link.save(); }
        else { m_link.places->removeNode(d->placeId, d->nodeId); m_link.places->save(); }
    }
    d->itemId.clear(); d->nodeId.clear(); d->placeId.clear();
    d->gaveta = dest.startsWith(QStringLiteral("g:"));
    d->drawer = d->gaveta ? dest.mid(2) : QString();
    m_lastDest = dest;
    if (wasReal) docWriteHtml(d, html);
    scheduleSave();
}
// Documento que nunca ganhou nada não fica pra trás.
void Panel::docDropIfEmpty(DocLink* d)
{
    if (!d) return;
    const bool real = d->gaveta ? !d->itemId.isEmpty() : !d->nodeId.isEmpty();
    if (real || !d->html.isEmpty() || d->title != tr("Documento sem título")) return;
    for (size_t i = 0; i < m_doc.docs.size(); i++) if (m_doc.docs[i].get() == d) { m_doc.docs.erase(m_doc.docs.begin() + long(i)); break; }
    scheduleSave();
}

QWidget* Panel::editorFor(DocLink* d, const std::function<void()>& back, QWidget* extraBar)
{
    const Ref o = m_doc.resolve(d->owner);
    auto* f = new QWidget();
    auto* v = new QVBoxLayout(f);
    v->setContentsMargins(16, 12, 16, 16);
    v->setSpacing(8);
    auto* bar = new QHBoxLayout();
    auto* backB = new QPushButton(QStringLiteral("‹ %1").arg(m_doc.labelOf(d->owner)), f);
    backB->setObjectName(QStringLiteral("gmLink"));
    backB->setCursor(Qt::PointingHandCursor);
    backB->setFocusPolicy(Qt::NoFocus);
    bar->addWidget(backB);
    bar->addStretch();
    if (extraBar) { extraBar->setParent(f); bar->addWidget(extraBar); }
    v->addLayout(bar);
    auto* where = new QPushButton(f);
    where->setObjectName(QStringLiteral("gmItem"));
    where->setCursor(Qt::PointingHandCursor);
    where->setFocusPolicy(Qt::NoFocus);
    where->setFixedHeight(46);
    auto* wh = new QHBoxLayout(where);
    wh->setContentsMargins(4, 4, 4, 4);
    wh->setSpacing(10);
    QWidget* ic = iconEl(d->owner, 30);
    ic->setParent(where);
    wh->addWidget(ic);
    auto* wc = new QVBoxLayout();
    wc->setSpacing(0);
    auto* wn = new QLabel(m_doc.labelOf(d->owner), where);
    wn->setObjectName(QStringLiteral("cartoValue"));
    wn->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* ws = new QLabel(subShort(d->owner, o), where);
    ws->setObjectName(QStringLiteral("cartoNote"));
    ws->setAttribute(Qt::WA_TransparentForMouseEvents);
    wc->addWidget(wn); wc->addWidget(ws);
    wh->addLayout(wc, 1);
    const QString owner = d->owner;
    connect(where, &QPushButton::clicked, this, [this, owner]() { double x, y, z; m_canvas->frameOfKey(owner, x, y, z); m_canvas->flyTo(x, y, z); });
    v->addWidget(where);
    auto* title = new QLineEdit(d->title == tr("Documento sem título") ? QString() : d->title, f);
    title->setObjectName(QStringLiteral("gmEdTitle"));
    title->setPlaceholderText(tr("Documento sem título"));
    v->addWidget(title);
    auto* meta = new QHBoxLayout();
    meta->setSpacing(8);
    auto* mc = new QLabel(tr("Salvar em"), f);
    mc->setObjectName(QStringLiteral("gmEditLabel"));
    meta->addWidget(mc);
    auto* dest = new QComboBox(f);
    dest->setObjectName(QStringLiteral("cartoCombo"));
    if (m_link.model) for (const Drawer& dr : m_link.model->drawers()) dest->addItem(tr("Gaveta %1").arg(dr.title), QStringLiteral("g:") + dr.key);
    dest->addItem(tr("Criador de Mundos"), QStringLiteral("criador"));
    dest->setCurrentIndex(std::max(0, dest->findData(d->gaveta ? QStringLiteral("g:") + d->drawer : QStringLiteral("criador"))));
    meta->addWidget(dest);
    auto* whereTxt = new QLabel(docDestLabel(d), f);
    whereTxt->setObjectName(QStringLiteral("cartoNote"));
    whereTxt->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    meta->addWidget(whereTxt, 1);
    v->addLayout(meta);
    auto* box = new QFrame(f);
    box->setObjectName(QStringLiteral("gmEdBox"));
    auto* bl = new QVBoxLayout(box);
    bl->setContentsMargins(6, 6, 6, 6);
    bl->setSpacing(4);
    auto* tools = new QHBoxLayout();
    tools->setSpacing(2);
    auto* body = new QTextEdit(box);
    body->setObjectName(QStringLiteral("gmEdBody"));
    body->setFrameShape(QFrame::NoFrame);
    body->setPlaceholderText(tr("Comece a escrever sobre %1…").arg(m_doc.labelOf(d->owner)));
    body->setMinimumHeight(300);
    body->setAcceptRichText(true);
    const auto fmt = [&](const QString& label, const QString& tip, const std::function<void()>& fn) {
        auto* b = new QToolButton(box);
        b->setObjectName(QStringLiteral("gmFmt"));
        b->setText(label);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        connect(b, &QToolButton::clicked, this, fn);
        tools->addWidget(b);
        return b;
    };
    fmt(QStringLiteral("B"), tr("Negrito (Ctrl+B)"), [body]() { body->setFontWeight(body->fontWeight() >= QFont::Bold ? QFont::Normal : QFont::Bold); body->setFocus(); });
    fmt(QStringLiteral("I"), tr("Itálico (Ctrl+I)"), [body]() { body->setFontItalic(!body->fontItalic()); body->setFocus(); });
    fmt(QStringLiteral("U"), tr("Sublinhado (Ctrl+U)"), [body]() { body->setFontUnderline(!body->fontUnderline()); body->setFocus(); });
    auto* imgB = fmt(tr("Imagem"), tr("Imagem"), [this, body, d]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Imagem"), QString(), tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (path.isEmpty()) return;
        QImage img(path);
        if (img.isNull()) return;
        if (img.width() > 1400) img = img.scaledToWidth(1400, Qt::SmoothTransformation);
        QString src;
        if (d->gaveta && !m_link.root.isEmpty()) {
            // documento de gaveta: a imagem vai pra pasta imagens/ do projeto, como no editor
            QDir().mkpath(m_link.root + QStringLiteral("/imagens"));
            const QString file = m_link.root + QStringLiteral("/imagens/") + QFileInfo(path).completeBaseName() + QLatin1Char('-') + ProjectModel::uid().left(8) + QStringLiteral(".jpg");
            if (img.save(file, "JPG", 88)) src = QUrl::fromLocalFile(file).toString();
        }
        if (src.isEmpty()) {
            QByteArray bytes; QBuffer buf(&bytes); buf.open(QIODevice::WriteOnly); img.save(&buf, "JPG", 86);
            src = QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(bytes.toBase64());
            body->document()->addResource(QTextDocument::ImageResource, QUrl(src), img);
        }
        QTextImageFormat tf;
        tf.setName(src);
        const double w = std::min<double>(img.width(), body->viewport()->width() - 40);
        tf.setWidth(w); tf.setHeight(img.height() * w / img.width());
        body->textCursor().insertImage(tf);
    });
    imgB->setIcon(carto::icon(QStringLiteral("image"), Theme::toColor(Theme::textMuted()), Theme::toColor(Theme::accentDefault()), 14));
    imgB->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tools->addStretch();
    bl->addLayout(tools);
    {
        const QString html = docHtml(d);
        if (!html.isEmpty()) body->setHtml(html);
        else if (!d->text.isEmpty()) body->setPlainText(d->text);
    }
    bl->addWidget(body, 1);
    v->addWidget(box, 1);

    // grava com uma espera curta, como o editor
    auto* timer = new QTimer(f);
    timer->setSingleShot(true);
    timer->setInterval(600);
    connect(timer, &QTimer::timeout, this, [this, d, body]() {
        const bool empty = body->toPlainText().trimmed().isEmpty() && !body->toHtml().contains(QStringLiteral("<img"));
        docWriteHtml(d, empty ? QString() : body->toHtml());
    });
    connect(body, &QTextEdit::textChanged, timer, qOverload<>(&QTimer::start));
    connect(title, &QLineEdit::textEdited, this, [this, d, title]() { docSetTitle(d, title->text().trimmed()); });
    connect(dest, &QComboBox::activated, this, [this, d, dest, whereTxt, timer, body]() {
        if (timer->isActive()) { timer->stop(); docWriteHtml(d, body->toHtml()); }
        docMoveTo(d, dest->currentData().toString());
        whereTxt->setText(docDestLabel(d));
    });
    connect(backB, &QPushButton::clicked, this, [this, timer, body, d, back]() {
        if (timer->isActive()) { timer->stop(); docWriteHtml(d, body->toHtml()); }
        back();
    });
    QTimer::singleShot(0, d->title == tr("Documento sem título") ? static_cast<QWidget*>(title) : static_cast<QWidget*>(body), [title, body, d, this]() { if (d->title == tr("Documento sem título")) title->setFocus(); else body->setFocus(); });
    return f;
}

} // namespace galaxy
