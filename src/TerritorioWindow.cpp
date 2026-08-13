#include "TerritorioWindow.h"
#include "AvatarUtils.h"
#include "ConstrutorStore.h"
#include "ConstrutorWindow.h"
#include "Theme.h"
#include "WorldContentEditor.h"

#include <QAction>
#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextDocument>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cmath>
#include <functional>

namespace {
constexpr int kNodeIdRole   = Qt::UserRole;
constexpr int kNodeTypeRole = Qt::UserRole + 1;
constexpr int kAvatarSize   = 76;

// Overlay transparente a mouse — só pinta as linhas de vínculo entre os
// círculos do Seletor. A interação (clicar/botão direito numa linha) é feita
// via eventFilter no viewport do QListWidget, não aqui — assim os cliques
// que NÃO acertam uma linha continuam chegando normalmente nos itens da
// lista (mesmo padrão de "overlay só desenha, eventFilter decide" já usado
// pro overlay de imagem do WorldContentEditor).
class LinksOverlay : public QWidget {
public:
    explicit LinksOverlay(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
    std::function<void(QPainter&)> paintFn;

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!paintFn) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        paintFn(p);
    }
};

// Distância de um ponto até um segmento de reta — usada pro hit-test das
// linhas de vínculo (clique/botão direito perto o bastante da linha).
qreal distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const QPointF ab = b - a;
    const qreal len2 = QPointF::dotProduct(ab, ab);
    qreal t = len2 > 0 ? QPointF::dotProduct(p - a, ab) / len2 : 0.0;
    t = qBound(0.0, t, 1.0);
    const QPointF proj = a + ab * t;
    const QPointF d = p - proj;
    return std::sqrt(QPointF::dotProduct(d, d));
}

// Prévia em texto puro do HTML de um território — usada no tooltip do
// Seletor (a lore completa fica no editor, o tooltip só adianta um resumo).
QString plainTextPreview(const QString& html, int maxLen)
{
    QTextDocument doc;
    if (html.startsWith(QLatin1String("<!DOCTYPE")))
        doc.setHtml(html);
    else
        doc.setPlainText(html);
    QString text = doc.toPlainText().trimmed();
    if (text.length() > maxLen)
        text = text.left(maxLen).trimmed() + QStringLiteral("…");
    return text;
}
}

TerritorioWindow::TerritorioWindow(TerritorioStore* store, QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(tr("Criador de Mundos"));
    setMinimumSize(900, 580);
    resize(1180, 720);
    buildUi();
    showNoTerritorioOpenState();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &TerritorioWindow::applyTheme);
    setStore(store);
}

void TerritorioWindow::setStore(TerritorioStore* store)
{
    if (m_store != store) {
        if (m_store)
            disconnect(m_store, &TerritorioStore::changed, this, &TerritorioWindow::onStoreChanged);
        m_store = store;
        if (m_store)
            connect(m_store, &TerritorioStore::changed, this, &TerritorioWindow::onStoreChanged);
    }
    // Reconstrói sempre — o mesmo ponteiro de store é reaproveitado entre
    // projetos (setProjectRoot()+load() só troca o conteúdo interno), então
    // igualdade de ponteiro sozinha não pode ser usada pra pular o rebuild
    // (mesmo bug já corrigido em ConstrutorWindow::setStore).
    if (m_store) rebuildSelector();
}

void TerritorioWindow::setConstrutorStore(ConstrutorStore* store)
{
    m_construtorStore = store;

    if (!m_embeddedConstrutor) {
        if (!store || !m_construtorPanel || !m_editor) return;
        // Editor compartilhado (m_editor) — Território e Construtor são a
        // mesma função, editam no mesmo lugar. setEditorActive(false) até
        // que o usuário clique em algo do lado do Construtor.
        m_embeddedConstrutor = new ConstrutorWindow(store, m_editor, m_construtorPanel);
        m_embeddedConstrutor->setTerritorioStore(m_store);
        m_embeddedConstrutor->setTerritoryFilter(m_currentTerritorioId);
        m_embeddedConstrutor->setEditorActive(false);
        connect(m_embeddedConstrutor, &ConstrutorWindow::openMentionInEditorRequested,
                this, &TerritorioWindow::openMentionInEditorRequested);
        connect(m_embeddedConstrutor, &ConstrutorWindow::editorOwnershipRequested, this, [this]() {
            // Construtor acabou de assumir o editor — Território cede. Limpa
            // a própria seleção/estado (nunca o m_editor em si, que agora
            // pertence ao Construtor) pro mesmo motivo de
            // ConstrutorWindow::setEditorActive(false): clicar de novo no
            // mesmo território/nó precisa voltar a disparar o sinal de
            // seleção e reclamar o editor de volta.
            m_editorActiveHere = false;
            m_currentTerritorioId.clear();
            m_currentNodeId.clear();
            m_currentLinkId.clear();
            m_rebuilding = true;
            if (m_selector) m_selector->setCurrentItem(nullptr);
            if (m_tree) m_tree->clear();
            m_rebuilding = false;
            if (m_charactersLabel) m_charactersLabel->setText(tr("Nenhum território aberto."));
            if (m_deleteNodeBtn) m_deleteNodeBtn->setEnabled(false);
            rebuildPlaceEvents();
            rebuildMentions();
        });
        m_construtorPanel->layout()->addWidget(m_embeddedConstrutor);
    } else {
        // Mesma ressalva de TerritorioStore::setStore(): o ponteiro é
        // reaproveitado entre projetos, setStore() já reconstrói sempre.
        m_embeddedConstrutor->setStore(store);
        m_embeddedConstrutor->setTerritorioStore(m_store);
    }
    m_construtorPanel->setVisible(m_construtorStore != nullptr);
}

void TerritorioWindow::rebuildPlaceEvents()
{
    if (!m_placeEventsLabel) return;
    if (m_currentTerritorioId.isEmpty() || !m_placeEventsProvider) {
        m_placeEventsLabel->setText(tr("—"));
        return;
    }
    const QStringList events = m_placeEventsProvider(m_currentTerritorioId);
    if (events.isEmpty()) {
        m_placeEventsLabel->setText(tr("Nenhum evento marcado aqui ainda."));
        return;
    }
    QStringList lines;
    for (const auto& e : events) lines << QStringLiteral("• %1").arg(e);
    m_placeEventsLabel->setText(lines.join(QStringLiteral("\n")));
}

void TerritorioWindow::rebuildMentions()
{
    if (!m_mentionsLabel || !m_store) return;
    const TerritorioStore::Territorio* t = m_currentTerritorioId.isEmpty()
        ? nullptr : m_store->territorio(m_currentTerritorioId);
    if (!t) {
        m_mentionsLabel->setText(tr("—"));
        return;
    }
    if (t->mentions.isEmpty()) {
        m_mentionsLabel->setText(tr("Nenhuma menção salva ainda."));
        return;
    }
    QStringList lines;
    for (const auto& m : t->mentions) {
        QString quote = m.text.trimmed();
        if (quote.size() > 80) quote = quote.left(80) + QStringLiteral("…");
        lines << (m.category.isEmpty()
            ? QStringLiteral("• “%1”").arg(quote)
            : QStringLiteral("• [%1] “%2”").arg(m.category, quote));
    }
    m_mentionsLabel->setText(lines.join(QStringLiteral("\n")));
}

void TerritorioWindow::openConstrutorNode(const QString& systemId, const QString& nodeId)
{
    if (!m_embeddedConstrutor) return;
    m_embeddedConstrutor->setTerritoryFilter(QString()); // garante que o alvo apareça, mesmo doutro território
    m_embeddedConstrutor->openNode(systemId, nodeId);
}

void TerritorioWindow::onToggleLeftPanel()
{
    if (m_leftPanel) m_leftPanel->setVisible(m_toggleLeftBtn->isChecked());
    if (m_vsep1) m_vsep1->setVisible(m_toggleLeftBtn->isChecked());
}

void TerritorioWindow::onToggleConstrutorPanel()
{
    const bool visible = m_toggleConstrutorBtn->isChecked();
    // Só reaparece de verdade se houver store — senão o painel continua
    // oculto (setConstrutorStore ainda controla essa visibilidade também).
    if (m_construtorPanel) m_construtorPanel->setVisible(visible && m_construtorStore != nullptr);
    if (m_vsep2) m_vsep2->setVisible(visible);
}

void TerritorioWindow::closeEvent(QCloseEvent* event)
{
    saveCurrentContent(); // no-op se o Construtor era o dono do editor
    if (m_embeddedConstrutor) m_embeddedConstrutor->flushPendingContent(); // idem, invertido
    event->accept();
}

void TerritorioWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    repositionLinksOverlay();
}

void TerritorioWindow::repositionLinksOverlay()
{
    if (!m_linksOverlay || !m_selector) return;
    m_linksOverlay->setGeometry(m_selector->viewport()->rect());
    m_linksOverlay->raise();
    m_linksOverlay->update();
}

const TerritorioStore::TerritorioLink* TerritorioWindow::linkNear(const QPoint& pos, qreal threshold) const
{
    if (!m_store) return nullptr;
    for (const auto& link : m_store->links()) {
        QListWidgetItem* fromItem = nullptr;
        QListWidgetItem* toItem   = nullptr;
        for (int i = 0; i < m_selector->count(); ++i) {
            auto* it = m_selector->item(i);
            const QString id = it->data(Qt::UserRole).toString();
            if (id == link.fromTerritorioId) fromItem = it;
            if (id == link.toTerritorioId)   toItem   = it;
        }
        if (!fromItem || !toItem) continue;
        const QPointF a = m_selector->visualItemRect(fromItem).center();
        const QPointF b = m_selector->visualItemRect(toItem).center();
        if (distanceToSegment(QPointF(pos), a, b) <= threshold)
            return &link;
    }
    return nullptr;
}

bool TerritorioWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (m_selector && watched == m_selector->viewport() && event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        const TerritorioStore::TerritorioLink* link = linkNear(me->pos());
        if (link) {
            if (me->button() == Qt::LeftButton) {
                loadLink(link->id);
                return true;
            }
            if (me->button() == Qt::RightButton) {
                const QString linkId = link->id;
                QMenu menu(this);
                menu.addAction(tr("Excluir vínculo"), this, [this, linkId]() {
                    if (!m_store) return;
                    if (linkId == m_currentLinkId) {
                        m_currentLinkId.clear();
                        showNoTerritorioOpenState();
                        if (!m_currentTerritorioId.isEmpty()) loadTerritorio(m_currentTerritorioId);
                    }
                    m_store->removeLink(linkId);
                    if (m_linksOverlay) m_linksOverlay->update();
                });
                menu.exec(me->globalPosition().toPoint());
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ── UI ────────────────────────────────────────────────────────────────────────

void TerritorioWindow::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Faixa superior — toggles de mostrar/ocultar os painéis laterais.
    // Fica fora dos painéis colapsáveis de propósito (senão, escondido o
    // painel, o próprio botão de trazer ele de volta sumiria junto).
    auto* topStrip = new QWidget(this);
    topStrip->setObjectName(QStringLiteral("terrTopStrip"));
    auto* topStripLay = new QHBoxLayout(topStrip);
    topStripLay->setContentsMargins(8, 4, 8, 4);
    topStripLay->setSpacing(6);
    m_toggleLeftBtn = new QPushButton(QStringLiteral("☰ Territórios"), topStrip);
    m_toggleLeftBtn->setObjectName(QStringLiteral("terrToggleBtn"));
    m_toggleLeftBtn->setCursor(Qt::PointingHandCursor);
    m_toggleLeftBtn->setCheckable(true);
    m_toggleLeftBtn->setChecked(true);
    topStripLay->addWidget(m_toggleLeftBtn);
    topStripLay->addStretch();
    m_toggleConstrutorBtn = new QPushButton(tr("Construtor ☰"), topStrip);
    m_toggleConstrutorBtn->setObjectName(QStringLiteral("terrToggleBtn"));
    m_toggleConstrutorBtn->setCursor(Qt::PointingHandCursor);
    m_toggleConstrutorBtn->setCheckable(true);
    m_toggleConstrutorBtn->setChecked(true);
    topStripLay->addWidget(m_toggleConstrutorBtn);
    outer->addWidget(topStrip);

    auto* root = new QHBoxLayout();
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    outer->addLayout(root, 1);

    m_leftPanel = new QWidget(this);
    m_leftPanel->setObjectName(QStringLiteral("terrLeft"));
    m_leftPanel->setFixedWidth(280);
    auto* leftLay = new QVBoxLayout(m_leftPanel);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(0);
    QWidget* leftPanel = m_leftPanel; // mantém os nomes locais abaixo intactos

    // ── Seletor de Território ────────────────────────────────────────────────
    auto* selHeader = new QWidget(leftPanel);
    selHeader->setObjectName(QStringLiteral("terrHeader"));
    auto* selHeaderLay = new QHBoxLayout(selHeader);
    selHeaderLay->setContentsMargins(12, 10, 12, 6);
    auto* selTitle = new QLabel(tr("TERRITÓRIOS"), selHeader);
    selTitle->setObjectName(QStringLiteral("terrSectionTitle"));
    selHeaderLay->addWidget(selTitle);
    leftLay->addWidget(selHeader);

    m_selector = new QListWidget(leftPanel);
    m_selector->setObjectName(QStringLiteral("terrSelector"));
    m_selector->setViewMode(QListView::IconMode);
    m_selector->setMovement(QListView::Static);
    m_selector->setResizeMode(QListView::Adjust);
    m_selector->setIconSize(QSize(kAvatarSize, kAvatarSize));
    m_selector->setGridSize(QSize(104, 112));
    m_selector->setWordWrap(true);
    m_selector->setFrameShape(QFrame::NoFrame);
    m_selector->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_selector->setMaximumHeight(240);
    m_selector->setContextMenuPolicy(Qt::CustomContextMenu);
    m_selector->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    leftLay->addWidget(m_selector);

    m_linksOverlay = new LinksOverlay(m_selector->viewport());
    static_cast<LinksOverlay*>(m_linksOverlay)->paintFn = [this](QPainter& p) {
        if (!m_store) return;
        for (const auto& link : m_store->links()) {
            QListWidgetItem* fromItem = nullptr;
            QListWidgetItem* toItem   = nullptr;
            for (int i = 0; i < m_selector->count(); ++i) {
                auto* it = m_selector->item(i);
                const QString id = it->data(Qt::UserRole).toString();
                if (id == link.fromTerritorioId) fromItem = it;
                if (id == link.toTerritorioId)   toItem   = it;
            }
            if (!fromItem || !toItem) continue;
            const QPoint a = m_selector->visualItemRect(fromItem).center();
            const QPoint b = m_selector->visualItemRect(toItem).center();
            QColor color = link.color.isEmpty() ? QColor(Theme::accentDefault()) : QColor(link.color);
            color.setAlpha(200);
            QPen pen(color, link.id == m_currentLinkId ? 3.0 : 2.0);
            p.setPen(pen);
            p.drawLine(a, b);
        }
    };
    m_selector->viewport()->installEventFilter(this);

    auto* newTerrWrap = new QWidget(leftPanel);
    auto* newTerrLay  = new QHBoxLayout(newTerrWrap);
    newTerrLay->setContentsMargins(12, 4, 12, 8);
    m_newTerritorioBtn = new QPushButton(tr("+ Novo território"), newTerrWrap);
    m_newTerritorioBtn->setObjectName(QStringLiteral("terrNewBtn"));
    m_newTerritorioBtn->setCursor(Qt::PointingHandCursor);
    newTerrLay->addWidget(m_newTerritorioBtn);
    leftLay->addWidget(newTerrWrap);

    auto makeHSep = [&](QWidget* parent) {
        auto* s = new QFrame(parent);
        s->setFrameShape(QFrame::HLine);
        s->setObjectName(QStringLiteral("terrHSep"));
        return s;
    };
    leftLay->addWidget(makeHSep(leftPanel));

    // ── Explorador de Território ─────────────────────────────────────────────
    auto* expHeader = new QWidget(leftPanel);
    expHeader->setObjectName(QStringLiteral("terrHeader"));
    auto* expHeaderLay = new QHBoxLayout(expHeader);
    expHeaderLay->setContentsMargins(12, 10, 12, 4);
    auto* expTitle = new QLabel(tr("PERSONAGENS"), expHeader);
    expTitle->setObjectName(QStringLiteral("terrSectionTitle"));
    expHeaderLay->addWidget(expTitle);
    leftLay->addWidget(expHeader);

    m_charactersLabel = new QLabel(leftPanel);
    m_charactersLabel->setObjectName(QStringLiteral("terrCharactersLabel"));
    m_charactersLabel->setWordWrap(true);
    m_charactersLabel->setContentsMargins(12, 0, 12, 8);
    leftLay->addWidget(m_charactersLabel);

    leftLay->addWidget(makeHSep(leftPanel));

    auto* eventsHeader = new QLabel(tr("O QUE ACONTECEU AQUI"), leftPanel);
    eventsHeader->setObjectName(QStringLiteral("terrSectionTitle"));
    eventsHeader->setContentsMargins(12, 6, 12, 0);
    leftLay->addWidget(eventsHeader);
    m_placeEventsLabel = new QLabel(leftPanel);
    m_placeEventsLabel->setObjectName(QStringLiteral("terrCharactersLabel"));
    m_placeEventsLabel->setWordWrap(true);
    m_placeEventsLabel->setContentsMargins(12, 0, 12, 8);
    leftLay->addWidget(m_placeEventsLabel);

    auto* mentionsHeader = new QLabel(tr("MENÇÕES"), leftPanel);
    mentionsHeader->setObjectName(QStringLiteral("terrSectionTitle"));
    mentionsHeader->setContentsMargins(12, 6, 12, 0);
    leftLay->addWidget(mentionsHeader);
    m_mentionsLabel = new QLabel(leftPanel);
    m_mentionsLabel->setObjectName(QStringLiteral("terrCharactersLabel"));
    m_mentionsLabel->setWordWrap(true);
    m_mentionsLabel->setContentsMargins(12, 0, 12, 8);
    leftLay->addWidget(m_mentionsLabel);

    leftLay->addWidget(makeHSep(leftPanel));

    auto* nodeBtns = new QHBoxLayout();
    nodeBtns->setContentsMargins(12, 6, 12, 4);
    nodeBtns->setSpacing(4);
    m_addFolderBtn = new QPushButton(tr("+ Pasta"), leftPanel);
    m_addFolderBtn->setObjectName(QStringLiteral("terrAddBtn"));
    m_addFolderBtn->setCursor(Qt::PointingHandCursor);
    m_addFolderBtn->setToolTip(tr("Adicionar pasta"));
    nodeBtns->addWidget(m_addFolderBtn);
    m_addDocBtn = new QPushButton(tr("+ Documento"), leftPanel);
    m_addDocBtn->setObjectName(QStringLiteral("terrAddBtn"));
    m_addDocBtn->setCursor(Qt::PointingHandCursor);
    m_addDocBtn->setToolTip(tr("Adicionar documento"));
    nodeBtns->addWidget(m_addDocBtn);
    nodeBtns->addStretch();
    m_deleteNodeBtn = new QPushButton(QStringLiteral("✕"), leftPanel);
    m_deleteNodeBtn->setObjectName(QStringLiteral("terrDeleteBtn"));
    m_deleteNodeBtn->setCursor(Qt::PointingHandCursor);
    m_deleteNodeBtn->setEnabled(false);
    m_deleteNodeBtn->setToolTip(tr("Excluir item selecionado"));
    m_deleteNodeBtn->setFixedSize(26, 26);
    nodeBtns->addWidget(m_deleteNodeBtn);
    leftLay->addLayout(nodeBtns);

    m_tree = new QTreeWidget(leftPanel);
    m_tree->setObjectName(QStringLiteral("terrTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    leftLay->addWidget(m_tree, 1);

    root->addWidget(leftPanel);

    m_vsep1 = new QFrame(this);
    m_vsep1->setFrameShape(QFrame::VLine);
    m_vsep1->setObjectName(QStringLiteral("terrVSep"));
    root->addWidget(m_vsep1);

    m_editor = new WorldContentEditor(this);
    root->addWidget(m_editor, 1);

    m_vsep2 = new QFrame(this);
    m_vsep2->setFrameShape(QFrame::VLine);
    m_vsep2->setObjectName(QStringLiteral("terrVSep"));
    root->addWidget(m_vsep2);

    // ── Construtor absorvido — embutido de verdade, não uma lista reduzida.
    // A ConstrutorWindow em si (m_embeddedConstrutor) só é criada em
    // setConstrutorStore(), quando a store chega — aqui só preparamos o
    // container que vai hospedá-la.
    // Largura fixa, como o painel de Território — hospeda só a navegação do
    // Construtor (lista de sistemas + árvore de nós), não um editor (o
    // conteúdo vai todo pro m_editor compartilhado acima).
    m_construtorPanel = new QWidget(this);
    m_construtorPanel->setObjectName(QStringLiteral("terrLeft")); // mesmo fundo do painel esquerdo
    m_construtorPanel->setFixedWidth(280);
    auto* ctrPanelLay = new QVBoxLayout(m_construtorPanel);
    ctrPanelLay->setContentsMargins(0, 0, 0, 0);
    ctrPanelLay->setSpacing(0);
    root->addWidget(m_construtorPanel);
    m_construtorPanel->setVisible(false); // só aparece se setConstrutorStore() for chamado

    connect(m_toggleLeftBtn, &QPushButton::toggled, this, &TerritorioWindow::onToggleLeftPanel);
    connect(m_toggleConstrutorBtn, &QPushButton::toggled, this, &TerritorioWindow::onToggleConstrutorPanel);

    connect(m_selector, &QListWidget::itemClicked, this, &TerritorioWindow::onTerritorioClicked);
    connect(m_selector, &QListWidget::itemChanged, this, &TerritorioWindow::onTerritorioItemChanged);
    connect(m_selector, &QListWidget::customContextMenuRequested,
            this, &TerritorioWindow::onTerritorioContextMenu);
    connect(m_newTerritorioBtn, &QPushButton::clicked, this, &TerritorioWindow::onNewTerritorio);

    connect(m_addFolderBtn, &QPushButton::clicked, this, &TerritorioWindow::onAddFolder);
    connect(m_addDocBtn,    &QPushButton::clicked, this, &TerritorioWindow::onAddDoc);
    connect(m_deleteNodeBtn, &QPushButton::clicked, this, &TerritorioWindow::onDeleteNode);
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &TerritorioWindow::onTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::itemChanged,
            this, &TerritorioWindow::onTreeItemChanged);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &TerritorioWindow::onTreeContextMenu);

    connect(m_editor, &WorldContentEditor::contentChanged,
            this, &TerritorioWindow::onEditorContentChanged);
}

void TerritorioWindow::applyTheme()
{
    const QString panelBg   = Theme::panelBackground();
    const QString border    = Theme::panelBorder();
    const QString subtle    = Theme::subtleBorder();
    const QString txtPrim   = Theme::textPrimary();
    const QString txtMuted  = Theme::textMuted();
    const QString txtBright = Theme::textBright();
    const QString hover     = Theme::hoverOverlay();
    const QString accentSf  = Theme::accentInfoSoft();
    const QString accentBd  = Theme::accentInfoBorderSoft();
    const QString accentDef = Theme::accentDefault();

    setStyleSheet(QStringLiteral(R"(
        TerritorioWindow { background: %1; }
        QWidget#terrLeft { background: %1; }
        QWidget#terrHeader { background: %1; }
        QWidget#terrTopStrip { background: %1; border-bottom: 1px solid %3; }
        QPushButton#terrToggleBtn {
            background: transparent; color: %5; border: 1px solid %3;
            border-radius: 6px; padding: 4px 10px; font-size: 11px;
        }
        QPushButton#terrToggleBtn:hover { background: %7; color: %6; }
        QPushButton#terrToggleBtn:checked { background: %8; color: %6; border-color: %9; }
        QLabel#terrSectionTitle {
            color: %5; font-size: 10px; font-weight: 700; letter-spacing: 1px;
        }
        QLabel#terrCharactersLabel { color: %5; font-size: 11px; font-style: italic; }
        QFrame#terrHSep { background: %3; border: none; max-height: 1px; margin: 4px 0; }
        QFrame#terrVSep { background: %3; border: none; max-width: 1px; }

        QListWidget#terrSelector {
            background: transparent; color: %4; border: none; outline: none;
        }
        QListWidget#terrSelector::item { border-radius: 8px; padding: 4px; }
        QListWidget#terrSelector::item:hover { background: %7; }
        QListWidget#terrSelector::item:selected { background: %8; color: %6; }

        QPushButton#terrNewBtn {
            background: %8; color: %6; border: 1px solid %9;
            border-radius: 6px; padding: 6px 10px; font-size: 12px; font-weight: 600;
        }
        QPushButton#terrNewBtn:hover { background: %9; }
        QPushButton#terrAddBtn {
            background: %8; color: %6; border: 1px solid %9;
            border-radius: 6px; padding: 4px 10px; font-size: 11px;
        }
        QPushButton#terrAddBtn:hover { background: %9; }
        QPushButton#terrDeleteBtn {
            background: transparent; color: %5; border: 1px solid %3;
            border-radius: 6px; font-size: 13px; font-weight: 600;
        }
        QPushButton#terrDeleteBtn:hover { background: %2; color: %10; border-color: %10; }
        QPushButton#terrDeleteBtn:disabled { color: %5; border-color: %3; }

        QTreeWidget#terrTree {
            background: %1; color: %4; border: none; font-size: 13px; outline: none;
        }
        QTreeWidget#terrTree::item { padding: 4px 6px; border-radius: 4px; }
        QTreeWidget#terrTree::item:hover { background: %7; color: %6; }
        QTreeWidget#terrTree::item:selected { background: %8; color: %6; }
    )").arg(panelBg, hover, subtle, txtPrim, txtMuted)   // %1-5
       .arg(txtBright, hover, accentSf, accentBd)         // %6-9
       .arg(Theme::accentDanger()));                      // %10
}

// ── Seletor de Território ─────────────────────────────────────────────────────

void TerritorioWindow::rebuildSelector()
{
    if (!m_store) return;

    const QString currentId = m_currentTerritorioId;
    m_rebuilding = true;
    m_selector->blockSignals(true);
    m_selector->clear();

    for (const auto& t : m_store->territorios()) {
        auto* item = new QListWidgetItem(m_selector);
        item->setIcon(QIcon(AvatarUtils::circularAvatar(t.avatarDataUrl, t.name, t.id, kAvatarSize)));
        item->setText(t.name);
        item->setTextAlignment(Qt::AlignHCenter);
        item->setToolTip(plainTextPreview(t.content, 240));
        item->setData(Qt::UserRole, t.id);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        if (t.id == currentId) m_selector->setCurrentItem(item);
    }

    m_selector->blockSignals(false);
    m_rebuilding = false;

    if (m_selector->currentItem() == nullptr && !m_currentTerritorioId.isEmpty()) {
        m_currentTerritorioId.clear();
        m_currentNodeId.clear();
        showNoTerritorioOpenState();
    }
    repositionLinksOverlay();
}

void TerritorioWindow::onNewTerritorio()
{
    if (!m_store) return;
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Novo território"), tr("Nome:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    const QString id = m_store->addTerritorio(name.trimmed());
    selectTerritorioAndNode(id, QString());
}

void TerritorioWindow::onTerritorioClicked(QListWidgetItem* item)
{
    if (!item) return;
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty() || id == m_currentTerritorioId) return;
    loadTerritorio(id);
}

void TerritorioWindow::onTerritorioItemChanged(QListWidgetItem* item)
{
    if (m_rebuilding || !item || !m_store) return;
    const QString id = item->data(Qt::UserRole).toString();
    const QString name = item->text().trimmed();
    if (id.isEmpty() || name.isEmpty()) { rebuildSelector(); return; }
    m_store->updateTerritorio(id, name);
}

void TerritorioWindow::onTerritorioContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_selector->itemAt(pos);
    if (!item || !m_store) return;
    const QString id = item->data(Qt::UserRole).toString();

    QMenu menu(this);
    QAction* changeImg = menu.addAction(tr("Trocar imagem…"));

    QMenu* linkMenu = menu.addMenu(tr("Vincular a…"));
    QHash<QAction*, QString> linkTargets;
    for (const auto& other : m_store->territorios()) {
        if (other.id == id) continue;
        if (m_store->linkBetween(id, other.id)) continue; // já vinculados
        QAction* a = linkMenu->addAction(other.name);
        linkTargets.insert(a, other.id);
    }
    linkMenu->setEnabled(!linkTargets.isEmpty());

    menu.addSeparator();
    QAction* del = menu.addAction(tr("Excluir território"));
    QAction* chosen = menu.exec(m_selector->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (linkTargets.contains(chosen)) {
        m_store->addLink(id, linkTargets.value(chosen));
        if (m_linksOverlay) m_linksOverlay->update();
        return;
    }

    if (chosen == changeImg) {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Escolher imagem"), QString(),
            tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"));
        if (path.isEmpty()) return;
        QImage img(path);
        if (img.isNull()) return;
        const QString dataUrl = AvatarUtils::encodeDataUrl(img, /*maxSide=*/480, /*jpegQuality=*/85);
        m_store->updateTerritorioAvatar(id, dataUrl);
    } else if (chosen == del) {
        const TerritorioStore::Territorio* t = m_store->territorio(id);
        const QString name = t ? t->name : QString();
        const auto r = QMessageBox::question(
            this, tr("Excluir território"),
            tr("Excluir o território \"%1\" e tudo o que ele contém?\n\nEssa ação não pode ser desfeita.")
                .arg(name),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (r != QMessageBox::Yes) return;
        if (id == m_currentTerritorioId) {
            m_currentTerritorioId.clear();
            m_currentNodeId.clear();
            showNoTerritorioOpenState();
        }
        m_store->removeTerritorio(id);
    }
}

// ── Carrega território selecionado ────────────────────────────────────────────

void TerritorioWindow::selectTerritorioAndNode(const QString& territorioId, const QString& nodeId)
{
    for (int i = 0; i < m_selector->count(); ++i) {
        if (m_selector->item(i)->data(Qt::UserRole).toString() == territorioId) {
            m_selector->setCurrentItem(m_selector->item(i));
            break;
        }
    }
    loadTerritorio(territorioId);
    if (nodeId.isEmpty()) return;
    const auto items = m_tree->findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
    for (auto* item : items) {
        if (item->data(0, kNodeIdRole).toString() == nodeId) {
            m_tree->setCurrentItem(item);
            break;
        }
    }
}

void TerritorioWindow::loadTerritorio(const QString& id)
{
    const TerritorioStore::Territorio* t = m_store ? m_store->territorio(id) : nullptr;
    if (!t) return;

    saveCurrentContent(); // flush território, se era o dono do editor
    if (m_embeddedConstrutor) m_embeddedConstrutor->flushPendingContent(); // idem pro Construtor
    m_editorActiveHere = true;
    if (m_embeddedConstrutor) m_embeddedConstrutor->setEditorActive(false);
    m_currentLinkId.clear();
    m_currentTerritorioId = id;
    m_currentNodeId.clear();

    rebuildTree();

    m_editor->setPlaceholderText(
        tr("Escreva a lore, o resumo ou a história deste território…"));
    m_editor->setContent(t->content);
    updateLastEditedLabel(t->updatedAt);
    m_deleteNodeBtn->setEnabled(false);
    m_charactersLabel->setText(tr("Nenhum personagem vinculado ainda."));
    if (m_embeddedConstrutor) m_embeddedConstrutor->setTerritoryFilter(id);
    rebuildPlaceEvents();
    rebuildMentions();
}

void TerritorioWindow::showNoTerritorioOpenState()
{
    m_editorActiveHere = true;
    if (m_embeddedConstrutor) m_embeddedConstrutor->setEditorActive(false);
    if (m_tree) m_tree->clear();
    if (m_editor) {
        m_editor->setEditorEnabled(false);
        m_editor->setPlaceholderText(
            tr("Nenhum território aberto. Selecione um território acima ou crie um novo para começar."));
    }
    updateLastEditedLabel(0);
    if (m_charactersLabel) m_charactersLabel->setText(tr("Nenhum território aberto."));
    if (m_deleteNodeBtn) m_deleteNodeBtn->setEnabled(false);
    if (m_embeddedConstrutor) m_embeddedConstrutor->setTerritoryFilter(QString());
    rebuildPlaceEvents();
    rebuildMentions();
}

void TerritorioWindow::updateLastEditedLabel(qint64 updatedAt)
{
    if (!m_editor) return;
    if (updatedAt <= 0) {
        m_editor->setStatusText(QString());
        return;
    }
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(updatedAt);
    m_editor->setStatusText(
        tr("Editado em %1").arg(dt.toString(QStringLiteral("dd/MM/yyyy HH:mm"))));
}

// ── Árvore de pastas/documentos ───────────────────────────────────────────────

void TerritorioWindow::rebuildTree()
{
    const TerritorioStore::Territorio* t =
        (m_store && !m_currentTerritorioId.isEmpty())
            ? m_store->territorio(m_currentTerritorioId)
            : nullptr;
    if (!t) return;

    m_rebuilding = true;
    m_tree->blockSignals(true);
    m_tree->clear();

    for (const auto& node : t->nodes)
        populateTreeNode(nullptr, node);

    m_tree->expandAll();

    if (!m_currentNodeId.isEmpty()) {
        const auto items = m_tree->findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
        for (auto* item : items) {
            if (item->data(0, kNodeIdRole).toString() == m_currentNodeId) {
                m_tree->setCurrentItem(item);
                break;
            }
        }
    }

    m_tree->blockSignals(false);
    m_rebuilding = false;
}

void TerritorioWindow::populateTreeNode(QTreeWidgetItem* parent, const TerritorioStore::Node& node)
{
    QTreeWidgetItem* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);

    const bool isFolder = (node.type == TerritorioStore::NodeType::Folder);
    item->setText(0, (isFolder ? QStringLiteral("📁 ") : QStringLiteral("📄 ")) + node.name);
    item->setData(0, kNodeIdRole,   node.id);
    item->setData(0, kNodeTypeRole, static_cast<int>(node.type));
    item->setFlags(item->flags() | Qt::ItemIsEditable);

    for (const auto& child : node.children)
        populateTreeNode(item, child);
}

void TerritorioWindow::onTreeSelectionChanged()
{
    if (m_rebuilding) return;
    saveCurrentContent();
    if (m_embeddedConstrutor) m_embeddedConstrutor->flushPendingContent();
    m_editorActiveHere = true;
    if (m_embeddedConstrutor) m_embeddedConstrutor->setEditorActive(false);
    m_currentLinkId.clear();

    const QString nodeId = selectedNodeId();
    m_currentNodeId = nodeId;

    const TerritorioStore::Territorio* t =
        m_store ? m_store->territorio(m_currentTerritorioId) : nullptr;

    if (nodeId.isEmpty()) {
        m_deleteNodeBtn->setEnabled(false);
        if (!t) {
            m_editor->setEditorEnabled(false);
            updateLastEditedLabel(0);
            return;
        }
        m_editor->setPlaceholderText(
            tr("Escreva a lore, o resumo ou a história deste território…"));
        m_editor->setContent(t->content);
        updateLastEditedLabel(t->updatedAt);
        return;
    }

    m_deleteNodeBtn->setEnabled(true);
    if (!t) return;

    std::function<const TerritorioStore::Node*(const QList<TerritorioStore::Node>&)>
        findConst = [&](const QList<TerritorioStore::Node>& nodes)
            -> const TerritorioStore::Node* {
        for (const auto& n : nodes) {
            if (n.id == nodeId) return &n;
            const auto* c = findConst(n.children);
            if (c) return c;
        }
        return nullptr;
    };
    const TerritorioStore::Node* node = findConst(t->nodes);
    if (!node) return;

    m_editor->setPlaceholderText(tr("Escreva aqui…"));
    m_editor->setContent(node->content);
    updateLastEditedLabel(node->updatedAt);
}

void TerritorioWindow::onTreeItemChanged(QTreeWidgetItem* item, int column)
{
    if (m_rebuilding || !item || column != 0 || !m_store) return;

    QString text = item->text(0);
    if (text.startsWith(QStringLiteral("📁 ")) || text.startsWith(QStringLiteral("📄 ")))
        text = text.mid(3);

    const QString nodeId = item->data(0, kNodeIdRole).toString();
    if (nodeId.isEmpty() || text.isEmpty()) { rebuildTree(); return; }

    const TerritorioStore::Territorio* t = m_store->territorio(m_currentTerritorioId);
    if (!t) return;
    std::function<const TerritorioStore::Node*(const QList<TerritorioStore::Node>&)>
        findConst = [&](const QList<TerritorioStore::Node>& nodes)
            -> const TerritorioStore::Node* {
        for (const auto& n : nodes) {
            if (n.id == nodeId) return &n;
            const auto* c = findConst(n.children);
            if (c) return c;
        }
        return nullptr;
    };
    const TerritorioStore::Node* node = findConst(t->nodes);
    const QString content = node ? node->content : QString();

    m_store->updateNode(m_currentTerritorioId, nodeId, text, content);
}

void TerritorioWindow::onTreeContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item) return;
    const QString nodeId = item->data(0, kNodeIdRole).toString();
    if (nodeId.isEmpty()) return;

    QMenu menu(this);
    menu.addAction(tr("Adicionar Pasta filha"), this,
                   [this]() { addChildNode(TerritorioStore::NodeType::Folder); });
    menu.addAction(tr("Adicionar Documento filho"), this,
                   [this]() { addChildNode(TerritorioStore::NodeType::Doc); });
    menu.addSeparator();
    menu.addAction(tr("Excluir"), this, &TerritorioWindow::onDeleteNode);
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void TerritorioWindow::onAddFolder() { addChildNode(TerritorioStore::NodeType::Folder); }
void TerritorioWindow::onAddDoc()    { addChildNode(TerritorioStore::NodeType::Doc); }

void TerritorioWindow::addChildNode(TerritorioStore::NodeType type)
{
    if (!m_store || m_currentTerritorioId.isEmpty()) return;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, type == TerritorioStore::NodeType::Folder ? tr("Nova pasta") : tr("Novo documento"),
        tr("Nome:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    const QString parentId = selectedNodeId();
    const QString newId = m_store->addNode(m_currentTerritorioId, parentId, type, name.trimmed());

    m_currentNodeId = newId;
    rebuildTree();
}

void TerritorioWindow::onDeleteNode()
{
    if (!m_store || m_currentTerritorioId.isEmpty() || m_currentNodeId.isEmpty()) return;
    const auto r = QMessageBox::question(
        this, tr("Excluir item"),
        tr("Excluir este item e tudo o que ele contém?\n\nEssa ação não pode ser desfeita."),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (r != QMessageBox::Yes) return;

    const QString nodeId = m_currentNodeId;
    m_currentNodeId.clear();
    m_deleteNodeBtn->setEnabled(false);

    const TerritorioStore::Territorio* t = m_store->territorio(m_currentTerritorioId);
    if (t) {
        m_editor->setPlaceholderText(
            tr("Escreva a lore, o resumo ou a história deste território…"));
        m_editor->setContent(t->content);
    } else {
        m_editor->setEditorEnabled(false);
    }
    m_store->removeNode(m_currentTerritorioId, nodeId);
}

// ── Conteúdo (editor compartilhado) ───────────────────────────────────────────

void TerritorioWindow::onEditorContentChanged()
{
    saveCurrentContent();
}

void TerritorioWindow::loadLink(const QString& linkId)
{
    const TerritorioStore::TerritorioLink* link = m_store ? m_store->link(linkId) : nullptr;
    if (!link) return;

    saveCurrentContent(); // flush o que estava aberto antes (território/nó ou outro vínculo)
    if (m_embeddedConstrutor) m_embeddedConstrutor->flushPendingContent();
    m_editorActiveHere = true;
    if (m_embeddedConstrutor) m_embeddedConstrutor->setEditorActive(false);
    m_currentLinkId = linkId;

    m_editor->setPlaceholderText(
        tr("Escreva a história compartilhada entre os dois territórios — guerras, alianças, passado…"));
    m_editor->setContent(link->docContent);
    updateLastEditedLabel(link->updatedAt);
    m_deleteNodeBtn->setEnabled(false);
    if (m_linksOverlay) m_linksOverlay->update();
}

void TerritorioWindow::saveCurrentContent()
{
    if (!m_store || !m_editor || !m_editorActiveHere) return;

    if (!m_currentLinkId.isEmpty()) {
        const TerritorioStore::TerritorioLink* link = m_store->link(m_currentLinkId);
        if (!link) return;
        const QString newContent = m_editor->content();
        if (link->docContent == newContent) return;
        m_store->updateLinkContent(m_currentLinkId, newContent);
        updateLastEditedLabel(QDateTime::currentMSecsSinceEpoch());
        return;
    }

    if (m_currentTerritorioId.isEmpty()) return;
    const TerritorioStore::Territorio* t = m_store->territorio(m_currentTerritorioId);
    if (!t) return;

    const QString newContent = m_editor->content();

    if (m_currentNodeId.isEmpty()) {
        if (t->content == newContent) return;
        m_store->updateTerritorioContent(m_currentTerritorioId, newContent);
        updateLastEditedLabel(QDateTime::currentMSecsSinceEpoch());
        return;
    }

    std::function<const TerritorioStore::Node*(const QList<TerritorioStore::Node>&)>
        findConst = [&](const QList<TerritorioStore::Node>& nodes)
            -> const TerritorioStore::Node* {
        for (const auto& n : nodes) {
            if (n.id == m_currentNodeId) return &n;
            const auto* c = findConst(n.children);
            if (c) return c;
        }
        return nullptr;
    };
    const TerritorioStore::Node* node = findConst(t->nodes);
    if (!node) return;

    if (node->content == newContent) return;
    m_store->updateNode(m_currentTerritorioId, m_currentNodeId, node->name, newContent);
    updateLastEditedLabel(QDateTime::currentMSecsSinceEpoch());
}

// ── Store changed ─────────────────────────────────────────────────────────────

void TerritorioWindow::onStoreChanged()
{
    rebuildSelector();
    if (!m_currentTerritorioId.isEmpty()) {
        const TerritorioStore::Territorio* t =
            m_store ? m_store->territorio(m_currentTerritorioId) : nullptr;
        if (!t) {
            m_currentTerritorioId.clear();
            showNoTerritorioOpenState();
        } else {
            rebuildTree();
            rebuildMentions();
        }
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString TerritorioWindow::selectedTerritorioId() const
{
    const QListWidgetItem* item = m_selector ? m_selector->currentItem() : nullptr;
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString TerritorioWindow::selectedNodeId() const
{
    const QTreeWidgetItem* item = m_tree ? m_tree->currentItem() : nullptr;
    return item ? item->data(0, kNodeIdRole).toString() : QString();
}

// ── Navegação externa (menções, Ctrl+clique) ─────────────────────────────────

void TerritorioWindow::openNode(const QString& territorioId, const QString& nodeId)
{
    selectTerritorioAndNode(territorioId, nodeId);
}
