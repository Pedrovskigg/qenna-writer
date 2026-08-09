#include "MiraPersonalityDialog.h"

#include "MiraPersonality.h"
#include "Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QVBoxLayout>

namespace {

QString headingQss() {
    return QStringLiteral("color: %1; font-size: 16px; font-weight: 600;").arg(Theme::textBright());
}

QString labelQss() {
    return QStringLiteral("color: %1; font-size: 11px;").arg(Theme::textMuted());
}

QString hintQss() {
    return QStringLiteral("color: %1; font-size: 11px; font-style: italic;").arg(Theme::textMuted());
}

QString fieldQss() {
    return QStringLiteral(
        "background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 6px 8px;"
    ).arg(Theme::inputBackground(), Theme::textBright(), Theme::subtleBorder());
}

QString chipQss(bool on) {
    if (on) {
        return QStringLiteral(
            "QPushButton { background: %1; color: white; border: 1px solid %1; "
            "border-radius: 999px; padding: 6px 14px; font-size: 11.5px; font-weight: 600; }"
        ).arg(Theme::accentDefault());
    }
    return QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; "
        "border-radius: 999px; padding: 6px 14px; font-size: 11.5px; }"
        "QPushButton:hover { border-color: %4; }"
    ).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::subtleBorder(), Theme::accentDefault());
}

// Mesmo remédio já usado no rail do AIChatPanel: sem isso o viewport interno
// do QScrollArea usa o fundo padrão da plataforma em vez do tema do diálogo.
QString scrollAreaQss() {
    return QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
    );
}

QString closeBtnQss() {
    return QStringLiteral(
        "background: %1; color: %2; border: none; border-radius: 6px; "
        "padding: 8px 16px; font-size: 12px; font-weight: 700;"
    ).arg(Theme::accentDefault(), Theme::textBright());
}

} // namespace

MiraPersonalityDialog::MiraPersonalityDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("miraPersonalityDialog"));
    setWindowTitle(tr("Personalidade da %1").arg(miraAssistantName()));
    setModal(true);
    resize(480, 680);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(10);

    auto* heading = new QLabel(tr("Quem é a %1?").arg(miraAssistantName()), this);
    heading->setStyleSheet(headingQss());
    root->addWidget(heading);

    root->addWidget(new QLabel(tr("Nome da assistente:"), this));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(QStringLiteral("Mira"));
    m_nameEdit->setStyleSheet(fieldQss());
    root->addWidget(m_nameEdit);

    root->addSpacing(4);
    auto* traitsLabel = new QLabel(tr("Traços de personalidade (combine quantos quiser):"), this);
    traitsLabel->setStyleSheet(labelQss());
    root->addWidget(traitsLabel);

    auto* traitsScroll = new QScrollArea(this);
    traitsScroll->setWidgetResizable(true);
    traitsScroll->setFrameShape(QFrame::NoFrame);
    traitsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    traitsScroll->setFixedHeight(220);
    traitsScroll->setStyleSheet(scrollAreaQss());

    auto* traitsContent = new QWidget(traitsScroll);
    auto* grid = new QGridLayout(traitsContent);
    grid->setContentsMargins(2, 2, 2, 2);
    grid->setSpacing(8);
    const QStringList savedTraits = QSettings().value(QStringLiteral("ai/personalityTraits")).toStringList();
    m_selectedTraits = savedTraits;
    int row = 0, col = 0;
    for (const MiraTraitDef& def : miraTraitDefs()) {
        auto* chip = new QPushButton(def.label, traitsContent);
        chip->setCheckable(true);
        chip->setChecked(savedTraits.contains(def.id));
        chip->setCursor(Qt::PointingHandCursor);
        chip->setStyleSheet(chipQss(chip->isChecked()));
        // O fragmento já vem escrito como "Nome: descrição" (ver
        // miraTraitDefs) — tira o "Nome: " do começo pro tooltip não repetir
        // o que o próprio chip já mostra.
        {
            const int colonIdx = def.fragment.indexOf(QStringLiteral(": "));
            chip->setToolTip(colonIdx >= 0 ? def.fragment.mid(colonIdx + 2) : def.fragment);
        }
        connect(chip, &QPushButton::clicked, this, [this, id = def.id, chip]() {
            toggleTrait(id, chip);
        });
        m_traitButtons.insert(def.id, chip);
        grid->addWidget(chip, row, col);
        if (++col >= 2) { col = 0; ++row; }
    }
    traitsScroll->setWidget(traitsContent);
    root->addWidget(traitsScroll);

    root->addSpacing(6);

    auto makeSlider = [this]() {
        auto* s = new QSlider(Qt::Horizontal, this);
        s->setRange(0, 100);
        s->setSingleStep(1);
        s->setPageStep(10);
        return s;
    };

    root->addWidget(new QLabel(tr("Calor emocional:"), this));
    auto* warmthRow = new QHBoxLayout();
    warmthRow->addWidget(new QLabel(tr("Fria"), this));
    m_warmthSlider = makeSlider();
    warmthRow->addWidget(m_warmthSlider, 1);
    warmthRow->addWidget(new QLabel(tr("Calorosa"), this));
    root->addLayout(warmthRow);

    root->addWidget(new QLabel(tr("Dureza da crítica:"), this));
    auto* harshRow = new QHBoxLayout();
    harshRow->addWidget(new QLabel(tr("Suave/protetora"), this));
    m_harshnessSlider = makeSlider();
    harshRow->addWidget(m_harshnessSlider, 1);
    harshRow->addWidget(new QLabel(tr("Direta/seca"), this));
    root->addLayout(harshRow);

    root->addWidget(new QLabel(tr("Descrição livre (opcional):"), this));
    m_freeformEdit = new QPlainTextEdit(this);
    m_freeformEdit->setPlaceholderText(
        tr("Ex.: \"gosta de fazer analogias com cinema\", \"evita emojis\"…"));
    m_freeformEdit->setFixedHeight(64);
    m_freeformEdit->setStyleSheet(fieldQss());
    root->addWidget(m_freeformEdit);

    auto* hint = new QLabel(
        tr("Tudo aqui vale pra QUALQUER projeto — a mesma %1 em todo lugar. "
           "Também dá pra mexer em Configurações → Assistente de IA.")
            .arg(miraAssistantName()), this);
    hint->setStyleSheet(hintQss());
    hint->setWordWrap(true);
    root->addWidget(hint);

    root->addStretch();

    auto* closeBtn = new QPushButton(tr("Fechar"), this);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(closeBtnQss());
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    // Carrega valores salvos e liga a persistência imediata — mesmas chaves
    // de QSettings do SettingsPanel, então as duas telas ficam sincronizadas
    // sem precisar de nenhum mecanismo extra.
    {
        QSettings settings;
        m_nameEdit->setText(settings.value(QStringLiteral("ai/assistantName")).toString());
        m_warmthSlider->setValue(settings.value(QStringLiteral("ai/personalityWarmth"), 50).toInt());
        m_harshnessSlider->setValue(settings.value(QStringLiteral("ai/personalityHarshness"), 50).toInt());
        m_freeformEdit->setPlainText(settings.value(QStringLiteral("ai/personalityFreeform")).toString());
    }
    connect(m_nameEdit, &QLineEdit::editingFinished, this, [this]() {
        const QString name = m_nameEdit->text().trimmed();
        QSettings().setValue(QStringLiteral("ai/assistantName"), name);
        emit nameChanged(name.isEmpty() ? QStringLiteral("Mira") : name);
    });
    connect(m_warmthSlider, &QSlider::valueChanged, this, [](int v) {
        QSettings().setValue(QStringLiteral("ai/personalityWarmth"), v);
    });
    connect(m_harshnessSlider, &QSlider::valueChanged, this, [](int v) {
        QSettings().setValue(QStringLiteral("ai/personalityHarshness"), v);
    });
    connect(m_freeformEdit, &QPlainTextEdit::textChanged, this, [this]() {
        QSettings().setValue(QStringLiteral("ai/personalityFreeform"), m_freeformEdit->toPlainText());
    });

    applyDialogStyle();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &MiraPersonalityDialog::applyDialogStyle);
}

void MiraPersonalityDialog::toggleTrait(const QString& id, QPushButton* chip)
{
    if (chip->isChecked()) {
        if (!m_selectedTraits.contains(id)) m_selectedTraits.append(id);
    } else {
        m_selectedTraits.removeAll(id);
    }
    chip->setStyleSheet(chipQss(chip->isChecked()));
    QSettings().setValue(QStringLiteral("ai/personalityTraits"), m_selectedTraits);
}

void MiraPersonalityDialog::applyDialogStyle()
{
    setStyleSheet(QStringLiteral("#miraPersonalityDialog { background: %1; } QLabel { color: %2; }")
        .arg(Theme::appBackground(), Theme::textPrimary()));
    for (auto it = m_traitButtons.constBegin(); it != m_traitButtons.constEnd(); ++it) {
        it.value()->setStyleSheet(chipQss(it.value()->isChecked()));
    }
}
