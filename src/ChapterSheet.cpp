#include "ChapterSheet.h"

#include "Theme.h"
#include "TimelineChrono.h"
#include "TimelineFillAssist.h"
#include "TimelineTracksTypes.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Tracks;

namespace {



void repolish(QWidget* w, const char* prop, const QVariant& v)
{
    if (w->property(prop) == v) return;
    w->setProperty(prop, v);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

ChapterSheet::ChapterSheet(ChapterSheetSpec& spec, QWidget* parent)
    : SheetDialog(parent, 420)
    , s(spec)
{
    m_startChrono = TimelineChrono::parse(s.startMarker, &m_startOk);
    build();
    applySheetTheme();
}

QString ChapterSheet::currentType() const
{
    static const char* ids[5] = { "chapter", "prologue", "interlude", "epilogue", "custom" };
    if (m_typeChoice && m_typeChoice->currentIndex() >= 0)
        return QString::fromLatin1(ids[m_typeChoice->currentIndex()]);
    return s.type;
}

void ChapterSheet::build()
{
    const bool chapter = s.kind == ChapterSheetSpec::Chapter;
    QWidget* card = this->card();
    if (chapter) setEyebrow(s.editMode ? tr("Editar capítulo")
                            : s.position > 1 ? tr("Novo capítulo · entra depois do %1").arg(s.position - 1)
                                             : tr("Novo capítulo"));
    else setEyebrow(s.editMode ? tr("Editar cena") : tr("Nova cena"));
    QVBoxLayout* body = this->body();

    // ── tipo (só capítulo) ──
    if (chapter) {
        const QStringList names = { tr("Capítulo"), tr("Prólogo"), tr("Interlúdio"), tr("Epílogo"), tr("Outro…") };
        static const char* ids[5] = { "chapter", "prologue", "interlude", "epilogue", "custom" };
        m_typeChoice = new SheetChoice(names, card, Qt::AlignHCenter);
        int cur = 0;
        for (int i = 0; i < 5; ++i)
            if (s.type == QLatin1String(ids[i])) cur = i;
        m_typeChoice->setCurrentIndex(cur);
        body->addWidget(m_typeChoice);
        m_custom = new QLineEdit(s.typeLabel, card);
        m_custom->setObjectName(QStringLiteral("sheetFld"));
        m_custom->setFont(uiFont(12.5));
        m_custom->setFixedHeight(32);
        m_custom->setPlaceholderText(tr("Rótulo: Nota do Autor, Apêndice…"));
        m_custom->setVisible(currentType() == QLatin1String("custom"));
        m_custom->installEventFilter(this);
        body->addWidget(m_custom);
        connect(m_typeChoice, &SheetChoice::activated, this, [this]() {
            m_custom->setVisible(currentType() == QLatin1String("custom"));
            if (m_custom->isVisible()) m_custom->setFocus(); else m_title->setFocus();
            refreshTitle();
            adjustSize();
        });
        connect(m_custom, &QLineEdit::textChanged, this, &ChapterSheet::refreshTitle);
    }

    // ── título grande ──
    m_title = new QLineEdit(s.title, card);
    m_title->setObjectName(QStringLiteral("sheetTitle"));
    m_title->setAlignment(Qt::AlignCenter);
    m_title->setFont(serifFont(27));
    m_title->setFixedHeight(48);
    m_title->installEventFilter(this);
    body->addWidget(m_title);
    connect(m_title, &QLineEdit::textChanged, this, &ChapterSheet::refreshTitle);
    if (chapter) {
        m_preview = new QLabel(card);
        m_preview->setObjectName(QStringLiteral("sheetDim"));
        m_preview->setAlignment(Qt::AlignCenter);
        m_preview->setTextFormat(Qt::RichText);
        m_preview->setFont(uiFont(11.5));
        body->addWidget(m_preview);
    }
    {
        auto* rule = new QFrame(card);
        rule->setObjectName(QStringLiteral("sheetRule"));
        rule->setFixedHeight(1);
        auto* h = new QHBoxLayout;
        h->setContentsMargins(40, 2, 40, 2);
        h->addWidget(rule);
        body->addLayout(h);
    }

    // ── quando se passa (opcional) ──
    {
        auto* g = new QVBoxLayout;
        g->setSpacing(6);
        g->addWidget(sectionLabel(tr("Quando se passa (opcional)"), card));
        m_marker = new QLineEdit(s.marker, card);
        m_marker->setObjectName(QStringLiteral("sheetFld"));
        m_marker->setFont(monoFont(12.5));
        m_marker->setFixedHeight(32);
        if (!chapter)
            m_marker->setPlaceholderText(s.inheritMarker.isEmpty() ? tr("herda do capítulo")
                                                                   : tr("herda: %1").arg(s.inheritMarker));
        else
            m_marker->setPlaceholderText(tr("ex.: Dia 17, 20 anos antes, Verão de 1999"));
        m_marker->installEventFilter(this);
        g->addWidget(m_marker);
        auto* under = new QHBoxLayout;
        under->setSpacing(6);
        m_reading = new QLabel(card);
        m_reading->setObjectName(QStringLiteral("sheetLei"));
        m_reading->setFont(monoFont(10.5));
        under->addWidget(m_reading, 1);
        if (!s.prevMarker.trimmed().isEmpty()) {
            const QString prev = s.prevMarker.trimmed();
            const QString next = FillAssist::shifted(prev, 1, FillAssist::Unit::Day);
            auto quick = [&](const QString& text, const QString& value, const QString& tip) {
                auto* b = new QToolButton(card);
                b->setObjectName(QStringLiteral("sheetQuick"));
                b->setText(text);
                b->setToolTip(tip);
                b->setFont(monoFont(11));
                b->setCursor(Qt::PointingHandCursor);
                b->setFocusPolicy(Qt::NoFocus);
                b->setFixedHeight(22);
                connect(b, &QToolButton::clicked, this, [this, value]() { m_marker->setText(value); m_marker->setFocus(); });
                under->addWidget(b);
                return b;
            };
            m_same = quick(QStringLiteral("= ") + prev, prev,
                           chapter ? tr("Mesmo momento do capítulo anterior") : tr("Mesmo momento da cena anterior"));
            if (!next.isEmpty())
                m_plus = quick(QStringLiteral("+1 · ") + next, next,
                               chapter ? tr("Um dia depois do capítulo anterior") : tr("Um dia depois da cena anterior"));
        }
        g->addLayout(under);
        body->addLayout(g);
        connect(m_marker, &QLineEdit::textChanged, this, &ChapterSheet::refreshReading);
    }

    // ── resumo (opcional) ──
    {
        auto* g = new QVBoxLayout;
        g->setSpacing(6);
        g->addWidget(sectionLabel(tr("Resumo (opcional)"), card));
        m_summary = new QTextEdit(card);
        m_summary->setObjectName(QStringLiteral("sheetTxa"));
        m_summary->setAcceptRichText(false);
        m_summary->setTabChangesFocus(true);
        m_summary->setFont(serifFont(13.5, QFont::Normal));
        m_summary->document()->setDocumentMargin(4);
        m_summary->setFixedHeight(66);
        m_summary->setPlainText(s.summary);
        m_summary->setPlaceholderText(!chapter && !s.inheritSummary.isEmpty()
            ? tr("herda do capítulo: %1").arg(s.inheritSummary)
            : tr("Uma ou duas frases. Aparecem na Timeline no nível Resumos."));
        m_summary->installEventFilter(this);
        g->addWidget(m_summary);
        body->addLayout(g);
    }

    // ── outro POV (só em obra com narrador) ──
    if (s.showPov) {
        m_pov = new QCheckBox(chapter ? tr("Este capítulo é de outro POV (não do narrador)")
                                      : tr("Esta cena é de outro POV (não do narrador)"), card);
        m_pov->setObjectName(QStringLiteral("sheetChk"));
        m_pov->setChecked(s.povOther);
        m_pov->setVisible(s.povOther);
        m_povAdd = new QToolButton(card);
        m_povAdd->setObjectName(QStringLiteral("sheetAdd"));
        m_povAdd->setText(tr("+ outro POV"));
        m_povAdd->setToolTip(tr("Marca que quem conta este trecho não é o narrador (ajuda a Timeline a separar as linhas)"));
        m_povAdd->setCursor(Qt::PointingHandCursor);
        m_povAdd->setFocusPolicy(Qt::NoFocus);
        m_povAdd->setFixedHeight(26);
        m_povAdd->setVisible(!s.povOther);
        connect(m_povAdd, &QToolButton::clicked, this, [this]() {
            m_povAdd->hide();
            m_pov->show();
            m_pov->setChecked(true);
            adjustSize();
        });
        auto* h = new QHBoxLayout;
        h->addStretch(1);
        h->addWidget(m_povAdd);
        h->addStretch(1);
        body->addLayout(h);
        body->addWidget(m_pov);
    }

    // ── não mostrar de novo (popup da cena criada com "----") ──
    if (s.showOptOut) {
        m_optOut = new QCheckBox(tr("Não perguntar isso a cada cena nova"), card);
        m_optOut->setObjectName(QStringLiteral("sheetChk"));
        m_optOut->setChecked(s.optOut);
        m_optOut->setToolTip(tr("O tempo e o resumo da cena passam a ser definidos pelo clique direito nela. "
                                "Dá pra religar em Configurações → Timeline."));
        body->addWidget(m_optOut);
    }

    // ── rodapé ──
    const bool newChapter = chapter && !s.editMode;
    QPushButton* ok = addFooter(s.editMode ? tr("Salvar") : tr("Criar"),
                                s.editMode ? tr("salva") : newChapter ? tr("cria e abre") : tr("cria"));
    disconnect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(ok, &QPushButton::clicked, this, [this]() { finish(true); });
    if (newChapter) ok->setToolTip(tr("Enter cria e abre · Ctrl+Enter cria e continua no capítulo atual"));

    refreshTitle();
    refreshReading();
}

void ChapterSheet::refreshTitle()
{
    const bool chapter = s.kind == ChapterSheetSpec::Chapter;
    QString ph;
    if (chapter && s.previewLabel)
        ph = s.previewLabel(currentType(), m_custom ? m_custom->text().trimmed() : QString());
    if (ph.isEmpty()) ph = chapter ? tr("Título do capítulo") : (s.titlePlaceholder.isEmpty() ? tr("Título da cena") : s.titlePlaceholder);
    m_title->setPlaceholderText(ph);
    if (!m_preview) return;
    const Palette pal = Palette::current();
    const QString shown = m_title->text().trimmed().isEmpty() ? ph : m_title->text().trimmed();
    QString line = tr("na gaveta: %1")
        .arg(QStringLiteral("<span style=\"font-family:'%1';color:%2\">%3</span>")
                 .arg(monoFont(11).family(), pal.ink.name(),
                      (s.position > 0 ? QString::number(s.position) + QStringLiteral(" - ") : QString())
                          + shown.toHtmlEscaped()));
    if (m_title->text().trimmed().isEmpty()) line += QStringLiteral(" · ") + tr("deixe vazio pra usar o padrão");
    m_preview->setText(line);
}

void ChapterSheet::refreshReading()
{
    const QString t = m_marker->text();
    FillAssist::Reading r = FillAssist::reading(t, m_startOk, m_startChrono);
    QString txt = r.text;
    QString kind = r.kind == FillAssist::Reading::Ok ? QStringLiteral("ok")
                 : r.kind == FillAssist::Reading::Bad ? QStringLiteral("bad") : QString();
    if (t.trimmed().isEmpty() && s.kind == ChapterSheetSpec::Scene && !s.inheritMarker.isEmpty()) {
        txt = tr("herda do capítulo");
        kind.clear();
    }
    const QFontMetrics fm(m_reading->font());
    const int w = qMax(60, m_reading->width());
    const QString shown = fm.horizontalAdvance(txt) <= w || r.compact.isEmpty() || kind.isEmpty() ? txt : r.compact;
    m_reading->setText(fm.elidedText(shown, Qt::ElideRight, w));
    m_reading->setToolTip(txt);
    repolish(m_reading, "kind", kind);
}

void ChapterSheet::finish(bool openAfter)
{
    const QString t = m_title->text().trimmed();
    s.title = t;
    s.marker = m_marker->text().trimmed();
    s.summary = m_summary->toPlainText().trimmed();
    if (s.kind == ChapterSheetSpec::Chapter) {
        s.type = currentType();
        s.typeLabel = m_custom ? m_custom->text().trimmed() : QString();
    } else if (t.isEmpty()) {
        s.title = s.titlePlaceholder;   // cena sem título = o nome padrão ("Cena 3")
    }
    if (m_pov) s.povOther = m_pov->isVisible() && m_pov->isChecked();
    if (m_optOut) s.optOut = m_optOut->isChecked();
    s.openAfter = openAfter;
    accept();
}

bool ChapterSheet::eventFilter(QObject* o, QEvent* e)
{
    if (e->type() == QEvent::KeyPress) {
        auto* k = static_cast<QKeyEvent*>(e);
        const bool enter = k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter;
        const bool ctrl = k->modifiers() & Qt::ControlModifier;
        if (enter && (o != m_summary || ctrl)) {
            // Enter cria e abre; Ctrl+Enter cria e fica onde está. No resumo,
            // Enter sozinho é quebra de linha.
            finish(!ctrl);
            return true;
        }
    } else if (e->type() == QEvent::Resize && o == m_reading) {
        refreshReading();
    }
    return SheetDialog::eventFilter(o, e);
}

void ChapterSheet::showEvent(QShowEvent* e)
{
    SheetDialog::showEvent(e);
    m_reading->installEventFilter(this);
    refreshReading();
    m_title->setFocus();
    if (!m_title->text().isEmpty()) m_title->selectAll();
}

bool runChapterSheet(QWidget* parent, ChapterSheetSpec& spec)
{
    ChapterSheet sheet(spec, parent ? parent->window() : nullptr);
    return sheet.exec() == QDialog::Accepted;
}
