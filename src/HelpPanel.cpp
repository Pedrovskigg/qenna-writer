#include "HelpPanel.h"
#include "AnchorUtils.h"
#include "IconUtils.h"

#include "Theme.h"

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextImageFormat>
#include <QTextObjectInterface>
#include <QUrl>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextDocumentFragment>
#include <QTimer>
#include <QToolButton>

namespace {

// Substitui o QTextImageHandler padrão do Qt para ativar SmoothPixmapTransform
// antes de desenhar cada imagem — sem isso, o QTextBrowser escala os prints
// (vários deliberadamente exibidos acima da resolução nativa) com interpolação
// de baixa qualidade, ficando serrilhado. Mesmo fix já usado em SpellEditor.cpp.
class SmoothImageHandler : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

    QHash<QUrl, QSize> m_sizeCache;

public:
    explicit SmoothImageHandler(QObject* parent = nullptr) : QObject(parent) {}

    // A largura pedida no <img> é o teto: se a coluna de texto for mais
    // estreita (o padding global do QTextEdit come 200px), o print encolhe
    // proporcionalmente em vez de ser cortado na borda direita.
    QSizeF intrinsicSize(QTextDocument* doc, int pos, const QTextFormat& format) override {
        QSizeF size = requestedSize(format);
        const qreal avail = doc ? doc->textWidth() - 2 * doc->documentMargin() - 4 : -1;
        Q_UNUSED(pos);
        if (avail > 50 && size.width() > avail)
            size = QSizeF(avail, size.height() * avail / size.width());
        return size;
    }

    QSizeF requestedSize(const QTextFormat& format) {
        const QTextImageFormat fmt = format.toImageFormat();
        const bool hasW = fmt.hasProperty(QTextFormat::ImageWidth);
        const bool hasH = fmt.hasProperty(QTextFormat::ImageHeight);

        if (hasW && hasH)
            return QSizeF(fmt.width(), fmt.height());

        const QUrl url(fmt.name());
        if (!m_sizeCache.contains(url)) {
            QSize sz;
            const QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                QImageReader reader(path);
                sz = reader.size();
            } else {
                QImageReader reader(fmt.name());
                sz = reader.size();
            }
            m_sizeCache[url] = sz.isEmpty() ? QSize(100, 100) : sz;
        }

        const QSize nat = m_sizeCache.value(url, QSize(100, 100));
        if (hasW && nat.height() > 0)
            return QSizeF(fmt.width(), fmt.width() * nat.height() / nat.width());
        if (hasH && nat.width() > 0)
            return QSizeF(fmt.height() * nat.width() / nat.height(), fmt.height());
        return QSizeF(nat);
    }

    void drawObject(QPainter* painter, const QRectF& rect, QTextDocument* doc,
                    int /*pos*/, const QTextFormat& format) override {
        const QUrl url(format.toImageFormat().name());
        const QVariant data = doc->resource(QTextDocument::ImageResource, url);

        QImage image;
        if (data.userType() == QMetaType::QPixmap)
            image = qvariant_cast<QPixmap>(data).toImage();
        else if (data.userType() == QMetaType::QImage)
            image = qvariant_cast<QImage>(data);
        else if (data.userType() == QMetaType::QByteArray)
            image.loadFromData(data.toByteArray());
        if (image.isNull())
            image = QImage(format.toImageFormat().name());

        if (image.isNull()) return;

        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawImage(rect, image, image.rect());
        painter->restore();
    }
};
#include "HelpPanel.moc"


// Item da lista de tópicos com o selo "NOVO" à direita (tópico que mudou na
// versão atual e ainda não foi aberto).
class HelpListDelegate : public QStyledItemDelegate {
public:
    static constexpr int NewRole = Qt::UserRole + 2;
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        const bool isNew = idx.data(NewRole).toBool();
        QStyleOptionViewItem o = opt;
        if (isNew) o.rect.adjust(0, 0, -44, 0);
        QStyledItemDelegate::paint(p, o, idx);
        if (!isNew) return;
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        QFont f = opt.font;
        f.setPixelSize(9);
        f.setBold(true);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
        p->setFont(f);
        const QString text = QCoreApplication::translate("HelpPanel", "NOVO");
        const int w = QFontMetrics(f).horizontalAdvance(text) + 12;
        const QRectF pill(opt.rect.right() - w - 6, opt.rect.center().y() - 8, w, 16);
        const QColor c = Theme::toColor(Theme::accentSuccess());
        QColor border = c;
        border.setAlphaF(0.6);
        p->setPen(QPen(border, 1));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(pill, 8, 8);
        p->setPen(c);
        p->drawText(pill, Qt::AlignCenter, text);
        p->restore();
    }
};

// Sem acento e em minúsculas, pra "memoria" achar "Memória".
QString foldForSearch(const QString& s)
{
    const QString d = s.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(d.size());
    for (const QChar c : d)
        if (c.category() != QChar::Mark_NonSpacing) out.append(c.toLower());
    return out;
}

constexpr int kPanelWidth = 980;
constexpr int kPanelHeight = 640;
constexpr int kMinWidth = 760;
constexpr int kMinHeight = 480;
constexpr int kGapBelowAnchor = 6;
constexpr int kSidebarWidth = 212;
constexpr int kRailWidth = 54;
constexpr int kTocWidth = 190;
constexpr int kThumbWidth = 480;
// Larguras de exibição das screenshots — aumentadas propositalmente acima da
// resolução nativa de várias delas (leve upscale) porque a qualidade das
// capturas é boa o bastante pra aguentar e, em compensação, ficam legíveis
// sem precisar clicar pra expandir toda hora (pedido do usuário 2026-07-18:
// "aumenta um pouco o tamanho dos prints").
constexpr int kManuscriptsThumbWidth = 460; // nativo 372x246
constexpr int kDrawersCreateThumbWidth  = 650; // nativo 556
constexpr int kDrawersOpenThumbWidth    = 460; // nativo 372
constexpr int kDrawersMenuThumbWidth    = 400; // nativo 300
constexpr int kExportTopbarThumbWidth   = 340; // nativo 245
constexpr int kExportPanelThumbWidth    = 500; // nativo 475
constexpr int kEditorToolbarThumbWidth      = 720; // nativo 893
constexpr int kEditorSelectionThumbWidth    = 500; // nativo 448
constexpr int kEditorFocusModesThumbWidth   = 160; // nativo 76 — recorte minúsculo, upscale maior
constexpr int kEditorFocusOnThumbWidth      = 320; // nativo 230
constexpr int kEditorPageConfigThumbWidth   = 480; // nativo 410
constexpr int kCounterThumbWidth       = 360; // nativo 269
constexpr int kCounterPanelThumbWidth  = 340; // nativo 260 (recorte alto, 260x865)
constexpr int kCalendarThumbWidth      = 520; // nativo 474
constexpr int kStatisticsThumbWidth    = 420; // nativo 337
constexpr int kOffDaysThumbWidth       = 340; // nativo 255
constexpr int kRefMenuOpenThumbWidth   = 500; // nativo 498, recorte alto
constexpr int kRefMenuSearchThumbWidth = 500; // nativo 460
constexpr int kRefMenuEditThumbWidth   = 480; // nativo 454, recorte alto
constexpr int kTimelinePanelThumbWidth       = 620; // nativo 748
constexpr int kTimelineManuscriptThumbWidth  = 420; // nativo 341
constexpr int kTimelineChapterSceneThumbWidth = 420; // nativo 344
constexpr int kTimelineCharactersThumbWidth  = 560; // nativo 621
constexpr int kTimelineEventCreatorThumbWidth = 380; // nativo 480x621, recorte alto
constexpr int kTimelineCreatorThumbWidth     = 420; // nativo 408
constexpr int kTimelineModeTrackThumbWidth   = 520; // nativo 561
constexpr int kTimelineModeBranchesThumbWidth = 620; // nativo 864
constexpr int kTimelineModeSpiralThumbWidth  = 520; // nativo 667
constexpr int kTimelineLineFocusThumbWidth   = 640; // nativo 893
constexpr int kCoverContextMenuThumbWidth = 320; // nativo 262
constexpr int kCoverAppEditingThumbWidth  = 640; // nativo 936
constexpr int kCreateDocSelectMenuThumbWidth = 560; // nativo 563
constexpr int kCreateDocDialogThumbWidth  = 480; // nativo 559
constexpr int kThemePanelThumbWidth = 620; // nativo 811
constexpr int kThemeFavoriteThumbWidth = 620; // nativo 793
constexpr int kThemeDuplicateThumbWidth = 620; // nativo 801
constexpr int kThemeEditorThumbWidth = 640; // nativo 968
constexpr int kThemeTimeChangeThumbWidth = 620; // nativo 799
constexpr int kMarkerSelectMenuThumbWidth   = 500; // nativo 455
constexpr int kMarkerColorPickerThumbWidth  = 300; // nativo 284
constexpr int kMarkerCommentPickerThumbWidth = 340; // nativo 328
constexpr int kMarkerCommentHoverThumbWidth = 380; // nativo 340
constexpr int kMarkerPlainThumbWidth        = 400; // nativo 387
constexpr int kMemoryCreationMenuThumbWidth = 480; // nativo 446
constexpr int kMemoryCreationDialogThumbWidth = 380; // nativo 338
constexpr int kMemoryPensarioTabThumbWidth  = 420; // nativo 395
constexpr int kBuilderSystemThumbWidth    = 720; // nativo 1400
constexpr int kBuilderTerritoryThumbWidth = 720; // nativo 1400
constexpr int kPensarioPanelThumbWidth      = 420; // nativo 376
constexpr int kPensarioCreateNote1ThumbWidth = 420; // nativo 377
constexpr int kPensarioCreateNote2ThumbWidth = 400; // nativo 352
constexpr int kPensarioDialogueThumbWidth   = 420; // nativo 385
constexpr int kPensarioNameGeneratorThumbWidth = 420; // nativo 397
constexpr int kBondOptionThumbWidth   = 260; // nativo 166
constexpr int kBondCreatorThumbWidth  = 420; // nativo 363
constexpr int kBondDisplayThumbWidth  = 380; // nativo 365, recorte alto (365x715)
constexpr int kMapPanelThumbWidth     = 700; // nativo 798
constexpr int kMapNoTextureThumbWidth = 680; // nativo 710
constexpr int kMapPinThumbWidth       = 480; // nativo 511
constexpr int kGlossaryTabThumbWidth    = 320; // nativo 380, recorte alto (380x560)
constexpr int kGlossaryAddThumbWidth    = 360; // nativo 372
constexpr int kGlossaryInTextThumbWidth = 620; // nativo 720 (ficha montada por cima)
constexpr int kStatsTopbarButtonThumbWidth = 460; // nativo 507
constexpr int kStatsPanelThumbWidth = 480; // nativo 555, recorte alto (555x937)
constexpr int kStatsCharPanelThumbWidth = 470; // nativo 546, recorte alto (546x918)
constexpr int kStatsChemistryDialogThumbWidth = 680; // nativo 980
constexpr int kAmbiencePanelThumbWidth = 380; // nativo 321
constexpr int kReminderPanelThumbWidth = 420; // nativo 389
constexpr int kReminderNotificationThumbWidth = 380; // nativo 295
constexpr int kSceneVarManuscriptThumbWidth = 400; // nativo 282
constexpr int kSceneVarButtonThumbWidth = 620; // nativo 570, recorte largo e baixo
constexpr int kSceneVarOptionsThumbWidth = 380; // nativo 308
}

HelpPanel::HelpPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("helpPanel"));
    // Janela normal (não-modal, com decoração própria do SO): fica aberta ao
    // lado do app, sem fechar sozinha quando o usuário clica de volta no
    // editor pra seguir as instruções.
    setWindowFlags(Qt::Window);
    setWindowTitle(tr("Ajuda"));
    setMinimumSize(kMinWidth, kMinHeight);
    resize(kPanelWidth, kPanelHeight);

    buildTopics();
    buildUi();
    applyTheme();

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, [this]() {
        applyTheme();
        refreshRailIcons();
        updateContent();   // as cores do título e dos passos vêm do tema
    });

    // Abre no último tópico lido (ou em "Comece aqui").
    QString last = QSettings().value(QStringLiteral("help/lastTopic"), QStringLiteral("comece-aqui")).toString();
    if (groupOf(last) < 0) last = QStringLiteral("comece-aqui");
    selectTopic(last);

    hide();
}

// Tópicos e grupos. Os rótulos vêm do Mira 1; a ordem dentro dos grupos é a
// de quem está aprendendo (primeiro o que se usa todo dia).
void HelpPanel::buildTopics()
{
    m_topics = {
        { QStringLiteral("comece-aqui"), tr("Comece aqui") },
        { QStringLiteral("manuscritos"), tr("Manuscritos") },
        { QStringLiteral("gavetas"), tr("Gavetas") },
        { QStringLiteral("exportacao"), tr("Exportação") },
        { QStringLiteral("editor"), tr("Editor") },
        { QStringLiteral("meta-diaria-contador"), tr("Meta Diária e Contador") },
        { QStringLiteral("menu-referencia"), tr("Menu de Referência") },
        { QStringLiteral("funcao-timeline"), tr("Função Timeline") },
        { QStringLiteral("criar-capas"), tr("Criar capas") },
        { QStringLiteral("atalhos-teclado"), tr("Atalhos de teclado") },
        { QStringLiteral("marcadores"), tr("Marcadores e Comentários") },
        { QStringLiteral("memorias"), tr("Memórias") },
        { QStringLiteral("criar-documentos"), tr("Criar Documentos a partir do texto ou comentários") },
        { QStringLiteral("funcao-temas"), tr("Função de Temas") },
        { QStringLiteral("construtor"), tr("Criador de Mundos") },
        { QStringLiteral("pensario"), tr("Pensário") },
        { QStringLiteral("estatisticas"), tr("Estatísticas") },
        { QStringLiteral("vinculos"), tr("Vínculos") },
        { QStringLiteral("mapa-mundi"), tr("Mapa-múndi") },
        { QStringLiteral("glossario"), tr("Glossário") },
        { QStringLiteral("som-imersivo"), tr("Som Imersivo") },
        { QStringLiteral("lembretes"), tr("Lembretes") },
        { QStringLiteral("variacao-cenas"), tr("Variação de cenas") },
    };
    m_groups = {
        { tr("Começo"),    QStringLiteral(":/icons/home.svg"),
          { QStringLiteral("comece-aqui"), QStringLiteral("atalhos-teclado") } },
        { tr("Escrever"),  QStringLiteral(":/icons/edit.svg"),
          { QStringLiteral("editor"), QStringLiteral("manuscritos"), QStringLiteral("variacao-cenas"),
            QStringLiteral("meta-diaria-contador"), QStringLiteral("marcadores"), QStringLiteral("som-imersivo"),
            QStringLiteral("lembretes") } },
        { tr("Organizar"), QStringLiteral(":/icons/pensario.svg"),
          { QStringLiteral("pensario"), QStringLiteral("memorias"), QStringLiteral("glossario"),
            QStringLiteral("estatisticas"), QStringLiteral("funcao-timeline"), QStringLiteral("criar-documentos") } },
        { tr("Mundo"),     QStringLiteral(":/icons/worldmap.svg"),
          { QStringLiteral("gavetas"), QStringLiteral("construtor"), QStringLiteral("vinculos"),
            QStringLiteral("mapa-mundi"), QStringLiteral("menu-referencia") } },
        { tr("Publicar"),  QStringLiteral(":/icons/ereader.svg"),
          { QStringLiteral("exportacao"), QStringLiteral("criar-capas"), QStringLiteral("funcao-temas") } },
    };
}

void HelpPanel::buildUi()
{
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Trilho: os grupos e, embaixo, a busca.
    auto* rail = new QWidget(this);
    rail->setObjectName(QStringLiteral("helpRail"));
    rail->setFixedWidth(kRailWidth);
    auto* railLay = new QVBoxLayout(rail);
    railLay->setContentsMargins(8, 12, 8, 12);
    railLay->setSpacing(6);
    for (int g = 0; g < m_groups.size(); ++g) {
        auto* b = new QToolButton(rail);
        b->setObjectName(QStringLiteral("helpRailBtn"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(m_groups.at(g).label);
        b->setFixedSize(38, 38);
        b->setIconSize(QSize(18, 18));
        connect(b, &QToolButton::clicked, this, [this, g]() { selectGroup(g); });
        railLay->addWidget(b, 0, Qt::AlignHCenter);
        m_railButtons << b;
    }
    railLay->addStretch(1);
    m_searchButton = new QToolButton(rail);
    m_searchButton->setObjectName(QStringLiteral("helpRailBtn"));
    m_searchButton->setCheckable(true);
    m_searchButton->setCursor(Qt::PointingHandCursor);
    m_searchButton->setToolTip(tr("Buscar na Ajuda"));
    m_searchButton->setFixedSize(38, 38);
    m_searchButton->setIconSize(QSize(18, 18));
    connect(m_searchButton, &QToolButton::clicked, this, [this]() { setSearchMode(!m_searchMode); });
    railLay->addWidget(m_searchButton, 0, Qt::AlignHCenter);
    refreshRailIcons();
    root->addWidget(rail);

    // Lista do grupo (ou resultados da busca).
    auto* side = new QWidget(this);
    side->setObjectName(QStringLiteral("helpSidebar"));
    side->setFixedWidth(kSidebarWidth);
    auto* sideLay = new QVBoxLayout(side);
    sideLay->setContentsMargins(8, 12, 8, 12);
    sideLay->setSpacing(6);
    m_listTitle = new QLabel(side);
    m_listTitle->setObjectName(QStringLiteral("helpListTitle"));
    m_listTitle->setContentsMargins(8, 4, 0, 4);
    sideLay->addWidget(m_listTitle);
    m_searchEdit = new QLineEdit(side);
    m_searchEdit->setObjectName(QStringLiteral("helpSearch"));
    m_searchEdit->setPlaceholderText(tr("O que você quer fazer?"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->hide();
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() { runSearch(); });
    sideLay->addWidget(m_searchEdit);
    m_list = new QListWidget(side);
    m_list->setObjectName(QStringLiteral("helpList"));
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setWordWrap(true);
    m_list->setItemDelegate(new HelpListDelegate(m_list));
    connect(m_list, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem*, QListWidgetItem*) { onTopicSelected(); });
    sideLay->addWidget(m_list, 1);
    root->addWidget(side);

    // Texto do tópico, numa coluna de leitura.
    m_content = new QTextBrowser(this);
    m_content->setObjectName(QStringLiteral("helpContent"));
    m_content->setOpenExternalLinks(false);
    m_content->setOpenLinks(false);
    m_content->setFrameShape(QFrame::NoFrame);
    m_content->setLineWrapMode(QTextEdit::WidgetWidth);
    m_content->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_content->document()->setDocumentMargin(30);
    m_content->document()->documentLayout()->registerHandler(
        QTextFormat::ImageObject, new SmoothImageHandler(m_content));
    connect(m_content, &QTextBrowser::anchorClicked, this, &HelpPanel::onAnchorClicked);
    connect(m_content->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() { updateTocHighlight(); });
    root->addWidget(m_content, 1);

    // "Neste tópico": os passos numerados do tópico, na margem.
    m_tocPanel = new QWidget(this);
    m_tocPanel->setObjectName(QStringLiteral("helpToc"));
    m_tocPanel->setFixedWidth(kTocWidth);
    m_tocLayout = new QVBoxLayout(m_tocPanel);
    m_tocLayout->setContentsMargins(4, 30, 16, 16);
    m_tocLayout->setSpacing(0);
    root->addWidget(m_tocPanel);
}

void HelpPanel::refreshRailIcons()
{
    const QColor muted = Theme::toColor(Theme::textMuted());
    const QColor bright = Theme::toColor(Theme::textBright());
    for (int g = 0; g < m_railButtons.size(); ++g)
        m_railButtons.at(g)->setIcon(IconUtils::loadToolbarIcon(m_groups.at(g).icon, muted, bright, bright, QSize(18, 18)));
    if (m_searchButton)
        m_searchButton->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/search.svg"), muted, bright, bright, QSize(18, 18)));
}

void HelpPanel::applyTheme()
{
    setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#helpPanel { background: %1; }"
        "QWidget#helpRail { background: %6; border-right: 1px solid %2; }"
        "QToolButton#helpRailBtn { background: transparent; border: none; border-radius: @radius-item; }"
        "QToolButton#helpRailBtn:hover { background: %5; }"
        "QToolButton#helpRailBtn:checked { background: %7; border-left: 3px solid %8; }"
        "QWidget#helpSidebar { background: %1; border-right: 1px solid %2; }"
        "QLabel#helpListTitle { color: %4; font-size: 10px; font-weight: bold; letter-spacing: 1px; }"
        "QLineEdit#helpSearch { background: %9; color: %3; border: 1px solid %2; border-radius: @radius-control; padding: 6px 8px; font-size: 12.5px; }"
        "QListWidget#helpList { background: transparent; color: %3; border: none; outline: 0; font-size: 13px; }"
        "QListWidget#helpList::item { padding: 7px 8px; border-radius: @radius-item; }"
        "QListWidget#helpList::item:hover { background: %5; }"
        "QListWidget#helpList::item:selected { background: %7; color: %3; }"
        // padding 0: o QTextEdit global tem 80/100px de padding (é do editor).
        "QTextBrowser#helpContent { background: transparent; color: %3; border: none; padding: 0; font-size: 14px; }"
        "QWidget#helpToc { background: transparent; }"
        "QLabel#helpTocTitle { color: %4; font-size: 10px; font-weight: bold; letter-spacing: 1px; padding: 0 0 8px 0; }"
        "QPushButton#helpTocItem { text-align: left; background: transparent; color: %4; border: none;"
        "  border-left: 2px solid %2; padding: 4px 0 4px 10px; font-size: 12px; }"
        "QPushButton#helpTocItem:hover { color: %3; }"
        "QPushButton#helpTocItem[current=\"true\"] { color: %3; border-left: 2px solid %8; }"
    ).arg(Theme::panelBackground(),   // 1
          Theme::subtleBorder(),      // 2
          Theme::textPrimary(),       // 3
          Theme::textMuted(),         // 4
          Theme::hoverOverlay(),      // 5
          Theme::appBackground(),     // 6
          Theme::accentInfoSoft(),    // 7
          Theme::accentDefault(),     // 8
          Theme::inputBackground())));// 9
}

int HelpPanel::groupOf(const QString& id) const
{
    for (int g = 0; g < m_groups.size(); ++g)
        if (m_groups.at(g).topicIds.contains(id)) return g;
    return -1;
}

QString HelpPanel::labelOf(const QString& id) const
{
    for (const Topic& t : m_topics)
        if (t.id == id) return t.label;
    return QString();
}

// Tópicos reescritos na versão atual ganham "NOVO" na lista até serem abertos.
bool HelpPanel::isNew(const QString& id) const
{
    static const QHash<QString, QString> changed = {
        { QStringLiteral("glossario"),  QStringLiteral("0.18.1") },
        { QStringLiteral("construtor"), QStringLiteral("0.18.1") },
    };
    const QString v = changed.value(id);
    return !v.isEmpty() && QSettings().value(QStringLiteral("help/seen/") + id).toString() != v;
}

void HelpPanel::rebuildList()
{
    if (!m_list) return;
    m_filling = true;
    m_list->clear();
    const Group& g = m_groups.at(m_group);
    m_listTitle->setText(g.label.toUpper());
    for (const QString& id : g.topicIds) {
        auto* item = new QListWidgetItem(labelOf(id), m_list);
        item->setData(Qt::UserRole, id);
        item->setData(Qt::UserRole + 1, -1);
        item->setData(HelpListDelegate::NewRole, isNew(id));
        if (id == m_selectedId) m_list->setCurrentItem(item);
    }
    m_filling = false;
}

void HelpPanel::selectGroup(int group)
{
    if (group < 0 || group >= m_groups.size()) return;
    // Trocar de grupo abre o primeiro tópico dele.
    selectTopic(m_groups.at(group).topicIds.value(0));
}

void HelpPanel::selectTopic(const QString& id, int step)
{
    const int g = groupOf(id);
    if (g < 0) return;
    const bool fromSearch = m_searchMode;
    if (!fromSearch) {
        m_group = g;
        for (int i = 0; i < m_railButtons.size(); ++i) m_railButtons.at(i)->setChecked(i == g);
    }
    m_selectedId = id;
    QSettings().setValue(QStringLiteral("help/lastTopic"), id);
    if (!fromSearch) rebuildList();
    updateContent();
    if (isNew(id)) {
        // Abriu: tira o "NOVO" (grava a versão em que o tópico mudou).
        QSettings().setValue(QStringLiteral("help/seen/") + id, QStringLiteral("0.18.1"));
        for (int i = 0; i < m_list->count(); ++i)
            if (m_list->item(i)->data(Qt::UserRole).toString() == id)
                m_list->item(i)->setData(HelpListDelegate::NewRole, false);
    }
    if (step >= 0) {
        // O layout do documento só existe depois do setHtml assentar.
        QTimer::singleShot(0, this, [this, step]() {
            m_content->scrollToAnchor(QStringLiteral("step-%1").arg(step + 1));
        });
    }
}

void HelpPanel::onTopicSelected()
{
    if (m_filling) return;
    auto* item = m_list ? m_list->currentItem() : nullptr;
    if (!item) return;
    const QString id = item->data(Qt::UserRole).toString();
    const int step = item->data(Qt::UserRole + 1).toInt();
    if (id == m_selectedId && step < 0) return;
    selectTopic(id, step);
}

void HelpPanel::setSearchMode(bool on)
{
    m_searchMode = on;
    m_searchButton->setChecked(on);
    m_searchEdit->setVisible(on);
    for (int i = 0; i < m_railButtons.size(); ++i) m_railButtons.at(i)->setChecked(!on && i == m_group);
    if (on) {
        m_listTitle->setText(tr("BUSCAR"));
        m_searchEdit->setFocus();
        runSearch();
    } else {
        const QSignalBlocker block(m_searchEdit);
        m_searchEdit->clear();
        m_group = qMax(0, groupOf(m_selectedId));
        for (int i = 0; i < m_railButtons.size(); ++i) m_railButtons.at(i)->setChecked(i == m_group);
        rebuildList();
    }
}

// Busca no texto inteiro dos tópicos (não só nos títulos). Cada resultado é
// um tópico, com o passo em que a palavra aparece primeiro.
void HelpPanel::runSearch()
{
    if (m_searchIndex.isEmpty()) {
        for (const Topic& t : m_topics) {
            QStringList steps;
            const QString html = decorate(t.id, contentFor(t.id), &steps);
            // Corta o HTML nas âncoras dos passos: antes da primeira é a introdução.
            static const QRegularExpression anchorRe(QStringLiteral("<a name=\"step-(\\d+)\"></a>"));
            int last = 0, lastStep = -1;
            auto push = [&](int end) {
                const QString plain = QTextDocumentFragment::fromHtml(html.mid(last, end - last)).toPlainText();
                m_searchIndex.append({ t.id, lastStep, lastStep >= 0 ? steps.value(lastStep) : QString(),
                                       foldForSearch(t.label + QLatin1Char(' ') + plain) });
            };
            auto it = anchorRe.globalMatch(html);
            while (it.hasNext()) {
                const auto m = it.next();
                push(m.capturedStart());
                last = m.capturedStart();
                lastStep = m.captured(1).toInt() - 1;
            }
            push(html.size());
        }
    }
    m_filling = true;
    m_list->clear();
    const QString q = foldForSearch(m_searchEdit->text().trimmed());
    if (q.size() >= 2) {
        QSet<QString> seen;
        for (const SearchChunk& c : std::as_const(m_searchIndex)) {
            if (seen.contains(c.topicId) || !c.folded.contains(q)) continue;
            seen.insert(c.topicId);
            const QString text = c.step >= 0
                ? QStringLiteral("%1\n%2. %3").arg(labelOf(c.topicId)).arg(c.step + 1).arg(c.stepTitle)
                : labelOf(c.topicId);
            auto* item = new QListWidgetItem(text, m_list);
            item->setData(Qt::UserRole, c.topicId);
            item->setData(Qt::UserRole + 1, c.step);
        }
        m_listTitle->setText(seen.isEmpty() ? tr("NADA ENCONTRADO") : tr("%n TÓPICO(S)", "", int(seen.size())));
    } else {
        m_listTitle->setText(tr("BUSCAR"));
    }
    m_filling = false;
}

QString HelpPanel::contentFor(const QString& id) const
{
    if (id == QStringLiteral("comece-aqui")) return startHereContent();
    if (id == QStringLiteral("manuscritos")) return manuscriptsContent();
    if (id == QStringLiteral("gavetas")) return drawersContent();
    if (id == QStringLiteral("exportacao")) return exportContent();
    if (id == QStringLiteral("editor")) return editorContent();
    if (id == QStringLiteral("meta-diaria-contador")) return dailyGoalCounterContent();
    if (id == QStringLiteral("menu-referencia")) return referenceMenuContent();
    if (id == QStringLiteral("atalhos-teclado")) return keyboardShortcutsContent();
    if (id == QStringLiteral("marcadores")) return markersContent();
    if (id == QStringLiteral("memorias")) return memoriesContent();
    if (id == QStringLiteral("funcao-timeline")) return timelineContent();
    if (id == QStringLiteral("criar-capas")) return coverCreatorContent();
    if (id == QStringLiteral("criar-documentos")) return createDocumentsContent();
    if (id == QStringLiteral("funcao-temas")) return themesContent();
    if (id == QStringLiteral("construtor")) return builderContent();
    if (id == QStringLiteral("pensario")) return pensarioContent();
    if (id == QStringLiteral("estatisticas")) return statsContent();
    if (id == QStringLiteral("vinculos")) return bondsContent();
    if (id == QStringLiteral("mapa-mundi")) return worldMapContent();
    if (id == QStringLiteral("glossario")) return glossaryContent();
    if (id == QStringLiteral("som-imersivo")) return ambienceContent();
    if (id == QStringLiteral("lembretes")) return remindersContent();
    if (id == QStringLiteral("variacao-cenas")) return sceneVariationContent();
    // Placeholder até reescrevermos o conteúdo das demais seções.
    return QStringLiteral("<p>%1</p>")
        .arg(tr("Conteúdo desta seção será adicionado em breve."));
}

// Conteúdo escrito pelo usuário em help-panel/start-here/, montado aqui em HTML.
// A imagem fica em miniatura (números do callout ainda legíveis) e é
// clicável — abre ampliada numa janela própria via onAnchorClicked/openImageZoom.
QString HelpPanel::startHereContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr("Olá, seja bem-vindo ao Qenna Writer!"));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Qenna Writer é um programa voltado para a escrita criativa e conta com "
        "inúmeras opções para torná-la mais fácil, organizada e aprofundada."));
    html += QStringLiteral("<p>%1</p>").arg(tr("Vamos começar!"));
    html += QStringLiteral("<p>%1</p>").arg(tr("Aqui, está a sua interface básica de usuário."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/start-here/numbered-ui.png' style='text-decoration:none;'>"
        "<img src=':/help/start-here/numbered-ui.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O editor é dividido em seções, para que tudo seja acessível no mesmo lugar sem "
        "que se espalhe. Conforme a screenshot acima, aqui estão o que cada número é:"));

    struct Step { const char* title; const char* desc; };
    const Step steps[] = {
        { QT_TR_NOOP("Opções diversas do editor de texto."),
          QT_TR_NOOP("Negrito, itálico, tamanho e fonte, alinhamento, adicionar imagens.") },
        { QT_TR_NOOP("Opções básicas do programa."),
          QT_TR_NOOP("Retornar ao menu, criar novo projeto, carregar, salvar e exportar documentos.") },
        { QT_TR_NOOP("Acesso geral de projeto e planejamento."),
          QT_TR_NOOP("Informações da obra, Lousa, Timeline, Manuscrito e organização de Grupos.") },
        { QT_TR_NOOP("Gavetas."),
          QT_TR_NOOP("Para criar novas gavetas e acessar as que já existem. Não há um limite de quantas gavetas você pode criar.") },
        { QT_TR_NOOP("Contador."),
          QT_TR_NOOP("Aqui você visualiza o seu contador de palavras e acessa o gerenciamento das suas metas de escrita.") },
        { QT_TR_NOOP("Documento em edição."),
          QT_TR_NOOP("Exibe o documento que está sendo editado no momento.") },
        { QT_TR_NOOP("Opções gerais."),
          QT_TR_NOOP("Aqui, você consegue ativar o Editor Focado e o Modo Foco (que apesar dos nomes "
                     "similares, são funções diferentes). O Editor Focado recua toda a UI e deixa "
                     "somente a página em exibição. O Modo Foco esmaece o texto e foca somente no "
                     "parágrafo que está sendo escrito.") },
        { QT_TR_NOOP("Opções de Ambiente de Trabalho."),
          QT_TR_NOOP("Acesso a função de Lembretes e Som Imersivo.") },
        { QT_TR_NOOP("Opções de Referência e Criação."),
          QT_TR_NOOP("Aqui, você pode acessar as ferramentas: Construtor, Pensário, Menu de Referência "
                     "e Estatísticas.") },
        { QT_TR_NOOP("Configurações do programa."),
          QT_TR_NOOP("Alterar o Tema, configurar e ativar o modo tela cheia.") },
    };

    // Numeração manual em vez de <ol>: o motor de rich text do Qt tem
    // problemas de layout com listas (indent/margin bugados, empurram o
    // texto todo pra direita). Um <p> com o número em negrito é confiável.
    for (int i = 0; i < static_cast<int>(sizeof(steps) / sizeof(steps[0])); ++i) {
        html += QStringLiteral("<p style='margin-bottom:12px;'><b>%1- %2</b><br>%3</p>")
            .arg(QString::number(i + 1), tr(steps[i].title), tr(steps[i].desc));
    }

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "A UI básica do app é pensada para manter as opções fora do seu caminho e não lhe "
        "atrapalharem durante o processo de escrita. Pode parecer que são muitos botões, mas "
        "acredite, é fácil de se acostumar."));
    return html;
}

// Conteúdo escrito pelo usuário em help-panel/manuscripts/, montado aqui em HTML.
QString HelpPanel::manuscriptsContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr("Bem-vindo à gaveta de Manuscrito — ou só Manuscrito."));
    html += QStringLiteral("<p>%1</p>").arg(tr("É aqui onde você gerencia os manuscritos do seu projeto."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Não há um limite de quantos manuscritos você pode ter. Você pode escrever uma saga de 60 "
        "livros no mesmo projeto — em teoria pelo menos."));
    html += QStringLiteral("<p>%1</p>").arg(tr("A gaveta de Manuscrito é bem direta, então vamos aos detalhes."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/manuscripts/manuscript-tutorial.png' style='text-decoration:none;'>"
        "<img src=':/help/manuscripts/manuscript-tutorial.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kManuscriptsThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Barra do Manuscrito"),
             tr("É nessa barra que você escolhe o manuscrito aberto. Você também pode renomear e "
                "excluir manuscritos clicando com o botão direito nela quando o manuscrito que quiser "
                "renomear ou excluir esteja selecionado."));

    html += QStringLiteral(
        "<p style='margin-bottom:4px;'><b>2- %1</b></p>"
        "<p style='margin-bottom:4px;'>%2</p>"
        "<p style='margin-bottom:4px;'>%3</p>"
        "<p style='margin-bottom:12px;'>%4</p>")
        .arg(tr("Criar novo capítulo"),
             tr("Nesse botão, você cria novos capítulos."),
             tr("É necessário que já haja um manuscrito para que capítulos sejam criados. Caso não "
                "tenha um, o app chamará a opção para criá-lo."),
             tr("Ao criar um novo capítulo, o app chamará uma janela pedindo duas informações: o nome "
                "do capítulo e quando ele se passa. Sobre quando ele se passa, essa informação é usada "
                "para organização da linha do tempo na Timeline, mas falaremos disso quando chegar a "
                "hora."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>3- %1</b><br>%2</p>")
        .arg(tr("É através desse botão que você abre a gaveta de Manuscritos."),
             tr("Ele fica na barra à esquerda, acessível a qualquer momento."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>4- %1</b><br>%2</p>")
        .arg(tr("Capítulos"),
             tr("Os capítulos criados aparecem nessa lista. Para acessá-los no editor, é somente "
                "clicar. Você também pode usar o clique direito para acessar as opções dele (renomear, "
                "excluir, abrir no Menu de Referência, definir elementos presentes e etc)."));

    html += QStringLiteral(
        "<p style='margin-bottom:4px;'><b>5- %1</b></p>"
        "<p style='margin-bottom:4px;'>%2</p>"
        "<p style='margin-bottom:2px;'>%3</p>"
        "<p style='margin:2px 0 2px 16px; font-family:monospace;'>%4</p>"
        "<p style='margin-bottom:12px;'>%5</p>")
        .arg(tr("Cenas"),
             tr("As cenas dos capítulos são exibidas abaixo deles conforme a imagem. Ao usar o clique "
                "direito em uma, você também pode acessar as opções dela."),
             tr("Para criar uma cena, digite quatro riscas em uma linha vazia e aperte Enter:"),
             QStringLiteral("----"),
             tr("Ao fazer isso, o app cria uma nova cena automaticamente. Com isso, você pode gerar "
                "novas cenas a qualquer momento no seu capítulo, sem atrapalhar o seu fluxo de "
                "escrita."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>6- %1</b><br>%2</p>")
        .arg(tr("Criar Manuscrito"),
             tr("Aperte nesse botão para criar novos Manuscritos."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/drawers/, montado aqui em HTML.
QString HelpPanel::drawersContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "As gavetas são uma parte fundamental da organização do seu projeto no app. Nelas, "
        "você pode dividir os seus documentos com base em contexto e função, mantendo cada "
        "coisa em seu devido lugar."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Não há um limite de gavetas — você pode ter quantas o seu projeto precisar."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/drawers/drawer-create.png' style='text-decoration:none;'>"
        "<img src=':/help/drawers/drawer-create.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kDrawersCreateThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Para criar uma gaveta, clique no botão de \"+\" que aparece na imagem acima (1). Ao "
        "clicar nele, surge um popup de criação (2)."));
    html += QStringLiteral("<p>%1</p>").arg(tr("Nesse popup, você tem as seguintes opções:"));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Ícone e cor da gaveta."),
             tr("Você pode escolher qualquer ícone da lista e escolher uma cor na paleta de cores."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Nome da gaveta."),
             tr("Defina o nome da sua gaveta. Com base no nome, a gaveta pode receber elementos "
                "de forma automática."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>3- %1</b><br>%2</p>")
        .arg(tr("Elemento das gavetas."),
             tr("Os elementos definem o que aquela gaveta armazena. Atualmente, há três opções: "
                "Personagens, Cenários e Objetos. Gavetas com esses três elementos têm um padrão "
                "de exibição diferente e recebem opções adicionais com base no elemento dela — "
                "falaremos dessas opções abaixo."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Após criar sua gaveta, você a encontrará na barra da esquerda e pode clicar para abri-la."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/drawers/drawer-open.png' style='text-decoration:none;'>"
        "<img src=':/help/drawers/drawer-open.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kDrawersOpenThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Dentro da gaveta aberta, você tem as opções a seguir (seguindo as áreas numeradas da "
        "imagem acima)."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Fixar, alternar exibição entre listas e blocos, "
                "configurar o tamanho dos blocos e ordenar os itens da gaveta."),
             tr("A ordenação inclui opções como A-Z e ordem de criação, entre outras."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("O botão grande e colorido."),
             tr("Nele, você cria novos documentos dentro dessa gaveta. Nas opções abaixo dele, "
                "você consegue criar pastas dentro da gaveta e acessar pastas que já existam no "
                "projeto."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Algumas opções só existem em gavetas de elementos (como o tamanho dos blocos)."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Para os documentos e itens dentro das gavetas, você encontra as seguintes opções ao "
        "dar um clique direito neles:"));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/drawers/drawer-item-menu.png' style='text-decoration:none;'>"
        "<img src=':/help/drawers/drawer-item-menu.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kDrawersMenuThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Editar metadados..."),
             tr("Altera nome, foto e demais opções. Para personagens, há opções adicionais, "
                "como definir apelido ou narrador."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Remover elemento."),
             tr("Remove o elemento de um documento (torna um cenário um documento comum, por "
                "exemplo)."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>3- %1</b><br>%2</p>")
        .arg(tr("Adicionar ao grupo."),
             tr("Adiciona o documento a um grupo específico da função de Grupos (falaremos dela "
                "mais tarde)."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>4- %1</b><br>%2</p>")
        .arg(tr("Abrir no Menu de Referência."),
             tr("Abre o documento em questão no Menu de Referência."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>5- %1</b><br>%2</p>")
        .arg(tr("Mover para."),
             tr("Move o documento para outra gaveta."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>6- %1</b><br>%2</p>")
        .arg(tr("Excluir."),
             tr("Exclui o documento (quem diria, hein?)."));

    return html;
}

// Conteúdo escrito pelo assistente em help-panel/export/, revisado pelo usuário,
// montado aqui em HTML.
QString HelpPanel::exportContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "A exportação é onde você tira o que escreveu de dentro do app e transforma num "
        "arquivo de verdade, pronto pra ler em outro programa, mandar pra alguém ou até "
        "publicar."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra abrir, clique no botão \"Exportar\" na barra de ferramentas do topo do editor."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/export/export-topbar.png' style='text-decoration:none;'>"
        "<img src=':/help/export/export-topbar.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kExportTopbarThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Isso abre uma janela com três partes: uma árvore de seleção, as opções de "
        "formato/modo, e o botão de exportar."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/export/export-panel.png' style='text-decoration:none;'>"
        "<img src=':/help/export/export-panel.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kExportPanelThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral(
        "<p style='margin-bottom:4px;'><b>1- %1</b></p>"
        "<p style='margin-bottom:4px;'>%2</p>"
        "<p style='margin-bottom:4px;'>%3</p>"
        "<p style='margin-bottom:12px;'>%4</p>")
        .arg(tr("Árvore de seleção."),
             tr("Você escolhe exatamente o que quer exportar. Ela é dividida em duas seções:"),
             tr("Manuscritos: mostra cada manuscrito do projeto com seus capítulos dentro. "
                "Você pode marcar o manuscrito inteiro (marca todos os capítulos de uma vez) "
                "ou só capítulos específicos. Gavetas: mostra suas gavetas, com pastas e "
                "documentos dentro delas, do mesmo jeito."),
             tr("Os botões \"Selecionar tudo\" e \"Desmarcar tudo\" no topo ajudam quando você "
                "quer exportar (quase) tudo de uma vez. O contador no canto (\"X / Y\") mostra "
                "quantos itens estão marcados no momento."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Formato."),
             tr("Escolha entre ODT, PDF, EPUB ou DOCX, clicando no botão do formato desejado."));

    html += QStringLiteral(
        "<p style='margin-bottom:4px;'><b>3- %1</b></p>"
        "<p style='margin-bottom:4px;'>%2</p>"
        "<p style='margin-bottom:4px;'>%3</p>"
        "<p style='margin-bottom:12px;'>%4</p>")
        .arg(tr("Modo do manuscrito."),
             tr("Só importa se você marcou mais de um capítulo:"),
             tr("Documento único: junta todos os capítulos marcados num arquivo só, na ordem "
                "deles."),
             tr("Capítulos separados: gera um arquivo pra cada capítulo."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>4- %1</b><br>%2</p>")
        .arg(tr("Marcadores."),
             tr("Se você usa a função de Marcadores (marcações de texto tipo grifo) no seu "
                "texto, aqui você escolhe se eles aparecem no arquivo exportado ou são "
                "removidos."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Um aviso que já vem na própria janela: se uma cena tiver variações criadas, a "
        "exportação sempre usa a variação marcada como primária — as outras não entram no "
        "arquivo."));

    html += QStringLiteral(
        "<p style='margin-bottom:4px;'>%1</p>"
        "<p style='margin-bottom:4px;'>%2</p>"
        "<p style='margin-bottom:12px;'>%3</p>")
        .arg(tr("Depois de escolher tudo, clique em \"Exportar (N)\" — o número mostra quantos "
                "itens serão exportados. O app pergunta onde salvar:"),
             tr("Se for só 1 arquivo, ele pede direto o nome/local do arquivo."),
             tr("Se for mais de um (ex.: \"capítulos separados\" com vários marcados, ou "
                "capítulos + itens de gaveta juntos), o app empacota tudo num .zip e pergunta "
                "onde salvar esse zip."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Durante a exportação (principalmente em PDF, que pode demorar um pouco mais), "
        "aparece um aviso na tela — é normal, só esperar terminar. No final, uma confirmação "
        "de sucesso aparece."));

    return html;
}

// Conteúdo escrito pelo assistente em help-panel/editor/, revisado pelo usuário,
// montado aqui em HTML.
QString HelpPanel::editorContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Editor é onde você realmente escreve — é a página em branco (ou não tão em branco "
        "assim) que ocupa o centro da tela. É a parte do app onde você passará mais tempo, "
        "então foi construída para ser confortável."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>1- %1</b></p>")
        .arg(tr("Formatação (barra de ferramentas do topo)."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/editor/toolbar-options.png' style='text-decoration:none;'>"
        "<img src=':/help/editor/toolbar-options.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kEditorToolbarThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Você encontrará as opções de Fonte, Tamanho, Espaçamento, Indentação (\"¶\"), "
        "Alinhamento, Negrito/Itálico/Sublinhado/Tachado. Diferente de negrito/itálico."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Fonte/Tamanho/Espaçamento/Indentação não exigem seleção — valem pro documento "
        "inteiro que você está editando. O botão Espaçamento abre um popup com entre-linhas, "
        "espaço antes e depois do parágrafo."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "O botão Alinhamento tem um extra: \"Aplicar em\" (só esse doc / todos / manuscrito / "
        "gavetas). Isso é diferente de Configurações → Escrita, que mexe no tamanho "
        "da folha e margens (visual do app, não do texto)."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Inserir imagem."),
             tr("Seletor de arquivo → diálogo com prévia, alinhamento e largura ajustável. "
                "Clicar na imagem depois de inserida abre um menu pra realinhar/redimensionar "
                "sem refazer tudo."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>3- %1</b></p>")
        .arg(tr("Selecionar um trecho."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Um menu flutuante aparece acima da seleção: formatação, marcador, marcador com "
        "comentário, Glossário, criar documento, criar evento na timeline, adicionar à "
        "memória, alinhamento. Com essa opção, você mudar o alinhamento ou adicionar negrito "
        "em um trecho específico, por exemplo."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/editor/selection-menu.png' style='text-decoration:none;'>"
        "<img src=':/help/editor/selection-menu.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kEditorSelectionThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Falaremos das Memórias, Glossário e Timeline mais tarde."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>4- %1</b></p>")
        .arg(tr("Editor Focado × Modo Foco."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/editor/focus-modes.png' style='text-decoration:none;'>"
        "<img src=':/help/editor/focus-modes.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kEditorFocusModesThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Editor Focado recua toda a UI, deixa só a página (mouse no topo/esquerda traz de "
        "volta)."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Modo Foco (Ctrl+F10) esmaece o texto todo menos o parágrafo atual (segurar Ctrl "
        "mostra tudo por um instante)."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/editor/focus-mode-on.png' style='text-decoration:none;'>"
        "<img src=':/help/editor/focus-mode-on.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kEditorFocusOnThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Dá pra usar os dois juntos para um modo foco extremo."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>5- %1</b></p>")
        .arg(tr("Opções adicionais."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Além disso, você pode acessar as Configurações e definir a largura e altura da "
        "página."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/editor/page-configuration.png' style='text-decoration:none;'>"
        "<img src=':/help/editor/page-configuration.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kEditorPageConfigThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    return html;
}

// Conteúdo escrito pelo assistente em help-panel/daily-goal-counter/, revisado
// pelo usuário, montado aqui em HTML.
QString HelpPanel::dailyGoalCounterContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Contador é o painel flutuante no canto inferior esquerdo da tela — aquele que "
        "mostra quantas palavras você já escreveu. Além de contar, ele também cuida da sua "
        "meta diária, de sprints de escrita e guarda um histórico completo do seu progresso."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/daily-goal-counter/counter.png' style='text-decoration:none;'>"
        "<img src=':/help/daily-goal-counter/counter.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kCounterThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr("Ele tem dois modos de exibição:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Compacto: mostra dois cartões (por padrão, Palavras e Caracteres) e, se você quiser, "
        "uma barrinha fina de progresso da meta do dia. Você pode alternar o que é exibido "
        "nele usando o clique direito (palavras, tempo de sessão, páginas e etc)."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Completo: abre o painel do contador completo — de onde contar, meta diária, sprint "
        "de escrita e o calendário. Tudo é acessível dentro dele."));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Clique no triângulo pra recolher/expandir o contador menor. Para exibir o painel "
        "completo, clique no corpo do contador pequeno. Pra reduzi-lo novamente, é o mesmo "
        "processo."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/daily-goal-counter/counter-panel.png' style='text-decoration:none;'>"
        "<img src=':/help/daily-goal-counter/counter-panel.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kCounterPanelThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "No painel completo, você encontrará as seguintes opções:"));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>0- %1</b><br>%2</p>")
        .arg(tr("O contador minúsculo."),
             tr("Ainda é exibido no modo completo. Para recuar do modo completo, basta clicar "
                "nele novamente."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("De onde contar."),
             tr("No modo completo, você escolhe o que os cartões contam: só os capítulos do "
                "manuscrito, só o documento que está aberto no editor agora, só as gavetas, ou "
                "tudo junto. Clique com o botão direito nos cartões pra escolher quais "
                "métricas aparecem ali (Palavras, Caracteres, Palavras hoje, Páginas, entre "
                "outras)."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Meta diária."),
             tr("Você define uma meta — em palavras ou em tempo (minutos escrevendo) — e "
                "escolhe se ela conta só os capítulos, só as gavetas, ou os dois juntos. Uma "
                "barra de progresso mostra \"Hoje: X / Y palavras\" (ou minutos), e embaixo "
                "dela aparece quanto tempo falta até a meta reiniciar (à meia-noite). Um botão "
                "de reiniciar deixa você zerar o progresso do dia manualmente, se precisar."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Tem também um pequeno grid de estatísticas ali do lado — streak atual, recorde, se "
        "você já bateu a meta hoje, páginas, e sua média de palavras por dia."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "E existe um sistema de folgas: dependendo de quantos dias seguidos você bate a meta, "
        "o app libera dias de descanso que não quebram seu streak."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>3- %1</b><br>%2</p>")
        .arg(tr("Sprint de escrita."),
             tr("Um sprint é uma corrida contra o tempo: você define uma duração (padrão 25 "
                "minutos) e uma meta de palavras (padrão 300), aperta \"Iniciar sprint\", e um "
                "cronômetro regressivo aparece junto com a contagem de palavras escritas desde "
                "que você começou. Ao final (ou quando você encerrar manualmente), o app avisa "
                "se você bateu a meta do sprint ou não."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>4- %1</b></p>")
        .arg(tr("Calendário."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "No fim do modo completo, um botão \"Exibir calendário\" abre um calendário mensal. "
        "Cada dia mostra até 5 estrelinhas — quanto mais vezes você bateu sua meta diária "
        "naquele dia, mais estrelas. Dias de folga aparecem com um ícone de lua. Clicando num "
        "dia, você vê os detalhes: quantas palavras escreveu e quanto tempo passou escrevendo. "
        "Clicando com o botão direito, você pode marcar ou desmarcar aquele dia como folga, "
        "usando o sistema de folgas (explicado mais abaixo)."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/daily-goal-counter/calendar.png' style='text-decoration:none;'>"
        "<img src=':/help/daily-goal-counter/calendar.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kCalendarThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;margin-top:12px;'><b>5- %1</b></p>")
        .arg(tr("Estatísticas."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Tudo que você faz no editor é contabilizado e salvo — e pode ser consultado."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/daily-goal-counter/statistics.png' style='text-decoration:none;'>"
        "<img src=':/help/daily-goal-counter/statistics.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kStatisticsThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "No modo compacto ou completo, o botão \"Estatísticas\" abre um resumo de tudo que "
        "você já escreveu no projeto: dias com meta batida, streak atual e recorde, total de "
        "palavras e tempo de escrita, seu recorde diário, sua média por dia ativo, qual dia da "
        "semana você mais escreve, e qual documento você mais trabalhou."));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Uma coisa importante: os cartões do topo mostram um total acumulado (de onde você "
        "escolheu contar), a barra da meta mostra seu progresso de hoje, e as Estatísticas "
        "mostram o histórico completo do projeto — são três números diferentes, cada um com "
        "seu propósito."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>6- %1</b></p>")
        .arg(tr("O sistema de folgas."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/daily-goal-counter/off-days.png' style='text-decoration:none;'>"
        "<img src=':/help/daily-goal-counter/off-days.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kOffDaysThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "O sistema de folgas é simples de entender e dinâmico."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Ele é desbloqueado após sua primeira semana completa (7 dias) batendo a meta diária "
        "— a partir daí você pode configurar o seu sistema de folgas. Ele funciona assim: "
        "cada vez que você bater sua meta diária ou manter sua streak pelo tempo configurado "
        "nele, você ganha direito a uma folga, que pode ser marcada no calendário. No dia de "
        "folga marcado, se nada for escrito, sua streak não é perdida — afinal foi uma folga."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Você pode marcar folgas no calendário clicando com o botão direito no dia que deseja "
        "folgar. Também é possível guardar folgas e depois tirá-las todas de uma vez quando "
        "quiser."));

    html += QStringLiteral("<p style='font-style:italic;color:%1;'>%2</p>")
        .arg(Theme::textMuted(), tr(
            "Nota do dev: por favor, ignore minhas metas e números nos prints acima. Como "
            "desenvolvedor, não tenho escrito muito, então meus números estão péssimos, mas "
            "vou voltar aos trilhos em breve, prometo."));

    return html;
}

// Conteúdo escrito pelo assistente em help-panel/reference-menu/, revisado
// pelo usuário, montado aqui em HTML.
QString HelpPanel::referenceMenuContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Menu de Referência é um painel flutuante que deixa você consultar (ou até editar) "
        "outro documento do projeto sem sair do que está escrevendo agora. Quer conferir a "
        "ficha de um personagem, reler uma cena anterior ou dar uma olhada num nó do "
        "Construtor enquanto escreve? É pra isso que ele existe — o editor principal continua "
        "exatamente do jeito que você deixou."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra abrir, clique no botão de Menu de Referência na barra do topo, ou use o atalho "
        "F6."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/reference-menu/refmenu-open.png' style='text-decoration:none;'>"
        "<img src=':/help/reference-menu/refmenu-open.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kRefMenuOpenThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Existem várias formas de abrir um documento dentro dele:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Ctrl+clique em qualquer @menção no seu texto abre automaticamente aquilo que foi "
        "mencionado (uma ficha, um capítulo, uma cena)."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Clique direito num capítulo, cena ou item de gaveta e escolha \"Abrir no Menu de "
        "Referência\"."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Dentro do próprio painel, use os seletores \"Manuscritos ▾\" e \"Gaveta ▾\" no topo "
        "pra navegar até o que você quer."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Use a busca (ícone de lupa, ou Ctrl+Alt+F) pra filtrar manuscritos, capítulos, cenas "
        "e gavetas de uma vez e pular direto pro resultado."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/reference-menu/refmenu-search.png' style='text-decoration:none;'>"
        "<img src=':/help/reference-menu/refmenu-search.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kRefMenuSearchThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Dá pra editar o documento aberto ali? Sim, com algumas restrições. O botão de lápis "
        "no topo do painel liga o modo de edição. Capítulos inteiros e itens de gaveta "
        "(fichas de personagem, cenários, etc.) podem ser editados direto dali. Cenas "
        "individuais, por enquanto, são só leitura. E se o mesmo documento já estiver aberto "
        "no editor principal, a edição fica bloqueada no Menu de Referência — pra evitar você "
        "editar a mesma coisa em dois lugares ao mesmo tempo e perder alguma coisa."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Uma vez ativado o modo edição, ela é feita diretamente na área de visualização do "
        "texto no Menu de Referência, conforme o print abaixo."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/reference-menu/edit-mode.png' style='text-decoration:none;'>"
        "<img src=':/help/reference-menu/edit-mode.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kRefMenuEditThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Outras coisas que você pode fazer no painel:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Arrastar pela alcinha no topo pra mover, ou puxar as bordas pra redimensionar. Duplo "
        "clique na alcinha reseta a posição."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Esconder a área de navegação e deixar só o preview visível, pra ganhar espaço de "
        "leitura."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Ajustar o tamanho da fonte do texto exibido (botão \"Aa\")."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Em gavetas de personagens/cenários/objetos, alternar entre visualização em grade "
        "(com foto) ou lista simples."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Fechar com o \"✕\" ou apertando F6 de novo."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Caso você use múltiplos monitores, o Menu de Referência pode ser arrastado para "
        "outro monitor."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O painel lembra onde você deixou (posição, tamanho, tamanho da fonte) na próxima vez "
        "que abrir o app."));

    return html;
}

// Conteúdo escrito pelo assistente (sem rascunho prévio do usuário — feature
// sem tela própria pra fotografar, então não há screenshots aqui).
QString HelpPanel::keyboardShortcutsContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Uma lista com todos os atalhos de teclado do Qenna Writer, separados por onde eles "
        "atuam. Os atalhos padrão de edição de texto (Ctrl+Z para desfazer, Ctrl+Y/Ctrl+Shift+Z "
        "para refazer, Ctrl+C/Ctrl+X/Ctrl+V para copiar/recortar/colar, Ctrl+A para selecionar "
        "tudo) também funcionam normalmente e não estão listados abaixo."));

    auto key = [](const QString& k) {
        return Theme::qss(QStringLiteral(
            "<span style='background:%1;color:%2;border-radius: @radius-item;padding:1px 7px;"
            "font-family:Consolas,monospace;font-size:12px;'>%3</span>")
            .arg(Theme::hoverOverlay(), Theme::textPrimary(), k));
    };
    auto row = [&key](const QString& k, const QString& desc) {
        return QStringLiteral(
            "<tr>"
            "<td style='padding:3px 14px 3px 0; white-space:nowrap; vertical-align:top;'>%1</td>"
            "<td style='padding:3px 0;'>%2</td>"
            "</tr>").arg(key(k), desc);
    };
    auto group = [](const QString& title) {
        return QStringLiteral("<p style='margin:14px 0 4px 0;'><b>%1</b></p>").arg(title);
    };

    html += group(tr("Painéis e navegação"));
    html += QStringLiteral("<table cellspacing='0' cellpadding='0'>");
    html += row(QStringLiteral("F2"), tr("Abrir a Lousa."));
    html += row(QStringLiteral("F3"), tr("Abrir a Timeline."));
    html += row(QStringLiteral("F4"), tr("Abrir/fechar o Pensário."));
    html += row(QStringLiteral("Shift+F4"), tr("Abrir o Mapa do Pensário."));
    html += row(QStringLiteral("F5"), tr("Abrir os Grupos."));
    html += row(QStringLiteral("F6"), tr("Abrir/fechar o Menu de Referência."));
    html += row(QStringLiteral("F7"), tr("Abrir/fechar os Lembretes."));
    html += row(QStringLiteral("F11"), tr("Ativar/sair da tela cheia."));
    html += row(QStringLiteral("F12"), tr("Voltar ao Menu Principal."));
    html += row(QStringLiteral("Ctrl+F10"), tr("Ativar/desativar o Modo Foco."));
    html += QStringLiteral("</table>");

    html += group(tr("Manuscrito e gavetas"));
    html += QStringLiteral("<table cellspacing='0' cellpadding='0'>");
    html += row(QStringLiteral("Ctrl+S"), tr("Salvar o projeto."));
    html += row(QStringLiteral("Ctrl+Shift+N"), tr("Criar um novo capítulo no manuscrito ativo."));
    html += row(QStringLiteral("Ctrl+N"), tr("Criar um novo item na gaveta aberta no momento."));
    html += row(QStringLiteral("Shift+Page&nbsp;Down"), tr(
        "Ir para o próximo capítulo (ou próximo item da gaveta aberta)."));
    html += row(QStringLiteral("Shift+Page&nbsp;Up"), tr(
        "Ir para o capítulo anterior (ou item anterior da gaveta aberta)."));
    html += QStringLiteral("</table>");

    html += group(tr("Busca"));
    html += QStringLiteral("<table cellspacing='0' cellpadding='0'>");
    html += row(QStringLiteral("Ctrl+F"), tr(
        "Buscar dentro do documento aberto no editor (ou dentro da ficha, se ela estiver "
        "aberta na tela)."));
    html += row(QStringLiteral("Ctrl+Shift+F"), tr("Busca global — em todo o projeto."));
    html += row(QStringLiteral("Ctrl+Alt+F"), tr("Buscar dentro do Menu de Referência."));
    html += row(QStringLiteral("Alt+F"), tr(
        "Buscar dentro do preview do documento já aberto no Menu de Referência."));
    html += QStringLiteral("</table>");

    html += group(tr("Formatação de texto"));
    html += QStringLiteral("<table cellspacing='0' cellpadding='0'>");
    html += row(QStringLiteral("Ctrl+B"), tr("Negrito."));
    html += row(QStringLiteral("Ctrl+I"), tr("Itálico."));
    html += row(QStringLiteral("Ctrl+U"), tr("Sublinhado."));
    html += row(QStringLiteral("Ctrl+Shift+S"), tr("Tachado."));
    html += row(QStringLiteral("Ctrl+-"), tr("Inserir um travessão (—) na posição do cursor."));
    html += QStringLiteral("</table>");

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/markers/, montado aqui em HTML.
QString HelpPanel::markersContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Marcadores são grifos coloridos que você aplica sobre um trecho do texto — com ou "
        "sem um comentário junto. Servem pra sinalizar o que precisa de atenção (\"revisar "
        "esse diálogo\", \"checar esse fato depois\") sem interromper a escrita: o grifo fica "
        "ali, visível, esperando você voltar."));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Pra criar um, selecione o trecho — o menu flutuante de seleção tem duas opções:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'><b>%1</b><br>%2</p>")
        .arg(tr("Marcador"),
             tr("Só pinta o trecho com a cor escolhida, sem comentário."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>%1</b><br>%2</p>")
        .arg(tr("Marcador com comentário"),
             tr("Pinta o trecho e abre um campo de texto pra você escrever uma nota sobre "
                "ele."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/markers/select-menu.png' style='text-decoration:none;'>"
        "<img src=':/help/markers/select-menu.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMarkerSelectMenuThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Nos dois casos, um popup de cores aparece acima da seleção: oito cores prontas, mais "
        "um botão pra escolher qualquer cor customizada. No modo \"com comentário\" o popup "
        "também mostra a caixinha de texto pro comentário."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/markers/color-picker.png' style='text-decoration:none;'>"
        "<img src=':/help/markers/color-picker.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "&nbsp;&nbsp;"
        "<a href='zoom:/help/markers/comment-picker.png' style='text-decoration:none;'>"
        "<img src=':/help/markers/comment-picker.png' width='%4'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMarkerColorPickerThumbWidth), Theme::textMuted(),
          tr("Clique para expandir"), QString::number(kMarkerCommentPickerThumbWidth));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Passando o mouse sobre um trecho marcado que tem comentário, um balão aparece "
        "mostrando o texto da nota, com botões pra editar (cor e/ou comentário) ou excluir o "
        "marcador."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/markers/comment-hover.png' style='text-decoration:none;'>"
        "<img src=':/help/markers/comment-hover.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMarkerCommentHoverThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Marcadores sem comentário não têm esse balão — pra mudar a cor de um deles (ou pra "
        "removê-lo de vez), é só selecionar o trecho de novo: o popup de cores reabre, com um "
        "botão de lixeira ao lado de confirmar/cancelar."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/markers/plain-marker.png' style='text-decoration:none;'>"
        "<img src=':/help/markers/plain-marker.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMarkerPlainThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Todos os seus marcadores comentados ficam reunidos numa lista só, dentro do "
        "Pensário, na aba \"Comentários\" — dá pra ordenar por data de criação ou agrupados "
        "na ordem do próprio manuscrito (por capítulo/cena), e clicar num deles leva direto "
        "pro trecho de origem. Mas o Pensário é um assunto diferente, falaremos dele quando "
        "chegar a hora."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Na hora de exportar, você escolhe se os marcadores aparecem no arquivo final ou se "
        "são removidos — essa opção fica na própria janela de Exportação (já falamos dela lá "
        "atrás)."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/memories/, montado aqui em HTML.
QString HelpPanel::memoriesContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Memórias são trechos de texto que você guarda de lado pra consultar depois — uma "
        "fala marcante, uma descrição que você quer reaproveitar, um detalhe de lore que "
        "surgiu no meio de uma cena e você não quer perder. Diferente do Marcador (que fica "
        "grudado no texto original), a Memória vira um cartão avulso, guardado à parte."));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Pra criar uma, selecione o trecho no editor e escolha \"Adicionar à memória...\" no "
        "menu flutuante de seleção."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/memories/creation-menu.png' style='text-decoration:none;'>"
        "<img src=':/help/memories/creation-menu.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMemoryCreationMenuThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr("Um popup abre com:"));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Destino."),
             tr("Guardar como memória do Projeto (geral) ou de um Personagem específico — "
                "nesse caso, você escolhe qual, numa lista dos personagens já cadastrados no "
                "projeto."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Nome (opcional)."),
             tr("Dê um título pra memória, se quiser. Se deixar em branco, ela aparece "
                "identificada pela fonte de onde veio (ex: \"Memória do Capítulo 3\")."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>3- %1</b><br>%2</p>")
        .arg(tr("Tags."),
             tr("Marque uma ou mais etiquetas livres pra organizar suas memórias — o popup "
                "sugere as tags que você já usou antes no projeto, pra manter consistência."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/memories/creation-dialog.png' style='text-decoration:none;'>"
        "<img src=':/help/memories/creation-dialog.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMemoryCreationDialogThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "A memória guarda também, automaticamente, de onde ela veio (o capítulo, a cena ou o "
        "documento de gaveta), então você sempre sabe o contexto original."));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Pra consultar, abra o Pensário e vá na aba \"Memórias\": lá você filtra entre "
        "memórias do Projeto ou de cada Personagem, e também por tag. Clicar num cartão leva "
        "você até o trecho de origem no editor; o \"×\" no canto do cartão exclui a memória."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/memories/pensario-tab.png' style='text-decoration:none;'>"
        "<img src=':/help/memories/pensario-tab.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMemoryPensarioTabThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/timeline/, montado aqui em HTML.
QString HelpPanel::timelineContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "A Timeline é a função responsável por organizar a linha do tempo da história. Ela gera "
        "ramificações, conexões, backstories e muito mais conforme você escreve o seu projeto."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Porém, apesar do conceito brilhante, sou sincero: a Timeline pode ser confusa de se "
        "entender caso você não saiba o que está fazendo. Então, para que tudo flua de maneira "
        "boa e você não fique perdido, irei te ensinar a navegar nas águas traiçoeiras do tempo."));
    html += QStringLiteral("<p>%1</p>").arg(tr("Vem comigo."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra abrir, clique no botão de Timeline na barra lateral (ou use o atalho F3)."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/timeline-panel.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/timeline-panel.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelinePanelThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p>%1</p>").arg(tr("Vamos começar."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>1- %1</b></p>")
        .arg(tr("A data-base da sua história."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Quando você cria um manuscrito (ou edita um já existente), tem um campo opcional "
        "\"Quando a história se passa\". Esse é o marco zero da sua Timeline — a partir dele, o "
        "app sabe distinguir o que é \"a história principal\" do que é \"flashback\"."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/manuscript-data.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/manuscript-data.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelineManuscriptThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "O marco zero da história é importante. Tenha uma data boa em mente. Usando um marcador "
        "não convencional — como uma medição temporal fictícia, textos com números (ex: verão, "
        "1988) e etc. — ela ainda deverá funcionar caso o marcador usado tenha lógica e o "
        "sistema consiga compreender o que se passa antes ou depois do marco zero, mas fica mais "
        "propenso a erros."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Como dev, eu realmente recomendo que seja utilizado um formato de data convencional. "
        "Como Dia/Mês/Ano, Mês/Dia/Ano, Dia/Mês, Mês/Ano, e etc. O sistema funciona de forma "
        "MUITO mais precisa desse modo."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>2- %1</b></p>")
        .arg(tr("Marcador e resumo do capítulo (e da cena)."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Ao criar ou editar um capítulo, você já viu os campos \"Quando se passa\" e \"Resumo\" "
        "— pois é, eles não são só enfeite. Preenchendo os dois, o app cria automaticamente um "
        "evento na Timeline pra aquele capítulo, usando o resumo como descrição do evento. Se um "
        "capítulo tiver cenas separadas (lembra do \"----\"?), cada cena pode ter seu próprio "
        "marcador e resumo — se você não preencher o da cena, ela simplesmente herda o do "
        "capítulo."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/chapter-scene-data.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/chapter-scene-data.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelineChapterSceneThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>3- %1</b></p>")
        .arg(tr("História × Flashback: a organização automática."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Aqui está o pulo do gato. Comparando o marcador de cada capítulo/cena com a data-base "
        "do manuscrito, o app decide sozinho onde aquele evento entra:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Se a data é igual ou depois da data-base → vai pra trilha \"Narrativa\" (a história "
        "andando pra frente)."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Se a data é antes da data-base → vai automaticamente pra trilha \"Flashback\" (mostrada "
        "com um trilho tracejado)."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Você não precisa arrastar nada nem escolher trilha manualmente — só escrever a data "
        "certa no capítulo/cena."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>4- %1</b></p>")
        .arg(tr("Ramificações automáticas: quando a história se divide sozinha."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Dentro da trilha \"Narrativa\", o app também percebe sozinho quando a sua história "
        "deixou de ser uma linha só. Três coisas disparam uma ramificação nova:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "— Regressão cronológica: um capítulo/cena com marcador anterior ao do capítulo/cena "
        "que veio antes na leitura, sem virar Flashback (porque ainda está depois da data-base "
        "do manuscrito)."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "— Elenco totalmente diferente: mesmo período de tempo, mas nenhum personagem em comum "
        "com o que estava presente até então — sinal de que você pulou pra outro grupo de "
        "personagens."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "— POV marcado manualmente: ao editar um capítulo ou cena, um checkbox \"Não é do "
        "narrador / é de outro POV\" aparece se a sua obra já tem um narrador definido nas "
        "gavetas de personagem — marque-o pra forçar a ramificação mesmo sem os outros dois "
        "sinais."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Uma anomalia isolada não é o bastante — o app espera confirmar o padrão (duas anomalias "
        "parecidas seguidas) antes de criar a ramificação de verdade, pra não reagir a um único "
        "capítulo fora de ordem. Quando as duas ramificações se reencontram (elenco em comum de "
        "novo), o app desenha uma conexão entre elas — nunca funde as linhas. Casos "
        "genuinamente ambíguos (empate entre continuar na linha atual ou retomar outra) abrem um "
        "popup discreto perguntando qual ramificação é a certa; sua escolha fica salva e não "
        "pergunta de novo pro mesmo evento."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Cada ramificação nova ganha uma cor sorteada automaticamente, só pra ficar fácil "
        "distinguir uma da outra de relance."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "E agora, vamos explorar o painel da Timeline de fato."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>5- %1</b></p>")
        .arg(tr("Alternando entre os eixos."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Um botão na barra superior do painel alterna entre o eixo \"Narrativa\" e o eixo "
        "\"História\" (aparece como \"Backstory\" na tradução em inglês) — cada evento só "
        "aparece num dos dois, nunca nos dois ao mesmo tempo, então essa alternância é como você "
        "navega entre \"o que acontece na história\" e \"o que aconteceu antes\"."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>6- %1</b></p>")
        .arg(tr("Quem está na cena."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Se você já usa a detecção de personagens, os eventos da Timeline mostram "
        "automaticamente \"Presentes: Fulano, Beltrana\" — quem aparece naquele capítulo/cena. E "
        "tem um filtro \"Personagem: Todos ▾\" na barra do topo que deixa só os eventos daquele "
        "personagem em destaque, esmaecendo o resto."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/characters-narrative.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/characters-narrative.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelineCharactersThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>7- %1</b></p>")
        .arg(tr("Criando e editando eventos e linhas do tempo manualmente."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Nem tudo precisa vir do capítulo. Clicando no \"+\" flutuante no canto do painel (ou "
        "selecionando um trecho de texto no editor e usando o menu de seleção), você cria um "
        "evento manual."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Você também pode criar linhas do tempo próprias, definir a importância delas e "
        "nomeá-las."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Linhas do tempo próprias não são alimentadas de forma automática como as linhas de "
        "narrativa e backstory, mas são boas para criação de lore e worldbuilding."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Pra apagar uma linha que você criou, clique com o botão direito na faixa dela (Modo "
        "Trilho) e escolha \"Excluir linha...\". Linhas automáticas (Narrativa, Flashback, "
        "trilhas de personagem e as ramificações do item 4) não aparecem pra exclusão por ali — "
        "elas voltam sozinhas no próximo sync, então excluir não resolveria nada."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/event-creator.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/event-creator.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "&nbsp;&nbsp;"
        "<a href='zoom:/help/timeline/timeline-creator.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/timeline-creator.png' width='%4'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelineEventCreatorThumbWidth), Theme::textMuted(),
          tr("Clique para expandir"), QString::number(kTimelineCreatorThumbWidth));

    html += QStringLiteral("<p style='margin-bottom:4px;margin-top:12px;'><b>8- %1</b></p>")
        .arg(tr("Formas de visualizar."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Um botão alterna entre três modos: Trilho (linhas horizontais organizadas por faixa), "
        "Ramificações (constelação livre) e Espiral. É só estética/organização — os mesmos "
        "eventos aparecem nos três, só a disposição muda."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/mode-track.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/mode-track.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
        "<p align='center'>"
        "<a href='zoom:/help/timeline/mode-branches.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/mode-branches.png' width='%4'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
        "<p align='center'>"
        "<a href='zoom:/help/timeline/mode-spiral.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/mode-spiral.png' width='%5'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelineModeTrackThumbWidth), Theme::textMuted(), tr("Clique para expandir"),
          QString::number(kTimelineModeBranchesThumbWidth), QString::number(kTimelineModeSpiralThumbWidth));

    html += QStringLiteral("<p style='margin-bottom:4px;margin-top:12px;'><b>9- %1</b></p>")
        .arg(tr("Foco."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Clicando em um evento (ou usando o menu de Foco), o resto do painel esmaece, "
        "destacando só aquela linha e o que está conectado a ela — útil quando o projeto cresce "
        "e a tela fica cheia de eventos."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "E sim, quanto mais a sua timeline cresce, especialmente se você usar linhas e eventos "
        "manuais que se conectam, isso fará toda a diferença, observe:"));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/line-focus.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/line-focus.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelineLineFocusThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;margin-top:12px;'><b>10- %1</b></p>")
        .arg(tr("Preencher: colocando projetos antigos em dia."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Se o seu projeto é de antes da Timeline orgânica existir, provavelmente boa parte dos "
        "seus capítulos e cenas não tem marcador nem resumo preenchido, e sem isso a bolinha "
        "fica vazada. Em vez de abrir capítulo por capítulo, clique em \"capítulos sem data · "
        "Preencher\" no rodapé da Timeline, ou em ⋯ → Preencher datas e resumos."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "O painel abre na direita, um capítulo por vez, mostrando como ele começa pra você "
        "lembrar quando acontece. Embaixo do marcador aparece o que o Qenna entendeu "
        "(\"→ dia 9\", \"→ 20 anos antes · Flashback\"); em laranja, ele não reconheceu o "
        "marcador, e o capítulo segue a ordem de leitura. Cada campo grava sozinho quando você "
        "sai dele, e \"Salvar e próximo\" (Ctrl+Enter) anda a fila."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Quando a primeira frase do capítulo já diz o tempo (\"cinco dias depois\", \"naquela "
        "mesma noite\"), aparece uma sugestão pronta; um clique aceita. \"igual\" copia o marcador "
        "do capítulo anterior (Ctrl+D) e \"+1 dia\" soma um dia a ele (Alt+↓). Capítulo com cenas "
        "é preenchido inteiro, e as cenas herdam; \"separar cenas\" dá um campo pra cada uma."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Por último: se você preferir o jeito antigo de acompanhar personagem por personagem "
        "(uma trilha dedicada por personagem, em vez do filtro), tem um botão \"Personagens "
        "(legado)\" que liga esse sistema de volta. Ele vem desligado por padrão porque o filtro "
        "+ \"Presentes:\" cobre o mesmo uso de um jeito mais simples, mas a opção continua lá se "
        "você preferir."));

    html += QStringLiteral("<p style='font-style:italic;color:%1;'><b>%2</b> %3</p>")
        .arg(Theme::textMuted(), tr("Nota do dev:"), tr(
            "A Timeline talvez seja a função que pareça mais complexa dentro do app. É UMA MINA "
            "DE OURO, mas tem uma leve curva de aprendizado para alcançar seu potencial máximo. "
            "Eu pretendo tentar simplificá-la um pouco em versões futuras, mas acredite, vale a "
            "pena tentar entendê-la. Especialmente se sua obra exige um bom planejamento "
            "cronológico e consistente."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/cover-creator/, montado aqui em HTML.
QString HelpPanel::coverCreatorContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Qenna Writer vem com uma ferramenta separada pra criar a capa do seu livro — o Mira "
        "Cover. Ela abre numa janela própria, fora do editor."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra acessar, vá até a tela inicial (o Menu Principal, com a lista dos seus projetos) e "
        "clique com o botão direito no card do livro que você quer dar uma capa. A opção \"Criar "
        "capa\" aparece no menu de contexto."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/cover-creator/context-menu.png' style='text-decoration:none;'>"
        "<img src=':/help/cover-creator/context-menu.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kCoverContextMenuThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Se for a primeira vez que você usa essa função, o app vai perguntar se quer instalar o "
        "Mira Cover — é rapidinho, e só precisa fazer isso uma vez."));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Dentro do Mira Cover, você monta a capa do zero:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Título e autor como textos editáveis, com controle de fonte (mais de 100 opções, "
        "agrupadas por estilo — literário, fantasia, terror, ficção científica, etc.), tamanho, "
        "cor, sombra, contorno, brilho e rotação."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Imagem de fundo: você pode subir uma imagem do seu computador ou escolher de uma "
        "galeria com dezenas de fotos já disponíveis no próprio app, com ajuste de zoom, foco e "
        "filtro."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Cor de fundo sólida, se preferir não usar imagem."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Camadas extras: mais textos, símbolos, e formas geométricas (círculo, linha, triângulo, "
        "retângulo)."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Borda ao redor da capa, com cor e espessura ajustáveis."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/cover-creator/app-editing.png' style='text-decoration:none;'>"
        "<img src=':/help/cover-creator/app-editing.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kCoverAppEditingThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Se você já tinha editado uma capa desse projeto antes, o Mira Cover lembra o que você "
        "fez da última vez e abre de onde parou."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra salvar, clique em \"OK\" (ou o botão equivalente de confirmar) — isso grava a capa "
        "na pasta do projeto e fecha o Mira Cover; ao voltar pro Qenna Writer, a capa já aparece "
        "atualizada no card do projeto. Também tem um botão de exportar (ícone de download) que "
        "baixa a imagem da capa avulsa, sem mexer no projeto — útil se você quiser usar essa "
        "capa em outro lugar."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Cover Creator conta com uma galeria de imagens sem copyright que podem ser usadas "
        "para capas do seu projeto em uso profissional."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Ele tem várias opções: ajustes de imagem, adicionar textos, símbolos, efeitos e muito "
        "mais."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Cover Creator não é muito difícil de se usar, caso já tenha costume com editores "
        "básicos de imagem, você vai se sentir em casa. Se não tiver, explore. Não vai se "
        "arrepender."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/create-documents-from-text/,
// montado aqui em HTML.
QString HelpPanel::createDocumentsContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Às vezes você escreve algo no meio de um capítulo — uma descrição, uma fala "
        "marcante, um trecho de worldbuilding solto — e percebe depois que aquilo merece "
        "virar um documento próprio numa gaveta, em vez de ficar perdido no meio do texto. "
        "Pra isso, você não precisa copiar, colar e reescrever nada na mão: o app faz isso "
        "por você."));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Selecione o trecho que quer transformar (pode ser uma frase, um parágrafo, ou "
        "vários parágrafos — com ou sem marcador/comentário aplicado neles, tanto faz) e "
        "escolha \"Criar documento disso...\" no menu flutuante de seleção."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/create-documents/select-menu.png' style='text-decoration:none;'>"
        "<img src=':/help/create-documents/select-menu.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kCreateDocSelectMenuThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "É necessário ter pelo menos uma gaveta criada no projeto — se não tiver nenhuma, o "
        "app avisa e cancela a ação."));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr("Uma janela abre com:"));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Nome do documento."),
             tr("Já vem sugerido a partir das primeiras palavras do trecho selecionado (até "
                "8 palavras ou ~48 caracteres), mas você pode mudar livremente."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Gaveta de destino."),
             tr("Escolha em qual gaveta o novo documento vai entrar."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/create-documents/creation-dialog.png' style='text-decoration:none;'>"
        "<img src=':/help/create-documents/creation-dialog.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kCreateDocDialogThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Se a gaveta escolhida for uma gaveta de elemento (Personagens, Cenários ou "
        "Objetos), ao confirmar o app abre em seguida o cadastro do elemento (foto, "
        "apelido/papel, conforme o tipo) antes de finalizar — o documento nasce já com a "
        "ficha certa."));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "O texto selecionado vira o conteúdo do documento novo, com os parágrafos "
        "preservados (linhas separadas por quebra dupla viram parágrafos separados). O "
        "trecho original continua no capítulo de onde veio — criar o documento não remove "
        "nem corta nada do texto-fonte, só copia."));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Em resumo, essa é uma função inestimável para:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Criar personagens, cenários ou outros elementos diretamente do seu texto, sem sair "
        "do seu fluxo."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Gerar documentos de trechos ou passagens importantes da história."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/themes/, montado aqui em HTML.
QString HelpPanel::themesContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Qenna Writer vem com mais de 140 temas prontos, além de deixar você criar os seus "
        "próprios do zero. Há várias opções de customização para o app e você pode deixá-lo "
        "com a aparência que quiser."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra acessar, clique no botão de Temas na barra de ferramentas ou vá em Configurações → Aparência → Tema."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/themes/theme-panel.png' style='text-decoration:none;'>"
        "<img src=':/help/themes/theme-panel.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kThemePanelThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>1- %1</b></p>").arg(tr("Busca e filtro."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Um campo de busca filtra os temas pelo nome em tempo real. Um menu de categorias ao "
        "lado deixa você navegar por grupos."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr("Os grupos principais são:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>")
        .arg(tr("Claros — focados em tons brancos ou próximos de branco."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>")
        .arg(tr("Escuros — focados em tons escurecidos, próximos de cinza e preto."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>")
        .arg(tr("Amarelados — tons amarelados, amarronzados e quentes."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>")
        .arg(tr("Coloridos — temas de cores destacadas e fortes."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>")
        .arg(tr("Estampados — temas com imagens de fundo."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>2- %1</b></p>").arg(tr(
        "Também há a opção \"♥ Favoritos\". Nela, você pode deixar salvos os temas que "
        "gosta mais."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Para adicionar um tema aos favoritos, basta clicar no coração que fica no canto de "
        "seu card."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/themes/favorite.png' style='text-decoration:none;'>"
        "<img src=':/help/themes/favorite.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kThemeFavoriteThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>3- %1</b><br>%2</p>")
        .arg(tr("Selecionar e aplicar."),
             tr("Clique num card pra selecioná-lo, depois clique em \"Aplicar\" pra usar esse "
                "tema no app."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>4- %1</b></p>").arg(tr("Criar um tema seu."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Selecione qualquer tema pronto como base e clique em \"Duplicar\":"));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/themes/duplicate.png' style='text-decoration:none;'>"
        "<img src=':/help/themes/duplicate.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kThemeDuplicateThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Uma vez feito isso, será aberta uma janela para que você edite o tema selecionado e "
        "crie o seu próprio partindo dele."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/themes/theme-editor.png' style='text-decoration:none;'>"
        "<img src=':/help/themes/theme-editor.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kThemeEditorThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "No Editor de Tema, você pode ajustar:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr("Nome do tema."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Cores principais: cor do texto do editor, fundo da página, texto da UI, texto "
        "secundário, cor de destaque."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Fundo da janela: cor do app, cor dos painéis, borda dos painéis — e, se quiser, uma "
        "imagem de fundo (com modo de exibição: Centralizar, Repetir, Esticar, Ajustar ou "
        "Preencher)."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Página de texto: opacidade, e sombra projetada (ativar/desativar, cor, raio e "
        "deslocamento)."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Uma pré-visualização ao lado mostra o resultado em tempo real enquanto você mexe. "
        "No final, \"Salvar\" grava seu tema personalizado na lista (ele aparece separado, "
        "com opção de editar de novo depois) ou \"Cancelar\" descarta."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>5- %1</b></p>").arg(tr("Troca automática por horário."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Tem uma opção \"Troca automática por horário\" que alterna sozinho entre um tema "
        "diurno e um noturno, nos horários que você configurar. Pra usar, selecione um tema "
        "e marque se ele é o tema do \"Dia\" ou da \"Noite\", depois defina os horários de "
        "troca. Se você aplicar um tema manualmente enquanto essa troca automática estiver "
        "ligada, ela se desliga sozinha — assim o app não fica sobrescrevendo uma escolha "
        "consciente sua."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/themes/time-change.png' style='text-decoration:none;'>"
        "<img src=':/help/themes/time-change.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kThemeTimeChangeThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    return html;
}

// Criador de Mundos (a Enciclopédia): espelho em help-panel/builder/.
QString HelpPanel::builderContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "As gavetas documentam o seu mundo: personagens, cenários, lore solta. O Criador de "
        "Mundos é onde esse mundo vira uma enciclopédia: os lugares da história e os sistemas "
        "que mandam neles (a magia, a política, a religião), cada um como um verbete que você "
        "lê e escreve como texto corrido."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra abrir, clique no botão Construtor na barra superior do editor. A janela abre no "
        "modo que você usou por último."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Lugares e Sistemas."),
             tr("No alto da janela, a chave Lugares | Sistemas troca o que aparece na lista da "
                "esquerda. Ao lado, a busca encontra lugares, sistemas, regras e documentos de uma "
                "vez só: clicar num resultado abre o verbete ali mesmo."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/construtor/sistema.png' style='text-decoration:none;'>"
        "<img src=':/help/construtor/sistema.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kBuilderSystemThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Criando um sistema."),
             tr("Clique em \"+ Novo sistema\" no pé da lista, dê um nome e escolha a categoria "
                "(Magia, Política, Religião, Social, Econômico, Militar, Tecnologia, Cosmologia, "
                "Organização/Facção, Linhagem, Mitologia ou Outro). Cada sistema pertence a uma "
                "categoria só."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>3- %1</b><br>%2</p>")
        .arg(tr("O verbete."),
             tr("No centro, o sistema se lê como um texto: o título, um resumo e, embaixo, as regras "
                "e as seções. Regras são as leis do sistema e ganham número (Art. 1, e as regras de "
                "dentro dela viram 1.1, 1.2); seções (§) guardam informação solta, mais perto de "
                "texto corrido. Use \"+ Regra\" e \"+ Seção\" no fim da página, e o ⋯ ao lado de cada "
                "título pra criar uma regra ou seção dentro dela, ou pra excluir. Cada trecho salva "
                "sozinho enquanto você escreve, e a barra no topo formata o trecho em que o cursor "
                "está (e liga o Modo foco)."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>4- %1</b><br>%2</p>")
        .arg(tr("O espectro."),
             tr("Na margem direita, a barra do Espectro mostra onde o sistema fica na régua da "
                "categoria: em Magia, de Soft a Hard; em Política, de Anarquia a Totalitarismo. "
                "Clique num ponto da barra pra mudar. Embaixo aparecem o que aquele ponto FAVORECE e "
                "o que ele EXIGE da sua história, pra você escolher sabendo das consequências e não "
                "só pelo nome bonito. \"Ver todos\" abre a lista inteira."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>5- %1</b><br>%2</p>")
        .arg(tr("Onde o sistema vale."),
             tr("Um sistema novo é global: vale no mundo inteiro. \"Escolher territórios\", em Vale "
                "em, prende ele aos lugares que você marcar."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>6- %1</b><br>%2</p>")
        .arg(tr("No livro."),
             tr("Selecione um trecho no editor e use \"Salvar como menção ao sistema...\" no menu de "
                "seleção; dá pra prender a menção a uma regra ou seção específica. Ela aparece na "
                "margem, em No livro, com o capítulo e a cena de onde veio. Clicar nela leva de volta "
                "ao trecho."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/construtor/territorio.png' style='text-decoration:none;'>"
        "<img src=':/help/construtor/territorio.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kBuilderTerritoryThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>7- %1</b><br>%2</p>")
        .arg(tr("Lugares."),
             tr("Em Lugares, cada território é um verbete com a lore dele e, embaixo, pastas e "
                "documentos (\"+ Pasta\", \"+ Documento\") pra ruas, prédios, história, o que "
                "precisar. A margem direita vira uma ficha: a imagem do lugar (clique nela pra "
                "trocar), os Vizinhos, a Gente daqui (personagens cuja ficha diz que nasceram ou "
                "moram ali), os sistemas que valem naquele lugar e os eventos da Timeline marcados "
                "nele. Menções funcionam igual aos sistemas, com \"Salvar como menção ao "
                "Território...\"."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>8- %1</b><br>%2</p>")
        .arg(tr("Vizinhos."),
             tr("\"Vincular a…\" liga um território a outro. Cada vínculo tem uma página própria: "
                "\"Escrever o vínculo\" abre ela pra você contar como os dois lugares se relacionam "
                "(a estrada, a guerra, o rio no meio). Depois de escrito, o botão vira \"Ler o "
                "vínculo\". O botão direito num território da lista também tem Trocar imagem…, "
                "Vincular a… e Excluir território."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>9- %1</b><br>%2</p>")
        .arg(tr("Referenciando de qualquer lugar."),
             tr("Digitando @ no meio do texto, você pode navegar até \"Construtor\", escolher um "
                "sistema e uma regra dele pra mencionar; Ctrl+clique na menção abre o Criador de "
                "Mundos direto ali. O Menu de Referência também mostra territórios e sistemas "
                "enquanto você escreve."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/pensarium/, montado aqui em HTML.
QString HelpPanel::pensarioContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Pensário é um painel flutuante que reúne várias ferramentas de apoio à escrita num "
        "lugar só — pense nele como uma central de anotações e descobertas sobre o seu projeto. "
        "Pra abrir, use o atalho F4 (o mesmo fecha)."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/pensario/panel.png' style='text-decoration:none;'>"
        "<img src=':/help/pensario/panel.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kPensarioPanelThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p>%1</p>").arg(tr("Ele é dividido em abas:"));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Comentários e Memórias."),
             tr("Essas duas já têm seção própria aqui no Help Panel — Comentários reúne os "
                "marcadores comentados do projeto inteiro, Memórias guarda os trechos que você "
                "salvou de lado. Dá uma olhada nas seções \"Marcadores e Comentários\" e "
                "\"Memórias\" se ainda não viu."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>2- %1</b></p>").arg(tr("Notas."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Notas são lembretes soltos, sem vínculo com nenhum trecho do texto — diferente de "
        "Memórias e Comentários, que sempre vêm de algum lugar do seu manuscrito. Clique em "
        "\"+ Nova nota\" pra criar uma, com cor e título opcionais."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/pensario/create-note-1.png' style='text-decoration:none;'>"
        "<img src=':/help/pensario/create-note-1.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kPensarioCreateNote1ThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/pensario/create-note-2.png' style='text-decoration:none;'>"
        "<img src=':/help/pensario/create-note-2.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kPensarioCreateNote2ThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>3- %1</b></p>").arg(tr("Diálogos."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Toda fala que você escreve com travessão, hífen, aspas ou « » é detectada alguns "
        "segundos depois que você para de digitar. O detector lê a cena como uma conversa: "
        "quando a tag diz o nome (\"— Não. — Klara disse.\"), a fala é daquele personagem; "
        "quando não diz, ele deduz por quem falou antes e por quem foi chamado pelo nome, e "
        "marca a fala como \"provável\". Essa aba lista tudo, com um filtro \"Fala: Todos ▾\" "
        "e chips que deixam filtrar por quem mais fala na mesma cena."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Fala provável pode estar errada, principalmente em cena com três pessoas conversando. "
        "Clique direito no card pra confirmar ou trocar o locutor: o que você corrige nunca é "
        "desfeito. Fala de quem não está no elenco (\"o porteiro\") fica em Figurantes; "
        "atribuir uma delas a um personagem pode transformar a tag em apelido dele. E o × tira "
        "uma linha que não é fala, sem ela voltar depois."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/pensario/dialogue.png' style='text-decoration:none;'>"
        "<img src=':/help/pensario/dialogue.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kPensarioDialogueThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>4- %1</b></p>").arg(tr("Nomes (o ✦ no canto)."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Um gerador de nomes pra quando a inspiração não vem: escolha a categoria (Personagens, "
        "Lugares ou Armas), um estilo (varia por categoria — inclui desde nomes reais, com "
        "opção de gênero, até estilos inventados), e clique em \"Gerar\". Pra estilos gerados "
        "(não os de nomes reais), dá pra filtrar o resultado por \"Começa com...\" e \"Termina "
        "com...\". Clicar num nome da lista copia ele pra área de transferência."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/pensario/name-generator.png' style='text-decoration:none;'>"
        "<img src=':/help/pensario/name-generator.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kPensarioNameGeneratorThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>5- %1</b><br>%2</p>")
        .arg(tr("Glossário."),
             tr("A aba Glossário guarda os termos do seu mundo: siglas, facções, gírias. Ela tem um "
                "tópico só dela aqui na Ajuda."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/additional-resources/bonds/, montado aqui em HTML.
QString HelpPanel::bondsContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Dentro de uma gaveta de Personagens, passe o mouse sobre um card: um botãozinho "
        "aparece no canto superior direito dele."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/vinculos/bond-option.png' style='text-decoration:none;'>"
        "<img src=':/help/vinculos/bond-option.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kBondOptionThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Arraste esse botão até outro personagem e solte. Se ainda não existir um vínculo "
        "entre os dois, abre a criação:"));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/vinculos/bond-creator.png' style='text-decoration:none;'>"
        "<img src=':/help/vinculos/bond-creator.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kBondCreatorThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Escolha um tipo (a lista já vem com várias opções prontas, organizadas por categoria "
        "— Família, Romântico, Social, Conflito e Poder — com alternância entre versão "
        "masculina/feminina, mas você também pode digitar um tipo personalizado), escreva uma "
        "descrição/histórico se quiser, e escolha a cor da linha. Note que essa opção só fica "
        "disponível quando a gaveta está exibindo os cards numa grade de até 2 colunas (grades "
        "mais densas não desenham vínculos)."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Feito isso, uma linha conectando os dois cards aparece na gaveta. Passe o mouse "
        "sobre a linha pra ver o tipo do vínculo, ou clique nela pra abrir a visão de leitura:"));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/vinculos/bond-display.png' style='text-decoration:none;'>"
        "<img src=':/help/vinculos/bond-display.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kBondDisplayThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Nessa visão, você pode editar o vínculo (lápis), criar um documento a partir dele — "
        "já sugerindo o nome \"Fulano ↔ Beltrana\" e pedindo a gaveta de destino —, excluir ou "
        "fechar. Excluir um personagem remove automaticamente todos os vínculos que ele tinha."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/additional-resources/map/, montado aqui em HTML.
QString HelpPanel::worldMapContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra acessar, abra o Pensário (F4) e clique no ícone de mapa no cabeçalho dele. Ele "
        "abre num painel próprio, flutuante e redimensionável."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/mapa-mundi/panel.png' style='text-decoration:none;'>"
        "<img src=':/help/mapa-mundi/panel.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMapPanelThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Você pode navegar por \"Ir para local\", escolhendo País, Estado e Cidade em "
        "sequência, ou buscar direto pelo nome (a busca sugere países, estados e cidades "
        "conforme você digita). Clicar em qualquer lugar do mapa mostra um card com "
        "informações dele (capital e população, no caso de países; população, no caso de "
        "cidades). Também há uma régua pra medir distância entre dois pontos, e botões pra "
        "alternar entre mapa simples ou texturizado, e entre projeção plana ou globo 3D."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/mapa-mundi/no-texture.png' style='text-decoration:none;'>"
        "<img src=':/help/mapa-mundi/no-texture.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMapNoTextureThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra fixar um marcador de referência (pin) num local, use o botão de fixar pin na "
        "barra de navegação e clique no mapa. O popup do pin pede um nome, uma nota opcional, "
        "e permite vincular esse pin a qualquer elemento das suas gavetas (um personagem, por "
        "exemplo) — ou deixar sem vínculo. Clicar num pin já existente reabre esse popup pra "
        "editar."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/mapa-mundi/pin.png' style='text-decoration:none;'>"
        "<img src=':/help/mapa-mundi/pin.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMapPinThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    return html;
}

// Glossário (aba do Pensário): espelho em help-panel/additional-resources/glossary/.
QString HelpPanel::glossaryContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Glossário guarda as palavras que só existem no seu livro: siglas, facções, "
        "lugares, títulos, gírias. Ele mora numa aba própria do Pensário (F4 pra abrir), e "
        "alimenta o corretor ortográfico, a Mira e a Bíblia do universo."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Adicionando pelo texto."),
             tr("Selecione a palavra no editor e use \"Adicionar ao Glossário\" no menu de seleção (o "
                "mesmo item aparece no botão direito de uma palavra que o corretor sublinhou). O "
                "popup já mostra a frase em que o termo aparece pela primeira vez no livro, quantas "
                "vezes ele aparece e em quantos capítulos; \"Ir até a primeira vez\" leva direto pra "
                "lá."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/glossario/adicionar.png' style='text-decoration:none;'>"
        "<img src=':/help/glossario/adicionar.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kGlossaryAddThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Escolha um tipo (sigla, grupo, lugar, título, objeto ou gíria) e escreva a "
        "definição. Se o termo tem outras formas no texto, liste em \"Também escrito como\", "
        "separadas por vírgula: \"CAL\" e \"Comando Alto Leste\" viram um termo só, e as "
        "contagens somam. Tudo além do termo é opcional. Selecionando uma palavra que já está "
        "no glossário (ou uma das outras grafias dela), o popup abre direto na edição."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("A aba Glossário."),
             tr("Os termos ficam em ordem alfabética, com o alfabeto na borda direita pra pular de "
                "letra. Cada verbete mostra o tipo, as outras grafias, a definição e a primeira vez "
                "que o termo aparece. Clique num termo pra editar; o botão direito tem Editar, Ir até "
                "a primeira vez e Remover termo. A busca no topo procura nos termos, nas outras "
                "grafias e nas definições, e \"+ Novo termo\" cria um do zero."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/glossario/aba.png' style='text-decoration:none;'>"
        "<img src=':/help/glossario/aba.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kGlossaryTabThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>3- %1</b><br>%2</p>")
        .arg(tr("Termo no texto."),
             tr("No editor, os termos do glossário ganham um sublinhado pontilhado discreto. Pare o "
                "mouse em cima pra ver a ficha: tipo, definição, a primeira vez e quantas vezes "
                "aparece. \"1ª vez\" leva ao primeiro uso e \"Glossário\" abre o verbete na aba. Se "
                "preferir o texto limpo, desligue em Configurações › Corretor › Termos do "
                "glossário no texto."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/glossario/no-texto.png' style='text-decoration:none;'>"
        "<img src=':/help/glossario/no-texto.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kGlossaryInTextThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Uma sigla toda em maiúsculas só é reconhecida em maiúsculas: ROTA é o termo, e "
        "\"rota\" continua sendo palavra comum. E o corretor para de marcar como erro cada "
        "termo e cada grafia dele, então vale cadastrar os nomes inventados nem que seja só "
        "por isso."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/estatisticas/, montado aqui em HTML.
QString HelpPanel::statsContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O botão de Estatísticas fica na barra superior do editor, junto dos botões de "
        "Construtor, Pensário e Menu de Referência. Clique nele pra abrir o painel."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/estatisticas/topbar-button.png' style='text-decoration:none;'>"
        "<img src=':/help/estatisticas/topbar-button.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kStatsTopbarButtonThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "O painel tem duas partes: uma visão geral do projeto, e um mergulho fundo em cada "
        "personagem."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>1- %1</b></p>")
        .arg(tr("Visão geral."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Logo no topo, uma fileira com a foto de todos os personagens do projeto. Clicar em "
        "qualquer um deles abre a página individual dele (item 2 abaixo)."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Embaixo da fileira, um gráfico de barrinhas mostra a \"Participação\" de cada "
        "personagem — a porcentagem de cenas do projeto em que ele aparece (mesma detecção "
        "automática por nome que já existia)."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Mais embaixo, \"Manuscrito, por capítulo\" — um gráfico de barras com um capítulo por "
        "coluna, numerados na ordem da obra. Um menu no canto (\"Palavras ▾\") deixa escolher a "
        "métrica: palavras por capítulo, ou % de diálogo em relação à narração. Clicar numa "
        "barra abre as estatísticas daquele capítulo específico (a mesma janela que já existe "
        "no Manuscrito)."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Por fim, \"Resumo do projeto\": total de palavras, quantos capítulos e cenas tem o "
        "manuscrito atual, qual foi o maior e o menor capítulo (em palavras), quantos Vínculos "
        "existem por tipo, e um resumo da sua sequência de escrita (streak atual, recorde, "
        "páginas estimadas)."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/estatisticas/panel.png' style='text-decoration:none;'>"
        "<img src=':/help/estatisticas/panel.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kStatsPanelThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;margin-top:12px;'><b>2- %1</b></p>")
        .arg(tr("Por personagem."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Clicando numa foto na fileira, você entra na página daquele personagem. Um botão de "
        "voltar (←) no cabeçalho do painel te traz de volta pra visão geral."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Aqui você encontra: a foto e o nome, um resumo de presença (em quantas cenas/capítulos "
        "ele aparece), quantas falas e palavras faladas o Detector de Diálogos já achou pra ele, "
        "e dois botões — Status e Local — que fazem exatamente o que faziam no antigo Modo "
        "Consistência (que não existe mais — tudo que ele fazia foi pra cá). Se o personagem "
        "estiver marcado como Morto ou Desaparecido mas ainda aparecer em alguma cena depois "
        "disso, um aviso chama atenção pra essa inconsistência."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Logo abaixo, a lista de Vínculos daquele personagem (mesmos vínculos que você já cria "
        "arrastando um personagem em cima do outro na gaveta) — aqui é só consulta, criar/editar "
        "vínculo continua sendo na gaveta mesmo."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/estatisticas/char-panel.png' style='text-decoration:none;'>"
        "<img src=':/help/estatisticas/char-panel.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kStatsCharPanelThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;margin-top:12px;'><b>3- %1</b></p>")
        .arg(tr("Química."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Essa é nova: uma lista mostrando com quem aquele personagem mais \"contracenou\" — "
        "quantas cenas, capítulos e falas cruzadas ele tem com cada outro personagem do elenco. "
        "Um menu deixa escolher qual dessas três métricas ordena a lista."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Clicando num nome da lista, abre um popup só com os diálogos entre aquele par de "
        "personagens, com um menu no topo pra pular direto pra um capítulo específico. O popup "
        "pode ser arrastado pelo título, pra tirar ele do meio do caminho."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/estatisticas/chemistry-dialog.png' style='text-decoration:none;'>"
        "<img src=':/help/estatisticas/chemistry-dialog.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kStatsChemistryDialogThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;margin-top:12px;'><b>4- %1</b></p>")
        .arg(tr("Ficha / documento."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Por último, o conteúdo real da ficha (ou do documento livre, se o personagem não usa "
        "ficha estruturada) daquele personagem, exibido ali mesmo — sem precisar abrir a gaveta. "
        "Só o texto: fotos que estejam dentro da ficha/doc não aparecem aqui (a foto do "
        "personagem já está lá em cima). A área tem altura limitada e rola por dentro se o "
        "conteúdo for grande."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/additional-resources/immersive-sound/, montado aqui em HTML.
QString HelpPanel::ambienceContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Fica na barra superior, ao lado do botão de Lembretes."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/som-imersivo/panel.png' style='text-decoration:none;'>"
        "<img src=':/help/som-imersivo/panel.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kAmbiencePanelThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O app varre uma pasta de sons do seu computador e lista os arquivos de áudio "
        "encontrados nela. Só uma faixa toca por vez (escolher uma nova troca a que estava "
        "tocando), em loop contínuo, com um controle de volume único pra todas. Sua faixa e "
        "volume escolhidos ficam salvos e voltam a mesma coisa da próxima vez que você abrir o "
        "app."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/additional-resources/reminders/, montado aqui em HTML.
QString HelpPanel::remindersContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Fica na barra superior, ao lado do Som Imersivo. Um aviso vermelho aparece no botão "
        "quando você tem lembretes ativos."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/lembretes/panel.png' style='text-decoration:none;'>"
        "<img src=':/help/lembretes/panel.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kReminderPanelThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Digite o texto do lembrete e aperte Enter (ou o \"+\") pra criá-lo. Se marcar "
        "\"Notificar às\", escolha um horário do dia — ao chegar nesse horário, um aviso "
        "aparece dentro do próprio app (não é uma notificação do Windows). Lembretes não têm "
        "data específica (só horário), nem prioridade, nem vínculo com capítulos ou documentos "
        "— são só lembretes de texto livre mesmo."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/lembretes/notification.png' style='text-decoration:none;'>"
        "<img src=':/help/lembretes/notification.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kReminderNotificationThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Marque o quadradinho de um lembrete pra concluí-lo; ele vai pra uma lista de "
        "\"Concluídos\" que pode ser expandida ou escondida, com opção de limpar tudo de uma "
        "vez."));

    return html;
}

QString HelpPanel::sceneVariationContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Caso você tenha escrito uma cena, gostou dela, mas por algum motivo quer reescrevê-la, "
        "dá pra fazer isso sem perder a cena que já escreveu — e é mais simples do que parece."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "No painel de Manuscrito, acesse a sua cena diretamente — essa opção não é acessível em "
        "capítulos completos, só em cenas isoladas. Então você precisa abrir a cena sozinha no "
        "editor, não o capítulo inteiro."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/variacao-cenas/scene-on-manuscript.png' style='text-decoration:none;'>"
        "<img src=':/help/variacao-cenas/scene-on-manuscript.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kSceneVarManuscriptThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Quando a cena abrir sozinha, aparece um botão discreto ao lado do nome dela, na barra "
        "superior. Clique nele — sim, ele é bem pequeno e discreto mesmo, é de propósito."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/variacao-cenas/button.png' style='text-decoration:none;'>"
        "<img src=':/help/variacao-cenas/button.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kSceneVarButtonThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Ao clicar, aparecem três opções: nova, primária e apagar."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/variacao-cenas/options.png' style='text-decoration:none;'>"
        "<img src=':/help/variacao-cenas/options.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kSceneVarOptionsThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "<b>+ nova</b> cria outra variação da cena: um popup pede um nome pra ela e, ao "
        "confirmar, a variação já abre em branco no editor, pronta pra escrever."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "<b>★ primária</b> define qual variação é a principal — é ela que aparece quando o "
        "capítulo inteiro é aberto de uma vez e também a que sai na exportação."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "<b>✕ apagar</b> exclui a variação que está aberta no momento."));

    return html;
}

// Os passos que o conteúdo já escreve como "<b>1- Título.</b>" viram âncoras
// (step-1, step-2…) com o número discreto na frente; a lista dos títulos
// alimenta o "Neste tópico" e a busca. Em cima entram o grupo e o título.
QString HelpPanel::decorate(const QString& id, const QString& html, QStringList* steps) const
{
    static const QRegularExpression stepRe(QStringLiteral("<b>(\\d+)- ([^<]+)</b>"));
    QString out;
    int last = 0;
    auto it = stepRe.globalMatch(html);
    while (it.hasNext()) {
        const auto m = it.next();
        QString title = m.captured(2).trimmed();
        if (title.endsWith(QLatin1Char('.'))) title.chop(1);
        if (steps) steps->append(title);
        out += html.mid(last, m.capturedStart() - last);
        out += QStringLiteral("<a name=\"step-%1\"></a><span style=\"color:%2; font-weight:600;\">%1</span>&nbsp;&nbsp;"
                              "<span style=\"color:%3; font-size:15px; font-weight:600;\">%4</span>")
                   .arg(m.captured(1), Theme::textMuted(), Theme::textBright(), title);
        last = m.capturedEnd();
    }
    out += html.mid(last);

    const int g = groupOf(id);
    const QString head = QStringLiteral(
        "<p style=\"margin:0 0 4px 0; color:%1; font-size:10px; font-weight:bold; letter-spacing:1.4px;\">%2</p>"
        "<p style=\"margin:0 0 14px 0; color:%3; font-family:'Lora','Georgia',serif; font-size:26px; font-weight:600;\">%4</p>")
        .arg(Theme::accentDefault(), g >= 0 ? m_groups.at(g).label.toUpper().toHtmlEscaped() : QString(),
             Theme::textBright(), labelOf(id).toHtmlEscaped());
    return head + out;
}

void HelpPanel::updateContent()
{
    if (!m_content) return;
    QStringList steps;
    QString html = decorate(m_selectedId, contentFor(m_selectedId), &steps);

    // Anterior / próximo, na ordem dos grupos.
    QStringList order;
    for (const Group& g : std::as_const(m_groups)) order += g.topicIds;
    const int i = order.indexOf(m_selectedId);
    auto link = [&](const QString& id, const QString& kicker, bool right) {
        if (id.isEmpty()) return QString();
        return QStringLiteral("<td%1 width=\"50%\"><a href=\"go:%2\" style=\"text-decoration:none;\">"
                              "<span style=\"color:%3; font-size:10px; letter-spacing:0.8px;\">%4</span><br>"
                              "<span style=\"color:%5; font-size:13.5px;\">%6</span></a></td>")
            .arg(right ? QStringLiteral(" align=\"right\"") : QString(), id, Theme::textMuted(),
                 kicker.toHtmlEscaped(), Theme::textPrimary(), labelOf(id).toHtmlEscaped());
    };
    const QString prev = i > 0 ? order.at(i - 1) : QString();
    const QString next = i >= 0 && i + 1 < order.size() ? order.at(i + 1) : QString();
    html += QStringLiteral("<hr style=\"margin-top:22px;\"><table width=\"100%\" cellspacing=\"0\" cellpadding=\"6\"><tr>%1%2</tr></table>")
                .arg(prev.isEmpty() ? QStringLiteral("<td width=\"50%\"></td>") : link(prev, tr("‹ ANTERIOR"), false),
                     link(next, tr("PRÓXIMO ›"), true));

    m_content->setHtml(html);
    m_content->verticalScrollBar()->setValue(0);
    rebuildToc(steps);
}

void HelpPanel::rebuildToc(const QStringList& steps)
{
    qDeleteAll(m_tocButtons);
    m_tocButtons.clear();
    while (QLayoutItem* it = m_tocLayout->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    // Tópico sem passos numerados (texto corrido): a margem some e o texto usa a largura.
    m_tocPanel->setVisible(!steps.isEmpty());
    if (steps.isEmpty()) return;
    auto* title = new QLabel(tr("NESTE TÓPICO"), m_tocPanel);
    title->setObjectName(QStringLiteral("helpTocTitle"));
    m_tocLayout->addWidget(title);
    for (int s = 0; s < steps.size(); ++s) {
        auto* b = new QPushButton(QStringLiteral("%1. %2").arg(s + 1).arg(steps.at(s)), m_tocPanel);
        b->setObjectName(QStringLiteral("helpTocItem"));
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(steps.at(s));
        // Texto longo quebra em vez de alargar a margem.
        b->setMinimumWidth(0);
        b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        const QFontMetrics fm(b->font());
        b->setText(fm.elidedText(b->text(), Qt::ElideRight, kTocWidth - 34));
        connect(b, &QPushButton::clicked, this, [this, s]() {
            m_content->scrollToAnchor(QStringLiteral("step-%1").arg(s + 1));
        });
        m_tocLayout->addWidget(b);
        m_tocButtons << b;
    }
    m_tocLayout->addStretch(1);
    updateTocHighlight();
}

// Marca na margem o passo que está no topo da leitura.
void HelpPanel::updateTocHighlight()
{
    if (m_tocButtons.isEmpty()) return;
    QTextDocument* doc = m_content->document();
    auto* layout = doc->documentLayout();
    const int y = m_content->verticalScrollBar()->value() + 40;
    int current = 0;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        for (auto f = b.begin(); !f.atEnd(); ++f) {
            const QStringList names = f.fragment().charFormat().anchorNames();
            for (const QString& n : names) {
                if (!n.startsWith(QLatin1String("step-"))) continue;
                if (layout->blockBoundingRect(b).top() <= y) current = n.mid(5).toInt() - 1;
            }
        }
    }
    // No fim da rolagem, o último passo é o atual mesmo que não chegue ao topo.
    const QScrollBar* sb = m_content->verticalScrollBar();
    if (sb->maximum() > 0 && sb->value() >= sb->maximum()) current = m_tocButtons.size() - 1;
    for (int i = 0; i < m_tocButtons.size(); ++i) {
        QPushButton* b = m_tocButtons.at(i);
        const bool cur = i == current;
        if (b->property("current").toBool() == cur) continue;
        b->setProperty("current", cur);
        b->style()->unpolish(b);
        b->style()->polish(b);
    }
}

void HelpPanel::onAnchorClicked(const QUrl& url)
{
    if (url.scheme() == QStringLiteral("go")) {
        if (m_searchMode) setSearchMode(false);
        selectTopic(url.path());
        return;
    }
    if (url.scheme() != QStringLiteral("zoom")) return;
    openImageZoom(QStringLiteral(":") + url.path());
}

void HelpPanel::openImageZoom(const QString& resourcePath)
{
    const QPixmap pix(resourcePath);
    if (pix.isNull()) return;

    auto* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Ajuda"));

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* label = new QLabel(dlg);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    const QScreen* screen = QGuiApplication::screenAt(geometry().center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    QSize maxSize = screen ? screen->availableGeometry().size() * 0.85 : QSize(1200, 800);
    QPixmap shown = pix;
    if (shown.width() > maxSize.width() || shown.height() > maxSize.height()) {
        shown = shown.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    label->setPixmap(shown);
    dlg->setFixedSize(shown.size());
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void HelpPanel::openNear(const QRect& anchorGlobal, Qt::Edge barSide)
{
    if (!m_positioned) {
        m_positioned = true;
        QPoint pos(anchorGlobal.right() - width(), anchorGlobal.bottom() + kGapBelowAnchor);
        const QScreen* screen = QGuiApplication::screenAt(anchorGlobal.center());
        if (screen) {
            // Alinhado à direita do anchor (não à esquerda) quando a barra é
            // horizontal — constrói um anchor sintético já deslocado, pra
            // reaproveitar o mesmo AnchorUtils::positionNear dos outros popups.
            QRect effectiveAnchor = anchorGlobal;
            if (barSide == Qt::TopEdge || barSide == Qt::BottomEdge) {
                effectiveAnchor.moveLeft(anchorGlobal.right() - width());
                effectiveAnchor.setWidth(width());
            }
            pos = AnchorUtils::positionNear(effectiveAnchor, size(), barSide,
                                             screen->availableGeometry(), kGapBelowAnchor);
        }
        move(pos);
    }
    show();
    raise();
    activateWindow();
}
