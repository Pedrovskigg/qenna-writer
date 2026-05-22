#include "ManuscriptPanel.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr const char* kChapterMime = "application/x-mira-chapter-id";
constexpr const char* kSceneMime   = "application/x-mira-scene-ref";
}

namespace {
constexpr int kPanelWidth = 280;

// Mesmo "plus" do botão grande da DrawerListPanel.
QPixmap circlePlusPixmap(const QColor& color, int size = 18) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color);
    pen.setWidthF(1.6);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    const int margin = 2;
    const QRectF r(margin, margin, size - margin * 2, size - margin * 2);
    p.drawEllipse(r);
    const double cx = size / 2.0;
    const double cy = size / 2.0;
    const double L = (size - margin * 2) * 0.34;
    p.drawLine(QPointF(cx - L, cy), QPointF(cx + L, cy));
    p.drawLine(QPointF(cx, cy - L), QPointF(cx, cy + L));
    return pm;
}

QString createButtonQss(const QString& accent) {
    const QColor c(accent);
    const QString bgStr = QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(0.10);
    const QString bgHoverStr = QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(0.20);
    return QStringLiteral(R"(
        QPushButton {
            background: %4;
            color: %1;
            border: 1px solid %2;
            border-radius: 8px;
            padding: 10px 12px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 14px;
            font-weight: 600;
            text-align: left;
        }
        QPushButton:hover {
            background: %3;
        }
        QPushButton:pressed {
            background: %5;
        }
    )").arg(accent, accent, bgHoverStr, bgStr, bgHoverStr);
}
}

ManuscriptPanel::ManuscriptPanel(ProjectModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_combo(nullptr)
    , m_listLayout(nullptr)
    , m_scroll(nullptr)
{
    setObjectName(QStringLiteral("manuscriptPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(kPanelWidth);
    setStyleSheet(Theme::panelQss(QStringLiteral("manuscriptPanel")));
    setAcceptDrops(true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header --------------------------------------------------------------
    auto* header = new QWidget(this);
    m_header = header;
    header->setObjectName(QStringLiteral("manuscriptHeader"));
    header->setStyleSheet(QStringLiteral(
        "#manuscriptHeader { border-bottom: 1px solid %1; }").arg(Theme::panelBorder()));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 10, 8, 10);
    headerLayout->setSpacing(4);

    m_combo = new QComboBox(header);
    m_combo->setStyleSheet(QStringLiteral(R"(
        QComboBox {
            background: transparent;
            color: %1;
            border: 1px solid %2;
            border-radius: 6px;
            padding: 4px 10px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 13px;
        }
        QComboBox:hover { border-color: %3; }
        QComboBox::drop-down { border: none; width: 18px; }
        QComboBox QAbstractItemView {
            background: %4;
            color: %1;
            border: 1px solid %2;
            selection-background-color: %5;
        }
    )").arg(Theme::textBright(), Theme::panelBorder(), Theme::subtleBorder(),
           Theme::panelBackground(), Theme::hoverOverlay()));
    headerLayout->addWidget(m_combo, /*stretch=*/1);
    connect(m_combo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ManuscriptPanel::onComboChanged);

    auto makeIconBtn = [this](const QString& glyph, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setText(glyph);
        b->setToolTip(tip);
        b->setFixedSize(28, 28);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QStringLiteral(R"(
            QToolButton {
                background: transparent;
                color: %1;
                border: 1px solid transparent;
                border-radius: 6px;
                font-size: 15px;
            }
            QToolButton:hover {
                background: %2;
                color: %3;
                border-color: %4;
            }
        )").arg(Theme::textMuted(), Theme::hoverOverlay(), Theme::textBright(), Theme::subtleBorder()));
        m_headerIconBtns.append(b);
        return b;
    };

    auto* btnAddMs = makeIconBtn(QStringLiteral("✦"), tr("Novo manuscrito"));
    connect(btnAddMs, &QToolButton::clicked, this, &ManuscriptPanel::newManuscriptRequested);
    headerLayout->addWidget(btnAddMs);

    auto* btnClose = makeIconBtn(QStringLiteral("×"), tr("Fechar"));
    connect(btnClose, &QToolButton::clicked, this, &ManuscriptPanel::closePanel);
    headerLayout->addWidget(btnClose);

    root->addWidget(header);

    // Action bar (botão grande "Criar novo capítulo") ---------------------
    auto* actionBar = new QWidget(this);
    actionBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* actionLayout = new QVBoxLayout(actionBar);
    actionLayout->setContentsMargins(12, 12, 12, 10);
    actionLayout->setSpacing(8);

    const QString accent = Theme::accentDefault();
    auto* createChapterBtn = new QPushButton(actionBar);
    m_createChapterBtn = createChapterBtn;
    createChapterBtn->setCursor(Qt::PointingHandCursor);
    createChapterBtn->setIconSize(QSize(18, 18));
    createChapterBtn->setMinimumHeight(40);
    createChapterBtn->setText(QStringLiteral("  ") + tr("Criar novo capítulo"));
    createChapterBtn->setIcon(QIcon(circlePlusPixmap(QColor(accent), 18)));
    createChapterBtn->setStyleSheet(createButtonQss(accent));
    connect(createChapterBtn, &QPushButton::clicked, this, [this]() {
        emit newChapterRequested(activeManuscriptId());
    });
    actionLayout->addWidget(createChapterBtn);
    root->addWidget(actionBar);

    // Lista de capítulos --------------------------------------------------
    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName(QStringLiteral("manuscriptScroll"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setStyleSheet(QStringLiteral("#manuscriptScroll { background: transparent; }"));

    auto* listHost = new QWidget(m_scroll);
    listHost->setStyleSheet(QStringLiteral("background: transparent;"));
    m_listLayout = new QVBoxLayout(listHost);
    m_listLayout->setContentsMargins(8, 8, 8, 8);
    m_listLayout->setSpacing(4);
    m_listLayout->addStretch();
    m_scroll->setWidget(listHost);
    root->addWidget(m_scroll, 1);

    if (m_model) {
        connect(m_model, &ProjectModel::manuscriptsChanged, this, &ManuscriptPanel::onManuscriptsChanged);
        connect(m_model, &ProjectModel::chaptersChanged, this, &ManuscriptPanel::onChaptersChanged);
        connect(m_model, &ProjectModel::loaded, this, [this]() { syncCombo(); rebuildList(); });
    }

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &ManuscriptPanel::applyTheme);

    hide();
}

void ManuscriptPanel::applyTheme() {
    setStyleSheet(Theme::panelQss(QStringLiteral("manuscriptPanel")));
    applyHeaderStyles();
    if (isPanelOpen()) rebuildList();
}

void ManuscriptPanel::applyHeaderStyles() {
    if (m_header) {
        m_header->setStyleSheet(QStringLiteral(
            "#manuscriptHeader { border-bottom: 1px solid %1; }").arg(Theme::panelBorder()));
    }
    if (m_combo) {
        m_combo->setStyleSheet(QStringLiteral(R"(
            QComboBox {
                background: transparent;
                color: %1;
                border: 1px solid %2;
                border-radius: 6px;
                padding: 4px 10px;
                font-family: 'Lora','Crimson Text',serif;
                font-size: 13px;
            }
            QComboBox:hover { border-color: %3; }
            QComboBox::drop-down { border: none; width: 18px; }
            QComboBox QAbstractItemView {
                background: %4;
                color: %1;
                border: 1px solid %2;
                selection-background-color: %5;
            }
        )").arg(Theme::textBright(), Theme::panelBorder(), Theme::subtleBorder(),
               Theme::panelBackground(), Theme::hoverOverlay()));
    }
    for (auto* b : m_headerIconBtns) {
        if (!b) continue;
        b->setStyleSheet(QStringLiteral(R"(
            QToolButton {
                background: transparent;
                color: %1;
                border: 1px solid transparent;
                border-radius: 6px;
                font-size: 15px;
            }
            QToolButton:hover {
                background: %2;
                color: %3;
                border-color: %4;
            }
        )").arg(Theme::textMuted(), Theme::hoverOverlay(), Theme::textBright(), Theme::subtleBorder()));
    }
    if (m_createChapterBtn) {
        m_createChapterBtn->setStyleSheet(createButtonQss(Theme::accentDefault()));
    }
}

void ManuscriptPanel::open() {
    syncCombo();
    rebuildList();
    show();
}

void ManuscriptPanel::closePanel() {
    hide();
    emit panelClosed();
}

QString ManuscriptPanel::activeManuscriptId() const {
    if (!m_combo || m_combo->currentIndex() < 0) return QString();
    return m_combo->currentData().toString();
}

void ManuscriptPanel::syncCombo() {
    if (!m_combo || !m_model) return;
    const QString preserved = m_model->activeManuscriptId().isEmpty()
        ? activeManuscriptId()
        : m_model->activeManuscriptId();

    QSignalBlocker blocker(m_combo);
    m_combo->clear();
    for (const auto& m : m_model->manuscripts()) {
        const QString label = m.title.isEmpty() ? tr("(sem título)") : m.title;
        m_combo->addItem(label, m.id);
    }
    if (m_combo->count() == 0) {
        m_combo->addItem(tr("Nenhum manuscrito"), QString());
        m_combo->setEnabled(false);
    } else {
        m_combo->setEnabled(true);
        int idx = m_combo->findData(preserved);
        m_combo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
}

void ManuscriptPanel::onManuscriptsChanged() {
    syncCombo();
    rebuildList();
}

void ManuscriptPanel::onChaptersChanged() {
    rebuildList();
}

void ManuscriptPanel::onComboChanged(int /*index*/) {
    if (m_model) {
        const QString id = activeManuscriptId();
        if (m_model->activeManuscriptId() != id) {
            m_model->setActiveManuscriptId(id);
        }
    }
    rebuildList();
}

void ManuscriptPanel::rebuildList() {
    if (!m_listLayout) return;
    while (m_listLayout->count() > 1) {
        QLayoutItem* item = m_listLayout->takeAt(0);
        if (auto* w = item->widget()) w->deleteLater();
        delete item;
    }
    if (!m_model) return;

    const QString msId = activeManuscriptId();

    QList<Chapter> filtered;
    for (const auto& c : m_model->chapters()) {
        if (c.manuscriptId == msId) filtered.append(c);
    }
    std::sort(filtered.begin(), filtered.end(), [](const Chapter& a, const Chapter& b) {
        return a.order < b.order;
    });

    if (filtered.isEmpty()) {
        auto* lbl = new QLabel(msId.isEmpty()
            ? tr("Crie um manuscrito pra começar.")
            : tr("Nenhum capítulo ainda."), this);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setWordWrap(true);
        lbl->setStyleSheet(QStringLiteral(
            "color: %1; font-style: italic; padding: 20px 12px;").arg(Theme::textMuted()));
        m_listLayout->insertWidget(0, lbl);
        return;
    }

    const QString chapterQss = QStringLiteral(R"(
        QToolButton {
            background: transparent;
            color: %1;
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 6px 10px;
            text-align: left;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 14px;
        }
        QToolButton:hover {
            background: %2;
            color: %3;
            border-color: %4;
        }
    )").arg(Theme::textPrimary(), Theme::hoverOverlay(), Theme::textBright(), Theme::subtleBorder());

    const QString sceneQss = QStringLiteral(R"(
        QToolButton {
            background: transparent;
            color: %1;
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 4px 10px 4px 26px;
            text-align: left;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 13px;
            font-style: italic;
        }
        QToolButton:hover {
            background: %2;
            color: %3;
        }
    )").arg(Theme::textMuted(), Theme::hoverOverlay(), Theme::textBright());

    int row = 0;
    for (const auto& c : filtered) {
        auto* btn = new QToolButton(this);
        btn->setText(QStringLiteral("%1.  %2").arg(c.order).arg(c.title.isEmpty() ? tr("(sem título)") : c.title));
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(34);
        btn->setStyleSheet(chapterQss);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setContextMenuPolicy(Qt::CustomContextMenu);

        const QString chapterId = c.id;
        const QString manuscriptId = c.manuscriptId;
        connect(btn, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() {
            emit chapterActivated(manuscriptId, chapterId);
        });
        connect(btn, &QToolButton::customContextMenuRequested, this, [this, btn, chapterId](const QPoint& pos) {
            showChapterContextMenu(chapterId, btn->mapToGlobal(pos));
        });
        btn->setProperty("kind", QStringLiteral("chapter"));
        btn->setProperty("chapterId", chapterId);
        btn->installEventFilter(this);
        m_listLayout->insertWidget(row++, btn);

        // Cenas indentadas (só aparecem quando há mais de uma).
        if (c.scenes.size() > 1) {
            for (int i = 0; i < c.scenes.size(); ++i) {
                const Scene& s = c.scenes.at(i);
                auto* sbtn = new QToolButton(this);
                const QString title = s.title.isEmpty()
                    ? tr("Cena %1").arg(i + 1)
                    : s.title;
                sbtn->setText(QStringLiteral("· %1").arg(title));
                sbtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
                sbtn->setCursor(Qt::PointingHandCursor);
                sbtn->setMinimumHeight(26);
                sbtn->setStyleSheet(sceneQss);
                sbtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                sbtn->setContextMenuPolicy(Qt::CustomContextMenu);

                const int sceneIdx = i;
                connect(sbtn, &QToolButton::clicked, this, [this, manuscriptId, chapterId, sceneIdx]() {
                    emit sceneActivated(manuscriptId, chapterId, sceneIdx);
                });
                connect(sbtn, &QToolButton::customContextMenuRequested, this, [this, sbtn, chapterId, sceneIdx](const QPoint& pos) {
                    showSceneContextMenu(chapterId, sceneIdx, sbtn->mapToGlobal(pos));
                });
                sbtn->setProperty("kind", QStringLiteral("scene"));
                sbtn->setProperty("chapterId", chapterId);
                sbtn->setProperty("sceneIndex", sceneIdx);
                sbtn->installEventFilter(this);
                m_listLayout->insertWidget(row++, sbtn);
            }
        }
    }
}

static QString contextMenuQss() {
    return QStringLiteral(R"(
        QMenu {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item { padding: 6px 16px; border-radius: 4px; }
        QMenu::item:selected { background: %4; color: %5; }
        QMenu::item:disabled { color: %6; }
        QMenu::separator { height: 1px; background: %3; margin: 4px 6px; }
    )").arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
           Theme::hoverOverlay(), Theme::textBright(), Theme::textMuted());
}

void ManuscriptPanel::showChapterContextMenu(const QString& chapterId, const QPoint& globalPos) {
    QMenu menu(this);
    menu.setStyleSheet(contextMenuQss());

    auto* exportAct = menu.addAction(tr("Exportar para DOCX…"));
    exportAct->setEnabled(false);
    exportAct->setToolTip(tr("Em breve"));

    auto* renameAct = menu.addAction(tr("Renomear capítulo"));
    connect(renameAct, &QAction::triggered, this, [this, chapterId]() {
        emit renameChapterRequested(chapterId);
    });

    auto* elemsAct = menu.addAction(tr("Elementos presentes…"));
    elemsAct->setEnabled(false);
    elemsAct->setToolTip(tr("Em breve"));

    menu.addSeparator();

    auto* deleteAct = menu.addAction(tr("Excluir capítulo"));
    connect(deleteAct, &QAction::triggered, this, [this, chapterId]() {
        emit deleteChapterRequested(chapterId);
    });

    menu.exec(globalPos);
}

void ManuscriptPanel::showSceneContextMenu(const QString& chapterId, int sceneIndex, const QPoint& globalPos) {
    QMenu menu(this);
    menu.setStyleSheet(contextMenuQss());

    auto* renameAct = menu.addAction(tr("Renomear cena"));
    connect(renameAct, &QAction::triggered, this, [this, chapterId, sceneIndex]() {
        emit renameSceneRequested(chapterId, sceneIndex);
    });

    auto* elemsAct = menu.addAction(tr("Elementos presentes…"));
    elemsAct->setEnabled(false);
    elemsAct->setToolTip(tr("Em breve"));

    auto* varAct = menu.addAction(tr("Criar variação"));
    connect(varAct, &QAction::triggered, this, [this, chapterId, sceneIndex]() {
        emit createVariationRequested(chapterId, sceneIndex);
    });

    menu.addSeparator();

    auto* deleteAct = menu.addAction(tr("Excluir cena"));
    connect(deleteAct, &QAction::triggered, this, [this, chapterId, sceneIndex]() {
        emit deleteSceneRequested(chapterId, sceneIndex);
    });

    menu.exec(globalPos);
}

// ---- Drag&drop ----------------------------------------------------------

bool ManuscriptPanel::eventFilter(QObject* watched, QEvent* event) {
    auto* btn = qobject_cast<QToolButton*>(watched);
    if (!btn) return QWidget::eventFilter(watched, event);
    const QString kind = btn->property("kind").toString();
    if (kind.isEmpty()) return QWidget::eventFilter(watched, event);

    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_dragStartPos = me->pos();
            if (kind == QStringLiteral("chapter")) {
                m_pressedChapterId = btn->property("chapterId").toString();
                m_pressedSceneIndex = -1;
            } else {
                m_pressedChapterId = btn->property("chapterId").toString();
                m_pressedSceneIndex = btn->property("sceneIndex").toInt();
            }
        }
    } else if (event->type() == QEvent::MouseMove) {
        auto* me = static_cast<QMouseEvent*>(event);
        if ((me->buttons() & Qt::LeftButton) && !m_pressedChapterId.isEmpty()) {
            if ((me->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                if (m_pressedSceneIndex < 0) {
                    const QString id = m_pressedChapterId;
                    m_pressedChapterId.clear();
                    startChapterDrag(btn, id);
                } else {
                    const QString chId = m_pressedChapterId;
                    const int sIdx = m_pressedSceneIndex;
                    m_pressedChapterId.clear();
                    m_pressedSceneIndex = -1;
                    startSceneDrag(btn, chId, sIdx);
                }
                return true;
            }
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        m_pressedChapterId.clear();
        m_pressedSceneIndex = -1;
    }
    return QWidget::eventFilter(watched, event);
}

void ManuscriptPanel::startChapterDrag(QWidget* source, const QString& chapterId) {
    QDrag* drag = new QDrag(source);
    auto* mime = new QMimeData();
    mime->setData(QLatin1String(kChapterMime), chapterId.toUtf8());
    drag->setMimeData(mime);
    const QPixmap pm = source->grab();
    drag->setPixmap(pm);
    drag->setHotSpot(QPoint(pm.width() / 2, pm.height() / 2));
    drag->exec(Qt::MoveAction);
    clearDropIndicator();
}

void ManuscriptPanel::startSceneDrag(QWidget* source, const QString& chapterId, int sceneIndex) {
    QDrag* drag = new QDrag(source);
    auto* mime = new QMimeData();
    // Payload: "chapterId|sceneIndex"
    const QByteArray payload = (chapterId + QLatin1Char('|') + QString::number(sceneIndex)).toUtf8();
    mime->setData(QLatin1String(kSceneMime), payload);
    drag->setMimeData(mime);
    const QPixmap pm = source->grab();
    drag->setPixmap(pm);
    drag->setHotSpot(QPoint(pm.width() / 2, pm.height() / 2));
    drag->exec(Qt::MoveAction);
    clearDropIndicator();
}

void ManuscriptPanel::clearDropIndicator() {
    if (m_dropIndicator) {
        m_dropIndicator->hide();
        m_dropIndicator->deleteLater();
        m_dropIndicator = nullptr;
    }
}

void ManuscriptPanel::showDropIndicatorAt(QWidget* target, bool before) {
    if (!target) return;
    if (!m_dropIndicator) {
        m_dropIndicator = new QWidget(target->parentWidget());
        m_dropIndicator->setFixedHeight(2);
        m_dropIndicator->setStyleSheet(QStringLiteral(
            "background: %1; border-radius: 1px;").arg(Theme::accentDefault()));
        m_dropIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    } else if (m_dropIndicator->parentWidget() != target->parentWidget()) {
        m_dropIndicator->setParent(target->parentWidget());
    }
    const int y = before ? target->y() - 1 : target->y() + target->height();
    m_dropIndicator->setGeometry(target->x() + 4, y, target->width() - 8, 2);
    m_dropIndicator->raise();
    m_dropIndicator->show();
}

// Decompõe payload de cena.
static bool parseScenePayload(const QByteArray& payload, QString& chapterId, int& sceneIndex) {
    const QString s = QString::fromUtf8(payload);
    const int sep = s.indexOf(QLatin1Char('|'));
    if (sep < 0) return false;
    chapterId = s.left(sep);
    bool ok = false;
    sceneIndex = s.mid(sep + 1).toInt(&ok);
    return ok && !chapterId.isEmpty();
}

void ManuscriptPanel::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(QLatin1String(kChapterMime)) ||
        event->mimeData()->hasFormat(QLatin1String(kSceneMime))) {
        event->acceptProposedAction();
    }
}

void ManuscriptPanel::dragMoveEvent(QDragMoveEvent* event) {
    if (!m_listLayout) return;
    if (!event->mimeData()->hasFormat(QLatin1String(kChapterMime)) &&
        !event->mimeData()->hasFormat(QLatin1String(kSceneMime))) return;

    // Encontra o widget mais próximo do cursor dentro do listLayout.
    const QPoint posInPanel = event->position().toPoint();
    QWidget* hostWidget = m_scroll ? m_scroll->widget() : nullptr;
    if (!hostWidget) return;
    const QPoint posInHost = hostWidget->mapFrom(this, posInPanel);

    QWidget* nearest = nullptr;
    int nearestDist = INT_MAX;
    for (int i = 0; i < m_listLayout->count(); ++i) {
        QLayoutItem* it = m_listLayout->itemAt(i);
        if (!it) continue;
        QWidget* w = it->widget();
        if (!w) continue;
        const int midY = w->y() + w->height() / 2;
        const int d = qAbs(midY - posInHost.y());
        if (d < nearestDist) { nearestDist = d; nearest = w; }
    }
    if (!nearest) return;

    const bool isSceneDrag = event->mimeData()->hasFormat(QLatin1String(kSceneMime));
    if (isSceneDrag) {
        // Cenas: só pode soltar perto de outras cenas do MESMO capítulo (mesma kind+chapterId).
        QString srcChId; int srcIdx = -1;
        parseScenePayload(event->mimeData()->data(QLatin1String(kSceneMime)), srcChId, srcIdx);
        if (nearest->property("kind").toString() != QStringLiteral("scene") ||
            nearest->property("chapterId").toString() != srcChId) {
            clearDropIndicator();
            event->ignore();
            return;
        }
    } else {
        // Capítulos: alvo precisa ser um botão de capítulo.
        if (nearest->property("kind").toString() != QStringLiteral("chapter")) {
            clearDropIndicator();
            event->ignore();
            return;
        }
    }

    const QPoint posInTarget = nearest->mapFrom(hostWidget, posInHost);
    const bool before = posInTarget.y() < nearest->height() / 2;
    showDropIndicatorAt(nearest, before);
    event->acceptProposedAction();
}

void ManuscriptPanel::dragLeaveEvent(QDragLeaveEvent* /*event*/) {
    clearDropIndicator();
}

void ManuscriptPanel::dropEvent(QDropEvent* event) {
    if (!m_listLayout) return;
    QWidget* hostWidget = m_scroll ? m_scroll->widget() : nullptr;
    if (!hostWidget) { clearDropIndicator(); return; }
    const QPoint posInPanel = event->position().toPoint();
    const QPoint posInHost = hostWidget->mapFrom(this, posInPanel);

    // Encontra alvo
    QWidget* nearest = nullptr;
    int nearestDist = INT_MAX;
    for (int i = 0; i < m_listLayout->count(); ++i) {
        QLayoutItem* it = m_listLayout->itemAt(i);
        if (!it) continue;
        QWidget* w = it->widget();
        if (!w) continue;
        const int midY = w->y() + w->height() / 2;
        const int d = qAbs(midY - posInHost.y());
        if (d < nearestDist) { nearestDist = d; nearest = w; }
    }
    clearDropIndicator();
    if (!nearest) return;

    const QPoint posInTarget = nearest->mapFrom(hostWidget, posInHost);
    const bool before = posInTarget.y() < nearest->height() / 2;

    if (event->mimeData()->hasFormat(QLatin1String(kSceneMime))) {
        QString srcChId; int srcIdx = -1;
        if (!parseScenePayload(event->mimeData()->data(QLatin1String(kSceneMime)), srcChId, srcIdx)) return;
        if (nearest->property("kind").toString() != QStringLiteral("scene") ||
            nearest->property("chapterId").toString() != srcChId) return;
        int tgtIdx = nearest->property("sceneIndex").toInt();
        if (!before) tgtIdx += 1;
        if (tgtIdx == srcIdx || tgtIdx == srcIdx + 1) { event->acceptProposedAction(); return; }
        emit reorderSceneRequested(srcChId, srcIdx, tgtIdx);
        event->acceptProposedAction();
        return;
    }

    if (event->mimeData()->hasFormat(QLatin1String(kChapterMime))) {
        const QString srcChId = QString::fromUtf8(event->mimeData()->data(QLatin1String(kChapterMime)));
        if (nearest->property("kind").toString() != QStringLiteral("chapter")) return;
        // Coleta a lista atual de capítulos visíveis pra calcular targetIndex local.
        QStringList orderedIds;
        for (int i = 0; i < m_listLayout->count(); ++i) {
            QLayoutItem* it = m_listLayout->itemAt(i);
            if (!it || !it->widget()) continue;
            if (it->widget()->property("kind").toString() == QStringLiteral("chapter")) {
                orderedIds.append(it->widget()->property("chapterId").toString());
            }
        }
        const QString tgtChId = nearest->property("chapterId").toString();
        int tgtIdx = orderedIds.indexOf(tgtChId);
        if (tgtIdx < 0) return;
        if (!before) tgtIdx += 1;
        emit reorderChapterRequested(srcChId, tgtIdx);
        event->acceptProposedAction();
        return;
    }
}
