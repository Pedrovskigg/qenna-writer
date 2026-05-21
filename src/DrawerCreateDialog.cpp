#include "DrawerCreateDialog.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "Theme.h"

#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSize>
#include <QVBoxLayout>

namespace {

constexpr int kIconRenderSize = 18;
constexpr auto kIconColor = "#d8d3c6";

struct IconEntry {
    const char* id;
    const char* label;
};

const QList<IconEntry>& drawerIconCatalogRaw() {
    static const QList<IconEntry> kCatalog = {
        { "user",       "Pessoa" },
        { "map",        "Mapa" },
        { "cube",       "Cubo" },
        { "planet",     "Planeta" },
        { "moon",       "Lua" },
        { "sun",        "Sol" },
        { "sword",      "Espada" },
        { "gun",        "Arma" },
        { "car",        "Carro" },
        { "pin",        "Pin" },
        { "star",       "Estrela" },
        { "crown",      "Coroa" },
        { "heart",      "Coração" },
        { "tree",       "Árvore" },
        { "castle",     "Castelo" },
        { "flask",      "Frasco" },
        { "skull",      "Caveira" },
        { "book",       "Livro" },
        { "file",       "Arquivo" },
        { "drawer",     "Gaveta" },
        { "manuscript", "Manuscrito" },
        { "chapter",    "Capítulo" },
    };
    return kCatalog;
}

QIcon loadElementIcon(const QString& id) {
    return IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/elements/%1.svg").arg(id),
        QColor(kIconColor),
        QColor(kIconColor),
        QColor(kIconColor),
        QSize(kIconRenderSize, kIconRenderSize));
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
        if (QString::fromLatin1(e.id) == iconId) return QString::fromUtf8(e.label);
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

DrawerCreateDialog::DrawerCreateDialog(ElementsStore* store, QWidget* parent)
    : QDialog(parent)
    , m_store(store)
    , m_nameEdit(new QLineEdit(this))
    , m_iconCombo(new QComboBox(this))
    , m_colorBtn(new QPushButton(this))
    , m_typeCombo(new QComboBox(this))
    , m_buttons(nullptr)
    , m_color(QStringLiteral("#2b79ff"))
{
    setWindowTitle(tr("Nova gaveta"));
    setModal(true);

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    // ---- Ícone + Cor (na mesma linha) ----
    populateIcons();
    m_iconCombo->setIconSize(QSize(kIconRenderSize, kIconRenderSize));
    m_iconCombo->setMinimumHeight(32);

    m_colorBtn->setFixedSize(40, 32);
    m_colorBtn->setCursor(Qt::PointingHandCursor);
    m_colorBtn->setToolTip(tr("Escolher cor"));
    updateColorSwatch();

    auto* iconColorRow = new QHBoxLayout();
    iconColorRow->setContentsMargins(0, 0, 0, 0);
    iconColorRow->setSpacing(8);
    iconColorRow->addWidget(m_iconCombo, /*stretch=*/1);
    iconColorRow->addWidget(m_colorBtn);

    auto* iconColorContainer = new QWidget(this);
    iconColorContainer->setLayout(iconColorRow);
    form->addRow(tr("Ícone / Cor"), iconColorContainer);

    // ---- Nome ----
    m_nameEdit->setPlaceholderText(tr("Digite o nome da gaveta"));
    m_nameEdit->setMinimumHeight(32);
    form->addRow(tr("Nome"), m_nameEdit);

    // ---- Tipo de elemento ----
    populateElementTypes();
    m_typeCombo->setMinimumHeight(32);
    form->addRow(tr("Elemento"), m_typeCombo);

    // ---- Botões OK/Cancelar ----
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Criar"));
    m_buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 16);
    mainLayout->setSpacing(16);
    mainLayout->addLayout(form);
    mainLayout->addWidget(m_buttons);

    setMinimumWidth(420);

    // Eventos
    connect(m_nameEdit, &QLineEdit::textChanged, this, &DrawerCreateDialog::onNameChanged);
    connect(m_colorBtn, &QPushButton::clicked, this, &DrawerCreateDialog::onPickColor);
    connect(m_iconCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        // Clique manual no dropdown desativa o auto-segue por nome.
        m_iconManual = true;
    });

    m_nameEdit->setFocus();
}

void DrawerCreateDialog::populateIcons() {
    m_iconCombo->clear();
    for (const auto& e : drawerIconCatalogRaw()) {
        const QString id = QString::fromLatin1(e.id);
        const QString label = QString::fromUtf8(e.label);
        QIcon icon = loadElementIcon(id);
        m_iconCombo->addItem(icon, label, id);
    }
    // Padrão: "drawer" (ícone genérico).
    const int idx = m_iconCombo->findData(QStringLiteral("drawer"));
    if (idx >= 0) m_iconCombo->setCurrentIndex(idx);
}

void DrawerCreateDialog::populateElementTypes() {
    m_typeCombo->clear();
    m_typeCombo->addItem(tr("Automático"), QString());
    if (m_store) {
        for (const auto& t : m_store->elementTypes()) {
            m_typeCombo->addItem(t.label, t.id);
        }
    }
}

void DrawerCreateDialog::onNameChanged(const QString& text) {
    if (m_iconManual) return;
    const QString autoId = autoIconFromTitle(text);
    const int idx = m_iconCombo->findData(autoId);
    if (idx >= 0 && idx != m_iconCombo->currentIndex()) {
        // bloquear o sinal activated pra não setar m_iconManual = true
        QSignalBlocker block(m_iconCombo);
        m_iconCombo->setCurrentIndex(idx);
    }
}

void DrawerCreateDialog::onPickColor() {
    const QColor initial(m_color);
    const QColor chosen = QColorDialog::getColor(initial, this, tr("Cor da gaveta"));
    if (!chosen.isValid()) return;
    m_color = chosen.name(QColor::HexRgb);
    updateColorSwatch();
}

void DrawerCreateDialog::updateColorSwatch() {
    // Bordinha sutil pra cores claras não sumirem no fundo claro do botão.
    m_colorBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "}"
        "QPushButton:hover { border-color: %3; }"
    ).arg(m_color, Theme::panelBorder(), Theme::textPrimary()));
}

QString DrawerCreateDialog::title() const {
    return m_nameEdit->text().trimmed();
}

QString DrawerCreateDialog::iconId() const {
    return m_iconCombo->currentData().toString();
}

QString DrawerCreateDialog::color() const {
    return m_color;
}

QString DrawerCreateDialog::elementTypeId() const {
    return m_typeCombo->currentData().toString();
}

void DrawerCreateDialog::configureForEdit(const QString& title,
                                          const QString& iconId,
                                          const QString& color,
                                          const QString& elementTypeId) {
    setWindowTitle(tr("Editar gaveta"));
    if (m_buttons) m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Salvar"));

    // Bloqueia auto-segue de ícone por nome — o usuário já escolheu antes.
    m_iconManual = true;

    {
        QSignalBlocker block(m_nameEdit);
        m_nameEdit->setText(title);
    }

    if (!iconId.isEmpty()) {
        const int idx = m_iconCombo->findData(iconId);
        if (idx >= 0) {
            QSignalBlocker block(m_iconCombo);
            m_iconCombo->setCurrentIndex(idx);
        }
    }

    if (!color.isEmpty()) {
        m_color = color;
        updateColorSwatch();
    }

    if (!elementTypeId.isNull()) {
        const int idx = m_typeCombo->findData(elementTypeId);
        if (idx >= 0) m_typeCombo->setCurrentIndex(idx);
    }
}
