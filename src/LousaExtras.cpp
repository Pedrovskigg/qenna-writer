#include "LousaExtras.h"

#include "CardItem.h"
#include "ColorPopover.h"
#include "IconUtils.h"
#include "LousaScene.h"
#include "LousaView.h"
#include "Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <memory>

namespace {

QIcon lousaIcon(const QString& name, const QColor& normal = QColor())
{
    return IconUtils::loadToolbarIcon(QStringLiteral(":/icons/lousa/%1.svg").arg(name),
        normal.isValid() ? normal : QColor(Theme::textMuted()),
        QColor(Theme::textPrimary()), QColor(Theme::accentDefault()));
}

QColor styleColor(const QString& style, const QColor& custom, const QColor& themeBg)
{
    if (style == QStringLiteral("cork"))       return QColor(0x9c, 0x74, 0x4c);
    if (style == QStringLiteral("whiteboard")) return QColor(0xf4, 0xf3, 0xee);
    return custom.isValid() ? custom : themeBg;
}

QColor cardFill(const CanvasCard& c)
{
    const QString t = c.type;
    if (t == QStringLiteral("image") || t == QStringLiteral("doc") || t == QStringLiteral("chapter"))
        return QColor(0x3a, 0x3a, 0x37);
    if (t == QStringLiteral("character")) return QColor(0xf3, 0xef, 0xe6);
    return c.color.isValid() ? c.color : QColor(0xf6, 0xd0, 0x6a);
}

// Popup sem moldura, no desenho do app (fundo do painel, borda, cantos do tema).
QDialog* makePopup(QWidget* parent, QWidget** panelOut, const QString& objectName)
{
    auto* dlg = new QDialog(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    dlg->setAttribute(Qt::WA_TranslucentBackground);
    auto* outer = new QVBoxLayout(dlg);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* panel = new QWidget(dlg);
    panel->setObjectName(objectName);
    panel->setAttribute(Qt::WA_StyledBackground, true);
    outer->addWidget(panel);
    *panelOut = panel;
    return dlg;
}

void placePopup(QDialog* dlg, const QPoint& at, const QSize& size)
{
    dlg->resize(size);
    QPoint p = at;
    if (QScreen* scr = QGuiApplication::screenAt(at)) {
        const QRect ar = scr->availableGeometry();
        p.setX(qBound(ar.left() + 8, p.x() - size.width() / 2, ar.right() - size.width() - 8));
        p.setY(qBound(ar.top() + 8, p.y(), ar.bottom() - size.height() - 8));
    }
    dlg->move(p);
}

QString popupQss(const QString& name)
{
    return Theme::qss(QStringLiteral(
        "QWidget#%1 { background: %2; border: 1px solid %3; border-radius: @radius-panel; }"
        "QWidget#%1 QLabel { background: transparent; color: %4; }"
        "QWidget#%1 QLabel[role=\"section\"] { color: %5; font-size: 11px; font-weight: 700; letter-spacing: 1px; }"
        "QWidget#%1 QCheckBox { color: %4; font-size: 12px; spacing: 8px; background: transparent; }"
        "QWidget#%1 QToolButton#lousaLookTile { background: transparent; border: 1px solid transparent;"
        "  border-radius: @radius-control; color: %4; font-size: 11px; padding: 4px; }"
        "QWidget#%1 QToolButton#lousaLookTile:hover { background: %6; }"
        "QWidget#%1 QToolButton#lousaLookTile:checked { border-color: %7; background: %6; }"
        "QWidget#%1 QToolButton#lousaLookPill { background: transparent; border: 1px solid %3;"
        "  border-radius: 13px; color: %4; font-size: 12px; padding: 4px 12px; }"
        "QWidget#%1 QToolButton#lousaLookPill:hover { background: %6; }"
        "QWidget#%1 QToolButton#lousaLookPill:checked { border-color: %7; color: %8; }"
    ).arg(name, Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(),
          Theme::textMuted(), Theme::hoverOverlay(), Theme::accentDefault(), Theme::textBright()));
}

} // namespace

// ═════════════════════════════ LousaDraw ═════════════════════════════════════

namespace LousaDraw {

Snapshot fromScene(const LousaScene* scene)
{
    Snapshot s;
    if (!scene) return s;
    s.cards = scene->allCardData();
    s.zones = scene->allZoneData();
    s.conns = scene->allConnectionData();
    s.inks  = scene->allInkData();
    s.bg    = scene->effectiveCanvasColor();
    return s;
}

Snapshot fromFile(const QString& path, const QColor& themeBg)
{
    Snapshot s;
    QFile f(path);
    QJsonObject root;
    if (f.open(QIODevice::ReadOnly)) root = QJsonDocument::fromJson(f.readAll()).object();
    // Antes do rework, a cor era sempre gravada (#1a1a2e = a padrão de então).
    QColor custom(root.value(QStringLiteral("canvasColor")).toString());
    const bool isCustom = root.contains(QStringLiteral("canvasColorCustom"))
        ? root.value(QStringLiteral("canvasColorCustom")).toBool(false)
        : (custom.isValid() && custom != QColor(QStringLiteral("#1a1a2e")));
    if (!isCustom) custom = QColor();
    s.bg = styleColor(root.value(QStringLiteral("canvasStyle")).toString(), custom, themeBg);
    for (const auto& v : root.value(QStringLiteral("cards")).toArray()) {
        const QJsonObject o = v.toObject();
        CanvasCard c;
        c.id = o.value(QStringLiteral("id")).toString();
        c.type = o.value(QStringLiteral("type")).toString(QStringLiteral("note"));
        c.x = o.value(QStringLiteral("x")).toDouble();
        c.y = o.value(QStringLiteral("y")).toDouble();
        c.width = o.value(QStringLiteral("width")).toDouble(200);
        c.height = o.value(QStringLiteral("height")).toDouble(160);
        c.color = QColor(o.value(QStringLiteral("color")).toString(QStringLiteral("#ffd060")));
        s.cards << c;
    }
    for (const auto& v : root.value(QStringLiteral("zones")).toArray()) {
        const QJsonObject o = v.toObject();
        CanvasZone z;
        z.x = o.value(QStringLiteral("x")).toDouble();
        z.y = o.value(QStringLiteral("y")).toDouble();
        z.width = o.value(QStringLiteral("width")).toDouble(300);
        z.height = o.value(QStringLiteral("height")).toDouble(200);
        z.color = QColor(o.value(QStringLiteral("color")).toString(QStringLiteral("#6ea8fe")));
        s.zones << z;
    }
    for (const auto& v : root.value(QStringLiteral("connections")).toArray()) {
        const QJsonObject o = v.toObject();
        CanvasConnection c;
        c.fromId = o.value(QStringLiteral("fromId")).toString();
        c.toId = o.value(QStringLiteral("toId")).toString();
        c.color = QColor(o.value(QStringLiteral("color")).toString(QStringLiteral("#ffffff")));
        s.conns << c;
    }
    for (const auto& v : root.value(QStringLiteral("inks")).toArray())
        s.inks << LousaInk::fromJson(v.toObject());
    return s;
}

QRectF bounds(const Snapshot& s)
{
    QRectF r;
    for (const CanvasCard& c : s.cards) {
        const qreal w = (c.type == QStringLiteral("text") || c.type == QStringLiteral("symbol")) ? qMax(60.0, c.width) : c.width;
        r = r.united(QRectF(c.x, c.y, w, c.height));
    }
    for (const CanvasZone& z : s.zones) r = r.united(QRectF(z.x, z.y, z.width, z.height));
    for (const CanvasInk& k : s.inks) r = r.united(LousaInk::bounds(k));
    return r;
}

QTransform draw(QPainter* p, const QRectF& target, const Snapshot& s, const QRectF& areaIn)
{
    QRectF area = areaIn.isValid() ? areaIn : bounds(s);
    if (!area.isValid() || area.isEmpty()) area = QRectF(0, 0, 800, 500);
    const qreal sc = qMin(target.width() / area.width(), target.height() / area.height());
    const qreal ox = target.center().x() - area.center().x() * sc;
    const qreal oy = target.center().y() - area.center().y() * sc;
    QTransform xf;
    xf.translate(ox, oy);
    xf.scale(sc, sc);

    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setClipRect(target);
    for (const CanvasZone& z : s.zones) {
        const QRectF r = xf.mapRect(QRectF(z.x, z.y, z.width, z.height));
        QColor c = z.color; c.setAlpha(150);
        p->setPen(QPen(c, 1.0));
        c.setAlpha(36);
        p->setBrush(c);
        p->drawRoundedRect(r, 2, 2);
    }
    QHash<QString, QPointF> pins;
    for (const CanvasCard& c : s.cards) pins.insert(c.id, xf.map(QPointF(c.x + c.width / 2, c.y)));
    for (const CanvasConnection& k : s.conns) {
        if (!pins.contains(k.fromId) || !pins.contains(k.toId)) continue;
        QColor c = k.color.isValid() ? k.color : QColor(Qt::white);
        c.setAlpha(170);
        p->setPen(QPen(c, 1.0));
        p->drawLine(pins.value(k.fromId), pins.value(k.toId));
    }
    p->setPen(Qt::NoPen);
    for (const CanvasCard& c : s.cards) {
        if (c.type == QStringLiteral("sticker")) continue;   // enfeite: fora do mapa
        if (c.type == QStringLiteral("text")) {
            const QRectF r = xf.mapRect(QRectF(c.x, c.y, qMax(60.0, c.width), 14));
            QColor col = c.color.isValid() ? c.color : QColor(Qt::white);
            col.setAlpha(170);
            p->setBrush(col);
            p->drawRect(QRectF(r.left(), r.center().y() - 0.75, r.width(), 1.5));
            continue;
        }
        if (c.type == QStringLiteral("symbol")) {
            p->setBrush(c.color.isValid() ? c.color : QColor(Qt::white));
            p->drawEllipse(xf.map(QPointF(c.x + 20, c.y + 20)), 1.8, 1.8);
            continue;
        }
        p->setBrush(cardFill(c));
        p->drawRect(xf.mapRect(QRectF(c.x, c.y, c.width, c.height)));
    }
    // Traços da Caneta: linha fina na cor de cada um.
    p->setBrush(Qt::NoBrush);
    for (const CanvasInk& k : s.inks) {
        if (k.pts.size() < 2) continue;
        QColor col = k.color;
        col.setAlpha(k.tool == QStringLiteral("highlight") ? 110 : 200);
        QPen pen(col, k.tool == QStringLiteral("highlight") ? 2.0 : 1.0);
        pen.setCapStyle(Qt::RoundCap);
        p->setPen(pen);
        QPolygonF poly;
        poly.reserve(k.pts.size());
        for (const InkPoint& q : k.pts) poly << xf.map(QPointF(q.x, q.y));
        p->drawPolyline(poly);
    }
    p->restore();
    return xf;
}

} // namespace LousaDraw

// ═════════════════════════════ Minimapa ══════════════════════════════════════

LousaMinimap::LousaMinimap(LousaScene* scene, LousaView* view, QWidget* parent)
    : QWidget(parent), m_scene(scene), m_view(view)
{
    setFixedSize(184, 116);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Minimapa: clique pra ir até lá"));
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(90);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_snap = LousaDraw::fromScene(m_scene);
        update();
    });
}

void LousaMinimap::scheduleRefresh()
{
    if (!m_timer->isActive()) m_timer->start();
}

void LousaMinimap::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF outer = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(QColor(Theme::panelBorder()), 1));
    p.setBrush(QColor(Theme::panelBackground()));
    p.drawRoundedRect(outer, 9, 9);
    const QRectF inner = outer.adjusted(7, 7, -7, -7);
    p.setPen(Qt::NoPen);
    p.setBrush(m_snap.bg.isValid() ? m_snap.bg : QColor(Theme::appBackground()));
    p.drawRoundedRect(inner, 5, 5);

    QRectF vis;
    if (m_view) vis = m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
    QRectF area = LousaDraw::bounds(m_snap);
    area = area.isValid() ? area.united(vis) : vis;
    area = area.adjusted(-area.width() * 0.05, -area.height() * 0.05, area.width() * 0.05, area.height() * 0.05);
    m_area = area;
    m_xf = LousaDraw::draw(&p, inner, m_snap, area);
    if (vis.isValid()) {
        p.setPen(QPen(QColor(Theme::accentDefault()), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(m_xf.mapRect(vis).intersected(inner), 2, 2);
    }
}

void LousaMinimap::jumpTo(const QPoint& pos)
{
    bool ok = false;
    const QTransform inv = m_xf.inverted(&ok);
    if (!ok) return;
    emit centerRequested(inv.map(QPointF(pos)));
}

void LousaMinimap::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) jumpTo(e->pos());
}

void LousaMinimap::mouseMoveEvent(QMouseEvent* e)
{
    if (e->buttons() & Qt::LeftButton) jumpTo(e->pos());
}

// ═════════════════════════════ Cola de atalhos ═══════════════════════════════

LousaCheatSheet::LousaCheatSheet(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("lousaCheat"));
    setFocusPolicy(Qt::StrongFocus);
    hide();
    build();
}

void LousaCheatSheet::build()
{
    m_card = new QWidget(this);
    m_card->setObjectName(QStringLiteral("lousaCheatCard"));
    m_card->setAttribute(Qt::WA_StyledBackground, true);
    auto* v = new QVBoxLayout(m_card);
    v->setContentsMargins(28, 22, 22, 26);
    v->setSpacing(16);

    auto* head = new QHBoxLayout;
    auto* title = new QLabel(tr("Atalhos da Lousa"), m_card);
    title->setObjectName(QStringLiteral("lousaCheatTitle"));
    head->addWidget(title);
    head->addStretch(1);
    auto* close = new QToolButton(m_card);
    close->setObjectName(QStringLiteral("lousaCheatClose"));
    close->setIcon(lousaIcon(QStringLiteral("close")));
    close->setIconSize(QSize(16, 16));
    close->setFixedSize(30, 30);
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip(tr("Fechar (Esc)"));
    connect(close, &QToolButton::clicked, this, &QWidget::hide);
    head->addWidget(close);
    v->addLayout(head);

    struct Row { QString keys; QString what; };
    struct Group { QString title; QVector<Row> rows; };
    const QVector<Group> groups = {
        { tr("Pôr no quadro"), {
            { QStringLiteral("Ctrl+N"), tr("Post-it") },
            { QStringLiteral("Alt+C"), tr("Comentário") },
            { QStringLiteral("Ctrl+T"), tr("Texto livre") },
            { tr("2 cliques no fundo"), tr("Texto livre ali mesmo") },
            { QStringLiteral("Ctrl+G"), tr("Imagem") },
            { tr("Arrastar ou colar um PNG"), tr("Fundo transparente vira adesivo") },
            { QStringLiteral("Ctrl+H"), tr("Documento de uma gaveta") },
            { QStringLiteral("Ctrl+Shift+C"), tr("Personagem") },
            { tr("Personagem, Documento"), tr("Na barra de baixo: escolhe do projeto (inclui capítulos)") },
        }},
        { tr("Andar pelo quadro"), {
            { tr("Arrastar o fundo"), tr("Move o quadro") },
            { tr("Rodinha"), tr("Aproxima e afasta") },
            { QStringLiteral("Ctrl+0"), tr("Ver tudo") },
            { QStringLiteral("Ctrl+F"), tr("Buscar um card") },
            { QStringLiteral("F"), tr("Lista das áreas") },
            { tr("Minimapa"), tr("Clica e vai") },
        }},
        { tr("Selecionar"), {
            { tr("Shift+clique"), tr("Marca mais um") },
            { tr("Shift+S (segurar)"), tr("Pincel: marca o que o mouse tocar") },
            { QStringLiteral("Ctrl+X · Ctrl+V"), tr("Recorta e cola onde está o mouse") },
            { QStringLiteral("Esc"), tr("Cancela") },
        }},
        { tr("Ligar"), {
            { tr("Arrastar o pin"), tr("Do alfinete até outro card") },
            { tr("Ligar (barra de baixo)"), tr("Clica num card e depois no outro") },
            { tr("Post-it na linha, 1 s"), tr("Vira uma parada da linha") },
            { tr("2 cliques na linha"), tr("Dá um nome à ligação") },
        }},
        { tr("Texto livre"), {
            { tr("2 cliques"), tr("Escreve") },
            { QStringLiteral("Esc"), tr("Termina") },
            { tr("Bolinha de cima"), tr("Gira (ou Shift+arrastar)") },
            { tr("Canto"), tr("Tamanho da letra") },
            { tr("Lateral"), tr("Largura do texto") },
        }},
        { tr("Guardar e desfazer"), {
            { QStringLiteral("Delete"), tr("Guarda na gaveta da lousa") },
            { QStringLiteral("Shift+Delete"), tr("Apaga de vez") },
            { QStringLiteral("Ctrl+Z"), tr("Desfaz (até 50 vezes)") },
            { QStringLiteral("Ctrl+Y"), tr("Refaz") },
        }},
        { tr("Áreas"), {
            { tr("Área (barra de baixo)"), tr("E arraste no quadro") },
            { tr("Shift+clique na área"), tr("Marca a área com tudo dentro; arrastar leva junto") },
            { QStringLiteral("Ctrl+D"), tr("A área marcada vira uma gaveta") },
            { QStringLiteral("Ctrl+Shift+D"), tr("Todas as áreas viram gavetas") },
        }},
        { tr("Cards"), {
            { tr("2 cliques no título"), tr("Edita o título") },
            { tr("2 cliques no personagem"), tr("Vira a ficha") },
            { tr("2 cliques no documento"), tr("Abre no editor") },
            { tr("Estilo (no post-it)"), tr("Formato, presilha e borda") },
            { QStringLiteral("Ctrl+] · Ctrl+["), tr("Adesivo pra frente / pra trás") },
            { tr("Não precisa salvar"), tr("Tudo é salvo sozinho") },
        }},
    };

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(30);
    grid->setVerticalSpacing(18);
    int gi = 0;
    for (const Group& g : groups) {
        auto* box = new QWidget(m_card);
        auto* bl = new QVBoxLayout(box);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(6);
        auto* gt = new QLabel(g.title.toUpper(), box);
        gt->setObjectName(QStringLiteral("lousaCheatGroup"));
        bl->addWidget(gt);
        for (const Row& r : g.rows) {
            auto* row = new QHBoxLayout;
            row->setSpacing(8);
            auto* keys = new QLabel(r.keys, box);
            keys->setObjectName(QStringLiteral("lousaKey"));
            row->addWidget(keys, 0, Qt::AlignVCenter);
            auto* what = new QLabel(r.what, box);
            what->setObjectName(QStringLiteral("lousaCheatWhat"));
            what->setWordWrap(true);
            row->addWidget(what, 1);
            bl->addLayout(row);
        }
        bl->addStretch(1);
        grid->addWidget(box, gi / 4, gi % 4, Qt::AlignTop);
        ++gi;
    }
    v->addLayout(grid);
    applyTheme();
}

void LousaCheatSheet::applyTheme()
{
    if (!m_card) return;
    m_card->setStyleSheet(Theme::qss(QStringLiteral(
        "QWidget#lousaCheatCard { background: %1; border: 1px solid %2; border-radius: @radius-panel; }"
        "QWidget#lousaCheatCard QWidget { background: transparent; }"
        "QLabel#lousaCheatTitle { color: %3; font-size: 18px; font-weight: 700; }"
        "QLabel#lousaCheatGroup { color: %4; font-size: 11px; font-weight: 700; letter-spacing: 1px; }"
        "QLabel#lousaKey { color: %3; background: %5; border: 1px solid %2; border-bottom-width: 2px;"
        "  border-radius: 5px; padding: 1px 7px; font-size: 11.5px; font-weight: 600; }"
        "QLabel#lousaCheatWhat { color: %6; font-size: 12.5px; }"
        "QToolButton#lousaCheatClose { background: transparent; border: none; border-radius: @radius-control; }"
        "QToolButton#lousaCheatClose:hover { background: %7; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textBright(), Theme::textMuted(),
          Theme::inputBackground(), Theme::textPrimary(), Theme::hoverOverlay())));
}

void LousaCheatSheet::showOver()
{
    if (!parentWidget()) return;
    setGeometry(parentWidget()->rect());
    show();
    raise();
    setFocus();
}

void LousaCheatSheet::resizeEvent(QResizeEvent*)
{
    if (!m_card) return;
    const int w = qMin(width() - 48, 1040);
    m_card->setFixedWidth(w);
    QLayout* lay = m_card->layout();
    lay->activate();
    const int want = lay->hasHeightForWidth() ? lay->totalHeightForWidth(w) : m_card->sizeHint().height();
    const int h = qMin(want + 8, height() - 40);
    m_card->setGeometry((width() - w) / 2, qMax(20, (height() - h) / 2), w, h);
}

void LousaCheatSheet::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 120));
}

void LousaCheatSheet::mousePressEvent(QMouseEvent* e)
{
    if (!m_card->geometry().contains(e->pos())) hide();
}

void LousaCheatSheet::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Question) { hide(); return; }
    QWidget::keyPressEvent(e);
}

// ═════════════════════════════ Busca ═════════════════════════════════════════

LousaSearchBar::LousaSearchBar(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("lousaSearch"));
    setAttribute(Qt::WA_StyledBackground, true);
    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(10, 4, 4, 4);
    h->setSpacing(6);
    auto* icon = new QLabel(this);
    icon->setPixmap(lousaIcon(QStringLiteral("search")).pixmap(16, 16));
    h->addWidget(icon);
    m_edit = new QLineEdit(this);
    m_edit->setObjectName(QStringLiteral("lousaSearchEdit"));
    m_edit->setPlaceholderText(tr("Buscar na lousa…"));
    m_edit->setFixedWidth(220);
    m_edit->installEventFilter(this);
    connect(m_edit, &QLineEdit::textChanged, this, &LousaSearchBar::queryChanged);
    h->addWidget(m_edit);
    m_count = new QLabel(this);
    m_count->setObjectName(QStringLiteral("lousaSearchCount"));
    m_count->setMinimumWidth(56);
    h->addWidget(m_count);
    auto mk = [this, h](const QString& icon, const QString& tip, auto slot) {
        auto* b = new QToolButton(this);
        b->setObjectName(QStringLiteral("lousaSearchBtn"));
        b->setIcon(lousaIcon(icon));
        b->setIconSize(QSize(16, 16));
        b->setFixedSize(28, 28);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, slot);
        h->addWidget(b);
    };
    mk(QStringLiteral("tray-up"), tr("Anterior (Shift+Enter)"), [this]() { emit previous(); });
    mk(QStringLiteral("tray-down"), tr("Próximo (Enter)"), [this]() { emit next(); });
    mk(QStringLiteral("close"), tr("Fechar (Esc)"), [this]() { hide(); emit closed(); });
    applyTheme();
    hide();
}

void LousaSearchBar::applyTheme()
{
    setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#lousaSearch { background: %1; border: 1px solid %2; border-radius: @radius-panel; }"
        "QFrame#lousaSearch QLabel { background: transparent; color: %3; font-size: 12px; }"
        "QLineEdit#lousaSearchEdit { background: transparent; border: none; color: %4; font-size: 13px; }"
        "QToolButton#lousaSearchBtn { background: transparent; border: none; border-radius: @radius-control; }"
        "QToolButton#lousaSearchBtn:hover { background: %5; }"
    ).arg(Theme::panelBackground(), Theme::accentDefault(), Theme::textMuted(),
          Theme::textBright(), Theme::hoverOverlay())));
}

void LousaSearchBar::open()
{
    show();
    raise();
    m_edit->setFocus();
    m_edit->selectAll();
}

QString LousaSearchBar::text() const { return m_edit->text(); }

void LousaSearchBar::setResult(int current, int total)
{
    if (total < 0 || m_edit->text().trimmed().isEmpty()) m_count->clear();
    else if (total == 0) m_count->setText(tr("nada"));
    else m_count->setText(tr("%1 de %2").arg(current + 1).arg(total));
    adjustSize();
}

bool LousaSearchBar::eventFilter(QObject* o, QEvent* e)
{
    if (o == m_edit && e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Escape) { hide(); emit closed(); return true; }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (ke->modifiers() & Qt::ShiftModifier) emit previous();
            else emit next();
            return true;
        }
    }
    return QFrame::eventFilter(o, e);
}

// ═════════════════════════════ Fundo e cards ════════════════════════════════

namespace LousaBoardLook {

static QPixmap stylePreview(const QString& style, const QColor& custom)
{
    const QColor themeBg(Theme::appBackground());
    const QColor bg = styleColor(style, custom, themeBg);
    QPixmap px(128, 84);
    px.setDevicePixelRatio(2.0);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip; clip.addRoundedRect(QRectF(0.5, 0.5, 63, 41), 5, 5);
    p.setClipPath(clip);
    p.fillRect(QRectF(0, 0, 64, 42), bg);
    const bool light = bg.lightness() > 150;
    const QColor ink = light ? QColor(0, 0, 0, 50) : QColor(255, 255, 255, 50);
    if (style == QStringLiteral("dots") || style == QStringLiteral("whiteboard")) {
        p.setPen(Qt::NoPen); p.setBrush(ink);
        for (int x = 6; x < 64; x += 9) for (int y = 6; y < 42; y += 9) p.drawEllipse(QPointF(x, y), 0.8, 0.8);
    } else if (style == QStringLiteral("grid")) {
        p.setPen(QPen(ink, 0.6));
        for (int x = 4; x < 64; x += 9) p.drawLine(QPointF(x, 0), QPointF(x, 42));
        for (int y = 4; y < 42; y += 9) p.drawLine(QPointF(0, y), QPointF(64, y));
    } else if (style == QStringLiteral("lines")) {
        p.setPen(QPen(light ? QColor(80, 120, 190, 110) : QColor(130, 165, 220, 90), 0.7));
        for (int y = 7; y < 42; y += 8) p.drawLine(QPointF(0, y), QPointF(64, y));
    } else if (style == QStringLiteral("cork")) {
        p.setPen(Qt::NoPen);
        quint32 seed = 7u;
        auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0; };
        for (int i = 0; i < 160; ++i) {
            p.setBrush(rnd() < 0.6 ? QColor(60, 34, 14, 90) : QColor(255, 228, 190, 70));
            p.drawEllipse(QPointF(rnd() * 64, rnd() * 42), 0.6, 0.6);
        }
    }
    p.setClipping(false);
    p.setPen(QPen(QColor(Theme::panelBorder()), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(0.5, 0.5, 63, 41), 5, 5);
    return px;
}

void popup(QWidget* parent, const QPoint& globalPos, const Choice& current,
           const std::function<void(const Choice&)>& onChange)
{
    QWidget* panel = nullptr;
    QDialog* dlg = makePopup(parent, &panel, QStringLiteral("lousaLookPanel"));
    panel->setStyleSheet(popupQss(QStringLiteral("lousaLookPanel")));
    auto* v = new QVBoxLayout(panel);
    v->setContentsMargins(16, 14, 16, 16);
    v->setSpacing(10);
    auto section = [&](const QString& t) {
        auto* l = new QLabel(t, panel);
        l->setProperty("role", QStringLiteral("section"));
        v->addWidget(l);
    };

    auto state = std::make_shared<Choice>(current);

    section(QCoreApplication::translate("LousaBoardLook", "FUNDO"));
    struct S { const char* id; const char* label; };
    static const S kStyles[] = {
        { "dots", QT_TRANSLATE_NOOP("LousaBoardLook", "Pontos") },
        { "grid", QT_TRANSLATE_NOOP("LousaBoardLook", "Grade") },
        { "lines", QT_TRANSLATE_NOOP("LousaBoardLook", "Caderno") },
        { "plain", QT_TRANSLATE_NOOP("LousaBoardLook", "Liso") },
        { "cork", QT_TRANSLATE_NOOP("LousaBoardLook", "Cortiça") },
        { "whiteboard", QT_TRANSLATE_NOOP("LousaBoardLook", "Lousa branca") },
    };
    auto* grid = new QGridLayout;
    grid->setSpacing(6);
    QList<QToolButton*> tiles;
    int i = 0;
    for (const S& s : kStyles) {
        auto* b = new QToolButton(panel);
        b->setObjectName(QStringLiteral("lousaLookTile"));
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setIcon(QIcon(stylePreview(QString::fromLatin1(s.id), current.color)));
        b->setIconSize(QSize(64, 42));
        b->setText(QCoreApplication::translate("LousaBoardLook", s.label));
        b->setCheckable(true);
        b->setChecked(current.style == QLatin1String(s.id));
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(84, 70);
        const QString id = QString::fromLatin1(s.id);
        QObject::connect(b, &QToolButton::clicked, dlg, [state, id, &tiles, onChange, b]() {
            for (QToolButton* t : tiles) t->setChecked(t == b);
            state->style = id;
            onChange(*state);
        });
        tiles << b;
        grid->addWidget(b, i / 3, i % 3);
        ++i;
    }
    v->addLayout(grid);

    section(QCoreApplication::translate("LousaBoardLook", "COR DO FUNDO"));
    auto* colorRow = new QHBoxLayout;
    colorRow->setSpacing(6);
    auto* themePill = new QToolButton(panel);
    themePill->setObjectName(QStringLiteral("lousaLookPill"));
    themePill->setText(QCoreApplication::translate("LousaBoardLook", "A da mesa do tema"));
    themePill->setCheckable(true);
    themePill->setChecked(!current.color.isValid());
    themePill->setCursor(Qt::PointingHandCursor);
    auto* customPill = new QToolButton(panel);
    customPill->setObjectName(QStringLiteral("lousaLookPill"));
    customPill->setText(QCoreApplication::translate("LousaBoardLook", "Outra cor…"));
    customPill->setCheckable(true);
    customPill->setChecked(current.color.isValid());
    customPill->setCursor(Qt::PointingHandCursor);
    auto refreshCustomIcon = [customPill, state]() {
        if (!state->color.isValid()) { customPill->setIcon(QIcon()); return; }
        QPixmap px(28, 28); px.setDevicePixelRatio(2.0); px.fill(Qt::transparent);
        QPainter pp(&px); pp.setRenderHint(QPainter::Antialiasing);
        pp.setPen(QPen(QColor(Theme::textMuted()), 1)); pp.setBrush(state->color);
        pp.drawEllipse(QPointF(7, 7), 6, 6);
        pp.end();
        customPill->setIcon(QIcon(px));
        customPill->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    };
    refreshCustomIcon();
    auto refreshTiles = [&tiles, state]() {
        static const char* ids[] = { "dots", "grid", "lines", "plain", "cork", "whiteboard" };
        for (int k = 0; k < tiles.size(); ++k)
            tiles[k]->setIcon(QIcon(stylePreview(QString::fromLatin1(ids[k]), state->color)));
    };
    QObject::connect(themePill, &QToolButton::clicked, dlg, [=]() {
        themePill->setChecked(true);
        customPill->setChecked(false);
        state->color = QColor();
        refreshCustomIcon();
        refreshTiles();
        onChange(*state);
    });
    // "Outra cor…" fecha este balão e abre o seletor de cor direto da Lousa,
    // como era antes do rework. Seletor aberto de dentro do balão (popup dentro
    // de popup) fazia o Windows devolver a vez pra janela principal ao fechar:
    // o app parecia minimizar e a Lousa voltava sem responder ao mouse.
    auto wantCustom = std::make_shared<bool>(false);
    QObject::connect(customPill, &QToolButton::clicked, dlg, [=]() {
        customPill->setChecked(state->color.isValid());
        *wantCustom = true;
        dlg->accept();
    });
    colorRow->addWidget(themePill);
    colorRow->addWidget(customPill);
    colorRow->addStretch(1);
    v->addLayout(colorRow);

    section(QCoreApplication::translate("LousaBoardLook", "CARDS"));
    auto* tilt = new QCheckBox(QCoreApplication::translate("LousaBoardLook", "Meio tortos, como presos num quadro"), panel);
    tilt->setChecked(current.tilt);
    QObject::connect(tilt, &QCheckBox::toggled, dlg, [state, onChange](bool on) { state->tilt = on; onChange(*state); });
    v->addWidget(tilt);
    auto* mini = new QCheckBox(QCoreApplication::translate("LousaBoardLook", "Minimapa no canto"), panel);
    mini->setChecked(current.minimap);
    QObject::connect(mini, &QCheckBox::toggled, dlg, [state, onChange](bool on) { state->minimap = on; onChange(*state); });
    v->addWidget(mini);

    placePopup(dlg, globalPos, QSize(300, 380));
    dlg->exec();
    dlg->deleteLater();

    if (*wantCustom) {
        const QColor start = state->color.isValid() ? state->color : QColor(Theme::appBackground());
        const QColor nc = ColorPopover::getColor(start, parent,
                                                 QCoreApplication::translate("LousaBoardLook", "Cor do fundo"));
        if (nc.isValid()) {
            state->color = nc;
            onChange(*state);
        }
    }
}

} // namespace LousaBoardLook

// ═════════════════════════════ Exportar ═════════════════════════════════════

LousaExportSheet::LousaExportSheet(LousaScene* scene, LousaView* view, QWidget* parent)
    : SheetDialog(parent, 480), m_scene(scene), m_view(view)
{
    setEyebrow(tr("EXPORTAR COMO IMAGEM"));
    m_zones = scene ? scene->allZoneData() : QList<CanvasZone>();

    body()->addWidget(sectionLabel(tr("O que entra na imagem"), this));
    QStringList modes = { tr("A lousa toda"), tr("Uma área"), tr("O que está na tela") };
    if (m_zones.isEmpty()) modes = { tr("A lousa toda"), tr("O que está na tela") };
    auto* modeChoice = new SheetChoice(modes, this);
    modeChoice->setCurrentIndex(0);
    body()->addWidget(modeChoice);
    connect(modeChoice, &SheetChoice::activated, this, [this](int i) {
        m_mode = m_zones.isEmpty() ? (i == 0 ? 0 : 2) : i;
        if (m_zoneRow) m_zoneRow->setVisible(m_mode == 1);
        refreshPreview();
    });

    if (!m_zones.isEmpty()) {
        m_zoneRow = new QWidget(this);
        auto* zl = new QVBoxLayout(m_zoneRow);
        zl->setContentsMargins(0, 0, 0, 0);
        zl->setSpacing(6);
        zl->addWidget(sectionLabel(tr("Qual área"), m_zoneRow));
        QStringList names;
        for (const CanvasZone& z : m_zones) names << (z.title.isEmpty() ? tr("Área sem nome") : z.title);
        auto* zc = new SheetChoice(names, m_zoneRow);
        zc->setCurrentIndex(0);
        connect(zc, &SheetChoice::activated, this, [this](int i) { m_zone = i; refreshPreview(); });
        zl->addWidget(zc);
        m_zoneRow->setVisible(false);
        body()->addWidget(m_zoneRow);
    }

    m_transparent = new QCheckBox(tr("Fundo transparente"), this);
    m_transparent->setObjectName(QStringLiteral("sheetCheck"));
    connect(m_transparent, &QCheckBox::toggled, this, [this]() { refreshPreview(); });
    body()->addWidget(m_transparent);

    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("lousaExportPreview"));
    m_preview->setFixedSize(432, 232);
    m_preview->setAlignment(Qt::AlignCenter);
    body()->addWidget(m_preview, 0, Qt::AlignHCenter);
    m_size = new QLabel(this);
    m_size->setObjectName(QStringLiteral("lousaExportSize"));
    m_size->setAlignment(Qt::AlignCenter);
    body()->addWidget(m_size);

    addFooter(tr("Salvar imagem…"), tr("Enter salva"));
    applySheetTheme(QStringLiteral(
        "QLabel#lousaExportPreview { background: %1; border: 1px solid %2; border-radius: 8px; }"
        "QLabel#lousaExportSize { color: %3; font-size: 12px; }"
        "QCheckBox#sheetCheck { font-size: 13px; }")
        .arg(Theme::inputBackground(), Theme::panelBorder(), Theme::textMuted()));
    refreshPreview();
}

QRectF LousaExportSheet::chosenRect() const
{
    constexpr qreal kMargin = 28.0;
    if (m_mode == 1 && m_zone >= 0 && m_zone < m_zones.size()) {
        const CanvasZone& z = m_zones[m_zone];
        return QRectF(z.x, z.y, z.width, z.height).adjusted(-kMargin, -kMargin - 14, kMargin, kMargin);
    }
    if (m_mode == 2 && m_view)
        return m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
    const QRectF b = m_scene ? m_scene->contentBounds() : QRectF();
    return b.adjusted(-kMargin, -kMargin, kMargin, kMargin);
}

QImage LousaExportSheet::render() const
{
    const QRectF r = chosenRect();
    if (!m_scene || !r.isValid() || r.isEmpty()) return QImage();
    constexpr qreal kMaxDimension = 6000.0;
    qreal scale = 2.0;
    const qreal longSide = qMax(r.width(), r.height()) * scale;
    if (longSide > kMaxDimension) scale *= kMaxDimension / longSide;
    QImage img(qMax(1, qRound(r.width() * scale)), qMax(1, qRound(r.height() * scale)),
               QImage::Format_ARGB32_Premultiplied);
    const bool transparent = m_transparent && m_transparent->isChecked();
    img.fill(transparent ? QColor(Qt::transparent) : m_scene->effectiveCanvasColor());
    m_scene->setSkipBackground(transparent);
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    m_scene->render(&painter, QRectF(0, 0, img.width(), img.height()), r);
    painter.end();
    m_scene->setSkipBackground(false);
    return img;
}

void LousaExportSheet::refreshPreview()
{
    const QRectF r = chosenRect();
    if (!m_scene || !r.isValid() || r.isEmpty()) {
        m_preview->setText(tr("Nada pra exportar."));
        m_size->clear();
        if (m_okBtn) m_okBtn->setEnabled(false);
        return;
    }
    if (m_okBtn) m_okBtn->setEnabled(true);
    const QSizeF target(m_preview->width() - 16, m_preview->height() - 16);
    const qreal sc = qMin(target.width() / r.width(), target.height() / r.height());
    QImage img(qMax(1, qRound(r.width() * sc * 2)), qMax(1, qRound(r.height() * sc * 2)),
               QImage::Format_ARGB32_Premultiplied);
    const bool transparent = m_transparent->isChecked();
    if (transparent) {
        // xadrez de "sem fundo"
        img.fill(QColor(200, 200, 200));
        QPainter cp(&img);
        for (int y = 0; y < img.height(); y += 12)
            for (int x = (y / 12) % 2 * 12; x < img.width(); x += 24)
                cp.fillRect(x, y, 12, 12, QColor(235, 235, 235));
    } else {
        img.fill(m_scene->effectiveCanvasColor());
    }
    m_scene->setSkipBackground(transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    m_scene->render(&p, QRectF(0, 0, img.width(), img.height()), r);
    p.end();
    m_scene->setSkipBackground(false);
    QPixmap px = QPixmap::fromImage(img);
    px.setDevicePixelRatio(2.0);
    m_preview->setPixmap(px);

    qreal scale = 2.0;
    const qreal longSide = qMax(r.width(), r.height()) * scale;
    if (longSide > 6000.0) scale *= 6000.0 / longSide;
    m_size->setText(tr("%1 × %2 px").arg(qRound(r.width() * scale)).arg(qRound(r.height() * scale)));
}

// ═════════════════════════════ Seletor de lousas ════════════════════════════

namespace LousaBoardPicker {

QString pick(QWidget* parent, const QPoint& globalPos, const QString& projectRoot,
             const QList<Entry>& boards, const QString& activeId, bool manage)
{
    QWidget* panel = nullptr;
    QDialog* dlg = makePopup(parent, &panel, QStringLiteral("lousaPickPanel"));
    panel->setStyleSheet(popupQss(QStringLiteral("lousaPickPanel")) + Theme::qss(QStringLiteral(
        "QToolButton#lousaPickTile { background: transparent; border: 1px solid transparent;"
        "  border-radius: @radius-control; color: %1; font-size: 12px; padding: 6px; }"
        "QToolButton#lousaPickTile:hover { background: %2; }"
        "QToolButton#lousaPickTile:checked { border-color: %3; color: %4; }")
        .arg(Theme::textPrimary(), Theme::hoverOverlay(), Theme::accentDefault(), Theme::textBright())));
    auto* v = new QVBoxLayout(panel);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(10);
    auto* title = new QLabel(QCoreApplication::translate("LousaBoardPicker", "QUAL LOUSA"), panel);
    title->setProperty("role", QStringLiteral("section"));
    v->addWidget(title);
    auto* grid = new QGridLayout;
    grid->setSpacing(8);
    QString picked;
    const QColor themeBg(Theme::appBackground());
    const int tiles = int(boards.size()) + (manage ? 1 : 0);
    const int cols = tiles <= 4 ? qMax(1, tiles) : 4;
    for (int i = 0; i < boards.size(); ++i) {
        const Entry& b = boards[i];
        const LousaDraw::Snapshot snap = LousaDraw::fromFile(QDir(projectRoot).filePath(b.file), themeBg);
        QPixmap px(360, 224);
        px.setDevicePixelRatio(2.0);
        px.fill(Qt::transparent);
        {
            QPainter p(&px);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath clip; clip.addRoundedRect(QRectF(0, 0, 180, 112), 6, 6);
            p.setClipPath(clip);
            p.fillRect(QRectF(0, 0, 180, 112), snap.bg.isValid() ? snap.bg : themeBg);
            LousaDraw::draw(&p, QRectF(8, 8, 164, 96), snap, QRectF());
            if (snap.cards.isEmpty() && snap.zones.isEmpty() && snap.inks.isEmpty()) {
                p.setPen(QColor(Theme::textMuted()));
                p.drawText(QRectF(0, 0, 180, 112), Qt::AlignCenter,
                           QCoreApplication::translate("LousaBoardPicker", "vazia"));
            }
        }
        auto* t = new QToolButton(panel);
        t->setObjectName(QStringLiteral("lousaPickTile"));
        t->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        t->setIcon(QIcon(px));
        t->setIconSize(QSize(180, 112));
        t->setText(b.name);
        t->setCheckable(true);
        t->setChecked(b.id == activeId);
        t->setCursor(Qt::PointingHandCursor);
        const QString id = b.id;
        QObject::connect(t, &QToolButton::clicked, dlg, [dlg, &picked, id]() { picked = id; dlg->accept(); });
        if (manage) {
            t->setContextMenuPolicy(Qt::CustomContextMenu);
            QObject::connect(t, &QToolButton::customContextMenuRequested, dlg, [dlg, t, &picked, id](const QPoint& p) {
                QMenu menu(t);
                QAction* ren = menu.addAction(QCoreApplication::translate("LousaBoardPicker", "Renomear…"));
                QAction* del = menu.addAction(QCoreApplication::translate("LousaBoardPicker", "Excluir…"));
                QAction* ch = menu.exec(t->mapToGlobal(p));
                if (ch == ren) { picked = QStringLiteral("__rename__:") + id; dlg->accept(); }
                else if (ch == del) { picked = QStringLiteral("__delete__:") + id; dlg->accept(); }
            });
        }
        grid->addWidget(t, i / cols, i % cols);
    }
    if (manage) {
        QPixmap px(360, 224);
        px.setDevicePixelRatio(2.0);
        px.fill(Qt::transparent);
        {
            QPainter p(&px);
            p.setRenderHint(QPainter::Antialiasing);
            QPen pen(QColor(Theme::panelBorder()), 1.5, Qt::DashLine);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(1, 1, 178, 110), 6, 6);
            p.setPen(QColor(Theme::textMuted()));
            QFont f(QStringLiteral("Segoe UI")); f.setPixelSize(26);
            p.setFont(f);
            p.drawText(QRectF(0, 0, 180, 112), Qt::AlignCenter, QStringLiteral("+"));
        }
        auto* t = new QToolButton(panel);
        t->setObjectName(QStringLiteral("lousaPickTile"));
        t->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        t->setIcon(QIcon(px));
        t->setIconSize(QSize(180, 112));
        t->setText(QCoreApplication::translate("LousaBoardPicker", "Nova lousa"));
        t->setCursor(Qt::PointingHandCursor);
        QObject::connect(t, &QToolButton::clicked, dlg, [dlg, &picked]() { picked = QStringLiteral("__new__"); dlg->accept(); });
        const int i = int(boards.size());
        grid->addWidget(t, i / cols, i % cols);
    }
    v->addLayout(grid);
    if (manage) {
        auto* hint = new QLabel(QCoreApplication::translate("LousaBoardPicker", "Botão direito numa lousa pra renomear ou excluir."), panel);
        hint->setStyleSheet(QStringLiteral("font-size: 11px;"));
        v->addWidget(hint);
    }
    const int rows = (tiles + cols - 1) / cols;
    placePopup(dlg, globalPos, QSize(28 + cols * 204, 60 + rows * 150 + (manage ? 22 : 0)));
    dlg->exec();
    dlg->deleteLater();
    return picked;
}

} // namespace LousaBoardPicker
