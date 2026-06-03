#include "ElementCreateDialog.h"

#include "Theme.h"

#include <QBuffer>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QString labelForType(const QString& t) {
    if (t == QStringLiteral("character")) return QObject::tr("Novo personagem");
    if (t == QStringLiteral("setting"))   return QObject::tr("Novo cenário");
    if (t == QStringLiteral("object"))    return QObject::tr("Novo objeto");
    return QObject::tr("Novo documento");
}

QString nameLabelForType(const QString& t) {
    if (t == QStringLiteral("character")) return QObject::tr("Nome do personagem:");
    if (t == QStringLiteral("setting"))   return QObject::tr("Nome do cenário:");
    if (t == QStringLiteral("object"))    return QObject::tr("Nome do objeto:");
    return QObject::tr("Nome:");
}

// Carrega imagem do disco, redimensiona pra 400×400 quadrado (crop centralizado)
// e devolve como data URL JPEG base64.
QString loadImageAsDataUrl(const QString& path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) return QString();
    // Crop centralizado pra quadrado
    const int side = qMin(img.width(), img.height());
    const int x = (img.width() - side) / 2;
    const int y = (img.height() - side) / 2;
    QImage square = img.copy(x, y, side, side);
    QImage scaled = square.scaled(400, 400, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    scaled.save(&buf, "JPEG", 92);
    return QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(bytes.toBase64());
}

QPixmap pixmapFromDataUrl(const QString& dataUrl) {
    if (dataUrl.isEmpty()) return QPixmap();
    const int comma = dataUrl.indexOf(QLatin1Char(','));
    if (comma < 0) return QPixmap();
    const QByteArray raw = QByteArray::fromBase64(dataUrl.mid(comma + 1).toLatin1());
    QPixmap pm;
    pm.loadFromData(raw);
    return pm;
}

}

ElementCreateDialog::ElementCreateDialog(const QString& elementType, QWidget* parent)
    : QDialog(parent)
    , m_elementType(elementType)
    , m_titleEdit(nullptr)
    , m_roleCombo(nullptr)
    , m_narratorCheck(nullptr)
    , m_imagePreview(nullptr)
    , m_pickImageBtn(nullptr)
    , m_clearImageBtn(nullptr)
    , m_okBtn(nullptr)
    , m_cancelBtn(nullptr)
{
    setObjectName(QStringLiteral("elementCreateDialog"));
    setWindowTitle(labelForType(elementType));
    setModal(true);
    buildUi();
    setMinimumWidth(360);
}

void ElementCreateDialog::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(10);

    // Foto (se for elemento visual)
    const bool visual = m_elementType == QStringLiteral("character")
        || m_elementType == QStringLiteral("setting")
        || m_elementType == QStringLiteral("object");
    if (visual) {
        auto* imgRow = new QHBoxLayout();
        m_imagePreview = new QLabel(this);
        m_imagePreview->setObjectName(QStringLiteral("ecdImagePreview"));
        m_imagePreview->setFixedSize(96, 96);
        m_imagePreview->setAlignment(Qt::AlignCenter);
        m_imagePreview->setText(tr("sem foto"));
        imgRow->addWidget(m_imagePreview);

        auto* imgBtnsCol = new QVBoxLayout();
        m_pickImageBtn = new QPushButton(tr("Escolher foto…"), this);
        m_pickImageBtn->setObjectName(QStringLiteral("ecdBtn"));
        m_pickImageBtn->setCursor(Qt::PointingHandCursor);
        connect(m_pickImageBtn, &QPushButton::clicked, this, &ElementCreateDialog::pickImage);
        m_clearImageBtn = new QPushButton(tr("Remover"), this);
        m_clearImageBtn->setObjectName(QStringLiteral("ecdBtn"));
        m_clearImageBtn->setCursor(Qt::PointingHandCursor);
        connect(m_clearImageBtn, &QPushButton::clicked, this, &ElementCreateDialog::clearImage);
        imgBtnsCol->addWidget(m_pickImageBtn);
        imgBtnsCol->addWidget(m_clearImageBtn);
        imgBtnsCol->addStretch();
        imgRow->addLayout(imgBtnsCol);
        imgRow->addStretch();
        outer->addLayout(imgRow);
    }

    // Nome
    auto* nameLabel = new QLabel(nameLabelForType(m_elementType), this);
    outer->addWidget(nameLabel);
    m_titleEdit = new QLineEdit(this);
    outer->addWidget(m_titleEdit);

    // Role + Narrador (só personagem)
    if (m_elementType == QStringLiteral("character")) {
        // Apelidos: variações de nome que o detector de presença também procura
        // (ex.: "Mari", "a herdeira"). Separados por vírgula.
        auto* aliasesLabel = new QLabel(tr("Apelidos:"), this);
        outer->addWidget(aliasesLabel);
        m_aliasesEdit = new QLineEdit(this);
        m_aliasesEdit->setPlaceholderText(tr("separados por vírgula (ex.: Mari, a herdeira)"));
        outer->addWidget(m_aliasesEdit);

        auto* roleLabel = new QLabel(tr("Papel:"), this);
        outer->addWidget(roleLabel);
        m_roleCombo = new QComboBox(this);
        m_roleCombo->setEditable(true);
        m_roleCombo->addItem(tr("Protagonista"), QStringLiteral("PROTAGONISTA"));
        m_roleCombo->addItem(tr("Deuteragonista"), QStringLiteral("DEUTERAGONISTA"));
        m_roleCombo->addItem(tr("Coadjuvante"), QStringLiteral("COADJUVANTE"));
        m_roleCombo->addItem(tr("Antagonista"), QStringLiteral("ANTAGONISTA"));
        m_roleCombo->addItem(tr("Contraponto"), QStringLiteral("CONTRAPONTO"));
        m_roleCombo->addItem(tr("Trickster"), QStringLiteral("TRICKSTER"));
        m_roleCombo->addItem(tr("Mentor"), QStringLiteral("MENTOR"));
        m_roleCombo->addItem(tr("Figurante"), QStringLiteral("FIGURANTE"));
        m_roleCombo->setCurrentIndex(-1);
        outer->addWidget(m_roleCombo);

        m_narratorCheck = new QCheckBox(tr("Narrador (voz em 1ª pessoa)"), this);
        m_narratorCheck->setObjectName(QStringLiteral("ecdNarratorCheck"));
        outer->addWidget(m_narratorCheck);

        auto* trackLabel = new QLabel(tr("Trilha na linha do tempo:"), this);
        outer->addWidget(trackLabel);
        m_trackCombo = new QComboBox(this);
        m_trackCombo->addItem(tr("Automático (pelo papel)"), QString());
        m_trackCombo->addItem(tr("Sempre acompanhar"),       QStringLiteral("on"));
        m_trackCombo->addItem(tr("Nunca acompanhar"),        QStringLiteral("off"));
        outer->addWidget(m_trackCombo);
    }

    outer->addStretch();

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_cancelBtn = new QPushButton(tr("Cancelar"), this);
    m_cancelBtn->setObjectName(QStringLiteral("ecdBtn"));
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    m_okBtn = new QPushButton(tr("Criar"), this);
    m_okBtn->setObjectName(QStringLiteral("ecdBtn"));
    m_okBtn->setDefault(true);
    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_okBtn);
    outer->addLayout(btnRow);

    setStyleSheet(QStringLiteral(R"(
        QDialog#elementCreateDialog { background: %1; }
        QDialog#elementCreateDialog QLabel { color: %2; font-size: 12px; }
        QDialog#elementCreateDialog QLineEdit, QDialog#elementCreateDialog QComboBox {
            background: %3; color: %4;
            border: 1px solid %5; border-radius: 4px;
            padding: 6px 8px; min-height: 24px;
        }
        QDialog#elementCreateDialog QComboBox::drop-down { border: none; width: 16px; }
        QDialog#elementCreateDialog QComboBox QAbstractItemView {
            background: %3; color: %4;
            selection-background-color: %6;
            border: 1px solid %5;
        }
        QLabel#ecdImagePreview {
            background: %3; color: %7;
            border: 1px dashed %5; border-radius: 4px;
            font-size: 10px;
        }
        QPushButton#ecdBtn {
            background: %8; color: %2;
            border: 1px solid %5; border-radius: 4px;
            padding: 6px 14px; min-height: 26px;
        }
        QPushButton#ecdBtn:hover { background: %9; color: %4; }
        QPushButton#ecdBtn:default { background: %6; color: %4; border-color: %10; }
        QCheckBox#ecdNarratorCheck { color: %2; font-size: 12px; spacing: 6px; }
        QCheckBox#ecdNarratorCheck::indicator { width: 14px; height: 14px; border: 1px solid %5; border-radius: 3px; background: %3; }
        QCheckBox#ecdNarratorCheck::indicator:checked { background: %6; border-color: %10; }
    )").arg(Theme::panelBackground(),     // 1
           Theme::textPrimary(),          // 2
           Theme::inputBackground(),      // 3
           Theme::textBright(),           // 4
           Theme::panelBorder(),          // 5
           Theme::accentInfoSoft(),       // 6 (selection bg)
           Theme::textMuted(),            // 7 (image placeholder text)
           Theme::hoverOverlay(),         // 8 (button bg)
           Theme::hoverStrong(),          // 9 (button hover bg)
           Theme::accentInfoBorderSoft()  // 10 (default button border)
        ));
}

void ElementCreateDialog::setInitial(const QString& title, const QString& role, const QString& imageDataUrl, bool narrator, const QString& trackMode, const QStringList& aliases)
{
    if (m_titleEdit) m_titleEdit->setText(title);
    if (m_aliasesEdit) m_aliasesEdit->setText(aliases.join(QStringLiteral(", ")));
    if (m_roleCombo) {
        const int idx = m_roleCombo->findData(role);
        if (idx >= 0) m_roleCombo->setCurrentIndex(idx);
        else m_roleCombo->setEditText(role);
    }
    if (m_narratorCheck) m_narratorCheck->setChecked(narrator);
    if (m_trackCombo) {
        const int idx = m_trackCombo->findData(trackMode);
        m_trackCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    m_imageDataUrl = imageDataUrl;
    updatePreview();
    if (m_okBtn) m_okBtn->setText(tr("Salvar"));
    setWindowTitle(tr("Editar elemento"));
}

void ElementCreateDialog::updatePreview()
{
    if (!m_imagePreview) return;
    if (m_imageDataUrl.isEmpty()) {
        m_imagePreview->setPixmap(QPixmap());
        m_imagePreview->setText(tr("sem foto"));
        return;
    }
    QPixmap pm = pixmapFromDataUrl(m_imageDataUrl);
    if (pm.isNull()) { m_imagePreview->setText(tr("erro")); return; }
    m_imagePreview->setPixmap(pm.scaled(96, 96, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
}

void ElementCreateDialog::pickImage()
{
    const QString path = QFileDialog::getOpenFileName(this,
        tr("Selecionar foto"),
        QString(),
        tr("Imagens (*.png *.jpg *.jpeg *.gif *.bmp *.webp)"));
    if (path.isEmpty()) return;
    const QString dataUrl = loadImageAsDataUrl(path);
    if (dataUrl.isEmpty()) return;
    m_imageDataUrl = dataUrl;
    updatePreview();
}

void ElementCreateDialog::clearImage()
{
    m_imageDataUrl.clear();
    updatePreview();
}

QString ElementCreateDialog::title() const
{
    return m_titleEdit ? m_titleEdit->text().trimmed() : QString();
}

QString ElementCreateDialog::role() const
{
    if (!m_roleCombo) return QString();
    const QString data = m_roleCombo->currentData().toString();
    if (!data.isEmpty()) return data;
    return m_roleCombo->currentText().trimmed().toUpper();
}

bool ElementCreateDialog::narrator() const
{
    return m_narratorCheck ? m_narratorCheck->isChecked() : false;
}

QString ElementCreateDialog::trackMode() const
{
    return m_trackCombo ? m_trackCombo->currentData().toString() : QString();
}

QStringList ElementCreateDialog::aliases() const
{
    if (!m_aliasesEdit) return QStringList();
    QStringList out;
    const QStringList parts = m_aliasesEdit->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        const QString t = p.trimmed();
        if (!t.isEmpty()) out.append(t);
    }
    return out;
}
