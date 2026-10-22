// Docas das camadas do mapa: Territórios, Construir e Nomes (os mesmos
// controles do Map Lab, no visual do Qenna). A doca só mostra e muda as
// opções da mesa; quem edita o mapa é a mesa (com desfazer).

#include "CartoPanel.h"

#include "CartoIcons.h"
#include "../Theme.h"

#include <QCheckBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace carto {

QString buildKindName(const QString& k)
{
    if (k == QLatin1String("select")) return Panel::tr("Mover");
    if (k == QLatin1String("bridge")) return Panel::tr("Ponte");
    if (k == QLatin1String("village")) return Panel::tr("Vila");
    if (k == QLatin1String("fields")) return Panel::tr("Plantação");
    if (k == QLatin1String("wall")) return Panel::tr("Muralha");
    if (k == QLatin1String("road")) return Panel::tr("Estrada");
    if (k == QLatin1String("erase")) return Panel::tr("Apagar");
    return artName(k);
}

namespace {
QLabel* caption(QWidget* parent, const QString& text)
{
    auto* c = new QLabel(text, parent);
    c->setObjectName(QStringLiteral("cartoCaption"));
    return c;
}
QLabel* note(QWidget* parent)
{
    auto* n = new QLabel(parent);
    n->setObjectName(QStringLiteral("cartoNote"));
    n->setWordWrap(true);
    return n;
}
// rótulo + valor numa linha, e o slider embaixo
QSlider* sliderRow(QWidget* parent, QVBoxLayout* l, const QString& label, int lo, int hi, QLabel** value, QWidget** rowOut = nullptr)
{
    auto* box = new QWidget(parent);
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);
    auto* row = new QHBoxLayout();
    row->addWidget(caption(box, label));
    row->addStretch();
    *value = new QLabel(box);
    (*value)->setObjectName(QStringLiteral("cartoValue"));
    row->addWidget(*value);
    v->addLayout(row);
    auto* s = new QSlider(Qt::Horizontal, box);
    s->setRange(lo, hi);
    v->addWidget(s);
    l->addWidget(box);
    if (rowOut) *rowOut = box;
    return s;
}
}

QFrame* Panel::newCard(QLabel** title, const QString& text)
{
    auto* card = new QFrame(m_dock);
    card->setObjectName(QStringLiteral("cartoCard"));
    card->setFixedWidth(326);
    auto* l = new QVBoxLayout(card);
    l->setContentsMargins(14, 12, 14, 14);
    l->setSpacing(9);
    *title = new QLabel(text, card);
    (*title)->setObjectName(QStringLiteral("cartoCardTitle"));
    (*title)->setCursor(Qt::OpenHandCursor);
    (*title)->installEventFilter(this);
    l->addWidget(*title);
    card->hide();
    return card;
}

QToolButton* Panel::newChoice(QWidget* parent, const QString& iconName, const QString& text, const QString& tip)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(QStringLiteral("cartoChoice"));
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    b->setIconSize(QSize(18, 18));
    b->setText(text);
    b->setToolTip(tip);
    b->setMinimumWidth(48);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_icons.append({ b, iconName });
    return b;
}

QHBoxLayout* Panel::newSeg(QWidget* parent, const QStringList& values, const QStringList& labels, const QStringList& tips, QHash<QString, QToolButton*>& out,
                           const std::function<void(const QString&)>& onPick)
{
    auto* row = new QHBoxLayout();
    row->setSpacing(4);
    for (int i = 0; i < values.size(); i++) {
        auto* b = new QToolButton(parent);
        b->setObjectName(QStringLiteral("cartoSeg"));
        b->setText(labels.value(i));
        if (i < tips.size() && !tips[i].isEmpty()) b->setToolTip(tips[i]);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        const QString v = values[i];
        connect(b, &QToolButton::clicked, this, [onPick, v]() { onPick(v); });
        out.insert(v, b);
        row->addWidget(b);
    }
    return row;
}

// ── territórios ──
void Panel::buildRealmCard()
{
    m_realmCard = newCard(&m_realmTitle, tr("Territórios"));
    auto* l = static_cast<QVBoxLayout*>(m_realmCard->layout());
    auto* listBox = new QWidget(m_realmCard);
    m_realmList = new QVBoxLayout(listBox);
    m_realmList->setContentsMargins(0, 0, 0, 0);
    m_realmList->setSpacing(3);
    l->addWidget(listBox);
    auto* add = new QToolButton(m_realmCard);
    add->setObjectName(QStringLiteral("cartoLink"));
    add->setText(tr("+ Novo território"));
    add->setCursor(Qt::PointingHandCursor);
    connect(add, &QToolButton::clicked, this, [this]() {
        m_canvas->realmEdit([this](Info& in) {
            std::vector<bool> used(8, false);
            for (const RealmDef& d : in.defs) for (int c = 0; c < 8; c++) if (d.color == kRealmPalette[c]) used[c] = true;
            int ci = -1;
            for (int c = 0; c < 8; c++) if (!used[c]) { ci = c; break; }
            RealmDef d;
            d.name = tr("Território %1").arg(in.defs.size() + 1);
            d.color = kRealmPalette[ci >= 0 ? ci : static_cast<int>(in.defs.size() % 8)];
            d.reach = 120;
            in.defs.push_back(d);
            m_canvas->opts().realmSel = static_cast<int>(in.defs.size()) - 1;
        });
        m_canvas->opts().realmTool = QStringLiteral("seed");
        syncUi();
    });
    l->addWidget(add, 0, Qt::AlignLeft);
    l->addWidget(caption(m_realmCard, tr("FERRAMENTA")));
    l->addLayout(newSeg(m_realmCard, { QStringLiteral("seed"), QStringLiteral("paint"), QStringLiteral("neutral") }, { tr("Semear"), tr("Pintar"), tr("Neutro") },
                        { tr("Clique pra plantar o território; arraste a semente pra mover; Shift+clique apaga"),
                          tr("Pinte área pro território selecionado: ela cresce dali pelo terreno"), tr("Pinte terra sem dono (barreira)") },
                        m_realmToolBtns, [this](const QString& v) { m_canvas->opts().realmTool = v; m_canvas->optsChanged(); syncUi(); }));
    m_reachSlider = sliderRow(m_realmCard, l, tr("ALCANCE"), 20, 400, &m_reachValue);
    connect(m_reachSlider, &QSlider::valueChanged, this, [this](int v) { m_reachValue->setText(v >= 400 ? tr("sem limite") : QString::number(v)); });
    connect(m_reachSlider, &QSlider::sliderReleased, this, [this]() {
        const int v = m_reachSlider->value(), r = m_canvas->opts().realmSel;
        m_canvas->realmEdit([v, r](Info& in) { if (r >= 0 && r < static_cast<int>(in.defs.size())) in.defs[r].reach = v >= 400 ? kReachAll : v; });
    });
    auto* ends = new QHBoxLayout();
    auto* e0 = note(m_realmCard); e0->setText(tr("Pequeno"));
    auto* e1 = note(m_realmCard); e1->setText(tr("Sem limite")); e1->setAlignment(Qt::AlignRight);
    ends->addWidget(e0); ends->addWidget(e1);
    l->addLayout(ends);
    m_realmSize = sliderRow(m_realmCard, l, tr("PINCEL"), 6, 80, &m_realmSizeValue, &m_realmSizeRow);
    m_realmSize->setValue(22);
    m_realmSizeValue->setText(QStringLiteral("22"));
    connect(m_realmSize, &QSlider::valueChanged, this, [this](int v) { m_canvas->opts().realmBrush = v; m_realmSizeValue->setText(QString::number(v)); m_canvas->update(); });
    m_realmHint = note(m_realmCard);
    l->addWidget(m_realmHint);
}

void Panel::syncRealmCard()
{
    if (!m_realmCard || !m_realmCard->isVisible()) return;
    const ToolOpts& o = m_canvas->opts();
    for (auto it = m_realmToolBtns.begin(); it != m_realmToolBtns.end(); ++it) it.value()->setChecked(it.key() == o.realmTool);
    m_realmSizeRow->setVisible(o.realmTool != QLatin1String("seed"));
    m_realmHint->setText(o.realmTool == QLatin1String("paint") ? tr("Pinte área pro território selecionado. Ela vira semente: a borda continua seguindo serra, vale e costa.")
                         : o.realmTool == QLatin1String("neutral") ? tr("Pinte terra sem dono. Ela vira barreira: nenhum território passa por ali.")
                                                                    : tr("Clique no mapa pra plantar o território selecionado; ele cresce pelo terreno. Arraste uma semente pra mover; Shift+clique apaga."));
    const Info* in = m_doc.info.get();
    const int n = in ? static_cast<int>(in->defs.size()) : 0;
    const int sel = std::max(0, std::min(o.realmSel, n - 1));
    // a lista só se refaz quando os territórios mudam (senão o campo de nome perde o foco a cada quadro).
    // Trocar a seleção NÃO refaz: o clique no nome de outro território chega aqui no meio do próprio
    // evento do campo, e apagar o campo ali derrubava o app (21/10). Seleção = só o destaque das linhas.
    QString sigText = QString::number(n);
    if (in) for (const RealmDef& d : in->defs) sigText += QStringLiteral("|%1/%2,%3,%4").arg(d.name).arg(d.color[0]).arg(d.color[1]).arg(d.color[2]);
    const int sig = static_cast<int>(qHash(sigText));
    if (sig != m_realmListSig) {
        m_realmListSig = sig;
        // deleteLater: a lista também se refaz de dentro do editingFinished do próprio campo de nome
        while (QLayoutItem* it = m_realmList->takeAt(0)) { if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); } delete it; }
        const QColor muted = Theme::toColor(Theme::textMuted()), accent = Theme::toColor(Theme::accentDefault());
        for (int i = 0; i < n; i++) {
            const RealmDef& d = in->defs[i];
            auto* row = new QFrame(m_realmCard);
            row->setObjectName(QStringLiteral("cartoRow"));
            row->setProperty("sel", i == sel);
            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(6, 3, 3, 3);
            h->setSpacing(6);
            auto* sw = new QToolButton(row);
            sw->setObjectName(QStringLiteral("cartoSwatch"));
            sw->setFixedSize(18, 18);
            sw->setCursor(Qt::PointingHandCursor);
            sw->setToolTip(tr("Trocar a cor"));
            sw->setStyleSheet(QStringLiteral("QToolButton { background: rgb(%1,%2,%3); border: 1px solid rgba(0,0,0,0.35); border-radius: 9px; }")
                                  .arg(qRound(d.color[0])).arg(qRound(d.color[1])).arg(qRound(d.color[2])));
            connect(sw, &QToolButton::clicked, this, [this, i]() {
                m_canvas->opts().realmSel = i;
                m_canvas->realmEdit([i](Info& info) {
                    if (i >= static_cast<int>(info.defs.size())) return;
                    int k = 0;
                    for (int c = 0; c < 8; c++) if (kRealmPalette[c] == info.defs[i].color) { k = c + 1; break; }
                    info.defs[i].color = kRealmPalette[k % 8];
                });
            });
            auto* name = new QLineEdit(d.name, row);
            name->setObjectName(QStringLiteral("cartoRowName"));
            connect(name, &QLineEdit::editingFinished, this, [this, i, name]() {
                const QString v = name->text().trimmed();
                const Info* info = m_doc.info.get();
                if (!info || i >= static_cast<int>(info->defs.size()) || v.isEmpty() || v == info->defs[i].name) return;
                m_canvas->realmEdit([i, v](Info& inf) { inf.defs[i].name = v; });
            });
            name->installEventFilter(this);
            name->setProperty("realmRow", i);
            auto* rm = new QToolButton(row);
            rm->setObjectName(QStringLiteral("cartoBtn"));
            rm->setFixedSize(26, 26);
            rm->setToolTip(tr("Apagar território"));
            rm->setCursor(Qt::PointingHandCursor);
            rm->setIcon(icon(QStringLiteral("trash"), muted, accent));
            connect(rm, &QToolButton::clicked, this, [this, i]() {
                m_canvas->realmEdit([i](Info& info) { if (i < static_cast<int>(info.defs.size())) info.defs.erase(info.defs.begin() + i); });
            });
            h->addWidget(sw); h->addWidget(name, 1); h->addWidget(rm);
            row->installEventFilter(this);
            row->setProperty("realmRow", i);
            m_realmList->addWidget(row);
        }
        if (!n) {
            auto* empty = note(m_realmCard);
            empty->setText(tr("Nenhum território ainda. Clique no mapa pra plantar o primeiro."));
            m_realmList->addWidget(empty);
        }
    }
    // o destaque do selecionado
    for (int k = 0; k < m_realmList->count(); k++) {
        QWidget* row = m_realmList->itemAt(k)->widget();
        if (!row || !row->property("realmRow").isValid()) continue;
        const bool on = row->property("realmRow").toInt() == sel;
        if (row->property("sel").toBool() == on) continue;
        row->setProperty("sel", on);
        row->style()->unpolish(row); row->style()->polish(row);
    }
    const RealmDef* d = in && n ? &in->defs[sel] : nullptr;
    const int reach = !d || d->reach >= kReachAll ? 400 : qRound(d->reach);
    if (!m_reachSlider->isSliderDown()) {
        m_reachSlider->blockSignals(true);
        m_reachSlider->setValue(reach);
        m_reachSlider->blockSignals(false);
        m_reachValue->setText(reach >= 400 ? tr("sem limite") : QString::number(reach));
    }
    m_reachSlider->setEnabled(d != nullptr);
}

// ── construir ──
void Panel::buildBuildCard()
{
    m_buildCard = newCard(&m_buildTitle, tr("Construir"));
    auto* l = static_cast<QVBoxLayout*>(m_buildCard->layout());
    l->addWidget(caption(m_buildCard, tr("ESCALA DO MUNDO")));
    l->addLayout(newSeg(m_buildCard, { QStringLiteral("1"), QStringLiteral("0.7"), QStringLiteral("0.5") }, { tr("Normal"), tr("Menor"), tr("Mínima") },
                        { tr("Tudo que é construído ou plantado no tamanho de sempre"), tr("Construções e árvores menores: o mesmo continente parece um mundo maior"),
                          tr("Ainda menores: o continente vira um império") },
                        m_scaleBtns, [this](const QString& v) {
                            m_canvas->waitIdle();
                            m_doc.world.scale = v.toDouble();
                            m_doc.touch();               // floresta e cidades sugeridas se refazem na escala (territórios e construções ficam)
                            m_canvas->layersReplaced();
                            scheduleSave();
                            syncUi();
                        }));
    auto grid = [&](const QString& cap, const QList<QPair<QString, QString>>& items, const QStringList& tips) {
        l->addWidget(caption(m_buildCard, cap));
        auto* g = new QGridLayout();
        g->setHorizontalSpacing(4);
        g->setVerticalSpacing(4);
        for (int c = 0; c < 4; c++) g->setColumnStretch(c, 1);
        for (int i = 0; i < items.size(); i++) {
            const QString k = items[i].first;
            auto* b = newChoice(m_buildCard, items[i].second, buildKindName(k), tips.value(i));
            connect(b, &QToolButton::clicked, this, [this, k]() { m_canvas->opts().buildKind = k; m_canvas->optsChanged(); syncUi(); });
            m_buildBtns.insert(k, b);
            g->addWidget(b, i / 4, i % 4);
        }
        l->addLayout(g);
    };
    grid(tr("ÚNICOS"), { { "select", "cursor" }, { "castle", "castle" }, { "tower", "tower" }, { "lighthouse", "lighthouse" },
                         { "windmill", "windmill" }, { "temple", "temple" }, { "ruins", "ruins" }, { "bridge", "bridge" } },
         { tr("Selecione um artefato pra arrastar, girar, renomear ou apagar"), tr("Castelo: clique pra colocar"), tr("Torre de vigia"), tr("Farol (na costa fica bonito)"),
           tr("Moinho"), tr("Templo"), tr("Ruínas"), tr("Ponte: clique numa margem e depois na outra") });
    grid(tr("PINCÉIS"), { { "village", "house" }, { "fields", "field" }, { "wall", "wall" }, { "road", "road" }, { "erase", "erase" } },
         { tr("Pincel de vila: casas brotam em terra firme"), tr("Pincel de plantação"), tr("Muralha: desenhe o traço; ela sobe e desce com o chão"),
           tr("Estrada: ligue dois pontos (ela acha o caminho pelo vale) ou desenhe à mão"), tr("Apaga o que você construiu") });
    m_csizeRow = new QWidget(m_buildCard);
    auto* cs = new QVBoxLayout(m_csizeRow);
    cs->setContentsMargins(0, 0, 0, 0);
    cs->setSpacing(4);
    cs->addWidget(caption(m_csizeRow, tr("TAMANHO DO CASTELO")));
    cs->addLayout(newSeg(m_csizeRow, { QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3") }, { tr("Pequeno"), tr("Médio"), tr("Grande"), tr("Colossal") }, {},
                         m_csizeBtns, [this](const QString& v) {
                             const int size = v.toInt();
                             if (Art* a = m_canvas->selectedArt(); a && a->kind == QLatin1String("castle")) {
                                 const QString id = a->id;
                                 m_canvas->userEdit([id, size](Info& in) { for (Art& x : in.user.arts) if (x.id == id) { x.size = size; return true; } return false; });
                             } else m_canvas->opts().castleSize = size;
                             syncUi();
                         }));
    l->addWidget(m_csizeRow);
    m_roadRow = new QWidget(m_buildCard);
    auto* rr = new QVBoxLayout(m_roadRow);
    rr->setContentsMargins(0, 0, 0, 0);
    rr->setSpacing(6);
    rr->addWidget(caption(m_roadRow, tr("ESTRADA")));
    rr->addLayout(newSeg(m_roadRow, { QStringLiteral("route"), QStringLiteral("hand") }, { tr("Pelo vale"), tr("À mão") },
                         { tr("Clique no começo e no fim: ela acha o caminho pelo vale"), tr("Desenhe o traço") },
                         m_rmodeBtns, [this](const QString& v) { m_canvas->opts().roadMode = v; m_canvas->optsChanged(); syncUi(); }));
    rr->addLayout(newSeg(m_roadRow, { QStringLiteral("trail"), QStringLiteral("road"), QStringLiteral("royal") }, { tr("Trilha"), tr("Estrada"), tr("Real") },
                         { tr("Caminho de terra estreito"), tr("Estrada de terra"), tr("Estrada real, calçada de pedra, com lanternas à noite") },
                         m_rkindBtns, [this](const QString& v) { m_canvas->opts().roadKind = v; m_canvas->optsChanged(); syncUi(); }));
    l->addWidget(m_roadRow);
    m_buildSize = sliderRow(m_buildCard, l, tr("PINCEL"), 4, 60, &m_buildSizeValue, &m_buildSizeRow);
    m_buildSize->setValue(14);
    m_buildSizeValue->setText(QStringLiteral("14"));
    connect(m_buildSize, &QSlider::valueChanged, this, [this](int v) { m_canvas->opts().buildBrush = v; m_buildSizeValue->setText(QString::number(v)); m_canvas->update(); });
    // artefato selecionado: nome, girar, apagar
    m_selBox = new QFrame(m_buildCard);
    m_selBox->setObjectName(QStringLiteral("cartoSelBox"));
    auto* sb = new QVBoxLayout(m_selBox);
    sb->setContentsMargins(10, 8, 10, 10);
    sb->setSpacing(6);
    m_selKind = caption(m_selBox, QString());
    sb->addWidget(m_selKind);
    m_selName = new QLineEdit(m_selBox);
    m_selName->setObjectName(QStringLiteral("cartoField"));
    m_selName->setPlaceholderText(tr("Nome (opcional)"));
    connect(m_selName, &QLineEdit::editingFinished, this, [this]() {
        Art* a = m_canvas->selectedArt();
        if (!a) return;
        const QString v = m_selName->text().trimmed(), id = a->id;
        if (v == a->name) return;
        m_canvas->userEdit([id, v](Info& in) { for (Art& x : in.user.arts) if (x.id == id) { x.name = v; return true; } return false; });
    });
    sb->addWidget(m_selName);
    auto* sr = new QHBoxLayout();
    sr->setSpacing(4);
    const QColor muted = Theme::toColor(Theme::textMuted()), accent = Theme::toColor(Theme::accentDefault());
    auto* rl = new QToolButton(m_selBox);
    rl->setObjectName(QStringLiteral("cartoSeg"));
    rl->setToolTip(tr("Girar (Q)"));
    m_icons.append({ rl, QStringLiteral("rotl") });
    connect(rl, &QToolButton::clicked, this, [this]() { rotateSelected(-M_PI / 12); });
    auto* rrb = new QToolButton(m_selBox);
    rrb->setObjectName(QStringLiteral("cartoSeg"));
    rrb->setToolTip(tr("Girar (E)"));
    m_icons.append({ rrb, QStringLiteral("rotr") });
    connect(rrb, &QToolButton::clicked, this, [this]() { rotateSelected(M_PI / 12); });
    auto* del = new QToolButton(m_selBox);
    del->setObjectName(QStringLiteral("cartoSeg"));
    del->setText(tr("Apagar"));
    del->setToolTip(tr("Apagar (Delete)"));
    del->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(del, &QToolButton::clicked, this, &Panel::deleteSelected);
    for (QToolButton* b : { rl, rrb, del }) b->setCursor(Qt::PointingHandCursor);
    sr->addWidget(rl); sr->addWidget(rrb); sr->addWidget(del, 1);
    sb->addLayout(sr);
    (void)muted; (void)accent;
    l->addWidget(m_selBox);
    m_showGen = new QToolButton(m_buildCard);
    m_showGen->setObjectName(QStringLiteral("cartoCheck"));
    m_showGen->setCheckable(true);
    m_showGen->setCursor(Qt::PointingHandCursor);
    m_showGen->setText(tr("Mostrar cidades e estradas sugeridas pelo terreno"));
    m_showGen->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_showGen->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_showGen, &QToolButton::clicked, this, [this](bool on) {
        m_canvas->userEdit([on](Info& in) { in.user.showGen = on; return true; });
    });
    l->addWidget(m_showGen);
    m_buildHint = note(m_buildCard);
    l->addWidget(m_buildHint);
}

void Panel::syncBuildCard()
{
    if (!m_buildCard || !m_buildCard->isVisible()) return;
    const ToolOpts& o = m_canvas->opts();
    for (auto it = m_buildBtns.begin(); it != m_buildBtns.end(); ++it) it.value()->setChecked(it.key() == o.buildKind);
    const double sc = worldScale(m_doc.world);
    for (auto it = m_scaleBtns.begin(); it != m_scaleBtns.end(); ++it) it.value()->setChecked(std::fabs(it.key().toDouble() - sc) < 1e-6);
    const Art* sel = m_canvas->selectedArt();
    const bool selCastle = sel && sel->kind == QLatin1String("castle");
    m_csizeRow->setVisible(o.buildKind == QLatin1String("castle") || selCastle);
    const int cs = selCastle ? sel->size : o.castleSize;
    for (auto it = m_csizeBtns.begin(); it != m_csizeBtns.end(); ++it) it.value()->setChecked(it.key().toInt() == cs);
    m_roadRow->setVisible(o.buildKind == QLatin1String("road"));
    for (auto it = m_rmodeBtns.begin(); it != m_rmodeBtns.end(); ++it) it.value()->setChecked(it.key() == o.roadMode);
    for (auto it = m_rkindBtns.begin(); it != m_rkindBtns.end(); ++it) it.value()->setChecked(it.key() == o.roadKind);
    m_buildSizeRow->setVisible(o.buildKind == QLatin1String("village") || o.buildKind == QLatin1String("fields") || o.buildKind == QLatin1String("erase"));
    m_selBox->setVisible(sel != nullptr);
    if (sel) {
        m_selKind->setText(artName(sel->kind).toUpper());
        if (!m_selName->hasFocus()) m_selName->setText(sel->name);
    }
    m_showGen->setChecked(m_doc.info ? m_doc.info->user.showGen : true);
    m_showGen->setIcon(icon(m_showGen->isChecked() ? QStringLiteral("check") : QStringLiteral("box"),
                            Theme::toColor(Theme::textMuted()), Theme::toColor(Theme::accentDefault())));
    QString hint;
    const QString& k = o.buildKind;
    if (!m_canvas->toolMessage().isEmpty()) hint = m_canvas->toolMessage();
    else if (k == QLatin1String("castle")) hint = tr("Clique pra colocar um castelo; a bandeira pega a cor do território. Clique num artefato pra selecionar e arrastar.");
    else if (k == QLatin1String("tower")) hint = tr("Clique pra colocar uma torre de vigia.");
    else if (k == QLatin1String("lighthouse")) hint = tr("Clique pra colocar um farol (na costa fica bonito).");
    else if (k == QLatin1String("windmill")) hint = tr("Clique pra colocar um moinho. Q/E giram as pás pra outro lado.");
    else if (k == QLatin1String("temple")) hint = tr("Clique pra colocar um templo.");
    else if (k == QLatin1String("ruins")) hint = tr("Clique pra colocar ruínas.");
    else if (k == QLatin1String("bridge")) hint = tr("Clique numa margem e depois na outra.");
    else if (k == QLatin1String("select")) hint = tr("Clique num artefato pra selecionar; arraste pra mover. Q/E giram, Delete apaga.");
    else if (k == QLatin1String("village")) hint = tr("Pinte: as casas brotam em terra firme e pouco inclinada.");
    else if (k == QLatin1String("fields")) hint = tr("Pinte: retalhos de plantação com sebe.");
    else if (k == QLatin1String("wall")) hint = tr("Desenhe o traço da muralha; ela sobe e desce com o chão, com torres no caminho.");
    else if (k == QLatin1String("erase")) hint = tr("Pinte por cima do que você construiu pra apagar (estradas também).");
    else if (k == QLatin1String("road")) hint = o.roadMode == QLatin1String("hand") ? tr("Desenhe o traço da estrada; onde ela passar na água vira ponte.")
                                                                                  : tr("Clique no começo e no fim: a estrada acha o caminho pelo vale e cruza rio estreito com ponte. Ela continua do último ponto; Esc termina.");
    m_buildHint->setText(hint);
}

// ── nomes ──
void Panel::buildNamesCard()
{
    m_namesCard = newCard(&m_namesTitle, tr("Nomes"));
    auto* l = static_cast<QVBoxLayout*>(m_namesCard->layout());
    m_namesAuto = new QToolButton(m_namesCard);
    m_namesAuto->setObjectName(QStringLiteral("cartoCheck"));
    m_namesAuto->setCheckable(true);
    m_namesAuto->setChecked(true);
    m_namesAuto->setCursor(Qt::PointingHandCursor);
    m_namesAuto->setText(tr("Sugerir nomes pelo relevo"));
    m_namesAuto->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_namesAuto->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_namesAuto, &QToolButton::clicked, this, [this](bool on) { m_doc.labels.autoOn = on; m_canvas->update(); syncUi(); scheduleSave(); });
    l->addWidget(m_namesAuto);
    l->addWidget(caption(m_namesCard, tr("QUANTIDADE")));
    l->addLayout(newSeg(m_namesCard, { QStringLiteral("few"), QStringLiteral("normal"), QStringLiteral("many") }, { tr("Poucos"), tr("Normal"), tr("Muitos") },
                        { tr("Só os grandes: oceano, mares, serras e as maiores regiões"), QString(), tr("Também picos, campos, lagos e ilhas pequenas") },
                        m_densBtns, [this](const QString& v) { m_doc.labels.density = v; m_canvas->update(); syncUi(); scheduleSave(); }));
    l->addWidget(caption(m_namesCard, tr("NOME NOVO")));
    l->addLayout(newSeg(m_namesCard, { QStringLiteral("land"), QStringLiteral("water") }, { tr("Terra"), tr("Água") },
                        { tr("Letras de terra: versalete espaçado"), tr("Letras de água: itálico") },
                        m_nstyleBtns, [this](const QString& v) { m_canvas->opts().nameStyle = v; syncUi(); }));
    m_nameBox = new QFrame(m_namesCard);
    m_nameBox->setObjectName(QStringLiteral("cartoSelBox"));
    auto* nb = new QVBoxLayout(m_nameBox);
    nb->setContentsMargins(10, 8, 10, 10);
    nb->setSpacing(6);
    m_nameKind = caption(m_nameBox, QString());
    nb->addWidget(m_nameKind);
    m_nameEdit = new QLineEdit(m_nameBox);
    m_nameEdit->setObjectName(QStringLiteral("cartoField"));
    connect(m_nameEdit, &QLineEdit::editingFinished, this, [this]() {
        const Label* lab = m_canvas->scene().nameByRef(m_canvas->selName());
        if (!lab) return;
        Label copy = *lab;
        const QString v = m_nameEdit->text().trimmed();
        if (v == copy.text) return;
        m_canvas->nameEdit([&](LabelLayer& L) {
            if (copy.autoName) {
                const int e = editFor(L, m_doc.names, copy);
                if (e < 0) return false;
                const QString sugg = copy.feature >= 0 && copy.feature < static_cast<int>(m_doc.names.size()) ? m_doc.names[copy.feature].name : QString();
                L.edits[e].name = (!v.isEmpty() && v != sugg) ? v : QString();
                return true;
            }
            if (v.isEmpty() || copy.custom < 0 || copy.custom >= static_cast<int>(L.custom.size())) return false;
            L.custom[copy.custom].text = v;
            return true;
        });
    });
    nb->addWidget(m_nameEdit);
    auto* sl = new QVBoxLayout();
    m_nameSize = sliderRow(m_nameBox, sl, tr("TAMANHO"), 50, 220, &m_nameSizeValue);
    m_nameSize->setSingleStep(5);
    nb->addLayout(sl);
    connect(m_nameSize, &QSlider::sliderPressed, this, [this]() { m_nameSizeBefore = std::make_shared<Doc::Layers>(m_doc.layers()); });
    connect(m_nameSize, &QSlider::valueChanged, this, [this](int v) {
        m_nameSizeValue->setText(QStringLiteral("%1%").arg(v));
        const Label* lab = m_canvas->scene().nameByRef(m_canvas->selName());
        if (!lab) return;
        Label copy = *lab;
        LabelLayer& L = m_doc.labels;
        if (copy.autoName) { const int e = editFor(L, m_doc.names, copy); if (e >= 0) L.edits[e].size = v / 100.0; }
        else if (copy.custom >= 0 && copy.custom < static_cast<int>(L.custom.size())) L.custom[copy.custom].size = v / 100.0;
        m_canvas->update();
    });
    connect(m_nameSize, &QSlider::sliderReleased, this, [this]() {
        if (m_nameSizeBefore) emit m_canvas->layersEdited(std::move(m_nameSizeBefore));
        m_nameSizeBefore.reset();
    });
    auto* br = new QHBoxLayout();
    br->setSpacing(4);
    m_nameReset = new QToolButton(m_nameBox);
    m_nameReset->setObjectName(QStringLiteral("cartoSeg"));
    m_nameReset->setText(tr("Sugestão"));
    m_nameReset->setToolTip(tr("Volta pro nome e o lugar que o mapa sugeriu"));
    m_nameReset->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_nameReset, &QToolButton::clicked, this, [this]() {
        const Label* lab = m_canvas->scene().nameByRef(m_canvas->selName());
        if (!lab || !lab->autoName || lab->edit < 0) return;
        const int e = lab->edit;
        m_canvas->nameEdit([e](LabelLayer& L) { if (e >= static_cast<int>(L.edits.size())) return false; L.edits.erase(L.edits.begin() + e); return true; });
    });
    m_nameDel = new QToolButton(m_nameBox);
    m_nameDel->setObjectName(QStringLiteral("cartoSeg"));
    m_nameDel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_nameDel, &QToolButton::clicked, this, &Panel::deleteSelected);
    for (QToolButton* b : { m_nameReset, m_nameDel }) b->setCursor(Qt::PointingHandCursor);
    br->addWidget(m_nameReset); br->addWidget(m_nameDel);
    nb->addLayout(br);
    l->addWidget(m_nameBox);
    m_namesHint = note(m_namesCard);
    l->addWidget(m_namesHint);
}

void Panel::syncNamesCard()
{
    if (!m_namesCard || !m_namesCard->isVisible()) return;
    const ToolOpts& o = m_canvas->opts();
    m_namesAuto->setChecked(m_doc.labels.autoOn);
    m_namesAuto->setIcon(icon(m_doc.labels.autoOn ? QStringLiteral("check") : QStringLiteral("box"), Theme::toColor(Theme::textMuted()), Theme::toColor(Theme::accentDefault())));
    for (auto it = m_densBtns.begin(); it != m_densBtns.end(); ++it) it.value()->setChecked(it.key() == m_doc.labels.density);
    for (auto it = m_nstyleBtns.begin(); it != m_nstyleBtns.end(); ++it) it.value()->setChecked(it.key() == o.nameStyle);
    const Label* lab = m_canvas->selName().isEmpty() ? nullptr : m_canvas->scene().nameByRef(m_canvas->selName());
    m_nameBox->setVisible(lab != nullptr);
    if (!lab) {
        m_namesHint->setText(tr("Clique num nome pra editar. Clique no mapa pra pôr um nome novo; arrastando enquanto clica, ele segue o traço (rio, estrada, serra)."));
        return;
    }
    m_nameKind->setText(nameKindLabel(lab->kind).toUpper());
    if (!m_nameEdit->hasFocus()) m_nameEdit->setText(lab->text);
    if (!m_nameSize->isSliderDown()) {
        m_nameSize->blockSignals(true);
        m_nameSize->setValue(qRound(lab->size * 100));
        m_nameSize->blockSignals(false);
        m_nameSizeValue->setText(QStringLiteral("%1%").arg(qRound(lab->size * 100)));
    }
    m_nameReset->setVisible(lab->autoName);
    m_nameDel->setText(lab->autoName ? (lab->hidden ? tr("Mostrar") : tr("Esconder")) : tr("Apagar"));
    m_nameDel->setToolTip(lab->autoName ? tr("Esconder (Delete)") : tr("Apagar (Delete)"));
    m_namesHint->setText(lab->autoName ? tr("Sugestão do mapa: troque o nome ou arraste pra mover. \"Sugestão\" desfaz o que você mudou nele.")
                                       : tr("Nome seu: arraste pra mover; Delete apaga."));
    if (m_canvas->consumeNewName()) { m_nameEdit->setText(lab->text); m_nameEdit->setFocus(); m_nameEdit->selectAll(); }
}

} // namespace carto
