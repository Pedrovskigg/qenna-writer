#include "GroupsPanel.h"
#include "GroupBadge.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

namespace {

static const QStringList kDefaultColors = {
    QStringLiteral("#E57373"), QStringLiteral("#FFB74D"), QStringLiteral("#FFF176"),
    QStringLiteral("#81C784"), QStringLiteral("#4FC3F7"), QStringLiteral("#CE93D8"),
    QStringLiteral("#F06292"), QStringLiteral("#80CBC4"), QStringLiteral("#A1887F"),
};

constexpr int kRowH = 32, kIndent = 18, kPad = 6;

QColor themeColor(const QString& c) { return Theme::toColor(c); }

QPixmap badgePixmap(const QString& color, bool social, int size, qreal dpr)
{
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    GroupBadge::draw(p, QPointF(size / 2.0, size / 2.0), size / 2.0 - 0.5, QColor(color), social);
    return pm;
}

// quantos documentos de gaveta moram em cada grupo
QHash<QString, int> docCounts(const ProjectModel* m)
{
    QHash<QString, int> out;
    if (m) for (const Drawer& d : m->drawers()) for (const DrawerItem& it : d.items) if (!it.markerId.isEmpty()) out[it.markerId]++;
    return out;
}

// campo de renomear que desiste no Esc
class RenameEdit : public QLineEdit {
public:
    using QLineEdit::QLineEdit;
    QString original;
    bool cancelled = false;
protected:
    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Escape) { cancelled = true; setText(original); clearFocus(); return; }
        QLineEdit::keyPressEvent(e);
    }
};

} // namespace

// ── a árvore: linhas pintadas à mão, com recuo, selo, contagem e o "+" do filho ──
namespace GroupsUi {

class Tree : public QWidget {
public:
    Tree(ProjectModel* m, QWidget* parent) : QWidget(parent), m_model(m)
    {
        setMouseTracking(true);
        setAttribute(Qt::WA_Hover);
        setContextMenuPolicy(Qt::PreventContextMenu);
        m_scrollTimer = new QTimer(this);
        m_scrollTimer->setInterval(30);
        QObject::connect(m_scrollTimer, &QTimer::timeout, this, [this]() { autoScroll(); });
    }

    QString selected;
    QPointer<QScrollArea> scroll;
    QPointer<QLabel> foot;
    std::function<void(const QString&)> onSelect;
    std::function<void(const QString&, const QPoint&)> onAdd, onContext;
    std::function<void(const QString&, const QString&)> onMove, onRename;
    std::function<void(bool)> onFootHot;

    void rebuild()
    {
        m_rows.clear();
        const QHash<QString, int> counts = docCounts(m_model);
        if (m_model) for (const auto& [g, depth] : m_model->groupTree())
            m_rows.push_back({ g->id, g->title, g->color, depth, g->social(), g->map, counts.value(g->id), !g->parentId.isEmpty() });
        setFixedHeight(std::max(60, int(m_rows.size()) * kRowH + 2 * kPad));
        if (m_hover >= m_rows.size()) m_hover = -1;
        update();
    }

    void ensureVisible(const QString& id)
    {
        const int i = indexOf(id);
        if (i >= 0 && scroll) scroll->ensureVisible(0, rowRect(i).center().y(), 0, kRowH);
    }

    void startRename(const QString& id)
    {
        const int i = indexOf(id);
        if (i < 0) return;
        ensureVisible(id);
        delete m_edit;
        auto* ed = new RenameEdit(this);
        m_edit = ed;
        m_editId = id;
        ed->original = m_rows[i].title;
        ed->setText(m_rows[i].title);
        ed->setObjectName(QStringLiteral("groupRename"));
        const QRect nr = nameRect(i);
        ed->setGeometry(nr.left() - 4, rowRect(i).top() + 4, rowRect(i).right() - 34 - nr.left(), kRowH - 10);
        ed->show();
        ed->setFocus();
        ed->selectAll();
        QObject::connect(ed, &QLineEdit::editingFinished, this, [this, ed]() {
            if (m_edit != ed || m_editId.isEmpty()) return;
            const QString id = m_editId, text = ed->text().trimmed();
            const bool keep = !ed->cancelled && !text.isEmpty() && text != ed->original;
            m_editId.clear();
            ed->hide();
            ed->deleteLater();
            m_edit = nullptr;
            if (keep && onRename) onRename(id, text);
        });
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor ink = themeColor(Theme::textPrimary()), bright = themeColor(Theme::textBright()),
                     muted = themeColor(Theme::textMuted()), line = themeColor(Theme::subtleBorder()),
                     accent = themeColor(Theme::accentDefault()), hover = themeColor(Theme::hoverOverlay());
        QColor soft = accent; soft.setAlphaF(0.16);
        QColor softLine = accent; softLine.setAlphaF(0.5);
        if (m_rows.isEmpty()) {
            p.setPen(muted);
            QFont f = font(); f.setPixelSize(12); f.setItalic(true); p.setFont(f);
            p.drawText(rect().adjusted(12, 0, -12, 0), Qt::AlignCenter | Qt::TextWordWrap, GroupsPanel::tr("Nenhum grupo criado"));
            return;
        }
        QFont nf = font(); nf.setPixelSize(13);
        QFont sf = font(); sf.setPixelSize(11);
        const QString dragId = m_dragging && m_press >= 0 ? m_rows[m_press].id : QString();
        for (int i = 0; i < m_rows.size(); i++) {
            const Row& r = m_rows[i];
            const QRectF rr = QRectF(rowRect(i)).adjusted(0.5, 0.5, -0.5, -0.5);
            const bool sel = r.id == selected, hov = i == m_hover && !m_dragging;
            p.setOpacity(r.id == dragId ? 0.4 : 1.0);
            if (i == m_drop) { p.setBrush(soft); p.setPen(QPen(accent, 1.5)); p.drawRoundedRect(rr, 8, 8); }
            else if (sel) { p.setBrush(soft); p.setPen(QPen(softLine, 1)); p.drawRoundedRect(rr, 8, 8); }
            else if (hov) { p.setBrush(hover); p.setPen(Qt::NoPen); p.drawRoundedRect(rr, 8, 8); }
            // um fio entre as raízes (o mapa e os grupos soltos)
            if (r.depth == 0 && i > 0) {
                p.setPen(QPen(line, 1));
                p.drawLine(QPointF(rr.left() + 8, rr.top() - 1.5), QPointF(rr.right() - 8, rr.top() - 1.5));
            }
            // as guias de cada nível
            p.setPen(QPen(line, 1));
            for (int k = 1; k <= r.depth; k++) {
                const double x = rr.left() + k * kIndent - 5 + 0.5;
                p.drawLine(QPointF(x, rr.top() - 1), QPointF(x, rr.bottom() + 1));
            }
            const double x0 = rr.left() + 6 + r.depth * kIndent;
            // o pegador (quem pode ser arrastado)
            if (hov && !r.map) {
                p.setPen(Qt::NoPen); p.setBrush(muted);
                for (int a = 0; a < 2; a++) for (int b = 0; b < 3; b++)
                    p.drawEllipse(QPointF(x0 + 4 + a * 5, rr.center().y() - 5 + b * 5), 1.2, 1.2);
            }
            GroupBadge::draw(p, QPointF(x0 + 14 + 7 + 8, rr.center().y()), 8, QColor(r.color), r.social);
            // contagem, "mapa" e o "+"
            double right = rr.right() - 8;
            const bool showAdd = (hov || sel) && m_editId != r.id;
            if (showAdd) {
                const QRectF a = addRect(i);
                if (a.contains(m_mouse)) { p.setPen(Qt::NoPen); p.setBrush(hover); p.drawRoundedRect(a, 6, 6); }
                p.setPen(QPen(a.contains(m_mouse) ? bright : muted, 1.5, Qt::SolidLine, Qt::RoundCap));
                const QPointF c = a.center();
                p.drawLine(QPointF(c.x() - 4.5, c.y()), QPointF(c.x() + 4.5, c.y()));
                p.drawLine(QPointF(c.x(), c.y() - 4.5), QPointF(c.x(), c.y() + 4.5));
            }
            right = rr.right() - 34;
            p.setFont(sf);
            if (r.count > 0) {
                const QString t = QString::number(r.count);
                const double w = p.fontMetrics().horizontalAdvance(t);
                p.setPen(muted);
                p.drawText(QRectF(right - w, rr.top(), w, rr.height()), Qt::AlignVCenter | Qt::AlignRight, t);
                right -= w + 8;
            }
            if (r.map) {
                const QString t = GroupsPanel::tr("mapa");
                const double w = p.fontMetrics().horizontalAdvance(t) + 14;
                const QRectF pill(right - w, rr.center().y() - 9, w, 18);
                p.setBrush(Qt::NoBrush); p.setPen(QPen(themeColor(Theme::panelBorder()), 1));
                p.drawRoundedRect(pill, 9, 9);
                p.setPen(muted);
                p.drawText(pill, Qt::AlignCenter, t);
                right -= w + 8;
            }
            if (m_editId != r.id) {
                p.setFont(nf);
                p.setPen(sel ? bright : ink);
                const QRect nr = nameRect(i);
                const int w = std::max(10, int(right) - nr.left());
                p.drawText(QRect(nr.left(), nr.top(), w, nr.height()), Qt::AlignVCenter | Qt::AlignLeft,
                           p.fontMetrics().elidedText(r.title, Qt::ElideRight, w));
            }
        }
        p.setOpacity(1);
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        const int i = rowAt(e->position().toPoint());
        if (e->button() == Qt::RightButton) {
            if (i >= 0 && onContext) onContext(m_rows[i].id, e->globalPosition().toPoint());
            return;
        }
        if (e->button() != Qt::LeftButton) return;
        if (i >= 0 && (m_rows[i].id == selected || i == m_hover) && addRect(i).contains(e->position())) {
            if (onAdd) onAdd(m_rows[i].id, mapToGlobal(addRect(i).bottomLeft().toPoint() + QPoint(0, 4)));
            return;
        }
        m_press = i;
        m_pressAt = e->position().toPoint();
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        m_mouse = e->position();
        const QPoint pos = e->position().toPoint();
        if (m_press >= 0 && (e->buttons() & Qt::LeftButton)) {
            if (!m_dragging && (pos - m_pressAt).manhattanLength() >= QApplication::startDragDistance() && !m_rows[m_press].map) {
                m_dragging = true;
                setCursor(Qt::ClosedHandCursor);
                m_scrollTimer->start();
            }
            if (m_dragging) { updateDrop(e->globalPosition().toPoint()); return; }
        }
        const int h = rowAt(pos);
        if (h != m_hover) m_hover = h;
        update();
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        const int press = m_press;
        m_press = -1;
        if (m_dragging) {
            m_dragging = false;
            m_scrollTimer->stop();
            unsetCursor();
            const QString id = m_rows[press].id;
            const int drop = m_drop;
            const bool top = m_dropTop;
            m_drop = -1; m_dropTop = false;
            if (onFootHot) onFootHot(false);
            update();
            if (top && onMove) onMove(id, QString());
            else if (drop >= 0 && onMove) onMove(id, m_rows[drop].id);
            return;
        }
        const int i = rowAt(e->position().toPoint());
        if (i >= 0 && i == press && onSelect) onSelect(m_rows[i].id);
    }

    void mouseDoubleClickEvent(QMouseEvent* e) override
    {
        const int i = rowAt(e->position().toPoint());
        if (i >= 0 && !addRect(i).contains(e->position())) startRename(m_rows[i].id);
    }

    void leaveEvent(QEvent*) override { m_hover = -1; m_mouse = QPointF(-1, -1); update(); }

private:
    struct Row { QString id, title, color; int depth = 0; bool social = false, map = false; int count = 0; bool hasParent = false; };

    int indexOf(const QString& id) const
    {
        for (int i = 0; i < m_rows.size(); i++) if (m_rows[i].id == id) return i;
        return -1;
    }
    QRect rowRect(int i) const { return QRect(kPad, kPad + i * kRowH, width() - 2 * kPad, kRowH - 2); }
    QRectF addRect(int i) const { const QRect r = rowRect(i); return QRectF(r.right() - 28, r.center().y() - 11, 22, 22); }
    QRect nameRect(int i) const
    {
        const QRect r = rowRect(i);
        const int x = r.left() + 6 + m_rows[i].depth * kIndent + 14 + 7 + 16 + 8;
        return QRect(x, r.top(), r.right() - x, r.height());
    }
    int rowAt(QPoint p) const
    {
        if (p.y() < kPad) return -1;
        const int i = (p.y() - kPad) / kRowH;
        return i >= 0 && i < m_rows.size() && p.x() >= kPad && p.x() <= width() - kPad ? i : -1;
    }

    // onde o arrastado cai: dentro de outro grupo, ou no rodapé (sem pai)
    void updateDrop(const QPoint& global)
    {
        const QString id = m_rows[m_press].id;
        m_drop = -1;
        m_dropTop = false;
        if (foot && foot->isVisible() && foot->rect().contains(foot->mapFromGlobal(global))) {
            m_dropTop = m_rows[m_press].hasParent;
        } else {
            const int i = rowAt(mapFromGlobal(global));
            if (i >= 0 && m_model && !m_model->groupIsInside(m_rows[i].id, id)) {
                const Group* g = m_model->findGroup(id);
                if (g && g->parentId != m_rows[i].id) m_drop = i;
            }
        }
        if (onFootHot) onFootHot(m_dropTop);
        m_lastGlobal = global;
        update();
    }
    void autoScroll()
    {
        if (!m_dragging || !scroll) return;
        QWidget* vp = scroll->viewport();
        const QPoint v = vp->mapFromGlobal(QCursor::pos());
        int dy = 0;
        if (v.y() < 24 && v.y() > -40) dy = -8;
        else if (v.y() > vp->height() - 24 && v.y() < vp->height() + 10) dy = 8;
        if (!dy) return;
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->value() + dy);
        updateDrop(QCursor::pos());
    }

    ProjectModel* m_model;
    QVector<Row> m_rows;
    int m_hover = -1, m_press = -1, m_drop = -1;
    bool m_dragging = false, m_dropTop = false;
    QPoint m_pressAt, m_lastGlobal;
    QPointF m_mouse{ -1, -1 };
    QTimer* m_scrollTimer = nullptr;
    QPointer<RenameEdit> m_edit;
    QString m_editId;
};

} // namespace GroupsUi

GroupsPanel::GroupsPanel(ProjectModel* model, QWidget* parent)
    : QFrame(parent), m_model(model)
{
    setObjectName(QStringLiteral("groupsPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFrameShape(QFrame::NoFrame);

    buildUi();

    if (m_model) {
        connect(m_model, &ProjectModel::groupsChanged,  this, &GroupsPanel::onGroupsChanged);
        connect(m_model, &ProjectModel::drawersChanged, this, &GroupsPanel::onGroupsChanged);
        connect(m_model, &ProjectModel::loaded,         this, &GroupsPanel::onGroupsChanged);
    }
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &GroupsPanel::applyTheme);
}

void GroupsPanel::buildUi()
{
    auto* rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    // ---- Header ----
    m_header = new QWidget(this);
    m_header->setFixedHeight(38);
    m_header->setCursor(Qt::SizeAllCursor);
    m_header->installEventFilter(this);
    auto* hLay = new QHBoxLayout(m_header);
    hLay->setContentsMargins(12, 0, 6, 0);
    hLay->setSpacing(6);

    m_title = new QLabel(tr("Grupos"), m_header);
    hLay->addWidget(m_title);
    hLay->addStretch();

    m_newBtn = new QToolButton(m_header);
    m_newBtn->setText(QStringLiteral("+"));
    m_newBtn->setToolTip(tr("Novo grupo"));
    m_newBtn->setCursor(Qt::PointingHandCursor);
    m_newBtn->setFixedSize(26, 26);
    hLay->addWidget(m_newBtn);
    connect(m_newBtn, &QToolButton::clicked, this, [this]() {
        showNewMenu(QString(), m_newBtn->mapToGlobal(QPoint(0, m_newBtn->height() + 4)));
    });

    m_closeBtn = new QToolButton(m_header);
    m_closeBtn->setText(QStringLiteral("×"));
    m_closeBtn->setToolTip(tr("Fechar"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setFixedSize(26, 26);
    hLay->addWidget(m_closeBtn);
    connect(m_closeBtn, &QToolButton::clicked, this, &GroupsPanel::closeRequested);

    rootLay->addWidget(m_header);

    auto* sep = new QFrame(this);
    sep->setObjectName(QStringLiteral("gpSep"));
    sep->setFixedHeight(1);
    rootLay->addWidget(sep);

    // ---- corpo: árvore | grupo escolhido ----
    auto* body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    auto* left = new QWidget(this);
    left->setObjectName(QStringLiteral("gpLeft"));
    left->setFixedWidth(290);
    auto* ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(0);
    m_treeScroll = new QScrollArea(left);
    m_treeScroll->setObjectName(QStringLiteral("gpScroll"));
    m_treeScroll->setWidgetResizable(true);
    m_treeScroll->setFrameShape(QFrame::NoFrame);
    m_treeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tree = new GroupsUi::Tree(m_model, m_treeScroll);
    m_tree->scroll = m_treeScroll;
    m_treeScroll->setWidget(m_tree);
    ll->addWidget(m_treeScroll, 1);
    m_treeFoot = new QLabel(tr("Arraste um grupo pra dentro de outro pra mudar o pai. Solte aqui pra ele ficar solto, sem pai."), left);
    m_treeFoot->setObjectName(QStringLiteral("gpFoot"));
    m_treeFoot->setWordWrap(true);
    m_treeFoot->setProperty("hot", false);
    ll->addWidget(m_treeFoot);
    m_tree->foot = m_treeFoot;
    body->addWidget(left);

    auto* vsep = new QFrame(this);
    vsep->setObjectName(QStringLiteral("gpSep"));
    vsep->setFixedWidth(1);
    body->addWidget(vsep);

    m_detailScroll = new QScrollArea(this);
    m_detailScroll->setObjectName(QStringLiteral("gpScroll"));
    m_detailScroll->setWidgetResizable(true);
    m_detailScroll->setFrameShape(QFrame::NoFrame);
    m_detailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_detailHost = new QWidget(m_detailScroll);
    m_detailHost->setObjectName(QStringLiteral("gpDetail"));
    m_detailLay = new QVBoxLayout(m_detailHost);
    m_detailLay->setContentsMargins(18, 14, 18, 16);
    m_detailLay->setSpacing(5);
    m_detailScroll->setWidget(m_detailHost);
    body->addWidget(m_detailScroll, 1);
    rootLay->addLayout(body, 1);

    // ---- a árvore conversa com o modelo ----
    m_tree->onSelect = [this](const QString& id) { select(id); };
    m_tree->onAdd = [this](const QString& id, const QPoint& at) { showNewMenu(id, at); };
    m_tree->onContext = [this](const QString& id, const QPoint& at) { select(id); showGroupContextMenu(id, at); };
    m_tree->onRename = [this](const QString& id, const QString& title) {
        if (const Group* g = m_model->findGroup(id)) m_model->updateGroup(id, title, g->color);
    };
    m_tree->onMove = [this](const QString& id, const QString& parent) {
        if (m_model->setGroupParent(id, parent)) select(id);
    };
    m_tree->onFootHot = [this](bool hot) {
        if (m_treeFoot->property("hot").toBool() == hot) return;
        m_treeFoot->setProperty("hot", hot);
        m_treeFoot->style()->unpolish(m_treeFoot);
        m_treeFoot->style()->polish(m_treeFoot);
    };

    applyTheme();
}

void GroupsPanel::applyTheme()
{
    QColor soft = themeColor(Theme::accentDefault()); soft.setAlphaF(0.14);
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QFrame#groupsPanel { background: %1; border: 1px solid %2; border-radius: @radius-panel; }
        QFrame#gpSep { background: %2; border: none; }
        QScrollArea#gpScroll, QWidget#gpDetail { background: transparent; border: none; }
        QScrollArea#gpScroll > QWidget > QWidget { background: transparent; }
        QWidget#gpDetail QLabel { background: transparent; }
        QWidget#gpLeft { background: transparent; }
        QLabel#gpTitle { color: %3; font-family: 'Lora','Crimson Text',serif; font-size: 13px; font-weight: 700; }
        QLabel#gpFoot { color: %4; font-size: 11px; padding: 9px 14px 11px; border-top: 1px solid %2; }
        QLabel#gpFoot[hot="true"] { color: %5; background: %6; border: 1px solid %5; }
        QToolButton#gpNew { background: transparent; color: %4; border: 1px solid %2; border-radius: @radius-control; font-size: 18px; font-weight: 300; }
        QToolButton#gpNew:hover, QToolButton#gpClose:hover { background: %7; color: %3; }
        QToolButton#gpClose { background: transparent; color: %4; border: none; border-radius: @radius-control; font-size: 16px; }
        QLabel#gdTitle { color: %3; font-family: 'Lora','Crimson Text',Georgia,serif; font-size: 19px; font-weight: 600; }
        QLabel#gdPath { color: %4; font-size: 11.5px; }
        QLabel#gdCaption { color: %4; font-size: 10.5px; font-weight: 600; letter-spacing: 1px; padding-top: 12px; }
        QLabel#gdEmpty { color: %4; font-size: 12px; font-style: italic; }
        QFrame#gdRow { background: %8; border: 1px solid %9; border-radius: @radius-panel; }
        QFrame#gdRow:hover { background: %7; border-color: %10; }
        QFrame#gdRowUp { background: transparent; border: 1px dashed %9; border-radius: @radius-panel; }
        QFrame#gdRowUp:hover { background: %7; border-color: %10; }
        QLabel#gdRowTitle { color: %11; font-size: 12.5px; background: transparent; border: none; }
        QLabel#gdDrawer { color: %4; background: %7; border-radius: 8px; padding: 1px 7px; font-size: 10.5px; border: none; }
        QLabel#gdFrom { color: %5; font-size: 10.5px; background: transparent; border: none; }
        QPushButton#gdChip { background: transparent; color: %11; border: 1px solid %9; border-radius: 12px; padding: 3px 10px 3px 6px; font-size: 12px; text-align: left; }
        QPushButton#gdChip:hover { border-color: %5; }
        QPushButton#gdLink { background: transparent; color: %4; border: none; font-size: 11.5px; padding: 2px 0; text-align: left; }
        QPushButton#gdLink:hover { color: %5; }
        QToolButton#gdSeg { background: transparent; color: %4; border: none; border-radius: 7px; padding: 4px 9px; font-size: 12px; }
        QToolButton#gdSeg:checked { background: %6; color: %5; font-weight: 600; }
        QToolButton#gdSeg:disabled { color: %4; }
        QFrame#gdSegBox { border: 1px solid %9; border-radius: 9px; background: transparent; }
        QLineEdit#groupRename { background: %12; color: %3; border: 1px solid %5; border-radius: 6px; padding: 1px 5px; font-size: 13px; }
    )")).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textBright(), Theme::textMuted(),
            Theme::accentDefault(), QStringLiteral("rgba(%1,%2,%3,0.14)").arg(soft.red()).arg(soft.green()).arg(soft.blue()),
            Theme::hoverOverlay(), Theme::pressedOverlay(), Theme::subtleBorder(), Theme::borderStrong(),
            Theme::textPrimary(), Theme::inputBackground()));
    m_title->setObjectName(QStringLiteral("gpTitle"));
    m_newBtn->setObjectName(QStringLiteral("gpNew"));
    m_closeBtn->setObjectName(QStringLiteral("gpClose"));
    for (QWidget* w : findChildren<QWidget*>()) { w->style()->unpolish(w); w->style()->polish(w); }
    onGroupsChanged();
}

void GroupsPanel::onGroupsChanged()
{
    if (!m_model) return;
    // o escolhido sumiu (ou nada escolhido ainda): o primeiro da árvore
    if (m_selectedGroupId.isEmpty() || !m_model->findGroup(m_selectedGroupId)) {
        const auto tree = m_model->groupTree();
        m_selectedGroupId = tree.isEmpty() ? QString() : tree.first().first->id;
    }
    m_tree->selected = m_selectedGroupId;
    m_tree->rebuild();
    m_treeFoot->setVisible(!m_model->groups().isEmpty());
    rebuildDetail();
}

void GroupsPanel::select(const QString& groupId)
{
    m_selectedGroupId = groupId;
    m_tree->selected = groupId;
    m_tree->update();
    m_tree->ensureVisible(groupId);
    rebuildDetail();
}

QString GroupsPanel::nextColor() const
{
    return kDefaultColors[(m_model ? m_model->groups().size() : 0) % kDefaultColors.size()];
}

void GroupsPanel::rebuildDetail()
{
    while (QLayoutItem* it = m_detailLay->takeAt(0)) {
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        else if (QLayout* l = it->layout()) {
            while (QLayoutItem* s = l->takeAt(0)) { if (QWidget* w = s->widget()) { w->hide(); w->deleteLater(); } delete s; }
            delete l;
            continue;
        }
        delete it;
    }
    const Group* g = m_model ? m_model->findGroup(m_selectedGroupId) : nullptr;
    if (!g) {
        auto* hint = new QLabel(tr("Crie um grupo com o botão +"), m_detailHost);
        hint->setObjectName(QStringLiteral("gdEmpty"));
        hint->setAlignment(Qt::AlignCenter);
        m_detailLay->addStretch(1);
        m_detailLay->addWidget(hint);
        m_detailLay->addStretch(2);
        return;
    }
    const QString gid = g->id;
    const qreal dpr = devicePixelRatioF();

    // título + Lugar/Social
    auto* head = new QHBoxLayout();
    head->setSpacing(10);
    auto* title = new QLabel(g->title, m_detailHost);
    title->setObjectName(QStringLiteral("gdTitle"));
    title->setWordWrap(true);
    head->addWidget(title, 1);
    auto* segBox = new QFrame(m_detailHost);
    segBox->setObjectName(QStringLiteral("gdSegBox"));
    auto* sl = new QHBoxLayout(segBox);
    sl->setContentsMargins(2, 2, 2, 2);
    sl->setSpacing(2);
    for (const QString& k : { QStringLiteral("place"), QStringLiteral("social") }) {
        auto* b = new QToolButton(segBox);
        b->setObjectName(QStringLiteral("gdSeg"));
        b->setCheckable(true);
        b->setChecked(g->kind == k);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setText(k == QLatin1String("social") ? tr("Social") : tr("Lugar"));
        b->setIcon(QIcon(badgePixmap(g->color, k == QLatin1String("social"), 14, dpr)));
        b->setIconSize(QSize(14, 14));
        b->setToolTip(k == QLatin1String("social") ? tr("Gente: família, facção, ordem, elenco") : tr("Tem chão: reino, região, cidade, planeta"));
        if (g->map) { b->setEnabled(false); b->setToolTip(tr("O grupo do mapa é o próprio mapa: ele é sempre um Lugar")); }
        connect(b, &QToolButton::clicked, this, [this, gid, k]() { m_model->setGroupKind(gid, k); });
        sl->addWidget(b);
    }
    head->addWidget(segBox, 0, Qt::AlignTop);
    m_detailLay->addLayout(head);

    // o caminho (e se está fora do mapa, quando o projeto tem mapa)
    bool hasMap = false, onMap = false;
    for (const Group& o : m_model->groups()) if (o.map) hasMap = true;
    for (const Group* a : m_model->groupPath(gid)) if (a->map) onMap = true;
    QString pathText = m_model->groupPathText(gid);
    if (hasMap && !onMap) pathText += QStringLiteral(" · ") + tr("fora do mapa");
    auto* pathLbl = new QLabel(pathText, m_detailHost);
    pathLbl->setObjectName(QStringLiteral("gdPath"));
    pathLbl->setWordWrap(true);
    m_detailLay->addWidget(pathLbl);

    auto caption = [this](const QString& t) {
        auto* c = new QLabel(t.toUpper(), m_detailHost);
        c->setObjectName(QStringLiteral("gdCaption"));
        m_detailLay->addWidget(c);
    };
    auto docRow = [this](const DrawerItem& it, const Drawer& dr, const QString& from) {
        auto* row = new QFrame(m_detailHost);
        row->setObjectName(from.isEmpty() ? QStringLiteral("gdRow") : QStringLiteral("gdRowUp"));
        row->setCursor(Qt::PointingHandCursor);
        row->setAttribute(Qt::WA_StyledBackground, true);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(10, 6, 10, 6);
        rl->setSpacing(8);
        auto* t = new QLabel(row);
        t->setObjectName(QStringLiteral("gdRowTitle"));
        const QString name = it.title.isEmpty() ? tr("(sem nome)") : it.title;
        t->setText(t->fontMetrics().elidedText(name, Qt::ElideRight, 230));
        t->setToolTip(name);
        rl->addWidget(t, 1);
        auto* r = new QLabel(from.isEmpty() ? (dr.title.isEmpty() ? tr("(gaveta)") : dr.title) : tr("de %1").arg(from), row);
        r->setObjectName(from.isEmpty() ? QStringLiteral("gdDrawer") : QStringLiteral("gdFrom"));
        rl->addWidget(r, 0, Qt::AlignVCenter);
        for (QWidget* w : { static_cast<QWidget*>(t), static_cast<QWidget*>(r) }) w->setAttribute(Qt::WA_TransparentForMouseEvents);
        row->setProperty("drawerKey", dr.key);
        row->setProperty("itemId", it.id);
        row->installEventFilter(this);
        m_detailLay->addWidget(row);
    };
    auto chip = [this, dpr](QLayout* lay, const Group* o, const QString& text, const std::function<void()>& fn) {
        auto* b = new QPushButton(text, m_detailHost);
        b->setObjectName(QStringLiteral("gdChip"));
        b->setCursor(Qt::PointingHandCursor);
        b->setIcon(QIcon(badgePixmap(o->color, o->social(), 14, dpr)));
        b->setIconSize(QSize(14, 14));
        b->setToolTip(m_model->groupPathText(o->id));
        connect(b, &QPushButton::clicked, this, fn);
        lay->addWidget(b);
        return b;
    };
    // fileira de etiquetas que quebra linha (no máximo 2 por linha, o painel é estreito)
    auto chipRows = [this](const QList<std::function<void(QLayout*)>>& makers) {
        QHBoxLayout* line = nullptr;
        int n = 0;
        for (const auto& mk : makers) {
            if (!line || n == 2) { line = new QHBoxLayout(); line->setSpacing(6); line->setContentsMargins(0, 0, 0, 0); m_detailLay->addLayout(line); n = 0; }
            mk(line);
            n++;
        }
        if (line) line->addStretch(1);
    };

    // 1) deste grupo
    caption(tr("Deste grupo"));
    int own = 0;
    for (const Drawer& dr : m_model->drawers())
        for (const DrawerItem& it : dr.items) if (it.markerId == gid) { docRow(it, dr, QString()); own++; }
    if (!own) {
        auto* e = new QLabel(tr("Nenhum documento ainda."), m_detailHost);
        e->setObjectName(QStringLiteral("gdEmpty"));
        m_detailLay->addWidget(e);
    }

    // 2) atua em (e quem atua aqui)
    QList<std::function<void(QLayout*)>> acts;
    for (const QString& pid : g->actsIn) {
        const Group* p = m_model->findGroup(pid);
        if (!p) continue;
        acts << [this, chip, p, gid](QLayout* l) {
            chip(l, p, p->title + QStringLiteral("  ×"), [this, gid, pid = p->id]() {
                const Group* me = m_model->findGroup(gid);
                if (!me) return;
                QStringList list = me->actsIn;
                list.removeAll(pid);
                m_model->setGroupActsIn(gid, list);
            })->setToolTip(tr("Tirar %1 de “Atua em”").arg(p->title));
        };
    }
    if (!acts.isEmpty()) { caption(tr("Atua em")); chipRows(acts); }
    auto* addActs = new QPushButton(acts.isEmpty() ? tr("+ Atua em… (lugares onde ele se espalha sem morar lá)") : tr("+ outro lugar"), m_detailHost);
    addActs->setObjectName(QStringLiteral("gdLink"));
    addActs->setCursor(Qt::PointingHandCursor);
    connect(addActs, &QPushButton::clicked, this, [this, gid, addActs]() { showActsInMenu(gid, addActs->mapToGlobal(QPoint(0, addActs->height()))); });
    m_detailLay->addWidget(addActs);
    QList<std::function<void(QLayout*)>> actors;
    for (const Group& o : m_model->groups()) if (o.actsIn.contains(gid)) {
        const Group* op = &o;
        actors << [this, chip, op](QLayout* l) { chip(l, op, tr("%1 atua aqui").arg(op->title), [this, id = op->id]() { select(id); }); };
    }
    if (!actors.isEmpty()) { caption(tr("Atuam aqui")); chipRows(actors); }

    // 3) vindo de cima: os documentos do pai, do avô... (o mais perto primeiro)
    const QList<const Group*> path = m_model->groupPath(gid);
    bool upCaption = false;
    for (int k = path.size() - 2; k >= 0; k--) {
        const Group* a = path[k];
        for (const Drawer& dr : m_model->drawers())
            for (const DrawerItem& it : dr.items) if (it.markerId == a->id) {
                if (!upCaption) { caption(QStringLiteral("↑ ") + tr("Vindo de cima")); upCaption = true; }
                docRow(it, dr, a->title);
            }
    }

    // 4) dentro dele
    const QList<const Group*> kids = m_model->groupChildren(gid);
    if (!kids.isEmpty()) {
        caption(tr("Dentro dele"));
        const QHash<QString, int> counts = docCounts(m_model);
        QList<std::function<void(QLayout*)>> makers;
        for (const Group* k : kids) {
            const int c = counts.value(k->id);
            makers << [this, chip, k, c](QLayout* l) {
                chip(l, k, c ? QStringLiteral("%1  ·  %2").arg(k->title, tr("%n doc(s)", nullptr, c)) : k->title, [this, id = k->id]() { select(id); });
            };
        }
        chipRows(makers);
    }
    m_detailLay->addStretch(1);
}

void GroupsPanel::showNewMenu(const QString& parentId, const QPoint& globalPos)
{
    if (!m_model) return;
    const Group* parent = m_model->findGroup(parentId);
    QMenu menu(this);
    menu.setStyleSheet(Theme::qss(QStringLiteral(R"(
        QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: @radius-panel; padding: 4px; }
        QMenu::item { padding: 7px 16px 7px 10px; border-radius: @radius-item; }
        QMenu::item:selected { background: %4; color: %5; }
    )")).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
            Theme::hoverOverlay(), Theme::textBright()));
    const QString color = parent ? parent->color : nextColor();   // filho nasce com a cor do pai
    const qreal dpr = devicePixelRatioF();
    auto* place = menu.addAction(QIcon(badgePixmap(color, false, 16, dpr)),
                                 parent ? tr("Lugar dentro de %1").arg(parent->title) : tr("Lugar (reino, região, cidade, planeta)"));
    auto* social = menu.addAction(QIcon(badgePixmap(color, true, 16, dpr)),
                                  parent ? tr("Grupo social dentro de %1").arg(parent->title) : tr("Grupo social (família, facção, ordem, elenco)"));
    QAction* chosen = menu.exec(globalPos);
    if (!chosen) return;
    const bool isSocial = chosen == social;
    const QString id = m_model->addGroup(isSocial ? tr("Novo grupo") : tr("Novo lugar"), color, parentId,
                                         isSocial ? QStringLiteral("social") : QStringLiteral("place"));
    select(id);
    QTimer::singleShot(0, this, [this, id]() { m_tree->startRename(id); });
}

void GroupsPanel::showActsInMenu(const QString& groupId, const QPoint& globalPos)
{
    const Group* g = m_model ? m_model->findGroup(groupId) : nullptr;
    if (!g) return;
    QMenu menu(this);
    menu.setStyleSheet(Theme::qss(QStringLiteral(R"(
        QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: @radius-panel; padding: 4px; }
        QMenu::item { padding: 6px 16px 6px 10px; border-radius: @radius-item; }
        QMenu::item:selected { background: %4; color: %5; }
        QMenu::item:disabled { color: %6; }
    )")).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
            Theme::hoverOverlay(), Theme::textBright(), Theme::textMuted()));
    const qreal dpr = devicePixelRatioF();
    int shown = 0;
    // só lugares; nem ele, nem quem já está na lista, nem os de dentro dele (já é dele)
    for (const auto& [o, depth] : m_model->groupTree()) {
        if (o->social() || o->id == groupId || g->actsIn.contains(o->id) || m_model->groupIsInside(o->id, groupId)) continue;
        auto* a = menu.addAction(QIcon(badgePixmap(o->color, false, 14, dpr)), QString(std::min(depth, 6) * 3, QLatin1Char(' ')) + o->title);
        a->setData(o->id);
        shown++;
    }
    if (!shown) menu.addAction(tr("Nenhum lugar pra escolher"))->setEnabled(false);
    QAction* chosen = menu.exec(globalPos);
    if (!chosen || chosen->data().toString().isEmpty()) return;
    QStringList list = g->actsIn;
    list << chosen->data().toString();
    m_model->setGroupActsIn(groupId, list);
}

void GroupsPanel::showGroupContextMenu(const QString& groupId, const QPoint& globalPos)
{
    if (!m_model) return;
    const Group* g = m_model->findGroup(groupId);
    if (!g) return;

    QMenu menu(this);
    menu.setStyleSheet(Theme::qss(QStringLiteral(R"(
        QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: @radius-panel; padding: 4px; }
        QMenu::item { padding: 6px 16px; border-radius: @radius-item; }
        QMenu::item:selected { background: %4; color: %5; }
    )")).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
            Theme::hoverOverlay(), Theme::textBright()));

    auto* renameAct = menu.addAction(tr("Renomear"));
    connect(renameAct, &QAction::triggered, this, [this, groupId]() { m_tree->startRename(groupId); });
    auto* editAct = menu.addAction(tr("Editar grupo…"));
    connect(editAct, &QAction::triggered, this, [this, groupId]() { showEditGroupDialog(groupId); });
    auto* addAct = menu.addAction(tr("Novo grupo dentro dele…"));
    connect(addAct, &QAction::triggered, this, [this, groupId, globalPos]() { showNewMenu(groupId, globalPos); });
    if (!g->parentId.isEmpty()) {
        auto* topAct = menu.addAction(tr("Tirar de %1").arg(m_model->findGroup(g->parentId) ? m_model->findGroup(g->parentId)->title : QString()));
        connect(topAct, &QAction::triggered, this, [this, groupId]() { m_model->setGroupParent(groupId, QString()); });
    }

    menu.addSeparator();
    auto* deleteAct = menu.addAction(tr("Excluir grupo"));
    deleteAct->setToolTip(tr("Os grupos de dentro sobem um nível; os documentos continuam nas gavetas"));
    connect(deleteAct, &QAction::triggered, this, [this, groupId]() {
        m_model->removeGroup(groupId);
    });

    menu.exec(globalPos);
}

void GroupsPanel::showEditGroupDialog(const QString& groupId)
{
    if (!m_model) return;
    const bool isNew = groupId.isEmpty();
    const Group* existing = isNew ? nullptr : m_model->findGroup(groupId);

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(isNew ? tr("Novo grupo") : tr("Editar grupo"));
    dlg->setModal(true);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setFixedWidth(300);
    dlg->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QDialog { background: %1; color: %2; }
        QLineEdit {
            background: %3; color: %2; border: 1px solid %4;
            border-radius: @radius-control; padding: 6px 10px;
        }
        QPushButton#okBtn {
            background: %5; color: %6; border: none; border-radius: @radius-control; padding: 6px 14px; font-weight: 600;
        }
        QPushButton#okBtn:hover { background: %7; }
        QPushButton#cancelBtn {
            background: transparent; color: %8; border: 1px solid %4;
            border-radius: @radius-control; padding: 6px 14px;
        }
        QPushButton#cancelBtn:hover { background: %9; }
    )")).arg(Theme::panelBackground(), Theme::textPrimary(),
            Theme::inputBackground(), Theme::panelBorder(),
            Theme::accentDefault(), Theme::textBright(),
            Theme::borderStrong(), Theme::textMuted(),
            Theme::hoverOverlay()));

    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(12);

    auto* nameEdit = new QLineEdit(dlg);
    nameEdit->setPlaceholderText(tr("Nome do grupo"));
    if (existing) nameEdit->setText(existing->title);
    lay->addWidget(nameEdit);

    // Cor inicial
    QString chosenColor = existing ? existing->color : kDefaultColors.first();

    auto* colorRow = new QHBoxLayout();
    colorRow->setSpacing(6);
    QList<QPushButton*> colorBtns;
    for (const QString& col : kDefaultColors) {
        auto* cb = new QPushButton(dlg);
        cb->setFixedSize(24, 24);
        cb->setCheckable(true);
        cb->setChecked(col == chosenColor);
        cb->setProperty("colorValue", col);
        cb->setStyleSheet(Theme::qss(QStringLiteral(
            // Swatch redondo: o botão é 24x24, o raio é metade do lado. Fora da
            // escala do tema de propósito — com outro raio deixa de ser círculo.
            "QPushButton { background: %1; border-radius: 12px; border: 2px solid transparent; }"
            "QPushButton:checked { border-color: %2; }"
            "QPushButton:hover   { border-color: %3; }")
            .arg(col, Theme::textBright(), Theme::textMuted())));
        colorRow->addWidget(cb);
        colorBtns.append(cb);
    }
    colorRow->addStretch();
    lay->addLayout(colorRow);

    for (auto* cb : colorBtns) {
        connect(cb, &QPushButton::clicked, dlg, [cb, &chosenColor, &colorBtns](bool) {
            chosenColor = cb->property("colorValue").toString();
            for (auto* b : colorBtns) b->setChecked(b == cb);
        });
    }

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    auto* cancelBtn = new QPushButton(tr("Cancelar"), dlg);
    cancelBtn->setObjectName(QStringLiteral("cancelBtn"));
    auto* okBtn = new QPushButton(isNew ? tr("Criar") : tr("Salvar"), dlg);
    okBtn->setObjectName(QStringLiteral("okBtn"));
    okBtn->setDefault(true);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, dlg, [dlg, nameEdit, &chosenColor, this, groupId, isNew]() {
        const QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) { nameEdit->setFocus(); return; }
        if (isNew) {
            const QString newId = m_model->addGroup(name, chosenColor);
            m_selectedGroupId = newId;
        } else {
            m_model->updateGroup(groupId, name, chosenColor);
        }
        dlg->accept();
    });

    dlg->exec();
}

void GroupsPanel::showNear(const QRect& anchorGlobal)
{
    if (!isVisible()) {
        resize(640, 470);
        // o painel mora dentro da janela: a âncora vem da tela, a posição é da janela
        QWidget* host = parentWidget();
        const QRect area = host ? host->rect() : (QApplication::primaryScreen() ? QApplication::primaryScreen()->availableGeometry() : QRect(0, 0, 1920, 1080));
        const QRect a = host ? QRect(host->mapFromGlobal(anchorGlobal.topLeft()), anchorGlobal.size()) : anchorGlobal;
        int x = a.right() + 8;
        int y = a.top();
        if (x + width() > area.right()) x = a.left() - width() - 8;
        if (y + height() > area.bottom()) y = area.bottom() - height() - 8;
        move(std::max(area.left() + 8, x), std::max(area.top() + 8, y));
    }
    onGroupsChanged();
    show();
    raise();
}

bool GroupsPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Drag pelo header
    if (watched == m_header) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragOffset = me->globalPosition().toPoint() - pos();
                return false;
            }
        }
    }

    // Clique nos documentos da lista
    auto* w = qobject_cast<QWidget*>(watched);
    if (w && event->type() == QEvent::MouseButtonPress) {
        const QString dkey = w->property("drawerKey").toString();
        const QString iid  = w->property("itemId").toString();
        if (!dkey.isEmpty() && !iid.isEmpty()) {
            emit itemActivated(dkey, iid);
            return true;
        }
    }

    return QFrame::eventFilter(watched, event);
}

void GroupsPanel::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && m_header && m_header->geometry().contains(e->pos())) {
        m_dragging = true;
        m_dragOffset = e->globalPosition().toPoint() - pos();
    }
    QFrame::mousePressEvent(e);
}

void GroupsPanel::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging)
        move(e->globalPosition().toPoint() - m_dragOffset);
    QFrame::mouseMoveEvent(e);
}

void GroupsPanel::mouseReleaseEvent(QMouseEvent* e)
{
    m_dragging = false;
    QFrame::mouseReleaseEvent(e);
}
