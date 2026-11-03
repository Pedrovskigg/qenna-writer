// As classes de planeta na interface: o cartão da vista, o que o painel e o Caderno mostram e a edição.

#include "GalaxyPanel.h"
#include "GalaxyCaderno.h"
#include "GalaxyHab.h"

#include "../carto/CartoIcons.h"
#include "../Theme.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace galaxy {

namespace {
QLabel* textOf(const QString& text, const QString& obj, QWidget* parent, bool wrap = false)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(obj);
    l->setWordWrap(wrap);
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
    return l;
}
}

void Panel::buildHab()
{
    m_habBtn = makeBtn(m_cbar, QStringLiteral("galClasses"), tr("Classes de planeta (L)"));
    m_habBtn->setCheckable(true);
    connect(m_habBtn, &QToolButton::clicked, this, [this]() { setHabOn(!m_canvas->hab); });
    m_habCard = new QFrame(this);
    m_habCard->setObjectName(QStringLiteral("cartoCard"));
    m_habCard->setFixedWidth(300);
    auto* l = new QVBoxLayout(m_habCard);
    l->setContentsMargins(14, 12, 14, 12);
    l->setSpacing(7);
    m_habCard->hide();
}
void Panel::setHabOn(bool on)
{
    m_canvas->setHab(on);
    renderHabCard();
    syncHabCard();
    if (m_peek && m_peek->isVisible()) placePeek();
}
void Panel::syncHabCard()
{
    if (!m_habCard) return;
    const bool hide = !m_canvas->hab || m_settings->isVisible() || m_export->isVisible() || m_tool == QLatin1String("realm") || (m_caderno && m_caderno->isVisible());
    m_habCard->setVisible(!hide);
    if (m_habBtn) m_habBtn->setChecked(m_canvas->hab);
    if (!hide) {
        m_habCard->adjustSize();
        m_habCard->move(m_cbar->x() - 8 - m_habCard->width(), 14);
        m_habCard->raise();
    }
}
void Panel::renderHabCard()
{
    if (!m_habCard || !m_canvas->hab || !m_doc.gal) return;
    auto* v = static_cast<QVBoxLayout*>(m_habCard->layout());
    QLayoutItem* it;
    while ((it = v->takeAt(0))) { if (it->widget()) { it->widget()->hide(); it->widget()->deleteLater(); } delete it; }
    QString scope = tr("Toda a galáxia");
    std::vector<const System*> ss;
    Region* fr = nullptr;
    for (const auto& r : m_doc.regions) if (r->key == m_canvas->focusKey) fr = r.get();
    if (fr) { scope = fr->name; for (int id : fr->systems) ss.push_back(m_doc.gal->systems[size_t(id)].get()); }
    else for (const auto& s : m_doc.gal->systems) if (!s->gone) ss.push_back(s.get());
    const std::array<int, 7> c = m_canvas->habTally(ss);
    v->addWidget(textOf(tr("Classes de planeta"), QStringLiteral("cartoCardTitle"), m_habCard));
    v->addWidget(textOf(scope.toUpper(), QStringLiteral("cartoCaption"), m_habCard));
    auto* bar = new HabBar(m_habCard);
    bar->setCounts(c);
    v->addWidget(bar);
    for (int n = 1; n <= 6; n++) {
        auto* b = new QPushButton(m_habCard);
        b->setObjectName(QStringLiteral("gmHabRow"));
        b->setCheckable(true);
        b->setChecked(m_canvas->habOnly == n);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setProperty("off", c[size_t(n)] == 0);
        auto* h = new QHBoxLayout(b);
        h->setContentsMargins(8, 6, 8, 6);
        h->setSpacing(9);
        h->addWidget(habPill(n, b), 0, Qt::AlignVCenter);
        auto* col = new QVBoxLayout();
        col->setSpacing(1);
        col->addWidget(textOf(habName(n), QStringLiteral("cartoValue"), b));
        col->addWidget(textOf(habDesc(n), QStringLiteral("cartoNote"), b, true));
        h->addLayout(col, 1);
        h->addWidget(textOf(nf(c[size_t(n)]), QStringLiteral("cartoNote"), b), 0, Qt::AlignVCenter);
        // Botão com layout não conta o que tem dentro: a altura sai da largura que a linha tem no cartão.
        h->activate();
        b->setFixedHeight(h->hasHeightForWidth() ? h->totalHeightForWidth(272) : h->totalSizeHint().height());
        connect(b, &QPushButton::clicked, this, [this, n]() {
            m_canvas->setHabOnly(m_canvas->habOnly == n ? 0 : n);
            QTimer::singleShot(0, this, [this]() { renderHabCard(); syncHabCard(); });
        });
        v->addWidget(b);
    }
    QString note = tr("Cada sistema brilha na cor do melhor mundo que tem. Clique numa classe pra ver só ela.");
    if (c[0]) note += QLatin1Char(' ') + tr("%n mundo(s) construído(s) fora da régua.", "", c[0]);
    v->addWidget(textOf(note, QStringLiteral("cartoNote"), m_habCard, true));
}
// O que o painel e o Caderno mostram: a classe do mundo, ou a mistura de um lugar maior.
QWidget* Panel::habSection(const QString&, const Ref& o)
{
    auto* w = new QWidget();
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    const auto showBtn = [&]() {
        auto* l = new QToolButton(w);
        l->setText(tr("Ver no mapa"));
        l->setObjectName(QStringLiteral("cartoLink"));
        l->setCursor(Qt::PointingHandCursor);
        l->setFocusPolicy(Qt::NoFocus);
        connect(l, &QToolButton::clicked, this, [this]() { QTimer::singleShot(0, this, [this]() { if (m_caderno && m_caderno->isVisible()) closeCaderno(); setHabOn(true); }); });
        return l;
    };
    if (o.kind == RefKind::Planet || o.kind == RefKind::Moon) {
        const bool moon = o.kind == RefKind::Moon;
        const Body& b = moon ? *o.moon : *o.body;
        const HabInfo i = habInfo(b, o.sys, moon ? o.body : nullptr);
        if (!i.cls) { v->addWidget(textOf(habWhy(i, QStringLiteral(" · ")) + QLatin1Char('.'), QStringLiteral("cartoNote"), w, true)); return w; }
        auto* hd = new QHBoxLayout();
        hd->setSpacing(8);
        hd->addWidget(habPill(i.cls, w));
        hd->addWidget(textOf(habName(i.cls), QStringLiteral("cartoValue"), w));
        hd->addStretch();
        hd->addWidget(showBtn());
        v->addLayout(hd);
        v->addWidget(textOf(habDesc(i.cls), QStringLiteral("cartoNote"), w, true));
        if (b.habSet && b.habSet != i.autoCls) v->addWidget(textOf(tr("Classe escolhida por você. Pela conta seria %1: %2.").arg(habLabel(i.autoCls), habWhy(i, QStringLiteral(", ")).toLower()), QStringLiteral("cartoNote"), w, true));
        else if (!b.habSet) v->addWidget(textOf(habWhy(i, QStringLiteral(" · ")), QStringLiteral("cartoNote"), w, true));
        return w;
    }
    std::vector<const System*> ss;
    if (o.kind == RefKind::System) ss.push_back(o.sys);
    else if (o.kind == RefKind::Region) { for (int id : o.region->systems) ss.push_back(m_doc.gal->systems[size_t(id)].get()); }
    else if (o.kind == RefKind::Galaxy) { for (const auto& s : m_doc.gal->systems) if (!s->gone) ss.push_back(s.get()); }
    else { delete w; return nullptr; }
    const std::array<int, 7> c = m_canvas->habTally(ss);
    bool any = false;
    for (int n = 1; n <= 6; n++) if (c[size_t(n)]) any = true;
    if (!any) { delete w; return nullptr; }
    auto* hd = new QHBoxLayout();
    hd->addWidget(textOf(tr("Classes dos mundos").toUpper(), QStringLiteral("gmEyebrow"), w));
    hd->addStretch();
    hd->addWidget(showBtn());
    v->addLayout(hd);
    auto* bar = new HabBar(w);
    bar->setCounts(c);
    v->addWidget(bar);
    QStringList parts;
    for (int n = 1; n <= 6; n++) if (c[size_t(n)]) {
        const Rgb q = habRgb(n);
        parts << QStringLiteral("<span style='color:rgb(%1,%2,%3)'>■</span> %4 %5").arg(q[0]).arg(q[1]).arg(q[2]).arg(QString::fromLatin1(habRoman(n)), nf(c[size_t(n)]));
    }
    auto* list = textOf(parts.join(QStringLiteral(" &nbsp; ")), QStringLiteral("cartoNote"), w, true);
    list->setTextFormat(Qt::RichText);
    v->addWidget(list);
    int best = 0;
    QString bestKey;
    for (const System* s : ss) {
        const Canvas::SysHab& h = m_canvas->sysHab(*s);
        if (h.best && (!best || h.best < best || (h.best == best && m_doc.touched(h.bestKey) && !m_doc.touched(bestKey)))) { best = h.best; bestKey = h.bestKey; }
    }
    if (!bestKey.isEmpty()) {
        auto* row = new QPushButton(w);
        row->setObjectName(QStringLiteral("gmHabBest"));
        row->setCursor(Qt::PointingHandCursor);
        row->setFocusPolicy(Qt::NoFocus);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 2, 0, 2);
        h->setSpacing(8);
        h->addWidget(habPill(best, row));
        h->addWidget(textOf(tr("Melhor mundo: %1").arg(m_doc.labelOf(bestKey)), QStringLiteral("cartoValue"), row));
        h->addStretch();
        row->setFixedHeight(h->totalSizeHint().height());
        connect(row, &QPushButton::clicked, this, [this, bestKey]() {
            QTimer::singleShot(0, this, [this, bestKey]() { if (m_caderno && m_caderno->isVisible()) m_caderno->open(bestKey, m_caderno->returnTool()); else { m_canvas->select(bestKey, true); selected(bestKey); } });
        });
        v->addWidget(row);
    }
    return w;
}
// A classe na mão e o ar: a escolha fica, mesmo se a temperatura mudar depois.
QWidget* Panel::habEditSection(Body* b, System* s, Body* host)
{
    auto* w = new QWidget();
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(7);
    const HabInfo i = habInfo(*b, s, host);
    v->addWidget(textOf(tr("Classe").toUpper(), QStringLiteral("gmEyebrow"), w));
    auto* combo = new QComboBox(w);
    combo->setObjectName(QStringLiteral("cartoCombo"));
    combo->addItem(tr("Automática: %1").arg(habLabel(i.autoCls)), 0);
    for (int n = 1; n <= 6; n++) combo->addItem(habLabel(n), n);
    combo->setCurrentIndex(std::max(0, combo->findData(b->habSet)));
    connect(combo, &QComboBox::activated, this, [this, b, combo]() { b->habSet = combo->currentData().toInt(); edited(b); });
    v->addWidget(combo);
    if (i.autoCls != 6) {
        auto* ck = new QToolButton(w);
        ck->setObjectName(QStringLiteral("cartoCheck"));
        const bool on = airOf(*b) == AirKind::Breathable;
        ck->setCheckable(true);
        ck->setChecked(on);
        ck->setText(tr("Dá pra respirar o ar daqui"));
        ck->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        ck->setCursor(Qt::PointingHandCursor);
        ck->setFocusPolicy(Qt::NoFocus);
        const QColor muted = Theme::toColor(Theme::textMuted()), a = Theme::toColor(Theme::accentDefault());
        ck->setIcon(carto::icon(on ? QStringLiteral("check") : QStringLiteral("box"), muted, a));
        connect(ck, &QToolButton::clicked, this, [this, b, on]() { b->air = on ? -1 : 1; edited(b); });
        v->addWidget(ck);
    }
    v->addWidget(textOf(tr("%1. A classe é sugestão: a sua escolha fica, mesmo se a temperatura mudar depois.").arg(habWhy(i, QStringLiteral(" · "))), QStringLiteral("cartoNote"), w, true));
    return w;
}

} // namespace galaxy
