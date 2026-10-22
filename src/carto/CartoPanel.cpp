#include "CartoPanel.h"
#include "CartoProf.h"

#include "CartoAstrolabe.h"
#include "CartoCartela.h"
#include "CartoExplore.h"
#include "CartoIcons.h"
#include "CartoRing.h"
#include "../SheetDialogs.h"
#include "../Theme.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QThreadPool>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace carto {

namespace {
constexpr double DEG = M_PI / 180;
QString modeName(const QString& m)
{
    if (m == QLatin1String("full")) return Panel::tr("Completo");
    if (m == QLatin1String("atlas")) return Panel::tr("Atlas");
    if (m == QLatin1String("ink")) return Panel::tr("Tinta");
    return Panel::tr("Terreno");
}
QString sizeName(int s)
{
    switch (s) {
    case 0: return Panel::tr("Ilha");
    case 1: return Panel::tr("Região");
    case 2: return Panel::tr("Mundo");
    case 3: return Panel::tr("Épico");
    case 4: return Panel::tr("Continente");
    default: return Panel::tr("Supercontinente");
    }
}
QString brushName(Brush b)
{
    switch (b) {
    case Brush::Land: return Panel::tr("Terra");
    case Brush::Sea: return Panel::tr("Mar");
    case Brush::Mountain: return Panel::tr("Montanha");
    case Brush::Smooth: return Panel::tr("Suavizar");
    case Brush::River: return Panel::tr("Rio");
    case Brush::Wet: return Panel::tr("Úmido");
    case Brush::Dry: return Panel::tr("Seco");
    default: return Panel::tr("Frio");
    }
}
bool isClimateBrush(Brush b) { return b == Brush::Wet || b == Brush::Dry || b == Brush::Cold; }
const char* kBrushIcon[8] = { "land", "sea", "mountain", "smooth", "river", "wet", "dry", "cold" };
QString fmtHour(double h)
{
    const int m = qRound(std::fmod(std::fmod(h, 24.0) + 24, 24.0) * 60) % 1440;
    return QStringLiteral("%1:%2").arg(m / 60, 2, 10, QLatin1Char('0')).arg(m % 60, 2, 10, QLatin1Char('0'));
}
QString phaseName(double h)
{
    switch (hourPhase(h)) {
    case 0: return Panel::tr("Noite");
    case 1: return Panel::tr("Amanhecer");
    case 2: return Panel::tr("Manhã");
    case 3: return Panel::tr("Meio-dia");
    case 4: return Panel::tr("Tarde");
    case 5: return Panel::tr("Entardecer");
    default: return Panel::tr("Anoitecer");
    }
}
}

Panel::Panel(QWidget* parent) : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("cartoPanel"));
    setWindowTitle(tr("Mapa"));
    setMinimumSize(820, 560);
    resize(1280, 820);
    m_brushSizes = { { int(Brush::River), 10.0 } };
    m_savePool = new QThreadPool(this);
    m_savePool->setMaxThreadCount(1);
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(1500);
    connect(m_saveTimer, &QTimer::timeout, this, &Panel::saveNow);
    m_playTimer = new QTimer(this);
    m_playTimer->setInterval(16);
    connect(m_playTimer, &QTimer::timeout, this, [this]() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const double dt = std::min(0.1, (now - m_playLast) / 1000.0);
        m_playLast = now;
        m_canvas->setHour(m_canvas->hour() + dt * 24 / 48, false);   // um dia inteiro em 48 s
    });
    buildUi();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, &Panel::applyTheme);
}

Panel::~Panel()
{
    if (m_saveTimer->isActive()) saveNow();
    flushSaves();
}

void Panel::flushSaves() { if (m_savePool) m_savePool->waitForDone(); }

// ── montagem ──
void Panel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    m_canvas = new Canvas(this);
    root->addWidget(m_canvas);

    auto makeFloat = [this](const QString& name) {
        auto* f = new QFrame(this);
        f->setObjectName(name);
        f->setProperty("cartoFloat", true);
        f->setAttribute(Qt::WA_StyledBackground, true);
        return f;
    };
    auto makeBtn = [this](QWidget* parent, const QString& iconName, const QString& tip, const QString& text = QString()) {
        auto* b = new QToolButton(parent);
        b->setObjectName(QStringLiteral("cartoBtn"));
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setIconSize(QSize(18, 18));
        b->setFixedHeight(30);
        if (!text.isEmpty()) { b->setText(text); b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); }
        else b->setFixedWidth(30);
        m_icons.append({ b, iconName });
        return b;
    };
    auto addSep = [](QFrame* f) {
        auto* sep = new QFrame(f);
        sep->setObjectName(QStringLiteral("cartoSep"));
        sep->setFixedSize(1, 18);
        static_cast<QHBoxLayout*>(f->layout())->addWidget(sep, 0, Qt::AlignVCenter);
    };

    // título do mapa (clique pra trocar o nome) + troca de mapa
    m_titlePill = makeFloat(QStringLiteral("cartoTitle"));
    auto* tl = new QHBoxLayout(m_titlePill);
    tl->setContentsMargins(14, 6, 6, 6);
    tl->setSpacing(6);
    auto* tcol = new QVBoxLayout();
    tcol->setSpacing(1);
    m_title = new QLabel(m_titlePill);
    m_title->setObjectName(QStringLiteral("cartoTitleText"));
    m_title->setCursor(Qt::IBeamCursor);
    m_title->setToolTip(tr("Clique pra trocar o nome do mapa"));
    m_title->installEventFilter(this);
    m_titleEdit = new QLineEdit(m_titlePill);
    m_titleEdit->setObjectName(QStringLiteral("cartoTitleEdit"));
    m_titleEdit->hide();
    connect(m_titleEdit, &QLineEdit::editingFinished, this, [this]() {
        if (m_titleEdit->isHidden()) return;
        const QString v = m_titleEdit->text().trimmed();
        m_titleEdit->hide(); m_title->show();
        if (!v.isEmpty() && !m_mapId.isEmpty()) m_store.renameMap(m_mapId, v);
        syncUi();
    });
    m_sub = new QLabel(m_titlePill);
    m_sub->setObjectName(QStringLiteral("cartoSubText"));
    tcol->addWidget(m_title);
    tcol->addWidget(m_titleEdit);
    tcol->addWidget(m_sub);
    tl->addLayout(tcol);
    m_mapsBtn = makeBtn(m_titlePill, QStringLiteral("down"), tr("Trocar, criar, renomear ou excluir mapas"));
    connect(m_mapsBtn, &QToolButton::clicked, this, &Panel::showMapMenu);
    tl->addWidget(m_mapsBtn, 0, Qt::AlignTop);

    // pílula de cima: desfazer, refazer, gerar, enquadrar, ajustes, fechar
    m_topPill = makeFloat(QStringLiteral("cartoTop"));
    auto* tp = new QHBoxLayout(m_topPill);
    tp->setContentsMargins(4, 4, 4, 4);
    tp->setSpacing(2);
    // vistas (no modo Cartela; no Astrolábio elas moram no instrumento)
    m_viewSeg = new QWidget(m_topPill);
    auto* vs = new QHBoxLayout(m_viewSeg);
    vs->setContentsMargins(0, 0, 0, 0);
    vs->setSpacing(2);
    const QStringList vids = { QStringLiteral("full"), QStringLiteral("atlas"), QStringLiteral("ink"), QStringLiteral("terrain") };
    for (const QString& v : vids) {
        auto* b = new QToolButton(m_viewSeg);
        b->setObjectName(QStringLiteral("cartoBtn"));
        b->setCheckable(true);
        b->setFixedHeight(30);
        b->setCursor(Qt::PointingHandCursor);
        b->setText(modeName(v));
        connect(b, &QToolButton::clicked, this, [this, v]() { setMode(v); });
        m_viewBtns.insert(v, b);
        vs->addWidget(b);
    }
    auto* vsep = new QFrame(m_viewSeg);
    vsep->setObjectName(QStringLiteral("cartoSep"));
    vsep->setFixedSize(1, 18);
    vs->addWidget(vsep, 0, Qt::AlignVCenter);
    tp->addWidget(m_viewSeg);
    m_undoBtn = makeBtn(m_topPill, QStringLiteral("undo"), tr("Desfazer (Ctrl+Z)"));
    m_redoBtn = makeBtn(m_topPill, QStringLiteral("redo"), tr("Refazer (Ctrl+Y)"));
    connect(m_undoBtn, &QToolButton::clicked, this, [this]() { restore(true); });
    connect(m_redoBtn, &QToolButton::clicked, this, [this]() { restore(false); });
    tp->addWidget(m_undoBtn); tp->addWidget(m_redoBtn); addSep(m_topPill);
    m_genBtn = makeBtn(m_topPill, QStringLiteral("gen"), tr("Gerar mapa (G)"), tr("Gerar"));
    m_genBtn->setCheckable(true);
    connect(m_genBtn, &QToolButton::clicked, this, [this]() { m_gen->setVisible(!m_gen->isVisible()); positionOverlays(); syncUi(); });
    m_fitBtn = makeBtn(m_topPill, QStringLiteral("fit"), tr("Enquadrar (F)"));
    connect(m_fitBtn, &QToolButton::clicked, m_canvas, &Canvas::fit);
    m_setBtn = makeBtn(m_topPill, QStringLiteral("gear"), tr("Ajustes"));
    m_setBtn->setCheckable(true);
    connect(m_setBtn, &QToolButton::clicked, this, [this]() { m_settings->setVisible(!m_settings->isVisible()); positionOverlays(); syncUi(); });
    m_rulerBtn = makeBtn(m_topPill, QStringLiteral("ruler"), tr("Régua: distância e tempo de viagem (R)"));
    m_rulerBtn->setCheckable(true);
    connect(m_rulerBtn, &QToolButton::clicked, this, [this]() { pickTool(m_tool == QLatin1String("ruler") ? QStringLiteral("pan") : QStringLiteral("ruler"), QPoint()); });
    m_exportBtn = makeBtn(m_topPill, QStringLiteral("image"), tr("Exportar imagem (PNG pro livro)"));
    m_exportBtn->setCheckable(true);
    connect(m_exportBtn, &QToolButton::clicked, this, [this]() { m_export->setVisible(!m_export->isVisible()); m_gen->hide(); positionOverlays(); syncUi(); });
    tp->addWidget(m_genBtn); tp->addWidget(m_fitBtn); tp->addWidget(m_rulerBtn); tp->addWidget(m_exportBtn); tp->addWidget(m_setBtn); addSep(m_topPill);
    m_closeBtn = makeBtn(m_topPill, QStringLiteral("close"), tr("Fechar o mapa"));
    connect(m_closeBtn, &QToolButton::clicked, this, &Panel::close);
    tp->addWidget(m_closeBtn);

    // Astrolábio + pílulas da hora e da ferramenta
    m_astro = new Astrolabe(this);
    connect(m_astro, &Astrolabe::modeRequested, this, &Panel::setMode);
    connect(m_astro, &Astrolabe::angleDragged, this, [this](double a, bool quick) { m_canvas->setView(a, m_canvas->pitch(), quick); });
    connect(m_astro, &Astrolabe::pitchDragged, this, [this](double p, bool quick) { m_canvas->setView(m_canvas->angle(), p, quick); });
    connect(m_astro, &Astrolabe::hourDragged, this, [this](double h, bool fin) { m_canvas->setHour(h, fin); if (fin) scheduleSave(); });
    connect(m_astro, &Astrolabe::northRequested, this, [this]() { m_canvas->setView(0, m_canvas->pitch(), false); });

    m_hourPill = makeFloat(QStringLiteral("cartoHour"));
    auto* hp = new QHBoxLayout(m_hourPill);
    hp->setContentsMargins(4, 4, 10, 4);
    hp->setSpacing(4);
    m_playBtn = makeBtn(m_hourPill, QStringLiteral("play"), tr("Passar o dia (P)"));
    connect(m_playBtn, &QToolButton::clicked, this, [this]() { setPlaying(!m_playTimer->isActive()); });
    m_hourLabel = new QLabel(m_hourPill);
    m_hourLabel->setObjectName(QStringLiteral("cartoHourText"));
    hp->addWidget(m_playBtn); hp->addWidget(m_hourLabel);

    m_toolPill = makeFloat(QStringLiteral("cartoTool"));
    auto* tpl = new QHBoxLayout(m_toolPill);
    tpl->setContentsMargins(4, 4, 4, 4);
    m_toolBtn = makeBtn(m_toolPill, QStringLiteral("ring"), tr("Ferramentas (ou botão direito no mapa)"), QStringLiteral(" "));
    m_toolBtn->setObjectName(QStringLiteral("cartoToolBtn"));
    connect(m_toolBtn, &QToolButton::clicked, this, [this]() {
        const QPoint c = m_toolPill->mapTo(this, QPoint(m_toolPill->width() / 2, 0));
        openRingAt(QPoint(c.x(), c.y() - 140));
    });
    tpl->addWidget(m_toolBtn);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("cartoHint"));
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setWordWrap(false);
    m_hint->setAttribute(Qt::WA_TransparentForMouseEvents);

    // doca das opções da ferramenta
    m_dock = new QFrame(this);
    m_dock->setObjectName(QStringLiteral("cartoDock"));
    auto* dl = new QVBoxLayout(m_dock);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->setSpacing(8);
    buildBrushCard();
    buildRealmCard();
    buildBuildCard();
    buildNamesCard();
    dl->addWidget(m_brushCard);
    dl->addWidget(m_legend);
    dl->addWidget(m_realmCard);
    dl->addWidget(m_buildCard);
    dl->addWidget(m_namesCard);
    m_dock->hide();

    buildGenPanel();
    buildSettings();
    buildWelcome();
    buildCartelaUi();
    buildExportCard();

    m_ring = new Ring(this);
    m_ring->setItems({
        { QStringLiteral("pan"), tr("Mover"), QStringLiteral("hand"), true },
        { QStringLiteral("gen"), tr("Gerar"), QStringLiteral("gen"), true },
        { QStringLiteral("area"), tr("Área"), QStringLiteral("area"), true },
        { QStringLiteral("brush"), tr("Pincel"), QStringLiteral("brush"), true },
        { QStringLiteral("climate"), tr("Clima"), QStringLiteral("clima"), true },
        { QStringLiteral("realm"), tr("Territórios"), QStringLiteral("flag"), true },
        { QStringLiteral("build"), tr("Construir"), QStringLiteral("castle"), true },
        { QStringLiteral("names"), tr("Nomes"), QStringLiteral("names"), true },
        { QStringLiteral("explore"), tr("Explorar"), QStringLiteral("explore"), true },
    });
    connect(m_ring, &Ring::picked, this, [this](const QString& id, QPoint c) { pickTool(id, c); });

    // Explorar: os documentos de cada território
    m_explore = new Explorer(this, m_canvas, &m_doc);
    m_explore->mapEdited = [this]() { scheduleSave(); };
    m_explore->flash = [this](const QString& m) { flash(m); };
    m_explore->cartelaTop = [this]() { return m_cartela && m_cartela->isVisible() ? m_cartela->y() : -1; };
    connect(m_canvas, &Canvas::exploreHover, this, [this](int r, const QString& link, QPoint at) { m_explore->hovered(r, link, at); });
    connect(m_canvas, &Canvas::exploreClicked, this, [this](int r, const QString& link) { m_explore->clicked(r, link); syncUi(); });

    // sinais da mesa
    connect(m_canvas, &Canvas::ringRequested, this, [this](QPoint at) { openRingAt(m_canvas->mapTo(this, at)); });
    connect(m_canvas, &Canvas::viewChanged, this, [this]() { syncUi(); scheduleSave(); });
    connect(m_canvas, &Canvas::hourChanged, this, [this](double) { syncUi(); });
    connect(m_canvas, &Canvas::statusChanged, this, &Panel::syncUi);
    connect(m_canvas, &Canvas::strokeFinished, this, [this](Snapshot before, Snapshot after, int riverId) {
        Undo u; u.before = std::move(before); u.after = std::move(after);
        if (riverId) { u.riverId = riverId; for (const River& r : m_doc.rivers) if (r.id == riverId) u.river = r; }
        pushUndo(std::move(u));
        scheduleSave();
    });
    connect(m_canvas, &Canvas::areaAdded, this, [this](std::shared_ptr<World> before) {
        Undo u; u.kind = Undo::Full; u.world = std::move(before);
        pushUndo(std::move(u));
        scheduleSave();
    });
    connect(m_canvas, &Canvas::layersEdited, this, [this](std::shared_ptr<Doc::Layers> before) {
        Undo u; u.kind = Undo::Layers; u.lBefore = std::move(before); u.lAfter = std::make_shared<Doc::Layers>(m_doc.layers());
        pushUndo(std::move(u));
        scheduleSave();
    });
    connect(m_canvas, &Canvas::selectionChanged, this, &Panel::syncUi);
    m_dock->raise();
    applyUiMode();
}

void Panel::buildBrushCard()
{
    m_brushCard = new QFrame(m_dock);
    m_brushCard->setObjectName(QStringLiteral("cartoCard"));
    m_brushCard->setFixedWidth(326);
    auto* l = new QVBoxLayout(m_brushCard);
    l->setContentsMargins(14, 12, 14, 14);
    l->setSpacing(10);
    m_brushTitle = new QLabel(tr("Pincel"), m_brushCard);
    m_brushTitle->setObjectName(QStringLiteral("cartoCardTitle"));
    m_brushTitle->setCursor(Qt::OpenHandCursor);
    m_brushTitle->installEventFilter(this);
    l->addWidget(m_brushTitle);
    auto group = [&](const QString& title, std::initializer_list<Brush> list) {
        auto* cap = new QLabel(title, m_brushCard);
        cap->setObjectName(QStringLiteral("cartoCaption"));
        l->addWidget(cap);
        auto* row = new QGridLayout();
        row->setHorizontalSpacing(4);
        for (int c = 0; c < 5; c++) row->setColumnStretch(c, 1);
        int col = 0;
        for (Brush b : list) {
            auto* btn = new QToolButton(m_brushCard);
            btn->setObjectName(QStringLiteral("cartoChoice"));
            btn->setCheckable(true);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            btn->setIconSize(QSize(18, 18));
            btn->setText(brushName(b));
            btn->setMinimumWidth(48);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            const QString keyTip = QStringLiteral(" (%1)").arg(int(b) + 1);
            btn->setToolTip(brushName(b) + keyTip);
            m_icons.append({ btn, QString::fromLatin1(kBrushIcon[int(b)]) });
            connect(btn, &QToolButton::clicked, this, [this, b]() { setBrush(b); });
            m_brushBtns.insert(int(b), btn);
            row->addWidget(btn, 0, col++);
        }
        l->addLayout(row);
    };
    group(tr("RELEVO"), { Brush::Land, Brush::Sea, Brush::Mountain, Brush::Smooth, Brush::River });
    group(tr("CLIMA"), { Brush::Wet, Brush::Dry, Brush::Cold });
    auto slider = [&](const QString& label, int lo, int hi, QSlider** s, QLabel** v) {
        auto* row = new QHBoxLayout();
        auto* cap = new QLabel(label, m_brushCard);
        cap->setObjectName(QStringLiteral("cartoCaption"));
        *v = new QLabel(m_brushCard);
        (*v)->setObjectName(QStringLiteral("cartoValue"));
        row->addWidget(cap); row->addStretch(); row->addWidget(*v);
        l->addLayout(row);
        *s = new QSlider(Qt::Horizontal, m_brushCard);
        (*s)->setRange(lo, hi);
        l->addWidget(*s);
    };
    slider(tr("TAMANHO"), 4, 160, &m_sizeSlider, &m_sizeValue);
    slider(tr("FORÇA"), 5, 100, &m_strSlider, &m_strValue);
    m_strSlider->setValue(50);
    connect(m_sizeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_brushSizes[int(m_brush)] = v;
        m_canvas->setBrushSize(v);
        m_sizeValue->setText(QString::number(v));
    });
    connect(m_strSlider, &QSlider::valueChanged, this, [this](int v) {
        m_strength = v / 100.0;
        m_canvas->setBrushStrength(m_strength);
        m_strValue->setText(QStringLiteral("%1%").arg(v));
    });
    m_strValue->setText(QStringLiteral("50%"));
    auto* tip = new QLabel(tr("Shift inverte o pincel (Terra vira Mar, Úmido vira Seco). [ e ] mudam o tamanho."), m_brushCard);
    tip->setObjectName(QStringLiteral("cartoNote"));
    tip->setWordWrap(true);
    l->addWidget(tip);

    // legenda do clima (aparece com a camada ligada)
    m_legend = new QFrame(m_dock);
    m_legend->setObjectName(QStringLiteral("cartoCard"));
    m_legend->setFixedWidth(326);
    auto* ll = new QVBoxLayout(m_legend);
    ll->setContentsMargins(14, 10, 14, 12);
    ll->setSpacing(5);
    auto* lt = new QLabel(tr("Mapa de clima"), m_legend);
    lt->setObjectName(QStringLiteral("cartoCardTitle"));
    ll->addWidget(lt);
    const QString names[4] = { tr("Gélido"), tr("Úmido"), tr("Temperado"), tr("Árido") };
    for (int i = 0; i < 4; i++) {
        auto* row = new QHBoxLayout();
        auto* sw = new QLabel(m_legend);
        sw->setFixedSize(16, 11);
        const RGB& c = kClimate[i].c;
        sw->setStyleSheet(QStringLiteral("background: rgb(%1,%2,%3); border: 1px solid rgba(0,0,0,0.35); border-radius: 3px;").arg(c[0]).arg(c[1]).arg(c[2]));
        auto* nm = new QLabel(names[i], m_legend);
        nm->setObjectName(QStringLiteral("cartoValue"));
        row->addWidget(sw); row->addWidget(nm); row->addStretch();
        ll->addLayout(row);
    }
    auto* lp = new QLabel(tr("É o que a terra vira em cada lugar. Pinte com Úmido, Seco e Frio, até no mar. Terra bem baixa e úmida em clima quente vira pântano."), m_legend);
    lp->setObjectName(QStringLiteral("cartoNote"));
    lp->setWordWrap(true);
    ll->addWidget(lp);
    m_legend->hide();
}

void Panel::buildGenPanel()
{
    m_gen = new QFrame(this);
    m_gen->setObjectName(QStringLiteral("cartoCard"));
    m_gen->setFixedWidth(340);
    auto* l = new QVBoxLayout(m_gen);
    l->setContentsMargins(16, 14, 16, 16);
    l->setSpacing(10);
    auto* t = new QLabel(tr("Gerar mapa"), m_gen);
    t->setObjectName(QStringLiteral("cartoCardTitle"));
    l->addWidget(t);
    auto seg = [&](const QString& caption, const QStringList& labels, const QStringList& tips, QButtonGroup** grp) {
        auto* cap = new QLabel(caption, m_gen);
        cap->setObjectName(QStringLiteral("cartoCaption"));
        l->addWidget(cap);
        auto* row = new QHBoxLayout();
        row->setSpacing(4);
        *grp = new QButtonGroup(m_gen);
        for (int i = 0; i < labels.size(); i++) {
            auto* b = new QToolButton(m_gen);
            b->setObjectName(QStringLiteral("cartoSeg"));
            b->setText(labels[i]);
            if (i < tips.size()) b->setToolTip(tips[i]);
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            (*grp)->addButton(b, i);
            row->addWidget(b);
        }
        (*grp)->button(0)->setChecked(true);
        l->addLayout(row);
    };
    seg(tr("TIPO"), { tr("Continente"), tr("Arquipélago"), tr("Oceânico"), tr("Vazio") },
        { QString(), QString(), QString(), tr("Só oceano: desenhe o mundo com os pincéis") }, &m_genType);
    auto* wrow = new QHBoxLayout();
    auto* wcap = new QLabel(tr("OCEANO"), m_gen);
    wcap->setObjectName(QStringLiteral("cartoCaption"));
    m_genWaterValue = new QLabel(QStringLiteral("60%"), m_gen);
    m_genWaterValue->setObjectName(QStringLiteral("cartoValue"));
    wrow->addWidget(wcap); wrow->addStretch(); wrow->addWidget(m_genWaterValue);
    l->addLayout(wrow);
    m_genWater = new QSlider(Qt::Horizontal, m_gen);
    m_genWater->setRange(20, 85);
    m_genWater->setValue(60);
    connect(m_genWater, &QSlider::valueChanged, this, [this](int v) { m_genWaterValue->setText(QStringLiteral("%1%").arg(v)); });
    l->addWidget(m_genWater);
    connect(m_genType, &QButtonGroup::idClicked, this, [this, wcap](int id) {
        m_genWater->setEnabled(id != 3); wcap->setEnabled(id != 3); m_genWaterValue->setEnabled(id != 3);
    });
    auto* scap = new QLabel(tr("TAMANHO"), m_gen);
    scap->setObjectName(QStringLiteral("cartoCaption"));
    l->addWidget(scap);
    m_genSize = new QComboBox(m_gen);
    m_genSize->setObjectName(QStringLiteral("cartoCombo"));
    for (int s = 0; s < 6; s++) {
        const SizePreset& sp = sizePreset(s);
        const int W = (sp.gw - 1) * 5 + 1, H = (sp.gh - 1) * 5 + 1;
        QString label = QStringLiteral("%1 · %2×%3").arg(sizeName(s)).arg(W - 1).arg(H - 1);
        if (s == 4) label += tr(" (2× o Épico)");
        if (s == 5) label += tr(" (3× o Épico)");
        m_genSize->addItem(label, s);
    }
    m_genSize->setCurrentIndex(1);
    l->addWidget(m_genSize);
    seg(tr("CLIMA"), { tr("Misto"), tr("Frio"), tr("Árido"), tr("Verde") },
        { tr("Sorteia a latitude, se o mundo é mais frio ou mais quente, e manchas de clima maiores"),
          tr("Perto do polo, cheio de gelo e tundra. No Mundo e acima, era do gelo"),
          tr("Cinturão de desertos, com oásis onde sobra umidade"), tr("Úmido: floresta e pântano") }, &m_genClimate);
    auto* sdcap = new QLabel(tr("SEMENTE"), m_gen);
    sdcap->setObjectName(QStringLiteral("cartoCaption"));
    l->addWidget(sdcap);
    auto* sdrow = new QHBoxLayout();
    m_genSeed = new QSpinBox(m_gen);
    m_genSeed->setObjectName(QStringLiteral("cartoSpin"));
    m_genSeed->setRange(0, 99999);
    m_genSeed->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_genSeed->setValue(QRandomGenerator::global()->bounded(99999));
    auto* dice = new QToolButton(m_gen);
    dice->setObjectName(QStringLiteral("cartoBtn"));
    dice->setFixedSize(34, 30);
    dice->setToolTip(tr("Semente aleatória"));
    dice->setCursor(Qt::PointingHandCursor);
    m_icons.append({ dice, QStringLiteral("dice") });
    connect(dice, &QToolButton::clicked, this, [this]() { m_genSeed->setValue(QRandomGenerator::global()->bounded(99999)); });
    sdrow->addWidget(m_genSeed, 1); sdrow->addWidget(dice);
    l->addLayout(sdrow);
    m_genInfo = new QToolButton(m_gen);
    m_genInfo->setObjectName(QStringLiteral("cartoCheck"));
    m_genInfo->setCheckable(true);
    m_genInfo->setChecked(QSettings().value(QStringLiteral("carto/genInfo"), true).toBool());
    m_genInfo->setCursor(Qt::PointingHandCursor);
    m_genInfo->setText(tr("Gerar territórios e locais"));
    m_genInfo->setToolTip(tr("Reinos, cidades, estradas e nomes sugeridos pelo terreno. Desmarcado, o mapa vem só com o relevo e o resto é com você."));
    m_genInfo->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_genInfo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_genInfo, &QToolButton::clicked, this, [this](bool on) { QSettings().setValue(QStringLiteral("carto/genInfo"), on); syncUi(); });
    connect(m_genType, &QButtonGroup::idClicked, this, [this](int id) { m_genInfo->setEnabled(id != 3); syncUi(); });   // mar vazio: não há onde pôr
    l->addWidget(m_genInfo);
    auto* go = new QPushButton(tr("Gerar mapa"), m_gen);
    go->setObjectName(QStringLiteral("cartoGo"));
    go->setCursor(Qt::PointingHandCursor);
    connect(go, &QPushButton::clicked, this, &Panel::generateFromPanel);
    connect(m_genType, &QButtonGroup::idClicked, this, [go](int id) { go->setText(id == 3 ? tr("Criar mar vazio") : tr("Gerar mapa")); });
    l->addWidget(go);
    auto* note = new QLabel(tr("O mapa atual é substituído. Ctrl+Z desfaz."), m_gen);
    note->setObjectName(QStringLiteral("cartoNote"));
    note->setAlignment(Qt::AlignCenter);
    l->addWidget(note);
    m_gen->hide();
}

void Panel::buildWelcome()
{
    m_welcome = new QFrame(this);
    m_welcome->setObjectName(QStringLiteral("cartoCard"));
    m_welcome->setFixedWidth(380);
    auto* l = new QVBoxLayout(m_welcome);
    l->setContentsMargins(20, 18, 20, 18);
    l->setSpacing(12);
    auto* t = new QLabel(tr("Bem-vindo ao Qenna Map!"), m_welcome);
    t->setObjectName(QStringLiteral("cartoCardTitle"));
    l->addWidget(t);
    auto* body = new QLabel(tr("Aqui, você pode começar a criar o mapa do seu mundo e gerenciá-lo utilizando os documentos do seu projeto. "
                               "Desenhe o seu mapa ou gere um nas opções acima."), m_welcome);
    body->setObjectName(QStringLiteral("cartoNote"));
    body->setStyleSheet(QStringLiteral("font-size: 13px;"));
    body->setWordWrap(true);
    l->addWidget(body);
    auto* ok = new QPushButton(tr("OK"), m_welcome);
    ok->setObjectName(QStringLiteral("cartoGo"));
    ok->setCursor(Qt::PointingHandCursor);
    connect(ok, &QPushButton::clicked, this, [this]() {
        m_welcome->hide();
        QSettings().setValue(QStringLiteral("carto/welcomed"), true);
        if (m_askSize) { m_sizeCard->show(); positionOverlays(); }
    });
    l->addWidget(ok);
    m_welcome->hide();

    // logo depois: o tamanho do mapa em branco (quem desenha precisa escolher antes)
    m_sizeCard = new QFrame(this);
    m_sizeCard->setObjectName(QStringLiteral("cartoCard"));
    m_sizeCard->setFixedWidth(340);
    auto* sl = new QVBoxLayout(m_sizeCard);
    sl->setContentsMargins(20, 18, 20, 18);
    sl->setSpacing(8);
    auto* st = new QLabel(tr("Tamanho do mapa"), m_sizeCard);
    st->setObjectName(QStringLiteral("cartoCardTitle"));
    sl->addWidget(st);
    auto* sn = new QLabel(tr("Quanto maior, mais espaço pra desenhar e mais pesado fica."), m_sizeCard);
    sn->setObjectName(QStringLiteral("cartoNote"));
    sn->setWordWrap(true);
    sl->addWidget(sn);
    for (int s = 0; s < 6; s++) {
        const SizePreset& sp = sizePreset(s);
        const int W = (sp.gw - 1) * 5 + 1, H = (sp.gh - 1) * 5 + 1;
        QString label = QStringLiteral("%1 · %2×%3").arg(sizeName(s)).arg(W - 1).arg(H - 1);
        if (s == 4) label += tr(" (2× o Épico)");
        if (s == 5) label += tr(" (3× o Épico)");
        auto* b = new QPushButton(label, m_sizeCard);
        b->setObjectName(QStringLiteral("cartoSize"));
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, s]() { pickBlankSize(s); });
        sl->addWidget(b);
    }
    m_sizeCard->hide();
}

void Panel::askBlankSize()
{
    m_askSize = true;
    if (!m_welcome->isVisible()) m_sizeCard->show();
    positionOverlays();
}

// o mar vazio do começo no tamanho que o usuário escolheu
void Panel::pickBlankSize(int size)
{
    m_askSize = false;
    m_sizeCard->hide();
    m_genSize->setCurrentIndex(size);   // o Gerar já abre no mesmo tamanho
    if (size != m_doc.world.size) {
        m_canvas->waitIdle();
        const int seed = m_doc.world.seed;
        m_canvas->setDoc(nullptr, false);
        m_doc = Doc();
        m_doc.world = emptyWorld(seed, size, QStringLiteral("mixed"));
        m_doc.clearSuggestions();
        m_canvas->setDoc(&m_doc, true); if (m_explore) m_explore->mapReplaced();
        saveNow();
    }
    syncUi();
}

void Panel::buildSettings()
{
    m_settings = new QFrame(this);
    m_settings->setObjectName(QStringLiteral("cartoCard"));
    m_settings->setFixedWidth(360);
    auto* l = new QVBoxLayout(m_settings);
    l->setContentsMargins(14, 12, 14, 14);
    l->setSpacing(8);
    auto* t = new QLabel(tr("Ajustes do mapa"), m_settings);
    t->setObjectName(QStringLiteral("cartoCardTitle"));
    l->addWidget(t);
    auto* cap = new QLabel(tr("VISUAL DO TERRENO"), m_settings);
    cap->setObjectName(QStringLiteral("cartoCaption"));
    l->addWidget(cap);
    auto* row = new QHBoxLayout();
    row->setSpacing(4);
    m_styleGroup = new QButtonGroup(m_settings);
    const QStringList labels = { tr("Mira"), tr("Novo") };
    const QStringList tips = { tr("O visual original do Mira (V)"), tr("Tons variados, texturas e luz de fim de tarde (V)") };
    for (int i = 0; i < 2; i++) {
        auto* b = new QToolButton(m_settings);
        b->setObjectName(QStringLiteral("cartoSeg"));
        b->setText(labels[i]); b->setToolTip(tips[i]);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_styleGroup->addButton(b, i);
        row->addWidget(b);
    }
    m_styleGroup->button(1)->setChecked(true);
    connect(m_styleGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_canvas->setTerrainStyle(id == 0 ? QStringLiteral("mira") : QStringLiteral("rich"));
        scheduleSave();
    });
    l->addLayout(row);
    auto* ucap = new QLabel(tr("INTERFACE"), m_settings);
    ucap->setObjectName(QStringLiteral("cartoCaption"));
    l->addWidget(ucap);
    l->addLayout(newSeg(m_settings, { QStringLiteral("astro"), QStringLiteral("cartela") }, { tr("Astrolábio"), tr("Cartela") },
                        { tr("O instrumento no canto junta vista, rotação, inclinação e hora; as ferramentas abrem num anel"),
                          tr("A legenda do mapa é a paleta; a rosa dos ventos gira o mapa") },
                        m_uiBtns, [this](const QString& v) {
                            m_ui = v;
                            QSettings().setValue(QStringLiteral("carto/ui"), v);
                            m_dockFromRing = false; m_dockFromCartela = false; m_dockUser = false;
                            applyUiMode();
                        }));
    auto check = [&](const QString& text, QToolButton** out, const std::function<void(bool)>& onToggle) {
        auto* b = new QToolButton(m_settings);
        b->setObjectName(QStringLiteral("cartoCheck"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setText(text);
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(b, &QToolButton::clicked, this, onToggle);
        l->addWidget(b);
        *out = b;
    };
    check(tr("Cartela no mapa (legenda, escala e título)"), &m_cartelaChk, [this](bool on) {
        m_cartelaOn = on;
        QSettings().setValue(QStringLiteral("carto/cartela"), on);
        applyUiMode();
    });
    check(tr("Árvores, casas e castelos"), &m_spritesChk, [this](bool on) {
        m_canvas->opts().sprites = on;
        QSettings().setValue(QStringLiteral("carto/sprites"), on);
        m_canvas->optsChanged();
        applyUiMode();
    });
    m_settings->hide();
}

void Panel::applyTheme()
{
    const QColor a = Theme::toColor(Theme::accentDefault());
    const QString soft = QStringLiteral("rgba(%1,%2,%3,0.18)").arg(a.red()).arg(a.green()).arg(a.blue());
    setStyleSheet(Theme::qss(QStringLiteral(
        "QWidget#cartoPanel { background: %8; }"
        "QFrame[cartoFloat=\"true\"], QFrame#cartoCard { background: %1; border: 1px solid %2; border-radius: 12px; }"
        "QFrame#cartoDock { background: transparent; border: none; }"
        "QToolButton#cartoBtn, QToolButton#cartoToolBtn { background: transparent; border: none; border-radius: @radius-control; color: %6; font-size: 12px; font-weight: 600; padding: 0 6px; }"
        "QToolButton#cartoBtn:hover, QToolButton#cartoToolBtn:hover { background: %4; color: %3; }"
        "QToolButton#cartoBtn:checked { background: %9; color: %5; }"
        "QToolButton#cartoToolBtn { color: %3; padding: 0 10px; }"
        "QFrame#cartoSep { background: %2; }"
        "QLabel#cartoTitleText { color: %7; font-family: 'IM FELL English SC', Georgia, serif; font-size: 21px; letter-spacing: 1px; background: transparent; }"
        "QLineEdit#cartoTitleEdit { color: %7; font-family: 'IM FELL English SC', Georgia, serif; font-size: 21px; background: transparent;"
        "  border: none; border-bottom: 1px solid %5; padding: 0; min-width: 260px; }"
        "QLabel#cartoSubText { color: %6; font-size: 11px; background: transparent; }"
        "QLabel#cartoHourText { color: %3; font-size: 12px; background: transparent; }"
        "QLabel#cartoHint { color: %6; font-size: 11px; background: %1; border: 1px solid %2; border-radius: 12px; padding: 4px 12px; }"
        "QLabel#cartoCardTitle { color: %5; font-family: 'IM FELL English SC', Georgia, serif; font-size: 16px; letter-spacing: 1px; background: transparent; }"
        "QLabel#cartoCaption { color: %6; font-size: 10px; font-weight: 600; letter-spacing: 1px; background: transparent; }"
        "QLabel#cartoCaption:disabled { color: %10; }"
        "QLabel#cartoValue { color: %3; font-size: 12px; font-weight: 600; background: transparent; }"
        "QLabel#cartoNote { color: %6; font-size: 11px; background: transparent; }"
        "QToolButton#cartoChoice { background: transparent; border: 1px solid transparent; border-radius: 9px; color: %6; font-size: 10px; font-weight: 600; padding: 5px 2px 3px; }"
        "QToolButton#cartoChoice:hover { background: %4; color: %3; }"
        "QToolButton#cartoChoice:checked { background: %9; color: %5; border-color: %5; }"
        "QToolButton#cartoSeg { background: %4; border: 1px solid %2; border-radius: 8px; color: %6; font-size: 12px; padding: 6px 4px; }"
        "QToolButton#cartoSeg:checked { background: %9; color: %5; border-color: %5; font-weight: 600; }"
        "QPushButton#cartoSize { background: %4; border: 1px solid %2; border-radius: 8px; color: %3; font-size: 12px; padding: 8px 12px; text-align: left; }"
        "QPushButton#cartoSize:hover { background: %9; border-color: %5; color: %5; }"
        "QComboBox#cartoCombo, QSpinBox#cartoSpin { background: %11; border: 1px solid %2; border-radius: 8px; color: %3; padding: 5px 8px; font-size: 13px; }"
        "QPushButton#cartoGo { background: %5; color: %12; border: none; border-radius: 9px; padding: 9px 0; font-size: 13px; font-weight: 700; }"
        "QPushButton#cartoGo:hover { background: %5; }"
        "QToolButton#cartoLink { background: transparent; border: none; color: %5; font-size: 12px; font-weight: 600; padding: 2px 0; }"
        "QToolButton#cartoLink:hover { text-decoration: underline; }"
        "QToolButton#cartoCheck { background: transparent; border: none; color: %3; font-size: 12px; padding: 2px 0; text-align: left; }"
        "QFrame#cartoRow { background: transparent; border: 1px solid transparent; border-radius: 8px; }"
        "QFrame#cartoRow[sel=\"true\"] { background: %9; border-color: %5; }"
        "QLineEdit#cartoRowName { background: transparent; border: none; color: %3; font-size: 13px; padding: 2px 0; }"
        "QLineEdit#cartoField { background: %11; border: 1px solid %2; border-radius: 8px; color: %3; padding: 5px 8px; font-size: 13px; }"
        "QFrame#cartoSelBox { background: %4; border: 1px solid %2; border-radius: 10px; }"
        "QSlider::groove:horizontal { height: 4px; background: %2; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: %5; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: %1; border: 2px solid %5; width: 12px; height: 12px; margin: -6px 0; border-radius: 8px; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(), Theme::hoverOverlay(), Theme::accentDefault(),
          Theme::textMuted(), Theme::textBright(), QStringLiteral("#1a3d58"), soft)
     .arg(Theme::disabledText(), Theme::inputBackground(), Theme::readableTextOn(a).name())));
    const QColor muted = Theme::toColor(Theme::textMuted()), ink = Theme::toColor(Theme::textPrimary());
    for (const auto& [btn, name] : m_icons) btn->setIcon(icon(name, muted, a));
    if (m_toolBtn) m_toolBtn->setIcon(icon(QStringLiteral("ring"), ink, a));
    if (m_explore) m_explore->applyTheme();
    if (m_astro) m_astro->update();
    syncUi();
}

// ── posições ──
void Panel::positionOverlays()
{
    const int W = width(), H = height();
    // o layout guarda a medida velha até o próximo LayoutRequest: refaz antes de medir
    for (QWidget* w : { static_cast<QWidget*>(m_titlePill), static_cast<QWidget*>(m_topPill), static_cast<QWidget*>(m_toolPill), static_cast<QWidget*>(m_hourPill) })
        if (w->layout()) w->layout()->invalidate();
    m_titlePill->adjustSize();
    m_titlePill->move(16, 14);
    m_topPill->adjustSize();
    m_topPill->move(W - m_topPill->width() - 16, 14);
    m_astro->move(W - m_astro->width() - 4, H - m_astro->height() - 2);
    m_toolPill->adjustSize();
    m_hourPill->adjustSize();
    const int px = m_astro->x() - 8;
    m_toolPill->move(px - m_toolPill->width(), H - 18 - m_toolPill->height());
    m_hourPill->move(px - m_hourPill->width(), m_toolPill->y() - 8 - m_hourPill->height());
    // uma linha só, no meio; encosta pra esquerda (ou quebra) antes de bater nas pílulas da direita
    const int right = m_toolPill->x() - 12, room = std::max(200, right - 16);
    const bool wrap = m_hint->fontMetrics().horizontalAdvance(m_hint->text()) + 28 > room;
    m_hint->setWordWrap(wrap);
    if (wrap) m_hint->setFixedWidth(room); else { m_hint->setMinimumWidth(0); m_hint->setMaximumWidth(QWIDGETSIZE_MAX); }
    m_hint->adjustSize();
    m_hint->move(std::max(16, std::min((W - m_hint->width()) / 2, right - m_hint->width())), H - 16 - m_hint->height());
    m_gen->adjustSize();
    m_gen->move((W - m_gen->width()) / 2, 70);
    m_settings->adjustSize();
    m_settings->move(W - m_settings->width() - 16, m_topPill->y() + m_topPill->height() + 8);
    if (m_export) {
        m_export->adjustSize();
        m_export->move(W - m_export->width() - 16, m_topPill->y() + m_topPill->height() + 8);
    }
    if (m_cartela) {
        const bool cartelaUi = m_ui == QLatin1String("cartela");
        m_cartela->resize(m_cartela->sizeHint());
        m_cartela->move(16, H - m_cartela->height() - 14);
        m_rose->move(m_cartela->x() + m_cartela->width() - 74, m_cartela->y() - 96);
        m_cbar->adjustSize();
        m_cbar->move(W - m_cbar->width() - 16, m_topPill->y() + m_topPill->height() + 8);
        m_cview->adjustSize();
        m_cview->move(W - m_cview->width() - 16, H - m_cview->height() - 14);
        // a dica fica no meio, entre a Cartela e o canto direito
        const int left = m_cartela->isVisible() ? m_cartela->x() + m_cartela->width() + 12 : 16;
        const int right = (cartelaUi ? m_cview->x() : m_toolPill->x()) - 12;
        const int room = std::max(200, right - left);
        const bool wrap = m_hint->fontMetrics().horizontalAdvance(m_hint->text()) + 28 > room;
        m_hint->setWordWrap(wrap);
        if (wrap) m_hint->setFixedWidth(room); else { m_hint->setMinimumWidth(0); m_hint->setMaximumWidth(QWIDGETSIZE_MAX); }
        m_hint->adjustSize();
        m_hint->move(std::max(left, std::min((W - m_hint->width()) / 2, right - m_hint->width())), H - 16 - m_hint->height());
        for (QWidget* w : { static_cast<QWidget*>(m_cartela), static_cast<QWidget*>(m_rose), static_cast<QWidget*>(m_cbar), static_cast<QWidget*>(m_cview) }) w->raise();
        m_gen->raise(); m_settings->raise(); m_export->raise();
    }
    for (QFrame* card : { m_welcome, m_sizeCard }) {
        if (!card || !card->isVisible()) continue;
        card->adjustSize();
        card->move((W - card->width()) / 2, (H - card->height()) / 2);
        card->raise();
    }
    placeDock();
    if (m_ring->isVisible()) m_ring->setGeometry(rect());
    if (m_explore) m_explore->place();
}

void Panel::placeDock()
{
    if (!m_dock->isVisible()) return;
    m_dock->adjustSize();
    const int w = m_dock->width(), h = m_dock->height();
    int x, y;
    auto clampTo = [&](int& cx, int& cy) {
        cx = std::max(8, std::min(width() - w - 8, cx));
        cy = std::max(64, std::min(std::max(64, height() - h - 8), cy));
    };
    if (m_dockUser) { x = m_dockAt.x(); y = m_dockAt.y(); clampTo(x, y); }
    else {
        // canto de baixo à direita é do Astrolábio e das pílulas (ou do painel da vista): a doca não senta em cima
        const bool cartelaUi = m_ui == QLatin1String("cartela");
        const QRect corner = cartelaUi ? m_cview->geometry().adjusted(-8, -8, 8, 8)
                                       : m_astro->geometry().united(m_hourPill->geometry()).united(m_toolPill->geometry()).adjusted(-8, -8, 8, 8);
        QList<QPoint> spots;
        if (m_dockFromRing) spots = { { m_dockAt.x() + 158, m_dockAt.y() - h / 2 }, { m_dockAt.x() - 158 - w, m_dockAt.y() - h / 2 } };
        if (m_dockFromCartela && m_cartela && m_cartela->isVisible()) spots.append(QPoint(m_cartela->x() + m_cartela->width() + 12, m_cartela->y() + m_cartela->height() - h));
        spots.append(cartelaUi ? QPoint(width() - w - 76, 64) : QPoint(16, 86));
        x = spots.first().x(); y = spots.first().y(); clampTo(x, y);
        for (QPoint sp : spots) {
            int cx = sp.x(), cy = sp.y();
            clampTo(cx, cy);
            if (!QRect(cx, cy, w, h).intersects(corner)) { x = cx; y = cy; break; }
        }
    }
    m_dock->move(x, y);
    m_dock->raise();
}

void Panel::resizeEvent(QResizeEvent*) { positionOverlays(); }

// ── estado → interface ──
QString Panel::toolLabel() const
{
    if (m_tool == QLatin1String("brush")) return (m_canvas->showClimate() ? tr("Clima") : tr("Pincel")) + QStringLiteral(" · ") + brushName(m_brush);
    if (m_tool == QLatin1String("area")) return tr("Área");
    if (m_tool == QLatin1String("realm")) {
        const QString& t = m_canvas->opts().realmTool;
        return tr("Territórios") + QStringLiteral(" · ") + (t == QLatin1String("paint") ? tr("Pintar") : t == QLatin1String("neutral") ? tr("Neutro") : tr("Semear"));
    }
    if (m_tool == QLatin1String("build")) return tr("Construir") + QStringLiteral(" · ") + buildKindName(m_canvas->opts().buildKind);
    if (m_tool == QLatin1String("names")) return tr("Nomes");
    if (m_tool == QLatin1String("ruler")) return tr("Régua");
    if (m_tool == QLatin1String("explore")) return tr("Explorar");
    return tr("Mover");
}

void Panel::syncUi()
{
    CARTO_PROF("syncUi");
    if (!m_canvas) return;
    const int idx = m_store.indexOf(m_mapId);
    if (!m_titleEdit->isVisible()) m_title->setText(idx >= 0 ? m_store.maps()[idx].name : tr("Mapa"));
    QString sub = m_hasWorld ? QStringLiteral("%1 · %2 · %3").arg(modeName(m_canvas->mode()), sizeName(m_doc.world.size), tr("semente %1").arg(m_doc.world.seed)) : QString();
    if (m_canvas->busy()) sub += tr(" · pintando…");
    m_sub->setText(sub);
    m_astro->setState(m_canvas->angle(), m_canvas->pitch(), m_canvas->hour(), m_canvas->mode());
    m_hourLabel->setText(QStringLiteral("%1 · %2").arg(fmtHour(m_canvas->hour()), phaseName(m_canvas->hour())));
    m_toolBtn->setText(toolLabel() + QStringLiteral("  ▴"));
    m_undoBtn->setEnabled(!m_undo.empty());
    m_redoBtn->setEnabled(!m_redo.empty());
    m_genBtn->setChecked(m_gen->isVisible());
    m_genInfo->setIcon(icon(m_genInfo->isChecked() && m_genInfo->isEnabled() ? QStringLiteral("check") : QStringLiteral("box"),
                            Theme::toColor(Theme::textMuted()), Theme::toColor(Theme::accentDefault())));
    // o Completo só abre quando o mapa tem o que mostrar
    const bool fullLocked = m_hasWorld && !m_doc.hasContent();
    m_astro->setLockedMode(fullLocked ? QStringLiteral("full") : QString());
    if (QToolButton* fb = m_viewBtns.value(QStringLiteral("full"))) {
        fb->setEnabled(!fullLocked);
        fb->setToolTip(fullLocked ? fullLockedWhy() : QString());
    }
    m_setBtn->setChecked(m_settings->isVisible());
    m_playBtn->setIcon(icon(m_playTimer->isActive() ? QStringLiteral("pause") : QStringLiteral("play"),
                            Theme::toColor(Theme::textMuted()), Theme::toColor(Theme::accentDefault())));
    for (auto it = m_brushBtns.begin(); it != m_brushBtns.end(); ++it) it.value()->setChecked(it.key() == int(m_brush));
    m_brushTitle->setText(m_canvas->showClimate() ? tr("Clima") : tr("Pincel"));
    syncRealmCard();
    syncBuildCard();
    syncNamesCard();
    syncCartela();
    if (m_rulerBtn) m_rulerBtn->setChecked(m_tool == QLatin1String("ruler"));
    if (m_exportBtn && m_export) {
        m_exportBtn->setChecked(m_export->isVisible());
        for (auto it = m_expScaleBtns.begin(); it != m_expScaleBtns.end(); ++it) it.value()->setChecked(std::fabs(it.key().toDouble() - m_expScale) < 0.01);
        m_expSize->setText(tr("%1 × %2 px").arg(qRound(m_canvas->width() * m_expScale)).arg(qRound(m_canvas->height() * m_expScale)));
        m_expCartela->setIcon(icon(m_expWithCartela ? QStringLiteral("check") : QStringLiteral("box"), Theme::toColor(Theme::textMuted()), Theme::toColor(Theme::accentDefault())));
    }
    // dicas
    QString hint;
    if (m_tool == QLatin1String("area")) hint = tr("Área: arraste um retângulo pra criar terra · roda: zoom · botão direito: ferramentas (arrastado gira e inclina)");
    else if (m_tool == QLatin1String("brush")) hint = tr("%1 · Shift inverte · [ e ] tamanho · botão direito: ferramentas (arrastado gira e inclina)").arg(brushName(m_brush));
    else if (m_tool == QLatin1String("realm")) hint = tr("Territórios: semear, pintar ou neutro (na doca) · Ctrl+Z desfaz · botão direito: ferramentas");
    else if (m_tool == QLatin1String("build")) hint = tr("Construir: escolha na doca · Q/E giram o selecionado · Delete apaga · Ctrl+Z desfaz");
    else if (m_tool == QLatin1String("names")) hint = tr("Nomes: clique num nome pra editar, arraste pra mover · clique no mapa pra pôr um nome novo (arrastando, ele segue o traço) · Delete esconde");
    else if (m_tool == QLatin1String("ruler")) hint = tr("Régua: clique pra marcar o caminho · duplo clique termina · Esc limpa");
    else if (m_tool == QLatin1String("explore")) hint = m_explore->hint();
    else hint = tr("Arraste pra mover · roda: zoom · botão direito: ferramentas (arrastado gira e inclina)");
    if (!m_flash.isEmpty()) hint = m_flash;
    m_hint->setText(hint);
    positionOverlays();
}

// ── mapas do projeto ──
void Panel::setProjectRoot(const QString& root)
{
    if (m_saveTimer->isActive()) saveNow();
    m_canvas->waitIdle();
    m_store.setProjectRoot(root);
    m_mapId.clear();
    m_hasWorld = false;
    m_askSize = false; m_sizeCard->hide(); m_welcome->hide();
    m_undo.clear(); m_redo.clear();
    m_canvas->setDoc(nullptr, false);
    if (isVisible()) ensureMap();
}

void Panel::setProjectLink(const ProjectLink& link) { if (m_explore) m_explore->setLink(link); }

void Panel::showEvent(QShowEvent*)
{
    positionOverlays();
    ensureMap();
}

void Panel::ensureMap()
{
    if (m_hasWorld || m_store.projectRoot().isEmpty()) return;
    if (m_store.maps().isEmpty()) {
        // o primeiro mapa do projeto nasce em branco: o usuário desenha ou gera
        newMap(false, true);
        if (!QSettings().value(QStringLiteral("carto/welcomed"), false).toBool()) m_welcome->show();   // o OK chama o tamanho
        askBlankSize();
        return;
    }
    openMap(m_store.activeId());
}

QString Panel::autoTitle() const { return m_hasWorld ? mapTitle(m_doc.world) : tr("Mapa"); }

void Panel::openMap(const QString& id)
{
    m_canvas->waitIdle();
    World w;
    MapViewState v;
    if (!m_store.loadMap(id, w, v)) {
        // arquivo ainda não existe (mapa novo) ou estragado: nasce um mundo da semente
        w = generate(QRandomGenerator::global()->bounded(99999), 60, QStringLiteral("continent"), 1, QStringLiteral("mixed"));
    }
    m_mapId = id;
    m_store.setActiveId(id);
    m_canvas->setDoc(nullptr, false);
    m_doc = Doc();
    m_doc.world = std::move(w);
    if (!v.extra.isEmpty()) m_doc.layersFromJson(v.extra);
    m_view = v;
    m_hasWorld = true;
    m_undo.clear(); m_redo.clear();
    m_canvas->setHour(v.hour, true);
    m_canvas->setTerrainStyle(v.style);
    m_styleGroup->button(v.style == QLatin1String("mira") ? 0 : 1)->setChecked(true);
    m_canvas->setMode(v.mode == QLatin1String("full") && !m_doc.hasContent() ? QStringLiteral("terrain") : v.mode);
    m_canvas->presetView(v.ang, v.pitch);
    m_canvas->setDoc(&m_doc, true); if (m_explore) m_explore->mapReplaced();
    syncUi();
}

void Panel::newMap(bool ask, bool blank)
{
    if (m_saveTimer->isActive()) saveNow();
    m_canvas->waitIdle();
    const int seed = QRandomGenerator::global()->bounded(99999);
    World w = blank ? emptyWorld(seed, 1, QStringLiteral("mixed"))
                    : generate(seed, 60, QStringLiteral("continent"), 1, QStringLiteral("mixed"));
    QString name = mapTitle(w);
    if (ask) {
        bool ok = false;
        name = Sheets::askText(this, tr("Novo mapa"), tr("Nome do mapa"), name, &ok, tr("Criar")).trimmed();
        if (!ok || name.isEmpty()) return;
    }
    const MapMeta m = m_store.createMap(name);
    m_mapId = m.id;
    m_canvas->setDoc(nullptr, false);
    m_doc = Doc();
    m_doc.world = std::move(w);
    if (blank) m_doc.clearSuggestions();
    m_view = MapViewState();
    m_hasWorld = true;
    m_undo.clear(); m_redo.clear();
    m_canvas->setMode(m_view.mode);
    m_canvas->setDoc(&m_doc, true); if (m_explore) m_explore->mapReplaced();
    if (blank) { setTool(QStringLiteral("brush")); setBrush(Brush::Land); }
    saveNow();
    syncUi();
}

void Panel::showMapMenu()
{
    QMenu menu(this);
    for (const MapMeta& m : m_store.maps()) {
        QAction* a = menu.addAction(m.name);
        a->setCheckable(true);
        a->setChecked(m.id == m_mapId);
        connect(a, &QAction::triggered, this, [this, id = m.id]() {
            if (id == m_mapId) return;
            if (m_saveTimer->isActive()) saveNow();
            openMap(id);
        });
    }
    menu.addSeparator();
    menu.addAction(tr("Novo mapa…"), this, [this]() { newMap(true); });
    menu.addAction(tr("Renomear…"), this, [this]() { m_title->hide(); m_titleEdit->setText(m_title->text()); m_titleEdit->show(); m_titleEdit->setFocus(); m_titleEdit->selectAll(); positionOverlays(); });
    QAction* del = menu.addAction(tr("Excluir este mapa…"), this, [this]() {
        const int i = m_store.indexOf(m_mapId);
        if (i < 0) return;
        if (!Sheets::confirm(this, tr("Excluir mapa"), tr("Excluir o mapa \"%1\"?").arg(m_store.maps()[i].name),
                             tr("O mapa some do projeto. Um backup do projeto traz ele de volta."), tr("Excluir"))) return;
        m_saveTimer->stop();
        m_canvas->waitIdle();
        flushSaves();   // uma gravação atrasada não pode recriar o arquivo apagado
        m_store.removeMap(m_mapId);
        m_hasWorld = false;
        m_mapId.clear();
        if (m_store.maps().isEmpty()) { newMap(false, true); askBlankSize(); }
        else openMap(m_store.activeId());
    });
    del->setEnabled(m_store.maps().size() > 0);
    menu.exec(m_mapsBtn->mapToGlobal(QPoint(0, m_mapsBtn->height() + 4)));
}

void Panel::scheduleSave() { if (m_hasWorld) m_saveTimer->start(); }

void Panel::saveNow()
{
    CARTO_PROF("salvar (painel)");
    m_saveTimer->stop();
    if (!m_hasWorld || m_mapId.isEmpty()) return;
    m_view.ang = m_canvas->angle();
    m_view.pitch = m_canvas->pitch();
    m_view.mode = m_canvas->mode();
    m_view.style = m_canvas->terrainStyle();
    m_view.hour = m_canvas->hour();
    m_view.extra = m_doc.layersToJson();
    auto job = std::make_shared<SaveJob>(m_store.prepareSave(m_mapId, m_doc.world, m_view));
    m_savePool->start([job]() { writeSave(*job); });
}

void Panel::hideEvent(QHideEvent*)
{
    setPlaying(false);
    if (m_saveTimer->isActive()) saveNow();
}
void Panel::closeEvent(QCloseEvent* e)
{
    if (m_saveTimer->isActive()) saveNow();
    flushSaves();
    emit closeRequested();
    e->accept();
}

// ── ferramentas ──
void Panel::openRingAt(QPoint at)
{
    const QString active = (m_tool == QLatin1String("brush") && m_canvas->showClimate()) ? QStringLiteral("climate") : m_tool;
    const QStringList parts = toolLabel().split(QStringLiteral(" · "));
    m_ring->openAt(at, active, parts.value(0), parts.value(1));
}

void Panel::pickTool(const QString& id, QPoint ringCenter)
{
    if (id == QLatin1String("gen")) { m_gen->show(); positionOverlays(); syncUi(); return; }
    m_dockAt = ringCenter; m_dockFromRing = true; m_dockUser = false;
    if (id == QLatin1String("climate")) {
        setTool(QStringLiteral("brush"));
        setClimate(true);
        if (!isClimateBrush(m_brush)) setBrush(Brush::Wet);
    } else if (id == QLatin1String("brush")) {
        setTool(QStringLiteral("brush"));
        setClimate(false);
        if (isClimateBrush(m_brush)) setBrush(Brush::Land);
    } else {
        setClimate(false);
        if (id == QLatin1String("build")) ensureView(QStringLiteral("full"));
        else if (id == QLatin1String("realm") || id == QLatin1String("names") || id == QLatin1String("explore")) ensureView(QStringLiteral("info"));
        setTool(id);
    }
}

void Panel::ensureView(const QString& v)
{
    const QString m = m_canvas->mode();
    if (v == QLatin1String("terrain") && m != QLatin1String("terrain")) m_canvas->setMode(QStringLiteral("terrain"));
    else if (v == QLatin1String("full") && m != QLatin1String("full")) m_canvas->setMode(QStringLiteral("full"));
    else if (v == QLatin1String("info") && m == QLatin1String("terrain")) m_canvas->setMode(QStringLiteral("full"));
    if (m_canvas->mode() == QLatin1String("full") && m_flash == fullLockedWhy()) m_flash.clear();   // a ferramenta abriu o Completo
    scheduleSave();
}

void Panel::rotateSelected(double d)
{
    Art* a = m_canvas->selectedArt();
    if (!a) return;
    const QString id = a->id;
    m_canvas->userEdit([id, d](Info& in) { for (Art& x : in.user.arts) if (x.id == id) { x.yaw += d; return true; } return false; });
}

void Panel::deleteSelected()
{
    if (m_tool == QLatin1String("build")) {
        const QString id = m_canvas->selArt();
        if (id.isEmpty()) return;
        m_canvas->setSelArt(QString());
        m_canvas->userEdit([id](Info& in) {
            auto& A = in.user.arts;
            const size_t n = A.size();
            A.erase(std::remove_if(A.begin(), A.end(), [&](const Art& x) { return x.id == id; }), A.end());
            return A.size() != n;
        });
    } else if (m_tool == QLatin1String("names")) {
        const Label* lab = m_canvas->scene().nameByRef(m_canvas->selName());
        if (!lab) return;
        Label copy = *lab;
        if (copy.autoName) {
            m_canvas->nameEdit([&](LabelLayer& L) { const int e = editFor(L, m_doc.names, copy); if (e < 0) return false; L.edits[e].hidden = !L.edits[e].hidden; return true; });
        } else {
            const QString ref = copy.ref;
            m_canvas->setSelName(QString());
            m_canvas->nameEdit([ref](LabelLayer& L) {
                const size_t n = L.custom.size();
                L.custom.erase(std::remove_if(L.custom.begin(), L.custom.end(), [&](const NameCustom& c) { return c.id == ref; }), L.custom.end());
                return L.custom.size() != n;
            });
        }
    }
}

void Panel::setTool(const QString& id)
{
    m_tool = id;
    const bool draws = id == QLatin1String("area") || id == QLatin1String("brush");
    if (draws && m_canvas->mode() != QLatin1String("terrain")) m_canvas->setMode(QStringLiteral("terrain"));   // desenhar é na vista Terreno
    m_canvas->setTool(id == QLatin1String("area") ? Tool::Area : id == QLatin1String("brush") ? Tool::Brush : id == QLatin1String("realm") ? Tool::Realm
                      : id == QLatin1String("build") ? Tool::Build : id == QLatin1String("names") ? Tool::Names
                      : id == QLatin1String("ruler") ? Tool::Ruler : id == QLatin1String("explore") ? Tool::Explore : Tool::Pan);
    if (id != QLatin1String("ruler")) m_canvas->clearRuler();
    if (m_explore) m_explore->setActive(id == QLatin1String("explore"));
    const bool brush = id == QLatin1String("brush"), realm = id == QLatin1String("realm"), build = id == QLatin1String("build"), names = id == QLatin1String("names");
    m_brushCard->setVisible(brush);
    m_realmCard->setVisible(realm);
    m_buildCard->setVisible(build);
    m_namesCard->setVisible(names);
    m_dock->setVisible(brush || realm || build || names);
    m_legend->setVisible(brush && m_canvas->showClimate());
    if (id == QLatin1String("brush")) setBrush(m_brush);
    syncUi();
}

void Panel::setBrush(Brush b)
{
    m_brush = b;
    m_canvas->setBrush(b);
    const double sz = m_brushSizes.value(int(b), m_brushSizes.value(-1, 36.0));
    m_sizeSlider->blockSignals(true);
    m_sizeSlider->setValue(qRound(sz));
    m_sizeSlider->blockSignals(false);
    m_sizeValue->setText(QString::number(qRound(sz)));
    m_canvas->setBrushSize(sz);
    m_canvas->setBrushStrength(m_strength);
    if (m_tool != QLatin1String("brush")) { setTool(QStringLiteral("brush")); return; }
    syncUi();
}

QString Panel::fullLockedWhy() const
{
    return tr("O Completo abre quando o mapa tiver um território, uma construção ou um nome. Crie um pelas ferramentas ou gere com territórios e locais.");
}

void Panel::setMode(const QString& mode)
{
    if (mode == QLatin1String("full") && m_hasWorld && !m_doc.hasContent()) { flash(fullLockedWhy()); return; }
    // cada ferramenta mora numa vista: desenhar no Terreno, territórios e nomes nas vistas com informação,
    // construir no Completo; se a vista nova não serve, a ferramenta volta pra Mover
    const bool terrain = mode == QLatin1String("terrain");
    const bool keep = m_tool == QLatin1String("pan") || m_tool == QLatin1String("ruler") || (terrain && (m_tool == QLatin1String("area") || m_tool == QLatin1String("brush")))
        || (!terrain && (m_tool == QLatin1String("realm") || m_tool == QLatin1String("names") || m_tool == QLatin1String("explore")))
        || (mode == QLatin1String("full") && m_tool == QLatin1String("build"));
    if (!keep) { setClimate(false); setTool(QStringLiteral("pan")); }
    m_canvas->setMode(mode);
    scheduleSave();
    syncUi();
}

void Panel::setClimate(bool on)
{
    m_canvas->setShowClimate(on);
    m_legend->setVisible(on && m_tool == QLatin1String("brush"));
    syncUi();
}

void Panel::setPlaying(bool on)
{
    if (on) { m_playLast = QDateTime::currentMSecsSinceEpoch(); m_playTimer->start(); }
    else if (m_playTimer->isActive()) { m_playTimer->stop(); m_canvas->setHour(m_canvas->hour(), true); scheduleSave(); }
    syncUi();
}

void Panel::generateFromPanel()
{
    CARTO_PROF("gerar (painel)");
    m_canvas->waitIdle();
    Undo u; u.kind = Undo::Full; u.world = std::make_shared<World>(m_doc.world); u.layers = std::make_shared<Doc::Layers>(m_doc.layers());
    const bool wasAuto = m_store.indexOf(m_mapId) >= 0 && m_store.maps()[m_store.indexOf(m_mapId)].name == mapTitle(m_doc.world);
    static const char* types[4] = { "continent", "archipelago", "oceanic", "empty" };
    static const char* climates[4] = { "mixed", "cold", "arid", "green" };
    const double keepScale = m_doc.world.scale;
    // mapa novo: territórios, construções e nomes do antigo ficam no desfazer
    const int ver = m_doc.ver;
    m_canvas->setDoc(nullptr, false);
    m_doc = Doc();
    m_doc.ver = ver + 1;
    m_doc.world = rebuildBase(m_genSeed->value(), m_genWater->value(), QString::fromLatin1(types[std::max(0, m_genType->checkedId())]),
                              m_genSize->currentData().toInt(), QString::fromLatin1(climates[std::max(0, m_genClimate->checkedId())]));
    m_doc.world.scale = keepScale;
    const bool bare = !m_genInfo->isChecked() || m_doc.world.type == QLatin1String("empty");
    if (bare) m_doc.clearSuggestions();
    pushUndo(std::move(u));
    if (wasAuto) m_store.renameMap(m_mapId, mapTitle(m_doc.world));   // o nome sugerido acompanha o mapa novo
    m_gen->hide();
    if (m_welcome->isVisible()) { m_welcome->hide(); QSettings().setValue(QStringLiteral("carto/welcomed"), true); }
    m_askSize = false; m_sizeCard->hide();   // o tamanho veio do Gerar
    m_canvas->setDoc(&m_doc, true); if (m_explore) m_explore->mapReplaced();
    if (m_doc.world.type == QLatin1String("empty")) { setTool(QStringLiteral("brush")); setBrush(Brush::Land); }
    else if (bare && m_canvas->mode() == QLatin1String("full")) setMode(QStringLiteral("terrain"));
    scheduleSave();
    syncUi();
}

// ── desfazer ──
void Panel::pushUndo(Undo u)
{
    // começou a desenhar: o tamanho fica o que está (trocar agora apagaria o desenho)
    if (m_askSize) { m_askSize = false; m_sizeCard->hide(); }
    m_undo.push_back(std::move(u));
    if (m_undo.size() > 40) m_undo.pop_front();
    // mundos inteiros pesam: guarda poucos em mapa grande
    const size_t n = static_cast<size_t>(m_doc.world.W) * m_doc.world.H;
    const int maxFull = n > 4000000 ? 3 : n > 2000000 ? 6 : 12;
    int fulls = 0;
    for (auto it = m_undo.end(); it != m_undo.begin();) {
        --it;
        if (it->kind == Undo::Full && ++fulls > maxFull) it = m_undo.erase(it);
    }
    m_redo.clear();
    syncUi();
}

void Panel::restore(bool undoing)
{
    auto& from = undoing ? m_undo : m_redo;
    auto& to = undoing ? m_redo : m_undo;
    if (from.empty()) return;
    m_canvas->waitIdle();
    Undo e = std::move(from.back());
    from.pop_back();
    // o desfazer não desliga um território do grupo e do Lugar dele: os vínculos voltam pelo nome
    QHash<QString, QPair<QString, QString>> links;
    if (m_doc.info) for (const RealmDef& d : m_doc.info->defs) if (!d.groupId.isEmpty() || !d.placeId.isEmpty()) links.insert(d.name, { d.groupId, d.placeId });
    auto relink = [this, &links]() {
        if (!m_doc.info) return;
        for (RealmDef& d : m_doc.info->defs) {
            const auto it = links.constFind(d.name);
            if (it == links.cend()) continue;
            if (d.groupId.isEmpty()) d.groupId = it->first;
            if (d.placeId.isEmpty()) d.placeId = it->second;
        }
    };
    if (e.kind == Undo::Full) {
        Undo back; back.kind = Undo::Full; back.world = std::make_shared<World>(m_doc.world);
        if (e.layers) back.layers = std::make_shared<Doc::Layers>(m_doc.layers());
        const bool fresh = e.world->seed != m_doc.world.seed || e.world->W != m_doc.world.W;
        m_canvas->setDoc(nullptr, false);
        m_doc.world = *e.world;
        m_doc.touch();
        if (e.layers) {
            // o mapa trocou inteiro (gerar): as camadas voltam junto
            m_doc.info.reset();
            m_doc.setLayers(*e.layers);
            relink();
        }
        to.push_back(std::move(back));
        m_canvas->setDoc(&m_doc, fresh);
        if (m_explore) m_explore->mapReplaced();
    } else if (e.kind == Undo::Layers) {
        m_doc.setLayers(undoing ? *e.lBefore : *e.lAfter);
        relink();
        m_canvas->layersReplaced();
        if (m_explore) m_explore->mapReplaced();
        to.push_back(std::move(e));
    } else {
        applySnapshot(m_doc.world, undoing ? e.before : e.after);
        m_doc.touch();
        if (e.riverId) {
            if (undoing) m_doc.rivers.erase(std::remove_if(m_doc.rivers.begin(), m_doc.rivers.end(), [&](const River& r) { return r.id == e.riverId; }), m_doc.rivers.end());
            else m_doc.rivers.push_back(e.river);
        }
        m_canvas->repaintBox({ e.before.x0, e.before.y0, e.before.x1, e.before.y1 });
        if (m_canvas->mode() != QLatin1String("terrain")) m_canvas->layersReplaced();   // cidades e estradas se refazem no terreno novo
        to.push_back(std::move(e));
    }
    scheduleSave();
    syncUi();
}

// ── teclado e arrastos ──
void Panel::keyPressEvent(QKeyEvent* e)
{
    const int k = e->key();
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    if (m_explore && k == Qt::Key_Escape && m_explore->escape()) { syncUi(); return; }
    if (m_explore && m_explore->owns(QApplication::focusWidget())) { QWidget::keyPressEvent(e); return; }
    if (ctrl && k == Qt::Key_Z && !(e->modifiers() & Qt::ShiftModifier)) { restore(true); return; }
    if (ctrl && (k == Qt::Key_Y || (k == Qt::Key_Z && (e->modifiers() & Qt::ShiftModifier)))) { restore(false); return; }
    if (ctrl) { QWidget::keyPressEvent(e); return; }
    switch (k) {
    case Qt::Key_H: pickTool(QStringLiteral("pan"), QPoint()); m_dockFromRing = false; break;
    case Qt::Key_A: pickTool(QStringLiteral("area"), QPoint()); m_dockFromRing = false; break;
    case Qt::Key_B: pickTool(QStringLiteral("brush"), QPoint()); m_dockFromRing = false; placeDock(); break;
    case Qt::Key_C: pickTool(m_canvas->showClimate() ? QStringLiteral("brush") : QStringLiteral("climate"), QPoint()); m_dockFromRing = false; placeDock(); break;
    case Qt::Key_T: pickTool(m_tool == QLatin1String("realm") ? QStringLiteral("pan") : QStringLiteral("realm"), QPoint()); m_dockFromRing = false; placeDock(); break;
    case Qt::Key_K: pickTool(m_tool == QLatin1String("build") ? QStringLiteral("pan") : QStringLiteral("build"), QPoint()); m_dockFromRing = false; placeDock(); break;
    case Qt::Key_N: pickTool(m_tool == QLatin1String("names") ? QStringLiteral("pan") : QStringLiteral("names"), QPoint()); m_dockFromRing = false; placeDock(); break;
    case Qt::Key_S: m_canvas->opts().sprites = !m_canvas->opts().sprites; QSettings().setValue(QStringLiteral("carto/sprites"), m_canvas->opts().sprites); m_canvas->optsChanged(); applyUiMode(); break;
    case Qt::Key_R: pickTool(m_tool == QLatin1String("ruler") ? QStringLiteral("pan") : QStringLiteral("ruler"), QPoint()); break;
    case Qt::Key_X: pickTool(m_tool == QLatin1String("explore") ? QStringLiteral("pan") : QStringLiteral("explore"), QPoint()); m_dockFromRing = false; break;
    case Qt::Key_Delete: case Qt::Key_Backspace: deleteSelected(); break;
    case Qt::Key_G: m_gen->setVisible(!m_gen->isVisible()); positionOverlays(); syncUi(); break;
    case Qt::Key_F: m_canvas->fit(); break;
    case Qt::Key_Q:
        if (m_tool == QLatin1String("build") && m_canvas->selectedArt()) rotateSelected(-M_PI / 12);
        else m_canvas->setView(m_canvas->angle() - 15 * DEG, m_canvas->pitch(), false);
        break;
    case Qt::Key_E:
        if (m_tool == QLatin1String("build") && m_canvas->selectedArt()) rotateSelected(M_PI / 12);
        else m_canvas->setView(m_canvas->angle() + 15 * DEG, m_canvas->pitch(), false);
        break;
    case Qt::Key_P: setPlaying(!m_playTimer->isActive()); break;
    case Qt::Key_V: {
        const bool mira = m_canvas->terrainStyle() == QLatin1String("mira");
        m_styleGroup->button(mira ? 1 : 0)->setChecked(true);
        m_canvas->setTerrainStyle(mira ? QStringLiteral("rich") : QStringLiteral("mira"));
        break;
    }
    case Qt::Key_BracketLeft: m_sizeSlider->setValue(qRound(m_sizeSlider->value() * 0.85)); break;
    case Qt::Key_BracketRight: m_sizeSlider->setValue(qRound(m_sizeSlider->value() / 0.85 + 1)); break;
    case Qt::Key_Escape:
        m_gen->hide(); m_settings->hide(); m_ring->hide(); m_export->hide();
        m_canvas->cancelAction();
        m_canvas->clearRuler();
        syncUi();
        break;
    default:
        if (k >= Qt::Key_1 && k <= Qt::Key_8) {
            const Brush b = static_cast<Brush>(k - Qt::Key_1);
            setClimate(isClimateBrush(b) && m_canvas->showClimate());
            setBrush(b);
            m_dockFromRing = false;
            placeDock();
            break;
        }
        QWidget::keyPressEvent(e);
    }
}

bool Panel::eventFilter(QObject* o, QEvent* e)
{
    if (o == m_title && e->type() == QEvent::MouseButtonPress) {
        m_title->hide();
        m_titleEdit->setText(m_title->text());
        m_titleEdit->show();
        m_titleEdit->setFocus();
        m_titleEdit->selectAll();
        positionOverlays();
        return true;
    }
    // linha da lista de territórios: clicar (ou entrar no nome) seleciona
    if (o->property("realmRow").isValid() && (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::FocusIn)) {
        const int i = o->property("realmRow").toInt();
        if (m_canvas->opts().realmSel != i) { m_canvas->opts().realmSel = i; m_canvas->update(); syncUi(); }
    }
    // a doca arrasta pelo título do cartão
    if (o == m_brushTitle || o == m_realmTitle || o == m_buildTitle || o == m_namesTitle) {
        if (e->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(e);
            m_dockDragging = true;
            m_dockGrab = me->globalPosition().toPoint() - m_dock->pos();
            return true;
        }
        if (e->type() == QEvent::MouseMove && m_dockDragging) {
            auto* me = static_cast<QMouseEvent*>(e);
            m_dockUser = true;
            m_dockAt = me->globalPosition().toPoint() - m_dockGrab;
            placeDock();
            return true;
        }
        if (e->type() == QEvent::MouseButtonRelease) { m_dockDragging = false; return true; }
    }
    return QWidget::eventFilter(o, e);
}

} // namespace carto
