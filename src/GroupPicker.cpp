#include "GroupPicker.h"

#include "GroupBadge.h"
#include "ProjectModel.h"
#include "TimelineTracksTypes.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QFrame>
#include <QGuiApplication>
#include <QScreen>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
using Tracks::Palette;
using Tracks::alpha;
using Tracks::mix;


void drawDot(QPainter& p, QPointF c, const QString& color, const QColor& ring)
{
    if (color.isEmpty()) { p.setBrush(Qt::NoBrush); p.setPen(QPen(ring, 1.4)); p.drawEllipse(c, 4.3, 4.3); return; }
    p.setBrush(QColor(color)); p.setPen(QPen(QColor(0, 0, 0, 64), 1));
    p.drawEllipse(c, 5, 5);
}

void drawChevron(QPainter& p, QPointF c, const QColor& col)
{
    QPainterPath v; v.moveTo(c.x() - 3.5, c.y() - 1.5); v.lineTo(c.x(), c.y() + 2); v.lineTo(c.x() + 3.5, c.y() - 1.5);
    p.setBrush(Qt::NoBrush); p.setPen(QPen(col, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(v);
}

// a etiqueta: bolinha (cheia na cor do grupo, vazia sem grupo) + nome + setinha
class Tag : public QAbstractButton {
public:
    QString color, label;
    bool set = false, onPhoto = false;
    explicit Tag(QWidget* parent) : QAbstractButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
        setFixedHeight(22);
    }
    QSize sizeHint() const override
    {
        const QFontMetrics fm(Tracks::uiFont(11.5));
        return QSize(std::min(150, 8 + 10 + 6 + fm.horizontalAdvance(label) + 6 + 8 + 9), 22);
    }
    QSize minimumSizeHint() const override { return QSize(60, 22); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette pal = Palette::onPage();
        const bool hover = underMouse() || isDown();
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QColor text = set || hover ? pal.ink : pal.dim, border = set ? pal.accent : hover ? alpha(pal.ink, 0.35) : pal.border;
        QColor fill = Qt::transparent;
        if (onPhoto) { fill = QColor(0, 0, 0, 115); text = QColor(255, 255, 255, 235); border = set ? pal.accent : QColor(255, 255, 255, 90); }
        p.setBrush(fill); p.setPen(QPen(border, 1));
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        drawDot(p, QPointF(13, height() / 2.0), color, onPhoto ? QColor(255, 255, 255, 200) : pal.dim);
        QFont f = Tracks::uiFont(11.5); p.setFont(f);
        p.setPen(text);
        const QRectF tr(24, 0, width() - 24 - 18, height());
        p.drawText(tr, Qt::AlignVCenter | Qt::AlignLeft, p.fontMetrics().elidedText(label, Qt::ElideRight, int(tr.width())));
        drawChevron(p, QPointF(width() - 11, height() / 2.0), text);
    }
};

// uma linha da lista: recuo da árvore, selo (Lugar/Social), nome e quantos documentos já moram no grupo
class Row : public QAbstractButton {
public:
    QString color, label, count;
    int depth = 0;
    bool social = false, none = false;
    explicit Row(QWidget* parent) : QAbstractButton(parent)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
        setFixedHeight(28);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette pal = Palette::onPage();
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        if (isChecked()) { p.setBrush(alpha(pal.accent, 0.15)); p.setPen(QPen(alpha(pal.accent, 0.6), 1)); p.drawRoundedRect(r, 6, 6); }
        else if (underMouse()) { p.setBrush(alpha(pal.ink, 0.07)); p.setPen(Qt::NoPen); p.drawRoundedRect(r, 6, 6); }
        const double x0 = 14 + depth * 14;
        if (none) drawDot(p, QPointF(x0, height() / 2.0), QString(), pal.dim);
        else GroupBadge::draw(p, QPointF(x0, height() / 2.0), 7, QColor(color), social);
        QFont f = Tracks::uiFont(12.5); p.setFont(f);
        const QFontMetrics fm(f);
        QFont fs = Tracks::uiFont(10.5);
        const int cw = QFontMetrics(fs).horizontalAdvance(count);
        p.setPen(pal.bright);
        const QRectF tr(x0 + 13, 0, width() - x0 - 13 - cw - 18, height());
        p.drawText(tr, Qt::AlignVCenter | Qt::AlignLeft, fm.elidedText(label, Qt::ElideRight, int(tr.width())));
        if (!count.isEmpty()) {
            p.setFont(fs); p.setPen(pal.dim);
            p.drawText(QRectF(width() - cw - 10, 0, cw, height()), Qt::AlignVCenter | Qt::AlignRight, count);
        }
    }
};

// o fundo da lista, igual ao da lista de papéis do pôster
class ListFrame : public QFrame {
public:
    using QFrame::QFrame;
    bool plain = false;   // dentro do popup: o fundo é o da janela
protected:
    void paintEvent(QPaintEvent*) override
    {
        if (plain) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette pal = Palette::onPage();
        p.setBrush(mix(pal.app, pal.page, 0.55)); p.setPen(QPen(pal.border, 1));
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
    }
};
} // namespace

GroupPicker::GroupPicker(const ProjectModel* model, QWidget* parent, bool popup) : QObject(parent), m_model(model)
{
    auto* tag = new Tag(parent);
    tag->setToolTip(QCoreApplication::translate("GroupPicker", "Grupo: o documento aparece no território do mesmo nome no mapa"));
    m_tag = tag;
    connect(m_tag, &QAbstractButton::clicked, this, &GroupPicker::toggle);
    if (popup) {
        // no DocHeader: uma janela comum por cima do editor, como o "Quem disse?" do Granna
        m_popup = new QFrame(parent, Qt::Popup | Qt::FramelessWindowHint);
        m_popup->setObjectName(QStringLiteral("groupPickPopup"));
        const Palette pal = Palette::onPage();
        m_popup->setStyleSheet(QStringLiteral("QFrame#groupPickPopup { background: %1; border: 1px solid %2; border-radius: 8px; }"
                                              "QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }")
                                   .arg(pal.page.name(), pal.border.name()));
        auto* pl = new QVBoxLayout(m_popup);
        pl->setContentsMargins(4, 4, 4, 4);
        m_list = new ListFrame(m_popup);
        static_cast<ListFrame*>(m_list)->plain = true;
        m_list->setFixedWidth(270);
        pl->addWidget(m_list);
    } else {
        m_list = new ListFrame(parent);
        m_list->hide();
    }
    m_rows = new QVBoxLayout(m_list);
    m_rows->setContentsMargins(3, 3, 3, 3);
    m_rows->setSpacing(0);
    rebuild();
    m_tag->setVisible(available());
    sync();
}

bool GroupPicker::available() const { return m_model && !m_model->groups().isEmpty(); }

// a lista refeita a partir do projeto (grupos novos, contagens de agora)
void GroupPicker::rebuild()
{
    while (QLayoutItem* it = m_rows->takeAt(0)) { if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); } delete it; }
    const int n = m_model ? int(m_model->groups().size()) + 1 : 1;
    QWidget* host = m_list;
    QVBoxLayout* rows = m_rows;
    if (n > 8) {   // muitos grupos: a lista rola em vez de esticar a folha
        auto* sc = new QScrollArea(m_list);
        sc->setFrameShape(QFrame::NoFrame);
        sc->setWidgetResizable(true);
        sc->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        sc->setStyleSheet(QStringLiteral("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }"));
        sc->setFixedHeight(8 * 28 + 4);
        host = new QWidget();
        rows = new QVBoxLayout(host);
        rows->setContentsMargins(0, 0, 0, 0);
        rows->setSpacing(0);
        sc->setWidget(host);
        m_rows->addWidget(sc);
    }
    // quantos documentos de gaveta já moram em cada grupo
    QHash<QString, int> counts;
    if (m_model) for (const Drawer& d : m_model->drawers()) for (const DrawerItem& it : d.items) if (!it.markerId.isEmpty()) counts[it.markerId]++;
    auto addRow = [&](const QString& id, const QString& label, const QString& color, int depth, bool social) {
        auto* r = new Row(host);
        r->label = label; r->color = color; r->depth = depth; r->social = social; r->none = id.isEmpty();
        if (m_model && !id.isEmpty() && depth > 0) r->setToolTip(m_model->groupPathText(id));
        if (counts.value(id) > 0) r->count = QCoreApplication::translate("GroupPicker", "%n documento(s)", nullptr, counts.value(id));   // vazio não mostra número
        r->setProperty("groupId", id);
        r->setChecked(id == m_id);
        connect(r, &QAbstractButton::clicked, this, [this, id]() {
            setGroupId(id);
            if (picked) picked(m_id);
            toggle();
        });
        rows->addWidget(r);
    };
    addRow(QString(), QCoreApplication::translate("GroupPicker", "Sem grupo"), QString(), 0, false);
    // em ordem de árvore, com recuo: dá pra escolher a Vila de Wely sabendo de onde ela é
    if (m_model) for (const auto& [g, depth] : m_model->groupTree()) addRow(g->id, g->title, g->color, std::min(depth, 6), g->social());
}

void GroupPicker::setGroupId(const QString& id)
{
    m_id = m_model && m_model->findGroup(id) ? id : QString();
    sync();
}

void GroupPicker::setOnPhoto(bool on)
{
    static_cast<Tag*>(m_tag)->onPhoto = on;
    m_tag->update();
}

void GroupPicker::setMaxTagWidth(int w)
{
    w = std::max(60, w);
    if (w == m_maxW) return;
    m_maxW = w;
    sync();
}

void GroupPicker::toggle()
{
    if (m_popup) {
        if (m_popup->isVisible()) { m_popup->hide(); return; }
        rebuild();
        m_popup->adjustSize();
        const QRect a(m_tag->mapToGlobal(QPoint(0, 0)), m_tag->size());
        QPoint pos(a.right() - m_popup->width(), a.bottom() + 6);   // encosta pela direita, embaixo da etiqueta
        if (QScreen* scr = QGuiApplication::screenAt(a.center())) {
            const QRect av = scr->availableGeometry();
            pos.setX(std::max(av.left(), std::min(pos.x(), av.right() - m_popup->width())));
            if (pos.y() + m_popup->height() > av.bottom()) pos.setY(a.top() - 6 - m_popup->height());
        }
        m_popup->move(pos);
        m_popup->show();
        return;
    }
    if (!m_list->isVisible()) rebuild();
    m_list->setVisible(!m_list->isVisible());
    if (resized) resized();
}

void GroupPicker::sync()
{
    auto* tag = static_cast<Tag*>(m_tag);
    const Group* g = m_model && !m_id.isEmpty() ? m_model->findGroup(m_id) : nullptr;
    tag->set = g != nullptr;
    tag->label = g ? g->title : QCoreApplication::translate("GroupPicker", "Sem grupo");
    tag->color = g ? g->color : QString();
    // o caminho inteiro quando o grupo mora dentro de outro
    tag->setToolTip(g && !g->parentId.isEmpty() ? m_model->groupPathText(g->id)
                                                 : QCoreApplication::translate("GroupPicker", "Grupo: o documento aparece no território do mesmo nome no mapa"));
    tag->setFixedWidth(std::min(m_maxW, tag->sizeHint().width()));
    tag->update();
    for (QAbstractButton* r : m_list->findChildren<QAbstractButton*>()) r->setChecked(r->property("groupId").toString() == m_id);
    if (changed) changed();
}
