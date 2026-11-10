#include "FontPickerPopup.h"

#include "AnchorUtils.h"
#include "Theme.h"

#include <QCursor>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace {

enum Role {
    RoleKind = Qt::UserRole + 1,
    RoleSection,
    RoleFamily,
    RoleParent,
    RoleCount,
    RoleOpen,
};

enum Kind { KindHeader, KindFont, KindVariant, KindEmpty };

constexpr int kMaxRecents = 6;
constexpr int kPopupHeight = 500;
constexpr int kExpanderWidth = 56;
constexpr int kVariantIndent = 14;

const char kRecentsKey[] = "ui/fontPickerRecents";
const char kOpenSectionsKey[] = "ui/fontPickerOpenSections";
const char kOpenFamiliesKey[] = "ui/fontPickerOpenFamilies";

const QString kRecent = QStringLiteral("recent");
const QString kCommon = QStringLiteral("common");
const QString kBook = QStringLiteral("book");
const QString kSans = QStringLiteral("sans");
const QString kScript = QStringLiteral("script");
const QString kTitle = QStringLiteral("title");
const QString kFantasy = QStringLiteral("fantasy");
const QString kHorror = QStringLiteral("horror");
const QString kSciFi = QStringLiteral("scifi");
const QString kMono = QStringLiteral("mono");
const QString kSystem = QStringLiteral("system");
const QString kSymbol = QStringLiteral("symbol");

// Ordem em que aparecem. "recent" e "common" repetem fontes de propósito: são
// atalhos, e a busca os ignora pra não listar a mesma fonte duas vezes.
const QStringList &sectionOrder()
{
    static const QStringList order = { kRecent, kCommon, kBook, kSans, kScript, kTitle,
                                       kFantasy, kHorror, kSciFi, kMono, kSystem, kSymbol };
    return order;
}

bool isShortcutSection(const QString &section)
{
    return section == kRecent || section == kCommon;
}

// Só entram as que existem na máquina; as últimas cobrem o Mac.
const QStringList &commonFamilies()
{
    static const QStringList common = {
        QStringLiteral("Times New Roman"), QStringLiteral("Arial"), QStringLiteral("Cambria"),
        QStringLiteral("Calibri"), QStringLiteral("Georgia"), QStringLiteral("Garamond"),
        QStringLiteral("Verdana"), QStringLiteral("Courier New"),
        QStringLiteral("Helvetica"), QStringLiteral("Helvetica Neue"),
        QStringLiteral("Baskerville"), QStringLiteral("Palatino"),
    };
    return common;
}

// Fontes conhecidas por categoria. O que não está aqui (fonte que a pessoa
// instalou, por exemplo) cai em "Do seu computador", ou em Símbolos e Máquina
// de escrever quando o próprio arquivo da fonte diz que é disso.
const QHash<QString, QString> &catalog()
{
    static const QHash<QString, QString> map = [] {
        QHash<QString, QString> m;
        const auto add = [&m](const QString &section, const char *names) {
            for (const QString &name : QString::fromLatin1(names).split(QLatin1Char(';')))
                m.insert(name.toLower(), section);
        };
        add(kBook, "Alegreya;Alegreya SC;Arvo;Bitter;Bodoni Moda;Bodoni Moda SC;Cormorant Garamond;"
                   "Crete Round;Crimson Pro;Crimson Text;Della Respira;EB Garamond;Gabriela;"
                   "IM FELL English;IM FELL English SC;Libre Baskerville;Lora;Lustria;Oranienbaum;"
                   "Playfair Display;Source Serif 4;Spectral;Spectral SC;Young Serif;"
                   "Times New Roman;Georgia;Cambria;Constantia;Garamond;Book Antiqua;"
                   "Bookman Old Style;Palatino Linotype;Palatino;Baskerville;Baskerville Old Face;"
                   "Bell MT;Bodoni MT;Californian FB;Calisto MT;Centaur;Century;Century Schoolbook;"
                   "Footlight MT;Goudy Old Style;High Tower Text;Lucida Bright;Lucida Fax;"
                   "Modern No. 20;MS Serif;Perpetua;Rockwell;Sitka;Sylfaen");
        add(kSans, "Alegreya Sans;Archivo;League Spartan;Liter;Oswald;Space Grotesk;Arial;Calibri;"
                   "Candara;Corbel;Verdana;Tahoma;Trebuchet MS;Segoe UI;Segoe UI Variable;Bahnschrift;"
                   "Century Gothic;Franklin Gothic;Franklin Gothic Book;Gill Sans;Gill Sans MT;"
                   "Lucida Sans;Lucida Sans Unicode;Microsoft Sans Serif;MS Sans Serif;"
                   "MS Reference Sans Serif;Montserrat;Roboto;Tw Cen MT;Maiandra GD;Berlin Sans FB;"
                   "Sans Serif Collection;Agency FB;Helvetica;Helvetica Neue");
        add(kScript, "Alex Brush;Allura;Ballet;Bilbo Swash Caps;Caveat;Caveat Brush;Corinthia;"
                     "Dancing Script;Edu AU VIC WA NT Guides;Great Vibes;Indie Flower;Meddon;"
                     "Pinyon Script;Playwrite NZ Guides;Reenie Beanie;Rock Salt;Sacramento;Square Peg;"
                     "Blackadder ITC;Bradley Hand ITC;Brush Script MT;Comic Sans MS;"
                     "Edwardian Script ITC;Freestyle Script;French Script MT;Gabriola;Gigi;"
                     "Harlow Solid;Informal Roman;Ink Free;Juice ITC;Kristen ITC;Kunstler Script;"
                     "Lucida Calligraphy;Lucida Handwriting;Matura MT Script Capitals;Mistral;"
                     "Monotype Corsiva;MV Boli;Palace Script MT;Parchment;Pristina;Rage;Script MT;"
                     "Segoe Print;Segoe Script;Tempus Sans ITC;Viner Hand ITC;Vivaldi;Vladimir Script");
        add(kTitle, "Aboreto;Abril Fatface;Alfa Slab One;Allan;Anton;Anton SC;Archivo Black;Bangers;"
                    "Bebas Neue;Bellefair;Big Shoulders;Big Shoulders Inline;Big Shoulders Stencil;"
                    "Boogaloo;Bungee;Bungee Hairline;Bungee Inline;Danfo;DM Serif Display;Faster One;"
                    "Honk;Italiana;Lacquer;Righteous;Rubik 80s Fade;Rubik Mono One;Six Caps;Splash;"
                    "Syne Tactile;Yeseva One;Algerian;Arial Rounded MT;Bauhaus 93;Bernard MT;Britannic;"
                    "Broadway;Castellar;Colonna MT;Cooper;Copperplate Gothic;Curlz MT;Elephant;"
                    "Engravers MT;Eras ITC;Felix Titling;Forte;Gloucester MT;Goudy Stout;"
                    "Haettenschweiler;Harrington;Impact;Imprint MT Shadow;Jokerman;Magneto;"
                    "Niagara Engraved;Niagara Solid;Onyx;Perpetua Titling MT;Playbill;Poor Richard;"
                    "Ravie;Showcard Gothic;Snap ITC;Stencil;Wide Latin");
        add(kFantasy, "Almendra Display;Cinzel;Henny Penny;Macondo Swash Caps;MedievalSharp;"
                      "Metamorphous;New Rocker;Pirata One;Trade Winds;Uncial Antiqua;"
                      "UnifrakturMaguntia;Old English Text MT;Papyrus");
        add(kHorror, "Butcherman;Creepster;Eater;Jolly Lodger;Metal Mania;Nosifer;Rubik Dirt;"
                     "Rubik Glitch;Chiller");
        add(kSciFi, "Aldrich;Audiowide;Black Ops One;Chakra Petch;Electrolize;Exo 2;Iceland;Michroma;"
                    "Monofett;Nova Square;Odibee Sans;Orbitron;Oxanium;Quantico;Rajdhani;"
                    "Saira Stencil One;Sekuya;Share Tech;Stalinist One;Syncopate;Turret Road;"
                    "Unica One;Wallpoet;Zen Tokyo Zoo");
        add(kMono, "Courier New;Courier;Special Elite");
        add(kSymbol, "Wingdings;Wingdings 2;Wingdings 3;Webdings;Symbol;Marlett;Bookshelf Symbol 7;"
                     "MS Outlook;MS Reference Specialty;MT Extra;Segoe Fluent Icons;Segoe MDL2 Assets;"
                     "Segoe UI Emoji;Segoe UI Symbol;TtsNote;Cambria Math");
        // Têm largura fixa, mas não servem pra escrever em português.
        add(kSystem, "8514oem;Fixedsys;Terminal;MS Gothic;NSimSun;SimSun-ExtB;SimSun-ExtG");
        return m;
    }();
    return map;
}

QString sectionFor(const QString &family)
{
    const auto it = catalog().constFind(family.toLower());
    if (it != catalog().constEnd()) return it.value();
    const auto systems = QFontDatabase::writingSystems(family);
    const bool latin = systems.contains(QFontDatabase::Latin);
    if (systems.contains(QFontDatabase::Symbol) && !latin) return kSymbol;
    if (latin && QFontDatabase::isFixedPitch(family)) return kMono;
    return kSystem;
}

// Se `family` é só um peso de outra família da lista, devolve a família.
QString parentFamilyOf(const QString &family, const QSet<QString> &all)
{
    static const QRegularExpression weight(
        QStringLiteral("^(.+) (Black|ExtraBold|ExtraLight|Light|Medium|SemiBold|Thin)$"));
    const auto match = weight.match(family);
    if (match.hasMatch() && all.contains(match.captured(1))) return match.captured(1);
    // Famílias cujos estilos têm nome próprio em vez de peso.
    for (const QString &base : { QStringLiteral("Honk"), QStringLiteral("Danfo") }) {
        if (family.startsWith(base + QLatin1Char(' ')) && all.contains(base)) return base;
    }
    return {};
}

class FontRowDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    // Altura fixa de propósito: medir pela fonte de cada linha obrigaria o Qt a
    // carregar centenas de fontes antes de mostrar a lista.
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        switch (index.data(RoleKind).toInt()) {
        case KindHeader: return { option.rect.width(), 28 };
        case KindEmpty:  return { option.rect.width(), 64 };
        default:         return { option.rect.width(), 32 };
        }
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const int kind = index.data(RoleKind).toInt();
        if (kind == KindVariant) opt.rect.adjust(kVariantIndent, 0, 0, 0);

        // O estilo pinta só o fundo (hover e seleção vêm do QSS); o resto é daqui.
        const QString text = opt.text;
        const QFont rowFont = opt.font;
        opt.text.clear();
        opt.widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        const QColor muted = Theme::toColor(Theme::textMuted());
        const QColor primary = Theme::toColor(Theme::textPrimary());
        QRect area = opt.rect.adjusted(12, 0, -12, 0);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        if (kind == KindEmpty) {
            painter->setPen(muted);
            painter->drawText(opt.rect, Qt::AlignCenter, text);
            painter->restore();
            return;
        }

        const int count = index.data(RoleCount).toInt();
        const bool expandable = kind == KindHeader || (kind == KindFont && count > 0);
        if (expandable) {
            const bool open = index.data(RoleOpen).toBool();
            const QPointF c(area.right() - 4.0, area.center().y() + 0.5);
            painter->setPen(QPen(muted, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPainterPath chevron;
            if (open) {
                chevron.moveTo(c + QPointF(-4, -2));
                chevron.lineTo(c + QPointF(0, 2));
                chevron.lineTo(c + QPointF(4, -2));
            } else {
                chevron.moveTo(c + QPointF(-2, -4));
                chevron.lineTo(c + QPointF(2, 0));
                chevron.lineTo(c + QPointF(-2, 4));
            }
            painter->drawPath(chevron);
        }

        QFont uiFont = opt.widget->font();
        if (count > 0) {
            QFont small = uiFont;
            small.setPixelSize(11);
            painter->setFont(small);
            painter->setPen(muted);
            const QString label = kind == KindHeader ? QString::number(count)
                                                     : QStringLiteral("+%1").arg(count);
            painter->drawText(area.adjusted(0, 0, -18, 0), Qt::AlignRight | Qt::AlignVCenter, label);
        }
        if (expandable) area.adjust(0, 0, -(kExpanderWidth - 12), 0);

        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setClipRect(opt.rect);
        if (kind == KindHeader) {
            uiFont.setPixelSize(11);
            uiFont.setBold(true);
            painter->setFont(uiFont);
            painter->setPen(muted);
        } else {
            painter->setFont(rowFont);
            painter->setPen(primary);
        }
        const QFontMetrics metrics(painter->font());
        painter->drawText(area, Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(text, Qt::ElideRight, area.width()));
        painter->restore();
    }
};

} // namespace

FontPickerPopup::FontPickerPopup(QWidget *parent)
    : QFrame(parent, Qt::Popup)
    , search(new QLineEdit(this))
    , list(new QListWidget(this))
{
    setObjectName(QStringLiteral("fontPickerPopup"));
    setFrameShape(QFrame::NoFrame);

    search->setObjectName(QStringLiteral("fontPickerSearch"));
    search->setPlaceholderText(tr("Buscar fonte…"));
    search->installEventFilter(this);

    list->setObjectName(QStringLiteral("fontPickerList"));
    list->setItemDelegate(new FontRowDelegate(list));
    list->setUniformItemSizes(false);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setMouseTracking(true);
    // O foco fica sempre na busca: dá pra digitar a qualquer momento, e as setas
    // chegam por um caminho só (eventFilter).
    list->setFocusPolicy(Qt::NoFocus);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 4);
    layout->setSpacing(6);
    layout->addWidget(search);
    layout->addWidget(list);

    setFixedSize(300, kPopupHeight);

    const QSettings settings;
    m_recents = settings.value(QLatin1String(kRecentsKey)).toStringList();
    const QStringList openSections = settings.value(QLatin1String(kOpenSectionsKey),
                                                    QStringList{ kRecent, kCommon }).toStringList();
    m_openSections = QSet<QString>(openSections.begin(), openSections.end());
    const QStringList openFamilies = settings.value(QLatin1String(kOpenFamiliesKey)).toStringList();
    m_openFamilies = QSet<QString>(openFamilies.begin(), openFamilies.end());

    connect(search, &QLineEdit::textChanged, this, [this](const QString &text) {
        applyVisibility();
        if (text.trimmed().isEmpty()) selectCurrentFont();
        else selectFirstMatch();
    });

    connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item) return;
        const int kind = item->data(RoleKind).toInt();
        if (kind == KindHeader) {
            toggleSection(item);
            selectCurrentFont();
            return;
        }
        if (kind == KindFont && item->data(RoleCount).toInt() > 0
            && search->text().trimmed().isEmpty()) {
            const QRect row = list->visualItemRect(item);
            const QPoint at = list->viewport()->mapFromGlobal(QCursor::pos());
            if (at.x() >= row.right() - kExpanderWidth) {
                toggleFamily(item);
                selectCurrentFont();
                return;
            }
        }
        activate(item);
    });
}

QString FontPickerPopup::sectionTitle(const QString &section) const
{
    if (section == kRecent) return tr("Usadas recentemente");
    if (section == kCommon) return tr("Mais comuns");
    if (section == kBook) return tr("Para livros");
    if (section == kSans) return tr("Sem serifa");
    if (section == kScript) return tr("Manuscritas");
    if (section == kTitle) return tr("Títulos e cartazes");
    if (section == kFantasy) return tr("Fantasia e época");
    if (section == kHorror) return tr("Terror");
    if (section == kSciFi) return tr("Ficção científica");
    if (section == kMono) return tr("Máquina de escrever");
    if (section == kSystem) return tr("Do seu computador");
    return tr("Símbolos");
}

void FontPickerPopup::setFontFamilies(const QStringList &families, const QString &current)
{
    currentFamily = current;
    rememberRecent(current);
    if (families != m_families) {
        m_families = families;
        rebuildCatalog();
    }
    populate();
}

void FontPickerPopup::rebuildCatalog()
{
    m_sections.clear();
    const QSet<QString> all(m_families.begin(), m_families.end());

    QHash<QString, QStringList> variantsOf;
    for (const QString &family : std::as_const(m_families)) {
        const QString parent = parentFamilyOf(family, all);
        if (!parent.isEmpty()) variantsOf[parent].append(family);
    }
    for (const QString &family : std::as_const(m_families)) {
        if (!parentFamilyOf(family, all).isEmpty()) continue;
        m_sections[sectionFor(family)].append({ family, variantsOf.value(family) });
    }
}

void FontPickerPopup::populate()
{
    list->clear();

    QHash<QString, QString> realName;
    for (const QString &family : std::as_const(m_families)) realName.insert(family.toLower(), family);

    QHash<QString, QVector<FontEntry>> shown = m_sections;
    for (const QString &family : std::as_const(m_recents)) {
        const QString real = realName.value(family.toLower());
        if (!real.isEmpty()) shown[kRecent].append({ real, {} });
    }
    for (const QString &family : commonFamilies()) {
        const QString real = realName.value(family.toLower());
        if (!real.isEmpty()) shown[kCommon].append({ real, {} });
    }

    const auto addRow = [this](int kind, const QString &text, const QString &section,
                               const QString &family, const QString &parent, int count) {
        auto *item = new QListWidgetItem(text);
        item->setData(RoleKind, kind);
        item->setData(RoleSection, section);
        item->setData(RoleFamily, family);
        item->setData(RoleParent, parent);
        item->setData(RoleCount, count);
        // Fonte de símbolo escreveria o próprio nome em pictogramas.
        if (kind != KindHeader && section != kSymbol)
            item->setData(Qt::FontRole, QFont(family, 12));
        list->addItem(item);
        return item;
    };

    for (const QString &section : sectionOrder()) {
        const QVector<FontEntry> entries = shown.value(section);
        if (entries.isEmpty()) continue;
        const int count = isShortcutSection(section) ? 0 : int(entries.size());
        addRow(KindHeader, sectionTitle(section), section, QString(), QString(), count);
        for (const FontEntry &entry : entries) {
            addRow(KindFont, entry.family, section, entry.family, QString(), int(entry.variants.size()));
            for (const QString &variant : entry.variants)
                addRow(KindVariant, variant, section, variant, entry.family, 0);
        }
    }

    auto *empty = new QListWidgetItem(tr("Nenhuma fonte encontrada"));
    empty->setData(RoleKind, KindEmpty);
    empty->setFlags(Qt::NoItemFlags);
    list->addItem(empty);
}

void FontPickerPopup::applyVisibility()
{
    const QString query = search->text().trimmed();
    const bool searching = !query.isEmpty();
    int matches = 0;
    QListWidgetItem *empty = nullptr;

    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem *item = list->item(i);
        const int kind = item->data(RoleKind).toInt();
        const QString section = item->data(RoleSection).toString();
        bool show = false;

        if (kind == KindEmpty) {
            empty = item;
            continue;
        }
        if (kind == KindHeader) {
            item->setData(RoleOpen, m_openSections.contains(section));
            show = !searching;
        } else if (searching) {
            show = !isShortcutSection(section)
                && item->data(RoleFamily).toString().contains(query, Qt::CaseInsensitive);
            if (show) ++matches;
            item->setData(RoleOpen, true); // na busca, os pesos já aparecem soltos
        } else if (kind == KindFont) {
            item->setData(RoleOpen, m_openFamilies.contains(item->data(RoleFamily).toString()));
            show = m_openSections.contains(section);
        } else {
            show = m_openSections.contains(section)
                && m_openFamilies.contains(item->data(RoleParent).toString());
        }
        item->setHidden(!show);
    }
    if (empty) empty->setHidden(!searching || matches > 0);
}

void FontPickerPopup::selectCurrentFont()
{
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem *item = list->item(i);
        if (item->isHidden() || item->data(RoleKind).toInt() == KindHeader) continue;
        if (item->data(RoleFamily).toString().compare(currentFamily, Qt::CaseInsensitive) == 0) {
            list->setCurrentItem(item);
            return;
        }
    }
    list->setCurrentRow(-1);
}

void FontPickerPopup::selectFirstMatch()
{
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem *item = list->item(i);
        if (item->isHidden() || item->data(RoleKind).toInt() == KindEmpty) continue;
        list->setCurrentItem(item);
        list->scrollToItem(item);
        return;
    }
    list->setCurrentRow(-1);
}

void FontPickerPopup::moveCursor(int step)
{
    for (int i = list->currentRow() + step; i >= 0 && i < list->count(); i += step) {
        QListWidgetItem *item = list->item(i);
        if (item->isHidden() || item->data(RoleKind).toInt() == KindEmpty) continue;
        list->setCurrentItem(item);
        list->scrollToItem(item);
        return;
    }
}

void FontPickerPopup::activate(QListWidgetItem *item)
{
    if (!item) return;
    switch (item->data(RoleKind).toInt()) {
    case KindHeader: toggleSection(item); break;
    case KindFont:
    case KindVariant: choose(item->data(RoleFamily).toString()); break;
    default: break;
    }
}

void FontPickerPopup::toggleSection(QListWidgetItem *header)
{
    const QString section = header->data(RoleSection).toString();
    if (!m_openSections.remove(section)) m_openSections.insert(section);
    saveOpenState();
    applyVisibility();
}

void FontPickerPopup::toggleFamily(QListWidgetItem *row)
{
    const QString family = row->data(RoleFamily).toString();
    if (!m_openFamilies.remove(family)) m_openFamilies.insert(family);
    saveOpenState();
    applyVisibility();
}

void FontPickerPopup::choose(const QString &family)
{
    currentFamily = family;
    rememberRecent(family);
    emit fontSelected(family);
    hide();
}

void FontPickerPopup::rememberRecent(const QString &family)
{
    if (family.isEmpty()) return;
    m_recents.removeAll(family);
    m_recents.prepend(family);
    while (m_recents.size() > kMaxRecents) m_recents.removeLast();
    QSettings().setValue(QLatin1String(kRecentsKey), m_recents);
}

void FontPickerPopup::saveOpenState() const
{
    QSettings settings;
    settings.setValue(QLatin1String(kOpenSectionsKey), QStringList(m_openSections.begin(), m_openSections.end()));
    settings.setValue(QLatin1String(kOpenFamiliesKey), QStringList(m_openFamilies.begin(), m_openFamilies.end()));
}

bool FontPickerPopup::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == search && event->type() == QEvent::KeyPress) {
        switch (static_cast<QKeyEvent *>(event)->key()) {
        case Qt::Key_Down: moveCursor(1); return true;
        case Qt::Key_Up: moveCursor(-1); return true;
        case Qt::Key_Return:
        case Qt::Key_Enter: activate(list->currentItem()); return true;
        case Qt::Key_Escape: hide(); return true;
        default: break;
        }
    }
    return QFrame::eventFilter(watched, event);
}

void FontPickerPopup::showNear(const QRect &anchorGlobal, Qt::Edge barSide)
{
    {
        const QSignalBlocker blocker(search);
        search->clear();
    }
    applyVisibility();
    selectCurrentFont();
    list->scrollToTop();

    QPoint pos = anchorGlobal.bottomLeft();
    const QScreen *screen = QGuiApplication::screenAt(anchorGlobal.center());
    if (screen) {
        const QRect available = screen->availableGeometry();
        setFixedHeight(qMin(kPopupHeight, available.height() - 80));
        pos = AnchorUtils::positionNear(anchorGlobal, size(), barSide, available);
    }
    move(pos);
    show();
    search->setFocus();
}
