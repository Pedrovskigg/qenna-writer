#include "LousaDock.h"

#include "IconUtils.h"
#include "Theme.h"

#include <QCoreApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QToolButton>

namespace {

struct ToolDef { const char* kind; const char* icon; const char* label; const char* tip; bool sepBefore; };
const ToolDef kTools[] = {
    { "postit",    "postit",    QT_TRANSLATE_NOOP("LousaDock", "Post-it"),    QT_TRANSLATE_NOOP("LousaDock", "Post-it (Ctrl+N)"), false },
    { "comment",   "comment",   QT_TRANSLATE_NOOP("LousaDock", "Comentário"), QT_TRANSLATE_NOOP("LousaDock", "Comentário (Alt+C)"), false },
    { "text",      "text",      QT_TRANSLATE_NOOP("LousaDock", "Texto"),      QT_TRANSLATE_NOOP("LousaDock", "Texto livre (Ctrl+T, ou dois cliques no quadro)"), false },
    { "symbol",    "symbol",    QT_TRANSLATE_NOOP("LousaDock", "Símbolo"),    QT_TRANSLATE_NOOP("LousaDock", "Símbolo"), false },
    { "image",     "image",     QT_TRANSLATE_NOOP("LousaDock", "Imagem"),     QT_TRANSLATE_NOOP("LousaDock", "Imagem (Ctrl+G)"), false },
    { "character", "character", QT_TRANSLATE_NOOP("LousaDock", "Personagem"), QT_TRANSLATE_NOOP("LousaDock", "Personagem do projeto (Ctrl+Shift+C)"), true },
    { "doc",       "doc",       QT_TRANSLATE_NOOP("LousaDock", "Documento"),  QT_TRANSLATE_NOOP("LousaDock", "Documento ou capítulo do projeto (Ctrl+H)"), false },
    { "area",      "area",      QT_TRANSLATE_NOOP("LousaDock", "Área"),       QT_TRANSLATE_NOOP("LousaDock", "Desenhar uma área (arraste no quadro)"), true },
    { "connect",   "connect",   QT_TRANSLATE_NOOP("LousaDock", "Ligar"),      QT_TRANSLATE_NOOP("LousaDock", "Ligar dois cards: clique num e depois no outro"), false },
};

QIcon dockIcon(const QString& name)
{
    return IconUtils::loadToolbarIcon(QStringLiteral(":/icons/lousa/%1.svg").arg(name),
                                      QColor(Theme::textPrimary()), QColor(Theme::textBright()),
                                      QColor(Theme::accentDefault()));
}

} // namespace

LousaDock::LousaDock(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("lousaDock"));
    setAttribute(Qt::WA_StyledBackground, true);
    m_lay = new QHBoxLayout(this);
    m_lay->setContentsMargins(6, 6, 6, 6);
    m_lay->setSpacing(2);
    for (const ToolDef& d : kTools) {
        if (d.sepBefore) {
            auto* sep = new QFrame(this);
            sep->setObjectName(QStringLiteral("lousaDockSep"));
            sep->setFixedSize(1, 30);
            m_lay->addSpacing(4);
            m_lay->addWidget(sep, 0, Qt::AlignVCenter);
            m_lay->addSpacing(4);
            m_seps << sep;
        }
        auto* b = new QToolButton(this);
        b->setObjectName(QStringLiteral("lousaDockBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setIconSize(QSize(20, 20));
        b->setText(QCoreApplication::translate("LousaDock", d.label));
        b->setToolTip(QCoreApplication::translate("LousaDock", d.tip));
        const QString kind = QString::fromLatin1(d.kind);
        if (kind == QStringLiteral("area") || kind == QStringLiteral("connect")) b->setCheckable(true);
        connect(b, &QToolButton::clicked, this, [this, kind]() { emit createRequested(kind); });
        m_lay->addWidget(b);
        m_tools.append({b, kind});
    }
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 120));
    setGraphicsEffect(shadow);
    layoutButtons();
    applyTheme();
}

void LousaDock::layoutButtons()
{
    for (auto& [b, kind] : m_tools) {
        if (m_compact) {
            b->setToolButtonStyle(Qt::ToolButtonIconOnly);
            b->setFixedSize(40, 40);
        } else {
            b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            b->setFixedSize(kind == QStringLiteral("character") || kind == QStringLiteral("doc")
                            || kind == QStringLiteral("comment") ? 76 : 64, 54);
        }
    }
    for (QFrame* s : m_seps) s->setFixedHeight(m_compact ? 22 : 30);
    adjustSize();
}

int LousaDock::fullWidth() const
{
    int w = 12 + int(m_seps.size()) * 9;
    for (const auto& [b, kind] : m_tools)
        w += (kind == QStringLiteral("character") || kind == QStringLiteral("doc")
              || kind == QStringLiteral("comment") ? 76 : 64) + 2;
    return w;
}

void LousaDock::setCompact(bool compact)
{
    if (m_compact == compact) return;
    m_compact = compact;
    layoutButtons();
}

void LousaDock::setToolChecked(const QString& kind, bool on)
{
    for (auto& [b, k] : m_tools)
        if (k == kind) { QSignalBlocker bl(b); b->setChecked(on); }
}

void LousaDock::applyTheme()
{
    const QColor a(Theme::accentDefault());
    const QString soft = QStringLiteral("rgba(%1,%2,%3,0.18)").arg(a.red()).arg(a.green()).arg(a.blue());
    setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#lousaDock { background: %1; border: 1px solid %2; border-radius: 16px; }"
        "QToolButton#lousaDockBtn { background: transparent; border: none; border-radius: 11px;"
        "  color: %3; font-size: 11px; padding: 4px 0 0 0; }"
        "QToolButton#lousaDockBtn:hover { background: %4; color: %5; }"
        "QToolButton#lousaDockBtn:checked { background: %6; color: %5; }"
        "QFrame#lousaDockSep { background: %2; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(),
          Theme::hoverStrong(), Theme::textBright(), soft)));
    int i = 0;
    for (const ToolDef& d : kTools) m_tools[i++].first->setIcon(dockIcon(QString::fromLatin1(d.icon)));
}
