// Estilos e ferramentas da gaveta (DrawerListPanel): menu ⋯, Retratos,
// Polaroid, Crachás, Galeria, Dossiê, Tabela, Lado a lado, Aparições e ficha no hover.
// O Clássico continua em DrawerListPanel.cpp.
#include "DrawerListPanel.h"
#include "CoverUtils.h"
#include "DrawerViews.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "ManuscriptViews.h"
#include "ProjectModel.h"
#include "ProjectStorage.h"
#include "RoleTiers.h"
#include "PanelMotion.h"
#include "Theme.h"
#include "DocCache.h"

#include <QActionGroup>
#include <QBuffer>
#include <QCursor>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMap>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QColor tcol(const QString& css) { return Theme::toColor(css); }

QString pngTag(const QPixmap& pm, int w, int h) {
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    pm.save(&buf, "PNG");
    return QStringLiteral("<img src=\"data:image/png;base64,%1\" width=\"%2\" height=\"%3\">")
        .arg(QString::fromLatin1(bytes.toBase64())).arg(w).arg(h);
}

QString dotTag(const QColor& c) {
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(QRectF(2, 2, 12, 12));
    p.end();
    return pngTag(pm, 8, 8);
}

QPixmap appearsPixmap(const QList<bool>& ap, const QColor& on, int w, int h, qreal dpr) {
    QPixmap pm(QSize(w, h) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    if (ap.isEmpty()) return pm;
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal gap = 2, cw = (w - gap * (ap.size() - 1)) / ap.size();
    for (int i = 0; i < ap.size(); ++i) {
        QColor c = ap.at(i) ? on : tcol(Theme::textPrimary());
        if (!ap.at(i)) c.setAlphaF(0.14);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(QRectF(i * (cw + gap), 0, cw, h), 1.5, 1.5);
    }
    return pm;
}

QLabel* caps(const QString& text, QWidget* parent, const QString& color = QString()) {
    auto* l = new QLabel(text.toUpper(), parent);
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(10);
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.9);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(color.isEmpty() ? Theme::textMuted() : color));
    return l;
}

QString chipQss(bool on) {
    return Theme::qss(QStringLiteral(
        "QToolButton { color: %1; background: %2; border: 1px solid %3; border-radius: @radius-control;"
        " padding: 3px 10px; font-size: 11.5px; }"
        "QToolButton:hover { color: %4; border-color: %5; }"))
        .arg(on ? Theme::textBright() : Theme::textPrimary(),
             on ? Theme::accentInfoSoft() : QStringLiteral("transparent"),
             on ? Theme::accentInfoBorderSoft() : Theme::subtleBorder(),
             Theme::textBright(), Theme::borderStrong());
}

QString roleOrderKey(const QString& roleId) {
    static const QStringList order = {
        QStringLiteral("PROTAGONISTA"), QStringLiteral("DEUTERAGONISTA"), QStringLiteral("ANTAGONISTA"),
        QStringLiteral("CONTRAPONTO"), QStringLiteral("MENTOR"), QStringLiteral("TRICKSTER"),
        QStringLiteral("COADJUVANTE"), QStringLiteral("FIGURANTE") };
    const QString r = roleId.trimmed().toUpper();
    const int i = order.indexOf(r);
    if (r.isEmpty()) return QStringLiteral("z");
    return i >= 0 ? QStringLiteral("a%1").arg(i, 2, 10, QLatin1Char('0')) : QStringLiteral("m") + r;
}

// Célula de tabela que ordena por uma chave própria (número, índice).
class SortItem : public QTableWidgetItem {
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

QString DrawerListPanel::styleId(Style s) {
    switch (s) {
    case Style::Classic:   return QStringLiteral("classic");
    case Style::Portraits: return QStringLiteral("portraits");
    case Style::Polaroid:  return QStringLiteral("polaroid");
    case Style::Badges:    return QStringLiteral("badges");
    case Style::Gallery:   return QStringLiteral("gallery");
    case Style::Dossier:   return QStringLiteral("dossier");
    case Style::Table:     return QStringLiteral("table");
    }
    return QStringLiteral("classic");
}

bool DrawerListPanel::styleAllowed(Style s) const {
    switch (s) {
    case Style::Classic:
    case Style::Dossier:
    case Style::Table:     return true;
    case Style::Portraits:
    case Style::Polaroid:
    case Style::Badges:
    case Style::Gallery:   return currentDrawerIsElement();
    }
    return false;
}

DrawerListPanel::Style DrawerListPanel::storedStyleFor(const QString& drawerKey) const {
    const QString id = QSettings().value(QStringLiteral("ui/drawerStyle/") + drawerKey).toString();
    for (Style s : { Style::Classic, Style::Portraits, Style::Polaroid, Style::Badges, Style::Gallery,
                     Style::Dossier, Style::Table })
        if (styleId(s) == id) return styleAllowed(s) ? s : Style::Classic;
    return Style::Classic;
}

QString DrawerListPanel::widthKeyFor(Style s) const {
    // O Clássico continua na chave antiga: quem atualiza não perde a largura.
    if (s == Style::Classic) return QStringLiteral("ui/drawerListPanelWidth");
    return QStringLiteral("ui/drawerListPanelWidth-") + styleId(s);
}

int DrawerListPanel::defaultWidthFor(Style s) const {
    switch (s) {
    case Style::Classic:   return 300;
    case Style::Portraits: return 340;
    case Style::Polaroid:  return 340;
    case Style::Badges:    return 340;
    case Style::Gallery:   return 360;
    case Style::Dossier:   return 640;
    case Style::Table:     return 600;
    }
    return 300;
}

void DrawerListPanel::applyStyleWidth() {
    const int stored = QSettings().value(widthKeyFor(m_style), 0).toInt();
    const int w = stored > 0 ? qBound(240, stored, 900) : defaultWidthFor(m_style);
    if (w != width()) {
        resize(w, height());
        emit panelWidthChanged();
    }
}

void DrawerListPanel::setStyle(Style s) {
    if (!styleAllowed(s)) return;
    const bool animate = isVisible() && s != m_style;
    if (animate) PanelMotion::swapOut(motionArea());
    m_style = s;
    QSettings().setValue(QStringLiteral("ui/drawerStyle/") + m_currentKey, styleId(s));
    m_compareA.clear();
    m_compareB.clear();
    applyStyleWidth();
    rebuildContents();
    if (animate) playIntro(60);
}

QString DrawerListPanel::menuQss() const {
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

void DrawerListPanel::showMoreMenu() {
    if (m_currentKey.isEmpty()) return;
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
    heading(tr("Estilo desta gaveta"));
    {
        auto* g = new QActionGroup(&menu);
        struct Opt { Style s; QString name; QString tip; };
        const QList<Opt> opts = {
            { Style::Classic,   tr("Clássico"), tr("Blocos com foto ou lista, como sempre foi") },
            { Style::Portraits, tr("Retratos"), tr("Elenco: rosto, nome e uma frase da ficha") },
            { Style::Polaroid,  tr("Polaroid"), tr("Fotos presas num quadro de cortiça") },
            { Style::Badges,    tr("Crachás"),  tr("Uma linha por pessoa, como crachá: foto, papel e dois dados da ficha") },
            { Style::Gallery,   tr("Galeria de rostos"), tr("O elenco em grade de retratos; o mouse sobre um rosto acende quem tem vínculo com ele") },
            { Style::Dossier,   tr("Dossiê"),   tr("Lista de um lado, a ficha ou o documento inteiro do outro") },
            { Style::Table,     tr("Tabela"),   tr("Uma linha por item, colunas que ordenam") },
        };
        for (const Opt& o : opts) {
            if (!styleAllowed(o.s)) continue;
            QAction* a = menu.addAction(o.name);
            a->setCheckable(true);
            a->setChecked(m_style == o.s);
            a->setToolTip(o.tip);
            g->addAction(a);
            const Style s = o.s;
            connect(a, &QAction::triggered, this, [this, s]() { setStyle(s); });
        }
    }
    if (m_style == Style::Classic && currentDrawerIsElement()) {
        menu.addSeparator();
        heading(tr("Exibição"));
        auto* g = new QActionGroup(&menu);
        QAction* blocks = menu.addAction(tr("Blocos"));
        QAction* list = menu.addAction(tr("Lista"));
        for (QAction* a : { blocks, list }) { a->setCheckable(true); g->addAction(a); }
        blocks->setChecked(m_gridView);
        list->setChecked(!m_gridView);
        connect(blocks, &QAction::triggered, this, [this]() { m_gridView = true; rebuildContents(); });
        connect(list, &QAction::triggered, this, [this]() { m_gridView = false; rebuildContents(); });
        if (m_gridView) {
            QMenu* size = menu.addMenu(tr("Tamanho dos cards"));
            size->setStyleSheet(menuQss());
            auto* sg = new QActionGroup(size);
            const QStringList names = { tr("Pequeno"), tr("Médio"), tr("Grande") };
            for (int i = 0; i < 3; ++i) {
                QAction* a = size->addAction(names.at(i));
                a->setCheckable(true);
                a->setChecked(m_cardSizeIdx == i);
                sg->addAction(a);
                connect(a, &QAction::triggered, this, [this, i]() {
                    m_cardSizeIdx = i;
                    QSettings().setValue(QStringLiteral("ui/cardSizeIdx"), i);
                    rebuildContents();
                });
            }
        }
    }
    {
        menu.addSeparator();
        heading(tr("Ordem"));
        auto* g = new QActionGroup(&menu);
        auto addOpt = [&](const QString& label, SortMode mode, bool ascending) {
            QAction* act = menu.addAction(label);
            act->setCheckable(true);
            act->setChecked(m_sortMode == mode && m_sortAscending == ascending);
            g->addAction(act);
            connect(act, &QAction::triggered, this, [this, mode, ascending]() {
                m_sortMode = mode;
                m_sortAscending = ascending;
                updateSortButton();
                rebuildContents();
            });
        };
        addOpt(tr("Criação ↑"), SortCreation, true);
        addOpt(tr("Criação ↓"), SortCreation, false);
        addOpt(tr("A → Z"),     SortAlpha,    true);
        if (currentDrawerIsCharacter()) addOpt(tr("Por papel"), SortRole, true);
        if (m_toolAppears && currentDrawerIsElement()) addOpt(tr("Por aparições"), SortAppearances, true);
    }
    menu.addSeparator();
    heading(tr("Ferramentas"));
    if (currentDrawerIsElement()) {
        QAction* a = menu.addAction(tr("Aparições"));
        a->setCheckable(true);
        a->setChecked(m_toolAppears);
        a->setToolTip(tr("Em quais capítulos cada um aparece, e desde quando sumiu"));
        connect(a, &QAction::toggled, this, [this](bool on) {
            m_toolAppears = on;
            QSettings().setValue(QStringLiteral("ui/drawerPanel/tool/appears"), on);
            if (!on && m_sortMode == SortAppearances) m_sortMode = SortCreation;
            rebuildContents();
        });
    }
    {
        QAction* a = menu.addAction(tr("Ficha no hover"));
        a->setCheckable(true);
        a->setChecked(m_toolHover);
        a->setToolTip(tr("Parando o mouse num item, o essencial dele sem abrir"));
        connect(a, &QAction::toggled, this, [this](bool on) {
            m_toolHover = on;
            QSettings().setValue(QStringLiteral("ui/drawerPanel/tool/hover"), on);
            if (!on) disarmHover();
        });
    }
    if (currentDrawerIsElement()) {
        QAction* hint = menu.addAction(tr("Lado a lado: botão direito num item › Comparar com…"));
        hint->setEnabled(false);
    }
    PanelMotion::animateMenu(&menu);
    menu.exec(m_moreBtn->mapToGlobal(QPoint(0, m_moreBtn->height() + 2)));
}

// ============================================================ dados

QList<DrawerItem> DrawerListPanel::visibleItems(bool* searching) const {
    QList<DrawerItem> items;
    const Drawer* drawer = m_model ? m_model->findDrawer(m_currentKey) : nullptr;
    if (!drawer) return items;
    const bool buscando = !m_searchQuery.isEmpty();
    if (searching) *searching = buscando;
    if (buscando) return searchHits();
    for (const auto& it : drawer->items)
        if (it.folderId == m_currentFolderId) items.append(it);
    if (m_sortMode == SortAlpha) {
        std::sort(items.begin(), items.end(), [](const DrawerItem& a, const DrawerItem& b) {
            return QString::localeAwareCompare(a.title, b.title) < 0;
        });
    } else if (m_sortMode == SortRole) {
        std::sort(items.begin(), items.end(), [this](const DrawerItem& a, const DrawerItem& b) {
            const int cmp = QString::localeAwareCompare(effectiveRole(a), effectiveRole(b));
            if (cmp != 0) return cmp < 0;
            return QString::localeAwareCompare(a.title, b.title) < 0;
        });
    } else if (m_sortMode == SortAppearances) {
        std::stable_sort(items.begin(), items.end(), [this](const DrawerItem& a, const DrawerItem& b) {
            return appearsFor(a).count(true) > appearsFor(b).count(true);
        });
    } else if (!m_sortAscending) {
        std::reverse(items.begin(), items.end());
    }
    return items;
}

QString DrawerListPanel::effectiveRole(const DrawerItem& it) const {
    if (!it.role.isEmpty()) return it.role;
    if (m_elementsStore && !it.elementId.isEmpty())
        if (const Element* e = m_elementsStore->findElement(it.elementId)) return e->role;
    return QString();
}

QPixmap DrawerListPanel::itemPhoto(const DrawerItem& it) const {
    if (!m_elementsStore || it.elementId.isEmpty()) return QPixmap();
    const Element* e = m_elementsStore->findElement(it.elementId);
    return (e && !e->image.isEmpty()) ? CoverUtils::pixmapFromDataUrl(e->image) : QPixmap();
}

QString DrawerListPanel::itemHtml(const DrawerItem& it) const {
    if (it.isSheet) return ProjectModel::characterSheetToHtml(it.sheet, it.title, QString(), QString());
    if (it.hasInlineHtml) return it.html;
    if (m_cache && m_cache->has(DocCache::itemKey(it.id))) return m_cache->get(DocCache::itemKey(it.id));
    if (!m_projectRoot.isEmpty() && !it.file.isEmpty()) {
        bool ok = false;
        const QString html = ProjectStorage::readText(ProjectStorage::joinPath(m_projectRoot, it.file), &ok);
        if (ok) return html;
    }
    return QString();
}

QString DrawerListPanel::itemOneLine(const DrawerItem& it) const {
    auto c = m_previewCache.constFind(it.id);
    if (c != m_previewCache.constEnd()) return c.value();
    QString text;
    if (it.isSheet) {
        // Primeiro campo de TEXTO preenchido da ficha: é onde mora a frase que
        // resume a pessoa. Campo de dado sozinho ("61") não diz nada numa linha.
        for (const SheetField& f : it.sheet.fields) {
            if (f.kind != QStringLiteral("text") || f.value.trimmed().isEmpty()) continue;
            QTextDocument d;
            d.setHtml(f.value);
            text = d.toPlainText().simplified();
            if (!text.isEmpty()) break;
        }
    } else {
        const QString html = itemHtml(it);
        if (!html.isEmpty()) {
            QTextDocument d;
            d.setHtml(html);
            text = d.toPlainText().simplified();
        }
    }
    // Uma frase só (a primeira), até ~140 letras.
    static const QRegularExpression end(QStringLiteral("[.!?…](\\s|$)"));
    const QRegularExpressionMatch m = end.match(text);
    if (m.hasMatch() && m.capturedEnd() < text.size()) text = text.left(m.capturedEnd()).trimmed();
    if (text.size() > 140) text = text.left(138).trimmed() + QStringLiteral("…");
    m_previewCache.insert(it.id, text);
    return text;
}

QList<QPair<QString, QString>> DrawerListPanel::itemFields(const DrawerItem& it) const {
    QList<QPair<QString, QString>> out;
    if (!it.isSheet) return out;
    for (const SheetField& f : it.sheet.fields) {
        if (f.kind != QStringLiteral("data")) continue;
        const QString v = f.value.trimmed();
        if (!v.isEmpty()) out.append({ f.label, v });
    }
    return out;
}

void DrawerListPanel::computeAppearances() {
    m_appears.clear();
    m_chapterLabels.clear();
    if (!m_model || !m_elementsStore) return;
    const QString ms = m_model->activeManuscriptId();
    const QList<const Chapter*> chs = m_model->orderedChaptersForManuscript(ms);
    const int n = chs.size();
    for (int i = 0; i < n; ++i) {
        const Chapter* c = chs.at(i);
        const QString num = m_model->chapterNumberLabel(*c);
        m_chapterLabels << ((c->type == QStringLiteral("chapter") && !num.isEmpty())
                                ? num : m_model->chapterTypeName(*c).left(1).toUpper());
        QStringList ids = m_elementsStore->docElementIds(ElementsStore::elementDocKeyForChapter(ms, c->id));
        for (const Scene& s : c->scenes)
            ids += m_elementsStore->docElementIds(ElementsStore::elementDocKeyForScene(ms, c->id, s.id));
        for (const QString& id : ids) {
            auto it = m_appears.find(id);
            if (it == m_appears.end()) it = m_appears.insert(id, QList<bool>(n, false));
            (*it)[i] = true;
        }
    }
}

QList<bool> DrawerListPanel::appearsFor(const DrawerItem& it) const {
    if (!m_toolAppears || it.elementId.isEmpty() || m_chapterLabels.isEmpty()) return {};
    return m_appears.value(it.elementId, QList<bool>(m_chapterLabels.size(), false));
}

QString DrawerListPanel::appearsMeta(const QList<bool>& ap) const {
    const int c = ap.count(true);
    if (!c) return tr("ainda não aparece");
    const int first = ap.indexOf(true), last = ap.lastIndexOf(true);
    return tr("em %1 de %2 · desde o %3 · última no %4")
        .arg(c).arg(ap.size()).arg(m_chapterLabels.value(first), m_chapterLabels.value(last));
}

QList<DwBond> DrawerListPanel::dwBonds() const {
    QList<DwBond> out;
    if (!m_model || !currentDrawerIsCharacter()) return out;
    for (const auto& b : m_model->characterBondsForDrawer(m_currentKey)) {
        DwBond d;
        d.id = b.id;
        d.from = b.fromItemId;
        d.to = b.toItemId;
        d.color = QColor(b.color.isEmpty() ? QStringLiteral("#4a9eff") : b.color);
        d.label = b.type.trimmed().isEmpty() ? tr("Vínculo") : b.type;
        out.append(d);
    }
    return out;
}

QList<DwEntry> DrawerListPanel::dwEntries(const QList<DrawerItem>& items) const {
    QList<DwEntry> out;
    for (const DrawerItem& it : items) {
        DwEntry e;
        e.id = it.id;
        e.title = it.title.isEmpty() ? tr("(sem nome)") : it.title;
        const QString role = effectiveRole(it);
        e.role = role.isEmpty() ? QString() : RoleTiers::roleDisplayName(role);
        e.photo = itemPhoto(it);
        const QString r = role.trimmed().toUpper();
        e.roleColor = r == QStringLiteral("PROTAGONISTA") ? QColor("#d9a441")
                    : r == QStringLiteral("ANTAGONISTA")  ? QColor("#e57373")
                    : tcol(Theme::panelBorder());
        e.appears = appearsFor(it);
        if (!e.appears.isEmpty()) e.appearsMeta = appearsMeta(e.appears);
        out.append(e);
    }
    return out;
}

// ============================================================ Retratos / Polaroid / Crachás / Galeria

void DrawerListPanel::wireView(DwBondDragBase* v) {
    connect(v, &DwBondDragBase::itemActivated, this, [this](const QString& id) { emit itemActivated(m_currentKey, id); });
    connect(v, &DwBondDragBase::itemContextRequested, this, [this](const QString& id, const QPoint& pos) {
        showItemContextMenu(id, pos);
    });
    connect(v, &DwBondDragBase::itemHovered, this, [this](const QString& id, const QRect& r) {
        if (id.isEmpty()) disarmHover(); else armHover(id, r);
    });
    connect(v, &DwBondDragBase::bondCreateRequested, this, [this](const QString& from, const QString& to, const QPoint& pos) {
        emit bondCreateRequested(m_currentKey, from, to, pos);
    });
    connect(v, &DwBondDragBase::bondClicked, this, [this](const QString& bondId, const QPoint& pos) {
        emit bondViewRequested(m_currentKey, bondId, pos);
    });
}

namespace {
QLabel* hintLabel(const QString& text, QWidget* parent) {
    auto* l = new QLabel(text, parent);
    l->setAlignment(Qt::AlignCenter);
    l->setWordWrap(true);
    l->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Lora','Crimson Text',serif; font-size: 11px; "
        "font-style: italic; padding: 6px 8px 4px 8px;").arg(Theme::textMuted()));
    return l;
}
}

void DrawerListPanel::buildPortraits(const QList<DrawerItem>& items) {
    if (items.isEmpty()) { m_listLayout->insertWidget(0, makeEmptyState()); return; }
    QList<DwEntry> entries = dwEntries(items);
    for (int i = 0; i < items.size(); ++i) entries[i].oneLine = itemOneLine(items.at(i));
    QList<DwSection> sections;
    if (currentDrawerIsCharacter() && m_sortMode != SortAlpha && m_sortMode != SortAppearances) {
        // Agrupado por papel: protagonistas em cima, figurantes embaixo.
        QMap<QString, DwSection> byRole;
        for (int i = 0; i < items.size(); ++i) {
            const QString role = effectiveRole(items.at(i));
            const QString key = roleOrderKey(role);
            DwSection& s = byRole[key];
            if (s.title.isEmpty())
                s.title = role.isEmpty() ? tr("Sem papel") : RoleTiers::roleDisplayName(role);
            s.rows.append(i);
        }
        for (auto it = byRole.cbegin(); it != byRole.cend(); ++it) {
            DwSection s = it.value();
            s.title = QStringLiteral("%1 · %2").arg(s.title).arg(s.rows.size());
            sections.append(s);
        }
    } else {
        DwSection s;
        for (int i = 0; i < items.size(); ++i) s.rows.append(i);
        sections.append(s);
    }
    auto* v = new DwPortraits(this);
    v->bondsEnabled = currentDrawerIsCharacter();
    v->setData(entries, sections, dwBonds(), QColor(currentDrawerColor()));
    wireView(v);
    int row = 0;
    m_listLayout->insertWidget(row++, v);
    if (currentDrawerIsCharacter())
        m_listLayout->insertWidget(row++, hintLabel(tr("Arraste um rosto até outra linha pra criar vínculo."), this));
}

void DrawerListPanel::buildPolaroid(const QList<DrawerItem>& items) {
    if (items.isEmpty()) { m_listLayout->insertWidget(0, makeEmptyState()); return; }
    auto* v = new DwPolaroid(this);
    v->bondsEnabled = currentDrawerIsCharacter();
    v->setData(dwEntries(items), dwBonds(), QColor(currentDrawerColor()), QString());
    wireView(v);
    int row = 0;
    m_listLayout->insertWidget(row++, v);
    if (currentDrawerIsCharacter())
        m_listLayout->insertWidget(row++, hintLabel(tr("Arraste o alfinete de uma foto até outra pra criar vínculo."), this));
}

void DrawerListPanel::buildBadges(const QList<DrawerItem>& items) {
    if (items.isEmpty()) { m_listLayout->insertWidget(0, makeEmptyState()); return; }
    QList<DwEntry> entries = dwEntries(items);
    for (int i = 0; i < items.size(); ++i) entries[i].facts = itemFields(items.at(i)).mid(0, 2);
    auto* v = new DwBadges(this);
    v->bondsEnabled = currentDrawerIsCharacter();
    v->setData(entries, dwBonds(), QColor(currentDrawerColor()));
    wireView(v);
    int row = 0;
    m_listLayout->insertWidget(row++, v);
    if (currentDrawerIsCharacter())
        m_listLayout->insertWidget(row++, hintLabel(tr("Arraste a foto de um crachá até outro pra criar vínculo."), this));
}

void DrawerListPanel::buildGallery(const QList<DrawerItem>& items) {
    if (items.isEmpty()) { m_listLayout->insertWidget(0, makeEmptyState()); return; }
    auto* v = new DwGallery(this);
    v->bondsEnabled = currentDrawerIsCharacter();
    v->setData(dwEntries(items), dwBonds(), QColor(currentDrawerColor()));
    wireView(v);
    int row = 0;
    m_listLayout->insertWidget(row++, v);
    if (currentDrawerIsCharacter())
        m_listLayout->insertWidget(row++, hintLabel(tr("Passe o mouse num rosto pra ver os vínculos · arraste um rosto até outro pra criar um."), this));
}

// ============================================================ Dossiê

void DrawerListPanel::buildDossier(const QList<DrawerItem>& items) {
    if (items.isEmpty()) { m_altLayout->addWidget(makeEmptyState()); m_altLayout->addStretch(1); return; }
    const DrawerItem* sel = nullptr;
    for (const auto& it : items) if (it.id == m_dossierSel) sel = &it;
    if (!sel) { sel = &items.first(); m_dossierSel = sel->id; }
    const bool element = currentDrawerIsElement();
    const bool character = currentDrawerIsCharacter();
    const qreal dpr = devicePixelRatioF();
    const QColor accent(currentDrawerColor());

    auto* root = new QWidget(m_altHost);
    auto* hl = new QHBoxLayout(root);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    // lista
    auto* ls = new QScrollArea(root);
    ls->setObjectName(QStringLiteral("dossierList"));
    ls->setFixedWidth(210);
    ls->setWidgetResizable(true);
    ls->setFrameShape(QFrame::NoFrame);
    ls->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ls->setStyleSheet(QStringLiteral("#dossierList { background: %1; border-right: 1px solid %2; }")
        .arg(Theme::appBackground(), Theme::subtleBorder()));
    ls->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* lh = new QWidget(ls);
    auto* ll = new QVBoxLayout(lh);
    ll->setContentsMargins(6, 6, 6, 6);
    ll->setSpacing(1);
    for (const DrawerItem& it : items) {
        const bool on = (it.id == m_dossierSel);
        const QString title = it.title.isEmpty() ? tr("(sem nome)") : it.title;
        const QString role = effectiveRole(it);
        const QString sub = role.isEmpty() ? QString() : RoleTiers::roleDisplayName(role).toUpper();
        const QPixmap face = element ? DwPaint::face(itemPhoto(it), title, accent, 30, dpr) : QPixmap();
        auto* row = new MsRow(lh);
        row->setRowHeight(element ? 42 : 30);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (on || self->isHovered()) {
                p.setPen(Qt::NoPen);
                p.setBrush(on ? tcol(Theme::accentInfoSoft()) : tcol(Theme::hoverOverlay()));
                p.drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 5, 5);
            }
            int x = 8;
            if (!face.isNull()) { p.drawPixmap(x, r.center().y() - 15, face); x += 38; }
            QFont f(QStringLiteral("Segoe UI"));
            f.setPixelSize(12);
            f.setWeight(on ? QFont::DemiBold : QFont::Medium);
            p.setFont(f);
            p.setPen(on || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
            const int tw = r.width() - x - 6;
            const bool two = !sub.isEmpty();
            p.drawText(QRect(x, two ? r.top() + 5 : r.top(), tw, two ? 17 : r.height()), Qt::AlignVCenter | Qt::AlignLeft,
                       p.fontMetrics().elidedText(title, Qt::ElideRight, tw));
            if (two) {
                QFont g(QStringLiteral("Segoe UI"));
                g.setPixelSize(9);
                g.setBold(true);
                g.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
                p.setFont(g);
                p.setPen(tcol(Theme::accentInfo()));
                p.drawText(QRect(x, r.top() + 22, tw, 13), Qt::AlignVCenter | Qt::AlignLeft, sub);
            }
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString id = it.id;
        connect(row, &QToolButton::clicked, this, [this, id]() { m_dossierSel = id; rebuildContents(); });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, id](const QPoint& pos) {
            showItemContextMenu(id, row->mapToGlobal(pos));
        });
        ll->addWidget(row);
    }
    ll->addStretch(1);
    ls->setWidget(lh);
    hl->addWidget(ls);

    // ficha / documento
    const DrawerItem it = *sel;
    const QString title = it.title.isEmpty() ? tr("(sem nome)") : it.title;
    auto* rs = new QScrollArea(root);
    rs->setWidgetResizable(true);
    rs->setFrameShape(QFrame::NoFrame);
    rs->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rs->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    rs->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* dh = new QWidget(rs);
    auto* dl = new QVBoxLayout(dh);
    dl->setContentsMargins(18, 16, 18, 16);
    dl->setSpacing(10);
    auto* top = new QHBoxLayout;
    top->setSpacing(14);
    if (element) {
        auto* ph = new QLabel(dh);
        ph->setPixmap(DwPaint::face(itemPhoto(it), title, accent, 104, dpr, false));
        ph->setFixedSize(104, 104);
        top->addWidget(ph, 0, Qt::AlignTop);
    }
    auto* tv = new QVBoxLayout;
    tv->setSpacing(3);
    auto* name = new QLabel(title, dh);
    name->setWordWrap(true);
    QFont nf(QStringLiteral("Lora"));
    nf.setPixelSize(20);
    nf.setWeight(QFont::DemiBold);
    name->setFont(nf);
    name->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textBright()));
    tv->addWidget(name);
    const QString role = effectiveRole(it);
    if (!role.isEmpty()) tv->addWidget(caps(RoleTiers::roleDisplayName(role), dh, Theme::accentInfo()));
    const QString path = itemFolderPath(it.folderId);
    if (!path.isEmpty()) {
        auto* pl = new QLabel(path, dh);
        pl->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; background: transparent;").arg(Theme::textMuted()));
        tv->addWidget(pl);
    }
    tv->addStretch(1);
    top->addLayout(tv, 1);
    dl->addLayout(top);

    // campos curtos da ficha
    const auto fields = itemFields(it);
    if (!fields.isEmpty()) {
        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(14);
        grid->setVerticalSpacing(5);
        int r = 0;
        for (const auto& f : fields) {
            grid->addWidget(caps(f.first, dh), r, 0, Qt::AlignTop);
            auto* v = new QLabel(f.second, dh);
            v->setWordWrap(true);
            v->setStyleSheet(QStringLiteral("color: %1; font-size: 12.5px; background: transparent;").arg(Theme::textPrimary()));
            grid->addWidget(v, r++, 1);
        }
        grid->setColumnStretch(1, 1);
        dl->addLayout(grid);
    }
    // vínculos: fichas clicáveis + "+ vínculo"
    if (character) {
        QStringList parts;
        for (const DwBond& b : dwBonds()) {
            if (b.from != it.id && b.to != it.id) continue;
            const QString other = b.from == it.id ? b.to : b.from;
            const DrawerItem* o = m_model->findDrawerItem(other);
            parts << QStringLiteral("<a href=\"bond:%1\" style=\"color:%2; text-decoration:none\">%3&nbsp;%4 · %5</a>")
                .arg(b.id, tcol(Theme::textPrimary()).name(), dotTag(b.color), b.label.toHtmlEscaped(),
                     (o ? o->title : QStringLiteral("?")).toHtmlEscaped());
        }
        parts << QStringLiteral("<a href=\"add\" style=\"color:%1; text-decoration:none\">+ %2</a>")
            .arg(tcol(Theme::textMuted()).name(), tr("vínculo"));
        auto* row = new QHBoxLayout;
        row->setSpacing(14);
        row->addWidget(caps(tr("Vínculos"), dh), 0, Qt::AlignTop);
        auto* bl = new QLabel(parts.join(QStringLiteral(" &nbsp;&nbsp; ")), dh);
        bl->setTextFormat(Qt::RichText);
        bl->setWordWrap(true);
        bl->setStyleSheet(QStringLiteral("font-size: 12px; background: transparent;"));
        const QString fromId = it.id;
        connect(bl, &QLabel::linkActivated, this, [this, fromId](const QString& link) {
            if (link.startsWith(QStringLiteral("bond:"))) {
                emit bondViewRequested(m_currentKey, link.mid(5), QCursor::pos());
            } else {
                QMenu menu(this);
                menu.setStyleSheet(menuQss());
                addBondMenu(&menu, fromId);
                // o submenu já tem tudo; abre ele direto
                if (!menu.actions().isEmpty() && menu.actions().first()->menu())
                    menu.actions().first()->menu()->exec(QCursor::pos());
            }
        });
        row->addWidget(bl, 1);
        dl->addLayout(row);
    }
    if (m_toolAppears) {
        const QList<bool> ap = appearsFor(it);
        if (!ap.isEmpty()) {
            auto* row = new QHBoxLayout;
            row->setSpacing(14);
            row->addWidget(caps(tr("Aparições"), dh), 0, Qt::AlignTop);
            auto* col = new QVBoxLayout;
            col->setSpacing(3);
            auto* strip = new QLabel(dh);
            strip->setPixmap(appearsPixmap(ap, accent, 180, 7, dpr));
            col->addWidget(strip);
            auto* meta = new QLabel(appearsMeta(ap), dh);
            meta->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; background: transparent;").arg(Theme::textMuted()));
            col->addWidget(meta);
            row->addLayout(col, 1);
            dl->addLayout(row);
        }
    }
    // o texto inteiro: na ficha, os campos longos como seções (os curtos já
    // estão na grade acima); no documento, ele todo.
    QString html;
    if (it.isSheet) {
        for (const SheetField& f : it.sheet.fields) {
            if (f.kind != QStringLiteral("text") || f.value.trimmed().isEmpty()) continue;
            html += QStringLiteral("<h4 style=\"margin-top:10px; margin-bottom:2px; color:%1\">%2</h4>%3")
                .arg(tcol(Theme::textBright()).name(), f.label.toHtmlEscaped(), f.value);
        }
    } else {
        html = itemHtml(it);
    }
    auto* doc = new QTextBrowser(dh);
    doc->setOpenLinks(false);
    doc->setFrameShape(QFrame::NoFrame);
    doc->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // padding: 0 obrigatório — o QSS global dá 80px/100px pra todo QTextEdit
    // (margem de página do editor), e o QTextBrowser herda.
    doc->setStyleSheet(QStringLiteral("QTextBrowser { background: transparent; color: %1; padding: 0px; border: none; }").arg(Theme::textPrimary()));
    doc->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    doc->document()->setDefaultStyleSheet(QStringLiteral(
        "body { font-family: 'Source Serif 4', 'Lora', Georgia, serif; font-size: 13.5px; line-height: 150%; }"
        "h1, h2, h3 { color: %1; }").arg(tcol(Theme::textBright()).name()));
    if (html.trimmed().isEmpty())
        html = QStringLiteral("<p style=\"color:%1\"><i>%2</i></p>").arg(tcol(Theme::textMuted()).name(), tr("Sem texto ainda."));
    doc->setHtml(html);
    // Altura pelo conteúdo: o scroll é o da coluna, não o do documento.
    doc->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    doc->document()->setTextWidth(qMax(200, width() - 210 - 64));
    doc->setFixedHeight(int(doc->document()->size().height()) + 12);
    dl->addWidget(doc);

    // ações
    // Três por linha: com quatro ações, uma linha só estourava a coluna.
    auto* acts = new QGridLayout;
    acts->setHorizontalSpacing(6);
    acts->setVerticalSpacing(6);
    int actN = 0;
    auto mkAct = [&](const QString& text, bool primary) {
        auto* b = new QPushButton(text, dh);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(Theme::qss(primary
            ? QStringLiteral("QPushButton { background: %1; color: #ffffff; border: none; border-radius: @radius-control; padding: 5px 12px; font-size: 12px; }"
                             "QPushButton:hover { background: %2; }").arg(accent.name(), accent.lighter(115).name())
            : QStringLiteral("QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: @radius-control; padding: 5px 12px; font-size: 12px; }"
                             "QPushButton:hover { background: %3; color: %4; }")
                  .arg(Theme::textPrimary(), Theme::subtleBorder(), Theme::hoverOverlay(), Theme::textBright())));
        acts->addWidget(b, actN / 3, actN % 3);
        ++actN;
        return b;
    };
    const QString id = it.id;
    connect(mkAct(it.isSheet ? tr("Editar ficha") : tr("Abrir"), true), &QPushButton::clicked, this,
            [this, id]() { emit itemActivated(m_currentKey, id); });
    connect(mkAct(tr("Abrir no RefMenu"), false), &QPushButton::clicked, this,
            [this, id]() { emit openInRefMenuRequested(m_currentKey, id); });
    if (character)
        connect(mkAct(tr("Gerar imagem"), false), &QPushButton::clicked, this,
                [this, id]() { emit generateCharacterImageRequested(m_currentKey, id); });
    if (element && items.size() > 1) {
        auto* cmp = mkAct(tr("Comparar com…"), false);
        connect(cmp, &QPushButton::clicked, this, [this, cmp, id]() {
            QMenu menu(this);
            menu.setStyleSheet(menuQss());
            addCompareMenu(&menu, id);
            if (!menu.actions().isEmpty() && menu.actions().first()->menu())
                menu.actions().first()->menu()->exec(cmp->mapToGlobal(QPoint(0, cmp->height())));
        });
    }
    acts->setColumnStretch(3, 1);
    dl->addLayout(acts);
    dl->addStretch(1);
    rs->setWidget(dh);
    hl->addWidget(rs, 1);
    m_altLayout->addWidget(root, 1);
}

// ============================================================ Tabela

void DrawerListPanel::buildTable(const QList<DrawerItem>& items) {
    if (items.isEmpty()) { m_altLayout->addWidget(makeEmptyState()); m_altLayout->addStretch(1); return; }
    const bool element = currentDrawerIsElement();
    const bool character = currentDrawerIsCharacter();
    const qreal dpr = devicePixelRatioF();
    const QColor accent(currentDrawerColor());
    bool anyRole = false;
    for (const auto& it : items) if (!effectiveRole(it).isEmpty()) anyRole = true;
    const bool appears = m_toolAppears && element && !m_chapterLabels.isEmpty();

    enum Col { CName, CRole, CFolder, CAppears, CLast, CBonds, CWords };
    QList<Col> cols = { CName };
    if (anyRole) cols << CRole;
    cols << CFolder;
    if (appears) cols << CAppears << CLast;
    if (character) cols << CBonds;
    if (!element) cols << CWords;
    QStringList heads;
    for (Col c : cols) {
        switch (c) {
        case CName:    heads << tr("Nome"); break;
        case CRole:    heads << tr("Papel"); break;
        case CFolder:  heads << tr("Pasta"); break;
        case CAppears: heads << tr("Aparições"); break;
        case CLast:    heads << tr("Última"); break;
        case CBonds:   heads << tr("Vínculos"); break;
        case CWords:   heads << tr("Palavras"); break;
        }
    }
    for (QString& hd : heads) hd = hd.toUpper();
    auto* t = new QTableWidget(items.size(), cols.size(), m_altHost);
    t->setHorizontalHeaderLabels(heads);
    t->verticalHeader()->hide();
    t->setShowGrid(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->setMouseTracking(true);
    t->setIconSize(QSize(22, 22));
    t->setContextMenuPolicy(Qt::CustomContextMenu);
    t->verticalHeader()->setDefaultSectionSize(32);
    t->horizontalHeader()->setHighlightSections(false);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    t->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QTableWidget { background: transparent; color: %1; border: none; font-size: 12px; }
        QTableWidget::item { padding: 0 8px; border-bottom: 1px solid %2; }
        QTableWidget::item:hover { background: %3; }
        QHeaderView::section { background: %4; color: %5; border: none; border-bottom: 1px solid %6;
                               padding: 7px 8px; font-size: 10px; font-weight: 700; }
    )")).arg(Theme::textPrimary(), Theme::subtleBorder(), Theme::hoverOverlay(),
             Theme::appBackground(), Theme::textMuted(), Theme::subtleBorder()));
    t->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    const QList<DwBond> bonds = dwBonds();
    for (int r = 0; r < items.size(); ++r) {
        const DrawerItem& it = items.at(r);
        const QString title = it.title.isEmpty() ? tr("(sem nome)") : it.title;
        const QList<bool> ap = appears ? appearsFor(it) : QList<bool>();
        for (int c = 0; c < cols.size(); ++c) {
            auto* cell = new SortItem();
            cell->setData(Qt::UserRole, it.id);
            switch (cols.at(c)) {
            case CName:
                cell->setText(title);
                cell->setData(Qt::UserRole + 1, title);
                if (element) cell->setIcon(QIcon(DwPaint::face(itemPhoto(it), title, accent, 22, dpr)));
                cell->setForeground(tcol(Theme::textBright()));
                break;
            case CRole: {
                const QString role = effectiveRole(it);
                cell->setText(role.isEmpty() ? QStringLiteral("—") : RoleTiers::roleDisplayName(role).toUpper());
                cell->setData(Qt::UserRole + 1, roleOrderKey(role));
                cell->setForeground(tcol(Theme::accentInfo()));
                QFont f = cell->font(); f.setPixelSize(10); f.setBold(true); cell->setFont(f);
                break;
            }
            case CFolder: {
                const QString p = itemFolderPath(it.folderId);
                cell->setText(p.isEmpty() ? QStringLiteral("—") : p);
                cell->setData(Qt::UserRole + 1, p);
                cell->setForeground(tcol(Theme::textMuted()));
                break;
            }
            case CAppears:
                cell->setText(QString::number(ap.count(true)));
                cell->setData(Qt::UserRole + 1, ap.count(true));
                cell->setForeground(tcol(Theme::textMuted()));
                cell->setToolTip(appearsMeta(ap));
                break;
            case CLast: {
                const int last = ap.lastIndexOf(true);
                cell->setText(last >= 0 ? tr("cap. %1").arg(m_chapterLabels.value(last)) : QStringLiteral("—"));
                cell->setData(Qt::UserRole + 1, last);
                cell->setForeground(tcol(Theme::textMuted()));
                break;
            }
            case CBonds: {
                QList<QColor> dots;
                QStringList tips;
                for (const DwBond& b : bonds) {
                    if (b.from != it.id && b.to != it.id) continue;
                    dots << b.color;
                    const DrawerItem* o = m_model->findDrawerItem(b.from == it.id ? b.to : b.from);
                    tips << QStringLiteral("%1 · %2").arg(b.label, o ? o->title : QStringLiteral("?"));
                }
                cell->setData(Qt::UserRole + 1, dots.size());
                if (dots.isEmpty()) { cell->setText(QStringLiteral("—")); cell->setForeground(tcol(Theme::textMuted())); break; }
                const int n = qMin(8, int(dots.size()));
                QPixmap pm(QSize(n * 10, 10) * dpr);
                pm.setDevicePixelRatio(dpr);
                pm.fill(Qt::transparent);
                QPainter p(&pm);
                p.setRenderHint(QPainter::Antialiasing);
                p.setPen(Qt::NoPen);
                for (int i = 0; i < n; ++i) { p.setBrush(dots.at(i)); p.drawEllipse(QRectF(i * 10 + 1.5, 1.5, 7, 7)); }
                p.end();
                cell->setIcon(QIcon(pm));
                cell->setText(QString::number(dots.size()));
                cell->setForeground(tcol(Theme::textMuted()));
                cell->setToolTip(tips.join(QStringLiteral("\n")));
                break;
            }
            case CWords: {
                int words = -1;
                if (items.size() <= 300) {
                    const QString key = QStringLiteral("w:") + it.id;
                    auto cw = m_previewCache.constFind(key);
                    if (cw != m_previewCache.constEnd()) words = cw.value().toInt();
                    else {
                        QTextDocument d;
                        d.setHtml(itemHtml(it));
                        words = d.toPlainText().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();
                        m_previewCache.insert(key, QString::number(words));
                    }
                }
                cell->setText(words >= 0 ? QLocale().toString(words) : QStringLiteral("—"));
                cell->setData(Qt::UserRole + 1, words);
                cell->setForeground(tcol(Theme::textMuted()));
                break;
            }
            }
            t->setItem(r, c, cell);
        }
    }
    // Sem coluna marcada, ligar a ordenação não mexe na ordem da gaveta.
    t->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    t->setSortingEnabled(true);
    t->horizontalHeader()->setSortIndicatorShown(true);
    connect(t, &QTableWidget::cellClicked, this, [this, t](int r, int) {
        if (auto* it = t->item(r, 0)) emit itemActivated(m_currentKey, it->data(Qt::UserRole).toString());
    });
    connect(t, &QTableWidget::customContextMenuRequested, this, [this, t](const QPoint& pos) {
        if (auto* it = t->itemAt(pos)) showItemContextMenu(it->data(Qt::UserRole).toString(), t->viewport()->mapToGlobal(pos));
    });
    connect(t, &QTableWidget::cellEntered, this, [this, t](int r, int) {
        if (auto* it = t->item(r, 0)) {
            const QRect rr = t->visualItemRect(it);
            armHover(it->data(Qt::UserRole).toString(),
                     QRect(t->viewport()->mapToGlobal(QPoint(0, rr.top())), QSize(t->viewport()->width(), rr.height())));
        }
    });
    t->viewport()->setProperty("disarmOnLeave", true);
    t->viewport()->installEventFilter(this);
    m_altLayout->addWidget(t, 1);
}

// ============================================================ Lado a lado

void DrawerListPanel::openCompare(const QString& a, const QString& b) {
    m_compareA = a;
    m_compareB = b;
    m_compareDiff = false;
    if (width() < 540) {
        resize(560, height());
        emit panelWidthChanged();
    }
    rebuildContents();
}

void DrawerListPanel::closeCompare() {
    m_compareA.clear();
    m_compareB.clear();
    applyStyleWidth();
    rebuildContents();
}

void DrawerListPanel::buildCompare() {
    const DrawerItem* pa = m_model->findDrawerItem(m_compareA);
    const DrawerItem* pb = m_model->findDrawerItem(m_compareB);
    if (!pa || !pb) {
        m_compareA.clear();
        m_compareB.clear();
        QTimer::singleShot(0, this, [this]() { rebuildContents(); });
        return;
    }
    const DrawerItem a = *pa, b = *pb;
    const qreal dpr = devicePixelRatioF();
    const QColor accent(currentDrawerColor());
    const bool character = currentDrawerIsCharacter();

    auto* head = new QWidget(m_altHost);
    head->setObjectName(QStringLiteral("cmpHead"));
    head->setAttribute(Qt::WA_StyledBackground, true);
    head->setStyleSheet(QStringLiteral("#cmpHead { background: %1; border-bottom: 1px solid %2; }")
        .arg(Theme::appBackground(), Theme::subtleBorder()));
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 7, 8, 7);
    hl->setSpacing(6);
    auto* ht = new QLabel(tr("Comparando 2 fichas"), head);
    ht->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; background: transparent;").arg(Theme::textMuted()));
    hl->addWidget(ht, 1);
    auto* diff = new QToolButton(head);
    diff->setText(tr("só diferenças"));
    diff->setCursor(Qt::PointingHandCursor);
    diff->setStyleSheet(chipQss(m_compareDiff));
    connect(diff, &QToolButton::clicked, this, [this]() { m_compareDiff = !m_compareDiff; rebuildContents(); });
    hl->addWidget(diff);
    auto* close = new QToolButton(head);
    close->setText(QStringLiteral("✕"));
    close->setToolTip(tr("Voltar pra gaveta"));
    close->setCursor(Qt::PointingHandCursor);
    close->setStyleSheet(chipQss(false));
    connect(close, &QToolButton::clicked, this, &DrawerListPanel::closeCompare);
    hl->addWidget(close);
    m_altLayout->addWidget(head);

    // Entre os dois (personagens)
    if (character) {
        auto* band = new QWidget(m_altHost);
        band->setObjectName(QStringLiteral("cmpBand"));
        band->setAttribute(Qt::WA_StyledBackground, true);
        band->setStyleSheet(QStringLiteral("#cmpBand { border-bottom: 1px solid %1; }").arg(Theme::subtleBorder()));
        auto* bl = new QHBoxLayout(band);
        bl->setContentsMargins(14, 8, 12, 8);
        bl->setSpacing(8);
        bl->addWidget(caps(tr("Entre os dois"), band));
        const DwBond* between = nullptr;
        const QList<DwBond> bonds = dwBonds();
        for (const DwBond& x : bonds)
            if ((x.from == a.id && x.to == b.id) || (x.from == b.id && x.to == a.id)) between = &x;
        if (between) {
            auto* l = new QLabel(QStringLiteral("<a href=\"v\" style=\"color:%1; text-decoration:none\">%2&nbsp;%3</a> "
                                                "<span style=\"color:%4\">· %5 e %6</span>")
                .arg(tcol(Theme::textBright()).name(), dotTag(between->color), between->label.toHtmlEscaped(),
                     tcol(Theme::textMuted()).name(), a.title.toHtmlEscaped(), b.title.toHtmlEscaped()), band);
            l->setTextFormat(Qt::RichText);
            const QString bid = between->id;
            connect(l, &QLabel::linkActivated, this, [this, bid]() { emit bondViewRequested(m_currentKey, bid, QCursor::pos()); });
            bl->addWidget(l, 1);
        } else {
            auto* none = new QLabel(tr("nenhum vínculo"), band);
            none->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; background: transparent;").arg(Theme::textMuted()));
            bl->addWidget(none, 1);
            auto* add = new QToolButton(band);
            add->setText(tr("+ criar"));
            add->setCursor(Qt::PointingHandCursor);
            add->setStyleSheet(chipQss(false));
            const QString ida = a.id, idb = b.id;
            connect(add, &QToolButton::clicked, this, [this, add, ida, idb]() {
                emit bondCreateRequested(m_currentKey, ida, idb, add->mapToGlobal(QPoint(0, add->height())));
            });
            bl->addWidget(add);
        }
        m_altLayout->addWidget(band);
    }

    // Campo com campo, na mesma altura.
    struct Row { QString label; QString va; QString vb; };
    QList<Row> rows;
    const QString ra = effectiveRole(a), rb = effectiveRole(b);
    if (!ra.isEmpty() || !rb.isEmpty())
        rows.append({ tr("Papel"), ra.isEmpty() ? QStringLiteral("—") : RoleTiers::roleDisplayName(ra),
                      rb.isEmpty() ? QStringLiteral("—") : RoleTiers::roleDisplayName(rb) });
    const auto fa = itemFields(a), fb = itemFields(b);
    QStringList labels;
    for (const auto& f : fa) if (!labels.contains(f.first)) labels << f.first;
    for (const auto& f : fb) if (!labels.contains(f.first)) labels << f.first;
    auto valueOf = [](const QList<QPair<QString, QString>>& fs, const QString& l) {
        for (const auto& f : fs) if (f.first == l) return f.second;
        return QStringLiteral("—");
    };
    for (const QString& l : labels) rows.append({ l, valueOf(fa, l), valueOf(fb, l) });
    if (m_toolAppears) {
        const auto apA = appearsFor(a), apB = appearsFor(b);
        if (!apA.isEmpty() || !apB.isEmpty())
            rows.append({ tr("Aparições"), tr("%n capítulo(s)", "", apA.count(true)), tr("%n capítulo(s)", "", apB.count(true)) });
    }
    if (character) {
        auto bondText = [this](const QString& id) {
            QStringList out;
            for (const DwBond& x : dwBonds()) {
                if (x.from != id && x.to != id) continue;
                const DrawerItem* o = m_model->findDrawerItem(x.from == id ? x.to : x.from);
                out << tr("%1 com %2").arg(x.label, o ? o->title : QStringLiteral("?"));
            }
            return out.isEmpty() ? QStringLiteral("—") : out.join(QStringLiteral(", "));
        };
        rows.append({ tr("Vínculos"), bondText(a.id), bondText(b.id) });
    }

    auto* sc = new QScrollArea(m_altHost);
    sc->setWidgetResizable(true);
    sc->setFrameShape(QFrame::NoFrame);
    sc->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    sc->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* body = new QWidget(sc);
    auto* g = new QGridLayout(body);
    g->setContentsMargins(14, 12, 14, 16);
    g->setHorizontalSpacing(24);
    g->setVerticalSpacing(0);
    int r = 0;
    const bool element = currentDrawerIsElement();
    if (element) {
        for (int c = 0; c < 2; ++c) {
            const DrawerItem& x = c == 0 ? a : b;
            auto* ph = new QLabel(body);
            ph->setPixmap(DwPaint::face(itemPhoto(x), x.title, accent, 110, dpr, false));
            ph->setFixedSize(110, 110);
            g->addWidget(ph, r, c, Qt::AlignLeft);
        }
        ++r;
    }
    for (int c = 0; c < 2; ++c) {
        const DrawerItem& x = c == 0 ? a : b;
        auto* n = new QLabel(x.title, body);
        n->setWordWrap(true);
        QFont nf(QStringLiteral("Lora"));
        nf.setPixelSize(16);
        nf.setWeight(QFont::DemiBold);
        n->setFont(nf);
        n->setStyleSheet(QStringLiteral("color: %1; background: transparent; padding: 6px 0 8px 0;").arg(Theme::textBright()));
        g->addWidget(n, r, c);
    }
    ++r;
    for (const Row& row : rows) {
        const bool same = (row.va == row.vb);
        if (m_compareDiff && same) continue;
        for (int c = 0; c < 2; ++c) {
            auto* cell = new QWidget(body);
            cell->setObjectName(QStringLiteral("cmpCell"));
            cell->setAttribute(Qt::WA_StyledBackground, true);
            cell->setStyleSheet(QStringLiteral("#cmpCell { border-top: 1px solid %1; }").arg(Theme::subtleBorder()));
            auto* cl = new QVBoxLayout(cell);
            cl->setContentsMargins(0, 6, 0, 6);
            cl->setSpacing(2);
            cl->addWidget(caps(row.label, cell));
            auto* v = new QLabel(c == 0 ? row.va : row.vb, cell);
            v->setWordWrap(true);
            v->setStyleSheet(QStringLiteral("color: %1; font-size: 12.5px; background: transparent;")
                .arg(same ? Theme::textMuted() : Theme::textBright()));
            cl->addWidget(v);
            g->addWidget(cell, r, c);
        }
        ++r;
    }
    if (rows.isEmpty()) {
        auto* l = new QLabel(tr("As duas fichas ainda não têm campos preenchidos."), body);
        l->setStyleSheet(QStringLiteral("color: %1; font-style: italic; background: transparent;").arg(Theme::textMuted()));
        g->addWidget(l, r++, 0, 1, 2);
    }
    g->setColumnStretch(0, 1);
    g->setColumnStretch(1, 1);
    g->setRowStretch(r, 1);
    sc->setWidget(body);
    m_altLayout->addWidget(sc, 1);
}

// ============================================================ menus

void DrawerListPanel::addBondMenu(QMenu* menu, const QString& fromId) {
    const Drawer* d = m_model ? m_model->findDrawer(m_currentKey) : nullptr;
    if (!d) return;
    QMenu* sub = menu->addMenu(tr("Criar vínculo com…"));
    sub->setStyleSheet(menuQss());
    QList<DrawerItem> others;
    for (const auto& it : d->items) if (it.id != fromId) others.append(it);
    std::sort(others.begin(), others.end(), [](const DrawerItem& a, const DrawerItem& b) {
        return QString::localeAwareCompare(a.title, b.title) < 0;
    });
    for (const auto& it : others) {
        const QString to = it.id;
        connect(sub->addAction(it.title.isEmpty() ? tr("(sem nome)") : it.title), &QAction::triggered, this,
                [this, fromId, to]() { emit bondCreateRequested(m_currentKey, fromId, to, QCursor::pos()); });
    }
    if (others.isEmpty()) sub->setEnabled(false);
}

void DrawerListPanel::addCompareMenu(QMenu* menu, const QString& itemId) {
    const Drawer* d = m_model ? m_model->findDrawer(m_currentKey) : nullptr;
    if (!d) return;
    QMenu* sub = menu->addMenu(tr("Comparar com…"));
    sub->setStyleSheet(menuQss());
    QList<DrawerItem> others;
    for (const auto& it : d->items) if (it.id != itemId) others.append(it);
    std::sort(others.begin(), others.end(), [](const DrawerItem& a, const DrawerItem& b) {
        return QString::localeAwareCompare(a.title, b.title) < 0;
    });
    for (const auto& it : others) {
        const QString other = it.id;
        connect(sub->addAction(it.title.isEmpty() ? tr("(sem nome)") : it.title), &QAction::triggered, this,
                [this, itemId, other]() { openCompare(itemId, other); });
    }
    if (others.isEmpty()) sub->setEnabled(false);
}

// ============================================================ ficha no hover

void DrawerListPanel::armHover(const QString& itemId, const QRect& globalRect) {
    if (!m_toolHover || itemId.isEmpty()) return;
    m_hoverRect = globalRect;
    if (m_hoverItemId == itemId && m_hoverCard->isVisible()) return;
    m_hoverItemId = itemId;
    if (m_hoverCard->isVisible()) showHoverCard();
    else m_hoverTimer->start();
}

void DrawerListPanel::disarmHover() {
    if (m_hoverTimer) m_hoverTimer->stop();
    if (m_hoverCard) m_hoverCard->hide();
    m_hoverItemId.clear();
}

void DrawerListPanel::showHoverCard() {
    if (!isVisible() || m_hoverItemId.isEmpty() || !m_model) return;
    const DrawerItem* it = m_model->findDrawerItem(m_hoverItemId);
    if (!it) return;
    const QString muted = tcol(Theme::textMuted()).name();
    const QString bright = tcol(Theme::textBright()).name();
    const QString title = it->title.isEmpty() ? tr("(sem nome)") : it->title;
    QString html;
    if (currentDrawerIsElement()) {
        const QPixmap face = DwPaint::face(itemPhoto(*it), title, QColor(currentDrawerColor()), 88, 1.0);
        const QString role = effectiveRole(*it);
        html += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"0\"><tr><td>%1</td><td style=\"padding-left:10px\">"
                               "<div style=\"font-family:'Lora'; font-size:15px; font-weight:600; color:%2\">%3</div>%4</td></tr></table>")
            .arg(pngTag(face, 44, 44), bright, title.toHtmlEscaped(),
                 role.isEmpty() ? QString()
                     : QStringLiteral("<div style=\"color:%1; font-size:9.5px; font-weight:700\">%2</div>")
                           .arg(tcol(Theme::accentInfo()).name(), RoleTiers::roleDisplayName(role).toUpper().toHtmlEscaped()));
        const auto fields = itemFields(*it);
        if (!fields.isEmpty()) {
            html += QStringLiteral("<table style=\"margin-top:8px\" cellspacing=\"0\" cellpadding=\"2\">");
            for (int i = 0; i < fields.size() && i < 5; ++i)
                html += QStringLiteral("<tr><td style=\"color:%1; font-size:9.5px; font-weight:700; padding-right:10px\">%2</td>"
                                       "<td style=\"font-size:12px\">%3</td></tr>")
                    .arg(muted, fields.at(i).first.toUpper().toHtmlEscaped(), fields.at(i).second.toHtmlEscaped());
            html += QStringLiteral("</table>");
        } else {
            const QString one = itemOneLine(*it);
            if (!one.isEmpty())
                html += QStringLiteral("<div style=\"margin-top:8px; font-style:italic\">%1</div>").arg(one.toHtmlEscaped());
        }
        if (currentDrawerIsCharacter()) {
            QStringList bonds;
            for (const DwBond& b : dwBonds()) {
                if (b.from != it->id && b.to != it->id) continue;
                const DrawerItem* o = m_model->findDrawerItem(b.from == it->id ? b.to : b.from);
                bonds << QStringLiteral("%1&nbsp;%2").arg(dotTag(b.color), (o ? o->title : QStringLiteral("?")).toHtmlEscaped());
            }
            if (!bonds.isEmpty())
                html += QStringLiteral("<div style=\"margin-top:8px; color:%1; font-size:11.5px\">%2</div>")
                    .arg(muted, bonds.join(QStringLiteral(" &nbsp; ")));
        }
        const QList<bool> ap = appearsFor(*it);
        if (!ap.isEmpty())
            html += QStringLiteral("<div style=\"margin-top:6px; color:%1; font-size:11px\">%2</div>").arg(muted, appearsMeta(ap));
    } else {
        html += QStringLiteral("<div style=\"font-family:'Lora'; font-size:14px; font-weight:600; color:%1\">%2</div>")
            .arg(bright, title.toHtmlEscaped());
        const QString path = itemFolderPath(it->folderId);
        if (!path.isEmpty())
            html += QStringLiteral("<div style=\"color:%1; font-size:11px\">%2</div>").arg(muted, path.toHtmlEscaped());
        QString prev;
        {
            QTextDocument d;
            d.setHtml(itemHtml(*it));
            prev = d.toPlainText().simplified();
        }
        if (prev.size() > 280) prev = prev.left(278).trimmed() + QStringLiteral("…");
        html += prev.isEmpty()
            ? QStringLiteral("<div style=\"margin-top:6px; color:%1; font-style:italic\">%2</div>").arg(muted, tr("Sem texto ainda."))
            : QStringLiteral("<div style=\"margin-top:6px; font-family:'Lora'; font-size:12.5px\">%1</div>").arg(prev.toHtmlEscaped());
    }
    m_hoverCard->setStyleSheet(Theme::qss(QStringLiteral(
        "QLabel { background-color: %1; color: %2; border: 1px solid %3;"
        " border-radius: @radius-panel; padding: 10px 12px; font-size: 12px; }")
        .arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder())));
    m_hoverCard->setText(html);
    m_hoverCard->adjustSize();
    const QRect panelG(mapToGlobal(QPoint(0, 0)), size());
    QScreen* scr = screen();
    const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 4000, 4000);
    int x = panelG.right() + 8;
    if (x + m_hoverCard->width() > avail.right()) x = panelG.left() - 8 - m_hoverCard->width();
    const int y = qBound(avail.top() + 4, m_hoverRect.top() - 6, avail.bottom() - m_hoverCard->height() - 4);
    const bool wasShown = m_hoverCard->isVisible();
    m_hoverCard->move(x, y);
    m_hoverCard->show();
    if (!wasShown) PanelMotion::popIn(m_hoverCard);
    m_hoverCard->raise();
}
