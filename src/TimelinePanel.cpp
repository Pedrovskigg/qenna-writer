#include "TimelinePanel.h"

#include "ElementsStore.h"
#include "IconUtils.h"
#include "ProjectModel.h"
#include "RoleTiers.h"
#include "Theme.h"

#include <QAction>
#include <QComboBox>
#include <QHash>
#include <QMessageBox>
#include <QRegularExpression>
#include "TimelineEventItem.h"
#include "TimelineEventPopup.h"
#include "TimelineScene.h"
#include "TimelineView.h"

#include <QCloseEvent>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QPixmap>
#include <QResizeEvent>
#include <QSaveFile>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int kToolbarH = 46;
constexpr int kBtnSize  = 32;
constexpr int kIconSize = 20;

QToolButton* makeBtn(const QString& text, const QString& tip, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setText(text);
    b->setToolTip(tip);
    b->setObjectName(QStringLiteral("tlToolBtn"));
    b->setFixedHeight(kBtnSize);
    b->setMinimumWidth(kBtnSize);
    b->setCursor(Qt::PointingHandCursor);
    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    return b;
}

QFrame* makeSep(QWidget* parent)
{
    auto* f = new QFrame(parent);
    f->setFrameShape(QFrame::VLine);
    f->setObjectName(QStringLiteral("tlSep"));
    f->setFixedWidth(1);
    return f;
}

QPixmap colorDot(const QColor& c, int sz = 14)
{
    QPixmap px(sz, sz);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(c); p.setPen(c.darker(130));
    p.drawEllipse(1, 1, sz-2, sz-2);
    return px;
}
} // namespace

TimelinePanel::TimelinePanel(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("timelinePanel"));
    setWindowTitle(tr("Linha do tempo"));
    setMinimumSize(400, 400);
    resize(620, 620);
    buildUi();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &TimelinePanel::applyTheme);
}

void TimelinePanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    m_toolbar = new QWidget(this);
    m_toolbar->setObjectName(QStringLiteral("tlToolbar"));
    m_toolbar->setFixedHeight(kToolbarH);

    auto* tl = new QHBoxLayout(m_toolbar);
    tl->setContentsMargins(12, 0, 12, 0);
    tl->setSpacing(6);

    auto* title = new QLabel(tr("Linha do Tempo"), m_toolbar);
    title->setObjectName(QStringLiteral("tlTitle"));
    tl->addWidget(title);

    tl->addWidget(makeSep(m_toolbar));

    auto* btnNewTimeline = makeBtn(tr("+ Timeline"), tr("Nova linha do tempo"), m_toolbar);
    tl->addWidget(btnNewTimeline);

    tl->addWidget(makeSep(m_toolbar));

    m_btnView = makeBtn(QString(), tr("Alternar entre Trilho, Ramificações e Espiral"), m_toolbar);
    m_btnAxis = makeBtn(QString(), tr("Narrativa: presença dos personagens por cena  ·  "
                                      "História: eventos do enredo"), m_toolbar);
    tl->addWidget(m_btnView);
    tl->addWidget(m_btnAxis);

    tl->addWidget(makeSep(m_toolbar));

    // ── Foco / Filtro por linha ────────────────────────────────────────────────
    m_btnFocus = makeBtn(QString(), tr("Focar em uma linha (esmaece o resto)"), m_toolbar);
    m_btnFocus->setPopupMode(QToolButton::InstantPopup);
    m_focusMenu = new QMenu(m_btnFocus);
    m_btnFocus->setMenu(m_focusMenu);

    m_btnDepth = makeBtn(QString(), tr("Alcance do foco em saltos pelas conexões"), m_toolbar);
    m_btnDepth->setPopupMode(QToolButton::InstantPopup);
    auto* depthMenu = new QMenu(m_btnDepth);
    for (int d = 0; d <= 3; ++d) {
        const QString label = d == 0 ? tr("Só a linha")
                            : d == 1 ? tr("1 salto")
                                     : tr("%1 saltos").arg(d);
        QAction* a = depthMenu->addAction(label);
        a->setData(d);
        connect(a, &QAction::triggered, this, [this, d]() { setFocusDepth(d); });
    }
    m_btnDepth->setMenu(depthMenu);
    tl->addWidget(m_btnFocus);
    tl->addWidget(m_btnDepth);

    tl->addStretch();

    auto* btnClose = makeBtn(QStringLiteral("×"), tr("Fechar"), m_toolbar);
    tl->addWidget(btnClose);

    connect(btnNewTimeline, &QToolButton::clicked, this, &TimelinePanel::createTimeline);
    connect(m_btnView, &QToolButton::clicked, this, &TimelinePanel::toggleViewMode);
    connect(m_btnAxis, &QToolButton::clicked, this, &TimelinePanel::toggleAxisMode);
    connect(btnClose, &QToolButton::clicked, this, &TimelinePanel::closeRequested);

    root->addWidget(m_toolbar);

    // ── Canvas ───────────────────────────────────────────────────────────────
    m_scene = new TimelineScene(this);
    m_view  = new TimelineView(m_scene, this);
    root->addWidget(m_view, 1);

    // Botão "+" flutuante sobre o canvas (canto superior esquerdo) → novo evento.
    m_btnAdd = new QToolButton(m_view);
    m_btnAdd->setObjectName(QStringLiteral("tlAddOverlay"));
    m_btnAdd->setText(QStringLiteral("+"));
    m_btnAdd->setToolTip(tr("Novo evento"));
    m_btnAdd->setCursor(Qt::PointingHandCursor);
    m_btnAdd->setFixedSize(36, 36);
    m_btnAdd->move(12, 12);
    m_btnAdd->raise();
    connect(m_btnAdd, &QToolButton::clicked, this, [this]() {
        // cria no centro do que está visível agora
        const QPointF c = m_view->mapToScene(m_view->viewport()->rect().center());
        createEventAt(c);
    });

    connect(m_scene, &TimelineScene::eventDataChanged,  this, [this]() { save(); });
    connect(m_scene, &TimelineScene::eventEditRequested, this, &TimelinePanel::openEditPopup);
    connect(m_scene, &TimelineScene::exportEventAsDoc,   this, &TimelinePanel::onExportEventAsDoc);
    connect(m_scene, &TimelineScene::focusChanged, this, &TimelinePanel::refreshFocusButtons);
    connect(m_scene, &TimelineScene::editTimelineRequested, this, &TimelinePanel::editTimeline);
    // clique no fundo (sem arrastar): foca a faixa clicada ou limpa o foco
    connect(m_view, &TimelineView::bgClicked, this,
            [this](const QPointF& scenePos, Qt::KeyboardModifiers mods) {
        const QString id = m_scene->timelineIdAtRailPos(scenePos);
        m_scene->focusLine(id, mods.testFlag(Qt::ShiftModifier)); // id vazio = limpa
    });

    refreshModeButtons();
    rebuildFocusMenu();
    refreshFocusButtons();
}

void TimelinePanel::toggleViewMode()
{
    if (!m_scene) return;
    using VM = TimelineScene::ViewMode;
    const VM cur  = m_scene->viewMode();
    const VM next = (cur == VM::Rail)          ? VM::Constellation
                  : (cur == VM::Constellation) ? VM::Spiral
                                               : VM::Rail;
    m_scene->setViewMode(next);
    if      (next == VM::Rail   && m_view) m_view->scrollToRailStart();
    else if (next == VM::Spiral && m_view) m_view->fitAll(); // enquadra a espiral
    refreshModeButtons();
    save();
}

void TimelinePanel::toggleAxisMode()
{
    if (!m_scene) return;
    const bool narr = (m_scene->axisMode() == TimelineScene::AxisMode::Narrative);
    m_scene->setAxisMode(narr ? TimelineScene::AxisMode::Story
                              : TimelineScene::AxisMode::Narrative);
    refreshModeButtons();
    save();
}

void TimelinePanel::refreshModeButtons()
{
    if (!m_scene) return;
    if (m_btnView) {
        const auto vm = m_scene->viewMode();
        m_btnView->setText(vm == TimelineScene::ViewMode::Rail          ? tr("Trilho")
                         : vm == TimelineScene::ViewMode::Constellation ? tr("Ramificações")
                                                                        : tr("Espiral"));
    }
    if (m_btnAxis)
        m_btnAxis->setText(m_scene->axisMode() == TimelineScene::AxisMode::Narrative
                               ? tr("Narrativa") : tr("História"));
}

// ── Foco / Filtro por linha ──────────────────────────────────────────────────

void TimelinePanel::rebuildFocusMenu()
{
    if (!m_focusMenu) return;
    m_focusMenu->clear();

    QAction* all = m_focusMenu->addAction(tr("Mostrar tudo"));
    connect(all, &QAction::triggered, this, [this]() { setFocusTimeline(QString()); });
    m_focusMenu->addSeparator();

    QList<TimelineDef> sorted = m_timelines;
    std::sort(sorted.begin(), sorted.end(),
              [](const TimelineDef& a, const TimelineDef& b){ return a.railOrder < b.railOrder; });
    for (const auto& t : sorted) {
        const QString name = t.name.isEmpty() ? tr("Linha") : t.name;
        QAction* a = m_focusMenu->addAction(QIcon(colorDot(t.color, 12)), name);
        const QString id = t.id;
        connect(a, &QAction::triggered, this, [this, id]() { setFocusTimeline(id); });
    }
}

void TimelinePanel::setFocusTimeline(const QString& id)
{
    if (!m_scene) return;
    if (id.isEmpty()) m_scene->clearFocus();
    else              m_scene->setFocus(id, m_scene->focusDepth());
    refreshFocusButtons();
}

void TimelinePanel::setFocusDepth(int depth)
{
    if (!m_scene || m_scene->focusTimelineId().isEmpty()) return;
    m_scene->setFocus(m_scene->focusTimelineId(), depth);
    refreshFocusButtons();
}

void TimelinePanel::refreshFocusButtons()
{
    if (!m_scene || !m_btnFocus || !m_btnDepth) return;

    QString fid = m_scene->focusTimelineId();
    // foco apontando para uma linha que sumiu (ex.: trilha auto removida) → limpa
    if (!fid.isEmpty()) {
        bool found = false;
        for (const auto& t : m_timelines) if (t.id == fid) { found = true; break; }
        if (!found) { m_scene->clearFocus(); fid.clear(); }
    }

    if (fid.isEmpty()) {
        m_btnFocus->setText(tr("Foco"));
        m_btnDepth->setEnabled(false);
    } else {
        QString name = tr("Linha");
        for (const auto& t : m_timelines) if (t.id == fid) { name = t.name; break; }
        m_btnFocus->setText(tr("Foco: %1").arg(name));
        m_btnDepth->setEnabled(true);
    }
    const int d = m_scene->focusDepth();
    m_btnDepth->setText(d == 0 ? tr("Só a linha")
                      : d == 1 ? tr("1 salto")
                               : tr("%1 saltos").arg(d));
}

bool TimelinePanel::editTimelineDef(TimelineDef& def, bool isNew)
{
    // Popup: nome + cor + importância
    auto* dlg = new QDialog(this, Qt::Dialog);
    dlg->setWindowTitle(isNew ? tr("Nova linha do tempo") : tr("Editar linha do tempo"));
    dlg->setMinimumWidth(320);

    auto* layout = new QVBoxLayout(dlg);
    layout->setSpacing(10);
    layout->setContentsMargins(16, 16, 16, 16);

    auto* form = new QFormLayout;
    auto* nameEdit = new QLineEdit(dlg);
    nameEdit->setPlaceholderText(tr("Nome da timeline"));
    nameEdit->setText(def.name);
    form->addRow(tr("Nome:"), nameEdit);

    // Importância: controla espessura do trilho e tamanho das bolinhas
    auto* impCombo = new QComboBox(dlg);
    impCombo->addItem(tr("Principal"),             QString::fromLatin1(TimelineWeight::Primary));
    impCombo->addItem(tr("Secundária"),            QString::fromLatin1(TimelineWeight::Secondary));
    impCombo->addItem(tr("Backstory / Flashback"), QString::fromLatin1(TimelineWeight::Backstory));
    const int impIdx = impCombo->findData(def.weight);
    impCombo->setCurrentIndex(impIdx < 0 ? 1 : impIdx); // default: secundária
    form->addRow(tr("Importância:"), impCombo);
    layout->addLayout(form);

    // Seletor de cor
    QColor chosenColor = def.color.isValid() ? def.color : QColor(QStringLiteral("#6c8ebf"));
    auto* colorRow = new QHBoxLayout;
    auto* colorBtn = new QToolButton(dlg);
    colorBtn->setObjectName(QStringLiteral("tlPopupSmallBtn"));
    colorBtn->setFixedHeight(28);
    colorBtn->setCursor(Qt::PointingHandCursor);
    auto updateColorBtn = [&]() {
        colorBtn->setIcon(QIcon(colorDot(chosenColor, 14)));
        colorBtn->setText(QStringLiteral("  ") + chosenColor.name());
        colorBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    };
    updateColorBtn();
    connect(colorBtn, &QToolButton::clicked, dlg, [&, dlg]() {
        const QColor c = QColorDialog::getColor(chosenColor, dlg, tr("Cor da timeline"));
        if (c.isValid()) { chosenColor = c; updateColorBtn(); }
    });
    colorRow->addWidget(new QLabel(tr("Cor:"), dlg));
    colorRow->addWidget(colorBtn);
    colorRow->addStretch();
    layout->addLayout(colorRow);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    btns->button(QDialogButtonBox::Ok)->setText(isNew ? tr("Criar") : tr("Salvar"));
    btns->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(btns, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    layout->addWidget(btns);

    dlg->setStyleSheet(styleSheet());

    const bool ok = (dlg->exec() == QDialog::Accepted);
    if (ok) {
        const QString name = nameEdit->text().trimmed();
        if (!name.isEmpty()) def.name = name;          // edição: nome vazio mantém o atual
        def.color  = chosenColor;
        def.weight = impCombo->currentData().toString();
    }
    dlg->deleteLater();
    return ok && !def.name.isEmpty();
}

void TimelinePanel::createTimeline()
{
    TimelineDef t;
    t.id        = QUuid::createUuid().toString(QUuid::WithoutBraces);
    t.color     = QColor(QStringLiteral("#6c8ebf"));
    t.kind      = QStringLiteral("custom");
    t.weight    = QString::fromLatin1(TimelineWeight::Secondary);
    t.railOrder = m_timelines.size();

    if (!editTimelineDef(t, /*isNew=*/true)) return; // cancelado ou nome vazio

    m_timelines.append(t);
    m_scene->setTimelines(m_timelines);
    m_scene->relayout();
    rebuildFocusMenu();
    refreshFocusButtons();
    save();
}

void TimelinePanel::editTimeline(const QString& id)
{
    for (int i = 0; i < m_timelines.size(); ++i) {
        if (m_timelines[i].id != id) continue;
        TimelineDef t = m_timelines[i];
        if (!editTimelineDef(t, /*isNew=*/false)) return;
        m_timelines[i] = t;
        m_scene->setTimelines(m_timelines); // repropaga cor/peso e redesenha
        m_scene->relayout();
        rebuildFocusMenu();
        refreshFocusButtons();
        save();
        return;
    }
}

void TimelinePanel::createEventAt(const QPointF& scenePos)
{
    TimelineEventPopup dlg(m_timelines, m_projectModel, this);
    dlg.setDocTextResolver(m_docTextResolver);
    if (dlg.exec() != QDialog::Accepted) return;
    commitEvent(dlg.eventData(), scenePos);
}

void TimelinePanel::commitEvent(TimelineEvent e, const QPointF& scenePos)
{
    if (e.title.isEmpty()) e.title = tr("Novo evento");
    e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    e.x  = scenePos.x();
    e.y  = scenePos.y();
    e.narrativeTick = 1e6; // sentinela: relayout joga pro fim da faixa

    // Se não veio timeline do popup, escolhe pela faixa mais próxima do clique
    // (Modo Trilho) ou cai na primeira existente.
    if (e.timelineId.isEmpty() && !m_timelines.isEmpty()) {
        QList<TimelineDef> sorted = m_timelines;
        std::sort(sorted.begin(), sorted.end(),
                  [](const TimelineDef& a, const TimelineDef& b){ return a.railOrder < b.railOrder; });
        const int order = m_scene->nearestRailOrder(scenePos.y());
        e.timelineId = sorted.value(qBound(0, order, sorted.size() - 1)).id;
    }

    m_events.append(e);
    m_scene->addEvent(e);

    // Conexão automática (ordem) com o evento anterior da mesma timeline
    if (!e.timelineId.isEmpty()) {
        const TimelineConn conn = m_scene->autoConnect(e.id, e.timelineId);
        if (!conn.id.isEmpty())
            m_connections.append(conn);
    }

    m_scene->relayout();
    save();
}

void TimelinePanel::promptNewEvent(const QString& description, const QString& marker,
                                   const QString& title)
{
    // título sugerido a partir das primeiras palavras do trecho
    auto suggestTitle = [](const QString& text) -> QString {
        QString flat = text;
        flat.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
        flat = flat.trimmed();
        const QStringList words = flat.split(QChar(' '), Qt::SkipEmptyParts);
        QStringList picked; int total = 0;
        for (const QString& w : words) {
            if (picked.size() >= 7 || total + w.size() > 44) break;
            picked.append(w); total += w.size() + 1;
        }
        return picked.isEmpty() ? flat.left(44) : picked.join(QChar(' '));
    };

    TimelineEventPopup dlg(m_timelines, m_projectModel, this);
    dlg.setDocTextResolver(m_docTextResolver);
    TimelineEvent seed;
    seed.title       = title.trimmed().isEmpty() ? suggestTitle(description)
                                                  : title.trimmed();
    seed.timeMarker  = marker;
    seed.description = description;
    dlg.setEventData(seed);
    if (dlg.exec() != QDialog::Accepted) return;

    const QPointF c = m_view ? m_view->mapToScene(m_view->viewport()->rect().center())
                             : QPointF(0, 0);
    commitEvent(dlg.eventData(), c);
}

void TimelinePanel::openEditPopup(const QString& id)
{
    auto* item = m_scene->findEvent(id);
    if (!item) return;

    const QString oldTimelineId = item->eventData().timelineId;

    TimelineEventPopup dlg(m_timelines, m_projectModel, this);
    dlg.setDocTextResolver(m_docTextResolver);
    dlg.setEventData(item->eventData());
    if (dlg.exec() != QDialog::Accepted) return;

    TimelineEvent updated = dlg.eventData();
    const TimelineEvent prev = item->eventData();
    updated.id            = id;
    updated.x             = prev.x;
    updated.y             = prev.y;
    updated.narrativeTick = prev.narrativeTick;
    updated.storyOrder    = prev.storyOrder;
    updated.autoEvent     = prev.autoEvent;

    // Se mudou de timeline, reconectar
    if (updated.timelineId != oldTimelineId) {
        // Remove conexoes do evento na cena (removeEvent nao e chamado aqui,
        // entao fazemos manualmente com a lista copiada antes de iterar)
        const QList<TimelineConn> snapshot = m_scene->allConnectionData();
        for (const auto& conn : snapshot) {
            if (conn.fromEventId == id || conn.toEventId == id) {
                m_scene->removeConnection(conn.id);
                for (int i = m_connections.size() - 1; i >= 0; --i) {
                    if (m_connections[i].id == conn.id) {
                        m_connections.removeAt(i);
                        break;
                    }
                }
            }
        }
        // Cria nova conexao automatica para a nova timeline
        if (!updated.timelineId.isEmpty()) {
            // Atualiza o dado do item antes de autoConnect para que o algoritmo
            // enxergue a nova timelineId na busca pelo "ultimo evento anterior"
            item->setEventData(updated);
            const TimelineConn newConn = m_scene->autoConnect(id, updated.timelineId);
            if (!newConn.id.isEmpty())
                m_connections.append(newConn);
        }
    }

    // Atualiza no item grafico
    item->setEventData(updated);
    item->setTimelineColor(m_scene->timelineColor(updated.timelineId));

    // Atualiza na lista local
    for (auto& e : m_events) {
        if (e.id == id) { e = updated; break; }
    }
    save();
}

void TimelinePanel::setProjectRoot(const QString& root)
{
    m_projectRoot = root;
    load();
}

void TimelinePanel::setProjectModel(ProjectModel* model)
{
    m_projectModel = model;
}

void TimelinePanel::setDocTextResolver(std::function<QString(const QString&)> resolver)
{
    m_docTextResolver = std::move(resolver);
}

void TimelinePanel::setElementsStore(ElementsStore* store)
{
    if (m_elementsStore == store) return;
    m_elementsStore = store;
    if (m_elementsStore) {
        // Atualização silenciosa quando a presença/elementos mudam (sem perguntar).
        connect(m_elementsStore, &ElementsStore::changed, this, [this]() {
            if (isVisible()) syncCharacterTimelines(false);
        });
    }
}

namespace {
// Cor estável e agradável a partir de um id (trilhas de personagem auto).
QColor colorForId(const QString& id)
{
    uint h = 0;
    for (const QChar c : id) h = h * 131u + c.unicode();
    return QColor::fromHsv(int(h % 360), 150, 210);
}
} // namespace

void TimelinePanel::syncCharacterTimelines(bool askSecondary)
{
    if (!m_elementsStore || !m_projectModel || !m_scene) return;
    if (!m_presenceProvider) return; // sem análise de presença → nada a fazer

    // ── 1. Capítulos em ordem de leitura → mapas por chapterId ────────────────
    struct ChapInfo { int rank; QString title; QString marker; QString docKey; };
    QHash<QString, ChapInfo> chapById;
    {
        const QList<Manuscript>& mss = m_projectModel->manuscripts();
        const QList<Chapter>&    chs = m_projectModel->chapters();
        QHash<QString, int> msIndex;
        for (int i = 0; i < mss.size(); ++i) msIndex.insert(mss[i].id, i);

        QList<Chapter> ordered = chs;
        std::sort(ordered.begin(), ordered.end(), [&](const Chapter& a, const Chapter& b) {
            const int ma = msIndex.value(a.manuscriptId, 9999);
            const int mb = msIndex.value(b.manuscriptId, 9999);
            if (ma != mb) return ma < mb;
            return a.order < b.order;
        });
        int gi = 0;
        for (const Chapter& c : ordered) {
            const QString ms = c.manuscriptId.isEmpty() ? QStringLiteral("root") : c.manuscriptId;
            chapById.insert(c.id, { gi,
                c.title.isEmpty() ? tr("Capítulo %1").arg(gi + 1) : c.title,
                c.timeMarker,
                QStringLiteral("ch:%1:%2").arg(ms, c.id) });
            ++gi;
        }
    }

    // ── 2. Presença por CENA (uma varredura p/ todos os personagens) ──────────
    QHash<QString, Element> charById;
    QStringList names;
    for (const Element& e : m_elementsStore->elements()) {
        if (e.type != QStringLiteral("character")) continue;
        charById.insert(e.id, e);
        names << e.name;
    }
    QHash<QString, CharPresenceResult> presence;
    int totalScenes = 0, totalChapters = 0;
    m_presenceProvider(names, &presence, &totalScenes, &totalChapters);

    // ── 3. Decide quem é elegível (perguntando secundários, se for o caso) ─────
    QSet<QString> eligible;
    for (auto it = charById.begin(); it != charById.end(); ++it) {
        Element e = it.value();
        const CharPresenceResult& r = presence.value(e.name.toLower());

        int d = RoleTiers::autoTrackDecision(e.role, e.trackMode);
        if (d == -1) {
            if (r.sceneCount == 0) continue;  // não aparece → não pergunta
            if (!askSecondary) continue;       // modo silencioso não pergunta

            const auto ans = QMessageBox::question(this, tr("Trilha na linha do tempo"),
                tr("%1 é um personagem secundário e aparece na obra.\n\n"
                   "Quer acompanhá-lo com uma trilha na linha do tempo?")
                    .arg(e.name.isEmpty() ? tr("Este personagem") : e.name),
                QMessageBox::Yes | QMessageBox::No);
            e.trackMode = (ans == QMessageBox::Yes) ? QStringLiteral("on")
                                                    : QStringLiteral("off");
            { QSignalBlocker block(m_elementsStore);
              m_elementsStore->updateElement(e.id, e); }
            it.value() = e;
            d = (ans == QMessageBox::Yes) ? 1 : 0;
        }
        if (d == 1) eligible.insert(e.id);
    }

    // ── 4. Reconcilia trilhas/eventos sobre o estado AO VIVO da cena ───────────
    QList<TimelineEvent> events = m_scene->allEventData();
    QList<TimelineConn>  conns  = m_scene->allConnectionData();
    QList<TimelineDef>   defs   = m_timelines;

    // remove trilhas auto de personagem que deixaram de ser elegíveis
    for (int i = defs.size() - 1; i >= 0; --i) {
        const TimelineDef& t = defs[i];
        if (t.autoGenerated && t.kind == QString::fromLatin1(TimelineKind::Character)
            && !eligible.contains(t.characterId)) {
            const QString tid = t.id;
            events.erase(std::remove_if(events.begin(), events.end(),
                [&](const TimelineEvent& e){ return e.timelineId == tid; }), events.end());
            defs.removeAt(i);
        }
    }

    // garante trilha + eventos (por CENA) para cada personagem elegível
    for (const QString& cid : std::as_const(eligible)) {
        const Element& el = charById[cid];
        const QString tid = QStringLiteral("char:%1").arg(cid);

        // acha/cria a TimelineDef
        int defIdx = -1;
        for (int i = 0; i < defs.size(); ++i) if (defs[i].id == tid) { defIdx = i; break; }
        if (defIdx < 0) {
            TimelineDef t;
            t.id            = tid;
            t.name          = el.name.isEmpty() ? tr("Personagem") : el.name;
            t.color         = colorForId(cid);
            t.kind          = QString::fromLatin1(TimelineKind::Character);
            t.characterId   = cid;
            t.autoGenerated = true;
            t.railOrder     = defs.size();
            defs.append(t);
        } else {
            defs[defIdx].name = el.name.isEmpty() ? tr("Personagem") : el.name; // renome
        }

        // presenças (capítulo + cena) em ordem de leitura
        struct Hit { int rank; int scene; QString chId; bool perScene; QString sceneTitle; };
        QList<Hit> hits;
        const CharPresenceResult& r = presence.value(el.name.toLower());
        for (const PresenceChapterEntry& ce : r.chapters) {
            if (!chapById.contains(ce.id)) continue;
            const int rank = chapById.value(ce.id).rank;
            if (ce.scenes.isEmpty()) {
                hits.append({ rank, 0, ce.id, false, QString() }); // capítulo de cena única
            } else {
                for (const PresenceSceneEntry& se : ce.scenes)
                    hits.append({ rank, se.index, ce.id, true, se.title });
            }
        }
        std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b){
            if (a.rank != b.rank) return a.rank < b.rank;
            return a.scene < b.scene;
        });

        QSet<QString> desiredEvIds;
        for (const Hit& h : hits)
            desiredEvIds.insert(QStringLiteral("auto:%1:%2:%3").arg(cid, h.chId).arg(h.scene));

        // remove eventos auto obsoletos desta trilha
        events.erase(std::remove_if(events.begin(), events.end(), [&](const TimelineEvent& e){
            return e.autoEvent && e.timelineId == tid && !desiredEvIds.contains(e.id);
        }), events.end());

        // adiciona/atualiza — narrativeTick sequencial pela ordem de leitura
        int tick = 0;
        for (const Hit& h : hits) {
            const ChapInfo& info = chapById[h.chId];
            QString label  = info.title;
            QString marker = info.marker;
            if (h.perScene) {
                const QString sc = h.sceneTitle.isEmpty()
                    ? tr("Cena %1").arg(h.scene + 1) : h.sceneTitle;
                label = info.title + QStringLiteral(" · ") + sc;
                if (const Scene* s = m_projectModel->findScene(h.chId, h.scene))
                    if (!s->timeMarker.isEmpty()) marker = s->timeMarker;
            }
            const QString sceneRef = h.perScene
                ? QStringLiteral("%1:%2").arg(h.chId).arg(h.scene) : QString();
            const QString evId = QStringLiteral("auto:%1:%2:%3").arg(cid, h.chId).arg(h.scene);

            TimelineEvent* found = nullptr;
            for (auto& e : events) if (e.id == evId) { found = &e; break; }
            if (found) {
                found->title         = label;
                found->narrativeTick = tick;
                found->timeMarker    = marker; // propaga marcador renomeado
                found->timelineId    = tid;
                found->linkedDocId   = info.docKey;
                found->linkedSceneId = sceneRef;
                // storyOrder preservado (arraste do usuário no eixo História)
            } else {
                TimelineEvent e;
                e.id            = evId;
                e.timelineId    = tid;
                e.title         = label;
                e.narrativeTick = tick;
                e.storyOrder    = tick; // semente; usuário rearranja no eixo História
                e.timeMarker    = marker;
                e.autoEvent     = true;
                e.linkedDocId   = info.docKey;
                e.linkedSceneId = sceneRef;
                events.append(e);
            }
            ++tick;
        }
    }

    // descarta conexões cujos eventos sumiram
    QSet<QString> liveIds;
    for (const auto& e : events) liveIds.insert(e.id);
    conns.erase(std::remove_if(conns.begin(), conns.end(), [&](const TimelineConn& c){
        return !liveIds.contains(c.fromEventId) || !liveIds.contains(c.toEventId);
    }), conns.end());

    // ── 4. Empurra de volta pra cena ───────────────────────────────────────────
    m_timelines = defs;
    m_scene->setTimelines(m_timelines);
    m_scene->clearEvents();
    for (const auto& e : events) m_scene->addEvent(e);
    m_scene->clearConnections();
    for (const auto& c : conns) m_scene->addConnection(c);
    m_scene->relayout();
    rebuildFocusMenu();
    refreshFocusButtons();
    save();
}

void TimelinePanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

void TimelinePanel::closeEvent(QCloseEvent* event)
{
    save();
    emit closeRequested();
    event->accept();
}

void TimelinePanel::save() const
{
    if (m_projectRoot.isEmpty()) return;

    QJsonObject root;
    root[QStringLiteral("zoom")] = m_view ? m_view->zoomFactor() : 1.0;
    root[QStringLiteral("panX")] = m_view ? m_view->scrollPos().x() : 0.0;
    root[QStringLiteral("panY")] = m_view ? m_view->scrollPos().y() : 0.0;
    if (m_scene) {
        const auto vm = m_scene->viewMode();
        root[QStringLiteral("viewMode")] =
            vm == TimelineScene::ViewMode::Rail          ? QStringLiteral("rail")
          : vm == TimelineScene::ViewMode::Constellation ? QStringLiteral("constellation")
                                                         : QStringLiteral("spiral");
        root[QStringLiteral("axisMode")] =
            m_scene->axisMode() == TimelineScene::AxisMode::Narrative
                ? QStringLiteral("narrative") : QStringLiteral("story");
    }

    QJsonArray tls;
    for (const auto& t : m_timelines) {
        QJsonObject o;
        o[QStringLiteral("id")]            = t.id;
        o[QStringLiteral("name")]          = t.name;
        o[QStringLiteral("color")]         = t.color.name();
        o[QStringLiteral("kind")]          = t.kind;
        o[QStringLiteral("weight")]        = t.weight;
        o[QStringLiteral("railOrder")]     = t.railOrder;
        o[QStringLiteral("parentId")]      = t.parentId;
        o[QStringLiteral("branchFrom")]    = t.branchFromEventId;
        o[QStringLiteral("characterId")]   = t.characterId;
        o[QStringLiteral("autoGenerated")] = t.autoGenerated;
        tls.append(o);
    }
    root[QStringLiteral("timelines")] = tls;

    QJsonArray evs;
    // Usa dados ao vivo da cena (têm posição/tick atualizados)
    for (const auto& e : m_scene ? m_scene->allEventData() : m_events) {
        QJsonObject o;
        o[QStringLiteral("id")]            = e.id;
        o[QStringLiteral("timelineId")]    = e.timelineId;
        o[QStringLiteral("title")]         = e.title;
        o[QStringLiteral("description")]   = e.description;
        o[QStringLiteral("color")]         = e.color.isValid() ? e.color.name() : QString();
        o[QStringLiteral("narrativeTick")] = e.narrativeTick;
        o[QStringLiteral("storyOrder")]    = e.storyOrder;
        o[QStringLiteral("x")]             = e.x;
        o[QStringLiteral("y")]             = e.y;
        o[QStringLiteral("timeMarker")]    = e.timeMarker;
        o[QStringLiteral("linkedSceneId")] = e.linkedSceneId;
        o[QStringLiteral("linkedDocId")]   = e.linkedDocId;
        o[QStringLiteral("conclusion")]    = e.conclusion;
        o[QStringLiteral("autoEvent")]     = e.autoEvent;
        evs.append(o);
    }
    root[QStringLiteral("events")] = evs;

    QJsonArray conns;
    const auto connList = m_scene ? m_scene->allConnectionData() : m_connections;
    for (const auto& c : connList) {
        QJsonObject o;
        o[QStringLiteral("id")]          = c.id;
        o[QStringLiteral("fromEventId")] = c.fromEventId;
        o[QStringLiteral("toEventId")]   = c.toEventId;
        o[QStringLiteral("type")]        = c.type;
        conns.append(o);
    }
    root[QStringLiteral("connections")] = conns;

    QSaveFile f(m_projectRoot + QStringLiteral("/timeline.json"));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson());
        f.commit();
    }
}

void TimelinePanel::load()
{
    if (m_projectRoot.isEmpty()) return;

    QFile f(m_projectRoot + QStringLiteral("/timeline.json"));
    if (!f.open(QIODevice::ReadOnly)) return;

    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();

    const qreal zoom = root[QStringLiteral("zoom")].toDouble(1.0);
    const qreal panX = root[QStringLiteral("panX")].toDouble(0.0);
    const qreal panY = root[QStringLiteral("panY")].toDouble(0.0);
    if (m_view) m_view->applyZoomAndPan(zoom, panX, panY);

    if (m_scene) {
        const QString vm = root[QStringLiteral("viewMode")].toString(QStringLiteral("rail"));
        m_scene->setViewMode(vm == QStringLiteral("constellation") ? TimelineScene::ViewMode::Constellation
                           : vm == QStringLiteral("spiral")        ? TimelineScene::ViewMode::Spiral
                                                                   : TimelineScene::ViewMode::Rail);
        m_scene->setAxisMode(root[QStringLiteral("axisMode")].toString(QStringLiteral("narrative"))
                                 == QStringLiteral("story")
                                 ? TimelineScene::AxisMode::Story
                                 : TimelineScene::AxisMode::Narrative);
    }

    m_timelines.clear();
    int idx = 0;
    for (const auto& v : root[QStringLiteral("timelines")].toArray()) {
        const QJsonObject o = v.toObject();
        TimelineDef t;
        t.id            = o[QStringLiteral("id")].toString();
        t.name          = o[QStringLiteral("name")].toString();
        t.color         = QColor(o[QStringLiteral("color")].toString());
        t.kind          = o[QStringLiteral("kind")].toString(QStringLiteral("custom"));
        t.weight        = o[QStringLiteral("weight")].toString(QStringLiteral("secondary")); // migração: secundária
        t.railOrder     = o[QStringLiteral("railOrder")].toInt(idx); // migração: ordem = índice
        t.parentId          = o[QStringLiteral("parentId")].toString();
        t.branchFromEventId = o[QStringLiteral("branchFrom")].toString();
        t.characterId       = o[QStringLiteral("characterId")].toString();
        t.autoGenerated     = o[QStringLiteral("autoGenerated")].toBool(false);
        m_timelines.append(t);
        ++idx;
    }
    if (m_scene) m_scene->setTimelines(m_timelines);

    m_events.clear();
    if (m_scene) m_scene->clearEvents();
    // contador por timeline p/ derivar narrativeTick em projetos legados
    QHash<QString, int> tickSeq;
    for (const auto& v : root[QStringLiteral("events")].toArray()) {
        const QJsonObject o = v.toObject();
        TimelineEvent e;
        e.id           = o[QStringLiteral("id")].toString();
        e.timelineId   = o[QStringLiteral("timelineId")].toString();
        e.title        = o[QStringLiteral("title")].toString();
        e.description  = o[QStringLiteral("description")].toString();
        const QString col = o[QStringLiteral("color")].toString();
        e.color        = col.isEmpty() ? QColor() : QColor(col);
        // migração: sem narrativeTick → ordem de inserção dentro da timeline
        e.narrativeTick = o[QStringLiteral("narrativeTick")].toDouble(
                              double(tickSeq.value(e.timelineId, 0)));
        e.storyOrder   = o[QStringLiteral("storyOrder")].toDouble(-1.0);
        e.x            = o[QStringLiteral("x")].toDouble();
        e.y            = o[QStringLiteral("y")].toDouble();
        e.timeMarker   = o[QStringLiteral("timeMarker")].toString();
        e.linkedSceneId = o[QStringLiteral("linkedSceneId")].toString();
        e.linkedDocId  = o[QStringLiteral("linkedDocId")].toString();
        e.conclusion   = o[QStringLiteral("conclusion")].toString();
        e.autoEvent    = o[QStringLiteral("autoEvent")].toBool(false);
        tickSeq[e.timelineId] = tickSeq.value(e.timelineId, 0) + 1;
        m_events.append(e);
        if (m_scene) m_scene->addEvent(e);
    }

    m_connections.clear();
    if (m_scene) m_scene->clearConnections();
    for (const auto& v : root[QStringLiteral("connections")].toArray()) {
        const QJsonObject o = v.toObject();
        TimelineConn c;
        c.id          = o[QStringLiteral("id")].toString();
        c.fromEventId = o[QStringLiteral("fromEventId")].toString();
        c.toEventId   = o[QStringLiteral("toEventId")].toString();
        c.type        = o[QStringLiteral("type")].toString(QStringLiteral("sequence"));
        m_connections.append(c);
        if (m_scene) m_scene->addConnection(c);
    }

    if (m_scene) m_scene->relayout();
    refreshModeButtons();
    rebuildFocusMenu();
    refreshFocusButtons();

    // Gera as trilhas automáticas de personagem (e pergunta sobre secundários).
    syncCharacterTimelines(true);
}

void TimelinePanel::onExportEventAsDoc(const TimelineEvent& event)
{
    if (!m_projectModel) return;

    const QList<Drawer>& drawers = m_projectModel->drawers();
    if (drawers.isEmpty()) {
        QMessageBox::information(this, tr("Exportar como documento"),
            tr("Crie uma gaveta antes de usar este recurso."));
        return;
    }

    // ── Diálogo de destino ────────────────────────────────────────────────────
    auto* dlg = new QDialog(this, Qt::Dialog);
    dlg->setWindowTitle(tr("Exportar como documento"));
    dlg->setMinimumWidth(360);

    auto* root = new QVBoxLayout(dlg);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto* nameLab = new QLabel(tr("Nome do documento:"), dlg);
    root->addWidget(nameLab);
    auto* nameEdit = new QLineEdit(event.title, dlg);
    nameEdit->selectAll();
    root->addWidget(nameEdit);

    auto* drawerLab = new QLabel(tr("Gaveta de destino:"), dlg);
    root->addWidget(drawerLab);
    auto* drawerCombo = new QComboBox(dlg);
    for (const auto& d : drawers) {
        const QString label = d.title.isEmpty() ? tr("(sem nome)") : d.title;
        drawerCombo->addItem(label, d.key);
    }
    root->addWidget(drawerCombo);
    root->addStretch();

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto* cancel = new QPushButton(tr("Cancelar"), dlg);
    auto* ok     = new QPushButton(tr("Criar"), dlg);
    ok->setDefault(true);
    btnRow->addWidget(cancel);
    btnRow->addWidget(ok);
    root->addLayout(btnRow);

    connect(cancel, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(ok,     &QPushButton::clicked, dlg, &QDialog::accept);
    dlg->setStyleSheet(styleSheet());

    if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return; }

    const QString title   = nameEdit->text().trimmed();
    const QString destKey = drawerCombo->currentData().toString();
    dlg->deleteLater();

    if (title.isEmpty() || destKey.isEmpty()) return;

    // ── Converte descrição em HTML básico ────────────────────────────────────
    auto buildHtml = [](const QString& text) -> QString {
        if (text.trimmed().isEmpty()) return QStringLiteral("<p></p>");
        const QStringList paras = text.split(
            QRegularExpression(QStringLiteral("\\n{2,}")), Qt::SkipEmptyParts);
        QString out;
        for (const QString& para : paras) {
            QString chunk = para.trimmed().toHtmlEscaped();
            chunk.replace(QChar('\n'), QStringLiteral("<br/>"));
            if (!chunk.isEmpty())
                out += QStringLiteral("<p>%1</p>").arg(chunk);
        }
        return out.isEmpty() ? QStringLiteral("<p></p>") : out;
    };

    DrawerItem it;
    it.id           = ProjectModel::uid();
    it.title        = title;
    it.hasInlineHtml = true;
    it.html         = buildHtml(event.description);
    m_projectModel->addDrawerItem(destKey, it);
}

void TimelinePanel::applyTheme()
{
    if (m_scene)
        m_scene->setBackgroundColor(QColor(Theme::appBackground()));

    const QString qss = QStringLiteral(R"(
        QWidget#timelinePanel { background: %1; }
        QWidget#tlToolbar {
            background: %2;
            border-bottom: 1px solid %3;
        }
        QLabel#tlTitle {
            color: %4;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 14px;
            font-weight: 600;
            padding: 0 4px;
        }
        QToolButton#tlToolBtn {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 6px;
            color: %5;
            font-size: 12px;
            padding: 2px 8px;
        }
        QToolButton#tlToolBtn:hover {
            background: %6;
            border-color: %3;
            color: %4;
        }
        QFrame#tlSep {
            background: %3;
            border: none;
        }
        QToolButton#tlAddOverlay {
            background: %7;
            color: %4;
            border: 1px solid %3;
            border-radius: 18px;
            font-size: 20px;
            font-weight: 600;
            padding-bottom: 2px;
        }
        QToolButton#tlAddOverlay:hover {
            background: %8;
            border-color: %8;
            color: #ffffff;
        }
    )").arg(Theme::appBackground(),
            Theme::panelBackground(),
            Theme::subtleBorder(),
            Theme::textPrimary(),
            Theme::textMuted(),
            Theme::hoverOverlay(),
            Theme::panelBackground(),
            Theme::accentDefault());

    setStyleSheet(qss);
}
