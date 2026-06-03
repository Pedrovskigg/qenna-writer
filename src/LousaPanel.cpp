#include "LousaPanel.h"

#include "CardItem.h"
#include "ConnectionItem.h"
#include "ElementCreateDialog.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "ProjectModel.h"
#include "LousaScene.h"
#include "LousaView.h"
#include "Theme.h"

#include <QBuffer>
#include <QCloseEvent>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QRegularExpression>
#include <QDir>
#include <QFileDialog>
#include <QImage>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSaveFile>
#include <QScrollArea>
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
    tl->setContentsMargins(12, 0, 12, 0);
    tl->setSpacing(8);

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
    auto* btnText = makeIconBtn(tr("Texto livre"), m_toolbar);
    btnText->setEnabled(true);
    auto* btnSym  = makeIconBtn(tr("Símbolo"), m_toolbar);
    btnSym->setEnabled(true);
    m_btnNote = btnNote; m_btnCmt = btnCmt; m_btnImg = btnImg;
    m_btnDoc = btnDoc;   m_btnChar = btnChar; m_btnText = btnText;
    tl->addWidget(btnNote);
    tl->addWidget(btnCmt);
    tl->addWidget(btnImg);
    tl->addWidget(btnDoc);
    tl->addWidget(btnChar);
    tl->addWidget(btnText);
    tl->addWidget(btnSym);

    connect(btnText, &QToolButton::clicked, this, [this]() {
        if (!m_scene) return;
        // Mesmo fluxo do símbolo: popup de texto + cor + fonte; o card nasce pronto.
        QString txt;
        QColor  col = QColor(QStringLiteral("#ffffff"));
        QString fam;
        if (!CardItem::pickText(this, txt, col, fam)) return; // cancelou → não cria
        pushUndo();
        CanvasCard c = nextCardData(QStringLiteral("text"));
        c.content = txt;
        if (col.isValid()) c.color = col;
        c.fontFamily = fam;
        m_scene->addCard(c);
        refreshEmptyState();
    });
    connect(btnSym, &QToolButton::clicked, this, [this]() {
        if (!m_scene) return;
        // Fluxo Mira 2: já abre o popup de símbolo + cor; o card nasce pronto.
        QString sym = QStringLiteral("★");
        QColor  col = QColor(QStringLiteral("#ffffff"));
        if (!CardItem::pickSymbol(this, sym, col)) return; // cancelou → não cria
        pushUndo();
        CanvasCard c = nextCardData(QStringLiteral("symbol"));
        c.content = sym;
        if (col.isValid()) c.color = col;
        m_scene->addCard(c);
        refreshEmptyState();
    });

    connect(btnNote, &QToolButton::clicked, this, [this]() {
        if (!m_scene) return;
        pushUndo();
        m_scene->addCard(nextCardData(QStringLiteral("note")));
        refreshEmptyState();
    });
    connect(btnCmt, &QToolButton::clicked, this, [this]() {
        if (!m_scene) return;
        pushUndo();
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
        pushUndo();
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
        pushUndo();
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
        struct CEntry { QString drawerKey, drawerName, itemId, title, elementId, html; };
        QVector<CEntry> entries;
        // Coleta personagens das gavetas de tipo character
        for (const Drawer& d : m_projectModel->drawers()) {
            const bool isChar = (d.drawerElementType == QStringLiteral("character"));
            for (const DrawerItem& it : d.items) {
                if (!isChar && it.elementType != QStringLiteral("character")) continue;
                entries.append({d.key, d.title, it.id, it.title, it.elementId, it.html});
            }
        }

        QDialog dlg(this);
        dlg.setWindowTitle(tr("Personagem na lousa"));
        dlg.resize(400, 360);
        auto* vl = new QVBoxLayout(&dlg);
        vl->setSpacing(8);

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
        connect(search, &QLineEdit::textChanged, this, [&](const QString& t) { populate(t); });
        connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);
        vl->addWidget(list, 1);

        // Botão "Novo personagem" + Ok/Cancel
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        auto* newCharBtn = new QPushButton(tr("+ Novo personagem"), &dlg);
        newCharBtn->setCursor(Qt::PointingHandCursor);
        bb->addButton(newCharBtn, QDialogButtonBox::ResetRole);

        connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        // "Novo personagem" — abre o ElementCreateDialog completo (role, foto, narrador)
        connect(newCharBtn, &QPushButton::clicked, this, [&]() {
            // Encontra o primeiro drawer de personagens
            QString targetDrawerKey;
            for (const Drawer& d : m_projectModel->drawers()) {
                if (d.drawerElementType == QStringLiteral("character")) {
                    targetDrawerKey = d.key; break;
                }
            }
            if (targetDrawerKey.isEmpty()) {
                QMessageBox::information(&dlg, tr("Sem gaveta de personagens"),
                    tr("Crie primeiro uma gaveta de personagens no projeto."));
                return;
            }

            // Abre o dialog completo com foto, papel, narrador
            ElementCreateDialog edlg(QStringLiteral("character"), &dlg);
            if (edlg.exec() != QDialog::Accepted || edlg.title().trimmed().isEmpty()) return;

            // Cria elemento no ElementsStore
            Element elem;
            elem.name     = edlg.title().trimmed();
            elem.type     = QStringLiteral("character");
            elem.icon     = QStringLiteral("user");
            elem.role     = edlg.role();
            elem.image    = edlg.imageDataUrl();
            elem.narrator = edlg.narrator();
            elem.aliases  = edlg.aliases();
            const QString elementId = m_elementsStore ? m_elementsStore->addElement(elem) : QString();

            // Cria DrawerItem vinculado ao elemento
            DrawerItem newItem;
            newItem.id           = ProjectModel::uid();
            newItem.title        = elem.name;
            newItem.hasInlineHtml = true;
            newItem.html         = QStringLiteral("<p></p>");
            newItem.elementType  = QStringLiteral("character");
            newItem.elementId    = elementId;
            newItem.role         = elem.role;
            m_projectModel->addDrawerItem(targetDrawerKey, newItem);

            // Cria e adiciona o card direto na lousa
            pushUndo();
            CanvasCard c = nextCardData(QStringLiteral("character"));
            c.title           = elem.name;
            c.linkedDrawerKey = targetDrawerKey;
            c.linkedItemId    = newItem.id;
            c.photoDataUrl    = elem.image;
            m_scene->addCard(c);
            refreshEmptyState();

            // Fecha o picker — card já está na lousa
            dlg.accept();
        });

        vl->addWidget(bb);
        if (dlg.exec() != QDialog::Accepted || !list->currentItem()) return;

        const int idx = list->currentItem()->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= entries.size()) return;
        const CEntry& e = entries[idx];

        // Busca a foto do personagem via ElementsStore (Element::image = data URL)
        QString photoDataUrl;
        if (m_elementsStore && !e.elementId.isEmpty()) {
            if (const Element* el = m_elementsStore->findElement(e.elementId))
                photoDataUrl = el->image; // data:image/jpeg;base64,...
        }

        pushUndo();
        CanvasCard c = nextCardData(QStringLiteral("character"));
        c.title           = e.title;
        c.linkedDrawerKey = e.drawerKey;
        c.linkedItemId    = e.itemId;
        c.photoDataUrl    = photoDataUrl;
        c.content         = e.html; // HTML do doc do personagem (para overlay no double-click)
        m_scene->addCard(c);
        refreshEmptyState();
    });

    m_iconBindings.append({btnNote, QStringLiteral(":/icons/canvasicons/newpost-it.svg")});
    m_iconBindings.append({btnCmt,  QStringLiteral(":/icons/canvasicons/new-comment.svg")});
    m_iconBindings.append({btnImg,  QStringLiteral(":/icons/canvasicons/new-image.svg")});
    m_iconBindings.append({btnDoc,  QStringLiteral(":/icons/canvasicons/link-doc.svg")});
    m_iconBindings.append({btnChar, QStringLiteral(":/icons/elements/user.svg")});
    m_iconBindings.append({btnText, QStringLiteral(":/icons/canvasicons/add-text.svg")});
    m_iconBindings.append({btnSym,  QStringLiteral(":/icons/canvasicons/add-symbol.svg")});

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
    m_btnZone = btnZone;
    m_iconBindings.append({btnZone, QStringLiteral(":/icons/canvasicons/add-plan-area.svg")});
    tl->addWidget(btnZone);
    connect(btnZone, &QToolButton::toggled, this, [this, btnZone](bool on) {
        if (m_view) m_view->setPlanMode(on);
        if (!on) btnZone->setChecked(false);
    });

    // Exportar área selecionada / todas as áreas
    auto* btnExportArea = makeIconBtn(tr("Exportar área (Ctrl+D)"), m_toolbar);
    btnExportArea->setEnabled(true);
    m_iconBindings.append({btnExportArea, QStringLiteral(":/icons/canvasicons/export-plan-area.svg")});
    tl->addWidget(btnExportArea);
    connect(btnExportArea, &QToolButton::clicked, this, &LousaPanel::exportSelectedZone);

    auto* btnExportAll = makeIconBtn(tr("Exportar todas as áreas (Ctrl+Shift+D)"), m_toolbar);
    btnExportAll->setEnabled(true);
    m_iconBindings.append({btnExportAll, QStringLiteral(":/icons/canvasicons/export-all-plan-areas.svg")});
    tl->addWidget(btnExportAll);
    connect(btnExportAll, &QToolButton::clicked, this, [this]() {
        if (!m_scene) return;
        const QList<CanvasZone> all = m_scene->allZoneData();
        if (all.isEmpty()) {
            QMessageBox::information(this, tr("Exportar áreas"),
                tr("Não há áreas na lousa. Crie uma área primeiro."));
            return;
        }
        exportZones(all);
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

    // Ajuda
    auto* helpBtn = new QToolButton(m_toolbar);
    helpBtn->setObjectName(QStringLiteral("lousaHelpBtn"));
    helpBtn->setText(QStringLiteral("?"));
    helpBtn->setToolTip(tr("Ajuda"));
    helpBtn->setFixedSize(28, 28);
    helpBtn->setCursor(Qt::PointingHandCursor);
    connect(helpBtn, &QToolButton::clicked, this, &LousaPanel::toggleHelp);
    tl->addWidget(helpBtn);

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
        pushUndo();
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
    connect(m_scene, &LousaScene::undoSnapshotRequested, this, [this]() { pushUndo(); });
    connect(m_scene, &LousaScene::cardStashRequested, this, [this](const CanvasCard& c) {
        m_stash.append(c);
        refreshStashList();
        refreshStashBtn();
        save();
    });
    connect(m_scene, &LousaScene::cardCreateDocRequested, this,
            [this](const CanvasCard& c) { createDocFromCard(c); });
    connect(m_scene, &LousaScene::cardCreateTimelineEventRequested, this,
            [this](const CanvasCard& c) {
        // Título = título do card; descrição = corpo do card. O MainWindow
        // abre a Timeline e o popup de criar evento já preenchido.
        emit createTimelineEventRequested(c.title.trimmed(), c.content);
    });
    connect(m_scene, &LousaScene::zoneExportRequested, this, [this](const QString& id) {
        for (const CanvasZone& z : m_scene->allZoneData())
            if (z.id == id) { exportZones({z}); break; }
    });
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
        pushUndo();
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

    buildHelpPanel();
    buildStashPanel();
    buildMapPanel();
    reloadIcons();
}

// ── Mapa de áreas ────────────────────────────────────────────────────────────

void LousaPanel::buildMapPanel()
{
    m_mapBtn = new QToolButton(this);
    m_mapBtn->setObjectName(QStringLiteral("lousaStashBtn"));  // reaproveita o estilo
    m_mapBtn->setCursor(Qt::PointingHandCursor);
    m_mapBtn->setToolTip(tr("Áreas (F)"));
    m_mapBtn->setText(tr("Áreas"));
    connect(m_mapBtn, &QToolButton::clicked, this, &LousaPanel::toggleMap);

    m_mapPanel = new QWidget(this);
    m_mapPanel->setObjectName(QStringLiteral("lousaStashPanel"));  // mesmo estilo do stash
    m_mapPanel->setVisible(false);
    auto* vl = new QVBoxLayout(m_mapPanel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);
    auto* title = new QLabel(tr("Áreas"), m_mapPanel);
    title->setObjectName(QStringLiteral("lousaStashTitle"));
    vl->addWidget(title);
    m_mapList = new QListWidget(m_mapPanel);
    m_mapList->setObjectName(QStringLiteral("lousaStashList"));
    connect(m_mapList, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        const int row = m_mapList->row(it);
        if (!m_scene || !m_view) return;
        const QList<CanvasZone> zones = m_scene->allZoneData();
        if (row < 0 || row >= zones.size()) return;
        const CanvasZone& z = zones[row];
        m_view->fitSceneRect(QRectF(z.x, z.y, z.width, z.height));
        save();
    });
    vl->addWidget(m_mapList, 1);
}

void LousaPanel::positionMapPanel()
{
    if (m_mapBtn) {
        const int bw = qMax(80, m_mapBtn->sizeHint().width() + 16);
        m_mapBtn->setFixedHeight(30);
        m_mapBtn->setFixedWidth(bw);
        const int stashRight = m_stashBtn ? (m_stashBtn->x() + m_stashBtn->width()) : 14;
        m_mapBtn->move(stashRight + 8, height() - m_mapBtn->height() - 14);
        m_mapBtn->raise();
    }
    if (m_mapPanel && m_mapOpen) {
        const int pw = 240, ph = qMin(300, height() - kToolbarH - 70);
        m_mapPanel->setGeometry(14, height() - ph - 52, pw, ph);
        m_mapPanel->raise();
    }
}

void LousaPanel::toggleMap()
{
    if (!m_mapPanel) return;
    m_mapOpen = !m_mapOpen;
    if (m_mapOpen) {
        refreshMapList();
        positionMapPanel();
        m_mapPanel->setVisible(true);
        m_mapPanel->raise();
    } else {
        m_mapPanel->setVisible(false);
    }
}

void LousaPanel::refreshMapList()
{
    if (!m_mapList || !m_scene) return;
    m_mapList->clear();
    for (const CanvasZone& z : m_scene->allZoneData())
        m_mapList->addItem(z.title.isEmpty() ? tr("(área sem nome)") : z.title);
    if (m_mapList->count() == 0)
        m_mapList->addItem(tr("Nenhuma área criada"));
}

// ── Exportação de áreas ──────────────────────────────────────────────────────

static QString lousaCardTitle(const CanvasCard& c)
{
    if (!c.title.trimmed().isEmpty()) return c.title.trimmed();
    QString text = c.content;
    text.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    text = text.simplified();
    const QStringList words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString out;
    for (int i = 0; i < qMin(6, int(words.size())); ++i)
        out += (i ? QStringLiteral(" ") : QString()) + words[i];
    return out.isEmpty() ? QObject::tr("Sem título") : out;
}

static QList<CanvasCard> lousaCardsInZone(const CanvasZone& z, const QList<CanvasCard>& all)
{
    QList<CanvasCard> out;
    const QRectF zr(z.x, z.y, z.width, z.height);
    for (const CanvasCard& c : all) {
        const QRectF cr(c.x, c.y, c.width, c.height);
        if (zr.contains(cr)) out.append(c);
    }
    return out;
}

static QString lousaBuildCardHtml(const CanvasCard& card, const QList<CanvasCard>& all,
                                  const QList<CanvasConnection>& conns)
{
    QString html = card.content.isEmpty() ? QStringLiteral("<p></p>") : card.content;
    // Imagens conectadas diretamente a este card.
    for (const CanvasConnection& conn : conns) {
        if (conn.fromId != card.id && conn.toId != card.id) continue;
        const QString otherId = (conn.fromId == card.id) ? conn.toId : conn.fromId;
        for (const CanvasCard& o : all)
            if (o.id == otherId && o.type == QStringLiteral("image") && !o.content.isEmpty())
                html += QStringLiteral("<p><img src=\"data:image/jpeg;base64,%1\" "
                                       "style=\"max-width:100%;border-radius:6px;\"/></p>").arg(o.content);
    }
    // Conexões anotadas.
    QStringList lines;
    for (const CanvasConnection& conn : conns) {
        if (conn.fromId != card.id && conn.toId != card.id) continue;
        const QString otherId = (conn.fromId == card.id) ? conn.toId : conn.fromId;
        const QString dir = (conn.fromId == card.id) ? QStringLiteral("→") : QStringLiteral("←");
        for (const CanvasCard& o : all)
            if (o.id == otherId && o.type != QStringLiteral("image"))
                lines << QStringLiteral("<li>%1 %2</li>").arg(dir, lousaCardTitle(o).toHtmlEscaped());
    }
    if (!lines.isEmpty())
        html += QStringLiteral("<hr/><p><em>Conexões:</em></p><ul>%1</ul>").arg(lines.join(QString()));
    return html;
}

void LousaPanel::exportZones(const QList<CanvasZone>& zones)
{
    if (!m_projectModel || !m_scene || zones.isEmpty()) return;
    const QList<CanvasCard>       allCards = m_scene->allCardData();
    const QList<CanvasConnection> allConns = m_scene->allConnectionData();

    // Pré-conta: quantas áreas têm conteúdo + se há cards sem título.
    bool hasUntitled = false;
    int  totalExportable = 0;
    int  zonesWithContent = 0;
    for (const CanvasZone& z : zones) {
        int n = 0;
        for (const CanvasCard& c : lousaCardsInZone(z, allCards))
            if (c.type == QStringLiteral("note") || c.type == QStringLiteral("comment")) {
                ++n;
                if (c.title.trimmed().isEmpty()) hasUntitled = true;
            }
        if (n > 0) ++zonesWithContent;
        totalExportable += n;
    }

    if (totalExportable == 0) {
        QMessageBox::information(this, tr("Exportar área"),
            tr("Não há post-its ou comentários dentro da(s) área(s)."));
        return;
    }

    // Confirmação. Cada área vira uma gaveta própria, com o nome da área.
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Exportar áreas"));
    dlg.setMinimumWidth(340);
    auto* vl = new QVBoxLayout(&dlg);
    vl->setSpacing(10);

    auto* info = new QLabel(
        zonesWithContent == 1
            ? tr("A área será exportada para uma gaveta nova, com o nome da área.")
            : tr("Cada área vira uma gaveta nova, com o nome da área "
                 "(%1 áreas → %1 gavetas).").arg(zonesWithContent),
        &dlg);
    info->setWordWrap(true);
    vl->addWidget(info);

    if (hasUntitled) {
        auto* warn = new QLabel(tr("Os post-its sem título serão nomeados com as "
                                   "primeiras palavras do conteúdo."), &dlg);
        warn->setWordWrap(true);
        warn->setStyleSheet(QStringLiteral("color:%1;font-size:11px;").arg(Theme::accentWarning()));
        vl->addWidget(warn);
    }

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
    bb->addButton(tr("Exportar"), QDialogButtonBox::AcceptRole);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vl->addWidget(bb);

    if (dlg.exec() != QDialog::Accepted) return;

    // Uma gaveta por área (com conteúdo).
    int createdDrawers = 0, totalDocs = 0;
    for (const CanvasZone& z : zones) {
        QList<CanvasCard> inside;
        for (const CanvasCard& c : lousaCardsInZone(z, allCards))
            if (c.type == QStringLiteral("note") || c.type == QStringLiteral("comment"))
                inside.append(c);
        if (inside.isEmpty()) continue;

        Drawer d;
        d.key   = ProjectModel::uid();
        d.title = z.title.trimmed().isEmpty() ? tr("Área") : z.title.trimmed();
        m_projectModel->addDrawer(d);
        ++createdDrawers;

        for (const CanvasCard& c : inside) {
            DrawerItem it;
            it.id            = ProjectModel::uid();
            it.title         = lousaCardTitle(c);
            it.html          = lousaBuildCardHtml(c, allCards, allConns);
            it.hasInlineHtml = true;
            m_projectModel->addDrawerItem(d.key, it);
            ++totalDocs;
        }
    }

    QMessageBox::information(this, tr("Exportar áreas"),
        tr("%1 gaveta(s) criada(s) com %2 documento(s).").arg(createdDrawers).arg(totalDocs));
}

void LousaPanel::exportSelectedZone()
{
    if (!m_scene) return;
    const QString sel = m_scene->selectedZoneId();
    if (sel.isEmpty()) {
        QMessageBox::information(this, tr("Exportar área"),
            tr("Selecione uma área primeiro — clique na barra de topo dela."));
        return;
    }
    for (const CanvasZone& z : m_scene->allZoneData())
        if (z.id == sel) { exportZones({z}); return; }
}

// ── Criar documento a partir de um card ──────────────────────────────────────

static QString lousaHtmlFromPlain(const QString& text)
{
    const QString normalized = QString(text).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    const QStringList paras = normalized.split(QRegularExpression(QStringLiteral("\n{2,}")),
                                               Qt::SkipEmptyParts);
    QString out;
    for (const QString& p : paras) {
        QString chunk = p.trimmed();
        if (chunk.isEmpty()) continue;
        chunk = chunk.toHtmlEscaped();
        chunk.replace(QChar('\n'), QStringLiteral("<br/>"));
        out += QStringLiteral("<p>%1</p>").arg(chunk);
    }
    if (out.isEmpty()) out = QStringLiteral("<p></p>");
    return out;
}

void LousaPanel::createDocFromCard(const CanvasCard& c)
{
    if (!m_projectModel) return;
    if (m_projectModel->drawers().isEmpty()) {
        QMessageBox::warning(this, tr("Criar documento"),
            tr("Crie uma gaveta antes de usar este recurso."));
        return;
    }

    // Título sugerido + corpo do documento conforme o tipo do card.
    QString suggested = c.title.trimmed();
    QString bodyHtml;
    if (c.type == QStringLiteral("image")) {
        if (!c.content.isEmpty())
            bodyHtml = QStringLiteral("<p><img src=\"data:image/jpeg;base64,%1\" "
                                      "style=\"max-width:100%;border-radius:6px;\"/></p>").arg(c.content);
        bodyHtml += lousaHtmlFromPlain(c.description);
        if (suggested.isEmpty()) suggested = c.description.trimmed().left(48);
        if (suggested.isEmpty()) suggested = tr("Imagem");
    } else {
        bodyHtml = lousaHtmlFromPlain(c.content);
        if (suggested.isEmpty()) {
            QString flat = c.content;
            flat.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
            flat = flat.trimmed();
            const QStringList words = flat.split(QChar(' '), Qt::SkipEmptyParts);
            QStringList picked;
            for (int i = 0; i < qMin(8, int(words.size())); ++i) picked << words[i];
            suggested = picked.join(QChar(' '));
        }
        if (suggested.isEmpty()) suggested = tr("Documento");
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Criar documento"));
    dlg.setMinimumWidth(360);
    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 16, 16, 12);
    root->setSpacing(10);

    root->addWidget(new QLabel(tr("Nome do documento:"), &dlg));
    auto* nameEdit = new QLineEdit(suggested, &dlg);
    nameEdit->selectAll();
    root->addWidget(nameEdit);

    root->addWidget(new QLabel(tr("Gaveta de destino:"), &dlg));
    auto* combo = new QComboBox(&dlg);
    for (const Drawer& d : m_projectModel->drawers())
        combo->addItem(d.title.isEmpty() ? tr("(sem nome)") : d.title, d.key);
    root->addWidget(combo);

    auto* hint = new QLabel(&dlg);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:%1;font-size:11px;").arg(Theme::textMuted()));
    root->addWidget(hint);
    auto refreshHint = [this, combo, hint]() {
        const Drawer* d = m_projectModel->findDrawer(combo->currentData().toString());
        const QString et = d ? d->drawerElementType : QString();
        if (et == QStringLiteral("character"))    hint->setText(tr("Vai abrir o cadastro de personagem em seguida (foto e papel)."));
        else if (et == QStringLiteral("setting")) hint->setText(tr("Vai abrir o cadastro de cenário em seguida (foto)."));
        else if (et == QStringLiteral("object"))  hint->setText(tr("Vai abrir o cadastro de objeto em seguida (foto)."));
        else hint->clear();
    };
    refreshHint();
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
            [refreshHint](int) { refreshHint(); });

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
    bb->addButton(tr("Criar"), QDialogButtonBox::AcceptRole);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    root->addWidget(bb);

    if (dlg.exec() != QDialog::Accepted) return;
    const QString title = nameEdit->text().trimmed();
    if (title.isEmpty()) return;
    const QString destKey = combo->currentData().toString();
    if (destKey.isEmpty()) return;

    const Drawer* destDrawer = m_projectModel->findDrawer(destKey);
    const QString et = destDrawer ? destDrawer->drawerElementType : QString();
    const bool isVisual = et == QStringLiteral("character")
                       || et == QStringLiteral("setting")
                       || et == QStringLiteral("object");

    if (isVisual) {
        ElementCreateDialog edlg(et, this);
        edlg.setInitial(title, QString(), QString());
        if (edlg.exec() != QDialog::Accepted) return;
        const QString finalTitle = edlg.title().trimmed();
        if (finalTitle.isEmpty()) return;
        Element elem;
        elem.name  = finalTitle;
        elem.type  = et;
        elem.icon  = (et == QStringLiteral("character")) ? QStringLiteral("user")
                   : (et == QStringLiteral("setting"))   ? QStringLiteral("map")
                                                         : QStringLiteral("cube");
        elem.role  = edlg.role();
        elem.image = edlg.imageDataUrl();
        elem.aliases = edlg.aliases();
        const QString elementId = m_elementsStore ? m_elementsStore->addElement(elem) : QString();
        DrawerItem it;
        it.id            = ProjectModel::uid();
        it.title         = finalTitle;
        it.hasInlineHtml = true;
        it.html          = bodyHtml;
        it.elementType   = et;
        it.elementId     = elementId;
        it.role          = edlg.role();
        m_projectModel->addDrawerItem(destKey, it);
    } else {
        DrawerItem it;
        it.id            = ProjectModel::uid();
        it.title         = title;
        it.hasInlineHtml = true;
        it.html          = bodyHtml;
        m_projectModel->addDrawerItem(destKey, it);
    }
}

// ── Stash de cards ───────────────────────────────────────────────────────────

void LousaPanel::buildStashPanel()
{
    // Botão flutuante no canto inferior esquerdo
    m_stashBtn = new QToolButton(this);
    m_stashBtn->setObjectName(QStringLiteral("lousaStashBtn"));
    m_stashBtn->setCursor(Qt::PointingHandCursor);
    m_stashBtn->setToolTip(tr("Cards guardados"));
    connect(m_stashBtn, &QToolButton::clicked, this, &LousaPanel::toggleStash);

    // Painel flutuante
    m_stashPanel = new QWidget(this);
    m_stashPanel->setObjectName(QStringLiteral("lousaStashPanel"));
    m_stashPanel->setVisible(false);
    auto* vl = new QVBoxLayout(m_stashPanel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* title = new QLabel(tr("Cards guardados"), m_stashPanel);
    title->setObjectName(QStringLiteral("lousaStashTitle"));
    vl->addWidget(title);

    m_stashList = new QListWidget(m_stashPanel);
    m_stashList->setObjectName(QStringLiteral("lousaStashList"));
    connect(m_stashList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        restoreFromStash(m_stashList->row(it));
    });
    vl->addWidget(m_stashList, 1);

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);
    auto* restoreBtn = new QPushButton(tr("Restaurar"), m_stashPanel);
    restoreBtn->setCursor(Qt::PointingHandCursor);
    connect(restoreBtn, &QPushButton::clicked, this, [this]() {
        if (m_stashList->currentRow() >= 0) restoreFromStash(m_stashList->currentRow());
    });
    auto* delBtn = new QPushButton(tr("Apagar"), m_stashPanel);
    delBtn->setCursor(Qt::PointingHandCursor);
    connect(delBtn, &QPushButton::clicked, this, [this]() {
        const int r = m_stashList->currentRow();
        if (r < 0 || r >= m_stash.size()) return;
        m_stash.removeAt(r);
        refreshStashList();
        refreshStashBtn();
        save();
    });
    btnRow->addWidget(restoreBtn);
    btnRow->addWidget(delBtn);
    btnRow->addStretch(1);
    vl->addLayout(btnRow);

    refreshStashBtn();
}

void LousaPanel::positionStashPanel()
{
    if (m_stashBtn) {
        const int bw = m_stashBtn->sizeHint().width() + 16;
        m_stashBtn->setFixedHeight(30);
        m_stashBtn->setFixedWidth(qMax(96, bw));
        m_stashBtn->move(14, height() - m_stashBtn->height() - 14);
        m_stashBtn->raise();
    }
    if (m_stashPanel && m_stashOpen) {
        const int pw = 280, ph = qMin(340, height() - kToolbarH - 70);
        m_stashPanel->setGeometry(14, height() - ph - 52, pw, ph);
        m_stashPanel->raise();
    }
}

void LousaPanel::toggleStash()
{
    if (!m_stashPanel) return;
    m_stashOpen = !m_stashOpen;
    if (m_stashOpen) {
        refreshStashList();
        positionStashPanel();
        m_stashPanel->setVisible(true);
        m_stashPanel->raise();
    } else {
        m_stashPanel->setVisible(false);
    }
}

void LousaPanel::refreshStashBtn()
{
    if (!m_stashBtn) return;
    m_stashBtn->setText(tr("Gaveta (%1)").arg(m_stash.size()));
    positionStashPanel();
}

void LousaPanel::refreshStashList()
{
    if (!m_stashList) return;
    m_stashList->clear();
    for (const CanvasCard& c : m_stash) {
        QString label = c.title.trimmed();
        if (label.isEmpty()) {
            QString text = c.content;
            text.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
            text = text.simplified();
            label = text.left(40);
        }
        if (label.isEmpty()) label = c.type;
        m_stashList->addItem(QStringLiteral("%1 — %2").arg(c.type, label));
    }
}

void LousaPanel::restoreFromStash(int index)
{
    if (index < 0 || index >= m_stash.size() || !m_scene) return;
    pushUndo();
    CanvasCard c = m_stash.takeAt(index);
    // Reposiciona no centro da viewport para garantir que apareça na tela.
    if (m_view) {
        const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
        c.x = center.x() - c.width / 2.0;
        c.y = center.y() - c.height / 2.0;
    }
    m_scene->addCard(c);
    refreshDocCards();
    refreshStashList();
    refreshStashBtn();
    refreshEmptyState();
    save();
}

void LousaPanel::stashSelectedCards()
{
    if (!m_scene) return;
    const QList<CardItem*> sel = m_scene->selectedCardItems();
    if (sel.isEmpty()) return;
    pushUndo();
    for (CardItem* c : sel) {
        m_stash.append(c->cardData());
        m_scene->removeCard(c->cardData().id);
    }
    refreshStashList();
    refreshStashBtn();
    refreshEmptyState();
    save();
}

void LousaPanel::deleteSelectedCards()
{
    if (!m_scene) return;
    const QList<CardItem*> sel = m_scene->selectedCardItems();
    if (sel.isEmpty()) return;
    const int n = sel.size();
    const auto answer = QMessageBox::question(
        this, tr("Apagar cards"),
        n == 1 ? tr("Apagar este card definitivamente?")
               : tr("Apagar %1 cards definitivamente?").arg(n),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    pushUndo();
    for (CardItem* c : sel) m_scene->removeCard(c->cardData().id);
    refreshEmptyState();
    save();
}

// ── Painel de ajuda ──────────────────────────────────────────────────────────

void LousaPanel::buildHelpPanel()
{
    m_helpPanel = new QWidget(this);
    m_helpPanel->setObjectName(QStringLiteral("lousaHelpPanel"));
    m_helpPanel->setVisible(false);

    auto* outer = new QVBoxLayout(m_helpPanel);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Header
    auto* header = new QWidget(m_helpPanel);
    header->setObjectName(QStringLiteral("lousaHelpHeader"));
    header->setFixedHeight(44);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 0, 10, 0);
    auto* hTitle = new QLabel(tr("Ajuda — Lousa"), header);
    hTitle->setObjectName(QStringLiteral("lousaHelpTitle"));
    hl->addWidget(hTitle);
    hl->addStretch(1);
    auto* hClose = new QToolButton(header);
    hClose->setObjectName(QStringLiteral("lousaHelpClose"));
    hClose->setText(QStringLiteral("×"));
    hClose->setFixedSize(26, 26);
    hClose->setCursor(Qt::PointingHandCursor);
    connect(hClose, &QToolButton::clicked, this, &LousaPanel::toggleHelp);
    hl->addWidget(hClose);
    outer->addWidget(header);

    // Conteúdo rolável
    auto* scroll = new QScrollArea(m_helpPanel);
    scroll->setObjectName(QStringLiteral("lousaHelpScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(scroll);
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(16, 14, 16, 22);
    cl->setSpacing(18);

    auto addSectionLabel = [&](const QString& text) {
        auto* l = new QLabel(text, content);
        l->setObjectName(QStringLiteral("lousaHelpSection"));
        cl->addWidget(l);
    };

    // Introdução
    addSectionLabel(tr("Introdução"));
    auto* intro = new QLabel(content);
    intro->setObjectName(QStringLiteral("lousaHelpIntro"));
    intro->setWordWrap(true);
    intro->setText(tr(
        "Bem-vindo à lousa.\n\n"
        "Aqui você tem um espaço livre para criar, planejar e organizar seu projeto — "
        "antes de começar a escrever, no meio do processo, ou quando a cabeça pedir.\n\n"
        "Solte post-its, comentários, imagens, documentos e personagens pela tela. "
        "Conecte eles com linhas, delimite áreas de planejamento, rotacione, empilhe, "
        "organize como quiser.\n\n"
        "Se tiver dúvida em alguma coisa, as seções abaixo têm tudo explicado."));
    cl->addWidget(intro);

    struct Section { QString title; QVector<QPair<QString, QString>> rows; };
    const QVector<Section> sections = {
        { tr("Se mover na lousa"), {
            { tr("Arrastar o fundo"), tr("Move a lousa") },
            { tr("Scroll do mouse"), tr("Aproxima e afasta") },
            { tr("Indicador de zoom"), tr("Mostra o zoom atual") },
        }},
        { tr("Colocar coisas na lousa"), {
            { QStringLiteral("Ctrl+N"), tr("Post-it") },
            { QStringLiteral("Alt+C"), tr("Comentário com rabinho de balão") },
            { QStringLiteral("Ctrl+G"), tr("Imagem") },
            { QStringLiteral("Ctrl+H"), tr("Traz um doc da gaveta") },
            { QStringLiteral("Ctrl+Shift+C"), tr("Coloca um personagem do projeto") },
            { QStringLiteral("Ctrl+T"), tr("Texto solto, sem card") },
            { tr("Botão ★ na toolbar"), tr("Símbolo ou emoji") },
        }},
        { tr("Selecionar e mover cards"), {
            { tr("Shift+click"), tr("Marca ou desmarca um card") },
            { tr("Shift+S (segurar)"), tr("Passa o mouse e seleciona tudo que tocar") },
            { tr("Arrastar selecionado"), tr("Move todos os marcados juntos") },
            { tr("Ctrl+X (1 card marcado)"), tr("Recorta — o card fica transparente até ser colado") },
            { QStringLiteral("Ctrl+V"), tr("Cola onde o mouse estiver") },
            { tr("Escape"), tr("Cancela o recorte") },
        }},
        { tr("Apagar e guardar"), {
            { tr("× no card"), tr("Guarda na gaveta da lousa — pode ser restaurado depois") },
            { tr("Shift+× no card"), tr("Deleta o card definitivamente") },
            { tr("Delete (com cards marcados)"), tr("Manda todos para a gaveta da lousa") },
            { tr("Shift+Delete"), tr("Apaga de vez — pede confirmação") },
            { tr("Botão gaveta (canto inf. esq.)"), tr("Abre a gaveta de cards guardados") },
        }},
        { tr("Desfazer e refazer"), {
            { QStringLiteral("Ctrl+Z"), tr("Volta atrás — até 50 vezes") },
            { QStringLiteral("Ctrl+Y"), tr("Vai pra frente de novo") },
        }},
        { tr("Ligar cards com linhas"), {
            { tr("Ponto colorido no topo"), tr("Arrasta até outro card pra conectar") },
            { tr("Post-it perto de uma linha (1s)"), tr("Vira uma parada no meio da linha") },
            { tr("Passar o mouse na linha"), tr("Aparece o × pra deletar a linha") },
        }},
        { tr("Textos e símbolos"), {
            { tr("Duplo-click"), tr("Entra pra editar") },
            { tr("Shift+arrastar"), tr("Gira o elemento") },
            { tr("Puxar o canto inferior dir."), tr("Muda o tamanho da letra") },
        }},
        { tr("Áreas de planejamento"), {
            { tr("Botão Definir área na toolbar"), tr("Ativa o modo de desenhar área") },
            { tr("Arrastar no fundo"), tr("Cria o retângulo da área") },
            { tr("Grade de pontos na área"), tr("Segura aqui pra mover a área") },
            { tr("Ctrl+Shift+mover"), tr("Move a área com tudo que tem dentro") },
            { tr("Passar o mouse na borda"), tr("Aparece os pontos pra redimensionar") },
            { QStringLiteral("Ctrl+D"), tr("Exporta a área marcada pra uma gaveta") },
            { QStringLiteral("Ctrl+Shift+D"), tr("Exporta todas as áreas de uma vez") },
            { tr("Tecla F"), tr("Lista de áreas — clica pra ir direto") },
        }},
        { tr("Outros"), {
            { tr("Círculo colorido na toolbar"), tr("Muda a cor do fundo da lousa") },
            { tr("Ícone de doc no card"), tr("Cria um doc a partir desse card") },
            { tr("Duplo-click na foto/iniciais"), tr("Abre o doc do personagem dentro do card") },
            { tr("Não precisa salvar"), tr("Tudo é salvo sozinho enquanto você trabalha") },
        }},
    };

    for (const Section& s : sections) {
        addSectionLabel(s.title);
        auto* rowsBox = new QWidget(content);
        auto* rl = new QVBoxLayout(rowsBox);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(3);
        for (const auto& [key, desc] : s.rows) {
            auto* row = new QLabel(rowsBox);
            row->setObjectName(QStringLiteral("lousaHelpRow"));
            row->setWordWrap(true);
            row->setTextFormat(Qt::RichText);
            row->setText(QStringLiteral("<b>%1</b> — %2")
                             .arg(key.toHtmlEscaped(), desc.toHtmlEscaped()));
            rl->addWidget(row);
        }
        cl->addWidget(rowsBox);
    }

    cl->addStretch(1);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
}

void LousaPanel::positionHelpPanel()
{
    if (!m_helpPanel) return;
    const int w = qMin(360, width());
    m_helpPanel->setGeometry(width() - w, kToolbarH, w, height() - kToolbarH);
}

void LousaPanel::toggleHelp()
{
    if (!m_helpPanel) return;
    m_helpOpen = !m_helpOpen;
    if (m_helpOpen) {
        positionHelpPanel();
        m_helpPanel->raise();
        m_helpPanel->setVisible(true);
    } else {
        m_helpPanel->setVisible(false);
    }
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
    if (m_helpOpen) positionHelpPanel();
    positionStashPanel();
    positionMapPanel();
}

void LousaPanel::closeEvent(QCloseEvent* event)
{
    // Esconde em vez de destruir — o estado fica vivo entre aberturas.
    hide();
    event->ignore();
    emit closeRequested();
}

void LousaPanel::keyPressEvent(QKeyEvent* event)
{
    const auto mods  = event->modifiers();
    const bool ctrl  = mods & Qt::ControlModifier;
    const bool shift = mods & Qt::ShiftModifier;
    const bool alt   = mods & Qt::AltModifier;
    const int  key   = event->key();

    // Desfazer / refazer
    if (ctrl && !shift && !alt && key == Qt::Key_Z) { undo(); event->accept(); return; }
    if (ctrl && !shift && !alt && key == Qt::Key_Y) { redo(); event->accept(); return; }

    // Delete = guardar selecionados no stash; Shift+Delete = apagar de vez
    if ((key == Qt::Key_Delete || key == Qt::Key_Backspace)
        && m_scene && !m_scene->selectedCardItems().isEmpty()) {
        if (shift) deleteSelectedCards();
        else       stashSelectedCards();
        event->accept(); return;
    }

    // Criação de cards (dispara o mesmo fluxo dos botões da toolbar)
    if (ctrl && !shift && !alt && key == Qt::Key_N) { if (m_btnNote) m_btnNote->click(); event->accept(); return; }
    if (ctrl && !shift && !alt && key == Qt::Key_T) { if (m_btnText) m_btnText->click(); event->accept(); return; }
    if (!ctrl && !shift && alt  && key == Qt::Key_C) { if (m_btnCmt)  m_btnCmt->click();  event->accept(); return; }
    if (ctrl && !shift && !alt && key == Qt::Key_H) { if (m_btnDoc)  m_btnDoc->click();  event->accept(); return; }
    if (ctrl && !shift && !alt && key == Qt::Key_G) { if (m_btnImg)  m_btnImg->click();  event->accept(); return; }
    if (ctrl && shift  && !alt && key == Qt::Key_C) { if (m_btnChar) m_btnChar->click(); event->accept(); return; }

    // Ctrl+X: 1 card selecionado → recorta (fica translúcido); sem seleção → modo área.
    if (ctrl && !shift && !alt && key == Qt::Key_X) {
        const QList<CardItem*> sel = m_scene ? m_scene->selectedCardItems()
                                             : QList<CardItem*>();
        if (sel.size() == 1) {
            if (!m_cutCardId.isEmpty())
                if (CardItem* prev = m_scene->findCard(m_cutCardId)) prev->setOpacity(1.0);
            m_cutCardId = sel.first()->cardData().id;
            sel.first()->setOpacity(0.4);
            event->accept(); return;
        }
        if (sel.isEmpty()) {
            if (m_btnZone) m_btnZone->click();
            event->accept(); return;
        }
    }

    // Ctrl+V: cola o card recortado na posição do mouse.
    if (ctrl && !shift && !alt && key == Qt::Key_V && !m_cutCardId.isEmpty() && m_scene && m_view) {
        if (CardItem* c = m_scene->findCard(m_cutCardId)) {
            pushUndo();
            const QPointF sp = m_view->mapToScene(m_view->mapFromGlobal(QCursor::pos()));
            const CanvasCard d = c->cardData();
            c->setPos(sp.x() - d.width / 2.0, sp.y() - d.height / 2.0);
            c->setOpacity(1.0);
            save();
        }
        m_cutCardId.clear();
        event->accept(); return;
    }

    // Escape: cancela o recorte.
    if (key == Qt::Key_Escape && !m_cutCardId.isEmpty()) {
        if (CardItem* c = m_scene->findCard(m_cutCardId)) c->setOpacity(1.0);
        m_cutCardId.clear();
        event->accept(); return;
    }

    // Brush select: Shift+S segurado seleciona os cards que o mouse tocar.
    if (shift && key == Qt::Key_S) {
        if (m_view) m_view->setBrushMode(true);
        event->accept(); return;
    }

    // F: abre/fecha a lista de áreas.
    if (!ctrl && !shift && !alt && key == Qt::Key_F) {
        toggleMap();
        event->accept(); return;
    }

    // Ctrl+D: exporta a área selecionada (ou a que está sob o mouse).
    // Ctrl+Shift+D: exporta todas — cada área para a sua própria gaveta.
    if (ctrl && !alt && key == Qt::Key_D && m_scene) {
        const QList<CanvasZone> all = m_scene->allZoneData();
        if (shift) {
            if (!all.isEmpty()) exportZones(all);
        } else {
            CanvasZone target;
            bool found = false;
            const QString sel = m_scene->selectedZoneId();
            if (!sel.isEmpty())
                for (const CanvasZone& z : all)
                    if (z.id == sel) { target = z; found = true; break; }
            // Fallback: a menor área que contém o cursor.
            if (!found && m_view) {
                const QPointF sp = m_view->mapToScene(m_view->mapFromGlobal(QCursor::pos()));
                qreal bestArea = -1.0;
                for (const CanvasZone& z : all) {
                    if (QRectF(z.x, z.y, z.width, z.height).contains(sp)) {
                        const qreal a = z.width * z.height;
                        if (bestArea < 0 || a < bestArea) { bestArea = a; target = z; found = true; }
                    }
                }
            }
            if (found) exportZones({target});
        }
        event->accept(); return;
    }

    QWidget::keyPressEvent(event);
}

void LousaPanel::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_S || event->key() == Qt::Key_Shift) {
        if (m_view) m_view->setBrushMode(false);
    }
    QWidget::keyReleaseEvent(event);
}

void LousaPanel::setProjectModel(ProjectModel* model) { m_projectModel = model; }
void LousaPanel::setElementsStore(ElementsStore* store) { m_elementsStore = store; }

void LousaPanel::refreshDocCards()
{
    if (!m_scene || !m_projectModel) return;
    for (CardItem* item : m_scene->cardItems()) {
        const CanvasCard d = item->cardData();
        if (d.type == QStringLiteral("doc")) {
            const DrawerItem* di = m_projectModel->findDrawerItem(d.linkedItemId);
            if (di) item->setLinkedHtml(di->html);
        } else if (d.type == QStringLiteral("character")) {
            const DrawerItem* di = m_projectModel->findDrawerItem(d.linkedItemId);
            if (di) {
                item->setLinkedHtml(di->html);
                // Re-busca a foto do ElementsStore se não estiver salva
                if (d.photoDataUrl.isEmpty() && m_elementsStore && !di->elementId.isEmpty()) {
                    if (const Element* el = m_elementsStore->findElement(di->elementId)) {
                        item->setCharacterPhoto(el->image);
                    }
                }
            }
        }
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
    c.photoDataUrl    = o.value(QStringLiteral("photoDataUrl")).toString();
    c.linkedItemId    = o.value(QStringLiteral("linkedItemId")).toString();
    c.linkedDrawerKey = o.value(QStringLiteral("linkedDrawerName")).toString();
    c.fontSize        = o.value(QStringLiteral("fontSize")).toInt(0);
    c.bold            = o.value(QStringLiteral("bold")).toBool(false);
    c.italic          = o.value(QStringLiteral("italic")).toBool(false);
    c.rotation        = o.value(QStringLiteral("rotation")).toDouble(0.0);
    c.fontFamily      = o.value(QStringLiteral("fontFamily")).toString();
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
    o.insert(QStringLiteral("description"),   c.description);
    o.insert(QStringLiteral("photoDataUrl"), c.photoDataUrl);
    o.insert(QStringLiteral("linkedItemId"),    c.linkedItemId);
    o.insert(QStringLiteral("linkedDrawerName"), c.linkedDrawerKey);
    if (c.fontSize > 0) o.insert(QStringLiteral("fontSize"), c.fontSize);
    if (c.bold)         o.insert(QStringLiteral("bold"),   true);
    if (c.italic)       o.insert(QStringLiteral("italic"), true);
    if (!qFuzzyIsNull(c.rotation)) o.insert(QStringLiteral("rotation"), c.rotation);
    if (!c.fontFamily.isEmpty()) o.insert(QStringLiteral("fontFamily"), c.fontFamily);
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
    m_stash.clear();
    for (const auto& v : root.value(QStringLiteral("stash")).toArray()) {
        const CanvasCard c = cardFromJson(v.toObject());
        if (!c.id.isEmpty()) m_stash.append(c);
    }
    refreshStashList();
    refreshStashBtn();
    // Re-busca HTML e foto dos cards vinculados ao projeto
    QTimer::singleShot(0, this, [this]() { refreshDocCards(); });
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

    QJsonArray stash;
    for (const CanvasCard& c : m_stash) stash.append(cardToJson(c));
    root.insert(QStringLiteral("stash"), stash);

    QSaveFile f(QDir::cleanPath(m_projectRoot + QStringLiteral("/canvas.json")));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.commit();
}

// ── Undo / Redo ──────────────────────────────────────────────────────────────

LousaPanel::BoardState LousaPanel::captureState() const
{
    BoardState s;
    if (!m_scene) return s;
    s.cards       = m_scene->allCardData();
    s.connections = m_scene->allConnectionData();
    s.zones       = m_scene->allZoneData();
    // Tira o conteúdo pesado de imagem dos snapshots (guarda à parte por id).
    for (CanvasCard& c : s.cards) {
        if (c.type == QStringLiteral("image") && !c.content.isEmpty()) {
            const_cast<LousaPanel*>(this)->m_contentStore.insert(c.id, c.content);
            c.content.clear();
        }
    }
    return s;
}

void LousaPanel::applyState(const BoardState& s)
{
    if (!m_scene || !m_view) return;
    m_loading = true;
    m_scene->clearCards();
    m_scene->clearConnections();
    m_scene->clearZones();
    for (CanvasCard c : s.cards) {
        if (c.type == QStringLiteral("image") && c.content.isEmpty()
            && m_contentStore.contains(c.id))
            c.content = m_contentStore.value(c.id);
        m_scene->addCard(c);
    }
    for (const CanvasConnection& conn : s.connections) m_scene->addConnection(conn);
    for (const CanvasZone& z : s.zones)                m_scene->addZone(z);
    refreshDocCards();
    m_loading = false;
    save();
    refreshEmptyState();
}

void LousaPanel::pushUndo()
{
    if (m_loading) return;
    m_undo.append(captureState());
    while (m_undo.size() > 50) m_undo.removeFirst();
    m_redo.clear();
}

void LousaPanel::undo()
{
    if (m_undo.isEmpty()) return;
    m_redo.append(captureState());
    const BoardState s = m_undo.takeLast();
    applyState(s);
}

void LousaPanel::redo()
{
    if (m_redo.isEmpty()) return;
    m_undo.append(captureState());
    const BoardState s = m_redo.takeLast();
    applyState(s);
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
    } else if (type == QStringLiteral("text")) {
        c.width  = 200; c.height = 80;
        c.color  = QColor(QStringLiteral("#ffffff"));
        c.fontSize = 120;
    } else if (type == QStringLiteral("symbol")) {
        c.width  = 100; c.height = 100;
        c.color  = QColor(QStringLiteral("#ffffff"));
        c.fontSize = 60;
        c.content  = QStringLiteral("★"); // ★ default
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
        "QToolButton#lousaHelpBtn {"
        "  background: transparent; border: 1px solid %2; border-radius: 14px;"
        "  color: %3; font-size: 14px; font-weight: 700;"
        "}"
        "QToolButton#lousaHelpBtn:hover { background: %4; border-color: %5; }"
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

    if (m_helpPanel) {
        m_helpPanel->setStyleSheet(QStringLiteral(
            "QWidget#lousaHelpPanel { background: %1; border-left: 1px solid %2; }"
            "QWidget#lousaHelpHeader { background: transparent; border-bottom: 1px solid %2; }"
            "QLabel#lousaHelpTitle { color: %3; font-size: 12px; font-weight: 700;"
            "  letter-spacing: 0.05em; }"
            "QToolButton#lousaHelpClose { background: transparent; border: none;"
            "  color: %4; font-size: 16px; }"
            "QToolButton#lousaHelpClose:hover { color: %3; }"
            "QScrollArea#lousaHelpScroll { background: transparent; border: none; }"
            "QScrollArea#lousaHelpScroll > QWidget > QWidget { background: transparent; }"
            "QLabel#lousaHelpSection { color: %4; font-size: 10px; font-weight: 700;"
            "  text-transform: uppercase; letter-spacing: 0.08em; }"
            "QLabel#lousaHelpIntro { color: %3; font-size: 13px; }"
            "QLabel#lousaHelpRow { color: %3; font-size: 12px; }"
        ).arg(Theme::panelBackground(),  // 1
             Theme::panelBorder(),       // 2
             Theme::textPrimary(),       // 3
             Theme::textMuted()));        // 4
    }

    const QString stashBtnQss = QStringLiteral(
        "QToolButton#lousaStashBtn { background: %1; border: 1px solid %2;"
        "  border-radius: 8px; color: %3; font-size: 12px; padding: 4px 12px; }"
        "QToolButton#lousaStashBtn:hover { background: %4; border-color: %5; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(),
         Theme::hoverOverlay(), Theme::subtleBorder());
    const QString stashPanelQss = QStringLiteral(
        "QWidget#lousaStashPanel { background: %1; border: 1px solid %2;"
        "  border-radius: 10px; }"
        "QLabel#lousaStashTitle { color: %4; font-size: 11px; font-weight: 700;"
        "  text-transform: uppercase; letter-spacing: 0.06em; }"
        "QListWidget#lousaStashList { background: %6; border: 1px solid %2;"
        "  border-radius: 6px; color: %3; font-size: 12px; }"
        "QPushButton { background: %5; border: 1px solid %2; border-radius: 6px;"
        "  color: %3; font-size: 12px; padding: 4px 10px; }"
        "QPushButton:hover { background: %4; }"
    ).arg(Theme::panelBackground(),  // 1
         Theme::panelBorder(),        // 2
         Theme::textPrimary(),        // 3
         Theme::textMuted(),          // 4
         Theme::hoverOverlay(),       // 5
         Theme::inputBackground());   // 6
    if (m_stashBtn)   m_stashBtn->setStyleSheet(stashBtnQss);
    if (m_stashPanel) m_stashPanel->setStyleSheet(stashPanelQss);
    if (m_mapBtn)     m_mapBtn->setStyleSheet(stashBtnQss);
    if (m_mapPanel)   m_mapPanel->setStyleSheet(stashPanelQss);

    reloadIcons();
    updateColorBtn();

    if (m_view) {
        m_view->setStyleSheet(QStringLiteral(
            "QGraphicsView { border: none; background: transparent; }"));
    }
}
