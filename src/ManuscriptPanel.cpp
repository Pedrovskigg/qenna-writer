#include "ManuscriptPanel.h"
#include "ColorPopover.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "ManuscriptViews.h"
#include "MsVignette.h"
#include "TimelineChrono.h"
#include "CoverUtils.h"
#include "DialogueStore.h"
#include "ProjectInfoHover.h"
#include "ProjectModel.h"
#include "PanelMotion.h"
#include "Theme.h"
#include "WordCounter.h"

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QInputDialog>
#include <QJsonObject>
#include <QLineEdit>
#include <QLocale>
#include <QScreen>
#include <QSettings>
#include <QToolTip>
#include <algorithm>
#include <QApplication>
#include <QBuffer>
#include <QColor>
#include <QCoreApplication>
#include <QComboBox>
#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEnterEvent>
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
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr const char* kChapterMime = "application/x-mira-chapter-id";
constexpr const char* kSceneMime   = "application/x-mira-scene-ref";
}

namespace {
constexpr int kPanelWidth = 280;
constexpr int kComboThumbW = 28, kComboThumbH = 40;
constexpr int kComboRowH = kComboThumbH + 12;

// Delegate do popup aberto do combo de manuscritos: desenha a miniatura de
// capa (Qt::DecorationRole) maior que o ícone nativo de combo + o nome ao
// lado, numa linha mais alta. O combo FECHADO continua desenhando sozinho
// (ícone pequeno nativo do Qt) — isso só entra em ação no popup.
class ManuscriptComboDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return QSize(220, kComboRowH);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const override {
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, index);
        o.text.clear();
        o.icon = QIcon();
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);

        const QPixmap thumb = index.data(Qt::DecorationRole).value<QIcon>()
                                    .pixmap(kComboThumbW, kComboThumbH);
        const QString name = index.data(Qt::DisplayRole).toString();
        const QRect r = opt.rect;
        const QRect thumbRect(r.left() + 6, r.top() + (r.height() - kComboThumbH) / 2,
                               kComboThumbW, kComboThumbH);

        p->save();
        if (!thumb.isNull()) p->drawPixmap(thumbRect, thumb);
        else p->fillRect(thumbRect, opt.palette.mid());

        const QRect textRect(thumbRect.right() + 10, r.top(),
                              r.width() - thumbRect.width() - 22, r.height());
        p->setPen(opt.palette.color(o.state & QStyle::State_Selected
                                     ? QPalette::HighlightedText : QPalette::Text));
        p->drawText(textRect, Qt::AlignVCenter | Qt::TextSingleLine, name);
        p->restore();
    }
};

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
    return Theme::qss(QStringLiteral(R"(
        QPushButton {
            background: %4;
            color: %1;
            border: 1px solid %2;
            border-radius: @radius-control;
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
    )")).arg(accent, accent, bgHoverStr, bgStr, bgHoverStr);
}

// Quadradinho colorido pronto pra embutir em rich text via <img> — usado no
// tooltip da pilula em vez de `<span style="color:...">■</span>`: depender
// de cor de TEXTO em rich text de tooltip nativo se mostrou pouco confiável
// (ficava preto direto, mesmo com hex válido e o texto forçado como HTML).
// Uma imagem de verdade não depende de nenhuma interpretação de estilo de
// texto — os pixels JÁ nascem com a cor certa.
QString colorSquareImgTag(const QColor& color, int sizePx = 10)
{
    QPixmap pm(sizePx, sizePx);
    pm.fill(color);
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    pm.save(&buf, "PNG");
    return QStringLiteral("<img src=\"data:image/png;base64,%1\" width=\"%2\" height=\"%2\">")
        .arg(QString::fromLatin1(bytes.toBase64())).arg(sizePx);
}

// Pilulazinha VERTICAL de proporção diálogo/narração, encostada no início
// do nome do capítulo — azul de destaque (Theme::accentDefault) empilhado
// por cima pra diálogo, tom neutro (Theme::subtleBorder) embaixo pra
// narração. Cores lidas do Theme na hora de pintar (não cacheadas), então
// acompanham a troca de tema de graça — rebuildList() já reconstrói tudo do
// zero a cada applyTheme(), então nem precisa de conexão própria com
// themeChanged. Tooltip no hover mostra os dois valores em porcentagem.
class DialogueRatioBar : public QWidget {
public:
    explicit DialogueRatioBar(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedWidth(5);
        // Fixed, não Expanding: sem sizeHint próprio, uma policy vertical
        // Expanding aqui faz essa barra (e a linha inteira do capítulo, já
        // que ela mora dentro de um container ao lado do botão) engolir todo
        // o espaço sobrando da lista de capítulos — texto do capítulo fica
        // verticalmente centralizado num vão gigante e empurra as cenas pra
        // baixo. Altura fixa em 34 pra casar com btn->setMinimumHeight(34)
        // logo abaixo, que é quem realmente define a altura da linha.
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFixedHeight(34);
    }
    ~DialogueRatioBar() override { hidePopup(); }

    void setRatio(double dialogueFraction) {
        m_ratio = qBound(0.0, dialogueFraction, 1.0);
        const int dlgPct = qRound(m_ratio * 100.0);
        // QCoreApplication::translate explícito, não tr() — esta classe não
        // tem Q_OBJECT, então tr() resolveria pro contexto "QWidget" herdado
        // (nunca consultado em runtime), deixando a tradução presa em
        // português mesmo com o .ts certinho (mesmo bug já visto antes).
        m_infoHtml = QCoreApplication::translate("DialogueRatioBar",
                      "Conteúdo do texto:<br>"
                      "%1 Diálogo: %2%<br>"
                      "%3 Narração: %4%")
            .arg(colorSquareImgTag(QColor(Theme::accentDefault()))).arg(dlgPct)
            .arg(colorSquareImgTag(QColor(Theme::textMuted()))).arg(100 - dlgPct);
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        const QRectF full = QRectF(rect()).adjusted(0, 2, 0, -2);
        QPainterPath track;
        track.addRoundedRect(full, 2, 2);
        // Theme::subtleBorder() devolve uma string "rgba(r,g,b,a)" (só serve
        // em QSS) — QColor(QString) não entende esse formato e cai pra
        // preto opaco silenciosamente. Theme::panelBorder() é hex de
        // verdade, seguro pra construir QColor direto no C++.
        p.fillPath(track, QColor(Theme::panelBorder()));
        if (m_ratio > 0.0) {
            // Empilhado de CIMA pra baixo: diálogo primeiro.
            QRectF dlg = full;
            dlg.setHeight(full.height() * m_ratio);
            QPainterPath dlgPath;
            dlgPath.addRoundedRect(dlg, 2, 2);
            p.fillPath(dlgPath, QColor(Theme::accentDefault()));
        }
    }
    // Popup próprio em vez de setToolTip()/QToolTip nativo — no Windows, o
    // estilo nativo (windowsvista) IGNORA background-color do QSS e até a
    // QPalette::ToolTipBase pra esse widget específico, sempre pintando um
    // fundo preto do próprio SO por baixo. Um QLabel com Qt::ToolTip (só a
    // flag de janela, não a classe QToolTip) dá controle total de verdade,
    // igual o ProjectInfoHover já usa nesta mesma sessão.
    void enterEvent(QEnterEvent* event) override {
        showPopup();
        QWidget::enterEvent(event);
    }
    void leaveEvent(QEvent* event) override {
        hidePopup();
        QWidget::leaveEvent(event);
    }
private:
    void showPopup() {
        hidePopup();
        if (m_infoHtml.isEmpty()) return;
        m_popup = new QLabel(nullptr);
        m_popup->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        m_popup->setAttribute(Qt::WA_ShowWithoutActivating);
        m_popup->setTextFormat(Qt::RichText);
        m_popup->setStyleSheet(Theme::qss(QStringLiteral(
            "QLabel { background-color: %1; color: %2; border: 1px solid %3; "
            "border-radius: @radius-control; padding: 6px 10px; font-size: 12px; }")
            .arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder())));
        m_popup->setText(m_infoHtml);
        m_popup->adjustSize();
        m_popup->move(mapToGlobal(QPoint(width() + 6, 0)));
        m_popup->show();
    }
    void hidePopup() {
        if (m_popup) {
            m_popup->deleteLater();
            m_popup = nullptr;
        }
    }

    double m_ratio = 0.0;
    QString m_infoHtml;
    QLabel* m_popup = nullptr;
};
}

namespace {
constexpr int kRailW = 50;
constexpr const char* kKeyStyle = "ui/manuscriptPanel/style";
constexpr const char* kKeyCollapsed = "ui/manuscriptPanel/collapsedParts";
const char* const kPartColors[] = { "#d97757", "#8fc7a4", "#90caf9", "#b39ddb", "#d9a441", "#e57373" };

QColor tcol(const QString& css) { return Theme::toColor(css); }

QFont serifFont(qreal px, int weight = QFont::Normal, bool italic = false) {
    QFont f(QStringLiteral("Lora"));
    f.setPixelSize(qRound(px));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    return f;
}
QFont uiFont(qreal px, int weight = QFont::Normal) {
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(qRound(px));
    f.setWeight(QFont::Weight(weight));
    return f;
}

const QList<QColor>& povPalette() {
    static const QList<QColor> p = {
        QColor("#d9a441"), QColor("#8fa3c4"), QColor("#e57373"), QColor("#8fc7a4"),
        QColor("#b39ddb"), QColor("#90caf9"), QColor("#f0a07a"), QColor("#c5d86d") };
    return p;
}

QString contextMenuQss() {
    return Theme::qss(QStringLiteral(R"(
        QMenu {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: @radius-panel;
            padding: 4px;
        }
        QMenu::item { padding: 6px 16px; border-radius: @radius-item; }
        QMenu::item:selected { background: %4; color: %5; }
        QMenu::item:disabled { color: %6; }
        QMenu::separator { height: 1px; background: %3; margin: 4px 6px; }
    )")).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
           Theme::hoverOverlay(), Theme::textBright(), Theme::textMuted());
}

// Rótulo em versalete (SUMÁRIO, ONDE VOCÊ PAROU…).
QLabel* capsLabel(const QString& text, QWidget* parent, const QString& color = QString()) {
    auto* l = new QLabel(text.toUpper(), parent);
    QFont f = uiFont(9.5, QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
        .arg(color.isEmpty() ? Theme::textMuted() : color));
    return l;
}

QLabel* mutedLabel(const QString& text, QWidget* parent, qreal px = 11.5) {
    auto* l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setFont(uiFont(px));
    l->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textMuted()));
    return l;
}

// Botão-texto pequeno (links "Continuar ›", chips, segmentos).
QToolButton* linkButton(const QString& text, QWidget* parent, const QString& color, qreal px = 11.5,
                        bool bold = false) {
    auto* b = new QToolButton(parent);
    b->setText(text);
    b->setCursor(Qt::PointingHandCursor);
    b->setAutoRaise(true);
    b->setStyleSheet(Theme::qss(QStringLiteral(
        "QToolButton { color: %1; background: transparent; border: none; padding: 1px 2px;"
        " font-size: %2px; font-weight: %3; text-align: left; }"
        "QToolButton:hover { color: %4; }"))
        .arg(color).arg(px).arg(bold ? 600 : 400).arg(Theme::textBright()));
    return b;
}

QToolButton* chipButton(const QString& text, QWidget* parent, bool on) {
    auto* b = new QToolButton(parent);
    b->setText(text);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(Theme::qss(QStringLiteral(
        "QToolButton { color: %1; background: %2; border: 1px solid %3; border-radius: @radius-control;"
        " padding: 2px 8px; font-size: 11px; }"
        "QToolButton:hover { color: %4; border-color: %5; }"))
        .arg(on ? Theme::textBright() : Theme::textPrimary(),
             on ? Theme::accentInfoSoft() : QStringLiteral("transparent"),
             on ? Theme::accentInfoBorderSoft() : Theme::subtleBorder(),
             Theme::textBright(), Theme::borderStrong()));
    return b;
}

// Bolinha de cor pronta pra rich text (<img>): cor de texto em rich text de
// janela ToolTip já se mostrou traiçoeira aqui (ver colorSquareImgTag).
QString dotImgTag(const QColor& color, int sizePx = 9) {
    QPixmap pm(sizePx * 2, sizePx * 2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(QRectF(1, 1, sizePx * 2 - 2, sizePx * 2 - 2));
    p.end();
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    pm.save(&buf, "PNG");
    return QStringLiteral("<img src=\"data:image/png;base64,%1\" width=\"%2\" height=\"%2\">")
        .arg(QString::fromLatin1(bytes.toBase64())).arg(sizePx);
}

QString romanLower(int n) { return ProjectModel::toRomanNumeral(n).toLower(); }

// Linha da lista embrulhada: margem vertical própria (a lista usa spacing 0)
// e, dentro de uma Parte, o fio na cor dela — contínuo de uma linha pra outra.
class RowWrap : public QWidget {
public:
    RowWrap(QWidget* row, const QColor& line, QWidget* parent) : QWidget(parent), m_line(line) {
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(line.isValid() ? 16 : 0, 2, 0, 2);
        lay->setSpacing(0);
        lay->addWidget(row, 1);
        for (const char* key : { "kind", "chapterId", "sceneIndex" }) {
            const QVariant v = row->property(key);
            if (v.isValid()) setProperty(key, v);
        }
    }
protected:
    void paintEvent(QPaintEvent*) override {
        if (!m_line.isValid()) return;
        QPainter p(this);
        QColor c = m_line; c.setAlphaF(0.75);
        p.fillRect(QRect(9, 0, 2, height()), c);
    }
private:
    QColor m_line;
};

// Barrinha empilhada do Status (proporção de cada estágio).
class StackBar : public QWidget {
public:
    QList<QPair<QColor, int>> parts;
    explicit StackBar(QWidget* parent) : QWidget(parent) { setFixedHeight(5); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        int total = 0;
        for (const auto& pr : parts) total += pr.second;
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()), 2.5, 2.5);
        p.setClipPath(clip);
        p.fillRect(rect(), tcol(Theme::panelBorder()));
        if (!total) return;
        qreal x = 0;
        for (const auto& pr : parts) {
            const qreal w = qreal(width()) * pr.second / total;
            p.fillRect(QRectF(x, 0, w, height()), pr.first);
            x += w;
        }
    }
};

QString relativeWhen(const QDateTime& when) {
    if (!when.isValid()) return QString();
    const QDate d = when.date(), today = QDate::currentDate();
    const QString hm = QLocale().toString(when.time(), QLocale::ShortFormat);
    if (d == today) return QCoreApplication::translate("ManuscriptPanel", "hoje, %1").arg(hm);
    if (d == today.addDays(-1)) return QCoreApplication::translate("ManuscriptPanel", "ontem, %1").arg(hm);
    return QStringLiteral("%1, %2").arg(QLocale().toString(d, QLocale::ShortFormat), hm);
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
    loadSettings();

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

    // Trilho: o livro aberto sobe pro corpo e o cabeçalho vira só um rótulo.
    m_headerTitle = capsLabel(tr("Manuscritos"), header);
    headerLayout->addWidget(m_headerTitle, 1);

    m_combo = new QComboBox(header);
    m_combo->setContextMenuPolicy(Qt::CustomContextMenu);
    m_combo->setItemDelegate(new ManuscriptComboDelegate(m_combo));
    headerLayout->addWidget(m_combo, /*stretch=*/1);
    connect(m_combo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ManuscriptPanel::onComboChanged);
    connect(m_combo, &QComboBox::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QString msId = activeManuscriptId();
        if (msId.isEmpty()) return;
        showManuscriptContextMenu(msId, m_combo->mapToGlobal(pos));
    });

    // Tooltip rica ao passar o mouse num item do popup ABERTO do combo
    // (não é o combo fechado) — reaproveita o ProjectInfoHover (mesmo
    // layout capa+autor+gêneros+sinopse do hover da LeftBar), só que numa
    // instância própria deste painel, mostrando o manuscrito sob o mouse
    // em vez do ativo. Mesmo par de timers open/close do MainWindow.
    m_comboItemHover = new ProjectInfoHover(m_model, this);
    m_combo->view()->installEventFilter(this);
    m_combo->view()->viewport()->installEventFilter(this);
    m_comboHoverOpenTimer = new QTimer(this);
    m_comboHoverOpenTimer->setSingleShot(true);
    m_comboHoverOpenTimer->setInterval(350);
    m_comboHoverCloseTimer = new QTimer(this);
    m_comboHoverCloseTimer->setSingleShot(true);
    m_comboHoverCloseTimer->setInterval(220);
    connect(m_comboHoverOpenTimer, &QTimer::timeout, this, [this]() {
        auto* view = m_combo->view();
        const QModelIndex idx = view->indexAt(view->viewport()->mapFromGlobal(QCursor::pos()));
        if (!idx.isValid()) return;
        const QString msId = idx.data(Qt::UserRole).toString();
        if (msId.isEmpty()) return;
        const QRect r = view->visualRect(idx);
        const QPoint anchor = view->viewport()->mapToGlobal(r.topRight());
        m_comboItemHover->presentNear(anchor, msId);
    });
    connect(m_comboHoverCloseTimer, &QTimer::timeout, this, [this]() {
        if (m_comboItemHover && !m_comboItemHover->underMouse()) m_comboItemHover->hide();
    });
    connect(m_comboItemHover, &ProjectInfoHover::hoverLeft, this, [this]() {
        if (m_comboItemHover) m_comboItemHover->hide();
    });

    auto makeIconBtn = [this](const QString& glyph, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setText(glyph);
        b->setToolTip(tip);
        b->setFixedSize(28, 28);
        b->setCursor(Qt::PointingHandCursor);
        m_headerIconBtns.append(b);
        return b;
    };

    m_styleBtn = makeIconBtn(QString(), tr("Estilo e ferramentas"));
    m_styleBtn->setIconSize(QSize(16, 16));
    connect(m_styleBtn, &QToolButton::clicked, this, &ManuscriptPanel::showStyleMenu);
    headerLayout->addWidget(m_styleBtn);

    m_addMsBtn = makeIconBtn(QStringLiteral("✦"), tr("Novo manuscrito"));
    connect(m_addMsBtn, &QToolButton::clicked, this, &ManuscriptPanel::newManuscriptRequested);
    headerLayout->addWidget(m_addMsBtn);

    auto* btnClose = makeIconBtn(QStringLiteral("×"), tr("Fechar"));
    connect(btnClose, &QToolButton::clicked, this, &ManuscriptPanel::closePanel);
    headerLayout->addWidget(btnClose);

    root->addWidget(header);

    // Corpo: [trilho] [coluna: topo, botão de capítulo, lista, rodapé] -----
    auto* body = new QWidget(this);
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);

    m_rail = new QWidget(body);
    m_rail->setObjectName(QStringLiteral("msRail"));
    m_rail->setAttribute(Qt::WA_StyledBackground, true);
    m_rail->setFixedWidth(kRailW);
    m_railLayout = new QVBoxLayout(m_rail);
    m_railLayout->setContentsMargins(8, 10, 8, 10);
    m_railLayout->setSpacing(8);
    bodyLay->addWidget(m_rail);

    auto* column = new QWidget(body);
    auto* colLay = new QVBoxLayout(column);
    colLay->setContentsMargins(0, 0, 0, 0);
    colLay->setSpacing(0);

    m_top = new QWidget(column);
    m_topLayout = new QVBoxLayout(m_top);
    m_topLayout->setContentsMargins(12, 10, 12, 4);
    m_topLayout->setSpacing(10);
    colLay->addWidget(m_top);

    // Action bar (botão grande "Criar novo capítulo") ---------------------
    auto* actionBar = new QWidget(column);
    m_actionBar = actionBar;
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
    connect(createChapterBtn, &QPushButton::clicked, this, [this]() {
        emit newChapterRequested(activeManuscriptId());
    });
    actionLayout->addWidget(createChapterBtn);
    colLay->addWidget(actionBar);

    // Lista de capítulos --------------------------------------------------
    m_scroll = new QScrollArea(column);
    m_scroll->setObjectName(QStringLiteral("manuscriptScroll"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setStyleSheet(QStringLiteral("#manuscriptScroll { background: transparent; }"));
    m_scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* listHost = new QWidget(m_scroll);
    listHost->setStyleSheet(QStringLiteral("background: transparent;"));
    m_listLayout = new QVBoxLayout(listHost);
    m_listLayout->setContentsMargins(8, 8, 8, 8);
    m_listLayout->setSpacing(0);
    m_listLayout->addStretch();
    m_scroll->setWidget(listHost);
    colLay->addWidget(m_scroll, 1);

    m_bottom = new QWidget(column);
    m_bottomLayout = new QVBoxLayout(m_bottom);
    m_bottomLayout->setContentsMargins(12, 6, 12, 10);
    m_bottomLayout->setSpacing(6);
    colLay->addWidget(m_bottom);

    bodyLay->addWidget(column, 1);
    root->addWidget(body, 1);

    // Ficha no hover ------------------------------------------------------
    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    m_hoverTimer->setInterval(450);
    connect(m_hoverTimer, &QTimer::timeout, this, &ManuscriptPanel::showHoverCard);
    m_hoverCard = new QLabel(this, Qt::ToolTip | Qt::FramelessWindowHint);
    m_hoverCard->setAttribute(Qt::WA_ShowWithoutActivating);
    m_hoverCard->setTextFormat(Qt::RichText);
    m_hoverCard->setWordWrap(true);
    m_hoverCard->setFixedWidth(280);
    m_hoverCard->hide();

    if (m_model) {
        connect(m_model, &ProjectModel::manuscriptsChanged, this, &ManuscriptPanel::onManuscriptsChanged);
        connect(m_model, &ProjectModel::chaptersChanged, this, &ManuscriptPanel::onChaptersChanged);
        connect(m_model, &ProjectModel::loaded, this, [this]() {
            m_statusFilter.clear();
            m_povFilter.clear();
            m_povReading = 0;
            syncCombo();
            rebuildList();
        });
        // romanChapterNumbers (e outras prefs futuras) mudam o texto exibido
        // sem mexer na lista de capítulos em si — settingsChanged cobre isso.
        connect(m_model, &ProjectModel::settingsChanged, this, &ManuscriptPanel::onChaptersChanged);
    }

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &ManuscriptPanel::applyTheme);

    m_resizeHandle = new QWidget(this);
    m_resizeHandle->setObjectName(QStringLiteral("msResizeHandle"));
    m_resizeHandle->setCursor(Qt::SizeHorCursor);
    m_resizeHandle->setAttribute(Qt::WA_StyledBackground, true);
    m_resizeHandle->setToolTip(tr("Arraste pra mudar a largura"));
    m_resizeHandle->setProperty("resizeAxes", 1);
    m_resizeHandle->installEventFilter(this);
    m_resizeHandleBottom = new QWidget(this);
    m_resizeHandleBottom->setObjectName(QStringLiteral("msResizeHandle"));
    m_resizeHandleBottom->setCursor(Qt::SizeVerCursor);
    m_resizeHandleBottom->setAttribute(Qt::WA_StyledBackground, true);
    m_resizeHandleBottom->setToolTip(tr("Arraste pra mudar a altura · duplo clique volta pra altura toda"));
    m_resizeHandleBottom->setProperty("resizeAxes", 2);
    m_resizeHandleBottom->installEventFilter(this);
    m_resizeHandleCorner = new QWidget(this);
    m_resizeHandleCorner->setObjectName(QStringLiteral("msResizeHandle"));
    m_resizeHandleCorner->setCursor(Qt::SizeFDiagCursor);
    m_resizeHandleCorner->setAttribute(Qt::WA_StyledBackground, true);
    m_resizeHandleCorner->setProperty("resizeAxes", 3);
    m_resizeHandleCorner->installEventFilter(this);
    m_desiredHeight = QSettings().value(QStringLiteral("ui/manuscriptPanel/height"), 0).toInt();

    applyHeaderStyles();
    applyStyleWidth();
    hide();
}

void ManuscriptPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_resizeHandle) {
        m_resizeHandle->setGeometry(width() - 5, 0, 5, height() - 12);
        m_resizeHandle->raise();
    }
    if (m_resizeHandleBottom) {
        m_resizeHandleBottom->setGeometry(0, height() - 5, width() - 12, 5);
        m_resizeHandleBottom->raise();
    }
    if (m_resizeHandleCorner) {
        m_resizeHandleCorner->setGeometry(width() - 12, height() - 12, 12, 12);
        m_resizeHandleCorner->raise();
    }
}

void ManuscriptPanel::loadBookColors() {
    QHash<QString, QColor> colors;
    if (m_model) {
        const QJsonObject o = m_model->settings().value(QStringLiteral("manuscriptColors")).toObject();
        for (auto it = o.begin(); it != o.end(); ++it) colors.insert(it.key(), QColor(it.value().toString()));
    }
    MsPaint::setBookColorOverrides(colors);
}

void ManuscriptPanel::applyTheme() {
    setStyleSheet(Theme::panelQss(QStringLiteral("manuscriptPanel")));
    applyHeaderStyles();
    if (isPanelOpen()) rebuildList();
}

void ManuscriptPanel::setDialogueStore(DialogueStore* store) {
    if (m_dialogueStore == store) return;
    m_dialogueStore = store;
    if (m_dialogueStore) connect(m_dialogueStore, &DialogueStore::changed, this, [this]() {
        if (isPanelOpen()) rebuildList();
    });
    if (isPanelOpen()) rebuildList();
}

void ManuscriptPanel::setWordCounter(WordCounter* counter) {
    if (m_wordCounter == counter) return;
    m_wordCounter = counter;
    // countsChanged já é debounced lá dentro — seguro conectar direto.
    if (m_wordCounter) connect(m_wordCounter, &WordCounter::countsChanged, this, [this]() {
        if (isPanelOpen()) rebuildList();
    });
    if (isPanelOpen()) rebuildList();
}

void ManuscriptPanel::setElementsStore(ElementsStore* store) {
    m_elementsStore = store;
}

void ManuscriptPanel::setCurrentLocation(const QString& chapterId, int sceneIndex) {
    if (m_curChapterId == chapterId && m_curScene == sceneIndex) return;
    m_curChapterId = chapterId;
    m_curScene = sceneIndex;
    if (isPanelOpen()) rebuildList();
}

void ManuscriptPanel::setVisitedProgress(const QHash<QString, double>& visited) {
    m_visited = visited;
    if (isVisible() && m_style == Style::Reader) rebuildList();
}

void ManuscriptPanel::setResumeTrail(const QList<ResumeEntry>& trail) {
    m_resume = trail;
    if (isPanelOpen() && (tool(ToolResume) || m_style == Style::Reader || m_style == Style::Seasons)) rebuildList();
}

void ManuscriptPanel::hideEvent(QHideEvent* event) {
    disarmHover();
    QWidget::hideEvent(event);
}

void ManuscriptPanel::applyHeaderStyles() {
    if (m_header) {
        m_header->setStyleSheet(QStringLiteral(
            "#manuscriptHeader { border-bottom: 1px solid %1; }").arg(Theme::panelBorder()));
    }
    if (m_combo) {
        m_combo->setStyleSheet(Theme::qss(QStringLiteral(R"(
            QComboBox {
                background: transparent;
                color: %1;
                border: 1px solid %2;
                border-radius: @radius-control;
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
        )")).arg(Theme::textBright(), Theme::panelBorder(), Theme::subtleBorder(),
               Theme::panelBackground(), Theme::hoverOverlay()));
    }
    for (auto* b : m_headerIconBtns) {
        if (!b) continue;
        b->setStyleSheet(Theme::qss(QStringLiteral(R"(
            QToolButton {
                background: transparent;
                color: %1;
                border: 1px solid transparent;
                border-radius: @radius-control;
                font-size: 15px;
            }
            QToolButton:hover {
                background: %2;
                color: %3;
                border-color: %4;
            }
        )")).arg(Theme::textMuted(), Theme::hoverOverlay(), Theme::textBright(), Theme::subtleBorder()));
    }
    if (m_styleBtn) {
        m_styleBtn->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/layout.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()), QSize(16, 16)));
    }
    if (m_createChapterBtn) {
        m_createChapterBtn->setStyleSheet(createButtonQss(Theme::accentDefault()));
        m_createChapterBtn->setIcon(QIcon(circlePlusPixmap(QColor(Theme::accentDefault()), 18)));
    }
    if (m_rail) {
        // Mesma cor da gaveta; só a linha separa o trilho da lista.
        m_rail->setStyleSheet(QStringLiteral("#msRail { background: transparent; border-right: 1px solid %1; }")
            .arg(Theme::subtleBorder()));
    }
    for (QWidget* h : { m_resizeHandleBottom, m_resizeHandleCorner }) {
        if (h) h->setStyleSheet(QStringLiteral("#msResizeHandle { background: transparent; }"
                                               "#msResizeHandle:hover { background: %1; }").arg(Theme::subtleBorder()));
    }
    if (m_resizeHandle) {
        m_resizeHandle->setStyleSheet(QStringLiteral("#msResizeHandle { background: transparent; }"
                                                     "#msResizeHandle:hover { background: %1; }").arg(Theme::subtleBorder()));
    }
    if (m_hoverCard) {
        m_hoverCard->setStyleSheet(Theme::qss(QStringLiteral(
            "QLabel { background-color: %1; color: %2; border: 1px solid %3;"
            " border-radius: @radius-panel; padding: 10px 12px; font-size: 12px; }")
            .arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder())));
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
        const QPixmap cover = CoverUtils::pixmapFromDataUrl(m_model->manuscriptEffectiveCoverDataUrl(m.id));
        const QIcon icon = cover.isNull() ? QIcon()
            : QIcon(cover.scaled(kComboThumbW, kComboThumbH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_combo->addItem(icon, label, m.id);
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
    // Filtros são do livro que estava aberto.
    m_statusFilter.clear();
    m_povFilter.clear();
    m_povReading = 0;
    const bool animate = isVisible();
    if (animate) PanelMotion::swapOut(m_scroll->viewport());
    rebuildList();
    if (animate) playIntro(60);
}

// ============================================================ estilo e ferramentas

QString ManuscriptPanel::styleId(Style s) {
    switch (s) {
    case Style::Classic:  return QStringLiteral("classic");
    case Style::Rail:     return QStringLiteral("rail");
    case Style::Spines:   return QStringLiteral("spines");
    case Style::Showcase: return QStringLiteral("showcase");
    case Style::Toc:      return QStringLiteral("toc");
    case Style::Spine:    return QStringLiteral("spine");
    case Style::Grid:     return QStringLiteral("grid");
    case Style::Mosaic:   return QStringLiteral("mosaic");
    case Style::Journey:  return QStringLiteral("journey");
    case Style::TitlePage:   return QStringLiteral("titlepage");
    case Style::Reader:      return QStringLiteral("reader");
    case Style::Illustrated: return QStringLiteral("illustrated");
    case Style::Seasons:     return QStringLiteral("seasons");
    case Style::Store:       return QStringLiteral("store");
    case Style::Box:         return QStringLiteral("box");
    }
    return QStringLiteral("rail");
}

ManuscriptPanel::Style ManuscriptPanel::styleFromId(const QString& id) {
    for (Style s : { Style::Classic, Style::Rail, Style::Spines, Style::Showcase, Style::Toc,
                     Style::Spine, Style::Grid, Style::Mosaic, Style::Journey, Style::TitlePage,
                     Style::Reader, Style::Illustrated, Style::Seasons, Style::Store, Style::Box })
        if (styleId(s) == id) return s;
    return Style::Rail;   // padrão da gaveta
}

QString ManuscriptPanel::toolId(Tool t) {
    switch (t) {
    case ToolStatus:     return QStringLiteral("status");
    case ToolHover:      return QStringLiteral("hover");
    case ToolParts:      return QStringLiteral("parts");
    case ToolResume:     return QStringLiteral("resume");
    case ToolStory:      return QStringLiteral("storyOrder");
    case ToolPov:        return QStringLiteral("pov");
    case ToolVariations: return QStringLiteral("variations");
    case ToolRhythm:     return QStringLiteral("rhythm");
    case ToolCount:      break;
    }
    return QString();
}

void ManuscriptPanel::loadSettings() {
    QSettings s;
    m_style = styleFromId(s.value(QString::fromLatin1(kKeyStyle)).toString());
    // Status e POV nascem desligados: é informação que só aparece pra quem pede.
    const bool defaults[ToolCount] = { false, true, true, true, false, false, true, false };
    for (int i = 0; i < ToolCount; ++i)
        m_tools[i] = s.value(QStringLiteral("ui/manuscriptPanel/tool/") + toolId(Tool(i)), defaults[i]).toBool();
    const QStringList collapsed = s.value(QString::fromLatin1(kKeyCollapsed)).toStringList();
    m_collapsedParts = QSet<QString>(collapsed.begin(), collapsed.end());
}

void ManuscriptPanel::setStyle(Style s) {
    if (m_style == s) return;
    const bool animate = isVisible();
    if (animate) PanelMotion::swapOut(m_scroll->viewport());
    m_style = s;
    QSettings().setValue(QString::fromLatin1(kKeyStyle), styleId(s));
    applyStyleWidth();
    rebuildList();
    if (animate) playIntro(60);
}

void ManuscriptPanel::playIntro(int delayMs) {
    QList<QWidget*> rows;
    auto take = [&rows](QLayout* lay) {
        if (!lay) return;
        for (int i = 0; i < lay->count(); ++i)
            if (QWidget* w = lay->itemAt(i)->widget()) if (w->isVisible()) rows << w;
    };
    if (m_top && m_top->isVisible()) take(m_topLayout);
    take(m_listLayout);
    PanelMotion::cascade(rows, delayMs);
}

void ManuscriptPanel::setTool(Tool t, bool on) {
    m_tools[t] = on;
    QSettings().setValue(QStringLiteral("ui/manuscriptPanel/tool/") + toolId(t), on);
    if (!on) {
        if (t == ToolStatus) m_statusFilter.clear();
        if (t == ToolPov) { m_povFilter.clear(); m_povReading = 0; }
        if (t == ToolStory) m_storyMode = false;
        if (t == ToolHover) disarmHover();
    }
    rebuildList();
}

void ManuscriptPanel::applyStyleWidth() {
    int w = kPanelWidth;
    switch (m_style) {
    case Style::Classic:  w = kPanelWidth; break;
    case Style::Rail:     w = 310; break;
    case Style::Spines:   w = 290; break;
    case Style::Showcase: w = 300; break;
    case Style::Toc:      w = 290; break;
    case Style::Spine:    w = 280; break;
    case Style::Grid:     w = 290; break;
    case Style::Mosaic:   w = 290; break;
    case Style::Journey:  w = 300; break;
    case Style::TitlePage:   w = 300; break;
    case Style::Reader:      w = 300; break;
    case Style::Illustrated: w = 330; break;
    case Style::Seasons:     w = 340; break;
    case Style::Store:       w = 320; break;
    case Style::Box:         w = 300; break;
    }
    // Largura arrastada pelo usuário vale por estilo.
    const int stored = QSettings().value(QStringLiteral("ui/manuscriptPanel/width-") + styleId(m_style), 0).toInt();
    if (stored > 0) w = qBound(240, stored, 700);
    if (width() != w) {
        setFixedWidth(w);
        emit widthChanged();
    }
}

void ManuscriptPanel::showStyleMenu() {
    QMenu menu(this);
    menu.setStyleSheet(contextMenuQss());
    menu.setToolTipsVisible(true);
    auto addHeading = [&menu](const QString& text) {
        QAction* a = menu.addAction(text.toUpper());
        a->setEnabled(false);
        QFont f = a->font();
        f.setPointSizeF(7.5);
        f.setBold(true);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
        a->setFont(f);
    };
    addHeading(tr("Estilo"));
    auto* group = new QActionGroup(&menu);
    group->setExclusive(true);
    struct Opt { Style s; QString name; QString tip; };
    const QList<Opt> styles = {
        { Style::Classic,  tr("Clássico"),       tr("Lista de capítulos com o seletor de manuscrito em cima") },
        { Style::Rail,     tr("Trilho"),         tr("As capas dos livros num trilho; o aberto sobe pro topo") },
        { Style::Spines,   tr("Lombadas"),       tr("A saga como livros numa prateleira") },
        { Style::Showcase, tr("Vitrine"),        tr("A prateleira com as capas de frente") },
        { Style::Toc,      tr("Sumário"),        tr("Como o sumário de um livro impresso") },
        { Style::Spine,    tr("Espinha"),        tr("Capítulos e cenas como estações numa linha") },
        { Style::Grid,     tr("Grade de cenas"), tr("Uma fileira por capítulo, um bloco por cena") },
        { Style::Mosaic,   tr("Mosaico"),        tr("Todos os capítulos numa grade numerada, pra livro longo") },
        { Style::Journey,  tr("Jornada"),        tr("O livro como um caminho, com as partes como regiões") },
        { Style::TitlePage,   tr("Folha de rosto"),    tr("A folha de rosto do livro, com o sumário e o número de página") },
        { Style::Reader,      tr("Leitor"),            tr("Como a biblioteca de um e-reader: quanto de cada capítulo você já revisou") },
        { Style::Illustrated, tr("Índice ilustrado"),  tr("Cada capítulo com uma vinheta só dele, que cresce junto com o texto") },
        { Style::Seasons,     tr("Temporadas"),        tr("O livro como série: cada livro é uma temporada, cada capítulo um episódio") },
        { Style::Store,       tr("Página da loja"),    tr("O livro como na página de uma livraria: capa, números, sinopse e sumário") },
        { Style::Box,         tr("Box da saga"),       tr("Os livros da série num estojo; o aberto sai puxado pra fora") },
    };
    for (const Opt& o : styles) {
        QAction* a = menu.addAction(o.name);
        a->setCheckable(true);
        a->setChecked(o.s == m_style);
        a->setToolTip(o.tip);
        group->addAction(a);
        const Style s = o.s;
        connect(a, &QAction::triggered, this, [this, s]() { setStyle(s); });
    }
    menu.addSeparator();
    addHeading(tr("Ferramentas"));
    struct TOpt { Tool t; QString name; QString tip; };
    const QList<TOpt> tools = {
        { ToolResume,     tr("Onde parei"),         tr("O último lugar em que você escreveu, com a última frase") },
        { ToolHover,      tr("Ficha no hover"),     tr("Resumo, quando, POV, palavras e quem aparece, parando o mouse no capítulo") },
        { ToolParts,      tr("Partes"),             tr("Agrupar capítulos em partes ou atos") },
        { ToolVariations, tr("Variações à vista"),  tr("Mostra quantas versões cada cena tem e deixa trocar a ativa") },
        { ToolRhythm,     tr("Ritmo"),              tr("Gráfico do tamanho dos capítulos, com a parte de diálogo") },
        { ToolStory,      tr("Ordem da história"),  tr("Alterna entre a ordem de leitura e a ordem em que as coisas acontecem") },
        { ToolPov,        tr("POV à vista"),        tr("O narrador de cada capítulo e a linha de cada um") },
        { ToolStatus,     tr("Status de produção"), tr("Rascunho, revisado ou final, com progresso e filtro") },
    };
    for (const TOpt& o : tools) {
        QAction* a = menu.addAction(o.name);
        a->setCheckable(true);
        a->setChecked(tool(o.t));
        a->setToolTip(o.tip);
        const Tool t = o.t;
        connect(a, &QAction::toggled, this, [this, t](bool on) { setTool(t, on); });
    }
    PanelMotion::animateMenu(&menu);
    menu.exec(m_styleBtn->mapToGlobal(QPoint(0, m_styleBtn->height() + 2)));
}

// ============================================================ dados

QList<Chapter> ManuscriptPanel::readingChapters() const {
    QList<Chapter> out;
    if (!m_model) return out;
    const QString msId = activeManuscriptId();
    for (const auto& c : m_model->chapters())
        if (c.manuscriptId == msId) out.append(c);
    std::sort(out.begin(), out.end(), [](const Chapter& a, const Chapter& b) { return a.order < b.order; });
    return out;
}

bool ManuscriptPanel::storyMode() const { return tool(ToolStory) && m_storyMode; }

QList<Chapter> ManuscriptPanel::displayChapters() const {
    QList<Chapter> chs = readingChapters();
    if (storyMode()) {
        const StoryOrder so = storyOrder(chs);
        if (so.marked >= 2) {
            QList<Chapter> sorted;
            for (int i : so.byStory) sorted.append(chs.at(i));
            chs = sorted;
        }
    }
    if (statusVisible() && !m_statusFilter.isEmpty()) {
        QList<Chapter> f;
        for (const auto& c : chs) {
            const QString st = c.status.isEmpty() ? QStringLiteral("none") : c.status;
            if (st == m_statusFilter) f.append(c);
        }
        chs = f;
    }
    return chs;
}

QString ManuscriptPanel::chapterRowText(const Chapter& c) const {
    // Com título: só o número, sem a palavra do tipo ("3 - A Batalha"), pra
    // todos os tipos — vazio quando o capítulo não é numerado (tipo especial
    // sozinho no manuscrito), aí é só o título puro. Sem título: cai no
    // rótulo completo de sempre ("Capítulo 3", "Prólogo").
    if (c.title.isEmpty()) return m_model->chapterDisplayLabel(c);
    const QString numLabel = m_model->chapterNumberLabel(c);
    return numLabel.isEmpty() ? c.title : QStringLiteral("%1 - %2").arg(numLabel, c.title);
}

QString ManuscriptPanel::chapterShortLabel(const Chapter& c) const {
    const QString n = m_model->chapterNumberLabel(c);
    if (c.type == QStringLiteral("chapter") && !n.isEmpty()) return n;
    const QString t = m_model->chapterTypeName(c);
    return t.isEmpty() ? n : t.left(1).toUpper();
}

QString ManuscriptPanel::chapterTitleOnly(const Chapter& c) const {
    if (!c.title.isEmpty()) return c.title;
    return m_model->chapterDisplayLabel(c);
}

QString ManuscriptPanel::sceneTitle(const Chapter& c, int idx) const {
    if (idx < 0 || idx >= c.scenes.size()) return QString();
    const QString t = c.scenes.at(idx).title;
    return t.isEmpty() ? tr("Cena %1").arg(idx + 1) : t;
}

int ManuscriptPanel::chapterWords(const QString& chapterId) const {
    return m_wordCounter ? m_wordCounter->countChapter(chapterId) : 0;
}

int ManuscriptPanel::sceneWords(const QString& chapterId, int idx) const {
    return m_wordCounter ? m_wordCounter->countScene(chapterId, idx) : 0;
}

double ManuscriptPanel::dialogueFraction(const QString& chapterId) const {
    if (!m_dialogueStore || !m_wordCounter) return 0.0;
    const int total = m_wordCounter->countChapter(chapterId);
    if (total <= 0) return 0.0;
    return qBound(0.0, double(m_dialogueStore->dialogueWordsForChapter(chapterId)) / total, 1.0);
}

bool ManuscriptPanel::statusVisible() const { return tool(ToolStatus); }

QColor ManuscriptPanel::statusColor(const Chapter& c) const {
    // Sem a ferramenta de Status, os estilos desenhados usam a cor de destaque:
    // quem nunca pediu status não vê cor de status em lugar nenhum.
    if (!statusVisible()) return tcol(Theme::accentDefault());
    if (c.status.isEmpty()) return tcol(Theme::textMuted());
    return QColor(ProjectModel::findWorkStatusColor(c.status));
}

bool ManuscriptPanel::isCurrent(const QString& chapterId, int sceneIdx) const {
    if (chapterId != m_curChapterId || m_curChapterId.isEmpty()) return false;
    return sceneIdx == m_curScene;
}

// ---- Partes ----

bool ManuscriptPanel::partsActive() const { return tool(ToolParts) && !storyMode(); }

QList<ManuscriptPart> ManuscriptPanel::validParts(const QList<Chapter>& chs) const {
    QList<ManuscriptPart> out;
    const Manuscript* m = m_model ? m_model->findManuscript(activeManuscriptId()) : nullptr;
    if (!m) return out;
    QHash<QString, int> pos;
    for (int i = 0; i < chs.size(); ++i) pos.insert(chs.at(i).id, i);
    for (const auto& p : m->parts)
        if (pos.contains(p.startChapterId)) out.append(p);
    std::sort(out.begin(), out.end(), [&pos](const ManuscriptPart& a, const ManuscriptPart& b) {
        return pos.value(a.startChapterId) < pos.value(b.startChapterId);
    });
    return out;
}

QHash<QString, int> ManuscriptPanel::partIndexByChapter(const QList<Chapter>& chs,
                                                        const QList<ManuscriptPart>& parts) const {
    QHash<QString, int> starts;
    for (int i = 0; i < parts.size(); ++i) starts.insert(parts.at(i).startChapterId, i);
    QHash<QString, int> out;
    int cur = -1;
    for (const auto& c : chs) {
        if (starts.contains(c.id)) cur = starts.value(c.id);
        out.insert(c.id, cur);
    }
    return out;
}

void ManuscriptPanel::setParts(const QList<ManuscriptPart>& parts) {
    if (m_model) m_model->setManuscriptParts(activeManuscriptId(), parts);
}

void ManuscriptPanel::startPartAt(const QString& chapterId) {
    const Manuscript* m = m_model ? m_model->findManuscript(activeManuscriptId()) : nullptr;
    if (!m) return;
    QList<ManuscriptPart> parts = m->parts;
    for (const auto& p : parts) if (p.startChapterId == chapterId) return;
    bool ok = false;
    const QString title = QInputDialog::getText(this, tr("Nova parte"), tr("Nome da parte:"),
        QLineEdit::Normal, tr("Parte %1").arg(ProjectModel::toRomanNumeral(parts.size() + 1)), &ok).trimmed();
    if (!ok || title.isEmpty()) return;
    ManuscriptPart p;
    p.id = ProjectModel::uid();
    p.title = title;
    p.color = QString::fromLatin1(kPartColors[parts.size() % (sizeof(kPartColors) / sizeof(kPartColors[0]))]);
    p.startChapterId = chapterId;
    parts.append(p);
    setParts(parts);
}

void ManuscriptPanel::removePartStart(const QString& chapterId) {
    const Manuscript* m = m_model ? m_model->findManuscript(activeManuscriptId()) : nullptr;
    if (!m) return;
    QList<ManuscriptPart> parts = m->parts;
    parts.erase(std::remove_if(parts.begin(), parts.end(),
        [&](const ManuscriptPart& p) { return p.startChapterId == chapterId; }), parts.end());
    setParts(parts);
}

void ManuscriptPanel::askNewPart(const QPoint& globalPos) {
    const QList<Chapter> chs = readingChapters();
    const QList<ManuscriptPart> parts = validParts(chs);
    QSet<QString> starts;
    for (const auto& p : parts) starts.insert(p.startChapterId);
    QMenu menu(this);
    menu.setStyleSheet(contextMenuQss());
    QAction* head = menu.addAction(tr("Começa no capítulo:"));
    head->setEnabled(false);
    for (const auto& c : chs) {
        if (starts.contains(c.id)) continue;
        const QString id = c.id;
        connect(menu.addAction(chapterRowText(c)), &QAction::triggered, this, [this, id]() { startPartAt(id); });
    }
    PanelMotion::animateMenu(&menu);
    menu.exec(globalPos);
}

void ManuscriptPanel::cycleStatus(const QString& chapterId) {
    const Chapter* c = m_model ? m_model->findChapter(chapterId) : nullptr;
    if (!c) return;
    const QStringList cycle = { QString(), QStringLiteral("draft"), QStringLiteral("revised"), QStringLiteral("final") };
    const int i = cycle.indexOf(c->status);
    m_model->updateChapterStatus(chapterId, cycle.at((i + 1) % cycle.size()));
}

// ---- Ordem da história ----

ManuscriptPanel::StoryOrder ManuscriptPanel::storyOrder(const QList<Chapter>& chs) const {
    StoryOrder so;
    const int n = chs.size();
    QVector<double> key(n);
    double last = -1e300;
    for (int i = 0; i < n; ++i) {
        bool ok = false;
        const double v = chs.at(i).timeMarker.trimmed().isEmpty()
            ? 0.0 : TimelineChrono::parse(chs.at(i).timeMarker, &ok);
        if (ok) { key[i] = v; last = v; ++so.marked; }
        else key[i] = last;   // sem marcador: acompanha o anterior da leitura
    }
    so.byStory.resize(n);
    for (int i = 0; i < n; ++i) so.byStory[i] = i;
    std::stable_sort(so.byStory.begin(), so.byStory.end(), [&key](int a, int b) { return key[a] < key[b]; });
    so.storyPos.resize(n);
    for (int j = 0; j < n; ++j) so.storyPos[so.byStory[j]] = j;
    // Maior subsequência crescente da leitura ao longo da história: quem fica
    // fora dela é salto de verdade (flashback), não quem só andou duas casas.
    QVector<int> L(n, 1), prv(n, -1);
    for (int a = 0; a < n; ++a)
        for (int b = 0; b < a; ++b)
            if (so.byStory[b] < so.byStory[a] && L[b] + 1 > L[a]) { L[a] = L[b] + 1; prv[a] = b; }
    int best = 0;
    for (int a = 1; a < n; ++a) if (L[a] > L[best]) best = a;
    QSet<int> keep;
    for (int e = (n ? best : -1); e >= 0; e = prv[e]) keep.insert(so.byStory[e]);
    so.jump.resize(n);
    for (int i = 0; i < n; ++i) so.jump[i] = !keep.contains(i);
    return so;
}

// ---- POV ----

QString ManuscriptPanel::povOf(const Chapter& c) const {
    if (c.pov.isEmpty() || !m_elementsStore || !m_elementsStore->findElement(c.pov)) return QString();
    return c.pov;
}

QString ManuscriptPanel::suggestedPov(const Chapter& c) const {
    if (!m_elementsStore) return QString();
    QString narrator;
    for (const auto& e : m_elementsStore->elements())
        if (e.narrator && e.type == QStringLiteral("character")) { narrator = e.id; break; }
    if (!narrator.isEmpty() && !c.povOther) return narrator;
    const QStringList present = m_elementsStore->docElementIds(
        ElementsStore::elementDocKeyForChapter(c.manuscriptId, c.id));
    for (const QString& id : present) {
        const Element* e = m_elementsStore->findElement(id);
        if (e && e->type == QStringLiteral("character") && id != narrator) return id;
    }
    return QString();
}

QStringList ManuscriptPanel::narratorsInOrder(const QList<Chapter>& chs) const {
    QStringList out;
    for (const auto& c : chs) {
        const QString p = povOf(c);
        if (!p.isEmpty() && !out.contains(p)) out.append(p);
    }
    return out;
}

QColor ManuscriptPanel::povColor(const QString& elementId, const QStringList& order) const {
    const int i = order.indexOf(elementId);
    if (i < 0) return tcol(Theme::panelBorder());
    return povPalette().at(i % povPalette().size());
}

QString ManuscriptPanel::elementName(const QString& elementId) const {
    const Element* e = m_elementsStore ? m_elementsStore->findElement(elementId) : nullptr;
    return e ? e->name : QString();
}

QPixmap ManuscriptPanel::elementAvatar(const QString& elementId, const QColor& color, int size) const {
    const Element* e = m_elementsStore ? m_elementsStore->findElement(elementId) : nullptr;
    const QPixmap photo = (e && !e->image.isEmpty()) ? CoverUtils::pixmapFromDataUrl(e->image) : QPixmap();
    return MsPaint::avatar(photo, e ? e->name : QString(), color, size, devicePixelRatioF());
}

bool ManuscriptPanel::povBarVisible(const QList<Chapter>& chs) const {
    // Livro de um narrador só não tem nada a mostrar.
    return tool(ToolPov) && narratorsInOrder(chs).size() >= 2;
}

// ============================================================ montagem

void ManuscriptPanel::clearLayout(QLayout* lay) {
    if (!lay) return;
    while (lay->count() > 0) {
        QLayoutItem* it = lay->takeAt(0);
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        else if (QLayout* l = it->layout()) clearLayout(l);
        delete it;
    }
}

void ManuscriptPanel::rebuildList() {
    if (!m_listLayout) return;
    disarmHover();
    clearDropIndicator();
    loadBookColors();
    rebuildHeader();
    rebuildRail();
    rebuildTop();
    rebuildBody();
    rebuildBottom();
}

void ManuscriptPanel::rebuildHeader() {
    const bool rail = (m_style == Style::Rail);
    m_headerTitle->setVisible(rail);
    m_combo->setVisible(!rail);
    m_addMsBtn->setVisible(!rail);
    const bool bigButton = (m_style == Style::Classic || m_style == Style::Spine
                            || m_style == Style::Illustrated);
    m_actionBar->setVisible(bigButton && m_model && !activeManuscriptId().isEmpty());
}

namespace {
QPixmap coverPixmap(ProjectModel* model, const Manuscript& m, int number, QSize size, qreal dpr,
                    const QString& bookWord) {
    const QPixmap real = CoverUtils::pixmapFromDataUrl(model->manuscriptEffectiveCoverDataUrl(m.id));
    if (real.isNull())
        return MsPaint::generatedCover(m.title, number, MsPaint::bookColor(m.id), size, dpr, bookWord);
    QPixmap out(size * dpr);
    out.setDevicePixelRatio(dpr);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, size.width(), size.height()), 2, 2);
    p.setClipPath(clip);
    QPixmap sc = real.scaled(size * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    sc.setDevicePixelRatio(dpr);
    p.drawPixmap(QPointF((size.width() - sc.width() / dpr) / 2.0, (size.height() - sc.height() / dpr) / 2.0), sc);
    return out;
}
}

void ManuscriptPanel::rebuildRail() {
    clearLayout(m_railLayout);
    const bool rail = (m_style == Style::Rail);
    m_rail->setVisible(rail);
    if (!rail || !m_model) return;

    const QString btnQss = Theme::qss(QStringLiteral(
        "QToolButton { background: transparent; color: %1; border: 1px solid transparent;"
        " border-radius: @radius-control; font-size: 15px; }"
        "QToolButton:hover { background: %2; color: %3; border-color: %4; }"))
        .arg(Theme::textMuted(), Theme::hoverOverlay(), Theme::textBright(), Theme::subtleBorder());
    auto railBtn = [&](const QString& text, const QString& tip) {
        auto* b = new QToolButton(m_rail);
        b->setText(text);
        b->setToolTip(tip);
        b->setFixedSize(34, 34);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(btnQss);
        m_railLayout->addWidget(b, 0, Qt::AlignHCenter);
        return b;
    };

    // ✦ no topo, em cima das capas.
    connect(railBtn(QStringLiteral("✦"), tr("Novo manuscrito")), &QToolButton::clicked,
            this, &ManuscriptPanel::newManuscriptRequested);

    // Picker real: o livro aberto SAI do trilho (sobe pro topo da coluna); os
    // outros ficam na ordem da saga, cada um com o seu número.
    const QString active = activeManuscriptId();
    const auto& mss = m_model->manuscripts();
    const qreal dpr = devicePixelRatioF();
    for (int i = 0; i < mss.size(); ++i) {
        const Manuscript& m = mss.at(i);
        if (m.id == active) continue;
        QPixmap pm = coverPixmap(m_model, m, i + 1, QSize(30, 42), dpr, tr("Livro"));
        if (!CoverUtils::pixmapFromDataUrl(m_model->manuscriptEffectiveCoverDataUrl(m.id)).isNull()) {
            // número no canto (capa gerada já traz o número grande)
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            const QRectF badge(0, 30, 13, 12);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 160));
            p.drawRoundedRect(badge, 2, 2);
            p.setPen(QColor(255, 255, 255, 230));
            p.setFont(uiFont(8, QFont::Bold));
            p.drawText(badge, Qt::AlignCenter, QString::number(i + 1));
        }
        auto* b = new QToolButton(m_rail);
        b->setIcon(QIcon(pm));
        b->setIconSize(QSize(30, 42));
        b->setFixedSize(36, 48);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(m.title.isEmpty() ? tr("(sem título)") : m.title);
        b->setStyleSheet(btnQss);
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString id = m.id;
        connect(b, &QToolButton::clicked, this, [this, id]() {
            const int idx = m_combo->findData(id);
            if (idx >= 0) m_combo->setCurrentIndex(idx);
        });
        connect(b, &QToolButton::customContextMenuRequested, this, [this, b, id](const QPoint& pos) {
            showManuscriptContextMenu(id, b->mapToGlobal(pos));
        });
        m_railLayout->addWidget(b, 0, Qt::AlignHCenter);
    }
    // Como no concept: as ações do livro aberto vêm logo depois das capas,
    // separadas por um fio — não lá no pé do trilho.
    auto* sep = new QWidget(m_rail);
    sep->setFixedSize(22, 1);
    sep->setStyleSheet(QStringLiteral("background: %1;").arg(Theme::subtleBorder()));
    m_railLayout->addWidget(sep, 0, Qt::AlignHCenter);
    if (active.isEmpty()) { m_railLayout->addStretch(1); return; }

    // Ações do livro aberto, embaixo.
    auto* add = railBtn(QString(), tr("Novo capítulo"));
    add->setIcon(QIcon(circlePlusPixmap(QColor(Theme::accentDefault()), 20)));
    add->setIconSize(QSize(20, 20));
    connect(add, &QToolButton::clicked, this, [this]() { emit newChapterRequested(activeManuscriptId()); });
    auto* reader = railBtn(QString(), tr("Visualizar como e-reader"));
    reader->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/ereader.svg"),
        QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()), QSize(18, 18)));
    connect(reader, &QToolButton::clicked, this, [this]() { emit previewEreaderRequested(activeManuscriptId()); });
    auto* stats = railBtn(QString(), tr("Estatísticas"));
    stats->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/stats-chart.svg"),
        QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()), QSize(16, 16)));
    connect(stats, &QToolButton::clicked, this, [this]() { emit statsRequested(activeManuscriptId()); });
    m_railLayout->addStretch(1);
}

QWidget* ManuscriptPanel::makeBookBlock(bool withCover) {
    const QString msId = activeManuscriptId();
    const Manuscript* m = m_model ? m_model->findManuscript(msId) : nullptr;
    if (!m) return nullptr;
    const auto& mss = m_model->manuscripts();
    int number = 1;
    for (int i = 0; i < mss.size(); ++i) if (mss.at(i).id == msId) number = i + 1;

    auto* w = new QWidget(m_top);
    w->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(w, &QWidget::customContextMenuRequested, this, [this, w, msId](const QPoint& pos) {
        showManuscriptContextMenu(msId, w->mapToGlobal(pos));
    });
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(12);
    if (withCover) {
        auto* cov = new QLabel(w);
        cov->setPixmap(coverPixmap(m_model, *m, number, QSize(42, 60), devicePixelRatioF(), tr("Livro")));
        cov->setFixedSize(42, 60);
        cov->setToolTip(tr("Capa"));
        lay->addWidget(cov, 0, Qt::AlignTop);
    }
    auto* txt = new QVBoxLayout;
    txt->setSpacing(3);
    auto* title = new QLabel(m->title.isEmpty() ? tr("(sem título)") : m->title, w);
    title->setWordWrap(true);
    title->setFont(serifFont(16, QFont::DemiBold));
    title->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textBright()));
    txt->addWidget(title);
    const int chCount = readingChapters().size();
    const int words = m_wordCounter ? m_wordCounter->countManuscript(msId) : 0;
    txt->addWidget(mutedLabel(tr("livro %1 de %2 · %3 capítulos · %4 palavras")
        .arg(number).arg(mss.size()).arg(chCount).arg(MsPaint::fmtInt(words)), w, 11));
    txt->addStretch(1);
    lay->addLayout(txt, 1);
    return w;
}

void ManuscriptPanel::rebuildTop() {
    clearLayout(m_topLayout);
    m_rhythmTip = nullptr;
    if (!m_model) { m_top->hide(); return; }
    const QList<Chapter> chs = readingChapters();

    if (m_style == Style::Rail) {
        if (QWidget* b = makeBookBlock(true)) m_topLayout->addWidget(b);
    } else if (m_style == Style::Spines || m_style == Style::Showcase) {
        auto* shelf = new MsShelf(m_style == Style::Spines ? MsShelf::Spines : MsShelf::Covers);
        shelf->addLabel = tr("Novo manuscrito");
        QList<MsBook> books;
        const auto& mss = m_model->manuscripts();
        for (int i = 0; i < mss.size(); ++i) {
            MsBook b;
            b.id = mss.at(i).id;
            b.title = mss.at(i).title.isEmpty() ? tr("(sem título)") : mss.at(i).title;
            b.number = i + 1;
            b.words = m_wordCounter ? m_wordCounter->countManuscript(b.id) : 0;
            b.color = MsPaint::bookColor(b.id);
            b.cover = CoverUtils::pixmapFromDataUrl(m_model->manuscriptEffectiveCoverDataUrl(b.id));
            books.append(b);
        }
        shelf->setBooks(books, activeManuscriptId());
        connect(shelf, &MsShelf::bookClicked, this, [this](const QString& id) {
            const int idx = m_combo->findData(id);
            if (idx >= 0) m_combo->setCurrentIndex(idx);
        });
        connect(shelf, &MsShelf::addClicked, this, &ManuscriptPanel::newManuscriptRequested);
        connect(shelf, &MsShelf::bookContextRequested, this, [this](const QString& id, const QPoint& pos) {
            showManuscriptContextMenu(id, pos);
        });
        // Com muitos livros a prateleira rola de lado.
        auto* sc = new QScrollArea(m_top);
        sc->setFrameShape(QFrame::NoFrame);
        sc->setWidget(shelf);
        sc->setWidgetResizable(false);
        sc->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        sc->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        sc->setFixedHeight(shelf->height() + 10);
        sc->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
        sc->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
        shelf->resize(shelf->sizeHint());
        m_topLayout->addWidget(sc);
        if (QWidget* b = makeBookBlock(false)) m_topLayout->addWidget(b);
    } else if (m_style == Style::Reader) {
        if (QWidget* w = makeReaderTop()) m_topLayout->addWidget(w);
    } else if (m_style == Style::Seasons) {
        if (QWidget* w = makeSeasonsTop()) m_topLayout->addWidget(w);
    } else if (m_style == Style::Box) {
        if (QWidget* w = makeBoxTop()) m_topLayout->addWidget(w);
    }

    m_top->setVisible(m_topLayout->count() > 0);
}

// ---- ferramentas do topo ----

QWidget* ManuscriptPanel::makeResumeCard() {
    const QString msId = activeManuscriptId();
    QList<ResumeEntry> mine;
    for (const auto& e : m_resume)
        if (e.manuscriptId == msId && m_model->findChapter(e.chapterId)) mine.append(e);
    if (mine.isEmpty()) return nullptr;
    const ResumeEntry& e = mine.first();
    // Já está lá: o cartão só repetiria o que o editor mostra.
    if (m_curChapterId == e.chapterId && (m_curScene == -1 || m_curScene == e.sceneIndex)) return nullptr;

    auto where = [this](const ResumeEntry& r) {
        const Chapter* c = m_model->findChapter(r.chapterId);
        if (!c) return QString();
        QString s = QStringLiteral("%1 · %2").arg(chapterShortLabel(*c), chapterTitleOnly(*c));
        if (c->scenes.size() > 1 && r.sceneIndex >= 0) s += QStringLiteral(" — ") + sceneTitle(*c, r.sceneIndex);
        return s;
    };

    auto* card = new QFrame(m_top);
    card->setObjectName(QStringLiteral("msResume"));
    card->setStyleSheet(Theme::qss(QStringLiteral(
        "#msResume { background: %1; border: 1px solid %2; border-left: 3px solid %3;"
        " border-radius: @radius-control; }"))
        .arg(Theme::hoverOverlay(), Theme::subtleBorder(), Theme::accentDefault()));
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(12, 9, 10, 9);
    lay->setSpacing(3);
    lay->addWidget(capsLabel(tr("Onde você parou"), card));
    auto* w = new QLabel(where(e), card);
    w->setWordWrap(true);
    w->setFont(serifFont(14, QFont::DemiBold));
    w->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textBright()));
    lay->addWidget(w);
    if (!e.sentence.trimmed().isEmpty()) {
        QString q = e.sentence.simplified();
        if (q.size() > 140) q = QStringLiteral("…") + q.right(140);
        auto* ql = new QLabel(QStringLiteral("“%1”").arg(q), card);
        ql->setWordWrap(true);
        ql->setFont(serifFont(12.5, QFont::Normal, true));
        ql->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textPrimary()));
        lay->addWidget(ql);
    }
    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    row->addWidget(mutedLabel(relativeWhen(e.when), card, 11), 1);
    auto* go = linkButton(tr("Continuar ›"), card, Theme::accentDefault(), 12, true);
    const ResumeEntry ec = e;
    connect(go, &QToolButton::clicked, this, [this, ec]() {
        emit resumeRequested(ec.manuscriptId, ec.chapterId, ec.sceneIndex, ec.sentence, ec.position);
    });
    row->addWidget(go);
    lay->addLayout(row);
    for (int i = 1; i < mine.size() && i < 3; ++i) {
        const ResumeEntry r = mine.at(i);
        const QString full = tr("antes: %1 (%2)").arg(where(r), relativeWhen(r.when));
        auto* prev = linkButton(full, card, Theme::textMuted(), 11);
        // Texto longo não pode alargar a lista: vira reticências.
        prev->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        prev->setText(QFontMetrics(uiFont(11)).elidedText(full, Qt::ElideRight, width() - 90));
        prev->setToolTip(full);
        connect(prev, &QToolButton::clicked, this, [this, r]() {
            emit resumeRequested(r.manuscriptId, r.chapterId, r.sceneIndex, r.sentence, r.position);
        });
        lay->addWidget(prev);
    }
    return card;
}

QWidget* ManuscriptPanel::makeStatusBar(const QList<Chapter>& chs) {
    QHash<QString, int> cnt;
    for (const auto& c : chs) cnt[c.status.isEmpty() ? QStringLiteral("none") : c.status] += 1;
    // Discreto: enquanto ninguém definiu status, não há barra (só as bolinhas).
    if (cnt.value(QStringLiteral("none")) == chs.size()) return nullptr;

    auto* w = new QWidget(m_top);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(5);
    const int fin = cnt.value(QStringLiteral("final"));
    auto* head = new QHBoxLayout;
    head->addWidget(mutedLabel(tr("%1 de %2 finais").arg(fin).arg(chs.size()), w, 11), 1);
    head->addWidget(mutedLabel(QStringLiteral("%1%").arg(qRound(100.0 * fin / chs.size())), w, 11));
    lay->addLayout(head);
    auto* bar = new StackBar(w);
    const QList<WorkStatus> wss = ProjectModel::workStatuses();
    for (int i = wss.size() - 1; i >= 0; --i)   // final primeiro
        bar->parts.append({ QColor(wss.at(i).color), cnt.value(wss.at(i).id) });
    bar->parts.append({ tcol(Theme::panelBorder()), cnt.value(QStringLiteral("none")) });
    lay->addWidget(bar);
    auto* chips = new QGridLayout;
    chips->setHorizontalSpacing(4);
    chips->setVerticalSpacing(4);
    int chipN = 0;
    QList<QPair<QString, QString>> opts;
    for (const auto& ws : ProjectModel::workStatuses()) opts.append({ ws.id, ws.label });
    opts.append({ QStringLiteral("none"), tr("Sem status") });
    for (const auto& o : opts) {
        const int n = cnt.value(o.first);
        if (!n) continue;
        const QColor c = o.first == QStringLiteral("none") ? tcol(Theme::textMuted())
                                                           : QColor(ProjectModel::findWorkStatusColor(o.first));
        auto* chip = chipButton(QStringLiteral("%1 %2").arg(o.second).arg(n), w, m_statusFilter == o.first);
        chip->setIcon(QIcon(MsPaint::avatar(QPixmap(), QString(), c, 8, devicePixelRatioF())));
        chip->setIconSize(QSize(7, 7));
        chip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        chip->setToolTip(tr("Mostrar só estes"));
        const QString id = o.first;
        connect(chip, &QToolButton::clicked, this, [this, id]() {
            m_statusFilter = (m_statusFilter == id) ? QString() : id;
            rebuildList();
        });
        chip->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        chips->addWidget(chip, chipN / 2, chipN % 2);
        ++chipN;
    }
    lay->addLayout(chips);
    return w;
}

QWidget* ManuscriptPanel::makeStoryBar(const QList<Chapter>& chs) {
    const StoryOrder so = storyOrder(chs);
    auto* w = new QWidget(m_top);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);
    auto* seg = new QHBoxLayout;
    seg->setSpacing(4);
    auto* read = chipButton(tr("Leitura"), w, !m_storyMode);
    auto* story = chipButton(tr("História"), w, m_storyMode);
    read->setToolTip(tr("Na ordem em que o leitor lê"));
    story->setToolTip(tr("Na ordem em que as coisas acontecem (pelo \"quando se passa\")"));
    connect(read, &QToolButton::clicked, this, [this]() { m_storyMode = false; rebuildList(); });
    connect(story, &QToolButton::clicked, this, [this]() { m_storyMode = true; rebuildList(); });
    seg->addWidget(read);
    seg->addWidget(story);
    seg->addStretch(1);
    lay->addLayout(seg);
    if (so.marked < 2) {
        lay->addWidget(mutedLabel(tr("Preencha o \"quando se passa\" dos capítulos (Renomear capítulo) pra ver a ordem da história."), w, 11));
        return w;
    }
    int jumps = 0;
    for (bool j : so.jump) if (j) ++jumps;
    if (m_storyMode) {
        auto* cross = new MsCrossing(w);
        cross->setData(so.storyPos, so.jump, tr("Leitura"), tr("História"));
        lay->addWidget(cross);
    }
    lay->addWidget(mutedLabel(jumps == 0 ? tr("A leitura segue a ordem da história.")
        : tr("%n capítulo(s) fora da ordem da história", "", jumps), w, 11));
    return w;
}

QWidget* ManuscriptPanel::makePovBar(const QList<Chapter>& chs) {
    const QStringList order = narratorsInOrder(chs);
    if (order.isEmpty())
        return mutedLabel(tr("Defina o narrador de cada capítulo pelo botão direito (Narrador)."), m_top, 11);
    if (order.size() < 2) return nullptr;   // um narrador só: nada a mostrar

    auto* w = new QWidget(m_top);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);
    auto* strip = new MsPovStrip(w);
    QList<MsPovStrip::Seg> segs;
    QHash<QString, int> cnt;
    for (const auto& c : chs) {
        const QString p = povOf(c);
        if (!p.isEmpty()) cnt[p] += 1;
        MsPovStrip::Seg s;
        s.chapterId = c.id;
        s.color = povColor(p, order);
        s.dim = !m_povFilter.isEmpty() && p != m_povFilter;
        s.current = (c.id == m_curChapterId);
        s.tip = QStringLiteral("%1 · %2").arg(chapterRowText(c), p.isEmpty() ? tr("sem narrador") : elementName(p));
        segs.append(s);
    }
    strip->setSegments(segs);
    const QString msId = activeManuscriptId();
    connect(strip, &MsPovStrip::chapterClicked, this, [this, msId](const QString& id) { emit chapterActivated(msId, id); });
    lay->addWidget(strip);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(4);
    for (int i = 0; i < order.size(); ++i) {
        const QString id = order.at(i);
        auto* face = chipButton(QStringLiteral("%1  %2").arg(elementName(id)).arg(cnt.value(id)), w, m_povFilter == id);
        face->setIcon(QIcon(elementAvatar(id, povColor(id, order), 18)));
        face->setIconSize(QSize(18, 18));
        face->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        face->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        connect(face, &QToolButton::clicked, this, [this, id]() {
            m_povFilter = (m_povFilter == id) ? QString() : id;
            m_povReading = 0;
            rebuildList();
        });
        grid->addWidget(face, i / 2, i % 2);
    }
    lay->addLayout(grid);

    QString facts;
    if (m_povFilter.isEmpty()) {
        QString top = order.first();
        for (const QString& id : order) if (cnt.value(id) > cnt.value(top)) top = id;
        facts = tr("%1 narradores · <b>%2</b> narra %3 de %4")
            .arg(order.size()).arg(elementName(top).toHtmlEscaped()).arg(cnt.value(top)).arg(chs.size());
    } else {
        QStringList mine;
        int gap = 0, run = 0;
        for (const auto& c : chs) {
            if (povOf(c) == m_povFilter) { mine << chapterShortLabel(c); run = 0; }
            else { ++run; gap = qMax(gap, run); }
        }
        const QString name = elementName(m_povFilter).toHtmlEscaped();
        facts = tr("<b>%1</b> narra %2 de %3: %4").arg(name).arg(mine.size()).arg(chs.size()).arg(mine.join(QStringLiteral(", ")));
        if (gap > 0) facts += QStringLiteral("<br>") + tr("maior intervalo sem %1: %n capítulo(s)", "", gap).arg(name);
    }
    auto* fl = mutedLabel(facts, w, 11);
    fl->setTextFormat(Qt::RichText);
    lay->addWidget(fl);
    return w;
}

QWidget* ManuscriptPanel::makeRhythmBar(const QList<Chapter>& chs) {
    QList<MsChapterDot> dots;
    int total = 0, n = 0;
    for (const auto& c : chs) {
        MsChapterDot d;
        d.chapterId = c.id;
        d.shortLabel = chapterShortLabel(c);
        d.title = chapterRowText(c);
        d.words = chapterWords(c.id);
        d.dialogue = dialogueFraction(c.id);
        d.special = (c.type != QStringLiteral("chapter"));
        d.current = (c.id == m_curChapterId);
        dots.append(d);
        total += d.words;
        if (d.words) ++n;
    }
    auto* w = new QWidget(m_top);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    auto* head = new QHBoxLayout;
    head->addWidget(capsLabel(tr("Ritmo do livro"), w), 1);
    auto* hr = mutedLabel(tr("%1 palavras · média %2").arg(MsPaint::fmtInt(total))
        .arg(MsPaint::fmtInt(n ? total / n : 0)), w, 10.5);
    hr->setWordWrap(false);
    head->addWidget(hr);
    lay->addLayout(head);
    auto* chart = new MsRhythm(w);
    chart->setDots(dots);
    lay->addWidget(chart);
    m_rhythmTip = mutedLabel(QString(), w, 11);
    m_rhythmTip->setTextFormat(Qt::RichText);
    lay->addWidget(m_rhythmTip);
    auto tipFor = [this, dots](int i) {
        if (i < 0 || i >= dots.size()) {
            for (int k = 0; k < dots.size(); ++k) if (dots.at(k).current) { i = k; break; }
        }
        if (i < 0 || i >= dots.size()) return tr("Passe o mouse nas barras. Azul = diálogo.");
        const auto& d = dots.at(i);
        return tr("<b>%1</b> · %2 palavras · %3% diálogo").arg(d.title.toHtmlEscaped())
            .arg(MsPaint::fmtInt(d.words)).arg(qRound(d.dialogue * 100));
    };
    m_rhythmTip->setText(tipFor(-1));
    QLabel* tip = m_rhythmTip;
    connect(chart, &MsRhythm::hoverChanged, tip, [tip, tipFor](int i) { tip->setText(tipFor(i)); });
    const QString msId = activeManuscriptId();
    connect(chart, &MsRhythm::chapterClicked, this, [this, msId](const QString& id) { emit chapterActivated(msId, id); });
    return w;
}

// ============================================================ corpo

void ManuscriptPanel::wireRow(QWidget* w, const QString& kind, const QString& chapterId, int sceneIdx) {
    w->setProperty("kind", kind);
    if (!chapterId.isEmpty()) w->setProperty("chapterId", chapterId);
    if (sceneIdx >= 0) w->setProperty("sceneIndex", sceneIdx);
    if (!chapterId.isEmpty() && kind == QStringLiteral("chapter")) w->setProperty("hoverChapterId", chapterId);
    w->installEventFilter(this);
}

QWidget* ManuscriptPanel::wrapInPart(QWidget* row, const QColor& partColor) {
    return new RowWrap(row, partColor, this);
}

void ManuscriptPanel::rebuildBody() {
    while (m_listLayout->count() > 1) {
        QLayoutItem* item = m_listLayout->takeAt(0);
        if (auto* w = item->widget()) { w->hide(); w->deleteLater(); }
        delete item;
    }
    if (!m_model) return;
    const QString msId = activeManuscriptId();
    const QList<Chapter> reading = readingChapters();
    const bool newStyle = (m_style == Style::TitlePage || m_style == Style::Reader || m_style == Style::Illustrated
                           || m_style == Style::Seasons || m_style == Style::Store || m_style == Style::Box);
    const bool compactAdd = (m_style == Style::Spines || m_style == Style::Showcase || m_style == Style::Toc
                             || m_style == Style::Grid || m_style == Style::Mosaic || m_style == Style::Journey
                             || (newStyle && m_style != Style::Illustrated));
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };

    // Folha de rosto e Página da loja: a cabeça do livro rola junto com a lista.
    if (!msId.isEmpty() && m_style == Style::TitlePage) if (QWidget* h = makeTitlePageHead()) add(h);
    if (!msId.isEmpty() && m_style == Style::Store) if (QWidget* h = makeStoreHead()) add(h);

    if (reading.isEmpty()) {
        auto* lbl = new QLabel(msId.isEmpty()
            ? tr("Crie um manuscrito pra começar.")
            : tr("Nenhum capítulo ainda."), this);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setWordWrap(true);
        lbl->setStyleSheet(QStringLiteral(
            "color: %1; font-style: italic; padding: 20px 12px;").arg(Theme::textMuted()));
        add(lbl);
        if (!msId.isEmpty() && compactAdd) add(makeAddRow(tr("Novo capítulo"), QStringLiteral("addChapter")));
        return;
    }

    // Ferramentas no começo da lista: rolam junto com os capítulos, senão com
    // várias ligadas o topo espremeria a lista.
    {
        auto* tools = new QWidget(this);
        auto* tl = new QVBoxLayout(tools);
        tl->setContentsMargins(4, 2, 4, 10);
        tl->setSpacing(10);
        // Leitor e Temporadas já trazem o "Continuar" no topo.
        if (tool(ToolResume) && m_style != Style::Reader && m_style != Style::Seasons)
            if (QWidget* w = makeResumeCard()) tl->addWidget(w);
        if (statusVisible()) if (QWidget* w = makeStatusBar(reading)) tl->addWidget(w);
        if (tool(ToolStory)) if (QWidget* w = makeStoryBar(reading)) tl->addWidget(w);
        if (tool(ToolPov)) if (QWidget* w = makePovBar(reading)) tl->addWidget(w);
        if (tool(ToolRhythm) && reading.size() >= 2) if (QWidget* w = makeRhythmBar(reading)) tl->addWidget(w);
        if (tl->count() > 0) add(tools); else delete tools;
    }

    const QList<Chapter> chs = displayChapters();
    if (chs.isEmpty()) {
        add(mutedLabel(tr("Nenhum capítulo com esse status."), this));
    } else {
        switch (m_style) {
        case Style::Classic:
        case Style::Rail:
        case Style::Spines:
        case Style::Showcase: buildListView(chs); break;
        case Style::Toc:      buildTocView(chs); break;
        case Style::Spine:    buildSpineView(chs); break;
        case Style::Grid:     buildGridView(chs); break;
        case Style::Mosaic:   buildMosaicView(chs); break;
        case Style::Journey:  buildJourneyView(chs); break;
        case Style::TitlePage:   buildTitlePageView(chs); break;
        case Style::Reader:      buildReaderView(chs); break;
        case Style::Illustrated: buildIllustratedView(chs); break;
        case Style::Seasons:     buildSeasonsView(chs); break;
        case Style::Store:       buildStoreView(chs); break;
        case Style::Box:         buildBoxView(chs); break;
        }
    }
    if (partsActive() && m_style != Style::Mosaic && m_style != Style::Journey && !newStyle && reading.size() > 1)
        add(makeAddRow(tr("Nova parte"), QStringLiteral("addPart")));
    if (compactAdd) add(makeAddRow(tr("Novo capítulo"), QStringLiteral("addChapter")));
}

QWidget* ManuscriptPanel::makeAddRow(const QString& text, const QString& kind) {
    auto* row = new MsRow(this);
    row->setRowHeight(30);
    row->setToolTip(text);
    row->setPainter([text](QPainter& p, const QRect& r, const MsRow* self) {
        if (self->isHovered()) {
            p.setPen(Qt::NoPen);
            p.setBrush(tcol(Theme::hoverOverlay()));
            p.drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 4, 4);
        }
        p.setPen(self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textMuted()));
        p.setFont(uiFont(12));
        p.drawText(r.adjusted(12, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("+  ") + text);
    });
    row->setProperty("kind", kind);
    row->installEventFilter(this);
    connect(row, &QToolButton::clicked, this, [this, kind, row]() {
        if (kind == QStringLiteral("addChapter")) emit newChapterRequested(activeManuscriptId());
        else askNewPart(row->mapToGlobal(QPoint(0, row->height())));
    });
    return row;
}

QWidget* ManuscriptPanel::makePartHeader(const ManuscriptPart& part, int chapterCount, int words) {
    auto* row = new MsRow(this);
    const bool onSpine = (m_style == Style::Spine);
    row->setRowHeight(onSpine ? 34 : 30);
    const bool collapsed = m_collapsedParts.contains(part.id);
    const QColor color(part.color);
    const QString title = part.title;
    const QString count = tr("%n cap.", "", chapterCount) + QStringLiteral(" · ") + MsPaint::fmtInt(words);
    row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
        if (self->isHovered()) {
            p.setPen(Qt::NoPen);
            p.setBrush(tcol(Theme::hoverOverlay()));
            p.drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 4, 4);
        }
        int x = r.left() + 6;
        if (onSpine) {
            // losango na própria espinha
            QPen axis(tcol(Theme::panelBorder()), 2);
            p.setPen(axis);
            p.drawLine(QPointF(18, r.top()), QPointF(18, r.bottom() + 1));
            QPainterPath d;
            const QPointF c(18, r.center().y());
            d.moveTo(c + QPointF(0, -6)); d.lineTo(c + QPointF(6, 0)); d.lineTo(c + QPointF(0, 6)); d.lineTo(c + QPointF(-6, 0)); d.closeSubpath();
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawPath(d);
            x = 34;
        } else {
            p.setPen(tcol(Theme::textMuted()));
            p.setFont(uiFont(12));
            p.drawText(QRect(x, r.top(), 12, r.height()), Qt::AlignCenter, collapsed ? QStringLiteral("›") : QStringLiteral("⌄"));
            x += 16;
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawEllipse(QPointF(x + 4, r.center().y() + 0.5), 4, 4);
            x += 14;
        }
        p.setFont(uiFont(10.5));
        const int cw = p.fontMetrics().horizontalAdvance(count);
        p.setPen(tcol(Theme::textMuted()));
        p.drawText(QRect(r.right() - cw - 8, r.top(), cw + 4, r.height()), Qt::AlignVCenter | Qt::AlignRight, count);
        QFont f = uiFont(11, QFont::Bold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        p.setFont(f);
        p.setPen(onSpine ? color : tcol(Theme::textBright()));
        const int tw = r.right() - cw - 16 - x;
        p.drawText(QRect(x, r.top(), tw, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(onSpine ? title.toUpper() : title, Qt::ElideRight, tw));
    });
    row->setToolTip(collapsed ? tr("Mostrar os capítulos desta parte") : tr("Recolher esta parte"));
    row->setProperty("kind", QStringLiteral("part"));
    row->setProperty("partId", part.id);
    row->setContextMenuPolicy(Qt::CustomContextMenu);
    row->installEventFilter(this);
    const QString pid = part.id;
    connect(row, &QToolButton::clicked, this, [this, pid]() {
        if (m_collapsedParts.contains(pid)) m_collapsedParts.remove(pid);
        else m_collapsedParts.insert(pid);
        QSettings().setValue(QString::fromLatin1(kKeyCollapsed), QStringList(m_collapsedParts.begin(), m_collapsedParts.end()));
        rebuildList();
    });
    connect(row, &QToolButton::customContextMenuRequested, this, [this, row, pid](const QPoint& pos) {
        showPartContextMenu(pid, row->mapToGlobal(pos));
    });
    return row;
}

QWidget* ManuscriptPanel::makeStatusDot(const Chapter& c) {
    auto* b = new QToolButton(this);
    b->setFixedSize(16, 16);
    b->setCursor(Qt::PointingHandCursor);
    const bool none = c.status.isEmpty();
    const QString color = none ? Theme::textMuted() : ProjectModel::findWorkStatusColor(c.status);
    b->setStyleSheet(QStringLiteral(
        "QToolButton { background: transparent; border: none; }"
        "QToolButton::menu-indicator { image: none; }"));
    const qreal dpr = devicePixelRatioF();
    QPixmap pm(QSize(16, 16) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor col = tcol(color);
        if (none) { p.setPen(QPen(col, 1.2)); p.setBrush(Qt::NoBrush); }
        else { p.setPen(Qt::NoPen); p.setBrush(col); }
        p.drawEllipse(QPointF(8, 8), 3.8, 3.8);
    }
    b->setIcon(QIcon(pm));
    b->setIconSize(QSize(16, 16));
    b->setToolTip((none ? tr("Sem status") : ProjectModel::findWorkStatusLabel(c.status))
                  + QStringLiteral(" · ") + tr("clique pra trocar"));
    const QString id = c.id;
    connect(b, &QToolButton::clicked, this, [this, id]() { cycleStatus(id); });
    return b;
}

QWidget* ManuscriptPanel::makeVariationBadge(const Chapter& c, int sceneIdx) {
    if (!tool(ToolVariations) || sceneIdx < 0 || sceneIdx >= c.scenes.size()) return nullptr;
    const Scene& s = c.scenes.at(sceneIdx);
    if (s.variations.size() < 2) return nullptr;
    QString active;
    for (const auto& v : s.variations) if (v.id == s.activeVariationId) active = v.label;
    auto* b = chipButton(tr("%n versões", "", s.variations.size()), this, false);
    b->setToolTip(tr("Versão ativa: %1").arg(active.isEmpty() ? tr("(sem nome)") : active));
    const QString chId = c.id, msId = c.manuscriptId;
    connect(b, &QToolButton::clicked, this, [this, b, chId, msId, sceneIdx]() {
        const Scene* sc = m_model->findScene(chId, sceneIdx);
        if (!sc) return;
        QMenu menu(this);
        menu.setStyleSheet(contextMenuQss());
        auto* group = new QActionGroup(&menu);
        for (const auto& v : sc->variations) {
            QAction* a = menu.addAction(v.label.isEmpty() ? tr("(sem nome)") : v.label);
            a->setCheckable(true);
            a->setChecked(v.id == sc->activeVariationId);
            group->addAction(a);
            const QString vid = v.id;
            connect(a, &QAction::triggered, this, [this, msId, chId, sceneIdx, vid]() {
                emit switchVariationRequested(msId, chId, sceneIdx, vid);
            });
        }
        menu.addSeparator();
        connect(menu.addAction(tr("Nova variação")), &QAction::triggered, this, [this, chId, sceneIdx]() {
            emit createVariationRequested(chId, sceneIdx);
        });
        PanelMotion::animateMenu(&menu);
        menu.exec(b->mapToGlobal(QPoint(0, b->height())));
    });
    return b;
}

namespace {
QLabel* hereDot(QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setFixedSize(8, 8);
    l->setStyleSheet(QStringLiteral("background: %1; border-radius: 4px;").arg(Theme::accentDefault()));
    l->setToolTip(QCoreApplication::translate("ManuscriptPanel", "Você parou aqui"));
    return l;
}
}

QWidget* ManuscriptPanel::makeListChapterRow(const Chapter& c, const QColor& partColor) {
    const bool cur = isCurrent(c.id, -1);
    // O Clássico continua centralizado como sempre foi; os outros alinham à
    // esquerda, com reticências quando o título não cabe.
    const bool centered = (m_style == Style::Classic);
    const QString text = chapterRowText(c);
    auto* btn = new MsRow(this);
    btn->setRowHeight(34);
    btn->setPainter([cur, centered, text](QPainter& p, const QRect& r, const MsRow* self) {
        const bool hov = self->isHovered();
        if (cur || hov) {
            p.setPen(QPen(tcol(cur ? Theme::accentInfoBorderSoft() : Theme::subtleBorder()), 1));
            p.setBrush(tcol(cur ? Theme::accentInfoSoft() : Theme::hoverOverlay()));
            p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
        }
        p.setFont(serifFont(14));
        p.setPen(cur || hov ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
        const QRect tr2 = r.adjusted(10, 0, -10, 0);
        p.drawText(tr2, Qt::AlignVCenter | (centered ? Qt::AlignHCenter : Qt::AlignLeft),
                   p.fontMetrics().elidedText(text, Qt::ElideRight, tr2.width()));
    });
    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    const QString chapterId = c.id;
    const QString manuscriptId = c.manuscriptId;
    connect(btn, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() {
        emit chapterActivated(manuscriptId, chapterId);
    });
    connect(btn, &QToolButton::customContextMenuRequested, this, [this, btn, manuscriptId, chapterId](const QPoint& pos) {
        showChapterContextMenu(manuscriptId, chapterId, btn->mapToGlobal(pos));
    });
    wireRow(btn, QStringLiteral("chapter"), chapterId);

    auto* container = new QWidget(this);
    auto* rowLay = new QHBoxLayout(container);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(6);

    // Ordem da história: posição na LEITURA, acesa quando é salto.
    if (storyMode()) {
        const QList<Chapter> reading = readingChapters();
        const StoryOrder so = storyOrder(reading);
        int ri = 0;
        for (int i = 0; i < reading.size(); ++i) if (reading.at(i).id == c.id) ri = i;
        auto* n = new QLabel(chapterShortLabel(c), container);
        n->setFixedWidth(22);
        n->setAlignment(Qt::AlignCenter);
        n->setToolTip(tr("Posição na leitura"));
        n->setStyleSheet(QStringLiteral("color: %1; font-weight: 700; font-size: 11px; background: transparent;")
            .arg(so.jump.value(ri) ? Theme::accentWarning() : Theme::textMuted()));
        rowLay->addWidget(n);
    }
    // POV: rostinho do narrador.
    const QList<Chapter> reading = readingChapters();
    if (povBarVisible(reading)) {
        const QStringList order = narratorsInOrder(reading);
        auto* face = new QLabel(container);
        face->setFixedSize(18, 18);
        const QString p = povOf(c);
        if (!p.isEmpty()) {
            face->setPixmap(elementAvatar(p, povColor(p, order), 18));
            face->setToolTip(tr("Narrador: %1").arg(elementName(p)));
        }
        rowLay->addWidget(face);
    }
    // Pilula de proporção diálogo/narração encostada no início do título —
    // só quando há as duas fontes conectadas E o capítulo tem pelo menos uma
    // palavra (senão vira um traço cinza sem sentido).
    if (m_dialogueStore && m_wordCounter) {
        const int totalWords = m_wordCounter->countChapter(chapterId);
        if (totalWords > 0) {
            const int dlgWords = m_dialogueStore->dialogueWordsForChapter(chapterId);
            auto* bar = new DialogueRatioBar(container);
            bar->setRatio(double(dlgWords) / double(totalWords));
            // Clique abre a janela de estatísticas do capítulo — a barra não
            // tem Q_OBJECT (de propósito, por causa do tr()), então o clique é
            // pego via propriedade + eventFilter do painel.
            bar->setProperty("ratioBarChapterId", chapterId);
            bar->setProperty("ratioBarManuscriptId", manuscriptId);
            bar->installEventFilter(this);
            rowLay->addWidget(bar);
        }
    }
    rowLay->addWidget(btn, 1);
    if (storyMode() && !c.timeMarker.trimmed().isEmpty()) {
        auto* tm = mutedLabel(c.timeMarker, container, 10.5);
        tm->setWordWrap(false);
        tm->setFixedWidth(qMin(92, tm->fontMetrics().horizontalAdvance(c.timeMarker) + 4));
        rowLay->addWidget(tm);
    }
    if (tool(ToolResume) && !m_resume.isEmpty() && m_resume.first().chapterId == c.id
        && (c.scenes.size() <= 1 || m_resume.first().sceneIndex < 0))
        rowLay->addWidget(hereDot(container));
    if (statusVisible()) rowLay->addWidget(makeStatusDot(c));

    if (!m_povFilter.isEmpty() && povOf(c) != m_povFilter) {
        auto* eff = new QGraphicsOpacityEffect(container);
        eff->setOpacity(0.4);
        container->setGraphicsEffect(eff);
    }
    container->setProperty("kind", QStringLiteral("chapter"));
    container->setProperty("chapterId", chapterId);
    return wrapInPart(container, partColor);
}

QWidget* ManuscriptPanel::makeListSceneRow(const Chapter& c, int i, const QColor& partColor) {
    const bool cur = isCurrent(c.id, i);
    const bool centered = (m_style == Style::Classic);
    const QString text = QStringLiteral("· %1").arg(sceneTitle(c, i));
    auto* sbtn = new MsRow(this);
    sbtn->setRowHeight(26);
    sbtn->setToolTip(sceneTitle(c, i));
    sbtn->setPainter([cur, centered, text](QPainter& p, const QRect& r, const MsRow* self) {
        const bool hov = self->isHovered();
        if (cur || hov) {
            p.setPen(Qt::NoPen);
            p.setBrush(tcol(cur ? Theme::accentInfoSoft() : Theme::hoverOverlay()));
            p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
        }
        p.setFont(serifFont(13, QFont::Normal, true));
        p.setPen(cur || hov ? tcol(Theme::textBright()) : tcol(Theme::textMuted()));
        const QRect tr2 = r.adjusted(centered ? 10 : 26, 0, -10, 0);
        p.drawText(tr2, Qt::AlignVCenter | (centered ? Qt::AlignHCenter : Qt::AlignLeft),
                   p.fontMetrics().elidedText(text, Qt::ElideRight, tr2.width()));
    });
    sbtn->setContextMenuPolicy(Qt::CustomContextMenu);
    const QString chapterId = c.id, manuscriptId = c.manuscriptId;
    connect(sbtn, &QToolButton::clicked, this, [this, manuscriptId, chapterId, i]() {
        emit sceneActivated(manuscriptId, chapterId, i);
    });
    connect(sbtn, &QToolButton::customContextMenuRequested, this, [this, sbtn, manuscriptId, chapterId, i](const QPoint& pos) {
        showSceneContextMenu(manuscriptId, chapterId, i, sbtn->mapToGlobal(pos));
    });
    wireRow(sbtn, QStringLiteral("scene"), chapterId, i);

    auto* container = new QWidget(this);
    auto* rowLay = new QHBoxLayout(container);
    rowLay->setContentsMargins(0, 0, 4, 0);
    rowLay->setSpacing(6);
    rowLay->addWidget(sbtn, 1);
    if (QWidget* badge = makeVariationBadge(c, i)) rowLay->addWidget(badge);
    if (tool(ToolResume) && !m_resume.isEmpty() && m_resume.first().chapterId == c.id
        && m_resume.first().sceneIndex == i)
        rowLay->addWidget(hereDot(container));
    if (!m_povFilter.isEmpty() && povOf(c) != m_povFilter) {
        auto* eff = new QGraphicsOpacityEffect(container);
        eff->setOpacity(0.4);
        container->setGraphicsEffect(eff);
    }
    container->setProperty("kind", QStringLiteral("scene"));
    container->setProperty("chapterId", chapterId);
    container->setProperty("sceneIndex", i);
    return wrapInPart(container, partColor);
}

// Lista, Sumário, Espinha e Grade percorrem os capítulos na ordem da lista,
// com o cabeçalho de parte na hora certa e pulando o miolo das recolhidas.
void ManuscriptPanel::buildListView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = partsActive() ? validParts(reading) : QList<ManuscriptPart>();
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    QHash<int, QPair<int, int>> stats;
    for (const auto& c : reading) {
        const int pi = pidx.value(c.id, -1);
        if (pi >= 0) { stats[pi].first += 1; stats[pi].second += chapterWords(c.id); }
    }
    int curPart = -2;
    for (const auto& c : chs) {
        const int pi = parts.isEmpty() ? -1 : pidx.value(c.id, -1);
        if (pi != curPart) {
            curPart = pi;
            if (pi >= 0) add(makePartHeader(parts.at(pi), stats.value(pi).first, stats.value(pi).second));
        }
        if (pi >= 0 && m_collapsedParts.contains(parts.at(pi).id)) continue;
        const QColor pc = pi >= 0 ? QColor(parts.at(pi).color) : QColor();
        add(makeListChapterRow(c, pc));
        if (c.scenes.size() > 1)
            for (int i = 0; i < c.scenes.size(); ++i) add(makeListSceneRow(c, i, pc));
    }
}

// ---- Sumário ----

void ManuscriptPanel::buildTocView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const Manuscript* m = m_model->findManuscript(activeManuscriptId());
    {
        auto* head = new QWidget(this);
        auto* hl = new QVBoxLayout(head);
        hl->setContentsMargins(0, 6, 0, 12);
        hl->setSpacing(4);
        auto* k = capsLabel(tr("Sumário"), head);
        k->setAlignment(Qt::AlignCenter);
        hl->addWidget(k);
        auto* t = new QLabel(m && !m->title.isEmpty() ? m->title : tr("(sem título)"), head);
        t->setAlignment(Qt::AlignCenter);
        t->setWordWrap(true);
        t->setFont(serifFont(17, QFont::Normal, true));
        t->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textBright()));
        hl->addWidget(t);
        add(head);
    }
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = partsActive() ? validParts(reading) : QList<ManuscriptPart>();
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    QHash<int, QPair<int, int>> stats;
    for (const auto& c : reading) {
        const int pi = pidx.value(c.id, -1);
        if (pi >= 0) { stats[pi].first += 1; stats[pi].second += chapterWords(c.id); }
    }
    const QStringList order = narratorsInOrder(reading);
    const bool pov = povBarVisible(reading);
    const bool status = statusVisible();
    const QString resumeCh = (tool(ToolResume) && !m_resume.isEmpty()) ? m_resume.first().chapterId : QString();
    int curPart = -2;
    for (const auto& c : chs) {
        const int pi = parts.isEmpty() ? -1 : pidx.value(c.id, -1);
        if (pi != curPart) {
            curPart = pi;
            if (pi >= 0) add(makePartHeader(parts.at(pi), stats.value(pi).first, stats.value(pi).second));
        }
        if (pi >= 0 && m_collapsedParts.contains(parts.at(pi).id)) continue;
        const bool special = (c.type != QStringLiteral("chapter"));
        const QString num = special ? QString() : m_model->chapterNumberLabel(c);
        const QString title = special
            ? m_model->chapterTypeName(c) + (c.title.isEmpty() ? QString() : QStringLiteral(" · ") + c.title)
            : chapterTitleOnly(c);
        const QString words = chapterWords(c.id) ? MsPaint::fmtInt(chapterWords(c.id)) : QStringLiteral("—");
        const bool cur = isCurrent(c.id, -1);
        const bool dim = !m_povFilter.isEmpty() && povOf(c) != m_povFilter;
        const QColor povC = pov && !povOf(c).isEmpty() ? povColor(povOf(c), order) : QColor();
        const QColor stC = status ? statusColor(c) : QColor();
        const bool statusNone = c.status.isEmpty();
        const bool here = (c.id == resumeCh && m_resume.first().sceneIndex < 0);
        const QColor partC = pi >= 0 ? QColor(parts.at(pi).color) : QColor();
        auto* row = new MsRow(this);
        row->setRowHeight(30);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (dim) p.setOpacity(0.4);
            if (cur || self->isHovered()) {
                p.setPen(Qt::NoPen);
                p.setBrush(cur ? tcol(Theme::accentInfoSoft()) : tcol(Theme::hoverOverlay()));
                p.drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 4, 4);
            }
            int x = r.left() + 4;
            if (partC.isValid()) { QColor pc = partC; pc.setAlphaF(0.75); p.fillRect(QRect(x + 2, r.top(), 2, r.height()), pc); x += 8; }
            if (povC.isValid()) { p.fillRect(QRect(x, r.top() + 8, 3, r.height() - 16), povC); }
            x += 6;
            if (stC.isValid()) {
                if (statusNone) { p.setPen(QPen(stC, 1.1)); p.setBrush(Qt::NoBrush); }
                else { p.setPen(Qt::NoPen); p.setBrush(stC); }
                p.drawEllipse(QPointF(x + 3, r.center().y() + 0.5), 3, 3);
                x += 9;
            }
            const QColor ink = (cur || self->isHovered()) ? tcol(Theme::textBright()) : tcol(Theme::textPrimary());
            p.setFont(serifFont(13));
            p.setPen(tcol(Theme::textMuted()));
            p.drawText(QRect(x, r.top(), 22, r.height()), Qt::AlignVCenter | Qt::AlignRight, num);
            x += 30;
            p.setFont(uiFont(11));
            const int ww = p.fontMetrics().horizontalAdvance(words);
            const int rightX = r.right() - 8 - ww;
            p.drawText(QRect(rightX, r.top(), ww + 2, r.height()), Qt::AlignVCenter | Qt::AlignRight, words);
            p.setFont(serifFont(14, QFont::Normal, special));
            p.setPen(ink);
            const int maxT = rightX - x - 16;
            const QString t = p.fontMetrics().elidedText(title, Qt::ElideRight, maxT);
            const int tw = p.fontMetrics().horizontalAdvance(t);
            p.drawText(QRect(x, r.top(), maxT, r.height()), Qt::AlignVCenter | Qt::AlignLeft, t);
            int lx = x + tw + 6;
            if (here) {
                p.setPen(Qt::NoPen);
                p.setBrush(tcol(Theme::accentDefault()));
                p.drawEllipse(QPointF(lx + 3, r.center().y()), 3, 3);
                lx += 10;
            }
            // reticências até o número de palavras
            QPen dots(tcol(Theme::textMuted()), 1, Qt::DotLine);
            p.setPen(dots);
            const int by = r.center().y() + 5;
            if (rightX - 6 > lx) p.drawLine(lx, by, rightX - 6, by);
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString chapterId = c.id, manuscriptId = c.manuscriptId;
        connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
            showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chapterId);
        add(row);

        // Cenas do capítulo aberto, com algarismos romanos.
        if (c.id == m_curChapterId && c.scenes.size() > 1) {
            for (int j = 0; j < c.scenes.size(); ++j) {
                const QString st = sceneTitle(c, j);
                const bool scur = isCurrent(c.id, j);
                const int nvar = tool(ToolVariations) ? c.scenes.at(j).variations.size() : 0;
                const QString vtxt = nvar >= 2 ? tr("%n versões", "", nvar) : QString();
                auto* srow = new MsRow(this);
                srow->setRowHeight(24);
                srow->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
                    if (scur || self->isHovered()) {
                        p.setPen(Qt::NoPen);
                        p.setBrush(scur ? tcol(Theme::accentInfoSoft()) : tcol(Theme::hoverOverlay()));
                        p.drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 4, 4);
                    }
                    const int x = r.left() + 52;
                    p.setFont(serifFont(12));
                    p.setPen(tcol(Theme::textMuted()));
                    const QString rn = romanLower(j + 1);
                    const int rw = p.fontMetrics().horizontalAdvance(rn);
                    const int rightX = r.right() - 8 - rw;
                    p.drawText(QRect(rightX, r.top(), rw + 2, r.height()), Qt::AlignVCenter | Qt::AlignRight, rn);
                    p.setFont(serifFont(12.5, QFont::Normal, true));
                    p.setPen(scur || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
                    QString t = st + (vtxt.isEmpty() ? QString() : QStringLiteral("  (") + vtxt + QStringLiteral(")"));
                    t = p.fontMetrics().elidedText(t, Qt::ElideRight, rightX - x - 14);
                    const int tw = p.fontMetrics().horizontalAdvance(t);
                    p.drawText(QRect(x, r.top(), rightX - x, r.height()), Qt::AlignVCenter | Qt::AlignLeft, t);
                    QPen dots(tcol(Theme::textMuted()), 1, Qt::DotLine);
                    p.setPen(dots);
                    const int by = r.center().y() + 5;
                    if (rightX - 6 > x + tw + 6) p.drawLine(x + tw + 6, by, rightX - 6, by);
                });
                srow->setContextMenuPolicy(Qt::CustomContextMenu);
                connect(srow, &QToolButton::clicked, this, [this, manuscriptId, chapterId, j]() { emit sceneActivated(manuscriptId, chapterId, j); });
                connect(srow, &QToolButton::customContextMenuRequested, this, [this, srow, manuscriptId, chapterId, j](const QPoint& pos) {
                    showSceneContextMenu(manuscriptId, chapterId, j, srow->mapToGlobal(pos));
                });
                wireRow(srow, QStringLiteral("scene"), chapterId, j);
                add(srow);
            }
        }
    }
}

// ---- Espinha ----

void ManuscriptPanel::buildSpineView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = partsActive() ? validParts(reading) : QList<ManuscriptPart>();
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    QHash<int, QPair<int, int>> stats;
    int maxW = 1;
    for (const auto& c : reading) {
        const int w = chapterWords(c.id);
        maxW = qMax(maxW, w);
        const int pi = pidx.value(c.id, -1);
        if (pi >= 0) { stats[pi].first += 1; stats[pi].second += w; }
    }
    const QStringList order = narratorsInOrder(reading);
    const bool pov = povBarVisible(reading);
    const bool status = statusVisible();
    const QString resumeCh = (tool(ToolResume) && !m_resume.isEmpty()) ? m_resume.first().chapterId : QString();
    const int resumeSc = resumeCh.isEmpty() ? -2 : m_resume.first().sceneIndex;
    int curPart = -2;
    for (int ci = 0; ci < chs.size(); ++ci) {
        const Chapter& c = chs.at(ci);
        const int pi = parts.isEmpty() ? -1 : pidx.value(c.id, -1);
        if (pi != curPart) {
            curPart = pi;
            if (pi >= 0) add(makePartHeader(parts.at(pi), stats.value(pi).first, stats.value(pi).second));
        }
        if (pi >= 0 && m_collapsedParts.contains(parts.at(pi).id)) continue;
        const int words = chapterWords(c.id);
        const bool special = (c.type != QStringLiteral("chapter"));
        const QString t1 = special
            ? m_model->chapterTypeName(c) + QStringLiteral(" · ") + chapterTitleOnly(c)
            : m_model->chapterNumberLabel(c) + QStringLiteral(" · ") + chapterTitleOnly(c);
        QString t2 = tr("%1 palavras").arg(MsPaint::fmtInt(words));
        if (status && !c.status.isEmpty()) t2 += QStringLiteral(" · ") + ProjectModel::findWorkStatusLabel(c.status);
        const QColor ball = statusColor(c);
        const bool hollow = status && c.status.isEmpty();
        // Faixa curta: capítulo de 12 mil palavras não pode virar um balão.
        const qreal radius = words ? 5.5 + 3.5 * words / maxW : 4.5;
        const bool cur = isCurrent(c.id, -1);
        const bool first = (ci == 0), last = (ci == chs.size() - 1) && c.scenes.size() <= 1;
        const bool dim = !m_povFilter.isEmpty() && povOf(c) != m_povFilter;
        const QString pv = pov ? povOf(c) : QString();
        const QPixmap face = pv.isEmpty() ? QPixmap() : elementAvatar(pv, povColor(pv, order), 16);
        const bool here = (c.id == resumeCh && (resumeSc < 0 || c.scenes.size() <= 1));
        auto* row = new MsRow(this);
        row->setRowHeight(46);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (dim) p.setOpacity(0.4);
            if (cur || self->isHovered()) {
                p.setPen(Qt::NoPen);
                p.setBrush(cur ? tcol(Theme::accentInfoSoft()) : tcol(Theme::hoverOverlay()));
                p.drawRoundedRect(QRectF(r).adjusted(28, 3, -1, -3), 5, 5);
            }
            const qreal ax = 18, cy = r.top() + 20;
            p.setPen(QPen(tcol(Theme::panelBorder()), 2));
            p.drawLine(QPointF(ax, first ? cy : r.top()), QPointF(ax, last ? cy : r.bottom() + 1));
            if (cur) {
                QColor halo = ball; halo.setAlphaF(0.35);
                p.setPen(QPen(halo, 2));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QPointF(ax, cy), radius + 4, radius + 4);
            }
            QPen bp(ball, 2);
            if (special) bp.setStyle(Qt::DotLine);
            p.setPen(hollow || special ? bp : QPen(Qt::NoPen));
            p.setBrush(hollow ? tcol(Theme::panelBackground()) : ball);
            p.drawEllipse(QPointF(ax, cy), radius, radius);
            if (!hollow) {
                // Anel por dentro: estação, não bola.
                p.setPen(QPen(tcol(Theme::panelBackground()), 1.4));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QPointF(ax, cy), radius * 0.5, radius * 0.5);
            }
            int right = r.right() - 8;
            if (!face.isNull()) { p.drawPixmap(right - 16, int(cy) - 8, face); right -= 22; }
            if (here) {
                p.setPen(Qt::NoPen);
                p.setBrush(tcol(Theme::accentDefault()));
                p.drawEllipse(QPointF(right - 4, cy), 3.5, 3.5);
                right -= 12;
            }
            const int x = 38;
            p.setFont(serifFont(13.5, QFont::DemiBold));
            p.setPen(cur || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
            p.drawText(QRect(x, r.top() + 5, right - x, 20), Qt::AlignVCenter | Qt::AlignLeft,
                       p.fontMetrics().elidedText(t1, Qt::ElideRight, right - x));
            p.setFont(uiFont(10.5));
            p.setPen(tcol(Theme::textMuted()));
            p.drawText(QRect(x, r.top() + 24, right - x, 16), Qt::AlignVCenter | Qt::AlignLeft, t2);
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString chapterId = c.id, manuscriptId = c.manuscriptId;
        connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
            showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chapterId);
        add(row);

        // Cenas: estações menores na mesma espinha, com nome e tamanho.
        if (c.scenes.size() > 1) {
            for (int j = 0; j < c.scenes.size(); ++j) {
                const QString st = sceneTitle(c, j);
                const int sw = sceneWords(c.id, j);
                const int nvar = tool(ToolVariations) ? c.scenes.at(j).variations.size() : 0;
                QString meta = MsPaint::fmtInt(sw);
                if (nvar >= 2) meta += QStringLiteral(" · ") + tr("%n versões", "", nvar);
                const bool scur = isCurrent(c.id, j);
                const bool slast = (ci == chs.size() - 1) && (j == c.scenes.size() - 1);
                const bool shere = (c.id == resumeCh && resumeSc == j);
                auto* srow = new MsRow(this);
                srow->setRowHeight(28);
                srow->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
                    if (dim) p.setOpacity(0.4);
                    if (scur || self->isHovered()) {
                        p.setPen(Qt::NoPen);
                        p.setBrush(scur ? tcol(Theme::accentInfoSoft()) : tcol(Theme::hoverOverlay()));
                        p.drawRoundedRect(QRectF(r).adjusted(28, 2, -1, -2), 4, 4);
                    }
                    const qreal ax = 18, cy = r.center().y();
                    p.setPen(QPen(tcol(Theme::panelBorder()), 2));
                    p.drawLine(QPointF(ax, r.top()), QPointF(ax, slast ? cy : r.bottom() + 1));
                    p.setPen(QPen(ball, 1.6));
                    p.setBrush(scur ? ball : tcol(Theme::panelBackground()));
                    p.drawEllipse(QPointF(ax, cy), 3.5, 3.5);
                    p.setFont(uiFont(10.5));
                    const int mw = p.fontMetrics().horizontalAdvance(meta);
                    int right = r.right() - 8;
                    if (shere) {
                        p.setPen(Qt::NoPen);
                        p.setBrush(tcol(Theme::accentDefault()));
                        p.drawEllipse(QPointF(right - 3, cy), 3, 3);
                        right -= 10;
                    }
                    p.setPen(tcol(Theme::textMuted()));
                    p.drawText(QRect(right - mw, r.top(), mw + 2, r.height()), Qt::AlignVCenter | Qt::AlignRight, meta);
                    p.setFont(serifFont(12.5, QFont::Normal, true));
                    p.setPen(scur || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
                    const int x = 38, tw = right - mw - 10 - x;
                    p.drawText(QRect(x, r.top(), tw, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
                               p.fontMetrics().elidedText(st, Qt::ElideRight, tw));
                });
                srow->setContextMenuPolicy(Qt::CustomContextMenu);
                connect(srow, &QToolButton::clicked, this, [this, manuscriptId, chapterId, j]() { emit sceneActivated(manuscriptId, chapterId, j); });
                connect(srow, &QToolButton::customContextMenuRequested, this, [this, srow, manuscriptId, chapterId, j](const QPoint& pos) {
                    showSceneContextMenu(manuscriptId, chapterId, j, srow->mapToGlobal(pos));
                });
                wireRow(srow, QStringLiteral("scene"), chapterId, j);
                add(srow);
            }
        }
    }
}

// ---- Grade de cenas ----

void ManuscriptPanel::buildGridView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = partsActive() ? validParts(reading) : QList<ManuscriptPart>();
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    QHash<int, QPair<int, int>> stats;
    int maxChapter = 1;
    for (const auto& c : reading) {
        const int w = chapterWords(c.id);
        maxChapter = qMax(maxChapter, w);
        const int pi = pidx.value(c.id, -1);
        if (pi >= 0) { stats[pi].first += 1; stats[pi].second += w; }
    }
    constexpr int kGap = 3, kCellH = 9;
    int curPart = -2;
    for (const auto& c : chs) {
        const int pi = parts.isEmpty() ? -1 : pidx.value(c.id, -1);
        if (pi != curPart) {
            curPart = pi;
            if (pi >= 0) add(makePartHeader(parts.at(pi), stats.value(pi).first, stats.value(pi).second));
        }
        if (pi >= 0 && m_collapsedParts.contains(parts.at(pi).id)) continue;
        QList<int> sw;
        QList<QColor> sc;
        QStringList tips;
        const int n = qMax(1, int(c.scenes.size()));
        for (int j = 0; j < n; ++j) {
            const int w = c.scenes.size() > 1 ? sceneWords(c.id, j) : chapterWords(c.id);
            sw.append(w);
            QColor col = statusColor(c);
            if (statusVisible() && j < c.scenes.size() && !c.scenes.at(j).status.isEmpty())
                col = QColor(ProjectModel::findWorkStatusColor(c.scenes.at(j).status));
            sc.append(col);
            QString tip = QStringLiteral("%1 · %2").arg(c.scenes.size() > 1 ? sceneTitle(c, j) : chapterRowText(c),
                                                        tr("%1 palavras").arg(MsPaint::fmtInt(w)));
            const int nvar = (tool(ToolVariations) && j < c.scenes.size()) ? c.scenes.at(j).variations.size() : 0;
            if (nvar >= 2) tip += QStringLiteral(" · ") + tr("%n versões", "", nvar);
            tips.append(tip);
        }
        const QString label = (c.type == QStringLiteral("chapter"))
            ? m_model->chapterNumberLabel(c) + QStringLiteral(" · ") + chapterTitleOnly(c)
            : m_model->chapterTypeName(c);
        const bool curCh = (c.id == m_curChapterId);
        const int curSc = curCh ? m_curScene : -99;
        const bool dim = !m_povFilter.isEmpty() && povOf(c) != m_povFilter;
        const QColor partC = pi >= 0 ? QColor(parts.at(pi).color) : QColor();
        // Título em cima, tijolinhos embaixo (pequenos, na largura toda).
        auto cellRects = [sw, maxChapter](const QRect& r) {
            QList<QRect> out;
            const int left = r.left() + 10;
            const int avail = r.width() - 20 - kGap * int(sw.size());
            int x = left;
            for (int w : sw) {
                const int cw = qMax(6, int(double(avail) * w / maxChapter));
                out.append(QRect(x, r.top() + 24, cw, kCellH));
                x += cw + kGap;
            }
            return out;
        };
        auto* row = new MsRow(this);
        row->setRowHeight(24 + kCellH + 8);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (dim) p.setOpacity(0.4);
            if (curCh || (self->isHovered() && self->hoverPart() < 0)) {
                p.setPen(Qt::NoPen);
                p.setBrush(curCh ? tcol(Theme::accentInfoSoft()) : tcol(Theme::hoverOverlay()));
                p.drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 4, 4);
            }
            if (partC.isValid()) { QColor pc = partC; pc.setAlphaF(0.75); p.fillRect(QRect(r.left() + 1, r.top(), 2, r.height()), pc); }
            p.setFont(serifFont(13, curCh ? QFont::DemiBold : QFont::Normal));
            p.setPen(curCh || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
            p.drawText(QRect(r.left() + 10, r.top() + 3, r.width() - 20, 19), Qt::AlignVCenter | Qt::AlignLeft,
                       p.fontMetrics().elidedText(label, Qt::ElideRight, r.width() - 20));
            const QList<QRect> cells = cellRects(r);
            for (int j = 0; j < cells.size(); ++j) {
                QColor f = sc.at(j);
                const bool on = (curSc == j) || (curCh && sw.size() == 1 && curSc < 0);
                f.setAlphaF(sw.at(j) == 0 ? 0.18 : (on ? 1.0 : (self->hoverPart() == j ? 0.85 : 0.55)));
                p.setPen(Qt::NoPen);
                p.setBrush(f);
                p.drawRoundedRect(QRectF(cells.at(j)), 2, 2);
                if (on) {
                    p.setPen(QPen(tcol(Theme::textBright()), 1.2));
                    p.setBrush(Qt::NoBrush);
                    p.drawRoundedRect(QRectF(cells.at(j)).adjusted(-1.5, -1.5, 1.5, 1.5), 3, 3);
                }
            }
        });
        row->setHitTest([row, cellRects](const QPoint& pt) {
            const QList<QRect> cells = cellRects(row->rect());
            for (int j = 0; j < cells.size(); ++j) if (cells.at(j).adjusted(-1, -4, 1, 4).contains(pt)) return j;
            return -1;
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString chapterId = c.id, manuscriptId = c.manuscriptId;
        const bool multi = c.scenes.size() > 1;
        connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
        connect(row, &MsRow::partClicked, this, [this, manuscriptId, chapterId, multi](int j) {
            if (multi) emit sceneActivated(manuscriptId, chapterId, j);
            else emit chapterActivated(manuscriptId, chapterId);
        });
        connect(row, &MsRow::partHovered, this, [tips](int j) {
            if (j >= 0 && j < tips.size()) QToolTip::showText(QCursor::pos(), tips.at(j));
            else QToolTip::hideText();
        });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId, multi](const QPoint& pos) {
            const int j = multi ? row->hoverPart() : -1;
            if (j >= 0) showSceneContextMenu(manuscriptId, chapterId, j, row->mapToGlobal(pos));
            else showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chapterId);
        add(row);
    }
}

// ---- Mosaico e Jornada ----

namespace {
MsChapterDot dotFor(const Chapter& c, const QString& shortLabel, const QString& title, const QString& meta,
                    int words, double dlg, const QColor& color, bool current, bool dim) {
    MsChapterDot d;
    d.chapterId = c.id;
    d.shortLabel = shortLabel;
    d.title = title;
    d.meta = meta;
    d.words = words;
    d.dialogue = dlg;
    d.color = color;
    d.special = (c.type != QStringLiteral("chapter"));
    d.current = current;
    d.dim = dim;
    return d;
}
}

void ManuscriptPanel::buildMosaicView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    int total = 0, fin = 0;
    QList<MsChapterDot> dots;
    for (const auto& c : chs) {
        const int w = chapterWords(c.id);
        total += w;
        if (c.status == QStringLiteral("final")) ++fin;
        dots.append(dotFor(c, chapterShortLabel(c), chapterTitleOnly(c), QString(), w, 0.0, statusColor(c),
                           c.id == m_curChapterId, !m_povFilter.isEmpty() && povOf(c) != m_povFilter));
    }
    auto* head = new QWidget(this);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(4, 2, 4, 6);
    hl->addWidget(capsLabel(tr("%n capítulos", "", chs.size()) + QStringLiteral(" · ")
                            + tr("%1 palavras").arg(MsPaint::fmtInt(total)), head), 1);
    if (statusVisible() && fin) hl->addWidget(mutedLabel(tr("%n finais", "", fin), head, 11));
    add(head);
    auto* mosaic = new MsMosaic(this);
    mosaic->setDots(dots);
    const QString msId = activeManuscriptId();
    connect(mosaic, &MsMosaic::chapterClicked, this, [this, msId](const QString& id) { emit chapterActivated(msId, id); });
    connect(mosaic, &MsMosaic::chapterContextRequested, this, [this, msId](const QString& id, const QPoint& pos) {
        showChapterContextMenu(msId, id, pos);
    });
    connect(mosaic, &MsMosaic::chapterHovered, this, [this](const QString& id, const QRect& r) {
        if (id.isEmpty()) disarmHover(); else armHover(id, r);
    });
    add(mosaic);
}

void ManuscriptPanel::buildJourneyView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    QList<MsChapterDot> dots;
    for (const auto& c : chs) {
        const int w = chapterWords(c.id);
        QString meta = MsPaint::fmtInt(w);
        if (statusVisible() && !c.status.isEmpty()) meta += QStringLiteral(" · ") + ProjectModel::findWorkStatusLabel(c.status);
        dots.append(dotFor(c, chapterShortLabel(c), chapterTitleOnly(c), meta, w, 0.0, statusColor(c),
                           c.id == m_curChapterId, !m_povFilter.isEmpty() && povOf(c) != m_povFilter));
    }
    QList<MsBand> bands;
    if (partsActive()) {
        const QList<ManuscriptPart> parts = validParts(chs);
        const QHash<QString, int> pidx = partIndexByChapter(chs, parts);
        for (int pi = 0; pi < parts.size(); ++pi) {
            MsBand b;
            b.title = parts.at(pi).title;
            b.color = QColor(parts.at(pi).color);
            b.first = -1;
            for (int i = 0; i < chs.size(); ++i) {
                if (pidx.value(chs.at(i).id, -1) != pi) continue;
                if (b.first < 0) b.first = i;
                b.last = i;
            }
            if (b.first >= 0) bands.append(b);
        }
    }
    auto* j = new MsJourney(this);
    j->setData(dots, bands);
    const QString msId = activeManuscriptId();
    connect(j, &MsJourney::chapterClicked, this, [this, msId](const QString& id) { emit chapterActivated(msId, id); });
    connect(j, &MsJourney::chapterContextRequested, this, [this, msId](const QString& id, const QPoint& pos) {
        showChapterContextMenu(msId, id, pos);
    });
    connect(j, &MsJourney::chapterHovered, this, [this](const QString& id, const QRect& r) {
        if (id.isEmpty()) disarmHover(); else armHover(id, r);
    });
    add(j);
}

// ============================================================ rodapé

void ManuscriptPanel::rebuildBottom() {
    clearLayout(m_bottomLayout);
    if (!m_model) { m_bottom->hide(); return; }
    const QList<Chapter> reading = readingChapters();
    const QString msId = activeManuscriptId();

    // Mosaico: o detalhe do capítulo aberto (ou do primeiro).
    if (m_style == Style::Mosaic && !reading.isEmpty()) {
        const Chapter* c = nullptr;
        for (const auto& ch : reading) if (ch.id == m_curChapterId) c = &ch;
        if (!c) c = &reading.first();
        auto* box = new QFrame(m_bottom);
        box->setObjectName(QStringLiteral("msMosaicDetail"));
        box->setStyleSheet(Theme::qss(QStringLiteral("#msMosaicDetail { border-top: 1px solid %1; }"))
            .arg(Theme::subtleBorder()));
        auto* bl = new QVBoxLayout(box);
        bl->setContentsMargins(0, 8, 0, 0);
        bl->setSpacing(3);
        auto* h = new QLabel(chapterShortLabel(*c) + QStringLiteral(" · ") + chapterTitleOnly(*c), box);
        h->setFont(serifFont(14, QFont::DemiBold));
        h->setWordWrap(true);
        h->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textBright()));
        bl->addWidget(h);
        QString meta = tr("%1 palavras").arg(MsPaint::fmtInt(chapterWords(c->id))) + QStringLiteral(" · ")
            + tr("%n cenas", "", qMax(1, int(c->scenes.size())));
        if (statusVisible() && !c->status.isEmpty()) meta += QStringLiteral(" · ") + ProjectModel::findWorkStatusLabel(c->status);
        bl->addWidget(mutedLabel(meta, box, 11));
        if (c->scenes.size() > 1) {
            for (int j = 0; j < c->scenes.size() && j < 8; ++j) {
                auto* s = linkButton(QStringLiteral("· ") + sceneTitle(*c, j), box,
                                     isCurrent(c->id, j) ? Theme::textBright() : Theme::textPrimary(), 12);
                const QString chId = c->id;
                connect(s, &QToolButton::clicked, this, [this, msId, chId, j]() { emit sceneActivated(msId, chId, j); });
                bl->addWidget(s);
            }
        }
        m_bottomLayout->addWidget(box);
    }

    // Grade: legenda.
    if (m_style == Style::Grid && !reading.isEmpty()) {
        auto* leg = new QHBoxLayout;
        leg->setSpacing(10);
        if (statusVisible()) {
            for (const auto& ws : ProjectModel::workStatuses()) {
                auto* l = new QLabel(dotImgTag(QColor(ws.color), 8) + QStringLiteral(" ") + ws.label.toHtmlEscaped(), m_bottom);
                l->setTextFormat(Qt::RichText);
                l->setStyleSheet(QStringLiteral("color: %1; font-size: 10.5px; background: transparent;").arg(Theme::textMuted()));
                leg->addWidget(l);
            }
        }
        leg->addStretch(1);
        auto* wl = mutedLabel(tr("largura = tamanho da cena"), m_bottom, 10.5);
        wl->setWordWrap(false);
        leg->addWidget(wl);
        m_bottomLayout->addLayout(leg);
    }

    // POV: ler a linha de um narrador em sequência.
    if (tool(ToolPov) && !m_povFilter.isEmpty() && povBarVisible(reading)) {
        QList<Chapter> mine;
        for (const auto& c : reading) if (povOf(c) == m_povFilter) mine.append(c);
        if (!mine.isEmpty()) {
            auto* bar = new QFrame(m_bottom);
            bar->setObjectName(QStringLiteral("msReadLine"));
            bar->setStyleSheet(Theme::qss(QStringLiteral(
                "#msReadLine { background: %1; border: 1px solid %2; border-radius: @radius-control; }"))
                .arg(Theme::hoverOverlay(), Theme::subtleBorder()));
            auto* rl = new QHBoxLayout(bar);
            rl->setContentsMargins(8, 6, 8, 6);
            rl->setSpacing(8);
            auto* face = new QLabel(bar);
            face->setPixmap(elementAvatar(m_povFilter, povColor(m_povFilter, narratorsInOrder(reading)), 22));
            face->setFixedSize(22, 22);
            rl->addWidget(face);
            const QString name = elementName(m_povFilter).toHtmlEscaped();
            auto* txt = mutedLabel(m_povReading > 0
                ? tr("Lendo a linha de %1: <b>%2 de %3</b>").arg(name).arg(m_povReading).arg(mine.size())
                : tr("Linha de %1: %n capítulo(s)", "", mine.size()).arg(name), bar, 11.5);
            txt->setTextFormat(Qt::RichText);
            rl->addWidget(txt, 1);
            const QString goText = m_povReading == 0 ? tr("Ler a linha ›")
                : (m_povReading < mine.size() ? tr("Próximo ›") : tr("Recomeçar"));
            auto* go = linkButton(goText, bar, Theme::accentDefault(), 12, true);
            connect(go, &QToolButton::clicked, this, [this, mine, msId]() {
                m_povReading = (m_povReading >= mine.size()) ? 1 : m_povReading + 1;
                emit chapterActivated(msId, mine.at(m_povReading - 1).id);
                rebuildList();
            });
            rl->addWidget(go);
            m_bottomLayout->addWidget(bar);
        }
    }
    m_bottom->setVisible(m_bottomLayout->count() > 0);
}

// ============================================================ ficha no hover

void ManuscriptPanel::armHover(const QString& chapterId, const QRect& globalRect) {
    if (!tool(ToolHover) || chapterId.isEmpty()) return;
    m_hoverRect = globalRect;
    if (m_hoverChapterId == chapterId && m_hoverCard->isVisible()) return;
    m_hoverChapterId = chapterId;
    // Com a ficha já aberta, trocar de capítulo troca na hora.
    if (m_hoverCard->isVisible()) showHoverCard();
    else m_hoverTimer->start();
}

void ManuscriptPanel::disarmHover() {
    if (m_hoverTimer) m_hoverTimer->stop();
    if (m_hoverCard) m_hoverCard->hide();
    m_hoverChapterId.clear();
}

void ManuscriptPanel::showHoverCard() {
    if (!isVisible() || m_hoverChapterId.isEmpty() || !m_model) return;
    const Chapter* c = m_model->findChapter(m_hoverChapterId);
    if (!c) return;
    const QString muted = tcol(Theme::textMuted()).name();
    const QString bright = tcol(Theme::textBright()).name();

    QString title = m_model->chapterDisplayLabel(*c);
    if (!c->title.isEmpty()) title += QStringLiteral(" · ") + c->title;
    QString html = QStringLiteral("<div style=\"font-family:'Lora'; font-size:14px; font-weight:600; color:%1\">%2</div>")
        .arg(bright, title.toHtmlEscaped());
    if (statusVisible() && !c->status.isEmpty()) {
        html += QStringLiteral("<div style=\"margin-top:3px; font-size:11px\">%1 %2</div>")
            .arg(dotImgTag(QColor(ProjectModel::findWorkStatusColor(c->status)), 8),
                 ProjectModel::findWorkStatusLabel(c->status).toHtmlEscaped());
    }
    QStringList meta;
    if (!c->timeMarker.trimmed().isEmpty()) meta << tr("Quando: %1").arg(c->timeMarker.toHtmlEscaped());
    const QString pov = povOf(*c);
    if (!pov.isEmpty()) meta << tr("POV: %1").arg(elementName(pov).toHtmlEscaped());
    meta << tr("%1 palavras").arg(MsPaint::fmtInt(chapterWords(c->id)));
    meta << tr("%n cenas", "", qMax(1, int(c->scenes.size())));
    html += QStringLiteral("<div style=\"margin-top:5px; color:%1; font-size:11px\">%2</div>")
        .arg(muted, meta.join(QStringLiteral(" · ")));
    html += c->summary.trimmed().isEmpty()
        ? QStringLiteral("<div style=\"margin-top:7px; color:%1; font-style:italic\">%2</div>").arg(muted, tr("Sem resumo ainda."))
        : QStringLiteral("<div style=\"margin-top:7px; font-family:'Lora'; font-size:12.5px\">%1</div>")
              .arg(c->summary.trimmed().toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
    QStringList foot;
    if (m_elementsStore) {
        QStringList names;
        const QStringList ids = m_elementsStore->docElementIds(
            ElementsStore::elementDocKeyForChapter(c->manuscriptId, c->id));
        for (const QString& id : ids) {
            const Element* e = m_elementsStore->findElement(id);
            if (e && e->type == QStringLiteral("character")) names << e->name;
        }
        if (!names.isEmpty()) {
            const int more = names.size() - 6;
            QString s = names.mid(0, 6).join(QStringLiteral(", "));
            if (more > 0) s += QStringLiteral(" +%1").arg(more);
            foot << tr("Aparecem: %1").arg(s.toHtmlEscaped());
        }
    }
    if (chapterWords(c->id) > 0 && m_dialogueStore)
        foot << tr("diálogo %1%").arg(qRound(dialogueFraction(c->id) * 100));
    if (!foot.isEmpty())
        html += QStringLiteral("<div style=\"margin-top:8px; color:%1; font-size:11px\">%2</div>")
            .arg(muted, foot.join(QStringLiteral(" · ")));
    m_hoverCard->setText(html);
    m_hoverCard->adjustSize();

    // Ao lado da gaveta, na altura do capítulo — do lado que tiver espaço.
    const QRect panelG(mapToGlobal(QPoint(0, 0)), size());
    QScreen* scr = screen();
    const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 4000, 4000);
    int x = panelG.right() + 8;
    if (x + m_hoverCard->width() > avail.right()) x = panelG.left() - 8 - m_hoverCard->width();
    int y = qBound(avail.top() + 4, m_hoverRect.top() - 6, avail.bottom() - m_hoverCard->height() - 4);
    const bool wasShown = m_hoverCard->isVisible();
    m_hoverCard->move(x, y);
    m_hoverCard->show();
    if (!wasShown) PanelMotion::popIn(m_hoverCard);
    m_hoverCard->raise();
}

// ============================================================ menus

void ManuscriptPanel::showManuscriptContextMenu(const QString& manuscriptId, const QPoint& globalPos) {
    QMenu menu(this);
    menu.setStyleSheet(contextMenuQss());

    auto* renameAct = menu.addAction(tr("Renomear manuscrito"));
    connect(renameAct, &QAction::triggered, this, [this, manuscriptId]() {
        emit renameManuscriptRequested(manuscriptId);
    });

    auto* previewAct = menu.addAction(tr("Visualizar como e-reader"));
    connect(previewAct, &QAction::triggered, this, [this, manuscriptId]() {
        emit previewEreaderRequested(manuscriptId);
    });

    // Cor da lombada / da capa gerada (Lombadas, Vitrine, Trilho).
    auto* colorAct = menu.addAction(tr("Cor do livro…"));
    connect(colorAct, &QAction::triggered, this, [this, manuscriptId]() {
        const QColor c = ColorPopover::getColor(MsPaint::bookColor(manuscriptId), this, tr("Cor do livro"));
        if (!c.isValid() || !m_model) return;
        QJsonObject s = m_model->settings();
        QJsonObject o = s.value(QStringLiteral("manuscriptColors")).toObject();
        o.insert(manuscriptId, c.name());
        s.insert(QStringLiteral("manuscriptColors"), o);
        m_model->setSettings(s);
    });
    if (m_model && m_model->settings().value(QStringLiteral("manuscriptColors")).toObject().contains(manuscriptId)) {
        auto* autoAct = menu.addAction(tr("Cor automática"));
        connect(autoAct, &QAction::triggered, this, [this, manuscriptId]() {
            QJsonObject s = m_model->settings();
            QJsonObject o = s.value(QStringLiteral("manuscriptColors")).toObject();
            o.remove(manuscriptId);
            s.insert(QStringLiteral("manuscriptColors"), o);
            m_model->setSettings(s);
        });
    }

    if (m_style == Style::Reader) {
        auto* resetAct = menu.addAction(tr("Começar nova revisão"));
        resetAct->setToolTip(tr("Zera o quanto de cada capítulo você já passou"));
        connect(resetAct, &QAction::triggered, this, [this, manuscriptId]() { emit resetVisitedRequested(manuscriptId); });
    }

    menu.addSeparator();

    auto* deleteAct = menu.addAction(tr("Excluir manuscrito"));
    connect(deleteAct, &QAction::triggered, this, [this, manuscriptId]() {
        emit deleteManuscriptRequested(manuscriptId);
    });

    PanelMotion::animateMenu(&menu);
    menu.exec(globalPos);
}

void ManuscriptPanel::showChapterContextMenu(const QString& manuscriptId, const QString& chapterId, const QPoint& globalPos) {
    disarmHover();
    QMenu menu(this);
    menu.setStyleSheet(contextMenuQss());
    const Chapter* ch = m_model ? m_model->findChapter(chapterId) : nullptr;

    if (ch && (m_style == Style::Illustrated || m_style == Style::Seasons)) {
        auto* vigAct = menu.addAction(tr("Trocar desenho…"));
        connect(vigAct, &QAction::triggered, this, [this, chapterId, globalPos]() {
            showVignettePicker(chapterId, globalPos);
        });
        menu.addSeparator();
    }

    auto* exportAct = menu.addAction(tr("Exportar para DOCX…"));
    exportAct->setEnabled(false);
    exportAct->setToolTip(tr("Em breve"));

    auto* renameAct = menu.addAction(tr("Renomear capítulo"));
    connect(renameAct, &QAction::triggered, this, [this, chapterId]() {
        emit renameChapterRequested(chapterId);
    });

    auto* elemsAct = menu.addAction(tr("Elementos presentes…"));
    connect(elemsAct, &QAction::triggered, this, [this, manuscriptId, chapterId]() {
        emit elementsPresentRequested(manuscriptId, chapterId);
    });

    auto* refAct = menu.addAction(tr("Abrir no Menu de Referência"));
    connect(refAct, &QAction::triggered, this, [this, manuscriptId, chapterId]() {
        emit openChapterInRefMenuRequested(manuscriptId, chapterId);
    });

    // Ferramentas ligadas ganham o seu pedaço do menu.
    if (ch && (statusVisible() || tool(ToolPov) || partsActive())) menu.addSeparator();
    if (ch && statusVisible()) {
        QMenu* sm = menu.addMenu(tr("Status"));
        sm->setStyleSheet(contextMenuQss());
        auto* group = new QActionGroup(sm);
        QList<QPair<QString, QString>> opts;
        opts.append({ QString(), tr("Sem status") });
        for (const auto& ws : ProjectModel::workStatuses()) opts.append({ ws.id, ws.label });
        for (const auto& o : opts) {
            QAction* a = sm->addAction(o.second);
            a->setCheckable(true);
            a->setChecked(ch->status == o.first);
            group->addAction(a);
            const QString id = o.first;
            connect(a, &QAction::triggered, this, [this, chapterId, id]() { m_model->updateChapterStatus(chapterId, id); });
        }
    }
    if (ch && tool(ToolPov) && m_elementsStore) {
        QMenu* pm = menu.addMenu(tr("Narrador"));
        pm->setStyleSheet(contextMenuQss());
        auto* group = new QActionGroup(pm);
        const QString current = povOf(*ch);
        auto addPov = [&](const QString& id, const QString& label) {
            QAction* a = pm->addAction(label);
            a->setCheckable(true);
            a->setChecked(current == id);
            group->addAction(a);
            connect(a, &QAction::triggered, this, [this, chapterId, id]() { m_model->updateChapterPov(chapterId, id); });
        };
        const QString sug = suggestedPov(*ch);
        if (!sug.isEmpty()) {
            addPov(sug, tr("%1 (sugerido)").arg(elementName(sug)));
            pm->addSeparator();
        }
        QList<Element> chars;
        for (const auto& e : m_elementsStore->elements())
            if (e.type == QStringLiteral("character") && e.id != sug) chars.append(e);
        std::sort(chars.begin(), chars.end(), [](const Element& a, const Element& b) {
            return a.name.localeAwareCompare(b.name) < 0;
        });
        for (const auto& e : chars) addPov(e.id, e.name);
        if (sug.isEmpty() && chars.isEmpty()) {
            QAction* none = pm->addAction(tr("Nenhum personagem cadastrado"));
            none->setEnabled(false);
        }
        pm->addSeparator();
        addPov(QString(), tr("Sem narrador definido"));
    }
    if (ch && partsActive()) {
        bool isStart = false;
        for (const auto& p : validParts(readingChapters())) if (p.startChapterId == chapterId) isStart = true;
        if (isStart) {
            connect(menu.addAction(tr("Tirar o começo de parte daqui")), &QAction::triggered, this,
                    [this, chapterId]() { removePartStart(chapterId); });
        } else {
            connect(menu.addAction(tr("Começar uma parte aqui…")), &QAction::triggered, this,
                    [this, chapterId]() { startPartAt(chapterId); });
        }
    }

    menu.addSeparator();

    auto* deleteAct = menu.addAction(tr("Excluir capítulo"));
    connect(deleteAct, &QAction::triggered, this, [this, chapterId]() {
        emit deleteChapterRequested(chapterId);
    });

    PanelMotion::animateMenu(&menu);
    menu.exec(globalPos);
}

void ManuscriptPanel::showSceneContextMenu(const QString& manuscriptId, const QString& chapterId, int sceneIndex, const QPoint& globalPos) {
    disarmHover();
    QMenu menu(this);
    menu.setStyleSheet(contextMenuQss());

    auto* renameAct = menu.addAction(tr("Renomear cena"));
    connect(renameAct, &QAction::triggered, this, [this, chapterId, sceneIndex]() {
        emit renameSceneRequested(chapterId, sceneIndex);
    });

    auto* elemsAct = menu.addAction(tr("Elementos presentes…"));
    connect(elemsAct, &QAction::triggered, this, [this, manuscriptId, chapterId, sceneIndex]() {
        emit sceneElementsPresentRequested(manuscriptId, chapterId, sceneIndex);
    });

    auto* varAct = menu.addAction(tr("Criar variação"));
    connect(varAct, &QAction::triggered, this, [this, chapterId, sceneIndex]() {
        emit createVariationRequested(chapterId, sceneIndex);
    });

    // Variações à vista: trocar a versão ativa daqui mesmo.
    const Scene* sc = m_model ? m_model->findScene(chapterId, sceneIndex) : nullptr;
    if (tool(ToolVariations) && sc && sc->variations.size() >= 2) {
        QMenu* vm = menu.addMenu(tr("Versão ativa"));
        vm->setStyleSheet(contextMenuQss());
        auto* group = new QActionGroup(vm);
        for (const auto& v : sc->variations) {
            QAction* a = vm->addAction(v.label.isEmpty() ? tr("(sem nome)") : v.label);
            a->setCheckable(true);
            a->setChecked(v.id == sc->activeVariationId);
            group->addAction(a);
            const QString vid = v.id;
            connect(a, &QAction::triggered, this, [this, manuscriptId, chapterId, sceneIndex, vid]() {
                emit switchVariationRequested(manuscriptId, chapterId, sceneIndex, vid);
            });
        }
    }

    auto* refAct = menu.addAction(tr("Abrir no Menu de Referência"));
    connect(refAct, &QAction::triggered, this, [this, manuscriptId, chapterId, sceneIndex]() {
        emit openSceneInRefMenuRequested(manuscriptId, chapterId, sceneIndex);
    });

    menu.addSeparator();

    auto* deleteAct = menu.addAction(tr("Excluir cena"));
    connect(deleteAct, &QAction::triggered, this, [this, chapterId, sceneIndex]() {
        emit deleteSceneRequested(chapterId, sceneIndex);
    });

    PanelMotion::animateMenu(&menu);
    menu.exec(globalPos);
}

void ManuscriptPanel::showPartContextMenu(const QString& partId, const QPoint& globalPos) {
    const Manuscript* m = m_model ? m_model->findManuscript(activeManuscriptId()) : nullptr;
    if (!m) return;
    QMenu menu(this);
    menu.setStyleSheet(contextMenuQss());
    connect(menu.addAction(tr("Renomear parte…")), &QAction::triggered, this, [this, partId]() {
        const Manuscript* mm = m_model->findManuscript(activeManuscriptId());
        if (!mm) return;
        QList<ManuscriptPart> parts = mm->parts;
        for (auto& p : parts) {
            if (p.id != partId) continue;
            bool ok = false;
            const QString t = QInputDialog::getText(this, tr("Renomear parte"), tr("Nome da parte:"),
                                                    QLineEdit::Normal, p.title, &ok).trimmed();
            if (!ok || t.isEmpty()) return;
            p.title = t;
        }
        setParts(parts);
    });
    connect(menu.addAction(tr("Cor…")), &QAction::triggered, this, [this, partId]() {
        const Manuscript* mm = m_model->findManuscript(activeManuscriptId());
        if (!mm) return;
        QList<ManuscriptPart> parts = mm->parts;
        for (auto& p : parts) {
            if (p.id != partId) continue;
            const QColor c = ColorPopover::getColor(QColor(p.color), this, tr("Cor da parte"));
            if (!c.isValid()) return;
            p.color = c.name();
        }
        setParts(parts);
    });
    menu.addSeparator();
    connect(menu.addAction(tr("Desfazer a parte (os capítulos ficam)")), &QAction::triggered, this, [this, partId]() {
        const Manuscript* mm = m_model->findManuscript(activeManuscriptId());
        if (!mm) return;
        QList<ManuscriptPart> parts = mm->parts;
        parts.erase(std::remove_if(parts.begin(), parts.end(),
            [&](const ManuscriptPart& p) { return p.id == partId; }), parts.end());
        m_collapsedParts.remove(partId);
        setParts(parts);
    });
    PanelMotion::animateMenu(&menu);
    menu.exec(globalPos);
}

// ---- Drag&drop ----------------------------------------------------------

// ---- Drag&drop ----------------------------------------------------------

bool ManuscriptPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_resizeHandle || watched == m_resizeHandleBottom || watched == m_resizeHandleCorner) {
        auto* me = static_cast<QMouseEvent*>(event);
        const int axes = static_cast<QWidget*>(watched)->property("resizeAxes").toInt();
        if (event->type() == QEvent::MouseButtonDblClick && (axes & 2)) {
            // Volta pra altura toda.
            m_desiredHeight = 0;
            QSettings().remove(QStringLiteral("ui/manuscriptPanel/height"));
            emit widthChanged();   // o MainWindow reposiciona (e devolve a altura cheia)
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
            m_resizeAxes = axes;
            m_resizeStartX = int(me->globalPosition().x());
            m_resizeStartY = int(me->globalPosition().y());
            m_resizeStartW = width();
            m_resizeStartH = height();
            return true;
        }
        if (event->type() == QEvent::MouseMove && m_resizeAxes) {
            if (m_resizeAxes & 1) {
                const int w = qBound(240, m_resizeStartW + int(me->globalPosition().x()) - m_resizeStartX, 700);
                if (w != width()) { setFixedWidth(w); emit widthChanged(); }
            }
            if (m_resizeAxes & 2) {
                int maxH = QWIDGETSIZE_MAX;
                if (parentWidget()) maxH = parentWidget()->height() - y() - 10;
                const int h = qBound(220, m_resizeStartH + int(me->globalPosition().y()) - m_resizeStartY, qMax(220, maxH));
                if (h != height()) { m_desiredHeight = h; resize(width(), h); }
            }
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease && m_resizeAxes) {
            if (m_resizeAxes & 1)
                QSettings().setValue(QStringLiteral("ui/manuscriptPanel/width-") + styleId(m_style), width());
            if (m_resizeAxes & 2)
                QSettings().setValue(QStringLiteral("ui/manuscriptPanel/height"), m_desiredHeight);
            m_resizeAxes = 0;
            rebuildList();
            return true;
        }
    }
    // Ficha no hover: qualquer linha de capítulo, em qualquer estilo.
    if (auto* hw = qobject_cast<QWidget*>(watched)) {
        const QVariant hid = hw->property("hoverChapterId");
        if (hid.isValid()) {
            if (event->type() == QEvent::Enter)
                armHover(hid.toString(), QRect(hw->mapToGlobal(QPoint(0, 0)), hw->size()));
            else if (event->type() == QEvent::Leave || event->type() == QEvent::MouseButtonPress)
                disarmHover();
        }
    }
    // Pilula de diálogo/narração: não é QToolButton (é a DialogueRatioBar,
    // sem Q_OBJECT de propósito), então é pega por propriedade em vez de
    // qobject_cast — checada ANTES do branch de QToolButton abaixo, que é
    // pros botões de capítulo/cena de verdade.
    if (event->type() == QEvent::MouseButtonPress) {
        if (auto* w = qobject_cast<QWidget*>(watched)) {
            const QVariant chId = w->property("ratioBarChapterId");
            if (chId.isValid()) {
                auto* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    QPoint anchor = w->mapToGlobal(QPoint(0, 0));
                    anchor.setX(mapToGlobal(QPoint(width(), 0)).x());
                    emit chapterStatsRequested(w->property("ratioBarManuscriptId").toString(),
                                               chId.toString(), anchor);
                }
                return true;
            }
        }
    }

    // Hover num item do popup ABERTO do combo de manuscritos — dispara a
    // tooltip rica (ProjectInfoHover) do manuscrito sob o mouse.
    if (watched == m_combo->view()) {
        if (event->type() == QEvent::Hide) { // popup fechou
            m_comboHoverOpenTimer->stop();
            m_comboHoverCloseTimer->stop();
            m_comboHoverPendingManuscriptId.clear();
            if (m_comboItemHover) m_comboItemHover->hide();
        }
        return QWidget::eventFilter(watched, event);
    }
    if (watched == m_combo->view()->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            const QModelIndex idx = m_combo->view()->indexAt(me->pos());
            const QString msId = idx.isValid() ? idx.data(Qt::UserRole).toString() : QString();
            if (msId != m_comboHoverPendingManuscriptId) {
                m_comboHoverPendingManuscriptId = msId;
                m_comboHoverOpenTimer->stop();
                m_comboHoverCloseTimer->stop();
                if (!msId.isEmpty()) m_comboHoverOpenTimer->start();
                else m_comboHoverCloseTimer->start();
            }
        } else if (event->type() == QEvent::Leave) {
            m_comboHoverOpenTimer->stop();
            m_comboHoverPendingManuscriptId.clear();
            m_comboHoverCloseTimer->start();
        }
        return QWidget::eventFilter(watched, event); // não engole — combo continua
                                                       // fazendo seu próprio highlight
    }

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
        m_dropIndicator->setStyleSheet(Theme::qss(QStringLiteral(
            "background: %1; border-radius: @radius-control;").arg(Theme::accentDefault())));
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
        // Cena pode cair em cima de outra cena (de qualquer capítulo) ou em
        // cima do próprio capítulo de destino — nesse caso entra no fim dele.
        const QString targetKind = nearest->property("kind").toString();
        if (targetKind != QStringLiteral("scene") && targetKind != QStringLiteral("chapter")) {
            clearDropIndicator();
            event->ignore();
            return;
        }
    } else {
        // Capítulos: alvo precisa ser um botão de capítulo. Na ordem da
        // história não se reordena (arrastar ali não diria o que muda).
        if (storyMode() || nearest->property("kind").toString() != QStringLiteral("chapter")) {
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

        const QString targetKind = nearest->property("kind").toString();
        QString dstChId;
        int tgtIdx = 0;

        if (targetKind == QStringLiteral("scene")) {
            dstChId = nearest->property("chapterId").toString();
            tgtIdx = nearest->property("sceneIndex").toInt();
            if (!before) tgtIdx += 1;
        } else if (targetKind == QStringLiteral("chapter")) {
            // Soltar sobre o cabeçalho do capítulo = mandar a cena pro fim
            // dele. Contar as cenas visíveis é o jeito de saber onde é "o fim"
            // sem o painel precisar consultar o modelo.
            dstChId = nearest->property("chapterId").toString();
            if (dstChId == srcChId) return; // arrastar pro próprio cabeçalho não faz nada
            int sceneCount = 0;
            for (int i = 0; i < m_listLayout->count(); ++i) {
                QLayoutItem* it = m_listLayout->itemAt(i);
                if (!it || !it->widget()) continue;
                QWidget* w = it->widget();
                if (w->property("kind").toString() == QStringLiteral("scene") &&
                    w->property("chapterId").toString() == dstChId) ++sceneCount;
            }
            tgtIdx = sceneCount;
        } else {
            return;
        }

        if (dstChId == srcChId) {
            if (tgtIdx == srcIdx || tgtIdx == srcIdx + 1) { event->acceptProposedAction(); return; }
            emit reorderSceneRequested(srcChId, srcIdx, tgtIdx);
        } else {
            emit moveSceneToChapterRequested(srcChId, srcIdx, dstChId, tgtIdx);
        }
        event->acceptProposedAction();
        return;
    }

    if (event->mimeData()->hasFormat(QLatin1String(kChapterMime))) {
        const QString srcChId = QString::fromUtf8(event->mimeData()->data(QLatin1String(kChapterMime)));
        if (storyMode() || nearest->property("kind").toString() != QStringLiteral("chapter")) return;
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
        // A parte começa no capítulo arrastado: o começo passa pro seguinte,
        // senão a parte inteira iria junto pro lugar novo.
        if (const Manuscript* m = m_model ? m_model->findManuscript(activeManuscriptId()) : nullptr) {
            QList<ManuscriptPart> parts = m->parts;
            const QList<Chapter> reading = readingChapters();
            QSet<QString> starts;
            for (const auto& p : parts) starts.insert(p.startChapterId);
            bool changed = false;
            for (auto& p : parts) {
                if (p.startChapterId != srcChId) continue;
                for (int i = 0; i + 1 < reading.size(); ++i) {
                    if (reading.at(i).id != srcChId) continue;
                    const QString next = reading.at(i + 1).id;
                    if (!starts.contains(next)) { p.startChapterId = next; changed = true; }
                }
            }
            if (changed) setParts(parts);
        }
        emit reorderChapterRequested(srcChId, tgtIdx);
        event->acceptProposedAction();
        return;
    }
}

// ============================================================ Folha de rosto, Leitor,
// Índice ilustrado, Temporadas e Página da loja

namespace {

// Quebra em até maxLines linhas; a última ganha reticências se sobrar texto.
QStringList wrapLines(const QFontMetrics& fm, const QString& text, int width, int maxLines) {
    QStringList out;
    const QStringList words = text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString line;
    int i = 0;
    for (; i < words.size(); ++i) {
        const QString tryLine = line.isEmpty() ? words.at(i) : line + QLatin1Char(' ') + words.at(i);
        if (fm.horizontalAdvance(tryLine) <= width || line.isEmpty()) { line = tryLine; continue; }
        out << line;
        line = words.at(i);
        if (out.size() == maxLines) break;
    }
    if (out.size() < maxLines && !line.isEmpty()) { out << line; line.clear(); ++i; }
    if (out.size() == maxLines && i < words.size()) {
        QString last = out.last();
        for (int k = i; k < words.size() && fm.horizontalAdvance(last) < width * 2; ++k) last += QLatin1Char(' ') + words.at(k);
        out.last() = fm.elidedText(last, Qt::ElideRight, width);
    } else if (!out.isEmpty()) {
        out.last() = fm.elidedText(out.last(), Qt::ElideRight, width);
    }
    return out;
}

// Mini capa da saga (Folha de rosto e Página da loja): gradiente na cor do livro.
QPixmap miniCover(ProjectModel* model, const Manuscript& m, int number, QSize size, qreal dpr, bool showNumber) {
    const QPixmap real = CoverUtils::pixmapFromDataUrl(model->manuscriptEffectiveCoverDataUrl(m.id));
    QPixmap out(size * dpr);
    out.setDevicePixelRatio(dpr);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF r(0, 0, size.width(), size.height());
    QPainterPath clip;
    clip.addRoundedRect(r, 2.5, 2.5);
    p.setClipPath(clip);
    if (!real.isNull()) {
        QPixmap sc = real.scaled(size * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        sc.setDevicePixelRatio(dpr);
        p.drawPixmap(QPointF((r.width() - sc.width() / dpr) / 2.0, (r.height() - sc.height() / dpr) / 2.0), sc);
    } else {
        const QColor c = MsPaint::bookColor(m.id);
        QLinearGradient g(r.topLeft(), r.bottomRight());
        g.setColorAt(0, QColor::fromHslF(c.hslHueF() < 0 ? 0.08f : c.hslHueF(), 0.35f, 0.40f));
        g.setColorAt(1, QColor::fromHslF(c.hslHueF() < 0 ? 0.16f : std::fmod(c.hslHueF() + 30.0f / 360.0f, 1.0f), 0.35f, 0.15f));
        p.fillRect(r, g);
    }
    p.fillRect(QRectF(0, 0, 2, r.height()), QColor(0, 0, 0, 70));
    if (showNumber) {
        p.setPen(QColor(255, 255, 255, 200));
        p.setFont(serifFont(std::max(8.0, size.height() / 4.0), QFont::DemiBold));
        p.drawText(r.adjusted(0, 0, 0, -2), Qt::AlignHCenter | Qt::AlignBottom, QString::number(number));
    }
    return out;
}

QString dataUrlFromImage(const QImage& src, int maxSide) {
    QImage img = src;
    if (std::max(img.width(), img.height()) > maxSide)
        img = img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.convertToFormat(QImage::Format_RGB32).save(&buf, "JPEG", 86);
    return QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(bytes.toBase64());
}

}  // namespace

void ManuscriptPanel::selectManuscript(const QString& id) {
    const int idx = m_combo->findData(id);
    if (idx >= 0) m_combo->setCurrentIndex(idx);
}

QString ManuscriptPanel::scenesText(const Chapter& c) const {
    if (chapterWords(c.id) <= 0) return tr("vazio");
    const int n = std::max<int>(1, c.scenes.size());
    return tr("%n cena(s)", "", n);
}

bool ManuscriptPanel::lastResumeHere(ResumeEntry* out) const {
    const QString msId = activeManuscriptId();
    for (const auto& e : m_resume) {
        if (e.manuscriptId != msId || !m_model->findChapter(e.chapterId)) continue;
        if (out) *out = e;
        return true;
    }
    return false;
}

QHash<QString, int> ManuscriptPanel::pageStarts(const QList<Chapter>& reading) const {
    QHash<QString, int> out;
    int page = 1;
    for (const auto& c : reading) {
        out.insert(c.id, page);
        page += std::max(1, (chapterWords(c.id) + 249) / 250);
    }
    return out;
}

QHash<QString, int> ManuscriptPanel::vignetteFamilies() const {
    const QList<Chapter> reading = readingChapters();
    QStringList ids, own;
    for (const auto& c : reading) { ids << c.id; own << c.vignette; }
    const Manuscript* m = m_model->findManuscript(activeManuscriptId());
    const QList<int> fams = MsVignette::familiesForBook(ids, own, m ? m->vignette : QString());
    QHash<QString, int> out;
    for (int i = 0; i < ids.size(); ++i) out.insert(ids.at(i), fams.at(i));
    return out;
}

QColor ManuscriptPanel::vignetteColor(const QString& chapterId) const {
    // Cor da Parte; sem partes, a cor do livro.
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = validParts(reading);
    if (!parts.isEmpty()) {
        const int pi = partIndexByChapter(reading, parts).value(chapterId, -1);
        if (pi >= 0 && QColor(parts.at(pi).color).isValid()) return QColor(parts.at(pi).color);
    }
    return MsPaint::bookColor(activeManuscriptId());
}

QPixmap ManuscriptPanel::chapterVignette(const Chapter& c, int family, QSize size, bool circle) const {
    const QColor col = vignetteColor(c.id);
    if (!c.vignetteImage.isEmpty()) {
        const QPixmap img = CoverUtils::pixmapFromDataUrl(c.vignetteImage);
        if (!img.isNull()) return MsVignette::renderImage(img, col, size, devicePixelRatioF(), circle);
    }
    return MsVignette::render(c.id, chapterWords(c.id), col, family, size, devicePixelRatioF(), circle);
}

QWidget* ManuscriptPanel::makeSagaMinis(QWidget* parent, QSize size, bool withLabel) {
    const auto& mss = m_model->manuscripts();
    if (mss.size() < 2) return nullptr;
    auto* w = new QWidget(parent);
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 4, 0, 0);
    lay->setSpacing(size.width() > 30 ? 8 : 6);
    const QString active = activeManuscriptId();
    const qreal dpr = devicePixelRatioF();
    for (int i = 0; i < mss.size(); ++i) {
        const Manuscript& m = mss.at(i);
        const bool on = (m.id == active);
        const QPixmap pm = miniCover(m_model, m, i + 1, size, dpr, true);
        auto* b = new MsRow(w);
        b->setFixedSize(size.width() + 6, size.height() + 7);
        b->setToolTip(m.title.isEmpty() ? tr("(sem título)") : m.title);
        b->setPainter([pm, on, size](QPainter& p, const QRect& r, const MsRow* self) {
            const QPointF at(r.left() + 3, r.top() + (on ? 1 : 4));
            p.setOpacity(on || self->isHovered() ? 1.0 : 0.72);
            p.drawPixmap(at, pm);
            p.setOpacity(1.0);
            if (on) {
                p.setRenderHint(QPainter::Antialiasing);
                p.setPen(QPen(tcol(Theme::textBright()), 1.4));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(QRectF(at.x() - 2, at.y() - 2, size.width() + 4, size.height() + 4), 3, 3);
            }
        });
        const QString id = m.id;
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, &QToolButton::clicked, this, [this, id]() { selectManuscript(id); });
        connect(b, &QToolButton::customContextMenuRequested, this, [this, b, id](const QPoint& pos) {
            showManuscriptContextMenu(id, b->mapToGlobal(pos));
        });
        lay->addWidget(b, 0, Qt::AlignBottom);
    }
    if (withLabel) {
        auto* lb = mutedLabel(tr("a saga"), w, 10.5);
        lb->setWordWrap(false);
        lay->addSpacing(4);
        lay->addWidget(lb, 0, Qt::AlignVCenter);
    }
    lay->addStretch(1);
    return w;
}

// ---- Folha de rosto ----

QWidget* ManuscriptPanel::makeTitlePageHead() {
    const QString msId = activeManuscriptId();
    const Manuscript* m = m_model->findManuscript(msId);
    if (!m) return nullptr;
    const auto& mss = m_model->manuscripts();
    int number = 1;
    for (int i = 0; i < mss.size(); ++i) if (mss.at(i).id == msId) number = i + 1;

    auto* w = new QWidget(this);
    w->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(w, &QWidget::customContextMenuRequested, this, [this, w, msId](const QPoint& pos) {
        showManuscriptContextMenu(msId, w->mapToGlobal(pos));
    });
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(6, 10, 6, 4);
    v->setSpacing(8);
    auto* top = new QHBoxLayout;
    top->setSpacing(14);
    auto* cov = new QLabel(w);
    cov->setPixmap(coverPixmap(m_model, *m, number, QSize(70, 100), devicePixelRatioF(), tr("Livro")));
    cov->setFixedSize(70, 100);
    top->addWidget(cov, 0, Qt::AlignBottom);
    auto* txt = new QVBoxLayout;
    txt->setSpacing(4);
    txt->addStretch(1);
    auto* ti = new QLabel(tr("LIVRO %1 DE %2").arg(number).arg(mss.size()), w);
    QFont tf = uiFont(9, QFont::DemiBold);
    tf.setLetterSpacing(QFont::AbsoluteSpacing, 2.6);
    ti->setFont(tf);
    ti->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textMuted()));
    txt->addWidget(ti);
    auto* tt = new QLabel(m->title.isEmpty() ? tr("(sem título)") : m->title, w);
    tt->setWordWrap(true);
    tt->setFont(serifFont(21, QFont::Normal, true));
    tt->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textBright()));
    txt->addWidget(tt);
    const QString author = m_model->projectAuthor();
    if (!author.isEmpty()) {
        auto* au = new QLabel(author, w);
        QFont af = serifFont(12);
        af.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
        au->setFont(af);
        au->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textPrimary()));
        txt->addWidget(au);
    }
    top->addLayout(txt, 1);
    v->addLayout(top);
    auto* line = new QWidget(w);
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral("background: %1;").arg(Theme::subtleBorder()));
    v->addWidget(line);
    if (QWidget* minis = makeSagaMinis(w, QSize(22, 32), true)) {
        v->addWidget(minis);
        auto* line2 = new QWidget(w);
        line2->setFixedHeight(1);
        line2->setStyleSheet(QStringLiteral("background: %1;").arg(Theme::subtleBorder()));
        v->addWidget(line2);
    }
    return w;
}

void ManuscriptPanel::buildTitlePageView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QList<Chapter> reading = readingChapters();
    const QHash<QString, int> pages = pageStarts(reading);
    {
        auto* hd = new QLabel(tr("SUMÁRIO"), this);
        QFont f = uiFont(9.5, QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 3.4);
        hd->setFont(f);
        hd->setAlignment(Qt::AlignCenter);
        hd->setContentsMargins(0, 8, 0, 6);
        hd->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textMuted()));
        add(hd);
    }
    const bool status = statusVisible();
    for (const auto& c : chs) {
        const bool special = (c.type != QStringLiteral("chapter"));
        const QString num = special ? QString() : m_model->chapterNumberLabel(c);
        const QString title = special
            ? m_model->chapterTypeName(c) + (c.title.isEmpty() ? QString() : QStringLiteral(" · ") + c.title)
            : chapterTitleOnly(c);
        const QString page = QString::number(pages.value(c.id, 1));
        const bool cur = isCurrent(c.id, -1);
        const bool dim = !m_povFilter.isEmpty() && povOf(c) != m_povFilter;
        const QColor stC = status ? statusColor(c) : QColor();
        auto* row = new MsRow(this);
        row->setRowHeight(28);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (dim) p.setOpacity(0.4);
            p.setRenderHint(QPainter::Antialiasing);
            if (cur || self->isHovered()) {
                p.setPen(Qt::NoPen);
                p.setBrush(cur ? tcol(Theme::accentInfoSoft()) : tcol(Theme::hoverOverlay()));
                p.drawRoundedRect(QRectF(r).adjusted(4, 1, -4, -1), 4, 4);
            }
            int x = r.left() + 8;
            if (stC.isValid()) {
                p.setPen(Qt::NoPen);
                p.setBrush(stC);
                p.drawEllipse(QPointF(x + 2, r.center().y() + 0.5), 2.5, 2.5);
            }
            x += 6;
            p.setFont(serifFont(12));
            p.setPen(tcol(Theme::textMuted()));
            p.drawText(QRect(x, r.top(), 18, r.height()), Qt::AlignVCenter | Qt::AlignRight, num);
            x += 26;
            const int pw = p.fontMetrics().horizontalAdvance(page);
            const int rightX = r.right() - 10 - pw;
            p.drawText(QRect(rightX, r.top(), pw + 2, r.height()), Qt::AlignVCenter | Qt::AlignRight, page);
            p.setFont(serifFont(14, QFont::Normal, special));
            p.setPen(cur || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
            const QString t = p.fontMetrics().elidedText(title, Qt::ElideRight, rightX - x - 18);
            const int tw = p.fontMetrics().horizontalAdvance(t);
            p.drawText(QRect(x, r.top(), rightX - x, r.height()), Qt::AlignVCenter | Qt::AlignLeft, t);
            QColor dc = tcol(Theme::textMuted());
            dc.setAlphaF(0.55);
            p.setPen(QPen(dc, 1, Qt::DotLine));
            const int by = r.center().y() + 5;
            if (rightX - 6 > x + tw + 8) p.drawLine(x + tw + 8, by, rightX - 6, by);
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString chapterId = c.id, manuscriptId = c.manuscriptId;
        connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
            showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chapterId);
        add(row);

        // Cenas do capítulo aberto, cada uma com a página em que começa.
        if (c.id == m_curChapterId && c.scenes.size() > 1) {
            int sp = pages.value(c.id, 1);
            for (int j = 0; j < c.scenes.size(); ++j) {
                const QString st = sceneTitle(c, j);
                const QString spage = QString::number(sp);
                sp += std::max(1, (sceneWords(c.id, j) + 249) / 250);
                const bool scur = isCurrent(c.id, j);
                auto* srow = new MsRow(this);
                srow->setRowHeight(24);
                srow->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
                    p.setRenderHint(QPainter::Antialiasing);
                    if (scur || self->isHovered()) {
                        p.setPen(Qt::NoPen);
                        p.setBrush(scur ? tcol(Theme::accentInfoSoft()) : tcol(Theme::hoverOverlay()));
                        p.drawRoundedRect(QRectF(r).adjusted(4, 1, -4, -1), 4, 4);
                    }
                    const int x = r.left() + 48;
                    p.setFont(serifFont(12));
                    p.setPen(tcol(Theme::textMuted()));
                    const int pw = p.fontMetrics().horizontalAdvance(spage);
                    const int rightX = r.right() - 10 - pw;
                    p.drawText(QRect(rightX, r.top(), pw + 2, r.height()), Qt::AlignVCenter | Qt::AlignRight, spage);
                    p.setFont(serifFont(12.5, QFont::Normal, true));
                    p.setPen(scur || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textMuted()));
                    const QString t = p.fontMetrics().elidedText(st, Qt::ElideRight, rightX - x - 16);
                    const int tw = p.fontMetrics().horizontalAdvance(t);
                    p.drawText(QRect(x, r.top(), rightX - x, r.height()), Qt::AlignVCenter | Qt::AlignLeft, t);
                    QColor dc = tcol(Theme::textMuted());
                    dc.setAlphaF(0.45);
                    p.setPen(QPen(dc, 1, Qt::DotLine));
                    const int by = r.center().y() + 5;
                    if (rightX - 6 > x + tw + 8) p.drawLine(x + tw + 8, by, rightX - 6, by);
                });
                srow->setContextMenuPolicy(Qt::CustomContextMenu);
                connect(srow, &QToolButton::clicked, this, [this, manuscriptId, chapterId, j]() { emit sceneActivated(manuscriptId, chapterId, j); });
                connect(srow, &QToolButton::customContextMenuRequested, this, [this, srow, manuscriptId, chapterId, j](const QPoint& pos) {
                    showSceneContextMenu(manuscriptId, chapterId, j, srow->mapToGlobal(pos));
                });
                wireRow(srow, QStringLiteral("scene"), chapterId, j);
                add(srow);
            }
        }
    }
    int total = 0;
    for (const auto& c : reading) total += chapterWords(c.id);
    auto* foot = new QLabel(tr("%1 palavras · cerca de %n lauda(s)", "", std::max(1, (total + 249) / 250))
                                .arg(MsPaint::fmtInt(total)), this);
    foot->setAlignment(Qt::AlignCenter);
    foot->setFont(serifFont(11, QFont::Normal, true));
    foot->setContentsMargins(0, 10, 0, 6);
    foot->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textMuted()));
    add(foot);
}

// ---- Leitor ----

QWidget* ManuscriptPanel::makeReaderTop() {
    const QString msId = activeManuscriptId();
    const Manuscript* m = m_model->findManuscript(msId);
    if (!m) return nullptr;
    const auto& mss = m_model->manuscripts();
    int number = 1;
    for (int i = 0; i < mss.size(); ++i) if (mss.at(i).id == msId) number = i + 1;
    const QList<Chapter> reading = readingChapters();
    int total = 0;
    double seen = 0;
    QList<int> words;
    for (const auto& c : reading) {
        const int w = chapterWords(c.id);
        words << w;
        total += w;
        seen += w * std::clamp(m_visited.value(c.id, 0.0), 0.0, 1.0);
    }
    const int pct = total > 0 ? int(std::round(seen / total * 100)) : 0;

    auto* w = new QWidget(m_top);
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(2, 4, 2, 2);
    v->setSpacing(10);
    auto* top = new QHBoxLayout;
    top->setSpacing(12);
    auto* cov = new QLabel(w);
    cov->setPixmap(coverPixmap(m_model, *m, number, QSize(58, 84), devicePixelRatioF(), tr("Livro")));
    cov->setFixedSize(58, 84);
    cov->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(cov, &QWidget::customContextMenuRequested, this, [this, cov, msId](const QPoint& pos) {
        showManuscriptContextMenu(msId, cov->mapToGlobal(pos));
    });
    top->addWidget(cov, 0, Qt::AlignTop);
    auto* txt = new QVBoxLayout;
    txt->setSpacing(3);
    auto* tt = new QLabel(m->title.isEmpty() ? tr("(sem título)") : m->title, w);
    tt->setWordWrap(true);
    tt->setFont(serifFont(15, QFont::DemiBold));
    tt->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textBright()));
    txt->addWidget(tt);
    const QString author = m_model->projectAuthor();
    const QString sub = author.isEmpty() ? tr("%1 palavras").arg(MsPaint::fmtInt(total))
                                         : tr("%1 · %2 palavras").arg(author, MsPaint::fmtInt(total));
    txt->addWidget(mutedLabel(sub, w, 11.5));
    ResumeEntry e;
    if (lastResumeHere(&e)) {
        auto* go = new QPushButton(tr("▸ Continuar de onde parou"), w);
        go->setCursor(Qt::PointingHandCursor);
        go->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: #ffffff; border: none; border-radius: 12px;"
            " padding: 5px 12px; font-size: 12px; font-weight: 600; }"
            "QPushButton:hover { background: %2; }")
            .arg(Theme::accentDefault(), tcol(Theme::accentDefault()).lighter(112).name()));
        connect(go, &QPushButton::clicked, this, [this, e]() {
            emit resumeRequested(e.manuscriptId, e.chapterId, e.sceneIndex, e.sentence, e.position);
        });
        txt->addSpacing(4);
        txt->addWidget(go, 0, Qt::AlignLeft);
    }
    txt->addStretch(1);
    top->addLayout(txt, 1);
    v->addLayout(top);

    // Barra do livro: uma marca por fronteira de capítulo.
    auto* bar = new MsRow(w);
    bar->setRowHeight(12);
    bar->setCursor(Qt::ArrowCursor);
    bar->setToolTip(tr("Botão direito: começar nova revisão"));
    bar->setPainter([words, total, pct](QPainter& p, const QRect& r, const MsRow*) {
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF track(r.left(), r.center().y() - 3, r.width(), 6);
        QColor bg = tcol(Theme::textPrimary());
        bg.setAlphaF(0.14);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(track, 3, 3);
        p.setBrush(tcol(Theme::accentDefault()));
        if (pct > 0) p.drawRoundedRect(QRectF(track.left(), track.top(), track.width() * pct / 100.0, 6), 3, 3);
        if (total > 0) {
            QColor tick = tcol(Theme::textPrimary());
            tick.setAlphaF(0.35);
            int acc = 0;
            for (int i = 0; i + 1 < words.size(); ++i) {
                acc += words.at(i);
                const double x = track.left() + track.width() * acc / double(total);
                p.fillRect(QRectF(x, r.top(), 1, r.height()), tick);
            }
        }
    });
    bar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(bar, &QToolButton::customContextMenuRequested, this, [this, bar, msId](const QPoint& pos) {
        QMenu menu(this);
        menu.setStyleSheet(contextMenuQss());
        auto* a = menu.addAction(tr("Começar nova revisão"));
        connect(a, &QAction::triggered, this, [this, msId]() { emit resetVisitedRequested(msId); });
        PanelMotion::animateMenu(&menu);
        menu.exec(bar->mapToGlobal(pos));
    });
    auto* pb = new QVBoxLayout;
    pb->setSpacing(4);
    pb->addWidget(bar);
    auto* labels = new QHBoxLayout;
    auto* left = mutedLabel(tr("%1% do livro revisitado").arg(pct), w, 10.5);
    left->setWordWrap(false);
    labels->addWidget(left);
    labels->addStretch(1);
    if (lastResumeHere(&e)) {
        if (const Chapter* c = m_model->findChapter(e.chapterId)) {
            const QString where = (c->type == QStringLiteral("chapter") && !m_model->chapterNumberLabel(*c).isEmpty())
                ? tr("cap. %1 · %2").arg(m_model->chapterNumberLabel(*c), chapterTitleOnly(*c))
                : chapterTitleOnly(*c);
            auto* right = mutedLabel(QFontMetrics(uiFont(10.5)).elidedText(where, Qt::ElideRight, 150), w, 10.5);
            right->setWordWrap(false);
            labels->addWidget(right);
        }
    }
    pb->addLayout(labels);
    v->addLayout(pb);
    return w;
}

void ManuscriptPanel::buildReaderView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    for (const auto& c : chs) {
        const bool special = (c.type != QStringLiteral("chapter"));
        const QString num = special ? QString() : m_model->chapterNumberLabel(c);
        const QString title = special
            ? m_model->chapterTypeName(c) + (c.title.isEmpty() ? QString() : QStringLiteral(": ") + c.title)
            : (num.isEmpty() ? chapterTitleOnly(c) : num + QStringLiteral(". ") + chapterTitleOnly(c));
        const QString right = scenesText(c);
        const double seen = std::clamp(m_visited.value(c.id, 0.0), 0.0, 1.0);
        const bool full = seen >= 0.97;
        const bool cur = isCurrent(c.id, -1);
        const bool dim = !m_povFilter.isEmpty() && povOf(c) != m_povFilter;
        auto* row = new MsRow(this);
        row->setRowHeight(36);
        row->setToolTip(full ? tr("Revisado até o fim")
                             : seen > 0 ? tr("Você foi até %1% deste capítulo").arg(int(std::round(seen * 100)))
                                        : tr("Ainda não passou por aqui nesta revisão"));
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (dim) p.setOpacity(0.4);
            p.setRenderHint(QPainter::Antialiasing);
            if (cur || self->isHovered())
                p.fillRect(r, cur ? tcol(Theme::accentInfoSoft()) : tcol(Theme::hoverOverlay()));
            QColor line = tcol(Theme::textPrimary());
            line.setAlphaF(0.06);
            p.fillRect(QRect(r.left(), r.bottom(), r.width(), 1), line);
            const QRectF ck(r.left() + 12, r.center().y() - 8.5, 17, 17);
            if (full) {
                const QColor ok = tcol(Theme::accentSuccess());
                p.setPen(Qt::NoPen);
                p.setBrush(ok);
                p.drawEllipse(ck);
                QPen tickPen(tcol(Theme::panelBackground()), 1.8);
                tickPen.setCapStyle(Qt::RoundCap);
                tickPen.setJoinStyle(Qt::RoundJoin);
                p.setPen(tickPen);
                QPainterPath t(QPointF(ck.left() + 4.5, ck.center().y() + 0.5));
                t.lineTo(ck.left() + 7.3, ck.center().y() + 3.2);
                t.lineTo(ck.right() - 4.2, ck.center().y() - 3);
                p.drawPath(t);
            } else if (seen > 0) {
                const QColor acc = tcol(Theme::accentDefault());
                p.setPen(Qt::NoPen);
                p.setBrush(acc);
                p.drawPie(ck, 90 * 16, -int(seen * 360 * 16));
                p.setPen(QPen(acc, 1.5));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(ck.adjusted(0.75, 0.75, -0.75, -0.75));
            } else {
                p.setPen(QPen(tcol(Theme::textMuted()), 1.5));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(ck.adjusted(0.75, 0.75, -0.75, -0.75));
            }
            p.setFont(uiFont(11));
            p.setPen(tcol(Theme::textMuted()));
            const int rw = p.fontMetrics().horizontalAdvance(right);
            const int rightX = r.right() - 14 - rw;
            p.drawText(QRect(rightX, r.top(), rw + 2, r.height()), Qt::AlignVCenter | Qt::AlignRight, right);
            const int x = r.left() + 40;
            p.setFont(serifFont(14));
            p.setPen(cur || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
            p.drawText(QRect(x, r.top(), rightX - x - 10, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
                       p.fontMetrics().elidedText(title, Qt::ElideRight, rightX - x - 10));
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString chapterId = c.id, manuscriptId = c.manuscriptId;
        connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
            showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chapterId);
        add(row);
    }
}

// ---- Índice ilustrado ----

void ManuscriptPanel::buildIllustratedView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QHash<QString, int> fams = vignetteFamilies();
    const int textW = std::max(120, width() - 16 - 12 - 52 - 11 - 12);
    const QFontMetrics sumFm(serifFont(13, QFont::Normal, true));
    for (const auto& c : chs) {
        const bool special = (c.type != QStringLiteral("chapter"));
        const QString num = m_model->chapterNumberLabel(c);
        const QString kicker = (special || num.isEmpty()) ? m_model->chapterTypeName(c).toUpper()
                                                        : tr("CAP. %1").arg(num);
        const QString title = c.title.isEmpty() ? m_model->chapterDisplayLabel(c) : c.title;
        const bool hasSum = !c.summary.trimmed().isEmpty();
        const QString sum = hasSum ? c.summary : tr("Sem resumo ainda.");
        const int nLines = std::max(1, int(wrapLines(sumFm, sum, textW, 2).size()));
        const int fam = fams.value(c.id, MsVignette::autoFamily(c.id));
        const int words = chapterWords(c.id);
        const QString meta = (c.vignetteImage.isEmpty() ? MsVignette::familyName(fam).toLower() : tr("imagem própria"))
                             + QStringLiteral(" · ") + tr("%1 palavras").arg(MsPaint::fmtInt(words));
        const QPixmap vig = chapterVignette(c, fam, QSize(52, 52), true);
        const bool cur = isCurrent(c.id, -1);
        const bool dim = !m_povFilter.isEmpty() && povOf(c) != m_povFilter;
        const int h = std::max(70, 10 + 20 + nLines * 17 + 16 + 8);
        auto* row = new MsRow(this);
        row->setRowHeight(h);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (dim) p.setOpacity(0.4);
            p.setRenderHint(QPainter::Antialiasing);
            if (cur || self->isHovered())
                p.fillRect(r, cur ? tcol(Theme::accentInfoSoft()) : tcol(Theme::hoverOverlay()));
            QColor line = tcol(Theme::textPrimary());
            line.setAlphaF(0.06);
            p.fillRect(QRect(r.left(), r.bottom(), r.width(), 1), line);
            p.drawPixmap(QPointF(r.left() + 12, r.top() + (r.height() - 52) / 2.0), vig);
            const int x = r.left() + 12 + 52 + 11;
            const int tw = r.right() - 12 - x;
            int y = r.top() + 9;
            QFont kf = serifFont(11, QFont::DemiBold);
            kf.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
            p.setFont(kf);
            p.setPen(tcol(Theme::accentDefault()));
            const int kw = p.fontMetrics().horizontalAdvance(kicker);
            p.drawText(QRect(x, y, kw + 2, 20), Qt::AlignLeft | Qt::AlignBottom, kicker);
            p.setFont(serifFont(15, QFont::DemiBold));
            p.setPen(tcol(Theme::textBright()));
            p.drawText(QRect(x + kw + 7, y, tw - kw - 7, 20), Qt::AlignLeft | Qt::AlignBottom,
                       p.fontMetrics().elidedText(title, Qt::ElideRight, tw - kw - 7));
            y += 22;
            p.setFont(serifFont(13, QFont::Normal, true));
            QColor sc = tcol(Theme::textMuted());
            if (!hasSum) sc.setAlphaF(0.7);
            p.setPen(sc);
            const QStringList ls = wrapLines(p.fontMetrics(), sum, tw, nLines);
            for (const QString& l : ls) { p.drawText(QRect(x, y, tw, 17), Qt::AlignLeft | Qt::AlignVCenter, l); y += 17; }
            p.setFont(uiFont(10));
            QColor mc = tcol(Theme::textMuted());
            mc.setAlphaF(0.75);
            p.setPen(mc);
            p.drawText(QRect(x, y + 1, tw, 14), Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(meta, Qt::ElideRight, tw));
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString chapterId = c.id, manuscriptId = c.manuscriptId;
        connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
            showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chapterId);
        add(row);
    }
}

// ---- Temporadas ----

QWidget* ManuscriptPanel::makeSeasonsTop() {
    const auto& mss = m_model->manuscripts();
    if (mss.isEmpty()) return nullptr;
    auto* w = new QWidget(m_top);
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    // Temporadas: um chip por livro (o picker).
    auto* chips = new QWidget(w);
    auto* cl = new QHBoxLayout(chips);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(6);
    const QString active = activeManuscriptId();
    for (int i = 0; i < mss.size(); ++i) {
        const bool on = (mss.at(i).id == active);
        auto* b = new QToolButton(chips);
        b->setText(tr("Livro %1").arg(i + 1));
        b->setToolTip(mss.at(i).title.isEmpty() ? tr("(sem título)") : mss.at(i).title);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QStringLiteral(
            "QToolButton { color: %1; background: %2; border: 1px solid %3; border-radius: 12px;"
            " padding: 3px 11px; font-size: 12px; font-weight: %4; }"
            "QToolButton:hover { border-color: %5; color: %6; }")
            .arg(on ? QStringLiteral("#1a1a19") : Theme::textPrimary(),
                 on ? Theme::textBright() : QStringLiteral("transparent"),
                 on ? Theme::textBright() : Theme::subtleBorder(),
                 on ? QStringLiteral("600") : QStringLiteral("400"),
                 Theme::borderStrong(), on ? QStringLiteral("#1a1a19") : Theme::textBright()));
        const QString id = mss.at(i).id;
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, &QToolButton::clicked, this, [this, id]() { selectManuscript(id); });
        connect(b, &QToolButton::customContextMenuRequested, this, [this, b, id](const QPoint& pos) {
            showManuscriptContextMenu(id, b->mapToGlobal(pos));
        });
        cl->addWidget(b);
    }
    cl->addStretch(1);
    auto* sc = new QScrollArea(w);
    sc->setFrameShape(QFrame::NoFrame);
    sc->setWidget(chips);
    sc->setWidgetResizable(true);
    sc->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sc->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sc->setFixedHeight(chips->sizeHint().height() + 2);
    sc->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    sc->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    chips->setStyleSheet(QStringLiteral("background: transparent;"));
    v->addWidget(sc);

    // Continuar: o Onde parei, com a vinheta do capítulo.
    ResumeEntry e;
    if (lastResumeHere(&e)) {
        const Chapter* c = m_model->findChapter(e.chapterId);
        if (c) {
            const Chapter cc = *c;
            const QHash<QString, int> fams = vignetteFamilies();
            const QPixmap vig = chapterVignette(cc, fams.value(cc.id, MsVignette::autoFamily(cc.id)), QSize(96, 56), false);
            const QString num = m_model->chapterNumberLabel(cc);
            const QString title = (cc.type == QStringLiteral("chapter") && !num.isEmpty())
                ? num + QStringLiteral(". ") + chapterTitleOnly(cc) : chapterTitleOnly(cc);
            QString where;
            if (cc.scenes.size() > 1 && e.sceneIndex >= 0)
                where = tr("cena %1 de %2 · onde você parou").arg(e.sceneIndex + 1).arg(cc.scenes.size());
            else
                where = tr("onde você parou");
            auto* card = new MsRow(w);
            card->setRowHeight(74);
            card->setToolTip(e.sentence.isEmpty() ? QString() : QStringLiteral("“%1”").arg(e.sentence.simplified()));
            card->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
                p.setRenderHint(QPainter::Antialiasing);
                const QRectF box = QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5);
                p.setPen(QPen(tcol(Theme::subtleBorder()), 1));
                p.setBrush(self->isHovered() ? tcol(Theme::hoverStrong()) : tcol(Theme::hoverOverlay()));
                p.drawRoundedRect(box, 8, 8);
                p.drawPixmap(QPointF(r.left() + 8, r.top() + 9), vig);
                const int x = r.left() + 8 + 96 + 10;
                const int tw = r.right() - 10 - x;
                QFont kf = uiFont(9.5, QFont::DemiBold);
                kf.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
                p.setFont(kf);
                p.setPen(tcol(Theme::textMuted()));
                p.drawText(QRect(x, r.top() + 12, tw, 14), Qt::AlignLeft | Qt::AlignVCenter, tr("CONTINUAR"));
                p.setFont(serifFont(14, QFont::DemiBold));
                p.setPen(tcol(Theme::textBright()));
                p.drawText(QRect(x, r.top() + 27, tw, 20), Qt::AlignLeft | Qt::AlignVCenter,
                           p.fontMetrics().elidedText(title, Qt::ElideRight, tw));
                p.setFont(uiFont(10.5));
                p.setPen(tcol(Theme::textMuted()));
                p.drawText(QRect(x, r.top() + 48, tw, 14), Qt::AlignLeft | Qt::AlignVCenter,
                           p.fontMetrics().elidedText(where, Qt::ElideRight, tw));
            });
            connect(card, &QToolButton::clicked, this, [this, e]() {
                emit resumeRequested(e.manuscriptId, e.chapterId, e.sceneIndex, e.sentence, e.position);
            });
            v->addWidget(card);
        }
    }
    return w;
}

void ManuscriptPanel::buildSeasonsView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QHash<QString, int> fams = vignetteFamilies();
    int maxW = 1;
    for (const auto& c : readingChapters()) maxW = std::max(maxW, chapterWords(c.id));
    for (const auto& c : chs) {
        const bool special = (c.type != QStringLiteral("chapter"));
        const QString num = special ? QString() : m_model->chapterNumberLabel(c);
        const QString title = special
            ? m_model->chapterTypeName(c) + (c.title.isEmpty() ? QString() : QStringLiteral(": ") + c.title)
            : (num.isEmpty() ? chapterTitleOnly(c) : num + QStringLiteral(". ") + chapterTitleOnly(c));
        const QString right = scenesText(c);
        const QString sum = c.summary.trimmed();
        const int words = chapterWords(c.id);
        const double size = words / double(maxW);
        const int fam = fams.value(c.id, MsVignette::autoFamily(c.id));
        const QPixmap vig = chapterVignette(c, fam, QSize(96, 56), false);
        const bool cur = isCurrent(c.id, -1);
        const bool dim = !m_povFilter.isEmpty() && povOf(c) != m_povFilter;
        auto* row = new MsRow(this);
        row->setRowHeight(72);
        row->setToolTip(words > 0 ? tr("%1 palavras").arg(MsPaint::fmtInt(words)) : QString());
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (dim) p.setOpacity(0.4);
            p.setRenderHint(QPainter::Antialiasing);
            if (cur || self->isHovered())
                p.fillRect(r, cur ? tcol(Theme::accentInfoSoft()) : tcol(Theme::hoverOverlay()));
            QColor line = tcol(Theme::textPrimary());
            line.setAlphaF(0.06);
            p.fillRect(QRect(r.left(), r.top(), r.width(), 1), line);
            const QPointF at(r.left() + 12, r.top() + 8);
            p.drawPixmap(at, vig);
            // Barra: o tamanho do capítulo perto do maior do livro.
            if (words > 0) {
                const QRectF track(at.x() + 4, at.y() + 56 - 7, 88, 3);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 128));
                p.drawRoundedRect(track, 1.5, 1.5);
                p.setBrush(tcol(Theme::accentDefault()));
                p.drawRoundedRect(QRectF(track.left(), track.top(), std::max(3.0, track.width() * size), 3), 1.5, 1.5);
            }
            const int x = r.left() + 12 + 96 + 10;
            p.setFont(uiFont(11));
            p.setPen(tcol(Theme::textMuted()));
            const int rw = p.fontMetrics().horizontalAdvance(right);
            p.drawText(QRect(r.right() - 12 - rw, r.top() + 8, rw + 2, 20), Qt::AlignRight | Qt::AlignVCenter, right);
            const int tw = r.right() - 12 - rw - 6 - x;
            p.setFont(serifFont(13.5, QFont::DemiBold));
            p.setPen(tcol(Theme::textBright()));
            p.drawText(QRect(x, r.top() + 8, tw, 20), Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(title, Qt::ElideRight, tw));
            if (!sum.isEmpty()) {
                p.setFont(uiFont(11.5));
                p.setPen(tcol(Theme::textMuted()));
                const int sw = r.right() - 12 - x;
                int y = r.top() + 30;
                for (const QString& l : wrapLines(p.fontMetrics(), sum, sw, 2)) {
                    p.drawText(QRect(x, y, sw, 16), Qt::AlignLeft | Qt::AlignVCenter, l);
                    y += 16;
                }
            }
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString chapterId = c.id, manuscriptId = c.manuscriptId;
        connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
            showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chapterId);
        add(row);
    }
}

// ---- Página da loja ----

QWidget* ManuscriptPanel::makeStoreHead() {
    const QString msId = activeManuscriptId();
    const Manuscript* m = m_model->findManuscript(msId);
    if (!m) return nullptr;
    const auto& mss = m_model->manuscripts();
    int number = 1;
    for (int i = 0; i < mss.size(); ++i) if (mss.at(i).id == msId) number = i + 1;
    const QList<Chapter> reading = readingChapters();
    int total = 0;
    for (const auto& c : reading) total += chapterWords(c.id);
    int pages = 0;
    for (const auto& c : reading) pages += std::max(1, (chapterWords(c.id) + 249) / 250);

    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(6, 8, 6, 2);
    v->setSpacing(6);
    auto* top = new QHBoxLayout;
    top->setSpacing(12);
    auto* cov = new QLabel(w);
    cov->setPixmap(coverPixmap(m_model, *m, number, QSize(92, 134), devicePixelRatioF(), tr("Livro")));
    cov->setFixedSize(92, 134);
    cov->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(cov, &QWidget::customContextMenuRequested, this, [this, cov, msId](const QPoint& pos) {
        showManuscriptContextMenu(msId, cov->mapToGlobal(pos));
    });
    top->addWidget(cov, 0, Qt::AlignTop);
    auto* txt = new QVBoxLayout;
    txt->setSpacing(3);
    auto* tt = new QLabel(m->title.isEmpty() ? tr("(sem título)") : m->title, w);
    tt->setWordWrap(true);
    tt->setFont(serifFont(17, QFont::DemiBold));
    tt->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textBright()));
    txt->addWidget(tt);
    const QString author = m_model->projectAuthor();
    if (!author.isEmpty()) {
        auto* au = new QLabel(author, w);
        au->setFont(uiFont(12));
        au->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textPrimary()));
        txt->addWidget(au);
    }
    if (mss.size() > 1) {
        const QString saga = m_model->projectName();
        auto* se = new QLabel(saga.isEmpty() ? tr("Livro %1 de %2").arg(number).arg(mss.size())
                                             : tr("Livro %1 da saga %2").arg(number).arg(saga), w);
        se->setWordWrap(true);
        se->setFont(uiFont(11.5));
        se->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::accentInfo()));
        txt->addWidget(se);
    }
    txt->addSpacing(6);
    auto* facts = new QHBoxLayout;
    facts->setSpacing(6);
    auto fact = [&](const QString& value, const QString& label) {
        auto* f = new QFrame(w);
        f->setObjectName(QStringLiteral("msFact"));
        f->setStyleSheet(QStringLiteral("#msFact { background: %1; border: 1px solid %2; border-radius: 6px; }")
            .arg(Theme::hoverOverlay(), Theme::subtleBorder()));
        auto* fl = new QVBoxLayout(f);
        fl->setContentsMargins(4, 5, 4, 5);
        fl->setSpacing(1);
        auto* vl = new QLabel(value, f);
        vl->setAlignment(Qt::AlignCenter);
        vl->setFont(uiFont(value.size() > 6 ? 12 : 13.5, QFont::Bold));
        vl->setStyleSheet(QStringLiteral("color: %1; background: transparent; border: none;").arg(Theme::textBright()));
        fl->addWidget(vl);
        auto* ll = new QLabel(label, f);
        ll->setAlignment(Qt::AlignCenter);
        QFont lf = uiFont(8.5);
        lf.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
        ll->setFont(lf);
        ll->setStyleSheet(QStringLiteral("color: %1; background: transparent; border: none;").arg(Theme::textMuted()));
        fl->addWidget(ll);
        facts->addWidget(f, 1);
    };
    fact(MsPaint::fmtInt(pages), tr("PÁGINAS"));
    fact(MsPaint::fmtInt(total), tr("PALAVRAS"));
    fact(QString::number(reading.size()), tr("CAPÍTULOS"));
    txt->addLayout(facts);
    txt->addStretch(1);
    top->addLayout(txt, 1);
    v->addLayout(top);

    // Sinopse do manuscrito (a do projeto quando ele não tem uma própria).
    v->addSpacing(8);
    v->addWidget(capsLabel(tr("Sinopse"), w));
    const QString syn = m_model->manuscriptEffectiveSynopsis(msId).trimmed();
    auto* sl = new QLabel(syn.isEmpty() ? tr("Sem sinopse ainda.") : syn, w);
    sl->setWordWrap(true);
    sl->setFont(serifFont(13.5, QFont::Normal, syn.isEmpty()));
    sl->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
        .arg(syn.isEmpty() ? Theme::textMuted() : Theme::textPrimary()));
    sl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(sl);
    auto* edit = linkButton(syn.isEmpty() ? tr("escrever") : tr("editar"), w, Theme::accentDefault(), 12);
    connect(edit, &QToolButton::clicked, this, [this, msId]() {
        const Manuscript* mm = m_model->findManuscript(msId);
        if (!mm) return;
        bool ok = false;
        const QString cur = mm->synopsis.isEmpty() ? m_model->manuscriptEffectiveSynopsis(msId) : mm->synopsis;
        const QString t = QInputDialog::getMultiLineText(this, tr("Sinopse"), tr("Sinopse deste livro:"), cur, &ok);
        if (ok) m_model->updateManuscriptSynopsis(msId, t.trimmed());
    });
    v->addWidget(edit, 0, Qt::AlignLeft);
    return w;
}

void ManuscriptPanel::buildStoreView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const bool status = statusVisible();
    {
        auto* hd = capsLabel(tr("Sumário"), this);
        hd->setContentsMargins(6, 6, 6, 2);
        add(hd);
    }
    for (const auto& c : chs) {
        const bool special = (c.type != QStringLiteral("chapter"));
        const QString num = special ? QString() : m_model->chapterNumberLabel(c);
        const QString title = special
            ? m_model->chapterTypeName(c) + (c.title.isEmpty() ? QString() : QStringLiteral(": ") + c.title)
            : (num.isEmpty() ? chapterTitleOnly(c) : num + QStringLiteral(" - ") + chapterTitleOnly(c));
        const QString right = scenesText(c);
        const bool cur = isCurrent(c.id, -1);
        const bool dim = !m_povFilter.isEmpty() && povOf(c) != m_povFilter;
        const QColor stC = status ? statusColor(c) : QColor();
        auto* row = new MsRow(this);
        row->setRowHeight(30);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (dim) p.setOpacity(0.4);
            p.setRenderHint(QPainter::Antialiasing);
            if (cur || self->isHovered()) {
                p.setPen(Qt::NoPen);
                p.setBrush(cur ? tcol(Theme::accentInfoSoft()) : tcol(Theme::hoverOverlay()));
                p.drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 4, 4);
            }
            int x = r.left() + 12;
            if (stC.isValid()) {
                p.setPen(Qt::NoPen);
                p.setBrush(stC);
                p.drawEllipse(QPointF(x + 2, r.center().y() + 0.5), 2.5, 2.5);
                x += 10;
            }
            p.setFont(uiFont(11));
            p.setPen(tcol(Theme::textMuted()));
            const int rw = p.fontMetrics().horizontalAdvance(right);
            const int rightX = r.right() - 12 - rw;
            p.drawText(QRect(rightX, r.top(), rw + 2, r.height()), Qt::AlignVCenter | Qt::AlignRight, right);
            p.setFont(serifFont(14));
            p.setPen(cur || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
            p.drawText(QRect(x, r.top(), rightX - x - 10, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
                       p.fontMetrics().elidedText(title, Qt::ElideRight, rightX - x - 10));
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString chapterId = c.id, manuscriptId = c.manuscriptId;
        connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
            showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chapterId);
        add(row);
    }
    if (m_model->manuscripts().size() > 1) {
        auto* w = new QWidget(this);
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(6, 12, 6, 4);
        v->setSpacing(6);
        v->addWidget(capsLabel(tr("Da mesma série"), w));
        if (QWidget* minis = makeSagaMinis(w, QSize(44, 64), false)) v->addWidget(minis);
        add(w);
    }
}

// ---- Trocar desenho ----

void ManuscriptPanel::showVignettePicker(const QString& chapterId, const QPoint& globalPos) {
    const Chapter* found = m_model ? m_model->findChapter(chapterId) : nullptr;
    const Manuscript* ms = found ? m_model->findManuscript(found->manuscriptId) : nullptr;
    if (!found || !ms) return;
    const Chapter c = *found;
    const QString msId = ms->id;
    const bool circle = (m_style == Style::Illustrated);
    const QSize thumb = circle ? QSize(46, 46) : QSize(72, 42);

    QStringList ids;
    for (const auto& rc : readingChapters()) ids << rc.id;
    const int idx = std::max<qsizetype>(0, ids.indexOf(chapterId));
    const QList<int> autos = MsVignette::autoFamilies(ids);
    const int autoFam = idx < autos.size() ? autos.at(idx) : MsVignette::autoFamily(chapterId);
    const int current = vignetteFamilies().value(chapterId, autoFam);
    const bool bookMode = m_pickerBookMode;
    const bool isAuto = bookMode ? ms->vignette.isEmpty() : c.vignette.isEmpty();
    const int pick = bookMode ? MsVignette::familyFromId(ms->vignette) : current;

    auto* pop = new QFrame(this, Qt::Popup);
    pop->setAttribute(Qt::WA_DeleteOnClose);
    pop->setObjectName(QStringLiteral("msVigPicker"));
    pop->setStyleSheet(Theme::qss(QStringLiteral(
        "#msVigPicker { background: %1; border: 1px solid %2; border-radius: @radius-panel; }"))
        .arg(Theme::panelBackground(), Theme::borderStrong()));
    auto* v = new QVBoxLayout(pop);
    v->setContentsMargins(10, 10, 10, 10);
    v->setSpacing(8);
    auto* hr = new QHBoxLayout;
    auto* ht = new QLabel(tr("Trocar desenho"), pop);
    ht->setFont(uiFont(12, QFont::DemiBold));
    ht->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textBright()));
    hr->addWidget(ht);
    hr->addStretch(1);
    const QString num = m_model->chapterNumberLabel(c);
    auto* hs = mutedLabel((c.type == QStringLiteral("chapter") && !num.isEmpty())
                              ? tr("cap. %1").arg(num) : m_model->chapterTypeName(c).toLower(), pop, 11);
    hs->setWordWrap(false);
    hr->addWidget(hs);
    v->addLayout(hr);

    // Só este capítulo | O livro todo
    auto* seg = new QFrame(pop);
    seg->setObjectName(QStringLiteral("msVigSeg"));
    seg->setStyleSheet(QStringLiteral("#msVigSeg { background: %1; border-radius: 6px; }").arg(Theme::inputBackground()));
    auto* sl = new QHBoxLayout(seg);
    sl->setContentsMargins(2, 2, 2, 2);
    sl->setSpacing(2);
    auto segBtn = [&](const QString& text, bool on, bool toBook) {
        auto* b = new QToolButton(seg);
        b->setText(text);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setStyleSheet(QStringLiteral(
            "QToolButton { color: %1; background: %2; border: none; border-radius: 4px; padding: 4px 6px; font-size: 11.5px; }"
            "QToolButton:hover { color: %3; }")
            .arg(on ? Theme::textBright() : Theme::textMuted(), on ? Theme::hoverStrong() : QStringLiteral("transparent"),
                 Theme::textBright()));
        connect(b, &QToolButton::clicked, this, [this, pop, toBook, chapterId, globalPos]() {
            if (m_pickerBookMode == toBook) return;
            m_pickerBookMode = toBook;
            pop->close();
            QTimer::singleShot(0, this, [this, chapterId, globalPos]() { showVignettePicker(chapterId, globalPos); });
        });
        sl->addWidget(b);
    };
    segBtn(tr("Só este capítulo"), !bookMode, false);
    segBtn(tr("O livro todo"), bookMode, true);
    v->addWidget(seg);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    const QString itemQss = QStringLiteral(
        "QToolButton { color: %1; background: transparent; border: 1px solid transparent; border-radius: 6px;"
        " padding: 3px 2px 2px 2px; font-size: 10.5px; font-weight: %3; }"
        "QToolButton:hover { background: %2; }");
    auto apply = [this, chapterId, msId, bookMode](int fam) {
        const QString id = fam < 0 ? QString() : MsVignette::familyId(fam);
        if (!bookMode) {
            m_model->updateChapterVignette(chapterId, id);
            return;
        }
        // "O livro todo" vale pra todos: as escolhas por capítulo saem.
        QStringList own;
        for (const auto& rc : m_model->chapters())
            if (rc.manuscriptId == msId && !rc.vignette.isEmpty()) own << rc.id;
        for (const QString& id2 : own) m_model->updateChapterVignette(id2, QString());
        m_model->setManuscriptVignette(msId, id);
    };
    auto item = [&](int fam, const QString& label, bool on, int pos) {
        const int drawFam = fam < 0 ? autoFam : fam;
        QPixmap pm = MsVignette::render(c.id, chapterWords(c.id), vignetteColor(c.id), drawFam, thumb,
                                        devicePixelRatioF(), circle);
        if (on) {
            // Contorno na cor de destaque, como no concept.
            QPixmap framed(QSize(thumb.width() + 6, thumb.height() + 6) * devicePixelRatioF());
            framed.setDevicePixelRatio(devicePixelRatioF());
            framed.fill(Qt::transparent);
            QPainter p(&framed);
            p.setRenderHint(QPainter::Antialiasing);
            p.drawPixmap(QPointF(3, 3), pm);
            p.setPen(QPen(tcol(Theme::accentDefault()), 2));
            p.setBrush(Qt::NoBrush);
            if (circle) p.drawEllipse(QRectF(1, 1, thumb.width() + 4, thumb.height() + 4));
            else p.drawRoundedRect(QRectF(1, 1, thumb.width() + 4, thumb.height() + 4), 6, 6);
            p.end();
            pm = framed;
        } else {
            QPixmap padded(QSize(thumb.width() + 6, thumb.height() + 6) * devicePixelRatioF());
            padded.setDevicePixelRatio(devicePixelRatioF());
            padded.fill(Qt::transparent);
            QPainter p(&padded);
            p.drawPixmap(QPointF(3, 3), pm);
            p.end();
            pm = padded;
        }
        auto* b = new QToolButton(pop);
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setIcon(QIcon(pm));
        b->setIconSize(QSize(thumb.width() + 6, thumb.height() + 6));
        b->setText(label);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(itemQss.arg(on ? Theme::textBright() : Theme::textPrimary(), Theme::hoverOverlay(),
                                     on ? QStringLiteral("600") : QStringLiteral("400")));
        connect(b, &QToolButton::clicked, this, [pop, apply, fam]() {
            pop->close();
            apply(fam);
        });
        const int cols = circle ? 4 : 3;
        grid->addWidget(b, pos / cols, pos % cols);
    };
    item(-1, tr("Automático"), isAuto, 0);
    for (int f = 0; f < MsVignette::FamilyCount; ++f)
        item(f, MsVignette::familyName(f), !isAuto && pick == f, f + 1);
    v->addLayout(grid);

    auto* foot = new QLabel(tr("o desenho continua crescendo com o capítulo; só muda a fundação"), pop);
    foot->setWordWrap(true);
    foot->setAlignment(Qt::AlignCenter);
    foot->setFont(serifFont(11, QFont::Normal, true));
    foot->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textMuted()));
    v->addWidget(foot);

    // A porta: uma ilustração de verdade no lugar do desenho.
    auto* imgRow = new QHBoxLayout;
    auto* useImg = linkButton(c.vignetteImage.isEmpty() ? tr("Usar imagem minha…") : tr("Trocar imagem…"),
                              pop, Theme::accentDefault(), 11.5);
    connect(useImg, &QToolButton::clicked, this, [this, pop, chapterId]() {
        pop->close();
        const QString path = QFileDialog::getOpenFileName(this, tr("Imagem do capítulo"), QString(),
            tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (path.isEmpty()) return;
        const QImage img(path);
        if (img.isNull()) return;
        m_model->updateChapterVignetteImage(chapterId, dataUrlFromImage(img, 480));
    });
    imgRow->addWidget(useImg);
    imgRow->addStretch(1);
    if (!c.vignetteImage.isEmpty()) {
        auto* back = linkButton(tr("Voltar ao desenho"), pop, Theme::textMuted(), 11.5);
        connect(back, &QToolButton::clicked, this, [this, pop, chapterId]() {
            pop->close();
            m_model->updateChapterVignetteImage(chapterId, QString());
        });
        imgRow->addWidget(back);
    }
    v->addLayout(imgRow);

    pop->adjustSize();
    QPoint at = globalPos + QPoint(2, 2);
    if (QScreen* scr = QApplication::screenAt(globalPos)) {
        const QRect avail = scr->availableGeometry();
        at.setX(std::clamp(at.x(), avail.left() + 4, avail.right() - pop->width() - 4));
        at.setY(std::clamp(at.y(), avail.top() + 4, avail.bottom() - pop->height() - 4));
    }
    pop->move(at);
    pop->show();
}

// ---- Box da saga ----

QWidget* ManuscriptPanel::makeBoxTop() {
    const auto& mss = m_model->manuscripts();
    if (mss.isEmpty()) return nullptr;
    const QString active = activeManuscriptId();
    struct Sp { QString id; QString title; int number; QColor color; bool on; };
    QList<Sp> spines;
    for (int i = 0; i < mss.size(); ++i)
        spines << Sp{ mss.at(i).id, mss.at(i).title.isEmpty() ? tr("(sem título)") : mss.at(i).title, i + 1,
                      MsPaint::bookColor(mss.at(i).id), mss.at(i).id == active };
    const QString saga = m_model->projectName().trimmed();
    const QString label = saga.isEmpty() ? tr("A SAGA") : tr("A SAGA %1").arg(saga.toUpper());

    auto* w = new QWidget(m_top);
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    // O estojo: as lombadas lado a lado; a do livro aberto sai mais alta.
    auto* box = new MsRow(w);
    box->setRowHeight(164);
    box->setCursor(Qt::PointingHandCursor);
    auto geom = [spines](const QRect& r) {
        const int n = spines.size();
        const int gap = 4;
        const int sw = std::clamp((r.width() - 20 - gap * (n - 1)) / std::max(1, n), 14, 34);
        const int total = n * sw + (n - 1) * gap;
        const int x0 = r.left() + (r.width() - total) / 2;
        const int bottom = r.top() + 10 + 17 + 118;
        QList<QRect> out;
        for (int i = 0; i < n; ++i) {
            const int h = spines.at(i).on ? 118 : 92;
            out << QRect(x0 + i * (sw + gap), bottom - h, sw, h);
        }
        return out;
    };
    box->setPainter([spines, label, geom](QPainter& p, const QRect& r, const MsRow* self) {
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF shell = QRectF(r).adjusted(0, 0, 0, -6);
        QLinearGradient bg(shell.topLeft(), shell.bottomLeft());
        bg.setColorAt(0, QColor("#2b2a28"));
        bg.setColorAt(1, QColor("#1d1c1b"));
        p.setPen(QPen(QColor(255, 255, 255, 16), 1));
        p.setBrush(bg);
        p.drawRoundedRect(shell, 4, 4);
        QFont lf = uiFont(9, QFont::DemiBold);
        lf.setLetterSpacing(QFont::AbsoluteSpacing, 2.6);
        p.setFont(lf);
        p.setPen(tcol(Theme::textMuted()));
        p.drawText(QRect(r.left() + 8, r.top() + 10, r.width() - 16, 12), Qt::AlignCenter,
                   p.fontMetrics().elidedText(label, Qt::ElideRight, r.width() - 16));
        const QList<QRect> rects = geom(r);
        for (int i = 0; i < rects.size(); ++i) {
            const QRect s = rects.at(i);
            const double hue = spines.at(i).color.hslHueF() < 0 ? 0.08 : spines.at(i).color.hslHueF();
            QLinearGradient g(s.topLeft(), s.topRight());
            g.setColorAt(0, QColor::fromHslF(float(hue), 0.35f, 0.26f));
            g.setColorAt(0.4, QColor::fromHslF(float(hue), 0.35f, 0.40f));
            g.setColorAt(1, QColor::fromHslF(float(hue), 0.35f, 0.24f));
            QPainterPath path;
            path.addRoundedRect(QRectF(s), 2, 2);
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawPath(path);
            if (self->hoverPart() == i && !spines.at(i).on) p.fillPath(path, QColor(255, 255, 255, 22));
            if (spines.at(i).on) {
                p.setPen(QPen(tcol(Theme::textBright()), 1.5));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(QRectF(s).adjusted(-1, -1, 1, 1), 2.5, 2.5);
            }
            p.setPen(QColor(255, 255, 255, 180));
            p.setFont(uiFont(9, QFont::Bold));
            p.drawText(QRect(s.left(), s.top() + 5, s.width(), 12), Qt::AlignCenter, QString::number(spines.at(i).number));
            // Título de baixo pra cima, como numa lombada.
            p.save();
            p.translate(s.center().x() + 4, s.bottom() - 8);
            p.rotate(-90);
            p.setFont(serifFont(std::min(11.0, s.width() * 0.34), QFont::DemiBold));
            p.setPen(QColor(255, 255, 255, 215));
            const int room = s.height() - 30;
            p.drawText(QPointF(0, 0), p.fontMetrics().elidedText(spines.at(i).title, Qt::ElideRight, room));
            p.restore();
        }
        // A borda do estojo, na frente das lombadas.
        const QRectF lip(r.left(), r.top() + 10 + 17 + 118, r.width(), 12);
        QLinearGradient lg(lip.topLeft(), lip.bottomLeft());
        lg.setColorAt(0, QColor("#34332f"));
        lg.setColorAt(1, QColor("#262523"));
        p.setPen(Qt::NoPen);
        p.setBrush(lg);
        p.drawRoundedRect(lip, 3, 3);
    });
    box->setHitTest([box, geom](const QPoint& pt) {
        const QList<QRect> rects = geom(box->rect());
        for (int i = 0; i < rects.size(); ++i)
            if (rects.at(i).adjusted(-2, 0, 2, 0).contains(pt)) return i;
        return -1;
    });
    connect(box, &MsRow::partClicked, this, [this, spines](int i) {
        if (i >= 0 && i < spines.size()) selectManuscript(spines.at(i).id);
    });
    box->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(box, &QToolButton::customContextMenuRequested, this, [this, box, spines, geom](const QPoint& pos) {
        const QList<QRect> rects = geom(box->rect());
        for (int i = 0; i < rects.size(); ++i)
            if (rects.at(i).contains(pos)) { showManuscriptContextMenu(spines.at(i).id, box->mapToGlobal(pos)); return; }
    });
    v->addWidget(box);

    // O livro puxado: capa, título e a contagem.
    const Manuscript* m = m_model->findManuscript(active);
    if (m) {
        int number = 1;
        for (int i = 0; i < mss.size(); ++i) if (mss.at(i).id == active) number = i + 1;
        const QList<Chapter> reading = readingChapters();
        int total = 0;
        for (const auto& c : reading) total += chapterWords(c.id);
        auto* pulled = new QWidget(w);
        auto* pl = new QHBoxLayout(pulled);
        pl->setContentsMargins(2, 0, 2, 0);
        pl->setSpacing(10);
        auto* cov = new QLabel(pulled);
        cov->setPixmap(coverPixmap(m_model, *m, number, QSize(38, 54), devicePixelRatioF(), tr("Livro")));
        cov->setFixedSize(38, 54);
        pl->addWidget(cov);
        auto* tx = new QVBoxLayout;
        tx->setSpacing(2);
        auto* tt = new QLabel(m->title.isEmpty() ? tr("(sem título)") : m->title, pulled);
        tt->setWordWrap(true);
        tt->setFont(serifFont(14, QFont::DemiBold));
        tt->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textBright()));
        tx->addWidget(tt);
        tx->addWidget(mutedLabel(tr("livro %1 de %2 · %3 capítulos · %4 palavras")
            .arg(number).arg(mss.size()).arg(reading.size()).arg(MsPaint::fmtInt(total)), pulled, 11));
        pl->addLayout(tx, 1);
        pulled->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString msId = m->id;
        connect(pulled, &QWidget::customContextMenuRequested, this, [this, pulled, msId](const QPoint& pos) {
            showManuscriptContextMenu(msId, pulled->mapToGlobal(pos));
        });
        v->addWidget(pulled);
    }
    return w;
}

void ManuscriptPanel::buildBoxView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = partsActive() ? validParts(reading) : QList<ManuscriptPart>();
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    const bool status = statusVisible();
    int curPart = -2;
    for (const auto& c : chs) {
        const int pi = parts.isEmpty() ? -1 : pidx.value(c.id, -1);
        if (pi != curPart) {
            curPart = pi;
            if (pi >= 0) {
                // Nome da Parte no meio da linha, na cor dela.
                const QString name = parts.at(pi).title.toUpper();
                const QColor pc = QColor(parts.at(pi).color).isValid() ? QColor(parts.at(pi).color) : tcol(Theme::textMuted());
                auto* div = new MsRow(this);
                div->setRowHeight(28);
                div->setCursor(Qt::ArrowCursor);
                div->setPainter([name, pc](QPainter& p, const QRect& r, const MsRow*) {
                    QFont f = uiFont(9, QFont::DemiBold);
                    f.setLetterSpacing(QFont::AbsoluteSpacing, 2.6);
                    p.setFont(f);
                    const int tw = std::min(r.width() - 60, p.fontMetrics().horizontalAdvance(name));
                    const int cx = r.center().x();
                    const int y = r.top() + 16;
                    QColor lc = pc;
                    lc.setAlphaF(0.4);
                    p.fillRect(QRect(r.left() + 12, y, cx - tw / 2 - 8 - (r.left() + 12), 1), lc);
                    p.fillRect(QRect(cx + tw / 2 + 8, y, r.right() - 12 - (cx + tw / 2 + 8), 1), lc);
                    p.setPen(pc);
                    p.drawText(QRect(cx - tw / 2 - 2, r.top() + 8, tw + 4, 16), Qt::AlignCenter,
                               p.fontMetrics().elidedText(name, Qt::ElideRight, tw + 4));
                });
                const QString partId = parts.at(pi).id;
                div->setContextMenuPolicy(Qt::CustomContextMenu);
                connect(div, &QToolButton::customContextMenuRequested, this, [this, div, partId](const QPoint& pos) {
                    showPartContextMenu(partId, div->mapToGlobal(pos));
                });
                add(div);
            }
        }
        const bool special = (c.type != QStringLiteral("chapter"));
        const QString num = special ? QString() : m_model->chapterNumberLabel(c);
        const QString title = special
            ? m_model->chapterTypeName(c) + (c.title.isEmpty() ? QString() : QStringLiteral(": ") + c.title)
            : (num.isEmpty() ? chapterTitleOnly(c) : num + QStringLiteral(" - ") + chapterTitleOnly(c));
        const int words = chapterWords(c.id);
        const QString right = words ? MsPaint::fmtInt(words) : QStringLiteral("—");
        const bool cur = isCurrent(c.id, -1);
        const bool dim = !m_povFilter.isEmpty() && povOf(c) != m_povFilter;
        const QColor stC = status ? statusColor(c) : QColor();
        auto* row = new MsRow(this);
        row->setRowHeight(30);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (dim) p.setOpacity(0.4);
            p.setRenderHint(QPainter::Antialiasing);
            if (cur || self->isHovered()) {
                p.setPen(Qt::NoPen);
                p.setBrush(cur ? tcol(Theme::accentInfoSoft()) : tcol(Theme::hoverOverlay()));
                p.drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 4, 4);
            }
            int x = r.left() + 12;
            if (stC.isValid()) {
                p.setPen(Qt::NoPen);
                p.setBrush(stC);
                p.drawEllipse(QPointF(x + 2, r.center().y() + 0.5), 2.5, 2.5);
                x += 10;
            }
            p.setFont(uiFont(11));
            p.setPen(tcol(Theme::textMuted()));
            const int rw = p.fontMetrics().horizontalAdvance(right);
            const int rightX = r.right() - 12 - rw;
            p.drawText(QRect(rightX, r.top(), rw + 2, r.height()), Qt::AlignVCenter | Qt::AlignRight, right);
            p.setFont(serifFont(14));
            p.setPen(cur || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
            p.drawText(QRect(x, r.top(), rightX - x - 10, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
                       p.fontMetrics().elidedText(title, Qt::ElideRight, rightX - x - 10));
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString chapterId = c.id, manuscriptId = c.manuscriptId;
        connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
            showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chapterId);
        add(row);
    }
}
