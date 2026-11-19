#include "LeftBar.h"
#include "LeftBarLabels.h"
#include "LeftBarViews.h"
#include "CarouselViewport.h"
#include "IconUtils.h"
#include "PanelMotion.h"
#include "ProjectModel.h"
#include "Theme.h"
#include "ToolbarGroupWidget.h"
#include "UiScale.h"

#include <QApplication>
#include <QColor>
#include <QCursor>
#include <QEnterEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QSettings>
#include <QTimer>
#include <QVariantAnimation>
#include <functional>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFrame>
#include <QMimeData>
#include <QMouseEvent>
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QLabel>
#include <QMenu>
#include <QPixmap>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString separatorQss() {
    return QStringLiteral(
        "background: %1; border: none; max-height: 1px; margin: 2px 4px;"
    ).arg(Theme::subtleBorder());
}
}

namespace {
constexpr const char* kDrawerMime = "application/x-mira-drawer-key";
}

namespace {

// Valores-base (escala 1.0). O tamanho de verdade em pixels vem das funções
// abaixo, que aplicam o fator de UiScale — permite o slider de "Tamanho da
// interface" nas Configurações reescalar a barra em tempo real.
// Mais fina (13/11, pedido do Pe): era 60/40. O ícone continua 26; saiu só a
// folga em volta dele.
constexpr int kBarWidthBase = 46;
constexpr int kBtnSizeBase  = 36;
constexpr int kIconSizeBase = 26;

int barWidthPx() { return qMax(40, qRound(kBarWidthBase * UiScale::scale())); }
int btnSizePx()  { return qMax(24, qRound(kBtnSizeBase  * UiScale::scale())); }
// Margem dos lados: o que sobra da largura, dividido igual (o botão no meio).
int sidePadPx()  { return qMax(2, (barWidthPx() - btnSizePx()) / 2); }
int iconSizePx() { return qMax(14, qRound(kIconSizeBase * UiScale::scale())); }

QIcon loadLeftbarIcon(const QString& fileName) {
    const int px = iconSizePx();
    return IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/leftbar/%1").arg(fileName),
        QColor(Theme::textMuted()),
        QColor(Theme::textPrimary()),
        QColor(Theme::textBright()),
        QSize(px, px));
}

QString fixedButtonQss() {
    // Sem fundo no estado normal; borda translúcida no hover; outline em
    // accent + leve tint no estado checked (= painel aberto). Sem mudança
    // de tamanho — todas as bordas têm 1px reservado em transparent.
    return Theme::qss(QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: @radius-control;
            color: %1;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 15px;
            font-weight: 600;
        }
        QToolButton:hover {
            background: %2;
            border-color: %3;
            color: %4;
        }
        QToolButton:checked {
            background: %6;
            border-color: %5;
            color: %4;
        }
    )")).arg(Theme::textMuted(),
            Theme::hoverOverlay(),
            Theme::subtleBorder(),
            Theme::textBright(),
            Theme::accentDefault(),
            Theme::pressedOverlay());
}

QString drawerButtonQss(const QString& accent) {
    // Letra colorida com o accent da gaveta; fundo sempre transparente.
    // Hover e checked usam a própria cor da gaveta como borda.
    return Theme::qss(QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: @radius-control;
            color: %1;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 17px;
            font-weight: 700;
        }
        QToolButton:hover {
            background: %3;
            border-color: %4;
            color: %2;
        }
        QToolButton:checked {
            background: %5;
            border-color: %1;
            color: %2;
        }
    )")).arg(accent, Theme::textBright(),
           Theme::hoverOverlay(), Theme::subtleBorder(),
           Theme::pressedOverlay());
}

QString newDrawerQss() {
    return Theme::qss(QStringLiteral(R"(
        QToolButton {
            background: transparent;
            color: %1;
            border: 1px dashed %2;
            border-radius: @radius-control;
            font-size: 22px;
            font-weight: 300;
        }
        QToolButton:hover {
            color: %3;
            border-color: %4;
            background: %5;
        }
    )")).arg(Theme::textMuted(),
            Theme::panelBorder(),
            Theme::textBright(),
            Theme::textMuted(),
            Theme::pressedOverlay());
}

// Gavetas com cor: o ladrilho é um pouco menor que o botão, dentro da bandeja.
int tilePx() { return qMax(20, qRound(btnSizePx() * 0.84)); }

QString rgba(const QColor& c, qreal a) {
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(QString::number(a, 'f', 2));
}

QString tileQss(const QColor& c) {
    const int r = qMax(5, qRound(9 * UiScale::scale()));
    return QStringLiteral(R"(
        QToolButton { background: %1; border: none; border-radius: %4px; color: %3;
                      font-family: 'Lora','Crimson Text',serif; font-size: %5px; font-weight: 700; }
        QToolButton:hover { background: %2; }
        QToolButton:checked { background: %3; color: #ffffff; }
    )").arg(rgba(c, 0.17), rgba(c, 0.28), c.name()).arg(r).arg(qMax(10, qRound(14 * UiScale::scale())));
}

QString newTileQss() {
    const int r = qMax(5, qRound(9 * UiScale::scale()));
    return QStringLiteral(R"(
        QToolButton { background: transparent; color: %1; border: 1px dashed %2; border-radius: %5px;
                      font-size: %6px; font-weight: 300; }
        QToolButton:hover { color: %3; border-color: %1; background: %4; }
    )").arg(Theme::textMuted(), Theme::panelBorder(), Theme::textBright(), Theme::pressedOverlay())
       .arg(r).arg(qMax(12, qRound(18 * UiScale::scale())));
}

// A barra que lê a cena: gaveta com gente na folha acende na cor dela.
QString litQss(const QColor& c) {
    return Theme::qss(QStringLiteral(R"(
        QToolButton { background: %1; border: 1px solid %2; border-radius: @radius-control; color: %3;
                      font-family: 'Lora','Crimson Text',serif; font-size: 17px; font-weight: 700; }
        QToolButton:hover { background: %4; color: %5; }
        QToolButton:checked { background: %6; border-color: %3; color: %5; }
    )")).arg(rgba(c, 0.13), rgba(c, 0.45), c.name(), rgba(c, 0.22), Theme::textBright(), Theme::pressedOverlay());
}

QString dimQss(const QColor& c) {
    return Theme::qss(QStringLiteral(R"(
        QToolButton { background: transparent; border: 1px solid transparent; border-radius: @radius-control; color: %1;
                      font-family: 'Lora','Crimson Text',serif; font-size: 17px; font-weight: 700; }
        QToolButton:hover { background: %2; border-color: %3; color: %4; }
        QToolButton:checked { background: %5; border-color: %4; color: %6; }
    )")).arg(rgba(c, 0.38), Theme::hoverOverlay(), Theme::subtleBorder(), c.name(), Theme::pressedOverlay(), Theme::textBright());
}

QColor mixColors(const QColor& a, const QColor& b, qreal t) {
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, 1.0);
}

QString shortToolName(const QString& id) {
    if (id == QLatin1String("info"))        return LeftBar::tr("Informações");
    if (id == QLatin1String("whiteboard"))  return LeftBar::tr("Lousa");
    if (id == QLatin1String("timeline"))    return LeftBar::tr("Linha do tempo");
    if (id == QLatin1String("worldmap"))    return LeftBar::tr("Mapa");
    if (id == QLatin1String("manuscripts")) return LeftBar::tr("Manuscritos");
    if (id == QLatin1String("outline"))     return LeftBar::tr("Outline");
    if (id == QLatin1String("groups"))      return LeftBar::tr("Grupos");
    return QString();
}

QString groupTitle(const QString& gid) {
    if (gid == QLatin1String("project"))  return LeftBar::tr("Projeto");
    if (gid == QLatin1String("planning")) return LeftBar::tr("Planejamento");
    if (gid == QLatin1String("writing"))  return LeftBar::tr("Escrita");
    return QString();
}

QFrame* makeGroupSeparator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setStyleSheet(separatorQss());
    return line;
}

} // namespace

// ============================================================ Etiquetas
// Passando o mouse na barra, uma faixa sai da borda dela com o nome de cada
// botão (alinhado a ele), o título dos grupos em cima dos separadores e
// quantos itens cada gaveta tem. A barra de verdade continua embaixo, inteira:
// arrasto de gaveta, reorganizar e menus seguem funcionando nela.

// A faixa de nomes mora em LeftBarLabels.h (a RightBar usa a mesma).

int LeftBar::barWidth() {
    return barWidthPx();
}

LeftBar::BarStyle LeftBar::savedStyle() {
    return BarStyle(qBound(0, QSettings().value(QStringLiteral("ui/leftBarStyle"), 0).toInt(), kLastStyle));
}

LeftBar::BarLength LeftBar::savedLength() {
    return QSettings().value(QStringLiteral("ui/leftBarLength"), 0).toInt() == 1 ? BarLength::Plate : BarLength::Full;
}

LeftBar::LeftBar(ProjectModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_drawerLayout(nullptr)
    , m_rootLayout(nullptr)
{
    setObjectName(QStringLiteral("leftBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    m_style = savedStyle();
    m_length = savedLength();
    m_namedNarrow = QSettings().value(QStringLiteral("ui/leftBarNamedNarrow"), false).toBool();
    setFixedWidth(barWidthPx());
    setStyleSheet(panelStyle());
    setAcceptDrops(true);

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(sidePadPx(), 10, sidePadPx(), 10);
    m_rootLayout->setSpacing(4);

    // ---- Botões fixos: Projeto / Planejamento / Escrita ----
    // Cada bloco é um ToolbarGroupWidget; a composição (ordem dos grupos e
    // quais botões cada um tem) vem do QSettings e é montada em
    // rebuildGroupLayout(), que também cuida dos separadores.
    makeFixedButton(QStringLiteral("projectinfo.svg"), QStringLiteral("i"),
                    tr("Informações do projeto"), Info);
    makeFixedButton(QStringLiteral("whiteboard.svg"), QStringLiteral("L"),
                    tr("Lousa de planejamento"), Whiteboard);
    makeFixedButton(QStringLiteral("timeline.svg"), QStringLiteral("T"),
                    tr("Linha do tempo"), Timeline);
    makeFixedButton(QStringLiteral("worldmap.svg"), QStringLiteral("M"),
                    tr("Mapa do mundo"), WorldMap);
    makeFixedButton(QStringLiteral("manuscriptpanel.svg"), QStringLiteral("T"),
                    tr("Manuscritos"), Manuscripts);
    makeFixedButton(QStringLiteral("outline.svg"), QStringLiteral("O"),
                    tr("Outline"), Outline);
    makeFixedButton(QStringLiteral("groups.svg"), QStringLiteral("G"),
                    tr("Grupos"), Groups);

    m_groupsLayout = new QVBoxLayout();
    m_groupsLayout->setContentsMargins(0, 0, 0, 0);
    m_groupsLayout->setSpacing(4);
    m_rootLayout->addLayout(m_groupsLayout);

    buildGroups();
    const QStringList groupIds = m_groupWidgets.keys();
    const QStringList buttonIds = m_buttonsById.keys();
    m_groupOrder = ToolbarLayout::loadGroupOrder(QStringLiteral("ui/leftBarGroupOrder"), defaultGroupOrder());
    m_groupButtons = ToolbarLayout::loadButtonLayout(QStringLiteral("ui/leftBarButtonLayout"),
                                                     defaultButtonLayout(),
                                                     QSet<QString>(groupIds.begin(), groupIds.end()),
                                                     QSet<QString>(buttonIds.begin(), buttonIds.end()));
    rebuildGroupLayout();

    // ---- Botão "+" (nova gaveta) — fica logo acima da lista, igual Mira 1
    m_newDrawerBtn = makeNewDrawerButton();
    m_rootLayout->addWidget(m_newDrawerBtn);

    // ---- Gavetas ----
    // Moram num carrossel que ocupa o vão até a persiana: cabendo, ficam
    // empilhadas no topo como sempre; passando da altura, a lista gira.
    m_drawerContent = new QWidget(this);
    m_drawerContent->setObjectName(QStringLiteral("leftBarDrawers"));
    m_drawerLayout = new QVBoxLayout(m_drawerContent);
    m_drawerLayout->setContentsMargins(0, 0, 0, 0);
    m_drawerLayout->setSpacing(4);
    m_drawerCarousel = new CarouselViewport(Qt::Vertical, this);
    m_drawerCarousel->setContent(m_drawerContent);
    // A faixa de etiquetas é continuação da barra: andar nela também gira.
    m_drawerCarousel->extraHover = [this](const QPoint& g) {
        return m_labels && m_labels->isVisible() && m_labels->rect().contains(m_labels->mapFromGlobal(g));
    };
    connect(m_drawerCarousel, &CarouselViewport::scrolled, this, [this]() {
        if (m_labels && m_labels->isVisible()) refreshLabels();
    });
    m_rootLayout->addWidget(m_drawerCarousel, 1);

    // ---- Tambor de gavetas (estilo) ----
    m_drum = new LeftBarDrum(this);
    m_drum->hide();
    // Logo embaixo do "+" (pedido do Pe, 14/11): altura própria, sem esticar;
    // o vão que sobra fica embaixo dele.
    m_drum->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
    m_rootLayout->addWidget(m_drum, 0, Qt::AlignHCenter);
    connect(m_drum, &LeftBarDrum::activated, this, [this](const QString& key) { emit drawerSelected(key); });
    connect(m_drum, &LeftBarDrum::contextRequested, this, &LeftBar::drawerContextRequested);
    connect(m_drum, &LeftBarDrum::centerChanged, this, [this]() { updateDrumTag(true); });
    connect(m_drum, &LeftBarDrum::moved, this, [this]() { if (m_drumTag && m_drumTag->isVisible()) updateFloaters(); });

    // ---- Barra com nomes (estilo): abre por cima da mesa, fora do layout ----
    m_named = new LeftBarNamed(this);
    m_named->setObjectName(QStringLiteral("leftBarNamed"));
    m_named->setAttribute(Qt::WA_StyledBackground, true);
    m_named->hide();
    connect(m_named, &LeftBarNamed::emptyContextRequested, this, [this](const QPoint& g) { showStyleMenu(g); });
    connect(m_named, &LeftBarNamed::infoClicked, this, [this]() { if (QToolButton* b = m_fixedButtons.value(Info)) b->click(); });
    connect(m_named, &LeftBarNamed::toolClicked, this, [this](const QString& id) { if (QToolButton* b = m_buttonsById.value(id)) b->click(); });
    connect(m_named, &LeftBarNamed::openDrawerRequested, this, [this](const QString& key) { emit drawerSelected(key); });
    connect(m_named, &LeftBarNamed::itemActivated, this, &LeftBar::drawerItemActivated);
    connect(m_named, &LeftBarNamed::newDrawerClicked, this, &LeftBar::newDrawerRequested);
    connect(m_named, &LeftBarNamed::drawerContextRequested, this, &LeftBar::drawerContextRequested);
    connect(m_named, &LeftBarNamed::collapseClicked, this, [this]() { setNamedNarrow(true, true); });
    connect(m_named, &LeftBarNamed::contentChanged, this, [this]() { updatePlateGeometry(); });

    // Recolhida na placa, só o botão do pé fica: este vão empurra ele pro pé.
    m_rootLayout->addStretch(0);

    // ---- Persiana: o botão do pé recolhe a barra pra baixo ----
    m_collapseBtn = new QToolButton(this);
    m_collapseBtn->setCursor(Qt::PointingHandCursor);
    m_collapseBtn->setAutoRaise(true);
    m_collapseBtn->setStyleSheet(fixedButtonQss());
    connect(m_collapseBtn, &QToolButton::clicked, this, [this]() {
        // Na Barra com nomes recolhida, o botão do pé abre os nomes de volta.
        if (m_style == BarStyle::Named) { setNamedNarrow(false, true); return; }
        setCollapsed(!m_collapsed);
    });
    m_rootLayout->addWidget(m_collapseBtn, 0, Qt::AlignHCenter);

    m_sceneCardTimer = new QTimer(this);
    m_sceneCardTimer->setSingleShot(true);
    m_sceneCardTimer->setInterval(240);
    connect(m_sceneCardTimer, &QTimer::timeout, this, [this]() { if (m_sceneCardBtn) showSceneCard(m_sceneCardBtn); });

    if (m_model) {
        connect(m_model, &ProjectModel::drawersChanged, this, &LeftBar::rebuildDrawerButtons);
        connect(m_model, &ProjectModel::loaded, this, &LeftBar::rebuildDrawerButtons);
        connect(m_model, &ProjectModel::projectNameChanged, this, &LeftBar::refreshNamed);
        rebuildDrawerButtons();
    }

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &LeftBar::applyTheme);
    connect(UiScale::Manager::instance(), &UiScale::Manager::scaleChanged,
            this, &LeftBar::applyUiScale);
    applyUiScale(); // aplica a escala persistida (botões/ícones no tamanho certo)
    applyStyle();

    // A persiana não vale na Barra com nomes (lá o pé recolhe os nomes).
    if (m_style != BarStyle::Named && QSettings().value(QStringLiteral("ui/leftBarCollapsed"), false).toBool())
        setCollapsed(true, /*animated=*/false);
    refreshCollapseButton();
}

// ============================================================ Estilos

int LeftBar::targetWidth() const {
    return barWidthPx();   // a Barra com nomes cresce por cima (placeNamed), não no layout
}

void LeftBar::setBarStyle(BarStyle style) {
    if (m_style == style) return;
    QSettings().setValue(QStringLiteral("ui/leftBarStyle"), int(style));
    if (m_widthAnim) { QVariantAnimation* a = m_widthAnim; m_widthAnim.clear(); a->stop(); }
    m_namedReveal = -1;
    setEditMode(false);
    if (style == BarStyle::Named && m_collapsed) setCollapsed(false, /*animated=*/false);
    m_style = style;
    applyStyle();
    emit barStyleChanged(m_style);
}

void LeftBar::setBarLength(BarLength length) {
    if (m_length == length) return;
    QSettings().setValue(QStringLiteral("ui/leftBarLength"), int(length));
    // A altura da placa só se mede aberta: recolhida, abre, troca e recolhe.
    if (m_rolling) finishRoll();
    const bool wasCollapsed = m_collapsed;
    if (wasCollapsed) setCollapsed(false, /*animated=*/false);
    m_length = length;
    if (!isPlate()) {
        setPlateVisible(-1);
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
    }
    updatePlateGeometry();
    if (wasCollapsed) setCollapsed(true, /*animated=*/false);
    applyTheme();
    updateFloaters();
    emit barLengthChanged(m_length);
}

void LeftBar::applyStyle() {
    hideSceneCard();
    hideLabels(false);
    setFixedWidth(targetWidth());
    if (m_rootLayout) m_rootLayout->setContentsMargins(sidePadPx(), 10, sidePadPx(), 10);
    if (m_named) {
        m_named->setMirrored(m_barSide == Qt::RightEdge);
        m_named->setStyleSheet(panelStyle(QStringLiteral("leftBarNamed")));
    }
    rebuildDrawerButtons();   // veste as gavetas, põe o "+" no lugar, atualiza tambor e nomes
    applyContentVisibility();
    refreshCollapseButton();
    updatePlateGeometry();
    updateFloaters();
    emit widthChanged();
}

void LeftBar::setNamedNarrow(bool narrow, bool animated) {
    if (m_style != BarStyle::Named) { m_namedNarrow = narrow; return; }
    if (m_namedNarrow == narrow && m_namedReveal < 0) return;
    QSettings().setValue(QStringLiteral("ui/leftBarNamedNarrow"), narrow);
    if (m_widthAnim) { QVariantAnimation* a = m_widthAnim; m_widthAnim.clear(); a->stop(); }
    hideLabels(false);
    if (!animated || !isVisible() || !PanelMotion::enabled() || m_chromeHidden) {
        m_namedNarrow = narrow;
        m_namedReveal = -1;
        applyStyle();
        return;
    }
    // Durante a animação a barra com nomes fica à mostra por cima e a largura
    // corta o que sobra; no fim, recolhida, volta a barra de sempre.
    const qreal from = m_namedReveal >= 0 ? m_namedReveal : (narrow ? 1.0 : 0.0);
    m_namedNarrow = narrow;
    m_namedReveal = from;
    refreshNamed();
    applyContentVisibility();
    placeNamed();
    auto* a = new QVariantAnimation(this);
    a->setDuration(320);
    a->setStartValue(from);
    a->setEndValue(narrow ? 0.0 : 1.0);
    a->setEasingCurve(QEasingCurve::OutCubic);
    connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_namedReveal = v.toReal();
        placeNamed();
        emit widthChanged();
    });
    connect(a, &QVariantAnimation::finished, this, [this]() {
        m_namedReveal = -1;
        m_widthAnim.clear();
        applyStyle();
    });
    m_widthAnim = a;
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

QList<LeftBarDrawerInfo> LeftBar::drawerInfos() const {
    QList<LeftBarDrawerInfo> out;
    if (!m_model) return out;
    for (const auto& d : m_model->drawers()) {
        LeftBarDrawerInfo i;
        i.key = d.key;
        i.title = d.title;
        i.color = d.color.isEmpty() ? Theme::accentDefault() : d.color;
        i.iconId = !d.drawerIcon.isEmpty() ? d.drawerIcon : d.drawerElementIcon;
        i.count = d.items.size();
        for (const auto& it : d.items)
            i.items.append({ it.id, it.title.isEmpty() ? tr("Sem título") : it.title });
        out.append(i);
    }
    return out;
}

void LeftBar::refreshNamed() {
    if (!m_named || m_style != BarStyle::Named) return;
    QList<LeftBarNamed::Group> groups;
    for (const QString& gid : std::as_const(m_groupOrder)) {
        LeftBarNamed::Group g;
        g.title = groupTitle(gid);
        for (const QString& bid : m_groupButtons.value(gid)) {
            if (bid == QLatin1String("info")) continue;   // o projeto, lá em cima
            QToolButton* b = m_buttonsById.value(bid);
            if (!b) continue;
            LeftBarNamed::Tool t;
            t.id = bid;
            t.text = shortToolName(bid);
            t.iconRes = QStringLiteral(":/icons/leftbar/%1").arg(b->property("iconRes").toString());
            t.active = b->isChecked();
            g.tools.append(t);
        }
        groups.append(g);
    }
    m_named->setGroups(groups);
    QString title, sub;
    if (m_model) {
        title = m_model->projectName();
        const QString genres = m_model->projectGenres();
        const QString author = m_model->projectAuthor();
        sub = !genres.isEmpty() ? genres : (!author.isEmpty() ? tr("de %1").arg(author) : tr("Informações do projeto"));
    }
    m_named->setProject(title, sub, m_activeFixed == Info);
    m_named->setDrawers(drawerInfos(), m_activeDrawer);
}

void LeftBar::refreshDrum() {
    if (!m_drum || m_style != BarStyle::Drum) return;
    m_drum->setDrawers(drawerInfos());
    m_drum->setActiveKey(m_activeDrawer);
    updateDrumTag(false);
}

void LeftBar::updateDrumTag(bool animate) {
    if (m_style == BarStyle::Drum && m_drum && host()) {
        if (const LeftBarDrawerInfo* d = m_drum->centerInfo()) {
            if (!m_drumTag) m_drumTag = new LeftBarTag(host());
            m_drumTag->setInfo(d->title, d->color, d->count, m_barSide == Qt::RightEdge, animate);
        }
    }
    updateFloaters();
}

// Tudo que sai da barra pra fora (lingueta do tambor, fio da gaveta com cor)
// segue a barra e some quando ela some.
void LeftBar::updateFloaters() {
    QWidget* h = host();
    // A barra nasce filha da janela e só depois vai pro container: os
    // bilhetes acompanham o pai de verdade.
    for (QWidget* w : { static_cast<QWidget*>(m_drumTag.data()), static_cast<QWidget*>(m_thread.data()),
                        static_cast<QWidget*>(m_sceneCard.data()) })
        if (w && h && w->parentWidget() != h) w->setParent(h);
    placeNamed();
    const bool barUp = h && isVisible() && classicShown();
    const bool labelsUp = m_labels && m_labels->isVisible();
    const bool mirrored = (m_barSide == Qt::RightEdge);
    const QRect g = geometry();

    // Lingueta do tambor: só com nada aberto ao lado da barra.
    const bool tagOn = barUp && m_style == BarStyle::Drum && m_drum && m_drum->isVisible() && m_drum->centerInfo()
                       && m_activeDrawer.isEmpty() && m_activeFixed < 0 && !labelsUp && !m_editMode;
    if (tagOn && m_drumTag) {
        const int cy = m_drum->mapTo(h, QPoint(0, m_drum->centerY())).y();
        m_drumTag->placeAt(mirrored ? g.left() : g.right(), cy);
        m_drumTag->raise();
        m_drumTag->show();
    } else if (m_drumTag) {
        m_drumTag->hide();
    }

    // Fio: do ladrilho aceso até a beira do painel (que abre a 10 px da barra).
    QToolButton* active = m_activeDrawer.isEmpty() ? nullptr : m_drawerButtons.value(m_activeDrawer);
    const bool threadOn = barUp && m_style == BarStyle::Colors && active && active->isVisible()
                          && drawerInCarouselWindow(active);
    if (threadOn) {
        if (!m_thread) m_thread = new LeftBarThread(h);
        const QRect br(active->mapTo(h, QPoint(0, 0)), active->size());
        const int th = qMax(2, qRound(2 * UiScale::scale()));
        const int gap = qRound(4 * UiScale::scale());
        int x0, x1;
        if (!mirrored) { x0 = br.right() + 1 + gap; x1 = g.right() + 1 + 10; }
        else           { x0 = g.left() - 10;        x1 = br.left() - gap; }
        const Drawer* d = m_model ? m_model->findDrawer(m_activeDrawer) : nullptr;
        m_thread->setColor(QColor(d && !d->color.isEmpty() ? d->color : Theme::accentDefault()));
        m_thread->setGeometry(x0, br.center().y() - th / 2 + 1, qMax(0, x1 - x0), th);
        m_thread->raise();
        m_thread->show();
    } else if (m_thread) {
        m_thread->hide();
    }
}

bool LeftBar::drawerInScene(const QString& drawerKey, int* count) const {
    const int n = sceneItems(drawerKey).size();
    if (count) *count = n;
    return n > 0;
}

QList<QPair<QString, QString>> LeftBar::sceneItems(const QString& drawerKey) const {
    QList<QPair<QString, QString>> out;
    if (!m_model || m_sceneElements.isEmpty()) return out;
    const Drawer* d = m_model->findDrawer(drawerKey);
    if (!d) return out;
    for (const auto& it : d->items)
        if (!it.elementId.isEmpty() && m_sceneElements.contains(it.elementId))
            out.append({ it.id, it.title.isEmpty() ? tr("Sem título") : it.title });
    return out;
}

void LeftBar::setSceneElements(const QStringList& elementIds, const QString& heading) {
    if (m_sceneElements == elementIds && m_sceneHeading == heading) return;
    m_sceneElements = elementIds;
    m_sceneHeading = heading;
    if (m_style == BarStyle::Scene) {
        hideSceneCard();
        rebuildDrawerButtons();
    }
}

void LeftBar::styleDrawerButton(QToolButton* btn, const Drawer& d) {
    const QString accent = d.color.isEmpty() ? Theme::accentDefault() : d.color;
    const QColor col(accent);
    const QString iconId = !d.drawerIcon.isEmpty() ? d.drawerIcon : d.drawerElementIcon;
    const QString path = QStringLiteral(":/icons/elements/%1.svg").arg(iconId);
    if (m_style == BarStyle::Colors) {
        const int t = tilePx();
        btn->setFixedSize(t, t);
        btn->setStyleSheet(tileQss(col));
        if (!iconId.isEmpty()) {
            const int is = qMax(12, qRound(iconSizePx() * 0.66));
            const QIcon ic = IconUtils::loadToolbarIcon(path, col, col, QColor(Qt::white), QSize(is, is));
            if (!ic.isNull()) { btn->setIcon(ic); btn->setIconSize(QSize(is, is)); }
        }
        return;
    }
    if (m_style == BarStyle::Scene && sceneContext() && !d.drawerElementType.isEmpty()) {
        int n = 0;
        if (drawerInScene(d.key, &n)) {
            btn->setStyleSheet(litQss(col));
            auto* badge = new QLabel(QString::number(n), btn);
            badge->setAttribute(Qt::WA_TransparentForMouseEvents);
            badge->setAlignment(Qt::AlignCenter);
            const int bh = qMax(12, qRound(15 * UiScale::scale()));
            QFont f(QStringLiteral("Segoe UI"));
            f.setPixelSize(qMax(8, qRound(9.5 * UiScale::scale())));
            f.setBold(true);
            badge->setFont(f);
            const int bw = qMax(bh, QFontMetrics(f).horizontalAdvance(badge->text()) + qRound(7 * UiScale::scale()));
            badge->setStyleSheet(QStringLiteral("background: %1; color: #ffffff; border-radius: %2px;").arg(accent).arg(bh / 2));
            badge->setGeometry(btn->width() - bw, btn->height() - bh, bw, bh);
        } else {
            btn->setStyleSheet(dimQss(col));
            if (!iconId.isEmpty()) {
                QColor dim = col;
                dim.setAlphaF(0.38f);
                const QIcon ic = IconUtils::loadToolbarIcon(path, dim, col, QColor(Theme::textBright()),
                                                            QSize(iconSizePx(), iconSizePx()));
                if (!ic.isNull()) btn->setIcon(ic);
            }
        }
    }
}

void LeftBar::placeNewDrawerButton() {
    if (!m_newDrawerBtn) return;
    if (m_style == BarStyle::Colors) {
        if (m_rootLayout->indexOf(m_newDrawerBtn) >= 0) m_rootLayout->removeWidget(m_newDrawerBtn);
        m_newDrawerBtn->setFixedSize(tilePx(), tilePx());
        m_newDrawerBtn->setStyleSheet(newTileQss());
        m_drawerLayout->addWidget(m_newDrawerBtn, 0, Qt::AlignHCenter);
    } else {
        m_newDrawerBtn->setFixedSize(btnSizePx(), btnSizePx());
        m_newDrawerBtn->setStyleSheet(newDrawerQss());
        if (m_rootLayout->indexOf(m_newDrawerBtn) < 0)
            m_rootLayout->insertWidget(m_rootLayout->indexOf(m_drawerCarousel), m_newDrawerBtn);
    }
    m_newDrawerBtn->setVisible(classicShown());
}

void LeftBar::showSceneCard(QToolButton* btn) {
    QWidget* h = host();
    if (!h || !btn || m_style != BarStyle::Scene || !sceneContext() || !classicShown() || !btn->isVisible()) return;
    const QString key = btn->property("drawerKey").toString();
    const Drawer* d = m_model ? m_model->findDrawer(key) : nullptr;
    if (!d || d->drawerElementType.isEmpty()) return;
    QStringList names;
    for (const auto& it : sceneItems(key)) names << it.second;
    if (!m_sceneCard) m_sceneCard = new LeftBarSceneCard(h);
    if (m_sceneCard->parentWidget() != h) m_sceneCard->setParent(h);
    m_sceneCard->setContent(d->title, d->color.isEmpty() ? Theme::accentDefault() : d->color, m_sceneHeading, names);
    const bool mirrored = (m_barSide == Qt::RightEdge);
    const int gap = qRound(8 * UiScale::scale());
    int x;
    if (m_labels && m_labels->isVisible()) {
        const QRect lg = m_labels->geometry();
        x = mirrored ? lg.left() + kLabelsShadow - gap : lg.right() - kLabelsShadow + gap;
    } else {
        x = mirrored ? geometry().left() - gap : geometry().right() + gap;
    }
    const int cy = btn->mapTo(h, QPoint(0, btn->height() / 2)).y();
    m_sceneCard->placeAt(x, cy, mirrored);
    m_sceneCardBtn = btn;
    m_sceneCard->raise();
    m_sceneCard->show();
}

void LeftBar::hideSceneCard() {
    if (m_sceneCardTimer) m_sceneCardTimer->stop();
    if (m_sceneCard) m_sceneCard->hide();
}

void LeftBar::contextMenuEvent(QContextMenuEvent* event) {
    if (m_editMode) { event->ignore(); return; }
    showStyleMenu(event->globalPos());
}

void LeftBar::showStyleMenu(const QPoint& globalPos) {
    hideLabels(false);
    QMenu menu(this);
    menu.setStyleSheet(Theme::qss(QStringLiteral(R"(
        QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: @radius-panel; padding: 4px; }
        QMenu::item { padding: 6px 16px 6px 26px; border-radius: @radius-item; }
        QMenu::item:selected { background: %4; color: %5; }
        QMenu::item:disabled { color: %6; }
        QMenu::indicator { left: 8px; }
        QMenu::separator { height: 1px; background: %3; margin: 4px 6px; }
    )")).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
             Theme::hoverOverlay(), Theme::textBright(), Theme::textMuted()));
    QAction* head = menu.addAction(tr("Estilo da barra lateral"));
    head->setEnabled(false);
    auto* styles = new QActionGroup(&menu);
    const QList<QPair<BarStyle, QString>> list = {
        { BarStyle::Classic, tr("Clássica") },
        { BarStyle::Colors,  tr("Gavetas com cor") },
        { BarStyle::Named,   tr("Barra com nomes") },
        { BarStyle::Drum,    tr("Tambor de gavetas") },
        { BarStyle::Scene,   tr("Lê a cena") },
    };
    for (const auto& it : list) {
        QAction* a = menu.addAction(it.second);
        a->setCheckable(true);
        a->setChecked(m_style == it.first);
        styles->addAction(a);
        const BarStyle st = it.first;
        connect(a, &QAction::triggered, this, [this, st]() { setBarStyle(st); });
    }
    menu.addSeparator();
    QAction* head2 = menu.addAction(tr("Comprimento"));
    head2->setEnabled(false);
    auto* lengths = new QActionGroup(&menu);
    const QList<QPair<BarLength, QString>> lens = {
        { BarLength::Full,  tr("De ponta a ponta") },
        { BarLength::Plate, tr("Placa") },
    };
    for (const auto& it : lens) {
        QAction* a = menu.addAction(it.second);
        a->setCheckable(true);
        a->setChecked(m_length == it.first);
        lengths->addAction(a);
        const BarLength ln = it.first;
        connect(a, &QAction::triggered, this, [this, ln]() { setBarLength(ln); });
    }
    menu.exec(globalPos);
}

// ============================================================ Placa

int LeftBar::plateHeight() const {
    int avail = 0;
    if (QWidget* p = parentWidget()) {
        if (QLayout* l = p->layout()) avail = l->contentsRect().height();
        if (avail <= 0) avail = p->height() - 20;
    }
    if (avail <= 0) avail = height();
    int need = 0;
    if (namedWide() && m_named) need = m_named->sizeHint().height();
    else if (m_rootLayout) need = m_rootLayout->sizeHint().height();
    return qBound(collapsedHeight(), need, qMax(collapsedHeight(), avail));
}

void LeftBar::updatePlateGeometry() {
    if (!isPlate()) {
        if (m_plateVisibleH >= 0) setPlateVisible(-1);
        if (!m_collapsed && !m_rolling) { setMinimumHeight(0); setMaximumHeight(QWIDGETSIZE_MAX); }
        syncLayoutAlignment();
        return;
    }
    if (m_rolling) return;
    // Só dá pra medir com o conteúdo à mostra; recolhida ou no modo focado,
    // vale a última medida. Os layouts guardam a medida de quando os botões
    // estavam escondidos: invalida tudo antes de medir.
    if (contentShown()) {
        // Cada item de layout guarda a medida do seu widget; quem acabou de
        // reaparecer ainda conta como vazio até medir de novo.
        for (QWidget* w : findChildren<QWidget*>()) w->updateGeometry();
        for (QLayout* l : findChildren<QLayout*>()) l->invalidate();
        if (m_rootLayout) m_rootLayout->invalidate();
        m_plateCachedH = plateHeight();
    }
    if (m_plateCachedH <= 0) m_plateCachedH = collapsedHeight();
    setFixedHeight(qMax(collapsedHeight(), m_plateCachedH));
    if (m_collapsed) setPlateVisible(collapsedHeight());
    syncLayoutAlignment();
    updateFloaters();
    // Uma segunda medida depois que os layouts assentarem (botões que acabaram
    // de aparecer só entram na conta no giro seguinte).
    if (!m_platePassQueued && contentShown()) {
        m_platePassQueued = true;
        QTimer::singleShot(0, this, [this]() {
            m_platePassQueued = false;
            remeasurePlate();
        });
    }
}

// Medida leve (sem mandar ninguém se medir de novo): o carrossel e os grupos
// assentam um giro depois, e avisam com um LayoutRequest na barra.
void LeftBar::remeasurePlate() {
    if (!isPlate() || m_rolling || !contentShown()) return;
    const int h = plateHeight();
    if (h == m_plateCachedH) return;
    m_plateCachedH = h;
    setFixedHeight(qMax(collapsedHeight(), m_plateCachedH));
    updateFloaters();
}

bool LeftBar::event(QEvent* e) {
    const bool r = QWidget::event(e);
    if (e->type() == QEvent::LayoutRequest) remeasurePlate();
    return r;
}

void LeftBar::setPlateVisible(int h) {
    m_plateVisibleH = h;
    if (h < 0) clearMask();
    else setMask(QRegion(visibleBox()));
    update();
}

QRect LeftBar::visibleBox() const {
    if (m_plateVisibleH < 0) return rect();
    const int h = qMin(height(), m_plateVisibleH);
    return QRect(0, height() - h, width(), h);
}

void LeftBar::applyUiScale() {
    if (m_namedReveal < 0) setFixedWidth(targetWidth());
    if (m_rootLayout) {

        m_rootLayout->setContentsMargins(sidePadPx(), 10, sidePadPx(), 10);
    }
    if (m_drum) m_drum->refreshLook();
    if (m_named) m_named->refreshLook();
    const int btn = btnSizePx();
    const int ico = iconSizePx();

    for (auto it = m_fixedButtons.constBegin(); it != m_fixedButtons.constEnd(); ++it) {
        auto* b = it.value();
        if (!b) continue;
        b->setFixedSize(btn, btn);
        const QString res = b->property("iconRes").toString();
        if (!res.isEmpty()) {
            const QIcon ic = loadLeftbarIcon(res);
            if (!ic.isNull()) {
                b->setIcon(ic);
                b->setIconSize(QSize(ico, ico));
            }
        }
    }
    refreshCollapseButton();
    if (m_collapsed && !m_rolling && !isPlate()) setFixedHeight(collapsedHeight());
    rebuildDrawerButtons(); // recria os botões de gaveta já no tamanho novo
    updatePlateGeometry();
}

QString LeftBar::panelStyle(const QString& objectName) const {
    QString qss = Theme::panelQss(objectName, Theme::PanelKey::LeftBar);
    if (m_joinedTop && !m_collapsed) {
        const bool right = (m_barSide == Qt::RightEdge);
        qss += QStringLiteral("#%2 { border-top: none; border-top-left-radius: 0px; border-top-right-radius: 0px; %1 }")
                   .arg(right ? QStringLiteral("border-right: none;") : QStringLiteral("border-left: none;"), objectName);
    }
    return qss;
}

void LeftBar::setJoinedTop(bool on) {
    if (m_joinedTop == on) return;
    m_joinedTop = on;
    if (!m_chromeHidden && !m_rolling) setStyleSheet(panelStyle());
    if (m_named) m_named->setStyleSheet(panelStyle(QStringLiteral("leftBarNamed")));
}

// A Barra com nomes abre por cima da mesa, como as gavetas: a barra de
// verdade continua com 46 px no layout e a folha não se mexe (pedido do Pe,
// 14/11: a folha deslizando "não curti").
void LeftBar::placeNamed() {
    if (!m_named) return;
    QWidget* h = host();
    if (h && m_named->parentWidget() != h) m_named->setParent(h);
    if (!h || !isVisible() || !contentShown() || !namedWide()) { m_named->hide(); return; }
    const int wN = barWidthPx(), wW = LeftBarNamed::designWidth();
    const int w = m_namedReveal >= 0 ? qRound(wN + (wW - wN) * m_namedReveal) : wW;
    const QRect g = geometry();
    const int x = (m_barSide == Qt::RightEdge) ? g.right() + 1 - w : g.left();
    m_named->setGeometry(x, g.top(), w, g.height());
    m_named->raise();
    m_named->show();
}

QRect LeftBar::outerGeometry() const {
    if (m_named && m_named->isVisible() && m_named->parentWidget() == parentWidget()) return m_named->geometry();
    return geometry();
}

void LeftBar::applyTheme() {
    // Recolhida na placa, quem pinta a alcinha é o paintEvent (a máscara
    // cortaria a borda de cima da QSS).
    if (m_chromeHidden || m_rolling || m_plateVisibleH >= 0)
        setStyleSheet(QStringLiteral("#leftBar { background: transparent; border: none; }"));
    else
        setStyleSheet(panelStyle());
    if (m_drum) m_drum->refreshLook();
    if (m_named) { m_named->refreshLook(); m_named->setStyleSheet(panelStyle(QStringLiteral("leftBarNamed"))); }
    refreshCollapseButton();
    for (auto it = m_fixedButtons.constBegin(); it != m_fixedButtons.constEnd(); ++it) {
        if (auto* btn = it.value()) {
            btn->setStyleSheet(fixedButtonQss());
            // Re-tinta o ícone com as cores do novo tema.
            const QString res = btn->property("iconRes").toString();
            if (!res.isEmpty()) {
                const QIcon ic = loadLeftbarIcon(res);
                if (!ic.isNull()) btn->setIcon(ic);
            }
        }
    }
    for (auto* sep : m_groupSeparators) {
        if (sep) sep->setStyleSheet(separatorQss());
    }
    for (ToolbarGroupWidget* g : std::as_const(m_groupWidgets)) {
        if (g) g->applyTheme(QColor(Theme::textMuted()), QColor(Theme::accentDefault()));
    }
    // Recria botões de gaveta pra refletirem accent/textBright atualizados.
    rebuildDrawerButtons();
}

// A barra em si nao muda nada ao trocar de lado (ela e vertical dos dois
// lados); quem reposiciona e o MainWindow, ouvindo barSideChanged. Guardar o
// lado aqui existe pra que chromeInset() e os paineis flutuantes possam
// perguntar "de que lado voce esta?" em vez de assumir a esquerda.
void LeftBar::setBarSide(Qt::Edge side)
{
    if (m_barSide == side) return;
    m_barSide = side;
    if (m_named) m_named->setMirrored(side == Qt::RightEdge);
    updateDrumTag(false);
    emit barSideChanged(m_barSide);
}

void LeftBar::setChromeHidden(bool hidden) {
    if (hidden) hideLabels(false);
    // Modo focado recolhendo a barra no meio de uma reorganização: sai da
    // edição antes, senão os grupos voltariam tracejados quando ela reaparecer.
    if (hidden) setEditMode(false);
    // Persiana no meio do caminho: termina já, senão a foto fica pintada por
    // cima da barra transparente do modo focado.
    if (m_rolling) finishRoll();
    m_chromeHidden = hidden;
    if (hidden) hideSceneCard();
    applyContentVisibility();
    if (hidden || m_plateVisibleH >= 0)
        setStyleSheet(QStringLiteral("#leftBar { background: transparent; }"));
    else
        setStyleSheet(panelStyle());
    update();
    updateFloaters();
}

void LeftBar::setMirrored(bool mirrored) {
    if (m_mirrored == mirrored) return;
    m_mirrored = mirrored;
    applyMirrorStyle();
}

void LeftBar::applyMirrorStyle() {
    // O lado físico (esquerda/direita) na janela é decidido pelo MainWindow.
    // Aqui o painel tem borda completa (radius); por ora nada muda visualmente.
    setStyleSheet(panelStyle());
}

void LeftBar::setActiveFixedAction(FixedAction action) {
    m_activeFixed = static_cast<int>(action);
    m_activeDrawer.clear();
    refreshActiveStates();
}

void LeftBar::setActiveDrawer(const QString& drawerKey) {
    m_activeDrawer = drawerKey;
    m_activeFixed = -1;
    refreshActiveStates();
    // Gaveta aberta por outro caminho (busca, menu): aparece no carrossel.
    if (m_drawerCarousel)
        if (QToolButton* b = m_drawerButtons.value(drawerKey)) m_drawerCarousel->ensureVisible(b, true);
    if (m_style == BarStyle::Drum && m_drum) m_drum->revealKey(drawerKey);
}

void LeftBar::clearSelection() {
    m_activeFixed = -1;
    m_activeDrawer.clear();
    refreshActiveStates();
}

void LeftBar::refreshActiveStates() {
    for (auto it = m_fixedButtons.constBegin(); it != m_fixedButtons.constEnd(); ++it) {
        if (auto* btn = it.value()) {
            btn->setChecked(it.key() == m_activeFixed);
        }
    }
    for (auto it = m_drawerButtons.constBegin(); it != m_drawerButtons.constEnd(); ++it) {
        if (auto* btn = it.value()) {
            btn->setChecked(it.key() == m_activeDrawer);
        }
    }
    if (m_drum) m_drum->setActiveKey(m_activeDrawer);
    refreshNamed();
    updateFloaters();
}

QToolButton* LeftBar::fixedButton(FixedAction action) const {
    return m_fixedButtons.value(static_cast<int>(action), nullptr);
}

QToolButton* LeftBar::makeFixedButton(const QString& iconResource,
                                     const QString& placeholderLetter,
                                     const QString& tooltip,
                                     FixedAction action) {
    auto* btn = new QToolButton(this);
    btn->setToolTip(tooltip);
    btn->setFixedSize(btnSizePx(), btnSizePx());
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(true);
    btn->setAutoRaise(true);
    btn->setStyleSheet(fixedButtonQss());
    // Guarda o recurso do ícone pra recarregá-lo (recolorido) ao trocar de tema —
    // o IconUtils tinta o SVG no load, então o ícone precisa ser re-tintado.
    btn->setProperty("iconRes", iconResource);

    if (!iconResource.isEmpty()) {
        QIcon ic = loadLeftbarIcon(iconResource);
        if (!ic.isNull()) {
            btn->setIcon(ic);
            btn->setIconSize(QSize(iconSizePx(), iconSizePx()));
            btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        } else {
            btn->setText(placeholderLetter);
            btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        }
    } else {
        btn->setText(placeholderLetter);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    // Os botões são "radio-like" entre si — clicar emite o sinal e o MainWindow
    // decide se abre/fecha. O setChecked é controlado por setActiveFixedAction.
    connect(btn, &QToolButton::clicked, this, [this, action, btn]() {
        // Bloqueia o toggle automático do QToolButton — quem manda no estado é
        // o MainWindow via setActiveFixedAction. Reverte o checked imediato.
        btn->setChecked(m_activeFixed == static_cast<int>(action));
        emit fixedActionTriggered(action);
    });

    m_fixedButtons.insert(static_cast<int>(action), btn);
    return btn;
}

QToolButton* LeftBar::makeDrawerButton(const QString& drawerKey,
                                      const QString& title,
                                      const QString& color,
                                      const QString& iconId) {
    auto* btn = new QToolButton(this);
    btn->setToolTip(title);
    btn->setFixedSize(btnSizePx(), btnSizePx());
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(true);
    btn->setAutoRaise(true);

    const QString accent = color.isEmpty() ? Theme::accentDefault() : color;

    // Se a gaveta tem ícone escolhido, renderiza-o tintado com a cor da gaveta;
    // senão, cai pra letra inicial (fallback).
    bool iconLoaded = false;
    if (!iconId.isEmpty()) {
        QIcon ic = IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/elements/%1.svg").arg(iconId),
            QColor(accent),
            QColor(accent),
            QColor(Theme::textBright()),
            QSize(iconSizePx(), iconSizePx()));
        if (!ic.isNull()) {
            btn->setIcon(ic);
            btn->setIconSize(QSize(iconSizePx(), iconSizePx()));
            btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
            iconLoaded = true;
        }
    }
    if (!iconLoaded) {
        const QString letter = title.isEmpty() ? QStringLiteral("?") : title.left(1).toUpper();
        btn->setText(letter);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    btn->setStyleSheet(drawerButtonQss(accent));

    connect(btn, &QToolButton::clicked, this, [this, drawerKey, btn]() {
        btn->setChecked(m_activeDrawer == drawerKey);
        emit drawerSelected(drawerKey);
    });

    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QToolButton::customContextMenuRequested, this, [this, btn, drawerKey](const QPoint& pos) {
        emit drawerContextRequested(drawerKey, btn->mapToGlobal(pos));
    });

    btn->installEventFilter(this);
    btn->setProperty("drawerKey", drawerKey);

    m_drawerButtons.insert(drawerKey, btn);
    return btn;
}

QToolButton* LeftBar::makeNewDrawerButton() {
    auto* btn = new QToolButton(this);
    btn->setText(QStringLiteral("+"));
    btn->setToolTip(tr("Nova gaveta"));
    btn->setFixedSize(btnSizePx(), btnSizePx());
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(newDrawerQss());
    connect(btn, &QToolButton::clicked, this, &LeftBar::newDrawerRequested);
    return btn;
}

void LeftBar::buildGroups() {
    for (const QString& id : defaultGroupOrder()) {
        auto* g = new ToolbarGroupWidget(id, Qt::Vertical, this);
        g->setSpacing(4); // o mesmo passo que os botões sempre tiveram na barra
        connect(g, &ToolbarGroupWidget::groupDroppedOn, this, &LeftBar::onGroupDropped);
        connect(g, &ToolbarGroupWidget::buttonDroppedOn, this, &LeftBar::onButtonDropped);
        connect(g, &ToolbarGroupWidget::toggleEditModeRequested,
                this, [this]() { setEditMode(!m_editMode); });
        g->applyTheme(QColor(Theme::textMuted()), QColor(Theme::accentDefault()));
        m_groupWidgets.insert(id, g);
    }

    // Ids ESTÁVEIS: é isso que vai pro QSettings. Renomear um id aqui invalida
    // a organização salva do usuário — trate como formato de arquivo.
    m_buttonsById = {
        { QStringLiteral("info"),        m_fixedButtons.value(Info) },
        { QStringLiteral("whiteboard"),  m_fixedButtons.value(Whiteboard) },
        { QStringLiteral("timeline"),    m_fixedButtons.value(Timeline) },
        { QStringLiteral("worldmap"),    m_fixedButtons.value(WorldMap) },
        { QStringLiteral("manuscripts"), m_fixedButtons.value(Manuscripts) },
        { QStringLiteral("outline"),     m_fixedButtons.value(Outline) },
        { QStringLiteral("groups"),      m_fixedButtons.value(Groups) },
    };
}

QHash<QString, QStringList> LeftBar::defaultButtonLayout() const {
    return {
        { QStringLiteral("project"),  { QStringLiteral("info") } },
        { QStringLiteral("planning"), { QStringLiteral("whiteboard"), QStringLiteral("timeline"), QStringLiteral("worldmap") } },
        { QStringLiteral("writing"),  { QStringLiteral("manuscripts"), QStringLiteral("outline"),
                                        QStringLiteral("groups") } },
    };
}

QStringList LeftBar::defaultGroupOrder() const {
    return { QStringLiteral("project"), QStringLiteral("planning"), QStringLiteral("writing") };
}

void LeftBar::rebuildGroupLayout() {
    if (!m_groupsLayout) return;

    QLayoutItem* item;
    while ((item = m_groupsLayout->takeAt(0)) != nullptr) delete item; // não deleta os widgets
    // hide() antes do deleteLater: fora do layout, o separador velho ficaria
    // desenhado na posição antiga até o próximo giro do event loop.
    for (QFrame* sep : std::as_const(m_groupSeparators)) if (sep) { sep->hide(); sep->deleteLater(); }
    m_groupSeparators.clear();

    for (const QString& gid : std::as_const(m_groupOrder)) {
        ToolbarGroupWidget* g = m_groupWidgets.value(gid);
        if (!g) continue;
        g->clearButtons();
        for (const QString& bid : m_groupButtons.value(gid)) {
            if (QToolButton* b = m_buttonsById.value(bid)) g->addButton(b, bid);
        }
        g->refreshEmptyPlaceholder();
    }

    // Grupo vazio só ocupa lugar durante a edição (é onde se devolve um botão
    // pra ele); fora dela some, junto com o separador que ficaria sobrando.
    for (const QString& gid : std::as_const(m_groupOrder)) {
        ToolbarGroupWidget* g = m_groupWidgets.value(gid);
        if (!g) continue;
        if (g->isEmpty() && !m_editMode) { g->hide(); continue; }
        g->setVisible(classicShown());
        m_groupsLayout->addWidget(g, 0, Qt::AlignHCenter);
        // Separador DEPOIS de cada grupo, inclusive o último: é ele que divide
        // os botões fixos do "+" e das gavetas, como sempre foi.
        auto* sep = makeGroupSeparator(this);
        sep->setVisible(classicShown());
        m_groupSeparators.append(sep);
        m_groupsLayout->addWidget(sep);
    }
    refreshNamed();
    updatePlateGeometry();
}

void LeftBar::setEditMode(bool on) {
    if (m_editMode == on) return;
    m_editMode = on;
    if (on) hideLabels(false);
    // Filtro de aplicação só enquanto edita — existe só pra perceber o clique
    // fora da barra que encerra a edição (ver eventFilter).
    if (on) qApp->installEventFilter(this);
    else    qApp->removeEventFilter(this);
    for (ToolbarGroupWidget* g : std::as_const(m_groupWidgets)) {
        if (g) g->setDraggable(on);
    }
    rebuildGroupLayout();
}

void LeftBar::onButtonDropped(const QString& buttonId, const QString& targetGroupId, int index) {
    if (!m_buttonsById.contains(buttonId)) return;
    if (!ToolbarLayout::moveButton(m_groupButtons, buttonId, targetGroupId, index)) return;
    ToolbarLayout::saveButtonLayout(QStringLiteral("ui/leftBarButtonLayout"), m_groupOrder, m_groupButtons);
    rebuildGroupLayout();
}

void LeftBar::onGroupDropped(const QString& draggedId, const QString& targetId) {
    if (!ToolbarLayout::moveGroup(m_groupOrder, draggedId, targetId)) return;
    ToolbarLayout::saveGroupOrder(QStringLiteral("ui/leftBarGroupOrder"), m_groupOrder);
    // O layout é serializado na ordem dos grupos — precisa acompanhar.
    ToolbarLayout::saveButtonLayout(QStringLiteral("ui/leftBarButtonLayout"), m_groupOrder, m_groupButtons);
    rebuildGroupLayout();
}

// Sair da reorganização é clicar em qualquer coisa que não seja um ícone
// arrastável. Este handler cobre o vazio da própria barra — inclusive o de
// dentro de um grupo, cujo press ignorado sobe pra cá. Clique fora da barra é
// o eventFilter; gaveta e "+" são tratados lá também.
void LeftBar::mousePressEvent(QMouseEvent* event) {
    if (m_editMode && event->button() == Qt::LeftButton) {
        setEditMode(false);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

bool LeftBar::eventFilter(QObject* watched, QEvent* event) {
    // Placa: a janela mudou de altura, a placa confere se ainda cabe.
    if (watched == m_filteredParent && event->type() == QEvent::Resize) {
        if (isPlate()) QTimer::singleShot(0, this, [this]() { updatePlateGeometry(); });
        else QTimer::singleShot(0, this, [this]() { updateFloaters(); });
        return QWidget::eventFilter(watched, event);
    }
    // A barra que lê a cena: o mouse numa gaveta mostra quem dela está na folha.
    if (m_style == BarStyle::Scene) {
        auto* db = qobject_cast<QToolButton*>(watched);
        if (db && db->property("drawerKey").isValid() && m_drawerButtons.value(db->property("drawerKey").toString()) == db) {
            if (event->type() == QEvent::Enter) {
                m_sceneCardBtn = db;
                if (m_sceneCard && m_sceneCard->isVisible()) showSceneCard(db);
                else if (m_sceneCardTimer) m_sceneCardTimer->start();
            } else if (event->type() == QEvent::Leave || event->type() == QEvent::MouseButtonPress) {
                hideSceneCard();
            }
        }
    }
    // Etiquetas abertas: tooltip de botão sobra, e clicar na barra fecha a
    // faixa (a gaveta vai abrir bem ali).
    if (m_labels && m_labels->isVisible() && watched != this && qobject_cast<QToolButton*>(watched)) {
        if (event->type() == QEvent::ToolTip) return true;
        if (event->type() == QEvent::MouseButtonPress) hideLabels(false);
    }
    // Filtro de aplicação (só instalado durante a edição). O clique não é
    // consumido: quem clicou no texto quer o cursor lá, e quem clicou numa
    // gaveta quer a gaveta aberta — sair da edição é efeito colateral.
    if (m_editMode && event->type() == QEvent::MouseButtonPress) {
        auto* w = qobject_cast<QWidget*>(watched);
        if (w && w != this) {
            bool insideGroup = false;
            for (ToolbarGroupWidget* g : std::as_const(m_groupWidgets))
                if (g && (w == g || g->isAncestorOf(w))) { insideGroup = true; break; }
            // Dentro da barra mas fora dos grupos (gaveta, "+"): também encerra.
            // O vazio da barra em si cai no mousePressEvent acima.
            if (!insideGroup) setEditMode(false);
        }
    }

    auto* btn = qobject_cast<QToolButton*>(watched);
    if (btn && m_drawerButtons.values().contains(btn)) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragStartPos = me->pos();
                m_pressedDrawerKey = btn->property("drawerKey").toString();
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if ((me->buttons() & Qt::LeftButton) && !m_pressedDrawerKey.isEmpty()) {
                if ((me->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                    const QString key = m_pressedDrawerKey;
                    m_pressedDrawerKey.clear();
                    startDrawerDrag(btn, key);
                    return true; // consome o move para não disparar click
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_pressedDrawerKey.clear();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void LeftBar::startDrawerDrag(QToolButton* btn, const QString& drawerKey) {
    QDrag* drag = new QDrag(btn);
    auto* mime = new QMimeData();
    mime->setData(QLatin1String(kDrawerMime), drawerKey.toUtf8());
    drag->setMimeData(mime);
    const QPixmap pm = btn->grab();
    drag->setPixmap(pm);
    drag->setHotSpot(QPoint(pm.width() / 2, pm.height() / 2));
    drag->exec(Qt::MoveAction);
    clearDropIndicator();
}

int LeftBar::drawerInsertIndexAt(const QPoint& posInBar) const {
    // Calcula índice de inserção (0..N) com base na coordenada Y dentro da barra.
    // Pega o midpoint vertical de cada drawer button; se posY < midpoint, insere antes.
    if (!m_drawerLayout) return 0;
    const int count = m_model ? m_model->drawers().size() : 0;
    int idx = 0;
    for (const auto& d : (m_model ? m_model->drawers() : QList<Drawer>{})) {
        QToolButton* b = m_drawerButtons.value(d.key, nullptr);
        if (!b) { ++idx; continue; }
        const QPoint topLeft = b->mapTo(const_cast<LeftBar*>(this), QPoint(0, 0));
        const int mid = topLeft.y() + b->height() / 2;
        if (posInBar.y() < mid) return idx;
        ++idx;
    }
    return count;
}

void LeftBar::updateDropIndicator(int targetIndex) {
    if (targetIndex == m_dropTargetIndex && m_dropIndicator && m_dropIndicator->isVisible()) return;
    m_dropTargetIndex = targetIndex;

    if (!m_dropIndicator) {
        m_dropIndicator = new QWidget(this);
        m_dropIndicator->setFixedHeight(2);
        m_dropIndicator->setStyleSheet(Theme::qss(QStringLiteral(
            "background: %1; border-radius: @radius-control;").arg(Theme::accentDefault())));
        m_dropIndicator->hide();
    }

    // Posição Y do indicador: topo do botão no targetIndex, ou base do último se idx == count.
    int y = 0;
    const QList<Drawer>& list = m_model ? m_model->drawers() : QList<Drawer>{};
    if (targetIndex >= list.size()) {
        if (!list.isEmpty()) {
            QToolButton* b = m_drawerButtons.value(list.last().key);
            if (b) y = b->mapTo(this, QPoint(0, b->height())).y();
        }
    } else {
        QToolButton* b = m_drawerButtons.value(list.at(targetIndex).key);
        if (b) y = b->mapTo(this, QPoint(0, 0)).y();
    }

    const int margin = 6;
    m_dropIndicator->setGeometry(margin, y - 1, width() - margin * 2, 2);
    m_dropIndicator->raise();
    m_dropIndicator->show();
}

void LeftBar::clearDropIndicator() {
    if (m_dropIndicator) m_dropIndicator->hide();
    m_dropTargetIndex = -1;
}

void LeftBar::dragEnterEvent(QDragEnterEvent* event) {
    if (m_collapsed || m_rolling) return;   // recolhida não tem gaveta pra reordenar
    if (event->mimeData()->hasFormat(QLatin1String(kDrawerMime))) {
        event->acceptProposedAction();
    }
}

void LeftBar::dragMoveEvent(QDragMoveEvent* event) {
    if (!event->mimeData()->hasFormat(QLatin1String(kDrawerMime))) return;
    updateDropIndicator(drawerInsertIndexAt(event->position().toPoint()));
    event->acceptProposedAction();
}

void LeftBar::dragLeaveEvent(QDragLeaveEvent* /*event*/) {
    clearDropIndicator();
}

void LeftBar::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasFormat(QLatin1String(kDrawerMime))) return;
    const QString key = QString::fromUtf8(event->mimeData()->data(QLatin1String(kDrawerMime)));
    const int target = drawerInsertIndexAt(event->position().toPoint());
    clearDropIndicator();
    if (m_model && !key.isEmpty()) {
        m_model->reorderDrawer(key, target);
    }
    event->acceptProposedAction();
}

void LeftBar::rebuildDrawerButtons() {
    if (!m_drawerLayout) return;
    hideSceneCard();
    m_drawerButtons.clear();
    // O "+" mora dentro da bandeja nas Gavetas com cor: sai antes da faxina.
    if (m_newDrawerBtn && m_drawerLayout->indexOf(m_newDrawerBtn) >= 0) {
        m_drawerLayout->removeWidget(m_newDrawerBtn);
        m_newDrawerBtn->setParent(this);
    }
    QLayoutItem* child;
    while ((child = m_drawerLayout->takeAt(0)) != nullptr) {
        if (auto* w = child->widget()) { w->hide(); w->deleteLater(); }
        delete child;
    }
    const bool colors = (m_style == BarStyle::Colors);
    const int pad = colors ? qRound(5 * UiScale::scale()) : 0;
    m_drawerLayout->setContentsMargins(0, pad, 0, pad);
    m_drawerLayout->setSpacing(colors ? qRound(5 * UiScale::scale()) : 4);
    if (m_model) {
        for (const auto& d : m_model->drawers()) {
            // Preferência: drawerIcon (escolha do usuário). Fallback:
            // drawerElementIcon (herdado do tipo). Vazio => letra.
            const QString iconId = !d.drawerIcon.isEmpty() ? d.drawerIcon : d.drawerElementIcon;
            auto* b = makeDrawerButton(d.key, d.title, d.color, iconId);
            if (m_style != BarStyle::Classic) styleDrawerButton(b, d);
            b->setVisible(classicShown());
            if (colors) m_drawerLayout->addWidget(b, 0, Qt::AlignHCenter);
            else m_drawerLayout->addWidget(b);
        }
    }
    placeNewDrawerButton();
    m_drawerLayout->addStretch(1);
    refreshDrum();
    refreshActiveStates();
    updatePlateGeometry();
    // Faixa aberta: os botões novos ainda vão ganhar posição no layout.
    if (m_labels && m_labels->isVisible())
        QTimer::singleShot(0, this, [this]() { if (m_labels && m_labels->isVisible()) refreshLabels(); });
}

bool LeftBar::labelsUnderMouse() const {
    if (m_named && m_named->isVisible() && m_named->rect().contains(m_named->mapFromGlobal(QCursor::pos()))) return true;
    return m_labels && m_labels->isVisible() && m_labels->rect().contains(m_labels->mapFromGlobal(QCursor::pos()));
}

bool LeftBar::labelsEnabled() {
    return QSettings().value(QStringLiteral("ui/leftBarLabels"), true).toBool();
}

void LeftBar::setLabelsEnabled(bool on) {
    QSettings().setValue(QStringLiteral("ui/leftBarLabels"), on);
}

void LeftBar::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    if (!labelsEnabled() || (m_labels && m_labels->isVisible())) return;
    if (!m_labelsTimer) {
        m_labelsTimer = new QTimer(this);
        m_labelsTimer->setSingleShot(true);
        connect(m_labelsTimer, &QTimer::timeout, this, &LeftBar::showLabels);
    }
    m_labelsTimer->start(kLabelsOpenDelayMs);
}

void LeftBar::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    if (m_labelsTimer) m_labelsTimer->stop();
    maybeHideLabels();
}

void LeftBar::moveEvent(QMoveEvent* event) {
    QWidget::moveEvent(event);
    if (m_labels && m_labels->isVisible()) updateLabelsGeometry();
    hideSceneCard();
    updateFloaters();
}

void LeftBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_labels && m_labels->isVisible()) updateLabelsGeometry();
    if (m_plateVisibleH >= 0) setPlateVisible(m_plateVisibleH);   // a máscara acompanha
    QTimer::singleShot(0, this, [this]() { updateFloaters(); });
}

void LeftBar::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (parentWidget() != m_filteredParent) {
        if (m_filteredParent) m_filteredParent->removeEventFilter(this);
        m_filteredParent = parentWidget();
        if (m_filteredParent) m_filteredParent->installEventFilter(this);
    }
    syncLayoutAlignment();
    QTimer::singleShot(0, this, [this]() { updatePlateGeometry(); updateFloaters(); });
}

// ================= Persiana =================

void LeftBar::applyContentVisibility() {
    const bool on = classicShown();
    for (auto it = m_fixedButtons.constBegin(); it != m_fixedButtons.constEnd(); ++it)
        if (it.value()) it.value()->setVisible(on);
    for (ToolbarGroupWidget* g : std::as_const(m_groupWidgets))
        if (g && (!g->isEmpty() || m_editMode)) g->setVisible(on);
    for (auto it = m_drawerButtons.constBegin(); it != m_drawerButtons.constEnd(); ++it)
        if (it.value()) it.value()->setVisible(on);
    if (m_newDrawerBtn) m_newDrawerBtn->setVisible(on);
    if (m_drawerCarousel) m_drawerCarousel->setVisible(on && m_style != BarStyle::Drum);
    if (m_drum) m_drum->setVisible(on && m_style == BarStyle::Drum);
    placeNamed();
    for (auto* sep : std::as_const(m_groupSeparators))
        if (sep) sep->setVisible(on);
    if (m_collapseBtn) m_collapseBtn->setVisible(!m_chromeHidden && !m_rolling && !namedWide());
    if (!on) hideSceneCard();
}

void LeftBar::refreshCollapseButton() {
    if (!m_collapseBtn) return;
    const int w = btnSizePx();
    const int h = qMax(18, qRound(w * 0.6));
    m_collapseBtn->setFixedSize(w, h);
    m_collapseBtn->setStyleSheet(fixedButtonQss());
    if (m_style == BarStyle::Named) {
        // Barra com nomes recolhida: o pé abre os nomes de volta.
        const int px = iconSizePx();
        const QIcon ic = IconUtils::loadToolbarIcon(
            m_barSide == Qt::RightEdge ? QStringLiteral(":/icons/panel-left.svg") : QStringLiteral(":/icons/panel-right.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textPrimary()), QColor(Theme::textBright()), QSize(px, px));
        if (!ic.isNull()) {
            m_collapseBtn->setIcon(ic);
            const int ico = qRound(px * 0.75);
            m_collapseBtn->setIconSize(QSize(ico, ico));
        }
        m_collapseBtn->setToolTip(tr("Mostrar os nomes"));
        return;
    }
    // A seta aponta pra onde a barra vai: aberta, recolhe descendo; recolhida, sobe.
    const QIcon ic = loadLeftbarIcon(m_collapsed ? QStringLiteral("collapse-up.svg")
                                                 : QStringLiteral("collapse-down.svg"));
    if (!ic.isNull()) {
        m_collapseBtn->setIcon(ic);
        const int ico = qRound(iconSizePx() * 0.85);
        m_collapseBtn->setIconSize(QSize(ico, ico));
    }
    m_collapseBtn->setToolTip(m_collapsed ? tr("Mostrar a barra") : tr("Recolher a barra"));
}

int LeftBar::collapsedHeight() const {
    const QMargins m = m_rootLayout ? m_rootLayout->contentsMargins() : QMargins();
    const int btnH = m_collapseBtn ? m_collapseBtn->height() : qRound(btnSizePx() * 0.6);
    return m.top() + btnH + m.bottom();
}

int LeftBar::expandedHeight() const {
    if (isPlate()) return m_plateCachedH > 0 ? m_plateCachedH : plateHeight();
    // A altura que o layout do pai daria pra barra esticada.
    if (QWidget* p = parentWidget()) {
        if (QLayout* l = p->layout()) {
            const int h = l->contentsRect().height();
            if (h > 0) return h;
        }
        return qMax(collapsedHeight(), p->height() - 20);
    }
    return height();
}

void LeftBar::syncLayoutAlignment() {
    QWidget* p = parentWidget();
    QLayout* l = p ? p->layout() : nullptr;
    if (!l) return;
    // Aberta e parada: sem alinhamento, o layout estica até o chão. Recolhida
    // ou animando: presa no pé (onde mora o botão), com a altura que a
    // persiana mandar.
    // Placa: sempre presa no alto (recolhida, a máscara mostra só o pé).
    if (isPlate()) { l->setAlignment(this, Qt::AlignTop); return; }
    const bool pinBottom = m_collapsed || m_rolling;
    l->setAlignment(this, pinBottom ? Qt::AlignBottom : Qt::Alignment());
}

void LeftBar::setCollapsed(bool collapsed, bool animated) {
    if (m_rolling) finishRoll();
    if (m_collapsed == collapsed) return;
    hideLabels(false);
    if (collapsed) setEditMode(false);
    QSettings().setValue(QStringLiteral("ui/leftBarCollapsed"), collapsed);

    if (isPlate()) {
        // Placa: a barra não muda de altura; a máscara desce do topo até o pé,
        // e o botão da persiana fica parado onde está.
        if (!collapsed) {
            m_collapsed = false;
            refreshCollapseButton();
            applyContentVisibility();
            setPlateVisible(-1);
            setStyleSheet(panelStyle());
            updatePlateGeometry();
            // Os botões que voltaram só entram na medida depois dos avisos de
            // layout: processa agora, pra foto da persiana sair inteira.
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            remeasurePlate();
        }
        const int fullH = qMax(collapsedHeight(), m_plateCachedH > 0 ? m_plateCachedH : plateHeight());
        if (!animated || !isVisible() || m_chromeHidden) {
            m_collapsed = collapsed;
            refreshCollapseButton();
            setFixedHeight(fullH);
            setPlateVisible(collapsed ? collapsedHeight() : -1);
            applyContentVisibility();
            syncLayoutAlignment();
            applyTheme();
            updateFloaters();
            return;
        }
        if (m_rootLayout) m_rootLayout->activate();
        m_rollSnapshot = grab();
        m_collapsed = collapsed;
        refreshCollapseButton();
        m_rolling = true;
        applyContentVisibility();
        applyTheme();
        updateFloaters();
        const int fromH = collapsed ? fullH : collapsedHeight();
        const int toH = collapsed ? collapsedHeight() : fullH;
        setPlateVisible(fromH);
        auto* a = new QVariantAnimation(this);
        a->setDuration(collapsed ? 240 : 280);
        a->setStartValue(fromH);
        a->setEndValue(toH);
        a->setEasingCurve(collapsed ? QEasingCurve::InOutCubic : QEasingCurve::OutCubic);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { setPlateVisible(v.toInt()); });
        connect(a, &QVariantAnimation::finished, this, [this]() { finishRoll(); });
        m_rollAnim = a;
        a->start(QAbstractAnimation::DeleteWhenStopped);
        return;
    }

    if (!animated || !isVisible() || m_chromeHidden) {
        m_collapsed = collapsed;
        refreshCollapseButton();
        if (collapsed) setFixedHeight(collapsedHeight());
        else { setMinimumHeight(0); setMaximumHeight(QWIDGETSIZE_MAX); }
        applyContentVisibility();
        syncLayoutAlignment();
        if (!m_chromeHidden) setStyleSheet(panelStyle());   // recolhida solta da faixa
        return;
    }

    // A foto é sempre da barra ABERTA. Recolhendo, ela já está aberta; abrindo,
    // estica um instante (sem pintar) só pra tirar a foto e volta.
    const int fromH = height();
    const int fullH = collapsed ? fromH : expandedHeight();
    if (!collapsed) {
        m_collapsed = false;
        refreshCollapseButton();
        applyContentVisibility();
        setMinimumHeight(0); setMaximumHeight(QWIDGETSIZE_MAX);
        resize(width(), fullH);   // o layout próprio reposiciona na hora (evento de resize)
        m_rootLayout->activate();
    }
    m_rollSnapshot = grab();
    m_collapsed = collapsed;
    refreshCollapseButton();
    const int toH = collapsed ? collapsedHeight() : fullH;

    m_rolling = true;
    applyContentVisibility();
    applyTheme();               // fundo transparente: quem pinta agora é a foto
    setFixedHeight(fromH);
    resize(width(), fromH);
    syncLayoutAlignment();

    auto* a = new QVariantAnimation(this);
    a->setDuration(collapsed ? 240 : 280);
    a->setStartValue(fromH);
    a->setEndValue(toH);
    a->setEasingCurve(collapsed ? QEasingCurve::InOutCubic : QEasingCurve::OutCubic);
    connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        setFixedHeight(v.toInt());
        update();
    });
    connect(a, &QVariantAnimation::finished, this, [this]() { finishRoll(); });
    m_rollAnim = a;
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

void LeftBar::finishRoll() {
    if (m_rollAnim) { QVariantAnimation* a = m_rollAnim; m_rollAnim.clear(); a->stop(); }
    if (!m_rolling) return;
    m_rolling = false;
    m_rollSnapshot = QPixmap();
    if (isPlate()) {
        setPlateVisible(m_collapsed ? collapsedHeight() : -1);
        applyContentVisibility();
        if (!m_collapsed) updatePlateGeometry();
    } else if (m_collapsed) setFixedHeight(collapsedHeight());
    else { setMinimumHeight(0); setMaximumHeight(QWIDGETSIZE_MAX); }
    syncLayoutAlignment();
    applyTheme();
    applyContentVisibility();
    updateFloaters();
    update();
}

void LeftBar::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    // Gavetas com cor: a bandeja das gavetas, um tom abaixo do painel.
    if (m_style == BarStyle::Colors && classicShown() && m_drawerCarousel && m_drawerCarousel->isVisible()) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QColor panel = Theme::toColor(Theme::panelBackgroundFor(Theme::PanelKey::LeftBar));
        const QColor app = Theme::toColor(Theme::appBackground());
        // Um tom abaixo do próprio painel (não da mesa: em tema com painel e
        // mesa parecidos, a bandeja sumia). Escuro escurece mais.
        const bool dark = qGray(panel.rgb()) < 128;
        QColor tray = mixColors(panel, QColor(0, 0, 0), dark ? 0.30 : 0.07);
        Q_UNUSED(app);
        tray.setAlphaF(qMax(0.7, panel.alphaF()));
        const QRect g = m_drawerCarousel->geometry();
        const int tw = tilePx() + qRound(8 * UiScale::scale());
        const QRectF r(width() / 2.0 - tw / 2.0, g.top(), tw, g.height());
        const qreal rad = qRound(10 * UiScale::scale());
        p.setPen(Qt::NoPen);
        p.setBrush(tray);
        p.drawRoundedRect(r, rad, rad);
    }
    // Placa recolhida (parada): a alcinha do pé, pintada aqui porque a máscara
    // cortaria a borda de cima da QSS.
    if (m_plateVisibleH >= 0 && !m_rolling && !m_chromeHidden) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const qreal bw = Theme::panelBorderWidth() / 2.0;
        const QRectF box = QRectF(visibleBox()).adjusted(bw, bw, -bw, -bw);
        const qreal r = qMin<qreal>(Theme::panelRadius(), box.height() / 2.0);
        p.setPen(QPen(Theme::panelBorderBrushFor(Theme::PanelKey::LeftBar, box), Theme::panelBorderWidth()));
        p.setBrush(Theme::toColor(Theme::panelBackgroundFor(Theme::PanelKey::LeftBar)));
        p.drawRoundedRect(box, r, r);
        return;
    }
    if (!m_rolling || m_rollSnapshot.isNull()) return;
    // A barra aberta parada no lugar, colada pelo pé: o pé (com o botão) não
    // se mexe e a borda de cima desce cobrindo os botões, como uma persiana.
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF box = QRectF(visibleBox()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal r = qMin<qreal>(Theme::panelRadius(), qMin(box.width(), box.height()) / 2.0);
    QPainterPath shape;
    shape.addRoundedRect(box, r, r);
    p.setClipPath(shape);
    const int snapH = qRound(m_rollSnapshot.height() / m_rollSnapshot.devicePixelRatio());
    p.drawPixmap(0, height() - snapH, m_rollSnapshot);
    p.setClipping(false);
    p.setPen(QPen(Theme::panelBorderBrushFor(Theme::PanelKey::LeftBar, box), Theme::panelBorderWidth()));
    p.setBrush(Qt::NoBrush);
    p.drawPath(shape);
}

void LeftBar::hideEvent(QHideEvent* event) {
    hideLabels(false);
    hideSceneCard();
    if (m_drumTag) m_drumTag->hide();
    if (m_thread) m_thread->hide();
    if (m_named) m_named->hide();
    QWidget::hideEvent(event);
}

void LeftBar::updateLabelsGeometry() {
    if (!m_labels) return;
    const int w = m_labelsWidth + kLabelsShadow;
    const QRect g = geometry();
    const bool mirrored = (m_barSide == Qt::RightEdge);
    // 1px por cima da borda da barra, pra não ficar uma linha dupla no meio.
    const int x = mirrored ? g.x() - w + 1 : g.x() + g.width() - 1;
    // Só até a última linha: a faixa é uma aba do tamanho do conteúdo, não um
    // painel vazio até o chão.
    const int h = m_labelsHeight > 0 ? qMin(g.height(), m_labelsHeight) : g.height();
    m_labels->setGeometry(x, g.y(), w, h);
}

void LeftBar::showLabels() {
    if (!labelsEnabled() || m_editMode || m_chromeHidden || m_collapsed || m_rolling || !isVisible() || QApplication::mouseButtons() != Qt::NoButton) return;
    if (namedWide()) return;   // na Barra com nomes, os nomes já estão lá
    QWidget* host = parentWidget();
    if (!host) return;
    if (!m_labels || m_labels->parentWidget() != host) {
        delete m_labels;
        m_labels = new LeftBarLabels(host);
        m_labels->onLeave = [this]() { maybeHideLabels(); };
    }
    refreshLabels();
    updateLabelsGeometry();
    m_labels->raise();
    m_labels->show();
    m_labels->open(PanelMotion::enabled());
    // Enquanto a faixa está aberta, os botões não precisam de tooltip, e um
    // clique na barra fecha a faixa (é a gaveta que vai abrir ali).
    for (QToolButton* b : findChildren<QToolButton*>()) b->installEventFilter(this);
    updateFloaters();   // a lingueta do tambor dá lugar à faixa
    if (m_sceneCard && m_sceneCard->isVisible() && m_sceneCardBtn) showSceneCard(m_sceneCardBtn);
}

bool LeftBar::drawerInCarouselWindow(QToolButton* b) const {
    if (!b || !m_drawerCarousel || !m_drawerCarousel->isOverflowing()) return true;
    const QRect flat = m_drawerCarousel->flatRect();
    const int mid = b->mapTo(m_drawerCarousel, QPoint(0, b->height() / 2)).y();
    return mid >= flat.top() && mid <= flat.bottom();
}

void LeftBar::refreshLabels() {
    if (!m_labels) return;
    QList<LeftBarLabels::Row> rows;
    QList<LeftBarLabels::Head> heads;
    auto rowFor = [&](QToolButton* b, const QString& text, const QString& count) {
        LeftBarLabels::Row r;
        const QPoint at = b->mapTo(this, QPoint(0, 0));
        r.y = at.y();
        r.h = b->height();
        r.text = text;
        r.count = count;
        r.active = b->isChecked();
        QPointer<QToolButton> pb(b);
        r.click = [this, pb]() { hideLabels(false); if (pb) pb->click(); };
        return r;
    };
    auto groupName = [this](const QString& gid) {
        if (gid == QStringLiteral("project")) return tr("Projeto");
        if (gid == QStringLiteral("planning")) return tr("Planejamento");
        if (gid == QStringLiteral("writing")) return tr("Escrita");
        return QString();
    };
    // Na faixa, o nome curto; a dica completa continua no tooltip do botão.
    const QHash<QString, QString> shortName = {
        { QStringLiteral("info"),        tr("Informações") },
        { QStringLiteral("whiteboard"),  tr("Lousa") },
        { QStringLiteral("timeline"),    tr("Linha do tempo") },
        { QStringLiteral("worldmap"),    tr("Mapa") },
        { QStringLiteral("manuscripts"), tr("Manuscritos") },
        { QStringLiteral("outline"),     tr("Outline") },
        { QStringLiteral("groups"),      tr("Grupos") },
    };
    QStringList shownGroups;
    for (const QString& gid : std::as_const(m_groupOrder)) {
        ToolbarGroupWidget* g = m_groupWidgets.value(gid);
        if (!g || !g->isVisible()) continue;
        shownGroups << gid;
        for (const QString& bid : m_groupButtons.value(gid)) {
            QToolButton* b = m_buttonsById.value(bid);
            if (b && b->isVisible()) rows << rowFor(b, shortName.value(bid, b->toolTip()), QString());
        }
    }
    // Título de cada grupo em cima do separador que o antecede; o último
    // separador abre as gavetas.
    for (int i = 0; i < m_groupSeparators.size(); ++i) {
        QFrame* sep = m_groupSeparators.at(i);
        if (!sep || !sep->isVisible()) continue;
        LeftBarLabels::Head h;
        h.y = sep->mapTo(this, QPoint(0, 0)).y() + sep->height() / 2;
        h.text = (i + 1 < shownGroups.size()) ? groupName(shownGroups.at(i + 1)) : tr("Gavetas");
        if (!h.text.isEmpty()) heads << h;
    }
    if (m_newDrawerBtn && m_newDrawerBtn->isVisible()) rows << rowFor(m_newDrawerBtn, tr("Nova gaveta"), QString());
    if (m_model) {
        for (const auto& d : m_model->drawers()) {
            QToolButton* b = m_drawerButtons.value(d.key);
            if (!b || !b->isVisible() || !drawerInCarouselWindow(b)) continue;
            LeftBarLabels::Row r = rowFor(b, d.title, d.items.isEmpty() ? QString() : QString::number(d.items.size()));
            const QString key = d.key;
            r.context = [this, key](const QPoint& pos) { hideLabels(false); emit drawerContextRequested(key, pos); };
            rows << r;
        }
    }
    // Largura pelo conteúdo: nome mais comprido (em negrito, que é o mais
    // largo) + contagem + respiros das margens.
    const qreal s = UiScale::scale();
    QFont nf(QStringLiteral("Segoe UI"));
    nf.setPixelSize(qMax(10, qRound(13 * s)));
    nf.setWeight(QFont::DemiBold);
    QFont cf(QStringLiteral("Segoe UI"));
    cf.setPixelSize(qMax(9, qRound(11 * s)));
    qreal need = 0;
    for (const auto& r : std::as_const(rows)) {
        qreal w = QFontMetricsF(nf).horizontalAdvance(r.text);
        if (!r.count.isEmpty()) w += QFontMetricsF(cf).horizontalAdvance(r.count) + 8;
        need = qMax(need, w);
    }
    // Com o carrossel girando, as gavetas escondidas também entram na conta:
    // a faixa não muda de largura a cada gaveta que passa.
    if (m_model && m_drawerCarousel && m_drawerCarousel->isOverflowing()) {
        for (const auto& d : m_model->drawers()) {
            qreal w = QFontMetricsF(nf).horizontalAdvance(d.title);
            if (!d.items.isEmpty()) w += QFontMetricsF(cf).horizontalAdvance(QString::number(d.items.size())) + 8;
            need = qMax(need, w);
        }
    }
    QFont hf(QStringLiteral("Segoe UI"));
    hf.setPixelSize(qMax(7, qRound(8.5 * s)));
    hf.setBold(true);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
    for (const auto& h : std::as_const(heads)) need = qMax(need, QFontMetricsF(hf).horizontalAdvance(h.text.toUpper()) - 8);
    m_labelsWidth = qBound(labelsMinWidthPx(), qCeil(need) + 30, labelsMaxWidthPx());
    int bottom = 0;
    for (const auto& r : std::as_const(rows)) bottom = qMax(bottom, r.y + r.h);
    m_labelsHeight = bottom > 0 ? bottom + qRound(10 * s) : 0;
    // Girando, a lista vai até o pé: a faixa acompanha a barra inteira.
    if (m_drawerCarousel && m_drawerCarousel->isOverflowing()) m_labelsHeight = 0;
    m_labels->setContent(rows, heads, m_barSide == Qt::RightEdge);
    updateLabelsGeometry();
}

void LeftBar::hideLabels(bool animated) {
    if (m_labelsTimer) m_labelsTimer->stop();
    if (m_labels && m_labels->isVisible()) {
        m_labels->close(animated && PanelMotion::enabled());
        // a lingueta volta quando a faixa terminar de fechar
        QTimer::singleShot(animated ? 140 : 0, this, [this]() { updateFloaters(); });
    }
}

void LeftBar::maybeHideLabels() {
    // Um respiro pro mouse atravessar da barra pra faixa (e de volta).
    QTimer::singleShot(70, this, [this]() {
        const QPoint g = QCursor::pos();
        if (rect().contains(mapFromGlobal(g))) return;
        if (m_labels && m_labels->isVisible() && m_labels->rect().contains(m_labels->mapFromGlobal(g))) return;
        hideLabels(true);
    });
}
