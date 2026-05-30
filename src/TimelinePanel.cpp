#include "TimelinePanel.h"

#include "IconUtils.h"
#include "ProjectModel.h"
#include "Theme.h"
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
#include <QPainter>
#include <QPushButton>
#include <QPixmap>
#include <QResizeEvent>
#include <QSaveFile>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

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
    auto* btnNewEvent    = makeBtn(tr("+ Evento"),   tr("Novo evento"),         m_toolbar);
    tl->addWidget(btnNewTimeline);
    tl->addWidget(btnNewEvent);

    tl->addStretch();

    auto* btnClose = makeBtn(QStringLiteral("×"), tr("Fechar"), m_toolbar);
    tl->addWidget(btnClose);

    connect(btnNewTimeline, &QToolButton::clicked, this, &TimelinePanel::createTimeline);
    connect(btnNewEvent,    &QToolButton::clicked, this, [this]() {
        createEventAt(QPointF(0, 0));
    });
    connect(btnClose, &QToolButton::clicked, this, &TimelinePanel::closeRequested);

    root->addWidget(m_toolbar);

    // ── Canvas ───────────────────────────────────────────────────────────────
    m_scene = new TimelineScene(this);
    m_view  = new TimelineView(m_scene, this);
    root->addWidget(m_view, 1);

    connect(m_scene, &TimelineScene::eventDataChanged, this, [this]() { save(); });
    connect(m_scene, &TimelineScene::eventEditRequested, this, &TimelinePanel::openEditPopup);
    connect(m_scene, &TimelineScene::canvasDoubleClicked, this, &TimelinePanel::createEventAt);
}

void TimelinePanel::createTimeline()
{
    // Popup rápido: nome + cor
    auto* dlg = new QDialog(this, Qt::Dialog);
    dlg->setWindowTitle(tr("Nova linha do tempo"));
    dlg->setMinimumWidth(300);

    auto* layout = new QVBoxLayout(dlg);
    layout->setSpacing(10);
    layout->setContentsMargins(16, 16, 16, 16);

    auto* form = new QFormLayout;
    auto* nameEdit = new QLineEdit(dlg);
    nameEdit->setPlaceholderText(tr("Nome da timeline"));
    form->addRow(tr("Nome:"), nameEdit);
    layout->addLayout(form);

    // Seletor de cor
    QColor chosenColor = QColor(QStringLiteral("#6c8ebf"));
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
    btns->button(QDialogButtonBox::Ok)->setText(tr("Criar"));
    btns->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(btns, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    layout->addWidget(btns);

    dlg->setStyleSheet(styleSheet());

    if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return; }

    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) { dlg->deleteLater(); return; }

    TimelineDef t;
    t.id    = QUuid::createUuid().toString(QUuid::WithoutBraces);
    t.name  = name;
    t.color = chosenColor;
    m_timelines.append(t);
    m_scene->setTimelines(m_timelines);
    save();
    dlg->deleteLater();
}

void TimelinePanel::createEventAt(const QPointF& scenePos)
{
    TimelineEventPopup dlg(m_timelines, this);
    if (dlg.exec() != QDialog::Accepted) return;

    TimelineEvent e = dlg.eventData();
    if (e.title.isEmpty()) e.title = tr("Novo evento");
    e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    e.x  = scenePos.x() - TimelineEventItem::kW / 2.0;
    e.y  = scenePos.y() - TimelineEventItem::kH / 2.0;

    // se não tiver timeline selecionada mas existir alguma, usa a primeira
    if (e.timelineId.isEmpty() && !m_timelines.isEmpty())
        e.timelineId = m_timelines.first().id;

    m_events.append(e);
    m_scene->addEvent(e);

    // Conexão automática com o evento anterior da mesma timeline
    if (!e.timelineId.isEmpty()) {
        const TimelineConn conn = m_scene->autoConnect(e.id, e.timelineId);
        if (!conn.id.isEmpty())
            m_connections.append(conn);
    }

    save();
}

void TimelinePanel::openEditPopup(const QString& id)
{
    auto* item = m_scene->findEvent(id);
    if (!item) return;

    TimelineEventPopup dlg(m_timelines, this);
    dlg.setEventData(item->eventData());
    if (dlg.exec() != QDialog::Accepted) return;

    TimelineEvent updated = dlg.eventData();
    updated.id = id;
    updated.x  = item->eventData().x;
    updated.y  = item->eventData().y;

    // Atualiza no item gráfico
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

    QJsonArray tls;
    for (const auto& t : m_timelines) {
        QJsonObject o;
        o[QStringLiteral("id")]    = t.id;
        o[QStringLiteral("name")]  = t.name;
        o[QStringLiteral("color")] = t.color.name();
        tls.append(o);
    }
    root[QStringLiteral("timelines")] = tls;

    QJsonArray evs;
    // Usa dados ao vivo da cena (têm posição atualizada)
    for (const auto& e : m_scene ? m_scene->allEventData() : m_events) {
        QJsonObject o;
        o[QStringLiteral("id")]            = e.id;
        o[QStringLiteral("timelineId")]    = e.timelineId;
        o[QStringLiteral("title")]         = e.title;
        o[QStringLiteral("description")]   = e.description;
        o[QStringLiteral("shape")]         = e.shape;
        o[QStringLiteral("color")]         = e.color.isValid() ? e.color.name() : QString();
        o[QStringLiteral("x")]             = e.x;
        o[QStringLiteral("y")]             = e.y;
        o[QStringLiteral("timeMarker")]    = e.timeMarker;
        o[QStringLiteral("linkedSceneId")] = e.linkedSceneId;
        o[QStringLiteral("linkedDocId")]   = e.linkedDocId;
        o[QStringLiteral("conclusion")]    = e.conclusion;
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

    m_timelines.clear();
    for (const auto& v : root[QStringLiteral("timelines")].toArray()) {
        const QJsonObject o = v.toObject();
        TimelineDef t;
        t.id    = o[QStringLiteral("id")].toString();
        t.name  = o[QStringLiteral("name")].toString();
        t.color = QColor(o[QStringLiteral("color")].toString());
        m_timelines.append(t);
    }
    if (m_scene) m_scene->setTimelines(m_timelines);

    m_events.clear();
    if (m_scene) m_scene->clearEvents();
    for (const auto& v : root[QStringLiteral("events")].toArray()) {
        const QJsonObject o = v.toObject();
        TimelineEvent e;
        e.id           = o[QStringLiteral("id")].toString();
        e.timelineId   = o[QStringLiteral("timelineId")].toString();
        e.title        = o[QStringLiteral("title")].toString();
        e.description  = o[QStringLiteral("description")].toString();
        e.shape        = o[QStringLiteral("shape")].toString(QStringLiteral("square"));
        const QString col = o[QStringLiteral("color")].toString();
        e.color        = col.isEmpty() ? QColor() : QColor(col);
        e.x            = o[QStringLiteral("x")].toDouble();
        e.y            = o[QStringLiteral("y")].toDouble();
        e.timeMarker   = o[QStringLiteral("timeMarker")].toString();
        e.linkedSceneId = o[QStringLiteral("linkedSceneId")].toString();
        e.linkedDocId  = o[QStringLiteral("linkedDocId")].toString();
        e.conclusion   = o[QStringLiteral("conclusion")].toString();
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
        m_connections.append(c);
        if (m_scene) m_scene->addConnection(c);
    }
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
    )").arg(Theme::appBackground(),
            Theme::panelBackground(),
            Theme::subtleBorder(),
            Theme::textPrimary(),
            Theme::textMuted(),
            Theme::hoverOverlay());

    setStyleSheet(qss);
}
