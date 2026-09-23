#include "TimelineInspector.h"

#include "Theme.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace Tracks;

namespace {

enum class Icon { Pencil, Pin, Open, Close };

QPixmap iconPx(Icon kind, const QColor& c, int px = 14)
{
    const qreal dpr = 2.0;
    QPixmap pm(QSize(px, px) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(px / 16.0, px / 16.0);
    QPen pen(c, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    switch (kind) {
    case Icon::Pencil:
        path.moveTo(3, 13); path.lineTo(13, 13);
        path.moveTo(4, 10); path.lineTo(10, 4); path.lineTo(12, 6); path.lineTo(6, 12); path.lineTo(4, 12);
        path.closeSubpath();
        break;
    case Icon::Pin:
        path.moveTo(8, 14);
        path.cubicTo(8, 14, 13, 9.5, 13, 5.8);
        path.arcTo(QRectF(3, 0.8, 10, 10), 0, 180);
        path.cubicTo(3, 9.5, 8, 14, 8, 14);
        p.drawPath(path);
        path = QPainterPath();
        path.addEllipse(QPointF(8, 6), 1.7, 1.7);
        break;
    case Icon::Open:
        path.moveTo(9, 3); path.lineTo(13, 3); path.lineTo(13, 7);
        path.moveTo(13, 3); path.lineTo(7, 9);
        path.moveTo(11, 10); path.lineTo(11, 13); path.lineTo(3, 13); path.lineTo(3, 5); path.lineTo(6, 5);
        break;
    case Icon::Close:
        path.moveTo(4, 4); path.lineTo(12, 12); path.moveTo(12, 4); path.lineTo(4, 12);
        break;
    }
    p.drawPath(path);
    return pm;
}

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text.toUpper(), parent);
    l->setObjectName(QStringLiteral("tlInspSection"));
    QFont f = uiFont(9.5, QFont::DemiBold);
    f.setLetterSpacing(QFont::PercentageSpacing, 113);
    l->setFont(f);
    return l;
}

QString css(const QColor& c) { return c.name(QColor::HexArgb); }

// Linha de pessoas presentes (avatar + nome), com quebra — cada uma clicável.
class PeopleRow : public QWidget {
public:
    PeopleRow(const QList<Character>& people, QWidget* parent) : QWidget(parent), m_people(people)
    {
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    std::function<void(const QString&)> onClick;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return int(layoutItems(w).second); }
    QSize sizeHint() const override { return { 268, heightForWidth(268) }; }
protected:
    QPair<QList<QPair<QRectF, int>>, qreal> layoutItems(int w) const
    {
        QList<QPair<QRectF, int>> out;
        const QFontMetricsF fm(uiFont(12.5));
        qreal x = 0, y = 0;
        const qreal h = 20;
        for (int i = 0; i < m_people.size(); ++i) {
            const qreal iw = 16 + 6 + fm.horizontalAdvance(m_people[i].name);
            if (x > 0 && x + iw > w) { x = 0; y += h + 6; }
            out.append({ QRectF(x, y, iw, h), i });
            x += iw + 12;
        }
        return { out, m_people.isEmpty() ? 0.0 : y + h };
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette pal = Palette::current();
        const QFont f = uiFont(12.5);
        const QFontMetricsF fm(f);
        for (const auto& it : layoutItems(width()).first) {
            const Character& c = m_people[it.second];
            drawAvatar(&p, QRectF(it.first.left(), it.first.center().y() - 8, 16, 16), c, pal.app);
            p.setFont(f);
            p.setPen(it.second == m_hover ? pal.bright : pal.ink);
            p.drawText(QPointF(it.first.left() + 22, it.first.center().y() + fm.ascent() / 2 - 1.5), c.name);
        }
    }
    int at(const QPointF& pos) const
    {
        for (const auto& it : layoutItems(width()).first) if (it.first.contains(pos)) return it.second;
        return -1;
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        const int h = at(e->position());
        if (h != m_hover) { m_hover = h; setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); }
    }
    void leaveEvent(QEvent*) override { m_hover = -1; update(); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        const int i = at(e->position());
        if (i >= 0 && onClick) onClick(m_people[i].id);
    }
private:
    QList<Character> m_people;
    int m_hover = -1;
};

// Linha de "Ligações": glifo + texto rico, clicável, com hover.
class LinkRow : public QFrame {
public:
    LinkRow(const QString& glyph, const QColor& glyphColor, const QString& html, const QString& small,
            QWidget* parent) : QFrame(parent)
    {
        setObjectName(QStringLiteral("tlInspLink"));
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        auto* h = new QHBoxLayout(this);
        h->setContentsMargins(7, 6, 7, 6);
        h->setSpacing(9);
        auto* g = new QLabel(glyph, this);
        g->setFont(monoFont(12, QFont::Medium));
        g->setFixedWidth(16);
        g->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        g->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(glyphColor.name()));
        h->addWidget(g, 0, Qt::AlignTop);
        const Palette pal = Palette::current();
        auto* t = new QLabel(this);
        t->setTextFormat(Qt::RichText);
        t->setWordWrap(true);
        t->setFont(uiFont(12));
        t->setText(QStringLiteral("<div style='color:%1'>%2</div><div style='color:%3;font-size:11px'>%4</div>")
                       .arg(pal.ink.name(), html, pal.dim.name(), small.toHtmlEscaped()));
        t->setAttribute(Qt::WA_TransparentForMouseEvents);
        t->setStyleSheet(QStringLiteral("background:transparent;"));
        h->addWidget(t, 1);
    }
    std::function<void()> onClick;
protected:
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && onClick) onClick();
    }
};

QPushButton* textLink(const QString& text, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(QStringLiteral("tlInspTextLink"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFlat(true);
    return b;
}

QPushButton* primaryBtn(const QString& text, const QPixmap& icon, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(QStringLiteral("tlInspPrimary"));
    b->setCursor(Qt::PointingHandCursor);
    if (!icon.isNull()) { b->setIcon(QIcon(icon)); b->setIconSize(QSize(14, 14)); }
    b->setFixedHeight(27);
    return b;
}
} // namespace

TimelineInspector::TimelineInspector(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("tlInspector"));
    setAttribute(Qt::WA_StyledBackground);
    setFixedWidth(300);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName(QStringLiteral("tlInspScroll"));
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(m_scroll);

    m_close = new QToolButton(this);
    m_close->setObjectName(QStringLiteral("tlInspClose"));
    m_close->setCursor(Qt::PointingHandCursor);
    m_close->setToolTip(tr("Fechar (Esc)"));
    m_close->setFixedSize(28, 28);
    connect(m_close, &QToolButton::clicked, this, &TimelineInspector::closeRequested);

    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, &TimelineInspector::applyTheme);
}

void TimelineInspector::applyTheme()
{
    const Palette pal = Palette::current();
    m_close->setIcon(QIcon(iconPx(Icon::Close, pal.dim, 14)));
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QWidget#tlInspector { background: %1; }
        QScrollArea#tlInspScroll, QScrollArea#tlInspScroll > QWidget > QWidget { background: %1; }
        QLabel { background: transparent; }
        QLabel#tlInspSection { color: %2; }
        QToolButton#tlInspClose { background: transparent; border: none; border-radius: @radius-control; }
        QToolButton#tlInspClose:hover { background: %3; }
        QFrame#tlInspLink { background: transparent; border-radius: @radius-item; }
        QFrame#tlInspLink:hover { background: %3; }
        QPushButton#tlInspTextLink { background: transparent; border: none; color: %2; font-size: 12px;
                                     padding: 0; text-align: left; }
        QPushButton#tlInspTextLink:hover { color: %4; }
        QPushButton#tlInspPrimary {
            background: %5; border: 1px solid %6; border-radius: @radius-control;
            color: %4; font-size: 12px; padding: 0 10px;
        }
        QPushButton#tlInspPrimary:hover { background: %7; }
        QFrame#tlInspMoved { background: %8; border-radius: @radius-control; }
        QLineEdit#tlInspFill {
            background: rgba(0,0,0,0.2); color: %4; border-radius: @radius-control;
            border: 1px solid %9; padding: 0 9px; min-height: 26px;
        }
        QLineEdit#tlInspFill:focus { border-color: %10; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
        QScrollBar::handle:vertical { background: %11; border-radius: 3px; min-height: 30px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )"))
        .arg(pal.panel.name(),                          // 1
             pal.dim.name(),                            // 2
             css(alpha(pal.ink, 0.07)),                 // 3
             pal.bright.name(),                         // 4
             css(alpha(pal.accent, 0.16)),              // 5
             css(alpha(pal.accent, 0.55)),              // 6
             css(alpha(pal.accent, 0.26)),              // 7
             css(alpha(pal.accent, 0.09)),              // 8
             css(alpha(pal.warning, 0.6)),              // 9
             pal.warning.name(),                        // 10
             pal.border.name()));                       // 11
}

void TimelineInspector::paintEvent(QPaintEvent* e)
{
    QWidget::paintEvent(e);
    QPainter p(this);
    p.fillRect(QRect(0, 0, 1, height()), Palette::current().border);
}

void TimelineInspector::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    m_close->move(width() - 10 - m_close->width(), 10);
    m_close->raise();
}

void TimelineInspector::clear()
{
    if (m_body) { m_scroll->takeWidget(); m_body->deleteLater(); m_body = nullptr; m_lay = nullptr; }
}

void TimelineInspector::showFor(const Data& d, const QString& id)
{
    m_id = id;
    clear();
    const Event* e = d.event(id);
    if (!e) return;
    const Palette pal = Palette::current();

    m_body = new QWidget;
    m_lay = new QVBoxLayout(m_body);
    m_lay->setContentsMargins(16, 16, 16, 20);
    m_lay->setSpacing(16);

    const Lane* L = d.lane(e->laneId);
    const bool loose = e->col < 0;
    const QColor lc = loose ? pal.ink : (L ? L->color : pal.accent);
    const QString laneName = loose ? tr("Soltos") : (L ? L->name : QString());

    // ── cabeçalho ──────────────────────────────────────────────────────────────
    {
        auto* head = new QWidget(m_body);
        auto* v = new QVBoxLayout(head);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);
        auto* chip = new QLabel(head);
        chip->setTextFormat(Qt::RichText);
        chip->setFont(uiFont(11));
        chip->setText(QStringLiteral("<span style='color:%1;font-size:10px'>&#9679;</span>&nbsp;"
                                     "<span style='color:%2'>%3</span>")
                          .arg(lc.name(), pal.dim.name(), laneName.toHtmlEscaped()));
        v->addWidget(chip);
        v->addSpacing(8);
        auto* mk = new QLabel(e->hollow ? tr("sem data") : e->marker, head);
        mk->setFont(monoFont(13, QFont::Medium));
        mk->setStyleSheet(QStringLiteral("color:%1;").arg((e->hollow ? pal.warning : lc).name()));
        mk->setVisible(e->hollow || !e->marker.isEmpty());
        v->addWidget(mk);
        v->addSpacing(4);
        auto* title = new QLabel(e->title, head);
        title->setWordWrap(true);
        title->setFont(serifFont(20));
        title->setStyleSheet(QStringLiteral("color:%1;padding-right:20px;").arg(pal.bright.name()));
        v->addWidget(title);
        m_lay->addWidget(head);
    }

    // ── sem data: preencher aqui mesmo ─────────────────────────────────────────
    if (e->hollow) {
        auto* sec = new QWidget(m_body);
        auto* v = new QVBoxLayout(sec);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(7);
        v->addWidget(sectionLabel(tr("Quando se passa?"), sec));
        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        auto* edit = new QLineEdit(sec);
        edit->setObjectName(QStringLiteral("tlInspFill"));
        edit->setFont(monoFont(12));
        edit->setPlaceholderText(tr("ex.: Dia 17"));
        auto* save = primaryBtn(tr("Salvar"), QPixmap(), sec);
        row->addWidget(edit, 1);
        row->addWidget(save);
        v->addLayout(row);
        auto* hint = new QLabel(tr("Sem marcador, este capítulo não entra na ordem da história."), sec);
        hint->setWordWrap(true);
        hint->setFont(uiFont(11));
        hint->setStyleSheet(QStringLiteral("color:%1;").arg(pal.dim.name()));
        v->addWidget(hint);
        const QString evId = e->id;
        auto go = [this, edit, evId]() {
            const QString m = edit->text().trimmed();
            if (!m.isEmpty()) emit fillMarkerRequested(evId, m);
        };
        connect(save, &QPushButton::clicked, this, go);
        connect(edit, &QLineEdit::returnPressed, this, go);
        m_lay->addWidget(sec);
    }

    // ── origem (evento manual) ─────────────────────────────────────────────────
    if (e->manual && !e->originWhere.isEmpty()) {
        auto* row = new QWidget(m_body);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        auto* ic = new QLabel(row);
        ic->setPixmap(iconPx(Icon::Pencil, pal.dim, 14));
        h->addWidget(ic);
        auto* t = new QLabel(tr("Criado %1").arg(e->originWhere), row);
        t->setWordWrap(true);
        t->setFont(uiFont(11.5));
        t->setStyleSheet(QStringLiteral("color:%1;").arg(pal.dim.name()));
        h->addWidget(t, 1);
        m_lay->addWidget(row);
    }

    // ── movido pelo usuário ────────────────────────────────────────────────────
    if (e->moved && L) {
        auto* box = new QFrame(m_body);
        box->setObjectName(QStringLiteral("tlInspMoved"));
        auto* h = new QVBoxLayout(box);
        h->setContentsMargins(10, 8, 10, 8);
        auto* t = new QLabel(box);
        t->setTextFormat(Qt::RichText);
        t->setWordWrap(true);
        t->setFont(uiFont(12));
        t->setText(tr("Você moveu este capítulo para <b style='color:%1'>%2</b>. "
                      "A detecção automática não mexe mais nele. "
                      "<a href='undo' style='color:%3;text-decoration:none'>Desfazer</a>")
                       .arg(pal.bright.name(), L->name.toHtmlEscaped(), pal.accent.name()));
        t->setStyleSheet(QStringLiteral("color:%1;").arg(pal.ink.name()));
        const QString evId = e->id;
        connect(t, &QLabel::linkActivated, this, [this, evId]() { emit undoMoveRequested(evId); });
        h->addWidget(t);
        m_lay->addWidget(box);
    }

    // ── capítulo + abrir no editor ─────────────────────────────────────────────
    if (loose) {
        auto* hint = new QLabel(tr("Ainda sem lugar. Arraste o cartão para uma linha, na altura do capítulo."), m_body);
        hint->setWordWrap(true);
        hint->setFont(uiFont(11));
        hint->setStyleSheet(QStringLiteral("color:%1;").arg(pal.dim.name()));
        m_lay->addWidget(hint);
    } else if (e->col < d.cols.size()) {
        const Column& col = d.cols[e->col];
        auto* row = new QWidget(m_body);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(8);
        QString txt = e->manual ? tr("Dentro de %1").arg(col.unitLabel) : col.unitLabel;
        if (!e->manual && e->col == d.editorCol) txt += tr(" · aberto agora");
        auto* t = new QLabel(txt, row);
        t->setWordWrap(true);
        t->setFont(uiFont(12));
        t->setStyleSheet(QStringLiteral("color:%1;").arg(pal.dim.name()));
        h->addWidget(t, 1);
        auto* open = primaryBtn(e->manual && e->origin == QLatin1String("editor") ? tr("Abrir o trecho")
                                                                                    : tr("Abrir no editor"),
                                iconPx(Icon::Open, pal.bright, 14), row);
        const QString evId = e->id;
        connect(open, &QPushButton::clicked, this, [this, evId]() { emit openInEditorRequested(evId); });
        h->addWidget(open);
        m_lay->addWidget(row);
    }

    // ── resumo ─────────────────────────────────────────────────────────────────
    if (!e->summary.isEmpty()) {
        auto* sum = new QLabel(m_body);
        sum->setTextFormat(Qt::RichText);
        sum->setWordWrap(true);
        sum->setFont(serifFont(13.5, QFont::Normal));
        sum->setText(QStringLiteral("<p style='line-height:120%;color:%1;margin:0'>%2</p>")
                         .arg(pal.ink.name(), e->summary.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"))));
        sum->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_lay->addWidget(sum);
    }

    // ── presentes + onde ───────────────────────────────────────────────────────
    if (!e->castIds.isEmpty() || !e->placeName.isEmpty()) {
        auto* meta = new QWidget(m_body);
        auto* v = new QVBoxLayout(meta);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(9);
        if (!e->castIds.isEmpty()) {
            QList<Character> people;
            for (const QString& cid : e->castIds) if (const Character* c = d.character(cid)) people << *c;
            auto* pr = new PeopleRow(people, meta);
            pr->onClick = [this](const QString& cid) { emit characterClicked(cid); };
            v->addWidget(pr);
        }
        if (!e->placeName.isEmpty()) {
            auto* row = new QWidget(meta);
            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(6);
            auto* ic = new QLabel(row);
            ic->setPixmap(iconPx(Icon::Pin, pal.ink, 14));
            h->addWidget(ic);
            auto* t = new QLabel(e->placeName, row);
            t->setFont(uiFont(12.5));
            t->setStyleSheet(QStringLiteral("color:%1;").arg(pal.ink.name()));
            h->addWidget(t, 1);
            v->addWidget(row);
        }
        m_lay->addWidget(meta);
    }

    // ── ligações ───────────────────────────────────────────────────────────────
    struct Link { QString g; QString id; QString html; QString small; };
    QList<Link> links;
    auto unit = [&](const Event& o) { return o.col >= 0 && o.col < d.cols.size() ? d.cols[o.col].ruler : QString(); };
    auto em = [&](const QString& s) { return QStringLiteral("<span style='color:%1'>%2</span>").arg(pal.bright.name(), s.toHtmlEscaped()); };
    if (e->manual && e->col >= 0) {
        for (const Event& o : d.events)
            if (!o.manual && o.col == e->col) {
                const Lane* ol = d.lane(o.laneId);
                links.append({ QStringLiteral("◦"), o.id,
                               tr("Parte de %1").arg(em(tr("Cap %1 · %2").arg(unit(o), o.title))),
                               ol ? ol->name : QString() });
                break;
            }
    } else if (!e->manual && e->col >= 0) {
        if (e->chronoOk)
            for (const Event& o : d.events) {
                if (o.id == e->id || o.manual || o.col < 0 || !o.chronoOk || o.chrono != e->chrono) continue;
                const Lane* ol = d.lane(o.laneId);
                links.append({ QStringLiteral("="), o.id,
                               tr("Acontece ao mesmo tempo que %1").arg(em(tr("Cap %1 · %2").arg(unit(o), o.title))),
                               (ol ? ol->name : QString()) + QStringLiteral(" · ") + o.marker });
            }
        QList<const Event*> same;
        for (const Event& o : d.events) if (!o.manual && o.col >= 0 && o.laneId == e->laneId) same << &o;
        std::sort(same.begin(), same.end(), [](const Event* a, const Event* b) { return a->col < b->col; });
        const int i = int(std::find(same.begin(), same.end(), e) - same.begin());
        auto gapTxt = [](const Event* a, const Event* b, const QString& w) {
            if (!a->chronoOk || !b->chronoOk) return tr("intervalo desconhecido");
            if (a->chrono == b->chrono) return tr("mesmo dia");
            return gapWord(std::abs(b->chrono - a->chrono) / 1440.0) + QLatin1Char(' ') + w;
        };
        if (i > 0 && i < same.size())
            links.append({ QStringLiteral("←"), same[i - 1]->id,
                           tr("Antes: %1").arg(em(tr("Cap %1 · %2").arg(unit(*same[i - 1]), same[i - 1]->title))),
                           gapTxt(same[i - 1], e, tr("antes")) });
        if (i >= 0 && i + 1 < same.size())
            links.append({ QStringLiteral("→"), same[i + 1]->id,
                           tr("Depois: %1").arg(em(tr("Cap %1 · %2").arg(unit(*same[i + 1]), same[i + 1]->title))),
                           gapTxt(e, same[i + 1], tr("depois")) });
        // ramificações
        const int li = d.laneIndex(e->laneId);
        const BranchSpan bs = branchSpan(d, li);
        if (bs.parent >= 0) {
            const Lane& P = d.lanes[bs.parent];
            if (e->col == bs.first && bs.from >= 0) {
                QString fromId;
                for (const Event& o : d.events) if (!o.manual && o.laneId == P.id && o.col == bs.from) fromId = o.id;
                links.append({ QStringLiteral("⑂"), fromId,
                               tr("Separou de %1 depois do %2").arg(em(P.name), em(tr("Cap %1").arg(d.cols[bs.from].ruler))),
                               tr("detectado sozinho") });
            }
            if (e->col == bs.last && bs.to >= 0) {
                QString toId;
                for (const Event& o : d.events) if (!o.manual && o.laneId == P.id && o.col == bs.to) toId = o.id;
                links.append({ QStringLiteral("⑃"), toId,
                               tr("Reencontra %1 no %2").arg(em(P.name), em(tr("Cap %1").arg(d.cols[bs.to].ruler))),
                               tr("o elenco volta a se cruzar") });
            }
        }
        for (int bi = 0; bi < d.lanes.size(); ++bi) {
            const BranchSpan b = branchSpan(d, bi);
            if (b.parent != li || b.to != e->col) continue;
            QString lastId;
            for (const Event& o : d.events) if (!o.manual && o.laneId == d.lanes[bi].id && o.col == b.last) lastId = o.id;
            links.append({ QStringLiteral("⑃"), lastId,
                           tr("Recebe %1 (%2)").arg(em(d.lanes[bi].name), tr("Cap %1").arg(d.cols[b.last].ruler)),
                           tr("convergência") });
        }
    }
    if (!links.isEmpty()) {
        auto* sec = new QWidget(m_body);
        auto* v = new QVBoxLayout(sec);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(7);
        v->addWidget(sectionLabel(tr("Ligações"), sec));
        auto* list = new QVBoxLayout;
        list->setSpacing(2);
        for (const Link& lk : links) {
            auto* row = new LinkRow(lk.g, lc, lk.html, lk.small, sec);
            const QString target = lk.id;
            row->onClick = [this, target]() { if (!target.isEmpty()) emit selectRequested(target); };
            list->addWidget(row);
        }
        v->addLayout(list);
        m_lay->addWidget(sec);
    }

    // ── rodapé ────────────────────────────────────────────────────────────────
    m_lay->addStretch(1);
    {
        auto* foot = new QWidget(m_body);
        auto* h = new QHBoxLayout(foot);
        h->setContentsMargins(0, 6, 0, 0);
        h->setSpacing(18);
        auto* edit = textLink(tr("Editar evento"), foot);
        auto* exp  = textLink(tr("Exportar como documento"), foot);
        const QString evId = e->id;
        connect(edit, &QPushButton::clicked, this, [this, evId]() { emit editRequested(evId); });
        connect(exp,  &QPushButton::clicked, this, [this, evId]() { emit exportRequested(evId); });
        h->addWidget(edit);
        h->addWidget(exp);
        h->addStretch();
        m_lay->addWidget(foot);
    }

    m_scroll->setWidget(m_body);
    m_close->raise();
}
