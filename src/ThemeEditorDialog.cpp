#include "ThemeEditorDialog.h"

#include "ColorPopover.h"
#include "Theme.h"
#include "ThemeScene.h"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QImageReader>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>
#include <algorithm>

using ThemeScene::ImageCache;

namespace {

QColor c(const QString& css) { return Theme::toColor(css); }

QFont px(int size, int weight = QFont::Normal)
{
    QFont f = QApplication::font();
    f.setPixelSize(size);
    f.setWeight(QFont::Weight(weight));
    return f;
}

QString colorToString(const QColor& col)
{
    if (!col.isValid()) return QString();
    if (col.alpha() == 255) return col.name();
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(col.red()).arg(col.green()).arg(col.blue())
        .arg(QString::number(col.alphaF(), 'f', 3));
}

QString fingerprint(const Theme::MiraTheme& t)
{
    return QString::number(qHash(QJsonDocument(Theme::themeToJson(t)).toJson(QJsonDocument::Compact)), 16);
}

QString levelWord(double v)
{
    // As mesmas palavras neutras do painel de Temas: contraste descrito, não julgado.
    if (v >= 7.0) return QCoreApplication::translate("ThemesPanel", "alto");
    if (v >= 4.5) return QCoreApplication::translate("ThemesPanel", "médio");
    if (v >= 3.0) return QCoreApplication::translate("ThemesPanel", "suave");
    return QCoreApplication::translate("ThemesPanel", "bem suave");
}

// Fontes pro título do capítulo — todas já embarcadas no Qenna.
const QStringList& titleFonts()
{
    static const QStringList f = {
        QStringLiteral("Lora"), QStringLiteral("Playfair Display"), QStringLiteral("Cormorant Garamond"),
        QStringLiteral("EB Garamond"), QStringLiteral("Libre Baskerville"), QStringLiteral("Crimson Pro"),
        QStringLiteral("Source Serif 4"), QStringLiteral("Spectral"), QStringLiteral("Cinzel"),
        QStringLiteral("Alegreya SC"), QStringLiteral("Abril Fatface"), QStringLiteral("Bebas Neue"),
        QStringLiteral("Special Elite"), QStringLiteral("Caveat"),
    };
    return f;
}

// Até 7 cores dominantes e bem distintas da foto (grade 48x32, cores
// arredondadas em degraus de 24 e contadas).
QList<QColor> photoColors(const QString& path)
{
    QImageReader r(path);
    r.setAutoTransform(true);
    if (r.size().isValid()) r.setScaledSize(QSize(48, 32));
    QImage img = r.read();
    if (img.isNull()) return {};
    img = img.convertToFormat(QImage::Format_RGB32);
    QHash<quint32, int> bins;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            auto q = [](int v) { return qMin(255, qRound(v / 24.0) * 24); };
            bins[qRgb(q(qRed(row[x])), q(qGreen(row[x])), q(qBlue(row[x])))] += 1;
        }
    }
    QList<QPair<int, quint32>> sorted;
    for (auto it = bins.constBegin(); it != bins.constEnd(); ++it) sorted.append({ it.value(), it.key() });
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    QList<QColor> out;
    for (const auto& s : sorted) {
        const QColor cand(s.second);
        const bool far = std::all_of(out.cbegin(), out.cend(), [&](const QColor& o) {
            return std::hypot(double(o.red() - cand.red()), double(o.green() - cand.green()), double(o.blue() - cand.blue())) > 60;
        });
        if (far) out.append(cand);
        if (out.size() == 7) break;
    }
    return out;
}

} // namespace

namespace CreatorDetail {

// Interruptor pequeno (Antes e depois, Sombra).
class Toggle : public QAbstractButton {
public:
    Toggle(const QString& text, QWidget* parent) : QAbstractButton(parent)
    {
        setText(text);
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
    }
    QSize sizeHint() const override
    {
        return QSize(26 + 8 + QFontMetrics(px(12)).horizontalAdvance(text()) + 2, 22);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF track(0, (height() - 14) / 2.0, 26, 14);
        p.setPen(Qt::NoPen);
        p.setBrush(isChecked() ? c(Theme::accentDefault()) : c(Theme::panelBorder()));
        p.drawRoundedRect(track, 7, 7);
        p.setBrush(c(Theme::textBright()));
        p.drawEllipse(QRectF(isChecked() ? 14 : 2, track.top() + 2, 10, 10));
        p.setPen(c(Theme::textPrimary()));
        p.setFont(px(12));
        p.drawText(QRectF(34, 0, width() - 34, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
    }
};

// Amostra de cor com o nome embaixo. Anel de destaque = cor "marcada" (a que
// recebe as cores da foto).
class Swatch : public QAbstractButton {
public:
    Swatch(const QString& label, int size, QWidget* parent) : QAbstractButton(parent), m_size(size)
    {
        setText(label);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }
    void setColor(const QColor& col) { m_color = col; update(); }
    void setMarked(bool m) { m_marked = m; update(); }
    QSize sizeHint() const override
    {
        const int w = qMax(m_size + 8, QFontMetrics(px(11)).horizontalAdvance(text()) + 4);
        return QSize(w, m_size + (text().isEmpty() ? 4 : 20));
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF sq((width() - m_size) / 2.0, 2, m_size, m_size);
        // Xadrez atrás: cor com transparência continua legível.
        if (m_color.alpha() < 255) {
            QPainterPath clip;
            clip.addRoundedRect(sq, 6, 6);
            p.save();
            p.setClipPath(clip);
            const int k = 5;
            for (int y = 0; y * k < m_size; ++y)
                for (int x = 0; x * k < m_size; ++x)
                    p.fillRect(QRectF(sq.left() + x * k, sq.top() + y * k, k, k), (x + y) % 2 ? QColor(90, 90, 90) : QColor(150, 150, 150));
            p.restore();
        }
        p.setPen(QPen(m_marked ? c(Theme::accentDefault()) : QColor(255, 255, 255, underMouse() ? 70 : 36), m_marked ? 2 : 1));
        p.setBrush(m_color);
        p.drawRoundedRect(sq.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
        if (!text().isEmpty()) {
            p.setPen(underMouse() ? c(Theme::textBright()) : c(Theme::textMuted()));
            p.setFont(px(11));
            p.drawText(QRectF(0, m_size + 5, width(), 16), Qt::AlignHCenter | Qt::AlignTop, text());
        }
    }
private:
    QColor m_color;
    int m_size;
    bool m_marked = false;
};

class Dot : public QWidget {
public:
    Dot(const QColor& col, QWidget* parent) : QWidget(parent), m_color(col) { setFixedSize(9, 9); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(c(Theme::panelBorder()), 1));
        p.setBrush(m_color);
        p.drawEllipse(QRectF(1, 1, 7, 7));
    }
private:
    QColor m_color;
};

// A janela do Qenna com o tema em edição. Passar o mouse acende a parte que
// o clique editaria; com Antes e depois, a linha divide o tema de partida
// (à esquerda) do editado.
class Canvas : public QWidget {
public:
    std::function<void(ThemeScene::Group, const QString&)> onPick;

    explicit Canvas(QWidget* parent) : QWidget(parent)
    {
        setMouseTracking(true);
        setMinimumSize(640, 340);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_flash.setSingleShot(true);
        m_flash.setInterval(1100);
        QObject::connect(&m_flash, &QTimer::timeout, this, [this]() { m_selectedOn = false; update(); });
        connect(ImageCache::instance(), &ImageCache::ready, this, [this](const QString& path, int) {
            if (path == m_edited.backgroundImage || path == m_original.backgroundImage) update();
        });
    }

    void setThemes(const Theme::MiraTheme& edited, const Theme::MiraTheme& original)
    {
        m_edited = edited;
        m_original = original;
        m_editedFp = fingerprint(edited);
        m_originalFp = fingerprint(original);
        update();
    }
    void setScene(int s) { m_scene = s; m_hover = -1; update(); }
    void setCompare(bool on) { m_compare = on; update(); }
    void flashSelection(ThemeScene::Group g, const QString& panel)
    {
        m_selGroup = g;
        m_selPanel = panel;
        m_selectedOn = true;
        m_flash.start();
        update();
    }

    QRectF target() const
    {
        const qreal w = width(), h = height();
        qreal tw = w, th = w * ThemeScene::kSceneH / ThemeScene::kSceneW;
        if (th > h) { th = h; tw = h * ThemeScene::kSceneW / ThemeScene::kSceneH; }
        return QRectF((w - tw) / 2.0, (h - th) / 2.0, tw, th);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF t = target();
        QPainterPath clip;
        clip.addRoundedRect(t, 6, 6);
        p.setClipPath(clip);
        if (m_compare) {
            p.drawPixmap(t.topLeft(), render(m_original, m_originalFp));
            const qreal x = t.left() + qRound(t.width() * m_cut);
            p.save();
            p.setClipRect(QRectF(x, t.top(), t.right() - x, t.height()), Qt::IntersectClip);
            p.drawPixmap(t.topLeft(), render(m_edited, m_editedFp));
            p.restore();
        } else {
            p.drawPixmap(t.topLeft(), render(m_edited, m_editedFp));
        }

        // Realce: o hover em azul; o que acabou de ser clicado, no destaque.
        const auto regs = ThemeScene::regions(ThemeScene::Scene(m_scene), t);
        auto outline = [&](ThemeScene::Group g, const QString& panel, const QColor& col, const QString& label) {
            QList<QRectF> rects;
            for (const auto& r : regs) if (r.group == g && (panel.isEmpty() || r.panel == panel)) rects.append(r.rect);
            if (g == ThemeScene::Group::Background) rects = { t.adjusted(3, 3, -3, -3) };
            QColor fill = col; fill.setAlpha(26);
            for (const QRectF& r : rects) {
                p.setPen(QPen(QColor(0, 0, 0, 90), 3));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(r.adjusted(-1, -1, 1, 1), 4, 4);
                p.setPen(QPen(col, 1.5));
                p.setBrush(fill);
                p.drawRoundedRect(r.adjusted(-1, -1, 1, 1), 4, 4);
            }
            if (rects.isEmpty()) return;
            QRectF first = rects.first();
            for (const QRectF& r : rects) if (r.top() < first.top() || (r.top() == first.top() && r.left() < first.left())) first = r;
            const QFont f = px(11, QFont::DemiBold);
            const qreal lw = QFontMetricsF(f).horizontalAdvance(label) + 16;
            qreal ly = g == ThemeScene::Group::Background ? t.top() + 34 : first.bottom() + 4;
            if (ly + 20 > t.bottom()) ly = first.top() + 4;
            const QRectF lr(qBound(t.left() + 4, first.left(), t.right() - lw - 4), ly, lw, 20);
            p.setPen(Qt::NoPen);
            p.setBrush(col);
            p.drawRoundedRect(lr, 4, 4);
            p.setPen(QColor(16, 19, 26));
            p.setFont(f);
            p.drawText(lr, Qt::AlignCenter, label);
        };
        if (!m_labelFor) return;
        if (m_hover >= 0 && m_hover < regs.size()) {
            const auto& r = regs.at(m_hover);
            outline(r.group, r.group == ThemeScene::Group::Panels ? r.panel : QString(), QColor(0x7f, 0xb2, 0xff), m_labelFor(r.group, r.panel));
        } else if (m_selectedOn) {
            outline(m_selGroup, m_selGroup == ThemeScene::Group::Panels ? m_selPanel : QString(), c(Theme::accentDefault()),
                    m_labelFor(m_selGroup, m_selPanel));
        }

        if (m_compare) {
            const qreal x = t.left() + qRound(t.width() * m_cut);
            p.setClipping(false);
            QFont lf = px(9, QFont::Bold);
            lf.setLetterSpacing(QFont::AbsoluteSpacing, 1);
            p.setFont(lf);
            auto lab = [&](const QString& s, bool right) {
                const qreal w = QFontMetricsF(lf).horizontalAdvance(s) + 14;
                const QRectF r(right ? t.right() - 8 - w : t.left() + 8, t.top() + 8, w, 16);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 140));
                p.drawRoundedRect(r, 4, 4);
                p.setPen(Qt::white);
                p.drawText(r, Qt::AlignCenter, s);
            };
            lab(QCoreApplication::translate("ThemeEditorDialog", "ANTES"), false);
            lab(QCoreApplication::translate("ThemeEditorDialog", "DEPOIS"), true);
            p.fillRect(QRectF(x - 1.5, t.top(), 4, t.height()), QColor(0, 0, 0, 40));
            p.fillRect(QRectF(x - 0.5, t.top(), 1, t.height()), QColor(255, 255, 255, 217));
            p.setPen(Qt::NoPen);
            p.setBrush(Qt::white);
            p.drawRoundedRect(QRectF(x - 3, t.center().y() - 13, 6, 26), 3, 3);
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        const QPointF pos = e->position();
        if (m_drag) {
            const QRectF t = target();
            m_cut = qBound(0.0, (pos.x() - t.left()) / t.width(), 1.0);
            update();
            return;
        }
        const bool onLine = nearLine(pos);
        setCursor(onLine ? Qt::SizeHorCursor : Qt::PointingHandCursor);
        const int h = onLine ? -1 : hit(pos);
        if (h != m_hover) { m_hover = h; update(); }
    }
    void leaveEvent(QEvent*) override { if (m_hover >= 0) { m_hover = -1; update(); } }
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        if (nearLine(e->position())) { m_drag = true; return; }
        const int h = hit(e->position());
        if (h < 0) return;
        const auto regs = ThemeScene::regions(ThemeScene::Scene(m_scene), target());
        const auto& r = regs.at(h);
        if (onPick) onPick(r.group, r.panel);
        m_hover = -1;
        flashSelection(r.group, r.panel);
    }
    void mouseReleaseEvent(QMouseEvent*) override { m_drag = false; }

public:
    std::function<QString(ThemeScene::Group, const QString&)> m_labelFor;

private:
    bool nearLine(const QPointF& pos) const
    {
        if (!m_compare) return false;
        const QRectF t = target();
        return t.contains(pos) && qAbs(pos.x() - (t.left() + t.width() * m_cut)) <= 6;
    }
    int hit(const QPointF& pos) const
    {
        const QRectF t = target();
        if (!t.contains(pos)) return -1;
        const auto regs = ThemeScene::regions(ThemeScene::Scene(m_scene), t);
        for (int i = 0; i < regs.size(); ++i) if (regs.at(i).rect.contains(pos)) return i;
        return -1;
    }

    QPixmap render(const Theme::MiraTheme& th, const QString& fp)
    {
        const QRectF t = target();
        const QSize sz(qMax(1, qRound(t.width())), qMax(1, qRound(t.height())));
        const qreal dpr = devicePixelRatioF();
        QPixmap bg;
        if (m_scene != ThemeScene::Editor && !th.backgroundImage.isEmpty()) {
            bg = ImageCache::instance()->get(th.backgroundImage, ImageCache::kSceneCap);
            if (bg.isNull()) bg = ImageCache::instance()->get(th.backgroundImage, ImageCache::kThumbCap);
        }
        const QString key = QStringLiteral("%1|%2|%3x%4@%5|%6").arg(fp).arg(m_scene).arg(sz.width()).arg(sz.height())
                                .arg(dpr).arg(bg.cacheKey());
        const auto it = m_cache.constFind(key);
        if (it != m_cache.constEnd()) return it.value();
        QPixmap pm(QSize(qCeil(sz.width() * dpr), qCeil(sz.height() * dpr)));
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        {
            QPainter p(&pm);
            ThemeScene::paintScene(p, QRectF(QPointF(0, 0), sz), th, ThemeScene::Scene(m_scene), bg);
        }
        if (m_cache.size() >= 12) m_cache.remove(m_order.takeFirst());
        m_cache.insert(key, pm);
        m_order.append(key);
        return pm;
    }

    Theme::MiraTheme m_edited, m_original;
    QString m_editedFp, m_originalFp;
    int m_scene = 0;
    bool m_compare = false;
    qreal m_cut = 0.5;
    bool m_drag = false;
    int m_hover = -1;
    ThemeScene::Group m_selGroup = ThemeScene::Group::Sheet;
    QString m_selPanel;
    bool m_selectedOn = false;
    QTimer m_flash;
    QHash<QString, QPixmap> m_cache;
    QStringList m_order;
};

} // namespace CreatorDetail

using namespace CreatorDetail;

// ======================================================================

ThemeEditorDialog::ThemeEditorDialog(const Theme::MiraTheme& base, QWidget* parent)
    : QDialog(parent)
    , m_theme(base)
    , m_original(base)
{
    setObjectName(QStringLiteral("themeEditorDialog"));
    setWindowTitle(tr("Criador de Temas"));
    setModal(true);
    setMinimumSize(1000, 700);
    QSize want(1200, 860);
    if (const QScreen* s = (parent ? parent->screen() : QGuiApplication::primaryScreen())) {
        const QSize avail = s->availableGeometry().size() - QSize(40, 60);
        want = want.boundedTo(avail).expandedTo(minimumSize());
    }
    resize(want);

    buildUi();
    applyStyle();
    setScene(ThemeScene::Writing);
    setGroup(Sheet);
    refreshPreview();
}

// ------------------------------------------------------------------ montagem

void ThemeEditorDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 14);
    root->setSpacing(12);

    m_canvas = new Canvas(this);
    m_canvas->setObjectName(QStringLiteral("creatorCanvas"));
    m_canvas->m_labelFor = [](ThemeScene::Group g, const QString& panel) {
        if (g == ThemeScene::Group::Panels && !panel.isEmpty()) return panelName(panel);
        return groupName(Group(int(g)));
    };
    m_canvas->onPick = [this](ThemeScene::Group g, const QString& panel) {
        setGroup(Group(int(g)), panel);
    };
    root->addWidget(m_canvas, 1);

    // Linha 1: nome, cenas, antes e depois, desfazer, cancelar, salvar
    auto* r1 = new QHBoxLayout;
    r1->setSpacing(14);
    m_nameEdit = new QLineEdit(m_theme.name, this);
    m_nameEdit->setObjectName(QStringLiteral("creatorName"));
    m_nameEdit->setPlaceholderText(tr("Nome do tema"));
    m_nameEdit->setFixedWidth(240);
    QFont nf(QStringLiteral("Lora"));
    nf.setPixelSize(16);
    nf.setWeight(QFont::DemiBold);
    m_nameEdit->setFont(nf);
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString& s) { m_theme.name = s; });
    r1->addWidget(m_nameEdit);
    auto* tabs = new QHBoxLayout;
    tabs->setSpacing(16);
    const QStringList names = { tr("Escrita"), tr("Gavetas"), tr("Editor") };
    for (int i = 0; i < names.size(); ++i) {
        auto* b = new QPushButton(names.at(i), this);
        b->setObjectName(QStringLiteral("sceneTab"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, i]() { setScene(i); });
        m_sceneTabs.append(b);
        tabs->addWidget(b);
    }
    r1->addLayout(tabs);
    r1->addStretch(1);
    m_compare = new Toggle(tr("Antes e depois"), this);
    m_compare->setToolTip(tr("Compara com o tema de partida. Arraste a linha pra mover."));
    connect(m_compare, &QAbstractButton::toggled, this, [this](bool on) { m_canvas->setCompare(on); });
    r1->addWidget(m_compare);
    m_undoBtn = new QPushButton(tr("↶ Desfazer"), this);
    m_undoBtn->setObjectName(QStringLiteral("linkBtn"));
    m_undoBtn->setCursor(Qt::PointingHandCursor);
    m_undoBtn->setEnabled(false);
    connect(m_undoBtn, &QPushButton::clicked, this, &ThemeEditorDialog::undo);
    r1->addWidget(m_undoBtn);
    auto* cancel = new QPushButton(tr("Cancelar"), this);
    cancel->setObjectName(QStringLiteral("footBtn"));
    cancel->setCursor(Qt::PointingHandCursor);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    r1->addWidget(cancel);
    auto* save = new QPushButton(tr("Salvar"), this);
    save->setObjectName(QStringLiteral("applyBtn"));
    save->setCursor(Qt::PointingHandCursor);
    save->setDefault(true);
    connect(save, &QPushButton::clicked, this, [this]() {
        m_theme.name = m_nameEdit->text().trimmed();
        if (m_theme.name.isEmpty()) m_theme.name = tr("Tema sem nome");
        accept();
    });
    r1->addWidget(save);
    root->addLayout(r1);

    // Linha 2: quem está sendo editado + os controles daquilo
    auto* r2 = new QHBoxLayout;
    r2->setSpacing(22);
    auto* who = new QWidget(this);
    who->setObjectName(QStringLiteral("creatorWho"));
    who->setFixedWidth(250);
    auto* wl = new QVBoxLayout(who);
    wl->setContentsMargins(0, 0, 18, 0);
    wl->setSpacing(6);
    auto* head = new QLabel(tr("Editando"), who);
    head->setObjectName(QStringLiteral("sectionHead"));
    QFont hf = px(10, QFont::DemiBold);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    hf.setCapitalization(QFont::AllUppercase);
    head->setFont(hf);
    wl->addWidget(head);
    m_groupTitle = new QLabel(who);
    m_groupTitle->setObjectName(QStringLiteral("groupTitle"));
    m_groupTitle->setTextFormat(Qt::RichText);
    QFont gf(QStringLiteral("Lora"));
    gf.setPixelSize(18);
    gf.setWeight(QFont::DemiBold);
    m_groupTitle->setFont(gf);
    wl->addWidget(m_groupTitle);
    auto* pills = new QGridLayout;
    pills->setHorizontalSpacing(4);
    pills->setVerticalSpacing(4);
    const QList<QPair<Group, QString>> pl = {
        { Background, tr("Fundo") }, { Panels, tr("Painéis") }, { Icons, tr("Ícones") }, { Sheet, tr("Folha") },
        { Title, tr("Título") }, { Text, tr("Texto") }, { Accent, tr("Destaque") },
    };
    for (int i = 0; i < pl.size(); ++i) {
        auto* b = new QPushButton(pl.at(i).second, who);
        b->setObjectName(QStringLiteral("pill"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        const Group g = pl.at(i).first;
        b->setProperty("group", int(g));
        connect(b, &QPushButton::clicked, this, [this, g]() { setGroup(g); m_canvas->flashSelection(ThemeScene::Group(int(g)), QString()); });
        pills->addWidget(b, i / 4, i % 4);
        m_pills.append(b);
    }
    wl->addLayout(pills);
    wl->addStretch(1);
    r2->addWidget(who);

    m_controls = new QWidget(this);
    m_controls->setObjectName(QStringLiteral("creatorControls"));
    m_controlsLay = new QHBoxLayout(m_controls);
    m_controlsLay->setContentsMargins(0, 4, 0, 0);
    m_controlsLay->setSpacing(18);
    r2->addWidget(m_controls, 1);
    auto* strip = new QWidget(this);
    strip->setFixedHeight(118);
    strip->setLayout(r2);
    r2->setContentsMargins(0, 0, 0, 0);
    root->addWidget(strip);
}

// ------------------------------------------------------------------ estado

QString ThemeEditorDialog::groupName(Group g)
{
    switch (g) {
    case Background: return tr("Fundo");
    case Panels:     return tr("Painéis e barras");
    case Icons:      return tr("Ícones");
    case Sheet:      return tr("Folha");
    case Title:      return tr("Título do capítulo");
    case Text:       return tr("Texto");
    case Accent:     return tr("Destaque");
    }
    return QString();
}

QString ThemeEditorDialog::panelName(const QString& key)
{
    if (key == Theme::PanelKey::TopToolbar) return tr("Barra de cima");
    if (key == Theme::PanelKey::LeftBar)    return tr("Barra lateral");
    if (key == Theme::PanelKey::RefMenu)    return tr("Referência");
    if (key == Theme::PanelKey::Counter)    return tr("Contador");
    if (key == Theme::PanelKey::Drawers)    return tr("Gavetas");
    return key;
}

QString ThemeEditorDialog::onlyPanelText(const QString& key)
{
    if (key == Theme::PanelKey::TopToolbar) return tr("Só a barra de cima");
    if (key == Theme::PanelKey::LeftBar)    return tr("Só a barra lateral");
    if (key == Theme::PanelKey::RefMenu)    return tr("Só a Referência");
    if (key == Theme::PanelKey::Counter)    return tr("Só o contador");
    if (key == Theme::PanelKey::Drawers)    return tr("Só as gavetas");
    return key;
}

void ThemeEditorDialog::setScene(int scene)
{
    m_scene = scene;
    for (int i = 0; i < m_sceneTabs.size(); ++i) m_sceneTabs.at(i)->setChecked(i == scene);
    m_canvas->setScene(scene);
}

void ThemeEditorDialog::setGroup(Group g, const QString& panel)
{
    m_group = g;
    if (g == Panels) { if (!panel.isEmpty()) m_panel = panel; }
    else m_panel.clear();
    if (g == Icons) m_activeKey = QStringLiteral("iconColor");
    else if (g == Title) m_activeKey = QStringLiteral("docHeaderColor");
    refreshHeader();
    rebuildControls();
}

void ThemeEditorDialog::refreshHeader()
{
    QString t = groupName(m_group).toHtmlEscaped();
    if (m_group == Panels && !m_panel.isEmpty())
        t += QStringLiteral(" <span style='font-family:\"Segoe UI\";font-size:13px;font-weight:400;color:%1'>· %2</span>")
                 .arg(Theme::textMuted(), panelName(m_panel).toHtmlEscaped());
    m_groupTitle->setText(t);
    for (auto* b : std::as_const(m_pills)) b->setChecked(b->property("group").toInt() == int(m_group));
}

void ThemeEditorDialog::refreshPreview()
{
    m_canvas->setThemes(m_theme, m_original);
}

void ThemeEditorDialog::pushUndo()
{
    m_history.append(m_theme);
    if (m_history.size() > 80) m_history.removeFirst();
    m_undoBtn->setEnabled(true);
}

void ThemeEditorDialog::undo()
{
    if (m_history.isEmpty()) return;
    const QString name = m_theme.name;
    m_theme = m_history.takeLast();
    m_theme.name = name;   // o nome não entra no desfazer: ele tem o próprio campo
    m_undoBtn->setEnabled(!m_history.isEmpty());
    refreshPreview();
    rebuildControls();
}

void ThemeEditorDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Undo) && !m_nameEdit->hasFocus()) {
        undo();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

// ------------------------------------------------------------------ cores

QString ThemeEditorDialog::colorOf(const QString& key) const
{
    const auto& t = m_theme;
    if (key == QLatin1String("appBackground"))    return t.appBackground;
    if (key == QLatin1String("panelBackground"))  return t.panelBackground;
    if (key == QLatin1String("panelBorder"))      return t.panelBorder;
    if (key == QLatin1String("textPrimary"))      return t.textPrimary;
    if (key == QLatin1String("textMuted"))        return t.textMuted;
    if (key == QLatin1String("accentDefault"))    return t.accentDefault;
    if (key == QLatin1String("editorBackground")) return t.editorBackground;
    if (key == QLatin1String("editorTextColor"))  return t.editorTextColor;
    if (key == QLatin1String("pageShadowColor"))  return t.pageShadowColor;
    if (key == QLatin1String("iconColor"))        return t.iconColor.isEmpty() ? t.textMuted : t.iconColor;
    if (key == QLatin1String("docHeaderColor"))   return t.docHeaderColor.isEmpty() ? t.editorTextColor : t.docHeaderColor;
    if (key == QLatin1String("bgOverlayColor"))   return t.bgOverlayColor;
    if (key.startsWith(QLatin1String("panel:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        const auto pc = t.panelColors.value(parts.value(1));
        if (parts.value(2) == QLatin1String("bg")) return pc.background.isEmpty() ? t.panelBackground : pc.background;
        return pc.border.isEmpty() ? t.panelBorder : pc.border;
    }
    return QString();
}

void ThemeEditorDialog::setColor(const QString& key, const QString& v)
{
    auto& t = m_theme;
    if (key == QLatin1String("appBackground"))         t.appBackground = v;
    else if (key == QLatin1String("panelBackground"))  t.panelBackground = v;
    else if (key == QLatin1String("panelBorder"))      t.panelBorder = v;
    else if (key == QLatin1String("textPrimary"))      t.textPrimary = v;
    else if (key == QLatin1String("textMuted"))        t.textMuted = v;
    else if (key == QLatin1String("accentDefault"))    t.accentDefault = v;
    else if (key == QLatin1String("editorBackground")) t.editorBackground = v;
    else if (key == QLatin1String("editorTextColor"))  t.editorTextColor = v;
    else if (key == QLatin1String("pageShadowColor"))  t.pageShadowColor = v;
    else if (key == QLatin1String("iconColor"))        t.iconColor = v;
    else if (key == QLatin1String("docHeaderColor"))   t.docHeaderColor = v;
    else if (key == QLatin1String("bgOverlayColor"))   t.bgOverlayColor = v;
    else if (key.startsWith(QLatin1String("panel:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        auto& pc = t.panelColors[parts.value(1)];
        if (parts.value(2) == QLatin1String("bg")) pc.background = v; else pc.border = v;
    }
}

QString ThemeEditorDialog::colorLabel(const QString& key) const
{
    if (key == QLatin1String("appBackground"))    return tr("Cor do app");
    if (key == QLatin1String("panelBackground"))  return tr("Painéis");
    if (key == QLatin1String("panelBorder"))      return tr("Borda");
    if (key == QLatin1String("textPrimary"))      return tr("Texto da UI");
    if (key == QLatin1String("textMuted"))        return tr("Texto apagado");
    if (key == QLatin1String("accentDefault"))    return tr("Destaque");
    if (key == QLatin1String("editorBackground")) return tr("Folha");
    if (key == QLatin1String("editorTextColor"))  return tr("Texto");
    if (key == QLatin1String("pageShadowColor"))  return tr("Sombra");
    if (key == QLatin1String("iconColor"))        return tr("Ícones");
    if (key == QLatin1String("docHeaderColor"))   return tr("Cor do título");
    if (key == QLatin1String("bgOverlayColor"))   return tr("Cor do degradê");
    if (key.endsWith(QLatin1String(":bg")))       return tr("Cor");
    if (key.endsWith(QLatin1String(":bd")))       return tr("Borda");
    return key;
}

bool ThemeEditorDialog::colorHasAlpha(const QString& key) const
{
    // Como no editor de antes: borda e sombra aceitam transparência.
    return key == QLatin1String("panelBorder") || key == QLatin1String("pageShadowColor") || key.endsWith(QLatin1String(":bd"));
}

void ThemeEditorDialog::pickColor(const QString& key, QWidget* anchor)
{
    m_activeKey = key;
    for (auto* b : m_controls->findChildren<QAbstractButton*>()) {
        if (!b->property("key").isValid()) continue;
        static_cast<Swatch*>(b)->setMarked(b->property("key").toString() == key);
    }

    // Prévia ao vivo: o balão manda cada cor visitada, o app muda na hora; ao
    // cancelar volta o que estava.
    const Theme::MiraTheme before = m_theme;
    ColorPick::Choice cur;
    cur.color = c(colorOf(key));
    ColorPopover::Options opt;
    opt.title = colorLabel(key);
    opt.alpha = colorHasAlpha(key);
    auto* pop = new ColorPopover(cur, opt, this);
    const bool alpha = opt.alpha;
    auto normalized = [alpha](QColor col) { if (!alpha) col.setAlpha(255); return col; };
    connect(pop, &ColorPopover::previewed, this, [this, key, normalized](ColorPick::Choice ch) {
        if (!ch.color.isValid()) return;
        setColor(key, colorToString(normalized(ch.color)));
        refreshPreview();
    });
    connect(pop, &ColorPopover::finished, this, [this, key, before, normalized](ColorPick::Choice ch, ColorPopover::Result r) {
        m_theme = before;
        if (r == ColorPopover::Accepted && ch.color.isValid()) {
            pushUndo();
            setColor(key, colorToString(normalized(ch.color)));
        }
        refreshPreview();
        rebuildControls();
    });
    pop->popupAt(anchor->mapToGlobal(QPoint(anchor->width() / 2, 0)));
}

// ------------------------------------------------------------------ faixa

QWidget* ThemeEditorDialog::swatch(const QString& key, const QString& label)
{
    auto* sw = new Swatch(label.isEmpty() ? colorLabel(key) : label, 34, m_controls);
    sw->setProperty("key", key);
    sw->setColor(c(colorOf(key)));
    sw->setMarked(key == m_activeKey);
    sw->setToolTip(colorOf(key));
    connect(sw, &QAbstractButton::clicked, this, [this, key, sw]() { pickColor(key, sw); });
    return sw;
}

QWidget* ThemeEditorDialog::slider(const QString& label, int min, int max, int value, const QString& unit,
                                   const std::function<void(int)>& apply)
{
    auto* w = new QWidget(m_controls);
    w->setMinimumWidth(118);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(4);
    auto* top = new QHBoxLayout;
    auto* name = new QLabel(label, w);
    name->setObjectName(QStringLiteral("ctlLabel"));
    auto* val = new QLabel(QStringLiteral("%1%2").arg(value).arg(unit), w);
    val->setObjectName(QStringLiteral("ctlValue"));
    top->addWidget(name);
    top->addStretch(1);
    top->addWidget(val);
    l->addLayout(top);
    auto* s = new QSlider(Qt::Horizontal, w);
    s->setObjectName(QStringLiteral("creatorSlider"));
    s->setRange(min, max);
    s->setValue(value);
    s->setCursor(Qt::PointingHandCursor);
    l->addWidget(s);
    connect(s, &QSlider::sliderPressed, this, [this]() { pushUndo(); });
    connect(s, &QSlider::valueChanged, this, [this, s, val, unit, apply](int v) {
        if (!s->isSliderDown()) pushUndo();   // teclado/roda: cada passo desfaz sozinho
        val->setText(QStringLiteral("%1%2").arg(v).arg(unit));
        apply(v);
        refreshPreview();
    });
    return w;
}

// Overlay da mesa (do Mira Cover): o tipo de degradê em pílulas, e — só com um
// degradê escolhido — a cor e os knobs de opacidade/tamanho empilhados, pra
// caber na faixa ao lado da foto. O grão vale com ou sem degradê.
void ThemeEditorDialog::addOverlayControls()
{
    auto& t = m_theme;
    auto* box = new QWidget(m_controls);
    auto* bl = new QVBoxLayout(box);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(6);
    auto* lab = new QLabel(tr("Degradê"), box);
    lab->setObjectName(QStringLiteral("ctlLabel"));
    bl->addWidget(lab);
    auto* grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(2);
    grid->setVerticalSpacing(2);
    const QList<QPair<int, QString>> types = {
        { Theme::OverlayNone, tr("Nenhum") }, { Theme::OverlayBottom, tr("Embaixo") }, { Theme::OverlayTop, tr("Em cima") },
        { Theme::OverlayBoth, tr("Os dois") }, { Theme::OverlayVignette, tr("Vinheta") },
    };
    for (int i = 0; i < types.size(); ++i) {
        auto* b = new QPushButton(types.at(i).second, box);
        b->setObjectName(QStringLiteral("pill"));
        b->setCheckable(true);
        b->setChecked(t.bgOverlayType == types.at(i).first);
        b->setCursor(Qt::PointingHandCursor);
        const int type = types.at(i).first;
        connect(b, &QPushButton::clicked, this, [this, type]() {
            if (m_theme.bgOverlayType == type) { rebuildControls(); return; }
            pushUndo();
            m_theme.bgOverlayType = type;
            refreshPreview();
            rebuildControls();
        });
        grid->addWidget(b, i / 3, i % 3);
    }
    bl->addLayout(grid);
    bl->addStretch(1);
    box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_controlsLay->addWidget(box, 0, Qt::AlignVCenter);

    if (t.bgOverlayType != Theme::OverlayNone) {
        m_controlsLay->addWidget(swatch(QStringLiteral("bgOverlayColor")), 0, Qt::AlignVCenter);
        auto* knobs = new QWidget(m_controls);
        auto* kl = new QVBoxLayout(knobs);
        kl->setContentsMargins(0, 0, 0, 0);
        kl->setSpacing(8);
        kl->addWidget(slider(tr("Opacidade"), 0, 100, t.bgOverlayOpacity, QStringLiteral("%"),
                             [this](int v) { m_theme.bgOverlayOpacity = v; }));
        kl->addWidget(slider(tr("Tamanho"), 5, 100, t.bgOverlaySize, QStringLiteral("%"),
                             [this](int v) { m_theme.bgOverlaySize = v; }));
        m_controlsLay->addWidget(knobs, 0, Qt::AlignVCenter);
    }
    auto* grain = new QWidget(m_controls);
    auto* gl = new QVBoxLayout(grain);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(8);
    gl->addWidget(slider(tr("Grão"), 0, 100, t.bgGrain, QString(), [this](int v) { m_theme.bgGrain = v; }));
    gl->addWidget(slider(tr("Tamanho do grão"), 1, 12, t.bgGrainSize, QStringLiteral("×"),
                         [this](int v) { m_theme.bgGrainSize = v; }));
    m_controlsLay->addWidget(grain, 0, Qt::AlignVCenter);
}

QWidget* ThemeEditorDialog::link(const QString& text, const std::function<void()>& onClick)
{
    auto* b = new QPushButton(text, m_controls);
    b->setObjectName(QStringLiteral("linkBtn"));
    b->setCursor(Qt::PointingHandCursor);
    connect(b, &QPushButton::clicked, this, [onClick]() { onClick(); });
    return b;
}

QWidget* ThemeEditorDialog::hint(const QString& text, int maxWidth)
{
    auto* l = new QLabel(text, m_controls);
    l->setObjectName(QStringLiteral("ctlHint"));
    l->setWordWrap(true);
    l->setMaximumWidth(maxWidth);
    return l;
}

QWidget* ThemeEditorDialog::contrastBox()
{
    auto* w = new QWidget(m_controls);
    w->setObjectName(QStringLiteral("ctlBox"));
    auto* g = new QGridLayout(w);
    g->setContentsMargins(18, 0, 0, 0);
    g->setHorizontalSpacing(8);
    g->setVerticalSpacing(6);
    const QColor ed = c(m_theme.editorBackground);
    auto row = [&](int r, const QString& label, const QString& colorCss) {
        const QColor col = c(colorCss);
        const double v = ThemeScene::contrast(col, ed);
        auto* k = new QLabel(label, w);
        k->setObjectName(QStringLiteral("ctlLabel"));
        auto* val = new QLabel(QStringLiteral("%1 · %2").arg(levelWord(v), QLocale().toString(v, 'f', 1)), w);
        val->setObjectName(QStringLiteral("ctlValue"));
        g->addWidget(k, r, 0);
        g->addWidget(new Dot(col, w), r, 1, Qt::AlignVCenter);
        g->addWidget(val, r, 2);
    };
    auto* head = new QLabel(tr("Contraste com a folha"), w);
    head->setObjectName(QStringLiteral("ctlLabel"));
    g->addWidget(head, 0, 0, 1, 3);
    row(1, tr("Texto"), m_theme.editorTextColor);
    row(2, tr("Texto apagado"), m_theme.textMuted);
    return w;
}

QWidget* ThemeEditorDialog::photoPalette()
{
    if (m_theme.backgroundImage.isEmpty()) return nullptr;
    if (m_photoFor != m_theme.backgroundImage) {
        m_photoFor = m_theme.backgroundImage;
        m_photoColors = photoColors(m_theme.backgroundImage);
    }
    if (m_photoColors.isEmpty()) return nullptr;
    auto* w = new QWidget(m_controls);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(18, 0, 0, 0);
    l->setSpacing(6);
    auto* t = new QLabel(tr("Cores da foto → <b>%1</b>").arg(colorLabel(m_activeKey).toHtmlEscaped()), w);
    t->setObjectName(QStringLiteral("ctlLabel"));
    t->setTextFormat(Qt::RichText);
    l->addWidget(t);
    auto* row = new QHBoxLayout;
    row->setSpacing(5);
    for (const QColor& col : std::as_const(m_photoColors)) {
        auto* chip = new Swatch(QString(), 22, w);
        chip->setColor(col);
        chip->setToolTip(col.name());
        connect(chip, &QAbstractButton::clicked, this, [this, col]() {
            pushUndo();
            setColor(m_activeKey, col.name());
            refreshPreview();
            rebuildControls();
        });
        row->addWidget(chip);
    }
    row->addStretch(1);
    l->addLayout(row);
    return w;
}

QWidget* ThemeEditorDialog::imageButton()
{
    auto* b = new QPushButton(m_controls);
    b->setObjectName(QStringLiteral("imageBtn"));
    b->setCursor(Qt::PointingHandCursor);
    b->setText(m_theme.backgroundImage.isEmpty() ? tr("Imagem…") : tr("Trocar foto"));
    if (!m_theme.backgroundImage.isEmpty()) {
        QImageReader r(m_theme.backgroundImage);
        r.setAutoTransform(true);
        if (r.size().isValid()) r.setScaledSize(r.size().scaled(88, 56, Qt::KeepAspectRatioByExpanding));
        const QImage img = r.read();
        if (!img.isNull()) {
            b->setIcon(QIcon(QPixmap::fromImage(img)));
            b->setIconSize(QSize(44, 28));
        }
    }
    connect(b, &QPushButton::clicked, this, [this]() {
        const QString filter = tr("Imagens (*.png *.jpg *.jpeg *.bmp *.webp *.tiff)");
        const QString path = QFileDialog::getOpenFileName(this, tr("Escolher imagem de fundo"), QString(), filter);
        if (path.isEmpty()) return;
        pushUndo();
        m_theme.backgroundImage = path;
        refreshPreview();
        rebuildControls();
    });
    return b;
}

void ThemeEditorDialog::rebuildControls()
{
    // Refaz a faixa do grupo atual (é pequena; recriar é mais simples que sincronizar).
    while (QLayoutItem* it = m_controlsLay->takeAt(0)) {
        // Esconde já: o deleteLater só apaga na volta do laço de eventos, e
        // até lá o controle velho aparecia por cima do novo.
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        delete it;
    }
    auto add = [this](QWidget* w) { if (w) m_controlsLay->addWidget(w, 0, Qt::AlignVCenter); };
    auto& t = m_theme;

    switch (m_group) {
    case Background: {
        add(swatch(QStringLiteral("appBackground")));
        add(imageButton());
        if (!t.backgroundImage.isEmpty()) {
            add(link(tr("Remover foto"), [this]() {
                pushUndo();
                m_theme.backgroundImage.clear();
                refreshPreview();
                rebuildControls();
            }));
            auto* box = new QWidget(m_controls);
            auto* bl = new QVBoxLayout(box);
            bl->setContentsMargins(0, 0, 0, 0);
            bl->setSpacing(4);
            auto* lab = new QLabel(tr("Modo da foto"), box);
            lab->setObjectName(QStringLiteral("ctlLabel"));
            auto* combo = new QComboBox(box);
            combo->addItem(tr("Centralizar"), Theme::BgCenter);
            combo->addItem(tr("Repetir"), Theme::BgTile);
            combo->addItem(tr("Esticar"), Theme::BgStretch);
            combo->addItem(tr("Ajustar"), Theme::BgFit);
            combo->addItem(tr("Preencher"), Theme::BgZoom);
            combo->setCurrentIndex(qMax(0, combo->findData(t.backgroundMode)));
            connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, combo](int) {
                pushUndo();
                m_theme.backgroundMode = combo->currentData().toInt();
                refreshPreview();
            });
            bl->addWidget(lab);
            bl->addWidget(combo);
            add(box);
            add(photoPalette());
        }
        addOverlayControls();
        break;
    }
    case Panels: {
        const bool only = !m_panel.isEmpty() && t.panelColors.contains(m_panel);
        if (!m_panel.isEmpty()) {
            auto* box = new QWidget(m_controls);
            auto* bl = new QVBoxLayout(box);
            bl->setContentsMargins(0, 0, 0, 0);
            bl->setSpacing(6);
            auto* lab = new QLabel(tr("Vale pra"), box);
            lab->setObjectName(QStringLiteral("ctlLabel"));
            bl->addWidget(lab);
            auto* seg = new QHBoxLayout;
            seg->setSpacing(0);
            auto* all = new QPushButton(tr("Todos os painéis"), box);
            auto* one = new QPushButton(onlyPanelText(m_panel), box);
            for (auto* b : { all, one }) {
                b->setObjectName(QStringLiteral("segBtn"));
                b->setCheckable(true);
                b->setCursor(Qt::PointingHandCursor);
                b->setMinimumWidth(QFontMetrics(px(11)).horizontalAdvance(b->text()) + 26);
                seg->addWidget(b);
            }
            box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
            box->setMinimumWidth(all->minimumWidth() + one->minimumWidth());
            all->setProperty("pos", QStringLiteral("left"));
            one->setProperty("pos", QStringLiteral("right"));
            all->setChecked(!only);
            one->setChecked(only);
            connect(all, &QPushButton::clicked, this, [this]() {
                if (!m_theme.panelColors.contains(m_panel)) { rebuildControls(); return; }
                pushUndo();
                m_theme.panelColors.remove(m_panel);
                refreshPreview();
                rebuildControls();
            });
            connect(one, &QPushButton::clicked, this, [this]() {
                if (m_theme.panelColors.contains(m_panel)) { rebuildControls(); return; }
                pushUndo();
                m_theme.panelColors.insert(m_panel, { m_theme.panelBackground, m_theme.panelBorder });
                refreshPreview();
                rebuildControls();
            });
            bl->addLayout(seg);
            add(box);
        }
        if (only) {
            add(swatch(QStringLiteral("panel:%1:bg").arg(m_panel)));
            add(swatch(QStringLiteral("panel:%1:bd").arg(m_panel)));
            add(hint(tr("Arredondamento, opacidade e vidro continuam valendo pra todos. \"Todos os painéis\" devolve este painel pra cor global."), 320));
        } else {
            add(swatch(QStringLiteral("panelBackground")));
            add(swatch(QStringLiteral("panelBorder")));
            add(swatch(QStringLiteral("textPrimary")));
            add(slider(tr("Arredondamento"), 0, 24, t.panelRadius, tr("px"), [this](int v) { m_theme.panelRadius = v; }));
            add(slider(tr("Opacidade"), 0, 100, t.panelOpacity, QStringLiteral("%"), [this](int v) { m_theme.panelOpacity = v; }));
            add(slider(tr("Vidro"), 0, 30, t.panelBlur, tr("px"), [this](int v) { m_theme.panelBlur = v; }));
        }
        break;
    }
    case Icons:
        add(swatch(QStringLiteral("iconColor")));
        if (!t.iconColor.isEmpty())
            add(link(tr("Voltar pra cor do texto apagado"), [this]() {
                pushUndo();
                m_theme.iconColor.clear();
                refreshPreview();
                rebuildControls();
            }));
        add(hint(tr("Barra de cima (inclusive os botões de fonte, tamanho e entrelinha), barra lateral e os botões dos painéis. As gavetas mantêm a cor que você deu a cada uma."), 420));
        break;
    case Sheet:
        add(swatch(QStringLiteral("editorBackground")));
        add(slider(tr("Opacidade"), 20, 100, t.editorOpacity, QStringLiteral("%"), [this](int v) { m_theme.editorOpacity = v; }));
        {
            auto* tg = new Toggle(tr("Sombra"), m_controls);
            tg->setChecked(t.pageShadowEnabled);
            connect(tg, &QAbstractButton::toggled, this, [this](bool on) {
                pushUndo();
                m_theme.pageShadowEnabled = on;
                refreshPreview();
                QTimer::singleShot(0, this, [this]() { rebuildControls(); });
            });
            add(tg);
        }
        if (t.pageShadowEnabled) {
            add(swatch(QStringLiteral("pageShadowColor")));
            add(slider(tr("Raio"), 0, 100, t.pageShadowRadius, tr("px"), [this](int v) { m_theme.pageShadowRadius = v; }));
            add(slider(tr("Desloc. Y"), -50, 50, t.pageShadowOffset, tr("px"), [this](int v) { m_theme.pageShadowOffset = v; }));
        }
        break;
    case Title: {
        add(swatch(QStringLiteral("docHeaderColor"), tr("Cor")));
        auto* box = new QWidget(m_controls);
        auto* bl = new QVBoxLayout(box);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(4);
        auto* lab = new QLabel(tr("Fonte"), box);
        lab->setObjectName(QStringLiteral("ctlLabel"));
        auto* combo = new QComboBox(box);
        combo->setMinimumWidth(200);
        for (const QString& f : titleFonts()) {
            combo->addItem(f, f);
            QFont ff(f);
            ff.setPixelSize(14);
            combo->setItemData(combo->count() - 1, ff, Qt::FontRole);
        }
        const QString cur = t.docHeaderFont.isEmpty() ? QStringLiteral("Lora") : t.docHeaderFont;
        combo->setCurrentIndex(qMax(0, combo->findData(cur)));
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, combo](int) {
            pushUndo();
            const QString f = combo->currentData().toString();
            m_theme.docHeaderFont = (f == QLatin1String("Lora")) ? QString() : f;
            refreshPreview();
        });
        bl->addWidget(lab);
        bl->addWidget(combo);
        add(box);
        if (!t.docHeaderColor.isEmpty() || !t.docHeaderFont.isEmpty())
            add(link(tr("Voltar pro padrão"), [this]() {
                pushUndo();
                m_theme.docHeaderColor.clear();
                m_theme.docHeaderFont.clear();
                refreshPreview();
                rebuildControls();
            }));
        add(hint(tr("O título no alto da folha. Padrão: a cor do texto, em Lora."), 300));
        break;
    }
    case Text:
        add(swatch(QStringLiteral("editorTextColor")));
        add(swatch(QStringLiteral("textMuted")));
        add(contrastBox());
        break;
    case Accent: {
        add(swatch(QStringLiteral("accentDefault")));
        // Cores das gavetas: a cor que o escritor deu a cada gaveta (padrão)
        // ou o destaque do tema, lá dentro (botão de criar, etiquetas, barrinhas).
        auto* box = new QWidget(m_controls);
        auto* bl = new QVBoxLayout(box);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(6);
        auto* lab = new QLabel(tr("Cores das gavetas"), box);
        lab->setObjectName(QStringLiteral("ctlLabel"));
        bl->addWidget(lab);
        auto* seg = new QHBoxLayout;
        seg->setSpacing(0);
        auto* own = new QPushButton(tr("Usar a cor da própria gaveta"), box);
        auto* theme = new QPushButton(tr("Usar a cor do tema"), box);
        for (auto* b : { own, theme }) {
            b->setObjectName(QStringLiteral("segBtn"));
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setMinimumWidth(QFontMetrics(px(11)).horizontalAdvance(b->text()) + 26);
            seg->addWidget(b);
        }
        own->setProperty("pos", QStringLiteral("left"));
        theme->setProperty("pos", QStringLiteral("right"));
        own->setChecked(!t.drawerAccentFromTheme);
        theme->setChecked(t.drawerAccentFromTheme);
        auto set = [this](bool fromTheme) {
            if (m_theme.drawerAccentFromTheme != fromTheme) {
                pushUndo();
                m_theme.drawerAccentFromTheme = fromTheme;
                refreshPreview();
            }
            rebuildControls();
        };
        connect(own, &QPushButton::clicked, this, [set]() { set(false); });
        connect(theme, &QPushButton::clicked, this, [set]() { set(true); });
        bl->addLayout(seg);
        box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        box->setMinimumWidth(own->minimumWidth() + theme->minimumWidth());
        add(box);
        add(hint(t.drawerAccentFromTheme
                     ? tr("Aparece no anel do contador, no item aberto da barra e, dentro das gavetas, no botão de criar, nas etiquetas e nas barrinhas.")
                     : tr("Aparece no anel do contador e no item aberto da barra. Dentro das gavetas vale a cor que você deu a cada uma (veja em Gavetas)."), 360));
        break;
    }
    }
    m_controlsLay->addStretch(1);
}

void ThemeEditorDialog::applyStyle()
{
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        #themeEditorDialog { background: %1; }
        #themeEditorDialog QLabel { color: %2; }
        QLabel#sectionHead { color: %4; }
        QLabel#groupTitle { color: %3; }
        QLabel#ctlLabel { color: %4; font-size: 11px; }
        QLabel#ctlValue { color: %3; font-size: 12px; }
        QLabel#ctlHint { color: %4; font-size: 12px; }
        QLineEdit#creatorName {
            background: transparent; border: none; border-bottom: 1px solid %6;
            color: %3; padding: 3px 2px;
        }
        QLineEdit#creatorName:focus { border-bottom-color: %9; }
        QPushButton#sceneTab {
            background: transparent; border: none; border-bottom: 2px solid transparent;
            color: %4; font-size: 12px; padding: 2px 0px 5px 0px;
        }
        QPushButton#sceneTab:hover { color: %3; }
        QPushButton#sceneTab:checked { color: %3; border-bottom: 2px solid %9; }
        QPushButton#pill {
            background: transparent; border: 1px solid transparent; border-radius: 10px;
            color: %4; font-size: 11px; padding: 2px 8px;
        }
        QPushButton#pill:hover { color: %3; }
        QPushButton#pill:checked { color: %3; border-color: %6; background: %7; }
        QPushButton#segBtn {
            background: transparent; border: 1px solid %6; color: %4; font-size: 11px; padding: 5px 10px;
        }
        QPushButton#segBtn[pos="left"] { border-top-left-radius: @radius-control; border-bottom-left-radius: @radius-control; }
        QPushButton#segBtn[pos="right"] { border-left: none; border-top-right-radius: @radius-control; border-bottom-right-radius: @radius-control; }
        QPushButton#segBtn:checked { background: %7; color: %3; }
        QPushButton#applyBtn {
            background: %9; color: %5; border: 1px solid %9; border-radius: @radius-control;
            padding: 6px 16px; font-size: 13px; font-weight: 600;
        }
        QPushButton#footBtn {
            background: transparent; color: %2; border: 1px solid %6;
            border-radius: @radius-control; padding: 6px 14px; font-size: 12px;
        }
        QPushButton#footBtn:hover { border-color: %10; color: %3; }
        QPushButton#linkBtn {
            background: transparent; border: none; color: %4; font-size: 12px;
            padding: 5px 8px; border-radius: @radius-control;
        }
        QPushButton#linkBtn:hover { color: %3; background: %7; }
        QPushButton#linkBtn:disabled { color: %8; }
        QPushButton#imageBtn {
            background: transparent; border: 1px solid %6; border-radius: @radius-control;
            color: %2; font-size: 11px; padding: 4px 10px;
        }
        QPushButton#imageBtn:hover { border-color: %10; color: %3; }
        #themeEditorDialog QComboBox {
            background: %5; color: %2; border: 1px solid %6; border-radius: @radius-control;
            padding: 4px 8px; font-size: 12px;
        }
        QSlider#creatorSlider::groove:horizontal { height: 4px; background: %6; border-radius: 2px; }
        QSlider#creatorSlider::sub-page:horizontal { background: %9; border-radius: 2px; }
        QSlider#creatorSlider::handle:horizontal {
            background: %3; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px;
        }
    )")).arg(
        Theme::chromeBackground(),     // 1
        Theme::textPrimary(),       // 2
        Theme::textBright(),        // 3
        Theme::textMuted(),         // 4
        Theme::panelBackground(),   // 5
        Theme::panelBorder(),       // 6
        Theme::hoverOverlay(),      // 7
        Theme::disabledText(),      // 8
        Theme::accentDefault(),     // 9
        Theme::accentInfo()         // 10
    ));
}
