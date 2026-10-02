#include "GrannaPickPopup.h"

#include "Theme.h"
#include "UiScale.h"

#include <QAbstractButton>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

constexpr int kFaceBase = 44;
constexpr int kCellBase = 60;
constexpr int kColumns = 5;
constexpr int kMaxOtherRows = 2; // passou disso, os "outros" rolam

qreal sc() { return UiScale::scale(); }

// Rosto + primeiro nome. O sugerido (palpite do Granna ou quem já está
// atribuído) ganha um anel afastado; os de fora da cena ficam apagados até o
// mouse passar.
class FaceButton : public QAbstractButton
{
public:
    FaceButton(const GrannaPickPopup::Person& person, bool suggested, bool dim, QWidget* parent)
        : QAbstractButton(parent), m_person(person), m_suggested(suggested), m_dim(dim)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setToolTip(person.name);
        QFont f = font();
        f.setPixelSize(qMax(9, qRound(11.5 * sc())));
        f.setWeight(QFont::DemiBold);
        setFont(f);
        const int face = GrannaPickPopup::faceSize();
        setFixedSize(qRound(kCellBase * sc()), face + qRound(10 * sc()) + QFontMetrics(f).height());
    }

protected:
    void enterEvent(QEnterEvent* e) override { QAbstractButton::enterEvent(e); update(); }
    void leaveEvent(QEvent* e) override { QAbstractButton::leaveEvent(e); update(); }
    // Halo de foco só quando o foco veio do teclado (Tab): o foco inicial do
    // popup não pode parecer uma escolha já feita.
    void focusInEvent(QFocusEvent* e) override
    {
        QAbstractButton::focusInEvent(e);
        m_keyboardFocus = e->reason() == Qt::TabFocusReason || e->reason() == Qt::BacktabFocusReason;
        update();
    }
    void focusOutEvent(QFocusEvent* e) override { QAbstractButton::focusOutEvent(e); m_keyboardFocus = false; update(); }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const bool hot = underMouse() || (hasFocus() && m_keyboardFocus);
        if (m_dim && !hot) p.setOpacity(0.62);

        const qreal s = sc();
        const int face = GrannaPickPopup::faceSize();
        const QPointF c(width() / 2.0, s * 3 + face / 2.0);
        const qreal r = face / 2.0;
        if (hot) {
            QColor halo = m_person.voice;
            halo.setAlphaF(0.22);
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(c, r + 5 * s, r + 5 * s);
        }
        p.drawPixmap(QPointF(c.x() - r, c.y() - r), m_person.face);
        p.setBrush(Qt::NoBrush);
        if (m_suggested) {
            p.setPen(QPen(m_person.voice, 1.8 * s));
            p.drawEllipse(c, r + 3.2 * s, r + 3.2 * s);
        } else {
            p.setPen(QPen(m_person.voice, 2.0 * s));
            p.drawEllipse(c, r + 0.2 * s, r + 0.2 * s);
        }

        p.setPen(QColor(Theme::textPrimary()));
        const QRect nameRect(0, int(c.y() + r + 6 * s), width(), QFontMetrics(font()).height());
        // Primeiro nome; "O Tecelão" não vira "O".
        QString first = m_person.name.section(QLatin1Char(' '), 0, 0);
        if (first.size() <= 2) first = m_person.name;
        p.drawText(nameRect, Qt::AlignHCenter | Qt::AlignTop,
                   QFontMetrics(font()).elidedText(first, Qt::ElideRight, width()));
    }

private:
    GrannaPickPopup::Person m_person;
    bool m_suggested = false;
    bool m_dim = false;
    bool m_keyboardFocus = false;
};

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text.toUpper(), parent);
    QFont f = l->font();
    f.setPixelSize(qMax(8, qRound(9.5 * sc())));
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.1 * sc());
    l->setFont(f);
    l->setObjectName(QStringLiteral("grannaPickSection"));
    return l;
}

} // namespace

int GrannaPickPopup::faceSize()
{
    return qMax(24, qRound(kFaceBase * sc()));
}

GrannaPickPopup::GrannaPickPopup(const QString& quote, const QString& hint,
                                 const QVector<Person>& scene, const QVector<Person>& others,
                                 const QString& suggestedId, QWidget* parent)
    : QFrame(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("grannaPick"));
    const qreal s = sc();
    setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#grannaPick { background: %1; border: 1px solid %2; border-radius: @radius-panel; }"
        "QLabel { background: transparent; }"
        "QLabel#grannaPickSection { color: %3; }"
        "QLabel#grannaPickQuote { color: %4; }"
        "QLabel#grannaPickHint { color: %3; }"
        "QFrame#grannaPickLine { background: %2; border: none; }"
        "QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %2; border-radius: 4px; min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QPushButton#grannaPickFoot { background: transparent; border: none; color: %3; padding: 2px 0; }"
        "QPushButton#grannaPickFoot:hover, QPushButton#grannaPickFoot:focus { color: %5; }")
        .arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textMuted(),
             Theme::textBright(), Theme::textBright())));

    const int cell = qRound(kCellBase * s);
    const int gap = qRound(6 * s);
    const int innerW = kColumns * cell + (kColumns - 1) * gap;
    const int side = qRound(14 * s);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(side, qRound(12 * s), side, qRound(10 * s));
    lay->setSpacing(0);

    lay->addWidget(sectionLabel(tr("Quem disse?"), this));
    lay->addSpacing(qRound(6 * s));

    auto* q = new QLabel(this);
    q->setObjectName(QStringLiteral("grannaPickQuote"));
    QFont qf(QStringLiteral("Lora"));
    qf.setPixelSize(qMax(10, qRound(13.5 * s)));
    q->setFont(qf);
    q->setWordWrap(true);
    q->setFixedWidth(innerW);
    // Duas linhas no máximo: a fala inteira pode ser um parágrafo longo.
    const QFontMetrics qfm(qf);
    q->setText(qfm.elidedText(quote.simplified(), Qt::ElideRight, innerW * 2 - qfm.averageCharWidth() * 6));
    lay->addWidget(q);

    if (!hint.isEmpty()) {
        lay->addSpacing(qRound(4 * s));
        auto* h = new QLabel(hint, this);
        h->setObjectName(QStringLiteral("grannaPickHint"));
        QFont hf = h->font();
        hf.setPixelSize(qMax(9, qRound(11.5 * s)));
        h->setFont(hf);
        h->setWordWrap(true);
        h->setFixedWidth(innerW);
        lay->addWidget(h);
    }

    auto addGrid = [&](const QVector<Person>& people, bool dim, QWidget* host) {
        auto* grid = new QGridLayout(host);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(gap);
        grid->setVerticalSpacing(qRound(4 * s));
        for (int i = 0; i < people.size(); ++i) {
            auto* b = new FaceButton(people.at(i), people.at(i).id == suggestedId, dim, host);
            if (!m_firstFace) m_firstFace = b;
            const QString id = people.at(i).id;
            connect(b, &QAbstractButton::clicked, this, [this, id]() {
                emit characterChosen(id);
                close();
            });
            grid->addWidget(b, i / kColumns, i % kColumns, Qt::AlignLeft | Qt::AlignTop);
        }
        for (int c = people.size(); c < kColumns; ++c) grid->setColumnMinimumWidth(c, cell);
        return grid;
    };

    if (!scene.isEmpty()) {
        lay->addSpacing(qRound(12 * s));
        lay->addWidget(sectionLabel(tr("Nesta cena"), this));
        lay->addSpacing(qRound(6 * s));
        auto* host = new QWidget(this);
        addGrid(scene, false, host);
        lay->addWidget(host);
    }
    if (!others.isEmpty()) {
        lay->addSpacing(qRound(10 * s));
        lay->addWidget(sectionLabel(scene.isEmpty() ? tr("Personagens") : tr("Outros personagens"), this));
        lay->addSpacing(qRound(6 * s));
        auto* host = new QWidget;
        addGrid(others, !scene.isEmpty(), host);
        const int rows = (others.size() + kColumns - 1) / kColumns;
        if (rows > kMaxOtherRows) {
            auto* area = new QScrollArea(this);
            area->setWidget(host);
            area->setWidgetResizable(true);
            area->setFrameShape(QFrame::NoFrame);
            area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            host->adjustSize();
            const int rowH = host->sizeHint().height() / rows;
            area->setFixedSize(innerW + qRound(10 * s), rowH * kMaxOtherRows + rowH / 2);
            lay->addWidget(area);
        } else {
            host->setParent(this);
            lay->addWidget(host);
        }
    }

    lay->addSpacing(qRound(10 * s));
    auto* line = new QFrame(this);
    line->setObjectName(QStringLiteral("grannaPickLine"));
    line->setFixedHeight(1);
    lay->addWidget(line);
    lay->addSpacing(qRound(8 * s));

    auto* foot = new QHBoxLayout;
    foot->setContentsMargins(0, 0, 0, 0);
    foot->setSpacing(qRound(14 * s));
    auto footBtn = [&](const QString& text) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(QStringLiteral("grannaPickFoot"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFlat(true);
        QFont f = b->font();
        f.setPixelSize(qMax(9, qRound(11.5 * s)));
        b->setFont(f);
        return b;
    };
    QPushButton* extra = footBtn(tr("Figurante"));
    extra->setToolTip(tr("Quem fala não é do elenco"));
    QPushButton* notSpeech = footBtn(tr("Não é fala"));
    notSpeech->setToolTip(tr("Some da lista de diálogos e não volta"));
    QPushButton* closeBtn = footBtn(tr("Fechar"));
    foot->addWidget(extra);
    foot->addWidget(notSpeech);
    foot->addStretch(1);
    foot->addWidget(closeBtn);
    lay->addLayout(foot);

    connect(extra, &QPushButton::clicked, this, [this]() { emit extraChosen(); close(); });
    connect(notSpeech, &QPushButton::clicked, this, [this]() { emit notSpeechChosen(); close(); });
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
}

void GrannaPickPopup::popupBelow(const QRect& anchorGlobal)
{
    adjustSize();
    QPoint pos(anchorGlobal.left(), anchorGlobal.bottom() + qRound(4 * sc()));
    if (QScreen* screen = QGuiApplication::screenAt(anchorGlobal.center())) {
        const QRect avail = screen->availableGeometry();
        if (pos.x() + width() > avail.right()) pos.setX(avail.right() - width());
        if (pos.x() < avail.left()) pos.setX(avail.left());
        if (pos.y() + height() > avail.bottom()) pos.setY(anchorGlobal.top() - height() - qRound(4 * sc()));
    }
    move(pos);
    show();
    if (m_firstFace) m_firstFace->setFocus(Qt::PopupFocusReason);
}

void GrannaPickPopup::hideEvent(QHideEvent* event)
{
    QFrame::hideEvent(event);
    emit closed();
}
