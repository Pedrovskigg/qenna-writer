#include "ThemeExportDialog.h"

#include "Theme.h"

#include "ThemePackage.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

ThemeExportDialog::ThemeExportDialog(const Theme::MiraTheme& theme, QWidget* parent)
    : QDialog(parent)
    , m_theme(theme)
    , m_authorEdit(nullptr)
    , m_contactEdit(nullptr)
    , m_licenseCombo(nullptr)
    , m_descriptionEdit(nullptr)
{
    setObjectName(QStringLiteral("themeExportDialog"));
    setWindowTitle(tr("Exportar tema"));
    setModal(true);
    buildUi();
    loadRemembered();
    applyDialogStyle();
}

void ThemeExportDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(14);

    auto* title = new QLabel(tr("Exportar “%1”").arg(m_theme.name), this);
    title->setObjectName(QStringLiteral("themeExportTitle"));
    root->addWidget(title);

    auto* hint = new QLabel(
        tr("Estes dados vão dentro do arquivo do tema, pra que quem receber saiba de quem ele é."),
        this);
    hint->setObjectName(QStringLiteral("themeExportHint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_authorEdit = new QLineEdit(this);
    m_authorEdit->setPlaceholderText(tr("Seu nome ou apelido"));
    m_authorEdit->setMaxLength(80);
    form->addRow(tr("Autor"), m_authorEdit);

    m_contactEdit = new QLineEdit(this);
    m_contactEdit->setPlaceholderText(tr("Site, e-mail ou @ — opcional"));
    m_contactEdit->setMaxLength(120);
    form->addRow(tr("Contato"), m_contactEdit);

    m_licenseCombo = new QComboBox(this);
    m_licenseCombo->setEditable(true); // o autor pode escrever a dele
    for (const QString& code : ThemePackage::licenseCodes())
        m_licenseCombo->addItem(ThemePackage::licenseDisplayName(code), code);
    m_licenseCombo->lineEdit()->setPlaceholderText(tr("O que os outros podem fazer com o tema"));
    form->addRow(tr("Licença"), m_licenseCombo);

    m_descriptionEdit = new QLineEdit(this);
    m_descriptionEdit->setPlaceholderText(tr("Uma linha sobre o tema — opcional"));
    m_descriptionEdit->setMaxLength(160);
    form->addRow(tr("Descrição"), m_descriptionEdit);

    root->addLayout(form);

    if (!m_theme.backgroundImage.isEmpty()) {
        auto* imgNote = new QLabel(
            tr("A imagem de fundo vai junto, dentro do arquivo."), this);
        imgNote->setObjectName(QStringLiteral("themeExportHint"));
        imgNote->setWordWrap(true);
        root->addWidget(imgNote);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Continuar"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(buttons, &QDialogButtonBox::accepted, this, &ThemeExportDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    setMinimumWidth(460);
}

void ThemeExportDialog::loadRemembered()
{
    QSettings s;
    m_authorEdit->setText(m_theme.author.isEmpty()
        ? s.value(QStringLiteral("theme/exportAuthor")).toString()
        : m_theme.author);
    m_contactEdit->setText(m_theme.authorContact.isEmpty()
        ? s.value(QStringLiteral("theme/exportAuthorContact")).toString()
        : m_theme.authorContact);
    m_descriptionEdit->setText(m_theme.description);

    // Licença: o que o tema já declara ganha do que foi usado da última vez.
    const QString license = m_theme.license.isEmpty()
        ? s.value(QStringLiteral("theme/exportLicense"),
                  QStringLiteral("free-with-credit")).toString()
        : m_theme.license;
    const int idx = m_licenseCombo->findData(license);
    if (idx >= 0) m_licenseCombo->setCurrentIndex(idx);
    else m_licenseCombo->setCurrentText(license); // licença própria do autor
}

void ThemeExportDialog::saveRemembered() const
{
    QSettings s;
    s.setValue(QStringLiteral("theme/exportAuthor"), m_theme.author);
    s.setValue(QStringLiteral("theme/exportAuthorContact"), m_theme.authorContact);
    s.setValue(QStringLiteral("theme/exportLicense"), m_theme.license);
}

void ThemeExportDialog::onAccept()
{
    m_theme.author = m_authorEdit->text().trimmed();
    m_theme.authorContact = m_contactEdit->text().trimmed();
    m_theme.description = m_descriptionEdit->text().trimmed();

    // Se o texto no combo é exatamente um dos nossos rótulos, gravamos o código
    // por trás dele; qualquer outra coisa é licença escrita pelo autor e vai
    // como texto mesmo.
    const QString typed = m_licenseCombo->currentText().trimmed();
    const int known = m_licenseCombo->findText(typed);
    m_theme.license = (known >= 0) ? m_licenseCombo->itemData(known).toString() : typed;

    saveRemembered();
    accept();
}

void ThemeExportDialog::applyDialogStyle()
{
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        #themeExportDialog {
            background: %1;
        }
        #themeExportDialog QLabel {
            color: %2;
            font-size: 12px;
        }
        #themeExportTitle {
            color: %3;
            font-size: 15px;
            font-weight: 600;
        }
        #themeExportHint {
            color: %4;
            font-size: 11px;
        }
        #themeExportDialog QLineEdit, #themeExportDialog QComboBox {
            background: %5;
            color: %2;
            border: 1px solid %6;
            border-radius: @radius-control;
            padding: 6px 8px;
            font-size: 12px;
        }
        #themeExportDialog QLineEdit:focus, #themeExportDialog QComboBox:focus {
            border: 1px solid %7;
        }
        #themeExportDialog QPushButton {
            background: %8;
            color: %2;
            border: 1px solid %6;
            border-radius: @radius-control;
            padding: 6px 16px;
            font-size: 12px;
        }
        #themeExportDialog QPushButton:hover {
            background: %9;
        }
    )"))
        .arg(Theme::panelBackground(),
             Theme::textPrimary(),
             Theme::textBright(),
             Theme::textMuted(),
             Theme::inputBackground(),
             Theme::subtleBorder(),
             Theme::focusBorder(),
             Theme::hoverOverlay(),
             Theme::hoverStrong()));
}
