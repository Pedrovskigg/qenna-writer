#include "LeftBar.h"
#include "IconUtils.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <QColor>
#include <QFrame>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kBarWidth = 60;
constexpr int kBtnSize  = 40;
constexpr int kIconSize = 20;

constexpr auto kIconNormal   = "#8a857a"; // textMuted
constexpr auto kIconHover    = "#d8d3c6"; // textPrimary
constexpr auto kIconSelected = "#f0e8d8"; // textBright

QIcon loadLeftbarIcon(const QString& fileName) {
    return IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/leftbar/%1").arg(fileName),
        QColor(kIconNormal),
        QColor(kIconHover),
        QColor(kIconSelected),
        QSize(kIconSize, kIconSize));
}

QString fixedButtonQss() {
    // Sem fundo no estado normal; borda translúcida no hover; outline em
    // accent + leve tint no estado checked (= painel aberto). Sem mudança
    // de tamanho — todas as bordas têm 1px reservado em transparent.
    return QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 8px;
            color: %1;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 15px;
            font-weight: 600;
        }
        QToolButton:hover {
            background: %2;
            border-color: %3;
            color: %4;
        }
        QToolButton:checked {
            background: rgba(255,255,255,0.05);
            border-color: %5;
            color: %4;
        }
    )").arg(Theme::textMuted(),
            Theme::hoverOverlay(),
            Theme::subtleBorder(),
            Theme::textBright(),
            Theme::accentDefault());
}

QString drawerButtonQss(const QString& accent) {
    // Letra colorida com o accent da gaveta; fundo sempre transparente.
    // Hover e checked usam a própria cor da gaveta como borda.
    return QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 8px;
            color: %1;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 17px;
            font-weight: 700;
        }
        QToolButton:hover {
            background: rgba(255,255,255,0.05);
            border-color: rgba(255,255,255,0.16);
            color: %2;
        }
        QToolButton:checked {
            background: rgba(255,255,255,0.06);
            border-color: %1;
            color: %2;
        }
    )").arg(accent, Theme::textBright());
}

QString newDrawerQss() {
    return QStringLiteral(R"(
        QToolButton {
            background: transparent;
            color: %1;
            border: 1px dashed %2;
            border-radius: 8px;
            font-size: 22px;
            font-weight: 300;
        }
        QToolButton:hover {
            color: %3;
            border-color: %4;
            background: %5;
        }
    )").arg(Theme::textMuted(),
            Theme::panelBorder(),
            Theme::textBright(),
            Theme::textMuted(),
            Theme::pressedOverlay());
}

QFrame* makeGroupSeparator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral(
        "background: %1; border: none; max-height: 1px; margin: 2px 6px;"
    ).arg(Theme::subtleBorder()));
    return line;
}

} // namespace

int LeftBar::barWidth() {
    return kBarWidth;
}

LeftBar::LeftBar(ProjectModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_drawerLayout(nullptr)
    , m_rootLayout(nullptr)
{
    setObjectName(QStringLiteral("leftBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(kBarWidth);
    setStyleSheet(Theme::panelQss(QStringLiteral("leftBar")));

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(8, 10, 8, 10);
    m_rootLayout->setSpacing(4);

    // ---- Grupo 1: Projeto ----
    m_rootLayout->addWidget(makeFixedButton(
        QStringLiteral("projectinfo.svg"),
        QStringLiteral("i"),
        tr("Informações do projeto"),
        Info));

    m_rootLayout->addWidget(makeGroupSeparator(this));

    // ---- Grupo 2: Planejamento ----
    m_rootLayout->addWidget(makeFixedButton(
        QStringLiteral("whiteboard.svg"),
        QStringLiteral("L"),
        tr("Lousa de planejamento"),
        Whiteboard));

    m_rootLayout->addWidget(makeGroupSeparator(this));

    // ---- Grupo 3: Escrita ----
    m_rootLayout->addWidget(makeFixedButton(
        QStringLiteral("manuscriptpanel.svg"),
        QStringLiteral("T"),
        tr("Manuscritos"),
        Manuscripts));
    m_rootLayout->addWidget(makeFixedButton(
        QStringLiteral("consistency.svg"),
        QStringLiteral("C"),
        tr("Consistência narrativa"),
        Consistency));

    m_rootLayout->addWidget(makeGroupSeparator(this));

    // ---- Botão "+" (nova gaveta) — fica logo acima da lista, igual Mira 1
    m_rootLayout->addWidget(makeNewDrawerButton());

    // ---- Gavetas ----
    m_drawerLayout = new QVBoxLayout();
    m_drawerLayout->setContentsMargins(0, 0, 0, 0);
    m_drawerLayout->setSpacing(4);
    m_rootLayout->addLayout(m_drawerLayout);

    m_rootLayout->addStretch();

    if (m_model) {
        connect(m_model, &ProjectModel::drawersChanged, this, &LeftBar::rebuildDrawerButtons);
        connect(m_model, &ProjectModel::loaded, this, &LeftBar::rebuildDrawerButtons);
        rebuildDrawerButtons();
    }
}

void LeftBar::setMirrored(bool mirrored) {
    if (m_mirrored == mirrored) return;
    m_mirrored = mirrored;
    applyMirrorStyle();
}

void LeftBar::applyMirrorStyle() {
    // O lado físico (esquerda/direita) na janela é decidido pelo MainWindow.
    // Aqui o painel tem borda completa (radius); por ora nada muda visualmente.
    setStyleSheet(Theme::panelQss(QStringLiteral("leftBar")));
}

void LeftBar::setActiveFixedAction(FixedAction action) {
    m_activeFixed = static_cast<int>(action);
    m_activeDrawer.clear();
    refreshActiveStates();
}

void LeftBar::setActiveDrawer(const QString& drawerKey) {
    m_activeDrawer = drawerKey;
    m_activeFixed = -1;
    refreshActiveStates();
}

void LeftBar::clearSelection() {
    m_activeFixed = -1;
    m_activeDrawer.clear();
    refreshActiveStates();
}

void LeftBar::refreshActiveStates() {
    for (auto it = m_fixedButtons.constBegin(); it != m_fixedButtons.constEnd(); ++it) {
        if (auto* btn = it.value()) {
            btn->setChecked(it.key() == m_activeFixed);
        }
    }
    for (auto it = m_drawerButtons.constBegin(); it != m_drawerButtons.constEnd(); ++it) {
        if (auto* btn = it.value()) {
            btn->setChecked(it.key() == m_activeDrawer);
        }
    }
}

QToolButton* LeftBar::makeFixedButton(const QString& iconResource,
                                     const QString& placeholderLetter,
                                     const QString& tooltip,
                                     FixedAction action) {
    auto* btn = new QToolButton(this);
    btn->setToolTip(tooltip);
    btn->setFixedSize(kBtnSize, kBtnSize);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(true);
    btn->setAutoRaise(true);
    btn->setStyleSheet(fixedButtonQss());

    if (!iconResource.isEmpty()) {
        QIcon ic = loadLeftbarIcon(iconResource);
        if (!ic.isNull()) {
            btn->setIcon(ic);
            btn->setIconSize(QSize(kIconSize, kIconSize));
            btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        } else {
            btn->setText(placeholderLetter);
            btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        }
    } else {
        btn->setText(placeholderLetter);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    // Os botões são "radio-like" entre si — clicar emite o sinal e o MainWindow
    // decide se abre/fecha. O setChecked é controlado por setActiveFixedAction.
    connect(btn, &QToolButton::clicked, this, [this, action, btn]() {
        // Bloqueia o toggle automático do QToolButton — quem manda no estado é
        // o MainWindow via setActiveFixedAction. Reverte o checked imediato.
        btn->setChecked(m_activeFixed == static_cast<int>(action));
        emit fixedActionTriggered(action);
    });

    m_fixedButtons.insert(static_cast<int>(action), btn);
    return btn;
}

QToolButton* LeftBar::makeDrawerButton(const QString& drawerKey,
                                      const QString& title,
                                      const QString& color,
                                      const QString& iconId) {
    auto* btn = new QToolButton(this);
    btn->setToolTip(title);
    btn->setFixedSize(kBtnSize, kBtnSize);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(true);
    btn->setAutoRaise(true);

    const QString accent = color.isEmpty() ? Theme::accentDefault() : color;

    // Se a gaveta tem ícone escolhido, renderiza-o tintado com a cor da gaveta;
    // senão, cai pra letra inicial (fallback).
    bool iconLoaded = false;
    if (!iconId.isEmpty()) {
        QIcon ic = IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/elements/%1.svg").arg(iconId),
            QColor(accent),
            QColor(accent),
            QColor(Theme::textBright()),
            QSize(kIconSize, kIconSize));
        if (!ic.isNull()) {
            btn->setIcon(ic);
            btn->setIconSize(QSize(kIconSize, kIconSize));
            btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
            iconLoaded = true;
        }
    }
    if (!iconLoaded) {
        const QString letter = title.isEmpty() ? QStringLiteral("?") : title.left(1).toUpper();
        btn->setText(letter);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    btn->setStyleSheet(drawerButtonQss(accent));

    connect(btn, &QToolButton::clicked, this, [this, drawerKey, btn]() {
        btn->setChecked(m_activeDrawer == drawerKey);
        emit drawerSelected(drawerKey);
    });

    m_drawerButtons.insert(drawerKey, btn);
    return btn;
}

QToolButton* LeftBar::makeNewDrawerButton() {
    auto* btn = new QToolButton(this);
    btn->setText(QStringLiteral("+"));
    btn->setToolTip(tr("Nova gaveta"));
    btn->setFixedSize(kBtnSize, kBtnSize);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(newDrawerQss());
    connect(btn, &QToolButton::clicked, this, &LeftBar::newDrawerRequested);
    return btn;
}

void LeftBar::rebuildDrawerButtons() {
    if (!m_drawerLayout) return;
    m_drawerButtons.clear();
    QLayoutItem* child;
    while ((child = m_drawerLayout->takeAt(0)) != nullptr) {
        if (auto* w = child->widget()) w->deleteLater();
        delete child;
    }
    if (!m_model) return;
    for (const auto& d : m_model->drawers()) {
        // Preferência: drawerIcon (escolha do usuário). Fallback:
        // drawerElementIcon (herdado do tipo). Vazio => letra.
        const QString iconId = !d.drawerIcon.isEmpty() ? d.drawerIcon : d.drawerElementIcon;
        m_drawerLayout->addWidget(makeDrawerButton(d.key, d.title, d.color, iconId));
    }
    refreshActiveStates();
}
