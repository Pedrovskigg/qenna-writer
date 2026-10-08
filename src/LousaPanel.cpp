#include "LousaPanel.h"
#include "SheetDialogs.h"
#include "ColorPopover.h"

#include "CardItem.h"
#include "ConnectionItem.h"
#include "CoverUtils.h"
#include "DocCache.h"
#include "ElementCreateDialog.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "LousaActionBar.h"
#include "LousaExtras.h"
#include "LousaScene.h"
#include "LousaDock.h"
#include "LousaStickers.h"
#include "LousaView.h"
#include "ProjectModel.h"
#include "RoleTiers.h"
#include "Theme.h"
#include "ZoneItem.h"

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QGridLayout>
#include <QMimeData>
#include <QScreen>
#include <QStackedWidget>
#include <QUrl>
#include <QCloseEvent>
#include <QCursor>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFontDatabase>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {
// Acima desse zoom, o conteúdo do card já é legível direto na Lousa — o
// tooltip de hover só ajuda (em vez de atrapalhar) com o zoom reduzido.
constexpr qreal kCardPreviewMaxZoom = 0.6;
// "text" tem fonte ajustável por card — o que importa é o tamanho EFETIVO
// na tela (fonte base * zoom), não o zoom isolado.
constexpr qreal kCardPreviewMinEffectiveFontPx = 14.0;

bool shouldShowCardPreview(const QString& type, int fontSize, qreal zoom)
{
    if (type == QStringLiteral("text")) {
        const int base = fontSize > 0 ? fontSize : 18; // mesmo fallback de CardItem::effFontSize()
        return base * zoom <= kCardPreviewMinEffectiveFontPx;
    }
    return zoom <= kCardPreviewMaxZoom;
}

QToolButton* makeTopBtn(const QString& tip, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setToolButtonStyle(Qt::ToolButtonIconOnly);
    b->setAutoRaise(true);
    b->setToolTip(tip);
    b->setObjectName(QStringLiteral("lousaTopBtn"));
    b->setFixedSize(32, 30);
    b->setIconSize(QSize(16, 16));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QIcon loadLousaIcon(const QString& path)
{
    return IconUtils::loadToolbarIcon(
        path,
        QColor(Theme::textMuted()),
        QColor(Theme::textPrimary()),
        QColor(Theme::textBright()));
}

QString plainOf(const QString& html)
{
    if (!html.contains(QLatin1Char('<'))) return html;
    QTextDocument d;
    d.setHtml(html);
    return d.toPlainText();
}

int wordCount(const QString& html)
{
    static const QRegularExpression kWord(QStringLiteral("\\S+"));
    const QString t = plainOf(html);
    int n = 0;
    auto it = kWord.globalMatch(t);
    while (it.hasNext()) { it.next(); ++n; }
    return n;
}

QString wordsLabel(int n)
{
    if (n <= 0) return QCoreApplication::translate("LousaPanel", "vazio");
    if (n == 1) return QCoreApplication::translate("LousaPanel", "1 palavra");
    return QCoreApplication::translate("LousaPanel", "%1 palavras").arg(QLocale().toString(n));
}

// Nome do tipo de card, pra gaveta da lousa (aba Guardados).
QString cardTypeName(const QString& type)
{
    if (type == QStringLiteral("note"))      return QCoreApplication::translate("LousaPanel", "Post-it");
    if (type == QStringLiteral("comment"))   return QCoreApplication::translate("LousaPanel", "Comentário");
    if (type == QStringLiteral("text"))      return QCoreApplication::translate("LousaPanel", "Texto livre");
    if (type == QStringLiteral("symbol"))    return QCoreApplication::translate("LousaPanel", "Símbolo");
    if (type == QStringLiteral("image"))     return QCoreApplication::translate("LousaPanel", "Imagem");
    if (type == QStringLiteral("sticker"))   return QCoreApplication::translate("LousaPanel", "Adesivo");
    if (type == QStringLiteral("character")) return QCoreApplication::translate("LousaPanel", "Personagem");
    if (type == QStringLiteral("chapter"))   return QCoreApplication::translate("LousaPanel", "Capítulo");
    return QCoreApplication::translate("LousaPanel", "Documento");
}

// "Capítulo 3", "Prólogo" ou o rótulo livre do autor.
QString chapterLabel(const ProjectModel* model, const Chapter* ch)
{
    if (!model || !ch) return QString();
    if (ch->type == QStringLiteral("custom") && !ch->typeLabel.trimmed().isEmpty())
        return ch->typeLabel.trimmed();
    if (ch->type != QStringLiteral("chapter") && !ch->type.isEmpty()) {
        const QString l = ProjectModel::findChapterTypeLabel(ch->type);
        if (!l.isEmpty()) return l;
    }
    int n = 0;
    for (const Chapter* c : model->orderedChaptersForManuscript(ch->manuscriptId)) {
        if (c->type == QStringLiteral("chapter") || c->type.isEmpty()) ++n;
        if (c->id == ch->id) break;
    }
    return QCoreApplication::translate("LousaPanel", "Capítulo %1").arg(n);
}

QPixmap photoFromDataUrl(const QString& url)
{
    // Cache: o mesmo retrato não é decodificado de novo a cada atualização.
    static QHash<QString, QPixmap> cache;
    if (url.isEmpty()) return QPixmap();
    const QString key = QString::number(qHash(url));
    auto it = cache.constFind(key);
    if (it != cache.constEnd()) return it.value();
    QPixmap px = CoverUtils::pixmapFromDataUrl(url);
    if (!px.isNull() && (px.width() > 200 || px.height() > 200))
        px = px.scaled(200, 200, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    if (cache.size() > 200) cache.clear();
    cache.insert(key, px);
    return px;
}

// ── Templates de layout inicial ──────────────────────────────────────────────
// Só oferecidos com a lousa vazia (ver LousaPanel::showTemplatePicker). Cada
// template é um conjunto pronto de zonas/cards/conexões — ponto de partida
// pra quem não sabe por onde começar, não um recurso "vivo" (o usuário edita/
// apaga/substitui à vontade depois de aplicado).

struct BoardTemplate {
    QList<CanvasZone>       zones;
    QList<CanvasCard>       cards;
    QList<CanvasConnection> connections;
};

// Cria um card "note" compacto (tamanho custom, não o default de nextCardData
// — aqui os cards são só nós/prompts curtos, não precisam do tamanho padrão
// de post-it).
CanvasCard templateNote(const QString& title, const QString& content,
                        qreal x, qreal y, qreal w, qreal h, const QString& colorHex)
{
    CanvasCard c;
    c.id      = QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.type    = QStringLiteral("note");
    c.title   = title;
    c.content = content;
    c.x = x; c.y = y; c.width = w; c.height = h;
    c.color = QColor(colorHex);
    return c;
}

CanvasCard templateTitleText(const QString& text, qreal x, qreal y)
{
    CanvasCard c;
    c.id       = QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.type     = QStringLiteral("text");
    c.content  = text;
    c.x = x; c.y = y; c.width = 300; c.height = 50;
    c.color    = QColor(QStringLiteral("#ffffff"));
    c.fontSize = 28;
    return c;
}

CanvasZone templateZone(const QString& title, qreal x, qreal y, qreal w, qreal h, const QString& colorHex)
{
    CanvasZone z;
    z.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    z.title = title;
    z.x = x; z.y = y; z.width = w; z.height = h;
    z.color = QColor(colorHex);
    return z;
}

CanvasConnection templateConnection(const QString& fromId, const QString& toId, const QString& colorHex)
{
    CanvasConnection conn;
    conn.id     = QUuid::createUuid().toString(QUuid::WithoutBraces);
    conn.fromId = fromId;
    conn.toId   = toId;
    conn.color  = QColor(colorHex);
    return conn;
}

BoardTemplate buildCharacterMapTemplate()
{
    BoardTemplate t;
    t.cards.append(templateTitleText(
        QObject::tr("Mapa de Personagens — arraste, edite e conecte à vontade"), 60, 20));

    const CanvasCard protagonist = templateNote(
        QObject::tr("Protagonista"), QObject::tr("Quem carrega a história."),
        460, 140, 200, 100, QStringLiteral("#6ea8fe"));
    const CanvasCard mentor = templateNote(
        QObject::tr("Mentor"), QObject::tr("Guia e ensina o protagonista."),
        100, 280, 200, 100, QStringLiteral("#34d399"));
    const CanvasCard ally = templateNote(
        QObject::tr("Aliado"), QObject::tr("Luta ao lado do protagonista."),
        820, 280, 200, 100, QStringLiteral("#34d399"));
    const CanvasCard antagonist = templateNote(
        QObject::tr("Antagonista"), QObject::tr("Se opõe diretamente ao protagonista."),
        460, 440, 200, 100, QStringLiteral("#f87171"));
    const CanvasCard minorVillain = templateNote(
        QObject::tr("Vilão secundário"), QObject::tr("Serve ou desafia o antagonista principal."),
        820, 580, 220, 100, QStringLiteral("#fbbf24"));

    t.cards << protagonist << mentor << ally << antagonist << minorVillain;
    t.connections
        << templateConnection(protagonist.id, mentor.id,      QStringLiteral("#6ea8fe"))
        << templateConnection(protagonist.id, ally.id,        QStringLiteral("#6ea8fe"))
        << templateConnection(protagonist.id, antagonist.id,  QStringLiteral("#f87171"))
        << templateConnection(antagonist.id,  minorVillain.id, QStringLiteral("#f87171"));
    return t;
}

BoardTemplate buildPlotArcTemplate()
{
    BoardTemplate t;
    t.cards.append(templateTitleText(QObject::tr("Arco da História"), 60, 20));

    t.zones
        << templateZone(QObject::tr("Ato 1 — Detonante"),   40,  100, 320, 420, QStringLiteral("#6ea8fe"))
        << templateZone(QObject::tr("Ato 2 — Confronto"),   400, 100, 320, 420, QStringLiteral("#f97316"))
        << templateZone(QObject::tr("Ato 3 — Resolução"),   760, 100, 320, 420, QStringLiteral("#34d399"));

    t.cards
        << templateNote(QObject::tr("Incidente incitante"),
               QObject::tr("O que tira o herói da zona de conforto?"),
               60, 160, 260, 110, QStringLiteral("#ffd060"))
        << templateNote(QObject::tr("1º ponto de virada"),
               QObject::tr("A decisão que não tem mais volta."),
               60, 320, 260, 110, QStringLiteral("#ffd060"))
        << templateNote(QObject::tr("Meio do meio"),
               QObject::tr("O ponto sem retorno emocional."),
               420, 160, 260, 110, QStringLiteral("#ffd060"))
        << templateNote(QObject::tr("2º ponto de virada"),
               QObject::tr("A crise que empurra pro clímax."),
               420, 320, 260, 110, QStringLiteral("#ffd060"))
        << templateNote(QObject::tr("Clímax"),
               QObject::tr("O confronto final."),
               780, 160, 260, 110, QStringLiteral("#ffd060"))
        << templateNote(QObject::tr("Resolução"),
               QObject::tr("O novo normal do protagonista."),
               780, 320, 260, 110, QStringLiteral("#ffd060"));
    return t;
}

BoardTemplate buildWorldbuildingTemplate()
{
    BoardTemplate t;
    t.cards.append(templateTitleText(QObject::tr("Construção de Mundo"), 60, 20));

    t.zones
        << templateZone(QObject::tr("Geografia"),             40,  100, 380, 300, QStringLiteral("#34d399"))
        << templateZone(QObject::tr("Poder & Política"),      460, 100, 380, 300, QStringLiteral("#f87171"))
        << templateZone(QObject::tr("Cultura & Sociedade"),   40,  440, 380, 300, QStringLiteral("#fbbf24"))
        << templateZone(QObject::tr("História & Mitologia"),  460, 440, 380, 300, QStringLiteral("#a78bfa"));

    t.cards
        << templateNote(QObject::tr("Onde isso acontece?"),
               QObject::tr("Clima, território, o que molda quem vive aqui."),
               70, 170, 300, 120, QStringLiteral("#ffd060"))
        << templateNote(QObject::tr("Quem manda aqui?"),
               QObject::tr("Como o poder é conquistado, mantido ou perdido."),
               490, 170, 300, 120, QStringLiteral("#ffd060"))
        << templateNote(QObject::tr("Como vivem?"),
               QObject::tr("Costumes, crenças, o que é considerado normal."),
               70, 510, 300, 120, QStringLiteral("#ffd060"))
        << templateNote(QObject::tr("O que aconteceu antes?"),
               QObject::tr("Os eventos e mitos que ainda pesam no presente."),
               490, 510, 300, 120, QStringLiteral("#ffd060"));
    return t;
}

BoardTemplate buildBoardTemplate(const QString& id)
{
    if (id == QStringLiteral("plot"))  return buildPlotArcTemplate();
    if (id == QStringLiteral("world")) return buildWorldbuildingTemplate();
    return buildCharacterMapTemplate(); // "characters" (default)
}
} // namespace


LousaPanel::LousaPanel(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("lousaPanel"));
    setWindowTitle(tr("Lousa"));
    setMinimumSize(760, 520);
    resize(1180, 780);
    QSettings st;
    CardItem::setTiltEnabled(st.value(QStringLiteral("lousa/tilt"), true).toBool());
    m_minimapOn = st.value(QStringLiteral("lousa/minimap"), true).toBool();
    buildUi();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &LousaPanel::applyTheme);
}

void LousaPanel::buildChrome()
{
    auto makeFloat = [this](const QString& name) {
        auto* f = new QFrame(this);
        f->setObjectName(name);
        f->setProperty("lousaFloat", true);
        f->setAttribute(Qt::WA_StyledBackground, true);
        auto* l = new QHBoxLayout(f);
        l->setContentsMargins(4, 4, 4, 4);
        l->setSpacing(2);
        auto* sh = new QGraphicsDropShadowEffect(f);
        sh->setBlurRadius(20);
        sh->setOffset(0, 5);
        sh->setColor(QColor(0, 0, 0, 90));
        f->setGraphicsEffect(sh);
        return f;
    };
    auto addSep = [](QFrame* f) {
        auto* sep = new QFrame(f);
        sep->setObjectName(QStringLiteral("lousaTopSep"));
        sep->setFixedSize(1, 18);
        auto* l = static_cast<QHBoxLayout*>(f->layout());
        l->addSpacing(4);
        l->addWidget(sep, 0, Qt::AlignVCenter);
        l->addSpacing(4);
    };

    // ── Canto de cima, à esquerda: qual lousa + desfazer/refazer ──
    m_leftBar = makeFloat(QStringLiteral("lousaLeftBar"));
    m_boardPill = new QToolButton(m_leftBar);
    m_boardPill->setObjectName(QStringLiteral("lousaBoardPill"));
    m_boardPill->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_boardPill->setIconSize(QSize(16, 16));
    m_boardPill->setFixedHeight(30);
    m_boardPill->setCursor(Qt::PointingHandCursor);
    m_boardPill->setToolTip(tr("Trocar, criar, renomear ou excluir lousas"));
    connect(m_boardPill, &QToolButton::clicked, this, &LousaPanel::showBoardMenu);
    m_leftBar->layout()->addWidget(m_boardPill);
    addSep(m_leftBar);
    m_undoBtn = makeTopBtn(tr("Desfazer (Ctrl+Z)"), m_leftBar);
    m_redoBtn = makeTopBtn(tr("Refazer (Ctrl+Y)"), m_leftBar);
    connect(m_undoBtn, &QToolButton::clicked, this, &LousaPanel::undo);
    connect(m_redoBtn, &QToolButton::clicked, this, &LousaPanel::redo);
    m_iconBindings.append({m_undoBtn, QStringLiteral(":/icons/lousa/undo.svg")});
    m_iconBindings.append({m_redoBtn, QStringLiteral(":/icons/lousa/redo.svg")});
    m_leftBar->layout()->addWidget(m_undoBtn);
    m_leftBar->layout()->addWidget(m_redoBtn);

    // ── Canto de cima, à direita: zoom, fundo, exportar, atalhos, fechar ──
    m_rightBar = makeFloat(QStringLiteral("lousaRightBar"));
    m_zoomOutBtn = makeTopBtn(tr("Afastar"), m_rightBar);
    m_zoomLabel = new QToolButton(m_rightBar);
    m_zoomLabel->setObjectName(QStringLiteral("lousaZoomLabel"));
    m_zoomLabel->setText(QStringLiteral("100%"));
    m_zoomLabel->setToolTip(tr("Voltar a 100%"));
    m_zoomLabel->setFixedSize(50, 30);
    m_zoomLabel->setCursor(Qt::PointingHandCursor);
    m_zoomInBtn = makeTopBtn(tr("Aproximar"), m_rightBar);
    m_fitBtn = makeTopBtn(tr("Ver tudo (Ctrl+0)"), m_rightBar);
    connect(m_zoomOutBtn, &QToolButton::clicked, this, [this]() { m_view->zoomBy(1.0 / 1.2); });
    connect(m_zoomInBtn, &QToolButton::clicked, this, [this]() { m_view->zoomBy(1.2); });
    connect(m_zoomLabel, &QToolButton::clicked, this, [this]() { m_view->zoomBy(1.0 / m_view->zoomFactor()); });
    connect(m_fitBtn, &QToolButton::clicked, this, [this]() {
        const QRectF b = m_scene->contentBounds();
        if (b.isValid()) m_view->fitSceneRect(b);
    });
    m_iconBindings.append({m_zoomOutBtn, QStringLiteral(":/icons/lousa/minus.svg")});
    m_iconBindings.append({m_zoomInBtn, QStringLiteral(":/icons/lousa/plus.svg")});
    m_iconBindings.append({m_fitBtn, QStringLiteral(":/icons/lousa/fit.svg")});
    m_rightBar->layout()->addWidget(m_zoomOutBtn);
    m_rightBar->layout()->addWidget(m_zoomLabel);
    m_rightBar->layout()->addWidget(m_zoomInBtn);
    m_rightBar->layout()->addWidget(m_fitBtn);
    addSep(m_rightBar);
    m_lookBtn = makeTopBtn(tr("Fundo e cards"), m_rightBar);
    m_exportBtn = makeTopBtn(tr("Exportar como imagem"), m_rightBar);
    m_helpBtn = makeTopBtn(tr("Atalhos (?)"), m_rightBar);
    m_closeBtn = makeTopBtn(tr("Fechar a lousa"), m_rightBar);
    connect(m_lookBtn, &QToolButton::clicked, this, &LousaPanel::showBoardLook);
    connect(m_exportBtn, &QToolButton::clicked, this, &LousaPanel::exportBoardAsImage);
    connect(m_helpBtn, &QToolButton::clicked, this, [this]() { if (m_cheat) { m_cheat->showOver(); m_cheat->raise(); } });
    connect(m_closeBtn, &QToolButton::clicked, this, &LousaPanel::close);
    m_iconBindings.append({m_lookBtn, QStringLiteral(":/icons/lousa/background.svg")});
    m_iconBindings.append({m_exportBtn, QStringLiteral(":/icons/lousa/export.svg")});
    m_iconBindings.append({m_helpBtn, QStringLiteral(":/icons/lousa/help.svg")});
    m_iconBindings.append({m_closeBtn, QStringLiteral(":/icons/lousa/close.svg")});
    m_rightBar->layout()->addWidget(m_lookBtn);
    m_rightBar->layout()->addWidget(m_exportBtn);
    m_rightBar->layout()->addWidget(m_helpBtn);
    addSep(m_rightBar);
    m_rightBar->layout()->addWidget(m_closeBtn);

    // ── Embaixo: a Doca, e os botões de Guardados e Áreas ──
    m_dock = new LousaDock(this);
    connect(m_dock, &LousaDock::createRequested, this, &LousaPanel::createFromTool);

    auto makeChip = [this](const QString& icon, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setObjectName(QStringLiteral("lousaChip"));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setIconSize(QSize(16, 16));
        b->setFixedHeight(36);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        m_iconBindings.append({b, QStringLiteral(":/icons/lousa/%1.svg").arg(icon)});
        auto* sh = new QGraphicsDropShadowEffect(b);
        sh->setBlurRadius(18);
        sh->setOffset(0, 4);
        sh->setColor(QColor(0, 0, 0, 80));
        b->setGraphicsEffect(sh);
        return b;
    };
    m_stashChip = makeChip(QStringLiteral("stash"), tr("Cards guardados (Delete guarda o card marcado)"));
    connect(m_stashChip, &QToolButton::clicked, this, &LousaPanel::toggleStash);
    m_areasChip = makeChip(QStringLiteral("areas"), tr("Lista das áreas (F)"));
    m_areasChip->setText(tr("Áreas"));
    connect(m_areasChip, &QToolButton::clicked, this, &LousaPanel::toggleMap);

    // Painel dos guardados: clica e o card volta pro quadro.
    m_stashPanel = new QWidget(this);
    m_stashPanel->setObjectName(QStringLiteral("lousaMapPanel"));
    m_stashPanel->setAttribute(Qt::WA_StyledBackground, true);
    m_stashPanel->setVisible(false);
    auto* sl = new QVBoxLayout(m_stashPanel);
    sl->setContentsMargins(12, 12, 12, 12);
    sl->setSpacing(8);
    auto* st = new QLabel(tr("GUARDADOS"), m_stashPanel);
    st->setObjectName(QStringLiteral("lousaMapTitle"));
    sl->addWidget(st);
    m_stashList = new QListWidget(m_stashPanel);
    m_stashList->setObjectName(QStringLiteral("lousaMapList"));
    m_stashList->setIconSize(QSize(14, 14));
    m_stashList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_stashList->setToolTip(tr("Clique pra pôr de volta no quadro; botão direito pra apagar de vez"));
    connect(m_stashList, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        const int i = it->data(Qt::UserRole).toInt();
        if (i >= 0) restoreFromStash(i);
    });
    connect(m_stashList, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* it = m_stashList->itemAt(pos);
        if (!it) return;
        const int i = it->data(Qt::UserRole).toInt();
        if (i < 0 || i >= m_stash.size()) return;
        QMenu menu(this);
        QAction* restore = menu.addAction(tr("Pôr de volta no quadro"));
        QAction* del = menu.addAction(tr("Apagar de vez"));
        QAction* chosen = menu.exec(m_stashList->viewport()->mapToGlobal(pos));
        if (chosen == restore) restoreFromStash(i);
        else if (chosen == del) {
            if (!Sheets::confirm(this, tr("Apagar cards"), tr("Apagar este card definitivamente?"),
                                 QString(), tr("Apagar"))) return;
            m_stash.removeAt(i);
            save();
            refreshStashUi();
        }
    });
    sl->addWidget(m_stashList, 1);
}

void LousaPanel::rebuildTabs()
{
    // A pílula do canto mostra a lousa aberta (e quantas existem).
    if (!m_boardPill) return;
    const int idx = boardIndexOf(m_activeBoardId);
    QString name = idx >= 0 ? m_boards[idx].name : tr("Lousa");
    if (name.size() > 28) name = name.left(26) + QStringLiteral("…");
    if (m_boards.size() > 1)
        name = tr("%1  ·  %2 lousas").arg(name).arg(m_boards.size());
    m_boardPill->setText(name + QStringLiteral("  ▾"));
    if (m_leftBar) m_leftBar->adjustSize();
}

void LousaPanel::showBoardMenu()
{
    if (!m_boardPill) return;
    save();
    QList<LousaBoardPicker::Entry> entries;
    for (const LousaBoardMeta& b : m_boards) entries.append({ b.id, b.name, b.file });
    const QString r = LousaBoardPicker::pick(this, m_boardPill->mapToGlobal(QPoint(m_boardPill->width() / 2, m_boardPill->height() + 6)),
                                             m_projectRoot, entries, m_activeBoardId, true);
    if (r.isEmpty()) return;
    if (r == QStringLiteral("__new__")) { createNewBoard(); return; }
    if (r.startsWith(QStringLiteral("__rename__:"))) { renameBoard(r.mid(11)); return; }
    if (r.startsWith(QStringLiteral("__delete__:"))) { deleteBoard(r.mid(11)); return; }
    switchToBoard(r);
}

bool LousaPanel::eventFilter(QObject* o, QEvent* e)
{
    return QWidget::eventFilter(o, e);
}

void LousaPanel::toggleStash()
{
    if (!m_stashPanel) return;
    m_stashOpen = !m_stashOpen;
    if (m_stashOpen && m_mapOpen) toggleMap();
    if (m_stashOpen) {
        refreshStashUi();
        positionOverlays();
        m_stashPanel->setVisible(true);
        m_stashPanel->raise();
    } else {
        m_stashPanel->setVisible(false);
    }
}

void LousaPanel::refreshStashUi()
{
    if (m_stashChip) {
        m_stashChip->setText(m_stash.isEmpty() ? tr("Guardados")
                                               : tr("Guardados  %1").arg(m_stash.size()));
        m_stashChip->adjustSize();
    }
    if (!m_stashList) return;
    m_stashList->clear();
    for (int i = 0; i < m_stash.size(); ++i) {
        const CanvasCard& c = m_stash[i];
        const bool isSticker = (c.type == QStringLiteral("sticker"));
        QString label = c.title.trimmed();
        if (label.isEmpty() && !isSticker) {
            const QString text = (c.type == QStringLiteral("image")) ? c.description : plainOf(c.content);
            label = text.simplified().left(48);
        }
        if (label.isEmpty()) label = cardTypeName(c.type);
        const bool paper = (c.type == QStringLiteral("note") || c.type == QStringLiteral("comment"));
        const QColor col = paper ? c.color
                         : (c.type == QStringLiteral("character")) ? QColor(0xf3, 0xef, 0xe6)
                         : QColor(0x5a, 0x5a, 0x56);
        QPixmap dot(28, 28);
        dot.setDevicePixelRatio(2.0);
        dot.fill(Qt::transparent);
        QPainter p(&dot);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        if (isSticker) {
            QImage img;
            img.loadFromData(QByteArray::fromBase64(c.content.toLatin1()));
            if (!img.isNull())
                p.drawImage(QRectF(0, 0, 14, 14), img.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                                                     .copy(QRect(0, 0, 28, 28)));
        } else {
            p.setBrush(col);
            p.drawRoundedRect(QRectF(1, 2, 12, 10), 2, 2);
        }
        p.end();
        auto* item = new QListWidgetItem(QIcon(dot), QStringLiteral("%1  ·  %2").arg(label, cardTypeName(c.type)));
        item->setData(Qt::UserRole, i);
        m_stashList->addItem(item);
    }
    if (m_stash.isEmpty()) {
        auto* item = new QListWidgetItem(tr("Nada guardado. Delete guarda o card marcado."));
        item->setData(Qt::UserRole, -1);
        item->setFlags(Qt::NoItemFlags);
        m_stashList->addItem(item);
    }
    positionOverlays();
}

QString LousaPanel::pickBoard(QWidget* parent, const QPoint& globalPos) const
{
    save();
    QList<LousaBoardPicker::Entry> entries;
    for (const LousaBoardMeta& b : m_boards) entries.append({ b.id, b.name, b.file });
    return LousaBoardPicker::pick(parent, globalPos, m_projectRoot, entries, m_activeBoardId);
}

QString LousaPanel::boardsManifestPath() const
{
    return QDir::cleanPath(m_projectRoot + QStringLiteral("/lousas.json"));
}

QString LousaPanel::activeBoardFile() const
{
    const int idx = boardIndexOf(m_activeBoardId);
    return idx >= 0 ? m_boards[idx].file : QStringLiteral("canvas.json");
}

int LousaPanel::boardIndexOf(const QString& boardId) const
{
    for (int i = 0; i < m_boards.size(); ++i)
        if (m_boards[i].id == boardId) return i;
    return -1;
}

void LousaPanel::loadBoardsManifest()
{
    m_boards.clear();
    m_activeBoardId.clear();

    QFile f(boardsManifestPath());
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        for (const auto& v : root.value(QStringLiteral("boards")).toArray()) {
            const QJsonObject o = v.toObject();
            LousaBoardMeta b;
            b.id   = o.value(QStringLiteral("id")).toString();
            b.name = o.value(QStringLiteral("name")).toString();
            b.file = o.value(QStringLiteral("file")).toString();
            if (!b.id.isEmpty() && !b.file.isEmpty()) m_boards.append(b);
        }
        m_activeBoardId = root.value(QStringLiteral("activeBoardId")).toString();
    }

    if (m_boards.isEmpty()) {
        // Projeto sem manifesto ainda — tanto faz se já existia um
        // canvas.json (projeto de antes das múltiplas lousas) ou se é um
        // projeto totalmente novo: em ambos os casos nasce com uma única
        // lousa, migrada automaticamente, sem ação manual do usuário.
        LousaBoardMeta first;
        first.id   = QUuid::createUuid().toString(QUuid::WithoutBraces);
        first.name = tr("Lousa 1");
        first.file = QStringLiteral("canvas.json");
        m_boards.append(first);
        m_activeBoardId = first.id;
        saveBoardsManifest();
    } else if (boardIndexOf(m_activeBoardId) < 0) {
        m_activeBoardId = m_boards.first().id;
    }
}

void LousaPanel::saveBoardsManifest() const
{
    if (m_projectRoot.isEmpty()) return;
    QJsonArray boards;
    for (const LousaBoardMeta& b : m_boards) {
        QJsonObject o;
        o.insert(QStringLiteral("id"),   b.id);
        o.insert(QStringLiteral("name"), b.name);
        o.insert(QStringLiteral("file"), b.file);
        boards.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("boards"), boards);
    root.insert(QStringLiteral("activeBoardId"), m_activeBoardId);

    QSaveFile f(boardsManifestPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.commit();
}



void LousaPanel::switchToBoard(const QString& boardId)
{
    if (boardId == m_activeBoardId || boardIndexOf(boardId) < 0) { rebuildTabs(); return; }
    save();   // persiste a lousa atual antes de trocar
    m_activeBoardId = boardId;
    saveBoardsManifest();
    // Undo/redo não deve atravessar boards — cada lousa tem sua própria história.
    m_undo.clear();
    m_redo.clear();
    refreshUndoButtons();
    load();
    rebuildTabs();
}

void LousaPanel::createNewBoard()
{
    bool ok = false;
    const QString name = Sheets::askText(this, tr("Nova lousa"), tr("Nome da lousa"),
        tr("Lousa %1").arg(m_boards.size() + 1), &ok, tr("Criar")).trimmed();
    if (!ok || name.isEmpty()) return;

    save();   // persiste a lousa atual antes de trocar

    LousaBoardMeta meta;
    meta.id   = QUuid::createUuid().toString(QUuid::WithoutBraces);
    meta.name = name;
    meta.file = QStringLiteral("canvas-%1.json").arg(meta.id);
    m_boards.append(meta);
    m_activeBoardId = meta.id;
    saveBoardsManifest();

    m_undo.clear();
    m_redo.clear();
    refreshUndoButtons();
    load();   // arquivo ainda não existe → carrega lousa vazia
    rebuildTabs();
}

void LousaPanel::renameBoard(const QString& boardId)
{
    const int idx = boardIndexOf(boardId);
    if (idx < 0) return;
    bool ok = false;
    const QString newName = Sheets::askText(this, tr("Renomear lousa"), tr("Nome da lousa"),
        m_boards[idx].name, &ok).trimmed();
    if (!ok || newName.isEmpty()) return;
    m_boards[idx].name = newName;
    saveBoardsManifest();
    rebuildTabs();
}

void LousaPanel::deleteBoard(const QString& boardId)
{
    if (m_boards.size() <= 1) {
        QMessageBox::information(this, tr("Excluir lousa"),
            tr("Não é possível excluir a última lousa do projeto."));
        return;
    }
    const int idx = boardIndexOf(boardId);
    if (idx < 0) return;

    if (!Sheets::confirm(this, tr("Excluir lousa"),
            tr("Excluir a lousa \"%1\" e todo o seu conteúdo? Essa ação não pode ser desfeita.")
                .arg(m_boards[idx].name), QString(), tr("Excluir")))
        return;

    const LousaBoardMeta removed = m_boards.takeAt(idx);
    QFile::remove(QDir::cleanPath(m_projectRoot + QStringLiteral("/") + removed.file));

    if (removed.id == m_activeBoardId) {
        m_activeBoardId = m_boards.first().id;
        saveBoardsManifest();
        m_undo.clear();
        m_redo.clear();
        refreshUndoButtons();
        load();
    } else {
        saveBoardsManifest();
    }
    rebuildTabs();
}

void LousaPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Quadro (ocupa a janela toda; o resto flutua por cima) ────────────
    m_scene = new LousaScene(this);
    m_view  = new LousaView(m_scene, this);
    root->addWidget(m_view, 1);
    buildChrome();

    // ── Sinais do quadro ─────────────────────────────────────────────────
    connect(m_view, &LousaView::zoneDrawn, this, [this](const QRectF& r) {
        if (!m_scene) return;
        m_dock->setToolChecked(QStringLiteral("area"), false);
        pushUndo();
        CanvasZone z;
        z.id     = QUuid::createUuid().toString(QUuid::WithoutBraces);
        z.x      = r.x();  z.y = r.y();
        z.width  = r.width(); z.height = r.height();
        z.title  = tr("Área");
        z.color  = QColor(QStringLiteral("#6ea8fe"));
        m_scene->addZone(z);
        m_scene->selectZone(z.id);
        refreshEmptyState();
        save();
    });
    connect(m_view, &LousaView::zoomChanged, this, [this](qreal z) {
        if (m_zoomLabel) m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(z * 100)));
        m_scene->setViewZoom(z);
        if (m_cardPreview && m_cardPreview->isVisible()
            && !shouldShowCardPreview(m_previewCardType, m_previewCardFontSize, z)) {
            hideCardPreview();
        }
        save();
    });
    connect(m_view, &LousaView::viewportMoved, this, [this]() {
        positionActionBar();
        if (m_minimap) m_minimap->scheduleRefresh();
    });
    connect(m_view, &LousaView::backgroundDoubleClicked, this, [this](const QPointF& p) { createText(p); });
    connect(m_view, &LousaView::itemDropped, this, [this](const QString& payload, const QPointF& p) {
        placeFromPayload(payload, &p);
    });
    connect(m_view, &LousaView::imageDropped, this, [this](const QImage& img, const QPointF& p) {
        placeImage(img, &p);
    });
    connect(m_view, &LousaView::connectPicked, this, &LousaPanel::askNewConnection);
    connect(m_view, &LousaView::connectModeChanged, this, [this](bool on) {
        m_dock->setToolChecked(QStringLiteral("connect"), on);
    });

    connect(m_scene, &LousaScene::zoneDataChanged, this, [this]() { m_scene->refreshZoneCounts(); save(); });
    connect(m_scene, &LousaScene::cardDataChanged,       this, [this]() { save(); scheduleTrayRefresh(); });
    connect(m_scene, &LousaScene::connectionDataChanged, this, [this]() { save(); });
    connect(m_scene, &LousaScene::undoSnapshotRequested, this, [this]() { pushUndo(); });
    connect(m_scene, &LousaScene::selectionChanged, this, &LousaPanel::scheduleActionBar);
    connect(m_scene, &LousaScene::gestureFinished, this, &LousaPanel::positionActionBar);
    connect(m_scene, &LousaScene::cardOpenRequested, this, &LousaPanel::openCard);
    connect(m_scene, &LousaScene::connectionLabelEditRequested, this, &LousaPanel::editConnectionLabel);
    connect(m_scene, &LousaScene::connectionMenuRequested, this, &LousaPanel::showConnectionMenu);
    connect(m_scene, &LousaScene::boardLookChanged, this, [this]() {
        if (m_minimap) m_minimap->scheduleRefresh();
    });
    connect(m_scene, &QGraphicsScene::changed, this, [this](const QList<QRectF>&) {
        positionActionBar();
        if (m_minimap) m_minimap->scheduleRefresh();
        refreshEmptyState();
    });
    connect(m_scene, &LousaScene::cardStashRequested, this, [this](const CanvasCard& c) {
        m_stash.append(c);
        save();
        scheduleTrayRefresh();
    });
    connect(m_scene, &LousaScene::cardCreateDocRequested, this,
            [this](const CanvasCard& c) { createDocFromCard(c); });
    connect(m_scene, &LousaScene::cardCreateTimelineEventRequested, this,
            [this](const CanvasCard& c) {
        emit createTimelineEventRequested(c.title.trimmed(), c.content);
    });
    connect(m_scene, &LousaScene::zoneExportRequested, this, [this](const QString& id) {
        for (const CanvasZone& z : m_scene->allZoneData())
            if (z.id == id) { exportZones({z}); break; }
    });
    connect(m_scene, &LousaScene::pendingConnection, this, &LousaPanel::askNewConnection);

    buildCardPreview();
    connect(m_scene, &LousaScene::cardHoverPreview, this,
            [this](const CanvasCard& data, const QPoint& screenPos) {
        m_previewCardType = data.type;
        m_previewCardFontSize = data.fontSize;
        const qreal zoom = m_view ? m_view->zoomFactor() : 1.0;
        if (!shouldShowCardPreview(data.type, data.fontSize, zoom)) return;
        showCardPreview(data, screenPos);
    });
    connect(m_scene, &LousaScene::cardHoverDismissed, this, [this]() { hideCardPreview(); });

    // ── Por cima do quadro ───────────────────────────────────────────────
    m_actionBar = new LousaActionBar(this);
    connect(m_actionBar, &LousaActionBar::triggered, this, &LousaPanel::onAction);

    m_minimap = new LousaMinimap(m_scene, m_view, this);
    connect(m_minimap, &LousaMinimap::centerRequested, this, [this](const QPointF& p) {
        m_view->centerOn(p);
        positionActionBar();
        m_minimap->scheduleRefresh();
    });

    m_search = new LousaSearchBar(this);
    connect(m_search, &LousaSearchBar::queryChanged, this, &LousaPanel::runSearch);
    connect(m_search, &LousaSearchBar::next, this, [this]() { stepSearch(1); });
    connect(m_search, &LousaSearchBar::previous, this, [this]() { stepSearch(-1); });
    connect(m_search, &LousaSearchBar::closed, this, [this]() { m_view->setFocus(); });

    m_emptyStateBox = new QWidget(this);
    m_emptyStateBox->setObjectName(QStringLiteral("lousaEmptyBox"));
    auto* esLay = new QVBoxLayout(m_emptyStateBox);
    esLay->setAlignment(Qt::AlignCenter);
    esLay->setSpacing(12);
    m_emptyLabel = new QLabel(tr("A lousa está vazia.\nEscolha algo na barra aqui embaixo, "
                                 "ou dê dois cliques no quadro pra escrever."), m_emptyStateBox);
    m_emptyLabel->setObjectName(QStringLiteral("lousaEmpty"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    esLay->addWidget(m_emptyLabel);
    m_useTemplateBtn = new QPushButton(tr("Ou comece com um modelo…"), m_emptyStateBox);
    m_useTemplateBtn->setObjectName(QStringLiteral("lousaTemplateBtn"));
    m_useTemplateBtn->setCursor(Qt::PointingHandCursor);
    connect(m_useTemplateBtn, &QPushButton::clicked, this, &LousaPanel::showTemplatePicker);
    esLay->addWidget(m_useTemplateBtn, 0, Qt::AlignHCenter);

    buildMapPanel();
    m_cheat = new LousaCheatSheet(this);

    m_trayTimer = new QTimer(this);
    m_trayTimer->setSingleShot(true);
    m_trayTimer->setInterval(120);
    connect(m_trayTimer, &QTimer::timeout, this, &LousaPanel::refreshTray);
    m_barTimer = new QTimer(this);
    m_barTimer->setSingleShot(true);
    m_barTimer->setInterval(0);
    connect(m_barTimer, &QTimer::timeout, this, &LousaPanel::refreshActionBar);

    rebuildTabs();
    refreshUndoButtons();
    reloadIcons();
}

// ── Pôr coisas no quadro ─────────────────────────────────────────────────────

QPointF LousaPanel::viewCenter() const
{
    return m_view ? m_view->mapToScene(m_view->viewport()->rect().center()) : QPointF(100, 100);
}

CanvasCard LousaPanel::cardAt(const QString& type, const QPointF& center) const
{
    CanvasCard c = nextCardData(type);
    c.x = center.x() - c.width / 2.0;
    c.y = center.y() - c.height / 2.0;
    return c;
}

QColor LousaPanel::boardInk() const
{
    return CardItem::boardIsLight() ? QColor(0x2a, 0x26, 0x22) : QColor(0xf5, 0xf4, 0xef);
}

CardItem* LousaPanel::placeCard(const CanvasCard& c)
{
    if (!m_scene) return nullptr;
    CardItem* item = m_scene->addCard(c);
    m_scene->selectOnlyCard(item);
    refreshEmptyState();
    scheduleTrayRefresh();
    return item;
}

void LousaPanel::createFromTool(const QString& kind)
{
    if (!m_scene || !m_view) return;
    if (kind != QStringLiteral("area") && m_view->isPlanMode()) {
        m_view->setPlanMode(false);
        m_dock->setToolChecked(QStringLiteral("area"), false);
    }
    if (kind != QStringLiteral("connect") && m_view->isConnectMode()) m_view->setConnectMode(false);

    if (kind == QStringLiteral("postit") || kind == QStringLiteral("comment")) {
        pushUndo();
        placeCard(nextCardData(kind == QStringLiteral("postit") ? QStringLiteral("note") : QStringLiteral("comment")));
    } else if (kind == QStringLiteral("text")) {
        createText(viewCenter() - QPointF(150, 16));
    } else if (kind == QStringLiteral("symbol")) {
        createSymbol();
    } else if (kind == QStringLiteral("image")) {
        createImage();
    } else if (kind == QStringLiteral("sticker")) {
        showStickerPicker();
    } else if (kind == QStringLiteral("area")) {
        const bool on = !m_view->isPlanMode();
        m_view->setPlanMode(on);
        m_dock->setToolChecked(QStringLiteral("area"), on);
    } else if (kind == QStringLiteral("connect")) {
        m_view->setConnectMode(!m_view->isConnectMode());
    } else if (kind == QStringLiteral("areas")) {
        toggleMap();
    } else if (kind == QStringLiteral("character")) {
        pickCharacterForBoard();
    } else if (kind == QStringLiteral("doc")) {
        pickDocForBoard();
    }
    m_view->setFocus();
}

void LousaPanel::createText(const QPointF& at)
{
    if (!m_scene) return;
    pushUndo();
    CanvasCard c = nextCardData(QStringLiteral("text"));
    c.x = at.x();
    c.y = at.y();
    c.color = boardInk();
    if (CardItem* item = placeCard(c)) {
        m_view->setFocus();
        item->beginTextEdit(true);
    }
}

void LousaPanel::createSymbol(const QPointF* at)
{
    QString sym = QStringLiteral("★");
    if (!CardItem::pickSymbol(this, sym)) return;
    pushUndo();
    CanvasCard c = at ? cardAt(QStringLiteral("symbol"), *at) : nextCardData(QStringLiteral("symbol"));
    c.content = sym;
    placeCard(c);
}

void LousaPanel::createImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Escolher imagem"), QString(),
        tr("Imagens (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
    if (path.isEmpty()) return;
    const QImage img(path);
    if (img.isNull()) return;
    createImageCard(img, nullptr);
}

void LousaPanel::createImageCard(const QImage& src, const QPointF* at)
{
    if (!m_scene || src.isNull()) return;
    QImage img = src;
    if (img.width() > 900 || img.height() > 900)
        img = img.scaled(900, 900, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (img.hasAlphaChannel()) {
        // JPEG não tem transparência: o que for transparente vira papel claro, não preto.
        QImage flat(img.size(), QImage::Format_RGB32);
        flat.fill(QColor(0xf3, 0xef, 0xe6));
        QPainter fp(&flat);
        fp.drawImage(0, 0, img);
        fp.end();
        img = flat;
    }
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", 82);
    pushUndo();
    CanvasCard c = nextCardData(QStringLiteral("image"));
    // A moldura acompanha a proporção da foto (a legenda fica embaixo).
    const qreal photoW = c.width - 14.0;
    const qreal photoH = qBound(90.0, photoW * img.height() / qMax(1, img.width()), 340.0);
    c.height = photoH + 7.0 + CardItem::kCaptionH;
    if (at) {
        c.x = at->x() - c.width / 2.0;
        c.y = at->y() - c.height / 2.0;
    }
    c.content = QString::fromLatin1(ba.toBase64());
    placeCard(c);
    save();
}

// ── Adesivos ─────────────────────────────────────────────────────────────────

// Corta a sobra transparente em volta: o adesivo começa onde o desenho começa.
static QImage trimTransparent(const QImage& src)
{
    const QImage a = src.convertToFormat(QImage::Format_ARGB32);
    int x0 = a.width(), y0 = a.height(), x1 = -1, y1 = -1;
    for (int y = 0; y < a.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(a.constScanLine(y));
        for (int x = 0; x < a.width(); ++x) {
            if (qAlpha(line[x]) <= 8) continue;
            x0 = qMin(x0, x); x1 = qMax(x1, x);
            y0 = qMin(y0, y); y1 = qMax(y1, y);
        }
    }
    if (x1 < 0) return a;   // toda transparente: deixa como está
    const QRect r = QRect(QPoint(x0, y0), QPoint(x1, y1)).adjusted(-2, -2, 2, 2).intersected(a.rect());
    return a.copy(r);
}

void LousaPanel::placeImage(const QImage& img, const QPointF* at)
{
    if (img.isNull()) return;
    if (LousaStickers::hasTransparency(img)) placeSticker(img, at, QString(), 0.0, true);
    else                                     createImageCard(img, at);
}

void LousaPanel::placeSticker(const QImage& src, const QPointF* at, const QString& outline,
                              qreal longest, bool remember)
{
    if (!m_scene || src.isNull()) return;
    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    if (qMax(img.width(), img.height()) > 1000)
        img = img.scaled(1000, 1000, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    img = trimTransparent(img);
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");

    pushUndo();
    CanvasCard c = nextCardData(QStringLiteral("sticker"));
    const qreal L = longest > 0 ? longest : qMin(220.0, qreal(qMax(img.width(), img.height())));
    const qreal sc = L / qMax(1, qMax(img.width(), img.height()));
    c.width  = qMax(8.0, img.width() * sc);
    c.height = qMax(8.0, img.height() * sc);
    c.baseWidth = c.width;
    const QPointF ctr = at ? *at : viewCenter();
    c.x = ctr.x() - c.width / 2.0;
    c.y = ctr.y() - c.height / 2.0;
    c.content = QString::fromLatin1(ba.toBase64());
    c.outline = outline;
    qreal top = 0.0;
    for (CardItem* x : m_scene->cardItems())
        if (x->isSticker()) top = qMax(top, x->cardData().z);
    c.z = top + 1.0;
    placeCard(c);
    if (remember) LousaStickers::remember(img);
    save();
}

void LousaPanel::setStickerLayer(CardItem* card, const QString& how)
{
    if (!m_scene || !card || !card->isSticker()) return;
    pushUndo();
    QList<CardItem*> front, behind;
    for (CardItem* x : m_scene->cardItems())
        if (x->isSticker() && x != card) (x->cardData().z > 0 ? front : behind) << x;
    auto byZ = [](CardItem* a, CardItem* b) { return a->cardData().z < b->cardData().z; };
    std::sort(front.begin(), front.end(), byZ);
    std::sort(behind.begin(), behind.end(), byZ);
    if (how == QStringLiteral("front"))     front.append(card);    // por cima de tudo
    else if (how == QStringLiteral("back")) front.prepend(card);   // logo acima dos cards
    else                                    behind.append(card);   // atrás dos cards
    // Por cima dos cards: 1, 2, 3...; atrás: entre as áreas (-2) e as linhas (-1).
    for (int i = 0; i < front.size(); ++i) front[i]->setStickerZ(1.0 + i);
    for (int i = 0; i < behind.size(); ++i)
        behind[i]->setStickerZ(-1.9 + 0.8 * (i + 1) / (behind.size() + 1));
    save();
    scheduleActionBar();
}

bool LousaPanel::pasteImageFromClipboard()
{
    const QMimeData* md = QApplication::clipboard()->mimeData();
    if (!md) return false;
    QImage img;
    // PNG primeiro: é o formato que guarda a transparência.
    if (md->hasFormat(QStringLiteral("image/png")))
        img = QImage::fromData(md->data(QStringLiteral("image/png")), "PNG");
    if (img.isNull()) {
        for (const QUrl& u : md->urls()) {
            if (!u.isLocalFile()) continue;
            img = QImage(u.toLocalFile());
            if (!img.isNull()) break;
        }
    }
    if (img.isNull() && md->hasImage()) img = qvariant_cast<QImage>(md->imageData());
    if (img.isNull()) return false;
    QPointF at = viewCenter();
    if (m_view) {
        const QPoint vp = m_view->viewport()->mapFromGlobal(QCursor::pos());
        if (m_view->viewport()->rect().contains(vp)) at = m_view->mapToScene(vp);
    }
    placeImage(img, &at);
    return true;
}

namespace {

// Botão da cartela / da folha de estilo, com a miniatura desenhada.
QToolButton* thumbButton(QWidget* parent, const QPixmap& pm, const QSize& box, const QString& tip,
                         const QString& objectName)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(objectName);
    b->setIcon(QIcon(pm));
    b->setIconSize(box - QSize(10, 10));
    b->setFixedSize(box);
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QPixmap fitPixmap(const QImage& img, const QSize& size, qreal dpr)
{
    QPixmap pm(QSize(qRound(size.width() * dpr), qRound(size.height() * dpr)));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    if (img.isNull()) return pm;
    QPainter p(&pm);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal sc = qMin(size.width() / qreal(img.width()), size.height() / qreal(img.height()));
    const QSizeF s(img.width() * sc, img.height() * sc);
    p.drawImage(QRectF(QPointF((size.width() - s.width()) / 2.0, (size.height() - s.height()) / 2.0), s), img);
    p.end();
    return pm;
}

QString popupQss(const QString& panel)
{
    return Theme::qss(QStringLiteral(
        "QWidget#%7 { background: %1; border: 1px solid %2; border-radius: @radius-panel; }"
        "QLabel#lousaPopTitle { color: %4; font-size: 11px; font-weight: 700; letter-spacing: 1px; background: transparent; }"
        "QLabel#lousaPopHint { color: %4; font-size: 11px; background: transparent; }"
        "QToolButton#lousaPopTab { background: transparent; color: %3; border: 1px solid %2;"
        "  border-radius: 11px; padding: 3px 12px; font-size: 12px; }"
        "QToolButton#lousaPopTab:checked { background: %3; color: %1; border-color: %3; }"
        "QToolButton#lousaPopCell { background: #efeae2; border: 1px solid transparent; border-radius: @radius-control; }"
        "QToolButton#lousaPopCell:hover { border-color: %6; }"
        "QToolButton#lousaPopOpt { background: %5; border: 1.5px solid transparent; border-radius: @radius-control; }"
        "QToolButton#lousaPopOpt:hover { border-color: %2; }"
        "QToolButton#lousaPopOpt:checked { border-color: %6; }"
        "QToolButton#lousaPopSwatch { border: 2px solid transparent; border-radius: 11px; }"
        "QToolButton#lousaPopSwatch:checked { border-color: %6; }"
        "QPushButton#lousaPopBtn { background: %6; color: white; border: none; border-radius: @radius-control;"
        "  padding: 6px 14px; font-weight: 600; }"
        "QCheckBox { color: %3; background: transparent; spacing: 8px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; border: 1.5px solid %4; border-radius: 3px; background: transparent; }"
        "QCheckBox::indicator:checked { background: %6; border-color: %6; }"
        "QFrame#lousaPopSep { background: %2; border: none; }"
        "QScrollArea, QWidget#lousaPopPage { background: transparent; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(),
          Theme::textMuted(), Theme::hoverOverlay(), Theme::accentDefault(), panel));
}

QDialog* makePopupDialog(QWidget* parent, QWidget** panelOut, const QString& objectName)
{
    auto* dlg = new QDialog(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    dlg->setAttribute(Qt::WA_TranslucentBackground);
    auto* outer = new QVBoxLayout(dlg);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* panel = new QWidget(dlg);
    panel->setObjectName(objectName);
    panel->setAttribute(Qt::WA_StyledBackground, true);
    outer->addWidget(panel);
    panel->setStyleSheet(popupQss(objectName));
    *panelOut = panel;
    return dlg;
}

void placePopup(QDialog* dlg, const QRect& anchor, bool above)
{
    dlg->adjustSize();
    const QSize s = dlg->sizeHint();
    QPoint at(anchor.center().x() - s.width() / 2, above ? anchor.top() - s.height() - 10 : anchor.bottom() + 8);
    if (QScreen* scr = QGuiApplication::screenAt(anchor.center())) {
        const QRect ar = scr->availableGeometry();
        at.setX(qBound(ar.left() + 8, at.x(), ar.right() - s.width() - 8));
        at.setY(qBound(ar.top() + 8, at.y(), ar.bottom() - s.height() - 8));
    }
    dlg->move(at);
}

QFrame* popupSep(QWidget* parent)
{
    auto* sep = new QFrame(parent);
    sep->setObjectName(QStringLiteral("lousaPopSep"));
    sep->setFixedHeight(1);
    return sep;
}

} // namespace

void LousaPanel::showStickerPicker()
{
    if (!m_scene) return;
    QWidget* panel = nullptr;
    std::unique_ptr<QDialog> dlg(makePopupDialog(this, &panel, QStringLiteral("lousaStkPanel")));
    auto* pl = new QVBoxLayout(panel);
    pl->setContentsMargins(14, 12, 14, 12);
    pl->setSpacing(10);
    const qreal dpr = devicePixelRatioF();
    const QSize cell(58, 58);

    // Abas: a cartela da Qenna e os PNGs já usados
    auto* tabs = new QHBoxLayout;
    tabs->setSpacing(6);
    auto* tabQenna = new QToolButton(panel);
    tabQenna->setObjectName(QStringLiteral("lousaPopTab"));
    tabQenna->setText(tr("Da Qenna"));
    auto* tabRecent = new QToolButton(panel);
    tabRecent->setObjectName(QStringLiteral("lousaPopTab"));
    tabRecent->setText(tr("Usados antes"));
    for (QToolButton* t : { tabQenna, tabRecent }) {
        t->setCheckable(true);
        t->setAutoExclusive(true);
        t->setCursor(Qt::PointingHandCursor);
        tabs->addWidget(t);
    }
    tabs->addStretch(1);
    pl->addLayout(tabs);

    auto* stack = new QStackedWidget(panel);
    QString picked;   // "qenna:<id>", "recent:<arquivo>" ou "file"
    QDialog* d = dlg.get();
    auto pick = [d, &picked](const QString& what) { picked = what; d->accept(); };

    // Página 1: a cartela da Qenna
    auto* pageQ = new QWidget;
    pageQ->setObjectName(QStringLiteral("lousaPopPage"));
    auto* ql = new QVBoxLayout(pageQ);
    ql->setContentsMargins(0, 0, 0, 0);
    ql->setSpacing(6);
    auto addGroup = [&](const QString& title, const QStringList& ids) {
        auto* lbl = new QLabel(title, pageQ);
        lbl->setObjectName(QStringLiteral("lousaPopTitle"));
        ql->addWidget(lbl);
        auto* gw = new QWidget(pageQ);
        gw->setObjectName(QStringLiteral("lousaPopPage"));
        auto* g = new QGridLayout(gw);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(6);
        int n = 0;
        for (const QString& id : ids) {
            const QImage img = LousaStickers::render(id, 120);
            QToolButton* b = thumbButton(gw, fitPixmap(img, cell - QSize(14, 14), dpr), cell,
                                         LousaStickers::label(id), QStringLiteral("lousaPopCell"));
            connect(b, &QToolButton::clicked, d, [pick, id]() { pick(QStringLiteral("qenna:") + id); });
            g->addWidget(b, n / 6, n % 6);
            ++n;
        }
        ql->addWidget(gw);
    };
    addGroup(tr("MARCAÇÕES"), LousaStickers::markIds());
    addGroup(tr("CARIMBOS E OBJETOS"), LousaStickers::objectIds());
    stack->addWidget(pageQ);

    // Página 2: usados antes (em qualquer projeto)
    auto* pageR = new QWidget;
    pageR->setObjectName(QStringLiteral("lousaPopPage"));
    auto* rl = new QVBoxLayout(pageR);
    rl->setContentsMargins(0, 0, 0, 0);
    const QList<LousaStickers::Recent> recents = LousaStickers::recent();
    if (recents.isEmpty()) {
        auto* empty = new QLabel(tr("Os PNGs que você puser no quadro aparecem aqui, em qualquer projeto."), pageR);
        empty->setObjectName(QStringLiteral("lousaPopHint"));
        empty->setWordWrap(true);
        empty->setMinimumHeight(80);
        empty->setAlignment(Qt::AlignCenter);
        rl->addWidget(empty);
    } else {
        auto* scroll = new QScrollArea(pageR);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* gw = new QWidget;
        gw->setObjectName(QStringLiteral("lousaPopPage"));
        auto* g = new QGridLayout(gw);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(6);
        int n = 0;
        for (const LousaStickers::Recent& r : recents) {
            QToolButton* b = thumbButton(gw, fitPixmap(r.image, cell - QSize(14, 14), dpr), cell, QString(),
                                         QStringLiteral("lousaPopCell"));
            const QString path = r.path;
            connect(b, &QToolButton::clicked, d, [pick, path]() { pick(QStringLiteral("recent:") + path); });
            g->addWidget(b, n / 6, n % 6);
            ++n;
        }
        g->setRowStretch(g->rowCount(), 1);
        scroll->setWidget(gw);
        scroll->setFixedHeight(qMin(3, (n + 5) / 6) * (cell.height() + 6));
        rl->addWidget(scroll);
    }
    stack->addWidget(pageR);
    pl->addWidget(stack);

    QSettings st;
    const bool showRecent = st.value(QStringLiteral("lousa/stickerTab")).toString() == QStringLiteral("recent")
                            && !recents.isEmpty();
    (showRecent ? tabRecent : tabQenna)->setChecked(true);
    stack->setCurrentIndex(showRecent ? 1 : 0);
    connect(tabQenna, &QToolButton::clicked, d, [stack]() { stack->setCurrentIndex(0); });
    connect(tabRecent, &QToolButton::clicked, d, [stack]() { stack->setCurrentIndex(1); });

    pl->addWidget(popupSep(panel));
    auto* foot = new QHBoxLayout;
    foot->setSpacing(10);
    auto* fileBtn = new QPushButton(tr("Escolher PNG…"), panel);
    fileBtn->setObjectName(QStringLiteral("lousaPopBtn"));
    fileBtn->setCursor(Qt::PointingHandCursor);
    connect(fileBtn, &QPushButton::clicked, d, [pick]() { pick(QStringLiteral("file")); });
    foot->addWidget(fileBtn);
    auto* hint = new QLabel(tr("ou arraste uma imagem pro quadro,\nou cole com Ctrl+V"), panel);
    hint->setObjectName(QStringLiteral("lousaPopHint"));
    foot->addWidget(hint, 1);
    pl->addLayout(foot);

    placePopup(d, m_dock ? m_dock->toolGlobalRect(QStringLiteral("sticker")) : QRect(QCursor::pos(), QSize(1, 1)), true);
    const int res = d->exec();
    st.setValue(QStringLiteral("lousa/stickerTab"), stack->currentIndex() == 1 ? QStringLiteral("recent") : QStringLiteral("qenna"));
    if (res != QDialog::Accepted || picked.isEmpty()) return;

    if (picked.startsWith(QStringLiteral("qenna:"))) {
        const QString id = picked.mid(6);
        placeSticker(LousaStickers::render(id, 480), nullptr, LousaStickers::defaultOutline(id),
                     LousaStickers::defaultSize(id), false);
    } else if (picked.startsWith(QStringLiteral("recent:"))) {
        placeSticker(QImage(picked.mid(7)), nullptr, QString(), 0.0, true);
    } else {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Escolher adesivo"), QString(),
            tr("Imagens (*.png *.webp *.gif *.jpg *.jpeg *.bmp)"));
        if (path.isEmpty()) return;
        const QImage img(path);
        if (img.isNull()) return;
        // Pelo botão Adesivo, até foto sem transparência vira adesivo (com a borda branca).
        placeSticker(img, nullptr, QString(), 0.0, true);
    }
    m_view->setFocus();
}

void LousaPanel::showNoteStyle(CardItem* card, const QPoint& globalPos)
{
    if (!card || !m_scene) return;
    QWidget* panel = nullptr;
    std::unique_ptr<QDialog> dlg(makePopupDialog(this, &panel, QStringLiteral("lousaStylePanel")));
    QDialog* d = dlg.get();
    auto* pl = new QVBoxLayout(panel);
    pl->setContentsMargins(14, 12, 14, 12);
    pl->setSpacing(8);
    const qreal dpr = devicePixelRatioF();
    const bool isNote = card->cardData().type == QStringLiteral("note");

    struct Opt { QString key; QString label; };
    const QList<Opt> shapes = {
        { QString(), tr("Quadrado") }, { QStringLiteral("index"), tr("Ficha pautada") },
        { QStringLiteral("strip"), tr("Tira") }, { QStringLiteral("round"), tr("Redondo") },
        { QStringLiteral("tag"), tr("Etiqueta") }, { QStringLiteral("torn"), tr("Folha arrancada") },
        { QStringLiteral("pinked"), tr("Picotado") }, { QStringLiteral("dymo"), tr("Fita rotuladora") } };
    const QList<Opt> fasteners = {
        { QString(), tr("Pin") }, { QStringLiteral("tape"), tr("Fita adesiva") },
        { QStringLiteral("clip"), tr("Clipe") }, { QStringLiteral("staple"), tr("Grampo") },
        { QStringLiteral("none"), tr("Nada") } };
    const QList<Opt> frames = {
        { QString(), tr("Nenhuma") }, { QStringLiteral("pen"), tr("Caneta") },
        { QStringLiteral("cut"), tr("Recorte") }, { QStringLiteral("corners"), tr("Cantoneiras") } };
    const QList<QColor> palette = {
        QColor(QStringLiteral("#f6d06a")), QColor(QStringLiteral("#f39a95")), QColor(QStringLiteral("#6dd3ac")),
        QColor(QStringLiteral("#b49cf5")), QColor(QStringLiteral("#8fc3f0")), QColor(QStringLiteral("#f4a96a")),
        QColor(QStringLiteral("#e8e4dc")) };

    QList<QToolButton*> shapeBtns, fastBtns, frameBtns, swatches;
    auto section = [&](const QString& title, const QList<Opt>& opts, int cols, const QSize& box,
                       QList<QToolButton*>& out) {
        auto* lbl = new QLabel(title, panel);
        lbl->setObjectName(QStringLiteral("lousaPopTitle"));
        pl->addWidget(lbl);
        auto* gw = new QWidget(panel);
        gw->setObjectName(QStringLiteral("lousaPopPage"));
        auto* g = new QGridLayout(gw);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(6);
        for (int i = 0; i < opts.size(); ++i) {
            QToolButton* b = thumbButton(gw, QPixmap(), box, opts[i].label, QStringLiteral("lousaPopOpt"));
            b->setCheckable(true);
            g->addWidget(b, i / cols, i % cols);
            out << b;
        }
        pl->addWidget(gw);
    };
    section(tr("FORMATO"), shapes, 4, QSize(68, 58), shapeBtns);
    section(tr("PRESO COM"), fasteners, 5, QSize(53, 58), fastBtns);
    section(tr("BORDA"), frames, 4, QSize(68, 58), frameBtns);

    auto* colorLbl = new QLabel(tr("COR"), panel);
    colorLbl->setObjectName(QStringLiteral("lousaPopTitle"));
    pl->addWidget(colorLbl);
    auto* colors = new QHBoxLayout;
    colors->setSpacing(4);
    for (const QColor& c : palette) {
        QPixmap dot(QSize(qRound(18 * dpr), qRound(18 * dpr)));
        dot.setDevicePixelRatio(dpr);
        dot.fill(Qt::transparent);
        QPainter p(&dot);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(c.darker(118), 1));
        p.setBrush(c);
        p.drawEllipse(QRectF(1, 1, 16, 16));
        p.end();
        QToolButton* b = thumbButton(panel, dot, QSize(26, 26), QString(), QStringLiteral("lousaPopSwatch"));
        b->setIconSize(QSize(18, 18));
        b->setCheckable(true);
        colors->addWidget(b);
        swatches << b;
    }
    auto* more = new QToolButton(panel);
    more->setObjectName(QStringLiteral("lousaPopTab"));
    more->setText(tr("Outra…"));
    more->setCursor(Qt::PointingHandCursor);
    colors->addWidget(more);
    colors->addStretch(1);
    pl->addLayout(colors);

    QCheckBox* keep = nullptr;
    QSettings st;
    auto savedStyle = [&st]() {
        return QStringList{ st.value(QStringLiteral("lousa/noteShape")).toString(),
                            st.value(QStringLiteral("lousa/noteFastener")).toString(),
                            st.value(QStringLiteral("lousa/noteFrame")).toString(),
                            st.value(QStringLiteral("lousa/noteColor")).toString() };
    };
    auto currentStyle = [card]() {
        const CanvasCard cd = card->cardData();
        return QStringList{ cd.shape, cd.fastener, cd.frame, cd.color.name() };
    };
    if (isNote) {
        pl->addWidget(popupSep(panel));
        keep = new QCheckBox(tr("Usar nos próximos post-its"), panel);
        keep->setCursor(Qt::PointingHandCursor);
        const QStringList saved = savedStyle();
        keep->setChecked(!saved.at(3).isEmpty() && saved == currentStyle());
        pl->addWidget(keep);
    }
    auto storeDefaults = [&st, keep, currentStyle]() {
        if (!keep) return;
        const QStringList s = keep->isChecked() ? currentStyle() : QStringList{ QString(), QString(), QString(), QString() };
        st.setValue(QStringLiteral("lousa/noteShape"), s.at(0));
        st.setValue(QStringLiteral("lousa/noteFastener"), s.at(1));
        st.setValue(QStringLiteral("lousa/noteFrame"), s.at(2));
        st.setValue(QStringLiteral("lousa/noteColor"), s.at(3));
    };

    // Miniaturas desenhadas com o card de verdade, na cor e no jeito atuais.
    auto refresh = [&]() {
        const CanvasCard cd = card->cardData();
        auto preview = [&](CanvasCard p, const QSize& box) {
            p.id = QStringLiteral("preview-") + p.shape + p.fastener + p.frame;
            p.x = p.y = 0;
            p.title = (p.shape == QStringLiteral("dymo")) ? tr("Fita") : QString();
            p.content.clear();
            const QSizeF sz = CardItem::defaultNoteSize(p.shape, p.type);
            p.width = sz.width();
            p.height = sz.height();
            return CardItem::renderPreview(p, box - QSize(12, 12), dpr);
        };
        for (int i = 0; i < shapes.size(); ++i) {
            CanvasCard p = cd;
            p.shape = shapes[i].key;
            p.frame.clear();
            shapeBtns[i]->setIcon(QIcon(preview(p, shapeBtns[i]->size())));
            shapeBtns[i]->setChecked(cd.shape == shapes[i].key);
        }
        for (int i = 0; i < fasteners.size(); ++i) {
            CanvasCard p = cd;
            p.fastener = fasteners[i].key;
            p.frame.clear();
            fastBtns[i]->setIcon(QIcon(preview(p, fastBtns[i]->size())));
            fastBtns[i]->setChecked(cd.fastener == fasteners[i].key);
        }
        for (int i = 0; i < frames.size(); ++i) {
            CanvasCard p = cd;
            p.frame = frames[i].key;
            frameBtns[i]->setIcon(QIcon(preview(p, frameBtns[i]->size())));
            frameBtns[i]->setChecked(cd.frame == frames[i].key);
        }
        for (int i = 0; i < palette.size(); ++i)
            swatches[i]->setChecked(cd.color.name() == palette[i].name());
    };
    for (QToolButton* b : shapeBtns) b->setIconSize(b->size() - QSize(12, 12));
    for (QToolButton* b : fastBtns)  b->setIconSize(b->size() - QSize(12, 12));
    for (QToolButton* b : frameBtns) b->setIconSize(b->size() - QSize(12, 12));
    refresh();

    auto apply = [&](const QString& shape, const QString& fastener, const QString& frame) {
        card->setNoteStyle(shape, fastener, frame);
        storeDefaults();
        refresh();
    };
    for (int i = 0; i < shapes.size(); ++i)
        connect(shapeBtns[i], &QToolButton::clicked, d, [&, i]() {
            const CanvasCard cd = card->cardData();
            apply(shapes[i].key, cd.fastener, cd.frame);
        });
    for (int i = 0; i < fasteners.size(); ++i)
        connect(fastBtns[i], &QToolButton::clicked, d, [&, i]() {
            const CanvasCard cd = card->cardData();
            apply(cd.shape, fasteners[i].key, cd.frame);
        });
    for (int i = 0; i < frames.size(); ++i)
        connect(frameBtns[i], &QToolButton::clicked, d, [&, i]() {
            const CanvasCard cd = card->cardData();
            apply(cd.shape, cd.fastener, frames[i].key);
        });
    for (int i = 0; i < palette.size(); ++i)
        connect(swatches[i], &QToolButton::clicked, d, [&, i]() {
            card->setCardColor(palette[i]);
            storeDefaults();
            refresh();
        });
    connect(more, &QToolButton::clicked, d, [&]() {
        const QColor nc = ColorPopover::getColor(card->cardData().color, this, tr("Cor"));
        if (nc.isValid()) card->setCardColor(nc);
        storeDefaults();
        refresh();
    });
    if (keep) connect(keep, &QCheckBox::toggled, d, [&]() { storeDefaults(); });

    placePopup(d, QRect(globalPos, QSize(1, 1)), false);
    d->exec();
    m_scene->refreshZoneCounts();
    save();
    scheduleActionBar();
}

void LousaPanel::pickDocForBoard()
{
    if (!m_scene || !m_projectModel) return;
    // Os capítulos do manuscrito aberto primeiro, depois os documentos das gavetas.
    struct Entry { QString payload, title, sub; };
    QVector<Entry> entries;
    const QString ms = m_projectModel->activeManuscriptId();
    for (const Chapter* ch : m_projectModel->orderedChaptersForManuscript(ms))
        entries.append({QStringLiteral("chapter:%1:%2").arg(ms, ch->id),
                        ch->title.isEmpty() ? tr("Capítulo sem título") : ch->title,
                        chapterLabel(m_projectModel, ch)});
    for (const Drawer& d : m_projectModel->drawers())
        for (const DrawerItem& it : d.items) {
            if (d.drawerElementType == QStringLiteral("character") || it.elementType == QStringLiteral("character"))
                continue;   // personagem tem o botão dele
            entries.append({QStringLiteral("doc:") + it.id, it.title, d.title});
        }
    if (entries.isEmpty()) return;
    QVector<Sheets::PickItem> items;
    for (const Entry& e : entries) items.append({e.title, e.sub, QPixmap()});
    const int idx = Sheets::askPick(this, tr("Documento ou capítulo"), tr("Buscar..."), items, tr("Pôr na lousa"),
                                    QString(), tr("Nada nas gavetas nem no manuscrito ainda."));
    if (idx < 0 || idx >= entries.size()) return;
    const QPointF c = viewCenter();
    placeFromPayload(entries[idx].payload, &c);
}

void LousaPanel::pickCharacterForBoard()
{
    if (!m_scene || !m_projectModel) return;
    struct CEntry { QString drawerName, itemId, title, elementId; QString drawerKey; };
    QVector<CEntry> entries;
    for (const Drawer& d : m_projectModel->drawers()) {
        const bool isChar = (d.drawerElementType == QStringLiteral("character"));
        for (const DrawerItem& it : d.items) {
            if (!isChar && it.elementType != QStringLiteral("character")) continue;
            entries.append({d.title, it.id, it.title, it.elementId, d.key});
        }
    }
    QSet<QString> drawerKeys;
    for (const CEntry& e : entries) drawerKeys.insert(e.drawerKey);
    QVector<Sheets::PickItem> items;
    for (const CEntry& e : entries) {
        QPixmap photo;
        if (m_elementsStore && !e.elementId.isEmpty())
            if (const Element* el = m_elementsStore->findElement(e.elementId))
                photo = CoverUtils::pixmapFromDataUrl(el->image);
        items.append({e.title, drawerKeys.size() > 1 ? e.drawerName : QString(), photo});
    }
    const int idx = Sheets::askPick(this, tr("Personagem na lousa"), tr("Buscar personagem..."), items,
                                    tr("Pôr na lousa"), tr("+ Novo personagem"), tr("Nenhum personagem ainda."));
    if (idx == Sheets::kPickExtra) { newCharacterOnBoard(); return; }
    if (idx < 0 || idx >= entries.size()) return;
    const QPointF c = viewCenter();
    placeFromPayload(QStringLiteral("character:") + entries[idx].itemId, &c);
}

void LousaPanel::placeFromPayload(const QString& payload, const QPointF* at)
{
    if (!m_scene) return;
    const QStringList parts = payload.split(QLatin1Char(':'));
    const QString kind = parts.value(0);
    if (kind == QStringLiteral("stash")) {
        restoreFromStash(parts.value(1).toInt(), at);
        return;
    }
    const QPointF center = at ? *at : viewCenter();

    // Clique num item que já está no quadro: vai até ele em vez de duplicar.
    auto flyToExisting = [this, at](const QString& type, const QString& linkedId) {
        if (at) return false;
        for (CardItem* ci : m_scene->cardItems()) {
            const CanvasCard d = ci->cardData();
            if (d.type == type && d.linkedItemId == linkedId) {
                m_scene->selectOnlyCard(ci);
                m_view->centerOnAnimated(ci->sceneBoundingRect().center());
                return true;
            }
        }
        return false;
    };

    if ((kind == QStringLiteral("character") || kind == QStringLiteral("doc")) && m_projectModel) {
        const QString itemId = parts.value(1);
        QString drawerKey;
        const DrawerItem* di = m_projectModel->findDrawerItem(itemId, &drawerKey);
        if (!di) return;
        if (flyToExisting(kind, itemId)) return;
        pushUndo();
        CanvasCard c = cardAt(kind, center);
        c.title = di->title;
        c.linkedDrawerKey = drawerKey;
        c.linkedItemId = itemId;
        if (kind == QStringLiteral("character") && m_elementsStore && !di->elementId.isEmpty())
            if (const Element* el = m_elementsStore->findElement(di->elementId))
                c.photoDataUrl = el->image;
        placeCard(c);
        refreshDocCards();
        return;
    }
    if (kind == QStringLiteral("chapter") && m_projectModel) {
        const QString msId = parts.value(1), chId = parts.value(2);
        const Chapter* ch = m_projectModel->findChapter(chId);
        if (!ch) return;
        if (flyToExisting(kind, chId)) return;
        pushUndo();
        CanvasCard c = cardAt(QStringLiteral("chapter"), center);
        c.title = ch->title;
        c.linkedDrawerKey = msId;
        c.linkedItemId = chId;
        placeCard(c);
        refreshDocCards();
    }
}

void LousaPanel::newCharacterOnBoard(const QPointF* at)
{
    if (!m_projectModel) return;
    QString targetDrawerKey;
    for (const Drawer& d : m_projectModel->drawers()) {
        if (d.drawerElementType == QStringLiteral("character")) {
            targetDrawerKey = d.key;
            break;
        }
    }
    if (targetDrawerKey.isEmpty()) {
        QMessageBox::information(this, tr("Sem gaveta de personagens"),
            tr("Crie primeiro uma gaveta de personagens no projeto."));
        return;
    }
    const QPointF where = at ? *at : viewCenter();
    openElementSheet(QStringLiteral("character"), QString(), [this, targetDrawerKey, where](ElementCreateDialog* dlg) {
        if (!m_scene || !m_projectModel || !m_projectModel->findDrawer(targetDrawerKey)) return;
        const QString name = dlg->title().trimmed();
        if (name.isEmpty()) return;

        Element elem;
        elem.name      = name;
        elem.type      = QStringLiteral("character");
        elem.icon      = QStringLiteral("user");
        elem.role      = dlg->role();
        elem.image     = dlg->imageDataUrl();
        elem.narrator  = dlg->narrator();
        elem.gender    = dlg->gender();
        elem.trackMode = dlg->trackMode();
        elem.aliases   = dlg->aliases();
        const QString elementId = m_elementsStore ? m_elementsStore->addElement(elem) : QString();

        DrawerItem newItem;
        newItem.id            = ProjectModel::uid();
        newItem.title         = elem.name;
        newItem.hasInlineHtml = true;
        newItem.html          = QStringLiteral("<p></p>");
        newItem.elementType   = QStringLiteral("character");
        newItem.elementId     = elementId;
        newItem.role          = elem.role;
        m_projectModel->addDrawerItem(targetDrawerKey, newItem);

        pushUndo();
        CanvasCard c = cardAt(QStringLiteral("character"), where);
        c.title           = elem.name;
        c.linkedDrawerKey = targetDrawerKey;
        c.linkedItemId    = newItem.id;
        c.photoDataUrl    = elem.image;
        placeCard(c);
        refreshDocCards();
    });
}

void LousaPanel::openCard(const CanvasCard& c)
{
    if (c.type == QStringLiteral("chapter"))
        emit openDocKeyRequested(DocCache::chapterKey(c.linkedDrawerKey, c.linkedItemId));
    else if (!c.linkedItemId.isEmpty())
        emit openDocKeyRequested(DocCache::itemKey(c.linkedItemId));
}

QString LousaPanel::htmlFor(const QString& docKey) const
{
    return m_htmlProvider ? m_htmlProvider(docKey) : QString();
}

void LousaPanel::setHtmlProvider(std::function<QString(const QString&)> provider)
{
    m_htmlProvider = std::move(provider);
}

// ── Gaveta da lousa ──────────────────────────────────────────────────────────

void LousaPanel::restoreFromStash(int index, const QPointF* at)
{
    if (index < 0 || index >= m_stash.size() || !m_scene) return;
    pushUndo();
    CanvasCard c = m_stash.takeAt(index);
    const QPointF center = at ? *at : viewCenter();
    c.x = center.x() - c.width / 2.0;
    c.y = center.y() - c.height / 2.0;
    placeCard(c);
    refreshDocCards();
    save();
}

void LousaPanel::stashSelectedCards()
{
    if (!m_scene) return;
    const QList<CardItem*> sel = m_scene->selectedCardItems();
    if (sel.isEmpty()) return;
    pushUndo();
    for (CardItem* c : sel) {
        m_stash.append(c->cardData());
        m_scene->removeCard(c->cardData().id);
    }
    refreshEmptyState();
    scheduleTrayRefresh();
    save();
}

void LousaPanel::deleteSelectedCards()
{
    if (!m_scene) return;
    const QList<CardItem*> sel = m_scene->selectedCardItems();
    if (sel.isEmpty()) return;
    const int n = sel.size();
    if (!Sheets::confirm(this, tr("Apagar cards"),
            n == 1 ? tr("Apagar este card definitivamente?")
                   : tr("Apagar %1 cards definitivamente?").arg(n),
            tr("Dá pra voltar com Ctrl+Z enquanto a lousa estiver aberta."), tr("Apagar")))
        return;
    pushUndo();
    for (CardItem* c : sel) m_scene->removeCard(c->cardData().id);
    refreshEmptyState();
    scheduleTrayRefresh();
    save();
}

// ── Bandeja ──────────────────────────────────────────────────────────────────

void LousaPanel::scheduleTrayRefresh()
{
    if (m_trayTimer && !m_trayTimer->isActive()) m_trayTimer->start();
}

void LousaPanel::refreshTray()
{
    refreshStashUi();
}

// ── Barra de ações ───────────────────────────────────────────────────────────

void LousaPanel::scheduleActionBar()
{
    if (m_barTimer && !m_barTimer->isActive()) m_barTimer->start();
}

void LousaPanel::refreshActionBar()
{
    if (!m_actionBar || !m_scene) return;
    using Btn = LousaActionBar::Btn;
    QVector<Btn> b;
    const QList<CardItem*> sel = m_scene->selectedCardItems();
    auto btn = [](const QString& id, const QString& icon, const QString& text, const QString& tip = QString()) {
        Btn x; x.id = id; x.icon = icon; x.text = text; x.tip = tip; return x;
    };
    auto tail = [&]() {
        Btn s = btn(QStringLiteral("stash"), QStringLiteral("stash"), tr("Guardar"),
                    tr("Guardar na gaveta da lousa (Delete)"));
        s.separatorBefore = true;
        b << s;
        Btn d = btn(QStringLiteral("delete"), QStringLiteral("trash"), QString(), tr("Apagar de vez (Shift+Delete)"));
        d.danger = true;
        b << d;
    };
    if (sel.size() == 1) {
        const CanvasCard d = sel.first()->cardData();
        const QString t = d.type;
        Btn color; color.id = QStringLiteral("color"); color.swatch = d.color; color.tip = tr("Cor");
        if (t == QStringLiteral("note") || t == QStringLiteral("comment")) {
            b << color;
            b << btn(QStringLiteral("style"), QStringLiteral("style"), tr("Estilo"), tr("Formato, presilha e borda"));
            Btn doc = btn(QStringLiteral("doc"), QStringLiteral("doc-plus"), tr("Documento"),
                          tr("Criar um documento com este card"));
            doc.separatorBefore = true;
            b << doc;
            b << btn(QStringLiteral("event"), QStringLiteral("event"), tr("Evento"), tr("Virar evento na Timeline"));
            tail();
        } else if (t == QStringLiteral("text")) {
            b << color;
            QString fam = d.fontFamily.isEmpty() ? QStringLiteral("Segoe UI") : d.fontFamily;
            if (fam.size() > 16) fam = fam.left(15) + QStringLiteral("…");
            Btn font = btn(QStringLiteral("font"), QStringLiteral("font"), fam, tr("Fonte"));
            font.separatorBefore = true;
            b << font;
            Btn bold = btn(QStringLiteral("bold"), QStringLiteral("bold"), QString(), tr("Negrito"));
            bold.checkable = true; bold.checked = d.bold;
            Btn ital = btn(QStringLiteral("italic"), QStringLiteral("italic"), QString(), tr("Itálico"));
            ital.checkable = true; ital.checked = d.italic;
            b << bold << ital;
            tail();
        } else if (t == QStringLiteral("symbol")) {
            b << color;
            Btn sym = btn(QStringLiteral("symbol"), QStringLiteral("symbol"), tr("Trocar"), tr("Trocar o símbolo"));
            sym.separatorBefore = true;
            b << sym;
            tail();
        } else if (t == QStringLiteral("sticker")) {
            Btn fx = btn(QStringLiteral("stk-flipx"), QStringLiteral("flip-h"), tr("Espelhar"));
            fx.checkable = true; fx.checked = d.flipX;
            Btn fy = btn(QStringLiteral("stk-flipy"), QStringLiteral("flip-v"), tr("Virar"), tr("De ponta-cabeça"));
            fy.checkable = true; fy.checked = d.flipY;
            b << fx << fy;
            const QString on = d.outline == QStringLiteral("none")   ? tr("Sem contorno")
                             : d.outline == QStringLiteral("shadow") ? tr("Só sombra")
                                                                     : tr("Recorte branco");
            Btn ol = btn(QStringLiteral("stk-outline"), QStringLiteral("outline"), on, tr("Contorno"));
            ol.separatorBefore = true;
            b << ol;
            b << btn(QStringLiteral("stk-layer"), QStringLiteral("layers"), tr("Camada"),
                     tr("Pra frente, pra trás ou atrás dos cards"));
            Btn lk = btn(QStringLiteral("stk-lock"), d.locked ? QStringLiteral("lock") : QStringLiteral("unlock"),
                         d.locked ? tr("Destravar") : tr("Travar"),
                         d.locked ? tr("Destravar: volta a mexer") : tr("Travar no lugar"));
            lk.separatorBefore = true; lk.checkable = true; lk.checked = d.locked;
            b << lk;
            tail();
        } else if (t == QStringLiteral("image")) {
            b << btn(QStringLiteral("image"), QStringLiteral("photo-swap"), tr("Trocar imagem"));
            b << btn(QStringLiteral("caption"), QStringLiteral("label"), tr("Legenda"));
            b << btn(QStringLiteral("doc"), QStringLiteral("doc-plus"), tr("Documento"), tr("Criar um documento com esta imagem"));
            tail();
        } else if (t == QStringLiteral("character")) {
            b << btn(QStringLiteral("flip"), QStringLiteral("flip"),
                     sel.first()->isFlipped() ? tr("Foto") : tr("Ficha"), tr("Virar o card (dois cliques)"));
            b << btn(QStringLiteral("open"), QStringLiteral("open"), tr("Abrir"), tr("Abrir o documento do personagem"));
            tail();
        } else {
            b << btn(QStringLiteral("open"), QStringLiteral("open"), tr("Abrir"), tr("Abrir no editor"));
            tail();
        }
    } else if (sel.size() > 1) {
        b << btn(QStringLiteral("align-top"), QStringLiteral("align-top"), QString(), tr("Alinhar pelo topo"));
        b << btn(QStringLiteral("align-middle"), QStringLiteral("align-middle"), QString(), tr("Alinhar pelo meio"));
        b << btn(QStringLiteral("align-left"), QStringLiteral("align-left"), QString(), tr("Alinhar pela esquerda"));
        b << btn(QStringLiteral("distribute-h"), QStringLiteral("distribute-h"), QString(), tr("Espaçar igual na horizontal"));
        b << btn(QStringLiteral("distribute-v"), QStringLiteral("distribute-v"), QString(), tr("Espaçar igual na vertical"));
        b << btn(QStringLiteral("grid"), QStringLiteral("grid"), tr("Arrumar"), tr("Arrumar em grade"));
        tail();
    } else if (ConnectionItem* ci = m_scene->findConnection(m_scene->selectedConnectionId())) {
        const CanvasConnection d = ci->connData();
        Btn color; color.id = QStringLiteral("line-color"); color.swatch = d.color; color.tip = tr("Cor da linha");
        b << color;
        Btn name = btn(QStringLiteral("line-label"), QStringLiteral("label"),
                       d.label.isEmpty() ? tr("Dar nome") : tr("Nome"), tr("Nome da ligação (dois cliques na linha)"));
        name.separatorBefore = true;
        b << name;
        Btn arrow = btn(QStringLiteral("line-arrow"), QStringLiteral("arrow"), tr("Seta"), tr("Seta na ponta"));
        arrow.checkable = true; arrow.checked = d.arrow;
        Btn curve = btn(QStringLiteral("line-curve"), QStringLiteral("curve"), tr("Curva"),
                        tr("Linha em curva (o padrão é reta, de pin a pin)"));
        curve.checkable = true; curve.checked = d.curved;
        b << arrow << curve;
        Btn del = btn(QStringLiteral("line-delete"), QStringLiteral("trash"), QString(), tr("Remover a linha"));
        del.danger = true; del.separatorBefore = true;
        b << del;
    } else if (!m_scene->selectedZoneId().isEmpty()) {
        for (const CanvasZone& z : m_scene->allZoneData()) {
            if (z.id != m_scene->selectedZoneId()) continue;
            Btn color; color.id = QStringLiteral("zone-color"); color.swatch = z.color; color.tip = tr("Cor da área");
            b << color;
            Btn name = btn(QStringLiteral("zone-rename"), QStringLiteral("label"), tr("Renomear"));
            name.separatorBefore = true;
            b << name;
            b << btn(QStringLiteral("zone-export"), QStringLiteral("export"), tr("Virar gaveta"),
                     tr("Exportar a área para uma gaveta (Ctrl+D)"));
            Btn del = btn(QStringLiteral("zone-delete"), QStringLiteral("trash"), QString(), tr("Remover a área"));
            del.danger = true; del.separatorBefore = true;
            b << del;
        }
    }
    if (b.isEmpty()) { m_actionBar->hide(); return; }
    m_actionBar->setButtons(b);
    positionActionBar();
}

void LousaPanel::positionActionBar()
{
    if (!m_actionBar || !m_scene || !m_view) return;
    QRectF sr;
    const QList<CardItem*> sel = m_scene->selectedCardItems();
    if (!sel.isEmpty()) {
        for (CardItem* c : sel) sr = sr.united(c->mapRectToScene(c->shape().boundingRect()));
    } else if (ConnectionItem* ci = m_scene->findConnection(m_scene->selectedConnectionId())) {
        const QPointF a = ci->labelAnchor();
        sr = QRectF(a - QPointF(1, 10), QSizeF(2, 20));
    } else if (!m_scene->selectedZoneId().isEmpty()) {
        for (ZoneItem* z : m_scene->zoneItems())
            if (z->zoneData().id == m_scene->selectedZoneId())
                sr = QRectF(z->pos() + QPointF(0, -14), QSizeF(z->zoneData().width, 28));
    }
    if (!sr.isValid()) { m_actionBar->hide(); return; }
    // Durante um arrasto a barra sai da frente.
    if (m_scene->mouseGrabberItem() && (QApplication::mouseButtons() & Qt::LeftButton)) {
        m_actionBar->hide();
        return;
    }
    const QRect vr = m_view->mapFromScene(sr).boundingRect().translated(m_view->pos());
    const QRect area = m_view->geometry();
    if (!area.intersects(vr)) { m_actionBar->hide(); return; }
    m_actionBar->adjustSize();
    const int bw = m_actionBar->width(), bh = m_actionBar->height();
    int x = vr.center().x() - bw / 2;
    int y = vr.top() - bh - 12;
    const int topLimit = area.top() + 64;   // abaixo dos cantos de cima
    if (y < topLimit) y = vr.bottom() + 12;
    x = qBound(area.left() + 8, x, area.right() - bw - 8);
    y = qBound(area.top() + 8, y, area.bottom() - bh - 8);
    m_actionBar->move(x, y);
    m_actionBar->show();
    m_actionBar->raise();
    if (m_cheat && m_cheat->isVisible()) m_cheat->raise();   // a cola fica por cima de tudo
}

void LousaPanel::onAction(const QString& id, const QPoint& globalPos)
{
    if (!m_scene) return;
    const QList<CardItem*> sel = m_scene->selectedCardItems();
    CardItem* one = sel.size() == 1 ? sel.first() : nullptr;

    if (id == QStringLiteral("stash"))  { stashSelectedCards(); return; }
    if (id == QStringLiteral("delete")) { deleteSelectedCards(); return; }
    if (id.startsWith(QStringLiteral("align-")) || id.startsWith(QStringLiteral("distribute-"))
        || id == QStringLiteral("grid")) { alignSelection(id); return; }

    if (one) {
        const CanvasCard d = one->cardData();
        if (id == QStringLiteral("color")) {
            const QColor nc = ColorPopover::getColor(d.color, this, tr("Cor"));
            if (nc.isValid()) one->setCardColor(nc);
        } else if (id == QStringLiteral("doc")) {
            createDocFromCard(d);
        } else if (id == QStringLiteral("event")) {
            emit createTimelineEventRequested(d.title.trimmed(), d.content);
        } else if (id == QStringLiteral("font")) {
            static const QStringList kFamilies = {
                QStringLiteral("Segoe UI"), QStringLiteral("Georgia"), QStringLiteral("Garamond"),
                QStringLiteral("Palatino Linotype"), QStringLiteral("Book Antiqua"), QStringLiteral("Cambria"),
                QStringLiteral("Constantia"), QStringLiteral("Times New Roman"), QStringLiteral("Alegreya"),
                QStringLiteral("Courier New"), QStringLiteral("Consolas"), QStringLiteral("Segoe Print"),
                QStringLiteral("Segoe Script"), QStringLiteral("Ink Free"), QStringLiteral("Gabriola"),
                QStringLiteral("Comic Sans MS"), QStringLiteral("Impact") };
            const QStringList installed = QFontDatabase::families();
            QMenu menu(this);
            const QString cur = d.fontFamily.isEmpty() ? QStringLiteral("Segoe UI") : d.fontFamily;
            for (const QString& f : kFamilies) {
                if (!installed.contains(f)) continue;
                QAction* a = menu.addAction(f);
                QFont af(f); af.setPointSize(11);
                a->setFont(af);
                a->setCheckable(true);
                a->setChecked(f == cur);
                a->setData(f);
            }
            if (QAction* chosen = menu.exec(globalPos))
                one->setTextStyle(chosen->data().toString(), d.bold, d.italic);
        } else if (id == QStringLiteral("bold")) {
            one->setTextStyle(d.fontFamily, !d.bold, d.italic);
        } else if (id == QStringLiteral("italic")) {
            one->setTextStyle(d.fontFamily, d.bold, !d.italic);
        } else if (id == QStringLiteral("symbol")) {
            QString sym = d.content.isEmpty() ? QStringLiteral("★") : d.content;
            if (CardItem::pickSymbol(this, sym, globalPos)) one->setSymbol(sym);
        } else if (id == QStringLiteral("image")) {
            one->chooseImage();
        } else if (id == QStringLiteral("caption")) {
            m_view->setFocus();
            one->beginCaptionEdit();
        } else if (id == QStringLiteral("style")) {
            showNoteStyle(one, globalPos);
        } else if (id == QStringLiteral("stk-flipx")) {
            one->setStickerFlip(!d.flipX, d.flipY);
        } else if (id == QStringLiteral("stk-flipy")) {
            one->setStickerFlip(d.flipX, !d.flipY);
        } else if (id == QStringLiteral("stk-outline")) {
            QMenu menu(this);
            const QList<QPair<QString, QString>> opts = {
                { QString(), tr("Recorte branco") }, { QStringLiteral("none"), tr("Nenhum") },
                { QStringLiteral("shadow"), tr("Só sombra") } };
            for (const auto& [key, label] : opts) {
                QAction* a = menu.addAction(label);
                a->setCheckable(true);
                a->setChecked(d.outline == key);
                a->setData(key);
            }
            if (QAction* chosen = menu.exec(globalPos)) one->setStickerOutline(chosen->data().toString());
        } else if (id == QStringLiteral("stk-layer")) {
            QMenu menu(this);
            QAction* front  = menu.addAction(tr("Trazer pra frente") + QStringLiteral("\tCtrl+]"));
            QAction* back   = menu.addAction(tr("Mandar pra trás") + QStringLiteral("\tCtrl+["));
            QAction* behind = menu.addAction(tr("Atrás dos cards"));
            behind->setCheckable(true);
            behind->setChecked(d.z < 0);
            QAction* chosen = menu.exec(globalPos);
            if (chosen == front)       setStickerLayer(one, QStringLiteral("front"));
            else if (chosen == back)   setStickerLayer(one, QStringLiteral("back"));
            else if (chosen == behind) setStickerLayer(one, d.z < 0 ? QStringLiteral("back") : QStringLiteral("behind"));
        } else if (id == QStringLiteral("stk-lock")) {
            one->setLocked(!d.locked);
        } else if (id == QStringLiteral("flip")) {
            one->toggleImageDesc(!one->isFlipped());
        } else if (id == QStringLiteral("open")) {
            openCard(d);
        }
        scheduleActionBar();
        return;
    }

    const QString connId = m_scene->selectedConnectionId();
    if (!connId.isEmpty()) {
        ConnectionItem* ci = m_scene->findConnection(connId);
        if (!ci) return;
        const CanvasConnection d = ci->connData();
        if (id == QStringLiteral("line-color")) {
            const QColor nc = ColorPopover::getColor(d.color, this, tr("Cor da linha"));
            if (nc.isValid()) updateConnection(connId, [nc](CanvasConnection& c) { c.color = nc; });
        } else if (id == QStringLiteral("line-label")) {
            editConnectionLabel(connId);
        } else if (id == QStringLiteral("line-arrow")) {
            updateConnection(connId, [](CanvasConnection& c) { c.arrow = !c.arrow; });
        } else if (id == QStringLiteral("line-curve")) {
            updateConnection(connId, [](CanvasConnection& c) { c.curved = !c.curved; });
        } else if (id == QStringLiteral("line-delete")) {
            pushUndo();
            m_scene->removeConnection(connId);
            save();
        }
        scheduleActionBar();
        return;
    }

    const QString zoneId = m_scene->selectedZoneId();
    if (!zoneId.isEmpty()) {
        ZoneItem* zi = nullptr;
        for (ZoneItem* z : m_scene->zoneItems()) if (z->zoneData().id == zoneId) zi = z;
        if (!zi) return;
        CanvasZone d = zi->zoneData();
        d.x = zi->pos().x(); d.y = zi->pos().y();
        if (id == QStringLiteral("zone-color")) {
            const QColor nc = ColorPopover::getColor(d.color, this, tr("Cor da área"));
            if (nc.isValid()) { pushUndo(); d.color = nc; zi->setZoneData(d); save(); }
        } else if (id == QStringLiteral("zone-rename")) {
            bool ok = false;
            const QString name = Sheets::askText(this, tr("Nome da área"), tr("Nome da área"),
                d.title, &ok, QString(), false, /*allowEmpty=*/true);
            if (ok) { pushUndo(); d.title = name.trimmed(); zi->setZoneData(d); save(); }
        } else if (id == QStringLiteral("zone-export")) {
            exportZones({d});
        } else if (id == QStringLiteral("zone-delete")) {
            pushUndo();
            m_scene->removeZone(zoneId);
            save();
        }
        scheduleActionBar();
    }
}

void LousaPanel::alignSelection(const QString& how)
{
    QList<CardItem*> sel = m_scene->selectedCardItems();
    if (sel.size() < 2) return;
    pushUndo();
    auto rectOf = [](CardItem* c) { const CanvasCard d = c->cardData(); return QRectF(d.x, d.y, d.width, d.height); };
    if (how == QStringLiteral("align-top")) {
        qreal top = 1e12;
        for (CardItem* c : sel) top = qMin(top, rectOf(c).top());
        for (CardItem* c : sel) c->setPos(c->pos().x(), top);
    } else if (how == QStringLiteral("align-middle")) {
        qreal sum = 0;
        for (CardItem* c : sel) sum += rectOf(c).center().y();
        const qreal mid = sum / sel.size();
        for (CardItem* c : sel) c->setPos(c->pos().x(), mid - rectOf(c).height() / 2.0);
    } else if (how == QStringLiteral("align-left")) {
        qreal left = 1e12;
        for (CardItem* c : sel) left = qMin(left, rectOf(c).left());
        for (CardItem* c : sel) c->setPos(left, c->pos().y());
    } else if (how == QStringLiteral("distribute-h") || how == QStringLiteral("distribute-v")) {
        const bool horiz = (how == QStringLiteral("distribute-h"));
        std::sort(sel.begin(), sel.end(), [&](CardItem* a, CardItem* b) {
            return horiz ? rectOf(a).left() < rectOf(b).left() : rectOf(a).top() < rectOf(b).top();
        });
        qreal total = 0;
        for (CardItem* c : sel) total += horiz ? rectOf(c).width() : rectOf(c).height();
        const QRectF first = rectOf(sel.first()), last = rectOf(sel.last());
        const qreal span = horiz ? (last.right() - first.left()) : (last.bottom() - first.top());
        const qreal gap = qMax(16.0, (span - total) / (sel.size() - 1));
        qreal cursor = horiz ? first.left() : first.top();
        for (CardItem* c : sel) {
            if (horiz) c->setPos(cursor, c->pos().y());
            else       c->setPos(c->pos().x(), cursor);
            cursor += (horiz ? rectOf(c).width() : rectOf(c).height()) + gap;
        }
    } else if (how == QStringLiteral("grid")) {
        std::sort(sel.begin(), sel.end(), [&](CardItem* a, CardItem* b) {
            const QRectF ra = rectOf(a), rb = rectOf(b);
            if (qAbs(ra.top() - rb.top()) > 40) return ra.top() < rb.top();
            return ra.left() < rb.left();
        });
        const int cols = qMax(1, int(std::ceil(std::sqrt(double(sel.size())))));
        qreal cellW = 0, cellH = 0, left = 1e12, top = 1e12;
        for (CardItem* c : sel) {
            const QRectF r = rectOf(c);
            cellW = qMax(cellW, r.width()); cellH = qMax(cellH, r.height());
            left = qMin(left, r.left()); top = qMin(top, r.top());
        }
        for (int i = 0; i < sel.size(); ++i)
            sel[i]->setPos(left + (i % cols) * (cellW + 28), top + (i / cols) * (cellH + 32));
    }
    m_scene->refreshZoneCounts();
    save();
    positionActionBar();
}

// ── Linhas ───────────────────────────────────────────────────────────────────

QList<QColor> LousaPanel::connectionPalette() const
{
    // Paleta do Mira 1; a primeira cor acompanha o fundo (branca no escuro).
    QList<QColor> p = {
        CardItem::boardIsLight() ? QColor(QStringLiteral("#3a3a37")) : QColor(QStringLiteral("#ffffff")),
        QColor(QStringLiteral("#e0483e")), QColor(QStringLiteral("#6ea8fe")),
        QColor(QStringLiteral("#f97316")), QColor(QStringLiteral("#34d399")),
        QColor(QStringLiteral("#f472b6")), QColor(QStringLiteral("#fbbf24")),
        QColor(QStringLiteral("#a78bfa")),
    };
    return p;
}

void LousaPanel::askNewConnection(const QString& fromId, const QString& toId)
{
    if (!m_scene || fromId == toId) return;
    const QList<QColor> palette = connectionPalette();
    QColor selectedColor = palette.first();
    if (!Sheets::askColor(this, tr("Nova conexão"), tr("Cor da conexão"), palette, &selectedColor, tr("Criar")))
        return;
    pushUndo();
    CanvasConnection conn;
    conn.id     = QUuid::createUuid().toString(QUuid::WithoutBraces);
    conn.fromId = fromId;
    conn.toId   = toId;
    conn.color  = selectedColor;
    m_scene->addConnection(conn);
    save();
}

void LousaPanel::updateConnection(const QString& id, const std::function<void(CanvasConnection&)>& change)
{
    ConnectionItem* ci = m_scene ? m_scene->findConnection(id) : nullptr;
    if (!ci) return;
    pushUndo();
    CanvasConnection d = ci->connData();
    change(d);
    ci->setConnData(d);
    save();
    scheduleActionBar();
}

void LousaPanel::editConnectionLabel(const QString& id)
{
    ConnectionItem* ci = m_scene ? m_scene->findConnection(id) : nullptr;
    if (!ci) return;
    bool ok = false;
    const QString name = Sheets::askText(this, tr("Nome da ligação"), tr("irmãos, trai, deve dinheiro…"),
        ci->connData().label, &ok, tr("Salvar"), false, /*allowEmpty=*/true).trimmed();
    if (!ok) return;
    updateConnection(id, [name](CanvasConnection& c) { c.label = name; });
}

void LousaPanel::showConnectionMenu(const QString& id, const QPoint& screenPos)
{
    ConnectionItem* ci = m_scene ? m_scene->findConnection(id) : nullptr;
    if (!ci) return;
    const CanvasConnection d = ci->connData();
    QMenu menu(this);
    QAction* color = menu.addAction(tr("Cor…"));
    QAction* label = menu.addAction(d.label.isEmpty() ? tr("Dar um nome…") : tr("Trocar o nome…"));
    QAction* arrow = menu.addAction(tr("Seta na ponta"));
    arrow->setCheckable(true); arrow->setChecked(d.arrow);
    QAction* curve = menu.addAction(tr("Em curva"));
    curve->setCheckable(true); curve->setChecked(d.curved);
    menu.addSeparator();
    QAction* del = menu.addAction(tr("Remover a linha"));
    QAction* chosen = menu.exec(screenPos);
    if (chosen == color) {
        const QColor nc = ColorPopover::getColor(d.color, this, tr("Cor da linha"));
        if (nc.isValid()) updateConnection(id, [nc](CanvasConnection& c) { c.color = nc; });
    } else if (chosen == label) {
        editConnectionLabel(id);
    } else if (chosen == arrow) {
        updateConnection(id, [](CanvasConnection& c) { c.arrow = !c.arrow; });
    } else if (chosen == curve) {
        updateConnection(id, [](CanvasConnection& c) { c.curved = !c.curved; });
    } else if (chosen == del) {
        pushUndo();
        m_scene->removeConnection(id);
        save();
    }
}

// ── Mapa de áreas ────────────────────────────────────────────────────────────

void LousaPanel::buildMapPanel()
{
    m_mapPanel = new QWidget(this);
    m_mapPanel->setObjectName(QStringLiteral("lousaMapPanel"));
    m_mapPanel->setAttribute(Qt::WA_StyledBackground, true);
    m_mapPanel->setVisible(false);
    auto* vl = new QVBoxLayout(m_mapPanel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);
    auto* title = new QLabel(tr("ÁREAS"), m_mapPanel);
    title->setObjectName(QStringLiteral("lousaMapTitle"));
    vl->addWidget(title);
    m_mapList = new QListWidget(m_mapPanel);
    m_mapList->setObjectName(QStringLiteral("lousaMapList"));
    m_mapList->setIconSize(QSize(12, 12));
    connect(m_mapList, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        const QString id = it->data(Qt::UserRole).toString();
        if (!m_scene || !m_view || id.isEmpty()) return;
        for (const CanvasZone& z : m_scene->allZoneData()) {
            if (z.id != id) continue;
            m_view->fitSceneRect(QRectF(z.x, z.y, z.width, z.height));
            m_scene->selectZone(z.id);
        }
        save();
    });
    vl->addWidget(m_mapList, 1);
}

void LousaPanel::toggleMap()
{
    if (!m_mapPanel) return;
    m_mapOpen = !m_mapOpen;
    if (m_mapOpen && m_stashOpen) toggleStash();
    if (m_mapOpen) {
        refreshMapList();
        positionOverlays();
        m_mapPanel->setVisible(true);
        m_mapPanel->raise();
    } else {
        m_mapPanel->setVisible(false);
    }
}

void LousaPanel::refreshMapList()
{
    if (!m_mapList || !m_scene) return;
    m_mapList->clear();
    for (ZoneItem* zi : m_scene->zoneItems()) {
        const CanvasZone z = zi->zoneData();
        QPixmap dot(24, 24);
        dot.setDevicePixelRatio(2.0);
        dot.fill(Qt::transparent);
        QPainter p(&dot);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(z.color);
        p.drawEllipse(QPointF(6, 6), 5, 5);
        p.end();
        auto* item = new QListWidgetItem(QIcon(dot), z.title.isEmpty() ? tr("(área sem nome)") : z.title);
        item->setData(Qt::UserRole, z.id);
        m_mapList->addItem(item);
    }
    if (m_mapList->count() == 0)
        m_mapList->addItem(tr("Nenhuma área criada"));
}

void LousaPanel::positionOverlays()
{
    if (!m_view) return;
    const QRect va = m_view->geometry();
    constexpr int kM = 14;   // distância das bordas
    if (m_leftBar) { m_leftBar->adjustSize(); m_leftBar->move(va.left() + kM, va.top() + kM); m_leftBar->raise(); }
    if (m_rightBar) {
        m_rightBar->adjustSize();
        m_rightBar->move(va.right() - m_rightBar->width() - kM + 1, va.top() + kM);
        m_rightBar->raise();
    }
    // Embaixo: Guardados e Áreas à esquerda, a Doca no meio, o minimapa à direita.
    int chipsRight = va.left() + kM;
    if (m_stashChip && m_areasChip) {
        m_stashChip->adjustSize();
        m_areasChip->adjustSize();
        const int y = va.bottom() - 36 - kM - 4;
        m_stashChip->move(chipsRight, y);
        chipsRight += m_stashChip->width() + 8;
        m_areasChip->move(chipsRight, y);
        chipsRight += m_areasChip->width();
        m_stashChip->raise();
        m_areasChip->raise();
    }
    const bool hasContent = m_scene && (!m_scene->cardItems().isEmpty() || !m_scene->zoneItems().isEmpty());
    bool miniOn = m_minimap && m_minimapOn && hasContent;
    int miniLeft = miniOn ? va.right() - m_minimap->width() - kM : va.right() - kM;
    if (m_dock) {
        // Sem espaço pros nomes entre os botões e o minimapa: só ícones. Se
        // nem assim couber, o minimapa sai da frente.
        int room = miniLeft - chipsRight - 2 * 16;
        m_dock->setCompact(m_dock->fullWidth() > room);
        m_dock->adjustSize();
        if (miniOn && m_dock->width() > room) {
            miniOn = false;
            miniLeft = va.right() - kM;
        }
        int x = va.center().x() - m_dock->width() / 2;
        x = qBound(chipsRight + 16, x, qMax(chipsRight + 16, miniLeft - 16 - m_dock->width()));
        m_dock->move(x, va.bottom() - m_dock->height() - kM - 4);
        m_dock->raise();
    }
    if (m_minimap) {
        m_minimap->move(va.right() - m_minimap->width() - kM, va.bottom() - m_minimap->height() - kM);
        m_minimap->setVisible(miniOn);
        m_minimap->raise();
    }
    if (m_search && m_search->isVisible()) {
        m_search->adjustSize();
        m_search->move(va.center().x() - m_search->width() / 2, va.top() + kM + 46);
        m_search->raise();
    }
    const int panelBottom = (m_stashChip ? m_stashChip->y() : va.bottom()) - 10;
    if (m_mapPanel && m_mapOpen) {
        const int pw = 260, ph = qMin(300, panelBottom - va.top() - 70);
        m_mapPanel->setGeometry(va.left() + kM, panelBottom - ph, pw, ph);
        m_mapPanel->raise();
    }
    if (m_stashPanel && m_stashOpen) {
        const int pw = 300, ph = qMin(320, panelBottom - va.top() - 70);
        m_stashPanel->setGeometry(va.left() + kM, panelBottom - ph, pw, ph);
        m_stashPanel->raise();
    }
    positionActionBar();
    if (m_cheat && m_cheat->isVisible()) { m_cheat->setGeometry(rect()); m_cheat->raise(); }
}

// ── Fundo e cards ────────────────────────────────────────────────────────────

void LousaPanel::showBoardLook()
{
    if (!m_scene || !m_lookBtn) return;
    LousaBoardLook::Choice cur;
    cur.style = m_scene->boardStyle();
    cur.color = m_scene->canvasColor();
    cur.tilt = CardItem::tiltEnabled();
    cur.minimap = m_minimapOn;
    const QPoint at = m_lookBtn->mapToGlobal(QPoint(m_lookBtn->width() / 2, m_lookBtn->height() + 6));
    // Enquanto a janelinha está aberta, cada escolha muda só o quadro. Reaplicar
    // o estilo da janela inteira a cada clique (applyTheme) redesenhava a Lousa
    // toda por baixo do popup, e ela saía dali sem responder direito ao mouse.
    LousaBoardLook::popup(this, at, cur, [this](const LousaBoardLook::Choice& c) {
        m_scene->setBoardStyle(c.style);
        m_scene->setCanvasColor(c.color);
        m_scene->setTiltEnabled(c.tilt);
        m_minimapOn = c.minimap;
        QSettings st;
        st.setValue(QStringLiteral("lousa/tilt"), c.tilt);
        st.setValue(QStringLiteral("lousa/minimap"), c.minimap);
        positionOverlays();
        save();
    });
    applyLookPrefs();   // uma vez, com o popup já fechado
    reclaimInput();
}

void LousaPanel::reclaimInput()
{
    // Depois de popup dentro de popup (Fundo → Outra cor…), a Lousa volta a ser
    // a janela ativa e ninguém fica segurando o mouse.
    if (QWidget* g = QWidget::mouseGrabber()) g->releaseMouse();
    if (QWidget* k = QWidget::keyboardGrabber()) k->releaseKeyboard();
    if (m_scene)
        if (QGraphicsItem* it = m_scene->mouseGrabberItem()) it->ungrabMouse();
    raise();
    activateWindow();
    if (m_view) m_view->setFocus(Qt::OtherFocusReason);
}


void LousaPanel::applyLookPrefs()
{
    positionOverlays();
    applyTheme();
}

// ── Busca ────────────────────────────────────────────────────────────────────

void LousaPanel::runSearch(const QString& text)
{
    m_searchHits.clear();
    m_searchIndex = -1;
    const QString q = text.trimmed();
    if (q.isEmpty() || !m_scene) { m_search->setResult(-1, -1); return; }
    struct Hit { qreal y, x; int kind; QString id; };
    QVector<Hit> hits;
    for (CardItem* ci : m_scene->cardItems()) {
        const CanvasCard d = ci->cardData();
        QString hay = d.title;
        if (d.type == QStringLiteral("note") || d.type == QStringLiteral("comment") || d.type == QStringLiteral("text"))
            hay += QLatin1Char(' ') + d.content;
        else if (d.type == QStringLiteral("image"))
            hay += QLatin1Char(' ') + d.description;
        if (hay.contains(q, Qt::CaseInsensitive)) hits.append({d.y, d.x, 0, d.id});
    }
    for (const CanvasZone& z : m_scene->allZoneData())
        if (z.title.contains(q, Qt::CaseInsensitive)) hits.append({z.y, z.x, 1, z.id});
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        return qAbs(a.y - b.y) > 30 ? a.y < b.y : a.x < b.x;
    });
    for (const Hit& h : hits) m_searchHits.append({h.kind, h.id});
    if (!m_searchHits.isEmpty()) { m_searchIndex = -1; stepSearch(1); }
    else m_search->setResult(0, 0);
    positionOverlays();
}

void LousaPanel::stepSearch(int delta)
{
    if (m_searchHits.isEmpty()) return;
    m_searchIndex = (m_searchIndex + delta + m_searchHits.size()) % m_searchHits.size();
    const auto& hit = m_searchHits[m_searchIndex];
    if (hit.first == 0) {
        if (CardItem* ci = m_scene->findCard(hit.second)) {
            m_scene->clearAllSelection();
            m_scene->selectOnlyCard(ci);
            m_view->centerOnAnimated(ci->sceneBoundingRect().center());
        }
    } else {
        for (const CanvasZone& z : m_scene->allZoneData())
            if (z.id == hit.second) {
                m_scene->selectZone(z.id);
                m_view->centerOnAnimated(QRectF(z.x, z.y, z.width, z.height).center());
            }
    }
    m_search->setResult(m_searchIndex, m_searchHits.size());
    positionOverlays();
}

// ── Exportar como imagem ─────────────────────────────────────────────────────

void LousaPanel::exportBoardAsImage()
{
    if (!m_scene || (m_scene->cardItems().isEmpty() && m_scene->zoneItems().isEmpty())) {
        QMessageBox::information(this, tr("Exportar como imagem"),
            tr("A lousa está vazia — não há nada para exportar."));
        return;
    }
    // Sem contorno de seleção nem barra de ações na imagem.
    m_scene->clearAllSelection();
    if (m_actionBar) m_actionBar->hide();
    LousaExportSheet sheet(m_scene, m_view, this);
    if (sheet.exec() != QDialog::Accepted) return;
    const QImage image = sheet.render();
    if (image.isNull()) return;
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar como imagem"),
        QStringLiteral("lousa.png"), tr("Imagem PNG (*.png)"));
    if (path.isEmpty()) return;
    if (!image.save(path, "PNG")) {
        QMessageBox::warning(this, tr("Exportar como imagem"),
            tr("Não foi possível salvar a imagem."));
        return;
    }
    QMessageBox::information(this, tr("Exportar como imagem"), tr("Imagem exportada com sucesso."));
}

// ── Templates de layout inicial (só com a lousa vazia) ──────────────────────

void LousaPanel::showTemplatePicker()
{
    if (!m_useTemplateBtn) return;
    QMenu menu(this);
    menu.addAction(tr("Mapa de Personagens"), this, [this]() { applyTemplate(QStringLiteral("characters")); });
    menu.addAction(tr("Arco da História"),    this, [this]() { applyTemplate(QStringLiteral("plot")); });
    menu.addAction(tr("Construção de Mundo"), this, [this]() { applyTemplate(QStringLiteral("world")); });
    menu.exec(m_useTemplateBtn->mapToGlobal(QPoint(0, m_useTemplateBtn->height())));
}

void LousaPanel::applyTemplate(const QString& id)
{
    if (!m_scene) return;
    pushUndo();
    BoardTemplate tpl = buildBoardTemplate(id);
    for (CanvasCard& c : tpl.cards)
        if (c.type == QStringLiteral("text")) c.color = boardInk();   // título legível em qualquer fundo
    for (const CanvasZone& z : tpl.zones)                m_scene->addZone(z);
    for (const CanvasCard& c : tpl.cards)                m_scene->addCard(c);
    for (const CanvasConnection& conn : tpl.connections) m_scene->addConnection(conn);
    refreshEmptyState();
    save();

    if (m_view && !tpl.cards.isEmpty()) {
        QRectF bounds;
        for (const CanvasCard& c : tpl.cards) bounds = bounds.united(QRectF(c.x, c.y, c.width, c.height));
        for (const CanvasZone& z : tpl.zones) bounds = bounds.united(QRectF(z.x, z.y, z.width, z.height));
        m_view->fitSceneRect(bounds.adjusted(-40, -40, 40, 40));
    }
}

// ── Exportação de áreas ──────────────────────────────────────────────────────

static QString lousaCardTitle(const CanvasCard& c)
{
    if (!c.title.trimmed().isEmpty()) return c.title.trimmed();
    QString text = c.content;
    text.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    text = text.simplified();
    const QStringList words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString out;
    for (int i = 0; i < qMin(6, int(words.size())); ++i)
        out += (i ? QStringLiteral(" ") : QString()) + words[i];
    return out.isEmpty() ? QObject::tr("Sem título") : out;
}

static QList<CanvasCard> lousaCardsInZone(const CanvasZone& z, const QList<CanvasCard>& all)
{
    QList<CanvasCard> out;
    const QRectF zr(z.x, z.y, z.width, z.height);
    for (const CanvasCard& c : all) {
        const QRectF cr(c.x, c.y, c.width, c.height);
        if (zr.contains(cr)) out.append(c);
    }
    return out;
}

static QString lousaBuildCardHtml(const CanvasCard& card, const QList<CanvasCard>& all,
                                  const QList<CanvasConnection>& conns)
{
    QString html = card.content.isEmpty() ? QStringLiteral("<p></p>") : card.content;
    // Imagens conectadas diretamente a este card.
    for (const CanvasConnection& conn : conns) {
        if (conn.fromId != card.id && conn.toId != card.id) continue;
        const QString otherId = (conn.fromId == card.id) ? conn.toId : conn.fromId;
        for (const CanvasCard& o : all)
            if (o.id == otherId && o.type == QStringLiteral("image") && !o.content.isEmpty())
                html += QStringLiteral("<p><img src=\"data:image/jpeg;base64,%1\" "
                                       "style=\"max-width:100%;border-radius:6px;\"/></p>").arg(o.content);
    }
    // Conexões anotadas.
    QStringList lines;
    for (const CanvasConnection& conn : conns) {
        if (conn.fromId != card.id && conn.toId != card.id) continue;
        const QString otherId = (conn.fromId == card.id) ? conn.toId : conn.fromId;
        const QString dir = (conn.fromId == card.id) ? QStringLiteral("→") : QStringLiteral("←");
        for (const CanvasCard& o : all)
            if (o.id == otherId && o.type != QStringLiteral("image"))
                lines << QStringLiteral("<li>%1 %2</li>").arg(dir, lousaCardTitle(o).toHtmlEscaped());
    }
    if (!lines.isEmpty())
        html += QStringLiteral("<hr/><p><em>Conexões:</em></p><ul>%1</ul>").arg(lines.join(QString()));
    return html;
}

void LousaPanel::exportZones(const QList<CanvasZone>& zones)
{
    if (!m_projectModel || !m_scene || zones.isEmpty()) return;
    const QList<CanvasCard>       allCards = m_scene->allCardData();
    const QList<CanvasConnection> allConns = m_scene->allConnectionData();

    // Pré-conta: quantas áreas têm conteúdo + se há cards sem título.
    bool hasUntitled = false;
    int  totalExportable = 0;
    int  zonesWithContent = 0;
    for (const CanvasZone& z : zones) {
        int n = 0;
        for (const CanvasCard& c : lousaCardsInZone(z, allCards))
            if (c.type == QStringLiteral("note") || c.type == QStringLiteral("comment")) {
                ++n;
                if (c.title.trimmed().isEmpty()) hasUntitled = true;
            }
        if (n > 0) ++zonesWithContent;
        totalExportable += n;
    }

    if (totalExportable == 0) {
        QMessageBox::information(this, tr("Exportar área"),
            tr("Não há post-its ou comentários dentro da(s) área(s)."));
        return;
    }

    // Confirmação. Cada área vira uma gaveta própria, com o nome da área.
    const QString text = zonesWithContent == 1
        ? tr("A área será exportada para uma gaveta nova, com o nome da área.")
        : tr("Cada área vira uma gaveta nova, com o nome da área "
             "(%1 áreas → %1 gavetas).").arg(zonesWithContent);
    const QString warn = hasUntitled
        ? tr("Os post-its sem título serão nomeados com as primeiras palavras do conteúdo.")
        : QString();
    if (!Sheets::confirm(this, tr("Exportar áreas"), text, warn, tr("Exportar"))) return;

    // Uma gaveta por área (com conteúdo).
    int createdDrawers = 0, totalDocs = 0;
    for (const CanvasZone& z : zones) {
        QList<CanvasCard> inside;
        for (const CanvasCard& c : lousaCardsInZone(z, allCards))
            if (c.type == QStringLiteral("note") || c.type == QStringLiteral("comment"))
                inside.append(c);
        if (inside.isEmpty()) continue;

        Drawer d;
        d.key   = ProjectModel::uid();
        d.title = z.title.trimmed().isEmpty() ? tr("Área") : z.title.trimmed();
        m_projectModel->addDrawer(d);
        ++createdDrawers;

        for (const CanvasCard& c : inside) {
            DrawerItem it;
            it.id            = ProjectModel::uid();
            it.title         = lousaCardTitle(c);
            it.html          = lousaBuildCardHtml(c, allCards, allConns);
            it.hasInlineHtml = true;
            m_projectModel->addDrawerItem(d.key, it);
            ++totalDocs;
        }
    }

    QMessageBox::information(this, tr("Exportar áreas"),
        tr("%1 gaveta(s) criada(s) com %2 documento(s).").arg(createdDrawers).arg(totalDocs));
}

void LousaPanel::exportSelectedZone()
{
    if (!m_scene) return;
    const QString sel = m_scene->selectedZoneId();
    if (sel.isEmpty()) {
        QMessageBox::information(this, tr("Exportar área"),
            tr("Selecione uma área primeiro — clique na barra de topo dela."));
        return;
    }
    for (const CanvasZone& z : m_scene->allZoneData())
        if (z.id == sel) { exportZones({z}); return; }
}


// ── Criar documento a partir de um card ──────────────────────────────────────

static QString lousaHtmlFromPlain(const QString& text)
{
    const QString normalized = QString(text).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    const QStringList paras = normalized.split(QRegularExpression(QStringLiteral("\n{2,}")),
                                               Qt::SkipEmptyParts);
    QString out;
    for (const QString& p : paras) {
        QString chunk = p.trimmed();
        if (chunk.isEmpty()) continue;
        chunk = chunk.toHtmlEscaped();
        chunk.replace(QChar('\n'), QStringLiteral("<br/>"));
        out += QStringLiteral("<p>%1</p>").arg(chunk);
    }
    if (out.isEmpty()) out = QStringLiteral("<p></p>");
    return out;
}

void LousaPanel::createDocFromCard(const CanvasCard& c)
{
    if (!m_projectModel) return;
    if (m_projectModel->drawers().isEmpty()) {
        QMessageBox::warning(this, tr("Criar documento"),
            tr("Crie uma gaveta antes de usar este recurso."));
        return;
    }

    // Título sugerido + corpo do documento conforme o tipo do card.
    QString suggested = c.title.trimmed();
    QString bodyHtml;
    if (c.type == QStringLiteral("image")) {
        if (!c.content.isEmpty())
            bodyHtml = QStringLiteral("<p><img src=\"data:image/jpeg;base64,%1\" "
                                      "style=\"max-width:100%;border-radius:6px;\"/></p>").arg(c.content);
        bodyHtml += lousaHtmlFromPlain(c.description);
        if (suggested.isEmpty()) suggested = c.description.trimmed().left(48);
        if (suggested.isEmpty()) suggested = tr("Imagem");
    } else {
        bodyHtml = lousaHtmlFromPlain(c.content);
        if (suggested.isEmpty()) {
            QString flat = c.content;
            flat.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
            flat = flat.trimmed();
            const QStringList words = flat.split(QChar(' '), Qt::SkipEmptyParts);
            QStringList picked;
            for (int i = 0; i < qMin(8, int(words.size())); ++i) picked << words[i];
            suggested = picked.join(QChar(' '));
        }
        if (suggested.isEmpty()) suggested = tr("Documento");
    }

    QStringList labels, keys, hints;
    for (const Drawer& d : m_projectModel->drawers()) {
        labels << (d.title.isEmpty() ? tr("(sem nome)") : d.title);
        keys << d.key;
        const QString et = d.drawerElementType;
        hints << (et == QStringLiteral("character") ? tr("Vai abrir o cadastro de personagem em seguida (foto e papel).")
                : et == QStringLiteral("setting")   ? tr("Vai abrir o cadastro de cenário em seguida (foto).")
                : et == QStringLiteral("object")    ? tr("Vai abrir o cadastro de objeto em seguida (foto).")
                : QString());
    }
    bool ok = false;
    int pick = 0;
    const QString title = Sheets::askTextWithChoice(this, tr("Criar documento"), tr("Nome do documento"), suggested,
                                                    tr("Gaveta de destino"), labels, &pick, &ok, tr("Criar"), hints).trimmed();
    if (!ok || title.isEmpty() || pick < 0 || pick >= keys.size()) return;
    const QString destKey = keys[pick];

    const Drawer* destDrawer = m_projectModel->findDrawer(destKey);
    const QString et = destDrawer ? destDrawer->drawerElementType : QString();
    const bool isVisual = et == QStringLiteral("character")
                       || et == QStringLiteral("setting")
                       || et == QStringLiteral("object");

    if (isVisual) {
        openElementSheet(et, title, [this, destKey, et, bodyHtml](ElementCreateDialog* dlg) {
            if (!m_projectModel || !m_projectModel->findDrawer(destKey)) return;   // a gaveta sumiu enquanto a folha estava aberta
            const QString finalTitle = dlg->title().trimmed();
            if (finalTitle.isEmpty()) return;
            Element elem;
            elem.name  = finalTitle;
            elem.type  = et;
            elem.icon  = (et == QStringLiteral("character")) ? QStringLiteral("user")
                       : (et == QStringLiteral("setting"))   ? QStringLiteral("map")
                                                             : QStringLiteral("cube");
            elem.role  = dlg->role();
            elem.image = dlg->imageDataUrl();
            elem.narrator = dlg->narrator();
            elem.gender = dlg->gender();
            elem.trackMode = dlg->trackMode();
            elem.aliases = dlg->aliases();
            const QString elementId = m_elementsStore ? m_elementsStore->addElement(elem) : QString();
            DrawerItem it;
            it.id            = ProjectModel::uid();
            it.title         = finalTitle;
            it.hasInlineHtml = true;
            it.html          = bodyHtml;
            it.elementType   = et;
            it.elementId     = elementId;
            it.role          = elem.role;
            m_projectModel->addDrawerItem(destKey, it);
        });
    } else {
        DrawerItem it;
        it.id            = ProjectModel::uid();
        it.title         = title;
        it.hasInlineHtml = true;
        it.html          = bodyHtml;
        m_projectModel->addDrawerItem(destKey, it);
    }
}

// ── Pôster do personagem (e cenário/objeto) ──────────────────────────────────
// Flutuando no canto do quadro, sem modal: dá pra olhar a lousa (e mexer nela)
// enquanto preenche. Uma de cada vez; o resto acontece quando ela é confirmada.

void LousaPanel::openElementSheet(const QString& type, const QString& title,
                                  const std::function<void(ElementCreateDialog*)>& onAccept)
{
    if (m_elementSheet) m_elementSheet->reject();
    auto* dlg = new ElementCreateDialog(type, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    if (!title.isEmpty()) dlg->presetTitle(title);
    m_elementSheet = dlg;
    connect(dlg, &QDialog::accepted, this, [dlg, onAccept]() { onAccept(dlg); });
    dlg->openInside(m_view);
}


// ── Preview flutuante de card ────────────────────────────────────────────────

void LousaPanel::buildCardPreview()
{
    m_cardPreview = new QWidget(this);
    m_cardPreview->setObjectName(QStringLiteral("cardPreview"));
    m_cardPreview->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_cardPreview->hide();

    auto* lay = new QVBoxLayout(m_cardPreview);
    lay->setContentsMargins(10, 8, 10, 10);
    lay->setSpacing(5);

    m_previewTitle = new QLabel(m_cardPreview);
    m_previewTitle->setObjectName(QStringLiteral("cardPreviewTitle"));
    m_previewTitle->setWordWrap(true);
    m_previewTitle->hide();
    lay->addWidget(m_previewTitle);

    m_previewDivider = new QWidget(m_cardPreview);
    m_previewDivider->setObjectName(QStringLiteral("cardPreviewDivider"));
    m_previewDivider->setFixedHeight(1);
    m_previewDivider->hide();
    lay->addWidget(m_previewDivider);

    m_previewBody = new QTextEdit(m_cardPreview);
    m_previewBody->setObjectName(QStringLiteral("cardPreviewBody"));
    m_previewBody->setReadOnly(true);
    m_previewBody->setFrameShape(QFrame::NoFrame);
    m_previewBody->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_previewBody->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_previewBody->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    lay->addWidget(m_previewBody);

    applyTheme();   // estiliza o preview junto com o resto
}

void LousaPanel::showCardPreview(const CanvasCard& data, const QPoint& screenPos)
{
    if (!m_cardPreview) return;

    const QString type = data.type;

    // Monta título
    const QString title = data.title.trimmed();
    if (!title.isEmpty()) {
        m_previewTitle->setText(title);
        m_previewTitle->show();
        m_previewDivider->setVisible(!data.content.isEmpty() || !data.description.isEmpty());
    } else {
        m_previewTitle->hide();
        m_previewDivider->hide();
    }

    // Monta corpo
    QString body;
    if (type == QStringLiteral("note") || type == QStringLiteral("comment") ||
        type == QStringLiteral("text")) {
        body = data.content;
    } else if (type == QStringLiteral("image")) {
        body = data.description;
    }

    if (body.trimmed().isEmpty() && title.isEmpty()) {
        hideCardPreview();
        return;
    }

    m_previewBody->setVisible(!body.trimmed().isEmpty());
    if (!body.isEmpty()) {
        if (body.contains(QLatin1Char('<')))
            m_previewBody->setHtml(body);
        else
            m_previewBody->setPlainText(body);
    }

    // Dimensiona o widget
    // Dimensiona pelo conteúdo real, não por um máximo fixo.
    constexpr int kMaxW = 240;
    m_cardPreview->setFixedWidth(kMaxW);
    m_previewBody->document()->setTextWidth(kMaxW - 20);  // conta padding
    const int docH = qMin(static_cast<int>(m_previewBody->document()->size().height()) + 4, 120);
    m_previewBody->setFixedHeight(docH);
    m_cardPreview->setMaximumHeight(200);
    m_cardPreview->adjustSize();

    // Posiciona: converte screen → painel, depois ajusta pra não sair da borda
    QPoint pos = mapFromGlobal(screenPos) + QPoint(14, 14);
    const int pw = m_cardPreview->width();
    const int ph = m_cardPreview->height();
    if (pos.x() + pw > width()  - 8) pos.setX(width()  - pw - 8);
    if (pos.y() + ph > height() - 8) pos.setY(height() - ph - 8);
    if (pos.x() < 8) pos.setX(8);
    if (pos.y() < 8) pos.setY(8);

    m_cardPreview->move(pos);
    m_cardPreview->raise();
    m_cardPreview->show();
}

void LousaPanel::hideCardPreview()
{
    if (m_cardPreview) m_cardPreview->hide();
}


// ── Undo / Redo ──────────────────────────────────────────────────────────────

LousaPanel::BoardState LousaPanel::captureState() const
{
    BoardState s;
    if (!m_scene) return s;
    s.cards       = m_scene->allCardData();
    s.connections = m_scene->allConnectionData();
    s.zones       = m_scene->allZoneData();
    // Tira o conteúdo pesado de imagem dos snapshots (guarda à parte por id).
    for (CanvasCard& c : s.cards) {
        if (c.type == QStringLiteral("image") && !c.content.isEmpty()) {
            const_cast<LousaPanel*>(this)->m_contentStore.insert(c.id, c.content);
            c.content.clear();
        }
    }
    return s;
}

void LousaPanel::applyState(const BoardState& s)
{
    if (!m_scene || !m_view) return;
    m_loading = true;
    m_scene->clearCards();
    m_scene->clearConnections();
    m_scene->clearZones();
    for (CanvasCard c : s.cards) {
        // Texto livre recém-criado e ainda vazio não volta no desfazer.
        if (c.type == QStringLiteral("text") && c.content.trimmed().isEmpty()) continue;
        if (c.type == QStringLiteral("image") && c.content.isEmpty()
            && m_contentStore.contains(c.id))
            c.content = m_contentStore.value(c.id);
        m_scene->addCard(c);
    }
    for (const CanvasConnection& conn : s.connections) m_scene->addConnection(conn);
    for (const CanvasZone& z : s.zones)                m_scene->addZone(z);
    m_loading = false;
    refreshDocCards();
    m_scene->refreshZoneCounts();
    save();
    refreshEmptyState();
    scheduleTrayRefresh();
}

void LousaPanel::pushUndo()
{
    if (m_loading) return;
    m_undo.append(captureState());
    while (m_undo.size() > 50) m_undo.removeFirst();
    m_redo.clear();
    refreshUndoButtons();
}

void LousaPanel::undo()
{
    if (m_undo.isEmpty()) return;
    m_redo.append(captureState());
    const BoardState s = m_undo.takeLast();
    applyState(s);
    refreshUndoButtons();
}

void LousaPanel::redo()
{
    if (m_redo.isEmpty()) return;
    m_undo.append(captureState());
    const BoardState s = m_redo.takeLast();
    applyState(s);
    refreshUndoButtons();
}


// ── Teclado ──────────────────────────────────────────────────────────────────

void LousaPanel::refreshEmptyState()
{
    if (!m_emptyStateBox || !m_scene || !m_view) return;
    const bool empty = m_scene->cardItems().isEmpty() && m_scene->zoneItems().isEmpty();
    if (m_emptyStateBox->isVisible() != empty) m_emptyStateBox->setVisible(empty);
    if (empty) {
        m_emptyStateBox->adjustSize();
        const QRect va = m_view->geometry();
        m_emptyStateBox->move(va.center().x() - m_emptyStateBox->width() / 2,
                              va.center().y() - m_emptyStateBox->height() / 2);
        m_emptyStateBox->raise();
    }
    if (m_minimap) {
        const bool show = m_minimapOn && !empty;
        if (m_minimap->isVisible() != show) m_minimap->setVisible(show);
    }
}

void LousaPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    QTimer::singleShot(0, this, [this]() {
        refreshEmptyState();
        positionOverlays();
    });
}

void LousaPanel::closeEvent(QCloseEvent* event)
{
    // Esconde em vez de destruir — o estado fica vivo entre aberturas.
    hide();
    event->ignore();
    emit closeRequested();
}

void LousaPanel::keyPressEvent(QKeyEvent* event)
{
    const auto mods  = event->modifiers();
    const bool ctrl  = mods & Qt::ControlModifier;
    const bool shift = mods & Qt::ShiftModifier;
    const bool alt   = mods & Qt::AltModifier;
    const int  key   = event->key();

    // Desfazer / refazer
    if (ctrl && !shift && !alt && key == Qt::Key_Z) { undo(); event->accept(); return; }
    if (ctrl && !shift && !alt && key == Qt::Key_Y) { redo(); event->accept(); return; }

    // Delete = guardar selecionados na gaveta; Shift+Delete = apagar de vez
    if ((key == Qt::Key_Delete || key == Qt::Key_Backspace) && m_scene) {
        if (!m_scene->selectedCardItems().isEmpty()) {
            if (shift) deleteSelectedCards();
            else       stashSelectedCards();
            event->accept(); return;
        }
        if (!m_scene->selectedConnectionId().isEmpty()) {
            pushUndo();
            m_scene->removeConnection(m_scene->selectedConnectionId());
            save();
            event->accept(); return;
        }
        if (!m_scene->selectedZoneId().isEmpty()) {
            pushUndo();
            m_scene->removeZone(m_scene->selectedZoneId());
            save();
            event->accept(); return;
        }
    }

    // Criar (mesmo fluxo da Doca)
    if (ctrl && !shift && !alt && key == Qt::Key_N) { createFromTool(QStringLiteral("postit")); event->accept(); return; }
    if (ctrl && !shift && !alt && key == Qt::Key_T) { createFromTool(QStringLiteral("text")); event->accept(); return; }
    if (!ctrl && !shift && alt && key == Qt::Key_C) { createFromTool(QStringLiteral("comment")); event->accept(); return; }
    if (ctrl && !shift && !alt && key == Qt::Key_H) { pickDocForBoard(); event->accept(); return; }
    if (ctrl && !shift && !alt && key == Qt::Key_G) { createImage(); event->accept(); return; }
    if (ctrl && shift  && !alt && key == Qt::Key_C) { pickCharacterForBoard(); event->accept(); return; }

    // Buscar / ver tudo / atalhos
    if (ctrl && !shift && !alt && key == Qt::Key_F) {
        if (m_search) { m_search->open(); positionOverlays(); }
        event->accept(); return;
    }
    if (ctrl && !alt && key == Qt::Key_0) {
        const QRectF b = m_scene ? m_scene->contentBounds() : QRectF();
        if (b.isValid()) m_view->fitSceneRect(b);
        event->accept(); return;
    }
    if (key == Qt::Key_Question) {
        if (m_cheat) m_cheat->showOver();
        event->accept(); return;
    }

    // Ctrl+X: 1 card selecionado → recorta (fica translúcido); sem seleção → desenhar área.
    if (ctrl && !shift && !alt && key == Qt::Key_X) {
        const QList<CardItem*> sel = m_scene ? m_scene->selectedCardItems()
                                             : QList<CardItem*>();
        if (sel.size() == 1) {
            if (!m_cutCardId.isEmpty())
                if (CardItem* prev = m_scene->findCard(m_cutCardId)) prev->setOpacity(1.0);
            m_cutCardId = sel.first()->cardData().id;
            sel.first()->setOpacity(0.4);
            event->accept(); return;
        }
        if (sel.isEmpty()) {
            createFromTool(QStringLiteral("area"));
            event->accept(); return;
        }
    }

    // Ctrl+V: cola o card recortado na posição do mouse.
    if (ctrl && !shift && !alt && key == Qt::Key_V && !m_cutCardId.isEmpty() && m_scene && m_view) {
        if (CardItem* c = m_scene->findCard(m_cutCardId)) {
            pushUndo();
            const QPointF sp = m_view->mapToScene(m_view->mapFromGlobal(QCursor::pos()));
            const CanvasCard d = c->cardData();
            c->setPos(sp.x() - d.width / 2.0, sp.y() - d.height / 2.0);
            c->setOpacity(1.0);
            m_scene->refreshZoneCounts();
            save();
        }
        m_cutCardId.clear();
        event->accept(); return;
    }

    // Ctrl+V com imagem copiada: PNG transparente vira adesivo; o resto, card de imagem.
    if (ctrl && !shift && !alt && key == Qt::Key_V && m_cutCardId.isEmpty()) {
        if (pasteImageFromClipboard()) { event->accept(); return; }
    }

    // Ctrl+] / Ctrl+[: adesivo pra frente / pra trás
    if (ctrl && !alt && (key == Qt::Key_BracketRight || key == Qt::Key_BracketLeft) && m_scene) {
        const QList<CardItem*> sel = m_scene->selectedCardItems();
        if (sel.size() == 1 && sel.first()->isSticker()) {
            setStickerLayer(sel.first(), key == Qt::Key_BracketRight ? QStringLiteral("front") : QStringLiteral("back"));
            event->accept(); return;
        }
    }

    if (key == Qt::Key_Escape) {
        if (m_cheat && m_cheat->isVisible()) { m_cheat->hide(); event->accept(); return; }
        if (m_search && m_search->isVisible()) { m_search->hide(); event->accept(); return; }
        if (!m_cutCardId.isEmpty()) {
            if (CardItem* c = m_scene->findCard(m_cutCardId)) c->setOpacity(1.0);
            m_cutCardId.clear();
            event->accept(); return;
        }
        if (m_view && m_view->isPlanMode()) {
            m_view->setPlanMode(false);
            m_dock->setToolChecked(QStringLiteral("area"), false);
            event->accept(); return;
        }
        if (m_view && m_view->isConnectMode()) { m_view->setConnectMode(false); event->accept(); return; }
        if (m_mapOpen) { toggleMap(); event->accept(); return; }
        if (m_scene) m_scene->clearAllSelection();
        event->accept(); return;
    }

    // Pincel: Shift+S segurado seleciona os cards que o mouse tocar.
    if (shift && key == Qt::Key_S) {
        if (m_view) m_view->setBrushMode(true);
        event->accept(); return;
    }

    // F: abre/fecha a lista de áreas.
    if (!ctrl && !shift && !alt && key == Qt::Key_F) {
        toggleMap();
        event->accept(); return;
    }

    // Ctrl+D: exporta a área selecionada (ou a que está sob o mouse).
    // Ctrl+Shift+D: exporta todas — cada área para a sua própria gaveta.
    if (ctrl && !alt && key == Qt::Key_D && m_scene) {
        const QList<CanvasZone> all = m_scene->allZoneData();
        if (shift) {
            if (!all.isEmpty()) exportZones(all);
        } else {
            CanvasZone target;
            bool found = false;
            const QString sel = m_scene->selectedZoneId();
            if (!sel.isEmpty())
                for (const CanvasZone& z : all)
                    if (z.id == sel) { target = z; found = true; break; }
            if (!found && m_view) {
                const QPointF sp = m_view->mapToScene(m_view->mapFromGlobal(QCursor::pos()));
                qreal bestArea = -1.0;
                for (const CanvasZone& z : all) {
                    if (QRectF(z.x, z.y, z.width, z.height).contains(sp)) {
                        const qreal a = z.width * z.height;
                        if (bestArea < 0 || a < bestArea) { bestArea = a; target = z; found = true; }
                    }
                }
            }
            if (found) exportZones({target});
        }
        event->accept(); return;
    }

    QWidget::keyPressEvent(event);
}

void LousaPanel::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_S || event->key() == Qt::Key_Shift) {
        if (m_view) m_view->setBrushMode(false);
    }
    QWidget::keyReleaseEvent(event);
}

// ── Projeto ──────────────────────────────────────────────────────────────────

void LousaPanel::setProjectModel(ProjectModel* model)
{
    if (m_elementSheet) m_elementSheet->reject();
    if (m_projectModel) disconnect(m_projectModel, nullptr, this, nullptr);
    m_projectModel = model;
    if (m_projectModel) {
        connect(m_projectModel, &ProjectModel::drawersChanged, this, &LousaPanel::scheduleProjectRefresh);
        connect(m_projectModel, &ProjectModel::chaptersChanged, this, &LousaPanel::scheduleProjectRefresh);
        connect(m_projectModel, &ProjectModel::activeManuscriptChanged, this, &LousaPanel::scheduleProjectRefresh);
    }
    scheduleTrayRefresh();
}

void LousaPanel::setElementsStore(ElementsStore* store)
{
    if (m_elementsStore) disconnect(m_elementsStore, nullptr, this, nullptr);
    m_elementsStore = store;
    if (m_elementsStore) {
        // Foto ou papel do personagem mudou: os polaroids acompanham.
        connect(m_elementsStore, &ElementsStore::changed, this, &LousaPanel::scheduleProjectRefresh);
    }
    scheduleTrayRefresh();
}

void LousaPanel::scheduleProjectRefresh()
{
    // Fechada, a Lousa só anota que o projeto mudou; atualiza ao abrir.
    if (!isVisible()) { m_projectDirty = true; return; }
    m_projectDirty = false;
    scheduleTrayRefresh();
    QTimer::singleShot(0, this, [this]() { refreshDocCards(); });
}

void LousaPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_projectDirty) scheduleProjectRefresh();
    QTimer::singleShot(0, this, [this]() { positionOverlays(); refreshEmptyState(); });
}

void LousaPanel::refreshDocCards()
{
    if (!m_scene || !m_projectModel) return;
    for (CardItem* item : m_scene->cardItems()) {
        const CanvasCard d = item->cardData();
        if (d.type == QStringLiteral("doc")) {
            QString drawerKey;
            const DrawerItem* di = m_projectModel->findDrawerItem(d.linkedItemId, &drawerKey);
            if (!di) { item->setLinkedMeta(tr("Documento não encontrado"), QString()); continue; }
            QString html = htmlFor(DocCache::itemKey(di->id));
            if (html.isEmpty()) html = di->html;
            item->setLinkedHtml(html);
            item->setLinkedTitle(di->title);
            const Drawer* dr = m_projectModel->findDrawer(drawerKey);
            item->setLinkedMeta(dr ? dr->title : QString(), wordsLabel(wordCount(html)));
        } else if (d.type == QStringLiteral("character")) {
            const DrawerItem* di = m_projectModel->findDrawerItem(d.linkedItemId);
            if (!di) continue;
            QString html = htmlFor(DocCache::itemKey(di->id));
            if (html.isEmpty()) html = di->html;
            item->setLinkedHtml(html);
            item->setLinkedTitle(di->title);
            if (m_elementsStore && !di->elementId.isEmpty()) {
                if (const Element* el = m_elementsStore->findElement(di->elementId)) {
                    if (el->image != d.photoDataUrl) item->setCharacterPhoto(el->image);
                    item->setRoleLabel(RoleTiers::roleDisplayName(el->role));
                }
            }
        } else if (d.type == QStringLiteral("chapter")) {
            const Chapter* ch = m_projectModel->findChapter(d.linkedItemId);
            if (!ch) { item->setLinkedMeta(tr("Capítulo não encontrado"), QString()); continue; }
            const QString html = htmlFor(DocCache::chapterKey(ch->manuscriptId, ch->id));
            // No quadro aparece o resumo; sem resumo, o começo do capítulo.
            item->setLinkedHtml(ch->summary.trimmed().isEmpty() ? html : ch->summary);
            item->setLinkedTitle(ch->title);
            item->setLinkedMeta(chapterLabel(m_projectModel, ch), wordsLabel(wordCount(html)));
        }
    }
}

void LousaPanel::setProjectRoot(const QString& root)
{
    if (m_projectRoot == root) return;
    // pôster aberto no quadro é do projeto anterior
    if (m_elementSheet) m_elementSheet->reject();
    m_projectRoot = root;
    loadBoardsManifest();
    m_undo.clear();
    m_redo.clear();
    refreshUndoButtons();
    load();
    rebuildTabs();
}

static CanvasCard cardFromJson(const QJsonObject& o)
{
    CanvasCard c;
    c.id      = o.value(QStringLiteral("id")).toString();
    c.type    = o.value(QStringLiteral("type")).toString(QStringLiteral("note"));
    c.x       = o.value(QStringLiteral("x")).toDouble();
    c.y       = o.value(QStringLiteral("y")).toDouble();
    c.width   = o.value(QStringLiteral("width")).toDouble(200);
    c.height  = o.value(QStringLiteral("height")).toDouble(160);
    c.title   = o.value(QStringLiteral("title")).toString();
    c.content = o.value(QStringLiteral("content")).toString();
    c.color   = QColor(o.value(QStringLiteral("color")).toString(QStringLiteral("#ffd060")));
    if (!c.color.isValid()) c.color = QColor(QStringLiteral("#ffd060"));
    c.description     = o.value(QStringLiteral("description")).toString();
    c.photoDataUrl    = o.value(QStringLiteral("photoDataUrl")).toString();
    c.linkedItemId    = o.value(QStringLiteral("linkedItemId")).toString();
    c.linkedDrawerKey = o.value(QStringLiteral("linkedDrawerName")).toString();
    c.fontSize        = o.value(QStringLiteral("fontSize")).toInt(0);
    c.bold            = o.value(QStringLiteral("bold")).toBool(false);
    c.italic          = o.value(QStringLiteral("italic")).toBool(false);
    c.rotation        = o.value(QStringLiteral("rotation")).toDouble(0.0);
    c.fontFamily      = o.value(QStringLiteral("fontFamily")).toString();
    c.wrapWidth       = o.value(QStringLiteral("wrapWidth")).toDouble(0.0);
    c.shape           = o.value(QStringLiteral("shape")).toString();
    c.fastener        = o.value(QStringLiteral("fastener")).toString();
    c.frame           = o.value(QStringLiteral("frame")).toString();
    c.flipX           = o.value(QStringLiteral("flipX")).toBool(false);
    c.flipY           = o.value(QStringLiteral("flipY")).toBool(false);
    c.outline         = o.value(QStringLiteral("outline")).toString();
    c.baseWidth       = o.value(QStringLiteral("baseWidth")).toDouble(0.0);
    c.z               = o.value(QStringLiteral("z")).toDouble(1.0);
    c.locked          = o.value(QStringLiteral("locked")).toBool(false);
    return c;
}

static QJsonObject cardToJson(const CanvasCard& c)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"),              c.id);
    o.insert(QStringLiteral("type"),            c.type);
    o.insert(QStringLiteral("x"),               c.x);
    o.insert(QStringLiteral("y"),               c.y);
    o.insert(QStringLiteral("width"),           c.width);
    o.insert(QStringLiteral("height"),          c.height);
    o.insert(QStringLiteral("title"),           c.title);
    // Documento, capítulo e personagem: o texto vem do projeto ao abrir.
    const bool linked = c.type == QStringLiteral("doc") || c.type == QStringLiteral("chapter")
                     || c.type == QStringLiteral("character");
    o.insert(QStringLiteral("content"),         linked ? QString() : c.content);
    o.insert(QStringLiteral("color"),           c.color.name());
    o.insert(QStringLiteral("description"),   c.description);
    o.insert(QStringLiteral("photoDataUrl"), c.photoDataUrl);
    o.insert(QStringLiteral("linkedItemId"),    c.linkedItemId);
    o.insert(QStringLiteral("linkedDrawerName"), c.linkedDrawerKey);
    if (c.fontSize > 0) o.insert(QStringLiteral("fontSize"), c.fontSize);
    if (c.bold)         o.insert(QStringLiteral("bold"),   true);
    if (c.italic)       o.insert(QStringLiteral("italic"), true);
    if (!qFuzzyIsNull(c.rotation)) o.insert(QStringLiteral("rotation"), c.rotation);
    if (!c.fontFamily.isEmpty()) o.insert(QStringLiteral("fontFamily"), c.fontFamily);
    if (c.wrapWidth > 0) o.insert(QStringLiteral("wrapWidth"), c.wrapWidth);
    if (!c.shape.isEmpty())    o.insert(QStringLiteral("shape"), c.shape);
    if (!c.fastener.isEmpty()) o.insert(QStringLiteral("fastener"), c.fastener);
    if (!c.frame.isEmpty())    o.insert(QStringLiteral("frame"), c.frame);
    if (c.type == QStringLiteral("sticker")) {
        if (c.flipX) o.insert(QStringLiteral("flipX"), true);
        if (c.flipY) o.insert(QStringLiteral("flipY"), true);
        if (!c.outline.isEmpty()) o.insert(QStringLiteral("outline"), c.outline);
        o.insert(QStringLiteral("baseWidth"), c.baseWidth);
        o.insert(QStringLiteral("z"), c.z);
    }
    if (c.locked) o.insert(QStringLiteral("locked"), true);
    return o;
}

void LousaPanel::load()
{
    if (m_projectRoot.isEmpty() || !m_scene || !m_view) return;
    const QString path = QDir::cleanPath(m_projectRoot + QStringLiteral("/") + activeBoardFile());
    QJsonObject root;
    QFile f(path);
    // Board novo, ainda sem arquivo: segue com `root` vazio — os valores
    // default produzem uma lousa em branco, e é preciso mesmo assim limpar a
    // cena (pode estar vindo de outro board com conteúdo).
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
    }

    // Fundo: antes do rework a cor era sempre gravada, e #1a1a2e era a padrão
    // de então. Quem nunca escolheu cor passa a ter a cor da mesa do tema.
    QColor color(root.value(QStringLiteral("canvasColor")).toString());
    const bool custom = root.contains(QStringLiteral("canvasColorCustom"))
        ? root.value(QStringLiteral("canvasColorCustom")).toBool(false)
        : (color.isValid() && color != QColor(QStringLiteral("#1a1a2e")));
    m_scene->setBoardStyle(root.value(QStringLiteral("canvasStyle")).toString(QStringLiteral("dots")));
    m_scene->setCanvasColor(custom ? color : QColor());

    const qreal zoom = root.value(QStringLiteral("zoom")).toDouble(1.0);
    const qreal panX = root.value(QStringLiteral("panX")).toDouble(0.0);
    const qreal panY = root.value(QStringLiteral("panY")).toDouble(0.0);
    m_view->applyZoomAndPan(zoom, panX, panY);
    if (m_zoomLabel) m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(m_view->zoomFactor() * 100)));
    m_scene->setViewZoom(m_view->zoomFactor());

    m_loading = true;
    m_scene->clearCards();
    m_scene->clearConnections();
    m_scene->clearZones();
    for (const auto& v : root.value(QStringLiteral("zones")).toArray()) {
        const QJsonObject o = v.toObject();
        CanvasZone z;
        z.id     = o.value(QStringLiteral("id")).toString();
        z.x      = o.value(QStringLiteral("x")).toDouble();
        z.y      = o.value(QStringLiteral("y")).toDouble();
        z.width  = o.value(QStringLiteral("width")).toDouble(300);
        z.height = o.value(QStringLiteral("height")).toDouble(200);
        z.title  = o.value(QStringLiteral("title")).toString();
        z.color  = QColor(o.value(QStringLiteral("color")).toString(QStringLiteral("#6ea8fe")));
        if (!z.id.isEmpty()) m_scene->addZone(z);
    }
    for (const auto& v : root.value(QStringLiteral("cards")).toArray()) {
        const CanvasCard c = cardFromJson(v.toObject());
        if (!c.id.isEmpty()) m_scene->addCard(c);
    }
    for (const auto& v : root.value(QStringLiteral("connections")).toArray()) {
        const QJsonObject o = v.toObject();
        CanvasConnection conn;
        conn.id     = o.value(QStringLiteral("id")).toString();
        conn.fromId = o.value(QStringLiteral("fromId")).toString();
        conn.toId   = o.value(QStringLiteral("toId")).toString();
        conn.color  = QColor(o.value(QStringLiteral("color")).toString(QStringLiteral("#ffffff")));
        for (const auto& wv : o.value(QStringLiteral("waypointCardIds")).toArray())
            conn.waypointCardIds.append(wv.toString());
        conn.label  = o.value(QStringLiteral("label")).toString();
        conn.arrow  = o.value(QStringLiteral("arrow")).toBool(true);
        conn.curved = o.value(QStringLiteral("curved")).toBool(false);
        if (!conn.id.isEmpty() && !conn.fromId.isEmpty() && !conn.toId.isEmpty())
            m_scene->addConnection(conn);
    }
    m_loading = false;
    m_stash.clear();
    for (const auto& v : root.value(QStringLiteral("stash")).toArray()) {
        const CanvasCard c = cardFromJson(v.toObject());
        if (!c.id.isEmpty()) m_stash.append(c);
    }
    m_scene->refreshZoneCounts();
    // Re-busca HTML e foto dos cards vinculados ao projeto
    QTimer::singleShot(0, this, [this]() { refreshDocCards(); });
    scheduleTrayRefresh();
    refreshEmptyState();
    positionOverlays();
    if (m_minimap) m_minimap->scheduleRefresh();
}

void LousaPanel::save() const
{
    if (m_projectRoot.isEmpty() || !m_scene || !m_view || m_loading) return;

    QJsonArray cards;
    for (const CanvasCard& c : m_scene->allCardData())
        cards.append(cardToJson(c));

    QJsonObject root;
    if (m_scene->canvasColor().isValid()) {
        root.insert(QStringLiteral("canvasColor"), m_scene->canvasColor().name());
        root.insert(QStringLiteral("canvasColorCustom"), true);
    } else {
        root.insert(QStringLiteral("canvasColorCustom"), false);
    }
    root.insert(QStringLiteral("canvasStyle"), m_scene->boardStyle());
    root.insert(QStringLiteral("zoom"), m_view->zoomFactor());
    root.insert(QStringLiteral("panX"), m_view->scrollPos().x());
    root.insert(QStringLiteral("panY"), m_view->scrollPos().y());
    root.insert(QStringLiteral("cards"),       cards);
    QJsonArray connections;
    for (const CanvasConnection& conn : m_scene->allConnectionData()) {
        QJsonObject o;
        o.insert(QStringLiteral("id"),     conn.id);
        o.insert(QStringLiteral("fromId"), conn.fromId);
        o.insert(QStringLiteral("toId"),   conn.toId);
        o.insert(QStringLiteral("color"),  conn.color.name());
        QJsonArray wpts;
        for (const QString& wid : conn.waypointCardIds) wpts.append(wid);
        o.insert(QStringLiteral("waypointCardIds"), wpts);
        if (!conn.label.isEmpty()) o.insert(QStringLiteral("label"), conn.label);
        if (!conn.arrow)           o.insert(QStringLiteral("arrow"), false);
        if (conn.curved)           o.insert(QStringLiteral("curved"), true);
        connections.append(o);
    }
    root.insert(QStringLiteral("connections"), connections);
    QJsonArray zones;
    for (const CanvasZone& z : m_scene->allZoneData()) {
        QJsonObject o;
        o.insert(QStringLiteral("id"),     z.id);
        o.insert(QStringLiteral("x"),      z.x);
        o.insert(QStringLiteral("y"),      z.y);
        o.insert(QStringLiteral("width"),  z.width);
        o.insert(QStringLiteral("height"), z.height);
        o.insert(QStringLiteral("title"),  z.title);
        o.insert(QStringLiteral("color"),  z.color.name());
        zones.append(o);
    }
    root.insert(QStringLiteral("zones"), zones);

    QJsonArray stash;
    for (const CanvasCard& c : m_stash) stash.append(cardToJson(c));
    root.insert(QStringLiteral("stash"), stash);

    QSaveFile f(QDir::cleanPath(m_projectRoot + QStringLiteral("/") + activeBoardFile()));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.commit();
}

void LousaPanel::refreshUndoButtons()
{
    if (m_undoBtn) m_undoBtn->setEnabled(!m_undo.isEmpty());
    if (m_redoBtn) m_redoBtn->setEnabled(!m_redo.isEmpty());
}

CanvasCard LousaPanel::nextCardData(const QString& type) const
{
    const QPointF center = viewCenter();

    CanvasCard c;
    c.id   = QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.type = type;

    if (type == QStringLiteral("comment")) {
        c.width  = 220; c.height = 130;
        c.color  = QColor(QStringLiteral("#b49cf5"));
    } else if (type == QStringLiteral("image")) {
        c.width  = 240; c.height = 220;
        c.color  = QColor(QStringLiteral("#34d399"));
    } else if (type == QStringLiteral("doc")) {
        c.width  = 240; c.height = 200;
        c.color  = QColor(QStringLiteral("#60a5fa"));
    } else if (type == QStringLiteral("chapter")) {
        c.width  = 240; c.height = 190;
        c.color  = QColor(QStringLiteral("#d97757"));
    } else if (type == QStringLiteral("character")) {
        c.width  = 150; c.height = 196;
        c.color  = QColor(QStringLiteral("#f97316"));
    } else if (type == QStringLiteral("text")) {
        c.width  = 300; c.height = 40;
        c.color  = boardInk();
        c.fontSize = 22;
        c.wrapWidth = 300;
    } else if (type == QStringLiteral("symbol")) {
        c.width  = 100; c.height = 100;
        c.color  = QColor(QStringLiteral("#fbbf24"));
        c.fontSize = 60;
        c.content  = QStringLiteral("★");
    } else if (type == QStringLiteral("sticker")) {
        c.width  = 160; c.height = 160;
        c.baseWidth = 160;
    } else { // note (default) — com o estilo de "Usar nos próximos post-its"
        QSettings st;
        c.shape    = st.value(QStringLiteral("lousa/noteShape")).toString();
        c.fastener = st.value(QStringLiteral("lousa/noteFastener")).toString();
        c.frame    = st.value(QStringLiteral("lousa/noteFrame")).toString();
        const QSizeF sz = CardItem::defaultNoteSize(c.shape, QStringLiteral("note"));
        c.width  = sz.width(); c.height = sz.height();
        const QColor saved(st.value(QStringLiteral("lousa/noteColor")).toString());
        c.color  = saved.isValid() ? saved : QColor(QStringLiteral("#f6d06a"));
    }
    c.x = center.x() - c.width  / 2.0;
    c.y = center.y() - c.height / 2.0;
    return c;
}

void LousaPanel::reloadIcons()
{
    for (const auto& [btn, path] : m_iconBindings)
        btn->setIcon(loadLousaIcon(path));
    if (m_boardPill) m_boardPill->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/lousa/layers.svg"),
        QColor(Theme::accentDefault()), QColor(Theme::accentDefault()), QColor(Theme::accentDefault())));
}

void LousaPanel::applyTheme()
{
    if (m_scene) m_scene->refreshBoardLook();   // fundo "do tema" acompanha a troca
    const QColor board = m_scene ? m_scene->effectiveCanvasColor() : QColor(Theme::appBackground());
    const QColor a(Theme::accentDefault());
    const QString accentSoft = QStringLiteral("rgba(%1,%2,%3,0.18)").arg(a.red()).arg(a.green()).arg(a.blue());
    setStyleSheet(Theme::qss(QStringLiteral(
        "QWidget#lousaPanel   { background: %8; }"
        "QFrame[lousaFloat=\"true\"] { background: %1; border: 1px solid %2; border-radius: 10px; }"
        "QToolButton#lousaTopBtn {"
        "  background: transparent; border: none; border-radius: @radius-control; }"
        "QToolButton#lousaTopBtn:hover { background: %4; }"
        "QToolButton#lousaTopBtn:disabled { background: transparent; }"
        "QToolButton#lousaZoomLabel { background: transparent; border: none; border-radius: @radius-control;"
        "  color: %3; font-size: 12px; }"
        "QToolButton#lousaZoomLabel:hover { background: %4; }"
        "QToolButton#lousaBoardPill { background: transparent; border: none; border-radius: @radius-control;"
        "  color: %7; font-size: 13px; font-weight: 600; padding: 0 8px; }"
        "QToolButton#lousaBoardPill:hover { background: %4; }"
        "QToolButton#lousaChip { background: %1; border: 1px solid %2; border-radius: 10px;"
        "  color: %3; font-size: 13px; padding: 0 12px; }"
        "QToolButton#lousaChip:hover { color: %7; border-color: %5; }"
        "QFrame#lousaTopSep { background: %2; }"
        "QWidget#lousaEmptyBox { background: transparent; }"
        "QLabel#lousaEmpty { color: %6; font-size: 14px; background: transparent; }"
        "QPushButton#lousaTemplateBtn { background: %1; border: 1px solid %2; border-radius: @radius-control;"
        "  color: %3; font-size: 13px; padding: 7px 16px; }"
        "QPushButton#lousaTemplateBtn:hover { border-color: %5; color: %7; }"
        "QWidget#lousaMapPanel { background: %1; border: 1px solid %2; border-radius: @radius-panel; }"
        "QLabel#lousaMapTitle { color: %6; font-size: 11px; font-weight: 700; letter-spacing: 1px; background: transparent; }"
        "QListWidget#lousaMapList { background: transparent; border: none; color: %3; font-size: 13px; outline: none; }"
        "QListWidget#lousaMapList::item { padding: 6px 4px; border-radius: 6px; }"
        "QListWidget#lousaMapList::item:hover { background: %4; }"
        "QListWidget#lousaMapList::item:selected { background: %9; color: %7; }"
    ).arg(Theme::panelBackground(),   // 1
         Theme::panelBorder(),        // 2
         Theme::textPrimary(),        // 3
         Theme::hoverOverlay(),       // 4
         Theme::accentDefault(),      // 5
         Theme::textMuted(),          // 6
         Theme::textBright(),         // 7
         board.name(),                // 8
         accentSoft)));               // 9

    if (m_cardPreview) {
        m_cardPreview->setStyleSheet(Theme::qss(QStringLiteral(
            "QWidget#cardPreview {"
            "  background: %1;"
            "  border: 1px solid %2;"
            "  border-radius: @radius-panel;"
            "}"
            "QLabel#cardPreviewTitle {"
            "  color: %3;"
            "  font-size: 11px;"
            "  font-weight: 700;"
            "  background: transparent;"
            "}"
            "QWidget#cardPreviewDivider {"
            "  background: %2;"
            "}"
            "QTextEdit#cardPreviewBody {"
            "  color: %4;"
            "  font-size: 11px;"
            "  background: transparent;"
            "  border: none;"
            "  padding: 0;"
            "  selection-background-color: transparent;"
            "}"
        ).arg(Theme::panelBackground(), Theme::panelBorder(),
              Theme::textBright(),      Theme::textPrimary())));
        if (!m_cardPreview->graphicsEffect()) {
            auto* shadow = new QGraphicsDropShadowEffect(m_cardPreview);
            shadow->setBlurRadius(18);
            shadow->setOffset(0, 4);
            shadow->setColor(QColor(0, 0, 0, 100));
            m_cardPreview->setGraphicsEffect(shadow);
        }
    }
    if (m_dock) m_dock->applyTheme();
    if (m_actionBar) m_actionBar->applyTheme();
    if (m_search) m_search->applyTheme();
    if (m_cheat) m_cheat->applyTheme();
    if (m_minimap) m_minimap->scheduleRefresh();
    reloadIcons();
    if (m_view) {
        m_view->setStyleSheet(QStringLiteral(
            "QGraphicsView { border: none; background: transparent; }"));
    }
    scheduleActionBar();
}
