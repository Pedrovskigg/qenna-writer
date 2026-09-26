#include "GlossaryAddPopup.h"

#include "GlossaryIndex.h"
#include "GlossaryStore.h"
#include "Theme.h"

#include <QApplication>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QLabel* fieldLabel(const QString& text, QWidget* parent) {
    auto* l = new QLabel(text.toUpper(), parent);
    l->setObjectName(QStringLiteral("glsAddFieldLabel"));
    return l;
}
bool allCaps(const QString& s) {
    bool letter = false;
    for (const QChar c : s) { if (c.isLetter()) { letter = true; if (c.isLower()) return false; } }
    return letter && s.size() >= 2;
}
}

QStringList GlossaryAddPopup::categoryKeys() {
    return { QStringLiteral("acronym"), QStringLiteral("group"), QStringLiteral("place"),
             QStringLiteral("title"), QStringLiteral("object"), QStringLiteral("slang") };
}

QString GlossaryAddPopup::categoryLabel(const QString& key) {
    if (key == QStringLiteral("acronym")) return tr("sigla");
    if (key == QStringLiteral("group"))   return tr("grupo");
    if (key == QStringLiteral("place"))   return tr("lugar");
    if (key == QStringLiteral("title"))   return tr("título");
    if (key == QStringLiteral("object"))  return tr("objeto");
    if (key == QStringLiteral("slang"))   return tr("gíria");
    return key;   // tipo livre de uma versão futura: mostra como veio
}

QStringList GlossaryAddPopup::parseAliases(const QString& text) {
    QStringList out;
    for (const QString& part : text.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts)) {
        const QString t = part.trimmed();
        if (!t.isEmpty() && !out.contains(t, Qt::CaseInsensitive)) out << t;
    }
    return out;
}

GlossaryAddPopup::GlossaryAddPopup(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("glsAddPopup"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    setFocusPolicy(Qt::ClickFocus);

    m_contextTimer = new QTimer(this);
    m_contextTimer->setSingleShot(true);
    m_contextTimer->setInterval(280);
    connect(m_contextTimer, &QTimer::timeout, this, &GlossaryAddPopup::refreshContext);

    buildUi();
    applyTheme();

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 3);
    setGraphicsEffect(shadow);

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &GlossaryAddPopup::applyTheme);

    hide();
    qApp->installEventFilter(this);
}

void GlossaryAddPopup::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 14);
    root->setSpacing(6);

    auto* head = new QHBoxLayout();
    m_header = new QLabel(this);
    m_header->setObjectName(QStringLiteral("glsAddHeader"));
    head->addWidget(m_header, 1);
    m_closeBtn = new QToolButton(this);
    m_closeBtn->setObjectName(QStringLiteral("glsAddClose"));
    m_closeBtn->setText(QStringLiteral("×"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setFixedSize(22, 22);
    connect(m_closeBtn, &QToolButton::clicked, this, [this]() { hide(); emit cancelled(); });
    head->addWidget(m_closeBtn);
    root->addLayout(head);

    root->addWidget(fieldLabel(tr("Termo"), this));
    m_termEdit = new QLineEdit(this);
    m_termEdit->setObjectName(QStringLiteral("glsAddTerm"));
    connect(m_termEdit, &QLineEdit::textEdited, this, [this]() { m_contextTimer->start(); });
    root->addWidget(m_termEdit);

    // A frase da primeira vez, com a conta embaixo.
    auto* ctxRow = new QHBoxLayout();
    ctxRow->setSpacing(6);
    m_context = new QLabel(this);
    m_context->setObjectName(QStringLiteral("glsAddContext"));
    m_context->setWordWrap(true);
    m_context->setTextFormat(Qt::RichText);
    ctxRow->addWidget(m_context, 1);
    root->addLayout(ctxRow);
    m_goBtn = new QToolButton(this);
    m_goBtn->setObjectName(QStringLiteral("glsAddGo"));
    m_goBtn->setText(tr("Ir até a primeira vez ›"));
    m_goBtn->setCursor(Qt::PointingHandCursor);
    connect(m_goBtn, &QToolButton::clicked, this, [this]() {
        if (m_firstDocKey.isEmpty()) return;
        hide();
        emit goToFirstUseRequested(m_firstDocKey, m_firstSentence);
    });
    root->addWidget(m_goBtn, 0, Qt::AlignLeft);

    root->addWidget(fieldLabel(tr("Tipo (opcional)"), this));
    // Os chips quebram em mais uma linha quando o idioma não cabe numa só
    // ("acronym" em inglês virava "ac…ym"). Largura estimada pela fonte do
    // QSS (11px) + padding e borda do chip.
    auto* catRows = new QVBoxLayout();
    catRows->setSpacing(5);
    auto* cats = new QHBoxLayout();
    cats->setSpacing(5);
    catRows->addLayout(cats);
    QFont chipFont = font();
    chipFont.setPixelSize(11);
    const QFontMetrics chipFm(chipFont);
    const int rowWidth = 372 - 14 - 14;
    int used = 0;
    for (const QString& key : categoryKeys()) {
        const int w = chipFm.horizontalAdvance(categoryLabel(key)) + 26;
        if (used > 0 && used + w > rowWidth) {
            cats->addStretch(1);
            cats = new QHBoxLayout();
            cats->setSpacing(5);
            catRows->addLayout(cats);
            used = 0;
        }
        used += w + 5;
        auto* b = new QToolButton(this);
        b->setObjectName(QStringLiteral("glsAddCat"));
        b->setText(categoryLabel(key));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("catKey", key);
        connect(b, &QToolButton::clicked, this, [this, key]() { setCategory(m_category == key ? QString() : key); });
        cats->addWidget(b);
        m_catBtns << b;
    }
    cats->addStretch(1);
    root->addLayout(catRows);

    root->addWidget(fieldLabel(tr("Definição (opcional)"), this));
    m_defEdit = new QTextEdit(this);
    m_defEdit->setObjectName(QStringLiteral("glsAddDef"));
    m_defEdit->setAcceptRichText(false);
    m_defEdit->setFixedHeight(76);
    m_defEdit->setPlaceholderText(tr("O que o leitor precisa saber…"));
    root->addWidget(m_defEdit);

    root->addWidget(fieldLabel(tr("Também escrito como"), this));
    m_aliasEdit = new QLineEdit(this);
    m_aliasEdit->setObjectName(QStringLiteral("glsAddAlias"));
    m_aliasEdit->setPlaceholderText(tr("Outras grafias, separadas por vírgula"));
    connect(m_aliasEdit, &QLineEdit::textEdited, this, [this]() { m_contextTimer->start(); });
    root->addWidget(m_aliasEdit);

    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 6, 0, 0);
    m_removeBtn = new QToolButton(this);
    m_removeBtn->setObjectName(QStringLiteral("glsAddRemove"));
    m_removeBtn->setText(tr("Remover termo"));
    m_removeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_removeBtn, &QToolButton::clicked, this, [this]() {
        const QString id = m_editId;
        hide();
        if (!id.isEmpty()) emit removeRequested(id);
    });
    row->addWidget(m_removeBtn);
    row->addStretch(1);
    m_cancelBtn = new QPushButton(tr("Cancelar"), this);
    m_cancelBtn->setObjectName(QStringLiteral("glsAddCancel"));
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() { hide(); emit cancelled(); });
    row->addWidget(m_cancelBtn);
    m_okBtn = new QPushButton(this);
    m_okBtn->setObjectName(QStringLiteral("glsAddOk"));
    m_okBtn->setDefault(true);
    m_okBtn->setCursor(Qt::PointingHandCursor);
    connect(m_okBtn, &QPushButton::clicked, this, &GlossaryAddPopup::emitConfirm);
    row->addWidget(m_okBtn);
    root->addLayout(row);

    setFixedWidth(372);
}

void GlossaryAddPopup::applyTheme()
{
    setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#glsAddPopup { background: %1; border: 1px solid %2; border-radius: @radius-panel; }"
        "QLabel#glsAddHeader { color: %3; font-size: 13px; font-weight: 600; }"
        "QLabel#glsAddFieldLabel { color: %4; font-size: 9.5px; font-weight: 700; letter-spacing: 1.4px; margin-top: 4px; }"
        "QLabel#glsAddContext { color: %3; font-size: 12px; border-left: 2px solid %10; padding: 2px 0 2px 9px; }"
        "QToolButton#glsAddGo { color: %10; background: transparent; border: none; font-size: 11.5px; padding: 0 0 0 9px; }"
        "QToolButton#glsAddGo:hover { color: %3; }"
        "QToolButton#glsAddClose { color: %4; background: transparent; border: none; font-size: 15px; }"
        "QToolButton#glsAddClose:hover { color: %3; }"
        "QToolButton#glsAddCat { color: %3; background: transparent; border: 1px solid %11; border-radius: 10px; padding: 2px 9px; font-size: 11px; }"
        "QToolButton#glsAddCat:hover { border-color: %2; }"
        "QToolButton#glsAddCat:checked { color: %10; border-color: %10; background: %12; }"
        "QLineEdit, QTextEdit { background: %5; color: %3; border: 1px solid %2; border-radius: @radius-control; padding: 5px 7px; font-size: 12.5px; }"
        "QLineEdit#glsAddTerm { font-size: 14px; font-weight: 600; }"
        "QToolButton#glsAddRemove { color: %13; background: transparent; border: none; font-size: 11.5px; }"
        "QToolButton#glsAddRemove:hover { text-decoration: underline; }"
        "QPushButton { background: transparent; color: %3; border: 1px solid %2; border-radius: @radius-control; padding: 5px 14px; font-size: 12px; }"
        "QPushButton:hover { background: %6; }"
        "QPushButton#glsAddOk { color: %7; border-color: %8; font-weight: 600; }"
        "QPushButton#glsAddOk:hover { background: %9; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(), Theme::textMuted(),
          Theme::inputBackground(), Theme::hoverOverlay(), Theme::accentSuccess(),
          Theme::accentSuccessBorderSoft(), Theme::accentSuccessSoft())
     .arg(Theme::accentDefault(), Theme::subtleBorder(), Theme::hoverOverlay(), Theme::accentDanger())));
}

void GlossaryAddPopup::setCategory(const QString& key)
{
    m_category = key;
    for (auto* b : m_catBtns) b->setChecked(b->property("catKey").toString() == key);
}

void GlossaryAddPopup::refreshContext()
{
    m_firstDocKey.clear();
    m_firstSentence.clear();
    QStringList spellings;
    const QString term = m_termEdit ? m_termEdit->text().trimmed() : QString();
    if (!term.isEmpty()) spellings << term;
    spellings << parseAliases(m_aliasEdit ? m_aliasEdit->text() : QString());
    if (!m_index || term.isEmpty()) {
        m_context->setVisible(false);
        m_goBtn->setVisible(false);
        return;
    }
    const GlossaryIndex::Usage u = m_index->usage(spellings);
    const QString muted = Theme::textMuted();
    if (u.count == 0) {
        m_context->setText(QStringLiteral("<span style=\"color:%1\">%2</span>").arg(muted, tr("Ainda não aparece no texto.").toHtmlEscaped()));
        m_goBtn->setVisible(false);
    } else {
        // Negrito em volta do termo dentro da frase.
        QString sentence;
        const QRegularExpression re = GlossaryIndex::patternFor(spellings);
        int last = 0;
        auto it = re.globalMatch(u.firstSentence);
        while (re.isValid() && it.hasNext()) {
            const auto m = it.next();
            sentence += u.firstSentence.mid(last, m.capturedStart() - last).toHtmlEscaped()
                      + QStringLiteral("<b>") + m.captured(0).toHtmlEscaped() + QStringLiteral("</b>");
            last = m.capturedEnd();
        }
        sentence += u.firstSentence.mid(last).toHtmlEscaped();
        const QString stats = tr("primeira vez · %1 · aparece %n vez(es)", "", u.count).arg(u.firstLabel)
            + (u.chapterIds.size() > 1 ? QStringLiteral(", ") + tr("em %n capítulo(s)", "", u.chapterIds.size()) : QString());
        m_context->setText(QStringLiteral("<i>%1</i><br><span style=\"color:%2; font-size:10.5px;\">%3</span>")
            .arg(sentence, muted, stats.toHtmlEscaped()));
        m_firstDocKey = u.firstDocKey;
        m_firstSentence = u.firstSentence;
        m_goBtn->setVisible(true);
    }
    m_context->setVisible(true);
    adjustSize();
}

void GlossaryAddPopup::place(const QPoint& globalAnchor)
{
    adjustSize();
    QPoint pos = globalAnchor;
    if (const QScreen* screen = QGuiApplication::screenAt(pos)) {
        const QRect avail = screen->availableGeometry();
        if (pos.x() + width() > avail.right()) pos.setX(avail.right() - width() - 4);
        if (pos.y() + height() > avail.bottom()) pos.setY(avail.bottom() - height() - 4);
        if (pos.x() < avail.left()) pos.setX(avail.left() + 4);
        if (pos.y() < avail.top()) pos.setY(avail.top() + 4);
    }
    move(pos);
    show();
    raise();
    activateWindow();
}

void GlossaryAddPopup::presentAt(const QPoint& globalAnchor, const QString& seedTerm)
{
    m_editId.clear();
    m_header->setText(tr("Adicionar ao glossário"));
    m_okBtn->setText(tr("Adicionar"));
    m_removeBtn->setVisible(false);
    const QString seed = seedTerm.trimmed();
    // Termo que já existe: abre direto em edição, em vez de duplicar.
    if (m_store) {
        for (const auto& e : m_store->entries()) {
            for (const QString& sp : e.spellings()) {
                if (sp.compare(seed, Qt::CaseInsensitive) == 0) { presentEdit(globalAnchor, e.id); return; }
            }
        }
    }
    m_termEdit->setText(seed);
    m_defEdit->clear();
    m_aliasEdit->clear();
    setCategory(allCaps(seed) ? QStringLiteral("acronym") : QString());
    refreshContext();
    m_termEdit->setFocus();
    m_termEdit->selectAll();
    place(globalAnchor);
}

void GlossaryAddPopup::presentEdit(const QPoint& globalAnchor, const QString& entryId)
{
    if (!m_store) return;
    const GlossaryStore::Entry e = m_store->findById(entryId);
    if (e.id.isEmpty()) return;
    m_editId = e.id;
    m_header->setText(tr("Editar termo"));
    m_okBtn->setText(tr("Salvar"));
    m_removeBtn->setVisible(true);
    m_termEdit->setText(e.term);
    m_defEdit->setPlainText(e.definition);
    m_aliasEdit->setText(e.aliases.join(QStringLiteral(", ")));
    setCategory(e.category);
    refreshContext();
    m_defEdit->setFocus();
    m_defEdit->moveCursor(QTextCursor::End);
    place(globalAnchor);
}

void GlossaryAddPopup::emitConfirm()
{
    const QString term = m_termEdit ? m_termEdit->text().trimmed() : QString();
    if (term.isEmpty()) {
        if (m_termEdit) m_termEdit->setFocus();
        return;
    }
    const QString def = m_defEdit ? m_defEdit->toPlainText().trimmed() : QString();
    const QStringList aliases = parseAliases(m_aliasEdit ? m_aliasEdit->text() : QString());
    const QString id = m_editId;
    hide();
    if (id.isEmpty()) emit confirmed(term, def, m_category, aliases);
    else emit saved(id, term, def, m_category, aliases);
}

bool GlossaryAddPopup::eventFilter(QObject* /*watched*/, QEvent* event)
{
    if (!isVisible()) return false;
    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) {
            hide();
            emit cancelled();
            return true;
        }
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
            && (ke->modifiers() & Qt::ControlModifier)) {
            emitConfirm();
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        const QPoint gp = me->globalPosition().toPoint();
        if (frameGeometry().contains(gp)) return false;
        QWidget* w = QApplication::widgetAt(gp);
        while (w) {
            if (w == this) return false;
            w = w->parentWidget();
        }
        hide();
        emit cancelled();
    }
    return false;
}
