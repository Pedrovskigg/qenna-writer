#include "GlossaryInText.h"

#include "GlossaryAddPopup.h"
#include "GlossaryIndex.h"
#include "GlossaryStore.h"
#include "ManuscriptViews2.h"
#include "Theme.h"

#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QScreen>
#include <QSettings>
#include <QTextBlock>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr const char* kKey = "editor/glossaryInText";
}

bool GlossaryInText::enabledSetting() { return QSettings().value(QString::fromLatin1(kKey), true).toBool(); }

GlossaryInText::GlossaryInText(QTextEdit* editor, GlossaryStore* store, GlossaryIndex* index,
                               ApplyLayer apply, QObject* parent)
    : QObject(parent), m_editor(editor), m_store(store), m_index(index), m_apply(std::move(apply)) {
    m_enabled = enabledSetting();
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(300);
    connect(m_refreshTimer, &QTimer::timeout, this, &GlossaryInText::refresh);
    m_showTimer = new QTimer(this);
    m_showTimer->setSingleShot(true);
    m_showTimer->setInterval(380);
    connect(m_showTimer, &QTimer::timeout, this, [this]() { showCard(m_pendingId, m_pendingPos); });
    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(260);
    connect(m_hideTimer, &QTimer::timeout, this, [this]() {
        if (m_card && m_card->isVisible() && m_card->geometry().contains(QCursor::pos())) return;
        if (m_card) m_card->hide();
        m_cardId.clear();
    });
    if (m_editor) {
        connect(m_editor, &QTextEdit::textChanged, this, &GlossaryInText::scheduleRefresh);
        m_editor->viewport()->installEventFilter(this);
        m_editor->installEventFilter(this);
    }
    if (m_store) connect(m_store, &GlossaryStore::changed, this, &GlossaryInText::scheduleRefresh);
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, [this]() {
        applyCardTheme();
        scheduleRefresh();
    });
}

void GlossaryInText::setEnabled(bool on) {
    m_enabled = on;
    QSettings().setValue(QString::fromLatin1(kKey), on);
    if (!on && m_card) m_card->hide();
    refresh();
}

void GlossaryInText::scheduleRefresh() { m_refreshTimer->start(); }

void GlossaryInText::refresh() {
    m_hits.clear();
    if (!m_editor || !m_apply) return;
    if (!m_enabled || !m_store || m_store->entries().isEmpty()) { m_apply({}); return; }
    // Uma expressão com todas as grafias; a grafia casada diz de qual termo é.
    QStringList all;
    QHash<QString, QString> owner;   // grafia (minúscula) → id do termo
    for (const auto& e : m_store->entries()) {
        for (const QString& sp : e.spellings()) {
            all << sp;
            owner.insert(sp.toLower().simplified(), e.id);
        }
    }
    const QRegularExpression re = GlossaryIndex::patternFor(all);
    if (!re.isValid() || re.pattern().isEmpty()) { m_apply({}); return; }
    QColor line(Theme::accentDefault());
    line.setAlphaF(0.75f);
    QList<QTextEdit::ExtraSelection> sels;
    QTextDocument* doc = m_editor->document();
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        const QString text = b.text();
        if (text.isEmpty()) continue;
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            const QString id = owner.value(m.captured(0).toLower().simplified());
            if (id.isEmpty()) continue;
            Hit h;
            h.start = b.position() + m.capturedStart();
            h.end = h.start + m.capturedLength();
            h.entryId = id;
            m_hits << h;
            QTextEdit::ExtraSelection es;
            es.cursor = QTextCursor(doc);
            es.cursor.setPosition(h.start);
            es.cursor.setPosition(h.end, QTextCursor::KeepAnchor);
            es.format.setFontUnderline(true);
            es.format.setUnderlineStyle(QTextCharFormat::DotLine);
            es.format.setUnderlineColor(line);
            sels << es;
        }
    }
    m_apply(sels);
}

bool GlossaryInText::eventFilter(QObject* watched, QEvent* event) {
    // A ficha fica aberta enquanto o mouse está nela (pra clicar nos links).
    if (m_card && watched == m_card) {
        if (event->type() == QEvent::Enter) m_hideTimer->stop();
        else if (event->type() == QEvent::Leave) hideCardSoon();
        return false;
    }
    if (!m_enabled) return false;
    if (m_editor && watched == m_editor->viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove:
            hoverAt(static_cast<QMouseEvent*>(event)->position().toPoint());
            break;
        case QEvent::Leave:
            m_showTimer->stop();
            hideCardSoon();
            break;
        case QEvent::MouseButtonPress:
        case QEvent::Wheel:
            m_showTimer->stop();
            if (m_card) m_card->hide();
            m_cardId.clear();
            break;
        default: break;
        }
    } else if (m_editor && watched == m_editor && event->type() == QEvent::KeyPress) {
        m_showTimer->stop();
        if (m_card) m_card->hide();
        m_cardId.clear();
    }
    return false;
}

void GlossaryInText::hoverAt(const QPoint& viewportPos) {
    if (m_hits.isEmpty()) return;
    const QTextCursor c = m_editor->cursorForPosition(viewportPos);
    // cursorForPosition devolve a posição mais perto; confirma que o mouse
    // está mesmo em cima do caractere (e não na margem da linha).
    const QRect r = m_editor->cursorRect(c);
    const int pos = c.position();
    QString id;
    if (std::abs(r.center().y() - viewportPos.y()) <= r.height()) {
        auto it = std::upper_bound(m_hits.begin(), m_hits.end(), pos,
                                   [](int p, const Hit& h) { return p < h.start; });
        if (it != m_hits.begin()) {
            --it;
            if (pos >= it->start && pos <= it->end) id = it->entryId;
        }
    }
    if (id.isEmpty()) {
        m_showTimer->stop();
        if (!m_cardId.isEmpty()) hideCardSoon();
        return;
    }
    m_hideTimer->stop();
    if (id == m_cardId && m_card && m_card->isVisible()) return;
    m_pendingId = id;
    m_pendingPos = m_editor->viewport()->mapToGlobal(QPoint(viewportPos.x(), r.bottom() + 6));
    m_showTimer->start();
}

void GlossaryInText::hideCardSoon() { m_hideTimer->start(); }

void GlossaryInText::ensureCard() {
    if (m_card) return;
    m_card = new QFrame(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    m_card->setObjectName(QStringLiteral("glsCard"));
    m_card->setAttribute(Qt::WA_ShowWithoutActivating);
    m_card->setAttribute(Qt::WA_StyledBackground, true);
    m_card->setProperty("qennaNoRestore", true);   // hover: não volta sozinho quando o app volta
    m_card->setFixedWidth(300);
    m_card->installEventFilter(this);
    auto* lay = new QVBoxLayout(m_card);
    lay->setContentsMargins(13, 10, 13, 10);
    lay->setSpacing(5);
    m_cardTitle = new QLabel(m_card);
    m_cardTitle->setTextFormat(Qt::RichText);
    m_cardTitle->setWordWrap(true);
    lay->addWidget(m_cardTitle);
    m_cardDef = new QLabel(m_card);
    m_cardDef->setWordWrap(true);
    m_cardDef->setObjectName(QStringLiteral("glsCardDef"));
    lay->addWidget(m_cardDef);
    auto* row = new QHBoxLayout();
    row->setSpacing(8);
    m_cardMeta = new QLabel(m_card);
    m_cardMeta->setObjectName(QStringLiteral("glsCardMeta"));
    row->addWidget(m_cardMeta, 1);
    m_cardGo = new QToolButton(m_card);
    m_cardGo->setObjectName(QStringLiteral("glsCardLink"));
    m_cardGo->setText(tr("1ª vez ›"));
    m_cardGo->setToolTip(tr("Ir até a primeira vez que o termo aparece"));
    m_cardGo->setCursor(Qt::PointingHandCursor);
    connect(m_cardGo, &QToolButton::clicked, this, [this]() {
        m_card->hide();
        const QString k = m_cardDocKey, s = m_cardSentence;
        m_cardId.clear();
        if (!k.isEmpty()) emit goToFirstUseRequested(k, s);
    });
    row->addWidget(m_cardGo);
    m_cardOpen = new QToolButton(m_card);
    m_cardOpen->setObjectName(QStringLiteral("glsCardLink"));
    m_cardOpen->setText(tr("Glossário ›"));
    m_cardOpen->setToolTip(tr("Abrir o termo no glossário"));
    m_cardOpen->setCursor(Qt::PointingHandCursor);
    connect(m_cardOpen, &QToolButton::clicked, this, [this]() {
        m_card->hide();
        const QString id = m_cardId;
        m_cardId.clear();
        if (!id.isEmpty()) emit openInGlossaryRequested(id);
    });
    row->addWidget(m_cardOpen);
    lay->addLayout(row);
    applyCardTheme();
}

void GlossaryInText::applyCardTheme() {
    if (!m_card) return;
    m_card->setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#glsCard { background: %1; border: 1px solid %2; border-radius: @radius-panel; }"
        "QLabel { background: transparent; }"
        "QLabel#glsCardDef { color: %3; font-family: '%6'; font-size: 15px; }"
        "QLabel#glsCardMeta { color: %4; font-size: 10.5px; }"
        "QToolButton#glsCardLink { color: %5; background: transparent; border: none; font-size: 11px; padding: 0; }"
        "QToolButton#glsCardLink:hover { color: %3; }"))
        .arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(), Theme::textMuted(),
             Theme::accentDefault(), MsFonts::garamond()));
}

void GlossaryInText::showCard(const QString& entryId, const QPoint& globalPos) {
    if (!m_store || entryId.isEmpty() || !m_enabled) return;
    const GlossaryStore::Entry e = m_store->findById(entryId);
    if (e.id.isEmpty()) return;
    ensureCard();
    m_cardId = e.id;
    QString title = QStringLiteral("<span style=\"font-family:'%1'; font-size:18px; font-weight:600; color:%2;\">%3</span>")
        .arg(MsFonts::garamond(), Theme::textBright(), e.term.toHtmlEscaped());
    if (!e.category.isEmpty())
        title += QStringLiteral("&nbsp;&nbsp;<i style=\"font-family:'%1'; font-size:14px; color:%2;\">%3</i>")
            .arg(MsFonts::garamond(), Theme::textMuted(), GlossaryAddPopup::categoryLabel(e.category).toHtmlEscaped());
    if (!e.aliases.isEmpty())
        title += QStringLiteral("<br><i style=\"font-family:'%1'; font-size:13px; color:%2;\">%3</i>")
            .arg(MsFonts::garamond(), Theme::textMuted(), tr("também %1").arg(e.aliases.join(QStringLiteral(", "))).toHtmlEscaped());
    m_cardTitle->setText(title);
    m_cardDef->setText(e.definition.isEmpty() ? tr("Sem definição ainda.") : e.definition);
    m_cardDef->setStyleSheet(e.definition.isEmpty() ? QStringLiteral("font-style: italic; color: %1;").arg(Theme::textMuted()) : QString());
    m_cardDocKey.clear();
    m_cardSentence.clear();
    if (m_index) {
        const GlossaryIndex::Usage u = m_index->usage(e.spellings());
        if (u.count > 0) {
            m_cardMeta->setText(tr("1ª vez: %1 · %n×", "", u.count).arg(u.firstLabel));
            m_cardDocKey = u.firstDocKey;
            m_cardSentence = u.firstSentence;
        } else {
            m_cardMeta->setText(QString());
        }
    }
    m_cardGo->setVisible(!m_cardDocKey.isEmpty());
    // Altura pela largura fixa, depois do polish: o adjustSize media os
    // rótulos antes da fonte do QSS e cortava a definição quando o termo
    // tinha outras grafias.
    m_card->ensurePolished();
    for (QLabel* l : { m_cardTitle, m_cardDef, m_cardMeta }) l->ensurePolished();
    QLayout* lay = m_card->layout();
    lay->invalidate();
    m_card->setFixedHeight(lay->hasHeightForWidth() ? lay->totalHeightForWidth(m_card->width())
                                                     : lay->totalSizeHint().height());
    QPoint p = globalPos;
    if (const QScreen* s = QGuiApplication::screenAt(p)) {
        const QRect a = s->availableGeometry();
        if (p.x() + m_card->width() > a.right()) p.setX(a.right() - m_card->width() - 4);
        if (p.y() + m_card->height() > a.bottom()) p.setY(p.y() - m_card->height() - 34);
    }
    m_card->move(p);
    m_card->show();
    m_card->raise();
}
