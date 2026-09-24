#include "RefMenuPanel.h"
#include "DocPreview.h"

#include "ConstrutorStore.h"
#include "TerritorioStore.h"
#include "DocCache.h"
#include "DocPreview.h"
#include "EditorHost.h"
#include "ElementsStore.h"
#include "FindBar.h"
#include "AvatarUtils.h"
#include "IconUtils.h"
#include "MarkerStore.h"
#include "MemoriesStore.h"
#include "ProjectModel.h"
#include "RoleTiers.h"
#include "ProjectStorage.h"
#include "SceneUtils.h"
#include "Theme.h"
#include "WordCounter.h"

#include <QApplication>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QByteArray>
#include <QEvent>
#include <functional>
#include <QFile>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSplitter>
#include <QTabBar>
#include <QLocale>
#include <QToolTip>
#include <QScrollBar>
#include <QStyle>
#include <QSettings>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTimer>
#include <QUrl>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QActionGroup>
#include <algorithm>

namespace {
constexpr int kDefaultW = 380;
constexpr int kDefaultH = 720;
constexpr int kMinW = 280;
constexpr int kMinH = 360;
constexpr int kHandleW = 6;     // espessura das faixas de resize
constexpr int kCornerSz = 12;   // tamanho dos handles de canto
constexpr int kHeaderH = 38;

const char* kKeyGeom        = "ui/refMenuPanel/geometry";
const char* kKeySplit       = "ui/refMenuPanel/bodySplit";
const char* kKeyNavHidden   = "ui/refMenuPanel/navHidden";
const char* kKeyVisualMode  = "ui/refMenuPanel/visualMode";
const char* kKeyPinned      = "ui/refMenuPanel/pinned";
const char* kKeyFontPt      = "ui/refMenuPanel/previewFontPt";
// Estilo de layout (global) e o que cada estilo lembra por conta própria.
const char* kKeyLayout      = "ui/refMenuPanel/layout";
const char* kKeyNavRight    = "ui/refMenuPanel/navRight";
const char* kKeyNavColW     = "ui/refMenuPanel/navColW";

// keys de seleção (UserRole nos QListWidgetItem):
//   ms:<msId>            → manuscrito
//   ch:<msId>:<chId>     → capítulo
//   sc:<msId>:<chId>:<i> → cena
//   it:<itemId>          → item de gaveta
//   fl:<folderId>        → folder (drilling)
}


// =========================================================================
// Widgets desenhados dos estilos (Mapa de Uso, Fichário, Livro aberto)
// =========================================================================

// Mapa de Uso: cada linha é um personagem/lugar (na cor da gaveta), cada
// coluna um capítulo. Bolinha = aparece ali; traço cheio liga capítulos
// seguidos; tracejado atravessa uma ausência.
class RefUsageMap : public QWidget {
public:
    struct Row { QString id, name, key; QColor color; QList<int> cols; };
    QList<Row> rows;
    QStringList colKeys, colTitles;
    int editorCol = -1, selCol = -1;
    QString selRow;
    QString chapterWord;                          // "capítulo", já traduzido pelo painel
    std::function<void(const QString&)> onOpen;   // abre um documento
    std::function<void(const QString&)> onRow;    // destaca uma linha ("" limpa)

    explicit RefUsageMap(QWidget* parent) : QWidget(parent) { setMouseTracking(true); }

    void relayout(int availW)
    {
        const QFontMetrics fm(font());
        m_nameW = 60;
        for (const Row& r : rows) m_nameW = qMax(m_nameW, fm.horizontalAdvance(r.name));
        m_nameW = qMin(m_nameW, 160);
        const int n = int(colKeys.size());
        m_dx = n > 1 ? qBound(30, (availW - x0() - 24) / qMax(1, n - 1), 90) : 44;
        setFixedSize(x0() + m_dx * qMax(0, n - 1) + 30, kY0 + kDy * int(rows.size()) + 8);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QColor muted = Theme::toColor(Theme::textMuted());
        const QColor prim = Theme::toColor(Theme::textPrimary());
        const QColor bright = Theme::toColor(Theme::textBright());
        const QColor panel = Theme::toColor(Theme::panelBackground());
        auto band = [&](int col, QColor c) {
            if (col < 0 || col >= colKeys.size()) return;
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRoundedRect(QRectF(colX(col) - 14, 2, 28, height() - 4), 5, 5);
        };
        QColor ed = Theme::toColor(Theme::accentDefault()); ed.setAlpha(34);
        QColor sl = Theme::toColor(Theme::accentInfo());    sl.setAlpha(52);
        band(editorCol, ed);
        band(selCol, sl);

        QFont f = font();
        for (int i = 0; i < colKeys.size(); ++i) {
            QFont hf = f;
            hf.setPixelSize(11);
            hf.setBold(i == selCol);
            p.setFont(hf);
            p.setPen(i == selCol ? bright : muted);
            p.drawText(QRect(colX(i) - 20, 2, 40, 18), Qt::AlignCenter, QString::number(i + 1));
            QColor g = prim; g.setAlpha(18);
            p.setPen(QPen(g, 1));
            p.drawLine(QPointF(colX(i), 22), QPointF(colX(i), height() - 2));
        }

        for (int r = 0; r < rows.size(); ++r) {
            const Row& row = rows.at(r);
            const qreal y = rowY(r);
            p.setOpacity(!selRow.isEmpty() && selRow != row.id ? 0.28 : 1.0);
            QFont nf = f;
            nf.setPixelSize(12);
            p.setFont(nf);
            p.setPen(selRow == row.id ? bright : prim);
            p.drawText(QRect(0, int(y) - 10, x0() - 14, 20), Qt::AlignRight | Qt::AlignVCenter,
                       QFontMetrics(nf).elidedText(row.name, Qt::ElideRight, x0() - 18));
            for (int i = 0; i + 1 < row.cols.size(); ++i) {
                const int a = row.cols.at(i), b = row.cols.at(i + 1);
                if (b == a + 1) {
                    p.setPen(QPen(row.color, 4, Qt::SolidLine, Qt::RoundCap));
                } else {
                    QColor c = row.color; c.setAlpha(140);
                    QPen pen(c, 2, Qt::CustomDashLine, Qt::RoundCap);
                    pen.setDashPattern({ 1, 3 });
                    p.setPen(pen);
                }
                p.drawLine(QPointF(colX(a), y), QPointF(colX(b), y));
            }
            for (int c : row.cols) {
                const bool on = (row.id == selRow && c == selCol);
                p.setPen(QPen(row.color, on ? 3.5 : 2.5));
                p.setBrush(panel);
                const qreal rad = on ? 7 : 5.5;
                p.drawEllipse(QPointF(colX(c), y), rad, rad);
            }
            p.setOpacity(1.0);
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        const Hit h = hitAt(e->position().toPoint());
        setCursor(h.kind != HitNone ? Qt::PointingHandCursor : Qt::ArrowCursor);
        if (h.kind == HitStation) {
            QToolTip::showText(e->globalPosition().toPoint(),
                QStringLiteral("%1 · %2 %3 — %4").arg(rows.at(h.row).name, chapterWord)
                    .arg(h.col + 1).arg(colTitles.value(h.col)), this);
        } else if (h.kind == HitHeader) {
            QToolTip::showText(e->globalPosition().toPoint(), colTitles.value(h.col), this);
        } else {
            QToolTip::hideText();
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        const Hit h = hitAt(e->position().toPoint());
        switch (h.kind) {
        case HitHeader:
            if (onOpen) onOpen(colKeys.at(h.col));
            break;
        case HitName:
            if (onRow) onRow(rows.at(h.row).id);
            if (onOpen && !rows.at(h.row).key.isEmpty()) onOpen(rows.at(h.row).key);
            break;
        case HitStation:
            if (onRow) onRow(rows.at(h.row).id);
            if (onOpen) onOpen(colKeys.at(h.col));
            break;
        case HitNone:
            if (onRow) onRow(QString());
            break;
        }
    }

private:
    static constexpr int kY0 = 26;
    static constexpr int kDy = 24;
    int m_nameW = 100, m_dx = 44;
    int x0() const { return m_nameW + 26; }
    qreal colX(int i) const { return x0() + m_dx * i; }
    qreal rowY(int r) const { return kY0 + kDy * r + kDy / 2.0; }

    enum HitKind { HitNone, HitHeader, HitName, HitStation };
    struct Hit { HitKind kind = HitNone; int row = -1; int col = -1; };
    Hit hitAt(const QPoint& pt) const
    {
        Hit h;
        if (pt.y() < 22) {
            for (int i = 0; i < colKeys.size(); ++i)
                if (qAbs(pt.x() - colX(i)) <= 14) { h.kind = HitHeader; h.col = i; return h; }
            return h;
        }
        const int r = int((pt.y() - kY0) / kDy);
        if (r < 0 || r >= rows.size()) return h;
        if (pt.x() < x0() - 8) { h.kind = HitName; h.row = r; return h; }
        for (int c : rows.at(r).cols) {
            if (QLineF(pt, QPointF(colX(c), rowY(r))).length() <= 9) {
                h.kind = HitStation; h.row = r; h.col = c; return h;
            }
        }
        return h;
    }
};

// Fichário: as divisórias coloridas na borda direita, com o nome na vertical.
class RefDividers : public QWidget {
public:
    struct Tab { QString filter, label; QColor color; };
    QList<Tab> tabs;
    QString current;
    std::function<void(const QString&)> onPick;

    explicit RefDividers(QWidget* parent) : QWidget(parent)
    {
        setMouseTracking(true);
        setFixedWidth(34);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), Theme::toColor(Theme::appBackground()));
        QColor line = Theme::toColor(Theme::subtleBorder());
        p.setPen(QPen(line, 1));
        p.drawLine(0, 0, 0, height());
        QFont f = font();
        f.setPixelSize(11);
        f.setBold(true);
        p.setFont(f);
        const QFontMetrics fm(f);
        m_rects.clear();
        int y = 14;
        for (int i = 0; i < tabs.size(); ++i) {
            const Tab& t = tabs.at(i);
            const int h = fm.horizontalAdvance(t.label) + 24;
            const bool on = (t.filter == current);
            const int w = on ? 34 : (i == m_hover ? 30 : 26);
            const QRectF r(0, y, w, h);
            m_rects.append(r.toRect());
            QPainterPath path;
            path.moveTo(0, y);
            path.lineTo(w - 6, y);
            path.quadTo(w, y, w, y + 6);
            path.lineTo(w, y + h - 6);
            path.quadTo(w, y + h, w - 6, y + h);
            path.lineTo(0, y + h);
            path.closeSubpath();
            QColor fill = t.color;
            if (!on) fill = fill.darker(112);
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawPath(path);
            // Texto com contraste pela luminância da própria divisória.
            const qreal lum = 0.2126 * fill.redF() + 0.7152 * fill.greenF() + 0.0722 * fill.blueF();
            p.setPen(lum > 0.55 ? QColor(20, 20, 20, 220) : QColor(250, 248, 242, 235));
            p.save();
            p.translate(w / 2.0, y + h / 2.0);
            p.rotate(90);
            p.drawText(QRectF(-h / 2.0, -w / 2.0, h, w), Qt::AlignCenter, t.label);
            p.restore();
            y += h + 5;
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        int hov = -1;
        for (int i = 0; i < m_rects.size(); ++i)
            if (m_rects.at(i).adjusted(0, 0, 8, 0).contains(e->position().toPoint())) hov = i;
        if (hov != m_hover) { m_hover = hov; update(); }
        setCursor(hov >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        if (hov >= 0) QToolTip::showText(e->globalPosition().toPoint(), tabs.at(hov).label, this);
    }
    void leaveEvent(QEvent*) override { m_hover = -1; update(); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        for (int i = 0; i < m_rects.size(); ++i)
            if (m_rects.at(i).adjusted(0, 0, 8, 0).contains(e->position().toPoint())) {
                if (onPick) onPick(tabs.at(i).filter);
                return;
            }
    }

private:
    QList<QRect> m_rects;
    int m_hover = -1;
};

namespace {

// Fichário: as argolas na borda esquerda. Só desenho.
class RefRings : public QWidget {
public:
    explicit RefRings(QWidget* parent) : QWidget(parent) { setFixedWidth(26); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), Theme::toColor(Theme::appBackground()));
        p.setPen(QPen(Theme::toColor(Theme::subtleBorder()), 1));
        p.drawLine(width() - 1, 0, width() - 1, height());
        const QColor hole = Theme::toColor(Theme::panelBackground());
        const QColor rim = Theme::toColor(Theme::panelBorder());
        for (int i = 1; i <= 3; ++i) {
            const qreal y = height() * i / 4.0;
            p.setPen(QPen(rim, 1.2));
            p.setBrush(hole);
            p.drawEllipse(QPointF(width() / 2.0, y), 6, 6);
            QColor shade(0, 0, 0, 70);
            p.setPen(Qt::NoPen);
            p.setBrush(shade);
            p.drawEllipse(QPointF(width() / 2.0, y - 1.2), 4.2, 3.2);
        }
    }
};

// Livro aberto: as reticências entre o título e a contagem.
class RefLeader : public QWidget {
public:
    explicit RefLeader(QWidget* parent) : QWidget(parent)
    {
        setMinimumWidth(12);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        QColor c = Theme::toColor(Theme::textMuted());
        c.setAlpha(110);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const qreal y = height() - 6.5;
        for (qreal x = 2; x < width() - 2; x += 4.5) p.drawEllipse(QPointF(x, y), 0.8, 0.8);
    }
};

} // namespace

// =========================================================================
// Construção e ciclo de vida
// =========================================================================

RefMenuPanel::RefMenuPanel(ProjectModel* model, EditorHost* host, DocCache* cache,
                           ElementsStore* elements, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_model(model)
    , m_host(host)
    , m_cache(cache)
    , m_elements(elements)
{
    setObjectName(QStringLiteral("refMenuPanel"));
    setWindowTitle(tr("Referência"));
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumSize(kMinW, kMinH);
    resize(kDefaultW, kDefaultH);

    buildUi();
    loadGeometryFromSettings();
    applyPreviewFont();
    applyLayout();

    if (m_model) {
        connect(m_model, &ProjectModel::manuscriptsChanged, this, &RefMenuPanel::refresh);
        connect(m_model, &ProjectModel::chaptersChanged, this, &RefMenuPanel::refresh);
        connect(m_model, &ProjectModel::drawersChanged, this, &RefMenuPanel::refresh);
        connect(m_model, &ProjectModel::loaded, this, &RefMenuPanel::refresh);
    }
    if (m_elements) {
        connect(m_elements, &ElementsStore::changed, this, &RefMenuPanel::refresh);
    }

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &RefMenuPanel::applyTheme);

    hide();
}

void RefMenuPanel::applyTheme()
{
    // Reaplica a stylesheet principal (header, tabs, preview wrapper, handles)
    // e força um rebuild da nav/preview pra estilos inline serem regerados.
    applyMainStyleSheet();
    refresh();
}

void RefMenuPanel::setProjectRoot(const QString& root)
{
    // Fixados e recentes são por projeto: troca de projeto, troca a lista.
    const bool changed = (m_projectRoot != root);
    m_projectRoot = root;
    if (changed) {
        m_pinnedKeys.clear();
        m_recentKeys.clear();
        m_tabs.clear();
        m_tabIdx = 0;
        m_gallerySource.clear();
        m_excerptCache.clear();
        loadPinsAndRecents();
        if (m_compare) setCompare(false);
        m_navMode = NavMode::Home;
    }
}

// =========================================================================
// UI
// =========================================================================

void RefMenuPanel::buildUi()
{
    // Layout externo: sem margem — os handles dedicados cobrem as bordas.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_frame = new QWidget(this);
    m_frame->setObjectName(QStringLiteral("refFrame"));
    m_frame->setAttribute(Qt::WA_StyledBackground, true);
    outer->addWidget(m_frame, /*stretch=*/1);

    m_frameLay = new QVBoxLayout(m_frame);
    m_frameLay->setContentsMargins(0, 0, 0, 0);
    m_frameLay->setSpacing(0);

    // ---- Header ----
    m_header = new QWidget(m_frame);
    m_header->setObjectName(QStringLiteral("refHeader"));
    m_header->setAttribute(Qt::WA_StyledBackground, true);
    m_header->setFixedHeight(kHeaderH);
    auto* hLay = new QHBoxLayout(m_header);
    hLay->setContentsMargins(8, 4, 6, 4);
    hLay->setSpacing(6);

    m_dragHandle = new QToolButton(m_header);
    m_dragHandle->setObjectName(QStringLiteral("refDragHandle"));
    m_dragHandle->setText(QStringLiteral(":::"));
    m_dragHandle->setToolTip(tr("Arrastar (duplo clique pra resetar)"));
    m_dragHandle->setCursor(Qt::OpenHandCursor);
    m_dragHandle->installEventFilter(this);
    hLay->addWidget(m_dragHandle);

    m_title = new QLabel(tr("Referência"), m_header);
    m_title->setObjectName(QStringLiteral("refTitle"));
    hLay->addWidget(m_title);

    hLay->addStretch();

    m_toggleNavBtn = new QToolButton(m_header);
    m_toggleNavBtn->setObjectName(QStringLiteral("refTinyBtn"));
    m_toggleNavBtn->setText(QStringLiteral("▾"));
    m_toggleNavBtn->setToolTip(tr("Ocultar/Mostrar explorador"));
    m_toggleNavBtn->setCursor(Qt::PointingHandCursor);
    connect(m_toggleNavBtn, &QToolButton::clicked, this, &RefMenuPanel::onToggleNav);
    hLay->addWidget(m_toggleNavBtn);

    // Mapa de Uso (global): onde cada personagem e lugar aparece no livro.
    m_mapBtn = new QToolButton(m_header);
    m_mapBtn->setObjectName(QStringLiteral("refTinyBtn"));
    m_mapBtn->setCheckable(true);
    m_mapBtn->setCursor(Qt::PointingHandCursor);
    m_mapBtn->setToolTip(tr("Mapa de Uso: onde cada personagem e lugar aparece no livro"));
    m_mapBtn->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/usage-map.svg"),
        QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
        QSize(14, 14)));
    connect(m_mapBtn, &QToolButton::toggled, this, [this](bool on) { setUsageMap(on); });
    hLay->addWidget(m_mapBtn);

    // Estilo do painel (Clássico, Duas colunas, Trilho…). Menu próprio no
    // cabeçalho: é uma escolha sobre o painel inteiro, então mora nele.
    m_layoutBtn = new QToolButton(m_header);
    m_layoutBtn->setObjectName(QStringLiteral("refTinyBtn"));
    {
        QIcon ic = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/layout.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        if (!ic.isNull()) m_layoutBtn->setIcon(ic);
    }
    m_layoutBtn->setToolTip(tr("Estilo do RefMenu"));
    m_layoutBtn->setCursor(Qt::PointingHandCursor);
    m_layoutBtn->setPopupMode(QToolButton::InstantPopup);
    m_layoutMenu = new QMenu(m_layoutBtn);
    connect(m_layoutMenu, &QMenu::aboutToShow, this, &RefMenuPanel::rebuildLayoutMenu);
    m_layoutBtn->setMenu(m_layoutMenu);
    hLay->addWidget(m_layoutBtn);

    m_searchBtn = new QToolButton(m_header);
    m_searchBtn->setObjectName(QStringLiteral("refTinyBtn"));
    {
        QIcon ic = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/search.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        if (!ic.isNull()) m_searchBtn->setIcon(ic);
    }
    m_searchBtn->setToolTip(tr("Pesquisar no RefMenu (Ctrl+Alt+F)"));
    m_searchBtn->setCursor(Qt::PointingHandCursor);
    m_searchBtn->setCheckable(true);
    connect(m_searchBtn, &QToolButton::clicked, this, &RefMenuPanel::onToggleSearch);
    hLay->addWidget(m_searchBtn);

    m_fontSizeBtn = new QToolButton(m_header);
    m_fontSizeBtn->setObjectName(QStringLiteral("refTinyBtn"));
    m_fontSizeBtn->setText(QStringLiteral("Aa"));
    m_fontSizeBtn->setToolTip(tr("Tamanho do texto do preview"));
    m_fontSizeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_fontSizeBtn, &QToolButton::clicked, this, &RefMenuPanel::onCycleFontSize);
    hLay->addWidget(m_fontSizeBtn);

    m_editBtn = new QToolButton(m_header);
    m_editBtn->setObjectName(QStringLiteral("refTinyBtn"));
    {
        QIcon ic = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/edit.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        if (!ic.isNull()) m_editBtn->setIcon(ic);
    }
    m_editBtn->setCheckable(true);
    m_editBtn->setCursor(Qt::PointingHandCursor);
    m_editBtn->setToolTip(tr("Editar documento"));
    m_editBtn->setEnabled(false);
    connect(m_editBtn, &QToolButton::toggled, this, &RefMenuPanel::onToggleEdit);
    hLay->addWidget(m_editBtn);

    m_pinBtn = new QToolButton(m_header);
    m_pinBtn->setObjectName(QStringLiteral("refTinyBtn"));
    {
        QIcon ic = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/pin.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        if (!ic.isNull()) m_pinBtn->setIcon(ic);
    }
    m_pinBtn->setToolTip(tr("Fixar"));
    m_pinBtn->setCheckable(true);
    m_pinBtn->setCursor(Qt::PointingHandCursor);
    connect(m_pinBtn, &QToolButton::clicked, this, &RefMenuPanel::onTogglePin);
    hLay->addWidget(m_pinBtn);

    m_closeBtn = new QToolButton(m_header);
    m_closeBtn->setObjectName(QStringLiteral("refTinyBtn"));
    m_closeBtn->setText(QStringLiteral("✕"));
    m_closeBtn->setToolTip(tr("Fechar"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_closeBtn, &QToolButton::clicked, this, &RefMenuPanel::onCloseClicked);
    hLay->addWidget(m_closeBtn);

    m_frameLay->addWidget(m_header);

    // ---- Busca (sempre visível) ----
    // Antes ficava atrás de um botão de lupa. Consultar É a função do painel,
    // então o campo é a primeira coisa abaixo do cabeçalho — não um recurso
    // escondido que só quem sabe do atalho encontra.
    m_searchRow = new QWidget(m_frame);
    m_searchRow->setObjectName(QStringLiteral("refSearchRow"));
    m_searchRow->setAttribute(Qt::WA_StyledBackground, true);
    {
        auto* sLay = new QHBoxLayout(m_searchRow);
        sLay->setContentsMargins(10, 8, 10, 6);
        sLay->setSpacing(6);
        m_searchInput = new QLineEdit(m_searchRow);
        m_searchInput->setObjectName(QStringLiteral("refSearchInput"));
        m_searchInput->setPlaceholderText(tr("Buscar em tudo…"));
        m_searchInput->setClearButtonEnabled(true);
        connect(m_searchInput, &QLineEdit::textChanged, this, &RefMenuPanel::onSearchQueryChanged);
        m_searchInput->installEventFilter(this);   // Esc fecha a navegação flutuante
        sLay->addWidget(m_searchInput, 1);
    }
    // Onde a busca mora depende do estilo — applyLayout() a posiciona.

    // ---- Trilha ----
    // No lugar de "Manuscritos ▾" + "Gaveta ▾", que competiam pelo comando e
    // não diziam onde você estava. Some na tela inicial (não há caminho a
    // mostrar) e reaparece ao entrar numa seção ou abrir um documento.
    m_crumbsRow = new QWidget(m_frame);
    m_crumbsRow->setObjectName(QStringLiteral("refCrumbsRow"));
    m_crumbsRow->setAttribute(Qt::WA_StyledBackground, true);
    m_crumbsLay = new QHBoxLayout(m_crumbsRow);
    m_crumbsLay->setContentsMargins(10, 2, 8, 8);
    m_crumbsLay->setSpacing(4);

    // Modo visual e filtro de território seguem existindo, mas agora à direita
    // da trilha — aparecem só nas seções em que fazem sentido.
    m_viewModeBtn = new QToolButton(m_crumbsRow);
    m_viewModeBtn->setObjectName(QStringLiteral("refTabBtnSmall"));
    m_viewModeBtn->setText(QStringLiteral("≡"));
    m_viewModeBtn->setToolTip(tr("Alternar modo visual/lista"));
    m_viewModeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_viewModeBtn, &QToolButton::clicked, this, &RefMenuPanel::onToggleVisualMode);

    m_territorioFilterBtn = new QToolButton(m_crumbsRow);
    m_territorioFilterBtn->setObjectName(QStringLiteral("refTabBtnSmall"));
    m_territorioFilterBtn->setText(tr("Território: Todos"));
    m_territorioFilterBtn->setCursor(Qt::PointingHandCursor);
    m_territorioFilterBtn->setPopupMode(QToolButton::InstantPopup);
    m_territorioFilterMenu = new QMenu(m_territorioFilterBtn);
    connect(m_territorioFilterMenu, &QMenu::aboutToShow, this, &RefMenuPanel::rebuildTerritorioFilterMenu);
    m_territorioFilterBtn->setMenu(m_territorioFilterMenu);

    m_backBtn = new QToolButton(m_crumbsRow);
    m_backBtn->setObjectName(QStringLiteral("refBackBtn"));
    m_backBtn->setText(QString::fromUtf8("‹ ") + tr("Voltar ao mural"));
    m_backBtn->setCursor(Qt::PointingHandCursor);
    m_backBtn->setVisible(false);
    connect(m_backBtn, &QToolButton::clicked, this, [this]() { hideFloat(); });

    m_crumbsRow->setVisible(false);

    // ---- Barra do Leitor ----
    // Busca + fileira com quem está na cena e os fixados. Só existe no estilo
    // Leitor, onde a navegação não tem lugar fixo.
    m_readerBar = new QWidget(m_frame);
    m_readerBar->setObjectName(QStringLiteral("refReaderBar"));
    m_readerBar->setAttribute(Qt::WA_StyledBackground, true);
    m_readerBarLay = new QVBoxLayout(m_readerBar);
    m_readerBarLay->setContentsMargins(0, 0, 0, 6);
    m_readerBarLay->setSpacing(0);
    m_readerChipsScroll = new QScrollArea(m_readerBar);
    m_readerChipsScroll->setObjectName(QStringLiteral("refChipsScroll"));
    m_readerChipsScroll->setWidgetResizable(true);
    m_readerChipsScroll->setFrameShape(QFrame::NoFrame);
    m_readerChipsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_readerChipsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_readerChipsScroll->setFixedHeight(28);
    m_readerChipsScroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;border:none;}"));
    m_readerChipsScroll->viewport()->setAutoFillBackground(false);
    m_readerChipsScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    m_readerChips = new QWidget(m_readerChipsScroll);
    m_readerChips->setObjectName(QStringLiteral("refChipsInner"));
    m_readerChipsLay = new QHBoxLayout(m_readerChips);
    m_readerChipsLay->setContentsMargins(10, 0, 10, 0);
    m_readerChipsLay->setSpacing(6);
    m_readerChipsScroll->setWidget(m_readerChips);
    m_readerBarLay->addWidget(m_readerChipsScroll);
    m_readerBar->setVisible(false);

    // ---- Mapa de Uso ----
    m_mapPanel = new QWidget(m_frame);
    m_mapPanel->setObjectName(QStringLiteral("refMapPanel"));
    m_mapPanel->setAttribute(Qt::WA_StyledBackground, true);
    {
        auto* ml = new QVBoxLayout(m_mapPanel);
        ml->setContentsMargins(0, 0, 0, 0);
        ml->setSpacing(0);
        auto* bar = new QWidget(m_mapPanel);
        m_mapBarLay = new QHBoxLayout(bar);
        m_mapBarLay->setContentsMargins(12, 6, 6, 2);
        m_mapBarLay->setSpacing(6);
        ml->addWidget(bar);
        m_mapScroll = new QScrollArea(m_mapPanel);
        m_mapScroll->setObjectName(QStringLiteral("refMapScroll"));
        m_mapScroll->setFrameShape(QFrame::NoFrame);
        m_mapScroll->setWidgetResizable(false);
        m_mapScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_mapScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_mapScroll->viewport()->setAutoFillBackground(false);
        m_mapScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
        m_map = new RefUsageMap(m_mapScroll);
        m_map->chapterWord = tr("capítulo");
        m_map->onOpen = [this](const QString& key) {
            if (key.isEmpty()) return;
            setSelected(key);
            addRecent(key);
        };
        m_map->onRow = [this](const QString& id) {
            m_mapRow = id;
            m_map->selRow = id;
            m_map->update();
        };
        m_mapScroll->setWidget(m_map);
        ml->addWidget(m_mapScroll);
    }
    m_mapPanel->setVisible(false);

    // ---- Nav body ----
    m_navScroll = new QScrollArea(m_frame);
    m_navScroll->setObjectName(QStringLiteral("refNavScroll"));
    m_navScroll->setWidgetResizable(true);
    m_navScroll->setFrameShape(QFrame::NoFrame);
    m_navScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_navInner = new QWidget(m_navScroll);
    m_navInner->setObjectName(QStringLiteral("refNavInner"));
    m_navInner->setAttribute(Qt::WA_StyledBackground, true);
    m_navInnerLay = new QVBoxLayout(m_navInner);
    m_navInnerLay->setContentsMargins(10, 6, 10, 10);
    m_navInnerLay->setSpacing(6);
    m_navInnerLay->addStretch();
    m_navScroll->setWidget(m_navInner);
    // Navegação e preview vivem num divisor arrastável: a divisão entre "lista"
    // e "texto" é preferência do momento (às vezes você quer ver a árvore
    // inteira, às vezes quer ler). Sem teto fixo de altura — quem decide é a
    // alça, e a posição fica guardada.
    m_navScroll->setMinimumHeight(80);

    // Corpo: [trilho | coluna de fontes | lingueta] + splitter(nav, preview).
    // A ordem e quem aparece ficam com applyLayout().
    m_body = new QWidget(m_frame);
    m_body->setObjectName(QStringLiteral("refBody"));
    m_bodyLay = new QHBoxLayout(m_body);
    m_bodyLay->setContentsMargins(0, 0, 0, 0);
    m_bodyLay->setSpacing(0);
    m_frameLay->addWidget(m_body, /*stretch=*/1);

    m_navPane = new QWidget(m_body);
    m_navPane->setObjectName(QStringLiteral("refNavPane"));
    m_navPane->setAttribute(Qt::WA_StyledBackground, true);
    m_navPaneLay = new QVBoxLayout(m_navPane);
    m_navPaneLay->setContentsMargins(0, 0, 0, 0);
    m_navPaneLay->setSpacing(0);
    m_navPaneLay->addWidget(m_navScroll, 1);

    // Filtros do mural (Galeria): Na cena, cada gaveta, cada manuscrito, Mundos.
    m_galleryBar = new QScrollArea(m_navPane);
    m_galleryBar->setObjectName(QStringLiteral("refGalleryBar"));
    m_galleryBar->setWidgetResizable(true);
    m_galleryBar->setFrameShape(QFrame::NoFrame);
    m_galleryBar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_galleryBar->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_galleryBar->setFixedHeight(36);
    // O viewport não herda o fundo da QScrollArea: sem isto ele pinta o
    // cinza-escuro padrão por cima da cor do tema.
    m_galleryBar->viewport()->setAutoFillBackground(false);
    m_galleryBar->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    m_galleryChips = new QWidget(m_galleryBar);
    m_galleryChips->setObjectName(QStringLiteral("refChipsInner"));
    m_galleryChipsLay = new QHBoxLayout(m_galleryChips);
    m_galleryChipsLay->setContentsMargins(10, 4, 10, 4);
    m_galleryChipsLay->setSpacing(6);
    m_galleryBar->setWidget(m_galleryChips);
    m_galleryBar->setVisible(false);
    m_navPaneLay->insertWidget(0, m_galleryBar);
    m_galleryRelayout = new QTimer(this);
    m_galleryRelayout->setSingleShot(true);
    m_galleryRelayout->setInterval(120);
    connect(m_galleryRelayout, &QTimer::timeout, this, [this]() {
        if (m_layout == Layout::Gallery && m_navPane && m_navPane->isVisible()) rebuildNavBody();
    });

    m_bodySplit = new QSplitter(Qt::Vertical, m_body);
    m_bodySplit->setObjectName(QStringLiteral("refBodySplit"));
    m_bodySplit->setChildrenCollapsible(false);
    m_bodySplit->setHandleWidth(7);
    m_bodySplit->addWidget(m_navPane);
    connect(m_bodySplit, &QSplitter::splitterMoved, this, [this]() {
        // Largura da coluna de navegação nos estilos deitados: preferência.
        if (m_bodySplit->orientation() != Qt::Horizontal) return;
        if (!m_navPane->isVisible() || m_navPane->parentWidget() != m_bodySplit) return;
        const QList<int> sz = m_bodySplit->sizes();
        const int i = m_bodySplit->indexOf(m_navPane);
        if (i >= 0 && i < sz.size() && sz.at(i) > 0) {
            if (m_layout == Layout::Book) m_bookColW = sz.at(i);
            else                          m_navColW = sz.at(i);
            scheduleGeometrySave();
        }
    });

    m_rail = new QScrollArea(m_body);
    m_rail->setObjectName(QStringLiteral("refRail"));
    m_rail->setWidgetResizable(true);
    m_rail->setFrameShape(QFrame::NoFrame);
    m_rail->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_rail->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // rola na roda, sem barra
    m_rail->setFixedWidth(46);
    m_railInner = new QWidget(m_rail);
    m_railInner->setObjectName(QStringLiteral("refRailInner"));
    m_railLay = new QBoxLayout(QBoxLayout::TopToBottom, m_railInner);
    m_railLay->setContentsMargins(0, 8, 0, 8);
    m_railLay->setSpacing(3);
    m_rail->setWidget(m_railInner);
    m_rail->setVisible(false);

    m_millerCol = new QScrollArea(m_body);
    m_millerCol->setObjectName(QStringLiteral("refMillerCol"));
    m_millerCol->setWidgetResizable(true);
    m_millerCol->setFrameShape(QFrame::NoFrame);
    m_millerCol->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_millerCol->setFixedWidth(180);
    m_millerInner = new QWidget(m_millerCol);
    m_millerInner->setObjectName(QStringLiteral("refMillerInner"));
    m_millerLay = new QVBoxLayout(m_millerInner);
    m_millerLay->setContentsMargins(6, 8, 6, 10);
    m_millerLay->setSpacing(1);
    m_millerCol->setWidget(m_millerInner);
    m_millerCol->setVisible(false);

    m_edgeBtn = new QToolButton(m_body);
    m_edgeBtn->setObjectName(QStringLiteral("refEdgeBtn"));
    m_edgeBtn->setCursor(Qt::PointingHandCursor);
    m_edgeBtn->setToolTip(tr("Mostrar navegação"));
    m_edgeBtn->setFixedWidth(16);
    m_edgeBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    connect(m_edgeBtn, &QToolButton::clicked, this, &RefMenuPanel::onToggleNav);
    m_edgeBtn->setVisible(false);

    // Fichário: argolas à esquerda e divisórias à direita.
    m_rings = new RefRings(m_body);
    m_rings->setVisible(false);
    m_dividers = new RefDividers(m_body);
    m_dividers->setObjectName(QStringLiteral("refDividers"));
    m_dividers->onPick = [this](const QString& filter) {
        if (m_floatKind == FloatKind::Divider && m_floatFilter == filter) hideFloat();
        else showFloat(FloatKind::Divider, filter);
    };
    m_dividers->setVisible(false);

    // ---- Preview ----
    // A página viva e a lateral do Comparar saem da mesma fábrica. Os membros
    // antigos (m_preview, m_previewTitle…) apontam pra página viva.
    buildDocView(m_main, m_frame);
    m_previewWrap = m_main.wrap;
    m_previewTitle = m_main.title;
    m_previewRole = m_main.role;
    m_previewImagesScroll = m_main.imgScroll;
    m_previewImagesHost = m_main.imgHost;
    m_previewImagesLay = m_main.imgLay;
    m_preview = m_main.browser;
    m_previewPlaceholder = m_main.placeholder;
    buildDocView(m_side, m_frame);
    m_side.wrap->setVisible(false);

    m_previewPane = new QWidget(m_body);
    m_previewPane->setObjectName(QStringLiteral("refPreviewPane"));
    m_previewPane->setAttribute(Qt::WA_StyledBackground, true);
    m_previewPaneLay = new QVBoxLayout(m_previewPane);
    m_previewPaneLay->setContentsMargins(0, 0, 0, 0);
    m_previewPaneLay->setSpacing(0);
    // Abas (globais): uma fileira em cima da página, com "+" e Comparar.
    m_tabRow = new QWidget(m_previewPane);
    m_tabRow->setObjectName(QStringLiteral("refTabRow"));
    m_tabRow->setAttribute(Qt::WA_StyledBackground, true);
    {
        auto* tl = new QHBoxLayout(m_tabRow);
        tl->setContentsMargins(6, 5, 6, 0);
        tl->setSpacing(2);
        m_tabBar = new QTabBar(m_tabRow);
        m_tabBar->setObjectName(QStringLiteral("refTabs"));
        m_tabBar->setDrawBase(false);
        m_tabBar->setExpanding(false);
        m_tabBar->setMovable(true);
        m_tabBar->setUsesScrollButtons(true);
        m_tabBar->setElideMode(Qt::ElideRight);
        m_tabBar->setDocumentMode(true);
        m_tabBar->installEventFilter(this);     // clique do meio fecha
        connect(m_tabBar, &QTabBar::currentChanged, this, [this](int i) {
            if (m_syncingTabs || i < 0 || i >= m_tabs.size()) return;
            m_tabIdx = i;
            const QString key = m_tabs.at(i);
            if (key.isEmpty()) {
                m_syncingTabs = true;
                changeSelectedKey(QString());
                m_syncingTabs = false;
                rebuildPreview();
                rebuildCrumbs();
            } else {
                setSelected(key);
            }
            savePinsAndRecents();
        });
        connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
            if (from < 0 || to < 0 || from >= m_tabs.size() || to >= m_tabs.size()) return;
            m_tabs.move(from, to);
            m_tabBarKeys = m_tabs;
            m_tabIdx = m_tabBar->currentIndex();
            savePinsAndRecents();
        });
        tl->addWidget(m_tabBar, 1, Qt::AlignBottom);

        m_tabAddBtn = new QToolButton(m_tabRow);
        m_tabAddBtn->setObjectName(QStringLiteral("refTinyBtn"));
        m_tabAddBtn->setText(QStringLiteral("+"));
        m_tabAddBtn->setToolTip(tr("Nova aba (Ctrl+clique num documento também abre em aba nova)"));
        m_tabAddBtn->setCursor(Qt::PointingHandCursor);
        connect(m_tabAddBtn, &QToolButton::clicked, this, [this]() {
            // Aba vazia: o próximo documento escolhido cai nela.
            m_tabs.insert(qMin(m_tabIdx + 1, int(m_tabs.size())), QString());
            m_tabIdx = qMin(m_tabIdx + 1, int(m_tabs.size()) - 1);
            m_syncingTabs = true;
            changeSelectedKey(QString());
            m_syncingTabs = false;
            rebuildTabBar();
            rebuildPreview();
            rebuildCrumbs();
            savePinsAndRecents();
            if (!navPaneShown() && m_layout != Layout::Classic && m_layout != Layout::Gallery)
                showFloat(floatKindForLayout(), QString());
        });
        tl->addWidget(m_tabAddBtn, 0, Qt::AlignVCenter);

        m_compareBtn = new QToolButton(m_tabRow);
        m_compareBtn->setObjectName(QStringLiteral("refTinyBtn"));
        m_compareBtn->setCheckable(true);
        m_compareBtn->setCursor(Qt::PointingHandCursor);
        m_compareBtn->setToolTip(tr("Comparar: segura este documento de um lado e abre outro do outro"));
        m_compareBtn->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/compare.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14)));
        connect(m_compareBtn, &QToolButton::toggled, this, [this](bool on) { setCompare(on); });
        tl->addWidget(m_compareBtn, 0, Qt::AlignVCenter);
    }
    m_tabRow->setVisible(false);
    m_previewPaneLay->addWidget(m_tabRow);

    m_cmpSplit = new QSplitter(Qt::Horizontal, m_previewPane);
    m_cmpSplit->setObjectName(QStringLiteral("refBodySplitH"));
    m_cmpSplit->setChildrenCollapsible(false);
    m_cmpSplit->setHandleWidth(7);
    m_cmpSplit->addWidget(m_previewWrap);
    m_cmpSplit->addWidget(m_side.wrap);
    m_previewPaneLay->addWidget(m_cmpSplit, 1);

    m_bodySplit->addWidget(m_previewPane);
    m_bodySplit->setStretchFactor(0, 0);   // a árvore mantém o tamanho pedido
    m_bodySplit->setStretchFactor(1, 1);   // o texto absorve o resto
    m_bodySplit->setSizes({ 260, 460 });   // padrão até o usuário arrastar

    // Navegação flutuante: gaveta (Overlay), lista do trilho (Trilho) e
    // dropdown da busca (Leitor). Fica fora de layout, por cima do corpo; o
    // véu atrás fecha ao clicar fora.
    m_scrim = new QWidget(m_body);
    m_scrim->setObjectName(QStringLiteral("refScrim"));
    m_scrim->setAttribute(Qt::WA_StyledBackground, true);
    m_scrim->installEventFilter(this);
    m_scrim->hide();
    m_floatHost = new QFrame(m_body);
    m_floatHost->setObjectName(QStringLiteral("refFloat"));
    m_floatHost->setAttribute(Qt::WA_StyledBackground, true);
    m_floatLay = new QVBoxLayout(m_floatHost);
    m_floatLay->setContentsMargins(1, 1, 1, 1);
    m_floatLay->setSpacing(0);
    // Faixa na cor da divisória aberta (Fichário).
    m_floatBand = new QWidget(m_floatHost);
    m_floatBand->setFixedHeight(5);
    m_floatBand->setVisible(false);
    m_floatLay->addWidget(m_floatBand);
    m_floatHost->hide();

    // FindBar do preview (Alt+F). Não entra no layout — flutua sobre o
    // m_previewWrap, posicionada em positionPreviewFindBar().
    m_previewFind = new FindBar(this);
    m_previewFind->attachTo(m_preview);
    connect(m_previewFind, &FindBar::selectionsChanged, this, [this]() {
        if (m_preview) m_preview->setExtraSelections(m_previewFind->findSelections());
    });
    connect(m_previewFind, &FindBar::closed, this, [this]() {
        if (m_preview) m_preview->setExtraSelections({});
    });
    m_previewFind->hide();
    m_previewFind->raise();

    // ---- Resize handles (estilo DrawerListPanel) ----
    auto makeHandle = [this](const QString& name, Qt::CursorShape cur) {
        auto* h = new QWidget(this);
        h->setObjectName(name);
        h->setAttribute(Qt::WA_StyledBackground, true);
        h->setCursor(cur);
        h->installEventFilter(this);
        h->raise();
        return h;
    };
    m_hL  = makeHandle(QStringLiteral("refHandleL"),  Qt::SizeHorCursor);
    m_hR  = makeHandle(QStringLiteral("refHandleR"),  Qt::SizeHorCursor);
    m_hT  = makeHandle(QStringLiteral("refHandleT"),  Qt::SizeVerCursor);
    m_hB  = makeHandle(QStringLiteral("refHandleB"),  Qt::SizeVerCursor);
    m_hTL = makeHandle(QStringLiteral("refHandleTL"), Qt::SizeFDiagCursor);
    m_hTR = makeHandle(QStringLiteral("refHandleTR"), Qt::SizeBDiagCursor);
    m_hBL = makeHandle(QStringLiteral("refHandleBL"), Qt::SizeBDiagCursor);
    m_hBR = makeHandle(QStringLiteral("refHandleBR"), Qt::SizeFDiagCursor);
    layoutResizeHandles();

    applyMainStyleSheet();
}

void RefMenuPanel::buildDocView(DocView& v, QWidget* parent)
{
    v.wrap = new QWidget(parent);
    v.wrap->setObjectName(QStringLiteral("refPreviewWrap"));
    v.wrap->setAttribute(Qt::WA_StyledBackground, true);
    auto* outer = new QVBoxLayout(v.wrap);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Faixa do Comparar: diz qual página recebe o próximo documento. Clicar
    // na faixa da outra troca o alvo.
    v.bar = new QWidget(v.wrap);
    v.bar->setObjectName(QStringLiteral("refPaneBar"));
    v.bar->setAttribute(Qt::WA_StyledBackground, true);
    v.bar->setCursor(Qt::PointingHandCursor);
    v.bar->installEventFilter(this);
    auto* barLay = new QHBoxLayout(v.bar);
    barLay->setContentsMargins(12, 4, 10, 4);
    v.barLabel = new QLabel(v.bar);
    v.barLabel->setObjectName(QStringLiteral("refPaneTag"));
    barLay->addWidget(v.barLabel);
    barLay->addStretch();
    v.bar->setVisible(false);
    outer->addWidget(v.bar);

    auto* inner = new QWidget(v.wrap);
    auto* pvLay = new QVBoxLayout(inner);
    pvLay->setContentsMargins(12, 10, 12, 12);
    pvLay->setSpacing(6);
    outer->addWidget(inner, 1);

    v.title = new QLabel(inner);
    v.title->setObjectName(QStringLiteral("refPreviewTitle"));
    v.title->setWordWrap(true);
    v.title->setVisible(false);
    pvLay->addWidget(v.title);

    v.role = new QLabel(inner);
    v.role->setObjectName(QStringLiteral("refPreviewRole"));
    v.role->setVisible(false);
    pvLay->addWidget(v.role);

    v.imgScroll = new QScrollArea(inner);
    v.imgScroll->setObjectName(QStringLiteral("refImagesScroll"));
    v.imgScroll->setWidgetResizable(true);
    v.imgScroll->setFrameShape(QFrame::NoFrame);
    v.imgScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    v.imgScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    v.imgScroll->setMaximumHeight(360);
    v.imgScroll->setVisible(false);
    v.imgHost = new QWidget(v.imgScroll);
    v.imgLay = new QVBoxLayout(v.imgHost);
    v.imgLay->setContentsMargins(0, 0, 0, 0);
    v.imgLay->setSpacing(8);
    v.imgScroll->setWidget(v.imgHost);
    pvLay->addWidget(v.imgScroll);

    v.browser = new QTextBrowser(inner);
    v.browser->setObjectName(QStringLiteral("refPreview"));
    v.browser->setReadOnly(true);
    v.browser->setOpenExternalLinks(false);
    v.browser->setFrameShape(QFrame::NoFrame);
    pvLay->addWidget(v.browser, /*stretch=*/1);

    v.placeholder = new QLabel(tr("Selecione um documento acima pra visualizar aqui."), inner);
    v.placeholder->setObjectName(QStringLiteral("refPreviewPlaceholder"));
    v.placeholder->setAlignment(Qt::AlignCenter);
    v.placeholder->setWordWrap(true);
    pvLay->addWidget(v.placeholder);
    v.folio = new QLabel(inner);
    v.folio->setObjectName(QStringLiteral("refFolio"));
    v.folio->setAlignment(Qt::AlignCenter);
    v.folio->setVisible(false);
    pvLay->addWidget(v.folio);
    v.browser->setVisible(false);
}

void RefMenuPanel::applyMainStyleSheet()
{
    const QString panelBg   = Theme::panelBackground();
    const QString appBg     = Theme::appBackground();
    const QString border    = Theme::panelBorder();
    const QString subtle    = Theme::subtleBorder();
    const QString txtPrim   = Theme::textPrimary();
    const QString txtMuted  = Theme::textMuted();
    const QString txtBright = Theme::textBright();
    const QString hover     = Theme::hoverOverlay();
    const QString accentSf  = Theme::accentInfoSoft();
    const QString accentBd  = Theme::accentInfoBorderSoft();
    const QString disabled  = Theme::disabledText();

    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QWidget#refMenuPanel { background: transparent; }
        QWidget#refFrame {
            background: %1;
            border: 1px solid %3;
            border-radius: @radius-panel;
        }
        QWidget#refHeader {
            background: %2;
            border-bottom: 1px solid %4;
            border-top-left-radius: @radius-panel;
            border-top-right-radius: @radius-panel;
        }
        QLabel#refTitle { color: %7; font-size: 13px; font-weight: 600; }
        QToolButton#refTinyBtn {
            background: transparent; color: %6;
            border: none; border-radius: @radius-control;
            min-width: 24px; min-height: 24px;
            padding: 0 4px;
            font-size: 13px;
        }
        QToolButton#refTinyBtn:hover { background: %8; color: %7; }
        QToolButton#refTinyBtn:checked { background: %9; color: %7; }
        QToolButton#refTinyBtn:disabled { color: %11; }
        QToolButton#refTinyBtn::menu-indicator { image: none; width: 0; }
        QToolButton#refDragHandle {
            background: transparent; color: %6;
            border: none; padding: 0 6px; font-size: 14px;
        }
        QToolButton#refDragHandle:hover { color: %7; }
        QWidget#refSearchRow {
            background: %2;
            border-bottom: 1px solid %4;
        }
        QLineEdit#refSearchInput {
            background: %1;
            color: %7;
            border: 1px solid %4;
            border-radius: @radius-control;
            padding: 4px 8px;
            font-size: 12px;
        }
        QLineEdit#refSearchInput:focus { border-color: %10; }
        QWidget#refCrumbsRow {
            background: %2;
            border-bottom: 1px solid %4;
        }
        QToolButton#refCrumbLink {
            background: transparent;
            color: %10;
            border: 0;
            border-radius: @radius-item;
            padding: 2px 5px;
            font-size: 12px;
        }
        QToolButton#refCrumbLink:hover { background: %8; }
        QLabel#refCrumbSep  { color: %5; font-size: 12px; padding: 0 1px; }
        QLabel#refCrumbHere { color: %7; font-size: 12px; padding: 2px 3px; }
        QToolButton#refTabBtn, QToolButton#refTabBtnSmall {
            background: transparent;
            color: %5;
            border: 1px solid transparent;
            border-radius: @radius-control;
            padding: 4px 10px;
            font-size: 12px;
        }
        QToolButton#refTabBtnSmall {
            padding: 2px 7px;
            min-width: 24px;
            font-size: 13px;
        }
        QToolButton#refTabBtn:hover, QToolButton#refTabBtnSmall:hover {
            background: %8; color: %7;
        }
        QToolButton#refTabBtn:checked {
            background: %9;
            border-color: %10;
            color: %7;
        }
        QToolButton#refTabBtn:disabled { color: %11; }
        QScrollArea#refNavScroll { background: transparent; border: none; }
        /* Alca do divisor: uma linha discreta que acende no hover, pra
           anunciar que da' pra arrastar sem virar um elemento gritante. */
        QSplitter#refBodySplit::handle:vertical {
            background: %4;
            height: 1px;
            margin: 3px 10px;
        }
        QSplitter#refBodySplit::handle:vertical:hover,
        QSplitter#refBodySplit::handle:vertical:pressed {
            background: %10;
            height: 2px;
            margin: 2px 8px;
        }
        /* Deitado (estilos em colunas) o splitter troca de nome: a regra de
           cima tem height, e o QSS mede a alca por ela sem olhar orientacao
           (a alca ficava com 27 px). Aqui a espessura e' a do setHandleWidth. */
        QSplitter#refBodySplitH::handle {
            background: %4;
            margin: 10px 3px;
        }
        QSplitter#refBodySplitH::handle:hover,
        QSplitter#refBodySplitH::handle:pressed {
            background: %10;
            margin: 8px 2px;
        }
        QWidget#refNavInner { background: transparent; }
        QWidget#refPreviewWrap { background: transparent; }
        QWidget#refPreviewWrap[classic="true"] { border-top: 1px solid %4; }
        QLabel#refPreviewTitle {
            color: %7; font-size: 14px; font-weight: 700;
        }
        QLabel#refPreviewRole {
            color: %5; font-size: 10px; font-weight: 700;
            letter-spacing: 0.5px;
        }
        QTextBrowser#refPreview {
            background: transparent;
            color: %5;
            padding: 0;
        }
        QLabel#refPreviewPlaceholder {
            color: %6; font-size: 11px; padding: 12px 0;
        }
        QScrollArea#refImagesScroll { background: transparent; border: none; }
        QWidget#refHandleL, QWidget#refHandleR, QWidget#refHandleT, QWidget#refHandleB,
        QWidget#refHandleTL, QWidget#refHandleTR, QWidget#refHandleBL, QWidget#refHandleBR {
            background: transparent;
        }
        QWidget#refHandleL:hover, QWidget#refHandleR:hover,
        QWidget#refHandleT:hover, QWidget#refHandleB:hover,
        QWidget#refHandleTL:hover, QWidget#refHandleTR:hover,
        QWidget#refHandleBL:hover, QWidget#refHandleBR:hover {
            background: %12;
        }
    )"))
        .arg(panelBg,   // 1
             appBg,     // 2
             border,    // 3
             subtle,    // 4
             txtPrim,   // 5
             txtMuted,  // 6
             txtBright, // 7
             hover,     // 8
             accentSf,  // 9
             accentBd,  // 10
             disabled)  // 11
        .arg(subtle)    // 12 (handles hover)
        // Estilos de layout: trilho, coluna de fontes (Miller), lingueta,
        // navegação flutuante e fileira do Leitor.
        + Theme::qss(QStringLiteral(R"(
        QScrollArea#refRail { background: %2; border: none; border-right: 1px solid %4; }
        QScrollArea#refRail[right="true"] { border-right: none; border-left: 1px solid %4; }
        QWidget#refRailInner { background: transparent; }
        QToolButton#refRailBtn {
            background: transparent; border: none;
            border-radius: @radius-control;
            min-width: 32px; max-width: 32px; min-height: 32px; max-height: 32px;
            padding: 0;
        }
        QToolButton#refRailBtn:hover { background: %8; }
        QToolButton#refRailBtn:checked { background: %9; }
        QToolButton#refRailAvatar {
            background: transparent; border: 2px solid transparent;
            border-radius: 16px; padding: 0;
            min-width: 32px; max-width: 32px; min-height: 32px; max-height: 32px;
        }
        QToolButton#refRailAvatar:hover { border-color: %10; }
        QToolButton#refRailAvatar:checked { border-color: %7; }
        QFrame#refRailSep { background: %4; border: none; min-height: 1px; max-height: 1px; }
        QLabel#refRailLabel { color: %6; font-size: 8px; font-weight: 700; letter-spacing: 0.5px; }
        QToolButton#refEdgeBtn {
            background: %2; color: %6; border: none; border-right: 1px solid %4;
            font-size: 13px; padding: 0;
        }
        QToolButton#refEdgeBtn[right="true"] { border-right: none; border-left: 1px solid %4; }
        QToolButton#refEdgeBtn:hover { background: %8; color: %7; }
        QScrollArea#refMillerCol { background: %2; border: none; border-right: 1px solid %4; }
        QWidget#refMillerInner { background: transparent; }
        QWidget#refMillerRow { border-radius: @radius-item; }
        QWidget#refMillerRow:hover { background: %8; }
        QWidget#refMillerRow[sel="true"] { background: %9; }
        QFrame#refFloat { background: %1; border: 1px solid %3; }
        QWidget#refReaderBar { background: %2; border-bottom: 1px solid %4; }
        QToolButton#refChip {
            background: transparent; color: %5;
            border: 1px solid %4; border-radius: @radius-control;
            padding: 2px 9px 2px 4px; font-size: 11px;
        }
        QToolButton#refChip:hover { border-color: %10; color: %7; }
        QToolButton#refChip:checked { background: %9; border-color: transparent; color: %7; }
        QLabel#refChipsLabel { color: %6; font-size: 9px; font-weight: 700; letter-spacing: 1px; }
        QScrollArea#refRail[dock="true"] { border: none; border-top: 1px solid %4; }
        QWidget#refTabRow { background: %2; border-bottom: 1px solid %4; }
        QTabBar#refTabs { background: transparent; }
        QTabBar#refTabs::tab {
            background: transparent; color: %6;
            border: 1px solid transparent; border-bottom: none;
            border-top-left-radius: @radius-control; border-top-right-radius: @radius-control;
            padding: 5px 4px 6px 9px; margin-right: 2px;
            font-size: 12px; max-width: 180px;
        }
        QTabBar#refTabs::tab:hover { background: %8; color: %5; }
        QTabBar#refTabs::tab:selected { background: %1; color: %7; border-color: %4; }
        QToolButton#refTabClose {
            background: transparent; border: none; color: %6;
            font-size: 12px; padding: 0 3px; border-radius: 3px;
        }
        QToolButton#refTabClose:hover { background: %8; color: %7; }
        QWidget#refPaneBar { background: %2; border-bottom: 1px solid %4; border-top: 2px solid transparent; }
        QWidget#refPaneBar[live="true"] { border-top: 2px solid %11; }
        QLabel#refPaneTag { color: %6; font-size: 11px; }
        QLabel#refPaneTag[live="true"] { color: %11; font-size: 10px; font-weight: 700; letter-spacing: 0.6px; }
        QWidget#refGalleryCard {
            background: %13; border: 1px solid %4; border-radius: @radius-control;
        }
        QScrollArea#refGalleryBar { background: %2; border: none; border-bottom: 1px solid %4; }
        QWidget#refChipsInner { background: transparent; }
        QWidget#refGalleryCard:hover { border-color: %10; }
        QLabel#refCardName { color: %7; font-size: 12px; }
        QLabel#refCardRole { color: %12; font-size: 9px; font-weight: 700; letter-spacing: 0.5px; }
        QLabel#refCardMeta { color: %6; font-size: 9px; font-weight: 700; letter-spacing: 0.5px; }
        QLabel#refCardExcerpt { color: %5; font-size: 12px; font-style: italic; }
        QToolButton#refBackBtn {
            background: transparent; color: %10; border: 0;
            border-radius: @radius-item; padding: 2px 6px; font-size: 12px;
        }
        QToolButton#refBackBtn:hover { background: %8; }
        QWidget#refMapPanel { background: %2; border-bottom: 1px solid %4; }
        QLabel#refMapTitle { color: %6; font-size: 10px; font-weight: 700; letter-spacing: 1.2px; }
        QScrollArea#refMapScroll { background: transparent; border: none; }
        QWidget#refNavPane[book="true"] {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 %1, stop:0.86 %1, stop:1 rgba(0,0,0,0.22));
        }
        QWidget#refPreviewPane[book="true"] {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 rgba(0,0,0,0.24), stop:0.07 %1, stop:1 %1);
        }
        QLabel#refFolio { color: %6; font-size: 11px; font-style: italic; padding: 4px 0 2px; }
    )"))
        .arg(panelBg,   // 1
             appBg,     // 2
             border,    // 3
             subtle,    // 4
             txtPrim,   // 5
             txtMuted,  // 6
             txtBright, // 7
             hover,     // 8
             accentSf)  // 9
        .arg(accentBd,   // 10
             Theme::accentDefault(),  // 11
             Theme::accentInfo(),     // 12
             Theme::inputBackground())); // 13
}

void RefMenuPanel::layoutResizeHandles()
{
    if (!m_hL || !m_hR || !m_hT || !m_hB) return;
    const int w = width();
    const int h = height();
    const int hw = kHandleW;
    const int cs = kCornerSz;

    // Faixas: cobrem a borda inteira menos os cantos
    m_hL->setGeometry(0,        cs,       hw,       h - 2 * cs);
    m_hR->setGeometry(w - hw,   cs,       hw,       h - 2 * cs);
    m_hT->setGeometry(cs,       0,        w - 2*cs, hw);
    m_hB->setGeometry(cs,       h - hw,   w - 2*cs, hw);
    // Cantos: quadrados acima das faixas
    m_hTL->setGeometry(0,        0,        cs, cs);
    m_hTR->setGeometry(w - cs,   0,        cs, cs);
    m_hBL->setGeometry(0,        h - cs,   cs, cs);
    m_hBR->setGeometry(w - cs,   h - cs,   cs, cs);

    for (QWidget* h : { m_hL, m_hR, m_hT, m_hB, m_hTL, m_hTR, m_hBL, m_hBR }) {
        h->raise();
    }
}

// =========================================================================
// Reconstrução dos painéis
// =========================================================================

void RefMenuPanel::refresh()
{
    rebuildCrumbs();
    rebuildNavBody();
    rebuildPreview();
    rebuildRail();
    rebuildMillerSources();
    rebuildReaderChips();
    rebuildGalleryBar();
    rebuildDividers();
    m_excerptCache.clear();
    m_wordCache.clear();
    if (m_mapOn) rebuildUsageMap();
    pruneTabs();
    m_tabBarKeys.clear();   // títulos podem ter mudado: refaz a barra
    rebuildTabBar();
    if (m_compare) renderDoc(m_side, m_sideKey);
}

void RefMenuPanel::rebuildCrumbs()
{
    if (!m_crumbsRow || !m_crumbsLay) return;

    // Esvazia a trilha sem destruir os dois botoes persistentes (modo visual e
    // filtro de territorio), que sao membros e voltam ao layout logo abaixo.
    while (QLayoutItem* it = m_crumbsLay->takeAt(0)) {
        QWidget* w = it->widget();
        if (w && w != m_viewModeBtn && w != m_territorioFilterBtn && w != m_backBtn) w->deleteLater();
        delete it;
    }

    const bool hasDoc = !m_selectedKey.isEmpty();
    const bool searching = !m_searchQuery.isEmpty();
    const bool inSection = (m_navMode == NavMode::Section);

    // Na tela inicial nao ha caminho a mostrar.
    if (!inSection && !hasDoc && !searching) {
        m_crumbsRow->setVisible(false);
        return;
    }
    // No Clássico a trilha some junto com a navegação; nos estilos em que ela
    // mora em cima do documento, fica. O Leitor não tem trilha.
    m_crumbsRow->setVisible(m_layout == Layout::Classic ? !m_navHidden
                                                        : m_layout != Layout::Reader);

    auto addLink = [this](const QString& text, std::function<void()> onClick) {
        auto* b = new QToolButton(m_crumbsRow);
        b->setObjectName(QStringLiteral("refCrumbLink"));
        b->setText(text);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, [onClick]() { onClick(); });
        m_crumbsLay->addWidget(b);
    };
    auto addSep = [this]() {
        auto* s = new QLabel(QString::fromUtf8("›"), m_crumbsRow);
        s->setObjectName(QStringLiteral("refCrumbSep"));
        m_crumbsLay->addWidget(s);
    };
    auto addHere = [this](const QString& text) {
        auto* l = new QLabel(text, m_crumbsRow);
        l->setObjectName(QStringLiteral("refCrumbHere"));
        m_crumbsLay->addWidget(l);
    };

    // Galeria: o documento está por cima do mural; voltar é fechar a folha.
    if (m_backBtn) {
        const bool back = (m_layout == Layout::Gallery);
        m_backBtn->setVisible(back);
        if (back) m_crumbsLay->addWidget(m_backBtn);
    }
    addLink(tr("Tudo"), [this]() { goHome(); });

    if (searching) {
        addSep();
        addHere(tr("Resultados"));
    } else {
        const QString sec = (m_navMode == NavMode::Home) ? QString() : currentSectionLabel();
        if (!sec.isEmpty()) {
            addSep();
            if (hasDoc) {
                const SourceKind kind = m_sourceKind;
                const QString key = m_currentDrawerKey;
                addLink(sec, [this, kind, key]() { enterSection(kind, key); });
            } else {
                addHere(sec);
            }
        }
        if (hasDoc) {
            KeyInfo info;
            if (describeKey(m_selectedKey, &info)) {
                addSep();
                addHere(info.name);
            }
        }
    }

    m_crumbsLay->addStretch();

    // Modo visual: so numa gaveta com cards de foto.
    bool visualDrawer = false;
    if (m_sourceKind == SourceKind::Drawer && m_model) {
        for (const Drawer& d : m_model->drawers()) {
            if (d.key == m_currentDrawerKey) { visualDrawer = drawerIsVisual(&d); break; }
        }
    }
    if (m_viewModeBtn) {
        m_crumbsLay->addWidget(m_viewModeBtn);
        m_viewModeBtn->setVisible(inSection && m_sourceKind == SourceKind::Drawer && visualDrawer);
        m_viewModeBtn->setText(m_visualMode ? QStringLiteral("≡") : QStringLiteral("⊞"));
        m_viewModeBtn->setToolTip(m_visualMode ? tr("Modo lista") : tr("Modo visual"));
    }
    if (m_territorioFilterBtn) {
        m_crumbsLay->addWidget(m_territorioFilterBtn);
        m_territorioFilterBtn->setVisible(inSection && m_sourceKind == SourceKind::Drawer
            && m_territorioStore && !m_territorioStore->territorios().isEmpty());
    }
}

QString RefMenuPanel::currentSectionLabel() const
{
    switch (m_sourceKind) {
    case SourceKind::Manuscript: {
        if (m_model) {
            for (const auto& ms : m_model->manuscripts()) {
                if (ms.id == m_currentManuscriptId && !ms.title.isEmpty()) return ms.title;
            }
        }
        return tr("Manuscrito");
    }
    case SourceKind::WorldExplorer:        return tr("Mundos");
    case SourceKind::TimelinesPlaceholder: return tr("Timelines");
    case SourceKind::MarkersPlaceholder:   return tr("Grupos");
    case SourceKind::Drawer: {
        if (m_currentDrawerKey == QStringLiteral("__groups__")) return tr("Grupos");
        if (m_model) {
            for (const Drawer& d : m_model->drawers()) {
                if (d.key == m_currentDrawerKey && !d.title.isEmpty()) return d.title;
            }
        }
        return tr("Gaveta");
    }
    }
    return QString();
}

void RefMenuPanel::goHome()
{
    m_navMode = NavMode::Home;
    m_currentGroupId.clear();
    m_currentFolderId.clear();
    m_currentConstrutorSystemId.clear();
    m_currentLugarTerritorioId.clear();
    changeSelectedKey(QString());
    if (m_searchInput && !m_searchInput->text().isEmpty()) {
        QSignalBlocker b(m_searchInput);
        m_searchInput->clear();
        m_searchQuery.clear();
    }
    rebuildNavBody();
    rebuildPreview();
    rebuildCrumbs();
    // "Tudo" sem navegação à vista (coluna recolhida, gaveta fechada): abre a
    // navegação, senão o clique não mostraria nada além do documento sumindo.
    if (m_layout == Layout::Miller) {
        m_millerSource = QStringLiteral("home");
        rebuildMillerSources();
        rebuildNavBody();
    } else if (m_layout == Layout::Gallery) {
        hideFloat();
    } else if (m_layout != Layout::Classic && !navPaneShown()) {
        showFloat(floatKindForLayout(), QString());
    }
}

void RefMenuPanel::enterSection(SourceKind kind, const QString& key)
{
    // No Miller a seção é a coluna de fontes: marca a fonte e fica na tela
    // inicial filtrada, em vez de cair na view de seção.
    if (m_layout == Layout::Miller || m_layout == Layout::Gallery) {
        m_navMode = NavMode::Home;
        QString src;
        if (kind == SourceKind::Drawer) src = QStringLiteral("dr:%1").arg(key);
        else if (kind == SourceKind::Manuscript)
            src = QStringLiteral("ms:%1").arg(key.isEmpty() ? m_currentManuscriptId : key);
        else if (kind == SourceKind::WorldExplorer) src = QStringLiteral("world");
        if (m_layout == Layout::Miller) m_millerSource = src.isEmpty() ? QStringLiteral("home") : src;
        else { m_gallerySource = src; hideFloat(); rebuildGalleryBar(); }
        changeSelectedKey(QString());
        rebuildMillerSources();
        rebuildNavBody();
        rebuildPreview();
        rebuildCrumbs();
        return;
    }
    m_navMode = NavMode::Section;
    m_sourceKind = kind;
    if (kind == SourceKind::Drawer) m_currentDrawerKey = key;
    else if (kind == SourceKind::Manuscript && !key.isEmpty()) m_currentManuscriptId = key;
    m_currentGroupId.clear();
    m_currentFolderId.clear();
    m_currentConstrutorSystemId.clear();
    m_currentLugarTerritorioId.clear();
    changeSelectedKey(QString());
    rebuildNavBody();
    rebuildPreview();
    rebuildCrumbs();
    if (m_layout != Layout::Classic && !navPaneShown()) showFloat(floatKindForLayout(), QString());
}

void RefMenuPanel::rebuildTerritorioFilterMenu()
{
    if (!m_territorioFilterMenu || !m_territorioStore) return;
    m_territorioFilterMenu->clear();

    QAction* all = m_territorioFilterMenu->addAction(tr("Todos"));
    connect(all, &QAction::triggered, this, [this]() {
        m_territorioFilterId.clear();
        if (m_territorioFilterBtn) m_territorioFilterBtn->setText(tr("Território: Todos"));
        rebuildNavBody();
    });
    m_territorioFilterMenu->addSeparator();

    QList<TerritorioStore::Territorio> territorios = m_territorioStore->territorios();
    std::sort(territorios.begin(), territorios.end(),
              [](const TerritorioStore::Territorio& a, const TerritorioStore::Territorio& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    for (const auto& t : territorios) {
        const QString name = t.name.isEmpty() ? tr("(sem nome)") : t.name;
        QAction* a = m_territorioFilterMenu->addAction(name);
        a->setCheckable(true);
        a->setChecked(t.id == m_territorioFilterId);
        const QString id = t.id;
        connect(a, &QAction::triggered, this, [this, id, name]() {
            m_territorioFilterId = id;
            if (m_territorioFilterBtn) m_territorioFilterBtn->setText(tr("Território: %1").arg(name));
            rebuildNavBody();
        });
    }
}

void RefMenuPanel::rebuildNavBody()
{
    if (!m_navInner || !m_navInnerLay) return;
    // Limpa filhos do layout (mantendo o stretch no final).
    while (m_navInnerLay->count() > 0) {
        QLayoutItem* item = m_navInnerLay->takeAt(0);
        if (auto* w = item->widget()) w->deleteLater();
        delete item;
    }

    // Com busca ativa, varre tudo (manuscritos, capítulos+cenas, gavetas)
    // independente do source/drawer aberto. Sem busca, segue o source normal.
    if (!m_searchQuery.isEmpty()) {
        buildSearchAllView();
        m_navInnerLay->addStretch();
        return;
    }

    // Livro aberto: o sumário, com as gavetas como apêndices.
    if (m_layout == Layout::Book && m_floatKind == FloatKind::None) {
        buildTocView();
        m_navInnerLay->addStretch();
        return;
    }

    // Galeria: o mural de cartões da fonte escolhida nos filtros.
    if (m_layout == Layout::Gallery && m_floatKind == FloatKind::None) {
        buildGalleryView();
        m_navInnerLay->addStretch();
        return;
    }

    // Tela inicial: o que você provavelmente quer, antes de qualquer navegação.
    if (m_navMode == NavMode::Home) {
        buildHomeView();
        m_navInnerLay->addStretch();
        return;
    }

    if (m_sourceKind == SourceKind::Manuscript) {
        buildManuscriptsView();
    } else if (m_sourceKind == SourceKind::Drawer) {
        buildDrawerView();
    } else if (m_sourceKind == SourceKind::MarkersPlaceholder) {
        buildGroupsView();
    } else if (m_sourceKind == SourceKind::TimelinesPlaceholder) {
        buildPlaceholderView(tr("Timelines"), tr("Em breve. Vai listar as linhas do tempo."));
    } else if (m_sourceKind == SourceKind::WorldExplorer) {
        buildWorldExplorerView();
    }

    m_navInnerLay->addStretch();
}

const ConstrutorStore::Node* RefMenuPanel::findConstrutorNode(
    const ConstrutorStore::System* sys, const QString& nodeId) const
{
    if (!sys) return nullptr;
    std::function<const ConstrutorStore::Node*(const QList<ConstrutorStore::Node>&)> find;
    find = [&](const QList<ConstrutorStore::Node>& nodes) -> const ConstrutorStore::Node* {
        for (const auto& n : nodes) {
            if (n.id == nodeId) return &n;
            if (const auto* c = find(n.children)) return c;
        }
        return nullptr;
    };
    return find(sys->nodes);
}

const TerritorioStore::Node* RefMenuPanel::findTerritorioNode(
    const TerritorioStore::Territorio* ter, const QString& nodeId) const
{
    if (!ter) return nullptr;
    std::function<const TerritorioStore::Node*(const QList<TerritorioStore::Node>&)> find;
    find = [&](const QList<TerritorioStore::Node>& nodes) -> const TerritorioStore::Node* {
        for (const auto& n : nodes) {
            if (n.id == nodeId) return &n;
            if (const auto* c = find(n.children)) return c;
        }
        return nullptr;
    };
    return find(ter->nodes);
}

static QString refListStyleSheet()
{
    return Theme::qss(QStringLiteral(R"(
        QListWidget {
            background: %1; color: %2;
            border: 1px solid %3; border-radius: @radius-control;
            outline: none; padding: 4px;
        }
        QListWidget::item { padding: 6px 8px; border-radius: @radius-item; }
        QListWidget::item:hover { background: %4; color: %5; }
        QListWidget::item:selected { background: %6; color: %5; }
    )")).arg(Theme::appBackground(),
           Theme::textPrimary(),
           Theme::panelBorder(),
           Theme::hoverOverlay(),
           Theme::textBright(),
           Theme::accentInfoSoft());
}

// "Explorador de Mundos" — fusão de Território (Lugares) e Construtor
// (Sistemas) numa view só. Na raiz mostra as duas seções lado a lado; ao
// entrar num território/sistema, mostra só os nós daquele um (mesmo
// comportamento de antes, só realocado pra dentro de uma função só).
void RefMenuPanel::buildWorldExplorerView()
{
    if (!m_navInner || !m_navInnerLay) return;
    const bool hasTerritorios = m_territorioStore && !m_territorioStore->territorios().isEmpty();
    const bool hasSistemas = m_construtorStore && !m_construtorStore->systems().isEmpty();
    if (!hasTerritorios && !hasSistemas) {
        buildPlaceholderView(tr("Explorador de Mundos"),
            tr("Nenhum território ou sistema criado ainda. Use o botão do Construtor na barra superior pra abrir o Criador de Mundos."));
        return;
    }

    const ConstrutorStore::System* openSys = m_currentConstrutorSystemId.isEmpty()
        ? nullptr : m_construtorStore->system(m_currentConstrutorSystemId);
    if (!openSys) m_currentConstrutorSystemId.clear();
    const TerritorioStore::Territorio* openTer = m_currentLugarTerritorioId.isEmpty()
        ? nullptr : m_territorioStore->territorio(m_currentLugarTerritorioId);
    if (!openTer) m_currentLugarTerritorioId.clear();

    auto sectionTitle = [this](const QString& text) {
        auto* lab = new QLabel(text, m_navInner);
        lab->setStyleSheet(QStringLiteral("color:%1; font-size:10px; font-weight:700; letter-spacing:1px; padding:6px 6px 2px;")
                               .arg(Theme::textMuted()));
        m_navInnerLay->addWidget(lab);
    };
    auto makeList = [this]() {
        auto* lw = new QListWidget(m_navInner);
        lw->setSelectionMode(QAbstractItemView::SingleSelection);
        lw->setFrameShape(QFrame::NoFrame);
        lw->setIconSize(QSize(22, 22));
        lw->setStyleSheet(refListStyleSheet());
        return lw;
    };
    auto backRow = [this](const QPixmap& avatar, const QString& name, std::function<void()> onBack) {
        auto* row = new QWidget(m_navInner);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 4);
        rowLay->setSpacing(6);
        auto* back = new QToolButton(row);
        back->setObjectName(QStringLiteral("refTinyBtn"));
        back->setText(QStringLiteral("←"));
        back->setCursor(Qt::PointingHandCursor);
        connect(back, &QToolButton::clicked, this, [onBack]() { onBack(); });
        rowLay->addWidget(back);
        if (!avatar.isNull()) {
            auto* av = new QLabel(row);
            av->setPixmap(avatar);
            av->setFixedSize(avatar.size());
            rowLay->addWidget(av);
        }
        auto* lab = new QLabel(name, row);
        lab->setStyleSheet(QStringLiteral("color:%1; font-size:12px; font-weight:600;").arg(Theme::textPrimary()));
        rowLay->addWidget(lab);
        rowLay->addStretch();
        m_navInnerLay->addWidget(row);
    };

    if (openSys) {
        backRow(QPixmap(), openSys->name, [this]() {
            m_currentConstrutorSystemId.clear();
            rebuildNavBody();
        });

        auto* list = makeList();
        std::function<void(const QString&, const QList<ConstrutorStore::Node>&)> walk;
        walk = [&](const QString& breadcrumb, const QList<ConstrutorStore::Node>& nodes) {
            for (const auto& n : nodes) {
                const QString path = breadcrumb.isEmpty() ? n.name : breadcrumb + QStringLiteral(" ▸ ") + n.name;
                if (matchesSearch(path)) {
                    const QString key = QStringLiteral("ctr:%1:%2").arg(openSys->id, n.id);
                    auto* li = new QListWidgetItem(path);
                    li->setData(Qt::UserRole, key);
                    list->addItem(li);
                    if (key == m_selectedKey) li->setSelected(true);
                }
                walk(path, n.children);
            }
        };
        walk(QString(), openSys->nodes);
        connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            setSelected(it->data(Qt::UserRole).toString());
        });

        if (list->count() == 0) {
            delete list;
            auto* empty = new QLabel(tr("Este sistema ainda não tem regras/seções."), m_navInner);
            empty->setStyleSheet(QStringLiteral("color:%1; font-size:11px; padding:8px;").arg(Theme::textMuted()));
            empty->setAlignment(Qt::AlignCenter);
            m_navInnerLay->addWidget(empty);
            return;
        }
        m_navInnerLay->addWidget(list);
        return;
    }

    if (openTer) {
        const QPixmap avatar = AvatarUtils::circularAvatar(openTer->avatarDataUrl, openTer->name, openTer->id, 22);
        backRow(avatar, openTer->name, [this]() {
            m_currentLugarTerritorioId.clear();
            rebuildNavBody();
        });

        auto* list = makeList();
        std::function<void(const QString&, const QList<TerritorioStore::Node>&)> walk;
        walk = [&](const QString& breadcrumb, const QList<TerritorioStore::Node>& nodes) {
            for (const auto& n : nodes) {
                const QString path = breadcrumb.isEmpty() ? n.name : breadcrumb + QStringLiteral(" ▸ ") + n.name;
                if (matchesSearch(path)) {
                    const QString key = QStringLiteral("lug:%1:%2").arg(openTer->id, n.id);
                    auto* li = new QListWidgetItem(path);
                    li->setData(Qt::UserRole, key);
                    list->addItem(li);
                    if (key == m_selectedKey) li->setSelected(true);
                }
                walk(path, n.children);
            }
        };
        walk(QString(), openTer->nodes);
        connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            setSelected(it->data(Qt::UserRole).toString());
        });

        if (list->count() == 0) {
            delete list;
            auto* empty = new QLabel(tr("Este território ainda não tem pastas/documentos."), m_navInner);
            empty->setStyleSheet(QStringLiteral("color:%1; font-size:11px; padding:8px;").arg(Theme::textMuted()));
            empty->setAlignment(Qt::AlignCenter);
            m_navInnerLay->addWidget(empty);
            return;
        }
        m_navInnerLay->addWidget(list);
        return;
    }

    // Raiz: territórios (avatar real, mesmo AvatarUtils::circularAvatar da
    // StatsPanel) e sistemas (badge de vínculo), como cards — mesmo padrão
    // visual de "refVisualCard" já usado no modo visual da Drawer
    // (buildDrawerView), não a lista de texto simples de antes.
    std::function<int(const QList<TerritorioStore::Node>&)> countTerNodes;
    countTerNodes = [&](const QList<TerritorioStore::Node>& nodes) -> int {
        int n = nodes.size();
        for (const auto& node : nodes) n += countTerNodes(node.children);
        return n;
    };
    std::function<int(const QList<ConstrutorStore::Node>&)> countSysNodes;
    countSysNodes = [&](const QList<ConstrutorStore::Node>& nodes) -> int {
        int n = nodes.size();
        for (const auto& node : nodes) n += countSysNodes(node.children);
        return n;
    };

    auto makeCard = [this](const QPixmap& avatar, const QString& title, const QString& meta,
                            const QStringList& badges, std::function<void()> onClick) {
        auto* card = new QToolButton(m_navInner);
        card->setObjectName(QStringLiteral("refVisualCard"));
        card->setCursor(Qt::PointingHandCursor);
        card->setMinimumHeight(58);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        card->setStyleSheet(Theme::qss(QStringLiteral(R"(
            QToolButton#refVisualCard {
                background: %1;
                border: 1px solid %2;
                border-radius: @radius-control;
                text-align: left;
                padding: 0;
            }
            QToolButton#refVisualCard:hover { background: %3; border-color: %4; }
        )")).arg(Theme::appBackground(), Theme::panelBorder(), Theme::hoverOverlay(), Theme::borderStrong()));

        auto* inner = new QWidget(card);
        inner->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* inLay = new QHBoxLayout(inner);
        inLay->setContentsMargins(10, 8, 10, 8);
        inLay->setSpacing(10);

        auto* avLabel = new QLabel(inner);
        avLabel->setFixedSize(32, 32);
        avLabel->setPixmap(avatar);
        inLay->addWidget(avLabel);

        auto* info = new QWidget(inner);
        auto* infoLay = new QVBoxLayout(info);
        infoLay->setContentsMargins(0, 0, 0, 0);
        infoLay->setSpacing(3);
        auto* nameLab = new QLabel(title, info);
        nameLab->setStyleSheet(QStringLiteral("color:%1; font-size:13px; font-weight:600;").arg(Theme::textBright()));
        infoLay->addWidget(nameLab);
        if (!meta.isEmpty()) {
            auto* metaLab = new QLabel(meta, info);
            metaLab->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(Theme::textMuted()));
            infoLay->addWidget(metaLab);
        }
        if (!badges.isEmpty()) {
            auto* badgeRow = new QWidget(info);
            auto* badgeLay = new QHBoxLayout(badgeRow);
            badgeLay->setContentsMargins(0, 2, 0, 0);
            badgeLay->setSpacing(4);
            for (const auto& b : badges) {
                auto* pill = new QLabel(QStringLiteral(" 🔗 %1 ").arg(b), badgeRow);
                pill->setStyleSheet(Theme::qss(QStringLiteral(
                    "background:%1; color:%2; border-radius: @radius-control; padding:1px 6px; font-size:10px; font-weight:600;")
                    .arg(Theme::hoverOverlay(), Theme::accentDefault())));
                badgeLay->addWidget(pill);
            }
            badgeLay->addStretch();
            infoLay->addWidget(badgeRow);
        }
        inLay->addWidget(info, /*stretch=*/1);

        auto* chev = new QLabel(QStringLiteral("›"), inner);
        chev->setStyleSheet(QStringLiteral("color:%1; font-size:18px;").arg(Theme::textMuted()));
        inLay->addWidget(chev);

        auto* cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(0, 0, 0, 0);
        cardLay->addWidget(inner);

        connect(card, &QToolButton::clicked, this, [onClick]() { onClick(); });
        return card;
    };

    bool anyShown = false;

    if (hasTerritorios) {
        QList<TerritorioStore::Territorio> hits;
        for (const auto& t : m_territorioStore->territorios())
            if (matchesSearch(t.name)) hits.append(t);
        if (!hits.isEmpty()) {
            anyShown = true;
            sectionTitle(tr("TERRITÓRIOS"));
            auto* col = new QWidget(m_navInner);
            auto* colLay = new QVBoxLayout(col);
            colLay->setContentsMargins(0, 0, 0, 0);
            colLay->setSpacing(6);
            for (const auto& t : hits) {
                int linkCount = 0;
                for (const auto& l : m_territorioStore->links())
                    if (l.fromTerritorioId == t.id || l.toTerritorioId == t.id) ++linkCount;
                QString meta = tr("%n documento(s)", nullptr, countTerNodes(t.nodes));
                if (linkCount > 0) meta += tr(" · %n vínculo(s)", nullptr, linkCount);
                const QPixmap avatar = AvatarUtils::circularAvatar(t.avatarDataUrl, t.name, t.id, 32);
                const QString territorioId = t.id;
                colLay->addWidget(makeCard(avatar, t.name, meta, {}, [this, territorioId]() {
                    m_currentConstrutorSystemId.clear();
                    m_currentLugarTerritorioId = territorioId;
                    setSelected(QStringLiteral("lug:%1").arg(territorioId));
                    rebuildNavBody();
                }));
            }
            m_navInnerLay->addWidget(col);
        }
    }

    if (hasSistemas) {
        QList<ConstrutorStore::System> hits;
        for (const auto& s : m_construtorStore->systems())
            if (matchesSearch(s.name)) hits.append(s);
        if (!hits.isEmpty()) {
            anyShown = true;
            sectionTitle(tr("SISTEMAS"));
            const QIcon icSys = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/cube.svg"),
                QColor(Theme::accentDefault()), QColor(Theme::accentDefault()), QColor(Theme::textBright()),
                QSize(16, 16));
            QPixmap sysAvatar(32, 32);
            sysAvatar.fill(Qt::transparent);
            {
                QPainter p(&sysAvatar);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setBrush(QColor(58, 140, 122, 40));
                p.setPen(Qt::NoPen);
                p.drawEllipse(0, 0, 32, 32);
                if (!icSys.isNull()) icSys.paint(&p, 8, 8, 16, 16);
            }
            auto* col = new QWidget(m_navInner);
            auto* colLay = new QVBoxLayout(col);
            colLay->setContentsMargins(0, 0, 0, 0);
            colLay->setSpacing(6);
            for (const auto& s : hits) {
                QString meta;
                if (const auto* cat = ConstrutorStore::categoryById(s.categoryId)) meta = cat->displayName;
                const int nodeCount = countSysNodes(s.nodes);
                if (nodeCount > 0) {
                    if (!meta.isEmpty()) meta += QStringLiteral(" · ");
                    meta += tr("%n regra(s)/seção(ões)", nullptr, nodeCount);
                }
                QStringList badges;
                if (m_territorioStore) {
                    for (const auto& tid : s.territoryIds)
                        if (const auto* t = m_territorioStore->territorio(tid)) badges << t->name;
                }
                const QString systemId = s.id;
                colLay->addWidget(makeCard(sysAvatar, s.name, meta, badges, [this, systemId]() {
                    m_currentLugarTerritorioId.clear();
                    m_currentConstrutorSystemId = systemId;
                    setSelected(QStringLiteral("ctr:%1").arg(systemId));
                    rebuildNavBody();
                }));
            }
            m_navInnerLay->addWidget(col);
        }
    }

    if (!anyShown) {
        auto* empty = new QLabel(tr("Nada encontrado."), m_navInner);
        empty->setStyleSheet(QStringLiteral("color:%1; font-size:11px; padding:8px;").arg(Theme::textMuted()));
        empty->setAlignment(Qt::AlignCenter);
        m_navInnerLay->addWidget(empty);
    }
}

void RefMenuPanel::buildGroupsView()
{
    if (!m_model || !m_navInner || !m_navInnerLay) return;

    // Limpa seleção de grupo se foi removido
    if (!m_currentGroupId.isEmpty() && !m_model->findGroup(m_currentGroupId))
        m_currentGroupId.clear();

    // ---- Seção: picker de grupo ----
    auto* grpTitle = new QLabel(tr("GRUPOS"), m_navInner);
    grpTitle->setStyleSheet(QStringLiteral(
        "color:%1; font-size:10px; font-weight:700; letter-spacing:1px; padding:6px 6px 2px;")
        .arg(Theme::textMuted()));
    m_navInnerLay->addWidget(grpTitle);

    const auto& groups = m_model->groups();

    if (groups.isEmpty()) {
        auto* hint = new QLabel(
            tr("Nenhum grupo criado. Clique com o botão direito num documento de gaveta e escolha \"Adicionar ao grupo\"."),
            m_navInner);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral(
            "color:%1; font-size:11px; padding:8px 6px; font-style:italic;").arg(Theme::textMuted()));
        m_navInnerLay->addWidget(hint);
        return;
    }

    auto* grpList = new QListWidget(m_navInner);
    grpList->setObjectName(QStringLiteral("refListGrp"));
    grpList->setSelectionMode(QAbstractItemView::SingleSelection);
    grpList->setFrameShape(QFrame::NoFrame);
    grpList->setUniformItemSizes(true);
    grpList->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QListWidget {
            background: %1; color: %2;
            border: 1px solid %3; border-radius: @radius-control;
            outline: none; padding: 4px;
        }
        QListWidget::item { padding: 6px 8px; border-radius: @radius-item; }
        QListWidget::item:hover    { background: %4; color: %5; }
        QListWidget::item:selected { background: %6; color: %5; }
    )")).arg(Theme::appBackground(), Theme::textPrimary(), Theme::panelBorder(),
            Theme::hoverOverlay(), Theme::textBright(), Theme::accentInfoSoft()));

    for (const auto& g : groups) {
        QPixmap dot(10, 10);
        dot.fill(Qt::transparent);
        QPainter dotP(&dot);
        dotP.setRenderHint(QPainter::Antialiasing);
        dotP.setBrush(QColor(g.color));
        dotP.setPen(Qt::NoPen);
        dotP.drawEllipse(1, 1, 8, 8);
        dotP.end();

        auto* item = new QListWidgetItem(g.title);
        item->setIcon(QIcon(dot));
        item->setData(Qt::UserRole, g.id);
        grpList->addItem(item);
        if (g.id == m_currentGroupId)
            item->setSelected(true);
    }
    grpList->setFixedHeight(qMin(140, 6 + 30 * qMax(1, groups.size()) + 4));

    connect(grpList, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        if (!it) return;
        const QString id = it->data(Qt::UserRole).toString();
        m_currentGroupId = (m_currentGroupId == id) ? QString() : id;
        changeSelectedKey(QString());
        rebuildNavBody();
        rebuildPreview();
    });
    m_navInnerLay->addWidget(grpList);

    if (m_currentGroupId.isEmpty()) return;

    // ---- Seção: itens do grupo ----
    auto* itemsTitle = new QLabel(tr("DOCUMENTOS NO GRUPO"), m_navInner);
    itemsTitle->setStyleSheet(QStringLiteral(
        "color:%1; font-size:10px; font-weight:700; letter-spacing:1px; padding:10px 6px 2px;")
        .arg(Theme::textMuted()));
    m_navInnerLay->addWidget(itemsTitle);

    auto* itemList = new QListWidget(m_navInner);
    itemList->setObjectName(QStringLiteral("refListGrpItems"));
    itemList->setSelectionMode(QAbstractItemView::SingleSelection);
    itemList->setFrameShape(QFrame::NoFrame);
    itemList->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QListWidget {
            background: %1; color: %2;
            border: 1px solid %3; border-radius: @radius-control;
            outline: none; padding: 4px;
        }
        QListWidget::item { padding: 6px 8px; border-radius: @radius-item; }
        QListWidget::item:hover    { background: %4; color: %5; }
        QListWidget::item:selected { background: %6; color: %5; }
    )")).arg(Theme::appBackground(), Theme::textPrimary(), Theme::panelBorder(),
            Theme::hoverOverlay(), Theme::textBright(), Theme::accentInfoSoft()));

    int shown = 0;
    for (const auto& drawer : m_model->drawers()) {
        for (const auto& di : drawer.items) {
            if (di.markerId != m_currentGroupId) continue;
            if (!m_searchQuery.isEmpty() && !matchesSearch(di.title)) continue;
            const QString label = di.title.isEmpty() ? tr("(sem nome)") : di.title;
            const QString full  = QStringLiteral("%1  —  %2")
                .arg(label, drawer.title.isEmpty() ? tr("gaveta") : drawer.title);
            auto* it = new QListWidgetItem(full);
            it->setData(Qt::UserRole, QStringLiteral("it:%1").arg(di.id));
            if (QStringLiteral("it:%1").arg(di.id) == m_selectedKey)
                it->setSelected(true);
            itemList->addItem(it);
            ++shown;
        }
    }

    if (shown == 0) {
        auto* noItems = new QLabel(tr("Nenhum documento neste grupo"), m_navInner);
        noItems->setStyleSheet(QStringLiteral(
            "color:%1; font-size:11px; padding:6px; font-style:italic;").arg(Theme::textMuted()));
        m_navInnerLay->addWidget(noItems);
    } else {
        itemList->setFixedHeight(qMin(200, 6 + 30 * shown + 4));
        connect(itemList, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            const QString key = it->data(Qt::UserRole).toString();
            setSelected(key);
            rebuildPreview();
        });
        m_navInnerLay->addWidget(itemList);
    }
}

namespace {

// Carrega miniatura a partir de data URL base64 (Element::image) ou de um
// caminho local. Devolve pixmap nulo quando não há imagem — a linha então cai
// na bolinha discreta, mantendo o texto alinhado com as linhas que têm foto.
QPixmap refThumbFrom(const QString& src)
{
    if (src.isEmpty()) return QPixmap();
    QPixmap pm;
    const int comma = src.indexOf(QLatin1Char(','));
    if (src.startsWith(QLatin1String("data:")) && comma > 0) {
        const QByteArray raw = QByteArray::fromBase64(src.mid(comma + 1).toLatin1());
        pm.loadFromData(raw);
    } else {
        pm.load(src);
    }
    return pm;
}

QString refDrawerIconPath(const Drawer& d)
{
    const QString iconId = !d.drawerIcon.isEmpty() ? d.drawerIcon : d.drawerElementIcon;
    const QString path = QStringLiteral(":/icons/elements/%1.svg").arg(iconId);
    return (!iconId.isEmpty() && QFile::exists(path)) ? path : QStringLiteral(":/icons/elements/drawer.svg");
}

// Ícone da gaveta na COR DA GAVETA (a escolhida ao criar), como na LeftBar.
// Cinza sumia em temas de fundo colorido (Desktop 95) e ainda jogava fora a
// cor que o usuário escolheu pra reconhecer a gaveta de relance. Sem cor
// definida, cai na cor de destaque do tema — mesmo fallback da LeftBar.
QIcon refDrawerIcon(const Drawer& d, int px)
{
    const QColor c(d.color.isEmpty() ? Theme::accentDefault() : d.color);
    const QColor accent = c.isValid() ? c : QColor(Theme::accentDefault());
    return IconUtils::loadToolbarIcon(refDrawerIconPath(d), accent, accent, accent, QSize(px, px));
}

QString refPinsBase(const QString& projectRoot)
{
    return QStringLiteral("ui/refMenuPanel/%1/").arg(QString::fromLatin1(
        QCryptographicHash::hash(QDir::cleanPath(projectRoot).toUtf8(),
                                 QCryptographicHash::Sha1).toHex()));
}

} // namespace

void RefMenuPanel::loadPinsAndRecents()
{
    if (m_projectRoot.isEmpty()) return;
    QSettings s;
    const QString base = refPinsBase(m_projectRoot);
    m_pinnedKeys = s.value(base + QStringLiteral("pinned")).toStringList();
    m_recentKeys = s.value(base + QStringLiteral("recent")).toStringList();
    m_tabs = s.value(base + QStringLiteral("tabs")).toStringList();
    m_tabIdx = qBound(0, s.value(base + QStringLiteral("tabIdx"), 0).toInt(), qMax(0, int(m_tabs.size()) - 1));
}

void RefMenuPanel::savePinsAndRecents() const
{
    if (m_projectRoot.isEmpty()) return;
    QSettings s;
    const QString base = refPinsBase(m_projectRoot);
    s.setValue(base + QStringLiteral("pinned"), m_pinnedKeys);
    s.setValue(base + QStringLiteral("recent"), m_recentKeys);
    s.setValue(base + QStringLiteral("tabs"), m_tabs);
    s.setValue(base + QStringLiteral("tabIdx"), m_tabIdx);
}

bool RefMenuPanel::isPinned(const QString& key) const
{
    return m_pinnedKeys.contains(key);
}

void RefMenuPanel::togglePin(const QString& key)
{
    if (key.isEmpty()) return;
    if (m_pinnedKeys.removeAll(key) == 0) m_pinnedKeys.prepend(key);
    savePinsAndRecents();
    if (m_navMode == NavMode::Home && m_searchQuery.isEmpty()) rebuildNavBody();
    rebuildReaderChips();
}

void RefMenuPanel::addRecent(const QString& key)
{
    if (key.isEmpty()) return;
    m_recentKeys.removeAll(key);
    m_recentKeys.prepend(key);
    // Oito cobre uma sessão de escrita sem virar uma segunda lista de tudo.
    while (m_recentKeys.size() > 8) m_recentKeys.removeLast();
    savePinsAndRecents();
}

void RefMenuPanel::setCurrentDocKey(const QString& docKey)
{
    if (m_currentDocKey == docKey) return;
    m_currentDocKey = docKey;
    // Só redesenha se "Nesta cena" estiver à vista.
    if (isVisible() && m_navMode == NavMode::Home && m_searchQuery.isEmpty()) rebuildNavBody();
    if (isVisible()) {
        rebuildRail();          // rostos de quem está na cena
        rebuildReaderChips();
        if (m_mapOn) rebuildUsageMap();   // coluna "no editor"
    }
}

QString RefMenuPanel::editorDocKey() const
{
    // O painel já tem o EditorHost, então descobre sozinho qual documento está
    // aberto — não depende do MainWindow avisar a cada troca de cena. Mesma
    // chave que o ElementsStore usa pra guardar quem está presente.
    if (!m_host || !m_model) return QString();
    const auto vm = m_host->viewMode();
    if (vm.type == EditorHost::SceneDoc) {
        const Chapter* ch = m_model->findChapter(vm.chapterId);
        if (!ch || vm.sceneIndex < 0 || vm.sceneIndex >= ch->scenes.size()) return QString();
        return ElementsStore::elementDocKeyForScene(vm.manuscriptId, vm.chapterId,
                                                    ch->scenes[vm.sceneIndex].id);
    }
    if (vm.type == EditorHost::ChapterDoc) {
        return ElementsStore::elementDocKeyForChapter(vm.manuscriptId, vm.chapterId);
    }
    return QString();
}

bool RefMenuPanel::describeKey(const QString& key, KeyInfo* out) const
{
    if (key.isEmpty() || !out || !m_model) return false;
    const QStringList p = key.split(QLatin1Char(':'));

    if (key.startsWith(QLatin1String("it:")) && p.size() >= 2) {
        for (const Drawer& d : m_model->drawers()) {
            for (const DrawerItem& it : d.items) {
                if (it.id != p.at(1)) continue;
                out->name = it.title.isEmpty() ? tr("(sem título)") : it.title;
                out->sectionLabel = d.title;
                out->meta = d.title;
                out->role = roleOrLabelForItem(it);
                out->imagePath = imageForItem(it);
                return true;
            }
        }
        return false;
    }

    if (key.startsWith(QLatin1String("ch:")) && p.size() >= 3) {
        for (const Chapter& c : m_model->chapters()) {
            if (c.id != p.at(2)) continue;
            out->name = c.title.isEmpty() ? tr("(sem título)") : c.title;
            out->sectionLabel = tr("Manuscrito");
            out->meta = tr("capítulo");
            return true;
        }
        return false;
    }

    if (key.startsWith(QLatin1String("sc:")) && p.size() >= 4) {
        for (const Chapter& c : m_model->chapters()) {
            if (c.id != p.at(2)) continue;
            const int idx = p.at(3).toInt();
            if (idx < 0 || idx >= c.scenes.size()) return false;
            const QString st = c.scenes.at(idx).title;
            out->name = st.isEmpty() ? tr("Cena %1").arg(idx + 1) : st;
            out->sectionLabel = tr("Manuscrito");
            out->meta = c.title.isEmpty() ? tr("capítulo") : c.title;
            return true;
        }
        return false;
    }

    if (key.startsWith(QLatin1String("ctr:")) && m_construtorStore && p.size() >= 2) {
        for (const auto& sys : m_construtorStore->systems()) {
            if (sys.id != p.at(1)) continue;
            if (p.size() >= 3) {
                const ConstrutorStore::Node* n = findConstrutorNode(&sys, p.at(2));
                if (!n) return false;
                out->name = n->name;
                out->meta = sys.name;
            } else {
                out->name = sys.name;
                out->meta = tr("sistema");
            }
            out->sectionLabel = tr("Mundos");
            return true;
        }
        return false;
    }

    if (key.startsWith(QLatin1String("lug:")) && m_territorioStore && p.size() >= 2) {
        for (const auto& ter : m_territorioStore->territorios()) {
            if (ter.id != p.at(1)) continue;
            if (p.size() >= 3) {
                const TerritorioStore::Node* n = findTerritorioNode(&ter, p.at(2));
                if (!n) return false;
                out->name = n->name;
                out->meta = ter.name;
            } else {
                out->name = ter.name.isEmpty() ? tr("(sem nome)") : ter.name;
                out->meta = tr("território");
            }
            out->sectionLabel = tr("Mundos");
            return true;
        }
        return false;
    }

    return false;
}

QWidget* RefMenuPanel::makeNavRow(const QString& key, const KeyInfo& info, bool withPin,
                                  int fontPx)
{
    auto* row = new QWidget(m_navInner);
    row->setObjectName(QStringLiteral("refNavRow"));
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setCursor(Qt::PointingHandCursor);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(6, 5, 6, 5);
    lay->setSpacing(9);

    const QPixmap pm = refThumbFrom(info.imagePath);
    auto* thumb = new QLabel(row);
    thumb->setFixedSize(26, 26);
    if (!pm.isNull()) {
        thumb->setPixmap(pm.scaled(26, 26, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        thumb->setStyleSheet(Theme::qss(QStringLiteral("border-radius: @radius-item;")));
    } else {
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setText(QStringLiteral("•"));
        thumb->setStyleSheet(QStringLiteral("color:%1; font-size:16px;").arg(Theme::textMuted()));
    }
    lay->addWidget(thumb);

    auto* textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(1);
    auto* name = new QLabel(info.name, row);
    name->setMinimumWidth(1);   // coluna estreita corta o nome, não alarga o painel
    name->setStyleSheet(QStringLiteral("color:%1; font-size:%2px;")
        .arg(Theme::textPrimary()).arg(fontPx));
    textCol->addWidget(name);
    // O papel (PROTAGONISTA etc.) fica sob o nome, como nos cards da gaveta.
    const QString sub = !info.role.isEmpty() ? info.role : info.meta;
    if (!sub.isEmpty()) {
        auto* lbl = new QLabel(sub, row);
        lbl->setMinimumWidth(1);
        if (info.role.isEmpty()) {
            lbl->setStyleSheet(QStringLiteral("color:%1; font-size:10px;").arg(Theme::textMuted()));
        } else {
            lbl->setStyleSheet(QStringLiteral(
                "color:%1; font-size:10px; font-weight:700; letter-spacing:.5px;")
                .arg(Theme::accentInfo()));
        }
        textCol->addWidget(lbl);
    }
    lay->addLayout(textCol, 1);

    if (withPin) {
        auto* pin = new QToolButton(row);
        pin->setObjectName(QStringLiteral("refPinBtn"));
        const bool on = isPinned(key);
        pin->setText(on ? QStringLiteral("★") : QStringLiteral("☆"));
        pin->setToolTip(on ? tr("Desafixar") : tr("Fixar no topo"));
        pin->setCursor(Qt::PointingHandCursor);
        pin->setStyleSheet(QStringLiteral(
            "QToolButton{border:0;background:transparent;color:%1;font-size:13px;padding:2px 4px;}")
            .arg(on ? Theme::accentWarning() : Theme::textMuted()));
        connect(pin, &QToolButton::clicked, this, [this, key]() { togglePin(key); });
        lay->addWidget(pin);
    }

    row->setStyleSheet(Theme::qss(QStringLiteral(
        "QWidget#refNavRow { border-radius: @radius-item; }"
        "QWidget#refNavRow:hover { background: %1; }")).arg(Theme::hoverOverlay()));

    row->setProperty("refRowKey", key);
    row->installEventFilter(this);
    return row;
}

void RefMenuPanel::buildHomeView()
{
    if (!m_navInner || !m_navInnerLay) return;

    auto sectionTitle = [this](const QString& text) {
        auto* l = new QLabel(text, m_navInner);
        l->setStyleSheet(QStringLiteral(
            "color:%1; font-size:10px; font-weight:700; letter-spacing:1.2px; padding:10px 6px 2px;")
            .arg(Theme::textMuted()));
        m_navInnerLay->addWidget(l);
    };
    auto hint = [this](const QString& text) {
        auto* l = new QLabel(text, m_navInner);
        l->setWordWrap(true);
        l->setStyleSheet(QStringLiteral("color:%1; font-size:11px; padding:8px 6px;")
            .arg(Theme::textMuted()));
        m_navInnerLay->addWidget(l);
    };

    // Filtro dos estilos (ver homeFilter): o trilho e a coluna do meio do
    // Miller mostram UMA parte desta mesma tela; sem filtro, ela inteira.
    const QString filter = homeFilter();
    const bool showAll = filter.isEmpty();
    const bool branchMode = filter.startsWith(QLatin1String("ms:"))
        || filter.startsWith(QLatin1String("dr:")) || filter == QLatin1String("world");
    auto wants = [&](const char* section) {
        return showAll || filter == QLatin1String("home") || filter == QLatin1String(section);
    };
    int quickRows = 0;

    // ---- Nesta cena ----
    // Leitura pura do ElementsStore, que já mantém quem está presente em cada
    // documento. Nada é recalculado aqui.
    if (wants("scene")) {
        const QList<ScenePerson> people = scenePeople();
        if (!people.isEmpty()) {
            sectionTitle(tr("NESTA CENA"));
            for (const ScenePerson& sp : people) {
                // Sem ficha na gaveta, a linha ainda aparece — saber quem está
                // na cena já vale — só não abre nada.
                KeyInfo info;
                info.name = sp.name;
                info.role = sp.role;
                info.imagePath = sp.image;
                m_navInnerLay->addWidget(makeNavRow(sp.key, info, !sp.key.isEmpty()));
                ++quickRows;
            }
        }
    }

    // ---- Fixados ----
    if (wants("pinned") && !m_pinnedKeys.isEmpty()) {
        QStringList alive;
        QList<QWidget*> rows;
        for (const QString& k : m_pinnedKeys) {
            KeyInfo info;
            if (!describeKey(k, &info)) continue;   // item apagado: some sem alarde
            alive << k;
            rows << makeNavRow(k, info, true);
        }
        if (alive.size() != m_pinnedKeys.size()) {
            m_pinnedKeys = alive;
            savePinsAndRecents();
        }
        if (!rows.isEmpty()) {
            sectionTitle(tr("FIXADOS"));
            for (QWidget* w : rows) m_navInnerLay->addWidget(w);
            quickRows += int(rows.size());
        }
    }
    if (filter == QLatin1String("pinned") && quickRows == 0) {
        sectionTitle(tr("FIXADOS"));
        hint(tr("Nada fixado ainda. A estrela ao lado de um documento o prende aqui."));
    }

    // ---- Recentes ----
    // Na tela inicial, quatro bastam; a lista própria do trilho mostra todos.
    const int recentMax = (filter == QLatin1String("recent")) ? 8 : 4;
    if (wants("recent") && !m_recentKeys.isEmpty()) {
        QStringList alive;
        QList<QWidget*> rows;
        for (const QString& k : m_recentKeys) {
            KeyInfo info;
            if (!describeKey(k, &info)) continue;
            alive << k;
            if (m_pinnedKeys.contains(k) && filter != QLatin1String("recent")) continue; // já apareceu em Fixados
            if (rows.size() < recentMax) rows << makeNavRow(k, info, true);
        }
        if (alive.size() != m_recentKeys.size()) {
            m_recentKeys = alive;
            savePinsAndRecents();
        }
        if (!rows.isEmpty()) {
            sectionTitle(tr("RECENTES"));
            for (QWidget* w : rows) m_navInnerLay->addWidget(w);
            quickRows += int(rows.size());
        }
    }
    if (filter == QLatin1String("recent") && quickRows == 0) {
        sectionTitle(tr("RECENTES"));
        hint(tr("Nada aberto ainda."));
    }
    if (filter == QLatin1String("home") && quickRows == 0) {
        hint(tr("Nada por aqui ainda. Escolha uma fonte ao lado."));
    }

    // Só a tela inteira ou uma fonte específica seguem pra árvore.
    if (!showAll && !branchMode) return;

    // ---- Percorrer ----
    // As fontes viram lista visível, com contagem — antes estavam atrás de dois
    // menus suspensos que não diziam o que havia dentro. Clicar abre a fonte
    // ali mesmo, para baixo, em vez de trocar a tela inteira: o resto da
    // navegação (Nesta cena, Fixados, Recentes) continua à vista.
    if (showAll) sectionTitle(tr("PERCORRER"));

    // Linha de fonte/pasta: mesma estética das outras, mais uma seta que gira.
    // `depth` recua a linha pra mostrar o aninhamento.
    // thumbSrc: capa do manuscrito (data URL) ou imagem da gaveta.
    // iconId: icone do catalogo de gavetas (:/icons/elements/<id>.svg).
    // Com filtro de fonte, só a fonte pedida passa, já aberta e sem a própria
    // linha (o título da seção faz esse papel), e tudo sobe um degrau.
    auto addBranch = [this, branchMode, filter, sectionTitle](const QString& label, const QString& meta,
                            const QString& expandKey, int depth,
                            const QString& thumbSrc = QString(),
                            const QString& iconId = QString(),
                            const QString& docKey = QString(),
                            const QIcon& readyIcon = QIcon()) {
        if (branchMode && depth == 0) {
            if (expandKey != filter) return false;
            sectionTitle(label.toUpper());
            return true;
        }
        if (branchMode) depth -= 1;
        const bool open = m_expanded.contains(expandKey);
        auto* row = new QWidget(m_navInner);
        row->setObjectName(QStringLiteral("refNavRow"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setCursor(Qt::PointingHandCursor);
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(6 + depth * 14, 7, 8, 7);
        lay->setSpacing(8);

        // Com docKey, a linha tem duas acoes: a seta abre/fecha os filhos e o
        // resto abre o documento inteiro. Sem isso, um capitulo com cenas so
        // poderia ser lido cena a cena — nunca do comeco ao fim.
        if (!docKey.isEmpty()) {
            auto* chev = new QToolButton(row);
            chev->setObjectName(QStringLiteral("refChevBtn"));
            chev->setText(open ? QString::fromUtf8("⌄") : QString::fromUtf8("›"));
            chev->setCursor(Qt::PointingHandCursor);
            chev->setToolTip(open ? tr("Recolher cenas") : tr("Mostrar cenas"));
            chev->setStyleSheet(QStringLiteral(
                "QToolButton{border:0;background:transparent;color:%1;font-size:13px;"
                "min-width:14px;padding:0 1px;}").arg(Theme::textMuted()));
            connect(chev, &QToolButton::clicked, this, [this, expandKey]() {
                if (m_expanded.contains(expandKey)) m_expanded.remove(expandKey);
                else m_expanded.insert(expandKey);
                rebuildNavBody();
            });
            lay->addWidget(chev);
        } else {
            auto* chev = new QLabel(open ? QString::fromUtf8("⌄") : QString::fromUtf8("›"), row);
            chev->setFixedWidth(12);
            chev->setAlignment(Qt::AlignCenter);
            chev->setStyleSheet(QStringLiteral("color:%1; font-size:13px;").arg(Theme::textMuted()));
            lay->addWidget(chev);
        }

        // Miniatura da capa (manuscrito) ou icone (gaveta), quando houver —
        // e o que separa a linha-mae das folhas de olho, junto com o negrito.
        const QPixmap tp = refThumbFrom(thumbSrc);
        if (!tp.isNull()) {
            auto* th = new QLabel(row);
            th->setFixedSize(20, 26);
            th->setPixmap(tp.scaled(20, 26, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            th->setStyleSheet(Theme::qss(QStringLiteral("border-radius: @radius-item;")));
            lay->addWidget(th);
        } else if (!readyIcon.isNull() || !iconId.isEmpty()) {
            QIcon ic = !readyIcon.isNull() ? readyIcon : IconUtils::loadToolbarIcon(
                QStringLiteral(":/icons/elements/%1.svg").arg(iconId),
                QColor(Theme::textMuted()), QColor(Theme::textBright()),
                QColor(Theme::textBright()), QSize(15, 15));
            if (!ic.isNull()) {
                auto* th = new QLabel(row);
                th->setFixedSize(18, 18);
                th->setPixmap(ic.pixmap(15, 15));
                th->setAlignment(Qt::AlignCenter);
                lay->addWidget(th);
            }
        }

        auto* name = new QLabel(label, row);
        name->setMinimumWidth(1);
        name->setStyleSheet(QStringLiteral("color:%1; font-size:13px; font-weight:600;")
            .arg(open ? Theme::textBright() : Theme::textPrimary()));
        lay->addWidget(name, 1);

        if (!meta.isEmpty()) {
            auto* cnt = new QLabel(meta, row);
            cnt->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(Theme::textMuted()));
            lay->addWidget(cnt);
        }

        row->setStyleSheet(Theme::qss(QStringLiteral(
            "QWidget#refNavRow { border-radius: @radius-item; }"
            "QWidget#refNavRow:hover { background: %1; }")).arg(Theme::hoverOverlay()));
        if (docKey.isEmpty()) row->setProperty("refExpandKey", expandKey);
        else                  row->setProperty("refRowKey", docKey);
        row->installEventFilter(this);
        m_navInnerLay->addWidget(row);
        return open;
    };

    // Folha da árvore: um documento de verdade, que abre no preview.
    auto addLeaf = [this, branchMode](const QString& key, const KeyInfo& info, int depth) {
        if (branchMode) depth = qMax(0, depth - 1);
        QWidget* w = makeNavRow(key, info, true, 12);   // um degrau abaixo da mae
        if (auto* lay = qobject_cast<QHBoxLayout*>(w->layout())) {
            const QMargins m = lay->contentsMargins();
            lay->setContentsMargins(m.left() + depth * 14, m.top(), m.right(), m.bottom());
        }
        m_navInnerLay->addWidget(w);
    };

    if (m_model) {
        for (const auto& ms : m_model->manuscripts()) {
            QList<Chapter> chs;
            for (const Chapter& c : m_model->chapters())
                if (c.manuscriptId == ms.id) chs.append(c);
            std::sort(chs.begin(), chs.end(),
                      [](const Chapter& a, const Chapter& b) { return a.order < b.order; });

            const bool open = addBranch(ms.title.isEmpty() ? tr("Manuscrito") : ms.title,
                                        tr("%n capítulo(s)", nullptr, int(chs.size())),
                                        QStringLiteral("ms:%1").arg(ms.id), 0,
                                        ms.coverDataUrl);
            if (!open) continue;

            for (const Chapter& c : chs) {
                const QString chKey = QStringLiteral("ch:%1:%2").arg(ms.id, c.id);
                const QString chName = c.title.isEmpty() ? tr("(sem título)") : c.title;

                // Capítulo com mais de uma cena vira galho; com uma só, é folha.
                if (c.scenes.size() > 1) {
                    // Passa a chave do capitulo: da' pra ler o capitulo inteiro
                    // clicando no nome, e abrir as cenas pela seta.
                    const bool chOpen = addBranch(chName,
                                                  tr("%n cena(s)", nullptr, int(c.scenes.size())),
                                                  QStringLiteral("chx:%1").arg(c.id), 1,
                                                  QString(), QString(), chKey);
                    if (chOpen) {
                        for (int i = 0; i < c.scenes.size(); ++i) {
                            const QString st = c.scenes.at(i).title;
                            KeyInfo si;
                            si.name = st.isEmpty() ? tr("Cena %1").arg(i + 1) : st;
                            addLeaf(QStringLiteral("sc:%1:%2:%3").arg(ms.id, c.id).arg(i), si, 2);
                        }
                    }
                } else {
                    KeyInfo ci;
                    ci.name = chName;
                    addLeaf(chKey, ci, 1);
                }
            }
        }

        for (const Drawer& d : m_model->drawers()) {
            const bool open = addBranch(d.title.isEmpty() ? tr("Gaveta") : d.title,
                                        tr("%n item(ns)", nullptr, int(d.items.size())),
                                        QStringLiteral("dr:%1").arg(d.key), 0,
                                        QString(), QString(), QString(), refDrawerIcon(d, 15));
            if (!open) continue;
            for (const DrawerItem& it : d.items) {
                KeyInfo info;
                info.name = it.title.isEmpty() ? tr("(sem título)") : it.title;
                info.role = roleOrLabelForItem(it);
                info.imagePath = imageForItem(it);
                addLeaf(QStringLiteral("it:%1").arg(it.id), info, 1);
            }
        }
    }

    // Mundos: territórios e sistemas juntos, como no Explorador.
    {
        int worlds = 0;
        if (m_territorioStore) worlds += int(m_territorioStore->territorios().size());
        if (m_construtorStore) worlds += int(m_construtorStore->systems().size());
        const bool open = addBranch(tr("Mundos"), tr("%n item(ns)", nullptr, worlds),
                                    QStringLiteral("world"), 0);
        if (open) {
            if (m_territorioStore) {
                for (const auto& ter : m_territorioStore->territorios()) {
                    KeyInfo info;
                    info.name = ter.name.isEmpty() ? tr("(sem nome)") : ter.name;
                    info.meta = tr("território");
                    addLeaf(QStringLiteral("lug:%1").arg(ter.id), info, 1);
                }
            }
            if (m_construtorStore) {
                for (const auto& sys : m_construtorStore->systems()) {
                    KeyInfo info;
                    info.name = sys.name;
                    info.meta = tr("sistema");
                    addLeaf(QStringLiteral("ctr:%1").arg(sys.id), info, 1);
                }
            }
        }
    }
}

void RefMenuPanel::buildManuscriptsView()
{
    if (!m_model || !m_navInner || !m_navInnerLay) return;

    // ---- Seção Capítulos (do manuscrito escolhido via picker) ----
    auto* chTitle = new QLabel(tr("CAPÍTULOS"), m_navInner);
    chTitle->setStyleSheet(QStringLiteral("color:%1; font-size:10px; font-weight:700; letter-spacing:1px; padding:6px 6px 2px;")
                               .arg(Theme::textMuted()));
    m_navInnerLay->addWidget(chTitle);

    auto* chList = new QListWidget(m_navInner);
    chList->setObjectName(QStringLiteral("refListCh"));
    chList->setSelectionMode(QAbstractItemView::SingleSelection);
    chList->setFrameShape(QFrame::NoFrame);
    chList->setIconSize(QSize(14, 14));
    chList->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QListWidget {
            background: %1; color: %2;
            border: 1px solid %3; border-radius: @radius-control;
            outline: none; padding: 4px;
        }
        QListWidget::item { padding: 6px 8px; border-radius: @radius-item; }
        QListWidget::item:hover { background: %4; color: %5; }
        QListWidget::item:selected { background: %6; color: %5; }
    )")).arg(Theme::appBackground(),
           Theme::textPrimary(),
           Theme::panelBorder(),
           Theme::hoverOverlay(),
           Theme::textBright(),
           Theme::accentInfoSoft()));
    QIcon icCh = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/chapter.svg"),
        QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
        QSize(14, 14));

    QList<Chapter> chs;
    QString msId = m_currentManuscriptId;
    if (msId.isEmpty() && !m_model->manuscripts().isEmpty()) {
        msId = m_model->manuscripts().first().id;
        m_currentManuscriptId = msId;
    }
    for (const auto& c : m_model->chapters()) {
        if (c.manuscriptId == msId) chs.append(c);
    }
    std::sort(chs.begin(), chs.end(), [](const Chapter& a, const Chapter& b) {
        return a.order < b.order;
    });
    for (const auto& c : chs) {
        const QString chTitle = c.title.isEmpty() ? tr("(sem título)") : c.title;
        const bool chTitleHit = matchesSearch(chTitle);

        // Coleta cenas que batem na busca (se houver query).
        QList<int> matchedScenes;
        if (c.scenes.size() > 1) {
            for (int i = 0; i < c.scenes.size(); ++i) {
                const auto& sc = c.scenes.at(i);
                const QString scTitle = sc.title.isEmpty() ? tr("Cena %1").arg(i + 1) : sc.title;
                if (matchesSearch(scTitle)) matchedScenes.append(i);
            }
        }

        // Pula capítulo se busca ativa e nem ele nem nenhuma cena batem.
        if (!m_searchQuery.isEmpty() && !chTitleHit && matchedScenes.isEmpty()) {
            continue;
        }

        auto* it = new QListWidgetItem(chTitle);
        if (!icCh.isNull()) it->setIcon(icCh);
        const QString key = QStringLiteral("ch:%1:%2").arg(msId, c.id);
        it->setData(Qt::UserRole, key);
        chList->addItem(it);
        if (key == m_selectedKey) it->setSelected(true);

        if (c.scenes.size() > 1) {
            for (int i = 0; i < c.scenes.size(); ++i) {
                const auto& sc = c.scenes.at(i);
                const QString scTitle = sc.title.isEmpty() ? tr("Cena %1").arg(i + 1) : sc.title;
                // Com busca ativa: só cenas que batem; sem busca: todas.
                if (!m_searchQuery.isEmpty() && !matchedScenes.contains(i)) continue;
                auto* si = new QListWidgetItem(QStringLiteral("       ") + scTitle);
                QFont f = si->font(); f.setItalic(true); si->setFont(f);
                const QString skey = QStringLiteral("sc:%1:%2:%3").arg(msId, c.id).arg(i);
                si->setData(Qt::UserRole, skey);
                chList->addItem(si);
                if (skey == m_selectedKey) si->setSelected(true);
            }
        }
    }
    if (chList->count() == 0) {
        chList->addItem(chs.isEmpty() ? tr("(nenhum capítulo)") : tr("(nenhum resultado)"));
        chList->item(0)->setFlags(Qt::NoItemFlags);
    }
    connect(chList, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        if (!it) return;
        const QString key = it->data(Qt::UserRole).toString();
        if (key.isEmpty()) return;
        setSelected(key);
    });
    m_navInnerLay->addWidget(chList);
}

void RefMenuPanel::buildDrawerView()
{
    if (!m_model || !m_navInner || !m_navInnerLay) return;
    const Drawer* d = m_model->findDrawer(m_currentDrawerKey);
    if (!d) {
        buildPlaceholderView(tr("Gaveta"), tr("Selecione uma gaveta acima."));
        return;
    }

    // Breadcrumb se em pasta
    if (!m_currentFolderId.isEmpty()) {
        const Folder* folder = nullptr;
        for (const auto& f : d->folders) if (f.id == m_currentFolderId) { folder = &f; break; }
        auto* row = new QWidget(m_navInner);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 4);
        rowLay->setSpacing(6);
        auto* back = new QToolButton(row);
        back->setObjectName(QStringLiteral("refTinyBtn"));
        back->setText(QStringLiteral("←"));
        back->setCursor(Qt::PointingHandCursor);
        connect(back, &QToolButton::clicked, this, [this]() {
            m_currentFolderId.clear();
            rebuildNavBody();
        });
        rowLay->addWidget(back);
        auto* lab = new QLabel(folder ? folder->title : tr("Pasta"), row);
        lab->setStyleSheet(QStringLiteral("color:%1; font-size:12px; font-weight:600;").arg(Theme::textPrimary()));
        rowLay->addWidget(lab);
        rowLay->addStretch();
        m_navInnerLay->addWidget(row);
    }

    // Folders strip — pastas em chips horizontais (estilo Mira 1 drawer strip)
    QList<Folder> childFolders;
    for (const auto& f : d->folders) {
        if ((f.parentId.isEmpty() && m_currentFolderId.isEmpty())
            || (f.parentId == m_currentFolderId && !m_currentFolderId.isEmpty())) {
            childFolders.append(f);
        }
    }
    if (!childFolders.isEmpty()) {
        auto* foldersScroll = new QScrollArea(m_navInner);
        foldersScroll->setWidgetResizable(true);
        foldersScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        foldersScroll->setFrameShape(QFrame::NoFrame);
        foldersScroll->setFixedHeight(36);
        foldersScroll->setStyleSheet(QStringLiteral("background:transparent;"));
        auto* host = new QWidget(foldersScroll);
        auto* hLay = new QHBoxLayout(host);
        hLay->setContentsMargins(0, 0, 0, 0);
        hLay->setSpacing(6);
        QIcon icFolder = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/folder.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(12, 12));
        for (const auto& f : childFolders) {
            auto* chip = new QToolButton(host);
            chip->setObjectName(QStringLiteral("refFolderChip"));
            chip->setText(f.title.isEmpty() ? tr("Pasta") : f.title);
            if (!icFolder.isNull()) {
                chip->setIcon(icFolder);
                chip->setIconSize(QSize(12, 12));
                chip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            }
            chip->setCursor(Qt::PointingHandCursor);
            chip->setStyleSheet(Theme::qss(QStringLiteral(R"(
                QToolButton {
                    background: %1; color: %2;
                    border: 1px solid %3; border-radius: @radius-control;
                    padding: 4px 10px; font-size: 11px;
                }
                QToolButton:hover { background: %4; color: %5; }
            )")).arg(Theme::appBackground(),
                   Theme::textPrimary(),
                   Theme::panelBorder(),
                   Theme::hoverOverlay(),
                   Theme::textBright()));
            const QString folderId = f.id;
            connect(chip, &QToolButton::clicked, this, [this, folderId]() {
                m_currentFolderId = folderId;
                rebuildNavBody();
            });
            hLay->addWidget(chip);
        }
        hLay->addStretch();
        foldersScroll->setWidget(host);
        m_navInnerLay->addWidget(foldersScroll);
    }

    // Items da pasta atual (ou raiz) — com filtro de busca
    QList<DrawerItem> items;
    for (const auto& it : d->items) {
        if (it.folderId != m_currentFolderId) continue;
        const QString title = it.title.isEmpty() ? tr("(sem título)") : it.title;
        if (!matchesSearch(title)) continue;
        items.append(it);
    }

    const bool visualDrawer = drawerIsVisual(d);
    const bool useVisual = visualDrawer && m_visualMode;

    if (items.isEmpty()) {
        auto* empty = new QLabel(
            m_searchQuery.isEmpty() ? tr("Sem itens nesta gaveta.") : tr("Nada encontrado."),
            m_navInner);
        empty->setStyleSheet(QStringLiteral("color:%1; font-size:11px; padding:8px;").arg(Theme::textMuted()));
        empty->setAlignment(Qt::AlignCenter);
        m_navInnerLay->addWidget(empty);
        return;
    }

    if (useVisual) {
        auto* gridHost = new QWidget(m_navInner);
        auto* gridLay = new QGridLayout(gridHost);
        gridLay->setContentsMargins(0, 0, 0, 0);
        gridLay->setSpacing(8);
        int row = 0;
        const QString accent = d->color.isEmpty() ? Theme::accentDefault() : d->color;
        for (const auto& it : items) {
            const QString key = QStringLiteral("it:%1").arg(it.id);
            auto* card = new QToolButton(gridHost);
            card->setObjectName(QStringLiteral("refVisualCard"));
            card->setCheckable(true);
            card->setAutoExclusive(false);
            card->setChecked(key == m_selectedKey);
            card->setCursor(Qt::PointingHandCursor);
            card->setMinimumHeight(72);
            card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            card->setStyleSheet(Theme::qss(QStringLiteral(R"(
                QToolButton#refVisualCard {
                    background: %2;
                    border: 1px solid %3;
                    border-radius: @radius-control;
                    text-align: left;
                    padding: 0;
                }
                QToolButton#refVisualCard:hover { background: %4; border-color: %5; }
                QToolButton#refVisualCard:checked { border-color: %1; background: %6; }
            )")).arg(accent,
                   Theme::appBackground(),
                   Theme::panelBorder(),
                   Theme::hoverOverlay(),
                   Theme::borderStrong(),
                   Theme::accentInfoSoft()));

            // Layout interno (foto à esquerda, infos à direita)
            auto* inner = new QWidget(card);
            inner->setAttribute(Qt::WA_TransparentForMouseEvents);
            auto* inLay = new QHBoxLayout(inner);
            inLay->setContentsMargins(8, 8, 10, 8);
            inLay->setSpacing(10);

            auto* photo = new QLabel(inner);
            photo->setFixedSize(56, 56);
            photo->setStyleSheet(Theme::qss(QStringLiteral("background:%1; border-radius: @radius-control;")).arg(Theme::inputBackground()));
            photo->setAlignment(Qt::AlignCenter);
            const QString img = imageForItem(it);
            QPixmap pm;
            if (!img.isEmpty()) {
                if (img.startsWith(QStringLiteral("data:"))) {
                    const int comma = img.indexOf(',');
                    if (comma > 0) {
                        QByteArray b64 = img.mid(comma + 1).toUtf8();
                        QByteArray bin = QByteArray::fromBase64(b64);
                        pm.loadFromData(bin);
                    }
                } else if (!m_projectRoot.isEmpty()) {
                    const QString path = ProjectStorage::joinPath(m_projectRoot, img);
                    pm.load(path);
                }
            }
            if (!pm.isNull()) {
                photo->setPixmap(pm.scaled(56, 56, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
                photo->setScaledContents(false);
            } else {
                photo->setText(QStringLiteral("?"));
                photo->setStyleSheet(Theme::qss(QStringLiteral("background:%1; border-radius: @radius-control; color:%2; font-size:22px; font-weight:700;"))
                                         .arg(Theme::inputBackground(), Theme::disabledText()));
            }
            inLay->addWidget(photo);

            auto* info = new QWidget(inner);
            auto* infoLay = new QVBoxLayout(info);
            infoLay->setContentsMargins(0, 4, 0, 4);
            infoLay->setSpacing(2);
            auto* name = new QLabel(it.title.isEmpty() ? tr("(sem título)") : it.title, info);
            name->setStyleSheet(QStringLiteral("color:%1; font-size:13px; font-weight:600;").arg(Theme::textBright()));
            infoLay->addWidget(name);
            const QString roleText = roleOrLabelForItem(it);
            if (!roleText.isEmpty()) {
                auto* roleLab = new QLabel(roleText.toUpper(), info);
                roleLab->setStyleSheet(QStringLiteral("color:%1; font-size:9px; font-weight:700; letter-spacing:1px;").arg(accent));
                infoLay->addWidget(roleLab);
            }
            infoLay->addStretch();
            inLay->addWidget(info, /*stretch=*/1);

            // monta o "ícone" do toolbutton como o widget interno via setLayout no card
            auto* cardLay = new QVBoxLayout(card);
            cardLay->setContentsMargins(0, 0, 0, 0);
            cardLay->addWidget(inner);

            connect(card, &QToolButton::clicked, this, [this, key]() { setSelected(key); });

            // Filtro por território (M7): esmaece (não esconde) quem não bate.
            if (!m_territorioFilterId.isEmpty()
                && it.origemTerritorioId != m_territorioFilterId
                && it.localAtualTerritorioId != m_territorioFilterId) {
                static constexpr qreal kTerritorioDimOpacity = 0.16; // mesmo valor de TimelineScene
                auto* effect = new QGraphicsOpacityEffect(card);
                effect->setOpacity(kTerritorioDimOpacity);
                card->setGraphicsEffect(effect);
            }

            gridLay->addWidget(card, row++, 0);
        }
        m_navInnerLay->addWidget(gridHost);
    } else {
        auto* list = new QListWidget(m_navInner);
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setFrameShape(QFrame::NoFrame);
        list->setIconSize(QSize(14, 14));
        list->setStyleSheet(Theme::qss(QStringLiteral(R"(
            QListWidget {
                background: %1; color: %2;
                border: 1px solid %3; border-radius: @radius-control;
                outline: none; padding: 4px;
            }
            QListWidget::item { padding: 6px 8px; border-radius: @radius-item; }
            QListWidget::item:hover { background: %4; color: %5; }
            QListWidget::item:selected { background: %6; color: %5; }
        )")).arg(Theme::appBackground(),
               Theme::textPrimary(),
               Theme::panelBorder(),
               Theme::hoverOverlay(),
               Theme::textBright(),
               Theme::accentInfoSoft()));
        QIcon icItemDefault = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/file.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        for (const auto& it : items) {
            const QString key = QStringLiteral("it:%1").arg(it.id);
            QString text = it.title.isEmpty() ? tr("(sem título)") : it.title;
            const QString roleText = roleOrLabelForItem(it);
            if (!roleText.isEmpty()) text.append(QStringLiteral("   ·  ") + roleText);
            auto* li = new QListWidgetItem(text);
            // Ícone: prefere o elementIcon do item (se mapeia uma gaveta visual);
            // senão cai pro default file.svg.
            QIcon used;
            if (!it.elementIcon.isEmpty()) {
                QIcon ic = IconUtils::loadToolbarIcon(
                    QStringLiteral(":/icons/elements/%1.svg").arg(it.elementIcon),
                    QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
                    QSize(14, 14));
                if (!ic.isNull()) used = ic;
            }
            if (used.isNull()) used = icItemDefault;
            if (!used.isNull()) li->setIcon(used);
            li->setData(Qt::UserRole, key);
            list->addItem(li);
            if (key == m_selectedKey) li->setSelected(true);
        }
        connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            setSelected(it->data(Qt::UserRole).toString());
        });
        m_navInnerLay->addWidget(list);
    }
}

void RefMenuPanel::buildSearchAllView()
{
    if (!m_model || !m_navInner || !m_navInnerLay) return;

    auto sectionTitle = [this](const QString& text) {
        auto* lab = new QLabel(text, m_navInner);
        lab->setStyleSheet(QStringLiteral("color:%1; font-size:10px; font-weight:700; letter-spacing:1px; padding:8px 6px 2px;")
                               .arg(Theme::textMuted()));
        m_navInnerLay->addWidget(lab);
    };

    auto makeList = [this]() {
        auto* lw = new QListWidget(m_navInner);
        lw->setFrameShape(QFrame::NoFrame);
        lw->setIconSize(QSize(14, 14));
        lw->setStyleSheet(Theme::qss(QStringLiteral(R"(
            QListWidget {
                background: %1; color: %2;
                border: 1px solid %3; border-radius: @radius-control;
                outline: none; padding: 4px;
            }
            QListWidget::item { padding: 6px 8px; border-radius: @radius-item; }
            QListWidget::item:hover { background: %4; color: %5; }
            QListWidget::item:selected { background: %6; color: %5; }
        )")).arg(Theme::appBackground(),
               Theme::textPrimary(),
               Theme::panelBorder(),
               Theme::hoverOverlay(),
               Theme::textBright(),
               Theme::accentInfoSoft()));
        return lw;
    };

    // ===== Manuscritos =====
    QList<Manuscript> msHits;
    for (const auto& m : m_model->manuscripts()) {
        if (matchesSearch(m.title)) msHits.append(m);
    }
    if (!msHits.isEmpty()) {
        sectionTitle(tr("MANUSCRITOS"));
        auto* lw = makeList();
        const QIcon ic = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/manuscript.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        for (const auto& m : msHits) {
            auto* it = new QListWidgetItem(m.title.isEmpty() ? tr("Manuscrito") : m.title);
            if (!ic.isNull()) it->setIcon(ic);
            it->setData(Qt::UserRole, QStringLiteral("ms:%1").arg(m.id));
            lw->addItem(it);
        }
        lw->setFixedHeight(qMin(160, 6 + 30 * msHits.size() + 4));
        connect(lw, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            const QString key = it->data(Qt::UserRole).toString();
            if (!key.startsWith(QStringLiteral("ms:"))) return;
            m_currentManuscriptId = key.mid(3);
            m_sourceKind = SourceKind::Manuscript;
            changeSelectedKey(QString());
            // sair do modo de busca pra revelar o manuscrito escolhido
            m_searchQuery.clear();
            if (m_searchInput) m_searchInput->clear();
            rebuildCrumbs();
            rebuildNavBody();
            rebuildPreview();
        });
        m_navInnerLay->addWidget(lw);
    }

    // ===== Capítulos + Cenas =====
    struct ChHit {
        QString msId;
        QString chId;
        QString title;
        QList<QPair<int, QString>> sceneHits; // (sceneIndex, sceneTitle)
    };
    QList<ChHit> chHits;
    for (const auto& c : m_model->chapters()) {
        const QString chTitle = c.title.isEmpty() ? tr("(sem título)") : c.title;
        const bool chHit = matchesSearch(chTitle);
        QList<QPair<int, QString>> sceneHits;
        if (c.scenes.size() > 1) {
            for (int i = 0; i < c.scenes.size(); ++i) {
                const auto& sc = c.scenes.at(i);
                const QString scTitle = sc.title.isEmpty() ? tr("Cena %1").arg(i + 1) : sc.title;
                if (matchesSearch(scTitle)) sceneHits.append({ i, scTitle });
            }
        }
        if (chHit || !sceneHits.isEmpty()) {
            ChHit h;
            h.msId = c.manuscriptId;
            h.chId = c.id;
            h.title = chTitle;
            h.sceneHits = sceneHits;
            chHits.append(h);
        }
    }
    if (!chHits.isEmpty()) {
        sectionTitle(tr("CAPÍTULOS"));
        auto* lw = makeList();
        const QIcon icCh = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/chapter.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        for (const auto& h : chHits) {
            auto* it = new QListWidgetItem(h.title);
            if (!icCh.isNull()) it->setIcon(icCh);
            const QString key = QStringLiteral("ch:%1:%2").arg(h.msId, h.chId);
            it->setData(Qt::UserRole, key);
            lw->addItem(it);
            for (const auto& p : h.sceneHits) {
                auto* si = new QListWidgetItem(QStringLiteral("       ") + p.second);
                QFont f = si->font(); f.setItalic(true); si->setFont(f);
                const QString skey = QStringLiteral("sc:%1:%2:%3").arg(h.msId, h.chId).arg(p.first);
                si->setData(Qt::UserRole, skey);
                lw->addItem(si);
            }
        }
        connect(lw, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            const QString key = it->data(Qt::UserRole).toString();
            if (key.isEmpty()) return;
            // Atualiza source/manuscriptId pra coerência caso o usuário tire a busca depois.
            if (key.startsWith(QStringLiteral("ch:")) || key.startsWith(QStringLiteral("sc:"))) {
                const QStringList parts = key.split(QLatin1Char(':'));
                if (parts.size() >= 3) {
                    m_currentManuscriptId = parts.at(1);
                    m_sourceKind = SourceKind::Manuscript;
                }
            }
            setSelected(key);
        });
        m_navInnerLay->addWidget(lw);
    }

    // ===== Gavetas =====
    bool anyDrawerHit = false;
    for (const auto& d : m_model->drawers()) {
        QList<DrawerItem> hits;
        for (const auto& it : d.items) {
            const QString title = it.title.isEmpty() ? tr("(sem título)") : it.title;
            if (matchesSearch(title)) hits.append(it);
        }
        if (hits.isEmpty()) continue;
        anyDrawerHit = true;

        const QString label = d.title.isEmpty() ? tr("Gaveta") : d.title;
        sectionTitle(label.toUpper());
        auto* lw = makeList();
        const QIcon ic = refDrawerIcon(d, 14);
        for (const auto& it : hits) {
            auto* row = new QListWidgetItem(it.title.isEmpty() ? tr("(sem título)") : it.title);
            if (!ic.isNull()) row->setIcon(ic);
            row->setData(Qt::UserRole, QStringLiteral("it:%1").arg(it.id));
            // chave secundária pra navegar pra drawer correta ao clicar
            row->setData(Qt::UserRole + 1, d.key);
            lw->addItem(row);
        }
        const QString drawerKey = d.key;
        connect(lw, &QListWidget::itemClicked, this, [this, drawerKey](QListWidgetItem* it) {
            if (!it) return;
            const QString key = it->data(Qt::UserRole).toString();
            if (!key.startsWith(QStringLiteral("it:"))) return;
            // Move pro modo Drawer correto, mas mantém o resultado destacado.
            m_currentDrawerKey = drawerKey;
            m_sourceKind = SourceKind::Drawer;
            setSelected(key);
        });
        m_navInnerLay->addWidget(lw);
    }

    // ===== Territórios =====
    struct TerHit { QString path; QString key; QString territorioId; };
    QList<TerHit> terHits;
    if (m_territorioStore) {
        for (const auto& t : m_territorioStore->territorios()) {
            if (matchesSearch(t.name))
                terHits.append({ t.name, QStringLiteral("lug:%1").arg(t.id), t.id });
            std::function<void(const QString&, const QList<TerritorioStore::Node>&)> walk;
            walk = [&](const QString& breadcrumb, const QList<TerritorioStore::Node>& nodes) {
                for (const auto& n : nodes) {
                    const QString path = breadcrumb + QStringLiteral(" ▸ ") + n.name;
                    if (matchesSearch(path))
                        terHits.append({ path, QStringLiteral("lug:%1:%2").arg(t.id, n.id), t.id });
                    walk(path, n.children);
                }
            };
            walk(t.name, t.nodes);
        }
    }
    if (!terHits.isEmpty()) {
        sectionTitle(tr("TERRITÓRIOS"));
        auto* lw = makeList();
        for (const auto& h : terHits) {
            const TerritorioStore::Territorio* t = m_territorioStore->territorio(h.territorioId);
            const QIcon ic(AvatarUtils::circularAvatar(
                t ? t->avatarDataUrl : QString(), t ? t->name : h.territorioId, h.territorioId, 14));
            auto* it = new QListWidgetItem(ic, h.path);
            it->setData(Qt::UserRole, h.key);
            lw->addItem(it);
        }
        lw->setFixedHeight(qMin(160, 6 + 30 * terHits.size() + 4));
        connect(lw, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            const QString key = it->data(Qt::UserRole).toString();
            const QStringList parts = key.split(QLatin1Char(':'));
            if (parts.size() < 2) return;
            m_currentConstrutorSystemId.clear(); // garante que a view abra em território, não sistema
            m_currentLugarTerritorioId = parts.at(1);
            m_sourceKind = SourceKind::WorldExplorer;
            m_searchQuery.clear();
            if (m_searchInput) m_searchInput->clear();
            setSelected(key); // seta m_selectedKey + preview, antes do rebuild abaixo
            rebuildCrumbs();
            rebuildNavBody(); // já entra com o nó certo destacado na lista
        });
        m_navInnerLay->addWidget(lw);
    }

    // ===== Sistemas =====
    struct SysHit { QString path; QString key; };
    QList<SysHit> sysHits;
    if (m_construtorStore) {
        for (const auto& s : m_construtorStore->systems()) {
            if (matchesSearch(s.name))
                sysHits.append({ s.name, QStringLiteral("ctr:%1").arg(s.id) });
            std::function<void(const QString&, const QList<ConstrutorStore::Node>&)> walk;
            walk = [&](const QString& breadcrumb, const QList<ConstrutorStore::Node>& nodes) {
                for (const auto& n : nodes) {
                    const QString path = breadcrumb + QStringLiteral(" ▸ ") + n.name;
                    if (matchesSearch(path))
                        sysHits.append({ path, QStringLiteral("ctr:%1:%2").arg(s.id, n.id) });
                    walk(path, n.children);
                }
            };
            walk(s.name, s.nodes);
        }
    }
    if (!sysHits.isEmpty()) {
        sectionTitle(tr("SISTEMAS"));
        auto* lw = makeList();
        const QIcon icSys = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/cube.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        for (const auto& h : sysHits) {
            auto* it = new QListWidgetItem(h.path);
            if (!icSys.isNull()) it->setIcon(icSys);
            it->setData(Qt::UserRole, h.key);
            lw->addItem(it);
        }
        lw->setFixedHeight(qMin(160, 6 + 30 * sysHits.size() + 4));
        connect(lw, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            const QString key = it->data(Qt::UserRole).toString();
            const QStringList parts = key.split(QLatin1Char(':'));
            if (parts.size() < 2) return;
            m_currentLugarTerritorioId.clear(); // garante que a view abra em sistema, não território
            m_currentConstrutorSystemId = parts.at(1);
            m_sourceKind = SourceKind::WorldExplorer;
            m_searchQuery.clear();
            if (m_searchInput) m_searchInput->clear();
            setSelected(key); // seta m_selectedKey + preview, antes do rebuild abaixo
            rebuildCrumbs();
            rebuildNavBody(); // já entra com o nó certo destacado na lista
        });
        m_navInnerLay->addWidget(lw);
    }

    if (msHits.isEmpty() && chHits.isEmpty() && !anyDrawerHit
        && terHits.isEmpty() && sysHits.isEmpty()) {
        auto* empty = new QLabel(tr("Nada encontrado."), m_navInner);
        empty->setStyleSheet(QStringLiteral("color:%1; font-size:12px; padding:16px;").arg(Theme::textMuted()));
        empty->setAlignment(Qt::AlignCenter);
        m_navInnerLay->addWidget(empty);
    }
}

void RefMenuPanel::buildPlaceholderView(const QString& title, const QString& subtitle)
{
    auto* card = new QFrame(m_navInner);
    card->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QFrame {
            background: %1;
            border: 1px dashed %2;
            border-radius: @radius-panel;
        }
    )")).arg(Theme::appBackground(), Theme::panelBorder()));
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(6);
    auto* t = new QLabel(title, card);
    t->setStyleSheet(QStringLiteral("color:%1; font-size:13px; font-weight:700;").arg(Theme::textBright()));
    t->setAlignment(Qt::AlignCenter);
    lay->addWidget(t);
    auto* s = new QLabel(subtitle, card);
    s->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(Theme::textMuted()));
    s->setAlignment(Qt::AlignCenter);
    s->setWordWrap(true);
    lay->addWidget(s);
    m_navInnerLay->addWidget(card);
}

namespace {
// Trecho de busca a partir do texto da memória: primeira linha não-vazia,
// limitada a ~60 chars (QTextEdit::find casa só dentro de uma linha).
QString memSearchQuery(const QString& text)
{
    QString t = text;
    t.replace(QChar(0x2029), QChar('\n'));
    const QStringList lines = t.split(QChar('\n'), Qt::SkipEmptyParts);
    QString q = lines.isEmpty() ? t.trimmed() : lines.first().trimmed();
    if (q.size() > 60) q = q.left(60);
    return q;
}
} // namespace

void RefMenuPanel::openMemoryInRef(const MemoriesStore::Memory& mem)
{
    const QString query = memSearchQuery(mem.text);

    if (mem.sourceType == QStringLiteral("scene") && !mem.chapterId.isEmpty()) {
        enterManuscriptMode(mem.manuscriptId);
        setSelected(QStringLiteral("sc:%1:%2:%3")
                        .arg(mem.manuscriptId, mem.chapterId).arg(mem.sceneIndex));
    } else if (mem.sourceType == QStringLiteral("chapter") && !mem.chapterId.isEmpty()) {
        enterManuscriptMode(mem.manuscriptId);
        setSelected(QStringLiteral("ch:%1:%2").arg(mem.manuscriptId, mem.chapterId));
    } else if (mem.sourceType == QStringLiteral("drawer") && !mem.itemId.isEmpty()) {
        QString drawerKey;
        if (m_model) m_model->findDrawerItem(mem.itemId, &drawerKey);
        if (!drawerKey.isEmpty()) {
            enterDrawerMode(drawerKey);
            setSelected(QStringLiteral("it:%1").arg(mem.itemId));
        }
    } else {
        return;
    }

    if (!isVisible()) openPanel();
    else { show(); raise(); }
    highlightInPreview(query);
}

void RefMenuPanel::highlightInPreview(const QString& query)
{
    if (!m_preview || query.isEmpty()) return;
    // "Ctrl+F" automático: leva o cursor ao início e seleciona o 1º casamento,
    // o que rola até ele e o deixa marcado pela seleção.
    m_preview->moveCursor(QTextCursor::Start);
    if (m_preview->find(query)) {
        m_preview->ensureCursorVisible();
    }
}

// =========================================================================
// Preview
// =========================================================================

void RefMenuPanel::rebuildPreview()
{
    if (!m_preview || !m_previewTitle || !m_previewRole) return;

    // Antes de trocar o conteúdo do preview, salva/encerra edição em
    // andamento (se houver) — senão o texto editado se perde no setHtml.
    updateEditAvailability();
    renderDoc(m_main, m_selectedKey);
}

// Preenche uma página de documento (a viva ou a lateral do Comparar).
void RefMenuPanel::renderDoc(DocView& v, const QString& key)
{
    if (!v.browser || !v.title || !v.role) return;

    // limpa imagens anteriores
    while (v.imgLay && v.imgLay->count() > 0) {
        QLayoutItem* item = v.imgLay->takeAt(0);
        if (auto* w = item->widget()) w->deleteLater();
        delete item;
    }
    v.pix.clear();
    v.labels.clear();
    v.imgScroll->setVisible(false);

    if (key.isEmpty()) {
        v.title->setVisible(false);
        v.role->setVisible(false);
        v.browser->setVisible(false);
        v.placeholder->setVisible(true);
        return;
    }
    v.placeholder->setVisible(false);
    v.browser->setVisible(true);
    if (v.folio) {
        QString folio;
        if (m_layout == Layout::Book && m_model
            && (key.startsWith(QLatin1String("ch:")) || key.startsWith(QLatin1String("sc:")))) {
            const QStringList parts = key.split(QLatin1Char(':'));
            if (parts.size() >= 3) {
                QList<Chapter> chs;
                for (const Chapter& c : m_model->chapters())
                    if (c.manuscriptId == parts.at(1)) chs.append(c);
                std::sort(chs.begin(), chs.end(), [](const Chapter& a, const Chapter& b) { return a.order < b.order; });
                for (int i = 0; i < chs.size(); ++i)
                    if (chs.at(i).id == parts.at(2)) folio = QString::fromUtf8("— %1 —").arg(i + 1);
            }
        }
        v.folio->setText(folio);
        v.folio->setVisible(!folio.isEmpty());
    }

    // Resolve título e role
    QString title;
    QString role;
    if (key.startsWith(QStringLiteral("ch:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        if (parts.size() >= 3 && m_model) {
            const Chapter* ch = m_model->findChapter(parts.at(2));
            if (ch) title = ch->title;
        }
    } else if (key.startsWith(QStringLiteral("sc:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        if (parts.size() >= 4 && m_model) {
            const Chapter* ch = m_model->findChapter(parts.at(2));
            int idx = parts.at(3).toInt();
            if (ch && idx >= 0 && idx < ch->scenes.size()) {
                const Scene& sc = ch->scenes.at(idx);
                title = sc.title.isEmpty() ? tr("Cena %1").arg(idx + 1) : sc.title;
            }
        }
    } else if (key.startsWith(QStringLiteral("it:"))) {
        const QString itemId = key.mid(3);
        const DrawerItem* it = m_model ? m_model->findDrawerItem(itemId) : nullptr;
        if (it) {
            title = it->title;
            role = roleOrLabelForItem(*it);
        }
    } else if (key.startsWith(QStringLiteral("ctr:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        const ConstrutorStore::System* sys =
            (m_construtorStore && parts.size() >= 2) ? m_construtorStore->system(parts.at(1)) : nullptr;
        if (sys && parts.size() >= 3) {
            if (const ConstrutorStore::Node* node = findConstrutorNode(sys, parts.at(2))) {
                title = node->name;
                role = tr("%1 · %2").arg(
                    node->type == ConstrutorStore::NodeType::Rule ? tr("Regra") : tr("Seção"), sys->name);
            }
        } else if (sys) {
            title = sys->name;
            role = tr("Sistema · Explorador de Mundos");
        }
    } else if (key.startsWith(QStringLiteral("lug:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        const TerritorioStore::Territorio* ter =
            (m_territorioStore && parts.size() >= 2) ? m_territorioStore->territorio(parts.at(1)) : nullptr;
        if (ter && parts.size() >= 3) {
            if (const TerritorioStore::Node* node = findTerritorioNode(ter, parts.at(2))) {
                title = node->name;
                role = tr("%1 · %2").arg(
                    node->type == TerritorioStore::NodeType::Doc ? tr("Documento") : tr("Pasta"), ter->name);
            }
        } else if (ter) {
            title = ter->name;
            role = tr("Território · Explorador de Mundos");
        }
    }

    v.title->setText(title.isEmpty() ? tr("(sem título)") : title);
    v.title->setVisible(true);
    if (!role.isEmpty()) {
        v.role->setText(role.toUpper());
        v.role->setVisible(true);
    } else {
        v.role->setVisible(false);
    }

    // HTML
    const QString html = resolveDocHtml(key);
    QStringList images;
    QString rest;
    extractImagesFromHtml(html, &images, &rest);

    // Imagens no topo
    if (!images.isEmpty() && v.imgLay) {
        for (const QString& imgHtml : images) {
            QRegularExpression re(QStringLiteral("src=\"([^\"]*)\""));
            QRegularExpressionMatch m = re.match(imgHtml);
            QString src = m.hasMatch() ? m.captured(1) : QString();
            QString resolved = resolveImageSrc(src);
            QPixmap pm;
            if (resolved.startsWith(QStringLiteral("data:"))) {
                const int comma = resolved.indexOf(',');
                if (comma > 0) {
                    QByteArray bin = QByteArray::fromBase64(resolved.mid(comma + 1).toUtf8());
                    pm.loadFromData(bin);
                }
            } else if (!resolved.isEmpty()) {
                pm.load(resolved);
            }
            if (pm.isNull()) continue;
            auto* lab = new QLabel(v.imgHost);
            lab->setAlignment(Qt::AlignCenter);
            lab->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            v.pix.append(pm);
            v.labels.append(lab);
            v.imgLay->addWidget(lab);
        }
        if (!v.labels.isEmpty()) {
            v.imgScroll->setVisible(true);
            rescaleDoc(v);
        }
    }

    // Conteúdo HTML
    if (rest.trimmed().isEmpty()) {
        v.browser->setHtml(QStringLiteral("<p style='color:%2;'>%1</p>")
                               .arg(tr("(documento vazio)"), Theme::textMuted()));
    } else {
        v.browser->setHtml(rest);
    }

    // O HTML salvo vem com `color:` inline do tema da hora em que foi escrito
    // (charformat embutido pelo toHtml). Sobrescreve com a cor do tema atual
    // pra não ficar branco no tema claro. Onde houver background de marker,
    // foreground vai por contraste WCAG (mesma lógica do editor).
    DocPreview::applyThemeTextColors(v.browser->document());

    // Resolve src de imagens internas remanescentes para QTextBrowser (caso restem inline)
    // Simples: o QTextDocument resolve via setSearchPaths se houver projectRoot.
    if (!m_projectRoot.isEmpty()) {
        QStringList paths;
        paths << m_projectRoot << ProjectStorage::joinPath(m_projectRoot, QStringLiteral("content/images"));
        v.browser->setSearchPaths(paths);
    }

    // Doc novo herda o tamanho de fonte do ciclo Aa, vencendo o font-size
    // inline do HTML salvo.
    applyDocFont(v);
}

void RefMenuPanel::rescalePreviewImages()
{
    rescaleDoc(m_main);
    rescaleDoc(m_side);
}

void RefMenuPanel::rescaleDoc(DocView& v)
{
    if (!v.imgScroll || v.pix.isEmpty()) return;

    const int w = v.imgScroll->viewport()->width();
    if (w <= 0) return;

    int totalH = 0;
    for (int i = 0; i < v.pix.size() && i < v.labels.size(); ++i) {
        const QPixmap& pm = v.pix.at(i);
        QLabel* lab = v.labels.at(i);
        if (!lab || pm.isNull()) continue;
        const int h = pm.height() * w / qMax(1, pm.width());
        lab->setFixedHeight(h);
        lab->setPixmap(pm.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        if (totalH > 0) totalH += v.imgLay->spacing();
        totalH += h;
    }

    // A QScrollArea não cresce com o conteúdo por conta própria (widgetResizable
    // só controla a largura/altura do widget interno). Ajusta a altura da área
    // pra caber a(s) imagem(ns) inteira(s), até o teto de 360px (acima disso,
    // scroll vertical).
    v.imgScroll->setFixedHeight(qBound(1, totalH, 360));
}

void RefMenuPanel::setEditorFontFamily(const QString& family)
{
    if (m_editorFontFamily == family) return;
    m_editorFontFamily = family;
    applyPreviewFont();
}

void RefMenuPanel::applyPreviewFont()
{
    applyDocFont(m_main);
    applyDocFont(m_side);
}

void RefMenuPanel::applyDocFont(DocView& v)
{
    if (!v.browser) return;
    QFont f = v.browser->font();
    f.setPointSize(m_previewFontPt);
    if (!m_editorFontFamily.isEmpty()) f.setFamily(m_editorFontFamily);
    v.browser->setFont(f);

    // HTML salvo do Mira 1 vem com font-size inline em cada bloco (toHtml do
    // QTextDocument embute charformat). Sem isso, setFont é silenciosamente
    // derrotado. mergeCharFormat só toca charFormat — não mexe em indent,
    // line-height nem espaçamento de parágrafo (esses ficam em blockFormat).
    DocPreview::applyPreviewFontSize(v.browser->document(), m_previewFontPt);

    // título escala junto
    if (v.title) {
        QFont tf = v.title->font();
        tf.setPointSize(m_previewFontPt + 2);
        tf.setBold(true);
        // Livro aberto: título de página impressa, na fonte da escrita.
        tf.setFamily(m_layout == Layout::Book
            ? (m_editorFontFamily.isEmpty() ? QStringLiteral("Georgia") : m_editorFontFamily)
            : QApplication::font().family());
        v.title->setFont(tf);
    }
}

QString RefMenuPanel::resolveDocHtml(const QString& key) const
{
    if (!m_model) return QString();

    if (key.startsWith(QStringLiteral("ch:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        if (parts.size() < 3) return QString();
        const QString msId = parts.at(1);
        const QString chId = parts.at(2);
        const QString cacheKey = DocCache::chapterKey(msId, chId);
        if (m_cache && m_cache->has(cacheKey)) return m_cache->get(cacheKey);
        const Chapter* ch = m_model->findChapter(chId);
        if (!ch || ch->file.isEmpty() || m_projectRoot.isEmpty()) return QString();
        bool ok = false;
        return ProjectStorage::readChapter(m_projectRoot, ch->file, &ok);
    }
    if (key.startsWith(QStringLiteral("sc:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        if (parts.size() < 4) return QString();
        const QString msId = parts.at(1);
        const QString chId = parts.at(2);
        const int sceneIdx = parts.at(3).toInt();
        const QString cacheKey = DocCache::chapterKey(msId, chId);
        QString chapterHtml;
        if (m_cache && m_cache->has(cacheKey)) {
            chapterHtml = m_cache->get(cacheKey);
        } else if (const Chapter* ch = m_model->findChapter(chId); ch && !m_projectRoot.isEmpty()) {
            bool ok = false;
            chapterHtml = ProjectStorage::readChapter(m_projectRoot, ch->file, &ok);
        }
        return SceneUtils::getSceneHtml(chapterHtml, sceneIdx);
    }
    if (key.startsWith(QStringLiteral("it:"))) {
        const QString itemId = key.mid(3);
        const DrawerItem* item = m_model->findDrawerItem(itemId);
        return DocPreview::resolveDrawerItemHtml(item, m_elements, m_cache, m_projectRoot);
    }
    if (key.startsWith(QStringLiteral("ctr:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        if (!m_construtorStore || parts.size() < 2) return QString();
        const ConstrutorStore::System* sys = m_construtorStore->system(parts.at(1));
        if (!sys) return QString();
        if (parts.size() < 3) return sys->content;
        const ConstrutorStore::Node* node = findConstrutorNode(sys, parts.at(2));
        return node ? node->content : QString();
    }
    if (key.startsWith(QStringLiteral("lug:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        if (!m_territorioStore || parts.size() < 2) return QString();
        const TerritorioStore::Territorio* ter = m_territorioStore->territorio(parts.at(1));
        if (!ter) return QString();
        if (parts.size() < 3) return ter->content;
        const TerritorioStore::Node* node = findTerritorioNode(ter, parts.at(2));
        return node ? node->content : QString();
    }
    return QString();
}

void RefMenuPanel::extractImagesFromHtml(const QString& html, QStringList* imagesOut, QString* restOut) const
{
    if (restOut) restOut->clear();
    if (imagesOut) imagesOut->clear();
    if (html.isEmpty()) return;

    // Imagens flutuantes (inseridas via QTextFrame no editor principal) são
    // serializadas pelo toHtml() como uma <table> envolvendo o <img>. Extrai
    // primeiro esses wrappers completos, senão a <table> vazia sobra no HTML
    // e renderiza como um retângulo com borda no preview.
    QRegularExpression reTable(QStringLiteral("<table\\b[^>]*>(?:(?!</table>).)*?(<img\\b[^>]*?>)(?:(?!</table>).)*?</table>"),
                                QRegularExpression::CaseInsensitiveOption
                                | QRegularExpression::DotMatchesEverythingOption);

    QString working = html;
    int last = 0;
    QString out;
    auto itTable = reTable.globalMatch(working);
    while (itTable.hasNext()) {
        QRegularExpressionMatch m = itTable.next();
        if (m.capturedStart() > last) out.append(working.mid(last, m.capturedStart() - last));
        if (imagesOut) imagesOut->append(m.captured(1));
        last = m.capturedEnd();
    }
    if (last < working.size()) out.append(working.mid(last));
    working = out;

    // Segunda passada: imagens "soltas" (não envolvidas por <table>).
    QRegularExpression re(QStringLiteral("<img\\b[^>]*?>"),
                          QRegularExpression::CaseInsensitiveOption);
    last = 0;
    out.clear();
    auto it = re.globalMatch(working);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        if (m.capturedStart() > last) out.append(working.mid(last, m.capturedStart() - last));
        if (imagesOut) imagesOut->append(m.captured(0));
        last = m.capturedEnd();
    }
    if (last < working.size()) out.append(working.mid(last));
    if (restOut) *restOut = out;
}

QString RefMenuPanel::resolveImageSrc(const QString& src) const
{
    if (src.isEmpty()) return src;
    if (src.startsWith(QStringLiteral("data:"))) return src;
    // Imagens inseridas pelo editor principal usam URL absoluta file:///...
    QUrl url(src);
    if (url.isLocalFile()) return url.toLocalFile();
    if (m_projectRoot.isEmpty()) return src;
    QString s = src;
    if (s.startsWith(QStringLiteral("./"))) s = s.mid(2);
    return ProjectStorage::joinPath(m_projectRoot, s);
}

// =========================================================================
// Drawer picker menu
// =========================================================================



// =========================================================================
// Estado: modos
// =========================================================================

void RefMenuPanel::enterManuscriptMode(const QString& msId)
{
    // Entrar por fora (mencao, Pensario, atalho) tambem cai na arvore: abre o
    // galho certo em vez de trocar pra outra tela.
    m_navMode = NavMode::Home;
    m_sourceKind = SourceKind::Manuscript;
    if (!msId.isEmpty()) m_currentManuscriptId = msId;
    if (m_currentManuscriptId.isEmpty() && m_model && !m_model->manuscripts().isEmpty()) {
        m_currentManuscriptId = m_model->manuscripts().first().id;
    }
    if (!m_currentManuscriptId.isEmpty())
        m_expanded.insert(QStringLiteral("ms:%1").arg(m_currentManuscriptId));
    if (m_layout == Layout::Miller && !m_currentManuscriptId.isEmpty())
        m_millerSource = QStringLiteral("ms:%1").arg(m_currentManuscriptId);
    if (m_layout == Layout::Gallery && !m_currentManuscriptId.isEmpty())
        m_gallerySource = QStringLiteral("ms:%1").arg(m_currentManuscriptId);
    m_currentFolderId.clear();
    refresh();
}

void RefMenuPanel::enterDrawerMode(const QString& drawerKey)
{
    m_navMode = NavMode::Home;
    m_sourceKind = SourceKind::Drawer;
    m_currentDrawerKey = drawerKey;
    if (!drawerKey.isEmpty()) m_expanded.insert(QStringLiteral("dr:%1").arg(drawerKey));
    if (m_layout == Layout::Miller && !drawerKey.isEmpty())
        m_millerSource = QStringLiteral("dr:%1").arg(drawerKey);
    if (m_layout == Layout::Gallery && !drawerKey.isEmpty())
        m_gallerySource = QStringLiteral("dr:%1").arg(drawerKey);
    m_currentFolderId.clear();
    changeSelectedKey(QString());
    refresh();
}

void RefMenuPanel::enterPlaceholderMode(SourceKind kind)
{
    m_navMode = NavMode::Home;
    if (kind == SourceKind::WorldExplorer) m_expanded.insert(QStringLiteral("world"));
    if (m_layout == Layout::Miller && kind == SourceKind::WorldExplorer)
        m_millerSource = QStringLiteral("world");
    m_sourceKind = kind;
    m_currentConstrutorSystemId.clear();
    m_currentLugarTerritorioId.clear();
    changeSelectedKey(QString());
    refresh();
}

void RefMenuPanel::changeSelectedKey(const QString& key)
{
    if (m_selectedKey == key) return;
    m_selectedKey = key;
    if (!m_syncingTabs) syncTabs(key);
    emit selectedKeyChanged(selectedCacheKey());
}

QString RefMenuPanel::selectedCacheKey() const
{
    if (m_selectedKey.isEmpty()) return QString();
    // "ch:<ms>:<ch>" → já é chave de cache
    if (m_selectedKey.startsWith(QStringLiteral("ch:"))) return m_selectedKey;
    // "sc:<ms>:<ch>:<idx>" → cena vive no cache do capítulo pai
    if (m_selectedKey.startsWith(QStringLiteral("sc:"))) {
        const QStringList p = m_selectedKey.split(QLatin1Char(':'));
        if (p.size() >= 3) return DocCache::chapterKey(p.at(1), p.at(2));
    }
    // "it:<id>" → já é chave de cache
    if (m_selectedKey.startsWith(QStringLiteral("it:"))) return m_selectedKey;
    return QString();
}

QString RefMenuPanel::editableCacheKey() const
{
    // Território/Sistema do Criador de Mundos — editável direto (persiste
    // em TerritorioStore/ConstrutorStore, não no DocCache), liberado sempre
    // que o alvo ainda existir. Nunca abre no editor principal, então o
    // guard de conflito abaixo não se aplica a esses dois prefixos.
    if (m_selectedKey.startsWith(QStringLiteral("ctr:"))) {
        const QStringList parts = m_selectedKey.split(QLatin1Char(':'));
        if (parts.size() < 2 || !m_construtorStore) return QString();
        return m_construtorStore->system(parts.at(1)) ? m_selectedKey : QString();
    }
    if (m_selectedKey.startsWith(QStringLiteral("lug:"))) {
        const QStringList parts = m_selectedKey.split(QLatin1Char(':'));
        if (parts.size() < 2 || !m_territorioStore) return QString();
        return m_territorioStore->territorio(parts.at(1)) ? m_selectedKey : QString();
    }

    // Só capítulos e itens de gaveta têm um HTML completo de 1:1 com a
    // chave de cache. Cenas individuais (parte de um capítulo) ficam de
    // fora por ora.
    if (!m_selectedKey.startsWith(QStringLiteral("ch:"))
        && !m_selectedKey.startsWith(QStringLiteral("it:"))) {
        return QString();
    }
    const QString key = selectedCacheKey();
    if (key.isEmpty()) return QString();
    // Evita conflito com o editor principal: se o mesmo doc está aberto lá,
    // edição fica bloqueada no RefMenu.
    if (m_host && m_host->activeKey() == key) return QString();
    return key;
}

void RefMenuPanel::updateEditAvailability()
{
    if (!m_editBtn) return;

    if (m_editing) {
        commitEdit();
        m_editing = false;
        m_editingKey.clear();
        if (m_preview) {
            m_preview->setReadOnly(true);
            m_preview->setStyleSheet(QString());
        }
        QSignalBlocker b(m_editBtn);
        m_editBtn->setChecked(false);
    }

    const QString key = editableCacheKey();
    m_editBtn->setEnabled(!key.isEmpty());

    if (!key.isEmpty()) {
        m_editBtn->setToolTip(tr("Editar documento"));
    } else if (m_selectedKey.startsWith(QStringLiteral("sc:"))) {
        m_editBtn->setToolTip(tr("Edição de cena individual em breve"));
    } else if (m_host && !m_selectedKey.isEmpty() && m_host->activeKey() == selectedCacheKey()) {
        m_editBtn->setToolTip(tr("Este documento está aberto no editor principal"));
    } else {
        m_editBtn->setToolTip(tr("Editar documento"));
    }
}

void RefMenuPanel::commitEdit()
{
    if (!m_editing || !m_preview || m_editingKey.isEmpty()) return;

    if (m_editingKey.startsWith(QStringLiteral("ctr:"))) {
        const QStringList parts = m_editingKey.split(QLatin1Char(':'));
        if (parts.size() < 2 || !m_construtorStore) return;
        const ConstrutorStore::System* sys = m_construtorStore->system(parts.at(1));
        if (!sys) return;
        const QString html = m_preview->toHtml();
        if (parts.size() >= 3) {
            if (const ConstrutorStore::Node* node = findConstrutorNode(sys, parts.at(2)))
                m_construtorStore->updateNode(sys->id, node->id, node->name, html);
        } else {
            m_construtorStore->updateSystemContent(sys->id, html);
        }
        return;
    }
    if (m_editingKey.startsWith(QStringLiteral("lug:"))) {
        const QStringList parts = m_editingKey.split(QLatin1Char(':'));
        if (parts.size() < 2 || !m_territorioStore) return;
        const TerritorioStore::Territorio* ter = m_territorioStore->territorio(parts.at(1));
        if (!ter) return;
        const QString html = m_preview->toHtml();
        if (parts.size() >= 3) {
            if (const TerritorioStore::Node* node = findTerritorioNode(ter, parts.at(2)))
                m_territorioStore->updateNode(ter->id, node->id, node->name, html);
        } else {
            m_territorioStore->updateTerritorioContent(ter->id, html);
        }
        return;
    }

    if (!m_cache) return;
    m_cache->set(m_editingKey, m_preview->toHtml(), /*markDirty=*/true);
}

void RefMenuPanel::onToggleEdit(bool on)
{
    if (!m_preview) return;

    if (on) {
        m_editingKey = editableCacheKey();
        if (m_editingKey.isEmpty()) {
            QSignalBlocker b(m_editBtn);
            m_editBtn->setChecked(false);
            return;
        }
        m_editing = true;
        m_preview->setReadOnly(false);
        m_preview->setStyleSheet(QStringLiteral("QTextBrowser#refPreview { background: %1; }")
                                      .arg(Theme::inputBackground()));
        m_preview->setFocus();
    } else {
        commitEdit();
        m_editing = false;
        m_editingKey.clear();
        m_preview->setReadOnly(true);
        m_preview->setStyleSheet(QString());
    }
}

void RefMenuPanel::setSelected(const QString& key)
{
    changeSelectedKey(key);
    rebuildPreview();
    rebuildCrumbs();
    // Galeria: o documento sobe por cima do mural. Nos outros estilos, a
    // navegação flutuante cumpriu o papel e fecha — adiado porque quem chama
    // costuma ser uma linha dentro dela, ainda no meio do próprio evento.
    if (m_layout == Layout::Gallery) {
        if (!key.isEmpty()) QTimer::singleShot(0, this, [this]() {
            if (!m_selectedKey.isEmpty()) showFloat(FloatKind::DocSheet, QString());
        });
    } else if (m_floatKind != FloatKind::None && !key.isEmpty()) {
        QTimer::singleShot(0, this, [this]() { hideFloat(); });
    }
    rebuildRail();
    rebuildReaderChips();
    rebuildDividers();
    if (m_mapOn) rebuildUsageMap();   // coluna e linha selecionadas
    if (m_layout == Layout::Book && m_navPane && m_navPane->isVisible()) rebuildNavBody();
}

// =========================================================================
// Open / close / toggles
// =========================================================================

void RefMenuPanel::openPanel()
{
    if (m_sourceKind == SourceKind::Manuscript && m_currentManuscriptId.isEmpty() && m_model
        && !m_model->manuscripts().isEmpty()) {
        m_currentManuscriptId = m_model->manuscripts().first().id;
    }
    refresh();
    // Abas lembradas: o painel reabre no documento da aba ativa.
    if (m_selectedKey.isEmpty() && m_tabIdx >= 0 && m_tabIdx < m_tabs.size()
        && !m_tabs.at(m_tabIdx).isEmpty())
        setSelected(m_tabs.at(m_tabIdx));
    show();
    raise();
    nudgeClearOfChrome();
    emit geometryChanged();
}

// O RefMenu e uma janela livre com geometria propria salva (o usuario escolhe
// onde ela fica), entao NAO reposicionamos por gosto: so empurramos pra
// esquerda quando ela ficaria literalmente escondida atras da barra lateral.
// Sem isto, quem tinha o painel salvo no canto direito de antes da barra virar
// vertical passa a abri-lo debaixo dela.
void RefMenuPanel::nudgeClearOfChrome()
{
    if (m_rightInset <= 0) return;
    QWidget* p = parentWidget();
    if (!p || !p->window()) return;
    const QRect win = p->window()->geometry();
    constexpr int kGap = 12;
    const int limit = win.right() - m_rightInset - kGap;
    QRect g = geometry();
    if (g.right() <= limit) return; // ja esta livre
    g.moveRight(qMax(win.left() + kGap, limit));
    setGeometry(g);
    scheduleGeometrySave();
}

void RefMenuPanel::closePanel()
{
    if (m_editing) {
        commitEdit();
        m_editing = false;
        m_editingKey.clear();
        if (m_preview) {
            m_preview->setReadOnly(true);
            m_preview->setStyleSheet(QString());
        }
        if (m_editBtn) {
            QSignalBlocker b(m_editBtn);
            m_editBtn->setChecked(false);
        }
    }
    hide();
    emit geometryChanged();
}

void RefMenuPanel::togglePanel()
{
    if (isVisible()) closePanel();
    else openPanel();
}

void RefMenuPanel::openForDrawer(const QString& drawerKey, const QString& itemId)
{
    enterDrawerMode(drawerKey);
    if (!itemId.isEmpty()) setSelected(QStringLiteral("it:%1").arg(itemId));
    if (!isVisible()) openPanel();
    else { show(); raise(); }
}

void RefMenuPanel::openForChapter(const QString& manuscriptId, const QString& chapterId)
{
    enterManuscriptMode(manuscriptId);
    setSelected(QStringLiteral("ch:%1:%2").arg(manuscriptId, chapterId));
    if (!isVisible()) openPanel();
    else { show(); raise(); }
}

void RefMenuPanel::openForScene(const QString& manuscriptId, const QString& chapterId, int sceneIndex)
{
    enterManuscriptMode(manuscriptId);
    setSelected(QStringLiteral("sc:%1:%2:%3").arg(manuscriptId, chapterId).arg(sceneIndex));
    if (!isVisible()) openPanel();
    else { show(); raise(); }
}

void RefMenuPanel::onCloseClicked()
{
    closePanel();
}

void RefMenuPanel::onToggleNav()
{
    // Estilos sem coluna fixa (ou coluna que não cabe no painel estreito):
    // o botão abre e fecha a navegação flutuante.
    if (m_layout == Layout::Overlay || m_layout == Layout::Reader || m_layout == Layout::Dock
        || m_layout == Layout::Binder || m_autoCollapsed) {
        if (m_floatKind != FloatKind::None) hideFloat();
        else showFloat(floatKindForLayout(), QString());
        return;
    }
    if (m_floatKind != FloatKind::None) hideFloat();
    m_navHidden = !m_navHidden;
    applyNavVisibility();
    QSettings().setValue(navHiddenKeyFor(m_layout), m_navHidden);
}

void RefMenuPanel::applyNavVisibility()
{
    updateToggleNavButton();
    if (m_crumbsRow) rebuildCrumbs();   // a trilha decide sozinha se aparece
    const bool hidden = m_navHidden || m_autoCollapsed;
    // Flutuando, a navegação é da gaveta/lista — não mexe na visibilidade.
    if (m_navPane && m_floatKind == FloatKind::None) {
        bool show = false;
        switch (m_layout) {
        case Layout::Classic: show = !m_navHidden; break;     // o preview ocupa tudo
        case Layout::Columns:
        case Layout::Rail:
        case Layout::Miller:
        case Layout::Book:    show = !hidden; break;
        case Layout::Binder:  show = false; break;
        case Layout::Overlay:
        case Layout::Reader:
        case Layout::Dock:    show = false; break;
        case Layout::Gallery: show = true; break;     // o mural é a navegação
        }
        m_navPane->setVisible(show);
        // Na Galeria o documento só existe subindo por cima do mural.
        if (m_previewPane) m_previewPane->setVisible(m_layout != Layout::Gallery);
        if (show) applySplitSizes();
    }
    if (m_edgeBtn) m_edgeBtn->setVisible((m_layout == Layout::Columns || m_layout == Layout::Book) && hidden);
    if (m_millerCol) m_millerCol->setVisible(m_layout == Layout::Miller && !hidden);
    rebuildRail();
    if (navPaneShown()) rebuildNavBody();
}

void RefMenuPanel::onTogglePin()
{
    m_pinned = m_pinBtn->isChecked();
    QSettings().setValue(QString::fromLatin1(kKeyPinned), m_pinned);
}

void RefMenuPanel::onToggleSearch()
{
    if (!m_searchRow) return;
    // A busca mora dentro da navegação nos estilos deitados. Com ela
    // escondida (coluna recolhida, gaveta fechada), buscar abre a navegação
    // flutuante já com o campo em foco.
    if (m_layout != Layout::Classic && m_layout != Layout::Reader && !navPaneShown()) {
        if (m_layout == Layout::Gallery) hideFloat();   // a busca mora no mural
        else showFloat(floatKindForLayout(), QString());
        m_searchRow->setVisible(true);
        if (m_searchBtn) m_searchBtn->setChecked(true);
        if (m_searchInput) {
            m_searchInput->setFocus();
            m_searchInput->selectAll();
        }
        return;
    }
    const bool willShow = !m_searchRow->isVisible();
    m_searchRow->setVisible(willShow);
    if (m_searchBtn) m_searchBtn->setChecked(willShow);
    if (willShow) {
        if (m_searchInput) {
            m_searchInput->setFocus();
            m_searchInput->selectAll();
        }
    } else {
        if (m_searchInput) m_searchInput->clear();
        // textChanged limpa m_searchQuery via onSearchQueryChanged
    }
}

void RefMenuPanel::onSearchQueryChanged(const QString& q)
{
    m_searchQuery = q.trimmed();
    // Leitor: os resultados descem do campo, por cima do documento.
    if (m_layout == Layout::Reader && !m_searchQuery.isEmpty()
        && m_floatKind != FloatKind::Dropdown) {
        showFloat(FloatKind::Dropdown, QString());   // já reconstrói a navegação
        return;
    }
    rebuildNavBody();
}

void RefMenuPanel::openSearch()
{
    if (!isVisible()) openPanel();
    if (m_searchRow && !m_searchRow->isVisible()) onToggleSearch();
    else if (m_searchInput) {
        m_searchInput->setFocus();
        m_searchInput->selectAll();
    }
}

void RefMenuPanel::closeSearch()
{
    if (m_searchRow && m_searchRow->isVisible()) onToggleSearch();
}

void RefMenuPanel::openPreviewFind()
{
    if (!isVisible()) openPanel();
    if (!m_previewFind) return;
    positionPreviewFindBar();
    m_previewFind->openBar();
    m_previewFind->raise();
}

void RefMenuPanel::positionPreviewFindBar()
{
    if (!m_previewFind || !m_previewWrap) return;
    m_previewFind->adjustSize();
    const int margin = 8;
    const int w = qMax(280, m_previewFind->sizeHint().width());
    const int h = m_previewFind->height();
    m_previewFind->resize(w, h);
    // Coordenadas do canto sup. direito do previewWrap relativas ao painel.
    const QPoint topRight = m_previewWrap->mapTo(this, QPoint(m_previewWrap->width(), 0));
    int x = topRight.x() - w - margin;
    int y = topRight.y() + margin;
    x = qMax(margin, x);
    m_previewFind->move(x, y);
}

bool RefMenuPanel::matchesSearch(const QString& text) const
{
    if (m_searchQuery.isEmpty()) return true;
    return text.contains(m_searchQuery, Qt::CaseInsensitive);
}


void RefMenuPanel::onCycleFontSize()
{
    // 11 → 13 → 15 → 17 → 11
    int next = m_previewFontPt + 2;
    if (next > 17) next = 11;
    m_previewFontPt = next;
    applyPreviewFont();
    QSettings().setValue(QString::fromLatin1(kKeyFontPt), m_previewFontPt);
}

void RefMenuPanel::onToggleVisualMode()
{
    m_visualMode = !m_visualMode;
    rebuildCrumbs();
    rebuildNavBody();
    QSettings().setValue(QString::fromLatin1(kKeyVisualMode), m_visualMode);
}

// =========================================================================
// Helpers (drawer/visual heuristic)
// =========================================================================

bool RefMenuPanel::drawerIsVisual(const Drawer* d) const
{
    if (!d) return false;
    const QString t = d->drawerElementType.toLower();
    if (t.isEmpty()) return false;
    QRegularExpression re(QStringLiteral("personagem|character|cen[aá]rios?|setting|objeto|object"),
                          QRegularExpression::CaseInsensitiveOption);
    return re.match(t).hasMatch();
}

QString RefMenuPanel::roleOrLabelForItem(const DrawerItem& it) const
{
    if (!it.role.isEmpty()) return RoleTiers::roleDisplayName(it.role);
    if (m_elements && !it.elementId.isEmpty()) {
        const Element* el = m_elements->findElement(it.elementId);
        if (el && !el->role.isEmpty()) return RoleTiers::roleDisplayName(el->role);
    }
    return QString();
}

QString RefMenuPanel::imageForItem(const DrawerItem& it) const
{
    if (!m_elements) return QString();
    if (it.elementId.isEmpty()) return QString();
    const Element* el = m_elements->findElement(it.elementId);
    if (!el) return QString();
    return el->image;
}

// =========================================================================
// Drag livre via ⠿ + Resize livre via handles dedicados (estilo Drawer)
// =========================================================================

void RefMenuPanel::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);
    emit geometryChanged();
    scheduleGeometrySave();
}

void RefMenuPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutResizeHandles();
    if (m_previewFind && m_previewFind->isVisible()) positionPreviewFindBar();
    QTimer::singleShot(0, this, [this]() { rescalePreviewImages(); layoutFloat(); });
    if (m_layout == Layout::Gallery && m_galleryRelayout) m_galleryRelayout->start();
    // Coluna que não cabe recolhe sozinha (sem mexer na preferência salva) e
    // volta quando o painel alarga. Folga de 24 px pra não piscar na borda.
    if (m_layout == Layout::Columns || m_layout == Layout::Rail || m_layout == Layout::Miller
        || m_layout == Layout::Book) {
        const int need = (m_layout == Layout::Miller) ? 620 : 500;
        const bool narrow = m_autoCollapsed ? width() < need + 24 : width() < need;
        if (narrow != m_autoCollapsed) {
            m_autoCollapsed = narrow;
            if (!narrow) hideFloat();
            applyNavVisibility();
        }
    }
    emit geometryChanged();
    scheduleGeometrySave();
}

void RefMenuPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    clampToScreen();
    // Só agora o splitter sabe a própria largura de verdade.
    QTimer::singleShot(0, this, [this]() {
        if (m_navPane && m_navPane->isVisible()) applySplitSizes();
        if (m_layout == Layout::Gallery) rebuildNavBody();   // mural na largura real
    });
}

void RefMenuPanel::clampToScreen()
{
    // Garante que se a geometria salva ficou fora de qualquer tela (monitor
    // desconectado, etc.), recolocamos dentro da tela mais próxima.
    if (const QScreen* scr = screen()) {
        QRect sg = scr->availableGeometry();
        QRect g = geometry();
        if (g.width() > sg.width() - 8) g.setWidth(sg.width() - 8);
        if (g.right() > sg.right()) g.moveRight(sg.right() - 4);
        if (g.bottom() > sg.bottom()) g.moveBottom(sg.bottom() - 4);
        if (g.left() < sg.left() + 4) g.moveLeft(sg.left() + 4);
        if (g.top() < sg.top() + 4) g.moveTop(sg.top() + 4);
        if (g != geometry()) setGeometry(g);
    }
}

// Event filter: handles de resize (8 widgets) + drag handle ⠿.
bool RefMenuPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Véu da navegação flutuante: clicar fora fecha.
    if (watched == m_scrim && event->type() == QEvent::MouseButtonPress) {
        hideFloat();
        return true;
    }
    // Faixa do Comparar: clicar na da página lateral faz dela a viva.
    if (event->type() == QEvent::MouseButtonPress && watched == m_side.bar) {
        activateSidePane();
        return true;
    }
    // Abas: clique do meio fecha.
    if (watched == m_tabBar && event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::MiddleButton) {
            const int i = m_tabBar->tabAt(me->position().toPoint());
            if (i >= 0) closeTab(i);
            return true;
        }
    }
    // Linha de documento: botão direito oferece aba nova e Comparar.
    if (event->type() == QEvent::ContextMenu) {
        if (auto* w = qobject_cast<QWidget*>(watched)) {
            const QString key = w->property("refRowKey").toString();
            if (!key.isEmpty()) {
                QMenu menu(this);
                menu.addAction(tr("Abrir em nova aba"), this, [this, key]() {
                    openInNewTab(key);
                    addRecent(key);
                });
                menu.addAction(tr("Abrir ao lado (comparar)"), this, [this, key]() { openBeside(key); });
                menu.exec(static_cast<QContextMenuEvent*>(event)->globalPos());
                return true;
            }
        }
    }
    if (watched == m_searchInput && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape
        && m_floatKind != FloatKind::None) {
        hideFloat();
        return true;
    }
    // Linhas da tela inicial: abrir documento, ou entrar numa fonte.
    // Ficam marcadas por propriedade em vez de widget dedicado, pra que uma
    // linha nova em qualquer seção funcione sem fiação extra.
    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto* w = qobject_cast<QWidget*>(watched)) {
            const QVariant rowKey = w->property("refRowKey");
            if (rowKey.isValid() && !rowKey.toString().isEmpty()) {
                const QString key = rowKey.toString();
                // Ctrl+clique ou clique do meio: aba nova, como no navegador.
                auto* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::MiddleButton || (me->modifiers() & Qt::ControlModifier)) {
                    openInNewTab(key);
                    addRecent(key);
                    return true;
                }
                // NAO troca de modo: abrir um documento mantem a arvore na
                // tela, com os galhos como estavam. Trocar pra Section aqui era
                // o que fazia a navegacao cair na view antiga, de caixa bordada.
                setSelected(key);
                addRecent(key);
                rebuildCrumbs();
                return true;
            }
            // Galho da árvore: abre/fecha ali mesmo, sem trocar de tela.
            const QVariant expandVar = w->property("refExpandKey");
            if (expandVar.isValid() && !expandVar.toString().isEmpty()) {
                const QString ek = expandVar.toString();
                if (m_expanded.contains(ek)) m_expanded.remove(ek);
                else m_expanded.insert(ek);
                rebuildNavBody();
                return true;
            }
            // Coluna de fontes do Miller: troca o que a coluna do meio mostra.
            const QVariant millerVar = w->property("refMillerKey");
            if (millerVar.isValid() && !millerVar.toString().isEmpty()) {
                m_millerSource = millerVar.toString();
                m_navMode = NavMode::Home;
                rebuildMillerSources();
                rebuildNavBody();
                return true;
            }
            const QVariant kindVar = w->property("refSectionKind");
            if (kindVar.isValid()) {
                enterSection(static_cast<SourceKind>(kindVar.toInt()),
                             w->property("refSectionKey").toString());
                return true;
            }
        }
    }

    // Mapa watched -> ResizeEdge.
    auto edgeOf = [this](QObject* o) -> ResizeEdge {
        if (o == m_hL)  return ResizeEdge::Left;
        if (o == m_hR)  return ResizeEdge::Right;
        if (o == m_hT)  return ResizeEdge::Top;
        if (o == m_hB)  return ResizeEdge::Bottom;
        if (o == m_hTL) return ResizeEdge::TL;
        if (o == m_hTR) return ResizeEdge::TR;
        if (o == m_hBL) return ResizeEdge::BL;
        if (o == m_hBR) return ResizeEdge::BR;
        return ResizeEdge::None;
    };
    ResizeEdge edge = edgeOf(watched);
    if (edge != ResizeEdge::None) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_resizing = true;
                m_activeEdge = edge;
                m_resizeStartGeom = geometry();
                m_resizeStartMouse = me->globalPosition().toPoint();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_resizing) {
                auto* me = static_cast<QMouseEvent*>(event);
                const QPoint g = me->globalPosition().toPoint();
                const int dx = g.x() - m_resizeStartMouse.x();
                const int dy = g.y() - m_resizeStartMouse.y();
                int newX = m_resizeStartGeom.x();
                int newY = m_resizeStartGeom.y();
                int newW = m_resizeStartGeom.width();
                int newH = m_resizeStartGeom.height();

                switch (m_activeEdge) {
                    case ResizeEdge::Left:
                        newW = m_resizeStartGeom.width() - dx;
                        newX = m_resizeStartGeom.x() + dx;
                        break;
                    case ResizeEdge::Right:
                        newW = m_resizeStartGeom.width() + dx;
                        break;
                    case ResizeEdge::Top:
                        newH = m_resizeStartGeom.height() - dy;
                        newY = m_resizeStartGeom.y() + dy;
                        break;
                    case ResizeEdge::Bottom:
                        newH = m_resizeStartGeom.height() + dy;
                        break;
                    case ResizeEdge::TL:
                        newW = m_resizeStartGeom.width() - dx;
                        newH = m_resizeStartGeom.height() - dy;
                        newX = m_resizeStartGeom.x() + dx;
                        newY = m_resizeStartGeom.y() + dy;
                        break;
                    case ResizeEdge::TR:
                        newW = m_resizeStartGeom.width() + dx;
                        newH = m_resizeStartGeom.height() - dy;
                        newY = m_resizeStartGeom.y() + dy;
                        break;
                    case ResizeEdge::BL:
                        newW = m_resizeStartGeom.width() - dx;
                        newH = m_resizeStartGeom.height() + dy;
                        newX = m_resizeStartGeom.x() + dx;
                        break;
                    case ResizeEdge::BR:
                        newW = m_resizeStartGeom.width() + dx;
                        newH = m_resizeStartGeom.height() + dy;
                        break;
                    default: break;
                }
                // Clamp pelos mínimos sem deixar a borda fixa pular.
                if (newW < kMinW) {
                    if (m_activeEdge == ResizeEdge::Left || m_activeEdge == ResizeEdge::TL || m_activeEdge == ResizeEdge::BL) {
                        newX = m_resizeStartGeom.right() - kMinW + 1;
                    }
                    newW = kMinW;
                }
                if (newH < kMinH) {
                    if (m_activeEdge == ResizeEdge::Top || m_activeEdge == ResizeEdge::TL || m_activeEdge == ResizeEdge::TR) {
                        newY = m_resizeStartGeom.bottom() - kMinH + 1;
                    }
                    newH = kMinH;
                }
                setGeometry(newX, newY, newW, newH);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_resizing) {
                m_resizing = false;
                m_activeEdge = ResizeEdge::None;
                scheduleGeometrySave();
                return true;
            }
        }
    }

    if (watched == m_dragHandle) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragOffset = me->globalPosition().toPoint() - pos();
                m_dragHandle->setCursor(Qt::ClosedHandCursor);
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_dragging) {
                auto* me = static_cast<QMouseEvent*>(event);
                const QPoint newPos = me->globalPosition().toPoint() - m_dragOffset;
                move(newPos);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_dragging) {
                m_dragging = false;
                m_dragHandle->setCursor(Qt::OpenHandCursor);
                scheduleGeometrySave();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            // duplo clique no drag handle → reseta posição/tamanho ao canto
            // direito da janela principal do app.
            if (auto* p = parentWidget()) {
                const QRect pg = p->window()->geometry();
                const int margin = 12;
                resize(defaultWidthFor(m_layout), qMax(kMinH, pg.height() - margin * 2));
                move(pg.right() - width() - margin, pg.top() + margin);
                scheduleGeometrySave();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// =========================================================================
// Persistência QSettings
// =========================================================================

void RefMenuPanel::loadGeometryFromSettings()
{
    QSettings s;
    // Sem escolha salva, o Trilho é o padrão (decisão do usuário, 2026-09-24).
    m_layout = layoutFromId(s.value(QString::fromLatin1(kKeyLayout)).toString());
    m_navRight = s.value(QString::fromLatin1(kKeyNavRight), false).toBool();
    m_navColW = qBound(160, s.value(QString::fromLatin1(kKeyNavColW), 250).toInt(), 600);
    m_bookColW = qBound(200, s.value(QStringLiteral("ui/refMenuPanel/bookColW"), 320).toInt(), 700);
    m_mapOn = s.value(QStringLiteral("ui/refMenuPanel/usageMap"), false).toBool();
    if (m_mapBtn) {
        QSignalBlocker b(m_mapBtn);
        m_mapBtn->setChecked(m_mapOn);
    }
    // O divisor do Clássico é restaurado em applyLayout(), que sabe a orientação.
    QByteArray ba = s.value(geometryKeyFor(m_layout)).toByteArray();
    if (!ba.isEmpty()) {
        restoreGeometry(ba);
    } else if (m_layout != Layout::Classic) {
        // Estilo sem geometria própria: parte da do Clássico, na largura do estilo.
        ba = s.value(QString::fromLatin1(kKeyGeom)).toByteArray();
        if (!ba.isEmpty()) restoreGeometry(ba);
        QRect g = geometry();
        g.setLeft(g.right() - defaultWidthFor(m_layout) + 1);
        setGeometry(g);
    }
    m_navHidden = s.value(navHiddenKeyFor(m_layout), false).toBool();
    m_visualMode = s.value(QString::fromLatin1(kKeyVisualMode), true).toBool();
    m_pinned = s.value(QString::fromLatin1(kKeyPinned), false).toBool();
    m_previewFontPt = qBound(9, s.value(QString::fromLatin1(kKeyFontPt), 13).toInt(), 24);

    if (m_pinBtn) {
        QSignalBlocker b(m_pinBtn);
        m_pinBtn->setChecked(m_pinned);
    }
}

void RefMenuPanel::saveGeometryToSettings()
{
    QSettings s;
    s.setValue(geometryKeyFor(m_layout), saveGeometry());
    // A divisao entre arvore e texto e' preferencia: sobrevive ao fechar. O
    // estado do splitter guarda a orientacao, entao so o Classico o usa; os
    // estilos deitados guardam so a largura da coluna.
    if (m_bodySplit && m_layout == Layout::Classic && m_navPane
        && m_navPane->parentWidget() == m_bodySplit && m_navPane->isVisible())
        s.setValue(QString::fromLatin1(kKeySplit), m_bodySplit->saveState());
    s.setValue(QString::fromLatin1(kKeyNavColW), m_navColW);
    s.setValue(QStringLiteral("ui/refMenuPanel/bookColW"), m_bookColW);
}

void RefMenuPanel::scheduleGeometrySave()
{
    if (m_savePending) return;
    m_savePending = true;
    QTimer::singleShot(400, this, [this]() {
        m_savePending = false;
        saveGeometryToSettings();
    });
}

// =========================================================================
// Estilos de layout
// =========================================================================

namespace {

// Miniatura redonda (trilho, fileira do Leitor): foto se houver, senão o
// círculo com inicial do AvatarUtils. `px` em pixels lógicos, desenhada no
// DPR da tela pra não borrar.
QPixmap refRoundThumb(const QString& src, const QString& name, const QString& id,
                      int px, qreal dpr)
{
    const int s = qMax(1, int(px * dpr));
    QPixmap out;
    const QPixmap photo = refThumbFrom(src);
    if (photo.isNull()) {
        out = AvatarUtils::circularAvatar(QString(), name, id.isEmpty() ? name : id, s);
    } else {
        out = QPixmap(s, s);
        out.fill(Qt::transparent);
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath clip;
        clip.addEllipse(0, 0, s, s);
        p.setClipPath(clip);
        const QPixmap sc = photo.scaled(s, s, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        p.drawPixmap((s - sc.width()) / 2, (s - sc.height()) / 2, sc);
    }
    out.setDevicePixelRatio(dpr);
    return out;
}

// Capa do manuscrito no formato do trilho/coluna de fontes (retrato 14×19).
QPixmap refCoverThumb(const QString& src, qreal dpr)
{
    const QPixmap pm = refThumbFrom(src);
    if (pm.isNull()) return QPixmap();
    QPixmap t = pm.scaled(QSize(14, 19) * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    t.setDevicePixelRatio(dpr);
    return t;
}

QIcon refMutedIcon(const QString& path, int px)
{
    return IconUtils::loadToolbarIcon(path, QColor(Theme::textMuted()),
        QColor(Theme::textBright()), QColor(Theme::textBright()), QSize(px, px));
}

// Esvazia um layout sem destruir no meio de um evento: quem chama pode ser
// um botão lá de dentro (o trilho se refaz a cada clique).
void refClearLayout(QLayout* lay)
{
    if (!lay) return;
    while (lay->count() > 0) {
        QLayoutItem* it = lay->takeAt(0);
        if (QWidget* w = it->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete it;
    }
}

} // namespace

QString RefMenuPanel::layoutId(Layout l)
{
    switch (l) {
    case Layout::Classic: return QStringLiteral("classic");
    case Layout::Columns: return QStringLiteral("columns");
    case Layout::Rail:    return QStringLiteral("rail");
    case Layout::Overlay: return QStringLiteral("overlay");
    case Layout::Miller:  return QStringLiteral("miller");
    case Layout::Reader:  return QStringLiteral("reader");
    case Layout::Dock:    return QStringLiteral("dock");
    case Layout::Gallery: return QStringLiteral("gallery");
    case Layout::Book:    return QStringLiteral("book");
    case Layout::Binder:  return QStringLiteral("binder");
    }
    return QStringLiteral("classic");
}

RefMenuPanel::Layout RefMenuPanel::layoutFromId(const QString& id)
{
    for (Layout l : { Layout::Classic, Layout::Columns, Layout::Rail, Layout::Overlay,
                      Layout::Miller, Layout::Reader, Layout::Dock, Layout::Gallery,
                      Layout::Book, Layout::Binder }) {
        if (layoutId(l) == id) return l;
    }
    return Layout::Rail;   // padrão do RefMenu
}

int RefMenuPanel::defaultWidthFor(Layout l)
{
    switch (l) {
    case Layout::Classic: return kDefaultW;
    case Layout::Columns: return 680;
    case Layout::Rail:    return 640;
    case Layout::Overlay: return 440;
    case Layout::Miller:  return 860;
    case Layout::Reader:  return 460;
    case Layout::Dock:    return 440;
    case Layout::Gallery: return 660;
    case Layout::Book:    return 760;
    case Layout::Binder:  return 480;
    }
    return kDefaultW;
}

QString RefMenuPanel::geometryKeyFor(Layout l) const
{
    // O Clássico continua na chave antiga: quem atualiza não perde a posição.
    if (l == Layout::Classic) return QString::fromLatin1(kKeyGeom);
    return QStringLiteral("ui/refMenuPanel/geometry-%1").arg(layoutId(l));
}

QString RefMenuPanel::navHiddenKeyFor(Layout l) const
{
    if (l == Layout::Classic) return QString::fromLatin1(kKeyNavHidden);
    return QStringLiteral("ui/refMenuPanel/navHidden-%1").arg(layoutId(l));
}

int RefMenuPanel::navSplitIndex() const
{
    const bool mirror = m_navRight && (m_layout == Layout::Columns || m_layout == Layout::Rail);
    return mirror ? 1 : 0;
}

bool RefMenuPanel::navPaneShown() const
{
    return m_navPane && m_navPane->isVisibleTo(const_cast<RefMenuPanel*>(this));
}

QString RefMenuPanel::homeFilter() const
{
    if (m_floatKind != FloatKind::None) return m_floatFilter;
    if (m_layout == Layout::Miller) return m_millerSource;
    return QString();
}

RefMenuPanel::FloatKind RefMenuPanel::floatKindForLayout() const
{
    switch (m_layout) {
    case Layout::Rail:   return FloatKind::Flyout;
    case Layout::Reader: return FloatKind::Dropdown;
    case Layout::Dock:   return FloatKind::Sheet;
    case Layout::Binder: return FloatKind::Divider;
    default:             return FloatKind::Drawer;
    }
}

void RefMenuPanel::setLayoutStyle(Layout l)
{
    if (l == m_layout) return;
    hideFloat();
    saveGeometryToSettings();           // guarda a geometria do estilo que sai
    m_layout = l;
    QSettings s;
    s.setValue(QString::fromLatin1(kKeyLayout), layoutId(l));
    m_navHidden = s.value(navHiddenKeyFor(l), false).toBool();
    m_autoCollapsed = false;

    // Cada estilo lembra a própria geometria: o Clássico vive bem com 380 px
    // e as Três colunas pedem mais de 800. Sem geometria salva, o painel
    // assume a largura padrão do estilo mantendo a borda direita (onde ele
    // costuma morar) e a altura.
    const QByteArray ba = s.value(geometryKeyFor(l)).toByteArray();
    if (ba.isEmpty() || !restoreGeometry(ba)) {
        QRect g = geometry();
        g.setLeft(g.right() - defaultWidthFor(l) + 1);
        setGeometry(g);
    }
    clampToScreen();
    applyLayout();
    nudgeClearOfChrome();
    scheduleGeometrySave();
}

void RefMenuPanel::applyLayout()
{
    if (!m_body || !m_bodySplit || !m_navPane || !m_previewPane) return;
    hideFloat();

    const bool classic = (m_layout == Layout::Classic);
    const bool reader  = (m_layout == Layout::Reader);
    const bool dock    = (m_layout == Layout::Dock);
    const bool gallery = (m_layout == Layout::Gallery);
    const bool book    = (m_layout == Layout::Book);
    const bool binder  = (m_layout == Layout::Binder);
    const bool mirror  = m_navRight && (m_layout == Layout::Columns || m_layout == Layout::Rail);

    // ---- Busca e trilha: onde moram ----
    auto detach = [](QWidget* w) {
        if (!w) return;
        if (QWidget* p = w->parentWidget())
            if (QLayout* l = p->layout()) l->removeWidget(w);
    };
    detach(m_searchRow);
    detach(m_crumbsRow);
    detach(m_readerBar);
    detach(m_rail);
    detach(m_mapPanel);
    if (classic) {
        // Como sempre foi: busca e trilha atravessam o painel, acima da árvore.
        m_frameLay->insertWidget(1, m_searchRow);
        m_frameLay->insertWidget(2, m_crumbsRow);
    } else if (reader) {
        m_readerBarLay->insertWidget(0, m_searchRow);
        m_frameLay->insertWidget(1, m_readerBar);
        m_previewPaneLay->insertWidget(0, m_crumbsRow);
    } else {
        // A busca filtra a navegação, então fica nela; a trilha descreve o
        // documento aberto, então fica em cima dele.
        m_navPaneLay->insertWidget(0, m_searchRow);
        m_previewPaneLay->insertWidget(0, m_crumbsRow);
    }
    m_readerBar->setVisible(reader);
    m_galleryBar->setVisible(gallery);
    // Mapa de Uso: atravessa o painel, logo abaixo das barras de cima.
    m_frameLay->insertWidget(classic ? 3 : (reader ? 2 : 1), m_mapPanel);
    m_mapPanel->setVisible(m_mapOn);

    // ---- Splitter ----
    m_bodySplit->setOrientation(classic ? Qt::Vertical : Qt::Horizontal);
    const QString splitName = classic ? QStringLiteral("refBodySplit") : QStringLiteral("refBodySplitH");
    if (m_bodySplit->objectName() != splitName) {
        m_bodySplit->setObjectName(splitName);
        applyMainStyleSheet();   // a regra da alça é por nome
    }
    if (mirror) {
        m_bodySplit->insertWidget(0, m_previewPane);
        m_bodySplit->insertWidget(1, m_navPane);
    } else {
        m_bodySplit->insertWidget(0, m_navPane);
        m_bodySplit->insertWidget(1, m_previewPane);
    }
    m_bodySplit->setStretchFactor(navSplitIndex(), 0);
    m_bodySplit->setStretchFactor(1 - navSplitIndex(), 1);

    // ---- Corpo: trilho / lingueta / fontes em volta do splitter ----
    while (m_bodyLay->count() > 0) delete m_bodyLay->takeAt(0);   // só os itens
    // Doca: o mesmo trilho, deitado no pé do painel (fora do corpo).
    if (dock) m_frameLay->addWidget(m_rail);
    m_railLay->setDirection(dock ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    m_railLay->setContentsMargins(dock ? 8 : 0, dock ? 0 : 8, dock ? 8 : 0, dock ? 0 : 8);
    m_railLay->setSpacing(dock ? 4 : 3);
    if (dock) {
        m_rail->setMinimumWidth(0);
        m_rail->setMaximumWidth(QWIDGETSIZE_MAX);
        m_rail->setFixedHeight(50);
    } else {
        m_rail->setMinimumHeight(0);
        m_rail->setMaximumHeight(QWIDGETSIZE_MAX);
        m_rail->setFixedWidth(46);
    }
    if (binder)                                 m_bodyLay->addWidget(m_rings);
    if (m_layout == Layout::Rail && !mirror)    m_bodyLay->addWidget(m_rail);
    if ((m_layout == Layout::Columns && !mirror) || book) m_bodyLay->addWidget(m_edgeBtn);
    if (m_layout == Layout::Miller)             m_bodyLay->addWidget(m_millerCol);
    m_bodyLay->addWidget(m_bodySplit, 1);
    if (m_layout == Layout::Rail && mirror)     m_bodyLay->addWidget(m_rail);
    if (m_layout == Layout::Columns && mirror)  m_bodyLay->addWidget(m_edgeBtn);
    if (binder)                                 m_bodyLay->addWidget(m_dividers);
    m_rail->setVisible(m_layout == Layout::Rail || dock);
    m_rings->setVisible(binder);
    m_dividers->setVisible(binder);
    if (m_layout != Layout::Columns && !book) m_edgeBtn->setVisible(false);
    if (m_layout != Layout::Miller) m_millerCol->setVisible(false);
    m_edgeBtn->setText(mirror ? QString::fromUtf8("‹") : QString::fromUtf8("›"));

    auto repolish = [](QWidget* w, const char* prop, bool value) {
        w->setProperty(prop, value);
        w->style()->unpolish(w);
        w->style()->polish(w);
    };
    repolish(m_rail, "right", mirror);
    repolish(m_rail, "dock", dock);
    repolish(m_edgeBtn, "right", mirror);
    repolish(m_previewWrap, "classic", classic);
    repolish(m_navPane, "book", book);
    repolish(m_previewPane, "book", book);
    // Livro aberto: título e papel centralizados, como numa página impressa.
    m_previewTitle->setAlignment(book ? Qt::AlignHCenter : Qt::AlignLeft);
    m_previewRole->setAlignment(book ? Qt::AlignHCenter : Qt::AlignLeft);
    applyPreviewFont();

    if (m_layout == Layout::Miller || gallery) m_navMode = NavMode::Home;

    // ---- Divisor ----
    if (classic) {
        const QByteArray st = QSettings().value(QString::fromLatin1(kKeySplit)).toByteArray();
        if (st.isEmpty() || !m_bodySplit->restoreState(st)) m_bodySplit->setSizes({ 260, 460 });
        m_bodySplit->setOrientation(Qt::Vertical);   // estado antigo nunca deita o Clássico
    }

    // ---- Texto do preview vazio ----
    if (m_previewPlaceholder) {
        switch (m_layout) {
        case Layout::Classic:
            m_previewPlaceholder->setText(tr("Selecione um documento acima pra visualizar aqui."));
            break;
        case Layout::Columns:
        case Layout::Rail:
        case Layout::Miller:
            m_previewPlaceholder->setText(tr("Selecione um documento ao lado pra visualizar aqui."));
            break;
        case Layout::Overlay:
            m_previewPlaceholder->setText(tr("Abra a navegação pelo botão do cabeçalho pra escolher um documento."));
            break;
        case Layout::Reader:
            m_previewPlaceholder->setText(tr("Busque um documento pelo nome no campo acima."));
            break;
        case Layout::Dock:
            m_previewPlaceholder->setText(tr("Escolha um documento pela barra de baixo."));
            break;
        case Layout::Gallery:
            m_previewPlaceholder->setText(tr("Escolha um cartão no mural."));
            break;
        case Layout::Book:
            m_previewPlaceholder->setText(tr("Escolha um capítulo ou apêndice no sumário."));
            break;
        case Layout::Binder:
            m_previewPlaceholder->setText(tr("Puxe uma divisória pra escolher um documento."));
            break;
        }
    }

    applyNavVisibility();
    refresh();
}

void RefMenuPanel::applySplitSizes()
{
    if (!m_bodySplit || m_bodySplit->orientation() != Qt::Horizontal) return;
    if (!m_navPane || m_navPane->parentWidget() != m_bodySplit) return;
    int total = m_bodySplit->width();
    if (total < 200) {
        // Antes de mostrar, o splitter ainda não tem largura de verdade.
        total = width();
        if (m_layout == Layout::Rail) total -= 46;
        if (m_layout == Layout::Miller) total -= 180;
    }
    const int want = (m_layout == Layout::Book) ? m_bookColW : m_navColW;
    const int nav = qBound(160, want, qMax(160, total - 220));
    QList<int> sz = { 0, 0 };
    sz[navSplitIndex()] = nav;
    sz[1 - navSplitIndex()] = qMax(1, total - nav);
    m_bodySplit->setSizes(sz);
}

void RefMenuPanel::updateToggleNavButton()
{
    if (!m_toggleNavBtn) return;
    const bool hidden = m_navHidden || m_autoCollapsed;
    // No Trilho quem abre e fecha a coluna é o próprio trilho.
    m_toggleNavBtn->setVisible(m_layout != Layout::Rail && m_layout != Layout::Gallery);
    switch (m_layout) {
    case Layout::Classic:
        m_toggleNavBtn->setIcon(QIcon());
        m_toggleNavBtn->setText(m_navHidden ? QStringLiteral("▸") : QStringLiteral("▾"));
        m_toggleNavBtn->setToolTip(m_navHidden ? tr("Mostrar explorador") : tr("Ocultar explorador"));
        break;
    case Layout::Columns:
    case Layout::Miller:
    case Layout::Book:
        m_toggleNavBtn->setText(QString());
        m_toggleNavBtn->setIcon(refMutedIcon(
            (m_navRight && m_layout == Layout::Columns) ? QStringLiteral(":/icons/panel-right.svg")
                                                        : QStringLiteral(":/icons/panel-left.svg"), 14));
        m_toggleNavBtn->setToolTip(hidden ? tr("Mostrar navegação") : tr("Recolher navegação"));
        break;
    case Layout::Overlay:
    case Layout::Reader:
    case Layout::Dock:
    case Layout::Binder:
        m_toggleNavBtn->setText(QString());
        m_toggleNavBtn->setIcon(refMutedIcon(QStringLiteral(":/icons/menu.svg"), 14));
        m_toggleNavBtn->setToolTip(tr("Navegação"));
        break;
    case Layout::Rail:
    case Layout::Gallery:
        break;
    }
}

void RefMenuPanel::rebuildLayoutMenu()
{
    if (!m_layoutMenu) return;
    m_layoutMenu->clear();
    m_layoutMenu->setToolTipsVisible(true);
    auto* group = new QActionGroup(m_layoutMenu);
    group->setExclusive(true);
    struct Opt { Layout l; QString name; QString tip; };
    const QList<Opt> opts = {
        { Layout::Classic, tr("Clássico"),        tr("Navegação em cima, documento embaixo") },
        { Layout::Columns, tr("Duas colunas"),    tr("Navegação numa coluna ao lado do documento") },
        { Layout::Rail,    tr("Trilho"),          tr("Coluna que recolhe pra um trilho de ícones") },
        { Layout::Dock,    tr("Doca"),            tr("Os ícones do Trilho numa barra embaixo; as listas sobem") },
        { Layout::Overlay, tr("Gaveta por cima"), tr("Documento cheio; a navegação desliza por cima") },
        { Layout::Miller,  tr("Três colunas"),    tr("Fontes, itens e documento lado a lado") },
        { Layout::Gallery, tr("Galeria"),         tr("O projeto como mural de cartões com foto") },
        { Layout::Book,    tr("Livro aberto"),    tr("Sumário na página da esquerda, documento na da direita") },
        { Layout::Binder,  tr("Fichário"),        tr("As gavetas viram divisórias coloridas na borda") },
        { Layout::Reader,  tr("Leitor"),          tr("Quase só o documento; navegue buscando pelo nome") },
    };
    for (const Opt& o : opts) {
        QAction* a = m_layoutMenu->addAction(o.name);
        a->setCheckable(true);
        a->setChecked(o.l == m_layout);
        a->setToolTip(o.tip);
        group->addAction(a);
        const Layout l = o.l;
        connect(a, &QAction::triggered, this, [this, l]() { setLayoutStyle(l); });
    }
    m_layoutMenu->addSeparator();
    QAction* right = m_layoutMenu->addAction(tr("Navegação à direita"));
    right->setCheckable(true);
    right->setChecked(m_navRight);
    right->setEnabled(m_layout == Layout::Columns || m_layout == Layout::Rail);
    connect(right, &QAction::toggled, this, [this](bool on) {
        m_navRight = on;
        QSettings().setValue(QString::fromLatin1(kKeyNavRight), on);
        applyLayout();
    });
}

// ---- Navegação flutuante ----

QWidget* RefMenuPanel::floatingWidget() const
{
    return m_floatKind == FloatKind::DocSheet ? m_previewPane : m_navPane;
}

void RefMenuPanel::showFloat(FloatKind kind, const QString& filter)
{
    if (!m_navPane || !m_floatHost || kind == FloatKind::None) return;
    // Trocar de tipo (ex.: lista → documento) devolve o que estava flutuando.
    if (m_floatKind != FloatKind::None && m_floatKind != kind) hideFloat();
    m_floatKind = kind;
    m_floatFilter = filter;
    if (!filter.isEmpty()) m_navMode = NavMode::Home;   // lista de uma seção = tela inicial filtrada
    QWidget* w = floatingWidget();
    if (w->parentWidget() != m_floatHost) m_floatLay->addWidget(w);
    w->setVisible(true);
    // O dropdown do Leitor só precisa separar do texto; a gaveta e a lista do
    // trilho escurecem mais, porque cobrem o documento de verdade.
    m_scrim->setStyleSheet(QStringLiteral("QWidget#refScrim { background: rgba(0,0,0,%1); }")
        .arg(kind == FloatKind::Dropdown ? QStringLiteral("0.10") : QStringLiteral("0.30")));
    // Fichário: a lista entra com a faixa da cor da divisória.
    if (m_floatBand) {
        m_floatBand->setVisible(kind == FloatKind::Divider);
        if (kind == FloatKind::Divider && m_dividers) {
            QColor c = Theme::toColor(Theme::textMuted());
            for (const auto& t : m_dividers->tabs) if (t.filter == filter) c = t.color;
            m_floatBand->setStyleSheet(QStringLiteral("background:%1;").arg(c.name()));
        }
    }
    m_scrim->show();
    m_floatHost->show();
    layoutFloat();
    if (kind != FloatKind::DocSheet) rebuildNavBody();
    rebuildDividers();
    rebuildRail();
}

void RefMenuPanel::hideFloat()
{
    if (m_floatKind == FloatKind::None) return;
    const bool wasDropdown = (m_floatKind == FloatKind::Dropdown);
    const bool wasDoc = (m_floatKind == FloatKind::DocSheet);
    m_floatKind = FloatKind::None;
    m_floatFilter.clear();
    if (m_floatHost) m_floatHost->hide();
    if (m_scrim) m_scrim->hide();
    if (m_floatBand) m_floatBand->setVisible(false);
    // Devolve ao splitter; applyNavVisibility decide se aparece.
    if (wasDoc) m_bodySplit->insertWidget(1 - navSplitIndex(), m_previewPane);
    else        m_bodySplit->insertWidget(navSplitIndex(), m_navPane);
    m_bodySplit->setStretchFactor(navSplitIndex(), 0);
    m_bodySplit->setStretchFactor(1 - navSplitIndex(), 1);
    if (wasDropdown && m_searchInput && !m_searchInput->text().isEmpty()) {
        QSignalBlocker b(m_searchInput);
        m_searchInput->clear();
        m_searchQuery.clear();
    }
    applyNavVisibility();
    rebuildDividers();
}

void RefMenuPanel::layoutFloat()
{
    if (m_floatKind == FloatKind::None || !m_body || !m_floatHost || !m_scrim) return;
    const QRect r = m_body->rect();
    const bool mirror = m_navRight && (m_layout == Layout::Columns || m_layout == Layout::Rail);
    const int rw = (m_rail && m_rail->isVisible()) ? m_rail->width() : 0;
    QRect g;
    QRect veil = r;
    switch (m_floatKind) {
    case FloatKind::Drawer: {
        const int w = qMin(r.width(), qBound(240, int(r.width() * 0.8), 360));
        g = QRect(mirror ? r.width() - w : 0, 0, w, r.height());
        break;
    }
    case FloatKind::Flyout: {
        const int w = qMin(280, r.width() - rw);
        g = QRect(mirror ? r.width() - rw - w : rw, 0, w, r.height());
        // O véu não cobre o trilho: dá pra pular de seção sem fechar antes.
        if (mirror) veil.setRight(r.width() - rw - 1);
        else        veil.setLeft(rw);
        break;
    }
    case FloatKind::Dropdown:
        g = QRect(8, 0, qMax(100, r.width() - 16), qMin(r.height() - 8, 440));
        break;
    case FloatKind::Sheet: {
        // Folha subindo da Doca (que fica logo abaixo do corpo).
        const int h = qMin(r.height(), qMax(220, int(r.height() * 0.62)));
        g = QRect(0, r.height() - h, r.width(), h);
        break;
    }
    case FloatKind::Divider: {
        // Entra pela direita, rente às divisórias, que continuam clicáveis.
        const int dw = (m_dividers && m_dividers->isVisible()) ? m_dividers->width() : 0;
        const int w = qMin(r.width() - dw, qMax(240, int((r.width() - dw) * 0.82)));
        g = QRect(r.width() - dw - w, 0, w, r.height());
        veil.setRight(r.width() - dw - 1);
        break;
    }
    case FloatKind::DocSheet:
        // O documento sobe por cima do mural, deixando uma borda à vista.
        g = QRect(10, 10, qMax(100, r.width() - 20), qMax(100, r.height() - 10));
        break;
    case FloatKind::None:
        return;
    }
    m_scrim->setGeometry(veil);
    m_floatHost->setGeometry(g);
    m_scrim->raise();
    m_floatHost->raise();
}

// ---- Quem está na cena ----

QList<RefMenuPanel::ScenePerson> RefMenuPanel::scenePeople() const
{
    QList<ScenePerson> out;
    const QString sceneKey = m_currentDocKey.isEmpty() ? editorDocKey() : m_currentDocKey;
    if (!m_elements || sceneKey.isEmpty()) return out;
    const QStringList ids = m_elements->docElementIds(sceneKey);
    for (const QString& id : ids) {
        for (const Element& e : m_elements->elements()) {
            if (e.id != id) continue;
            ScenePerson sp;
            sp.id = e.id;
            sp.name = e.name;
            sp.role = e.role;
            sp.image = e.image;
            // A ficha do elemento é um item de gaveta que aponta pra ele
            // (DrawerItem::elementId).
            if (m_model) {
                for (const Drawer& d : m_model->drawers()) {
                    for (const DrawerItem& it : d.items) {
                        if (it.elementId == e.id) { sp.key = QStringLiteral("it:%1").arg(it.id); break; }
                    }
                    if (!sp.key.isEmpty()) break;
                }
            }
            out.append(sp);
            break;
        }
    }
    return out;
}

// ---- Trilho ----

void RefMenuPanel::rebuildRail()
{
    const bool dock = (m_layout == Layout::Dock);
    if (!m_railLay || (m_layout != Layout::Rail && !dock)) return;
    refClearLayout(m_railLay);
    const qreal dpr = devicePixelRatioF();
    // No Trilho as listas abrem ao lado; na Doca, sobem como folha.
    const FloatKind listKind = dock ? FloatKind::Sheet : FloatKind::Flyout;

    auto center = [this, dock](QWidget* w) {
        m_railLay->addWidget(w, 0, dock ? Qt::AlignVCenter : Qt::AlignHCenter);
    };
    auto sep = [this, &center, dock]() {
        auto* f = new QFrame(m_railInner);
        f->setObjectName(QStringLiteral("refRailSep"));
        if (dock) {
            f->setStyleSheet(QStringLiteral("QFrame#refRailSep{min-height:22px;max-height:22px;min-width:1px;max-width:1px;margin:0 3px;}"));
            f->setFixedSize(1, 22);
        } else {
            f->setFixedWidth(22);
        }
        center(f);
    };
    auto button = [this, &center](const QIcon& icon, const QSize& iconSize,
                                  const QString& tip, bool checked) {
        auto* b = new QToolButton(m_railInner);
        b->setObjectName(QStringLiteral("refRailBtn"));
        b->setCheckable(true);
        b->setChecked(checked);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setIcon(icon);
        b->setIconSize(iconSize);
        center(b);
        return b;
    };

    // Abre/fecha a coluna inteira — com ela aberta, é o estilo Duas colunas.
    // A Doca não tem coluna: a navegação inteira é o botão do cabeçalho.
    const bool colOpen = !m_navHidden && !m_autoCollapsed;
    if (!dock) {
    auto* colBtn = button(refMutedIcon(m_navRight ? QStringLiteral(":/icons/panel-right.svg")
                                                  : QStringLiteral(":/icons/panel-left.svg"), 16),
                          QSize(16, 16),
                          colOpen ? tr("Recolher navegação") : tr("Abrir navegação"),
                          colOpen && m_floatKind == FloatKind::None);
    connect(colBtn, &QToolButton::clicked, this, &RefMenuPanel::onToggleNav);
    }

    // Rostos de quem está na cena: a ficha a um clique, sem abrir lista.
    const QList<ScenePerson> people = scenePeople();
    if (!people.isEmpty()) {
        if (!dock) sep();
        int shown = 0;
        for (const ScenePerson& sp : people) {
            if (++shown > 6) break;
            auto* a = new QToolButton(m_railInner);
            a->setObjectName(QStringLiteral("refRailAvatar"));
            a->setCheckable(true);
            a->setChecked(!sp.key.isEmpty() && sp.key == m_selectedKey);
            a->setIcon(QIcon(refRoundThumb(sp.image, sp.name, sp.id, 26, dpr)));
            a->setIconSize(QSize(26, 26));
            a->setToolTip(sp.role.isEmpty() ? sp.name : QStringLiteral("%1 · %2").arg(sp.name, sp.role));
            if (sp.key.isEmpty()) {
                a->setEnabled(false);   // marcado na cena, mas sem ficha pra abrir
            } else {
                a->setCursor(Qt::PointingHandCursor);
                const QString key = sp.key;
                connect(a, &QToolButton::clicked, this, [this, key]() {
                    setSelected(key);
                    addRecent(key);
                    rebuildCrumbs();
                });
            }
            center(a);
        }
    }

    sep();
    auto flyBtn = [&](const QIcon& icon, const QSize& iconSize, const QString& tip, const QString& filter) {
        const bool on = (m_floatKind == listKind && m_floatFilter == filter);
        auto* b = button(icon, iconSize, tip, on);
        connect(b, &QToolButton::clicked, this, [this, filter, listKind]() {
            if (m_floatKind == listKind && m_floatFilter == filter) hideFloat();
            else showFloat(listKind, filter);
        });
        return b;
    };
    flyBtn(refMutedIcon(QStringLiteral(":/icons/elements/star.svg"), 16), QSize(16, 16),
           tr("Fixados"), QStringLiteral("pinned"));
    flyBtn(refMutedIcon(QStringLiteral(":/icons/stats-clock.svg"), 16), QSize(16, 16),
           tr("Recentes"), QStringLiteral("recent"));
    sep();
    if (m_model) {
        for (const auto& ms : m_model->manuscripts()) {
            const QPixmap cover = refCoverThumb(ms.coverDataUrl, dpr);
            const QString name = ms.title.isEmpty() ? tr("Manuscrito") : ms.title;
            if (!cover.isNull())
                flyBtn(QIcon(cover), QSize(14, 19), name, QStringLiteral("ms:%1").arg(ms.id));
            else
                flyBtn(refMutedIcon(QStringLiteral(":/icons/elements/manuscript.svg"), 16), QSize(16, 16),
                       name, QStringLiteral("ms:%1").arg(ms.id));
        }
        for (const Drawer& d : m_model->drawers()) {
            flyBtn(refDrawerIcon(d, 16), QSize(16, 16),
                   d.title.isEmpty() ? tr("Gaveta") : d.title, QStringLiteral("dr:%1").arg(d.key));
        }
    }
    flyBtn(refMutedIcon(QStringLiteral(":/icons/worldmap.svg"), 16), QSize(16, 16),
           tr("Mundos"), QStringLiteral("world"));
    m_railLay->addStretch();
}

// ---- Três colunas: coluna de fontes ----

void RefMenuPanel::rebuildMillerSources()
{
    if (!m_millerLay || m_layout != Layout::Miller) return;
    refClearLayout(m_millerLay);
    const qreal dpr = devicePixelRatioF();

    // Fonte que sumiu (gaveta apagada, manuscrito excluído) volta pro Início.
    bool valid = (m_millerSource == QLatin1String("home") || m_millerSource == QLatin1String("world"));
    if (!valid && m_model) {
        for (const auto& ms : m_model->manuscripts())
            if (m_millerSource == QStringLiteral("ms:%1").arg(ms.id)) { valid = true; break; }
        for (const Drawer& d : m_model->drawers())
            if (m_millerSource == QStringLiteral("dr:%1").arg(d.key)) { valid = true; break; }
    }
    if (!valid) m_millerSource = QStringLiteral("home");

    auto addRow = [&](const QString& key, const QString& label, const QString& meta,
                      const QPixmap& thumb, const QIcon& icon) {
        const bool sel = (key == m_millerSource);
        auto* row = new QWidget(m_millerInner);
        row->setObjectName(QStringLiteral("refMillerRow"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setCursor(Qt::PointingHandCursor);
        row->setProperty("sel", sel);
        row->setToolTip(label);
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(8, 6, 8, 6);
        lay->setSpacing(8);
        auto* ic = new QLabel(row);
        ic->setFixedSize(16, 20);
        ic->setAlignment(Qt::AlignCenter);
        if (!thumb.isNull()) ic->setPixmap(thumb);
        else                 ic->setPixmap(icon.pixmap(15, 15));
        lay->addWidget(ic);
        auto* name = new QLabel(label, row);
        name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        name->setStyleSheet(QStringLiteral("color:%1; font-size:13px;%2")
            .arg(sel ? Theme::textBright() : Theme::textPrimary(),
                 sel ? QStringLiteral(" font-weight:600;") : QString()));
        lay->addWidget(name, 1);
        if (!meta.isEmpty()) {
            auto* cnt = new QLabel(meta, row);
            cnt->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(Theme::textMuted()));
            lay->addWidget(cnt);
        }
        row->setProperty("refMillerKey", key);
        row->installEventFilter(this);
        m_millerLay->addWidget(row);
    };

    addRow(QStringLiteral("home"), tr("Início"), QString(), QPixmap(),
           refMutedIcon(QStringLiteral(":/icons/home.svg"), 15));
    auto* title = new QLabel(tr("FONTES"), m_millerInner);
    title->setStyleSheet(QStringLiteral(
        "color:%1; font-size:10px; font-weight:700; letter-spacing:1.2px; padding:12px 8px 3px;")
        .arg(Theme::textMuted()));
    m_millerLay->addWidget(title);

    if (m_model) {
        for (const auto& ms : m_model->manuscripts()) {
            int chapters = 0;
            for (const Chapter& c : m_model->chapters())
                if (c.manuscriptId == ms.id) ++chapters;
            addRow(QStringLiteral("ms:%1").arg(ms.id), ms.title.isEmpty() ? tr("Manuscrito") : ms.title,
                   QString::number(chapters), refCoverThumb(ms.coverDataUrl, dpr),
                   refMutedIcon(QStringLiteral(":/icons/elements/manuscript.svg"), 15));
        }
        for (const Drawer& d : m_model->drawers()) {
            addRow(QStringLiteral("dr:%1").arg(d.key), d.title.isEmpty() ? tr("Gaveta") : d.title,
                   QString::number(d.items.size()), QPixmap(), refDrawerIcon(d, 15));
        }
    }
    int worlds = 0;
    if (m_territorioStore) worlds += int(m_territorioStore->territorios().size());
    if (m_construtorStore) worlds += int(m_construtorStore->systems().size());
    addRow(QStringLiteral("world"), tr("Mundos"), QString::number(worlds), QPixmap(),
           refMutedIcon(QStringLiteral(":/icons/worldmap.svg"), 15));
    m_millerLay->addStretch();
}

// ---- Leitor: fileira de atalhos ----

void RefMenuPanel::rebuildReaderChips()
{
    if (!m_readerChipsLay || m_layout != Layout::Reader) return;
    refClearLayout(m_readerChipsLay);
    const qreal dpr = devicePixelRatioF();
    int shown = 0;

    auto chip = [&](const QString& key, const QString& text, const QIcon& icon, const QString& tip) {
        auto* b = new QToolButton(m_readerChips);
        b->setObjectName(QStringLiteral("refChip"));
        b->setCheckable(true);
        b->setChecked(key == m_selectedKey);
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setText(QFontMetrics(b->font()).elidedText(text, Qt::ElideRight, 150));
        b->setIcon(icon);
        b->setIconSize(QSize(16, 16));
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, [this, key]() {
            setSelected(key);
            addRecent(key);
            rebuildCrumbs();
        });
        m_readerChipsLay->addWidget(b);
        ++shown;
    };

    bool labeled = false;
    int people = 0;
    for (const ScenePerson& sp : scenePeople()) {
        if (sp.key.isEmpty()) continue;      // sem ficha, não há o que abrir
        if (people >= 4) break;
        if (!labeled) {
            auto* l = new QLabel(tr("NA CENA"), m_readerChips);
            l->setObjectName(QStringLiteral("refChipsLabel"));
            m_readerChipsLay->addWidget(l);
            labeled = true;
        }
        chip(sp.key, sp.name.section(QLatin1Char(' '), 0, 0),
             QIcon(refRoundThumb(sp.image, sp.name, sp.id, 16, dpr)), sp.name);
        ++people;
    }
    const QIcon star = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/star.svg"),
        QColor(Theme::accentWarning()), QColor(Theme::accentWarning()), QColor(Theme::accentWarning()),
        QSize(13, 13));
    int pins = 0;
    for (const QString& k : m_pinnedKeys) {
        if (pins >= 3) break;
        KeyInfo info;
        if (!describeKey(k, &info)) continue;
        chip(k, info.name, star, info.name);
        ++pins;
    }
    m_readerChipsLay->addStretch();
    if (m_readerChipsScroll) m_readerChipsScroll->setVisible(shown > 0);
}

// =========================================================================
// Abas (globais — valem em todos os estilos)
// =========================================================================
//
// Cada aba guarda a chave do documento que mostra ("" = aba nova, vazia). A
// aba ativa acompanha a seleção: abrir um documento troca o conteúdo da aba
// ativa (como clicar num link no navegador) — a não ser que ele já esteja
// aberto em outra aba, aí ela vira a ativa. Ctrl+clique, clique do meio ou o
// menu da linha abrem numa aba nova.

namespace {
constexpr int kMaxTabs = 10;
}

void RefMenuPanel::syncTabs(const QString& key)
{
    if (m_tabs.isEmpty()) {
        if (!key.isEmpty()) {
            m_tabs << key;
            m_tabIdx = 0;
        }
    } else if (!key.isEmpty() && m_tabs.contains(key)) {
        m_tabIdx = int(m_tabs.indexOf(key));
    } else if (key.isEmpty() && m_tabs.size() == 1) {
        // Voltar pro "Tudo" com uma aba só não deixa uma aba vazia pendurada.
        m_tabs.clear();
        m_tabIdx = 0;
    } else {
        m_tabIdx = qBound(0, m_tabIdx, int(m_tabs.size()) - 1);
        m_tabs[m_tabIdx] = key;
    }
    rebuildTabBar();
    savePinsAndRecents();
}

void RefMenuPanel::rebuildTabBar()
{
    if (!m_tabBar || !m_tabRow) return;
    m_tabRow->setVisible(!m_tabs.isEmpty());
    // Só trocou a aba ativa: não refaz a barra (pode estar no meio do clique
    // dela). Refaz quando a lista muda ou o refresh pede (títulos mudaram).
    if (m_tabBarKeys == m_tabs && m_tabBar->count() == m_tabs.size()) {
        QSignalBlocker b(m_tabBar);
        if (!m_tabs.isEmpty()) m_tabBar->setCurrentIndex(qBound(0, m_tabIdx, int(m_tabs.size()) - 1));
        return;
    }
    m_tabBarKeys = m_tabs;
    m_syncingTabs = true;
    {
        QSignalBlocker b(m_tabBar);
        while (m_tabBar->count() > 0) m_tabBar->removeTab(0);
        const qreal dpr = devicePixelRatioF();
        for (int i = 0; i < m_tabs.size(); ++i) {
            const QString key = m_tabs.at(i);
            KeyInfo info;
            const bool known = !key.isEmpty() && describeKey(key, &info);
            const QString title = key.isEmpty() ? tr("Nova aba")
                                : (known ? info.name : tr("(apagado)"));
            QIcon icon;
            if (known && !info.imagePath.isEmpty())
                icon = QIcon(refRoundThumb(info.imagePath, info.name, key, 16, dpr));
            const int idx = m_tabBar->addTab(icon, title);
            m_tabBar->setTabToolTip(idx, known && !info.sectionLabel.isEmpty()
                ? QStringLiteral("%1 · %2").arg(info.name, info.sectionLabel) : title);
            // Fechar com botão próprio: o do estilo nativo some em tema escuro.
            auto* close = new QToolButton(m_tabBar);
            close->setObjectName(QStringLiteral("refTabClose"));
            close->setText(QString::fromUtf8("×"));
            close->setToolTip(tr("Fechar aba"));
            close->setCursor(Qt::PointingHandCursor);
            connect(close, &QToolButton::clicked, this, [this, close]() {
                for (int t = 0; t < m_tabBar->count(); ++t) {
                    if (m_tabBar->tabButton(t, QTabBar::RightSide) == close) { closeTab(t); return; }
                }
            });
            m_tabBar->setTabButton(idx, QTabBar::RightSide, close);
        }
        if (!m_tabs.isEmpty()) m_tabBar->setCurrentIndex(qBound(0, m_tabIdx, int(m_tabs.size()) - 1));
    }
    m_syncingTabs = false;
    m_tabRow->setVisible(!m_tabs.isEmpty());
}

void RefMenuPanel::openInNewTab(const QString& key)
{
    if (key.isEmpty()) return;
    if (!m_tabs.contains(key)) {
        const int at = m_tabs.isEmpty() ? 0 : qMin(m_tabIdx + 1, int(m_tabs.size()));
        m_tabs.insert(at, key);
        // Passou do limite: sai a aba mais distante da ativa.
        while (m_tabs.size() > kMaxTabs) {
            const int drop = (at > m_tabs.size() / 2) ? 0 : int(m_tabs.size()) - 1;
            m_tabs.removeAt(drop);
        }
    }
    m_syncingTabs = false;
    setSelected(key);            // changeSelectedKey → syncTabs acha a aba e ativa
    if (m_selectedKey == key) {  // já era a seleção: syncTabs não rodou
        m_tabIdx = int(m_tabs.indexOf(key));
        rebuildTabBar();
        savePinsAndRecents();
    }
}

void RefMenuPanel::closeTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;
    const bool wasActive = (index == m_tabIdx);
    m_tabs.removeAt(index);
    if (index < m_tabIdx) --m_tabIdx;
    m_tabIdx = qBound(0, m_tabIdx, qMax(0, int(m_tabs.size()) - 1));
    if (wasActive) {
        const QString next = m_tabs.isEmpty() ? QString() : m_tabs.at(m_tabIdx);
        m_syncingTabs = true;
        changeSelectedKey(next);
        m_syncingTabs = false;
        rebuildPreview();
        rebuildCrumbs();
        rebuildRail();
        rebuildReaderChips();
        if (next.isEmpty() && m_layout == Layout::Gallery) hideFloat();
    }
    rebuildTabBar();
    savePinsAndRecents();
}

void RefMenuPanel::pruneTabs()
{
    // Só com o projeto carregado: antes disso tudo pareceria apagado.
    if (!m_model || (m_model->manuscripts().isEmpty() && m_model->drawers().isEmpty())) return;
    bool changed = false;
    for (int i = int(m_tabs.size()) - 1; i >= 0; --i) {
        const QString key = m_tabs.at(i);
        KeyInfo info;
        if (!key.isEmpty() && !describeKey(key, &info)) {
            m_tabs.removeAt(i);
            if (i < m_tabIdx) --m_tabIdx;
            changed = true;
        }
    }
    if (changed) {
        m_tabIdx = qBound(0, m_tabIdx, qMax(0, int(m_tabs.size()) - 1));
        savePinsAndRecents();
    }
}

// =========================================================================
// Comparar (global)
// =========================================================================
//
// Duas páginas lado a lado. A "viva" é a de sempre: segue a navegação, edita,
// busca, tem abas. A lateral segura um documento. Ligar o Comparar prende o
// documento atual na lateral (à esquerda) e deixa a viva à direita, pronta
// pro próximo. Clicar na faixa da lateral troca os papéis.

void RefMenuPanel::setCompare(bool on)
{
    if (m_compare == on) return;
    m_compare = on;
    if (on) {
        m_sideKey = m_selectedKey;
        m_livePane = 1;
    } else {
        m_sideKey.clear();
    }
    if (m_compareBtn) {
        QSignalBlocker b(m_compareBtn);
        m_compareBtn->setChecked(on);
    }
    placeCompareViews();
    renderDoc(m_side, m_sideKey);
}

void RefMenuPanel::openBeside(const QString& key)
{
    if (key.isEmpty()) return;
    if (!m_compare) {
        m_compare = true;
        m_livePane = 0;   // a viva fica onde estava; o novo abre à direita
        if (m_compareBtn) {
            QSignalBlocker b(m_compareBtn);
            m_compareBtn->setChecked(true);
        }
    }
    m_sideKey = key;
    placeCompareViews();
    renderDoc(m_side, m_sideKey);
}

void RefMenuPanel::activateSidePane()
{
    if (!m_compare) return;
    const QString held = m_selectedKey;
    const QString next = m_sideKey;
    m_sideKey = held;
    m_livePane = 1 - m_livePane;
    placeCompareViews();
    renderDoc(m_side, m_sideKey);
    if (!next.isEmpty()) {
        setSelected(next);
        addRecent(next);
    }
}

void RefMenuPanel::placeCompareViews()
{
    if (!m_cmpSplit) return;
    m_cmpSplit->insertWidget(m_livePane, m_previewWrap);
    m_cmpSplit->insertWidget(1 - m_livePane, m_side.wrap);
    m_side.wrap->setVisible(m_compare);
    for (DocView* v : { &m_main, &m_side }) {
        const bool live = (v == &m_main);
        v->bar->setVisible(m_compare);
        v->barLabel->setText(live ? tr("ABRE AQUI") : tr("Abrir aqui"));
        v->bar->setToolTip(live ? tr("O próximo documento abre nesta página")
                                : tr("Clique pra esta página receber o próximo documento"));
        for (QWidget* w : { static_cast<QWidget*>(v->bar), static_cast<QWidget*>(v->barLabel) }) {
            w->setProperty("live", live);
            w->style()->unpolish(w);
            w->style()->polish(w);
        }
    }
    if (m_compare) {
        const int total = qMax(2, m_cmpSplit->width());
        m_cmpSplit->setSizes({ total / 2, total - total / 2 });
    }
}

// =========================================================================
// Galeria
// =========================================================================

namespace {

// Cartão sem foto: um degradê com as iniciais, na mesma família dos
// avatares (cor estável pelo id), com os cantos de cima arredondados.
QPixmap refGalleryPicture(const QString& photoSrc, const QString& name, const QString& id,
                          int w, int h, int radius, qreal dpr)
{
    const int pw = qMax(1, int(w * dpr)), ph = qMax(1, int(h * dpr));
    QPixmap out(pw, ph);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clip;
    const qreal r = radius * dpr;
    clip.moveTo(0, ph);
    clip.lineTo(0, r);
    clip.quadTo(0, 0, r, 0);
    clip.lineTo(pw - r, 0);
    clip.quadTo(pw, 0, pw, r);
    clip.lineTo(pw, ph);
    clip.closeSubpath();
    p.setClipPath(clip);

    const QPixmap photo = refThumbFrom(photoSrc);
    if (!photo.isNull()) {
        const QPixmap sc = photo.scaled(pw, ph, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        p.drawPixmap((pw - sc.width()) / 2, (ph - sc.height()) / 2, sc);
    } else {
        const int hue = int(qHash(id.isEmpty() ? name : id) % 360);
        QRadialGradient g(QPointF(pw * 0.3, ph * 0.25), qMax(pw, ph) * 0.9);
        g.setColorAt(0.0, QColor::fromHsl(hue, 90, 118));
        g.setColorAt(1.0, QColor::fromHsl((hue + 30) % 360, 70, 42));
        p.fillRect(0, 0, pw, ph, g);
        QStringList parts = name.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        QString initials;
        for (const QString& part : parts) {
            if (initials.size() >= 2) break;
            initials += part.left(1).toUpper();
        }
        QFont f(QStringLiteral("Georgia"));
        f.setBold(true);
        f.setPixelSize(qMax(10, int(qMin(pw, ph) * 0.2)));
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 140));
        p.drawText(QRect(int(10 * dpr), 0, pw - int(20 * dpr), ph - int(8 * dpr)),
                   Qt::AlignLeft | Qt::AlignBottom, initials);
    }
    p.end();
    out.setDevicePixelRatio(dpr);
    return out;
}

} // namespace

QString RefMenuPanel::gallerySource()
{
    // Sem escolha ainda: quem está na cena; sem ninguém, a primeira gaveta;
    // sem gaveta, o primeiro manuscrito.
    auto valid = [this](const QString& src) {
        if (src == QLatin1String("scene")) return !scenePeople().isEmpty();
        if (src == QLatin1String("world")) return true;
        if (!m_model) return false;
        for (const Drawer& d : m_model->drawers())
            if (src == QStringLiteral("dr:%1").arg(d.key)) return true;
        for (const auto& ms : m_model->manuscripts())
            if (src == QStringLiteral("ms:%1").arg(ms.id)) return true;
        return false;
    };
    if (!m_gallerySource.isEmpty() && valid(m_gallerySource)) return m_gallerySource;
    if (!scenePeople().isEmpty()) return m_gallerySource = QStringLiteral("scene");
    if (m_model && !m_model->drawers().isEmpty())
        return m_gallerySource = QStringLiteral("dr:%1").arg(m_model->drawers().first().key);
    if (m_model && !m_model->manuscripts().isEmpty())
        return m_gallerySource = QStringLiteral("ms:%1").arg(m_model->manuscripts().first().id);
    return m_gallerySource = QStringLiteral("world");
}

void RefMenuPanel::rebuildGalleryBar()
{
    if (!m_galleryChipsLay || m_layout != Layout::Gallery) return;
    refClearLayout(m_galleryChipsLay);
    const QString cur = gallerySource();
    auto chip = [&](const QString& key, const QString& text, const QIcon& icon) {
        auto* b = new QToolButton(m_galleryChips);
        b->setObjectName(QStringLiteral("refChip"));
        b->setCheckable(true);
        b->setChecked(key == cur);
        b->setToolButtonStyle(icon.isNull() ? Qt::ToolButtonTextOnly : Qt::ToolButtonTextBesideIcon);
        b->setText(QFontMetrics(b->font()).elidedText(text, Qt::ElideRight, 160));
        if (!icon.isNull()) {
            b->setIcon(icon);
            b->setIconSize(QSize(13, 13));
        }
        b->setToolTip(text);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, [this, key]() {
            m_gallerySource = key;
            hideFloat();
            rebuildGalleryBar();
            rebuildNavBody();
        });
        m_galleryChipsLay->addWidget(b);
    };
    if (!scenePeople().isEmpty()) chip(QStringLiteral("scene"), tr("Na cena"), QIcon());
    if (m_model) {
        for (const Drawer& d : m_model->drawers())
            chip(QStringLiteral("dr:%1").arg(d.key), d.title.isEmpty() ? tr("Gaveta") : d.title, refDrawerIcon(d, 13));
        for (const auto& ms : m_model->manuscripts())
            chip(QStringLiteral("ms:%1").arg(ms.id), ms.title.isEmpty() ? tr("Manuscrito") : ms.title,
                 refMutedIcon(QStringLiteral(":/icons/elements/manuscript.svg"), 13));
    }
    chip(QStringLiteral("world"), tr("Mundos"), refMutedIcon(QStringLiteral(":/icons/worldmap.svg"), 13));
    m_galleryChipsLay->addStretch();
}

void RefMenuPanel::buildGalleryView()
{
    if (!m_navInner || !m_navInnerLay || !m_navScroll) return;
    const QString src = gallerySource();

    struct Card { QString key, name, sub, image, id; bool role = false; bool portrait = true; bool excerpt = false; };
    QList<Card> cards;
    if (src == QLatin1String("scene")) {
        for (const ScenePerson& sp : scenePeople()) {
            if (sp.key.isEmpty()) continue;
            cards.append({ sp.key, sp.name, sp.role.isEmpty() ? QString() : RoleTiers::roleDisplayName(sp.role),
                           sp.image, sp.id, true, true, false });
        }
    } else if (src.startsWith(QLatin1String("dr:")) && m_model) {
        if (const Drawer* d = m_model->findDrawer(src.mid(3))) {
            for (const DrawerItem& it : d->items) {
                const QString role = roleOrLabelForItem(it);
                // Gente vem em retrato; lugar e objeto, em paisagem.
                cards.append({ QStringLiteral("it:%1").arg(it.id),
                               it.title.isEmpty() ? tr("(sem título)") : it.title,
                               role.isEmpty() ? d->title : role, imageForItem(it), it.id,
                               !role.isEmpty(), !role.isEmpty(), false });
            }
        }
    } else if (src.startsWith(QLatin1String("ms:")) && m_model) {
        const QString msId = src.mid(3);
        QList<Chapter> chs;
        for (const Chapter& c : m_model->chapters())
            if (c.manuscriptId == msId) chs.append(c);
        std::sort(chs.begin(), chs.end(), [](const Chapter& a, const Chapter& b) { return a.order < b.order; });
        int n = 0;
        for (const Chapter& c : chs) {
            ++n;
            cards.append({ QStringLiteral("ch:%1:%2").arg(msId, c.id),
                           c.title.isEmpty() ? tr("(sem título)") : c.title,
                           tr("Capítulo %1").arg(n), QString(), c.id, false, false, true });
        }
    } else if (src == QLatin1String("world")) {
        if (m_territorioStore)
            for (const auto& t : m_territorioStore->territorios())
                cards.append({ QStringLiteral("lug:%1").arg(t.id), t.name.isEmpty() ? tr("(sem nome)") : t.name,
                               tr("território"), t.avatarDataUrl, t.id, false, false, false });
        if (m_construtorStore)
            for (const auto& sys : m_construtorStore->systems())
                cards.append({ QStringLiteral("ctr:%1").arg(sys.id), sys.name, tr("sistema"),
                               QString(), sys.id, false, false, false });
    }

    if (cards.isEmpty()) {
        auto* l = new QLabel(tr("Nada aqui ainda."), m_navInner);
        l->setStyleSheet(QStringLiteral("color:%1; font-size:11px; padding:12px 6px;").arg(Theme::textMuted()));
        m_navInnerLay->addWidget(l);
        return;
    }

    // Colunas pela largura de verdade: cartões de ~150 px, nunca menos de 2.
    const QMargins mm = m_navInnerLay->contentsMargins();
    const int avail = qMax(200, m_navScroll->viewport()->width() - mm.left() - mm.right());
    const int gap = 10;
    const int cols = qMax(2, (avail + gap) / (150 + gap));
    const int cardW = qMax(80, (avail - gap * (cols - 1)) / cols);
    const int radius = Theme::controlRadius();
    const qreal dpr = devicePixelRatioF();

    auto* host = new QWidget(m_navInner);
    auto* grid = new QGridLayout(host);
    grid->setContentsMargins(0, 4, 0, 0);
    grid->setHorizontalSpacing(gap);
    grid->setVerticalSpacing(gap);
    grid->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    int i = 0;
    for (const Card& c : cards) {
        auto* card = new QWidget(host);
        card->setObjectName(QStringLiteral("refGalleryCard"));
        card->setAttribute(Qt::WA_StyledBackground, true);
        card->setAttribute(Qt::WA_Hover, true);
        card->setCursor(Qt::PointingHandCursor);
        card->setFixedWidth(cardW);
        card->setToolTip(c.name);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(1, 1, 1, 1);
        v->setSpacing(0);

        if (c.excerpt) {
            // Capítulo: um trecho do começo, em itálico, no lugar da foto.
            QString text = m_excerptCache.value(c.key);
            if (!m_excerptCache.contains(c.key)) {
                QTextDocument doc;
                doc.setHtml(resolveDocHtml(c.key));
                text = doc.toPlainText().simplified();
                if (text.size() > 240) text = text.left(240).section(QLatin1Char(' '), 0, -2) + QString::fromUtf8("…");
                m_excerptCache.insert(c.key, text);
            }
            auto* ex = new QLabel(text.isEmpty() ? tr("(em branco)") : text, card);
            ex->setObjectName(QStringLiteral("refCardExcerpt"));
            ex->setWordWrap(true);
            ex->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            ex->setContentsMargins(9, 9, 9, 2);
            ex->setFixedHeight(qMax(70, cardW * 3 / 4));
            if (!m_editorFontFamily.isEmpty()) {
                QFont f(m_editorFontFamily);
                f.setItalic(true);
                f.setPixelSize(12);
                ex->setFont(f);
            }
            v->addWidget(ex);
        } else {
            const int picH = c.portrait ? cardW * 5 / 4 : cardW * 3 / 4;
            auto* pic = new QLabel(card);
            pic->setFixedSize(cardW - 2, picH);
            pic->setPixmap(refGalleryPicture(c.image, c.name, c.id, cardW - 2, picH, qMax(0, radius - 1), dpr));
            v->addWidget(pic);
        }

        auto* txt = new QWidget(card);
        auto* tl = new QVBoxLayout(txt);
        tl->setContentsMargins(9, 6, 9, 8);
        tl->setSpacing(1);
        auto* name = new QLabel(QFontMetrics(card->font()).elidedText(c.name, Qt::ElideRight, cardW - 20), txt);
        name->setObjectName(QStringLiteral("refCardName"));
        tl->addWidget(name);
        if (!c.sub.isEmpty()) {
            auto* sub = new QLabel(QFontMetrics(card->font()).elidedText(c.sub.toUpper(), Qt::ElideRight, cardW - 20), txt);
            sub->setObjectName(c.role ? QStringLiteral("refCardRole") : QStringLiteral("refCardMeta"));
            tl->addWidget(sub);
        }
        v->addWidget(txt);

        // Abre pelo mesmo caminho das linhas (eventFilter → setSelected).
        card->setProperty("refRowKey", c.key);
        card->installEventFilter(this);
        for (QWidget* child : card->findChildren<QWidget*>())
            child->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        grid->addWidget(card, i / cols, i % cols, Qt::AlignTop);
        ++i;
    }
    m_navInnerLay->addWidget(host);
}

// =========================================================================
// Mapa de Uso (global)
// =========================================================================
//
// Onde cada personagem e lugar aparece no livro, capítulo a capítulo. Lê a
// mesma presença que alimenta o "Nesta cena" (ElementsStore::docElementIds,
// do capítulo e de cada cena dele) — nada é recalculado.

void RefMenuPanel::setUsageMap(bool on)
{
    m_mapOn = on;
    QSettings().setValue(QStringLiteral("ui/refMenuPanel/usageMap"), on);
    if (m_mapBtn) {
        QSignalBlocker b(m_mapBtn);
        m_mapBtn->setChecked(on);
    }
    if (m_mapPanel) m_mapPanel->setVisible(on);
    if (on) rebuildUsageMap();
    QTimer::singleShot(0, this, [this]() { layoutFloat(); });
}

QString RefMenuPanel::usageMapManuscript() const
{
    if (!m_model || m_model->manuscripts().isEmpty()) return QString();
    auto exists = [this](const QString& id) {
        for (const auto& ms : m_model->manuscripts()) if (ms.id == id) return true;
        return false;
    };
    if (!m_mapMsId.isEmpty() && exists(m_mapMsId)) return m_mapMsId;
    if (m_host) {
        const auto vm = m_host->viewMode();
        if (!vm.manuscriptId.isEmpty() && exists(vm.manuscriptId)) return vm.manuscriptId;
    }
    if (!m_currentManuscriptId.isEmpty() && exists(m_currentManuscriptId)) return m_currentManuscriptId;
    return m_model->manuscripts().first().id;
}

void RefMenuPanel::rebuildUsageMap()
{
    if (!m_map || !m_mapBarLay || !m_model) return;

    // ---- Barra: título, manuscrito, filtros, fechar ----
    refClearLayout(m_mapBarLay);
    auto* title = new QLabel(tr("MAPA DE USO"), m_mapPanel);
    title->setObjectName(QStringLiteral("refMapTitle"));
    m_mapBarLay->addWidget(title);
    const QString msId = usageMapManuscript();
    if (m_model->manuscripts().size() > 1) {
        // Vários livros: escolhe qual mapear.
        auto* msBtn = new QToolButton(m_mapPanel);
        msBtn->setObjectName(QStringLiteral("refChip"));
        msBtn->setPopupMode(QToolButton::InstantPopup);
        msBtn->setCursor(Qt::PointingHandCursor);
        QString msTitle;
        auto* menu = new QMenu(msBtn);
        for (const auto& ms : m_model->manuscripts()) {
            const QString name = ms.title.isEmpty() ? tr("Manuscrito") : ms.title;
            if (ms.id == msId) msTitle = name;
            QAction* a = menu->addAction(name);
            a->setCheckable(true);
            a->setChecked(ms.id == msId);
            const QString id = ms.id;
            connect(a, &QAction::triggered, this, [this, id]() { m_mapMsId = id; rebuildUsageMap(); });
        }
        msBtn->setMenu(menu);
        msBtn->setText(QFontMetrics(msBtn->font()).elidedText(msTitle, Qt::ElideRight, 160)
                       + QString::fromUtf8(" ▾"));
        m_mapBarLay->addWidget(msBtn);
    }
    m_mapBarLay->addStretch();
    const QList<QPair<QString, QString>> filters = {
        { QStringLiteral("all"), tr("Tudo") },
        { QStringLiteral("people"), tr("Pessoas") },
        { QStringLiteral("things"), tr("Lugares e objetos") },
    };
    for (const auto& f : filters) {
        auto* b = new QToolButton(m_mapPanel);
        b->setObjectName(QStringLiteral("refChip"));
        b->setCheckable(true);
        b->setChecked(m_mapFilter == f.first);
        b->setText(f.second);
        b->setCursor(Qt::PointingHandCursor);
        const QString key = f.first;
        connect(b, &QToolButton::clicked, this, [this, key]() {
            m_mapFilter = key;
            m_mapRow.clear();
            rebuildUsageMap();
        });
        m_mapBarLay->addWidget(b);
    }
    auto* close = new QToolButton(m_mapPanel);
    close->setObjectName(QStringLiteral("refTinyBtn"));
    close->setText(QStringLiteral("✕"));
    close->setToolTip(tr("Fechar o Mapa de Uso"));
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QToolButton::clicked, this, [this]() { setUsageMap(false); });
    m_mapBarLay->addWidget(close);

    // ---- Colunas: os capítulos do manuscrito, na ordem ----
    QList<Chapter> chs;
    for (const Chapter& c : m_model->chapters())
        if (c.manuscriptId == msId) chs.append(c);
    std::sort(chs.begin(), chs.end(), [](const Chapter& a, const Chapter& b) { return a.order < b.order; });
    m_map->colKeys.clear();
    m_map->colTitles.clear();
    QHash<QString, QList<int>> presence;
    for (int i = 0; i < chs.size(); ++i) {
        const Chapter& c = chs.at(i);
        m_map->colKeys << QStringLiteral("ch:%1:%2").arg(msId, c.id);
        m_map->colTitles << (c.title.isEmpty() ? tr("(sem título)") : c.title);
        QSet<QString> ids;
        if (m_elements) {
            for (const QString& id : m_elements->docElementIds(ElementsStore::elementDocKeyForChapter(msId, c.id)))
                ids.insert(id);
            for (const Scene& sc : c.scenes)
                for (const QString& id : m_elements->docElementIds(ElementsStore::elementDocKeyForScene(msId, c.id, sc.id)))
                    ids.insert(id);
        }
        for (const QString& id : ids) presence[id].append(i);
    }

    // ---- Linhas: quem aparece; pessoas primeiro, depois por estreia ----
    QHash<QString, QColor> typeColor;
    if (m_elements)
        for (const ElementType& t : m_elements->elementTypes()) typeColor.insert(t.id, Theme::toColor(t.color));
    struct Tmp { RefUsageMap::Row row; bool person; };
    QList<Tmp> tmp;
    if (m_elements) {
        for (const Element& e : m_elements->elements()) {
            const QList<int> cols = presence.value(e.id);
            if (cols.isEmpty()) continue;
            const bool person = (e.type == QLatin1String("character")) || !e.role.isEmpty();
            if (m_mapFilter == QLatin1String("people") && !person) continue;
            if (m_mapFilter == QLatin1String("things") && person) continue;
            RefUsageMap::Row r;
            r.id = e.id;
            r.name = e.name;
            r.cols = cols;
            std::sort(r.cols.begin(), r.cols.end());
            // Cor da gaveta da ficha; sem ficha, a cor do tipo; sem tipo, a do tema.
            QColor color;
            for (const Drawer& d : m_model->drawers()) {
                for (const DrawerItem& it : d.items) {
                    if (it.elementId != e.id) continue;
                    r.key = QStringLiteral("it:%1").arg(it.id);
                    if (!d.color.isEmpty()) color = Theme::toColor(d.color);
                    break;
                }
                if (!r.key.isEmpty()) break;
            }
            if (!color.isValid()) color = typeColor.value(e.type);
            if (!color.isValid()) color = Theme::toColor(Theme::accentDefault());
            r.color = color;
            tmp.append({ r, person });
        }
    }
    std::sort(tmp.begin(), tmp.end(), [](const Tmp& a, const Tmp& b) {
        if (a.person != b.person) return a.person;
        if (a.row.cols.first() != b.row.cols.first()) return a.row.cols.first() < b.row.cols.first();
        return a.row.name.localeAwareCompare(b.row.name) < 0;
    });
    m_map->rows.clear();
    for (const Tmp& t : tmp) m_map->rows.append(t.row);

    // ---- Destaques: capítulo no editor e capítulo aberto aqui ----
    auto colOf = [&](const QString& chapterId) {
        for (int i = 0; i < chs.size(); ++i) if (chs.at(i).id == chapterId) return i;
        return -1;
    };
    m_map->editorCol = -1;
    if (m_host) {
        const auto vm = m_host->viewMode();
        if (vm.manuscriptId == msId) m_map->editorCol = colOf(vm.chapterId);
    }
    m_map->selCol = -1;
    if (m_selectedKey.startsWith(QLatin1String("ch:")) || m_selectedKey.startsWith(QLatin1String("sc:"))) {
        const QStringList parts = m_selectedKey.split(QLatin1Char(':'));
        if (parts.size() >= 3 && parts.at(1) == msId) m_map->selCol = colOf(parts.at(2));
    }
    // Ficha aberta destaca a linha dela.
    if (m_selectedKey.startsWith(QLatin1String("it:")))
        for (const auto& r : m_map->rows) if (r.key == m_selectedKey) m_mapRow = r.id;
    m_map->selRow = m_mapRow;

    const int avail = qMax(300, (m_mapScroll ? m_mapScroll->viewport()->width() : width()) - 4);
    m_map->relayout(avail);
    if (m_map->rows.isEmpty()) {
        m_map->setFixedSize(avail, 40);
        m_map->setToolTip(tr("Ninguém marcado nos capítulos deste manuscrito ainda."));
    } else {
        m_map->setToolTip(QString());
    }
    // Altura: o mapa inteiro, até 42% do painel (acima disso, rola).
    const int cap = qMax(110, int(height() * 0.42));
    const int hbar = m_mapScroll->horizontalScrollBar()->sizeHint().height();
    const bool needH = m_map->width() > avail;
    m_mapScroll->setFixedHeight(qMin(cap, m_map->height() + (needH ? hbar : 0) + 2));
    // Capítulo do editor sempre à vista.
    if (m_map->editorCol >= 0 && needH) {
        const int x = (m_map->width() - 30) * m_map->editorCol / qMax(1, int(m_map->colKeys.size()) - 1);
        m_mapScroll->ensureVisible(x, 0, 80, 0);
    }
}

// =========================================================================
// Livro aberto: sumário
// =========================================================================

void RefMenuPanel::buildTocView()
{
    if (!m_navInner || !m_navInnerLay || !m_model) return;
    const QString serif = m_editorFontFamily.isEmpty() ? QStringLiteral("Georgia") : m_editorFontFamily;
    const QString muted = Theme::textMuted();

    auto label = [&](const QString& text, const QString& css, Qt::Alignment align = Qt::AlignLeft) {
        auto* l = new QLabel(text, m_navInner);
        l->setAlignment(align);
        l->setStyleSheet(css);
        m_navInnerLay->addWidget(l);
        return l;
    };
    // Linha do sumário: número · título · reticências · número à direita.
    auto line = [&](const QString& key, const QString& num, const QString& text, const QString& right,
                    bool italic, int indent) {
        auto* row = new QWidget(m_navInner);
        row->setObjectName(QStringLiteral("refNavRow"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setCursor(Qt::PointingHandCursor);
        const bool sel = (key == m_selectedKey);
        row->setStyleSheet(Theme::qss(QStringLiteral(
            "QWidget#refNavRow { border-radius: @radius-item; background: %1; }"
            "QWidget#refNavRow:hover { background: %2; }"))
            .arg(sel ? Theme::accentInfoSoft() : QStringLiteral("transparent"), Theme::hoverOverlay()));
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(4 + indent, 3, 4, 3);
        lay->setSpacing(6);
        auto* n = new QLabel(num, row);
        n->setFixedWidth(18);
        n->setStyleSheet(QStringLiteral("color:%1; font-family:'%2'; font-size:12px;").arg(muted, serif));
        lay->addWidget(n);
        auto* t = new QLabel(text, row);
        t->setMinimumWidth(1);
        t->setStyleSheet(QStringLiteral("color:%1; font-family:'%2'; font-size:%3px;%4")
            .arg(sel ? Theme::textBright() : Theme::textPrimary(), serif)
            .arg(italic ? 13 : 14).arg(italic ? QStringLiteral(" font-style:italic;") : QString()));
        lay->addWidget(t, 0);
        lay->addWidget(new RefLeader(row), 1);
        auto* r = new QLabel(right, row);
        r->setStyleSheet(QStringLiteral("color:%1; font-family:'%2'; font-size:12px; font-style:italic;")
            .arg(muted, serif));
        lay->addWidget(r);
        row->setProperty("refRowKey", key);
        row->installEventFilter(this);
        m_navInnerLay->addWidget(row);
    };
    auto words = [&](const QString& key) {
        auto it = m_wordCache.constFind(key);
        if (it != m_wordCache.constEnd()) return it.value();
        const int n = WordCounter::countWordsInHtml(resolveDocHtml(key));
        m_wordCache.insert(key, n);
        return n;
    };
    static const char* kRoman[] = { "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix", "x",
                                    "xi", "xii", "xiii", "xiv", "xv", "xvi", "xvii", "xviii", "xix", "xx" };

    label(tr("SUMÁRIO"), QStringLiteral("color:%1; font-size:10px; letter-spacing:3px; padding-top:8px;").arg(muted),
          Qt::AlignHCenter);
    for (const auto& ms : m_model->manuscripts()) {
        label(ms.title.isEmpty() ? tr("Manuscrito") : ms.title,
              QStringLiteral("color:%1; font-family:'%2'; font-size:17px; font-style:italic; padding:2px 0 10px;")
                  .arg(Theme::textBright(), serif), Qt::AlignHCenter);
        QList<Chapter> chs;
        for (const Chapter& c : m_model->chapters())
            if (c.manuscriptId == ms.id) chs.append(c);
        std::sort(chs.begin(), chs.end(), [](const Chapter& a, const Chapter& b) { return a.order < b.order; });
        const QStringList sel = m_selectedKey.split(QLatin1Char(':'));
        for (int i = 0; i < chs.size(); ++i) {
            const Chapter& c = chs.at(i);
            const QString key = QStringLiteral("ch:%1:%2").arg(ms.id, c.id);
            const int w = words(key);
            line(key, QString::number(i + 1), c.title.isEmpty() ? tr("(sem título)") : c.title,
                 w > 0 ? QLocale().toString(w) : QString::fromUtf8("—"), false, 0);
            // Capítulo aberto: as cenas dele, em algarismos romanos.
            const bool open = sel.size() >= 3 && sel.at(2) == c.id;
            if (open && c.scenes.size() > 1) {
                for (int s = 0; s < c.scenes.size(); ++s) {
                    const QString st = c.scenes.at(s).title;
                    line(QStringLiteral("sc:%1:%2:%3").arg(ms.id, c.id).arg(s), QString(),
                         st.isEmpty() ? tr("Cena %1").arg(s + 1) : st,
                         s < 20 ? QString::fromLatin1(kRoman[s]) : QString::number(s + 1), true, 22);
                }
            }
        }
    }

    // Apêndices: cada gaveta, com a cor dela; depois Mundos.
    int ap = 0;
    auto appendix = [&](const QString& name, const QString& color) {
        const QChar letter = QChar(QLatin1Char('A').unicode() + (ap++ % 26));
        auto* row = new QWidget(m_navInner);
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(4, 16, 4, 4);
        lay->setSpacing(8);
        auto* dot = new QLabel(row);
        dot->setFixedSize(7, 7);
        dot->setStyleSheet(QStringLiteral("background:%1; border-radius:3px;").arg(color));
        lay->addWidget(dot, 0, Qt::AlignVCenter);
        auto* l = new QLabel(tr("APÊNDICE %1 · %2").arg(letter).arg(name.toUpper()), row);
        l->setMinimumWidth(1);
        l->setStyleSheet(QStringLiteral("color:%1; font-size:9px; letter-spacing:2px;").arg(muted));
        lay->addWidget(l, 1);
        m_navInnerLay->addWidget(row);
    };
    for (const Drawer& d : m_model->drawers()) {
        if (d.items.isEmpty()) continue;
        appendix(d.title.isEmpty() ? tr("Gaveta") : d.title,
                 d.color.isEmpty() ? Theme::accentDefault() : d.color);
        for (const DrawerItem& it : d.items) {
            const QString role = roleOrLabelForItem(it);
            line(QStringLiteral("it:%1").arg(it.id), QString(),
                 it.title.isEmpty() ? tr("(sem título)") : it.title, role.toLower(), false, 0);
        }
    }
    const bool hasWorld = (m_territorioStore && !m_territorioStore->territorios().isEmpty())
                       || (m_construtorStore && !m_construtorStore->systems().isEmpty());
    if (hasWorld) {
        appendix(tr("Mundos"), Theme::textMuted());
        if (m_territorioStore)
            for (const auto& t : m_territorioStore->territorios())
                line(QStringLiteral("lug:%1").arg(t.id), QString(), t.name.isEmpty() ? tr("(sem nome)") : t.name,
                     tr("território"), false, 0);
        if (m_construtorStore)
            for (const auto& sys : m_construtorStore->systems())
                line(QStringLiteral("ctr:%1").arg(sys.id), QString(), sys.name, tr("sistema"), false, 0);
    }
}

// =========================================================================
// Fichário: divisórias
// =========================================================================

QString RefMenuPanel::sourceKeyOf(const QString& key) const
{
    if (key.startsWith(QLatin1String("ch:")) || key.startsWith(QLatin1String("sc:"))) {
        const QStringList p = key.split(QLatin1Char(':'));
        return p.size() >= 2 ? QStringLiteral("ms:%1").arg(p.at(1)) : QString();
    }
    if (key.startsWith(QLatin1String("lug:")) || key.startsWith(QLatin1String("ctr:")))
        return QStringLiteral("world");
    if (key.startsWith(QLatin1String("it:")) && m_model) {
        const QString id = key.mid(3);
        for (const Drawer& d : m_model->drawers())
            for (const DrawerItem& it : d.items)
                if (it.id == id) return QStringLiteral("dr:%1").arg(d.key);
    }
    return QString();
}

void RefMenuPanel::rebuildDividers()
{
    if (!m_dividers || m_layout != Layout::Binder) return;
    const QColor neutral = Theme::toColor(Theme::textMuted());
    m_dividers->tabs.clear();
    if (m_model) {
        for (const auto& ms : m_model->manuscripts())
            m_dividers->tabs.append({ QStringLiteral("ms:%1").arg(ms.id),
                                      ms.title.isEmpty() ? tr("Manuscrito") : ms.title, neutral });
        for (const Drawer& d : m_model->drawers()) {
            QColor c = Theme::toColor(d.color.isEmpty() ? Theme::accentDefault() : d.color);
            if (!c.isValid()) c = neutral;
            m_dividers->tabs.append({ QStringLiteral("dr:%1").arg(d.key),
                                      d.title.isEmpty() ? tr("Gaveta") : d.title, c });
        }
    }
    m_dividers->tabs.append({ QStringLiteral("world"), tr("Mundos"), neutral });
    m_dividers->tabs.append({ QStringLiteral("pinned"), QString::fromUtf8("★ ") + tr("Fixados"),
                              Theme::toColor(Theme::accentWarning()) });
    // A divisória saliente: a aberta agora ou, sem nenhuma aberta, a do documento.
    m_dividers->current = (m_floatKind == FloatKind::Divider) ? m_floatFilter : sourceKeyOf(m_selectedKey);
    m_dividers->update();
}
