#include "TerritorioWindow.h"
#include "AvatarUtils.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "ImageOverlay.h"
#include "ProjectModel.h"
#include "Theme.h"
#include "WorldContentEditor.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBuffer>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFocusEvent>
#include <QFontComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidgetAction>

#include <tuple>

#include <functional>

namespace {

constexpr auto kPrefs = "worldContentEditor/"; // mesmas preferências do editor antigo
constexpr int kSaveDelay = 600;
constexpr int kRoleKind = Qt::UserRole;      // 0 = item de topo, 1 = nó
constexpr int kRoleId = Qt::UserRole + 1;    // id do item de topo ou do nó
constexpr int kRoleOwner = Qt::UserRole + 2; // id do dono (território/sistema) do nó

QColor tc(const QString& css) { return Theme::toColor(css); }
QColor withAlpha(QColor c, qreal a) { c.setAlphaF(a); return c; }

QFont serif(qreal px, int weight = QFont::Normal, bool italic = false)
{
    QFont f(QStringLiteral("Lora"));
    f.setPixelSize(qRound(px));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    return f;
}

QFont sans(qreal px, int weight = QFont::Normal)
{
    QFont f = QApplication::font();
    f.setPixelSize(qRound(px));
    f.setWeight(QFont::Weight(weight));
    return f;
}

QLabel* kick(const QString& text, QWidget* parent, const QColor& color = QColor())
{
    auto* l = new QLabel(text.toUpper(), parent);
    l->setObjectName(QStringLiteral("encKick"));
    QFont f = sans(10, QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
    l->setFont(f);
    if (color.isValid()) l->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
    return l;
}

QLabel* textLabel(const QString& text, const char* objectName, const QFont& font, QWidget* parent, bool wrap = true)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(QLatin1String(objectName));
    l->setFont(font);
    l->setWordWrap(wrap);
    return l;
}

// Cor de cada categoria do Construtor: lista, título do verbete e espectro.
QColor categoryColor(const QString& id)
{
    static const QHash<QString, QString> map = {
        { QStringLiteral("magic"), QStringLiteral("#9b6fc4") },
        { QStringLiteral("politics"), QStringLiteral("#c28f35") },
        { QStringLiteral("religion"), QStringLiteral("#b0694f") },
        { QStringLiteral("social"), QStringLiteral("#4f9aa0") },
        { QStringLiteral("economy"), QStringLiteral("#5f9357") },
        { QStringLiteral("military"), QStringLiteral("#5d7fae") },
        { QStringLiteral("technology"), QStringLiteral("#7d8794") },
        { QStringLiteral("cosmology"), QStringLiteral("#6a6fc2") },
        { QStringLiteral("faction"), QStringLiteral("#a05f9a") },
        { QStringLiteral("lineage"), QStringLiteral("#b35f7a") },
        { QStringLiteral("mythology"), QStringLiteral("#b89b4a") },
        { QStringLiteral("other"), QStringLiteral("#8a877f") },
    };
    return QColor(map.value(id, QStringLiteral("#8a877f")));
}

QPixmap categoryDot(const QString& id, int size, qreal dpr)
{
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(categoryColor(id));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 5, 5);
    return pm;
}

QString plainPreview(const QString& html, int maxLen)
{
    QTextDocument doc;
    if (html.startsWith(QLatin1String("<!DOCTYPE"))) doc.setHtml(html);
    else doc.setPlainText(html);
    QString text = doc.toPlainText().simplified();
    if (text.size() > maxLen) text = text.left(maxLen).trimmed() + QStringLiteral("…");
    return text;
}

// Numeração do verbete de sistema: regras viram artigos (1, 1.1, 2…), só as
// regras contam; seção é "§". Devolve id do nó → rótulo.
void numberNodes(const QList<ConstrutorStore::Node>& nodes, const QString& prefix,
                 QHash<QString, QString>& out)
{
    int n = 0;
    for (const auto& node : nodes) {
        if (node.type == ConstrutorStore::NodeType::Rule) {
            ++n;
            const QString num = prefix.isEmpty() ? QString::number(n) : prefix + QLatin1Char('.') + QString::number(n);
            out.insert(node.id, num);
            numberNodes(node.children, num, out);
        } else {
            out.insert(node.id, QStringLiteral("§"));
            numberNodes(node.children, prefix, out);
        }
    }
}

// Clique simples num QFrame (cards de menção, linhas da ficha).
class ClickFrame : public QFrame {
public:
    ClickFrame(const char* objectName, QWidget* parent) : QFrame(parent)
    {
        setObjectName(QLatin1String(objectName));
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }
    std::function<void()> onClick;
protected:
    void mousePressEvent(QMouseEvent* e) override { e->accept(); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && onClick) onClick();
    }
};

// Espectro do sistema: um segmento por ponto; clicar escolhe o ponto.
class SpectrumBar : public QWidget {
public:
    SpectrumBar(int count, int index, const QColor& color, const QStringList& labels, QWidget* parent)
        : QWidget(parent), m_count(qMax(1, count)), m_index(index), m_color(color), m_labels(labels)
    {
        setFixedHeight(18);
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
    }
    std::function<void(int)> onPick;
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const qreal gap = 4, h = 7;
        const qreal w = (width() - gap * (m_count - 1)) / m_count;
        for (int i = 0; i < m_count; ++i) {
            const QRectF r(i * (w + gap), (height() - h) / 2, w, h);
            QColor c = i <= m_index ? m_color : tc(Theme::panelBorder());
            if (i == m_hover && i != m_index) c = c.lighter(130);
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRoundedRect(r, 3, 3);
            if (i == m_index) {
                p.setBrush(tc(Theme::textBright()));
                p.drawEllipse(r.center() + QPointF(w / 2 - 4, 0), 2.2, 2.2);
            }
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        const int i = indexAt(e->position().x());
        if (i != m_hover) {
            m_hover = i;
            setToolTip(m_labels.value(i));
            update();
        }
    }
    void leaveEvent(QEvent*) override { m_hover = -1; update(); }
    void mousePressEvent(QMouseEvent* e) override { e->accept(); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        const int i = indexAt(e->position().x());
        if (e->button() == Qt::LeftButton && i >= 0 && onPick) onPick(i);
    }
private:
    int indexAt(qreal x) const { return qBound(0, int(x / (width() / qreal(m_count))), m_count - 1); }
    int m_count, m_index, m_hover = -1;
    QColor m_color;
    QStringList m_labels;
};

} // namespace

// ── EncSection: um trecho do verbete ─────────────────────────────────────────
// QTextEdit sem moldura que cresce com o texto (a rolagem é do verbete
// inteiro), salva sozinho 600ms depois de parar de digitar e aceita imagem
// com o overlay de tamanho de sempre.
class EncSection : public QTextEdit {
public:
    enum class Owner { Territorio, TerritorioNode, Link, Sistema, SistemaNode };

    EncSection(Owner owner, const QString& ownerId, const QString& nodeId, QWidget* parent)
        : QTextEdit(parent), m_owner(owner), m_ownerId(ownerId), m_nodeId(nodeId)
    {
        setObjectName(QStringLiteral("encSection"));
        setFrameShape(QFrame::NoFrame);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAcceptRichText(true);
        document()->setDocumentMargin(10);
        document()->setDefaultStyleSheet(QStringLiteral("p { margin: 0 0 10px 0; }"));
        { QFont f(QStringLiteral("Lora")); f.setPointSize(12); document()->setDefaultFont(f); }
        WorldContentEditor::installImageHandler(document(), this);

        m_overlay = new ImageOverlay(viewport(), /*showAlignment=*/false);
        m_overlay->hide();
        QObject::connect(m_overlay, &ImageOverlay::widthChangeRequested, this, [this](int delta) {
            if (m_imageCursor.isNull() || !m_imageCursor.charFormat().isImageFormat()) return;
            QTextImageFormat fmt = m_imageCursor.charFormat().toImageFormat();
            const int w = qBound(60, int(fmt.width() > 0 ? fmt.width() : 320) + delta, 1200);
            fmt.setWidth(w);
            m_imageCursor.setCharFormat(fmt);
            m_overlay->setCurrentWidth(w);
        });
        viewport()->installEventFilter(this);

        m_saveTimer = new QTimer(this);
        m_saveTimer->setSingleShot(true);
        m_saveTimer->setInterval(kSaveDelay);
        QObject::connect(m_saveTimer, &QTimer::timeout, this, [this]() { if (onSave) onSave(this); });
        QObject::connect(this, &QTextEdit::textChanged, this, [this]() {
            if (!m_loading) m_saveTimer->start();
        });
        QObject::connect(document(), &QTextDocument::contentsChanged, this, [this]() { fitHeight(); });
        QObject::connect(this, &QTextEdit::cursorPositionChanged, this, [this]() {
            if (onCursorMoved && hasFocus()) onCursorMoved(this);
        });
    }

    Owner owner() const { return m_owner; }
    QString ownerId() const { return m_ownerId; }
    QString nodeId() const { return m_nodeId; }

    std::function<void(EncSection*)> onSave;
    std::function<void(EncSection*)> onFocused;
    std::function<void(EncSection*)> onCursorMoved;

    void setContentHtml(const QString& content)
    {
        m_loading = true;
        if (content.startsWith(QLatin1String("<!DOCTYPE"))) setHtml(content);
        else setPlainText(content);
        applyPrefs();
        applyTextColor(1.0);
        document()->setModified(false);
        m_loading = false;
        fitHeight();
    }
    QString html() const { return toHtml(); }

    // Recuo de 1ª linha, entrelinha e espaço entre parágrafos: preferências
    // do app pra todos os textos do Criador de Mundos (como no editor antigo).
    void applyPrefs()
    {
        QSettings s;
        const bool indent = s.value(QStringLiteral("%1firstLineIndent").arg(kPrefs), false).toBool();
        const int lh = s.value(QStringLiteral("%1lineHeightPercent").arg(kPrefs), 115).toInt();
        const int before = s.value(QStringLiteral("%1paraSpaceBefore").arg(kPrefs), 0).toInt();
        const int after = s.value(QStringLiteral("%1paraSpaceAfter").arg(kPrefs), 8).toInt();
        const bool was = m_loading;
        m_loading = true;
        QTextCursor c(document());
        c.select(QTextCursor::Document);
        QTextBlockFormat bf;
        bf.setTextIndent(indent ? 24.0 : 0.0);
        bf.setLineHeight(lh, QTextBlockFormat::ProportionalHeight);
        bf.setTopMargin(before);
        bf.setBottomMargin(after);
        c.mergeBlockFormat(bf);
        m_loading = was;
    }

    // Cor do texto pelo tema (e esmaecida no modo foco). Não conta como edição.
    void applyTextColor(qreal alpha)
    {
        QColor c = tc(Theme::editorTextColor());
        c.setAlphaF(alpha);
        QPalette pal = palette();
        pal.setColor(QPalette::Text, c);
        pal.setColor(QPalette::PlaceholderText, withAlpha(tc(Theme::textMuted()), 0.85));
        setPalette(pal);
        const bool was = m_loading;
        m_loading = true;
        blockSignals(true);
        const bool mod = document()->isModified();
        QTextCursor cur(document());
        cur.select(QTextCursor::Document);
        QTextCharFormat fmt;
        fmt.setForeground(c);
        cur.mergeCharFormat(fmt);
        document()->setModified(mod);
        blockSignals(false);
        m_loading = was;
    }

    void flush()
    {
        if (m_saveTimer->isActive()) {
            m_saveTimer->stop();
            if (onSave) onSave(this);
        }
    }

    void insertImageFromFile(QWidget* dialogParent)
    {
        const QString path = QFileDialog::getOpenFileName(
            dialogParent, TerritorioWindow::tr("Inserir imagem"), QString(),
            TerritorioWindow::tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"));
        if (path.isEmpty()) return;
        QImage img(path);
        if (img.isNull()) return;
        if (img.width() > 1200) img = img.scaledToWidth(1200, Qt::SmoothTransformation);
        QByteArray data;
        QBuffer buf(&data);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        QTextImageFormat fmt;
        fmt.setName(QStringLiteral("data:image/png;base64,") + QString::fromLatin1(data.toBase64()));
        fmt.setWidth(qMin(img.width(), 560));
        textCursor().insertImage(fmt);
        setFocus();
    }

    QSize sizeHint() const override { return QSize(QTextEdit::sizeHint().width(), m_height); }
    QSize minimumSizeHint() const override { return QSize(80, m_height); }

protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QTextEdit::resizeEvent(e);
        fitHeight();
    }
    void focusInEvent(QFocusEvent* e) override
    {
        QTextEdit::focusInEvent(e);
        if (onFocused) onFocused(this);
    }
    void focusOutEvent(QFocusEvent* e) override
    {
        QTextEdit::focusOutEvent(e);
        flush();
    }
    // Sem rolagem própria: a roda rola o verbete.
    void wheelEvent(QWheelEvent* e) override { e->ignore(); }
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == viewport() && event->type() == QEvent::MouseButtonPress) {
            const QPoint pos = static_cast<QMouseEvent*>(event)->pos();
            if (showOverlayAt(pos)) return true;
            m_overlay->hide();
        }
        return QTextEdit::eventFilter(watched, event);
    }

private:
    void fitHeight()
    {
        document()->setTextWidth(viewport()->width());
        const int h = qMax(40, int(document()->size().height()) + 4);
        if (h != m_height) {
            m_height = h;
            setFixedHeight(h);
            updateGeometry();
        }
    }
    bool showOverlayAt(const QPoint& pos)
    {
        auto* layout = document()->documentLayout();
        for (QTextBlock b = document()->begin(); b.isValid(); b = b.next()) {
            for (auto it = b.begin(); !it.atEnd(); ++it) {
                const QTextFragment frag = it.fragment();
                if (!frag.isValid() || !frag.charFormat().isImageFormat()) continue;
                const QRect r = layout->blockBoundingRect(b).toRect();
                if (!r.adjusted(-4, -4, 4, 4).contains(pos)) continue;
                m_imageCursor = QTextCursor(document());
                m_imageCursor.setPosition(frag.position());
                m_imageCursor.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
                const QTextImageFormat fmt = m_imageCursor.charFormat().toImageFormat();
                m_overlay->setCurrentWidth(int(fmt.width() > 0 ? fmt.width() : 320));
                m_overlay->adjustSize();
                const int x = qBound(4, r.center().x() - m_overlay->width() / 2, qMax(4, viewport()->width() - m_overlay->width() - 4));
                m_overlay->move(x, qMax(4, r.top() + 4));
                m_overlay->show();
                m_overlay->raise();
                return true;
            }
        }
        return false;
    }

    Owner m_owner;
    QString m_ownerId, m_nodeId;
    QTimer* m_saveTimer = nullptr;
    ImageOverlay* m_overlay = nullptr;
    QTextCursor m_imageCursor;
    bool m_loading = false;
    int m_height = 40;
};

// ── EncFormatBar: a barra de formatação do verbete ───────────────────────────
// Age no trecho em foco. Negrito/itálico/sublinhado/tachado valem pra seleção;
// fonte, tamanho e alinhamento valem pro trecho inteiro; recuo, entrelinha e
// espaço entre parágrafos são preferência e valem pra todos os trechos.
class EncFormatBar : public QWidget {
public:
    explicit EncFormatBar(QWidget* parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("encFormatBar"));
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(12, 4, 12, 4);
        lay->setSpacing(2);
        auto btn = [&](const QString& text, const QString& tip, bool checkable) {
            auto* b = new QPushButton(text, this);
            b->setObjectName(QStringLiteral("encFmtBtn"));
            b->setCursor(Qt::PointingHandCursor);
            b->setCheckable(checkable);
            b->setFixedSize(28, 26);
            b->setToolTip(tip);
            b->setFocusPolicy(Qt::NoFocus);
            lay->addWidget(b);
            return b;
        };
        auto sep = [&]() {
            auto* s = new QFrame(this);
            s->setObjectName(QStringLiteral("encFmtSep"));
            s->setFixedSize(1, 18);
            lay->addSpacing(4);
            lay->addWidget(s);
            lay->addSpacing(4);
        };
        m_bold = btn(QStringLiteral("B"), TerritorioWindow::tr("Negrito (Ctrl+B)"), true);
        { QFont f = m_bold->font(); f.setBold(true); m_bold->setFont(f); }
        m_italic = btn(QStringLiteral("I"), TerritorioWindow::tr("Itálico (Ctrl+I)"), true);
        { QFont f = m_italic->font(); f.setItalic(true); m_italic->setFont(f); }
        m_underline = btn(QStringLiteral("U"), TerritorioWindow::tr("Sublinhado (Ctrl+U)"), true);
        { QFont f = m_underline->font(); f.setUnderline(true); m_underline->setFont(f); }
        m_strike = btn(QStringLiteral("S"), TerritorioWindow::tr("Tachado"), true);
        { QFont f = m_strike->font(); f.setStrikeOut(true); m_strike->setFont(f); }
        m_indent = btn(QStringLiteral("¶"), TerritorioWindow::tr("Indentar primeira linha do parágrafo"), true);
        sep();
        m_font = new QFontComboBox(this);
        m_font->setObjectName(QStringLiteral("encFontCombo"));
        m_font->setMaximumWidth(170);
        m_font->setFocusPolicy(Qt::ClickFocus);
        lay->addWidget(m_font);
        m_size = new QComboBox(this);
        m_size->setObjectName(QStringLiteral("encFontCombo"));
        m_size->setEditable(true);
        m_size->setFixedWidth(64);
        for (int s : { 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 22, 24, 28, 32, 36, 48, 72 }) m_size->addItem(QString::number(s));
        lay->addWidget(m_size);
        m_spacing = btn(QString(), TerritorioWindow::tr("Espaçamento de linhas e parágrafos"), false);
        buildSpacingMenu();
        sep();
        m_alignLeft = btn(QString(), TerritorioWindow::tr("Alinhar à esquerda"), true);
        m_alignCenter = btn(QString(), TerritorioWindow::tr("Centralizar"), true);
        m_alignRight = btn(QString(), TerritorioWindow::tr("Alinhar à direita"), true);
        auto* group = new QButtonGroup(this);
        group->addButton(m_alignLeft);
        group->addButton(m_alignCenter);
        group->addButton(m_alignRight);
        sep();
        m_focus = btn(QString(), TerritorioWindow::tr("Modo foco"), true);
        m_image = btn(QString(), TerritorioWindow::tr("Inserir imagem"), false);
        lay->addStretch(1);
        m_status = new QLabel(this);
        m_status->setObjectName(QStringLiteral("encFmtStatus"));
        lay->addWidget(m_status);

        m_indent->setChecked(QSettings().value(QStringLiteral("%1firstLineIndent").arg(kPrefs), false).toBool());
        m_focus->setChecked(QSettings().value(QStringLiteral("%1focusMode").arg(kPrefs), false).toBool());

        auto charFmt = [this](const std::function<void(QTextCharFormat&, bool)>& set, QPushButton* b) {
            QObject::connect(b, &QPushButton::toggled, this, [this, set](bool on) {
                if (m_syncing || !m_target) return;
                QTextCharFormat f;
                set(f, on);
                m_target->mergeCurrentCharFormat(f);
                m_target->setFocus();
            });
        };
        charFmt([](QTextCharFormat& f, bool on) { f.setFontWeight(on ? QFont::Bold : QFont::Normal); }, m_bold);
        charFmt([](QTextCharFormat& f, bool on) { f.setFontItalic(on); }, m_italic);
        charFmt([](QTextCharFormat& f, bool on) { f.setFontUnderline(on); }, m_underline);
        charFmt([](QTextCharFormat& f, bool on) { f.setFontStrikeOut(on); }, m_strike);
        m_bold->setShortcut(QKeySequence::Bold);
        m_italic->setShortcut(QKeySequence::Italic);
        m_underline->setShortcut(QKeySequence::Underline);

        QObject::connect(m_indent, &QPushButton::toggled, this, [this](bool on) {
            if (m_syncing) return;
            QSettings().setValue(QStringLiteral("%1firstLineIndent").arg(kPrefs), on);
            if (onPrefsChanged) onPrefsChanged();
        });
        QObject::connect(m_font, &QFontComboBox::currentFontChanged, this, [this](const QFont& f) {
            if (m_syncing || !m_target) return;
            QTextCursor c(m_target->document());
            c.select(QTextCursor::Document);
            QTextCharFormat fmt;
            fmt.setFontFamilies({ f.family() });
            c.mergeCharFormat(fmt);
            m_target->mergeCurrentCharFormat(fmt);
            QFont d = m_target->document()->defaultFont();
            d.setFamily(f.family());
            m_target->document()->setDefaultFont(d);
            m_target->setFocus();
        });
        QObject::connect(m_size, &QComboBox::currentTextChanged, this, [this](const QString& t) {
            if (m_syncing || !m_target) return;
            bool ok = false;
            const qreal sz = t.toDouble(&ok);
            if (!ok || sz <= 0) return;
            QTextCursor c(m_target->document());
            c.select(QTextCursor::Document);
            QTextCharFormat fmt;
            fmt.setFontPointSize(sz);
            c.mergeCharFormat(fmt);
            m_target->mergeCurrentCharFormat(fmt);
        });
        auto align = [this](QPushButton* b, Qt::Alignment a) {
            QObject::connect(b, &QPushButton::clicked, this, [this, a]() {
                if (!m_target) return;
                QTextCursor c(m_target->document());
                c.select(QTextCursor::Document);
                QTextBlockFormat bf;
                bf.setAlignment(a);
                c.mergeBlockFormat(bf);
                m_target->setFocus();
            });
        };
        align(m_alignLeft, Qt::AlignLeft);
        align(m_alignCenter, Qt::AlignHCenter);
        align(m_alignRight, Qt::AlignRight);
        QObject::connect(m_focus, &QPushButton::toggled, this, [this](bool on) {
            QSettings().setValue(QStringLiteral("%1focusMode").arg(kPrefs), on);
            if (onFocusModeChanged) onFocusModeChanged(on);
        });
        QObject::connect(m_image, &QPushButton::clicked, this, [this]() {
            if (m_target) m_target->insertImageFromFile(window());
        });
        setTarget(nullptr);
    }

    std::function<void()> onPrefsChanged;
    std::function<void(bool)> onFocusModeChanged;

    bool focusMode() const { return m_focus->isChecked(); }
    void setStatus(const QString& text) { m_status->setText(text); }

    void setTarget(EncSection* s)
    {
        if (m_target) QObject::disconnect(m_target, nullptr, this, nullptr);
        m_target = s;
        for (QWidget* w : { static_cast<QWidget*>(m_bold), static_cast<QWidget*>(m_italic),
                            static_cast<QWidget*>(m_underline), static_cast<QWidget*>(m_strike),
                            static_cast<QWidget*>(m_font), static_cast<QWidget*>(m_size),
                            static_cast<QWidget*>(m_alignLeft), static_cast<QWidget*>(m_alignCenter),
                            static_cast<QWidget*>(m_alignRight), static_cast<QWidget*>(m_image) })
            w->setEnabled(s != nullptr);
        if (!s) {
            m_syncing = true;
            m_size->setCurrentText(QString());
            m_syncing = false;
            return;
        }
        QObject::connect(s, &QTextEdit::currentCharFormatChanged, this, [this](const QTextCharFormat&) { sync(); });
        QObject::connect(s, &QTextEdit::cursorPositionChanged, this, [this]() { sync(); });
        sync();
    }
    EncSection* target() const { return m_target; }

    void applyIcons()
    {
        const QColor m = tc(Theme::textMuted()), p = tc(Theme::textPrimary()), b = tc(Theme::textBright());
        auto icon = [&](const QString& n) { return IconUtils::loadToolbarIcon(QStringLiteral(":/icons/") + n, m, p, b, QSize(16, 16)); };
        m_alignLeft->setIcon(icon(QStringLiteral("align-left.svg")));
        m_alignCenter->setIcon(icon(QStringLiteral("align-center.svg")));
        m_alignRight->setIcon(icon(QStringLiteral("align-right.svg")));
        m_spacing->setIcon(icon(QStringLiteral("text-spacing.svg")));
        m_image->setIcon(icon(QStringLiteral("add-image.svg")));
        m_focus->setIcon(icon(m_focus->isChecked() ? QStringLiteral("focusmode-on.svg") : QStringLiteral("focusmode-off.svg")));
        for (auto* b2 : { m_alignLeft, m_alignCenter, m_alignRight, m_spacing, m_image, m_focus }) b2->setIconSize(QSize(16, 16));
    }

private:
    void sync()
    {
        if (!m_target) return;
        m_syncing = true;
        const QTextCharFormat f = m_target->currentCharFormat();
        m_bold->setChecked(f.fontWeight() >= QFont::Bold);
        m_italic->setChecked(f.fontItalic());
        m_underline->setChecked(f.fontUnderline());
        m_strike->setChecked(f.fontStrikeOut());
        const QStringList fams = f.fontFamilies().toStringList();
        m_font->setCurrentFont(QFont(fams.isEmpty() ? m_target->document()->defaultFont().family() : fams.first()));
        const qreal pt = f.fontPointSize() > 0 ? f.fontPointSize() : m_target->document()->defaultFont().pointSizeF();
        if (pt > 0) m_size->setCurrentText(QString::number(qRound(pt)));
        const Qt::Alignment a = m_target->alignment();
        m_alignLeft->setChecked(!(a & (Qt::AlignHCenter | Qt::AlignRight)));
        m_alignCenter->setChecked(a & Qt::AlignHCenter);
        m_alignRight->setChecked(a & Qt::AlignRight);
        m_syncing = false;
    }

    void buildSpacingMenu()
    {
        auto* menu = new QMenu(m_spacing);
        menu->setObjectName(QStringLiteral("encSpacingMenu"));
        menu->addAction(TerritorioWindow::tr("ENTRE LINHAS"))->setEnabled(false);
        const QList<QPair<int, QString>> presets = {
            { 100, TerritorioWindow::tr("Simples (1.0)") }, { 115, TerritorioWindow::tr("Justo (1.15)") },
            { 130, TerritorioWindow::tr("Compacto (1.3)") }, { 150, TerritorioWindow::tr("Confortável (1.5)") },
            { 170, TerritorioWindow::tr("Padrão (1.7)") }, { 190, TerritorioWindow::tr("Amplo (1.9)") },
            { 220, TerritorioWindow::tr("Espaçoso (2.2)") } };
        const int cur = QSettings().value(QStringLiteral("%1lineHeightPercent").arg(kPrefs), 115).toInt();
        auto* group = new QActionGroup(menu);
        for (const auto& pr : presets) {
            QAction* a = menu->addAction(pr.second);
            a->setCheckable(true);
            a->setChecked(pr.first == cur);
            group->addAction(a);
            const int v = pr.first;
            QObject::connect(a, &QAction::triggered, this, [this, v]() {
                QSettings().setValue(QStringLiteral("%1lineHeightPercent").arg(kPrefs), v);
                if (onPrefsChanged) onPrefsChanged();
            });
        }
        auto stepper = [&](const QString& title, const QString& key, int def) {
            menu->addSeparator();
            menu->addAction(title)->setEnabled(false);
            auto* row = new QWidget(menu);
            auto* l = new QHBoxLayout(row);
            l->setContentsMargins(10, 2, 10, 6);
            auto* minus = new QPushButton(QStringLiteral("−"), row);
            auto* plus = new QPushButton(QStringLiteral("+"), row);
            minus->setFixedSize(26, 26);
            plus->setFixedSize(26, 26);
            auto* val = new QLabel(QStringLiteral("%1 px").arg(QSettings().value(key, def).toInt()), row);
            val->setAlignment(Qt::AlignCenter);
            val->setFixedWidth(50);
            l->addWidget(minus);
            l->addWidget(val);
            l->addWidget(plus);
            auto step = [this, key, def, val](int d) {
                const int v = qBound(0, QSettings().value(key, def).toInt() + d, 64);
                QSettings().setValue(key, v);
                val->setText(QStringLiteral("%1 px").arg(v));
                if (onPrefsChanged) onPrefsChanged();
            };
            QObject::connect(minus, &QPushButton::clicked, this, [step]() { step(-2); });
            QObject::connect(plus, &QPushButton::clicked, this, [step]() { step(2); });
            auto* act = new QWidgetAction(menu);
            act->setDefaultWidget(row);
            menu->addAction(act);
        };
        stepper(TerritorioWindow::tr("ANTES DO PARÁGRAFO"), QStringLiteral("%1paraSpaceBefore").arg(kPrefs), 0);
        stepper(TerritorioWindow::tr("DEPOIS DO PARÁGRAFO"), QStringLiteral("%1paraSpaceAfter").arg(kPrefs), 8);
        m_spacing->setMenu(menu);
    }

    QPointer<EncSection> m_target;
    QPushButton *m_bold, *m_italic, *m_underline, *m_strike, *m_indent, *m_spacing;
    QPushButton *m_alignLeft, *m_alignCenter, *m_alignRight, *m_focus, *m_image;
    QFontComboBox* m_font;
    QComboBox* m_size;
    QLabel* m_status;
    bool m_syncing = false;
};

// ═════════════════════════════════════════════════════════════════════════════

TerritorioWindow::TerritorioWindow(TerritorioStore* store, QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("encWindow"));
    setWindowTitle(tr("Criador de Mundos"));
    setMinimumSize(980, 600);
    resize(1320, 820);
    const QString saved = QSettings().value(QStringLiteral("worldEncyclopedia/mode"), QStringLiteral("sistemas")).toString();
    m_mode = saved == QStringLiteral("lugares") ? Mode::Lugares : Mode::Sistemas;
    buildUi();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, &TerritorioWindow::applyTheme);
    setStore(store);
}

// ── Stores ───────────────────────────────────────────────────────────────────

void TerritorioWindow::setStore(TerritorioStore* store)
{
    if (m_store != store) {
        if (m_store) disconnect(m_store, nullptr, this, nullptr);
        m_store = store;
        if (m_store) connect(m_store, &TerritorioStore::changed, this, &TerritorioWindow::onTerritorioStoreChanged);
    }
    // O mesmo ponteiro é reaproveitado entre projetos: sempre refaz.
    if (m_kind == Kind::Territorio || m_kind == Kind::Link) { m_kind = Kind::None; m_currentId.clear(); }
    m_lastTerritorioId.clear();
    if (isVisible()) setMode(m_mode);
    else rebuildSidebar();
}

void TerritorioWindow::setConstrutorStore(ConstrutorStore* store)
{
    if (m_construtorStore != store) {
        if (m_construtorStore) disconnect(m_construtorStore, nullptr, this, nullptr);
        m_construtorStore = store;
        if (m_construtorStore) connect(m_construtorStore, &ConstrutorStore::changed, this, &TerritorioWindow::onConstrutorStoreChanged);
    }
    if (m_kind == Kind::Sistema) { m_kind = Kind::None; m_currentId.clear(); }
    m_lastSistemaId.clear();
    if (isVisible()) setMode(m_mode);
    else rebuildSidebar();
}

void TerritorioWindow::onTerritorioStoreChanged()
{
    if (m_selfWriting) return;
    QTimer::singleShot(0, this, [this]() {
        if (m_kind == Kind::Territorio && m_store && !m_store->territorio(m_currentId)) { m_kind = Kind::None; m_currentId.clear(); }
        if (m_kind == Kind::Link && m_store && !m_store->link(m_currentId)) { m_kind = Kind::None; m_currentId.clear(); }
        rebuildSidebar();
        if (m_kind == Kind::None) { if (m_mode == Mode::Lugares) showEmptyPage(); }
        else if (pageSignature() != m_pageSig) rebuildPage();
        rebuildMargin();
    });
}

void TerritorioWindow::onConstrutorStoreChanged()
{
    if (m_selfWriting) return;
    QTimer::singleShot(0, this, [this]() {
        if (m_kind == Kind::Sistema && m_construtorStore && !m_construtorStore->system(m_currentId)) { m_kind = Kind::None; m_currentId.clear(); }
        rebuildSidebar();
        if (m_kind == Kind::None) { if (m_mode == Mode::Sistemas) showEmptyPage(); }
        else if (pageSignature() != m_pageSig) rebuildPage();
        rebuildMargin();
    });
}

// ── Modo ─────────────────────────────────────────────────────────────────────

void TerritorioWindow::setMode(Mode mode)
{
    if (m_lugaresBtn) m_lugaresBtn->setChecked(mode == Mode::Lugares);
    if (m_sistemasBtn) m_sistemasBtn->setChecked(mode == Mode::Sistemas);
    if (mode == m_mode && m_kind != Kind::None) return;
    flushAll();
    m_mode = mode;
    QSettings().setValue(QStringLiteral("worldEncyclopedia/mode"),
                         mode == Mode::Lugares ? QStringLiteral("lugares") : QStringLiteral("sistemas"));
    rebuildSidebar();
    if (mode == Mode::Lugares) {
        if (m_store && m_store->territorio(m_lastTerritorioId)) openTerritorio(m_lastTerritorioId);
        else if (m_store && !m_store->territorios().isEmpty()) openTerritorio(m_store->territorios().first().id);
        else showEmptyPage();
    } else {
        if (m_construtorStore && m_construtorStore->system(m_lastSistemaId)) openSistema(m_lastSistemaId);
        else if (m_construtorStore && !m_construtorStore->systems().isEmpty()) openSistema(m_construtorStore->systems().first().id);
        else showEmptyPage();
    }
}

void TerritorioWindow::openConstrutorNode(const QString& systemId, const QString& nodeId)
{
    if (m_mode != Mode::Sistemas) {
        m_lastSistemaId = systemId;
        setMode(Mode::Sistemas);
    }
    openSistema(systemId, nodeId);
}

void TerritorioWindow::openNode(const QString& territorioId, const QString& nodeId)
{
    if (m_mode != Mode::Lugares) {
        m_lastTerritorioId = territorioId;
        setMode(Mode::Lugares);
    }
    openTerritorio(territorioId, nodeId);
}

// ── UI ───────────────────────────────────────────────────────────────────────

void TerritorioWindow::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Topo: título, chave Lugares | Sistemas, busca.
    auto* top = new QWidget(this);
    top->setObjectName(QStringLiteral("encTop"));
    auto* tl = new QHBoxLayout(top);
    tl->setContentsMargins(14, 8, 14, 8);
    tl->setSpacing(10);
    auto* title = textLabel(tr("Criador de Mundos"), "encTopTitle", sans(13, QFont::DemiBold), top, false);
    tl->addWidget(title);
    auto* seg = new QFrame(top);
    seg->setObjectName(QStringLiteral("encSeg"));
    auto* sl = new QHBoxLayout(seg);
    sl->setContentsMargins(2, 2, 2, 2);
    sl->setSpacing(2);
    m_lugaresBtn = new QPushButton(tr("Lugares"), seg);
    m_sistemasBtn = new QPushButton(tr("Sistemas"), seg);
    for (auto* b : { m_lugaresBtn, m_sistemasBtn }) {
        b->setObjectName(QStringLiteral("encSegBtn"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        sl->addWidget(b);
    }
    m_lugaresBtn->setChecked(m_mode == Mode::Lugares);
    m_sistemasBtn->setChecked(m_mode == Mode::Sistemas);
    connect(m_lugaresBtn, &QPushButton::clicked, this, [this]() { setMode(Mode::Lugares); });
    connect(m_sistemasBtn, &QPushButton::clicked, this, [this]() { setMode(Mode::Sistemas); });
    tl->addWidget(seg);
    tl->addStretch(1);
    m_searchEdit = new QLineEdit(top);
    m_searchEdit->setObjectName(QStringLiteral("encSearch"));
    m_searchEdit->setPlaceholderText(tr("Buscar lugar, sistema, regra ou documento"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(300);
    tl->addWidget(m_searchEdit);
    outer->addWidget(top);

    m_formatBar = new EncFormatBar(this);
    m_formatBar->onPrefsChanged = [this]() {
        for (EncSection* s : std::as_const(m_sections)) s->applyPrefs();
    };
    m_formatBar->onFocusModeChanged = [this](bool on) {
        for (EncSection* s : std::as_const(m_sections)) {
            s->applyTextColor(on && s != m_focusedSection ? 0.38 : 1.0);
        }
        m_formatBar->applyIcons();
    };
    outer->addWidget(m_formatBar);

    auto* body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    outer->addLayout(body, 1);

    // Esquerda: sumário.
    auto* left = new QWidget(this);
    left->setObjectName(QStringLiteral("encSide"));
    left->setFixedWidth(260);
    auto* ll = new QVBoxLayout(left);
    ll->setContentsMargins(8, 10, 8, 10);
    ll->setSpacing(8);
    m_sidebar = new QTreeWidget(left);
    m_sidebar->setObjectName(QStringLiteral("encTree"));
    m_sidebar->setHeaderHidden(true);
    m_sidebar->setFrameShape(QFrame::NoFrame);
    m_sidebar->setIndentation(14);
    m_sidebar->setIconSize(QSize(22, 22));
    m_sidebar->setContextMenuPolicy(Qt::CustomContextMenu);
    m_sidebar->setExpandsOnDoubleClick(false);
    m_sidebar->setFocusPolicy(Qt::NoFocus);
    ll->addWidget(m_sidebar, 1);
    m_newBtn = new QPushButton(left);
    m_newBtn->setObjectName(QStringLiteral("encNewBtn"));
    m_newBtn->setCursor(Qt::PointingHandCursor);
    ll->addWidget(m_newBtn);
    body->addWidget(left);
    connect(m_newBtn, &QPushButton::clicked, this, [this]() {
        if (m_mode == Mode::Lugares) newTerritorio();
        else newSistema();
    });
    connect(m_sidebar, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        if (!item) return;
        const int kind = item->data(0, kRoleKind).toInt();
        const QString id = item->data(0, kRoleId).toString();
        const QString owner = item->data(0, kRoleOwner).toString();
        // Adiado: abrir refaz o sumário, e o item clicado não pode sumir
        // no meio do próprio clique.
        QTimer::singleShot(0, this, [this, kind, id, owner]() {
        if (m_mode == Mode::Lugares) {
            if (kind == 0) openTerritorio(id);
            else if (m_kind == Kind::Territorio && m_currentId == owner) scrollToSection(id);
            else openTerritorio(owner, id);
        } else {
            if (kind == 0) openSistema(id);
            else if (m_kind == Kind::Sistema && m_currentId == owner) scrollToSection(id);
            else openSistema(owner, id);
        }
        });
    });
    connect(m_sidebar, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QTreeWidgetItem* item = m_sidebar->itemAt(pos);
        if (!item) return;
        const QPoint g = m_sidebar->viewport()->mapToGlobal(pos);
        const QString id = item->data(0, kRoleId).toString();
        if (item->data(0, kRoleKind).toInt() == 1) {
            const QString owner = item->data(0, kRoleOwner).toString();
            if (m_currentId != owner) {
                if (m_mode == Mode::Lugares) openTerritorio(owner); else openSistema(owner);
            }
            nodeMenu(id, g);
        } else if (m_mode == Mode::Lugares) territorioMenu(id, g);
        else sistemaMenu(id, g);
    });

    // Meio: o verbete.
    m_pageScroll = new QScrollArea(this);
    m_pageScroll->setObjectName(QStringLiteral("encPageScroll"));
    m_pageScroll->setFrameShape(QFrame::NoFrame);
    m_pageScroll->setWidgetResizable(true);
    m_pageScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->addWidget(m_pageScroll, 1);

    // Direita: margem / ficha.
    m_marginScroll = new QScrollArea(this);
    m_marginScroll->setObjectName(QStringLiteral("encMarginScroll"));
    m_marginScroll->setFrameShape(QFrame::NoFrame);
    m_marginScroll->setWidgetResizable(true);
    m_marginScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_marginScroll->setFixedWidth(300);
    body->addWidget(m_marginScroll);

    // Busca: lista solta sob o campo.
    m_searchResults = new QListWidget(this);
    m_searchResults->setObjectName(QStringLiteral("encSearchResults"));
    m_searchResults->hide();
    connect(m_searchEdit, &QLineEdit::textChanged, this, &TerritorioWindow::runSearch);
    m_searchEdit->installEventFilter(this);
    connect(m_searchResults, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        const QString kind = it->data(Qt::UserRole).toString();
        const QString owner = it->data(Qt::UserRole + 1).toString();
        const QString node = it->data(Qt::UserRole + 2).toString();
        m_searchResults->hide();
        m_searchEdit->clear();
        if (kind == QStringLiteral("t")) openNode(owner, node);
        else openConstrutorNode(owner, node);
    });
}

bool TerritorioWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_searchEdit && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) { m_searchEdit->clear(); return true; }
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && m_searchResults->count() > 0) {
            emit m_searchResults->itemClicked(m_searchResults->item(0));
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TerritorioWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // Primeira abertura (ou nada aberto): cai no último verbete do modo.
    if (m_kind == Kind::None) setMode(m_mode);
}

void TerritorioWindow::closeEvent(QCloseEvent* event)
{
    flushAll();
    event->accept();
}

void TerritorioWindow::applyTheme()
{
    const QString css = Theme::qss(QStringLiteral(R"(
        #encWindow { background: %1; }
        #encTop { background: %2; border-bottom: 1px solid %3; }
        #encTopTitle { color: %5; }
        QFrame#encSeg { border: 1px solid %3; border-radius: @radius-control; background: transparent; }
        QPushButton#encSegBtn { background: transparent; color: %6; border: none; border-radius: @radius-item; padding: 4px 16px; font-size: 12px; }
        QPushButton#encSegBtn:hover { color: %5; }
        QPushButton#encSegBtn:checked { background: %7; color: %5; font-weight: 600; }
        QLineEdit#encSearch { background: %1; color: %4; border: 1px solid %3; border-radius: @radius-control; padding: 5px 9px; font-size: 12px; }
        QLineEdit#encSearch:focus { border-color: %8; }
        QListWidget#encSearchResults { background: %2; color: %4; border: 1px solid %3; border-radius: @radius-panel; padding: 4px; font-size: 12px; outline: none; }
        QListWidget#encSearchResults::item { padding: 6px 8px; border-radius: @radius-item; }
        QListWidget#encSearchResults::item:hover, QListWidget#encSearchResults::item:selected { background: %7; color: %5; }

        #encFormatBar { background: %1; border-bottom: 1px solid %3; }
        QPushButton#encFmtBtn { background: transparent; color: %4; border: 1px solid transparent; border-radius: @radius-control; font-size: 13px; }
        QPushButton#encFmtBtn:hover { background: %7; color: %5; }
        QPushButton#encFmtBtn:checked { background: %7; color: %5; border-color: %3; }
        QPushButton#encFmtBtn:disabled { color: %6; }
        QPushButton#encFmtBtn::menu-indicator { image: none; width: 0; }
        QFrame#encFmtSep { background: %3; border: none; }
        QComboBox#encFontCombo, QFontComboBox#encFontCombo { background: %1; color: %4; border: 1px solid %3; border-radius: @radius-control; padding: 2px 5px; font-size: 12px; }
        QComboBox#encFontCombo::drop-down, QFontComboBox#encFontCombo::drop-down { border: none; width: 14px; }
        QComboBox#encFontCombo QAbstractItemView, QFontComboBox#encFontCombo QAbstractItemView { background: %2; color: %4; border: 1px solid %3; selection-background-color: %7; selection-color: %5; }
        QLabel#encFmtStatus { color: %6; font-size: 10.5px; }

        #encSide { background: %2; border-right: 1px solid %3; }
        QTreeWidget#encTree { background: transparent; color: %4; border: none; outline: none; font-size: 12.5px; }
        QTreeWidget#encTree::item { padding: 5px 4px; border-radius: @radius-item; }
        QTreeWidget#encTree::item:hover { background: %7; color: %5; }
        QTreeWidget#encTree::item:selected { background: %7; color: %5; }
        QPushButton#encNewBtn { background: transparent; color: %4; border: 1px solid %3; border-radius: @radius-control; padding: 7px; font-size: 12px; }
        QPushButton#encNewBtn:hover { border-color: %8; color: %5; }

        #encPageScroll, #encMarginScroll { background: transparent; border: none; }
        #encPage { background: %1; }
        #encMargin { background: %2; border-left: 1px solid %3; }
        QLabel#encKick { color: %6; }
        QLineEdit#encTitle { background: transparent; color: %5; border: 1px solid transparent; border-radius: @radius-item; padding: 0 6px; }
        QLineEdit#encTitle:hover { background: %7; }
        QLineEdit#encTitle:focus { border-color: %3; }
        QLineEdit#encHeading { background: transparent; color: %5; border: 1px solid transparent; border-radius: @radius-item; padding: 0 4px; }
        QLineEdit#encHeading:hover { background: %7; }
        QLineEdit#encHeading:focus { border-color: %3; }
        QLabel#encNum { color: %6; }
        QTextEdit#encSection { background: transparent; border: 1px solid transparent; border-radius: @radius-item; padding: 0; margin: 0; selection-background-color: %9; }
        QTextEdit#encSection:hover { background: %7; }
        QTextEdit#encSection:focus { background: %10; border-color: %11; }
        QToolButton#encTool { background: transparent; color: %6; border: 1px solid transparent; border-radius: @radius-item; padding: 2px 6px; font-size: 12px; }
        QToolButton#encTool:hover { color: %5; border-color: %3; }
        QToolButton#encTool::menu-indicator { image: none; width: 0; }
        QPushButton#encAddBtn { background: transparent; color: %6; border: 1px dashed %3; border-radius: @radius-control; padding: 7px 14px; font-size: 12px; }
        QPushButton#encAddBtn:hover { color: %5; border-color: %8; }
        QLabel#encEmpty { color: %6; }

        QLabel#encMarginText { color: %4; }
        QLabel#encMarginMuted { color: %6; }
        QLabel#encMarginBright { color: %5; }
        QFrame#encInfobox { background: %1; border: 1px solid %3; border-radius: @radius-panel; }
        QFrame#encInfoRow { border-top: 1px solid %3; }
        QFrame#encRowLink { border-radius: @radius-item; }
        QFrame#encRowLink:hover { background: %7; }
        QFrame#encMention { background: %1; border: 1px solid %3; border-radius: @radius-item; }
        QFrame#encMention:hover { border-color: %8; }
        QPushButton#encMarginBtn { background: transparent; color: %4; border: 1px solid %3; border-radius: @radius-control; padding: 5px 10px; font-size: 11.5px; }
        QPushButton#encMarginBtn:hover { color: %5; border-color: %8; }
        QPushButton#encMarginBtn::menu-indicator { image: none; width: 0; }
        QPushButton#encDangerBtn { background: transparent; color: %6; border: none; font-size: 11.5px; padding: 4px; text-align: left; }
        QPushButton#encDangerBtn:hover { color: %12; }
        QPushButton#encLinkBtn { background: transparent; color: %6; border: none; font-size: 11.5px; padding: 0 2px; }
        QPushButton#encLinkBtn:hover { color: %5; text-decoration: underline; }

        QScrollBar:vertical { background: transparent; width: 9px; margin: 0; }
        QScrollBar::handle:vertical { background: %3; border-radius: 4px; min-height: 28px; }
        QScrollBar::handle:vertical:hover { background: %6; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )"))
        .arg(Theme::editorBackground(), Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary())  // 1-4
        .arg(Theme::textBright(), Theme::textMuted(), Theme::hoverOverlay(), Theme::accentDefault())           // 5-8
        .arg(Theme::accentInfoSoft(), Theme::hoverOverlay(), Theme::subtleBorder(), Theme::accentDanger());   // 9-12
    setStyleSheet(css);
    if (m_formatBar) m_formatBar->applyIcons();
    for (EncSection* s : std::as_const(m_sections)) s->applyTextColor(1.0);
    rebuildSidebar();
    rebuildMargin();
}

// ── Sumário (esquerda) ───────────────────────────────────────────────────────

void TerritorioWindow::rebuildSidebar()
{
    if (!m_sidebar) return;
    m_rebuilding = true;
    m_sidebar->clear();
    const qreal dpr = devicePixelRatioF();
    const QIcon folderIcon = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/folder.svg"),
        tc(Theme::textMuted()), tc(Theme::textPrimary()), tc(Theme::textBright()), QSize(14, 14));

    if (m_mode == Mode::Lugares) {
        m_newBtn->setText(tr("+ Novo território"));
        if (m_store) {
            for (const auto& t : m_store->territorios()) {
                auto* it = new QTreeWidgetItem(m_sidebar);
                it->setText(0, t.name);
                it->setIcon(0, QIcon(AvatarUtils::circularAvatar(t.avatarDataUrl, t.name, t.id, 44)));
                it->setData(0, kRoleKind, 0);
                it->setData(0, kRoleId, t.id);
                it->setToolTip(0, plainPreview(t.content, 200));
                if (m_kind == Kind::Territorio && t.id == m_currentId) {
                    std::function<void(QTreeWidgetItem*, const QList<TerritorioStore::Node>&)> add =
                        [&](QTreeWidgetItem* parent, const QList<TerritorioStore::Node>& nodes) {
                            for (const auto& n : nodes) {
                                auto* c = new QTreeWidgetItem(parent);
                                c->setText(0, n.name);
                                if (n.type == TerritorioStore::NodeType::Folder) c->setIcon(0, folderIcon);
                                c->setData(0, kRoleKind, 1);
                                c->setData(0, kRoleId, n.id);
                                c->setData(0, kRoleOwner, t.id);
                                add(c, n.children);
                            }
                        };
                    add(it, t.nodes);
                    for (QTreeWidgetItemIterator x(it); *x; ++x) (*x)->setExpanded(true);
                    m_sidebar->setCurrentItem(it);
                }
            }
        }
    } else {
        m_newBtn->setText(tr("+ Novo sistema"));
        if (m_construtorStore) {
            for (const auto& s : m_construtorStore->systems()) {
                const auto* cat = ConstrutorStore::categoryById(s.categoryId);
                auto* it = new QTreeWidgetItem(m_sidebar);
                it->setText(0, s.name);
                it->setIcon(0, QIcon(categoryDot(s.categoryId, 18, dpr)));
                it->setData(0, kRoleKind, 0);
                it->setData(0, kRoleId, s.id);
                if (cat) {
                    const QString wp = cat->waypoints.value(s.sliderIndex).label;
                    it->setToolTip(0, wp.isEmpty() ? cat->displayName : cat->displayName + QStringLiteral(" · ") + wp);
                }
                if (m_kind == Kind::Sistema && s.id == m_currentId) {
                    QHash<QString, QString> nums;
                    numberNodes(s.nodes, QString(), nums);
                    std::function<void(QTreeWidgetItem*, const QList<ConstrutorStore::Node>&)> add =
                        [&](QTreeWidgetItem* parent, const QList<ConstrutorStore::Node>& nodes) {
                            for (const auto& n : nodes) {
                                auto* c = new QTreeWidgetItem(parent);
                                const QString num = nums.value(n.id);
                                c->setText(0, num + QStringLiteral("  ") + n.name);
                                c->setData(0, kRoleKind, 1);
                                c->setData(0, kRoleId, n.id);
                                c->setData(0, kRoleOwner, s.id);
                                add(c, n.children);
                            }
                        };
                    add(it, s.nodes);
                    for (QTreeWidgetItemIterator x(it); *x; ++x) (*x)->setExpanded(true);
                    m_sidebar->setCurrentItem(it);
                }
            }
        }
    }
    m_rebuilding = false;
}

// ── Abrir ────────────────────────────────────────────────────────────────────

void TerritorioWindow::openTerritorio(const QString& id, const QString& nodeId)
{
    if (!m_store || !m_store->territorio(id)) return;
    if (!(m_kind == Kind::Territorio && m_currentId == id)) {
        flushAll();
        m_kind = Kind::Territorio;
        m_currentId = id;
        m_lastTerritorioId = id;
        rebuildSidebar();
        rebuildPage();
        rebuildMargin();
    }
    if (!nodeId.isEmpty()) scrollToSection(nodeId);
}

void TerritorioWindow::openLink(const QString& linkId)
{
    if (!m_store || !m_store->link(linkId)) return;
    flushAll();
    m_kind = Kind::Link;
    m_currentId = linkId;
    rebuildSidebar();
    rebuildPage();
    rebuildMargin();
}

void TerritorioWindow::openSistema(const QString& id, const QString& nodeId)
{
    if (!m_construtorStore || !m_construtorStore->system(id)) return;
    if (!(m_kind == Kind::Sistema && m_currentId == id)) {
        flushAll();
        m_kind = Kind::Sistema;
        m_currentId = id;
        m_lastSistemaId = id;
        rebuildSidebar();
        rebuildPage();
        rebuildMargin();
    }
    if (!nodeId.isEmpty()) scrollToSection(nodeId);
}

void TerritorioWindow::showEmptyPage()
{
    flushAll();
    m_kind = Kind::None;
    m_currentId.clear();
    rebuildSidebar();
    rebuildPage();
    rebuildMargin();
}

void TerritorioWindow::flushAll()
{
    for (EncSection* s : std::as_const(m_sections)) s->flush();
}

void TerritorioWindow::scrollToSection(const QString& nodeId)
{
    QTimer::singleShot(0, this, [this, nodeId]() {
        for (EncSection* s : std::as_const(m_sections)) {
            if (s->nodeId() != nodeId) continue;
            QWidget* anchor = s->property("encHeading").value<QWidget*>();
            QWidget* target = anchor ? anchor : s;
            const QPoint p = target->mapTo(m_page, QPoint(0, 0));
            m_pageScroll->verticalScrollBar()->setValue(qMax(0, p.y() - 24));
            s->setFocus();
            return;
        }
    });
}

QString TerritorioWindow::pageSignature() const
{
    QStringList parts;
    parts << QString::number(int(m_kind)) << m_currentId;
    if (m_kind == Kind::Territorio && m_store) {
        if (const auto* t = m_store->territorio(m_currentId)) {
            std::function<void(const QList<TerritorioStore::Node>&)> walk = [&](const QList<TerritorioStore::Node>& ns) {
                for (const auto& n : ns) { parts << n.id << QString::number(int(n.type)); walk(n.children); }
            };
            walk(t->nodes);
        }
    } else if (m_kind == Kind::Sistema && m_construtorStore) {
        if (const auto* s = m_construtorStore->system(m_currentId)) {
            std::function<void(const QList<ConstrutorStore::Node>&)> walk = [&](const QList<ConstrutorStore::Node>& ns) {
                for (const auto& n : ns) { parts << n.id << QString::number(int(n.type)); walk(n.children); }
            };
            walk(s->nodes);
            parts << s->categoryId;
        }
    }
    return parts.join(QLatin1Char('|'));
}

// ── O verbete (meio) ─────────────────────────────────────────────────────────

void TerritorioWindow::rebuildPage()
{
    flushAll();
    m_formatBar->setTarget(nullptr);
    m_formatBar->setStatus(QString());
    m_focusedSection = nullptr;
    m_sections.clear();
    m_titleEdit = nullptr;

    const int scrollBefore = m_pageScroll->verticalScrollBar()->value();
    const bool sameItem = m_pageSig.section(QLatin1Char('|'), 0, 1) == pageSignature().section(QLatin1Char('|'), 0, 1);

    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("encPage"));
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(24, 30, 24, 60);
    row->addStretch(1);
    auto* colW = new QWidget(page);
    colW->setMaximumWidth(760);
    colW->setMinimumWidth(360);
    colW->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* col = new QVBoxLayout(colW);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);
    row->addWidget(colW, 12);
    row->addStretch(1);
    m_page = page;
    m_pageLay = col;

    const QColor muted = tc(Theme::textMuted());

    auto makeSection = [&](EncSection::Owner owner, const QString& ownerId, const QString& nodeId,
                           const QString& content, const QString& placeholder, qint64 updatedAt) {
        auto* s = new EncSection(owner, ownerId, nodeId, colW);
        s->setPlaceholderText(placeholder);
        s->setContentHtml(content);
        s->setProperty("encUpdatedAt", updatedAt);
        s->onSave = [this](EncSection* sec) { saveSection(sec); };
        s->onFocused = [this](EncSection* sec) {
            m_focusedSection = sec;
            m_formatBar->setTarget(sec);
            const qint64 at = sec->property("encUpdatedAt").toLongLong();
            m_formatBar->setStatus(at > 0 ? tr("Editado em %1").arg(QDateTime::fromMSecsSinceEpoch(at).toString(QStringLiteral("dd/MM/yyyy HH:mm"))) : QString());
            if (m_formatBar->focusMode()) {
                for (EncSection* o : std::as_const(m_sections)) o->applyTextColor(o == sec ? 1.0 : 0.38);
            }
        };
        s->onCursorMoved = [this](EncSection* sec) {
            const QRect r = sec->cursorRect();
            const QPoint p = sec->viewport()->mapTo(m_page, r.center());
            m_pageScroll->ensureVisible(p.x(), p.y(), 10, 60);
        };
        if (m_formatBar->focusMode()) s->applyTextColor(0.38);
        m_sections.append(s);
        return s;
    };

    auto headingRow = [&](const QString& num, const QString& name, int depth, const QString& nodeId,
                          const std::function<void(const QString&)>& rename) {
        auto* w = new QWidget(colW);
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, depth == 0 ? 26 : 16, 0, 2);
        h->setSpacing(6);
        if (!num.isEmpty()) {
            auto* n = textLabel(num, "encNum", QFont(QStringLiteral("IBM Plex Mono"), 10), w, false);
            QFont nf(QStringLiteral("IBM Plex Mono"));
            nf.setPixelSize(depth == 0 ? 13 : 12);
            n->setFont(nf);
            h->addWidget(n);
        }
        auto* e = new QLineEdit(name, w);
        e->setObjectName(QStringLiteral("encHeading"));
        e->setFont(serif(depth == 0 ? 20 : 16.5, QFont::DemiBold));
        e->setFrame(false);
        QObject::connect(e, &QLineEdit::editingFinished, this, [e, rename]() {
            const QString t = e->text().trimmed();
            if (!t.isEmpty()) rename(t);
        });
        h->addWidget(e, 1);
        auto* more = new QToolButton(w);
        more->setObjectName(QStringLiteral("encTool"));
        more->setText(QStringLiteral("⋯"));
        more->setCursor(Qt::PointingHandCursor);
        more->setToolTip(tr("Mais opções"));
        QObject::connect(more, &QToolButton::clicked, this, [this, more, nodeId]() {
            nodeMenu(nodeId, more->mapToGlobal(QPoint(0, more->height())));
        });
        h->addWidget(more);
        col->addWidget(w);
        return w;
    };

    if (m_kind == Kind::None) {
        col->addStretch(1);
        const bool lugares = m_mode == Mode::Lugares;
        auto* t = textLabel(lugares ? tr("Nenhum território ainda") : tr("Nenhum sistema ainda"), "encMarginBright", serif(24, QFont::DemiBold), colW);
        t->setAlignment(Qt::AlignHCenter);
        col->addWidget(t);
        col->addSpacing(8);
        auto* d = textLabel(lugares
            ? tr("Territórios são os lugares do seu mundo: reinos, cidades, casas. Cada um vira um verbete com documentos, vizinhos e quem é de lá.")
            : tr("Sistemas são as regras do seu mundo: magia, política, religião. Cada um vira um verbete com artigos, espectro e onde vale."),
            "encEmpty", sans(13.5), colW);
        d->setAlignment(Qt::AlignHCenter);
        col->addWidget(d);
        col->addSpacing(16);
        auto* b = new QPushButton(lugares ? tr("+ Novo território") : tr("+ Novo sistema"), colW);
        b->setObjectName(QStringLiteral("encAddBtn"));
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this]() { if (m_mode == Mode::Lugares) newTerritorio(); else newSistema(); });
        col->addWidget(b, 0, Qt::AlignHCenter);
        col->addStretch(2);
    } else if (m_kind == Kind::Territorio) {
        const TerritorioStore::Territorio* t = m_store->territorio(m_currentId);
        int neighbors = 0;
        for (const auto& l : m_store->links())
            if (l.fromTerritorioId == t->id || l.toTerritorioId == t->id) ++neighbors;
        col->addWidget(kick(neighbors == 1 ? tr("Território · 1 vizinho") : tr("Território · %1 vizinhos").arg(neighbors), colW));
        col->addSpacing(6);
        m_titleEdit = new QLineEdit(t->name, colW);
        m_titleEdit->setObjectName(QStringLiteral("encTitle"));
        m_titleEdit->setFont(serif(34, QFont::DemiBold));
        m_titleEdit->setFrame(false);
        connect(m_titleEdit, &QLineEdit::editingFinished, this, [this]() { saveTitle(m_titleEdit->text()); });
        col->addWidget(m_titleEdit);
        col->addSpacing(8);
        col->addWidget(makeSection(EncSection::Owner::Territorio, t->id, QString(), t->content,
            tr("Escreva a lore, o resumo ou a história deste território…"), t->updatedAt));

        const QString tid = t->id;
        std::function<void(const QList<TerritorioStore::Node>&, int)> walk = [&](const QList<TerritorioStore::Node>& nodes, int depth) {
            for (const auto& n : nodes) {
                const QString nid = n.id;
                const bool folder = n.type == TerritorioStore::NodeType::Folder;
                QWidget* head = headingRow(QString(), n.name, folder ? 0 : qMax(1, depth), nid, [this, tid, nid](const QString& name) {
                    const auto* tt = m_store->territorio(tid);
                    if (!tt) return;
                    std::function<const TerritorioStore::Node*(const QList<TerritorioStore::Node>&)> find =
                        [&](const QList<TerritorioStore::Node>& ns) -> const TerritorioStore::Node* {
                            for (const auto& x : ns) { if (x.id == nid) return &x; if (const auto* c = find(x.children)) return c; }
                            return nullptr;
                        };
                    const auto* node = find(tt->nodes);
                    if (node && node->name != name) m_store->updateNode(tid, nid, name, node->content);
                });
                auto* s = makeSection(EncSection::Owner::TerritorioNode, tid, nid, n.content, tr("Escreva aqui…"), n.updatedAt);
                s->setProperty("encHeading", QVariant::fromValue<QWidget*>(head));
                col->addWidget(s);
                walk(n.children, depth + 1);
            }
        };
        walk(t->nodes, 0);

        col->addSpacing(22);
        auto* adds = new QHBoxLayout;
        adds->setSpacing(8);
        auto* addFolder = new QPushButton(tr("+ Pasta"), colW);
        auto* addDoc = new QPushButton(tr("+ Documento"), colW);
        for (auto* b : { addFolder, addDoc }) { b->setObjectName(QStringLiteral("encAddBtn")); b->setCursor(Qt::PointingHandCursor); adds->addWidget(b); }
        adds->addStretch(1);
        connect(addFolder, &QPushButton::clicked, this, [this]() { addTerritorioNode(QString(), TerritorioStore::NodeType::Folder); });
        connect(addDoc, &QPushButton::clicked, this, [this]() { addTerritorioNode(QString(), TerritorioStore::NodeType::Doc); });
        col->addLayout(adds);
        col->addStretch(1);
    } else if (m_kind == Kind::Link) {
        const auto* l = m_store->link(m_currentId);
        const auto* a = m_store->territorio(l->fromTerritorioId);
        const auto* b = m_store->territorio(l->toTerritorioId);
        col->addWidget(kick(tr("Vínculo entre territórios"), colW));
        col->addSpacing(6);
        auto* title = textLabel(QStringLiteral("%1  ↔  %2").arg(a ? a->name : QStringLiteral("?"), b ? b->name : QStringLiteral("?")),
                                "encMarginBright", serif(30, QFont::DemiBold), colW);
        col->addWidget(title);
        col->addSpacing(10);
        col->addWidget(makeSection(EncSection::Owner::Link, l->id, QString(), l->docContent,
            tr("Escreva a história compartilhada entre os dois territórios — guerras, alianças, passado…"), l->updatedAt));
        col->addStretch(1);
    } else if (m_kind == Kind::Sistema) {
        const ConstrutorStore::System* s = m_construtorStore->system(m_currentId);
        const auto* cat = ConstrutorStore::categoryById(s->categoryId);
        const QColor cc = categoryColor(s->categoryId);
        QString k = cat ? cat->displayName : QString();
        if (cat && s->sliderIndex >= 0 && s->sliderIndex < cat->waypoints.size())
            k += QStringLiteral(" · ") + cat->waypoints.at(s->sliderIndex).label;
        col->addWidget(kick(k, colW, cc));
        col->addSpacing(6);
        m_titleEdit = new QLineEdit(s->name, colW);
        m_titleEdit->setObjectName(QStringLiteral("encTitle"));
        m_titleEdit->setFont(serif(34, QFont::DemiBold));
        m_titleEdit->setFrame(false);
        connect(m_titleEdit, &QLineEdit::editingFinished, this, [this]() { saveTitle(m_titleEdit->text()); });
        col->addWidget(m_titleEdit);
        col->addSpacing(8);
        col->addWidget(makeSection(EncSection::Owner::Sistema, s->id, QString(), s->content,
            tr("Escreva um resumo, parecer ou introdução deste sistema…"), s->updatedAt));

        QHash<QString, QString> nums;
        numberNodes(s->nodes, QString(), nums);
        const QString sid = s->id;
        std::function<void(const QList<ConstrutorStore::Node>&, int)> walk = [&](const QList<ConstrutorStore::Node>& nodes, int depth) {
            for (const auto& n : nodes) {
                const QString nid = n.id;
                const QString num = nums.value(nid);
                const QString label = num == QStringLiteral("§") ? num : (depth == 0 ? tr("Art. %1").arg(num) : num);
                QWidget* head = headingRow(label, n.name, depth, nid, [this, sid, nid](const QString& name) {
                    const auto* ss = m_construtorStore->system(sid);
                    if (!ss) return;
                    std::function<const ConstrutorStore::Node*(const QList<ConstrutorStore::Node>&)> find =
                        [&](const QList<ConstrutorStore::Node>& ns) -> const ConstrutorStore::Node* {
                            for (const auto& x : ns) { if (x.id == nid) return &x; if (const auto* c = find(x.children)) return c; }
                            return nullptr;
                        };
                    const auto* node = find(ss->nodes);
                    if (node && node->name != name) m_construtorStore->updateNode(sid, nid, name, node->content);
                });
                auto* sec = makeSection(EncSection::Owner::SistemaNode, sid, nid, n.content, tr("Escreva aqui…"), n.updatedAt);
                sec->setProperty("encHeading", QVariant::fromValue<QWidget*>(head));
                col->addWidget(sec);
                walk(n.children, depth + 1);
            }
        };
        walk(s->nodes, 0);

        col->addSpacing(22);
        auto* adds = new QHBoxLayout;
        adds->setSpacing(8);
        auto* addRule = new QPushButton(tr("+ Regra"), colW);
        auto* addSec = new QPushButton(tr("+ Seção"), colW);
        for (auto* b : { addRule, addSec }) { b->setObjectName(QStringLiteral("encAddBtn")); b->setCursor(Qt::PointingHandCursor); adds->addWidget(b); }
        adds->addStretch(1);
        connect(addRule, &QPushButton::clicked, this, [this]() { addSistemaNode(QString(), ConstrutorStore::NodeType::Rule); });
        connect(addSec, &QPushButton::clicked, this, [this]() { addSistemaNode(QString(), ConstrutorStore::NodeType::Section); });
        col->addLayout(adds);
        col->addStretch(1);
    }
    Q_UNUSED(muted);

    QWidget* old = m_pageScroll->takeWidget();
    m_pageScroll->setWidget(page);
    if (old) old->deleteLater();
    m_pageSig = pageSignature();
    if (sameItem) QTimer::singleShot(0, this, [this, scrollBefore]() { m_pageScroll->verticalScrollBar()->setValue(scrollBefore); });
    else m_pageScroll->verticalScrollBar()->setValue(0);
}

// ── Margem / ficha (direita) ─────────────────────────────────────────────────

void TerritorioWindow::rebuildMargin()
{
    if (!m_marginScroll) return;
    auto* m = new QWidget;
    m->setObjectName(QStringLiteral("encMargin"));
    auto* col = new QVBoxLayout(m);
    col->setContentsMargins(18, 22, 18, 22);
    col->setSpacing(0);

    auto mentionCard = [&](const QString& text, const QString& source, const std::function<void()>& open,
                           const std::function<void()>& remove) {
        auto* card = new ClickFrame("encMention", m);
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(10, 8, 8, 8);
        cl->setSpacing(4);
        auto* top = new QHBoxLayout;
        top->setSpacing(4);
        QString q = text.simplified();
        if (q.size() > 220) q = q.left(220) + QStringLiteral("…");
        auto* ql = textLabel(QStringLiteral("“%1”").arg(q), "encMarginText", serif(13, QFont::Normal, true), card);
        top->addWidget(ql, 1);
        auto* x = new QPushButton(QStringLiteral("×"), card);
        x->setObjectName(QStringLiteral("encLinkBtn"));
        x->setCursor(Qt::PointingHandCursor);
        x->setToolTip(tr("Remover menção"));
        connect(x, &QPushButton::clicked, this, [remove]() { QTimer::singleShot(0, remove); });
        top->addWidget(x, 0, Qt::AlignTop);
        cl->addLayout(top);
        if (!source.isEmpty()) cl->addWidget(textLabel(source, "encMarginMuted", sans(11), card));
        card->onClick = [open]() { QTimer::singleShot(0, open); };
        col->addWidget(card);
        col->addSpacing(8);
    };

    if (m_kind == Kind::Sistema && m_construtorStore) {
        const ConstrutorStore::System* s = m_construtorStore->system(m_currentId);
        const auto* cat = s ? ConstrutorStore::categoryById(s->categoryId) : nullptr;
        if (s && cat) {
            const QColor cc = categoryColor(s->categoryId);
            col->addWidget(kick(tr("Espectro"), m));
            col->addSpacing(8);
            QStringList labels;
            for (const auto& w : cat->waypoints) labels << w.label;
            auto* bar = new SpectrumBar(cat->waypoints.size(), s->sliderIndex, cc, labels, m);
            const QString sid = s->id;
            bar->onPick = [this, sid](int i) {
                QTimer::singleShot(0, this, [this, sid, i]() {
                    const auto* ss = m_construtorStore->system(sid);
                    if (ss && ss->sliderIndex != i) m_construtorStore->updateSystem(sid, ss->name, i);
                });
            };
            col->addWidget(bar);
            col->addSpacing(4);
            auto* ends = new QHBoxLayout;
            ends->addWidget(textLabel(labels.value(0), "encMarginMuted", sans(10.5), m, false));
            ends->addStretch(1);
            ends->addWidget(textLabel(labels.value(labels.size() - 1), "encMarginMuted", sans(10.5), m, false));
            col->addLayout(ends);
            col->addSpacing(8);
            const auto& wp = cat->waypoints.value(s->sliderIndex);
            col->addWidget(textLabel(wp.label, "encMarginBright", sans(14, QFont::DemiBold), m));
            col->addSpacing(3);
            col->addWidget(textLabel(wp.tooltip, "encMarginText", sans(12), m));
            col->addSpacing(18);

            const bool expanded = property("encTradeoffsOpen").toBool();
            auto list = [&](const QString& title, const QStringList& items, const QColor& color) {
                col->addWidget(kick(title, m, color));
                col->addSpacing(6);
                const int n = expanded ? items.size() : qMin(4, items.size());
                for (int i = 0; i < n; ++i) {
                    auto* l = textLabel(QStringLiteral("•  ") + items.at(i), "encMarginText", sans(12), m);
                    col->addWidget(l);
                    col->addSpacing(4);
                }
                col->addSpacing(10);
            };
            list(tr("Favorece"), wp.favors, tc(Theme::accentSuccess()));
            list(tr("Exige"), wp.demands, tc(Theme::accentWarning()));
            if (wp.favors.size() > 4 || wp.demands.size() > 4) {
                auto* more = new QPushButton(expanded ? tr("Ver menos") : tr("Ver todos"), m);
                more->setObjectName(QStringLiteral("encLinkBtn"));
                more->setCursor(Qt::PointingHandCursor);
                connect(more, &QPushButton::clicked, this, [this, expanded]() {
                    setProperty("encTradeoffsOpen", !expanded);
                    QTimer::singleShot(0, this, &TerritorioWindow::rebuildMargin);
                });
                col->addWidget(more, 0, Qt::AlignLeft);
                col->addSpacing(14);
            }

            // Onde vale
            col->addWidget(kick(tr("Vale em"), m));
            col->addSpacing(6);
            QStringList names;
            if (m_store)
                for (const auto& tid : s->territoryIds)
                    if (const auto* t = m_store->territorio(tid)) names << t->name;
            col->addWidget(textLabel(names.isEmpty() ? tr("Global: vale no mundo inteiro.") : names.join(QStringLiteral(" · ")),
                                     "encMarginText", sans(12.5), m));
            col->addSpacing(8);
            if (m_store && !m_store->territorios().isEmpty()) {
                auto* pick = new QPushButton(tr("Escolher territórios"), m);
                pick->setObjectName(QStringLiteral("encMarginBtn"));
                pick->setCursor(Qt::PointingHandCursor);
                auto* menu = new QMenu(pick);
                for (const auto& t : m_store->territorios()) {
                    QAction* a = menu->addAction(t.name);
                    a->setCheckable(true);
                    a->setChecked(s->territoryIds.contains(t.id));
                    const QString tid = t.id;
                    connect(a, &QAction::toggled, this, [this, sid, tid](bool on) {
                        const auto* ss = m_construtorStore->system(sid);
                        if (!ss) return;
                        QStringList ids = ss->territoryIds;
                        if (on && !ids.contains(tid)) ids << tid;
                        if (!on) ids.removeAll(tid);
                        QTimer::singleShot(0, this, [this, sid, ids]() { m_construtorStore->updateSystemTerritories(sid, ids); });
                    });
                }
                pick->setMenu(menu);
                col->addWidget(pick, 0, Qt::AlignLeft);
            }
            col->addSpacing(20);

            // No livro
            col->addWidget(kick(tr("No livro"), m));
            col->addSpacing(8);
            if (s->mentions.isEmpty()) {
                col->addWidget(textLabel(tr("Nenhuma menção ainda. Selecione um trecho no livro e use \"Salvar como menção ao sistema\"."),
                                         "encMarginMuted", sans(11.5), m));
            } else {
                QHash<QString, QString> nums;
                numberNodes(s->nodes, QString(), nums);
                for (const auto& men : s->mentions) {
                    QString src = men.sourceLabel;
                    if (!men.nodeId.isEmpty() && nums.contains(men.nodeId)) {
                        const QString num = nums.value(men.nodeId);
                        src += (src.isEmpty() ? QString() : QStringLiteral(" · ")) +
                               (num == QStringLiteral("§") ? tr("ao lado de uma seção") : tr("ao lado do Art. %1").arg(num));
                    }
                    const ConstrutorStore::Mention copy = men;
                    const QString mid = men.id;
                    mentionCard(men.text, src,
                                [this, copy]() { emit openMentionInEditorRequested(copy); },
                                [this, sid, mid]() { m_construtorStore->removeMention(sid, mid); });
                }
            }
            col->addSpacing(20);
            auto* del = new QPushButton(tr("Excluir sistema"), m);
            del->setObjectName(QStringLiteral("encDangerBtn"));
            del->setCursor(Qt::PointingHandCursor);
            connect(del, &QPushButton::clicked, this, [this, sid]() { QTimer::singleShot(0, this, [this, sid]() { deleteSistema(sid); }); });
            col->addWidget(del, 0, Qt::AlignLeft);
        }
    } else if (m_kind == Kind::Territorio && m_store) {
        const TerritorioStore::Territorio* t = m_store->territorio(m_currentId);
        if (t) {
            const QString tid = t->id;
            // Ficha
            auto* box = new QFrame(m);
            box->setObjectName(QStringLiteral("encInfobox"));
            auto* bl = new QVBoxLayout(box);
            bl->setContentsMargins(0, 0, 0, 0);
            bl->setSpacing(0);
            auto* banner = new ClickFrame("encBanner", box);
            banner->setFixedHeight(130);
            banner->setToolTip(tr("Trocar imagem"));
            {
                const int w = 262, h = 130;
                const qreal dpr = devicePixelRatioF();
                QPixmap pm(QSize(w, h) * dpr);
                pm.setDevicePixelRatio(dpr);
                pm.fill(Qt::transparent);
                QPainter p(&pm);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                QPainterPath clip;
                clip.addRoundedRect(QRectF(0, 0, w, h + 12), Theme::panelRadius(), Theme::panelRadius());
                p.setClipPath(clip);
                const QPixmap img = AvatarUtils::decodeDataUrl(t->avatarDataUrl);
                if (!img.isNull()) {
                    const QPixmap sc = img.scaled(QSize(w, h) * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                    p.drawPixmap(QRectF(0, 0, w, h), sc, QRectF((sc.width() - w * dpr) / 2, (sc.height() - h * dpr) / 2, w * dpr, h * dpr));
                    QLinearGradient g(0, h * 0.35, 0, h);
                    g.setColorAt(0, QColor(0, 0, 0, 0));
                    g.setColorAt(1, QColor(0, 0, 0, 170));
                    p.fillRect(QRectF(0, 0, w, h), g);
                } else {
                    p.fillRect(QRectF(0, 0, w, h), tc(Theme::hoverOverlay()));
                    p.drawPixmap(QPointF(w / 2.0 - 28, 18), AvatarUtils::circularAvatar(QString(), t->name, t->id, 56));
                }
                p.setFont(serif(17, QFont::DemiBold));
                p.setPen(img.isNull() ? tc(Theme::textBright()) : QColor(255, 255, 255));
                p.drawText(QRectF(12, h - 34, w - 24, 26), Qt::AlignLeft | Qt::AlignVCenter,
                           QFontMetrics(serif(17, QFont::DemiBold)).elidedText(t->name, Qt::ElideRight, w - 24));
                p.end();
                auto* bl2 = new QVBoxLayout(banner);
                bl2->setContentsMargins(0, 0, 0, 0);
                auto* lab = new QLabel(banner);
                lab->setPixmap(pm);
                lab->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                bl2->addWidget(lab);
            }
            banner->onClick = [this, tid]() { QTimer::singleShot(0, this, [this, tid]() { changeTerritorioImage(tid); }); };
            bl->addWidget(banner);

            auto infoRow = [&](const QString& label, QWidget* value) {
                auto* r = new QFrame(box);
                r->setObjectName(QStringLiteral("encInfoRow"));
                auto* rl = new QHBoxLayout(r);
                rl->setContentsMargins(12, 8, 12, 8);
                rl->setSpacing(8);
                auto* lab = textLabel(label, "encMarginMuted", sans(11.5), r, false);
                lab->setFixedWidth(74);
                lab->setAlignment(Qt::AlignTop | Qt::AlignLeft);
                rl->addWidget(lab, 0, Qt::AlignTop);
                value->setParent(r);
                rl->addWidget(value, 1);
                bl->addWidget(r);
            };
            auto linkList = [&](const QList<std::tuple<QString, QString, std::function<void()>>>& items, const QString& empty) {
                auto* w = new QWidget;
                auto* l = new QVBoxLayout(w);
                l->setContentsMargins(0, 0, 0, 0);
                l->setSpacing(2);
                if (items.isEmpty()) l->addWidget(textLabel(empty, "encMarginMuted", sans(11.5), w));
                for (const auto& [title, sub, fn] : items) {
                    auto* row = new ClickFrame("encRowLink", w);
                    auto* rl = new QVBoxLayout(row);
                    rl->setContentsMargins(4, 2, 4, 2);
                    rl->setSpacing(0);
                    rl->addWidget(textLabel(title, "encMarginBright", sans(12, QFont::DemiBold), row));
                    if (!sub.isEmpty()) rl->addWidget(textLabel(sub, "encMarginMuted", sans(10.5), row));
                    const auto f = fn;
                    row->onClick = [f]() { QTimer::singleShot(0, f); };
                    l->addWidget(row);
                }
                return w;
            };

            // Vizinhos (cada vínculo abre o documento dele; o nome abre o vizinho)
            {
                auto* w = new QWidget;
                auto* l = new QVBoxLayout(w);
                l->setContentsMargins(0, 0, 0, 0);
                l->setSpacing(4);
                int count = 0;
                for (const auto& link : m_store->links()) {
                    QString other;
                    if (link.fromTerritorioId == tid) other = link.toTerritorioId;
                    else if (link.toTerritorioId == tid) other = link.fromTerritorioId;
                    const auto* o = other.isEmpty() ? nullptr : m_store->territorio(other);
                    if (!o) continue;
                    ++count;
                    auto* row = new ClickFrame("encRowLink", w);
                    auto* rl = new QHBoxLayout(row);
                    rl->setContentsMargins(4, 2, 4, 2);
                    rl->setSpacing(6);
                    auto* av = new QLabel(row);
                    av->setPixmap(AvatarUtils::circularAvatar(o->avatarDataUrl, o->name, o->id, 18));
                    av->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                    rl->addWidget(av);
                    rl->addWidget(textLabel(o->name, "encMarginBright", sans(12, QFont::DemiBold), row), 1);
                    const QString oid = o->id;
                    row->onClick = [this, oid]() { QTimer::singleShot(0, this, [this, oid]() { openTerritorio(oid); }); };
                    l->addWidget(row);
                    const QString preview = plainPreview(link.docContent, 70);
                    auto* read = new QPushButton(preview.isEmpty() ? tr("Escrever o vínculo") : tr("Ler o vínculo"), w);
                    read->setObjectName(QStringLiteral("encLinkBtn"));
                    read->setCursor(Qt::PointingHandCursor);
                    if (!preview.isEmpty()) read->setToolTip(preview);
                    const QString lid = link.id;
                    connect(read, &QPushButton::clicked, this, [this, lid]() { QTimer::singleShot(0, this, [this, lid]() { openLink(lid); }); });
                    l->addWidget(read, 0, Qt::AlignLeft);
                }
                if (count == 0) l->addWidget(textLabel(tr("Nenhum vínculo ainda."), "encMarginMuted", sans(11.5), w));
                infoRow(tr("Vizinhos"), w);
            }

            // Gente daqui
            QList<std::tuple<QString, QString, std::function<void()>>> people;
            if (m_projectModel) {
                for (const auto& d : m_projectModel->drawers()) {
                    for (const auto& item : d.items) {
                        const bool born = item.origemTerritorioId == tid;
                        const bool lives = item.localAtualTerritorioId == tid;
                        if (!born && !lives) continue;
                        const QString rel = born && lives ? tr("nasceu e mora aqui") : born ? tr("nasceu aqui") : tr("mora aqui");
                        const QString key = d.key, iid = item.id;
                        people.append({ item.title, rel, [this, key, iid]() { emit openCharacterRequested(key, iid); } });
                    }
                }
            }
            infoRow(tr("Gente daqui"), linkList(people, tr("Ninguém com origem ou local aqui.")));

            // Sistemas que valem aqui
            QList<std::tuple<QString, QString, std::function<void()>>> systems;
            if (m_construtorStore) {
                for (const auto& s : m_construtorStore->systems()) {
                    if (!s.territoryIds.isEmpty() && !s.territoryIds.contains(tid)) continue;
                    const auto* cat = ConstrutorStore::categoryById(s.categoryId);
                    QString sub = cat ? cat->displayName : QString();
                    if (cat && s.sliderIndex < cat->waypoints.size()) sub += QStringLiteral(" · ") + cat->waypoints.at(s.sliderIndex).label;
                    if (s.territoryIds.isEmpty()) sub += tr(" · global");
                    const QString sid = s.id;
                    systems.append({ s.name, sub, [this, sid]() { openConstrutorNode(sid, QString()); } });
                }
            }
            infoRow(tr("Sistemas"), linkList(systems, tr("Nenhum sistema vale aqui.")));

            // Timeline
            const QStringList events = m_placeEventsProvider ? m_placeEventsProvider(tid) : QStringList();
            {
                auto* w = new QWidget;
                auto* l = new QVBoxLayout(w);
                l->setContentsMargins(0, 0, 0, 0);
                l->setSpacing(3);
                if (events.isEmpty()) l->addWidget(textLabel(tr("Nenhum evento marcado aqui ainda."), "encMarginMuted", sans(11.5), w));
                for (int i = 0; i < qMin(8, int(events.size())); ++i)
                    l->addWidget(textLabel(QStringLiteral("•  ") + events.at(i), "encMarginText", sans(12), w));
                if (events.size() > 8) l->addWidget(textLabel(tr("+ %1 eventos").arg(events.size() - 8), "encMarginMuted", sans(11), w));
                infoRow(tr("Na Timeline"), w);
            }
            col->addWidget(box);
            col->addSpacing(10);

            auto* actions = new QHBoxLayout;
            actions->setSpacing(6);
            auto* linkBtn = new QPushButton(tr("Vincular a…"), m);
            linkBtn->setObjectName(QStringLiteral("encMarginBtn"));
            linkBtn->setCursor(Qt::PointingHandCursor);
            auto* linkMenu = new QMenu(linkBtn);
            for (const auto& o : m_store->territorios()) {
                if (o.id == tid || m_store->linkBetween(tid, o.id)) continue;
                const QString oid = o.id;
                linkMenu->addAction(o.name, this, [this, tid, oid]() {
                    QTimer::singleShot(0, this, [this, tid, oid]() { m_store->addLink(tid, oid); });
                });
            }
            linkBtn->setEnabled(!linkMenu->isEmpty());
            linkBtn->setMenu(linkMenu);
            actions->addWidget(linkBtn);
            actions->addStretch(1);
            col->addLayout(actions);
            col->addSpacing(20);

            col->addWidget(kick(tr("No livro"), m));
            col->addSpacing(8);
            if (t->mentions.isEmpty()) {
                col->addWidget(textLabel(tr("Nenhuma menção ainda. Selecione um trecho no livro e use \"Salvar como menção ao Território\"."),
                                         "encMarginMuted", sans(11.5), m));
            } else {
                for (const auto& men : t->mentions) {
                    QString src = men.sourceLabel;
                    if (!men.category.isEmpty()) src += (src.isEmpty() ? QString() : QStringLiteral(" · ")) + men.category;
                    ConstrutorStore::Mention copy;
                    copy.id = men.id; copy.text = men.text; copy.nodeId = men.nodeId; copy.sourceType = men.sourceType;
                    copy.sourceLabel = men.sourceLabel; copy.chapterId = men.chapterId; copy.sceneIndex = men.sceneIndex;
                    copy.manuscriptId = men.manuscriptId; copy.itemId = men.itemId; copy.createdAt = men.createdAt;
                    const QString mid = men.id;
                    mentionCard(men.text, src,
                                [this, copy]() { emit openMentionInEditorRequested(copy); },
                                [this, tid, mid]() { m_store->removeMention(tid, mid); });
                }
            }
            col->addSpacing(20);
            auto* del = new QPushButton(tr("Excluir território"), m);
            del->setObjectName(QStringLiteral("encDangerBtn"));
            del->setCursor(Qt::PointingHandCursor);
            connect(del, &QPushButton::clicked, this, [this, tid]() { QTimer::singleShot(0, this, [this, tid]() { deleteTerritorio(tid); }); });
            col->addWidget(del, 0, Qt::AlignLeft);
        }
    } else if (m_kind == Kind::Link && m_store) {
        const auto* l = m_store->link(m_currentId);
        if (l) {
            col->addWidget(kick(tr("Entre"), m));
            col->addSpacing(8);
            for (const QString& id : { l->fromTerritorioId, l->toTerritorioId }) {
                const auto* t = m_store->territorio(id);
                if (!t) continue;
                auto* row = new ClickFrame("encRowLink", m);
                auto* rl = new QHBoxLayout(row);
                rl->setContentsMargins(4, 4, 4, 4);
                rl->setSpacing(8);
                auto* av = new QLabel(row);
                av->setPixmap(AvatarUtils::circularAvatar(t->avatarDataUrl, t->name, t->id, 28));
                av->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                rl->addWidget(av);
                rl->addWidget(textLabel(t->name, "encMarginBright", sans(13, QFont::DemiBold), row), 1);
                const QString tid = t->id;
                row->onClick = [this, tid]() { QTimer::singleShot(0, this, [this, tid]() { openTerritorio(tid); }); };
                col->addWidget(row);
                col->addSpacing(4);
            }
            col->addSpacing(20);
            const QString lid = l->id;
            const QString back = l->fromTerritorioId;
            auto* del = new QPushButton(tr("Excluir vínculo"), m);
            del->setObjectName(QStringLiteral("encDangerBtn"));
            del->setCursor(Qt::PointingHandCursor);
            connect(del, &QPushButton::clicked, this, [this, lid, back]() {
                QTimer::singleShot(0, this, [this, lid, back]() {
                    flushAll();
                    m_kind = Kind::None;
                    m_currentId.clear();
                    m_store->removeLink(lid);
                    openTerritorio(back);
                });
            });
            col->addWidget(del, 0, Qt::AlignLeft);
        }
    }
    col->addStretch(1);

    QWidget* old = m_marginScroll->takeWidget();
    m_marginScroll->setWidget(m);
    if (old) old->deleteLater();
    m_marginScroll->setVisible(m_kind != Kind::None);
}

// ── Salvar ───────────────────────────────────────────────────────────────────

void TerritorioWindow::saveSection(EncSection* sec)
{
    if (!sec) return;
    const QString html = sec->html();
    m_selfWriting = true;
    switch (sec->owner()) {
    case EncSection::Owner::Territorio:
        if (const auto* t = m_store ? m_store->territorio(sec->ownerId()) : nullptr)
            if (t->content != html) m_store->updateTerritorioContent(t->id, html);
        break;
    case EncSection::Owner::Link:
        if (const auto* l = m_store ? m_store->link(sec->ownerId()) : nullptr)
            if (l->docContent != html) m_store->updateLinkContent(l->id, html);
        break;
    case EncSection::Owner::TerritorioNode: {
        const auto* t = m_store ? m_store->territorio(sec->ownerId()) : nullptr;
        if (!t) break;
        std::function<const TerritorioStore::Node*(const QList<TerritorioStore::Node>&)> find =
            [&](const QList<TerritorioStore::Node>& ns) -> const TerritorioStore::Node* {
                for (const auto& x : ns) { if (x.id == sec->nodeId()) return &x; if (const auto* c = find(x.children)) return c; }
                return nullptr;
            };
        if (const auto* n = find(t->nodes))
            if (n->content != html) m_store->updateNode(t->id, n->id, n->name, html);
        break;
    }
    case EncSection::Owner::Sistema:
        if (const auto* s = m_construtorStore ? m_construtorStore->system(sec->ownerId()) : nullptr)
            if (s->content != html) m_construtorStore->updateSystemContent(s->id, html);
        break;
    case EncSection::Owner::SistemaNode: {
        const auto* s = m_construtorStore ? m_construtorStore->system(sec->ownerId()) : nullptr;
        if (!s) break;
        std::function<const ConstrutorStore::Node*(const QList<ConstrutorStore::Node>&)> find =
            [&](const QList<ConstrutorStore::Node>& ns) -> const ConstrutorStore::Node* {
                for (const auto& x : ns) { if (x.id == sec->nodeId()) return &x; if (const auto* c = find(x.children)) return c; }
                return nullptr;
            };
        if (const auto* n = find(s->nodes))
            if (n->content != html) m_construtorStore->updateNode(s->id, n->id, n->name, html);
        break;
    }
    }
    m_selfWriting = false;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    sec->setProperty("encUpdatedAt", now);
    if (sec == m_focusedSection)
        m_formatBar->setStatus(tr("Editado em %1").arg(QDateTime::fromMSecsSinceEpoch(now).toString(QStringLiteral("dd/MM/yyyy HH:mm"))));
}

void TerritorioWindow::saveTitle(const QString& raw)
{
    const QString name = raw.trimmed();
    if (name.isEmpty()) return;
    if (m_kind == Kind::Territorio && m_store) {
        const auto* t = m_store->territorio(m_currentId);
        if (t && t->name != name) m_store->updateTerritorio(t->id, name);
    } else if (m_kind == Kind::Sistema && m_construtorStore) {
        const auto* s = m_construtorStore->system(m_currentId);
        if (s && s->name != name) m_construtorStore->updateSystem(s->id, name, s->sliderIndex);
    }
}

// ── Ações ────────────────────────────────────────────────────────────────────

void TerritorioWindow::newTerritorio()
{
    if (!m_store) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Novo território"), tr("Nome:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    const QString id = m_store->addTerritorio(name.trimmed());
    if (m_mode != Mode::Lugares) setMode(Mode::Lugares);
    m_kind = Kind::None;
    openTerritorio(id);
}

void TerritorioWindow::newSistema()
{
    if (!m_construtorStore) return;
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Novo sistema"));
    auto* form = new QFormLayout;
    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(tr("Nome do sistema"));
    form->addRow(tr("Nome:"), nameEdit);
    auto* catCombo = new QComboBox(&dlg);
    for (const auto& c : ConstrutorStore::categories()) catCombo->addItem(c.displayName, c.id);
    form->addRow(tr("Categoria:"), catCombo);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QPushButton* okBtn = buttons->button(QDialogButtonBox::Ok);
    okBtn->setEnabled(false);
    connect(nameEdit, &QLineEdit::textChanged, &dlg, [okBtn](const QString& t) { okBtn->setEnabled(!t.trimmed().isEmpty()); });
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    auto* lay = new QVBoxLayout(&dlg);
    lay->addLayout(form);
    lay->addWidget(buttons);
    if (dlg.exec() != QDialog::Accepted) return;
    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) return;
    const QString id = m_construtorStore->addSystem(name, catCombo->currentData().toString(), 0);
    if (m_mode != Mode::Sistemas) setMode(Mode::Sistemas);
    m_kind = Kind::None;
    openSistema(id);
}

void TerritorioWindow::addTerritorioNode(const QString& parentId, TerritorioStore::NodeType type)
{
    if (!m_store || m_kind != Kind::Territorio) return;
    flushAll();
    const QString name = type == TerritorioStore::NodeType::Folder ? tr("Nova pasta") : tr("Novo documento");
    const QString id = m_store->addNode(m_currentId, parentId, type, name);
    QTimer::singleShot(30, this, [this, id]() {
        for (EncSection* s : std::as_const(m_sections)) {
            if (s->nodeId() != id) continue;
            if (auto* head = s->property("encHeading").value<QWidget*>()) {
                if (auto* e = head->findChild<QLineEdit*>()) { e->setFocus(); e->selectAll(); }
            }
            scrollToSection(id);
            break;
        }
    });
}

void TerritorioWindow::addSistemaNode(const QString& parentId, ConstrutorStore::NodeType type)
{
    if (!m_construtorStore || m_kind != Kind::Sistema) return;
    flushAll();
    const QString name = type == ConstrutorStore::NodeType::Rule ? tr("Nova regra") : tr("Nova seção");
    const QString id = m_construtorStore->addNode(m_currentId, parentId, type, name);
    QTimer::singleShot(30, this, [this, id]() {
        for (EncSection* s : std::as_const(m_sections)) {
            if (s->nodeId() != id) continue;
            const QPoint p = s->mapTo(m_page, QPoint(0, 0));
            m_pageScroll->verticalScrollBar()->setValue(qMax(0, p.y() - 120));
            if (auto* head = s->property("encHeading").value<QWidget*>()) {
                if (auto* e = head->findChild<QLineEdit*>()) { e->setFocus(); e->selectAll(); }
            }
            break;
        }
    });
}

void TerritorioWindow::nodeMenu(const QString& nodeId, const QPoint& globalPos)
{
    QMenu menu(this);
    if (m_kind == Kind::Territorio) {
        menu.addAction(tr("Adicionar Pasta filha"), this, [this, nodeId]() { addTerritorioNode(nodeId, TerritorioStore::NodeType::Folder); });
        menu.addAction(tr("Adicionar Documento filho"), this, [this, nodeId]() { addTerritorioNode(nodeId, TerritorioStore::NodeType::Doc); });
    } else if (m_kind == Kind::Sistema) {
        menu.addAction(tr("Adicionar Regra filha"), this, [this, nodeId]() { addSistemaNode(nodeId, ConstrutorStore::NodeType::Rule); });
        menu.addAction(tr("Adicionar Seção filha"), this, [this, nodeId]() { addSistemaNode(nodeId, ConstrutorStore::NodeType::Section); });
    } else {
        return;
    }
    menu.addSeparator();
    menu.addAction(tr("Excluir"), this, [this, nodeId]() { QTimer::singleShot(0, this, [this, nodeId]() { deleteNode(nodeId); }); });
    menu.exec(globalPos);
}

void TerritorioWindow::deleteNode(const QString& nodeId)
{
    if (m_kind == Kind::Territorio && m_store) {
        const auto r = QMessageBox::question(this, tr("Excluir item"),
            tr("Excluir este item e tudo o que ele contém?\n\nEssa ação não pode ser desfeita."),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (r != QMessageBox::Yes) return;
        flushAll();
        m_store->removeNode(m_currentId, nodeId);
    } else if (m_kind == Kind::Sistema && m_construtorStore) {
        const auto r = QMessageBox::question(this, tr("Excluir nó"),
            tr("Excluir este nó e todos os seus filhos?\n\nEssa ação não pode ser desfeita."),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (r != QMessageBox::Yes) return;
        flushAll();
        m_construtorStore->removeNode(m_currentId, nodeId);
    }
}

void TerritorioWindow::territorioMenu(const QString& id, const QPoint& globalPos)
{
    if (!m_store) return;
    QMenu menu(this);
    menu.addAction(tr("Trocar imagem…"), this, [this, id]() { QTimer::singleShot(0, this, [this, id]() { changeTerritorioImage(id); }); });
    QMenu* linkMenu = menu.addMenu(tr("Vincular a…"));
    for (const auto& o : m_store->territorios()) {
        if (o.id == id || m_store->linkBetween(id, o.id)) continue;
        const QString oid = o.id;
        linkMenu->addAction(o.name, this, [this, id, oid]() { QTimer::singleShot(0, this, [this, id, oid]() { m_store->addLink(id, oid); }); });
    }
    linkMenu->setEnabled(!linkMenu->isEmpty());
    menu.addSeparator();
    menu.addAction(tr("Excluir território"), this, [this, id]() { QTimer::singleShot(0, this, [this, id]() { deleteTerritorio(id); }); });
    menu.exec(globalPos);
}

void TerritorioWindow::sistemaMenu(const QString& id, const QPoint& globalPos)
{
    QMenu menu(this);
    menu.addAction(tr("Excluir sistema"), this, [this, id]() { QTimer::singleShot(0, this, [this, id]() { deleteSistema(id); }); });
    menu.exec(globalPos);
}

void TerritorioWindow::changeTerritorioImage(const QString& id)
{
    if (!m_store) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Escolher imagem"), QString(),
                                                      tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"));
    if (path.isEmpty()) return;
    QImage img(path);
    if (img.isNull()) return;
    m_store->updateTerritorioAvatar(id, AvatarUtils::encodeDataUrl(img, /*maxSide=*/480, /*jpegQuality=*/85));
}

void TerritorioWindow::deleteTerritorio(const QString& id)
{
    if (!m_store) return;
    const auto* t = m_store->territorio(id);
    if (!t) return;
    const auto r = QMessageBox::question(this, tr("Excluir território"),
        tr("Excluir o território \"%1\" e tudo o que ele contém?\n\nEssa ação não pode ser desfeita.").arg(t->name),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (r != QMessageBox::Yes) return;
    flushAll();
    if (m_currentId == id) { m_kind = Kind::None; m_currentId.clear(); }
    m_store->removeTerritorio(id);
    // Limpeza cross-store: fichas (origem/local), Timeline (placeId) e
    // Construtor (territoryIds) não podem ficar apontando pra ele.
    if (m_projectModel) m_projectModel->clearTerritorioReferences(id);
    if (m_construtorStore) m_construtorStore->removeTerritoryFromAllSystems(id);
    if (m_placeReferenceCleaner) m_placeReferenceCleaner(id);
}

void TerritorioWindow::deleteSistema(const QString& id)
{
    if (!m_construtorStore) return;
    const auto* s = m_construtorStore->system(id);
    if (!s) return;
    const auto r = QMessageBox::question(this, tr("Excluir sistema"),
        tr("Excluir o sistema \"%1\" e todos os seus nós?\n\nEssa ação não pode ser desfeita.").arg(s->name),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (r != QMessageBox::Yes) return;
    flushAll();
    if (m_currentId == id) { m_kind = Kind::None; m_currentId.clear(); }
    m_construtorStore->removeSystem(id);
}

// ── Busca ────────────────────────────────────────────────────────────────────

void TerritorioWindow::runSearch(const QString& text)
{
    const QString q = text.trimmed();
    m_searchResults->clear();
    if (q.size() < 2) { m_searchResults->hide(); return; }
    auto add = [&](const QString& label, const QString& kind, const QString& owner, const QString& node) {
        auto* it = new QListWidgetItem(label, m_searchResults);
        it->setData(Qt::UserRole, kind);
        it->setData(Qt::UserRole + 1, owner);
        it->setData(Qt::UserRole + 2, node);
    };
    if (m_store) {
        for (const auto& t : m_store->territorios()) {
            if (t.name.contains(q, Qt::CaseInsensitive)) add(t.name + QStringLiteral("  ·  ") + tr("Território"), QStringLiteral("t"), t.id, QString());
            std::function<void(const QList<TerritorioStore::Node>&)> walk = [&](const QList<TerritorioStore::Node>& ns) {
                for (const auto& n : ns) {
                    if (n.name.contains(q, Qt::CaseInsensitive)) add(n.name + QStringLiteral("  ·  ") + t.name, QStringLiteral("t"), t.id, n.id);
                    walk(n.children);
                }
            };
            walk(t.nodes);
        }
    }
    if (m_construtorStore) {
        for (const auto& s : m_construtorStore->systems()) {
            if (s.name.contains(q, Qt::CaseInsensitive)) add(s.name + QStringLiteral("  ·  ") + tr("Sistema"), QStringLiteral("s"), s.id, QString());
            std::function<void(const QList<ConstrutorStore::Node>&)> walk = [&](const QList<ConstrutorStore::Node>& ns) {
                for (const auto& n : ns) {
                    if (n.name.contains(q, Qt::CaseInsensitive)) add(n.name + QStringLiteral("  ·  ") + s.name, QStringLiteral("s"), s.id, n.id);
                    walk(n.children);
                }
            };
            walk(s.nodes);
        }
    }
    if (m_searchResults->count() == 0) {
        auto* it = new QListWidgetItem(tr("Nada encontrado."), m_searchResults);
        it->setFlags(Qt::NoItemFlags);
    }
    const QPoint p = m_searchEdit->mapTo(this, QPoint(0, m_searchEdit->height() + 4));
    const int rows = qMin(10, m_searchResults->count());
    m_searchResults->setGeometry(p.x() - 40, p.y(), m_searchEdit->width() + 40, rows * 30 + 12);
    m_searchResults->show();
    m_searchResults->raise();
}
