#include "CharacterSheetPanel.h"

#include "ElementsStore.h"
#include "Theme.h"

#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QByteArray>
#include <QEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// Carrega imagem do disco como data URL JPEG quadrado 400×400 (crop central).
QString loadImageAsDataUrl(const QString& path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) return QString();
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

QToolButton* makeFieldBtn(const QString& glyph, const QString& tip) {
    auto* b = new QToolButton;
    b->setText(glyph);
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setObjectName(QStringLiteral("sheetFieldBtn"));
    return b;
}

} // namespace

CharacterSheetPanel::CharacterSheetPanel(ProjectModel* model, ElementsStore* elements, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_elements(elements)
{
    setObjectName(QStringLiteral("characterSheetPanel"));

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(600);
    connect(m_saveTimer, &QTimer::timeout, this, &CharacterSheetPanel::commitNow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setObjectName(QStringLiteral("sheetScroll"));
    outer->addWidget(m_scroll);

    setStyleSheet(QStringLiteral(
        "#characterSheetPanel, #sheetScroll, #sheetContent { background: %1; }"
        "#sheetScroll { border: none; }"
        "QLabel#sheetName  { font-size: 26px; font-weight: 700; color: %2; }"
        "QLabel#sheetAlias { font-size: 14px; font-style: italic; color: %3; }"
        "QLabel#sheetPhoto { background: %4; border: 1px solid %5; border-radius: 10px; color: %3; }"
        "QLineEdit#sheetLabel { border: none; background: transparent; font-weight: 700; "
        "  color: %3; padding: 2px 0; }"
        "QLineEdit#sheetData { border: none; border-bottom: 1px solid transparent; "
        "  background: transparent; color: %2; padding: 4px 2px; }"
        "QLineEdit#sheetData:hover { border-bottom: 1px solid %5; }"
        "QLineEdit#sheetData:focus { border-bottom: 1px solid %6; background: %7; }"
        "QTextEdit#sheetText { border: none; background: transparent; color: %2; padding: 2px; }"
        "QTextEdit#sheetText:hover, QTextEdit#sheetText:focus { background: %7; "
        "  border-radius: 6px; }"
        "QToolButton#sheetFieldBtn { border: none; color: %3; font-size: 14px; "
        "  padding: 0 4px; }"
        "QToolButton#sheetFieldBtn:hover { color: %2; }"
        "QPushButton#sheetGhostBtn { border: 1px solid %5; border-radius: 6px; "
        "  color: %3; padding: 5px 12px; background: transparent; }"
        "QPushButton#sheetGhostBtn:hover { color: %2; border-color: %6; background: %7; }"
    ).arg(Theme::appBackground(), Theme::textPrimary(), Theme::textMuted(),
          Theme::inputBackground(), Theme::subtleBorder(), Theme::focusBorder(),
          Theme::hoverOverlay()));
}

void CharacterSheetPanel::openItem(const QString& itemId)
{
    const DrawerItem* item = m_model ? m_model->findDrawerItem(itemId) : nullptr;
    if (!item || !item->isSheet) return;
    m_itemId = itemId;
    m_elementId = item->elementId;
    m_sheet = item->sheet;
    rebuild();
}

void CharacterSheetPanel::scheduleSave()
{
    if (m_saveTimer) m_saveTimer->start();
}

void CharacterSheetPanel::commitNow()
{
    if (m_itemId.isEmpty() || !m_model) return;
    m_model->updateDrawerItemSheet(m_itemId, m_sheet);
    emit edited();
}

void CharacterSheetPanel::setFieldValue(const QString& id, const QString& value)
{
    for (auto& f : m_sheet.fields) {
        if (f.id == id) { f.value = value; return; }
    }
}

void CharacterSheetPanel::addField(const QString& kind)
{
    SheetField f;
    f.id = ProjectModel::uid();
    f.kind = kind;
    f.label = (kind == QStringLiteral("text")) ? tr("Novo bloco") : tr("Novo dado");
    f.column = (m_sheet.columns == 2)
        ? (kind == QStringLiteral("text") ? QStringLiteral("right") : QStringLiteral("left"))
        : QStringLiteral("left");
    f.order = m_sheet.fields.size();
    m_sheet.fields.append(f);
    commitNow();
    rebuild();
}

void CharacterSheetPanel::removeField(const QString& id)
{
    for (int i = 0; i < m_sheet.fields.size(); ++i) {
        if (m_sheet.fields[i].id == id) { m_sheet.fields.removeAt(i); break; }
    }
    commitNow();
    rebuild();
}

void CharacterSheetPanel::moveFieldColumn(const QString& id)
{
    for (auto& f : m_sheet.fields) {
        if (f.id == id) {
            f.column = (f.column == QStringLiteral("left"))
                ? QStringLiteral("right") : QStringLiteral("left");
            break;
        }
    }
    commitNow();
    rebuild();
}

void CharacterSheetPanel::toggleColumns()
{
    m_sheet.columns = (m_sheet.columns == 2) ? 1 : 2;
    commitNow();
    rebuild();
}

void CharacterSheetPanel::pickPhoto()
{
    if (m_elementId.isEmpty() || !m_elements) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Selecionar foto"), QString(),
        tr("Imagens (*.png *.jpg *.jpeg *.gif *.bmp *.webp)"));
    if (path.isEmpty()) return;
    const QString dataUrl = loadImageAsDataUrl(path);
    if (dataUrl.isEmpty()) return;
    if (const Element* e = m_elements->findElement(m_elementId)) {
        Element copy = *e;
        copy.image = dataUrl;
        m_elements->updateElement(m_elementId, copy);
    }
    refreshPhoto();
    scheduleSave();
}

void CharacterSheetPanel::refreshPhoto()
{
    if (!m_photo) return;
    const Element* e = (!m_elementId.isEmpty() && m_elements)
        ? m_elements->findElement(m_elementId) : nullptr;
    if (e && !e->image.isEmpty()) {
        QPixmap pm = pixmapFromDataUrl(e->image);
        if (!pm.isNull()) {
            m_photo->setText(QString());
            m_photo->setPixmap(pm.scaled(m_photo->size(),
                Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            return;
        }
    }
    m_photo->setPixmap(QPixmap());
    m_photo->setText(tr("+ foto"));
}

QWidget* CharacterSheetPanel::buildColumn(const QString& side)
{
    auto* col = new QWidget;
    auto* lay = new QVBoxLayout(col);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(18);
    for (const auto& f : m_sheet.fields) {
        if (!side.isEmpty() && f.column != side) continue;
        lay->addWidget(buildFieldWidget(f));
    }
    lay->addStretch();
    return col;
}

QWidget* CharacterSheetPanel::buildFieldWidget(const SheetField& f)
{
    const QString id = f.id;
    auto* box = new QWidget;
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);

    // Linha do rótulo + controles (mover coluna / remover)
    auto* head = new QHBoxLayout;
    head->setSpacing(2);
    auto* label = new QLineEdit(f.label);
    label->setObjectName(QStringLiteral("sheetLabel"));
    connect(label, &QLineEdit::editingFinished, this, [this, id, label]() {
        for (auto& ff : m_sheet.fields) if (ff.id == id) { ff.label = label->text(); break; }
        scheduleSave();
    });
    head->addWidget(label, 1);

    if (m_sheet.columns == 2) {
        auto* moveBtn = makeFieldBtn(f.column == QStringLiteral("left")
            ? QStringLiteral("→") : QStringLiteral("←"),
            tr("Mover de coluna"));
        connect(moveBtn, &QToolButton::clicked, this, [this, id]() { moveFieldColumn(id); });
        head->addWidget(moveBtn);
    }
    auto* delBtn = makeFieldBtn(QStringLiteral("✕"), tr("Remover campo"));
    connect(delBtn, &QToolButton::clicked, this, [this, id]() { removeField(id); });
    head->addWidget(delBtn);
    v->addLayout(head);

    // Valor
    if (f.kind == QStringLiteral("text")) {
        auto* te = new QTextEdit;
        te->setObjectName(QStringLiteral("sheetText"));
        te->setLineWrapMode(QTextEdit::WidgetWidth);
        te->setHtml(f.value);
        te->setPlaceholderText(tr("Escreva aqui…"));
        te->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        te->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        // Auto-altura: acompanha o tamanho do documento (inclui quebra por largura).
        auto* doc = te->document();
        connect(doc->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
                te, [te](const QSizeF& s) {
            const int h = int(s.height()) + 14;
            te->setFixedHeight(qMax(44, h));
        });
        connect(te, &QTextEdit::textChanged, this, [this, id, te]() {
            setFieldValue(id, te->toHtml());
            scheduleSave();
        });
        v->addWidget(te);
    } else {
        auto* le = new QLineEdit(f.value);
        le->setObjectName(QStringLiteral("sheetData"));
        le->setPlaceholderText(QStringLiteral("—"));
        connect(le, &QLineEdit::editingFinished, this, [this, id, le]() {
            setFieldValue(id, le->text());
            scheduleSave();
        });
        v->addWidget(le);
    }
    return box;
}

void CharacterSheetPanel::rebuild()
{
    m_content = new QWidget;
    m_content->setObjectName(QStringLiteral("sheetContent"));
    auto* root = new QVBoxLayout(m_content);
    root->setContentsMargins(48, 28, 48, 40);
    root->setSpacing(22);

    // Barra superior: alternar 1/2 colunas.
    auto* topBar = new QHBoxLayout;
    topBar->addStretch();
    auto* colBtn = new QPushButton(m_sheet.columns == 2 ? tr("1 coluna") : tr("2 colunas"));
    colBtn->setObjectName(QStringLiteral("sheetGhostBtn"));
    colBtn->setCursor(Qt::PointingHandCursor);
    connect(colBtn, &QPushButton::clicked, this, &CharacterSheetPanel::toggleColumns);
    topBar->addWidget(colBtn);
    root->addLayout(topBar);

    // Cabeçalho: foto + nome + apelido (do Element vinculado).
    const Element* e = (!m_elementId.isEmpty() && m_elements)
        ? m_elements->findElement(m_elementId) : nullptr;
    auto* header = new QHBoxLayout;
    header->setSpacing(22);
    m_photo = new QLabel;
    m_photo->setObjectName(QStringLiteral("sheetPhoto"));
    m_photo->setFixedSize(132, 132);
    m_photo->setAlignment(Qt::AlignCenter);
    m_photo->setCursor(Qt::PointingHandCursor);
    m_photo->installEventFilter(this);
    refreshPhoto();
    header->addWidget(m_photo, 0, Qt::AlignTop);

    auto* nameCol = new QVBoxLayout;
    nameCol->setSpacing(4);
    nameCol->addSpacing(8);
    auto* nameLbl = new QLabel(e ? e->name : tr("Personagem"));
    nameLbl->setObjectName(QStringLiteral("sheetName"));
    nameCol->addWidget(nameLbl);
    if (e && !e->aliases.isEmpty()) {
        auto* aliasLbl = new QLabel(e->aliases.join(QStringLiteral(", ")));
        aliasLbl->setObjectName(QStringLiteral("sheetAlias"));
        nameCol->addWidget(aliasLbl);
    }
    nameCol->addStretch();
    header->addLayout(nameCol, 1);
    root->addLayout(header);

    // Corpo: 1 ou 2 colunas.
    if (m_sheet.columns == 2) {
        auto* cols = new QHBoxLayout;
        cols->setSpacing(40);
        cols->addWidget(buildColumn(QStringLiteral("left")), 1);
        cols->addWidget(buildColumn(QStringLiteral("right")), 1);
        root->addLayout(cols);
    } else {
        root->addWidget(buildColumn(QString()));
    }

    // Rodapé: adicionar campos.
    auto* addBar = new QHBoxLayout;
    auto* addData = new QPushButton(tr("+ Dado"));
    auto* addText = new QPushButton(tr("+ Texto"));
    addData->setObjectName(QStringLiteral("sheetGhostBtn"));
    addText->setObjectName(QStringLiteral("sheetGhostBtn"));
    addData->setCursor(Qt::PointingHandCursor);
    addText->setCursor(Qt::PointingHandCursor);
    connect(addData, &QPushButton::clicked, this, [this]() { addField(QStringLiteral("data")); });
    connect(addText, &QPushButton::clicked, this, [this]() { addField(QStringLiteral("text")); });
    addBar->addWidget(addData);
    addBar->addWidget(addText);
    addBar->addStretch();
    root->addLayout(addBar);
    root->addStretch();

    m_scroll->setWidget(m_content);  // substitui e destrói o conteúdo anterior
}

bool CharacterSheetPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_photo && event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) { pickPhoto(); return true; }
    }
    return QWidget::eventFilter(watched, event);
}
