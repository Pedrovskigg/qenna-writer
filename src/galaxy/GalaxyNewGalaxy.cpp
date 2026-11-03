#include "GalaxyNewGalaxy.h"
#include "GalaxyHab.h"
#include "GalaxyPanel.h"
#include "GalaxyPresets.h"

#include "../Theme.h"
#include "../carto/CartoIcons.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QGridLayout>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QThreadPool>
#include <QToolButton>
#include <QVBoxLayout>

namespace galaxy {

class NewGalaxyPreview : public QWidget {
public:
    explicit NewGalaxyPreview(NewGalaxy* o) : QWidget(o), m_o(o) {}
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()), 12, 12);
        p.setClipPath(clip);
        p.fillRect(rect(), Qt::black);
        if (!m_o->m_img.isNull()) {
            const int S = std::min(width(), height());
            p.drawImage(QRect((width() - S) / 2, (height() - S) / 2, S, S), m_o->m_img);
        }
    }
private:
    NewGalaxy* m_o;
};

namespace {
QString sizeLabel(Size s)
{
    switch (s) {
    case Size::Pequena: return Panel::tr("Pequena");
    case Size::Media: return Panel::tr("Média");
    case Size::Grande: return Panel::tr("Grande");
    default: return Panel::tr("Imensa");
    }
}
QString lifeLabel(double v) { return v <= 0 ? Panel::tr("Cruel") : v < 0.35 ? Panel::tr("Dura") : v < 0.7 ? Panel::tr("Generosa") : Panel::tr("Cheia de vida"); }
double mixRound(double v) { return std::floor(v * 10 + 0.5) / 10; }
QString mixText(double v) { return QLocale().toString(mixRound(v), 'g', 4); }
// Inteiros que somam 100, respeitando a proporção e sem zerar classe que existe.
std::vector<double> mixFromCounts(const std::array<int, 7>& c)
{
    int sum = 0;
    for (int n = 1; n <= 6; n++) sum += c[size_t(n)];
    if (!sum) return { 0, 2, 3, 2, 43, 31, 19 };
    std::vector<double> m(7, 0);
    std::vector<std::pair<double, int>> rest;
    int left = 100;
    for (int n = 1; n <= 6; n++) {
        const double x = (double(c[size_t(n)]) / sum) * 100;
        m[size_t(n)] = c[size_t(n)] ? std::max(1.0, std::floor(x)) : 0;
        left -= int(m[size_t(n)]);
        rest.push_back({ x - std::floor(x), n });
    }
    std::stable_sort(rest.begin(), rest.end(), [](const std::pair<double, int>& a, const std::pair<double, int>& b) { return a.first > b.first; });
    for (int i = 0; left > 0; i = (i + 1) % 6) if (c[size_t(rest[size_t(i)].second)]) { m[size_t(rest[size_t(i)].second)]++; left--; }
    for (int n = 6; left < 0; n = n == 1 ? 6 : n - 1) if (m[size_t(n)] > 1) { m[size_t(n)]--; left++; }
    return m;
}
inline bool spiralStyle(Style s) { return s == Style::Espiral || s == Style::Barrada; }
}

// A barra de arrastar: cada limite divide duas classes. Passar por cima de outro limite vai empurrando,
// e limites empilhados saem pela direção do arrasto, pra classe zerada poder voltar.
class MixBar : public QWidget {
public:
    explicit MixBar(NewGalaxy* o, QWidget* parent) : QWidget(parent), m_o(o)
    {
        setFixedHeight(36);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
    }
    std::array<double, 5> cuts() const { std::array<double, 5> c {}; double a = 0; for (int n = 1; n <= 5; n++) { a += m_o->m_d.mix[size_t(n)]; c[size_t(n - 1)] = mixRound(a); } return c; }
    void setCuts(const std::array<double, 5>& c)
    {
        double last = 0;
        for (int n = 1; n <= 5; n++) { m_o->m_d.mix[size_t(n)] = mixRound(c[size_t(n - 1)] - last); last = c[size_t(n - 1)]; }
        m_o->m_d.mix[6] = mixRound(100 - last);
    }
protected:
    double inset() const { return 8; }
    double xOf(double pct) const { return inset() + (width() - 2 * inset()) * pct / 100; }
    double pctOf(double x) const { return clamp((x - inset()) / (width() - 2 * inset()) * 100, 0, 100); }
    void moveCut(int i, double v)
    {
        auto c = cuts();
        c[size_t(i)] = v;
        for (int j = i + 1; j < 5; j++) c[size_t(j)] = std::max(c[size_t(j)], v);
        for (int j = i - 1; j >= 0; j--) c[size_t(j)] = std::min(c[size_t(j)], v);
        setCuts(c);
        m_o->mixMoved();
    }
    void paintEvent(QPaintEvent*) override
    {
        if (m_o->m_d.mix.size() != 7) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF tr(inset(), 3, width() - 2 * inset(), height() - 6);
        QPainterPath clip;
        clip.addRoundedRect(tr, 7, 7);
        p.save();
        p.setClipPath(clip);
        QFont f(QStringLiteral("IBM Plex Mono")); f.setPixelSize(11); f.setWeight(QFont::Bold);
        p.setFont(f);
        double left = 0;
        for (int n = 1; n <= 6; n++) {
            const double w = m_o->m_d.mix[size_t(n)];
            if (w > 0) {
                const QRectF seg(xOf(left), tr.top(), xOf(left + w) - xOf(left), tr.height());
                p.fillRect(seg, habColor(n));
                if (w >= 5) { p.setPen(QColor(0x0a, 0x0d, 0x14)); p.drawText(seg, Qt::AlignCenter, QString::fromLatin1(habRoman(n))); }
            }
            left += w;
        }
        p.restore();
        const auto c = cuts();
        for (int i = 0; i < 5; i++) {
            const double x = xOf(c[size_t(i)]);
            const QRectF h(x - 2, 0, 4, height());
            p.setPen(QPen(QColor(5, 8, 14, 190), 1));
            p.setBrush(i == m_active || i == m_hover ? Theme::toColor(Theme::accentDefault()) : QColor(0xf4, 0xf7, 0xfc));
            p.drawRoundedRect(h, 2, 2);
        }
    }
    int cutAt(double x) const
    {
        const auto c = cuts();
        int best = -1;
        double bd = 9;
        for (int i = 0; i < 5; i++) { const double d = std::fabs(xOf(c[size_t(i)]) - x); if (d < bd) { bd = d; best = i; } }
        return best;
    }
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton || m_o->m_d.mix.size() != 7) return;
        setFocus();
        const int i = cutAt(e->position().x());
        if (i < 0) return;
        const auto c = cuts();
        m_stack.clear();
        for (int j = 0; j < 5; j++) if (std::fabs(c[size_t(j)] - c[size_t(i)]) < 0.05) m_stack.push_back(j);
        m_active = m_stack.size() == 1 ? i : -1;
        m_dragging = true;
        m_startX = e->position().x();
        update();
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        const double x = e->position().x();
        if (!m_dragging) {
            const int h = m_o->m_d.mix.size() == 7 ? cutAt(x) : -1;
            if (h != m_hover) { m_hover = h; setCursor(h >= 0 ? Qt::SizeHorCursor : Qt::ArrowCursor); update(); }
            return;
        }
        // Limites empilhados: a direção do arrasto escolhe qual deles sai.
        if (m_active < 0) {
            if (std::fabs(x - m_startX) < 2) return;
            m_active = x > m_startX ? m_stack.back() : m_stack.front();
        }
        moveCut(m_active, std::floor(pctOf(x) + 0.5));
    }
    void mouseReleaseEvent(QMouseEvent*) override { m_dragging = false; m_active = -1; update(); }
    void leaveEvent(QEvent*) override { if (m_hover >= 0) { m_hover = -1; update(); } }
    void keyPressEvent(QKeyEvent* e) override
    {
        const int step = e->key() == Qt::Key_Right ? 1 : e->key() == Qt::Key_Left ? -1 : 0;
        if (e->key() == Qt::Key_Tab) { m_key = (m_key + 1) % 5; m_hover = m_key; update(); return; }
        if (!step || m_o->m_d.mix.size() != 7) { QWidget::keyPressEvent(e); return; }
        m_hover = m_key;
        moveCut(m_key, clamp(std::floor(cuts()[size_t(m_key)] + 0.5) + step, 0, 100));
    }
    bool focusNextPrevChild(bool) override { return false; }
private:
    NewGalaxy* m_o;
    std::vector<int> m_stack;
    int m_active = -1, m_hover = -1, m_key = 0;
    bool m_dragging = false;
    double m_startX = 0;
};

NewGalaxy::NewGalaxy(QWidget* parent) : QWidget(parent)
{
    m_mixTimer.setSingleShot(true);
    connect(&m_mixTimer, &QTimer::timeout, this, &NewGalaxy::previewMix);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground, false);
    m_timer.setSingleShot(true);
    m_timer.setInterval(180);
    connect(&m_timer, &QTimer::timeout, this, &NewGalaxy::preview);
    build();
    hide();
}
void NewGalaxy::build()
{
    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("cartoCard"));
    auto* root = new QHBoxLayout(m_card);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(18);
    auto* left = new QVBoxLayout();
    left->setSpacing(6);
    m_previewBox = new NewGalaxyPreview(this);
    m_previewBox->setParent(m_card);
    m_previewBox->setMinimumSize(300, 300);
    left->addWidget(m_previewBox, 1);
    m_info = new QLabel(m_card);
    m_info->setObjectName(QStringLiteral("cartoNote"));
    m_info->setAlignment(Qt::AlignCenter);
    left->addWidget(m_info);
    root->addLayout(left, 1);
    auto* scroll = new QScrollArea(m_card);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }"));
    scroll->viewport()->setAutoFillBackground(false);
    auto* form = new QWidget(scroll);
    auto* f = new QVBoxLayout(form);
    f->setContentsMargins(0, 0, 14, 0);
    f->setSpacing(10);
    auto* head = new QHBoxLayout();
    m_title = new QLabel(form);
    m_title->setObjectName(QStringLiteral("cartoCardTitle"));
    m_title->setStyleSheet(QStringLiteral("font-size: 22px;"));
    head->addWidget(m_title);
    head->addStretch();
    m_close = new QToolButton(form);
    m_close->setObjectName(QStringLiteral("cartoBtn"));
    m_close->setFixedSize(28, 28);
    m_close->setIconSize(QSize(16, 16));
    m_close->setCursor(Qt::PointingHandCursor);
    m_close->setToolTip(tr("Fechar"));
    connect(m_close, &QToolButton::clicked, this, &NewGalaxy::dismiss);
    head->addWidget(m_close);
    f->addLayout(head);
    // Terra ou Galáxia
    auto* tabs = new QHBoxLayout();
    tabs->setSpacing(4);
    const auto tab = [&](const QString& text) {
        auto* b = new QToolButton(form);
        b->setObjectName(QStringLiteral("cartoSeg"));
        b->setText(text);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        tabs->addWidget(b);
        return b;
    };
    m_tabEarth = tab(tr("Terra"));
    m_tabGal = tab(tr("Galáxia"));
    m_tabGal->setChecked(true);
    connect(m_tabEarth, &QToolButton::clicked, this, [this]() { m_tabEarth->setChecked(false); if (wantEarth) wantEarth(); });
    connect(m_tabGal, &QToolButton::clicked, this, [this]() { m_tabGal->setChecked(true); });
    f->addLayout(tabs);
    const auto cap = [&](const QString& t, QWidget* parent) { auto* c = new QLabel(t, parent); c->setObjectName(QStringLiteral("cartoCaption")); return c; };
    f->addWidget(cap(tr("GALÁXIA PRONTA"), form));
    m_preset = new QComboBox(form);
    m_preset->setObjectName(QStringLiteral("cartoCombo"));
    m_preset->addItem(tr("Nenhuma, vou inventar a minha"), QString());
    QStringList groups;
    for (const Preset& p : presets()) { const QString g = QCoreApplication::translate("galaxy::Preset", p.group); if (!groups.contains(g)) groups << g; }
    for (const QString& g : groups) {
        m_preset->insertSeparator(m_preset->count());
        for (const Preset& p : presets()) if (QCoreApplication::translate("galaxy::Preset", p.group) == g) m_preset->addItem(QCoreApplication::translate("galaxy::Preset", p.name), QString::fromLatin1(p.id));
    }
    connect(m_preset, &QComboBox::activated, this, [this]() {
        if (m_loading) return;
        const QString id = m_preset->currentData().toString();
        const Preset* P = presetById(id);
        const Preset* old = presetById(m_d.preset);
        if (old && m_nameText == QCoreApplication::translate("galaxy::Preset", old->name)) m_nameText.clear();
        m_d.preset = P ? id : QString();
        if (P) {
            m_d.style = P->style;
            if (P->arms) m_d.arms = P->arms;
            m_d.size = P->size;
            m_d.count = P->count;
            m_d.seed = P->seed;
            if (m_nameText.isEmpty()) m_nameText = QCoreApplication::translate("galaxy::Preset", P->name);
        }
        change();
    });
    f->addWidget(m_preset);
    m_presetNote = new QLabel(form);
    m_presetNote->setObjectName(QStringLiteral("cartoNote"));
    m_presetNote->setWordWrap(true);
    f->addWidget(m_presetNote);
    f->addWidget(cap(tr("NOME"), form));
    m_name = new QLineEdit(form);
    m_name->setObjectName(QStringLiteral("cartoField"));
    m_name->setPlaceholderText(tr("Sem nome, ela se chama pelo estilo"));
    connect(m_name, &QLineEdit::textEdited, this, [this](const QString& t) { m_nameText = t; });
    f->addWidget(m_name);
    f->addWidget(cap(tr("ESTILO"), form));
    m_style = new QComboBox(form);
    m_style->setObjectName(QStringLiteral("cartoCombo"));
    for (Style s : { Style::Espiral, Style::Barrada, Style::Eliptica, Style::Irregular, Style::Anel }) m_style->addItem(QCoreApplication::translate("galaxy::Core", styleLabel(s)), int(s));
    connect(m_style, &QComboBox::activated, this, [this]() { if (m_loading) return; leavePreset(); m_d.style = Style(m_style->currentData().toInt()); change(); });
    f->addWidget(m_style);
    m_armsRow = new QWidget(form);
    auto* ar = new QVBoxLayout(m_armsRow);
    ar->setContentsMargins(0, 0, 0, 0);
    ar->setSpacing(4);
    auto* arh = new QHBoxLayout();
    arh->addWidget(cap(tr("BRAÇOS"), m_armsRow));
    arh->addStretch();
    m_armsV = new QLabel(m_armsRow);
    m_armsV->setObjectName(QStringLiteral("cartoValue"));
    arh->addWidget(m_armsV);
    ar->addLayout(arh);
    m_arms = new QSlider(Qt::Horizontal, m_armsRow);
    m_arms->setRange(2, 6);
    connect(m_arms, &QSlider::valueChanged, this, [this](int v) { if (m_loading) return; leavePreset(); m_d.arms = v; change(); });
    ar->addWidget(m_arms);
    f->addWidget(m_armsRow);
    f->addWidget(cap(tr("TAMANHO"), form));
    auto* szr = new QHBoxLayout();
    szr->setSpacing(4);
    for (Size s : { Size::Pequena, Size::Media, Size::Grande, Size::Imensa }) {
        auto* b = new QToolButton(form);
        b->setObjectName(QStringLiteral("cartoSeg"));
        b->setText(sizeLabel(s));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(b, &QToolButton::clicked, this, [this, s]() { leavePreset(); m_d.size = s; m_d.count = sizeSystems(s); change(); });
        m_sizes.insert(int(s), b);
        szr->addWidget(b);
    }
    f->addLayout(szr);
    auto* cnh = new QHBoxLayout();
    cnh->addWidget(cap(tr("SISTEMAS"), form));
    cnh->addStretch();
    m_count = new QSpinBox(form);
    m_count->setObjectName(QStringLiteral("cartoSpin"));
    m_count->setRange(1, 800);
    connect(m_count, &QSpinBox::editingFinished, this, [this]() { if (m_loading || m_count->value() == m_d.count) return; m_d.count = m_count->value(); change(); });
    cnh->addWidget(m_count);
    f->addLayout(cnh);
    auto* lh = new QHBoxLayout();
    lh->addWidget(cap(tr("MISTURA DE MUNDOS"), form));
    lh->addStretch();
    m_lifeV = new QLabel(form);
    m_lifeV->setObjectName(QStringLiteral("cartoValue"));
    lh->addWidget(m_lifeV);
    f->addLayout(lh);
    m_life = new QSlider(Qt::Horizontal, form);
    m_life->setRange(0, 100);
    connect(m_life, &QSlider::valueChanged, this, [this](int v) { if (m_loading) return; m_d.life = v / 100.0; m_lifeV->setText(lifeLabel(m_d.life)); m_d.mix.clear(); m_mixTimer.start(120); });
    f->addWidget(m_life);
    m_handBox = new QWidget(form);
    {
        auto* hb = new QVBoxLayout(m_handBox);
        hb->setContentsMargins(0, 0, 0, 0);
        hb->setSpacing(8);
        m_mixBar = new MixBar(this, m_handBox);
        hb->addWidget(m_mixBar);
        auto* grid = new QGridLayout();
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(6);
        for (int n = 1; n <= 6; n++) {
            auto* cell = new QWidget(m_handBox);
            cell->setToolTip(habDesc(n));
            auto* h = new QHBoxLayout(cell);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(6);
            h->addWidget(habPill(n, cell));
            auto* col = new QVBoxLayout();
            col->setSpacing(0);
            auto* nm = new QLabel(habName(n), cell);
            nm->setObjectName(QStringLiteral("cartoValue"));
            col->addWidget(nm);
            m_mixCount[n] = new QLabel(cell);
            m_mixCount[n]->setObjectName(QStringLiteral("cartoNote"));
            m_mixCount[n]->setStyleSheet(QStringLiteral("font-size: 10px;"));
            col->addWidget(m_mixCount[n]);
            h->addLayout(col, 1);
            m_mixIn[n] = new QLineEdit(cell);
            m_mixIn[n]->setObjectName(QStringLiteral("cartoField"));
            m_mixIn[n]->setFixedWidth(46);
            m_mixIn[n]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            connect(m_mixIn[n], &QLineEdit::editingFinished, this, [this, n]() {
                if (m_loading || m_d.mix.size() != 7) return;
                QString t = m_mixIn[n]->text();
                t.replace(QLatin1Char(','), QLatin1Char('.'));
                bool ok = false;
                const double v = t.toDouble(&ok);
                if (ok && std::fabs(mixRound(v) - m_d.mix[size_t(n)]) > 0.01) { setMixShare(n, v); mixMoved(); }
                else syncMix();
            });
            h->addWidget(m_mixIn[n]);
            auto* pc = new QLabel(QStringLiteral("%"), cell);
            pc->setObjectName(QStringLiteral("cartoNote"));
            h->addWidget(pc);
            grid->addWidget(cell, (n - 1) / 2, (n - 1) % 2);
        }
        hb->addLayout(grid);
    }
    m_handBox->hide();
    f->addWidget(m_handBox);
    m_natBar = new HabBar(form);
    f->addWidget(m_natBar);
    m_mixText = new QLabel(form);
    m_mixText->setObjectName(QStringLiteral("cartoNote"));
    m_mixText->setWordWrap(true);
    f->addWidget(m_mixText);
    m_handCheck = new QToolButton(form);
    m_handCheck->setObjectName(QStringLiteral("cartoCheck"));
    m_handCheck->setCheckable(true);
    m_handCheck->setText(tr("Definir o espaço de cada classe"));
    m_handCheck->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_handCheck->setCursor(Qt::PointingHandCursor);
    m_handCheck->setFocusPolicy(Qt::NoFocus);
    connect(m_handCheck, &QToolButton::clicked, this, [this](bool on) { setHand(on); });
    f->addWidget(m_handCheck);
    f->addWidget(cap(tr("SEMENTE"), form));
    auto* sr = new QHBoxLayout();
    sr->setSpacing(6);
    m_seed = new QLineEdit(form);
    m_seed->setObjectName(QStringLiteral("cartoField"));
    connect(m_seed, &QLineEdit::editingFinished, this, [this]() { if (m_loading) return; const uint32_t v = m_seed->text().toUInt(); if (v == m_d.seed) return; m_d.seed = v ? v : 1; change(); });
    sr->addWidget(m_seed, 1);
    auto* dice = new QPushButton(tr("Sortear"), form);
    dice->setObjectName(QStringLiteral("cartoSize"));
    dice->setCursor(Qt::PointingHandCursor);
    connect(dice, &QPushButton::clicked, this, [this]() { m_d.seed = QRandomGenerator::global()->bounded(1000000000); change(); });
    sr->addWidget(dice);
    f->addLayout(sr);
    m_note = new QLabel(form);
    m_note->setObjectName(QStringLiteral("cartoNote"));
    m_note->setWordWrap(true);
    f->addWidget(m_note);
    f->addStretch();
    m_go = new QPushButton(form);
    m_go->setObjectName(QStringLiteral("cartoGo"));
    m_go->setCursor(Qt::PointingHandCursor);
    connect(m_go, &QPushButton::clicked, this, [this]() {
        GalOpts o = m_d;
        if (!m_hand) o.mix.clear();
        const QString name = m_nameText.trimmed();
        const Mode md = m_mode;
        hide();
        ++m_tok;
        m_timer.stop();
        if (accepted) accepted(o, name, md);
    });
    f->addWidget(m_go);
    scroll->setWidget(form);
    root->addWidget(scroll, 1);
}
void NewGalaxy::applyTheme()
{
    m_close->setIcon(carto::icon(QStringLiteral("close"), Theme::toColor(Theme::textMuted()), Theme::toColor(Theme::accentDefault()), 16));
}
void NewGalaxy::open(Mode mode, const GalOpts* current, int currentCount)
{
    m_mode = mode;
    GalOpts d;
    if (current) d = *current;
    const bool regen = mode == Mode::Regen;
    d.preset = regen && current ? current->preset : QString();
    d.count = regen ? currentCount : sizeSystems(d.size);
    if (!regen) { d.seed = QRandomGenerator::global()->bounded(1000000000); d.mix.clear(); }
    m_d = d;
    m_hand = d.mix.size() == 7;
    m_hasNatural = false; m_worlds = 0; m_touched.clear();
    m_nameText.clear();
    m_title->setText(regen ? tr("Regenerar galáxia") : tr("Nova galáxia"));
    m_note->setText(regen ? tr("Regenerar troca a galáxia inteira, e o que foi construído e escrito nela vai junto. Logo depois dá pra desfazer.") : QString());
    m_note->setVisible(regen);
    m_go->setText(regen ? tr("Regenerar galáxia") : tr("Criar galáxia"));
    m_tabEarth->setVisible(!regen);
    m_tabGal->setVisible(!regen);
    m_close->setVisible(mode != Mode::First);
    applyTheme();
    setGeometry(parentWidget()->rect());
    sync();
    show();
    raise();
    setFocus();
    preview();
    previewMix();
}
void NewGalaxy::dismiss()
{
    if (m_mode == Mode::First) return;
    hide();
    ++m_tok;
    m_timer.stop();
    if (dismissed) dismissed();
}
void NewGalaxy::sync()
{
    m_loading = true;
    const Preset* P = presetById(m_d.preset);
    const int pi = m_preset->findData(m_d.preset);
    m_preset->setCurrentIndex(std::max(0, pi));
    m_presetNote->setVisible(P != nullptr);
    m_presetNote->setText(P ? QCoreApplication::translate("galaxy::Preset", P->desc) : QString());
    m_style->setCurrentIndex(std::max(0, m_style->findData(int(m_d.style))));
    if (!m_name->hasFocus()) m_name->setText(m_nameText);
    const bool spiral = spiralStyle(m_d.style);
    m_arms->setEnabled(spiral);
    m_arms->setValue(m_d.arms);
    m_armsV->setText(spiral ? QString::number(m_d.arms) : QStringLiteral("—"));
    for (auto it = m_sizes.begin(); it != m_sizes.end(); ++it) it.value()->setChecked(it.key() == int(m_d.size));
    m_count->setValue(m_d.count);
    m_life->setValue(qRound(m_d.life * 100));
    m_life->setVisible(!m_hand);
    m_lifeV->setText(m_hand ? tr("Na mão") : lifeLabel(m_d.life));
    m_handBox->setVisible(m_hand);
    m_natBar->setVisible(!m_hand);
    {
        const QColor muted = Theme::toColor(Theme::textMuted()), a = Theme::toColor(Theme::accentDefault());
        m_handCheck->setChecked(m_hand);
        m_handCheck->setIcon(carto::icon(m_hand ? QStringLiteral("check") : QStringLiteral("box"), muted, a));
    }
    if (!m_seed->hasFocus()) m_seed->setText(QString::number(m_d.seed));
    m_loading = false;
}
void NewGalaxy::change() { sync(); syncMix(); m_timer.start(); m_mixTimer.start(180); }
// A mistura sai do mesmo gerador, numa amostra da galáxia, e é contada pro tamanho inteiro.
void NewGalaxy::previewMix()
{
    const int tok = ++m_mixTok;
    GalaxyOpts g;
    g.seed = m_d.seed; g.style = m_d.style; g.size = m_d.size; g.arms = m_d.arms; g.life = m_d.life; g.count = std::min(m_d.count, 120);
    if (const Preset* p = presetById(m_d.preset)) g.tune = p->tune;
    if (m_hand && m_d.mix.size() == 7) g.mix = m_d.mix;
    const double k = double(m_d.count) / g.count;
    const bool hand = m_hand;
    QPointer<NewGalaxy> self(this);
    QThreadPool::globalInstance()->start([self, g, tok, k, hand]() {
        auto gal = makeGalaxy(g);
        std::array<int, 7> c {};
        for (const auto& s : gal->systems) for (const auto& p : s->bodies) {
            if (p->body != BodyKind::Planeta) continue;
            c[size_t(habInfo(*p, s.get(), nullptr).cls)]++;
            for (const auto& m : p->moons) c[size_t(habInfo(*m, s.get(), p.get()).cls)]++;
        }
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, c, tok, k, hand]() {
            if (!self || tok != self->m_mixTok) return;
            int sum = 0;
            for (int n = 1; n <= 6; n++) sum += c[size_t(n)];
            self->m_worlds = int(sum * k + 0.5);
            if (hand) {
                self->m_mixText->setText(Panel::tr("Cerca de %n mundo(s) nesta galáxia. Estrelas e órbitas não mudam: os mundos que mais combinam com cada classe são adaptados a ela.", "", self->m_worlds));
                self->syncMix();
                return;
            }
            self->m_natural = c; self->m_hasNatural = true;
            self->m_natBar->setCounts(c);
            const int roomy = int((c[1] + c[2]) * k + 0.5);
            self->m_mixText->setText(Panel::tr("Cerca de %n mundo(s) de classe I ou II, %1 de III ou IV e %2 de V ou VI.", "", roomy).arg(nf((c[3] + c[4]) * k), nf((c[5] + c[6]) * k)));
        }, Qt::QueuedConnection);
    });
}
// Ligar parte da mistura que estava na tela; desligar e ligar de novo guarda o que foi definido.
void NewGalaxy::setHand(bool on)
{
    m_hand = on;
    if (on && m_d.mix.size() != 7) { m_d.mix = m_hasNatural ? mixFromCounts(m_natural) : std::vector<double> { 0, 2, 3, 2, 43, 31, 19 }; m_touched.clear(); }
    sync();
    syncMix();
    previewMix();
}
void NewGalaxy::syncMix()
{
    if (!m_hand || m_d.mix.size() != 7) return;
    const bool was = m_loading;
    m_loading = true;
    for (int n = 1; n <= 6; n++) {
        if (!m_mixIn[n]->hasFocus()) m_mixIn[n]->setText(mixText(m_d.mix[size_t(n)]));
        const int v = int(m_worlds * m_d.mix[size_t(n)] / 100 + 0.5);
        m_mixCount[n]->setText(m_d.mix[size_t(n)] > 0 && m_worlds ? tr("≈ %n mundo(s)", "", v) : tr("nenhum"));
    }
    m_loading = was;
    m_mixBar->update();
}
void NewGalaxy::mixMoved() { syncMix(); m_mixTimer.start(220); }
// Digitar numa classe tira dos valores que ainda não foram digitados; só mexe nos outros se não der.
void NewGalaxy::setMixShare(int n, double value)
{
    std::vector<double>& m = m_d.mix;
    const double v = clamp(mixRound(value), 0, 100);
    double delta = v - m[size_t(n)];
    m[size_t(n)] = v;
    m_touched.erase(std::remove(m_touched.begin(), m_touched.end(), n), m_touched.end());
    m_touched.push_back(n);
    if (m_touched.size() > 5) m_touched.erase(m_touched.begin());
    std::vector<int> fresh, held;
    for (int x = 1; x <= 6; x++) { if (x == n) continue; (std::find(m_touched.begin(), m_touched.end(), x) != m_touched.end() ? held : fresh).push_back(x); }
    for (const std::vector<int>* group : { &fresh, &held }) {
        if (group->empty() || std::fabs(delta) < 0.01) continue;
        double sum = 0;
        for (int x : *group) sum += m[size_t(x)];
        if (delta > 0) {
            const double take = std::min(delta, sum);
            if (sum > 0) for (int x : *group) m[size_t(x)] -= take * (m[size_t(x)] / sum);
            delta -= take;
        } else {
            for (int x : *group) m[size_t(x)] += sum > 0 ? -delta * (m[size_t(x)] / sum) : -delta / double(group->size());
            delta = 0;
        }
    }
    double total = 0;
    int big = 0;
    for (int x = 1; x <= 6; x++) { m[size_t(x)] = mixRound(m[size_t(x)]); total += m[size_t(x)]; if (x != n && (!big || m[size_t(x)] > m[size_t(big)])) big = x; }
    if (big) m[size_t(big)] = mixRound(m[size_t(big)] + 100 - total);
}
// Mexer na forma transforma a pronta numa galáxia sua; semente, quantidade e vida não.
void NewGalaxy::leavePreset()
{
    const Preset* P = presetById(m_d.preset);
    if (P && m_nameText == QCoreApplication::translate("galaxy::Preset", P->name)) m_nameText.clear();
    m_d.preset.clear();
}
// A prévia sai do mesmo gerador, numa versão leve.
void NewGalaxy::preview()
{
    const int tok = ++m_tok;
    m_info->setText(tr("Gerando a prévia…"));
    GalaxyOpts g;
    g.seed = m_d.seed; g.style = m_d.style; g.size = m_d.size; g.arms = m_d.arms; g.life = 0; g.count = std::min(m_d.count, 40);
    if (const Preset* p = presetById(m_d.preset)) g.tune = p->tune;
    const int count = m_d.count;
    QPointer<NewGalaxy> self(this);
    QThreadPool::globalInstance()->start([self, g, tok, count]() {
        auto gal = makeGalaxy(g);
        const int T = 680;
        const std::vector<uint8_t> px = buildGalaxyImage(*gal, T);
        QImage img(T, T, QImage::Format_RGB32);
        for (int y = 0; y < T; y++) {
            QRgb* d = reinterpret_cast<QRgb*>(img.scanLine(y));
            const uint8_t* s = px.data() + size_t(y) * T * 4;
            for (int x = 0; x < T; x++) d[x] = qRgb(s[x * 4], s[x * 4 + 1], s[x * 4 + 2]);
        }
        const double ly = gal->ly;
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, img, tok, ly, count]() {
            if (!self || tok != self->m_tok) return;
            self->m_img = img;
            self->m_info->setText(Panel::tr("%1 anos-luz · %2 sistemas").arg(nf(ly), nf(count)));
            self->m_previewBox->update();
        }, Qt::QueuedConnection);
    });
}
void NewGalaxy::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 150));
}
void NewGalaxy::resizeEvent(QResizeEvent*)
{
    const int w = std::min(980, width() - 40), h = std::min(680, height() - 40);
    m_card->setGeometry((width() - w) / 2, (height() - h) / 2, w, h);
}
void NewGalaxy::mousePressEvent(QMouseEvent*) {}
void NewGalaxy::wheelEvent(QWheelEvent*) {}
void NewGalaxy::keyPressEvent(QKeyEvent* e) { if (e->key() == Qt::Key_Escape) { dismiss(); return; } QWidget::keyPressEvent(e); }

} // namespace galaxy
