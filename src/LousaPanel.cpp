#include "LousaPanel.h"

#include "LousaScene.h"
#include "LousaView.h"
#include "Theme.h"

#include <QCloseEvent>
#include <QColorDialog>
#include <QDir>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSaveFile>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kToolbarH = 46;

QToolButton* makeBtn(const QString& text, const QString& tip, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setText(text);
    b->setToolTip(tip);
    b->setObjectName(QStringLiteral("lousaToolBtn"));
    b->setCursor(Qt::PointingHandCursor);
    b->setEnabled(false); // habilitado conforme cada grupo é implementado
    return b;
}
} // namespace

LousaPanel::LousaPanel(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("lousaPanel"));
    setWindowTitle(tr("Lousa"));
    setMinimumSize(640, 420);
    resize(960, 660);
    buildUi();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &LousaPanel::applyTheme);
}

void LousaPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar ──────────────────────────────────────────────────────────
    m_toolbar = new QWidget(this);
    m_toolbar->setObjectName(QStringLiteral("lousaToolbar"));
    m_toolbar->setFixedHeight(kToolbarH);

    auto* tl = new QHBoxLayout(m_toolbar);
    tl->setContentsMargins(10, 0, 10, 0);
    tl->setSpacing(4);

    // Grupo 1 — tipos de card (desabilitados agora, ligados conforme Grupo 1)
    auto* btnNote  = makeBtn(tr("Post-it"),     tr("Adicionar post-it"),      m_toolbar);
    auto* btnCmt   = makeBtn(tr("Comentário"),  tr("Adicionar comentário"),   m_toolbar);
    auto* btnImg   = makeBtn(tr("Imagem"),      tr("Adicionar imagem"),       m_toolbar);
    auto* btnDoc   = makeBtn(tr("Documento"),   tr("Vincular documento"),     m_toolbar);
    auto* btnChar  = makeBtn(tr("Personagem"),  tr("Adicionar personagem"),   m_toolbar);
    tl->addWidget(btnNote);
    tl->addWidget(btnCmt);
    tl->addWidget(btnImg);
    tl->addWidget(btnDoc);
    tl->addWidget(btnChar);

    // Separador
    auto* sep1 = new QFrame(m_toolbar);
    sep1->setObjectName(QStringLiteral("lousaSep"));
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFixedHeight(22);
    tl->addWidget(sep1);

    // Grupo 3 — zona (desabilitado)
    auto* btnZone = makeBtn(tr("Zona"), tr("Definir área"), m_toolbar);
    tl->addWidget(btnZone);

    tl->addStretch(1);

    // Cor do canvas
    m_colorBtn = new QToolButton(m_toolbar);
    m_colorBtn->setObjectName(QStringLiteral("lousaColorBtn"));
    m_colorBtn->setToolTip(tr("Cor do canvas"));
    m_colorBtn->setFixedSize(26, 26);
    m_colorBtn->setCursor(Qt::PointingHandCursor);
    connect(m_colorBtn, &QToolButton::clicked, this, &LousaPanel::onPickColor);
    tl->addWidget(m_colorBtn);

    // Separador
    auto* sep2 = new QFrame(m_toolbar);
    sep2->setObjectName(QStringLiteral("lousaSep"));
    sep2->setFrameShape(QFrame::VLine);
    sep2->setFixedHeight(22);
    tl->addWidget(sep2);

    // Label de zoom (decorativo)
    m_zoomLabel = new QLabel(QStringLiteral("100%"), m_toolbar);
    m_zoomLabel->setObjectName(QStringLiteral("lousaZoomLabel"));
    m_zoomLabel->setFixedWidth(44);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    tl->addWidget(m_zoomLabel);

    // Fechar
    auto* closeBtn = new QToolButton(m_toolbar);
    closeBtn->setObjectName(QStringLiteral("lousaCloseBtn"));
    closeBtn->setText(QStringLiteral("×"));
    closeBtn->setToolTip(tr("Fechar lousa"));
    closeBtn->setFixedSize(30, 30);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QToolButton::clicked, this, &LousaPanel::close);
    tl->addWidget(closeBtn);

    root->addWidget(m_toolbar);

    // ── Canvas ───────────────────────────────────────────────────────────
    m_scene = new LousaScene(this);
    m_view  = new LousaView(m_scene, this);
    root->addWidget(m_view, 1);

    connect(m_view, &LousaView::zoomChanged, this, [this](qreal z) {
        if (m_zoomLabel) m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(z * 100)));
        save();
    });

    // ── Placeholder ──────────────────────────────────────────────────────
    m_emptyLabel = new QLabel(tr("Clique em + para adicionar um card à lousa"), this);
    m_emptyLabel->setObjectName(QStringLiteral("lousaEmpty"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_emptyLabel->setVisible(true);

    connect(m_scene, &QGraphicsScene::changed, this, [this](const QList<QRectF>&) {
        refreshEmptyState();
    });
}

void LousaPanel::refreshEmptyState()
{
    if (!m_emptyLabel) return;
    const bool empty = m_scene->items().isEmpty();
    m_emptyLabel->setVisible(empty);
    if (empty) {
        m_emptyLabel->adjustSize();
        m_emptyLabel->move(
            (width()  - m_emptyLabel->width())  / 2,
            (height() - m_emptyLabel->height()) / 2 + kToolbarH / 2);
    }
}

void LousaPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    refreshEmptyState();
}

void LousaPanel::closeEvent(QCloseEvent* event)
{
    // Esconde em vez de destruir — o estado fica vivo entre aberturas.
    hide();
    event->ignore();
    emit closeRequested();
}

void LousaPanel::setProjectRoot(const QString& root)
{
    if (m_projectRoot == root) return;
    m_projectRoot = root;
    load();
}

void LousaPanel::load()
{
    if (m_projectRoot.isEmpty() || !m_scene || !m_view) return;
    const QString path = QDir::cleanPath(m_projectRoot + QStringLiteral("/canvas.json"));
    QFile f(path);
    if (!f.exists()) return;
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    const QColor color(root.value(QStringLiteral("canvasColor")).toString(QStringLiteral("#1a1a2e")));
    m_scene->setCanvasColor(color.isValid() ? color : QColor(QStringLiteral("#1a1a2e")));
    updateColorBtn();

    const qreal zoom = root.value(QStringLiteral("zoom")).toDouble(1.0);
    const qreal panX = root.value(QStringLiteral("panX")).toDouble(0.0);
    const qreal panY = root.value(QStringLiteral("panY")).toDouble(0.0);
    m_view->applyZoomAndPan(zoom, panX, panY);
    if (m_zoomLabel) m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(zoom * 100)));
}

void LousaPanel::save() const
{
    if (m_projectRoot.isEmpty() || !m_scene || !m_view) return;

    QJsonObject root;
    root.insert(QStringLiteral("canvasColor"), m_scene->canvasColor().name());
    root.insert(QStringLiteral("zoom"), m_view->zoomFactor());
    root.insert(QStringLiteral("panX"), m_view->scrollPos().x());
    root.insert(QStringLiteral("panY"), m_view->scrollPos().y());
    root.insert(QStringLiteral("cards"),       QJsonArray{});
    root.insert(QStringLiteral("connections"), QJsonArray{});
    root.insert(QStringLiteral("zones"),       QJsonArray{});

    QSaveFile f(QDir::cleanPath(m_projectRoot + QStringLiteral("/canvas.json")));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.commit();
}

void LousaPanel::onPickColor()
{
    if (!m_scene) return;
    const QColor chosen = QColorDialog::getColor(
        m_scene->canvasColor(), this, tr("Cor do canvas"),
        QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid()) return;
    m_scene->setCanvasColor(chosen);
    updateColorBtn();
    save();
}

void LousaPanel::updateColorBtn()
{
    if (!m_colorBtn || !m_scene) return;
    const QColor c = m_scene->canvasColor();
    m_colorBtn->setStyleSheet(QStringLiteral(
        "QToolButton#lousaColorBtn {"
        "  background: %1; border: 2px solid %2; border-radius: 5px;"
        "}"
        "QToolButton#lousaColorBtn:hover { border-color: %3; }"
    ).arg(c.name(), Theme::panelBorder(), Theme::textPrimary()));
}

void LousaPanel::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QWidget#lousaPanel   { background: transparent; }"
        "QWidget#lousaToolbar { background: %1; border-bottom: 1px solid %2; }"
        "QToolButton#lousaToolBtn {"
        "  background: transparent; border: 1px solid transparent;"
        "  border-radius: 6px; color: %3; padding: 4px 8px; font-size: 11px;"
        "}"
        "QToolButton#lousaToolBtn:enabled:hover { background: %4; border-color: %5; }"
        "QToolButton#lousaToolBtn:disabled { color: %6; }"
        "QToolButton#lousaCloseBtn {"
        "  background: transparent; border: none; color: %3; font-size: 18px;"
        "}"
        "QToolButton#lousaCloseBtn:hover { color: %7; }"
        "QFrame#lousaSep { color: %2; background: %2; }"
        "QLabel#lousaZoomLabel { color: %6; font-size: 11px; }"
        "QLabel#lousaEmpty { color: %6; font-size: 14px; }"
    ).arg(Theme::panelBackground(),   // 1
         Theme::panelBorder(),        // 2
         Theme::textPrimary(),        // 3
         Theme::hoverOverlay(),       // 4
         Theme::subtleBorder(),       // 5
         Theme::textMuted(),          // 6
         Theme::accentDanger()));     // 7

    updateColorBtn();

    if (m_view) {
        m_view->setStyleSheet(QStringLiteral(
            "QGraphicsView { border: none; background: transparent; }"));
    }
}
