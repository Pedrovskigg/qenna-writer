#include "SheetDialogs.h"

#include "CoverUtils.h"
#include "SheetDialog.h"
#include "TimelineChrono.h"
#include "TimelineFillAssist.h"
#include "TimelineTracksTypes.h"

#include <QButtonGroup>
#include <QCoreApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStandardPaths>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Tracks;

namespace Sheets {

namespace {

// ── um texto ─────────────────────────────────────────────────────────────────
class TextSheet : public SheetDialog {
public:
    TextSheet(QWidget* parent, const QString& eyebrow, const QString& placeholder,
              const QString& initial, const QString& okText, bool multiline, bool allowEmpty,
              const QString& hint)
        : SheetDialog(parent, multiline ? 460 : 400)
    {
        setEyebrow(eyebrow);
        if (multiline) {
            m_area = textArea(card(), 150);
            m_area->setPlainText(initial);
            m_area->setPlaceholderText(placeholder);
            body()->addWidget(m_area);
            confirmOnEnter(m_area);
        } else {
            m_line = titleEdit(card(), true, 24);
            m_line->setText(initial);
            m_line->setPlaceholderText(placeholder);
            body()->addWidget(m_line);
            confirmOnEnter(m_line);
        }
        if (!hint.isEmpty()) {
            auto* h = new QLabel(hint, card());
            h->setObjectName(QStringLiteral("sheetDim"));
            h->setWordWrap(true);
            h->setAlignment(multiline ? Qt::AlignLeft : Qt::AlignCenter);
            h->setFont(uiFont(11.5));
            body()->addWidget(h);
        }
        QPushButton* ok = addFooter(okText.isEmpty() ? QCoreApplication::translate("Sheets", "Salvar") : okText,
                                    multiline ? QString() : QCoreApplication::translate("Sheets", "confirma"));
        if (!allowEmpty) {
            auto sync = [this, ok]() { ok->setEnabled(!value().trimmed().isEmpty()); };
            if (m_line) connect(m_line, &QLineEdit::textChanged, this, sync);
            if (m_area) connect(m_area, &QTextEdit::textChanged, this, sync);
            sync();
        }
        applySheetTheme();
    }
    QString value() const { return m_line ? m_line->text() : m_area->toPlainText(); }
protected:
    void showEvent(QShowEvent* e) override
    {
        SheetDialog::showEvent(e);
        if (m_line) { m_line->setFocus(); m_line->selectAll(); }
        else m_area->setFocus();
    }
private:
    QLineEdit* m_line = nullptr;
    QTextEdit* m_area = nullptr;
};

// ── escolha entre opções ─────────────────────────────────────────────────────
class ChoiceSheet : public SheetDialog {
public:
    ChoiceSheet(QWidget* parent, const QString& eyebrow, const QStringList& options, const QString& okText)
        : SheetDialog(parent, 400)
    {
        setEyebrow(eyebrow);
        if (options.size() > SheetChoice::kMaxPills) {
            m_list = new SheetChoice(options, card());
            m_list->setAlwaysOpen();
            m_list->setCurrentIndex(0);
            body()->addWidget(m_list);
            addFooter(okText.isEmpty() ? QCoreApplication::translate("Sheets", "Escolher") : okText, QString());
            applySheetTheme();
            return;
        }
        m_group = new QButtonGroup(this);
        m_group->setExclusive(true);
        QHBoxLayout* row = nullptr;
        int inRow = 0;
        for (int i = 0; i < options.size(); ++i) {
            if (!row || inRow == 3) {
                row = new QHBoxLayout;
                row->setSpacing(6);
                body()->addLayout(row);
                inRow = 0;
            }
            auto* b = pill(options[i], card());
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            b->setFixedHeight(32);
            m_group->addButton(b, i);
            row->addWidget(b);
            ++inRow;
            connect(b, &QToolButton::clicked, this, [this]() { if (m_okBtn) m_okBtn->setEnabled(true); });
        }
        if (row) for (; inRow < 3; ++inRow) row->addStretch(1);
        addFooter(okText.isEmpty() ? QCoreApplication::translate("Sheets", "Escolher") : okText, QString());
        if (auto* first = m_group->button(0)) first->setChecked(true);
        applySheetTheme();
    }
    int chosen() const { return m_list ? m_list->currentIndex() : m_group->checkedId(); }
private:
    QButtonGroup* m_group = nullptr;
    SheetChoice* m_list = nullptr;
};

// ── manuscrito ───────────────────────────────────────────────────────────────
constexpr int kCoverW = 116, kCoverH = 174;

class ManuscriptSheet : public SheetDialog {
public:
    ManuscriptSheet(QWidget* parent, bool editMode, const QString& title, const QString& start,
                    const QString& synopsis, const QString& cover)
        : SheetDialog(parent, 560), m_cover(cover)
    {
        setEyebrow(editMode ? QCoreApplication::translate("Sheets", "Editar manuscrito") : QCoreApplication::translate("Sheets", "Novo manuscrito"));
        auto* row = new QHBoxLayout;
        row->setSpacing(20);
        body()->addLayout(row);

        // capa: clicar escolhe; em branco, usa a do projeto
        auto* coverCol = new QVBoxLayout;
        coverCol->setSpacing(6);
        m_coverBtn = new QToolButton(card());
        m_coverBtn->setObjectName(QStringLiteral("sheetCover"));
        m_coverBtn->setFixedSize(kCoverW, kCoverH);
        m_coverBtn->setCursor(Qt::PointingHandCursor);
        m_coverBtn->setFocusPolicy(Qt::NoFocus);
        m_coverBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        m_coverBtn->setToolTip(QCoreApplication::translate("Sheets", "Escolher capa (opcional; em branco, usa a do projeto)"));
        connect(m_coverBtn, &QToolButton::clicked, this, [this]() { pickCover(); });
        coverCol->addWidget(m_coverBtn);
        m_removeBtn = new QToolButton(card());
        m_removeBtn->setObjectName(QStringLiteral("sheetLink"));
        m_removeBtn->setText(QCoreApplication::translate("Sheets", "remover capa"));
        m_removeBtn->setCursor(Qt::PointingHandCursor);
        m_removeBtn->setFocusPolicy(Qt::NoFocus);
        connect(m_removeBtn, &QToolButton::clicked, this, [this]() { m_cover.clear(); refreshCover(); });
        coverCol->addWidget(m_removeBtn, 0, Qt::AlignHCenter);
        coverCol->addStretch(1);
        row->addLayout(coverCol);

        auto* right = new QVBoxLayout;
        right->setSpacing(12);
        row->addLayout(right, 1);
        m_title = titleEdit(card(), false, 24);
        m_title->setText(title);
        m_title->setPlaceholderText(QCoreApplication::translate("Sheets", "Título do manuscrito"));
        confirmOnEnter(m_title);
        right->addWidget(m_title);

        auto* g1 = new QVBoxLayout;
        g1->setSpacing(6);
        g1->addWidget(sectionLabel(QCoreApplication::translate("Sheets", "Quando a história começa (opcional)"), card()));
        m_start = field(card(), true);
        m_start->setText(start);
        m_start->setPlaceholderText(QCoreApplication::translate("Sheets", "ex.: Dia 1, 15/05/2026, Verão de 1999"));
        confirmOnEnter(m_start);
        g1->addWidget(m_start);
        m_reading = new QLabel(card());
        m_reading->setObjectName(QStringLiteral("sheetLei"));
        m_reading->setFont(monoFont(10.5));
        g1->addWidget(m_reading);
        auto* hint = new QLabel(QCoreApplication::translate("Sheets", "É a data-base da Timeline: capítulos antes dela caem no Flashback."), card());
        hint->setObjectName(QStringLiteral("sheetDim"));
        hint->setWordWrap(true);
        hint->setFont(uiFont(11));
        g1->addWidget(hint);
        right->addLayout(g1);

        auto* g2 = new QVBoxLayout;
        g2->setSpacing(6);
        g2->addWidget(sectionLabel(QCoreApplication::translate("Sheets", "Sinopse (opcional)"), card()));
        m_syn = textArea(card(), 96);
        m_syn->setPlainText(synopsis);
        m_syn->setPlaceholderText(QCoreApplication::translate("Sheets", "Em branco, usa a sinopse do projeto."));
        confirmOnEnter(m_syn);
        g2->addWidget(m_syn);
        right->addLayout(g2);

        QPushButton* ok = addFooter(editMode ? QCoreApplication::translate("Sheets", "Salvar") : QCoreApplication::translate("Sheets", "Criar"), editMode ? QCoreApplication::translate("Sheets", "salva") : QCoreApplication::translate("Sheets", "cria"));
        auto sync = [this, ok]() { ok->setEnabled(!m_title->text().trimmed().isEmpty()); };
        connect(m_title, &QLineEdit::textChanged, this, sync);
        connect(m_start, &QLineEdit::textChanged, this, [this]() { refreshReading(); });
        sync();
        const Palette pal = Palette::current();
        applySheetTheme(QStringLiteral(
            "QToolButton#sheetCover { background: %1; border: 1px dashed %2; border-radius: 6px; color: %3;"
            " font-size: 11.5px; padding: 8px; }"
            "QToolButton#sheetCover:hover { border-color: %4; color: %5; }")
            .arg(mix(pal.app, pal.page, 0.55).name(), alpha(pal.ink, 0.35).name(QColor::HexArgb),
                 pal.dim.name(), pal.accent.name(), pal.bright.name()));
        refreshCover();
        refreshReading();
    }
    QString title() const { return m_title->text().trimmed(); }
    QString start() const { return m_start->text().trimmed(); }
    QString synopsis() const { return m_syn->toPlainText().trimmed(); }
    QString cover() const { return m_cover; }

protected:
    void showEvent(QShowEvent* e) override { SheetDialog::showEvent(e); m_title->setFocus(); }

private:
    void pickCover()
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        const QString path = QFileDialog::getOpenFileName(this, QCoreApplication::translate("Sheets", "Escolher capa"), dir,
            QCoreApplication::translate("Sheets", "Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (path.isEmpty()) return;
        const QString url = CoverUtils::loadCoverAsDataUrl(path);
        if (url.isEmpty()) return;
        m_cover = url;
        refreshCover();
    }
    void refreshCover()
    {
        const QPixmap pm = CoverUtils::pixmapFromDataUrl(m_cover);
        if (pm.isNull()) {
            m_coverBtn->setIcon(QIcon());
            m_coverBtn->setText(QCoreApplication::translate("Sheets", "+ capa\n\n(opcional)"));
            m_removeBtn->hide();
            return;
        }
        // capa com cantos arredondados, cortada pra preencher
        const qreal dpr = devicePixelRatioF();
        QPixmap out(QSize(kCoverW, kCoverH) * dpr);
        out.setDevicePixelRatio(dpr);
        out.fill(Qt::transparent);
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath clip; clip.addRoundedRect(QRectF(0, 0, kCoverW, kCoverH), 6, 6);
        p.setClipPath(clip);
        const QPixmap sc = pm.scaled(QSize(kCoverW, kCoverH) * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        p.drawPixmap(QRectF(0, 0, kCoverW, kCoverH), sc,
                     QRectF((sc.width() - kCoverW * dpr) / 2, (sc.height() - kCoverH * dpr) / 2, kCoverW * dpr, kCoverH * dpr));
        p.end();
        m_coverBtn->setText(QString());
        m_coverBtn->setIcon(QIcon(out));
        m_coverBtn->setIconSize(QSize(kCoverW, kCoverH));
        m_removeBtn->show();
    }
    void refreshReading()
    {
        const FillAssist::Reading r = FillAssist::reading(m_start->text(), false, 0.0);
        m_reading->setText(r.text);
        const QString kind = r.kind == FillAssist::Reading::Ok ? QStringLiteral("ok")
                           : r.kind == FillAssist::Reading::Bad ? QStringLiteral("bad") : QString();
        if (m_reading->property("kind").toString() != kind) {
            m_reading->setProperty("kind", kind);
            m_reading->style()->unpolish(m_reading);
            m_reading->style()->polish(m_reading);
        }
    }

    QString m_cover;
    QToolButton* m_coverBtn = nullptr;
    QToolButton* m_removeBtn = nullptr;
    QLineEdit* m_title = nullptr;
    QLineEdit* m_start = nullptr;
    QLabel* m_reading = nullptr;
    QTextEdit* m_syn = nullptr;
};

QLabel* dimLabel(const QString& text, QWidget* parent, Qt::Alignment align = Qt::AlignLeft)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("sheetDim"));
    l->setWordWrap(true);
    l->setAlignment(align);
    l->setFont(uiFont(11.5));
    return l;
}

// ── nome + escolha ───────────────────────────────────────────────────────────
class TextChoiceSheet : public SheetDialog {
public:
    TextChoiceSheet(QWidget* parent, const QString& eyebrow, const QString& placeholder,
                    const QString& initial, const QString& choiceLabel, const QStringList& options,
                    int selected, const QString& okText, const QStringList& hints)
        : SheetDialog(parent, 440), m_hints(hints)
    {
        setEyebrow(eyebrow);
        m_line = titleEdit(card(), true, 24);
        m_line->setText(initial);
        m_line->setPlaceholderText(placeholder);
        confirmOnEnter(m_line);
        body()->addWidget(m_line);
        body()->addSpacing(4);
        body()->addWidget(sectionLabel(choiceLabel, card()));
        m_choice = new SheetChoice(options, card(), Qt::AlignLeft, 440 - 52);
        m_choice->setCurrentIndex(qBound(0, selected, int(options.size()) - 1));
        body()->addWidget(m_choice);
        m_hint = dimLabel(QString(), card());
        body()->addWidget(m_hint);
        connect(m_choice, &SheetChoice::activated, this, [this](int) { refreshHint(); });
        refreshHint();

        QPushButton* ok = addFooter(okText.isEmpty() ? QCoreApplication::translate("Sheets", "Criar") : okText,
                                    QCoreApplication::translate("Sheets", "cria"));
        auto sync = [this, ok]() { ok->setEnabled(!m_line->text().trimmed().isEmpty() && m_choice->currentIndex() >= 0); };
        connect(m_line, &QLineEdit::textChanged, this, sync);
        sync();
        applySheetTheme();
    }
    QString value() const { return m_line->text(); }
    int chosen() const { return m_choice->currentIndex(); }
protected:
    void showEvent(QShowEvent* e) override
    {
        SheetDialog::showEvent(e);
        m_line->setFocus();
        m_line->selectAll();
    }
private:
    void refreshHint()
    {
        const int i = m_choice->currentIndex();
        const QString h = (i >= 0 && i < m_hints.size()) ? m_hints[i] : QString();
        m_hint->setText(h);
        m_hint->setVisible(!h.isEmpty());
    }
    QStringList m_hints;
    QLineEdit* m_line = nullptr;
    SheetChoice* m_choice = nullptr;
    QLabel* m_hint = nullptr;
};

// ── cor (com ou sem nome) ────────────────────────────────────────────────────
class ColorSheet : public SheetDialog {
public:
    ColorSheet(QWidget* parent, const QString& eyebrow, bool withName, const QString& placeholder,
               const QString& label, const QList<QColor>& palette, const QColor& initial,
               const QString& okText)
        : SheetDialog(parent, withName ? 400 : 360), m_palette(palette)
    {
        setEyebrow(eyebrow);
        if (withName) {
            m_line = titleEdit(card(), true, 24);
            m_line->setPlaceholderText(placeholder);
            confirmOnEnter(m_line);
            body()->addWidget(m_line);
            body()->addSpacing(4);
        }
        if (!label.isEmpty()) body()->addWidget(sectionLabel(label, card()));
        const Palette pal = Palette::current();
        m_group = new QButtonGroup(this);
        m_group->setExclusive(true);
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        for (int i = 0; i < palette.size(); ++i) {
            auto* b = new QToolButton(card());
            b->setCheckable(true);
            b->setFixedSize(26, 26);
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            // bolinha: o raio é metade do lado (fora da escala do tema de
            // propósito — com outro raio deixa de ser círculo). A borda fina
            // escura é pra cor clara (o branco da conexão) aparecer em tema claro.
            b->setStyleSheet(QStringLiteral(
                "QToolButton { background: %1; border: 1px solid %2; border-radius: 13px; }"
                "QToolButton:hover { border: 2px solid %3; }"
                "QToolButton:checked { border: 3px solid %4; }")
                .arg(palette[i].name(), alpha(pal.ink, 0.28).name(QColor::HexArgb),
                     alpha(pal.ink, 0.55).name(QColor::HexArgb), pal.bright.name()));
            m_group->addButton(b, i);
            row->addWidget(b);
        }
        row->addStretch(1);
        body()->addLayout(row);
        int sel = 0;
        for (int i = 0; i < palette.size(); ++i)
            if (palette[i] == initial) { sel = i; break; }
        if (auto* b = m_group->button(sel)) b->setChecked(true);

        QPushButton* ok = addFooter(okText.isEmpty() ? QCoreApplication::translate("Sheets", "Criar") : okText,
                                    QCoreApplication::translate("Sheets", "cria"));
        if (m_line) {
            auto sync = [this, ok]() { ok->setEnabled(!m_line->text().trimmed().isEmpty()); };
            connect(m_line, &QLineEdit::textChanged, this, sync);
            sync();
        } else {
            // sem campo, o Enter vai pro próprio diálogo
            ok->setDefault(true);
        }
        applySheetTheme();
    }
    QString value() const { return m_line ? m_line->text() : QString(); }
    QColor color() const
    {
        const int i = m_group->checkedId();
        return (i >= 0 && i < m_palette.size()) ? m_palette[i] : QColor();
    }
protected:
    void showEvent(QShowEvent* e) override
    {
        SheetDialog::showEvent(e);
        if (m_line) m_line->setFocus();
        else if (m_okBtn) m_okBtn->setFocus();
    }
private:
    QList<QColor> m_palette;
    QLineEdit* m_line = nullptr;
    QButtonGroup* m_group = nullptr;
};

// ── lista com busca ──────────────────────────────────────────────────────────
// Nome em claro à esquerda, a gaveta em cinza à direita; foto em bolinha.
class PickDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    bool photos = false;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return QSize(10, photos ? 40 : 32);
    }
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override
    {
        const Palette pal = Palette::current();
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF r = QRectF(opt.rect).adjusted(2, 1, -2, -1);
        const bool sel = opt.state & QStyle::State_Selected;
        const bool hov = opt.state & QStyle::State_MouseOver;
        if (sel || hov) {
            p->setPen(Qt::NoPen);
            p->setBrush(sel ? alpha(pal.accent, 0.18) : alpha(pal.ink, 0.06));
            p->drawRoundedRect(r, 6, 6);
        }
        qreal x = r.left() + 10;
        if (photos) {
            const QRectF av(x, r.center().y() - 13, 26, 26);
            const QPixmap pm = idx.data(Qt::DecorationRole).value<QPixmap>();
            if (!pm.isNull()) {
                QPainterPath clip;
                clip.addEllipse(av);
                const qreal dpr = p->device()->devicePixelRatioF();
                const QPixmap sc = pm.scaled(QSize(26, 26) * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                p->save();
                p->setClipPath(clip);
                p->drawPixmap(av, sc, QRectF((sc.width() - 26 * dpr) / 2, (sc.height() - 26 * dpr) / 2, 26 * dpr, 26 * dpr));
                p->restore();
            } else {
                // sem foto: a inicial numa bolinha, como no pôster
                p->setPen(Qt::NoPen);
                p->setBrush(alpha(pal.ink, 0.12));
                p->drawEllipse(av);
                p->setPen(pal.dim);
                p->setFont(uiFont(11, QFont::DemiBold));
                p->drawText(av, Qt::AlignCenter, idx.data(Qt::DisplayRole).toString().left(1).toUpper());
            }
            x = av.right() + 10;
        }
        const QString sub = idx.data(Qt::UserRole + 1).toString();
        p->setFont(uiFont(11));
        const QFontMetrics fmSub(p->font());
        const int subW = sub.isEmpty() ? 0 : qMin(fmSub.horizontalAdvance(sub), int(r.width() * 0.4));
        if (!sub.isEmpty()) {
            p->setPen(pal.dim);
            p->drawText(QRectF(r.right() - 10 - subW, r.top(), subW, r.height()), Qt::AlignVCenter | Qt::AlignRight,
                        fmSub.elidedText(sub, Qt::ElideRight, subW));
        }
        p->setFont(uiFont(12.5));
        p->setPen(sel ? pal.bright : pal.ink);
        const QRectF tr(x, r.top(), r.right() - 10 - (subW ? subW + 12 : 0) - x, r.height());
        p->drawText(tr, Qt::AlignVCenter | Qt::AlignLeft,
                    QFontMetrics(p->font()).elidedText(idx.data(Qt::DisplayRole).toString(), Qt::ElideRight, int(tr.width())));
        p->restore();
    }
};

class PickSheet : public SheetDialog {
public:
    PickSheet(QWidget* parent, const QString& eyebrow, const QString& searchPlaceholder,
              const QVector<PickItem>& items, const QString& okText, const QString& extraText,
              const QString& emptyText)
        : SheetDialog(parent, 440)
    {
        setEyebrow(eyebrow);
        bool photos = false;
        for (const PickItem& it : items)
            if (!it.photo.isNull()) { photos = true; break; }

        m_search = field(card());
        m_search->setPlaceholderText(searchPlaceholder);
        m_search->installEventFilter(this);
        body()->addWidget(m_search);

        m_list = new QListWidget(card());
        m_list->setObjectName(QStringLiteral("sheetList"));
        m_list->setFrameShape(QFrame::NoFrame);
        m_list->setMouseTracking(true);
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_list->setFocusPolicy(Qt::NoFocus);
        auto* dg = new PickDelegate(m_list);
        dg->photos = photos;
        m_list->setItemDelegate(dg);
        for (int i = 0; i < items.size(); ++i) {
            const QString label = items[i].label.isEmpty()
                ? QCoreApplication::translate("Sheets", "(sem título)") : items[i].label;
            auto* li = new QListWidgetItem(label, m_list);
            li->setData(Qt::UserRole, i);
            li->setData(Qt::UserRole + 1, items[i].sub);
            if (!items[i].photo.isNull()) li->setData(Qt::DecorationRole, items[i].photo);
        }
        const int rowH = photos ? 40 : 32;
        m_list->setFixedHeight(qBound(rowH * 3 + 4, int(items.size()) * rowH + 4, 300));
        body()->addWidget(m_list);

        m_empty = dimLabel(emptyText.isEmpty() ? QCoreApplication::translate("Sheets", "Nada por aqui ainda.") : emptyText,
                           card(), Qt::AlignCenter);
        body()->addWidget(m_empty);
        if (items.isEmpty()) { m_search->hide(); m_list->hide(); }
        else m_empty->hide();

        QPushButton* ok = addFooter(okText.isEmpty() ? QCoreApplication::translate("Sheets", "Escolher") : okText,
                                    extraText.isEmpty() && !items.isEmpty()
                                        ? QCoreApplication::translate("Sheets", "escolhe") : QString());
        if (!extraText.isEmpty()) {
            auto* extra = new QPushButton(extraText, card());
            extra->setObjectName(QStringLiteral("sheetBtn"));
            extra->setCursor(Qt::PointingHandCursor);
            extra->setFixedHeight(30);
            extra->setAutoDefault(false);
            connect(extra, &QPushButton::clicked, this, [this]() { m_extra = true; accept(); });
            footer()->insertWidget(0, extra);
        }
        connect(m_search, &QLineEdit::textChanged, this, [this](const QString& t) { filter(t); });
        connect(m_list, &QListWidget::currentRowChanged, this, [this, ok](int) { ok->setEnabled(current() >= 0); });
        connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { accept(); });
        filter(QString());
        ok->setEnabled(current() >= 0);

        const Palette pal = Palette::current();
        applySheetTheme(QStringLiteral(
            "QListWidget#sheetList { background: transparent; border: none; outline: none; }"
            "QListWidget#sheetList::item { border: none; }"
            "QListWidget#sheetList QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
            "QListWidget#sheetList QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 24px; }"
            "QListWidget#sheetList QScrollBar::add-line, QListWidget#sheetList QScrollBar::sub-line { height: 0; }"
            "QListWidget#sheetList QScrollBar::add-page, QListWidget#sheetList QScrollBar::sub-page { background: transparent; }")
            .arg(alpha(pal.ink, 0.22).name(QColor::HexArgb)));
        // o viewport não herda o transparent do QSS (ver qt-viewport-transparent-bg-bug)
        m_list->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    }
    // índice em items, kPickExtra ou -1
    int result() const { return m_extra ? kPickExtra : current(); }
protected:
    void showEvent(QShowEvent* e) override
    {
        SheetDialog::showEvent(e);
        if (m_search->isVisible()) m_search->setFocus();
    }
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (o == m_search && e->type() == QEvent::KeyPress) {
            auto* k = static_cast<QKeyEvent*>(e);
            if (k->key() == Qt::Key_Down || k->key() == Qt::Key_Up) {
                step(k->key() == Qt::Key_Down ? 1 : -1);
                return true;
            }
            if (k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter) {
                if (current() >= 0) accept();
                return true;
            }
        }
        return SheetDialog::eventFilter(o, e);
    }
private:
    int current() const
    {
        QListWidgetItem* it = m_list->currentItem();
        return (it && !it->isHidden()) ? it->data(Qt::UserRole).toInt() : -1;
    }
    void filter(const QString& q)
    {
        const QString needle = q.trimmed();
        int first = -1;
        for (int r = 0; r < m_list->count(); ++r) {
            QListWidgetItem* it = m_list->item(r);
            const bool show = needle.isEmpty()
                || it->text().contains(needle, Qt::CaseInsensitive)
                || it->data(Qt::UserRole + 1).toString().contains(needle, Qt::CaseInsensitive);
            it->setHidden(!show);
            if (show && first < 0) first = r;
        }
        QListWidgetItem* cur = m_list->currentItem();
        if (!cur || cur->isHidden()) m_list->setCurrentRow(first);
    }
    void step(int dir)
    {
        for (int i = m_list->currentRow() + dir; i >= 0 && i < m_list->count(); i += dir) {
            if (m_list->item(i)->isHidden()) continue;
            m_list->setCurrentRow(i);
            m_list->scrollToItem(m_list->item(i));
            return;
        }
    }
    QLineEdit* m_search = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_empty = nullptr;
    bool m_extra = false;
};

// ── confirmação ──────────────────────────────────────────────────────────────
class ConfirmSheet : public SheetDialog {
public:
    ConfirmSheet(QWidget* parent, const QString& eyebrow, const QString& text,
                 const QString& warning, const QString& okText)
        : SheetDialog(parent, 400)
    {
        setEyebrow(eyebrow);
        auto* t = new QLabel(text, card());
        t->setObjectName(QStringLiteral("sheetBody"));
        t->setWordWrap(true);
        t->setFont(serifFont(14.5, QFont::Normal));
        body()->addWidget(t);
        if (!warning.isEmpty()) {
            auto* w = new QLabel(warning, card());
            w->setObjectName(QStringLiteral("sheetLei"));
            w->setProperty("kind", QStringLiteral("bad"));
            w->setWordWrap(true);
            w->setFont(uiFont(11.5));
            body()->addWidget(w);
        }
        addFooter(okText, QCoreApplication::translate("Sheets", "confirma"));
        m_okBtn->setDefault(true);
        applySheetTheme(QStringLiteral("QLabel#sheetBody { color: %1; background: transparent; }")
                        .arg(Palette::current().ink.name()));
    }
protected:
    void showEvent(QShowEvent* e) override
    {
        SheetDialog::showEvent(e);
        if (m_okBtn) m_okBtn->setFocus();
    }
};

// ── alterações não salvas ────────────────────────────────────────────────────
class SaveSheet : public SheetDialog {
public:
    SaveSheet(QWidget* parent, const QString& text)
        : SheetDialog(parent, 420)
    {
        setEyebrow(QCoreApplication::translate("Sheets", "Alterações não salvas"));
        auto* t = new QLabel(text, card());
        t->setObjectName(QStringLiteral("sheetBody"));
        t->setWordWrap(true);
        t->setFont(serifFont(14.5, QFont::Normal));
        body()->addWidget(t);
        addFooter(QCoreApplication::translate("Sheets", "Salvar"), QString());
        m_okBtn->setDefault(true);
        // Descartar fica à esquerda, longe do Salvar: não é pra sair no reflexo
        auto* discard = new QPushButton(QCoreApplication::translate("Sheets", "Descartar"), card());
        discard->setObjectName(QStringLiteral("sheetBtn"));
        discard->setCursor(Qt::PointingHandCursor);
        discard->setFixedHeight(30);
        discard->setAutoDefault(false);
        QObject::connect(discard, &QPushButton::clicked, this, [this]() { m_discard = true; accept(); });
        footer()->insertWidget(0, discard);
        footer()->insertStretch(1, 1);
        applySheetTheme(QStringLiteral("QLabel#sheetBody { color: %1; background: transparent; }")
                        .arg(Palette::current().ink.name()));
    }
    bool discarded() const { return m_discard; }
protected:
    void showEvent(QShowEvent* e) override
    {
        SheetDialog::showEvent(e);
        if (m_okBtn) m_okBtn->setFocus();
    }
private:
    bool m_discard = false;
};

} // namespace

SaveChoice askSaveChanges(QWidget* parent, const QString& text)
{
    SaveSheet s(parent, text);
    if (s.exec() != QDialog::Accepted) return SaveChoice::Cancel;
    return s.discarded() ? SaveChoice::Discard : SaveChoice::Save;
}

QString askText(QWidget* parent, const QString& eyebrow, const QString& placeholder,
                const QString& initial, bool* ok, const QString& okText,
                bool multiline, bool allowEmpty, const QString& hint)
{
    TextSheet s(parent, eyebrow, placeholder, initial, okText, multiline, allowEmpty, hint);
    const bool accepted = s.exec() == QDialog::Accepted;
    if (ok) *ok = accepted;
    return accepted ? s.value() : QString();
}

int askChoice(QWidget* parent, const QString& eyebrow, const QStringList& options, const QString& okText)
{
    ChoiceSheet s(parent, eyebrow, options, okText);
    return s.exec() == QDialog::Accepted ? s.chosen() : -1;
}

bool askManuscript(QWidget* parent, bool editMode, QString* title, QString* storyStart,
                   QString* synopsis, QString* coverDataUrl)
{
    ManuscriptSheet s(parent, editMode, *title, *storyStart, *synopsis, *coverDataUrl);
    if (s.exec() != QDialog::Accepted) return false;
    *title = s.title();
    *storyStart = s.start();
    *synopsis = s.synopsis();
    *coverDataUrl = s.cover();
    return true;
}

QString askTextWithChoice(QWidget* parent, const QString& eyebrow, const QString& placeholder,
                          const QString& initial, const QString& choiceLabel,
                          const QStringList& options, int* choice, bool* ok,
                          const QString& okText, const QStringList& optionHints)
{
    TextChoiceSheet s(parent, eyebrow, placeholder, initial, choiceLabel, options,
                      choice ? *choice : 0, okText, optionHints);
    const bool accepted = s.exec() == QDialog::Accepted;
    if (ok) *ok = accepted;
    if (accepted && choice) *choice = s.chosen();
    return accepted ? s.value() : QString();
}

bool askColor(QWidget* parent, const QString& eyebrow, const QString& label,
              const QList<QColor>& palette, QColor* color, const QString& okText)
{
    ColorSheet s(parent, eyebrow, false, QString(), label, palette, color ? *color : QColor(), okText);
    if (s.exec() != QDialog::Accepted) return false;
    if (color) *color = s.color();
    return true;
}

QString askTextWithColor(QWidget* parent, const QString& eyebrow, const QString& placeholder,
                         const QList<QColor>& palette, QColor* color, bool* ok, const QString& okText)
{
    ColorSheet s(parent, eyebrow, true, placeholder, QCoreApplication::translate("Sheets", "Cor"),
                 palette, color ? *color : QColor(), okText);
    const bool accepted = s.exec() == QDialog::Accepted;
    if (ok) *ok = accepted;
    if (accepted && color) *color = s.color();
    return accepted ? s.value() : QString();
}

int askPick(QWidget* parent, const QString& eyebrow, const QString& searchPlaceholder,
            const QVector<PickItem>& items, const QString& okText, const QString& extraText,
            const QString& emptyText)
{
    PickSheet s(parent, eyebrow, searchPlaceholder, items, okText, extraText, emptyText);
    return s.exec() == QDialog::Accepted ? s.result() : -1;
}

bool confirm(QWidget* parent, const QString& eyebrow, const QString& text,
             const QString& warning, const QString& okText)
{
    ConfirmSheet s(parent, eyebrow, text, warning, okText);
    return s.exec() == QDialog::Accepted;
}

} // namespace Sheets
