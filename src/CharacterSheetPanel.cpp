#include "CharacterSheetPanel.h"

#include "EditorLayout.h"
#include "ElementsStore.h"
#include "Theme.h"

#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QByteArray>
#include <QCursor>
#include <QEvent>
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
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

constexpr int kPhotoW = 150;
constexpr int kPhotoH = 188;

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
    b->setVisible(false);   // hover-reveal
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
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->viewport()->setAutoFillBackground(false);
    outer->addWidget(m_scroll);

    setStyleSheet(QStringLiteral(
        "#characterSheetPanel { background: %7; }"
        "#sheetScroll, #sheetOuter { background: transparent; border: none; }"
        "#sheetPage { background: %1; }"
        "QLabel#sheetName  { font-size: 22px; font-weight: 700; color: %2; }"
        "QLabel#sheetAlias { font-size: 13px; font-style: italic; color: %3; }"
        "QLabel#sheetPhoto { background: %4; border: 1px solid %5; border-radius: 6px; color: %3; }"
        "QLineEdit#sheetLabel { border: none; background: transparent; font-weight: 700; "
        "  color: %2; padding: 0; }"
        "QLineEdit#sheetData { border: none; background: transparent; color: %2; padding: 1px 0; }"
        "QLineEdit#sheetData:hover, QLineEdit#sheetData:focus { background: %6; border-radius: 4px; }"
        "QTextEdit#sheetText { border: none; background: transparent; color: %2; padding: 0; }"
        "QTextEdit#sheetText:hover, QTextEdit#sheetText:focus { background: %6; border-radius: 4px; }"
        "QToolButton#sheetFieldBtn { border: none; color: %3; font-size: 13px; padding: 0 3px; }"
        "QToolButton#sheetFieldBtn:hover { color: %2; }"
        "QToolButton#sheetGhostTool { border: none; color: %3; font-size: 12px; padding: 2px 6px; }"
        "QToolButton#sheetGhostTool:hover { color: %2; }"
        "QPushButton#sheetGhostBtn { border: 1px dashed %5; border-radius: 5px; "
        "  color: %3; padding: 4px 12px; background: transparent; }"
        "QPushButton#sheetGhostBtn:hover { color: %2; border-color: %2; }"
    ).arg(Theme::editorBackground(), Theme::editorTextColor(), Theme::textMuted(),
          Theme::inputBackground(), Theme::subtleBorder(), Theme::hoverOverlay(),
          Theme::appBackground()));
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

void CharacterSheetPanel::setContentFont(const QFont& f)
{
    if (m_contentFont == f) return;
    m_contentFont = f;
    if (!m_itemId.isEmpty()) rebuild();
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
            QPixmap scaled = pm.scaled(m_photo->size(),
                Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            // Crop central pra preencher o retrato sem distorcer.
            const int dx = (scaled.width() - m_photo->width()) / 2;
            const int dy = (scaled.height() - m_photo->height()) / 2;
            m_photo->setPixmap(scaled.copy(dx, dy, m_photo->width(), m_photo->height()));
            return;
        }
    }
    m_photo->setPixmap(QPixmap());
    m_photo->setText(tr("＋ foto"));
}

QWidget* CharacterSheetPanel::buildColumn(const QString& side)
{
    auto* col = new QWidget;
    auto* lay = new QVBoxLayout(col);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(11);
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
    box->setProperty("fieldBox", true);
    box->installEventFilter(this);
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(1);

    // Linha do rótulo + controles (hover-reveal): mover coluna / remover.
    auto* head = new QHBoxLayout;
    head->setSpacing(2);
    auto* label = new QLineEdit(f.label);
    label->setObjectName(QStringLiteral("sheetLabel"));
    label->setCursorPosition(0);
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
        te->document()->setDefaultFont(m_contentFont);  // pegada de escrita (Alegreya)
        te->setHtml(f.value);
        te->setPlaceholderText(tr("Escreva aqui…"));
        te->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        te->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* doc = te->document();
        doc->setDocumentMargin(0);
        connect(doc->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
                te, [te](const QSizeF& s) {
            te->setFixedHeight(qMax(24, int(s.height()) + 6));
        });
        connect(te, &QTextEdit::textChanged, this, [this, id, te]() {
            setFieldValue(id, te->toHtml());
            scheduleSave();
        });
        v->addWidget(te);
    } else {
        auto* le = new QLineEdit(f.value);
        le->setObjectName(QStringLiteral("sheetData"));
        le->setFont(m_contentFont);  // pegada de escrita (Alegreya)
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
    // Outer transparente que centraliza a "folha" (página) horizontalmente.
    auto* outer = new QWidget;
    outer->setObjectName(QStringLiteral("sheetOuter"));
    auto* outerLay = new QHBoxLayout(outer);
    outerLay->setContentsMargins(0, 16, 0, 24);

    // A folha: mesma largura da página do editor, cor de página, sombra.
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("sheetPage"));
    page->setFixedWidth(EditorLayout::pageWidth());
    if (Theme::pageShadowEnabled()) {
        auto* shadow = new QGraphicsDropShadowEffect(page);
        shadow->setBlurRadius(Theme::pageShadowRadius());
        shadow->setOffset(0, Theme::pageShadowOffset());
        shadow->setColor(QColor(0, 0, 0, 140));
        page->setGraphicsEffect(shadow);
    }
    const int hm = qMax(28, EditorLayout::horizontalMargin());
    const int vm = qMax(28, EditorLayout::verticalMargin());
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(hm, vm, hm, vm);
    root->setSpacing(14);

    // Barra superior discreta: alternar 1/2 colunas.
    auto* topBar = new QHBoxLayout;
    topBar->addStretch();
    auto* colBtn = new QToolButton;
    colBtn->setObjectName(QStringLiteral("sheetGhostTool"));
    colBtn->setText(m_sheet.columns == 2 ? tr("1 coluna") : tr("2 colunas"));
    colBtn->setCursor(Qt::PointingHandCursor);
    connect(colBtn, &QToolButton::clicked, this, &CharacterSheetPanel::toggleColumns);
    topBar->addWidget(colBtn);
    root->addLayout(topBar);

    // Cabeçalho: foto grande + nome + apelido (do Element vinculado).
    const Element* e = (!m_elementId.isEmpty() && m_elements)
        ? m_elements->findElement(m_elementId) : nullptr;
    auto* header = new QHBoxLayout;
    header->setSpacing(16);
    m_photo = new QLabel;
    m_photo->setObjectName(QStringLiteral("sheetPhoto"));
    m_photo->setFixedSize(kPhotoW, kPhotoH);
    m_photo->setAlignment(Qt::AlignCenter);
    m_photo->setCursor(Qt::PointingHandCursor);
    m_photo->installEventFilter(this);
    refreshPhoto();
    header->addWidget(m_photo, 0, Qt::AlignTop);

    auto* nameCol = new QVBoxLayout;
    nameCol->setSpacing(2);
    auto* nameLbl = new QLabel(e ? e->name : tr("Personagem"));
    nameLbl->setObjectName(QStringLiteral("sheetName"));
    nameLbl->setWordWrap(true);
    nameCol->addWidget(nameLbl);
    if (e && !e->aliases.isEmpty()) {
        auto* aliasLbl = new QLabel(e->aliases.join(QStringLiteral(", ")));
        aliasLbl->setObjectName(QStringLiteral("sheetAlias"));
        aliasLbl->setWordWrap(true);
        nameCol->addWidget(aliasLbl);
    }
    nameCol->addStretch();
    header->addLayout(nameCol, 1);
    root->addLayout(header);

    // Corpo: 1 ou 2 colunas (próximas).
    if (m_sheet.columns == 2) {
        auto* cols = new QHBoxLayout;
        cols->setSpacing(26);
        cols->addWidget(buildColumn(QStringLiteral("left")), 1);
        cols->addWidget(buildColumn(QStringLiteral("right")), 1);
        root->addLayout(cols);
    } else {
        root->addWidget(buildColumn(QString()));
    }

    // Rodapé: adicionar campos (discreto).
    auto* addBar = new QHBoxLayout;
    auto* addData = new QPushButton(tr("＋ Dado"));
    auto* addText = new QPushButton(tr("＋ Texto"));
    addData->setObjectName(QStringLiteral("sheetGhostBtn"));
    addText->setObjectName(QStringLiteral("sheetGhostBtn"));
    addData->setCursor(Qt::PointingHandCursor);
    addText->setCursor(Qt::PointingHandCursor);
    connect(addData, &QPushButton::clicked, this, [this]() { addField(QStringLiteral("data")); });
    connect(addText, &QPushButton::clicked, this, [this]() { addField(QStringLiteral("text")); });
    addBar->addWidget(addData);
    addBar->addWidget(addText);
    addBar->addStretch();
    root->addSpacing(6);
    root->addLayout(addBar);

    outerLay->addStretch();
    outerLay->addWidget(page, 0, Qt::AlignTop);
    outerLay->addStretch();

    m_content = outer;
    m_scroll->setWidget(m_content);  // substitui e destrói o conteúdo anterior
}

bool CharacterSheetPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_photo && event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) { pickPhoto(); return true; }
        return QWidget::eventFilter(watched, event);
    }
    // Hover-reveal dos botões de cada campo.
    auto* box = qobject_cast<QWidget*>(watched);
    if (box && box->property("fieldBox").toBool()) {
        if (event->type() == QEvent::Enter) {
            for (auto* b : box->findChildren<QToolButton*>()) b->setVisible(true);
        } else if (event->type() == QEvent::Leave) {
            // Ignora "leave" causado por entrar num filho (lineedit/textedit).
            if (!box->rect().contains(box->mapFromGlobal(QCursor::pos())))
                for (auto* b : box->findChildren<QToolButton*>()) b->setVisible(false);
        }
    }
    return QWidget::eventFilter(watched, event);
}
