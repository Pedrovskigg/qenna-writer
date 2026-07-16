#include "MapPanel.h"

#include "GeoData.h"
#include "MapView.h"
#include "MapPinsStore.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QComboBox>
#include <QFileInfo>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSvgRenderer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int kWidth = 700;
constexpr int kHeight = 500;
constexpr int kMargin = 12;
constexpr int kHandleW = 6;
constexpr int kCornerSz = 14;
constexpr int kMinW = 380;
constexpr int kMinH = 300;

// Minúsculas sem acento, pra busca casar "São" com "sao", "Brasília" com "brasi".
QString normLatin(const QString& s)
{
    const QString d = s.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(d.size());
    for (const QChar ch : d) {
        if (ch.category() == QChar::Mark_NonSpacing) continue; // remove diacríticos
        out.append(ch.toLower());
    }
    return out;
}
}

MapPanel::MapPanel(MapPinsStore* pins, ProjectModel* model, QWidget* parent)
    : QFrame(parent)
    , m_pins(pins)
    , m_model(model)
{
    setObjectName(QStringLiteral("mapPanel"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("mapHeader"));
    m_header->setFixedHeight(40);
    m_header->installEventFilter(this);
    auto* headLay = new QHBoxLayout(m_header);
    headLay->setContentsMargins(14, 0, 8, 0);

    m_title = new QLabel(tr("Mapa-múndi"), m_header);
    m_title->setObjectName(QStringLiteral("mapTitle"));
    m_title->installEventFilter(this);
    headLay->addWidget(m_title);
    headLay->addStretch();

    m_maxBtn = new QToolButton(m_header);
    m_maxBtn->setObjectName(QStringLiteral("mapClose"));
    m_maxBtn->setText(QStringLiteral("▢"));
    m_maxBtn->setCursor(Qt::PointingHandCursor);
    m_maxBtn->setToolTip(tr("Maximizar / restaurar"));
    m_maxBtn->setFixedSize(28, 28);
    connect(m_maxBtn, &QToolButton::clicked, this, &MapPanel::toggleMaximize);
    headLay->addWidget(m_maxBtn);

    m_closeBtn = new QToolButton(m_header);
    m_closeBtn->setObjectName(QStringLiteral("mapClose"));
    m_closeBtn->setText(QStringLiteral("×"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip(tr("Fechar"));
    m_closeBtn->setFixedSize(28, 28);
    connect(m_closeBtn, &QToolButton::clicked, this, &MapPanel::closePanel);
    headLay->addWidget(m_closeBtn);

    root->addWidget(m_header);

    // Barra de navegação (botão que abre a lista país > estado > cidade).
    auto* navBar = new QWidget(this);
    navBar->setObjectName(QStringLiteral("mapNavBar"));
    auto* navLay = new QHBoxLayout(navBar);
    navLay->setContentsMargins(10, 6, 10, 6);
    navLay->setSpacing(6);
    m_navBtn = new QToolButton(navBar);
    m_navBtn->setObjectName(QStringLiteral("mapNavBtn"));
    m_navBtn->setText(tr("Ir para local  ▾"));
    m_navBtn->setCheckable(true);
    m_navBtn->setCursor(Qt::PointingHandCursor);
    connect(m_navBtn, &QToolButton::clicked, this, &MapPanel::toggleNavTree);
    navLay->addWidget(m_navBtn);

    auto* rulerBtn = new QToolButton(navBar);
    rulerBtn->setObjectName(QStringLiteral("mapNavBtn"));
    rulerBtn->setText(tr("Régua"));
    rulerBtn->setCheckable(true);
    rulerBtn->setCursor(Qt::PointingHandCursor);
    rulerBtn->setToolTip(tr("Medir distância e fuso entre dois pontos"));
    connect(rulerBtn, &QToolButton::toggled, this, [this, rulerBtn](bool on) {
        if (m_map) m_map->setRulerMode(on);
        if (on && m_pinBtn) m_pinBtn->setChecked(false);
    });
    navLay->addWidget(rulerBtn);

    m_pinBtn = new QToolButton(navBar);
    m_pinBtn->setObjectName(QStringLiteral("mapNavBtn"));
    m_pinBtn->setText(tr("Fixar pin"));
    m_pinBtn->setCheckable(true);
    m_pinBtn->setCursor(Qt::PointingHandCursor);
    m_pinBtn->setToolTip(tr("Clique no mapa pra fixar um lugar de referência"));
    connect(m_pinBtn, &QToolButton::toggled, this, [this, rulerBtn](bool on) {
        if (m_map) m_map->setAddPinMode(on);
        if (on && rulerBtn) rulerBtn->setChecked(false);
    });
    navLay->addWidget(m_pinBtn);

    m_search = new QLineEdit(navBar);
    m_search->setObjectName(QStringLiteral("mapSearch"));
    m_search->setPlaceholderText(tr("Buscar lugar…"));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, &MapPanel::updateSuggestions);
    connect(m_search, &QLineEdit::returnPressed, this, [this] {
        // Enter aceita a sugestão destacada; sem popup, faz a busca direta.
        if (m_suggest && m_suggest->isVisible() && m_suggest->currentItem())
            acceptSuggestion(m_suggest->currentItem());
        else
            searchAndGo();
    });
    m_search->installEventFilter(this); // setas/Esc navegam no popup
    navLay->addWidget(m_search, 1);

    root->addWidget(navBar);

    m_map = new MapView(this);
    root->addWidget(m_map, 1);

    buildInfoCard();
    buildPinPopup();

    // Alças de redimensionamento nas bordas (invisíveis, só cursor + drag).
    auto makeHandle = [this](Qt::CursorShape cur) {
        auto* h = new QWidget(this);
        h->setCursor(cur);
        h->installEventFilter(this);
        h->raise();
        return h;
    };
    m_hL = makeHandle(Qt::SizeHorCursor);
    m_hR = makeHandle(Qt::SizeHorCursor);
    m_hT = makeHandle(Qt::SizeVerCursor);
    m_hB = makeHandle(Qt::SizeVerCursor);
    m_hTL = makeHandle(Qt::SizeFDiagCursor);
    m_hTR = makeHandle(Qt::SizeBDiagCursor);
    m_hBL = makeHandle(Qt::SizeBDiagCursor);
    m_hBR = makeHandle(Qt::SizeFDiagCursor);

    connect(m_map, &MapView::featureClicked, this, &MapPanel::showFeatureInfo);
    if (m_map) m_map->setPinsStore(m_pins);
    connect(m_map, &MapView::pinRequestedAt, this, &MapPanel::onPinRequested);
    connect(m_map, &MapView::pinClicked, this, &MapPanel::onPinClicked);

    // Toggle Simples / Texturizado (mapa físico).
    auto* texBtn = new QToolButton(navBar);
    texBtn->setObjectName(QStringLiteral("mapNavBtn"));
    texBtn->setText(tr("Textura"));
    texBtn->setCheckable(true);
    texBtn->setCursor(Qt::PointingHandCursor);
    texBtn->setToolTip(tr("Alternar mapa simples / texturizado"));
    texBtn->blockSignals(true);
    texBtn->setChecked(m_map->textured());
    texBtn->blockSignals(false);
    connect(texBtn, &QToolButton::toggled, this, [this](bool on) {
        if (m_map) m_map->setTextured(on);
    });
    navLay->insertWidget(3, texBtn);

    // Toggle Plano / Globo 3D.
    auto* globeBtn = new QToolButton(navBar);
    globeBtn->setObjectName(QStringLiteral("mapNavBtn"));
    globeBtn->setText(tr("Globo"));
    globeBtn->setCheckable(true);
    globeBtn->setCursor(Qt::PointingHandCursor);
    globeBtn->setToolTip(tr("Alternar mapa plano / globo 3D"));
    globeBtn->blockSignals(true);
    globeBtn->setChecked(m_map->globeMode());
    globeBtn->blockSignals(false);
    connect(globeBtn, &QToolButton::toggled, this, [this](bool on) {
        if (m_map) m_map->setGlobeMode(on);
    });
    navLay->insertWidget(4, globeBtn);

    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &MapPanel::applyTheme);
    hide();
}

void MapPanel::centerInParent()
{
    QWidget* p = parentWidget();
    if (!p) return;
    const int w = qMin(kWidth, p->width() - kMargin * 2);
    const int h = qMin(kHeight, p->height() - m_topInset - kMargin * 2);
    resize(qMax(280, w), qMax(240, h));
    move((p->width() - width()) / 2,
         m_topInset + qMax(kMargin, (p->height() - m_topInset - height()) / 2));
    m_positioned = true;
}

void MapPanel::layoutResizeHandles()
{
    if (!m_hL) return;
    const int w = width(), h = height(), hw = kHandleW, cs = kCornerSz;
    m_hL->setGeometry(0, cs, hw, h - 2 * cs);
    m_hR->setGeometry(w - hw, cs, hw, h - 2 * cs);
    m_hT->setGeometry(cs, 0, w - 2 * cs, hw);
    m_hB->setGeometry(cs, h - hw, w - 2 * cs, hw);
    m_hTL->setGeometry(0, 0, cs, cs);
    m_hTR->setGeometry(w - cs, 0, cs, cs);
    m_hBL->setGeometry(0, h - cs, cs, cs);
    m_hBR->setGeometry(w - cs, h - cs, cs, cs);
    for (QWidget* hd : { m_hL, m_hR, m_hT, m_hB, m_hTL, m_hTR, m_hBL, m_hBR })
        hd->raise();
}

void MapPanel::toggleMaximize()
{
    QWidget* p = parentWidget();
    if (!p) return;
    if (!m_maximized) {
        m_normalGeom = geometry();
        m_maximized = true;
        const int m = 8;
        setGeometry(m, m_topInset + m, p->width() - 2 * m, p->height() - m_topInset - 2 * m);
    } else {
        m_maximized = false;
        if (m_normalGeom.isValid()) setGeometry(m_normalGeom);
        else centerInParent();
    }
}

void MapPanel::togglePanel()
{
    if (isPanelOpen()) closePanel();
    else openPanel();
}

void MapPanel::openPanel()
{
    applyTheme();
    centerInParent();
    show();
    raise();
}

void MapPanel::closePanel()
{
    hide();
}

bool MapPanel::isPanelOpen() const
{
    return isVisible();
}

void MapPanel::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    if (!m_positioned) centerInParent();
}

void MapPanel::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    layoutResizeHandles();
    if (m_infoCard && m_infoCard->isVisible()) positionInfoCard();
    if (m_navTree && m_navTree->isVisible()) positionNavTree();
}

void MapPanel::buildInfoCard()
{
    m_infoCard = new QFrame(this);
    m_infoCard->setObjectName(QStringLiteral("mapInfoCard"));
    m_infoCard->setAutoFillBackground(true); // fundo sólido (nunca translúcido)
    m_infoCard->setFixedWidth(250);
    auto* lay = new QVBoxLayout(m_infoCard);
    lay->setContentsMargins(14, 12, 14, 12);
    lay->setSpacing(3);

    // Topo: bandeira + (título / subtítulo) + fechar.
    auto* top = new QHBoxLayout();
    top->setContentsMargins(0, 0, 0, 0);
    top->setSpacing(10);

    m_infoFlag = new QLabel(m_infoCard);
    m_infoFlag->setObjectName(QStringLiteral("mapInfoFlag"));
    m_infoFlag->setFixedSize(40, 30);
    m_infoFlag->setAlignment(Qt::AlignCenter);
    top->addWidget(m_infoFlag, 0, Qt::AlignTop);

    auto* titleCol = new QVBoxLayout();
    titleCol->setContentsMargins(0, 0, 0, 0);
    titleCol->setSpacing(1);
    m_infoTitle = new QLabel(m_infoCard);
    m_infoTitle->setObjectName(QStringLiteral("mapInfoTitle"));
    m_infoTitle->setWordWrap(true);
    titleCol->addWidget(m_infoTitle);
    m_infoSub = new QLabel(m_infoCard);
    m_infoSub->setObjectName(QStringLiteral("mapInfoSub"));
    titleCol->addWidget(m_infoSub);
    top->addLayout(titleCol, 1);

    auto* close = new QToolButton(m_infoCard);
    close->setObjectName(QStringLiteral("mapInfoClose"));
    close->setText(QStringLiteral("×"));
    close->setCursor(Qt::PointingHandCursor);
    close->setFixedSize(20, 20);
    connect(close, &QToolButton::clicked, m_infoCard, &QWidget::hide);
    top->addWidget(close, 0, Qt::AlignTop);
    lay->addLayout(top);

    auto* sep = new QFrame(m_infoCard);
    sep->setObjectName(QStringLiteral("mapInfoSep"));
    sep->setFixedHeight(1);
    lay->addWidget(sep);

    m_infoBody = new QLabel(m_infoCard);
    m_infoBody->setObjectName(QStringLiteral("mapInfoBody"));
    m_infoBody->setWordWrap(true);
    m_infoBody->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(m_infoBody);

    m_infoCard->hide();
}

void MapPanel::showFeatureInfo(QString title, QString subtitle, QStringList details, QString flagIso)
{
    if (!m_infoCard) return;
    m_infoTitle->setText(title);
    m_infoSub->setText(subtitle);
    m_infoBody->setText(details.join(QLatin1Char('\n')));
    m_infoBody->setVisible(!details.isEmpty());

    // Bandeira do país (SVG colorido renderizado num pixmap).
    bool hasFlag = false;
    if (!flagIso.isEmpty()) {
        const QString path = QStringLiteral(":/flags/%1.svg").arg(flagIso.toLower());
        QSvgRenderer renderer(path);
        if (renderer.isValid()) {
            QPixmap pm(40, 30);
            pm.fill(Qt::transparent);
            QPainter pp(&pm);
            renderer.render(&pp, QRectF(0, 0, 40, 30));
            pp.end();
            m_infoFlag->setPixmap(pm);
            hasFlag = true;
        }
    }
    m_infoFlag->setVisible(hasFlag);

    m_infoCard->adjustSize();
    positionInfoCard();
    m_infoCard->show();
    m_infoCard->raise();
}

void MapPanel::positionInfoCard()
{
    if (!m_infoCard) return;
    const int m = 12;
    m_infoCard->move(m, height() - m_infoCard->height() - m);
}

void MapPanel::buildPinPopup()
{
    m_pinPopup = new QFrame(this);
    m_pinPopup->setObjectName(QStringLiteral("mapPinPopup"));
    m_pinPopup->setFixedWidth(280);
    auto* lay = new QVBoxLayout(m_pinPopup);
    lay->setContentsMargins(14, 12, 14, 12);
    lay->setSpacing(8);

    m_pinTitle = new QLabel(tr("Novo pin"), m_pinPopup);
    m_pinTitle->setObjectName(QStringLiteral("mapPinTitle"));
    lay->addWidget(m_pinTitle);

    m_pinLabelEdit = new QLineEdit(m_pinPopup);
    m_pinLabelEdit->setObjectName(QStringLiteral("mapPinField"));
    m_pinLabelEdit->setPlaceholderText(tr("Nome do lugar"));
    connect(m_pinLabelEdit, &QLineEdit::returnPressed, this, &MapPanel::savePinFromPopup);
    lay->addWidget(m_pinLabelEdit);

    m_pinNoteEdit = new QLineEdit(m_pinPopup);
    m_pinNoteEdit->setObjectName(QStringLiteral("mapPinField"));
    m_pinNoteEdit->setPlaceholderText(tr("Nota (opcional)"));
    lay->addWidget(m_pinNoteEdit);

    m_pinLink = new QComboBox(m_pinPopup);
    m_pinLink->setObjectName(QStringLiteral("mapPinLink"));
    lay->addWidget(m_pinLink);

    auto* actions = new QHBoxLayout();
    actions->setContentsMargins(0, 0, 0, 0);
    m_pinDeleteBtn = new QToolButton(m_pinPopup);
    m_pinDeleteBtn->setObjectName(QStringLiteral("mapPinDelete"));
    m_pinDeleteBtn->setText(tr("Excluir"));
    m_pinDeleteBtn->setCursor(Qt::PointingHandCursor);
    connect(m_pinDeleteBtn, &QToolButton::clicked, this, &MapPanel::deletePinFromPopup);
    actions->addWidget(m_pinDeleteBtn);
    actions->addStretch();
    auto* cancel = new QToolButton(m_pinPopup);
    cancel->setObjectName(QStringLiteral("mapPinCancel"));
    cancel->setText(tr("Cancelar"));
    cancel->setCursor(Qt::PointingHandCursor);
    connect(cancel, &QToolButton::clicked, m_pinPopup, &QWidget::hide);
    actions->addWidget(cancel);
    auto* save = new QToolButton(m_pinPopup);
    save->setObjectName(QStringLiteral("mapPinSave"));
    save->setText(tr("Salvar"));
    save->setCursor(Qt::PointingHandCursor);
    connect(save, &QToolButton::clicked, this, &MapPanel::savePinFromPopup);
    actions->addWidget(save);
    lay->addLayout(actions);

    m_pinPopup->hide();
}

void MapPanel::fillPinLink(const QString& selectedId)
{
    if (!m_pinLink) return;
    m_pinLink->clear();
    m_pinLink->addItem(tr("Sem vínculo"), QString());
    if (m_model) {
        for (const Drawer& d : m_model->drawers()) {
            for (const DrawerItem& it : d.items) {
                const QString name = it.title.isEmpty() ? tr("(sem título)") : it.title;
                m_pinLink->addItem(name, it.id);
            }
        }
    }
    const int idx = m_pinLink->findData(selectedId);
    m_pinLink->setCurrentIndex(idx >= 0 ? idx : 0);
}

void MapPanel::openPinPopup()
{
    if (!m_pinPopup) return;
    m_pinPopup->adjustSize();
    const int y = m_map ? m_map->y() + 30 : 60;
    m_pinPopup->move((width() - m_pinPopup->width()) / 2, y);
    m_pinPopup->show();
    m_pinPopup->raise();
    m_pinLabelEdit->setFocus();
}

void MapPanel::onPinRequested(double lon, double lat)
{
    m_editPinId.clear();
    m_pinLon = lon;
    m_pinLat = lat;
    m_pinTitle->setText(tr("Novo pin"));
    m_pinLabelEdit->clear();
    m_pinNoteEdit->clear();
    fillPinLink(QString());
    m_pinDeleteBtn->hide();
    openPinPopup();
    if (m_pinBtn) m_pinBtn->setChecked(false); // modo "fixar" é de um clique só
}

void MapPanel::onPinClicked(QString id)
{
    if (!m_pins) return;
    for (const MapPinsStore::Pin& pin : m_pins->pins()) {
        if (pin.id != id) continue;
        m_editPinId = id;
        m_pinLon = pin.lon;
        m_pinLat = pin.lat;
        m_pinTitle->setText(tr("Editar pin"));
        m_pinLabelEdit->setText(pin.label);
        m_pinNoteEdit->setText(pin.note);
        fillPinLink(pin.linkId);
        m_pinDeleteBtn->show();
        openPinPopup();
        return;
    }
}

void MapPanel::savePinFromPopup()
{
    if (!m_pins) return;
    const QString label = m_pinLabelEdit->text().trimmed();
    if (label.isEmpty()) { m_pinLabelEdit->setFocus(); return; }
    MapPinsStore::Pin p;
    p.id = m_editPinId; // vazio = novo
    p.lon = m_pinLon;
    p.lat = m_pinLat;
    p.label = label;
    p.note = m_pinNoteEdit->text().trimmed();
    if (m_pinLink) {
        p.linkId = m_pinLink->currentData().toString();
        if (!p.linkId.isEmpty()) p.linkLabel = m_pinLink->currentText();
    }
    if (m_editPinId.isEmpty()) m_pins->addPin(p);
    else m_pins->updatePin(p);
    m_pinPopup->hide();
}

void MapPanel::deletePinFromPopup()
{
    if (m_pins && !m_editPinId.isEmpty()) m_pins->removePin(m_editPinId);
    m_pinPopup->hide();
}

void MapPanel::buildIndex()
{
    if (m_indexBuilt) return;
    const GeoData& geo = GeoData::instance();
    for (const GeoData::Country& c : geo.countries())
        if (!c.iso.isEmpty()) m_countryNames.insert(c.iso, c.name);
    const QVector<GeoData::Place>& places = geo.places();
    for (int i = 0; i < places.size(); ++i) {
        const GeoData::Place& p = places.at(i);
        if (p.countryCode.isEmpty()) continue;
        m_index[p.countryCode][p.state].append(i);
    }
    m_indexBuilt = true;
}

void MapPanel::populateCountries()
{
    QList<QPair<QString, QString>> list; // nome, cc
    for (auto it = m_index.constBegin(); it != m_index.constEnd(); ++it)
        list.append({ m_countryNames.value(it.key(), it.key()), it.key() });
    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        return a.first.localeAwareCompare(b.first) < 0;
    });
    for (const auto& pr : list) {
        auto* item = new QTreeWidgetItem(m_navTree);
        item->setText(0, pr.first);
        item->setData(0, Qt::UserRole, QStringLiteral("country"));
        item->setData(0, Qt::UserRole + 1, pr.second);
        new QTreeWidgetItem(item); // dummy → mostra a seta de expandir
    }
}

void MapPanel::onNavItemExpanded(QTreeWidgetItem* item)
{
    if (!item || item->data(0, Qt::UserRole + 9).toBool()) return; // já carregado
    qDeleteAll(item->takeChildren()); // remove o dummy

    const GeoData& geo = GeoData::instance();
    const QVector<GeoData::Place>& places = geo.places();
    const QString type = item->data(0, Qt::UserRole).toString();

    auto addCities = [&](QTreeWidgetItem* parent, const QString& cc, const QString& st) {
        QList<int> idx = m_index[cc][st];
        std::sort(idx.begin(), idx.end(), [&](int a, int b) {
            return places.at(a).population > places.at(b).population;
        });
        for (int i : idx) {
            auto* c = new QTreeWidgetItem(parent);
            c->setText(0, places.at(i).name);
            c->setData(0, Qt::UserRole, QStringLiteral("city"));
            c->setData(0, Qt::UserRole + 1, i);
        }
    };

    if (type == QStringLiteral("country")) {
        const QString cc = item->data(0, Qt::UserRole + 1).toString();
        const QMap<QString, QList<int>>& states = m_index[cc];
        for (auto sit = states.constBegin(); sit != states.constEnd(); ++sit) {
            if (sit.key().isEmpty()) {
                addCities(item, cc, sit.key()); // cidades sem estado: direto no país
            } else {
                auto* sItem = new QTreeWidgetItem(item);
                sItem->setText(0, sit.key());
                sItem->setData(0, Qt::UserRole, QStringLiteral("state"));
                sItem->setData(0, Qt::UserRole + 1, cc);
                sItem->setData(0, Qt::UserRole + 2, sit.key());
                new QTreeWidgetItem(sItem); // dummy
            }
        }
    } else if (type == QStringLiteral("state")) {
        addCities(item, item->data(0, Qt::UserRole + 1).toString(),
                        item->data(0, Qt::UserRole + 2).toString());
    }
    item->setData(0, Qt::UserRole + 9, true);
}

void MapPanel::onNavItemClicked(QTreeWidgetItem* item, int)
{
    if (!item || !m_map) return;
    const GeoData& geo = GeoData::instance();
    const QVector<GeoData::Place>& places = geo.places();
    const QString type = item->data(0, Qt::UserRole).toString();

    if (type == QStringLiteral("country")) {
        const QString cc = item->data(0, Qt::UserRole + 1).toString();
        for (const GeoData::Country& c : geo.countries())
            if (c.iso == cc) { m_map->flyToBounds(c.bounds); return; }
        // fallback: bounds das cidades do país
        QRectF b;
        for (auto sit = m_index[cc].constBegin(); sit != m_index[cc].constEnd(); ++sit)
            for (int i : sit.value())
                b = b.united(QRectF(QPointF(places.at(i).lon, places.at(i).lat), QSizeF(0.01, 0.01)));
        if (!b.isNull()) m_map->flyToBounds(b.adjusted(-1, -1, 1, 1));
    } else if (type == QStringLiteral("state")) {
        const QString cc = item->data(0, Qt::UserRole + 1).toString();
        const QString st = item->data(0, Qt::UserRole + 2).toString();
        QRectF b;
        for (int i : m_index[cc][st])
            b = b.united(QRectF(QPointF(places.at(i).lon, places.at(i).lat), QSizeF(0.01, 0.01)));
        if (!b.isNull()) m_map->flyToBounds(b.adjusted(-1, -1, 1, 1));
    } else if (type == QStringLiteral("city")) {
        const int i = item->data(0, Qt::UserRole + 1).toInt();
        if (i >= 0 && i < places.size())
            m_map->flyToPoint(places.at(i).lon, places.at(i).lat);
    }
}

void MapPanel::toggleNavTree()
{
    if (!m_navTree) {
        m_navTree = new QTreeWidget(this);
        m_navTree->setObjectName(QStringLiteral("mapNavTree"));
        m_navTree->setHeaderHidden(true);
        m_navTree->setColumnCount(1);
        connect(m_navTree, &QTreeWidget::itemExpanded, this, &MapPanel::onNavItemExpanded);
        connect(m_navTree, &QTreeWidget::itemClicked, this, &MapPanel::onNavItemClicked);
        buildIndex();
        populateCountries();
    }
    if (m_navTree->isVisible()) {
        m_navTree->hide();
        if (m_navBtn) m_navBtn->setChecked(false);
        return;
    }
    positionNavTree();
    m_navTree->show();
    m_navTree->raise();
    if (m_navBtn) m_navBtn->setChecked(true);
}

void MapPanel::positionNavTree()
{
    if (!m_navTree || !m_map) return;
    const int top = m_map->y();
    m_navTree->setGeometry(8, top + 8, 240, qMax(120, m_map->height() - 16));
}

void MapPanel::searchAndGo()
{
    if (!m_search || !m_map) return;
    const QString q = m_search->text().trimmed();
    if (q.size() < 2) return;
    const QString ql = q.toLower();
    const GeoData& geo = GeoData::instance();

    // 1) País por nome (pt ou en).
    for (const GeoData::Country& c : geo.countries()) {
        if (c.name.toLower().contains(ql) || c.nameEn.toLower().contains(ql)) {
            m_map->flyToBounds(c.bounds);
            return;
        }
    }
    // 2) Cidade: o melhor casamento (mais populoso). Casa nome, sem acento e
    //    nomes alternativos (busca multilíngue).
    const GeoData::Place* best = nullptr;
    qint64 bestPop = -1;
    for (const GeoData::Place& p : geo.places()) {
        const bool hit = p.name.toLower().contains(ql)
            || p.asciiName.toLower().contains(ql)
            || p.altNames.toLower().contains(ql);
        if (hit && p.population > bestPop) { bestPop = p.population; best = &p; }
    }
    if (best) m_map->flyToPoint(best->lon, best->lat);
}

void MapPanel::updateSuggestions(const QString& text)
{
    if (!m_search) return;
    const QString q = text.trimmed();
    if (q.size() < 2) { if (m_suggest) m_suggest->hide(); return; }

    if (!m_suggest) {
        m_suggest = new QListWidget(this);
        m_suggest->setObjectName(QStringLiteral("mapSuggest"));
        m_suggest->setFocusPolicy(Qt::NoFocus); // foco fica no campo de texto
        m_suggest->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        connect(m_suggest, &QListWidget::itemClicked, this, &MapPanel::acceptSuggestion);
    }

    const QString nq = normLatin(q);
    const GeoData& geo = GeoData::instance();
    m_suggest->clear();

    auto add = [&](const QString& name, const QString& sub,
                   const QString& type, int idx) {
        auto* it = new QListWidgetItem(name + QStringLiteral("   —   ") + sub, m_suggest);
        it->setData(Qt::UserRole, type);
        it->setData(Qt::UserRole + 1, idx);
    };

    // Ordem: países, depois estados, depois cidades (por população).
    int nC = 0;
    for (int i = 0; i < geo.countries().size() && nC < 5; ++i) {
        const GeoData::Country& c = geo.countries().at(i);
        if (normLatin(c.name).startsWith(nq) || normLatin(c.nameEn).startsWith(nq)) {
            add(c.name, tr("País"), QStringLiteral("country"), i);
            ++nC;
        }
    }
    int nS = 0;
    for (int i = 0; i < geo.states().size() && nS < 5; ++i) {
        const GeoData::State& s = geo.states().at(i);
        if (normLatin(s.name).startsWith(nq)) {
            add(s.name, tr("Estado · %1").arg(s.country), QStringLiteral("state"), i);
            ++nS;
        }
    }
    struct Cm { int idx; qint64 pop; };
    QVector<Cm> cm;
    for (int i = 0; i < geo.places().size(); ++i) {
        const GeoData::Place& p = geo.places().at(i);
        if (normLatin(p.name).startsWith(nq) || normLatin(p.asciiName).startsWith(nq))
            cm.push_back({ i, p.population });
    }
    std::sort(cm.begin(), cm.end(), [](const Cm& a, const Cm& b) { return a.pop > b.pop; });
    for (int k = 0; k < cm.size() && k < 7; ++k) {
        const GeoData::Place& p = geo.places().at(cm[k].idx);
        const QString where = p.state.isEmpty() ? p.countryCode : p.state;
        add(p.name, where.isEmpty() ? tr("Cidade") : tr("Cidade · %1").arg(where),
            QStringLiteral("city"), cm[k].idx);
    }

    if (m_suggest->count() == 0) { m_suggest->hide(); return; }
    m_suggest->setCurrentRow(0);
    positionSuggest();
    m_suggest->show();
    m_suggest->raise();
}

void MapPanel::positionSuggest()
{
    if (!m_suggest || !m_search) return;
    const QPoint tl = m_search->mapTo(this, QPoint(0, m_search->height() + 2));
    const int rows = qMin(12, m_suggest->count());
    const int rowH = qMax(22, m_suggest->sizeHintForRow(0));
    m_suggest->setGeometry(tl.x(), tl.y(), m_search->width(), rows * rowH + 6);
}

void MapPanel::acceptSuggestion(QListWidgetItem* item)
{
    if (!item || !m_map || !m_search) return;
    const GeoData& geo = GeoData::instance();
    const QString type = item->data(Qt::UserRole).toString();
    const int idx = item->data(Qt::UserRole + 1).toInt();

    QString name;
    if (type == QStringLiteral("country") && idx < geo.countries().size()) {
        const GeoData::Country& c = geo.countries().at(idx);
        name = c.name;
        m_map->flyToBounds(c.bounds);
    } else if (type == QStringLiteral("state") && idx < geo.states().size()) {
        const GeoData::State& s = geo.states().at(idx);
        name = s.name;
        if (!s.bounds.isNull()) m_map->flyToBounds(s.bounds.adjusted(-1, -1, 1, 1));
    } else if (type == QStringLiteral("city") && idx < geo.places().size()) {
        const GeoData::Place& p = geo.places().at(idx);
        name = p.name;
        m_map->flyToPoint(p.lon, p.lat);
    }
    if (!name.isEmpty()) {
        m_search->blockSignals(true); // não reabrir o popup ao preencher
        m_search->setText(name);
        m_search->blockSignals(false);
    }
    m_suggest->hide();
}

bool MapPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Setas/Esc no campo de busca navegam o popup de sugestões.
    if (watched == m_search && event->type() == QEvent::KeyPress
        && m_suggest && m_suggest->isVisible() && m_suggest->count() > 0) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Down) {
            m_suggest->setCurrentRow(qMin(m_suggest->currentRow() + 1, m_suggest->count() - 1));
            return true;
        }
        if (ke->key() == Qt::Key_Up) {
            m_suggest->setCurrentRow(qMax(m_suggest->currentRow() - 1, 0));
            return true;
        }
        if (ke->key() == Qt::Key_Escape) { m_suggest->hide(); return true; }
    }

    // Redimensionamento pelas alças das bordas.
    auto edgeOf = [this](QObject* o) -> ResizeEdge {
        if (o == m_hL)  return ResizeEdge::Left;
        if (o == m_hR)  return ResizeEdge::Right;
        if (o == m_hT)  return ResizeEdge::Top;
        if (o == m_hB)  return ResizeEdge::Bottom;
        if (o == m_hTL) return ResizeEdge::TL;
        if (o == m_hTR) return ResizeEdge::TR;
        if (o == m_hBL) return ResizeEdge::BL;
        if (o == m_hBR) return ResizeEdge::BR;
        return ResizeEdge::None;
    };
    const ResizeEdge edge = edgeOf(watched);
    if (edge != ResizeEdge::None) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_resizing = true;
                m_activeEdge = edge;
                m_resizeStartGeom = geometry();
                m_resizeStartMouse = me->globalPosition().toPoint();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_resizing) {
            auto* me = static_cast<QMouseEvent*>(event);
            const QPoint g = me->globalPosition().toPoint();
            const int dx = g.x() - m_resizeStartMouse.x();
            const int dy = g.y() - m_resizeStartMouse.y();
            int nx = m_resizeStartGeom.x(), ny = m_resizeStartGeom.y();
            int nw = m_resizeStartGeom.width(), nh = m_resizeStartGeom.height();
            switch (m_activeEdge) {
            case ResizeEdge::Left:   nw -= dx; nx += dx; break;
            case ResizeEdge::Right:  nw += dx; break;
            case ResizeEdge::Top:    nh -= dy; ny += dy; break;
            case ResizeEdge::Bottom: nh += dy; break;
            case ResizeEdge::TL: nw -= dx; nx += dx; nh -= dy; ny += dy; break;
            case ResizeEdge::TR: nw += dx; nh -= dy; ny += dy; break;
            case ResizeEdge::BL: nw -= dx; nx += dx; nh += dy; break;
            case ResizeEdge::BR: nw += dx; nh += dy; break;
            default: break;
            }
            if (nw < kMinW) {
                if (m_activeEdge == ResizeEdge::Left || m_activeEdge == ResizeEdge::TL
                    || m_activeEdge == ResizeEdge::BL)
                    nx = m_resizeStartGeom.right() - kMinW + 1;
                nw = kMinW;
            }
            if (nh < kMinH) {
                if (m_activeEdge == ResizeEdge::Top || m_activeEdge == ResizeEdge::TL
                    || m_activeEdge == ResizeEdge::TR)
                    ny = m_resizeStartGeom.bottom() - kMinH + 1;
                nh = kMinH;
            }
            setGeometry(nx, ny, nw, nh);
            m_maximized = false;
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_resizing = false;
        }
        return QFrame::eventFilter(watched, event);
    }

    if (watched == m_header || watched == m_title) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            auto* me = static_cast<QMouseEvent*>(event);
            QPoint target = me->globalPosition().toPoint() - m_dragOffset;
            if (QWidget* p = parentWidget()) {
                QPoint local = p->mapFromGlobal(target);
                local.setX(qBound(0, local.x(), qMax(0, p->width() - width())));
                local.setY(qBound(0, local.y(), qMax(0, p->height() - height())));
                move(local);
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
        }
    }
    return QFrame::eventFilter(watched, event);
}

void MapPanel::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        #mapPanel {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }
        #mapHeader { border-bottom: 1px solid %2; }
        #mapTitle { color: %3; font-size: 15px; font-weight: 600; }
        #mapClose {
            color: %4; background: transparent; border: none;
            font-size: 18px; border-radius: 6px;
        }
        #mapClose:hover { background: %5; color: %3; }
        #mapInfoCard {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
        #mapInfoTitle { color: %3; font-size: 14px; font-weight: 600; }
        #mapInfoSub { color: %7; font-size: 11px; }
        #mapInfoSep { background: %2; border: none; }
        #mapInfoFlag { border: 1px solid %2; }
        #mapInfoBody { color: %4; font-size: 12px; }
        #mapInfoClose {
            color: %4; background: transparent; border: none; font-size: 15px;
        }
        #mapInfoClose:hover { color: %3; }
        #mapNavBar { border-bottom: 1px solid %2; }
        #mapNavBtn {
            color: %3; background: %6; border: 1px solid %2;
            border-radius: 6px; padding: 5px 12px; font-size: 12px;
        }
        #mapNavBtn:hover { background: %5; }
        #mapNavBtn:checked { background: %7; color: %3; border-color: %7; }
        #mapSearch {
            color: %3; background: %6; border: 1px solid %2;
            border-radius: 6px; padding: 5px 10px; font-size: 12px;
        }
        #mapSearch:focus { border-color: %7; }
        #mapNavTree {
            background: %1; color: %3; border: 1px solid %2;
            border-radius: 8px; font-size: 12px; outline: none;
        }
        #mapNavTree::item { padding: 3px 2px; }
        #mapNavTree::item:hover { background: %5; }
        #mapNavTree::item:selected { background: %7; color: %3; }
        #mapPinPopup {
            background: %1; border: 1px solid %2; border-radius: 10px;
        }
        #mapPinTitle { color: %3; font-size: 14px; font-weight: 600; }
        #mapPinField {
            color: %3; background: %6; border: 1px solid %2;
            border-radius: 6px; padding: 6px 8px; font-size: 13px;
        }
        #mapPinField:focus { border-color: %7; }
        #mapPinLink {
            color: %3; background: %6; border: 1px solid %2;
            border-radius: 6px; padding: 5px 8px; font-size: 12px;
        }
        #mapPinLink QAbstractItemView {
            background: %1; color: %3; border: 1px solid %2;
            selection-background-color: %7;
        }
        #mapPinSave {
            color: %3; background: %7; border: none;
            border-radius: 6px; padding: 5px 14px; font-size: 13px; font-weight: 600;
        }
        #mapPinCancel, #mapPinDelete {
            color: %4; background: transparent; border: none;
            border-radius: 6px; padding: 5px 10px; font-size: 13px;
        }
        #mapPinCancel:hover { color: %3; background: %5; }
        #mapPinDelete:hover { color: %3; background: %5; }
        #mapSuggest {
            background: %1; color: %3; border: 1px solid %2;
            border-radius: 8px; font-size: 12px; outline: none;
        }
        #mapSuggest::item { padding: 4px 9px; border-radius: 5px; }
        #mapSuggest::item:hover { background: %5; }
        #mapSuggest::item:selected { background: %7; color: %3; }
    )")
        .arg(Theme::panelBackground(), Theme::borderStrong(), Theme::textPrimary(),
             Theme::textMuted(), Theme::hoverStrong(), Theme::inputBackground(),
             Theme::accentDefault()));
}
