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

constexpr int kCoverFrameW = 220;
constexpr int kCoverFrameH = 330;
constexpr int kMaxCoverSide = 1200;

// Mesma estratégia do ProjectInfoPanel: lê, redimensiona se necessário,
// devolve data URL JPEG. Mantém compat com Mira 1.
QString loadCoverAsDataUrl(const QString& path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) return {};
    if (img.width() > kMaxCoverSide || img.height() > kMaxCoverSide) {
        img = img.scaled(kMaxCoverSide, kMaxCoverSide,
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", 88);
    return QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(bytes.toBase64());
}

QPixmap pixmapFromDataUrl(const QString& dataUrl) {
    if (dataUrl.isEmpty()) return {};
    const int comma = dataUrl.indexOf(QLatin1Char(','));
    if (comma < 0) return {};
    const QByteArray raw = QByteArray::fromBase64(dataUrl.mid(comma + 1).toLatin1());
    QPixmap pm;
    pm.loadFromData(raw);
    return pm;
}

} // namespace

// =============================================================
// NewProjectTemplateDialog
// =============================================================

NewProjectTemplateDialog::NewProjectTemplateDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("newProjectTemplate"));
    setWindowTitle(tr("Escolher template"));
    setModal(true);
    resize(420, 360);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(10);

    auto* title = new QLabel(tr("Escolha um template"), this);
    title->setObjectName(QStringLiteral("npHeading"));
    root->addWidget(title);

    struct Tpl { QString id; QString name; QString desc; };
    const QList<Tpl> tpls = {
        {QStringLiteral("blank"),    tr("Projeto em branco"), tr("Projeto limpo, sem conteúdo inicial.")},
        {QStringLiteral("basic"),    tr("Projeto básico"),    tr("Gavetas de planejamento, personagens, cenários, objetos e notas.")},
        {QStringLiteral("advanced"), tr("Projeto avançado"),  tr("Estrutura completa: lore, base de dados, pesquisa, planejamento detalhado.")},
    };

    auto* group = new QButtonGroup(this);
    bool first = true;
    for (const Tpl& tpl : tpls) {
        auto* rb = new QRadioButton(tpl.name, this);
        rb->setObjectName(QStringLiteral("npTemplateOption"));
        rb->setCursor(Qt::PointingHandCursor);
        if (first) { rb->setChecked(true); m_templateId = tpl.id; first = false; }
        const QString tplId = tpl.id;
        const QString tplDesc = tpl.desc;
        connect(rb, &QRadioButton::toggled, this, [this, tplId, tplDesc](bool on) {
            if (!on) return;
            m_templateId = tplId;
            if (m_hintLabel) m_hintLabel->setText(tplDesc + QStringLiteral("\n") + tr("Você pode mudar isso depois."));
        });
        group->addButton(rb);
        root->addWidget(rb);
    }

    m_hintLabel = new QLabel(tpls.first().desc + QStringLiteral("\n") + tr("Você pode mudar isso depois."), this);
    m_hintLabel->setObjectName(QStringLiteral("npHint"));
    m_hintLabel->setWordWrap(true);
    root->addWidget(m_hintLabel);

    root->addStretch();

    auto* actions = new QHBoxLayout;
    actions->addStretch();
    auto* cancelBtn = new QPushButton(tr("Cancelar"), this);
    cancelBtn->setObjectName(QStringLiteral("npBtn"));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    auto* okBtn = new QPushButton(tr("Continuar"), this);
    okBtn->setObjectName(QStringLiteral("npBtnPrimary"));
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    actions->addWidget(cancelBtn);
    actions->addWidget(okBtn);
    root->addLayout(actions);

    applyDialogStyle();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &NewProjectTemplateDialog::applyDialogStyle);
}

void NewProjectTemplateDialog::applyDialogStyle() {
    setStyleSheet(QStringLiteral(R"(
        #newProjectTemplate { background: %1; }
        #npHeading { color: %3; font-size: 16px; font-weight: 600; padding-bottom: 6px; }
        #npHint { color: %4; font-size: 11px; font-style: italic; padding: 8px 2px; line-height: 150%; }
        #npTemplateOption { color: %2; font-size: 13px; padding: 4px 2px; spacing: 10px; }
        #npTemplateOption::indicator {
            width: 14px; height: 14px;
            border: 1px solid %6; border-radius: 3px; background: %5;
        }
        #npTemplateOption::indicator:hover { border-color: %9; }
        #npTemplateOption::indicator:checked {
            background: %9; border-color: %9;
            image: url(:/icons/check.svg);
        }
        QPushButton#npBtn, QPushButton#npBtnPrimary {
            background: %5; color: %2; border: 1px solid %6;
            padding: 6px 16px; border-radius: 6px; font-size: 12px; min-height: 26px;
        }
        QPushButton#npBtn:hover, QPushButton#npBtnPrimary:hover {
            background: %7; color: %3; border-color: %9;
        }
        QPushButton#npBtnPrimary {
            background: %9; color: white; border-color: %9;
        }
        QPushButton#npBtnPrimary:hover {
            background: %9; color: white;
        }
    )").arg(
        Theme::appBackground(), Theme::textPrimary(), Theme::textBright(),
        Theme::textMuted(), Theme::panelBackground(), Theme::panelBorder(),
        Theme::hoverOverlay(), Theme::subtleBorder(), Theme::accentDefault()
    ));
}

// =============================================================
// NewProjectDetailsDialog
// =============================================================

NewProjectDetailsDialog::NewProjectDetailsDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("newProjectDetails"));
    setWindowTitle(tr("Detalhes da obra"));
    setModal(true);
    resize(720, 520);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(22, 22, 22, 18);
    root->setSpacing(22);

    // ---- Coluna esquerda: capa ----
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(8);

    m_coverPreview = new QLabel(this);
    m_coverPreview->setObjectName(QStringLiteral("npCover"));
    m_coverPreview->setFixedSize(kCoverFrameW, kCoverFrameH);
    m_coverPreview->setAlignment(Qt::AlignCenter);
    m_coverPreview->setText(tr("Sem capa"));
    leftCol->addWidget(m_coverPreview);

    auto* pickBtn = new QPushButton(tr("Escolher capa…"), this);
    pickBtn->setObjectName(QStringLiteral("npBtn"));
    pickBtn->setCursor(Qt::PointingHandCursor);
    connect(pickBtn, &QPushButton::clicked, this, &NewProjectDetailsDialog::onPickCover);
    leftCol->addWidget(pickBtn);

    auto* clearBtn = new QPushButton(tr("Remover capa"), this);
    clearBtn->setObjectName(QStringLiteral("npBtn"));
    clearBtn->setCursor(Qt::PointingHandCursor);
    connect(clearBtn, &QPushButton::clicked, this, &NewProjectDetailsDialog::onClearCover);
    leftCol->addWidget(clearBtn);

    leftCol->addStretch();
    root->addLayout(leftCol);

    // ---- Coluna direita: formulário ----
    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(6);

    auto* title = new QLabel(tr("Detalhes da obra"), this);
    title->setObjectName(QStringLiteral("npHeading"));
    rightCol->addWidget(title);

    auto addLabel = [this, rightCol](const QString& text) {
        auto* lab = new QLabel(text, this);
        lab->setObjectName(QStringLiteral("npLabel"));
        rightCol->addWidget(lab);
    };

    addLabel(tr("Nome do projeto *"));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setText(tr("Projeto"));
    rightCol->addWidget(m_nameEdit);

    addLabel(tr("Nome do autor *"));
    m_authorEdit = new QLineEdit(this);
    m_authorEdit->setPlaceholderText(tr("Ex: Maria Silva"));
    rightCol->addWidget(m_authorEdit);

    addLabel(tr("Gêneros"));
    m_genresEdit = new QLineEdit(this);
    m_genresEdit->setPlaceholderText(tr("Ex: Fantasia, Romance"));
    rightCol->addWidget(m_genresEdit);

    addLabel(tr("Sinopse"));
    m_synopsisEdit = new QPlainTextEdit(this);
    m_synopsisEdit->setPlaceholderText(tr("Escreva uma breve sinopse…"));
    rightCol->addWidget(m_synopsisEdit, /*stretch=*/1);

    auto* note = new QLabel(tr("Você pode editar isso mais tarde."), this);
    note->setObjectName(QStringLiteral("npNote"));
    rightCol->addWidget(note);

    auto* actions = new QHBoxLayout;
    actions->addStretch();
    auto* cancelBtn = new QPushButton(tr("Cancelar"), this);
    cancelBtn->setObjectName(QStringLiteral("npBtn"));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    m_continueBtn = new QPushButton(tr("Continuar"), this);
    m_continueBtn->setObjectName(QStringLiteral("npBtnPrimary"));
    m_continueBtn->setDefault(true);
    connect(m_continueBtn, &QPushButton::clicked, this, &QDialog::accept);
    actions->addWidget(cancelBtn);
    actions->addWidget(m_continueBtn);
    rightCol->addLayout(actions);

    root->addLayout(rightCol, /*stretch=*/1);

    auto refreshEnabled = [this]() {
        const bool ok = !m_nameEdit->text().trimmed().isEmpty()
                     && !m_authorEdit->text().trimmed().isEmpty();
        m_continueBtn->setEnabled(ok);
    };
    connect(m_nameEdit, &QLineEdit::textChanged, this, refreshEnabled);
    connect(m_authorEdit, &QLineEdit::textChanged, this, refreshEnabled);
    refreshEnabled();

    applyDialogStyle();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &NewProjectDetailsDialog::applyDialogStyle);
}

void NewProjectDetailsDialog::onPickCover() {
    const QString startDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString path = QFileDialog::getOpenFileName(this,
        tr("Escolher capa"), startDir,
        tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (path.isEmpty()) return;
    const QString dataUrl = loadCoverAsDataUrl(path);
    if (dataUrl.isEmpty()) return;
    m_coverDataUrl = dataUrl;
    updateCoverPreview();
}

void NewProjectDetailsDialog::onClearCover() {
    if (m_coverDataUrl.isEmpty()) return;
    m_coverDataUrl.clear();
    updateCoverPreview();
}

void NewProjectDetailsDialog::updateCoverPreview() {
    if (!m_coverPreview) return;
    const QPixmap pm = pixmapFromDataUrl(m_coverDataUrl);
    if (pm.isNull()) {
        m_coverPreview->clear();
        m_coverPreview->setText(tr("Sem capa"));
        return;
    }
    m_coverPreview->setPixmap(pm.scaled(kCoverFrameW, kCoverFrameH,
                                        Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QString NewProjectDetailsDialog::projectName() const { return m_nameEdit->text().trimmed(); }
QString NewProjectDetailsDialog::author() const      { return m_authorEdit->text().trimmed(); }
QString NewProjectDetailsDialog::genres() const      { return m_genresEdit->text().trimmed(); }
QString NewProjectDetailsDialog::synopsis() const    { return m_synopsisEdit->toPlainText().trimmed(); }

void NewProjectDetailsDialog::applyDialogStyle() {
    setStyleSheet(QStringLiteral(R"(
        #newProjectDetails { background: %1; }
        #npHeading { color: %3; font-size: 16px; font-weight: 600; padding-bottom: 6px; }
        #npLabel { color: %4; font-size: 11px; margin-top: 4px; }
        #npNote { color: %4; font-size: 11px; font-style: italic; padding: 6px 0; }
        #npCover {
            background: %5; color: %4; border: 1px solid %6;
            border-radius: 6px; font-size: 11px;
        }
        QLineEdit, QPlainTextEdit {
            background: %5; color: %3; border: 1px solid %6;
            border-radius: 6px; padding: 6px 8px;
            selection-background-color: %7;
        }
        QLineEdit:focus, QPlainTextEdit:focus { border-color: %9; }
        QPushButton#npBtn, QPushButton#npBtnPrimary {
            background: %5; color: %2; border: 1px solid %6;
            padding: 6px 16px; border-radius: 6px; font-size: 12px; min-height: 26px;
        }
        QPushButton#npBtn:hover { background: %7; color: %3; border-color: %9; }
        QPushButton#npBtnPrimary { background: %9; color: white; border-color: %9; }
        QPushButton#npBtnPrimary:hover { background: %9; }
        QPushButton#npBtnPrimary:disabled { background: %5; color: %4; border-color: %6; }
    )").arg(
        Theme::appBackground(), Theme::textPrimary(), Theme::textBright(),
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
        tr("Escolha a pasta onde <b>%1</b> será criada. Se quiser, use o botão ao lado para navegar.")
            .arg(m_safeName.toHtmlEscaped()),
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
        tr("Você não precisa criar a pasta — o Mira faz isso automaticamente."), this);
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
    setStyleSheet(QStringLiteral(R"(
        #newProjectFolder { background: %1; }
        #npHeading { color: %3; font-size: 16px; font-weight: 600; padding-bottom: 4px; }
        #npSub { color: %4; font-size: 12px; line-height: 150%; }
        #npNote { color: %4; font-size: 11px; font-style: italic; padding-top: 8px; }
        #npPathBox {
            background: %5; border: 1px solid %6; border-radius: 8px;
        }
        #npPath { color: %2; font-size: 12px; }
        QPushButton#npBtn {
            background: %5; color: %2; border: 1px solid %6;
            padding: 6px 14px; border-radius: 6px; font-size: 12px; min-height: 26px;
        }
        QPushButton#npBtn:hover { background: %7; color: %3; border-color: %9; }
        QPushButton#npBtnPrimary {
            background: %9; color: white; border: 1px solid %9;
            padding: 6px 16px; border-radius: 6px; font-size: 12px; min-height: 26px;
        }
        QPushButton#npBtnPrimary:hover { background: %9; }
    )").arg(
        Theme::appBackground(), Theme::textPrimary(), Theme::textBright(),
        Theme::textMuted(), Theme::panelBackground(), Theme::panelBorder(),
        Theme::hoverOverlay(), Theme::subtleBorder(), Theme::accentDefault()
    ));
    updatePathDisplay(); // re-renderiza com nova cor accent
}
