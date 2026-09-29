#include "QuickCoverWidgets.h"

#include "ColorPopover.h"
#include "SheetDialog.h"
#include "Theme.h"
#include "TimelineTracksTypes.h"

#include <QButtonGroup>
#include <QEnterEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QStackedWidget>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Tracks;

namespace {

constexpr int kCoverW = 240;
constexpr int kCoverH = 360;

// Ícones em SVG com "currentColor", pintados na cor pedida.
QIcon svgIcon(const QString& svg, const QColor& c, int px)
{
    QString s = svg;
    s.replace(QStringLiteral("currentColor"), c.name());
    QSvgRenderer r(s.toUtf8());
    QPixmap pm(QSize(px, px) * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    r.render(&p, QRectF(0, 0, px, px));
    return QIcon(pm);
}

const char* kIconFoto = R"(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><rect x="3" y="4" width="18" height="16" rx="2"/><circle cx="9" cy="10" r="2"/><path d="M21 16l-5-5-9 9"/></svg>)";
const char* kIconFade = R"(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><rect x="4" y="3" width="16" height="18" rx="2"/><path d="M4 14h16M4 17.5h16" opacity=".6"/></svg>)";
const char* kIconGrain = R"(<svg viewBox="0 0 24 24" fill="currentColor"><circle cx="6" cy="6" r="1.3"/><circle cx="12" cy="8" r="1.3"/><circle cx="18" cy="5" r="1.3"/><circle cx="8" cy="13" r="1.3"/><circle cx="15" cy="14" r="1.3"/><circle cx="5" cy="19" r="1.3"/><circle cx="12" cy="19" r="1.3"/><circle cx="19" cy="18" r="1.3"/></svg>)";
const char* kIconText = R"(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7"><path d="M4 19L9.5 5 15 19M6.2 14h6.6"/><path d="M16 19v-5.5a2.5 2.5 0 015 0V19M16 16h5"/></svg>)";
const char* kIconFolder = R"(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"><path d="M3 7.5A1.5 1.5 0 014.5 6h4.2l2 2h8.8A1.5 1.5 0 0121 9.5v8a1.5 1.5 0 01-1.5 1.5h-15A1.5 1.5 0 013 17.5z"/></svg>)";
const char* kIconSave = R"(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M12 4v11M7.5 10.5L12 15l4.5-4.5M5 19.5h14"/></svg>)";
// pincel com faísca: o que aparece na capa com o mouse em cima
const char* kIconBrush = R"(<svg viewBox="0 0 32 32" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M22.5 5.5l4 4-10.2 10.2-4-4z"/><path d="M12.3 15.7c-2.6-.3-4.6 1.4-4.9 3.9-.2 1.6-.8 3-2.4 3.9 3.3 1.5 7.6.9 9.3-1.4 1-1.4 1.1-3.3-.1-4.5z" fill="currentColor" fill-opacity=".25"/><path d="M25 17.5v3M23.5 19h3M8 5v3.4M6.3 6.7h3.4"/></svg>)";

// As cinco formas do degradê, desenhadas (um retângulo com a faixa escura).
QIcon fadeShapeIcon(int type, const QColor& c)
{
    QPixmap pm(QSize(20, 20) * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(5, 2, 10, 16);
    QColor clear = c; clear.setAlpha(0);
    auto band = [&](qreal y0, qreal y1) {
        QLinearGradient g(0, y0, 0, y1);
        g.setColorAt(0, c);
        g.setColorAt(1, clear);
        p.fillRect(r, g);
    };
    if (type == 1) band(r.bottom(), r.bottom() - 10);
    if (type == 2) band(r.top(), r.top() + 10);
    if (type == 3) { band(r.bottom(), r.bottom() - 7); band(r.top(), r.top() + 7); }
    if (type == 4) {
        QRadialGradient g(r.center(), 9);
        g.setColorAt(0.45, clear);
        g.setColorAt(1, c);
        p.fillRect(r, g);
    }
    p.setPen(QPen(c, 1.3));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 1.5, 1.5);
    return QIcon(pm);
}

QString css(const QColor& c) { return c.name(QColor::HexArgb); }

} // namespace

// ── bolinha de cor ───────────────────────────────────────────────────────────

ColorWheelButton::ColorWheelButton(QWidget* parent)
    : QAbstractButton(parent), m_color(Qt::white)
{
    setCursor(Qt::PointingHandCursor);
    setFixedSize(28, 28);
    setFocusPolicy(Qt::NoFocus);
    connect(this, &QAbstractButton::clicked, this, [this]() {
        const QColor c = ColorPopover::getColor(m_color, this, m_title);
        if (!c.isValid()) return;
        setColor(c);
        emit colorPicked(c);
    });
}

void ColorWheelButton::setColor(const QColor& c) { m_color = c; update(); }
void ColorWheelButton::setMarked(bool on) { m_marked = on; update(); }

void ColorWheelButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(3, 3, -3, -3);
    const Palette pal = Palette::current();
    if (m_marked) {
        p.setPen(QPen(pal.bright, 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(rect()).adjusted(0.8, 0.8, -0.8, -0.8));
    }
    QConicalGradient g(r.center(), 90);
    const QColor stops[] = { QColor("#ff4d4d"), QColor("#ff4dc4"), QColor("#b45bff"), QColor("#5b7dff"),
                             QColor("#4dd8e6"), QColor("#5ee06a"), QColor("#f5f04d"), QColor("#ffb84d") };
    for (int i = 0; i < 8; ++i) g.setColorAt(i / 8.0, stops[i]);
    g.setColorAt(1.0, stops[0]);
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(r);
    // o miolo com a cor atual, com um aro da cor da página em volta
    const QRectF inner = r.adjusted(5, 5, -5, -5);
    p.setBrush(pal.page);
    p.drawEllipse(inner.adjusted(-2, -2, 2, 2));
    p.setBrush(m_color);
    p.setPen(QPen(QColor(0, 0, 0, 60), 1));
    p.drawEllipse(inner);
    if (underMouse()) {
        p.setPen(QPen(QColor(255, 255, 255, 90), 1));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(r);
    }
}

// ── a capa ───────────────────────────────────────────────────────────────────

QuickCoverCanvas::QuickCoverCanvas(QuickCover::Spec* spec, QWidget* parent)
    : QWidget(parent), m_spec(spec)
{
    setFixedSize(kCoverW, kCoverH);
    setMouseTracking(true);
    setFocusPolicy(Qt::TabFocus);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Capa rápida"));
}

void QuickCoverCanvas::setTexts(const QString& title, const QString& author)
{
    m_title = title;
    m_author = author;
    update();
}

void QuickCoverCanvas::setEditing(bool on)
{
    m_editing = on;
    setCursor(on ? Qt::ArrowCursor : Qt::PointingHandCursor);
    setToolTip(on ? QString() : tr("Capa rápida"));
    update();
}

void QuickCoverCanvas::setSelected(int i)
{
    m_sel = i;
    update();
}

int QuickCoverCanvas::hitAt(const QPointF& p) const
{
    for (int i = m_hits.size() - 1; i >= 0; --i)
        if (m_hits[i].contains(p)) return i;
    return -1;
}

void QuickCoverCanvas::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal dpr = devicePixelRatioF();
    QImage img;
    if (!m_static.isNull() && !m_editing) {
        // capa de fora: cortada no formato, sem nada por cima
        const QSize px(qRound(kCoverW * dpr), qRound(kCoverH * dpr));
        img = m_static.scaled(px, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation).toImage();
        img = img.copy((img.width() - px.width()) / 2, (img.height() - px.height()) / 2, px.width(), px.height());
        m_hits.clear();
    } else {
        img = QuickCover::render(*m_spec, QSize(qRound(kCoverW * dpr), qRound(kCoverH * dpr)), true,
                                 m_title, m_author, &m_hits);
    }
    img.setDevicePixelRatio(dpr);
    // livro: cantos de capa (lombada à esquerda) e a sombra da lombada
    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()), 5, 5);
    p.setClipPath(clip);
    p.drawImage(QPointF(0, 0), img);
    QLinearGradient spine(0, 0, 7, 0);
    spine.setColorAt(0, QColor(0, 0, 0, 90));
    spine.setColorAt(0.5, QColor(255, 255, 255, 14));
    spine.setColorAt(1, QColor(0, 0, 0, 0));
    p.fillRect(QRectF(0, 0, 7, height()), spine);

    const Palette pal = Palette::current();
    if (m_editing) {
        // a moldura tracejada do texto escolhido; a linha do meio no arrasto
        if (m_sel >= 0 && m_sel < m_hits.size()) {
            QPen pen(pal.accent, 1, Qt::DashLine);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(m_hits[m_sel], 3, 3);
        }
        if (m_snap) {
            QPen pen(pal.accent, 1, Qt::DashLine);
            p.setPen(pen);
            p.drawLine(QPointF(width() / 2.0, 0), QPointF(width() / 2.0, height()));
        }
    } else if (m_hover || hasFocus()) {
        // véu + pincel + "CAPA RÁPIDA": é aqui que se chama a ferramenta
        p.fillRect(rect(), QColor(10, 8, 6, 118));
        const QPointF c(width() / 2.0, height() / 2.0 - 10);
        p.setPen(QPen(QColor(255, 255, 255, 180), 1.5));
        p.setBrush(QColor(255, 255, 255, 36));
        p.drawEllipse(c, 29, 29);
        svgIcon(QString::fromUtf8(kIconBrush), Qt::white, 28).paint(&p, QRect(int(c.x()) - 14, int(c.y()) - 14, 28, 28));
        QFont f = uiFont(10.5, QFont::DemiBold);
        f.setLetterSpacing(QFont::PercentageSpacing, 118);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(QRectF(0, c.y() + 36, width(), 20), Qt::AlignHCenter | Qt::AlignTop, tr("CAPA RÁPIDA"));
    }
}

void QuickCoverCanvas::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    if (!m_editing) { emit editRequested(); return; }
    const int i = hitAt(e->position());
    if (i < 0) return;
    if (i != m_sel) { m_sel = i; emit selectedChanged(i); }
    m_drag = i;
    const QPointF center(m_spec->texts[i].pos.x() * kCoverW, m_spec->texts[i].pos.y() * kCoverH);
    m_grab = e->position() - center;
    setCursor(Qt::ClosedHandCursor);
    update();
}

void QuickCoverCanvas::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_editing) return;
    if (m_drag < 0) {
        setCursor(hitAt(e->position()) >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }
    QPointF c = e->position() - m_grab;
    qreal x = qBound(0.08, c.x() / kCoverW, 0.92);
    const qreal y = qBound(0.04, c.y() / kCoverH, 0.96);
    // gruda no meio quando passa perto
    m_snap = qAbs(x - 0.5) * kCoverW < 6;
    if (m_snap) x = 0.5;
    m_spec->texts[m_drag].pos = QPointF(x, y);
    update();
}

void QuickCoverCanvas::mouseReleaseEvent(QMouseEvent*)
{
    if (m_drag >= 0) {
        m_drag = -1;
        m_snap = false;
        setCursor(Qt::OpenHandCursor);
        emit moved();
        update();
    }
}

void QuickCoverCanvas::enterEvent(QEnterEvent* e) { QWidget::enterEvent(e); m_hover = true; update(); }
void QuickCoverCanvas::leaveEvent(QEvent* e) { QWidget::leaveEvent(e); m_hover = false; update(); }

void QuickCoverCanvas::keyPressEvent(QKeyEvent* e)
{
    if (!m_editing && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space)) {
        emit editRequested();
        return;
    }
    QWidget::keyPressEvent(e);
}

// ── o painel ─────────────────────────────────────────────────────────────────

QuickCoverPanel::QuickCoverPanel(QuickCover::Spec* spec, QuickCoverCanvas* canvas,
                                 std::function<QString()> title, std::function<void(const QString&)> setTitle,
                                 std::function<QString()> author, std::function<void(const QString&)> setAuthor,
                                 const QStringList& fontFamilies, QWidget* parent)
    : QWidget(parent), m_spec(spec), m_canvas(canvas),
      m_title(std::move(title)), m_author(std::move(author)),
      m_setTitle(std::move(setTitle)), m_setAuthor(std::move(setAuthor)),
      m_families(fontFamilies)
{
    // "Alegreya Black", "Alegreya Sans Light"…: fontes estáticas registram cada
    // peso como família própria. Fica só a família (o peso é o B do negrito).
    static const QRegularExpression weight(QStringLiteral(
        "\\s+(Thin|Hairline|ExtraLight|Extra Light|UltraLight|Light|Book|Medium|SemiBold|Semi Bold|DemiBold|"
        "ExtraBold|Extra Bold|UltraBold|Black|Heavy|ExtraBlack)$"), QRegularExpression::CaseInsensitiveOption);
    QStringList kept;
    for (const QString& fam : std::as_const(m_families)) {
        QString base = fam;
        base.remove(weight);
        if (base != fam && m_families.contains(base, Qt::CaseInsensitive)) continue;
        kept << fam;
    }
    m_families = kept;
    const Palette pal = Palette::current();
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);

    // abas
    auto* tabRow = new QHBoxLayout;
    tabRow->setSpacing(4);
    m_tabs = new QButtonGroup(this);
    m_tabs->setExclusive(true);
    const QList<QPair<QString, const char*>> tabs = {
        { tr("Foto"), kIconFoto }, { tr("Degradê"), kIconFade }, { tr("Grão"), kIconGrain }, { tr("Texto"), kIconText } };
    for (int i = 0; i < tabs.size(); ++i) {
        auto* b = new QToolButton(this);
        b->setObjectName(QStringLiteral("qcTab"));
        b->setText(tabs[i].first);
        b->setIcon(svgIcon(QString::fromUtf8(tabs[i].second), pal.dim, 15));
        b->setIconSize(QSize(15, 15));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setFixedHeight(32);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setFont(uiFont(12));
        m_tabs->addButton(b, i);
        tabRow->addWidget(b);
    }
    v->addLayout(tabRow);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildFoto());
    m_pages->addWidget(buildFade());
    m_pages->addWidget(buildGrain());
    m_pages->addWidget(buildText());
    v->addWidget(m_pages, 1);
    connect(m_tabs, &QButtonGroup::idClicked, this, [this](int id) {
        m_pages->setCurrentIndex(id);
        if (id == 3) refreshTextPane();
    });
    m_tabs->button(0)->setChecked(true);

    // rodapé do painel: salvar como imagem + pronto
    auto* foot = new QHBoxLayout;
    foot->setSpacing(8);
    auto* save = new QToolButton(this);
    save->setObjectName(QStringLiteral("qcMini"));
    save->setText(tr("Salvar como imagem"));
    save->setToolTip(tr("Salvar a capa como imagem (PNG, 1600×2400)"));
    save->setIcon(svgIcon(QString::fromUtf8(kIconSave), pal.ink, 15));
    save->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    save->setCursor(Qt::PointingHandCursor);
    save->setFixedHeight(28);
    connect(save, &QToolButton::clicked, this, &QuickCoverPanel::exportRequested);
    foot->addWidget(save);
    foot->addStretch(1);
    auto* done = new QPushButton(tr("Pronto"), this);
    done->setObjectName(QStringLiteral("sheetPri"));
    done->setCursor(Qt::PointingHandCursor);
    done->setFixedHeight(28);
    done->setAutoDefault(false);
    connect(done, &QPushButton::clicked, this, &QuickCoverPanel::doneRequested);
    foot->addWidget(done);
    v->addLayout(foot);

    connect(m_canvas, &QuickCoverCanvas::selectedChanged, this, [this](int i) { showText(i); });
    applyTheme();
    syncFromSpec();
}

QuickCover::Text* QuickCoverPanel::current()
{
    const int i = m_canvas->selected();
    if (i < 0 || i >= m_spec->texts.size()) return m_spec->texts.isEmpty() ? nullptr : &m_spec->texts[0];
    return &m_spec->texts[i];
}

void QuickCoverPanel::showText(int index)
{
    m_canvas->setSelected(index);
    m_tabs->button(3)->setChecked(true);
    m_pages->setCurrentIndex(3);
    refreshTextPane();
}

// ── Foto ──
QWidget* QuickCoverPanel::buildFoto()
{
    const Palette pal = Palette::current();
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    auto* lbl = new QLabel(tr("Sem foto, só cor:"), w);
    lbl->setObjectName(QStringLiteral("sheetDim"));
    lbl->setFont(uiFont(11.5));
    row->addWidget(lbl);
    m_bgWheel = new ColorWheelButton(w);
    m_bgWheel->setPickerTitle(tr("Cor da capa"));
    m_bgWheel->setToolTip(tr("Cor de fundo (sem foto)"));
    connect(m_bgWheel, &ColorWheelButton::colorPicked, this, [this](const QColor& c) {
        m_spec->image.clear();
        m_spec->bg = c;
        refreshThumbMarks();
        m_canvas->refresh();
        emit changed();
    });
    row->addWidget(m_bgWheel);
    row->addStretch(1);
    auto* folder = new QToolButton(w);
    folder->setObjectName(QStringLiteral("qcMini"));
    folder->setIcon(svgIcon(QString::fromUtf8(kIconFolder), pal.ink, 17));
    folder->setIconSize(QSize(17, 17));
    folder->setToolTip(tr("Usar uma imagem do computador"));
    folder->setCursor(Qt::PointingHandCursor);
    folder->setFixedSize(32, 28);
    // a folha abre o arquivo e põe a imagem no spec
    connect(folder, &QToolButton::clicked, this, &QuickCoverPanel::pickImageRequested);
    row->addWidget(folder);
    v->addLayout(row);

    m_imageNames = QuickCover::imageNames();
    auto* count = new QLabel(m_imageNames.size() == 1 ? tr("1 foto dos temas")
                                                      : tr("%1 fotos dos temas").arg(m_imageNames.size()), w);
    count->setObjectName(QStringLiteral("sheetDim"));
    count->setFont(uiFont(11));
    v->addWidget(count);

    auto* scroll = new QScrollArea(w);
    scroll->setObjectName(QStringLiteral("qcScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* inner = new QWidget(scroll);
    inner->setObjectName(QStringLiteral("qcInner"));
    m_thumbGrid = new QGridLayout(inner);
    m_thumbGrid->setContentsMargins(2, 2, 6, 2);
    m_thumbGrid->setSpacing(6);
    m_thumbGrid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scroll->setWidget(inner);
    scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    v->addWidget(scroll, 1);

    // miniaturas aos poucos: decodificar as 92 de uma vez travaria a folha
    m_thumbTimer = new QTimer(this);
    m_thumbTimer->setInterval(0);
    connect(m_thumbTimer, &QTimer::timeout, this, &QuickCoverPanel::loadMoreThumbs);
    m_thumbTimer->start();
    return w;
}

void QuickCoverPanel::loadMoreThumbs()
{
    const QSize tsz(56, 84);
    const qreal dpr = devicePixelRatioF();
    for (int n = 0; n < 4 && m_thumbsLoaded < m_imageNames.size(); ++n, ++m_thumbsLoaded) {
        const QString name = m_imageNames[m_thumbsLoaded];
        auto* b = new QToolButton(m_thumbGrid->parentWidget());
        b->setObjectName(QStringLiteral("qcThumb"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setFixedSize(tsz + QSize(4, 4));
        QPixmap pm = QuickCover::thumbnail(name, (QSizeF(tsz) * dpr).toSize());
        pm.setDevicePixelRatio(dpr);
        b->setIcon(QIcon(pm));
        b->setIconSize(tsz);
        b->setToolTip(QFileInfo(name).completeBaseName().remove(QStringLiteral("derived-")));
        connect(b, &QToolButton::clicked, this, [this, name]() {
            m_spec->image = name;
            refreshThumbMarks();
            m_canvas->refresh();
            emit changed();
        });
        m_thumbGrid->addWidget(b, m_thumbsLoaded / 5, m_thumbsLoaded % 5);
        m_thumbs << b;
        b->setChecked(m_spec->image == name);
    }
    if (m_thumbsLoaded >= m_imageNames.size()) m_thumbTimer->stop();
}

void QuickCoverPanel::refreshThumbMarks()
{
    for (int i = 0; i < m_thumbs.size(); ++i)
        m_thumbs[i]->setChecked(m_spec->image == m_imageNames.value(i));
    m_bgWheel->setColor(m_spec->bg);
    m_bgWheel->setMarked(m_spec->image.isEmpty());
}

// ── Degradê ──
QWidget* QuickCoverPanel::buildFade()
{
    const Palette pal = Palette::current();
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);

    auto* top = new QHBoxLayout;
    top->setSpacing(4);
    auto* shapes = new QHBoxLayout;
    shapes->setSpacing(0);
    m_fadeGroup = new QButtonGroup(this);
    m_fadeGroup->setExclusive(true);
    const QStringList names = { tr("Nenhum"), tr("Embaixo"), tr("Em cima"), tr("Os dois"), tr("Vinheta") };
    for (int i = 0; i < 5; ++i) {
        auto* b = new QToolButton(w);
        b->setObjectName(QStringLiteral("qcSeg"));
        b->setProperty("pos", i == 0 ? "first" : i == 4 ? "last" : "mid");
        b->setIcon(fadeShapeIcon(i, pal.ink));
        b->setIconSize(QSize(20, 20));
        b->setToolTip(names[i]);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setFixedSize(36, 30);
        m_fadeGroup->addButton(b, i);
        shapes->addWidget(b);
    }
    top->addLayout(shapes);
    top->addStretch(1);
    m_fadeDark = new QToolButton(w);
    m_fadeLight = new QToolButton(w);
    for (auto* b : { m_fadeDark, m_fadeLight }) {
        b->setObjectName(QStringLiteral("qcMini"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(28);
        top->addWidget(b);
    }
    m_fadeDark->setText(tr("escuro"));
    m_fadeLight->setText(tr("claro"));
    v->addLayout(top);
    connect(m_fadeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_spec->fadeType = id;
        m_canvas->refresh();
        emit changed();
    });
    auto setFadeColor = [this](bool dark) {
        m_spec->fadeColor = dark ? QColor(Qt::black) : QColor(Qt::white);
        // ponto de partida: texto claro sobre degradê escuro e vice-versa
        const QColor tc = dark ? QColor(QStringLiteral("#f6efe3")) : QColor(QStringLiteral("#1b1612"));
        for (auto& t : m_spec->texts) t.color = tc;
        m_fadeDark->setChecked(dark);
        m_fadeLight->setChecked(!dark);
        m_canvas->refresh();
        emit changed();
    };
    connect(m_fadeDark, &QToolButton::clicked, this, [setFadeColor]() { setFadeColor(true); });
    connect(m_fadeLight, &QToolButton::clicked, this, [setFadeColor]() { setFadeColor(false); });

    auto slider = [this, w, v](const QString& label, int lo, int hi, int* field) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* l = new QLabel(label, w);
        l->setObjectName(QStringLiteral("sheetDim"));
        l->setFont(uiFont(11.5));
        l->setFixedWidth(58);
        auto* s = new QSlider(Qt::Horizontal, w);
        s->setObjectName(QStringLiteral("qcSlider"));
        s->setRange(lo, hi);
        s->setValue(*field);
        auto* val = new QLabel(QString::number(*field), w);
        val->setFont(monoFont(10.5));
        val->setFixedWidth(26);
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(s, &QSlider::valueChanged, this, [this, field, val](int x) {
            *field = x;
            val->setText(QString::number(x));
            m_canvas->refresh();
            emit changed();
        });
        row->addWidget(l);
        row->addWidget(s, 1);
        row->addWidget(val);
        v->addLayout(row);
        return s;
    };
    m_fadeOp = slider(tr("força"), 0, 100, &m_spec->fadeOpacity);
    m_fadeSize = slider(tr("tamanho"), 5, 100, &m_spec->fadeSize);
    auto* hint = new QLabel(tr("O mesmo degradê do fundo dos temas."), w);
    hint->setObjectName(QStringLiteral("sheetDim"));
    hint->setFont(uiFont(11));
    v->addWidget(hint);
    v->addStretch(1);
    return w;
}

// ── Grão ──
QWidget* QuickCoverPanel::buildGrain()
{
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);
    auto slider = [this, w, v](const QString& label, int lo, int hi, int* field) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* l = new QLabel(label, w);
        l->setObjectName(QStringLiteral("sheetDim"));
        l->setFont(uiFont(11.5));
        l->setFixedWidth(58);
        auto* s = new QSlider(Qt::Horizontal, w);
        s->setObjectName(QStringLiteral("qcSlider"));
        s->setRange(lo, hi);
        s->setValue(*field);
        auto* val = new QLabel(QString::number(*field), w);
        val->setFont(monoFont(10.5));
        val->setFixedWidth(26);
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(s, &QSlider::valueChanged, this, [this, field, val](int x) {
            *field = x;
            val->setText(QString::number(x));
            m_canvas->refresh();
            emit changed();
        });
        row->addWidget(l);
        row->addWidget(s, 1);
        row->addWidget(val);
        v->addLayout(row);
        return s;
    };
    m_grain = slider(tr("quanto"), 0, 100, &m_spec->grain);
    m_grainSize = slider(tr("tamanho"), 1, 12, &m_spec->grainSize);
    auto* hint = new QLabel(tr("Fino é película; grande vira mancha (perolado, feltro, pedra). O mesmo grão dos temas."), w);
    hint->setObjectName(QStringLiteral("sheetDim"));
    hint->setWordWrap(true);
    hint->setFont(uiFont(11));
    v->addWidget(hint);
    v->addStretch(1);
    return w;
}

// ── Texto ──
QWidget* QuickCoverPanel::buildText()
{
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto* top = new QHBoxLayout;
    m_editingLbl = new QLabel(w);
    m_editingLbl->setObjectName(QStringLiteral("sheetDim"));
    m_editingLbl->setTextFormat(Qt::RichText);
    m_editingLbl->setFont(uiFont(11.5));
    top->addWidget(m_editingLbl, 1);
    auto* add = new QToolButton(w);
    add->setObjectName(QStringLiteral("qcMini"));
    add->setText(tr("+ texto"));
    add->setToolTip(tr("Uma linha a mais na capa (série, frase de efeito)"));
    add->setCursor(Qt::PointingHandCursor);
    add->setFixedHeight(28);
    connect(add, &QToolButton::clicked, this, [this]() {
        const QuickCover::Text* base = current();
        QuickCover::Text t;
        t.role = QStringLiteral("extra");
        t.text = tr("Livro um");
        t.family = base ? base->family : QString();
        t.size = 14;
        t.italic = true;
        t.spacing = 0.04;
        t.color = base ? base->color : QColor(QStringLiteral("#f6efe3"));
        int extras = 0;
        for (const auto& x : m_spec->texts) if (x.role == QLatin1String("extra")) ++extras;
        t.pos = QPointF(0.5, 0.12 + (extras % 3) * 0.07);
        m_spec->texts.append(t);
        showText(int(m_spec->texts.size()) - 1);
        m_textEdit->setFocus();
        m_textEdit->selectAll();
        m_canvas->refresh();
        emit changed();
    });
    top->addWidget(add);
    v->addLayout(top);

    auto* editRow = new QHBoxLayout;
    editRow->setSpacing(6);
    m_textEdit = SheetDialog::field(w);
    editRow->addWidget(m_textEdit, 1);
    m_removeBtn = new QToolButton(w);
    m_removeBtn->setObjectName(QStringLiteral("qcMini"));
    m_removeBtn->setText(tr("remover"));
    m_removeBtn->setToolTip(tr("Tirar este texto da capa"));
    m_removeBtn->setCursor(Qt::PointingHandCursor);
    m_removeBtn->setFixedHeight(28);
    connect(m_removeBtn, &QToolButton::clicked, this, [this]() {
        const int i = m_canvas->selected();
        if (i < 0 || i >= m_spec->texts.size() || m_spec->texts[i].role != QLatin1String("extra")) return;
        m_spec->texts.removeAt(i);
        showText(0);
        m_canvas->refresh();
        emit changed();
    });
    editRow->addWidget(m_removeBtn);
    v->addLayout(editRow);
    connect(m_textEdit, &QLineEdit::textEdited, this, [this](const QString& s) {
        QuickCover::Text* t = current();
        if (!t) return;
        // título e autor: o mesmo valor dos campos da folha
        if (t->role == QLatin1String("title")) m_setTitle(s);
        else if (t->role == QLatin1String("author")) m_setAuthor(s);
        else t->text = s;
        m_canvas->refresh();
        emit changed();
    });

    auto* style = new QHBoxLayout;
    style->setSpacing(4);
    auto mini = [w](const QString& text, const QString& tip) {
        auto* b = new QToolButton(w);
        b->setObjectName(QStringLiteral("qcMini"));
        b->setText(text);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(30, 28);
        return b;
    };
    m_boldBtn = mini(QStringLiteral("B"), tr("Negrito"));
    m_italicBtn = mini(QStringLiteral("I"), tr("Itálico"));
    m_underBtn = mini(QStringLiteral("U"), tr("Sublinhado"));
    QFont bf = uiFont(12.5, QFont::Bold); m_boldBtn->setFont(bf);
    QFont itf = serifFont(13); itf.setItalic(true); m_italicBtn->setFont(itf);
    QFont uf = uiFont(12.5); uf.setUnderline(true); m_underBtn->setFont(uf);
    for (auto* b : { m_boldBtn, m_italicBtn, m_underBtn }) { b->setCheckable(true); style->addWidget(b); }
    style->addSpacing(8);
    auto* smaller = mini(QStringLiteral("A−"), tr("Menor"));
    auto* bigger = mini(QStringLiteral("A+"), tr("Maior"));
    for (auto* b : { smaller, bigger }) { b->setFixedSize(36, 28); b->setFont(uiFont(12)); }
    style->addWidget(smaller);
    style->addWidget(bigger);
    style->addSpacing(8);
    m_textWheel = new ColorWheelButton(w);
    m_textWheel->setPickerTitle(tr("Cor do texto"));
    m_textWheel->setToolTip(tr("Cor do texto"));
    style->addWidget(m_textWheel);
    style->addStretch(1);
    v->addLayout(style);
    auto toggle = [this](bool QuickCover::Text::* field, QToolButton* b) {
        connect(b, &QToolButton::clicked, this, [this, field, b]() {
            if (QuickCover::Text* t = current()) { t->*field = b->isChecked(); m_canvas->refresh(); emit changed(); }
        });
    };
    toggle(&QuickCover::Text::bold, m_boldBtn);
    toggle(&QuickCover::Text::italic, m_italicBtn);
    toggle(&QuickCover::Text::underline, m_underBtn);
    auto resize = [this](int delta) {
        if (QuickCover::Text* t = current()) {
            t->size = qBound(8.0, t->size + delta, 72.0);
            m_canvas->refresh();
            emit changed();
        }
    };
    connect(smaller, &QToolButton::clicked, this, [resize]() { resize(-2); });
    connect(bigger, &QToolButton::clicked, this, [resize]() { resize(2); });
    connect(m_textWheel, &ColorWheelButton::colorPicked, this, [this](const QColor& c) {
        if (QuickCover::Text* t = current()) { t->color = c; m_canvas->refresh(); emit changed(); }
    });

    m_fontSearch = new QLineEdit(w);
    m_fontSearch->setObjectName(QStringLiteral("sheetFld"));
    m_fontSearch->setFont(uiFont(12.5));
    m_fontSearch->setFixedHeight(30);
    m_fontSearch->setPlaceholderText(tr("buscar fonte (%1)").arg(m_families.size()));
    m_fontSearch->setClearButtonEnabled(true);
    connect(m_fontSearch, &QLineEdit::textChanged, this, [this]() { rebuildFontList(); });
    v->addWidget(m_fontSearch);

    auto* scroll = new QScrollArea(w);
    scroll->setObjectName(QStringLiteral("qcScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_fontBox = new QWidget(scroll);
    m_fontBox->setObjectName(QStringLiteral("qcInner"));
    m_fontGrid = new QGridLayout(m_fontBox);
    m_fontGrid->setContentsMargins(0, 0, 6, 0);
    m_fontGrid->setSpacing(4);
    m_fontGrid->setAlignment(Qt::AlignTop);
    scroll->setWidget(m_fontBox);
    scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    v->addWidget(scroll, 1);
    // a lista de fontes é montada na primeira vez que a aba abre (125 botões,
    // cada um desenhado na própria fonte)
    return w;
}

void QuickCoverPanel::rebuildFontList()
{
    const QString q = m_fontSearch->text().trimmed();
    if (m_fontBtns.isEmpty()) {
        for (const QString& fam : m_families) {
            auto* b = new QPushButton(fam, m_fontBox);
            b->setObjectName(QStringLiteral("qcFont"));
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            b->setAutoDefault(false);
            b->setFixedHeight(32);
            b->setMinimumWidth(0);
            b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            QFont f(fam);
            f.setPixelSize(15);
            b->setFont(f);
            b->setToolTip(fam);
            connect(b, &QPushButton::clicked, this, [this, fam]() {
                if (QuickCover::Text* t = current()) { t->family = fam; m_canvas->refresh(); emit changed(); }
                refreshTextPane();
            });
            m_fontBtns << b;
        }
    }
    int n = 0;
    for (QPushButton* b : std::as_const(m_fontBtns)) {
        const bool show = q.isEmpty() || b->text().contains(q, Qt::CaseInsensitive);
        m_fontGrid->removeWidget(b);
        b->setVisible(show);
        if (show) { m_fontGrid->addWidget(b, n / 3, n % 3); ++n; }
    }
}

void QuickCoverPanel::refreshTextPane()
{
    if (m_fontBtns.isEmpty()) rebuildFontList();
    QuickCover::Text* t = current();
    if (!t) return;
    const Palette pal = Palette::current();
    const QString which = t->role == QLatin1String("title") ? tr("título")
                        : t->role == QLatin1String("author") ? tr("autor") : tr("texto seu");
    m_editingLbl->setText(tr("Editando: <b style='color:%1'>%2</b>").arg(pal.bright.name(), which));
    m_editingLbl->setToolTip(tr("Clique num texto da capa pra trocar"));
    const QString value = t->role == QLatin1String("title") ? m_title()
                        : t->role == QLatin1String("author") ? m_author() : t->text;
    if (m_textEdit->text() != value) m_textEdit->setText(value);
    m_textEdit->setToolTip(t->role == QLatin1String("extra") ? QString()
                           : tr("É o mesmo %1 da folha").arg(which));
    m_removeBtn->setVisible(t->role == QLatin1String("extra"));
    m_boldBtn->setChecked(t->bold);
    m_italicBtn->setChecked(t->italic);
    m_underBtn->setChecked(t->underline);
    m_textWheel->setColor(t->color);
    for (QPushButton* b : std::as_const(m_fontBtns)) b->setChecked(b->text() == t->family);
}

void QuickCoverPanel::syncFromSpec()
{
    if (m_bgWheel) refreshThumbMarks();
    if (auto* b = m_fadeGroup->button(m_spec->fadeType)) b->setChecked(true);
    const bool dark = m_spec->fadeColor.lightness() < 128;
    m_fadeDark->setChecked(dark);
    m_fadeLight->setChecked(!dark);
    for (auto [s, v] : { std::pair{ m_fadeOp, m_spec->fadeOpacity }, std::pair{ m_fadeSize, m_spec->fadeSize },
                         std::pair{ m_grain, m_spec->grain }, std::pair{ m_grainSize, m_spec->grainSize } }) {
        QSignalBlocker block(s);
        s->setValue(v);
    }
    if (m_pages->currentIndex() == 3) refreshTextPane();
}

void QuickCoverPanel::applyTheme()
{
    const Palette pal = Palette::current();
    setStyleSheet(Theme::qss(QStringLiteral(
        "QToolButton#qcTab { background: transparent; border: none; border-radius: @radius-control; color: %1; padding: 0 6px; }"
        "QToolButton#qcTab:hover { color: %2; }"
        "QToolButton#qcTab:checked { background: %3; color: %2; }"
        "QToolButton#qcMini { background: transparent; border: 1px solid %4; border-radius: 5px; color: %5; padding: 0 8px; font-size: 12px; }"
        "QToolButton#qcMini:hover { border-color: %6; }"
        "QToolButton#qcMini:checked { border-color: %7; background: %8; color: %2; }"
        "QToolButton#qcSeg { background: transparent; border: 1px solid %4; border-radius: 0; }"
        "QToolButton#qcSeg[pos=\"first\"] { border-top-left-radius: 6px; border-bottom-left-radius: 6px; }"
        "QToolButton#qcSeg[pos=\"last\"] { border-top-right-radius: 6px; border-bottom-right-radius: 6px; }"
        "QToolButton#qcSeg:checked { background: %8; border-color: %7; }"
        "QToolButton#qcThumb { background: transparent; border: 2px solid transparent; border-radius: 4px; padding: 0; }"
        "QToolButton#qcThumb:hover { border-color: %6; }"
        "QToolButton#qcThumb:checked { border-color: %7; }"
        "QPushButton#qcFont { background: transparent; border: 1px solid transparent; border-radius: 5px; color: %5;"
        "  text-align: left; padding: 0 7px; }"
        "QPushButton#qcFont:hover { background: %9; }"
        "QPushButton#qcFont:checked { border-color: %7; background: %8; color: %2; }"
        "QWidget#qcInner { background: transparent; }"
        "QScrollArea#qcScroll { background: transparent; }"
        "QSlider#qcSlider::groove:horizontal { height: 4px; border-radius: 2px; background: %4; }"
        "QSlider#qcSlider::sub-page:horizontal { height: 4px; border-radius: 2px; background: %7; }"
        "QSlider#qcSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; background: %10; }")
        .arg(pal.dim.name(), pal.bright.name(), css(alpha(pal.ink, 0.10)), pal.border.name(), pal.ink.name(),
             css(alpha(pal.ink, 0.40)), pal.accent.name(), css(alpha(pal.accent, 0.15)), css(alpha(pal.ink, 0.07)))
        .arg(pal.accent.name())));
}
