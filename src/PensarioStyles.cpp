// Estilos e ferramentas do Pensário: trilho/doca/divisórias, os desenhos de
// item (cartão, post-it, caderno, WhatsQenna, revista, lista + detalhe,
// baralho, tabela, quadro) e as ferramentas globais (Lente de capítulo,
// Busca, Cores com nome, Revisão e Levar pro texto).
#include "PensarioPanel.h"

#include "DocCache.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "MarkerStore.h"
#include "NotesStore.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <QAbstractButton>
#include <QActionGroup>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <QCursor>
#include <QMap>
#include <QSet>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <limits>

namespace {
QColor tc(const QString& css) { return Theme::toColor(css); }
QColor mix(const QColor& a, const QColor& b, qreal t) {
    return QColor::fromRgbF(a.redF() * t + b.redF() * (1 - t), a.greenF() * t + b.greenF() * (1 - t),
                            a.blueF() * t + b.blueF() * (1 - t));
}
// Várias cores do tema são películas translúcidas (inputBackground é
// rgba(...,0.05) em todos os temas). mix() e QColor::name() jogam o alfa fora
// e a película vira cor sólida — marrom-quase-preto num tema claro, branco num
// escuro, e o texto some. Antes de misturar ou virar CSS, achata sobre o painel.
QColor flat(const QColor& c) { return mix(c, Theme::toColor(Theme::panelBackground()), c.alphaF()); }
QString handFamily() { return QStringLiteral("'Caveat','Segoe Print','Comic Sans MS'"); }
QString serifFamily() { return QStringLiteral("'Source Serif 4','Lora',Georgia,serif"); }

QString menuQss() {
    return Theme::qss(QStringLiteral(R"(
        QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: @radius-panel; padding: 4px; }
        QMenu::item { padding: 6px 16px; border-radius: @radius-item; }
        QMenu::item:selected { background: %4; color: %5; }
        QMenu::item:disabled { color: %6; }
        QMenu::separator { height: 1px; background: %3; margin: 4px 6px; }
    )")).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
           Theme::hoverOverlay(), Theme::textBright(), Theme::textMuted());
}

QString chipQss(bool on, const QString& accent = QString()) {
    return Theme::qss(QStringLiteral(
        "QToolButton { color: %1; background: %2; border: 1px solid %3; border-radius: @radius-control;"
        " padding: 2px 9px; font-size: 11px; }"
        "QToolButton:hover { color: %4; border-color: %5; }"))
        .arg(on ? Theme::textBright() : Theme::textPrimary(),
             on ? Theme::accentInfoSoft() : QStringLiteral("transparent"),
             on ? (accent.isEmpty() ? Theme::accentInfoBorderSoft() : accent) : Theme::subtleBorder(),
             Theme::textBright(), Theme::borderStrong());
}

QLabel* capsLabel(const QString& t, QWidget* parent, const QString& color = QString()) {
    auto* l = new QLabel(t.toUpper(), parent);
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(10);
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(color.isEmpty() ? Theme::textMuted() : color));
    return l;
}

// Botão do trilho/doca: ícone, contador no canto e o risquinho de ativo.
class PnRailButton : public QAbstractButton {
public:
    PnRailButton(const QString& icon, const QString& glyph, int count, QWidget* parent)
        : QAbstractButton(parent), m_iconPath(icon), m_glyph(glyph), m_count(count) {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(36, 36);
        setAttribute(Qt::WA_Hover);
    }
    bool horizontal = false;
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
        if (isChecked()) {
            p.setPen(Qt::NoPen);
            p.setBrush(tc(Theme::accentInfoSoft()));
            p.drawRoundedRect(r, 7, 7);
            p.setBrush(tc(Theme::accentDefault()));
            if (horizontal) p.drawRoundedRect(QRectF(9, height() - 2.5, width() - 18, 2), 1, 1);
            else p.drawRoundedRect(QRectF(0, 9, 2, height() - 18), 1, 1);
        } else if (underMouse()) {
            p.setPen(Qt::NoPen);
            p.setBrush(tc(Theme::hoverOverlay()));
            p.drawRoundedRect(r, 7, 7);
        }
        const QColor ink = isChecked() || underMouse() ? tc(Theme::textBright()) : tc(Theme::textMuted());
        if (!m_iconPath.isEmpty()) {
            const QIcon ic = IconUtils::loadToolbarIcon(m_iconPath, ink, ink, ink, QSize(18, 18));
            ic.paint(&p, QRect((width() - 18) / 2, (height() - 18) / 2, 18, 18));
        } else {
            p.setPen(ink);
            QFont f = font();
            f.setPixelSize(15);
            p.setFont(f);
            p.drawText(rect(), Qt::AlignCenter, m_glyph);
        }
        if (m_count > 0) {
            QFont f(QStringLiteral("Segoe UI"));
            f.setPixelSize(8);
            f.setBold(true);
            p.setFont(f);
            const QString t = m_count > 99 ? QStringLiteral("99+") : QString::number(m_count);
            const qreal w = qMax(14.0, QFontMetricsF(f).horizontalAdvance(t) + 6);
            const QRectF b(width() - w - 1, 1, w, 14);
            p.setPen(Qt::NoPen);
            p.setBrush(tc(Theme::accentDefault()));
            p.drawRoundedRect(b, 7, 7);
            p.setPen(Qt::white);
            p.drawText(b, Qt::AlignCenter, t);
        }
    }
private:
    QString m_iconPath, m_glyph;
    int m_count = 0;
};

// Divisória do Caderno: aba colorida na borda, texto de pé.
class PnDividerTab : public QAbstractButton {
public:
    PnDividerTab(const QString& text, const QColor& color, QWidget* parent)
        : QAbstractButton(parent), m_color(color) {
        setText(text);
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFixedWidth(34);
        QFont f(QStringLiteral("Segoe UI"));
        f.setPixelSize(11);
        f.setBold(true);
        setFont(f);
        setFixedHeight(QFontMetrics(f).horizontalAdvance(text) + 26);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int w = isChecked() ? 34 : 26;
        QPainterPath path;
        path.addRoundedRect(QRectF(0, 0, w, height()), 6, 6);
        path.addRect(QRectF(0, 0, 6, height()));
        p.setPen(Qt::NoPen);
        p.setBrush(m_color);
        p.drawPath(path.simplified());
        if (!isChecked()) {
            QLinearGradient g(0, 0, 6, 0);
            g.setColorAt(0, QColor(0, 0, 0, 60));
            g.setColorAt(1, QColor(0, 0, 0, 0));
            p.fillRect(QRectF(0, 0, 6, height()), g);
        }
        p.save();
        p.translate(w / 2.0 + 4, height() / 2.0);
        p.rotate(90);
        p.setPen(QColor(20, 20, 20, 220));
        p.setFont(font());
        p.drawText(QRectF(-height() / 2.0, -10, height(), 20), Qt::AlignCenter, text());
        p.restore();
    }
private:
    QColor m_color;
};

// Papel pautado do Caderno: linhas de 28px e a margem vermelha.
class PnPaper : public QWidget {
public:
    explicit PnPaper(QWidget* parent) : QWidget(parent) { setAttribute(Qt::WA_StyledBackground, false); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), tc(Theme::panelBackground()));
        QColor line = tc(Theme::accentInfo());
        line.setAlphaF(0.16);
        for (int y = 40; y < height(); y += 28) p.fillRect(QRect(0, y, width(), 1), line);
        p.fillRect(QRect(50, 0, 1, height()), QColor(229, 115, 115, 115));
    }
};

// Baralho: as duas fichas "atrás" da ficha da frente.
class PnDeckBack : public QWidget {
public:
    explicit PnDeckBack(QWidget* parent) : QWidget(parent) {}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor card = tc(Theme::inputBackground());
        const QColor bd = tc(Theme::subtleBorder());
        for (int i = 2; i >= 1; --i) {
            const QRectF r = QRectF(rect()).adjusted(10 + i * 6, 6 + i * 7, -(10 + i * 6), -2 + i * 4);
            p.setPen(QPen(bd, 1));
            p.setBrush(card.darker(100 + i * 8));
            p.save();
            p.translate(r.center());
            p.rotate(i == 1 ? -2.5 : 2);
            p.drawRoundedRect(QRectF(-r.width() / 2, -r.height() / 2, r.width(), r.height()), 10, 10);
            p.restore();
        }
    }
};

QString quoted(const QString& s) {
    const QString t = s.trimmed();
    if (t.isEmpty()) return t;
    if (t.startsWith(QChar(0x201C)) || t.startsWith(QLatin1Char('"'))) return t;
    return QStringLiteral("“%1”").arg(t);
}

QString fmtDate(qint64 ms) {
    if (ms <= 0) return QStringLiteral("—");
    return QLocale().toString(QDateTime::fromMSecsSinceEpoch(ms).date(), QLocale::ShortFormat);
}

QColor speakerColor(const QString& id) {
    if (id.isEmpty()) return QColor(0x88, 0x88, 0x88);
    return QColor::fromHsl(int(qHash(id) % 360), 120, 150);
}

// Célula de tabela com chave de ordenação própria.
class PnSortItem : public QTableWidgetItem {
public:
    using QTableWidgetItem::QTableWidgetItem;
    bool operator<(const QTableWidgetItem& other) const override {
        const QVariant a = data(Qt::UserRole + 1), b = other.data(Qt::UserRole + 1);
        if (a.isValid() && b.isValid()) {
            if (a.typeId() == QMetaType::QString) return QString::localeAwareCompare(a.toString(), b.toString()) < 0;
            return a.toDouble() < b.toDouble();
        }
        return QTableWidgetItem::operator<(other);
    }
};
}

// ============================================================ estilo

QString PensarioPanel::styleId(Style s) {
    switch (s) {
    case Style::Classic:    return QStringLiteral("classic");
    case Style::Rail:       return QStringLiteral("rail");
    case Style::TwoColumns: return QStringLiteral("columns");
    case Style::Mural:      return QStringLiteral("mural");
    case Style::Notebook:   return QStringLiteral("notebook");
    case Style::Deck:       return QStringLiteral("deck");
    case Style::Board:      return QStringLiteral("board");
    case Style::WhatsQenna: return QStringLiteral("whatsqenna");
    case Style::Table:      return QStringLiteral("table");
    case Style::Magazine:   return QStringLiteral("magazine");
    case Style::Dock:       return QStringLiteral("dock");
    }
    return QStringLiteral("rail");
}

PensarioPanel::Style PensarioPanel::styleFromId(const QString& id) {
    for (Style s : { Style::Classic, Style::Rail, Style::TwoColumns, Style::Mural, Style::Notebook, Style::Deck,
                     Style::Board, Style::WhatsQenna, Style::Table, Style::Magazine, Style::Dock })
        if (styleId(s) == id) return s;
    return Style::Rail;   // padrão do Pensário
}

int PensarioPanel::styleWidth() const {
    const int stored = QSettings().value(QStringLiteral("ui/pensario/width-") + styleId(m_style), 0).toInt();
    if (stored > 0) return qBound(300, stored, 1200);
    switch (m_style) {
    case Style::Classic:    return 380;
    case Style::Rail:       return 410;
    case Style::TwoColumns: return 720;
    case Style::Mural:      return 560;
    case Style::Notebook:   return 450;
    case Style::Deck:       return 420;
    case Style::Board:      return 600;
    case Style::WhatsQenna: return 420;
    case Style::Table:      return 460;
    case Style::Magazine:   return 620;
    case Style::Dock:       return 390;
    }
    return 380;
}

PensarioPanel::Nav PensarioPanel::navFor(Style s) const {
    switch (s) {
    case Style::Rail:     return Nav::Rail;
    case Style::Dock:     return Nav::Dock;
    case Style::Notebook: return Nav::Dividers;
    case Style::Board:    return Nav::None;
    default:              return Nav::Tabs;
    }
}

void PensarioPanel::loadStyleSettings() {
    QSettings s;
    m_style = styleFromId(s.value(QStringLiteral("ui/pensario/style")).toString());
    m_toolLens = s.value(QStringLiteral("ui/pensario/tool/lens"), false).toBool();
    m_toolSearch = s.value(QStringLiteral("ui/pensario/tool/search"), false).toBool();
    m_toolColors = s.value(QStringLiteral("ui/pensario/tool/colors"), false).toBool();
    m_toolReview = s.value(QStringLiteral("ui/pensario/tool/review"), true).toBool();
    m_toolInsert = s.value(QStringLiteral("ui/pensario/tool/insert"), false).toBool();
}

void PensarioPanel::setStyle(Style s) {
    if (m_style == s) return;
    m_style = s;
    QSettings().setValue(QStringLiteral("ui/pensario/style"), styleId(s));
    m_pickKey.clear();
    applyStyleLayout();
}

void PensarioPanel::applyStyleLayout() {
    const Nav nav = navFor(m_style);
    const bool board = (m_style == Style::Board);
    if (m_tabsRow) m_tabsRow->setVisible(nav == Nav::Tabs && !m_lensView);
    if (m_rail) m_rail->setVisible(nav == Nav::Rail);
    if (m_dock) m_dock->setVisible(nav == Nav::Dock);
    if (m_dividers) m_dividers->setVisible(nav == Nav::Dividers);
    // No trilho e na doca, Nomes/Mapa/Glossário descem do cabeçalho.
    const bool toolsInHeader = (nav != Nav::Rail && nav != Nav::Dock);
    if (m_namesBtn) m_namesBtn->setVisible(toolsInHeader);
    if (m_mapBtn) m_mapBtn->setVisible(toolsInHeader);
    if (m_glossaryBtn) m_glossaryBtn->setVisible(toolsInHeader);
    if (m_sectionRow) m_sectionRow->setVisible((nav == Nav::Rail || nav == Nav::Dock) && !alternateViewActive());
    if (m_detail) m_detail->setVisible(m_style == Style::TwoColumns && !alternateViewActive() && m_tab != Tab::Names);
    const bool alt = alternateViewActive();
    if (m_board) m_board->setVisible(board && !alt);
    if (m_altScroll) m_altScroll->setVisible(alt);
    if (m_stack) m_stack->setVisible(!board && !alt);
    if (m_sortBtn) m_sortBtn->setVisible(m_tab == Tab::Comments && !board && !alt);

    // Aparência das abas: chips (Mural), revista (Revista) ou abas.
    const QString look = m_style == Style::Mural ? QStringLiteral("chips")
                       : m_style == Style::Magazine ? QStringLiteral("mag") : QString();
    for (QToolButton* b : { m_tabComments, m_tabNotes, m_tabMemories, m_tabDialogues }) {
        if (!b) continue;
        b->setProperty("look", look);
        b->style()->unpolish(b);
        b->style()->polish(b);
    }

    // Largura do estilo, mantendo a borda direita no lugar.
    if (m_positioned && parentWidget()) {
        const int right = x() + width();
        resize(styleWidth(), height());
        move(qMax(0, right - width()), y());
    } else {
        resize(styleWidth(), height());
    }
    rebuildNav();
    rebuildToolsBar();
    if (board && !alt) rebuildBoard();
    else if (alt) { if (!m_searchQuery.isEmpty()) rebuildSearchPage(); else rebuildLensPage(); }
    else selectTab(m_tab);
}

void PensarioPanel::showStyleMenu() {
    QMenu menu(this);
    menu.setStyleSheet(menuQss());
    menu.setToolTipsVisible(true);
    auto heading = [&menu](const QString& t) {
        QAction* a = menu.addAction(t.toUpper());
        a->setEnabled(false);
        QFont f = a->font();
        f.setPointSizeF(7.5);
        f.setBold(true);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
        a->setFont(f);
    };
    heading(tr("Estilo"));
    auto* g = new QActionGroup(&menu);
    struct Opt { Style s; QString name; QString tip; };
    const QList<Opt> opts = {
        { Style::Classic,    tr("Clássico"),     tr("As abas em cima e os cartões") },
        { Style::Rail,       tr("Trilho"),       tr("As seções num trilho de ícones com contador") },
        { Style::Dock,       tr("Doca"),         tr("O trilho deitado, embaixo") },
        { Style::TwoColumns, tr("Duas colunas"), tr("Lista de um lado, o item inteiro do outro") },
        { Style::Mural,      tr("Mural"),        tr("Post-its tingidos pela cor") },
        { Style::Notebook,   tr("Caderno"),      tr("Folha pautada, grifo na cor do marcador, divisórias na borda") },
        { Style::Deck,       tr("Baralho"),      tr("Uma ficha grande por vez") },
        { Style::Board,      tr("Quadro"),       tr("As quatro seções em colunas, lado a lado") },
        { Style::WhatsQenna, QStringLiteral("WhatsQenna"), tr("Os diálogos como conversa; comentários como resposta ao trecho") },
        { Style::Table,      tr("Tabela"),       tr("Densa e ordenável") },
        { Style::Magazine,   tr("Revista"),      tr("Citações em duas colunas, como numa revista") },
    };
    for (const Opt& o : opts) {
        QAction* a = menu.addAction(o.name);
        a->setCheckable(true);
        a->setChecked(m_style == o.s);
        a->setToolTip(o.tip);
        g->addAction(a);
        const Style s = o.s;
        connect(a, &QAction::triggered, this, [this, s]() { setStyle(s); });
    }
    menu.addSeparator();
    heading(tr("Ferramentas"));
    struct T { bool* flag; QString key; QString name; QString tip; };
    const QList<T> tools = {
        { &m_toolSearch, QStringLiteral("search"), tr("Busca no Pensário"), tr("Uma busca que atravessa comentários, notas, memórias e diálogos") },
        { &m_toolLens,   QStringLiteral("lens"),   tr("Lente de capítulo"), tr("Tudo de um capítulo, de todos os tipos. É filtro: escolha qualquer capítulo ou Todos") },
        { &m_toolColors, QStringLiteral("colors"), tr("Cores com nome"),    tr("A cor vira categoria, com legenda do projeto e filtro (comentários e notas)") },
        { &m_toolReview, QStringLiteral("review"), tr("Revisão"),           tr("Comentários marcados como tarefa ganham caixinha de resolvido e progresso") },
        { &m_toolInsert, QStringLiteral("insert"), tr("Levar pro texto"),   tr("Inserir nota, memória ou fala no cursor do editor") },
    };
    for (const T& t : tools) {
        QAction* a = menu.addAction(t.name);
        a->setCheckable(true);
        a->setChecked(*t.flag);
        a->setToolTip(t.tip);
        bool* flag = t.flag;
        const QString key = t.key;
        connect(a, &QAction::toggled, this, [this, flag, key](bool on) {
            *flag = on;
            QSettings().setValue(QStringLiteral("ui/pensario/tool/") + key, on);
            if (!on) {
                if (key == QStringLiteral("search")) m_searchQuery.clear();
                if (key == QStringLiteral("lens")) m_lensView = false;
                if (key == QStringLiteral("colors")) m_colorFilter.clear();
                if (key == QStringLiteral("review")) { m_taskFilter = QStringLiteral("all"); m_hideDone = false; }
            }
            applyStyleLayout();
        });
    }
    menu.exec(m_styleBtn->mapToGlobal(QPoint(0, m_styleBtn->height() + 2)));
}

// ============================================================ navegação

QString PensarioPanel::tabName(Tab t) const {
    switch (t) {
    case Tab::Comments:  return tr("Comentários");
    case Tab::Notes:     return tr("Notas");
    case Tab::Memories:  return tr("Memórias");
    case Tab::Dialogues: return tr("Diálogos");
    case Tab::Names:     return tr("Nomes");
    }
    return QString();
}

QString PensarioPanel::tabIcon(Tab t) const {
    switch (t) {
    case Tab::Comments:  return QStringLiteral(":/icons/pn-comment.svg");
    case Tab::Notes:     return QStringLiteral(":/icons/pn-note.svg");
    case Tab::Memories:  return QStringLiteral(":/icons/pn-memory.svg");
    case Tab::Dialogues: return QStringLiteral(":/icons/pn-dialogue.svg");
    default:             return QString();
    }
}

int PensarioPanel::tabCount(Tab t) const {
    switch (t) {
    case Tab::Comments: {
        int n = 0;
        if (m_markers) for (const auto& v : m_markers->allEntries()) n += v.size();
        return n;
    }
    case Tab::Notes:     return m_notesStore ? m_notesStore->notes().size() : 0;
    case Tab::Memories:  return m_memories ? m_memories->memories().size() : 0;
    case Tab::Dialogues: return m_dialogues ? m_dialogues->dialogues().size() : 0;
    default:             return 0;
    }
}

void PensarioPanel::rebuildNav() {
    const Nav nav = navFor(m_style);
    const QList<Tab> tabs = { Tab::Comments, Tab::Notes, Tab::Memories, Tab::Dialogues };
    auto clear = [](QBoxLayout* lay) {
        if (!lay) return;
        while (QLayoutItem* it = lay->takeAt(0)) {
            if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
            delete it;
        }
    };
    clear(m_railLay);
    clear(m_dockLay);
    clear(m_dividersLay);
    if (nav == Nav::Rail || nav == Nav::Dock) {
        QBoxLayout* lay = nav == Nav::Rail ? m_railLay : m_dockLay;
        QWidget* host = nav == Nav::Rail ? m_rail : m_dock;
        for (Tab t : tabs) {
            auto* b = new PnRailButton(tabIcon(t), QString(), tabCount(t), host);
            b->horizontal = (nav == Nav::Dock);
            b->setToolTip(tabName(t));
            b->setProperty("pnTab", int(t));
            connect(b, &QAbstractButton::clicked, this, [this, t]() { m_lensView = false; selectTab(t); applyStyleLayout(); });
            lay->addWidget(b, 0, Qt::AlignCenter);
        }
        lay->addStretch(1);
        auto* names = new PnRailButton(QString(), QStringLiteral("✦"), 0, host);
        names->horizontal = (nav == Nav::Dock);
        names->setToolTip(tr("Gerador de nomes"));
        names->setProperty("pnTab", int(Tab::Names));
        connect(names, &QAbstractButton::clicked, this, [this]() { m_lensView = false; selectTab(Tab::Names); applyStyleLayout(); });
        lay->addWidget(names, 0, Qt::AlignCenter);
        auto* map = new PnRailButton(QStringLiteral(":/icons/worldmap.svg"), QString(), 0, host);
        map->setCheckable(false);
        map->horizontal = (nav == Nav::Dock);
        map->setToolTip(tr("Mapa-múndi"));
        connect(map, &QAbstractButton::clicked, this, &PensarioPanel::openMapPanel);
        lay->addWidget(map, 0, Qt::AlignCenter);
        auto* gl = new PnRailButton(QStringLiteral(":/icons/glossary.svg"), QString(), 0, host);
        gl->setCheckable(false);
        gl->horizontal = (nav == Nav::Dock);
        gl->setToolTip(tr("Glossário"));
        connect(gl, &QAbstractButton::clicked, this, &PensarioPanel::toggleGlossaryPopup);
        lay->addWidget(gl, 0, Qt::AlignCenter);
    } else if (nav == Nav::Dividers) {
        const QList<QColor> colors = { QColor("#FFD54F"), QColor("#b39ddb"), QColor("#8fc7a4"), QColor("#90caf9") };
        for (int i = 0; i < tabs.size(); ++i) {
            const Tab t = tabs.at(i);
            auto* d = new PnDividerTab(tabName(t), colors.at(i), m_dividers);
            d->setProperty("pnTab", int(t));
            connect(d, &QAbstractButton::clicked, this, [this, t]() { m_lensView = false; selectTab(t); applyStyleLayout(); });
            m_dividersLay->addWidget(d, 0, Qt::AlignLeft);
        }
        m_dividersLay->addStretch(1);
    }
    // Mural: as abas viram chips com contador.
    auto setTabText = [this](QToolButton* b, Tab t) {
        if (!b) return;
        b->setText(m_style == Style::Mural ? QStringLiteral("%1  %2").arg(tabName(t)).arg(tabCount(t)) : tabName(t));
    };
    setTabText(m_tabComments, Tab::Comments);
    setTabText(m_tabNotes, Tab::Notes);
    setTabText(m_tabMemories, Tab::Memories);
    setTabText(m_tabDialogues, Tab::Dialogues);
    updateNavChecks();
}

void PensarioPanel::updateNavChecks() {
    for (QWidget* host : { m_rail, m_dock, m_dividers }) {
        if (!host) continue;
        for (auto* b : host->findChildren<QAbstractButton*>()) {
            const QVariant v = b->property("pnTab");
            if (v.isValid()) b->setChecked(!m_lensView && v.toInt() == int(m_tab));
        }
    }
    if (m_sectionTitle) {
        m_sectionTitle->setText(m_tab == Tab::Names ? tabName(Tab::Names).toUpper()
            : QStringLiteral("%1 · %2").arg(tabName(m_tab).toUpper()).arg(tabCount(m_tab)));
    }
}

bool PensarioPanel::alternateViewActive() const {
    return (m_toolSearch && !m_searchQuery.trimmed().isEmpty()) || (m_toolLens && m_lensView);
}

// ============================================================ barra de ferramentas (Busca, Lente)

void PensarioPanel::rebuildToolsBar() {
    if (!m_toolsLay) return;
    // O campo de busca sobrevive ao rebuild (senão perderia o foco a cada tecla).
    while (QLayoutItem* it = m_toolsLay->takeAt(0)) {
        if (QWidget* w = it->widget()) { if (w != m_searchEdit) { w->hide(); w->deleteLater(); } }
        delete it;
    }
    if (m_toolSearch) {
        if (!m_searchEdit) {
            m_searchEdit = new QLineEdit(m_toolsBar);
            m_searchEdit->setClearButtonEnabled(true);
            m_searchEdit->setPlaceholderText(tr("Buscar no Pensário…"));
            connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
                const bool was = alternateViewActive();
                m_searchQuery = t;
                if (was != alternateViewActive()) applyStyleLayout();
                else if (alternateViewActive()) rebuildSearchPage();
            });
        }
        m_searchEdit->setStyleSheet(Theme::qss(QStringLiteral(
            "QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: @radius-control;"
            " padding: 5px 8px; font-size: 12.5px; } QLineEdit:focus { border-color: %4; }"))
            .arg(Theme::inputBackground(), Theme::textPrimary(), Theme::subtleBorder(), Theme::accentDefault()));
        m_searchEdit->show();
        m_toolsLay->addWidget(m_searchEdit);
    } else if (m_searchEdit) {
        m_searchEdit->hide();
    }
    if (m_toolLens && m_model) {
        auto* row = new QWidget(m_toolsBar);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(6);
        rl->addWidget(capsLabel(tr("Lente"), row));
        auto* combo = new QComboBox(row);
        combo->addItem(tr("Todos os capítulos"), QString());
        for (const Manuscript& ms : m_model->manuscripts())
            for (const Chapter* ch : m_model->orderedChaptersForManuscript(ms.id))
                combo->addItem(chapterLabel(ch->manuscriptId, m_model->chapterDisplayLabel(*ch)
                    + (ch->title.isEmpty() ? QString() : QStringLiteral(" · ") + ch->title)), ch->id);
        const int idx = combo->findData(m_lensChapter);
        combo->setCurrentIndex(idx >= 0 ? idx : 0);
        combo->setStyleSheet(Theme::qss(QStringLiteral(
            "QComboBox { background: %1; color: %2; border: 1px solid %3; border-radius: @radius-control; padding: 3px 8px; font-size: 12px; }"
            "QComboBox QAbstractItemView { background: %4; color: %2; border: 1px solid %3; selection-background-color: %5; }"))
            .arg(Theme::inputBackground(), Theme::textPrimary(), Theme::subtleBorder(), Theme::panelBackground(), Theme::hoverOverlay()));
        connect(combo, qOverload<int>(&QComboBox::activated), this, [this, combo](int i) {
            m_lensChapter = combo->itemData(i).toString();
            m_lensView = true;
            applyStyleLayout();
        });
        rl->addWidget(combo, 1);
        auto* here = new QToolButton(row);
        here->setText(tr("No editor"));
        here->setToolTip(tr("O capítulo aberto no editor"));
        here->setCursor(Qt::PointingHandCursor);
        here->setStyleSheet(chipQss(m_lensView && m_lensChapter == m_currentChapterId && !m_currentChapterId.isEmpty()));
        here->setEnabled(!m_currentChapterId.isEmpty());
        connect(here, &QToolButton::clicked, this, [this]() {
            m_lensChapter = m_currentChapterId;
            m_lensView = true;
            applyStyleLayout();
        });
        rl->addWidget(here);
        if (m_lensView) {
            auto* off = new QToolButton(row);
            off->setText(QStringLiteral("✕"));
            off->setToolTip(tr("Fechar a lente"));
            off->setCursor(Qt::PointingHandCursor);
            off->setStyleSheet(chipQss(false));
            connect(off, &QToolButton::clicked, this, [this]() { m_lensView = false; applyStyleLayout(); });
            rl->addWidget(off);
        }
        m_toolsLay->addWidget(row);
    }
    m_toolsBar->setVisible(m_toolSearch || m_toolLens);
}

// ============================================================ coletores

QVector<PensarioPanel::PnItem> PensarioPanel::collectComments(bool applyToolFilters) const {
    QVector<PnItem> out;
    if (!m_markers) return out;
    const auto& all = m_markers->allEntries();
    for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
        const Chapter* ch = chapterForKey(it.key());
        for (const MarkerStore::Entry& e : it.value()) {
            PnItem p;
            p.kind = Tab::Comments;
            p.id = e.id;
            p.docKey = it.key();
            p.start = e.start;
            p.end = e.end;
            p.color = e.color.isEmpty() ? QStringLiteral("#FFD54F") : e.color;
            p.title = e.comment;
            p.quote = e.text;
            p.origin = originLabel(it.key(), e.sceneIndex);
            p.chapterId = ch ? ch->id : QString();
            p.created = e.createdAt;
            p.rank = rankForKey(it.key()) * 1000 + qMax(0, e.sceneIndex) * 100000 / 1000;
            p.task = e.task;
            p.done = e.done;
            if (applyToolFilters) {
                if (m_toolColors && !m_colorFilter.isEmpty() && QColor(p.color) != QColor(m_colorFilter)) continue;
                if (m_toolReview) {
                    if (m_taskFilter == QStringLiteral("tasks") && !p.task) continue;
                    if (m_taskFilter == QStringLiteral("comments") && p.task) continue;
                    if (m_hideDone && p.task && p.done) continue;
                }
            }
            out.append(p);
        }
    }
    if (m_sortMode == SortMode::Creation) {
        std::sort(out.begin(), out.end(), [](const PnItem& a, const PnItem& b) {
            if (a.created != b.created) return a.created > b.created;
            return a.start < b.start;
        });
    } else {
        std::sort(out.begin(), out.end(), [this](const PnItem& a, const PnItem& b) {
            const int ra = rankForKey(a.docKey), rb = rankForKey(b.docKey);
            if (ra != rb) return ra < rb;
            if (a.docKey != b.docKey) return a.docKey < b.docKey;
            return a.start < b.start;
        });
        for (PnItem& p : out) p.group = p.origin;
    }
    return out;
}

QVector<PensarioPanel::PnItem> PensarioPanel::collectNotes(bool applyToolFilters) const {
    QVector<PnItem> out;
    if (!m_notesStore) return out;
    QVector<NotesStore::Note> list = m_notesStore->notes();
    std::sort(list.begin(), list.end(), [](const NotesStore::Note& a, const NotesStore::Note& b) {
        return a.createdAt > b.createdAt;
    });
    for (const NotesStore::Note& n : list) {
        PnItem p;
        p.kind = Tab::Notes;
        p.id = n.id;
        p.color = n.color.isEmpty() ? QStringLiteral("#FFD54F") : n.color;
        p.title = n.title.trimmed();
        p.body = n.text;
        p.created = n.createdAt;
        if (applyToolFilters && m_toolColors && !m_colorFilter.isEmpty() && QColor(p.color) != QColor(m_colorFilter)) continue;
        out.append(p);
    }
    return out;
}

QVector<PensarioPanel::PnItem> PensarioPanel::collectMemories() const {
    QVector<PnItem> out;
    if (!m_memories) return out;
    QVector<MemoriesStore::Memory> list = m_memories->memories();
    std::sort(list.begin(), list.end(), [](const MemoriesStore::Memory& a, const MemoriesStore::Memory& b) {
        return a.createdAt > b.createdAt;
    });
    for (const auto& m : list) {
        PnItem p;
        p.kind = Tab::Memories;
        p.id = m.id;
        p.title = m.name.isEmpty() ? (m.sourceLabel.isEmpty() ? tr("Memória") : tr("Memória do %1").arg(m.sourceLabel)) : m.name;
        p.quote = m.text;
        p.origin = m.sourceLabel;
        p.tags = m.tags;
        p.chapterId = m.chapterId;
        p.created = m.createdAt;
        p.color = QStringLiteral("#8fc7a4");
        out.append(p);
    }
    return out;
}

PensarioPanel::PnItem PensarioPanel::dialogueItem(const DialogueStore::Dialogue& d) const {
    PnItem p;
    p.kind = Tab::Dialogues;
    p.id = d.id;
    p.title = dialogueSpeakerLabel(d.characterId);
    p.body = d.text;
    p.origin = d.sourceLabel;
    p.group = d.sourceLabel.section(QStringLiteral(" — "), 0, 0);
    p.chapterId = d.chapterId;
    p.speakerId = d.characterId;
    p.created = d.createdAt;
    p.color = speakerColor(d.characterId).name();
    return p;
}

QVector<PensarioPanel::PnItem> PensarioPanel::collectDialogues(const QString& chapterId, int limit) const {
    QVector<PnItem> out;
    if (!m_dialogues) return out;
    for (const auto& d : m_dialogues->dialogues()) {
        if (!chapterId.isEmpty() && d.chapterId != chapterId) continue;
        out.append(dialogueItem(d));
        if (limit > 0 && out.size() >= limit) break;
    }
    return out;
}

// ============================================================ render

QString PensarioPanel::itemKey(const PnItem& it) const {
    return QStringLiteral("%1:%2").arg(int(it.kind)).arg(it.id);
}

void PensarioPanel::renderItems(Tab tab, const QVector<PnItem>& items, QVBoxLayout* lay, QWidget* inner) {
    for (const PnItem& it : items) m_itemIndex.insert(itemKey(it), it);
    auto groupLabel = [inner](const QString& text) {
        auto* g = new QLabel(text, inner);
        g->setObjectName(QStringLiteral("pnGroup"));
        g->setWordWrap(true);
        return g;
    };
    switch (m_style) {
    case Style::Classic:
    case Style::Rail:
    case Style::Dock:
    case Style::Board: {
        QString last;
        for (const PnItem& it : items) {
            if (!it.group.isEmpty() && it.group != last && tab == Tab::Comments) { last = it.group; lay->addWidget(groupLabel(it.group)); }
            lay->addWidget(renderClassic(it, inner));
        }
        break;
    }
    case Style::TwoColumns: {
        QString last;
        for (const PnItem& it : items) {
            if (!it.group.isEmpty() && it.group != last && tab == Tab::Comments) { last = it.group; lay->addWidget(groupLabel(it.group)); }
            lay->addWidget(renderListRow(it, inner));
        }
        if (!items.isEmpty() && (!m_itemIndex.contains(m_pickKey) || m_itemIndex.value(m_pickKey).kind != tab))
            m_pickKey = itemKey(items.first());
        if (items.isEmpty()) m_pickKey.clear();
        updateDetail();
        break;
    }
    case Style::Mural: {
        auto* host = new QWidget(inner);
        auto* hl = new QHBoxLayout(host);
        hl->setContentsMargins(0, 4, 0, 0);
        hl->setSpacing(10);
        const int cols = width() >= 680 ? 3 : 2;
        QList<QVBoxLayout*> colLays;
        QList<int> heights;
        for (int c = 0; c < cols; ++c) {
            auto* cl = new QVBoxLayout;
            cl->setSpacing(12);
            hl->addLayout(cl, 1);
            colLays << cl;
            heights << 0;
        }
        for (const PnItem& it : items) {
            int best = 0;
            for (int c = 1; c < cols; ++c) if (heights.at(c) < heights.at(best)) best = c;
            colLays[best]->addWidget(renderPostit(it, host));
            heights[best] += 60 + int((it.title.size() + it.body.size() + it.quote.size()) * 0.6);
        }
        for (auto* cl : colLays) cl->addStretch(1);
        lay->addWidget(host);
        break;
    }
    case Style::Notebook: {
        auto* paper = new PnPaper(inner);
        auto* pl = new QVBoxLayout(paper);
        pl->setContentsMargins(58, 10, 14, 18);
        pl->setSpacing(10);
        auto* h = new QLabel(tabName(tab), paper);
        h->setStyleSheet(QStringLiteral("color: %1; font-family: %2; font-size: 28px; background: transparent;")
            .arg(Theme::textBright(), handFamily()));
        pl->addWidget(h);
        for (const PnItem& it : items) pl->addWidget(renderNotebookEntry(it, paper));
        pl->addStretch(1);
        lay->addWidget(paper);
        break;
    }
    case Style::Deck:
        lay->addWidget(renderDeck(tab, items, inner));
        break;
    case Style::WhatsQenna: {
        // Quem mais fala fica do lado "de cá" da conversa.
        QHash<QString, int> cnt;
        for (const PnItem& it : items) if (!it.speakerId.isEmpty()) cnt[it.speakerId] += 1;
        QString me;
        for (auto c = cnt.cbegin(); c != cnt.cend(); ++c) if (me.isEmpty() || c.value() > cnt.value(me)) me = c.key();
        setProperty("pnBubbleMe", me);
        QString last;
        if (tab == Tab::Notes && !items.isEmpty()) {
            auto* day = new QLabel(tr("Notas fixadas"), inner);
            day->setObjectName(QStringLiteral("pnDay"));
            lay->addWidget(day, 0, Qt::AlignHCenter);
        }
        for (const PnItem& it : items) {
            const QString g = (tab == Tab::Comments) ? it.origin : (tab == Tab::Dialogues ? it.group : QString());
            if (!g.isEmpty() && g != last) {
                last = g;
                auto* day = new QLabel(g, inner);
                day->setObjectName(QStringLiteral("pnDay"));
                lay->addWidget(day, 0, Qt::AlignHCenter);
            }
            lay->addWidget(renderBubble(it, inner));
        }
        break;
    }
    case Style::Table:
        lay->addWidget(renderTable(tab, items, inner));
        break;
    case Style::Magazine: {
        auto* kick = new QLabel(tab == Tab::Memories ? tr("MEMÓRIAS") : tab == Tab::Dialogues ? tr("FALAS DA EDIÇÃO")
                               : tab == Tab::Comments ? tr("NOTAS DA REVISÃO") : tr("CADERNO DE IDEIAS"), inner);
        QFont kf(QStringLiteral("Segoe UI"));
        kf.setPixelSize(10);
        kf.setBold(true);
        kf.setLetterSpacing(QFont::AbsoluteSpacing, 3);
        kick->setFont(kf);
        kick->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::accentDefault()));
        lay->addWidget(kick);
        auto* title = new QLabel(tab == Tab::Memories ? tr("O que ficou guardado") : tab == Tab::Dialogues ? tr("Eles disseram")
                                : tab == Tab::Comments ? tr("O que falta consertar") : tr("Ideias soltas"), inner);
        title->setStyleSheet(QStringLiteral("color: %1; font-family: %2; font-size: 26px; font-weight: 600; background: transparent; margin-bottom: 6px;")
            .arg(Theme::textBright(), serifFamily()));
        lay->addWidget(title);
        auto* grid = new QWidget(inner);
        auto* gl = new QGridLayout(grid);
        gl->setContentsMargins(0, 0, 0, 0);
        gl->setHorizontalSpacing(22);
        gl->setVerticalSpacing(14);
        int r = 0;
        QList<int> colH = { 0, 0 };
        for (int i = 0; i < items.size(); ++i) {
            if (i == 0) { gl->addWidget(renderPullQuote(items.at(i), true, grid), r++, 0, 1, 2); continue; }
            const int c = colH.at(0) <= colH.at(1) ? 0 : 1;
            gl->addWidget(renderPullQuote(items.at(i), false, grid), r, c);
            colH[c] += 1;
            if (c == 1) ++r;
        }
        gl->setColumnStretch(0, 1);
        gl->setColumnStretch(1, 1);
        lay->addWidget(grid);
        break;
    }
    }
}

void PensarioPanel::wireItem(QWidget* w, const PnItem& it) {
    w->setProperty("pnItemKey", itemKey(it));
    w->setCursor(Qt::PointingHandCursor);
    w->installEventFilter(this);
}

QWidget* PensarioPanel::renderClassic(const PnItem& it, QWidget* parent) {
    QWidget* card = nullptr;
    switch (it.kind) {
    case Tab::Comments:
        card = buildCommentCard(it.docKey, it.color, it.title, it.quote, it.start, it.end,
                                it.group.isEmpty() ? it.origin : QString());
        break;
    case Tab::Notes:
        card = buildNoteCard(it.id, it.color, it.title, it.body);
        break;
    case Tab::Memories:
        card = buildMemoryCard(it.id, it.title, it.quote, it.tags, parent);
        break;
    case Tab::Dialogues:
        if (m_dialogues)
            for (const auto& d : m_dialogues->dialogues())
                if (d.id == it.id) { card = buildDialogueCard(d, it.title, it.origin, parent); break; }
        break;
    default: break;
    }
    if (!card) card = new QWidget(parent);
    decorateCard(card, it);
    return card;
}

void PensarioPanel::decorateCard(QWidget* card, const PnItem& it) {
    if (it.kind != Tab::Dialogues) {
        card->setProperty("pnItemKey", itemKey(it));   // botão direito: menu do item
    }
    // Revisão: caixinha só em comentário marcado como tarefa.
    if (it.kind == Tab::Comments && m_toolReview && it.task) {
        if (auto* row = qobject_cast<QHBoxLayout*>(card->layout())) {
            auto* chk = new QToolButton(card);
            chk->setFixedSize(20, 20);
            chk->setCursor(Qt::PointingHandCursor);
            chk->setText(it.done ? QStringLiteral("✓") : QString());
            chk->setToolTip(it.done ? tr("Reabrir") : tr("Marcar como resolvido"));
            chk->setStyleSheet(Theme::qss(QStringLiteral(
                "QToolButton { color: %1; background: %2; border: 1.5px solid %3; border-radius: 10px; font-size: 12px; font-weight: 700; padding: 0px; }"
                "QToolButton:hover { border-color: %4; }"))
                .arg(Theme::panelBackground(), it.done ? Theme::accentSuccess() : QStringLiteral("transparent"),
                     it.done ? Theme::accentSuccess() : Theme::textMuted(), Theme::textBright()));
            const QString dk = it.docKey, id = it.id;
            const bool done = it.done;
            connect(chk, &QToolButton::clicked, this, [this, dk, id, done]() { if (m_markers) m_markers->setDone(dk, id, !done); });
            row->insertSpacing(1, 9);
            row->insertWidget(2, chk, 0, Qt::AlignVCenter);
        }
        if (it.done) {
            auto* eff = new QGraphicsOpacityEffect(card);
            eff->setOpacity(0.5);
            card->setGraphicsEffect(eff);
        }
    }
    // Levar pro texto.
    if (m_toolInsert && (it.kind == Tab::Notes || it.kind == Tab::Memories || it.kind == Tab::Dialogues)) {
        auto* link = new QToolButton(card);
        link->setText(tr("↳ Levar pro texto"));
        link->setCursor(Qt::PointingHandCursor);
        link->setToolTip(tr("Inserir no cursor do editor"));
        link->setStyleSheet(QStringLiteral("QToolButton { color: %1; background: transparent; border: none; font-size: 11px; padding: 2px 0; }"
                                           "QToolButton:hover { color: %2; }").arg(Theme::accentDefault(), Theme::textBright()));
        const QString text = insertableText(it);
        connect(link, &QToolButton::clicked, this, [this, text]() { emit insertTextRequested(text); });
        QVBoxLayout* target = nullptr;
        if (it.kind == Tab::Memories) target = qobject_cast<QVBoxLayout*>(card->layout());
        else if (it.kind == Tab::Notes) {
            auto* outer = qobject_cast<QVBoxLayout*>(card->layout());
            if (outer && outer->count() > 1 && outer->itemAt(1)->widget())
                target = qobject_cast<QVBoxLayout*>(outer->itemAt(1)->widget()->layout());
        } else {
            auto* outer = qobject_cast<QHBoxLayout*>(card->layout());
            if (outer && outer->count() > 1) target = qobject_cast<QVBoxLayout*>(outer->itemAt(1)->layout());
        }
        if (target) target->addWidget(link, 0, Qt::AlignLeft);
        else link->hide();
    }
}

QString PensarioPanel::insertableText(const PnItem& it) const {
    if (it.kind == Tab::Notes) return it.body.trimmed().isEmpty() ? it.title : it.body.trimmed();
    if (it.kind == Tab::Memories) return it.quote.trimmed();
    if (it.kind == Tab::Dialogues) return it.body.trimmed();
    return QString();
}

QWidget* PensarioPanel::renderPostit(const PnItem& it, QWidget* parent) {
    auto* f = new QFrame(parent);
    f->setObjectName(QStringLiteral("pnPostit"));
    const QColor c(it.kind == Tab::Memories ? tc(Theme::textPrimary()) : QColor(it.color));
    const QColor bg = mix(c, flat(tc(Theme::inputBackground())), 0.22);
    QColor bd = c; bd.setAlphaF(0.45);
    f->setStyleSheet(QStringLiteral("QFrame#pnPostit { background: %1; border: 1px solid %2; border-top-left-radius: 4px;"
                                    " border-top-right-radius: 4px; border-bottom-right-radius: 10px; border-bottom-left-radius: 4px; }")
        .arg(bg.name(), QStringLiteral("rgba(%1,%2,%3,%4)").arg(bd.red()).arg(bd.green()).arg(bd.blue()).arg(bd.alphaF())));
    auto* l = new QVBoxLayout(f);
    l->setContentsMargins(12, 12, 12, 14);
    l->setSpacing(5);
    auto lbl = [&](const QString& t, const QString& css) {
        auto* x = new QLabel(t, f);
        x->setWordWrap(true);
        x->setStyleSheet(QStringLiteral("background: transparent; ") + css);
        x->setAttribute(Qt::WA_TransparentForMouseEvents);
        l->addWidget(x);
        return x;
    };
    const QString ink = Theme::textPrimary(), bright = Theme::textBright(), muted = Theme::textMuted();
    const QString quoteCol = mix(c, tc(Theme::textPrimary()), 0.3).name();
    if (it.kind == Tab::Comments) {
        lbl(it.origin, QStringLiteral("color: %1; font-size: 10.5px;").arg(muted));
        lbl(it.title, QStringLiteral("color: %1; font-size: 13px;").arg(bright));
        if (!it.quote.trimmed().isEmpty()) lbl(quoted(it.quote), QStringLiteral("color: %1; font-size: 12px; font-style: italic;").arg(quoteCol));
    } else if (it.kind == Tab::Notes) {
        if (!it.title.isEmpty()) lbl(it.title, QStringLiteral("color: %1; font-family: %2; font-size: 21px;").arg(bright, handFamily()));
        lbl(it.body, QStringLiteral("color: %1; font-size: 12.5px;").arg(ink));
    } else if (it.kind == Tab::Memories) {
        lbl(it.title, QStringLiteral("color: %1; font-family: %2; font-size: 20px;").arg(bright, handFamily()));
        lbl(quoted(it.quote), QStringLiteral("color: %1; font-size: 12px; font-style: italic;").arg(quoteCol));
        if (!it.tags.isEmpty()) lbl(it.tags.join(QStringLiteral(" · ")), QStringLiteral("color: %1; font-size: 10.5px;").arg(muted));
    } else {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* av = new QLabel(f);
        av->setPixmap(it.speakerId.isEmpty() ? unattributedAvatar(22) : characterAvatar(it.speakerId, 22));
        av->setFixedSize(22, 22);
        row->addWidget(av);
        auto* nm = new QLabel(it.title, f);
        nm->setStyleSheet(QStringLiteral("background: transparent; color: %1; font-weight: 700; font-size: 12px;").arg(bright));
        row->addWidget(nm, 1);
        l->addLayout(row);
        lbl(it.body, QStringLiteral("color: %1; font-size: 12.5px;").arg(ink));
    }
    wireItem(f, it);
    if (m_toolInsert && (it.kind == Tab::Notes || it.kind == Tab::Memories || it.kind == Tab::Dialogues)) {
        auto* link = new QToolButton(f);
        link->setText(tr("↳ Levar pro texto"));
        link->setCursor(Qt::PointingHandCursor);
        link->setStyleSheet(QStringLiteral("QToolButton { color: %1; background: transparent; border: none; font-size: 11px; }").arg(Theme::accentDefault()));
        const QString text = insertableText(it);
        connect(link, &QToolButton::clicked, this, [this, text]() { emit insertTextRequested(text); });
        l->addWidget(link, 0, Qt::AlignLeft);
    }
    return f;
}

QWidget* PensarioPanel::renderNotebookEntry(const PnItem& it, QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(-52, 0, 0, 0);
    h->setSpacing(8);
    QString margin;
    if (it.kind == Tab::Notes) margin = tr("nota");
    else if (!it.chapterId.isEmpty() && m_model) {
        if (const Chapter* ch = m_model->findChapter(it.chapterId)) {
            const QString n = m_model->chapterNumberLabel(*ch);
            margin = n.isEmpty() ? m_model->chapterTypeName(*ch).left(4).toLower() : tr("cap. %1").arg(n);
        }
    } else if (it.kind == Tab::Memories) margin = tr("ficha");
    auto* mg = new QLabel(margin, w);
    mg->setFixedWidth(44);
    mg->setAlignment(Qt::AlignRight | Qt::AlignTop);
    mg->setStyleSheet(QStringLiteral("color: %1; font-family: %2; font-style: italic; font-size: 10.5px; background: transparent; padding-top: 6px;")
        .arg(Theme::textMuted(), serifFamily()));
    h->addWidget(mg, 0, Qt::AlignTop);
    const QColor c(it.color);
    const QString hl = QStringLiteral("rgba(%1,%2,%3,0.38)").arg(c.red()).arg(c.green()).arg(c.blue());
    QString html;
    const QString muted = tc(Theme::textMuted()).name(), bright = tc(Theme::textBright()).name();
    if (it.kind == Tab::Comments)
        html = QStringLiteral("%1 <span style=\"color:%2\"><i>— <span style=\"background:%3\">%4</span></i></span>")
            .arg(it.title.toHtmlEscaped(), muted, hl, it.quote.trimmed().toHtmlEscaped());
    else if (it.kind == Tab::Notes)
        html = (it.title.isEmpty() ? QString() : QStringLiteral("<span style=\"font-family:%1; font-size:17px; color:%2\">%3:</span> ")
                    .arg(handFamily(), tc(Theme::accentDefault()).name(), it.title.toHtmlEscaped()))
             + it.body.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    else if (it.kind == Tab::Memories)
        html = QStringLiteral("<span style=\"font-family:%1; font-size:17px; color:%2\">%3</span> <span style=\"color:%4\"><i>%5</i></span>")
            .arg(handFamily(), tc(Theme::accentDefault()).name(), it.title.toHtmlEscaped(), muted, quoted(it.quote).toHtmlEscaped());
    else
        html = QStringLiteral("<b style=\"color:%1\">%2:</b> %3").arg(bright, it.title.section(QLatin1Char(' '), 0, 0).toHtmlEscaped(),
                                                                     it.body.toHtmlEscaped());
    auto* text = new QLabel(html, w);
    text->setTextFormat(Qt::RichText);
    text->setWordWrap(true);
    text->setStyleSheet(QStringLiteral("color: %1; font-family: %2; font-size: 14px; background: transparent; line-height: 150%;")
        .arg(Theme::textPrimary(), serifFamily()));
    text->setAttribute(Qt::WA_TransparentForMouseEvents);
    h->addWidget(text, 1);
    wireItem(w, it);
    return w;
}

QWidget* PensarioPanel::renderBubble(const PnItem& it, QWidget* parent) {
    const QString meId = property("pnBubbleMe").toString();
    const bool me = (it.kind == Tab::Comments || it.kind == Tab::Notes)
                 || (it.kind == Tab::Dialogues && !meId.isEmpty() && it.speakerId == meId);
    auto* row = new QWidget(parent);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(7);
    auto* bub = new QFrame(row);
    bub->setObjectName(QStringLiteral("pnBubble"));
    const QColor accent = tc(Theme::accentDefault());
    const QColor card = flat(tc(Theme::inputBackground()));
    const QColor bg = me ? mix(accent, card, 0.24) : card;
    QColor bd = me ? accent : tc(Theme::subtleBorder());
    const QString bdCss = me ? QStringLiteral("rgba(%1,%2,%3,0.4)").arg(bd.red()).arg(bd.green()).arg(bd.blue()) : Theme::subtleBorder();
    const bool pinned = (it.kind == Tab::Notes);
    bub->setStyleSheet(QStringLiteral("QFrame#pnBubble { background: %1; border: 1px %5 %2; border-top-left-radius: 14px; border-top-right-radius: 14px;"
                                      " border-bottom-left-radius: %3px; border-bottom-right-radius: %4px; }")
        .arg(bg.name(), pinned ? it.color : bdCss).arg(me ? 14 : 4).arg(me ? 4 : 14).arg(pinned ? QStringLiteral("dashed") : QStringLiteral("solid")));
    bub->setMaximumWidth(int(width() * 0.78));
    auto* bl = new QVBoxLayout(bub);
    bl->setContentsMargins(11, 8, 11, 7);
    bl->setSpacing(3);
    auto lbl = [&](const QString& t, const QString& css, bool rich = false) {
        auto* x = new QLabel(t, bub);
        x->setWordWrap(true);
        if (rich) x->setTextFormat(Qt::RichText);
        x->setStyleSheet(QStringLiteral("background: transparent; ") + css);
        x->setAttribute(Qt::WA_TransparentForMouseEvents);
        bl->addWidget(x);
        return x;
    };
    const QString ink = me ? Theme::textBright() : Theme::textPrimary();
    if (it.kind == Tab::Dialogues) {
        if (!me) lbl(it.title.section(QLatin1Char(' '), 0, 0), QStringLiteral("color: %1; font-size: 10.5px; font-weight: 700;").arg(speakerColor(it.speakerId).lighter(125).name()));
        lbl(it.body, QStringLiteral("color: %1; font-size: 12.5px;").arg(ink));
    } else if (it.kind == Tab::Comments) {
        auto* rq = new QLabel(it.quote.trimmed(), bub);
        rq->setWordWrap(true);
        rq->setAttribute(Qt::WA_TransparentForMouseEvents);
        rq->setStyleSheet(QStringLiteral("background: rgba(0,0,0,0.15); border-left: 3px solid %1; padding: 3px 8px; color: %2;"
                                         " font-style: italic; font-size: 11.5px; border-radius: 3px;").arg(it.color, Theme::textMuted()));
        if (!it.quote.trimmed().isEmpty()) bl->addWidget(rq); else rq->deleteLater();
        lbl(it.title, QStringLiteral("color: %1; font-size: 12.5px;").arg(ink));
    } else if (it.kind == Tab::Notes) {
        if (!it.title.isEmpty()) lbl(it.title, QStringLiteral("color: %1; font-size: 10.5px; font-weight: 700;").arg(it.color));
        lbl(it.body, QStringLiteral("color: %1; font-size: 12.5px;").arg(ink));
    } else {
        lbl(tr("↪ encaminhada de %1").arg(it.origin.isEmpty() ? tr("uma ficha") : it.origin),
            QStringLiteral("color: %1; font-size: 10.5px;").arg(Theme::textMuted()));
        lbl(quoted(it.quote), QStringLiteral("color: %1; font-size: 12.5px; font-style: italic;").arg(ink));
    }
    const QString tm = it.kind == Tab::Memories ? it.tags.join(QStringLiteral(" · ")) : fmtDate(it.created);
    auto* t = lbl(tm, QStringLiteral("color: %1; font-size: 9.5px;").arg(Theme::textMuted()));
    t->setAlignment(Qt::AlignRight);
    if (it.kind == Tab::Comments && m_toolReview && it.task)
        t->setText(tm + QStringLiteral("  ") + (it.done ? tr("✓ resolvido") : tr("tarefa")));
    if (me) {
        h->addStretch(1);
        h->addWidget(bub);
    } else {
        auto* av = new QLabel(row);
        av->setFixedSize(24, 24);
        if (it.kind == Tab::Dialogues)
            av->setPixmap(it.speakerId.isEmpty() ? unattributedAvatar(24) : characterAvatar(it.speakerId, 24));
        h->addWidget(av, 0, Qt::AlignBottom);
        h->addWidget(bub);
        h->addStretch(1);
    }
    wireItem(bub, it);
    return row;
}

QWidget* PensarioPanel::renderPullQuote(const PnItem& it, bool featured, QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(4);
    const QString c = it.kind == Tab::Dialogues ? speakerColor(it.speakerId).name() : it.color;
    auto* mark = new QLabel(QStringLiteral("“"), w);
    mark->setStyleSheet(QStringLiteral("color: %1; font-family: %2; font-size: 40px; font-weight: 600; background: transparent;")
        .arg(c, serifFamily()));
    mark->setFixedHeight(28);
    QString body, by;
    if (it.kind == Tab::Memories) { body = it.quote; by = it.title + (it.origin.isEmpty() ? QString() : QStringLiteral(" · ") + it.origin); }
    else if (it.kind == Tab::Dialogues) { body = it.body; by = it.title + QStringLiteral(" · ") + it.origin; }
    else if (it.kind == Tab::Comments) { body = it.title; by = it.origin; }
    else { body = it.body; by = it.title; }
    if (it.kind == Tab::Comments || it.kind == Tab::Notes) {
        // Comentários e notas: texto de abertura (o "lead"), sem aspas grandes.
        auto* lead = new QLabel(QStringLiteral("<b style=\"color:%1\">%2</b>%3").arg(tc(Theme::textBright()).name(),
            it.title.toHtmlEscaped(), it.kind == Tab::Comments && !it.quote.trimmed().isEmpty()
                ? QStringLiteral("<br><i style=\"color:%1\">%2</i>").arg(tc(Theme::textMuted()).name(), quoted(it.quote).toHtmlEscaped())
                : (it.kind == Tab::Notes ? QStringLiteral(" ") + it.body.toHtmlEscaped() : QString())), w);
        lead->setTextFormat(Qt::RichText);
        lead->setWordWrap(true);
        lead->setStyleSheet(QStringLiteral("color: %1; font-family: %2; font-size: %3px; background: transparent; border-left: 3px solid %4; padding-left: 10px;")
            .arg(Theme::textPrimary(), serifFamily()).arg(featured ? 17 : 15).arg(c));
        lead->setAttribute(Qt::WA_TransparentForMouseEvents);
        l->addWidget(lead);
        if (it.kind == Tab::Comments) {
            auto* b = new QLabel(by.toUpper(), w);
            b->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; letter-spacing: 1px; background: transparent;").arg(Theme::textMuted()));
            l->addWidget(b);
        }
    } else {
        l->addWidget(mark);
        auto* q = new QLabel(body.trimmed().remove(QChar(0x201C)).remove(QChar(0x201D)), w);
        q->setWordWrap(true);
        q->setStyleSheet(QStringLiteral("color: %1; font-family: %2; font-style: italic; font-size: %3px; background: transparent;")
            .arg(Theme::textBright(), serifFamily()).arg(featured ? 22 : 17));
        q->setAttribute(Qt::WA_TransparentForMouseEvents);
        l->addWidget(q);
        auto* b = new QLabel(by.toUpper(), w);
        b->setWordWrap(true);
        b->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; background: transparent;").arg(Theme::textMuted()));
        l->addWidget(b);
    }
    wireItem(w, it);
    return w;
}

QWidget* PensarioPanel::renderListRow(const PnItem& it, QWidget* parent) {
    auto* f = new QFrame(parent);
    f->setObjectName(QStringLiteral("pnRow"));
    const bool sel = (itemKey(it) == m_pickKey);
    f->setProperty("sel", sel);
    f->setStyleSheet(Theme::qss(QStringLiteral("QFrame#pnRow { background: transparent; border-radius: @radius-control; }"
                                               "QFrame#pnRow:hover { background: %1; }"
                                               "QFrame#pnRow[sel=\"true\"] { background: %2; }"))
        .arg(Theme::hoverOverlay(), Theme::accentInfoSoft()));
    auto* h = new QHBoxLayout(f);
    h->setContentsMargins(8, 6, 8, 6);
    h->setSpacing(9);
    if (it.kind == Tab::Dialogues) {
        auto* av = new QLabel(f);
        av->setPixmap(it.speakerId.isEmpty() ? unattributedAvatar(22) : characterAvatar(it.speakerId, 22));
        av->setFixedSize(22, 22);
        h->addWidget(av, 0, Qt::AlignTop);
    } else {
        auto* dot = new QLabel(f);
        dot->setFixedSize(9, 9);
        dot->setStyleSheet(QStringLiteral("background: %1; border-radius: 4px;").arg(it.kind == Tab::Memories ? Theme::textMuted() : it.color));
        h->addWidget(dot, 0, Qt::AlignTop);
    }
    auto* col = new QVBoxLayout;
    col->setSpacing(1);
    QString l1 = it.kind == Tab::Dialogues ? it.body : (it.kind == Tab::Notes && it.title.isEmpty() ? it.body : it.title);
    l1 = l1.simplified();
    if (l1.size() > 90) l1 = l1.left(88) + QStringLiteral("…");
    auto* a = new QLabel(l1, f);
    a->setWordWrap(true);
    a->setStyleSheet(QStringLiteral("color: %1; font-size: 12.5px; background: transparent;").arg(Theme::textBright()));
    col->addWidget(a);
    const QString l2 = it.kind == Tab::Memories ? it.tags.join(QStringLiteral(" · ")) : (it.kind == Tab::Notes ? QString() : it.origin);
    if (!l2.isEmpty()) {
        auto* b = new QLabel(l2, f);
        b->setStyleSheet(QStringLiteral("color: %1; font-size: 10.5px; background: transparent;").arg(Theme::textMuted()));
        col->addWidget(b);
    }
    h->addLayout(col, 1);
    for (auto* lbl : f->findChildren<QLabel*>()) lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
    f->setProperty("pnPickable", true);
    wireItem(f, it);
    return f;
}

void PensarioPanel::updateDetail() {
    if (!m_detailLay) return;
    while (QLayoutItem* it = m_detailLay->takeAt(0)) {
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        else if (QLayout* l = it->layout()) { while (QLayoutItem* c = l->takeAt(0)) { if (c->widget()) c->widget()->deleteLater(); delete c; } }
        delete it;
    }
    if (m_style != Style::TwoColumns) return;
    // Seleção visível nas linhas.
    for (QFrame* row : findChildren<QFrame*>(QStringLiteral("pnRow"))) {
        const bool sel = row->property("pnItemKey").toString() == m_pickKey;
        if (row->property("sel").toBool() != sel) {
            row->setProperty("sel", sel);
            row->style()->unpolish(row);
            row->style()->polish(row);
        }
    }
    if (!m_itemIndex.contains(m_pickKey)) {
        auto* e = new QLabel(tr("Escolha um item na lista."), m_detail);
        e->setStyleSheet(QStringLiteral("color: %1; font-style: italic;").arg(Theme::textMuted()));
        m_detailLay->addWidget(e);
        m_detailLay->addStretch(1);
        return;
    }
    const PnItem it = m_itemIndex.value(m_pickKey);
    auto lbl = [this](const QString& t, const QString& css) {
        auto* x = new QLabel(t, m_detail);
        x->setWordWrap(true);
        x->setStyleSheet(QStringLiteral("background: transparent; ") + css);
        x->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_detailLay->addWidget(x);
        return x;
    };
    if (it.kind == Tab::Dialogues) {
        auto* row = new QHBoxLayout;
        row->setSpacing(10);
        auto* av = new QLabel(m_detail);
        av->setPixmap(it.speakerId.isEmpty() ? unattributedAvatar(34) : characterAvatar(it.speakerId, 34));
        av->setFixedSize(34, 34);
        row->addWidget(av);
        auto* nm = new QLabel(QStringLiteral("<b>%1</b><br><span style=\"color:%2; font-size:11px\">%3</span>")
            .arg(it.title.toHtmlEscaped(), tc(Theme::textMuted()).name(), it.origin.toHtmlEscaped()), m_detail);
        nm->setStyleSheet(QStringLiteral("color: %1; font-size: 14px; background: transparent;").arg(Theme::textBright()));
        row->addWidget(nm, 1);
        m_detailLay->addLayout(row);
    } else {
        if (!it.origin.isEmpty()) lbl(it.origin, QStringLiteral("color: %1; font-size: 11px; font-weight: 600;").arg(Theme::accentDefault()));
        const QString t = it.title.isEmpty() ? (it.kind == Tab::Notes ? tr("Nota sem título") : QString()) : it.title;
        if (!t.isEmpty()) lbl(t, QStringLiteral("color: %1; font-family: 'Lora'; font-size: 17px; font-weight: 600;").arg(Theme::textBright()));
    }
    const QString q = it.kind == Tab::Notes ? it.body : (it.kind == Tab::Dialogues ? it.body : it.quote);
    if (!q.trimmed().isEmpty()) {
        const QString c = it.kind == Tab::Memories ? Theme::textMuted() : it.color;
        lbl(it.kind == Tab::Notes ? q : quoted(q), QStringLiteral("color: %1; font-family: %2; font-size: 15px; %3 border-left: 3px solid %4; padding: 4px 0 4px 12px;")
            .arg(Theme::textPrimary(), serifFamily(), it.kind == Tab::Notes ? QString() : QStringLiteral("font-style: italic;"), c));
    }
    if (!it.tags.isEmpty()) lbl(it.tags.join(QStringLiteral("  ·  ")), QStringLiteral("color: %1; font-size: 12px;").arg(Theme::textMuted()));
    if (it.kind == Tab::Comments && m_toolReview && it.task)
        lbl(it.done ? tr("Tarefa resolvida") : tr("Tarefa pendente"),
            QStringLiteral("color: %1; font-size: 11px; font-weight: 700;").arg(it.done ? Theme::accentSuccess() : Theme::accentWarning()));

    auto* acts = new QGridLayout;
    acts->setHorizontalSpacing(6);
    acts->setVerticalSpacing(6);
    int n = 0;
    auto act = [&](const QString& text, bool primary) {
        auto* b = new QPushButton(text, m_detail);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(Theme::qss(primary
            ? QStringLiteral("QPushButton { background: %1; color: #ffffff; border: none; border-radius: @radius-control; padding: 5px 12px; font-size: 12px; }").arg(Theme::accentDefault())
            : QStringLiteral("QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: @radius-control; padding: 5px 12px; font-size: 12px; }"
                             "QPushButton:hover { background: %3; }").arg(Theme::textPrimary(), Theme::subtleBorder(), Theme::hoverOverlay())));
        acts->addWidget(b, n / 3, n % 3);
        ++n;
        return b;
    };
    const PnItem cp = it;
    connect(act(it.kind == Tab::Notes ? tr("Editar") : tr("Abrir no editor"), true), &QPushButton::clicked, this,
            [this, cp]() { activateItem(cp, QCursor::pos()); });
    if (it.kind == Tab::Memories) {
        connect(act(tr("Abrir no RefMenu"), false), &QPushButton::clicked, this, [this, cp]() {
            if (!m_memories) return;
            for (const auto& m : m_memories->memories()) if (m.id == cp.id) { emit openMemoryInRefRequested(m); break; }
        });
    }
    if (it.kind == Tab::Dialogues)
        connect(act(tr("Alterar locutor"), false), &QPushButton::clicked, this, [this, cp]() { showChangeSpeakerPopup(cp.id, QCursor::pos()); });
    if (m_toolInsert && !insertableText(it).isEmpty())
        connect(act(tr("Levar pro texto"), false), &QPushButton::clicked, this, [this, cp]() { emit insertTextRequested(insertableText(cp)); });
    if (it.kind == Tab::Comments && m_toolReview) {
        if (it.task)
            connect(act(it.done ? tr("Reabrir") : tr("Resolver"), false), &QPushButton::clicked, this,
                    [this, cp]() { if (m_markers) m_markers->setDone(cp.docKey, cp.id, !cp.done); });
        else
            connect(act(tr("Virar tarefa"), false), &QPushButton::clicked, this,
                    [this, cp]() { if (m_markers) m_markers->setTask(cp.docKey, cp.id, true); });
    }
    acts->setColumnStretch(3, 1);
    m_detailLay->addLayout(acts);
    m_detailLay->addStretch(1);
}

QWidget* PensarioPanel::renderDeck(Tab tab, const QVector<PnItem>& items, QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(8, 10, 8, 4);
    l->setSpacing(12);
    if (items.isEmpty()) return w;
    int& idx = m_deckIndex[int(tab)];
    idx = qBound(0, idx, int(items.size()) - 1);
    const PnItem it = items.at(idx);
    auto* stack = new QWidget(w);
    auto* sl = new QGridLayout(stack);
    sl->setContentsMargins(0, 0, 0, 0);
    auto* back = new PnDeckBack(stack);
    sl->addWidget(back, 0, 0);
    auto* big = new QFrame(stack);
    big->setObjectName(QStringLiteral("pnBig"));
    const QString c = it.kind == Tab::Memories ? QStringLiteral("#8fc7a4") : (it.kind == Tab::Dialogues ? Theme::accentDefault() : it.color);
    big->setStyleSheet(Theme::qss(QStringLiteral("QFrame#pnBig { background: %1; border: 1px solid %2; border-top: 5px solid %3; border-radius: 10px; }"))
        .arg(Theme::inputBackground(), Theme::borderStrong(), c));
    big->setMinimumHeight(300);
    auto* bl = new QVBoxLayout(big);
    bl->setContentsMargins(22, 18, 22, 16);
    bl->setSpacing(12);
    bl->addWidget(capsLabel(tr("%1 %2 de %3").arg(tabName(tab)).arg(idx + 1).arg(items.size()), big));
    auto lbl = [&](const QString& t, const QString& css) {
        auto* x = new QLabel(t, big);
        x->setWordWrap(true);
        x->setAttribute(Qt::WA_TransparentForMouseEvents);
        x->setStyleSheet(QStringLiteral("background: transparent; ") + css);
        bl->addWidget(x);
        return x;
    };
    if (it.kind == Tab::Dialogues) {
        auto* row = new QHBoxLayout;
        auto* av = new QLabel(big);
        av->setPixmap(it.speakerId.isEmpty() ? unattributedAvatar(30) : characterAvatar(it.speakerId, 30));
        av->setFixedSize(30, 30);
        row->addWidget(av);
        auto* nm = new QLabel(it.title, big);
        nm->setStyleSheet(QStringLiteral("background: transparent; color: %1; font-size: 14px; font-weight: 700;").arg(Theme::textBright()));
        row->addWidget(nm, 1);
        bl->addLayout(row);
    } else if (!it.title.isEmpty()) {
        lbl(it.title, QStringLiteral("color: %1; font-size: 15px; %2").arg(Theme::textBright(), it.kind == Tab::Comments ? QString() : QStringLiteral("font-weight: 600;")));
    }
    const QString q = it.kind == Tab::Notes || it.kind == Tab::Dialogues ? it.body : it.quote;
    auto* bq = lbl(it.kind == Tab::Notes ? q : quoted(q), QStringLiteral("color: %1; font-family: %2; font-style: italic; font-size: 19px;")
        .arg(Theme::textBright(), serifFamily()));
    bq->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    bq->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    QString o = it.origin;
    if (it.kind == Tab::Memories && !it.tags.isEmpty()) o += (o.isEmpty() ? QString() : QStringLiteral(" · ")) + it.tags.join(QStringLiteral(", "));
    if (it.kind == Tab::Notes) o = tr("Nota");
    lbl(o, QStringLiteral("color: %1; font-size: 11px;").arg(Theme::textMuted()));
    wireItem(big, it);
    sl->addWidget(big, 0, 0);
    l->addWidget(stack);

    auto* nav = new QHBoxLayout;
    nav->setSpacing(10);
    nav->addStretch(1);
    auto mkArrow = [&](const QString& t, int step) {
        auto* b = new QToolButton(w);
        b->setText(t);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(32, 32);
        b->setStyleSheet(Theme::qss(QStringLiteral("QToolButton { color: %1; background: transparent; border: 1px solid %2; border-radius: 16px; font-size: 16px; }"
                                                   "QToolButton:hover { color: %3; border-color: %3; }"))
            .arg(Theme::textPrimary(), Theme::subtleBorder(), Theme::textBright()));
        const int total = items.size();
        connect(b, &QToolButton::clicked, this, [this, tab, step, total]() {
            m_deckIndex[int(tab)] = (m_deckIndex[int(tab)] + step + total) % qMax(1, total);
            selectTab(tab);
        });
        return b;
    };
    nav->addWidget(mkArrow(QStringLiteral("‹"), -1));
    if (items.size() <= 14) {
        for (int i = 0; i < items.size(); ++i) {
            auto* d = new QToolButton(w);
            d->setFixedSize(10, 10);
            d->setCursor(Qt::PointingHandCursor);
            d->setStyleSheet(QStringLiteral("QToolButton { background: %1; border: none; border-radius: 4px; }")
                .arg(i == idx ? Theme::accentDefault() : Theme::subtleBorder()));
            connect(d, &QToolButton::clicked, this, [this, tab, i]() { m_deckIndex[int(tab)] = i; selectTab(tab); });
            nav->addWidget(d);
        }
    } else {
        auto* pos = new QLabel(QStringLiteral("%1 / %2").arg(idx + 1).arg(items.size()), w);
        pos->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(Theme::textMuted()));
        nav->addWidget(pos);
    }
    nav->addWidget(mkArrow(QStringLiteral("›"), 1));
    nav->addStretch(1);
    l->addLayout(nav);
    return w;
}

QWidget* PensarioPanel::renderTable(Tab tab, const QVector<PnItem>& items, QWidget* parent) {
    auto* t = new QTableWidget(items.size(), 4, parent);
    t->setHorizontalHeaderLabels({ QString(), tr("TEXTO"), tr("CAPÍTULO"), tr("DATA") });
    t->verticalHeader()->hide();
    t->setShowGrid(false);
    t->setWordWrap(false);
    t->setTextElideMode(Qt::ElideRight);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->setContextMenuPolicy(Qt::CustomContextMenu);
    t->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->horizontalHeader()->setHighlightSections(false);
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    t->setColumnWidth(0, 22);
    t->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    t->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    t->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QTableWidget { background: transparent; color: %1; border: none; font-size: 11.5px; }
        QTableWidget::item { padding: 0px 4px; border-bottom: 1px solid %2; }
        QHeaderView::section { background: %3; color: %4; border: none; border-bottom: 1px solid %2;
                               padding: 4px; font-size: 9.5px; font-weight: 700; }
    )")).arg(Theme::textPrimary(), Theme::subtleBorder(), Theme::appBackground(), Theme::textMuted()));
    t->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    for (int r = 0; r < items.size(); ++r) {
        const PnItem& it = items.at(r);
        auto* c0 = new PnSortItem();
        if (it.kind == Tab::Dialogues) {
            c0->setIcon(QIcon(it.speakerId.isEmpty() ? unattributedAvatar(16) : characterAvatar(it.speakerId, 16)));
        } else {
            QPixmap pm(12, 12);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(it.kind == Tab::Memories ? Theme::textMuted() : it.color));
            p.drawRoundedRect(QRectF(1, 1, 10, 10), 3, 3);
            p.end();
            c0->setIcon(QIcon(pm));
        }
        c0->setData(Qt::UserRole, itemKey(it));
        c0->setData(Qt::UserRole + 1, it.color);
        t->setItem(r, 0, c0);
        QString main = it.kind == Tab::Dialogues ? it.body : (it.kind == Tab::Notes && it.title.isEmpty() ? it.body : it.title);
        QString sub = it.kind == Tab::Comments || it.kind == Tab::Memories ? quoted(it.quote) : QString();
        if (it.kind == Tab::Comments && m_toolReview && it.task) main = (it.done ? QStringLiteral("✓ ") : QStringLiteral("☐ ")) + main;
        // Uma linha só; o trecho vai pro tooltip.
        auto* c1 = new PnSortItem(main.simplified());
        if (!sub.isEmpty()) c1->setToolTip(sub);
        c1->setData(Qt::UserRole, itemKey(it));
        c1->setData(Qt::UserRole + 1, main);
        c1->setForeground(tc(Theme::textBright()));
        t->setItem(r, 1, c1);
        QString chap = QStringLiteral("—");
        int rank = std::numeric_limits<int>::max();
        if (!it.chapterId.isEmpty() && m_model) {
            if (const Chapter* ch = m_model->findChapter(it.chapterId)) {
                const QString n = m_model->chapterNumberLabel(*ch);
                chap = (ch->type == QStringLiteral("chapter") && !n.isEmpty()) ? tr("cap. %1").arg(n)
                                                                              : m_model->chapterDisplayLabel(*ch);
                rank = rankForKey(DocCache::chapterKey(ch->manuscriptId, ch->id));
            }
        }
        auto* c2 = new PnSortItem(chap);
        c2->setData(Qt::UserRole, itemKey(it));
        c2->setData(Qt::UserRole + 1, rank);
        c2->setForeground(tc(Theme::textMuted()));
        t->setItem(r, 2, c2);
        auto* c3 = new PnSortItem(fmtDate(it.created));
        c3->setData(Qt::UserRole, itemKey(it));
        c3->setData(Qt::UserRole + 1, double(it.created));
        c3->setForeground(tc(Theme::textMuted()));
        t->setItem(r, 3, c3);
    }
    t->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    t->setSortingEnabled(true);
    t->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    t->verticalHeader()->setDefaultSectionSize(26);
    const int h = t->horizontalHeader()->sizeHint().height() + 2 + 26 * t->rowCount();
    t->setFixedHeight(h);
    connect(t, &QTableWidget::cellClicked, this, [this, t](int r, int) {
        if (auto* c = t->item(r, 1)) activateItem(m_itemIndex.value(c->data(Qt::UserRole).toString()), QCursor::pos());
    });
    connect(t, &QTableWidget::customContextMenuRequested, this, [this, t](const QPoint& pos) {
        if (auto* c = t->itemAt(pos))
            showItemMenu(m_itemIndex.value(c->data(Qt::UserRole).toString()), t->viewport()->mapToGlobal(pos));
    });
    Q_UNUSED(tab);
    return t;
}

// ============================================================ ações

void PensarioPanel::activateItem(const PnItem& it, const QPoint& globalPos) {
    switch (it.kind) {
    case Tab::Comments:
        emit openMarkerRequested(it.docKey, it.start, it.end, it.quote);
        break;
    case Tab::Notes:
        openNoteEditById(it.id);
        break;
    case Tab::Memories:
        showMemoryActions(it.id, globalPos);
        break;
    case Tab::Dialogues:
        if (m_dialogues)
            for (const auto& d : m_dialogues->dialogues())
                if (d.id == it.id) { emit openDialogueInEditorRequested(d); break; }
        break;
    default: break;
    }
}

void PensarioPanel::showItemMenu(const PnItem& it, const QPoint& globalPos) {
    if (it.id.isEmpty()) return;
    QMenu menu(this);
    menu.setStyleSheet(menuQss());
    const PnItem cp = it;
    auto add = [&](const QString& t, std::function<void()> fn) { connect(menu.addAction(t), &QAction::triggered, this, fn); };
    switch (it.kind) {
    case Tab::Comments:
        add(tr("Abrir no editor"), [this, cp]() { activateItem(cp, QCursor::pos()); });
        if (m_toolReview) {
            menu.addSeparator();
            if (it.task) {
                add(it.done ? tr("Reabrir") : tr("Marcar como resolvido"), [this, cp]() { if (m_markers) m_markers->setDone(cp.docKey, cp.id, !cp.done); });
                add(tr("Não é tarefa"), [this, cp]() { if (m_markers) m_markers->setTask(cp.docKey, cp.id, false); });
            } else {
                add(tr("Marcar como tarefa"), [this, cp]() { if (m_markers) m_markers->setTask(cp.docKey, cp.id, true); });
            }
        }
        break;
    case Tab::Notes:
        add(tr("Editar"), [this, cp]() { openNoteEditById(cp.id); });
        if (m_toolInsert) add(tr("Levar pro texto"), [this, cp]() { emit insertTextRequested(insertableText(cp)); });
        menu.addSeparator();
        add(tr("Excluir nota"), [this, cp]() { if (m_notesStore) m_notesStore->removeNote(cp.id); });
        break;
    case Tab::Memories:
        add(tr("Abrir no editor"), [this, cp]() {
            if (!m_memories) return;
            for (const auto& m : m_memories->memories()) if (m.id == cp.id) { emit openMemoryInEditorRequested(m); break; }
        });
        add(tr("Abrir no menu de referência"), [this, cp]() {
            if (!m_memories) return;
            for (const auto& m : m_memories->memories()) if (m.id == cp.id) { emit openMemoryInRefRequested(m); break; }
        });
        if (m_toolInsert) add(tr("Levar pro texto"), [this, cp]() { emit insertTextRequested(insertableText(cp)); });
        menu.addSeparator();
        add(tr("Excluir memória"), [this, cp]() { if (m_memories) m_memories->remove(cp.id); });
        break;
    case Tab::Dialogues:
        add(tr("Abrir no editor"), [this, cp]() { activateItem(cp, QCursor::pos()); });
        add(cp.speakerId.isEmpty() ? tr("Atribuir ao personagem…") : tr("Alterar locutor…"),
            [this, cp, globalPos]() { showChangeSpeakerPopup(cp.id, globalPos); });
        if (m_toolInsert) add(tr("Levar pro texto"), [this, cp]() { emit insertTextRequested(insertableText(cp)); });
        break;
    default: break;
    }
    menu.exec(globalPos);
}

// ============================================================ Cores com nome / Revisão

QString PensarioPanel::colorName(const QString& hex) const {
    if (!m_model) return QString();
    const QJsonObject names = m_model->settings().value(QStringLiteral("pensarioColorNames")).toObject();
    return names.value(QColor(hex).name()).toString();
}

void PensarioPanel::editColorLegend() {
    if (!m_model) return;
    QSet<QString> colors;
    for (const PnItem& it : collectComments(false)) colors.insert(QColor(it.color).name());
    for (const PnItem& it : collectNotes(false)) colors.insert(QColor(it.color).name());
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Legenda das cores"));
    dlg.setStyleSheet(Theme::qss(QStringLiteral(
        "QDialog { background: %1; color: %2; }"
        "QLineEdit { background: %3; color: %2; border: 1px solid %4; border-radius: @radius-control; padding: 5px 8px; }"
        "QPushButton { background: %5; color: #ffffff; border: none; border-radius: @radius-control; padding: 6px 14px; }"))
        .arg(Theme::panelBackground(), Theme::textPrimary(), Theme::inputBackground(), Theme::subtleBorder(), Theme::accentDefault()));
    auto* l = new QVBoxLayout(&dlg);
    auto* hint = new QLabel(tr("Dê um nome pra cada cor. Ele vale pro projeto inteiro."), &dlg);
    hint->setWordWrap(true);
    l->addWidget(hint);
    auto* grid = new QGridLayout;
    QList<QPair<QString, QLineEdit*>> edits;
    QStringList sorted(colors.begin(), colors.end());
    std::sort(sorted.begin(), sorted.end());
    int r = 0;
    for (const QString& c : sorted) {
        auto* sw = new QLabel(&dlg);
        sw->setFixedSize(18, 18);
        sw->setStyleSheet(QStringLiteral("background: %1; border-radius: 4px;").arg(c));
        grid->addWidget(sw, r, 0);
        auto* e = new QLineEdit(colorName(c), &dlg);
        e->setPlaceholderText(tr("ex.: Conferir, Problema, Ritmo"));
        grid->addWidget(e, r++, 1);
        edits.append({ c, e });
    }
    l->addLayout(grid);
    auto* ok = new QPushButton(tr("Salvar"), &dlg);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    l->addWidget(ok, 0, Qt::AlignRight);
    if (dlg.exec() != QDialog::Accepted) return;
    QJsonObject s = m_model->settings();
    QJsonObject names = s.value(QStringLiteral("pensarioColorNames")).toObject();
    for (const auto& e : edits) {
        const QString v = e.second->text().trimmed();
        if (v.isEmpty()) names.remove(e.first); else names.insert(e.first, v);
    }
    s.insert(QStringLiteral("pensarioColorNames"), names);
    m_model->setSettings(s);
    selectTab(m_tab);
}

QWidget* PensarioPanel::buildColorLegendBar(Tab tab, const QVector<PnItem>& items, QWidget* parent) {
    Q_UNUSED(tab);
    QVector<PnItem> all = tab == Tab::Comments ? collectComments(false) : collectNotes(false);
    QMap<QString, int> counts;
    for (const PnItem& it : all) counts[QColor(it.color).name()] += 1;
    Q_UNUSED(items);
    auto* w = new QWidget(parent);
    auto* g = new QGridLayout(w);
    g->setContentsMargins(0, 0, 0, 2);
    g->setHorizontalSpacing(4);
    g->setVerticalSpacing(4);
    int n = 0;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        const QString name = colorName(it.key());
        auto* b = new QToolButton(w);
        b->setText(QStringLiteral("%1  %2").arg(name.isEmpty() ? tr("sem nome") : name).arg(it.value()));
        QPixmap pm(12, 12);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(it.key()));
        p.drawEllipse(QRectF(1, 1, 10, 10));
        p.end();
        b->setIcon(QIcon(pm));
        b->setIconSize(QSize(10, 10));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setCursor(Qt::PointingHandCursor);
        const bool on = QColor(m_colorFilter) == QColor(it.key()) && !m_colorFilter.isEmpty();
        b->setStyleSheet(chipQss(on, it.key()));
        b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        const QString c = it.key();
        connect(b, &QToolButton::clicked, this, [this, c]() {
            m_colorFilter = (QColor(m_colorFilter) == QColor(c)) ? QString() : c;
            selectTab(m_tab);
        });
        g->addWidget(b, n / 3, n % 3);
        ++n;
    }
    auto* edit = new QToolButton(w);
    edit->setText(tr("Editar legenda"));
    edit->setCursor(Qt::PointingHandCursor);
    edit->setStyleSheet(QStringLiteral("QToolButton { color: %1; background: transparent; border: none; font-size: 11px; }"
                                       "QToolButton:hover { color: %2; }").arg(Theme::textMuted(), Theme::textBright()));
    connect(edit, &QToolButton::clicked, this, &PensarioPanel::editColorLegend);
    g->addWidget(edit, n / 3, n % 3);
    return w;
}

QWidget* PensarioPanel::buildReviewBar(QWidget* parent) {
    const QVector<PnItem> all = collectComments(false);
    int tasks = 0, done = 0;
    for (const PnItem& it : all) if (it.task) { ++tasks; if (it.done) ++done; }
    auto* w = new QWidget(parent);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 4);
    l->setSpacing(6);
    auto* seg = new QHBoxLayout;
    seg->setSpacing(4);
    const QList<QPair<QString, QString>> opts = { { QStringLiteral("tasks"), tr("Tarefas") },
                                                  { QStringLiteral("comments"), tr("Comentários") },
                                                  { QStringLiteral("all"), tr("Todos") } };
    for (const auto& o : opts) {
        auto* b = new QToolButton(w);
        b->setText(o.second);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(chipQss(m_taskFilter == o.first));
        const QString v = o.first;
        connect(b, &QToolButton::clicked, this, [this, v]() { m_taskFilter = v; selectTab(Tab::Comments); });
        seg->addWidget(b);
    }
    seg->addStretch(1);
    l->addLayout(seg);
    if (tasks > 0) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* n = new QLabel(QStringLiteral("%1/%2").arg(done).arg(tasks), w);
        n->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; background: transparent;").arg(Theme::textBright()));
        row->addWidget(n);
        auto* bar = new QFrame(w);
        bar->setFixedHeight(5);
        bar->setStyleSheet(QStringLiteral("background: %1; border-radius: 2px;").arg(Theme::panelBorder()));
        auto* fill = new QFrame(bar);
        fill->setStyleSheet(QStringLiteral("background: %1; border-radius: 2px;").arg(Theme::accentSuccess()));
        fill->setGeometry(0, 0, 0, 5);
        bar->installEventFilter(this);
        bar->setProperty("pnProgress", tasks ? double(done) / tasks : 0.0);
        row->addWidget(bar, 1);
        auto* hide = new QToolButton(w);
        hide->setText(tr("esconder resolvidos"));
        hide->setCursor(Qt::PointingHandCursor);
        hide->setStyleSheet(chipQss(m_hideDone));
        connect(hide, &QToolButton::clicked, this, [this]() { m_hideDone = !m_hideDone; selectTab(Tab::Comments); });
        row->addWidget(hide);
        l->addLayout(row);
    } else if (m_taskFilter == QStringLiteral("tasks")) {
        auto* hint = new QLabel(tr("Nenhuma tarefa ainda. Marque \"Tarefa\" ao criar um comentário, ou use o botão direito num comentário."), w);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; background: transparent;").arg(Theme::textMuted()));
        l->addWidget(hint);
    }
    return w;
}

// ============================================================ Busca, Lente e Quadro

void PensarioPanel::rebuildSearchPage() {
    if (!m_altLay) return;
    while (QLayoutItem* it = m_altLay->takeAt(0)) {
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        delete it;
    }
    QWidget* inner = m_altScroll->widget();
    const QString q = m_searchQuery.trimmed();
    auto hits = [&q](const PnItem& it) {
        return it.title.contains(q, Qt::CaseInsensitive) || it.body.contains(q, Qt::CaseInsensitive)
            || it.quote.contains(q, Qt::CaseInsensitive) || it.tags.join(QLatin1Char(' ')).contains(q, Qt::CaseInsensitive);
    };
    int total = 0;
    const QList<QPair<Tab, QVector<PnItem>>> groups = {
        { Tab::Comments, collectComments(false) }, { Tab::Notes, collectNotes(false) },
        { Tab::Memories, collectMemories() }, { Tab::Dialogues, collectDialogues(QString(), 0) } };
    const Style saved = m_style;
    for (const auto& g : groups) {
        QVector<PnItem> found;
        for (const PnItem& it : g.second) if (hits(it)) { PnItem c = it; c.group.clear(); found.append(c); }
        if (found.isEmpty()) continue;
        total += found.size();
        auto* h = capsLabel(QStringLiteral("%1 · %2").arg(tabName(g.first)).arg(found.size()), inner);
        m_altLay->addWidget(h);
        if (found.size() > 40) found.resize(40);
        m_style = Style::Classic;   // resultado sai em cartões, qualquer que seja o estilo
        renderItems(g.first, found, m_altLay, inner);
        m_style = saved;
    }
    if (!total) {
        auto* e = new QLabel(tr("Nada encontrado no Pensário."), inner);
        e->setObjectName(QStringLiteral("pnEmpty"));
        e->setAlignment(Qt::AlignCenter);
        m_altLay->addWidget(e);
    }
    m_altLay->addStretch(1);
}

void PensarioPanel::rebuildLensPage() {
    if (!m_altLay) return;
    while (QLayoutItem* it = m_altLay->takeAt(0)) {
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        delete it;
    }
    QWidget* inner = m_altScroll->widget();
    QString title = tr("O livro inteiro");
    if (!m_lensChapter.isEmpty() && m_model)
        if (const Chapter* ch = m_model->findChapter(m_lensChapter))
            title = m_model->chapterDisplayLabel(*ch) + (ch->title.isEmpty() ? QString() : QStringLiteral(" · ") + ch->title);
    m_altLay->addWidget(capsLabel(title, inner, Theme::textBright()));
    const Style saved = m_style;
    int total = 0;
    auto section = [&](Tab t, QVector<PnItem> items) {
        if (!m_lensChapter.isEmpty()) {
            QVector<PnItem> f;
            for (const PnItem& it : items) if (it.chapterId == m_lensChapter) { PnItem c = it; c.group.clear(); f.append(c); }
            items = f;
        }
        if (items.isEmpty()) return;
        total += items.size();
        m_altLay->addWidget(capsLabel(QStringLiteral("%1 · %2").arg(tabName(t)).arg(items.size()), inner));
        if (items.size() > 60) items.resize(60);
        m_style = Style::Classic;
        renderItems(t, items, m_altLay, inner);
        m_style = saved;
    };
    section(Tab::Comments, collectComments(false));
    section(Tab::Memories, collectMemories());
    section(Tab::Dialogues, collectDialogues(m_lensChapter, m_lensChapter.isEmpty() ? 60 : 0));
    if (!total) {
        auto* e = new QLabel(tr("Nada anotado neste capítulo ainda."), inner);
        e->setObjectName(QStringLiteral("pnEmpty"));
        e->setAlignment(Qt::AlignCenter);
        m_altLay->addWidget(e);
    }
    m_altLay->addStretch(1);
}

void PensarioPanel::rebuildBoard() {
    if (!m_boardLay) return;
    while (QLayoutItem* it = m_boardLay->takeAt(0)) {
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        delete it;
    }
    const QList<QPair<Tab, QColor>> all = { { Tab::Comments, QColor("#FFD54F") }, { Tab::Notes, QColor("#b39ddb") },
                                            { Tab::Memories, QColor("#8fc7a4") }, { Tab::Dialogues, QColor("#90caf9") } };
    // Duas seções por vez: quatro colunas finas quebravam até fala curta em
    // várias linhas. As setas andam pelas quatro.
    m_boardStart = ((m_boardStart % 4) + 4) % 4;
    const QList<QPair<Tab, QColor>> cols = { all.at(m_boardStart), all.at((m_boardStart + 1) % 4) };
    auto arrow = [this](const QString& t, int step) {
        auto* b = new QToolButton(m_board);
        b->setText(t);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedWidth(22);
        b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        b->setToolTip(step < 0 ? tr("Seções anteriores") : tr("Próximas seções"));
        b->setStyleSheet(Theme::qss(QStringLiteral("QToolButton { color: %1; background: transparent; border: none; border-radius: @radius-control; font-size: 18px; }"
                                                   "QToolButton:hover { background: %2; color: %3; }"))
            .arg(Theme::textMuted(), Theme::hoverOverlay(), Theme::textBright()));
        connect(b, &QToolButton::clicked, this, [this, step]() { m_boardStart += step; rebuildBoard(); });
        return b;
    };
    m_boardLay->addWidget(arrow(QString(QChar(0x2039)), -1));
    for (const auto& c : cols) {
        auto* col = new QFrame(m_board);
        col->setObjectName(QStringLiteral("pnBoardCol"));
        col->setStyleSheet(Theme::qss(QStringLiteral("QFrame#pnBoardCol { background: %1; border: 1px solid %2; border-top: 3px solid %3; border-radius: 8px; }"))
            .arg(Theme::appBackground(), Theme::subtleBorder(), c.second.name()));
        auto* cl = new QVBoxLayout(col);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(0);
        auto* head = new QWidget(col);
        auto* hl = new QHBoxLayout(head);
        hl->setContentsMargins(10, 8, 10, 6);
        auto* ic = new QLabel(head);
        ic->setPixmap(IconUtils::loadToolbarIcon(tabIcon(c.first), tc(Theme::textPrimary()), tc(Theme::textPrimary()),
                                                 tc(Theme::textPrimary()), QSize(13, 13)).pixmap(13, 13));
        hl->addWidget(ic);
        hl->addWidget(capsLabel(tabName(c.first), head, Theme::textPrimary()), 1);
        auto* n = new QLabel(QString::number(tabCount(c.first)), head);
        n->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; background: transparent;").arg(Theme::textMuted()));
        hl->addWidget(n);
        cl->addWidget(head);
        auto* sc = new QScrollArea(col);
        sc->setWidgetResizable(true);
        sc->setFrameShape(QFrame::NoFrame);
        sc->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        sc->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
        auto* inner = new QWidget(sc);
        auto* il = new QVBoxLayout(inner);
        il->setContentsMargins(8, 4, 8, 10);
        il->setSpacing(6);
        QVector<PnItem> items;
        if (c.first == Tab::Comments) items = collectComments(true);
        else if (c.first == Tab::Notes) items = collectNotes(true);
        else if (c.first == Tab::Memories) items = collectMemories();
        else items = collectDialogues(m_currentChapterId, m_currentChapterId.isEmpty() ? 60 : 0);
        for (PnItem& it : items) it.group.clear();
        if (c.first == Tab::Notes) {
            auto* add = new QToolButton(inner);
            add->setObjectName(QStringLiteral("pnAddNote"));
            add->setText(tr("+ Nova nota"));
            add->setCursor(Qt::PointingHandCursor);
            add->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            connect(add, &QToolButton::clicked, this, &PensarioPanel::openNoteCreate);
            il->addWidget(add);
        }
        if (items.isEmpty()) {
            auto* e = new QLabel(tr("Nada aqui ainda."), inner);
            e->setObjectName(QStringLiteral("pnEmpty"));
            e->setAlignment(Qt::AlignCenter);
            il->addWidget(e);
        } else {
            renderItems(c.first, items, il, inner);
        }
        il->addStretch(1);
        sc->setWidget(inner);
        cl->addWidget(sc, 1);
        m_boardLay->addWidget(col, 1);
    }
    m_boardLay->addWidget(arrow(QString(QChar(0x203A)), 1));
}
