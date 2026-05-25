#include "BondViewPanel.h"
#include "IconUtils.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kPanelW = 320;
constexpr int kPanelMinH = 200;
constexpr int kHeaderH = 28;

QString headerIconQss() {
    return QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 2px;
        }
        QToolButton:hover {
            background: %1;
            border-color: rgba(255,255,255,0.18);
        }
    )").arg(Theme::hoverOverlay());
}

QString deleteIconQss() {
    return QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 2px;
        }
        QToolButton:hover {
            background: rgba(224,85,85,0.12);
            border-color: rgba(224,85,85,0.50);
        }
    )");
}

} // namespace

BondViewPanel::BondViewPanel(QWidget* parent,
                             const CharacterBond& bond,
                             const QString& fromTitle,
                             const QString& toTitle)
    : QFrame(parent)
    , m_color(bond.color.isEmpty() ? QStringLiteral("#4a9eff") : bond.color)
{
    setObjectName(QStringLiteral("bondViewPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFrameShape(QFrame::NoFrame);
    setFixedWidth(kPanelW);
    setMinimumHeight(kPanelMinH);
    setStyleSheet(QStringLiteral(
        "QFrame#bondViewPanel { background: %1; border: 1px solid %2; border-radius: 8px; }")
        .arg(Theme::panelBackground(), Theme::panelBorder()));

    buildUi(bond, fromTitle, toTitle);
}

void BondViewPanel::buildUi(const CharacterBond& bond,
                            const QString& fromTitle,
                            const QString& toTitle) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 0, 10, 10);
    root->setSpacing(8);

    // Header — drag area + ícones
    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("bondViewPanelHeader"));
    m_header->setFixedHeight(kHeaderH);
    m_header->setCursor(Qt::OpenHandCursor);
    auto* hlay = new QHBoxLayout(m_header);
    hlay->setContentsMargins(2, 2, 2, 2);
    hlay->setSpacing(4);

    // Barrinha de drag visual à esquerda
    // Ícone de drag handle (6 pontos verticais) à esquerda
    auto* dragIconLabel = new QLabel(m_header);
    {
        QIcon dragIcon = IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/drag-handle.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textMuted()), QColor(Theme::textMuted()),
            QSize(14, 14));
        dragIconLabel->setPixmap(dragIcon.pixmap(QSize(14, 14)));
        dragIconLabel->setStyleSheet(QStringLiteral("padding: 0 4px;"));
    }
    hlay->addWidget(dragIconLabel);
    hlay->addStretch();

    auto makeIconBtn = [this](const QString& iconPath, const QString& tip,
                              const QColor& normal, const QColor& hover, bool danger) {
        auto* b = new QToolButton(m_header);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setIconSize(QSize(14, 14));
        b->setIcon(IconUtils::loadToolbarIcon(iconPath, normal, hover, hover, QSize(14, 14)));
        b->setMinimumSize(22, 22);
        b->setStyleSheet(danger ? deleteIconQss() : headerIconQss());
        return b;
    };

    const QColor iconN(Theme::textMuted());
    const QColor iconH(Theme::textBright());
    const QColor dangerN(QStringLiteral("#e05555"));

    auto* editBtn = makeIconBtn(QStringLiteral(":/icons/edit.svg"),
                                tr("Editar vínculo"), iconN, iconH, false);
    connect(editBtn, &QToolButton::clicked, this, [this]() { emit editRequested(); });
    hlay->addWidget(editBtn);

    auto* docBtn = makeIconBtn(QStringLiteral(":/icons/doc-plus.svg"),
                               tr("Criar documento deste vínculo"), iconN, iconH, false);
    connect(docBtn, &QToolButton::clicked, this, [this]() { emit createDocRequested(); });
    hlay->addWidget(docBtn);

    auto* delBtn = makeIconBtn(QStringLiteral(":/icons/trash.svg"),
                               tr("Excluir vínculo"), dangerN, dangerN, true);
    connect(delBtn, &QToolButton::clicked, this, [this]() { emit deleteRequested(); });
    hlay->addWidget(delBtn);

    auto* closeBtn = makeIconBtn(QStringLiteral(":/icons/close.svg"),
                                 tr("Fechar"), iconN, iconH, false);
    connect(closeBtn, &QToolButton::clicked, this, [this]() { emit closeRequested(); });
    hlay->addWidget(closeBtn);

    root->addWidget(m_header);

    // Sentença
    QString typeLower = bond.type.trimmed();
    if (typeLower.isEmpty()) typeLower = tr("vinculado");
    // Preserva acentos; só lowercase do primeiro caractere se já não vier assim.
    if (!typeLower.isEmpty() && typeLower.at(0).isUpper()) {
        typeLower = typeLower.at(0).toLower() + typeLower.mid(1);
    }

    const QString sentence = QStringLiteral(
        "<span style='color:%1; font-size:16px; line-height: 18px;'>●</span> "
        "<span style='font-family:\"Lora\",\"Crimson Text\",serif; font-size:14px; color:%2;'>"
        "<b>%3</b> é <span style='color:%1; font-weight:700;'>%4</span> de <b>%5</b>"
        "</span>")
        .arg(m_color,
             Theme::textBright(),
             fromTitle.toHtmlEscaped(),
             typeLower.toHtmlEscaped(),
             toTitle.toHtmlEscaped());

    auto* sentenceLabel = new QLabel(sentence, this);
    sentenceLabel->setTextFormat(Qt::RichText);
    sentenceLabel->setWordWrap(true);
    sentenceLabel->setStyleSheet(QStringLiteral("padding: 4px 4px;"));
    root->addWidget(sentenceLabel);

    if (!bond.description.isEmpty()) {
        // Separador
        auto* sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet(QStringLiteral("color: %1; background: %1; max-height: 1px;")
                               .arg(Theme::panelBorder()));
        root->addWidget(sep);

        auto* desc = new QLabel(bond.description, this);
        desc->setWordWrap(true);
        desc->setTextInteractionFlags(Qt::TextSelectableByMouse);
        desc->setStyleSheet(QStringLiteral(
            "color: %1; font-family: 'Lora','Crimson Text',serif; font-size: 13px; "
            "padding: 4px; line-height: 18px;").arg(Theme::textPrimary()));

        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setMaximumHeight(160);
        scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
        scroll->setWidget(desc);
        root->addWidget(scroll, /*stretch=*/1);
    } else {
        root->addStretch();
    }
}

void BondViewPanel::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_header && m_header->geometry().contains(e->pos())) {
        m_dragging = true;
        m_dragOffset = e->pos();
        m_header->setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    QFrame::mousePressEvent(e);
}

void BondViewPanel::mouseMoveEvent(QMouseEvent* e) {
    if (m_dragging) {
        const QPoint delta = e->pos() - m_dragOffset;
        move(pos() + delta);
        e->accept();
        return;
    }
    QFrame::mouseMoveEvent(e);
}

void BondViewPanel::mouseReleaseEvent(QMouseEvent* e) {
    if (m_dragging) {
        m_dragging = false;
        if (m_header) m_header->setCursor(Qt::OpenHandCursor);
        e->accept();
        return;
    }
    QFrame::mouseReleaseEvent(e);
}
