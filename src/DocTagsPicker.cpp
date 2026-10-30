#include "DocTagsPicker.h"

#include "ProjectTags.h"
#include "TagField.h"
#include "TimelineTracksTypes.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QScreen>
#include <QVBoxLayout>

namespace {
using Tracks::Palette;
using Tracks::alpha;

// a etiqueta: "#" + as tags separadas por ponto (ou "Tags" apagado), no desenho da etiqueta de grupo
class Chip : public QAbstractButton {
public:
    QString label;
    bool set = false;
    explicit Chip(QWidget* parent) : QAbstractButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
        setFixedHeight(22);
    }
    QSize sizeHint() const override
    {
        const QFontMetrics fm(Tracks::uiFont(11.5));
        return QSize(std::min(190, 8 + 10 + 5 + fm.horizontalAdvance(label) + 10), 22);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette pal = Palette::onPage();
        const bool hover = underMouse() || isDown();
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const QColor text = set || hover ? pal.ink : pal.dim;
        p.setBrush(Qt::transparent);
        p.setPen(QPen(set ? alpha(pal.accent, 0.8) : hover ? alpha(pal.ink, 0.35) : pal.border, 1));
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        QFont f = Tracks::uiFont(11.5);
        p.setFont(f);
        // o "#" na cor de destaque quando tem tag
        QFont hf = Tracks::uiFont(12, QFont::DemiBold);
        p.setFont(hf);
        p.setPen(set ? pal.accent : text);
        p.drawText(QRectF(9, 0, 10, height()), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("#"));
        p.setFont(f);
        p.setPen(text);
        const QRectF tr(22, 0, width() - 22 - 9, height());
        p.drawText(tr, Qt::AlignVCenter | Qt::AlignLeft, p.fontMetrics().elidedText(label, Qt::ElideRight, int(tr.width())));
    }
};
}

DocTagsPicker::DocTagsPicker(QWidget* parent) : QObject(parent)
{
    m_tag = new Chip(parent);
    connect(m_tag, &QAbstractButton::clicked, this, &DocTagsPicker::open);
    sync();
}

void DocTagsPicker::setTags(const QStringList& tags)
{
    if (m_popup && m_popup->isVisible()) return;   // a pessoa está mexendo: o que vale é o campo
    m_tags = tags;
    sync();
}

void DocTagsPicker::sync()
{
    auto* chip = static_cast<Chip*>(m_tag);
    chip->set = !m_tags.isEmpty();
    chip->label = m_tags.isEmpty() ? QCoreApplication::translate("DocTagsPicker", "Tags") : m_tags.join(QStringLiteral(" · "));
    chip->setToolTip(m_tags.isEmpty() ? QCoreApplication::translate("DocTagsPicker", "Tags do documento: palavras livres pra filtrar depois (guerra, monarquia…)")
                                      : QCoreApplication::translate("DocTagsPicker", "Tags: %1").arg(m_tags.join(QStringLiteral(", "))));
    chip->setFixedWidth(chip->sizeHint().width());
    chip->update();
    if (changed) changed();
}

void DocTagsPicker::apply(const QStringList& tags)
{
    if (tags == m_tags) return;
    m_tags = tags;
    sync();
    if (picked) picked(m_tags);
}

void DocTagsPicker::open()
{
    if (!m_popup) {
        // janela comum por cima do editor, como a lista de grupos do DocHeader
        m_popup = new QFrame(m_tag->parentWidget(), Qt::Popup | Qt::FramelessWindowHint);
        m_popup->setObjectName(QStringLiteral("docTagsPopup"));
        auto* l = new QVBoxLayout(m_popup);
        l->setContentsMargins(12, 10, 12, 12);
        l->setSpacing(7);
        auto* cap = new QLabel(QCoreApplication::translate("DocTagsPicker", "TAGS DESTE DOCUMENTO"), m_popup);
        cap->setObjectName(QStringLiteral("docTagsCap"));
        cap->setFont(Tracks::uiFont(9.5, QFont::DemiBold));
        l->addWidget(cap);
        m_field = new TagField(m_popup);
        m_field->setFixedWidth(300);
        m_field->resized = [this]() { if (m_popup) m_popup->adjustSize(); };
        connect(m_field, &TagField::tagsChanged, this, [this](const QStringList& t) { apply(t); });
        l->addWidget(m_field);
        auto* note = new QLabel(QCoreApplication::translate("DocTagsPicker", "Enter ou vírgula põe a tag. No mapa, o Explorar filtra por ela."), m_popup);
        note->setObjectName(QStringLiteral("docTagsNote"));
        note->setFont(Tracks::uiFont(10.5));
        note->setWordWrap(true);
        note->setFixedWidth(300);
        l->addWidget(note);
        m_popup->installEventFilter(this);
    }
    const Palette pal = Palette::onPage();
    m_popup->setStyleSheet(QStringLiteral("QFrame#docTagsPopup { background: %1; border: 1px solid %2; border-radius: 8px; }"
                                          "QLabel#docTagsCap { color: %3; letter-spacing: 1px; } QLabel#docTagsNote { color: %3; }")
                               .arg(pal.page.name(), pal.border.name(), pal.dim.name()));
    m_field->setVocabulary(ProjectTags::vocabulary());
    m_field->setTags(m_tags);
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
    m_field->focusInput();
}

bool DocTagsPicker::eventFilter(QObject* o, QEvent* e)
{
    // fechou com algo digitado sem Enter: vale também
    if (o == m_popup && e->type() == QEvent::Hide && m_field) apply(m_field->tags());
    return QObject::eventFilter(o, e);
}
