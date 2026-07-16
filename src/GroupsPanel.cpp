#include "GroupsPanel.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <QApplication>
#include <QColorDialog>
#include <QScreen>
#include <QDialog>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

static const QStringList kDefaultColors = {
    QStringLiteral("#E57373"), QStringLiteral("#FFB74D"), QStringLiteral("#FFF176"),
    QStringLiteral("#81C784"), QStringLiteral("#4FC3F7"), QStringLiteral("#CE93D8"),
    QStringLiteral("#F06292"), QStringLiteral("#80CBC4"), QStringLiteral("#A1887F"),
};

QPixmap makeColorDot(const QString& color, int size = 10) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(color));
    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, size - 2, size - 2);
    return pm;
}

} // namespace

GroupsPanel::GroupsPanel(ProjectModel* model, QWidget* parent)
    : QFrame(parent), m_model(model)
{
    setObjectName(QStringLiteral("groupsPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFrameShape(QFrame::NoFrame);

    buildUi();

    if (m_model) {
        connect(m_model, &ProjectModel::groupsChanged,  this, &GroupsPanel::onGroupsChanged);
        connect(m_model, &ProjectModel::drawersChanged, this, &GroupsPanel::onGroupsChanged);
        connect(m_model, &ProjectModel::loaded,         this, &GroupsPanel::onGroupsChanged);
    }
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &GroupsPanel::applyTheme);
}

void GroupsPanel::buildUi()
{
    auto* rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    // ---- Header ----
    m_header = new QWidget(this);
    m_header->setFixedHeight(38);
    m_header->setCursor(Qt::SizeAllCursor);
    m_header->installEventFilter(this);
    auto* hLay = new QHBoxLayout(m_header);
    hLay->setContentsMargins(10, 0, 6, 0);
    hLay->setSpacing(6);

    m_title = new QLabel(tr("Grupos"), m_header);
    m_title->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Lora','Crimson Text',serif; font-size: 13px; font-weight: 700;")
        .arg(Theme::textBright()));
    hLay->addWidget(m_title);
    hLay->addStretch();

    m_newBtn = new QToolButton(m_header);
    m_newBtn->setText(QStringLiteral("+"));
    m_newBtn->setToolTip(tr("Novo grupo"));
    m_newBtn->setCursor(Qt::PointingHandCursor);
    m_newBtn->setFixedSize(26, 26);
    m_newBtn->setStyleSheet(QStringLiteral(R"(
        QToolButton {
            background: transparent;
            color: %1;
            border: 1px solid %2;
            border-radius: 6px;
            font-size: 18px;
            font-weight: 300;
        }
        QToolButton:hover { background: %3; color: %4; border-color: %5; }
    )").arg(Theme::textMuted(), Theme::panelBorder(),
            Theme::hoverOverlay(), Theme::textBright(), Theme::textMuted()));
    hLay->addWidget(m_newBtn);

    m_closeBtn = new QToolButton(m_header);
    m_closeBtn->setText(QStringLiteral("×"));
    m_closeBtn->setToolTip(tr("Fechar"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setFixedSize(26, 26);
    m_closeBtn->setStyleSheet(QStringLiteral(R"(
        QToolButton {
            background: transparent;
            color: %1;
            border: none;
            border-radius: 6px;
            font-size: 16px;
        }
        QToolButton:hover { background: %2; color: %3; }
    )").arg(Theme::textMuted(), Theme::hoverOverlay(), Theme::textBright()));
    hLay->addWidget(m_closeBtn);
    connect(m_closeBtn, &QToolButton::clicked, this, &GroupsPanel::closeRequested);

    rootLay->addWidget(m_header);

    // Separador
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QStringLiteral("background: %1; border: none;").arg(Theme::panelBorder()));
    rootLay->addWidget(sep);

    // ---- Chips de grupo ----
    m_chipsScroll = new QScrollArea(this);
    m_chipsScroll->setFixedHeight(44);
    m_chipsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chipsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chipsScroll->setWidgetResizable(true);
    m_chipsScroll->setFrameShape(QFrame::NoFrame);
    m_chipsScroll->setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    m_chipsWrap = new QWidget(m_chipsScroll);
    m_chipsWrap->setStyleSheet(QStringLiteral("background: transparent;"));
    m_chipsScroll->setWidget(m_chipsWrap);
    rootLay->addWidget(m_chipsScroll);

    // Separador 2
    auto* sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFixedHeight(1);
    sep2->setStyleSheet(QStringLiteral("background: %1; border: none;").arg(Theme::subtleBorder()));
    rootLay->addWidget(sep2);

    // ---- Lista de itens ----
    m_listScroll = new QScrollArea(this);
    m_listScroll->setWidgetResizable(true);
    m_listScroll->setFrameShape(QFrame::NoFrame);
    m_listScroll->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    m_listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_listHost = new QWidget(m_listScroll);
    m_listHost->setStyleSheet(QStringLiteral("background: transparent;"));
    m_listLay = new QVBoxLayout(m_listHost);
    m_listLay->setContentsMargins(8, 8, 8, 8);
    m_listLay->setSpacing(4);
    m_listLay->addStretch();
    m_listScroll->setWidget(m_listHost);
    rootLay->addWidget(m_listScroll, 1);

    // Conecta botão Novo
    connect(m_newBtn, &QToolButton::clicked, this, [this]() {
        showEditGroupDialog(QString()); // id vazio = criar
    });

    applyTheme();
    rebuildGroupChips();
    rebuildItemList();
}

void GroupsPanel::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        QFrame#groupsPanel {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }
    )").arg(Theme::panelBackground(), Theme::panelBorder()));

    if (m_title)
        m_title->setStyleSheet(QStringLiteral(
            "color: %1; font-family: 'Lora','Crimson Text',serif; font-size: 13px; font-weight: 700;")
            .arg(Theme::textBright()));
    if (m_newBtn)
        m_newBtn->setStyleSheet(QStringLiteral(R"(
            QToolButton {
                background: transparent; color: %1; border: 1px solid %2;
                border-radius: 6px; font-size: 18px; font-weight: 300;
            }
            QToolButton:hover { background: %3; color: %4; border-color: %5; }
        )").arg(Theme::textMuted(), Theme::panelBorder(),
                Theme::hoverOverlay(), Theme::textBright(), Theme::textMuted()));
    if (m_closeBtn)
        m_closeBtn->setStyleSheet(QStringLiteral(R"(
            QToolButton {
                background: transparent; color: %1; border: none; border-radius: 6px; font-size: 16px;
            }
            QToolButton:hover { background: %2; color: %3; }
        )").arg(Theme::textMuted(), Theme::hoverOverlay(), Theme::textBright()));

    rebuildGroupChips();
    rebuildItemList();
}

void GroupsPanel::onGroupsChanged()
{
    // Se o grupo selecionado foi removido, limpa seleção
    if (!m_selectedGroupId.isEmpty() && !m_model->findGroup(m_selectedGroupId))
        m_selectedGroupId.clear();
    rebuildGroupChips();
    rebuildItemList();
}

void GroupsPanel::rebuildGroupChips()
{
    if (!m_chipsWrap) return;

    // Limpa filhos diretos do wrap
    for (auto* w : m_chipsWrap->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
        w->deleteLater();
    delete m_chipsWrap->layout();

    auto* lay = new QHBoxLayout(m_chipsWrap);
    lay->setContentsMargins(8, 6, 8, 6);
    lay->setSpacing(6);

    if (!m_model || m_model->groups().isEmpty()) {
        auto* lbl = new QLabel(tr("Nenhum grupo criado"), m_chipsWrap);
        lbl->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; font-style: italic;").arg(Theme::textMuted()));
        lay->addWidget(lbl);
        lay->addStretch();
        return;
    }

    for (const auto& g : m_model->groups()) {
        auto* chip = new QPushButton(m_chipsWrap);
        chip->setCheckable(true);
        chip->setChecked(g.id == m_selectedGroupId);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setContextMenuPolicy(Qt::CustomContextMenu);

        // ícone dot colorido
        chip->setIcon(QIcon(makeColorDot(g.color, 10)));
        chip->setIconSize(QSize(10, 10));
        chip->setText(g.title);

        const bool active = (g.id == m_selectedGroupId);
        chip->setStyleSheet(QStringLiteral(R"(
            QPushButton {
                background: %1;
                color: %2;
                border: 1px solid %3;
                border-radius: 12px;
                padding: 3px 10px 3px 6px;
                font-size: 11px;
                font-family: 'Lora','Crimson Text',serif;
            }
            QPushButton:hover   { background: %4; color: %5; border-color: %6; }
            QPushButton:checked { background: %7; color: %5; border-color: %8; }
        )").arg(active ? Theme::pressedOverlay() : QStringLiteral("transparent"),
                active ? Theme::textBright() : Theme::textPrimary(),
                active ? Theme::accentDefault() : Theme::subtleBorder(),
                Theme::hoverOverlay(), Theme::textBright(), Theme::borderStrong(),
                Theme::pressedOverlay(), Theme::accentDefault()));

        const QString gid = g.id;
        connect(chip, &QPushButton::clicked, this, [this, gid]() {
            m_selectedGroupId = (m_selectedGroupId == gid) ? QString() : gid;
            rebuildGroupChips();
            rebuildItemList();
        });
        connect(chip, &QPushButton::customContextMenuRequested, this, [this, chip, gid](const QPoint& pos) {
            showGroupContextMenu(gid, chip->mapToGlobal(pos));
        });
        lay->addWidget(chip);
    }
    lay->addStretch();
}

void GroupsPanel::rebuildItemList()
{
    if (!m_listLay) return;

    // Limpa
    while (m_listLay->count() > 0) {
        auto* it = m_listLay->takeAt(0);
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }

    if (!m_model) { m_listLay->addStretch(); return; }

    if (m_selectedGroupId.isEmpty() && !m_model->groups().isEmpty()) {
        auto* hint = new QLabel(tr("Selecione um grupo acima"), m_listHost);
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; font-style: italic; padding: 20px;").arg(Theme::textMuted()));
        m_listLay->addWidget(hint);
        m_listLay->addStretch();
        return;
    }

    if (m_model->groups().isEmpty()) {
        auto* hint = new QLabel(tr("Crie um grupo com o botão +"), m_listHost);
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; font-style: italic; padding: 20px;").arg(Theme::textMuted()));
        m_listLay->addWidget(hint);
        m_listLay->addStretch();
        return;
    }

    const Group* grp = m_model->findGroup(m_selectedGroupId);
    int count = 0;

    for (const auto& drawer : m_model->drawers()) {
        for (const auto& it : drawer.items) {
            if (it.markerId != m_selectedGroupId) continue;

            auto* row = new QFrame(m_listHost);
            row->setCursor(Qt::PointingHandCursor);
            row->setAttribute(Qt::WA_StyledBackground, true);
            row->setStyleSheet(QStringLiteral(R"(
                QFrame {
                    background: %1;
                    border: 1px solid %2;
                    border-radius: 6px;
                }
                QFrame:hover { background: %3; border-color: %4; }
            )").arg(Theme::pressedOverlay(), Theme::subtleBorder(),
                    Theme::hoverOverlay(), Theme::borderStrong()));

            auto* rowLay = new QHBoxLayout(row);
            rowLay->setContentsMargins(8, 6, 8, 6);
            rowLay->setSpacing(8);

            // Dot de cor do grupo
            if (grp) {
                auto* dot = new QLabel(row);
                dot->setFixedSize(8, 8);
                dot->setStyleSheet(QStringLiteral(
                    "background: %1; border-radius: 4px;").arg(grp->color));
                rowLay->addWidget(dot, 0, Qt::AlignVCenter);
            }

            // Título do item
            auto* titleLbl = new QLabel(it.title.isEmpty() ? tr("(sem nome)") : it.title, row);
            titleLbl->setStyleSheet(QStringLiteral(
                "color: %1; font-size: 12px; font-family: 'Lora','Crimson Text',serif;")
                .arg(Theme::textPrimary()));
            rowLay->addWidget(titleLbl, 1);

            // Chip da gaveta
            auto* drawerChip = new QLabel(drawer.title.isEmpty() ? tr("(gaveta)") : drawer.title, row);
            drawerChip->setStyleSheet(QStringLiteral(R"(
                color: %1;
                background: %2;
                border-radius: 8px;
                padding: 1px 7px;
                font-size: 10px;
                font-family: 'Lora','Crimson Text',serif;
            )").arg(Theme::textMuted(), Theme::hoverOverlay()));
            rowLay->addWidget(drawerChip, 0, Qt::AlignVCenter);

            const QString dkey = drawer.key;
            const QString iid  = it.id;
            row->installEventFilter(this);
            row->setProperty("drawerKey", dkey);
            row->setProperty("itemId",    iid);

            m_listLay->addWidget(row);
            ++count;
        }
    }

    if (count == 0) {
        auto* hint = new QLabel(tr("Nenhum documento neste grupo"), m_listHost);
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; font-style: italic; padding: 20px;").arg(Theme::textMuted()));
        m_listLay->addWidget(hint);
    }
    m_listLay->addStretch();
}

void GroupsPanel::showGroupContextMenu(const QString& groupId, const QPoint& globalPos)
{
    if (!m_model) return;
    const Group* g = m_model->findGroup(groupId);
    if (!g) return;

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(R"(
        QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 4px; }
        QMenu::item { padding: 6px 16px; border-radius: 4px; }
        QMenu::item:selected { background: %4; color: %5; }
    )").arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
            Theme::hoverOverlay(), Theme::textBright()));

    auto* editAct = menu.addAction(tr("Editar grupo…"));
    connect(editAct, &QAction::triggered, this, [this, groupId]() {
        showEditGroupDialog(groupId);
    });

    menu.addSeparator();
    auto* deleteAct = menu.addAction(tr("Excluir grupo"));
    connect(deleteAct, &QAction::triggered, this, [this, groupId]() {
        m_model->removeGroup(groupId);
    });

    menu.exec(globalPos);
}

void GroupsPanel::showEditGroupDialog(const QString& groupId)
{
    if (!m_model) return;
    const bool isNew = groupId.isEmpty();
    const Group* existing = isNew ? nullptr : m_model->findGroup(groupId);

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(isNew ? tr("Novo grupo") : tr("Editar grupo"));
    dlg->setModal(true);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setFixedWidth(300);
    dlg->setStyleSheet(QStringLiteral(R"(
        QDialog { background: %1; color: %2; }
        QLineEdit {
            background: %3; color: %2; border: 1px solid %4;
            border-radius: 6px; padding: 6px 10px;
        }
        QPushButton#okBtn {
            background: %5; color: %6; border: none; border-radius: 6px; padding: 6px 14px; font-weight: 600;
        }
        QPushButton#okBtn:hover { background: %7; }
        QPushButton#cancelBtn {
            background: transparent; color: %8; border: 1px solid %4;
            border-radius: 6px; padding: 6px 14px;
        }
        QPushButton#cancelBtn:hover { background: %9; }
    )").arg(Theme::panelBackground(), Theme::textPrimary(),
            Theme::inputBackground(), Theme::panelBorder(),
            Theme::accentDefault(), Theme::textBright(),
            Theme::borderStrong(), Theme::textMuted(),
            Theme::hoverOverlay()));

    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(12);

    auto* nameEdit = new QLineEdit(dlg);
    nameEdit->setPlaceholderText(tr("Nome do grupo"));
    if (existing) nameEdit->setText(existing->title);
    lay->addWidget(nameEdit);

    // Cor inicial
    QString chosenColor = existing ? existing->color : kDefaultColors.first();

    auto* colorRow = new QHBoxLayout();
    colorRow->setSpacing(6);
    QList<QPushButton*> colorBtns;
    for (const QString& col : kDefaultColors) {
        auto* cb = new QPushButton(dlg);
        cb->setFixedSize(24, 24);
        cb->setCheckable(true);
        cb->setChecked(col == chosenColor);
        cb->setProperty("colorValue", col);
        cb->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; border-radius: 12px; border: 2px solid transparent; }"
            "QPushButton:checked { border-color: %2; }"
            "QPushButton:hover   { border-color: %3; }")
            .arg(col, Theme::textBright(), Theme::textMuted()));
        colorRow->addWidget(cb);
        colorBtns.append(cb);
    }
    colorRow->addStretch();
    lay->addLayout(colorRow);

    for (auto* cb : colorBtns) {
        connect(cb, &QPushButton::clicked, dlg, [cb, &chosenColor, &colorBtns](bool) {
            chosenColor = cb->property("colorValue").toString();
            for (auto* b : colorBtns) b->setChecked(b == cb);
        });
    }

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    auto* cancelBtn = new QPushButton(tr("Cancelar"), dlg);
    cancelBtn->setObjectName(QStringLiteral("cancelBtn"));
    auto* okBtn = new QPushButton(isNew ? tr("Criar") : tr("Salvar"), dlg);
    okBtn->setObjectName(QStringLiteral("okBtn"));
    okBtn->setDefault(true);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, dlg, [dlg, nameEdit, &chosenColor, this, groupId, isNew]() {
        const QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) { nameEdit->setFocus(); return; }
        if (isNew) {
            const QString newId = m_model->addGroup(name, chosenColor);
            m_selectedGroupId = newId;
        } else {
            m_model->updateGroup(groupId, name, chosenColor);
        }
        dlg->accept();
    });

    dlg->exec();
}

void GroupsPanel::showNear(const QRect& anchorGlobal)
{
    const QRect screen = QApplication::primaryScreen()
        ? QApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, 1920, 1080);

    if (!isVisible()) {
        resize(280, 420);
        // Posiciona à direita do botão âncora
        int x = anchorGlobal.right() + 8;
        int y = anchorGlobal.top();
        if (x + width() > screen.right()) x = anchorGlobal.left() - width() - 8;
        if (y + height() > screen.bottom()) y = screen.bottom() - height() - 8;
        move(x, y);
    }
    show();
    raise();
}

bool GroupsPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Drag pelo header
    if (watched == m_header) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragOffset = me->globalPosition().toPoint() - pos();
                return false;
            }
        }
    }

    // Clique nos itens da lista
    auto* w = qobject_cast<QWidget*>(watched);
    if (w && event->type() == QEvent::MouseButtonPress) {
        const QString dkey = w->property("drawerKey").toString();
        const QString iid  = w->property("itemId").toString();
        if (!dkey.isEmpty() && !iid.isEmpty()) {
            emit itemActivated(dkey, iid);
            return true;
        }
    }

    return QFrame::eventFilter(watched, event);
}

void GroupsPanel::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && m_header && m_header->geometry().contains(e->pos())) {
        m_dragging = true;
        m_dragOffset = e->globalPosition().toPoint() - pos();
    }
    QFrame::mousePressEvent(e);
}

void GroupsPanel::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging)
        move(e->globalPosition().toPoint() - m_dragOffset);
    QFrame::mouseMoveEvent(e);
}

void GroupsPanel::mouseReleaseEvent(QMouseEvent* e)
{
    m_dragging = false;
    QFrame::mouseReleaseEvent(e);
}
