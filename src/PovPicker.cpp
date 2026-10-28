#include "PovPicker.h"

#include "CoverUtils.h"
#include "ManuscriptViews.h"
#include "TimelineTracksTypes.h"

#include <QAbstractButton>
#include <QFrame>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
using Tracks::Palette;
using Tracks::alpha;

// o rosto da gaveta (foto ou inicial na cor da linha do narrador)
QPixmap face(const PovPicker::Option& o, int size, qreal dpr)
{
    const QPixmap photo = o.image.isEmpty() ? QPixmap() : CoverUtils::pixmapFromDataUrl(o.image);
    return MsPaint::avatar(photo, o.name, o.color, size, dpr);
}

// sem narrador: o lugar do rosto fica tracejado
void drawEmptyFace(QPainter& p, QPointF c, qreal r, const QColor& col)
{
    QPen pen(col, 1.3, Qt::DashLine);
    pen.setDashPattern({ 2.2, 2.0 });
    p.setBrush(Qt::NoBrush); p.setPen(pen);
    p.drawEllipse(c, r, r);
}

void drawChevron(QPainter& p, QPointF c, const QColor& col)
{
    QPainterPath v; v.moveTo(c.x() - 3.5, c.y() - 1.5); v.lineTo(c.x(), c.y() + 2); v.lineTo(c.x() + 3.5, c.y() - 1.5);
    p.setBrush(Qt::NoBrush); p.setPen(QPen(col, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(v);
}

// a etiqueta: rosto do narrador + nome + setinha; o contorno na cor da linha dele
class Tag : public QAbstractButton {
public:
    QString label;
    QColor color;
    QPixmap avatar;   // vazio = sem narrador
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
        return QSize(std::min(150, 4 + 16 + 6 + fm.horizontalAdvance(label) + 6 + 8 + 9), 22);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette pal = Palette::onPage();
        const bool set = !avatar.isNull();
        const bool hover = underMouse() || isDown();
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const QColor text = set || hover ? pal.ink : pal.dim;
        const QColor border = set ? alpha(color, 0.85) : hover ? alpha(pal.ink, 0.35) : pal.border;
        p.setBrush(Qt::NoBrush); p.setPen(QPen(border, 1));
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        if (set) p.drawPixmap(QPoint(3, 3), avatar);
        else drawEmptyFace(p, QPointF(11, height() / 2.0), 6, pal.dim);
        p.setFont(Tracks::uiFont(11.5));
        p.setPen(text);
        const QRectF tr(25, 0, width() - 25 - 18, height());
        p.drawText(tr, Qt::AlignVCenter | Qt::AlignLeft, p.fontMetrics().elidedText(label, Qt::ElideRight, int(tr.width())));
        drawChevron(p, QPointF(width() - 11, height() / 2.0), text);
    }
};

// uma linha da lista: rosto, nome e "sugerido" pra quem a gaveta sugere
class Row : public QAbstractButton {
public:
    QString label, note;
    QPixmap avatar;
    explicit Row(QWidget* parent) : QAbstractButton(parent)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
        setFixedHeight(30);
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
        if (!avatar.isNull()) p.drawPixmap(QPoint(6, 5), avatar);
        else drawEmptyFace(p, QPointF(16, height() / 2.0), 8, pal.dim);
        const QFont f = Tracks::uiFont(12.5);
        const QFont fs = Tracks::uiFont(10.5);
        const int nw = note.isEmpty() ? 0 : QFontMetrics(fs).horizontalAdvance(note);
        p.setFont(f); p.setPen(pal.bright);
        const QRectF tr(33, 0, width() - 33 - nw - 18, height());
        p.drawText(tr, Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(f).elidedText(label, Qt::ElideRight, int(tr.width())));
        if (!note.isEmpty()) {
            p.setFont(fs); p.setPen(pal.dim);
            p.drawText(QRectF(width() - nw - 10, 0, nw, height()), Qt::AlignVCenter | Qt::AlignRight, note);
        }
    }
};
// o fundo da lista dentro da folha, igual ao da lista de grupos
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
        p.setBrush(Tracks::mix(pal.app, pal.page, 0.55)); p.setPen(QPen(pal.border, 1));
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
    }
};
} // namespace

PovPicker::PovPicker(QWidget* parent, bool popup) : QObject(parent)
{
    auto* tag = new Tag(parent);
    tag->setToolTip(tr("Quem narra este capítulo (POV à vista, na gaveta de Manuscritos)"));
    m_tag = tag;
    connect(m_tag, &QAbstractButton::clicked, this, &PovPicker::toggle);
    if (popup) {
        // uma janela comum por cima do editor, como a lista de grupos e o "Quem disse?" do Granna
        m_popup = new QFrame(parent, Qt::Popup | Qt::FramelessWindowHint);
        m_popup->setObjectName(QStringLiteral("povPickPopup"));
        auto* pl = new QVBoxLayout(m_popup);
        pl->setContentsMargins(4, 4, 4, 4);
        auto* list = new ListFrame(m_popup);
        list->plain = true;
        m_list = list;
        m_list->setFixedWidth(240);
        pl->addWidget(m_list);
    } else {
        m_list = new ListFrame(parent);
        m_list->hide();
    }
    m_rows = new QVBoxLayout(m_list);
    m_rows->setContentsMargins(3, 3, 3, 3);
    m_rows->setSpacing(0);
    setCurrent(Option());
}

void PovPicker::setCurrent(const Option& current)
{
    auto* tag = static_cast<Tag*>(m_tag);
    const QString label = current.id.isEmpty() ? tr("Quem narra?") : current.name;
    // o DocHeader chama isto a cada rolagem: só redesenha se algo mudou
    const qint64 key = current.id.isEmpty() ? 0 : current.image.size() + 1;
    if (current.id == m_id && label == tag->label && current.color == tag->color
        && tag->property("imgKey").toLongLong() == key) return;
    m_id = current.id;
    tag->label = label;
    tag->color = current.color;
    tag->setProperty("imgKey", key);
    tag->avatar = current.id.isEmpty() ? QPixmap() : face(current, 16, tag->devicePixelRatioF());
    tag->setFixedWidth(tag->sizeHint().width());
    tag->update();
    if (changed) changed();
}

// a lista refeita na hora de abrir (personagens novos, sugerido de agora)
void PovPicker::rebuild()
{
    while (QLayoutItem* it = m_rows->takeAt(0)) { if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); } delete it; }
    const QList<Option> opts = options ? options() : QList<Option>();
    QWidget* host = m_list;
    QVBoxLayout* rows = m_rows;
    if (opts.size() + 1 > 9) {   // elenco grande: a lista rola
        auto* sc = new QScrollArea(m_list);
        sc->setFrameShape(QFrame::NoFrame);
        sc->setWidgetResizable(true);
        sc->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        sc->setStyleSheet(QStringLiteral("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }"));
        sc->setFixedHeight(9 * 30 + 4);
        host = new QWidget();
        rows = new QVBoxLayout(host);
        rows->setContentsMargins(0, 0, 0, 0);
        rows->setSpacing(0);
        sc->setWidget(host);
        m_rows->addWidget(sc);
    }
    const qreal dpr = m_tag->devicePixelRatioF();
    auto addRow = [&](const Option& o) {
        auto* r = new Row(host);
        r->label = o.id.isEmpty() ? tr("Sem narrador definido") : o.name;
        if (o.suggested) r->note = tr("sugerido");
        if (!o.id.isEmpty()) r->avatar = face(o, 20, dpr);
        r->setChecked(o.id == m_id);
        const QString id = o.id;
        connect(r, &QAbstractButton::clicked, this, [this, id]() {
            toggle();   // fecha
            if (picked) picked(id);
        });
        rows->addWidget(r);
    };
    addRow(Option());
    for (const Option& o : opts) addRow(o);
}

void PovPicker::toggle()
{
    if (!m_popup) {
        if (!m_list->isVisible()) rebuild();
        m_list->setVisible(!m_list->isVisible());
        if (resized) resized();
        return;
    }
    if (m_popup->isVisible()) { m_popup->hide(); return; }
    const Palette pal = Palette::onPage();
    m_popup->setStyleSheet(QStringLiteral("QFrame#povPickPopup { background: %1; border: 1px solid %2; border-radius: 8px; }"
                                          "QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }")
                               .arg(pal.page.name(), pal.border.name()));
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
}
