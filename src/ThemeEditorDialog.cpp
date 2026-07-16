#include "ThemeEditorDialog.h"

#include "ThemePreviewWidget.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QColor parseAnyColor(const QString& s)
{
    if (s.isEmpty()) return QColor();
    QColor c(s);
    if (c.isValid()) return c;
    QString raw = s.trimmed().toLower();
    if (raw.startsWith("rgba(") && raw.endsWith(")")) {
        raw = raw.mid(5, raw.size() - 6);
        const auto parts = raw.split(',');
        if (parts.size() == 4) {
            const int r = parts.at(0).trimmed().toInt();
            const int g = parts.at(1).trimmed().toInt();
            const int b = parts.at(2).trimmed().toInt();
            bool ok = false;
            const double a = parts.at(3).trimmed().toDouble(&ok);
            int alpha = ok ? (a <= 1.0 ? int(a * 255) : int(a)) : 255;
            return QColor(r, g, b, qBound(0, alpha, 255));
        }
    }
    return QColor();
}

QString colorToString(const QColor& c)
{
    if (!c.isValid()) return QString();
    if (c.alpha() == 255) return c.name();
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 3));
}

void paintSwatch(QPushButton* btn, const QString& colorStr)
{
    const QColor c = parseAnyColor(colorStr);
    const QString css = c.isValid()
        ? QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(c.red()).arg(c.green()).arg(c.blue())
            .arg(QString::number(c.alphaF(), 'f', 3))
        : QStringLiteral("#444");
    btn->setText(QString());
    btn->setStyleSheet(QStringLiteral(
        "QPushButton#themeSwatch { background: %1; border: 1px solid rgba(255,255,255,0.30);"
        " border-radius: 4px; min-width: 56px; min-height: 22px; }"
        "QPushButton#themeSwatch:hover { border-color: rgba(255,255,255,0.65); }"
    ).arg(css));
}

} // namespace

ThemeEditorDialog::ThemeEditorDialog(const Theme::MiraTheme& base, QWidget* parent)
    : QDialog(parent)
    , m_theme(base)
    , m_nameEdit(new QLineEdit(this))
    , m_shadowEnabled(new QCheckBox(this))
    , m_shadowColorSwatch(new QPushButton(this))
    , m_shadowRadius(new QSpinBox(this))
    , m_shadowOffset(new QSpinBox(this))
    , m_imagePathLabel(new QLabel(this))
    , m_imagePickBtn(new QPushButton(tr("Escolher…"), this))
    , m_imageClearBtn(new QPushButton(tr("Remover"), this))
    , m_imageModeCombo(new QComboBox(this))
    , m_opacitySlider(new QSlider(Qt::Horizontal, this))
    , m_opacityLabel(new QLabel(this))
    , m_preview(new ThemePreviewWidget(this))
{
    setObjectName(QStringLiteral("themeEditorDialog"));
    setWindowTitle(tr("Editar Tema"));
    setModal(true);
    resize(960, 640);

    buildUi();
    wireSignals();
    applyDialogStyle();
    refreshPreview();
}

void ThemeEditorDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // Linha do nome (full width, acima das colunas)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* lbl = new QLabel(tr("Nome:"), this);
        lbl->setObjectName(QStringLiteral("themeEditorFieldLabel"));
        lbl->setMinimumWidth(70);
        m_nameEdit->setText(m_theme.name);
        m_nameEdit->setObjectName(QStringLiteral("themeNameEdit"));
        row->addWidget(lbl);
        row->addWidget(m_nameEdit, 1);
        root->addLayout(row);
    }

    // Colunas: esquerda (campos) | direita (preview)
    auto* columns = new QHBoxLayout;
    columns->setSpacing(14);

    // ===== ESQUERDA =====
    auto* leftScroll = new QScrollArea(this);
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    auto* leftContent = new QWidget(leftScroll);
    auto* leftLayout = new QVBoxLayout(leftContent);
    leftLayout->setContentsMargins(0, 0, 12, 0);
    leftLayout->setSpacing(14);

    // ---- Grupo: Texto e cores principais ----
    {
        auto* group = new QGroupBox(tr("Texto e cores principais"), leftContent);
        group->setObjectName(QStringLiteral("themeGroup"));
        auto* gl = new QVBoxLayout(group);
        gl->setSpacing(6);
        const QList<QPair<QString, QString>> fields = {
            { QStringLiteral("editorTextColor"),   tr("Cor do texto") },
            { QStringLiteral("editorBackground"),  tr("Fundo da página") },
            { QStringLiteral("textPrimary"),       tr("Texto da UI") },
            { QStringLiteral("textMuted"),         tr("Texto secundário") },
            { QStringLiteral("accentDefault"),     tr("Cor de destaque") },
        };
        for (const auto& f : fields) gl->addWidget(makeColorRow(f.first, f.second));
        leftLayout->addWidget(group);
    }

    // ---- Grupo: Plano de fundo da janela ----
    {
        auto* group = new QGroupBox(tr("Plano de fundo da janela"), leftContent);
        group->setObjectName(QStringLiteral("themeGroup"));
        auto* gl = new QVBoxLayout(group);
        gl->setSpacing(6);
        gl->addWidget(makeColorRow(QStringLiteral("appBackground"),   tr("Cor do app")));
        gl->addWidget(makeColorRow(QStringLiteral("panelBackground"), tr("Cor dos painéis")));
        gl->addWidget(makeColorRow(QStringLiteral("panelBorder"),     tr("Borda dos painéis")));

        // Imagem
        auto* imgRow = new QHBoxLayout;
        imgRow->setSpacing(8);
        auto* imgLbl = new QLabel(tr("Imagem:"), group);
        imgLbl->setObjectName(QStringLiteral("themeEditorFieldLabel"));
        imgLbl->setMinimumWidth(120);
        m_imagePathLabel->setObjectName(QStringLiteral("themeImagePath"));
        m_imagePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        refreshImageLabel();
        imgRow->addWidget(imgLbl);
        imgRow->addWidget(m_imagePathLabel, 1);
        gl->addLayout(imgRow);

        auto* imgBtnRow = new QHBoxLayout;
        imgBtnRow->setSpacing(8);
        imgBtnRow->addSpacing(120);
        imgBtnRow->addWidget(m_imagePickBtn);
        imgBtnRow->addWidget(m_imageClearBtn);
        imgBtnRow->addStretch();
        gl->addLayout(imgBtnRow);

        auto* modeRow = new QHBoxLayout;
        modeRow->setSpacing(8);
        auto* modeLbl = new QLabel(tr("Modo:"), group);
        modeLbl->setObjectName(QStringLiteral("themeEditorFieldLabel"));
        modeLbl->setMinimumWidth(120);
        m_imageModeCombo->addItem(tr("Centralizar"),  Theme::BgCenter);
        m_imageModeCombo->addItem(tr("Repetir"),      Theme::BgTile);
        m_imageModeCombo->addItem(tr("Esticar"),      Theme::BgStretch);
        m_imageModeCombo->addItem(tr("Ajustar"),      Theme::BgFit);
        m_imageModeCombo->addItem(tr("Preencher"),    Theme::BgZoom);
        const int idx = m_imageModeCombo->findData(m_theme.backgroundMode);
        if (idx >= 0) m_imageModeCombo->setCurrentIndex(idx);
        modeRow->addWidget(modeLbl);
        modeRow->addWidget(m_imageModeCombo, 1);
        gl->addLayout(modeRow);

        leftLayout->addWidget(group);
    }

    // ---- Grupo: Página (sombra + opacidade) ----
    {
        auto* group = new QGroupBox(tr("Página de texto"), leftContent);
        group->setObjectName(QStringLiteral("themeGroup"));
        auto* gl = new QVBoxLayout(group);
        gl->setSpacing(8);

        // Opacidade
        auto* opacityRow = new QHBoxLayout;
        opacityRow->setSpacing(8);
        auto* opacityLbl = new QLabel(tr("Opacidade:"), group);
        opacityLbl->setObjectName(QStringLiteral("themeEditorFieldLabel"));
        opacityLbl->setMinimumWidth(120);
        m_opacitySlider->setRange(20, 100);
        m_opacitySlider->setValue(qBound(20, m_theme.editorOpacity, 100));
        m_opacityLabel->setText(QStringLiteral("%1%").arg(m_opacitySlider->value()));
        m_opacityLabel->setMinimumWidth(40);
        opacityRow->addWidget(opacityLbl);
        opacityRow->addWidget(m_opacitySlider, 1);
        opacityRow->addWidget(m_opacityLabel);
        gl->addLayout(opacityRow);

        // Sombra: toggle + cor + raio + offset
        m_shadowEnabled->setText(tr("Sombra projetada"));
        m_shadowEnabled->setChecked(m_theme.pageShadowEnabled);
        gl->addWidget(m_shadowEnabled);

        auto* shadowGrid = new QFormLayout;
        shadowGrid->setHorizontalSpacing(8);
        shadowGrid->setVerticalSpacing(6);
        m_shadowColorSwatch->setObjectName(QStringLiteral("themeSwatch"));
        m_shadowColorSwatch->setCursor(Qt::PointingHandCursor);
        paintSwatch(m_shadowColorSwatch, m_theme.pageShadowColor);
        m_shadowRadius->setRange(0, 100);
        m_shadowRadius->setValue(m_theme.pageShadowRadius);
        m_shadowRadius->setSuffix(QStringLiteral(" px"));
        m_shadowOffset->setRange(-50, 50);
        m_shadowOffset->setValue(m_theme.pageShadowOffset);
        m_shadowOffset->setSuffix(QStringLiteral(" px"));

        auto* colorLbl = new QLabel(tr("Cor:"), group);
        colorLbl->setObjectName(QStringLiteral("themeEditorFieldLabel"));
        auto* radiusLbl = new QLabel(tr("Raio:"), group);
        radiusLbl->setObjectName(QStringLiteral("themeEditorFieldLabel"));
        auto* offsetLbl = new QLabel(tr("Desloc. Y:"), group);
        offsetLbl->setObjectName(QStringLiteral("themeEditorFieldLabel"));
        shadowGrid->addRow(colorLbl, m_shadowColorSwatch);
        shadowGrid->addRow(radiusLbl, m_shadowRadius);
        shadowGrid->addRow(offsetLbl, m_shadowOffset);
        gl->addLayout(shadowGrid);

        leftLayout->addWidget(group);
    }

    leftLayout->addStretch();
    leftScroll->setWidget(leftContent);
    columns->addWidget(leftScroll, 1);

    // ===== DIREITA: PREVIEW =====
    auto* previewCol = new QVBoxLayout;
    previewCol->setSpacing(6);
    auto* previewHeader = new QLabel(tr("Pré-visualização"), this);
    previewHeader->setObjectName(QStringLiteral("themeEditorPreviewHeader"));
    previewCol->addWidget(previewHeader);

    m_preview->setMinimumSize(380, 460);
    m_preview->setObjectName(QStringLiteral("themeEditorPreview"));
    previewCol->addWidget(m_preview, 1);
    columns->addLayout(previewCol, 1);

    root->addLayout(columns, 1);

    // Footer
    auto* footer = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    footer->button(QDialogButtonBox::Save)->setText(tr("Salvar"));
    footer->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(footer, &QDialogButtonBox::accepted, this, [this]() {
        m_theme.name = m_nameEdit->text().trimmed();
        if (m_theme.name.isEmpty()) m_theme.name = tr("Tema sem nome");
        m_theme.pageShadowEnabled = m_shadowEnabled->isChecked();
        m_theme.pageShadowRadius = m_shadowRadius->value();
        m_theme.pageShadowOffset = m_shadowOffset->value();
        m_theme.backgroundMode = m_imageModeCombo->currentData().toInt();
        m_theme.editorOpacity = m_opacitySlider->value();
        accept();
    });
    connect(footer, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(footer);
}

QWidget* ThemeEditorDialog::makeColorRow(const QString& fieldKey, const QString& label)
{
    auto* w = new QWidget(this);
    auto* row = new QHBoxLayout(w);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    auto* lbl = new QLabel(label, w);
    lbl->setObjectName(QStringLiteral("themeEditorFieldLabel"));
    lbl->setMinimumWidth(120);
    row->addWidget(lbl);

    auto* swatch = new QPushButton(w);
    swatch->setObjectName(QStringLiteral("themeSwatch"));
    swatch->setProperty("fieldKey", fieldKey);
    swatch->setCursor(Qt::PointingHandCursor);
    paintSwatch(swatch, colorOf(fieldKey));
    connect(swatch, &QPushButton::clicked, this, &ThemeEditorDialog::onPickColor);

    auto* hex = new QLabel(colorOf(fieldKey), w);
    hex->setObjectName(QStringLiteral("themeColorHex"));
    hex->setProperty("fieldKey", fieldKey);

    row->addWidget(swatch);
    row->addWidget(hex, 1);

    m_colorSwatches.append(swatch);
    return w;
}

QString ThemeEditorDialog::colorOf(const QString& fieldKey) const
{
    if (fieldKey == "appBackground")    return m_theme.appBackground;
    if (fieldKey == "panelBackground")  return m_theme.panelBackground;
    if (fieldKey == "panelBorder")      return m_theme.panelBorder;
    if (fieldKey == "textPrimary")      return m_theme.textPrimary;
    if (fieldKey == "textMuted")        return m_theme.textMuted;
    if (fieldKey == "textBright")       return m_theme.textBright;
    if (fieldKey == "accentDefault")    return m_theme.accentDefault;
    if (fieldKey == "editorBackground") return m_theme.editorBackground;
    if (fieldKey == "editorTextColor")  return m_theme.editorTextColor;
    return QString();
}

void ThemeEditorDialog::setColorIn(const QString& fieldKey, const QString& value)
{
    if (fieldKey == "appBackground")    m_theme.appBackground = value;
    else if (fieldKey == "panelBackground")  m_theme.panelBackground = value;
    else if (fieldKey == "panelBorder")      m_theme.panelBorder = value;
    else if (fieldKey == "textPrimary")      m_theme.textPrimary = value;
    else if (fieldKey == "textMuted")        m_theme.textMuted = value;
    else if (fieldKey == "textBright")       m_theme.textBright = value;
    else if (fieldKey == "accentDefault")    m_theme.accentDefault = value;
    else if (fieldKey == "editorBackground") m_theme.editorBackground = value;
    else if (fieldKey == "editorTextColor")  m_theme.editorTextColor = value;
}

void ThemeEditorDialog::wireSignals()
{
    connect(m_imagePickBtn, &QPushButton::clicked, this, &ThemeEditorDialog::onPickImage);
    connect(m_imageClearBtn, &QPushButton::clicked, this, &ThemeEditorDialog::onClearImage);
    connect(m_shadowColorSwatch, &QPushButton::clicked, this, &ThemeEditorDialog::onShadowColorClicked);

    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int v) {
        m_opacityLabel->setText(QStringLiteral("%1%").arg(v));
        m_theme.editorOpacity = v;
        refreshPreview();
    });
    connect(m_shadowEnabled, &QCheckBox::toggled, this, [this](bool on) {
        m_theme.pageShadowEnabled = on;
        refreshPreview();
    });
    connect(m_shadowRadius, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        m_theme.pageShadowRadius = v;
        refreshPreview();
    });
    connect(m_shadowOffset, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        m_theme.pageShadowOffset = v;
        refreshPreview();
    });
    connect(m_imageModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        m_theme.backgroundMode = m_imageModeCombo->currentData().toInt();
        refreshPreview();
    });
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString& s) {
        m_theme.name = s;
        // nome não afeta preview visual, mas ainda assim atualiza pra debug.
    });
}

void ThemeEditorDialog::onPickColor()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const QString fieldKey = btn->property("fieldKey").toString();
    QColor initial = parseAnyColor(colorOf(fieldKey));
    if (!initial.isValid()) initial = QColor(80, 80, 80);
    QColorDialog::ColorDialogOptions opts;
    if (fieldKey == "panelBorder") opts |= QColorDialog::ShowAlphaChannel;
    QColor chosen = QColorDialog::getColor(initial, this, tr("Escolher cor"), opts);
    if (!chosen.isValid()) return;
    const QString s = colorToString(chosen);
    setColorIn(fieldKey, s);
    paintSwatch(btn, s);
    // Atualiza label hex correspondente.
    for (auto* hex : findChildren<QLabel*>(QStringLiteral("themeColorHex"))) {
        if (hex->property("fieldKey").toString() == fieldKey) {
            hex->setText(s);
            break;
        }
    }
    refreshPreview();
}

void ThemeEditorDialog::onShadowColorClicked()
{
    QColor initial = parseAnyColor(m_theme.pageShadowColor);
    if (!initial.isValid()) initial = QColor(0, 0, 0, 140);
    QColor chosen = QColorDialog::getColor(initial, this, tr("Cor da sombra"),
                                           QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid()) return;
    m_theme.pageShadowColor = colorToString(chosen);
    paintSwatch(m_shadowColorSwatch, m_theme.pageShadowColor);
    refreshPreview();
}

void ThemeEditorDialog::onPickImage()
{
    const QString filter = tr("Imagens (*.png *.jpg *.jpeg *.bmp *.webp *.tiff)");
    const QString path = QFileDialog::getOpenFileName(this,
        tr("Escolher imagem de fundo"), QString(), filter);
    if (path.isEmpty()) return;
    m_theme.backgroundImage = path;
    refreshImageLabel();
    refreshPreview();
}

void ThemeEditorDialog::onClearImage()
{
    m_theme.backgroundImage.clear();
    refreshImageLabel();
    refreshPreview();
}

void ThemeEditorDialog::refreshPreview()
{
    if (m_preview) m_preview->setTheme(m_theme);
}

void ThemeEditorDialog::refreshImageLabel()
{
    if (m_theme.backgroundImage.isEmpty()) {
        m_imagePathLabel->setText(QStringLiteral("<i>%1</i>").arg(tr("Sem imagem")));
        m_imagePathLabel->setToolTip(QString());
    } else {
        const QString name = QFileInfo(m_theme.backgroundImage).fileName();
        m_imagePathLabel->setText(name);
        m_imagePathLabel->setToolTip(m_theme.backgroundImage);
    }
}

void ThemeEditorDialog::refreshSwatch(QPushButton* btn, const QString& colorStr)
{
    paintSwatch(btn, colorStr);
}

void ThemeEditorDialog::applyDialogStyle()
{
    setStyleSheet(QStringLiteral(R"(
        #themeEditorDialog {
            background: %1;
        }
        #themeEditorDialog QLabel {
            color: %2;
            font-size: 12px;
        }
        #themeEditorDialog QLabel#themeEditorFieldLabel {
            color: %4;
            font-size: 12px;
        }
        #themeEditorDialog QLabel#themeColorHex {
            color: %4;
            font-family: monospace;
            font-size: 11px;
        }
        #themeEditorDialog QLabel#themeImagePath {
            color: %3;
            font-size: 12px;
        }
        #themeEditorDialog QLabel#themeEditorPreviewHeader {
            color: %4;
            font-size: 11px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.04em;
        }
        #themeEditorDialog QGroupBox#themeGroup {
            color: %3;
            border: 1px solid %6;
            border-radius: 6px;
            margin-top: 14px;
            padding-top: 16px;
            font-weight: 600;
        }
        #themeEditorDialog QGroupBox#themeGroup::title {
            subcontrol-origin: margin;
            left: 12px;
            top: -2px;
            padding: 0 6px;
        }
        #themeEditorDialog QLineEdit, #themeEditorDialog QSpinBox, #themeEditorDialog QComboBox {
            background: %5;
            color: %2;
            border: 1px solid %6;
            border-radius: 4px;
            padding: 4px 6px;
            min-height: 22px;
        }
        #themeEditorDialog QPushButton {
            background: %5;
            color: %2;
            border: 1px solid %6;
            padding: 6px 12px;
            border-radius: 4px;
            font-size: 12px;
        }
        #themeEditorDialog QPushButton:hover {
            background: %7;
            color: %3;
        }
        #themeEditorDialog QCheckBox {
            color: %2;
            spacing: 6px;
        }
        #themeEditorDialog QScrollArea {
            background: transparent;
            border: none;
        }
        #themeEditorDialog QScrollArea > QWidget > QWidget {
            background: transparent;
        }
        #themeEditorDialog QSlider::groove:horizontal {
            background: %7;
            height: 4px;
            border-radius: 2px;
        }
        #themeEditorDialog QSlider::handle:horizontal {
            background: %9;
            width: 14px;
            height: 14px;
            margin: -6px 0;
            border-radius: 7px;
        }
        #themeEditorPreview {
            border: 1px solid %6;
            border-radius: 6px;
        }
    )").arg(
        Theme::appBackground(),     // 1
        Theme::textPrimary(),       // 2
        Theme::textBright(),        // 3
        Theme::textMuted(),         // 4
        Theme::panelBackground(),   // 5
        Theme::panelBorder(),       // 6
        Theme::hoverOverlay(),      // 7
        Theme::subtleBorder(),      // 8
        Theme::accentDefault()      // 9
    ));
}
