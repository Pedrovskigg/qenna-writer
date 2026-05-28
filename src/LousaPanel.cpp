#include "LousaPanel.h"

#include "CardItem.h"
#include "ConnectionItem.h"
#include "IconUtils.h"
#include "ProjectModel.h"
#include "LousaScene.h"
#include "LousaView.h"
#include "Theme.h"

#include <QBuffer>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QDir>
#include <QFileDialog>
#include <QImage>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSaveFile>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

namespace {
constexpr int kToolbarH  = 46;
constexpr int kBtnSize   = 32;
constexpr int kIconSize  = 20;

QToolButton* makeIconBtn(const QString& tip, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setToolButtonStyle(Qt::ToolButtonIconOnly);
    b->setAutoRaise(true);
    b->setToolTip(tip);
    b->setObjectName(QStringLiteral("lousaToolBtn"));
    b->setFixedSize(kBtnSize, kBtnSize);
    b->setIconSize(QSize(kIconSize, kIconSize));
    b->setCursor(Qt::PointingHandCursor);
    b->setEnabled(false);
    return b;
}

QIcon loadLousaIcon(const QString& path)
{
    return IconUtils::loadToolbarIcon(
        path,
        QColor(Theme::textMuted()),
        QColor(Theme::textPrimary()),
        QColor(Theme::textBright()));
}
} // namespace

LousaPanel::LousaPanel(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("lousaPanel"));
    setWindowTitle(tr("Lousa"));
    setMinimumSize(640, 420);
    resize(960, 660);
    buildUi();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &LousaPanel::applyTheme);
}

void LousaPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar ──────────────────────────────────────────────────────────
    m_toolbar = new QWidget(this);
    m_toolbar->setObjectName(QStringLiteral("lousaToolbar"));
    m_toolbar->setFixedHeight(kToolbarH);

    auto* tl = new QHBoxLayout(m_toolbar);
    tl->setContentsMargins(10, 0, 10, 0);
    tl->setSpacing(4);

    // Grupo 1 — tipos de card
    auto* btnNote = makeIconBtn(tr("Post-it"),    m_toolbar);
    btnNote->setEnabled(true);
    auto* btnCmt  = makeIconBtn(tr("Comentário"), m_toolbar);
    btnCmt->setEnabled(true);
    auto* btnImg  = makeIconBtn(tr("Imagem"),     m_toolbar);
    btnImg->setEnabled(true);
    auto* btnDoc  = makeIconBtn(tr("Documento"),  m_toolbar);
    btnDoc->setEnabled(true);
    auto* btnChar = makeIconBtn(tr("Personagem"), m_toolbar);
    btnChar->setEnabled(true);
    tl->addWidget(btnNote);
    tl->addWidget(btnCmt);
    tl->addWidget(btnImg);
    tl->addWidget(btnDoc);
    tl->addWidget(btnChar);

    connect(btnNote, &QToolButton::clicked, this, [this]() {
        if (!m_scene) return;
        m_scene->addCard(nextCardData(QStringLiteral("note")));
        refreshEmptyState();
    });
    connect(btnCmt, &QToolButton::clicked, this, [this]() {
        if (!m_scene) return;
        m_scene->addCard(nextCardData(QStringLiteral("comment")));
        refreshEmptyState();
    });
    connect(btnImg, &QToolButton::clicked, this, [this]() {
        if (!m_scene) return;
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Escolher imagem"), QString(),
            tr("Imagens (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
        if (path.isEmpty()) return;
        QImage img(path);
        if (img.isNull()) return;
        if (img.width() > 900 || img.height() > 900)
            img = img.scaled(900, 900, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QByteArray ba;
        QBuffer buf(&ba);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "JPEG", 82);
        CanvasCard c = nextCardData(QStringLiteral("image"));
        c.content = QString::fromLatin1(ba.toBase64());
        m_scene->addCard(c);
        refreshEmptyState();
    });

    // ── Doc picker ──────────────────────────────────────────────────────────
    connect(btnDoc, &QToolButton::clicked, this, [this]() {
        if (!m_scene || !m_projectModel) return;
        // Coleta todos os itens com html de todas as gavetas
        struct Entry { QString drawerKey, drawerName, itemId, title, html; };
        QVector<Entry> entries;
        for (const Drawer& d : m_projectModel->drawers())
            for (const DrawerItem& it : d.items)
                entries.append({d.key, d.title, it.id, it.title, it.html});
        if (entries.isEmpty()) return;

        QDialog dlg(this);
        dlg.setWindowTitle(tr("Vincular documento"));
        dlg.resize(400, 340);
        auto* vl = new QVBoxLayout(&dlg);
        auto* search = new QLineEdit(&dlg);
        search->setPlaceholderText(tr("Buscar..."));
        vl->addWidget(search);
        auto* list = new QListWidget(&dlg);
        list->setAlternatingRowColors(true);
        auto populate = [&](const QString& q) {
            list->clear();
            const QString needle = q.trimmed().toLower();
            for (const Entry& e : entries) {
                if (!needle.isEmpty() && !e.title.toLower().contains(needle)
                    && !e.drawerName.toLower().contains(needle)) continue;
                auto* item = new QListWidgetItem(
                    QStringLiteral("%1 — %2").arg(e.title.isEmpty() ? tr("(sem título)") : e.title, e.drawerName));
                item->setData(Qt::UserRole, QVariant::fromValue<int>(int(&e - entries.data())));
                list->addItem(item);
            }
        };
        populate(QString());
        connect(search, &QLineEdit::textChanged, list, [&](const QString& t) { populate(t); });
        connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);
        vl->addWidget(list, 1);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        vl->addWidget(bb);
        if (dlg.exec() != QDialog::Accepted || !list->currentItem()) return;

        const int idx = list->currentItem()->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= entries.size()) return;
        const Entry& e = entries[idx];
        CanvasCard c = nextCardData(QStringLiteral("doc"));
        c.title = e.title;
        c.linkedDrawerKey = e.drawerKey;
        c.linkedItemId    = e.itemId;
        c.content         = e.html; // HTML em memória; não persiste no canvas.json
        m_scene->addCard(c);
        refreshEmptyState();
    });

    // ── Character picker ─────────────────────────────────────────────────────
    connect(btnChar, &QToolButton::clicked, this, [this]() {
        if (!m_scene || !m_projectModel) return;
        struct CEntry { QString drawerKey, drawerName, itemId, title, photo; };
        QVector<CEntry> entries;
        for (const Drawer& d : m_projectModel->drawers()) {
            if (d.drawerElementType != QStringLiteral("character")) continue;
            for (const DrawerItem& it : d.items)
                entries.append({d.key, d.title, it.id, it.title, it.role}); // role = photo url
        }
        // Fallback: all items with elementType=character
        if (entries.isEmpty()) {
            for (const Drawer& d : m_projectModel->drawers())
                for (const DrawerItem& it : d.items)
                    if (it.elementType == QStringLiteral("character"))
                        entries.append({d.key, d.title, it.id, it.title, it.role});
        }
        if (entries.isEmpty()) return;

        QDialog dlg(this);
        dlg.setWindowTitle(tr("Adicionar personagem"));
        dlg.resize(380, 320);
        auto* vl = new QVBoxLayout(&dlg);
        auto* search = new QLineEdit(&dlg);
        search->setPlaceholderText(tr("Buscar personagem..."));
        vl->addWidget(search);
        auto* list = new QListWidget(&dlg);
        auto populate = [&](const QString& q) {
            list->clear();
            const QString needle = q.trimmed().toLower();
            for (const CEntry& e : entries) {
                if (!needle.isEmpty() && !e.title.toLower().contains(needle)) continue;
                auto* item = new QListWidgetItem(e.title.isEmpty() ? tr("(sem nome)") : e.title);
                item->setData(Qt::UserRole, QVariant::fromValue<int>(int(&e - entries.data())));
                list->addItem(item);
            }
        };
        populate(QString());
        connect(search, &QLineEdit::textChanged, list, [&](const QString& t) { populate(t); });
        connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);
        vl->addWidget(list, 1);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        vl->addWidget(bb);
        if (dlg.exec() != QDialog::Accepted || !list->currentItem()) return;

        const int idx = list->currentItem()->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= entries.size()) return;
        const CEntry& e = entries[idx];
        CanvasCard c = nextCardData(QStringLiteral("character"));
        c.title           = e.title;
        c.linkedDrawerKey = e.drawerKey;
        c.linkedItemId    = e.itemId;
        c.photoDataUrl    = e.photo; // base64 da foto do personagem
        m_scene->addCard(c);
        refreshEmptyState();
    });

    m_iconBindings.append({btnNote, QStringLiteral(":/icons/canvasicons/newpost-it.svg")});
    m_iconBindings.append({btnCmt,  QStringLiteral(":/icons/canvasicons/new-comment.svg")});
    m_iconBindings.append({btnImg,  QStringLiteral(":/icons/canvasicons/new-image.svg")});
    m_iconBindings.append({btnDoc,  QStringLiteral(":/icons/canvasicons/link-doc.svg")});
    m_iconBindings.append({btnChar, QStringLiteral(":/icons/elements/user.svg")});

    // Separador
    auto* sep1 = new QFrame(m_toolbar);
    sep1->setObjectName(QStringLiteral("lousaSep"));
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFixedHeight(22);
    tl->addWidget(sep1);

    // Grupo 3 — zona (desabilitado)
    auto* btnZone = makeIconBtn(tr("Definir área"), m_toolbar);
    btnZone->setEnabled(true);
    btnZone->setCheckable(true);
    m_iconBindings.append({btnZone, QStringLiteral(":/icons/canvasicons/add-plan-area.svg")});
    tl->addWidget(btnZone);
    connect(btnZone, &QToolButton::toggled, this, [this, btnZone](bool on) {
        if (m_view) m_view->setPlanMode(on);
        if (!on) btnZone->setChecked(false);
    });


    tl->addStretch(1);

    // Cor do canvas
    m_colorBtn = new QToolButton(m_toolbar);
    m_colorBtn->setObjectName(QStringLiteral("lousaColorBtn"));
    m_colorBtn->setToolTip(tr("Cor do canvas"));
    m_colorBtn->setFixedSize(26, 26);
    m_colorBtn->setCursor(Qt::PointingHandCursor);
    connect(m_colorBtn, &QToolButton::clicked, this, &LousaPanel::onPickColor);
    tl->addWidget(m_colorBtn);

    // Separador
    auto* sep2 = new QFrame(m_toolbar);
    sep2->setObjectName(QStringLiteral("lousaSep"));
    sep2->setFrameShape(QFrame::VLine);
    sep2->setFixedHeight(22);
    tl->addWidget(sep2);

    // Label de zoom (decorativo)
    m_zoomLabel = new QLabel(QStringLiteral("100%"), m_toolbar);
    m_zoomLabel->setObjectName(QStringLiteral("lousaZoomLabel"));
    m_zoomLabel->setFixedWidth(44);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    tl->addWidget(m_zoomLabel);

    // Fechar
    auto* closeBtn = new QToolButton(m_toolbar);
    closeBtn->setObjectName(QStringLiteral("lousaCloseBtn"));
    closeBtn->setText(QStringLiteral("×"));
    closeBtn->setToolTip(tr("Fechar lousa"));
    closeBtn->setFixedSize(30, 30);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QToolButton::clicked, this, &LousaPanel::close);
    tl->addWidget(closeBtn);

    root->addWidget(m_toolbar);

    // ── Canvas ───────────────────────────────────────────────────────────
    m_scene = new LousaScene(this);
    m_view  = new LousaView(m_scene, this);
    root->addWidget(m_view, 1);

    connect(m_view, &LousaView::zoneDrawn, this, [this](const QRectF& r) {
        if (!m_scene) return;
        CanvasZone z;
        z.id     = QUuid::createUuid().toString(QUuid::WithoutBraces);
        z.x      = r.x();  z.y = r.y();
        z.width  = r.width(); z.height = r.height();
        z.title  = tr("Área");
        z.color  = QColor(QStringLiteral("#6ea8fe"));
        m_scene->addZone(z);
        refreshEmptyState();
    });
    connect(m_scene, &LousaScene::zoneDataChanged, this, [this]() { save(); });

    connect(m_view, &LousaView::zoomChanged, this, [this](qreal z) {
        if (m_zoomLabel) m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(z * 100)));
        save();
    });
    connect(m_scene, &LousaScene::cardDataChanged,       this, [this]() { save(); });
    connect(m_scene, &LousaScene::connectionDataChanged, this, [this]() { save(); });
    connect(m_scene, &LousaScene::pendingConnection, this,
            [this](const QString& fromId, const QString& toId) {
        // Paleta igual ao Mira 1 CONN_PALETTE
        static const QColor kConnPalette[] = {
            QColor(QStringLiteral("#ffffff")), QColor(QStringLiteral("#6ea8fe")),
            QColor(QStringLiteral("#f97316")), QColor(QStringLiteral("#34d399")),
            QColor(QStringLiteral("#f472b6")), QColor(QStringLiteral("#fbbf24")),
            QColor(QStringLiteral("#a78bfa")), QColor(QStringLiteral("#f87171")),
        };

        // Popup de cor da conexão — QDialog centrado na janela
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Nova conexão"));
        dlg.setFixedWidth(280);
        auto* vl = new QVBoxLayout(&dlg);
        vl->setContentsMargins(14, 12, 14, 12);
        vl->setSpacing(10);

        auto* title = new QLabel(tr("Cor da conexão"), &dlg);
        title->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: 700; opacity: 0.6; text-transform: uppercase; letter-spacing: 0.06em;"));
        vl->addWidget(title);

        // Grid 4×2 de cores
        auto* grid = new QWidget(&dlg);
        auto* gl   = new QHBoxLayout(grid);
        gl->setSpacing(6); gl->setContentsMargins(0,0,0,0);
        QColor selectedColor = kConnPalette[0];
        QList<QPushButton*> swatches;
        for (const QColor& c : kConnPalette) {
            auto* btn = new QPushButton(grid);
            btn->setFixedSize(28, 22);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; border: 1.5px solid rgba(0,0,0,0.25);"
                " border-radius: 4px; } "
                "QPushButton:checked { border: 2px solid #fff; }").arg(c.name()));
            btn->setCheckable(true);
            btn->setChecked(c == selectedColor);
            swatches.append(btn);
            connect(btn, &QPushButton::clicked, &dlg, [btn, c, &selectedColor, &swatches]() {
                selectedColor = c;
                for (auto* s : swatches) s->setChecked(false);
                btn->setChecked(true);
            });
            gl->addWidget(btn);
        }
        vl->addWidget(grid);

        auto* bb = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
        auto* createBtn = bb->addButton(tr("Criar"), QDialogButtonBox::AcceptRole);
        createBtn->setDefault(true);
        connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        vl->addWidget(bb);

        if (dlg.exec() != QDialog::Accepted) return;

        // Cria a conexão
        CanvasConnection conn;
        conn.id     = QUuid::createUuid().toString(QUuid::WithoutBraces);
        conn.fromId = fromId;
        conn.toId   = toId;
        conn.color  = selectedColor;
        if (m_scene) m_scene->addConnection(conn);
    });

    // ── Placeholder ──────────────────────────────────────────────────────
    m_emptyLabel = new QLabel(tr("Clique em + para adicionar um card à lousa"), this);
    m_emptyLabel->setObjectName(QStringLiteral("lousaEmpty"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_emptyLabel->setVisible(true);

    connect(m_scene, &QGraphicsScene::changed, this, [this](const QList<QRectF>&) {
        refreshEmptyState();
    });

    reloadIcons();
}

void LousaPanel::refreshEmptyState()
{
    if (!m_emptyLabel) return;
    const bool empty = m_scene->items().isEmpty();
    m_emptyLabel->setVisible(empty);
    if (empty) {
        m_emptyLabel->adjustSize();
        m_emptyLabel->move(
            (width()  - m_emptyLabel->width())  / 2,
            (height() - m_emptyLabel->height()) / 2 + kToolbarH / 2);
    }
}

void LousaPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    refreshEmptyState();
}

void LousaPanel::closeEvent(QCloseEvent* event)
{
    // Esconde em vez de destruir — o estado fica vivo entre aberturas.
    hide();
    event->ignore();
    emit closeRequested();
}

void LousaPanel::setProjectModel(ProjectModel* model)
{
    m_projectModel = model;
}

void LousaPanel::refreshDocCards()
{
    if (!m_scene || !m_projectModel) return;
    for (CardItem* item : m_scene->cardItems()) {
        const CanvasCard d = item->cardData();
        if (d.type != QStringLiteral("doc") && d.type != QStringLiteral("character")) continue;
        const DrawerItem* di = m_projectModel->findDrawerItem(d.linkedItemId);
        if (!di) continue;
        if (d.type == QStringLiteral("doc"))
            item->setLinkedHtml(di->html);
        else
            item->setLinkedHtml(di->html); // photo já está em photoDataUrl
    }
}

void LousaPanel::setProjectRoot(const QString& root)
{
    if (m_projectRoot == root) return;
    m_projectRoot = root;
    load();
}

static CanvasCard cardFromJson(const QJsonObject& o)
{
    CanvasCard c;
    c.id      = o.value(QStringLiteral("id")).toString();
    c.type    = o.value(QStringLiteral("type")).toString(QStringLiteral("note"));
    c.x       = o.value(QStringLiteral("x")).toDouble();
    c.y       = o.value(QStringLiteral("y")).toDouble();
    c.width   = o.value(QStringLiteral("width")).toDouble(200);
    c.height  = o.value(QStringLiteral("height")).toDouble(160);
    c.title   = o.value(QStringLiteral("title")).toString();
    c.content = o.value(QStringLiteral("content")).toString();
    c.color   = QColor(o.value(QStringLiteral("color")).toString(QStringLiteral("#ffd060")));
    if (!c.color.isValid()) c.color = QColor(QStringLiteral("#ffd060"));
    c.description     = o.value(QStringLiteral("description")).toString();
    c.linkedItemId    = o.value(QStringLiteral("linkedItemId")).toString();
    c.linkedDrawerKey = o.value(QStringLiteral("linkedDrawerName")).toString();
    return c;
}

static QJsonObject cardToJson(const CanvasCard& c)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"),              c.id);
    o.insert(QStringLiteral("type"),            c.type);
    o.insert(QStringLiteral("x"),               c.x);
    o.insert(QStringLiteral("y"),               c.y);
    o.insert(QStringLiteral("width"),           c.width);
    o.insert(QStringLiteral("height"),          c.height);
    o.insert(QStringLiteral("title"),           c.title);
    o.insert(QStringLiteral("content"),         c.content);
    o.insert(QStringLiteral("color"),           c.color.name());
    o.insert(QStringLiteral("description"),      c.description);
    o.insert(QStringLiteral("linkedItemId"),    c.linkedItemId);
    o.insert(QStringLiteral("linkedDrawerName"), c.linkedDrawerKey);
    return o;
}

void LousaPanel::load()
{
    if (m_projectRoot.isEmpty() || !m_scene || !m_view) return;
    const QString path = QDir::cleanPath(m_projectRoot + QStringLiteral("/canvas.json"));
    QFile f(path);
    if (!f.exists()) return;
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    const QColor color(root.value(QStringLiteral("canvasColor")).toString(QStringLiteral("#1a1a2e")));
    m_scene->setCanvasColor(color.isValid() ? color : QColor(QStringLiteral("#1a1a2e")));
    updateColorBtn();

    const qreal zoom = root.value(QStringLiteral("zoom")).toDouble(1.0);
    const qreal panX = root.value(QStringLiteral("panX")).toDouble(0.0);
    const qreal panY = root.value(QStringLiteral("panY")).toDouble(0.0);
    m_view->applyZoomAndPan(zoom, panX, panY);
    if (m_zoomLabel) m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(zoom * 100)));

    m_scene->clearCards();
    m_scene->clearConnections();
    m_scene->clearZones();
    for (const auto& v : root.value(QStringLiteral("cards")).toArray()) {
        const CanvasCard c = cardFromJson(v.toObject());
        if (!c.id.isEmpty()) m_scene->addCard(c);
    }
    for (const auto& v : root.value(QStringLiteral("connections")).toArray()) {
        const QJsonObject o = v.toObject();
        CanvasConnection conn;
        conn.id     = o.value(QStringLiteral("id")).toString();
        conn.fromId = o.value(QStringLiteral("fromId")).toString();
        conn.toId   = o.value(QStringLiteral("toId")).toString();
        conn.color  = QColor(o.value(QStringLiteral("color")).toString(QStringLiteral("#ffffff")));
        for (const auto& wv : o.value(QStringLiteral("waypointCardIds")).toArray())
            conn.waypointCardIds.append(wv.toString());
        if (!conn.id.isEmpty() && !conn.fromId.isEmpty() && !conn.toId.isEmpty())
            m_scene->addConnection(conn);
    }
    for (const auto& v : root.value(QStringLiteral("zones")).toArray()) {
        const QJsonObject o = v.toObject();
        CanvasZone z;
        z.id     = o.value(QStringLiteral("id")).toString();
        z.x      = o.value(QStringLiteral("x")).toDouble();
        z.y      = o.value(QStringLiteral("y")).toDouble();
        z.width  = o.value(QStringLiteral("width")).toDouble(300);
        z.height = o.value(QStringLiteral("height")).toDouble(200);
        z.title  = o.value(QStringLiteral("title")).toString();
        z.color  = QColor(o.value(QStringLiteral("color")).toString(QStringLiteral("#6ea8fe")));
        if (!z.id.isEmpty()) m_scene->addZone(z);
    }
    refreshEmptyState();
}

void LousaPanel::save() const
{
    if (m_projectRoot.isEmpty() || !m_scene || !m_view) return;

    QJsonArray cards;
    for (const CanvasCard& c : m_scene->allCardData())
        cards.append(cardToJson(c));

    QJsonObject root;
    root.insert(QStringLiteral("canvasColor"), m_scene->canvasColor().name());
    root.insert(QStringLiteral("zoom"), m_view->zoomFactor());
    root.insert(QStringLiteral("panX"), m_view->scrollPos().x());
    root.insert(QStringLiteral("panY"), m_view->scrollPos().y());
    root.insert(QStringLiteral("cards"),       cards);
    QJsonArray connections;
    for (const CanvasConnection& conn : m_scene->allConnectionData()) {
        QJsonObject o;
        o.insert(QStringLiteral("id"),     conn.id);
        o.insert(QStringLiteral("fromId"), conn.fromId);
        o.insert(QStringLiteral("toId"),   conn.toId);
        o.insert(QStringLiteral("color"),  conn.color.name());
        QJsonArray wpts;
        for (const QString& wid : conn.waypointCardIds) wpts.append(wid);
        o.insert(QStringLiteral("waypointCardIds"), wpts);
        connections.append(o);
    }
    root.insert(QStringLiteral("connections"), connections);
    QJsonArray zones;
    for (const CanvasZone& z : m_scene->allZoneData()) {
        QJsonObject o;
        o.insert(QStringLiteral("id"),     z.id);
        o.insert(QStringLiteral("x"),      z.x);
        o.insert(QStringLiteral("y"),      z.y);
        o.insert(QStringLiteral("width"),  z.width);
        o.insert(QStringLiteral("height"), z.height);
        o.insert(QStringLiteral("title"),  z.title);
        o.insert(QStringLiteral("color"),  z.color.name());
        zones.append(o);
    }
    root.insert(QStringLiteral("zones"), zones);

    QSaveFile f(QDir::cleanPath(m_projectRoot + QStringLiteral("/canvas.json")));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.commit();
}

CanvasCard LousaPanel::nextCardData(const QString& type) const
{
    // Posiciona o novo card no centro da viewport.
    const QPointF center = m_view
        ? m_view->mapToScene(m_view->viewport()->rect().center())
        : QPointF(100, 100);

    CanvasCard c;
    c.id   = QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.type = type;

    // Defaults por tipo (iguais ao Mira 1 CARD_DEFAULTS)
    if (type == QStringLiteral("comment")) {
        c.width  = 220; c.height = 140;
        c.color  = QColor(QStringLiteral("#a78bfa"));
    } else if (type == QStringLiteral("image")) {
        c.width  = 220; c.height = 200;
        c.color  = QColor(QStringLiteral("#34d399"));
    } else if (type == QStringLiteral("doc")) {
        c.width  = 240; c.height = 220;
        c.color  = QColor(QStringLiteral("#60a5fa"));
    } else if (type == QStringLiteral("character")) {
        c.width  = 160; c.height = 200;
        c.color  = QColor(QStringLiteral("#f97316"));
    } else { // note (default)
        c.width  = 200; c.height = 170;
        c.color  = QColor(QStringLiteral("#ffd060"));
    }
    c.x = center.x() - c.width  / 2.0;
    c.y = center.y() - c.height / 2.0;
    return c;
}

void LousaPanel::reloadIcons()
{
    for (const auto& [btn, path] : m_iconBindings)
        if (btn) btn->setIcon(loadLousaIcon(path));
}

void LousaPanel::onPickColor()
{
    if (!m_scene) return;
    const QColor chosen = QColorDialog::getColor(
        m_scene->canvasColor(), this, tr("Cor do canvas"),
        QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid()) return;
    m_scene->setCanvasColor(chosen);
    updateColorBtn();
    save();
}

void LousaPanel::updateColorBtn()
{
    if (!m_colorBtn || !m_scene) return;
    const QColor c = m_scene->canvasColor();
    m_colorBtn->setStyleSheet(QStringLiteral(
        "QToolButton#lousaColorBtn {"
        "  background: %1; border: 2px solid %2; border-radius: 5px;"
        "}"
        "QToolButton#lousaColorBtn:hover { border-color: %3; }"
    ).arg(c.name(), Theme::panelBorder(), Theme::textPrimary()));
}

void LousaPanel::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QWidget#lousaPanel   { background: transparent; }"
        "QWidget#lousaToolbar { background: %1; border-bottom: 1px solid %2; }"
        "QToolButton#lousaToolBtn {"
        "  background: transparent; border: 1px solid transparent;"
        "  border-radius: 6px; color: %3; padding: 4px 8px; font-size: 11px;"
        "}"
        "QToolButton#lousaToolBtn:enabled:hover { background: %4; border-color: %5; }"
        "QToolButton#lousaToolBtn:disabled { color: %6; }"
        "QToolButton#lousaCloseBtn {"
        "  background: transparent; border: none; color: %3; font-size: 18px;"
        "}"
        "QToolButton#lousaCloseBtn:hover { color: %7; }"
        "QFrame#lousaSep { color: %2; background: %2; }"
        "QLabel#lousaZoomLabel { color: %6; font-size: 11px; }"
        "QLabel#lousaEmpty { color: %6; font-size: 14px; }"
    ).arg(Theme::panelBackground(),   // 1
         Theme::panelBorder(),        // 2
         Theme::textPrimary(),        // 3
         Theme::hoverOverlay(),       // 4
         Theme::subtleBorder(),       // 5
         Theme::textMuted(),          // 6
         Theme::accentDanger()));     // 7

    reloadIcons();
    updateColorBtn();

    if (m_view) {
        m_view->setStyleSheet(QStringLiteral(
            "QGraphicsView { border: none; background: transparent; }"));
    }
}
