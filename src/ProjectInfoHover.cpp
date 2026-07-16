#include "ProjectInfoHover.h"

#include "ProjectModel.h"
#include "Theme.h"

#include <QApplication>
#include <QByteArray>
#include <QEnterEvent>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPixmap>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr int kPanelWidth = 320;
constexpr int kCoverW = 280;
constexpr int kCoverH = 420;

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

ProjectInfoHover::ProjectInfoHover(ProjectModel* model, QWidget* parent)
    : QFrame(parent)
    , m_model(model)
{
    setObjectName(QStringLiteral("projectInfoHover"));
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(28);
    shadow->setColor(QColor(0, 0, 0, 200));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    setFixedWidth(kPanelWidth);
    buildUi();
    applyPanelStyle();

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &ProjectInfoHover::applyPanelStyle);
}

void ProjectInfoHover::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    m_cover = new QLabel(this);
    m_cover->setObjectName(QStringLiteral("projectInfoHoverCover"));
    m_cover->setFixedSize(kCoverW, kCoverH);
    m_cover->setAlignment(Qt::AlignCenter);
    m_cover->setScaledContents(false);
    root->addWidget(m_cover, 0, Qt::AlignHCenter);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName(QStringLiteral("projectInfoHoverName"));
    m_nameLabel->setWordWrap(true);
    root->addWidget(m_nameLabel);

    m_authorLabel = new QLabel(this);
    m_authorLabel->setObjectName(QStringLiteral("projectInfoHoverMeta"));
    m_authorLabel->setWordWrap(true);
    m_authorLabel->setTextFormat(Qt::RichText);
    root->addWidget(m_authorLabel);

    m_genresLabel = new QLabel(this);
    m_genresLabel->setObjectName(QStringLiteral("projectInfoHoverMeta"));
    m_genresLabel->setWordWrap(true);
    m_genresLabel->setTextFormat(Qt::RichText);
    root->addWidget(m_genresLabel);

    // Sinopse fica num scroll com altura máxima — texto longo não estoura o painel.
    m_synopsisLabel = new QLabel;
    m_synopsisLabel->setObjectName(QStringLiteral("projectInfoHoverSynopsis"));
    m_synopsisLabel->setWordWrap(true);
    m_synopsisLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_synopsisLabel->setContentsMargins(0, 0, 6, 0);

    m_synopsisScroll = new QScrollArea(this);
    m_synopsisScroll->setObjectName(QStringLiteral("projectInfoHoverScroll"));
    m_synopsisScroll->setWidget(m_synopsisLabel);
    m_synopsisScroll->setWidgetResizable(true);
    m_synopsisScroll->setFrameShape(QFrame::NoFrame);
    m_synopsisScroll->setMaximumHeight(180);
    m_synopsisScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(m_synopsisScroll, 1);
}

void ProjectInfoHover::refreshFromModel() {
    if (!m_model) return;

    const QString name = m_model->projectName();
    const QString author = m_model->projectAuthor();
    const QString genres = m_model->projectGenres();
    const QString synopsis = m_model->projectSynopsis();
    const QString coverUrl = m_model->projectCoverDataUrl();

    m_nameLabel->setText(name.isEmpty() ? tr("(sem nome)") : name);

    if (author.isEmpty()) m_authorLabel->hide();
    else {
        m_authorLabel->show();
        m_authorLabel->setText(QStringLiteral("<b>%1</b> %2")
            .arg(tr("Autor:"), author.toHtmlEscaped()));
    }

    if (genres.isEmpty()) m_genresLabel->hide();
    else {
        m_genresLabel->show();
        m_genresLabel->setText(QStringLiteral("<b>%1</b> %2")
            .arg(tr("Gêneros:"), genres.toHtmlEscaped()));
    }

    if (synopsis.isEmpty()) {
        m_synopsisLabel->setText(tr("Sem sinopse."));
    } else {
        m_synopsisLabel->setText(synopsis);
    }

    QPixmap pm = pixmapFromDataUrl(coverUrl);
    if (pm.isNull()) {
        m_cover->clear();
        m_cover->setText(tr("Sem capa"));
    } else {
        m_cover->setPixmap(pm.scaled(kCoverW, kCoverH,
                                     Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void ProjectInfoHover::presentNear(const QPoint& anchorGlobalTopRight) {
    refreshFromModel();
    adjustSize();

    // Posiciona com 8px de gap à direita da âncora; clampa ao screen.
    const QSize ps = size();
    QPoint pos = anchorGlobalTopRight + QPoint(8, 0);
    if (auto* scr = QApplication::screenAt(anchorGlobalTopRight)) {
        const QRect g = scr->availableGeometry();
        if (pos.x() + ps.width() > g.right()) pos.setX(g.right() - ps.width() - 8);
        if (pos.y() + ps.height() > g.bottom()) pos.setY(g.bottom() - ps.height() - 8);
        if (pos.y() < g.top()) pos.setY(g.top() + 8);
    }
    move(pos);
    show();
    raise();
}

void ProjectInfoHover::enterEvent(QEnterEvent* event) {
    QFrame::enterEvent(event);
}

void ProjectInfoHover::leaveEvent(QEvent* event) {
    QFrame::leaveEvent(event);
    hide();
    emit hoverLeft();
}

void ProjectInfoHover::applyPanelStyle() {
    setStyleSheet(QStringLiteral(R"(
        QFrame#projectInfoHover {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }
        QLabel#projectInfoHoverCover {
            background: %3;
            color: %4;
            border: 1px solid %2;
            border-radius: 6px;
            font-size: 11px;
        }
        QLabel#projectInfoHoverName {
            color: %5;
            font-size: 15px;
            font-weight: 600;
        }
        QLabel#projectInfoHoverMeta {
            color: %6;
            font-size: 11px;
        }
        QLabel#projectInfoHoverSynopsis {
            color: %6;
            font-size: 11px;
            line-height: 140%;
        }
        QScrollArea#projectInfoHoverScroll {
            background: transparent;
            border: none;
        }
        QScrollArea#projectInfoHoverScroll QWidget {
            background: transparent;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
        }
        QScrollBar::handle:vertical {
            background: %2;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
    )").arg(
        Theme::panelBackground(),   // 1
        Theme::panelBorder(),       // 2
        Theme::inputBackground(),   // 3
        Theme::textMuted(),         // 4
        Theme::textBright(),        // 5
        Theme::textPrimary()        // 6
    ));
}
