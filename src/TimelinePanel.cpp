#include "TimelinePanel.h"

#include "CrashLogger.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "ProjectModel.h"
#include "RoleTiers.h"
#include "Theme.h"
#include "TimelineBranchPopup.h"
#include "TimelineChrono.h"

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
#include <QCryptographicHash>
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
#include <QRandomGenerator>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QPixmap>
#include <QResizeEvent>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QVector>
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

    tl->addWidget(makeSep(m_toolbar));

    // ── Filtro por personagem (quem está presente na cena/capítulo) ────────────
    m_btnCharFilter = makeBtn(tr("Personagem: Todos"), tr("Filtrar eventos por personagem presente"), m_toolbar);
    m_btnCharFilter->setPopupMode(QToolButton::InstantPopup);
    m_charFilterMenu = new QMenu(m_btnCharFilter);
    m_btnCharFilter->setMenu(m_charFilterMenu);
    connect(m_charFilterMenu, &QMenu::aboutToShow, this, &TimelinePanel::rebuildCharFilterMenu);
    tl->addWidget(m_btnCharFilter);

    // ── Trilhas de personagem (legado) — desligado por padrão ───────────────────
    m_btnLegacyChars = makeBtn(tr("Personagens (legado)"), tr(
        "Trilhas dedicadas por personagem (substituídas pela presença mostrada "
        "no evento + filtro acima). Religa a geração automática dessas trilhas."),
        m_toolbar);
    m_btnLegacyChars->setCheckable(true);
    tl->addWidget(m_btnLegacyChars);

    tl->addStretch();

    auto* btnClose = makeBtn(QStringLiteral("×"), tr("Fechar"), m_toolbar);
    tl->addWidget(btnClose);

    connect(btnNewTimeline, &QToolButton::clicked, this, &TimelinePanel::createTimeline);
    connect(m_btnView, &QToolButton::clicked, this, &TimelinePanel::toggleViewMode);
    connect(m_btnAxis, &QToolButton::clicked, this, &TimelinePanel::toggleAxisMode);
    connect(m_btnLegacyChars, &QToolButton::toggled, this, &TimelinePanel::toggleLegacyCharacterTracks);
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
    connect(m_scene, &TimelineScene::deleteTimelineRequested, this, &TimelinePanel::deleteTimeline);
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

// ── Trilhas de personagem (legado) ───────────────────────────────────────────

void TimelinePanel::toggleLegacyCharacterTracks(bool checked)
{
    CrashLogger::log(QStringLiteral("tlToggleLegacyChars checked=%1").arg(checked));
    if (!m_projectModel) return;
    m_projectModel->setLegacyCharacterTracksEnabled(checked);
    syncCharacterTimelines(checked); // liga: pergunta secundários; desliga: poda tudo
}

// ── Filtro por personagem ────────────────────────────────────────────────────

void TimelinePanel::rebuildCharFilterMenu()
{
    CrashLogger::log("tlRebuildCharFilterMenu");
    if (!m_charFilterMenu || !m_elementsStore) return;
    m_charFilterMenu->clear();

    QAction* all = m_charFilterMenu->addAction(tr("Todos"));
    connect(all, &QAction::triggered, this, [this]() { setCharFilter(QString()); });
    m_charFilterMenu->addSeparator();

    QList<Element> chars;
    for (const Element& e : m_elementsStore->elements())
        if (e.type == QStringLiteral("character")) chars.append(e);
    std::sort(chars.begin(), chars.end(), [](const Element& a, const Element& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    for (const Element& e : chars) {
        const QString name = e.name.isEmpty() ? tr("(sem nome)") : e.name;
        QAction* a = m_charFilterMenu->addAction(name);
        a->setCheckable(true);
        a->setChecked(e.id == m_charFilterId);
        const QString id = e.id;
        connect(a, &QAction::triggered, this, [this, id]() { setCharFilter(id); });
    }
}

void TimelinePanel::setCharFilter(const QString& characterId)
{
    CrashLogger::log(QStringLiteral("tlSetCharFilter id=%1").arg(characterId));
    m_charFilterId = characterId;

    QString charName;
    if (!characterId.isEmpty() && m_elementsStore) {
        for (const Element& e : m_elementsStore->elements())
            if (e.id == characterId) { charName = e.name; break; }
    }
    if (m_btnCharFilter) {
        m_btnCharFilter->setText(charName.isEmpty() ? tr("Personagem: Todos")
                                                     : tr("Personagem: %1").arg(charName));
    }

    if (!m_scene) return;
    if (charName.isEmpty() || !m_presenceProvider) {
        m_scene->setCharacterFilter(false, {});
        return;
    }

    QHash<QString, CharPresenceResult> presence;
    int totalScenes = 0, totalChapters = 0;
    m_presenceProvider({ charName }, &presence, &totalScenes, &totalChapters);
    const CharPresenceResult& r = presence.value(charName.toLower());

    // "chapterId:sceneIndex" — mesma chave usada no id dos eventos auto de
    // capítulo/cena ("story:<chapterId>:<sceneIndex>"), sentinela 0 p/ capítulo
    // de cena única (ver mesma convenção em syncCharacterTimelines).
    QSet<QString> keys;
    for (const PresenceChapterEntry& ce : r.chapters) {
        if (ce.scenes.isEmpty()) {
            keys.insert(QStringLiteral("%1:0").arg(ce.id));
        } else {
            for (const PresenceSceneEntry& se : ce.scenes)
                keys.insert(QStringLiteral("%1:%2").arg(ce.id).arg(se.index));
        }
    }

    QSet<QString> matchingIds;
    static const QString kPrefix = QStringLiteral("story:");
    for (const auto& e : m_scene->allEventData()) {
        if (!e.id.startsWith(kPrefix)) continue;
        if (keys.contains(e.id.mid(kPrefix.size()))) matchingIds.insert(e.id);
    }
    m_scene->setCharacterFilter(true, matchingIds);
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

void TimelinePanel::deleteTimeline(const QString& id)
{
    for (int i = 0; i < m_timelines.size(); ++i) {
        if (m_timelines[i].id != id) continue;
        const TimelineDef def = m_timelines[i];

        // Linhas automáticas (Narrativa/Flashback/personagem/ramificação) são
        // regeneradas sozinhas no próximo resync — excluir aqui só ia fazê-las
        // reaparecer, então essas só somem de verdade mudando o que as gera
        // (ex.: config de trilhas de personagem, ou o padrão narrativo detectado).
        if (def.autoGenerated) {
            QMessageBox::information(this, tr("Linha automática"),
                tr("\"%1\" é gerada automaticamente a partir do manuscrito e "
                   "reaparece sozinha enquanto houver conteúdo correspondente — "
                   "não dá pra excluir direto.")
                    .arg(def.name.isEmpty() ? id : def.name));
            return;
        }

        const int eventCount = int(std::count_if(m_events.begin(), m_events.end(),
            [&](const TimelineEvent& e) { return e.timelineId == id; }));
        const QString question = eventCount > 0
            ? tr("Excluir \"%1\"? Isso também apaga %n evento(s) marcado(s) nessa linha.",
                 "", eventCount).arg(def.name)
            : tr("Excluir \"%1\"?").arg(def.name);
        if (QMessageBox::question(this, tr("Excluir linha do tempo"), question,
                                   QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) return;

        QSet<QString> removedEventIds;
        for (int j = m_events.size() - 1; j >= 0; --j) {
            if (m_events[j].timelineId != id) continue;
            removedEventIds.insert(m_events[j].id);
            m_events.removeAt(j);
        }
        m_connections.erase(std::remove_if(m_connections.begin(), m_connections.end(),
            [&](const TimelineConn& c) {
                return removedEventIds.contains(c.fromEventId) || removedEventIds.contains(c.toEventId);
            }), m_connections.end());
        m_timelines.removeAt(i);

        m_scene->setTimelines(m_timelines);
        m_scene->clearEvents();
        for (const auto& e : m_events) m_scene->addEvent(e);
        m_scene->clearConnections();
        for (const auto& c : m_connections) m_scene->addConnection(c);
        m_scene->relayout();
        rebuildFocusMenu();
        refreshFocusButtons();
        save();
        return;
    }
}

void TimelinePanel::createEventAt(const QPointF& scenePos)
{
    CrashLogger::log("tlCreateEventAt");
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
    CrashLogger::log(QStringLiteral("tlOpenEditPopup id=%1").arg(id));
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
    if (m_projectModel == model) return;
    m_projectModel = model;
    if (m_projectModel) {
        // Estrutura de capítulos/manuscritos mudou (criar/editar/apagar
        // capítulo, marcador, resumo, data-base...) → resync silencioso das
        // trilhas automáticas, sem esperar reabertura do projeto.
        auto resync = [this]() {
            if (!isVisible()) return;
            syncCharacterTimelines(false);
            syncStoryTimeline();
        };
        connect(m_projectModel, &ProjectModel::chaptersChanged, this, resync);
        connect(m_projectModel, &ProjectModel::manuscriptsChanged, this, resync);
    }
}

void TimelinePanel::refreshFromModel()
{
    if (!m_projectModel) return;
    syncCharacterTimelines(false);
    syncStoryTimeline();
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

// Cor aleatória e vívida pras ramificações automáticas — cada trilho novo
// (branch:auto:*) recebe uma cor sorteada, evitando só a cor de fundo do
// painel (a única referência estável, já que o resto muda por tema).
QColor randomBranchColor()
{
    const QColor panelBg(Theme::panelBackground());
    for (int attempt = 0; attempt < 16; ++attempt) {
        const int hue = QRandomGenerator::global()->bounded(360);
        const int sat = 150 + QRandomGenerator::global()->bounded(80);
        const int val = 180 + QRandomGenerator::global()->bounded(60);
        const QColor c = QColor::fromHsv(hue, sat, val);
        const int dr = c.red()   - panelBg.red();
        const int dg = c.green() - panelBg.green();
        const int db = c.blue()  - panelBg.blue();
        if (dr * dr + dg * dg + db * db > 6000) return c; // ~77 de distância euclidiana
    }
    return QColor::fromHsv(200, 150, 210); // fallback improvável
}
} // namespace

void TimelinePanel::syncCharacterTimelines(bool askSecondary)
{
    if (!m_elementsStore || !m_projectModel || !m_scene) return;
    if (!m_presenceProvider) return; // sem análise de presença → nada a fazer

    // Trilhas de personagem são legado, desligadas por padrão (substituídas
    // pela presença mostrada direto no evento de capítulo/cena + filtro).
    // Quando desligado, `eligible` fica vazio e a etapa 4 abaixo poda sozinha
    // qualquer trilha/evento remanescente de quando esteve ligado.
    const bool legacyEnabled = m_projectModel->legacyCharacterTracksEnabled();
    CrashLogger::log(QStringLiteral("tlSyncCharacters legacy=%1 ask=%2")
                          .arg(legacyEnabled).arg(askSecondary));

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
    QHash<QString, CharPresenceResult> presence;
    QSet<QString> eligible;
    if (legacyEnabled) {
        QStringList names;
        for (const Element& e : m_elementsStore->elements()) {
            if (e.type != QStringLiteral("character")) continue;
            charById.insert(e.id, e);
            names << e.name;
        }
        int totalScenes = 0, totalChapters = 0;
        m_presenceProvider(names, &presence, &totalScenes, &totalChapters);

        // ── 3. Decide quem é elegível (perguntando secundários, se for o caso) ──
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

    // ── Copresença automática: personagens elegíveis que dividem cena ──────────
    // Chave "chId:scene" casa exatamente com o sufixo do evId auto-gerado
    // (auto:<cid>:<chId>:<scene>), inclusive o sentinela scene=0 de capítulos
    // de cena única.
    {
        QHash<QString, QSet<QString>> keysByChar; // characterId -> {"chId:scene"}
        for (const QString& cid : std::as_const(eligible)) {
            const Element& el = charById[cid];
            const CharPresenceResult& r = presence.value(el.name.toLower());
            QSet<QString> keys;
            for (const PresenceChapterEntry& ce : r.chapters) {
                if (!chapById.contains(ce.id)) continue;
                if (ce.scenes.isEmpty()) {
                    keys.insert(QStringLiteral("%1:%2").arg(ce.id).arg(0));
                } else {
                    for (const PresenceSceneEntry& se : ce.scenes)
                        keys.insert(QStringLiteral("%1:%2").arg(ce.id).arg(se.index));
                }
            }
            keysByChar.insert(cid, keys);
        }

        // reconciliação completa: descarta e recria todas as conexões auto de copresença
        conns.erase(std::remove_if(conns.begin(), conns.end(), [](const TimelineConn& c) {
            return c.type == QString::fromLatin1(TimelineConnType::Copresence);
        }), conns.end());

        const QList<QString> ids = keysByChar.keys();
        for (int i = 0; i < ids.size(); ++i) {
            for (int j = i + 1; j < ids.size(); ++j) {
                const QString a = ids[i] < ids[j] ? ids[i] : ids[j];
                const QString b = ids[i] < ids[j] ? ids[j] : ids[i];
                const QSet<QString> shared = keysByChar.value(a) & keysByChar.value(b);
                for (const QString& key : shared) {
                    TimelineConn c;
                    c.id          = QStringLiteral("cop:%1:%2:%3").arg(a, b, key);
                    c.fromEventId = QStringLiteral("auto:%1:%2").arg(a, key);
                    c.toEventId   = QStringLiteral("auto:%1:%2").arg(b, key);
                    c.type        = QString::fromLatin1(TimelineConnType::Copresence);
                    conns.append(c);
                }
            }
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

void TimelinePanel::syncStoryTimeline()
{
    CrashLogger::log("tlSyncStory");
    if (!m_projectModel || !m_scene) return;

    // ── Capítulos/cenas COM marcador, em ordem de leitura global ────────────────
    // Cena com marcador próprio usa o dela; sem marcador, herda o do capítulo
    // (mesmo fallback já usado nas trilhas de personagem, TimelinePanel.cpp).
    QList<AutoHit> hits;
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
        int gi = 0, tick = 0;
        for (const Chapter& c : ordered) {
            const QString ms = c.manuscriptId.isEmpty() ? QStringLiteral("root") : c.manuscriptId;
            const QString docKey = QStringLiteral("ch:%1:%2").arg(ms, c.id);
            const QString chapTitle = c.title.isEmpty() ? tr("Capítulo %1").arg(gi + 1) : c.title;

            if (c.scenes.isEmpty()) {
                if (!c.timeMarker.trimmed().isEmpty()) {
                    hits.append({ tick, QStringLiteral("story:%1:0").arg(c.id), chapTitle,
                        c.timeMarker, c.summary, docKey, QString(), c.manuscriptId, c.povOther });
                    ++tick;
                }
            } else {
                for (int si = 0; si < c.scenes.size(); ++si) {
                    const Scene& s = c.scenes[si];
                    const QString marker = s.timeMarker.isEmpty() ? c.timeMarker : s.timeMarker;
                    if (marker.trimmed().isEmpty()) continue;
                    const QString summary = s.summary.isEmpty() ? c.summary : s.summary;
                    const QString sceneLabel = s.title.isEmpty() ? tr("Cena %1").arg(si + 1) : s.title;
                    hits.append({ tick, QStringLiteral("story:%1:%2").arg(c.id).arg(si),
                        chapTitle + QStringLiteral(" · ") + sceneLabel, marker, summary, docKey,
                        QStringLiteral("%1:%2").arg(c.id).arg(si), c.manuscriptId, s.povOther });
                    ++tick;
                }
            }
            ++gi;
        }
    }
    CrashLogger::log(QStringLiteral("tlSyncStory hits=%1").arg(hits.size()));

    // data-base cronológica de cada manuscrito (pra decidir Flashback)
    struct Base { qreal chrono = 0.0; bool ok = false; };
    QHash<QString, Base> baseByMs;
    for (const Manuscript& m : m_projectModel->manuscripts()) {
        Base b;
        b.chrono = TimelineChrono::parse(m.storyStartMarker, &b.ok);
        baseByMs.insert(m.id, b);
    }

    // Personagens presentes por evento (exibido no card do evento, "Presentes: ...").
    QHash<QString, QStringList> presentByEvId; // evId -> nomes
    if (!hits.isEmpty() && m_elementsStore && m_presenceProvider) {
        QStringList names;
        for (const Element& e : m_elementsStore->elements())
            if (e.type == QStringLiteral("character")) names << e.name;
        if (!names.isEmpty()) {
            CrashLogger::log(QStringLiteral("tlSyncStory presenceScan chars=%1").arg(names.size()));
            QHash<QString, CharPresenceResult> presence;
            int totalScenes = 0, totalChapters = 0;
            m_presenceProvider(names, &presence, &totalScenes, &totalChapters);
            CrashLogger::log("tlSyncStory presenceScanDone");
            for (const QString& name : std::as_const(names)) {
                const CharPresenceResult& r = presence.value(name.toLower());
                for (const PresenceChapterEntry& ce : r.chapters) {
                    if (ce.scenes.isEmpty()) {
                        presentByEvId[QStringLiteral("story:%1:0").arg(ce.id)] << name;
                    } else {
                        for (const PresenceSceneEntry& se : ce.scenes)
                            presentByEvId[QStringLiteral("story:%1:%2").arg(ce.id).arg(se.index)] << name;
                    }
                }
            }
        }
    }

    QList<TimelineEvent> events = m_scene->allEventData();
    QList<TimelineConn>  conns  = m_scene->allConnectionData();
    QList<TimelineDef>   defs   = m_timelines;

    static const QString kMainId  = QStringLiteral("story:main");
    static const QString kFlashId = QStringLiteral("story:flashback");

    auto ensureDef = [&](const QString& id, const QString& kind,
                          const QString& name, const QString& weight) {
        // Corrige nome/kind se ficaram dessincronizados (ex.: dado salvo de uma
        // versão anterior). Preserva color/weight/railOrder — esses sim o
        // usuário pode ter customizado na UI.
        for (auto& d : defs) {
            if (d.id != id) continue;
            d.name = name;
            d.kind = kind;
            return;
        }
        TimelineDef t;
        t.id            = id;
        t.name          = name;
        t.kind          = kind;
        t.weight        = weight;
        t.autoGenerated = true;
        t.railOrder     = defs.size();
        defs.append(t);
    };

    QSet<QString> desiredEvIds;
    if (!hits.isEmpty()) {
        ensureDef(kMainId, QString::fromLatin1(TimelineKind::Main), tr("Narrativa"),
                  QStringLiteral("secondary"));
        ensureDef(kFlashId, QString::fromLatin1(TimelineKind::Backstory), tr("Flashback"),
                  QString::fromLatin1(TimelineWeight::Backstory));
    }

    QList<AutoHit> mainHits; // hits não-Flashback — insumo do detector de ramificações
    for (const AutoHit& h : hits) {
        bool okChap = false;
        const qreal chapChrono = TimelineChrono::parse(h.marker, &okChap);
        const Base base = baseByMs.value(h.manuscriptId);
        const bool isFlashback = base.ok && okChap && chapChrono < base.chrono;
        const QString tid = isFlashback ? kFlashId : kMainId;
        desiredEvIds.insert(h.evId);
        if (!isFlashback) mainHits.append(h);

        TimelineEvent* found = nullptr;
        for (auto& e : events) if (e.id == h.evId) { found = &e; break; }
        if (found) {
            found->title         = h.title;
            found->description   = h.summary;
            found->narrativeTick = h.rank;
            found->timeMarker    = h.marker;
            found->timelineId    = tid;
            found->linkedDocId   = h.docKey;
            found->linkedSceneId = h.linkedSceneId;
            // storyOrder preservado (arraste manual do usuário no eixo História)
        } else {
            TimelineEvent e;
            e.id            = h.evId;
            e.timelineId    = tid;
            e.title         = h.title;
            e.description   = h.summary;
            e.narrativeTick = h.rank;
            e.storyOrder    = h.rank; // semente; usuário rearranja no eixo História
            e.timeMarker    = h.marker;
            // autoEvent fica false (default) — não é usado pra decidir eixo (ver
            // TimelineScene::timelineVisibleInAxis, que decide pelo kind da
            // trilha: main cai no eixo Narrativa, backstory no eixo História).
            // A reconciliação abaixo usa o prefixo "story:" do id, não esse flag.
            e.linkedDocId   = h.docKey;
            e.linkedSceneId = h.linkedSceneId;
            events.append(e);
        }
    }

    // Ramificações automáticas — só dentro da Narrativa (mainHits nunca inclui
    // Flashback). Muta events/conns/defs in-place antes do push final.
    syncAutoBranches(mainHits, presentByEvId, events, conns, defs);

    // remove eventos auto obsoletos das duas trilhas (capítulo apagado / marcador
    // esvaziado). Identidade "é meu" = prefixo "story:" do id (não dá pra usar
    // autoEvent aqui, ver comentário acima) — evento manual do usuário nessas
    // trilhas sempre tem um QUuid aleatório, nunca colide com esse prefixo.
    events.erase(std::remove_if(events.begin(), events.end(), [&](const TimelineEvent& e) {
        return e.id.startsWith(QStringLiteral("story:"))
            && (e.timelineId == kMainId || e.timelineId == kFlashId)
            && !desiredEvIds.contains(e.id);
    }), events.end());

    // remove as trilhas auto se ficaram sem nenhum evento
    for (int i = defs.size() - 1; i >= 0; --i) {
        const TimelineDef& t = defs[i];
        if (!t.autoGenerated || (t.id != kMainId && t.id != kFlashId)) continue;
        const bool hasEvents = std::any_of(events.begin(), events.end(),
            [&](const TimelineEvent& e){ return e.timelineId == t.id; });
        if (!hasEvents) defs.removeAt(i);
    }

    // descarta conexões cujos eventos sumiram
    QSet<QString> liveIds;
    for (const auto& e : events) liveIds.insert(e.id);
    conns.erase(std::remove_if(conns.begin(), conns.end(), [&](const TimelineConn& c){
        return !liveIds.contains(c.fromEventId) || !liveIds.contains(c.toEventId);
    }), conns.end());

    CrashLogger::log(QStringLiteral("tlSyncStory pushToScene events=%1").arg(events.size()));
    m_timelines = defs;
    m_scene->setTimelines(m_timelines);
    m_scene->clearEvents();
    for (const auto& e : events) {
        auto* item = m_scene->addEvent(e);
        if (item) item->setPresentCharacters(presentByEvId.value(e.id));
    }
    m_scene->clearConnections();
    for (const auto& c : conns) m_scene->addConnection(c);
    m_scene->relayout();
    rebuildFocusMenu();
    refreshFocusButtons();
    save();
    CrashLogger::log("tlSyncStory done");
}

// ── Ramificações automáticas ────────────────────────────────────────────────
// Ver design em memória/plano "timeline-auto-branching-design". v1: pesos de
// desempate (kTieEpsilon, penalidade de obsolescência, janela de agrupamento
// de anomalias) são heurísticas razoáveis, não uma fórmula calibrada — vão
// precisar de ajuste depois de testar com manuscritos reais.
void TimelinePanel::syncAutoBranches(const QList<AutoHit>& mainHits,
                                     const QHash<QString, QStringList>& presentByEvId,
                                     QList<TimelineEvent>& events,
                                     QList<TimelineConn>& conns,
                                     QList<TimelineDef>& defs)
{
    static const QString kMainId = QStringLiteral("story:main");
    if (mainHits.isEmpty()) return;

    const QHash<QString, QString> assignments = loadBranchAssignments();

    struct Branch {
        QString id;
        QString parentId;
        qreal   lastChrono = 0.0;
        bool    lastChronoOk = false;
        QSet<QString> lastCast;
        int     lastActiveTick = 0;
        QString lastEvId;
    };
    QVector<Branch> branches;
    { Branch main; main.id = kMainId; branches.append(main); }
    auto findBranch = [&](const QString& id) -> Branch* {
        for (auto& b : branches) if (b.id == id) return &b;
        return nullptr;
    };
    Branch* current = &branches[0];

    // Candidato pendente pra regra "2+ anomalias agrupando" (ponto 1 do
    // design) — uma anomalia isolada nunca vira ramificação nova sozinha.
    struct PendingCandidate {
        bool active = false;
        QString firstEvId;
        qreal chrono = 0.0; bool chronoOk = false;
        QSet<QString> cast;
        QString originId;
    };
    PendingCandidate pending;

    auto castFor = [&](const AutoHit& h) -> QSet<QString> {
        const QStringList names = presentByEvId.value(h.evId);
        return QSet<QString>(names.cbegin(), names.cend());
    };
    auto assignEvent = [&](const QString& evId, const QString& branchId) {
        for (auto& e : events) if (e.id == evId) { e.timelineId = branchId; return; }
    };
    auto touchBranch = [&](Branch* b, qreal chrono, bool chronoOk,
                           const QSet<QString>& cast, const AutoHit& h) {
        b->lastChrono = chrono; b->lastChronoOk = chronoOk;
        b->lastCast = cast; b->lastActiveTick = h.rank; b->lastEvId = h.evId;
        assignEvent(h.evId, b->id);
    };
    auto addConvergence = [&](const QString& fromEvId, const QString& toEvId) {
        if (fromEvId.isEmpty() || fromEvId == toEvId) return;
        for (const auto& c : conns) {
            if ((c.fromEventId == fromEvId && c.toEventId == toEvId)
             || (c.fromEventId == toEvId && c.toEventId == fromEvId)) return;
        }
        TimelineConn tc;
        tc.id = QStringLiteral("branch-conn:%1:%2").arg(fromEvId, toEvId);
        tc.fromEventId = fromEvId;
        tc.toEventId   = toEvId;
        tc.type = QString::fromLatin1(TimelineConnType::Branch);
        conns.append(tc);
    };
    // Distância heurística: cronologia é o fator primário (ponto 3 do
    // design); obsolescência penaliza ramificações silenciosas há muitos
    // capítulos (ponto 9); elenco só desempata quando a cronologia empata
    // (ponto 4) — aqui entra como parte da MESMA conta via kTieEpsilon.
    auto distance = [&](qreal chrono, bool chronoOk, const Branch& b, int tick) -> qreal {
        qreal d = 1e9;
        if (chronoOk && b.lastChronoOk) d = qAbs(chrono - b.lastChrono);
        const qreal obsolescence = qMax(0, tick - b.lastActiveTick) * 0.05;
        return d + obsolescence;
    };
    constexpr qreal kTieEpsilon = 0.35; // "dias" no escalar do TimelineChrono

    for (const AutoHit& h : mainHits) {
        bool chronoOk = false;
        const qreal chrono = TimelineChrono::parse(h.marker, &chronoOk);
        const QSet<QString> cast = castFor(h);

        // Decisão do usuário já persistida pra este evento? Usa direto.
        if (assignments.contains(h.evId)) {
            const QString forcedId = assignments.value(h.evId);
            Branch* b = findBranch(forcedId);
            if (!b) { Branch nb; nb.id = forcedId; branches.append(nb); b = &branches.last(); }
            touchBranch(b, chrono, chronoOk, cast, h);
            current = b;
            pending.active = false;
            continue;
        }

        const bool triggerA = chronoOk && current->lastChronoOk && chrono < current->lastChrono;
        const bool triggerB = !triggerA && chronoOk && current->lastChronoOk
                             && !cast.isEmpty() && !current->lastCast.isEmpty()
                             && (cast & current->lastCast).isEmpty();
        const bool triggerC = h.povOther;

        if (!triggerA && !triggerB && !triggerC) {
            pending.active = false; // sequência normal quebra qualquer candidato pendente
            touchBranch(current, chrono, chronoOk, cast, h);
            continue;
        }

        // Anomalia. Acha, entre as ramificações JÁ existentes (fora a
        // corrente), a mais próxima — candidata a "resumir".
        Branch* bestOther = nullptr; qreal bestOtherDist = 1e18;
        for (auto& b : branches) {
            if (&b == current) continue;
            const qreal d = distance(chrono, chronoOk, b, h.rank);
            if (d < bestOtherDist) { bestOtherDist = d; bestOther = &b; }
        }
        const qreal distOrigin = distance(chrono, chronoOk, *current, h.rank);
        const bool closerToOrigin = !bestOther || (distOrigin + kTieEpsilon < bestOtherDist);
        const bool closerToOther  = bestOther && (bestOtherDist + kTieEpsilon < distOrigin);

        if (closerToOrigin) {
            // Reabsorve — fora de ordem isolado, não confirma nada (ponto 1).
            pending.active = false;
            touchBranch(current, chrono, chronoOk, cast, h);
            continue;
        }
        if (closerToOther) {
            // Retoma a ramificação existente mais próxima; conecta com a
            // origem (convergência, ponto 6 — só conecta, nunca funde).
            pending.active = false;
            const QString originLastEv = current->lastEvId;
            touchBranch(bestOther, chrono, chronoOk, cast, h);
            addConvergence(originLastEv, h.evId);
            current = bestOther;
            continue;
        }
        if (bestOther) {
            // Empate real entre origem e outra ramificação — resíduo que só
            // o usuário decide (ponto 5). Reabsorve na origem enquanto isso.
            const QString mainLabel = current->id == kMainId ? tr("Narrativa") : current->id;
            const QString otherLabel = bestOther->id == kMainId ? tr("Narrativa") : bestOther->id;
            enqueueBranchAmbiguity(h.evId, h.title,
                { current->id, bestOther->id },
                { tr("Continua em: %1").arg(mainLabel), tr("Retoma: %1").arg(otherLabel) });
            pending.active = false;
            touchBranch(current, chrono, chronoOk, cast, h);
            continue;
        }

        // Nenhuma outra ramificação compete — candidata a ramificação NOVA.
        const bool matchesPending = pending.active && pending.originId == current->id
            && ((chronoOk && pending.chronoOk && qAbs(chrono - pending.chrono) < 3.0)
             || (!cast.isEmpty() && !pending.cast.isEmpty() && !(cast & pending.cast).isEmpty()));

        if (matchesPending) {
            // 2ª anomalia confirma o padrão (ponto 1) — cria a ramificação
            // agora, retroativa ao primeiro hit candidato.
            const QString newId = QStringLiteral("branch:auto:%1").arg(pending.firstEvId);
            Branch nb;
            nb.id = newId; nb.parentId = current->id;
            branches.append(nb);
            Branch* created = &branches.last();

            bool defFound = false;
            for (auto& d : defs) {
                if (d.id != newId) continue;
                d.kind = QString::fromLatin1(TimelineKind::Parallel);
                defFound = true;
                break;
            }
            if (!defFound) {
                TimelineDef d;
                d.id = newId;
                d.name = tr("Ramificação: %1").arg(h.title);
                d.color = randomBranchColor();
                d.kind = QString::fromLatin1(TimelineKind::Parallel);
                d.weight = QStringLiteral("secondary");
                d.parentId = current->id;
                d.branchFromEventId = pending.firstEvId;
                d.autoGenerated = true;
                d.railOrder = defs.size();
                defs.append(d);
            }

            assignEvent(pending.firstEvId, newId);
            touchBranch(created, chrono, chronoOk, cast, h);
            current = created;
            pending.active = false;
        } else {
            // Primeira anomalia isolada — candidata pendente, mas continua
            // na origem por enquanto.
            pending.active = true;
            pending.firstEvId = h.evId;
            pending.chrono = chrono; pending.chronoOk = chronoOk;
            pending.cast = cast;
            pending.originId = current->id;
            touchBranch(current, chrono, chronoOk, cast, h);
        }
    }

    // Limpa ramificações auto que ficaram sem nenhum evento (capítulo
    // apagado / marcador editado até deixar de ser anômalo).
    for (int i = defs.size() - 1; i >= 0; --i) {
        const TimelineDef& d = defs[i];
        if (!d.autoGenerated || d.kind != QString::fromLatin1(TimelineKind::Parallel)) continue;
        const bool hasEvents = std::any_of(events.begin(), events.end(),
            [&](const TimelineEvent& e){ return e.timelineId == d.id; });
        if (!hasEvents) defs.removeAt(i);
    }
}

QHash<QString, QString> TimelinePanel::loadBranchAssignments() const
{
    QHash<QString, QString> out;
    if (m_projectRoot.isEmpty()) return out;
    QSettings qs;
    qs.beginGroup(QStringLiteral("timelineBranchingState"));
    qs.beginGroup(QString::fromLatin1(
        QCryptographicHash::hash(m_projectRoot.toUtf8(), QCryptographicHash::Md5).toHex()));
    const QStringList pairs = qs.value(QStringLiteral("eventBranchAssignment")).toStringList();
    qs.endGroup();
    qs.endGroup();
    for (const QString& p : pairs) {
        const int eq = p.indexOf(QLatin1Char('='));
        if (eq <= 0) continue;
        out.insert(p.left(eq), p.mid(eq + 1));
    }
    return out;
}

void TimelinePanel::saveBranchAssignment(const QString& evId, const QString& branchId)
{
    if (m_projectRoot.isEmpty()) return;
    QHash<QString, QString> all = loadBranchAssignments();
    all.insert(evId, branchId);
    QStringList pairs;
    for (auto it = all.constBegin(); it != all.constEnd(); ++it)
        pairs << (it.key() + QLatin1Char('=') + it.value());
    QSettings qs;
    qs.beginGroup(QStringLiteral("timelineBranchingState"));
    qs.beginGroup(QString::fromLatin1(
        QCryptographicHash::hash(m_projectRoot.toUtf8(), QCryptographicHash::Md5).toHex()));
    qs.setValue(QStringLiteral("eventBranchAssignment"), pairs);
    qs.endGroup();
    qs.endGroup();
}

TimelineBranchPopup* TimelinePanel::ensureBranchPopup()
{
    if (!m_branchPopup) {
        m_branchPopup = new TimelineBranchPopup(this);
        connect(m_branchPopup, &TimelineBranchPopup::branchChosen, this,
                [this](const QString& evId, const QString& branchId) {
            saveBranchAssignment(evId, branchId);
            syncStoryTimeline(); // resync completo já aplica a decisão persistida
        });
    }
    return m_branchPopup;
}

void TimelinePanel::enqueueBranchAmbiguity(const QString& evId, const QString& evTitle,
                                           const QStringList& candidateIds,
                                           const QStringList& candidateLabels)
{
    // Só enfileira com o painel visível — se o resync rodou escondido (ex.:
    // depois do Gerador de Timeline em lote), o hit fica reabsorvido na
    // origem sem persistir nada; um resync futuro com o painel aberto
    // reavalia e pode perguntar de novo então.
    if (!isVisible()) return;
    TimelineBranchPopup::Ambiguity item;
    item.evId = evId;
    item.title = evTitle;
    item.candidateIds = candidateIds;
    item.candidateLabels = candidateLabels;
    auto* popup = ensureBranchPopup();
    popup->enqueue(item);
    popup->adjustSize();
    const QRect win = this->geometry();
    popup->move(win.x() + win.width() - popup->width() - 20,
               win.y() + win.height() - popup->height() - 20);
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
    CrashLogger::log("tlLoad");
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

    // Reflete o toggle de legado salvo neste projeto, e limpa filtro de
    // personagem de um projeto anterior (o botão é reaproveitado entre projetos).
    if (m_btnLegacyChars && m_projectModel) {
        QSignalBlocker block(m_btnLegacyChars);
        m_btnLegacyChars->setChecked(m_projectModel->legacyCharacterTracksEnabled());
    }
    setCharFilter(QString());

    // Gera as trilhas automáticas de personagem (e pergunta sobre secundários).
    syncCharacterTimelines(true);
    // Gera as trilhas automáticas "História"/"Flashback" a partir do timeMarker.
    syncStoryTimeline();
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
