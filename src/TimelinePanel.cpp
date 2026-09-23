#include "TimelinePanel.h"
#include "ColorPopover.h"

#include "CrashLogger.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "ProjectModel.h"
#include "RoleTiers.h"
#include "Theme.h"
#include "TerritorioStore.h"
#include "TimelineBranchPopup.h"
#include "TimelineChrono.h"
#include "ColorPopover.h"
#include "TimelineBraidView.h"
#include "TimelineInspector.h"
#include "TimelineTracksView.h"

#include <QAction>
#include <QButtonGroup>
#include <QEasingCurve>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVariantAnimation>
#include <functional>
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
    resize(1180, 720);
    m_legacyUi = QSettings().value(QStringLiteral("timeline/legacyUi"), false).toBool();
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

    // ── Filtro por território (M7 — Criador de Mundos) ──────────────────────────
    m_btnPlaceFilter = makeBtn(tr("Território: Todos"), tr("Filtrar eventos por território (onde aconteceu)"), m_toolbar);
    m_btnPlaceFilter->setPopupMode(QToolButton::InstantPopup);
    m_placeFilterMenu = new QMenu(m_btnPlaceFilter);
    m_btnPlaceFilter->setMenu(m_placeFilterMenu);
    connect(m_placeFilterMenu, &QMenu::aboutToShow, this, &TimelinePanel::rebuildTerritorioFilterMenu);
    tl->addWidget(m_btnPlaceFilter);

    // ── Trilhas de personagem (legado) — desligado por padrão ───────────────────
    m_btnLegacyChars = makeBtn(tr("Personagens (legado)"), tr(
        "Trilhas dedicadas por personagem (substituídas pela presença mostrada "
        "no evento + filtro acima). Religa a geração automática dessas trilhas."),
        m_toolbar);
    m_btnLegacyChars->setCheckable(true);
    tl->addWidget(m_btnLegacyChars);

    // Volta pra Timeline nova (esta barra só aparece na "UI Legado").
    m_btnNewUi = makeBtn(tr("UI nova"), tr("Voltar para a Linha do Tempo nova"), m_toolbar);
    tl->addWidget(m_btnNewUi);
    connect(m_btnNewUi, &QToolButton::clicked, this, [this]() { setLegacyUi(false); });

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

    // Timeline nova: barra própria + Trilhos/Trança + painel lateral. O
    // motor antigo (m_view) vive dentro da mesma pilha, pra Ramificações,
    // Espiral e a "UI Legado".
    auto* body = new QWidget(this);
    buildNewUi(body);
    root->addWidget(m_newTop);
    root->addWidget(body, 1);
    root->addWidget(m_newBottom);

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
    setLegacyUi(m_legacyUi);
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

// ── Filtro por território (M7) ───────────────────────────────────────────────

void TimelinePanel::rebuildTerritorioFilterMenu()
{
    if (!m_placeFilterMenu || !m_territorioStore) return;
    m_placeFilterMenu->clear();

    QAction* all = m_placeFilterMenu->addAction(tr("Todos"));
    connect(all, &QAction::triggered, this, [this]() { setPlaceFilter(QString()); });
    m_placeFilterMenu->addSeparator();

    QList<TerritorioStore::Territorio> territorios = m_territorioStore->territorios();
    std::sort(territorios.begin(), territorios.end(),
              [](const TerritorioStore::Territorio& a, const TerritorioStore::Territorio& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    for (const auto& t : territorios) {
        const QString name = t.name.isEmpty() ? tr("(sem nome)") : t.name;
        QAction* a = m_placeFilterMenu->addAction(name);
        a->setCheckable(true);
        a->setChecked(t.id == m_placeFilterId);
        const QString id = t.id;
        connect(a, &QAction::triggered, this, [this, id]() { setPlaceFilter(id); });
    }
}

void TimelinePanel::setPlaceFilter(const QString& territorioId)
{
    m_placeFilterId = territorioId;

    QString placeName;
    if (!territorioId.isEmpty() && m_territorioStore) {
        if (const auto* t = m_territorioStore->territorio(territorioId)) placeName = t->name;
    }
    if (m_btnPlaceFilter) {
        m_btnPlaceFilter->setText(placeName.isEmpty() ? tr("Território: Todos")
                                                       : tr("Território: %1").arg(placeName));
    }

    if (!m_scene) return;
    if (placeName.isEmpty()) {
        m_scene->setPlaceFilter(false, {});
        return;
    }

    QSet<QString> matchingIds;
    for (const auto& e : eventsForPlace(territorioId)) matchingIds.insert(e.id);
    m_scene->setPlaceFilter(true, matchingIds);
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
        const QColor c = ColorPopover::getColor(chosenColor, dlg, tr("Cor da timeline"));
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
        if (chosenColor != def.color) { def.userColor = true; def.colorTheme.clear(); }
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
    refreshNewUi();
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
        refreshNewUi();
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
        refreshNewUi();
        return;
    }
}

void TimelinePanel::createEventAt(const QPointF& scenePos)
{
    CrashLogger::log("tlCreateEventAt");
    TimelineEventPopup dlg(m_timelines, m_projectModel, m_territorioStore, this);
    dlg.setDocTextResolver(m_docTextResolver);
    if (dlg.exec() != QDialog::Accepted) return;
    commitEvent(dlg.eventData(), scenePos);
}

QString TimelinePanel::commitEvent(TimelineEvent e, const QPointF& scenePos)
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
    refreshNewUi();
    return e.id;
}

void TimelinePanel::promptNewEvent(const QString& description, const QString& marker,
                                   const QString& title, const QString& origin)
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

    TimelineEventPopup dlg(m_timelines, m_projectModel, m_territorioStore, this);
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
    TimelineEvent e = dlg.eventData();
    e.origin = origin;
    const QString id = commitEvent(e, c);
    if (!m_legacyUi) {
        if (m_newMode == NewMode::Engine) setNewMode(NewMode::Tracks);
        selectEvent(id);
    }
}

void TimelinePanel::openEditPopup(const QString& id)
{
    CrashLogger::log(QStringLiteral("tlOpenEditPopup id=%1").arg(id));
    auto* item = m_scene->findEvent(id);
    if (!item) return;

    const QString oldTimelineId = item->eventData().timelineId;

    TimelineEventPopup dlg(m_timelines, m_projectModel, m_territorioStore, this);
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
    updated.anchorKey     = prev.anchorKey;
    updated.anchorPos     = prev.anchorPos;
    updated.origin        = prev.origin;
    updated.originWhere   = prev.originWhere;
    updated.laneFollows   = prev.laneFollows && updated.timelineId == oldTimelineId;

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
    refreshNewUi();
}

void TimelinePanel::setProjectRoot(const QString& root)
{
    m_projectRoot = root;
    load();
}

QList<TimelineEvent> TimelinePanel::eventsForPlace(const QString& territorioId) const
{
    QList<TimelineEvent> out;
    if (territorioId.isEmpty()) return out;
    const QList<TimelineEvent> all = m_scene ? m_scene->allEventData() : m_events;
    for (const auto& e : all)
        if (e.placeId == territorioId) out.append(e);
    return out;
}

void TimelinePanel::clearPlaceReferences(const QString& territorioId)
{
    if (territorioId.isEmpty()) return;
    bool changed = false;
    for (auto& e : m_events) {
        if (e.placeId != territorioId) continue;
        e.placeId.clear();
        changed = true;
        if (m_scene) {
            if (auto* item = m_scene->findEvent(e.id)) {
                TimelineEvent d = item->eventData();
                d.placeId.clear();
                item->setEventData(d);
            }
        }
    }
    if (changed) save();
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
            if (!isVisible()) { m_staleWhileHidden = true; return; }
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
                c.title.isEmpty() ? m_projectModel->chapterDisplayLabel(c) : c.title,
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
    refreshNewUi();
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
            const QString chapTitle = c.title.isEmpty() ? m_projectModel->chapterDisplayLabel(c) : c.title;

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
    if (m_elementsStore && m_presenceProvider) {
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

    // Linha escolhida pelo usuário (arraste na Timeline nova) vence a
    // detecção — guarda a automática antes, pro "Desfazer" e pro aviso.
    m_autoLaneOf.clear();
    for (auto& e : events) {
        if (!e.id.startsWith(QStringLiteral("story:"))) continue;
        m_autoLaneOf.insert(e.id, e.timelineId);
        const QString want = m_laneOverrides.value(e.id);
        if (want.isEmpty()) continue;
        const bool exists = std::any_of(defs.begin(), defs.end(),
                                        [&](const TimelineDef& d) { return d.id == want; });
        if (exists) e.timelineId = want;
    }

    // remove eventos auto obsoletos das duas trilhas (capítulo apagado / marcador
    // esvaziado). Identidade "é meu" = prefixo "story:" do id (não dá pra usar
    // autoEvent aqui, ver comentário acima) — evento manual do usuário nessas
    // trilhas sempre tem um QUuid aleatório, nunca colide com esse prefixo.
    events.erase(std::remove_if(events.begin(), events.end(), [&](const TimelineEvent& e) {
        // qualquer linha: evento de capítulo movido pra ramificação/linha
        // manual também some quando o capítulo perde o marcador
        return e.id.startsWith(QStringLiteral("story:"))
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
    m_presentByEvId = presentByEvId;
    refreshNewUi();
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
            auto labelOf = [&](const QString& bid) {
                if (bid == kMainId) return tr("Narrativa");
                for (const auto& d : defs) if (d.id == bid && !d.name.isEmpty()) return d.name;
                return tr("outra linha");
            };
            const QString mainLabel = labelOf(current->id);
            const QString otherLabel = labelOf(bestOther->id);
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
        if (t.userColor) o[QStringLiteral("userColor")] = true;
        if (!t.colorTheme.isEmpty()) o[QStringLiteral("colorTheme")] = t.colorTheme;
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
        o[QStringLiteral("placeId")]       = e.placeId;
        o[QStringLiteral("autoEvent")]     = e.autoEvent;
        if (!e.anchorKey.isEmpty())   o[QStringLiteral("anchorKey")]   = e.anchorKey;
        if (e.anchorPos >= 0)         o[QStringLiteral("anchorPos")]   = e.anchorPos;
        if (!e.origin.isEmpty())      o[QStringLiteral("origin")]      = e.origin;
        if (!e.originWhere.isEmpty()) o[QStringLiteral("originWhere")] = e.originWhere;
        if (e.laneFollows)            o[QStringLiteral("laneFollows")] = true;
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

    QJsonObject ov;
    for (auto it = m_laneOverrides.constBegin(); it != m_laneOverrides.constEnd(); ++it) ov[it.key()] = it.value();
    root[QStringLiteral("laneOverrides")] = ov;

    QJsonObject ui;
    ui[QStringLiteral("mode")] = m_newMode == NewMode::Braid ? QStringLiteral("braid")
                               : m_newMode == NewMode::Engine ? QStringLiteral("engine")
                                                              : QStringLiteral("tracks");
    if (m_tracks) {
        ui[QStringLiteral("density")] = int(m_tracks->density());
        ui[QStringLiteral("lanesPaneH")] = m_tracks->lanesPaneHeight();
    }
    if (!m_msId.isEmpty()) ui[QStringLiteral("manuscript")] = m_msId;
    root[QStringLiteral("ui")] = ui;

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
        t.userColor         = o[QStringLiteral("userColor")].toBool(false);
        t.colorTheme        = o[QStringLiteral("colorTheme")].toString();
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
        e.placeId      = o[QStringLiteral("placeId")].toString();
        e.autoEvent    = o[QStringLiteral("autoEvent")].toBool(false);
        e.anchorKey    = o[QStringLiteral("anchorKey")].toString();
        e.anchorPos    = o[QStringLiteral("anchorPos")].toInt(-1);
        e.origin       = o[QStringLiteral("origin")].toString();
        e.originWhere  = o[QStringLiteral("originWhere")].toString();
        e.laneFollows  = o[QStringLiteral("laneFollows")].toBool(false);
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

    m_laneOverrides.clear();
    const QJsonObject ov = root[QStringLiteral("laneOverrides")].toObject();
    for (auto it = ov.constBegin(); it != ov.constEnd(); ++it) m_laneOverrides.insert(it.key(), it.value().toString());

    {
        const QJsonObject ui = root[QStringLiteral("ui")].toObject();
        const QString mode = ui[QStringLiteral("mode")].toString(QStringLiteral("tracks"));
        m_newMode = mode == QLatin1String("braid") ? NewMode::Braid
                  : mode == QLatin1String("engine") ? NewMode::Engine : NewMode::Tracks;
        m_msId = ui[QStringLiteral("manuscript")].toString();
        m_sel.clear();
        m_tf = Tracks::Filter();
        if (m_tracks) {
            const int den = qBound(0, ui[QStringLiteral("density")].toInt(1), 2);
            m_tracks->setDensity(TimelineTracksView::Density(den));
            if (auto* b = m_densityGrp->button(den)) b->setChecked(true);
            m_tracks->setLanesPaneHeight(ui[QStringLiteral("lanesPaneH")].toInt(-1));
        }
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
    m_staleWhileHidden = false;
    setLegacyUi(m_legacyUi);
}

void TimelinePanel::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    // Mudanças no manuscrito enquanto a janela estava fechada: resync agora.
    if (m_staleWhileHidden) {
        m_staleWhileHidden = false;
        refreshFromModel();
    } else {
        refreshNewUi();
    }
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

    const QString qss = Theme::qss(QStringLiteral(R"(
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
            border-radius: @radius-control;
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
            /* Circular por construção: o botão é 36x36, então o raio é metade
               do lado. Não entra na escala do tema — viraria elipse. */
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
    )")).arg(Theme::appBackground(),
            Theme::panelBackground(),
            Theme::subtleBorder(),
            Theme::textPrimary(),
            Theme::textMuted(),
            Theme::hoverOverlay(),
            Theme::panelBackground(),
            Theme::accentDefault());

    setStyleSheet(qss);
    resolveThemeBoundLaneColors();
    applyNewTheme();
    refreshNewUi();
}

// ═════════════════════════════════════════════════════════════════════════════
// Timeline nova — Trilhos, Trança e painel lateral
// ═════════════════════════════════════════════════════════════════════════════

namespace {
// Ícones de traço simples (mesmo desenho do protótipo aprovado).
QPixmap tlIcon(const QString& kind, const QColor& c, int px = 14)
{
    const qreal dpr = 2.0;
    QPixmap pm(QSize(px, px) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(px / 16.0, px / 16.0);
    p.setPen(QPen(c, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    if (kind == QLatin1String("chevron")) {
        path.moveTo(4, 6); path.lineTo(8, 10); path.lineTo(12, 6);
    } else if (kind == QLatin1String("lanes")) {
        for (int y : { 4, 8, 12 }) { path.moveTo(2, y); path.lineTo(14, y); }
    } else if (kind == QLatin1String("braid")) {
        path.moveTo(3, 2); path.cubicTo(3, 8, 13, 8, 13, 14);
        path.moveTo(13, 2); path.cubicTo(13, 8, 3, 8, 3, 14);
    } else if (kind == QLatin1String("search")) {
        path.addEllipse(QPointF(7, 7), 4.5, 4.5);
        path.moveTo(10.5, 10.5); path.lineTo(14, 14);
    } else if (kind == QLatin1String("plus")) {
        path.moveTo(8, 3); path.lineTo(8, 13); path.moveTo(3, 8); path.lineTo(13, 8);
    } else if (kind == QLatin1String("dots")) {
        p.setBrush(c); p.setPen(Qt::NoPen);
        for (qreal x : { 3.5, 8.0, 12.5 }) p.drawEllipse(QPointF(x, 8), 1.1, 1.1);
        return pm;
    } else if (kind == QLatin1String("close")) {
        path.moveTo(4, 4); path.lineTo(12, 12); path.moveTo(12, 4); path.lineTo(4, 12);
    }
    p.drawPath(path);
    return pm;
}

// Chip de filtro: com filtro ativo, clicar no "×" da direita limpa sem abrir o menu.
class FilterChip : public QToolButton {
public:
    using QToolButton::QToolButton;
    std::function<void()> onClear;
    bool active = false;
protected:
    void mousePressEvent(QMouseEvent* e) override
    {
        if (active && e->position().x() > width() - 22 && onClear) { onClear(); e->accept(); return; }
        QToolButton::mousePressEvent(e);
    }
};

QString chapterRuler(const ProjectModel* pm, const Chapter& c)
{
    QString n = pm->chapterNumberLabel(c);
    if (n.isEmpty()) {
        // tipo especial sem número (Prólogo, Epílogo...): abreviação curta
        const QString lab = pm->chapterDisplayLabel(c).trimmed();
        n = lab.size() > 5 ? lab.left(4) + QLatin1Char('.') : lab;
    }
    return n;
}

QString unitRuler(const ProjectModel* pm, const Chapter& c, int sceneIndex)
{
    const QString n = chapterRuler(pm, c);
    return sceneIndex >= 0 && c.scenes.size() > 0
        ? n + QStringLiteral("·") + QString::number(sceneIndex + 1) : n;
}

QString suggestTitleFrom(const QString& text)
{
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
}

QColor colorForIdNew(const QString& id)
{
    uint h = 0;
    for (const QChar c : id) h = h * 131u + c.unicode();
    return QColor::fromHsv(int(h % 360), 150, 210);
}
} // namespace

void TimelinePanel::buildNewUi(QWidget* body)
{
    // ── barra de cima ────────────────────────────────────────────────────────
    m_newTop = new QWidget(this);
    m_newTop->setObjectName(QStringLiteral("tlNewTop"));
    m_newTop->setAttribute(Qt::WA_StyledBackground);
    m_newTop->setFixedHeight(48);
    auto* top = new QHBoxLayout(m_newTop);
    top->setContentsMargins(16, 0, 12, 0);
    top->setSpacing(8);

    auto* title = new QLabel(tr("Linha do Tempo"), m_newTop);
    title->setObjectName(QStringLiteral("tlNewTitle"));
    top->addWidget(title);
    top->addSpacing(4);

    m_msBtn = new QToolButton(m_newTop);
    m_msBtn->setObjectName(QStringLiteral("tlMs"));
    m_msBtn->setPopupMode(QToolButton::InstantPopup);
    m_msBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_msBtn->setLayoutDirection(Qt::RightToLeft); // chevron à direita
    m_msBtn->setCursor(Qt::PointingHandCursor);
    m_msBtn->setMenu(new QMenu(m_msBtn));
    connect(m_msBtn->menu(), &QMenu::aboutToShow, this, &TimelinePanel::rebuildNewMenus);
    top->addWidget(m_msBtn);

    auto* seg = new QFrame(m_newTop);
    seg->setObjectName(QStringLiteral("tlSeg"));
    auto* segL = new QHBoxLayout(seg);
    segL->setContentsMargins(2, 2, 2, 2);
    segL->setSpacing(0);
    auto makeSeg = [&](const QString& text, QWidget* parent) {
        auto* b = new QToolButton(parent);
        b->setObjectName(QStringLiteral("tlSegBtn"));
        b->setText(text);
        b->setCheckable(true);
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(24);
        return b;
    };
    m_segTracks = makeSeg(tr("Trilhos"), seg);
    m_segBraid  = makeSeg(tr("Trança"), seg);
    segL->addWidget(m_segTracks);
    segL->addWidget(m_segBraid);
    seg->setFixedHeight(30);
    top->addWidget(seg, 0, Qt::AlignVCenter);
    connect(m_segTracks, &QToolButton::clicked, this, [this]() { setNewMode(NewMode::Tracks); });
    connect(m_segBraid,  &QToolButton::clicked, this, [this]() { setNewMode(NewMode::Braid); });

    top->addStretch(1);

    m_searchBox = new QFrame(m_newTop);
    m_searchBox->setObjectName(QStringLiteral("tlSearch"));
    auto* sl = new QHBoxLayout(m_searchBox);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(0);
    m_searchBtn = new QToolButton(m_searchBox);
    m_searchBtn->setObjectName(QStringLiteral("tlSearchBtn"));
    m_searchBtn->setFixedSize(28, 28);
    m_searchBtn->setCursor(Qt::PointingHandCursor);
    m_searchBtn->setToolTip(tr("Buscar evento"));
    m_searchEdit = new QLineEdit(m_searchBox);
    m_searchEdit->setObjectName(QStringLiteral("tlSearchEdit"));
    m_searchEdit->setPlaceholderText(tr("Buscar evento"));
    m_searchEdit->setMaximumWidth(0);
    m_searchEdit->setFixedWidth(0);
    m_searchEdit->installEventFilter(this);
    sl->addWidget(m_searchBtn);
    sl->addWidget(m_searchEdit);
    m_searchBox->setFixedHeight(28);
    top->addWidget(m_searchBox, 0, Qt::AlignVCenter);
    connect(m_searchBtn, &QToolButton::clicked, this, [this]() {
        if (m_searchEdit->width() > 0) { m_searchEdit->setFocus(); return; }
        auto* a = new QVariantAnimation(this);
        a->setDuration(220);
        a->setEasingCurve(QEasingCurve::OutCubic);
        a->setStartValue(0);
        a->setEndValue(182);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_searchEdit->setFixedWidth(v.toInt());
        });
        a->start(QAbstractAnimation::DeleteWhenStopped);
        m_searchBox->setProperty("open", true);
        m_searchBox->style()->unpolish(m_searchBox);
        m_searchBox->style()->polish(m_searchBox);
        m_searchEdit->setFocus();
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
        m_tf.query = t.trimmed();
        applyTracksFilter();
    });

    auto makeChip = [&](const QString& tip) {
        auto* c = new FilterChip(m_newTop);
        c->setObjectName(QStringLiteral("tlChip"));
        c->setPopupMode(QToolButton::InstantPopup);
        c->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        c->setLayoutDirection(Qt::RightToLeft);
        c->setCursor(Qt::PointingHandCursor);
        c->setToolTip(tip);
        c->setMenu(new QMenu(c));
        connect(c->menu(), &QMenu::aboutToShow, this, &TimelinePanel::rebuildNewMenus);
        return c;
    };
    auto* chipChar = makeChip(tr("Filtrar eventos por personagem presente"));
    chipChar->onClear = [this]() { m_tf.charId.clear(); applyTracksFilter(); };
    m_chipChar = chipChar;
    auto* chipPlace = makeChip(tr("Filtrar eventos pelo lugar onde aconteceram"));
    chipPlace->onClear = [this]() { m_tf.placeId.clear(); applyTracksFilter(); };
    m_chipPlace = chipPlace;
    top->addWidget(m_chipChar);
    top->addWidget(m_chipPlace);

    auto* add = new QPushButton(tr("Evento"), m_newTop);
    add->setObjectName(QStringLiteral("tlAddBtn"));
    add->setCursor(Qt::PointingHandCursor);
    add->setToolTip(tr("Novo evento (vai para \"Soltos\" até você arrastá-lo para uma linha)"));
    add->setFixedHeight(28);
    top->addSpacing(4);
    top->addWidget(add);
    connect(add, &QPushButton::clicked, this, [this]() {
        TimelineEventPopup dlg(m_timelines, m_projectModel, m_territorioStore, this);
        dlg.setDocTextResolver(m_docTextResolver);
        if (dlg.exec() != QDialog::Accepted) return;
        TimelineEvent e = dlg.eventData();
        e.origin = QStringLiteral("new");
        const QString id = commitEvent(e, QPointF());
        selectEvent(id);
    });

    m_moreBtn = new QToolButton(m_newTop);
    m_moreBtn->setObjectName(QStringLiteral("tlIconBtn"));
    m_moreBtn->setFixedSize(28, 28);
    m_moreBtn->setPopupMode(QToolButton::InstantPopup);
    m_moreBtn->setCursor(Qt::PointingHandCursor);
    m_moreBtn->setToolTip(tr("Mais"));
    m_moreBtn->setMenu(new QMenu(m_moreBtn));
    connect(m_moreBtn->menu(), &QMenu::aboutToShow, this, &TimelinePanel::rebuildNewMenus);
    top->addWidget(m_moreBtn);

    auto* close = new QToolButton(m_newTop);
    close->setObjectName(QStringLiteral("tlIconBtn"));
    close->setProperty("iconKind", QStringLiteral("close"));
    close->setFixedSize(28, 28);
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip(tr("Fechar"));
    top->addWidget(close);
    connect(close, &QToolButton::clicked, this, &TimelinePanel::closeRequested);

    // ── corpo: trilhos / trança / motor antigo + painel lateral ─────────────
    auto* bodyL = new QHBoxLayout(body);
    bodyL->setContentsMargins(0, 0, 0, 0);
    bodyL->setSpacing(0);
    m_stack = new QStackedWidget(body);
    m_tracks = new TimelineTracksView(m_stack);
    m_braid  = new TimelineBraidView(m_stack);
    m_stack->addWidget(m_tracks);
    m_stack->addWidget(m_braid);
    m_stack->addWidget(m_view);
    bodyL->addWidget(m_stack, 1);

    // Painel lateral: o contêiner anima a própria largura (0 ↔ 300) e o painel
    // fica encostado na borda direita dele — entra deslizando da direita.
    m_inspHolder = new QWidget(body);
    m_inspHolder->setFixedWidth(0);
    m_inspector = new TimelineInspector(m_inspHolder);
    m_inspHolder->installEventFilter(this);
    bodyL->addWidget(m_inspHolder);
    m_inspAnim = new QVariantAnimation(this);
    m_inspAnim->setDuration(220);
    m_inspAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_inspAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_inspHolder->setFixedWidth(v.toInt());
    });

    connect(m_tracks, &TimelineTracksView::eventClicked, this, &TimelinePanel::selectEvent);
    connect(m_tracks, &TimelineTracksView::backgroundClicked, this, [this]() { selectEvent(QString()); });
    connect(m_tracks, &TimelineTracksView::laneClicked, this, [this](const QString& id) {
        m_tf.laneId = m_tf.laneId == id ? QString() : id;
        applyTracksFilter();
    });
    connect(m_tracks, &TimelineTracksView::characterClicked, this, [this](const QString& id) {
        m_tf.charId = m_tf.charId == id ? QString() : id;
        applyTracksFilter();
    });
    connect(m_tracks, &TimelineTracksView::densityChangeRequested, this, [this](int d) {
        m_tracks->setDensity(TimelineTracksView::Density(d));
        if (auto* b = m_densityGrp->button(d)) b->setChecked(true);
    });
    connect(m_tracks, &TimelineTracksView::eventDropped, this, &TimelinePanel::onEventDropped);
    connect(m_tracks, &TimelineTracksView::createAtRequested, this, &TimelinePanel::createAtColumn);
    connect(m_tracks, &TimelineTracksView::layoutChanged, this, [this]() { save(); });
    connect(m_tracks, &TimelineTracksView::laneColorRequested, this, &TimelinePanel::openLaneColor);
    connect(m_tracks, &TimelineTracksView::laneContextMenu, this, [this](const QString& laneId, const QPoint& gp) {
        const TimelineDef* def = nullptr;
        for (const auto& t : m_timelines) if (t.id == laneId) def = &t;
        QMenu menu(this);
        QAction* color = menu.addAction(tr("Cor da linha…"));
        QAction* edit = nullptr;
        QAction* del = nullptr;
        if (def && !def->autoGenerated) {
            edit = menu.addAction(tr("Editar linha…"));
            menu.addSeparator();
            del = menu.addAction(tr("Excluir linha…"));
        }
        QAction* chosen = menu.exec(gp);
        if (chosen == color) openLaneColor(laneId, gp);
        else if (edit && chosen == edit) editTimeline(laneId);
        else if (del && chosen == del) deleteTimeline(laneId);
    });
    connect(m_tracks, &TimelineTracksView::eventContextMenu, this,
            [this](const QString& id, const QPoint& gp) {
        const Tracks::Event* e = m_tracksData.event(id);
        if (!e) return;
        QMenu menu(this);
        QAction* edit = menu.addAction(tr("Editar evento"));
        QAction* exp  = menu.addAction(tr("Exportar como documento"));
        QAction* del  = nullptr;
        if (e->manual) { menu.addSeparator(); del = menu.addAction(tr("Remover")); }
        QAction* chosen = menu.exec(gp);
        if (chosen == edit) editTracksEvent(id);
        else if (chosen == exp) {
            if (const TimelineEvent* le = liveEvent(id)) onExportEventAsDoc(*le);
        } else if (del && chosen == del) removeManualEvent(id);
    });
    connect(m_braid, &TimelineBraidView::eventClicked, this, &TimelinePanel::selectEvent);
    connect(m_braid, &TimelineBraidView::backgroundClicked, this, [this]() { selectEvent(QString()); });

    connect(m_inspector, &TimelineInspector::closeRequested, this, [this]() { selectEvent(QString()); });
    connect(m_inspector, &TimelineInspector::selectRequested, this, &TimelinePanel::selectEvent);
    connect(m_inspector, &TimelineInspector::openInEditorRequested, this, &TimelinePanel::openEventInEditor);
    connect(m_inspector, &TimelineInspector::editRequested, this, &TimelinePanel::editTracksEvent);
    connect(m_inspector, &TimelineInspector::exportRequested, this, [this](const QString& id) {
        if (const TimelineEvent* le = liveEvent(id)) { onExportEventAsDoc(*le); return; }
        if (const Tracks::Event* te = m_tracksData.event(id)) {
            TimelineEvent tmp; tmp.title = te->title; tmp.description = te->summary;
            onExportEventAsDoc(tmp);
        }
    });
    connect(m_inspector, &TimelineInspector::fillMarkerRequested, this, &TimelinePanel::fillMarker);
    connect(m_inspector, &TimelineInspector::undoMoveRequested, this, &TimelinePanel::undoLaneMove);
    connect(m_inspector, &TimelineInspector::characterClicked, this, [this](const QString& id) {
        m_tf.charId = m_tf.charId == id ? QString() : id;
        applyTracksFilter();
    });

    // ── rodapé ───────────────────────────────────────────────────────────────
    m_newBottom = new QWidget(this);
    m_newBottom->setObjectName(QStringLiteral("tlNewBottom"));
    m_newBottom->setAttribute(Qt::WA_StyledBackground);
    m_newBottom->setFixedHeight(36);
    auto* bot = new QHBoxLayout(m_newBottom);
    bot->setContentsMargins(16, 0, 12, 0);
    bot->setSpacing(14);
    m_statsLbl = new QLabel(m_newBottom);
    m_statsLbl->setObjectName(QStringLiteral("tlStats"));
    bot->addWidget(m_statsLbl);
    m_noDateBtn = new QToolButton(m_newBottom);
    m_noDateBtn->setObjectName(QStringLiteral("tlNoDate"));
    m_noDateBtn->setCursor(Qt::PointingHandCursor);
    m_noDateBtn->setToolTip(tr("Mostrar o primeiro capítulo sem marcador de tempo"));
    bot->addWidget(m_noDateBtn);
    connect(m_noDateBtn, &QToolButton::clicked, this, [this]() {
        for (const auto& e : m_tracksData.events)
            if (e.hollow) {
                if (m_newMode == NewMode::Tracks) m_tracks->scrollToColumn(e.col);
                selectEvent(e.id);
                return;
            }
    });
    bot->addStretch(1);
    m_densityBox = new QWidget(m_newBottom);
    auto* dl = new QHBoxLayout(m_densityBox);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->setSpacing(6);
    auto* kbd = new QLabel(QStringLiteral("Ctrl"), m_densityBox);
    kbd->setObjectName(QStringLiteral("tlKbd"));
    kbd->setFixedHeight(18);
    auto* kbdTail = new QLabel(tr("+ roda"), m_densityBox);
    kbdTail->setObjectName(QStringLiteral("tlStats"));
    dl->addWidget(kbd, 0, Qt::AlignVCenter);
    dl->addWidget(kbdTail, 0, Qt::AlignVCenter);
    dl->addSpacing(8);
    auto* dseg = new QFrame(m_densityBox);
    dseg->setObjectName(QStringLiteral("tlSeg"));
    auto* dsl = new QHBoxLayout(dseg);
    dsl->setContentsMargins(2, 2, 2, 2);
    dsl->setSpacing(0);
    m_densityGrp = new QButtonGroup(this);
    m_densityGrp->setExclusive(true);
    const QStringList dn = { tr("Pontos"), tr("Títulos"), tr("Resumos") };
    for (int i = 0; i < 3; ++i) {
        auto* b = new QToolButton(dseg);
        b->setObjectName(QStringLiteral("tlSegBtnSm"));
        b->setText(dn[i]);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(20);
        m_densityGrp->addButton(b, i);
        dsl->addWidget(b);
    }
    m_densityGrp->button(1)->setChecked(true);
    dseg->setFixedHeight(26);
    connect(m_densityGrp, &QButtonGroup::idClicked, this, [this](int id) {
        m_tracks->setDensity(TimelineTracksView::Density(id));
    });
    dl->addWidget(dseg, 0, Qt::AlignVCenter);
    bot->addWidget(m_densityBox);
}

void TimelinePanel::applyNewTheme()
{
    if (!m_newTop) return;
    const Tracks::Palette pal = Tracks::Palette::current();
    auto css = [](const QColor& c) { return c.name(QColor::HexArgb); };
    const QString menuQss = Theme::qss(QStringLiteral(R"(
        QMenu { background: %1; border: 1px solid %2; border-radius: @radius-control; padding: 4px; }
        QMenu::item { color: %3; padding: 6px 14px 6px 10px; border-radius: @radius-item; font-size: 12px; }
        QMenu::item:selected { background: %4; color: %5; }
        QMenu::item:disabled { color: %6; font-size: 10px; }
        QMenu::separator { height: 1px; background: %2; margin: 4px 6px; }
        QMenu::indicator { width: 12px; height: 12px; }
    )")).arg(pal.panel.name(), pal.border.name(), pal.ink.name(), css(Tracks::alpha(pal.ink, 0.09)),
             pal.bright.name(), pal.dim.name());
    for (QToolButton* b : { m_msBtn, m_chipChar, m_chipPlace, m_moreBtn })
        if (b && b->menu()) b->menu()->setStyleSheet(menuQss);

    m_newTop->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QWidget#tlNewTop { background: %1; border-bottom: 1px solid %2; }
        QLabel#tlNewTitle { color: %3; font-size: 14px; font-weight: 600; }
        QToolButton#tlMs { background: transparent; border: none; color: %3; font-family: '%9';
                           font-size: 13px; padding: 0 6px; min-height: 28px; }
        QToolButton#tlMs::menu-indicator, QToolButton#tlChip::menu-indicator,
        QToolButton#tlIconBtn::menu-indicator { image: none; width: 0; }
        QFrame#tlSeg { background: %4; border: 1px solid %2; border-radius: @radius-control; }
        QToolButton#tlSegBtn { background: transparent; border: none; border-radius: 4px; color: %5;
                               font-size: 12px; padding: 0 12px; }
        QToolButton#tlSegBtn:checked { background: %1; color: %3; }
        QFrame#tlSearch { background: transparent; border-radius: @radius-control; }
        QFrame#tlSearch[open="true"] { background: %6; }
        QToolButton#tlSearchBtn, QToolButton#tlIconBtn { background: transparent; border: none;
                                                         border-radius: @radius-control; }
        QToolButton#tlSearchBtn:hover, QToolButton#tlIconBtn:hover { background: %7; }
        QLineEdit#tlSearchEdit { background: transparent; border: none; color: %3; font-size: 12px;
                                 padding-right: 9px; selection-background-color: %8; }
        QToolButton#tlChip { background: transparent; border: none; border-radius: 14px; color: %10;
                             font-size: 12px; padding: 0 10px; min-height: 28px; }
        QToolButton#tlChip:hover { background: %7; }
        QToolButton#tlChip[active="true"] { background: %11; color: %3; }
        QPushButton#tlAddBtn { background: %12; color: %13; border: none; border-radius: @radius-control;
                               font-size: 12px; font-weight: 600; padding: 0 12px; }
        QPushButton#tlAddBtn:hover { background: %14; }
    )")).arg(pal.panel.name(),                       // 1
             pal.border.name(),                      // 2
             pal.bright.name(),                      // 3
             css(Tracks::alpha(pal.ink, 0.07)),      // 4
             pal.dim.name(),                         // 5
             css(Tracks::alpha(pal.ink, 0.07)),      // 6
             css(Tracks::alpha(pal.ink, 0.08)),      // 7
             css(Tracks::alpha(pal.accent, 0.35)),   // 8
             Tracks::serifFont(13).family(),         // 9
             pal.ink.name(),                         // 10
             css(Tracks::alpha(pal.accent, 0.14)),   // 11
             pal.accent.name(),                      // 12
             pal.app.name(),                         // 13
             Tracks::mix(pal.accent, pal.bright, 0.85).name())); // 14

    m_newBottom->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QWidget#tlNewBottom { background: %1; border-top: 1px solid %2; }
        QLabel#tlStats { color: %3; font-size: 11.5px; }
        QToolButton#tlNoDate { background: transparent; border: none; color: %4; font-size: 11.5px; padding: 0; }
        QToolButton#tlNoDate:hover { text-decoration: underline; }
        QToolButton#tlNoDate[allDated="true"] { color: %5; }
        QLabel#tlKbd { color: %6; font-family: '%7'; font-size: 10.5px; border: 1px solid %2;
                       border-radius: 3px; padding: 0 5px; }
        QFrame#tlSeg { background: %8; border: 1px solid %2; border-radius: @radius-control; }
        QToolButton#tlSegBtnSm { background: transparent; border: none; border-radius: 4px; color: %3;
                                 font-size: 11px; padding: 0 9px; }
        QToolButton#tlSegBtnSm:checked { background: %1; color: %9; }
    )")).arg(pal.panel.name(), pal.border.name(), pal.dim.name(), pal.warning.name(),
             pal.success.name(), pal.ink.name(), Tracks::monoFont(10).family(),
             css(Tracks::alpha(pal.ink, 0.07)), pal.bright.name()));

    m_segTracks->setIcon(QIcon(tlIcon(QStringLiteral("lanes"), m_segTracks->isChecked() ? pal.bright : pal.dim)));
    m_segBraid->setIcon(QIcon(tlIcon(QStringLiteral("braid"), m_segBraid->isChecked() ? pal.bright : pal.dim)));
    m_searchBtn->setIcon(QIcon(tlIcon(QStringLiteral("search"), pal.dim)));
    m_moreBtn->setIcon(QIcon(tlIcon(QStringLiteral("dots"), pal.dim)));
    m_msBtn->setIcon(QIcon(tlIcon(QStringLiteral("chevron"), pal.ink)));
    for (auto* b : m_newTop->findChildren<QToolButton*>(QStringLiteral("tlIconBtn")))
        if (b->property("iconKind").toString() == QLatin1String("close"))
            b->setIcon(QIcon(tlIcon(QStringLiteral("close"), pal.dim)));
    if (auto* add = m_newTop->findChild<QPushButton*>(QStringLiteral("tlAddBtn")))
        add->setIcon(QIcon(tlIcon(QStringLiteral("plus"), pal.app)));
    refreshNewChips();
    if (m_tracks) m_tracks->update();
    if (m_braid) m_braid->update();
    if (m_inspector && !m_sel.isEmpty()) m_inspector->showFor(m_tracksData, m_sel);
}

QString TimelinePanel::currentManuscriptId() const
{
    if (!m_projectModel) return {};
    const auto& mss = m_projectModel->manuscripts();
    auto exists = [&](const QString& id) {
        return std::any_of(mss.begin(), mss.end(), [&](const Manuscript& m) { return m.id == id; });
    };
    if (!m_msId.isEmpty() && exists(m_msId)) return m_msId;
    if (!m_editorMsId.isEmpty() && exists(m_editorMsId)) return m_editorMsId;
    return mss.isEmpty() ? QString() : mss.first().id;
}

Tracks::Data TimelinePanel::buildTracksData() const
{
    Tracks::Data d;
    if (!m_projectModel || !m_scene) return d;
    const Tracks::Palette pal = Tracks::Palette::current();

    const QString msId = currentManuscriptId();
    const auto& mss = m_projectModel->manuscripts();
    const bool msIsFirst = !mss.isEmpty() && mss.first().id == msId;
    for (const Manuscript& m : mss)
        if (m.id == msId) {
            d.manuscriptTitle = m.title.isEmpty() ? tr("Sem título") : m.title;
            d.startMarker = m.storyStartMarker;
            d.startChrono = TimelineChrono::parse(m.storyStartMarker, &d.startOk);
        }

    // ── colunas: capítulos (ou cenas) do manuscrito, em ordem de leitura ──────
    QList<Chapter> chs;
    for (const Chapter& c : m_projectModel->chapters())
        if (c.manuscriptId == msId || (c.manuscriptId.isEmpty() && (msIsFirst || msId.isEmpty()))) chs << c;
    std::sort(chs.begin(), chs.end(), [](const Chapter& a, const Chapter& b) { return a.order < b.order; });
    d.chapterCount = chs.size();

    struct UnitInfo { QString title, marker, summary; };
    QList<UnitInfo> units;
    for (const Chapter& c : chs) {
        const QString chTitle = c.title.isEmpty() ? m_projectModel->chapterDisplayLabel(c) : c.title;
        const QString chLabel = m_projectModel->chapterDisplayLabel(c);
        if (c.scenes.isEmpty()) {
            Tracks::Column col;
            col.key = c.id + QStringLiteral(":0");
            col.chapterId = c.id; col.sceneIndex = -1; col.manuscriptId = c.manuscriptId;
            col.ruler = unitRuler(m_projectModel, c, -1);
            col.unitLabel = chLabel;
            d.cols << col;
            units << UnitInfo{ chTitle, c.timeMarker, c.summary };
        } else {
            for (int si = 0; si < c.scenes.size(); ++si) {
                const Scene& s = c.scenes[si];
                Tracks::Column col;
                col.key = c.id + QLatin1Char(':') + QString::number(si);
                col.chapterId = c.id; col.sceneIndex = si; col.manuscriptId = c.manuscriptId;
                col.ruler = unitRuler(m_projectModel, c, si);
                col.unitLabel = tr("%1 · Cena %2").arg(chLabel).arg(si + 1);
                d.cols << col;
                units << UnitInfo{ s.title.isEmpty() ? tr("%1 · Cena %2").arg(chTitle).arg(si + 1) : s.title,
                                   s.timeMarker.isEmpty() ? c.timeMarker : s.timeMarker,
                                   s.summary.isEmpty() ? c.summary : s.summary };
            }
        }
    }
    QHash<QString, int> colOfKey;
    for (int i = 0; i < d.cols.size(); ++i) colOfKey.insert(d.cols[i].key, i);

    // ── personagens (id por nome) ──────────────────────────────────────────────
    QHash<QString, Element> charByName;
    if (m_elementsStore)
        for (const Element& e : m_elementsStore->elements())
            if (e.type == QStringLiteral("character")) charByName.insert(e.name.toLower(), e);

    auto placeName = [&](const QString& id) -> QString {
        if (id.isEmpty() || !m_territorioStore) return {};
        if (const auto* t = m_territorioStore->territorio(id)) return t->name;
        return {};
    };

    // ── eventos de capítulo/cena ──────────────────────────────────────────────
    const QList<TimelineEvent> live = m_scene->allEventData();
    QHash<QString, const TimelineEvent*> liveById;
    for (const auto& e : live) liveById.insert(e.id, &e);
    QHash<QString, int> presence;  // charId → nº de colunas
    QString prevLane = QStringLiteral("story:main");
    for (int i = 0; i < d.cols.size(); ++i) {
        const QString id = QStringLiteral("story:") + d.cols[i].key;
        Tracks::Event ev;
        ev.id = id;
        ev.col = i;
        const UnitInfo& u = units[i];
        ev.title = u.title;
        if (const TimelineEvent* le = liveById.value(id, nullptr)) {
            ev.laneId = le->timelineId;
            ev.marker = le->timeMarker;
            ev.summary = le->description.isEmpty() ? u.summary : le->description;
            ev.placeId = le->placeId;
            ev.autoLaneId = m_autoLaneOf.value(id, le->timelineId);
        } else {
            ev.hollow = true;
            ev.summary = u.summary;
            ev.autoLaneId = prevLane;
            ev.laneId = m_laneOverrides.value(id, prevLane);
        }
        ev.moved = m_laneOverrides.contains(id) && m_laneOverrides.value(id) != ev.autoLaneId;
        ev.placeName = placeName(ev.placeId);
        ev.chrono = TimelineChrono::parse(ev.marker, &ev.chronoOk);
        if (ev.hollow) ev.chronoOk = false;
        for (const QString& name : m_presentByEvId.value(id)) {
            const auto it = charByName.constFind(name.toLower());
            if (it == charByName.constEnd()) continue;
            if (!ev.castIds.contains(it->id)) { ev.castIds << it->id; presence[it->id] += 1; }
        }
        prevLane = ev.laneId;
        d.events << ev;
    }

    // ── eventos criados à mão ─────────────────────────────────────────────────
    for (const auto& le : live) {
        if (le.id.startsWith(QStringLiteral("story:")) || le.id.startsWith(QStringLiteral("auto:")) || le.autoEvent)
            continue;
        QString key = le.anchorKey;
        if (key.isEmpty() && !le.linkedSceneId.isEmpty()) key = le.linkedSceneId;
        if (key.isEmpty() && le.linkedDocId.startsWith(QStringLiteral("ch:"))) {
            const QStringList parts = le.linkedDocId.split(QLatin1Char(':'));
            if (parts.size() >= 3) key = parts.last() + QStringLiteral(":0");
        }
        int col = -1;
        if (!key.isEmpty()) {
            col = colOfKey.value(key, -1);
            if (col < 0) continue; // pertence a outro manuscrito
        }
        Tracks::Event ev;
        ev.id = le.id;
        ev.manual = true;
        ev.col = col;
        ev.order = le.anchorPos;
        ev.title = le.title.isEmpty() ? tr("Evento") : le.title;
        ev.marker = le.timeMarker;
        ev.summary = le.description;
        ev.placeId = le.placeId;
        ev.placeName = placeName(le.placeId);
        ev.chrono = TimelineChrono::parse(le.timeMarker, &ev.chronoOk);
        ev.origin = le.origin;
        if (le.origin == QLatin1String("editor"))
            ev.originWhere = le.originWhere.isEmpty() ? tr("na seleção do editor")
                                                      : tr("na seleção do editor · %1").arg(le.originWhere);
        else if (le.origin == QLatin1String("timeline")) ev.originWhere = tr("aqui na Timeline");
        else if (le.origin == QLatin1String("lousa"))    ev.originWhere = tr("na Lousa");
        else if (le.origin == QLatin1String("new"))      ev.originWhere = tr("pelo botão + Evento");
        ev.originShort = le.origin == QLatin1String("editor") ? tr("Editor")
                       : le.origin == QLatin1String("timeline") ? tr("Timeline")
                       : le.origin == QLatin1String("lousa") ? tr("Lousa")
                       : le.origin == QLatin1String("new") ? tr("+ Evento") : tr("Evento");
        if (col >= 0) {
            const QString colLane = d.events[col].laneId;
            ev.laneId = (le.laneFollows || le.timelineId.isEmpty()) ? colLane : le.timelineId;
        }
        d.events << ev;
    }

    // ── linhas ────────────────────────────────────────────────────────────────
    QSet<QString> used;
    for (const auto& e : d.events) if (e.col >= 0) used.insert(e.laneId);
    const QList<QColor> branchPalette = { pal.warning, pal.success, pal.danger };
    int branchIdx = 0;
    QList<TimelineDef> defs = m_timelines;
    std::sort(defs.begin(), defs.end(), [](const TimelineDef& a, const TimelineDef& b) {
        auto grp = [](const TimelineDef& t) {
            if (t.kind == QLatin1String(TimelineKind::Main)) return 0;
            if (t.kind == QLatin1String(TimelineKind::Parallel)) return 1;
            if (t.kind == QLatin1String(TimelineKind::Backstory)) return 2;
            return 3;
        };
        if (grp(a) != grp(b)) return grp(a) < grp(b);
        return a.railOrder < b.railOrder;
    });
    const QColor defaultColor(QStringLiteral("#6c8ebf"));
    QSet<QString> have;
    for (const TimelineDef& t : defs) {
        if (t.kind == QLatin1String(TimelineKind::Character)) continue;
        if (t.autoGenerated && !used.contains(t.id)) continue;
        Tracks::Lane L;
        L.id = t.id;
        L.name = t.name.isEmpty() ? tr("Linha") : t.name;
        L.kind = t.kind;
        L.parentId = t.parentId;
        L.dashed = weightIsDashed(t.weight);
        L.autoGenerated = t.autoGenerated;
        L.color = t.color.isValid() ? t.color : defaultColor;
        if (t.userColor) {
            if (!t.colorTheme.isEmpty()) L.color = ColorPick::themeColor(t.colorTheme);
            if (t.autoGenerated && t.kind == QLatin1String(TimelineKind::Parallel)) { L.sub = tr("automática"); ++branchIdx; }
            if (t.id == QLatin1String("story:main")) L.tip = tr("linha principal");
            else if (!t.autoGenerated) L.tip = tr("linha criada por você");
        } else if (t.id == QLatin1String("story:main") && L.color == defaultColor) {
            L.color = pal.accent;
            L.tip = tr("linha principal");
        } else if (t.id == QLatin1String("story:flashback") && L.color == defaultColor) {
            L.color = pal.info;
            L.tip = d.startMarker.isEmpty() ? tr("antes do início da história")
                                            : tr("antes de %1").arg(d.startMarker);
        } else if (t.autoGenerated && t.kind == QLatin1String(TimelineKind::Parallel)) {
            L.color = branchIdx < branchPalette.size() ? branchPalette[branchIdx] : colorForIdNew(t.id);
            ++branchIdx;
            L.sub = tr("automática");
            const int fromCol = colOfKey.value(t.branchFromEventId.mid(6), -1);
            L.tip = fromCol >= 0 ? tr("automática · nasceu no Cap %1").arg(d.cols[fromCol].ruler)
                                 : tr("ramificação automática");
        } else if (!t.autoGenerated) {
            L.tip = tr("linha criada por você");
        }
        d.lanes << L;
        have.insert(t.id);
    }
    if (!have.contains(QStringLiteral("story:main")) && used.contains(QStringLiteral("story:main"))) {
        Tracks::Lane L;
        L.id = QStringLiteral("story:main");
        L.name = tr("Narrativa");
        L.kind = QString::fromLatin1(TimelineKind::Main);
        L.color = pal.accent;
        L.tip = tr("linha principal");
        L.autoGenerated = true;
        d.lanes.prepend(L);
    }
    // evento apontando pra linha que não existe mais → cai na Narrativa
    for (auto& e : d.events)
        if (e.col >= 0 && d.laneIndex(e.laneId) < 0)
            e.laneId = d.lanes.isEmpty() ? QString() : d.lanes.first().id;

    // ── elenco (quem aparece, do mais presente ao menos) ──────────────────────
    QList<Element> cast;
    for (auto it = presence.constBegin(); it != presence.constEnd(); ++it)
        for (const Element& e : charByName) if (e.id == it.key()) { cast << e; break; }
    std::sort(cast.begin(), cast.end(), [&](const Element& a, const Element& b) {
        const int pa = presence.value(a.id), pb = presence.value(b.id);
        if (pa != pb) return pa > pb;
        return a.name.localeAwareCompare(b.name) < 0;
    });
    // Cor com sentido: narrador (ou o mais presente) na cor de destaque,
    // antagonista na cor de perigo, o resto na paleta do tema pela ordem.
    const bool hasNarrator = std::any_of(cast.begin(), cast.end(), [](const Element& e) { return e.narrator; });
    auto isAntagonist = [](const Element& e) { return e.role.contains(QStringLiteral("ANTAG"), Qt::CaseInsensitive); };
    const bool hasAntagonist = std::any_of(cast.begin(), cast.end(), isAntagonist);
    QList<QColor> castPalette = { pal.warning, pal.info, pal.success };
    if (!hasAntagonist) castPalette << pal.danger;
    int ci = 0;
    bool accentUsed = false, dangerUsed = false;
    for (int i = 0; i < cast.size(); ++i) {
        const Element& e = cast[i];
        Tracks::Character c;
        c.id = e.id;
        c.name = e.name.isEmpty() ? tr("(sem nome)") : e.name;
        if (!accentUsed && (e.narrator || (!hasNarrator && i == 0))) { c.color = pal.accent; accentUsed = true; }
        else if (!dangerUsed && isAntagonist(e)) { c.color = pal.danger; dangerUsed = true; }
        else c.color = ci < castPalette.size() ? castPalette[ci++] : colorForIdNew(e.id);
        if (!e.image.isEmpty()) {
            const QString cacheKey = e.id + QLatin1Char(':') + QString::number(e.image.size());
            auto it = m_avatarCache.constFind(cacheKey);
            if (it == m_avatarCache.constEnd()) {
                const int comma = e.image.indexOf(QLatin1Char(','));
                QImage img = QImage::fromData(QByteArray::fromBase64(e.image.mid(comma + 1).toLatin1()));
                if (!img.isNull()) img = img.scaled(64, 64, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                it = m_avatarCache.insert(cacheKey, img);
            }
            c.avatar = it.value();
        }
        d.chars << c;
    }

    // ── capítulo aberto no editor ─────────────────────────────────────────────
    if (!m_editorChapterId.isEmpty())
        for (int i = 0; i < d.cols.size(); ++i) {
            const auto& col = d.cols[i];
            if (col.chapterId != m_editorChapterId) continue;
            if (col.sceneIndex < 0 || m_editorScene < 0 || col.sceneIndex == m_editorScene) { d.editorCol = i; break; }
        }
    return d;
}

void TimelinePanel::refreshNewUi()
{
    if (!m_tracks || m_legacyUi) return;
    m_tracksData = buildTracksData();
    if (!m_sel.isEmpty() && !m_tracksData.event(m_sel)) m_sel.clear();
    m_tracks->setData(m_tracksData);
    m_braid->setData(m_tracksData);
    applyTracksFilter();

    // rodapé
    QStringList parts;
    const int nc = m_tracksData.chapterCount, nl = m_tracksData.lanes.size(), np = m_tracksData.chars.size();
    parts << (nc == 1 ? tr("1 capítulo") : tr("%1 capítulos").arg(nc));
    parts << (nl == 1 ? tr("1 linha") : tr("%1 linhas").arg(nl));
    parts << (np == 1 ? tr("1 personagem") : tr("%1 personagens").arg(np));
    m_statsLbl->setText(parts.join(QStringLiteral(" · ")));
    int nd = 0;
    for (const auto& e : m_tracksData.events) if (e.hollow) ++nd;
    m_noDateBtn->setText(nd == 0 ? tr("todos os capítulos têm data")
                       : nd == 1 ? tr("1 capítulo sem data") : tr("%1 capítulos sem data").arg(nd));
    m_noDateBtn->setEnabled(nd > 0);
    m_noDateBtn->setVisible(!m_tracksData.cols.isEmpty());
    m_noDateBtn->setProperty("allDated", nd == 0);
    m_noDateBtn->style()->unpolish(m_noDateBtn);
    m_noDateBtn->style()->polish(m_noDateBtn);
    m_msBtn->setText(m_tracksData.manuscriptTitle);
    m_msBtn->setVisible(!m_tracksData.manuscriptTitle.isEmpty());

    if (!m_sel.isEmpty() && m_newMode != NewMode::Engine) m_inspector->showFor(m_tracksData, m_sel);
    else setInspectorOpen(false);
}

void TimelinePanel::applyTracksFilter()
{
    if (!m_tracks) return;
    if (!m_tf.charId.isEmpty() && !m_tracksData.character(m_tf.charId)) m_tf.charId.clear();
    if (!m_tf.laneId.isEmpty() && !m_tracksData.lane(m_tf.laneId)) m_tf.laneId.clear();
    m_tracks->setFilter(m_tf);
    m_braid->setFilter(m_tf);
    m_tracks->setSelected(m_sel);
    m_braid->setSelected(m_sel);
    refreshNewChips();
}

void TimelinePanel::refreshNewChips()
{
    if (!m_chipChar) return;
    const Tracks::Palette pal = Tracks::Palette::current();
    auto set = [&](QToolButton* chip, const QString& label, const QString& value) {
        auto* fc = static_cast<FilterChip*>(chip);
        fc->active = !value.isEmpty();
        chip->setText(value.isEmpty() ? label : QStringLiteral("%1: %2  ×").arg(label, value));
        chip->setIcon(value.isEmpty() ? QIcon(tlIcon(QStringLiteral("chevron"), pal.ink)) : QIcon());
        chip->setProperty("active", fc->active);
        chip->style()->unpolish(chip);
        chip->style()->polish(chip);
    };
    QString cn;
    if (const auto* c = m_tracksData.character(m_tf.charId)) cn = c->name;
    QString pn;
    if (!m_tf.placeId.isEmpty() && m_territorioStore)
        if (const auto* t = m_territorioStore->territorio(m_tf.placeId)) pn = t->name;
    set(m_chipChar, tr("Personagem"), cn);
    set(m_chipPlace, tr("Lugar"), pn);
}

void TimelinePanel::rebuildNewMenus()
{
    const Tracks::Palette pal = Tracks::Palette::current();
    // manuscritos
    if (QMenu* m = m_msBtn->menu()) {
        m->clear();
        const QString cur = currentManuscriptId();
        for (const Manuscript& ms : m_projectModel ? m_projectModel->manuscripts() : QList<Manuscript>()) {
            QAction* a = m->addAction(ms.title.isEmpty() ? tr("Sem título") : ms.title);
            a->setCheckable(true);
            a->setChecked(ms.id == cur);
            const QString id = ms.id;
            connect(a, &QAction::triggered, this, [this, id]() {
                m_msId = id;
                m_sel.clear();
                refreshNewUi();
                save();
            });
        }
    }
    // personagem
    if (QMenu* m = m_chipChar->menu()) {
        m->clear();
        QAction* all = m->addAction(tr("Todos"));
        all->setCheckable(true);
        all->setChecked(m_tf.charId.isEmpty());
        connect(all, &QAction::triggered, this, [this]() { m_tf.charId.clear(); applyTracksFilter(); });
        for (const auto& c : m_tracksData.chars) {
            QPixmap px(QSize(15, 15) * 2);
            px.setDevicePixelRatio(2);
            px.fill(Qt::transparent);
            { QPainter p(&px); Tracks::drawAvatar(&p, QRectF(0, 0, 15, 15), c, pal.app); }
            QAction* a = m->addAction(QIcon(px), c.name);
            a->setCheckable(true);
            a->setChecked(c.id == m_tf.charId);
            const QString id = c.id;
            connect(a, &QAction::triggered, this, [this, id]() { m_tf.charId = id; applyTracksFilter(); });
        }
    }
    // lugar
    if (QMenu* m = m_chipPlace->menu()) {
        m->clear();
        QAction* all = m->addAction(tr("Todos"));
        all->setCheckable(true);
        all->setChecked(m_tf.placeId.isEmpty());
        connect(all, &QAction::triggered, this, [this]() { m_tf.placeId.clear(); applyTracksFilter(); });
        QList<QPair<QString, QString>> places;
        for (const auto& e : m_tracksData.events)
            if (!e.placeId.isEmpty() && !e.placeName.isEmpty()) {
                const QPair<QString, QString> pr{ e.placeName, e.placeId };
                if (!places.contains(pr)) places << pr;
            }
        std::sort(places.begin(), places.end(), [](const auto& a, const auto& b) { return a.first.localeAwareCompare(b.first) < 0; });
        if (places.isEmpty()) {
            QAction* none = m->addAction(tr("Nenhum evento tem lugar ainda"));
            none->setEnabled(false);
        }
        for (const auto& pr : places) {
            QAction* a = m->addAction(pr.first);
            a->setCheckable(true);
            a->setChecked(pr.second == m_tf.placeId);
            const QString id = pr.second;
            connect(a, &QAction::triggered, this, [this, id]() { m_tf.placeId = id; applyTracksFilter(); });
        }
    }
    // "⋯"
    if (QMenu* m = m_moreBtn->menu()) {
        m->clear();
        QAction* cap = m->addAction(tr("Outros modos (motor atual)"));
        cap->setEnabled(false);
        const bool engine = m_newMode == NewMode::Engine;
        QAction* branches = m->addAction(tr("Ramificações"));
        branches->setCheckable(true);
        branches->setChecked(engine && m_scene->viewMode() == TimelineScene::ViewMode::Constellation);
        connect(branches, &QAction::triggered, this, [this]() {
            m_scene->setViewMode(TimelineScene::ViewMode::Constellation);
            setNewMode(NewMode::Engine);
            if (m_view) m_view->fitAll();
        });
        QAction* spiral = m->addAction(tr("Espiral"));
        spiral->setCheckable(true);
        spiral->setChecked(engine && m_scene->viewMode() == TimelineScene::ViewMode::Spiral);
        connect(spiral, &QAction::triggered, this, [this]() {
            m_scene->setViewMode(TimelineScene::ViewMode::Spiral);
            setNewMode(NewMode::Engine);
            if (m_view) m_view->fitAll();
        });
        m->addSeparator();
        connect(m->addAction(tr("Gerador de Timeline")), &QAction::triggered, this, &TimelinePanel::generatorRequested);
        connect(m->addAction(tr("Nova linha manual")), &QAction::triggered, this, &TimelinePanel::createTimeline);
        m->addSeparator();
        connect(m->addAction(tr("UI Legado")), &QAction::triggered, this, [this]() { setLegacyUi(true); });
    }
    applyNewTheme();
}

void TimelinePanel::setNewMode(NewMode m)
{
    m_newMode = m;
    if (!m_stack) return;
    m_stack->setCurrentWidget(m == NewMode::Tracks ? static_cast<QWidget*>(m_tracks)
                            : m == NewMode::Braid  ? static_cast<QWidget*>(m_braid)
                                                   : static_cast<QWidget*>(m_view));
    m_segTracks->setChecked(m == NewMode::Tracks);
    m_segBraid->setChecked(m == NewMode::Braid);
    m_densityBox->setVisible(m == NewMode::Tracks);
    if (m == NewMode::Engine) {
        setInspectorOpen(false);
        m_scene->relayout();
    } else if (!m_sel.isEmpty()) {
        m_inspector->showFor(m_tracksData, m_sel);
        setInspectorOpen(true);
    }
    applyNewTheme();
    save();
}

void TimelinePanel::setLegacyUi(bool legacy)
{
    m_legacyUi = legacy;
    QSettings().setValue(QStringLiteral("timeline/legacyUi"), legacy);
    if (!m_stack) return;
    m_toolbar->setVisible(legacy);
    if (m_btnNewUi) m_btnNewUi->setVisible(legacy);
    m_newTop->setVisible(!legacy);
    m_newBottom->setVisible(!legacy);
    m_inspHolder->setVisible(!legacy);
    if (m_btnAdd) m_btnAdd->setVisible(legacy);
    if (legacy) {
        m_stack->setCurrentWidget(m_view);
        refreshModeButtons();
        m_scene->relayout();
    } else {
        refreshNewUi();
        setNewMode(m_newMode);
    }
}

void TimelinePanel::selectEvent(const QString& id)
{
    m_sel = id;
    if (!m_tracks) return;
    m_tracks->setSelected(id);
    m_braid->setSelected(id);
    if (id.isEmpty() || m_newMode == NewMode::Engine) { setInspectorOpen(false); return; }
    m_inspector->showFor(m_tracksData, id);
    setInspectorOpen(true);
}

void TimelinePanel::setInspectorOpen(bool open)
{
    if (!m_inspHolder) return;
    const int target = open ? m_inspector->width() : 0;
    if (m_inspHolder->maximumWidth() == target && m_inspAnim->state() != QAbstractAnimation::Running) return;
    m_inspAnim->stop();
    m_inspAnim->setStartValue(m_inspHolder->maximumWidth());
    m_inspAnim->setEndValue(target);
    m_inspAnim->start();
}

void TimelinePanel::collapseSearch(bool clear)
{
    if (!m_searchEdit) return;
    if (clear) m_searchEdit->clear();
    if (!m_searchEdit->text().isEmpty()) return;
    m_searchEdit->setFixedWidth(0);
    m_searchBox->setProperty("open", false);
    m_searchBox->style()->unpolish(m_searchBox);
    m_searchBox->style()->polish(m_searchBox);
    if (m_tracks) m_tracks->setFocus();
}

bool TimelinePanel::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_inspHolder && m_inspector && ev->type() == QEvent::Resize) {
        m_inspector->setGeometry(m_inspHolder->width() - m_inspector->width(), 0,
                                 m_inspector->width(), m_inspHolder->height());
    } else if (obj == m_searchEdit && ev->type() == QEvent::FocusOut) {
        QTimer::singleShot(120, this, [this]() { collapseSearch(false); });
    }
    return QWidget::eventFilter(obj, ev);
}

void TimelinePanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && !m_legacyUi) {
        if (m_searchEdit && m_searchEdit->hasFocus()) { collapseSearch(true); return; }
        if (!m_sel.isEmpty()) { selectEvent(QString()); return; }
    }
    QWidget::keyPressEvent(event);
}

const TimelineEvent* TimelinePanel::liveEvent(const QString& id) const
{
    if (!m_scene) return nullptr;
    if (auto* item = m_scene->findEvent(id)) return &item->eventData();
    return nullptr;
}

void TimelinePanel::updateLiveEvent(const TimelineEvent& e)
{
    auto* item = m_scene->findEvent(e.id);
    if (!item) return;
    item->setEventData(e);
    item->setTimelineColor(m_scene->timelineColor(e.timelineId));
    for (auto& le : m_events) if (le.id == e.id) { le = e; break; }
    m_scene->relayout();
    save();
    refreshNewUi();
}

void TimelinePanel::onEventDropped(const QString& id, const QString& laneId, int col)
{
    const Tracks::Event* te = m_tracksData.event(id);
    if (!te) return;
    m_sel = id;
    if (te->manual) {
        const TimelineEvent* le = liveEvent(id);
        if (!le) return;
        TimelineEvent e = *le;
        if (laneId.isEmpty()) {
            e.anchorKey.clear();
        } else {
            e.anchorKey = m_tracksData.cols.value(col).key;
            e.timelineId = laneId;
            e.laneFollows = false;
        }
        updateLiveEvent(e);
        return;
    }
    // capítulo/cena: só troca de linha; a escolha vira override persistido
    if (laneId.isEmpty() || laneId == te->laneId) return;
    if (laneId == te->autoLaneId) m_laneOverrides.remove(id);
    else m_laneOverrides.insert(id, laneId);
    save();
    syncStoryTimeline(); // reaplica o override e redesenha
}

void TimelinePanel::undoLaneMove(const QString& id)
{
    m_laneOverrides.remove(id);
    save();
    syncStoryTimeline();
}

void TimelinePanel::openEventInEditor(const QString& id)
{
    const Tracks::Event* te = m_tracksData.event(id);
    if (!te || te->col < 0 || te->col >= m_tracksData.cols.size()) return;
    const Tracks::Column& c = m_tracksData.cols[te->col];
    int pos = -1;
    if (te->manual && te->origin == QLatin1String("editor"))
        if (const TimelineEvent* le = liveEvent(id)) pos = le->anchorPos;
    emit openInEditorRequested(c.manuscriptId, c.chapterId, c.sceneIndex, pos);
}

void TimelinePanel::fillMarker(const QString& id, const QString& marker)
{
    if (!m_projectModel || !id.startsWith(QStringLiteral("story:"))) return;
    const QString key = id.mid(6);
    for (const auto& c : m_tracksData.cols) {
        if (c.key != key) continue;
        m_sel = id;
        if (c.sceneIndex < 0) m_projectModel->updateChapterTimeMarker(c.chapterId, marker);
        else m_projectModel->updateSceneTimeMarker(c.chapterId, c.sceneIndex, marker);
        if (!isVisible()) refreshFromModel();
        return;
    }
}

void TimelinePanel::editTracksEvent(const QString& id)
{
    const Tracks::Event* te = m_tracksData.event(id);
    if (!te) return;
    if (te->manual) { openEditPopup(id); refreshNewUi(); return; }

    // Evento de capítulo/cena: título, marcador e resumo moram no capítulo —
    // gravar lá, senão a próxima sincronização desfaria a edição.
    const Tracks::Column c = m_tracksData.cols.value(te->col);
    TimelineEventPopup dlg(m_timelines, m_projectModel, m_territorioStore, this);
    dlg.setDocTextResolver(m_docTextResolver);
    TimelineEvent seed;
    if (const TimelineEvent* le = liveEvent(id)) seed = *le;
    seed.id = id;
    seed.title = te->title;
    seed.timeMarker = te->marker;
    seed.description = te->summary;
    seed.timelineId = te->laneId;
    dlg.setEventData(seed);
    if (dlg.exec() != QDialog::Accepted) return;
    const TimelineEvent out = dlg.eventData();
    m_sel = id;
    if (!out.timelineId.isEmpty() && out.timelineId != te->laneId) {
        if (out.timelineId == te->autoLaneId) m_laneOverrides.remove(id);
        else m_laneOverrides.insert(id, out.timelineId);
    }
    if (const TimelineEvent* le = liveEvent(id); le && le->placeId != out.placeId) {
        TimelineEvent e = *le;
        e.placeId = out.placeId;
        auto* item = m_scene->findEvent(id);
        if (item) item->setEventData(e);
    }
    m_projectModel->beginBatchUpdate();
    if (c.sceneIndex < 0) {
        if (out.title != te->title && !out.title.isEmpty()) m_projectModel->updateChapterTitle(c.chapterId, out.title);
        m_projectModel->updateChapterTimeMarker(c.chapterId, out.timeMarker);
        m_projectModel->updateChapterSummary(c.chapterId, out.description);
    } else {
        if (out.title != te->title && !out.title.isEmpty()) m_projectModel->updateSceneTitle(c.chapterId, c.sceneIndex, out.title);
        m_projectModel->updateSceneTimeMarker(c.chapterId, c.sceneIndex, out.timeMarker);
        m_projectModel->updateSceneSummary(c.chapterId, c.sceneIndex, out.description);
    }
    m_projectModel->endBatchUpdate();
    save();
    refreshFromModel();
}

void TimelinePanel::createAtColumn(const QString& laneId, int col)
{
    if (col < 0 || col >= m_tracksData.cols.size()) return;
    TimelineEventPopup dlg(m_timelines, m_projectModel, m_territorioStore, this);
    dlg.setDocTextResolver(m_docTextResolver);
    TimelineEvent seed;
    seed.timelineId = laneId;
    dlg.setEventData(seed);
    if (dlg.exec() != QDialog::Accepted) return;
    TimelineEvent e = dlg.eventData();
    if (e.timelineId.isEmpty()) e.timelineId = laneId;
    e.anchorKey = m_tracksData.cols[col].key;
    e.origin = QStringLiteral("timeline");
    e.laneFollows = false;
    const QString id = commitEvent(e, QPointF());
    selectEvent(id);
}

void TimelinePanel::removeManualEvent(const QString& id)
{
    const TimelineEvent* le = liveEvent(id);
    if (!le) return;
    if (QMessageBox::question(this, tr("Remover evento"),
                              tr("Remover \"%1\" da linha do tempo?").arg(le->title),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    m_scene->removeEvent(id);
    m_events.erase(std::remove_if(m_events.begin(), m_events.end(),
                                  [&](const TimelineEvent& e) { return e.id == id; }), m_events.end());
    m_connections.erase(std::remove_if(m_connections.begin(), m_connections.end(),
        [&](const TimelineConn& c) { return c.fromEventId == id || c.toEventId == id; }), m_connections.end());
    if (m_sel == id) m_sel.clear();
    save();
    refreshNewUi();
}

void TimelinePanel::setEditorLocation(const QString& manuscriptId, const QString& chapterId, int sceneIndex)
{
    if (m_editorMsId == manuscriptId && m_editorChapterId == chapterId && m_editorScene == sceneIndex) return;
    m_editorMsId = manuscriptId;
    m_editorChapterId = chapterId;
    m_editorScene = sceneIndex;
    if (isVisible()) refreshNewUi();
}

void TimelinePanel::promptNewEventFromEditor(const QString& description, const QString& marker,
                                             const QString& chapterId, int sceneIndex,
                                             int textPos, int paragraph)
{
    const Chapter* ch = m_projectModel ? m_projectModel->findChapter(chapterId) : nullptr;
    if (!ch) { promptNewEvent(description, marker, QString(), QStringLiteral("editor")); return; }
    const int si = ch->scenes.isEmpty() ? -1 : qMax(0, sceneIndex);
    const QString key = chapterId + QLatin1Char(':') + QString::number(si < 0 ? 0 : si);
    const QString storyId = QStringLiteral("story:") + key;
    QString seedLane = m_laneOverrides.value(storyId);
    if (seedLane.isEmpty())
        if (const TimelineEvent* le = liveEvent(storyId)) seedLane = le->timelineId;
    if (seedLane.isEmpty()) seedLane = QStringLiteral("story:main");

    TimelineEventPopup dlg(m_timelines, m_projectModel, m_territorioStore, this);
    dlg.setDocTextResolver(m_docTextResolver);
    TimelineEvent seed;
    seed.title = suggestTitleFrom(description);
    seed.timeMarker = marker;
    seed.description = description;
    seed.timelineId = seedLane;
    dlg.setEventData(seed);
    if (dlg.exec() != QDialog::Accepted) return;
    TimelineEvent e = dlg.eventData();
    e.anchorKey = key;
    e.anchorPos = textPos;
    e.origin = QStringLiteral("editor");
    e.originWhere = tr("Cap %1, parágrafo %2").arg(unitRuler(m_projectModel, *ch, si)).arg(paragraph);
    e.laneFollows = e.timelineId.isEmpty() || e.timelineId == seedLane;
    if (!ch->manuscriptId.isEmpty() && ch->manuscriptId != currentManuscriptId()) m_msId = ch->manuscriptId;
    const QString id = commitEvent(e, m_view ? m_view->mapToScene(m_view->viewport()->rect().center()) : QPointF());
    if (!m_legacyUi) {
        if (m_newMode == NewMode::Engine) setNewMode(NewMode::Tracks);
        selectEvent(id);
    }
}

void TimelinePanel::resolveThemeBoundLaneColors()
{
    bool changed = false;
    for (auto& t : m_timelines) {
        if (!t.userColor || t.colorTheme.isEmpty()) continue;
        const QColor c = ColorPick::themeColor(t.colorTheme);
        if (c.isValid() && c != t.color) { t.color = c; changed = true; }
    }
    if (changed && m_scene) m_scene->setTimelines(m_timelines);
}

void TimelinePanel::openLaneColor(const QString& laneId, const QPoint& globalPos)
{
    const Tracks::Lane* L = m_tracksData.lane(laneId);
    int defIdx = -1;
    for (int i = 0; i < m_timelines.size(); ++i) if (m_timelines[i].id == laneId) defIdx = i;
    if (!L) return;

    ColorPick::Choice current{ L->color, QString() };
    if (defIdx >= 0 && m_timelines[defIdx].userColor) current.themeKey = m_timelines[defIdx].colorTheme;
    else
        for (const QString& k : ColorPick::themeKeys())
            if (ColorPick::themeColor(k) == L->color) { current.themeKey = k; break; }

    ColorPopover::Options opt;
    opt.title = tr("Cor da linha · %1").arg(L->name);
    opt.themeBinding = true;
    opt.resetAvailable = defIdx >= 0 && m_timelines[defIdx].userColor && m_timelines[defIdx].autoGenerated;
    auto* pop = new ColorPopover(current, opt, this);

    // prévia ao vivo: repinta Trilhos/Trança com a cor em teste, sem salvar
    connect(pop, &ColorPopover::previewed, this, [this, laneId](const ColorPick::Choice& c) {
        Tracks::Data d = m_tracksData;
        for (auto& l : d.lanes) if (l.id == laneId) l.color = c.color;
        m_tracks->setData(d);
        m_braid->setData(d);
    });
    connect(pop, &ColorPopover::finished, this,
            [this, laneId, defIdx](const ColorPick::Choice& c, ColorPopover::Result r) {
        if (r == ColorPopover::Cancelled || defIdx < 0 || defIdx >= m_timelines.size()
            || m_timelines[defIdx].id != laneId) {
            refreshNewUi();
            return;
        }
        TimelineDef& t = m_timelines[defIdx];
        if (r == ColorPopover::Reset) {
            t.userColor = false;
            t.colorTheme.clear();
            t.color = QColor(QStringLiteral("#6c8ebf"));
        } else {
            t.userColor = true;
            t.colorTheme = c.themeKey;
            t.color = c.color;
        }
        m_scene->setTimelines(m_timelines);
        save();
        refreshNewUi();
    });
    pop->popupAt(globalPos);
}
