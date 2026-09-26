// Glossário como aba do Pensário, no desenho de Dicionário: a letra grande
// abrindo cada grupo, os verbetes (termo, tipo, outras grafias, definição, a
// primeira vez que aparece no livro e quantas vezes) e o alfabeto na borda
// direita, como a unha dos dicionários de papel. Criar e editar termo usa o
// mesmo popup do "Adicionar ao glossário" do editor.

#include "PensarioPanel.h"

#include "GlossaryAddPopup.h"
#include "GlossaryIndex.h"
#include "GlossaryStore.h"
#include "ManuscriptViews2.h"
#include "PanelMotion.h"
#include "Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QStyle>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>

namespace {
QString letterOf(const QString& term) {
    const QString t = term.trimmed();
    if (t.isEmpty()) return QStringLiteral("#");
    const QChar base = t.normalized(QString::NormalizationForm_D).at(0).toUpper();
    return (base >= QLatin1Char('A') && base <= QLatin1Char('Z')) ? QString(base) : QStringLiteral("#");
}

// O alfabeto na borda: as letras com termo acendem e levam até eles.
class GlsAlpha : public QWidget {
public:
    std::function<void(const QString&)> onPick;
    explicit GlsAlpha(QWidget* parent) : QWidget(parent) { setFixedWidth(22); setMouseTracking(true); }
    void setPresent(const QSet<QString>& letters) { m_present = letters; update(); }
protected:
    QString letterAt(int y) const {
        const int n = kLetters.size();
        const qreal step = (height() - 16) / qreal(n);
        const int i = int((y - 8) / step);
        return (i >= 0 && i < n) ? kLetters.at(i) : QString();
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), Theme::toColor(Theme::appBackground()));
        p.setPen(QPen(Theme::toColor(Theme::subtleBorder()), 1));
        p.drawLine(0, 0, 0, height());
        const int n = kLetters.size();
        const qreal step = (height() - 16) / qreal(n);
        QFont f(QStringLiteral("Segoe UI"));
        f.setPixelSize(qBound(8, int(step * 0.62), 11));
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        for (int i = 0; i < n; ++i) {
            const QString L = kLetters.at(i);
            const bool on = m_present.contains(L);
            QColor c = Theme::toColor(on ? (L == m_hover ? Theme::textBright() : Theme::textPrimary()) : Theme::textMuted());
            if (!on) c.setAlphaF(0.3f);
            p.setPen(c);
            p.drawText(QRectF(0, 8 + i * step, width(), step), Qt::AlignCenter, L);
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        const QString L = letterAt(int(e->position().y()));
        const QString h = m_present.contains(L) ? L : QString();
        if (h != m_hover) { m_hover = h; setCursor(h.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor); update(); }
    }
    void leaveEvent(QEvent*) override { m_hover.clear(); update(); }
    void mousePressEvent(QMouseEvent* e) override {
        const QString L = letterAt(int(e->position().y()));
        if (m_present.contains(L) && onPick) onPick(L);
    }
private:
    static inline const QStringList kLetters = QStringLiteral("A B C D E F G H I J K L M N O P Q R S T U V W X Y Z #").split(QLatin1Char(' '));
    QSet<QString> m_present;
    QString m_hover;
};

// Um verbete: o texto todo num QLabel rico, e o quadro recebe clique e menu.
class GlsEntry : public QFrame {
public:
    std::function<void(const QPoint&)> onClick, onMenu;
    explicit GlsEntry(QWidget* parent) : QFrame(parent) {
        setObjectName(QStringLiteral("pnGlsEntry"));
        setAttribute(Qt::WA_StyledBackground, true);
        setCursor(Qt::PointingHandCursor);
    }
protected:
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && onClick)
            onClick(e->globalPosition().toPoint());
    }
    void contextMenuEvent(QContextMenuEvent* e) override { if (onMenu) onMenu(e->globalPos()); }
};
}

void PensarioPanel::setGlossaryIndex(GlossaryIndex* index) { m_glsIndex = index; }

void PensarioPanel::setGlossaryPopup(GlossaryAddPopup* popup) { m_glsPopup = popup; }

void PensarioPanel::glossaryTextChanged() {
    if (m_glsRebuildTimer && m_tab == Tab::Glossary && isVisible()) m_glsRebuildTimer->start();
}

void PensarioPanel::openGlossaryTerm(const QString& entryId) {
    m_glsFocusId = entryId;
    if (m_glsSearch && !m_glsSearch->text().isEmpty()) { QSignalBlocker b(m_glsSearch); m_glsSearch->clear(); }
    if (!isPanelOpen()) openPanel();
    selectTab(Tab::Glossary);
    applyStyleLayout();
}

QWidget* PensarioPanel::buildGlossaryPage() {
    auto* page = new QWidget(m_stack);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto* top = new QWidget(page);
    auto* tl = new QHBoxLayout(top);
    tl->setContentsMargins(12, 8, 12, 6);
    tl->setSpacing(6);
    m_glsSearch = new QLineEdit(top);
    m_glsSearch->setPlaceholderText(tr("Buscar no glossário…"));
    m_glsSearch->setClearButtonEnabled(true);
    connect(m_glsSearch, &QLineEdit::textChanged, this, [this]() { rebuildGlossary(); });
    tl->addWidget(m_glsSearch, 1);
    auto* add = new QToolButton(top);
    add->setObjectName(QStringLiteral("pnGlsAdd"));
    add->setText(QStringLiteral("＋ ") + tr("Novo termo"));
    add->setCursor(Qt::PointingHandCursor);
    connect(add, &QToolButton::clicked, this, [this, add]() {
        if (m_glsPopup) m_glsPopup->presentAt(add->mapToGlobal(QPoint(0, add->height() + 4)), QString());
    });
    tl->addWidget(add);
    v->addWidget(top);

    auto* body = new QWidget(page);
    auto* bl = new QHBoxLayout(body);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(0);
    m_glsScroll = new QScrollArea(body);
    m_glsScroll->setWidgetResizable(true);
    m_glsScroll->setFrameShape(QFrame::NoFrame);
    m_glsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_glsScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    m_glsInner = new QWidget(m_glsScroll);
    m_glsInner->setStyleSheet(QStringLiteral("background: transparent;"));
    m_glsLay = new QVBoxLayout(m_glsInner);
    m_glsLay->setContentsMargins(10, 0, 8, 18);
    m_glsLay->setSpacing(2);
    m_glsLay->addStretch(1);
    m_glsScroll->setWidget(m_glsInner);
    bl->addWidget(m_glsScroll, 1);
    auto* alpha = new GlsAlpha(body);
    alpha->onPick = [this](const QString& L) { scrollGlossaryToLetter(L); };
    m_glsAlpha = alpha;
    bl->addWidget(alpha);
    v->addWidget(body, 1);

    m_glsRebuildTimer = new QTimer(this);
    m_glsRebuildTimer->setSingleShot(true);
    m_glsRebuildTimer->setInterval(500);
    connect(m_glsRebuildTimer, &QTimer::timeout, this, [this]() {
        if (m_tab == Tab::Glossary && isVisible()) {
            const int keep = m_glsScroll ? m_glsScroll->verticalScrollBar()->value() : 0;
            rebuildGlossary();
            if (m_glsScroll) QTimer::singleShot(0, this, [this, keep]() { m_glsScroll->verticalScrollBar()->setValue(keep); });
        }
    });
    return page;
}

void PensarioPanel::scrollGlossaryToLetter(const QString& letter) {
    QWidget* head = m_glsLetterHeads.value(letter);
    if (!head || !m_glsScroll) return;
    m_glsScroll->verticalScrollBar()->setValue(head->y() - 4);
}

void PensarioPanel::rebuildGlossary() {
    if (!m_glsLay) return;
    while (m_glsLay->count() > 0) {
        QLayoutItem* it = m_glsLay->takeAt(0);
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        delete it;
    }
    m_glsLetterHeads.clear();

    const QString gar = MsFonts::garamond();
    const QString bright = Theme::textBright(), ink = Theme::textPrimary(), muted = Theme::textMuted();
    auto emptyNote = [&](const QString& title, const QString& hint) {
        auto* l = new QLabel(m_glsInner);
        l->setWordWrap(true);
        l->setTextFormat(Qt::RichText);
        l->setText(QStringLiteral("<div style=\"font-family:'%1'; font-size:17px; color:%2;\">%3</div>"
                                  "<div style=\"font-size:12px; color:%4; margin-top:6px;\">%5</div>")
            .arg(gar, ink, title.toHtmlEscaped(), muted, hint.toHtmlEscaped()));
        l->setContentsMargins(8, 24, 8, 8);
        m_glsLay->addWidget(l);
    };

    const QVector<GlossaryStore::Entry> all = m_glossary ? m_glossary->entries() : QVector<GlossaryStore::Entry>();
    const QString q = m_glsSearch ? m_glsSearch->text().trimmed() : QString();
    QSet<QString> present;
    QWidget* focusWidget = nullptr;
    int shown = 0;
    QString cur;
    for (const GlossaryStore::Entry& e : all) {
        const QString L = letterOf(e.term);
        present.insert(L);
        if (!q.isEmpty()) {
            bool hit = e.term.contains(q, Qt::CaseInsensitive) || e.definition.contains(q, Qt::CaseInsensitive);
            for (const QString& a : e.aliases) hit = hit || a.contains(q, Qt::CaseInsensitive);
            if (!hit) continue;
        }
        ++shown;
        if (L != cur) {
            cur = L;
            auto* head = new QLabel(L, m_glsInner);
            head->setObjectName(QStringLiteral("pnGlsLetter"));
            QFont hf(gar);
            hf.setPixelSize(34);
            head->setFont(hf);
            head->setContentsMargins(4, 12, 0, 0);
            m_glsLay->addWidget(head);
            m_glsLetterHeads.insert(L, head);
        }
        QString html = QStringLiteral("<span style=\"font-family:'%1'; font-size:17px; font-weight:600; color:%2;\">%3</span>")
            .arg(gar, bright, e.term.toHtmlEscaped());
        if (!e.category.isEmpty())
            html += QStringLiteral("&nbsp;&nbsp;<i style=\"font-family:'%1'; font-size:14px; color:%2;\">%3</i>")
                .arg(gar, muted, GlossaryAddPopup::categoryLabel(e.category).toHtmlEscaped());
        if (!e.aliases.isEmpty())
            html += QStringLiteral("<br><i style=\"font-family:'%1'; font-size:13.5px; color:%2;\">%3</i>")
                .arg(gar, muted, tr("também %1").arg(e.aliases.join(QStringLiteral(", "))).toHtmlEscaped());
        if (!e.definition.trimmed().isEmpty()) {
            QString def = e.definition.trimmed().toHtmlEscaped();
            def.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
            html += QStringLiteral("<div style=\"font-family:'%1'; font-size:15.5px; color:%2; margin-top:2px;\">%3</div>")
                .arg(gar, ink, def);
        } else {
            html += QStringLiteral("<div style=\"font-family:'%1'; font-size:14px; color:%2; margin-top:2px;\"><i>%3</i></div>")
                .arg(gar, muted, tr("sem definição · clique pra escrever").toHtmlEscaped());
        }
        QString meta;
        if (m_glsIndex) {
            const GlossaryIndex::Usage u = m_glsIndex->usage(e.spellings());
            meta = u.count > 0 ? tr("1ª vez: %1 · %n×", "", u.count).arg(u.firstLabel)
                               : tr("ainda não aparece no texto");
        }
        if (!meta.isEmpty())
            html += QStringLiteral("<div style=\"font-size:10.5px; color:%1; margin-top:4px;\">%2</div>").arg(muted, meta.toHtmlEscaped());

        auto* entry = new GlsEntry(m_glsInner);
        auto* el = new QVBoxLayout(entry);
        el->setContentsMargins(8, 7, 8, 8);
        auto* label = new QLabel(html, entry);
        label->setTextFormat(Qt::RichText);
        label->setWordWrap(true);
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        el->addWidget(label);
        const QString id = e.id;
        entry->onClick = [this, id](const QPoint& gp) { if (m_glsPopup) m_glsPopup->presentEdit(gp, id); };
        entry->onMenu = [this, id](const QPoint& gp) { showGlossaryEntryMenu(id, gp); };
        entry->setToolTip(tr("Clique pra editar · botão direito pra mais"));
        if (id == m_glsFocusId) {
            entry->setProperty("focus", true);
            focusWidget = entry;
        }
        m_glsLay->addWidget(entry);
    }
    if (all.isEmpty()) {
        emptyNote(tr("O glossário ainda está vazio."),
                  tr("Selecione uma palavra no texto e use \"Adicionar ao glossário\" no menu que aparece, ou clique em Novo termo."));
    } else if (shown == 0) {
        emptyNote(tr("Nenhum termo com \"%1\".").arg(q), QString());
    }
    m_glsLay->addStretch(1);
    if (auto* alpha = static_cast<GlsAlpha*>(m_glsAlpha)) alpha->setPresent(present);

    if (focusWidget) {
        m_glsFocusId.clear();
        QTimer::singleShot(0, this, [this, focusWidget]() {
            if (m_glsScroll) m_glsScroll->ensureWidgetVisible(focusWidget, 0, 80);
        });
        // O destaque some sozinho depois de um tempo.
        QPointer<QWidget> fw(focusWidget);
        QTimer::singleShot(2200, this, [fw]() {
            if (!fw) return;
            fw->setProperty("focus", false);
            fw->style()->unpolish(fw);
            fw->style()->polish(fw);
        });
    }

    if (auto* add = findChild<QToolButton*>(QStringLiteral("pnGlsAdd"))) {
        add->setStyleSheet(Theme::qss(QStringLiteral(
            "QToolButton { color: %1; background: transparent; border: 1px solid %2; border-radius: @radius-control;"
            " padding: 4px 10px; font-size: 12px; }"
            "QToolButton:hover { background: %3; color: %4; }"))
            .arg(Theme::accentDefault(), Theme::subtleBorder(), Theme::hoverOverlay(), Theme::textBright()));
    }
}

void PensarioPanel::goToGlossaryFirstUse(const QString& entryId) {
    if (!m_glossary || !m_glsIndex) return;
    const GlossaryStore::Entry e = m_glossary->findById(entryId);
    if (e.id.isEmpty()) return;
    const GlossaryIndex::Usage u = m_glsIndex->usage(e.spellings());
    if (u.firstDocKey.isEmpty()) return;
    QString snippet = u.firstSentence;
    if (snippet.startsWith(QChar(0x2026))) snippet.remove(0, 1);
    emit openMarkerRequested(u.firstDocKey, 0, 0, snippet.trimmed());
}

void PensarioPanel::showGlossaryEntryMenu(const QString& entryId, const QPoint& globalPos) {
    if (!m_glossary) return;
    QMenu menu(this);
    menu.setStyleSheet(Theme::qss(QStringLiteral(
        "QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: @radius-panel; padding: 4px; }"
        "QMenu::item { padding: 6px 16px; border-radius: @radius-item; }"
        "QMenu::item:selected { background: %4; color: %5; }"
        "QMenu::item:disabled { color: %6; }"
        "QMenu::separator { height: 1px; background: %3; margin: 4px 6px; }"))
        .arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
             Theme::hoverOverlay(), Theme::textBright(), Theme::textMuted()));
    connect(menu.addAction(tr("Editar…")), &QAction::triggered, this, [this, entryId, globalPos]() {
        if (m_glsPopup) m_glsPopup->presentEdit(globalPos, entryId);
    });
    QAction* go = menu.addAction(tr("Ir até a primeira vez"));
    const GlossaryStore::Entry e = m_glossary->findById(entryId);
    go->setEnabled(m_glsIndex && m_glsIndex->usage(e.spellings()).count > 0);
    connect(go, &QAction::triggered, this, [this, entryId]() { goToGlossaryFirstUse(entryId); });
    menu.addSeparator();
    connect(menu.addAction(tr("Remover termo")), &QAction::triggered, this, [this, entryId]() {
        if (m_glossary) m_glossary->remove(entryId);
    });
    PanelMotion::animateMenu(&menu);
    menu.exec(globalPos);
}
