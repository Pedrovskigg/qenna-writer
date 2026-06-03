#include "ExportPanel.h"

#include "ProjectModel.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

ExportPanel::ExportPanel(ProjectModel* model, QWidget* parent)
    : QDialog(parent), m_model(model) {
    setObjectName(QStringLiteral("exportPanel"));
    setWindowTitle(QStringLiteral("Exportar projeto"));
    setModal(true);
    resize(480, 620);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ──
    auto* header = new QWidget(this);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(18, 14, 12, 14);
    auto* title = new QLabel(QStringLiteral("Exportar projeto"), header);
    title->setObjectName(QStringLiteral("exportTitle"));
    auto* closeBtn = new QPushButton(QStringLiteral("✕"), header);
    closeBtn->setObjectName(QStringLiteral("exportClose"));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFixedSize(26, 26);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    hl->addWidget(title);
    hl->addStretch();
    hl->addWidget(closeBtn);
    root->addWidget(header);

    // ── Barra selecionar/desmarcar tudo ──
    auto* selBar = new QWidget(this);
    selBar->setObjectName(QStringLiteral("exportSelBar"));
    auto* sl = new QHBoxLayout(selBar);
    sl->setContentsMargins(18, 7, 18, 7);
    sl->setSpacing(8);
    auto* selectAll = new QPushButton(QStringLiteral("Selecionar tudo"), selBar);
    auto* deselectAll = new QPushButton(QStringLiteral("Desmarcar tudo"), selBar);
    selectAll->setObjectName(QStringLiteral("exportLink"));
    deselectAll->setObjectName(QStringLiteral("exportLink"));
    selectAll->setCursor(Qt::PointingHandCursor);
    deselectAll->setCursor(Qt::PointingHandCursor);
    auto* dot = new QLabel(QStringLiteral("·"), selBar);
    dot->setObjectName(QStringLiteral("exportDot"));
    m_countLabel = new QLabel(selBar);
    m_countLabel->setObjectName(QStringLiteral("exportCount"));
    connect(selectAll, &QPushButton::clicked, this, [this]() { setAllChecked(true); });
    connect(deselectAll, &QPushButton::clicked, this, [this]() { setAllChecked(false); });
    sl->addWidget(selectAll);
    sl->addWidget(dot);
    sl->addWidget(deselectAll);
    sl->addStretch();
    sl->addWidget(m_countLabel);
    root->addWidget(selBar);

    // ── Árvore ──
    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("exportTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setIndentation(16);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem*, int) {
        if (!m_building) recomputeCount();
    });
    root->addWidget(m_tree, /*stretch=*/1);

    // ── Footer ──
    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("exportFooter"));
    auto* fl = new QVBoxLayout(footer);
    fl->setContentsMargins(18, 12, 18, 14);
    fl->setSpacing(10);

    auto* note = new QLabel(
        QStringLiteral("Ao exportar capítulos, caso haja cenas com variações criadas, o app "
                       "exportará com a variação que estiver definida como primária."),
        footer);
    note->setObjectName(QStringLiteral("exportNote"));
    note->setWordWrap(true);
    fl->addWidget(note);

    // Linha: formato
    auto* fmtRow = new QHBoxLayout;
    fmtRow->setSpacing(6);
    auto* fmtLabel = new QLabel(QStringLiteral("Formato:"), footer);
    fmtLabel->setObjectName(QStringLiteral("exportFieldLabel"));
    fmtRow->addWidget(fmtLabel);
    const QStringList formats = { QStringLiteral("odt"), QStringLiteral("pdf"),
                                  QStringLiteral("epub"), QStringLiteral("docx") };
    for (const QString& f : formats) {
        auto* b = new QPushButton(f.toUpper(), footer);
        b->setObjectName(QStringLiteral("exportFormat"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("fmt", f);
        b->setEnabled(true);
        b->setChecked(f == m_format);
        connect(b, &QPushButton::clicked, this, [this, f]() {
            m_format = f;
            for (QPushButton* btn : m_formatBtns)
                btn->setChecked(btn->property("fmt").toString() == f);
        });
        m_formatBtns.append(b);
        fmtRow->addWidget(b);
    }
    fmtRow->addStretch();
    fl->addLayout(fmtRow);

    // Linha: modo do manuscrito
    auto* modeRow = new QHBoxLayout;
    modeRow->setSpacing(8);
    auto* modeLabel = new QLabel(QStringLiteral("Manuscrito:"), footer);
    modeLabel->setObjectName(QStringLiteral("exportFieldLabel"));
    m_singleRadio = new QRadioButton(QStringLiteral("Documento único"), footer);
    m_separateRadio = new QRadioButton(QStringLiteral("Capítulos separados"), footer);
    m_singleRadio->setChecked(true);
    auto* modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_singleRadio);
    modeGroup->addButton(m_separateRadio);
    modeRow->addWidget(modeLabel);
    modeRow->addWidget(m_singleRadio);
    modeRow->addWidget(m_separateRadio);
    modeRow->addStretch();
    fl->addLayout(modeRow);

    // Linha: marcadores (marca-texto) no documento exportado
    auto* markersRow = new QHBoxLayout;
    markersRow->setSpacing(8);
    auto* markersLabel = new QLabel(QStringLiteral("Marcadores:"), footer);
    markersLabel->setObjectName(QStringLiteral("exportFieldLabel"));
    m_markersIncludeRadio = new QRadioButton(QStringLiteral("Incluir"), footer);
    m_markersRemoveRadio = new QRadioButton(QStringLiteral("Remover"), footer);
    m_markersIncludeRadio->setChecked(true);
    auto* markersGroup = new QButtonGroup(this);
    markersGroup->addButton(m_markersIncludeRadio);
    markersGroup->addButton(m_markersRemoveRadio);
    markersRow->addWidget(markersLabel);
    markersRow->addWidget(m_markersIncludeRadio);
    markersRow->addWidget(m_markersRemoveRadio);
    markersRow->addStretch();
    fl->addLayout(markersRow);

    // Linha: botões
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto* cancel = new QPushButton(QStringLiteral("Cancelar"), footer);
    cancel->setObjectName(QStringLiteral("exportCancel"));
    cancel->setCursor(Qt::PointingHandCursor);
    m_exportBtn = new QPushButton(QStringLiteral("Exportar"), footer);
    m_exportBtn->setObjectName(QStringLiteral("exportConfirm"));
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_exportBtn, &QPushButton::clicked, this, &ExportPanel::onExportClicked);
    btnRow->addWidget(cancel);
    btnRow->addWidget(m_exportBtn);
    fl->addLayout(btnRow);

    root->addWidget(footer);

    buildTree();
    applyTheme();
    recomputeCount();
}

void ExportPanel::buildTree() {
    m_building = true;
    m_tree->clear();
    m_totalLeaves = 0;

    const auto groupFlags = Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsAutoTristate;
    const auto leafFlags = Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;

    auto styleHeader = [](QTreeWidgetItem* it) {
        it->setData(0, KindRole, KindHeader);
        it->setFlags(Qt::ItemIsEnabled);
        QFont f = it->font(0);
        f.setBold(true);
        f.setPointSizeF(f.pointSizeF() - 1.0);
        it->setFont(0, f);
        it->setForeground(0, QColor(Theme::textMuted()));
    };

    // ── Manuscritos ──
    auto* msHeader = new QTreeWidgetItem(m_tree);
    msHeader->setText(0, QStringLiteral("MANUSCRITOS"));
    styleHeader(msHeader);

    for (const Manuscript& ms : m_model->manuscripts()) {
        QList<const Chapter*> chaps;
        for (const Chapter& ch : m_model->chapters()) {
            const QString msId = ch.manuscriptId.isEmpty() ? ms.id : ch.manuscriptId;
            if (msId == ms.id) chaps.append(&ch);
        }
        if (chaps.isEmpty()) continue;
        std::sort(chaps.begin(), chaps.end(),
                  [](const Chapter* a, const Chapter* b) { return a->order < b->order; });

        auto* msItem = new QTreeWidgetItem(msHeader);
        msItem->setText(0, ms.title.isEmpty() ? QStringLiteral("Manuscrito") : ms.title);
        msItem->setData(0, KindRole, KindGroup);
        msItem->setFlags(groupFlags);
        msItem->setCheckState(0, Qt::Unchecked);

        for (int ci = 0; ci < chaps.size(); ++ci) {
            const Chapter* ch = chaps.at(ci);
            auto* c = new QTreeWidgetItem(msItem);
            const QString num = QString::number(ci + 1).rightJustified(2, QLatin1Char('0'));
            c->setText(0, QStringLiteral("%1  %2").arg(num,
                ch->title.isEmpty() ? QStringLiteral("Capítulo %1").arg(ci + 1) : ch->title));
            c->setData(0, IdRole, ch->id);
            c->setData(0, KindRole, KindChapter);
            c->setFlags(leafFlags);
            c->setCheckState(0, Qt::Unchecked);
            ++m_totalLeaves;
        }
        msItem->setExpanded(true);
    }
    if (msHeader->childCount() == 0) delete msHeader;
    else msHeader->setExpanded(true);

    // ── Gavetas ──
    auto* gvHeader = new QTreeWidgetItem(m_tree);
    gvHeader->setText(0, QStringLiteral("GAVETAS"));
    styleHeader(gvHeader);

    // Adiciona itens/pastas de uma pasta; retorna nº de folhas adicionadas abaixo.
    std::function<int(QTreeWidgetItem*, const Drawer&, const QString&)> addFolder =
        [&](QTreeWidgetItem* node, const Drawer& d, const QString& folderId) -> int {
            int leaves = 0;
            for (const DrawerItem& it : d.items) {
                if ((it.folderId.isEmpty() ? QString() : it.folderId) != folderId) continue;
                auto* leaf = new QTreeWidgetItem(node);
                leaf->setText(0, it.title.isEmpty() ? QStringLiteral("Documento") : it.title);
                leaf->setData(0, IdRole, it.id);
                leaf->setData(0, KindRole, KindItem);
                leaf->setFlags(leafFlags);
                leaf->setCheckState(0, Qt::Unchecked);
                ++leaves;
                ++m_totalLeaves;
            }
            for (const Folder& f : d.folders) {
                if ((f.parentId.isEmpty() ? QString() : f.parentId) != folderId) continue;
                auto* sub = new QTreeWidgetItem(node);
                sub->setText(0, f.title);
                sub->setData(0, KindRole, KindGroup);
                sub->setFlags(groupFlags);
                sub->setCheckState(0, Qt::Unchecked);
                const int childLeaves = addFolder(sub, d, f.id);
                if (childLeaves == 0) { delete sub; }
                else { sub->setExpanded(true); leaves += childLeaves; }
            }
            return leaves;
        };

    for (const Drawer& d : m_model->drawers()) {
        auto* dItem = new QTreeWidgetItem(gvHeader);
        dItem->setText(0, d.title);
        dItem->setData(0, KindRole, KindGroup);
        dItem->setFlags(groupFlags);
        dItem->setCheckState(0, Qt::Unchecked);
        const int leaves = addFolder(dItem, d, QString());
        if (leaves == 0) delete dItem;
        else dItem->setExpanded(true);
    }
    if (gvHeader->childCount() == 0) delete gvHeader;
    else gvHeader->setExpanded(true);

    m_building = false;
}

void ExportPanel::setAllChecked(bool checked) {
    m_building = true;
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        const int kind = (*it)->data(0, KindRole).toInt();
        if (kind == KindChapter || kind == KindItem)
            (*it)->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
        ++it;
    }
    m_building = false;
    recomputeCount();
}

void ExportPanel::recomputeCount() {
    int selected = 0;
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        const int kind = (*it)->data(0, KindRole).toInt();
        if ((kind == KindChapter || kind == KindItem) && (*it)->checkState(0) == Qt::Checked)
            ++selected;
        ++it;
    }
    if (m_countLabel)
        m_countLabel->setText(QStringLiteral("%1 / %2").arg(selected).arg(m_totalLeaves));
    if (m_exportBtn) {
        m_exportBtn->setEnabled(selected > 0);
        m_exportBtn->setText(selected > 0
            ? QStringLiteral("Exportar (%1)").arg(selected)
            : QStringLiteral("Exportar"));
    }
}

void ExportPanel::onExportClicked() {
    Exporter::Selection sel;
    sel.format = (m_format == QStringLiteral("pdf")) ? Exporter::Format::Pdf
               : (m_format == QStringLiteral("epub")) ? Exporter::Format::Epub
               : (m_format == QStringLiteral("docx")) ? Exporter::Format::Docx
               : Exporter::Format::Odt;
    sel.manuscriptMode = (m_singleRadio && m_singleRadio->isChecked())
        ? Exporter::ManuscriptMode::SingleDocument
        : Exporter::ManuscriptMode::SeparateChapters;
    sel.includeMarkers = !m_markersRemoveRadio || !m_markersRemoveRadio->isChecked();

    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        if ((*it)->checkState(0) == Qt::Checked) {
            const int kind = (*it)->data(0, KindRole).toInt();
            const QString id = (*it)->data(0, IdRole).toString();
            if (kind == KindChapter) sel.chapterIds.insert(id);
            else if (kind == KindItem) sel.itemIds.insert(id);
        }
        ++it;
    }

    if (sel.chapterIds.isEmpty() && sel.itemIds.isEmpty()) return;
    emit exportRequested(sel);
    accept();
}

void ExportPanel::applyTheme() {
    const QString panelBg   = Theme::panelBackground();
    const QString panelBd   = Theme::panelBorder();
    const QString txtPrim   = Theme::textPrimary();
    const QString txtMuted  = Theme::textMuted();
    const QString txtBright = Theme::textBright();
    const QString inputBg   = Theme::inputBackground();
    const QString subtleBd  = Theme::subtleBorder();
    const QString hover     = Theme::hoverOverlay();
    const QString hoverStr  = Theme::hoverStrong();
    const QString accent    = Theme::accentDefault();

    setStyleSheet(QStringLiteral(R"(
        #exportPanel { background-color: %1; }
        #exportPanel QLabel { color: %4; font-size: 13px; }
        #exportPanel QLabel#exportTitle { color: %6; font-size: 16px; font-weight: bold; }
        #exportPanel QLabel#exportNote { color: %5; font-size: 11px; }
        #exportPanel QLabel#exportCount { color: %5; font-size: 12px; }
        #exportPanel QLabel#exportDot { color: %5; }
        #exportPanel QLabel#exportFieldLabel { color: %5; font-size: 11px; }
        #exportPanel #exportSelBar { border-top: 1px solid %2; border-bottom: 1px solid %2; }
        #exportPanel #exportFooter { border-top: 1px solid %2; }
        #exportPanel QPushButton#exportClose {
            background: transparent; color: %5; border: none; font-size: 16px; border-radius: 6px;
        }
        #exportPanel QPushButton#exportClose:hover { background: %8; color: %6; }
        #exportPanel QPushButton#exportLink {
            background: transparent; color: %5; border: none; font-size: 12px; padding: 0;
        }
        #exportPanel QPushButton#exportLink:hover { color: %6; }
        #exportPanel QPushButton#exportFormat {
            background: transparent; color: %5; border: 1px solid %2; border-radius: 5px;
            padding: 3px 10px; font-size: 11px; font-weight: bold;
        }
        #exportPanel QPushButton#exportFormat:checked {
            background: %10; color: #ffffff; border-color: %10;
        }
        #exportPanel QPushButton#exportFormat:disabled { color: %7; border-color: %2; }
        #exportPanel QPushButton#exportCancel {
            background: %8; color: %4; border: none; padding: 7px 16px; border-radius: 6px; font-size: 12px;
        }
        #exportPanel QPushButton#exportCancel:hover { background: %9; color: %6; }
        #exportPanel QPushButton#exportConfirm {
            background: %10; color: #ffffff; border: none; padding: 7px 18px; border-radius: 6px;
            font-size: 12px; font-weight: bold;
        }
        #exportPanel QPushButton#exportConfirm:hover { background: %10; }
        #exportPanel QPushButton#exportConfirm:disabled { background: %8; color: %7; }
        #exportPanel QRadioButton { color: %4; font-size: 12px; spacing: 6px; }
        #exportPanel QRadioButton::indicator {
            width: 13px; height: 13px; border-radius: 7px; border: 1px solid %7; background: %3;
        }
        #exportPanel QRadioButton::indicator:checked { background: %10; border-color: %10; }
        #exportPanel QTreeWidget#exportTree {
            background: %1; border: none; outline: none; color: %4; font-size: 13px;
            padding: 8px 10px;
        }
        #exportPanel QTreeWidget#exportTree::item { padding: 3px 0; }
        #exportPanel QTreeWidget#exportTree::indicator {
            width: 14px; height: 14px; border-radius: 3px; border: 1px solid %7; background: %3;
        }
        #exportPanel QTreeWidget#exportTree::indicator:checked { background: %10; border-color: %10; }
        #exportPanel QTreeWidget#exportTree::indicator:indeterminate {
            background: %9; border-color: %10;
        }
        #exportPanel QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
        #exportPanel QScrollBar::handle:vertical { background: %2; border-radius: 5px; min-height: 30px; }
        #exportPanel QScrollBar::handle:vertical:hover { background: %9; }
        #exportPanel QScrollBar::add-line:vertical, #exportPanel QScrollBar::sub-line:vertical { height: 0; }
        #exportPanel QScrollBar::add-page:vertical, #exportPanel QScrollBar::sub-page:vertical { background: transparent; }
    )")
        .arg(panelBg,   // 1
             panelBd,   // 2
             inputBg,   // 3
             txtPrim,   // 4
             txtMuted,  // 5
             txtBright, // 6
             subtleBd,  // 7
             hover,     // 8
             hoverStr,  // 9
             accent));  // 10
}
