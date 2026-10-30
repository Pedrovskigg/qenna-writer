#include "TagField.h"

#include "ProjectTags.h"
#include "TimelineTracksTypes.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>

using Tracks::Palette;
using Tracks::alpha;
using Tracks::mix;

namespace {
constexpr int kPad = 5, kGap = 5, kChipH = 22, kInputMin = 110, kSuggestTop = 7;
}

// uma pílula: a tag do item (com ×, clicar tira) ou uma sugestão do projeto (com +, clicar põe)
class TagChip : public QAbstractButton {
public:
    QString tag;
    bool suggest = false;
    TagChip(const QString& t, bool s, QWidget* parent) : QAbstractButton(parent), tag(t), suggest(s)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
        setToolTip(s ? QCoreApplication::translate("TagField", "Pôr a tag “%1”").arg(t)
                     : QCoreApplication::translate("TagField", "Tirar a tag “%1”").arg(t));
    }
    QSize sizeHint() const override
    {
        const QFontMetrics fm(Tracks::uiFont(11.5));
        const int w = 10 + (suggest ? fm.horizontalAdvance(QStringLiteral("+ ")) : 0) + fm.horizontalAdvance(tag) + (suggest ? 10 : 22);
        return QSize(std::min(220, w), kChipH);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette pal = Palette::onPage();
        const bool hover = underMouse();
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QFont f = Tracks::uiFont(11.5);
        p.setFont(f);
        const QFontMetrics fm(f);
        if (suggest) {
            p.setBrush(hover ? alpha(pal.ink, 0.06) : QColor(Qt::transparent));
            p.setPen(QPen(hover ? alpha(pal.ink, 0.35) : pal.border, 1, Qt::DashLine));
            p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
            p.setPen(hover ? pal.ink : pal.dim);
            p.drawText(QRectF(10, 0, width() - 18, height()), Qt::AlignVCenter | Qt::AlignLeft,
                       fm.elidedText(QStringLiteral("+ ") + tag, Qt::ElideRight, width() - 18));
            return;
        }
        p.setBrush(alpha(pal.accent, hover ? 0.22 : 0.14));
        p.setPen(QPen(alpha(pal.accent, hover ? 0.85 : 0.55), 1));
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        p.setPen(pal.bright);
        p.drawText(QRectF(10, 0, width() - 30, height()), Qt::AlignVCenter | Qt::AlignLeft, fm.elidedText(tag, Qt::ElideRight, width() - 30));
        // o ×
        const QPointF c(width() - 11.5, height() / 2.0);
        p.setPen(QPen(hover ? pal.ink : pal.dim, 1.4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(c + QPointF(-3, -3), c + QPointF(3, 3));
        p.drawLine(c + QPointF(3, -3), c + QPointF(-3, 3));
    }
};

TagField::TagField(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setCursor(Qt::IBeamCursor);
    m_input = new QLineEdit(this);
    m_input->setObjectName(QStringLiteral("tagInput"));
    m_input->setFrame(false);
    m_input->setFont(Tracks::uiFont(12));
    m_input->installEventFilter(this);
    m_placeholder = tr("guerra, monarquia, linhagem…");
    connect(m_input, &QLineEdit::textEdited, this, [this](const QString& t) {
        // vírgula confirma o que veio antes dela
        if (t.contains(QLatin1Char(','))) {
            QStringList parts = t.split(QLatin1Char(','));
            const QString rest = parts.takeLast();
            for (const QString& part : parts) commit(part);
            m_input->setText(rest.trimmed());
        }
        rebuildSuggestions();
    });
    m_vocab = ProjectTags::vocabulary();
    styleInput();
    rebuildChips();
}

void TagField::setTags(const QStringList& tags)
{
    m_tags = ProjectTags::clean(tags);
    m_input->clear();
    rebuildChips();
}

QStringList TagField::tags() const
{
    QStringList all = m_tags;
    const QString pending = ProjectTags::normalize(m_input->text());
    if (!pending.isEmpty()) all.append(ProjectTags::canonical(pending, m_vocab));
    return ProjectTags::clean(all);
}

void TagField::setVocabulary(const QStringList& vocab)
{
    m_vocab = vocab;
    rebuildSuggestions();
}

void TagField::setMaxSuggestions(int n)
{
    m_maxSuggest = n;
    rebuildSuggestions();
}

void TagField::setSuggestOnFocus(bool on)
{
    m_suggestOnFocus = on;
    rebuildSuggestions();
}

void TagField::setPlaceholder(const QString& text)
{
    m_placeholder = text;
    rebuildChips();
}

void TagField::focusInput() { m_input->setFocus(); }

void TagField::commit(const QString& raw)
{
    QString t = ProjectTags::normalize(raw);
    if (t.isEmpty()) return;
    t = ProjectTags::canonical(t, m_vocab);   // "guerra" vira "Guerra" se o projeto já usa assim
    if (ProjectTags::has(m_tags, t)) return;
    m_tags.append(t);
    rebuildChips();
    emit tagsChanged(m_tags);
}

void TagField::removeTag(const QString& tag)
{
    for (int i = 0; i < m_tags.size(); i++)
        if (m_tags[i].compare(tag, Qt::CaseInsensitive) == 0) { m_tags.removeAt(i); break; }
    rebuildChips();
    emit tagsChanged(m_tags);
}

void TagField::rebuildChips()
{
    for (TagChip* c : std::as_const(m_chips)) { c->hide(); c->deleteLater(); }
    m_chips.clear();
    for (const QString& t : std::as_const(m_tags)) {
        auto* c = new TagChip(t, false, this);
        connect(c, &QAbstractButton::clicked, this, [this, t]() { removeTag(t); m_input->setFocus(); });
        c->show();
        m_chips.append(c);
    }
    m_input->setPlaceholderText(m_tags.isEmpty() ? m_placeholder : tr("mais uma tag…"));
    rebuildSuggestions();
}

void TagField::rebuildSuggestions()
{
    for (TagChip* c : std::as_const(m_suggest)) { c->hide(); c->deleteLater(); }
    m_suggest.clear();
    const QString q = ProjectTags::normalize(m_input->text());
    const bool show = !m_suggestOnFocus || m_input->hasFocus();
    for (const QString& v : std::as_const(m_vocab)) {
        if (!show) break;
        if (m_suggest.size() >= m_maxSuggest) break;
        if (ProjectTags::has(m_tags, v)) continue;
        if (!q.isEmpty() && !v.contains(q, Qt::CaseInsensitive)) continue;
        auto* c = new TagChip(v, true, this);
        connect(c, &QAbstractButton::clicked, this, [this, v]() { m_input->clear(); commit(v); m_input->setFocus(); });
        c->show();
        m_suggest.append(c);
    }
    relayout();
}

// a caixa: pílulas em fila que quebra, e a linha de digitar no fim (desce se não couber);
// as sugestões embaixo, fora da caixa
int TagField::arrange(int w, bool apply) const
{
    w = std::max(w, kInputMin + 2 * kPad);
    int x = kPad, y = kPad;
    for (TagChip* c : m_chips) {
        const int cw = std::min(c->sizeHint().width(), w - 2 * kPad);
        if (x > kPad && x + cw > w - kPad) { x = kPad; y += kChipH + kGap; }
        if (apply) c->setGeometry(x, y, cw, kChipH);
        x += cw + kGap;
    }
    int iw = w - kPad - x;
    if (iw < kInputMin) { x = kPad; y += kChipH + kGap; iw = w - 2 * kPad; }
    if (apply) m_input->setGeometry(x + 3, y, iw - 3, kChipH);
    const int boxH = y + kChipH + kPad;
    if (apply) m_boxH = boxH;
    if (m_suggest.isEmpty()) return boxH;
    x = 0; y = boxH + kSuggestTop;
    for (TagChip* c : m_suggest) {
        const int cw = std::min(c->sizeHint().width(), w);
        if (x > 0 && x + cw > w) { x = 0; y += kChipH + kGap; }
        if (apply) c->setGeometry(x, y, cw, kChipH);
        x += cw + kGap;
    }
    return y + kChipH;
}

int TagField::heightForWidth(int w) const { return arrange(w, false); }
QSize TagField::sizeHint() const { return QSize(300, heightForWidth(width() > 0 ? width() : 300)); }
QSize TagField::minimumSizeHint() const { return QSize(kInputMin + 2 * kPad, kChipH + 2 * kPad); }

void TagField::relayout()
{
    const int h = arrange(width(), true);
    updateGeometry();
    if (h != m_lastH) {
        m_lastH = h;
        setMinimumHeight(h);
        if (resized) resized();
    }
    update();
}

void TagField::resizeEvent(QResizeEvent*) { relayout(); }

void TagField::styleInput()
{
    const Palette pal = Palette::onPage();
    m_inkSeen = pal.ink.name();
    m_input->setStyleSheet(QStringLiteral("QLineEdit#tagInput { background: transparent; border: none; padding: 0; color: %1; selection-background-color: %2; }")
                               .arg(pal.ink.name(), alpha(pal.accent, 0.35).name(QColor::HexArgb)));
    QPalette qp = m_input->palette();
    qp.setColor(QPalette::PlaceholderText, pal.dim);
    m_input->setPalette(qp);
}

void TagField::paintEvent(QPaintEvent*)
{
    const Palette pal = Palette::onPage();
    if (pal.ink.name() != m_inkSeen) styleInput();   // o tema mudou
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF box = QRectF(0, 0, width(), m_boxH).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setBrush(mix(pal.app, pal.page, 0.55));
    p.setPen(QPen(m_input->hasFocus() ? alpha(pal.accent, 0.8) : pal.border, 1));
    p.drawRoundedRect(box, 8, 8);
}

void TagField::mousePressEvent(QMouseEvent*) { m_input->setFocus(); }

bool TagField::eventFilter(QObject* o, QEvent* e)
{
    if (o == m_input) {
        if (e->type() == QEvent::FocusIn) {
            const QStringList v = ProjectTags::vocabulary();   // o projeto pode ter ganhado tags desde que o campo nasceu
            if (!v.isEmpty()) m_vocab = v;
            rebuildSuggestions();
        }
        if (e->type() == QEvent::FocusOut) {
            if (m_suggestOnFocus) {
                // saiu do campo com algo digitado: vira tag; as sugestões somem
                if (!m_input->text().trimmed().isEmpty()) { commit(m_input->text()); m_input->clear(); }
                rebuildSuggestions();
            }
            update();
        }
        if (e->type() == QEvent::KeyPress) {
            auto* k = static_cast<QKeyEvent*>(e);
            const bool typed = !m_input->text().trimmed().isEmpty();
            if ((k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter || k->key() == Qt::Key_Tab) && typed) {
                const bool complete = k->key() == Qt::Key_Tab && !m_suggest.isEmpty();
                commit(complete ? m_suggest.first()->tag : m_input->text());
                m_input->clear();
                rebuildSuggestions();
                return true;   // Enter com texto põe a tag; vazio segue (a folha confirma)
            }
            if (k->key() == Qt::Key_Backspace && m_input->text().isEmpty() && !m_tags.isEmpty()) {
                removeTag(m_tags.last());
                return true;
            }
        }
    }
    return QWidget::eventFilter(o, e);
}
