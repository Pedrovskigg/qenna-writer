#include "DrawerCreateDialog.h"
#include "ColorPopover.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "Theme.h"
#include "TimelineTracksTypes.h"

#include <QColor>
#include <QButtonGroup>
#include <QPainter>
#include <QToolButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSize>
#include <QVBoxLayout>

namespace {

constexpr int kIconRenderSize = 18;

struct IconEntry {
    const char* id;
    const char* label;
};

// Os labels usam QT_TRANSLATE_NOOP (só marca pro lupdate extrair) porque o
// catálogo é um array estático de const char* fora de qualquer classe — a
// tradução de fato acontece em DrawerCreateDialog::tr(e.label) no ponto de uso.
const QList<IconEntry>& drawerIconCatalogRaw() {
    static const QList<IconEntry> kCatalog = {
        { "user",       QT_TRANSLATE_NOOP("DrawerCreateDialog", "Pessoa") },
        { "map",        QT_TRANSLATE_NOOP("DrawerCreateDialog", "Mapa") },
        { "cube",       QT_TRANSLATE_NOOP("DrawerCreateDialog", "Cubo") },
        { "planet",     QT_TRANSLATE_NOOP("DrawerCreateDialog", "Planeta") },
        { "moon",       QT_TRANSLATE_NOOP("DrawerCreateDialog", "Lua") },
        { "sun",        QT_TRANSLATE_NOOP("DrawerCreateDialog", "Sol") },
        { "sword",      QT_TRANSLATE_NOOP("DrawerCreateDialog", "Espada") },
        { "gun",        QT_TRANSLATE_NOOP("DrawerCreateDialog", "Arma") },
        { "car",        QT_TRANSLATE_NOOP("DrawerCreateDialog", "Carro") },
        { "pin",        QT_TRANSLATE_NOOP("DrawerCreateDialog", "Pin") },
        { "star",       QT_TRANSLATE_NOOP("DrawerCreateDialog", "Estrela") },
        { "crown",      QT_TRANSLATE_NOOP("DrawerCreateDialog", "Coroa") },
        { "heart",      QT_TRANSLATE_NOOP("DrawerCreateDialog", "Coração") },
        { "tree",       QT_TRANSLATE_NOOP("DrawerCreateDialog", "Árvore") },
        { "castle",     QT_TRANSLATE_NOOP("DrawerCreateDialog", "Castelo") },
        { "flask",      QT_TRANSLATE_NOOP("DrawerCreateDialog", "Frasco") },
        { "skull",      QT_TRANSLATE_NOOP("DrawerCreateDialog", "Caveira") },
        { "book",       QT_TRANSLATE_NOOP("DrawerCreateDialog", "Livro") },
        { "file",       QT_TRANSLATE_NOOP("DrawerCreateDialog", "Arquivo") },
        { "drawer",     QT_TRANSLATE_NOOP("DrawerCreateDialog", "Gaveta") },
        { "manuscript", QT_TRANSLATE_NOOP("DrawerCreateDialog", "Manuscrito") },
        { "chapter",    QT_TRANSLATE_NOOP("DrawerCreateDialog", "Capítulo") },
    };
    return kCatalog;
}

bool matchesAny(const QString& text, std::initializer_list<const char*> keywords) {
    for (const char* k : keywords) {
        if (text.contains(QLatin1String(k))) return true;
    }
    return false;
}

QString normalizeForSearch(const QString& s) {
    QString out = s.toLower();
    // Remove acentos básicos
    out.replace(QStringLiteral("á"), QStringLiteral("a"));
    out.replace(QStringLiteral("â"), QStringLiteral("a"));
    out.replace(QStringLiteral("ã"), QStringLiteral("a"));
    out.replace(QStringLiteral("à"), QStringLiteral("a"));
    out.replace(QStringLiteral("é"), QStringLiteral("e"));
    out.replace(QStringLiteral("ê"), QStringLiteral("e"));
    out.replace(QStringLiteral("í"), QStringLiteral("i"));
    out.replace(QStringLiteral("ó"), QStringLiteral("o"));
    out.replace(QStringLiteral("ô"), QStringLiteral("o"));
    out.replace(QStringLiteral("õ"), QStringLiteral("o"));
    out.replace(QStringLiteral("ú"), QStringLiteral("u"));
    out.replace(QStringLiteral("ç"), QStringLiteral("c"));
    return out;
}

} // namespace

QStringList DrawerCreateDialog::drawerIconCatalog() {
    QStringList ids;
    for (const auto& e : drawerIconCatalogRaw()) ids.append(QString::fromLatin1(e.id));
    return ids;
}

QString DrawerCreateDialog::iconLabel(const QString& iconId) {
    for (const auto& e : drawerIconCatalogRaw()) {
        if (QString::fromLatin1(e.id) == iconId) return DrawerCreateDialog::tr(e.label);
    }
    return iconId;
}

// Heurística por keyword (espelha getAutoDrawerIconId do Mira 1).
QString DrawerCreateDialog::autoIconFromTitle(const QString& title) {
    const QString t = normalizeForSearch(title);
    if (t.trimmed().isEmpty()) return QStringLiteral("drawer");

    if (matchesAny(t, { "personagem", "personagens", "character", "pessoa", "pessoas", "people", "npc", "cast", "elenco" }))
        return QStringLiteral("user");
    if (matchesAny(t, { "cenario", "cenarios", "local", "locais", "mapa", "mapas", "setting", "settings", "place", "places" }))
        return QStringLiteral("map");
    if (matchesAny(t, { "objeto", "objetos", "item", "itens", "arma", "armas", "weapon", "weapons" }))
        return QStringLiteral("cube");
    if (matchesAny(t, { "lore", "historia", "mitologia", "mito", "religiao", "religion" }))
        return QStringLiteral("book");
    if (matchesAny(t, { "timeline", "linha do tempo", "evento", "eventos", "plano", "planos" }))
        return QStringLiteral("pin");
    if (matchesAny(t, { "nota", "notas", "ideia", "ideias", "note", "notes" }))
        return QStringLiteral("file");
    if (matchesAny(t, { "manuscrito", "manuscritos", "manuscript", "livro", "livros", "book" }))
        return QStringLiteral("manuscript");
    if (matchesAny(t, { "capitulo", "capitulos", "chapter", "chapters" }))
        return QStringLiteral("chapter");
    return QStringLiteral("drawer");
}

namespace {
// Cores prontas da gaveta (a primeira é o padrão de sempre).
const char* kPresetColors[] = { "#2b79ff", "#0f9d8a", "#46a758", "#c9a227", "#f76b15",
                                "#e5484d", "#d6409f", "#8e4ec6", "#6e7ea3", "#8d8d8d" };
} // namespace

DrawerCreateDialog::DrawerCreateDialog(ElementsStore* store, QWidget* parent)
    : SheetDialog(parent, 440)
    , m_store(store)
{
    setEyebrow(tr("Nova gaveta"));
    QWidget* c = card();
    const Tracks::Palette pal = Tracks::Palette::current();

    // selo do ícone na cor da gaveta + nome grande
    auto* top = new QHBoxLayout;
    top->setSpacing(14);
    m_badge = new QLabel(c);
    m_badge->setFixedSize(48, 48);
    top->addWidget(m_badge, 0, Qt::AlignVCenter);
    m_nameEdit = titleEdit(c, false, 24);
    m_nameEdit->setPlaceholderText(tr("Nome da gaveta"));
    confirmOnEnter(m_nameEdit);
    top->addWidget(m_nameEdit, 1);
    body()->addLayout(top);

    // ícones em grade
    {
        auto* g = new QVBoxLayout;
        g->setSpacing(7);
        g->addWidget(sectionLabel(tr("Ícone"), c));
        m_iconGroup = new QButtonGroup(this);
        m_iconGroup->setExclusive(true);
        const auto& cat = drawerIconCatalogRaw();
        QHBoxLayout* row = nullptr;
        for (int i = 0; i < cat.size(); ++i) {
            if (i % 11 == 0) { row = new QHBoxLayout; row->setSpacing(4); g->addLayout(row); }
            const QString id = QString::fromLatin1(cat[i].id);
            auto* b = new QToolButton(c);
            b->setObjectName(QStringLiteral("sheetIcon"));
            b->setCheckable(true);
            b->setFixedSize(30, 30);
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            b->setToolTip(DrawerCreateDialog::tr(cat[i].label));
            b->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/%1.svg").arg(id),
                                                  pal.ink, pal.ink, pal.ink, QSize(kIconRenderSize, kIconRenderSize)));
            b->setIconSize(QSize(kIconRenderSize, kIconRenderSize));
            b->setProperty("iconId", id);
            m_iconGroup->addButton(b, i);
            row->addWidget(b);
        }
        if (row) row->addStretch(1);
        connect(m_iconGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* b) {
            m_iconManual = true;   // escolha manual: para de seguir o nome
            m_icon = b->property("iconId").toString();
            refreshBadge();
        });
        body()->addLayout(g);
    }

    // cores prontas + "outra…"
    {
        auto* g = new QVBoxLayout;
        g->setSpacing(7);
        g->addWidget(sectionLabel(tr("Cor"), c));
        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        m_colorGroup = new QButtonGroup(this);
        m_colorGroup->setExclusive(true);
        for (int i = 0; i < int(sizeof(kPresetColors) / sizeof(kPresetColors[0])); ++i) {
            const QString hex = QString::fromLatin1(kPresetColors[i]);
            auto* b = new QToolButton(c);
            b->setCheckable(true);
            b->setFixedSize(24, 24);
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            b->setProperty("hex", hex);
            b->setStyleSheet(QStringLiteral(
                "QToolButton { background: %1; border: 2px solid %2; border-radius: 12px; }"
                "QToolButton:hover { border-color: %3; }"
                "QToolButton:checked { border: 2px solid %4; }")
                .arg(hex, pal.page.name(), Tracks::alpha(pal.ink, 0.35).name(QColor::HexArgb), pal.bright.name()));
            m_colorGroup->addButton(b, i);
            row->addWidget(b);
        }
        auto* other = new QToolButton(c);
        other->setObjectName(QStringLiteral("sheetLink"));
        other->setText(tr("outra…"));
        other->setCursor(Qt::PointingHandCursor);
        other->setFocusPolicy(Qt::NoFocus);
        connect(other, &QToolButton::clicked, this, &DrawerCreateDialog::onPickColor);
        row->addSpacing(4);
        row->addWidget(other);
        row->addStretch(1);
        connect(m_colorGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* b) {
            m_color = b->property("hex").toString();
            refreshBadge();
        });
        g->addLayout(row);
        body()->addLayout(g);
    }

    // tipo de elemento
    {
        auto* g = new QVBoxLayout;
        g->setSpacing(7);
        g->addWidget(sectionLabel(tr("Elemento"), c));
        m_typeGroup = new QButtonGroup(this);
        m_typeGroup->setExclusive(true);
        QStringList labels = { tr("Automático") };
        m_typeIds = QStringList{ QString() };
        if (m_store)
            for (const auto& t : m_store->elementTypes()) { labels << t.label; m_typeIds << t.id; }
        QHBoxLayout* row = nullptr;
        for (int i = 0; i < labels.size(); ++i) {
            if (i % 4 == 0) { row = new QHBoxLayout; row->setSpacing(5); g->addLayout(row); }
            auto* b = pill(labels[i], c);
            m_typeGroup->addButton(b, i);
            row->addWidget(b);
        }
        if (row) row->addStretch(1);
        m_typeGroup->button(0)->setChecked(true);
        auto* hint = new QLabel(tr("Automático deixa o Qenna decidir pelo nome da gaveta."), c);
        hint->setObjectName(QStringLiteral("sheetDim"));
        hint->setFont(Tracks::uiFont(11));
        g->addWidget(hint);
        body()->addLayout(g);
    }

    QPushButton* ok = addFooter(tr("Criar"), tr("cria"));
    auto sync = [this, ok]() { ok->setEnabled(!m_nameEdit->text().trimmed().isEmpty()); };
    connect(m_nameEdit, &QLineEdit::textChanged, this, sync);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &DrawerCreateDialog::onNameChanged);
    sync();

    applySheetTheme(QStringLiteral(
        "QToolButton#sheetIcon { background: transparent; border: 1px solid transparent; border-radius: 6px; }"
        "QToolButton#sheetIcon:hover { background: %1; }"
        "QToolButton#sheetIcon:checked { border-color: %2; background: %3; }")
        .arg(Tracks::alpha(pal.ink, 0.08).name(QColor::HexArgb), Tracks::alpha(pal.accent, 0.65).name(QColor::HexArgb),
             Tracks::alpha(pal.accent, 0.15).name(QColor::HexArgb)));
    setIcon(m_icon);
    setColor(m_color);
}

void DrawerCreateDialog::showEvent(QShowEvent* e)
{
    SheetDialog::showEvent(e);
    m_nameEdit->setFocus();
    if (!m_nameEdit->text().isEmpty()) m_nameEdit->selectAll();
}

void DrawerCreateDialog::setIcon(const QString& id)
{
    m_icon = id;
    for (QAbstractButton* b : m_iconGroup->buttons())
        if (b->property("iconId").toString() == id) { b->setChecked(true); break; }
    refreshBadge();
}

void DrawerCreateDialog::setColor(const QString& hex)
{
    m_color = hex;
    bool preset = false;
    for (QAbstractButton* b : m_colorGroup->buttons())
        if (b->property("hex").toString().compare(hex, Qt::CaseInsensitive) == 0) { b->setChecked(true); preset = true; break; }
    if (!preset) {
        // cor escolhida no seletor: nenhuma pronta fica marcada
        m_colorGroup->setExclusive(false);
        for (QAbstractButton* b : m_colorGroup->buttons()) b->setChecked(false);
        m_colorGroup->setExclusive(true);
    }
    refreshBadge();
}

void DrawerCreateDialog::refreshBadge()
{
    const QColor col(m_color);
    const qreal dpr = devicePixelRatioF();
    QPixmap pm(QSize(48, 48) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QColor bg = col; bg.setAlphaF(0.18f);
    QColor bd = col; bd.setAlphaF(0.55f);
    p.setPen(QPen(bd, 1));
    p.setBrush(bg);
    p.drawRoundedRect(QRectF(0.5, 0.5, 47, 47), 10, 10);
    const QIcon ic = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/%1.svg").arg(m_icon),
                                                col, col, col, QSize(24, 24));
    ic.paint(&p, QRect(12, 12, 24, 24));
    p.end();
    m_badge->setPixmap(pm);
}

void DrawerCreateDialog::onNameChanged(const QString& text) {
    if (m_iconManual) return;
    setIcon(autoIconFromTitle(text));
}

void DrawerCreateDialog::onPickColor() {
    const QColor chosen = ColorPopover::getColor(QColor(m_color), this, tr("Cor da gaveta"));
    if (!chosen.isValid()) return;
    setColor(chosen.name(QColor::HexRgb));
}

QString DrawerCreateDialog::title() const {
    return m_nameEdit->text().trimmed();
}

QString DrawerCreateDialog::iconId() const {
    return m_icon;
}

QString DrawerCreateDialog::color() const {
    return m_color;
}

QString DrawerCreateDialog::elementTypeId() const {
    const int i = m_typeGroup->checkedId();
    return i >= 0 && i < m_typeIds.size() ? m_typeIds[i] : QString();
}

void DrawerCreateDialog::configureForEdit(const QString& title,
                                          const QString& iconId,
                                          const QString& color,
                                          const QString& elementTypeId) {
    setEyebrow(tr("Editar gaveta"));
    if (m_okBtn) m_okBtn->setText(tr("Salvar"));
    setEnterHint(tr("salva"));

    // Bloqueia auto-segue de ícone por nome — o usuário já escolheu antes.
    m_iconManual = true;
    {
        QSignalBlocker block(m_nameEdit);
        m_nameEdit->setText(title);
    }
    if (m_okBtn) m_okBtn->setEnabled(!title.trimmed().isEmpty());
    if (!iconId.isEmpty()) setIcon(iconId);
    if (!color.isEmpty()) setColor(color);
    if (!elementTypeId.isNull()) {
        const int i = m_typeIds.indexOf(elementTypeId);
        if (i >= 0) m_typeGroup->button(i)->setChecked(true);
    }
}
