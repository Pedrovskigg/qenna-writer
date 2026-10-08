#include "NewProjectFlow.h"

#include "Theme.h"

#include <QBuffer>
#include <QButtonGroup>
#include <QByteArray>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {

QString sanitizeProjectName(const QString& raw) {
    QString s = raw;
    s.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));
    s = s.trimmed();
    if (s.isEmpty()) s = QObject::tr("Novo Projeto");
    return s;
}

} // namespace

// =============================================================
// NewIdeaNameDialog
// =============================================================

NewIdeaNameDialog::NewIdeaNameDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("newIdeaName"));
    setWindowTitle(tr("Salvar ideia"));
    setModal(true);
    resize(420, 200);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 20);
    root->setSpacing(8);

    auto* title = new QLabel(tr("Como quer chamar esse projeto?"), this);
    title->setObjectName(QStringLiteral("npHeading"));
    root->addWidget(title);

    auto* sub = new QLabel(
        tr("Só o nome por enquanto — autor, gêneros e sinopse dá pra preencher "
           "depois nos detalhes do projeto."), this);
    sub->setObjectName(QStringLiteral("npSub"));
    sub->setWordWrap(true);
    root->addWidget(sub);

    root->addSpacing(6);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Ex: Minha nova ideia"));
    root->addWidget(m_nameEdit);

    root->addStretch();

    auto* actions = new QHBoxLayout;
    actions->addStretch();
    auto* cancelBtn = new QPushButton(tr("Cancelar"), this);
    cancelBtn->setObjectName(QStringLiteral("npBtn"));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    m_continueBtn = new QPushButton(tr("Continuar"), this);
    m_continueBtn->setObjectName(QStringLiteral("npBtnPrimary"));
    m_continueBtn->setDefault(true);
    m_continueBtn->setEnabled(false);
    connect(m_continueBtn, &QPushButton::clicked, this, &QDialog::accept);
    actions->addWidget(cancelBtn);
    actions->addWidget(m_continueBtn);
    root->addLayout(actions);

    connect(m_nameEdit, &QLineEdit::textChanged, this, [this]() {
        m_continueBtn->setEnabled(!m_nameEdit->text().trimmed().isEmpty());
    });

    applyDialogStyle();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &NewIdeaNameDialog::applyDialogStyle);
}

QString NewIdeaNameDialog::projectName() const { return m_nameEdit->text().trimmed(); }

void NewIdeaNameDialog::applyDialogStyle() {
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        #newIdeaName { background: %1; }
        #npHeading { color: %3; font-size: 16px; font-weight: 600; padding-bottom: 4px; }
        #npSub { color: %4; font-size: 12px; line-height: 150%; }
        QLineEdit {
            background: %5; color: %3; border: 1px solid %6;
            border-radius: @radius-control; padding: 6px 8px;
            selection-background-color: %7;
        }
        QLineEdit:focus { border-color: %9; }
        QPushButton#npBtn, QPushButton#npBtnPrimary {
            background: %5; color: %2; border: 1px solid %6;
            padding: 6px 16px; border-radius: @radius-control; font-size: 12px; min-height: 26px;
        }
        QPushButton#npBtn:hover { background: %7; color: %3; border-color: %9; }
        QPushButton#npBtnPrimary { background: %9; color: white; border-color: %9; }
        QPushButton#npBtnPrimary:hover { background: %9; }
        QPushButton#npBtnPrimary:disabled { background: %5; color: %4; border-color: %6; }
    )")).arg(
        Theme::chromeBackground(), Theme::textPrimary(), Theme::textBright(),
        Theme::textMuted(), Theme::panelBackground(), Theme::panelBorder(),
        Theme::hoverOverlay(), Theme::subtleBorder(), Theme::accentDefault()
    ));
}

// =============================================================
// NewProjectFolderDialog
// =============================================================

NewProjectFolderDialog::NewProjectFolderDialog(const QString& projectName, QWidget* parent)
    : QDialog(parent)
    , m_projectName(projectName)
    , m_safeName(sanitizeProjectName(projectName))
    , m_parentPath(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
{
    setObjectName(QStringLiteral("newProjectFolder"));
    setWindowTitle(tr("Onde salvar o projeto?"));
    setModal(true);
    resize(560, 280);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 20);
    root->setSpacing(8);

    auto* title = new QLabel(tr("Onde salvar o projeto?"), this);
    title->setObjectName(QStringLiteral("npHeading"));
    root->addWidget(title);

    auto* sub = new QLabel(
        tr("Escolha o local onde será criada a pasta do seu projeto, os arquivos referente a ele serão salvos dentro dela. Use o botão abaixo para navegar."),
        this);
    sub->setObjectName(QStringLiteral("npSub"));
    sub->setWordWrap(true);
    sub->setTextFormat(Qt::RichText);
    root->addWidget(sub);

    root->addSpacing(10);

    auto* pathRow = new QHBoxLayout;
    pathRow->setSpacing(8);
    auto* pathBox = new QFrame(this);
    pathBox->setObjectName(QStringLiteral("npPathBox"));
    auto* pathBoxLay = new QHBoxLayout(pathBox);
    pathBoxLay->setContentsMargins(12, 10, 8, 10);
    pathBoxLay->setSpacing(8);

    m_pathLabel = new QLabel(pathBox);
    m_pathLabel->setObjectName(QStringLiteral("npPath"));
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setTextFormat(Qt::RichText);
    pathBoxLay->addWidget(m_pathLabel, /*stretch=*/1);

    auto* browseBtn = new QPushButton(tr("Trocar…"), pathBox);
    browseBtn->setObjectName(QStringLiteral("npBtn"));
    browseBtn->setCursor(Qt::PointingHandCursor);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString chosen = QFileDialog::getExistingDirectory(this,
            tr("Escolher pasta-pai"),
            m_parentPath,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!chosen.isEmpty()) {
            m_parentPath = chosen;
            updatePathDisplay();
        }
    });
    pathBoxLay->addWidget(browseBtn);

    pathRow->addWidget(pathBox);
    root->addLayout(pathRow);

    auto* note = new QLabel(
        tr("Você não precisa criar a pasta — o Qenna Writer faz isso automaticamente."), this);
    note->setObjectName(QStringLiteral("npNote"));
    root->addWidget(note);

    root->addStretch();

    auto* actions = new QHBoxLayout;
    actions->addStretch();
    auto* cancelBtn = new QPushButton(tr("Cancelar"), this);
    cancelBtn->setObjectName(QStringLiteral("npBtn"));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    auto* createBtn = new QPushButton(tr("Criar projeto"), this);
    createBtn->setObjectName(QStringLiteral("npBtnPrimary"));
    createBtn->setDefault(true);
    connect(createBtn, &QPushButton::clicked, this, &QDialog::accept);
    actions->addWidget(cancelBtn);
    actions->addWidget(createBtn);
    root->addLayout(actions);

    updatePathDisplay();
    applyDialogStyle();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &NewProjectFolderDialog::applyDialogStyle);
}

QString NewProjectFolderDialog::fullPath() const {
    return QDir(m_parentPath).absoluteFilePath(m_safeName);
}

void NewProjectFolderDialog::updatePathDisplay() {
    if (!m_pathLabel) return;
    const QString sep = QDir::separator();
    const QString accent = Theme::accentDefault();
    m_pathLabel->setText(
        QStringLiteral("%1<span style='color:%3;font-weight:600;'>%2%4</span>")
            .arg(QDir::toNativeSeparators(m_parentPath).toHtmlEscaped(),
                 sep, accent, m_safeName.toHtmlEscaped()));
}

void NewProjectFolderDialog::applyDialogStyle() {
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        #newProjectFolder { background: %1; }
        #npHeading { color: %3; font-size: 16px; font-weight: 600; padding-bottom: 4px; }
        #npSub { color: %4; font-size: 12px; line-height: 150%; }
        #npNote { color: %4; font-size: 11px; font-style: italic; padding-top: 8px; }
        #npPathBox {
            background: %5; border: 1px solid %6; border-radius: @radius-panel;
        }
        #npPath { color: %2; font-size: 12px; }
        QPushButton#npBtn {
            background: %5; color: %2; border: 1px solid %6;
            padding: 6px 14px; border-radius: @radius-control; font-size: 12px; min-height: 26px;
        }
        QPushButton#npBtn:hover { background: %7; color: %3; border-color: %9; }
        QPushButton#npBtnPrimary {
            background: %9; color: white; border: 1px solid %9;
            padding: 6px 16px; border-radius: @radius-control; font-size: 12px; min-height: 26px;
        }
        QPushButton#npBtnPrimary:hover { background: %9; }
    )")).arg(
        Theme::chromeBackground(), Theme::textPrimary(), Theme::textBright(),
        Theme::textMuted(), Theme::panelBackground(), Theme::panelBorder(),
        Theme::hoverOverlay(), Theme::subtleBorder(), Theme::accentDefault()
    ));
    updatePathDisplay(); // re-renderiza com nova cor accent
}
