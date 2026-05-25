#include "BondPopup.h"
#include "BondTypes.h"
#include "IconUtils.h"
#include "Theme.h"

#include <QColorDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kPopupW = 360;
constexpr int kPopupMinH = 460;
constexpr int kHeaderH = 36;

QString tinyBtnQss() {
    return QStringLiteral(R"(
        QPushButton {
            background: rgba(255,255,255,0.06);
            color: %1;
            border: 1px solid rgba(255,255,255,0.10);
            border-radius: 6px;
            padding: 6px 12px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 12px;
        }
        QPushButton:hover {
            background: rgba(255,255,255,0.12);
            border-color: rgba(255,255,255,0.20);
            color: %2;
        }
        QPushButton:disabled {
            color: rgba(255,255,255,0.30);
            border-color: rgba(255,255,255,0.06);
        }
    )").arg(Theme::textPrimary(), Theme::textBright());
}

QString pillBtnQss(const QString& accent) {
    return QStringLiteral(R"(
        QPushButton {
            background: %1;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 7px 14px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton:hover { background: %2; }
        QPushButton:disabled {
            background: rgba(255,255,255,0.10);
            color: rgba(255,255,255,0.40);
        }
    )").arg(accent, accent);
}

QString labelQss() {
    return QStringLiteral("color: %1; font-size: 11px; font-weight: 700; letter-spacing: 0.6px;")
        .arg(Theme::textMuted());
}

QString typeButtonQss(bool empty) {
    return QStringLiteral(R"(
        QPushButton {
            background: rgba(0,0,0,0.30);
            color: %1;
            border: 1px solid rgba(255,255,255,0.10);
            border-radius: 6px;
            padding: 8px 12px;
            text-align: left;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 13px;
        }
        QPushButton:hover {
            border-color: rgba(255,255,255,0.22);
            color: %2;
        }
    )").arg(empty ? Theme::textMuted() : Theme::textBright(), Theme::textBright());
}

} // namespace

BondPopup::BondPopup(QWidget* parent,
                     Mode mode,
                     const QString& fromTitle,
                     const QString& toTitle,
                     const CharacterBond& initial)
    : QFrame(parent)
    , m_mode(mode)
    , m_fromTitle(fromTitle)
    , m_toTitle(toTitle)
    , m_type(initial.type)
    , m_color(initial.color.isEmpty() ? BondTypes::defaultColor() : initial.color)
{
    setObjectName(QStringLiteral("bondPopup"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFrameShape(QFrame::NoFrame);
    setFixedWidth(kPopupW);
    setMinimumHeight(kPopupMinH);
    setStyleSheet(QStringLiteral(
        "QFrame#bondPopup { background: %1; border: 1px solid %2; border-radius: 8px; }")
        .arg(Theme::panelBackground(), Theme::panelBorder()));

    buildUi();

    if (m_mode == Edit) {
        if (m_description && !initial.description.isEmpty()) {
            m_description->setPlainText(initial.description);
        }
        applyType(initial.type);
        applyColor(m_color);
    } else {
        applyColor(m_color);
    }
}

void BondPopup::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 0, 12, 12);
    root->setSpacing(8);

    // Header — drag handle + título + X
    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("bondPopupHeader"));
    m_header->setFixedHeight(kHeaderH);
    m_header->setCursor(Qt::OpenHandCursor);
    auto* hlay = new QHBoxLayout(m_header);
    hlay->setContentsMargins(2, 0, 2, 0);
    hlay->setSpacing(8);
    m_title = new QLabel(m_mode == Create ? tr("Criar Vínculo") : tr("Vínculo"), m_header);
    m_title->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Lora','Crimson Text',serif; font-size: 14px; font-weight: 700;")
        .arg(Theme::textBright()));
    hlay->addWidget(m_title);
    hlay->addStretch();
    m_closeBtn = new QToolButton(m_header);
    m_closeBtn->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/close.svg"),
        QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
        QSize(14, 14)));
    m_closeBtn->setIconSize(QSize(14, 14));
    m_closeBtn->setToolTip(tr("Fechar"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setMinimumSize(24, 24);
    m_closeBtn->setStyleSheet(QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 2px;
        }
        QToolButton:hover { background: %1; border-color: rgba(255,255,255,0.18); }
    )").arg(Theme::hoverOverlay()));
    connect(m_closeBtn, &QToolButton::clicked, this, [this]() { emit closeRequested(); });
    hlay->addWidget(m_closeBtn);
    root->addWidget(m_header);

    // Nomes "Ana ↔ João"
    m_namesLabel = new QLabel(QStringLiteral("<b>%1</b>  ↔  <b>%2</b>")
                                  .arg(m_fromTitle.toHtmlEscaped(), m_toTitle.toHtmlEscaped()), this);
    m_namesLabel->setTextFormat(Qt::RichText);
    m_namesLabel->setAlignment(Qt::AlignCenter);
    m_namesLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Lora','Crimson Text',serif; font-size: 13px; padding: 2px 0 6px 0;")
        .arg(Theme::textPrimary()));
    root->addWidget(m_namesLabel);

    // Tipo
    auto* typeLabel = new QLabel(tr("TIPO DE VÍNCULO"), this);
    typeLabel->setStyleSheet(labelQss());
    root->addWidget(typeLabel);
    m_typeButton = new QPushButton(this);
    m_typeButton->setCursor(Qt::PointingHandCursor);
    m_typeButton->setMinimumHeight(36);
    m_typeButton->setStyleSheet(typeButtonQss(m_type.isEmpty()));
    m_typeButton->setText(m_type.isEmpty() ? tr("Selecione ou escreva…") : m_type);
    connect(m_typeButton, &QPushButton::clicked, this, &BondPopup::openTypePicker);
    root->addWidget(m_typeButton);

    // Descrição / backstory
    auto* descLabel = new QLabel(tr("DESCRIÇÃO / BACKSTORY"), this);
    descLabel->setStyleSheet(labelQss());
    root->addWidget(descLabel);
    m_description = new QTextEdit(this);
    m_description->setMinimumHeight(110);
    m_description->setPlaceholderText(tr("Descreva o vínculo, a história por trás dele…"));
    m_description->setAcceptRichText(false);
    m_description->setStyleSheet(QStringLiteral(R"(
        QTextEdit {
            background: rgba(0,0,0,0.30);
            color: %1;
            border: 1px solid rgba(255,255,255,0.10);
            border-radius: 6px;
            padding: 8px 10px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 13px;
            selection-background-color: %2;
        }
        QTextEdit:focus { border-color: rgba(255,255,255,0.22); }
    )").arg(Theme::textBright(), Theme::accentDefault()));
    root->addWidget(m_description);

    // Cor
    auto* colorLabel = new QLabel(tr("COR DA LINHA"), this);
    colorLabel->setStyleSheet(labelQss());
    root->addWidget(colorLabel);

    auto* colorRow = new QWidget(this);
    auto* clay = new QHBoxLayout(colorRow);
    clay->setContentsMargins(0, 0, 0, 0);
    clay->setSpacing(8);

    m_colorMain = new QPushButton(colorRow);
    m_colorMain->setCursor(Qt::PointingHandCursor);
    m_colorMain->setFixedSize(34, 34);
    m_colorMain->setToolTip(tr("Escolher cor personalizada"));
    connect(m_colorMain, &QPushButton::clicked, this, &BondPopup::openColorPicker);
    clay->addWidget(m_colorMain);

    auto* swatchHost = new QWidget(colorRow);
    auto* slay = new QHBoxLayout(swatchHost);
    slay->setContentsMargins(0, 0, 0, 0);
    slay->setSpacing(4);
    for (const auto& c : BondTypes::presetColors()) {
        auto* btn = new QPushButton(swatchHost);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedSize(22, 22);
        btn->setProperty("colorCode", c);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; border: 2px solid transparent; border-radius: 11px; }"
            "QPushButton:hover { border-color: rgba(255,255,255,0.40); }").arg(c));
        connect(btn, &QPushButton::clicked, this, [this, c]() { applyColor(c); });
        slay->addWidget(btn);
        m_colorSwatches.append(btn);
    }
    slay->addStretch();
    clay->addWidget(swatchHost, 1);
    root->addWidget(colorRow);

    root->addStretch();

    // Ações
    auto* actionsRow = new QWidget(this);
    auto* alay = new QHBoxLayout(actionsRow);
    alay->setContentsMargins(0, 0, 0, 0);
    alay->setSpacing(8);

    m_deleteBtn = new QPushButton(tr("Excluir vínculo"), actionsRow);
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setStyleSheet(QStringLiteral(R"(
        QPushButton {
            background: transparent;
            color: #e05555;
            border: 1px solid rgba(224,85,85,0.40);
            border-radius: 6px;
            padding: 6px 12px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 12px;
        }
        QPushButton:hover { background: rgba(224,85,85,0.12); border-color: #e05555; }
    )"));
    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() { emit deleteRequested(); });
    m_deleteBtn->setVisible(m_mode == Edit);
    alay->addWidget(m_deleteBtn);
    alay->addStretch();

    m_cancelBtn = new QPushButton(tr("Cancelar"), actionsRow);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setStyleSheet(tinyBtnQss());
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() { emit closeRequested(); });
    alay->addWidget(m_cancelBtn);

    m_confirmBtn = new QPushButton(m_mode == Edit ? tr("Salvar") : tr("Criar"), actionsRow);
    m_confirmBtn->setCursor(Qt::PointingHandCursor);
    m_confirmBtn->setStyleSheet(pillBtnQss(Theme::accentDefault()));
    m_confirmBtn->setEnabled(!m_type.isEmpty());
    connect(m_confirmBtn, &QPushButton::clicked, this, [this]() {
        if (m_type.trimmed().isEmpty()) return;
        emit confirmed(m_type.trimmed(), description(), m_color);
    });
    alay->addWidget(m_confirmBtn);

    root->addWidget(actionsRow);
}

QString BondPopup::description() const {
    return m_description ? m_description->toPlainText().trimmed() : QString();
}

void BondPopup::applyColor(const QString& c) {
    m_color = c;
    if (m_colorMain) {
        m_colorMain->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; border: 2px solid rgba(255,255,255,0.20); border-radius: 6px; }"
            "QPushButton:hover { border-color: rgba(255,255,255,0.40); }").arg(c));
    }
    // Update swatches selection ring
    for (auto* btn : m_colorSwatches) {
        const QString cc = btn->property("colorCode").toString();
        const bool sel = cc.compare(c, Qt::CaseInsensitive) == 0;
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; border: 2px solid %2; border-radius: 11px; }"
            "QPushButton:hover { border-color: rgba(255,255,255,0.40); }")
            .arg(cc, sel ? QStringLiteral("white") : QStringLiteral("transparent")));
    }
}

void BondPopup::applyType(const QString& t) {
    m_type = t;
    if (m_typeButton) {
        m_typeButton->setText(t.isEmpty() ? tr("Selecione ou escreva…") : t);
        m_typeButton->setStyleSheet(typeButtonQss(t.isEmpty()));
    }
    if (m_confirmBtn) m_confirmBtn->setEnabled(!t.trimmed().isEmpty());
}

void BondPopup::openTypePicker() {
    // Gênero persistido entre sessões (igual Mira 1).
    QSettings settings;
    QString gender = settings.value(QStringLiteral("bonds/typeGender"), QStringLiteral("m")).toString();
    if (gender != QLatin1String("f")) gender = QStringLiteral("m");

    // Popup frameless com Qt::Popup: fecha sozinho ao clicar fora.
    auto* popup = new QFrame(this, Qt::Popup);
    popup->setObjectName(QStringLiteral("bondTypePicker"));
    popup->setAttribute(Qt::WA_DeleteOnClose, true);
    popup->setAttribute(Qt::WA_StyledBackground, true);
    popup->setStyleSheet(QStringLiteral(
        "QFrame#bondTypePicker { background: %1; border: 1px solid %2; border-radius: 6px; }")
        .arg(Theme::panelBackground(), Theme::panelBorder()));
    popup->setFixedWidth(m_typeButton ? m_typeButton->width() : 280);
    popup->setMaximumHeight(360);

    auto* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(4);

    // Header: toggle M/F
    auto* genderRow = new QWidget(popup);
    auto* glay = new QHBoxLayout(genderRow);
    glay->setContentsMargins(8, 4, 8, 4);
    glay->setSpacing(6);
    auto* gLabel = new QLabel(tr("Gênero"), genderRow);
    gLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; font-weight: 700; letter-spacing: 0.6px;")
                              .arg(Theme::textMuted()));
    glay->addWidget(gLabel);
    glay->addStretch();
    auto styleGender = [](QPushButton* b, bool active) {
        b->setFixedSize(28, 22);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: %2; border: 1px solid %3; "
            "border-radius: 4px; font-size: 11px; font-weight: 700; }")
            .arg(active ? QStringLiteral("rgba(74,158,255,0.30)") : QStringLiteral("transparent"),
                 active ? QStringLiteral("white") : QStringLiteral("rgba(255,255,255,0.55)"),
                 active ? QStringLiteral("rgba(74,158,255,0.60)") : QStringLiteral("rgba(255,255,255,0.18)")));
    };
    auto* bM = new QPushButton(QStringLiteral("M"), genderRow);
    auto* bF = new QPushButton(QStringLiteral("F"), genderRow);
    styleGender(bM, gender == QLatin1String("m"));
    styleGender(bF, gender == QLatin1String("f"));
    glay->addWidget(bM);
    glay->addWidget(bF);
    lay->addWidget(genderRow);

    // Separador
    auto* sep = new QFrame(popup);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color: %1; background: %1; max-height: 1px;").arg(Theme::panelBorder()));
    lay->addWidget(sep);

    // Lista rolável
    auto* list = new QListWidget(popup);
    list->setFrameShape(QFrame::NoFrame);
    list->setCursor(Qt::PointingHandCursor);
    list->setStyleSheet(QStringLiteral(R"(
        QListWidget {
            background: transparent;
            color: %1;
            border: none;
            outline: 0;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 12px;
        }
        QListWidget::item {
            padding: 5px 10px;
            border-radius: 4px;
        }
        QListWidget::item:hover { background: %2; color: %3; }
        QListWidget::item:selected { background: %4; color: %3; }
        QListWidget::item:disabled { color: %5; font-size: 10px; font-weight: 700; letter-spacing: 0.6px; padding-top: 10px; }
    )").arg(Theme::textPrimary(), Theme::hoverOverlay(), Theme::textBright(),
           Theme::pressedOverlay(), Theme::textMuted()));
    lay->addWidget(list, /*stretch=*/1);

    // Item de ação: "Personalizado…"
    {
        auto* it = new QListWidgetItem(tr("Digite um tipo personalizado…"));
        it->setData(Qt::UserRole, QStringLiteral("__custom__"));
        QFont f = it->font(); f.setItalic(true); it->setFont(f);
        list->addItem(it);
    }
    if (!m_type.isEmpty()) {
        auto* it = new QListWidgetItem(tr("Limpar tipo"));
        it->setData(Qt::UserRole, QStringLiteral("__clear__"));
        QFont f = it->font(); f.setItalic(true); it->setFont(f);
        list->addItem(it);
    }

    // Grupos
    for (const auto& grp : BondTypes::presetGroups()) {
        auto* header = new QListWidgetItem(grp.name.toUpper());
        header->setFlags(Qt::NoItemFlags); // header não selecionável
        list->addItem(header);
        for (const auto& opt : grp.options) {
            const QString label = (gender == QLatin1String("f")) ? opt.feminine : opt.masculine;
            auto* it = new QListWidgetItem(label);
            it->setData(Qt::UserRole, label);
            if (label == m_type) it->setSelected(true);
            list->addItem(it);
        }
    }

    connect(list, &QListWidget::itemClicked, this, [this, popup](QListWidgetItem* it) {
        if (!it) return;
        const QString role = it->data(Qt::UserRole).toString();
        if (role == QLatin1String("__custom__")) {
            popup->close();
            bool ok = false;
            const QString v = QInputDialog::getText(this, tr("Tipo de vínculo"),
                tr("Descreva o vínculo:"), QLineEdit::Normal, m_type, &ok).trimmed();
            if (ok && !v.isEmpty()) applyType(v);
            return;
        }
        if (role == QLatin1String("__clear__")) {
            applyType(QString());
            popup->close();
            return;
        }
        if (!role.isEmpty()) {
            applyType(role);
            popup->close();
        }
    });

    // M/F: persiste e reabre.
    auto switchGender = [this, popup, &settings](const QString& g) {
        settings.setValue(QStringLiteral("bonds/typeGender"), g);
        popup->close();
        openTypePicker();
    };
    connect(bM, &QPushButton::clicked, this, [switchGender]() { switchGender(QStringLiteral("m")); });
    connect(bF, &QPushButton::clicked, this, [switchGender]() { switchGender(QStringLiteral("f")); });

    // Posiciona logo abaixo do typeButton.
    const QPoint pos = m_typeButton->mapToGlobal(QPoint(0, m_typeButton->height() + 2));
    popup->move(pos);
    popup->show();
}

void BondPopup::openColorPicker() {
    QColor initial(m_color);
    QColor c = QColorDialog::getColor(initial, this, tr("Cor do vínculo"));
    if (c.isValid()) applyColor(c.name());
}

void BondPopup::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_header && m_header->geometry().contains(e->pos())) {
        m_dragging = true;
        m_dragOffset = e->pos();
        m_header->setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    QFrame::mousePressEvent(e);
}

void BondPopup::mouseMoveEvent(QMouseEvent* e) {
    if (m_dragging) {
        const QPoint delta = e->pos() - m_dragOffset;
        move(pos() + delta);
        e->accept();
        return;
    }
    QFrame::mouseMoveEvent(e);
}

void BondPopup::mouseReleaseEvent(QMouseEvent* e) {
    if (m_dragging) {
        m_dragging = false;
        if (m_header) m_header->setCursor(Qt::OpenHandCursor);
        e->accept();
        return;
    }
    QFrame::mouseReleaseEvent(e);
}

void BondPopup::paintEvent(QPaintEvent* e) {
    QFrame::paintEvent(e);
}

void BondPopup::rebuildTypeMenuColors() {
    // placeholder pra evitar warning de unused
}
