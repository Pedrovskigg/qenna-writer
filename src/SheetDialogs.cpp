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
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStandardPaths>
#include <QStyle>
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
    int chosen() const { return m_group->checkedId(); }
private:
    QButtonGroup* m_group = nullptr;
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

} // namespace

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

} // namespace Sheets
