#include "CharacterSheetPanel.h"
#include "SheetDialogs.h"

#include "AvatarUtils.h"
#include "CharacterImageGenDialog.h"
#include "CharacterImageGenService.h"
#include "EditorLayout.h"
#include "ElementsStore.h"
#include "ImageCropDialog.h"
#include "RoleTiers.h"
#include "SheetTemplatesStore.h"
#include "Theme.h"
#include "TimelineTracksTypes.h"

#include <QAbstractTextDocumentLayout>
#include <QActionGroup>
#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QHash>
#include <utility>

using Tracks::alpha;
using Tracks::mix;

namespace {

constexpr const char* kKeyLastLayout = "sheet/lastLayout";
const QString kDossie     = QStringLiteral("dossie");
const QString kInfobox    = QStringLiteral("infobox");
const QString kProntuario = QStringLiteral("prontuario");

// O ⋮⋮ das seções fica pendurado na margem da página, à esquerda do título:
// cada bloco que não é seção ganha esse recuo pra tudo alinhar pelo texto.
constexpr int kGripW = 24;

QString css(const QColor& c)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 3));
}

QWidget* indented(QWidget* w)
{
    auto* box = new QWidget;
    auto* l = new QVBoxLayout(box);
    l->setContentsMargins(kGripW, 0, 0, 0);
    l->setSpacing(0);
    l->addWidget(w);
    return box;
}

// Botão que só aparece com o mouse sobre o campo, sem empurrar o layout.
void makeReveal(QWidget* w)
{
    w->setProperty("reveal", true);
    QSizePolicy sp = w->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    w->setSizePolicy(sp);
    w->setVisible(false);
}

QToolButton* makeRemoveBtn(const QString& tip)
{
    auto* b = new QToolButton;
    b->setObjectName(QStringLiteral("shX"));
    b->setText(QStringLiteral("×"));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    makeReveal(b);
    return b;
}

// Layout que quebra linha (os botões do índice do Prontuário).
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent, int spacing) : QLayout(parent), m_spacing(spacing) {}
    ~FlowLayout() override { while (QLayoutItem* it = takeAt(0)) delete it; }
    void addItem(QLayoutItem* it) override { m_items.append(it); }
    int count() const override { return m_items.size(); }
    QLayoutItem* itemAt(int i) const override { return m_items.value(i); }
    QLayoutItem* takeAt(int i) override { return (i >= 0 && i < m_items.size()) ? m_items.takeAt(i) : nullptr; }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return place(QRect(0, 0, w, 0), false); }
    void setGeometry(const QRect& r) override { QLayout::setGeometry(r); place(r, true); }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override
    {
        QSize s;
        for (QLayoutItem* it : m_items) s = s.expandedTo(it->minimumSize());
        const QMargins m = contentsMargins();
        return s + QSize(m.left() + m.right(), m.top() + m.bottom());
    }

private:
    int place(const QRect& rect, bool apply) const
    {
        const QMargins m = contentsMargins();
        const QRect r = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
        int x = r.x(), y = r.y(), line = 0;
        for (QLayoutItem* it : m_items) {
            const QSize sh = it->sizeHint();
            if (x + sh.width() > r.right() + 1 && line > 0) {
                x = r.x();
                y += line + m_spacing;
                line = 0;
            }
            if (apply) it->setGeometry(QRect(QPoint(x, y), sh));
            x += sh.width() + m_spacing;
            line = qMax(line, sh.height());
        }
        return y + line - rect.y() + m.bottom();
    }
    QList<QLayoutItem*> m_items;
    int m_spacing;
};

} // namespace

// Cores da ficha: as da página do editor (não as da chrome), pra ela ser a
// folha e não um painel por cima dela.
struct CharacterSheetPanel::Colors {
    QColor ink, dim, accent, rule, page, paper, hover, focus, box;
    static Colors now()
    {
        const Tracks::Palette pal = Tracks::Palette::onPage();
        Colors c;
        c.page = QColor(Theme::editorBackground());
        c.ink = QColor(Theme::editorTextColor());
        if (!c.ink.isValid()) c.ink = pal.ink;
        c.dim = mix(c.ink, c.page, 0.58);
        c.accent = pal.accent;
        c.rule = alpha(c.ink, 0.16);
        c.hover = alpha(c.ink, 0.06);
        c.focus = alpha(c.accent, 0.10);
        c.box = mix(c.ink, c.page, 0.04);
        // Mesma cor E opacidade da folha (ver DocHeaderBar): a ficha mora no
        // lugar da página, logo abaixo da faixa do título.
        c.paper = c.page;
        c.paper.setAlpha(qBound(0, Theme::editorOpacity(), 100) * 255 / 100);
        return c;
    }
};

// A foto da ficha. Com imagem: o retrato preenche o quadro (corte puxado pro
// terço de cima) e, com o mouse em cima, aparecem Trocar · Gerar · Remover.
// Sem imagem: o mesmo quadrado com silhueta do Novo personagem, apagado até o
// mouse passar, com Escolher · Gerar.
class SheetPhoto : public QWidget {
public:
    std::function<void()> onPick, onGenerate, onRemove;

    explicit SheetPhoto(const QSize& size, QWidget* parent = nullptr) : QWidget(parent)
    {
        setFixedSize(size);
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(150);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_hover = v.toReal();
            update();
        });
    }

    void setImage(const QPixmap& pm)
    {
        m_pix = pm;
        m_cache = QPixmap();
        update();
    }

protected:
    void enterEvent(QEnterEvent* e) override { animateTo(1.0); QWidget::enterEvent(e); }
    void leaveEvent(QEvent* e) override { m_hotPill = -1; animateTo(0.0); QWidget::leaveEvent(e); }
    void resizeEvent(QResizeEvent* e) override { m_cache = QPixmap(); QWidget::resizeEvent(e); }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        int hot = -1;
        for (int i = 0; i < m_pills.size(); ++i)
            if (m_pills[i].contains(e->position())) hot = i;
        if (hot != m_hotPill) { m_hotPill = hot; update(); }
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        for (int i = 0; i < m_pills.size(); ++i) {
            if (!m_pills[i].contains(e->position())) continue;
            const auto act = pillActions().value(i);
            if (act) act();
            return;
        }
        if (onPick) onPick();
    }

    void paintEvent(QPaintEvent*) override
    {
        const Tracks::Palette pal = Tracks::Palette::onPage();
        const QColor ink(Theme::editorTextColor());
        const QColor page(Theme::editorBackground());
        const qreal h = m_hover;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath box;
        box.addRoundedRect(r, 6, 6);

        const bool has = !m_pix.isNull();
        if (has) {
            // Cache na resolução real da tela: no Retina a foto era esticada e borrava.
            const qreal dpr = devicePixelRatioF();
            if (m_cache.isNull() || !qFuzzyCompare(m_cache.devicePixelRatio(), dpr)) {
                const QSize target = (QSizeF(size()) * dpr).toSize();
                const QPixmap sc = m_pix.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                const int dx = (sc.width() - target.width()) / 2;
                const int dy = int((sc.height() - target.height()) * 0.25);
                m_cache = sc.copy(dx, dy, target.width(), target.height());
                m_cache.setDevicePixelRatio(dpr);
            }
            p.save();
            p.setClipPath(box);
            p.drawPixmap(QPointF(0, 0), m_cache);
            if (h > 0.01) {
                QLinearGradient g(0, height() * 0.45, 0, height());
                g.setColorAt(0, QColor(0, 0, 0, 0));
                g.setColorAt(1, QColor(0, 0, 0, int(140 * h)));
                p.fillRect(rect(), g);
            }
            p.restore();
        } else {
            p.save();
            p.setOpacity(0.5 + 0.5 * h);
            p.fillPath(box, mix(alpha(pal.accent, 0.09), alpha(ink, 0.05), h));
            p.setClipPath(box);
            const qreal side = qMin(width(), height()) * 0.795;
            const qreal s = side / 250.0;
            p.translate((width() - side) / 2.0, height() - side);
            p.scale(s, s);
            QPainterPath fig;
            fig.addEllipse(QPointF(125, 92), 52, 52);
            fig.moveTo(28, 250);
            fig.cubicTo(32, 184, 72, 148, 125, 148);
            fig.cubicTo(178, 148, 218, 184, 222, 250);
            fig.closeSubpath();
            p.fillPath(fig, mix(alpha(pal.accent, 0.5), alpha(ink, 0.4), h));
            p.restore();
            p.save();
            p.setOpacity(0.5 + 0.5 * h);
            QPen pen(mix(pal.accent, alpha(ink, 0.32), h), 1);
            pen.setStyle(h < 0.5 ? Qt::DashLine : Qt::SolidLine);
            p.setPen(pen);
            p.drawPath(box);
            p.restore();
        }

        // Pílulas de ação: em linha se couberem, senão empilhadas no meio.
        m_pills.clear();
        if (h <= 0.01) return;
        const QStringList labels = has
            ? QStringList{ QCoreApplication::translate("CharacterSheetPanel", "Trocar"),
                           QCoreApplication::translate("CharacterSheetPanel", "Gerar"),
                           QCoreApplication::translate("CharacterSheetPanel", "Remover") }
            : QStringList{ QCoreApplication::translate("CharacterSheetPanel", "Escolher"),
                           QCoreApplication::translate("CharacterSheetPanel", "Gerar") };
        const QFont f = Tracks::uiFont(11, QFont::Medium);
        const QFontMetricsF fm(f);
        const qreal ph = 20, gap = 4;
        QList<qreal> widths;
        qreal rowW = -gap;
        for (const QString& l : labels) { widths << fm.horizontalAdvance(l) + 18; rowW += widths.last() + gap; }
        const bool row = rowW <= width() - 12;
        qreal x = (width() - rowW) / 2.0;
        qreal y = row ? height() - 8 - ph : (height() - (labels.size() * (ph + gap) - gap)) / 2.0;
        p.setOpacity(h);
        p.setFont(f);
        for (int i = 0; i < labels.size(); ++i) {
            const QRectF pr = row ? QRectF(x, y, widths[i], ph)
                                  : QRectF((width() - widths[i]) / 2.0, y, widths[i], ph);
            m_pills << pr;
            const bool hot = i == m_hotPill;
            if (has) {
                p.setPen(QPen(QColor(255, 255, 255, 150), 1));
                p.setBrush(hot ? QColor(255, 255, 255, 60) : QColor(0, 0, 0, 70));
            } else {
                p.setPen(QPen(hot ? alpha(ink, 0.45) : alpha(ink, 0.22), 1));
                p.setBrush(hot ? mix(ink, page, 0.08) : page);
            }
            p.drawRoundedRect(pr.adjusted(0.5, 0.5, -0.5, -0.5), ph / 2, ph / 2);
            p.setPen(has ? QColor(Qt::white) : ink);
            p.drawText(pr, Qt::AlignCenter, labels[i]);
            if (row) x += widths[i] + gap; else y += ph + gap;
        }
    }

private:
    QList<std::function<void()>> pillActions() const
    {
        if (!m_pix.isNull()) return { onPick, onGenerate, onRemove };
        return { onPick, onGenerate };
    }
    void animateTo(qreal v)
    {
        m_anim->stop();
        m_anim->setStartValue(m_hover);
        m_anim->setEndValue(v);
        m_anim->start();
    }

    QPixmap m_pix, m_cache;
    QVariantAnimation* m_anim = nullptr;
    qreal m_hover = 0;
    QList<QRectF> m_pills;
    int m_hotPill = -1;
};

CharacterSheetPanel::CharacterSheetPanel(ProjectModel* model, ElementsStore* elements, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_elements(elements)
{
    setObjectName(QStringLiteral("characterSheetPanel"));

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(600);
    connect(m_saveTimer, &QTimer::timeout, this, &CharacterSheetPanel::commitNow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    buildFindBar();
    outer->addWidget(m_findBar);
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setObjectName(QStringLiteral("sheetScroll"));
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->viewport()->setAutoFillBackground(false);
    outer->addWidget(m_scroll);
    connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged, this, &CharacterSheetPanel::updateStickies);

    // Aviso no pé (remoção com "Desfazer", modelo aplicado…), por cima da folha.
    m_toast = new QWidget(this);
    m_toast->setObjectName(QStringLiteral("shToast"));
    m_toast->setAttribute(Qt::WA_StyledBackground, true);
    auto* th = new QHBoxLayout(m_toast);
    th->setContentsMargins(14, 7, 8, 7);
    th->setSpacing(10);
    m_toastText = new QLabel;
    m_toastText->setObjectName(QStringLiteral("shToastText"));
    th->addWidget(m_toastText);
    m_toastUndo = new QPushButton(tr("Desfazer"));
    m_toastUndo->setObjectName(QStringLiteral("shToastUndo"));
    m_toastUndo->setCursor(Qt::PointingHandCursor);
    connect(m_toastUndo, &QPushButton::clicked, this, [this]() {
        m_toast->hide();
        if (auto undo = std::exchange(m_undo, {})) undo();
    });
    th->addWidget(m_toastUndo);
    m_toast->hide();
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, this, [this]() { m_toast->hide(); m_undo = {}; });

    applyStyle();
    // Sem isto a ficha ficava com as cores do tema em que foi aberta pela
    // primeira vez: folha escura embaixo de um DocHeader creme.
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, [this]() {
        applyStyle();
        if (!m_itemId.isEmpty()) rebuild();
    });
    // Editou o personagem (nome, apelidos, papel, foto) por outra porta.
    if (m_elements)
        connect(m_elements, &ElementsStore::changed, this, [this]() { if (!m_itemId.isEmpty()) refreshPhoto(); });
}

QString CharacterSheetPanel::defaultLayout()
{
    const QString v = QSettings().value(QLatin1String(kKeyLastLayout), kDossie).toString();
    return (v == kInfobox || v == kProntuario) ? v : kDossie;
}

QString CharacterSheetPanel::layoutId() const
{
    const QString& v = m_sheet.layout;
    return (v == kDossie || v == kInfobox || v == kProntuario) ? v : defaultLayout();
}

void CharacterSheetPanel::setLayout(const QString& id)
{
    QSettings().setValue(QLatin1String(kKeyLastLayout), id);   // as fichas novas nascem assim
    if (m_sheet.layout == id) return;
    m_sheet.layout = id;
    commitNow();
    rebuild();
    m_scroll->verticalScrollBar()->setValue(0);
}

void CharacterSheetPanel::applyStyle()
{
    const Colors c = Colors::now();
    QPalette pal = palette();
    pal.setColor(QPalette::PlaceholderText, alpha(c.dim, 0.85));
    pal.setColor(QPalette::Text, c.ink);
    setPalette(pal);

    setStyleSheet(Theme::qss(QStringLiteral(
        "#characterSheetPanel { background: transparent; }"
        "#sheetScroll, #sheetOuter { background: transparent; border: none; }"
        "#sheetPage { background: %1; }"
        "QLineEdit#shLabel, QLineEdit#shValue, QLineEdit#shSecTitle {"
        "  border: none; background: transparent; padding: 1px 2px; border-radius: 4px; }"
        "QLineEdit#shLabel { color: %3; }"
        "QLineEdit#shValue, QLineEdit#shSecTitle { color: %2; }"
        "QLineEdit#shLabel:hover, QLineEdit#shValue:hover, QLineEdit#shSecTitle:hover { background: %5; }"
        "QLineEdit#shLabel:focus, QLineEdit#shValue:focus, QLineEdit#shSecTitle:focus { background: %6; }"
        "QTextEdit#shText { border: none; background: transparent; color: %2; padding: 2px; border-radius: 4px; }"
        "QTextEdit#shText:hover { background: %5; }"
        "QTextEdit#shText:focus { background: %6; }"
        "QToolButton#shX { border: none; background: transparent; color: %3; font-size: 16px; padding: 0 4px; }"
        "QToolButton#shX:hover { color: %4; }"
        "QLabel#shGrip { color: %3; font-size: 13px; }"
        "QLabel#shRole { color: %4; }"
        "QLabel#shName { color: %2; }"
        "QLabel#shAlias { color: %3; }"
        "QFrame#shRule { background: %7; border: none; min-height: 1px; max-height: 1px; }"
        "QFrame#shDropLine { background: %4; border: none; }"
        "QPushButton#shAdd { border: 1px dashed %7; border-radius: 13px; background: transparent;"
        "  color: %3; padding: 4px 13px; font-size: 12.5px; }"
        "QPushButton#shAdd:hover { color: %2; border-color: %8; }"
        "QWidget#shBox { background: %9; border: 1px solid %7; border-radius: 8px; }"
        "QWidget#shRow { border-bottom: 1px solid %7; }"
        "QWidget#shRailRow { border-top: 1px solid %7; }"
        "QWidget#shRail { background: %1; border-right: 1px solid %7; }"
        "QWidget#shRailTop { border-bottom: 1px solid %7; }"
        "QWidget#shToc { background: %1; border-bottom: 1px solid %7; }"
        "QPushButton#shTocBtn { border: 1px solid %7; border-radius: 12px; background: transparent;"
        "  color: %3; padding: 3px 11px; font-size: 12px; }"
        "QPushButton#shTocBtn:hover { color: %2; border-color: %8; }"
        "QWidget#shToast { background: %2; border-radius: 8px; }"
        "QLabel#shToastText { color: %10; font-size: 12.5px; }"
        "QPushButton#shToastUndo { border: none; background: transparent; color: %10; font-weight: 700;"
        "  font-size: 12.5px; padding: 2px 6px; text-decoration: underline; }"
        "#sheetFindBar { background: %10; border-bottom: 1px solid %7; }"
        "QLineEdit#sheetFindInput { background: %9; border: 1px solid %7; border-radius: @radius-control;"
        "  color: %2; padding: 4px 8px; }"
        "QLabel#sheetFindCount { color: %3; font-size: 12px; }"
        "QToolButton#sheetFindBtn { border: none; background: transparent; color: %3; font-size: 13px; padding: 0 4px; }"
        "QToolButton#sheetFindBtn:hover { color: %2; }"
    ).arg(css(c.paper), css(c.ink), css(c.dim), css(c.accent), css(c.hover), css(c.focus),
          css(c.rule), css(alpha(c.ink, 0.4)), css(c.box))
     .arg(css(c.page))));
}

void CharacterSheetPanel::buildFindBar()
{
    m_findBar = new QWidget(this);
    m_findBar->setObjectName(QStringLiteral("sheetFindBar"));
    m_findBar->setAttribute(Qt::WA_StyledBackground, true);
    auto* h = new QHBoxLayout(m_findBar);
    h->setContentsMargins(12, 6, 12, 6);
    h->setSpacing(6);

    m_findInput = new QLineEdit;
    m_findInput->setObjectName(QStringLiteral("sheetFindInput"));
    m_findInput->setPlaceholderText(tr("Buscar na ficha…"));
    m_findInput->setClearButtonEnabled(true);
    m_findInput->installEventFilter(this);   // Esc fecha
    connect(m_findInput, &QLineEdit::textChanged, this, &CharacterSheetPanel::onFindChanged);
    connect(m_findInput, &QLineEdit::returnPressed, this, &CharacterSheetPanel::findNext);
    h->addWidget(m_findInput, 1);

    m_findCount = new QLabel(QStringLiteral("0/0"));
    m_findCount->setObjectName(QStringLiteral("sheetFindCount"));
    h->addWidget(m_findCount);

    auto mkBtn = [this](const QString& glyph, void (CharacterSheetPanel::*slot)()) {
        auto* b = new QToolButton;
        b->setText(glyph);
        b->setObjectName(QStringLiteral("sheetFindBtn"));
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, slot);
        return b;
    };
    h->addWidget(mkBtn(QStringLiteral("↑"), &CharacterSheetPanel::findPrev));
    h->addWidget(mkBtn(QStringLiteral("↓"), &CharacterSheetPanel::findNext));
    h->addWidget(mkBtn(QStringLiteral("✕"), &CharacterSheetPanel::closeFind));

    m_findBar->setVisible(false);
}

void CharacterSheetPanel::openFind()
{
    if (!m_findBar) return;
    m_findBar->setVisible(true);
    m_findInput->setFocus();
    m_findInput->selectAll();
    onFindChanged(m_findInput->text());
}

void CharacterSheetPanel::onFindChanged(const QString& q)
{
    m_matches.clear();
    m_activeMatch = -1;
    if (!q.isEmpty()) {
        for (QWidget* w : m_searchFields) {
            QString text;
            if (auto* te = qobject_cast<QTextEdit*>(w)) text = te->toPlainText();
            else if (auto* le = qobject_cast<QLineEdit*>(w)) text = le->text();
            int from = 0;
            while (true) {
                const int idx = text.indexOf(q, from, Qt::CaseInsensitive);
                if (idx < 0) break;
                m_matches.append({ w, idx, int(q.size()) });
                from = idx + q.size();
            }
        }
    }
    applyFindHighlights();
    if (!m_matches.isEmpty()) gotoMatch(0);
    else if (m_findCount) m_findCount->setText(QStringLiteral("0/0"));
}

void CharacterSheetPanel::applyFindHighlights()
{
    QColor hl(Theme::accentWarning());
    if (!hl.isValid()) hl = QColor(255, 213, 79);
    hl.setAlpha(110);
    for (QTextEdit* te : m_textEdits) {
        QList<QTextEdit::ExtraSelection> sels;
        for (const FindMatch& m : m_matches) {
            if (m.w != te) continue;
            QTextCursor c(te->document());
            c.setPosition(m.pos);
            c.setPosition(m.pos + m.len, QTextCursor::KeepAnchor);
            QTextEdit::ExtraSelection es;
            es.cursor = c;
            es.format.setBackground(hl);
            sels.append(es);
        }
        te->setExtraSelections(sels);
    }
}

void CharacterSheetPanel::gotoMatch(int idx)
{
    if (idx < 0 || idx >= m_matches.size()) return;
    m_activeMatch = idx;
    const FindMatch& m = m_matches[idx];
    if (auto* te = qobject_cast<QTextEdit*>(m.w)) {
        QTextCursor c(te->document());
        c.setPosition(m.pos);
        c.setPosition(m.pos + m.len, QTextCursor::KeepAnchor);
        te->setTextCursor(c);
    } else if (auto* le = qobject_cast<QLineEdit*>(m.w)) {
        le->setSelection(m.pos, m.len);
    }
    if (m_scroll && m.w) m_scroll->ensureWidgetVisible(m.w, 50, 120);
    if (m_findCount)
        m_findCount->setText(QStringLiteral("%1/%2").arg(idx + 1).arg(m_matches.size()));
}

void CharacterSheetPanel::findNext()
{
    if (m_matches.isEmpty()) return;
    gotoMatch((m_activeMatch + 1) % m_matches.size());
}

void CharacterSheetPanel::findPrev()
{
    if (m_matches.isEmpty()) return;
    gotoMatch((m_activeMatch - 1 + m_matches.size()) % m_matches.size());
}

void CharacterSheetPanel::closeFind()
{
    if (m_findBar) m_findBar->setVisible(false);
    m_matches.clear();
    m_activeMatch = -1;
    for (QTextEdit* te : m_textEdits) te->setExtraSelections({});
    setFocus();
}

void CharacterSheetPanel::openItem(const QString& itemId)
{
    const DrawerItem* item = m_model ? m_model->findDrawerItem(itemId) : nullptr;
    if (!item || !item->isSheet) return;
    if (m_saveTimer->isActive()) { m_saveTimer->stop(); commitNow(); }   // a ficha anterior não perde nada
    m_itemId = itemId;
    m_elementId = item->elementId;
    m_sheet = item->sheet;
    // Ficha que ainda não tem forma adota a última escolhida, e guarda.
    if (m_sheet.layout.isEmpty()) {
        m_sheet.layout = defaultLayout();
        m_model->updateDrawerItemSheet(itemId, m_sheet);
    }
    m_toast->hide();
    m_undo = {};
    rebuild();
    m_scroll->verticalScrollBar()->setValue(0);
}

void CharacterSheetPanel::setContentFont(const QFont& f)
{
    if (m_contentFont.family() == f.family()) { m_contentFont = f; return; }
    m_contentFont = f;
    if (!m_itemId.isEmpty()) rebuild();
}

QFont CharacterSheetPanel::contentFont(int px, int weight, bool italic) const
{
    QFont f(m_contentFont.family().isEmpty() ? QStringLiteral("Georgia") : m_contentFont.family());
    f.setPixelSize(px);
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    return f;
}

void CharacterSheetPanel::scheduleSave()
{
    if (m_saveTimer) m_saveTimer->start();
}

void CharacterSheetPanel::commitNow()
{
    if (m_itemId.isEmpty() || !m_model) return;
    m_model->updateDrawerItemSheet(m_itemId, m_sheet);
    emit edited();
}

void CharacterSheetPanel::setFieldValue(const QString& id, const QString& value)
{
    for (auto& f : m_sheet.fields) {
        if (f.id == id) { f.value = value; return; }
    }
}

QList<SheetField> CharacterSheetPanel::fieldsOfKind(const QString& kind) const
{
    QList<SheetField> out;
    for (const auto& f : m_sheet.fields) {
        const bool isText = f.kind == QStringLiteral("text");
        if (isText == (kind == QStringLiteral("text"))) out << f;
    }
    return out;
}

void CharacterSheetPanel::addField(const QString& kind)
{
    SheetField f;
    f.id = ProjectModel::uid();
    f.kind = kind;
    f.label = (kind == QStringLiteral("text")) ? tr("Nova seção") : tr("Novo dado");
    // Coluna do leiaute antigo: mantida pra quem abrir o projeto numa versão anterior.
    f.column = kind == QStringLiteral("text") ? QStringLiteral("right") : QStringLiteral("left");
    f.order = m_sheet.fields.size();
    m_sheet.fields.append(f);
    m_focusAfterRebuild = f.id;
    commitNow();
    rebuild();
}

void CharacterSheetPanel::removeField(const QString& id)
{
    for (int i = 0; i < m_sheet.fields.size(); ++i) {
        if (m_sheet.fields[i].id != id) continue;
        const SheetField gone = m_sheet.fields.takeAt(i);
        commitNow();
        rebuild();
        const QString name = gone.label.trimmed().isEmpty() ? tr("Campo") : gone.label.trimmed();
        showToast(tr("\"%1\" removido").arg(name), [this, gone, i]() {
            m_sheet.fields.insert(qMin(i, int(m_sheet.fields.size())), gone);
            commitNow();
            rebuild();
        });
        return;
    }
}

void CharacterSheetPanel::moveSection(const QString& id, int targetIndex)
{
    QStringList order;
    for (const auto& f : fieldsOfKind(QStringLiteral("text"))) order << f.id;
    const int from = order.indexOf(id);
    if (from < 0 || targetIndex == from || targetIndex == from + 1) return;
    order.removeAt(from);
    order.insert(targetIndex > from ? targetIndex - 1 : targetIndex, id);
    // Os dados ficam onde estão; só as seções trocam de lugar entre si.
    QHash<QString, SheetField> byId;
    for (const auto& f : m_sheet.fields) byId.insert(f.id, f);
    int k = 0;
    for (auto& f : m_sheet.fields)
        if (f.kind == QStringLiteral("text")) f = byId.value(order.at(k++));
    for (int i = 0; i < m_sheet.fields.size(); ++i) m_sheet.fields[i].order = i;
    commitNow();
    rebuild();
}

void CharacterSheetPanel::applyTemplate(const QString& templateId)
{
    const SheetTemplate* t = m_templates ? m_templates->findById(templateId) : nullptr;
    if (!t) return;
    // Os campos do modelo vêm primeiro, na ordem dele; campo com o mesmo nome
    // reaproveita o que já foi escrito. O que o modelo não tem continua no fim:
    // nada do que você escreveu sai da ficha.
    QList<SheetField> pool = m_sheet.fields;
    QList<SheetField> out;
    for (const SheetField& tf : t->sheet.fields) {
        int hit = -1;
        for (int i = 0; i < pool.size(); ++i)
            if (pool[i].label.trimmed().compare(tf.label.trimmed(), Qt::CaseInsensitive) == 0) { hit = i; break; }
        if (hit >= 0) {
            out << pool.takeAt(hit);
        } else {
            SheetField f = tf;
            f.id = ProjectModel::uid();
            f.value.clear();
            out << f;
        }
    }
    out << pool;
    for (int i = 0; i < out.size(); ++i) out[i].order = i;
    const QList<SheetField> before = m_sheet.fields;
    m_sheet.fields = out;
    commitNow();
    rebuild();
    showToast(tr("Modelo \"%1\" aplicado").arg(t->name), [this, before]() {
        m_sheet.fields = before;
        commitNow();
        rebuild();
    });
}

void CharacterSheetPanel::saveAsTemplate()
{
    if (!m_templates) return;
    bool ok = false;
    const QString name = Sheets::askText(this, tr("Salvar como modelo"), tr("Nome do modelo"),
        QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    m_templates->add(name, m_sheet);
    showToast(tr("Modelo \"%1\" salvo").arg(name.trimmed()));
}

void CharacterSheetPanel::pickPhoto()
{
    if (m_elementId.isEmpty() || !m_elements) return;
    const QString dataUrl = ImageCropDialog::pickAndCropImage(this);
    if (dataUrl.isEmpty()) return;
    if (const Element* e = m_elements->findElement(m_elementId)) {
        Element copy = *e;
        copy.image = dataUrl;
        m_elements->updateElement(m_elementId, copy);
    }
    refreshPhoto();
    scheduleSave();
}

void CharacterSheetPanel::generateImage()
{
    if (m_elementId.isEmpty() || !m_elements) return;
    const Element* e = m_elements->findElement(m_elementId);
    CharacterImageGenDialog dlg(e ? e->name : QString(),
        CharacterImageGenService::sheetContextText(m_sheet), this);
    dlg.setProjectRoot(m_projectRoot);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString dataUrl = ImageCropDialog::cropImage(dlg.resultImage(), this);
    if (dataUrl.isEmpty()) return;
    if (const Element* cur = m_elements->findElement(m_elementId)) {
        Element copy = *cur;
        copy.image = dataUrl;
        m_elements->updateElement(m_elementId, copy);
    }
    refreshPhoto();
    scheduleSave();
}

void CharacterSheetPanel::removePhoto()
{
    if (m_elementId.isEmpty() || !m_elements) return;
    const Element* e = m_elements->findElement(m_elementId);
    if (!e || e->image.isEmpty()) return;
    const QString before = e->image;
    Element copy = *e;
    copy.image.clear();
    m_elements->updateElement(m_elementId, copy);
    refreshPhoto();
    scheduleSave();
    const QString elementId = m_elementId;
    showToast(tr("Foto removida"), [this, elementId, before]() {
        if (const Element* cur = m_elements->findElement(elementId)) {
            Element back = *cur;
            back.image = before;
            m_elements->updateElement(elementId, back);
        }
        refreshPhoto();
        scheduleSave();
    });
}

void CharacterSheetPanel::refreshPhoto()
{
    const Element* e = (!m_elementId.isEmpty() && m_elements)
        ? m_elements->findElement(m_elementId) : nullptr;
    QPixmap pm;
    if (e && !e->image.isEmpty()) pm = AvatarUtils::decodeDataUrl(e->image);
    for (const auto& ph : m_photos) if (ph) ph->setImage(pm);
    refreshIdentity();
}

void CharacterSheetPanel::refreshIdentity()
{
    const DrawerItem* item = m_model ? m_model->findDrawerItem(m_itemId) : nullptr;
    const Element* e = (!m_elementId.isEmpty() && m_elements)
        ? m_elements->findElement(m_elementId) : nullptr;
    const QString name = e && !e->name.isEmpty() ? e->name
                       : item && !item->title.isEmpty() ? item->title : tr("Personagem");
    QString role = item ? item->role : QString();
    if (role.isEmpty() && e) role = e->role;
    const QString alias = e ? e->aliases.join(QStringLiteral(", ")) : QString();
    for (const auto& l : m_names) if (l) l->setText(name);
    for (const auto& l : m_aliases) if (l) { l->setText(alias); l->setVisible(!alias.isEmpty()); }
    for (const auto& l : m_roles) {
        if (!l) continue;
        l->setText(role.isEmpty() ? QString() : RoleTiers::roleDisplayName(role).toUpper());
        l->setVisible(!role.isEmpty());
    }
}

// ---------------------------------------------------------------- peças

QWidget* CharacterSheetPanel::buildPhoto(const QSize& size)
{
    auto* ph = new SheetPhoto(size);
    ph->onPick = [this]() { pickPhoto(); };
    ph->onGenerate = [this]() { generateImage(); };
    ph->onRemove = [this]() { removePhoto(); };
    m_photos << ph;
    return ph;
}

QWidget* CharacterSheetPanel::buildIdentity(int namePx, Qt::Alignment align, bool withAlias)
{
    auto* box = new QWidget;
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(5);

    auto* role = new QLabel;
    role->setObjectName(QStringLiteral("shRole"));
    QFont rf = Tracks::uiFont(10.5, QFont::Bold);
    rf.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
    role->setFont(rf);
    role->setAlignment(align);
    m_roles << role;
    v->addWidget(role);

    auto* name = new QLabel;
    name->setObjectName(QStringLiteral("shName"));
    name->setFont(contentFont(namePx, QFont::Bold));
    name->setWordWrap(true);
    name->setAlignment(align);
    name->setToolTip(tr("Duplo clique pra editar nome, apelidos e papel"));
    name->setProperty("isName", true);
    name->installEventFilter(this);
    m_names << name;
    v->addWidget(name);

    if (withAlias) {
        auto* alias = new QLabel;
        alias->setObjectName(QStringLiteral("shAlias"));
        alias->setFont(contentFont(qMax(15, namePx / 2 - 2), QFont::Normal, true));
        alias->setWordWrap(true);
        alias->setAlignment(align);
        m_aliases << alias;
        v->addWidget(alias);
    }
    return box;
}

QWidget* CharacterSheetPanel::buildFact(const SheetField& f, FactStyle style)
{
    const QString id = f.id;
    auto* box = new QWidget;
    box->setProperty("fieldBox", true);
    box->installEventFilter(this);
    if (style == FactStyle::Row) box->setObjectName(QStringLiteral("shRow"));
    if (style == FactStyle::Rail) box->setObjectName(QStringLiteral("shRailRow"));
    if (style != FactStyle::Dossie) box->setAttribute(Qt::WA_StyledBackground, true);

    auto* label = new QLineEdit(f.label);
    label->setObjectName(QStringLiteral("shLabel"));
    label->setPlaceholderText(tr("Rótulo"));
    label->setProperty("fieldId", id);
    label->setCursorPosition(0);
    if (style == FactStyle::Row) {
        label->setFont(Tracks::uiFont(11.5, QFont::DemiBold));
    } else {
        QFont lf = Tracks::uiFont(10.5, QFont::DemiBold);
        lf.setCapitalization(QFont::AllUppercase);
        lf.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
        label->setFont(lf);
    }
    connect(label, &QLineEdit::textEdited, this, [this, id](const QString& t) {
        for (auto& ff : m_sheet.fields) if (ff.id == id) { ff.label = t; break; }
        scheduleSave();
    });

    auto* value = new QLineEdit(f.value);
    value->setObjectName(QStringLiteral("shValue"));
    value->setPlaceholderText(QStringLiteral("—"));
    value->setFont(contentFont(style == FactStyle::Dossie ? 18 : 16));
    value->setCursorPosition(0);
    connect(value, &QLineEdit::textEdited, this, [this, id](const QString& t) {
        setFieldValue(id, t);
        scheduleSave();
    });
    m_searchFields << label << value;

    auto* del = makeRemoveBtn(tr("Remover dado"));
    connect(del, &QToolButton::clicked, this, [this, id]() { removeField(id); });

    auto* g = new QGridLayout(box);
    g->setHorizontalSpacing(8);
    g->setVerticalSpacing(2);
    if (style == FactStyle::Row) {
        g->setContentsMargins(0, 6, 0, 6);
        label->setFixedWidth(86);
        g->addWidget(label, 0, 0);
        g->addWidget(value, 0, 1);
        g->addWidget(del, 0, 2);
        g->setColumnStretch(1, 1);
    } else {
        g->setContentsMargins(0, style == FactStyle::Rail ? 9 : 0, 0, 0);
        g->addWidget(label, 0, 0);
        g->addWidget(del, 0, 1);
        g->addWidget(value, 1, 0, 1, 2);
        g->setColumnStretch(0, 1);
    }
    return box;
}

QWidget* CharacterSheetPanel::buildSection(const SheetField& f)
{
    const QString id = f.id;
    auto* box = new QWidget;
    box->setProperty("fieldBox", true);
    box->setProperty("sectionId", id);
    box->installEventFilter(this);
    auto* g = new QGridLayout(box);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(0);
    g->setVerticalSpacing(6);

    auto* grip = new QLabel(QStringLiteral("⋮⋮"));
    grip->setObjectName(QStringLiteral("shGrip"));
    grip->setFixedWidth(kGripW);
    grip->setCursor(Qt::OpenHandCursor);
    grip->setToolTip(tr("Arrastar pra mudar a ordem"));
    grip->setProperty("gripFor", id);
    grip->installEventFilter(this);
    makeReveal(grip);
    g->addWidget(grip, 0, 0, Qt::AlignVCenter);

    auto* head = new QHBoxLayout;
    head->setSpacing(4);
    auto* title = new QLineEdit(f.label);
    title->setObjectName(QStringLiteral("shSecTitle"));
    title->setPlaceholderText(tr("Título"));
    title->setFont(contentFont(23, QFont::Bold));
    title->setProperty("fieldId", id);
    title->setCursorPosition(0);
    connect(title, &QLineEdit::textEdited, this, [this, id](const QString& t) {
        for (auto& ff : m_sheet.fields) if (ff.id == id) { ff.label = t; break; }
        scheduleSave();
    });
    head->addWidget(title, 1);
    auto* del = makeRemoveBtn(tr("Remover seção"));
    connect(del, &QToolButton::clicked, this, [this, id]() { removeField(id); });
    head->addWidget(del);
    g->addLayout(head, 0, 1);

    auto* te = new QTextEdit;
    te->setObjectName(QStringLiteral("shText"));
    te->setLineWrapMode(QTextEdit::WidgetWidth);
    const QFont bodyFont = contentFont(17);
    te->document()->setDefaultFont(bodyFont);
    te->setHtml(f.value);
    {
        // O html salvo traz tamanho e família embutidos: a ficha manda.
        QTextCursor c(te->document());
        c.select(QTextCursor::Document);
        QTextCharFormat cf;
        cf.setFontFamilies({ bodyFont.family() });
        cf.setFontPointSize(17.0 * 72.0 / qMax(1, te->logicalDpiY()));
        c.mergeCharFormat(cf);
        te->setCurrentCharFormat(cf);
    }
    te->setPlaceholderText(tr("Escreva aqui…"));
    te->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    te->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    te->document()->setDocumentMargin(2);
    // O bloco cresce com o texto (a rolagem é a da ficha, não a dele).
    connect(te->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
            te, [te](const QSizeF& s) { te->setFixedHeight(qMax(34, int(s.height()) + 6)); });
    te->setFixedHeight(34);
    connect(te, &QTextEdit::textChanged, this, [this, id, te]() {
        setFieldValue(id, te->toHtml());
        scheduleSave();
    });
    m_textEdits << te;
    m_searchFields << title << te;
    g->addWidget(te, 1, 1);
    g->setColumnStretch(1, 1);

    m_sections << box;
    return box;
}

QPushButton* CharacterSheetPanel::buildAdd(const QString& kind)
{
    auto* b = new QPushButton(kind == QStringLiteral("text") ? tr("+ seção") : tr("+ dado"));
    b->setObjectName(QStringLiteral("shAdd"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    connect(b, &QPushButton::clicked, this, [this, kind]() { addField(kind); });
    return b;
}

QFrame* CharacterSheetPanel::buildRule()
{
    auto* r = new QFrame;
    r->setObjectName(QStringLiteral("shRule"));
    r->setFixedHeight(1);
    return r;
}

// ---------------------------------------------------------------- as três formas

void CharacterSheetPanel::rebuild()
{
    m_searchFields.clear();
    m_textEdits.clear();
    m_matches.clear();
    m_activeMatch = -1;
    m_photos.clear();
    m_names.clear();
    m_aliases.clear();
    m_roles.clear();
    m_stickies.clear();
    m_sections.clear();
    m_dragId.clear();

    auto* outer = new QWidget;
    outer->setObjectName(QStringLiteral("sheetOuter"));
    auto* outerLay = new QHBoxLayout(outer);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    // A folha: a largura da página do editor. A sobra à direita do painel é
    // só a da barra de rolagem.
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("sheetPage"));
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setFixedWidth(EditorLayout::pageWidth());
    page->installEventFilter(this);
    m_page = page;

    const QString form = layoutId();
    if (form == kInfobox) buildInfobox(page);
    else if (form == kProntuario) buildProntuario(page);
    else buildDossie(page);

    m_dropLine = new QFrame(page);
    m_dropLine->setObjectName(QStringLiteral("shDropLine"));
    m_dropLine->setFixedHeight(2);
    m_dropLine->hide();

    outerLay->addWidget(page);  // sem AlignTop: a folha preenche a altura da viewport
    outerLay->addStretch();

    m_content = outer;
    m_scroll->setWidget(m_content);  // substitui e destrói o conteúdo anterior
    refreshPhoto();

    if (m_findBar && m_findBar->isVisible())
        onFindChanged(m_findInput->text());

    QTimer::singleShot(0, this, [this]() {
        updateStickies();
        if (m_focusAfterRebuild.isEmpty()) return;
        const QString id = std::exchange(m_focusAfterRebuild, QString());
        for (QWidget* w : std::as_const(m_searchFields)) {
            auto* le = qobject_cast<QLineEdit*>(w);
            if (!le || le->property("fieldId").toString() != id) continue;
            m_scroll->ensureWidgetVisible(le, 50, 160);
            le->setFocus();
            le->selectAll();
            break;
        }
    });
}

// Dossiê: a folha é um documento. Foto e nome no topo, os dados numa faixa
// entre dois fios, as seções embaixo numa largura boa de ler.
void CharacterSheetPanel::buildDossie(QWidget* page)
{
    const int hm = qMax(44, EditorLayout::horizontalMargin());
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(hm - kGripW, 34, hm, 60);
    root->setSpacing(30);

    auto* head = new QWidget;
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(22);
    hl->addWidget(buildPhoto(QSize(118, 148)), 0, Qt::AlignBottom);
    hl->addWidget(buildIdentity(38, Qt::AlignLeft, true), 1, Qt::AlignBottom);
    root->addWidget(indented(head));

    const QList<SheetField> facts = fieldsOfKind(QStringLiteral("data"));
    auto* factsBox = new QWidget;
    auto* fv = new QVBoxLayout(factsBox);
    fv->setContentsMargins(0, 0, 0, 0);
    fv->setSpacing(18);
    fv->addWidget(buildRule());
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(26);
    grid->setVerticalSpacing(18);
    const int avail = page->width() - 2 * hm;
    const int cols = avail >= 560 ? 3 : 2;
    int i = 0;
    for (const SheetField& f : facts) { grid->addWidget(buildFact(f, FactStyle::Dossie), i / cols, i % cols); ++i; }
    grid->addWidget(buildAdd(QStringLiteral("data")), i / cols, i % cols, Qt::AlignLeft | Qt::AlignVCenter);
    for (int c = 0; c < cols; ++c) grid->setColumnStretch(c, 1);
    fv->addLayout(grid);
    fv->addWidget(buildRule());
    root->addWidget(indented(factsBox));

    auto* secs = new QWidget;
    secs->setMaximumWidth(kGripW + 660);
    auto* sv = new QVBoxLayout(secs);
    sv->setContentsMargins(0, 0, 0, 0);
    sv->setSpacing(30);
    for (const SheetField& f : fieldsOfKind(QStringLiteral("text"))) sv->addWidget(buildSection(f));
    auto* add = buildAdd(QStringLiteral("text"));
    sv->addWidget(indented(add), 0, Qt::AlignLeft);
    root->addWidget(secs);
    root->addStretch();
}

// Infobox: como a página de personagem de uma wiki. Nome e texto à esquerda,
// a caixa com foto, papel e dados à direita, acompanhando a rolagem.
void CharacterSheetPanel::buildInfobox(QWidget* page)
{
    const int hm = qMax(44, EditorLayout::horizontalMargin());
    constexpr int kBoxW = 250;
    const bool narrow = page->width() < 700;   // página estreita: a caixa desce pro topo

    auto* body = new QWidget;
    auto* bv = new QVBoxLayout(body);
    bv->setContentsMargins(0, 0, 0, 0);
    bv->setSpacing(26);

    auto* box = new QWidget(narrow ? nullptr : page);
    box->setObjectName(QStringLiteral("shBox"));
    box->setAttribute(Qt::WA_StyledBackground, true);
    auto* xv = new QVBoxLayout(box);
    xv->setContentsMargins(12, 12, 12, 12);
    xv->setSpacing(10);
    xv->addWidget(buildPhoto(QSize(kBoxW - 24, (kBoxW - 24) * 4 / 3)), 0, Qt::AlignHCenter);
    xv->addWidget(buildIdentity(18, Qt::AlignHCenter, false));
    auto* tbl = new QWidget;
    auto* tv = new QVBoxLayout(tbl);
    tv->setContentsMargins(0, 0, 0, 0);
    tv->setSpacing(0);
    tv->addWidget(buildRule());
    for (const SheetField& f : fieldsOfKind(QStringLiteral("data"))) tv->addWidget(buildFact(f, FactStyle::Row));
    xv->addWidget(tbl);
    xv->addWidget(buildAdd(QStringLiteral("data")), 0, Qt::AlignLeft);
    box->setFixedWidth(kBoxW);

    auto* lead = buildIdentity(36, Qt::AlignLeft, true);
    m_roles.removeLast();      // o papel já está na caixa
    lead->findChild<QLabel*>(QStringLiteral("shRole"))->hide();
    if (narrow) bv->addWidget(indented(box), 0, Qt::AlignLeft);
    bv->addWidget(indented(lead));
    for (const SheetField& f : fieldsOfKind(QStringLiteral("text"))) bv->addWidget(buildSection(f));
    bv->addWidget(indented(buildAdd(QStringLiteral("text"))), 0, Qt::AlignLeft);
    bv->addStretch();

    auto* root = new QHBoxLayout(page);
    root->setContentsMargins(hm - kGripW, 30, hm, 60);
    root->setSpacing(34);
    root->addWidget(body, 1);
    if (!narrow) {
        auto* holder = new QWidget;
        holder->setFixedWidth(kBoxW);
        holder->installEventFilter(this);
        root->addWidget(holder, 0, Qt::AlignTop);
        m_stickies << Sticky{ box, holder, 16 };
    }
}

// Prontuário: trilho com foto, nome e dados parado à esquerda; à direita, o
// índice das seções (grudado no topo) e as seções.
void CharacterSheetPanel::buildProntuario(QWidget* page)
{
    constexpr int kRailW = 250;
    const bool narrow = page->width() < 760;   // página estreita: o trilho vira topo

    auto* rail = new QWidget(narrow ? nullptr : page);
    rail->setObjectName(narrow ? QStringLiteral("shRailTop") : QStringLiteral("shRail"));
    rail->setAttribute(Qt::WA_StyledBackground, true);
    auto* rv = new QVBoxLayout(rail);
    rv->setContentsMargins(22, 28, 22, 24);
    rv->setSpacing(12);
    const int photoW = narrow ? 150 : kRailW - 44;
    rv->addWidget(buildPhoto(QSize(photoW, photoW * 5 / 4)), 0, Qt::AlignLeft);
    rv->addWidget(buildIdentity(25, Qt::AlignLeft, true));
    for (const SheetField& f : fieldsOfKind(QStringLiteral("data"))) rv->addWidget(buildFact(f, FactStyle::Rail));
    rv->addWidget(buildAdd(QStringLiteral("data")), 0, Qt::AlignLeft);
    if (!narrow) rail->setFixedWidth(kRailW);

    const QList<SheetField> secs = fieldsOfKind(QStringLiteral("text"));
    auto* main = new QWidget;
    auto* mv = new QVBoxLayout(main);
    mv->setContentsMargins(46 - kGripW, 0, 46, 60);
    mv->setSpacing(0);

    // Índice: um botão por seção, grudado no topo enquanto o resto rola.
    auto* toc = new QWidget(page);
    toc->setObjectName(QStringLiteral("shToc"));
    toc->setAttribute(Qt::WA_StyledBackground, true);
    auto* flow = new FlowLayout(toc, 6);
    flow->setContentsMargins(0, 16, 0, 12);
    for (const SheetField& f : secs) {
        auto* b = new QPushButton(f.label.trimmed().isEmpty() ? tr("Sem título") : f.label.trimmed());
        b->setObjectName(QStringLiteral("shTocBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        const QString id = f.id;
        connect(b, &QPushButton::clicked, this, [this, id, toc]() {
            for (const auto& s : std::as_const(m_sections)) {
                if (!s || s->property("sectionId").toString() != id) continue;
                const int y = s->mapTo(m_page, QPoint(0, 0)).y();
                m_scroll->verticalScrollBar()->setValue(y - toc->height() - 14);
                break;
            }
        });
        flow->addWidget(b);
    }
    auto* tocHolder = new QWidget;
    tocHolder->installEventFilter(this);
    mv->addWidget(indented(tocHolder));   // alinha pelo texto, não pelo ⋮⋮
    mv->addSpacing(24);
    m_stickies << Sticky{ toc, tocHolder, 0 };
    if (secs.isEmpty()) toc->hide();

    auto* list = new QWidget;
    auto* lv = new QVBoxLayout(list);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(34);
    for (const SheetField& f : secs) lv->addWidget(buildSection(f));
    lv->addWidget(indented(buildAdd(QStringLiteral("text"))), 0, Qt::AlignLeft);
    mv->addWidget(list);
    mv->addStretch();

    if (narrow) {
        auto* root = new QVBoxLayout(page);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        root->addWidget(rail);
        root->addWidget(main, 1);
    } else {
        auto* root = new QHBoxLayout(page);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        auto* holder = new QWidget;
        holder->setFixedWidth(kRailW);
        holder->installEventFilter(this);
        root->addWidget(holder, 0, Qt::AlignTop);
        root->addWidget(main, 1);
        m_stickies << Sticky{ rail, holder, 0 };
    }
}

// Os que grudam no topo: o widget flutua por cima do lugar que ocupa no layout
// (o "holder", que só reserva o espaço) e desce junto com a rolagem até
// encostar no pé da folha.
void CharacterSheetPanel::updateStickies()
{
    if (!m_page) return;
    const int top = m_scroll->verticalScrollBar()->value();
    for (const Sticky& s : std::as_const(m_stickies)) {
        if (!s.w || !s.holder) continue;
        if (!s.w->isVisibleTo(m_page)) {   // índice sem seções: não reserva espaço
            if (s.holder->height() != 0) s.holder->setFixedHeight(0);
            continue;
        }
        const QRect h(s.holder->mapTo(m_page, QPoint(0, 0)), s.holder->size());
        const int wh = s.w->hasHeightForWidth() ? s.w->heightForWidth(h.width())
                                                 : s.w->sizeHint().height();
        if (s.holder->height() != wh) s.holder->setFixedHeight(wh);   // o layout se ajeita e chama de novo
        int y = qMax(h.y(), top + s.gap);
        y = qMax(h.y(), qMin(y, m_page->height() - wh - s.gap));
        // Mais alto que a tela: grudado, o pé (com o "+ dado") nunca apareceria.
        if (wh + s.gap > m_scroll->viewport()->height()) y = h.y();
        s.w->setGeometry(h.x(), y, h.width(), wh);
        s.w->raise();
    }
    if (m_dropLine) m_dropLine->raise();
}

void CharacterSheetPanel::showToast(const QString& text, std::function<void()> undo)
{
    m_undo = std::move(undo);
    m_toastText->setText(text);
    m_toastUndo->setVisible(bool(m_undo));
    m_toast->adjustSize();
    const int pageW = qMin(width(), EditorLayout::pageWidth());
    m_toast->move((pageW - m_toast->width()) / 2, height() - m_toast->height() - 18);
    m_toast->show();
    m_toast->raise();
    m_toastTimer->start(m_undo ? 7000 : 2600);
}

void CharacterSheetPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_toast && m_toast->isVisible()) {
        const int pageW = qMin(width(), EditorLayout::pageWidth());
        m_toast->move((pageW - m_toast->width()) / 2, height() - m_toast->height() - 18);
    }
    QTimer::singleShot(0, this, &CharacterSheetPanel::updateStickies);
}

int CharacterSheetPanel::scrollBarRoom() const
{
    return m_scroll->verticalScrollBar()->sizeHint().width() + 6;
}

// ---------------------------------------------------------------- o menu ⋯

void CharacterSheetPanel::openMenu(const QRect& anchorGlobal)
{
    if (m_itemId.isEmpty()) return;
    QMenu m(this);
    auto* cap = m.addAction(tr("Exibir como"));
    cap->setEnabled(false);
    auto* group = new QActionGroup(&m);
    const QList<QPair<QString, QString>> forms = {
        { kDossie, tr("Dossiê") }, { kInfobox, tr("Infobox") }, { kProntuario, tr("Prontuário") } };
    for (const auto& form : forms) {
        QAction* a = m.addAction(form.second);
        a->setCheckable(true);
        a->setChecked(layoutId() == form.first);
        group->addAction(a);
        const QString id = form.first;
        connect(a, &QAction::triggered, this, [this, id]() { setLayout(id); });
    }
    m.addSeparator();
    m.addAction(tr("Adicionar dado"), this, [this]() { addField(QStringLiteral("data")); });
    m.addAction(tr("Adicionar seção"), this, [this]() { addField(QStringLiteral("text")); });
    m.addSeparator();
    QMenu* tm = m.addMenu(tr("Usar modelo"));
    if (!m_templates || m_templates->all().isEmpty()) {
        tm->setEnabled(false);
    } else {
        for (const SheetTemplate& t : m_templates->all()) {
            const QString id = t.id;
            tm->addAction(t.name, this, [this, id]() { applyTemplate(id); });
        }
    }
    QAction* save = m.addAction(tr("Salvar como modelo…"), this, [this]() { saveAsTemplate(); });
    save->setEnabled(m_templates != nullptr);
    m.addSeparator();
    m.addAction(tr("Buscar na ficha") + QStringLiteral("\tCtrl+F"), this, [this]() { openFind(); });
    const QSize sz = m.sizeHint();
    m.exec(QPoint(anchorGlobal.right() - sz.width() + 1, anchorGlobal.bottom() + 6));
}

// ---------------------------------------------------------------- eventos

bool CharacterSheetPanel::eventFilter(QObject* watched, QEvent* event)
{
    const QEvent::Type t = event->type();
    // Esc fecha a busca local.
    if (watched == m_findInput && t == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) { closeFind(); return true; }
    }
    // Duplo clique no nome: a folha de editar personagem.
    if (t == QEvent::MouseButtonDblClick && watched->property("isName").toBool()) {
        emit editCharacterRequested(m_itemId);
        return true;
    }
    // A folha ou um lugar reservado mudou de tamanho/posição: os grudados acompanham.
    if (t == QEvent::Resize || t == QEvent::Move) {
        bool watch = watched == m_page;
        for (const Sticky& s : std::as_const(m_stickies)) if (s.holder == watched) watch = true;
        if (watch) QTimer::singleShot(0, this, &CharacterSheetPanel::updateStickies);
    }
    // Arrastar seção pelo ⋮⋮: uma linha mostra onde ela vai cair.
    const QString gripFor = watched->property("gripFor").toString();
    if (!gripFor.isEmpty()) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (t == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
            m_dragId = gripFor;
            m_dropIndex = -1;
            static_cast<QWidget*>(watched)->setCursor(Qt::ClosedHandCursor);
            return true;
        }
        if (t == QEvent::MouseMove && !m_dragId.isEmpty() && m_page) {
            const int y = m_page->mapFromGlobal(me->globalPosition().toPoint()).y();
            int idx = m_sections.size();
            for (int i = 0; i < m_sections.size(); ++i) {
                const QWidget* s = m_sections[i];
                if (!s) continue;
                const QRect r(s->mapTo(m_page, QPoint(0, 0)), s->size());
                if (y < r.center().y()) { idx = i; break; }
            }
            m_dropIndex = idx;
            const QWidget* ref = m_sections.value(qMin(idx, int(m_sections.size()) - 1));
            if (ref) {
                const QRect r(ref->mapTo(m_page, QPoint(0, 0)), ref->size());
                const int ly = idx < m_sections.size() ? r.top() - 15 : r.bottom() + 15;
                m_dropLine->setGeometry(r.x() + kGripW, ly, r.width() - kGripW, 2);
                m_dropLine->show();
                m_dropLine->raise();
            }
            // Perto da borda, a ficha rola sozinha.
            const QPoint vp = m_scroll->viewport()->mapFromGlobal(me->globalPosition().toPoint());
            QScrollBar* sb = m_scroll->verticalScrollBar();
            if (vp.y() < 40) sb->setValue(sb->value() - 12);
            else if (vp.y() > m_scroll->viewport()->height() - 40) sb->setValue(sb->value() + 12);
            return true;
        }
        if (t == QEvent::MouseButtonRelease && !m_dragId.isEmpty()) {
            static_cast<QWidget*>(watched)->setCursor(Qt::OpenHandCursor);
            if (m_dropLine) m_dropLine->hide();
            const QString id = std::exchange(m_dragId, QString());
            const int idx = std::exchange(m_dropIndex, -1);
            if (idx >= 0) QTimer::singleShot(0, this, [this, id, idx]() { moveSection(id, idx); });
            return true;
        }
    }
    // Hover-reveal do × e do ⋮⋮ de cada campo.
    auto* box = qobject_cast<QWidget*>(watched);
    if (box && box->property("fieldBox").toBool() && (t == QEvent::Enter || t == QEvent::Leave)) {
        const bool on = t == QEvent::Enter || box->rect().contains(box->mapFromGlobal(QCursor::pos()));
        for (QWidget* w : box->findChildren<QWidget*>())
            if (w->property("reveal").toBool()) w->setVisible(on || (!m_dragId.isEmpty() && w->property("gripFor").toString() == m_dragId));
    }
    return QWidget::eventFilter(watched, event);
}
