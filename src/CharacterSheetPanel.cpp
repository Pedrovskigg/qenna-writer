#include "CharacterSheetPanel.h"

#include "EditorLayout.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "ImageCropDialog.h"
#include "SheetTemplatesStore.h"
#include "Theme.h"

#include <QAbstractTextDocumentLayout>
#include <QByteArray>
#include <QCursor>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kPhotoW = 210;
constexpr int kPhotoH = 280;   // 3:4 (retrato)

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
    outer->setSpacing(0);
    buildFindBar();
    outer->addWidget(m_findBar);
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
        "  font-size: 15px; text-decoration: underline; color: %2; padding: 0; }"
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
        "#sheetFindBar { background: %4; border-bottom: 1px solid %5; }"
        "QLineEdit#sheetFindInput { background: %1; border: 1px solid %5; border-radius: 4px; "
        "  color: %2; padding: 4px 8px; }"
        "QLabel#sheetFindCount { color: %3; font-size: 12px; }"
    ).arg(Theme::editorBackground(), Theme::editorTextColor(), Theme::textMuted(),
          Theme::inputBackground(), Theme::subtleBorder(), Theme::hoverOverlay(),
          Theme::appBackground()));
}

void CharacterSheetPanel::buildFindBar()
{
    m_findBar = new QWidget(this);
    m_findBar->setObjectName(QStringLiteral("sheetFindBar"));
    auto* h = new QHBoxLayout(m_findBar);
    h->setContentsMargins(12, 6, 12, 6);
    h->setSpacing(6);

    m_findInput = new QLineEdit;
    m_findInput->setObjectName(QStringLiteral("sheetFindInput"));
    m_findInput->setPlaceholderText(tr("Buscar na ficha…"));
    m_findInput->setClearButtonEnabled(true);
    m_findInput->installEventFilter(this);   // Esc fecha
    connect(m_findInput, &QLineEdit::textChanged, this, &CharacterSheetPanel::onFindChanged);
    connect(m_findInput, &QLineEdit::returnPressed, this, &CharacterSheetPanel::findNext);
    h->addWidget(m_findInput, 1);

    m_findCount = new QLabel(QStringLiteral("0/0"));
    m_findCount->setObjectName(QStringLiteral("sheetFindCount"));
    h->addWidget(m_findCount);

    auto mkBtn = [this](const QString& glyph, void (CharacterSheetPanel::*slot)()) {
        auto* b = new QToolButton;
        b->setText(glyph);
        b->setObjectName(QStringLiteral("sheetFieldBtn"));
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, slot);
        return b;
    };
    h->addWidget(mkBtn(QStringLiteral("↑"), &CharacterSheetPanel::findPrev));
    h->addWidget(mkBtn(QStringLiteral("↓"), &CharacterSheetPanel::findNext));
    h->addWidget(mkBtn(QStringLiteral("✕"), &CharacterSheetPanel::closeFind));

    m_findBar->setVisible(false);
}

void CharacterSheetPanel::openFind()
{
    if (!m_findBar) return;
    m_findBar->setVisible(true);
    m_findInput->setFocus();
    m_findInput->selectAll();
    onFindChanged(m_findInput->text());
}

void CharacterSheetPanel::onFindChanged(const QString& q)
{
    m_matches.clear();
    m_activeMatch = -1;
    if (!q.isEmpty()) {
        for (QWidget* w : m_searchFields) {
            QString text;
            if (auto* te = qobject_cast<QTextEdit*>(w)) text = te->toPlainText();
            else if (auto* le = qobject_cast<QLineEdit*>(w)) text = le->text();
            int from = 0;
            while (true) {
                const int idx = text.indexOf(q, from, Qt::CaseInsensitive);
                if (idx < 0) break;
                m_matches.append({ w, idx, int(q.size()) });
                from = idx + q.size();
            }
        }
    }
    applyFindHighlights();
    if (!m_matches.isEmpty()) gotoMatch(0);
    else if (m_findCount) m_findCount->setText(QStringLiteral("0/0"));
}

void CharacterSheetPanel::applyFindHighlights()
{
    QColor hl(Theme::accentWarning());
    if (!hl.isValid()) hl = QColor(255, 213, 79);
    hl.setAlpha(110);
    for (QTextEdit* te : m_textEdits) {
        QList<QTextEdit::ExtraSelection> sels;
        for (const FindMatch& m : m_matches) {
            if (m.w != te) continue;
            QTextCursor c(te->document());
            c.setPosition(m.pos);
            c.setPosition(m.pos + m.len, QTextCursor::KeepAnchor);
            QTextEdit::ExtraSelection es;
            es.cursor = c;
            es.format.setBackground(hl);
            sels.append(es);
        }
        te->setExtraSelections(sels);
    }
}

void CharacterSheetPanel::gotoMatch(int idx)
{
    if (idx < 0 || idx >= m_matches.size()) return;
    m_activeMatch = idx;
    const FindMatch& m = m_matches[idx];
    if (auto* te = qobject_cast<QTextEdit*>(m.w)) {
        QTextCursor c(te->document());
        c.setPosition(m.pos);
        c.setPosition(m.pos + m.len, QTextCursor::KeepAnchor);
        te->setTextCursor(c);
    } else if (auto* le = qobject_cast<QLineEdit*>(m.w)) {
        le->setSelection(m.pos, m.len);
    }
    if (m_scroll && m.w) m_scroll->ensureWidgetVisible(m.w);
    if (m_findCount)
        m_findCount->setText(QStringLiteral("%1/%2").arg(idx + 1).arg(m_matches.size()));
}

void CharacterSheetPanel::findNext()
{
    if (m_matches.isEmpty()) return;
    gotoMatch((m_activeMatch + 1) % m_matches.size());
}

void CharacterSheetPanel::findPrev()
{
    if (m_matches.isEmpty()) return;
    gotoMatch((m_activeMatch - 1 + m_matches.size()) % m_matches.size());
}

void CharacterSheetPanel::closeFind()
{
    if (m_findBar) m_findBar->setVisible(false);
    m_matches.clear();
    m_activeMatch = -1;
    for (QTextEdit* te : m_textEdits) te->setExtraSelections({});
    setFocus();
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

void CharacterSheetPanel::saveAsTemplate()
{
    if (!m_templates) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Salvar como modelo"),
        tr("Nome do modelo:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    m_templates->add(name, m_sheet);
}

void CharacterSheetPanel::pickPhoto()
{
    if (m_elementId.isEmpty() || !m_elements) return;
    const QString dataUrl = ImageCropDialog::pickAndCropImage(this);
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
        // Força a fonte/tamanho atuais em TODO o conteúdo, sobrescrevendo o size
        // que o QTextEdit embute no html — assim o texto herda a fonte do editor.
        {
            QTextCursor c(te->document());
            c.select(QTextCursor::Document);
            QTextCharFormat cf;
            cf.setFontFamilies({m_contentFont.family()});
            if (m_contentFont.pointSizeF() > 0) cf.setFontPointSize(m_contentFont.pointSizeF());
            c.mergeCharFormat(cf);
            te->setCurrentCharFormat(cf);  // novo texto digitado também herda
        }
        te->setPlaceholderText(tr("Escreva aqui…"));
        te->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        te->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* doc = te->document();
        doc->setDocumentMargin(0);
        // Nasce alto (há espaço de sobra na página) e cresce conforme o texto.
        connect(doc->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
                te, [te](const QSizeF& s) {
            te->setFixedHeight(qMax(96, int(s.height()) + 8));
        });
        connect(te, &QTextEdit::textChanged, this, [this, id, te]() {
            setFieldValue(id, te->toHtml());
            scheduleSave();
        });
        m_textEdits.append(te);       // recebe highlight de busca
        m_searchFields.append(te);    // buscável (Ctrl+F)
        v->addWidget(te);
    } else {
        auto* le = new QLineEdit(f.value);
        le->setObjectName(QStringLiteral("sheetData"));
        le->setFont(m_contentFont);  // pegada de escrita (Alegreya)
        le->setPlaceholderText(QStringLiteral("—"));
        m_searchFields.append(le);    // buscável (Ctrl+F)
        connect(le, &QLineEdit::editingFinished, this, [this, id, le]() {
            setFieldValue(id, le->text());
            scheduleSave();
        });
        v->addWidget(le);
    }
    return box;
}

QWidget* CharacterSheetPanel::buildHeader(bool vertical)
{
    const Element* e = (!m_elementId.isEmpty() && m_elements)
        ? m_elements->findElement(m_elementId) : nullptr;

    m_photo = new QLabel;
    m_photo->setObjectName(QStringLiteral("sheetPhoto"));
    m_photo->setFixedSize(kPhotoW, kPhotoH);
    m_photo->setAlignment(Qt::AlignCenter);
    m_photo->setCursor(Qt::PointingHandCursor);
    m_photo->installEventFilter(this);
    refreshPhoto();

    auto* nameLbl = new QLabel(e ? e->name : tr("Personagem"));
    nameLbl->setObjectName(QStringLiteral("sheetName"));
    nameLbl->setWordWrap(true);
    QLabel* aliasLbl = nullptr;
    if (e && !e->aliases.isEmpty()) {
        aliasLbl = new QLabel(e->aliases.join(QStringLiteral(", ")));
        aliasLbl->setObjectName(QStringLiteral("sheetAlias"));
        aliasLbl->setWordWrap(true);
    }

    auto* box = new QWidget;
    if (vertical) {
        // 2 colunas: foto em cima, nome/apelido abaixo (topo da coluna esquerda).
        auto* v = new QVBoxLayout(box);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(6);
        v->addWidget(m_photo, 0, Qt::AlignLeft);
        v->addSpacing(2);
        v->addWidget(nameLbl);
        if (aliasLbl) v->addWidget(aliasLbl);
    } else {
        // 1 coluna: foto à esquerda, nome/apelido ao lado.
        auto* h = new QHBoxLayout(box);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(18);
        h->addWidget(m_photo, 0, Qt::AlignTop);
        auto* nameCol = new QVBoxLayout;
        nameCol->setSpacing(2);
        nameCol->addSpacing(6);
        nameCol->addWidget(nameLbl);
        if (aliasLbl) nameCol->addWidget(aliasLbl);
        nameCol->addStretch();
        h->addLayout(nameCol, 1);
    }
    return box;
}

void CharacterSheetPanel::rebuild()
{
    // Os campos antigos serão destruídos ao trocar o widget do scroll — zera as
    // listas de busca (serão repovoadas em buildFieldWidget).
    m_searchFields.clear();
    m_textEdits.clear();
    m_matches.clear();
    m_activeMatch = -1;

    // Outer transparente que centraliza a "folha" (página) horizontalmente.
    auto* outer = new QWidget;
    outer->setObjectName(QStringLiteral("sheetOuter"));
    auto* outerLay = new QHBoxLayout(outer);
    outerLay->setContentsMargins(0, 20, 0, 20);

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
    const int hm = qMax(44, EditorLayout::horizontalMargin());
    const int vm = qMax(40, EditorLayout::verticalMargin());
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(hm, vm, hm, vm);
    root->setSpacing(24);

    // Barra superior: adicionar campos + alternar 1/2 colunas.
    auto* topBar = new QHBoxLayout;
    topBar->setSpacing(8);
    topBar->addStretch();
    auto* addData = new QPushButton(tr("＋ Dado"));
    auto* addText = new QPushButton(tr("＋ Texto"));
    addData->setObjectName(QStringLiteral("sheetGhostBtn"));
    addText->setObjectName(QStringLiteral("sheetGhostBtn"));
    addData->setCursor(Qt::PointingHandCursor);
    addText->setCursor(Qt::PointingHandCursor);
    connect(addData, &QPushButton::clicked, this, [this]() { addField(QStringLiteral("data")); });
    connect(addText, &QPushButton::clicked, this, [this]() { addField(QStringLiteral("text")); });
    topBar->addWidget(addData);
    topBar->addWidget(addText);
    auto* colBtn = new QToolButton;
    colBtn->setObjectName(QStringLiteral("sheetGhostTool"));
    colBtn->setText(m_sheet.columns == 2 ? tr("1 coluna") : tr("2 colunas"));
    colBtn->setCursor(Qt::PointingHandCursor);
    connect(colBtn, &QToolButton::clicked, this, &CharacterSheetPanel::toggleColumns);
    topBar->addWidget(colBtn);
    if (m_templates) {
        auto* saveTemplateBtn = new QToolButton;
        saveTemplateBtn->setObjectName(QStringLiteral("sheetGhostTool"));
        saveTemplateBtn->setText(tr("Salvar como modelo"));
        saveTemplateBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        const QSize iconSz(14, 14);
        saveTemplateBtn->setIcon(IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/save-project.svg"),
            QColor(Theme::textMuted()), QColor(Theme::editorTextColor()), QColor(Theme::editorTextColor()),
            iconSz));
        saveTemplateBtn->setIconSize(iconSz);
        saveTemplateBtn->setCursor(Qt::PointingHandCursor);
        connect(saveTemplateBtn, &QToolButton::clicked, this, &CharacterSheetPanel::saveAsTemplate);
        topBar->addWidget(saveTemplateBtn);
    }
    root->addLayout(topBar);

    if (m_sheet.columns == 2) {
        // Esquerda: foto + nome + dados. Direita: blocos de texto, subindo ao lado da foto.
        auto* cols = new QHBoxLayout;
        cols->setSpacing(38);

        auto* leftW = new QWidget;
        auto* leftV = new QVBoxLayout(leftW);
        leftV->setContentsMargins(0, 0, 0, 0);
        leftV->setSpacing(20);
        leftV->addWidget(buildHeader(/*vertical=*/true));
        for (const auto& f : m_sheet.fields)
            if (f.column == QStringLiteral("left")) leftV->addWidget(buildFieldWidget(f));
        leftV->addStretch();

        auto* rightW = new QWidget;
        auto* rightV = new QVBoxLayout(rightW);
        rightV->setContentsMargins(0, 0, 0, 0);
        rightV->setSpacing(20);
        for (const auto& f : m_sheet.fields)
            if (f.column == QStringLiteral("right")) rightV->addWidget(buildFieldWidget(f));
        rightV->addStretch();

        cols->addWidget(leftW, 4);
        cols->addWidget(rightW, 6);
        root->addLayout(cols);
    } else {
        root->addWidget(buildHeader(/*vertical=*/false));
        auto* colW = new QWidget;
        auto* colV = new QVBoxLayout(colW);
        colV->setContentsMargins(0, 0, 0, 0);
        colV->setSpacing(20);
        for (const auto& f : m_sheet.fields)
            colV->addWidget(buildFieldWidget(f));
        root->addWidget(colW);
    }

    root->addStretch();   // empurra conteúdo pro topo; a folha estica até o rodapé da página

    outerLay->addStretch();
    outerLay->addWidget(page);  // sem AlignTop: a folha preenche a altura da viewport
    outerLay->addStretch();

    m_content = outer;
    m_scroll->setWidget(m_content);  // substitui e destrói o conteúdo anterior

    // Se a busca estava aberta, recalcula sobre os novos campos.
    if (m_findBar && m_findBar->isVisible())
        onFindChanged(m_findInput->text());
}

bool CharacterSheetPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Esc fecha a busca local.
    if (watched == m_findInput && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) { closeFind(); return true; }
    }
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
