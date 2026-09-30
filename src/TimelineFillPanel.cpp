#include "TimelineFillPanel.h"

#include "AIClient.h"
#include "MiraPersonality.h"
#include "Theme.h"

#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Tracks;

namespace {

QString css(const QColor& c) { return c.name(QColor::HexArgb); }

QIcon strokeIcon(const QString& kind, const QColor& c, int size = 14)
{
    const qreal dpr = 2.0;
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 16.0, size / 16.0);
    p.setPen(QPen(c, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath path;
    if (kind == QLatin1String("close")) {
        path.moveTo(4, 4); path.lineTo(12, 12); path.moveTo(12, 4); path.lineTo(4, 12);
    } else if (kind == QLatin1String("down")) {
        path.moveTo(8, 3); path.lineTo(8, 12);
        path.moveTo(4.5, 8.5); path.lineTo(8, 12); path.lineTo(11.5, 8.5);
    } else if (kind == QLatin1String("spark")) {
        path.moveTo(8, 2); path.lineTo(8, 5); path.moveTo(8, 11); path.lineTo(8, 14);
        path.moveTo(2, 8); path.lineTo(5, 8); path.moveTo(11, 8); path.lineTo(14, 8);
        path.moveTo(4, 4); path.lineTo(5.8, 5.8); path.moveTo(10.2, 10.2); path.lineTo(12, 12);
        path.moveTo(12, 4); path.lineTo(10.2, 5.8); path.moveTo(5.8, 10.2); path.lineTo(4, 12);
    }
    p.drawPath(path);
    return QIcon(pm);
}

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text.toUpper(), parent);
    l->setObjectName(QStringLiteral("tlFillSection"));
    QFont f = uiFont(9.5, QFont::DemiBold);
    f.setLetterSpacing(QFont::PercentageSpacing, 113);
    l->setFont(f);
    return l;
}

void repolish(QWidget* w, const char* prop, const QVariant& v)
{
    if (w->property(prop) == v) return;
    w->setProperty(prop, v);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

// "5·2" → "5"
QString chapterRuler(const QString& ruler) { return ruler.section(QChar(0x00B7), 0, 0); }
} // namespace

// ═════════════════════════════════════════════════════════════════════════════
namespace FillDetail {

// Bolinhas da fila: cheia = feito, anel = o da vez. Fila longa vira barra.
class Progress : public QWidget {
public:
    explicit Progress(TimelineFillPanel* p) : QWidget(p), m(p) { setFixedHeight(16); }
    void refresh()
    {
        const int n = m->m_queue.size();
        setFixedWidth(n <= kMaxDots ? qMax(0, n * 12 - 4) : 96);
        update();
    }
protected:
    static constexpr int kMaxDots = 9;  // mais que isso vira barra (o filtro precisa do espaço)
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette pal = Palette::onPanels();
        const int n = m->m_queue.size();
        if (n == 0) return;
        auto done = [&](int i) {
            const TimelineFillPanel::Chap c = m->chapterById(m->m_queue[i]);
            return c.c0 >= 0 && !m->chapterMissing(c, TimelineFillPanel::Filter(m->m_filter));
        };
        const qreal cy = height() / 2.0;
        if (n <= kMaxDots) {
            for (int i = 0; i < n; ++i) {
                const QPointF c(4 + i * 12, cy);
                if (i == m->m_idx) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(alpha(pal.accent, 0.30));
                    p.drawEllipse(c, 6, 6);
                    p.setPen(QPen(pal.accent, 2));
                    p.setBrush(pal.panel);
                    p.drawEllipse(c, 3.5, 3.5);
                } else if (done(i)) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(pal.success);
                    p.drawEllipse(c, 4, 4);
                } else {
                    p.setPen(QPen(pal.dim, 1.5));
                    p.setBrush(Qt::NoBrush);
                    p.drawEllipse(c, 3.25, 3.25);
                }
            }
            return;
        }
        int d = 0;
        for (int i = 0; i < n; ++i) if (done(i)) ++d;
        const QRectF bar(0, cy - 2, width(), 4);
        p.setPen(Qt::NoPen);
        p.setBrush(alpha(pal.ink, 0.12));
        p.drawRoundedRect(bar, 2, 2);
        p.setBrush(pal.success);
        p.drawRoundedRect(QRectF(bar.left(), bar.top(), bar.width() * d / n, bar.height()), 2, 2);
        if (m->m_idx < n) {
            const qreal x = bar.left() + bar.width() * (m->m_idx + 0.5) / n;
            p.setBrush(pal.accent);
            p.drawEllipse(QPointF(x, cy), 4, 4);
        }
    }
private:
    TimelineFillPanel* m;
};

} // namespace FillDetail

using namespace FillDetail;

// ═════════════════════════════════════════════════════════════════════════════

TimelineFillPanel::TimelineFillPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("tlFill"));
    setAttribute(Qt::WA_StyledBackground);
    setFixedWidth(kWidth);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── cabeçalho ──
    auto* head = new QWidget(this);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(16, 12, 10, 8);
    hl->setSpacing(10);
    auto* title = sectionLabel(tr("Preencher"), head);
    hl->addWidget(title);
    hl->addStretch(1);
    m_closeBtn = new QToolButton(head);
    m_closeBtn->setObjectName(QStringLiteral("tlFillIcon"));
    m_closeBtn->setFixedSize(28, 28);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip(tr("Fechar (Esc)"));
    connect(m_closeBtn, &QToolButton::clicked, this, [this]() { commitPending(); emit closeRequested(); });
    hl->addWidget(m_closeBtn);
    root->addWidget(head);

    // ── fila: filtro + progresso ──
    auto* queue = new QWidget(this);
    queue->setObjectName(QStringLiteral("tlFillQueue"));
    queue->setAttribute(Qt::WA_StyledBackground);
    auto* ql = new QHBoxLayout(queue);
    ql->setContentsMargins(16, 0, 16, 12);
    ql->setSpacing(10);
    auto* seg = new QFrame(queue);
    seg->setObjectName(QStringLiteral("tlFillSeg"));
    auto* sl = new QHBoxLayout(seg);
    sl->setContentsMargins(2, 2, 2, 2);
    sl->setSpacing(0);
    const QString segNames[3] = { tr("Sem data"), tr("Sem resumo"), tr("Todos") };
    for (int i = 0; i < 3; ++i) {
        auto* b = new QToolButton(seg);
        b->setObjectName(QStringLiteral("tlFillSegBtn"));
        b->setText(segNames[i]);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(22);
        sl->addWidget(b);
        m_segBtn[i] = b;
        connect(b, &QToolButton::clicked, this, [this, i]() { setFilter(Filter(i)); });
    }
    seg->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); // o filtro nunca encolhe
    ql->addWidget(seg);
    ql->addStretch(1);
    m_progress = new Progress(this);
    ql->addWidget(m_progress, 0, Qt::AlignVCenter);
    root->addWidget(queue);

    // ── páginas: capítulo da vez / pronto ──
    m_pages = new QStackedWidget(this);
    m_scroll = new QScrollArea(m_pages);
    m_scroll->setObjectName(QStringLiteral("tlFillScroll"));
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pages->addWidget(m_scroll);

    m_donePage = new QWidget(m_pages);
    m_donePage->setObjectName(QStringLiteral("tlFillDone"));
    auto* dl = new QVBoxLayout(m_donePage);
    dl->setContentsMargins(20, 28, 20, 20);
    dl->setSpacing(10);
    auto* doneTitle = new QLabel(tr("Pronto."), m_donePage);
    doneTitle->setObjectName(QStringLiteral("tlFillDoneTitle"));
    doneTitle->setFont(serifFont(19));
    m_doneText = new QLabel(m_donePage);
    m_doneText->setObjectName(QStringLiteral("tlFillDim"));
    m_doneText->setWordWrap(true);
    m_doneText->setFont(uiFont(12.5));
    auto* restart = new QPushButton(tr("Voltar ao começo da fila"), m_donePage);
    restart->setObjectName(QStringLiteral("tlFillPrimary"));
    restart->setCursor(Qt::PointingHandCursor);
    restart->setFixedHeight(28);
    connect(restart, &QPushButton::clicked, this, [this]() { buildQueue(QString()); showCurrent(); });
    dl->addWidget(doneTitle);
    dl->addWidget(m_doneText);
    dl->addSpacing(4);
    dl->addWidget(restart, 0, Qt::AlignLeft);
    dl->addStretch(1);
    m_pages->addWidget(m_donePage);
    root->addWidget(m_pages, 1);

    // ── rodapé ──
    m_foot = new QWidget(this);
    m_foot->setObjectName(QStringLiteral("tlFillFoot"));
    m_foot->setAttribute(Qt::WA_StyledBackground);
    auto* fl = new QHBoxLayout(m_foot);
    fl->setContentsMargins(16, 12, 16, 12);
    fl->setSpacing(8);
    m_prevBtn = new QPushButton(tr("← Anterior"), m_foot);
    m_prevBtn->setObjectName(QStringLiteral("tlFillBtn"));
    m_prevBtn->setCursor(Qt::PointingHandCursor);
    m_prevBtn->setFixedHeight(28);
    connect(m_prevBtn, &QPushButton::clicked, this, &TimelineFillPanel::goPrev);
    m_countLbl = new QLabel(m_foot);
    m_countLbl->setObjectName(QStringLiteral("tlFillDim"));
    m_countLbl->setFont(monoFont(11));
    m_nextBtn = new QPushButton(m_foot);
    m_nextBtn->setObjectName(QStringLiteral("tlFillPrimary"));
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    m_nextBtn->setFixedHeight(28);
    m_nextBtn->setToolTip(tr("Grava e passa pro próximo capítulo (Ctrl+Enter)"));
    connect(m_nextBtn, &QPushButton::clicked, this, &TimelineFillPanel::goNext);
    fl->addWidget(m_prevBtn);
    fl->addStretch(1);
    fl->addWidget(m_countLbl);
    fl->addWidget(m_nextBtn);
    root->addWidget(m_foot);

    for (auto key : { QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter) }) {
        auto* sc = new QShortcut(key, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, &TimelineFillPanel::goNext);
    }

    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, [this]() {
        applyTheme();
        m_builtSig.clear();
        if (isVisible()) showCurrent();
    });
}

// ── capítulos, a partir das colunas ─────────────────────────────────────────

QList<TimelineFillPanel::Chap> TimelineFillPanel::chapters() const
{
    QList<Chap> out;
    const auto& cols = m_data.cols;
    for (int i = 0; i < cols.size();) {
        Chap c;
        c.id = cols[i].chapterId;
        c.c0 = i;
        c.hasScenes = cols[i].sceneIndex >= 0;
        int j = i;
        while (j < cols.size() && cols[j].chapterId == c.id) ++j;
        c.span = j - i;
        out << c;
        i = j;
    }
    return out;
}

TimelineFillPanel::Chap TimelineFillPanel::chapterById(const QString& id) const
{
    for (const Chap& c : chapters()) if (c.id == id) return c;
    return {};
}

bool TimelineFillPanel::chapterMissing(const Chap& c, Filter f) const
{
    bool noDate = false, noSum = false;
    for (int k = c.c0; k < c.c0 + c.span; ++k) {
        if (hollow(k)) noDate = true;
        if (effSummary(k).isEmpty()) noSum = true;
    }
    return f == NoDate ? noDate : f == NoSummary ? noSum : (noDate || noSum);
}

bool TimelineFillPanel::isSplit(const Chap& c) const
{
    if (!c.hasScenes) return false;
    return m_splitChapters.contains(c.id) || !canJoin(c);
}

bool TimelineFillPanel::canJoin(const Chap& c) const
{
    for (int k = c.c0; k < c.c0 + c.span; ++k) {
        const Column& col = m_data.cols[k];
        if (!col.scMarker.trimmed().isEmpty() || !col.scSummary.trimmed().isEmpty()) return false;
    }
    return true;
}

QString TimelineFillPanel::effMarker(int col) const
{
    for (const Event& e : m_data.events)
        if (!e.manual && e.col == col) return e.hollow ? QString() : e.marker.trimmed();
    return {};
}
QString TimelineFillPanel::effSummary(int col) const
{
    for (const Event& e : m_data.events)
        if (!e.manual && e.col == col) return e.summary.trimmed();
    return {};
}
bool TimelineFillPanel::hollow(int col) const
{
    for (const Event& e : m_data.events)
        if (!e.manual && e.col == col) return e.hollow;
    return false;
}

// ── fila ─────────────────────────────────────────────────────────────────────

void TimelineFillPanel::buildQueue(const QString& focusChapter)
{
    auto fill = [&]() {
        m_queue.clear();
        for (const Chap& c : chapters())
            if (m_filter == All || chapterMissing(c, m_filter)) m_queue << c.id;
    };
    fill();
    if (!focusChapter.isEmpty() && !m_queue.contains(focusChapter) && m_filter != All) {
        m_filter = All;
        fill();
    }
    m_idx = qMax(0, int(m_queue.indexOf(focusChapter)));
    m_builtSig.clear();
}

void TimelineFillPanel::start(const QString& chapterId)
{
    // começa pelo que mais falta: sem data → sem resumo → todos
    const QList<Chap> all = chapters();
    auto any = [&](Filter f) { for (const Chap& c : all) if (chapterMissing(c, f)) return true; return false; };
    m_filter = any(NoDate) ? NoDate : any(NoSummary) ? NoSummary : All;
    buildQueue(chapterId);
    showCurrent();
}

void TimelineFillPanel::jumpTo(const QString& chapterId)
{
    commitPending();
    const int i = m_queue.indexOf(chapterId);
    if (i >= 0) m_idx = i;
    else { m_filter = All; buildQueue(chapterId); }
    m_builtSig.clear();
    showCurrent();
}

void TimelineFillPanel::setFilter(Filter f)
{
    commitPending();
    m_filter = f;
    buildQueue(QString());
    showCurrent();
}

void TimelineFillPanel::goNext()
{
    commitPending();
    if (m_idx < m_queue.size()) ++m_idx;
    m_builtSig.clear();
    showCurrent();
}

void TimelineFillPanel::goPrev()
{
    commitPending();
    m_idx = qMax(0, m_idx - 1);
    m_builtSig.clear();
    showCurrent();
}

void TimelineFillPanel::setData(const Tracks::Data& d)
{
    m_data = d;
    // capítulo apagado some da fila
    for (int i = m_queue.size() - 1; i >= 0; --i)
        if (chapterById(m_queue[i]).c0 < 0) { m_queue.removeAt(i); if (m_idx > i) --m_idx; }
    if (signature() != m_builtSig) {
        // recria fora da pilha atual: pode estar dentro do editingFinished de um campo
        QPointer<TimelineFillPanel> self(this);
        QTimer::singleShot(0, this, [self]() { if (self) self->showCurrent(); });
        return;
    }
    refreshDerived();
}

QString TimelineFillPanel::signature() const
{
    if (m_idx >= m_queue.size()) return QStringLiteral("done:%1").arg(m_queue.size());
    const Chap c = chapterById(m_queue[m_idx]);
    return QStringLiteral("%1|%2|%3|%4").arg(c.id).arg(c.span).arg(isSplit(c)).arg(m_idx);
}

// ── corpo do capítulo da vez ─────────────────────────────────────────────────

void TimelineFillPanel::showCurrent()
{
    const Palette pal = Palette::onPanels();
    m_progress->refresh();
    for (int i = 0; i < 3; ++i) m_segBtn[i]->setChecked(i == int(m_filter));

    if (m_idx >= m_queue.size()) {
        m_fields.clear();
        m_pages->setCurrentWidget(m_donePage);
        m_foot->hide();
        // o que ainda falta, entre os que estavam na fila
        int left = 0;
        for (const QString& id : std::as_const(m_queue))
            if (chapterMissing(chapterById(id), m_filter == All ? All : m_filter)) ++left;
        QString msg;
        if (m_queue.isEmpty())
            msg = m_filter == NoDate ? tr("Todo capítulo já tem data. Nada a preencher aqui.")
                : m_filter == NoSummary ? tr("Todo capítulo já tem resumo. Nada a preencher aqui.")
                                        : tr("Este manuscrito ainda não tem capítulos.");
        else if (left == 0)
            msg = m_filter == NoDate ? tr("Todo capítulo tem data agora. As bolinhas vazadas sumiram da Timeline.")
                : m_filter == NoSummary ? tr("Todo capítulo tem resumo agora.")
                                        : tr("Todo capítulo tem data e resumo agora.");
        else
            msg = (left == 1 ? tr("Fim da fila. 1 capítulo ficou pra depois.")
                             : tr("Fim da fila. %1 capítulos ficaram pra depois.").arg(left))
                + QLatin1Char(' ') + tr("Dá pra voltar a eles quando quiser.");
        m_doneText->setText(msg);
        m_builtSig = signature();
        emit currentChanged(-1, 1);
        return;
    }
    const Chap ch = chapterById(m_queue[m_idx]);
    if (ch.c0 < 0) { m_queue.removeAt(m_idx); showCurrent(); return; }
    m_pages->setCurrentWidget(m_scroll);
    m_foot->show();

    if (m_body) { m_scroll->takeWidget(); m_body->hide(); m_body->deleteLater(); m_body = nullptr; }
    m_fields.clear();
    m_excerpt = nullptr; m_sugBox = nullptr; m_aiBtn = nullptr; m_ditto = nullptr; m_plus = nullptr;

    m_body = new QWidget;
    m_body->setObjectName(QStringLiteral("tlFillBody"));
    auto* lay = new QVBoxLayout(m_body);
    lay->setContentsMargins(16, 16, 16, 20);
    lay->setSpacing(14);
    const Column& c0 = m_data.cols[ch.c0];
    const bool split = isSplit(ch);
    // grupo com espaçamento próprio (rótulo colado no campo, por exemplo)
    auto group = [&](int spacing) {
        auto* w = new QWidget(m_body);
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(spacing);
        lay->addWidget(w);
        return v;
    };

    // linha + capítulo
    {
        auto* g = group(6);
        auto* chip = new QLabel(m_body);
        chip->setObjectName(QStringLiteral("tlFillChip"));
        chip->setTextFormat(Qt::RichText);
        chip->setFont(uiFont(11));
        QString laneName; QColor lc = pal.accent;
        for (const Event& e : m_data.events)
            if (!e.manual && e.col == ch.c0)
                if (const Lane* L = m_data.lane(e.laneId)) { laneName = L->name; lc = L->color; }
        const QString where = tr("Cap %1").arg(chapterRuler(c0.ruler));
        chip->setText(QStringLiteral("<span style='color:%1;font-size:10px'>&#9679;</span>&nbsp;"
                                     "<span style='color:%2'>%3</span>")
                          .arg(lc.name(), pal.dim.name(),
                               (laneName.isEmpty() ? where : laneName + QStringLiteral(" · ") + where).toHtmlEscaped()));
        g->addWidget(chip);
        auto* t = new QLabel(c0.chTitle, m_body);
        t->setObjectName(QStringLiteral("tlFillTitle"));
        t->setWordWrap(true);
        t->setFont(serifFont(21));
        g->addWidget(t);
    }

    // cenas: juntas (padrão) ou separadas
    if (ch.hasScenes) {
        auto* row = new QWidget(m_body);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(8);
        auto* lbl = new QLabel(split ? tr("%1 cenas, cada uma no seu momento").arg(ch.span)
                                     : tr("%1 cenas, todas no mesmo momento").arg(ch.span), row);
        lbl->setObjectName(QStringLiteral("tlFillDim"));
        lbl->setFont(uiFont(11.5));
        h->addWidget(lbl, 1);
        const bool join = split && canJoin(ch);
        if (!split || join) {
            auto* b = new QToolButton(row);
            b->setObjectName(QStringLiteral("tlFillSplit"));
            b->setText(split ? tr("juntar cenas") : tr("separar cenas"));
            b->setToolTip(split ? tr("Volta a um campo só pro capítulo inteiro")
                                : tr("Pra quando as cenas acontecem em momentos diferentes: cada uma ganha o próprio tempo e resumo"));
            b->setCursor(Qt::PointingHandCursor);
            b->setFont(uiFont(11));
            b->setFixedHeight(22);
            const QString id = ch.id;
            connect(b, &QToolButton::clicked, this, [this, id, split]() {
                commitPending();
                if (split) m_splitChapters.remove(id); else m_splitChapters.insert(id);
                m_builtSig.clear();
                showCurrent();
            });
            h->addWidget(b);
        } else {
            lbl->setToolTip(tr("Há cenas com tempo ou resumo próprio; por isso ficam separadas"));
        }
        lay->addWidget(row);
    }

    // como o capítulo começa
    m_openingText = m_resolver ? m_resolver(QStringLiteral("ch:") + ch.id) : QString();
    if (!FillAssist::opening(m_openingText).isEmpty()) {
        auto* g = group(7);
        g->addWidget(sectionLabel(tr("Como o capítulo começa"), m_body));
        m_excerpt = new QLabel(m_body);
        m_excerpt->setObjectName(QStringLiteral("tlFillExcerpt"));
        m_excerpt->setTextFormat(Qt::RichText);
        m_excerpt->setWordWrap(true);
        m_excerpt->setFont(serifFont(13.5, QFont::Normal, true));
        g->addWidget(m_excerpt);
    }

    auto makeField = [&](int col, bool chapterLevel, int txaH) -> Fields {
        Fields f;
        f.col = col;
        f.chapterLevel = chapterLevel;
        const Column& cc = m_data.cols[col];
        f.fld = new QLineEdit(m_body);
        f.fld->setObjectName(QStringLiteral("tlFillFld"));
        f.fld->setFont(monoFont(12));
        f.fld->setFixedHeight(30);
        f.lei = new QLabel(m_body);
        f.lei->setObjectName(QStringLiteral("tlFillLei"));
        f.lei->setFont(monoFont(10.5));
        f.txa = new QTextEdit(m_body);
        f.txa->setObjectName(QStringLiteral("tlFillTxa"));
        f.txa->setAcceptRichText(false);
        f.txa->setTabChangesFocus(true);
        f.txa->setFont(serifFont(13.5, QFont::Normal));
        f.txa->document()->setDocumentMargin(4);
        f.txa->setFixedHeight(txaH);
        f.boundMarker = chapterLevel ? cc.chMarker : cc.scMarker;
        f.boundSummary = chapterLevel ? cc.chSummary : cc.scSummary;
        f.fld->setText(f.boundMarker);
        f.txa->setPlainText(f.boundSummary);
        if (chapterLevel) {
            f.fld->setPlaceholderText(tr("ex.: Dia 5, 20 anos antes, Verão de 1999"));
            f.txa->setPlaceholderText(tr("Uma ou duas frases. Aparecem na Timeline no nível Resumos."));
        } else {
            f.fld->setPlaceholderText(cc.chMarker.isEmpty() ? tr("herda do capítulo")
                                                            : tr("herda: %1").arg(cc.chMarker));
            f.txa->setPlaceholderText(cc.chSummary.isEmpty() ? tr("herda do capítulo") : cc.chSummary);
        }
        QPalette p = f.fld->palette();
        p.setColor(QPalette::PlaceholderText, mix(pal.ink, pal.page, 0.42));
        f.fld->setPalette(p);
        f.txa->setPalette(p);
        f.fld->installEventFilter(this);
        f.txa->installEventFilter(this);
        return f;
    };

    if (!split) {
        // sugestão tirada da primeira frase
        m_sugBox = new QFrame(m_body);
        m_sugBox->setObjectName(QStringLiteral("tlFillSug"));
        auto* sh = new QHBoxLayout(m_sugBox);
        sh->setContentsMargins(11, 9, 9, 9);
        sh->setSpacing(10);
        auto* sv = new QVBoxLayout;
        sv->setSpacing(2);
        m_sugMarker = new QLabel(m_sugBox);
        m_sugMarker->setObjectName(QStringLiteral("tlFillSugMarker"));
        m_sugMarker->setFont(monoFont(13, QFont::Medium));
        m_sugWhy = new QLabel(m_sugBox);
        m_sugWhy->setObjectName(QStringLiteral("tlFillDim"));
        m_sugWhy->setFont(uiFont(11));
        m_sugWhy->setWordWrap(true);
        sv->addWidget(m_sugMarker);
        sv->addWidget(m_sugWhy);
        sh->addLayout(sv, 1);
        auto* use = new QPushButton(tr("Usar"), m_sugBox);
        use->setObjectName(QStringLiteral("tlFillBtn"));
        use->setCursor(Qt::PointingHandCursor);
        use->setFixedHeight(28);
        sh->addWidget(use, 0, Qt::AlignVCenter);
        lay->addWidget(m_sugBox);
        connect(use, &QPushButton::clicked, this, [this]() {
            if (m_fields.isEmpty() || !m_sug.valid()) return;
            Fields& f = m_fields[0];
            f.fld->setText(m_sug.marker);
            commit(f);
            if (f.txa->toPlainText().trimmed().isEmpty()) f.txa->setFocus();
        });

        Fields f = makeField(ch.c0, true, 120);
        auto* gw = group(7);
        gw->addWidget(sectionLabel(tr("Quando se passa"), m_body));
        gw->addWidget(f.fld);
        auto* under = new QHBoxLayout;
        under->setSpacing(6);
        under->addWidget(f.lei, 1);
        if (ch.c0 > 0) {
            m_ditto = new QPushButton(tr("igual"), m_body);
            m_ditto->setObjectName(QStringLiteral("tlFillMini"));
            m_ditto->setIcon(strokeIcon(QStringLiteral("down"), pal.dim, 12));
            m_plus = new QPushButton(tr("+1 dia"), m_body);
            m_plus->setObjectName(QStringLiteral("tlFillMini"));
            for (QPushButton* b : { m_ditto, m_plus }) {
                b->setCursor(Qt::PointingHandCursor);
                b->setFocusPolicy(Qt::NoFocus);
                b->setFixedHeight(22);
                under->addWidget(b);
            }
            connect(m_ditto, &QPushButton::clicked, this, [this]() {
                if (m_fields.isEmpty()) return;
                const QString prev = effMarker(m_fields[0].col - 1);
                if (prev.isEmpty()) return;
                m_fields[0].fld->setText(prev);
                commit(m_fields[0]);
            });
            connect(m_plus, &QPushButton::clicked, this, [this]() {
                if (m_fields.isEmpty()) return;
                const QString next = FillAssist::shifted(effMarker(m_fields[0].col - 1), 1, FillAssist::Unit::Day);
                if (next.isEmpty()) return;
                m_fields[0].fld->setText(next);
                commit(m_fields[0]);
            });
        }
        gw->addLayout(under);
        auto* gs = group(7);
        auto* sumHead = new QHBoxLayout;
        sumHead->addWidget(sectionLabel(tr("Resumo"), m_body));
        sumHead->addStretch(1);
        m_aiBtn = new QPushButton(m_body);
        m_aiBtn->setObjectName(QStringLiteral("tlFillAi"));
        m_aiBtn->setCursor(Qt::PointingHandCursor);
        m_aiBtn->setFocusPolicy(Qt::NoFocus);
        m_aiBtn->setIcon(strokeIcon(QStringLiteral("spark"), pal.info, 12));
        m_aiBtn->setText(tr("%1 resume").arg(miraAssistantName()));
        m_aiBtn->setToolTip(tr("Pede um resumo do capítulo à assistente. O texto cai no campo e você pode mudar à vontade."));
        connect(m_aiBtn, &QPushButton::clicked, this, &TimelineFillPanel::askAiSummary);
        sumHead->addWidget(m_aiBtn);
        gs->addLayout(sumHead);
        gs->addWidget(f.txa);
        m_fields << f;
    } else {
        // uma cena por bloco; vazias herdam do capítulo
        for (int k = ch.c0; k < ch.c0 + ch.span; ++k) {
            const Column& cc = m_data.cols[k];
            QString sceneTitle;
            for (const Event& e : m_data.events) if (!e.manual && e.col == k) sceneTitle = e.title;
            auto* hdr = new QLabel(m_body);
            hdr->setObjectName(QStringLiteral("tlFillSceneHead"));
            hdr->setTextFormat(Qt::RichText);
            hdr->setText(QStringLiteral("<span style=\"font-family:'%1';font-size:11px;color:%2\">%3</span>"
                                        "&nbsp;&nbsp;<span style=\"font-family:'%4';font-size:14px;color:%5\">%6</span>")
                             .arg(monoFont(11).family(), pal.dim.name(), cc.ruler.toHtmlEscaped(),
                                  serifFont(14).family(), pal.bright.name(), sceneTitle.toHtmlEscaped()));
            auto* g = group(6);
            g->addWidget(hdr);
            Fields f = makeField(k, false, 72);
            g->addWidget(f.fld);
            g->addWidget(f.lei);
            g->addWidget(f.txa);
            m_fields << f;
        }
    }
    lay->addStretch(1);

    for (int i = 0; i < m_fields.size(); ++i) {
        connect(m_fields[i].fld, &QLineEdit::textChanged, this, [this, i]() {
            if (i < m_fields.size()) { refreshReading(m_fields[i]); refreshDerived(); }
        });
        connect(m_fields[i].fld, &QLineEdit::editingFinished, this, [this, i]() {
            if (i < m_fields.size()) commit(m_fields[i]);
        });
        connect(m_fields[i].txa, &QTextEdit::textChanged, this, [this, i]() {
            if (i < m_fields.size() && m_aiBtn && i == 0)
                m_aiBtn->setVisible(m_fields[0].txa->toPlainText().trimmed().isEmpty()
                                    && !QSettings().value(QStringLiteral("ai/apiKey")).toString().isEmpty());
        });
    }

    m_scroll->setWidget(m_body);
    m_builtSig = signature();
    emit currentChanged(ch.c0, ch.span);
    refreshDerived();

    // cursor no primeiro buraco: marcador se falta data, senão resumo
    QPointer<TimelineFillPanel> self(this);
    QTimer::singleShot(0, this, [self]() {
        if (!self || self->m_fields.isEmpty() || !self->isVisible()) return;
        for (Fields& f : self->m_fields)
            if (f.fld->text().trimmed().isEmpty() && (f.chapterLevel || self->hollow(f.col))) { f.fld->setFocus(); return; }
        for (Fields& f : self->m_fields)
            if (f.txa->toPlainText().trimmed().isEmpty()) { f.txa->setFocus(); return; }
    });
}

void TimelineFillPanel::refreshReading(Fields& f)
{
    const QString t = f.fld->text();
    const Column& cc = m_data.cols[f.col];
    const FillAssist::Reading r = FillAssist::reading(t, m_data.startOk, m_data.startChrono);
    QString txt = r.text;
    QString kind = r.kind == FillAssist::Reading::Ok ? QStringLiteral("ok")
                 : r.kind == FillAssist::Reading::Bad ? QStringLiteral("bad") : QString();
    if (t.trimmed().isEmpty() && !f.chapterLevel && !cc.chMarker.isEmpty()) {
        txt = tr("herda do capítulo");
        kind.clear();
    }
    const QFontMetrics fm(f.lei->font());
    const int w = qMax(40, f.lei->width());
    const QString shown = fm.horizontalAdvance(txt) <= w || r.compact.isEmpty() || kind.isEmpty() ? txt : r.compact;
    f.lei->setText(fm.elidedText(shown, Qt::ElideRight, w));
    f.lei->setToolTip(txt);
    repolish(f.lei, "kind", kind);
    const Chap ch = chapterById(cc.chapterId);
    bool anyHollow = false;
    for (int k = ch.c0; k < ch.c0 + ch.span; ++k) if (hollow(k)) anyHollow = true;
    const bool miss = t.trimmed().isEmpty() && (f.chapterLevel ? anyHollow : (hollow(f.col) && cc.chMarker.isEmpty()));
    repolish(f.fld, "miss", miss);
}

void TimelineFillPanel::refreshDerived()
{
    if (m_fields.isEmpty() || m_idx >= m_queue.size()) { m_progress->refresh(); return; }
    const Palette pal = Palette::onPanels();
    for (Fields& f : m_fields) {
        // o que foi gravado por fora (inspetor, editor) aparece; o campo em edição não é tocado
        const Column& cc = m_data.cols[f.col];
        const QString rm = f.chapterLevel ? cc.chMarker : cc.scMarker;
        const QString rs = f.chapterLevel ? cc.chSummary : cc.scSummary;
        if (!f.fld->hasFocus() && rm != f.boundMarker) { QSignalBlocker b(f.fld); f.fld->setText(rm); f.boundMarker = rm; }
        if (!f.txa->hasFocus() && rs != f.boundSummary) { QSignalBlocker b(f.txa); f.txa->setPlainText(rs); f.boundSummary = rs; }
        refreshReading(f);
    }

    const Fields& f0 = m_fields[0];
    const int c0 = f0.col;
    const Chap ch = chapterById(m_data.cols[c0].chapterId);
    bool anyHollow = false;
    for (int k = ch.c0; k < ch.c0 + ch.span; ++k) if (hollow(k)) anyHollow = true;

    // sugestão e trecho grifado
    const QString prev = ch.c0 > 0 ? effMarker(ch.c0 - 1) : QString();
    m_sug = FillAssist::Suggestion();
    if (m_sugBox && anyHollow && f0.chapterLevel && f0.fld->text().trimmed().isEmpty()) {
        const QString prevLabel = ch.c0 > 0 ? tr("Cap %1").arg(m_data.cols[ch.c0 - 1].ruler) : QString();
        m_sug = FillAssist::suggest(m_openingText, prev, prevLabel);
    }
    if (m_sugBox) {
        m_sugBox->setVisible(m_sug.valid());
        if (m_sug.valid()) { m_sugMarker->setText(m_sug.marker); m_sugWhy->setText(m_sug.why); }
    }
    if (m_excerpt) {
        const QString op = FillAssist::opening(m_openingText);
        QString html = op.toHtmlEscaped();
        if (m_sug.valid() && !m_sug.phrase.isEmpty()) {
            const int at = op.indexOf(m_sug.phrase);
            if (at >= 0)
                html = op.left(at).toHtmlEscaped()
                     + QStringLiteral("<span style='background:%1;color:%2;font-style:normal'>&nbsp;%3&nbsp;</span>")
                           .arg(css(alpha(pal.accent, 0.22)), pal.bright.name(), m_sug.phrase.toHtmlEscaped())
                     + op.mid(at + m_sug.phrase.size()).toHtmlEscaped();
        }
        m_excerpt->setText(html);
    }

    if (m_ditto) {
        m_ditto->setEnabled(!prev.isEmpty());
        m_ditto->setToolTip(prev.isEmpty() ? tr("O capítulo anterior não tem data")
                                           : tr("Igual ao anterior: %1 (Ctrl+D)").arg(prev));
        const QString next = prev.isEmpty() ? QString() : FillAssist::shifted(prev, 1, FillAssist::Unit::Day);
        m_plus->setEnabled(!next.isEmpty());
        m_plus->setToolTip(next.isEmpty() ? tr("Só dá pra somar a partir de \"Dia N\" ou de uma data")
                                          : tr("Um dia depois do anterior: %1 (Alt+↓)").arg(next));
    }
    if (m_aiBtn && !m_ai)
        m_aiBtn->setVisible(f0.txa->toPlainText().trimmed().isEmpty()
                            && !QSettings().value(QStringLiteral("ai/apiKey")).toString().isEmpty());

    m_progress->refresh();
    m_countLbl->setText(tr("%1 de %2").arg(m_idx + 1).arg(m_queue.size()));
    m_prevBtn->setEnabled(m_idx > 0);
    m_nextBtn->setText(m_idx >= m_queue.size() - 1 ? tr("Concluir") : tr("Salvar e próximo"));
}

void TimelineFillPanel::commit(Fields& f)
{
    const QString key = m_data.cols.value(f.col).key;
    if (key.isEmpty()) return;
    const QString m = f.fld->text().trimmed();
    const QString s = f.txa->toPlainText().trimmed();
    const bool mChanged = m != f.boundMarker.trimmed();
    const bool sChanged = s != f.boundSummary.trimmed();
    if (mChanged) f.boundMarker = m;
    if (sChanged) f.boundSummary = s;
    // os sinais podem voltar como setData() na mesma pilha — por isso o
    // estado "gravado" é atualizado antes de emitir
    const bool level = f.chapterLevel;
    if (mChanged) emit edited(key, level, false, m);
    if (sChanged) emit edited(key, level, true, s);
}

void TimelineFillPanel::commitPending()
{
    for (Fields& f : m_fields) commit(f);
}

TimelineFillPanel::Fields* TimelineFillPanel::fieldsOf(QObject* o)
{
    for (Fields& f : m_fields) if (f.fld == o || f.txa == o) return &f;
    return nullptr;
}

bool TimelineFillPanel::eventFilter(QObject* o, QEvent* e)
{
    if (e->type() == QEvent::FocusOut) {
        if (Fields* f = fieldsOf(o); f && o == f->txa) commit(*f);
    } else if (e->type() == QEvent::KeyPress) {
        auto* k = static_cast<QKeyEvent*>(e);
        Fields* f = fieldsOf(o);
        if (!f) return QWidget::eventFilter(o, e);
        const bool ctrl = k->modifiers() & Qt::ControlModifier;
        const bool alt = k->modifiers() & Qt::AltModifier;
        if (o == f->fld) {
            if ((k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter) && !ctrl) {
                commit(*f);
                f->txa->setFocus();
                return true;
            }
            if (ctrl && k->key() == Qt::Key_D && m_ditto && f == &m_fields[0]) { m_ditto->click(); return true; }
            if (alt && k->key() == Qt::Key_Down && m_plus && f == &m_fields[0]) { m_plus->click(); return true; }
        }
        if (k->key() == Qt::Key_Escape) { static_cast<QWidget*>(o)->clearFocus(); setFocus(); return true; }
    }
    return QWidget::eventFilter(o, e);
}

void TimelineFillPanel::askAiSummary()
{
    if (m_fields.isEmpty() || m_ai || !m_resolver) return;
    QSettings settings;
    const QString key = settings.value(QStringLiteral("ai/apiKey")).toString();
    if (key.isEmpty()) return;
    const Column& cc = m_data.cols[m_fields[0].col];
    const QString chId = cc.chapterId;
    const QString text = m_resolver(QStringLiteral("full:ch:") + chId).left(60000);
    if (text.trimmed().isEmpty()) {
        m_aiBtn->setText(tr("o capítulo está vazio"));
        QTimer::singleShot(2500, this, [this]() { if (m_aiBtn) m_aiBtn->setText(tr("%1 resume").arg(miraAssistantName())); });
        return;
    }
    m_ai = new AIClient(this);
    m_ai->setApiKey(key);
    m_ai->setBaseUrl(settings.value(QStringLiteral("ai/baseUrl"), QStringLiteral("https://api.openai.com/v1")).toString());
    m_ai->setModel(settings.value(QStringLiteral("ai/model"), QStringLiteral("gpt-4o-mini")).toString());
    AIChatMessage sys;
    sys.role = QStringLiteral("system");
    sys.content = QStringLiteral(
        "Resuma o capítulo abaixo em uma ou duas frases curtas, no idioma do próprio texto, "
        "no presente, dizendo o que acontece e com quem. Sem preâmbulo, sem aspas, sem "
        "opinião, e nada que não esteja no capítulo.");
    AIChatMessage user;
    user.role = QStringLiteral("user");
    user.content = QStringLiteral("Capítulo \"%1\":\n\n%2").arg(cc.chTitle, text);
    m_aiBtn->setEnabled(false);
    m_aiBtn->setText(tr("lendo o capítulo…"));
    QPointer<AIClient> client = m_ai;
    connect(m_ai, &AIClient::finished, this, [this, client, chId](const QString& answer) {
        if (client) client->deleteLater();
        if (m_aiBtn) { m_aiBtn->setEnabled(true); m_aiBtn->setText(tr("%1 resume").arg(miraAssistantName())); }
        if (m_fields.isEmpty() || m_data.cols.value(m_fields[0].col).chapterId != chId) return; // já trocou de capítulo
        Fields& f = m_fields[0];
        if (!f.txa->toPlainText().trimmed().isEmpty()) return; // o autor escreveu enquanto isso
        f.txa->setPlainText(answer.trimmed());
        commit(f);
    });
    connect(m_ai, &AIClient::errorOccurred, this, [this, client](const QString& err) {
        if (client) client->deleteLater();
        if (!m_aiBtn) return;
        m_aiBtn->setEnabled(true);
        m_aiBtn->setText(tr("não deu: %1").arg(err.left(40)));
        m_aiBtn->setToolTip(err);
        QTimer::singleShot(4000, this, [this]() { if (m_aiBtn) m_aiBtn->setText(tr("%1 resume").arg(miraAssistantName())); });
    });
    m_ai->sendMessage({ sys, user });
}

void TimelineFillPanel::paintEvent(QPaintEvent* e)
{
    QWidget::paintEvent(e);
    QPainter p(this);
    p.fillRect(QRect(0, 0, 1, height()), Palette::onPanels().border);
}

void TimelineFillPanel::applyTheme()
{
    const Palette pal = Palette::onPanels();
    const QColor fieldBg = mix(pal.app, pal.page, 0.55);
    m_closeBtn->setIcon(strokeIcon(QStringLiteral("close"), pal.dim));
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QWidget#tlFill { background: %1; }
        QScrollArea#tlFillScroll, QScrollArea#tlFillScroll > QWidget > QWidget, QWidget#tlFillBody,
        QWidget#tlFillDone { background: %1; }
        QWidget#tlFillQueue { background: %1; border-bottom: 1px solid %2; }
        QWidget#tlFillFoot { background: %1; border-top: 1px solid %2; }
        QLabel#tlFillSection { color: %3; background: transparent; }
        QLabel#tlFillDim, QLabel#tlFillChip { color: %3; background: transparent; }
        QLabel#tlFillTitle, QLabel#tlFillDoneTitle { color: %4; background: transparent; }
        QLabel#tlFillSceneHead { background: transparent; }
        QLabel#tlFillExcerpt { color: %5; background: transparent; border-left: 2px solid %2; padding-left: 11px; }
        QToolButton#tlFillIcon { background: transparent; border: none; border-radius: @radius-control; }
        QToolButton#tlFillIcon:hover { background: %6; }
        QFrame#tlFillSeg { background: %7; border: 1px solid %2; border-radius: @radius-control; }
        QToolButton#tlFillSegBtn { background: transparent; border: none; border-radius: 4px; color: %3;
                                   font-size: 11.5px; padding: 0 9px; }
        QToolButton#tlFillSegBtn:checked { background: %1; color: %4; }
        QToolButton#tlFillSplit { background: transparent; border: 1px solid %2; border-radius: 11px; color: %3; padding: 0 9px; }
        QToolButton#tlFillSplit:hover { color: %4; border-color: %8; }
        QFrame#tlFillSug { background: %9; border-radius: @radius-control; }
        QLabel#tlFillSugMarker { color: %4; background: transparent; }
        QLineEdit#tlFillFld { background: %10; border: 1px solid %2; border-radius: @radius-control; color: %4;
                              padding: 0 9px; selection-background-color: %11; }
        QLineEdit#tlFillFld[miss="true"] { border-color: %12; }
        QLineEdit#tlFillFld:focus { border-color: %13; }
        QTextEdit#tlFillTxa { background: %10; border: 1px solid %2; border-radius: @radius-control; color: %5;
                              padding: 2px 3px; selection-background-color: %11; }
        QTextEdit#tlFillTxa:focus { border-color: %13; }
        QLabel#tlFillLei { color: %3; background: transparent; }
        QLabel#tlFillLei[kind="ok"] { color: %14; }
        QLabel#tlFillLei[kind="bad"] { color: %15; }
        QPushButton#tlFillBtn { background: transparent; border: 1px solid %2; border-radius: @radius-control;
                                color: %5; font-size: 12px; padding: 0 11px; }
        QPushButton#tlFillBtn:hover { border-color: %8; }
        QPushButton#tlFillBtn:disabled { color: %3; border-color: %6; }
        QPushButton#tlFillPrimary { background: %13; border: none; border-radius: @radius-control;
                                    color: %16; font-size: 12px; font-weight: 600; padding: 0 13px; }
        QPushButton#tlFillPrimary:hover { background: %17; }
        QPushButton#tlFillMini { background: transparent; border: 1px solid %2; border-radius: @radius-item;
                                 color: %3; font-size: 11px; padding: 0 7px; }
        QPushButton#tlFillMini:hover { color: %4; border-color: %8; }
        QPushButton#tlFillMini:disabled { color: %6; border-color: %6; }
        QPushButton#tlFillAi { background: transparent; border: none; color: %18; font-size: 11px; padding: 0; }
        QPushButton#tlFillAi:hover { text-decoration: underline; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
        QScrollBar::handle:vertical { background: %2; border-radius: 3px; min-height: 30px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )")).arg(pal.panel.name(),                          // 1
             pal.border.name(),                         // 2
             pal.dim.name(),                            // 3
             pal.bright.name(),                         // 4
             pal.ink.name(),                            // 5
             css(alpha(pal.ink, 0.08)),                 // 6
             css(alpha(pal.ink, 0.07)),                 // 7
             css(alpha(pal.ink, 0.40)),                 // 8
             css(alpha(pal.accent, 0.09)))              // 9
         .arg(fieldBg.name(),                           // 10
              css(alpha(pal.accent, 0.35)),             // 11
              mix(pal.warning, pal.border, 0.55).name(),// 12
              pal.accent.name(),                        // 13
              pal.success.name(),                       // 14
              pal.warning.name(),                       // 15
              pal.app.name(),                           // 16
              mix(pal.accent, pal.bright, 0.85).name(), // 17
              pal.info.name()));                        // 18
}
