#include "GalaxyNewGalaxy.h"
#include "GalaxyPanel.h"
#include "GalaxyPresets.h"

#include "../Theme.h"
#include "../carto/CartoIcons.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
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
QString lifeLabel(double v) { return v <= 0 ? Panel::tr("Realista") : v < 0.35 ? Panel::tr("Alguns a mais") : v < 0.7 ? Panel::tr("Muitos") : Panel::tr("Cheio de vida"); }
inline bool spiralStyle(Style s) { return s == Style::Espiral || s == Style::Barrada; }
}

NewGalaxy::NewGalaxy(QWidget* parent) : QWidget(parent)
{
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
    m_previewBox->setMinimumSize(360, 360);
    left->addWidget(m_previewBox, 1);
    m_info = new QLabel(m_card);
    m_info->setObjectName(QStringLiteral("cartoNote"));
    m_info->setAlignment(Qt::AlignCenter);
    left->addWidget(m_info);
    root->addLayout(left, 3);
    auto* scroll = new QScrollArea(m_card);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }"));
    scroll->viewport()->setAutoFillBackground(false);
    auto* form = new QWidget(scroll);
    auto* f = new QVBoxLayout(form);
    f->setContentsMargins(0, 0, 8, 0);
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
    lh->addWidget(cap(tr("MUNDOS HABITÁVEIS"), form));
    lh->addStretch();
    m_lifeV = new QLabel(form);
    m_lifeV->setObjectName(QStringLiteral("cartoValue"));
    lh->addWidget(m_lifeV);
    f->addLayout(lh);
    m_life = new QSlider(Qt::Horizontal, form);
    m_life->setRange(0, 100);
    connect(m_life, &QSlider::valueChanged, this, [this](int v) { if (m_loading) return; m_d.life = v / 100.0; m_lifeV->setText(lifeLabel(m_d.life)); });
    f->addWidget(m_life);
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
        const QString name = m_nameText.trimmed();
        const Mode md = m_mode;
        hide();
        ++m_tok;
        m_timer.stop();
        if (accepted) accepted(o, name, md);
    });
    f->addWidget(m_go);
    scroll->setWidget(form);
    root->addWidget(scroll, 2);
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
    if (!regen) d.seed = QRandomGenerator::global()->bounded(1000000000);
    m_d = d;
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
    m_lifeV->setText(lifeLabel(m_d.life));
    if (!m_seed->hasFocus()) m_seed->setText(QString::number(m_d.seed));
    m_loading = false;
}
void NewGalaxy::change() { sync(); m_timer.start(); }
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
